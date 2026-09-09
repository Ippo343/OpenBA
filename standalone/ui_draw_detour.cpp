// ============================================================================
// standalone/ui_draw_detour.cpp — Wave 10 lane UIDRAW
// The detour for ?Draw@UserInterface@UI@@QEAAXAEAVContext@Video@@@Z 0x180121890
// ============================================================================
//
// Sibling of `standalone/ui_frame_detour.cpp` (lane UI1's `UserInterface::Update`)
// and `standalone/ui_detour.cpp` (`TabElement::UpdateContainers`). Read the
// first one's banner for why a UI frame function needs a CODE PATCH rather than
// an export registration: `Engine::MainLoop`'s frame calls both `Update` and
// `Draw` through direct rel32 CALLs inside the DLL that never consult the
// export table, so `RegisterRuntimeExport` redirects our own call sites and
// leaves the DLL running its own body.
//
// A separate file per subject because two lanes editing one file is how a wave
// loses a merge.
//
// ---------------------------------------------------------------------------
// WHY `RuntimeDetourInstall` AND NOT `InstallRuntimeDetourChecked` — AND WHY
// THE PATCH IS 13 BYTES, NOT 12
// ---------------------------------------------------------------------------
// The prologue, read out of the SHIPPED BYTES (not off a mnemonic listing, and
// not off lane UI1's note either — re-read here, and it agrees):
//
//   180121890  48 8B C4        MOV RAX,RSP          <- captures the entry RSP
//   180121893  48 89 48 08     MOV [RAX+0x8],RCX    <- `this` into home slot 1
//   180121897  55              PUSH RBP
//   180121898  53              PUSH RBX
//   180121899  56              PUSH RSI
//   18012189a  57              PUSH RDI
//   18012189b  41 54           PUSH R12             <- ends at 13
//   18012189d  41 55           PUSH R13
//   ...
//   1801218a3                  LEA RBP,[RAX-0x258]  <- RBP DERIVED FROM RAX
//
// **TWELVE IS NOT AN INSTRUCTION BOUNDARY HERE.** The boundaries around the
// 12-byte minimum are 11 and 13, because `PUSH R12` is the two-byte `41 54`.
// Patching 12 would leave the lone `0x54` behind and the process would die
// somewhere unrelated. The patch is 13 and `RuntimeDetourInstall` NOPs the
// thirteenth byte after the 12-byte absolute jump. None of the 13 bytes is
// PC-relative, so there is nothing for a relocating installer to relocate.
//
// ---------------------------------------------------------------------------
// THE INSTALLER CHOICE, AND ITS MECHANISM — THE LOAD-BEARING PART
// ---------------------------------------------------------------------------
// `InstallDetour` (and so every `InstallRuntimeDetour*` front door) DETECTS the
// leading `MOV RAX,RSP` and rewrites the trampoline copy to
// `MOV RAX,RSP; ADD RAX,8` ("Bug Class 3", runtime_detour.cpp:62-94), reasoning
// that "the CALL into the trampoline shifts RSP by 8".
//
// **THAT CORRECTION IS WRONG FOR THIS FUNCTION AND WOULD CORRUPT XMM6..XMM15.**
// Not an appeal to lane UI1's conclusion — the same mechanism, re-derived on
// THIS body's own instructions:
//
//   Retail's entry contract is RSP = E, return address at [E], the caller's
//   32-byte home space at [E+8]. When our wrapper CALLs the trampoline the
//   trampoline sees RSP = R, return address at [R], OUR home space at [R+8] —
//   the SAME contract with E := R. Nothing is shifted; R plays E's role, which
//   is also why `MOV [RAX+8],RCX` lands harmlessly in our own shadow space.
//
//   The function then mixes the two addressings. It SAVES the ten volatile-free
//   XMM registers RAX-relative and RESTORES them RSP-relative:
//
//     save     0x1801218bc  MOVAPS [RAX-0x58],XMM6      ...through XMM15 at -0xe8
//     frame    0x1801218a3  LEA RBP,[RAX-0x258]
//                           8 pushes (RSP = E-0x40) then SUB RSP,0x318
//                                                       (RSP = E-0x358)
//     restore  0x180122376  LEA R11,[RSP+0x318]         -> E-0x40
//              0x18012237e  MOVAPS XMM6,[R11-0x18]      -> E-0x58   MATCHES
//
//   The two agree EXACTLY, and only because RAX is the entry RSP. Apply the +8
//   and the saves move to E-0x50 while the restores still read E-0x58: all ten
//   XMM registers come back holding a stack word that was never theirs, and the
//   caller — `Engine::MainLoop`'s frame — keeps using them. The general shape
//   is "a frame that mixes RAX-relative and RSP-relative addressing cannot
//   tolerate a fix applied to only one of them"; `Update` had it on RBX, `Draw`
//   has it on the whole XMM save area.
//
//   Note what the correction does NOT break, because it is the reason the bug
//   is subtle: the epilogue's `MOV RSP,R11` / 8 POPs / RET are all RSP-derived
//   and stay correct, so the function returns cleanly to a caller whose
//   registers are silently wrong.
//
// So this subject uses `RuntimeDetourInstall`, the older byte-copy installer
// runtime_detour.h calls "deliberately dumb": it copies the prologue VERBATIM,
// inserts nothing, and refuses the patch on any byte mismatch.
//
// NOT A CLAIM ABOUT THE OTHER INSTALLER'S OTHER USERS. Lane FREEPRO is deriving
// the general rule; this file states only what it measured on 0x180121890.
//
// UNWIND CAVEAT, stated because it is real and not fixed here: the DLL's
// .pdata/.xdata still describe the original prologue and our wrapper's frame
// now sits between the caller and it. An exception unwinding THROUGH a detoured
// Runtime frame is undefined — which is why runtime_detour.h's rules keep C++
// exceptions out of detoured code, and why the ported body allocates nothing
// that can throw.
// ============================================================================

