// ============================================================================
// tests/unit/array_test.cpp — ENDGAME B.3.2 step 1, `Collection::Array<T>`
// ============================================================================
//
// The cases are B32_RULES's list, and every one of them is a rule read out of
// a shipped body rather than a preference:
//
//   grow from empty            capacity 1, NOT 3 (the treble is gated on
//                              oldSize != 0 — array.cpp's J3 banner)
//   append to non-empty        capacity 3 * newSize, not 3 * oldCapacity
//   exact-fit bulk SetSize(n)  capacity exactly n
//   shrink                     destroy on the TRUNCATED TAIL only, capacity and
//                              buffer kept
//   Clear()                    == SetSize(0): buffer retained
//   the three trait shapes     Array<int32_t>; a copy-ctor element
//                              (System::Filename, the kSpec_StringRef /
//                              kSpec_VideoResource shape); a destructor element
//                              (the kSpec_StringExt / kSpec_ExtArrayElem shape,
//                              modelled by a counting type so the exact
//                              ctor/dtor SEQUENCE on a realloc is asserted)
//   kSpec_PkgEntry shrink arm  0x1800298f0's shrink branch, which a live boot
//                              never takes (COLLECTION_INVENTORY.md §2)
//
// These drive the REAL `rtcoll::SetSizeCore` and the REAL spec rows through the
// template, which is the point: the template is a wrapper, so a test that
// re-implemented either would be testing a copy of the port.
// ============================================================================

#include "unit.h"

#include <cstdint>
#include <cstring>
#include <vector>

#include "collection/Collection.h"
#include "system/Filename.h"
#include "../../runtime/src/collection/array_internal.h"

// The host stub's resolver hook (tests/unit/rt_port_stub.cpp). The PkgEntry
// case installs one; every other case leaves it null.
extern uintptr_t (*g_unitResolveAddr)(int module, uintptr_t ghidraAddr);

namespace {

// ---------------------------------------------------------------------------
// The counting element — the `kSpec_StringExt` / `kSpec_ExtArrayElem` shape
// ---------------------------------------------------------------------------
// Non-trivially copyable AND non-trivially destructible, so `SpecFor<Counted>`
// produces byteCopy=false, a real `assign` and a real `destroy` — the same
// three fields those hand-derived rows carry.
struct Event {
    enum Kind { kDefaultCtor, kCopyCtor, kDtor } kind;
    int value;
};
std::vector<Event>* g_log = nullptr;

struct Counted {
    int value;
    Counted() : value(0) { if (g_log) g_log->push_back({Event::kDefaultCtor, 0}); }
    Counted(int v) : value(v) {}                       // seeding, deliberately unlogged
    Counted(const Counted& o) : value(o.value) {
        if (g_log) g_log->push_back({Event::kCopyCtor, o.value});
    }
    Counted& operator=(const Counted& o) { value = o.value; return *this; }
    ~Counted() { if (g_log) g_log->push_back({Event::kDtor, value}); }
};

int CountOf(const std::vector<Event>& log, Event::Kind k) {
    int n = 0;
    for (const Event& e : log) if (e.kind == k) ++n;
    return n;
}

}  // namespace

// ============================================================================
// Growth policy — the four capacity rules
// ============================================================================

UNIT_TEST(array_grow_from_empty_allocates_capacity_one) {
    Collection::Array<int32_t> a;
    CHECK(a.GetData() == nullptr);
    CHECK_EQ(a.GetSize(), 0);
    CHECK_EQ(a.GetCapacity(), 0);

    a.SetSize(1);
    CHECK_EQ(a.GetSize(), 1);
    CHECK_EQ(a.GetCapacity(), 1);      // the `* 3` is gated on oldSize != 0
    CHECK(a.GetData() != nullptr);
}

