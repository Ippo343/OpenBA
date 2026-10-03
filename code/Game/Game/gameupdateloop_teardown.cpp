// code/Game/Game/gameupdateloop_teardown.cpp
// GameUpdateLoop::Shutdown teardown chain.
//
// GameUpdateLoop::Shutdown (0x1400977C0) runs whenever a live game goes away:
// quitting to the menu, and — the reason this file exists — loading a save from
// inside a running game, which tears the current loop down before deserializing
// the new one. Our port of Shutdown was complete, but five of the functions it
// calls unguarded had no body anywhere in the tree, so ResolveAddr handed back 0
// and the first one reached died with EXEC-of-0:
//
//   0x1400c3d30  EntityList::Shutdown
//   0x1400c3e60  EntityList::~EntityList
//   0x1400907b0  CitizenList::~CitizenList
//   0x1401024d0  EntityUI::~EntityUI
//   0x1400f8ff0  Tutorial::~Tutorial
//
// Everything here is teardown: array frees, BST frees, and one entity-removal
// drain. The container-free leaves are shared with other subsystems, so they are
// registered at their own retail addresses too rather than kept file-local.
//
// Already ported elsewhere, called by name (B3R):
//   0x1400c2ea0  MarkEntityForRemoval              (Action/exe_bodies.h)
//   0x1400c38e0  EntityList::RemoveEntity          (Entity/EntityList.h)
//   0x14000e320  WorkBase_TreeNodeFree_e320, kids +0x18/+0x20   (Work/exe_bodies.h)
//   0x140014c30  WorkBase_TreeNodeFree_14c30, kids +0x10/+0x18  (Work/exe_bodies.h)
//   0x140014b00  TradeWork_helper_14b00, kids +0x20/+0x28       (Player/exe_bodies.h)

// ENDGAME B.3.2 step 2 (COLL-GAME sweep): every `MemFree(data); data = 0; one
// qword over size+capacity = 0` triple below was a hand-inlined
// `Collection::Array<T>::~Array` for a trivially-destructible T
// (`rtcoll::ArrayDtorImpl`, byte for byte). Each is now `->~Array()` on the
// header, spelled `rtcoll::ArrayHdr` (= Array<uint8_t>) where the element type
// is not pinned in this file and `Array<void*>` where it is a pointer list.
// `ArrayOfArrays_Clear_175d0` is `Array<ArrayHdr>::~Array`: the trait-derived
// row destroys each inner header (ascending, as retail's loop does) and then
// frees the outer block. Trees ({root,count} pairs, 4-byte count store),
// node chains and IBP payloads are NOT arrays and are untouched.
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "Action/exe_bodies.h"          // MarkEntityForRemoval
#include "Player/exe_bodies.h"          // TradeWork_helper_14b00
#include "Work/exe_bodies.h"            // WorkBase_TreeNodeFree_e320 / _14c30
#include "options.h"         // Opt().teardownHeapCheck
#include "redzone_alloc.h"   // RedzoneAlloc_FindCorruptBlockNative
#include "collection/Collection.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Player/Tool.h"       // B.3.x W0: ~EntityUI shuts the active tool down by name
#include "Game/exe_bodies.h"
#include "Entity/EntityList.h"          // EntityList (0xE8), its trees and buckets
#include "Entity/ComponentEntity.h"     // ComponentEntity::_entityFlags
#include "Entity/CitizenBehavior.h"     // CitizenBehavior (0x1C0), PathGraph (0x28)
#include "Game/PlayerToolManager.h"     // PlayerToolManager (0xB0), TrackedEntityNode
#include "Game/WorkSchedulerNodes.h"    // WorkScheduler (0x138) and its three node types

