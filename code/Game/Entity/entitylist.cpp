// code/Game/Entity/entitylist.cpp
// Clean reimplementation of EntityList methods from Application-x64.exe.
//
// EntityList (0xE0 bytes) — manages all ComponentEntity instances in the world.
// NOT directly hooked — called by GameUpdateLoop::Update (already hooked).
//
// The original EntityList::Update (0x1400c4090) HANGS due to a spinlock.
// This module provides a custom reimplementation that replicates the logic
// without the spinlock, preserving the workaround from gameupdateloop.cpp.
//
// Methods reimplemented:
//   - EntityList::Update           (0x1400c4090) — custom reimpl (original hangs)
//   - EntityList::PreUpdate        (0x1400c3bd0) — calls original (no spinlock issue)
//   - EntityList::PostUpdate       (0x1400c43d0) — calls original (no spinlock issue)
//   - EntityList::TreeNodeHandler  (0x1400c2de0) — inline reimpl for Update tree walk
//   - EntityList::PostUpdateNodeHandler (0x1400c2c70) — calls original
//   - EntityList::RemoveEntity     (0x1400c38e0) — calls original
//   - EntityList::FlagAggregation  (0x1400c3470) — calls original
//   - EntityList::Serialize        (0x1400c44a0) — calls original
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Entity/entitylist_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved function pointers (exe-internal, resolved lazily)
// ============================================================

// EntityList::PreUpdate (0x1400c3bd0) — tree walk, process staging arrays
typedef void (*fn_EntityList_PreUpdate_t)(EntityList* self);
static fn_EntityList_PreUpdate_t s_fn_PreUpdate = nullptr;

// EntityList::PostUpdate (0x1400c43d0) — post-update tree walk
typedef void (*fn_EntityList_PostUpdate_t)(EntityList* self);
static fn_EntityList_PostUpdate_t s_fn_PostUpdate = nullptr;

// EntityList::PostUpdateStaging (0x1400c3ab0) — process post-update staging array
typedef void (*fn_EntityList_PostUpdateStaging_t)(EntityList* self);
static fn_EntityList_PostUpdateStaging_t s_fn_PostUpdateStaging = nullptr;

// EntityList::RemoveEntity (0x1400c38e0) — remove entity from buckets + trees
typedef void (*fn_EntityList_RemoveEntity_t)(EntityList* self, void* entityIBP);
static fn_EntityList_RemoveEntity_t s_fn_RemoveEntity = nullptr;

// EntityList::FlagAggregation (0x1400c3470) — aggregate entity flags
typedef void (*fn_EntityList_FlagAggregation_t)(void* entity);
static fn_EntityList_FlagAggregation_t s_fn_FlagAggregation = nullptr;

// EntityList::Serialize (0x1400c44a0) — save/load
typedef void (*fn_EntityList_Serialize_t)(EntityList* self, void* stream);
static fn_EntityList_Serialize_t s_fn_Serialize = nullptr;

// EntityList::PostUpdateNodeHandler (0x1400c2c70) — per-entity post-update
typedef void (*fn_PostUpdateNodeHandler_t)(int64_t entity);
static fn_PostUpdateNodeHandler_t s_fn_PostUpdateNodeHandler = nullptr;

static bool s_initialized = false;

// ============================================================
// EntityList_InitFuncs — resolve all function pointers
// ============================================================
void EntityList_InitFuncs() {
    if (s_initialized) return;

    s_fn_PreUpdate = reinterpret_cast<fn_EntityList_PreUpdate_t>(
        ResolveAddr(Module::Exe, 0x1400c3bd0));
    s_fn_PostUpdate = reinterpret_cast<fn_EntityList_PostUpdate_t>(
        ResolveAddr(Module::Exe, 0x1400c43d0));
    s_fn_PostUpdateStaging = reinterpret_cast<fn_EntityList_PostUpdateStaging_t>(
        ResolveAddr(Module::Exe, 0x1400c3ab0));
    s_fn_RemoveEntity = reinterpret_cast<fn_EntityList_RemoveEntity_t>(
        ResolveAddr(Module::Exe, 0x1400c38e0));
    s_fn_FlagAggregation = reinterpret_cast<fn_EntityList_FlagAggregation_t>(
        ResolveAddr(Module::Exe, 0x1400c3470));
    s_fn_Serialize = reinterpret_cast<fn_EntityList_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400c44a0));
    s_fn_PostUpdateNodeHandler = reinterpret_cast<fn_PostUpdateNodeHandler_t>(
        ResolveAddr(Module::Exe, 0x1400c2c70));

    s_initialized = true;

