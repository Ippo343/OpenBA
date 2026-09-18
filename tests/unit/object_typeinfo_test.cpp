// ============================================================================
// tests/unit/object_typeinfo_test.cpp — ENDGAME B.3.3 step 3
//
// `Object::BaseTypeInfo` and `Object::Factory`, the two real classes step 3
// introduces, driven on the host. B33_RULES.md §8 names the cases.
//
// WHAT CAN AND CANNOT BE TESTED HERE
// ----------------------------------
// Everything below runs without the port registry, without the Runtime DLL and
// without the mint: `runtime/src/object/type_reflection.cpp` is registry-free by
// construction and takes a malloc allocator seam under -DBANISHED_UNIT_TEST.
//
// What is NOT covered, and only a live boot can be:
//   * that `Object::BaseTypeInfo::_firstTypeInfo` is the cell the minted row's
//     two doors publish (standalone/rt_data_mint.cpp's `MintStorageCell`), and
//     therefore that `[GameMain] Post-Startup _firstTypeInfo` still counts the
//     same entries with the same ids;
//   * the static-init ORDER of real `DEFINE_TYPE` objects against
//     `RtDataMintInit()` — nothing in the exe uses `DEFINE_TYPE` yet, and the
//     splice in `MintStorage()` is written for the day something does;
//   * `Factory::Get()`, which reads the minted `?_singleton@` cell and returns
//     null here by design.
// Those three are called out in the step's report for the coordinator to gate.
//
// THE CHAIN IS PROCESS-GLOBAL. `_firstTypeInfo` is one static, so a test that
// builds a chain must restore it — otherwise the next test in link order
// inherits it. `ChainScope` below is that guard, and it is not politeness: a
// leaked chain would make `Startup` insert another test's types and the failure
// would land in whichever test happened to run second.
// ============================================================================
#include "unit.h"

#include <cstring>
#include <type_traits>

#include "../../runtime/types/object/TypeInfo.h"

