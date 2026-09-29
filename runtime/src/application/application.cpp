// ============================================================================
// runtime/src/application/application.cpp — Wave 8 lane E — Application::Application
// ============================================================================
// The two members of this class that do NOT touch Win32:
// IsPaused / Quit. PlatformUpdate, SleepThread and YieldToThreads all call
// Win32 APIs directly (PeekMessageW/QueryPerformanceCounter/SwitchToThread)
// and live in application_win.cpp behind the PLAN.md §4.1 decision 1
// platform seam — <windows.h> must never appear here.
//
//   ?IsPaused@Application@1@QEAA_NXZ    retail 0x18009f4e0   ordinal 3150
//   ?Quit@Application@1@QEAAXXZ         retail 0x18009f4d0   ordinal 3437
//
// See runtime/types/application/Application.h for the layout and the
// vtable verdict (not polymorphic, proven from the ctor's first store).
//
// `Application::Application` (the class) is NOT in this lane's function list
// and is not ported: the wave brief names only the six functions in
// docs/runtime_port/wave8_lanes.txt, and the ctor is not one of them. This
// file only reads/writes the two bytes IsPaused and Quit touch.
// ============================================================================

#include <cstdint>
#include <cstring>
#include "../../types/application/Application.h"

// ENDGAME B.3.7: `Application::Application` is the real class; a global
// `using RtApplication::Application;` is ill-formed next to the namespace.
using App = Application::Application;

// ============================================================================
// Application::Application::IsPaused() const   retail 0x18009f4e0   ord 3150
// ?IsPaused@Application@1@QEAA_NXZ
// ----------------------------------------------------------------------------
// `return this[0x11] == 0`. A plain byte read, no state mutated.
// Oracle: FULL DIFF.
// ============================================================================
bool Application::Application::IsPaused() {
    const bool ours = (_running == 0);
    return ours;
}

// ============================================================================
// Application::Application::Quit()              retail 0x18009f4d0   ord 3437
// ?Quit@Application@1@QEAAXXZ
// ----------------------------------------------------------------------------
// `this[0x28] = 1`. One byte store.
//
// ORACLE: SCRATCH-COPY DIFF over sizeof(Application).
//
// CORRECTED BY WAVE 12 LANE B (PLAN §6 step 8). This comment used to say the
// copy was "restricted to the first 0x29 bytes ... this lane does not port the
// constructor and so does not know the class's true extent — only that it is AT
// LEAST 0x29 bytes". That caveat is now stale in both halves: the extent IS
// known — 0x30, from retail's own allocation at Engine::Startup 0x1800dbbb3
// (`MemAlloc(0x30, 4)`), asserted in the header — and the constructor IS ported
// (runtime/src/engine/singleton_ctors.cpp:576). Nothing about the code below
// changes: `sizeof(Application)` was already 0x30 to the compiler, because the
// struct's alignment is 8, so the copy was always a full-object copy and always
// in bounds. Only the reasoning was wrong.
// ============================================================================
void Application::Application::Quit() {
    _quitRequested = 1;
}
