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
#include "Component/coll_bridge.h"
#include "ui/UserInterface.h"
#include "options.h"
#include "perf_experimental.h"   // [PerfX] TREE_SNAPSHOT walk
#include "sampler.h"
#include <vector>
#include "Entity/componententity_funcs.h"
#include "Entity/ComponentEntity.h"
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // caller batch CB6: the world's named fields
#include "resolve.h"
#include "Dialog/EntityUI.h"             // caller batch CB6: TypeId()
#include "Player/EditTool.h"
#include "Component/ModelComponent.h"
#include "stress_harness.h"               // g_compProfOn / CompProf_AddVtbl (BANISHED_STRESS)
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <windows.h>   // QueryPerformanceCounter for the BANISHED_SAVE_TIMING probe
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include "io/Stream.h"      // ENDGAME B.3.4 callers batch H: IO::Stream::GetDirection
#include "system/Value.h"   // ENDGAME B.3.4b System callers: System::Time
#include "runtime/types/ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K4)
#include <new>
#include <new>

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
// [STRESS] EntityList_Update sub-phase accumulators (QPC ticks), read + reset by the
// stress harness's COMPPROF report. Only touched when g_compProfOn.
long long g_elProfDrainTicks = 0, g_elProfPreTicks = 0, g_elProfTreeTicks = 0, g_elProfBucketTicks = 0;
long long g_elProfTreeNodes = 0, g_elProfBucketVisits = 0, g_elProfBucketTicked = 0;
extern "C" int  CompProf_BeginKind(void* comp, int kind);
extern "C" void CompProf_End(int slot, long long qpcTicks);

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
        int64_t comp = *(int64_t*)compIBP;

        // Component flags at byte offset +0x20 from component base.
        // Verified from disassembly at 0x1400c2e42: MOV ECX,dword ptr [RAX + 0x20]
        // Then SHR ECX,0x2; TEST CL,0x1 checks bit 2.
        uint32_t compFlags = *(uint32_t*)((uintptr_t)comp + 0x20);

        if ((compFlags >> 2) & 1) {
            // CheckPtr again before vtable call
            comp = *(int64_t*)compIBP;

            // Component slot 8 (+0x40) Tick, by name.
            Component* c = reinterpret_cast<Component*>(comp);   // B.3.x bridge
            if (g_compProfOn >= 2) {
                // [STRESS] BANISHED_STRESS_COMPPROF=2: attribute the per-frame (slot 8) tick
                // per component type, separately from the slot-9 bucket Update (kind 1 =
                // "/tree"). Opt-in: ~30k QPC pairs per frame at 800x10 (~+0.8 ms/frame).
                const int slot = CompProf_BeginKind((void*)(uintptr_t)comp, 1);
                const long long q0 = CompProf_Now();
                c->Tick(time);
                CompProf_End(slot, CompProf_Now() - q0);
            } else {
                c->Tick(time);
            }
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
        int64_t comp = *(int64_t*)compIBP;

        // Component flags at +0x20, bit 4 = "needs post-update"
        uint32_t compFlags = *(uint32_t*)((uintptr_t)comp + 0x20);

        if ((compFlags >> 4) & 1) {
            // CheckPtr again before vtable call
            comp = *(int64_t*)compIBP;

            // Component slot 10 (+0x50) PostUpdate, by name.
            reinterpret_cast<Component*>(comp)->PostUpdate();   // B.3.x bridge
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
    uint64_t key = *(uint64_t*)ibpPtr;
    if (h->root == nullptr) {
        EntityTreeNode* n = IBPTree_AllocNode(key, nullptr);
        h->root = n;
        if (outNode) *outNode = n;
    } else {
        EntityTreeNode* cur = h->root;
        for (;;) {
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
// PERF A/B (2026-09-04): BANISHED_TREE_GUARDS=0 turns the corruption guards into no-ops
// so their walk cost can be measured against the faithful (unguarded) retail walk.
static const bool g_treeGuards = [] {
    const char* e = Opt().treeGuards;
    return !(e && e[0] == '0');
}();
static inline bool PtrCorrupt(uintptr_t p) {
    if (!g_treeGuards) return false;
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
// PERF A/B: successor prefetch in the update-tree walk. OFF by default — measured
// 2026-09-04 at 800x10: no prefetch 0.25 ms/iter, one level 0.26, two levels 0.27; the
// extra TreeSuccessor walk costs more than the hidden miss. BANISHED_TREE_PREFETCH=1 enables.
static const bool       g_treePrefetch = [] {
    const char* e = Opt().treePrefetch;
    return e && e[0] == '1';
}();
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
// Guard for the STAGING search/remove path (PreUpdate/PostUpdate staging loops).
//
// The always-on [TreeGuard] above covers the in-order WALK (TreeMinimum /
// TreeSuccessor / the walk body's entityIBP key). It does NOT cover the staging
// loops' own key-search, nor the AVL delete they call
// (fn_IBPTreeRemove -> WorkBST_BSTRemove_a6e0 -> SimpleBST_Retrace, in
// code/Game/Work/work_base_bst.cpp — a faithful 1:1 port of 0x14001a6e0 that
// dereferences node->parent unguarded, exactly as retail does).
//
// So the SAME corruption the walk guard was built for still crashed here
// (2026-09-04, tutorial run): ACCESS_VIOLATION reading 0x00000235_00006D1C — the
// documented half-written signature, high dword a live heap segment, low dword a
// small cell-index-like value, landing on a node's +0x08 parent field. Stack was
// SimpleBST_Retrace <- WorkBST_BSTRemove_a6e0 <- EntityList_PostUpdateStaging.
// See memory project_entitylist_postupdate_treesuccessor_crash: that writer was
// root-caused (async pathfinder result commit, fixed 17g) and this is either a
// second writer or an uncovered path for the same class.
//
// Refusing the remove here is NOT masking a faithful retail crash: retail never
// has the corrupt pointer. It converts a run-ending AV into a logged event that
// carries the forensics needed to name the writer, and it is the same
// defense-in-depth policy already applied to the walk. BANISHED_TREE_GUARDS=0
// still turns it off for a faithful A/B.
//
// Returns true when the node is safe to hand to the AVL delete.
static bool StagingNodeRemovable(EntityTreeNode* node) {
    if (NodeCorrupt(node)) return false;
    if (PtrCorrupt((uintptr_t)node->parent)) return false;
    if (PtrCorrupt((uintptr_t)node->left))   return false;
    if (PtrCorrupt((uintptr_t)node->right))  return false;
    if (PtrCorrupt((uintptr_t)node->entityIBP)) return false;
    return true;
}

static void ReportStagingCorrupt(const char* which, EntityList* self, EntityTreeNode* node) {
    // Rate-limited on the node identity: an isolated corrupt node re-trips every
    // frame until something unlinks it, and an unbuffered flood is its own bug.
    static EntityTreeNode* lastNode = nullptr;
    if (node == lastNode) return;
    lastNode = node;

    printf("[TreeGuard] CORRUPT node refused during %s staging REMOVE: node=%p\n", which, (void*)node);
    if (node && !NodeCorrupt(node)) {
        printf("[TreeGuard]   node entityIBP=%p parent=%p left=%p right=%p bal=%d ht=%d\n",
               (void*)node->entityIBP, (void*)node->parent, (void*)node->left,
               (void*)node->right, (int)node->balance, (int)node->height);
    }
    printf("[TreeGuard]   postUpdateRoot=%p (cnt=%lld)  updateRoot=%p (cnt=%lld)\n",
           (void*)self->postUpdateTreeRoot, (long long)self->postUpdateTreeCount,
           (void*)self->updateTreeRoot, (long long)self->updateTreeCount);
    fflush(stdout);
}

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

    // BANISHED_TREE_CATCH=1 — name the WRITER with a hardware write watchpoint.
    //
    // The corruption is always the same shape: a fully live node (valid entityIBP,
    // valid children, sane balance/height, real parent linking down to it) whose
    // parent field at +0x08 has had only its LOW 32 BITS overwritten with a small
    // value (0x096d, 0x3ccb, 0x41f8, 0x6d63, 0x6d7f, 0x6d86 …) while the high dword
    // keeps the live heap segment. So it is an EXTERNAL 32-bit store landing on
    // node+0x08, not anything entitylist.cpp does — our tree code only ever writes
    // that field as a full 64-bit pointer.
    //
    // One writer of this class was root-caused and fixed in July (async pathfinder
    // result commit, see memory project_entitylist_postupdate_treesuccessor_crash).
    // This is a second one, and it is not confined to tree nodes: the 2026-09-04
    // tutorial crash was UI::Element::Enable walking a child whose vtable had been
    // zeroed, with RCX=0x00000171_000045F6 — the identical half-written signature on
    // a UI object.
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
// ============================================================
// [PerfX TREE_SNAPSHOT -- NON-RETAIL data structure, IDENTICAL visit order]
// ============================================================
// The update tree is walked in order once per speed iteration (10x per frame at 10x)
// and is a plain BST of heap-pointer keys -- a pointer chase whose only product is the
// in-order entity sequence. That sequence changes only when a node is inserted or
// removed (PreUpdate / DrainRemovals, both OUTSIDE the walk: entities created during
// a tick land in the staging arrays, never directly in the tree), so it is cached as a
// flat array and rebuilt when either mutation site marks it dirty or the tree's node
// count no longer matches. The flat walk also prefetches the next entities' headers
// and component arrays. Kill-switch: BANISHED_PERF_TREE_SNAPSHOT=0.
static std::vector<int64_t> s_walkSnap;
static bool    s_walkDirty = true;
static int64_t s_walkCountAtBuild = -1;
static const EntityList* s_walkOwner = nullptr;
extern "C" void EntityList_MarkUpdateTreeDirty() { s_walkDirty = true; }

static bool RebuildWalkSnapshot(EntityList* self) {
    s_walkSnap.clear();
    s_walkOwner = self;
    s_walkCountAtBuild = self->updateTreeCount;
    EntityTreeNode* treeRoot = self->updateTreeRoot;
    if (!treeRoot) { s_walkDirty = false; return true; }
    g_treeCorrupt = false;
    EntityTreeNode* node = TreeMinimum(treeRoot);
    EntityTreeNode* prev = nullptr;
    s_walkSnap.reserve((size_t)(self->updateTreeCount > 0 ? self->updateTreeCount : 16));
    while (node && !g_treeCorrupt) {
        int64_t entity = (int64_t)node->entityIBP;
        if (PtrCorrupt((uintptr_t)entity)) { FlagTreeCorrupt("snap.entityIBP", node, (void*)entity); break; }
        s_walkSnap.push_back(entity);
        prev = node;
        node = TreeSuccessor(node);
    }
    if (g_treeCorrupt) { ReportTreeCorrupt("Snapshot", self, prev); s_walkSnap.clear(); return false; }
    s_walkDirty = false;
    return true;
}

// Returns true when the snapshot walk ran (so the tree walk is skipped).
static bool WalkUpdateTreeSnapshot(EntityList* self, void* time, bool prof) {
    if (!PerfX::TreeSnapshot()) return false;
    if (s_walkDirty || s_walkOwner != self || s_walkCountAtBuild != self->updateTreeCount) {
        if (!RebuildWalkSnapshot(self)) return false;   // corrupt: fall back to the tree walk
    }
    const size_t n = s_walkSnap.size();
    const int64_t* arr = s_walkSnap.data();
    for (size_t i = 0; i < n; ++i) {
        // Three-deep prefetch pipeline: entity header (i+3) -> its component IBP array
        // (i+2, header already resident) -> the component objects' flag words (i+1,
        // array already resident). Each level only touches memory the previous level
        // pulled in, so no prefetch ever waits on a miss.
        if (i + 3 < n) __builtin_prefetch((const char*)arr[i + 3] + 0x30);
        if (i + 2 < n) {
            const char* comps = *(const char* const*)((const char*)arr[i + 2] + 0x30);
            if (comps) __builtin_prefetch(comps);
        }
        if (i + 1 < n) {
            const char* pe1 = (const char*)arr[i + 1];
            const int64_t* comps = *(const int64_t* const*)(pe1 + 0x30);
            int cc = *(const int32_t*)(pe1 + 0x38);
            if (comps && cc > 0) {
                if (cc > 6) cc = 6;
                for (int k = 0; k < cc; ++k) {
                    const char* c = (const char*)comps[k];
                    if (c) __builtin_prefetch(c + 0x20);
                }
            }
        }
        int64_t entity = arr[i];
        if (!((*(uint32_t*)(entity + 0x40) >> 10) & 1)) {
            EntityList_TreeNodeHandler(entity, time);
        }
        if (prof) ++g_elProfTreeNodes;
    }
    return true;
}

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
    LARGE_INTEGER pq0, pq1;
    const bool prof = g_compProfOn != 0;
    if (prof) QueryPerformanceCounter(&pq0);
    EntityList_DrainRemovals(self);
    if (prof) { QueryPerformanceCounter(&pq1); g_elProfDrainTicks += pq1.QuadPart - pq0.QuadPart; pq0 = pq1; }

    // Step 1: Accumulate time
    self->accumulatedTime += *(int64_t*)time;

    // Step 2: PreUpdate — process staging arrays into update tree
    EntityList_PreUpdate(self);
    if (prof) { QueryPerformanceCounter(&pq1); g_elProfPreTicks += pq1.QuadPart - pq0.QuadPart; pq0 = pq1; }

    // Step 3: In-order traversal of update tree (+0x68)
    EntityTreeNode* treeRoot = self->updateTreeRoot;
    if (treeRoot && WalkUpdateTreeSnapshot(self, time, prof)) {
        treeRoot = nullptr;   // [PerfX] flat snapshot walk ran -- identical order, no tree chase
    }
    if (treeRoot) {
        g_treeCorrupt = false;

        // Find leftmost node (in-order start)
        EntityTreeNode* node = TreeMinimum(treeRoot);
        EntityTreeNode* prev = nullptr;

        while (node && !g_treeCorrupt) {
            // CheckPtr on the node (it's an IBP)

            // Guard the node's entity-IBP key against a freelist link (UAF node).
            int64_t entity = (int64_t)node->entityIBP;
            if (PtrCorrupt((uintptr_t)entity)) {
                FlagTreeCorrupt("entityIBP", node, (void*)entity);
                break;
            }

            // Prefetch the in-order successor's entity header + component array before
            // ticking this one (PERF 2026-09-04: the walk is a pointer chase — node ->
            // entity -> component IBP array -> component -> vtable — and the entity
            // miss showed as the hottest self-time line). The real successor is still
            // computed AFTER the handler, exactly as retail does, so a node the handler
            // inserts as our successor is not skipped; this is a hint only.
            if (g_treePrefetch) {
                EntityTreeNode* pf = TreeSuccessor(node);
                if (pf && !PtrCorrupt((uintptr_t)pf->entityIBP)) {
                    const char* pe = (const char*)pf->entityIBP;
                    __builtin_prefetch(pe + 0x30);
                    const char* comps = *(const char* const*)(pe + 0x30);
                    if (comps) __builtin_prefetch(comps);
                }
            }

            // Check entity not deleted (bit 10 of flags at entity+0x40)
            if (!((*(uint32_t*)(entity + 0x40) >> 10) & 1)) {
                // Call tree node handler (inline for Update, calls vtable +0x40)
                EntityList_TreeNodeHandler(entity, time);
            }

            // Move to in-order successor
            prev = node;
            node = TreeSuccessor(node);
            if (prof) ++g_elProfTreeNodes;
        }

        if (g_treeCorrupt) ReportTreeCorrupt("Update", self, prev);
    }
    if (prof) { QueryPerformanceCounter(&pq1); g_elProfTreeTicks += pq1.QuadPart - pq0.QuadPart; pq0 = pq1; }

    // Step 4: 5-bucket round-robin
    g_samplerTag = 8;   // [Sampler] sub-tag: bucket (slot-9) ticks; restored to 4 at the end
    struct BucketTagReset { ~BucketTagReset() { g_samplerTag = 4; } } bucketTagReset;
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
                if (prof) ++g_elProfBucketVisits;

                // Check not deleted (bit 10)
                if (!((*(uint32_t*)(entIBP + 0x40) >> 10) & 1)) {
                    int64_t delta = currentTime - *(int64_t*)(entIBP + 0x20);

                    if (delta > 0) {
                        if (prof) ++g_elProfBucketTicked;
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

                                // Component flags at byte +0x20, bit 3 = "needs Update tick"
                                // Verified from disassembly: MOV ECX,[RAX+0x20]; SHR ECX,3
                                if ((*(uint32_t*)(compIBP + 0x20) >> 3) & 1) {

                                    // Component slot 9 (+0x48) Update, by name.
                                    Component* comp = reinterpret_cast<Component*>(compIBP);   // B.3.x bridge
                                    if (g_compProfOn) {
                                        // [STRESS] per-component-type Update cost. Keyed by
                                        // vtable, so the only per-call cost is two QPC reads.
                                        const int slot = CompProf_Begin((void*)(uintptr_t)compIBP);
                                        const long long q0 = CompProf_Now();
                                        comp->Update(deltaTime);
                                        CompProf_End(slot, CompProf_Now() - q0);
                                    } else {
                                        comp->Update(deltaTime);
                                    }
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
    if (prof) { QueryPerformanceCounter(&pq1); g_elProfBucketTicks += pq1.QuadPart - pq0.QuadPart; }

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
    if (!gw)
        return;
    void* ui = reinterpret_cast<GameUpdateLoop*>(gw)->m_userInterface;   // B.3.8 bridge
    if (!ui) return;

    // EntityUI is the shared frame controller on every entity info dialog; its +0x28 is
    // the data-source entity (raw .rdata type-id is null in standalone -> resolve by name).
    const uint32_t s_tidEntityUI = EntityUI::TypeId();   // caller batch CB6: was a lazy by-name cache
    if (s_tidEntityUI == 0 || s_tidEntityUI == 0xFFFFFFFFu) return;

    // Collect first, close after — fn_UI_CloseDialog mutates the list we're walking.
    void* toClose[16];
    int nClose = 0;

    // B.3.2: UI::UserInterface::_dialogs IS a Collection::LinkedList<DialogInstancePtr>
    // at +0x000, and retail's UI::UserInterface::GetFirstDialog (0x180031150) is just
    // `*(void**)retval = _dialogs._head`. Walking the container by name replaces the
    // raw head-fetch + `node + 0x10` next-pointer arithmetic; the node layout it read
    // (element +0x00, prev +0x08, next +0x10) is exactly LinkedList<T>::Node.
    auto& dialogs = reinterpret_cast<UI::UserInterface*>(ui)->_dialogs;
    for (auto it = dialogs.GetFirst(); it && nClose < 16; ++it) {
        void* dialog = *it;                                        // node[0] = DialogInstance*
        if (dialog) {
            uintptr_t ctrlBuf[2] = {};
            ::new (ctrlBuf) Object::InternalPtr<UI::DialogController>(
                reinterpret_cast<UI::DialogInstance*>(dialog)->GetController(s_tidEntityUI));  // B.3.6 bridge
            if (ctrlBuf[0] != 0) {
                void* ctrl = reinterpret_cast<void*>(ctrlBuf[0]);
                void* ds = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(ctrl) + 0x28);
                if (ds && ((*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(ds) + 0x40) >> 10) & 1))
                    toClose[nClose++] = dialog;
            }
        }
    }

    for (int i = 0; i < nClose; i++) {
        void* dlgUI = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(toClose[i]) + 0x10);
        if (!dlgUI) dlgUI = ui;
        uint64_t dialogRef = reinterpret_cast<uint64_t>(toClose[i]);
        reinterpret_cast<UI::UserInterface*>(dlgUI)->CloseDialog(  // B.3.6 bridge
            reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&dialogRef));
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
    void* entityUI = reinterpret_cast<GameUpdateLoop*>(gw)->m_entityUI;   // B.3.8 bridge
    if (!entityUI) return;
    void* toolSlot = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(entityUI) + 0x28);
    if (*reinterpret_cast<void**>(toolSlot) == nullptr) return;
    void* tool = *reinterpret_cast<void**>(toolSlot);
    if (!tool) return;

    const uint32_t s_tidEditTool = EditTool::TypeId();   // caller batch CB6: was a lazy by-name cache
    if (s_tidEditTool == 0 || s_tidEditTool == 0xFFFFFFFFu) return;
    if (!fn_IsDerivedFrom(tool, s_tidEditTool)) return;

    // Selected-entity IBP at EditTool+0x30 (the outline-follow target).
    void* entSlot = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(tool) + 0x30);
    if (*reinterpret_cast<void**>(entSlot) == nullptr) return;
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

    const uint32_t s_tidModel = ModelComponent::TypeId();   // caller batch CB6: was a lazy by-name cache


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
            // Caller batch CB6: slot 7 `OnRemove` of the proved ModelComponent (the
            // IsDerivedFrom test above); a compiler table's slot is never null.
            reinterpret_cast<ModelComponent*>(c)->OnRemove();   // B.3.8 bridge
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

    for (int64_t i = 0; i < removalCount; i++) {
        void* entityIBP = (void*)(*(int64_t*)((uintptr_t)self->removalList.data + i * 8));
        AuditDyingEntity(reinterpret_cast<int64_t>(entityIBP));
        EntityList_RemoveEntity(self, &entityIBP);
    }

    // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
    // new tail zeroed. Element is the 8-byte entity InternalBasePtr (byte-copy).
    GameColl::AsArray<GameColl::Ptr8>(&self->removalList).SetSize(0);

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
        // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
        // new tail zeroed. Element is the 8-byte entity InternalBasePtr (byte-copy).
        GameColl::AsArray<GameColl::Ptr8>(&self->stagingUpdate).SetSize(0);
        return;
    }

    for (int i = 0; i < stagingCount; i++) {
        int64_t offset = (int64_t)i * 8;
        void* ibpPtr = (void*)((uintptr_t)self->stagingUpdate.data + offset);

        int64_t entityVal = *(int64_t*)ibpPtr;

        uint32_t entityFlags = *(uint32_t*)(entityVal + 0x40);
        bool hasUpdateFlag = (entityFlags & 0x4) != 0;
        bool isDeleted = ((entityFlags >> 10) & 1) != 0;

        if (hasUpdateFlag && !isDeleted) {
            // Insert into update tree
            uint64_t outBuf = 0;
            void* srcIBP = (void*)((uintptr_t)self->stagingUpdate.data + offset);
            fn_IBPTreeInsert(&self->updateTreeRoot, &outBuf, srcIBP);
            EntityList_MarkUpdateTreeDirty();
        } else {
            // Search for entity in update tree, remove if found
            EntityTreeNode* node = self->updateTreeRoot;
            void* ibpSrc = (void*)((uintptr_t)self->stagingUpdate.data + offset);

            while (node != nullptr) {
                uint64_t ibpVal = *(uint64_t*)ibpSrc;

                if (ibpVal < node->entityIBP) {
                    node = node->left;
                } else {
                    if (ibpVal <= node->entityIBP) {
                        // Found — remove from tree
                        // Validate first: the AVL delete derefs node->parent
                        // unguarded (faithful to retail 0x14001a6e0).
                        if (!StagingNodeRemovable(node)) {
                            ReportStagingCorrupt("PreUpdate", self, node);
                            break;
                        }
                        fn_IBPTreeRemove(&self->updateTreeRoot, node);
                        EntityList_MarkUpdateTreeDirty();
                        break;
                    }
                    node = node->right;
                }
            }
        }
    }

    // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
    // new tail zeroed. Element is the 8-byte entity InternalBasePtr (byte-copy).
    GameColl::AsArray<GameColl::Ptr8>(&self->stagingUpdate).SetSize(0);
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
        // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
        // new tail zeroed. Element is the 8-byte entity InternalBasePtr (byte-copy).
        GameColl::AsArray<GameColl::Ptr8>(&self->stagingPostUpdate).SetSize(0);
        return;
    }

    for (int i = 0; i < stagingCount; i++) {
        int64_t offset = (int64_t)i * 8;
        void* ibpPtr = (void*)((uintptr_t)self->stagingPostUpdate.data + offset);

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
                uint64_t ibpVal = *(uint64_t*)ibpSrc;

                if (ibpVal < node->entityIBP) {
                    node = node->left;
                } else {
                    if (ibpVal <= node->entityIBP) {
                        // Found — remove from tree
                        // Same guard as PreUpdate. This is the path that actually
                        // crashed on the 2026-09-04 tutorial run.
                        if (!StagingNodeRemovable(node)) {
                            ReportStagingCorrupt("PostUpdate", self, node);
                            break;
                        }
                        fn_IBPTreeRemove(&self->postUpdateTreeRoot, node);
                        break;
                    }
                    node = node->right;
                }
            }
        }
    }

    // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
    // new tail zeroed. Element is the 8-byte entity InternalBasePtr (byte-copy).
    GameColl::AsArray<GameColl::Ptr8>(&self->stagingPostUpdate).SetSize(0);
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

        // Guard the node's entity-IBP key: a freed-but-still-linked node has a
        // freelist link written over +0x00, so dereferencing it faults.
        int64_t entity = (int64_t)node->entityIBP;
        if (PtrCorrupt((uintptr_t)entity)) {
            FlagTreeCorrupt("entityIBP", node, (void*)entity);
            break;
        }

        // Check entity not deleted (bit 10 of flags at entity+0x40)
        if (!((*(uint32_t*)(entity + 0x40) >> 10) & 1)) {
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
    int64_t entity = *(int64_t*)entityIBP;


    // Read bucket index from entity+0x1F
    int8_t bucketIdx = *(int8_t*)(entity + 0x1F);

    // Step 1: Remove from bucket (if in one; bucket 6 means "none")
    if (bucketIdx != 6) {
        // Get bucket array pointer: self + 0x18 + bucketIdx * 0x10
        uintptr_t bucketBase = (uintptr_t)self + 0x18 + (int64_t)bucketIdx * 0x10;

        // Last element index in bucket
        uint32_t lastIdx = *(int32_t*)(bucketBase + 0x08) - 1;

        entity = *(int64_t*)entityIBP;

        // Entity's index within bucket (low 24 bits of +0x1C)
        uint32_t entityBucketIdx = (*(uint32_t*)(entity + 0x1C) << 8) >> 8;

        if (lastIdx != entityBucketIdx) {
            // Swap last element into removed element's slot
            int64_t* bucketData = *(int64_t**)bucketBase;
            *(int64_t*)(bucketData + entityBucketIdx) = *(int64_t*)(bucketData + lastIdx);

            // Update swapped entity's bucket index field
            void* swappedIBP = (void*)(*(int64_t**)bucketBase + entityBucketIdx);
            int64_t swappedEntity = *(int64_t*)swappedIBP;

            // Update low 24 bits of +0x1C with new index
            uint32_t* packedField = (uint32_t*)(swappedEntity + 0x1C);
            *packedField = *packedField ^ ((*(uint32_t*)(swappedEntity + 0x1C) ^ entityBucketIdx) & 0xFFFFFF);
        }

        // Shrink bucket array.
        // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
        // new tail zeroed. Element is the 8-byte entity InternalBasePtr (byte-copy).
        GameColl::AsArray<GameColl::Ptr8>(bucketBase).SetSize((int)lastIdx);
    }

    // Step 2: Remove from master array (+0x08)
    int32_t lastMasterIdx = self->masterArray.count - 1;

    entity = *(int64_t*)entityIBP;
    int32_t entityMasterIdx = *(int32_t*)(entity + 0x18);

    if (lastMasterIdx != entityMasterIdx) {
        // Swap last element into removed element's slot
        int64_t* masterData = (int64_t*)self->masterArray.data;
        masterData[entityMasterIdx] = masterData[lastMasterIdx];

        // Update swapped entity's master array index
        void* swappedIBP = (void*)&masterData[entityMasterIdx];
        *(int32_t*)(*(int64_t*)swappedIBP + 0x18) = entityMasterIdx;
    }

    // Shrink master array.
    // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
    // new tail zeroed. Element is the 8-byte entity InternalBasePtr (byte-copy).
    GameColl::AsArray<GameColl::Ptr8>(&self->masterArray).SetSize(lastMasterIdx);

    // Step 3: Search and remove from update tree (+0x68)
    EntityTreeNode* updateNode = self->updateTreeRoot;
    while (updateNode != nullptr) {
        if (*(uint64_t*)entityIBP < updateNode->entityIBP) {
            updateNode = updateNode->left;
        } else {
            if (*(uint64_t*)entityIBP <= updateNode->entityIBP) {
                break;  // Found
            }
            updateNode = updateNode->right;
        }
    }

    // Step 4: Search and remove from post-update tree (+0x78)
    EntityTreeNode* postUpdateNode = self->postUpdateTreeRoot;
    while (postUpdateNode != nullptr) {
        if (*(uint64_t*)entityIBP < postUpdateNode->entityIBP) {
            postUpdateNode = postUpdateNode->left;
        } else {
            if (*(uint64_t*)entityIBP <= postUpdateNode->entityIBP) {
                break;  // Found
            }
            postUpdateNode = postUpdateNode->right;
        }
    }

    // Remove found nodes from trees
    if (updateNode != nullptr) {
        fn_IBPTreeRemove(&self->updateTreeRoot, updateNode);
        EntityList_MarkUpdateTreeDirty();
    }
    if (postUpdateNode != nullptr) {
        fn_IBPTreeRemove(&self->postUpdateTreeRoot, postUpdateNode);
    }

    // Step 5: Delete the entity IBP reference
    reinterpret_cast< ::Object::InternalBasePtr*>(entityIBP)->DeletePtr();   // B.3.3 bridge
}

