#pragma once
// ============================================================================
// code/Game/Component/PopulationComponent.h — ENDGAME B.3.8 block CIT
//
// `PopulationComponent` (a wild-animal population: habitability grid, density
// cells, the herds it spawns) as a real C++ class with a compiler-generated
// vtable (B38_RULES.md D1-D4). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "PopulationComponent", parent "Component", ti 0x1401c4790
//   create 0x14000af50: MemAlloc(0x70, 8) + ctor 0x1400546d0
//   ctor   0x1400546d0: Component ctor, stamp 0x14015a360, +0x28 = 0, +0x30
//                       (dword) = 0, +0x38 = +0x40 = 0, +0x48 (dword) = 0,
//                       +0x50..+0x68 = 0, flags |= 0xcb
//   dtor   0x140054660: restamp, MemFree(+0x50) + zero +0x50/+0x58,
//                       MemFree(+0x38) + zero +0x38/+0x40, stamp 0x140162920,
//                       ~Internal. Scalar 0x14000b0a0 = dtor + MemFree on &1
//   table  0x14015a360, 16 slots:
//     [0] 0x14000b0a0 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x1400544a0 Persist                [4] 0x140054220 Startup (herds)
//     [6] 0x1400541c0 OnAdded (town registry)
//     [7] 0x140054180 OnRemove               [9] 0x140054070 Update
//     [11] 0x140054210 Slot11 (its own `RET`) every other slot = Component's
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class PopulationComponent final : public Component {
    DECLARE_EXE_TYPE(PopulationComponent)

public:
    PopulationComponent();                                        // 0x1400546d0

    // PORT GAP (retail 0x140054660) — B38 D2: retail frees the arrays at +0x50
    // and +0x38; the tree never did (Inert_ScalarDtor).
    ~PopulationComponent() override = default;                    // [0]  0x14000b0a0
    void Persist(void* stream) override;                          // [3]  0x1400544a0
    void Startup() override;                                      // [4]  0x140054220
    void OnAdded() override;                                      // [6]  0x1400541c0
    void OnRemove() override;                                     // [7]  0x140054180
    void Update(void* time) override;                             // [9]  0x140054070
    void Slot11() override;                                       // [11] 0x140054210 (RET)
    float HerdCellFood(int cell) const;                           // 0x140053f20 (body: herdcomponent.cpp)

    // PUBLIC while the ported bodies read them (B38 decision 1).
    void*    field28;      // +0x28 PopulationDescription
    uint32_t field30;      // +0x30
    uint8_t  _pad34[4];    // +0x34
    void*    field38;      // +0x38 habitability grid data (freed by the retail dtor)
    void*    field40;      // +0x40 its count / capacity
    uint32_t field48;      // +0x48 herd grid dimension
    uint8_t  _pad4C[4];    // +0x4C
    void*    field50;      // +0x50 density cells data (freed by the retail dtor)
    void*    field58;      // +0x58 its count / capacity
    void*    field60;      // +0x60 herd-entity list
    void*    field68;      // +0x68
};

static_assert(sizeof(PopulationComponent) == 0x70, "PopulationComponent is 0x70 (create 0x14000af50)");
static_assert(alignof(PopulationComponent) == 8, "PopulationComponent alignment");
static_assert(offsetof(PopulationComponent, field28) == 0x28, "");
static_assert(offsetof(PopulationComponent, field30) == 0x30, "");
static_assert(offsetof(PopulationComponent, field38) == 0x38, "");
static_assert(offsetof(PopulationComponent, field40) == 0x40, "");
static_assert(offsetof(PopulationComponent, field48) == 0x48, "");
static_assert(offsetof(PopulationComponent, field50) == 0x50, "");
static_assert(offsetof(PopulationComponent, field58) == 0x58, "");
static_assert(offsetof(PopulationComponent, field60) == 0x60, "");
static_assert(offsetof(PopulationComponent, field68) == 0x68, "");
