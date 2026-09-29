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
//   proved parent, polymorphic, a virtual destructor (B3X W1).
// PART C — SLOT ORDER, read off pointers to VIRTUAL members (Itanium PMF =
//   1 + vtable byte offset, adjustment 0): each override lands on the retail
//   index of B38_SLOTS.md §6, and slot 0 is the destructor (B3X W1: D1 + D0).
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
#include "slot_check.h"

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
    CHECK_EQ(offsetof(InteractComponent, useLocations),          (size_t)0x28);
    CHECK_EQ(offsetof(MapComponent, tileX0),                (size_t)0x28);
    CHECK_EQ(offsetof(MapComponent, extraField),            (size_t)0x30);
    CHECK_EQ(offsetof(ModelComponent, meshInstance),        (size_t)0x28);
    CHECK_EQ(offsetof(ModelComponent, transform),           (size_t)0x148);
    CHECK_EQ(offsetof(ModelComponent, modelId),             (size_t)0x16c);
    CHECK_EQ(offsetof(PickingComponent, pickingId),         (size_t)0x28);
    CHECK_EQ(offsetof(RawMaterialComponent, age),           (size_t)0x38);
    CHECK_EQ(offsetof(StorageComponent, slotCount),         (size_t)0x30);
    CHECK_EQ(offsetof(StorageDisplayComponent, pileCount),   (size_t)0x30);
    CHECK_EQ(offsetof(TerraformComponent, tileRecord),      (size_t)0x38);
    CHECK_EQ(offsetof(TrackerComponent, trackerId),         (size_t)0x48);
    CHECK_EQ(offsetof(UIComponent, uiData0),                (size_t)0x28);
    CHECK_EQ(offsetof(WorkComponent, workFlags),            (size_t)0x2c);
    // D6: the two embedded members at +0x28.
    CHECK_EQ(offsetof(StateMachineComponent, _controller),  (size_t)0x28);
    CHECK_EQ(offsetof(StateMachineComponent, stateMachine),  (size_t)0x48);
    CHECK_EQ(offsetof(StateMachine, currentInput),           (size_t)0x08);
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
    CHECK(std::has_virtual_destructor<T>::value);
    CHECK((std::is_base_of<Base, T>::value));
    CHECK_EQ(alignof(T), (size_t)8);
    CHECK_DTOR_SLOT(T);
}

