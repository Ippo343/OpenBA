#pragma once
// CreateNaturalResource -- Action to create a natural resource, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: CreateNaturalResource_Factory (0x140022204, inline ctor)
// Inherits: Object::Internal -> Action (0x20) -> CreateNaturalResource
//
// Fields from Serialize (version 1):
//   +0x20: description entity (ExternalPtr/type ref, serialized via FUN_140002340)
//   +0x28: InternalBasePtr (parent entity / ComponentEntity, version > 0)
//   +0x30: int32_t (cell index / resource slot)

#include <cstdint>
#include <cstddef>

namespace CreateNaturalResourceAddr {
    constexpr uintptr_t Factory = 0x140022180;
    constexpr uintptr_t Vtable  = 0x14015fca0;
}

struct CreateNaturalResource {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015fca0
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding

    // --- CreateNaturalResource fields ---
    void*    descEntity;         // +0x20: description entity (ExternalPtr)
    void*    parentEntity;       // +0x28: InternalBasePtr (parent entity)
    int32_t  cellIndex;          // +0x30: map cell index / resource slot
    uint8_t  _pad34[4];          // +0x34: padding
};

static_assert(sizeof(CreateNaturalResource) == 0x38, "CreateNaturalResource size mismatch");
static_assert(offsetof(CreateNaturalResource, vtable) == 0x00, "");
static_assert(offsetof(CreateNaturalResource, descEntity) == 0x20, "");
static_assert(offsetof(CreateNaturalResource, parentEntity) == 0x28, "");
static_assert(offsetof(CreateNaturalResource, cellIndex) == 0x30, "");