// B.4 P2 GAME-2: the five teardown bodies take their object typed and read it by
// name. What still goes through the offset helpers below is LEFT, each site with
// its reason: the arrays EntityList.h / CitizenBehavior.h / PlayerToolManager.h
// declare as three raw fields (not a Collection::Array, so `~Array` is reached
// through a cast; H5 in headers owned by ENT / PLAYER), the trees whose root is
// a `void*` there (WorkScheduler +0x20), and the tool registry whose node is defined in
// Dialog/toolbardialog.cpp.
namespace {

inline uint8_t*  B(void* p)               { return reinterpret_cast<uint8_t*>(p); }
inline void*&    P(void* base, int off)   { return *reinterpret_cast<void**>(B(base) + off); }
inline int32_t&  I32(void* base, int off) { return *reinterpret_cast<int32_t*>(B(base) + off); }

inline void MemFree(void* p) { if (p) System::MemoryAllocator::MemFree(p); }

// B.3.2 bridge: the owning structs are still untyped bytes.
inline rtcoll::ArrayHdr&          Arr   (void* base, int off) { return *reinterpret_cast<rtcoll::ArrayHdr*>(B(base) + off); }
inline Collection::Array<void*>&  PtrArr(void* base, int off) { return *reinterpret_cast<Collection::Array<void*>*>(B(base) + off); }

// MSVC's __ehvec_dtor_iter: destroy the elements of an embedded array in
// REVERSE order. Retail reaches these through the CRT helper at 0x14003ad30;
// the iteration is the whole content, so it is inlined here rather than ported.
template <typename T, int N, typename Dtor>
void VectorDtorIter(T (&elements)[N], Dtor dtor) {
    for (int i = N - 1; i >= 0; --i) dtor(&elements[i]);
}

} // namespace

// ============================================================
// 0x140001740 — {ptr,count} array clear (element dtor of the 5-bucket array)
// ============================================================
extern "C" void Array8_Clear_1740(void* hdrV) {
    // retail: MemFree; data = 0; count + capacity in one 8-byte store
    Arr(hdrV, 0x00).~Array();   // LEFT: EntityList.h (ENT): UpdateBucket is three raw fields, not a Collection::Array (H5)
}

// ============================================================
// 0x1400174e0 — recursive node free, children +0x20/+0x28, owns a buffer at +0x08
// ============================================================
// The nodes of CitizenBehavior's two entity-bucket trees (+0x100 profession,
// +0x160 template: CitizenBucketNode<Key>, Entity/CitizenBehavior.h). Retail
// has ONE body for both key types (B13): one template here.
template <class Key>
void TreeNodeFree_174e0(void* hdrV, CitizenBucketNode<Key>* node) {
    if (!node) return;
    TreeNodeFree_174e0(hdrV, node->left);
    TreeNodeFree_174e0(hdrV, node->right);
    Arr(node, 0x08).~Array();   // LEFT: CitizenBehavior.h (ENT): the bucket's entity Array is three raw fields (H5)
    MemFree(node);
}

// ============================================================
// 0x1400173d0 — clear a {root,count} tree of TreeNodeFree_174e0 nodes
// ============================================================
template <class Key>
void Tree_Clear_173d0(CitizenBucketNode<Key>*& root, int32_t& count) {
    CitizenBucketNode<Key>* node = root;
    if (node) {
        TreeNodeFree_174e0(&root, node->left);
        TreeNodeFree_174e0(&root, node->right);
        Arr(node, 0x08).~Array();   // LEFT: CitizenBehavior.h (ENT): the bucket's entity Array is three raw fields (H5)
        MemFree(node);
    }
    root = nullptr;
    count = 0;
}

// ============================================================
// 0x1400175d0 — array of {ptr,count} sub-arrays: free each, then the array
// ============================================================
extern "C" void ArrayOfArrays_Clear_175d0(void* hdrV) {
    // Array<ArrayHdr>::~Array — the trait-derived row's destroy is ~ArrayHdr on
    // each of `count` 0x10-byte elements in ascending order (retail's loop),
    // then ArrayDtorImpl on the outer header.
    reinterpret_cast<Collection::Array<rtcoll::ArrayHdr>*>(hdrV)->~Array();   // LEFT: CitizenBehavior.h (ENT) +0x140 / +0x150 and WorldGeometry::_arrays[4] are raw {data, count, capacity} fields (H5)
}

// ============================================================
// 0x1400171b0 — PathGraph's members: the distance-cache tree (+0x18), then the
// pair array (+0x00)
// ============================================================
extern "C" void ArrayPlusTree_Clear_171b0(PathGraph* self) {
    PathCostNode* root = self->m_bstRoot;
    if (root) {
        WorkBase_TreeNodeFree_e320(&self->m_bstRoot, root->left);
        WorkBase_TreeNodeFree_e320(&self->m_bstRoot, root->right);
        MemFree(root);
    }
    self->m_bstRoot = nullptr;
    self->m_bstCount = 0;
    Arr(self, 0x00).~Array();   // LEFT: CitizenBehavior.h (ENT): PathGraph::m_pairs is three raw fields, not a Collection::Array (H5)
}

