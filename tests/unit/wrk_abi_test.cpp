// ============================================================================
// tests/unit/wrk_abi_test.cpp — ENDGAME B.3.8 block WRK (B38_RULES.md B8-14)
//
// The ABI claims block WRK makes about the Work family, measured on the
// SHIPPED HEADERS (the exe_class_abi_test.cpp model):
//
// PART A — LAYOUT. sizeof / alignof of `Work`, `AutoPickupWork` and every
//   converted leaf against retail's create-function MemAlloc (B38_RULES §1.3),
//   and the Work base field offsets (retail ctor 0x1400f72e0, Persist 0x1400f7400).
// PART B — SLOT ORDER, read off pointers to VIRTUAL members (Itanium PMF =
//   1 + vtable byte offset; adjustment must be 0 — there is no multiple
//   inheritance in the exe). Work's 13 slots in retail order; every leaf
//   override lands on retail's index.
// PART C — SHAPE: abstract bases, concrete leaves, no virtual destructor.
// PART D — the one hidden-pointer member (slot 10 GetTarget, retail RCX = this,
//   RDX = the return buffer): a raw retail-order call through a compiler
//   vtable of the same declaration shape delivers (this, retbuf) in that order.
//
// NOT PROVEN HERE: the member bodies (they stand on the address registry and
// cannot link into a host test) — the coordinator's run and resave gate do.
// ============================================================================
#include "unit.h"

#include "Work/Work.h"
#include "Work/AutoPickupWork.h"
#include "Work/BuildRoad.h"
#include "Work/BuildStructure.h"
#include "Work/ClearArea.h"
#include "Work/ClearStorage.h"
#include "Work/ConsumeProduce.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace {

template <typename PMF>
uintptr_t SlotOffset(PMF pmf) {
    static_assert(sizeof(PMF) == 2 * sizeof(uintptr_t), "Itanium PMF is two words");
    uintptr_t words[2];
    std::memcpy(words, &pmf, sizeof words);
    if ((words[0] & 1u) == 0) return ~uintptr_t(0);   // not virtual
    if (words[1] != 0) return ~uintptr_t(0);           // this-adjusting: not single inheritance
    return words[0] - 1;
}

// ===========================================================================
// PART A — LAYOUT
// ===========================================================================
UNIT_TEST(wrk_base_sizes_match_retail) {
    CHECK_EQ(sizeof(Work),            (size_t)0x48);
    CHECK_EQ(alignof(Work),           (size_t)8);
    CHECK_EQ(sizeof(AutoPickupWork),  (size_t)0x48);   // adds no bytes
    CHECK_EQ(offsetof(Work, field_10), (size_t)0x10);
    CHECK_EQ(offsetof(Work, field_18), (size_t)0x18);
    CHECK_EQ(offsetof(Work, field_20), (size_t)0x20);
    CHECK_EQ(offsetof(Work, field_24), (size_t)0x24);
    CHECK_EQ(offsetof(Work, field_28), (size_t)0x28);
    CHECK_EQ(offsetof(Work, field_2c), (size_t)0x2c);
    CHECK_EQ(offsetof(Work, field_30), (size_t)0x30);
    CHECK_EQ(offsetof(Work, field_38), (size_t)0x38);
    CHECK_EQ(offsetof(Work, field_40), (size_t)0x40);
}

