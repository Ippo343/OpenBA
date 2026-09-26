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
#include "Dialog/CitizenUI.h"
#include "Dialog/ConsumeProduceUI.h"
#include "Dialog/ControlUI.h"
#include "Dialog/CropUI.h"
#include "Dialog/DebugUI.h"
#include "Dialog/DestroyUI.h"
#include "Dialog/EntityUI.h"
#include "Dialog/EventLogUI.h"
#include "Dialog/HelpUI.h"
#include "Dialog/HotKeyToolTipUI.h"
#include "Dialog/ImmigrationUI.h"
#include "Dialog/MapUI.h"
#include "Dialog/ModBrowseUI.h"
#include "Dialog/NoiseUI.h"
#include "Dialog/OrchardUI.h"
#include "Dialog/PageToggleUI.h"
#include "Dialog/PastureUI.h"
#include "Dialog/ProductionUI.h"
#include "Dialog/ProfessionUI.h"
#include "Dialog/RadiusUI.h"
#include "Dialog/ResidenceUI.h"
#include "Dialog/ResourceLimitUI.h"
#include "Dialog/SpriteCaptureUI.h"
#include "Dialog/StatusBarUI.h"
#include "Dialog/StatusIconUI.h"
#include "Dialog/StorageCapacityUI.h"
#include "Dialog/StorageUI.h"
#include "Dialog/TitleDescToolTipUI.h"
#include "Dialog/ToolUI.h"
#include "Dialog/TownHallUI.h"
#include "Dialog/TradeUI.h"
#include "Dialog/UpgradeUI.h"
#include "Dialog/WorkPlaceUI.h"
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

