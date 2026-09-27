// ============================================================================
// tests/unit/b39_obj_externallist_test.cpp — ENDGAME B.3.9 OBJ (B39_RULES D1,
// rule B9-16): `Object::ExternalList` is concrete.
//
// The class's KEY FUNCTION is `TypeName` (its constructor and slot 0 are inline
// in External.h), so GCC emits its vtable in whichever translation unit defines
// the out-of-line members. In production that is external_class_doors.cpp,
// which stands on the port registry and cannot link into `make unit`; here THIS
// file defines them as RECORDING stubs — exactly the shape
// tests/unit/pathfinding_picking_abi_test.cpp uses for Picking::PickingMesh —
// so the REAL header's inline constructor and inline vector deleting
// destructor run against a real compiler table. No other unit TU defines an
// ExternalList member (external_class.cpp deliberately defines none).
// ============================================================================

#include "unit.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>

#include "../../runtime/types/object/External.h"
#include "../../runtime/types/object/Internal.h"   // Object::detail::InternalMemAlloc

namespace {

int                   g_elDtorCount = 0;
Object::ExternalList* g_elDtorOrder[8] = {};
int                   g_elLastSlot = -1;


template <typename PMF>
uintptr_t ElSlotOffset(PMF pmf) {
    static_assert(sizeof(PMF) == 2 * sizeof(uintptr_t), "Itanium PMF is two words");
    uintptr_t words[2];
    std::memcpy(words, &pmf, sizeof words);
    if ((words[0] & 1u) == 0) return ~uintptr_t(0);   // not virtual
    if (words[1] != 0) return ~uintptr_t(0);          // this-adjustment
    return words[0] - 1;
}

const void* ElVTableOf(const void* o) { return *reinterpret_cast<const void* const*>(o); }

}  // namespace

// ---- the recording out-of-line members (see the banner) --------------------
namespace Object {
const char*       ExternalList::TypeName() const   { g_elLastSlot = 1; return "ExternalList"; }
uint32_t          ExternalList::Type() const       { g_elLastSlot = 2; return 0x7350u; }
uint32_t          ExternalList::TypeId()           { return 0x7360u; }
int32_t           ExternalList::GetFileVersion() const { g_elLastSlot = 3; return 3; }
bool              ExternalList::Serialize(void*)   { g_elLastSlot = 5; return true; }
External*         ExternalList::Reconstruct()      { g_elLastSlot = 6; return this; }
void              ExternalList::Destruct()         { g_elLastSlot = 7; }
External*         ExternalList::Construct()        { g_elLastSlot = 8; return this; }
ExternalList::~ExternalList() {
    if (g_elDtorCount < 8) g_elDtorOrder[g_elDtorCount] = this;
    ++g_elDtorCount;
}
}  // namespace Object

// Concrete, protected non-virtual dtor (so neither destructible from outside
// nor carrying a virtual destructor), 0x30 bytes.
UNIT_TEST(b39_obj_externallist_is_concrete_with_protected_nonvirtual_dtor) {
    CHECK(!std::is_abstract<Object::ExternalList>::value);
    CHECK(!std::is_destructible<Object::ExternalList>::value);
    CHECK(!std::has_virtual_destructor<Object::ExternalList>::value);
    CHECK_EQ(sizeof(Object::ExternalList), (size_t)0x30);
}