namespace {

// ---------------------------------------------------------------------------
// Save/restore the one process-global the reflection system has.
// ---------------------------------------------------------------------------
struct ChainScope {
    ::Object::BaseTypeInfo* saved;
    ChainScope() : saved(::Object::BaseTypeInfo::_firstTypeInfo) {
        ::Object::BaseTypeInfo::_firstTypeInfo = nullptr;
    }
    ~ChainScope() { ::Object::BaseTypeInfo::_firstTypeInfo = saved; }
};

// A `BaseTypeInfo` built the way `DEFINE_TYPE` builds one, but as a local so
// each test owns its own chain. The ctor head-inserts, so the LAST one
// constructed is the head — which is why the helpers below register parents
// first and the walk still comes out correct: `Startup`'s parent pass runs over
// the TREE, in key order, not over the chain.
::Object::Base* NoFactory()            { return nullptr; }
::Object::Base* NoInPlace(void* /*s*/) { return nullptr; }

::Object::BaseTypeInfo Make(const char* name, const char* parent, const char* ext) {
    return ::Object::BaseTypeInfo(name, parent, ext, &NoFactory, &NoInPlace);
}

// ===========================================================================
// BaseTypeInfo
// ===========================================================================

UNIT_TEST(basetypeinfo_layout_is_the_retail_record) {
    static_assert(sizeof(::Object::BaseTypeInfo) == 0x38, "0x38 bytes");
    static_assert(offsetof(::Object::BaseTypeInfo, typeId)           == 0x00, "");
    static_assert(offsetof(::Object::BaseTypeInfo, name)             == 0x08, "");
    static_assert(offsetof(::Object::BaseTypeInfo, parentName)       == 0x10, "");
    static_assert(offsetof(::Object::BaseTypeInfo, fileExtension)    == 0x18, "");
    static_assert(offsetof(::Object::BaseTypeInfo, factoryFn)        == 0x20, "");
    static_assert(offsetof(::Object::BaseTypeInfo, inPlaceFactoryFn) == 0x28, "");
    static_assert(offsetof(::Object::BaseTypeInfo, next)             == 0x30, "");

    // It must stay trivially destructible: 300+ of these are file-scope statics
    // and 14 more live inside standalone/rt_data_mint.cpp's `kStorageBytes`,
    // which memsets them. A destructor would run at exit over storage the mint
    // owns. (Not trivially CONSTRUCTIBLE — the five-argument ctor is the
    // registration.)
    CHECK(std::is_trivially_destructible<::Object::BaseTypeInfo>::value == true);
    CHECK(std::is_standard_layout<::Object::BaseTypeInfo>::value == true);
}

UNIT_TEST(basetypeinfo_ctor_stamps_the_sentinel_and_head_inserts) {
    ChainScope scope;

    ::Object::BaseTypeInfo a = Make("Alpha", "Object::Base", nullptr);
    CHECK_EQ(a.typeId, 0xFFFFFFFFu);          // ONLY Factory::Startup clears it
    CHECK_STREQ(a.name, "Alpha");
    CHECK(a.fileExtension == nullptr);
    CHECK(::Object::BaseTypeInfo::_firstTypeInfo == &a);
    CHECK(a.next == nullptr);

    ::Object::BaseTypeInfo b = Make("Beta", "Alpha", "bta");
    CHECK(::Object::BaseTypeInfo::_firstTypeInfo == &b);   // head insert
    CHECK(b.next == &a);
    CHECK_STREQ(b.fileExtension, "bta");
}

UNIT_TEST(basetypeinfo_strips_the_parent_namespace_only) {
    ChainScope scope;

    // Retail's backward scan keeps everything after the LAST ':'. Only the
    // PARENT is stripped — `name` is stored verbatim, which is what lets a
    // stripped parent's CRC match an unstripped child's `name`.
    ::Object::BaseTypeInfo a = Make("Goal", "Object::External", nullptr);
    CHECK_STREQ(a.parentName, "External");
    CHECK_STREQ(a.name, "Goal");

    ::Object::BaseTypeInfo b = Make("Deep", "A::B::C::D", nullptr);
    CHECK_STREQ(b.parentName, "D");

    // No colon: unchanged, and the pointer is the caller's own.
    const char* plain = "Component";
    ::Object::BaseTypeInfo c = Make("Leaf", plain, nullptr);
    CHECK(c.parentName == plain);

    // An EMPTY parent still gets its single (NUL) byte tested, which is the
    // one arm retail's `q == p` fall-out covers. It must not walk off the front.
    ::Object::BaseTypeInfo d = Make("Root", "", nullptr);
    CHECK_STREQ(d.parentName, "");

    // A trailing colon leaves an empty string, not a read past the NUL.
    ::Object::BaseTypeInfo e = Make("Odd", "Ns::", nullptr);
    CHECK_STREQ(e.parentName, "");

    // A null parent is left null (a root type); nothing is dereferenced.
    ::Object::BaseTypeInfo f = Make("Base", nullptr, nullptr);
    CHECK(f.parentName == nullptr);
}

// ===========================================================================
// The CRC
// ===========================================================================

UNIT_TEST(typename_crc_is_the_utf16_widened_reflected_crc32) {
    // Reflected CRC-32 (poly 0xEDB88320) over the name WIDENED TO UTF-16LE:
    // the id of "Base" is the CRC of "B\0a\0s\0e\0". These expectations are
    // computed here by an independent implementation rather than pasted, so the
    // test cannot agree with the port by copying it.
    struct Ref {
        static uint32_t Crc(const char* s) {
            uint32_t crc = 0xFFFFFFFFu;
            for (const char* p = s; *p; ++p) {
                const uint16_t w = (uint16_t)(int16_t)(signed char)*p;
                const uint8_t bytes[2] = { (uint8_t)(w & 0xFF), (uint8_t)(w >> 8) };
                for (int b = 0; b < 2; ++b) {
                    uint32_t c = (crc & 0xFFu) ^ bytes[b];
                    for (int k = 0; k < 8; ++k)
                        c = (c & 1u) ? ((c >> 1) ^ 0xEDB88320u) : (c >> 1);
                    crc = c ^ (crc >> 8);
                }
            }
            return ~crc;
        }
    };

    const char* const names[] = { "Base", "External", "Component", "CitizenComponent",
                                 "Goal", "Achievement", "UI", "a" };
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        CHECK_EQ(::Object::TypeNameCRC(names[i]), Ref::Crc(names[i]));

    // The empty name leaves crc at 0xFFFFFFFF, so `~crc` is 0 — the same value
    // "not registered" uses. Retail does this too; it is not a bug to fix.
    CHECK_EQ(::Object::TypeNameCRC(""), 0u);

    // Distinct names must not collide for the set the tests below use.
    CHECK(::Object::TypeNameCRC("Alpha") != ::Object::TypeNameCRC("Beta"));
}

// ===========================================================================
// Factory
// ===========================================================================

UNIT_TEST(factory_layout_is_the_set_header_and_the_node_is_0x50) {
    static_assert(sizeof(::Object::Factory) == 0x10, "Factory is 0x10, not 8");
    static_assert(offsetof(::Object::Factory, _types) == 0x00, "Factory IS the Set");
    static_assert(sizeof(::Object::Factory::TypeSet::Node) == 0x50, "TypeNode 0x50");
    static_assert(sizeof(::Object::TypeEntry) == 0x30, "the Pair is 0x30");

    ::Object::Factory f;
    CHECK_EQ(f.GetTypes().GetCount(), 0);
    CHECK(f.GetTypes().GetRoot() == nullptr);
}

// A three-generation hierarchy, wide enough that the tree is not a chain and
// the wanted keys are NOT all at the root. That last point is the whole reason
// this test exists: a descent written against the wrong node layout returns the
// right answer for a key that happens to BE the root and is permanently wrong
// for everything below it (reference_one_bst_find_two_node_layouts.md).
struct Fixture {
    ChainScope scope;
    ::Object::Factory factory;

