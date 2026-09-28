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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>

#include "../../runtime/types/object/External.h"
#include "../../runtime/types/object/Internal.h"   // Object::detail::InternalMemAlloc

namespace Object {
extern const BaseTypeInfo* g_unitCompiledExternalTypeInfo;
}  // namespace Object

namespace {

class TestCompiled : public Object::CompiledExternal {
public:
    int32_t           GetFileVersion() const override { return 1; }
    bool              Serialize(void*) override { return true; }
    Object::External* Reconstruct() override { return this; }
    void              Destruct() override {}
};
static_assert(sizeof(TestCompiled) == 0x20, "the leaf adds no bytes");

template <typename PMF>
uintptr_t CeSlotOffset(PMF pmf) {
    static_assert(sizeof(PMF) == 2 * sizeof(uintptr_t), "Itanium PMF is two words");
    uintptr_t words[2];
    std::memcpy(words, &pmf, sizeof words);
    if ((words[0] & 1u) == 0) return ~uintptr_t(0);   // not virtual
    if (words[1] != 0) return ~uintptr_t(0);          // this-adjustment
    return words[0] - 1;
}

Object::BaseTypeInfo g_compiledRecord;

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
    CHECK_EQ(CeSlotOffset(&CE::DeletingDtor),   (uintptr_t)0x00);   // 0x18000b7e0
    CHECK_EQ(CeSlotOffset(&CE::TypeName),       (uintptr_t)0x08);   // 0x18000b6b0
    CHECK_EQ(CeSlotOffset(&CE::Type),           (uintptr_t)0x10);   // 0x18000b690
    CHECK_EQ(CeSlotOffset(&CE::GetFileVersion), (uintptr_t)0x18);   // purecall
    CHECK_EQ(CeSlotOffset(&CE::Compile),        (uintptr_t)0x20);   // 0x1800066e0 (External's)
    CHECK_EQ(CeSlotOffset(&CE::Serialize),      (uintptr_t)0x28);   // purecall
    CHECK_EQ(CeSlotOffset(&CE::Reconstruct),    (uintptr_t)0x30);   // purecall
    CHECK_EQ(CeSlotOffset(&CE::Destruct),       (uintptr_t)0x38);   // purecall
}

// ??0CompiledExternal 0x1800bb920: External's zero stores, then the table.
// Slots 1/2 answer CompiledExternal's record, not External's.
UNIT_TEST(b39_obj_compiledexternal_ctor_and_own_type_slots) {
    g_compiledRecord.typeId = 0xAE5B8F65u;
    g_compiledRecord.name   = "CompiledExternal";
    ::Object::g_unitCompiledExternalTypeInfo = &g_compiledRecord;

    alignas(8) uint8_t buf[0x20];
    std::memset(buf, 0xCD, sizeof buf);
    TestCompiled* c = ::new (buf) TestCompiled();
    CHECK(c->_path.data == nullptr);
    CHECK(c->_refs._data == nullptr);
    CHECK_EQ(c->_refs._size, 0);
    CHECK_EQ(c->_refs._capacity, 0);
    Object::Base* b = c;
    CHECK_EQ(b->Type(), 0xAE5B8F65u);
    CHECK(std::strcmp(b->TypeName(), "CompiledExternal") == 0);
    CHECK_EQ(Object::CompiledExternal::TypeId(), 0xAE5B8F65u);
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