#ifdef _DEBUG
    printf("[EntityList] Function pointers resolved:\n");
    printf("  PreUpdate          = %p\n", (void*)s_fn_PreUpdate);
    printf("  PostUpdate         = %p\n", (void*)s_fn_PostUpdate);
    printf("  PostUpdateStaging  = %p\n", (void*)s_fn_PostUpdateStaging);
    printf("  RemoveEntity       = %p\n", (void*)s_fn_RemoveEntity);
    printf("  FlagAggregation    = %p\n", (void*)s_fn_FlagAggregation);
    printf("  Serialize          = %p\n", (void*)s_fn_Serialize);
    printf("  PostUpdateNodeHdlr = %p\n", (void*)s_fn_PostUpdateNodeHandler);
#endif
}

// ============================================================
// EntityList::TreeNodeHandler (0x1400c2de0)
// ============================================================
// Per-entity handler during update tree walk.
// Checks entity flag bit 2 (0x4) and calls component vtable +0x40
// on components that also have flag bit 2 set.
//
// Decompiled from 0x1400c2de0:
//   if ((entity+0x40 & 0x4) && !(entity+0x40 >> 10 & 1))
//     for each component:
//       if (comp+0x20 >> 2 & 1)  // component flag bit 2
//         call comp->vtable[+0x40](comp, time)
//       break if entity deleted (entity+0x40 & 0x400)
void EntityList_TreeNodeHandler(int64_t entity, void* time) {
    uint32_t entityFlags = *(uint32_t*)(entity + 0x40);

    // Check entity has "needs tree tick" flag (bit 2) and is not deleted (bit 10)
    if ((entityFlags & 0x4) == 0) return;
    if ((entityFlags >> 10) & 1) return;

    int32_t compCount = *(int32_t*)(entity + 0x38);
    int64_t ci = 0;

    if (compCount <= 0) return;

    do {
        // Read component IBP and CheckPtr
        void* compIBP = (void*)(*(uintptr_t*)(entity + 0x30) + ci * 8);
        fn_InternalBasePtr_CheckPtr(compIBP);
        int64_t comp = *(int64_t*)compIBP;

        // Component flags at byte offset +0x20 from component base.
        // Verified from disassembly at 0x1400c2e42: MOV ECX,dword ptr [RAX + 0x20]
        // Then SHR ECX,0x2; TEST CL,0x1 checks bit 2.
        uint32_t compFlags = *(uint32_t*)((uintptr_t)comp + 0x20);

        if ((compFlags >> 2) & 1) {
            // CheckPtr again before vtable call
            fn_InternalBasePtr_CheckPtr(compIBP);
            comp = *(int64_t*)compIBP;

            // Call component vtable slot +0x40 (slot index 8)
            void** vtbl = *(void***)(uintptr_t)comp;
            typedef void (*CompSlot8_t)(void*, void*);
            ((CompSlot8_t)vtbl[8])((void*)(uintptr_t)comp, time);
        }

        ci++;

        // Break if entity was deleted during component processing
        if (*(uint32_t*)(entity + 0x40) & 0x400) break;

    } while (ci < compCount);
}

// ============================================================
// EntityList::PostUpdateNodeHandler (0x1400c2c70)
// ============================================================
// Per-entity handler during post-update tree walk.
// Calls original function.
void EntityList_PostUpdateNodeHandler(int64_t entity) {
    if (s_fn_PostUpdateNodeHandler) {
        s_fn_PostUpdateNodeHandler(entity);
    }
}

// ============================================================
// Helper: In-order tree traversal
// ============================================================
// Finds the leftmost node in the tree (minimum).
static EntityTreeNode* TreeMinimum(EntityTreeNode* node) {
    if (!node) return nullptr;
    while (node->left) {
        node = node->left;
    }
    return node;
}

