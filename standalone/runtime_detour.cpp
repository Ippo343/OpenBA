// ============================================================================
// standalone/runtime_detour.cpp — inline detours into Runtime-x64.dll
// ============================================================================
//
// Revived from the proxy-DLL era (`fcd3fb6^:patches/src/hooks.cpp`), where this
// installer carried every hooked function for months. The low-level installers
// below are that code, renamed; their "Bug Class N" comments are the scars from
// the crashes that shaped them and should be read before changing anything:
//
//   Bug Class 2 — PC-relative instructions in the displaced prologue (CALL/JMP
//     rel32, Jcc, RIP-relative ModR/M) point at the wrong place once copied to
//     a trampoline hundreds of MB away. HDE64 decodes and re-encodes them.
//   Bug Class 3 — MOV RAX,RSP / MOV R11,RSP capture RSP for absolute stack
//     addressing. This installer USED TO insert ADD reg,8 after such an
//     instruction. THAT CORRECTION WAS ITSELF THE BUG and was removed in
//     Wave 10 (lane FREEPRO) after a register-level control caught it
//     rotating three callee-saved registers by one slot; the frame argument
//     and the evidence are in the long comment on the copy loop below, and in
//     docs/runtime_port/WAVE10_LANE_FREEPRO.md. A CALLed trampoline sees the
//     same frame SHAPE retail's entry sees, so nothing needs shifting.
//   Bug Class 8 — SEH-cookie functions whose prologue cannot be displaced
//     without desyncing .pdata/.xdata unwind. Detour the vtable slot instead.
//
// See runtime_detour.h for why the runtime port needs detours at all
// (short version: registering a port redirects our callers; the DLL's own
// internal calls are direct rel32 CALLs that only a patch can catch).
// ============================================================================

#include "runtime_detour.h"
#include "resolve.h"
#include "hde/hde64.h"

#include <cstdio>
#include <cstring>

// Forward declarations for relocation-aware hooking

static bool InstallDetourRelocating(uintptr_t targetAddr, void* detourFunc, RtDetour* info, size_t minPrologueSize);
static bool HasRelativeInstructions(uintptr_t addr, size_t minSize);

