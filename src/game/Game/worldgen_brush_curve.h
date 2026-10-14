// ============================================================================
// src/game/Game/worldgen_brush_curve.h -- an exact memo of the eee0 brush falloff
// ============================================================================
// NOT RETAIL (block WGPAR, docs/extensions/handins/WGPAR.md). A faithful-path speedup
// for the radial falloff brush 0x1400ceee0, which the port carries twice:
// wg_StampBrush_eee0 (worldgen_f390_rivers.cpp, the e910 stamp pass) and
// wg_eee0_radial_brush (worldgen_f390_erosion.cpp, the ec50 carve pass).
//
// THE CURVE. For a brush (falloff base B, cutoff C) the weight of a cell at integer
// offset (dx, dy) from the brush centre is
//     w(s) = 1 / (pow(B, -(sqrt((float)s) - C)) + 1),   s = dx*dx + dy*dy
// and the scan half-extent `span` comes from a radius-growth loop over pow(B, -(r - C))
// alone. Both depend ONLY on (B, C) and the integer s -- not on the centre, the mask,
// the strength or the mode. The brushes spent ~90% of their time in pow, evaluating
// the same w(s) for every symmetric cell (8 per s off-axis) and, along a path, for
// every one of its ~1000-2000 points (cc850 / ranges TryPlacePath stamp the same
// (strength, inner, outer) attributes into every point of a path).
//
// EXACTNESS. pow and sqrt are deterministic functions of their argument bits, and the
// memo evaluates w(s) with the same pow / sqrt functions (the brush file passes its
// own wg_powf / wg_sqrtf), the same sign-bit negate, the same float expression and the
// same operation order as the brush body did. The key is the BIT PATTERN of (B, C), so
// two brushes share an entry only when every input to the curve is identical. The
// stored float is therefore exactly the value the old body computed for that cell,
// and the brush's own min/max blend against the mask is unchanged.
// (The build has no FMA (-march is the x86-64 baseline) and x86-64 floats carry no
// excess precision, so the same expression rounds the same wherever it is compiled.)
//
// Weights are filled lazily per s (a brush whose (B, C) is unique -- river points and
// satellites have random radii -- still evaluates each distinct s once instead of up
// to eight times). A few entries are kept (the e910 / ec50 path pass alternates the
// inner and the outer brush of the same point).
// ============================================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

class WgBrushCurveCache {
public:
    using PowFn  = float (*)(float base, float exp);
    using SqrtFn = float (*)(float v);

    struct Curve {
        uint32_t baseBits   = 0;
        uint32_t cutoffBits = 0;
        bool     valid      = false;
        float    base   = 0.0f;
        float    cutoff = 0.0f;
        float    r      = 0.0f;   // the grown radius (float, as the brush keeps it)
        int      span   = 0;      // (int)r + 1: the scan half-extent
        std::vector<float>   w;      // w[s], s in [0, 2*span*span]
        std::vector<uint8_t> have;   // have[s] != 0 once w[s] is computed
    };

    WgBrushCurveCache(PowFn pw, SqrtFn sq, float one, float radiusThreshold)
        : _pow(pw), _sqrt(sq), _one(one), _thr(radiusThreshold) {}

    // The curve of brush (base, cutoff): span computed, weights lazily.
    Curve& Get(float base, float cutoff) {
        const uint32_t bb = Bits(base), cb = Bits(cutoff);
        for (Curve& c : _entries)
            if (c.valid && c.baseBits == bb && c.cutoffBits == cb) return c;
        Curve& c = _entries[_next];
        _next = (_next + 1) % kEntries;
        c.baseBits = bb;
        c.cutoffBits = cb;
        c.valid = true;
        c.base = base;
        c.cutoff = cutoff;
        // The brush's step 1, verbatim: grow r until the on-axis falloff reaches 0.99.
        float r = _one;
        float fall = _pow(base, Negate(_one - cutoff));
        while (_one / (fall + _one) < _thr) {
            r += _one;
            fall = _pow(base, Negate(r - cutoff));
        }
        c.r = r;
        c.span = (int)r + 1;
        const size_t n = (size_t)2 * (size_t)c.span * (size_t)c.span + 1;
        c.w.resize(n);
        c.have.assign(n, 0);
        return c;
    }

    // w(s) for s = dx*dx + dy*dy, |dx|, |dy| <= span: the brush's per-cell expression.
    float Weight(Curve& c, int s) {
        if (!c.have[(size_t)s]) {
            const float d = _sqrt((float)s);
            float f = _pow(c.base, Negate(d - c.cutoff));
            f = _one / (f + _one);
            c.w[(size_t)s] = f;
            c.have[(size_t)s] = 1;
        }
        return c.w[(size_t)s];
    }

    // The exe's `(uint)x ^ 0x80000000` float negate.
    static float Negate(float v) {
        uint32_t bits;
        std::memcpy(&bits, &v, 4);
        bits ^= 0x80000000u;
        float out;
        std::memcpy(&out, &bits, 4);
        return out;
    }

private:
    static constexpr int kEntries = 4;

    static uint32_t Bits(float v) {
        uint32_t b;
        std::memcpy(&b, &v, 4);
        return b;
    }

    PowFn  _pow;
    SqrtFn _sqrt;
    float  _one;
    float  _thr;
    Curve  _entries[kEntries];
    int    _next = 0;
};
