// ============================================================================
// src/engine/graphics/draw_sort.cpp -- DRAWSORT target 2 (see draw_sort.h)
// ============================================================================
// The reference body is retail's draw-item sort exactly as it was ported in
// wave27_lane_gfxbig.cpp (BATCH 8b); the fast body is NOT RETAIL, faithful perf
// work with the same output bit for bit. docs/extensions/handins/DRAWSORT.md.
// SORTCMP's plain path (kPlainSort) is exact on every byte the consumer reads,
// not on the pads: docs/extensions/handins/SORTCMP.md.
// ============================================================================
#include "graphics/draw_sort.h"

#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "collection/introsort_msvc.h"

namespace DrawSort {

int  g_profOn = 0;
int  g_verify = 0;
void (*g_report)(bool ok, long long detail) = nullptr;
Counters g_counters = {};

Counters TakeCounters() {
    const Counters c = g_counters;
    g_counters = Counters{};
    return c;
}

// ---------------------------------------------------------------------------
// The reference: 0x180063b90 (selection sort, max to high) and 0x180062d60 (the
// skeleton), the comparator and the swap reached through function pointers,
// exactly the instantiation wave27_lane_gfxbig.cpp had before this block.
// ---------------------------------------------------------------------------
void SortReferenceSmall(RendererDrawItem* lo, RendererDrawItem* hi) {
    rtsort::SelectionSortMaxToHigh(lo, hi, &DrawItemLess, &DrawItemSwap);
}

void SortReference(RendererDrawItem* data, int32_t count) {
    rtsort::Introsort(data, count, &DrawItemLess, &DrawItemSwap,
                      [](RendererDrawItem* lo, RendererDrawItem* hi) { SortReferenceSmall(lo, hi); });
}

// ---------------------------------------------------------------------------
// The key.
// ---------------------------------------------------------------------------
uint32_t SortKeyBits(float x) {
    // `x == 0.0f` is the same ordered compare COMISS makes, under the same MXCSR:
    // it is true for +0, -0 and (with DAZ on) every denormal -- exactly the values
    // retail's two `>` tests find equal to zero -- and they all get +0's bits.
    uint32_t u = 0;
    if (!(x == 0.0f)) std::memcpy(&u, &x, 4);
    // Ascending total order of the non-NaN floats as unsigned integers, then
    // reversed (retail's term is descending).
    const uint32_t asc = (u & 0x80000000u) ? ~u : (u | 0x80000000u);
    return ~asc;
}

namespace {

// The pointer's address bits: the unsigned order std::less (and retail's CMP/JB) compares by.
inline uint64_t P(const void* p) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));  // CAST-OK: ptr_int
}

}  // namespace

bool BuildKeys(const RendererDrawItem* data, int32_t n, Key* out, bool* nanFound, bool* ptrFound) {
    uint64_t highBits = 0;
    bool nan = false;
    for (int32_t i = 0; i < n; ++i) {
        const RendererDrawItem& it = data[i];
        const float sk = it._sortKey;
        nan |= (sk != sk);
        const uint64_t mat  = P(it._material);
        const uint64_t inst = P(it._instance);
        const uint64_t user = P(it._userData);
        const uint64_t geo  = P(it._geometry);
        highBits |= (mat | inst | user | geo) >> 48;
        const uint64_t f = it._flags;
        const uint64_t s = SortKeyBits(sk);
        Key& k = out[i];
        k.w[0] = (f << 48) | (s << 16) | (mat >> 32);
        k.w[1] = (mat << 32) | (inst >> 16);
        k.w[2] = (inst << 48) | (static_cast<uint64_t>(it._arg5) << 32) | it._arg10;
        k.w[3] = (user << 16) | (geo >> 32);
        k.w[4] = (geo << 32) | static_cast<uint32_t>(i);
    }
    *nanFound = nan;
    *ptrFound = highBits != 0;
    return !nan && highBits == 0;
}

