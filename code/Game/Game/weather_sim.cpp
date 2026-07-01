// code/Game/Game/weather_sim.cpp
//
// Clean reimplementation of the per-tick Weather SIMULATION driver
// (Weather::ResetTime, exe 0x1400944e0) — the sub-simulation that evolves the
// world temperature and writes the four terrain "weather" params the terrain
// shader reads (snow coverage + seasonal fades + active-weather intensity).
//
// This is the piece that was missing in standalone: fn_Weather_ResetTime
// resolved to NULL (0x1400944e0 was never reimplemented), so the weather sim
// never ticked and GameUpdateLoop::Draw pushed hard-zero snow params into the
// terrain cbuffer (spring/clear). Wiring this in makes the ground snow track a
// real, animated temperature model.
//
// SCOPE (decompiled from Application-x64.exe, verified in Ghidra):
//   Weather::ResetTime has SIX logical parts. This port implements the three
//   that are self-contained pure math + time/perlin (they drive the ground
//   snow) and DEFERS the three that need the precipitation-particle subsystem
//   (Environment.rsc weather descriptors + Graphics/Audio scene emitters),
//   which are null in standalone:
//
//     Part 1  active-effect audio/geometry cleanup   — DEFERRED (count==0)
//     Part 2  exponential smoothing (+0x2c,+0x44)     — ported
//     Part 3  Weather_helper_3810 temperature (+0x18) — ported
//     Part 4  FUN_140093420 coverage/fades (+0x1c..)  — ported
//     Part 5  FUN_1400935f0 weather-type state machine— DEFERRED (precip sched)
//     impl_42c0 effect enable/disable (2x/tick)       — DEFERRED (needs envRes)
//     Part 6  precipitation entity spawn              — DEFERRED (particles)
//
//   The deferred parts belong to the falling-rain/snow-particle task. They
//   dereference the active-weather array (weather+0x50, null here) and the
//   Environment weather-descriptor array (envRes+0x28, null here), so running
//   them now would fault — and they produce no visible output without the
//   particle emitter subsystem anyway.
//
// Weather sim struct (gameWorld+0x130, 0x78 bytes; ctor FUN_140093f60,
// env-bind FUN_140093e30):
//   +0x00 envRes (Environment weather descriptors; NULL in standalone)
//   +0x08 climateGen (ClimateGenerator resource; real, set by NewGameDialog)
//   +0x10 state    +0x14 intensityLevel (0..5)
//   +0x18 temperature (accumulated by Part 3)
//   +0x1c snowCoverage   +0x20 seasonalFadeA   +0x24 seasonalFadeB
//   +0x28 activeWeatherIntensity   (the four terrain-shader params)
//   +0x2c meanTemp (smoothed)   +0x30 meanTemp target (from climate+0x34)
//   +0x34 PerlinNoise (0x10 bytes, temperature noise; built by ctor)
//   +0x44 precipProb (smoothed)   +0x48 precipProb target
//   +0x50 activeEffect geometry array (ptr)   +0x58 count  (empty in standalone)
//   +0x70 elapsed-time accumulator (int64)
//
// ClimateGenerator fields (real; ctor 0x1400930c0 defaults):
//   +0x34 temperatureBase=50  +0x38 tempVariation=30  +0x3c precip=5  +0x40 wind=10

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cmath>

