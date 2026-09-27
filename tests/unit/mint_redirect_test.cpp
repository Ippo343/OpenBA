// B.3.9 COORD-0 (D7): the mint's storage redirect table (standalone/rt_mint_redirect.h),
// driven with fake rows -- no mint tables, no registry, no game.
#include "unit.h"
#include "rt_mint_redirect.h"

#include <cstdint>

namespace {

const unsigned char kSeed[8] = { 0x40, 0x42, 0x0f, 0, 0, 0, 0, 0 };   // 1000000

const RtMintRedirect::RowView kRows[] = {
    { "?_singleton@Foo@NS@@0PEAV12@EA", 0x1801fc670ull, 8, nullptr, 0, false },
    { nullptr,                          0x1801fc6d0ull, 8, nullptr, 0, false },   // by address
    { "?_ticks@Timer@NS@@0_JA",         0x1801fe720ull, 8, kSeed,   8, false },   // kInit row
    { "?_loader@X@@1VLoader@2@A",       0x1801fe7b8ull, 8, nullptr, 0, true  },   // constructed
};
const int kN = (int)(sizeof(kRows) / sizeof(kRows[0]));

}  // namespace

UNIT_TEST(b39_mintredirect_find_by_name_and_by_address) {
    using namespace RtMintRedirect;
    CHECK_EQ(FindRow("?_singleton@Foo@NS@@0PEAV12@EA", kRows, kN), 0);
    CHECK_EQ(FindRow("0x1801fc6d0", kRows, kN), 1);
    CHECK_EQ(FindRow("0x1801FC6D0", kRows, kN), 1);
    // a NAMED row is not found by its VA: its name is the literal the retirement scans for
    CHECK_EQ(FindRow("0x1801fc670", kRows, kN), -1);
    CHECK_EQ(FindRow("?_singleton@Bar@NS@@0PEAV12@EA", kRows, kN), -1);
    CHECK_EQ(FindRow(nullptr, kRows, kN), -1);
    CHECK_EQ(FindRow("0x", kRows, kN), -1);
}

UNIT_TEST(b39_mintredirect_refusals) {
    using namespace RtMintRedirect;
    Table<4> t;
    static void* cellA = nullptr;
    static void* cellB = nullptr;
    CHECK_EQ(t.Add("?_nope@@", &cellA, kRows, kN, false), kUnknownRow);
    CHECK_EQ(t.Add("?_singleton@Foo@NS@@0PEAV12@EA", nullptr, kRows, kN, false), kNullCell);
    CHECK_EQ(t.Add("?_loader@X@@1VLoader@2@A", &cellA, kRows, kN, false), kConstructed);
    CHECK_EQ(t.Add("?_singleton@Foo@NS@@0PEAV12@EA", &cellA, kRows, kN, true), kTooLate);
    CHECK_EQ(t.Count(), 0);
    CHECK_EQ(t.Add("?_singleton@Foo@NS@@0PEAV12@EA", &cellA, kRows, kN, false), kOk);
    CHECK_EQ(t.Add("?_singleton@Foo@NS@@0PEAV12@EA", &cellA, kRows, kN, false), kOk);       // idempotent
    CHECK_EQ(t.Add("?_singleton@Foo@NS@@0PEAV12@EA", &cellB, kRows, kN, false), kConflict);
    CHECK_EQ(t.Count(), 1);
    CHECK(t.CellFor(0) == (void*)&cellA);
    CHECK(t.CellFor(1) == nullptr);
}

UNIT_TEST(b39_mintredirect_never_zeroes_a_written_cell) {
    using namespace RtMintRedirect;
    Table<4> t;
    // a cell something wrote before the redirect (the _firstTypeInfo chain at CRT init):
    // no seed on the row, so the table neither zeroes nor rejects it
    static uint64_t cell = 0xdeadbeefull;
    CHECK_EQ(t.Add("0x1801fc6d0", &cell, kRows, kN, false), kOk);
    CHECK_EQ(cell, 0xdeadbeefull);
    CHECK(t.CellFor(1) == (void*)&cell);
    CHECK_EQ(t.Seeded(), 0);
}

UNIT_TEST(b39_mintredirect_seed_exactly_once) {
    using namespace RtMintRedirect;
    {   // zero cell: seeded at the redirect
        Table<4> t;
        static int64_t ticks = 0;
        CHECK_EQ(t.Add("?_ticks@Timer@NS@@0_JA", &ticks, kRows, kN, false), kOk);
        CHECK_EQ(ticks, (int64_t)1000000);
        CHECK_EQ(t.Seeded(), 1);
        ticks = 42;   // the owner's later write is its own: a repeat redirect does not re-seed
        CHECK_EQ(t.Add("?_ticks@Timer@NS@@0_JA", &ticks, kRows, kN, false), kOk);
        CHECK_EQ(ticks, (int64_t)42);
        CHECK_EQ(t.Seeded(), 1);
    }
    {   // the class's own initialiser already reproduces the seed
        Table<4> t;
        static int64_t ticks = 1000000;
        CHECK_EQ(t.Add("?_ticks@Timer@NS@@0_JA", &ticks, kRows, kN, false), kOk);
        CHECK_EQ(ticks, (int64_t)1000000);
        CHECK_EQ(t.Seeded(), 1);
    }
    {   // written with something else before the redirect: refused, cell untouched
        Table<4> t;
        static int64_t ticks = 7;
        CHECK_EQ(t.Add("?_ticks@Timer@NS@@0_JA", &ticks, kRows, kN, false), kSeedMismatch);
        CHECK_EQ(ticks, (int64_t)7);
        CHECK_EQ(t.Count(), 0);
    }
}

UNIT_TEST(b39_mintredirect_table_full) {
    using namespace RtMintRedirect;
    Table<1> t;
    static void* a = nullptr;
    static void* b = nullptr;
    CHECK_EQ(t.Add("?_singleton@Foo@NS@@0PEAV12@EA", &a, kRows, kN, false), kOk);
    CHECK_EQ(t.Add("0x1801fc6d0", &b, kRows, kN, false), kTableFull);
}