    ::Object::BaseTypeInfo base;
    ::Object::BaseTypeInfo external;
    ::Object::BaseTypeInfo component;
    ::Object::BaseTypeInfo citizen;
    ::Object::BaseTypeInfo work;
    ::Object::BaseTypeInfo goal;

    Fixture()
        : base(Make("Base", nullptr, nullptr)),
          external(Make("External", "Object::Base", "rsc")),
          component(Make("Component", "Object::Base", nullptr)),
          citizen(Make("CitizenComponent", "Component", nullptr)),
          work(Make("WorkComponent", "Component", nullptr)),
          goal(Make("Goal", "Object::External", "gol")) {
        factory.Startup();
    }
};

UNIT_TEST(factory_startup_crcs_every_name_and_inserts_every_type) {
    Fixture f;

    CHECK_EQ(f.factory.GetTypes().GetCount(), 6);

    // Startup is the ONLY thing that clears the 0xFFFFFFFF sentinel.
    CHECK_EQ(f.base.typeId,      ::Object::TypeNameCRC("Base"));
    CHECK_EQ(f.citizen.typeId,   ::Object::TypeNameCRC("CitizenComponent"));
    CHECK(f.goal.typeId != 0xFFFFFFFFu);

    // The key and the record's copy of it must agree — the invariant the
    // profile build asserts ("typePtr->_type == type && it->key == type").
    for (::Object::Factory::TypeSet::Iterator it = f.factory.GetTypes().Begin();
         it != f.factory.GetTypes().End(); ++it) {
        CHECK_EQ(it.GetNode()->element.typeId, it.GetNode()->element.record.typeId);
    }
}

UNIT_TEST(factory_find_works_at_the_root_and_two_levels_down) {
    Fixture f;

    // Locate the root, then prove at least one wanted key is NOT it and is
    // still found — plus one that is two levels below the root.
    const ::Object::Factory::TypeSet::Node* root = f.factory.GetTypes().GetRoot();
    CHECK(root != nullptr);

    int found = 0;
    int depthTwoOrMore = 0;
    const char* const names[] = { "Base", "External", "Component",
                                 "CitizenComponent", "WorkComponent", "Goal" };
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        const uint32_t id = ::Object::TypeNameCRC(names[i]);
        const ::Object::Factory::TypeSet::Node* n = f.factory.GetTypes().Find(id);
        CHECK(n != nullptr);
        if (!n) continue;
        ++found;
        CHECK_EQ(n->element.typeId, id);
        // Depth, measured by walking parents back to the root.
        int depth = 0;
        for (const ::Object::Factory::TypeSet::Node* p = n; p && p != root; p = p->parent)
            ++depth;
        if (depth >= 2) ++depthTwoOrMore;
    }
    CHECK_EQ(found, 6);
    // Six random-ish CRC keys in a balanced AVL tree give a height of at least
    // 2; if this ever became 0 the test above would have stopped proving
    // anything about the descent and should be rebuilt with more keys.
    CHECK(depthTwoOrMore >= 1);

