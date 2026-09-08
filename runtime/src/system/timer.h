#pragma once
// ============================================================================
// runtime/src/system/timer.h — System::Timer's five data exports, owned by us
// ============================================================================
//
// Wave 9 lane B3. `timer.cpp` ports the whole class AND owns the storage
// behind its five private statics, so the four consumers in the tree that were
// reading the DLL's copies through the raw data door
// (`RT_ORIGINAL_ONCE(double*, "?_secondsPerTick@…")` and friends) can read ours
// instead. Those reads are lane V's DATA READ rows for
// `?_secondsPerTick@Timer@System@@0NA`
// (application_win.cpp:45, engine/engine.cpp:217, save/saveload.cpp:163) plus
// the three sibling slots application_win.cpp resolves alongside it.
//
// **This closes those rows rather than handing them to lane C2's mint.** A
// minted vtable stands in for a layout; these are five scalars whose values a
// ported `Timer::Startup`/`PlatformStartup` already produces, so the honest
// answer is that the port owns them and nothing needs minting. Two of the five
// are the compile-time constants 1000000 and 1e-6 (see timer.cpp's header for
// the disassembly that establishes that, and for the correction it makes to
// WAVE9_LANE_V.md §4); the other three are QPC-derived and are written by
// `RtSystem_Timer_PlatformStartup()`.
//
// Reading through these accessors is also strictly SAFER than the DLL slot:
// the DLL's `_secondsPerTick` is zero until `Engine::Startup` runs, and
// `save/saveload.cpp`'s helper divided by it. Ours is initialised at namespace
// scope and is never zero.
// ============================================================================

#include <cstdint>

namespace RtSystem {

// ?_ticksPerSecond@Timer@System@@0_JA        retail 0x1801fe720  (== 1000000)
// ?_secondsPerTick@Timer@System@@0NA         retail 0x1801fe728  (== 1e-6)
// ?_systemTicksPerSecond@Timer@System@@0_JA  retail 0x1801fe730  (QPF)
// ?_systemInitialTick@Timer@System@@0_JA     retail 0x1801fe738  (QPC at startup)
// ?_systemSecondsPerTick@Timer@System@@0NA   retail 0x1801fe740  (1/QPF)
//
// The three QPC-derived ones are zero until PlatformStartup has run;
// TimerEnsurePlatformStarted() makes that true and is idempotent.
int64_t TimerTicksPerSecond();
double  TimerSecondsPerTick();
int64_t TimerSystemTicksPerSecond();
int64_t TimerSystemInitialTick();
double  TimerSystemSecondsPerTick();

void TimerEnsurePlatformStarted();

}  // namespace RtSystem
