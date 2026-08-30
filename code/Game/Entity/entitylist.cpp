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
//   - EntityList::PreUpdate        (0x1400c3bd0) — deep reimpl
//   - EntityList::PostUpdate       (0x1400c43d0) — deep reimpl
//   - EntityList::TreeNodeHandler  (0x1400c2de0) — deep reimpl
//   - EntityList::PostUpdateNodeHandler (0x1400c2c70) — deep reimpl
//   - EntityList::PostUpdateStaging(0x1400c3ab0) — deep reimpl (static helper)
//   - EntityList::RemoveEntity     (0x1400c38e0) — deep reimpl
//   - EntityList::FlagAggregation  (0x1400c3470) — delegates to ComponentEntity_RefreshEntityFlags
//   - EntityList::Serialize        (0x1400c44a0) — deep reimpl
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Entity/entitylist_funcs.h"
#include "Entity/componententity_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (dying-entity dialog close)
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <windows.h>   // QueryPerformanceCounter for the BANISHED_SAVE_TIMING probe

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

// Forward declaration (non-static: address registered for standalone exe resolution)
void EntityList_PostUpdateStaging(EntityList* self);

// One-frame-deferred removal drain (relocated retail step-5 tail — see
// EntityList_Update Step 0). Defined below EntityList_Update, used at its top.
void EntityList_DrainRemovals(EntityList* self);

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
// Checks entity flag bit 4 (0x10) and calls component vtable +0x50
// on components that also have flag bit 4 set.
//
// Decompiled from 0x1400c2c70:
//   if ((entity+0x40 & 0x10) && !(entity+0x40 >> 10 & 1))
//     for each component:
//       if (comp+0x20 >> 4 & 1)  // component flag bit 4
//         call comp->vtable[+0x50](comp)
//       break if entity deleted (entity+0x40 & 0x400)
void EntityList_PostUpdateNodeHandler(int64_t entity) {
    uint32_t entityFlags = *(uint32_t*)(entity + 0x40);

    // Check entity has "needs post-update" flag (bit 4) and is not deleted (bit 10)
    if ((entityFlags & 0x10) == 0) return;
    if ((entityFlags >> 10) & 1) return;

    int32_t compCount = *(int32_t*)(entity + 0x38);
    int64_t ci = 0;

    if (compCount <= 0) return;

    do {
        // Read component IBP and CheckPtr
        void* compIBP = (void*)(*(uintptr_t*)(entity + 0x30) + ci * 8);
        fn_InternalBasePtr_CheckPtr(compIBP);
        int64_t comp = *(int64_t*)compIBP;

        // Component flags at +0x20, bit 4 = "needs post-update"
        uint32_t compFlags = *(uint32_t*)((uintptr_t)comp + 0x20);

        if ((compFlags >> 4) & 1) {
            // CheckPtr again before vtable call
            fn_InternalBasePtr_CheckPtr(compIBP);
            comp = *(int64_t*)compIBP;

            // Call component vtable slot +0x50 (slot index 10)
            void** vtbl = *(void***)(uintptr_t)comp;
            typedef void (*CompSlot10_t)(void*);
            ((CompSlot10_t)vtbl[10])((void*)(uintptr_t)comp);
        }

        ci++;

        // Break if entity was deleted during component processing
        if (*(uint32_t*)(entity + 0x40) & 0x400) break;

    } while (ci < compCount);
}

// ============================================================
// IBP BST insert/remove (fn_IBPTreeInsert 0x140021a30 / fn_IBPTreeRemove
// 0x14001a6e0) — standalone reimplementation
// ============================================================
// The originals are an AVL tree (the node carries balance/height at +0x20/+0x22).
// In standalone those exe addresses are unmapped, so fn_IBPTreeInsert/Remove
// resolve to NULL and crash the first Update tick once any entity has an
// update-capable component. The tree is only ever consumed by in-order walks
// (TreeMinimum/TreeSuccessor) and key lookups, so balancing is a performance
// optimization, not a correctness requirement: a plain BST with the identical
// node layout is functionally equivalent. The node key is the (CheckPtr-resolved)
// entity IBP value, matching the originals' comparison.
//
// The tree-root holder is {EntityTreeNode* root @ +0x00, int count @ +0x08} —
// i.e. &EntityList::updateTreeRoot (root +0x68, count +0x70).
namespace {
struct TreeHolder { EntityTreeNode* root; int64_t count; };

EntityTreeNode* IBPTree_AllocNode(uint64_t key, EntityTreeNode* parent) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    EntityTreeNode* n = (EntityTreeNode*)fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
    if (!n) return nullptr;
    n->entityIBP = key; n->parent = parent;
    n->left = nullptr; n->right = nullptr;
    n->balance = 0; n->height = 0; n->_pad24 = 0;
    return n;
}

// Splice `child` into `node`'s slot in the tree (CLRS transplant).
void IBPTree_Transplant(TreeHolder* h, EntityTreeNode* node, EntityTreeNode* child) {
    EntityTreeNode* p = node->parent;
    if (p == nullptr)        h->root = child;
    else if (p->left == node) p->left = child;
    else                      p->right = child;
    if (child) child->parent = p;
}
} // namespace

extern "C" void* IBPTreeInsert_CppImpl(void* treeRootPtr, void* outBuf, void* ibpPtr) {
    TreeHolder* h = (TreeHolder*)treeRootPtr;
    void** outNode = (void**)outBuf;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(ibpPtr);
    uint64_t key = *(uint64_t*)ibpPtr;
    if (h->root == nullptr) {
        EntityTreeNode* n = IBPTree_AllocNode(key, nullptr);
        h->root = n;
        if (outNode) *outNode = n;
    } else {
        EntityTreeNode* cur = h->root;
        for (;;) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(cur);
            if (key < cur->entityIBP) {
                if (cur->left == nullptr) {
                    EntityTreeNode* n = IBPTree_AllocNode(key, cur);
                    cur->left = n; if (outNode) *outNode = n; break;
                }
                cur = cur->left; continue;
            }
            if (key <= cur->entityIBP) {           // equal — already present
                if (outNode) *outNode = cur;
                return outBuf;                      // original returns w/o bumping count
            }
            if (cur->right == nullptr) {
                EntityTreeNode* n = IBPTree_AllocNode(key, cur);
                cur->right = n; if (outNode) *outNode = n; break;
            }
            cur = cur->right; continue;
        }
    }
    *(int32_t*)((uint8_t*)treeRootPtr + 8) = (int32_t)h->count + 1;
    return outBuf;
}

