// ============================================================================
// standalone/ui_frame_detour.cpp — Wave 10 lane UI1
// The detour for ?Update@UserInterface@UI@@QEAAHAEBVTime@System@@@Z 0x1801213c0
// ============================================================================
//
// Sibling of `standalone/ui_detour.cpp` (Wave 10's first UI subject,
// `TabElement::UpdateContainers`). Read that file's banner first — it explains
// why a UI MUTATOR needs a code patch rather than an export registration at
// all, and why `RT_ORIGINAL_ONCE` must never appear inside a detoured body.
// A separate file only because `ui_detour.cpp` belongs to another lane's
// subject and two lanes editing one file is how a wave loses a merge.
//
// WHY THIS SUBJECT NEEDS THE DETOUR MORE THAN THE FIRST ONE DID
//
// `UserInterface::Update` has essentially ONE caller that matters and it is
// inside the DLL: `Engine::MainLoop`'s frame. `RegisterRuntimeExport` cannot
// reach it — the DLL's internal call graph is direct rel32 CALLs that never
// consult the export table. Meanwhile Wave 9 landed `AddDialog`, `SetCursor`,
// `EnableCursor` and `BringDialogToFront` as ports, and ALL FOUR mutate the
// very object `Update` drains and walks. So the split brain Wave 7 lane C
// warned about ("the day someone ports Update, the DLL's MainLoop will still be
// calling the DLL's Update on an object our mutator reshaped") is not a future
// hazard — **it is the state of the tree today**, with our mutators writing an
// object the DLL's `Update` consumes. This detour is what closes it.
//
// ---------------------------------------------------------------------------
// WHY `RuntimeDetourInstall` AND NOT `InstallRuntimeDetourChecked`
// ---------------------------------------------------------------------------
// This is the load-bearing decision in this file, and it turns on ONE
// INSTRUCTION.
//
// The prologue opens with an RSP CAPTURE (bytes read out of the shipped DLL,
// not off a mnemonic listing — this compiler emits redundant REX prefixes):
//
//   1801213c0  48 8B C4        MOV RAX,RSP        <- captures the entry RSP
//   1801213c3  55              PUSH RBP
//   1801213c4  56              PUSH RSI
//   1801213c5  57              PUSH RDI
//   1801213c6  41 54           PUSH R12
//   1801213c8  41 55           PUSH R13
//   1801213ca  41 56           PUSH R14           = 12 bytes exactly
//   1801213cc  41 57           PUSH R15           <- trampoline resumes here
//   1801213ce  48 8D 68 A1     LEA RBP,[RAX-0x5f] <- RBP is DERIVED FROM RAX
//
// Twelve is both the patch minimum (MOV RAX,imm64 + JMP RAX) and an exact
// instruction boundary here, and none of those twelve bytes is PC-relative.
//
// `InstallDetour` — and so every `InstallRuntimeDetour*` front door — DETECTS
// that `MOV RAX,RSP` and rewrites the trampoline copy to
// `MOV RAX,RSP; ADD RAX,8` ("Bug Class 3", runtime_detour.cpp:62-94), on the
// reasoning that "the CALL into the trampoline shifts RSP by 8".
//
// **That correction is wrong for this function, and applying it would corrupt
// RBX on return.** Work it through rather than taking it on faith:
//
//   Retail's entry contract is RSP = E, return address at [E], the caller's
//   32-byte home space at [E+8]. When our wrapper CALLs the trampoline the
//   trampoline sees RSP = R, return address at [R], OUR home space at [R+8] —
//   the SAME contract with E := R. Nothing is shifted; R simply plays E's role.
//
//   Now apply the +8 and follow one register:
//     save    0x1801213e1  MOV [RAX+0x10],RBX   ->  R+0x18   (RAX was fixed up)
//     restore 0x180121867  MOV RBX,[RSP+0xf8]   ->  R+0x10   (RSP was not)
//
//   The save is RAX-relative and the restore is RSP-relative. "Correcting" RAX
//   alone desynchronises them by exactly 8, and RBX comes back holding a stack
//   word that was never RBX. A frame-pointer function that mixes both
//   addressings cannot tolerate a fix applied to only one of them.
//
// So this subject uses `RuntimeDetourInstall`, the older byte-copy installer
// runtime_detour.h calls "deliberately dumb": it copies the prologue VERBATIM,
// inserts nothing, and refuses the patch on any byte mismatch. Dumb is exactly
// right here — the twelve bytes are already position-independent, so there is
// nothing for a relocating installer to do except the one thing that breaks it.
//
// NOT A CLAIM ABOUT THE OTHER INSTALLER'S OTHER USERS. `rt_alloc_swap.cpp`
// detours `MemoryAllocator::Free` (0x1800c6c00), whose 12-byte prologue also
// ends in `MOV RAX,RSP`, through the correcting path. Whether the same argument
// applies there was NOT tested by this lane and is NOT asserted here; that
// detour is opt-in and belongs to lane ALLOC. It is written down in
// docs/runtime_port/WAVE10_LANE_UI1.md so the question gets asked instead of
// inherited.
//
// UNWIND CAVEAT, stated because it is real and not fixed here: the DLL's
// .pdata/.xdata still describe the original prologue, and our wrapper's frame
// now sits between the caller and it. An exception unwinding THROUGH a detoured
// Runtime frame is undefined — which is why runtime_detour.h's rules say to
// keep C++ exceptions out of detoured code, and why the ported body allocates
// nothing that can throw.
// ============================================================================

