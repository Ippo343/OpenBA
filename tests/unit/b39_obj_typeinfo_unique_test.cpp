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
//   * test 2 (ENDGAME B.3.9 close) pins that population (2) is EMPTY: the
//     generated table declares no BaseTypeInfo and no vtable row any more (it
//     used to read the minted NAME column and assert the union unique -- the
//     check that would have failed on the Internal duplicate);
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

// Population (2) is EMPTY since ENDGAME B.3.9 close: the mint carries storage
// rows only. The generated table must declare no BaseTypeInfo storage and no
// `?_typeInfo@` / `??_7` row -- the check that a retired record never comes back
// through the mint (the generator refuses it too; this pins the emitted file).
struct MintShape {
    int typeInfoRows = 0;   // lines naming a "?_typeInfo@ row
    int vtableRows   = 0;   // lines naming a "??_7 row
    bool typeInfoStorage = false;
    int storageRows  = 0;
};

bool ReadMintShape(MintShape* s) {
    std::FILE* f = std::fopen("standalone/rt_data_mint_tables.inc", "rb");
    if (f == nullptr) return false;
    char line[1024];
    while (std::fgets(line, sizeof line, f)) {
        if (std::strncmp(line, "//", 2) == 0) continue;
        if (std::strstr(line, "kTypeInfoStorage")) s->typeInfoStorage = true;
        if (std::strstr(line, "{ \"?_typeInfo@")) ++s->typeInfoRows;
        if (std::strstr(line, "{ \"??_7")) ++s->vtableRows;
        if (std::strstr(line, "&kStorageBytes[")) ++s->storageRows;
    }
    std::fclose(f);
    return true;
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

UNIT_TEST(b39_close_mint_carries_no_typeinfo_and_no_vtable) {
    MintShape s;
    CHECK(ReadMintShape(&s));               // `make unit` runs from the repo root
    CHECK(!s.typeInfoStorage);
    CHECK_EQ(s.typeInfoRows, 0);
    CHECK_EQ(s.vtableRows, 0);
    CHECK(s.storageRows > 0);               // the parse saw the table it reads
}
