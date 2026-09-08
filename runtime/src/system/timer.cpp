// ============================================================================
// runtime/src/system/timer.cpp — System::Timer (Wave 9, lane B3)
// ============================================================================
//
// The whole class, nine exports, byte-faithful against the disassembly of
// Runtime-x64.dll at the retail addresses named per function (read as
// DISASSEMBLY, not decompiled C — reference_ghidra_c_output_vs_disassembly.md).
//
// WHY THIS CLASS FIRST. `?SecondsToTicks@Timer@System@@SA_JM@Z` and
// `?TicksToSeconds@Timer@System@@SAM_J@Z` are two of lane V's REAL DEPENDENCY
// rows (docs/runtime_port/WAVE9_LANE_V.md §1b) and they sit under
// `System::Time`'s ctor and `AsSeconds` — the value type every
// `Update(const Time&)` in the game constructs. They are the two most-executed
// raw-door calls left in the tree.
//
// ---------------------------------------------------------------------------
// THE FIVE STATICS, AND A CORRECTION TO LANE V
// ---------------------------------------------------------------------------
// Lane V (WAVE9_LANE_V.md §4) lists `?_secondsPerTick@Timer@System@@0NA` as a
// DATA READ row and says of it: *"it is written at runtime by System::Timer's
// init from QueryPerformanceFrequency, so minting a VALUE for it is wrong; the
// port has to own the initialisation."*
//
// **The first half of that is wrong, and it is worth being precise about
// because it changes who has to do the work.** Reading `Timer::Startup`
// (0x1800ca7c0) there is no QPC in it at all:
//
//     MOV  EAX,0xf4240                 ; 1,000,000
//     MOVSD XMM1,[0x1801616d0]         ; 1.0
//     CVTSI2SD XMM0,RAX
//     MOV  [0x1801fe720],RAX           ; _ticksPerSecond   = 1000000
//     DIVSD XMM1,XMM0
//     MOVSD [0x1801fe728],XMM1         ; _secondsPerTick   = 1.0 / 1000000.0
//     JMP  0x1800ca860                 ; tail into PlatformStartup
//
// `_ticksPerSecond` and `_secondsPerTick` are compile-time constants written by
// a store; the engine's tick is a fixed microsecond, not the platform's. It is
// `_systemTicksPerSecond`, `_systemSecondsPerTick` and `_systemInitialTick`
// (0x1801fe730 / 0x1801fe740 / 0x1801fe738) that are QPC-derived, and those are
// `PlatformStartup`'s (0x1800ca860). So the two hot functions depend on a
// CONSTANT, which is why they can be initialised statically below and carry no
// ordering hazard whatsoever.
//
// ---------------------------------------------------------------------------
// WHY OUR STATICS ARE INITIALISED HERE AND NOT BY A CALL TO Startup
// ---------------------------------------------------------------------------
// `?Startup@Timer@System@@SA_NXZ` has **no internal caller in the DLL** — its
// only xrefs are the export directory and the entry point. `Engine::Startup`
// (0x1800db8a0) INLINES the body instead: at 0x1800db9aa..0x1800db9ca it writes
// the same two constants to the same two globals itself and then makes a direct
// `CALL 0x1800ca860` to `PlatformStartup`.
//
// The consequence for the port is decisive. `Engine::Startup` is lane E's
// standing NO-GO (WAVE9.md §3, rows 12-15), so today it is the DLL's body, and
// a direct internal call cannot reach the port registry. **Our statics would
// therefore never be written by anything.** Initialising `_ticksPerSecond` and
// `_secondsPerTick` at namespace scope, to the exact constants the store above
// writes, produces a value identical to retail's under every ordering — this is
// not a mint standing in for an unknown, it is the same two literals.
//
// `_systemInitialTick` and the two QPC values genuinely need `PlatformStartup`
// to have run, so `CurrentTime`/`CurrentTimeTicks` call `EnsurePlatform()`
// first. See the note above `CurrentTimeTicks` for what that does and does not
// change, and for why it is not a "fix a faithful crash with a null check"
// (CLAUDE.md) — retail's own guarantee that Startup ran first does not survive
// the seam, and this is exactly the case where our standalone differs.
// ============================================================================

