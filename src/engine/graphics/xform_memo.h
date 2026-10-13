#pragma once
// ============================================================================
// src/engine/graphics/xform_memo.h -- XFORM: the per-draw-item world matrix
// computed once per key per Renderer::Draw emit pass
// ============================================================================
//
// NOT RETAIL. Faithful perf work (docs/extensions/handins/XFORM.md). The same
// Context calls in the same order with the same bytes, bit for bit.
//
// Every per-item world transform the renderer emits is
//
//     AsMatrix( { prev.rotation.Lerp(cur.rotation, t),
//                 prev.translation + (cur.translation - prev.translation) * t } )
//
// over ONE `InterpolatedState<Math::Transform>` pair: a Geometry's `_transform`,
// a MeshInstance's `_nodes[node]`, or a shared instanced pose's `_nodes[node]`.
// Eight sites spell it (hand-in section 1.1): MeshInstance / EdgeMeshInstance /
// Geometry / ParticleInstance `GetInstanceTransform`, the pose blocks of
// MeshInstance / EdgeMeshInstance / ParticleInstance `Draw`, and the game's
// `WorldGeometry::Draw` (one per terrain chunk item). Each spelling is the
// same IEEE operations on the same operands (no FMA, no fast-math), so the
// matrix is a pure function of the pair's 56 bytes and t's bits.
//
// Inside the emit pass of one `Renderer::Draw` (pass 2: the lights'
// DrawShadowMap / DrawLight -> DrawItemList), `t` is `Renderer::_alphaT`, and
// nothing writes or frees a pair (census: hand-in section 1.2). Yet one pair is
// asked for once per triangle list, once per pass (the sun's shadow map, then
// DrawLight per light and list) and once per instance that shares a pose.
//
// The memo: a direct-mapped slot table keyed on the pair's ADDRESS and t's
// BITS, each slot holding the 64-byte matrix the first request's own spelling
// produced. It is valid only inside one scope (`Scope`, held by Renderer::Draw
// from the start of pass 2): both Begin and End bump the epoch, so no entry
// outlives the pass it was computed in. Outside a scope every call is the
// site's own retail spelling. The COMPPROF=4 shadow (`ItemTransform`)
// recomputes every hit with the requesting site's retail spelling.
//
// Header-only and free of game types beyond the pair template, so the unit
// test (tests/unit/xform_memo_test.cpp) drives it directly.
// ============================================================================

#include <cstdint>
#include <cstring>
#include <functional>   // std::hash<const void*>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)
#include <x86intrin.h>
#endif

#include "math/Math.h"
#include "graphics/InterpolatedState.h"

namespace XformMemo {

// The kill switch. false = every site runs its retail spelling, with no lookup,
// no counter and no scope bookkeeping.
inline constexpr bool kEnabled = false;

using Pair = Graphics::InterpolatedState<Math::Transform>;

// The requesting sites, one per override arm (the /xform row's classes).
enum Site : int {
    kMeshGit = 0,   // MeshInstance::GetInstanceTransform (all three arms)
    kMeshDraw,      // MeshInstance::Draw, the pass == 1 pose block (both arms)
    kEdgeGit,       // EdgeMeshInstance::GetInstanceTransform, the own-transform arm
    kEdgeDraw,      // EdgeMeshInstance::Draw, the two own-transform arms
    kGeomGit,       // Geometry::GetInstanceTransform (the base)
    kPartGit,       // ParticleInstance::GetInstanceTransform
    kPartDraw,      // ParticleInstance::Draw, the instances <= 1 block
    kWorldDraw,     // WorldGeometry::Draw (src/game, the terrain chunks), one per chunk item
    kSites
};
inline constexpr const char* kSiteNames[kSites] = {
    "mesh.git", "mesh.draw", "edge.git", "edge.draw", "geom.git", "part.git", "part.draw", "world.draw"
};

// Everything the matrix depends on, by identity and bits. 16 bytes, no padding.
struct Key {
    const void* pair;   // the InterpolatedState<Transform> the site lerps
    uint32_t    t;      // the float's bits
    uint32_t    zero;   // always 0 (keeps the compare free of padding)
};
static_assert(sizeof(Key) == 16, "XformMemo::Key has no padding");

inline uint32_t Bits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }

inline bool SameKey(const Key& a, const Key& b) { return std::memcmp(&a, &b, sizeof(Key)) == 0; }