// ============================================================
// 0x14000dcb0 — {root,count} tree clear, children +0x18/+0x20 (WorkScheduler's
// fifteen id trees)
// ============================================================
extern "C" void Tree_Clear_dcb0(WorkSchedulerTree* tree) {
    WorkSchedulerIdNode* root = tree->root;
    if (root) {
        WorkBase_TreeNodeFree_e320(tree, root->left);
        WorkBase_TreeNodeFree_e320(tree, root->right);
        MemFree(root);
    }
    tree->root = nullptr;
    tree->count = 0;
}

// ============================================================
// 0x14002cf30 — node-owned sub-tree clear: the bucket's own id tree (node+0x08)
// ============================================================
extern "C" void NodeSubTree_Clear_2cf30(WorkSchedulerBucketNode* node) {
    WorkSchedulerTree* subTree = &node->bucket.entries;
    WorkSchedulerIdNode* root = subTree->root;
    if (root) {
        WorkBase_TreeNodeFree_e320(subTree, root->left);
        WorkBase_TreeNodeFree_e320(subTree, root->right);
        MemFree(root);
    }
    subTree->root = nullptr;
    node->bucket.entries.count = 0;
}

// ============================================================
// 0x14002ce60 — recursive node free (children +0x20/+0x28), each node owning a
// sub-tree cleared by NodeSubTree_Clear_2cf30
// ============================================================
extern "C" void TreeNodeFree_2ce60(void* hdrV, WorkSchedulerBucketNode* node) {
    if (!node) return;
    TreeNodeFree_2ce60(hdrV, node->left);
    TreeNodeFree_2ce60(hdrV, node->right);
    NodeSubTree_Clear_2cf30(node);
    MemFree(node);
}

// ============================================================
// 0x14002df90 — recursive node free (children +0x20/+0x28), IBP payload at +0x10
// ============================================================
// (PlayerToolManager +0x10's tool registry: ToolRegistryNode is defined in
// Dialog/toolbardialog.cpp, not in a header this file can read.)
extern "C" void TreeNodeFree_2df90(void* hdrV, void* nodeV) {
    if (!nodeV) return;
    TreeNodeFree_2df90(hdrV, P(nodeV, 0x20));   // LEFT: ToolRegistryNode is file-local to Dialog/toolbardialog.cpp (DLG)
    TreeNodeFree_2df90(hdrV, P(nodeV, 0x28));   // LEFT: ToolRegistryNode is file-local to Dialog/toolbardialog.cpp (DLG)
    if (P(nodeV, 0x10) != nullptr)   // LEFT: ToolRegistryNode is file-local to Dialog/toolbardialog.cpp (DLG)
        reinterpret_cast< ::Object::InternalBasePtr*>(B(nodeV) + 0x10)->DeletePtr();   // LEFT: ToolRegistryNode is file-local to Dialog/toolbardialog.cpp (DLG)
    MemFree(nodeV);
}

// ============================================================
// 0x14002de90 — clear a {root,count} tree of IBP-payload nodes
// ============================================================
extern "C" void IbpTree_Clear_2de90(void* hdrV) {
    void* root = P(hdrV, 0x00);   // LEFT: ToolRegistryNode is file-local to Dialog/toolbardialog.cpp (DLG)
    if (root) {
        TreeNodeFree_2df90(hdrV, P(root, 0x20));   // LEFT: ToolRegistryNode is file-local to Dialog/toolbardialog.cpp (DLG)
        TreeNodeFree_2df90(hdrV, P(root, 0x28));   // LEFT: ToolRegistryNode is file-local to Dialog/toolbardialog.cpp (DLG)
        if (P(root, 0x10) != nullptr)   // LEFT: ToolRegistryNode is file-local to Dialog/toolbardialog.cpp (DLG)
            reinterpret_cast< ::Object::InternalBasePtr*>(B(root) + 0x10)->DeletePtr();   // LEFT: ToolRegistryNode is file-local to Dialog/toolbardialog.cpp (DLG)
        MemFree(root);
    }
    P(hdrV, 0x00) = nullptr;   // LEFT: ToolRegistryNode is file-local to Dialog/toolbardialog.cpp (DLG)
    I32(hdrV, 0x08) = 0;   // LEFT: ToolRegistryNode is file-local to Dialog/toolbardialog.cpp (DLG)
}

