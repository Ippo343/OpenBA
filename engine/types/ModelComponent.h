#pragma once
// ModelComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x170.
//
// Decompiled from: ModelComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140163ba0
//   +0x28: Graphics::MeshInstance sub-object (initialized by MeshInstance ctor)
//   +0x148: Math::Transform sub-object (initialized by Transform ctor)
//   +0x164 = 0 (qword), +0x16C = -1 (int32)
//   flags |= 0x83
//
// Contains large embedded sub-objects — MeshInstance spans +0x28..+0x147
// (0x120 bytes) and Transform spans +0x148..+0x163 (0x1C bytes).

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace ModelComponentAddr {
    constexpr uintptr_t Vtable = 0x140163ba0;
}

struct ModelComponent {
    void*    vtable;            // +0x00
    void*    next;              // +0x08
    void*    owner;             // +0x10
    void*    _unk18;            // +0x18
    uint32_t flags;             // +0x20
    uint8_t  _pad24[4];         // +0x24

    // Graphics::MeshInstance sub-object (0x120 bytes)
    uint8_t  meshInstance[0x120]; // +0x28..+0x147

    // Math::Transform sub-object (0x1C bytes)
    uint8_t  transform[0x1C];    // +0x148..+0x163

    // Trailing fields
    uint8_t  _pad164[8];         // +0x164: zeroed (qword)
    int32_t  modelId;            // +0x16C: initialized to -1
};

static_assert(sizeof(ModelComponent) == 0x170, "ModelComponent size mismatch");
static_assert(offsetof(ModelComponent, vtable) == 0x00, "");
static_assert(offsetof(ModelComponent, flags) == 0x20, "");
static_assert(offsetof(ModelComponent, meshInstance) == 0x28, "");
static_assert(offsetof(ModelComponent, transform) == 0x148, "");
static_assert(offsetof(ModelComponent, modelId) == 0x16C, "");
