// ============================================================================
// tests/unit/vis_abi_test.cpp — ENDGAME B.3.8 block VIS (B38_RULES.md B8-14)
//
// The ABI claims block VIS makes for its 20 presentation `Component` leaves,
// the two embedded members (StateMachineComponent's root node, StatusIcon's
// billboard Geometry) and the five UI/Graphics strays, measured on the SHIPPED
// HEADERS (code/Game/Component/<Class>.h):
//
// PART A — LAYOUT. sizeof / alignof against each retail create function's
//   MemAlloc (B38_RULES §1.3), and the member offsets the ported bodies read raw.
// PART B — SHAPE. Every class is concrete, `final`, single inheritance from its
//   proved parent, polymorphic, NO virtual destructor.
// PART C — SLOT ORDER, read off pointers to VIRTUAL members (Itanium PMF =
//   1 + vtable byte offset, adjustment 0): each override lands on the retail
//   index of B38_SLOTS.md §6, and slot 0 is `DeletingDtor`.
// PART D — the RETSLOT member (the two descriptions' slot 8 CreateInstance) is
//   declared in retail order `T* (T* ret, const Dialog*) const` and sits on
//   slot 8.
//
// NOT PROVEN HERE: the bodies (they stand on the address registry and cannot
// link into a host test), so no object is constructed and the raw retail-order
// call through slot 8 is left to the real run; `Vtable() == vptr` is
// tests/unit/exe_class_abi_test.cpp PART C's proof of DEFINE_EXE_VTABLE.
// ============================================================================
#include "unit.h"

#include "Component/AutoPickupComponent.h"
#include "Component/DecalComponent.h"
#include "Component/FloorComponent.h"
#include "Component/HighlightComponent.h"
#include "Component/InteractComponent.h"
#include "Component/MapComponent.h"
#include "Component/MineComponent.h"
#include "Component/ModelComponent.h"
#include "Component/PickingComponent.h"
#include "Component/QuarryComponent.h"
#include "Component/RawMaterialComponent.h"
#include "Component/StateMachineComponent.h"
#include "Component/StatusIconComponent.h"
#include "Component/StorageComponent.h"
#include "Component/StorageDisplayComponent.h"
#include "Component/TerraformComponent.h"
#include "Component/TrackerComponent.h"
#include "Component/UIComponent.h"
#include "Component/WorkComponent.h"
#include "Component/ZoneComponent.h"
#include "Component/AnimAudioEvent.h"
#include "Component/CaptureBoxElement.h"
#include "Component/CaptureBoxDescription.h"
#include "Component/FrustumOutlineElement.h"
#include "Component/FrustumOutlineDescription.h"

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

constexpr uintptr_t S(int slot) { return static_cast<uintptr_t>(slot) * 8; }

// ===========================================================================
// PART A — LAYOUT
// ===========================================================================
UNIT_TEST(vis_component_sizes_match_retail) {
    CHECK_EQ(sizeof(AutoPickupComponent),     (size_t)0x40);    // create 0x1400033c0
    CHECK_EQ(sizeof(DecalComponent),          (size_t)0x38);    // create 0x1400337c0
    CHECK_EQ(sizeof(FloorComponent),          (size_t)0x30);    // create 0x140034600
    CHECK_EQ(sizeof(HighlightComponent),      (size_t)0x128);   // create 0x140006e40
    CHECK_EQ(sizeof(InteractComponent),       (size_t)0x38);    // create 0x1400351a0
    CHECK_EQ(sizeof(MapComponent),            (size_t)0x38);    // create 0x140035700
    CHECK_EQ(sizeof(MineComponent),           (size_t)0x28);    // create 0x140008dd0
    CHECK_EQ(sizeof(ModelComponent),          (size_t)0x170);   // create 0x1400358d0
    CHECK_EQ(sizeof(PickingComponent),        (size_t)0x38);    // create 0x140036da0
    CHECK_EQ(sizeof(QuarryComponent),         (size_t)0x28);    // create 0x14000b3d0
    CHECK_EQ(sizeof(RawMaterialComponent),    (size_t)0x40);    // create 0x140037270
    CHECK_EQ(sizeof(StateMachineComponent),   (size_t)0x58);    // create 0x1400382d0
    CHECK_EQ(sizeof(StatusIconComponent),     (size_t)0x108);   // create 0x1400388c0
    CHECK_EQ(sizeof(StorageComponent),        (size_t)0x40);    // create 0x140038b50
    CHECK_EQ(sizeof(StorageDisplayComponent), (size_t)0x38);    // create 0x14000c770
    CHECK_EQ(sizeof(TerraformComponent),      (size_t)0x48);    // create 0x14000cc90
    CHECK_EQ(sizeof(TrackerComponent),        (size_t)0x50);    // create 0x14000d7a0
    CHECK_EQ(sizeof(UIComponent),             (size_t)0x38);    // create 0x140039550
    CHECK_EQ(sizeof(WorkComponent),           (size_t)0x38);    // create 0x140039e70
    CHECK_EQ(sizeof(ZoneComponent),           (size_t)0x28);    // create 0x14003a6a0
}

