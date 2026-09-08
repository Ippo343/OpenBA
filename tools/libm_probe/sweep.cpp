// tools/libm_probe/sweep.cpp -- bit-exactness oracle for runtime/src/crt/msvc_libm.cpp
//
// Compares OUR ported bodies against Runtime-x64.dll's own CRT bodies at the
// five retail addresses, over a large sweep, on RAW 32-BIT PATTERNS -- NaN
// payloads and signed zeros included, no float == anywhere.
//
// Three things are reported and all three are needed:
//   1. n / diff per function            -- the result.
//   2. an EFFECTIVENESS CONTROL         -- how many cases actually entered each
//      branch of the ported body, counted BY THE BODY ITSELF (MSVC_LIBM_COVERAGE),
//      not re-derived here. A sweep that never reaches argument reduction
//      reports "0 diff" exactly as loudly as one that does.
//   3. a NEGATIVE CONTROL               -- the same sweep re-run with MinGW's
//      libm as "ours". It MUST report a large diff count. If it does not, the
//      harness is comparing the DLL against itself and the green line is worth
//      nothing.
//
// Build:
//   g++ -std=c++17 -O2 -DMSVC_LIBM_COVERAGE -I../../runtime/src/crt \
//       sweep.cpp ../../runtime/src/crt/msvc_libm.cpp -o sweep.exe
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include "msvc_libm.h"

typedef float (*Unary)(float);
typedef float (*Binary)(float, float);
static Unary  dllSqrt, dllSin, dllCos;
static Binary dllAtan2, dllPow;