bool InstallDetour(uintptr_t targetAddr, void* detourFunc, RtDetour* info, size_t prologueSize) {
    const size_t JUMP_SIZE = 12; // mov rax, imm64 (10) + jmp rax (2)

    // prologueSize must cover at least the jump and fit in our save buffer
    if (prologueSize < JUMP_SIZE) prologueSize = JUMP_SIZE;
    if (prologueSize > sizeof(info->original)) return false;

    // Auto-detect relative instructions and use relocating installer if needed
    if (HasRelativeInstructions(targetAddr, prologueSize)) {
#ifdef _DEBUG
        printf("[RtDetour] Detected relative instructions at 0x%llx, using relocating installer\n",
               (unsigned long long)targetAddr);
#endif
        return InstallDetourRelocating(targetAddr, detourFunc, info, prologueSize);
    }

    memcpy(info->original, (void*)targetAddr, prologueSize);
    info->target = (void*)targetAddr;
    info->patchSize = prologueSize;

    // Allocate executable trampoline: original bytes + jump back
    info->trampoline = VirtualAlloc(NULL, RT_TRAMPOLINE_BUFFER_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!info->trampoline) return false;

    // Copy prologue bytes to the trampoline VERBATIM.
    //
    // BUG CLASS 3 WAS ITSELF THE BUG — corrected Wave 10, lane FREEPRO, with a
    // register-level control (`standalone/rt_selftest.cpp` Test 2b).
    //
    // This loop used to insert `ADD RAX,8` after a copied `MOV RAX,RSP` (and
    // `ADD R11,8` after `MOV R11,RSP`), on the reasoning that "the CALL into the
    // trampoline shifts RSP by 8, so the captured value is 8 low". Follow the
    // frames and that reasoning does not survive:
    //
    //   retail:      caller RSP = C at the CALL; the CALL pushes the return
    //                address, so entry RSP = E = C-8 and `MOV RAX,RSP` gives
    //                RAX = E. The caller's 32-byte home space is at [E+8].
    //   trampoline:  our wrapper's RSP = C' at ITS call; entry RSP = R = C'-8
    //                and `MOV RAX,RSP` gives RAX = R. OUR home space is at
    //                [R+8].
    //
    // The two are the SAME SHAPE with E := R. Nothing is shifted, so nothing
    // needs correcting — and a JMP-reached trampoline (InstallDetourRSPSafe)
    // needs no correction either, because there RSP is caller-level already.
    // There is no configuration in which the +8 was right.
    //
    // What it actually did: these functions SAVE nonvolatiles through the
    // captured RAX and RESTORE them through an RSP-derived base. Moving only
    // RAX desynchronises the pair by one 8-byte slot, so the saved registers
    // come back rotated (RSI receives RBX's value, RDI receives RSI's, RBX
    // receives an unwritten home slot) and the last store lands just past the
    // 32-byte home space, in the wrapper's own frame.
    //
    // Three detour subjects in the tree have an RSP capture in the displaced
    // prologue: MemoryAllocator::Free (0x1800c6c00, trampoline never called —
    // rt_alloc_swap.cpp REPLACES the body), UI::UserInterface::Update
    // (0x1801213c0, routed to the byte-copy installer by lane UI1 for exactly
    // this reason), and Engine::Startup (0x1800db8a0), whose trampoline IS
    // called by rt_alloc_swap.cpp's shadow driver and which this corrects.
    memcpy(info->trampoline, info->original, prologueSize);
    uint8_t* dst = (uint8_t*)info->trampoline;
    size_t dstOff = prologueSize;

    // Append jump back to (target + prologueSize).
    // IMPORTANT: Use R10 (not RAX) for the jump. The copied prologue may set RAX
    // (e.g., MOV RAX,[RCX+0x28] to load a vtable pointer). Using MOV RAX for the
    // jump-back would clobber that value, causing crashes when the function body
    // tries to use RAX. R10 is volatile in the Win64 ABI and is never used in
    // MSVC function prologues, so it's safe to clobber here.
    uint8_t* jmpBack = dst + dstOff;
    jmpBack[0] = 0x49; jmpBack[1] = 0xBA; // mov r10, imm64
    *(uintptr_t*)(jmpBack + 2) = targetAddr + prologueSize;
    jmpBack[10] = 0x41; jmpBack[11] = 0xFF; jmpBack[12] = 0xE2; // jmp r10

    // Overwrite target with jump to detour (always 12 bytes)
    DWORD oldProtect;
    VirtualProtect((void*)targetAddr, prologueSize, PAGE_EXECUTE_READWRITE, &oldProtect);

    uint8_t* patch = (uint8_t*)targetAddr;
    patch[0] = 0x48; patch[1] = 0xB8; // mov rax, imm64
    *(uintptr_t*)(patch + 2) = (uintptr_t)detourFunc;
    patch[10] = 0xFF; patch[11] = 0xE0; // jmp rax

    // NOP any remaining bytes between the jump and the prologue boundary
    for (size_t i = JUMP_SIZE; i < prologueSize; i++)
        patch[i] = 0x90;

    VirtualProtect((void*)targetAddr, prologueSize, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), (void*)targetAddr, prologueSize);

    return true;
}

// ============================================================
// RSP-safe hook installer (for MOV RAX/R11,RSP prologues)
// ============================================================
//
// Builds a trampoline that is safe to reach by JMP (not CALL):
//   - No ADD/SUB RSP adjustment needed: the JMP-based detour ensures RSP is
//     exactly what the original caller had when the prologue runs.
//   - Trampoline = original_prologue_bytes + JMP back to (target + prologueSize)
//   - Same layout as regular InstallDetour trampoline, but detourFunc must JMP
//     (not CALL) into the trampoline.
//
// The hook patch at the target is identical (MOV RAX,detour; JMP RAX).
// The difference is in how detourFunc is implemented: it must be a naked
// function that JMPs to trampoline, never CALLs it.