template <typename T>
void CheckComponentShape() {
    CheckShape<T, Component>();
    CHECK((std::is_base_of<Object::Internal, T>::value));
    CHECK(!(std::is_base_of<ControllerComponent, T>::value));   // D6: members, not bases
    CHECK_SLOT(T, Persist, 3);
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
    CHECK_SLOT(AutoPickupComponent, Startup,      4);    // 0x140044990
    CHECK_SLOT(AutoPickupComponent, CommandMask,  13);   // 0x140044970
    CHECK_SLOT(AutoPickupComponent, Command,      14);   // 0x140044930
    CHECK_SLOT(DecalComponent, Startup,           4);    // 0x14011b7d0
    CHECK_SLOT(DecalComponent, OnAdded,           6);    // 0x14011b560
    CHECK_SLOT(DecalComponent, OnRemove,          7);    // 0x14011b510
    CHECK_SLOT(DecalComponent, Tick,              8);    // tree divergence
    CHECK_SLOT(DecalComponent, Slot11,            11);   // 0x14011b810
    CHECK_SLOT(DecalComponent, OnEvent,           12);   // 0x14011b760
    CHECK_SLOT(FloorComponent, OnAdded,           6);    // 0x14011f340
    CHECK_SLOT(FloorComponent, OnRemove,          7);    // 0x14011f2a0
    CHECK_SLOT(FloorComponent, Slot11,            11);   // 0x14011f540
    CHECK_SLOT(FloorComponent, OnEvent,           12);   // 0x14011f520
    CHECK_SLOT(HighlightComponent, OnRemove,      7);    // tree divergence
    CHECK_SLOT(HighlightComponent, Tick,          8);    // 0x14004c850
    CHECK_SLOT(HighlightComponent, OnEvent,       12);   // 0x14004c870
    CHECK_SLOT(InteractComponent, OnAdded,        6);    // 0x140123110
    CHECK_SLOT(MapComponent, OnAdded,             6);    // 0x140124370
    CHECK_SLOT(MapComponent, OnRemove,            7);    // 0x1401242f0
    CHECK_SLOT(MapComponent, OnEvent,             12);   // 0x1401243c0
    CHECK_SLOT(MineComponent, OnAdded,            6);    // 0x14004f250
    CHECK_SLOT(MineComponent, OnRemove,           7);    // 0x14004f240
    CHECK_SLOT(ModelComponent, Startup,           4);    // 0x140124ac0
    CHECK_SLOT(ModelComponent, OnAdded,           6);    // 0x140124a30
    CHECK_SLOT(ModelComponent, OnRemove,          7);    // 0x1401249d0
    CHECK_SLOT(ModelComponent, Tick,              8);    // 0x140124960
    CHECK_SLOT(PickingComponent, Startup,         4);    // 0x140128c30
    CHECK_SLOT(PickingComponent, OnAdded,         6);    // 0x140128b90
    CHECK_SLOT(PickingComponent, OnRemove,        7);    // 0x140128b50
    CHECK_SLOT(PickingComponent, Slot11,          11);   // 0x140128ba0
    CHECK_SLOT(QuarryComponent, OnAdded,          6);    // 0x140055220
    CHECK_SLOT(QuarryComponent, OnRemove,         7);    // 0x140055210
    CHECK_SLOT(QuarryComponent, OnEvent,          12);   // 0x1400550f0
    CHECK_SLOT(QuarryComponent, OnMessage,        15);   // 0x140054b80
    CHECK_SLOT(RawMaterialComponent, Startup,     4);    // 0x1401292c0
    CHECK_SLOT(RawMaterialComponent, OnAdded,     6);    // 0x1401291d0
    CHECK_SLOT(RawMaterialComponent, Update,      9);    // 0x140129130
    CHECK_SLOT(StateMachineComponent, Startup,    4);    // 0x14012b6a0
    CHECK_SLOT(StateMachineComponent, OnAdded,    6);    // 0x14012b660
    CHECK_SLOT(StateMachineComponent, Tick,       8);    // 0x14012b640
    CHECK_SLOT(StateMachineComponent, Update,     9);    // 0x14012b620
    CHECK_SLOT(StateMachineComponent, Slot11,     11);   // 0x14012b7d0
    CHECK_SLOT(StatusIconComponent, Startup,      4);    // 0x14012bed0
    CHECK_SLOT(StatusIconComponent, OnRemove,     7);    // 0x14012beb0 (tree body)
    CHECK_SLOT(StatusIconComponent, Tick,         8);    // 0x14012bbd0
    CHECK_SLOT(StatusIconComponent, Slot11,       11);   // 0x14012bf40
    CHECK_SLOT(StorageComponent, Startup,         4);    // 0x14012c720
    CHECK_SLOT(StorageComponent, OnAdded,         6);    // 0x14012c640
    CHECK_SLOT(StorageDisplayComponent, Startup,  4);    // 0x140056930
    CHECK_SLOT(StorageDisplayComponent, OnAdded,  6);    // 0x140056730
    CHECK_SLOT(StorageDisplayComponent, OnRemove, 7);    // 0x1400566b0
    CHECK_SLOT(StorageDisplayComponent, Update,   9);    // 0x1400566a0
    CHECK_SLOT(StorageDisplayComponent, Slot11,   11);   // 0x140056940
    CHECK_SLOT(TerraformComponent, OnAdded,       6);    // 0x1400574f0
    CHECK_SLOT(TerraformComponent, OnRemove,      7);    // 0x140057350
    CHECK_SLOT(TerraformComponent, OnEvent,       12);   // 0x140057310
    CHECK_SLOT(UIComponent, Startup,              4);    // 0x14012dca0
    CHECK_SLOT(WorkComponent, OnAdded,            6);    // 0x14012e6f0
    CHECK_SLOT(WorkComponent, OnRemove,           7);    // 0x14012e6c0
    CHECK_SLOT(WorkComponent, CommandMask,        13);   // 0x14012e6b0
    CHECK_SLOT(WorkComponent, Command,            14);   // 0x14012e6a0
    CHECK_SLOT(ZoneComponent, OnAdded,            6);    // 0x1401301f0
    CHECK_SLOT(ZoneComponent, OnRemove,           7);    // 0x140130080
    // Slots 1/2 answer the class's own registration (DECLARE_EXE_TYPE).
    CHECK_SLOT(MineComponent, TypeName, 1);
    CHECK_SLOT(MineComponent, Type,     2);
}