#include <cstdint>
#include "../../funcs/rt_port.h"
#include "timer.h"

// Provided by application_win.cpp's platform layer; declared here rather than
// including <windows.h>, which PLAN §4.1 keeps out of runtime/src (see the long
// note in rt_port.h about <wingdi.h>'s `Rectangle` and <windef.h>'s `near`).
extern "C" __declspec(dllimport) int __stdcall QueryPerformanceCounter(int64_t*);
extern "C" __declspec(dllimport) int __stdcall QueryPerformanceFrequency(int64_t*);

namespace {

// ?_ticksPerSecond@Timer@System@@0_JA   retail 0x1801fe720
// ?_secondsPerTick@Timer@System@@0NA    retail 0x1801fe728
// Constants, per the Startup disassembly quoted in the file header.
int64_t g_ticksPerSecond = 1000000;
double  g_secondsPerTick = 1.0 / 1000000.0;

// ?_systemTicksPerSecond@Timer@System@@0_JA  retail 0x1801fe730
// ?_systemInitialTick@Timer@System@@0_JA     retail 0x1801fe738
// ?_systemSecondsPerTick@Timer@System@@0NA   retail 0x1801fe740
// QPC-derived; written by PlatformStartup only.
int64_t g_systemTicksPerSecond = 0;
int64_t g_systemInitialTick    = 0;
double  g_systemSecondsPerTick = 0.0;

}  // namespace

// ?PlatformStartup@Timer@System@@CA_NXZ  retail 0x1800ca860 (private static).
//
// Retail on the QPF-failed branch calls the DLL's error reporter
// (0x1800c5fa0) with the literal at 0x180155f38 and returns false. That
// reporter is System::Error's, is not in this lane, and the branch is
// unreachable on every Windows version this game runs on (QPF has not been
// able to fail since XP). We return false without the message and say so here
// rather than reaching back into the DLL for one string on a dead path.
bool RtSystem_Timer_PlatformStartup() {
    int64_t freq = 0;
    if (!QueryPerformanceFrequency(&freq)) return false;

    g_systemTicksPerSecond = freq;
    // CVTSI2SD is signed; retail then adds 2^64 (0x180161720 = 1.8446744e19)
    // when the sign bit is set, i.e. it corrects the signed conversion back to
    // unsigned. Reproduced rather than simplified: a frequency with bit 63 set
    // is impossible in practice but the arithmetic is what the DLL does.
    double freqAsDouble = static_cast<double>(freq);
    if (freq < 0) freqAsDouble += 18446744073709551616.0;
    g_systemSecondsPerTick = 1.0 / freqAsDouble;

    QueryPerformanceCounter(&g_systemInitialTick);
    return true;
}

// ?Startup@Timer@System@@SA_NXZ  retail 0x1800ca7c0
// The two stores are already done by the namespace-scope initialisers above;
// they are repeated here because this function's contract is to establish
// them, and a caller that ran Shutdown first (a no-op in every shipped build,
// see below) must still get a live Timer back.
bool RtSystem_Timer_Startup() {
    g_ticksPerSecond = 1000000;
    g_secondsPerTick = 1.0 / static_cast<double>(1000000);
    return RtSystem_Timer_PlatformStartup();
}

// ?Shutdown@Timer@System@@SAXXZ          retail 0x1800ca7b0 — `RET`, no body.
// ?PlatformShutdown@Timer@System@@CAXXZ  retail 0x1800ca8e0 — `RET`, no body.
// Both are a bare return in the shipped build. Ported as empty rather than
// left unregistered so that neither is a missing symbol at cutover.
void RtSystem_Timer_Shutdown() {}
void RtSystem_Timer_PlatformShutdown() {}