bool InstallDetourRSPSafe(uintptr_t targetAddr, void* detourFunc, RtDetour* info, size_t prologueSize) {
    const size_t JUMP_SIZE = 12;

    if (prologueSize < JUMP_SIZE) prologueSize = JUMP_SIZE;
    if (prologueSize > sizeof(info->original)) return false;

    memcpy(info->original, (void*)targetAddr, prologueSize);
    info->target = (void*)targetAddr;
    info->patchSize = prologueSize;

    // Trampoline: original bytes + JMP back.
    // Identical layout to InstallDetour — the difference is that detourFunc
    // must JMP here rather than CALL here, so RSP is caller-level when
    // the MOV RAX/R11,RSP instruction executes.
    info->trampoline = VirtualAlloc(NULL, RT_TRAMPOLINE_BUFFER_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!info->trampoline) return false;

    uint8_t* t = (uint8_t*)info->trampoline;
    memcpy(t, info->original, prologueSize);

    // Append: MOV R10, imm64 + JMP R10  (back to original body)
    uint8_t* jmpBack = t + prologueSize;
    jmpBack[0] = 0x49; jmpBack[1] = 0xBA; // mov r10, imm64
    *(uintptr_t*)(jmpBack + 2) = targetAddr + prologueSize;
    jmpBack[10] = 0x41; jmpBack[11] = 0xFF; jmpBack[12] = 0xE2; // jmp r10

    // Patch target: MOV RAX, detour; JMP RAX
    DWORD oldProtect;
    VirtualProtect((void*)targetAddr, prologueSize, PAGE_EXECUTE_READWRITE, &oldProtect);

    uint8_t* patch = (uint8_t*)targetAddr;
    patch[0] = 0x48; patch[1] = 0xB8;
    *(uintptr_t*)(patch + 2) = (uintptr_t)detourFunc;
    patch[10] = 0xFF; patch[11] = 0xE0;
    for (size_t i = JUMP_SIZE; i < prologueSize; i++) patch[i] = 0x90;

    VirtualProtect((void*)targetAddr, prologueSize, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), (void*)targetAddr, prologueSize);
    return true;
}

// ============================================================
// Relocation-aware hook installer (fixes Bug Class 2)
// ============================================================
//
// Uses HDE64 to decode prologue instructions and relocates PC-relative
// instructions (CALL rel32, JMP rel32/rel8, Jcc rel8/rel32, RIP-relative
// addressing) so they work correctly from the trampoline's new address.
//
// Approach adapted from MinHook (TsudaKageyu/minhook, BSD-2-Clause).
// Key instruction types relocated:
//   - E8 CALL rel32 -> FF15 CALL [RIP+offset] with absolute target in data area
//   - E9 JMP rel32  -> FF25 JMP [RIP+offset] with absolute target in data area
//   - EB JMP rel8   -> same treatment
//   - 70-7F Jcc rel8, 0F 80-8F Jcc rel32 -> inverted Jcc + absolute JMP
//   - ModR/M [RIP+disp32] -> adjusted disp32 from new location

