#pragma once
// ============================================================================
// code/Game/Component/AmbientEmitterComponent.h — ENDGAME B.3.8 block CIT
//
// `AmbientEmitterComponent` as a real C++ class with a compiler-generated
// vtable (B38_RULES.md D1-D4). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "AmbientEmitterComponent", parent "Component", ti 0x1401c3e60
//   create 0x1400014b0: MemAlloc(0x48, 8) + ctor 0x1400430c0
//   ctor   0x1400430c0: Component ctor, stamp 0x140158a60, +0x28 = +0x30(dword)
//                       = +0x38 = +0x40 = 0, flags |= 0x43
//   dtor   0x1400016d0 (scalar): MemFree(+0x38); +0x38 = +0x40 = 0 (the emitter
//                       array), stamp 0x140162920, ~Internal, MemFree on &1
//   table  0x140158a60, 16 slots:
//     [0] 0x1400016d0 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x140042f30 Persist                [6] 0x140042ba0 OnAdded (emitters)
//     [7] 0x140042b90 OnRemove (JMP 0x140042d70)  every other slot = Component's
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class AmbientEmitterComponent final : public Component {
    DECLARE_EXE_TYPE(AmbientEmitterComponent)

public:
    AmbientEmitterComponent();                                    // 0x1400430c0

    // PORT GAP (retail 0x1400016d0) — B38 D2: retail frees the emitter array at
    // +0x38; the tree never did (Inert_ScalarDtor). Kept as the tree's.
    ~AmbientEmitterComponent() override = default;                // [0] 0x1400016d0
    void Persist(void* stream) override;                          // [3] 0x140042f30
    void OnAdded() override;                                      // [6] 0x140042ba0
    void OnRemove() override;                                     // [7] 0x140042b90 PORT GAP

    // PUBLIC while the ported bodies read them (B38 decision 1).
    void*    field28;      // +0x28 Vector3 position (x, y)
    uint32_t field30;      // +0x30 Vector3 position (z)
    uint8_t  _pad34[4];    // +0x34
    void*    field38;      // +0x38 emitter array data (0x10-byte entries)
    void*    field40;      // +0x40 emitter array count / capacity
};

static_assert(sizeof(AmbientEmitterComponent) == 0x48, "AmbientEmitterComponent is 0x48 (create 0x1400014b0)");
static_assert(alignof(AmbientEmitterComponent) == 8, "AmbientEmitterComponent alignment");
static_assert(offsetof(AmbientEmitterComponent, field28) == 0x28, "");
static_assert(offsetof(AmbientEmitterComponent, field30) == 0x30, "");
static_assert(offsetof(AmbientEmitterComponent, field38) == 0x38, "");
static_assert(offsetof(AmbientEmitterComponent, field40) == 0x40, "");
