// src/game/Game/weather_sun.cpp
//
// WeatherSimulation::DrawUpdate (Application-x64.exe 0x1400938d0) and the two
// helpers it runs: the per-frame lighting blend 0x140093ac0 and the
// EnvironmentSetting lerp 0x1400949f0.
//
// Per rendered frame, GameUpdateLoop::Draw (0x140096158) calls
// `m_weather->DrawUpdate(*context)`. The body:
//   1. blends the environment's twelve monthly lighting records
//      (EnvironmentDescription +0x38, 0x20 each) for the current year time,
//      then blends every live rain / snow effect's own lighting (its Weather
//      resource's +0x60..+0x7C record) in by the effect's fade (+0xC0);
//   2. copies the four terrain weather floats (+0x1C..+0x28: snow coverage,
//      the two seasonal fades, the strongest effect's fade) into the Context
//      twice (+0x3B8 and the pixel staging +0x288) and dirties 0x10800;
//   3. Context::SetFog(fogNear, fogFar, {fog rgb, fogMax});
//   4. SunLight::Set(FromYawPitch(heading, -pitch), light rgb1, ambient rgb1)
//      on the world's sun (GameUpdateLoop +0x100).

#include "graphics/SunLight.h"
#include "video/Context.h"
#include "Game/exe_bodies.h"          // g_gameSettings
#include "Game/exe_constants.h"       // kRdata_*
#include "Game/GameUpdateLoop.h"
#include "Game/WeatherSimulation.h"
#include "Game/WeatherParticleGeometry.h"
#include "Game/Weather.h"
#include "Game/Calendar.h"
#include "Game/GameSettings.h"
#include "Component/EnvironmentDescription.h"
#include "math/Math.h"
#include "system/Value.h"
#include <cstdint>

namespace {

using EnvironmentSetting = EnvironmentDescription::WeatherEntry;

inline float BitsF(uint32_t bits) { float f; __builtin_memcpy(&f, &bits, 4); return f; }

// One packed colour channel of the lerp: both ends to 0..1, weighted, back to
// 0..255 and truncated (CVTTSS2SI).
inline int32_t LerpChannel(uint32_t a, uint32_t b, int shift, float oneMinusT, float t) {
    const float k255 = BitsF(kRdata_14017222c);   // 255.0f
    float fa = static_cast<float>((a >> shift) & 0xFF) / k255;
    float fb = static_cast<float>((b >> shift) & 0xFF) / k255;
    return static_cast<int32_t>((fa * oneMinusT + fb * t) * k255);
}

// The packed result is R << 16 | G << 8 | B: the alpha byte comes out 0.
inline uint32_t LerpColor(uint32_t a, uint32_t b, float oneMinusT, float t) {
    uint32_t r = static_cast<uint32_t>(LerpChannel(a, b, 16, oneMinusT, t));
    uint32_t g = static_cast<uint32_t>(LerpChannel(a, b, 8, oneMinusT, t));
    uint32_t bl = static_cast<uint32_t>(LerpChannel(a, b, 0, oneMinusT, t));
    return ((r << 8 | g) << 8) | bl;
}

// 0x1400949f0 -- retail's EnvironmentSetting member `Lerp(const other&, float t,
// bool includeSun) const`, returned by value (RCX this, RDX the return slot, R8
// the other end, XMM3 t, the bool on the stack). The record is
// EnvironmentDescription::WeatherEntry, whose header is not this block's file,
// so the member is spelled here as a file-local function (DRAW hand-in).
// Retail first stores the record's defaults into the return slot; every field
// is then overwritten, so those stores are not spelled. The colours are written
// fog, light, ambient; then the three floats; the sun pair is lerped only with
// includeSun, else copied from `a`.
EnvironmentSetting LerpEnvironmentSetting(const EnvironmentSetting& a, const EnvironmentSetting& b,
                                          float t, bool includeSun) {
    const float oneMinusT = kRdata_14017214c - t;   // 1.0f - t
    EnvironmentSetting out;
    out.fogColor    = LerpColor(a.fogColor, b.fogColor, oneMinusT, t);
    out.groundColor = LerpColor(a.groundColor, b.groundColor, oneMinusT, t);
    out.skyColor    = LerpColor(a.skyColor, b.skyColor, oneMinusT, t);
    out.fogStart     = oneMinusT * a.fogStart + t * b.fogStart;
    out.fogEnd       = oneMinusT * a.fogEnd + t * b.fogEnd;
    out.sunIntensity = oneMinusT * a.sunIntensity + t * b.sunIntensity;
    if (includeSun) {
        out.sunAngle  = oneMinusT * a.sunAngle + t * b.sunAngle;
        out.sunSpread = oneMinusT * a.sunSpread + t * b.sunSpread;
    } else {
        out.sunAngle  = a.sunAngle;
        out.sunSpread = a.sunSpread;
    }
    return out;
}

// One packed colour to {r, g, b, alpha} with each channel / 255.
inline Math::Vector4 UnpackColor(uint32_t c, float alpha) {
    const float k255 = BitsF(kRdata_14017222c);
    return Math::Vector4(static_cast<float>((c >> 16) & 0xFF) / k255,
                         static_cast<float>((c >> 8) & 0xFF) / k255,
                         static_cast<float>(c & 0xFF) / k255,
                         alpha);
}

}  // namespace

