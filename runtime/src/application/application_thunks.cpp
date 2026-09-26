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
// PlatformShutdown (0x18009fd70) landed with Wave 12 lane B
// (platform_startup_win.cpp:556). Lane A had to leave a TEMPORARY SEAM here
// that forwarded through the raw DLL door into the shipped DLL body, because
// lane B's row was not in lane A's worktree. THE INTEGRATOR RE-POINTED IT at
// the merge: Shutdown() now calls our own RtApp_PlatformShutdown, exactly as
// Update/Startup call their siblings.
//
// This mattered. Leaving the seam in place would have shipped lane B's
// PlatformShutdown REGISTERED BUT BYPASSED on this path -- a port that exists,
// reports as ported, and never runs, which is one of the failure modes this
// project has already paid for.
// ============================================================================

#include "../../funcs/rt_port.h"
#include "../../types/application/Application.h"

// ENDGAME B.3.7: `Application::Application` is the real class; a global
// `using RtApplication::Application;` is ill-formed next to the namespace.
using App = Application::Application;

// application_win.cpp / platform_startup_win.cpp — already-ported siblings.
// PlatformUpdate has ordinary C++ linkage (application_win.cpp:79); only
// PlatformStartup is declared extern "C" at its definition
// (platform_startup_win.cpp:317) — match each exactly or the linker cannot
// find them.
bool RtApp_PlatformUpdate(App* self);
extern "C" bool RtApp_PlatformStartup(App* self);
extern "C" void RtApp_PlatformShutdown(App* self);

// ============================================================================
// Application::Application::Update()    retail 0x18009f4f0   jmp -> PlatformUpdate
// ?Update@Application@1@QEAA_NXZ
// ----------------------------------------------------------------------------
// The tail call's own callee (PlatformUpdate) carries its own RT_DIFF site
// and, per its file banner, has NO oracle (a second call would pump a second
// message loop) — so this wrapper does not call the DLL a second time; it
// records that the delegation happened and forwards.
// ============================================================================
bool Application::Application::Update() {
    return PlatformUpdate();
}

bool RtApp_Update(App* self) {
    return self->Application::Update();
}

// ============================================================================
// Application::Application::Shutdown()   retail 0x18009f500   jmp -> PlatformShutdown
// ?Shutdown@Application@1@QEAAXXZ
// ----------------------------------------------------------------------------
// Same shape as Update: PlatformShutdown (platform_startup_win.cpp:556)
// carries its own site, so this wrapper forwards without diffing again. The
// the diff oracle below is an ATTENDANCE MARKER, not a comparison -- it reports
// equal=true without ever consulting the DLL, so its `divergences=0` proves
// nothing and only its `calls=` is meaningful.
// ============================================================================
void Application::Application::Shutdown() {
    PlatformShutdown();
}

void RtApp_Shutdown(App* self) {
    self->Application::Shutdown();
}

// ============================================================================
// Application::Application::Startup()    retail 0x18009f510   jmp -> PlatformStartup
// ?Startup@Application@1@QEAA_NXZ
// ----------------------------------------------------------------------------
// Same shape as Update: PlatformStartup carries its own RT_DIFF site
// (platform_startup_win.cpp), so this wrapper forwards without diffing again.
// ============================================================================
bool Application::Application::Startup() {
    return PlatformStartup();
}

bool RtApp_Startup(App* self) {
    return self->Application::Startup();
}
