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
//   - EntityList::FlagAggregation  (0x1400c3470) — is ComponentEntity::RefreshEntityFlags
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
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "Game/GameUpdateLoop.h"   // caller batch CB6: the world's named fields
#include "Dialog/EntityUI.h"             // caller batch CB6: TypeId()
#include "Player/EditTool.h"
#include "Game/PlayerToolManager.h"   // m_toolManager->currentTool
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
#include "Entity/exe_bodies.h"
#include "Game/exe_bodies.h"   // BucketTree_Serialize_21ba0
#include "Work/exe_bodies.h"   // WorkBST_RegisterWork_1a30, WorkBST_BSTRemove_a6e0


// ============================================================
// ComponentEntity::TickComponents (0x1400c2de0)
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

void ComponentEntity::TickComponents(void* time) {
    uint32_t entityFlags = _entityFlags;

    // Check entity has "needs tree tick" flag (bit 2) and is not deleted (bit 10)
    if ((entityFlags & 0x4) == 0) return;
    if ((entityFlags >> 10) & 1) return;

    int32_t compCount = _componentCount;
    int64_t ci = 0;

    if (compCount <= 0) return;

    do {
        // Read component IBP and CheckPtr
        Component* c = _components[ci].Get();

        // Component flags at byte offset +0x20 from component base.
        // Verified from disassembly at 0x1400c2e42: MOV ECX,dword ptr [RAX + 0x20]
        // Then SHR ECX,0x2; TEST CL,0x1 checks bit 2.
        uint32_t compFlags = c->_flags;

        if ((compFlags >> 2) & 1) {
            // CheckPtr again before vtable call
            c = _components[ci].Get();

            // Component slot 8 (+0x40) Tick, by name.
            if (g_compProfOn >= 2) {
                // [STRESS] BANISHED_STRESS_COMPPROF=2: attribute the per-frame (slot 8) tick
                // per component type, separately from the slot-9 bucket Update (kind 1 =
                // "/tree"). Opt-in: ~30k QPC pairs per frame at 800x10 (~+0.8 ms/frame).
                const int slot = CompProf_BeginKind(c, 1);
                const long long q0 = CompProf_Now();
                c->Tick(time);
                CompProf_End(slot, CompProf_Now() - q0);
            } else {
                c->Tick(time);
            }
        }

        ci++;

        // Break if entity was deleted during component processing
        if (_entityFlags & 0x400) break;

    } while (ci < compCount);
}

// ============================================================
// ComponentEntity::PostUpdateComponents (0x1400c2c70)
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
void ComponentEntity::PostUpdateComponents() {
    uint32_t entityFlags = _entityFlags;

    // Check entity has "needs post-update" flag (bit 4) and is not deleted (bit 10)
    if ((entityFlags & 0x10) == 0) return;
    if ((entityFlags >> 10) & 1) return;

    int32_t compCount = _componentCount;
    int64_t ci = 0;

    if (compCount <= 0) return;

    do {
        // Read component IBP and CheckPtr
        Component* c = _components[ci].Get();

        // Component flags at +0x20, bit 4 = "needs post-update"
        uint32_t compFlags = c->_flags;

        if ((compFlags >> 4) & 1) {
            // CheckPtr again before vtable call; slot 10 (+0x50) PostUpdate, by name.
            _components[ci].Get()->PostUpdate();
        }

        ci++;

        // Break if entity was deleted during component processing
        if (_entityFlags & 0x400) break;

    } while (ci < compCount);
}

// (B3R W2-EZ: IBPTreeInsert_CppImpl / IBPTreeRemove_CppImpl, the plain-BST
// stand-ins engine_funcs.cpp installed when 0x140021a30 / 0x14001a6e0 resolved null,
// are gone with it. Both addresses are registered (the AVL ports in
// code/Game/Work/work_base_bst.cpp), so the fallback never ran; nothing else called them.)

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
static inline bool PtrCorrupt(const void* pv) {
    if (!g_treeGuards) return false;
    const uintptr_t p = reinterpret_cast<uintptr_t>(pv);  // CAST-OK: ptr_int
    if (p == 0) return false;                 // null = legitimate terminator / cleared slot
    if (p & 0x7) return true;                 // heap objects are 8-byte aligned
    if ((uint32_t)(p >> 32) != 0 && (uint32_t)p < 0x10000) return true; // half-written
    return false;
}

