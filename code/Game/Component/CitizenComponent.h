#pragma once
// ============================================================================
// code/Game/Component/CitizenComponent.h — ENDGAME B.3.8 block CIT
//
// `CitizenComponent` (a citizen's full state: health, happiness, inventory,
// housing, work, the age/clothing mesh) as a real C++ class with a
// compiler-generated vtable (B38_RULES.md D1-D4). The engine/types POD is a
// forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "CitizenComponent", parent "Component", ti 0x1401c7158
//   create 0x140030110: MemAlloc(0x290, 8) + ctor 0x140114770
//   ctor   0x140114770: Component ctor, stamp 0x140162580, scalars (+0x30 = -1,
//                       +0xa8 = -1, +0x1d8 = -1 (qword), +0x1e0 = 4, the rest 0),
//                       Graphics::MeshInstance::MeshInstance() at +0xb8,
//                       flags |= 0xcb
//   dtor   0x1401146f0: restamp, ~MeshInstance(+0xb8) (import 0x1401514a8),
//                       MemFree(+0x98) + zero +0x98/+0xa0, System::String::~String
//                       at +0x28 (import 0x140152af0), stamp 0x140162920,
//                       ~Internal; scalar 0x140030270 = dtor + MemFree on &1
//   table  0x140162580, 16 slots:
//     [0] 0x140030270 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x140114160 Persist                [4] 0x140114050 Startup
//     [6] 0x140114000 OnAdded (town list)    [7] 0x140113ca0 OnRemove
//     [9] 0x140112e70 Update                 [11] 0x1401140f0 Slot11 (post-load)
//     [15] 0x140112260 OnMessage (NeedsUpdate; EDX msg, R8 pointer, R9D int)
//     every other slot = Component's
//
// THE +0xB8 MeshInstance: retail constructs and destroys a real
// Graphics::MeshInstance there. The tree has always kept it as zero-filled raw
// bytes (the inert factory memset) that the ported bodies drive by offset; this
// class keeps that (PORT GAP, ctor and dtor — B38 D1/D2), a behaviour change for
// its own titled commit.
//
// FIELDS (ENDGAME B.4 W0-H1): every field from retail's ctor 0x140114770, dtor
// 0x1401146f0 and serializer 0x140114160 (its import slots: 0x140152ac8 int,
// 0x140152a60 float, 0x140152a50 uint, 0x140152a58 bool, 0x140152aa8 Time,
// 0x140152aa0 InternalBasePtr + an IsDerivedFrom check against the cell named
// on each field, 0x140151238 String). The serializer passes no names; names come
// from the profile build's assertions (`_parent`, `_prevSibling`,
// `_nextSibling`) and from what retail does with the field (Update 0x140112e70,
// the happiness update 0x140112ae0, PostLifeEvent, the two assign bodies).
// A field with neither keeps a typed guess name `field_0xNN`.
//
// H5 (the rule sheet): +0x28 (System::String), +0x98 (Collection::Array) and
// +0xB8 (Graphics::MeshInstance) have destructors in retail that the tree's
// defaulted dtor does not run, so they are raw storage of the right size here,
// with the real type named on the line. Declaring the real type is the
// `behaviour:` commit of B38 D2. System::Time fields are `int64_t` ticks: the
// runtime's System::Time (runtime/types/system/Value.h) cannot be included into
// the exe tree (its global `struct Time` collides with engine/types/common.h).
// InternalBasePtr / ExternalPtr members are declared as the bare pointer they
// hold (`T*`, retail's layout: one qword; the tree's precedent is
// ComponentEntity::_description).
//
// CORRECTED (B.4 W0-H1, retail): +0x238 is the HOME, not the workplace: the
// status icon 0 ("imageStatusHome") is raised on `+0x238 == 0`, the happiness
// update scores the home tile through it, and 0x1401163d0 (Ghidra and the tree:
// "AssignWorkplace") writes +0x238 / +0x240 and walks the children ring with
// them (children follow their parents home), finding the ResidenceComponent
// (cell 0x1401c7cf0) through 0x140008090. +0x240 is the household slot in that
// residence (ResidenceComponent_IsHouseholdFull 0x14012a4f0(res, +0x240)).
// +0x248 is the WORKPLACE: 0x140111ee0 (Ghidra and the tree: "AssignHousing")
// replaces it and adds / removes the citizen through 0x140002e10 (the
// WorkPlaceComponent finder) and 0x14012fa20 / 0x14012f7e0; Update raises the
// "no job" icon 3 on `profession+0x4C == 0 && +0x248 == 0`.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class Disease;
class Profession;
class AttendanceComponent;
class ComponentEntity;
class CharacterComponent;
class StatusIconComponent;
class CitizenDescription;

