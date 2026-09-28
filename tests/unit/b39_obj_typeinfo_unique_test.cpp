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
// WHAT THE UNIT HARNESS CAN AND CANNOT SEE. The boot chain is two
// populations: (1) the Runtime `DEFINE_TYPE` statics built at CRT init and
// (2) the exe's `DEFINE_EXE_TYPE` records built by `InvokeAllRegistrations`.
// (A third, the MINTED records, was emptied at ENDGAME B.3.9 close, and the mint
// itself -- with the test that pinned its table empty of records -- was deleted
// at B3R W0-SM M3.) `make unit` links only part of (1) (the tree sources in
// tests/unit/sources.mk — today `Object::Internal` — plus whatever the tests
// themselves define) and none of (2). So:
//   * the test walks the unit process's real `_firstTypeInfo` chain — population
//     (1) as far as it is linked — and asserts every name's CRC is unique;
//   * population (2), and the Runtime DEFINE_TYPEs not linked here, are NOT
//     covered: the whole-chain check is the boot's `Post-Startup
//     _firstTypeInfo` count, which the coordinator's gate reads.
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