// A non-null pointer that cannot be a live tree node.
static inline bool NodeCorrupt(const EntityTreeNode* n) {
    return PtrCorrupt(n);
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
static const void*      g_treeCorruptBad  = nullptr;   // the refused pointer value

static void FlagTreeCorrupt(const char* site, EntityTreeNode* from, const void* bad) {
    g_treeCorrupt = true;
    g_treeCorruptSite = site;
    g_treeCorruptFrom = from;
    g_treeCorruptBad = bad;
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
    if (PtrCorrupt(node->parent)) return false;
    if (PtrCorrupt(node->left))   return false;
    if (PtrCorrupt(node->right))  return false;
    if (PtrCorrupt(node->entityIBP)) return false;
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
               static_cast<void*>(node->entityIBP), static_cast<void*>(node->parent),
               static_cast<void*>(node->left), static_cast<void*>(node->right),
               (int)node->balance, (int)node->height);
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
    static const void*      lastBad  = nullptr;
    if (g_treeCorruptSite == lastSite && g_treeCorruptFrom == lastFrom &&
        g_treeCorruptBad == lastBad) {
        return;
    }
    lastSite = g_treeCorruptSite; lastFrom = g_treeCorruptFrom; lastBad = g_treeCorruptBad;

    printf("[TreeGuard] CORRUPT node refused during %s walk: site=%s bad=%p\n",
           walk, g_treeCorruptSite ? g_treeCorruptSite : "?", g_treeCorruptBad);
    EntityTreeNode* f = g_treeCorruptFrom;
    if (f && !NodeCorrupt(f)) {
        printf("[TreeGuard]   from-node=%p  entityIBP=%p parent=%p left=%p right=%p bal=%d ht=%d\n",
               static_cast<void*>(f), static_cast<void*>(f->entityIBP),
               static_cast<void*>(f->parent), static_cast<void*>(f->left),
               static_cast<void*>(f->right), (int)f->balance, (int)f->height);
    }
    if (prev && !NodeCorrupt(prev) && prev != f) {
        printf("[TreeGuard]   prev-node=%p  entityIBP=%p parent=%p left=%p right=%p\n",
               static_cast<void*>(prev), static_cast<void*>(prev->entityIBP),
               static_cast<void*>(prev->parent), static_cast<void*>(prev->left),
               static_cast<void*>(prev->right));
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
static std::vector<ComponentEntity*> s_walkSnap;
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
        ComponentEntity* entity = node->entityIBP;
        if (PtrCorrupt(entity)) { FlagTreeCorrupt("snap.entityIBP", node, entity); break; }
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
    ComponentEntity* const* arr = s_walkSnap.data();
    for (size_t i = 0; i < n; ++i) {
        // Three-deep prefetch pipeline: entity header (i+3) -> its component IBP array
        // (i+2, header already resident) -> the component objects' flag words (i+1,
        // array already resident). Each level only touches memory the previous level
        // pulled in, so no prefetch ever waits on a miss.
        if (i + 3 < n) __builtin_prefetch(&arr[i + 3]->_components);
        if (i + 2 < n) {
            const Object::InternalPtr<Component>* comps = arr[i + 2]->_components;
            if (comps) __builtin_prefetch(comps);
        }
        if (i + 1 < n) {
            const ComponentEntity* pe1 = arr[i + 1];
            const Object::InternalPtr<Component>* comps = pe1->_components;
            int cc = pe1->_componentCount;
            if (comps && cc > 0) {
                if (cc > 6) cc = 6;
                for (int k = 0; k < cc; ++k) {
                    const Component* c = comps[k].Get();
                    if (c) __builtin_prefetch(&c->_flags);
                }
            }
        }
        ComponentEntity* entity = arr[i];
        if (!((entity->_entityFlags >> 10) & 1)) {
            entity->TickComponents(time);
        }
        if (prof) ++g_elProfTreeNodes;
    }
    return true;
}

void EntityList::Update(void* time) {
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
    DrainRemovals();
    if (prof) { QueryPerformanceCounter(&pq1); g_elProfDrainTicks += pq1.QuadPart - pq0.QuadPart; pq0 = pq1; }

    // Step 1: Accumulate time
    accumulatedTime._ticks += *(int64_t*)time;   // LEFT: SIG EntityList::Update(void* time) (caller gameupdateloop.cpp, GAME-2)

    // Step 2: PreUpdate — process staging arrays into update tree
    PreUpdate();
    if (prof) { QueryPerformanceCounter(&pq1); g_elProfPreTicks += pq1.QuadPart - pq0.QuadPart; pq0 = pq1; }

    // Step 3: In-order traversal of update tree (+0x68)
    EntityTreeNode* treeRoot = updateTreeRoot;
    if (treeRoot && WalkUpdateTreeSnapshot(this, time, prof)) {
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
            ComponentEntity* entity = node->entityIBP;
            if (PtrCorrupt(entity)) {
                FlagTreeCorrupt("entityIBP", node, entity);
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
                if (pf && !PtrCorrupt(pf->entityIBP)) {
                    const ComponentEntity* pe = pf->entityIBP;
                    __builtin_prefetch(&pe->_components);
                    const Object::InternalPtr<Component>* comps = pe->_components;
                    if (comps) __builtin_prefetch(comps);
                }
            }

            // Check entity not deleted (bit 10 of flags at entity+0x40)
            if (!((entity->_entityFlags >> 10) & 1)) {
                // Call tree node handler (inline for Update, calls vtable +0x40)
                entity->TickComponents(time);
            }

            // Move to in-order successor
            prev = node;
            node = TreeSuccessor(node);
            if (prof) ++g_elProfTreeNodes;
        }

        if (g_treeCorrupt) ReportTreeCorrupt("Update", this, prev);
    }
    if (prof) { QueryPerformanceCounter(&pq1); g_elProfTreeTicks += pq1.QuadPart - pq0.QuadPart; pq0 = pq1; }

    // Step 4: 5-bucket round-robin
    g_samplerTag = 8;   // [Sampler] sub-tag: bucket (slot-9) ticks; restored to 4 at the end
    struct BucketTagReset { ~BucketTagReset() { g_samplerTag = 4; } } bucketTagReset;
    int64_t currentTime = accumulatedTime._ticks;

    for (int bucket = 0; bucket < 5; bucket++) {
        UpdateBucket* bkt = &buckets[bucket];
        int32_t count = bkt->count;
        if (count == 0) continue;

        uint32_t toProcess = processCount[bucket];
        uint32_t cursorVal = cursor[bucket];
        if ((int32_t)cursorVal >= count) cursorVal = 0;

        uint32_t startIdx = cursorVal;
        uint32_t idx = startIdx;
        uint32_t processed = 0;

        if ((int32_t)toProcess > 0) {
            do {
                // Re-read bucket data pointer each iteration (may be reallocated)
                ComponentEntity** arrBase = bkt->data;

                // Read entity IBP and CheckPtr
                ComponentEntity* entIBP = arrBase[idx];
                if (prof) ++g_elProfBucketVisits;

                // Check not deleted (bit 10)
                if (!((entIBP->_entityFlags >> 10) & 1)) {
                    // +0x20 is the entity's last-update System::Time (Serialize below
                    // writes it through Time::Serialize)
                    int64_t delta = currentTime - entIBP->_lastUpdateTime._ticks;

                    if (delta > 0) {
                        if (prof) ++g_elProfBucketTicked;
                        ComponentEntity* ent = entIBP;

                        // Check entity has "needs bucket update" flag (bit 3)
                        // and is not deleted (bit 10)
                        if ((ent->_entityFlags & 0x8) != 0 &&
                            (ent->_entityFlags & 0x400) == 0) {

                            int32_t compCount = ent->_componentCount;
                            int64_t deltaTime[1] = { delta };

                            for (int64_t ci = 0; ci < compCount; ci++) {
                                // Read component IBP and CheckPtr
                                Component* comp = ent->_components[ci].Get();

                                // Component flags at byte +0x20, bit 3 = "needs Update tick"
                                // Verified from disassembly: MOV ECX,[RAX+0x20]; SHR ECX,3
                                if ((comp->_flags >> 3) & 1) {

                                    // Component slot 9 (+0x48) Update, by name.
                                    if (g_compProfOn) {
                                        // [STRESS] per-component-type Update cost. Keyed by
                                        // vtable, so the only per-call cost is two QPC reads.
                                        const int slot = CompProf_Begin(comp);
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
                                currentTime = accumulatedTime._ticks;

                                // Check if entity deleted during component update
                                if (ent->_entityFlags & 0x400) break;
                            }
                        }

                        // Update entity timestamp
                        entIBP->_lastUpdateTime._ticks = currentTime;
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
        cursor[bucket] = cursorVal;
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
    GameUpdateLoop* gw = GameUpdateLoop::_singleton;
    if (!gw)
        return;
    UI::UserInterface* ui = gw->m_userInterface;
    if (!ui) return;

    // EntityUI is the shared frame controller on every entity info dialog; its +0x28 is
    // the data-source entity (raw .rdata type-id is null in standalone -> resolve by name).
    const uint32_t s_tidEntityUI = EntityUI::TypeId();   // caller batch CB6: was a lazy by-name cache
    if (s_tidEntityUI == 0 || s_tidEntityUI == 0xFFFFFFFFu) return;

    // Collect first, close after — fn_UI_CloseDialog mutates the list we're walking.
    UI::DialogInstance* toClose[16];
    int nClose = 0;

    // B.3.2: UI::UserInterface::_dialogs IS a Collection::LinkedList<DialogInstancePtr>
    // at +0x000, and retail's UI::UserInterface::GetFirstDialog (0x180031150) is just
    // `*(void**)retval = _dialogs._head`. Walking the container by name replaces the
    // raw head-fetch + `node + 0x10` next-pointer arithmetic; the node layout it read
    // (element +0x00, prev +0x08, next +0x10) is exactly LinkedList<T>::Node.
    auto& dialogs = ui->_dialogs;
    for (auto it = dialogs.GetFirst(); it && nClose < 16; ++it) {
        // node[0] = DialogInstance*
        UI::DialogInstance* dialog = reinterpret_cast<UI::DialogInstance*>(*it);   // LEFT: UserInterface.h DialogInstancePtr is void* (R-UI-4)
        if (dialog) {
            Object::InternalPtr<UI::DialogController> ctrlRef = dialog->GetController(s_tidEntityUI);
            UI::DialogController* ctrl = ctrlRef.Get();
            if (ctrl != nullptr) {
                Object::Base* ds = ctrl->_dataSource;   // +0x28
                // The data source's +0x40 entity flags, read with no type check (standalone-only code)
                if (ds && ((*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(ds) + 0x40) >> 10) & 1))   // LEFT: unchecked Object::Base -> ComponentEntity read
                    toClose[nClose++] = dialog;
            }
        }
    }

    for (int i = 0; i < nClose; i++) {
        UI::UserInterface* dlgUI = toClose[i]->_ui;   // +0x10
        if (!dlgUI) dlgUI = ui;
        Object::InternalPtr<UI::DialogInstance> dialogRef(toClose[i]);
        dlgUI->CloseDialog(&dialogRef);
        printf("[EntityList] closed info dialog %p on dying (marked-for-removal) entity\n",
               static_cast<void*>(toClose[i]));
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
    GameUpdateLoop* gw = GameUpdateLoop::_singleton;
    if (!gw) return;
    PlayerToolManager* toolManager = gw->m_toolManager;
    if (!toolManager) return;
    if (toolManager->currentTool == nullptr) return;   // +0x28
    Tool* currentTool = toolManager->currentTool;
    if (!currentTool) return;

    const uint32_t s_tidEditTool = EditTool::TypeId();   // caller batch CB6: was a lazy by-name cache
    if (s_tidEditTool == 0 || s_tidEditTool == 0xFFFFFFFFu) return;
    if (!::Object::IsDerivedFromNullSafe(currentTool, s_tidEditTool)) return;
    EditTool* tool = static_cast<EditTool*>(currentTool);   // after the type check (B10)

    // Selected-entity IBP at EditTool+0x30 (the outline-follow target).
    if (tool->lastEntityRef == 0) return;
    uintptr_t ent = tool->lastEntityRef;
    if (ent < 0x10000 || ent > 0x00007FFFFFFFFFFFULL) return;
    if (((*reinterpret_cast<uint32_t*>(ent + 0x40) >> 10) & 1) == 0) return;   // LEFT: EditTool.h lastEntityRef is uint64_t (SA)

    // Drop both the dialog ref (+0x28) and the selected-entity ref (+0x30), exactly as
    // EditTool_CloseInfoDialog does — leaving +0x30 set would keep the follow chasing it.
    tool->entityRef = 0;
    tool->lastEntityRef = 0;
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
static void AuditDyingEntity(ComponentEntity* entity) {
    if (reinterpret_cast<uintptr_t>(entity) < 0x10000) return;  // CAST-OK: ptr_int

    const uint32_t s_tidModel = ModelComponent::TypeId();   // caller batch CB6: was a lazy by-name cache


    int models = 0, live = 0;
    int   n   = entity->_componentCount;
    Object::InternalPtr<Component>* arr = entity->_components;
    if (s_tidModel && s_tidModel != 0xFFFFFFFFu) {
        for (int i = 0; i < n && arr; ++i) {
            Component* c = arr[i].Get();
            if (reinterpret_cast<uintptr_t>(c) < 0x10000) continue;  // CAST-OK: ptr_int
            if (!::Object::IsDerivedFromNullSafe(c, s_tidModel)) continue;
            ModelComponent* model = static_cast<ModelComponent*>(c);
            ++models;
            if (model->modelId < 0) continue;
            ++live;
            // Unlink it (vtable +0x38 = the removed-from-world handler; on
            // ModelComponent that is Scene::RemoveObject + modelId = -1). Dispatched
            // virtually so derived overrides (NaturalResourceComponent) still apply.
            // Caller batch CB6: slot 7 `OnRemove` of the proved ModelComponent (the
            // IsDerivedFrom test above); a compiler table's slot is never null.
            model->OnRemove();
        }
    }

}

void EntityList::DrainRemovals() {
    int32_t removalCount = removalList.count;
    if (removalCount <= 0) return;

    // Close dialogs + clear tool selection bound to the entities we're about to free
    // (see helpers above) — both are weak refs the free does not invalidate.
    EntityList_CloseDialogsForDyingEntities();
    EntityList_ClearToolSelectionForDyingEntities();

    PreUpdate();
    PostUpdateStaging();

    for (int64_t i = 0; i < removalCount; i++) {
        ComponentEntity* entityIBP = removalList.data[i];
        AuditDyingEntity(entityIBP);
        RemoveEntity(&entityIBP);
    }

    // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
    // new tail zeroed. Element is the 8-byte entity InternalBasePtr (byte-copy).
    GameColl::AsArray<GameColl::Ptr8>(&removalList).SetSize(0);

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
void EntityList::PreUpdate() {
    int32_t stagingCount = stagingUpdate.count;
    if (stagingCount <= 0) {
        // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
        // new tail zeroed. Element is the 8-byte entity InternalBasePtr (byte-copy).
        GameColl::AsArray<GameColl::Ptr8>(&stagingUpdate).SetSize(0);
        return;
    }

    for (int i = 0; i < stagingCount; i++) {
        ComponentEntity** ibpPtr = &stagingUpdate.data[i];

        uint32_t entityFlags = (*ibpPtr)->_entityFlags;
        bool hasUpdateFlag = (entityFlags & 0x4) != 0;
        bool isDeleted = ((entityFlags >> 10) & 1) != 0;

        if (hasUpdateFlag && !isDeleted) {
            // Insert into update tree
            uint64_t outBuf = 0;
            WorkBST_RegisterWork_1a30(reinterpret_cast<uintptr_t*>(&updateTreeRoot), &outBuf, ibpPtr);   // LEFT: SIG WorkBST_* (Work/exe_bodies.h, wave X)
            EntityList_MarkUpdateTreeDirty();
        } else {
            // Search for entity in update tree, remove if found
            EntityTreeNode* node = updateTreeRoot;

            while (node != nullptr) {
                ComponentEntity* ibpVal = *ibpPtr;

                if (ibpVal < node->entityIBP) {
                    node = node->left;
                } else {
                    if (ibpVal <= node->entityIBP) {
                        // Found — remove from tree
                        // Validate first: the AVL delete derefs node->parent
                        // unguarded (faithful to retail 0x14001a6e0).
                        if (!StagingNodeRemovable(node)) {
                            ReportStagingCorrupt("PreUpdate", this, node);
                            break;
                        }
                        WorkBST_BSTRemove_a6e0(reinterpret_cast<uintptr_t*>(&updateTreeRoot), node);   // LEFT: SIG WorkBST_* (Work/exe_bodies.h, wave X)
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
    GameColl::AsArray<GameColl::Ptr8>(&stagingUpdate).SetSize(0);
}

// ============================================================
// EntityList::PostUpdateStaging (0x1400c3ab0) — static helper
// ============================================================
// Same pattern as PreUpdate but for the post-update tree (+0x78)
// and staging array (+0x98/+0xa0), checking flag bit 4 (0x10).
//
// Decompiled from 0x1400c3ab0.
void EntityList::PostUpdateStaging() {
    int32_t stagingCount = stagingPostUpdate.count;
    if (stagingCount <= 0) {
        // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
        // new tail zeroed. Element is the 8-byte entity InternalBasePtr (byte-copy).
        GameColl::AsArray<GameColl::Ptr8>(&stagingPostUpdate).SetSize(0);
        return;
    }

    for (int i = 0; i < stagingCount; i++) {
        ComponentEntity** ibpPtr = &stagingPostUpdate.data[i];

        uint32_t entityFlags = (*ibpPtr)->_entityFlags;
        bool hasPostUpdateFlag = (entityFlags & 0x10) != 0;
        bool isDeleted = ((entityFlags >> 10) & 1) != 0;

        if (hasPostUpdateFlag && !isDeleted) {
            // Insert into post-update tree
            uint64_t outBuf = 0;
            WorkBST_RegisterWork_1a30(reinterpret_cast<uintptr_t*>(&postUpdateTreeRoot), &outBuf, ibpPtr);   // LEFT: SIG WorkBST_* (Work/exe_bodies.h, wave X)
        } else {
            // Search for entity in post-update tree, remove if found
            EntityTreeNode* node = postUpdateTreeRoot;

            while (node != nullptr) {
                ComponentEntity* ibpVal = *ibpPtr;

                if (ibpVal < node->entityIBP) {
                    node = node->left;
                } else {
                    if (ibpVal <= node->entityIBP) {
                        // Found — remove from tree
                        // Same guard as PreUpdate. This is the path that actually
                        // crashed on the 2026-09-04 tutorial run.
                        if (!StagingNodeRemovable(node)) {
                            ReportStagingCorrupt("PostUpdate", this, node);
                            break;
                        }
                        WorkBST_BSTRemove_a6e0(reinterpret_cast<uintptr_t*>(&postUpdateTreeRoot), node);   // LEFT: SIG WorkBST_* (Work/exe_bodies.h, wave X)
                        break;
                    }
                    node = node->right;
                }
            }
        }
    }

    // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
    // new tail zeroed. Element is the 8-byte entity InternalBasePtr (byte-copy).
    GameColl::AsArray<GameColl::Ptr8>(&stagingPostUpdate).SetSize(0);
}

// ============================================================
// EntityList::PostUpdate (0x1400c43d0)
// ============================================================
// Processes post-update staging array, then does in-order tree walk
// of the post-update tree (+0x78), calling PostUpdateNodeHandler per entity.
//
// Decompiled from 0x1400c43d0.
void EntityList::PostUpdate() {
    // Step 1: Process staging post-update array
    PostUpdateStaging();

    // Step 2: In-order traversal of post-update tree (+0x78)
    EntityTreeNode* treeRoot = postUpdateTreeRoot;
    if (!treeRoot) return;

    g_treeCorrupt = false;

    // Find leftmost node (in-order start)
    EntityTreeNode* node = TreeMinimum(treeRoot);
    EntityTreeNode* prev = nullptr;

    while (node && !g_treeCorrupt) {
        // CheckPtr on the node

        // Guard the node's entity-IBP key: a freed-but-still-linked node has a
        // freelist link written over +0x00, so dereferencing it faults.
        ComponentEntity* entity = node->entityIBP;
        if (PtrCorrupt(entity)) {
            FlagTreeCorrupt("entityIBP", node, entity);
            break;
        }

        // Check entity not deleted (bit 10 of flags at entity+0x40)
        if (!((entity->_entityFlags >> 10) & 1)) {
            entity->PostUpdateComponents();
        }

        // Move to in-order successor
        prev = node;
        node = TreeSuccessor(node);
    }

    if (g_treeCorrupt) ReportTreeCorrupt("PostUpdate", this, prev);
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
void EntityList::RemoveEntity(ComponentEntity** entityIBP) {
    ComponentEntity* entity = *entityIBP;


    // Read bucket index from entity+0x1F (the category byte of _handle)
    int8_t bucketIdx = static_cast<int8_t>(entity->_handle >> 24);

    // Step 1: Remove from bucket (if in one; bucket 6 means "none")
    if (bucketIdx != 6) {
        // The entity's bucket: buckets[bucketIdx] (retail `param_1 + 0x18 + idx * 0x10`)
        UpdateBucket* bucket = &buckets[bucketIdx];

        // Last element index in bucket
        uint32_t lastIdx = bucket->count - 1;

        entity = *entityIBP;

        // Entity's index within bucket (low 24 bits of +0x1C)
        uint32_t entityBucketIdx = (entity->_handle << 8) >> 8;

        if (lastIdx != entityBucketIdx) {
            // Swap last element into removed element's slot
            ComponentEntity** bucketData = bucket->data;
            bucketData[entityBucketIdx] = bucketData[lastIdx];

            // Update swapped entity's bucket index field
            ComponentEntity* swappedEntity = bucket->data[entityBucketIdx];

            // Update low 24 bits of +0x1C with new index
            swappedEntity->_handle = swappedEntity->_handle ^ ((swappedEntity->_handle ^ entityBucketIdx) & 0xFFFFFF);
        }

        // Shrink bucket array.
        // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
        // new tail zeroed. Element is the 8-byte entity InternalBasePtr (byte-copy).
        GameColl::AsArray<GameColl::Ptr8>(bucket).SetSize((int)lastIdx);
    }

    // Step 2: Remove from master array (+0x08)
    int32_t lastMasterIdx = masterArray.count - 1;

    entity = *entityIBP;
    int32_t entityMasterIdx = entity->_entityId;

    if (lastMasterIdx != entityMasterIdx) {
        // Swap last element into removed element's slot
        ComponentEntity** masterData = masterArray.data;
        masterData[entityMasterIdx] = masterData[lastMasterIdx];

        // Update swapped entity's master array index
        masterData[entityMasterIdx]->_entityId = entityMasterIdx;
    }

    // Shrink master array.
    // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
    // new tail zeroed. Element is the 8-byte entity InternalBasePtr (byte-copy).
    GameColl::AsArray<GameColl::Ptr8>(&masterArray).SetSize(lastMasterIdx);

    // Step 3: Search and remove from update tree (+0x68)
    EntityTreeNode* updateNode = updateTreeRoot;
    while (updateNode != nullptr) {
        if (*entityIBP < updateNode->entityIBP) {
            updateNode = updateNode->left;
        } else {
            if (*entityIBP <= updateNode->entityIBP) {
                break;  // Found
            }
            updateNode = updateNode->right;
        }
    }

    // Step 4: Search and remove from post-update tree (+0x78)
    EntityTreeNode* postUpdateNode = postUpdateTreeRoot;
    while (postUpdateNode != nullptr) {
        if (*entityIBP < postUpdateNode->entityIBP) {
            postUpdateNode = postUpdateNode->left;
        } else {
            if (*entityIBP <= postUpdateNode->entityIBP) {
                break;  // Found
            }
            postUpdateNode = postUpdateNode->right;
        }
    }

    // Remove found nodes from trees
    if (updateNode != nullptr) {
        WorkBST_BSTRemove_a6e0(reinterpret_cast<uintptr_t*>(&updateTreeRoot), updateNode);   // LEFT: SIG WorkBST_* (Work/exe_bodies.h, wave X)
        EntityList_MarkUpdateTreeDirty();
    }
    if (postUpdateNode != nullptr) {
        WorkBST_BSTRemove_a6e0(reinterpret_cast<uintptr_t*>(&postUpdateTreeRoot), postUpdateNode);   // LEFT: SIG WorkBST_* (Work/exe_bodies.h, wave X)
    }

    // Step 5: Delete the entity IBP reference. Object::InternalBasePtr::DeletePtr
    // (0x1800c0150) on the slot: delete the object through its virtual dtor, then
    // null the slot.
    if (ComponentEntity* dying = *entityIBP) {
        delete dying;
        *entityIBP = nullptr;
    }
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

void EntityList::Serialize(IO::Stream& stream) {
    // Step 1: Construct Serializer (version 0; destroyed at the end, as retail)
    IO::Serializer serializer(stream, 0);

    // Step 2: Flush staging arrays
    PreUpdate();
    PostUpdateStaging();

    // Step 3: On save (stream+0x08 == 1), process removal list
    if (static_cast<int>(stream.GetDirection()) == 1) {   // IO::Stream::GetDirection, retail 0x180006200
        int32_t removalCount = removalList.count;
        if (removalCount > 0) {
            for (int64_t i = 0; i < removalCount; i++) {
                ComponentEntity* localIBP = removalList.data[i];
                RemoveEntity(&localIBP);
            }
            // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8
            // align 8, new tail zeroed. Element is the entity InternalBasePtr.
            GameColl::AsArray<GameColl::Ptr8>(&removalList).SetSize(0);
        }
    }

    accumulatedTime.Serialize(serializer);

    // Step 5: Serialize master array count
    int32_t masterCount = masterArray.count;
    int32_t serializedCount = masterCount;
    serializer.Serialize(serializedCount, nullptr);

    // On load (mode == 0): resize master array to deserialized count
    if (static_cast<int>(serializer.GetDirection()) == 0) {   // IO::Serializer::GetDirection, retail 0x180006210
        // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
        // new tail zeroed. Element is the 8-byte entity InternalBasePtr (byte-copy).
        GameColl::AsArray<GameColl::Ptr8>(&masterArray).SetSize(serializedCount);
    }

    // Serialize each entity IBP in master array
    ComponentEntity** ibpIter = masterArray.data;
    int64_t ibpCount = (int64_t)serializedCount;
    if (ibpCount > 0) {
        for (int64_t j = 0; j < ibpCount; j++) {
            ComponentEntity** currentIBP = &ibpIter[j];
            reinterpret_cast<Object::InternalBasePtr*>(currentIBP)->Serialize(serializer);   // B.3.4 bridge   // LEFT: IBPArray holds bare ComponentEntity* (H5 raw triple)

            // After loading, verify it's a ComponentEntity
            if (*currentIBP != nullptr) {
                bool isDerived = ::Object::IsDerivedFromNullSafe(*currentIBP, ComponentEntity::TypeId());
                if (!isDerived) {
                    *currentIBP = nullptr;
                }
            }
        }
    }

    // Step 6: Serialize bucket trees via helper (5 buckets starting at +0x18)
    BucketTree_Serialize_21ba0(&serializer, nullptr, &buckets[0]);   // 0x140021ba0

    // Step 7: Serialize processCount[5] and cursor[5] as bulk arrays
    // Retail (0x1400c45d0 / 0x1400c45e8) calls dispatch slot 0x140151278 =
    // IO::Serializer::Serialize(int count, int* data, name) — the INT-array
    // overload, 20 bytes per call. We were calling slot 0x140152400
    // (fn_Serializer_SerializeBulk), the BYTE overload, which read only 5 bytes
    // each and left the stream 30 bytes short for every load — silently
    // desyncing Geometry and everything after it. The offsets prove the width:
    // processCount is at +0xA8 and cursor at +0xBC, exactly 5 ints apart.
    serializer.Serialize(5, processCount, nullptr);
    serializer.Serialize(5, cursor, nullptr);

    // Step 8: Per-entity metadata serialization
    int32_t entityIdx = 0;
    if (masterArray.count > 0) {
        do {
            // Read master array index from entity+0x18
            int32_t masterIdxField = masterArray.data[entityIdx]->_entityId;

            // Read bucket index (low 24 bits of entity+0x1C)
            int32_t bucketIdxField = static_cast<int32_t>((masterArray.data[entityIdx]->_handle << 8) >> 8);

            // Read bucket ID from entity+0x1F (the category byte of _handle)
            int32_t bucketId = (int32_t)static_cast<int8_t>(masterArray.data[entityIdx]->_handle >> 24);

            // Serialize the three metadata fields
            serializer.Serialize(masterIdxField, nullptr);
            serializer.Serialize(bucketIdxField, nullptr);
            serializer.Serialize(bucketId, nullptr);

            // Serialize entity's timestamp at +0x20 (System::Time)
            masterArray.data[entityIdx]->_lastUpdateTime.Serialize(serializer);

            // Write back deserialized metadata to entity
            masterArray.data[entityIdx]->_entityId = masterIdxField;

            ComponentEntity* ent = masterArray.data[entityIdx];
            ent->_handle = ent->_handle ^ ((static_cast<uint32_t>(bucketIdxField) ^ ent->_handle) & 0xFFFFFF);

            // The category byte +0x1F: retail stores the BYTE. ComponentEntity.h declares
            // +0x1C..+0x1F as one uint32 `_handle`: LEFT for the header's owner.
            reinterpret_cast<int8_t*>(&masterArray.data[entityIdx]->_handle)[3] = (int8_t)bucketId;   // LEFT: _handle's byte store vs its 32-bit sites

            entityIdx++;
        } while (entityIdx < masterArray.count);
    }

    // Step 9: the Serializer's destructor runs here
}

// ============================================================
// B4 FORWARD: the C entry point of the one member with callers outside this
// block's files (entitylist_funcs.h). The closing wave rewires the callers and
// deletes it.
// ============================================================
void EntityList_RemoveEntity(EntityList* self, void* entityIBP) {
    self->RemoveEntity(reinterpret_cast<ComponentEntity**>(entityIBP));   // LEFT: forwarder for gameupdateloop_teardown.cpp (wave X)
}