UNIT_TEST(vis_stray_sizes_match_retail) {
    CHECK_EQ(sizeof(AnimAudioEvent),            (size_t)0x30);  // create 0x140016f00
    CHECK_EQ(sizeof(CaptureBoxElement),         (size_t)0xe0);  // create 0x140013eb0
    CHECK_EQ(sizeof(CaptureBoxDescription),     (size_t)0x78);  // create 0x140013d90
    CHECK_EQ(sizeof(FrustumOutlineElement),     (size_t)0xd8);  // create 0x140012400
    CHECK_EQ(sizeof(FrustumOutlineDescription), (size_t)0x80);  // create 0x1400122c0
    CHECK_EQ(sizeof(StatusIconGeometry),        (size_t)0xd8);  // component +0x28..+0x100
    CHECK_EQ(sizeof(StateMachineComponent::RootController), (size_t)0x20);
}

UNIT_TEST(vis_member_offsets) {
    CHECK_EQ(offsetof(AutoPickupComponent, isEnabled),      (size_t)0x28);
    CHECK_EQ(offsetof(AutoPickupComponent, field30),        (size_t)0x30);
    CHECK_EQ(offsetof(DecalComponent, decalId),             (size_t)0x2c);
    CHECK_EQ(offsetof(DecalComponent, decalScale),          (size_t)0x30);
    CHECK_EQ(offsetof(FloorComponent, floorId),             (size_t)0x2c);
    CHECK_EQ(offsetof(HighlightComponent, edgeMeshInstance),(size_t)0x28);
    CHECK_EQ(offsetof(HighlightComponent, highlightId),     (size_t)0x120);
    CHECK_EQ(offsetof(InteractComponent, field28),          (size_t)0x28);
    CHECK_EQ(offsetof(MapComponent, tileX0),                (size_t)0x28);
    CHECK_EQ(offsetof(MapComponent, extraField),            (size_t)0x30);
    CHECK_EQ(offsetof(ModelComponent, meshInstance),        (size_t)0x28);
    CHECK_EQ(offsetof(ModelComponent, transform),           (size_t)0x148);
    CHECK_EQ(offsetof(ModelComponent, modelId),             (size_t)0x16c);
    CHECK_EQ(offsetof(PickingComponent, pickingId),         (size_t)0x28);
    CHECK_EQ(offsetof(RawMaterialComponent, field38),       (size_t)0x38);
    CHECK_EQ(offsetof(StorageComponent, slotCount),         (size_t)0x30);
    CHECK_EQ(offsetof(StorageDisplayComponent, displayData1),(size_t)0x30);
    CHECK_EQ(offsetof(TerraformComponent, tileRecord),      (size_t)0x38);
    CHECK_EQ(offsetof(TrackerComponent, trackerId),         (size_t)0x48);
    CHECK_EQ(offsetof(UIComponent, uiData0),                (size_t)0x28);
    CHECK_EQ(offsetof(WorkComponent, workFlags),            (size_t)0x2c);
    // D6: the two embedded members at +0x28.
    CHECK_EQ(offsetof(StateMachineComponent, _controller),  (size_t)0x28);
    CHECK_EQ(offsetof(StateMachineComponent, controllerSelf),(size_t)0x48);
    CHECK_EQ(offsetof(StateMachineComponent, stateFlags),   (size_t)0x50);
    CHECK_EQ(offsetof(ControllerComponent::Controller, _owner), (size_t)0x18);  // root node +0x40
    CHECK_EQ(offsetof(StatusIconComponent, _geometry),      (size_t)0x28);
    CHECK_EQ(offsetof(StatusIconComponent, iconId),         (size_t)0x100);
    CHECK_EQ(offsetof(StatusIconGeometry, iconSize),        (size_t)0xc0);   // component +0xe8
    CHECK_EQ(offsetof(StatusIconGeometry, activeMask),      (size_t)0xc8);   // component +0xf0
    CHECK_EQ(offsetof(StatusIconGeometry, constantSize),    (size_t)0xcc);   // component +0xf4
    CHECK_EQ(offsetof(StatusIconGeometry, spriteSheet),     (size_t)0xd0);   // component +0xf8
    // The strays.
    CHECK_EQ(offsetof(AnimAudioEvent, _sound),              (size_t)0x20);
    CHECK_EQ(offsetof(AnimAudioEvent, _chance),             (size_t)0x28);
    CHECK_EQ(offsetof(CaptureBoxElement, _lineSprite),      (size_t)0xc0);
    CHECK_EQ(offsetof(CaptureBoxElement, _y1),              (size_t)0xdc);
    CHECK_EQ(offsetof(FrustumOutlineElement, _lineSprite),  (size_t)0xc0);
    CHECK_EQ(offsetof(FrustumOutlineElement, _width),       (size_t)0xd0);
    CHECK_EQ(offsetof(CaptureBoxDescription, _spriteSheet), (size_t)0x68);
    CHECK_EQ(offsetof(CaptureBoxDescription, _spriteName),  (size_t)0x70);
    CHECK_EQ(offsetof(FrustumOutlineDescription, _width),   (size_t)0x78);
    CHECK_EQ(offsetof(FrustumOutlineDescription, _height),  (size_t)0x7c);
}