#include "runtime_detour.h"

#include <cstdio>
#include <cstdint>

namespace {

constexpr size_t    kUiUpdatePrologue = 12;
constexpr uintptr_t kUiUpdateAddr     = 0x1801213c0;

// The safety belt. `RuntimeDetourInstall` refuses the patch unless these bytes
// are actually there, which catches a different Runtime build, a wrong ASLR
// delta, and — the one that matters most — another detour already sitting on
// this address.
const uint8_t kUiUpdateBytes[kUiUpdatePrologue] = {
    0x48, 0x8B, 0xC4,   // MOV RAX,RSP
    0x55,               // PUSH RBP
    0x56,               // PUSH RSI
    0x57,               // PUSH RDI
    0x41, 0x54,         // PUSH R12
    0x41, 0x55,         // PUSH R13
    0x41, 0x56          // PUSH R14
};

void* g_uiUpdateTramp = nullptr;

} // namespace

extern "C" bool RtUiUserInterfaceUpdateInstallDetour(void* ourFn) {
    if (g_uiUpdateTramp) return true;
    if (!ourFn) return false;

    // NOTE THE UN-RESOLVED INPUT. `RuntimeDetourInstall` takes a GHIDRA address
    // and resolves it with `ResolveRuntimeRaw` on its own side of the boundary,
    // which is the fix for the defect that shipped thirteen times: a caller that
    // resolves through `ResolveAddr(Module::Runtime, …)` hands the installer OUR
    // OWN prologue once the port registers at the same address, and the byte
    // guard then refuses in silence (reference_a_rule_on_the_callers_side_gets_broken.md).
    // This call site must never learn to "help" by resolving first.
    g_uiUpdateTramp = RuntimeDetourInstall(
        kUiUpdateAddr, ourFn, static_cast<int>(kUiUpdatePrologue),
        kUiUpdateBytes, static_cast<int>(kUiUpdatePrologue),
        "UI::UserInterface::Update[port]");

    if (!g_uiUpdateTramp) {
        std::fprintf(stderr,
            "[UiDetour] UserInterface::Update detour FAILED -- the DLL is UNTOUCHED "
            "and MainLoop keeps calling its own Update. This is a safe failure, not "
            "a silent one: nothing was patched.\n");
        std::fflush(stderr);
        return false;
    }

    std::fprintf(stderr,
        "[UiDetour] DETOUR INSTALLED at 0x1801213c0 -- Engine::MainLoop's frame now "
        "runs the ported UI::UserInterface::Update. The split brain with our "
        "AddDialog / SetCursor / EnableCursor / BringDialogToFront ports is closed.\n");
    std::fflush(stderr);
    return true;
}

// Retail's body, or nullptr when no detour is installed.
//
// Same contract as `RtUiUpdateContainersTrampoline`: a NULL means "no detour, so
// the raw export IS retail and is safe to call". A NON-null means the raw export
// now jumps to us, so an oracle that reaches for `RT_ORIGINAL_ONCE` instead of
// this does not get a wrong answer — it gets a stack overflow.
extern "C" void* RtUiUserInterfaceUpdateTrampoline() { return g_uiUpdateTramp; }
