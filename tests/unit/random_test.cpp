// ============================================================================
// tests/unit/random_test.cpp -- System::Random: retail's MT19937, standing on its own
// ============================================================================
// src/engine/system/value_types.cpp (linked since 2026-10-17; before that link_stubs.cpp
// stood in with a xorshift). What is pinned:
//   * the seeded ctor is init_genrand: index 0, mt[0] = seed, the 0x6c078965 recurrence
//     over all 624 words;
//   * the time-seeded ctor every by-value InternalFactory runs: the same recurrence
//     from (uint32)time(nullptr);
//   * the full draw is the standard generator: seed 5489 gives 3499211612, 581869302,
//     3890346734, 3586334585, 545404204 (the reference implementation's first outputs:
//     retail's tempering masks 0xff3a58ad << 7 and 0xffffdf8c << 15 ARE 0x9d2c5680 and
//     0xefc60000), the index wraps to 0 after 624 draws and the twist runs then;
//   * every helper against the first draw by hand: Rand(bound) = u % bound, Rand(0) is 0
//     WITHOUT a draw, Rand(lo, hi) = lo + u % (hi - lo + 1), RandTol(t) = u % (2t + 1) - t,
//     RandFloat = float(u) * 2^-32 (in [0, 1]: float(0xffffffff) rounds to 2^32, so draws
//     >= 0xffffff80 give exactly 1.0f -- retail's arithmetic, kept), RandFloatTol = 1 - that * 2,
//     left-associated;
//   * two generators from one seed stay in step over a long mixed-helper sequence.
// ============================================================================
#include "unit.h"
#include "system/Value.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>

using Rng = System::Random;   // ::Random is Value.h's POD twin, so no using-declaration

namespace {
const uint32_t kFirst[5] = {3499211612u, 581869302u, 3890346734u, 3586334585u, 545404204u};

bool RecurrenceHolds(const Rng& r, uint32_t mt0) {
    if (r._index != 0 || r._mt[0] != mt0) return false;
    for (int i = 1; i < 624; ++i) {
        const uint32_t prev = r._mt[i - 1];
        if (r._mt[i] != (prev ^ (prev >> 30)) * 0x6c078965u + static_cast<uint32_t>(i)) return false;
    }
    return true;
}
}  // namespace

UNIT_TEST(random_seeded_ctor_is_init_genrand) {
    Rng a(5489u);
    CHECK(RecurrenceHolds(a, 5489u));
    Rng b(0u);
    CHECK(RecurrenceHolds(b, 0u));
    Rng c(0xffffffffu);
    CHECK(RecurrenceHolds(c, 0xffffffffu));
    CHECK(std::memcmp(a._mt, b._mt, sizeof(a._mt)) != 0);
}

UNIT_TEST(random_time_seeded_ctor_uses_the_same_recurrence) {
    const uint32_t before = static_cast<uint32_t>(std::time(nullptr));
    Rng r;
    const uint32_t after = static_cast<uint32_t>(std::time(nullptr));
    CHECK(r._mt[0] == before || r._mt[0] == after);   // the second may tick in between
    CHECK(RecurrenceHolds(r, r._mt[0]));
}

UNIT_TEST(random_full_draw_is_standard_mt19937) {
    Rng r(5489u);
    for (uint32_t want : kFirst) CHECK_EQ(r.Rand(), want);
    CHECK_EQ(r._index, 5u);
    // 624 draws per twist: the index wraps to 0 and the next draw twists again.
    Rng w(5489u);
    for (int i = 0; i < 624; ++i) w.Rand();
    CHECK_EQ(w._index, 0u);
    uint32_t stateBefore[624];
    std::memcpy(stateBefore, w._mt, sizeof(stateBefore));
    w.Rand();
    CHECK(std::memcmp(stateBefore, w._mt, sizeof(stateBefore)) != 0);   // the twist rewrote the words
    CHECK_EQ(w._index, 1u);
}

UNIT_TEST(random_bounded_draw_by_hand) {
    const uint32_t u = kFirst[0];
    { Rng r(5489u); CHECK_EQ(r.Rand(1000u), u % 1000u); CHECK_EQ(r.Rand(1000u), kFirst[1] % 1000u); }
    { Rng r(5489u); CHECK_EQ(r.Rand(1u), 0u); CHECK_EQ(r._index, 1u); }
    { Rng r(5489u); CHECK_EQ(r.Rand(0xffffffffu), u); }   // u < 2^32 - 1 here
    // Rand(0): 0 and NO draw, the state untouched.
    { Rng r(5489u); Rng s(5489u); CHECK_EQ(r.Rand(0u), 0u); CHECK_EQ(r._index, 0u);
      CHECK(std::memcmp(&r, &s, sizeof(Rng)) == 0); CHECK_EQ(r.Rand(), kFirst[0]); }
}