// ============================================================
// 0x1400c3d30 — EntityList::Shutdown
// ============================================================
// Sets the "list closed" flag at +0xE0 (which makes entity registration DELETE
// instead of file — see spawncomponent.cpp), marks every live entity for removal,
// drains the pending-removal array, then removes whatever is left in the master
// array until it is empty. The flag is cleared again on the way out.
extern "C" void EntityList_Shutdown_c3d30(EntityList* self) {
    self->shuttingDown = true;

    // BANISHED_TEARDOWN_HEAPCHECK brackets the mark loop. The stress town's
    // teardown crash (docs/runtime_port/TEARDOWN_WILD_WRITE.md) is ONE stray
    // 4-byte store into an allocator boundary tag, laid down inside a single
    // MarkForRemoval cascade and only tripped over hundreds of entities later,
    // in the sweep. A hardware watchpoint cannot be armed on a victim whose
    // address is unknown until after it has been written, so the boundary tags
    // are scanned after every step instead: the first step whose scan comes back
    // dirty IS the step that wrote. This is the instrument that pinned the writer
    // to mark step 13; it costs nothing when the flag is off.
    const bool bracket = Opt().teardownHeapCheck;
    bool bracketFired = false;
    if (bracket) {
        std::fprintf(stderr, "[TeardownBracket] ARMED; heap at entry is %s\n",
                     RedzoneAlloc_FindCorruptBlockNative() ? "ALREADY CORRUPT" : "clean");
        std::fflush(stderr);
    }

    // Mark every entity not already flagged deleted (entity+0x40 bit 10).
    for (int32_t i = 0; i < self->masterArray.count; ++i) {
        ComponentEntity** slot = &self->masterArray.data[i];
        ComponentEntity* entity = *slot;
        if ((entity->_entityFlags >> 10 & 1) == 0) {
            MarkEntityForRemoval(*slot);   // 0x1400c2ea0
        }
        if (bracket && !bracketFired) {
            if (void* bad = RedzoneAlloc_FindCorruptBlockNative()) {
                bracketFired = true;
                std::fprintf(stderr,
                    "[TeardownBracket] *** HEAP WENT CORRUPT *** mark step=%d "
                    "corruptBlock=%p tag=0x%016llx | the step that just ran is the "
                    "writer: entity=%p entityVtable=0x%016llx\n",
                    (int)i, bad, (unsigned long long)*reinterpret_cast<uint64_t*>(bad),  // CAST-OK: crash_walk
                    entity,
                    (unsigned long long)(entity ? *reinterpret_cast<uint64_t*>(entity) : 0));  // CAST-OK: crash_walk
                std::fflush(stderr);
                // Name the owner of the block written past, repair the tag, arm a
                // watchpoint on the dword (TEARDOWN_WILD_WRITE.md section 3).
                RedzoneAlloc_ReportWildWrite(bad);
            }
        }
    }

    // Drain the pending-removal array (+0xD0 data / +0xD8 count).
    for (int32_t i = 0; i < self->removalList.count; ++i) {
        ComponentEntity* ibp = self->removalList.data[i];
        self->RemoveEntity(&ibp);   // 0x1400c38e0
    }
    PtrArr(self, 0xD0).~Array();   // LEFT: EntityList.h (ENT): removalList is an IBPArray, not a Collection::Array (H5)

    // Whatever is still in the master array goes now. RemoveEntity shrinks the
    // array, so retail re-reads the count every pass and always takes slot 0.
    while (self->masterArray.count > 0) {
        ComponentEntity* ibp = self->masterArray.data[0];
        self->RemoveEntity(&ibp);   // 0x1400c38e0
    }

    self->shuttingDown = false;
}