static bool InstallDetourRelocating(uintptr_t targetAddr, void* detourFunc, RtDetour* info, size_t minPrologueSize) {
    const size_t JUMP_SIZE = 12; // MOV RAX, imm64 + JMP RAX

    if (minPrologueSize < JUMP_SIZE) minPrologueSize = JUMP_SIZE;

    // Allocate trampoline buffer (256 bytes: plenty for expanded instructions + data area)
    info->trampoline = VirtualAlloc(NULL, RT_TRAMPOLINE_BUFFER_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!info->trampoline) return false;

    uint8_t* trampBuf = (uint8_t*)info->trampoline;
    // Data area grows backwards from end of buffer — used for absolute addresses
    // that RIP-relative instructions in the trampoline point to.
    uint8_t* dataEnd = trampBuf + RT_TRAMPOLINE_BUFFER_SIZE;
    uint8_t* dataPtr = dataEnd; // next data slot (grows downward)

    size_t oldPos = 0; // offset in original function
    size_t newPos = 0; // offset in trampoline

    // Decode and copy instructions until we've covered enough for the jump patch
    while (oldPos < minPrologueSize) {
        hde64s hs;
        unsigned int instrLen = hde64_disasm((const void*)(targetAddr + oldPos), &hs);

        if (hs.flags & F_ERROR) {
#ifdef _DEBUG
            printf("[RtDetour] HDE64 decode error at offset %zu\n", oldPos);
#endif
            VirtualFree(info->trampoline, 0, MEM_RELEASE);
            info->trampoline = nullptr;
            return false;
        }

        uintptr_t oldInst = targetAddr + oldPos;
        uintptr_t newInst = (uintptr_t)trampBuf + newPos;
        size_t copySize = instrLen;
        const uint8_t* copySrc = (const uint8_t*)oldInst;

        // Temporary buffer for modified instruction
        uint8_t instBuf[16];
        bool modified = false;

        if (hs.opcode == 0xE8) {
            // CALL rel32 -> CALL [RIP+offset] (FF 15) with absolute target in data area
            uintptr_t dest = oldInst + instrLen + (int32_t)hs.imm.imm32;

            // Store absolute target in data area
            dataPtr -= 8;
            *(uintptr_t*)dataPtr = dest;

            // FF 15 disp32: CALL [RIP+disp32]
            // Instruction is 6 bytes: FF 15 xx xx xx xx
            // RIP at execution = newInst + 6
            // We need: newInst + 6 + disp32 = dataPtr
            int64_t ripOffset = (int64_t)((uintptr_t)dataPtr - (newInst + 6));
            if (ripOffset < INT32_MIN || ripOffset > INT32_MAX) {
                // Data area too far — shouldn't happen within 256-byte buffer
#ifdef _DEBUG
                printf("[RtDetour] CALL data area too far\n");
#endif
                VirtualFree(info->trampoline, 0, MEM_RELEASE);
                info->trampoline = nullptr;
                return false;
            }

            // But CALL [RIP+disp32] pushes return address pointing into trampoline.
            // The callee will RET back to trampoline, which is fine — the next instruction
            // in the trampoline follows naturally.
            // However, MinHook uses a clever trick: FF15 00000002 / EB 08 / <address>
            // This does CALL [RIP+2] which points past the EB 08, and after return,
            // execution hits EB 08 which jumps over the 8-byte address.
            // Let's use the simpler approach: just use the data area at end of buffer.
            trampBuf[newPos + 0] = 0xFF;
            trampBuf[newPos + 1] = 0x15;
            *(int32_t*)(trampBuf + newPos + 2) = (int32_t)ripOffset;
            copySize = 6;
            modified = true;
        }
        else if ((hs.opcode & 0xFD) == 0xE9) {
            // JMP rel32 (E9) or JMP rel8 (EB)
            uintptr_t dest = oldInst + instrLen;
            if (hs.opcode == 0xEB)
                dest += (int8_t)hs.imm.imm8;
            else
                dest += (int32_t)hs.imm.imm32;

            // Check if jump target is within the copied prologue region
            if (dest >= targetAddr && dest < targetAddr + minPrologueSize) {
                // Internal jump — adjust relative offset for new position
                // Convert to JMP rel32 in the trampoline
                // Internal jump — but since instruction expansion may change offsets,
                // convert to absolute JMP for safety.
                goto emit_abs_jmp;
            }

emit_abs_jmp:
            // Emit absolute JMP via data area: FF 25 disp32
            dataPtr -= 8;
            *(uintptr_t*)dataPtr = dest;

            int64_t ripOffset2 = (int64_t)((uintptr_t)dataPtr - (newInst + 6));
            trampBuf[newPos + 0] = 0xFF;
            trampBuf[newPos + 1] = 0x25;
            *(int32_t*)(trampBuf + newPos + 2) = (int32_t)ripOffset2;
            copySize = 6;
            modified = true;
        }
        else if ((hs.opcode & 0xF0) == 0x70
              || (hs.opcode2 & 0xF0) == 0x80) {
            // Conditional jump: Jcc rel8 (70-7F) or Jcc rel32 (0F 80-8F)
            uintptr_t dest = oldInst + instrLen;
            if ((hs.opcode & 0xF0) == 0x70)
                dest += (int8_t)hs.imm.imm8;
            else
                dest += (int32_t)hs.imm.imm32;

            // Check if internal jump
            if (dest >= targetAddr && dest < targetAddr + minPrologueSize) {
                // Internal Jcc — convert to Jcc rel32 for safety
                // (rel8 might not reach after expansion)
                uint8_t cond = ((hs.opcode != 0x0F ? hs.opcode : hs.opcode2) & 0x0F);
                // Target offset in trampoline: for now, same byte offset (works if no
                // earlier instructions expanded, which is the common case for short prologues).
                // A more robust approach would track old->new offset mapping.
                size_t targetOffsetInOrig = dest - targetAddr;
                // Approximate: assume 1:1 mapping (valid for short prologues where
                // only one instruction is relative)
                int32_t rel = (int32_t)((int64_t)((uintptr_t)trampBuf + targetOffsetInOrig) - (int64_t)(newInst + 6));
                trampBuf[newPos + 0] = 0x0F;
                trampBuf[newPos + 1] = 0x80 | cond;
                *(int32_t*)(trampBuf + newPos + 2) = rel;
                copySize = 6;
                modified = true;
            } else {
                // External Jcc — invert condition, JMP to absolute target
                // Format: inverted_Jcc +14 (skip JMP block) / FF 25 00000000 / <8-byte address>
                uint8_t cond = ((hs.opcode != 0x0F ? hs.opcode : hs.opcode2) & 0x0F);

                // Inverted Jcc rel8: skip 14 bytes (the FF25 + 8-byte addr)
                // 7x 0E = Jcc +14
                trampBuf[newPos + 0] = 0x70 | (cond ^ 1);
                trampBuf[newPos + 1] = 0x0E; // skip 14 bytes

                // FF 25 00000000 = JMP [RIP+0] -> reads 8 bytes immediately after
                trampBuf[newPos + 2] = 0xFF;
                trampBuf[newPos + 3] = 0x25;
                *(int32_t*)(trampBuf + newPos + 4) = 0x00000000;

                // 8-byte absolute address
                *(uintptr_t*)(trampBuf + newPos + 8) = dest;

                copySize = 16; // 2 (Jcc) + 6 (FF25 + disp32) + 8 (addr)
                modified = true;
            }
        }
        else if ((hs.flags & F_MODRM) && (hs.modrm & 0xC7) == 0x05) {
            // RIP-relative addressing: ModR/M = 00???101
            // Need to adjust the disp32 for the new instruction location
            memcpy(instBuf, (const uint8_t*)oldInst, instrLen);

            // The displacement is at: len - imm_size - 4
            size_t immSize = 0;
            if (hs.flags & F_IMM8)  immSize = 1;
            if (hs.flags & F_IMM16) immSize = 2;
            if (hs.flags & F_IMM32) immSize = 4;
            if (hs.flags & F_IMM64) immSize = 8;

            uint32_t* pRelAddr = (uint32_t*)(instBuf + instrLen - immSize - 4);
            uintptr_t absTarget = oldInst + instrLen + (int32_t)hs.disp.disp32;

            int64_t newDisp = (int64_t)(absTarget - (newInst + instrLen));
            if (newDisp >= INT32_MIN && newDisp <= INT32_MAX) {
                // Can still use direct RIP-relative
                *pRelAddr = (uint32_t)(int32_t)newDisp;
                copySrc = instBuf;
                // copySize unchanged (same instruction length)
            } else {
                // Too far — use data area indirection
                // For CALL/JMP [RIP+disp32] (opcode FF /2 or /4): read the function
                // pointer from absTarget (original .rdata slot) and store it in the
                // trampoline data area, then fix the displacement to point there.
                // For other RIP-relative (MOV, LEA, CMP, etc.): copy the 8-byte value
                // from absTarget into the data area and redirect similarly.
                dataPtr -= 8;
                *(uintptr_t*)dataPtr = *(uintptr_t*)absTarget;

                int64_t dataDisp = (int64_t)((uintptr_t)dataPtr - (newInst + instrLen));
                if (dataDisp >= INT32_MIN && dataDisp <= INT32_MAX) {
                    *pRelAddr = (int32_t)dataDisp;
                    copySrc = instBuf;
#ifdef _DEBUG
                    printf("[RtDetour] RIP-relative indirection via data area for 0x%llx -> data@%p\n",
                           (unsigned long long)oldInst, (void*)dataPtr);
#endif
                } else {
                    // Data area itself unreachable (shouldn't happen within 256-byte buffer)
                    dataPtr += 8; // restore
#ifdef _DEBUG
                    printf("[RtDetour] RIP-relative data area unreachable for 0x%llx\n",
                           (unsigned long long)oldInst);
#endif
                    VirtualFree(info->trampoline, 0, MEM_RELEASE);
                    info->trampoline = nullptr;
                    return false;
                }
            }
        }

        // Sanity check: don't overflow the trampoline code area
        // (leave room for the jump-back sequence and data area)
        if (newPos + copySize + 13 > (size_t)(dataPtr - trampBuf)) {
#ifdef _DEBUG
            printf("[RtDetour] Trampoline buffer overflow\n");
#endif
            VirtualFree(info->trampoline, 0, MEM_RELEASE);
            info->trampoline = nullptr;
            return false;
        }

        if (!modified) {
            memcpy(trampBuf + newPos, copySrc, copySize);
        }

        newPos += copySize;

        // NO RSP-capture "correction" here either. The +8 that used to be
        // inserted after a copied MOV RAX,RSP / MOV R11,RSP was wrong in every
        // configuration — see the long comment in InstallDetour above for the
        // frame argument and for the one-slot register rotation it produced.
        // MemoryAllocator::Free (0x1800c6c00) reaches THIS path, because the
        // Jcc rel32 at +3 makes HasRelativeInstructions pick the relocator.

        oldPos += instrLen;
    }

    // Save original bytes
    if (oldPos > sizeof(info->original)) {
        // Prologue too large for our save buffer
#ifdef _DEBUG
        printf("[RtDetour] Prologue too large: %zu bytes\n", oldPos);
#endif
        VirtualFree(info->trampoline, 0, MEM_RELEASE);
        info->trampoline = nullptr;
        return false;
    }

    memcpy(info->original, (void*)targetAddr, oldPos);
    info->target = (void*)targetAddr;
    info->patchSize = oldPos;

    // Append jump-back: MOV R10, imm64 + JMP R10 (13 bytes)
    uint8_t* jmpBack = trampBuf + newPos;
    jmpBack[0] = 0x49; jmpBack[1] = 0xBA; // MOV R10, imm64
    *(uintptr_t*)(jmpBack + 2) = targetAddr + oldPos;
    jmpBack[10] = 0x41; jmpBack[11] = 0xFF; jmpBack[12] = 0xE2; // JMP R10

    // Overwrite target with jump to detour (always 12 bytes)
    DWORD oldProtect;
    VirtualProtect((void*)targetAddr, oldPos, PAGE_EXECUTE_READWRITE, &oldProtect);

    uint8_t* patch = (uint8_t*)targetAddr;
    patch[0] = 0x48; patch[1] = 0xB8; // MOV RAX, imm64
    *(uintptr_t*)(patch + 2) = (uintptr_t)detourFunc;
    patch[10] = 0xFF; patch[11] = 0xE0; // JMP RAX

    // NOP remaining bytes
    for (size_t i = JUMP_SIZE; i < oldPos; i++)
        patch[i] = 0x90;

    VirtualProtect((void*)targetAddr, oldPos, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), (void*)targetAddr, oldPos);
    FlushInstructionCache(GetCurrentProcess(), info->trampoline, RT_TRAMPOLINE_BUFFER_SIZE);

