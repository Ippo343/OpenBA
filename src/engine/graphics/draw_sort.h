#pragma once
// ============================================================================
// src/engine/graphics/draw_sort.h -- DRAWSORT target 2: the per-pass draw-item sort
// ============================================================================
//
// The comparator and the swap of retail's draw-item sort (0x180062d60 /
// 0x180063b90; banner in wave27_lane_gfxbig.cpp), and NOT RETAIL beside them:
// an exact faster sort (docs/extensions/handins/DRAWSORT.md section 2).
//
// WHY RETAIL'S ALGORITHM STAYS. The consumer, `Renderer::DrawItemList`
// (batchc_gfx_leaves.cpp), reads `_arg8` (the instancing batch limit) and
// `_arg8` is not one of the eight compared terms; and a NaN `_sortKey` makes the
// comparator a non-strict-weak order, where the result depends on the algorithm.
// So this keeps retail's skeleton (introsort_msvc.h) and its swap sequence, and
// makes each DECISION cheaper:
//
//   * `Keys`: each item's eight compared terms are packed once, most significant
//     first, into the high 288 bits of a 320-bit key (five words); the low 32
//     bits are the item's index. Comparing the 288 bits lexicographically IS
//     DrawItemLess (proof: DRAWSORT.md section 2.2): every term is a fixed-width
//     field whose unsigned order is the term's order (the float term mapped
//     order-reversingly, +0 and -0 to one value, a denormal to the zero value
//     exactly when `== 0.0f` says so under the live MXCSR), and the four pointer
//     terms fit 48 bits each (checked per list).
//   * retail's skeleton runs on the KEY array: the same decisions at the same
//     positions, so the same swap sequence; the items are then moved once, by the
//     index each final key carries. Every swap moves a whole record, so the item
//     array ends bit for bit as retail's sort leaves it, the two never-written pad
//     dwords included.
//   * a list with a NaN `_sortKey` or a pointer at or above 2^48 cannot be keyed:
//     it takes retail's skeleton on the items with the comparator and swap
//     inlined (the same code as the reference, without the indirect calls).
// ============================================================================

#include <cstdint>
#include <functional>

#include "graphics/Camera.h"   // Graphics::RendererDrawItem

