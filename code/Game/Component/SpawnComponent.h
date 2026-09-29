#pragma once
// ============================================================================
// code/Game/Component/SpawnComponent.h — ENDGAME B.3.8 block CIT
//
// `SpawnComponent` (per-entry spawn timers for wild food / trees) as a real C++
// class with a compiler-generated vtable (B38_RULES.md D1-D4). The engine/types
// POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "SpawnComponent", parent "Component", ti 0x1401c48e0
//   create 0x14000c170: MemAlloc(0x38, 8) + ctor 0x140056030
//   ctor   0x140056030: Component ctor, stamp 0x14015a670, +0x28 = +0x30 = 0,
//                       flags |= 0x88
//   dtor   0x14000c350 (scalar): restamp, MemFree(+0x28), +0x28 = +0x30 = 0
//                       (the timer array), stamp 0x140162920, ~Internal,
//                       MemFree on &1
//   table  0x14015a670, 16 slots:
//     [0] 0x14000c350 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x140055ee0 Persist                [4] 0x140055e90 Startup
//     [9] 0x140055ad0 Update                 every other slot = Component's
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"
#include "system/Value.h"   // System::Time

class SpawnDescription;   // Component/SpawnDescription.h

class SpawnComponent final : public Component {
    DECLARE_EXE_TYPE(SpawnComponent)

public:
    SpawnComponent();                                             // 0x140056030

    // PORT GAP (retail 0x14000c350) — B38 D2: retail frees the timer array at
    // +0x28; the tree never did (Inert_ScalarDtor).
    ~SpawnComponent() override = default;                         // [0] 0x14000c350
    void Persist(void* stream) override;                          // [3] 0x140055ee0
    void Startup() override;                                      // [4] 0x140055e90
    void Update(void* time) override;                             // [9] 0x140055ad0

    // 0x140055cc0: the worldgen seed (one DoSpawn per entry in age, repeated by age).
    void InitialSpawn();
    // 0x140055780: roll one spawn of `desc->_entries[entryIndex]` near the owner;
    // `randomGrowth` sets the new entity's growth to a random value.
    void DoSpawn(SpawnDescription* desc, int entryIndex, char randomGrowth);

    // PUBLIC while the ported bodies read them (B38 decision 1). Persist 0x140055ee0.
    System::Time* timers;  // +0x28 Array<Time> data, one per spawn entry (freed by the retail dtor)
    int32_t  timerCount;   // +0x30
    int32_t  timerCapacity; // +0x34
};

static_assert(sizeof(SpawnComponent) == 0x38, "SpawnComponent is 0x38 (create 0x14000c170)");
static_assert(alignof(SpawnComponent) == 8, "SpawnComponent alignment");
static_assert(offsetof(SpawnComponent, timers) == 0x28, "");
static_assert(offsetof(SpawnComponent, timerCount) == 0x30, "");
static_assert(offsetof(SpawnComponent, timerCapacity) == 0x34, "");
