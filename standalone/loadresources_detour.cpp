// ============================================================================
// standalone/loadresources_detour.cpp — Wave 23 lane ENGINE
// The detour for ?LoadResources@UpdateLoop@Engine@@QEAAXXZ  0x1800dd7d0
// ============================================================================
//
// WHY A CODE PATCH AND NOT JUST A REGISTRATION. Lane ENGINE ported and
// registered `UpdateLoop::LoadResources` under both its mangled name and its
// address, ran the game, and the effectiveness counter said
//
//     [RtPort]   Engine::UpdateLoop::LoadResources: calls=0 ...
//
// calls=0 on a body that unquestionably executes every game load. That is not a
// mystery, it is this project's oldest structural fact stated about a BODY
// rather than a vtable row: WE INTERCEPT ONLY AT DOORS. The evidence, taken
// before writing this file rather than after:
//
//   * `fn_UpdateLoop_LoadResources` (engine/funcs/engine_funcs.cpp:1705) is
//     RESOLVED and then called from NOWHERE — the only three occurrences in the
//     whole tree are the typedef, the resolve, and a printf of its value. Our
//     side has zero callers, so `RegisterRuntimeExport` had nothing to redirect.
//   * The one live caller is the DLL's own `?Run@Loader@Engine@@UEAAXXZ`
//     (0x1800dbfc0), running on the LOADER THREAD, reaching LoadResources by an
//     in-DLL rel32 CALL that never consults the export table.
//     `standalone/loader_loop_stubs.cpp:43` already documents that chain
//     ("ThreadFunc -> Loader::Run -> UpdateLoop::LoadResources").
//   * `?Run@Loader@Engine@@` is not merely unported — it is not in the census
//     `called` set at all, i.e. our tree never calls it either. Its vptr comes
//     from `??_7Loader@Engine@@6B@`, stamped into the DLL's own `_loader`
//     static by the DLL's CRT initialiser, which is not a door either.
//
// So the registration is correct and INERT on the DLL-present route, and a
// detour is the only mechanism that gives the body a caller. This is exactly
// the case `runtime/src/ui/userinterface_draw.cpp` faced for MainLoop's frame
// calls; this file is its sibling and copies its installer choices verbatim.
//
// ---------------------------------------------------------------------------
// OPT-IN, NOT DEFAULT-ON, AND WHY THAT IS NOT TIMIDITY
// ---------------------------------------------------------------------------
// `BANISHED_RT_ENGINELOADRES_DETOUR=1`. CLAUDE.md's rule is that an unproven
// feature starts opt-in, and the stakes here are specific rather than generic:
// `standalone/loader_loop_stubs.cpp` records that SKIPPING this load "crashes
// boot 6/6", because the menu scene depends on the Game/Global resource tree it
// pulls in. A defective port on this path therefore does not degrade, it fails
// the boot. Default-on would hand that risk to every other lane's run.
//
// ---------------------------------------------------------------------------
// A CLAIM THIS FILE MADE AND THEN REFUTED WITH ITS OWN CONTROL
// ---------------------------------------------------------------------------
// This banner originally said: "because boot depends on the load, A GREEN BOOT
// WITH THE DETOUR ON IS A REAL FUNCTIONAL TEST of the port." THAT IS FALSE, and
// lane ENGINE proved it false rather than shipping it.
//
// The control (`BANISHED_LOADRES_NEGCONTROL=1`, still in the body so it can be
// re-run) forces the root-resource block to be skipped — exactly the value the
// port produces and stores into `UpdateLoop+0x08`. Measured, both runs detoured:
//
//     detour on, control off :  calls=6  ownList=3   world built, 24 citizens, exit 0
//     detour on, control ON  :  calls=6  ownList=0   world built, 24 citizens, exit 0
//
// NOT CAUGHT. The control is well aimed — it corrupts something the port really
// computes — so this is not the "control corrupted an unobservable value" trap;
// it is the probe that is blind. `loader_loop_stubs.cpp`'s "skipping it crashes
// boot 6/6" is about skipping `Engine::Loader::Load` ENTIRELY, which is a much
// bigger deletion than nulling three resource lists, and reading it as coverage
// of this subject was the error.
//
// So what a green detoured boot DOES establish: the body runs six times on the
// real loader thread, does not crash, does not corrupt the heap or the String
// refcounts badly enough to fail a full world build, and returns. What it does
// NOT establish: that its OUTPUTS equal retail's. There is no oracle here yet.
// See docs/runtime_port/WAVE23_LANE_ENGINE.md §2.4 and §5.
//
// ---------------------------------------------------------------------------
// THE PATCH IS 13 BYTES, NOT 12 — READ THIS BEFORE CHANGING IT
// ---------------------------------------------------------------------------
// The prologue bytes were read out of the shipped DLL with pefile
// (`pe.get_data(0xdd7d0, 16)`), NOT copied from a Ghidra mnemonic listing:
//
//     40 55          PUSH RBP     <- note the REX.B-less 0x40 prefix
//     53             PUSH RBX
//     56             PUSH RSI
//     57             PUSH RDI
//     41 54          PUSH R12
//     41 55          PUSH R13
//     41 56          PUSH R14
//     41 57          PUSH R15
//     48 8B EC       MOV  RBP,RSP
//
// The instruction boundaries in this range are 11 and 13. A 12-byte splice —
// the installer's DEFAULT — would end between the 0x41 and the 0x57 of
// `PUSH R15`, and runtime_detour.h says plainly that the byte-compare guard is
// SILENT on boundary errors: those bytes ARE what is at that address, so the
// memcmp passes and the trampoline is garbage. 13 is the first legal length at
// or above the 12 the patch needs.
//
// The trampoline is never executed by us: our detour replaces the whole
// function and returns, it does not tail-jump into the original. It is kept
// because RuntimeDetourInstall owns it and RemoveDetour needs it.
// ============================================================================

