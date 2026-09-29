#pragma once
// ============================================================================
// code/Game/Component/ResidenceComponent.h — ENDGAME B.3.8 block MI-A
//
// `ResidenceComponent` as a real C++ class with a compiler-generated vtable and its
// state controller as an EMBEDDED MEMBER (B38_RULES.md D1-D6; there is no
// multiple inheritance). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §2 and §6, bytes read 2026-09-26):
//   reflected "ResidenceComponent", parent "ControllerComponent", ti 0x1401c7cf0
//   create 0x140037440: MemAlloc(0x60, 8) + ctor 0x14012a2a0
//   createInPlace = the imported `Internal::CreateInPlaceObject` (returns null);
//     the tree's in-place entry builds the object and is kept (B38 B8-13).
//   ctor 0x14012a2a0: Component ctor; member at +0x28; +0x48 = +0x50 = +0x58
//     = 0; `_flags |= 0x82`.
//   dtor 0x14012a230 (inlined in [0] 0x140037610): 0x1400175d0 on +0x50 (free
//     each resident slot's inner array, then the slot array; zero +0x50/+0x58),
//     member restamp, ~Internal.
//   table 0x1401642c0, 18 slots:
//     [0] 0x140037610 dtor  [1]/[2] TypeName/Type  [3] 0x14012a170 Persist
//     [4] 0x14012a120 Startup (size the resident array)  [7] 0x14012a0e0 OnRemove
//     [16] 0x1400026c0 `MOV EAX,2`  [17] 0x140004080 `LEA RAX,[RCX+0x28]`
//     every other slot = ControllerComponent's / Component's.
//   member table 0x140164270 (9 slots): the table-driven state controller over the
//     class's state table (ControllerLive::Dispatcher<ResidenceCtrlTag>), stamped at
//     this+0x28 after the interface 0x14015f070; back-pointer at this+0x40.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/ControllerComponent.h"
#include "Component/controller_state_table.h"
#include "Object/exe_type.h"
#include "system/Value.h"   // System::Time
#include "object/Internal.h"   // Object::InternalPtr<T>

class CitizenComponent;
namespace IO { class Serializer; }

struct ResidenceCtrlTag {};

class ResidenceComponent final : public ControllerComponent {
    DECLARE_EXE_TYPE(ResidenceComponent)

public:
    using StateController = ::ControllerLive::TableController<ResidenceCtrlTag>;

    // One family (household slot): Collection::Array<InternalPtr<CitizenComponent>>,
    // 0x10 bytes (0x140037c50 serializes each element as an InternalBasePtr checked
    // against CitizenComponent's type; 0x140007960 copies it; 0x14012a4f0 compares the
    // count against ResidenceDescription::_maxResidentsPerFamily). H5: kept as its raw
    // triple (the retail dtor 0x1400175d0 frees each data buffer by hand).
    struct Family {
        Object::InternalPtr<CitizenComponent>* members;   // +0x00 data
        int32_t            count;     // +0x08
        int32_t            capacity;  // +0x0C
    };

    ResidenceComponent();                                 // 0x14012a2a0

    // 0x14012a230
    ~ResidenceComponent() override;                               // [0]  0x140037610
    void          Persist(void* stream) override;                 // [3]  0x14012a170
    void          Startup() override;                             // [4]  0x14012a120
    void          OnRemove() override;                            // [7]  0x14012a0e0
    int         GetDefaultState() const override { return 2; }  // [16] 0x1400026c0
    Controller* GetController() override { return &_controller; }  // [17] 0x140004080

    // The family roster (non-virtual members; receiver RCX = this, fields +0x50/+0x58).
    bool     IsFamilyFull(int family) const;                          // 0x14012a4f0
    void     RemoveResident(int family, Object::InternalPtr<CitizenComponent>* citizen);  // 0x14012a550
    void     AddResident(int family, Object::InternalPtr<CitizenComponent>* citizen);     // 0x14012a710
    uint32_t CountResidents() const;                                  // 0x140129ff0
    void     EvictResidents();                                        // 0x140129ef0
    static void SerializeFamilies(IO::Serializer& ser, int count, Family* families);  // 0x140037c50
    void SetFamilyCount(int newCount);                     // 0x140037ad0 Array<Family>::SetSize on residentSlots

    // State handlers (the controller's table, 0x14012a350).
    char     EnterBuilt();                                            // 0x140129de0 state 2 enter
    char     EnterDemolish();                                         // 0x140129920 state 3 enter
    char     HeatTick(System::Time* dt);                              // 0x140129970 state 2 per-frame (residence_heattick.cpp)

    // PUBLIC while the ported bodies read them at raw offsets (B38 decision 1).
    StateController _controller;   // +0x28 the embedded controller (D6)
    System::Time    heatRemaining; // +0x48 countdown (heat tick 0x140129970; Persist Time::Serialize)
    Family*         residentSlots; // +0x50 Array<Family> data (H5: the raw triple)
    int32_t         residentSlotCount;    // +0x58 family count (ResidenceDescription::_maxFamilies)
    int32_t         residentSlotCapacity; // +0x5c its capacity (SetSize 0x140037ad0; dtor 0x1400175d0 zeroes the qword)

private:
    void     EvictAndUnregister();   // the body shared by [7] 0x14012a0e0 and 0x140129920
};

static_assert(sizeof(ResidenceComponent) == 0x60, "ResidenceComponent is 0x60 (create 0x140037440)");
static_assert(alignof(ResidenceComponent) == 8, "ResidenceComponent alignment");
static_assert(offsetof(ResidenceComponent, _controller) == 0x28,
              "ResidenceComponent's controller member at +0x28 (slot 17 0x140004080: LEA RAX,[RCX+0x28])");
static_assert(offsetof(ResidenceComponent, _controller) + offsetof(ControllerComponent::Controller, _owner) == 0x40,
              "controller back-pointer at +0x40 (ctor 0x14012a2a0)");
static_assert(offsetof(ResidenceComponent, heatRemaining) == 0x48, "ResidenceComponent +0x48");
static_assert(offsetof(ResidenceComponent, residentSlots) == 0x50, "ResidenceComponent +0x50");
static_assert(offsetof(ResidenceComponent, residentSlotCount) == 0x58, "ResidenceComponent +0x58");
static_assert(offsetof(ResidenceComponent, residentSlotCapacity) == 0x5C, "ResidenceComponent +0x5c (SetSize 0x140037ad0)");
static_assert(sizeof(ResidenceComponent::Family) == 0x10, "a family is 0x10 bytes (0x140037ad0 stride)");
static_assert(offsetof(ResidenceComponent::Family, count) == 0x08, "family count +0x08 (0x14012a53c)");
static_assert(offsetof(ResidenceComponent::Family, capacity) == 0x0C, "family capacity +0x0C (0x140007960)");