// PlatformStartup is reached from Engine::Startup by a DIRECT internal call in
// the shipped DLL, which cannot pass through the port registry (file header).
// Until lane E owns Engine::Startup, nothing initialises our QPC statics, so
// the absolute-time readers ensure it themselves. This is not a guard bolted
// over a faithful crash: retail's precondition ("Startup ran") is genuinely
// broken by the seam, which is the one case CLAUDE.md's rule allows.
void RtSystem::TimerEnsurePlatformStarted() {
    if (g_systemTicksPerSecond == 0) RtSystem_Timer_PlatformStartup();
}

// The five data-export accessors declared in timer.h. TicksPerSecond and
// SecondsPerTick are constants and need no init; the three QPC-derived ones
// ensure PlatformStartup has run, so a consumer that reaches them before
// Engine::Startup gets a live value instead of the zero the DLL's slot holds
// at that moment.
int64_t RtSystem::TimerTicksPerSecond()       { return g_ticksPerSecond; }
double  RtSystem::TimerSecondsPerTick()       { return g_secondsPerTick; }
int64_t RtSystem::TimerSystemTicksPerSecond() { TimerEnsurePlatformStarted(); return g_systemTicksPerSecond; }
int64_t RtSystem::TimerSystemInitialTick()    { TimerEnsurePlatformStarted(); return g_systemInitialTick; }
double  RtSystem::TimerSystemSecondsPerTick() { TimerEnsurePlatformStarted(); return g_systemSecondsPerTick; }

namespace { inline void EnsurePlatform() { RtSystem::TimerEnsurePlatformStarted(); } }

// ?GetTicks@Timer@System@@CA_KXZ  retail 0x1800ca840 (private static).
// A raw QPC sample; the name's `_K` is unsigned but the OS writes a signed
// LARGE_INTEGER and retail moves it straight out.
uint64_t RtSystem_Timer_GetTicks() {
    int64_t now = 0;
    QueryPerformanceCounter(&now);
    return static_cast<uint64_t>(now);
}

// ?CurrentTimeTicks@Timer@System@@SA_JXZ  retail 0x1800ca8f0
//   (QPC() - _systemInitialTick) * _ticksPerSecond / _systemTicksPerSecond
// with a signed IMUL/CQO/IDIV — engine ticks (microseconds) since startup.
//
// NOT DIFFED, and the reason is a property of the function rather than a gap in
// the harness: this returns an offset from an EPOCH, and ours and the DLL's are
// two different `QueryPerformanceCounter` samples taken at two different
// moments. Ours-vs-theirs would differ by the interval between the two
// `PlatformStartup` calls, forever, and that difference is correct.
//
// The epoch-mixing hazard that implies is bounded by measurement, not by
// argument: **no file under code/, runtime/, engine/, standalone/ or dx11/
// names `?CurrentTime@Timer` or `?CurrentTimeTicks@Timer` at all** (swept
// 2026-09-07), so no ported caller can observe our epoch today, and the DLL's
// own callers (MainLoop::Run/Frame/Draw, Application::SleepThread,
// Video::Finish) read the globals directly and stay wholly on theirs. At
// cutover, Engine::Startup is ours, calls our PlatformStartup, and there is one
// epoch again. **These two therefore land with calls=0 and are unverified** —
// stated here rather than reported as clean, per
// reference_uncalled_port_is_not_verified.md.
int64_t RtSystem_Timer_CurrentTimeTicks() {
    EnsurePlatform();
    int64_t now = 0;
    QueryPerformanceCounter(&now);
    return (now - g_systemInitialTick) * g_ticksPerSecond / g_systemTicksPerSecond;
}