UNIT_TEST(array_append_to_nonempty_trebles_to_three_times_newsize) {
    Collection::Array<int32_t> a;
    a.SetSize(1);
    CHECK_EQ(a.GetCapacity(), 1);

    a.SetSize(2);                      // newSize == oldSize + 1, oldSize != 0
    CHECK_EQ(a.GetSize(), 2);
    CHECK_EQ(a.GetCapacity(), 6);      // 3 * newSize, not 3 * oldCapacity

    for (int n = 3; n <= 6; ++n) {     // still inside the capacity: no realloc
        a.SetSize(n);
        CHECK_EQ(a.GetCapacity(), 6);
    }

    // The `+ 1` is measured against _SIZE, not against capacity: the treble
    // fires again only on the append that actually outgrows the buffer.
    a.SetSize(7);
    CHECK_EQ(a.GetCapacity(), 21);     // the 1, 6, 21, 66, 201 walk
}

UNIT_TEST(array_bulk_setsize_allocates_exactly_n) {
    Collection::Array<int32_t> a;
    a.SetSize(10);                     // 10 != 0 + 1, so no treble
    CHECK_EQ(a.GetSize(), 10);
    CHECK_EQ(a.GetCapacity(), 10);
}

UNIT_TEST(array_setsize_to_same_size_is_a_noop) {
    Collection::Array<int32_t> a;
    a.SetSize(4);
    int32_t* before = a.GetData();
    for (int i = 0; i < 4; ++i) a[i] = i + 100;

    a.SetSize(4);                      // the exact-equal early return
    CHECK(a.GetData() == before);
    CHECK_EQ(a[3], 103);
}

UNIT_TEST(array_clear_retains_the_buffer) {
    Collection::Array<int32_t> a;
    a.SetSize(5);
    int32_t* buf = a.GetData();

    a.Clear();                         // == SetSize(0)
    CHECK_EQ(a.GetSize(), 0);
    CHECK_EQ(a.GetCapacity(), 5);      // capacity NEVER shrinks
    CHECK(a.GetData() == buf);         // and the buffer is kept
}

UNIT_TEST(array_setsize_with_fill_copies_the_fill_into_the_new_tail) {
    Collection::Array<int32_t> a;
    const int32_t fill = 0x5A5A5A5A;
    a.SetSize(3, fill);
    CHECK_EQ(a.GetSize(), 3);
    for (int i = 0; i < 3; ++i) CHECK_EQ(a[i], fill);
}

// ============================================================================
// Trait instantiation 1 — Array<int32_t> (kSpec_I32, 0x18002fea0)
// ============================================================================

UNIT_TEST(array_int32_spec_is_the_kspec_i32_row) {
    const rtcoll::ArraySpec* s = Collection::Array<int32_t>::Spec();
    CHECK_EQ(s->stride, 4u);
    CHECK_EQ(s->align, 4);
    CHECK(s->byteCopy);                // is_trivially_copyable<int32_t>
    CHECK(s->destroy == nullptr);      // is_trivially_destructible<int32_t>
    CHECK(s->init == nullptr);         // tail UNINITIALISED, retail's empty else
    CHECK(s->assign != nullptr);
}

UNIT_TEST(array_int32_realloc_keeps_the_old_contents) {
    Collection::Array<int32_t> a;
    a.SetSize(3);
    for (int i = 0; i < 3; ++i) a[i] = i * 7;

    a.SetSize(4);                      // grows and reallocates
    for (int i = 0; i < 3; ++i) CHECK_EQ(a[i], i * 7);
    CHECK_EQ(a.GetCapacity(), 12);
}

// ============================================================================
// Trait instantiation 2 — a copy-ctor element (System::Filename)
// The kSpec_StringRef (0x180034860) / kSpec_VideoResource (0x18004f620) shape:
// `assign` is a CALL to Filename's copy ctor, not a memcpy.
// ============================================================================

UNIT_TEST(array_filename_spec_is_the_copy_ctor_shape) {
    const rtcoll::ArraySpec* s = Collection::Array<System::Filename>::Spec();
    CHECK_EQ(s->stride, 8u);
    CHECK(!s->byteCopy);               // a copy ctor, so NOT a byte copy
    CHECK(s->destroy != nullptr);      // Filename releases its block
    CHECK(s->init != nullptr);         // Filename() is not trivial
}

