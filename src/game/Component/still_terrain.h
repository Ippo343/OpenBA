// src/game/Component/still_terrain.h -- STILLTERRAIN (IDEAS_20261012 §2.4, "E1-last"): the
// exact memo of a still character's terrain query on the LAST speed iteration of the move
// tick (CharacterComponent_MoveTick_1094c0, citizenai_executor.cpp).
// docs/extensions/handins/STILLTERRAIN.md is the proof.
//
// Faithful perf work, not an extension: no registry row, no settings key, no flag. On the
// last iteration (MeshInstance::_rootOnlyUpdates clear) a character whose description has
// +0x3c clear and whose cell is not 0x0b asks Map::GetTerrainPos(pos, flag 0, no normal)
// (0x14009f3e0, mapquery.cpp) for its standing point. When the cell carries no floor
// (MapCell::floorCount <= 0), that query returns {x, y, h(x, y)}: x / y are the input bits,
// and h reads only the int16 heightmap (WorldGeometry::_heights, its stride / edge) through
// GameUpdateLoop::_singleton's world pointers. So the result is a pure function of
//   (x bits, y bits, the heightmap, every cell's floorCount, the world / map pointers),
// and the memo is
//   per CharacterGrid index: {x bits, y bits, epoch} -> z bits
// where `g_epoch` is bumped right after EVERY write of the heightmap or of a floorCount and
// on every world create / load / teardown (the census, STILLTERRAIN.md §1.3). An entry is
// only recorded for a query that could not reach the floor branch, so the map's own picking
// scene (Map +0x08, whose Intersect also bumps the scene's query stamp) is never skipped.
//
// The memo can only decline on any write in between: the epoch is global, compared for
// equality, 64-bit (never wraps), and starts at 1 (0 = an empty entry). Header-only and
// free of game types so tests/unit/still_terrain_test.cpp checks the same code the tick runs.
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace StillTerrain {

// The kill switch (like StillPath::kEnabled): false = the terrain query always runs, the
// memo is never written, the counters stay 0. The epoch bumps stay (a store, no reader).
static constexpr bool kEnabled = true;

// The terrain epoch. Bumped (Bump()) by every writer of an input of the floorless terrain
// query right after it stores. Main thread only (every writer runs in the sim or in the
// world build / load / teardown, never concurrently with a move tick).
inline uint64_t g_epoch = 1;

// Counters for the [COMPPROF] /still-z row (stress_harness.cpp). Main thread only.
inline long long g_queries = 0;   // still last-iteration terrain queries that reached the memo test
inline long long g_hits = 0;      // of which answered by the memo (under COMPPROF=4: shadow-checked)
inline long long g_floor = 0;     // full queries left unrecorded because the cell may carry a floor

inline void Bump() { ++g_epoch; }

inline uint32_t Bits(float f) {
    uint32_t b;
    std::memcpy(&b, &f, sizeof b);
    return b;
}
inline float FromBits(uint32_t b) {
    float f;
    std::memcpy(&f, &b, sizeof f);
    return f;
}

// One remembered query: the input x / y bits, the z bits it returned, the epoch it ran in.
struct Memo {
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t z = 0;
    uint32_t pad = 0;
    uint64_t epoch = 0;   // 0 = never written (g_epoch starts at 1)
};
static_assert(sizeof(Memo) == 24, "STILLTERRAIN memo entry");

// One entry per CharacterGrid index (self+0x40). Never scanned, never cleared: an entry is
// usable only in the epoch it was written in, and only for the same x / y bits, so a stale
// entry, an index reused by another character or a moved model can only decline.
class MemoTable {
public:
    const Memo* Find(int idx) const {
        return static_cast<size_t>(static_cast<unsigned>(idx)) < m_.size() ? &m_[static_cast<size_t>(idx)] : nullptr;
    }
    void Put(int idx, float x, float y, float z, uint64_t epoch) {
        if (idx < 0) return;
        const size_t i = static_cast<size_t>(idx);
        if (i >= m_.size()) m_.resize(i + 256);
        Memo& e = m_[i];
        e.x = Bits(x); e.y = Bits(y); e.z = Bits(z); e.epoch = epoch;
    }
    size_t Size() const { return m_.size(); }
private:
    std::vector<Memo> m_;
};

// The hit test: the entry was written in this epoch for these exact x / y bits.
inline bool Hit(const Memo* m, float x, float y, uint64_t epoch) {
    return m != nullptr && m->epoch == epoch && m->x == Bits(x) && m->y == Bits(y);
}

// StillTerrain shadow (COMPPROF=4): the memo's claim {x, y, z} against what GetTerrainPos
// returned. Bit 0 x, bit 1 y, bit 2 z; 0 = identical bits.
inline long long ShadowDiff(float cx, float cy, float cz, const float* pf) {
    long long d = 0;
    if (Bits(cx) != Bits(pf[0])) d |= 1;
    if (Bits(cy) != Bits(pf[1])) d |= 2;
    if (Bits(cz) != Bits(pf[2])) d |= 4;
    return d;
}

// The [COMPPROF] /still-z row's window (BANISHED_STRESS_COMPPROF=2): the bracketed
// CharacterComponent ticks on the last iteration that reached the memo test, split by
// whether the memo answered. Ticks are the harness's RDTSC ticks.
struct ProfWindow {
    long long calls = 0, ticks = 0;    // last-iteration character ticks that reached the test
    long long hits = 0, hitTicks = 0;  // of which a hit
    void Reset() { *this = ProfWindow(); }
    void Add(long long t, bool hit) {
        ++calls; ticks += t;
        if (hit) { ++hits; hitTicks += t; }
    }
    long long Full() const { return calls - hits; }
    long long FullTicks() const { return ticks - hitTicks; }
    // Indicative: hits x (mean full tick - mean hit tick). The full population carries the
    // stops and the first tick of each epoch, so it overstates; the A/B is the number.
    double SavedTicks() const {
        if (hits <= 0 || Full() <= 0) return 0.0;
        return ((double)FullTicks() / (double)Full() - (double)hitTicks / (double)hits) * (double)hits;
    }
};

}  // namespace StillTerrain