// splitmix64's finaliser over the pointer's value (no cast) and the t bits:
// pairs sit at 0x38 strides and Geometry +0x84, so the low pointer bits alone
// would leave most slots unused.
inline uint64_t HashKey(const Key& k) {
    const std::hash<const void*> ph;
    uint64_t x = uint64_t{ ph(k.pair) } ^ (uint64_t{ k.t } << 32);
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

// A fixed, direct-mapped slot table: slot = hash & (kSlots - 1), each slot one
// stored key, its epoch and the 64-byte matrix. A slot is live only when its
// epoch is the current one, so a scope change empties the table with no clear.
// A different key hashing to a live slot EVICTS it (counted; the evicted key's
// next request recomputes). kSlots x 88 bytes = 1.4 MB, allocated on first use.
inline constexpr int32_t kSlots = 16384;   // power of two

class Table {
public:
    struct Slot { Key key; uint64_t epoch; Math::Matrix m; };

    int32_t SlotOf(uint64_t h) const { return (int32_t)(h & (uint64_t)(kSlots - 1)); }
    bool Live(int32_t s, const Key& k, uint64_t epoch) const {
        return !_slots.empty() && _slots[(size_t)s].epoch == epoch && SameKey(_slots[(size_t)s].key, k);
    }
    // True when filling slot `s` in `epoch` evicts another live key.
    bool Occupied(int32_t s, uint64_t epoch) const {
        return !_slots.empty() && _slots[(size_t)s].epoch == epoch;
    }
    // Claim slot `s` for `k`; returns its matrix, for the caller to fill.
    Math::Matrix& Claim(int32_t s, const Key& k, uint64_t epoch) {
        if (_slots.empty()) _slots.assign((size_t)kSlots, Slot{ Key{}, 0, Math::Matrix() });
        Slot& slot = _slots[(size_t)s];
        slot.key = k;
        slot.epoch = epoch;
        return slot.m;
    }
    const Math::Matrix& At(int32_t s) const { return _slots[(size_t)s].m; }

private:
    std::vector<Slot> _slots;
};

// The scope state (main thread only: every Renderer::Draw runs on the thread
// that owns the Video::Context).
inline uint64_t g_epoch = 1;   // bumped at every BeginScope and EndScope; 0 marks an empty slot
inline int      g_depth = 0;   // > 0 inside a Renderer::Draw emit pass
inline uint64_t g_scopes = 0;  // scopes opened
inline Table    g_table;

inline void BeginScope() { ++g_depth; ++g_epoch; ++g_scopes; }
inline void EndScope()   { --g_depth; ++g_epoch; }

// The RAII form Renderer::Draw holds from the start of pass 2 to its end.
struct Scope {
    Scope()  { if constexpr (kEnabled) BeginScope(); }
    ~Scope() { if constexpr (kEnabled) EndScope(); }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};

// ---- counters (main thread; always counted, cheap) and the profile ---------
inline uint64_t g_calls[kSites] = {};    // requests inside a scope, per site
inline uint64_t g_hits[kSites] = {};
inline uint64_t g_misses[kSites] = {};   // keys claimed by this site (distinct keys per scope, summed)
inline uint64_t g_declines = 0;          // requests outside a scope
inline uint64_t g_evictions = 0;         // a miss that overwrote another live key's slot
inline uint64_t g_missTicks = 0;         // RDTSC, only under g_prof
inline uint64_t g_hitTicks = 0;

// Set by the stress harness (src/diag/stress_harness.cpp): g_prof at
// BANISHED_STRESS_COMPPROF >= 1, g_verify + g_report at 4. Never otherwise; the
// engine does not include the harness.
inline int  g_prof = 0;
inline int  g_verify = 0;
inline void (*g_report)(bool ok, long long detail) = nullptr;

inline uint64_t Now() {
#if defined(__x86_64__) || defined(_M_X64)
    return (uint64_t)__rdtsc();
#else
    return 0;
#endif
}

// The memo's entry. `faithful` is the site's own retail spelling (a lambda over
// the same pair and t) returning the 64-byte matrix. Outside a scope it simply
// runs. Inside, a hit hands back the slot's bits; a miss runs `faithful` into
// the slot. Under the shadow every hit also runs `faithful` on the live inputs,
// compares all sixteen floats bit for bit and hands the game the REFERENCE.
// detail = (site << 16) | first differing float (0xffff = none).
template <class Faithful>
inline Math::Matrix World(const Pair& pair, float t, Site site, Faithful&& faithful) {
    if constexpr (!kEnabled) {
        return faithful();
    } else {
        if (g_depth <= 0) {
            ++g_declines;
            return faithful();
        }
        const Key key{ &pair, Bits(t), 0u };
        ++g_calls[site];
        const uint64_t t0 = g_prof ? Now() : 0;
        const int32_t s = g_table.SlotOf(HashKey(key));
        if (g_table.Live(s, key, g_epoch)) {
            ++g_hits[site];
            const Math::Matrix memo = g_table.At(s);
            if (g_prof) g_hitTicks += Now() - t0;
            if (g_verify) {
                const Math::Matrix ref = faithful();
                int firstBad = -1;
                for (int i = 0; i < 16 && firstBad < 0; ++i)
                    if (Bits(ref.m[i]) != Bits(memo.m[i])) firstBad = i;
                if (g_report) g_report(firstBad < 0, ((long long)site << 16) | (firstBad < 0 ? 0xffff : firstBad));
                return ref;
            }
            return memo;
        }
        ++g_misses[site];
        if (g_table.Occupied(s, g_epoch)) ++g_evictions;
        Math::Matrix& out = g_table.Claim(s, key, g_epoch);
        out = faithful();
        if (g_prof) g_missTicks += Now() - t0;
        return out;
    }
}

}  // namespace XformMemo
