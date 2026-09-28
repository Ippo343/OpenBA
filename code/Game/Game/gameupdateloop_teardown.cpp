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
//   0x1400c2ea0  ActHunt_MarkEntityRemoval_c2ea0   (Action/exe_bodies.h)
//   0x1400c38e0  EntityList_RemoveEntity           (Entity/entitylist_funcs.h)
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
#include "resolve.h"           // RegisterExeAddr
#include "Action/exe_bodies.h"          // ActHunt_MarkEntityRemoval_c2ea0
#include "Entity/entitylist_funcs.h"     // EntityList_RemoveEntity
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

namespace {

inline uint8_t*  B(void* p)               { return reinterpret_cast<uint8_t*>(p); }
inline void*&    P(void* base, int off)   { return *reinterpret_cast<void**>(B(base) + off); }
inline int32_t&  I32(void* base, int off) { return *reinterpret_cast<int32_t*>(B(base) + off); }
inline int64_t&  I64(void* base, int off) { return *reinterpret_cast<int64_t*>(B(base) + off); }

inline void MemFree(void* p) { if (p) System::MemoryAllocator::MemFree(p); }

// B.3.2 bridge: the owning structs are still untyped bytes.
inline rtcoll::ArrayHdr&          Arr   (void* base, int off) { return *reinterpret_cast<rtcoll::ArrayHdr*>(B(base) + off); }
inline Collection::Array<void*>&  PtrArr(void* base, int off) { return *reinterpret_cast<Collection::Array<void*>*>(B(base) + off); }

// MSVC's __ehvec_dtor_iter: destroy `count` elements of `size` bytes in REVERSE
// order. Retail reaches these through the CRT helper at 0x14003ad30; the
// iteration is the whole content, so it is inlined here rather than ported.
template <typename Dtor>
void VectorDtorIter(void* first, int size, int count, Dtor dtor) {
    for (int i = count - 1; i >= 0; --i) dtor(B(first) + (size_t)size * i);
}

} // namespace

// ============================================================
// 0x140001740 — {ptr,count} array clear (element dtor of the 5-bucket array)
// ============================================================
extern "C" void Array8_Clear_1740(void* hdrV) {
    Arr(hdrV, 0x00).~Array();   // retail: MemFree; data = 0; count + capacity in one 8-byte store
}

// ============================================================
// 0x1400174e0 — recursive node free, children +0x20/+0x28, owns a buffer at +0x08
// ============================================================
extern "C" void TreeNodeFree_174e0(void* hdrV, void* nodeV) {
    if (!nodeV) return;
    TreeNodeFree_174e0(hdrV, P(nodeV, 0x20));
    TreeNodeFree_174e0(hdrV, P(nodeV, 0x28));
    Arr(nodeV, 0x08).~Array();
    MemFree(nodeV);
}

// ============================================================
// 0x1400173d0 — clear a {root,count} tree of TreeNodeFree_174e0 nodes
// ============================================================
extern "C" void Tree_Clear_173d0(void* hdrV) {
    void* root = P(hdrV, 0x00);
    if (root) {
        TreeNodeFree_174e0(hdrV, P(root, 0x20));
        TreeNodeFree_174e0(hdrV, P(root, 0x28));
        Arr(root, 0x08).~Array();
        MemFree(root);
    }
    P(hdrV, 0x00) = nullptr;
    I32(hdrV, 0x08) = 0;
}

// ============================================================
// 0x1400175d0 — array of {ptr,count} sub-arrays: free each, then the array
// ============================================================
extern "C" void ArrayOfArrays_Clear_175d0(void* hdrV) {
    // Array<ArrayHdr>::~Array — the trait-derived row's destroy is ~ArrayHdr on
    // each of `count` 0x10-byte elements in ascending order (retail's loop),
    // then ArrayDtorImpl on the outer header.
    reinterpret_cast<Collection::Array<rtcoll::ArrayHdr>*>(hdrV)->~Array();   // B.3.2 bridge
}

// ============================================================
// 0x1400171b0 — {array@+0x00, tree@+0x18} pair clear
// ============================================================
extern "C" void ArrayPlusTree_Clear_171b0(void* selfV) {
    void* treeHdr = B(selfV) + 0x18;
    void* root = P(treeHdr, 0x00);
    if (root) {
        WorkBase_TreeNodeFree_e320(treeHdr, P(root, 0x18));
        WorkBase_TreeNodeFree_e320(treeHdr, P(root, 0x20));
        MemFree(root);
    }
    P(treeHdr, 0x00) = nullptr;
    I32(treeHdr, 0x08) = 0;
    Arr(selfV, 0x00).~Array();
}

