// Hook management and reimplementation registry.
// Low-level hooking + build number patch + reimplementation table.

#include "hooks.h"
#include "resolve.h"
#include <cstdio>
#include <cstring>

// ============================================================
// Crash diagnostics breadcrumb
// ============================================================

volatile const char* g_lastHook = "none";

// ============================================================
// Hook registry for crash diagnostics
// ============================================================

struct HookRegistryEntry {
    const char* name;
    HookInfo* info;
};

static HookRegistryEntry g_hookRegistry[1024];
static int g_hookRegistryCount = 0;

void RegisterHookInfo(const char* name, HookInfo* info) {
    if (g_hookRegistryCount < 1024) {
        g_hookRegistry[g_hookRegistryCount].name = name;
        g_hookRegistry[g_hookRegistryCount].info = info;
        g_hookRegistryCount++;
    }
}

const char* FindHookByAddress(uintptr_t addr) {
    for (int i = 0; i < g_hookRegistryCount; i++) {
        HookInfo* hi = g_hookRegistry[i].info;
        if (!hi) continue;

        // Check if addr matches the hooked target address
        if (hi->target && (uintptr_t)hi->target == addr)
            return g_hookRegistry[i].name;

        // Check if addr is within the trampoline buffer
        if (hi->trampoline) {
            uintptr_t tBase = (uintptr_t)hi->trampoline;
            if (addr >= tBase && addr < tBase + TRAMPOLINE_BUFFER_SIZE)
                return g_hookRegistry[i].name;
        }
    }
    return nullptr;
}

// ============================================================
// Low-level hook infrastructure
// ============================================================

// Forward declarations for relocation-aware hooking
#include "hde/hde64.h"
static bool InstallHookRelocating(uintptr_t targetAddr, void* detourFunc, HookInfo* info, size_t minPrologueSize);
static bool HasRelativeInstructions(uintptr_t addr, size_t minSize);