UNIT_TEST(array_filename_realloc_bumps_and_releases_the_refcount) {
    Collection::Array<System::Filename> a;
    a.SetSize(1);
    a[0] = System::Filename("data/models/house.rsc");
    const char* block = a[0]._data;
    CHECK(block != nullptr);
    CHECK_EQ(RtSystem::StringRefCount(block), 1);

    // The realloc copy-CONSTRUCTS the survivor into the new buffer and then
    // destroys the old one: the refcount goes 1 -> 2 -> 1, and the block lives.
    a.SetSize(2);
    CHECK(a[0]._data == block);
    CHECK_EQ(RtSystem::StringRefCount(block), 1);
    CHECK_STREQ(a[0]._data, "data\\models\\house.rsc");   // AdjustDelimiters

    // The new tail was default-CONSTRUCTED, not left uninitialised, because
    // Filename() is not trivial.
    CHECK(a[1]._data == nullptr);
}

// ============================================================================
// Trait instantiation 3 — a destructor element, and the exact realloc sequence
// ============================================================================

UNIT_TEST(array_counted_realloc_is_copy_then_destroy_then_free) {
    std::vector<Event> log;
    g_log = &log;

    Collection::Array<Counted> a;
    a.SetSize(2);                      // fresh buffer: two default ctors
    CHECK_EQ(CountOf(log, Event::kDefaultCtor), 2);
    CHECK_EQ(CountOf(log, Event::kCopyCtor), 0);
    CHECK_EQ(CountOf(log, Event::kDtor), 0);
    a[0].value = 10;
    a[1].value = 11;

    log.clear();
    a.SetSize(3);                      // reallocates

    // Retail's order, and the reason a move would be wrong: the two survivors
    // are COPY-constructed into the new buffer, the new element is
    // default-constructed, and only THEN are the old two destroyed.
    CHECK_EQ(static_cast<int>(log.size()), 5);
    if (log.size() == 5) {
        CHECK(log[0].kind == Event::kCopyCtor);     CHECK_EQ(log[0].value, 10);
        CHECK(log[1].kind == Event::kCopyCtor);     CHECK_EQ(log[1].value, 11);
        CHECK(log[2].kind == Event::kDefaultCtor);
        CHECK(log[3].kind == Event::kDtor);         CHECK_EQ(log[3].value, 10);
        CHECK(log[4].kind == Event::kDtor);         CHECK_EQ(log[4].value, 11);
    }
    CHECK_EQ(a[0].value, 10);
    CHECK_EQ(a[1].value, 11);

    log.clear();
    a.Clear();                          // SetSize(0) — the shrink arm
    CHECK_EQ(CountOf(log, Event::kDtor), 3);
    CHECK_EQ(a.GetCapacity(), 9);       // 3 * 3 from the append above

    log.clear();
    g_log = nullptr;
}

UNIT_TEST(array_shrink_destroys_only_the_truncated_tail) {
    std::vector<Event> log;
    g_log = &log;

    Collection::Array<Counted> a;
    a.SetSize(4);
    for (int i = 0; i < 4; ++i) a[i].value = i;
    Counted* buf = a.GetData();

    log.clear();
    a.SetSize(2);                       // no realloc: 2 < capacity 4

    CHECK_EQ(static_cast<int>(log.size()), 2);   // ONLY the two that fell off
    if (log.size() == 2) {
        CHECK(log[0].kind == Event::kDtor);  CHECK_EQ(log[0].value, 2);
        CHECK(log[1].kind == Event::kDtor);  CHECK_EQ(log[1].value, 3);
    }
    CHECK_EQ(a.GetCapacity(), 4);       // capacity is NOT reduced
    CHECK(a.GetData() == buf);          // and the buffer is not reallocated
    CHECK_EQ(a[0].value, 0);
    CHECK_EQ(a[1].value, 1);

    log.clear();
    g_log = nullptr;
}

UNIT_TEST(array_destructor_destroys_the_live_elements_then_frees) {
    std::vector<Event> log;
    g_log = &log;
    {
        Collection::Array<Counted> a;
        a.SetSize(3);
        log.clear();
    }                                   // ~Array here
    CHECK_EQ(CountOf(log, Event::kDtor), 3);
    g_log = nullptr;
}

