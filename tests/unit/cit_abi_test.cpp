// ============================================================================
// tests/unit/cit_abi_test.cpp — ENDGAME B.3.8 block CIT (B38_RULES.md B8-14)
//
// The ABI claims block CIT makes for its 19 `Component` leaves, measured on the
// SHIPPED HEADERS (code/Game/Component/<Class>.h):
//
// PART A — LAYOUT. sizeof / alignof against each retail create function's
//   MemAlloc (B38_RULES §1.3), and the member offsets the ported bodies read raw.
// PART B — SHAPE. Every class is concrete, `final`, single inheritance from
//   `Component` (so from Object::Internal), polymorphic, a virtual destructor (B3X W1).
// PART C — SLOT ORDER, read off pointers to VIRTUAL members (Itanium PMF =
//   1 + vtable byte offset, adjustment 0): each override lands on the retail
//   index of B38_SLOTS.md §6, and slot 0 is the destructor (B3X W1: D1 + D0).
//
// NOT PROVEN HERE: the bodies (they stand on the address registry and cannot
// link into a host test), and `Vtable() == vptr` (tests/unit/exe_class_abi_test.cpp
// PART C proves DEFINE_EXE_VTABLE on this toolchain; the real run and the resave
// gate verify the classes). There is no retslot or hidden-pointer member in
// this block (every slot returns void, a pointer or an int in RAX).
// ============================================================================
#include "unit.h"
#include "slot_check.h"

#include "Component/AmbientEmitterComponent.h"
#include "Component/AmbientEnvironmentComponent.h"
#include "Component/AnimalComponent.h"
#include "Component/CharacterComponent.h"
#include "Component/CitizenAIComponent.h"
#include "Component/CitizenComponent.h"
#include "Component/DiseaseComponent.h"
#include "Component/FireComponent.h"
#include "Component/FruitComponent.h"
#include "Component/GrowthComponent.h"
#include "Component/HerdComponent.h"
#include "Component/LivestockComponent.h"
#include "Component/NaturalResourceComponent.h"
#include "Component/ParticleComponent.h"
#include "Component/PopulationComponent.h"
#include "Component/SpawnComponent.h"
#include "Component/TombstoneComponent.h"
#include "Component/TornadoComponent.h"
#include "Component/WildAnimalComponent.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace {


// ===========================================================================
// PART A — LAYOUT
// ===========================================================================
UNIT_TEST(cit_component_sizes_match_retail) {
    CHECK_EQ(sizeof(AmbientEmitterComponent),     (size_t)0x48);    // create 0x1400014b0
    CHECK_EQ(sizeof(AmbientEnvironmentComponent), (size_t)0x48);    // create 0x140001ee0
    CHECK_EQ(sizeof(AnimalComponent),             (size_t)0x58);    // create 0x14002f020
    CHECK_EQ(sizeof(CharacterComponent),          (size_t)0x80);    // create 0x14002f960
    CHECK_EQ(sizeof(CitizenAIComponent),          (size_t)0xd0);    // create 0x14002fb20
    CHECK_EQ(sizeof(CitizenComponent),            (size_t)0x290);   // create 0x140030110
    CHECK_EQ(sizeof(DiseaseComponent),            (size_t)0x58);    // create 0x140004db0
    CHECK_EQ(sizeof(FireComponent),               (size_t)0x48);    // create 0x1400055e0
    CHECK_EQ(sizeof(FruitComponent),              (size_t)0x38);    // create 0x140005920
    CHECK_EQ(sizeof(GrowthComponent),             (size_t)0x50);    // create 0x140034b10
    CHECK_EQ(sizeof(HerdComponent),               (size_t)0x60);    // create 0x140034cd0
    CHECK_EQ(sizeof(LivestockComponent),          (size_t)0x38);    // create 0x140035480
    CHECK_EQ(sizeof(NaturalResourceComponent),    (size_t)0x30);    // create 0x140036550
    CHECK_EQ(sizeof(ParticleComponent),           (size_t)0x48);    // create 0x14000a360
    CHECK_EQ(sizeof(PopulationComponent),         (size_t)0x70);    // create 0x14000af50
    CHECK_EQ(sizeof(SpawnComponent),              (size_t)0x38);    // create 0x14000c170
    CHECK_EQ(sizeof(TombstoneComponent),          (size_t)0x30);    // create 0x14000d280
    CHECK_EQ(sizeof(TornadoComponent),            (size_t)0x68);    // create 0x14000d500
    CHECK_EQ(sizeof(WildAnimalComponent),         (size_t)0x30);    // create 0x140039c20
}