extern "C" void IBPTreeRemove_CppImpl(void* treeRootPtr, void* nodeV) {
    TreeHolder* h = (TreeHolder*)treeRootPtr;
    EntityTreeNode* node = (EntityTreeNode*)nodeV;
    if (!node) return;
    *(int32_t*)((uint8_t*)treeRootPtr + 8) = (int32_t)h->count - 1;
    if (node->left == nullptr) {
        IBPTree_Transplant(h, node, node->right);
    } else if (node->right == nullptr) {
        IBPTree_Transplant(h, node, node->left);
    } else {
        EntityTreeNode* succ = node->right;        // in-order successor
        while (succ->left) succ = succ->left;
        if (succ->parent != node) {
            IBPTree_Transplant(h, succ, succ->right);
            succ->right = node->right;
            succ->right->parent = succ;
        }
        IBPTree_Transplant(h, node, succ);
        succ->left = node->left;
        succ->left->parent = succ;
    }
    if (fn_MemFreeStatic) fn_MemFreeStatic(node);
}

// ============================================================
// Tree-walk corruption guard + diagnostics
// ============================================================
// The in-order walk (TreeMinimum/TreeSuccessor) intermittently faults reading
// a node's child/parent pointer through a HALF-CORRUPTED value — high dword a
// valid heap segment, low dword small garbage (e.g. 0x00000258_0000F1FA). See
// notes/project_entitylist_postupdate_treesuccessor_crash. Every real node is
// allocated 8-byte aligned (fn_MemAlloc(...,0x28,8)) with a heap address in the
// low-4GB..high range, so a non-null pointer that is MISALIGNED or has a set
// high dword with a tiny low dword cannot be a live node — it is corruption.
// We refuse to dereference it (converting the hard crash into a safe early
// stop) and record where it came from so the root cause can be pinned down.
// A non-null 64-bit value that cannot be a live heap pointer: misaligned, or
// a half-written pointer (valid high dword, tiny low dword — the freelist-link
// pattern left in a freed block, e.g. 0x000001AC_0000D661).
static inline bool PtrCorrupt(uintptr_t p) {
    if (p == 0) return false;                 // null = legitimate terminator / cleared slot
    if (p & 0x7) return true;                 // heap objects are 8-byte aligned
    if ((uint32_t)(p >> 32) != 0 && (uint32_t)p < 0x10000) return true; // half-written
    return false;
}

// A non-null pointer that cannot be a live tree node.
static inline bool NodeCorrupt(const EntityTreeNode* n) {
    return PtrCorrupt((uintptr_t)n);
}

// Set when a walk refuses a corrupt pointer; caller logs + continues.
static bool             g_treeCorrupt = false;
static const char*      g_treeCorruptSite = nullptr;   // which deref tripped
static EntityTreeNode*  g_treeCorruptFrom = nullptr;   // node holding the bad field
static uintptr_t        g_treeCorruptBad  = 0;         // the refused pointer value

static void FlagTreeCorrupt(const char* site, EntityTreeNode* from, const void* bad) {
    g_treeCorrupt = true;
    g_treeCorruptSite = site;
    g_treeCorruptFrom = from;
    g_treeCorruptBad = (uintptr_t)bad;
}

// Dump the captured corruption context. `from` is a live node whose child/parent
// field held the bad pointer — its full contents reveal whether the whole node
// was trashed (freed+reused) or just one field (partial write).
static void ReportTreeCorrupt(const char* walk, EntityList* self, EntityTreeNode* prev) {
    // Rate-limit: the same isolated corruption re-trips every frame. Log the
    // full context only when the (site, from-node, bad-ptr) tuple changes.
    static const char*     lastSite = nullptr;
    static EntityTreeNode*  lastFrom = nullptr;
    static uintptr_t        lastBad  = 0;
    if (g_treeCorruptSite == lastSite && g_treeCorruptFrom == lastFrom &&
        g_treeCorruptBad == lastBad) {
        return;
    }
    lastSite = g_treeCorruptSite; lastFrom = g_treeCorruptFrom; lastBad = g_treeCorruptBad;

    printf("[TreeGuard] CORRUPT node refused during %s walk: site=%s bad=%p\n",
           walk, g_treeCorruptSite ? g_treeCorruptSite : "?", (void*)g_treeCorruptBad);
    EntityTreeNode* f = g_treeCorruptFrom;
    if (f && !NodeCorrupt(f)) {
        printf("[TreeGuard]   from-node=%p  entityIBP=%p parent=%p left=%p right=%p bal=%d ht=%d\n",
               (void*)f, (void*)f->entityIBP, (void*)f->parent, (void*)f->left,
               (void*)f->right, (int)f->balance, (int)f->height);
    }
    if (prev && !NodeCorrupt(prev) && prev != f) {
        printf("[TreeGuard]   prev-node=%p  entityIBP=%p parent=%p left=%p right=%p\n",
               (void*)prev, (void*)prev->entityIBP, (void*)prev->parent,
               (void*)prev->left, (void*)prev->right);
    }
    printf("[TreeGuard]   postUpdateRoot=%p (cnt=%lld)  updateRoot=%p (cnt=%lld)\n",
           (void*)self->postUpdateTreeRoot, (long long)self->postUpdateTreeCount,
           (void*)self->updateTreeRoot, (long long)self->updateTreeCount);
    fflush(stdout);
}

// ============================================================
// Helper: In-order tree traversal (corruption-guarded)
// ============================================================
// Finds the leftmost node in the tree (minimum).
static EntityTreeNode* TreeMinimum(EntityTreeNode* node) {
    if (!node) return nullptr;
    for (;;) {
        EntityTreeNode* l = node->left;
        if (NodeCorrupt(l)) { FlagTreeCorrupt("min.left", node, l); return node; }
        if (!l) return node;
        node = l;
    }
}