// ============================================================
// 0x1400c3e60 — EntityList::~EntityList
// ============================================================
extern "C" void EntityList_Dtor_c3e60(EntityList* self) {
    EntityList_Shutdown_c3d30(self);

    PtrArr(self, 0xD0).~Array();   // removal list   // LEFT: EntityList.h (ENT): IBPArray is not a Collection::Array (H5)
    PtrArr(self, 0x98).~Array();   // staging post-update   // LEFT: EntityList.h (ENT): IBPArray is not a Collection::Array (H5)
    PtrArr(self, 0x88).~Array();   // staging update   // LEFT: EntityList.h (ENT): IBPArray is not a Collection::Array (H5)

    // post-update tree (+0x78) then update tree (+0x68): nodes with kids +0x10/+0x18
    {
        EntityTreeNode* root = self->postUpdateTreeRoot;
        if (root) {
            WorkBase_TreeNodeFree_14c30(&self->postUpdateTreeRoot, root->left);
            WorkBase_TreeNodeFree_14c30(&self->postUpdateTreeRoot, root->right);
            MemFree(root);
        }
        self->postUpdateTreeRoot = nullptr;
        self->postUpdateTreeCount = 0;
    }
    {
        EntityTreeNode* root = self->updateTreeRoot;
        if (root) {
            WorkBase_TreeNodeFree_14c30(&self->updateTreeRoot, root->left);
            WorkBase_TreeNodeFree_14c30(&self->updateTreeRoot, root->right);
            MemFree(root);
        }
        self->updateTreeRoot = nullptr;
        self->updateTreeCount = 0;
    }

    // 5 round-robin update buckets at +0x18, 0x10 bytes each
    VectorDtorIter(self->buckets, [](UpdateBucket* e) { Array8_Clear_1740(e); });

    PtrArr(self, 0x08).~Array();   // master array   // LEFT: EntityList.h (ENT): IBPArray is not a Collection::Array (H5)
}

// ============================================================
// 0x1400907b0 — CitizenList::~CitizenList
// ============================================================
// A long run of {ptr,count} arrays at qword indices 0..0x31, plus a few
// structured members: the path graph at +0x1B8, trees at +0x160 / +0x100, and
// two array-of-arrays at +0x150 / +0x140. Retail frees strictly back to front.
extern "C" void CitizenList_Dtor_907b0(CitizenBehavior* self) {
    if (self->pathGraph) {
        ArrayPlusTree_Clear_171b0(self->pathGraph);
        MemFree(self->pathGraph);
    }

    // Qword index i is the header at byte offset i*8; CitizenBehavior.h declares
    // each as three raw fields, so ArrayHdr throughout.
    Arr(self, 0x30 * 8).~Array();   // LEFT: CitizenBehavior.h (ENT): notifyEntries is three raw fields, not a Collection::Array (H5)
    Arr(self, 0x2e * 8).~Array();   // LEFT: CitizenBehavior.h (ENT): professionResources is three raw fields, not a Collection::Array (H5)

    Tree_Clear_173d0(self->field_0x160, self->field_0x168);
    ArrayOfArrays_Clear_175d0(&self->field_0x150);
    ArrayOfArrays_Clear_175d0(&self->field_0x140);

    Arr(self, 0x26 * 8).~Array();   // LEFT: CitizenBehavior.h (ENT): field_0x130 is three raw fields, not a Collection::Array (H5)
    Arr(self, 0x24 * 8).~Array();   // LEFT: CitizenBehavior.h (ENT): field_0x120 is three raw fields, not a Collection::Array (H5)
    Arr(self, 0x22 * 8).~Array();   // LEFT: CitizenBehavior.h (ENT): field_0x110 is three raw fields, not a Collection::Array (H5)

    Tree_Clear_173d0(self->field_0x100, self->field_0x108);

    for (int idx = 0x1e; idx >= 0; idx -= 2)
        Arr(self, idx * 8).~Array();   // LEFT: CitizenBehavior.h (ENT): the sixteen +0x00..+0xF0 lists are three raw fields each (H5)
}

