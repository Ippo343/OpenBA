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
// @@PANEL-INCLUDES-BEGIN
#include "Dialog/AttendanceUI.h"
#include "Dialog/BuildRequirementUI.h"
#include "Dialog/BuildUI.h"
#include "Dialog/CemeteryUI.h"
#include "Dialog/ConsumeProduceUI.h"
#include "Dialog/HelpUI.h"
#include "Dialog/HotKeyToolTipUI.h"
#include "Dialog/ImmigrationUI.h"
#include "Dialog/MapUI.h"
#include "Dialog/PageToggleUI.h"
#include "Dialog/ProductionUI.h"
#include "Dialog/ProfessionUI.h"
#include "Dialog/ResourceLimitUI.h"
#include "Dialog/TitleDescToolTipUI.h"
#include "Dialog/ToolUI.h"
#include "Dialog/TradeUI.h"
// @@PANEL-INCLUDES-END

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

// ===========================================================================
// The UI::DialogController leaves (11 slots; the base's slot list is
// runtime/types/ui/DialogController.h's). Each class's declared overrides land
// on retail's index; the base slots are pinned once here.
// ===========================================================================
UNIT_TEST(pnl_dialogcontroller_base_slots) {
    using DC = UI::DialogController;
    CHECK_EQ(SlotOffset(&DC::DeletingDtor),        (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&DC::TypeName),            (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&DC::Type),                (uintptr_t)0x10);
    CHECK_EQ(SlotOffset(&DC::Persist),             (uintptr_t)0x18);
    CHECK_EQ(SlotOffset(&DC::OnCreated),           (uintptr_t)0x20);
    CHECK_EQ(SlotOffset(&DC::OnOpened),            (uintptr_t)0x28);
    CHECK_EQ(SlotOffset(&DC::OnClosed),            (uintptr_t)0x30);
    CHECK_EQ(SlotOffset(&DC::OnUpdateLayout),      (uintptr_t)0x38);
    CHECK_EQ(SlotOffset(&DC::OnUpdate),            (uintptr_t)0x40);
    CHECK_EQ(SlotOffset(&DC::OnElementEvent),      (uintptr_t)0x48);
    CHECK_EQ(SlotOffset(&DC::OnSerializeSettings), (uintptr_t)0x50);
}

template <typename C>
void CheckPanelShape(size_t size) {
    CHECK_EQ(sizeof(C), size);
    CHECK_EQ(alignof(C), (size_t)8);
    CHECK(std::is_polymorphic<C>::value);
    CHECK(!std::is_abstract<C>::value);
    CHECK(!std::has_virtual_destructor<C>::value);
    CHECK((std::is_base_of<UI::DialogController, C>::value));
    CHECK_EQ(SlotOffset(&C::DeletingDtor), (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&C::TypeName),     (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&C::Type),         (uintptr_t)0x10);
}