bool InstallHook(uintptr_t targetAddr, void* detourFunc, HookInfo* info, size_t prologueSize) {
    const size_t JUMP_SIZE = 12; // mov rax, imm64 (10) + jmp rax (2)

    // prologueSize must cover at least the jump and fit in our save buffer
    if (prologueSize < JUMP_SIZE) prologueSize = JUMP_SIZE;
    if (prologueSize > sizeof(info->original)) return false;

    // Auto-detect relative instructions and use relocating installer if needed
    if (HasRelativeInstructions(targetAddr, prologueSize)) {
#ifdef _DEBUG
        printf("[InstallHook] Detected relative instructions at 0x%llx, using relocating installer\n",
               (unsigned long long)targetAddr);
#endif
        return InstallHookRelocating(targetAddr, detourFunc, info, prologueSize);
    }

    memcpy(info->original, (void*)targetAddr, prologueSize);
    info->target = (void*)targetAddr;
    info->patchSize = prologueSize;

    // Allocate executable trampoline: original bytes + jump back
    info->trampoline = VirtualAlloc(NULL, TRAMPOLINE_BUFFER_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!info->trampoline) return false;

    // Copy prologue bytes to trampoline WITH RSP capture correction (Bug Class 3).
    //
    // The CALL into the trampoline shifts RSP by 8. For most instructions this
    // is harmless (Bug Class 4 stores are internally consistent), but
    // MOV RAX,RSP (48 8B C4) and MOV R11,RSP (4C 8B DC) capture RSP into a
    // register used for ABSOLUTE stack addressing. We fix this by inserting
    // ADD RAX,8 or ADD R11,8 immediately after each such instruction.
    uint8_t* src = info->original;
    uint8_t* dst = (uint8_t*)info->trampoline;
    size_t srcOff = 0;
    size_t dstOff = 0;

    while (srcOff < prologueSize) {
        // Check for MOV RAX,RSP: 48 8B C4
        if (srcOff + 3 <= prologueSize &&
            src[srcOff] == 0x48 && src[srcOff+1] == 0x8B && src[srcOff+2] == 0xC4) {
            dst[dstOff++] = 0x48; dst[dstOff++] = 0x8B; dst[dstOff++] = 0xC4;
            srcOff += 3;
            // Insert ADD RAX,8 (48 83 C0 08) to compensate for CALL's RSP push
            dst[dstOff++] = 0x48; dst[dstOff++] = 0x83; dst[dstOff++] = 0xC0; dst[dstOff++] = 0x08;
#ifdef _DEBUG
            printf("[InstallHook] RSP correction: inserted ADD RAX,8 at trampoline offset %zu\n", dstOff - 4);
#endif
        }
        // Check for MOV R11,RSP: 4C 8B DC
        else if (srcOff + 3 <= prologueSize &&
                 src[srcOff] == 0x4C && src[srcOff+1] == 0x8B && src[srcOff+2] == 0xDC) {
            dst[dstOff++] = 0x4C; dst[dstOff++] = 0x8B; dst[dstOff++] = 0xDC;
            srcOff += 3;
            // Insert ADD R11,8 (49 83 C3 08) to compensate for CALL's RSP push
            dst[dstOff++] = 0x49; dst[dstOff++] = 0x83; dst[dstOff++] = 0xC3; dst[dstOff++] = 0x08;
#ifdef _DEBUG
            printf("[InstallHook] RSP correction: inserted ADD R11,8 at trampoline offset %zu\n", dstOff - 4);
#endif
        }
        else {
            dst[dstOff++] = src[srcOff++];
        }
    }

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
//   - Same layout as regular InstallHook trampoline, but detourFunc must JMP
//     (not CALL) into the trampoline.
//
// The hook patch at the target is identical (MOV RAX,detour; JMP RAX).
// The difference is in how detourFunc is implemented: it must be a naked
// function that JMPs to trampoline, never CALLs it.

bool InstallHookRSPSafe(uintptr_t targetAddr, void* detourFunc, HookInfo* info, size_t prologueSize) {
    const size_t JUMP_SIZE = 12;

    if (prologueSize < JUMP_SIZE) prologueSize = JUMP_SIZE;
    if (prologueSize > sizeof(info->original)) return false;

    memcpy(info->original, (void*)targetAddr, prologueSize);
    info->target = (void*)targetAddr;
    info->patchSize = prologueSize;

    // Trampoline: original bytes + JMP back.
    // Identical layout to InstallHook — the difference is that detourFunc
    // must JMP here rather than CALL here, so RSP is caller-level when
    // the MOV RAX/R11,RSP instruction executes.
    info->trampoline = VirtualAlloc(NULL, TRAMPOLINE_BUFFER_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
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

static bool InstallHookRelocating(uintptr_t targetAddr, void* detourFunc, HookInfo* info, size_t minPrologueSize) {
    const size_t JUMP_SIZE = 12; // MOV RAX, imm64 + JMP RAX

    if (minPrologueSize < JUMP_SIZE) minPrologueSize = JUMP_SIZE;

    // Allocate trampoline buffer (256 bytes: plenty for expanded instructions + data area)
    info->trampoline = VirtualAlloc(NULL, TRAMPOLINE_BUFFER_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!info->trampoline) return false;

    uint8_t* trampBuf = (uint8_t*)info->trampoline;
    // Data area grows backwards from end of buffer — used for absolute addresses
    // that RIP-relative instructions in the trampoline point to.
    uint8_t* dataEnd = trampBuf + TRAMPOLINE_BUFFER_SIZE;
    uint8_t* dataPtr = dataEnd; // next data slot (grows downward)

    size_t oldPos = 0; // offset in original function
    size_t newPos = 0; // offset in trampoline

    // Decode and copy instructions until we've covered enough for the jump patch
    while (oldPos < minPrologueSize) {
        hde64s hs;
        unsigned int instrLen = hde64_disasm((const void*)(targetAddr + oldPos), &hs);

        if (hs.flags & F_ERROR) {
#ifdef _DEBUG
            printf("[HookReloc] HDE64 decode error at offset %zu\n", oldPos);
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
                printf("[HookReloc] CALL data area too far\n");
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
                    printf("[HookReloc] RIP-relative indirection via data area for 0x%llx -> data@%p\n",
                           (unsigned long long)oldInst, (void*)dataPtr);
#endif
                } else {
                    // Data area itself unreachable (shouldn't happen within 256-byte buffer)
                    dataPtr += 8; // restore
#ifdef _DEBUG
                    printf("[HookReloc] RIP-relative data area unreachable for 0x%llx\n",
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
            printf("[HookReloc] Trampoline buffer overflow\n");
#endif
            VirtualFree(info->trampoline, 0, MEM_RELEASE);
            info->trampoline = nullptr;
            return false;
        }

        if (!modified) {
            memcpy(trampBuf + newPos, copySrc, copySize);
        }

        newPos += copySize;

        // Bug Class 3 fix: if the instruction we just copied is MOV RAX,RSP or
        // MOV R11,RSP, insert a correction (ADD RAX,8 or ADD R11,8) to compensate
        // for the CALL-induced RSP shift when entering the trampoline.
        if (instrLen == 3) {
            const uint8_t* origBytes = (const uint8_t*)oldInst;
            if (origBytes[0] == 0x48 && origBytes[1] == 0x8B && origBytes[2] == 0xC4) {
                // MOV RAX,RSP -> insert ADD RAX,8
                trampBuf[newPos++] = 0x48; trampBuf[newPos++] = 0x83;
                trampBuf[newPos++] = 0xC0; trampBuf[newPos++] = 0x08;
#ifdef _DEBUG
                printf("[HookReloc] RSP correction: inserted ADD RAX,8\n");
#endif
            }
            else if (origBytes[0] == 0x4C && origBytes[1] == 0x8B && origBytes[2] == 0xDC) {
                // MOV R11,RSP -> insert ADD R11,8
                trampBuf[newPos++] = 0x49; trampBuf[newPos++] = 0x83;
                trampBuf[newPos++] = 0xC3; trampBuf[newPos++] = 0x08;
#ifdef _DEBUG
                printf("[HookReloc] RSP correction: inserted ADD R11,8\n");
#endif
            }
        }

        oldPos += instrLen;
    }

    // Save original bytes
    if (oldPos > sizeof(info->original)) {
        // Prologue too large for our save buffer
#ifdef _DEBUG
        printf("[HookReloc] Prologue too large: %zu bytes\n", oldPos);
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
    FlushInstructionCache(GetCurrentProcess(), info->trampoline, TRAMPOLINE_BUFFER_SIZE);

#ifdef _DEBUG
    printf("[HookReloc] Hook installed: %zu prologue bytes -> %zu trampoline bytes\n",
           oldPos, newPos + 13);
#endif

    return true;
}

// ============================================================
// Auto-detecting hook installer
// ============================================================
//
// Scans the prologue for PC-relative instructions. If any are found,
// uses InstallHookRelocating() for proper relocation. Otherwise, falls
// back to the fast blind-copy path (original InstallHook behavior).

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

void RemoveHook(HookInfo* info) {
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

bool InstallHookVTable(uintptr_t vtableGhidraAddr, int slotIndex, void* detourFunc, HookInfo* info) {
    memset(info, 0, sizeof(*info));

    uintptr_t vtableRuntime = ResolveAddr(Module::Exe, vtableGhidraAddr);
    if (!vtableRuntime) return false;

    void** slot = &((void**)vtableRuntime)[slotIndex];

    // Make vtable slot writable
    DWORD oldProtect;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &oldProtect)) return false;

    // Save original function pointer as the "trampoline" (caller can call it directly)
    info->trampoline = *slot;
    // Store the slot address in target so RemoveHookVTable can restore it
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

void RemoveHookVTable(HookInfo* info) {
    if (!info->target) return;

    void** slot = (void**)info->target;
    DWORD oldProtect;
    VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &oldProtect);
    *slot = info->trampoline;
    VirtualProtect(slot, sizeof(void*), oldProtect, &oldProtect);

    info->target = nullptr;
    info->trampoline = nullptr;
}

// ============================================================
// Build number patch (data patch, not a hook)
// ============================================================

static bool PatchBuildNumber() {
    HMODULE hRuntime = GetModuleHandleA("Runtime-x64.dll");
    if (!hRuntime) return false;

    BYTE* base = (BYTE*)hRuntime;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    DWORD imageSize = nt->OptionalHeader.SizeOfImage;

    const char* dateTarget = "Sep 10 2017";
    const size_t dateLen = 11;
    BYTE* dateStr = nullptr;

    for (DWORD i = 0; i < imageSize - dateLen; i++) {
        if (memcmp(base + i, dateTarget, dateLen) == 0) {
            dateStr = base + i;
            break;
        }
    }

    if (!dateStr) return false;

    DWORD oldProtect;

    // Patch date string: "Sep 10 2017" -> "Dec 56 2012"
    VirtualProtect(dateStr, 12, PAGE_READWRITE, &oldProtect);
    memcpy(dateStr, "Dec 56 2012", 11);
    VirtualProtect(dateStr, 12, oldProtect, &oldProtect);

    // Patch month number for Dec: "12" -> "34"
    BYTE* decMonthNum = dateStr + 0x70;
    if (decMonthNum[0] == '1' && decMonthNum[1] == '2') {
        VirtualProtect(decMonthNum, 4, PAGE_READWRITE, &oldProtect);
        decMonthNum[0] = '3';
        decMonthNum[1] = '4';
        VirtualProtect(decMonthNum, 4, oldProtect, &oldProtect);
    }

    // Fallback: direct buffer overwrite
    BYTE* buildBuf = dateStr + 12;
    if (memcmp(buildBuf, "170910", 6) == 0) {
        VirtualProtect(buildBuf, 6, PAGE_READWRITE, &oldProtect);
        memcpy(buildBuf, "123456", 6);
        VirtualProtect(buildBuf, 6, oldProtect, &oldProtect);
    }

#ifdef _DEBUG
    printf("[BanishedPatch] Build number patch applied -> 123456\n");
#endif
    return true;
}

static void InstallBuildPatch() { PatchBuildNumber(); }
static void RemoveBuildPatch()  { /* data patch, no undo */ }

// ============================================================
// Reimplementation registry
// ============================================================
// Add entries here as functions are reimplemented.
// Each entry in engine/reimpl/*.cpp exposes Install/Remove functions.

// --- MainLoop reimplementations (engine/reimpl/mainloop.cpp) ---
extern void InstallMainLoopFrame();
extern void RemoveMainLoopFrame();
extern void InstallMainLoopUpdate();
extern void RemoveMainLoopUpdate();
extern void InstallMainLoopDraw();
extern void RemoveMainLoopDraw();
extern void InstallMainLoopSetNextLoop();
extern void RemoveMainLoopSetNextLoop();
extern void InstallMainLoopRun();
extern void RemoveMainLoopRun();
extern void InstallMainLoopIsRunningML();
extern void RemoveMainLoopIsRunningML();
extern void InstallMainLoopMarkFrameUpdateTime();
extern void RemoveMainLoopMarkFrameUpdateTime();
extern void InstallMainLoopAddPostUpdate();
extern void RemoveMainLoopAddPostUpdate();
extern void InstallMainLoopAddPostDraw();
extern void RemoveMainLoopAddPostDraw();
extern void InstallMainLoopRemovePostUpdate();
extern void RemoveMainLoopRemovePostUpdate();
extern void InstallMainLoopRemovePostDraw();
extern void RemoveMainLoopRemovePostDraw();
extern void InstallMainLoopOnVideoErrorStatic();
extern void RemoveMainLoopOnVideoErrorStatic();
extern void InstallMainLoopOnVideoRestartStatic();
extern void RemoveMainLoopOnVideoRestartStatic();
extern void InstallMainLoopCreateVideoCtx();
extern void RemoveMainLoopCreateVideoCtx();
extern void InstallMainLoopReleaseVideoCtx();
extern void RemoveMainLoopReleaseVideoCtx();
extern void InstallMainLoopCtor();
extern void RemoveMainLoopCtor();
extern void InstallMainLoopDtor();
extern void RemoveMainLoopDtor();

// --- UpdateLoop reimplementations (engine/reimpl/updateloop.cpp) ---
extern void InstallUpdateLoopIsRunning();
extern void RemoveUpdateLoopIsRunning();
extern void InstallUpdateLoopCtor();
extern void RemoveUpdateLoopCtor();
extern void InstallUpdateLoopDtor();
extern void RemoveUpdateLoopDtor();

// --- DialogLoaderLoopT reimplementations (engine/reimpl/dialogloaderloop.cpp) ---
extern void InstallDialogLoaderLoopStartup();
extern void RemoveDialogLoaderLoopStartup();
extern void InstallDialogLoaderLoopShutdown();
extern void RemoveDialogLoaderLoopShutdown();
extern void InstallDialogLoaderLoopUpdate();
extern void RemoveDialogLoaderLoopUpdate();
extern void InstallDialogLoaderLoopDraw();
extern void RemoveDialogLoaderLoopDraw();
extern void InstallDialogLoaderLoopLoad();
extern void RemoveDialogLoaderLoopLoad();

// --- DialogLoopT reimplementations (engine/reimpl/dialogloop.cpp) ---
extern void InstallDialogLoopStartup();
extern void RemoveDialogLoopStartup();
extern void InstallDialogLoopShutdown();
extern void RemoveDialogLoopShutdown();
extern void InstallDialogLoopUpdate();
extern void RemoveDialogLoopUpdate();
extern void InstallDialogLoopDraw();
extern void RemoveDialogLoopDraw();
extern void InstallDialogLoopLoad();
extern void RemoveDialogLoopLoad();

// --- PlatformConfigure (engine/reimpl/platformconfigure.cpp) ---
extern void InstallPlatformConfigure();
extern void RemovePlatformConfigure();

// --- Configuration reimplementations (engine/reimpl/configuration.cpp) ---
extern void InstallConfigCurrentBuildId();
extern void RemoveConfigCurrentBuildId();
extern void InstallConfigCurrentVersion();
extern void RemoveConfigCurrentVersion();
extern void InstallConfigGetVersionString();
extern void RemoveConfigGetVersionString();
extern void InstallConfigCtor();
extern void RemoveConfigCtor();
extern void InstallConfigDtor();
extern void RemoveConfigDtor();
extern void InstallConfigGetCommandLineString();
extern void RemoveConfigGetCommandLineString();
extern void InstallConfigSetCommandLine();
extern void RemoveConfigSetCommandLine();
extern void InstallConfigGetPlatformName();
extern void RemoveConfigGetPlatformName();
extern void InstallConfigGetLanguageName();
extern void RemoveConfigGetLanguageName();
extern void InstallConfigGetBuildName();
extern void RemoveConfigGetBuildName();

// --- Modification reimplementations (engine/reimpl/modification.cpp) ---
extern void InstallModificationFind();
extern void RemoveModificationFind();
extern void InstallModificationEnable();
extern void RemoveModificationEnable();
extern void InstallModificationDisableAll();
extern void RemoveModificationDisableAll();
extern void InstallModificationSwap();
extern void RemoveModificationSwap();
extern void InstallModificationEnableSavingSettings();
extern void RemoveModificationEnableSavingSettings();
extern void InstallModificationSaveList();
extern void RemoveModificationSaveList();
extern void InstallModificationRefreshList();
extern void RemoveModificationRefreshList();
extern void InstallModificationSetupFileSystem();
extern void RemoveModificationSetupFileSystem();

// --- LoaderLoop reimplementations (engine/reimpl/loaderloop.cpp) ---
extern void InstallLoaderLoopStartup();
extern void RemoveLoaderLoopStartup();
extern void InstallLoaderLoopShutdown();
extern void RemoveLoaderLoopShutdown();
extern void InstallLoaderLoopUpdate();
extern void RemoveLoaderLoopUpdate();
extern void InstallLoaderLoopDraw();
extern void RemoveLoaderLoopDraw();
extern void InstallLoaderLoopExit();
extern void RemoveLoaderLoopExit();
extern void InstallLoaderLoopIsFinished();
extern void RemoveLoaderLoopIsFinished();
extern void InstallLoaderLoopOnVideoError();
extern void RemoveLoaderLoopOnVideoError();

// --- GameUpdateLoop reimplementations (engine/reimpl/gameupdateloop.cpp) ---
extern void InstallGameUpdateLoopDestroy();
extern void RemoveGameUpdateLoopDestroy();
extern void InstallGameUpdateLoopStartup();
extern void RemoveGameUpdateLoopStartup();
extern void InstallGameUpdateLoopShutdown();
extern void RemoveGameUpdateLoopShutdown();
extern void InstallGameUpdateLoopLoad();
extern void RemoveGameUpdateLoopLoad();
extern void InstallGameUpdateLoopUpdate();
extern void RemoveGameUpdateLoopUpdate();
extern void InstallGameUpdateLoopDraw();
extern void RemoveGameUpdateLoopDraw();

// --- LivestockComponent reimplementations (engine/reimpl/livestockcomponent.cpp) ---
extern void InstallLivestockComponentUpdate();
extern void RemoveLivestockComponentUpdate();

// --- StateMachineComponent reimplementations (engine/reimpl/statemachinecomponent.cpp) ---
extern void InstallStateMachineComponentDtor();
extern void RemoveStateMachineComponentDtor();
extern void InstallStateMachineComponentShutdown();
extern void RemoveStateMachineComponentShutdown();
extern void InstallStateMachineComponentSerialize();
extern void RemoveStateMachineComponentSerialize();
extern void InstallStateMachineComponentDraw();
extern void RemoveStateMachineComponentDraw();
extern void InstallStateMachineComponentOnRemove();
extern void RemoveStateMachineComponentOnRemove();
extern void InstallStateMachineComponentUpdate();
extern void RemoveStateMachineComponentUpdate();
extern void InstallStateMachineComponentOnUpdate2();
extern void RemoveStateMachineComponentOnUpdate2();

// --- HuntComponent reimplementations (engine/reimpl/huntcomponent.cpp) ---
extern void InstallHuntComponentUpdate();
extern void RemoveHuntComponentUpdate();

// --- DiseaseComponent reimplementations (engine/reimpl/diseasecomponent.cpp) ---
extern void InstallDiseaseComponentUpdate();
extern void RemoveDiseaseComponentUpdate();

// --- AttendanceComponent reimplementations (engine/reimpl/attendancecomponent.cpp) ---
extern void InstallAttendanceComponentUpdate();
extern void RemoveAttendanceComponentUpdate();

// --- AmbientEnvironmentComponent reimplementations (engine/reimpl/ambientenvironmentcomponent.cpp) ---
extern void InstallAmbientEnvironmentComponentUpdate();
extern void RemoveAmbientEnvironmentComponentUpdate();

// --- CharacterComponent reimplementations (engine/reimpl/charactercomponent.cpp) ---
extern void InstallCharacterComponentUpdate();
extern void RemoveCharacterComponentUpdate();

// --- PopulationComponent reimplementations (engine/reimpl/populationcomponent.cpp) ---
extern void InstallPopulationComponentUpdate();
extern void RemovePopulationComponentUpdate();

// --- FruitComponent reimplementations (engine/reimpl/fruitcomponent.cpp) ---
extern void InstallFruitComponentUpdate();
extern void RemoveFruitComponentUpdate();

// --- SpawnComponent reimplementations (engine/reimpl/spawncomponent.cpp) ---
extern void InstallSpawnComponentUpdate();
extern void RemoveSpawnComponentUpdate();

// --- FireComponent reimplementations (engine/reimpl/firecomponent.cpp) ---
extern void InstallFireComponentUpdate();
extern void RemoveFireComponentUpdate();

// --- GrowthComponent reimplementations (engine/reimpl/growth.cpp) ---
extern void InstallGrowthComponentUpdate();
extern void RemoveGrowthComponentUpdate();

// --- HerdComponent reimplementations (engine/reimpl/herdcomponent.cpp) ---
extern void InstallHerdComponentUpdate();
extern void RemoveHerdComponentUpdate();

// --- AnimalComponent reimplementations (engine/reimpl/animal.cpp) ---
extern void InstallAnimalComponentUpdate();
extern void RemoveAnimalComponentUpdate();

// --- CitizenAIComponent reimplementations (engine/reimpl/citizenaicomponent.cpp) ---
extern void InstallCitizenAIComponentUpdate();
extern void RemoveCitizenAIComponentUpdate();

// --- CitizenComponent reimplementations (engine/reimpl/citizencomponent.cpp) ---
extern void InstallCitizenComponentUpdate();
extern void RemoveCitizenComponentUpdate();
extern void InstallCitizenComponentShouldSwitch();
extern void RemoveCitizenComponentShouldSwitch();

// ============================================================
// Phase 2: Passive component lifecycle methods
// ============================================================

// --- AbsorbResourceComponent (engine/reimpl/absorbresourcecomponent.cpp) ---
extern void InstallAbsorbResourceComponentShutdown();
extern void RemoveAbsorbResourceComponentShutdown();

// --- AmbientEmitterComponent (engine/reimpl/ambientemittercomponent.cpp) ---
extern void InstallAmbientEmitterComponentShutdown();
extern void RemoveAmbientEmitterComponentShutdown();
extern void InstallAmbientEmitterComponentDraw();
extern void RemoveAmbientEmitterComponentDraw();

// --- AutoPickupComponent (engine/reimpl/autopickupcomponent.cpp) ---
extern void InstallAutoPickupComponentShutdown();
extern void RemoveAutoPickupComponentShutdown();
extern void InstallAutoPickupComponentSerialize();
extern void RemoveAutoPickupComponentSerialize();

// --- BoatComponent (engine/reimpl/boatcomponent.cpp) ---
extern void InstallBoatComponentShutdown();
extern void RemoveBoatComponentShutdown();

// --- BuildComponent (engine/reimpl/buildcomponent.cpp) ---
extern void InstallBuildComponentShutdown();
extern void RemoveBuildComponentShutdown();
extern void InstallBuildComponentDraw();
extern void RemoveBuildComponentDraw();
extern void InstallBuildComponentOnCreate();
extern void RemoveBuildComponentOnCreate();

// --- CemeteryComponent (engine/reimpl/cemeterycomponent.cpp) ---
extern void InstallCemeteryComponentShutdown();
extern void RemoveCemeteryComponentShutdown();
extern void InstallCemeteryComponentDraw();
extern void RemoveCemeteryComponentDraw();
extern void InstallCemeteryComponentOnCreate();
extern void RemoveCemeteryComponentOnCreate();

// --- ClearAreaComponent (engine/reimpl/clearareacomponent.cpp) ---
extern void InstallClearAreaComponentShutdown();
extern void RemoveClearAreaComponentShutdown();

// --- ClearResourceComponent (engine/reimpl/clearresourcecomponent.cpp) ---
extern void InstallClearResourceComponentShutdown();
extern void RemoveClearResourceComponentShutdown();

// --- ConsumeProduceComponent (engine/reimpl/consumeproducecomponent.cpp) ---
extern void InstallConsumeProduceComponentShutdown();
extern void RemoveConsumeProduceComponentShutdown();
extern void InstallConsumeProduceComponentSerialize();
extern void RemoveConsumeProduceComponentSerialize();

// --- CropFieldComponent (engine/reimpl/cropfieldcomponent.cpp) ---
extern void InstallCropFieldComponentShutdown();
extern void RemoveCropFieldComponentShutdown();

// --- DecalComponent (engine/reimpl/decalcomponent.cpp) ---
extern void InstallDecalComponentShutdown();
extern void RemoveDecalComponentShutdown();
extern void InstallDecalComponentDraw();
extern void RemoveDecalComponentDraw();
extern void InstallDecalComponentOnCreate();
extern void RemoveDecalComponentOnCreate();
extern void InstallDecalComponentSerialize();
extern void RemoveDecalComponentSerialize();

// --- DestroyComponent (engine/reimpl/destroycomponent.cpp) ---
extern void InstallDestroyComponentShutdown();
extern void RemoveDestroyComponentShutdown();
extern void InstallDestroyComponentSerialize();
extern void RemoveDestroyComponentSerialize();

// --- FishingComponent (engine/reimpl/fishingcomponent.cpp) ---
extern void InstallFishingComponentShutdown();
extern void RemoveFishingComponentShutdown();

// --- FloorComponent (engine/reimpl/floorcomponent.cpp) ---
extern void InstallFloorComponentShutdown();
extern void RemoveFloorComponentShutdown();
extern void InstallFloorComponentDraw();
extern void RemoveFloorComponentDraw();
extern void InstallFloorComponentOnCreate();
extern void RemoveFloorComponentOnCreate();

// --- ForesterComponent (engine/reimpl/forestercomponent.cpp) ---
extern void InstallForesterComponentShutdown();
extern void RemoveForesterComponentShutdown();

// --- HappinessComponent (engine/reimpl/happinesscomponent.cpp) ---
extern void InstallHappinessComponentShutdown();
extern void RemoveHappinessComponentShutdown();
extern void InstallHappinessComponentOnCreate();
extern void RemoveHappinessComponentOnCreate();

// --- HealerComponent (engine/reimpl/healercomponent.cpp) ---
extern void InstallHealerComponentShutdown();
extern void RemoveHealerComponentShutdown();
extern void InstallHealerComponentOnCreate();
extern void RemoveHealerComponentOnCreate();

// --- HerbalistComponent (engine/reimpl/herbalistcomponent.cpp) ---
extern void InstallHerbalistComponentShutdown();
extern void RemoveHerbalistComponentShutdown();

// --- HighlightComponent (engine/reimpl/highlightcomponent.cpp) ---
extern void InstallHighlightComponentShutdown();
extern void RemoveHighlightComponentShutdown();

// --- ImmigrationComponent (engine/reimpl/immigrationcomponent.cpp) ---
extern void InstallImmigrationComponentShutdown();
extern void RemoveImmigrationComponentShutdown();

// --- InteractComponent (engine/reimpl/interactcomponent.cpp) ---
extern void InstallInteractComponentShutdown();
extern void RemoveInteractComponentShutdown();
extern void InstallInteractComponentDraw();
extern void RemoveInteractComponentDraw();

// --- MapComponent (engine/reimpl/mapcomponent.cpp) ---
extern void InstallMapComponentShutdown();
extern void RemoveMapComponentShutdown();
extern void InstallMapComponentDraw();
extern void RemoveMapComponentDraw();
extern void InstallMapComponentOnCreate();
extern void RemoveMapComponentOnCreate();

// --- MarketComponent (engine/reimpl/marketcomponent.cpp) ---
extern void InstallMarketComponentShutdown();
extern void RemoveMarketComponentShutdown();

// --- MarketLocationComponent (engine/reimpl/marketlocationcomponent.cpp) ---
extern void InstallMarketLocationComponentShutdown();
extern void RemoveMarketLocationComponentShutdown();
extern void InstallMarketLocationComponentOnCreate();
extern void RemoveMarketLocationComponentOnCreate();

// --- MineComponent (engine/reimpl/minecomponent.cpp) ---
extern void InstallMineComponentShutdown();
extern void RemoveMineComponentShutdown();

// --- ModelComponent (engine/reimpl/modelcomponent.cpp) ---
extern void InstallModelComponentShutdown();
extern void RemoveModelComponentShutdown();
extern void InstallModelComponentDraw();
extern void RemoveModelComponentDraw();
extern void InstallModelComponentOnCreate();
extern void RemoveModelComponentOnCreate();
extern void InstallModelComponentOnRemove();
extern void RemoveModelComponentOnRemove();
extern void InstallModelComponentSerialize();
extern void RemoveModelComponentSerialize();

// --- MultiObjectComponent (engine/reimpl/multiobjectcomponent.cpp) ---
extern void InstallMultiObjectComponentShutdown();
extern void RemoveMultiObjectComponentShutdown();
extern void InstallMultiObjectComponentOnCreate();
extern void RemoveMultiObjectComponentOnCreate();

// --- NaturalResourceComponent (engine/reimpl/naturalresourcecomponent.cpp) ---
extern void InstallNaturalResourceComponentShutdown();
extern void RemoveNaturalResourceComponentShutdown();
extern void InstallNaturalResourceComponentOnRemove();
extern void RemoveNaturalResourceComponentOnRemove();

// --- NomadComponent (engine/reimpl/nomadcomponent.cpp) ---
extern void InstallNomadComponentShutdown();
extern void RemoveNomadComponentShutdown();
extern void InstallNomadComponentOnCreate();
extern void RemoveNomadComponentOnCreate();

// --- OrchardComponent (engine/reimpl/orchardcomponent.cpp) ---
extern void InstallOrchardComponentShutdown();
extern void RemoveOrchardComponentShutdown();

// --- ParticleComponent (engine/reimpl/particlecomponent.cpp) ---
extern void InstallParticleComponentShutdown();
extern void RemoveParticleComponentShutdown();
extern void InstallParticleComponentDraw();
extern void RemoveParticleComponentDraw();
extern void InstallParticleComponentOnCreate();
extern void RemoveParticleComponentOnCreate();
extern void InstallParticleComponentOnRemove();
extern void RemoveParticleComponentOnRemove();
extern void InstallParticleComponentSerialize();
extern void RemoveParticleComponentSerialize();
extern void InstallParticleComponentOnUpdate2();
extern void RemoveParticleComponentOnUpdate2();

// --- PastureComponent (engine/reimpl/pasturecomponent.cpp) ---
extern void InstallPastureComponentShutdown();
extern void RemovePastureComponentShutdown();
extern void InstallPastureComponentDraw();
extern void RemovePastureComponentDraw();
extern void InstallPastureComponentOnCreate();
extern void RemovePastureComponentOnCreate();

// --- PickingComponent (engine/reimpl/pickingcomponent.cpp) ---
extern void InstallPickingComponentShutdown();
extern void RemovePickingComponentShutdown();
extern void InstallPickingComponentOnCreate();
extern void RemovePickingComponentOnCreate();

// --- QuarryComponent (engine/reimpl/quarrycomponent.cpp) ---
extern void InstallQuarryComponentShutdown();
extern void RemoveQuarryComponentShutdown();

// --- RawMaterialComponent (engine/reimpl/rawmaterialcomponent.cpp) ---
extern void InstallRawMaterialComponentShutdown();
extern void RemoveRawMaterialComponentShutdown();
extern void InstallRawMaterialComponentDraw();
extern void RemoveRawMaterialComponentDraw();
extern void InstallRawMaterialComponentSerialize();
extern void RemoveRawMaterialComponentSerialize();
extern void InstallRawMaterialComponentUpdate();
extern void RemoveRawMaterialComponentUpdate();

// --- ResidenceComponent (engine/reimpl/residencecomponent.cpp) ---
extern void InstallResidenceComponentShutdown();
extern void RemoveResidenceComponentShutdown();
extern void InstallResidenceComponentSerialize();
extern void RemoveResidenceComponentSerialize();
extern void InstallResidenceComponentOnCreate();
extern void RemoveResidenceComponentOnCreate();

// --- StatusIconComponent (engine/reimpl/statusiconcomponent.cpp) ---
extern void InstallStatusIconComponentShutdown();
extern void RemoveStatusIconComponentShutdown();
extern void InstallStatusIconComponentSerialize();
extern void RemoveStatusIconComponentSerialize();
extern void InstallStatusIconComponentOnRemove();
extern void RemoveStatusIconComponentOnRemove();
extern void InstallStatusIconComponentOnUpdate2();
extern void RemoveStatusIconComponentOnUpdate2();

// --- StorageComponent (engine/reimpl/storagecomponent.cpp) ---
extern void InstallStorageComponentQueryCount();
extern void RemoveStorageComponentQueryCount();
extern void InstallStorageComponentHelper_c560();
extern void RemoveStorageComponentHelper_c560();
extern void InstallStorageComponentShutdown();
extern void RemoveStorageComponentShutdown();
extern void InstallStorageComponentDraw();
extern void RemoveStorageComponentDraw();

// --- StorageDisplayComponent (engine/reimpl/storagedisplaycomponent.cpp) ---
extern void InstallStorageDisplayComponentShutdown();
extern void RemoveStorageDisplayComponentShutdown();
extern void InstallStorageDisplayComponentDraw();
extern void RemoveStorageDisplayComponentDraw();
extern void InstallStorageDisplayComponentOnCreate();
extern void RemoveStorageDisplayComponentOnCreate();
extern void InstallStorageDisplayComponentUpdate();
extern void RemoveStorageDisplayComponentUpdate();
extern void InstallStorageDisplayComponentOnUpdate2();
extern void RemoveStorageDisplayComponentOnUpdate2();

// --- StorageLocationComponent (engine/reimpl/storagelocationcomponent.cpp) ---
extern void InstallStorageLocationComponentShutdown();
extern void RemoveStorageLocationComponentShutdown();
extern void InstallStorageLocationComponentOnCreate();
extern void RemoveStorageLocationComponentOnCreate();

// --- TerraformComponent (engine/reimpl/terraformcomponent.cpp) ---
extern void InstallTerraformComponentShutdown();
extern void RemoveTerraformComponentShutdown();
extern void InstallTerraformComponentOnCreate();
extern void RemoveTerraformComponentOnCreate();

// --- TombstoneComponent (engine/reimpl/tombstonecomponent.cpp) ---
extern void InstallTombstoneComponentShutdown();
extern void RemoveTombstoneComponentShutdown();
extern void InstallTombstoneComponentSerialize();
extern void RemoveTombstoneComponentSerialize();
extern void InstallTombstoneComponentUpdate();
extern void RemoveTombstoneComponentUpdate();

// --- TornadoComponent (engine/reimpl/tornadocomponent.cpp) ---
extern void InstallTornadoComponentShutdown();
extern void RemoveTornadoComponentShutdown();
extern void InstallTornadoComponentSerialize();
extern void RemoveTornadoComponentSerialize();
extern void InstallTornadoComponentOnRemove();
extern void RemoveTornadoComponentOnRemove();

// --- TrackerComponent (engine/reimpl/trackercomponent.cpp) ---
extern void InstallTrackerComponentShutdown();
extern void RemoveTrackerComponentShutdown();

// --- TradeComponent (engine/reimpl/tradecomponent.cpp) ---
extern void InstallTradeComponentShutdown();
extern void RemoveTradeComponentShutdown();
extern void InstallTradeComponentDraw();
extern void RemoveTradeComponentDraw();
extern void InstallTradeComponentOnCreate();
extern void RemoveTradeComponentOnCreate();

// --- UIComponent (engine/reimpl/uicomponent.cpp) ---
extern void InstallUIComponentShutdown();
extern void RemoveUIComponentShutdown();
extern void InstallUIComponentSerialize();
extern void RemoveUIComponentSerialize();

// --- WellComponent (engine/reimpl/wellcomponent.cpp) ---
extern void InstallWellComponentShutdown();
extern void RemoveWellComponentShutdown();
extern void InstallWellComponentOnCreate();
extern void RemoveWellComponentOnCreate();

// --- WildAnimalComponent (engine/reimpl/wildanimalcomponent.cpp) ---
extern void InstallWildAnimalComponentShutdown();
extern void RemoveWildAnimalComponentShutdown();
extern void InstallWildAnimalComponentOnCreate();
extern void RemoveWildAnimalComponentOnCreate();

// --- WorkComponent (engine/reimpl/workcomponent.cpp) ---
extern void InstallWorkComponentShutdown();
extern void RemoveWorkComponentShutdown();
extern void InstallWorkComponentDraw();
extern void RemoveWorkComponentDraw();
extern void InstallWorkComponentOnCreate();
extern void RemoveWorkComponentOnCreate();

// --- WorkPlaceComponent (engine/reimpl/workplacecomponent.cpp) ---
extern void InstallWorkPlaceComponentShutdown();
extern void RemoveWorkPlaceComponentShutdown();
extern void InstallWorkPlaceComponentOnCreate();
extern void RemoveWorkPlaceComponentOnCreate();

// --- ZoneComponent (engine/reimpl/zonecomponent.cpp) ---
extern void InstallZoneComponentShutdown();
extern void RemoveZoneComponentShutdown();
extern void InstallZoneComponentDraw();
extern void RemoveZoneComponentDraw();
extern void InstallZoneComponentOnCreate();
extern void RemoveZoneComponentOnCreate();

// ====================================================================
// Phase 3: Action and Work class reimplementations
// ====================================================================

// --- AnimateNaturalResource (engine/reimpl/animatenaturalresource.cpp) ---
extern void InstallAnimateNaturalResourceSerialize();
extern void RemoveAnimateNaturalResourceSerialize();
extern void InstallAnimateNaturalResourcePurecall();
extern void RemoveAnimateNaturalResourcePurecall();
extern void InstallAnimateNaturalResourceExecute();
extern void RemoveAnimateNaturalResourceExecute();
extern void InstallAnimateNaturalResourceGetTarget();
extern void RemoveAnimateNaturalResourceGetTarget();

// --- AutoPickupWork (engine/reimpl/autopickupwork.cpp) ---
extern void InstallAutoPickupWorkSlot8();
extern void RemoveAutoPickupWorkSlot8();
extern void InstallAutoPickupWorkCtor();
extern void RemoveAutoPickupWorkCtor();

// --- BuildRoad (engine/reimpl/buildroad.cpp) ---
extern void InstallBuildRoadCtor();
extern void RemoveBuildRoadCtor();
extern void InstallBuildRoadScalarDtor();
extern void RemoveBuildRoadScalarDtor();
extern void InstallBuildRoadSerialize();
extern void RemoveBuildRoadSerialize();
extern void InstallBuildRoadExecute();
extern void RemoveBuildRoadExecute();
extern void InstallBuildRoadOnComplete();
extern void RemoveBuildRoadOnComplete();
extern void InstallBuildRoadOnCancel();
extern void RemoveBuildRoadOnCancel();
extern void InstallBuildRoadCleanup();
extern void RemoveBuildRoadCleanup();
extern void InstallBuildRoadValidate();
extern void RemoveBuildRoadValidate();

// --- BuildStructure (engine/reimpl/buildstructure.cpp) ---
extern void InstallBuildStructureCtor();
extern void RemoveBuildStructureCtor();
extern void InstallBuildStructureScalarDtor();
extern void RemoveBuildStructureScalarDtor();
extern void InstallBuildStructureSerialize();
extern void RemoveBuildStructureSerialize();
extern void InstallBuildStructureExecute();
extern void RemoveBuildStructureExecute();
extern void InstallBuildStructureOnComplete();
extern void RemoveBuildStructureOnComplete();
extern void InstallBuildStructureOnCancel();
extern void RemoveBuildStructureOnCancel();
extern void InstallBuildStructureCleanup();
extern void RemoveBuildStructureCleanup();

// --- ClearArea (engine/reimpl/cleararea.cpp) ---
extern void InstallClearAreaCtor();
extern void RemoveClearAreaCtor();
extern void InstallClearAreaScalarDtor();
extern void RemoveClearAreaScalarDtor();
extern void InstallClearAreaSerialize();
extern void RemoveClearAreaSerialize();
extern void InstallClearAreaExecute();
extern void RemoveClearAreaExecute();
extern void InstallClearAreaOnComplete();
extern void RemoveClearAreaOnComplete();
extern void InstallClearAreaCleanup();
extern void RemoveClearAreaCleanup();
extern void InstallClearAreaGetTarget();
extern void RemoveClearAreaGetTarget();

// --- ClearStorage (engine/reimpl/clearstorage.cpp) ---
extern void InstallClearStorageSerialize();
extern void RemoveClearStorageSerialize();
extern void InstallClearStorageSlot4();
extern void RemoveClearStorageSlot4();
extern void InstallClearStorageSlot6();
extern void RemoveClearStorageSlot6();
extern void InstallClearStorageSlot8();
extern void RemoveClearStorageSlot8();
extern void InstallClearStorageSlot9();
extern void RemoveClearStorageSlot9();
extern void InstallClearStorageSlot10();
extern void RemoveClearStorageSlot10();

// --- CompleteRoad (engine/reimpl/completeroad.cpp) ---
extern void InstallCompleteRoadSerialize();
extern void RemoveCompleteRoadSerialize();
extern void InstallCompleteRoadPurecall();
extern void RemoveCompleteRoadPurecall();

// --- CompleteTask (engine/reimpl/completetask.cpp) ---
extern void InstallCompleteTaskSerialize();
extern void RemoveCompleteTaskSerialize();
extern void InstallCompleteTaskPurecall();
extern void RemoveCompleteTaskPurecall();
extern void InstallCompleteTaskCheckEntity();
extern void RemoveCompleteTaskCheckEntity();
extern void InstallCompleteTaskGetTarget();
extern void RemoveCompleteTaskGetTarget();

// --- ConsumeProduce (engine/reimpl/consumeproduce.cpp) ---
extern void InstallConsumeProduceCtor();
extern void RemoveConsumeProduceCtor();
extern void InstallConsumeProduceScalarDtor();
extern void RemoveConsumeProduceScalarDtor();
extern void InstallConsumeProduceSerialize();
extern void RemoveConsumeProduceSerialize();

// --- ConsumeRawMaterial (engine/reimpl/consumerawmaterial.cpp) ---
extern void InstallConsumeRawMaterialSerialize();
extern void RemoveConsumeRawMaterialSerialize();
extern void InstallConsumeRawMaterialAbstractImpl();
extern void RemoveConsumeRawMaterialAbstractImpl();
extern void InstallConsumeRawMaterialCheckEntity();
extern void RemoveConsumeRawMaterialCheckEntity();

// --- CreateNaturalResource (engine/reimpl/createnaturalresource.cpp) ---
extern void InstallCreateNaturalResourceSerialize();
extern void RemoveCreateNaturalResourceSerialize();
extern void InstallCreateNaturalResourcePurecall();
extern void RemoveCreateNaturalResourcePurecall();
extern void InstallCreateNaturalResourceGetTarget();
extern void RemoveCreateNaturalResourceGetTarget();

// --- CreateRawMaterial (engine/reimpl/createrawmaterial.cpp) ---
extern void InstallCreateRawMaterialSerialize();
extern void RemoveCreateRawMaterialSerialize();
extern void InstallCreateRawMaterialAbstractImpl();
extern void RemoveCreateRawMaterialAbstractImpl();

// --- DropOffRawMaterial (engine/reimpl/dropoffrawmaterial.cpp) ---
extern void InstallDropOffRawMaterialSerialize();
extern void RemoveDropOffRawMaterialSerialize();
extern void InstallDropOffRawMaterialAbstractImpl();
extern void RemoveDropOffRawMaterialAbstractImpl();

// --- FireWork (engine/reimpl/firework.cpp) ---
extern void InstallFireWorkSerialize();
extern void RemoveFireWorkSerialize();
extern void InstallFireWorkSlot4();
extern void RemoveFireWorkSlot4();
extern void InstallFireWorkSlot6();
extern void RemoveFireWorkSlot6();
extern void InstallFireWorkSlot8();
extern void RemoveFireWorkSlot8();
extern void InstallFireWorkSlot9();
extern void RemoveFireWorkSlot9();

// --- FishingWork (engine/reimpl/fishingwork.cpp) ---
extern void InstallFishingWorkSerialize();
extern void RemoveFishingWorkSerialize();
extern void InstallFishingWorkSlot4();
extern void RemoveFishingWorkSlot4();
extern void InstallFishingWorkSlot6();
extern void RemoveFishingWorkSlot6();
extern void InstallFishingWorkSlot7();
extern void RemoveFishingWorkSlot7();
extern void InstallFishingWorkSlot8();
extern void RemoveFishingWorkSlot8();
extern void InstallFishingWorkSlot9();
extern void RemoveFishingWorkSlot9();
extern void InstallFishingWorkSlot10();
extern void RemoveFishingWorkSlot10();

// --- ForesterWork (engine/reimpl/foresterwork.cpp) ---
extern void InstallForesterWorkScalarDtor();
extern void RemoveForesterWorkScalarDtor();
extern void InstallForesterWorkSerialize();
extern void RemoveForesterWorkSerialize();
extern void InstallForesterWorkSlot4();
extern void RemoveForesterWorkSlot4();
extern void InstallForesterWorkSlot8();
extern void RemoveForesterWorkSlot8();
extern void InstallForesterWorkSlot9();
extern void RemoveForesterWorkSlot9();
extern void InstallForesterWorkSlot10();
extern void RemoveForesterWorkSlot10();

// --- GatherRawMaterials (engine/reimpl/gatherrawmaterials.cpp) ---
extern void InstallGatherRawMaterialsCtor();
extern void RemoveGatherRawMaterialsCtor();
extern void InstallGatherRawMaterialsScalarDtor();
extern void RemoveGatherRawMaterialsScalarDtor();
extern void InstallGatherRawMaterialsSerialize();
extern void RemoveGatherRawMaterialsSerialize();
extern void InstallGatherRawMaterialsExecute();
extern void RemoveGatherRawMaterialsExecute();
extern void InstallGatherRawMaterialsOnComplete();
extern void RemoveGatherRawMaterialsOnComplete();
extern void InstallGatherRawMaterialsOnCancel();
extern void RemoveGatherRawMaterialsOnCancel();
extern void InstallGatherRawMaterialsCleanup();
extern void RemoveGatherRawMaterialsCleanup();

// --- HarvestArea (engine/reimpl/harvestarea.cpp) ---
extern void InstallHarvestAreaScalarDtor();
extern void RemoveHarvestAreaScalarDtor();
extern void InstallHarvestAreaSerialize();
extern void RemoveHarvestAreaSerialize();
extern void InstallHarvestAreaSlot4();
extern void RemoveHarvestAreaSlot4();
extern void InstallHarvestAreaSlot6();
extern void RemoveHarvestAreaSlot6();
extern void InstallHarvestAreaSlot7();
extern void RemoveHarvestAreaSlot7();
extern void InstallHarvestAreaSlot8();
extern void RemoveHarvestAreaSlot8();
extern void InstallHarvestAreaSlot9();
extern void RemoveHarvestAreaSlot9();
extern void InstallHarvestAreaSlot10();
extern void RemoveHarvestAreaSlot10();

// --- HarvestOrchard (engine/reimpl/harvestorchard.cpp) ---
extern void InstallHarvestOrchardScalarDtor();
extern void RemoveHarvestOrchardScalarDtor();
extern void InstallHarvestOrchardSerialize();
extern void RemoveHarvestOrchardSerialize();
extern void InstallHarvestOrchardSlot4();
extern void RemoveHarvestOrchardSlot4();
extern void InstallHarvestOrchardSlot6();
extern void RemoveHarvestOrchardSlot6();
extern void InstallHarvestOrchardSlot7();
extern void RemoveHarvestOrchardSlot7();
extern void InstallHarvestOrchardSlot8();
extern void RemoveHarvestOrchardSlot8();
extern void InstallHarvestOrchardSlot9();
extern void RemoveHarvestOrchardSlot9();
extern void InstallHarvestOrchardSlot10();
extern void RemoveHarvestOrchardSlot10();

// --- HerbalistWork (engine/reimpl/herbalistwork.cpp) ---
extern void InstallHerbalistWorkScalarDtor();
extern void RemoveHerbalistWorkScalarDtor();
extern void InstallHerbalistWorkSerialize();
extern void RemoveHerbalistWorkSerialize();
extern void InstallHerbalistWorkSlot4();
extern void RemoveHerbalistWorkSlot4();
extern void InstallHerbalistWorkSlot8();
extern void RemoveHerbalistWorkSlot8();
extern void InstallHerbalistWorkSlot9();
extern void RemoveHerbalistWorkSlot9();
extern void InstallHerbalistWorkSlot10();
extern void RemoveHerbalistWorkSlot10();

// --- HuntAnimal (engine/reimpl/huntanimal.cpp) ---
extern void InstallHuntAnimalSerialize();
extern void RemoveHuntAnimalSerialize();
extern void InstallHuntAnimalEnter();
extern void RemoveHuntAnimalEnter();

// --- HuntWork (engine/reimpl/huntwork.cpp) ---
extern void InstallHuntWorkScalarDtor();
extern void RemoveHuntWorkScalarDtor();
extern void InstallHuntWorkSerialize();
extern void RemoveHuntWorkSerialize();
extern void InstallHuntWorkSlot4();
extern void RemoveHuntWorkSlot4();
extern void InstallHuntWorkSlot8();
extern void RemoveHuntWorkSlot8();
extern void InstallHuntWorkSlot9();
extern void RemoveHuntWorkSlot9();
extern void InstallHuntWorkSlot10();
extern void RemoveHuntWorkSlot10();

// --- LocationWork (engine/reimpl/locationwork.cpp) ---
extern void InstallLocationWorkSerialize();
extern void RemoveLocationWorkSerialize();
extern void InstallLocationWorkSlot4();
extern void RemoveLocationWorkSlot4();
extern void InstallLocationWorkSlot6();
extern void RemoveLocationWorkSlot6();
extern void InstallLocationWorkSlot7();
extern void RemoveLocationWorkSlot7();
extern void InstallLocationWorkSlot8();
extern void RemoveLocationWorkSlot8();
extern void InstallLocationWorkSlot9();
extern void RemoveLocationWorkSlot9();
extern void InstallLocationWorkSlot10();
extern void RemoveLocationWorkSlot10();

// --- MarketWork (engine/reimpl/marketwork.cpp) ---
extern void InstallMarketWorkSerialize();
extern void RemoveMarketWorkSerialize();
extern void InstallMarketWorkSlot4();
extern void RemoveMarketWorkSlot4();
extern void InstallMarketWorkSlot6();
extern void RemoveMarketWorkSlot6();
extern void InstallMarketWorkSlot8();
extern void RemoveMarketWorkSlot8();
extern void InstallMarketWorkSlot9();
extern void RemoveMarketWorkSlot9();
extern void InstallMarketWorkSlot10();
extern void RemoveMarketWorkSlot10();

// --- MoveTo (engine/reimpl/moveto.cpp) ---
extern void InstallMoveToSerialize();
extern void RemoveMoveToSerialize();
extern void InstallMoveToAbstractImpl();
extern void RemoveMoveToAbstractImpl();
extern void InstallMoveToExecute();
extern void RemoveMoveToExecute();
extern void InstallMoveToEnter();
extern void RemoveMoveToEnter();
extern void InstallMoveToExit();
extern void RemoveMoveToExit();
extern void InstallMoveToCheckEntity();
extern void RemoveMoveToCheckEntity();

// --- PastureWork (engine/reimpl/pasturework.cpp) ---
extern void InstallPastureWorkScalarDtor();
extern void RemovePastureWorkScalarDtor();
extern void InstallPastureWorkSerialize();
extern void RemovePastureWorkSerialize();
extern void InstallPastureWorkSlot4();
extern void RemovePastureWorkSlot4();
extern void InstallPastureWorkSlot7();
extern void RemovePastureWorkSlot7();
extern void InstallPastureWorkSlot8();
extern void RemovePastureWorkSlot8();
extern void InstallPastureWorkSlot9();
extern void RemovePastureWorkSlot9();
extern void InstallPastureWorkSlot10();
extern void RemovePastureWorkSlot10();

// --- PerformWork (engine/reimpl/performwork.cpp) ---
extern void InstallPerformWorkSerialize();
extern void RemovePerformWorkSerialize();
extern void InstallPerformWorkAbstractImpl();
extern void RemovePerformWorkAbstractImpl();
extern void InstallPerformWorkExecute();
extern void RemovePerformWorkExecute();
extern void InstallPerformWorkExit();
extern void RemovePerformWorkExit();
extern void InstallPerformWorkCheckEntity();
extern void RemovePerformWorkCheckEntity();

// --- Pickup (engine/reimpl/pickup.cpp) ---
extern void InstallPickupSerialize();
extern void RemovePickupSerialize();
extern void InstallPickupSlot4();
extern void RemovePickupSlot4();
extern void InstallPickupExecute();
extern void RemovePickupExecute();
extern void InstallPickupSlot6();
extern void RemovePickupSlot6();
extern void InstallPickupSlot7();
extern void RemovePickupSlot7();
extern void InstallPickupSlot8();
extern void RemovePickupSlot8();
extern void InstallPickupSlot9();
extern void RemovePickupSlot9();
extern void InstallPickupSlot10();
extern void RemovePickupSlot10();

// --- PickupRawMaterial (engine/reimpl/pickuprawmaterial.cpp) ---
extern void InstallPickupRawMaterialSerialize();
extern void RemovePickupRawMaterialSerialize();
extern void InstallPickupRawMaterialPurecall();
extern void RemovePickupRawMaterialPurecall();
extern void InstallPickupRawMaterialGetTarget();
extern void RemovePickupRawMaterialGetTarget();

// --- PickupStoredRawMaterial (engine/reimpl/pickupstoredrawmaterial.cpp) ---
extern void InstallPickupStoredRawMaterialSerialize();
extern void RemovePickupStoredRawMaterialSerialize();
extern void InstallPickupStoredRawMaterialAbstractImpl();
extern void RemovePickupStoredRawMaterialAbstractImpl();
extern void InstallPickupStoredRawMaterialCheckEntity();
extern void RemovePickupStoredRawMaterialCheckEntity();

// --- PlantArea (engine/reimpl/plantarea.cpp) ---
extern void InstallPlantAreaCtor();
extern void RemovePlantAreaCtor();
extern void InstallPlantAreaScalarDtor();
extern void RemovePlantAreaScalarDtor();
extern void InstallPlantAreaSerialize();
extern void RemovePlantAreaSerialize();

// --- PlantOrchard (engine/reimpl/plantorchard.cpp) ---
extern void InstallPlantOrchardCtor();
extern void RemovePlantOrchardCtor();
extern void InstallPlantOrchardScalarDtor();
extern void RemovePlantOrchardScalarDtor();
extern void InstallPlantOrchardSerialize();
extern void RemovePlantOrchardSerialize();

// --- PlayAnimation (engine/reimpl/playanimation.cpp) ---
extern void InstallPlayAnimationSerialize();
extern void RemovePlayAnimationSerialize();
extern void InstallPlayAnimationPurecall();
extern void RemovePlayAnimationPurecall();
extern void InstallPlayAnimationExecute();
extern void RemovePlayAnimationExecute();
extern void InstallPlayAnimationGetTarget();
extern void RemovePlayAnimationGetTarget();

// --- RemoveAnimal (engine/reimpl/removeanimal.cpp) ---
extern void InstallRemoveAnimalSerialize();
extern void RemoveRemoveAnimalSerialize();
extern void InstallRemoveAnimalPurecall();
extern void RemoveRemoveAnimalPurecall();
extern void InstallRemoveAnimalCheckEntity();
extern void RemoveRemoveAnimalCheckEntity();
extern void InstallRemoveAnimalGetTarget();
extern void RemoveRemoveAnimalGetTarget();

// --- RemoveNaturalResource (engine/reimpl/removenaturalresource.cpp) ---
extern void InstallRemoveNaturalResourceSerialize();
extern void RemoveRemoveNaturalResourceSerialize();
extern void InstallRemoveNaturalResourcePurecall();
extern void RemoveRemoveNaturalResourcePurecall();
extern void InstallRemoveNaturalResourceCheckEntity();
extern void RemoveRemoveNaturalResourceCheckEntity();
extern void InstallRemoveNaturalResourceGetTarget();
extern void RemoveRemoveNaturalResourceGetTarget();

// --- SendComponentMessage (engine/reimpl/sendcomponentmessage.cpp) ---
extern void InstallSendComponentMessageSerialize();
extern void RemoveSendComponentMessageSerialize();
extern void InstallSendComponentMessageAbstractImpl();
extern void RemoveSendComponentMessageAbstractImpl();
extern void InstallSendComponentMessageGetTarget();
extern void RemoveSendComponentMessageGetTarget();

// --- ShowTool (engine/reimpl/showtool.cpp) ---
extern void InstallShowToolSerialize();
extern void RemoveShowToolSerialize();
extern void InstallShowToolPurecall();
extern void RemoveShowToolPurecall();
extern void InstallShowToolCheckEntity();
extern void RemoveShowToolCheckEntity();
extern void InstallShowToolGetTarget();
extern void RemoveShowToolGetTarget();

// --- TendArea (engine/reimpl/tendarea.cpp) ---
extern void InstallTendAreaSerialize();
extern void RemoveTendAreaSerialize();
extern void InstallTendAreaSlot4();
extern void RemoveTendAreaSlot4();
extern void InstallTendAreaSlot6();
extern void RemoveTendAreaSlot6();
extern void InstallTendAreaSlot8();
extern void RemoveTendAreaSlot8();
extern void InstallTendAreaSlot9();
extern void RemoveTendAreaSlot9();
extern void InstallTendAreaSlot10();
extern void RemoveTendAreaSlot10();

// --- TradeWork (engine/reimpl/tradework.cpp) ---
extern void InstallTradeWorkSerialize();
extern void RemoveTradeWorkSerialize();
extern void InstallTradeWorkSlot4();
extern void RemoveTradeWorkSlot4();
extern void InstallTradeWorkSlot6();
extern void RemoveTradeWorkSlot6();
extern void InstallTradeWorkSlot8();
extern void RemoveTradeWorkSlot8();
extern void InstallTradeWorkSlot9();
extern void RemoveTradeWorkSlot9();
extern void InstallTradeWorkSlot10();
extern void RemoveTradeWorkSlot10();

// ============================================================
// Phase 4: UI Panel Controllers (A-E)
// ============================================================

// --- AttendanceUI (engine/reimpl/attendanceui.cpp) ---
extern void InstallAttendanceUIDtor();
extern void RemoveAttendanceUIDtor();
extern void InstallAttendanceUISlot4();
extern void RemoveAttendanceUISlot4();
extern void InstallAttendanceUISlot7();
extern void RemoveAttendanceUISlot7();

// --- BuildRequirementUI (engine/reimpl/buildrequirementui.cpp) ---
extern void InstallBuildRequirementUIDtor();
extern void RemoveBuildRequirementUIDtor();
extern void InstallBuildRequirementUISlot4();
extern void RemoveBuildRequirementUISlot4();

// --- BuildUI (engine/reimpl/buildui.cpp) ---
extern void InstallBuildUIDtor();
extern void RemoveBuildUIDtor();
extern void InstallBuildUISlot4();
extern void RemoveBuildUISlot4();
extern void InstallBuildUISlot7();
extern void RemoveBuildUISlot7();
extern void InstallBuildUISlot9();
extern void RemoveBuildUISlot9();

// --- CemeteryUI (engine/reimpl/cemeteryui.cpp) ---
extern void InstallCemeteryUIDtor();
extern void RemoveCemeteryUIDtor();
extern void InstallCemeteryUISlot4();
extern void RemoveCemeteryUISlot4();
extern void InstallCemeteryUISlot7();
extern void RemoveCemeteryUISlot7();

// --- CitizenUI (engine/reimpl/citizenui.cpp) ---
extern void InstallCitizenUIDtor();
extern void RemoveCitizenUIDtor();
extern void InstallCitizenUISlot4();
extern void RemoveCitizenUISlot4();
extern void InstallCitizenUISlot6();
extern void RemoveCitizenUISlot6();
extern void InstallCitizenUISlot7();
extern void RemoveCitizenUISlot7();
extern void InstallCitizenUISlot9();
extern void RemoveCitizenUISlot9();

// --- ConsumeProduceUI (engine/reimpl/consumeproduceui.cpp) ---
extern void InstallConsumeProduceUIDtor();
extern void RemoveConsumeProduceUIDtor();
extern void InstallConsumeProduceUISlot4();
extern void RemoveConsumeProduceUISlot4();
extern void InstallConsumeProduceUISlot7();
extern void RemoveConsumeProduceUISlot7();
extern void InstallConsumeProduceUISlot9();
extern void RemoveConsumeProduceUISlot9();

// --- ControlUI (engine/reimpl/controlui.cpp) ---
extern void InstallControlUIDtor();
extern void RemoveControlUIDtor();
extern void InstallControlUISlot4();
extern void RemoveControlUISlot4();
extern void InstallControlUISlot7();
extern void RemoveControlUISlot7();
extern void InstallControlUISlot9();
extern void RemoveControlUISlot9();

// --- CropUI (engine/reimpl/cropui.cpp) ---
extern void InstallCropUIDtor();
extern void RemoveCropUIDtor();
extern void InstallCropUISlot4();
extern void RemoveCropUISlot4();
extern void InstallCropUISlot7();
extern void RemoveCropUISlot7();
extern void InstallCropUISlot8();
extern void RemoveCropUISlot8();
extern void InstallCropUISlot9();
extern void RemoveCropUISlot9();

// --- DebugUI (engine/reimpl/debugui.cpp) ---
extern void InstallDebugUIDtor();
extern void RemoveDebugUIDtor();
extern void InstallDebugUISlot4();
extern void RemoveDebugUISlot4();
extern void InstallDebugUISlot7();
extern void RemoveDebugUISlot7();
extern void InstallDebugUISlot8();
extern void RemoveDebugUISlot8();
extern void InstallDebugUISlot9();
extern void RemoveDebugUISlot9();

// --- DestroyUI (engine/reimpl/destroyui.cpp) ---
extern void InstallDestroyUIDtor();
extern void RemoveDestroyUIDtor();
extern void InstallDestroyUISlot4();
extern void RemoveDestroyUISlot4();
extern void InstallDestroyUISlot7();
extern void RemoveDestroyUISlot7();
extern void InstallDestroyUISlot9();
extern void RemoveDestroyUISlot9();

// --- EntityUI (engine/reimpl/entityui.cpp) ---
extern void InstallEntityUIDtor();
extern void RemoveEntityUIDtor();
extern void InstallEntityUISlot4();
extern void RemoveEntityUISlot4();
extern void InstallEntityUISlot6();
extern void RemoveEntityUISlot6();
extern void InstallEntityUISlot7();
extern void RemoveEntityUISlot7();
extern void InstallEntityUISlot8();
extern void RemoveEntityUISlot8();
extern void InstallEntityUISlot9();
extern void RemoveEntityUISlot9();

// --- EventLogUI (engine/reimpl/eventlogui.cpp) ---
extern void InstallEventLogUIDtor();
extern void RemoveEventLogUIDtor();
extern void InstallEventLogUISlot5();
extern void RemoveEventLogUISlot5();
extern void InstallEventLogUISlot7();
extern void RemoveEventLogUISlot7();
extern void InstallEventLogUISlot9();
extern void RemoveEventLogUISlot9();

// --- HelpUI (engine/reimpl/helpui.cpp) ---
extern void InstallHelpUIDtor();
extern void RemoveHelpUIDtor();
extern void InstallHelpUISlot4();
extern void RemoveHelpUISlot4();
extern void InstallHelpUISlot7();
extern void RemoveHelpUISlot7();
extern void InstallHelpUISlot9();
extern void RemoveHelpUISlot9();

// ============================================================
// Phase 4: UI Panel Controllers (H-R)
// ============================================================

// --- HotKeyToolTipUI (engine/reimpl/hotkeytooltipu.cpp) ---
extern void InstallHotKeyToolTipUIDtor();
extern void RemoveHotKeyToolTipUIDtor();
extern void InstallHotKeyToolTipUISlot4();
extern void RemoveHotKeyToolTipUISlot4();

// --- ImmigrationUI (engine/reimpl/immigrationui.cpp) ---
extern void InstallImmigrationUIDtor();
extern void RemoveImmigrationUIDtor();
extern void InstallImmigrationUISlot4();
extern void RemoveImmigrationUISlot4();
extern void InstallImmigrationUISlot7();
extern void RemoveImmigrationUISlot7();
extern void InstallImmigrationUISlot9();
extern void RemoveImmigrationUISlot9();

// --- MapUI (engine/reimpl/mapui.cpp) ---
extern void InstallMapUIDtor();
extern void RemoveMapUIDtor();
extern void InstallMapUISlot4();
extern void RemoveMapUISlot4();
extern void InstallMapUISlot9();
extern void RemoveMapUISlot9();

// --- ModBrowseUI (engine/reimpl/modbrowseui.cpp) ---
extern void InstallModBrowseUIDtor();
extern void RemoveModBrowseUIDtor();

// --- NoiseUI (engine/reimpl/noiseui.cpp) ---
extern void InstallNoiseUIDtor();
extern void RemoveNoiseUIDtor();
extern void InstallNoiseUISlot4();
extern void RemoveNoiseUISlot4();
extern void InstallNoiseUISlot9();
extern void RemoveNoiseUISlot9();

// --- OrchardUI (engine/reimpl/orchardui.cpp) ---
extern void InstallOrchardUIDtor();
extern void RemoveOrchardUIDtor();
extern void InstallOrchardUISlot4();
extern void RemoveOrchardUISlot4();
extern void InstallOrchardUISlot7();
extern void RemoveOrchardUISlot7();
extern void InstallOrchardUISlot8();
extern void RemoveOrchardUISlot8();
extern void InstallOrchardUISlot9();
extern void RemoveOrchardUISlot9();
extern void InstallOrchardUISlot11();
extern void RemoveOrchardUISlot11();
extern void InstallOrchardUISlot12();
extern void RemoveOrchardUISlot12();

// --- PageToggleUI (engine/reimpl/pagetoggleui.cpp) ---
extern void InstallPageToggleUIDtor();
extern void RemovePageToggleUIDtor();
extern void InstallPageToggleUISlot4();
extern void RemovePageToggleUISlot4();
extern void InstallPageToggleUISlot7();
extern void RemovePageToggleUISlot7();
extern void InstallPageToggleUISlot9();
extern void RemovePageToggleUISlot9();

// --- PastureUI (engine/reimpl/pastureui.cpp) ---
extern void InstallPastureUIDtor();
extern void RemovePastureUIDtor();
extern void InstallPastureUISlot4();
extern void RemovePastureUISlot4();
extern void InstallPastureUISlot7();
extern void RemovePastureUISlot7();
extern void InstallPastureUISlot8();
extern void RemovePastureUISlot8();
extern void InstallPastureUISlot9();
extern void RemovePastureUISlot9();
extern void InstallPastureUISlot11();
extern void RemovePastureUISlot11();
extern void InstallPastureUISlot12();
extern void RemovePastureUISlot12();

// --- ProductionUI (engine/reimpl/productionui.cpp) ---
extern void InstallProductionUIDtor();
extern void RemoveProductionUIDtor();
extern void InstallProductionUISlot4();
extern void RemoveProductionUISlot4();
extern void InstallProductionUISlot7();
extern void RemoveProductionUISlot7();

// --- ProfessionUI (engine/reimpl/professionui.cpp) ---
extern void InstallProfessionUIDtor();
extern void RemoveProfessionUIDtor();
extern void InstallProfessionUISlot5();
extern void RemoveProfessionUISlot5();
extern void InstallProfessionUISlot8();
extern void RemoveProfessionUISlot8();
extern void InstallProfessionUISlot9();
extern void RemoveProfessionUISlot9();

// --- RadiusUI (engine/reimpl/radiusui.cpp) ---
extern void InstallRadiusUIDtor();
extern void RemoveRadiusUIDtor();
extern void InstallRadiusUISlot5();
extern void RemoveRadiusUISlot5();
extern void InstallRadiusUISlot6();
extern void RemoveRadiusUISlot6();

// --- ResidenceUI (engine/reimpl/residenceui.cpp) ---
extern void InstallResidenceUIDtor();
extern void RemoveResidenceUIDtor();
extern void InstallResidenceUISlot4();
extern void RemoveResidenceUISlot4();
extern void InstallResidenceUISlot7();
extern void RemoveResidenceUISlot7();
extern void InstallResidenceUISlot9();
extern void RemoveResidenceUISlot9();

// --- ResourceLimitUI (engine/reimpl/resourcelimitui.cpp) ---
extern void InstallResourceLimitUIDtor();
extern void RemoveResourceLimitUIDtor();
extern void InstallResourceLimitUISlot4();
extern void RemoveResourceLimitUISlot4();
extern void InstallResourceLimitUISlot7();
extern void RemoveResourceLimitUISlot7();
extern void InstallResourceLimitUISlot9();
extern void RemoveResourceLimitUISlot9();

// ============================================================
// Phase 4: UI Panel Controllers (S-W)
// ============================================================

// --- SpriteCaptureUI (engine/reimpl/spritecaptureui.cpp) ---
extern void InstallSpriteCaptureUIScalarDtor();
extern void RemoveSpriteCaptureUIScalarDtor();
extern void InstallSpriteCaptureUISerialize();
extern void RemoveSpriteCaptureUISerialize();
extern void InstallSpriteCaptureUIOnCreate();
extern void RemoveSpriteCaptureUIOnCreate();

// --- StatusBarUI (engine/reimpl/statusbarui.cpp) ---
extern void InstallStatusBarUIScalarDtor();
extern void RemoveStatusBarUIScalarDtor();
extern void InstallStatusBarUIDraw();
extern void RemoveStatusBarUIDraw();
extern void InstallStatusBarUIStartup();
extern void RemoveStatusBarUIStartup();
extern void InstallStatusBarUIOnRemove();
extern void RemoveStatusBarUIOnRemove();

// --- StatusIconUI (engine/reimpl/statusiconui.cpp) ---
extern void InstallStatusIconUIScalarDtor();
extern void RemoveStatusIconUIScalarDtor();
extern void InstallStatusIconUISerialize();
extern void RemoveStatusIconUISerialize();
extern void InstallStatusIconUIStartup();
extern void RemoveStatusIconUIStartup();

// --- StorageCapacityUI (engine/reimpl/storagecapacityui.cpp) ---
extern void InstallStorageCapacityUIScalarDtor();
extern void RemoveStorageCapacityUIScalarDtor();
extern void InstallStorageCapacityUISerialize();
extern void RemoveStorageCapacityUISerialize();
extern void InstallStorageCapacityUIStartup();
extern void RemoveStorageCapacityUIStartup();

// --- StorageUI (engine/reimpl/storageui.cpp) ---
extern void InstallStorageUIScalarDtor();
extern void RemoveStorageUIScalarDtor();
extern void InstallStorageUISerialize();
extern void RemoveStorageUISerialize();
extern void InstallStorageUIStartup();
extern void RemoveStorageUIStartup();

// --- TitleDescToolTipUI (engine/reimpl/titledesctooltipui.cpp) ---
extern void InstallTitleDescToolTipUIScalarDtor();
extern void RemoveTitleDescToolTipUIScalarDtor();
extern void InstallTitleDescToolTipUISerialize();
extern void RemoveTitleDescToolTipUISerialize();

// --- ToolUI (engine/reimpl/toolui.cpp) ---
extern void InstallToolUIScalarDtor();
extern void RemoveToolUIScalarDtor();
extern void InstallToolUISerialize();
extern void RemoveToolUISerialize();
extern void InstallToolUIOnCreate();
extern void RemoveToolUIOnCreate();
extern void InstallToolUIOnRemove();
extern void RemoveToolUIOnRemove();

// --- TownHallUI (engine/reimpl/townhallui.cpp) ---
extern void InstallTownHallUIScalarDtor();
extern void RemoveTownHallUIScalarDtor();
extern void InstallTownHallUISerialize();
extern void RemoveTownHallUISerialize();
extern void InstallTownHallUIStartup();
extern void RemoveTownHallUIStartup();
extern void InstallTownHallUIOnCreate();
extern void RemoveTownHallUIOnCreate();
extern void InstallTownHallUIOnRemove();
extern void RemoveTownHallUIOnRemove();
extern void InstallTownHallUIUpdate();
extern void RemoveTownHallUIUpdate();

// --- TradeUI (engine/reimpl/tradeui.cpp) ---
extern void InstallTradeUIScalarDtor();
extern void RemoveTradeUIScalarDtor();
extern void InstallTradeUISerialize();
extern void RemoveTradeUISerialize();
extern void InstallTradeUIDraw();
extern void RemoveTradeUIDraw();
extern void InstallTradeUISlot6();
extern void RemoveTradeUISlot6();
extern void InstallTradeUIStartup();
extern void RemoveTradeUIStartup();
extern void InstallTradeUIOnCreate();
extern void RemoveTradeUIOnCreate();
extern void InstallTradeUIOnRemove();
extern void RemoveTradeUIOnRemove();
extern void InstallTradeUIUpdate();
extern void RemoveTradeUIUpdate();

// --- TutorialUI (engine/reimpl/tutorialui.cpp) ---
extern void InstallTutorialUIScalarDtor();
extern void RemoveTutorialUIScalarDtor();
extern void InstallTutorialUISerialize();
extern void RemoveTutorialUISerialize();
extern void InstallTutorialUIStartup();
extern void RemoveTutorialUIStartup();
extern void InstallTutorialUIOnRemove();
extern void RemoveTutorialUIOnRemove();

// --- UpgradeUI (engine/reimpl/upgradeui.cpp) ---
extern void InstallUpgradeUIScalarDtor();
extern void RemoveUpgradeUIScalarDtor();
extern void InstallUpgradeUISerialize();
extern void RemoveUpgradeUISerialize();
extern void InstallUpgradeUIOnRemove();
extern void RemoveUpgradeUIOnRemove();

// --- WorkPlaceUI (engine/reimpl/workplaceui.cpp) ---
extern void InstallWorkPlaceUIScalarDtor();
extern void RemoveWorkPlaceUIScalarDtor();
extern void InstallWorkPlaceUISerialize();
extern void RemoveWorkPlaceUISerialize();
extern void InstallWorkPlaceUIStartup();
extern void RemoveWorkPlaceUIStartup();
extern void InstallWorkPlaceUIOnRemove();
extern void RemoveWorkPlaceUIOnRemove();

// ====================================================================
// Phase 4: Dialog Controller reimplementations
// ====================================================================

// --- EventNotifyDialog (engine/reimpl/eventnotifydialog.cpp) ---
extern void InstallEventNotifyDialogScalarDtor();
extern void RemoveEventNotifyDialogScalarDtor();
extern void InstallEventNotifyDialogStartup();
extern void RemoveEventNotifyDialogStartup();
extern void InstallEventNotifyDialogUpdate();
extern void RemoveEventNotifyDialogUpdate();
extern void InstallEventNotifyDialogLoad();
extern void RemoveEventNotifyDialogLoad();
extern void InstallEventNotifyDialogOnEvent();
extern void RemoveEventNotifyDialogOnEvent();

// --- MapParamsDialog (engine/reimpl/mapparamsdialog.cpp) ---
extern void InstallMapParamsDialogScalarDtor();
extern void RemoveMapParamsDialogScalarDtor();
extern void InstallMapParamsDialogStartup();
extern void RemoveMapParamsDialogStartup();
extern void InstallMapParamsDialogUpdate();
extern void RemoveMapParamsDialogUpdate();
extern void InstallMapParamsDialogOnEvent();
extern void RemoveMapParamsDialogOnEvent();

// --- NewGameDialog (engine/reimpl/newgamedialog.cpp) ---
extern void InstallNewGameDialogScalarDtor();
extern void RemoveNewGameDialogScalarDtor();
extern void InstallNewGameDialogStartup();
extern void RemoveNewGameDialogStartup();
extern void InstallNewGameDialogUpdate();
extern void RemoveNewGameDialogUpdate();
extern void InstallNewGameDialogOnEvent();
extern void RemoveNewGameDialogOnEvent();

// --- OptionsDialog (engine/reimpl/optionsdialog.cpp) ---
extern void InstallOptionsDialogScalarDtor();
extern void RemoveOptionsDialogScalarDtor();
extern void InstallOptionsDialogStartup();
extern void RemoveOptionsDialogStartup();
extern void InstallOptionsDialogOnEvent();
extern void RemoveOptionsDialogOnEvent();

// --- OptionInputBindDialog (engine/reimpl/optioninputbinddialog.cpp) ---
extern void InstallOptionInputBindDialogScalarDtor();
extern void RemoveOptionInputBindDialogScalarDtor();
extern void InstallOptionInputBindDialogStartup();
extern void RemoveOptionInputBindDialogStartup();
extern void InstallOptionInputBindDialogLoad();
extern void RemoveOptionInputBindDialogLoad();
extern void InstallOptionInputBindDialogOnEvent();
extern void RemoveOptionInputBindDialogOnEvent();
extern void InstallOptionInputBindDialogOnOk();
extern void RemoveOptionInputBindDialogOnOk();
extern void InstallOptionInputBindDialogOnCancel();
extern void RemoveOptionInputBindDialogOnCancel();
extern void InstallOptionInputBindDialogOnButton2();
extern void RemoveOptionInputBindDialogOnButton2();
extern void InstallOptionInputBindDialogOnButton3();
extern void RemoveOptionInputBindDialogOnButton3();

// --- SaveLoadDialog (engine/reimpl/saveloaddialog.cpp) ---
extern void InstallSaveLoadDialogScalarDtor();
extern void RemoveSaveLoadDialogScalarDtor();
extern void InstallSaveLoadDialogStartup();
extern void RemoveSaveLoadDialogStartup();
extern void InstallSaveLoadDialogUpdate();
extern void RemoveSaveLoadDialogUpdate();
extern void InstallSaveLoadDialogLoad();
extern void RemoveSaveLoadDialogLoad();
extern void InstallSaveLoadDialogOnEvent();
extern void RemoveSaveLoadDialogOnEvent();

// --- ToolbarDialog (engine/reimpl/toolbardialog.cpp) ---
extern void InstallToolbarDialogScalarDtor();
extern void RemoveToolbarDialogScalarDtor();
extern void InstallToolbarDialogUpdate();
extern void RemoveToolbarDialogUpdate();
extern void InstallToolbarDialogOnEvent();
extern void RemoveToolbarDialogOnEvent();

// --- TutorialHighlightDialog (engine/reimpl/tutorialhighlightdialog.cpp) ---
extern void InstallTutorialHighlightDialogScalarDtor();
extern void RemoveTutorialHighlightDialogScalarDtor();
extern void InstallTutorialHighlightDialogStartup();
extern void RemoveTutorialHighlightDialogStartup();
extern void InstallTutorialHighlightDialogUpdate();
extern void RemoveTutorialHighlightDialogUpdate();
extern void InstallTutorialHighlightDialogLoad();
extern void RemoveTutorialHighlightDialogLoad();

// --- TutorialGoalDialog (engine/reimpl/tutorialgoaldialog.cpp) ---
extern void InstallTutorialGoalDialogScalarDtor();
extern void RemoveTutorialGoalDialogScalarDtor();
extern void InstallTutorialGoalDialogShutdown();
extern void RemoveTutorialGoalDialogShutdown();
extern void InstallTutorialGoalDialogOnEvent();
extern void RemoveTutorialGoalDialogOnEvent();
extern void InstallTutorialGoalDialogOnInit();
extern void RemoveTutorialGoalDialogOnInit();
extern void InstallTutorialGoalDialogOnDestroy();
extern void RemoveTutorialGoalDialogOnDestroy();

// --- TutorialSetupDialog (engine/reimpl/tutorialsetupdialog.cpp) ---
extern void InstallTutorialSetupDialogScalarDtor();
extern void RemoveTutorialSetupDialogScalarDtor();
extern void InstallTutorialSetupDialogShutdown();
extern void RemoveTutorialSetupDialogShutdown();
extern void InstallTutorialSetupDialogOnEvent();
extern void RemoveTutorialSetupDialogOnEvent();
extern void InstallTutorialSetupDialogSlot8();
extern void RemoveTutorialSetupDialogSlot8();
extern void InstallTutorialSetupDialogSlot9();
extern void RemoveTutorialSetupDialogSlot9();
extern void InstallTutorialSetupDialogSlot10();
extern void RemoveTutorialSetupDialogSlot10();

// ====================================================================
// Phase 4: Simple Dialog Controllers (dialogs-simple)
// ====================================================================

// --- AchievementDialog (engine/reimpl/achievementdialog.cpp) ---
extern void InstallAchievementDialogDtor();
extern void RemoveAchievementDialogDtor();
extern void InstallAchievementDialogSlot4();
extern void RemoveAchievementDialogSlot4();
extern void InstallAchievementDialogSlot6();
extern void RemoveAchievementDialogSlot6();
extern void InstallAchievementDialogSlot8();
extern void RemoveAchievementDialogSlot8();
extern void InstallAchievementDialogSlot10();
extern void RemoveAchievementDialogSlot10();
extern void InstallAchievementDialogSlot14();
extern void RemoveAchievementDialogSlot14();
extern void InstallAchievementDialogSlot17();
extern void RemoveAchievementDialogSlot17();

// --- AutoSaveDialog (engine/reimpl/autosavedialog.cpp) ---
extern void InstallAutoSaveDialogDtor();
extern void RemoveAutoSaveDialogDtor();
extern void InstallAutoSaveDialogSlot6();
extern void RemoveAutoSaveDialogSlot6();
extern void InstallAutoSaveDialogSlot7();
extern void RemoveAutoSaveDialogSlot7();

// --- EventPopupDialog (engine/reimpl/eventpopupdialog.cpp) ---
extern void InstallEventPopupDialogDtor();
extern void RemoveEventPopupDialogDtor();
extern void InstallEventPopupDialogSlot4();
extern void RemoveEventPopupDialogSlot4();
extern void InstallEventPopupDialogSlot8();
extern void RemoveEventPopupDialogSlot8();

// --- FollowDialog (engine/reimpl/followdialog.cpp) ---
extern void InstallFollowDialogDtor();
extern void RemoveFollowDialogDtor();
extern void InstallFollowDialogSlot4();
extern void RemoveFollowDialogSlot4();
extern void InstallFollowDialogSlot5();
extern void RemoveFollowDialogSlot5();
extern void InstallFollowDialogSlot6();
extern void RemoveFollowDialogSlot6();
extern void InstallFollowDialogSlot7();
extern void RemoveFollowDialogSlot7();
extern void InstallFollowDialogSlot8();
extern void RemoveFollowDialogSlot8();

// --- GoalDialog (engine/reimpl/goaldialog.cpp) ---
extern void InstallGoalDialogDtor();
extern void RemoveGoalDialogDtor();
extern void InstallGoalDialogSlot4();
extern void RemoveGoalDialogSlot4();
extern void InstallGoalDialogSlot6();
extern void RemoveGoalDialogSlot6();
extern void InstallGoalDialogSlot8();
extern void RemoveGoalDialogSlot8();

// --- LoaderDialog (engine/reimpl/loaderdialog.cpp) ---
extern void InstallLoaderDialogDtor();
extern void RemoveLoaderDialogDtor();
extern void InstallLoaderDialogSlot4();
extern void RemoveLoaderDialogSlot4();
extern void InstallLoaderDialogSlot6();
extern void RemoveLoaderDialogSlot6();
extern void InstallLoaderDialogSlot7();
extern void RemoveLoaderDialogSlot7();

// --- MainMenuDialog (engine/reimpl/mainmenudialog.cpp) ---
extern void InstallMainMenuDialogDtor();
extern void RemoveMainMenuDialogDtor();
extern void InstallMainMenuDialogSlot4();
extern void RemoveMainMenuDialogSlot4();
extern void InstallMainMenuDialogSlot5();
extern void RemoveMainMenuDialogSlot5();
extern void InstallMainMenuDialogSlot8();
extern void RemoveMainMenuDialogSlot8();
extern void InstallMainMenuDialogSlot10();
extern void RemoveMainMenuDialogSlot10();

// --- MessageDialog (engine/reimpl/messagedialog.cpp) ---
extern void InstallMessageDialogDtor();
extern void RemoveMessageDialogDtor();
extern void InstallMessageDialogSlot4();
extern void RemoveMessageDialogSlot4();
extern void InstallMessageDialogSlot10();
extern void RemoveMessageDialogSlot10();
extern void InstallMessageDialogSlot11();
extern void RemoveMessageDialogSlot11();
extern void InstallMessageDialogSlot12();
extern void RemoveMessageDialogSlot12();
extern void InstallMessageDialogSlot13();
extern void RemoveMessageDialogSlot13();

// --- ModDetailsDialog (engine/reimpl/moddetailsdialog.cpp) ---
extern void InstallModDetailsDialogDtor();
extern void RemoveModDetailsDialogDtor();
extern void InstallModDetailsDialogSlot4();
extern void RemoveModDetailsDialogSlot4();
extern void InstallModDetailsDialogSlot8();
extern void RemoveModDetailsDialogSlot8();

// --- ModUploadDialog (engine/reimpl/moduploaddialog.cpp) ---
extern void InstallModUploadDialogDtor();
extern void RemoveModUploadDialogDtor();

// --- ModDialog (engine/reimpl/moddialog.cpp) ---
extern void InstallModDialogDtor();
extern void RemoveModDialogDtor();
extern void InstallModDialogSlot4();
extern void RemoveModDialogSlot4();
extern void InstallModDialogSlot8();
extern void RemoveModDialogSlot8();
extern void InstallModDialogSlot10();
extern void RemoveModDialogSlot10();

// --- ScenarioDialog (engine/reimpl/scenariodialog.cpp) ---
extern void InstallScenarioDialogDtor();
extern void RemoveScenarioDialogDtor();
extern void InstallScenarioDialogSlot4();
extern void RemoveScenarioDialogSlot4();
extern void InstallScenarioDialogSlot6();
extern void RemoveScenarioDialogSlot6();
extern void InstallScenarioDialogSlot8();
extern void RemoveScenarioDialogSlot8();

// ====================================================================
// Phase 5: Descriptions (engine/reimpl/create*description.cpp etc.)
// ====================================================================
extern void InstallCreateBridgeDescriptionDtor(); extern void RemoveCreateBridgeDescriptionDtor();
extern void InstallCreateBridgeDescriptionSerialize(); extern void RemoveCreateBridgeDescriptionSerialize();
extern void InstallCreateBridgeDescriptionCleanup(); extern void RemoveCreateBridgeDescriptionCleanup();
extern void InstallCreateDragDescriptionDtor(); extern void RemoveCreateDragDescriptionDtor();
extern void InstallCreateDragDescriptionSerialize(); extern void RemoveCreateDragDescriptionSerialize();
extern void InstallCreateDragDescriptionCleanup(); extern void RemoveCreateDragDescriptionCleanup();
extern void InstallCreateFencedDescriptionDtor(); extern void RemoveCreateFencedDescriptionDtor();
extern void InstallCreateFencedDescriptionSerialize(); extern void RemoveCreateFencedDescriptionSerialize();
extern void InstallCreateFencedDescriptionCleanup(); extern void RemoveCreateFencedDescriptionCleanup();
extern void InstallCreatePathDescriptionDtor(); extern void RemoveCreatePathDescriptionDtor();
extern void InstallCreatePathDescriptionSerialize(); extern void RemoveCreatePathDescriptionSerialize();
extern void InstallCreatePathDescriptionCleanup(); extern void RemoveCreatePathDescriptionCleanup();
extern void InstallCreatePathDescriptionCreateInPlace(); extern void RemoveCreatePathDescriptionCreateInPlace();
extern void InstallCreatePlacedDescriptionDtor(); extern void RemoveCreatePlacedDescriptionDtor();
extern void InstallCreatePlacedDescriptionSerialize(); extern void RemoveCreatePlacedDescriptionSerialize();
extern void InstallCreatePlacedDescriptionCleanup(); extern void RemoveCreatePlacedDescriptionCleanup();
extern void InstallCreateRemovalDescriptionDtor(); extern void RemoveCreateRemovalDescriptionDtor();
extern void InstallCreateRemovalDescriptionSerialize(); extern void RemoveCreateRemovalDescriptionSerialize();
extern void InstallCreateRemovalDescriptionCleanup(); extern void RemoveCreateRemovalDescriptionCleanup();
extern void InstallCreateRemovalDescriptionCreateInPlace(); extern void RemoveCreateRemovalDescriptionCreateInPlace();
extern void InstallCreateRoadDescriptionDtor(); extern void RemoveCreateRoadDescriptionDtor();
extern void InstallCreateRoadDescriptionSerialize(); extern void RemoveCreateRoadDescriptionSerialize();
extern void InstallCreateRoadDescriptionCleanup(); extern void RemoveCreateRoadDescriptionCleanup();
extern void InstallCreateRoadDescriptionCreateInPlace(); extern void RemoveCreateRoadDescriptionCreateInPlace();
extern void InstallRadiusDescriptionDtor(); extern void RemoveRadiusDescriptionDtor();
extern void InstallRadiusDescriptionSerialize(); extern void RemoveRadiusDescriptionSerialize();
extern void InstallRadiusDescriptionCleanup(); extern void RemoveRadiusDescriptionCleanup();
extern void InstallRadiusDescriptionCreateInPlace(); extern void RemoveRadiusDescriptionCreateInPlace();
extern void InstallSelectDescriptionDtor(); extern void RemoveSelectDescriptionDtor();
extern void InstallSelectDescriptionSerialize(); extern void RemoveSelectDescriptionSerialize();
extern void InstallSelectDescriptionCleanup(); extern void RemoveSelectDescriptionCleanup();
extern void InstallSelectDescriptionCreateInPlace(); extern void RemoveSelectDescriptionCreateInPlace();
extern void InstallValueDescriptionDtor(); extern void RemoveValueDescriptionDtor();
extern void InstallValueDescriptionSerialize(); extern void RemoveValueDescriptionSerialize();
extern void InstallValueDescriptionCleanup(); extern void RemoveValueDescriptionCleanup();
extern void InstallValueDescriptionCreateInPlace(); extern void RemoveValueDescriptionCreateInPlace();
extern void InstallToolbarDescriptionDtor(); extern void RemoveToolbarDescriptionDtor();
extern void InstallToolbarDescriptionSerialize(); extern void RemoveToolbarDescriptionSerialize();
extern void InstallToolbarDescriptionCleanup(); extern void RemoveToolbarDescriptionCleanup();
extern void InstallEnvironmentDescriptionDtor(); extern void RemoveEnvironmentDescriptionDtor();
extern void InstallEnvironmentDescriptionSerialize(); extern void RemoveEnvironmentDescriptionSerialize();
extern void InstallEnvironmentDescriptionCleanup(); extern void RemoveEnvironmentDescriptionCleanup();
extern void InstallCaptureBoxDescriptionDtor(); extern void RemoveCaptureBoxDescriptionDtor();
extern void InstallCaptureBoxDescriptionSlot3(); extern void RemoveCaptureBoxDescriptionSlot3();
extern void InstallCaptureBoxDescriptionSerialize(); extern void RemoveCaptureBoxDescriptionSerialize();
extern void InstallCaptureBoxDescriptionCleanup(); extern void RemoveCaptureBoxDescriptionCleanup();
extern void InstallCaptureBoxDescriptionCreateElement(); extern void RemoveCaptureBoxDescriptionCreateElement();
extern void InstallCaptureBoxDescriptionCreateInPlace(); extern void RemoveCaptureBoxDescriptionCreateInPlace();
extern void InstallFrustumOutlineDescriptionDtor(); extern void RemoveFrustumOutlineDescriptionDtor();
extern void InstallFrustumOutlineDescriptionSlot3(); extern void RemoveFrustumOutlineDescriptionSlot3();
extern void InstallFrustumOutlineDescriptionSerialize(); extern void RemoveFrustumOutlineDescriptionSerialize();
extern void InstallFrustumOutlineDescriptionCleanup(); extern void RemoveFrustumOutlineDescriptionCleanup();
extern void InstallFrustumOutlineDescriptionCreateElement(); extern void RemoveFrustumOutlineDescriptionCreateElement();
extern void InstallFrustumOutlineDescriptionCreateInPlace(); extern void RemoveFrustumOutlineDescriptionCreateInPlace();
// UI Elements
extern void InstallCaptureBoxElementDtor(); extern void RemoveCaptureBoxElementDtor();
extern void InstallCaptureBoxElementOnCreate(); extern void RemoveCaptureBoxElementOnCreate();
extern void InstallCaptureBoxElementComputeLayout(); extern void RemoveCaptureBoxElementComputeLayout();
extern void InstallCaptureBoxElementDraw(); extern void RemoveCaptureBoxElementDraw();
extern void InstallFrustumOutlineElementDtor(); extern void RemoveFrustumOutlineElementDtor();
extern void InstallFrustumOutlineElementOnCreate(); extern void RemoveFrustumOutlineElementOnCreate();
extern void InstallFrustumOutlineElementComputeLayout(); extern void RemoveFrustumOutlineElementComputeLayout();
extern void InstallFrustumOutlineElementDraw(); extern void RemoveFrustumOutlineElementDraw();

// ====================================================================
// Phase 5: Tools & Placement
// ====================================================================
extern void InstallToolbarDtor(); extern void RemoveToolbarDtor();
extern void InstallToolbarSerialize(); extern void RemoveToolbarSerialize();
extern void InstallToolbarCleanup(); extern void RemoveToolbarCleanup();
extern void InstallRotationSettingDtor(); extern void RemoveRotationSettingDtor();
extern void InstallSetTextParams(); extern void RemoveSetTextParams();
extern void InstallDragToolDtor(); extern void RemoveDragToolDtor();
extern void InstallDragToolOnKey(); extern void RemoveDragToolOnKey();
extern void InstallBridgeToolDtor(); extern void RemoveBridgeToolDtor();
extern void InstallBridgeToolStartup(); extern void RemoveBridgeToolStartup();
extern void InstallBridgeToolCalcPath(); extern void RemoveBridgeToolCalcPath();
extern void InstallBridgeToolCalcCost(); extern void RemoveBridgeToolCalcCost();
extern void InstallDragPlacementToolDtor(); extern void RemoveDragPlacementToolDtor();
extern void InstallDragPlacementToolStartup(); extern void RemoveDragPlacementToolStartup();
extern void InstallDragPlacementToolDraw(); extern void RemoveDragPlacementToolDraw();
extern void InstallDragPlacementToolCalcPath(); extern void RemoveDragPlacementToolCalcPath();
extern void InstallDragPlacementToolCalcCost(); extern void RemoveDragPlacementToolCalcCost();
extern void InstallEditToolDtor(); extern void RemoveEditToolDtor();
extern void InstallEditToolOnKey(); extern void RemoveEditToolOnKey();
extern void InstallEditToolStartup(); extern void RemoveEditToolStartup();
extern void InstallEditToolSerialize(); extern void RemoveEditToolSerialize();
extern void InstallFencedToolDtor(); extern void RemoveFencedToolDtor();
extern void InstallFencedToolStartup(); extern void RemoveFencedToolStartup();
extern void InstallFencedToolShutdown2(); extern void RemoveFencedToolShutdown2();
extern void InstallFencedToolOnKey(); extern void RemoveFencedToolOnKey();
extern void InstallFencedToolSerialize(); extern void RemoveFencedToolSerialize();
extern void InstallFencedToolDraw(); extern void RemoveFencedToolDraw();
extern void InstallFencedToolShutdown(); extern void RemoveFencedToolShutdown();
extern void InstallFencedToolCalcPath(); extern void RemoveFencedToolCalcPath();
extern void InstallFencedToolCalcCost(); extern void RemoveFencedToolCalcCost();
extern void InstallPathToolDtor(); extern void RemovePathToolDtor();
extern void InstallPathToolOnKey(); extern void RemovePathToolOnKey();
extern void InstallPathToolDraw(); extern void RemovePathToolDraw();
extern void InstallPathToolSerialize(); extern void RemovePathToolSerialize();
extern void InstallPlacementToolDtor(); extern void RemovePlacementToolDtor();
extern void InstallPlacementToolStartup(); extern void RemovePlacementToolStartup();
extern void InstallPlacementToolShutdown2(); extern void RemovePlacementToolShutdown2();
extern void InstallPlacementToolOnKey(); extern void RemovePlacementToolOnKey();
extern void InstallPlacementToolSerialize(); extern void RemovePlacementToolSerialize();
extern void InstallPlacementToolDraw(); extern void RemovePlacementToolDraw();
extern void InstallRemovalToolDtor(); extern void RemoveRemovalToolDtor();
extern void InstallRemovalToolStartup(); extern void RemoveRemovalToolStartup();
extern void InstallRemovalToolDraw(); extern void RemoveRemovalToolDraw();
extern void InstallRemovalToolCalcPath(); extern void RemoveRemovalToolCalcPath();
extern void InstallRemovalToolCalcCost(); extern void RemoveRemovalToolCalcCost();
extern void InstallRoadToolDtor(); extern void RemoveRoadToolDtor();
extern void InstallRoadToolStartup(); extern void RemoveRoadToolStartup();
extern void InstallRoadToolDraw(); extern void RemoveRoadToolDraw();

// ====================================================================
// Phase 5: Tutorial System
// ====================================================================
extern void InstallTutorialDtor(); extern void RemoveTutorialDtor();
extern void InstallTutorialSerialize(); extern void RemoveTutorialSerialize();
extern void InstallTutorialCleanup(); extern void RemoveTutorialCleanup();
extern void InstallTutorialCtor(); extern void RemoveTutorialCtor();
extern void InstallTutorialGoalDtor(); extern void RemoveTutorialGoalDtor();
extern void InstallTutorialGoalSerialize(); extern void RemoveTutorialGoalSerialize();
extern void InstallTutorialGoalButtonDtor(); extern void RemoveTutorialGoalButtonDtor();
extern void InstallTutorialGoalButtonSerialize(); extern void RemoveTutorialGoalButtonSerialize();
extern void InstallTutorialGoalButtonCleanup(); extern void RemoveTutorialGoalButtonCleanup();
extern void InstallTutorialGoalButtonSlot8(); extern void RemoveTutorialGoalButtonSlot8();
extern void InstallTutorialGoalButtonCtor(); extern void RemoveTutorialGoalButtonCtor();
extern void InstallTutorialGoalCountDtor(); extern void RemoveTutorialGoalCountDtor();
extern void InstallTutorialGoalCountSerialize(); extern void RemoveTutorialGoalCountSerialize();
extern void InstallTutorialGoalCountCleanup(); extern void RemoveTutorialGoalCountCleanup();
extern void InstallTutorialGoalCountSlot8(); extern void RemoveTutorialGoalCountSlot8();
extern void InstallTutorialGoalCountCtor(); extern void RemoveTutorialGoalCountCtor();
extern void InstallTutorialGoalGameSpeedDtor(); extern void RemoveTutorialGoalGameSpeedDtor();
extern void InstallTutorialGoalGameSpeedSerialize(); extern void RemoveTutorialGoalGameSpeedSerialize();
extern void InstallTutorialGoalGameSpeedCleanup(); extern void RemoveTutorialGoalGameSpeedCleanup();
extern void InstallTutorialGoalGameSpeedSlot8(); extern void RemoveTutorialGoalGameSpeedSlot8();
extern void InstallTutorialGoalGameSpeedCtor(); extern void RemoveTutorialGoalGameSpeedCtor();
extern void InstallTutorialImageDtor(); extern void RemoveTutorialImageDtor();
extern void InstallTutorialImageSerialize(); extern void RemoveTutorialImageSerialize();
extern void InstallTutorialImageCleanup(); extern void RemoveTutorialImageCleanup();
extern void InstallTutorialImageCtor(); extern void RemoveTutorialImageCtor();
extern void InstallTutorialImageSlot9(); extern void RemoveTutorialImageSlot9();
extern void InstallTutorialImageHotKeyDtor(); extern void RemoveTutorialImageHotKeyDtor();
extern void InstallTutorialImageHotKeySerialize(); extern void RemoveTutorialImageHotKeySerialize();
extern void InstallTutorialImageHotKeyCleanup(); extern void RemoveTutorialImageHotKeyCleanup();
extern void InstallTutorialImageHotKeyCtor(); extern void RemoveTutorialImageHotKeyCtor();
extern void InstallTutorialImageHotKeySlot9(); extern void RemoveTutorialImageHotKeySlot9();
extern void InstallTutorialImageInputKeyDtor(); extern void RemoveTutorialImageInputKeyDtor();
extern void InstallTutorialImageInputKeySerialize(); extern void RemoveTutorialImageInputKeySerialize();
extern void InstallTutorialImageInputKeyCleanup(); extern void RemoveTutorialImageInputKeyCleanup();
extern void InstallTutorialImageInputKeyCtor(); extern void RemoveTutorialImageInputKeyCtor();
extern void InstallTutorialImageInputKeySlot9(); extern void RemoveTutorialImageInputKeySlot9();
extern void InstallTutorialImageToolbarIconDtor(); extern void RemoveTutorialImageToolbarIconDtor();
extern void InstallTutorialImageToolbarIconSerialize(); extern void RemoveTutorialImageToolbarIconSerialize();
extern void InstallTutorialImageToolbarIconCleanup(); extern void RemoveTutorialImageToolbarIconCleanup();
extern void InstallTutorialImageToolbarIconCtor(); extern void RemoveTutorialImageToolbarIconCtor();
extern void InstallTutorialImageToolbarIconSlot9(); extern void RemoveTutorialImageToolbarIconSlot9();
extern void InstallTutorialInstanceDtor(); extern void RemoveTutorialInstanceDtor();
extern void InstallTutorialInstanceSerialize(); extern void RemoveTutorialInstanceSerialize();
extern void InstallTutorialSetupDtor(); extern void RemoveTutorialSetupDtor();
extern void InstallTutorialSetupSerialize(); extern void RemoveTutorialSetupSerialize();
extern void InstallTutorialSetupCameraDtor(); extern void RemoveTutorialSetupCameraDtor();
extern void InstallTutorialSetupCameraSerialize(); extern void RemoveTutorialSetupCameraSerialize();
extern void InstallTutorialSetupCameraCleanup(); extern void RemoveTutorialSetupCameraCleanup();
extern void InstallTutorialSetupCameraSlot8(); extern void RemoveTutorialSetupCameraSlot8();
extern void InstallTutorialSetupCameraCtor(); extern void RemoveTutorialSetupCameraCtor();
extern void InstallTutorialSetupGameSpeedDtor(); extern void RemoveTutorialSetupGameSpeedDtor();
extern void InstallTutorialSetupGameSpeedSerialize(); extern void RemoveTutorialSetupGameSpeedSerialize();
extern void InstallTutorialSetupGameSpeedCleanup(); extern void RemoveTutorialSetupGameSpeedCleanup();
extern void InstallTutorialSetupGameSpeedSlot8(); extern void RemoveTutorialSetupGameSpeedSlot8();
extern void InstallTutorialSetupGameSpeedCtor(); extern void RemoveTutorialSetupGameSpeedCtor();
extern void InstallTutorialSetupRestrictPlayerDtor(); extern void RemoveTutorialSetupRestrictPlayerDtor();
extern void InstallTutorialSetupRestrictPlayerSerialize(); extern void RemoveTutorialSetupRestrictPlayerSerialize();
extern void InstallTutorialSetupRestrictPlayerCleanup(); extern void RemoveTutorialSetupRestrictPlayerCleanup();
extern void InstallTutorialSetupRestrictPlayerSlot8(); extern void RemoveTutorialSetupRestrictPlayerSlot8();
extern void InstallTutorialSetupRestrictPlayerSlot10(); extern void RemoveTutorialSetupRestrictPlayerSlot10();
extern void InstallTutorialSetupRestrictPlayerCtor(); extern void RemoveTutorialSetupRestrictPlayerCtor();
extern void InstallTutorialStepDtor(); extern void RemoveTutorialStepDtor();
extern void InstallTutorialStepSerialize(); extern void RemoveTutorialStepSerialize();
extern void InstallTutorialStepCleanup(); extern void RemoveTutorialStepCleanup();
extern void InstallTutorialStepInit(); extern void RemoveTutorialStepInit();

// ====================================================================
// Phase 5: Achievement Goals
// ====================================================================
extern void InstallBuildingGoalDtor(); extern void RemoveBuildingGoalDtor();
extern void InstallBuildingGoalSlot5(); extern void RemoveBuildingGoalSlot5();
extern void InstallBuildingGoalSlot6(); extern void RemoveBuildingGoalSlot6();
extern void InstallBuildingGoalSlot8(); extern void RemoveBuildingGoalSlot8();
extern void InstallBuildingGoalSlot10(); extern void RemoveBuildingGoalSlot10();
extern void InstallPopulationGoalDtor(); extern void RemovePopulationGoalDtor();
extern void InstallPopulationGoalSlot5(); extern void RemovePopulationGoalSlot5();
extern void InstallPopulationGoalSlot6(); extern void RemovePopulationGoalSlot6();
extern void InstallPopulationGoalSlot8(); extern void RemovePopulationGoalSlot8();
extern void InstallPopulationGoalSlot10(); extern void RemovePopulationGoalSlot10();
extern void InstallFinishTutorialGoalDtor(); extern void RemoveFinishTutorialGoalDtor();
extern void InstallFinishTutorialGoalSlot5(); extern void RemoveFinishTutorialGoalSlot5();
extern void InstallFinishTutorialGoalSlot6(); extern void RemoveFinishTutorialGoalSlot6();
extern void InstallFinishTutorialGoalSlot10(); extern void RemoveFinishTutorialGoalSlot10();
extern void InstallFinishTutorialGoalSlot11(); extern void RemoveFinishTutorialGoalSlot11();
extern void InstallFinishTutorialInstanceDtor(); extern void RemoveFinishTutorialInstanceDtor();
extern void InstallFinishTutorialInstanceSlot3(); extern void RemoveFinishTutorialInstanceSlot3();
extern void InstallFinishTutorialInstanceSlot4(); extern void RemoveFinishTutorialInstanceSlot4();
extern void InstallFinishTutorialInstanceSlot5(); extern void RemoveFinishTutorialInstanceSlot5();
extern void InstallFinishTutorialInstanceSlot6(); extern void RemoveFinishTutorialInstanceSlot6();
extern void InstallStatGoalDtor(); extern void RemoveStatGoalDtor();
extern void InstallStatGoalSlot5(); extern void RemoveStatGoalSlot5();
extern void InstallStatGoalSlot6(); extern void RemoveStatGoalSlot6();
extern void InstallStatGoalSlot8(); extern void RemoveStatGoalSlot8();
extern void InstallStatGoalSlot10(); extern void RemoveStatGoalSlot10();
extern void InstallTimeGoalDtor(); extern void RemoveTimeGoalDtor();
extern void InstallTimeGoalSlot5(); extern void RemoveTimeGoalSlot5();
extern void InstallTimeGoalSlot6(); extern void RemoveTimeGoalSlot6();
extern void InstallTimeGoalSlot10(); extern void RemoveTimeGoalSlot10();
extern void InstallTimeGoalSlot11(); extern void RemoveTimeGoalSlot11();
extern void InstallTimeGoalInstanceDtor(); extern void RemoveTimeGoalInstanceDtor();
extern void InstallTimeGoalInstanceSlot3(); extern void RemoveTimeGoalInstanceSlot3();
extern void InstallTimeGoalInstanceSlot5(); extern void RemoveTimeGoalInstanceSlot5();
extern void InstallTimeGoalInstanceSlot6(); extern void RemoveTimeGoalInstanceSlot6();

// ====================================================================
// Phase 5: World Generation
// ====================================================================
extern void InstallMapGeneratorDtor(); extern void RemoveMapGeneratorDtor();
extern void InstallMapGeneratorSlot5(); extern void RemoveMapGeneratorSlot5();
extern void InstallMapGeneratorSlot6(); extern void RemoveMapGeneratorSlot6();
extern void InstallMapGeneratorSlot8(); extern void RemoveMapGeneratorSlot8();
extern void InstallTerrainGeneratorDtor(); extern void RemoveTerrainGeneratorDtor();
extern void InstallTerrainGeneratorSlot5(); extern void RemoveTerrainGeneratorSlot5();
extern void InstallTerrainGeneratorSlot6(); extern void RemoveTerrainGeneratorSlot6();
extern void InstallTerrainGeneratorSlot8(); extern void RemoveTerrainGeneratorSlot8();
extern void InstallClimateGeneratorDtor(); extern void RemoveClimateGeneratorDtor();
extern void InstallClimateGeneratorSlot5(); extern void RemoveClimateGeneratorSlot5();
extern void InstallClimateGeneratorSlot6(); extern void RemoveClimateGeneratorSlot6();
extern void InstallClimateGeneratorSlot8(); extern void RemoveClimateGeneratorSlot8();

// ====================================================================
// Phase 5: Miscellaneous Types
// ====================================================================
extern void InstallComponentEntityDtor(); extern void RemoveComponentEntityDtor();
extern void InstallComponentEntitySlot3(); extern void RemoveComponentEntitySlot3();
extern void InstallComponentEntitySlot5(); extern void RemoveComponentEntitySlot5();
extern void InstallComponentEntitySlot8(); extern void RemoveComponentEntitySlot8();
extern void InstallComponentEntitySlot9(); extern void RemoveComponentEntitySlot9();
extern void InstallProfessionDtor(); extern void RemoveProfessionDtor();
extern void InstallProfessionSlot5(); extern void RemoveProfessionSlot5();
extern void InstallProfessionSlot6(); extern void RemoveProfessionSlot6();
extern void InstallProfessionSlot8(); extern void RemoveProfessionSlot8();
extern void InstallProfessionListDtor(); extern void RemoveProfessionListDtor();
extern void InstallProfessionListSlot5(); extern void RemoveProfessionListSlot5();
extern void InstallProfessionListSlot6(); extern void RemoveProfessionListSlot6();
extern void InstallProfessionListSlot8(); extern void RemoveProfessionListSlot8();
extern void InstallScenarioDtor(); extern void RemoveScenarioDtor();
extern void InstallScenarioSlot3(); extern void RemoveScenarioSlot3();
extern void InstallScenarioSlot5(); extern void RemoveScenarioSlot5();
extern void InstallScenarioSlot6(); extern void RemoveScenarioSlot6();
extern void InstallScenarioSlot8(); extern void RemoveScenarioSlot8();
extern void InstallWeatherDtor(); extern void RemoveWeatherDtor();
extern void InstallWeatherSlot5(); extern void RemoveWeatherSlot5();
extern void InstallWeatherSlot6(); extern void RemoveWeatherSlot6();
extern void InstallWeatherSlot8(); extern void RemoveWeatherSlot8();
extern void InstallMenuSceneDtor(); extern void RemoveMenuSceneDtor();
extern void InstallMenuSceneSlot5(); extern void RemoveMenuSceneSlot5();
extern void InstallMenuSceneSlot6(); extern void RemoveMenuSceneSlot6();
extern void InstallMenuSceneSlot8(); extern void RemoveMenuSceneSlot8();

static ReimplEntry g_reimplementations[] = {
    // --- Data patches ---
    { "BuildNumber",           InstallBuildPatch,          RemoveBuildPatch,          false },

    // --- Launcher skip ---
    { "PlatformConfigure",     InstallPlatformConfigure,   RemovePlatformConfigure,   true  },

    // --- MainLoop reimplementations ---
    { "MainLoop::Frame",       InstallMainLoopFrame,       RemoveMainLoopFrame,       true  },
    { "MainLoop::Update",      InstallMainLoopUpdate,      RemoveMainLoopUpdate,      true  },
    { "MainLoop::Draw",        InstallMainLoopDraw,        RemoveMainLoopDraw,        true  },
    { "MainLoop::SetNextLoop", InstallMainLoopSetNextLoop, RemoveMainLoopSetNextLoop, true  },
    { "MainLoop::Run",         InstallMainLoopRun,         RemoveMainLoopRun,         true  },
    { "MainLoop::IsRunning",   InstallMainLoopIsRunningML, RemoveMainLoopIsRunningML, true  },
    { "MainLoop::MarkFrameUpdateTime", InstallMainLoopMarkFrameUpdateTime, RemoveMainLoopMarkFrameUpdateTime, true },
    { "MainLoop::AddPostUpdate",    InstallMainLoopAddPostUpdate,    RemoveMainLoopAddPostUpdate,    true  },
    { "MainLoop::AddPostDraw",      InstallMainLoopAddPostDraw,      RemoveMainLoopAddPostDraw,      true  },
    { "MainLoop::RemovePostUpdate", InstallMainLoopRemovePostUpdate, RemoveMainLoopRemovePostUpdate, true  },
    { "MainLoop::RemovePostDraw",   InstallMainLoopRemovePostDraw,   RemoveMainLoopRemovePostDraw,   true  },
    { "MainLoop::OnVideoError_s",   InstallMainLoopOnVideoErrorStatic,   RemoveMainLoopOnVideoErrorStatic,   true },
    { "MainLoop::OnVideoRestart_s", InstallMainLoopOnVideoRestartStatic, RemoveMainLoopOnVideoRestartStatic, true },
    { "MainLoop::CreateVideoCtx",   InstallMainLoopCreateVideoCtx,   RemoveMainLoopCreateVideoCtx,   true  },
    { "MainLoop::ReleaseVideoCtx",  InstallMainLoopReleaseVideoCtx,  RemoveMainLoopReleaseVideoCtx,  true  },
    { "MainLoop::Ctor",             InstallMainLoopCtor,             RemoveMainLoopCtor,             true  },
    { "MainLoop::Dtor",             InstallMainLoopDtor,             RemoveMainLoopDtor,             true  },

    // --- UpdateLoop reimplementations ---
    { "UpdateLoop::IsRunning", InstallUpdateLoopIsRunning, RemoveUpdateLoopIsRunning, true },
    { "UpdateLoop::Ctor",      InstallUpdateLoopCtor,      RemoveUpdateLoopCtor,      true },
    { "UpdateLoop::Dtor",      InstallUpdateLoopDtor,      RemoveUpdateLoopDtor,      true },

    // --- DialogLoaderLoopT reimplementations ---
    { "DialogLoaderLoopT::Startup",  InstallDialogLoaderLoopStartup,  RemoveDialogLoaderLoopStartup,  true  },
    { "DialogLoaderLoopT::Shutdown", InstallDialogLoaderLoopShutdown, RemoveDialogLoaderLoopShutdown, true  },
    { "DialogLoaderLoopT::Update",   InstallDialogLoaderLoopUpdate,   RemoveDialogLoaderLoopUpdate,   true  },
    { "DialogLoaderLoopT::Draw",     InstallDialogLoaderLoopDraw,     RemoveDialogLoaderLoopDraw,     true  },
    { "DialogLoaderLoopT::Load",     InstallDialogLoaderLoopLoad,     RemoveDialogLoaderLoopLoad,     true  },

    // --- DialogLoopT reimplementations ---
    { "DialogLoopT::Startup",  InstallDialogLoopStartup,  RemoveDialogLoopStartup,  true },
    { "DialogLoopT::Shutdown", InstallDialogLoopShutdown, RemoveDialogLoopShutdown, true },
    { "DialogLoopT::Update",   InstallDialogLoopUpdate,   RemoveDialogLoopUpdate,   true },
    { "DialogLoopT::Draw",     InstallDialogLoopDraw,     RemoveDialogLoopDraw,     true },
    { "DialogLoopT::Load",     InstallDialogLoopLoad,     RemoveDialogLoopLoad,     true },

    // --- LoaderLoop reimplementations ---
    { "LoaderLoop::Startup",      InstallLoaderLoopStartup,      RemoveLoaderLoopStartup,      true  },
    { "LoaderLoop::Shutdown",     InstallLoaderLoopShutdown,      RemoveLoaderLoopShutdown,      true  },
    { "LoaderLoop::Update",       InstallLoaderLoopUpdate,        RemoveLoaderLoopUpdate,        true  },
    { "LoaderLoop::Draw",         InstallLoaderLoopDraw,          RemoveLoaderLoopDraw,          true  },
    { "LoaderLoop::Exit",         InstallLoaderLoopExit,          RemoveLoaderLoopExit,          true  },
    { "LoaderLoop::IsFinished",   InstallLoaderLoopIsFinished,    RemoveLoaderLoopIsFinished,    true  },
    { "LoaderLoop::OnVideoError", InstallLoaderLoopOnVideoError,  RemoveLoaderLoopOnVideoError,  true  },

    // --- GameUpdateLoop reimplementations ---
    { "GameUpdateLoop::Destroy",  InstallGameUpdateLoopDestroy,  RemoveGameUpdateLoopDestroy,  true  },
    { "GameUpdateLoop::Startup",  InstallGameUpdateLoopStartup,  RemoveGameUpdateLoopStartup,  true  },
    { "GameUpdateLoop::Shutdown", InstallGameUpdateLoopShutdown, RemoveGameUpdateLoopShutdown, true  },
    { "GameUpdateLoop::Load",     InstallGameUpdateLoopLoad,     RemoveGameUpdateLoopLoad,     true  },
    // Vtable-patched (Bug Class 3 — MOV RAX,RSP prevents prologue hooking):
    { "GameUpdateLoop::Update",   InstallGameUpdateLoopUpdate,   RemoveGameUpdateLoopUpdate,   true  },
    { "GameUpdateLoop::Draw",     InstallGameUpdateLoopDraw,     RemoveGameUpdateLoopDraw,     true  },
    // NOT HOOKED: SaveRestore (0x1400959C0) — Bug Class 3, not yet reimplemented
    //   — Bug Class 3 (MOV RAX,RSP), need vtable patching instead.

    // --- Configuration reimplementations ---
    { "Configuration::CurrentBuildId",   InstallConfigCurrentBuildId,   RemoveConfigCurrentBuildId,   true  },
    { "Configuration::CurrentVersion",   InstallConfigCurrentVersion,   RemoveConfigCurrentVersion,   true  },
    { "Configuration::GetVersionString", InstallConfigGetVersionString, RemoveConfigGetVersionString, true  },
    { "Configuration::Ctor",             InstallConfigCtor,             RemoveConfigCtor,             true  },
    { "Configuration::Dtor",             InstallConfigDtor,             RemoveConfigDtor,             true  },
    { "Configuration::GetCommandLineString", InstallConfigGetCommandLineString, RemoveConfigGetCommandLineString, true },
    { "Configuration::SetCommandLine",   InstallConfigSetCommandLine,   RemoveConfigSetCommandLine,   true  },
    { "Configuration::GetPlatformName",  InstallConfigGetPlatformName,  RemoveConfigGetPlatformName,  true  },
    { "Configuration::GetLanguageName",  InstallConfigGetLanguageName,  RemoveConfigGetLanguageName,  true  },
    { "Configuration::GetBuildName",     InstallConfigGetBuildName,     RemoveConfigGetBuildName,     true  },

    // --- Modification reimplementations ---
    { "Modification::Find",       InstallModificationFind,       RemoveModificationFind,       true  },
    { "Modification::Enable",     InstallModificationEnable,     RemoveModificationEnable,     true  },
    { "Modification::DisableAll", InstallModificationDisableAll, RemoveModificationDisableAll, true  },
    { "Modification::Swap",       InstallModificationSwap,       RemoveModificationSwap,       true  },
    { "Modification::EnableSavingSettings", InstallModificationEnableSavingSettings, RemoveModificationEnableSavingSettings, true },
    { "Modification::SaveList",        InstallModificationSaveList,        RemoveModificationSaveList,        true },
    { "Modification::RefreshList",     InstallModificationRefreshList,     RemoveModificationRefreshList,     true },
    { "Modification::SetupFileSystem", InstallModificationSetupFileSystem, RemoveModificationSetupFileSystem, false }, // crashes — 709-instr function, likely SEH/unwind issue with trampoline

    // --- LivestockComponent reimplementations ---
    { "LivestockComponent::Update", InstallLivestockComponentUpdate, RemoveLivestockComponentUpdate, true },

    // --- StateMachineComponent reimplementations ---
    { "StateMachineComponent::ScalarDtor", InstallStateMachineComponentDtor, RemoveStateMachineComponentDtor, true },
    { "StateMachineComponent::Shutdown", InstallStateMachineComponentShutdown, RemoveStateMachineComponentShutdown, true },
    { "StateMachineComponent::Serialize", InstallStateMachineComponentSerialize, RemoveStateMachineComponentSerialize, true },
    { "StateMachineComponent::Draw", InstallStateMachineComponentDraw, RemoveStateMachineComponentDraw, true },
    { "StateMachineComponent::OnRemove", InstallStateMachineComponentOnRemove, RemoveStateMachineComponentOnRemove, true },
    { "StateMachineComponent::Update", InstallStateMachineComponentUpdate, RemoveStateMachineComponentUpdate, true },
    { "StateMachineComponent::OnUpdate2", InstallStateMachineComponentOnUpdate2, RemoveStateMachineComponentOnUpdate2, true },

    // --- HuntComponent reimplementations ---
    { "HuntComponent::Update", InstallHuntComponentUpdate, RemoveHuntComponentUpdate, true },

    // --- DiseaseComponent reimplementations ---
    { "DiseaseComponent::Update", InstallDiseaseComponentUpdate, RemoveDiseaseComponentUpdate, true }, // RE-ENABLED: relocating trampoline handles Jcc + relative JMP

    // --- AttendanceComponent reimplementations ---
    { "AttendanceComponent::Update", InstallAttendanceComponentUpdate, RemoveAttendanceComponentUpdate, true },

    // --- AmbientEnvironmentComponent reimplementations ---
    { "AmbientEnvironmentComponent::Update", InstallAmbientEnvironmentComponentUpdate, RemoveAmbientEnvironmentComponentUpdate, true },

    // --- CharacterComponent reimplementations ---
    { "CharacterComponent::Update", InstallCharacterComponentUpdate, RemoveCharacterComponentUpdate, true },

    // --- PopulationComponent reimplementations ---
    { "PopulationComponent::Update", InstallPopulationComponentUpdate, RemovePopulationComponentUpdate, true },

    // --- FruitComponent reimplementations ---
    { "FruitComponent::Update", InstallFruitComponentUpdate, RemoveFruitComponentUpdate, true },

    // --- SpawnComponent reimplementations ---
    { "SpawnComponent::Update", InstallSpawnComponentUpdate, RemoveSpawnComponentUpdate, true },

    // --- FireComponent reimplementations ---
    { "FireComponent::Update", InstallFireComponentUpdate, RemoveFireComponentUpdate, true },

    // --- GrowthComponent reimplementations ---
    { "GrowthComponent::Update", InstallGrowthComponentUpdate, RemoveGrowthComponentUpdate, true },

    // --- HerdComponent reimplementations ---
    { "HerdComponent::Update", InstallHerdComponentUpdate, RemoveHerdComponentUpdate, true },

    // --- AnimalComponent reimplementations ---
    { "AnimalComponent::Update", InstallAnimalComponentUpdate, RemoveAnimalComponentUpdate, true }, // RE-ENABLED: RSP correction in trampoline (Bug Class 3 fix)

    // --- CitizenAIComponent reimplementations ---
    { "CitizenAIComponent::Update", InstallCitizenAIComponentUpdate, RemoveCitizenAIComponentUpdate, true },

    // --- CitizenComponent reimplementations ---
    { "CitizenComponent::Update", InstallCitizenComponentUpdate, RemoveCitizenComponentUpdate, true },
    { "CitizenComponent::ShouldSwitch", InstallCitizenComponentShouldSwitch, RemoveCitizenComponentShouldSwitch, true },

    // ============================================================
    // Phase 2: Passive component lifecycle methods
    // ============================================================

    // --- AbsorbResourceComponent ---
    { "AbsorbResourceComponent::Shutdown", InstallAbsorbResourceComponentShutdown, RemoveAbsorbResourceComponentShutdown, true }, // RE-ENABLED: relocating trampoline handles relative CALL

    // --- AmbientEmitterComponent ---
    { "AmbientEmitterComponent::Shutdown", InstallAmbientEmitterComponentShutdown, RemoveAmbientEmitterComponentShutdown, true },
    { "AmbientEmitterComponent::Draw", InstallAmbientEmitterComponentDraw, RemoveAmbientEmitterComponentDraw, true },

    // --- AutoPickupComponent ---
    { "AutoPickupComponent::Shutdown", InstallAutoPickupComponentShutdown, RemoveAutoPickupComponentShutdown, true },
    { "AutoPickupComponent::Serialize", InstallAutoPickupComponentSerialize, RemoveAutoPickupComponentSerialize, true },

    // --- BoatComponent ---
    { "BoatComponent::Shutdown", InstallBoatComponentShutdown, RemoveBoatComponentShutdown, true },

    // --- BuildComponent ---
    { "BuildComponent::Shutdown", InstallBuildComponentShutdown, RemoveBuildComponentShutdown, true },
    { "BuildComponent::Draw", InstallBuildComponentDraw, RemoveBuildComponentDraw, true }, // RE-ENABLED: relocating trampoline handles RIP-relative
    { "BuildComponent::OnCreate", InstallBuildComponentOnCreate, RemoveBuildComponentOnCreate, true },

    // --- CemeteryComponent ---
    { "CemeteryComponent::Shutdown", InstallCemeteryComponentShutdown, RemoveCemeteryComponentShutdown, true },
    { "CemeteryComponent::Draw", InstallCemeteryComponentDraw, RemoveCemeteryComponentDraw, true },
    { "CemeteryComponent::OnCreate", InstallCemeteryComponentOnCreate, RemoveCemeteryComponentOnCreate, true },

    // --- ClearAreaComponent ---
    { "ClearAreaComponent::Shutdown", InstallClearAreaComponentShutdown, RemoveClearAreaComponentShutdown, true },

    // --- ClearResourceComponent ---
    { "ClearResourceComponent::Shutdown", InstallClearResourceComponentShutdown, RemoveClearResourceComponentShutdown, true }, // RE-ENABLED: relocating trampoline handles relative CALL

    // --- ConsumeProduceComponent ---
    { "ConsumeProduceComponent::Shutdown", InstallConsumeProduceComponentShutdown, RemoveConsumeProduceComponentShutdown, true },
    { "ConsumeProduceComponent::Serialize", InstallConsumeProduceComponentSerialize, RemoveConsumeProduceComponentSerialize, true },

    // --- CropFieldComponent ---
    { "CropFieldComponent::Shutdown", InstallCropFieldComponentShutdown, RemoveCropFieldComponentShutdown, true },

    // --- DecalComponent ---
    { "DecalComponent::Shutdown", InstallDecalComponentShutdown, RemoveDecalComponentShutdown, true },
    { "DecalComponent::Draw", InstallDecalComponentDraw, RemoveDecalComponentDraw, true }, // RE-ENABLED: RSP correction in trampoline (Bug Class 3 fix)
    { "DecalComponent::OnCreate", InstallDecalComponentOnCreate, RemoveDecalComponentOnCreate, true },
    { "DecalComponent::Serialize", InstallDecalComponentSerialize, RemoveDecalComponentSerialize, true },

    // --- DestroyComponent ---
    { "DestroyComponent::Shutdown", InstallDestroyComponentShutdown, RemoveDestroyComponentShutdown, true },
    { "DestroyComponent::Serialize", InstallDestroyComponentSerialize, RemoveDestroyComponentSerialize, true },

    // --- FishingComponent ---
    { "FishingComponent::Shutdown", InstallFishingComponentShutdown, RemoveFishingComponentShutdown, true },

    // --- FloorComponent ---
    { "FloorComponent::Shutdown", InstallFloorComponentShutdown, RemoveFloorComponentShutdown, true },
    { "FloorComponent::Draw", InstallFloorComponentDraw, RemoveFloorComponentDraw, true },
    { "FloorComponent::OnCreate", InstallFloorComponentOnCreate, RemoveFloorComponentOnCreate, true },

    // --- ForesterComponent ---
    { "ForesterComponent::Shutdown", InstallForesterComponentShutdown, RemoveForesterComponentShutdown, true },

    // --- HappinessComponent ---
    { "HappinessComponent::Shutdown", InstallHappinessComponentShutdown, RemoveHappinessComponentShutdown, true },
    { "HappinessComponent::OnCreate", InstallHappinessComponentOnCreate, RemoveHappinessComponentOnCreate, true },

    // --- HealerComponent ---
    { "HealerComponent::Shutdown", InstallHealerComponentShutdown, RemoveHealerComponentShutdown, true }, // RE-ENABLED: relocating trampoline handles relative CALL
    { "HealerComponent::OnCreate", InstallHealerComponentOnCreate, RemoveHealerComponentOnCreate, true },

    // --- HerbalistComponent ---
    { "HerbalistComponent::Shutdown", InstallHerbalistComponentShutdown, RemoveHerbalistComponentShutdown, true }, // RE-ENABLED: relocating trampoline handles relative CALL

    // --- HighlightComponent ---
    { "HighlightComponent::Shutdown", InstallHighlightComponentShutdown, RemoveHighlightComponentShutdown, true }, // RE-ENABLED: relocating trampoline handles relative CALL

    // --- ImmigrationComponent ---
    { "ImmigrationComponent::Shutdown", InstallImmigrationComponentShutdown, RemoveImmigrationComponentShutdown, true },

    // --- InteractComponent ---
    { "InteractComponent::Shutdown", InstallInteractComponentShutdown, RemoveInteractComponentShutdown, true },
    { "InteractComponent::Draw", InstallInteractComponentDraw, RemoveInteractComponentDraw, true },

    // --- MapComponent ---
    { "MapComponent::Shutdown", InstallMapComponentShutdown, RemoveMapComponentShutdown, true },
    { "MapComponent::Draw", InstallMapComponentDraw, RemoveMapComponentDraw, true },
    { "MapComponent::OnCreate", InstallMapComponentOnCreate, RemoveMapComponentOnCreate, true },

    // --- MarketComponent ---
    { "MarketComponent::Shutdown", InstallMarketComponentShutdown, RemoveMarketComponentShutdown, true }, // RE-ENABLED: relocating trampoline handles relative CALL

    // --- MarketLocationComponent ---
    { "MarketLocationComponent::Shutdown", InstallMarketLocationComponentShutdown, RemoveMarketLocationComponentShutdown, true }, // RE-ENABLED: relocating trampoline handles relative CALL
    { "MarketLocationComponent::OnCreate", InstallMarketLocationComponentOnCreate, RemoveMarketLocationComponentOnCreate, true },

    // --- MineComponent ---
    { "MineComponent::Shutdown", InstallMineComponentShutdown, RemoveMineComponentShutdown, true }, // RE-ENABLED: relocating trampoline handles relative CALL

    // --- ModelComponent ---
    { "ModelComponent::Shutdown", InstallModelComponentShutdown, RemoveModelComponentShutdown, true },
    { "ModelComponent::Draw", InstallModelComponentDraw, RemoveModelComponentDraw, true },
    { "ModelComponent::OnCreate", InstallModelComponentOnCreate, RemoveModelComponentOnCreate, true },
    { "ModelComponent::OnRemove", InstallModelComponentOnRemove, RemoveModelComponentOnRemove, true },
    { "ModelComponent::Serialize", InstallModelComponentSerialize, RemoveModelComponentSerialize, true },

    // --- MultiObjectComponent ---
    { "MultiObjectComponent::Shutdown", InstallMultiObjectComponentShutdown, RemoveMultiObjectComponentShutdown, true },
    { "MultiObjectComponent::OnCreate", InstallMultiObjectComponentOnCreate, RemoveMultiObjectComponentOnCreate, true },

    // --- NaturalResourceComponent ---
    { "NaturalResourceComponent::Shutdown", InstallNaturalResourceComponentShutdown, RemoveNaturalResourceComponentShutdown, true },
    { "NaturalResourceComponent::OnRemove", InstallNaturalResourceComponentOnRemove, RemoveNaturalResourceComponentOnRemove, true },

    // --- NomadComponent ---
    { "NomadComponent::Shutdown", InstallNomadComponentShutdown, RemoveNomadComponentShutdown, true },
    { "NomadComponent::OnCreate", InstallNomadComponentOnCreate, RemoveNomadComponentOnCreate, true }, // RE-ENABLED: relocating trampoline handles RIP-relative

    // --- OrchardComponent ---
    { "OrchardComponent::Shutdown", InstallOrchardComponentShutdown, RemoveOrchardComponentShutdown, true },

    // --- ParticleComponent ---
    { "ParticleComponent::Shutdown", InstallParticleComponentShutdown, RemoveParticleComponentShutdown, true },
    { "ParticleComponent::Draw", InstallParticleComponentDraw, RemoveParticleComponentDraw, true },
    { "ParticleComponent::OnCreate", InstallParticleComponentOnCreate, RemoveParticleComponentOnCreate, true },
    { "ParticleComponent::OnRemove", InstallParticleComponentOnRemove, RemoveParticleComponentOnRemove, true }, // RE-ENABLED: RSP correction in trampoline (Bug Class 3 fix)
    { "ParticleComponent::Serialize", InstallParticleComponentSerialize, RemoveParticleComponentSerialize, true }, // RE-ENABLED: relocating trampoline handles relative CALL
    { "ParticleComponent::OnUpdate2", InstallParticleComponentOnUpdate2, RemoveParticleComponentOnUpdate2, true },

    // --- PastureComponent ---
    { "PastureComponent::Shutdown", InstallPastureComponentShutdown, RemovePastureComponentShutdown, true },
    { "PastureComponent::Draw", InstallPastureComponentDraw, RemovePastureComponentDraw, true }, // RE-ENABLED: relocating trampoline handles relative CALL
    { "PastureComponent::OnCreate", InstallPastureComponentOnCreate, RemovePastureComponentOnCreate, true },

    // --- PickingComponent ---
    { "PickingComponent::Shutdown", InstallPickingComponentShutdown, RemovePickingComponentShutdown, true },
    { "PickingComponent::OnCreate", InstallPickingComponentOnCreate, RemovePickingComponentOnCreate, true },

    // --- QuarryComponent ---
    { "QuarryComponent::Shutdown", InstallQuarryComponentShutdown, RemoveQuarryComponentShutdown, true }, // RE-ENABLED: relocating trampoline handles relative CALL

    // --- RawMaterialComponent ---
    { "RawMaterialComponent::Shutdown", InstallRawMaterialComponentShutdown, RemoveRawMaterialComponentShutdown, true },
    { "RawMaterialComponent::Draw", InstallRawMaterialComponentDraw, RemoveRawMaterialComponentDraw, true },
    { "RawMaterialComponent::Serialize", InstallRawMaterialComponentSerialize, RemoveRawMaterialComponentSerialize, true },
    { "RawMaterialComponent::Update", InstallRawMaterialComponentUpdate, RemoveRawMaterialComponentUpdate, true },

    // --- ResidenceComponent ---
    { "ResidenceComponent::Shutdown", InstallResidenceComponentShutdown, RemoveResidenceComponentShutdown, true },
    { "ResidenceComponent::Serialize", InstallResidenceComponentSerialize, RemoveResidenceComponentSerialize, true },
    { "ResidenceComponent::OnCreate", InstallResidenceComponentOnCreate, RemoveResidenceComponentOnCreate, true }, // RE-ENABLED: relocating trampoline handles relative CALL

    // --- StatusIconComponent ---
    { "StatusIconComponent::Shutdown", InstallStatusIconComponentShutdown, RemoveStatusIconComponentShutdown, true },
    { "StatusIconComponent::Serialize", InstallStatusIconComponentSerialize, RemoveStatusIconComponentSerialize, true },
    { "StatusIconComponent::OnRemove", InstallStatusIconComponentOnRemove, RemoveStatusIconComponentOnRemove, true },
    { "StatusIconComponent::OnUpdate2", InstallStatusIconComponentOnUpdate2, RemoveStatusIconComponentOnUpdate2, true },

    // --- StorageComponent ---
    { "StorageComponent::QueryCount", InstallStorageComponentQueryCount, RemoveStorageComponentQueryCount, true },
    { "StorageComponent::Helper_c560", InstallStorageComponentHelper_c560, RemoveStorageComponentHelper_c560, true },
    { "StorageComponent::Shutdown", InstallStorageComponentShutdown, RemoveStorageComponentShutdown, true },
    { "StorageComponent::Draw", InstallStorageComponentDraw, RemoveStorageComponentDraw, true },

    // --- StorageDisplayComponent ---
    { "StorageDisplayComponent::Shutdown", InstallStorageDisplayComponentShutdown, RemoveStorageDisplayComponentShutdown, true }, // RE-ENABLED: relocating trampoline handles relative CALL
    { "StorageDisplayComponent::Draw", InstallStorageDisplayComponentDraw, RemoveStorageDisplayComponentDraw, true },
    { "StorageDisplayComponent::OnCreate", InstallStorageDisplayComponentOnCreate, RemoveStorageDisplayComponentOnCreate, true },
    { "StorageDisplayComponent::Update", InstallStorageDisplayComponentUpdate, RemoveStorageDisplayComponentUpdate, true },
    { "StorageDisplayComponent::OnUpdate2", InstallStorageDisplayComponentOnUpdate2, RemoveStorageDisplayComponentOnUpdate2, true },

    // --- StorageLocationComponent ---
    { "StorageLocationComponent::Shutdown", InstallStorageLocationComponentShutdown, RemoveStorageLocationComponentShutdown, true }, // RE-ENABLED: relocating trampoline handles relative CALL
    { "StorageLocationComponent::OnCreate", InstallStorageLocationComponentOnCreate, RemoveStorageLocationComponentOnCreate, true },

    // --- TerraformComponent ---
    { "TerraformComponent::Shutdown", InstallTerraformComponentShutdown, RemoveTerraformComponentShutdown, true },
    { "TerraformComponent::OnCreate", InstallTerraformComponentOnCreate, RemoveTerraformComponentOnCreate, true },

    // --- TombstoneComponent ---
    { "TombstoneComponent::Shutdown", InstallTombstoneComponentShutdown, RemoveTombstoneComponentShutdown, true },
    { "TombstoneComponent::Serialize", InstallTombstoneComponentSerialize, RemoveTombstoneComponentSerialize, true },
    { "TombstoneComponent::Update", InstallTombstoneComponentUpdate, RemoveTombstoneComponentUpdate, true },

    // --- TornadoComponent ---
    { "TornadoComponent::Shutdown", InstallTornadoComponentShutdown, RemoveTornadoComponentShutdown, true },
    { "TornadoComponent::Serialize", InstallTornadoComponentSerialize, RemoveTornadoComponentSerialize, true },
    { "TornadoComponent::OnRemove", InstallTornadoComponentOnRemove, RemoveTornadoComponentOnRemove, true },

    // --- TrackerComponent ---
    { "TrackerComponent::Shutdown", InstallTrackerComponentShutdown, RemoveTrackerComponentShutdown, true },

    // --- TradeComponent ---
    { "TradeComponent::Shutdown", InstallTradeComponentShutdown, RemoveTradeComponentShutdown, true },
    { "TradeComponent::Draw", InstallTradeComponentDraw, RemoveTradeComponentDraw, true },
    { "TradeComponent::OnCreate", InstallTradeComponentOnCreate, RemoveTradeComponentOnCreate, true },

    // --- UIComponent ---
    { "UIComponent::Shutdown", InstallUIComponentShutdown, RemoveUIComponentShutdown, true },
    { "UIComponent::Serialize", InstallUIComponentSerialize, RemoveUIComponentSerialize, true },

    // --- WellComponent ---
    { "WellComponent::Shutdown", InstallWellComponentShutdown, RemoveWellComponentShutdown, true }, // RE-ENABLED: relocating trampoline handles relative CALL
    { "WellComponent::OnCreate", InstallWellComponentOnCreate, RemoveWellComponentOnCreate, true },

    // --- WildAnimalComponent ---
    { "WildAnimalComponent::Shutdown", InstallWildAnimalComponentShutdown, RemoveWildAnimalComponentShutdown, true },
    { "WildAnimalComponent::OnCreate", InstallWildAnimalComponentOnCreate, RemoveWildAnimalComponentOnCreate, true },

    // --- WorkComponent ---
    { "WorkComponent::Shutdown", InstallWorkComponentShutdown, RemoveWorkComponentShutdown, false }, // DISABLED: causes tutorial load crash — reimpl serialization mismatch
    { "WorkComponent::Draw", InstallWorkComponentDraw, RemoveWorkComponentDraw, true }, // RE-ENABLED: relocating trampoline handles relative CALL
    { "WorkComponent::OnCreate", InstallWorkComponentOnCreate, RemoveWorkComponentOnCreate, true }, // RE-ENABLED: relocating trampoline handles relative CALL

    // --- WorkPlaceComponent ---
    { "WorkPlaceComponent::Shutdown", InstallWorkPlaceComponentShutdown, RemoveWorkPlaceComponentShutdown, true },
    { "WorkPlaceComponent::OnCreate", InstallWorkPlaceComponentOnCreate, RemoveWorkPlaceComponentOnCreate, true },

    // --- ZoneComponent ---
    { "ZoneComponent::Shutdown", InstallZoneComponentShutdown, RemoveZoneComponentShutdown, true },
    { "ZoneComponent::Draw", InstallZoneComponentDraw, RemoveZoneComponentDraw, true },
    { "ZoneComponent::OnCreate", InstallZoneComponentOnCreate, RemoveZoneComponentOnCreate, true },

    // ====================================================================
    // Phase 3: Action and Work class reimplementations
    // ====================================================================

    // --- AnimateNaturalResource ---
    { "AnimateNaturalResource::Serialize", InstallAnimateNaturalResourceSerialize, RemoveAnimateNaturalResourceSerialize, true },
    { "AnimateNaturalResource::Purecall", InstallAnimateNaturalResourcePurecall, RemoveAnimateNaturalResourcePurecall, true },
    { "AnimateNaturalResource::Execute", InstallAnimateNaturalResourceExecute, RemoveAnimateNaturalResourceExecute, true },
    { "AnimateNaturalResource::GetTarget", InstallAnimateNaturalResourceGetTarget, RemoveAnimateNaturalResourceGetTarget, true },

    // --- AutoPickupWork ---
    { "AutoPickupWork::Slot8", InstallAutoPickupWorkSlot8, RemoveAutoPickupWorkSlot8, true },
    { "AutoPickupWork::Ctor", InstallAutoPickupWorkCtor, RemoveAutoPickupWorkCtor, true },

    // --- BuildRoad ---
    { "BuildRoad::Ctor", InstallBuildRoadCtor, RemoveBuildRoadCtor, true },
    { "BuildRoad::ScalarDtor", InstallBuildRoadScalarDtor, RemoveBuildRoadScalarDtor, true },
    { "BuildRoad::Serialize", InstallBuildRoadSerialize, RemoveBuildRoadSerialize, true },
    { "BuildRoad::Execute", InstallBuildRoadExecute, RemoveBuildRoadExecute, true },
    { "BuildRoad::OnComplete", InstallBuildRoadOnComplete, RemoveBuildRoadOnComplete, true },
    { "BuildRoad::OnCancel", InstallBuildRoadOnCancel, RemoveBuildRoadOnCancel, true },
    { "BuildRoad::Cleanup", InstallBuildRoadCleanup, RemoveBuildRoadCleanup, true },
    { "BuildRoad::Validate", InstallBuildRoadValidate, RemoveBuildRoadValidate, true },

    // --- BuildStructure ---
    { "BuildStructure::Ctor", InstallBuildStructureCtor, RemoveBuildStructureCtor, true },
    { "BuildStructure::ScalarDtor", InstallBuildStructureScalarDtor, RemoveBuildStructureScalarDtor, true },
    { "BuildStructure::Serialize", InstallBuildStructureSerialize, RemoveBuildStructureSerialize, true },
    { "BuildStructure::Execute", InstallBuildStructureExecute, RemoveBuildStructureExecute, true },
    { "BuildStructure::OnComplete", InstallBuildStructureOnComplete, RemoveBuildStructureOnComplete, true },
    { "BuildStructure::OnCancel", InstallBuildStructureOnCancel, RemoveBuildStructureOnCancel, true },
    { "BuildStructure::Cleanup", InstallBuildStructureCleanup, RemoveBuildStructureCleanup, true },

    // --- ClearArea ---
    { "ClearArea::Ctor", InstallClearAreaCtor, RemoveClearAreaCtor, true },
    { "ClearArea::ScalarDtor", InstallClearAreaScalarDtor, RemoveClearAreaScalarDtor, true },
    { "ClearArea::Serialize", InstallClearAreaSerialize, RemoveClearAreaSerialize, true },
    { "ClearArea::Execute", InstallClearAreaExecute, RemoveClearAreaExecute, true },
    { "ClearArea::OnComplete", InstallClearAreaOnComplete, RemoveClearAreaOnComplete, true },
    { "ClearArea::Cleanup", InstallClearAreaCleanup, RemoveClearAreaCleanup, true },
    { "ClearArea::GetTarget", InstallClearAreaGetTarget, RemoveClearAreaGetTarget, true },

    // --- ClearStorage ---
    { "ClearStorage::Serialize", InstallClearStorageSerialize, RemoveClearStorageSerialize, true },
    { "ClearStorage::Slot4", InstallClearStorageSlot4, RemoveClearStorageSlot4, true },
    { "ClearStorage::Slot6", InstallClearStorageSlot6, RemoveClearStorageSlot6, true }, // RE-ENABLED: relocating trampoline handles relative CALL
    { "ClearStorage::Slot8", InstallClearStorageSlot8, RemoveClearStorageSlot8, true },
    { "ClearStorage::Slot9", InstallClearStorageSlot9, RemoveClearStorageSlot9, true }, // RE-ENABLED: RSP correction in trampoline (Bug Class 3 fix)
    { "ClearStorage::Slot10", InstallClearStorageSlot10, RemoveClearStorageSlot10, true }, // RE-ENABLED: relocating trampoline handles RIP-relative

    // --- CompleteRoad ---
    { "CompleteRoad::Serialize", InstallCompleteRoadSerialize, RemoveCompleteRoadSerialize, true },
    { "CompleteRoad::Purecall", InstallCompleteRoadPurecall, RemoveCompleteRoadPurecall, true },

    // --- CompleteTask ---
    { "CompleteTask::Serialize", InstallCompleteTaskSerialize, RemoveCompleteTaskSerialize, true },
    { "CompleteTask::Purecall", InstallCompleteTaskPurecall, RemoveCompleteTaskPurecall, true },
    { "CompleteTask::CheckEntity", InstallCompleteTaskCheckEntity, RemoveCompleteTaskCheckEntity, true },
    { "CompleteTask::GetTarget", InstallCompleteTaskGetTarget, RemoveCompleteTaskGetTarget, true },

    // --- ConsumeProduce ---
    { "ConsumeProduce::Ctor", InstallConsumeProduceCtor, RemoveConsumeProduceCtor, true },
    { "ConsumeProduce::ScalarDtor", InstallConsumeProduceScalarDtor, RemoveConsumeProduceScalarDtor, true },
    { "ConsumeProduce::Serialize", InstallConsumeProduceSerialize, RemoveConsumeProduceSerialize, true },

    // --- ConsumeRawMaterial ---
    { "ConsumeRawMaterial::Serialize", InstallConsumeRawMaterialSerialize, RemoveConsumeRawMaterialSerialize, true },
    { "ConsumeRawMaterial::AbstractImpl", InstallConsumeRawMaterialAbstractImpl, RemoveConsumeRawMaterialAbstractImpl, true },
    { "ConsumeRawMaterial::CheckEntity", InstallConsumeRawMaterialCheckEntity, RemoveConsumeRawMaterialCheckEntity, true },

    // --- CreateNaturalResource ---
    { "CreateNaturalResource::Serialize", InstallCreateNaturalResourceSerialize, RemoveCreateNaturalResourceSerialize, true },
    { "CreateNaturalResource::Purecall", InstallCreateNaturalResourcePurecall, RemoveCreateNaturalResourcePurecall, true },
    { "CreateNaturalResource::GetTarget", InstallCreateNaturalResourceGetTarget, RemoveCreateNaturalResourceGetTarget, true },

    // --- CreateRawMaterial ---
    { "CreateRawMaterial::Serialize", InstallCreateRawMaterialSerialize, RemoveCreateRawMaterialSerialize, true },
    { "CreateRawMaterial::AbstractImpl", InstallCreateRawMaterialAbstractImpl, RemoveCreateRawMaterialAbstractImpl, true },

    // --- DropOffRawMaterial ---
    { "DropOffRawMaterial::Serialize", InstallDropOffRawMaterialSerialize, RemoveDropOffRawMaterialSerialize, true },
    { "DropOffRawMaterial::AbstractImpl", InstallDropOffRawMaterialAbstractImpl, RemoveDropOffRawMaterialAbstractImpl, true },

    // --- FireWork ---
    { "FireWork::Serialize", InstallFireWorkSerialize, RemoveFireWorkSerialize, true },
    { "FireWork::Slot4", InstallFireWorkSlot4, RemoveFireWorkSlot4, true },
    { "FireWork::Slot6", InstallFireWorkSlot6, RemoveFireWorkSlot6, true }, // RE-ENABLED: relocating trampoline handles relative CALL
    { "FireWork::Slot8", InstallFireWorkSlot8, RemoveFireWorkSlot8, true },
    { "FireWork::Slot9", InstallFireWorkSlot9, RemoveFireWorkSlot9, true }, // RE-ENABLED: RSP correction in trampoline (Bug Class 3 fix)

    // --- FishingWork ---
    { "FishingWork::Serialize", InstallFishingWorkSerialize, RemoveFishingWorkSerialize, true },
    { "FishingWork::Slot4", InstallFishingWorkSlot4, RemoveFishingWorkSlot4, true },
    { "FishingWork::Slot6", InstallFishingWorkSlot6, RemoveFishingWorkSlot6, true },
    { "FishingWork::Slot7", InstallFishingWorkSlot7, RemoveFishingWorkSlot7, true },
    { "FishingWork::Slot8", InstallFishingWorkSlot8, RemoveFishingWorkSlot8, true },
    { "FishingWork::Slot9", InstallFishingWorkSlot9, RemoveFishingWorkSlot9, true },
    { "FishingWork::Slot10", InstallFishingWorkSlot10, RemoveFishingWorkSlot10, true },

    // --- ForesterWork ---
    { "ForesterWork::ScalarDtor", InstallForesterWorkScalarDtor, RemoveForesterWorkScalarDtor, true },
    { "ForesterWork::Serialize", InstallForesterWorkSerialize, RemoveForesterWorkSerialize, true },
    { "ForesterWork::Slot4", InstallForesterWorkSlot4, RemoveForesterWorkSlot4, true },
    { "ForesterWork::Slot8", InstallForesterWorkSlot8, RemoveForesterWorkSlot8, true },
    { "ForesterWork::Slot9", InstallForesterWorkSlot9, RemoveForesterWorkSlot9, true }, // RE-ENABLED: RSP correction in trampoline (Bug Class 3 fix)
    { "ForesterWork::Slot10", InstallForesterWorkSlot10, RemoveForesterWorkSlot10, true },

    // --- GatherRawMaterials ---
    { "GatherRawMaterials::Ctor", InstallGatherRawMaterialsCtor, RemoveGatherRawMaterialsCtor, true },
    { "GatherRawMaterials::ScalarDtor", InstallGatherRawMaterialsScalarDtor, RemoveGatherRawMaterialsScalarDtor, true },
    { "GatherRawMaterials::Serialize", InstallGatherRawMaterialsSerialize, RemoveGatherRawMaterialsSerialize, true },
    { "GatherRawMaterials::Execute", InstallGatherRawMaterialsExecute, RemoveGatherRawMaterialsExecute, true },
    { "GatherRawMaterials::OnComplete", InstallGatherRawMaterialsOnComplete, RemoveGatherRawMaterialsOnComplete, true },
    { "GatherRawMaterials::OnCancel", InstallGatherRawMaterialsOnCancel, RemoveGatherRawMaterialsOnCancel, true },
    { "GatherRawMaterials::Cleanup", InstallGatherRawMaterialsCleanup, RemoveGatherRawMaterialsCleanup, true },

    // --- HarvestArea ---
    { "HarvestArea::ScalarDtor", InstallHarvestAreaScalarDtor, RemoveHarvestAreaScalarDtor, true },
    { "HarvestArea::Serialize", InstallHarvestAreaSerialize, RemoveHarvestAreaSerialize, true },
    { "HarvestArea::Slot4", InstallHarvestAreaSlot4, RemoveHarvestAreaSlot4, true },
    { "HarvestArea::Slot6", InstallHarvestAreaSlot6, RemoveHarvestAreaSlot6, true },
    { "HarvestArea::Slot7", InstallHarvestAreaSlot7, RemoveHarvestAreaSlot7, true },
    { "HarvestArea::Slot8", InstallHarvestAreaSlot8, RemoveHarvestAreaSlot8, true },
    { "HarvestArea::Slot9", InstallHarvestAreaSlot9, RemoveHarvestAreaSlot9, true }, // RE-ENABLED: RSP correction in trampoline (Bug Class 3 fix)
    { "HarvestArea::Slot10", InstallHarvestAreaSlot10, RemoveHarvestAreaSlot10, true },

    // --- HarvestOrchard ---
    { "HarvestOrchard::ScalarDtor", InstallHarvestOrchardScalarDtor, RemoveHarvestOrchardScalarDtor, true },
    { "HarvestOrchard::Serialize", InstallHarvestOrchardSerialize, RemoveHarvestOrchardSerialize, true },
    { "HarvestOrchard::Slot4", InstallHarvestOrchardSlot4, RemoveHarvestOrchardSlot4, true },
    { "HarvestOrchard::Slot6", InstallHarvestOrchardSlot6, RemoveHarvestOrchardSlot6, true },
    { "HarvestOrchard::Slot7", InstallHarvestOrchardSlot7, RemoveHarvestOrchardSlot7, true },
    { "HarvestOrchard::Slot8", InstallHarvestOrchardSlot8, RemoveHarvestOrchardSlot8, true },
    { "HarvestOrchard::Slot9", InstallHarvestOrchardSlot9, RemoveHarvestOrchardSlot9, true }, // RE-ENABLED: RSP correction in trampoline (Bug Class 3 fix)
    { "HarvestOrchard::Slot10", InstallHarvestOrchardSlot10, RemoveHarvestOrchardSlot10, true },

    // --- HerbalistWork ---
    { "HerbalistWork::ScalarDtor", InstallHerbalistWorkScalarDtor, RemoveHerbalistWorkScalarDtor, true },
    { "HerbalistWork::Serialize", InstallHerbalistWorkSerialize, RemoveHerbalistWorkSerialize, false },
    { "HerbalistWork::Slot4", InstallHerbalistWorkSlot4, RemoveHerbalistWorkSlot4, true },
    { "HerbalistWork::Slot8", InstallHerbalistWorkSlot8, RemoveHerbalistWorkSlot8, true },
    { "HerbalistWork::Slot9", InstallHerbalistWorkSlot9, RemoveHerbalistWorkSlot9, true }, // RE-ENABLED: RSP correction in trampoline (Bug Class 3 fix)
    { "HerbalistWork::Slot10", InstallHerbalistWorkSlot10, RemoveHerbalistWorkSlot10, true },

    // --- HuntAnimal ---
    { "HuntAnimal::Serialize", InstallHuntAnimalSerialize, RemoveHuntAnimalSerialize, true },
    { "HuntAnimal::Enter", InstallHuntAnimalEnter, RemoveHuntAnimalEnter, true },

    // --- HuntWork ---
    { "HuntWork::ScalarDtor", InstallHuntWorkScalarDtor, RemoveHuntWorkScalarDtor, true },
    { "HuntWork::Serialize", InstallHuntWorkSerialize, RemoveHuntWorkSerialize, true },
    { "HuntWork::Slot4", InstallHuntWorkSlot4, RemoveHuntWorkSlot4, true },
    { "HuntWork::Slot8", InstallHuntWorkSlot8, RemoveHuntWorkSlot8, true },
    { "HuntWork::Slot9", InstallHuntWorkSlot9, RemoveHuntWorkSlot9, true }, // RE-ENABLED: RSP correction in trampoline (Bug Class 3 fix)
    { "HuntWork::Slot10", InstallHuntWorkSlot10, RemoveHuntWorkSlot10, true },

    // --- LocationWork ---
    { "LocationWork::Serialize", InstallLocationWorkSerialize, RemoveLocationWorkSerialize, false },
    { "LocationWork::Slot4", InstallLocationWorkSlot4, RemoveLocationWorkSlot4, true },
    { "LocationWork::Slot6", InstallLocationWorkSlot6, RemoveLocationWorkSlot6, true },
    { "LocationWork::Slot7", InstallLocationWorkSlot7, RemoveLocationWorkSlot7, true },
    { "LocationWork::Slot8", InstallLocationWorkSlot8, RemoveLocationWorkSlot8, true },
    { "LocationWork::Slot9", InstallLocationWorkSlot9, RemoveLocationWorkSlot9, true },
    { "LocationWork::Slot10", InstallLocationWorkSlot10, RemoveLocationWorkSlot10, true },

    // --- MarketWork ---
    { "MarketWork::Serialize", InstallMarketWorkSerialize, RemoveMarketWorkSerialize, false },
    { "MarketWork::Slot4", InstallMarketWorkSlot4, RemoveMarketWorkSlot4, true }, // RE-ENABLED: RSP correction in trampoline (Bug Class 3 fix)
    { "MarketWork::Slot6", InstallMarketWorkSlot6, RemoveMarketWorkSlot6, true }, // RE-ENABLED: relocating trampoline handles relative CALL
    { "MarketWork::Slot8", InstallMarketWorkSlot8, RemoveMarketWorkSlot8, true }, // RE-ENABLED: relocating trampoline handles short Jcc
    { "MarketWork::Slot9", InstallMarketWorkSlot9, RemoveMarketWorkSlot9, true }, // RE-ENABLED: RSP correction in trampoline (Bug Class 3 fix)
    { "MarketWork::Slot10", InstallMarketWorkSlot10, RemoveMarketWorkSlot10, true },

    // --- MoveTo ---
    { "MoveTo::Serialize", InstallMoveToSerialize, RemoveMoveToSerialize, true },
    { "MoveTo::AbstractImpl", InstallMoveToAbstractImpl, RemoveMoveToAbstractImpl, true },
    { "MoveTo::Execute", InstallMoveToExecute, RemoveMoveToExecute, true }, // RE-ENABLED: RSP correction in trampoline (Bug Class 3 fix)
    { "MoveTo::Enter", InstallMoveToEnter, RemoveMoveToEnter, true },
    { "MoveTo::Exit", InstallMoveToExit, RemoveMoveToExit, true },
    { "MoveTo::CheckEntity", InstallMoveToCheckEntity, RemoveMoveToCheckEntity, true },

    // --- PastureWork ---
    { "PastureWork::ScalarDtor", InstallPastureWorkScalarDtor, RemovePastureWorkScalarDtor, true },
    { "PastureWork::Serialize", InstallPastureWorkSerialize, RemovePastureWorkSerialize, false },
    { "PastureWork::Slot4", InstallPastureWorkSlot4, RemovePastureWorkSlot4, true },
    { "PastureWork::Slot7", InstallPastureWorkSlot7, RemovePastureWorkSlot7, true },
    { "PastureWork::Slot8", InstallPastureWorkSlot8, RemovePastureWorkSlot8, true },
    { "PastureWork::Slot9", InstallPastureWorkSlot9, RemovePastureWorkSlot9, true }, // RE-ENABLED: RSP correction in trampoline (Bug Class 3 fix)
    { "PastureWork::Slot10", InstallPastureWorkSlot10, RemovePastureWorkSlot10, true },

    // --- PerformWork ---
    { "PerformWork::Serialize", InstallPerformWorkSerialize, RemovePerformWorkSerialize, false },
    { "PerformWork::AbstractImpl", InstallPerformWorkAbstractImpl, RemovePerformWorkAbstractImpl, true },
    { "PerformWork::Execute", InstallPerformWorkExecute, RemovePerformWorkExecute, true },
    { "PerformWork::Exit", InstallPerformWorkExit, RemovePerformWorkExit, true },
    { "PerformWork::CheckEntity", InstallPerformWorkCheckEntity, RemovePerformWorkCheckEntity, true },

    // --- Pickup ---
    { "Pickup::Serialize", InstallPickupSerialize, RemovePickupSerialize, false },
    { "Pickup::Slot4", InstallPickupSlot4, RemovePickupSlot4, true }, // RE-ENABLED: RSP correction in trampoline (Bug Class 3 fix)
    { "Pickup::Execute", InstallPickupExecute, RemovePickupExecute, true },
    { "Pickup::Slot6", InstallPickupSlot6, RemovePickupSlot6, true },
    { "Pickup::Slot7", InstallPickupSlot7, RemovePickupSlot7, true },
    { "Pickup::Slot8", InstallPickupSlot8, RemovePickupSlot8, true }, // RE-ENABLED: relocating trampoline handles short Jcc
    { "Pickup::Slot9", InstallPickupSlot9, RemovePickupSlot9, true },
    { "Pickup::Slot10", InstallPickupSlot10, RemovePickupSlot10, true }, // RE-ENABLED: relocating trampoline handles RIP-relative

    // --- PickupRawMaterial ---
    { "PickupRawMaterial::Serialize", InstallPickupRawMaterialSerialize, RemovePickupRawMaterialSerialize, true },
    { "PickupRawMaterial::Purecall", InstallPickupRawMaterialPurecall, RemovePickupRawMaterialPurecall, true },
    { "PickupRawMaterial::GetTarget", InstallPickupRawMaterialGetTarget, RemovePickupRawMaterialGetTarget, true },

    // --- PickupStoredRawMaterial ---
    { "PickupStoredRawMaterial::Serialize", InstallPickupStoredRawMaterialSerialize, RemovePickupStoredRawMaterialSerialize, true },
    { "PickupStoredRawMaterial::AbstractImpl", InstallPickupStoredRawMaterialAbstractImpl, RemovePickupStoredRawMaterialAbstractImpl, true },
    { "PickupStoredRawMaterial::CheckEntity", InstallPickupStoredRawMaterialCheckEntity, RemovePickupStoredRawMaterialCheckEntity, true },

    // --- PlantArea ---
    { "PlantArea::Ctor", InstallPlantAreaCtor, RemovePlantAreaCtor, true },
    { "PlantArea::ScalarDtor", InstallPlantAreaScalarDtor, RemovePlantAreaScalarDtor, true },
    { "PlantArea::Serialize", InstallPlantAreaSerialize, RemovePlantAreaSerialize, false },

    // --- PlantOrchard ---
    { "PlantOrchard::Ctor", InstallPlantOrchardCtor, RemovePlantOrchardCtor, true },
    { "PlantOrchard::ScalarDtor", InstallPlantOrchardScalarDtor, RemovePlantOrchardScalarDtor, true },
    { "PlantOrchard::Serialize", InstallPlantOrchardSerialize, RemovePlantOrchardSerialize, false },

    // --- PlayAnimation ---
    { "PlayAnimation::Serialize", InstallPlayAnimationSerialize, RemovePlayAnimationSerialize, true },
    { "PlayAnimation::Purecall", InstallPlayAnimationPurecall, RemovePlayAnimationPurecall, true },
    { "PlayAnimation::Execute", InstallPlayAnimationExecute, RemovePlayAnimationExecute, true },
    { "PlayAnimation::GetTarget", InstallPlayAnimationGetTarget, RemovePlayAnimationGetTarget, true },

    // --- RemoveAnimal ---
    { "RemoveAnimal::Serialize", InstallRemoveAnimalSerialize, RemoveRemoveAnimalSerialize, true },
    { "RemoveAnimal::Purecall", InstallRemoveAnimalPurecall, RemoveRemoveAnimalPurecall, true }, // RE-ENABLED: relocating trampoline handles RIP-relative
    { "RemoveAnimal::CheckEntity", InstallRemoveAnimalCheckEntity, RemoveRemoveAnimalCheckEntity, true }, // RE-ENABLED: relocating trampoline handles RIP-relative CALL
    { "RemoveAnimal::GetTarget", InstallRemoveAnimalGetTarget, RemoveRemoveAnimalGetTarget, true },

    // --- RemoveNaturalResource ---
    { "RemoveNaturalResource::Serialize", InstallRemoveNaturalResourceSerialize, RemoveRemoveNaturalResourceSerialize, true },
    { "RemoveNaturalResource::Purecall", InstallRemoveNaturalResourcePurecall, RemoveRemoveNaturalResourcePurecall, true },
    { "RemoveNaturalResource::CheckEntity", InstallRemoveNaturalResourceCheckEntity, RemoveRemoveNaturalResourceCheckEntity, true },
    { "RemoveNaturalResource::GetTarget", InstallRemoveNaturalResourceGetTarget, RemoveRemoveNaturalResourceGetTarget, true },

    // --- SendComponentMessage ---
    { "SendComponentMessage::Serialize", InstallSendComponentMessageSerialize, RemoveSendComponentMessageSerialize, true },
    { "SendComponentMessage::AbstractImpl", InstallSendComponentMessageAbstractImpl, RemoveSendComponentMessageAbstractImpl, true },
    { "SendComponentMessage::GetTarget", InstallSendComponentMessageGetTarget, RemoveSendComponentMessageGetTarget, true },

    // --- ShowTool ---
    { "ShowTool::Serialize", InstallShowToolSerialize, RemoveShowToolSerialize, true },
    { "ShowTool::Purecall", InstallShowToolPurecall, RemoveShowToolPurecall, true },
    { "ShowTool::CheckEntity", InstallShowToolCheckEntity, RemoveShowToolCheckEntity, true },
    { "ShowTool::GetTarget", InstallShowToolGetTarget, RemoveShowToolGetTarget, true },

    // --- TendArea ---
    { "TendArea::Serialize", InstallTendAreaSerialize, RemoveTendAreaSerialize, false },
    { "TendArea::Slot4", InstallTendAreaSlot4, RemoveTendAreaSlot4, true },
    { "TendArea::Slot6", InstallTendAreaSlot6, RemoveTendAreaSlot6, true }, // RE-ENABLED: relocating trampoline handles relative CALL
    { "TendArea::Slot8", InstallTendAreaSlot8, RemoveTendAreaSlot8, true }, // RE-ENABLED: relocating trampoline handles short Jcc
    { "TendArea::Slot9", InstallTendAreaSlot9, RemoveTendAreaSlot9, true }, // RE-ENABLED: RSP correction in trampoline (Bug Class 3 fix)
    { "TendArea::Slot10", InstallTendAreaSlot10, RemoveTendAreaSlot10, true },

    // --- TradeWork ---
    { "TradeWork::Serialize", InstallTradeWorkSerialize, RemoveTradeWorkSerialize, false },
    { "TradeWork::Slot4", InstallTradeWorkSlot4, RemoveTradeWorkSlot4, true }, // RE-ENABLED: RSP correction in trampoline (Bug Class 3 fix)
    { "TradeWork::Slot6", InstallTradeWorkSlot6, RemoveTradeWorkSlot6, true }, // RE-ENABLED: relocating trampoline handles relative CALL
    { "TradeWork::Slot8", InstallTradeWorkSlot8, RemoveTradeWorkSlot8, true }, // RE-ENABLED: relocating trampoline handles short Jcc
    { "TradeWork::Slot9", InstallTradeWorkSlot9, RemoveTradeWorkSlot9, true }, // RE-ENABLED: RSP correction in trampoline (Bug Class 3 fix)
    { "TradeWork::Slot10", InstallTradeWorkSlot10, RemoveTradeWorkSlot10, true },

    // ============================================================
    // Phase 4: UI Panel Controllers (A-E)
    // ============================================================

    // --- AttendanceUI ---
    { "AttendanceUI::Dtor", InstallAttendanceUIDtor, RemoveAttendanceUIDtor, true },
    { "AttendanceUI::Slot4", InstallAttendanceUISlot4, RemoveAttendanceUISlot4, true }, // RE-ENABLED: reimpl doesn't use trampoline
    { "AttendanceUI::Slot7", InstallAttendanceUISlot7, RemoveAttendanceUISlot7, true },

    // --- BuildRequirementUI ---
    { "BuildRequirementUI::Dtor", InstallBuildRequirementUIDtor, RemoveBuildRequirementUIDtor, true },
    { "BuildRequirementUI::Slot4", InstallBuildRequirementUISlot4, RemoveBuildRequirementUISlot4, true },

    // --- BuildUI ---
    { "BuildUI::Dtor", InstallBuildUIDtor, RemoveBuildUIDtor, true },
    { "BuildUI::Slot4", InstallBuildUISlot4, RemoveBuildUISlot4, true },
    { "BuildUI::Slot7", InstallBuildUISlot7, RemoveBuildUISlot7, true },
    { "BuildUI::Slot9", InstallBuildUISlot9, RemoveBuildUISlot9, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // --- CemeteryUI ---
    { "CemeteryUI::Dtor", InstallCemeteryUIDtor, RemoveCemeteryUIDtor, true },
    { "CemeteryUI::Slot4", InstallCemeteryUISlot4, RemoveCemeteryUISlot4, true }, // RE-ENABLED: reimpl doesn't use trampoline
    { "CemeteryUI::Slot7", InstallCemeteryUISlot7, RemoveCemeteryUISlot7, true },

    // --- CitizenUI ---
    { "CitizenUI::Dtor", InstallCitizenUIDtor, RemoveCitizenUIDtor, true },
    { "CitizenUI::Slot4", InstallCitizenUISlot4, RemoveCitizenUISlot4, true },
    { "CitizenUI::Slot6", InstallCitizenUISlot6, RemoveCitizenUISlot6, true },
    { "CitizenUI::Slot7", InstallCitizenUISlot7, RemoveCitizenUISlot7, true }, // RE-ENABLED: null guards added
    { "CitizenUI::Slot9", InstallCitizenUISlot9, RemoveCitizenUISlot9, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // --- ConsumeProduceUI ---
    { "ConsumeProduceUI::Dtor", InstallConsumeProduceUIDtor, RemoveConsumeProduceUIDtor, true },
    { "ConsumeProduceUI::Slot4", InstallConsumeProduceUISlot4, RemoveConsumeProduceUISlot4, true },
    { "ConsumeProduceUI::Slot7", InstallConsumeProduceUISlot7, RemoveConsumeProduceUISlot7, true },
    { "ConsumeProduceUI::Slot9", InstallConsumeProduceUISlot9, RemoveConsumeProduceUISlot9, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // --- ControlUI ---
    { "ControlUI::Dtor", InstallControlUIDtor, RemoveControlUIDtor, true },
    { "ControlUI::Slot4", InstallControlUISlot4, RemoveControlUISlot4, true },
    { "ControlUI::Slot7", InstallControlUISlot7, RemoveControlUISlot7, true },
    { "ControlUI::Slot9", InstallControlUISlot9, RemoveControlUISlot9, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // --- CropUI ---
    { "CropUI::Dtor", InstallCropUIDtor, RemoveCropUIDtor, true },
    { "CropUI::Slot4", InstallCropUISlot4, RemoveCropUISlot4, true },
    { "CropUI::Slot7", InstallCropUISlot7, RemoveCropUISlot7, true },
    { "CropUI::Slot8", InstallCropUISlot8, RemoveCropUISlot8, true },
    { "CropUI::Slot9", InstallCropUISlot9, RemoveCropUISlot9, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // --- DebugUI ---
    { "DebugUI::Dtor", InstallDebugUIDtor, RemoveDebugUIDtor, true },
    { "DebugUI::Slot4", InstallDebugUISlot4, RemoveDebugUISlot4, true },
    { "DebugUI::Slot7", InstallDebugUISlot7, RemoveDebugUISlot7, true },
    { "DebugUI::Slot8", InstallDebugUISlot8, RemoveDebugUISlot8, true }, // RE-ENABLED: reimpl doesn't use trampoline
    { "DebugUI::Slot9", InstallDebugUISlot9, RemoveDebugUISlot9, true },

    // --- DestroyUI ---
    { "DestroyUI::Dtor", InstallDestroyUIDtor, RemoveDestroyUIDtor, true },
    { "DestroyUI::Slot4", InstallDestroyUISlot4, RemoveDestroyUISlot4, true },
    { "DestroyUI::Slot7", InstallDestroyUISlot7, RemoveDestroyUISlot7, true }, // RE-ENABLED: null guards added
    { "DestroyUI::Slot9", InstallDestroyUISlot9, RemoveDestroyUISlot9, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // --- EntityUI ---
    { "EntityUI::Dtor", InstallEntityUIDtor, RemoveEntityUIDtor, true },
    { "EntityUI::Slot4", InstallEntityUISlot4, RemoveEntityUISlot4, true },
    { "EntityUI::Slot6", InstallEntityUISlot6, RemoveEntityUISlot6, true },
    { "EntityUI::Slot7", InstallEntityUISlot7, RemoveEntityUISlot7, true },
    { "EntityUI::Slot8", InstallEntityUISlot8, RemoveEntityUISlot8, true },
    { "EntityUI::Slot9", InstallEntityUISlot9, RemoveEntityUISlot9, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // --- EventLogUI ---
    { "EventLogUI::Dtor", InstallEventLogUIDtor, RemoveEventLogUIDtor, true },
    { "EventLogUI::Slot5", InstallEventLogUISlot5, RemoveEventLogUISlot5, true },
    { "EventLogUI::Slot7", InstallEventLogUISlot7, RemoveEventLogUISlot7, true },
    { "EventLogUI::Slot9", InstallEventLogUISlot9, RemoveEventLogUISlot9, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // --- HelpUI ---
    { "HelpUI::Dtor", InstallHelpUIDtor, RemoveHelpUIDtor, true },
    { "HelpUI::Slot4", InstallHelpUISlot4, RemoveHelpUISlot4, true },
    { "HelpUI::Slot7", InstallHelpUISlot7, RemoveHelpUISlot7, true },
    { "HelpUI::Slot9", InstallHelpUISlot9, RemoveHelpUISlot9, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // ============================================================
    // Phase 4: UI Panel Controllers (H-R)
    // ============================================================

    // --- HotKeyToolTipUI ---
    { "HotKeyToolTipUI::Dtor", InstallHotKeyToolTipUIDtor, RemoveHotKeyToolTipUIDtor, true },
    { "HotKeyToolTipUI::Slot4", InstallHotKeyToolTipUISlot4, RemoveHotKeyToolTipUISlot4, true },

    // --- ImmigrationUI ---
    { "ImmigrationUI::Dtor", InstallImmigrationUIDtor, RemoveImmigrationUIDtor, true },
    { "ImmigrationUI::Slot4", InstallImmigrationUISlot4, RemoveImmigrationUISlot4, true },
    { "ImmigrationUI::Slot7", InstallImmigrationUISlot7, RemoveImmigrationUISlot7, true },
    { "ImmigrationUI::Slot9", InstallImmigrationUISlot9, RemoveImmigrationUISlot9, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // --- MapUI ---
    { "MapUI::Dtor", InstallMapUIDtor, RemoveMapUIDtor, true },
    { "MapUI::Slot4", InstallMapUISlot4, RemoveMapUISlot4, true }, // RE-ENABLED: reimpl doesn't use trampoline
    { "MapUI::Slot9", InstallMapUISlot9, RemoveMapUISlot9, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // --- ModBrowseUI ---
    { "ModBrowseUI::Dtor", InstallModBrowseUIDtor, RemoveModBrowseUIDtor, true },

    // --- NoiseUI ---
    { "NoiseUI::Dtor", InstallNoiseUIDtor, RemoveNoiseUIDtor, true },
    { "NoiseUI::Slot4", InstallNoiseUISlot4, RemoveNoiseUISlot4, true },
    { "NoiseUI::Slot9", InstallNoiseUISlot9, RemoveNoiseUISlot9, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // --- OrchardUI ---
    { "OrchardUI::Dtor", InstallOrchardUIDtor, RemoveOrchardUIDtor, true },
    { "OrchardUI::Slot4", InstallOrchardUISlot4, RemoveOrchardUISlot4, true },
    { "OrchardUI::Slot7", InstallOrchardUISlot7, RemoveOrchardUISlot7, true },
    { "OrchardUI::Slot8", InstallOrchardUISlot8, RemoveOrchardUISlot8, true },
    { "OrchardUI::Slot9", InstallOrchardUISlot9, RemoveOrchardUISlot9, true }, // RE-ENABLED: reimpl doesn't use trampoline
    { "OrchardUI::Slot11", InstallOrchardUISlot11, RemoveOrchardUISlot11, true },
    { "OrchardUI::Slot12", InstallOrchardUISlot12, RemoveOrchardUISlot12, true },

    // --- PageToggleUI ---
    { "PageToggleUI::Dtor", InstallPageToggleUIDtor, RemovePageToggleUIDtor, true },
    { "PageToggleUI::Slot4", InstallPageToggleUISlot4, RemovePageToggleUISlot4, true },
    { "PageToggleUI::Slot7", InstallPageToggleUISlot7, RemovePageToggleUISlot7, true },
    { "PageToggleUI::Slot9", InstallPageToggleUISlot9, RemovePageToggleUISlot9, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // --- PastureUI ---
    { "PastureUI::Dtor", InstallPastureUIDtor, RemovePastureUIDtor, true },
    { "PastureUI::Slot4", InstallPastureUISlot4, RemovePastureUISlot4, true },
    { "PastureUI::Slot7", InstallPastureUISlot7, RemovePastureUISlot7, true },
    { "PastureUI::Slot8", InstallPastureUISlot8, RemovePastureUISlot8, true },
    { "PastureUI::Slot9", InstallPastureUISlot9, RemovePastureUISlot9, true }, // RE-ENABLED: reimpl doesn't use trampoline
    { "PastureUI::Slot11", InstallPastureUISlot11, RemovePastureUISlot11, true },
    { "PastureUI::Slot12", InstallPastureUISlot12, RemovePastureUISlot12, true },

    // --- ProductionUI ---
    { "ProductionUI::Dtor", InstallProductionUIDtor, RemoveProductionUIDtor, true },
    { "ProductionUI::Slot4", InstallProductionUISlot4, RemoveProductionUISlot4, true },
    { "ProductionUI::Slot7", InstallProductionUISlot7, RemoveProductionUISlot7, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // --- ProfessionUI ---
    { "ProfessionUI::Dtor", InstallProfessionUIDtor, RemoveProfessionUIDtor, true },
    { "ProfessionUI::Slot5", InstallProfessionUISlot5, RemoveProfessionUISlot5, true },
    { "ProfessionUI::Slot8", InstallProfessionUISlot8, RemoveProfessionUISlot8, true },
    { "ProfessionUI::Slot9", InstallProfessionUISlot9, RemoveProfessionUISlot9, true },

    // --- RadiusUI ---
    { "RadiusUI::Dtor", InstallRadiusUIDtor, RemoveRadiusUIDtor, true },
    { "RadiusUI::Slot5", InstallRadiusUISlot5, RemoveRadiusUISlot5, true },
    { "RadiusUI::Slot6", InstallRadiusUISlot6, RemoveRadiusUISlot6, true },

    // --- ResidenceUI ---
    { "ResidenceUI::Dtor", InstallResidenceUIDtor, RemoveResidenceUIDtor, true },
    { "ResidenceUI::Slot4", InstallResidenceUISlot4, RemoveResidenceUISlot4, true },
    { "ResidenceUI::Slot7", InstallResidenceUISlot7, RemoveResidenceUISlot7, true }, // RE-ENABLED: null guards added, reimpl (no trampoline)
    { "ResidenceUI::Slot9", InstallResidenceUISlot9, RemoveResidenceUISlot9, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // --- ResourceLimitUI ---
    { "ResourceLimitUI::Dtor", InstallResourceLimitUIDtor, RemoveResourceLimitUIDtor, true },
    { "ResourceLimitUI::Slot4", InstallResourceLimitUISlot4, RemoveResourceLimitUISlot4, true },
    { "ResourceLimitUI::Slot7", InstallResourceLimitUISlot7, RemoveResourceLimitUISlot7, true },
    { "ResourceLimitUI::Slot9", InstallResourceLimitUISlot9, RemoveResourceLimitUISlot9, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // ============================================================
    // Phase 4: UI Panel Controllers (S-W)
    // ============================================================

    // --- SpriteCaptureUI ---
    { "SpriteCaptureUI::ScalarDtor", InstallSpriteCaptureUIScalarDtor, RemoveSpriteCaptureUIScalarDtor, true },
    { "SpriteCaptureUI::Serialize", InstallSpriteCaptureUISerialize, RemoveSpriteCaptureUISerialize, true },
    { "SpriteCaptureUI::OnCreate", InstallSpriteCaptureUIOnCreate, RemoveSpriteCaptureUIOnCreate, true },

    // --- StatusBarUI ---
    { "StatusBarUI::ScalarDtor", InstallStatusBarUIScalarDtor, RemoveStatusBarUIScalarDtor, true },
    { "StatusBarUI::Draw", InstallStatusBarUIDraw, RemoveStatusBarUIDraw, true },
    { "StatusBarUI::Startup", InstallStatusBarUIStartup, RemoveStatusBarUIStartup, true }, // RE-ENABLED: reimpl doesn't use trampoline
    { "StatusBarUI::OnRemove", InstallStatusBarUIOnRemove, RemoveStatusBarUIOnRemove, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // --- StatusIconUI ---
    { "StatusIconUI::ScalarDtor", InstallStatusIconUIScalarDtor, RemoveStatusIconUIScalarDtor, true },
    { "StatusIconUI::Serialize", InstallStatusIconUISerialize, RemoveStatusIconUISerialize, true },
    { "StatusIconUI::Startup", InstallStatusIconUIStartup, RemoveStatusIconUIStartup, true },

    // --- StorageCapacityUI ---
    { "StorageCapacityUI::ScalarDtor", InstallStorageCapacityUIScalarDtor, RemoveStorageCapacityUIScalarDtor, true },
    { "StorageCapacityUI::Serialize", InstallStorageCapacityUISerialize, RemoveStorageCapacityUISerialize, true },
    { "StorageCapacityUI::Startup", InstallStorageCapacityUIStartup, RemoveStorageCapacityUIStartup, true },

    // --- StorageUI ---
    { "StorageUI::ScalarDtor", InstallStorageUIScalarDtor, RemoveStorageUIScalarDtor, true },
    { "StorageUI::Serialize", InstallStorageUISerialize, RemoveStorageUISerialize, true },
    { "StorageUI::Startup", InstallStorageUIStartup, RemoveStorageUIStartup, true },

    // --- TitleDescToolTipUI ---
    { "TitleDescToolTipUI::ScalarDtor", InstallTitleDescToolTipUIScalarDtor, RemoveTitleDescToolTipUIScalarDtor, true },
    { "TitleDescToolTipUI::Serialize", InstallTitleDescToolTipUISerialize, RemoveTitleDescToolTipUISerialize, true },

    // --- ToolUI ---
    { "ToolUI::ScalarDtor", InstallToolUIScalarDtor, RemoveToolUIScalarDtor, true },
    { "ToolUI::Serialize", InstallToolUISerialize, RemoveToolUISerialize, true },
    { "ToolUI::OnCreate", InstallToolUIOnCreate, RemoveToolUIOnCreate, true },
    { "ToolUI::OnRemove", InstallToolUIOnRemove, RemoveToolUIOnRemove, true },

    // --- TownHallUI ---
    { "TownHallUI::ScalarDtor", InstallTownHallUIScalarDtor, RemoveTownHallUIScalarDtor, true },
    { "TownHallUI::Serialize", InstallTownHallUISerialize, RemoveTownHallUISerialize, true },
    { "TownHallUI::Startup", InstallTownHallUIStartup, RemoveTownHallUIStartup, true }, // RE-ENABLED: clean reimpl, no trampoline needed
    { "TownHallUI::OnCreate", InstallTownHallUIOnCreate, RemoveTownHallUIOnCreate, true }, // RE-ENABLED: clean reimpl, no trampoline needed
    { "TownHallUI::OnRemove", InstallTownHallUIOnRemove, RemoveTownHallUIOnRemove, true }, // RE-ENABLED: clean reimpl, no trampoline needed
    { "TownHallUI::Update", InstallTownHallUIUpdate, RemoveTownHallUIUpdate, true },

    // --- TradeUI ---
    { "TradeUI::ScalarDtor", InstallTradeUIScalarDtor, RemoveTradeUIScalarDtor, true },
    { "TradeUI::Serialize", InstallTradeUISerialize, RemoveTradeUISerialize, true },
    { "TradeUI::Draw", InstallTradeUIDraw, RemoveTradeUIDraw, true },
    { "TradeUI::Slot6", InstallTradeUISlot6, RemoveTradeUISlot6, true }, // RE-ENABLED: clean reimpl, no trampoline needed
    { "TradeUI::Startup", InstallTradeUIStartup, RemoveTradeUIStartup, true },
    { "TradeUI::OnCreate", InstallTradeUIOnCreate, RemoveTradeUIOnCreate, true },
    { "TradeUI::OnRemove", InstallTradeUIOnRemove, RemoveTradeUIOnRemove, true },
    { "TradeUI::Update", InstallTradeUIUpdate, RemoveTradeUIUpdate, true },

    // --- TutorialUI ---
    { "TutorialUI::ScalarDtor", InstallTutorialUIScalarDtor, RemoveTutorialUIScalarDtor, true },
    { "TutorialUI::Serialize", InstallTutorialUISerialize, RemoveTutorialUISerialize, true },
    { "TutorialUI::Startup", InstallTutorialUIStartup, RemoveTutorialUIStartup, true },
    { "TutorialUI::OnRemove", InstallTutorialUIOnRemove, RemoveTutorialUIOnRemove, true },

    // --- UpgradeUI ---
    { "UpgradeUI::ScalarDtor", InstallUpgradeUIScalarDtor, RemoveUpgradeUIScalarDtor, true },
    { "UpgradeUI::Serialize", InstallUpgradeUISerialize, RemoveUpgradeUISerialize, true },
    { "UpgradeUI::OnRemove", InstallUpgradeUIOnRemove, RemoveUpgradeUIOnRemove, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // --- WorkPlaceUI ---
    { "WorkPlaceUI::ScalarDtor", InstallWorkPlaceUIScalarDtor, RemoveWorkPlaceUIScalarDtor, true },
    { "WorkPlaceUI::Serialize", InstallWorkPlaceUISerialize, RemoveWorkPlaceUISerialize, true },
    { "WorkPlaceUI::Startup", InstallWorkPlaceUIStartup, RemoveWorkPlaceUIStartup, true },
    { "WorkPlaceUI::OnRemove", InstallWorkPlaceUIOnRemove, RemoveWorkPlaceUIOnRemove, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // ====================================================================
    // Phase 4: Dialog Controllers
    // ====================================================================

    // --- EventNotifyDialog ---
    { "EventNotifyDialog::ScalarDtor", InstallEventNotifyDialogScalarDtor, RemoveEventNotifyDialogScalarDtor, true },
    { "EventNotifyDialog::Startup", InstallEventNotifyDialogStartup, RemoveEventNotifyDialogStartup, true },
    { "EventNotifyDialog::Update", InstallEventNotifyDialogUpdate, RemoveEventNotifyDialogUpdate, true }, // RE-ENABLED: reimpl has null guards for controller ptrs
    { "EventNotifyDialog::Load", InstallEventNotifyDialogLoad, RemoveEventNotifyDialogLoad, true }, // RE-ENABLED: reimpl doesn't use trampoline
    { "EventNotifyDialog::OnEvent", InstallEventNotifyDialogOnEvent, RemoveEventNotifyDialogOnEvent, true }, // full reimpl, no trampoline

    // --- MapParamsDialog ---
    { "MapParamsDialog::ScalarDtor", InstallMapParamsDialogScalarDtor, RemoveMapParamsDialogScalarDtor, true },
    { "MapParamsDialog::Startup", InstallMapParamsDialogStartup, RemoveMapParamsDialogStartup, true },
    { "MapParamsDialog::Update", InstallMapParamsDialogUpdate, RemoveMapParamsDialogUpdate, true },
    { "MapParamsDialog::OnEvent", InstallMapParamsDialogOnEvent, RemoveMapParamsDialogOnEvent, true }, // full reimpl, no trampoline

    // --- NewGameDialog ---
    { "NewGameDialog::ScalarDtor", InstallNewGameDialogScalarDtor, RemoveNewGameDialogScalarDtor, true },
    { "NewGameDialog::Startup", InstallNewGameDialogStartup, RemoveNewGameDialogStartup, true },
    { "NewGameDialog::Update", InstallNewGameDialogUpdate, RemoveNewGameDialogUpdate, true },
    { "NewGameDialog::OnEvent", InstallNewGameDialogOnEvent, RemoveNewGameDialogOnEvent, true }, // full reimpl, no trampoline

    // --- OptionsDialog ---
    { "OptionsDialog::ScalarDtor", InstallOptionsDialogScalarDtor, RemoveOptionsDialogScalarDtor, true },
    { "OptionsDialog::Startup", InstallOptionsDialogStartup, RemoveOptionsDialogStartup, true },
    { "OptionsDialog::OnEvent", InstallOptionsDialogOnEvent, RemoveOptionsDialogOnEvent, true },

    // --- OptionInputBindDialog ---
    { "OptionInputBindDialog::ScalarDtor", InstallOptionInputBindDialogScalarDtor, RemoveOptionInputBindDialogScalarDtor, true },
    { "OptionInputBindDialog::Startup", InstallOptionInputBindDialogStartup, RemoveOptionInputBindDialogStartup, true },
    { "OptionInputBindDialog::Load", InstallOptionInputBindDialogLoad, RemoveOptionInputBindDialogLoad, true },
    { "OptionInputBindDialog::OnEvent", InstallOptionInputBindDialogOnEvent, RemoveOptionInputBindDialogOnEvent, true }, // full reimpl, no trampoline
    { "OptionInputBindDialog::OnOk", InstallOptionInputBindDialogOnOk, RemoveOptionInputBindDialogOnOk, true },
    { "OptionInputBindDialog::OnCancel", InstallOptionInputBindDialogOnCancel, RemoveOptionInputBindDialogOnCancel, true },
    { "OptionInputBindDialog::OnButton2", InstallOptionInputBindDialogOnButton2, RemoveOptionInputBindDialogOnButton2, true },
    { "OptionInputBindDialog::OnButton3", InstallOptionInputBindDialogOnButton3, RemoveOptionInputBindDialogOnButton3, true },

    // --- SaveLoadDialog ---
    { "SaveLoadDialog::ScalarDtor", InstallSaveLoadDialogScalarDtor, RemoveSaveLoadDialogScalarDtor, true },
    { "SaveLoadDialog::Startup", InstallSaveLoadDialogStartup, RemoveSaveLoadDialogStartup, true },
    { "SaveLoadDialog::Update", InstallSaveLoadDialogUpdate, RemoveSaveLoadDialogUpdate, true }, // RE-ENABLED: null guards added
    { "SaveLoadDialog::Load", InstallSaveLoadDialogLoad, RemoveSaveLoadDialogLoad, true }, // RE-ENABLED: reimpl doesn't use trampoline
    { "SaveLoadDialog::OnEvent", InstallSaveLoadDialogOnEvent, RemoveSaveLoadDialogOnEvent, true }, // full reimpl, no trampoline

    // --- ToolbarDialog ---
    { "ToolbarDialog::ScalarDtor", InstallToolbarDialogScalarDtor, RemoveToolbarDialogScalarDtor, true },
    { "ToolbarDialog::Update", InstallToolbarDialogUpdate, RemoveToolbarDialogUpdate, true },
    { "ToolbarDialog::OnEvent", InstallToolbarDialogOnEvent, RemoveToolbarDialogOnEvent, true },

    // --- TutorialHighlightDialog ---
    { "TutorialHighlightDialog::ScalarDtor", InstallTutorialHighlightDialogScalarDtor, RemoveTutorialHighlightDialogScalarDtor, true },
    { "TutorialHighlightDialog::Startup", InstallTutorialHighlightDialogStartup, RemoveTutorialHighlightDialogStartup, true },
    { "TutorialHighlightDialog::Update", InstallTutorialHighlightDialogUpdate, RemoveTutorialHighlightDialogUpdate, true }, // vtable patch (rel branch at byte 8)
    { "TutorialHighlightDialog::Load", InstallTutorialHighlightDialogLoad, RemoveTutorialHighlightDialogLoad, true },       // vtable patch (11b, too short)

    // --- TutorialGoalDialog ---
    { "TutorialGoalDialog::ScalarDtor", InstallTutorialGoalDialogScalarDtor, RemoveTutorialGoalDialogScalarDtor, true },
    { "TutorialGoalDialog::Shutdown", InstallTutorialGoalDialogShutdown, RemoveTutorialGoalDialogShutdown, true },
    { "TutorialGoalDialog::OnEvent", InstallTutorialGoalDialogOnEvent, RemoveTutorialGoalDialogOnEvent, true },
    { "TutorialGoalDialog::OnInit", InstallTutorialGoalDialogOnInit, RemoveTutorialGoalDialogOnInit, true },
    { "TutorialGoalDialog::OnDestroy", InstallTutorialGoalDialogOnDestroy, RemoveTutorialGoalDialogOnDestroy, true },

    // --- TutorialSetupDialog ---
    { "TutorialSetupDialog::ScalarDtor", InstallTutorialSetupDialogScalarDtor, RemoveTutorialSetupDialogScalarDtor, true },
    { "TutorialSetupDialog::Shutdown", InstallTutorialSetupDialogShutdown, RemoveTutorialSetupDialogShutdown, true },
    { "TutorialSetupDialog::OnEvent", InstallTutorialSetupDialogOnEvent, RemoveTutorialSetupDialogOnEvent, true },
    { "TutorialSetupDialog::Slot8", InstallTutorialSetupDialogSlot8, RemoveTutorialSetupDialogSlot8, true },
    { "TutorialSetupDialog::Slot9", InstallTutorialSetupDialogSlot9, RemoveTutorialSetupDialogSlot9, true },
    { "TutorialSetupDialog::Slot10", InstallTutorialSetupDialogSlot10, RemoveTutorialSetupDialogSlot10, true },

    // Phase 4: Simple Dialog Controllers
    // --- AchievementDialog ---
    { "AchievementDialog::Dtor", InstallAchievementDialogDtor, RemoveAchievementDialogDtor, true },
    { "AchievementDialog::Slot4", InstallAchievementDialogSlot4, RemoveAchievementDialogSlot4, true },
    { "AchievementDialog::Slot6", InstallAchievementDialogSlot6, RemoveAchievementDialogSlot6, true },
    { "AchievementDialog::Slot8", InstallAchievementDialogSlot8, RemoveAchievementDialogSlot8, true }, // RE-ENABLED: reimpl doesn't use trampoline
    { "AchievementDialog::Slot10", InstallAchievementDialogSlot10, RemoveAchievementDialogSlot10, true },
    { "AchievementDialog::Slot14", InstallAchievementDialogSlot14, RemoveAchievementDialogSlot14, true }, // RE-ENABLED: reimpl doesn't use trampoline
    { "AchievementDialog::Slot17", InstallAchievementDialogSlot17, RemoveAchievementDialogSlot17, true },
    // --- AutoSaveDialog ---
    { "AutoSaveDialog::Dtor", InstallAutoSaveDialogDtor, RemoveAutoSaveDialogDtor, true },
    { "AutoSaveDialog::Slot6", InstallAutoSaveDialogSlot6, RemoveAutoSaveDialogSlot6, true }, // RE-ENABLED: null guards added
    { "AutoSaveDialog::Slot7", InstallAutoSaveDialogSlot7, RemoveAutoSaveDialogSlot7, true },
    // --- EventPopupDialog ---
    { "EventPopupDialog::Dtor", InstallEventPopupDialogDtor, RemoveEventPopupDialogDtor, true },
    { "EventPopupDialog::Slot4", InstallEventPopupDialogSlot4, RemoveEventPopupDialogSlot4, true },
    { "EventPopupDialog::Slot8", InstallEventPopupDialogSlot8, RemoveEventPopupDialogSlot8, true }, // RE-ENABLED: reimpl doesn't use trampoline
    // --- FollowDialog ---
    { "FollowDialog::Dtor", InstallFollowDialogDtor, RemoveFollowDialogDtor, true },
    { "FollowDialog::Slot4", InstallFollowDialogSlot4, RemoveFollowDialogSlot4, true },
    { "FollowDialog::Slot5", InstallFollowDialogSlot5, RemoveFollowDialogSlot5, true }, // prologueSize=12, RIP-relative prologue but trampoline unused
    { "FollowDialog::Slot6", InstallFollowDialogSlot6, RemoveFollowDialogSlot6, true }, // RE-ENABLED: reimpl doesn't use trampoline
    { "FollowDialog::Slot7", InstallFollowDialogSlot7, RemoveFollowDialogSlot7, true },
    { "FollowDialog::Slot8", InstallFollowDialogSlot8, RemoveFollowDialogSlot8, true }, // prologueSize=12, JNZ rel8 in prologue but trampoline unused
    // --- GoalDialog ---
    { "GoalDialog::Dtor", InstallGoalDialogDtor, RemoveGoalDialogDtor, true },
    { "GoalDialog::Slot4", InstallGoalDialogSlot4, RemoveGoalDialogSlot4, true },
    { "GoalDialog::Slot6", InstallGoalDialogSlot6, RemoveGoalDialogSlot6, true }, // RE-ENABLED: reimpl doesn't use trampoline
    { "GoalDialog::Slot8", InstallGoalDialogSlot8, RemoveGoalDialogSlot8, true }, // RE-ENABLED: reimpl doesn't use trampoline
    // --- LoaderDialog ---
    { "LoaderDialog::Dtor", InstallLoaderDialogDtor, RemoveLoaderDialogDtor, true },
    { "LoaderDialog::Slot4", InstallLoaderDialogSlot4, RemoveLoaderDialogSlot4, true },
    { "LoaderDialog::Slot6", InstallLoaderDialogSlot6, RemoveLoaderDialogSlot6, true },
    { "LoaderDialog::Slot7", InstallLoaderDialogSlot7, RemoveLoaderDialogSlot7, true },
    // --- MainMenuDialog ---
    { "MainMenuDialog::Dtor", InstallMainMenuDialogDtor, RemoveMainMenuDialogDtor, true },
    { "MainMenuDialog::Slot4", InstallMainMenuDialogSlot4, RemoveMainMenuDialogSlot4, true },
    { "MainMenuDialog::Slot5", InstallMainMenuDialogSlot5, RemoveMainMenuDialogSlot5, true },
    { "MainMenuDialog::Slot8", InstallMainMenuDialogSlot8, RemoveMainMenuDialogSlot8, true }, // RE-ENABLED: reimpl doesn't use trampoline
    { "MainMenuDialog::Slot10", InstallMainMenuDialogSlot10, RemoveMainMenuDialogSlot10, true },
    // --- MessageDialog ---
    { "MessageDialog::Dtor", InstallMessageDialogDtor, RemoveMessageDialogDtor, true },
    { "MessageDialog::Slot4", InstallMessageDialogSlot4, RemoveMessageDialogSlot4, true },
    { "MessageDialog::Slot10", InstallMessageDialogSlot10, RemoveMessageDialogSlot10, true },
    { "MessageDialog::Slot11", InstallMessageDialogSlot11, RemoveMessageDialogSlot11, true },
    { "MessageDialog::Slot12", InstallMessageDialogSlot12, RemoveMessageDialogSlot12, true },
    { "MessageDialog::Slot13", InstallMessageDialogSlot13, RemoveMessageDialogSlot13, true },
    // --- ModDetailsDialog ---
    { "ModDetailsDialog::Dtor", InstallModDetailsDialogDtor, RemoveModDetailsDialogDtor, true },
    { "ModDetailsDialog::Slot4", InstallModDetailsDialogSlot4, RemoveModDetailsDialogSlot4, true },
    { "ModDetailsDialog::Slot8", InstallModDetailsDialogSlot8, RemoveModDetailsDialogSlot8, true }, // RE-ENABLED: reimpl doesn't use trampoline
    // --- ModUploadDialog ---
    { "ModUploadDialog::Dtor", InstallModUploadDialogDtor, RemoveModUploadDialogDtor, true },
    // --- ModDialog ---
    { "ModDialog::Dtor", InstallModDialogDtor, RemoveModDialogDtor, true },
    { "ModDialog::Slot4", InstallModDialogSlot4, RemoveModDialogSlot4, true },
    { "ModDialog::Slot8", InstallModDialogSlot8, RemoveModDialogSlot8, true },
    { "ModDialog::Slot10", InstallModDialogSlot10, RemoveModDialogSlot10, true },
    // --- ScenarioDialog ---
    { "ScenarioDialog::Dtor", InstallScenarioDialogDtor, RemoveScenarioDialogDtor, true },
    { "ScenarioDialog::Slot4", InstallScenarioDialogSlot4, RemoveScenarioDialogSlot4, true },
    { "ScenarioDialog::Slot6", InstallScenarioDialogSlot6, RemoveScenarioDialogSlot6, true },
    { "ScenarioDialog::Slot8", InstallScenarioDialogSlot8, RemoveScenarioDialogSlot8, true },

    // ================================================================
    // Phase 5: Descriptions
    // ================================================================
    { "CreateBridgeDesc::Dtor", InstallCreateBridgeDescriptionDtor, RemoveCreateBridgeDescriptionDtor, true },
    { "CreateBridgeDesc::Serialize", InstallCreateBridgeDescriptionSerialize, RemoveCreateBridgeDescriptionSerialize, true },
    { "CreateBridgeDesc::Cleanup", InstallCreateBridgeDescriptionCleanup, RemoveCreateBridgeDescriptionCleanup, true },
    { "CreateDragDesc::Dtor", InstallCreateDragDescriptionDtor, RemoveCreateDragDescriptionDtor, true },
    { "CreateDragDesc::Serialize", InstallCreateDragDescriptionSerialize, RemoveCreateDragDescriptionSerialize, true },
    { "CreateDragDesc::Cleanup", InstallCreateDragDescriptionCleanup, RemoveCreateDragDescriptionCleanup, true },
    { "CreateFencedDesc::Dtor", InstallCreateFencedDescriptionDtor, RemoveCreateFencedDescriptionDtor, true },
    { "CreateFencedDesc::Serialize", InstallCreateFencedDescriptionSerialize, RemoveCreateFencedDescriptionSerialize, true },
    { "CreateFencedDesc::Cleanup", InstallCreateFencedDescriptionCleanup, RemoveCreateFencedDescriptionCleanup, true },
    { "CreatePathDesc::Dtor", InstallCreatePathDescriptionDtor, RemoveCreatePathDescriptionDtor, true },
    { "CreatePathDesc::Serialize", InstallCreatePathDescriptionSerialize, RemoveCreatePathDescriptionSerialize, true },
    { "CreatePathDesc::Cleanup", InstallCreatePathDescriptionCleanup, RemoveCreatePathDescriptionCleanup, true },
    { "CreatePathDesc::CreateInPlace", InstallCreatePathDescriptionCreateInPlace, RemoveCreatePathDescriptionCreateInPlace, true },
    { "CreatePlacedDesc::Dtor", InstallCreatePlacedDescriptionDtor, RemoveCreatePlacedDescriptionDtor, true },
    { "CreatePlacedDesc::Serialize", InstallCreatePlacedDescriptionSerialize, RemoveCreatePlacedDescriptionSerialize, true },
    { "CreatePlacedDesc::Cleanup", InstallCreatePlacedDescriptionCleanup, RemoveCreatePlacedDescriptionCleanup, true },
    { "CreateRemovalDesc::Dtor", InstallCreateRemovalDescriptionDtor, RemoveCreateRemovalDescriptionDtor, true },
    { "CreateRemovalDesc::Serialize", InstallCreateRemovalDescriptionSerialize, RemoveCreateRemovalDescriptionSerialize, true },
    { "CreateRemovalDesc::Cleanup", InstallCreateRemovalDescriptionCleanup, RemoveCreateRemovalDescriptionCleanup, true },
    { "CreateRemovalDesc::CreateInPlace", InstallCreateRemovalDescriptionCreateInPlace, RemoveCreateRemovalDescriptionCreateInPlace, true },
    { "CreateRoadDesc::Dtor", InstallCreateRoadDescriptionDtor, RemoveCreateRoadDescriptionDtor, true },
    { "CreateRoadDesc::Serialize", InstallCreateRoadDescriptionSerialize, RemoveCreateRoadDescriptionSerialize, true },
    { "CreateRoadDesc::Cleanup", InstallCreateRoadDescriptionCleanup, RemoveCreateRoadDescriptionCleanup, true },
    { "CreateRoadDesc::CreateInPlace", InstallCreateRoadDescriptionCreateInPlace, RemoveCreateRoadDescriptionCreateInPlace, true },
    { "RadiusDesc::Dtor", InstallRadiusDescriptionDtor, RemoveRadiusDescriptionDtor, true },
    { "RadiusDesc::Serialize", InstallRadiusDescriptionSerialize, RemoveRadiusDescriptionSerialize, true },
    { "RadiusDesc::Cleanup", InstallRadiusDescriptionCleanup, RemoveRadiusDescriptionCleanup, true },
    { "RadiusDesc::CreateInPlace", InstallRadiusDescriptionCreateInPlace, RemoveRadiusDescriptionCreateInPlace, true },
    { "SelectDesc::Dtor", InstallSelectDescriptionDtor, RemoveSelectDescriptionDtor, true },
    { "SelectDesc::Serialize", InstallSelectDescriptionSerialize, RemoveSelectDescriptionSerialize, true },
    { "SelectDesc::Cleanup", InstallSelectDescriptionCleanup, RemoveSelectDescriptionCleanup, true },
    { "SelectDesc::CreateInPlace", InstallSelectDescriptionCreateInPlace, RemoveSelectDescriptionCreateInPlace, true },
    { "ValueDesc::Dtor", InstallValueDescriptionDtor, RemoveValueDescriptionDtor, true },
    { "ValueDesc::Serialize", InstallValueDescriptionSerialize, RemoveValueDescriptionSerialize, true },
    { "ValueDesc::Cleanup", InstallValueDescriptionCleanup, RemoveValueDescriptionCleanup, true },
    { "ValueDesc::CreateInPlace", InstallValueDescriptionCreateInPlace, RemoveValueDescriptionCreateInPlace, true },
    { "ToolbarDesc::Dtor", InstallToolbarDescriptionDtor, RemoveToolbarDescriptionDtor, true },
    { "ToolbarDesc::Serialize", InstallToolbarDescriptionSerialize, RemoveToolbarDescriptionSerialize, true },
    { "ToolbarDesc::Cleanup", InstallToolbarDescriptionCleanup, RemoveToolbarDescriptionCleanup, true },
    { "EnvironmentDesc::Dtor", InstallEnvironmentDescriptionDtor, RemoveEnvironmentDescriptionDtor, true },
    { "EnvironmentDesc::Serialize", InstallEnvironmentDescriptionSerialize, RemoveEnvironmentDescriptionSerialize, true },
    { "EnvironmentDesc::Cleanup", InstallEnvironmentDescriptionCleanup, RemoveEnvironmentDescriptionCleanup, true },
    { "CaptureBoxDesc::Dtor", InstallCaptureBoxDescriptionDtor, RemoveCaptureBoxDescriptionDtor, true },
    { "CaptureBoxDesc::Slot3", InstallCaptureBoxDescriptionSlot3, RemoveCaptureBoxDescriptionSlot3, true }, // RE-ENABLED: reimpl doesn't use trampoline
    { "CaptureBoxDesc::Serialize", InstallCaptureBoxDescriptionSerialize, RemoveCaptureBoxDescriptionSerialize, true },
    { "CaptureBoxDesc::Cleanup", InstallCaptureBoxDescriptionCleanup, RemoveCaptureBoxDescriptionCleanup, true },
    { "CaptureBoxDesc::CreateElement", InstallCaptureBoxDescriptionCreateElement, RemoveCaptureBoxDescriptionCreateElement, true },
    { "CaptureBoxDesc::CreateInPlace", InstallCaptureBoxDescriptionCreateInPlace, RemoveCaptureBoxDescriptionCreateInPlace, true },
    { "FrustumOutlineDesc::Dtor", InstallFrustumOutlineDescriptionDtor, RemoveFrustumOutlineDescriptionDtor, true },
    { "FrustumOutlineDesc::Slot3", InstallFrustumOutlineDescriptionSlot3, RemoveFrustumOutlineDescriptionSlot3, true }, // RE-ENABLED: reimpl doesn't use trampoline
    { "FrustumOutlineDesc::Serialize", InstallFrustumOutlineDescriptionSerialize, RemoveFrustumOutlineDescriptionSerialize, true },
    { "FrustumOutlineDesc::Cleanup", InstallFrustumOutlineDescriptionCleanup, RemoveFrustumOutlineDescriptionCleanup, true },
    { "FrustumOutlineDesc::CreateElement", InstallFrustumOutlineDescriptionCreateElement, RemoveFrustumOutlineDescriptionCreateElement, true },
    { "FrustumOutlineDesc::CreateInPlace", InstallFrustumOutlineDescriptionCreateInPlace, RemoveFrustumOutlineDescriptionCreateInPlace, true },
    // UI Elements
    { "CaptureBoxElement::Dtor", InstallCaptureBoxElementDtor, RemoveCaptureBoxElementDtor, true },
    { "CaptureBoxElement::OnCreate", InstallCaptureBoxElementOnCreate, RemoveCaptureBoxElementOnCreate, true }, // RE-ENABLED: reimpl doesn't use trampoline
    { "CaptureBoxElement::ComputeLayout", InstallCaptureBoxElementComputeLayout, RemoveCaptureBoxElementComputeLayout, true },
    { "CaptureBoxElement::Draw", InstallCaptureBoxElementDraw, RemoveCaptureBoxElementDraw, true },
    { "FrustumOutlineElement::Dtor", InstallFrustumOutlineElementDtor, RemoveFrustumOutlineElementDtor, true },
    { "FrustumOutlineElement::OnCreate", InstallFrustumOutlineElementOnCreate, RemoveFrustumOutlineElementOnCreate, true }, // RE-ENABLED: reimpl doesn't use trampoline
    { "FrustumOutlineElement::ComputeLayout", InstallFrustumOutlineElementComputeLayout, RemoveFrustumOutlineElementComputeLayout, true },
    { "FrustumOutlineElement::Draw", InstallFrustumOutlineElementDraw, RemoveFrustumOutlineElementDraw, true }, // RE-ENABLED: reimpl doesn't use trampoline

    // ================================================================
    // Phase 5: Tools & Placement
    // ================================================================
    { "Toolbar::Dtor", InstallToolbarDtor, RemoveToolbarDtor, true },
    { "Toolbar::Serialize", InstallToolbarSerialize, RemoveToolbarSerialize, true },
    { "Toolbar::Cleanup", InstallToolbarCleanup, RemoveToolbarCleanup, true },
    { "RotationSetting::Dtor", InstallRotationSettingDtor, RemoveRotationSettingDtor, true },
    { "SetTextParams::NullGuard", InstallSetTextParams, RemoveSetTextParams, true },
    { "DragTool::Dtor", InstallDragToolDtor, RemoveDragToolDtor, true },
    { "DragTool::OnKey", InstallDragToolOnKey, RemoveDragToolOnKey, true },
    { "BridgeTool::Dtor", InstallBridgeToolDtor, RemoveBridgeToolDtor, true },
    { "BridgeTool::Startup", InstallBridgeToolStartup, RemoveBridgeToolStartup, true },
    { "BridgeTool::CalcPath", InstallBridgeToolCalcPath, RemoveBridgeToolCalcPath, true },
    { "BridgeTool::CalcCost", InstallBridgeToolCalcCost, RemoveBridgeToolCalcCost, true },
    { "DragPlacementTool::Dtor", InstallDragPlacementToolDtor, RemoveDragPlacementToolDtor, true },
    { "DragPlacementTool::Startup", InstallDragPlacementToolStartup, RemoveDragPlacementToolStartup, true },
    { "DragPlacementTool::Draw", InstallDragPlacementToolDraw, RemoveDragPlacementToolDraw, true },
    { "DragPlacementTool::CalcPath", InstallDragPlacementToolCalcPath, RemoveDragPlacementToolCalcPath, true },
    { "DragPlacementTool::CalcCost", InstallDragPlacementToolCalcCost, RemoveDragPlacementToolCalcCost, true },
    { "EditTool::Dtor", InstallEditToolDtor, RemoveEditToolDtor, true },
    { "EditTool::OnKey", InstallEditToolOnKey, RemoveEditToolOnKey, true },
    { "EditTool::Startup", InstallEditToolStartup, RemoveEditToolStartup, true },
    { "EditTool::Serialize", InstallEditToolSerialize, RemoveEditToolSerialize, true },
    { "FencedTool::Dtor", InstallFencedToolDtor, RemoveFencedToolDtor, true },
    { "FencedTool::Startup", InstallFencedToolStartup, RemoveFencedToolStartup, true },
    { "FencedTool::Shutdown2", InstallFencedToolShutdown2, RemoveFencedToolShutdown2, true },
    { "FencedTool::OnKey", InstallFencedToolOnKey, RemoveFencedToolOnKey, true },
    { "FencedTool::Serialize", InstallFencedToolSerialize, RemoveFencedToolSerialize, true },
    { "FencedTool::Draw", InstallFencedToolDraw, RemoveFencedToolDraw, true },
    { "FencedTool::Shutdown", InstallFencedToolShutdown, RemoveFencedToolShutdown, true },
    { "FencedTool::CalcPath", InstallFencedToolCalcPath, RemoveFencedToolCalcPath, true },
    { "FencedTool::CalcCost", InstallFencedToolCalcCost, RemoveFencedToolCalcCost, true },
    { "PathTool::Dtor", InstallPathToolDtor, RemovePathToolDtor, true },
    { "PathTool::OnKey", InstallPathToolOnKey, RemovePathToolOnKey, true },
    { "PathTool::Draw", InstallPathToolDraw, RemovePathToolDraw, true },
    { "PathTool::Serialize", InstallPathToolSerialize, RemovePathToolSerialize, true },
    { "PlacementTool::Dtor", InstallPlacementToolDtor, RemovePlacementToolDtor, true },
    { "PlacementTool::Startup", InstallPlacementToolStartup, RemovePlacementToolStartup, true },
    { "PlacementTool::Shutdown2", InstallPlacementToolShutdown2, RemovePlacementToolShutdown2, true },
    { "PlacementTool::OnKey", InstallPlacementToolOnKey, RemovePlacementToolOnKey, true },
    { "PlacementTool::Serialize", InstallPlacementToolSerialize, RemovePlacementToolSerialize, true },
    { "PlacementTool::Draw", InstallPlacementToolDraw, RemovePlacementToolDraw, true },
    { "RemovalTool::Dtor", InstallRemovalToolDtor, RemoveRemovalToolDtor, true },
    { "RemovalTool::Startup", InstallRemovalToolStartup, RemoveRemovalToolStartup, true },
    { "RemovalTool::Draw", InstallRemovalToolDraw, RemoveRemovalToolDraw, true },
    { "RemovalTool::CalcPath", InstallRemovalToolCalcPath, RemoveRemovalToolCalcPath, true },
    { "RemovalTool::CalcCost", InstallRemovalToolCalcCost, RemoveRemovalToolCalcCost, true },
    { "RoadTool::Dtor", InstallRoadToolDtor, RemoveRoadToolDtor, true },
    { "RoadTool::Startup", InstallRoadToolStartup, RemoveRoadToolStartup, true },
    { "RoadTool::Draw", InstallRoadToolDraw, RemoveRoadToolDraw, true },

    // ================================================================
    // Phase 5: Tutorial System
    // ================================================================
    { "Tutorial::Dtor", InstallTutorialDtor, RemoveTutorialDtor, true },
    { "Tutorial::Serialize", InstallTutorialSerialize, RemoveTutorialSerialize, true },
    { "Tutorial::Cleanup", InstallTutorialCleanup, RemoveTutorialCleanup, true },
    { "Tutorial::Ctor", InstallTutorialCtor, RemoveTutorialCtor, true },
    { "TutorialGoal::Dtor", InstallTutorialGoalDtor, RemoveTutorialGoalDtor, true },
    { "TutorialGoal::Serialize", InstallTutorialGoalSerialize, RemoveTutorialGoalSerialize, true },
    { "TutorialGoalButton::Dtor", InstallTutorialGoalButtonDtor, RemoveTutorialGoalButtonDtor, true },
    { "TutorialGoalButton::Serialize", InstallTutorialGoalButtonSerialize, RemoveTutorialGoalButtonSerialize, true },
    { "TutorialGoalButton::Cleanup", InstallTutorialGoalButtonCleanup, RemoveTutorialGoalButtonCleanup, true },
    { "TutorialGoalButton::Slot8", InstallTutorialGoalButtonSlot8, RemoveTutorialGoalButtonSlot8, true },
    { "TutorialGoalButton::Ctor", InstallTutorialGoalButtonCtor, RemoveTutorialGoalButtonCtor, true },
    { "TutorialGoalCount::Dtor", InstallTutorialGoalCountDtor, RemoveTutorialGoalCountDtor, true },
    { "TutorialGoalCount::Serialize", InstallTutorialGoalCountSerialize, RemoveTutorialGoalCountSerialize, true },
    { "TutorialGoalCount::Cleanup", InstallTutorialGoalCountCleanup, RemoveTutorialGoalCountCleanup, true },
    { "TutorialGoalCount::Slot8", InstallTutorialGoalCountSlot8, RemoveTutorialGoalCountSlot8, true }, // RE-ENABLED: case 6 fixed (was misidentified as stored count, actually work capacity)
    { "TutorialGoalCount::Ctor", InstallTutorialGoalCountCtor, RemoveTutorialGoalCountCtor, true }, // RE-ENABLED: vtable patch (near Jcc broke trampoline)
    { "TutorialGoalGameSpeed::Dtor", InstallTutorialGoalGameSpeedDtor, RemoveTutorialGoalGameSpeedDtor, true },
    { "TutorialGoalGameSpeed::Serialize", InstallTutorialGoalGameSpeedSerialize, RemoveTutorialGoalGameSpeedSerialize, true },
    { "TutorialGoalGameSpeed::Cleanup", InstallTutorialGoalGameSpeedCleanup, RemoveTutorialGoalGameSpeedCleanup, true },
    { "TutorialGoalGameSpeed::Slot8", InstallTutorialGoalGameSpeedSlot8, RemoveTutorialGoalGameSpeedSlot8, true }, // RE-ENABLED: vtable patch (RIP-relative broke trampoline)
    { "TutorialGoalGameSpeed::Ctor", InstallTutorialGoalGameSpeedCtor, RemoveTutorialGoalGameSpeedCtor, true },
    { "TutorialImage::Dtor", InstallTutorialImageDtor, RemoveTutorialImageDtor, true },
    { "TutorialImage::Serialize", InstallTutorialImageSerialize, RemoveTutorialImageSerialize, true },
    { "TutorialImage::Cleanup", InstallTutorialImageCleanup, RemoveTutorialImageCleanup, true },
    { "TutorialImage::Ctor", InstallTutorialImageCtor, RemoveTutorialImageCtor, true },
    { "TutorialImage::Slot9", InstallTutorialImageSlot9, RemoveTutorialImageSlot9, true },
    { "TutorialImageHotKey::Dtor", InstallTutorialImageHotKeyDtor, RemoveTutorialImageHotKeyDtor, true },
    { "TutorialImageHotKey::Serialize", InstallTutorialImageHotKeySerialize, RemoveTutorialImageHotKeySerialize, true },
    { "TutorialImageHotKey::Cleanup", InstallTutorialImageHotKeyCleanup, RemoveTutorialImageHotKeyCleanup, true },
    { "TutorialImageHotKey::Ctor", InstallTutorialImageHotKeyCtor, RemoveTutorialImageHotKeyCtor, true }, // RE-ENABLED: vtable patch (near Jcc broke trampoline)
    { "TutorialImageHotKey::Slot9", InstallTutorialImageHotKeySlot9, RemoveTutorialImageHotKeySlot9, true },
    { "TutorialImageInputKey::Dtor", InstallTutorialImageInputKeyDtor, RemoveTutorialImageInputKeyDtor, true },
    { "TutorialImageInputKey::Serialize", InstallTutorialImageInputKeySerialize, RemoveTutorialImageInputKeySerialize, true },
    { "TutorialImageInputKey::Cleanup", InstallTutorialImageInputKeyCleanup, RemoveTutorialImageInputKeyCleanup, true },
    { "TutorialImageInputKey::Ctor", InstallTutorialImageInputKeyCtor, RemoveTutorialImageInputKeyCtor, true },
    { "TutorialImageInputKey::Slot9", InstallTutorialImageInputKeySlot9, RemoveTutorialImageInputKeySlot9, true },
    { "TutorialImageToolbarIcon::Dtor", InstallTutorialImageToolbarIconDtor, RemoveTutorialImageToolbarIconDtor, true },
    { "TutorialImageToolbarIcon::Serialize", InstallTutorialImageToolbarIconSerialize, RemoveTutorialImageToolbarIconSerialize, true },
    { "TutorialImageToolbarIcon::Cleanup", InstallTutorialImageToolbarIconCleanup, RemoveTutorialImageToolbarIconCleanup, true },
    { "TutorialImageToolbarIcon::Ctor", InstallTutorialImageToolbarIconCtor, RemoveTutorialImageToolbarIconCtor, true }, // RE-ENABLED: vtable patch (near Jcc broke trampoline)
    { "TutorialImageToolbarIcon::Slot9", InstallTutorialImageToolbarIconSlot9, RemoveTutorialImageToolbarIconSlot9, true },
    { "TutorialInstance::Dtor", InstallTutorialInstanceDtor, RemoveTutorialInstanceDtor, true },
    { "TutorialInstance::Serialize", InstallTutorialInstanceSerialize, RemoveTutorialInstanceSerialize, true },
    { "TutorialSetup::Dtor", InstallTutorialSetupDtor, RemoveTutorialSetupDtor, true },
    { "TutorialSetup::Serialize", InstallTutorialSetupSerialize, RemoveTutorialSetupSerialize, true },
    { "TutorialSetupCamera::Dtor", InstallTutorialSetupCameraDtor, RemoveTutorialSetupCameraDtor, true },
    { "TutorialSetupCamera::Serialize", InstallTutorialSetupCameraSerialize, RemoveTutorialSetupCameraSerialize, true },
    { "TutorialSetupCamera::Cleanup", InstallTutorialSetupCameraCleanup, RemoveTutorialSetupCameraCleanup, true },
    { "TutorialSetupCamera::Slot8", InstallTutorialSetupCameraSlot8, RemoveTutorialSetupCameraSlot8, true }, // RE-ENABLED: vtable patch (RIP-relative broke trampoline)
    { "TutorialSetupCamera::Ctor", InstallTutorialSetupCameraCtor, RemoveTutorialSetupCameraCtor, true },
    { "TutorialSetupGameSpeed::Dtor", InstallTutorialSetupGameSpeedDtor, RemoveTutorialSetupGameSpeedDtor, true },
    { "TutorialSetupGameSpeed::Serialize", InstallTutorialSetupGameSpeedSerialize, RemoveTutorialSetupGameSpeedSerialize, true },
    { "TutorialSetupGameSpeed::Cleanup", InstallTutorialSetupGameSpeedCleanup, RemoveTutorialSetupGameSpeedCleanup, true },
    { "TutorialSetupGameSpeed::Slot8", InstallTutorialSetupGameSpeedSlot8, RemoveTutorialSetupGameSpeedSlot8, true }, // RE-ENABLED: vtable patch (RIP-relative broke trampoline)
    { "TutorialSetupGameSpeed::Ctor", InstallTutorialSetupGameSpeedCtor, RemoveTutorialSetupGameSpeedCtor, true },
    { "TutorialSetupRestrictPlayer::Dtor", InstallTutorialSetupRestrictPlayerDtor, RemoveTutorialSetupRestrictPlayerDtor, true },
    { "TutorialSetupRestrictPlayer::Serialize", InstallTutorialSetupRestrictPlayerSerialize, RemoveTutorialSetupRestrictPlayerSerialize, true },
    { "TutorialSetupRestrictPlayer::Cleanup", InstallTutorialSetupRestrictPlayerCleanup, RemoveTutorialSetupRestrictPlayerCleanup, true },
    { "TutorialSetupRestrictPlayer::Slot8", InstallTutorialSetupRestrictPlayerSlot8, RemoveTutorialSetupRestrictPlayerSlot8, true }, // RE-ENABLED: vtable patch (MOV R11,RSP broke trampoline)
    { "TutorialSetupRestrictPlayer::Slot10", InstallTutorialSetupRestrictPlayerSlot10, RemoveTutorialSetupRestrictPlayerSlot10, true }, // RE-ENABLED: vtable patch (RIP-relative broke trampoline)
    { "TutorialSetupRestrictPlayer::Ctor", InstallTutorialSetupRestrictPlayerCtor, RemoveTutorialSetupRestrictPlayerCtor, true }, // RE-ENABLED: vtable patch (near Jcc broke trampoline)
    { "TutorialStep::Dtor", InstallTutorialStepDtor, RemoveTutorialStepDtor, true },
    { "TutorialStep::Serialize", InstallTutorialStepSerialize, RemoveTutorialStepSerialize, true },
    { "TutorialStep::Cleanup", InstallTutorialStepCleanup, RemoveTutorialStepCleanup, true },
    { "TutorialStep::Init", InstallTutorialStepInit, RemoveTutorialStepInit, true }, // RE-ENABLED: vtable patch (near Jcc broke trampoline)

    // ================================================================
    // Phase 5: Achievement Goals
    // ================================================================
    { "BuildingGoal::Dtor", InstallBuildingGoalDtor, RemoveBuildingGoalDtor, true },
    { "BuildingGoal::Slot5", InstallBuildingGoalSlot5, RemoveBuildingGoalSlot5, true },
    { "BuildingGoal::Slot6", InstallBuildingGoalSlot6, RemoveBuildingGoalSlot6, true },
    { "BuildingGoal::Slot8", InstallBuildingGoalSlot8, RemoveBuildingGoalSlot8, true }, // ENABLED: clean reimpl, trampoline not used
    { "BuildingGoal::Slot10", InstallBuildingGoalSlot10, RemoveBuildingGoalSlot10, true },
    { "PopulationGoal::Dtor", InstallPopulationGoalDtor, RemovePopulationGoalDtor, true },
    { "PopulationGoal::Slot5", InstallPopulationGoalSlot5, RemovePopulationGoalSlot5, true },
    { "PopulationGoal::Slot6", InstallPopulationGoalSlot6, RemovePopulationGoalSlot6, true },
    { "PopulationGoal::Slot8", InstallPopulationGoalSlot8, RemovePopulationGoalSlot8, true }, // ENABLED: clean reimpl, trampoline not used
    { "PopulationGoal::Slot10", InstallPopulationGoalSlot10, RemovePopulationGoalSlot10, true },
    { "FinishTutorialGoal::Dtor", InstallFinishTutorialGoalDtor, RemoveFinishTutorialGoalDtor, true },
    { "FinishTutorialGoal::Slot5", InstallFinishTutorialGoalSlot5, RemoveFinishTutorialGoalSlot5, true },
    { "FinishTutorialGoal::Slot6", InstallFinishTutorialGoalSlot6, RemoveFinishTutorialGoalSlot6, true },
    { "FinishTutorialGoal::Slot10", InstallFinishTutorialGoalSlot10, RemoveFinishTutorialGoalSlot10, true },
    // NOTE: FinishTutorialGoal::Slot11 skipped — same address as FinishTutorialInstance::Dtor
    { "FinishTutorialInstance::Dtor", InstallFinishTutorialInstanceDtor, RemoveFinishTutorialInstanceDtor, true },
    { "FinishTutorialInstance::Slot3", InstallFinishTutorialInstanceSlot3, RemoveFinishTutorialInstanceSlot3, true },
    { "FinishTutorialInstance::Slot4", InstallFinishTutorialInstanceSlot4, RemoveFinishTutorialInstanceSlot4, true },
    { "FinishTutorialInstance::Slot5", InstallFinishTutorialInstanceSlot5, RemoveFinishTutorialInstanceSlot5, false },
    { "FinishTutorialInstance::Slot6", InstallFinishTutorialInstanceSlot6, RemoveFinishTutorialInstanceSlot6, true },
    { "StatGoal::Dtor", InstallStatGoalDtor, RemoveStatGoalDtor, true },
    { "StatGoal::Slot5", InstallStatGoalSlot5, RemoveStatGoalSlot5, true },
    { "StatGoal::Slot6", InstallStatGoalSlot6, RemoveStatGoalSlot6, true },
    { "StatGoal::Slot8", InstallStatGoalSlot8, RemoveStatGoalSlot8, true },
    { "StatGoal::Slot10", InstallStatGoalSlot10, RemoveStatGoalSlot10, true }, // RE-ENABLED: relocating trampoline handles near Jcc
    { "TimeGoal::Dtor", InstallTimeGoalDtor, RemoveTimeGoalDtor, true },
    { "TimeGoal::Slot5", InstallTimeGoalSlot5, RemoveTimeGoalSlot5, true },
    { "TimeGoal::Slot6", InstallTimeGoalSlot6, RemoveTimeGoalSlot6, true },
    { "TimeGoal::Slot10", InstallTimeGoalSlot10, RemoveTimeGoalSlot10, true },
    // NOTE: TimeGoal::Slot11 skipped — same address as TimeGoalInstance::Dtor
    { "TimeGoalInstance::Dtor", InstallTimeGoalInstanceDtor, RemoveTimeGoalInstanceDtor, true },
    { "TimeGoalInstance::Slot3", InstallTimeGoalInstanceSlot3, RemoveTimeGoalInstanceSlot3, true },
    { "TimeGoalInstance::Slot5", InstallTimeGoalInstanceSlot5, RemoveTimeGoalInstanceSlot5, true },
    { "TimeGoalInstance::Slot6", InstallTimeGoalInstanceSlot6, RemoveTimeGoalInstanceSlot6, true },

    // ================================================================
    // Phase 5: World Generation
    // ================================================================
    { "MapGenerator::Dtor", InstallMapGeneratorDtor, RemoveMapGeneratorDtor, true },
    { "MapGenerator::Slot5", InstallMapGeneratorSlot5, RemoveMapGeneratorSlot5, true },
    { "MapGenerator::Slot6", InstallMapGeneratorSlot6, RemoveMapGeneratorSlot6, true },
    { "MapGenerator::Slot8", InstallMapGeneratorSlot8, RemoveMapGeneratorSlot8, true }, // RE-ENABLED: relocating trampoline handles near Jcc
    { "TerrainGenerator::Dtor", InstallTerrainGeneratorDtor, RemoveTerrainGeneratorDtor, true },
    { "TerrainGenerator::Slot5", InstallTerrainGeneratorSlot5, RemoveTerrainGeneratorSlot5, true },
    { "TerrainGenerator::Slot6", InstallTerrainGeneratorSlot6, RemoveTerrainGeneratorSlot6, true },
    { "TerrainGenerator::Slot8", InstallTerrainGeneratorSlot8, RemoveTerrainGeneratorSlot8, true }, // RE-ENABLED: relocating trampoline handles near Jcc
    { "ClimateGenerator::Dtor", InstallClimateGeneratorDtor, RemoveClimateGeneratorDtor, true },
    { "ClimateGenerator::Slot5", InstallClimateGeneratorSlot5, RemoveClimateGeneratorSlot5, true },
    { "ClimateGenerator::Slot6", InstallClimateGeneratorSlot6, RemoveClimateGeneratorSlot6, true },
    { "ClimateGenerator::Slot8", InstallClimateGeneratorSlot8, RemoveClimateGeneratorSlot8, true }, // RE-ENABLED: relocating trampoline handles near Jcc

    // ================================================================
    // Phase 5: Miscellaneous Types
    // ================================================================
    { "ComponentEntity::Dtor", InstallComponentEntityDtor, RemoveComponentEntityDtor, true },
    { "ComponentEntity::Slot3", InstallComponentEntitySlot3, RemoveComponentEntitySlot3, true }, // Re-enabled: vtable patching (Bug Class 8 fix)
    { "ComponentEntity::Slot5", InstallComponentEntitySlot5, RemoveComponentEntitySlot5, true },
    { "ComponentEntity::Slot8", InstallComponentEntitySlot8, RemoveComponentEntitySlot8, true },
    // NOTE: ComponentEntity::Slot9 skipped — shared function 0x14003addc
    { "Profession::Dtor", InstallProfessionDtor, RemoveProfessionDtor, true },
    { "Profession::Slot5", InstallProfessionSlot5, RemoveProfessionSlot5, true },
    { "Profession::Slot6", InstallProfessionSlot6, RemoveProfessionSlot6, true },
    { "Profession::Slot8", InstallProfessionSlot8, RemoveProfessionSlot8, true }, // RE-ENABLED: relocating trampoline handles near Jcc
    { "ProfessionList::Dtor", InstallProfessionListDtor, RemoveProfessionListDtor, true },
    { "ProfessionList::Slot5", InstallProfessionListSlot5, RemoveProfessionListSlot5, true },
    { "ProfessionList::Slot6", InstallProfessionListSlot6, RemoveProfessionListSlot6, true },
    { "ProfessionList::Slot8", InstallProfessionListSlot8, RemoveProfessionListSlot8, true },
    { "Scenario::Dtor", InstallScenarioDtor, RemoveScenarioDtor, true },
    { "Scenario::Slot3", InstallScenarioSlot3, RemoveScenarioSlot3, true }, // RE-ENABLED: relocating trampoline handles RIP-relative call
    { "Scenario::Slot5", InstallScenarioSlot5, RemoveScenarioSlot5, true },
    { "Scenario::Slot6", InstallScenarioSlot6, RemoveScenarioSlot6, true },
    { "Scenario::Slot8", InstallScenarioSlot8, RemoveScenarioSlot8, true },
    { "Weather::Dtor", InstallWeatherDtor, RemoveWeatherDtor, true },
    { "Weather::Slot5", InstallWeatherSlot5, RemoveWeatherSlot5, true },
    { "Weather::Slot6", InstallWeatherSlot6, RemoveWeatherSlot6, true },
    { "Weather::Slot8", InstallWeatherSlot8, RemoveWeatherSlot8, true }, // RE-ENABLED: relocating trampoline handles near Jcc
    { "MenuScene::Dtor", InstallMenuSceneDtor, RemoveMenuSceneDtor, true },
    { "MenuScene::Slot5", InstallMenuSceneSlot5, RemoveMenuSceneSlot5, true }, // RE-ENABLED: vtable hook bypasses Bug Class 8 (SEH cookie) — no prologue modification
    { "MenuScene::Slot6", InstallMenuSceneSlot6, RemoveMenuSceneSlot6, true },
    { "MenuScene::Slot8", InstallMenuSceneSlot8, RemoveMenuSceneSlot8, true }, // RE-ENABLED: relocating trampoline handles near Jcc
};

static constexpr int g_reimplCount = sizeof(g_reimplementations) / sizeof(g_reimplementations[0]);

const ReimplEntry* GetReimplTable(int* outCount) {
    if (outCount) *outCount = g_reimplCount;
    return g_reimplementations;
}

// Set to -1 to install all hooks, or a specific index to stop after that many.
// Use for bisecting crashes: try 30 (engine core only), then 100, 200, etc.
static constexpr int DEBUG_MAX_HOOKS = -1; // All hooks

// When non-null, only hooks whose name starts with one of these prefixes are installed.
// Essential hooks (BuildNumber, PlatformConfigure) are always installed.
// Set to nullptr to install all hooks normally.
static const char* DEBUG_HOOK_FILTER[] = {
    nullptr  // sentinel — all hooks installed
};

static bool HookPassesFilter(const char* name) {
    // No filter = allow all
    if (DEBUG_HOOK_FILTER[0] == nullptr) return true;
    // Always allow essential system hooks
    if (strncmp(name, "BuildNumber", 11) == 0) return true;
    if (strncmp(name, "PlatformConfigure", 17) == 0) return true;
    // Check against filter prefixes
    for (int i = 0; DEBUG_HOOK_FILTER[i]; i++) {
        size_t len = strlen(DEBUG_HOOK_FILTER[i]);
        if (strncmp(name, DEBUG_HOOK_FILTER[i], len) == 0) return true;
    }
    return false;
}

void InstallHooks() {
    int limit = (DEBUG_MAX_HOOKS < 0) ? g_reimplCount : DEBUG_MAX_HOOKS;
    if (limit > g_reimplCount) limit = g_reimplCount;

#ifdef _DEBUG
    printf("[BanishedPatch] Installing hooks (filter %s)...\n",
           DEBUG_HOOK_FILTER[0] ? "ACTIVE" : "off");
#endif

    int installed = 0;
    for (int i = 0; i < limit; i++) {
        auto& r = g_reimplementations[i];
        if (!r.enabled) {
#ifdef _DEBUG
            printf("[BanishedPatch]   [%d] %s: DISABLED\n", i, r.name);
#endif
            continue;
        }
        if (!HookPassesFilter(r.name)) {
#ifdef _DEBUG
            printf("[BanishedPatch]   [%d] %s: FILTERED\n", i, r.name);
#endif
            continue;
        }
        r.install();
        installed++;
#ifdef _DEBUG
        printf("[BanishedPatch]   [%d] %s: installed\n", i, r.name);
#endif
    }

#ifdef _DEBUG
    printf("[BanishedPatch] %d hooks installed (%d total, %d filtered).\n",
           installed, g_reimplCount, g_reimplCount - installed);
#endif
}

void RemoveHooks() {
#ifdef _DEBUG
    printf("[BanishedPatch] Removing hooks...\n");
#endif

    for (int i = g_reimplCount - 1; i >= 0; i--) {
        g_reimplementations[i].remove();
    }

#ifdef _DEBUG
    printf("[BanishedPatch] Hooks removed.\n");
#endif
}
