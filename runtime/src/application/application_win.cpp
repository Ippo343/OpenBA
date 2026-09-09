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

// Wave 12 lane B's synthetic-subject probe (application_lifecycle_probe.cpp).
// A no-op unless BANISHED_RT_APPB_PROBE armed it.
extern "C" void RtApp_LifecycleProbeMaybeRun();

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

    // Wave 12 lane B's lifecycle probe, self-scheduled here for the same reason
    // set_avl_probe.cpp self-schedules on the first RegisterMessageCallback: it
    // ALLOCATES, and at RuntimePortInit() time System::MemoryAllocator has been
    // constructed but not started. The first frame is provably past that. A
    // single load-and-branch when the probe is not armed, which is every run
    // that has not set BANISHED_RT_APPB_PROBE.
    RtApp_LifecycleProbeMaybeRun();

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

// ============================================================================
// Application::Application::PlatformConstruct(void* platformHandle)
// ?PlatformConstruct@Application@1@AEAAXPEAX@Z  retail 0x18009fbe0   ord 3331
// ============================================================================
// WAVE 12 LANE B. Four instructions, and the only body in this file that makes
// no Win32 call at all — it is here because it is the `Platform*` seam by
// retail's own architecture (a `_posix` twin would have a different one), not
// because it needs <windows.h>.
//
//   18009fbe0: MOV  QWORD PTR [RCX],0x0        ; _windowHandle   = null
//   18009fbe7: MOV  QWORD PTR [RCX+0x8],RDX    ; _platformHandle = arg
//   18009fbeb: MOV  WORD  PTR [RCX+0x10],0x100 ; ONE 16-bit store: +0x10 = 0x00,
//                                              ;                   +0x11 = 0x01
//   18009fbf1: RET
//
// IT IS THE CONSTRUCTOR'S TAIL, LIFTED. `??0Application@0@QEAA@PEAX@Z`
// (0x18009f590, ported in runtime/src/engine/singleton_ctors.cpp) ends with
// exactly these three stores in exactly this order — MSVC inlined the call.
// That is why `calls=` for this site is zero on every run today: nothing in our
// tree calls it, because retail's own only caller inlined it. The probe in
// application_lifecycle_probe.cpp is what executes it, and unlike the other
// four rows of this lane it gets a FULL BYTE-DIFF oracle, because the body is
// three stores and no side effects.
//
// The 16-bit store is reproduced as a 16-bit store rather than as two byte
// assignments. Single-threaded the two are indistinguishable, but this object
// is read from the window procedure, which USER32 can re-enter, and "the two
// bytes become visible together" is a property of the instruction retail chose.
// `std::memcpy` rather than a cast-and-assign: `_stateLow` and `_running` are
// two distinct `uint8_t` members and writing one `uint16_t` across them through
// a reinterpret_cast is undefined behaviour GCC is entitled to reorder.
// ============================================================================
extern "C" void RtApp_PlatformConstruct(Application* self, void* platformHandle) {
    RT_DIFF_BEGIN("Application::Application::PlatformConstruct");

    if (rt_diff_on__) {
        // A real scratch-copy diff: three stores, no side effects, no
        // allocation. Both scratches are pre-filled with 0xCD so a byte NEITHER
        // side writes (+0x12..+0x2f) stays 0xCD in both and compares equal —
        // the symmetric fill singleton_ctors.cpp uses, and the inverse of the
        // Wave 6 trap (reference_oracle_must_not_compare_unwritten_bytes.md).
        using Fn = void (*)(Application*, void*);
        alignas(8) uint8_t ours[sizeof(Application)];
        alignas(8) uint8_t theirs[sizeof(Application)];
        std::memset(ours,   0xCD, sizeof(ours));
        std::memset(theirs, 0xCD, sizeof(theirs));

        Application* o = reinterpret_cast<Application*>(ours);
        o->_windowHandle   = nullptr;
        o->_platformHandle = platformHandle;
        const uint16_t stateWord = 0x0100;
        std::memcpy(&o->_stateLow, &stateWord, sizeof(stateWord));

        RT_ORIGINAL_ONCE(Fn, "?PlatformConstruct@Application@1@AEAAXPEAX@Z")(
            reinterpret_cast<Application*>(theirs), platformHandle);
        RT_DIFF_BYTES(ours, theirs, sizeof(Application));
        RT_DIFF_WORK(true);
    }

    self->_windowHandle   = nullptr;                 // 0x18009fbe0
    self->_platformHandle = platformHandle;          // 0x18009fbe7
    const uint16_t stateWord = 0x0100;               // 0x18009fbeb
    std::memcpy(&self->_stateLow, &stateWord, sizeof(stateWord));
}