namespace {

std::vector<Key>&              KeyScratch()  { static std::vector<Key> v; return v; }
std::vector<RendererDrawItem>& ItemScratch() { static std::vector<RendererDrawItem> v; return v; }
std::vector<RendererDrawItem>& RefScratch()  { static std::vector<RendererDrawItem> v; return v; }

// SORTCMP's plain-path scratch (main thread only, grow-only, like the three above).
struct GroupKey {
    uint64_t a, b, c, d;   // the prefix: w0, w1, w2, w3 >> 16 (userData)
    int32_t  g;            // the group's id (first-seen order)
    int32_t  n;            // members
};
std::vector<int32_t>&  TableScratch()   { static std::vector<int32_t> v; return v; }
std::vector<int32_t>&  GroupOfScratch() { static std::vector<int32_t> v; return v; }
std::vector<int32_t>&  OffsetScratch()  { static std::vector<int32_t> v; return v; }
std::vector<GroupKey>& GroupScratch()   { static std::vector<GroupKey> v; return v; }
std::vector<uint64_t>& SegScratch()     { static std::vector<uint64_t> v; return v; }

template <class T>
T* Grow(std::vector<T>& v, int32_t n) {
    if (v.size() < static_cast<size_t>(n)) v.resize(static_cast<size_t>(n));
    return v.data();
}

// Retail's skeleton on the items with the comparator and the swap inlined.
template <bool kCount>
void SortItemsInline(RendererDrawItem* data, int32_t count, uint64_t* compares) {
    auto less = [compares](const RendererDrawItem& a, const RendererDrawItem& b) {
        if constexpr (kCount) ++*compares;
        return DrawItemLess(a, b);
    };
    auto swp = [](RendererDrawItem* a, RendererDrawItem* b) { DrawItemSwap(a, b); };
    rtsort::Introsort(data, count, less, swp,
                      [&](RendererDrawItem* lo, RendererDrawItem* hi) {
                          rtsort::SelectionSortMaxToHigh(lo, hi, less, swp);
                      });
}

// Retail's skeleton on the keys, then one move of the items.
template <bool kCount>
void SortKeyed(RendererDrawItem* data, int32_t count, Key* keys, uint64_t* compares) {
    auto less = [compares](const Key& a, const Key& b) {
        if constexpr (kCount) ++*compares;
        return KeyLess(a, b);
    };
    auto swp = [](Key* a, Key* b) { const Key t = *a; *a = *b; *b = t; };
    rtsort::Introsort(keys, count, less, swp,
                      [&](Key* lo, Key* hi) { rtsort::SelectionSortMaxToHigh(lo, hi, less, swp); });
    RendererDrawItem* const tmp = Grow(ItemScratch(), count);
    for (int32_t i = 0; i < count; ++i) tmp[i] = data[KeyIndex(keys[i])];
    std::memcpy(static_cast<void*>(data), tmp, static_cast<size_t>(count) * sizeof(RendererDrawItem));
}

// ---------------------------------------------------------------------------
// SORTCMP: the plain sort of a keyed list.
//
// The full key order is (prefix, geometry) lexicographic, where the prefix is
// the 240 bits above the geometry term (w0, w1, w2, w3 >> 16) and the geometry
// is the 48 bits (w3 & 0xffff, w4 >> 32). So: (1) hash every item's prefix into
// a group (open addressing, first-seen ids); (2) sort the distinct prefixes;
// (3) place each item's (geometry << 16 | index) word in its group's segment;
// (4) sort each segment as plain uint64 (geometry first; the index only orders
// equal geometries, which are full-key ties); (5) scan the segments for
// full-key ties whose `_arg8` differs -- one such tie and the function returns
// false before anything has been written to `data` or `keys`; (6) gather.
// ---------------------------------------------------------------------------
inline uint64_t PrefixHash(const Key& k) {
    uint64_t h = k.w[0] * 0x9E3779B97F4A7C15ull;
    h ^= h >> 29; h += k.w[1]; h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 31; h += k.w[2]; h *= 0x94D049BB133111EBull;
    h ^= h >> 29; h += k.w[3] >> 16; h *= 0x9E3779B97F4A7C15ull;
    return h ^ (h >> 32);
}

inline bool GroupLess(const GroupKey& x, const GroupKey& y) {
    if (x.a != y.a) return x.a < y.a;
    if (x.b != y.b) return x.b < y.b;
    if (x.c != y.c) return x.c < y.c;
    return x.d < y.d;
}

template <bool kCount>
bool SortPlainImpl(RendererDrawItem* data, int32_t count, const Key* keys,
                   uint64_t* compares, uint32_t* groupsOut) {
    uint64_t cmp = 0;
    uint32_t cap = 64;
    while (cap < static_cast<uint32_t>(count) * 2u) cap <<= 1;
    const uint32_t mask = cap - 1;
    int32_t*  const table = Grow(TableScratch(), static_cast<int32_t>(cap));
    int32_t*  const grp   = Grow(GroupOfScratch(), count);
    GroupKey* const gks   = Grow(GroupScratch(), count);
    std::fill(table, table + cap, -1);

    // (1) group by prefix.
    int32_t groupCount = 0;
    for (int32_t i = 0; i < count; ++i) {
        const Key& k = keys[i];
        const uint64_t d = k.w[3] >> 16;
        uint32_t h = static_cast<uint32_t>(PrefixHash(k)) & mask;
        int32_t g;
        for (;;) {
            g = table[h];
            if (g < 0) {
                g = groupCount++;
                table[h] = g;
                gks[g] = GroupKey{k.w[0], k.w[1], k.w[2], d, g, 0};
                break;
            }
            if constexpr (kCount) ++cmp;
            const GroupKey& e = gks[g];
            if (e.a == k.w[0] && e.b == k.w[1] && e.c == k.w[2] && e.d == d) break;
            h = (h + 1) & mask;
        }
        grp[i] = g;
        ++gks[g].n;
    }

    // (2) sort the distinct prefixes (all distinct, so the order is strict).
    std::sort(gks, gks + groupCount, [&cmp](const GroupKey& x, const GroupKey& y) {
        if constexpr (kCount) ++cmp;
        return GroupLess(x, y);
    });

    // (3) segment offsets in prefix order, then place every item.
    int32_t* const off = Grow(OffsetScratch(), groupCount);
    int32_t run = 0;
    for (int32_t r = 0; r < groupCount; ++r) { off[gks[r].g] = run; run += gks[r].n; }
    uint64_t* const seg = Grow(SegScratch(), count);
    for (int32_t i = 0; i < count; ++i) {
        const Key& k = keys[i];
        seg[off[grp[i]]++] = ((k.w[3] & 0xFFFFull) << 48) | ((k.w[4] >> 32) << 16) |
                             static_cast<uint64_t>(i);
    }

    // (4) sort each segment; (5) the tie scan.
    auto lessU = [&cmp](uint64_t x, uint64_t y) {
        if constexpr (kCount) ++cmp;
        return x < y;
    };
    int32_t start = 0;
    for (int32_t r = 0; r < groupCount; ++r) {
        const int32_t n = gks[r].n;
        uint64_t* const lo = seg + start;
        start += n;
        if (n < 2) continue;
        if (n <= 16) {
            for (int32_t j = 1; j < n; ++j) {
                const uint64_t v = lo[j];
                int32_t m = j;
                while (m > 0 && lessU(v, lo[m - 1])) { lo[m] = lo[m - 1]; --m; }
                lo[m] = v;
            }
        } else {
            std::sort(lo, lo + n, lessU);
        }
        for (int32_t j = 1; j < n; ++j) {
            if (((lo[j] ^ lo[j - 1]) >> 16) == 0 &&
                data[lo[j] & 0xFFFFu]._arg8 != data[lo[j - 1] & 0xFFFFu]._arg8) {
                *compares += cmp;
                *groupsOut = static_cast<uint32_t>(groupCount);
                return false;   // a differing-_arg8 tie: only retail's swap sequence is exact
            }
        }
    }

    // (6) one move of the items, as SortKeyed does.
    RendererDrawItem* const tmp = Grow(ItemScratch(), count);
    for (int32_t i = 0; i < count; ++i) tmp[i] = data[seg[i] & 0xFFFFu];
    std::memcpy(static_cast<void*>(data), tmp, static_cast<size_t>(count) * sizeof(RendererDrawItem));
    *compares += cmp;
    *groupsOut = static_cast<uint32_t>(groupCount);
    return true;
}

// The census's "same key array as this buffer's previous sort" memory: the
// list's buffer pointer -> the hash of the key words it held. COMPPROF only.
// (A group's item array keeps its buffer across frames: Renderer::Reset sets
// the size to 0 and keeps the capacity.)
std::unordered_map<const void*, uint64_t>& RepeatMap() {
    static std::unordered_map<const void*, uint64_t> m;
    return m;
}

uint64_t KeyArrayHash(const Key* keys, int32_t n) {
    uint64_t h = 0xCBF29CE484222325ull ^ static_cast<uint64_t>(n);
    for (int32_t i = 0; i < n; ++i)
        for (int w = 0; w < 5; ++w) { h ^= keys[i].w[w]; h *= 0x100000001B3ull; h ^= h >> 31; }
    return h;
}

template <bool kCount, bool kPlain>
void SortFastImpl(RendererDrawItem* data, int32_t count) {
    uint64_t compares = 0;
    uint32_t groups = 0;
    bool keyed = false, nan = false, ptr = false, plain = false, fellBack = false;
    uint64_t runs = 0;
    if (count >= kKeyedMinCount) {
        Key* const keys = Grow(KeyScratch(), count);
        if (BuildKeys(data, count, keys, &nan, &ptr)) {
            keyed = true;
            if constexpr (kCount) {
                // Presortedness of the gather's order, and the repeat census (option (c)).
                runs = 1;
                for (int32_t i = 1; i < count; ++i) runs += KeyLess(keys[i], keys[i - 1]) ? 1u : 0u;
                const uint64_t h = KeyArrayHash(keys, count);
                auto& m = RepeatMap();
                const auto it = m.find(data);
                if (it != m.end()) {
                    ++g_counters.repeatChecks;
                    if (it->second == h) ++g_counters.repeatHits;
                    it->second = h;
                } else {
                    m.emplace(data, h);
                }
            }
            if (kPlain && count <= kPlainMaxCount) {
                plain = true;
                if (!SortPlainImpl<kCount>(data, count, keys, &compares, &groups)) {
                    fellBack = true;
                    SortKeyed<kCount>(data, count, keys, &compares);   // keys untouched by SortPlain
                }
            } else {
                SortKeyed<kCount>(data, count, keys, &compares);
            }
        }
    }
    if (!keyed) SortItemsInline<kCount>(data, count, &compares);

    if constexpr (kCount) {
        Counters& c = g_counters;
        ++c.sorts;
        c.items += static_cast<uint64_t>(count);
        if (static_cast<uint64_t>(count) > c.maxItems) c.maxItems = static_cast<uint64_t>(count);
        if (keyed) ++c.keyed;
        else if (count < kKeyedMinCount) ++c.smallLists;
        else if (nan) ++c.nanLists;
        else if (ptr) ++c.ptrLists;
        c.compares += compares;
        if (plain) { ++c.plainLists; c.groups += groups; }
        if (fellBack) ++c.fallbacks;
        c.runs += runs;
        // Ties: adjacent sorted items neither of which sorts before the other.
        uint64_t tieRun = 1;
        if (count > 0 && data[0]._arg8 > 1) ++c.instItems;
        for (int32_t i = 1; i < count; ++i) {
            const RendererDrawItem& a = data[i - 1];
            const RendererDrawItem& b = data[i];
            if (b._arg8 > 1) ++c.instItems;
            if (!DrawItemLess(a, b) && !DrawItemLess(b, a)) {
                ++c.ties;
                if (a._arg8 != b._arg8) ++c.tiesArg8;
                if (++tieRun > c.maxTie) c.maxTie = tieRun;
            } else {
                tieRun = 1;
            }
        }
    }
}

template <bool kCount>
void SortFastSwitch(RendererDrawItem* data, int32_t count) {
    if constexpr (kPlainSort) SortFastImpl<kCount, true>(data, count);
    else                      SortFastImpl<kCount, false>(data, count);
}

}  // namespace