// Slot order read off the REAL header's pointers-to-virtual-member
// (??_7ExternalList@Object@@6B@ 0x180154800, nine slots).
UNIT_TEST(b39_obj_externallist_slot_order_by_pmf) {
    using EL = Object::ExternalList;
    CHECK_EQ(ElSlotOffset(&EL::DeletingDtor),   (uintptr_t)0x00);   // 0x180007800
    CHECK_EQ(ElSlotOffset(&EL::TypeName),       (uintptr_t)0x08);   // 0x180007370
    CHECK_EQ(ElSlotOffset(&EL::Type),           (uintptr_t)0x10);   // 0x180007350
    CHECK_EQ(ElSlotOffset(&EL::GetFileVersion), (uintptr_t)0x18);   // 0x1800bfa10
    CHECK_EQ(ElSlotOffset(&EL::Compile),        (uintptr_t)0x20);   // 0x1800066e0 (External's)
    CHECK_EQ(ElSlotOffset(&EL::Serialize),      (uintptr_t)0x28);   // 0x1800bfa20
    CHECK_EQ(ElSlotOffset(&EL::Reconstruct),    (uintptr_t)0x30);   // 0x180007570
    CHECK_EQ(ElSlotOffset(&EL::Destruct),       (uintptr_t)0x38);   // 0x180007640
    CHECK_EQ(ElSlotOffset(&EL::Construct),      (uintptr_t)0x40);   // 0x1800075e0
}

// The real constructor makes retail's factory stores (0x1800074c3..f4): the
// External fields and +0x20 / +0x28 zero as two qwords (so _pad2c too), and
// the compiler's table, the same one for every instance.
UNIT_TEST(b39_obj_externallist_ctor_makes_retails_stores) {
    alignas(8) uint8_t buf[0x30];
    std::memset(buf, 0xCD, sizeof buf);
    Object::ExternalList* l = ::new (buf) Object::ExternalList();
    CHECK(static_cast<void*>(l) == static_cast<void*>(buf));
    CHECK(l->_path.data == nullptr);
    CHECK(l->_refs._data == nullptr);
    CHECK_EQ(l->_refs._size, 0);
    CHECK_EQ(l->_refs._capacity, 0);
    CHECK(l->_resources == nullptr);
    CHECK_EQ(l->_count, 0);
    CHECK_EQ(l->_pad2c, 0);

    alignas(8) uint8_t buf2[0x30];
    Object::ExternalList* l2 = ::new (buf2) Object::ExternalList();
    CHECK(ElVTableOf(l) != nullptr);
    CHECK(ElVTableOf(l) == ElVTableOf(l2));

    g_elLastSlot = -1;
    CHECK(l->GetFileVersion() == 3);
    CHECK_EQ(g_elLastSlot, 3);

    g_elDtorCount = 0;
    CHECK(l->DeletingDtor(0) == l);        // flags 0: destroy, no free
    CHECK(l2->DeletingDtor(0) == l2);
    CHECK_EQ(g_elDtorCount, 2);
}

// Slot 0 FREES on `flags & 1` (host seam: TypeTreeFree == free, so a wrong
// pointer is a heap fault here), and the ARRAY arm reads the int32 count at
// `this - 8`, destroys DOWNWARD with the 0x30 stride, frees and returns
// `this - 8` — both arms exactly as the retired RT_DELDTOR row 0x180007800 ran
// them.
UNIT_TEST(b39_obj_externallist_deletingdtor_both_arms) {
    void* p = Object::detail::InternalMemAlloc(sizeof(Object::ExternalList), 8);
    CHECK(p != nullptr);
    Object::ExternalList* l = ::new (p) Object::ExternalList();
    g_elDtorCount = 0;
    CHECK(l->DeletingDtor(1) == l);
    CHECK_EQ(g_elDtorCount, 1);

    uint8_t* block = static_cast<uint8_t*>(Object::detail::InternalMemAlloc(8 + 3 * 0x30, 8));
    CHECK(block != nullptr);
    const int64_t cookie = 3;                       // the upper dword is 0 too
    std::memcpy(block, &cookie, sizeof cookie);
    Object::ExternalList* e[3];
    for (int i = 0; i < 3; ++i) e[i] = ::new (block + 8 + i * 0x30) Object::ExternalList();
    g_elDtorCount = 0;
    CHECK(reinterpret_cast<uint8_t*>(e[0]->DeletingDtor(3)) == block);
    CHECK_EQ(g_elDtorCount, 3);
    CHECK(g_elDtorOrder[0] == e[2]);
    CHECK(g_elDtorOrder[1] == e[1]);
    CHECK(g_elDtorOrder[2] == e[0]);
}
