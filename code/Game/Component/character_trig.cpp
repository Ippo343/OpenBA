// code/Game/Component/character_trig.cpp
// CharacterComponent transcendental helpers used by the move-tick TAIL
// (CharacterComponent_MoveTick_1094c0 heading quaternion).
//
//   0x140131c90  FUN_140131c90                  — sinf (IEEE arg-reduced)
//   0x140131090  CharacterComponent_helper_1090 — cosf (IEEE arg-reduced)
//
// The originals are the MSVC CRT sinf/cosf: the float is widened to DOUBLE, reduced
// by pi/2, run through a double minimax polynomial and rounded back to float. The
// first port forwarded to MinGW's sinf/cosf, which go through the x87 LONG DOUBLE
// __sinl_internal/__cosl_internal — the sampling profiler (BANISHED_SAMPLE) put those
// two calls at 16.7% of the whole EntityList phase at 800 citizens x10 (one sin + one
// cos per character per tick). This file now does what the CRT does, in double, and
// computes both from ONE argument reduction (the move tick always needs the pair).
// Accuracy is the same class as the original (double polynomial, |err| ~1 ulp float).
// BANISHED_FAST_TRIG=0 restores the libm calls for A/B.
//

#include <cmath>
#include "options.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

bool FastTrigOn() {
    static int on = -1;
    if (on < 0) {
        const char* e = Opt().fastTrig;
        on = (e && e[0] == '0') ? 0 : 1;
    }
    return on == 1;
}

// Cephes-style double polynomials, valid on |r| <= pi/4.
inline double SinPoly(double r) {
    const double z = r * r;
    return r + r * z * (-1.66666666666666307295e-1 + z * (8.33333333332211858878e-3
           + z * (-1.98412698295895385996e-4 + z * (2.75573136213857245213e-6
           + z * (-2.50507477628578072866e-8 + z * 1.58962301576546568060e-10)))));
}
inline double CosPoly(double r) {
    const double z = r * r;
    return 1.0 - 0.5 * z + z * z * (4.16666666666665929218e-2 + z * (-1.38888888888730564116e-3
           + z * (2.48015872888517045348e-5 + z * (-2.75573141792967388112e-7
           + z * (2.08757008419747316778e-9 + z * -1.13585365213876817300e-11)))));
}

// Both sin and cos of x from one reduction. Returns false for inf/nan (caller falls back).
inline bool SinCosD(float xf, float* s, float* c) {
    const uint32_t bits = [&] { uint32_t b; std::memcpy(&b, &xf, 4); return b; }();
    if ((bits & 0x7f800000u) == 0x7f800000u) return false;
    const double x = (double)xf;
    // k = round(x / (pi/2)); r = x - k*pi/2 (two-part constant keeps r exact to ~1e-16
    // for the |x| < 1e5 the heading math ever produces).
    // Round to nearest with plain arithmetic: std::nearbyint is a libcall on MinGW and
    // cost as much as the polynomials (12% of the EntityList phase in the sampler).
    const double kd = x * 6.36619772367581382433e-1;
    const int64_t ki = (int64_t)(kd + (kd >= 0.0 ? 0.5 : -0.5));
    const double k = (double)ki;
    double r = x - k * 1.57079632673412561417e+0;
    r -= k * 6.07710050650619224932e-11;
    const double sr = SinPoly(r), cr = CosPoly(r);
    switch (ki & 3) {
        case 0:  *s = (float)sr;  *c = (float)cr;  break;
        case 1:  *s = (float)cr;  *c = (float)-sr; break;
        case 2:  *s = (float)-sr; *c = (float)-cr; break;
        default: *s = (float)-cr; *c = (float)sr;  break;
    }
    return true;
}

} // namespace

extern "C" void CharacterComponent_SinCosf(float x, float* s, float* c) {
    if (FastTrigOn() && SinCosD(x, s, c)) return;
    *s = sinf(x);
    *c = cosf(x);
}
extern "C" float CharacterComponent_Sinf_131c90(float x) {
    float s, c;
    if (FastTrigOn() && SinCosD(x, &s, &c)) return s;
    return sinf(x);
}
extern "C" float CharacterComponent_Cosf_131090(float x) {
    float s, c;
    if (FastTrigOn() && SinCosD(x, &s, &c)) return c;
    return cosf(x);
}