#include "runtime_detour.h"

#include <cstdio>
#include <cstdint>
#include <cstdlib>

namespace {

constexpr size_t    kLoadResPrologue = 13;
constexpr uintptr_t kLoadResAddr     = 0x1800dd7d0;

const uint8_t kLoadResBytes[kLoadResPrologue] = {
    0x40, 0x55,                     // PUSH RBP
    0x53,                           // PUSH RBX
    0x56,                           // PUSH RSI
    0x57,                           // PUSH RDI
    0x41, 0x54,                     // PUSH R12
    0x41, 0x55,                     // PUSH R13
    0x41, 0x56,                     // PUSH R14
    0x41, 0x57                      // PUSH R15
};

void* g_loadResTramp = nullptr;

}  // namespace

extern "C" bool RtEngineLoadResourcesDetourOptedIn() {
    const char* e = std::getenv("BANISHED_RT_ENGINELOADRES_DETOUR");
    return e && *e && e[0] != '0';
}

extern "C" bool RtEngineLoadResourcesInstallDetour(void* ourFn) {
    if (g_loadResTramp) return true;
    if (!ourFn) return false;

    // UNRESOLVED input on purpose. RuntimeDetourInstall takes a GHIDRA address
    // and resolves it with ResolveRuntimeRaw on its own side. A caller that
    // "helpfully" resolved through ResolveAddr(Module::Runtime, ...) would hand
    // the installer OUR OWN GCC prologue the moment the port registers at the
    // same address — and RtEngineLoadResourcesRegister calls
    // RegisterRuntimeAddr(0x1800dd7d0, ...) immediately before this, so the
    // temptation is live here exactly as it was in ui_draw_detour.cpp.
    g_loadResTramp = RuntimeDetourInstall(
        kLoadResAddr, ourFn, static_cast<int>(kLoadResPrologue),
        kLoadResBytes, static_cast<int>(kLoadResPrologue),
        "Engine::UpdateLoop::LoadResources[port]");

    if (!g_loadResTramp) {
        std::fprintf(stderr,
            "[LoadResDetour] LoadResources detour FAILED -- the DLL is UNTOUCHED "
            "and Loader::Run keeps calling its own body. Safe failure, not a "
            "silent one: nothing was patched, and the port stays inert.\n");
        return false;
    }
    std::fprintf(stderr,
        "[LoadResDetour] armed on 0x1800dd7d0 -- the DLL's Loader::Run now "
        "reaches our port. A green boot shows the body RUNS and does not crash; "
        "it does NOT show its outputs match retail -- the negative control "
        "BANISHED_LOADRES_NEGCONTROL=1 booted green with ownList=0.\n");
    return true;
}