UNIT_TEST(cit_component_member_offsets) {
    // The first own field of every leaf sits right after Component's 0x28.
    CHECK_EQ(offsetof(AmbientEmitterComponent, position),      (size_t)0x28);
    CHECK_EQ(offsetof(AmbientEmitterComponent, slots),         (size_t)0x38);
    CHECK_EQ(offsetof(AmbientEnvironmentComponent, position.z), (size_t)0x40);
    CHECK_EQ(offsetof(AnimalComponent, age),                   (size_t)0x44);
    CHECK_EQ(offsetof(AnimalComponent, grazeTimer),            (size_t)0x50);
    CHECK_EQ(offsetof(CharacterComponent, characterIndex),     (size_t)0x40);   // char index
    CHECK_EQ(offsetof(CharacterComponent, currentAction),      (size_t)0x60);   // action index
    CHECK_EQ(offsetof(CharacterComponent, characterDescription), (size_t)0x78);
    CHECK_EQ(offsetof(CitizenAIComponent, timers),             (size_t)0x40);
    CHECK_EQ(offsetof(CitizenAIComponent, dataC8),             (size_t)0xc8);
    CHECK_EQ(offsetof(CitizenComponent, meshInstance),         (size_t)0xb8);   // Graphics::MeshInstance
    CHECK_EQ(offsetof(CitizenComponent, toolId),               (size_t)0x1d8);
    CHECK_EQ(offsetof(CitizenComponent, description),          (size_t)0x288);
    CHECK_EQ(offsetof(DiseaseComponent, curedIdentity),        (size_t)0x50);
    CHECK_EQ(offsetof(FireComponent, igniteAccum),             (size_t)0x40);
    CHECK_EQ(offsetof(FruitComponent, fruitFlag),              (size_t)0x34);
    CHECK_EQ(offsetof(GrowthComponent, growthScale),           (size_t)0x4c);
    CHECK_EQ(offsetof(HerdComponent, tile),                  (size_t)0x4c);
    CHECK_EQ(offsetof(LivestockComponent, data30),             (size_t)0x30);
    CHECK_EQ(offsetof(NaturalResourceComponent, resourceFlags),(size_t)0x2c);
    CHECK_EQ(offsetof(ParticleComponent, field40),             (size_t)0x40);
    CHECK_EQ(offsetof(PopulationComponent, field68),           (size_t)0x68);
    CHECK_EQ(offsetof(SpawnComponent, field30),                (size_t)0x30);
    CHECK_EQ(offsetof(TombstoneComponent, field28),            (size_t)0x28);
    CHECK_EQ(offsetof(TornadoComponent, pathIndex),            (size_t)0x50);
    CHECK_EQ(offsetof(TornadoComponent, posZ),                 (size_t)0x60);
    CHECK_EQ(offsetof(WildAnimalComponent, data28),            (size_t)0x28);
}

// ===========================================================================
// PART B — SHAPE
// ===========================================================================
template <typename T>
void CheckLeafShape() {
    CHECK(std::is_polymorphic<T>::value);
    CHECK(!std::is_abstract<T>::value);
    CHECK(std::is_final<T>::value);
    CHECK(std::has_virtual_destructor<T>::value);
    CHECK((std::is_base_of<Component, T>::value));
    CHECK((std::is_base_of<Object::Internal, T>::value));
    CHECK_EQ(alignof(T), (size_t)8);
    CHECK_DTOR_SLOT(T);
    CHECK_SLOT(T, Persist, 3);
}

UNIT_TEST(cit_component_class_shape_rules) {
    CheckLeafShape<AmbientEmitterComponent>();
    CheckLeafShape<AmbientEnvironmentComponent>();
    CheckLeafShape<AnimalComponent>();
    CheckLeafShape<CharacterComponent>();
    CheckLeafShape<CitizenAIComponent>();
    CheckLeafShape<CitizenComponent>();
    CheckLeafShape<DiseaseComponent>();
    CheckLeafShape<FireComponent>();
    CheckLeafShape<FruitComponent>();
    CheckLeafShape<GrowthComponent>();
    CheckLeafShape<HerdComponent>();
    CheckLeafShape<LivestockComponent>();
    CheckLeafShape<NaturalResourceComponent>();
    CheckLeafShape<ParticleComponent>();
    CheckLeafShape<PopulationComponent>();
    CheckLeafShape<SpawnComponent>();
    CheckLeafShape<TombstoneComponent>();
    CheckLeafShape<TornadoComponent>();
    CheckLeafShape<WildAnimalComponent>();
}