// In-order successor of a node.
static EntityTreeNode* TreeSuccessor(EntityTreeNode* node) {
    if (!node) return nullptr;

    // If right child exists, successor is leftmost of right subtree
    if (node->right) {
        return TreeMinimum(node->right);
    }

    // Walk up until we find a node that is a left child of its parent
    EntityTreeNode* parent = node->parent;
    while (parent && parent->right == node) {
        node = parent;
        parent = node->parent;
    }
    return parent;
}

// ============================================================
// EntityList::Update (0x1400c4090 — CUSTOM REIMPL)
// ============================================================
// The original function hangs due to a spinlock issue in the hooked environment.
// This reimplementation replicates the exact logic from the Ghidra decompilation.
//
// Steps:
//   1. Accumulate time: *self += *time
//   2. PreUpdate: process staging arrays into trees
//   3. Tree walk: in-order traversal of update tree, call TreeNodeHandler per entity
//   4. 5-bucket round-robin: for each bucket, tick processCount entities with vtable[+0x48]
//   5. If removal list non-empty: PreUpdate again, PostUpdateStaging, then process removals
void EntityList_Update(EntityList* self, void* time) {
    // Step 1: Accumulate time
    self->accumulatedTime += *(int64_t*)time;

    // Step 2: PreUpdate — process staging arrays into update tree
    if (s_fn_PreUpdate) {
        s_fn_PreUpdate(self);
    }

    // Step 3: In-order traversal of update tree (+0x68)
    EntityTreeNode* treeRoot = self->updateTreeRoot;
    if (treeRoot) {
        // Find leftmost node (in-order start)
        EntityTreeNode* node = TreeMinimum(treeRoot);

        while (node) {
            // CheckPtr on the node (it's an IBP)
            fn_InternalBasePtr_CheckPtr((void*)node);

            // Check entity not deleted (bit 10 of flags at entity+0x40)
            int64_t entity = (int64_t)node->entityIBP;
            if (!((*(uint32_t*)(entity + 0x40) >> 10) & 1)) {
                fn_InternalBasePtr_CheckPtr((void*)node);
                // Call tree node handler (inline for Update, calls vtable +0x40)
                EntityList_TreeNodeHandler(entity, time);
            }

            // Move to in-order successor
            node = TreeSuccessor(node);
        }
    }

    // Step 4: 5-bucket round-robin
    int64_t currentTime = self->accumulatedTime;

    for (int bucket = 0; bucket < 5; bucket++) {
        UpdateBucket* bkt = &self->buckets[bucket];
        int32_t count = bkt->count;
        if (count == 0) continue;

        uint32_t toProcess = self->processCount[bucket];
        uint32_t cursorVal = self->cursor[bucket];
        if ((int32_t)cursorVal >= count) cursorVal = 0;

        uint32_t startIdx = cursorVal;
        uint32_t idx = startIdx;
        uint32_t processed = 0;

        if ((int32_t)toProcess > 0) {
            do {
                // Re-read bucket data pointer each iteration (may be reallocated)
                int64_t* arrBase = bkt->data;

                // Read entity IBP and CheckPtr
                int64_t entIBP = arrBase[idx];
                fn_InternalBasePtr_CheckPtr(&entIBP);

                // Check not deleted (bit 10)
                if (!((*(uint32_t*)(entIBP + 0x40) >> 10) & 1)) {
                    fn_InternalBasePtr_CheckPtr(&entIBP);
                    int64_t delta = currentTime - *(int64_t*)(entIBP + 0x20);

                    if (delta > 0) {
                        fn_InternalBasePtr_CheckPtr(&entIBP);
                        int64_t ent = entIBP;

                        // Check entity has "needs bucket update" flag (bit 3)
                        // and is not deleted (bit 10)
                        if ((*(uint32_t*)(ent + 0x40) & 0x8) != 0 &&
                            (*(uint32_t*)(ent + 0x40) & 0x400) == 0) {

                            int32_t compCount = *(int32_t*)(ent + 0x38);
                            int64_t deltaTime[1] = { delta };

                            for (int64_t ci = 0; ci < compCount; ci++) {
                                // Read component IBP and CheckPtr
                                int64_t compIBP = *(int64_t*)(*(int64_t*)(ent + 0x30) + ci * 8);
                                fn_InternalBasePtr_CheckPtr(&compIBP);

                                // Component flags at byte +0x20, bit 3 = "needs Update tick"
                                // Verified from disassembly: MOV ECX,[RAX+0x20]; SHR ECX,3
                                if ((*(uint32_t*)(compIBP + 0x20) >> 3) & 1) {
                                    fn_InternalBasePtr_CheckPtr(&compIBP);

                                    // Call component vtable slot +0x48 (slot index 9 = Update)
                                    void** vtbl = *(void***)(uintptr_t)compIBP;
                                    typedef void (*CompUpdate_t)(void* self, void* time);
                                    CompUpdate_t compUpdate = (CompUpdate_t)vtbl[9];
                                    compUpdate((void*)(uintptr_t)compIBP, deltaTime);
                                }

                                // Reload self pointer (matches original: param_1 = local_res8)
                                // The entity list pointer is stored in GameUpdateLoop and
                                // component updates may cause reallocation. In the original,
                                // param_1 is reloaded from a local. We just re-read current time.
                                currentTime = self->accumulatedTime;

                                // Check if entity deleted during component update
                                if (*(uint32_t*)(ent + 0x40) & 0x400) break;
                            }
                        }

                        // Update entity timestamp
                        fn_InternalBasePtr_CheckPtr(&entIBP);
                        *(int64_t*)(entIBP + 0x20) = currentTime;
                    }
                }

                // Advance cursor with wrap-around
                idx++;
                processed++;
                cursorVal++;
                // Re-read count (array may have changed size)
                count = bkt->count;
                if ((int64_t)idx >= count) {
                    idx = 0;
                    cursorVal = 0;
                }
            } while (idx != startIdx && (int32_t)processed < (int32_t)toProcess);
        }

        // Save cursor position
        self->cursor[bucket] = cursorVal;
    }

    // Step 5: Process removal list
    // Original: if (param_1[0x1b] > 0) { PreUpdate; PostUpdateStaging; then remove each }
    int32_t removalCount = self->removalList.count;
    if (removalCount > 0) {
        if (s_fn_PreUpdate) s_fn_PreUpdate(self);
        if (s_fn_PostUpdateStaging) s_fn_PostUpdateStaging(self);

        // Process each removal
        for (int64_t i = 0; i < removalCount; i++) {
            void* entityIBP = (void*)(*(int64_t*)((uintptr_t)self->removalList.data + i * 8));
            EntityList_RemoveEntity(self, &entityIBP);
        }

        // Clear removal list
        fn_ExeArrayResize(&self->removalList, 0);
    }
}

// ============================================================
// EntityList::PreUpdate (0x1400c3bd0)
// ============================================================
void EntityList_PreUpdate(EntityList* self) {
    if (s_fn_PreUpdate) {
        s_fn_PreUpdate(self);
    }
}

// ============================================================
// EntityList::PostUpdate (0x1400c43d0)
// ============================================================
void EntityList_PostUpdate(EntityList* self) {
    if (s_fn_PostUpdate) {
        s_fn_PostUpdate(self);
    }
}

// ============================================================
// EntityList::RemoveEntity (0x1400c38e0)
// ============================================================
void EntityList_RemoveEntity(EntityList* self, void* entityIBP) {
    if (s_fn_RemoveEntity) {
        s_fn_RemoveEntity(self, entityIBP);
    }
}

// ============================================================
// EntityList::FlagAggregation (0x1400c3470)
// ============================================================
void EntityList_FlagAggregation(void* entity) {
    if (s_fn_FlagAggregation) {
        s_fn_FlagAggregation(entity);
    }
}

// ============================================================
// EntityList::Serialize (0x1400c44a0)
// ============================================================
void EntityList_Serialize(EntityList* self, void* stream) {
    if (s_fn_Serialize) {
        s_fn_Serialize(self, stream);
    }
}