// ============================================================================
// kSpec_PkgEntry (0x1800298f0) — the shrink arm with zero live coverage
// ============================================================================
//
// COLLECTION_INVENTORY.md §2: a live game session only ever GROWS the packfile
// entry directory, so `SetSizeCore`'s shrink-tail branch is the one arm of this
// row nothing exercises. It is driven here through the same explicit-spec
// instantiation the registered shim uses.
//
// The row's two element operations reach the Filename copy ctor (0x1800067d0)
// and the string-release range helper (0x180026b50) BY ADDRESS. On the host
// there is no registry, so the test installs doubles for exactly those two
// addresses and records their arguments: what is under test is which ARM
// SetSizeCore takes and over which RANGE, not what a Filename copy ctor does.

namespace {

struct PkgCall { const uint8_t* base; int count; };
std::vector<PkgCall>* g_pkgDestroyLog = nullptr;
int g_pkgAssignCalls = 0;

void* FakeFilenameCopyCtor(void* dst, const void* src) {
    *static_cast<uint64_t*>(dst) = *static_cast<const uint64_t*>(src);
    ++g_pkgAssignCalls;
    return dst;
}
void FakeReleaseStrings(void* /*ignoredThis*/, uint8_t* base, int count) {
    if (g_pkgDestroyLog) g_pkgDestroyLog->push_back({base, count});
}

uintptr_t PkgResolver(int /*module*/, uintptr_t addr) {
    if (addr == 0x1800067d0ull) return reinterpret_cast<uintptr_t>(&FakeFilenameCopyCtor);
    if (addr == 0x180026b50ull) return reinterpret_cast<uintptr_t>(&FakeReleaseStrings);
    return 0;
}

}  // namespace

UNIT_TEST(array_pkgentry_shrink_arm_destroys_the_tail_in_place) {
    std::vector<PkgCall> destroys;
    g_pkgDestroyLog = &destroys;
    g_pkgAssignCalls = 0;
    g_unitResolveAddr = &PkgResolver;

    // The instantiation the registered 0x1800298f0 shim uses.
    rtcoll::ArrayWithSpec<&rtcoll::kSpec_PkgEntry> a;
    a.SetSize(4);                       // grow: exact fit, capacity 4
    CHECK_EQ(a.GetCapacity(), 4);
    uint8_t* buf = a.GetData();
    CHECK(buf != nullptr);
    std::memset(buf, 0, 4 * 0x10);      // the row's init is null: the tail is
                                        // whatever the allocation held

    destroys.clear();
    a.SetSize(2);                       // THE SHRINK ARM

    CHECK_EQ(a.GetSize(), 2);
    CHECK_EQ(a.GetCapacity(), 4);       // no reallocation, no free
    CHECK(a.GetData() == buf);
    CHECK_EQ(static_cast<int>(destroys.size()), 1);
    if (destroys.size() == 1) {
        // base is the TAIL, not the whole buffer — the distinction the realloc
        // arm and this arm are impossible to tell apart from inside the callee.
        CHECK(destroys[0].base == buf + 2 * 0x10);
        CHECK_EQ(destroys[0].count, 2);
    }
    CHECK_EQ(g_pkgAssignCalls, 0);      // the shrink arm copies nothing

    // And the realloc arm of the same row, for the contrast: it destroys the
    // WHOLE old buffer after copying, and it does copy.
    destroys.clear();
    a.SetSize(5);
    CHECK_EQ(g_pkgAssignCalls, 2);      // the two survivors, copy-constructed
    CHECK_EQ(static_cast<int>(destroys.size()), 1);
    if (destroys.size() == 1) {
        CHECK(destroys[0].base == buf);
        CHECK_EQ(destroys[0].count, 2); // the OLD size, over the OLD buffer
    }

    a.SetSize(0);
    destroys.clear();
    g_pkgDestroyLog = nullptr;
    g_unitResolveAddr = nullptr;
}
