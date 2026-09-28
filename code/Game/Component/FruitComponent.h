#pragma once
// ============================================================================
// code/Game/Component/FruitComponent.h — ENDGAME B.3.8 block CIT
//
// `FruitComponent` (orchard-tree ripening) as a real C++ class with a
// compiler-generated vtable (B38_RULES.md D1-D4). The engine/types POD is a
// forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "FruitComponent", parent "Component", ti 0x1401c4250
//   create 0x140005920: MemAlloc(0x38, 8) + ctor 0x14004a160
//   ctor   0x14004a160: Component ctor, stamp 0x140159450, +0x28 = 0,
//                       +0x30 (dword) = 0, +0x34 (byte) = 0, flags |= 0x88
//   dtor   0x140005b20 (scalar, 83 B): restamp, ~Internal, MemFree on &1
//   table  0x140159450, 16 slots:
//     [0] 0x140005b20 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x14004a070 Persist                [4] 0x14004a030 Startup
//     [9] 0x140049de0 Update                 every other slot = Component's
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class FruitComponent final : public Component {
    DECLARE_EXE_TYPE(FruitComponent)

public:
    FruitComponent();                                             // 0x14004a160

    // no own work
    ~FruitComponent() override = default;                         // [0] 0x140005b20
    void Persist(void* stream) override;                          // [3] 0x14004a070
    void Startup() override;                                      // [4] 0x14004a030
    void Update(void* time) override;                             // [9] 0x140049de0

    // PUBLIC while the ported bodies read them (B38 decision 1).
    void*    field28;      // +0x28 sibling NaturalResourceComponent
    uint32_t field30;      // +0x30 float ripeness
    uint8_t  fruitFlag;    // +0x34
    uint8_t  _pad35[3];    // +0x35
};

static_assert(sizeof(FruitComponent) == 0x38, "FruitComponent is 0x38 (create 0x140005920)");
static_assert(alignof(FruitComponent) == 8, "FruitComponent alignment");
static_assert(offsetof(FruitComponent, field28)   == 0x28, "");
static_assert(offsetof(FruitComponent, field30)   == 0x30, "");
static_assert(offsetof(FruitComponent, fruitFlag) == 0x34, "");
