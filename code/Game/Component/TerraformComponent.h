#pragma once
// ============================================================================
// code/Game/Component/TerraformComponent.h — ENDGAME B.3.8 block VIS
//
// `TerraformComponent` as a real C++ class with a compiler-generated vtable
// (B38_RULES.md D1-D4). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "TerraformComponent", parent "Component", ti 0x1401c49c0
//   create 0x14000cc90: MemAlloc(0x48, 8) + ctor 0x140057620
//   ctor   0x140057620: Component ctor, stamp 0x14015a880, +0x28/+0x30/+0x38/
//                       +0x40 = 0, flags |= 3 (0x83)
//   dtor   0x1400575b0: MemFree(+0x38), +0x38 = +0x40 = 0, MemFree(+0x28),
//                       +0x28 = +0x30 = 0, restamp, ~Internal
//   table  0x14015a880, 16 slots:
//     [0]  0x14000ce90 scalar deleting dtor  [1]/[2] TypeName/Type
//     [3]  0x140057500 Persist (the undo record)
//     [6]  0x1400574f0 own `RET`             [7] 0x140057350 OnRemove (replay the undo)
//     [12] 0x140057310 OnEvent (msg 4: apply the deformation)
//     every other slot = Component's
// The [12] body is ALSO registered by address (standalone/reg_terraform_live.cpp),
// so it stays a free function and the member delegates (B38 D5).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"
#include "math/Math.h"   // Math::Vector3

class TerraformComponent final : public Component {
    DECLARE_EXE_TYPE(TerraformComponent)

public:
    TerraformComponent();                                         // 0x140057620

    // PORT GAP (retail 0x1400575b0) — B38 D2: retail frees both undo arrays; the
    // tree never did (Inert_ScalarDtor). Kept as the tree's.
    ~TerraformComponent() override = default;                     // [0]  0x14000ce90
    void Persist(void* stream) override;                          // [3]  0x140057500
    void OnAdded() override;                                      // [6]  0x1400574f0 own RET
    void OnRemove() override;                                     // [7]  0x140057350
    void OnEvent(int eventId) override;                           // [12] 0x140057310

    void ApplyMesh(bool ceiling);                                 // 0x140056b50 one heightmap pass over a description mesh
    void ApplyFootprint();                                        // 0x140057120 footprint cells type 10 -> 6

    // PUBLIC while the ported bodies read them (B38 decision 1).
    Math::Vector3* heightRecord; // +0x28 Array<Vector3> of original terrain points (freed by the retail dtor)
    int32_t  heightCount;  // +0x30
    int32_t  heightCap;    // +0x34
    int32_t* tileRecord;   // +0x38 Array<int> of retyped map cell indices (freed by the retail dtor)
    int32_t  tileCount;    // +0x40
    int32_t  tileCap;      // +0x44
};

static_assert(sizeof(TerraformComponent) == 0x48, "TerraformComponent is 0x48 (create 0x14000cc90)");
static_assert(alignof(TerraformComponent) == 8, "TerraformComponent alignment");
static_assert(offsetof(TerraformComponent, heightRecord) == 0x28, "");
static_assert(offsetof(TerraformComponent, heightCount) == 0x30, "");
static_assert(offsetof(TerraformComponent, heightCap) == 0x34, "");
static_assert(offsetof(TerraformComponent, tileRecord) == 0x38, "");
static_assert(offsetof(TerraformComponent, tileCount) == 0x40, "");
static_assert(offsetof(TerraformComponent, tileCap) == 0x44, "");