// ============================================================
// 0x14000dcb0 — {root,count} tree clear, children +0x18/+0x20
// ============================================================
extern "C" void Tree_Clear_dcb0(void* hdrV) {
    void* root = P(hdrV, 0x00);
    if (root) {
        WorkBase_TreeNodeFree_e320(hdrV, P(root, 0x18));
        WorkBase_TreeNodeFree_e320(hdrV, P(root, 0x20));
        MemFree(root);
    }
    P(hdrV, 0x00) = nullptr;
    I32(hdrV, 0x08) = 0;
}

// ============================================================
// 0x14002cf30 — node-owned sub-tree clear (sub-header at node+0x08)
// ============================================================
extern "C" void NodeSubTree_Clear_2cf30(void* nodeV) {
    void* subHdr = B(nodeV) + 0x08;
    void* root = P(subHdr, 0x00);
    if (root) {
        WorkBase_TreeNodeFree_e320(subHdr, P(root, 0x18));
        WorkBase_TreeNodeFree_e320(subHdr, P(root, 0x20));
        MemFree(root);
    }
    P(subHdr, 0x00) = nullptr;
    I32(nodeV, 0x10) = 0;
}

// ============================================================
// 0x14002ce60 — recursive node free (children +0x20/+0x28), each node owning a
// sub-tree cleared by NodeSubTree_Clear_2cf30
// ============================================================
extern "C" void TreeNodeFree_2ce60(void* hdrV, void* nodeV) {
    if (!nodeV) return;
    TreeNodeFree_2ce60(hdrV, P(nodeV, 0x20));
    TreeNodeFree_2ce60(hdrV, P(nodeV, 0x28));
    NodeSubTree_Clear_2cf30(nodeV);
    MemFree(nodeV);
}

// ============================================================
// 0x14002df90 — recursive node free (children +0x20/+0x28), IBP payload at +0x10
// ============================================================
extern "C" void TreeNodeFree_2df90(void* hdrV, void* nodeV) {
    if (!nodeV) return;
    TreeNodeFree_2df90(hdrV, P(nodeV, 0x20));
    TreeNodeFree_2df90(hdrV, P(nodeV, 0x28));
    if (P(nodeV, 0x10) != nullptr)
        reinterpret_cast< ::Object::InternalBasePtr*>(B(nodeV) + 0x10)->DeletePtr();   // B.3.3 bridge
    MemFree(nodeV);
}

// ============================================================
// 0x14002de90 — clear a {root,count} tree of IBP-payload nodes
// ============================================================
extern "C" void IbpTree_Clear_2de90(void* hdrV) {
    void* root = P(hdrV, 0x00);
    if (root) {
        TreeNodeFree_2df90(hdrV, P(root, 0x20));
        TreeNodeFree_2df90(hdrV, P(root, 0x28));
        if (P(root, 0x10) != nullptr)
            reinterpret_cast< ::Object::InternalBasePtr*>(B(root) + 0x10)->DeletePtr();   // B.3.3 bridge
        MemFree(root);
    }
    P(hdrV, 0x00) = nullptr;
    I32(hdrV, 0x08) = 0;
}

