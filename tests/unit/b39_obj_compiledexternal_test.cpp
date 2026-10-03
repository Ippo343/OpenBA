// ============================================================================
// tests/unit/b39_obj_compiledexternal_test.cpp — ENDGAME B.3.9 OBJ W3
// (B39_RULES D1, rule B9-16): `Object::CompiledExternal` carries its OWN slot 0
// and its own slots 1/2 on the compiler table.
//
// Retail's class is abstract (slots 3, 5, 6, 7 are `_purecall`), so the test
// drives it through a minimal leaf that overrides ONLY the pure slots: slots
// 0, 1 and 2 must then be CompiledExternal's, not External's (an inherited
// External slot-0 body ran ~External and skipped ~CompiledExternal — the
// INVENTORY_B finding-6 trap; B3X W1: slot 0 is now `virtual ~T()`, D1 + D0,
// and the compiler chains every level).
// ============================================================================

#include "unit.h"
#include "slot_check.h"
#include "dtor_check.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>

#include "../../runtime/types/object/External.h"
#include "../../runtime/types/object/Internal.h"   // Object::detail::InternalMemAlloc

namespace {

// B3X W1: the leaves record their destructor runs (the order `delete[]`
// destroys in) and carry a recording class-scope operator delete (retail
// flags 1's free, proven; the blocks come from InternalMemAlloc == malloc).
int         g_leafDtors = 0;
const void* g_leafOrder[8] = {};
void NoteLeafDtor(const void* self) {
    if (g_leafDtors < 8) g_leafOrder[g_leafDtors] = self;
    ++g_leafDtors;
}

class TestCompiled : public Object::CompiledExternal {
public:
    ~TestCompiled() override { NoteLeafDtor(this); }
    DTOR_CHECK_RECORDING_DELETE
    int32_t           GetFileVersion() const override { return 1; }
    bool              Serialize(IO::Stream&) override { return true; }
    Object::External* Reconstruct() override { return this; }
    void              Destruct() override {}
};
static_assert(sizeof(TestCompiled) == 0x20, "the leaf adds no bytes");

}  // namespace

UNIT_TEST(b39_obj_compiledexternal_protected_nonvirtual_dtor) {
    CHECK(std::is_abstract<Object::CompiledExternal>::value);   // retail's purecalls
    // B3X W1: a public virtual destructor (retail slot 0 = D1 + D0).
    CHECK(std::is_destructible<Object::CompiledExternal>::value);
    CHECK(std::has_virtual_destructor<Object::CompiledExternal>::value);
    CHECK_EQ(sizeof(Object::CompiledExternal), (size_t)0x20);
}

// ??_7CompiledExternal@Object@@6B@ 0x180154a30, eight slots.
UNIT_TEST(b39_obj_compiledexternal_slot_order_by_pmf) {
    using CE = Object::CompiledExternal;
    CHECK_DTOR_SLOT(CE);   // 0x18000b7e0
    CHECK_SLOT(CE, TypeName,       0x08/8);   // 0x18000b6b0
    CHECK_SLOT(CE, Type,           0x10/8);   // 0x18000b690
    CHECK_SLOT(CE, GetFileVersion, 0x18/8);   // purecall
    CHECK_SLOT(CE, Compile,        0x20/8);   // 0x1800066e0 (External's)
    CHECK_SLOT(CE, Serialize,      0x28/8);   // purecall
    CHECK_SLOT(CE, Reconstruct,    0x30/8);   // purecall
    CHECK_SLOT(CE, Destruct,       0x38/8);   // purecall
}

// ??0CompiledExternal 0x1800bb920: External's zero stores, then the table.
// Slots 1/2 answer CompiledExternal's record, not External's.
UNIT_TEST(b39_obj_compiledexternal_ctor_and_own_type_slots) {
    alignas(8) uint8_t buf[0x20];
    std::memset(buf, 0xCD, sizeof buf);
    TestCompiled* c = ::new (buf) TestCompiled();
    CHECK(c->_path.data == nullptr);
    CHECK(c->_refs._data == nullptr);
    CHECK_EQ(c->_refs._size, 0);
    CHECK_EQ(c->_refs._capacity, 0);
    Object::Base* b = c;
    // The class record (DEFINE_TYPE): the unit harness never runs
    // Factory::Startup, so the id is the ctor's sentinel -- the point is that
    // slots 1/2 read CompiledExternal's record, not External's.
    CHECK_EQ(b->Type(), Object::CompiledExternal::TypeId());
    CHECK(std::strcmp(b->TypeName(), "CompiledExternal") == 0);
    CHECK(std::strcmp(Object::CompiledExternal::GetTypeInfo().parentName, "External") == 0);
    CHECK(Object::CompiledExternal::GetTypeInfo().fileExtension == nullptr);
    CHECK(Object::CompiledExternal::CreateNewObject() == nullptr);
    CHECK(Object::CompiledExternal::CreateInPlaceObject(buf) == nullptr);
    // Retail flags 0: destroy, no free — a dispatching ~Base() is D1.
    g_leafDtors = 0; dtor_check::ResetFrees();
    b->~Base();
    CHECK_EQ(g_leafDtors, 1);
    CHECK(g_leafOrder[0] == c);
    CHECK_EQ(dtor_check::g_frees, 0);
}