// ===========================================================================
// PART B — SHAPE
// ===========================================================================
template <typename T, typename Base>
void CheckShape() {
    CHECK(std::is_polymorphic<T>::value);
    CHECK(!std::is_abstract<T>::value);
    CHECK(std::is_final<T>::value);
    CHECK(!std::has_virtual_destructor<T>::value);
    CHECK((std::is_base_of<Base, T>::value));
    CHECK_EQ(alignof(T), (size_t)8);
    CHECK_EQ(SlotOffset(&T::DeletingDtor), S(0));
}

template <typename T>
void CheckComponentShape() {
    CheckShape<T, Component>();
    CHECK((std::is_base_of<Object::Internal, T>::value));
    CHECK(!(std::is_base_of<ControllerComponent, T>::value));   // D6: members, not bases
    CHECK_EQ(SlotOffset(&T::Persist), S(3));
}

UNIT_TEST(vis_class_shape_rules) {
    CheckComponentShape<AutoPickupComponent>();
    CheckComponentShape<DecalComponent>();
    CheckComponentShape<FloorComponent>();
    CheckComponentShape<HighlightComponent>();
    CheckComponentShape<InteractComponent>();
    CheckComponentShape<MapComponent>();
    CheckComponentShape<MineComponent>();
    CheckComponentShape<ModelComponent>();
    CheckComponentShape<PickingComponent>();
    CheckComponentShape<QuarryComponent>();
    CheckComponentShape<RawMaterialComponent>();
    CheckComponentShape<StateMachineComponent>();
    CheckComponentShape<StatusIconComponent>();
    CheckComponentShape<StorageComponent>();
    CheckComponentShape<StorageDisplayComponent>();
    CheckComponentShape<TerraformComponent>();
    CheckComponentShape<TrackerComponent>();
    CheckComponentShape<UIComponent>();
    CheckComponentShape<WorkComponent>();
    CheckComponentShape<ZoneComponent>();

    CheckShape<AnimAudioEvent, Graphics::AnimationEvent>();
    CheckShape<CaptureBoxElement, UI::Element>();
    CheckShape<FrustumOutlineElement, UI::Element>();
    CheckShape<CaptureBoxDescription, UI::ElementDescription>();
    CheckShape<FrustumOutlineDescription, UI::ElementDescription>();
    CheckShape<StatusIconGeometry, Graphics::Geometry>();

    // The root node: a final implementation of the 9-slot interface, no dtor slot.
    using Root = StateMachineComponent::RootController;
    CHECK(std::is_final<Root>::value);
    CHECK(!std::is_abstract<Root>::value);
    CHECK(!std::has_virtual_destructor<Root>::value);
    CHECK((std::is_base_of<ControllerComponent::Controller, Root>::value));
}