// ============================================================
// 0x1400c3d30 — EntityList::Shutdown
// ============================================================
// Sets the "list closed" flag at +0xE0 (which makes entity registration DELETE
// instead of file — see spawncomponent.cpp), marks every live entity for removal,
// drains the pending-removal array, then removes whatever is left in the master
// array until it is empty. The flag is cleared again on the way out.
extern "C" void EntityList_Shutdown_c3d30(void* selfV) {
    uint8_t* self = B(selfV);

    *(self + 0xE0) = 1;

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
    for (int32_t i = 0; i < I32(self, 0x10); ++i) {
        void* slot = B(P(self, 0x08)) + (size_t)i * 8;
        void* entity = P(slot, 0x00);
        if ((*reinterpret_cast<uint32_t*>(B(entity) + 0x40) >> 10 & 1) == 0) {
            ActHunt_MarkEntityRemoval_c2ea0(P(slot, 0x00));   // 0x1400c2ea0
        }
        if (bracket && !bracketFired) {
            if (void* bad = RedzoneAlloc_FindCorruptBlockNative()) {
                bracketFired = true;
                std::fprintf(stderr,
                    "[TeardownBracket] *** HEAP WENT CORRUPT *** mark step=%d "
                    "corruptBlock=%p tag=0x%016llx | the step that just ran is the "
                    "writer: entity=%p entityVtable=0x%016llx\n",
                    (int)i, bad, (unsigned long long)*reinterpret_cast<uint64_t*>(bad),
                    entity,
                    (unsigned long long)(entity ? *reinterpret_cast<uint64_t*>(entity) : 0));
                std::fflush(stderr);
                // Name the owner of the block written past, repair the tag, arm a
                // watchpoint on the dword (TEARDOWN_WILD_WRITE.md section 3).
                RedzoneAlloc_ReportWildWrite(bad);
            }
        }
    }

    // Drain the pending-removal array (+0xD0 data / +0xD8 count).
    for (int32_t i = 0; i < I32(self, 0xD8); ++i) {
        void* ibp = *reinterpret_cast<void**>(B(P(self, 0xD0)) + (size_t)i * 8);
        EntityList_RemoveEntity(static_cast<EntityList*>(selfV), &ibp);   // 0x1400c38e0
    }
    PtrArr(self, 0xD0).~Array();

    // Whatever is still in the master array goes now. RemoveEntity shrinks the
    // array, so retail re-reads the count every pass and always takes slot 0.
    while (I32(self, 0x10) > 0) {
        void* ibp = *reinterpret_cast<void**>(P(self, 0x08));
        EntityList_RemoveEntity(static_cast<EntityList*>(selfV), &ibp);   // 0x1400c38e0
    }

    *(self + 0xE0) = 0;
}

// ============================================================
// 0x1400c3e60 — EntityList::~EntityList
// ============================================================
extern "C" void EntityList_Dtor_c3e60(void* selfV) {
    uint8_t* self = B(selfV);

    EntityList_Shutdown_c3d30(selfV);

    PtrArr(self, 0xD0).~Array();   // removal list
    PtrArr(self, 0x98).~Array();   // staging post-update
    PtrArr(self, 0x88).~Array();   // staging update

    // post-update tree (+0x78) then update tree (+0x68): nodes with kids +0x10/+0x18
    for (int off : {0x78, 0x68}) {
        void* hdr = self + off;
        void* root = P(hdr, 0x00);
        if (root) {
            WorkBase_TreeNodeFree_14c30(hdr, P(root, 0x10));
            WorkBase_TreeNodeFree_14c30(hdr, P(root, 0x18));
            MemFree(root);
        }
        P(hdr, 0x00) = nullptr;
        I32(hdr, 0x08) = 0;
    }

    // 5 round-robin update buckets at +0x18, 0x10 bytes each
    VectorDtorIter(self + 0x18, 0x10, 5, [](void* e) { Array8_Clear_1740(e); });

    PtrArr(self, 0x08).~Array();   // master array
}

// ============================================================
// 0x1400907b0 — CitizenList::~CitizenList
// ============================================================
// A long run of {ptr,count} arrays at qword indices 0..0x31, plus a few
// structured members: a tree-owning object at [0x37], trees at [0x2c] / [0x20],
// and two array-of-arrays at [0x2a] / [0x28]. Retail frees strictly back to front.
extern "C" void CitizenList_Dtor_907b0(void* selfV) {
    void** self = reinterpret_cast<void**>(selfV);

    if (self[0x37]) {
        ArrayPlusTree_Clear_171b0(self[0x37]);
        MemFree(self[0x37]);
    }

    // Qword index i is the header at byte offset i*8; the element types are not
    // pinned here (citizenlist_serialize.cpp owns them), so ArrayHdr throughout.
    Arr(self, 0x30 * 8).~Array();
    Arr(self, 0x2e * 8).~Array();

    Tree_Clear_173d0(self + 0x2c);
    ArrayOfArrays_Clear_175d0(self + 0x2a);
    ArrayOfArrays_Clear_175d0(self + 0x28);

    Arr(self, 0x26 * 8).~Array();
    Arr(self, 0x24 * 8).~Array();
    Arr(self, 0x22 * 8).~Array();

    Tree_Clear_173d0(self + 0x20);

    for (int idx = 0x1e; idx >= 0; idx -= 2)
        Arr(self, idx * 8).~Array();
}