class CitizenComponent final : public Component {
    DECLARE_EXE_TYPE(CitizenComponent)

public:
    CitizenComponent();                                           // 0x140114770

    // PORT GAP (retail 0x1401146f0) — B38 D2: retail runs ~MeshInstance(+0xb8),
    // frees the +0x98 array and runs ~String on +0x28; the tree's
    // Inert_ScalarDtor never did.
    ~CitizenComponent() override = default;                       // [0]  0x140030270
    void Persist(void* stream) override;                          // [3]  0x140114160
    void Startup() override;                                      // [4]  0x140114050
    void OnAdded() override;                                      // [6]  0x140114000
    void OnRemove() override;                                     // [7]  0x140113ca0
    void Update(void* time) override;                             // [9]  0x140112e70
    void Slot11() override;                                       // [11] 0x1401140f0
    void OnMessage(int message, void* data,
                   uint32_t arg0, uint32_t arg1) override;        // [15] 0x140112260

    // PUBLIC while the ported bodies read them (B38 decision 1).
    uint64_t name;                 // +0x28 System::String (H5 raw storage): the citizen's name;
                                   //       serialized first (0x140151238), handed to the event log
                                   //       by PostLifeEvent (EventLog_AddCitizenEvent's name
                                   //       argument), ~String in the dtor
    int32_t  field_0x30;           // +0x30 int, ctor -1; serialized after +0xB0
    int32_t  field_0x34;           // +0x34 int, ctor 0; serialized last
    int32_t  gender;               // +0x38 int; 0 = female: Update's childbearing block runs only
                                   //       on `+0x38 == 0` (0x140113873)
    float    age;                  // +0x3C += dt * desc->agingRate; >= desc->adultAge = adult
    float    maxAge;               // +0x40 `maxAge <= age` -> HandleLifeEvent(2) (old age)
    float    hunger;               // +0x44 += dt * desc->hungerRate; capped at desc->maxHunger
    float    coldness;             // +0x48 += dt * desc->coldRate[clothingLevel]; capped at desc->maxColdness
    float    pregnancyTimer;       // +0x4C > 0 counts down to a birth, < 0 counts up (the wait after one)
    float    health;               // +0x50 birth-risk ratio health / desc->maxHealth
    uint32_t statusFlags;          // +0x54 |= 1 hungry, |= 2 cold (Update)
    int64_t  foodTimer;            // +0x58 System::Time: += dt; each desc->foodInterval -> ConsumeFood
    int64_t  field_0x60;           // +0x60 System::Time: += dt (Update)
    Disease* disease;              // +0x68 ExternalPtr<Disease> (serialize helper 0x140030790)
    int64_t  field_0x70;           // +0x70 System::Time, serialized
    int64_t  field_0x78;           // +0x78 System::Time, serialized
    float    happiness;            // +0x80 the happiness update 0x140112ae0, clamped to [0, desc->maxHappiness]
    int32_t  field_0x84;           // +0x84 int, NOT written by the ctor, not serialized; the happiness
                                   //       update compares it with desc+0xB0
    uint32_t happinessFlags;       // +0x88 the bits the happiness update 0x140112ae0 scores
    uint8_t  _pad8C[4];            // +0x8C
    int64_t  happinessTimer;       // +0x90 System::Time: += dt; each desc+0xA0 -> the happiness update
    void*    consumedTypes;        // +0x98 Collection::Array<ExternalPtr<…>> data (H5 raw: MemFree'd by
                                   //       the retail dtor); serialized by 0x1400049c0
    int32_t  consumedTypeCount;    // +0xA0 Array count; the happiness update compares it with desc+0xA8/+0xAC
    int32_t  consumedTypeCapacity; // +0xA4 Array capacity
    int32_t  field_0xA8;           // +0xA8 int, ctor -1; serialized (version > 0)
    int32_t  field_0xAC;           // +0xAC int, ctor 0; serialized
    uint32_t field_0xB0;           // +0xB0 uint (Encoding slot 0x98), ctor 0
    uint8_t  _padB4[4];            // +0xB4