// ===========================================================================
// PART C — SLOT ORDER (the leaf's own slots, B38_SLOTS.md §6)
// ===========================================================================
UNIT_TEST(vis_component_own_slots_land_on_retail_indices) {
    CHECK_EQ(SlotOffset(&AutoPickupComponent::Startup),       S(4));    // 0x140044990
    CHECK_EQ(SlotOffset(&AutoPickupComponent::CommandMask),   S(13));   // 0x140044970
    CHECK_EQ(SlotOffset(&AutoPickupComponent::Command),       S(14));   // 0x140044930
    CHECK_EQ(SlotOffset(&DecalComponent::Startup),            S(4));    // 0x14011b7d0
    CHECK_EQ(SlotOffset(&DecalComponent::OnAdded),            S(6));    // 0x14011b560
    CHECK_EQ(SlotOffset(&DecalComponent::OnRemove),           S(7));    // 0x14011b510
    CHECK_EQ(SlotOffset(&DecalComponent::Tick),               S(8));    // tree divergence
    CHECK_EQ(SlotOffset(&DecalComponent::Slot11),             S(11));   // 0x14011b810
    CHECK_EQ(SlotOffset(&DecalComponent::OnEvent),            S(12));   // 0x14011b760
    CHECK_EQ(SlotOffset(&FloorComponent::OnAdded),            S(6));    // 0x14011f340
    CHECK_EQ(SlotOffset(&FloorComponent::OnRemove),           S(7));    // 0x14011f2a0
    CHECK_EQ(SlotOffset(&FloorComponent::Slot11),             S(11));   // 0x14011f540
    CHECK_EQ(SlotOffset(&FloorComponent::OnEvent),            S(12));   // 0x14011f520
    CHECK_EQ(SlotOffset(&HighlightComponent::OnRemove),       S(7));    // tree divergence
    CHECK_EQ(SlotOffset(&HighlightComponent::Tick),           S(8));    // 0x14004c850
    CHECK_EQ(SlotOffset(&HighlightComponent::OnEvent),        S(12));   // 0x14004c870
    CHECK_EQ(SlotOffset(&InteractComponent::OnAdded),         S(6));    // 0x140123110
    CHECK_EQ(SlotOffset(&MapComponent::OnAdded),              S(6));    // 0x140124370
    CHECK_EQ(SlotOffset(&MapComponent::OnRemove),             S(7));    // 0x1401242f0
    CHECK_EQ(SlotOffset(&MapComponent::OnEvent),              S(12));   // 0x1401243c0
    CHECK_EQ(SlotOffset(&MineComponent::OnAdded),             S(6));    // 0x14004f250
    CHECK_EQ(SlotOffset(&MineComponent::OnRemove),            S(7));    // 0x14004f240
    CHECK_EQ(SlotOffset(&ModelComponent::Startup),            S(4));    // 0x140124ac0
    CHECK_EQ(SlotOffset(&ModelComponent::OnAdded),            S(6));    // 0x140124a30
    CHECK_EQ(SlotOffset(&ModelComponent::OnRemove),           S(7));    // 0x1401249d0
    CHECK_EQ(SlotOffset(&ModelComponent::Tick),               S(8));    // 0x140124960
    CHECK_EQ(SlotOffset(&PickingComponent::Startup),          S(4));    // 0x140128c30
    CHECK_EQ(SlotOffset(&PickingComponent::OnAdded),          S(6));    // 0x140128b90
    CHECK_EQ(SlotOffset(&PickingComponent::OnRemove),         S(7));    // 0x140128b50
    CHECK_EQ(SlotOffset(&PickingComponent::Slot11),           S(11));   // 0x140128ba0
    CHECK_EQ(SlotOffset(&QuarryComponent::OnAdded),           S(6));    // 0x140055220
    CHECK_EQ(SlotOffset(&QuarryComponent::OnRemove),          S(7));    // 0x140055210
    CHECK_EQ(SlotOffset(&QuarryComponent::OnEvent),           S(12));   // 0x1400550f0
    CHECK_EQ(SlotOffset(&QuarryComponent::OnMessage),         S(15));   // 0x140054b80
    CHECK_EQ(SlotOffset(&RawMaterialComponent::Startup),      S(4));    // 0x1401292c0
    CHECK_EQ(SlotOffset(&RawMaterialComponent::OnAdded),      S(6));    // 0x1401291d0
    CHECK_EQ(SlotOffset(&RawMaterialComponent::Update),       S(9));    // 0x140129130
    CHECK_EQ(SlotOffset(&StateMachineComponent::Startup),     S(4));    // 0x14012b6a0
    CHECK_EQ(SlotOffset(&StateMachineComponent::OnAdded),     S(6));    // 0x14012b660
    CHECK_EQ(SlotOffset(&StateMachineComponent::Tick),        S(8));    // 0x14012b640
    CHECK_EQ(SlotOffset(&StateMachineComponent::Update),      S(9));    // 0x14012b620
    CHECK_EQ(SlotOffset(&StateMachineComponent::Slot11),      S(11));   // 0x14012b7d0
    CHECK_EQ(SlotOffset(&StatusIconComponent::Startup),       S(4));    // 0x14012bed0
    CHECK_EQ(SlotOffset(&StatusIconComponent::OnRemove),      S(7));    // 0x14012beb0 (tree body)
    CHECK_EQ(SlotOffset(&StatusIconComponent::Tick),          S(8));    // 0x14012bbd0
    CHECK_EQ(SlotOffset(&StatusIconComponent::Slot11),        S(11));   // 0x14012bf40
    CHECK_EQ(SlotOffset(&StorageComponent::Startup),          S(4));    // 0x14012c720
    CHECK_EQ(SlotOffset(&StorageComponent::OnAdded),          S(6));    // 0x14012c640
    CHECK_EQ(SlotOffset(&StorageDisplayComponent::Startup),   S(4));    // 0x140056930
    CHECK_EQ(SlotOffset(&StorageDisplayComponent::OnAdded),   S(6));    // 0x140056730
    CHECK_EQ(SlotOffset(&StorageDisplayComponent::OnRemove),  S(7));    // 0x1400566b0
    CHECK_EQ(SlotOffset(&StorageDisplayComponent::Update),    S(9));    // 0x1400566a0
    CHECK_EQ(SlotOffset(&StorageDisplayComponent::Slot11),    S(11));   // 0x140056940
    CHECK_EQ(SlotOffset(&TerraformComponent::OnAdded),        S(6));    // 0x1400574f0
    CHECK_EQ(SlotOffset(&TerraformComponent::OnRemove),       S(7));    // 0x140057350
    CHECK_EQ(SlotOffset(&TerraformComponent::OnEvent),        S(12));   // 0x140057310
    CHECK_EQ(SlotOffset(&UIComponent::Startup),               S(4));    // 0x14012dca0
    CHECK_EQ(SlotOffset(&WorkComponent::OnAdded),             S(6));    // 0x14012e6f0
    CHECK_EQ(SlotOffset(&WorkComponent::OnRemove),            S(7));    // 0x14012e6c0
    CHECK_EQ(SlotOffset(&WorkComponent::CommandMask),         S(13));   // 0x14012e6b0
    CHECK_EQ(SlotOffset(&WorkComponent::Command),             S(14));   // 0x14012e6a0
    CHECK_EQ(SlotOffset(&ZoneComponent::OnAdded),             S(6));    // 0x1401301f0
    CHECK_EQ(SlotOffset(&ZoneComponent::OnRemove),            S(7));    // 0x140130080
    // Slots 1/2 answer the class's own registration (DECLARE_EXE_TYPE).
    CHECK_EQ(SlotOffset(&MineComponent::TypeName),            S(1));
    CHECK_EQ(SlotOffset(&MineComponent::Type),                S(2));
}