    // A key that was never registered is a miss, not a wrong node.
    CHECK(f.factory.GetTypes().Find(::Object::TypeNameCRC("NotRegistered")) == nullptr);
    CHECK_EQ(f.factory.GetTypeIdFromName("NotRegistered", true), 0u);
    CHECK(f.factory.IsTypeValid(::Object::TypeNameCRC("NotRegistered")) == false);
}

UNIT_TEST(factory_lookups_answer_the_retail_surface) {
    Fixture f;

    const uint32_t idBase      = ::Object::TypeNameCRC("Base");
    const uint32_t idExternal  = ::Object::TypeNameCRC("External");
    const uint32_t idComponent = ::Object::TypeNameCRC("Component");
    const uint32_t idCitizen   = ::Object::TypeNameCRC("CitizenComponent");
    const uint32_t idGoal      = ::Object::TypeNameCRC("Goal");

    CHECK_EQ(f.factory.GetTypeIdFromName("CitizenComponent", true), idCitizen);
    CHECK_STREQ(f.factory.GetTypeName(idCitizen), "CitizenComponent");
    CHECK(f.factory.IsTypeValid(idGoal) == true);

    // Parent links, filled by Startup's second pass over the TREE.
    CHECK_EQ(f.factory.GetParentType(idCitizen),  idComponent);
    CHECK_EQ(f.factory.GetParentType(idComponent), idBase);
    CHECK_EQ(f.factory.GetParentType(idBase),      0u);     // a root

    // Derivation walks those links, three generations deep.
    CHECK(f.factory.IsDerivedFrom(idCitizen, idCitizen)   == true);   // reflexive
    CHECK(f.factory.IsDerivedFrom(idCitizen, idComponent) == true);
    CHECK(f.factory.IsDerivedFrom(idCitizen, idBase)      == true);
    CHECK(f.factory.IsDerivedFrom(idCitizen, idExternal)  == false);
    CHECK(f.factory.IsDerivedFrom(idGoal,    idBase)      == true);

    // Children: Component got two, Base got two, and the growth rule
    // (capacity = n*3 only on a growth of a NON-EMPTY array) is what the second
    // push exercises.
    CHECK_EQ(f.factory.GetChildTypeCount(idComponent), 2);
    CHECK_EQ(f.factory.GetChildTypeCount(idBase),      2);
    CHECK_EQ(f.factory.GetChildTypeCount(idCitizen),   0);

    const uint32_t c0 = f.factory.GetChildType(idComponent, 0);
    const uint32_t c1 = f.factory.GetChildType(idComponent, 1);
    CHECK((c0 == idCitizen && c1 == ::Object::TypeNameCRC("WorkComponent")) ||
          (c1 == idCitizen && c0 == ::Object::TypeNameCRC("WorkComponent")));

    // The +0x18 field is a FILE EXTENSION, matched case-insensitively, and a
    // type that registered none is skipped rather than matching "".
    CHECK_EQ(f.factory.GetTypeIdForFileExtension("rsc"), idExternal);
    CHECK_EQ(f.factory.GetTypeIdForFileExtension("RSC"), idExternal);
    CHECK_EQ(f.factory.GetTypeIdForFileExtension("gol"), idGoal);
    CHECK_EQ(f.factory.GetTypeIdForFileExtension("zzz"), 0u);
    CHECK_EQ(f.factory.GetTypeIdForFileExtension(nullptr), 0u);
}