// ============================================================
// 0x1401024d0 — EntityUI::~EntityUI
// ============================================================
extern "C" void EntityUI_Dtor_1024d0(void* selfV) {
    uint8_t* self = B(selfV);

    // Active tool (+0x28): Tool slot 5 Shutdown (0x140102505: RCX only), then
    // drop the IBP.
    if (P(self, 0x28) != nullptr) {
        void* tool = P(self, 0x28);
        reinterpret_cast<Tool*>(tool)->Shutdown();   // B.3.x bridge (slot 5)
        reinterpret_cast< ::Object::InternalBasePtr*>(self + 0x28)->DeletePtr(
            );   // B.3.3 bridge
    }

    Arr(self, 0x70).~Array();

    // Two trees with kids at +0x20/+0x28 (+0x60 then +0x50).
    for (int off : {0x60, 0x50}) {
        void* hdr = self + off;
        void* root = P(hdr, 0x00);
        if (root) {
            TradeWork_helper_14b00(reinterpret_cast<int64_t>(hdr), P(root, 0x20));
            TradeWork_helper_14b00(reinterpret_cast<int64_t>(hdr), P(root, 0x28));
            MemFree(root);
        }
        P(hdr, 0x00) = nullptr;
        I32(hdr, 0x08) = 0;
    }

    // Saved-camera-view tree (+0x10): nodes carry an IBP payload at +0x10.
    IbpTree_Clear_2de90(self + 0x10);
}

// ============================================================
// 0x1400f8ff0 — Tutorial::~Tutorial
// ============================================================
extern "C" void Tutorial_Dtor_f8ff0(void* selfV) {
    uint8_t* self = B(selfV);

    // In-order walk of the goal tree at +0x30 (parent +0x10, left +0x18,
    // right +0x20), dropping each node's IBP payload at +0x08. The nodes
    // themselves are freed by the vector-dtor pass further down, not here.
    {
        uint8_t* node = P(self, 0x30) ? B(P(self, 0x30)) : nullptr;
        while (node && P(node, 0x18)) node = B(P(node, 0x18));
        while (node) {
            reinterpret_cast< ::Object::InternalBasePtr*>(node + 0x08)->DeletePtr(
                );   // B.3.3 bridge
            uint8_t* right = P(node, 0x20) ? B(P(node, 0x20)) : nullptr;
            if (right) {
                node = right;
                while (P(node, 0x18)) node = B(P(node, 0x18));
            } else {
                uint8_t* parent = P(node, 0x10) ? B(P(node, 0x10)) : nullptr;
                while (parent && P(parent, 0x20) == node) {
                    node = parent;
                    parent = P(node, 0x10) ? B(P(node, 0x10)) : nullptr;
                }
                node = parent;
            }
        }
    }

    // Tree at +0x120 whose nodes each own a sub-tree (kids +0x20/+0x28).
    {
        void* hdr = self + 0x120;
        void* root = P(hdr, 0x00);
        if (root) {
            TreeNodeFree_2ce60(hdr, P(root, 0x20));
            TreeNodeFree_2ce60(hdr, P(root, 0x28));
            NodeSubTree_Clear_2cf30(root);
            MemFree(root);
        }
        P(hdr, 0x00) = nullptr;
        I32(hdr, 0x08) = 0;
    }

    // 15 tree headers of 0x10 bytes at +0x30, cleared back to front.
    VectorDtorIter(self + 0x30, 0x10, 0xF, [](void* e) { Tree_Clear_dcb0(e); });

    // Tree at +0x20 (kids +0x18/+0x20).
    {
        void* hdr = self + 0x20;
        void* root = P(hdr, 0x00);
        if (root) {
            WorkBase_TreeNodeFree_e320(hdr, P(root, 0x18));
            WorkBase_TreeNodeFree_e320(hdr, P(root, 0x20));
            MemFree(root);
        }
        P(hdr, 0x00) = nullptr;
        I32(hdr, 0x08) = 0;
    }

    // Tree at +0x10 (kids +0x10/+0x18).
    {
        void* hdr = self + 0x10;
        void* root = P(hdr, 0x00);
        if (root) {
            WorkBase_TreeNodeFree_14c30(hdr, P(root, 0x10));
            WorkBase_TreeNodeFree_14c30(hdr, P(root, 0x18));
            MemFree(root);
        }
        I32(hdr, 0x08) = 0;
        P(hdr, 0x00) = nullptr;
    }
}
