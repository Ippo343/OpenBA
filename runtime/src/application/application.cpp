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
#include "../../funcs/rt_port.h"
#include "../../types/application/Application.h"

using RtApplication::Application;

// ============================================================================
// Application::Application::IsPaused() const   retail 0x18009f4e0   ord 3150
// ?IsPaused@Application@1@QEAA_NXZ
// ----------------------------------------------------------------------------
// `return this[0x11] == 0`. A plain byte read, no state mutated.
// Oracle: FULL DIFF.
// ============================================================================
bool RtApp_IsPaused(const Application* self) {
    RT_DIFF_BEGIN("Application::Application::IsPaused");
    const bool ours = (self->_running == 0);
    if (rt_diff_on__) {
        using Fn = bool (*)(const Application*);
        const bool theirs = RT_ORIGINAL_ONCE(Fn, "?IsPaused@Application@1@QEAA_NXZ")(self);
        RT_DIFF_SCALAR(ours, theirs);
    }
    return ours;
}

// ============================================================================
// Application::Application::Quit()              retail 0x18009f4d0   ord 3437
// ?Quit@Application@1@QEAAXXZ
// ----------------------------------------------------------------------------
// `this[0x28] = 1`. One byte store.
//
// ORACLE: SCRATCH-COPY DIFF, restricted to the first sizeof(Application)
// (0x29) bytes of the live object rather than the whole object. That is
// deliberate, not a shortcut: this lane does not port the constructor and so
// does not know the class's true extent — only that it is AT LEAST 0x29
// bytes, because Quit itself (and the ctor, per the header comment) touches
// +0x28 on the real, live object every time it runs. Copying exactly the
// prefix this lane's own struct describes is therefore provably in-bounds; a
// full-object copy would not be, since no function this lane ported ever
// proved a larger size.
// ============================================================================
void RtApp_Quit(Application* self) {
    RT_DIFF_BEGIN("Application::Application::Quit");
    if (rt_diff_on__) {
        using Fn = void (*)(Application*);
        alignas(8) uint8_t ours[sizeof(Application)];
        alignas(8) uint8_t theirs[sizeof(Application)];
        std::memcpy(ours,   self, sizeof(Application));
        std::memcpy(theirs, self, sizeof(Application));
        reinterpret_cast<Application*>(ours)->_quitRequested = 1;
        RT_ORIGINAL_ONCE(Fn, "?Quit@Application@1@QEAAXXZ")(reinterpret_cast<Application*>(theirs));
        RT_DIFF_BYTES(ours, theirs, sizeof(Application));
    }
    self->_quitRequested = 1;
}
