// ============================================================================
// tests/unit/dtor_check_test.cpp — dtor_check.h itself (B3X W1)
// ============================================================================
// The suite proves what the retired slot-0 flags used to prove through three
// helpers: CheckArrayDelete (the vector arm's replacement), Recording<T> with
// its recording class-scope operator delete (flags 1's free), and
// HasClassDelete (D0 never reaches the global operator delete). These tests
// show each helper can FAIL — a helper that always passes proves nothing.
// ============================================================================
#include "unit.h"
#include "dtor_check.h"

#include <type_traits>

namespace {

int         g_n = 0;
const void* g_order[8] = {};

// Logs `this` once per destruction: what every stand-in in the suite does.
struct Logs {
    virtual ~Logs() { if (g_n < 8) g_order[g_n] = this; ++g_n; }
    int pad = 0;
};

// Logs nothing: CheckArrayDelete must see zero runs and fail.
struct Silent {
    virtual ~Silent() {}
    int pad = 0;
};

// Logs into the slot it is NOT destroyed in: the order comes out upward.
int g_upwardNext = 0;
struct LogsUpward {
    virtual ~LogsUpward() {
        const int slot = 2 - g_upwardNext++;
        if (slot >= 0) g_order[slot] = this;
        ++g_n;
    }
};

struct WithOwnDelete {
    virtual ~WithOwnDelete() {}
    static void operator delete(void* p) { std::free(p); }
};
struct Plain {
    virtual ~Plain() {}
};

// Runs `fn` and returns how many CHECK failures it recorded, then takes them
// back off the global count (they are the expected outcome here).
template <class Fn>
int FailuresOf(Fn fn) {
    const int before = unit::Failures();
    fn();
    const int added = unit::Failures() - before;
    unit::Failures() = before;
    return added;
}

}  // namespace

UNIT_TEST(dtor_check_array_delete_passes_on_three_downward_destructions) {
    CHECK_EQ(FailuresOf([] { CHECK_ARRAY_DELETE(Logs, g_n, g_order); }), 0);
    CHECK_EQ(g_n, 3);
    CHECK(g_order[0] != g_order[1]);
    CHECK(static_cast<const char*>(g_order[0]) > static_cast<const char*>(g_order[2]));   // downward
}

UNIT_TEST(dtor_check_array_delete_fails_on_a_missing_or_upward_destructor_log) {
    // No destructor run observed: the helper must report it.
    CHECK_EQ(FailuresOf([] { CHECK_ARRAY_DELETE(Silent, g_n, g_order); }), 1);
    // Three runs, but recorded in the wrong (upward) order.
    g_upwardNext = 0;
    CHECK_EQ(FailuresOf([] { CHECK_ARRAY_DELETE(LogsUpward, g_n, g_order); }), 1);
    CHECK_EQ(g_n, 3);
}

UNIT_TEST(dtor_check_recording_delete_sees_d0_and_not_d1) {
    // D0 (`delete`) hands the block to the class-scope operator delete once.
    Logs* r = dtor_check::NewOnMalloc<dtor_check::Recording<Logs>>();
    g_n = 0; dtor_check::ResetFrees();
    delete r;
    CHECK_EQ(g_n, 1);
    CHECK_EQ(dtor_check::g_frees, 1);
    CHECK(dtor_check::g_lastFree == static_cast<const void*>(r));

    // D1 (`p->~T()`, and the raw compiler slot 0) frees nothing; D0 (the raw
    // compiler slot 1) is a different entry that does.
    alignas(dtor_check::Recording<Logs>) unsigned char buf[sizeof(dtor_check::Recording<Logs>)];
    Logs* s = ::new (static_cast<void*>(buf)) dtor_check::Recording<Logs>();
    g_n = 0; dtor_check::ResetFrees();
    dtor_check::CallD1(s);
    CHECK_EQ(g_n, 1);
    CHECK_EQ(dtor_check::g_frees, 0);
    Logs* h = dtor_check::NewOnMalloc<dtor_check::Recording<Logs>>();
    CHECK(dtor_check::CompilerEntry<const void*>(h, 0) != dtor_check::CompilerEntry<const void*>(h, 1));
    g_n = 0; dtor_check::ResetFrees();
    dtor_check::CallD0(h);
    CHECK_EQ(g_n, 1);
    CHECK_EQ(dtor_check::g_frees, 1);
    CHECK(dtor_check::g_lastFree == static_cast<const void*>(h));

    // HasClassDelete tells a class-scope operator delete from the global one.
    CHECK(dtor_check::HasClassDelete<WithOwnDelete>::value);
    CHECK(!dtor_check::HasClassDelete<Plain>::value);
    CHECK(dtor_check::HasClassDelete<dtor_check::Recording<Plain>>::value);
}