UNIT_TEST(random_range_and_tolerance_by_hand) {
    const uint32_t u = kFirst[0];
    { Rng r(5489u); CHECK_EQ(r.Rand(10, 20), 10 + static_cast<int32_t>(u % 11u)); }   // 16
    { Rng r(5489u); CHECK_EQ(r.Rand(10, 20), 16); }
    { Rng r(5489u); CHECK_EQ(r.Rand(-3, -3), -3); }                                    // width 1
    { Rng r(5489u); CHECK_EQ(r.Rand(5, 5), 5); CHECK_EQ(r._index, 1u); }               // still draws
    { Rng r(5489u); CHECK_EQ(r.RandTol(5), static_cast<int32_t>(u % 11u) - 5); }      // 1
    { Rng r(5489u); CHECK_EQ(r.RandTol(5), 1); }
    { Rng r(5489u); CHECK_EQ(r.RandTol(0), 0); CHECK_EQ(r._index, 1u); }
    { Rng r(5489u); const int32_t t = r.RandTol(7); CHECK(t >= -7 && t <= 7); }
    // The second draw through the range helper: 581869302 % 11 = 581869302 - 11 * 52897209 = 3.
    { Rng r(5489u); r.Rand(); CHECK_EQ(r.Rand(0, 10), 3); }
}

UNIT_TEST(random_float_helpers_by_hand) {
    const float unit = 2.3283064365386963e-10f;   // 1 / 2^32, DAT_18016165c
    uint32_t bits; std::memcpy(&bits, &unit, 4);
    CHECK_EQ(bits, 0x2f800000u);
    { Rng r(5489u);
      const float f = r.RandFloat();
      CHECK_EQ(f, static_cast<float>(kFirst[0]) * unit);
      CHECK(f >= 0.0f && f < 1.0f);
      CHECK(std::fabs(f - 0.8147237f) < 1e-6f);              // 3499211612 / 2^32
      CHECK_EQ(r.RandFloat(), static_cast<float>(kFirst[1]) * unit);
    }
    { Rng r(5489u);
      const float t = r.RandFloatTol();
      CHECK_EQ(t, 1.0f - (static_cast<float>(kFirst[0]) * unit) * 2.0f);   // left-associated
      CHECK(std::fabs(t - (1.0f - 2.0f * 0.8147237f)) < 2e-6f);           // about -0.6294
      CHECK(t >= -1.0f && t <= 1.0f);
    }
    // The range is [0, 1], closed at the top: float(0xffffffff) rounds to 2^32 (24-bit
    // mantissa), so every draw >= 0xffffff80 returns exactly 1.0f. Retail's arithmetic, kept.
    CHECK_EQ(static_cast<float>(0xffffffffu) * unit, 1.0f);
    CHECK_EQ(static_cast<float>(0xffffff80u) * unit, 1.0f);
    CHECK(static_cast<float>(0xffffff7fu) * unit < 1.0f);
    CHECK(static_cast<float>(0u) * unit == 0.0f);
}

UNIT_TEST(random_two_generators_from_one_seed_stay_in_step) {
    Rng a(20261017u), b(20261017u);
    for (int i = 0; i < 5000; ++i) {
        switch (i % 7) {
        case 0: CHECK_EQ(a.Rand(), b.Rand()); break;
        case 1: CHECK_EQ(a.Rand(97u), b.Rand(97u)); break;
        case 2: CHECK_EQ(a.Rand(-50, 50), b.Rand(-50, 50)); break;
        case 3: CHECK_EQ(a.RandFloat(), b.RandFloat()); break;
        case 4: CHECK_EQ(a.RandFloatTol(), b.RandFloatTol()); break;
        case 5: CHECK_EQ(a.RandTol(9), b.RandTol(9)); break;
        default: CHECK_EQ(a.Rand(0u), b.Rand(0u)); break;
        }
    }
    CHECK(std::memcmp(&a, &b, sizeof(Rng)) == 0);
    // 714 of the 5000 steps were Rand(0) (no draw): 4286 draws, 4286 mod 624 = 542.
    CHECK_EQ(a._index, 542u);
}
