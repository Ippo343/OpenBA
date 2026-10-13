// ============================================================================
// tests/unit/xform_memo_test.cpp -- XFORM: the per-emit-pass world matrix memo
// (src/engine/graphics/xform_memo.h)
// ============================================================================
// The slot table is checked directly. Then two copies of one scripted renderer
// run side by side over a fixed pool of transform pairs (the Geometry
// `_transform`s and pose arrays): pairs are rewritten AT THE SAME ADDRESS
// between scopes (the pointer-key hazard), the pool is larger than the table
// so direct-mapped evictions happen, and every request goes through one of the
// five retail spellings the sites use (four copied below, one real). World A always runs the
// spelling; world B runs `XformMemo::World` with the real global table, epoch
// and scope. Every request's 64 bytes must match. A broken variant that
// rewrites a pair inside a scope (the census claim broken) must diverge, and
// the COMPPROF=4 shadow must report it.
// The five spellings are also checked against each other on hostile inputs
// (signed zeros, denormals, large magnitudes): the cross-site sharing rests on
// their agreement.
// ============================================================================
#include "unit.h"
#include "graphics/xform_memo.h"
#include "graphics/InterpolatedState.h"
#include "math/Math.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace {

using XformMemo::Pair;

uint64_t Mix(uint64_t x) {   // splitmix64
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

float F(uint64_t r, float scale) { return ((float)(r % 20001) / 10000.0f - 1.0f) * scale; }

// ---- the five retail spellings (four copies of the sites' lambdas, one real)
// mesh.cpp RtLerpTransformPair (MeshInstance / EdgeMeshInstance GetInstanceTransform)
Math::Matrix SpellRtPair(const Pair& p, float t) {
    Math::Transform out;
    out.rotation = p._previous.rotation.Lerp(p._current.rotation, Math::Scalar(t));
    out.translation.x = p._previous.translation.x + (p._current.translation.x - p._previous.translation.x) * t;
    out.translation.y = p._previous.translation.y + (p._current.translation.y - p._previous.translation.y) * t;
    out.translation.z = p._previous.translation.z + (p._current.translation.z - p._previous.translation.z) * t;
    return out.AsMatrix();
}
// meshinstance_draw.cpp LerpTransform (MeshInstance / EdgeMeshInstance Draw)
Math::Matrix SpellDraw(const Pair& p, float t) {
    const Math::Transform& cur = p._current;
    const Math::Transform& prev = p._previous;
    Math::Transform out;
    const Math::Quaternion quat = prev.rotation.Lerp(cur.rotation, Math::Scalar(t));
    const float dx = (cur.translation.x - prev.translation.x) * t;
    out.translation.x = prev.translation.x + dx;
    const float dy = (cur.translation.y - prev.translation.y) * t;
    out.translation.y = prev.translation.y + dy;
    const float dz = (cur.translation.z - prev.translation.z) * t;
    out.translation.z = prev.translation.z + dz;
    out.rotation = quat;
    return out.AsMatrix();
}
// scene.cpp Geometry::GetInstanceTransform (the base)
Math::Matrix SpellBase(const Pair& p, float t) {
    const float py = p._previous.translation.y, pz = p._previous.translation.z;
    const float cx = p._current.translation.x, cy = p._current.translation.y, cz = p._current.translation.z;
    const float px = p._previous.translation.x;
    const Math::Scalar s = { t };
    Math::Transform tmp;
    tmp.rotation = p._previous.rotation.Lerp(p._current.rotation, s);
    tmp.translation.x = px + (cx - px) * t;
    tmp.translation.y = py + (cy - py) * t;
    tmp.translation.z = pz + (cz - pz) * t;
    return tmp.AsMatrix();
}
// wave27_lane_gfxpart.cpp ParticleInstance (GetInstanceTransform and Draw)
Math::Matrix SpellParticle(const Pair& p, float t) {
    const Math::Transform& cur = p._current;
    const Math::Transform& prv = p._previous;
    Math::Transform blended;
    const float px = prv.translation.x, py = prv.translation.y, pz = prv.translation.z;
    blended.translation.x = (cur.translation.x - px) * t + px;
    blended.translation.y = (cur.translation.y - py) * t + py;
    blended.translation.z = (cur.translation.z - pz) * t + pz;
    blended.rotation = prv.rotation.Lerp(cur.rotation, Math::Scalar(t));
    return blended.AsMatrix();
}
// src/game/Game/geometry_draw.cpp WorldGeometry::Draw: the REAL
// InterpolatedState<Transform>::Lerp -> Transform::Lerp (transform.cpp, linked
// here), which spells x as `ax + tv * (other.x - ax)`.
Math::Matrix SpellWorld(const Pair& p, float t) {
    const Math::Transform lerped = p.Lerp(Math::Scalar(t));
    return lerped.AsMatrix();
}
Math::Matrix Spell(int which, const Pair& p, float t) {
    switch (which % 5) {
        case 0:  return SpellRtPair(p, t);
        case 1:  return SpellDraw(p, t);
        case 2:  return SpellBase(p, t);
        case 3:  return SpellParticle(p, t);
        default: return SpellWorld(p, t);
    }
}

void FillPair(Pair& p, uint64_t r) {
    Math::Transform* const halves[2] = { &p._current, &p._previous };
    for (int h = 0; h < 2; ++h) {
        Math::Transform& x = *halves[h];
        x.rotation.x = F(Mix(r + 1 + h * 8), 1.0f);
        x.rotation.y = F(Mix(r + 2 + h * 8), 1.0f);
        x.rotation.z = F(Mix(r + 3 + h * 8), 1.0f);
        x.rotation.w = F(Mix(r + 4 + h * 8), 1.0f) + 0.01f;
        x.translation.x = F(Mix(r + 5 + h * 8), 2000.0f);
        x.translation.y = F(Mix(r + 6 + h * 8), 50.0f);
        x.translation.z = F(Mix(r + 7 + h * 8), 2000.0f);
    }
    if ((r & 7) == 0) p._previous = p._current;   // a still pair
}

bool SameBits(const Math::Matrix& a, const Math::Matrix& b) { return std::memcmp(&a, &b, sizeof(Math::Matrix)) == 0; }

struct Stats { long long requests = 0, hits = 0, mismatches = 0, evictions = 0, rewrites = 0, shadowBad = 0; };

long long s_shadowChecks = 0, s_shadowBad = 0;

// One run of the two renderers. `writeInsideScope` breaks the census claim.
Stats Run(uint64_t seed, bool writeInsideScope, int scopes, bool shadow) {
    using namespace XformMemo;
    Stats st;
    constexpr int kPool = 30000;           // more pairs than kSlots: real evictions
    constexpr int kVisible = 5000;         // pairs drawn per scope
    constexpr int kRequests = 9000;        // requests per scope (~1.8 per key)
    std::vector<Pair> pool((size_t)kPool); // never reallocated: fixed addresses
    for (int i = 0; i < kPool; ++i) FillPair(pool[(size_t)i], Mix(seed * 7919 + (uint64_t)i));

    const uint64_t evict0 = g_evictions;
    long long hits0 = 0;
    for (int s = 0; s < kSites; ++s) hits0 += (long long)g_hits[s];
    s_shadowChecks = s_shadowBad = 0;
    g_verify = shadow ? 1 : 0;
    g_report = nullptr;
    if (shadow) g_report = [](bool ok, long long) { ++s_shadowChecks; if (!ok) ++s_shadowBad; };

    for (int f = 0; f < scopes; ++f) {
        // ---- between scopes: the sim rewrites pairs in place (same address)
        for (int w = 0; w < 400; ++w) {
            const uint64_t r = Mix(seed ^ ((uint64_t)f << 24) ^ (uint64_t)w);
            FillPair(pool[(size_t)(r % kPool)], r);
            ++st.rewrites;
        }
        const float t = (float)(Mix(seed + (uint64_t)f * 13) % 1000) / 999.0f;
        const int first = (int)(Mix(seed * 31 + (uint64_t)f) % (kPool - kVisible));

        BeginScope();
        for (int q = 0; q < kRequests; ++q) {
            const uint64_t r = Mix(seed ^ ((uint64_t)f << 32) ^ (uint64_t)q * 0x51ED27u);
            Pair& p = pool[(size_t)(first + (int)(r % kVisible))];
            const int which = (int)((r >> 40) % 5);
            if (writeInsideScope && q == kRequests / 2) FillPair(p, r ^ 0xABCDEFull);   // BROKEN

            const Math::Matrix ref = Spell(which, p, t);                                    // world A
            const Math::Matrix got = World(p, t, (Site)((r >> 44) % kSites),                // world B
                                           [&p, t, which] { return Spell(which, p, t); });
            ++st.requests;
            if (!SameBits(ref, got)) ++st.mismatches;
        }
        EndScope();
    }
    long long hits1 = 0;
    for (int s = 0; s < kSites; ++s) hits1 += (long long)g_hits[s];
    st.hits = hits1 - hits0;
    st.evictions = (long long)(g_evictions - evict0);
    st.shadowBad = s_shadowBad;
    g_verify = 0;
    g_report = nullptr;
    return st;
}

}  // namespace

