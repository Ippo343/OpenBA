// code/Game/Game/weather_sun.cpp
//
// Native time-of-day sun/lighting driver — clean reimplementation of the
// base game's Weather::DrawUpdate day cycle (exe 0x1400938d0 / helper_3ac0
// 0x140093ac0 / lerp sub_49f0), so the sun elevation, light/ambient colors,
// and fog animate across the in-game day exactly like Banished.
//
// Mechanism (verified in Ghidra, see project_shadow_pipeline memory):
//   - The base game stores 12 lighting keyframes in Environment/Environment.rsc
//     (EnvironmentDescription, array @+0x38, stride 0x20 = 3 Color + 5 float =
//     {ambientColor, lightColor, fogColor, fogNear, fogFar, fogMax,
//      sunHeading, sunPitch}).
//   - Weather::DrawUpdate runs per frame: reads game time, picks keyframe
//     kf = (t/dayLen)*12 (dayLen = 3600 s, 300 s/keyframe), interpolates
//     kf -> (kf+1)%12, then Vector3::FromYawPitch(heading, pitch) -> SunLight::Set
//     (g_pGameWorld+0x100) plus Context::SetFog.
//   - sunHeading is a constant 120 deg for all keyframes (the sun does NOT swing
//     in azimuth); sunPitch (elevation) oscillates 48 deg (dawn/dusk/night) to
//     65 deg (noon), so shadows are longest at dawn/dusk and shortest at noon.
//
// In standalone the original Weather::DrawUpdate isn't reimplemented (so it
// no-ops), and Environment.rsc isn't loaded. This driver bypasses both by
// embedding the real keyframe table (extracted from the modding toolkit source,
// ToolKit/resource/Environment/Environment.rsc) and applying it directly.
//
// FromYawPitch(yaw, pitch) [Runtime 0x1800220b0, takes DEGREES] returns the
// unit light direction ready for SunLight::Set (no negate): x=-cos(p)sin(y),
// y=cos(p)cos(y), z=sin(p).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cmath>

// ============================================================
// Base-game day-cycle keyframes (Environment/Environment.rsc)
// ============================================================
namespace {

struct EnvKeyframe {
    uint32_t ambientColor;   // 0xAARRGGBB (dim)   -> SunLight ambient (col2)
    uint32_t lightColor;     // 0xAARRGGBB (bright) -> SunLight diffuse (col1)
    uint32_t fogColor;       // 0xAARRGGBB
    float    fogNear;
    float    fogFar;
    float    fogMax;         // unused by SetFog here, kept for fidelity
    float    sunHeading;     // degrees (constant 120)
    float    sunPitch;       // degrees (elevation, 48..65)
};

// 12 keyframes spanning one in-game day (300 s each, 3600 s total).
constexpr EnvKeyframe kKeyframes[12] = {
    {0xFF59596B, 0xFFD1D1B9, 0xFFBFBFBF, 90.0f,      225.0f,      0.95f, 120.0f, 50.176468f},
    {0xFF595965, 0xFFCBC2B3, 0xFFBFBFBF, 90.0f,      225.0f,      0.95f, 120.0f, 55.470589f},
    {0xFF595965, 0xFFCBBF9E, 0xFFBFBFD4, 90.0f,      225.0f,      0.95f, 120.0f, 60.764706f},
    {0xFF59596B, 0xFFCBC2B0, 0xFFBFBFE3, 90.0f,      225.0f,      0.95f, 120.0f, 62.882355f},
    {0xFF595968, 0xFFCBC5B3, 0xFFBFBFDA, 90.0f,      225.0f,      0.95f, 120.0f, 65.000000f},
    {0xFF595968, 0xFFCBBFA1, 0xFFBFBFDD, 90.0f,      225.0f,      0.95f, 120.0f, 62.882355f},
    {0xFF595965, 0xFFCBB395, 0xFFBFBFDD, 90.0f,      225.0f,      0.95f, 120.0f, 60.764706f},
    {0xFF595965, 0xFFCBB083, 0xFFC8BFBF, 90.0f,      225.0f,      0.95f, 120.0f, 55.470589f},
    {0xFF595965, 0xFFCBAD74, 0xFFCBBFAD, 90.0f,      225.0f,      0.95f, 120.0f, 50.176472f},
    {0xFF595965, 0xFFB6A489, 0xFFE3E3E3, 57.647053f, 289.705872f, 0.95f, 120.0f, 48.058823f},
    {0xFF595965, 0xFFB6A489, 0xFFE3E3E3, 31.764700f, 335.000000f, 0.95f, 120.0f, 48.058823f},
    {0xFF595965, 0xFFA7A792, 0xFFBFBFD1, 31.764700f, 367.352936f, 0.95f, 120.0f, 48.058823f},
};

constexpr float kDayLength    = 3600.0f;   // exe DAT_14017225c
constexpr float kKeyframeSpan = 300.0f;    // exe literal (dayLength / 12)

// Resolved Runtime helpers (lazy, latched).
typedef void* (*fn_FromYawPitch_t)(void* retVec3, const float* yaw, const float* pitch);
typedef void  (*fn_Context_SetFog_t)(void* ctx, float fogNear, float fogFar, const float* color);
fn_FromYawPitch_t   s_fn_FromYawPitch = nullptr;
fn_Context_SetFog_t s_fn_Context_SetFog = nullptr;
bool s_resolved = false;

void EnsureResolved() {
    if (s_resolved) return;
    s_resolved = true;
    s_fn_FromYawPitch = reinterpret_cast<fn_FromYawPitch_t>(
        ResolveExport("?FromYawPitch@Vector3@Math@@SA?AV12@AEBVScalar@2@0@Z"));
    if (!s_fn_FromYawPitch)
        s_fn_FromYawPitch = reinterpret_cast<fn_FromYawPitch_t>(
            ResolveAddr(Module::Runtime, 0x1800220b0));
    s_fn_Context_SetFog = reinterpret_cast<fn_Context_SetFog_t>(
        ResolveExport("?SetFog@Context@Video@@QEAAXMMAEBVVector4@Math@@@Z"));
    if (!s_fn_Context_SetFog)
        s_fn_Context_SetFog = reinterpret_cast<fn_Context_SetFog_t>(
            ResolveAddr(Module::Runtime, 0x18001a360));
}

inline void UnpackColor(uint32_t c, float out[4]) {
    out[0] = (float)((c >> 16) & 0xFF) / 255.0f;  // R
    out[1] = (float)((c >> 8)  & 0xFF) / 255.0f;  // G
    out[2] = (float)( c        & 0xFF) / 255.0f;  // B
    out[3] = 1.0f;                                 // A forced to 1 (matches original)
}

inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

inline void LerpColor(uint32_t a, uint32_t b, float t, float out[4]) {
    float ca[4], cb[4];
    UnpackColor(a, ca);
    UnpackColor(b, cb);
    for (int i = 0; i < 4; ++i) out[i] = Lerp(ca[i], cb[i], t);
}

}  // namespace