namespace {

// Exact .rdata float constants (bit patterns read from the exe at 0x14017xxxx).
inline float f32(uint32_t bits) { float f; __builtin_memcpy(&f, &bits, 4); return f; }

const float kSmoothIn  = f32(0x3a831200);  // DAT_1401720b4  ~0.001   (EMA new weight)
const float kSmoothOld = f32(0x3f7fbe77);  // DAT_140172148  ~0.99897 (EMA old weight)
const float kDeg2Rad   = f32(0x3c8efa35);  // DAT_1401720cc  pi/180
const float kPerlinDiv = 180.0f;           // DAT_140172220  (perlin time scale)
const float kKfSpan    = 300.0f;           // DAT_140172234  (keyframe span; day-of-year divisor)
const float kDayLen    = 3600.0f;          // DAT_14017225c  (seasonal cycle length)
const float k360       = 360.0f;           // DAT_140172238
const float kFreeze    = 32.0f;            // DAT_1401721d0  (freeze threshold)
const float kCovScale  = -1.0f;            // DAT_140172280  (coverage = -(temp-32))
const float kFadeA0    = 6.0f;             // DAT_14017219c
const float kFadeA1    = 1.5f;             // DAT_140172160
const float kFadeB0    = 0.5f;             // DAT_140172114
const float kFadeB1    = 7.5f;             // DAT_1401721a4
const float kFadeB2    = 2.0f;             // DAT_140172174

inline float saturate(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

}  // namespace

// ============================================================
// Weather::ResetTime (exe 0x1400944e0) — snow/temperature sub-sim
//   weatherSelf = gameWorld+0x130 (the weather sim struct)
//   time        = Time* (int64 tick delta) — only the deferred spawn path uses it
// ============================================================
extern "C" void Weather_ResetTime_Reimpl(void* weatherSelf, void* time) {
    if (!weatherSelf) return;

    // Default ON ("fire in the sim"); BANISHED_LIVE_WEATHER=0 restores the old
    // hard-zero spring/clear behaviour for A/B comparison.
    static int s_enabled = -1;
    if (s_enabled < 0) {
        const char* e = std::getenv("BANISHED_LIVE_WEATHER");
        s_enabled = (e && (e[0] == '0' || e[0] == 'f' || e[0] == 'F')) ? 0 : 1;
    }
    if (!s_enabled) return;

    uint8_t* w = reinterpret_cast<uint8_t*>(weatherSelf);

    // --- Game clock (seconds) — reads the sim clock at gameWorld+0x128 -> Time+8,
    //     exactly like the original helper_3810 / FUN_140093420. ---
    void* gw = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gw || !fn_Time_AsSeconds) return;
    void* timeObj = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(gw) + 0x128);
    if (!timeObj) return;
    float sec = fn_Time_AsSeconds(reinterpret_cast<uint8_t*>(timeObj) + 8);

    // --- Part 2: exponential smoothing of the mean-temp and precip targets. ---
    *reinterpret_cast<float*>(w + 0x2c) =
        *reinterpret_cast<float*>(w + 0x30) * kSmoothIn +
        *reinterpret_cast<float*>(w + 0x2c) * kSmoothOld;
    *reinterpret_cast<float*>(w + 0x44) =
        *reinterpret_cast<float*>(w + 0x48) * kSmoothIn +
        *reinterpret_cast<float*>(w + 0x44) * kSmoothOld;

    // --- Part 3: temperature accumulation (Weather_helper_3810) -> +0x18.
    //     temp = sin(seasonalPhase) * variation + mean  +  perlin * noiseAmp   ---
    {
        void* climate = *reinterpret_cast<void**>(w + 0x08);
        float mean = *reinterpret_cast<float*>(w + 0x2c);
        float variation, noiseAmp;
        if (climate) {
            variation = *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(climate) + 0x38);
            noiseAmp  = *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(climate) + 0x3c);
        } else {
            // ClimateGenerator ctor defaults (0x1400930c0) — robust fallback if the
            // climate resource never bound (env-bind guards on climate != null).
            variation = 30.0f;
            noiseAmp  = 5.0f;
            if (mean == 0.0f) mean = 50.0f;   // temperatureBase default
        }
        float perlin = 0.0f;
        if (fn_PerlinNoise_Generate)
            perlin = fn_PerlinNoise_Generate(w + 0x34, sec / kPerlinDiv, 0.0f);
        float phase = ((sec - kKfSpan) / kDayLen) * k360 * kDeg2Rad;   // radians
        float temp = sinf(phase) * variation + mean;
        *reinterpret_cast<float*>(w + 0x18) = perlin * noiseAmp + temp;
    }

    // --- Part 4: snow coverage + seasonal fades + active intensity (FUN_140093420). ---
    {
        float accum = *reinterpret_cast<float*>(w + 0x18);
        float coverage = saturate((accum - kFreeze) * kCovScale);   // saturate(32 - temp)

        float doy = sec / kKfSpan;   // day-of-year progression (0..12 per cycle)

        float fadeA = (doy >= kFadeA0) ? saturate((doy - kFadeA0) / kFadeA1) : 0.0f;

        float fadeB;
        if (doy >= kFadeB0) {
            fadeB = (doy <= kFadeB1) ? 0.0f : saturate((doy - kFadeB1) * kFadeB2);
        } else {
            fadeB = 1.0f - saturate(doy / kFadeB0);
        }

        // Max intensity across active weather effects (empty in standalone -> 0).
        float maxIntensity = 0.0f;
        int cnt = *reinterpret_cast<int*>(w + 0x58);
        if (cnt > 0) {
            void** arr = *reinterpret_cast<void***>(w + 0x50);
            if (arr) {
                for (int i = 0; i < cnt; ++i) {
                    if (arr[i]) {
                        float v = *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(arr[i]) + 0xc0);
                        if (v > maxIntensity) maxIntensity = v;
                    }
                }
            }
        }

        *reinterpret_cast<float*>(w + 0x1c) = coverage;
        *reinterpret_cast<float*>(w + 0x20) = fadeA;
        *reinterpret_cast<float*>(w + 0x24) = fadeB;
        *reinterpret_cast<float*>(w + 0x28) = maxIntensity;
    }

    // --- Elapsed-time accumulator (+0x70) — faithful groundwork for the deferred
    //     precipitation-spawn path (FUN_1400a9290 consumes+zeros it). Inert here. ---
    if (time)
        *reinterpret_cast<int64_t*>(w + 0x70) += *reinterpret_cast<int64_t*>(time);

    // Optional verification trace: BANISHED_WEATHER_TRACE=1 logs the evolving
    // temperature + terrain params every ~120 ticks.
    {
        static int s_trace = -1;
        if (s_trace < 0) {
            const char* e = std::getenv("BANISHED_WEATHER_TRACE");
            s_trace = (e && e[0] && e[0] != '0') ? 1 : 0;
        }
        if (s_trace) {
            static int s_n = 0;
            if ((s_n++ % 120) == 0) {
                fprintf(stderr, "[weather] t=%.1fs temp=%.2f coverage=%.3f "
                        "fadeA=%.3f fadeB=%.3f intensity=%.3f\n",
                        sec,
                        *reinterpret_cast<float*>(w + 0x18),
                        *reinterpret_cast<float*>(w + 0x1c),
                        *reinterpret_cast<float*>(w + 0x20),
                        *reinterpret_cast<float*>(w + 0x24),
                        *reinterpret_cast<float*>(w + 0x28));
                fflush(stderr);
            }
        }
    }
}