// ===========================================================================
// PART B — SLOT ORDER (retail table 0x140161a80, 13 slots)
// ===========================================================================
UNIT_TEST(wrk_base_slot_order) {
    CHECK_EQ(SlotOffset(&Work::DeletingDtor), (uintptr_t)0x00);   // 0x14002ca60
    CHECK_EQ(SlotOffset(&Work::TypeName),     (uintptr_t)0x08);   // 0x14002ca50
    CHECK_EQ(SlotOffset(&Work::Type),         (uintptr_t)0x10);   // 0x14002ca40
    CHECK_EQ(SlotOffset(&Work::Persist),      (uintptr_t)0x18);   // 0x1400f7400
    CHECK_EQ(SlotOffset(&Work::Execute),      (uintptr_t)0x20);   // pure
    CHECK_EQ(SlotOffset(&Work::Slot5),        (uintptr_t)0x28);   // 0x1400f7540
    CHECK_EQ(SlotOffset(&Work::OnComplete),   (uintptr_t)0x30);   // pure
    CHECK_EQ(SlotOffset(&Work::OnCancel),     (uintptr_t)0x38);   // pure
    CHECK_EQ(SlotOffset(&Work::Cleanup),      (uintptr_t)0x40);   // pure
    CHECK_EQ(SlotOffset(&Work::Validate),     (uintptr_t)0x48);   // pure
    CHECK_EQ(SlotOffset(&Work::GetTarget),    (uintptr_t)0x50);   // pure
    CHECK_EQ(SlotOffset(&Work::GetPriority),  (uintptr_t)0x58);   // pure
    CHECK_EQ(SlotOffset(&Work::Slot12),       (uintptr_t)0x60);   // 0x140006b10, 13th and last

    // AutoPickupWork (0x140160fa0) overrides 0, 1, 2, 3 and 8.
    CHECK_EQ(SlotOffset(&AutoPickupWork::DeletingDtor), (uintptr_t)0x00);   // 0x1400294e0
    CHECK_EQ(SlotOffset(&AutoPickupWork::TypeName),     (uintptr_t)0x08);   // 0x1400294d0
    CHECK_EQ(SlotOffset(&AutoPickupWork::Type),         (uintptr_t)0x10);   // 0x1400294c0
    CHECK_EQ(SlotOffset(&AutoPickupWork::Persist),      (uintptr_t)0x18);   // 0x1400e3f80
    CHECK_EQ(SlotOffset(&AutoPickupWork::Cleanup),      (uintptr_t)0x40);   // 0x1400e3f30
}

// ===========================================================================
// PART C — SHAPE
// ===========================================================================
UNIT_TEST(wrk_base_class_shape_rules) {
    CHECK(std::is_polymorphic<Work>::value);
    CHECK(std::is_abstract<Work>::value);             // 4, 6-11 _purecall
    CHECK(std::is_abstract<AutoPickupWork>::value);   // inherits 4, 6, 7, 9-11 pure
    CHECK(!std::has_virtual_destructor<Work>::value);
    CHECK(!std::has_virtual_destructor<AutoPickupWork>::value);
    CHECK((std::is_base_of<Object::Internal, Work>::value));
    CHECK((std::is_base_of<Work, AutoPickupWork>::value));
}

// ===========================================================================
// PART E — the leaves: size against retail's create MemAlloc (B38_RULES §1.3),
// concrete, no virtual destructor, and every override on retail's slot index.
// ===========================================================================
#define WRK_LEAF_SHAPE(Class, Parent, Size)                                     \
    CHECK_EQ(sizeof(Class), (size_t)(Size));                                    \
    CHECK_EQ(alignof(Class), (size_t)8);                                        \
    CHECK(!std::is_abstract<Class>::value);                                     \
    CHECK(!std::has_virtual_destructor<Class>::value);                          \
    CHECK((std::is_base_of<Parent, Class>::value));                             \
    CHECK_EQ(SlotOffset(&Class::DeletingDtor), (uintptr_t)0x00);                \
    CHECK_EQ(SlotOffset(&Class::TypeName),     (uintptr_t)0x08);                \
    CHECK_EQ(SlotOffset(&Class::Type),         (uintptr_t)0x10);                \
    CHECK_EQ(SlotOffset(&Class::Persist),      (uintptr_t)0x18);                \
    CHECK_EQ(SlotOffset(&Class::Execute),      (uintptr_t)0x20);                \
    CHECK_EQ(SlotOffset(&Class::Slot5),        (uintptr_t)0x28);                \
    CHECK_EQ(SlotOffset(&Class::OnComplete),   (uintptr_t)0x30);                \
    CHECK_EQ(SlotOffset(&Class::OnCancel),     (uintptr_t)0x38);                \
    CHECK_EQ(SlotOffset(&Class::Cleanup),      (uintptr_t)0x40);                \
    CHECK_EQ(SlotOffset(&Class::Validate),     (uintptr_t)0x48);                \
    CHECK_EQ(SlotOffset(&Class::GetTarget),    (uintptr_t)0x50);                \
    CHECK_EQ(SlotOffset(&Class::GetPriority),  (uintptr_t)0x58);                \
    CHECK_EQ(SlotOffset(&Class::Slot12),       (uintptr_t)0x60)

UNIT_TEST(wrk_leaf_buildroad) {
    WRK_LEAF_SHAPE(BuildRoad, Work, 0x70);   // create 0x140029560, table 0x140161020
    CHECK_EQ(offsetof(BuildRoad, field_48), (size_t)0x48);
    CHECK_EQ(offsetof(BuildRoad, field_58), (size_t)0x58);
    CHECK_EQ(offsetof(BuildRoad, field_68), (size_t)0x68);
}