UNIT_TEST(vis_member_tables_land_on_retail_indices) {
    // StateMachineComponent's root node (member table 0x1401644d0).
    using Root = StateMachineComponent::RootController;
    CHECK_EQ(SlotOffset(&Root::FindState),      S(0));
    CHECK_EQ(SlotOffset(&Root::FindTransition), S(1));
    CHECK_EQ(SlotOffset(&Root::PostEvent),      S(2));
    CHECK_EQ(SlotOffset(&Root::Enter),          S(3));
    CHECK_EQ(SlotOffset(&Root::Update),         S(4));
    CHECK_EQ(SlotOffset(&Root::OnTransition),   S(5));
    CHECK_EQ(SlotOffset(&Root::Tick),           S(6));
    CHECK_EQ(SlotOffset(&Root::TickAlt),        S(7));
    CHECK_EQ(SlotOffset(&Root::HasStates),      S(8));
    // StatusIcon's billboard (member table 0x14015f0c0): 0-3 its own, 4-8 Geometry's.
    CHECK_EQ(SlotOffset(&StatusIconGeometry::DeletingDtor), S(0));   // 0x14001ebb0
    CHECK_EQ(SlotOffset(&StatusIconGeometry::Update),       S(1));   // 0x1400aa430
    CHECK_EQ(SlotOffset(&StatusIconGeometry::Draw),         S(2));   // 0x1400a9c10
    CHECK_EQ(SlotOffset(&StatusIconGeometry::AddDrawItems), S(3));   // 0x1400a9b90
    CHECK_EQ(SlotOffset(&Graphics::Geometry::UpdateAttached), S(4));
    CHECK_EQ(SlotOffset(&Graphics::Geometry::Clean),          S(8));
}