// ===========================================================================
// PART C — SLOT ORDER (the leaf's own slots, B38_SLOTS.md §6)
// ===========================================================================
UNIT_TEST(cit_component_own_slots_land_on_retail_indices) {
    CHECK_SLOT(AmbientEmitterComponent, OnAdded,    6);    // 0x140042ba0
    CHECK_SLOT(AmbientEmitterComponent, OnRemove,   7);    // 0x140042b90
    CHECK_SLOT(AmbientEnvironmentComponent, Update, 9);    // 0x140043690
    CHECK_SLOT(AnimalComponent, Startup,            4);    // 0x140106d50
    CHECK_SLOT(AnimalComponent, OnAdded,            6);    // 0x140106c20
    CHECK_SLOT(AnimalComponent, Update,             9);    // 0x140106420
    CHECK_SLOT(AnimalComponent, Slot11,             11);   // 0x140106dd0
    CHECK_SLOT(CharacterComponent, Startup,         4);    // 0x140109a30
    CHECK_SLOT(CharacterComponent, OnAdded,         6);    // 0x1401099c0
    CHECK_SLOT(CharacterComponent, OnRemove,        7);    // 0x140109980
    CHECK_SLOT(CharacterComponent, Tick,            8);    // 0x1401094c0
    CHECK_SLOT(CharacterComponent, Update,          9);    // 0x140109480
    CHECK_SLOT(CitizenAIComponent, Startup,         4);    // 0x14010f920
    CHECK_SLOT(CitizenAIComponent, OnAdded,         6);    // 0x14010f880
    CHECK_SLOT(CitizenAIComponent, Update,          9);    // 0x14010f3e0
    CHECK_SLOT(CitizenAIComponent, OnMessage,       15);   // 0x14010f340
    CHECK_SLOT(CitizenComponent, Startup,           4);    // 0x140114050
    CHECK_SLOT(CitizenComponent, OnAdded,           6);    // 0x140114000
    CHECK_SLOT(CitizenComponent, OnRemove,          7);    // 0x140113ca0
    CHECK_SLOT(CitizenComponent, Update,            9);    // 0x140112e70
    CHECK_SLOT(CitizenComponent, Slot11,            11);   // 0x1401140f0
    CHECK_SLOT(CitizenComponent, OnMessage,         15);   // 0x140112260
    CHECK_SLOT(DiseaseComponent, Startup,           4);    // 0x140048c50
    CHECK_SLOT(DiseaseComponent, OnAdded,           6);    // 0x140048c00
    CHECK_SLOT(DiseaseComponent, OnRemove,          7);    // 0x140048bb0
    CHECK_SLOT(DiseaseComponent, Update,            9);    // 0x140048b90
    CHECK_SLOT(FireComponent, Update,               9);    // 0x140048fb0
    CHECK_SLOT(FireComponent, OnMessage,            15);   // 0x140049090
    CHECK_SLOT(FruitComponent, Startup,             4);    // 0x14004a030
    CHECK_SLOT(FruitComponent, Update,              9);    // 0x140049de0
    CHECK_SLOT(GrowthComponent, Startup,            4);    // 0x1401206f0
    CHECK_SLOT(GrowthComponent, OnAdded,            6);    // 0x140120690
    CHECK_SLOT(GrowthComponent, OnRemove,           7);    // 0x140120560
    CHECK_SLOT(GrowthComponent, Update,             9);    // 0x140120050
    CHECK_SLOT(GrowthComponent, PostUpdate,         10);   // 0x14011fea0
    CHECK_SLOT(GrowthComponent, Slot11,             11);   // 0x1401206a0
    CHECK_SLOT(HerdComponent, OnAdded,              6);    // 0x140121f40
    CHECK_SLOT(HerdComponent, OnRemove,             7);    // 0x140121e20
    CHECK_SLOT(HerdComponent, Update,               9);    // 0x140121bf0
    CHECK_SLOT(LivestockComponent, OnAdded,         6);    // 0x140123c70
    CHECK_SLOT(LivestockComponent, OnRemove,        7);    // 0x140123c00
    CHECK_SLOT(LivestockComponent, Update,          9);    // 0x140123b50
    CHECK_SLOT(NaturalResourceComponent, Tick,      8);    // 0x140125c40
    CHECK_SLOT(ParticleComponent, Startup,          4);    // 0x140053230
    CHECK_SLOT(ParticleComponent, OnAdded,          6);    // 0x140052f70
    CHECK_SLOT(ParticleComponent, OnRemove,         7);    // 0x140052e80
    CHECK_SLOT(ParticleComponent, Tick,             8);    // 0x140052b30
    CHECK_SLOT(ParticleComponent, Slot11,           11);   // 0x1400531b0
    CHECK_SLOT(ParticleComponent, OnEvent,          12);   // 0x140053090
    CHECK_SLOT(PopulationComponent, Startup,        4);    // 0x140054220
    CHECK_SLOT(PopulationComponent, OnAdded,        6);    // 0x1400541c0
    CHECK_SLOT(PopulationComponent, OnRemove,       7);    // 0x140054180
    CHECK_SLOT(PopulationComponent, Update,         9);    // 0x140054070
    CHECK_SLOT(PopulationComponent, Slot11,         11);   // 0x140054210
    CHECK_SLOT(SpawnComponent, Startup,             4);    // 0x140055e90
    CHECK_SLOT(SpawnComponent, Update,              9);    // 0x140055ad0
    CHECK_SLOT(TombstoneComponent, Startup,         4);    // 0x140057860
    CHECK_SLOT(TombstoneComponent, Update,          9);    // 0x1400577d0
    CHECK_SLOT(TornadoComponent, Startup,           4);    // 0x1400585a0
    CHECK_SLOT(TornadoComponent, Tick,              8);    // 0x140057cd0
    CHECK_SLOT(WildAnimalComponent, OnRemove,       7);    // 0x14012e2b0

    // Slots retail leaves to Component stay Component's (the leaf does not
    // re-declare them): the PMF through the leaf names Component's slot.
    CHECK_SLOT(TombstoneComponent, CommandMask, 13);
    CHECK_SLOT(WildAnimalComponent, OnMessage,  15);
}

}  // namespace