#ifdef _DEBUG
    printf("[RtDetour] Hook installed: %zu prologue bytes -> %zu trampoline bytes\n",
           oldPos, newPos + 13);
#endif

    return true;
}

// ============================================================
// Auto-detecting hook installer
// ============================================================
//
// Scans the prologue for PC-relative instructions. If any are found,
// uses InstallDetourRelocating() for proper relocation. Otherwise, falls
// back to the fast blind-copy path (original InstallDetour behavior).

static bool HasRelativeInstructions(uintptr_t addr, size_t minSize) {
    size_t pos = 0;
    while (pos < minSize) {
        hde64s hs;
        unsigned int len = hde64_disasm((const void*)(addr + pos), &hs);
        if (hs.flags & F_ERROR) return true; // assume yes on error, let relocator handle it

        // Check for relative operands
        if (hs.flags & F_RELATIVE) return true; // CALL/JMP/Jcc with relative operand

        // Check for RIP-relative addressing (ModR/M = 00???101)
        if ((hs.flags & F_MODRM) && (hs.modrm & 0xC7) == 0x05) return true;

        pos += len;
    }
    return false;
}

void RemoveDetour(RtDetour* info) {
    if (!info->target) return;

    DWORD oldProtect;
    VirtualProtect(info->target, info->patchSize, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(info->target, info->original, info->patchSize);
    VirtualProtect(info->target, info->patchSize, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), info->target, info->patchSize);

    if (info->trampoline) {
        VirtualFree(info->trampoline, 0, MEM_RELEASE);
        info->trampoline = nullptr;
    }
    info->target = nullptr;
}

