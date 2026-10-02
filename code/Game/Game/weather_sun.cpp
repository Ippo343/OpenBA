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

// ENDGAME B.3.5 Video/Graphics caller batch: Graphics::SunLight and
// Video::Context are real classes. Both headers go AHEAD of "resolve.h" —
// resolve.h pulls <windows.h>, and <wingdi.h>'s global `Rectangle` FUNCTION
// hides `Volume::Rectangle`'s class name inside Volume.h's own static_asserts.
#include "graphics/SunLight.h"
#include "video/Context.h"
#include "graphics/Camera.h"
#include "Game/exe_bodies.h"   // B3R STORAGE: g_gameSettings
#include "Game/GameUpdateLoop.h"   // caller batch CB6: the world's named fields
#include "Game/WeatherSimulation.h"
#include "Game/WeatherParticleGeometry.h"
#include "Game/Weather.h"
#include "Game/Calendar.h"
#include "Game/GameSettings.h"
#include "Component/EnvironmentDescription.h"
#include "graphics/SunLight.h"
#include "math/Math.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)

// ============================================================
// Base-game day-cycle keyframes (Environment/Environment.rsc)
// ============================================================
namespace {

// One keyframe is EnvironmentDescription's month record (0x20): skyColor = the
// ambient colour (SunLight col2), groundColor = the light colour (col1), fogColor,
// fogStart / fogEnd, sunIntensity (fogMax; unused by SetFog here), sunAngle = the
// sun heading (degrees, constant 120), sunSpread = the sun pitch (48..65).
using EnvKeyframe = EnvironmentDescription::WeatherEntry;

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

// ENDGAME B.3.5: the lazy `?SetFog@Context@Video@@QEAAXMMAEBVVector4@Math@@@Z`
// door cache (typedef + static + EnsureResolved + its latch) is GONE — its only
// caller in this file now calls `Video::Context::SetFog` directly.

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

    // Caller batch CB6: `self` is the GameUpdateLoop (its one caller is Draw's
    // `this`); its fields are read by name.
    GameUpdateLoop* const gul = static_cast<GameUpdateLoop*>(self);   // B.3.8 bridge
    Graphics::SunLight* sun = gul->m_sunLight;   // +0x100
    // B.3.5: the `!fn_Graphics_SunLight_Set` term is gone with the door call;
    // the `!sun` object test is retail's and stays.
    if (!sun) return;

    // Game time in seconds (the sim clock). +0x128 -> Time struct, time @ +8.
    float seconds = 0.0f;
    Calendar* timeObj = gul->m_dayCounter;   // +0x128
    if (timeObj)
        seconds = timeObj->yearTime.AsSeconds();

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
    // heading@18, pitch@1c): it IS EnvironmentDescription::WeatherEntry.
    const EnvKeyframe* table = kKeyframes;
    WeatherSimulation* wsim = gul->m_weather;   // weather sim (gameWorld+0x130)
    if (wsim) {
        EnvironmentDescription* envRes = wsim->environment;  // *(weatherSim+0) == envRes
        if (envRes)
            table = envRes->_months;   // +0x38
    }
    const EnvKeyframe& a = table[kf];
    const EnvKeyframe& b = table[kfNext];

    // Base day-cycle look (interpolated keyframe): sun angle + light/ambient/fog.
    float heading = Lerp(a.sunAngle,  b.sunAngle,  frac);
    float pitch   = Lerp(a.sunSpread, b.sunSpread, frac);
    float lightCol[4], ambientCol[4], fogCol[4];
    LerpColor(a.groundColor, b.groundColor, frac, lightCol);    // diffuse (col1)
    LerpColor(a.skyColor,    b.skyColor,    frac, ambientCol);  // ambient (col2)
    LerpColor(a.fogColor,    b.fogColor,    frac, fogCol);
    float fogNear = Lerp(a.fogStart, b.fogStart, frac);
    float fogFar  = Lerp(a.fogEnd,   b.fogEnd,   frac);

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
        uint8_t weatherSetting = (g_gameSettings != nullptr)
            ? g_gameSettings->showWeather : 0;   // +0x16
        if (weatherSetting != 0 && wsim) {
            int cnt = wsim->effectCount;                 // +0x58
            Graphics::Geometry** arr = wsim->effects;    // +0x50
            if (cnt > 0 && arr) {
                for (int i = 0; i < cnt; ++i) {
                    if (!arr[i]) continue;
                    // The effects are WeatherParticleGeometry (WeatherSimulation::SetEffectEnabled).
                    const WeatherParticleGeometry* geo = static_cast<const WeatherParticleGeometry*>(arr[i]);
                    float t = geo->_intensity;                 // +0xC0
                    const Weather* desc = geo->_descriptor;    // +0xC8
                    if (!desc) continue;
                    float dAmb[4], dLight[4], dFog[4];
                    UnpackColor(desc->ambientColor._value, dAmb);   // +0x60
                    UnpackColor(desc->lightColor._value, dLight);   // +0x64
                    UnpackColor(desc->fogColor._value, dFog);       // +0x68
                    for (int k = 0; k < 4; ++k) {
                        ambientCol[k] = Lerp(ambientCol[k], dAmb[k],   t);
                        lightCol[k]   = Lerp(lightCol[k],   dLight[k], t);
                        fogCol[k]     = Lerp(fogCol[k],     dFog[k],   t);
                    }
                    fogNear = Lerp(fogNear, desc->fogNear, t);   // +0x6C
                    fogFar  = Lerp(fogFar,  desc->fogFar,  t);   // +0x70
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
    Math::Vector3 dir = Math::Vector3::FromYawPitch(heading, pitch);
    dir.z = -dir.z;

    // ?Set@SunLight@Graphics@@QEAAXAEBVVector3@Math@@AEBVVector4@4@1@Z.
    sun->Set(dir,
             Math::Vector4(lightCol[0], lightCol[1], lightCol[2], lightCol[3]),
             Math::Vector4(ambientCol[0], ambientCol[1], ambientCol[2], ambientCol[3]));

    // Fog (day-cycle distances + color, effect-blended above).
    if (ctx) {
        // ?SetFog@Context@Video@@QEAAXMMAEBVVector4@Math@@@Z.
        // LEFT: ctx is the caller's void* (gameupdateloop.cpp declares this entry untyped).
        reinterpret_cast<Video::Context*>(ctx)->SetFog(
            fogNear, fogFar, Math::Vector4(fogCol[0], fogCol[1], fogCol[2], fogCol[3]));
    }
}