// 0x140093ac0 -- the frame's lighting: the two monthly records around the year
// time, lerped with the sun pair; then, when weather is shown, every live
// effect's Weather lighting lerped in by its fade, the sun pair kept.
EnvironmentSetting* WeatherSimulation::BlendLighting(EnvironmentSetting* out) const {
    Calendar* calendar = GameUpdateLoop::_singleton->m_dayCounter;   // +0x128
    const float seconds = calendar->yearTime.AsSeconds();
    int month = static_cast<int>((seconds / BitsF(kRdata_14017225c)) * BitsF(kRdata_1401721b8));
    if (month >= 12) month = 0;
    const float t = (seconds - static_cast<float>(month * 300)) / kRdata_140172234;
    *out = LerpEnvironmentSetting(environment->_months[month],
                                  environment->_months[(month + 1) % 12], t, true);

    if (g_gameSettings->showWeather) {   // GameSettings +0x16
        for (int i = 0; i < effects.GetSize(); ++i) {
            const WeatherParticleGeometry* effect =
                static_cast<const WeatherParticleGeometry*>(effects._data[i]);
            if (effect == nullptr) continue;   // retail's own test: a disabled slot is null
            const Weather* weather = effect->_descriptor;   // +0xC8
            EnvironmentSetting setting;
            setting.skyColor     = weather->ambientColor._value;   // +0x60
            setting.groundColor  = weather->lightColor._value;     // +0x64
            setting.fogColor     = weather->fogColor._value;       // +0x68
            setting.fogStart     = weather->fogNear;               // +0x6C
            setting.fogEnd       = weather->fogFar;                // +0x70
            setting.sunIntensity = weather->fogMax;                // +0x74
            setting.sunAngle     = weather->sunHeading;            // +0x78
            setting.sunSpread    = weather->sunPitch;              // +0x7C
            *out = LerpEnvironmentSetting(*out, setting, effect->_intensity, false);   // +0xC0
        }
    }
    return out;
}

// 0x1400938d0
void WeatherSimulation::DrawUpdate(Video::Context& context) {
    EnvironmentSetting setting;
    BlendLighting(&setting);
    Graphics::SunLight* sun = GameUpdateLoop::_singleton->m_sunLight;   // +0x100

    // The four terrain weather floats, to +0x3B8 and to pixel constant 4 (+0x288).
    context._unknown03b8.x = field_0x1C.x;
    context._unknown03b8.y = field_0x1C.y;
    context._unknown03b8.z = field_0x1C.z;
    context._unknown03b8.w = field_0x1C.w;
    context._pixelConstantStaging.pixel[4].x = field_0x1C.x;
    context._pixelConstantStaging.pixel[4].y = field_0x1C.y;
    context._pixelConstantStaging.pixel[4].z = field_0x1C.z;
    context._pixelConstantStaging.pixel[4].w = field_0x1C.w;
    context._dirty |= 0x10800;

    // The fog colour's fourth channel is the record's fog maximum.
    context.SetFog(setting.fogStart, setting.fogEnd,
                   UnpackColor(setting.fogColor, setting.sunIntensity));

    const Math::Vector4 ambient = UnpackColor(setting.skyColor, 1.0f);
    const Math::Vector4 light   = UnpackColor(setting.groundColor, 1.0f);
    const Math::Scalar heading(setting.sunAngle);
    const Math::Scalar pitch(-setting.sunSpread);   // XORPS with the sign mask 0x1401722c0
    sun->Set(Math::Vector3::FromYawPitch(heading, pitch), light, ambient);
}