// ============================================================
// EntityList::FlagAggregation (0x1400c3470)
// ============================================================
// This function at 0x1400c3470 is the SAME function as
// ComponentEntity::RefreshEntityFlags, already reimplemented
// in code/Game/Entity/componententity.cpp.
// Delegates to that implementation.
// Caller batch CB6: the member (the old ComponentEntity_RefreshEntityFlags door's
// body); the dead `s_fn_FlagAggregation` door cache (a ResolveAddr of 0x1400c3470
// read only by a debug printf) went with it.
void EntityList_FlagAggregation(void* entity) {
    reinterpret_cast<ComponentEntity*>(entity)->RefreshEntityFlags();   // B.3.8 bridge
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

void EntityList_Serialize(EntityList* self, void* stream) {
    // Step 1: Construct Serializer (local buffer, 0x18 bytes)
    alignas(8) uint8_t serializerBuf[0x18];
    void* serializer = serializerBuf;
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Step 2: Flush staging arrays
    EntityList_PreUpdate(self);
    EntityList_PostUpdateStaging(self);

    // Step 3: On save (stream+0x08 == 1), process removal list
    if (static_cast<int>(reinterpret_cast<IO::Stream*>(stream)->GetDirection()) == 1) {   // B.3.4 bridge  (IO::Stream::GetDirection, retail 0x180006200)
        int32_t removalCount = self->removalList.count;
        if (removalCount > 0) {
            for (int64_t i = 0; i < removalCount; i++) {
                uint64_t ibpVal = *((uint64_t*)self->removalList.data + i);
                uint64_t localIBP = ibpVal;
                EntityList_RemoveEntity(self, &localIBP);
            }
            // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8
            // align 8, new tail zeroed. Element is the entity InternalBasePtr.
            GameColl::AsArray<GameColl::Ptr8>(&self->removalList).SetSize(0);
        }
    }

    reinterpret_cast<System::Time*>(self)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge

    // Step 5: Serialize master array count
    int32_t masterCount = self->masterArray.count;
    int32_t serializedCount = masterCount;
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(&serializedCount), nullptr);   // B.3.4 bridge

    // On load (mode == 0): resize master array to deserialized count
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
        // new tail zeroed. Element is the 8-byte entity InternalBasePtr (byte-copy).
        GameColl::AsArray<GameColl::Ptr8>(&self->masterArray).SetSize(serializedCount);
    }

    // Serialize each entity IBP in master array
    void* ibpIter = self->masterArray.data;
    int64_t ibpCount = (int64_t)serializedCount;
    if (ibpCount > 0) {
        for (int64_t j = 0; j < ibpCount; j++) {
            void* currentIBP = (void*)((uintptr_t)ibpIter + j * 8);
            reinterpret_cast<Object::InternalBasePtr*>(currentIBP)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge

            // After loading, verify it's a ComponentEntity
            if (*(void**)currentIBP != nullptr) {
                bool isDerived = fn_IsDerivedFrom(*(void**)currentIBP, ComponentEntity::TypeId());
                if (!isDerived) {
                    *(uint64_t*)currentIBP = 0;
                }
            }
        }
    }

    // Step 6: Serialize bucket trees via helper (5 buckets starting at +0x18)
    fn_BucketTreeSerialize(serializer, nullptr, &self->buckets[0]);

    // Step 7: Serialize processCount[5] and cursor[5] as bulk arrays
    // Retail (0x1400c45d0 / 0x1400c45e8) calls dispatch slot 0x140151278 =
    // IO::Serializer::Serialize(int count, int* data, name) — the INT-array
    // overload, 20 bytes per call. We were calling slot 0x140152400
    // (fn_Serializer_SerializeBulk), the BYTE overload, which read only 5 bytes
    // each and left the stream 30 bytes short for every load — silently
    // desyncing Geometry and everything after it. The offsets prove the width:
    // processCount is at +0xA8 and cursor at +0xBC, exactly 5 ints apart.
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(5, reinterpret_cast<int*>(self->processCount), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(5, reinterpret_cast<int*>(self->cursor), nullptr);   // B.3.4 bridge

    // Step 8: Per-entity metadata serialization
    int32_t entityIdx = 0;
    int64_t ibpOffset = 0;
    if (self->masterArray.count > 0) {
        do {
            uintptr_t masterData = (uintptr_t)self->masterArray.data;

            // Read master array index from entity+0x18
            int32_t masterIdxField = *(int32_t*)(*(int64_t*)(masterData + ibpOffset) + 0x18);

            // Read bucket index (low 24 bits of entity+0x1C)
            masterData = (uintptr_t)self->masterArray.data;
            uint32_t bucketIdxField = (*(uint32_t*)(*(int64_t*)(masterData + ibpOffset) + 0x1C) << 8) >> 8;

            // Read bucket ID from entity+0x1F
            masterData = (uintptr_t)self->masterArray.data;
            int32_t bucketId = (int32_t)*(int8_t*)(*(int64_t*)(masterData + ibpOffset) + 0x1F);

            // Serialize the three metadata fields
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(&masterIdxField), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>((int32_t*)&bucketIdxField), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(&bucketId), nullptr);   // B.3.4 bridge

            // Serialize entity's timestamp at +0x20 (System::Time)
            masterData = (uintptr_t)self->masterArray.data;
            reinterpret_cast<System::Time*>((void*)(*(int64_t*)(masterData + ibpOffset) + 0x20))->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge

            // Write back deserialized metadata to entity
            masterData = (uintptr_t)self->masterArray.data;
            *(int32_t*)(*(int64_t*)(masterData + ibpOffset) + 0x18) = masterIdxField;

            masterData = (uintptr_t)self->masterArray.data;
            int64_t ent = *(int64_t*)(masterData + ibpOffset);
            uint32_t* packedField = (uint32_t*)(ent + 0x1C);
            *packedField = *packedField ^ ((bucketIdxField ^ *(uint32_t*)(ent + 0x1C)) & 0xFFFFFF);

            masterData = (uintptr_t)self->masterArray.data;
            *(int8_t*)(*(int64_t*)(masterData + ibpOffset) + 0x1F) = (int8_t)bucketId;

            entityIdx++;
            ibpOffset += 8;
        } while (entityIdx < self->masterArray.count);
    }

    // Step 9: Destroy Serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}
