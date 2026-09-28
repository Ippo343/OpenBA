// ============================================================================
// tests/unit/b39_obj_compiledexternal_test.cpp — ENDGAME B.3.9 OBJ W3
// (B39_RULES D1, rule B9-16): `Object::CompiledExternal` carries its OWN slot 0
// and its own slots 1/2 on the compiler table.
//
// Retail's class is abstract (slots 3, 5, 6, 7 are `_purecall`), so the test
// drives it through a minimal leaf that overrides ONLY the pure slots: slots
// 0, 1 and 2 must then be CompiledExternal's, not External's (the inherited
// External::DeletingDtor ran ~External and skipped ~CompiledExternal — the
// INVENTORY_B finding-6 trap).
// ============================================================================

#include "unit.h"
#include "slot_check.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>

#include "../../runtime/types/object/External.h"
#include "../../runtime/types/object/Internal.h"   // Object::detail::InternalMemAlloc

namespace {

class TestCompiled : public Object::CompiledExternal {
public:
    int32_t           GetFileVersion() const override { return 1; }
    bool              Serialize(void*) override { return true; }
    Object::External* Reconstruct() override { return this; }
    void              Destruct() override {}
};
static_assert(sizeof(TestCompiled) == 0x20, "the leaf adds no bytes");

}  // namespace

UNIT_TEST(b39_obj_compiledexternal_protected_nonvirtual_dtor) {
    CHECK(std::is_abstract<Object::CompiledExternal>::value);   // retail's purecalls
    CHECK(!std::is_destructible<Object::CompiledExternal>::value);
    CHECK(!std::has_virtual_destructor<Object::CompiledExternal>::value);
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
    CHECK(b->DeletingDtor(0) == b);   // flags 0: destroy, no free
}

// Slot 0: frees on `flags & 1`; the ARRAY arm reads the int32 count at
// `this - 8`, walks the 0x20 stride, frees and returns `this - 8` — the
// retired RT_DELDTOR row 0x18000b7e0's two arms.
UNIT_TEST(b39_obj_compiledexternal_deletingdtor_both_arms) {
    void* p = Object::detail::InternalMemAlloc(sizeof(TestCompiled), 8);
    CHECK(p != nullptr);
    Object::Base* one = ::new (p) TestCompiled();
    CHECK(one->DeletingDtor(1) == one);

    uint8_t* block = static_cast<uint8_t*>(Object::detail::InternalMemAlloc(8 + 2 * 0x20, 8));
    CHECK(block != nullptr);
    const int64_t cookie = 2;
    std::memcpy(block, &cookie, sizeof cookie);
    TestCompiled* e0 = ::new (block + 8) TestCompiled();
    ::new (block + 8 + 0x20) TestCompiled();
    CHECK(reinterpret_cast<uint8_t*>(e0->DeletingDtor(3)) == block);
}

// ---- Object::External (ENDGAME B.3.9 OBJ W3, vtable row 55) ----------------
// External's own slot 0 now carries BOTH arms of the retired RT_DELDTOR row
// 0x180006640 (element 0x20): driven through a leaf that overrides only the
// pure slots, so slot 0 is External's.
namespace {
class TestPlainExternal : public Object::External {
public:
    int32_t           GetFileVersion() const override { return 1; }
    bool              Serialize(void*) override { return true; }
    Object::External* Reconstruct() override { return this; }
    void              Destruct() override {}
};
static_assert(sizeof(TestPlainExternal) == 0x20, "the leaf adds no bytes");
}  // namespace

UNIT_TEST(b39_obj_external_deletingdtor_both_arms) {
    CHECK(std::is_abstract<Object::External>::value);
    CHECK(!std::is_destructible<Object::External>::value);
    CHECK(!std::has_virtual_destructor<Object::External>::value);
    CHECK_DTOR_SLOT(Object::External);   // 0x180006640

    void* p = Object::detail::InternalMemAlloc(sizeof(TestPlainExternal), 8);
    CHECK(p != nullptr);
    Object::Base* one = ::new (p) TestPlainExternal();
    CHECK(one->DeletingDtor(1) == one);

    uint8_t* block = static_cast<uint8_t*>(Object::detail::InternalMemAlloc(8 + 3 * 0x20, 8));
    CHECK(block != nullptr);
    const int64_t cookie = 3;
    std::memcpy(block, &cookie, sizeof cookie);
    TestPlainExternal* e0 = ::new (block + 8) TestPlainExternal();
    ::new (block + 8 + 0x20) TestPlainExternal();
    ::new (block + 8 + 0x40) TestPlainExternal();
    CHECK(reinterpret_cast<uint8_t*>(e0->DeletingDtor(3)) == block);
}

// ---- Object::Base (ENDGAME B.3.9 OBJ W3, vtable row 12) --------------------
// Base's slot 0 carries both arms of the retired RT_DELDTOR row 0x1800065b0
// (element 8), driven through a leaf that overrides only the pure TypeName.
namespace {
class TestBaseLeaf : public Object::Base {
public:
    const char* TypeName() const override { return "TestBaseLeaf"; }
};
static_assert(sizeof(TestBaseLeaf) == 8, "one vtable pointer");
}  // namespace

UNIT_TEST(b39_obj_base_deletingdtor_both_arms) {
    CHECK(!std::is_destructible<Object::Base>::value);
    CHECK(!std::has_virtual_destructor<Object::Base>::value);
    CHECK_DTOR_SLOT(Object::Base);   // 0x1800065b0
    CHECK_SLOT(Object::Base, TypeName, 0x08/8);   // _purecall
    CHECK_SLOT(Object::Base, Type,     0x10/8);   // 0x1800c0380

    void* p = Object::detail::InternalMemAlloc(sizeof(TestBaseLeaf), 8);
    CHECK(p != nullptr);
    Object::Base* one = ::new (p) TestBaseLeaf();
    CHECK_EQ(one->Type(), 0u);                    // `XOR EAX,EAX; RET`
    CHECK(one->DeletingDtor(1) == one);

    uint8_t* block = static_cast<uint8_t*>(Object::detail::InternalMemAlloc(8 + 4 * 8, 8));
    CHECK(block != nullptr);
    const int64_t cookie = 4;
    std::memcpy(block, &cookie, sizeof cookie);
    TestBaseLeaf* e0 = nullptr;
    for (int i = 0; i < 4; ++i) {
        TestBaseLeaf* e = ::new (block + 8 + i * 8) TestBaseLeaf();
        if (i == 0) e0 = e;
    }
    CHECK(reinterpret_cast<uint8_t*>(e0->DeletingDtor(3)) == block);
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
