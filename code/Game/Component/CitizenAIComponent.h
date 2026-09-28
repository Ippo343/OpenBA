#pragma once
// ============================================================================
// code/Game/Component/CitizenAIComponent.h — ENDGAME B.3.8 block CIT
//
// `CitizenAIComponent` (a citizen's decision state machine) as a real C++ class
// with a compiler-generated vtable (B38_RULES.md D1-D4). The engine/types POD
// is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "CitizenAIComponent", parent "Component", ti 0x1401c70e8
//   create 0x14002fb20: MemAlloc(0xd0, 8) + ctor 0x14010fb70
//   ctor   0x14010fb70: Component ctor, stamp 0x140162470, +0x28 = 0xc,
//                       +0x2c = -1, +0x30 (word) = 0, +0x38 = 0, sixteen
//                       System::Time() at +0x40..+0xb8 (import 0x140152af8),
//                       +0xc0 = +0xc8 = 0, flags |= 0x89
//   dtor   0x14002fcf0 (scalar, 83 B): restamp, ~Internal, MemFree on &1
//   table  0x140162470, 16 slots:
//     [0] 0x14002fcf0 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x14010f960 Persist                [4] 0x14010f920 Startup
//     [6] 0x14010f880 OnAdded (JMP 0x14010f890 CheckAge)
//     [9] 0x14010f3e0 Update                 [15] 0x14010f340 OnMessage
//     every other slot = Component's
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class CitizenAIComponent final : public Component {
    DECLARE_EXE_TYPE(CitizenAIComponent)

public:
    CitizenAIComponent();                                         // 0x14010fb70

    // no own work
    ~CitizenAIComponent() override = default;                     // [0]  0x14002fcf0
    void Persist(void* stream) override;                          // [3]  0x14010f960
    void Startup() override;                                      // [4]  0x14010f920
    void OnAdded() override;                                      // [6]  0x14010f880 PORT GAP
    void Update(void* time) override;                             // [9]  0x14010f3e0
    void OnMessage(int message, void* data,
                   uint32_t arg0, uint32_t arg1) override;        // [15] 0x14010f340 PORT GAP

    // PUBLIC while the ported bodies read them (B38 decision 1).
    uint32_t aiState;      // +0x28 current AI state (ctor 0xc)
    int32_t  targetId;     // +0x2C pending state (ctor -1)
    uint16_t data30;       // +0x30
    uint8_t  _pad32[6];    // +0x32
    uint64_t data38;       // +0x38
    int64_t  timers[16];   // +0x40 System::Time per-state cooldowns
    uint64_t dataC0;       // +0xC0
    uint64_t dataC8;       // +0xC8
};

// PORT GAP (ENDGAME B.3.9 CB): the SendComponentMessage actions the work / AI action
// ctors build (retail: ~27 readers of [0x1401c70e8], e.g. PerformWork_Action_CopyCtor
// 0x1400baa7e, Pathfinding_FindPath 0x1400bf81e, RemoveAnimal_Action_Ctor 0x1400bbf30)
// target CitizenAIComponent, and retail delivers them to its slot-15 OnMessage
// 0x14010f340 (150 B: Update the sibling CitizenComponent (+0x38) with a zero
// System::Time, then -- unless the entity is marked for removal (+0x40 bit 10) --
// this->Update(zero Time): an immediate AI re-decision). That body is not ported
// (OnMessage below is the inert stub), so the tree keeps the target id at 0: the
// scan in SendComponentMessage::AbstractImpl finds no component and the action
// answers 2, exactly as it did while the cell read 0. Flip this to
// CitizenAIComponent::TypeId() in the same commit that ports 0x14010f340.
constexpr uint32_t kCitizenAIMessageTargetTypeId =
    /* CitizenAIComponent::TypeId() once CitizenAIComponent::OnMessage (0x14010f340) is ported */ 0u;

static_assert(sizeof(CitizenAIComponent) == 0xD0, "CitizenAIComponent is 0xd0 (create 0x14002fb20)");
static_assert(alignof(CitizenAIComponent) == 8, "CitizenAIComponent alignment");
static_assert(offsetof(CitizenAIComponent, aiState)  == 0x28, "");
static_assert(offsetof(CitizenAIComponent, targetId) == 0x2C, "");
static_assert(offsetof(CitizenAIComponent, data30)   == 0x30, "");
static_assert(offsetof(CitizenAIComponent, data38)   == 0x38, "");
static_assert(offsetof(CitizenAIComponent, timers)   == 0x40, "");
static_assert(offsetof(CitizenAIComponent, dataC0)   == 0xC0, "");
static_assert(offsetof(CitizenAIComponent, dataC8)   == 0xC8, "");