namespace DrawSort {

// The kill switch. false = every draw-item sort is the reference (retail's
// skeleton with the comparator and swap called through function pointers).
inline constexpr bool kFastSort = true;

// Lists shorter than this skip the key build (retail's selection sort handles
// up to 8 directly; below this the build and the move cost more than they save).
inline constexpr int kKeyedMinCount = 32;

using Graphics::RendererDrawItem;

// ---------------------------------------------------------------------------
// Retail's comparator. Every term is one `CMP`/`COMISS` pair read out of
// 0x180062dfe..0x180062e85 (the banner in wave27_lane_gfxbig.cpp); the field
// ORDER is the order of the compares, not of the struct.
// ---------------------------------------------------------------------------
inline bool DrawItemLess(const RendererDrawItem& a, const RendererDrawItem& b) {
    // +0x18, MOVZX / CMP AX,CX - UNSIGNED 16-bit.  JB -> less, JA -> not less.
    if (a._flags != b._flags) return a._flags < b._flags;

    // +0x1c, COMISS both ways - DESCENDING.  Written as two one-sided tests
    // rather than `!=` so that a NaN (for which both JA arms fail) falls
    // through to the next term exactly as retail's does.
    if (a._sortKey > b._sortKey) return true;
    if (b._sortKey > a._sortKey) return false;

    // The remaining six are unsigned integer compares, in this order. The
    // pointer terms are the address order (`std::less`, which is the unsigned
    // compare retail's CMP/JB makes).
    const std::less<> less;
    if (a._material != b._material) return less(a._material, b._material);   // +0x08
    if (a._instance != b._instance) return less(a._instance, b._instance);   // +0x10
    if (a._arg5  != b._arg5)  return a._arg5  < b._arg5;                      // +0x1a
    if (a._arg10 != b._arg10) return a._arg10 < b._arg10;                     // +0x30
    if (a._userData != b._userData) return less(a._userData, b._userData);   // +0x28
    // +0x00, the last term: `CMP RAX,[RBP] / JAE <not less>`.
    return less(a._geometry, b._geometry);
}

// Retail swaps the 0x38-byte element as SEVEN qword moves. The element is
// trivially copyable, so whole-record copies through a temporary move the same
// bytes. Self-swap is a no-op either way (the selection sort can swap best == last).
inline void DrawItemSwap(RendererDrawItem* a, RendererDrawItem* b) {
    const RendererDrawItem t = *a;
    *a = *b;
    *b = t;
}

// ---------------------------------------------------------------------------
// The packed key. Bit string, most significant first:
//   w0 = flags(16) | desc(sortKey)(32) | material[47:32](16)
//   w1 = material[31:0](32) | instance[47:16](32)
//   w2 = instance[15:0](16) | arg5(16) | arg10(32)
//   w3 = userData[47:0](48) | geometry[47:32](16)
//   w4 = geometry[31:0](32) | index(32)          <- only the high 32 bits compare
// ---------------------------------------------------------------------------
struct Key {
    uint64_t w[5];
};
static_assert(sizeof(Key) == 40, "DrawSort::Key is five words");

inline bool KeyLess(const Key& a, const Key& b) {
    if (a.w[0] != b.w[0]) return a.w[0] < b.w[0];
    if (a.w[1] != b.w[1]) return a.w[1] < b.w[1];
    if (a.w[2] != b.w[2]) return a.w[2] < b.w[2];
    if (a.w[3] != b.w[3]) return a.w[3] < b.w[3];
    return (a.w[4] >> 32) < (b.w[4] >> 32);
}
inline bool KeyEqual(const Key& a, const Key& b) {
    return a.w[0] == b.w[0] && a.w[1] == b.w[1] && a.w[2] == b.w[2] && a.w[3] == b.w[3] &&
           (a.w[4] >> 32) == (b.w[4] >> 32);
}
inline uint32_t KeyIndex(const Key& k) { return static_cast<uint32_t>(k.w[4]); }

// The float term, order-REVERSING (retail sorts `_sortKey` descending): for
// non-NaN x, y under the current MXCSR, `x > y` <=> SortKeyBits(x) < SortKeyBits(y)
// and `!(x > y) && !(y > x)` <=> equal bits. Undefined for NaN (the caller checks).
uint32_t SortKeyBits(float x);

// Builds the keys of data[0..n). Returns false (and the keys are not to be used)
// when an item has a NaN `_sortKey` or a pointer term at or above 2^48.
// `nanFound` / `ptrFound` say which.
bool BuildKeys(const RendererDrawItem* data, int32_t n, Key* out, bool* nanFound, bool* ptrFound);

// The reference: retail's 0x180062d60 / 0x180063b90 as ported before this block
// (the skeleton with DrawItemLess / DrawItemSwap through function pointers).
void SortReference(RendererDrawItem* data, int32_t count);
void SortReferenceSmall(RendererDrawItem* lo, RendererDrawItem* hi);

// The fast sort: keyed when the list can be keyed and is long enough, else the
// inlined skeleton on the items. Same array afterwards as SortReference, bit for bit.
void SortFast(RendererDrawItem* data, int32_t count);

// The entry the renderer calls (both callers of 0x180062d60): the kill switch,
// the COMPPROF counters and the COMPPROF=4 shadow.
void Sort(RendererDrawItem* data, int32_t count);

// ---- the stress harness's hooks (set only by src/diag/stress_harness.cpp) ----
extern int g_profOn;                                        // COMPPROF >= 1
extern int g_verify;                                        // COMPPROF == 4
extern void (*g_report)(bool ok, long long detail);         // the shadow's verdict

struct Counters {
    uint64_t sorts;         // lists sorted (count > 1)
    uint64_t items;         // items in them
    uint64_t keyed;         // lists that took the keyed path
    uint64_t smallLists;    // lists under kKeyedMinCount (inlined skeleton on items)
    uint64_t nanLists;      // lists with a NaN sort key (inlined skeleton on items)
    uint64_t ptrLists;      // lists with a pointer >= 2^48 (inlined skeleton on items)
    uint64_t compares;      // comparator calls
    uint64_t ties;          // adjacent pairs in the sorted output that compare equal
    uint64_t tiesArg8;      // of those, pairs whose _arg8 differs
    uint64_t maxItems;      // the longest list
};
extern Counters g_counters;
Counters TakeCounters();     // copies and resets

}  // namespace DrawSort
