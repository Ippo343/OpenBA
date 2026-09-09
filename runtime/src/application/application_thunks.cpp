// ============================================================================
// runtime/src/application/application_thunks.cpp — Wave 12 lane A
//   Application::Application::Update / Shutdown / Startup
// ============================================================================
// Three one-instruction tail-call thunks — each is a single `jmp` to its
// `Platform*` sibling, verified against `<SCRATCH>/app_disasm_main.txt`:
//
//   ?Update@Application@1@QEAA_NXZ    retail 0x18009f4f0   jmp 0x18009fdf0 (PlatformUpdate)
//   ?Shutdown@Application@1@QEAAXXZ   retail 0x18009f500   jmp 0x18009fd70 (PlatformShutdown)
//   ?Startup@Application@1@QEAA_NXZ   retail 0x18009f510   jmp 0x18009fc10 (PlatformStartup)
//
// A `jmp` (not `call`) is a true tail call: RCX (`this`) is untouched and the
// return value is whatever the callee returns, so each port here just forwards
// the call and its result.
//
// PlatformUpdate (application_win.cpp) and PlatformStartup
// (platform_startup_win.cpp) are already ported in this tree — this file
// forwards to THEM, not to the DLL, so calling Update()/Startup() runs our own
// body exactly once.
//
// PlatformShutdown (0x18009fd70) is lane B's function and is NOT present in
// this worktree. TEMPORARY SEAM: Shutdown() below forwards through
// RT_ORIGINAL_ONCE to the shipped DLL body instead. THE INTEGRATOR MUST
// RE-POINT THIS to lane B's RtApp_PlatformShutdown once it lands (declare it
// extern and swap the RT_ORIGINAL_ONCE call for a direct call, matching how
// Update/Startup call their siblings below).
// ============================================================================

#include "../../funcs/rt_port.h"
#include "../../types/application/Application.h"

using RtApplication::Application;

// application_win.cpp / platform_startup_win.cpp — already-ported siblings.
// PlatformUpdate has ordinary C++ linkage (application_win.cpp:79); only
// PlatformStartup is declared extern "C" at its definition
// (platform_startup_win.cpp:317) — match each exactly or the linker cannot
// find them.
bool RtApp_PlatformUpdate(Application* self);
extern "C" bool RtApp_PlatformStartup(Application* self);

// ============================================================================
// Application::Application::Update()    retail 0x18009f4f0   jmp -> PlatformUpdate
// ?Update@Application@1@QEAA_NXZ
// ----------------------------------------------------------------------------
// The tail call's own callee (PlatformUpdate) carries its own RT_DIFF site
// and, per its file banner, has NO oracle (a second call would pump a second
// message loop) — so this wrapper does not call the DLL a second time; it
// records that the delegation happened and forwards.
// ============================================================================
bool RtApp_Update(Application* self) {
    RT_DIFF_BEGIN("Application::Application::Update [tail-call thunk -> PlatformUpdate]");
    RT_DIFF_SHADOW(true, "delegated to PlatformUpdate (diffed at its own site)",
                    "not independently diffable: PlatformUpdate has no oracle");
    return RtApp_PlatformUpdate(self);
}

// ============================================================================
// Application::Application::Shutdown()   retail 0x18009f500   jmp -> PlatformShutdown
// ?Shutdown@Application@1@QEAAXXZ
// ----------------------------------------------------------------------------
// TEMPORARY SEAM — see file banner. PlatformShutdown is not yet ported in
// this tree, so this forwards to the shipped DLL body via RT_ORIGINAL_ONCE.
// That means this thunk always IS the retail body; there is nothing of ours
// to diff against it.
// ============================================================================
void RtApp_Shutdown(Application* self) {
    RT_DIFF_BEGIN("Application::Application::Shutdown [tail-call thunk -> PlatformShutdown, TEMPORARY SEAM]");
    RT_DIFF_SHADOW(true, "delegated to the shipped PlatformShutdown (lane B body not yet in this tree)",
                    "not diffable: this call IS the shipped body");
    using Fn = void (*)(Application*);
    RT_ORIGINAL_ONCE(Fn, "?PlatformShutdown@Application@1@AEAAXXZ")(self);
}

// ============================================================================
// Application::Application::Startup()    retail 0x18009f510   jmp -> PlatformStartup
// ?Startup@Application@1@QEAA_NXZ
// ----------------------------------------------------------------------------
// Same shape as Update: PlatformStartup carries its own RT_DIFF site
// (platform_startup_win.cpp), so this wrapper forwards without diffing again.
// ============================================================================
bool RtApp_Startup(Application* self) {
    RT_DIFF_BEGIN("Application::Application::Startup [tail-call thunk -> PlatformStartup]");
    RT_DIFF_SHADOW(true, "delegated to PlatformStartup (diffed at its own site)",
                    "not independently diffable");
    return RtApp_PlatformStartup(self);
}