// SelectResourceUI: abstract (slots 11-13 _purecall in 0x14015d330), 14 slots.
UNIT_TEST(pnl_selectresourceui_abstract_base) {
    using S = SelectResourceUI;
    CHECK(std::is_abstract<S>::value);
    CHECK(!std::has_virtual_destructor<S>::value);
    CHECK((std::is_base_of<UI::DialogController, S>::value));
    CHECK_EQ(sizeof(S), (size_t)0x38);
    CHECK_EQ(offsetof(S, internalBasePtr), (size_t)0x30);
    CHECK_EQ(SlotOffset(&S::DeletingDtor),   (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&S::OnCreated),      (uintptr_t)0x20);   // 0x140078be0
    CHECK_EQ(SlotOffset(&S::OnUpdateLayout), (uintptr_t)0x38);   // 0x140078cb0
    CHECK_EQ(SlotOffset(&S::OnUpdate),       (uintptr_t)0x40);   // 0x140078d30
    CHECK_EQ(SlotOffset(&S::OnElementEvent), (uintptr_t)0x48);   // 0x140078d80
    CHECK_EQ(SlotOffset(&S::SetResource),    (uintptr_t)0x58);   // [11]
    CHECK_EQ(SlotOffset(&S::GetResource),    (uintptr_t)0x60);   // [12]
    CHECK_EQ(SlotOffset(&S::GetCategory),    (uintptr_t)0x68);   // [13] last
    // CropUI inherits slot 9 (retail's CropUI table holds 0x140078d80).
    CHECK_EQ(SlotOffset(&CropUI::OnElementEvent), (uintptr_t)0x48);
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

UNIT_TEST(pnl_CitizenUI_layout_and_slots) {
    CheckPanelShape<CitizenUI>((size_t)0xe8);
    CHECK_EQ(SlotOffset(&CitizenUI::OnCreated), (uintptr_t)0x20);   // 0x1400651a0
    CHECK_EQ(SlotOffset(&CitizenUI::OnClosed), (uintptr_t)0x30);   // 0x140065130
    CHECK_EQ(SlotOffset(&CitizenUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x140064ae0
    CHECK_EQ(SlotOffset(&CitizenUI::OnUpdate), (uintptr_t)0x40);   // standalone deviation
    CHECK_EQ(SlotOffset(&CitizenUI::OnElementEvent), (uintptr_t)0x48);   // 0x1400650f0
}

UNIT_TEST(pnl_ConsumeProduceUI_layout_and_slots) {
    CheckPanelShape<ConsumeProduceUI>((size_t)0x40);
    CHECK_EQ(SlotOffset(&ConsumeProduceUI::OnCreated), (uintptr_t)0x20);   // 0x14005f3d0
    CHECK_EQ(SlotOffset(&ConsumeProduceUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x14005f1f0
    CHECK_EQ(SlotOffset(&ConsumeProduceUI::OnElementEvent), (uintptr_t)0x48);   // 0x14005f330
}

UNIT_TEST(pnl_ControlUI_layout_and_slots) {
    CheckPanelShape<ControlUI>((size_t)0x40);
    CHECK_EQ(SlotOffset(&ControlUI::OnCreated), (uintptr_t)0x20);   // 0x14005f9b0
    CHECK_EQ(SlotOffset(&ControlUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x14005f870
    CHECK_EQ(SlotOffset(&ControlUI::OnElementEvent), (uintptr_t)0x48);   // 0x14005f7f0
}

UNIT_TEST(pnl_CropUI_layout_and_slots) {
    CheckPanelShape<CropUI>((size_t)0x60);
    CHECK_EQ(SlotOffset(&CropUI::OnCreated), (uintptr_t)0x20);   // 0x1400b2b50
    CHECK_EQ(SlotOffset(&CropUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x1400b2a50
    CHECK_EQ(SlotOffset(&CropUI::OnUpdate), (uintptr_t)0x40);   // standalone deviation
    CHECK_EQ(SlotOffset(&CropUI::SetResource), (uintptr_t)0x58);   // 0x1400b2920
    CHECK_EQ(SlotOffset(&CropUI::GetResource), (uintptr_t)0x60);   // 0x1400b29d0
    CHECK_EQ(SlotOffset(&CropUI::GetCategory), (uintptr_t)0x68);   // 0x1400b29c0
}

UNIT_TEST(pnl_DebugUI_layout_and_slots) {
    CheckPanelShape<DebugUI>((size_t)0x130);
    CHECK_EQ(SlotOffset(&DebugUI::OnCreated), (uintptr_t)0x20);   // 0x1400619e0
    CHECK_EQ(SlotOffset(&DebugUI::OnClosed), (uintptr_t)0x30);   // 0x1400645f0
    CHECK_EQ(SlotOffset(&DebugUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x140061280
    CHECK_EQ(SlotOffset(&DebugUI::OnUpdate), (uintptr_t)0x40);   // 0x1400610c0
    CHECK_EQ(SlotOffset(&DebugUI::OnElementEvent), (uintptr_t)0x48);   // 0x140060bc0
}

UNIT_TEST(pnl_DestroyUI_layout_and_slots) {
    CheckPanelShape<DestroyUI>((size_t)0x78);
    CHECK_EQ(SlotOffset(&DestroyUI::OnCreated), (uintptr_t)0x20);   // 0x140066150
    CHECK_EQ(SlotOffset(&DestroyUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x140065b00
    CHECK_EQ(SlotOffset(&DestroyUI::OnUpdate), (uintptr_t)0x40);   // standalone deviation
    CHECK_EQ(SlotOffset(&DestroyUI::OnElementEvent), (uintptr_t)0x48);   // 0x140066080
}

UNIT_TEST(pnl_EntityUI_layout_and_slots) {
    CheckPanelShape<EntityUI>((size_t)0x48);
    CHECK_EQ(SlotOffset(&EntityUI::Persist), (uintptr_t)0x18);   // 0x1400672a0
    CHECK_EQ(SlotOffset(&EntityUI::OnCreated), (uintptr_t)0x20);   // 0x1400670d0
    CHECK_EQ(SlotOffset(&EntityUI::OnClosed), (uintptr_t)0x30);   // 0x140067090
    CHECK_EQ(SlotOffset(&EntityUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x140066d80
    CHECK_EQ(SlotOffset(&EntityUI::OnUpdate), (uintptr_t)0x40);   // 0x140066ad0
    CHECK_EQ(SlotOffset(&EntityUI::OnElementEvent), (uintptr_t)0x48);   // 0x140067050
}

UNIT_TEST(pnl_EventLogUI_layout_and_slots) {
    CheckPanelShape<EventLogUI>((size_t)0x80);
    CHECK_EQ(SlotOffset(&EventLogUI::OnOpened), (uintptr_t)0x28);   // 0x140067640
    CHECK_EQ(SlotOffset(&EventLogUI::OnClosed), (uintptr_t)0x30);   // 0x140067630
    CHECK_EQ(SlotOffset(&EventLogUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x140067440
    CHECK_EQ(SlotOffset(&EventLogUI::OnElementEvent), (uintptr_t)0x48);   // 0x1400675d0
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

UNIT_TEST(pnl_ModBrowseUI_layout_and_slots) {
    CheckPanelShape<ModBrowseUI>((size_t)0x30);
    CHECK_EQ(SlotOffset(&ModBrowseUI::Persist), (uintptr_t)0x18);   // 0x14006d390
}

UNIT_TEST(pnl_NoiseUI_layout_and_slots) {
    CheckPanelShape<NoiseUI>((size_t)0x70);
    CHECK_EQ(SlotOffset(&NoiseUI::Persist), (uintptr_t)0x18);   // 0x140071bb0
    CHECK_EQ(SlotOffset(&NoiseUI::OnCreated), (uintptr_t)0x20);   // 0x1400714c0
    CHECK_EQ(SlotOffset(&NoiseUI::OnElementEvent), (uintptr_t)0x48);   // 0x1400714a0
}

UNIT_TEST(pnl_OrchardUI_layout_and_slots) {
    CheckPanelShape<OrchardUI>((size_t)0x48);
    CHECK_EQ(SlotOffset(&OrchardUI::OnCreated), (uintptr_t)0x20);   // 0x1400755b0
    CHECK_EQ(SlotOffset(&OrchardUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x1400754b0
    CHECK_EQ(SlotOffset(&OrchardUI::OnUpdate), (uintptr_t)0x40);   // standalone deviation
    CHECK_EQ(SlotOffset(&OrchardUI::OnElementEvent), (uintptr_t)0x48);   // standalone deviation
    CHECK_EQ(SlotOffset(&OrchardUI::SetResource), (uintptr_t)0x58);   // 0x140075380
    CHECK_EQ(SlotOffset(&OrchardUI::GetResource), (uintptr_t)0x60);   // 0x140075430
    CHECK_EQ(SlotOffset(&OrchardUI::GetCategory), (uintptr_t)0x68);   // 0x140075420
}

UNIT_TEST(pnl_PageToggleUI_layout_and_slots) {
    CheckPanelShape<PageToggleUI>((size_t)0x48);
    CHECK_EQ(SlotOffset(&PageToggleUI::Persist), (uintptr_t)0x18);   // 0x1400759a0
    CHECK_EQ(SlotOffset(&PageToggleUI::OnCreated), (uintptr_t)0x20);   // 0x140075850
    CHECK_EQ(SlotOffset(&PageToggleUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x140075780
    CHECK_EQ(SlotOffset(&PageToggleUI::OnElementEvent), (uintptr_t)0x48);   // 0x140075710
}

UNIT_TEST(pnl_PastureUI_layout_and_slots) {
    CheckPanelShape<PastureUI>((size_t)0x40);
    CHECK_EQ(SlotOffset(&PastureUI::OnCreated), (uintptr_t)0x20);   // 0x1400b5090
    CHECK_EQ(SlotOffset(&PastureUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x1400b4e50
    CHECK_EQ(SlotOffset(&PastureUI::OnUpdate), (uintptr_t)0x40);   // standalone deviation
    CHECK_EQ(SlotOffset(&PastureUI::OnElementEvent), (uintptr_t)0x48);   // 0x1400b4e20
    CHECK_EQ(SlotOffset(&PastureUI::SetResource), (uintptr_t)0x58);   // 0x1400b4c40
    CHECK_EQ(SlotOffset(&PastureUI::GetResource), (uintptr_t)0x60);   // 0x1400b4d00
    CHECK_EQ(SlotOffset(&PastureUI::GetCategory), (uintptr_t)0x68);   // 0x1400b4cf0
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

UNIT_TEST(pnl_RadiusUI_layout_and_slots) {
    CheckPanelShape<RadiusUI>((size_t)0x38);
    CHECK_EQ(SlotOffset(&RadiusUI::OnOpened), (uintptr_t)0x28);   // 0x140077650
    CHECK_EQ(SlotOffset(&RadiusUI::OnClosed), (uintptr_t)0x30);   // 0x140077600
}

UNIT_TEST(pnl_ResidenceUI_layout_and_slots) {
    CheckPanelShape<ResidenceUI>((size_t)0x40);
    CHECK_EQ(SlotOffset(&ResidenceUI::Persist), (uintptr_t)0x18);   // 0x1400b5db0
    CHECK_EQ(SlotOffset(&ResidenceUI::OnCreated), (uintptr_t)0x20);   // 0x1400b5850
    CHECK_EQ(SlotOffset(&ResidenceUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x1400b51f0
    CHECK_EQ(SlotOffset(&ResidenceUI::OnUpdate), (uintptr_t)0x40);   // standalone deviation
    CHECK_EQ(SlotOffset(&ResidenceUI::OnElementEvent), (uintptr_t)0x48);   // 0x1400b5820
}

UNIT_TEST(pnl_ResourceLimitUI_layout_and_slots) {
    CheckPanelShape<ResourceLimitUI>((size_t)0x40);
    CHECK_EQ(SlotOffset(&ResourceLimitUI::OnCreated), (uintptr_t)0x20);   // 0x140077970
    CHECK_EQ(SlotOffset(&ResourceLimitUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x1400778d0
    CHECK_EQ(SlotOffset(&ResourceLimitUI::OnElementEvent), (uintptr_t)0x48);   // 0x1400777e0
}

UNIT_TEST(pnl_SpriteCaptureUI_layout_and_slots) {
    CheckPanelShape<SpriteCaptureUI>((size_t)0x70);
    CHECK_EQ(SlotOffset(&SpriteCaptureUI::Persist), (uintptr_t)0x18);   // 0x140079850
    CHECK_EQ(SlotOffset(&SpriteCaptureUI::OnCreated), (uintptr_t)0x20);   // 0x140079640
    CHECK_EQ(SlotOffset(&SpriteCaptureUI::OnUpdate), (uintptr_t)0x40);   // 0x1400793a0
    CHECK_EQ(SlotOffset(&SpriteCaptureUI::OnElementEvent), (uintptr_t)0x48);   // 0x140079620
}

UNIT_TEST(pnl_StatusBarUI_layout_and_slots) {
    CheckPanelShape<StatusBarUI>((size_t)0x1d0);
    CHECK_EQ(SlotOffset(&StatusBarUI::OnCreated), (uintptr_t)0x20);   // standalone deviation
    CHECK_EQ(SlotOffset(&StatusBarUI::OnOpened), (uintptr_t)0x28);   // 0x14007a3d0
    CHECK_EQ(SlotOffset(&StatusBarUI::OnClosed), (uintptr_t)0x30);   // 0x14007a3c0
    CHECK_EQ(SlotOffset(&StatusBarUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x1400798d0
    CHECK_EQ(SlotOffset(&StatusBarUI::OnUpdate), (uintptr_t)0x40);   // standalone deviation
    CHECK_EQ(SlotOffset(&StatusBarUI::OnElementEvent), (uintptr_t)0x48);   // 0x14007a3a0
}

UNIT_TEST(pnl_StatusIconUI_layout_and_slots) {
    CheckPanelShape<StatusIconUI>((size_t)0x40);
    CHECK_EQ(SlotOffset(&StatusIconUI::OnCreated), (uintptr_t)0x20);   // 0x14007b270
    CHECK_EQ(SlotOffset(&StatusIconUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x14007b0a0
    CHECK_EQ(SlotOffset(&StatusIconUI::OnUpdate), (uintptr_t)0x40);   // standalone deviation
}

UNIT_TEST(pnl_StorageCapacityUI_layout_and_slots) {
    CheckPanelShape<StorageCapacityUI>((size_t)0x38);
    CHECK_EQ(SlotOffset(&StorageCapacityUI::Persist), (uintptr_t)0x18);   // 0x14007b6b0
    CHECK_EQ(SlotOffset(&StorageCapacityUI::OnCreated), (uintptr_t)0x20);   // 0x14007b600
    CHECK_EQ(SlotOffset(&StorageCapacityUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x14007b4d0
    CHECK_EQ(SlotOffset(&StorageCapacityUI::OnUpdate), (uintptr_t)0x40);   // standalone deviation
}

UNIT_TEST(pnl_StorageUI_layout_and_slots) {
    CheckPanelShape<StorageUI>((size_t)0x40);
    CHECK_EQ(SlotOffset(&StorageUI::Persist), (uintptr_t)0x18);   // 0x1400b8ca0
    CHECK_EQ(SlotOffset(&StorageUI::OnCreated), (uintptr_t)0x20);   // 0x1400b8820
    CHECK_EQ(SlotOffset(&StorageUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x1400b8450
    CHECK_EQ(SlotOffset(&StorageUI::OnUpdate), (uintptr_t)0x40);   // standalone deviation
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

UNIT_TEST(pnl_TownHallUI_layout_and_slots) {
    CheckPanelShape<TownHallUI>((size_t)0x1e8);
    CHECK_EQ(SlotOffset(&TownHallUI::OnCreated), (uintptr_t)0x20);   // 0x140081000
    CHECK_EQ(SlotOffset(&TownHallUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x14007d550
    CHECK_EQ(SlotOffset(&TownHallUI::OnUpdate), (uintptr_t)0x40);   // 0x14007e550
    CHECK_EQ(SlotOffset(&TownHallUI::OnElementEvent), (uintptr_t)0x48);   // 0x14007c180
    CHECK_EQ(SlotOffset(&TownHallUI::OnSerializeSettings), (uintptr_t)0x50);   // 0x140080db0
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

UNIT_TEST(pnl_UpgradeUI_layout_and_slots) {
    CheckPanelShape<UpgradeUI>((size_t)0x38);
    CHECK_EQ(SlotOffset(&UpgradeUI::OnCreated), (uintptr_t)0x20);   // 0x140086d90
    CHECK_EQ(SlotOffset(&UpgradeUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x140086d80
    CHECK_EQ(SlotOffset(&UpgradeUI::OnUpdate), (uintptr_t)0x40);   // standalone deviation
    CHECK_EQ(SlotOffset(&UpgradeUI::OnElementEvent), (uintptr_t)0x48);   // 0x140086c90
}

UNIT_TEST(pnl_WorkPlaceUI_layout_and_slots) {
    CheckPanelShape<WorkPlaceUI>((size_t)0x68);
    CHECK_EQ(SlotOffset(&WorkPlaceUI::OnCreated), (uintptr_t)0x20);   // 0x140087940
    CHECK_EQ(SlotOffset(&WorkPlaceUI::OnUpdateLayout), (uintptr_t)0x38);   // 0x140087080
    CHECK_EQ(SlotOffset(&WorkPlaceUI::OnUpdate), (uintptr_t)0x40);   // standalone deviation
    CHECK_EQ(SlotOffset(&WorkPlaceUI::OnElementEvent), (uintptr_t)0x48);   // 0x1400877e0
}
// @@PANEL-TESTS-END

}  // namespace
