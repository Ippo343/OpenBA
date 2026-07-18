#pragma once
// Function declarations for deeply reimplemented ComponentEntity methods.
// Called from engine/reimpl/componententity.cpp wrappers.

#include "ComponentEntity.h"
#include <cstdint>

// ============================================================
// ComponentEntity -- ECS entity container
// Vtable: 0x14015fa78 (12 slots)
// Size: 0x48 bytes
// ============================================================

// Slot0 (vtable +0x00, 0x140021a00): Scalar deleting destructor.
// Resets vtable to ComponentEntity, calls Slot5 on flagged components,
// clears component array, deletes component IBPs, frees array memory,
// calls Object::Internal::~Internal, optionally frees self.
void* ComponentEntity_Dtor(ComponentEntity* self, uint32_t flags);

// Slot1 (vtable +0x08, 0x1400219b0): GetTypeInfo (8b, unhookable).
// Returns: *(void**)(0x1401c5d78)
inline void* ComponentEntity_GetTypeInfo(ComponentEntity* /*self*/) {
    return nullptr; // Runtime-filled; placeholder for standalone exe
}

// Slot2 (vtable +0x10, 0x1400219a0): GetTypeId (7b, unhookable).
// Returns: *(uint32_t*)(0x1401c5d70)
inline uint32_t ComponentEntity_GetTypeId(ComponentEntity* /*self*/) {
    return 0; // Runtime-filled; placeholder for standalone exe
}

// Slot3 (vtable +0x18, 0x1400c3140): Serialize.
// Serializes the entity: external ptr (+0x28), component count (+0x38),
// component IBPs (InternalBasePtr::Serialize), entityFlags (+0x40).
// On load: resizes component array, creates components from factory,
// sets component owner/description, calls Startup on each, then RefreshEntityFlags.
// Implemented as clean code for the standalone exe target.
void ComponentEntity_Serialize(ComponentEntity* self, void* stream);

// Slot4 (vtable +0x20): NULL -- no function, skip.

// Slot5 (vtable +0x28, 0x140021c90): Destructor variant.
// Sets vtable to a base class (Action vtable 0x14015fa48), deletes
// InternalBasePtr at +0x10 if non-null, calls Object::Internal::~Internal,
// optionally frees memory.
void* ComponentEntity_Slot5(ComponentEntity* self, uint32_t flags);

// Slot6 (vtable +0x30, 0x140021c80): GetTypeInfo2 (8b, unhookable).
// Returns: *(void**)(0x1401c5db0)
inline void* ComponentEntity_Slot6(ComponentEntity* /*self*/) {
    return nullptr; // Runtime-filled; placeholder for standalone exe
}

// Slot7 (vtable +0x38, 0x140021c70): GetTypeId2 (7b, unhookable).
// Returns: *(uint32_t*)(0x1401c5da8)
inline uint32_t ComponentEntity_Slot7(ComponentEntity* /*self*/) {
    return 0; // Runtime-filled; placeholder for standalone exe
}

// Slot8 (vtable +0x40, 0x1400c4b40): Load/OnRemove helper.
// Creates a Serializer, serializes entityId (int at +0x18), destroys Serializer.
void ComponentEntity_Slot8(ComponentEntity* self, void* stream);

// Slot9 (vtable +0x48, 0x14003addc): Shared _purecall function.
// This is a library pure virtual call handler, not entity-specific.
// Not reimplemented here.

// Slot10 (vtable +0x50, 0x1400c4a90): Returns 0 (xor eax,eax; ret).
inline int ComponentEntity_Slot10(ComponentEntity* /*self*/) { return 0; }

// Slot11 (vtable +0x58, 0x1400c4b90): Returns 0 (xor eax,eax; ret).
inline int ComponentEntity_Slot11(ComponentEntity* /*self*/) { return 0; }

// ============================================================
// Non-vtable functions
// ============================================================

// Default constructor (0x1400c3620): Initializes all fields to zero/sentinel.
// Calls Object::Internal::Internal, sets vtable, entityId=-1, flags|=0xFFFFFF.
ComponentEntity* ComponentEntity_CtorDefault(ComponentEntity* self);

// Copy/factory constructor (0x1400c3720): Creates entity from description.
// Calls Internal::Internal, sets vtable, initializes fields from description
// parameter, creates components via Factory::CreateObject, sets component
// owner/description, calls StartupComponents and RefreshEntityFlags.
ComponentEntity* ComponentEntity_CtorFromDescription(ComponentEntity* self, void* descriptionPtr);

// RefreshEntityFlags (0x1400c3470): Recomputes entityFlags from all components.
// Iterates component array, ORs in Component::+0x20 (short) for each.
// If bit 2 (update) changed, registers entity in EntityList update bucket.
// If bit 4 (draw) changed, registers entity in EntityList draw bucket.
void ComponentEntity_RefreshEntityFlags(ComponentEntity* self);

// ShutdownComponents (0x1400c3000): Calls Slot5 (Shutdown) on flagged components.
// Only runs if entityFlags bit 8 (0x100) is set.
// For each component where Component+0x20 bit 8 is set, calls vtable+0x28.
void ComponentEntity_ShutdownComponents(ComponentEntity* self);

// StartupComponents (0x1400c30a0): Calls Slot4 (Startup) on flagged components.
// Only runs if entityFlags bit 7 (0x80) is set.
// For each component where Component+0x20 bit 7 is set, calls vtable+0x20.
void ComponentEntity_StartupComponents(ComponentEntity* self);

// EntityList::RegisterEntity (0x1400c48a0): adds a freshly-instantiated entity
// (wrapped in an InternalBasePtr) to the world entity list under a category.
// Assigns id/owner/category-handle, registers update/draw buckets + main array
// + per-category bucket, runs the OnAdded lifecycle pass. Registered for
// standalone so fn_EntityList_RegisterEntity resolves to it.
void EntityList_RegisterEntity_Impl(void* entityList, void* entityIBP, int category);