// In-order successor of a node.
static EntityTreeNode* TreeSuccessor(EntityTreeNode* node) {
    if (!node) return nullptr;

    // If right child exists, successor is leftmost of right subtree
    EntityTreeNode* r = node->right;
    if (NodeCorrupt(r)) { FlagTreeCorrupt("succ.right", node, r); return nullptr; }
    if (r) {
        return TreeMinimum(r);
    }

    // Walk up until we find a node that is a left child of its parent
    EntityTreeNode* parent = node->parent;
    if (NodeCorrupt(parent)) { FlagTreeCorrupt("succ.parent", node, parent); return nullptr; }
    while (parent && parent->right == node) {
        node = parent;
        parent = node->parent;
        if (NodeCorrupt(parent)) { FlagTreeCorrupt("succ.up-parent", node, parent); return nullptr; }
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
    // Step 0 (SA DEVIATION — one-frame removal deferral): drain the removal list
    // that was BUILT LAST FRAME, at the TOP of this frame's update, instead of at
    // the bottom of the same frame it was built (retail drains at the end — the
    // 0x1400c4090 `param_1[0x1b]` block after the component loop).
    //
    // WHY: an entity is marked-for-removal (0x400) + pushed onto removalList during
    // its own component tick (destroy SM state-5 -> Destroy_MarkAndPushRemoval).
    // Retail relies on the open info dialog's STRONG datasource ref to keep the
    // entity's memory alive (marked-but-alive) until the dialog closes, so retail's
    // unguarded per-frame dialog slots (e.g. ResidenceUI::Slot7 0x1400B51F0) only
    // ever read valid memory. In standalone the dialog controllers don't hold that
    // strong ref, so an end-of-frame drain FREES the entity in the same frame it is
    // marked — and this frame's later UI pump (GameUpdateLoop step 9) + EditTool
    // tick (step ~984) then read freed memory (ACCESS_VIOLATION in IsDerivedFrom,
    // ResidenceUI_Slot7 <- freed entity, 2026-07-21).
    //
    // Deferring the free by one frame restores the "marked but alive" window every
    // SA_DialogEntityAlive guard and the EditTool close-on-entity-death path (Task
    // #1) depend on: frame N marks; frame N's UI pump + EditTool tick see 0x400 ->
    // guards bail and the tool closes the panel; frame N+1 drains + frees with no
    // dialog left pointing at it. Marked entities are inert meanwhile — every update
    // loop above already skips bit-10 (0x400) entities.
    EntityList_DrainRemovals(self);

    // Step 1: Accumulate time
    self->accumulatedTime += *(int64_t*)time;

    // Step 2: PreUpdate — process staging arrays into update tree
    EntityList_PreUpdate(self);

    // Step 3: In-order traversal of update tree (+0x68)
    EntityTreeNode* treeRoot = self->updateTreeRoot;
    if (treeRoot) {
        g_treeCorrupt = false;

        // Find leftmost node (in-order start)
        EntityTreeNode* node = TreeMinimum(treeRoot);
        EntityTreeNode* prev = nullptr;

        while (node && !g_treeCorrupt) {
            // CheckPtr on the node (it's an IBP)
            fn_InternalBasePtr_CheckPtr((void*)node);

            // Guard the node's entity-IBP key against a freelist link (UAF node).
            int64_t entity = (int64_t)node->entityIBP;
            if (PtrCorrupt((uintptr_t)entity)) {
                FlagTreeCorrupt("entityIBP", node, (void*)entity);
                break;
            }

            // Check entity not deleted (bit 10 of flags at entity+0x40)
            if (!((*(uint32_t*)(entity + 0x40) >> 10) & 1)) {
                fn_InternalBasePtr_CheckPtr((void*)node);
                // Call tree node handler (inline for Update, calls vtable +0x40)
                EntityList_TreeNodeHandler(entity, time);
            }

            // Move to in-order successor
            prev = node;
            node = TreeSuccessor(node);
        }

        if (g_treeCorrupt) ReportTreeCorrupt("Update", self, prev);
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

    // Step 5: removal drain DEFERRED to the TOP of the NEXT EntityList_Update (see
    // EntityList_DrainRemovals call + rationale at the head of this function). What
    // state-5 pushed onto removalList this frame stays marked-but-alive through this
    // frame's UI pump + EditTool tick, and is freed at the next frame's Step 0.
}

// The retail step-5 body (0x1400c4090 tail), relocated so it drains the PRIOR
// frame's removalList. Faithful otherwise: PreUpdate + PostUpdateStaging then
// RemoveEntity per pending IBP, then clear the list.
// Close any still-open info dialog whose data-source entity is marked-for-removal
// (0x400) this drain, BEFORE the entity is freed below. A PINNED info panel is driven
// by the UI pump, NOT by EditTool, so EditTool's close-on-death (edittool.cpp:544) can
// never reach it; next frame the panel would read its dangling +0x28, and because the
// freed entity's +0x40 no longer has the 0x400 bit it FALSE-POSITIVES the per-panel
// guard and dereferences freed memory. Confirmed crash 2026-07-22 (pin a house panel,
// demolish it -> ResidenceUI_Slot7 -> IsDerivedFrom(freed)). We close during the marked
// window (entity still ALIVE here — reading data-source+0x40 is safe) so the free frame
// has no dialog pointing at it. This is what retail's strong data-source ref achieves;
// the standalone dialog holds only a weak ref, so we close explicitly. Fixes every
// entity panel (pinned or not), not just ResidenceUI.
static void EntityList_CloseDialogsForDyingEntities() {
    void* gw = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gw || !fn_UI_GetFirstDialog || !fn_UI_CloseDialog || !fn_DialogInstance_GetController)
        return;
    void* ui = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0x98);
    if (!ui) return;

    // EntityUI is the shared frame controller on every entity info dialog; its +0x28 is
    // the data-source entity (raw .rdata type-id is null in standalone -> resolve by name).
    static uint32_t s_tidEntityUI = 0;
    if (s_tidEntityUI == 0 || s_tidEntityUI == 0xFFFFFFFFu)
        s_tidEntityUI = ::Object::GetTypeIdByName("EntityUI");
    if (s_tidEntityUI == 0 || s_tidEntityUI == 0xFFFFFFFFu) return;

    // Collect first, close after — fn_UI_CloseDialog mutates the list we're walking.
    void* toClose[16];
    int nClose = 0;

    uintptr_t nodeBuf[4] = {};
    fn_UI_GetFirstDialog(ui, nodeBuf);
    uintptr_t node = nodeBuf[0];
    while (node != 0 && nClose < 16) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)node);
        void* dialog = *reinterpret_cast<void**>(node);            // node[0] = DialogInstance*
        if (dialog) {
            uintptr_t ctrlBuf[2] = {};
            fn_DialogInstance_GetController(dialog, ctrlBuf, s_tidEntityUI);
            if (ctrlBuf[0] != 0) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ctrlBuf[0]);
                void* ctrl = reinterpret_cast<void*>(ctrlBuf[0]);
                void* ds = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(ctrl) + 0x28);
                if (ds && ((*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(ds) + 0x40) >> 10) & 1))
                    toClose[nClose++] = dialog;
            }
        }
        node = *reinterpret_cast<uintptr_t*>(node + 0x10);          // node+0x10 = next
    }

    for (int i = 0; i < nClose; i++) {
        void* dlgUI = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(toClose[i]) + 0x10);
        if (!dlgUI) dlgUI = ui;
        uint64_t dialogRef = reinterpret_cast<uint64_t>(toClose[i]);
        fn_UI_CloseDialog(dlgUI, &dialogRef);
        printf("[EntityList] closed info dialog %p on dying (marked-for-removal) entity\n",
               toClose[i]);
    }
}