// ============================================================
// Weather_SunUpdate — apply the time-of-day lighting to the scene SunLight.
//   self = GameUpdateLoop* (== g_pGameWorld). self+0x100 = SunLight,
//          self+0x128 = pointer to the 0x18-byte game Time struct (time @ +8).
//   ctx  = Video::Context*.
// Called once per rendered frame from GameUpdateLoop::Draw (step 5).
// ============================================================
extern "C" void Weather_SunUpdate(void* self, void* ctx) {
    if (!self) return;
    EnsureResolved();

    uint8_t* p = reinterpret_cast<uint8_t*>(self);
    void* sun = *reinterpret_cast<void**>(p + 0x100);
    if (!sun || !fn_Graphics_SunLight_Set || !s_fn_FromYawPitch) return;

    // Game time in seconds (the sim clock). +0x128 -> Time struct, time @ +8.
    float seconds = 0.0f;
    void* timeObj = *reinterpret_cast<void**>(p + 0x128);
    if (timeObj && fn_Time_AsSeconds)
        seconds = fn_Time_AsSeconds(reinterpret_cast<uint8_t*>(timeObj) + 8);

    // Phase within the day -> keyframe index + interpolation fraction.
    float dayT = seconds - kDayLength * floorf(seconds / kDayLength);  // wrap into [0,dayLen)
    int   kf   = (int)((dayT / kDayLength) * 12.0f);
    if (kf < 0) kf = 0;
    if (kf > 11) kf = 0;
    int   kfNext = (kf + 1) % 12;
    float frac   = (dayT - (float)kf * kKeyframeSpan) / kKeyframeSpan;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;

    const EnvKeyframe& a = kKeyframes[kf];
    const EnvKeyframe& b = kKeyframes[kfNext];

    float heading = Lerp(a.sunHeading, b.sunHeading, frac);
    float pitch   = Lerp(a.sunPitch,   b.sunPitch,   frac);

    // Direction from yaw/pitch (degrees) — ready for SunLight::Set (no negate).
    float dir[3] = {0.0f, 0.0f, 1.0f};
    s_fn_FromYawPitch(dir, &heading, &pitch);

    // Optional verification trace: BANISHED_SUN_TRACE=1 logs the advancing
    // day-time / keyframe / sun pitch every ~120 frames.
    {
        static int s_trace = -1;
        if (s_trace < 0) {
            const char* e = std::getenv("BANISHED_SUN_TRACE");
            s_trace = (e && e[0] && e[0] != '0') ? 1 : 0;
        }
        if (s_trace) {
            static int s_n = 0;
            if ((s_n++ % 120) == 0) {
                fprintf(stderr, "[sun] t=%.1fs dayT=%.1f kf=%d->%d frac=%.2f "
                        "heading=%.1f pitch=%.2f\n",
                        seconds, dayT, kf, kfNext, frac, heading, pitch);
                fflush(stderr);
            }
        }
    }

    float lightCol[4], ambientCol[4];
    LerpColor(a.lightColor,   b.lightColor,   frac, lightCol);    // diffuse (col1)
    LerpColor(a.ambientColor, b.ambientColor, frac, ambientCol);  // ambient (col2)

    fn_Graphics_SunLight_Set(sun, dir, lightCol, ambientCol);

    // Fog (matches the base game's per-time fog distances + color).
    if (ctx && s_fn_Context_SetFog) {
        float fogCol[4];
        LerpColor(a.fogColor, b.fogColor, frac, fogCol);
        float fogNear = Lerp(a.fogNear, b.fogNear, frac);
        float fogFar  = Lerp(a.fogFar,  b.fogFar,  frac);
        s_fn_Context_SetFog(ctx, fogNear, fogFar, fogCol);
    }
}
