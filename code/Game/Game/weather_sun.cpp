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
#include "math/Math.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)

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
typedef void  (*fn_Context_SetFog_t)(void* ctx, float fogNear, float fogFar, const float* color);
fn_Context_SetFog_t s_fn_Context_SetFog = nullptr;
bool s_resolved = false;

void EnsureResolved() {
    if (s_resolved) return;
    s_resolved = true;
    s_fn_Context_SetFog = reinterpret_cast<fn_Context_SetFog_t>(
        ResolveExport("?SetFog@Context@Video@@QEAAXMMAEBVVector4@Math@@@Z"));
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
    if (!sun || !fn_Graphics_SunLight_Set) return;

    // Game time in seconds (the sim clock). +0x128 -> Time struct, time @ +8.
    float seconds = 0.0f;
    void* timeObj = *reinterpret_cast<void**>(p + 0x128);
    if (timeObj && fn_Time_AsSeconds)
        seconds = reinterpret_cast<const System::Time*>(reinterpret_cast<uint8_t*>(timeObj) + 8)->AsSeconds();   // B.3.4b bridge

    // Phase within the day -> keyframe index + interpolation fraction.
    float dayT = seconds - kDayLength * floorf(seconds / kDayLength);  // wrap into [0,dayLen)
    int   kf   = (int)((dayT / kDayLength) * 12.0f);
    if (kf < 0) kf = 0;
    if (kf > 11) kf = 0;
    int   kfNext = (kf + 1) % 12;
    float frac   = (dayT - (float)kf * kKeyframeSpan) / kKeyframeSpan;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;

    // Base keyframes: retail Weather::DrawUpdate (helper_3ac0 @ exe 0x140093ac0)
    // reads the LIVE per-month lighting table from the loaded Environment.rsc
    // (envRes) at envRes+0x38+i*0x20 — the exact bytes the debug-UI lighting
    // sliders edit (DebugUI_WriteLightingColors, debugui.cpp). Source from there
    // so slider edits take effect and the day cycle is data-driven exactly like
    // retail; fall back to the baked-in kKeyframes only if Environment.rsc failed
    // to load (envRes null). EnvKeyframe is byte-compatible with the envRes entry
    // layout (ambient@0, light@4, fog@8, fogNear@c, fogFar@10, fogMax@14,
    // heading@18, pitch@1c), so the table is a direct reinterpret.
    const EnvKeyframe* table = kKeyframes;
    void* wsim = *reinterpret_cast<void**>(p + 0x130);   // weather sim (gameWorld+0x130)
    if (wsim) {
        void* envRes = *reinterpret_cast<void**>(wsim);  // *(weatherSim+0) == envRes
        if (envRes)
            table = reinterpret_cast<const EnvKeyframe*>(
                reinterpret_cast<uint8_t*>(envRes) + 0x38);
    }
    const EnvKeyframe& a = table[kf];
    const EnvKeyframe& b = table[kfNext];

    // Base day-cycle look (interpolated keyframe): sun angle + light/ambient/fog.
    float heading = Lerp(a.sunHeading, b.sunHeading, frac);
    float pitch   = Lerp(a.sunPitch,   b.sunPitch,   frac);
    float lightCol[4], ambientCol[4], fogCol[4];
    LerpColor(a.lightColor,   b.lightColor,   frac, lightCol);    // diffuse (col1)
    LerpColor(a.ambientColor, b.ambientColor, frac, ambientCol);  // ambient (col2)
    LerpColor(a.fogColor,     b.fogColor,     frac, fogCol);
    float fogNear = Lerp(a.fogNear, b.fogNear, frac);
    float fogFar  = Lerp(a.fogFar,  b.fogFar,  frac);

    // Active weather-effect color blend — faithful port of Weather::DrawUpdate's
    // helper_3ac0 second loop (exe 0x140093ac0). Each live rain/snow effect lerps
    // the day-cycle look toward its Weather descriptor colors/fog distances
    // (desc+0x60 ambient, +0x64 light, +0x68 fog, +0x6c/+0x70 fog dist), weighted
    // by the effect's fade intensity (geo+0xc0). This tints the scene grey during
    // rain / bright during snow. Note: retail's interpolator sub_49f0 is called
    // with param_5=0 here, so it does NOT blend sun heading/pitch (desc+0x78/+0x7c)
    // — the sun angle stays on the base day-cycle value during precipitation. No-op
    // when no effects are active, so the base look is intact.
    {
        uint8_t weatherSetting = (g_pGameSettingsPtr && *g_pGameSettingsPtr)
            ? reinterpret_cast<const uint8_t*>(*g_pGameSettingsPtr)[0x16] : 0;
        if (weatherSetting != 0 && wsim) {
            uint8_t* ws = reinterpret_cast<uint8_t*>(wsim);
            int cnt = *reinterpret_cast<int*>(ws + 0x58);
            void** arr = *reinterpret_cast<void***>(ws + 0x50);
            if (cnt > 0 && arr) {
                for (int i = 0; i < cnt; ++i) {
                    void* geo = arr[i];
                    if (!geo) continue;
                    float t = *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(geo) + 0xc0);
                    void* desc = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(geo) + 0xc8);
                    if (!desc) continue;
                    uint8_t* dd = reinterpret_cast<uint8_t*>(desc);
                    float dAmb[4], dLight[4], dFog[4];
                    UnpackColor(*reinterpret_cast<uint32_t*>(dd + 0x60), dAmb);
                    UnpackColor(*reinterpret_cast<uint32_t*>(dd + 0x64), dLight);
                    UnpackColor(*reinterpret_cast<uint32_t*>(dd + 0x68), dFog);
                    for (int k = 0; k < 4; ++k) {
                        ambientCol[k] = Lerp(ambientCol[k], dAmb[k],   t);
                        lightCol[k]   = Lerp(lightCol[k],   dLight[k], t);
                        fogCol[k]     = Lerp(fogCol[k],     dFog[k],   t);
                    }
                    fogNear = Lerp(fogNear, *reinterpret_cast<float*>(dd + 0x6c), t);
                    fogFar  = Lerp(fogFar,  *reinterpret_cast<float*>(dd + 0x70), t);
                    // heading/pitch intentionally NOT blended (retail sub_49f0
                    // param_5=0 keeps the base day-cycle sun angle during precip).
                }
            }
        }
    }

    // Direction from the (possibly effect-blended) yaw/pitch (degrees). FromYawPitch
    // returns z=+sin(pitch) (pointing UP toward the sun), but the engine's sun/shadow
    // system expects the light-travel direction with z=-sin(pitch) (pointing DOWN).
    // Verified by reading the retail in-game _LightConstants_ light dir
    // (-0.554,-0.320,-0.769) vs our unnegated (-0.557,-0.321,+0.766): only Z flips.
    // Without this, GetSunCamera builds the sun shadow camera from the wrong side ->
    // shadowProjection[0].z sign inverts -> the shadow depth comparison always passes
    // -> NO cast shadows (ground gets ambient-only). Negate Z to match retail.
    float dir[3] = {0.0f, 0.0f, 1.0f};
    *reinterpret_cast<Math::Vector3*>(dir) = Math::Vector3::FromYawPitch(heading, pitch);
    dir[2] = -dir[2];


    fn_Graphics_SunLight_Set(sun, dir, lightCol, ambientCol);

    // Fog (day-cycle distances + color, effect-blended above).
    if (ctx && s_fn_Context_SetFog) {
        s_fn_Context_SetFog(ctx, fogNear, fogFar, fogCol);
    }
}
