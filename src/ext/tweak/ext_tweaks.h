// ============================================================================
// src/ext/tweak/ext_tweaks.h -- the live-tunable constants of the graphics extensions
// ============================================================================
// NOT RETAIL. DEBUG FLAVOUR ONLY (CMake option BANISHED_TWEAK_OVERLAY; block TWEAKUI,
// docs/reference/tweak_overlay.md). Without the option this header is empty and
// ext_tweaks.cpp is not compiled: the extensions read their constexpr constants as before.
//
// The values the debug tweak overlay (src/diag/tweak_overlay.cpp) moves that have no
// settings key: ext.ambient_occlusion's ASSAO parameters and fog fade, ext.foliage_aa's
// distance-fill k and floor. Defaults = the constants the faithful-to-today code uses
// (ao_math.h, foliage_aa.h), so an untouched overlay changes nothing.
//
// Readers: AoMath::FrameFor (main thread, once per frame), AoMath::FadeFromFog (the AO
// pass: the render thread under ext.render_pipe), Ext::FoliageAA::Sync (main thread).
// Writer: the overlay, on the presenting thread. A copy under a mutex each way, so no
// reader sees a half-written struct. The row-off paths never read it.
// ============================================================================
#pragma once

#if BANISHED_TWEAK_OVERLAY

#include "ext/ambient_occlusion/ao_math.h"
#include "ext/foliage_aa/foliage_aa.h"

namespace Ext {
namespace Tweak {

struct Ao {
    float radiusStreet          = AoMath::kRadiusStreet;     // world units at the closest zoom
    float radiusOverview        = AoMath::kRadiusOverview;   // world units at the overview
    float shadowMultiplier      = AoMath::kShadowMultiplier; // x ext.ao_opacity / 100
    float shadowPower           = AoMath::kShadowPower;
    float horizonAngleThreshold = AoMath::kHorizonAngleThreshold;
    float sharpness             = AoMath::kSharpness;
    float detailShadowStrength  = AoMath::kDetailShadowStrength;
    int   blurPasses            = -1;     // -1 = the tier's (AoMath::TierFor), else 0..6
    bool  fogFade               = true;   // false: the zoom fade only (FadeFromFog not applied)
    float fogFromScale          = 1.0f;   // FadeOutFrom = fogNear x this
    float fogToScale            = 1.0f;   // FadeOutTo   = fogFar  x this
    float fadeCeiling           = AoMath::kFogFadeCeiling;   // FadeOutTo never past this (the sky at 512)
};

struct Faa {
    float fillK     = FoliageAA::kDistanceFillK;
    float fillFloor = FoliageAA::kDistanceFillFloor;
};

Ao   AoValues();
void SetAo(const Ao& v);
Faa  FaaValues();
void SetFaa(const Faa& v);

}  // namespace Tweak
}  // namespace Ext

#endif  // BANISHED_TWEAK_OVERLAY
