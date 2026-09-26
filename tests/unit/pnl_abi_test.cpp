// ============================================================================
// tests/unit/pnl_abi_test.cpp — ENDGAME B.3.8 block PNL (RULE B8-14)
//
// The exe panel classes (UI::DialogController subclasses) and the four
// UI::DialogControllerConfig subclasses, on their shipped headers:
//   PART A — LAYOUT: sizeof / alignof and every field offset against retail's
//            create-function MemAlloc and the bodies that read the fields.
//   PART B — SLOT ORDER: read off pointers to VIRTUAL members (Itanium PMF =
//            1 + vtable byte offset, adjustment 0 = single inheritance), so a
//            slot declared out of retail order fails here, not in a run.
//   PART C — SHAPE: polymorphic, no virtual destructor, abstract exactly where
//            retail is (SelectResourceUI, slots 11-13 _purecall).
// `Vtable() == *(void**)&obj` for DEFINE_EXE_VTABLE is proven once, on probe
// classes, by exe_class_abi_test.cpp PART C; the panel bodies stand on the
// port registry and do not link into a host test.
// ============================================================================
#include "unit.h"

#include "Dialog/ControlUIConfig.h"
#include "Dialog/ResourceLimitUIConfig.h"
#include "Dialog/StatusIconUIConfig.h"
#include "Dialog/UpgradeUIConfig.h"

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
    if (words[1] != 0) return ~uintptr_t(0);           // this-adjusting
    return words[0] - 1;
}

// ===========================================================================
// The four DialogControllerConfig leaves (9 slots each, sizeof 0x28)
// ===========================================================================
template <typename C>
void CheckConfigSlots() {
    CHECK_EQ(SlotOffset(&C::DeletingDtor),   (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&C::TypeName),       (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&C::Type),           (uintptr_t)0x10);
    CHECK_EQ(SlotOffset(&C::GetFileVersion), (uintptr_t)0x18);
    CHECK_EQ(SlotOffset(&C::Compile),        (uintptr_t)0x20);
    CHECK_EQ(SlotOffset(&C::Serialize),      (uintptr_t)0x28);
    CHECK_EQ(SlotOffset(&C::Reconstruct),    (uintptr_t)0x30);
    CHECK_EQ(SlotOffset(&C::Destruct),       (uintptr_t)0x38);
    CHECK_EQ(SlotOffset(&C::Construct),      (uintptr_t)0x40);   // 9th and last
    CHECK(std::is_polymorphic<C>::value);
    CHECK(!std::is_abstract<C>::value);
    CHECK(!std::has_virtual_destructor<C>::value);
    CHECK((std::is_base_of<UI::DialogControllerConfig, C>::value));
    CHECK_EQ(sizeof(C), (size_t)0x28);
    CHECK_EQ(alignof(C), (size_t)8);
}

UNIT_TEST(pnl_config_layout_and_slots) {
    CheckConfigSlots<ControlUIConfig>();
    CheckConfigSlots<ResourceLimitUIConfig>();
    CheckConfigSlots<StatusIconUIConfig>();
    CheckConfigSlots<UpgradeUIConfig>();
    CHECK_EQ(offsetof(ControlUIConfig, _command),             (size_t)0x20);
    CHECK_EQ(offsetof(ControlUIConfig, _componentType),       (size_t)0x24);
    CHECK_EQ(offsetof(ResourceLimitUIConfig, _resourceIndex), (size_t)0x20);
    CHECK_EQ(offsetof(StatusIconUIConfig, _showOnMainPage),   (size_t)0x20);
    CHECK_EQ(offsetof(UpgradeUIConfig, _target),              (size_t)0x20);
}

}  // namespace
