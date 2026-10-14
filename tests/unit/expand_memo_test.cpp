// ============================================================================
// tests/unit/expand_memo_test.cpp -- block SAVELOAD: the ExpandRelativePath
//   memo returns an entry only under the context it was stored under
// ============================================================================
// The memo (src/engine/object/expand_memo.{h,cpp}) is keyed on the request's
// characters + appendIdentifier, and every entry is dropped when the context
// (filesystem pointer, data-directory characters, package-index generation)
// changes. What the entry holds is the body's own output; the debug exe checks
// every hit against the body on the game's real request stream
// (src/engine/io/external_path.cpp). Here: the bookkeeping.
// ============================================================================
#include "unit.h"
#include "object/expand_memo.h"
#include "io/packfile_generation.h"

#include <string>

using namespace RtObject;

namespace {
int g_fsA = 0, g_fsB = 0;   // two distinct "filesystem" addresses
const char* const kDir = "C:\\GOG Games\\Banished\\WinData\\";

const std::string* Find(const char* p, bool app, const void* fs, const char* dir, uint32_t gen) {
    return ExpandMemoFind(p, static_cast<int>(std::string(p).size()), app, fs, dir, gen);
}
void Store(const char* p, bool app, const void* fs, const char* dir, uint32_t gen, const char* r) {
    ExpandMemoStore(p, static_cast<int>(std::string(p).size()), app, fs, dir, gen, r,
                    static_cast<int>(std::string(r).size()));
}
}  // namespace

UNIT_TEST(expand_memo_hit_only_for_the_same_request) {
    ExpandMemoClear();
    CHECK(Find("Template\\Tree.rsc:description", true, &g_fsA, kDir, 7) == nullptr);
    Store("Template\\Tree.rsc:description", true, &g_fsA, kDir, 7, "Template\\Tree.rsc:description");
    const std::string* h = Find("Template\\Tree.rsc:description", true, &g_fsA, kDir, 7);
    CHECK(h != nullptr && *h == "Template\\Tree.rsc:description");
    // a different flag, a prefix, a longer name, a case variant: all misses
    CHECK(Find("Template\\Tree.rsc:description", false, &g_fsA, kDir, 7) == nullptr);
    CHECK(Find("Template\\Tree.rsc:descriptio", true, &g_fsA, kDir, 7) == nullptr);
    CHECK(Find("Template\\Tree.rsc:descriptionX", true, &g_fsA, kDir, 7) == nullptr);
    CHECK(Find("template\\Tree.rsc:description", true, &g_fsA, kDir, 7) == nullptr);
    CHECK_EQ(ExpandMemoSize(), static_cast<size_t>(1));
}

UNIT_TEST(expand_memo_flag_is_part_of_the_key) {
    ExpandMemoClear();
    Store("a.rsc", true, &g_fsA, kDir, 1, "a.rsc:resource");
    Store("a.rsc", false, &g_fsA, kDir, 1, "a.rsc");
    CHECK(*Find("a.rsc", true, &g_fsA, kDir, 1) == "a.rsc:resource");
    CHECK(*Find("a.rsc", false, &g_fsA, kDir, 1) == "a.rsc");
}

UNIT_TEST(expand_memo_drops_everything_when_the_package_index_changes) {
    ExpandMemoClear();
    Store("a.rsc", true, &g_fsA, kDir, 1, "a.rsc:resource");
    CHECK(Find("a.rsc", true, &g_fsA, kDir, 2) == nullptr);
    CHECK_EQ(ExpandMemoSize(), static_cast<size_t>(0));
    // and the old generation does not bring it back
    CHECK(Find("a.rsc", true, &g_fsA, kDir, 1) == nullptr);
}

UNIT_TEST(expand_memo_drops_everything_when_the_filesystem_changes) {
    ExpandMemoClear();
    Store("a.rsc", true, &g_fsA, kDir, 1, "a.rsc:resource");
    CHECK(Find("a.rsc", true, &g_fsB, kDir, 1) == nullptr);
    CHECK_EQ(ExpandMemoSize(), static_cast<size_t>(0));
}

UNIT_TEST(expand_memo_drops_everything_when_the_data_directory_changes) {
    ExpandMemoClear();
    Store("a.rsc", true, &g_fsA, kDir, 1, "a.rsc:resource");
    CHECK(Find("a.rsc", true, &g_fsA, "D:\\Other\\WinData\\", 1) == nullptr);
    CHECK_EQ(ExpandMemoSize(), static_cast<size_t>(0));
    Store("a.rsc", true, &g_fsA, kDir, 1, "a.rsc:resource");
    CHECK(Find("a.rsc", true, &g_fsA, nullptr, 1) == nullptr);
    Store("a.rsc", true, &g_fsA, nullptr, 1, "a.rsc:resource");
    CHECK(Find("a.rsc", true, &g_fsA, nullptr, 1) != nullptr);
    CHECK(Find("a.rsc", true, &g_fsA, "", 1) == nullptr);   // null and "" differ
}

UNIT_TEST(expand_memo_is_bounded) {
    ExpandMemoClear();
    for (int i = 0; i < 70000; ++i) {
        const std::string p = "n" + std::to_string(i) + ".rsc";
        Store(p.c_str(), true, &g_fsA, kDir, 3, p.c_str());
    }
    CHECK(ExpandMemoSize() <= static_cast<size_t>(1u << 16));
    CHECK(Find("n69999.rsc", true, &g_fsA, kDir, 3) != nullptr);
    ExpandMemoClear();
}

UNIT_TEST(packfile_generation_counts_changes) {
    const uint32_t g0 = IO::PackfileIndexGeneration();
    IO::BumpPackfileIndexGeneration();
    IO::BumpPackfileIndexGeneration();
    CHECK_EQ(IO::PackfileIndexGeneration(), g0 + 2u);
}