// ?CurrentTime@Timer@System@@SAMXZ  retail 0x1800ca760
// The same integer expression, then `(float)(ticks * _secondsPerTick)` —
// CVTSI2SD, MULSD, CVTPD2PS, i.e. the multiply is done in double and rounded
// to float exactly once. Same calls=0 / epoch caveat as above.
float RtSystem_Timer_CurrentTime() {
    EnsurePlatform();
    int64_t now = 0;
    QueryPerformanceCounter(&now);
    int64_t ticks = (now - g_systemInitialTick) * g_ticksPerSecond / g_systemTicksPerSecond;
    return static_cast<float>(static_cast<double>(ticks) * g_secondsPerTick);
}

// ?SecondsToTicks@Timer@System@@SA_JM@Z  retail 0x1800ca800
//   XORPS XMM1,XMM1 / CVTSS2SD XMM1,XMM0 / DIVSD XMM1,[_secondsPerTick]
//   / CVTTSD2SI RAX,XMM1
// float widened to double, divided in double, then TRUNCATED toward zero.
int64_t RtSystem_Timer_SecondsToTicks(float seconds) {
    RT_DIFF_BEGIN("Timer::SecondsToTicks");
    int64_t ours = static_cast<int64_t>(static_cast<double>(seconds) / g_secondsPerTick);
    using Fn = int64_t (*)(float);
    RT_DIFF_SCALAR(ours, RtOriginal<Fn>("?SecondsToTicks@Timer@System@@SA_JM@Z")(seconds));
    return ours;
}

// ?TicksToSeconds@Timer@System@@SAM_J@Z  retail 0x1800ca820
//   CVTSI2SD XMM0,RCX / MULSD XMM0,[_secondsPerTick] / CVTPD2PS XMM0,XMM0
// SIGNED widening (CVTSI2SD, not the unsigned pair), multiply in double, one
// rounding to float at the end. Bit-exactness is the expectation here, so the
// diff below is RT_DIFF_SCALAR on the float — an exact compare — and not
// RT_DIFF_FLOAT with a tolerance.
float RtSystem_Timer_TicksToSeconds(int64_t ticks) {
    RT_DIFF_BEGIN("Timer::TicksToSeconds");
    float ours = static_cast<float>(static_cast<double>(ticks) * g_secondsPerTick);
    using Fn = float (*)(int64_t);
    RT_DIFF_SCALAR(ours, RtOriginal<Fn>("?TicksToSeconds@Timer@System@@SAM_J@Z")(ticks));
    return ours;
}

// ============================================================
// RtSystemTimerRegister — called from RtSystemPortInit() under
// BANISHED_RT_TIMER (rt_system_port.cpp).
// ============================================================
int RtSystemTimerRegister() {
    int n = 0;
    RegisterRuntimeExport("?Startup@Timer@System@@SA_NXZ",         (void*)&RtSystem_Timer_Startup); ++n;
    RegisterRuntimeExport("?Shutdown@Timer@System@@SAXXZ",         (void*)&RtSystem_Timer_Shutdown); ++n;
    RegisterRuntimeExport("?PlatformStartup@Timer@System@@CA_NXZ", (void*)&RtSystem_Timer_PlatformStartup); ++n;
    RegisterRuntimeExport("?PlatformShutdown@Timer@System@@CAXXZ", (void*)&RtSystem_Timer_PlatformShutdown); ++n;
    RegisterRuntimeExport("?GetTicks@Timer@System@@CA_KXZ",        (void*)&RtSystem_Timer_GetTicks); ++n;
    RegisterRuntimeExport("?CurrentTime@Timer@System@@SAMXZ",      (void*)&RtSystem_Timer_CurrentTime); ++n;
    RegisterRuntimeExport("?CurrentTimeTicks@Timer@System@@SA_JXZ",(void*)&RtSystem_Timer_CurrentTimeTicks); ++n;
    RegisterRuntimeExport("?SecondsToTicks@Timer@System@@SA_JM@Z", (void*)&RtSystem_Timer_SecondsToTicks); ++n;
    RegisterRuntimeExport("?TicksToSeconds@Timer@System@@SAM_J@Z", (void*)&RtSystem_Timer_TicksToSeconds); ++n;
    return n;
}
