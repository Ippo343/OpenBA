// fix.tavern_counter (docs/retail_bugs.md section 3): Ext::Fixes::TavernSpendOne on a
// storage array that compacts the way StorageComponent::SubtractItem (0x14012d150) does
// when a row reaches zero. Faithful: the counted material is the row re-read after the
// subtract (retail 0x14004a8e6). Fixed: the material loaded before the subtract.
#include "unit.h"
#include "ext/fixes/tavern_counter.h"

namespace {

struct Row { const char* _resource; int _count; int _reserve; };

struct Store {
    Row rows[3];
    int size;
    // SubtractItem's shape: find the material's row, subtract, remove a row that hits
    // zero by moving the tail down (the stale last slot is left in place).
    void Subtract(const char* material, int n) {
        for (int i = 0; i < size; ++i) {
            if (rows[i]._resource != material) continue;
            rows[i]._count -= n;
            if (rows[i]._count <= 0) {
                for (int j = i; j + 1 < size; ++j) rows[j] = rows[j + 1];
                --size;
            }
            return;
        }
    }
};

const char* const kAle  = "ale";
const char* const kWine = "wine";

}  // namespace

UNIT_TEST(fix_tavern_counter_last_unit) {
    // faithful: the last unit of ale is spent, the row now holds wine, wine is counted
    Store a{{{kAle, 1, 0}, {kWine, 5, 0}, {}}, 2};
    const char* counted = Ext::Fixes::TavernSpendOne(&a.rows[0], false,
        [&a](const char* m) { a.Subtract(m, 1); });
    CHECK(counted == kWine);
    CHECK_EQ(a.size, 1);

    // fixed: the same spend counts ale; the stock change is identical
    Store b{{{kAle, 1, 0}, {kWine, 5, 0}, {}}, 2};
    counted = Ext::Fixes::TavernSpendOne(&b.rows[0], true,
        [&b](const char* m) { b.Subtract(m, 1); });
    CHECK(counted == kAle);
    CHECK_EQ(b.size, 1);
    CHECK(b.rows[0]._resource == kWine);
    CHECK_EQ(b.rows[0]._count, 5);
}

UNIT_TEST(fix_tavern_counter_not_last_unit) {
    // a row that keeps units: both arms count the spent good
    for (int fixed = 0; fixed < 2; ++fixed) {
        Store s{{{kAle, 3, 0}, {kWine, 5, 0}, {}}, 2};
        const char* counted = Ext::Fixes::TavernSpendOne(&s.rows[0], fixed != 0,
            [&s](const char* m) { s.Subtract(m, 1); });
        CHECK(counted == kAle);
        CHECK_EQ(s.rows[0]._count, 2);
    }
}

UNIT_TEST(fix_tavern_counter_last_row_stale_tail) {
    // the spent row was the array's last: faithful re-reads the stale slot (same good)
    Store s{{{kWine, 5, 0}, {kAle, 1, 0}, {}}, 2};
    const char* counted = Ext::Fixes::TavernSpendOne(&s.rows[1], false,
        [&s](const char* m) { s.Subtract(m, 1); });
    CHECK(counted == kAle);
    CHECK_EQ(s.size, 1);
}
