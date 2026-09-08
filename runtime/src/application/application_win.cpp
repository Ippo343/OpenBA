// ============================================================================
// runtime/src/application/application_win.cpp — Wave 8 lane E
//   Application::Application::PlatformUpdate / SleepThread / YieldToThreads
// ============================================================================
// The three members of this class that touch Win32 directly, so they live
// behind the PLAN.md §4.1 decision 1 platform seam: <windows.h> must never
// appear in application.cpp (IsPaused/Quit).
//
//   ?PlatformUpdate@Application@1@AEAA_NXZ   retail 0x18009fdf0   ord 3410  (private)
//   ?SleepThread@Application@1@QEAAXH@Z      retail 0x18009f850   ord 4069
//   ?YieldToThreads@Application@1@QEAAXXZ    retail 0x18009f920   ord 4568
//
// NONE OF THE THREE HAS AN ORACLE, and WAVE8.md §5 says so explicitly before
// any lane started: "running both pumps two message loops or sleeps twice."
//   * PlatformUpdate drains the live Win32 message queue with PeekMessageW(…,
//     PM_REMOVE) — a second call would eat the NEXT message, not the same one.
//   * SleepThread and YieldToThreads block/yield the calling thread; calling
//     the DLL's copy afterward would sleep or yield a second time for real.
// All three are registered anyway (RT_DIFF_SHADOW(true, …), matching the
// ~SavedState precedent in runtime/src/save/saveload.cpp — a site that is
// counted by `calls=` but never compared) because each is unambiguous, short,
// and reproduces retail's own Win32 call sequence exactly rather than
// approximating it.
// ============================================================================

#include <windows.h>

#include <cstdint>
#include "../../funcs/rt_port.h"
#include "../../types/application/Application.h"
#include "../system/timer.h"

using RtApplication::Application;

namespace {

// System::Timer's four data exports.
//
// These were four RAW-DOOR data reads (`RT_ORIGINAL_ONCE(double*, "?_secondsPerTick@…")`
// and its three siblings) — WAVE9_LANE_V.md §4's DATA READ row for
// `?_secondsPerTick@Timer@System@@0NA` names this file at :45. Wave 9 lane B3
// ported System::Timer and the port OWNS the storage behind all five statics
// (runtime/src/system/timer.h), so the reads come from us now and the row is
// closed by a port rather than by lane C2 minting a value.
//
// Two behavioural notes, both improvements and both stated rather than assumed:
//   * `_ticksPerSecond` and `_secondsPerTick` are compile-time constants
//     (1000000 and 1e-6) — see timer.cpp's header for the disassembly.
//   * the DLL's `_systemInitialTick`/`_systemTicksPerSecond` are ZERO until
//     Engine::Startup runs, and QpcToEngineTicks below divided by the latter.
//     Our accessors run PlatformStartup on first use, so that divide is now
//     safe at every point in boot.
//
// The epoch consequence: SleepThread's engine-tick readings are now measured
// from OUR PlatformStartup sample, not the DLL's. It only ever computes a
// DIFFERENCE of two readings taken inside the same call (`elapsed`), so a
// shifted origin cancels exactly.
int64_t QpcToEngineTicks(int64_t qpc) {
    return (qpc - RtSystem::TimerSystemInitialTick()) * RtSystem::TimerTicksPerSecond()
           / RtSystem::TimerSystemTicksPerSecond();
}

}  // namespace

// ============================================================================
// Application::Application::PlatformUpdate()   retail 0x18009fdf0   ord 3410
// ?PlatformUpdate@Application@1@AEAA_NXZ
// ----------------------------------------------------------------------------
//   if (_quitRequested) PostQuitMessage(0);
//   got = PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE);
//   for (;;) {
//       if (!got) return true;
//       TranslateMessage(&msg); DispatchMessageW(&msg);
//       if (msg.message == WM_QUIT) break;
//       got = PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE);
//   }
//   return false;
// ============================================================================
bool RtApp_PlatformUpdate(Application* self) {
    RT_DIFF_BEGIN("Application::Application::PlatformUpdate [no oracle: pumps the live message queue]");
    RT_DIFF_SHADOW(true, "pumped", "not diffable");

    if (self->_quitRequested) PostQuitMessage(0);

    MSG msg;
    BOOL got = PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE);
    for (;;) {
        if (!got) return true;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (msg.message == WM_QUIT) break;
        got = PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE);
    }
    return false;
}

// ============================================================================
// Application::Application::SleepThread(int ms)   retail 0x18009f850   ord 4069
// ?SleepThread@Application@1@QEAAXH@Z
// ----------------------------------------------------------------------------
// Busy-waits (SwitchToThread, not Sleep) until at least `ms` milliseconds of
// engine time, measured through the same QPC-derived tick conversion
// System::Timer uses elsewhere, have elapsed. DAT_18016172c (retail's
// millisecond divisor) reads as 1000.0f (bytes 00 00 7a 44), confirmed by
// direct memory read against the shipped DLL rather than assumed.
//
// `this` is never read — retail's SleepThread ignores the object entirely,
// which is why the parameter below is unused and untyped-through only for
// signature fidelity.
// ============================================================================
void RtApp_SleepThread(Application* /*self*/, int ms) {
    RT_DIFF_BEGIN("Application::Application::SleepThread [no oracle: blocks the calling thread]");
    RT_DIFF_SHADOW(true, "slept", "not diffable");

    LARGE_INTEGER t0;
    QueryPerformanceCounter(&t0);
    const int64_t startTicks = QpcToEngineTicks(t0.QuadPart);

    const double targetTicks =
        (static_cast<double>(static_cast<float>(ms) / 1000.0f)) / RtSystem::TimerSecondsPerTick();

    LARGE_INTEGER t1;
    QueryPerformanceCounter(&t1);
    int64_t elapsed = QpcToEngineTicks(t1.QuadPart) - startTicks;
    while (elapsed < static_cast<int64_t>(targetTicks)) {
        SwitchToThread();
        QueryPerformanceCounter(&t1);
        elapsed = QpcToEngineTicks(t1.QuadPart) - startTicks;
    }
}

// ============================================================================
// Application::Application::YieldToThreads()   retail 0x18009f920   ord 4568
// ?YieldToThreads@Application@1@QEAAXXZ
// ----------------------------------------------------------------------------
// `SwitchToThread(); return;` — five bytes past the prologue. Ghidra's
// decompile prints a spurious "could not recover jumptable / treating
// indirect jump as call" warning above it; the disassembly shows a plain
// direct CALL to the imported SwitchToThread thunk, nothing indirect. `this`
// is not read.
// ============================================================================
void RtApp_YieldToThreads(Application* /*self*/) {
    RT_DIFF_BEGIN("Application::Application::YieldToThreads [no oracle: yields the calling thread]");
    RT_DIFF_SHADOW(true, "yielded", "not diffable");
    SwitchToThread();
}