#include "runtime_detour.h"

#include <cstdio>
#include <cstdint>

namespace {

// 13, not 12 — see the banner. The boundaries here are 11 and 13.
constexpr size_t    kUiDrawPrologue = 13;
constexpr uintptr_t kUiDrawAddr     = 0x180121890;

// The safety belt. `RuntimeDetourInstall` refuses the patch unless these bytes
// are actually there, which catches a different Runtime build, a wrong ASLR
// delta, and — the one that matters most — another detour already sitting on
// this address. Taken from memory, not from a mnemonic listing.
const uint8_t kUiDrawBytes[kUiDrawPrologue] = {
    0x48, 0x8B, 0xC4,               // MOV RAX,RSP
    0x48, 0x89, 0x48, 0x08,         // MOV [RAX+0x8],RCX
    0x55,                           // PUSH RBP
    0x53,                           // PUSH RBX
    0x56,                           // PUSH RSI
    0x57,                           // PUSH RDI
    0x41, 0x54                      // PUSH R12
};

void* g_uiDrawTramp = nullptr;

} // namespace

extern "C" bool RtUiUserInterfaceDrawInstallDetour(void* ourFn) {
    if (g_uiDrawTramp) return true;
    if (!ourFn) return false;

    // NOTE THE UN-RESOLVED INPUT. `RuntimeDetourInstall` takes a GHIDRA address
    // and resolves it with `ResolveRuntimeRaw` on its own side of the boundary.
    // That is the fix for the defect that shipped thirteen times: a caller that
    // resolves through `ResolveAddr(Module::Runtime, …)` hands the installer OUR
    // OWN GCC prologue once the port registers at the same address, and the byte
    // guard then refuses in silence
    // (reference_a_rule_on_the_callers_side_gets_broken.md). This call site must
    // never learn to "help" by resolving first — and `RtUiDrawRegister` calls
    // `RegisterRuntimeAddr(0x180121890, …)` immediately before it, so the
    // temptation is live.
    g_uiDrawTramp = RuntimeDetourInstall(
        kUiDrawAddr, ourFn, static_cast<int>(kUiDrawPrologue),
        kUiDrawBytes, static_cast<int>(kUiDrawPrologue),
        "UI::UserInterface::Draw[port]");

    if (!g_uiDrawTramp) {
        std::fprintf(stderr,
            "[UiDetour] UserInterface::Draw detour FAILED -- the DLL is UNTOUCHED "
            "and MainLoop keeps calling its own Draw. This is a safe failure, not "
            "a silent one: nothing was patched.\n");
        std::fflush(stderr);
        return false;
    }

    std::fprintf(stderr,
        "[UiDetour] DETOUR INSTALLED at 0x180121890 (13-byte patch) -- "
        "Engine::MainLoop's frame now runs the ported UI::UserInterface::Draw.\n");
    std::fflush(stderr);
    return true;
}

// Retail's body, or nullptr when no detour is installed.
//
// Same contract as the other two: NULL means "no detour, so the raw export IS
// retail and is safe to call". NON-null means the raw export now jumps to us,
// so an oracle reaching for `RT_ORIGINAL_ONCE` instead of this does not get a
// wrong answer — it gets a stack overflow.
extern "C" void* RtUiUserInterfaceDrawTrampoline() { return g_uiDrawTramp; }
