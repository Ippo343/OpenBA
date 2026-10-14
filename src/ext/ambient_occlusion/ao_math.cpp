// ============================================================================
// src/ext/ambient_occlusion/ao_math.cpp -- ao_math.h
// ============================================================================
// NOT RETAIL. ext.ambient_occlusion (docs/extensions/handins/AOCORE.md).
// ============================================================================
#include "ext/ambient_occlusion/ao_math.h"

#include "ext/ambient_occlusion/params.h"
#include "graphics/renderer_ext_ao.h"

namespace AoMath {

bool TierFor(int quality, bool lowIsLowest, Tier& out) {
    switch (quality) {
    case 1: out = {lowIsLowest ? kAssaoLowest : kAssaoLow, 1}; return true;
    case 2: out = {kAssaoMedium, 2}; return true;
    case 3: out = {kAssaoHigh, 2}; return true;
    default: return false;
    }
}

namespace {
float Lerp(float a, float b, float t) { return a + (b - a) * t; }
}  // namespace

Zoom ZoomFor(float d) {
    float t = (d - kZoomMin) / (kZoomMax - kZoomMin);
    if (!(t > 0.0f)) t = 0.0f;   // also NaN -> street
    if (t > 1.0f) t = 1.0f;
    Zoom z;
    z.t           = t;
    z.radius      = Lerp(kRadiusStreet, kRadiusOverview, t);
    z.fadeOutFrom = Lerp(kFadeFromStreet, kFadeFromOverview, t);
    z.fadeOutTo   = Lerp(kFadeToStreet, kFadeToOverview, t);
    return z;
}

void AssaoProjection(const float in[16], float out[16]) {
    for (int c = 0; c < 4; ++c) {
        out[0 * 4 + c] = in[0 * 4 + c];
        out[1 * 4 + c] = in[2 * 4 + c];
        out[2 * 4 + c] = in[1 * 4 + c];
        out[3 * 4 + c] = in[3 * 4 + c];
    }
}

bool LinearizeConsts(const float m[16], float& mul, float& add) {
    int row = -1;
    for (int r = 0; r < 3; ++r)
        if (m[r * 4 + 3] != 0.0f) row = r;
    if (row < 0) return false;
    add = m[row * 4 + 2] / m[row * 4 + 3];
    mul = -m[3 * 4 + 2];
    return true;
}

void AssaoUnpack(const float lh[16], float& mul, float& add, float& tanX, float& tanY) {
    mul = -lh[3 * 4 + 2];
    add = lh[2 * 4 + 2];
    if (mul * add < 0.0f) add = -add;
    tanX = 1.0f / lh[0 * 4 + 0];
    tanY = 1.0f / lh[1 * 4 + 1];
}

bool FrameFor(const Ext::AO::Params& p, float cameraDistance, Graphics::ExtAoFrame& out) {
    Tier tier;
    if (!TierFor(p.quality, kLowIsLowest, tier)) return false;
    int opacity = p.opacity;
    if (opacity <= 0) return false;
    if (opacity > 100) opacity = 100;
    const Zoom z = ZoomFor(cameraDistance);
    out.qualityLevel          = tier.qualityLevel;
    out.blurPassCount         = tier.blurPassCount;
    out.radius                = z.radius;
    out.shadowMultiplier      = kShadowMultiplier * static_cast<float>(opacity) / 100.0f;
    out.shadowPower           = 1.5f;    // ASSAO_Settings defaults below; AOTUNE owns them
    out.horizonAngleThreshold = 0.06f;
    out.sharpness             = 0.98f;
    out.detailShadowStrength  = 0.5f;
    out.fadeOutFrom           = z.fadeOutFrom;
    out.fadeOutTo             = z.fadeOutTo;
    return true;
}

}  // namespace AoMath