// ============================================================
// 0x1401024d0 — EntityUI::~EntityUI
// ============================================================
// (The receiver is the 0xB0 PlayerToolManager at GameUpdateLoop+0x120; the
// name is the tree's.)
extern "C" void EntityUI_Dtor_1024d0(PlayerToolManager* self) {
    // Active tool (+0x28): Tool slot 5 Shutdown (0x140102505: RCX only), then
    // drop the IBP.
    if (self->currentTool != nullptr) {
        self->currentTool->Shutdown();   // slot 5
        reinterpret_cast< ::Object::InternalBasePtr*>(&self->currentTool)->DeletePtr(
            );   // LEFT: PlayerToolManager.h (PLAYER): currentTool is a bare Tool*, retail an InternalPtr<Tool> (H5)
    }

    Arr(self, 0x70).~Array();   // LEFT: PlayerToolManager.h (PLAYER): cameraViews is three raw fields, not a Collection::Array (H5)

    // Two trees with kids at +0x20/+0x28 (+0x60 then +0x50).
    struct EntitySet { TrackedEntityNode** root; int32_t* count; };
    for (EntitySet set : {EntitySet{&self->field_0x60, &self->field_0x68},
                          EntitySet{&self->field_0x50, &self->field_0x58}}) {
        TrackedEntityNode* root = *set.root;
        if (root) {
            TradeWork_helper_14b00(reinterpret_cast<int64_t>(set.root), root->left);   // LEFT: SIG TradeWork_helper_14b00
            TradeWork_helper_14b00(reinterpret_cast<int64_t>(set.root), root->right);   // LEFT: SIG TradeWork_helper_14b00
            MemFree(root);
        }
        *set.root = nullptr;
        *set.count = 0;
    }

    // Tool registry tree (+0x10): nodes carry an IBP payload at +0x10.
    IbpTree_Clear_2de90(&self->field_0x10);
}

// ============================================================
// 0x1400f8ff0 — Tutorial::~Tutorial
// ============================================================
// (The receiver is the 0x138 WorkScheduler at GameUpdateLoop+0x118; the name
// is the tree's.)
extern "C" void Tutorial_Dtor_f8ff0(WorkScheduler* self) {
    // In-order walk of the id tree trees[0] (+0x30), dropping each node's Work
    // (+0x08). The nodes themselves are freed by the vector-dtor pass further
    // down, not here.
    {
        WorkSchedulerIdNode* node = self->trees[0].root;
        while (node && node->left) node = node->left;
        while (node) {
            node->work.DeletePtr();
            WorkSchedulerIdNode* right = node->right;
            if (right) {
                node = right;
                while (node->left) node = node->left;
            } else {
                WorkSchedulerIdNode* parent = node->parent;
                while (parent && parent->right == node) {
                    node = parent;
                    parent = node->parent;
                }
                node = parent;
            }
        }
    }

    // Tree at +0x120 whose nodes each own a sub-tree (kids +0x20/+0x28).
    {
        WorkSchedulerBucketNode* root = self->field_0x120;
        if (root) {
            TreeNodeFree_2ce60(&self->field_0x120, root->left);
            TreeNodeFree_2ce60(&self->field_0x120, root->right);
            NodeSubTree_Clear_2cf30(root);
            MemFree(root);
        }
        self->field_0x120 = nullptr;
        self->field_0x128 = 0;
    }

    // 15 tree headers of 0x10 bytes at +0x30, cleared back to front.
    VectorDtorIter(self->trees, [](WorkSchedulerTree* e) { Tree_Clear_dcb0(e); });

    // Tree at +0x20 (kids +0x18/+0x20).
    {
        void* root = self->field_0x20;
        if (root) {
            WorkBase_TreeNodeFree_e320(&self->field_0x20, P(root, 0x18));   // LEFT: WorkScheduler.h (COMP-8): field_0x20 is void*
            WorkBase_TreeNodeFree_e320(&self->field_0x20, P(root, 0x20));   // LEFT: WorkScheduler.h (COMP-8): field_0x20 is void*
            MemFree(root);
        }
        self->field_0x20 = nullptr;
        self->field_0x28 = 0;
    }

    // Tree at +0x10 (kids +0x10/+0x18).
    {
        WorkSchedulerCountNode* root = self->field_0x10;
        if (root) {
            WorkBase_TreeNodeFree_14c30(&self->field_0x10, root->left);
            WorkBase_TreeNodeFree_14c30(&self->field_0x10, root->right);
            MemFree(root);
        }
        self->field_0x18 = 0;
        self->field_0x10 = nullptr;
    }
}
