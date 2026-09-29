#pragma once
// ============================================================================
// code/Game/Component/NomadComponent.h — ENDGAME B.3.8 block MI-A
//
// `NomadComponent` as a real C++ class with a compiler-generated vtable and its
// state controller as an EMBEDDED MEMBER (B38_RULES.md D1-D6; there is no
// multiple inheritance). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §2 and §6, bytes read 2026-09-26):
//   reflected "NomadComponent", parent "ControllerComponent", ti 0x1401c4640
//   create 0x140009020: MemAlloc(0x78, 8) + ctor 0x14004fdb0
//   createInPlace = the imported `Internal::CreateInPlaceObject` (returns null);
//     the tree's in-place entry builds the object and is kept (B38 B8-13).
//   ctor 0x14004fdb0: Component ctor; +0x28..+0x48 = 0, +0x50 = 0 (dword);
//     member at +0x58; `_flags |= 2`.
//   dtor 0x14004fd50: restamp only, ~Internal (no own work).
//     [0] 0x140009230 = the dtor inlined + MemFree on &1.
//   table 0x14015a020, 18 slots:
//     [0] 0x140009230 dtor  [1]/[2] TypeName/Type  [3] 0x14004fc40 Persist
//     [7] 0x14004fd20 OnRemove (42 B)
//     [16] 0x140003730 `XOR EAX,EAX`  [17] 0x140009060 `LEA RAX,[RCX+0x58]`
//     every other slot = ControllerComponent's / Component's.
//   member table 0x140159fd0 (9 slots): the table-driven state controller over the
//     class's state table (ControllerLive::Dispatcher<NomadCtrlTag>), stamped at
//     this+0x58 after the interface 0x14015f070; back-pointer at this+0x70.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/ControllerComponent.h"
#include "Component/controller_state_table.h"
#include "Object/exe_type.h"
#include "math/Math.h"      // Math::Vector3
#include "system/Value.h"   // System::Time

class ComponentEntity;
class CitizenComponent;

struct NomadCtrlTag {};

class NomadComponent final : public ControllerComponent {
    DECLARE_EXE_TYPE(NomadComponent)

public:
    using StateController = ::ControllerLive::TableController<NomadCtrlTag>;

    NomadComponent();                                     // 0x14004fdb0

    // 0x14004fd50
    ~NomadComponent() override;                                   // [0]  0x140009230
    void          Persist(void* stream) override;                 // [3]  0x14004fc40
    void          OnRemove() override;                            // [7]  0x14004fd20
    int         GetDefaultState() const override { return 0; }  // [16] 0x140003730
    Controller* GetController() override { return &_controller; }  // [17] 0x140009060

    // ---- non-virtual members (retail addresses) ----
    void Init(ComponentEntity* const* hall, const Math::Vector3* spawn,
              const Math::Vector3* meet);                         // 0x140050030
    CitizenComponent* ConvertToCitizen(CitizenComponent** out, CitizenComponent* const* parent,
                                       int gender, char isChild); // 0x1400501a0
    void MoveTo(Math::Vector3* dest);                             // 0x14004fb60
    // The state table's handlers (the controller hands them this component).
    char EnterWalkIn();                                           // 0x14004faf0 state 0
    char TickWalkIn(void* sm);                                    // 0x14004f9c0
    char OnLeave(void* sm);                                       // 0x14004f4a0 event 1
    char TickLoiter(void* time);                                  // 0x14004f600 state 1
    char OnDestroy();                                             // 0x14004f420 event 0
    char EnterWalkOut();                                          // 0x14004f5e0 state 2
    char TickWalkOut();                                           // 0x14004f530

    // PUBLIC while the ported bodies read them (B38 decision 1). Types from Persist
    // 0x14004fc40: IBP +0x28 (validated ComponentEntity), Vector3 +0x30 / +0x3c, int
    // +0x50, Time +0x48.
    ComponentEntity* hall;         // +0x28 the town hall entity the nomad walks to
    Math::Vector3    spawnPoint;   // +0x30 map-edge cell it arrived at (and leaves by)
    Math::Vector3    meetPoint;    // +0x3C the hall's use point it stands at
    System::Time     wanderTimer;  // +0x48 loiter countdown
    int32_t          gender;       // +0x50 Rand(2) at Init; the mesh group and material
    uint8_t          _pad54[4];    // +0x54
    StateController  _controller;  // +0x58 the embedded controller (D6)
};

static_assert(sizeof(NomadComponent) == 0x78, "NomadComponent is 0x78 (create 0x140009020)");
static_assert(alignof(NomadComponent) == 8, "NomadComponent alignment");
static_assert(offsetof(NomadComponent, _controller) == 0x58,
              "NomadComponent's controller member at +0x58 (slot 17 0x140009060: LEA RAX,[RCX+0x58])");
static_assert(offsetof(NomadComponent, _controller) + offsetof(ControllerComponent::Controller, _owner) == 0x70,
              "controller back-pointer at +0x70 (ctor 0x14004fdb0)");
static_assert(offsetof(NomadComponent, hall) == 0x28, "NomadComponent +0x28");
static_assert(offsetof(NomadComponent, spawnPoint) == 0x30, "NomadComponent +0x30");
static_assert(offsetof(NomadComponent, meetPoint) == 0x3C, "NomadComponent +0x3C");
static_assert(offsetof(NomadComponent, wanderTimer) == 0x48, "NomadComponent +0x48");
static_assert(offsetof(NomadComponent, gender) == 0x50, "NomadComponent +0x50");
static_assert(offsetof(NomadComponent, _pad54) == 0x54, "NomadComponent +0x54");