UNIT_TEST(wrk_leaf_buildstructure) {
    WRK_LEAF_SHAPE(BuildStructure, Work, 0x70);   // create 0x140029630, table 0x1401610a0
    CHECK_EQ(offsetof(BuildStructure, field_48), (size_t)0x48);
    CHECK_EQ(offsetof(BuildStructure, field_50), (size_t)0x50);
    CHECK_EQ(offsetof(BuildStructure, field_5c), (size_t)0x5c);
    CHECK_EQ(offsetof(BuildStructure, field_60), (size_t)0x60);
    CHECK_EQ(offsetof(BuildStructure, field_68), (size_t)0x68);
}

UNIT_TEST(wrk_leaf_cleararea) {
    WRK_LEAF_SHAPE(ClearArea, Work, 0x78);
    CHECK_EQ(offsetof(ClearArea, field_48), (size_t)0x48);
    CHECK_EQ(offsetof(ClearArea, field_50), (size_t)0x50);
    CHECK_EQ(offsetof(ClearArea, field_58), (size_t)0x58);
    CHECK_EQ(offsetof(ClearArea, field_60), (size_t)0x60);
    CHECK_EQ(offsetof(ClearArea, field_68), (size_t)0x68);
    CHECK_EQ(offsetof(ClearArea, field_70), (size_t)0x70);
    CHECK_EQ(offsetof(ClearArea, field_72), (size_t)0x72);
}

UNIT_TEST(wrk_leaf_clearstorage) {
    WRK_LEAF_SHAPE(ClearStorage, Work, 0x68);
    CHECK_EQ(offsetof(ClearStorage, _field48), (size_t)0x48);
    CHECK_EQ(offsetof(ClearStorage, _field50), (size_t)0x50);
    CHECK_EQ(offsetof(ClearStorage, _field58), (size_t)0x58);
    CHECK_EQ(offsetof(ClearStorage, _field60), (size_t)0x60);
}

UNIT_TEST(wrk_leaf_consumeproduce) {
    WRK_LEAF_SHAPE(ConsumeProduce, Work, 0x78);
    CHECK_EQ(offsetof(ConsumeProduce, field_48), (size_t)0x48);
    CHECK_EQ(offsetof(ConsumeProduce, field_4c), (size_t)0x4c);
    CHECK_EQ(offsetof(ConsumeProduce, field_50), (size_t)0x50);
    CHECK_EQ(offsetof(ConsumeProduce, field_58), (size_t)0x58);
    CHECK_EQ(offsetof(ConsumeProduce, field_60), (size_t)0x60);
    CHECK_EQ(offsetof(ConsumeProduce, field_68), (size_t)0x68);
    CHECK_EQ(offsetof(ConsumeProduce, field_70), (size_t)0x70);
    CHECK_EQ(offsetof(ConsumeProduce, field_74), (size_t)0x74);
}

// ===========================================================================
// PART D — the hidden-pointer member's raw call order
// ===========================================================================
// GCC and MSVC agree on Win64: a member function's `this` is RCX and its first
// declared parameter RDX. Slot 10 is declared `void* GetTarget(void* retbuf)`
// (the buffer is an explicit parameter, not a C++ by-value return, so GCC's
// retslot-before-this rule does not apply); a raw retail-shaped call
// `vt[10](work, retbuf)` must hand the body this == work and retbuf == buffer.
struct WrkGetTargetProbe {
    virtual void* Slot0(void* p) { return p; }
    virtual void* GetTarget(void* retbuf) {
        seenThis = this;
        *reinterpret_cast<uint64_t*>(retbuf) = 0x5a5aull;
        return retbuf;
    }
    void* seenThis = nullptr;
};

UNIT_TEST(wrk_gettarget_raw_call_order) {
    WrkGetTargetProbe probe;
    void** vt = *reinterpret_cast<void***>(&probe);
    uint64_t buf = 0;
    using Fn = void* (*)(void* self, void* retbuf);
    void* r = reinterpret_cast<Fn>(vt[1])(&probe, &buf);
    CHECK(r == &buf);
    CHECK(probe.seenThis == &probe);
    CHECK_EQ(buf, (uint64_t)0x5a5a);
}

}  // namespace