// Sibling of the dialog-close above, same crash class (weak ref to a dying entity),
// same marked-window timing. The active world tool (EditTool) keeps the selected entity
// in an InternalBasePtr at +0x30 that its per-frame outline-follow (Impl_HighlightFollow,
// tool_base_render.cpp) dereferences. That ref is NOT generation-invalidated when the
// entity is freed, so the frame after a selected building is demolished the follow reads
// freed memory (Sub7080 -> GetTypeId on a garbage component ptr — confirmed crash on the
// LIVE_DESTROY path 2026-07-23). EditTool's own dead-ref cleanup keys off its dialog ref
// (+0x28) and also runs AFTER the follow, so it cannot save it. Clear the tool's selection
// here, during the marked window (entity still ALIVE — reading +0x40 is safe), so next
// frame there is nothing dangling to follow. Reaches the tool the same way the tool tick
// does: EntityUI == GameWorld+0x120, active-tool IBP at EntityUI+0x28 (EntityUI_ToolTick_SA).
static void EntityList_ClearToolSelectionForDyingEntities() {
    void* gw = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gw || !fn_IsDerivedFrom) return;
    void* entityUI = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0x120);
    if (!entityUI) return;
    void* toolSlot = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(entityUI) + 0x28);
    if (*reinterpret_cast<void**>(toolSlot) == nullptr) return;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(toolSlot);
    void* tool = *reinterpret_cast<void**>(toolSlot);
    if (!tool) return;

    static uint32_t s_tidEditTool = 0;
    if (s_tidEditTool == 0 || s_tidEditTool == 0xFFFFFFFFu)
        s_tidEditTool = ::Object::GetTypeIdByName("EditTool");
    if (s_tidEditTool == 0 || s_tidEditTool == 0xFFFFFFFFu) return;
    if (!fn_IsDerivedFrom(tool, s_tidEditTool)) return;

    // Selected-entity IBP at EditTool+0x30 (the outline-follow target).
    void* entSlot = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(tool) + 0x30);
    if (*reinterpret_cast<void**>(entSlot) == nullptr) return;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(entSlot);
    uintptr_t ent = *reinterpret_cast<uintptr_t*>(entSlot);
    if (ent < 0x10000 || ent > 0x00007FFFFFFFFFFFULL) return;
    if (((*reinterpret_cast<uint32_t*>(ent + 0x40) >> 10) & 1) == 0) return;

    // Drop both the dialog ref (+0x28) and the selected-entity ref (+0x30), exactly as
    // EditTool_CloseInfoDialog does — leaving +0x30 set would keep the follow chasing it.
    *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(tool) + 0x28) = 0;
    *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(tool) + 0x30) = 0;
    printf("[EntityList] cleared EditTool selection on dying (marked-for-removal) "
           "entity %p\n", reinterpret_cast<void*>(ent));
}

