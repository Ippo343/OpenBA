// ============================================================================
// tests/unit/setsize_fast_test.cpp -- DRAWSORT target 1: SetSizeCore's empty tail
// ============================================================================
// SetSizeFast::kSkipEmptyTail (collection/setsize_prof.h) stops SetSizeCore's
// element loop at min(newSize, oldSize) when there is no fill and no init. The
// claim is that the array holds the same bytes and the same header either way.
// These tests drive the REAL SetSizeCore and the REAL Reserve(16384) body
// (RtColl_ArrayReserve_18005f980, the AddCulledDrawItems visible list) and check:
//   * no byte of the new tail is written (in place and after a realloc);
//   * the copied prefix of a realloc is the old buffer's bytes;
//   * the header follows retail's grow rule (exact fit, x3 on a by-one append);
//   * with a fill or with an init the tail IS written (the arm is not taken);
//   * the COMPPROF counter attributes iterations, empty iterations and classes.
// ============================================================================
#include "unit.h"

#include <cstdint>
#include <cstring>

#include "collection/Collection.h"
#include "collection/array_internal.h"
#include "collection/setsize_prof.h"

namespace rtcoll { void RtColl_ArrayReserve_18005f980(void* a); }

namespace {

using rtcoll::ArrayHdr;
using rtcoll::SetSizeCore;

void InitA5(uint8_t* d) { std::memset(d, 0xA5, 8); }
void Assign8Bytes(uint8_t* d, const uint8_t* s) { std::memcpy(d, s, 8); }

// An 8-byte element whose spec HAS an init: the tail must be written.
const rtcoll::ArraySpec kSpecInit8 = {
    "test<init8>", 8, 8, &Assign8Bytes, &InitA5, nullptr, true, 0, 0, 0, 0, 0, 0 };

bool AllBytes(const uint8_t* p, size_t n, uint8_t v) {
    for (size_t i = 0; i < n; ++i) if (p[i] != v) return false;
    return true;
}

ArrayHdr Empty() { ArrayHdr h; h._data = nullptr; h._size = 0; h._capacity = 0; return h; }

}  // namespace

UNIT_TEST(setsize_fast_reserve_from_empty_writes_no_tail) {
    ArrayHdr h = Empty();
    rtcoll::RtColl_ArrayReserve_18005f980(&h);
    CHECK(h._data != nullptr);
    CHECK_EQ(h._size, 0);
    CHECK_EQ(h._capacity, 0x4000);
    // Paint the buffer, then reserve again (no realloc: capacity is enough) and
    // grow in place with no fill: nothing may be written.
    std::memset(h._data, 0xCD, 0x4000 * 8);
    uint8_t* const before = h._data;
    rtcoll::RtColl_ArrayReserve_18005f980(&h);
    CHECK(h._data == before);
    CHECK_EQ(h._size, 0);
    SetSizeCore(&h, 0x3000, nullptr, &rtcoll::kSpec_Fill8);
    CHECK(h._data == before);
    CHECK_EQ(h._size, 0x3000);
    CHECK_EQ(h._capacity, 0x4000);
    CHECK(AllBytes(h._data, 0x4000 * 8, 0xCD));
    rtcoll::ArrayDtorImpl(&h);
}

UNIT_TEST(setsize_fast_realloc_copies_prefix_only) {
    ArrayHdr h = Empty();
    // Five elements, each with a known value, through the fill form.
    for (int i = 0; i < 5; ++i) {
        uint64_t v = 0x1111111111111111ull * static_cast<uint64_t>(i + 1);
        SetSizeCore(&h, h._size + 1, reinterpret_cast<const uint8_t*>(&v), &rtcoll::kSpec_Fill8);  // CAST-OK: elem_stride
    }
    CHECK_EQ(h._size, 5);
    CHECK_EQ(h._capacity, 6);    // 0->1 allocates 1; 1->2 is a by-one append from non-empty: 2*3
    uint64_t prefix[5];
    std::memcpy(prefix, h._data, sizeof(prefix));
    uint8_t* const old = h._data;

    // Grow past capacity with no fill: a realloc, prefix copied, tail untouched.
    SetSizeCore(&h, 50, nullptr, &rtcoll::kSpec_Fill8);
    CHECK(h._data != old);
    CHECK_EQ(h._size, 50);
    CHECK_EQ(h._capacity, 50);   // bulk growth: exact fit
    CHECK(std::memcmp(h._data, prefix, sizeof(prefix)) == 0);

    // The reserve on a non-empty list: realloc, prefix kept, size restored.
    h._size = 5;
    std::memset(h._data + 5 * 8, 0xEE, 45 * 8);
    rtcoll::RtColl_ArrayReserve_18005f980(&h);
    CHECK_EQ(h._size, 5);
    CHECK_EQ(h._capacity, 0x4000);
    CHECK(std::memcmp(h._data, prefix, sizeof(prefix)) == 0);
    rtcoll::ArrayDtorImpl(&h);
}