UNIT_TEST(xform_memo_table_slots_and_epochs) {
    XformMemo::Table table;
    Pair a, b;
    XformMemo::Key ka{ &a, XformMemo::Bits(0.5f), 0u };
    XformMemo::Key kb = ka; kb.t = XformMemo::Bits(-0.5f);   // differs only in t's sign bit
    XformMemo::Key kc{ &b, XformMemo::Bits(0.5f), 0u };        // differs only in the pair
    CHECK(!XformMemo::SameKey(ka, kb));
    CHECK(!XformMemo::SameKey(ka, kc));
    const int32_t s = table.SlotOf(XformMemo::HashKey(ka));
    CHECK(!table.Live(s, ka, 5));
    CHECK(!table.Occupied(s, 5));
    table.Claim(s, ka, 5).m[0] = 1.0f;
    CHECK(table.Live(s, ka, 5));
    CHECK(table.At(s).m[0] == 1.0f);
    CHECK(!table.Live(s, kb, 5));                     // a stored key compare, not the hash
    CHECK(!table.Live(s, ka, 6));                     // a new scope: empty, no clear
    CHECK(!table.Occupied(s, 6));
    CHECK(table.Occupied(s, 5));
    table.Claim(s, kb, 5);                            // a collision evicts
    CHECK(!table.Live(s, ka, 5));
    CHECK(table.Live(s, kb, 5));
    CHECK(table.SlotOf(~0ull) == XformMemo::kSlots - 1);
    // The hash spreads 0x38-strided pairs over the table (the low pointer bits alone would not).
    std::vector<Pair> arr(4096);
    std::vector<int> used((size_t)XformMemo::kSlots, 0);
    int distinct = 0;
    for (const Pair& p : arr) {
        const XformMemo::Key k{ &p, XformMemo::Bits(0.25f), 0u };
        if (used[(size_t)table.SlotOf(XformMemo::HashKey(k))]++ == 0) ++distinct;
    }
    CHECK(distinct > 3400);   // ~3,550 expected for 4,096 keys in 16,384 slots
    // Outside a scope the memo declines and runs the faithful body.
    const uint64_t d0 = XformMemo::g_declines;
    int ran = 0;
    const Math::Matrix m = XformMemo::World(a, 0.5f, XformMemo::kMeshGit, [&ran] { ++ran; return Math::Matrix(); });
    CHECK(ran == 1);
    CHECK(XformMemo::g_declines == d0 + 1);
    CHECK(m.m[0] == 0.0f);
}