UNIT_TEST(vis_member_tables_land_on_retail_indices) {
    // StateMachineComponent's root node (member table 0x1401644d0).
    using Root = StateMachineComponent::RootController;
    CHECK_SLOT(Root, FindState,      0);
    CHECK_SLOT(Root, FindTransition, 1);
    CHECK_SLOT(Root, PostEvent,      2);
    CHECK_SLOT(Root, Enter,          3);
    CHECK_SLOT(Root, Update,         4);
    CHECK_SLOT(Root, OnTransition,   5);
    CHECK_SLOT(Root, Tick,           6);
    CHECK_SLOT(Root, TickAlt,        7);
    CHECK_SLOT(Root, HasStates,      8);
    // StatusIcon's billboard (member table 0x14015f0c0): 0-3 its own, 4-8 Geometry's.
    CHECK_DTOR_SLOT(StatusIconGeometry);   // 0x14001ebb0
    CHECK_SLOT(StatusIconGeometry, Update,         1);   // 0x1400aa430
    CHECK_SLOT(StatusIconGeometry, Draw,           2);   // 0x1400a9c10
    CHECK_SLOT(StatusIconGeometry, AddDrawItems,   3);   // 0x1400a9b90
    CHECK_SLOT(Graphics::Geometry, UpdateAttached, 4);
    CHECK_SLOT(Graphics::Geometry, Clean,          8);
}

UNIT_TEST(vis_stray_own_slots_land_on_retail_indices) {
    CHECK_SLOT(AnimAudioEvent, GetFileVersion,            3);   // 0x140087f30
    CHECK_SLOT(AnimAudioEvent, Serialize,                 5);   // 0x140087ed0
    CHECK_SLOT(AnimAudioEvent, Reconstruct,               6);   // 0x140016fb0
    CHECK_SLOT(AnimAudioEvent, Destruct,                  7);   // 0x140001640
    CHECK_SLOT(AnimAudioEvent, Construct,                 8);   // 0x140087e40 (the trigger)
    CHECK_SLOT(AnimAudioEvent, ConstructInPlace,          9);   // 0x140016f60
    CHECK_SLOT(CaptureBoxElement, ComputeSize,            4);   // 0x140078fb0
    CHECK_SLOT(CaptureBoxElement, ComputeLayout,          5);   // 0x140078ee0
    CHECK_SLOT(CaptureBoxElement, Draw,                   6);   // 0x140078da0
    CHECK_SLOT(FrustumOutlineElement, ComputeSize,        4);   // 0x14006c290
    CHECK_SLOT(FrustumOutlineElement, ComputeLayout,      5);   // 0x14006c1c0
    CHECK_SLOT(FrustumOutlineElement, Draw,               6);   // 0x14006be90
    CHECK_SLOT(FrustumOutlineElement, Enable,             8);   // inherited
    CHECK_SLOT(CaptureBoxDescription, GetFileVersion,     3);  // 0x140079210
    CHECK_SLOT(CaptureBoxDescription, Serialize,          5);  // 0x1400791a0
    CHECK_SLOT(CaptureBoxDescription, Reconstruct,        6);  // 0x140013e40
    CHECK_SLOT(CaptureBoxDescription, Destruct,           7);  // 0x140001640
    CHECK_SLOT(CaptureBoxDescription, Construct,          9);  // 0x140013df0
    CHECK_SLOT(FrustumOutlineDescription, GetFileVersion, 3);  // 0x14006c500
    CHECK_SLOT(FrustumOutlineDescription, Serialize,      5);  // 0x14006c460
    CHECK_SLOT(FrustumOutlineDescription, Construct,      9);  // 0x140012330
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
    CHECK_PMF_SLOT(cb, 8);   // 0x1400790a0
    CHECK_PMF_SLOT(fo, 8);   // 0x14006c340
}

}  // namespace
