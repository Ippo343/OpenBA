#pragma once
// ============================================================================
// code/Game/Component/RawMaterialComponent.h — ENDGAME B.3.8 block VIS
//
// `RawMaterialComponent` as a real C++ class with a compiler-generated vtable
// (B38_RULES.md D1-D4). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "RawMaterialComponent", parent "Component", ti 0x1401c7c80
//   create 0x140037270: MemAlloc(0x40, 8) + ctor 0x140129400
//   ctor   0x140129400: Component ctor, stamp 0x140164170, +0x30 = +0x38 = 0
//                       (+0x28 NOT written), flags |= 0x89
//   dtor   0x1401293c0 (slot 0 0x1400373c0, 83 B: the plain restamp + ~Internal shape)
//   table  0x140164170, 16 slots:
//     [0] 0x1400373c0 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x140129330 Persist                [4] 0x1401292c0 Startup (Ghidra "Serialize")
//     [6] 0x1401291d0 OnAdded (Ghidra "Draw", posts the Pickup work)
//     [9] 0x140129130 Update (expiry timer) PORT GAP
//     every other slot = Component's
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class RawMaterialComponent final : public Component {
    DECLARE_EXE_TYPE(RawMaterialComponent)

public:
    RawMaterialComponent();                                       // 0x140129400

    // 0x1401293c0
    ~RawMaterialComponent() override = default;                   // [0] 0x1400373c0
    void Persist(void* stream) override;                          // [3] 0x140129330
    void Startup() override;                                      // [4] 0x1401292c0
    void OnAdded() override;                                      // [6] 0x1401291d0
    void Update(void* time) override;                             // [9] 0x140129130 PORT GAP

    // PUBLIC while the ported bodies read them (B38 decision 1).
    void*    field28;      // +0x28 material type (set by Startup; retail ctor leaves it)
    void*    field30;      // +0x30 (ctor 0)
    void*    field38;      // +0x38 (ctor 0)
};

static_assert(sizeof(RawMaterialComponent) == 0x40, "RawMaterialComponent is 0x40 (create 0x140037270)");
static_assert(alignof(RawMaterialComponent) == 8, "RawMaterialComponent alignment");
static_assert(offsetof(RawMaterialComponent, field28) == 0x28, "");
static_assert(offsetof(RawMaterialComponent, field30) == 0x30, "");
static_assert(offsetof(RawMaterialComponent, field38) == 0x38, "");