// Retail slot 0 frees on `flags & 1`; its ARRAY arm read the int32 count at
// `this - 8`, walked the 0x20 stride, freed and returned `this - 8` — the
// retired RT_DELDTOR row 0x18000b7e0's two arms. B3X W1: flags 1 is `delete`
// through the root pointer (the chain once, then the class-scope operator
// delete, recorded); the vector arm has no Itanium counterpart and `delete[]`
// of a `new T[3]` replaces it.
UNIT_TEST(b39_obj_compiledexternal_deletingdtor_both_arms) {
    void* p = Object::detail::InternalMemAlloc(sizeof(TestCompiled), 8);
    CHECK(p != nullptr);
    Object::Base* one = ::new (p) TestCompiled();
    g_leafDtors = 0; dtor_check::ResetFrees();
    delete one;
    CHECK_EQ(g_leafDtors, 1);
    CHECK_EQ(dtor_check::g_frees, 1);
    CHECK(dtor_check::g_lastFree == p);

    CHECK_ARRAY_DELETE(TestCompiled, g_leafDtors, g_leafOrder);
}

// ---- Object::External (ENDGAME B.3.9 OBJ W3, vtable row 55) ----------------
// External's own slot 0 now carries BOTH arms of the retired RT_DELDTOR row
// 0x180006640 (element 0x20): driven through a leaf that overrides only the
// pure slots, so slot 0 is External's.
namespace {
class TestPlainExternal : public Object::External {
public:
    ~TestPlainExternal() override { NoteLeafDtor(this); }
    DTOR_CHECK_RECORDING_DELETE
    int32_t           GetFileVersion() const override { return 1; }
    bool              Serialize(IO::Stream&) override { return true; }
    Object::External* Reconstruct() override { return this; }
    void              Destruct() override {}
};
static_assert(sizeof(TestPlainExternal) == 0x20, "the leaf adds no bytes");
}  // namespace

UNIT_TEST(b39_obj_external_deletingdtor_both_arms) {
    CHECK(std::is_abstract<Object::External>::value);
    CHECK(std::is_destructible<Object::External>::value);         // B3X W1
    CHECK(std::has_virtual_destructor<Object::External>::value);
    CHECK_DTOR_SLOT(Object::External);   // 0x180006640

    void* p = Object::detail::InternalMemAlloc(sizeof(TestPlainExternal), 8);
    CHECK(p != nullptr);
    Object::Base* one = ::new (p) TestPlainExternal();
    g_leafDtors = 0; dtor_check::ResetFrees();
    delete one;                                                   // retail flags 1
    CHECK_EQ(g_leafDtors, 1);
    CHECK_EQ(dtor_check::g_frees, 1);
    CHECK(dtor_check::g_lastFree == p);

    // The retired vector arm of 0x180006640 (flags 3).
    CHECK_ARRAY_DELETE(TestPlainExternal, g_leafDtors, g_leafOrder);
}

// ---- Object::Base (ENDGAME B.3.9 OBJ W3, vtable row 12) --------------------
// Base's slot 0 carries both arms of the retired RT_DELDTOR row 0x1800065b0
// (element 8), driven through a leaf that overrides only the pure TypeName.
namespace {
class TestBaseLeaf : public Object::Base {
public:
    ~TestBaseLeaf() override { NoteLeafDtor(this); }
    DTOR_CHECK_RECORDING_DELETE
    const char* TypeName() const override { return "TestBaseLeaf"; }
};
static_assert(sizeof(TestBaseLeaf) == 8, "one vtable pointer");
}  // namespace

UNIT_TEST(b39_obj_base_deletingdtor_both_arms) {
    CHECK(std::is_destructible<Object::Base>::value);             // B3X W1
    CHECK(std::has_virtual_destructor<Object::Base>::value);
    // The root's class-scope operator delete (MemFree) is what every D0 below
    // it reaches unless a class declares its own.
    CHECK(dtor_check::HasClassDelete<Object::Base>::value);
    CHECK_DTOR_SLOT(Object::Base);   // 0x1800065b0
    CHECK_SLOT(Object::Base, TypeName, 0x08/8);   // _purecall
    CHECK_SLOT(Object::Base, Type,     0x10/8);   // 0x1800c0380

    void* p = Object::detail::InternalMemAlloc(sizeof(TestBaseLeaf), 8);
    CHECK(p != nullptr);
    Object::Base* one = ::new (p) TestBaseLeaf();
    CHECK_EQ(one->Type(), 0u);                    // `XOR EAX,EAX; RET`
    g_leafDtors = 0; dtor_check::ResetFrees();
    delete one;                                   // retail flags 1
    CHECK_EQ(g_leafDtors, 1);
    CHECK_EQ(dtor_check::g_frees, 1);
    CHECK(dtor_check::g_lastFree == p);

    // The retired vector arm of 0x1800065b0 (flags 3, element 8).
    CHECK_ARRAY_DELETE(TestBaseLeaf, g_leafDtors, g_leafOrder);
}


// Base's reflection record (?_typeInfo@Base@Object@@ 0x1801f9c50), the class's
// own since the W3 fold: the ROOT -- name "Base", no parent, no extension, no
// factory pair -- and Base::TypeId (0x18007e010) reads its +0x00.
UNIT_TEST(b39_obj_base_record_is_the_root) {
    const Object::BaseTypeInfo& ti = Object::Base::GetTypeInfo();
    CHECK(std::strcmp(ti.name, "Base") == 0);
    CHECK(ti.parentName == nullptr);
    CHECK(ti.fileExtension == nullptr);
    CHECK(ti.factoryFn == nullptr);
    CHECK(ti.inPlaceFactoryFn == nullptr);
    CHECK_EQ(Object::Base::TypeId(), ti.typeId);
    bool inChain = false;
    for (const Object::BaseTypeInfo* p = Object::BaseTypeInfo::_firstTypeInfo; p; p = p->next)
        if (p == &ti) inChain = true;
    CHECK(inChain);
}