UNIT_TEST(xform_memo_site_spellings_agree) {
    long long cases = 0, bad = 0;
    for (uint64_t i = 0; i < 20000; ++i) {
        Pair p;
        FillPair(p, Mix(i + 777));
        const uint64_t r = Mix(i * 3 + 1);
        // Hostile inputs: signed zeros, denormals, large magnitudes, t at the ends.
        if ((r & 15) == 1) { p._current.translation.x = -0.0f; p._previous.translation.x = 0.0f; }
        if ((r & 15) == 2) { p._current.translation.y = 1e-40f; p._previous.translation.y = -1e-41f; }
        if ((r & 15) == 3) { p._current.translation.z = 3.0e30f; p._previous.translation.z = -2.5e30f; }
        if ((r & 15) == 4) { p._previous.rotation = p._current.rotation; }
        float t = (float)(r % 100001) / 100000.0f;
        if ((r >> 20 & 7) == 0) t = 0.0f;
        if ((r >> 20 & 7) == 1) t = 1.0f;
        const Math::Matrix m0 = SpellRtPair(p, t);
        for (int w = 1; w < 5; ++w) { ++cases; if (!SameBits(m0, Spell(w, p, t))) ++bad; }
    }
    CHECK_EQ(bad, 0LL);
    CHECK(cases == 80000);
}

UNIT_TEST(xform_memo_two_renderers_bit_identical) {
    long long req = 0, hits = 0, ev = 0, rw = 0;
    for (uint64_t seed = 1; seed <= 3; ++seed) {
        const Stats st = Run(seed, false, 40, false);
        CHECK_EQ(st.mismatches, 0LL);
        req += st.requests; hits += st.hits; ev += st.evictions; rw += st.rewrites;
    }
    std::printf("  xform_memo: %lld requests, %lld memo hits, %lld evictions, %lld in-place rewrites\n",
                req, hits, ev, rw);
    CHECK(hits > req / 4);
    CHECK(ev > 0);
    CHECK(XformMemo::g_depth == 0);
}

UNIT_TEST(xform_memo_shadow_clean_on_faithful_run) {
    const Stats st = Run(9, false, 10, true);
    CHECK_EQ(st.mismatches, 0LL);
    CHECK(s_shadowChecks == st.hits);   // every hit recomputed
    CHECK_EQ(st.shadowBad, 0LL);
}

UNIT_TEST(xform_memo_write_inside_scope_diverges) {
    long long mism = 0;
    for (uint64_t seed = 1; seed <= 3; ++seed) mism += Run(seed, true, 20, false).mismatches;
    CHECK(mism > 0);
    // The shadow catches the same broken census and hands the game the reference.
    const Stats st = Run(4, true, 20, true);
    CHECK(st.shadowBad > 0);
    CHECK_EQ(st.mismatches, 0LL);
}
