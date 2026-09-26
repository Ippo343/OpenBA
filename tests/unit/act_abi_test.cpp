// ============================================================================
// tests/unit/act_abi_test.cpp — ENDGAME B.3.8 block ACT (B38_RULES.md B8-14)
//
// The ABI claims of the Action family, measured on the SHIPPED HEADERS
// (code/Game/Action/*.h):
//   PART A — layout: sizeof / alignof, every field offset the bodies read raw.
//   PART B — slot order, read off pointers to VIRTUAL members (Itanium PMF =
//            1 + vtable byte offset; adjustment 0 = single inheritance):
//            retail table 0x14015faa0, 10 slots, and each leaf's overrides at
//            retail's index (B38_SLOTS.md §6).
//   PART C — class-shape rules: polymorphic, no virtual destructor, Action
//            abstract (slot 4 `_purecall`), every leaf concrete.
//
// NOT PROVEN HERE: `X::Vtable() == *(void**)&obj` needs a constructed object,
// i.e. the member bodies, which stand on the address registry and do not link
// into the host test (the pilot's exe_class_abi_test.cpp PART C proves the
// DEFINE_EXE_VTABLE mechanism itself on this toolchain).
// ============================================================================
#include "unit.h"

#include "Action/Action.h"

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
// Action (the base, retail 0x14015faa0)
// ===========================================================================
UNIT_TEST(act_action_layout) {
    CHECK_EQ(sizeof(Action), (size_t)0x20);
    CHECK_EQ(alignof(Action), (size_t)8);
    CHECK_EQ(offsetof(Action, _entity), (size_t)0x10);
    CHECK_EQ(offsetof(Action, _status), (size_t)0x18);
    CHECK_EQ(sizeof(Action::_entity), (size_t)8);
}

UNIT_TEST(act_action_shape) {
    CHECK(std::is_polymorphic<Action>::value);
    CHECK(std::is_abstract<Action>::value);                 // [4] _purecall 0x14003addc
    CHECK(!std::has_virtual_destructor<Action>::value);
    CHECK((std::is_base_of<Object::Internal, Action>::value));
    CHECK(std::is_trivially_destructible<Object::InternalBasePtr>::value);
}

UNIT_TEST(act_action_slot_order) {
    CHECK_EQ(SlotOffset(&Action::DeletingDtor), (uintptr_t)0x00);   // 0x140021c90
    CHECK_EQ(SlotOffset(&Action::TypeName),     (uintptr_t)0x08);   // 0x140021c80
    CHECK_EQ(SlotOffset(&Action::Type),         (uintptr_t)0x10);   // 0x140021c70
    CHECK_EQ(SlotOffset(&Action::Persist),      (uintptr_t)0x18);   // 0x1400c4b40
    CHECK_EQ(SlotOffset(&Action::AbstractImpl), (uintptr_t)0x20);   // _purecall
    CHECK_EQ(SlotOffset(&Action::Execute),      (uintptr_t)0x28);   // 0x1400c4a90
    CHECK_EQ(SlotOffset(&Action::Enter),        (uintptr_t)0x30);   // 0x1400c4b90
    CHECK_EQ(SlotOffset(&Action::Exit),         (uintptr_t)0x38);   // 0x1400c4ba0
    CHECK_EQ(SlotOffset(&Action::CheckEntity),  (uintptr_t)0x40);   // 0x1400c4a30
    CHECK_EQ(SlotOffset(&Action::GetTarget),    (uintptr_t)0x48);   // 0x1400c4a40 (last)
}

}  // namespace
