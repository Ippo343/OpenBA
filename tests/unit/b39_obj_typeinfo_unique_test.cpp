// ============================================================================
// tests/unit/b39_obj_typeinfo_unique_test.cpp — ENDGAME B.3.9 OBJ (B39_RULES
// D6, "the duplicate"): every type name in the reflection chain is ONE record.
//
// `Factory::Startup` keys the type tree by `~CRC32(name)` and its `Insert`
// SILENTLY keeps the first node on an equal key, so two records for one name
// (the minted `?_typeInfo@Internal@Object@@` beside `DEFINE_TYPE(Object::
// Internal)`, until OBJ retired the minted one) never fault — they only make
// which record wins an accident of chain order. These tests make a second
// record a unit failure.
//
// WHAT THE UNIT HARNESS CAN AND CANNOT SEE. The boot chain is three
// populations: (1) the Runtime `DEFINE_TYPE` statics built at CRT init, (2) the
// MINTED records `MintStorage()` splices in at port init, (3) the exe's
// `DEFINE_EXE_TYPE` records built by `InvokeAllRegistrations`. `make unit` links
// only part of (1) (the tree sources in tests/unit/sources.mk — today
// `Object::Internal` — plus whatever the tests themselves define), none of (3),
// and (2) exists only in the generated standalone/rt_data_mint_tables.inc. So:
//   * test 1 walks the unit process's real `_firstTypeInfo` chain — population
//     (1) as far as it is linked — and asserts every name's CRC is unique;
//   * test 2 adds population (2) by reading the minted records' NAME column out
//     of the generated table (`make unit` runs from the repository root) and
//     asserts the union is unique — the check that would have failed on the
//     Internal duplicate;
//   * population (3), and the Runtime DEFINE_TYPEs not linked here, are NOT
//     covered: the whole-chain check is the boot's `Post-Startup
//     _firstTypeInfo` count, which the coordinator's gate reads (416 -> 415
//     with this retirement).
// ============================================================================

#include "unit.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../runtime/types/object/TypeInfo.h"
#include "../../runtime/types/object/Internal.h"

namespace {

struct NameCrc {
    std::string name;
    uint32_t    crc;
};

// Population (1): the process's CRT-init chain, as Factory::Startup walks it.
std::vector<NameCrc> ChainNames() {
    std::vector<NameCrc> out;
    for (::Object::BaseTypeInfo* ti = ::Object::BaseTypeInfo::_firstTypeInfo;
         ti != nullptr; ti = ti->next)
        out.push_back({ ti->name ? ti->name : "", ::Object::TypeNameCRC(ti->name) });
    return out;
}

// Population (2): the minted records' `name` column. Each kMintTypeInfos row is
//     { "?_typeInfo@...", 0x...ull, &kTypeInfoStorage[i],
//       "<name>", "<parent>", <ext>,
// so the name is the first quoted string on the line after the mangled one.
bool MintedNames(std::vector<NameCrc>& out) {
    std::FILE* f = std::fopen("standalone/rt_data_mint_tables.inc", "rb");
    if (f == nullptr) return false;
    char line[1024];
    bool inTable = false, wantName = false;
    while (std::fgets(line, sizeof line, f)) {
        if (std::strstr(line, "kMintTypeInfos[] = {")) { inTable = true; continue; }
        if (!inTable) continue;
        if (line[0] == '}') break;
        if (wantName) {
            wantName = false;
            const char* q = std::strchr(line, '"');
            const char* e = q ? std::strchr(q + 1, '"') : nullptr;
            if (q && e) {
                std::string n(q + 1, e);
                out.push_back({ n, ::Object::TypeNameCRC(n.c_str()) });
            }
            continue;
        }
        if (std::strstr(line, "{ \"?_typeInfo@")) wantName = true;
    }
    std::fclose(f);
    return true;
}

// The number of (i, j) pairs with equal CRCs; each is printed.
int Duplicates(const std::vector<NameCrc>& v) {
    int dup = 0;
    for (size_t i = 0; i < v.size(); ++i)
        for (size_t j = i + 1; j < v.size(); ++j)
            if (v[i].crc == v[j].crc) {
                std::printf("  duplicate type record: \"%s\" and \"%s\" (crc 0x%08x)\n",
                            v[i].name.c_str(), v[j].name.c_str(), v[i].crc);
                ++dup;
            }
    return dup;
}

}  // namespace

UNIT_TEST(b39_obj_typeinfo_chain_names_are_unique) {
    const std::vector<NameCrc> chain = ChainNames();
    int internal = 0;
    for (const NameCrc& n : chain)
        if (n.name == "Internal") ++internal;
    CHECK_EQ(internal, 1);                 // DEFINE_TYPE(Object::Internal) is linked here
    CHECK(&::Object::Internal::GetTypeInfo() != nullptr);
    CHECK_EQ(Duplicates(chain), 0);
}

UNIT_TEST(b39_obj_typeinfo_chain_plus_minted_names_are_unique) {
    std::vector<NameCrc> all = ChainNames();
    std::vector<NameCrc> minted;
    CHECK(MintedNames(minted));            // `make unit` runs from the repo root
    CHECK(minted.size() > 50);             // the table parsed (103 records today)
    for (const NameCrc& n : minted)
        CHECK(n.name != "Internal");       // the retired duplicate stays retired
    all.insert(all.end(), minted.begin(), minted.end());
    CHECK_EQ(Duplicates(all), 0);
}
