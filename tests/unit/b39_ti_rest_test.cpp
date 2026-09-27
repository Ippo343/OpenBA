// ============================================================================
// tests/unit/b39_ti_rest_test.cpp — ENDGAME B.3.9 TI-REST (B39_RULES D6, W3):
// the `DEFINE_TYPE` fold of every Runtime namespace except UI and Object.
//
// Each folded class's record is built by `DEFINE_TYPE` in
// runtime/src/<ns>/<ns>_type_records.cpp, which tests/unit/sources.mk links
// here, so the unit process's `_firstTypeInfo` chain carries the REAL records
// the exe carries. Per class these tests check:
//   * the NAME literal and the stripped PARENT literal are the persisted ABI:
//     each is compared with the row of the coordinator's type-table baseline
//     (docs/runtime_port/endgame_b/b39_session/TYPETABLE_414.txt — the
//     `typeId=` / `name=` / `parent=` columns are pasted below, an independent
//     witness to the retired minted row);
//   * after a Factory::Startup over the process chain, `X::TypeId()` and the
//     inline slot-2 `Type()` equal `~CRC32(name)` AND the baseline id, and the
//     slot-1 `TypeName()` returns the record's name;
//   * the record's factory pair is the class's own statics (retail's record
//     points at the class's `CreateNewObject` / `CreateInPlaceObject`; the two
//     `Object::Internal` subclasses share Internal's in-place half);
//   * no two records in the chain share a name.
// Slots 1 and 2 (DECLARE_TYPE's inline `TypeName()` / `Type()`) are checked
// against the record by each namespace's own ABI test (slot-order tests).
//
// STARTUP-EQUIVALENT. `Factory::Startup` (type_reflection.cpp) is run for
// real, over the process chain, with stub records for the parents the unit
// harness does not link (`Base`, `External`, `CompiledExternal`: minted in the
// exe, OBJ's). Startup's second pass faults on a parent missing from the
// chain — exactly as retail's does — so every missing parent is stubbed. The
// stubs are head-inserted and the chain head is restored afterwards.
//
// The factory statics live in the classes' registry-bound TUs, which do not
// link here: this file defines them as null-returning MARKERS so the records'
// `&X::CreateNewObject` / `&X::CreateInPlaceObject` resolve. The test compares
// POINTERS against them, which is the property under test.
// ============================================================================
#include "unit.h"

#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "../../runtime/types/object/TypeInfo.h"
#include "../../runtime/types/object/Internal.h"
#include "../../runtime/types/picking/PickingMesh.h"
#include "../../runtime/types/pathfinding/PointList.h"
#include "../../runtime/types/io/PackageFile.h"

// ===========================================================================
// Factory-pair markers (see the banner).
// ===========================================================================
#define TIR_FACTORY_MARKERS(Class)                                                 \
    ::Object::Base* Class::CreateNewObject() { return nullptr; }                  \
    ::Object::Base* Class::CreateInPlaceObject(void*) { return nullptr; }

TIR_FACTORY_MARKERS(Picking::PickingMesh)
TIR_FACTORY_MARKERS(Pathfinding::PointList)
TIR_FACTORY_MARKERS(IO::PackageFile)

