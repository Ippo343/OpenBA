#pragma once
// ComponentEntity -- The ECS entity container, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: ComponentEntity_Ctor (0x1400c3620), ComponentEntity_Serialize (0x1400c3140),
//                  ComponentEntity_CopyCtor (0x1400c3720), ComponentEntity_Dtor (0x1400c3550),
//                  FUN_1400c3470 (RefreshEntityFlags)
//
// Base: Object::Internal (0x10 bytes)
// This is the container that holds Component* linked lists.
// Each entity has an ID, flags, and component array.
//
// Field analysis:
//   +0x10: entityList -- pointer to the EntityList that owns this entity (set by CopyCtor)
//   +0x18: entityId   -- unique entity ID (-1 sentinel)
//   +0x1C: flags      -- 24-bit flag mask (|= 0xFFFFFF in ctor)
//   +0x20: externals  -- external ptr for serializing the entity's external type
//   +0x28: description-- pointer to entity description (ExternalPtr, set in CopyCtor param2)
//   +0x30: components -- array of InternalBasePtr to Component objects (8 bytes each)
//   +0x38: componentCount -- number of components in the array
//   +0x40: entityFlags -- aggregated component flags (bits from Component::+0x20)
//                         bit 2 = has update components, bit 4 = has draw components
//                         bit 7 (0x80) = has Startup-capable components
//                         bit 8 (0x100) = has Shutdown-capable components
//   +0x44: padding

#include <cstdint>
#include <cstddef>

namespace ComponentEntityAddr {
    constexpr uintptr_t Ctor_Simple  = 0x1400c3550;  // destructor helper (dtor body)
    constexpr uintptr_t Ctor_Full    = 0x1400c3620;  // default ctor
    constexpr uintptr_t CopyCtor     = 0x1400c3720;  // copy/factory ctor
    constexpr uintptr_t Vtable       = 0x14015fa78;
    constexpr uintptr_t RegisterType = 0x14014cfd0;

    // Helper functions
    constexpr uintptr_t RefreshEntityFlags  = 0x1400c3470;
    constexpr uintptr_t ShutdownComponents  = 0x1400c3000;
    constexpr uintptr_t StartupComponents   = 0x1400c30a0;
    constexpr uintptr_t ResizeComponentArray = 0x1400032e0;

    // Type IDs
    constexpr uintptr_t TypeId_Component = 0x1401c72a8;  // Component base class type ID
    constexpr uintptr_t TypeInfo_CE      = 0x1401c5d78;  // ComponentEntity type info ptr
    constexpr uintptr_t TypeId_CE        = 0x1401c5d70;  // ComponentEntity type id
    constexpr uintptr_t TypeInfo_CE_Ext  = 0x1401c5db0;  // ComponentEntity external type info
    constexpr uintptr_t TypeId_CE_Ext    = 0x1401c5da8;  // ComponentEntity external type id
}

struct ComponentEntity {
    // --- Object::Internal base (0x10 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015fa78
    uint8_t  _base08[8];         // +0x08: Internal base (refcount, etc.)

    // --- ComponentEntity fields ---
    void*    entityList;         // +0x10: pointer to EntityList that owns this entity
    int32_t  entityId;           // +0x18: -1 sentinel (unique entity ID)
    uint32_t flags;              // +0x1C: |= 0xFFFFFF (24-bit flag mask)
    // Note: +0x1F is a byte within the flags word, zeroed separately in ctor
    void*    externals;          // +0x20: serialized external ptr (entity external type)
    void*    description;        // +0x28: entity description (ExternalPtr, from CopyCtor param2)
    void*    components;         // +0x30: array of InternalBasePtr (8 bytes each, Component*)
    int32_t  componentCount;     // +0x38: number of components
    uint8_t  _pad3C[4];          // +0x3C: padding (aligns entityFlags to +0x40)
    uint32_t entityFlags;        // +0x40: aggregated component flags from Component+0x20
    uint8_t  _pad44[4];          // +0x44: padding
};

static_assert(sizeof(ComponentEntity) == 0x48, "ComponentEntity size mismatch");
static_assert(offsetof(ComponentEntity, vtable) == 0x00, "");
static_assert(offsetof(ComponentEntity, entityList) == 0x10, "");
static_assert(offsetof(ComponentEntity, entityId) == 0x18, "");
static_assert(offsetof(ComponentEntity, flags) == 0x1C, "");
static_assert(offsetof(ComponentEntity, externals) == 0x20, "");
static_assert(offsetof(ComponentEntity, description) == 0x28, "");
static_assert(offsetof(ComponentEntity, components) == 0x30, "");
static_assert(offsetof(ComponentEntity, componentCount) == 0x38, "");
static_assert(offsetof(ComponentEntity, entityFlags) == 0x40, "");
