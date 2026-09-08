// Exhaustive check: is the sinReducedTiny / cosReducedTiny branch (|y| below
// 2^-33 after Cody-Waite reduction) reachable AT ALL from a float argument?
//
// The branch lives in the band pi/4 < |x| < 500000 where sinf/cosf reduce by
// pi/2 rather than calling __rem_pio2. This scans EVERY float in that band,
// replicating the port's reduction exactly (extended split included), and
// reports the smallest reduced exponent any of them produces.
#include <cstdio>
#include <cstdint>
#include <cstring>

static inline uint32_t B(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static inline float    F(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }
static inline uint64_t U64(double d) { uint64_t u; std::memcpy(&u, &d, 8); return u; }
static inline double   F64(uint64_t u) { double d; std::memcpy(&d, &u, 8); return d; }

int main() {
    const double twoOverPi = F64(0x3fe45f306dc9c883ull);
    const double half      = F64(0x3fe0000000000000ull);
    const double pio2Hi    = F64(0x3ff921fb54400000ull);
    const double pio2Mid   = F64(0x3dd0b4611a626331ull);
    const double pio2Mid2  = F64(0x3dd0b4611a600000ull);
    const double pio2Lo    = F64(0x3ba3198a2e037073ull);

    int64_t  minExp = 0x7ff;
    uint32_t minArg = 0;
    uint64_t nBand = 0, nTiny = 0, nSmall = 0, nSplit = 0;

    // pi/4 in double is 0x3fe921fb54442d18; the band top is 500000.0.
    for (uint32_t e = 126; e <= 145; ++e) {
        for (uint32_t m = 0; m < 0x800000u; ++m) {
            const uint32_t bits = (e << 23) | m;
            const float xf = F(bits);
            const double x = (double)xf;
            const uint64_t r10 = U64(x) & 0x7fffffffffffffffull;
            if (r10 <= 0x3fe921fb54442d18ull) continue;      // |x| <= pi/4
            if (r10 >= 0x411e848000000000ull) continue;      // |x| >= 500000
            ++nBand;

            double t = x * twoOverPi;
            t = t + half;
            const int64_t expx = (int64_t)(r10 >> 52);
            const int32_t n = (int32_t)t;                     // cvttsd2si
            const double fn = (double)n;
            double r = x - pio2Hi * fn;
            double w = pio2Mid * fn;
            double y = r - w;
            int64_t expy = (int64_t)((U64(y) << 1) >> 53);
            if (expx - expy > 15) {
                ++nSplit;
                double r0 = r;
                const double w1 = pio2Mid2 * fn;
                double w2 = pio2Lo * fn;
                r  = r - w1;
                r0 = r0 - r;
                r0 = r0 - w1;
                w2 = w2 - r0;
                y  = r - w2;
                expy = (int64_t)((U64(y) << 1) >> 53);
            }
            if (expy < minExp) { minExp = expy; minArg = bits; }
            if (expy <= 0x3de) ++nTiny;
            else if (expy < 0x3f2) ++nSmall;
        }
    }
    std::printf("floats in the Cody-Waite band : %llu\n", (unsigned long long)nBand);
    std::printf("extended-split refinements     : %llu\n", (unsigned long long)nSplit);
    std::printf("reducedSmall (0x3df..0x3f1)    : %llu\n", (unsigned long long)nSmall);
    std::printf("reducedTiny  (<= 0x3de)        : %llu\n", (unsigned long long)nTiny);
    std::printf("smallest reduced exponent      : 0x%llx  at x=0x%08x (%.9g)\n",
                (unsigned long long)minExp, minArg, (double)F(minArg));
    std::printf("-> reducedTiny is %s from a float argument.\n",
                nTiny ? "REACHABLE" : "UNREACHABLE");
    return 0;
}