bool SortPlain(RendererDrawItem* data, int32_t count, const Key* keys,
               uint64_t* compares, uint32_t* groups) {
    *compares = 0;
    *groups = 0;
    return SortPlainImpl<true>(data, count, keys, compares, groups);
}

void SortFast(RendererDrawItem* data, int32_t count) {
    SortFastSwitch<false>(data, count);
}

void SortFastExact(RendererDrawItem* data, int32_t count) {
    SortFastImpl<false, false>(data, count);
}

void Sort(RendererDrawItem* data, int32_t count) {
    if constexpr (!kFastSort) {
        SortReference(data, count);
        return;
    }
    if (g_verify) {
        // The shadow: retail's sort on a copy, the fast sort on the list, then
        // the arrays compared ITEM BY ITEM ON WHAT THE CONSUMER OBSERVES
        // (ConsumerEqual: the seven fields DrawItemList reads, _flags, and
        // _sortKey up to the comparator's equivalence). A permutation inside a
        // tie group of agreeing _arg8 (SORTCMP's plain path) moves only pad and
        // raw sort-key bytes: it is a match, counted in `shadowBytes`. Anything
        // else is a mismatch, reported as (count << 32) | first differing
        // index. Whenever the bytes differ at all, retail's order is copied
        // back, so a shadow run draws (and later sorts from) the faithful bytes.
        const size_t bytes = static_cast<size_t>(count) * sizeof(RendererDrawItem);
        RendererDrawItem* const ref = Grow(RefScratch(), count);
        std::memcpy(static_cast<void*>(ref), data, bytes);
        SortReference(ref, count);
        if (g_profOn) SortFastSwitch<true>(data, count); else SortFastSwitch<false>(data, count);
        bool ok = true;
        long long first = 0;
        if (std::memcmp(ref, data, bytes) != 0) {
            for (int32_t i = 0; i < count; ++i) {
                if (!ConsumerEqual(ref[i], data[i])) { ok = false; first = i; break; }
            }
            if (ok) ++g_counters.shadowBytes;
            std::memcpy(static_cast<void*>(data), ref, bytes);
        }
        if (g_report) g_report(ok, (static_cast<long long>(count) << 32) | first);
        return;
    }
    if (g_profOn) SortFastSwitch<true>(data, count);
    else          SortFastSwitch<false>(data, count);
}

}  // namespace DrawSort