UNIT_TEST(setsize_fast_fill_and_init_still_write_the_tail) {
    ArrayHdr h = Empty();
    const uint64_t v = 0x0123456789ABCDEFull;
    SetSizeCore(&h, 32, reinterpret_cast<const uint8_t*>(&v), &rtcoll::kSpec_Fill8);  // CAST-OK: elem_stride
    for (int i = 0; i < 32; ++i) CHECK(std::memcmp(h._data + i * 8, &v, 8) == 0);
    rtcoll::ArrayDtorImpl(&h);

    ArrayHdr g = Empty();
    SetSizeCore(&g, 16, nullptr, &kSpecInit8);
    CHECK(AllBytes(g._data, 16 * 8, 0xA5));
    // In place (capacity 16 after a shrink): init writes [8, 16) again.
    SetSizeCore(&g, 8, nullptr, &kSpecInit8);
    std::memset(g._data + 8 * 8, 0, 8 * 8);
    SetSizeCore(&g, 16, nullptr, &kSpecInit8);
    CHECK(AllBytes(g._data, 16 * 8, 0xA5));
    rtcoll::ArrayDtorImpl(&g);
}

UNIT_TEST(setsize_fast_counter_attributes_iterations) {
    rtcoll::SetSizeProfRow rows[8];
    uint64_t c = 0, it = 0, em = 0;
    CHECK_EQ(rtcoll::SetSizeProf_Take(rows, 8, &c, &it, &em), 0);   // a clean window at entry: no earlier test leaks rows
    rtcoll::SetSizeProf_MarkThisThread();
    rtcoll::g_setSizeProfOn = 1;

    ArrayHdr h = Empty();
    rtcoll::RtColl_ArrayReserve_18005f980(&h);           // grow0 from 0: 16384 iterations, all empty
    const uint64_t v = 7;
    SetSizeCore(&h, 1, reinterpret_cast<const uint8_t*>(&v), &rtcoll::kSpec_Fill8);  // append: 1 iteration, not empty  // CAST-OK: elem_stride
    SetSizeCore(&h, 0, nullptr, &rtcoll::kSpec_Fill8);  // clear: 0 iterations
    rtcoll::g_setSizeProfOn = 0;

    const int n = rtcoll::SetSizeProf_Take(rows, 8, &c, &it, &em);
    CHECK_EQ(n, 1);
    CHECK_EQ(c, 3ull);
    CHECK_EQ(it, 0x4001ull);
    CHECK_EQ(em, 0x4000ull);
    CHECK(rows[0].spec == &rtcoll::kSpec_Fill8);
    CHECK_EQ(rows[0].calls[rtcoll::kSetSizeGrow0], 1ull);
    CHECK_EQ(rows[0].iters[rtcoll::kSetSizeGrow0], 0x4000ull);
    CHECK_EQ(rows[0].calls[rtcoll::kSetSizeAppend], 1ull);
    CHECK_EQ(rows[0].calls[rtcoll::kSetSizeClear], 1ull);
    CHECK_EQ(rows[0].reallocs, 1ull);
    CHECK_EQ(rows[0].maxNewSize, 0x4000);
    CHECK_EQ(rtcoll::SetSizeProf_Take(rows, 8, &c, &it, &em), 0);   // the take reset the window
    rtcoll::ArrayDtorImpl(&h);
}
