// ============================================================================
// src/engine/graphics/draw_sort.cpp -- DRAWSORT target 2 (see draw_sort.h)
// ============================================================================
// The reference body is retail's draw-item sort exactly as it was ported in
// wave27_lane_gfxbig.cpp (BATCH 8b); the fast body is NOT RETAIL, faithful perf
// work with the same output bit for bit. docs/extensions/handins/DRAWSORT.md.
// ============================================================================
#include "graphics/draw_sort.h"

#include <cstring>
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

template <bool kCount>
void SortFastImpl(RendererDrawItem* data, int32_t count) {
    uint64_t compares = 0;
    bool keyed = false, nan = false, ptr = false;
    if (count >= kKeyedMinCount) {
        Key* const keys = Grow(KeyScratch(), count);
        if (BuildKeys(data, count, keys, &nan, &ptr)) {
            SortKeyed<kCount>(data, count, keys, &compares);
            keyed = true;
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
        // Ties: adjacent sorted items neither of which sorts before the other.
        for (int32_t i = 1; i < count; ++i) {
            const RendererDrawItem& a = data[i - 1];
            const RendererDrawItem& b = data[i];
            if (!DrawItemLess(a, b) && !DrawItemLess(b, a)) {
                ++c.ties;
                if (a._arg8 != b._arg8) ++c.tiesArg8;
            }
        }
    }
}

}  // namespace

void SortFast(RendererDrawItem* data, int32_t count) {
    SortFastImpl<false>(data, count);
}

void Sort(RendererDrawItem* data, int32_t count) {
    if constexpr (!kFastSort) {
        SortReference(data, count);
        return;
    }
    if (g_verify) {
        // The shadow: retail's sort on a copy, the fast sort on the list, the
        // whole arrays compared (pad dwords included: both start from the same
        // bytes and only ever move whole records). A mismatch reports
        // (count << 32) | first differing index and leaves retail's order in the
        // list, so a shadow run draws the faithful frame.
        RendererDrawItem* const ref = Grow(RefScratch(), count);
        std::memcpy(static_cast<void*>(ref), data, static_cast<size_t>(count) * sizeof(RendererDrawItem));
        SortReference(ref, count);
        if (g_profOn) SortFastImpl<true>(data, count); else SortFastImpl<false>(data, count);
        const bool ok = std::memcmp(ref, data, static_cast<size_t>(count) * sizeof(RendererDrawItem)) == 0;
        long long first = 0;
        if (!ok) {
            while (first < count &&
                   std::memcmp(&ref[first], &data[first], sizeof(RendererDrawItem)) == 0) ++first;
            std::memcpy(static_cast<void*>(data), ref, static_cast<size_t>(count) * sizeof(RendererDrawItem));
        }
        if (g_report) g_report(ok, (static_cast<long long>(count) << 32) | first);
        return;
    }
    if (g_profOn) SortFastImpl<true>(data, count);
    else          SortFastImpl<false>(data, count);
}

}  // namespace DrawSort