// ============================================================
// VTable hook: swap a function pointer in a vtable
// ============================================================
// No code modification — just replaces the pointer in the vtable.
// Solves Bug Class 8 (SEH cookie functions) because the original
// function's prologue and .pdata/.xdata unwind info stay intact.

bool InstallDetourVTableAt(uintptr_t vtableAddr, int slotIndex, void* detourFunc, RtDetour* info) {
    memset(info, 0, sizeof(*info));

    // Already-resolved address: the caller decides which module it came from
    // (and, for Runtime, resolves it Raw).
    if (!vtableAddr) return false;

    void** slot = &((void**)vtableAddr)[slotIndex];

    // Make vtable slot writable
    DWORD oldProtect;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &oldProtect)) return false;

    // Save original function pointer as the "trampoline" (caller can call it directly)
    info->trampoline = *slot;
    // Store the slot address in target so RemoveDetourVTable can restore it
    info->target = (void*)slot;
    // Store original pointer in original[] for diagnostics
    memcpy(info->original, &info->trampoline, sizeof(void*));
    info->patchSize = 0; // sentinel: 0 means vtable hook (no code bytes to restore)

    // Swap in our detour
    *slot = detourFunc;

    // Restore original protection
    VirtualProtect(slot, sizeof(void*), oldProtect, &oldProtect);
    return true;
}