UNIT_TEST(factory_insert_is_idempotent_on_a_duplicate_key) {
    ChainScope scope;
    ::Object::Factory factory;

    // Two records with the SAME name — the shape a double registration takes.
    // `Collection::Set::Insert` returns the existing node and does not bump the
    // count (retail's equal arm at 0x180030418 -> 0x180030443).
    ::Object::BaseTypeInfo a = Make("Twin", nullptr, nullptr);
    ::Object::BaseTypeInfo b = Make("Twin", nullptr, nullptr);
    (void)a; (void)b;

    factory.Startup();
    CHECK_EQ(factory.GetTypes().GetCount(), 1);
    CHECK_EQ(a.typeId, b.typeId);
}

UNIT_TEST(factory_startup_is_independent_of_chain_order) {
    // The chain is a head-insert list, so declaration order is REVERSED in it.
    // Startup's parent pass runs over the tree in key order, not over the
    // chain, so a child registered before its parent must still link — and that
    // is the property the exe depends on, because 300+ REGISTER_TYPE statics
    // land in link order nobody controls.
    ChainScope scope;
    ::Object::Factory factory;

    ::Object::BaseTypeInfo child  = Make("Child",  "Parent", nullptr);
    ::Object::BaseTypeInfo parent = Make("Parent", "Root",   nullptr);
    ::Object::BaseTypeInfo root   = Make("Root",   nullptr,  nullptr);
    (void)child; (void)parent; (void)root;

    factory.Startup();
    CHECK_EQ(factory.GetTypes().GetCount(), 3);
    CHECK(factory.IsDerivedFrom(::Object::TypeNameCRC("Child"),
                                ::Object::TypeNameCRC("Root")) == true);
    CHECK_EQ(factory.GetChildTypeCount(::Object::TypeNameCRC("Root")), 1);
}

// ===========================================================================
// The POD aliases stay byte-compatible — decision 1's invariant, exercised
// rather than only asserted.
// ===========================================================================
UNIT_TEST(the_old_pods_still_describe_the_new_classes_bytes) {
    ChainScope scope;

    ::Object::BaseTypeInfo real = Make("Aliased", "Object::Base", "ali");

    // An unconverted C body reads the same record through the POD, at the same
    // offsets, with no conversion beyond the cast the B.3.3 bridge uses.
    const RtObject::BaseTypeInfo* pod =
        reinterpret_cast<const RtObject::BaseTypeInfo*>(&real);
    CHECK_EQ(pod->typeId, 0xFFFFFFFFu);
    CHECK_STREQ(pod->name, "Aliased");
    CHECK_STREQ(pod->parentName, "Base");
    CHECK_STREQ(pod->fileExtension, "ali");
    CHECK(pod->next == nullptr);

    // And the node the Set builds is the POD's TypeNode.
    static_assert(sizeof(::Object::Factory::TypeSet::Node) == sizeof(RtObject::TypeNode),
                  "TypeNode alias");
    static_assert(sizeof(::Object::TypeEntry) ==
                      sizeof(uint32_t) * 2 + sizeof(RtObject::TypeRecord),
                  "TypeEntry is key + pad + TypeRecord");
}

}  // namespace