    uint8_t  meshInstance[0x120];  // +0xB8 Graphics::MeshInstance storage (H5, see above)

    int32_t  toolId;               // +0x1D8 int, ctor -1 (one qword store with +0x1DC); serialized
    int32_t  toolSceneId;          // +0x1DC int, ctor -1; NOT serialized; HideTool 0x1401176e0 guards >= 0
    int32_t  field_0x1E0;          // +0x1E0 int, ctor 4 (one qword store with +0x1E4); serialized (version > 1)
    int32_t  clothingLevel;        // +0x1E4 indexes desc->coldRate[]; reset to 0 when clothingTimer < 0
    int64_t  clothingTimer;        // +0x1E8 System::Time: -= dt; < 0 -> clothingLevel = 0
    Profession* profession;        // +0x1F0 ExternalPtr<Profession> (0x140018020); Update reads its +0x4C
    Profession* pendingProfession; // +0x1F8 ExternalPtr<Profession>; != 0 -> HandleLifeEvent(7, its name)
    int64_t  field_0x200;          // +0x200 System::Time: += dt (Update), not serialized
    AttendanceComponent* school;   // +0x208 InternalPtr (cell 0x1401c3f40); != 0: schoolTime += dt
    int32_t  education;            // +0x210 raised to desc->maxEducation (Update)
    uint8_t  _pad214[4];           // +0x214
    int64_t  schoolTime;           // +0x218 System::Time: += dt while school != 0 (version > 2)
    bool     field_0x220;          // +0x220 bool; 0 at adulthood -> TryEnrolInSchool 0x140111840
    uint8_t  _pad221[7];           // +0x221
    AttendanceComponent* careBuilding; // +0x228 InternalPtr (cell 0x1401c3f40); FindCareBuilding 0x140111590
    AttendanceComponent* field_0x230;  // +0x230 InternalPtr (cell 0x1401c3f40); found through desc+0x28
                                       //        by 0x1401116f0; != 0 scores happiness
    ComponentEntity* home;         // +0x238 InternalPtr<ComponentEntity> (cell 0x1401c5d70): the residence
    int32_t  homeSlot;             // +0x240 int, NOT written by the ctor; the household slot in the residence
    uint8_t  _pad244[4];           // +0x244
    ComponentEntity* workplace;    // +0x248 InternalPtr<ComponentEntity> (cell 0x1401c5d70)
    CitizenComponent* spouse;      // +0x250 InternalPtr (cell 0x1401c7158); set pairwise by the marriage scan
    CitizenComponent* _parent;     // +0x258 InternalPtr (profile assert `child->_parent.IsPtrNull()`)
    CitizenComponent* _firstChild; // +0x260 InternalPtr: head of the children ring (RemoveChild 0x140112040)
    CitizenComponent* _nextSibling; // +0x268 InternalPtr (profile assert)
    CitizenComponent* _prevSibling; // +0x270 InternalPtr (profile assert)
    CharacterComponent* character; // +0x278 InternalPtr (cell 0x1401c7078); Update reads its +0x60
    StatusIconComponent* statusIcon; // +0x280 InternalPtr (cell 0x1401c7e08); SetFlag 0x14012c3f0
    CitizenDescription* description; // +0x288 ExternalPtr<CitizenDescription> (0x140030990)
};

