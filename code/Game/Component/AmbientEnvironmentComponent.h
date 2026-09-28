#pragma once
// ============================================================================
// code/Game/Component/AmbientEnvironmentComponent.h — ENDGAME B.3.8 block CIT
//
// `AmbientEnvironmentComponent` as a real C++ class with a compiler-generated
// vtable (B38_RULES.md D1-D4). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "AmbientEnvironmentComponent", parent "Component", ti 0x1401c3ed0
//   create 0x140001ee0: MemAlloc(0x48, 8) + ctor 0x1400438d0
//   ctor   0x1400438d0: Component ctor, stamp 0x140158b70, +0x38 = 0,
//                       +0x40 (dword) = 0, flags |= 0x88. +0x28/+0x30 are NOT
//                       written (the tree zeroes them; so does this ctor)
//   dtor   0x140002170 (scalar, 83 B): restamp, ~Internal, MemFree on &1
//   table  0x140158b70, 16 slots:
//     [0] 0x140002170 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x1400437e0 Persist                [9] 0x140043690 Update (JMP 0x140043310)
//     every other slot = Component's
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class AmbientEnvironmentComponent final : public Component {
    DECLARE_EXE_TYPE(AmbientEnvironmentComponent)

public:
    AmbientEnvironmentComponent();                                // 0x1400438d0

    // no own work
    ~AmbientEnvironmentComponent() override = default;            // [0] 0x140002170
    void Persist(void* stream) override;                          // [3] 0x1400437e0
    void Update(void* time) override;                             // [9] 0x140043690

    // PUBLIC while the ported bodies read them (B38 decision 1).
    void*    field28;      // +0x28 not written by the retail ctor
    void*    field30;      // +0x30 not written by the retail ctor
    void*    field38;      // +0x38
    uint32_t field40;      // +0x40
    uint8_t  _pad44[4];    // +0x44
};

static_assert(sizeof(AmbientEnvironmentComponent) == 0x48, "AmbientEnvironmentComponent is 0x48 (create 0x140001ee0)");
static_assert(alignof(AmbientEnvironmentComponent) == 8, "AmbientEnvironmentComponent alignment");
static_assert(offsetof(AmbientEnvironmentComponent, field28) == 0x28, "");
static_assert(offsetof(AmbientEnvironmentComponent, field38) == 0x38, "");
static_assert(offsetof(AmbientEnvironmentComponent, field40) == 0x40, "");
