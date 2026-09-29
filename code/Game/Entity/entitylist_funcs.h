#pragma once
// Function declarations for deeply reimplemented EntityList methods.
// Called from code/Game/Game/gameupdateloop.cpp (not hooked directly).

#include "EntityList.h"
#include <cstdint>

// ============================================================
// EntityList — entity management system
// Size: 0xE0 bytes
// ============================================================
//
// EntityList is NOT directly hooked. It is called by the already-hooked
// GameUpdateLoop::Update. The original fn_EntityList_Update (0x1400c4090)
// HANGS due to a spinlock issue — our custom reimpl is used instead.

// ============================================================
// EntityList::Update (0x1400c4090 — CUSTOM REIMPL, original hangs)
// ============================================================
// Main simulation tick for all entities:
//   1. Accumulate time
//   2. PreUpdate tree walk (process staging arrays)
//   3. Tree node iteration — in-order traversal calling component vtable +0x40
//   4. 5-bucket round-robin — call component vtable +0x48 (Update)
//   5. Process removal list
//
// IMPORTANT: The original function hangs due to a spinlock. This custom
// reimplementation replicates the logic without the spinlock.
void EntityList_Update(EntityList* self, void* time);

// ============================================================
// EntityList::PreUpdate (0x1400c3bd0)
// ============================================================
// Processes the staging update array (+0x88/+0x90):
// For each staged entity, either inserts it into the update tree (+0x68)
// or removes it (if flag bit 2 cleared or entity deleted).
// Then clears the staging array.
void EntityList_PreUpdate(EntityList* self);

// ============================================================
// EntityList::PostUpdate (0x1400c43d0)
// ============================================================
// Processes the post-update staging array, then does an in-order tree walk
// of the post-update tree (+0x78), calling FUN_1400c2c70 per entity
// (which calls component vtable +0x50 on components with flag bit 4).
void EntityList_PostUpdate(EntityList* self);

// ============================================================
// EntityList::TreeNodeHandler (0x1400c2de0)
// ============================================================
// Per-entity processing during tree walk:
// If entity has flag bit 2 (0x4) set and is not deleted (bit 10),
// iterates components and calls vtable +0x40 on those with component flag bit 2.
void EntityList_TreeNodeHandler(int64_t entity, void* time);

// ============================================================
// EntityList::PostUpdateNodeHandler (0x1400c2c70)
// ============================================================
// Per-entity processing during post-update tree walk:
// If entity has flag bit 4 (0x10) set and is not deleted,
// iterates components and calls vtable +0x50 on those with component flag bit 4.
void EntityList_PostUpdateNodeHandler(int64_t entity);

// ============================================================
// EntityList::PostUpdateStaging (0x1400c3ab0) — static helper
// ============================================================
// Processes the post-update staging array (mirrors PreUpdate for the
// post-update tree). Exposed (non-static) so the standalone address
// registry can map 0x1400c3ab0 → this reimpl body.
void EntityList_PostUpdateStaging(EntityList* self);

// ============================================================
// EntityList::RemoveEntity (0x1400c38e0)
// ============================================================
// Removes an entity from its bucket, the master array, and both trees.
// Also calls InternalBasePtr::DeletePtr to release the entity reference.
void EntityList_RemoveEntity(EntityList* self, void* entityIBP);

// ============================================================
// EntityList::Serialize (0x1400c44a0)
// ============================================================
// Save/load the entity list. Steps:
// - Serializer construction
// - PreUpdate + PostUpdate staging flush
// - On save: removal list processing
// - Time serialization
// - Master array count + IBP serialization (with IsDerivedFrom validation)
// - Bucket tree serialization via helper
// - Per-bucket processCount and cursor bulk serialization
// - Per-entity metadata serialization (master index, bucket index, bucket ID, time)
void EntityList_Serialize(EntityList* self, void* stream);