// ============================================================
// [diag] BANISHED_DRAIN_AUDIT — "why does a destroyed entity leave a LIVE node in
// Graphics::Scene?" (the open question from the wild-food seed-crash wave).
//
// Nothing in the free path removes scene geometry: RemoveEntity only unlinks the
// entity from the buckets/trees and DeletePtr's the last strong ref. The ONLY
// unlink is MarkForRemoval's (0x1400c2ea0) +0x38 broadcast, fired at MARK time —
// so any re-registration between the mark and the free ships a scene node that
// points into memory the drain is about to recycle.
//
// Two things are measured on every entity the drain is about to free:
//   markStillSet — is bit 0x400 still set at free time? A CLEARED bit means the
//                  entity was un-marked after the mark (NotifyEntityChanged
//                  0x1400c3470 ZEROES entity+0x40 and rebuilds it from the
//                  component flag words, which do not carry 0x400) and was
//                  therefore ticked again like a live entity.
//   liveModel    — a ModelComponent-derived component whose modelId (+0x16c) is
//                  >= 0, i.e. it is STILL registered in Graphics::Scene as the
//                  allocation goes back to the heap. This is the dangling node.
static bool DrainAudit() {
    static int on = -1;
    if (on < 0) { const char* e = std::getenv("BANISHED_DRAIN_AUDIT");
                  on = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return on == 1;
}

long g_drainFreed        = 0;   // entities freed by the drain
long g_drainUnmarked     = 0;   // ...whose 0x400 removal bit was gone again
long g_drainLiveModel    = 0;   // ...still holding a Graphics::Scene object id
long g_drainLiveUnmarked = 0;   // both at once (the crash shape)

extern "C" void EntityList_ReportDrainAudit(const char* tag) {
    std::fprintf(stderr,
                 "[drainAudit:%s] freed=%ld unmarked=%ld liveSceneNode=%ld both=%ld\n",
                 tag ? tag : "?", g_drainFreed, g_drainUnmarked, g_drainLiveModel,
                 g_drainLiveUnmarked);
    std::fflush(stderr);
}

// Audit + LAST-CHANCE SCENE UNLINK for one entity the drain is about to free.
//
// ROOT CAUSE this closes (measured 2026-08-14, PREREG A/B arm): an entity can be
// marked-for-removal BEFORE its own registration has run, so the mark-time +0x38
// broadcast finds modelId == -1 and unlinks nothing; the registration OnAdded pass
// (0x1400c2f60, component slot 6) then runs ModelComponent::Draw unconditionally —
// it has no notion of the 0x400 removal bit — and registers geometry that lives
// inside the allocation this drain is about to recycle. 44 of 109 freed entities in
// the repro arm were freed exactly like that; Graphics::Scene::Clean then walks a
// recycled node (RAX = two packed floats) and faults.
//
// Retail never needs a net here because it drains at the end of the same frame and
// (in the correct order) nothing registers after a mark. Our drain is already a
// deliberate SA deviation (see the one-frame deferral above), so the net lives here
// rather than as a guard inside a byte-faithful function: re-fire the component's
// own +0x38 handler, which is a no-op unless modelId >= 0 — i.e. unless something
// really did re-register after the mark.
static void AuditDyingEntity(int64_t entity) {
    if (entity < 0x10000) return;

    static uint32_t s_tidModel = 0;
    if (s_tidModel == 0 || s_tidModel == 0xFFFFFFFFu)
        s_tidModel = ::Object::GetTypeIdByName("ModelComponent");

    uint32_t flags   = *reinterpret_cast<uint32_t*>(entity + 0x40);
    bool markStillSet = ((flags >> 10) & 1) != 0;

    int models = 0, live = 0;
    int   n   = *reinterpret_cast<int*>(entity + 0x38);
    int64_t arr = *reinterpret_cast<int64_t*>(entity + 0x30);
    if (s_tidModel && s_tidModel != 0xFFFFFFFFu && fn_IsDerivedFrom) {
        for (int i = 0; i < n && arr; ++i) {
            int64_t c = *reinterpret_cast<int64_t*>(arr + (int64_t)i * 8);
            if (c < 0x10000) continue;
            if (!fn_IsDerivedFrom(reinterpret_cast<void*>(c), s_tidModel)) continue;
            ++models;
            if (*reinterpret_cast<int32_t*>(c + 0x16c) < 0) continue;
            ++live;
            // Unlink it (vtable +0x38 = the removed-from-world handler; on
            // ModelComponent that is Scene::RemoveObject + modelId = -1). Dispatched
            // virtually so derived overrides (NaturalResourceComponent) still apply.
            void** vt = *reinterpret_cast<void***>(c);
            if (vt && vt[7])
                reinterpret_cast<void(*)(void*)>(vt[7])(reinterpret_cast<void*>(c));
        }
    }

    ++g_drainFreed;
    if (!markStillSet) ++g_drainUnmarked;
    if (live)          ++g_drainLiveModel;
    if (live && !markStillSet) ++g_drainLiveUnmarked;

    if ((live || !markStillSet) && DrainAudit()) {
        static int s_lines = 0;
        if (++s_lines <= 30) {
            std::fprintf(stderr, "[drainAudit] entity=%p flags=0x%x mark0x400=%d "
                         "models=%d liveSceneNodes=%d (unlinked)\n",
                         reinterpret_cast<void*>(entity), flags, markStillSet ? 1 : 0,
                         models, live);
            std::fflush(stderr);
        }
    }
}

void EntityList_DrainRemovals(EntityList* self) {
    int32_t removalCount = self->removalList.count;
    if (removalCount <= 0) return;

    // Close dialogs + clear tool selection bound to the entities we're about to free
    // (see helpers above) — both are weak refs the free does not invalidate.
    EntityList_CloseDialogsForDyingEntities();
    EntityList_ClearToolSelectionForDyingEntities();

    EntityList_PreUpdate(self);
    EntityList_PostUpdateStaging(self);

    const bool audit = DrainAudit();
    for (int64_t i = 0; i < removalCount; i++) {
        void* entityIBP = (void*)(*(int64_t*)((uintptr_t)self->removalList.data + i * 8));
        AuditDyingEntity(reinterpret_cast<int64_t>(entityIBP));
        EntityList_RemoveEntity(self, &entityIBP);
    }

    fn_ExeArrayResize(&self->removalList, 0);

    if (audit) {
        static int s_summaries = 0;
        if (++s_summaries <= 200) EntityList_ReportDrainAudit("drain");
    }
}

// ============================================================
// EntityList::PreUpdate (0x1400c3bd0)
// ============================================================
// Processes the staging update array (+0x88/+0x90):
// For each staged entity IBP, checks if entity has flag bit 2 set and is not deleted.
//   - If yes: inserts into update tree (+0x68) via BST insert
//   - If no: searches update tree for the entity and removes it via BST remove
// Then clears the staging array.
//
// Decompiled from 0x1400c3bd0.
void EntityList_PreUpdate(EntityList* self) {
    int32_t stagingCount = self->stagingUpdate.count;
    if (stagingCount <= 0) {
        fn_ExeArrayResize(&self->stagingUpdate, 0);
        return;
    }

    for (int i = 0; i < stagingCount; i++) {
        int64_t offset = (int64_t)i * 8;
        void* ibpPtr = (void*)((uintptr_t)self->stagingUpdate.data + offset);

        fn_InternalBasePtr_CheckPtr(ibpPtr);
        int64_t entityVal = *(int64_t*)ibpPtr;

        uint32_t entityFlags = *(uint32_t*)(entityVal + 0x40);
        bool hasUpdateFlag = (entityFlags & 0x4) != 0;
        bool isDeleted = ((entityFlags >> 10) & 1) != 0;

        if (hasUpdateFlag && !isDeleted) {
            // Insert into update tree
            uint64_t outBuf = 0;
            void* srcIBP = (void*)((uintptr_t)self->stagingUpdate.data + offset);
            fn_IBPTreeInsert(&self->updateTreeRoot, &outBuf, srcIBP);
        } else {
            // Search for entity in update tree, remove if found
            EntityTreeNode* node = self->updateTreeRoot;
            void* ibpSrc = (void*)((uintptr_t)self->stagingUpdate.data + offset);

            while (node != nullptr) {
                fn_InternalBasePtr_CheckPtr(node);
                uint64_t ibpVal = *(uint64_t*)ibpSrc;

                if (ibpVal < node->entityIBP) {
                    node = node->left;
                } else {
                    fn_InternalBasePtr_CheckPtr(ibpSrc);
                    if (ibpVal <= node->entityIBP) {
                        // Found — remove from tree
                        fn_IBPTreeRemove(&self->updateTreeRoot, node);
                        break;
                    }
                    node = node->right;
                }
            }
        }
    }

    fn_ExeArrayResize(&self->stagingUpdate, 0);
}

// ============================================================
// EntityList::PostUpdateStaging (0x1400c3ab0) — static helper
// ============================================================
// Same pattern as PreUpdate but for the post-update tree (+0x78)
// and staging array (+0x98/+0xa0), checking flag bit 4 (0x10).
//
// Decompiled from 0x1400c3ab0.
void EntityList_PostUpdateStaging(EntityList* self) {
    int32_t stagingCount = self->stagingPostUpdate.count;
    if (stagingCount <= 0) {
        fn_ExeArrayResize(&self->stagingPostUpdate, 0);
        return;
    }

    for (int i = 0; i < stagingCount; i++) {
        int64_t offset = (int64_t)i * 8;
        void* ibpPtr = (void*)((uintptr_t)self->stagingPostUpdate.data + offset);

        fn_InternalBasePtr_CheckPtr(ibpPtr);
        int64_t entityVal = *(int64_t*)ibpPtr;

        uint32_t entityFlags = *(uint32_t*)(entityVal + 0x40);
        bool hasPostUpdateFlag = (entityFlags & 0x10) != 0;
        bool isDeleted = ((entityFlags >> 10) & 1) != 0;

        if (hasPostUpdateFlag && !isDeleted) {
            // Insert into post-update tree
            uint64_t outBuf = 0;
            void* srcIBP = (void*)((uintptr_t)self->stagingPostUpdate.data + offset);
            fn_IBPTreeInsert(&self->postUpdateTreeRoot, &outBuf, srcIBP);
        } else {
            // Search for entity in post-update tree, remove if found
            EntityTreeNode* node = self->postUpdateTreeRoot;
            void* ibpSrc = (void*)((uintptr_t)self->stagingPostUpdate.data + offset);

            while (node != nullptr) {
                fn_InternalBasePtr_CheckPtr(node);
                uint64_t ibpVal = *(uint64_t*)ibpSrc;

                if (ibpVal < node->entityIBP) {
                    node = node->left;
                } else {
                    fn_InternalBasePtr_CheckPtr(ibpSrc);
                    if (ibpVal <= node->entityIBP) {
                        // Found — remove from tree
                        fn_IBPTreeRemove(&self->postUpdateTreeRoot, node);
                        break;
                    }
                    node = node->right;
                }
            }
        }
    }

    fn_ExeArrayResize(&self->stagingPostUpdate, 0);
}

// ============================================================
// EntityList::PostUpdate (0x1400c43d0)
// ============================================================
// Processes post-update staging array, then does in-order tree walk
// of the post-update tree (+0x78), calling PostUpdateNodeHandler per entity.
//
// Decompiled from 0x1400c43d0.
void EntityList_PostUpdate(EntityList* self) {
    // Step 1: Process staging post-update array
    EntityList_PostUpdateStaging(self);

    // Step 2: In-order traversal of post-update tree (+0x78)
    EntityTreeNode* treeRoot = self->postUpdateTreeRoot;
    if (!treeRoot) return;

    g_treeCorrupt = false;

    // Find leftmost node (in-order start)
    EntityTreeNode* node = TreeMinimum(treeRoot);
    EntityTreeNode* prev = nullptr;

    while (node && !g_treeCorrupt) {
        // CheckPtr on the node
        fn_InternalBasePtr_CheckPtr((void*)node);

        // Guard the node's entity-IBP key: a freed-but-still-linked node has a
        // freelist link written over +0x00, so dereferencing it faults.
        int64_t entity = (int64_t)node->entityIBP;
        if (PtrCorrupt((uintptr_t)entity)) {
            FlagTreeCorrupt("entityIBP", node, (void*)entity);
            break;
        }

        // Check entity not deleted (bit 10 of flags at entity+0x40)
        if (!((*(uint32_t*)(entity + 0x40) >> 10) & 1)) {
            fn_InternalBasePtr_CheckPtr((void*)node);
            EntityList_PostUpdateNodeHandler(entity);
        }

        // Move to in-order successor
        prev = node;
        node = TreeSuccessor(node);
    }

    if (g_treeCorrupt) ReportTreeCorrupt("PostUpdate", self, prev);
}

// ============================================================
// EntityList::RemoveEntity (0x1400c38e0)
// ============================================================
// Removes an entity from its bucket, the master array, and both trees.
// Also calls InternalBasePtr::DeletePtr to release the entity reference.
//
// Entity metadata layout (on the entity itself):
//   +0x18: int32 — index in master array
//   +0x1C: packed field — low 24 bits = index in bucket, high 8 bits include bucket ID
//   +0x1F: int8 — bucket index (0-5, where 6 means "not in any bucket")
//
// Decompiled from 0x1400c38e0.
void EntityList_RemoveEntity(EntityList* self, void* entityIBP) {
    fn_InternalBasePtr_CheckPtr(entityIBP);
    int64_t entity = *(int64_t*)entityIBP;

    // Diagnostic (BANISHED_DESTROY_TRACE): the final DeletePtr below is what drops
    // the last strong ref and frees the entity + its component block. Log the block
    // so a post-crash fault address can be matched against the freed range
    // (compCount/compArray give the component-block neighborhood).
    if (std::getenv("BANISHED_DESTROY_TRACE")) {
        std::fprintf(stderr, "[EntityDrain] freeing entity=%p compArray=%p compCount=%d\n",
                     (void*)entity, *(void**)(entity + 0x30), *(int*)(entity + 0x38));
        std::fflush(stderr);
    }

    // Read bucket index from entity+0x1F
    int8_t bucketIdx = *(int8_t*)(entity + 0x1F);

    // Step 1: Remove from bucket (if in one; bucket 6 means "none")
    if (bucketIdx != 6) {
        // Get bucket array pointer: self + 0x18 + bucketIdx * 0x10
        uintptr_t bucketBase = (uintptr_t)self + 0x18 + (int64_t)bucketIdx * 0x10;

        // Last element index in bucket
        uint32_t lastIdx = *(int32_t*)(bucketBase + 0x08) - 1;

        fn_InternalBasePtr_CheckPtr(entityIBP);
        entity = *(int64_t*)entityIBP;

        // Entity's index within bucket (low 24 bits of +0x1C)
        uint32_t entityBucketIdx = (*(uint32_t*)(entity + 0x1C) << 8) >> 8;

        if (lastIdx != entityBucketIdx) {
            // Swap last element into removed element's slot
            int64_t* bucketData = *(int64_t**)bucketBase;
            *(int64_t*)(bucketData + entityBucketIdx) = *(int64_t*)(bucketData + lastIdx);

            // Update swapped entity's bucket index field
            void* swappedIBP = (void*)(*(int64_t**)bucketBase + entityBucketIdx);
            fn_InternalBasePtr_CheckPtr(swappedIBP);
            int64_t swappedEntity = *(int64_t*)swappedIBP;

            // Update low 24 bits of +0x1C with new index
            uint32_t* packedField = (uint32_t*)(swappedEntity + 0x1C);
            *packedField = *packedField ^ ((*(uint32_t*)(swappedEntity + 0x1C) ^ entityBucketIdx) & 0xFFFFFF);
        }

        // Shrink bucket array
        fn_ExeArrayResize((void*)bucketBase, lastIdx);
    }

    // Step 2: Remove from master array (+0x08)
    int32_t lastMasterIdx = self->masterArray.count - 1;

    fn_InternalBasePtr_CheckPtr(entityIBP);
    entity = *(int64_t*)entityIBP;
    int32_t entityMasterIdx = *(int32_t*)(entity + 0x18);

    if (lastMasterIdx != entityMasterIdx) {
        // Swap last element into removed element's slot
        int64_t* masterData = (int64_t*)self->masterArray.data;
        masterData[entityMasterIdx] = masterData[lastMasterIdx];

        // Update swapped entity's master array index
        void* swappedIBP = (void*)&masterData[entityMasterIdx];
        fn_InternalBasePtr_CheckPtr(swappedIBP);
        *(int32_t*)(*(int64_t*)swappedIBP + 0x18) = entityMasterIdx;
    }

    // Shrink master array
    fn_ExeArrayResize(&self->masterArray, lastMasterIdx);

    // Step 3: Search and remove from update tree (+0x68)
    EntityTreeNode* updateNode = self->updateTreeRoot;
    while (updateNode != nullptr) {
        fn_InternalBasePtr_CheckPtr(updateNode);
        if (*(uint64_t*)entityIBP < updateNode->entityIBP) {
            updateNode = updateNode->left;
        } else {
            fn_InternalBasePtr_CheckPtr(entityIBP);
            if (*(uint64_t*)entityIBP <= updateNode->entityIBP) {
                break;  // Found
            }
            updateNode = updateNode->right;
        }
    }

    // Step 4: Search and remove from post-update tree (+0x78)
    EntityTreeNode* postUpdateNode = self->postUpdateTreeRoot;
    while (postUpdateNode != nullptr) {
        fn_InternalBasePtr_CheckPtr(postUpdateNode);
        if (*(uint64_t*)entityIBP < postUpdateNode->entityIBP) {
            postUpdateNode = postUpdateNode->left;
        } else {
            fn_InternalBasePtr_CheckPtr(entityIBP);
            if (*(uint64_t*)entityIBP <= postUpdateNode->entityIBP) {
                break;  // Found
            }
            postUpdateNode = postUpdateNode->right;
        }
    }

    // Remove found nodes from trees
    if (updateNode != nullptr) {
        fn_IBPTreeRemove(&self->updateTreeRoot, updateNode);
    }
    if (postUpdateNode != nullptr) {
        fn_IBPTreeRemove(&self->postUpdateTreeRoot, postUpdateNode);
    }

    // Step 5: Delete the entity IBP reference
    fn_InternalBasePtr_DeletePtr(entityIBP);
}

// ============================================================
// EntityList::FlagAggregation (0x1400c3470)
// ============================================================
// This function at 0x1400c3470 is the SAME function as
// ComponentEntity::RefreshEntityFlags, already reimplemented
// in code/Game/Entity/componententity.cpp.
// Delegates to that implementation.
void EntityList_FlagAggregation(void* entity) {
    ComponentEntity_RefreshEntityFlags(reinterpret_cast<ComponentEntity*>(entity));
}

// ============================================================
// EntityList::Serialize (0x1400c44a0)
// ============================================================
// Save/load the entity list. Steps:
//   1. Construct Serializer from stream
//   2. Flush PreUpdate and PostUpdateStaging
//   3. On save: process removal list
//   4. Serialize time (System::Time::Serialize)
//   5. Serialize master array count, then each IBP
//   6. After load: verify each IBP is a ComponentEntity (clear if not)
//   7. Serialize bucket data via helper
//   8. Serialize processCount[5] and cursor[5] arrays
//   9. Per-entity metadata: master index, bucket index, bucket ID, time
//
// Decompiled from 0x1400c44a0.
// [diag] BANISHED_SAVE_TIMING - EntityList::Serialize owns ~99% of the save
// snapshot freeze, so break its phases apart: removal drain, the per-entity
// InternalBasePtr::Serialize loop, the bucket trees, and the metadata loop.
static bool ElSaveTimingOn() {
    static int on = -1;
    if (on < 0) { const char* e = std::getenv("BANISHED_SAVE_TIMING");
                  on = (e && *e && e[0] != 0x30) ? 1 : 0; }
    return on != 0;
}
static long long ElQpc() { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; }
static double ElQpcMs(long long d) {
    static long long f = 0;
    if (!f) { LARGE_INTEGER q; QueryPerformanceFrequency(&q); f = q.QuadPart; }
    return f ? 1000.0 * (double)d / (double)f : 0.0;
}

void EntityList_Serialize(EntityList* self, void* stream) {
    // Step 1: Construct Serializer (local buffer, 0x18 bytes)
    uint8_t serializerBuf[0x18];
    void* serializer = serializerBuf;
    g_fn_Serializer_Ctor(serializer, stream, 0);
    const bool elTime = ElSaveTimingOn();
    long long elT0 = elTime ? ElQpc() : 0;
    long long elMark = elT0;

    // Step 2: Flush staging arrays
    EntityList_PreUpdate(self);
    EntityList_PostUpdateStaging(self);

    // Step 3: On save (stream+0x08 == 1), process removal list
    if (*(int32_t*)((uintptr_t)stream + 0x08) == 1) {
        int32_t removalCount = self->removalList.count;
        if (removalCount > 0) {
            for (int64_t i = 0; i < removalCount; i++) {
                uint64_t ibpVal = *((uint64_t*)self->removalList.data + i);
                uint64_t localIBP = ibpVal;
                EntityList_RemoveEntity(self, &localIBP);
            }
            fn_ExeArrayResize(&self->removalList, 0);
        }
    }

    // Step 4: Serialize time
    if (elTime) { fprintf(stderr, "[ElTiming] removal-drain %8.2fms\n",
                          ElQpcMs(ElQpc() - elMark)); fflush(stderr); elMark = ElQpc(); }
    g_fn_Time_Serialize(self, serializer);

    // Step 5: Serialize master array count
    int32_t masterCount = self->masterArray.count;
    int32_t serializedCount = masterCount;
    g_fn_Serialize_Int(serializer, &serializedCount, nullptr);

    // On load (mode == 0): resize master array to deserialized count
    void* serializerInner = *(void**)serializer;
    if (*(int32_t*)((uintptr_t)serializerInner + 0x08) == 0) {
        fn_ExeArrayResize(&self->masterArray, serializedCount);
    }

    // Serialize each entity IBP in master array
    void* ibpIter = self->masterArray.data;
    int64_t ibpCount = (int64_t)serializedCount;
    // The master loop is a plausible suspect whenever a PersistObject call made
    // DIRECTLY from here reads an all-zero record: that is either a component
    // under-read leaving the stream short, or this loop simply running past the
    // last entity. Printing the count and the index the loop reaches separates
    // the two without any byte forensics.
    if (elTime) {
        fprintf(stderr, "[ElLoop] masterCount(before)=%d serializedCount=%d data=%p\n",
            masterCount, serializedCount, ibpIter);
    fflush(stderr);
    }
    if (ibpCount > 0) {
        long long elBucketT0 = elTime ? ElQpc() : 0;
        for (int64_t j = 0; j < ibpCount; j++) {
            void* currentIBP = (void*)((uintptr_t)ibpIter + j * 8);
            if (elTime && j + 8 >= ibpCount) {
                fprintf(stderr, "[ElLoop] entity j=%lld of %lld\n",
                        (long long)j, (long long)ibpCount);
                fflush(stderr);
            }
            fn_InternalBasePtr_Serialize(currentIBP, serializer);
            // [diag] per-1000 slice: a FLAT cost means each entity is simply
            // expensive; a RISING cost means something behind
            // InternalBasePtr::Serialize scans a table that grows with every
            // object already written (O(n^2)).
            if (elTime && ((j + 1) % 1000 == 0 || j + 1 == ibpCount)) {
                long long now = ElQpc();
                fprintf(stderr, "[ElTiming]   ibp[%5d..%5d] %8.2fms\n",
                        (int)(j / 1000) * 1000, (int)j, ElQpcMs(now - elBucketT0));
                fflush(stderr);
                elBucketT0 = now;
            }

            // After loading, verify it's a ComponentEntity
            if (*(void**)currentIBP != nullptr) {
                bool isDerived = fn_IsDerivedFrom(*(void**)currentIBP, *g_TypeId_ComponentEntity);
                if (!isDerived) {
                    *(uint64_t*)currentIBP = 0;
                }
            }
        }
    }

    // Step 6: Serialize bucket trees via helper (5 buckets starting at +0x18)
    if (elTime) { fprintf(stderr, "[ElTiming] ibp-loop      %8.2fms  entities=%d\n",
                          ElQpcMs(ElQpc() - elMark), (int)serializedCount);
                  fflush(stderr); elMark = ElQpc(); }
    fn_BucketTreeSerialize(serializer, nullptr, &self->buckets[0]);

    // Step 7: Serialize processCount[5] and cursor[5] as bulk arrays
    if (elTime) { fprintf(stderr, "[ElTiming] bucket-trees  %8.2fms\n",
                          ElQpcMs(ElQpc() - elMark)); fflush(stderr); elMark = ElQpc(); }
    fn_Serializer_SerializeBulk(serializer, 5, self->processCount, nullptr);
    fn_Serializer_SerializeBulk(serializer, 5, self->cursor, nullptr);

    // Step 8: Per-entity metadata serialization
    int32_t entityIdx = 0;
    int64_t ibpOffset = 0;
    if (self->masterArray.count > 0) {
        do {
            uintptr_t masterData = (uintptr_t)self->masterArray.data;

            // Read master array index from entity+0x18
            fn_InternalBasePtr_CheckPtr((void*)(masterData + ibpOffset));
            int32_t masterIdxField = *(int32_t*)(*(int64_t*)(masterData + ibpOffset) + 0x18);

            // Read bucket index (low 24 bits of entity+0x1C)
            masterData = (uintptr_t)self->masterArray.data;
            fn_InternalBasePtr_CheckPtr((void*)(masterData + ibpOffset));
            uint32_t bucketIdxField = (*(uint32_t*)(*(int64_t*)(masterData + ibpOffset) + 0x1C) << 8) >> 8;

            // Read bucket ID from entity+0x1F
            masterData = (uintptr_t)self->masterArray.data;
            fn_InternalBasePtr_CheckPtr((void*)(masterData + ibpOffset));
            int32_t bucketId = (int32_t)*(int8_t*)(*(int64_t*)(masterData + ibpOffset) + 0x1F);

            // Serialize the three metadata fields
            g_fn_Serialize_Int(serializer, &masterIdxField, nullptr);
            g_fn_Serialize_Int(serializer, (int32_t*)&bucketIdxField, nullptr);
            g_fn_Serialize_Int(serializer, &bucketId, nullptr);

            // Serialize entity's timestamp at +0x20 (System::Time)
            masterData = (uintptr_t)self->masterArray.data;
            fn_InternalBasePtr_CheckPtr((void*)(masterData + ibpOffset));
            g_fn_Time_Serialize((void*)(*(int64_t*)(masterData + ibpOffset) + 0x20), serializer);

            // Write back deserialized metadata to entity
            masterData = (uintptr_t)self->masterArray.data;
            fn_InternalBasePtr_CheckPtr((void*)(masterData + ibpOffset));
            *(int32_t*)(*(int64_t*)(masterData + ibpOffset) + 0x18) = masterIdxField;

            masterData = (uintptr_t)self->masterArray.data;
            fn_InternalBasePtr_CheckPtr((void*)(masterData + ibpOffset));
            int64_t ent = *(int64_t*)(masterData + ibpOffset);
            uint32_t* packedField = (uint32_t*)(ent + 0x1C);
            *packedField = *packedField ^ ((bucketIdxField ^ *(uint32_t*)(ent + 0x1C)) & 0xFFFFFF);

            masterData = (uintptr_t)self->masterArray.data;
            fn_InternalBasePtr_CheckPtr((void*)(masterData + ibpOffset));
            *(int8_t*)(*(int64_t*)(masterData + ibpOffset) + 0x1F) = (int8_t)bucketId;

            entityIdx++;
            ibpOffset += 8;
        } while (entityIdx < self->masterArray.count);
    }

    // Step 9: Destroy Serializer
    if (elTime) { fprintf(stderr, "[ElTiming] metadata-loop %8.2fms  TOTAL %8.2fms\n",
                          ElQpcMs(ElQpc() - elMark), ElQpcMs(ElQpc() - elT0)); fflush(stderr); }
    g_fn_Serializer_Dtor(serializer);
}