void RemoveDetourVTable(RtDetour* info) {
    if (!info->target) return;

    void** slot = (void**)info->target;
    DWORD oldProtect;
    VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &oldProtect);
    *slot = info->trampoline;
    VirtualProtect(slot, sizeof(void*), oldProtect, &oldProtect);

    info->target = nullptr;
    info->trampoline = nullptr;
}

// ============================================================================
// Runtime-DLL front doors
// ============================================================================
//
// Everything above is module-agnostic. These are the entry points the runtime
// port actually uses: they resolve the ORIGINAL DLL code (never through the
// port registry — see the header), install, and record the detour so the crash
// handler can name it.

namespace {

struct DetourRegistryEntry {
    const RtDetour* info;
};

constexpr size_t kMaxDetours = 512;
DetourRegistryEntry g_detours[kMaxDetours];
size_t              g_detourCount = 0;

void RecordDetour(const RtDetour* info) {
    if (g_detourCount < kMaxDetours) g_detours[g_detourCount++].info = info;
}

bool InstallResolved(const char* label, uintptr_t target, void* ourFn,
                     RtDetour* info, size_t prologueSize,
                     const uint8_t* expected, size_t expectedCount) {
    if (!target) {
        fprintf(stderr, "[RtDetour] %s: target did not resolve — not installed\n",
                label ? label : "(unnamed)");
        fflush(stderr);
        return false;
    }
    if (!ourFn || !info) return false;

    // Version drift guard. The allocator lock does this by hand for its two
    // targets; any caller that knows the expected bytes should pass them, so a
    // different Runtime-x64.dll build refuses the patch instead of corrupting
    // a function whose prologue moved.
    if (expected && expectedCount) {
        if (memcmp(reinterpret_cast<const void*>(target), expected, expectedCount) != 0) {
            fprintf(stderr, "[RtDetour] %s: prologue at %p does not match expected "
                            "%zu bytes — refusing to patch\n",
                    label ? label : "(unnamed)", (void*)target, expectedCount);
            fflush(stderr);
            return false;
        }
    }

    memset(info, 0, sizeof(*info));
    if (!InstallDetour(target, ourFn, info, prologueSize)) {
        fprintf(stderr, "[RtDetour] %s: install FAILED at %p\n",
                label ? label : "(unnamed)", (void*)target);
        fflush(stderr);
        return false;
    }
    info->label = label;
    RecordDetour(info);
    fprintf(stderr, "[RtDetour] %s: %p -> %p (trampoline %p, %zu bytes displaced)\n",
            label ? label : "(unnamed)", (void*)target, ourFn,
            info->trampoline, info->patchSize);
    fflush(stderr);
    return true;
}

} // namespace

