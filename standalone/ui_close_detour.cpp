// ============================================================================
// standalone/ui_close_detour.cpp — Wave 10 lane CLOSEDLG
// The detour for
//   ?CloseDialog@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z
//   0x180120f20
// ============================================================================
//
// Sibling of `standalone/ui_frame_detour.cpp` (lane UI1's `UserInterface::Update`)
// and `standalone/ui_detour.cpp` (`TabElement::UpdateContainers`). Read
// ui_detour.cpp's banner first — it explains why a UI MUTATOR needs a code patch
// rather than an export registration at all. A separate file only because those
// two belong to other lanes' subjects and two lanes editing one file is how a
// wave loses a merge.
//
// WHY THIS SUBJECT WANTS A DETOUR
//
// `CloseDialog` has many exe-side callers (30-odd `fn_UI_CloseDialog` sites in
// `code/Game/Dialog/`), all of which `RegisterRuntimeExport` already reaches.
// But it also has callers INSIDE the DLL that an export registration cannot
// touch — `UserInterface::Update`'s tooltip arm, `ComboElement::SetCurrentItem`,
// `~UserInterface`, and **CloseDialog itself**, which recurses through a direct
// `CALL 0x180120f20` at two sites. With the detour in, retail's own recursion
// re-enters the port, which is the correct composition rather than a hazard.
//
// ---------------------------------------------------------------------------
// THE PROLOGUE, AND WHY THE INSTALLER CHOICE IS EASY HERE
// ---------------------------------------------------------------------------
// Bytes read out of the shipped DLL with pefile, not off a mnemonic listing:
//
//   180120f20  48 89 54 24 10   MOV [RSP+0x10],RDX   ; the RDX home slot
//   180120f25  55               PUSH RBP
//   180120f26  56               PUSH RSI
//   180120f27  57               PUSH RDI
//   180120f28  48 83 EC 60      SUB RSP,0x60         = 12 bytes EXACTLY
//   180120f2c  48 C7 44 24 ...  MOV [RSP+0x20],-2    <- trampoline resumes here
//
// Twelve is both the patch minimum (MOV RAX,imm64 + JMP RAX) and an exact
// instruction boundary, and none of the twelve bytes is PC-relative.
//
// **There is no `MOV RAX,RSP` here**, so "Bug Class 3" — the `ADD RAX,8`
// rewrite that lane UI1 had to avoid on `Update` — does not arise at all. This
// file still uses `RuntimeDetourInstall`, the deliberately dumb byte-copy
// installer, because the twelve bytes are already position-independent and
// there is nothing for a relocating installer to do.
//
// THE HOME-SLOT WRITE IS FINE, and it is the one thing worth checking twice.
// `MOV [RSP+0x10],RDX` in the trampoline writes OUR wrapper's shadow space
// rather than the original caller's. The body then reads that same word back at
// 0x180121123 and 0x18012131d as `[RSP+0x88]`, which is `entry_RSP+0x10` after
// the three pushes and the 0x60 subtract. Since the trampoline's "entry RSP" is
// our wrapper's, the store and both loads move together and address the same
// word. The Microsoft x64 ABI guarantees the wrapper provides that 32-byte home
// space, so the store is legal. This is the same reasoning UI1 recorded for
// `Draw`'s `MOV [RAX+8],RCX`.
//
// UNWIND CAVEAT, stated because it is real and not fixed here: the DLL's
// .pdata/.xdata still describe the original prologue, and our wrapper's frame
// now sits between the caller and it. An exception unwinding THROUGH a detoured
// Runtime frame is undefined — which is why runtime_detour.h's rules say to keep
// C++ exceptions out of detoured code, and why the ported body allocates nothing
// that can throw. Note that retail's own prologue writes -2 into the EH state
// slot at [RSP+0x20] two bytes past our patch, i.e. the trampoline runs it.
// ============================================================================

#include "runtime_detour.h"

#include <cstdio>
#include <cstdint>

namespace {

constexpr size_t    kClosePrologue = 12;
constexpr uintptr_t kCloseAddr     = 0x180120f20;

// The safety belt. `RuntimeDetourInstall` refuses the patch unless these bytes
// are actually there, which catches a different Runtime build, a wrong ASLR
// delta, and — the one that matters most — another detour already sitting on
// this address.
const uint8_t kCloseBytes[kClosePrologue] = {
    0x48, 0x89, 0x54, 0x24, 0x10,   // MOV [RSP+0x10],RDX
    0x55,                           // PUSH RBP
    0x56,                           // PUSH RSI
    0x57,                           // PUSH RDI
    0x48, 0x83, 0xEC, 0x60          // SUB RSP,0x60
};

void* g_closeTramp = nullptr;

} // namespace

extern "C" bool RtUiCloseDialogInstallDetour(void* ourFn) {
    if (g_closeTramp) return true;
    if (!ourFn) return false;

    // NOTE THE UN-RESOLVED INPUT. `RuntimeDetourInstall` takes a GHIDRA address
    // and resolves it with `ResolveRuntimeRaw` on its own side of the boundary,
    // which is the fix for the defect that shipped thirteen times: a caller that
    // resolves through `ResolveAddr(Module::Runtime, …)` hands the installer OUR
    // OWN prologue once the port registers at the same address, and the byte
    // guard then refuses in silence
    // (reference_a_rule_on_the_callers_side_gets_broken.md). This call site must
    // never learn to "help" by resolving first.
    g_closeTramp = RuntimeDetourInstall(
        kCloseAddr, ourFn, static_cast<int>(kClosePrologue),
        kCloseBytes, static_cast<int>(kClosePrologue),
        "UI::UserInterface::CloseDialog[port]");

    if (!g_closeTramp) {
        std::fprintf(stderr,
            "[UiDetour] UserInterface::CloseDialog detour FAILED -- the DLL is "
            "UNTOUCHED and its internal callers keep their own body. This is a safe "
            "failure, not a silent one: nothing was patched.\n");
        std::fflush(stderr);
        return false;
    }

    std::fprintf(stderr,
        "[UiDetour] DETOUR INSTALLED at 0x180120f20 -- the DLL's own CloseDialog "
        "callers (Update's tooltip arm, ComboElement::SetCurrentItem, "
        "~UserInterface, and CloseDialog's own recursion) now run the port.\n");
    std::fflush(stderr);
    return true;
}

// Retail's body, or nullptr when no detour is installed.
//
// A NULL means "no detour, so the raw export IS retail and is safe to call". A
// NON-null means the raw export now jumps to us, so an oracle that reaches for
// `RT_ORIGINAL_ONCE` instead of this does not get a wrong answer — it gets a
// stack overflow.
extern "C" void* RtUiCloseDialogTrampoline() { return g_closeTramp; }