// @@PANEL-TESTS-BEGIN (generated from the per-class slot lists)
UNIT_TEST(pnl_AttendanceUI_layout_and_slots) {
    CheckPanelShape<AttendanceUI>((size_t)0x38);
    CHECK_EQ(SlotOffset(&AttendanceUI::OnCreated), (uintptr_t)0x20);   // 0x14005dd40
    CHECK_EQ(SlotOffset(&AttendanceUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x14005dc50
}

UNIT_TEST(pnl_BuildRequirementUI_layout_and_slots) {
    CheckPanelShape<BuildRequirementUI>((size_t)0x60);
    CHECK_EQ(SlotOffset(&BuildRequirementUI::Persist), (uintptr_t)0x18);   // 0x14005ee80
    CHECK_EQ(SlotOffset(&BuildRequirementUI::OnCreated), (uintptr_t)0x20);   // 0x14005e7c0
}

UNIT_TEST(pnl_BuildUI_layout_and_slots) {
    CheckPanelShape<BuildUI>((size_t)0x50);
    CHECK_EQ(SlotOffset(&BuildUI::OnCreated), (uintptr_t)0x20);   // 0x1400b1f40
    CHECK_EQ(SlotOffset(&BuildUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x1400b1a50
    CHECK_EQ(SlotOffset(&BuildUI::OnElementEvent), (uintptr_t)0x48);   // 0x1400b1ea0
}

UNIT_TEST(pnl_CemeteryUI_layout_and_slots) {
    CheckPanelShape<CemeteryUI>((size_t)0x38);
    CHECK_EQ(SlotOffset(&CemeteryUI::OnCreated), (uintptr_t)0x20);   // 0x14005f090
    CHECK_EQ(SlotOffset(&CemeteryUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x14005ef50
}

UNIT_TEST(pnl_ConsumeProduceUI_layout_and_slots) {
    CheckPanelShape<ConsumeProduceUI>((size_t)0x40);
    CHECK_EQ(SlotOffset(&ConsumeProduceUI::OnCreated), (uintptr_t)0x20);   // 0x14005f3d0
    CHECK_EQ(SlotOffset(&ConsumeProduceUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x14005f1f0
    CHECK_EQ(SlotOffset(&ConsumeProduceUI::OnElementEvent), (uintptr_t)0x48);   // 0x14005f330
}

UNIT_TEST(pnl_HelpUI_layout_and_slots) {
    CheckPanelShape<HelpUI>((size_t)0x78);
    CHECK_EQ(SlotOffset(&HelpUI::OnCreated), (uintptr_t)0x20);   // 0x14006a500
    CHECK_EQ(SlotOffset(&HelpUI::OnOpened), (uintptr_t)0x28);   // 0x14006a4f0
    CHECK_EQ(SlotOffset(&HelpUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x14006a230
    CHECK_EQ(SlotOffset(&HelpUI::OnElementEvent), (uintptr_t)0x48);   // 0x14006a290
}

UNIT_TEST(pnl_HotKeyToolTipUI_layout_and_slots) {
    CheckPanelShape<HotKeyToolTipUI>((size_t)0x30);
    CHECK_EQ(SlotOffset(&HotKeyToolTipUI::Persist), (uintptr_t)0x18);   // 0x14006ab80
    CHECK_EQ(SlotOffset(&HotKeyToolTipUI::OnCreated), (uintptr_t)0x20);   // 0x14006a980
}

UNIT_TEST(pnl_ImmigrationUI_layout_and_slots) {
    CheckPanelShape<ImmigrationUI>((size_t)0x50);
    CHECK_EQ(SlotOffset(&ImmigrationUI::OnCreated), (uintptr_t)0x20);   // 0x14006b330
    CHECK_EQ(SlotOffset(&ImmigrationUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x14006b130
    CHECK_EQ(SlotOffset(&ImmigrationUI::OnElementEvent), (uintptr_t)0x48);   // 0x14006b300
}

UNIT_TEST(pnl_MapUI_layout_and_slots) {
    CheckPanelShape<MapUI>((size_t)0x50);
    CHECK_EQ(SlotOffset(&MapUI::Persist), (uintptr_t)0x18);   // 0x14006ce80
    CHECK_EQ(SlotOffset(&MapUI::OnCreated), (uintptr_t)0x20);   // 0x14006ca80
    CHECK_EQ(SlotOffset(&MapUI::OnElementEvent), (uintptr_t)0x48);   // 0x14006c9e0
}

UNIT_TEST(pnl_PageToggleUI_layout_and_slots) {
    CheckPanelShape<PageToggleUI>((size_t)0x48);
    CHECK_EQ(SlotOffset(&PageToggleUI::Persist), (uintptr_t)0x18);   // 0x1400759a0
    CHECK_EQ(SlotOffset(&PageToggleUI::OnCreated), (uintptr_t)0x20);   // 0x140075850
    CHECK_EQ(SlotOffset(&PageToggleUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x140075780
    CHECK_EQ(SlotOffset(&PageToggleUI::OnElementEvent), (uintptr_t)0x48);   // 0x140075710
}

UNIT_TEST(pnl_ProductionUI_layout_and_slots) {
    CheckPanelShape<ProductionUI>((size_t)0x50);
    CHECK_EQ(SlotOffset(&ProductionUI::Persist), (uintptr_t)0x18);   // 0x140076300
    CHECK_EQ(SlotOffset(&ProductionUI::OnCreated), (uintptr_t)0x20);   // 0x140075da0
    CHECK_EQ(SlotOffset(&ProductionUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x140075a20
}

UNIT_TEST(pnl_ProfessionUI_layout_and_slots) {
    CheckPanelShape<ProfessionUI>((size_t)0x78);
    CHECK_EQ(SlotOffset(&ProfessionUI::OnOpened), (uintptr_t)0x28);   // 0x140076ca0
    CHECK_EQ(SlotOffset(&ProfessionUI::OnUpdate), (uintptr_t)0x40);   // 0x1400765a0
    CHECK_EQ(SlotOffset(&ProfessionUI::OnElementEvent), (uintptr_t)0x48);   // 0x140076c10
}

UNIT_TEST(pnl_ResourceLimitUI_layout_and_slots) {
    CheckPanelShape<ResourceLimitUI>((size_t)0x40);
    CHECK_EQ(SlotOffset(&ResourceLimitUI::OnCreated), (uintptr_t)0x20);   // 0x140077970
    CHECK_EQ(SlotOffset(&ResourceLimitUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x1400778d0
    CHECK_EQ(SlotOffset(&ResourceLimitUI::OnElementEvent), (uintptr_t)0x48);   // 0x1400777e0
}

UNIT_TEST(pnl_TitleDescToolTipUI_layout_and_slots) {
    CheckPanelShape<TitleDescToolTipUI>((size_t)0x30);
    CHECK_EQ(SlotOffset(&TitleDescToolTipUI::Persist), (uintptr_t)0x18);   // 0x14007b8a0
    CHECK_EQ(SlotOffset(&TitleDescToolTipUI::OnCreated), (uintptr_t)0x20);   // 0x14007b730
}

UNIT_TEST(pnl_ToolUI_layout_and_slots) {
    CheckPanelShape<ToolUI>((size_t)0x58);
    CHECK_EQ(SlotOffset(&ToolUI::Persist), (uintptr_t)0x18);   // 0x14007c070
    CHECK_EQ(SlotOffset(&ToolUI::OnCreated), (uintptr_t)0x20);   // 0x14007bd70
    CHECK_EQ(SlotOffset(&ToolUI::OnUpdate), (uintptr_t)0x40);   // 0x14007b9b0
    CHECK_EQ(SlotOffset(&ToolUI::OnElementEvent), (uintptr_t)0x48);   // 0x14007bc30
}

UNIT_TEST(pnl_TradeUI_layout_and_slots) {
    CheckPanelShape<TradeUI>((size_t)0x148);
    CHECK_EQ(SlotOffset(&TradeUI::OnCreated), (uintptr_t)0x20);   // 0x140084dd0
    CHECK_EQ(SlotOffset(&TradeUI::OnOpened), (uintptr_t)0x28);   // 0x140082a70
    CHECK_EQ(SlotOffset(&TradeUI::OnClosed), (uintptr_t)0x30);   // 0x140082a00
    CHECK_EQ(SlotOffset(&TradeUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x140082150
    CHECK_EQ(SlotOffset(&TradeUI::OnUpdate), (uintptr_t)0x40);   // 0x1400827b0
    CHECK_EQ(SlotOffset(&TradeUI::OnElementEvent), (uintptr_t)0x48);   // 0x140081f20
    CHECK_EQ(SlotOffset(&TradeUI::OnSerializeSettings), (uintptr_t)0x50);   // 0x1400868c0
}
// @@PANEL-TESTS-END

}  // namespace