namespace {

struct Expect {
    const ::Object::BaseTypeInfo* record;
    uint32_t (*typeId)();
    const char* name;        // TYPETABLE_414 `name=`
    const char* parent;      // TYPETABLE_414 `parent=` (namespace-stripped)
    uint32_t    id;          // TYPETABLE_414 `typeId=`
    void*       factory;     // the class's own CreateNewObject
    void*       inPlace;     // CreateInPlaceObject (the class's, or Internal's)
};

#define TIR_ROW(Class, Name, Parent, Id)                                           \
    { &Class::GetTypeInfo(), &Class::TypeId, Name, Parent, Id,                     \
      reinterpret_cast<void*>(&Class::CreateNewObject),                            \
      reinterpret_cast<void*>(&Class::CreateInPlaceObject) }

std::vector<Expect> Rows() {
    return {
        // ---- Picking ----
        TIR_ROW(Picking::PickingMesh,   "PickingMesh", "External", 0x9C2301F9u),
        // ---- Pathfinding ----
        TIR_ROW(Pathfinding::PointList, "PointList",   "External", 0x3BC4D9A5u),
        // ---- IO ----
        TIR_ROW(IO::PackageFile,        "PackageFile", "External", 0x3C81C678u),
    };
}

bool InChain(const char* name) {
    for (::Object::BaseTypeInfo* ti = ::Object::BaseTypeInfo::_firstTypeInfo; ti; ti = ti->next)
        if (ti->name && std::strcmp(ti->name, name) == 0) return true;
    return false;
}

::Object::Base* NoFactory()      { return nullptr; }
::Object::Base* NoInPlace(void*) { return nullptr; }

// Stub every parent the chain names but does not hold (Startup's parent pass
// faults on a missing one). The stubs head-insert; the caller restores the head.
struct ParentStubs {
    std::deque<std::string>            names;
    std::deque<::Object::BaseTypeInfo> records;
    void Fill() {
        for (bool added = true; added;) {
            added = false;
            for (::Object::BaseTypeInfo* ti = ::Object::BaseTypeInfo::_firstTypeInfo; ti; ti = ti->next) {
                if (!ti->parentName || InChain(ti->parentName)) continue;
                names.emplace_back(ti->parentName);
                records.emplace_back(names.back().c_str(),
                                     names.back() == "Base" ? nullptr : "Base",
                                     nullptr, &NoFactory, &NoInPlace);
                added = true;
                break;   // the chain head moved: walk again
            }
        }
    }
};

}  // namespace

UNIT_TEST(b39_ti_rest_records_carry_the_retail_literals) {
    for (const Expect& e : Rows()) {
        CHECK(e.record != nullptr);
        CHECK_STREQ(e.record->name, e.name);
        CHECK_STREQ(e.record->parentName, e.parent);
        CHECK(e.record->fileExtension == nullptr);        // every minted row: ext nullptr
        CHECK(e.record->factoryFn == e.factory);
        CHECK(e.record->inPlaceFactoryFn == e.inPlace);
        CHECK(InChain(e.name));                           // built at CRT init
        CHECK_EQ(::Object::TypeNameCRC(e.name), e.id);    // the baseline id IS ~CRC32(name)
    }
}

UNIT_TEST(b39_ti_rest_typeid_after_startup_is_the_name_crc) {
    ::Object::BaseTypeInfo* const saved = ::Object::BaseTypeInfo::_firstTypeInfo;
    // Startup writes every chain record's id; other tests assert the CRT-init
    // sentinel on the process-global records, so the ids are put back after.
    std::vector<std::pair<::Object::BaseTypeInfo*, uint32_t>> ids;
    for (::Object::BaseTypeInfo* ti = saved; ti; ti = ti->next) ids.emplace_back(ti, ti->typeId);
    {
        ParentStubs stubs;
        stubs.Fill();
        ::Object::Factory factory;
        CHECK(factory.Startup());
        for (const Expect& e : Rows()) {
            CHECK_EQ(e.typeId(), e.id);
            CHECK_EQ(e.record->typeId, e.id);
            CHECK(factory.IsTypeValid(e.id));
            CHECK_EQ(factory.GetParentType(e.id), ::Object::TypeNameCRC(e.parent));
        }
        ::Object::BaseTypeInfo::_firstTypeInfo = saved;   // drop the stubs
    }
    for (const auto& p : ids) p.first->typeId = p.second;
}

UNIT_TEST(b39_ti_rest_chain_names_are_unique) {
    std::vector<std::pair<std::string, uint32_t>> seen;
    int dup = 0;
    for (::Object::BaseTypeInfo* ti = ::Object::BaseTypeInfo::_firstTypeInfo; ti; ti = ti->next) {
        const uint32_t crc = ::Object::TypeNameCRC(ti->name);
        for (const auto& s : seen)
            if (s.second == crc) {
                std::printf("  duplicate type record: \"%s\" and \"%s\"\n", s.first.c_str(), ti->name);
                ++dup;
            }
        seen.emplace_back(ti->name ? ti->name : "", crc);
    }
    CHECK_EQ(dup, 0);
    CHECK(seen.size() >= Rows().size() + 1);             // Internal + every TI-REST record
}