static inline uint32_t B(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static inline float    F(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }

// The two implementations under test. Mode 0 is the port; mode 1 is MinGW's
// libm, which is the negative control (and was the shipping default before
// this lane).
static float StdSqrt(float v)           { return std::sqrt(v); }
static float StdSin(float v)            { return std::sin(v); }
static float StdCos(float v)            { return std::cos(v); }
static float StdAtan2(float y, float x) { return std::atan2(y, x); }
static float StdPow(float b, float e)   { return std::pow(b, e); }

struct Impl { const char* label; Unary sq, si, co; Binary at, pw; };
static const Impl kImpls[2] = {
    {"PORT (runtime/src/crt/msvc_libm.cpp)",
     MsvcLibm::Sqrtf, MsvcLibm::Sinf, MsvcLibm::Cosf, MsvcLibm::Atan2f, MsvcLibm::Powf},
    {"NEGATIVE CONTROL: MinGW/UCRT libm",
     StdSqrt, StdSin, StdCos, StdAtan2, StdPow},
};

struct Stat {
    const char* name = "";
    uint64_t n = 0, diff = 0;
    int64_t  maxUlp = 0;
    uint32_t wx = 0, wy = 0, wa = 0, wb = 0;
    std::vector<std::pair<uint32_t, uint32_t>> samples;
};

static int64_t UlpDiff(uint32_t ua, uint32_t ub) {
    auto mono = [](uint32_t u) -> int64_t {
        return (u & 0x80000000u) ? (int64_t)0x80000000LL - (int64_t)(u & 0x7fffffffu)
                                 : (int64_t)u;
    };
    int64_t d = mono(ua) - mono(ub);
    return d < 0 ? -d : d;
}

static void Cmp1(Stat& s, Unary ours, Unary theirs, float x) {
    const uint32_t ua = B(ours(x)), ub = B(theirs(x));
    s.n++;
    if (ua == ub) return;
    s.diff++;
    const int64_t u = UlpDiff(ua, ub);
    if (u > s.maxUlp) { s.maxUlp = u; s.wx = B(x); s.wa = ua; s.wb = ub; }
    if (s.samples.size() < 6) s.samples.push_back({B(x), 0});
}
static void Cmp2(Stat& s, Binary ours, Binary theirs, float x, float y) {
    const uint32_t ua = B(ours(x, y)), ub = B(theirs(x, y));
    s.n++;
    if (ua == ub) return;
    s.diff++;
    const int64_t u = UlpDiff(ua, ub);
    if (u > s.maxUlp) { s.maxUlp = u; s.wx = B(x); s.wy = B(y); s.wa = ua; s.wb = ub; }
    if (s.samples.size() < 6) s.samples.push_back({B(x), B(y)});
}

static const uint32_t kSpecials[] = {
    0x00000000u, 0x80000000u,              // +-0
    0x3f800000u, 0xbf800000u,              // +-1
    0x40000000u, 0xc0000000u,              // +-2
    0x3f000000u, 0x40400000u,              // 0.5, 3
    0x40490fdbu, 0xc0490fdbu,              // +-pi
    0x3fc90fdbu, 0xbfc90fdbu,              // +-pi/2
    0x3f490fdbu, 0xbf490fdbu,              // +-pi/4
    0x7f800000u, 0xff800000u,              // +-inf
    0x7fc00000u, 0xffc00000u,              // quiet NaN
    0x7fa00000u, 0xffa00000u,              // signalling NaN
    0x00800000u, 0x80800000u,              // +-FLT_MIN
    0x00000001u, 0x80000001u,              // +-denormal min
    0x007fffffu, 0x00400000u,              // denormal max, mid denormal
    0x7f7fffffu, 0xff7fffffu,              // +-FLT_MAX
    0x4b800000u, 0x4f000000u, 0x2e800000u, // 2^24, 2^31, 2^-34
    0x497423f0u, 0x501502f9u, 0x5cbf0000u, // large args -> Payne-Hanek
    0x4f000001u, 0x2e7fffffu,              // just across the pow thresholds
    0x3f7fffffu, 0x3f800001u,              // just below/above 1.0
};

static int RunSweep(const Impl& impl, bool quick, bool exhaustive) {
    Stat sq; sq.name = "sqrtf"; Stat si; si.name = "sinf"; Stat co; co.name = "cosf";
    Stat at; at.name = "atan2f"; Stat pw; pw.name = "powf";
    const uint32_t mStep  = quick ? 0x2000u : 0x40u;
    const uint32_t mStep2 = quick ? 0x8000u : 0x100u;

    // Phase 1: dense over the whole finite float exponent range, both signs.
    for (uint32_t e = 1; e <= 254; ++e)
        for (uint32_t m = 0; m < 0x800000u; m += mStep)
            for (uint32_t sgn = 0; sgn < 2; ++sgn) {
                const float x = F((sgn << 31) | (e << 23) | m);
                Cmp1(sq, impl.sq, dllSqrt, x);
                Cmp1(si, impl.si, dllSin, x);
                Cmp1(co, impl.co, dllCos, x);
            }

    // Phase 2: the subnormal band.
    for (uint32_t m = 1; m < 0x800000u; m += mStep)
        for (uint32_t sgn = 0; sgn < 2; ++sgn) {
            const float x = F((sgn << 31) | m);
            Cmp1(sq, impl.sq, dllSqrt, x);
            Cmp1(si, impl.si, dllSin, x);
            Cmp1(co, impl.co, dllCos, x);
        }

    // Phase 3: atan2 / pow over a 2D grid across the exponent range, all four
    // sign quadrants.
    for (uint32_t ex = 1; ex <= 254; ex += (quick ? 17 : 3))
        for (uint32_t mx = 0; mx < 0x800000u; mx += mStep2 * 8) {
            const float x = F((ex << 23) | mx);
            for (uint32_t ey = 1; ey <= 254; ey += (quick ? 23 : 7))
                for (uint32_t my = 0; my < 0x800000u; my += 0x155555u) {
                    const float y = F((ey << 23) | my);
                    for (int s = 0; s < 4; ++s) {
                        const float xx = (s & 1) ? -x : x;
                        const float yy = (s & 2) ? -y : y;
                        Cmp2(at, impl.at, dllAtan2, yy, xx);
                        Cmp2(pw, impl.pw, dllPow, xx, yy);
                    }
                }
        }

    // Phase 4: pow with integral and quarter-integral exponents -- the
    // odd/even-integer sign rule and the exp2 table's j index.
    for (int k = -80; k <= 80; ++k)
        for (int half = 0; half < 4; ++half) {
            const float y = (float)k + 0.25f * (float)half;
            for (uint32_t ex = 90; ex <= 160; ++ex)
                for (uint32_t mx = 0; mx < 0x800000u; mx += 0x111111u) {
                    const float x = F((ex << 23) | mx);
                    Cmp2(pw, impl.pw, dllPow, x, y);
                    Cmp2(pw, impl.pw, dllPow, -x, y);
                }
        }

    // Phase 5: special values, exhaustively crossed.
    const int NS = (int)(sizeof(kSpecials) / sizeof(kSpecials[0]));
    for (int i = 0; i < NS; ++i) {
        const float x = F(kSpecials[i]);
        Cmp1(sq, impl.sq, dllSqrt, x);
        Cmp1(si, impl.si, dllSin, x);
        Cmp1(co, impl.co, dllCos, x);
        for (int j = 0; j < NS; ++j) {
            const float y = F(kSpecials[j]);
            Cmp2(at, impl.at, dllAtan2, x, y);
            Cmp2(pw, impl.pw, dllPow, x, y);
        }
    }

    // Phase 6: sin/cos EXHAUSTIVE over the band the game actually feeds them
    // (|x| in [2^-4, 2^12] radians) -- every representable float, both signs.
    if (exhaustive)
        for (uint32_t e = 123; e <= 139; ++e)
            for (uint32_t m = 0; m < 0x800000u; ++m)
                for (uint32_t sgn = 0; sgn < 2; ++sgn) {
                    const float x = F((sgn << 31) | (e << 23) | m);
                    Cmp1(si, impl.si, dllSin, x);
                    Cmp1(co, impl.co, dllCos, x);
                }

    std::printf("\n--- %s ---\n", impl.label);
    Stat* all[] = {&sq, &si, &co, &at, &pw};
    int diffs = 0;
    for (Stat* s : all) {
        std::printf("%-8s n=%-12llu diff=%-12llu", s->name,
                    (unsigned long long)s->n, (unsigned long long)s->diff);
        if (!s->diff) { std::printf("BIT-EXACT\n"); continue; }
        diffs++;
        std::printf("(%.6f%%) maxUlp=%lld  worst x=0x%08x y=0x%08x ours=0x%08x dll=0x%08x\n",
                    100.0 * (double)s->diff / (double)s->n, (long long)s->maxUlp,
                    s->wx, s->wy, s->wa, s->wb);
    }
    return diffs;
}

int main(int argc, char** argv) {
    const bool quick = (argc > 1 && std::strcmp(argv[1], "--quick") == 0);
    HMODULE h = LoadLibraryA("C:/GOG Games/Banished/Runtime-x64.dll");
    if (!h) { std::printf("LoadLibrary failed %lu\n", (unsigned long)GetLastError()); return 2; }
    const uintptr_t base = (uintptr_t)h;
    auto R = [&](uintptr_t g) { return (void*)(base + (g - 0x180000000ull)); };
    dllSqrt  = (Unary)R(0x180128110);
    dllSin   = (Unary)R(0x180127d90);
    dllCos   = (Unary)R(0x180126d10);
    dllAtan2 = (Binary)R(0x180126680);
    dllPow   = (Binary)R(0x180127560);

    const int portDiffs = RunSweep(kImpls[0], quick, !quick);

#ifdef MSVC_LIBM_COVERAGE
    const MsvcLibm::Coverage& c = MsvcLibm::g_cov;
    std::printf("\n=== EFFECTIVENESS CONTROL (branch entries counted inside the port) ===\n");
#define SHOW(f) std::printf("  %-22s %llu\n", #f, (unsigned long long)c.f)
    std::printf("sqrtf:\n");   SHOW(sqrtHardware); SHOW(sqrtNegative); SHOW(sqrtNaN);
                               SHOW(sqrtPosInf); SHOW(sqrtNegInf);
    std::printf("sinf:\n");    SHOW(sinSmallPoly); SHOW(sinCubicTerm); SHOW(sinIdentity);
                               SHOW(sinNegArg); SHOW(sinCodyWaite); SHOW(sinExtendedSplit);
                               SHOW(sinPayneHanek); SHOW(sinReducedTiny); SHOW(sinReducedSmall);
                               SHOW(sinFullSinSeries); SHOW(sinFullCosSeries);
                               SHOW(sinQuadrantNegate);
    std::printf("cosf:\n");    SHOW(cosSmall); SHOW(cosNegArg); SHOW(cosCodyWaite);
                               SHOW(cosExtendedSplit); SHOW(cosPayneHanek);
                               SHOW(cosReducedTiny); SHOW(cosReducedSmall);
                               SHOW(cosFullSinSeries); SHOW(cosFullCosSeries);
                               SHOW(cosQuadrantNegate);
    std::printf("__rem_pio2:\n"); SHOW(remPio2); SHOW(remPio2RoundUp); SHOW(remPio2ExtraLimb);
    std::printf("trig non-finite:\n"); SHOW(trigNaN); SHOW(trigInf);
    std::printf("atan2f:\n");  SHOW(atanTablePath); SHOW(atanPolyPath); SHOW(atanTinyRatio);
                               SHOW(atanSwap); SHOW(atanNegX); SHOW(atanSubnormal);
                               SHOW(atanPlainDivide); SHOW(atanRatioHuge); SHOW(atanRatioZero);
                               SHOW(atanNearPi); SHOW(atanBothInf); SHOW(atanZeroY);
                               SHOW(atanZeroXNegY); SHOW(atanNanX); SHOW(atanNanY);
    std::printf("powf:\n");    SHOW(powCore); SHOW(powOverflow); SHOW(powUnderflow);
                               SHOW(powExpZero); SHOW(powExpOne); SHOW(powExpTiny);
                               SHOW(powExpHuge); SHOW(powNegBase); SHOW(powNegBaseOddInt);
                               SHOW(powNegBaseEvenInt); SHOW(powNegBaseNonInt);
                               SHOW(powBaseOne); SHOW(powBaseZero); SHOW(powBaseMinusOne);
                               SHOW(powBaseInfNan);
#undef SHOW
#endif

    // The negative control runs the SHORT form of the same sweep: its job is to
    // prove the comparison can see a defect at all, not to characterise MinGW.
    std::printf("\n=== NEGATIVE CONTROL ===\n");
    const int ctlDiffs = RunSweep(kImpls[1], true, false);
    if (ctlDiffs == 0) {
        std::printf("\nHARNESS FAILURE: the negative control found no differences either.\n"
                    "The sweep is not actually comparing two implementations; the PORT\n"
                    "result above proves nothing.\n");
        return 3;
    }
    std::printf("\nNegative control OK: %d of 5 functions differ under MinGW's libm,\n"
                "so the sweep can see a last-bit defect when one is present.\n", ctlDiffs);

    std::printf("\n=== VERDICT: port %s ===\n",
                portDiffs == 0 ? "BIT-EXACT on all five" : "HAS DIVERGENCES");
    return portDiffs == 0 ? 0 : 1;
}