bool InstallRuntimeDetour(const char* label, uintptr_t ghidraAddr, void* ourFn,
                          RtDetour* info, size_t prologueSize) {
    return InstallResolved(label, ResolveRuntimeRaw(ghidraAddr), ourFn, info,
                           prologueSize, nullptr, 0);
}

bool InstallRuntimeDetourExport(const char* label, const char* mangledName, void* ourFn,
                                RtDetour* info, size_t prologueSize) {
    return InstallResolved(label, reinterpret_cast<uintptr_t>(ResolveExportRaw(mangledName)),
                           ourFn, info, prologueSize, nullptr, 0);
}

bool InstallRuntimeDetourChecked(const char* label, uintptr_t ghidraAddr, void* ourFn,
                                 RtDetour* info, size_t prologueSize,
                                 const uint8_t* expected, size_t count) {
    return InstallResolved(label, ResolveRuntimeRaw(ghidraAddr), ourFn, info,
                           prologueSize, expected, count);
}

const char* FindRuntimeDetourByAddress(uintptr_t addr) {
    for (size_t i = 0; i < g_detourCount; i++) {
        const RtDetour* d = g_detours[i].info;
        if (!d || !d->target) continue;
        uintptr_t t = reinterpret_cast<uintptr_t>(d->target);
        if (d->patchSize && addr >= t && addr < t + d->patchSize) return d->label;
        uintptr_t tr = reinterpret_cast<uintptr_t>(d->trampoline);
        if (tr && addr >= tr && addr < tr + RT_TRAMPOLINE_BUFFER_SIZE) return d->label;
    }
    return nullptr;
}

size_t RuntimeDetourCount() { return g_detourCount; }

void DumpRuntimeDetours() {
    fprintf(stderr, "[RtDetour] ==== %zu installed ====\n", g_detourCount);
    for (size_t i = 0; i < g_detourCount; i++) {
        const RtDetour* d = g_detours[i].info;
        if (!d) continue;
        fprintf(stderr, "[RtDetour]   %-40s target=%p tramp=%p size=%zu\n",
                d->label ? d->label : "(unnamed)", d->target, d->trampoline, d->patchSize);
    }
    fflush(stderr);
}