static_assert(sizeof(CitizenComponent) == 0x290, "CitizenComponent is 0x290 (create 0x140030110)");
static_assert(alignof(CitizenComponent) == 8, "CitizenComponent alignment");
static_assert(offsetof(CitizenComponent, name)                 == 0x28,  "");
static_assert(offsetof(CitizenComponent, field_0x30)           == 0x30,  "");
static_assert(offsetof(CitizenComponent, field_0x34)           == 0x34,  "");
static_assert(offsetof(CitizenComponent, gender)               == 0x38,  "");
static_assert(offsetof(CitizenComponent, age)                  == 0x3C,  "");
static_assert(offsetof(CitizenComponent, maxAge)               == 0x40,  "");
static_assert(offsetof(CitizenComponent, hunger)               == 0x44,  "");
static_assert(offsetof(CitizenComponent, coldness)             == 0x48,  "");
static_assert(offsetof(CitizenComponent, pregnancyTimer)       == 0x4C,  "");
static_assert(offsetof(CitizenComponent, health)               == 0x50,  "");
static_assert(offsetof(CitizenComponent, statusFlags)          == 0x54,  "");
static_assert(offsetof(CitizenComponent, foodTimer)            == 0x58,  "");
static_assert(offsetof(CitizenComponent, field_0x60)           == 0x60,  "");
static_assert(offsetof(CitizenComponent, disease)              == 0x68,  "");
static_assert(offsetof(CitizenComponent, field_0x70)           == 0x70,  "");
static_assert(offsetof(CitizenComponent, field_0x78)           == 0x78,  "");
static_assert(offsetof(CitizenComponent, happiness)            == 0x80,  "");
static_assert(offsetof(CitizenComponent, field_0x84)           == 0x84,  "");
static_assert(offsetof(CitizenComponent, happinessFlags)       == 0x88,  "");
static_assert(offsetof(CitizenComponent, _pad8C)               == 0x8C,  "");
static_assert(offsetof(CitizenComponent, happinessTimer)       == 0x90,  "");
static_assert(offsetof(CitizenComponent, consumedTypes)        == 0x98,  "");
static_assert(offsetof(CitizenComponent, consumedTypeCount)    == 0xA0,  "");
static_assert(offsetof(CitizenComponent, consumedTypeCapacity) == 0xA4,  "");
static_assert(offsetof(CitizenComponent, field_0xA8)           == 0xA8,  "");
static_assert(offsetof(CitizenComponent, field_0xAC)           == 0xAC,  "");
static_assert(offsetof(CitizenComponent, field_0xB0)           == 0xB0,  "");
static_assert(offsetof(CitizenComponent, _padB4)               == 0xB4,  "");
static_assert(offsetof(CitizenComponent, meshInstance)         == 0xB8,  "");
static_assert(offsetof(CitizenComponent, toolId)               == 0x1D8, "");
static_assert(offsetof(CitizenComponent, toolSceneId)          == 0x1DC, "");
static_assert(offsetof(CitizenComponent, field_0x1E0)          == 0x1E0, "");
static_assert(offsetof(CitizenComponent, clothingLevel)        == 0x1E4, "");
static_assert(offsetof(CitizenComponent, clothingTimer)        == 0x1E8, "");
static_assert(offsetof(CitizenComponent, profession)           == 0x1F0, "");
static_assert(offsetof(CitizenComponent, pendingProfession)    == 0x1F8, "");
static_assert(offsetof(CitizenComponent, field_0x200)          == 0x200, "");
static_assert(offsetof(CitizenComponent, school)               == 0x208, "");
static_assert(offsetof(CitizenComponent, education)            == 0x210, "");
static_assert(offsetof(CitizenComponent, _pad214)              == 0x214, "");
static_assert(offsetof(CitizenComponent, schoolTime)           == 0x218, "");
static_assert(offsetof(CitizenComponent, field_0x220)          == 0x220, "");
static_assert(offsetof(CitizenComponent, _pad221)              == 0x221, "");
static_assert(offsetof(CitizenComponent, careBuilding)         == 0x228, "");
static_assert(offsetof(CitizenComponent, field_0x230)          == 0x230, "");
static_assert(offsetof(CitizenComponent, home)                 == 0x238, "");
static_assert(offsetof(CitizenComponent, homeSlot)             == 0x240, "");
static_assert(offsetof(CitizenComponent, _pad244)              == 0x244, "");
static_assert(offsetof(CitizenComponent, workplace)            == 0x248, "");
static_assert(offsetof(CitizenComponent, spouse)               == 0x250, "");
static_assert(offsetof(CitizenComponent, _parent)              == 0x258, "");
static_assert(offsetof(CitizenComponent, _firstChild)          == 0x260, "");
static_assert(offsetof(CitizenComponent, _nextSibling)         == 0x268, "");
static_assert(offsetof(CitizenComponent, _prevSibling)         == 0x270, "");
static_assert(offsetof(CitizenComponent, character)            == 0x278, "");
static_assert(offsetof(CitizenComponent, statusIcon)           == 0x280, "");
static_assert(offsetof(CitizenComponent, description)          == 0x288, "");