UNIT_TEST(vis_stray_own_slots_land_on_retail_indices) {
    CHECK_EQ(SlotOffset(&AnimAudioEvent::GetFileVersion),   S(3));   // 0x140087f30
    CHECK_EQ(SlotOffset(&AnimAudioEvent::Serialize),        S(5));   // 0x140087ed0
    CHECK_EQ(SlotOffset(&AnimAudioEvent::Reconstruct),      S(6));   // 0x140016fb0
    CHECK_EQ(SlotOffset(&AnimAudioEvent::Destruct),         S(7));   // 0x140001640
    CHECK_EQ(SlotOffset(&AnimAudioEvent::Construct),        S(8));   // 0x140087e40 (the trigger)
    CHECK_EQ(SlotOffset(&AnimAudioEvent::ConstructInPlace), S(9));   // 0x140016f60
    CHECK_EQ(SlotOffset(&CaptureBoxElement::ComputeSize),   S(4));   // 0x140078fb0
    CHECK_EQ(SlotOffset(&CaptureBoxElement::ComputeLayout), S(5));   // 0x140078ee0
    CHECK_EQ(SlotOffset(&CaptureBoxElement::Draw),          S(6));   // 0x140078da0
    CHECK_EQ(SlotOffset(&FrustumOutlineElement::ComputeSize),   S(4));   // 0x14006c290
    CHECK_EQ(SlotOffset(&FrustumOutlineElement::ComputeLayout), S(5));   // 0x14006c1c0
    CHECK_EQ(SlotOffset(&FrustumOutlineElement::Draw),          S(6));   // 0x14006be90
    CHECK_EQ(SlotOffset(&FrustumOutlineElement::Enable),        S(8));   // inherited
    CHECK_EQ(SlotOffset(&CaptureBoxDescription::GetFileVersion), S(3));  // 0x140079210
    CHECK_EQ(SlotOffset(&CaptureBoxDescription::Serialize),      S(5));  // 0x1400791a0
    CHECK_EQ(SlotOffset(&CaptureBoxDescription::Reconstruct),    S(6));  // 0x140013e40
    CHECK_EQ(SlotOffset(&CaptureBoxDescription::Destruct),       S(7));  // 0x140001640
    CHECK_EQ(SlotOffset(&CaptureBoxDescription::Construct),      S(9));  // 0x140013df0
    CHECK_EQ(SlotOffset(&FrustumOutlineDescription::GetFileVersion), S(3));  // 0x14006c500
    CHECK_EQ(SlotOffset(&FrustumOutlineDescription::Serialize),      S(5));  // 0x14006c460
    CHECK_EQ(SlotOffset(&FrustumOutlineDescription::Construct),      S(9));  // 0x140012330
}

// ===========================================================================
// PART D — the RETSLOT member: slot 8 CreateInstance, retail order
// ===========================================================================
UNIT_TEST(vis_description_slot8_is_the_retslot_create_instance) {
    using Ret = Object::InternalPtr<UI::Element>;
    using CBSig = Ret* (CaptureBoxDescription::*)(Ret*, const UI::Dialog*) const;
    using FOSig = Ret* (FrustumOutlineDescription::*)(Ret*, const UI::Dialog*) const;
    CBSig cb = &CaptureBoxDescription::CreateInstance;
    FOSig fo = &FrustumOutlineDescription::CreateInstance;
    CHECK_EQ(SlotOffset(cb), S(8));   // 0x1400790a0
    CHECK_EQ(SlotOffset(fo), S(8));   // 0x14006c340
}

}  // namespace
