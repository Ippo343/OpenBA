// code/Game/Work/work_base_bst.cpp
// Shared Work "simple BST" (AVL) family — citizen-AI work-tracking tree.
//
// These are the exe's inlined Collection::Set instances for an 8-byte key (an
// Object pointer compared as an unsigned 64-bit value): EntityList's update trees,
// the Work leaves' citizen sets (WorkCitizenSetNode), the scheduler's order tree.
#include "Work/exe_bodies.h"
#include "Work/KeyBst.h"   // WorkBST_Retrace (0x14001a8c0), the remove templates
//
// All functions are faithful 1:1 ports of the decompiled exe helpers (Ghidra-verified).
//
// Node (Collection::Set<uint64_t>::Node, 0x28 bytes):
//   +0x00  uint64  element      (the key == the pointer an InternalPtr holds)
//   +0x08  ptr     parent
//   +0x10  ptr     left child
//   +0x18  ptr     right child
//   +0x20  int16   balance      (= rightHeight - leftHeight)
//   +0x22  int16   height       (leaf = 0; node = max(childHeight)+1 encoded as child.height+1)
// Tree: the Set's header, _root (+0x00) and _count (+0x08).
//
// LEFT (every entry point): ONE retail body serves every 8-byte-key set (the Work
// leaves' citizen sets, EntityList's two update trees, the leisure scan's local),
// and every caller holds its set as a raw root + count (H5: Collection::Set has a
// dtor). Each body views the set, the key and the node once, at the top.

#include "object/TypeInfo.h"
#include "system/MemoryAllocator.h"
#include "io/Serializer.h"     // ENDGAME B.3.4 caller batch C
#include "object/Internal.h"   // ENDGAME B.3.4a closing: InternalBasePtr::Serialize
#include "collection/Collection.h"
#include "Component/CitizenComponent.h"   // ENDGAME B.3.9 D5: CitizenComponent::TypeId()
#include <cstdint>

namespace {
using KeySet = Collection::Set<uint64_t>;
using Node   = KeySet::Node;
}  // namespace

// ============================================================
// Internal AVL primitives (static; not registered)
// ============================================================

// The retrace 0x14001a8c0 and its rotations 0x140014c70 / 0x140014da0 are templates over
// the node type (Work/KeyBst.h, B8 D5); the insert below instantiates them on Node.

// Reimpl of FUN_140021b40 — allocate + init a 0x28-byte leaf (key from *key).
static Node* AllocNode_1b40(const uint64_t* key, Node* parent) {
    Node* node = static_cast<Node*>(System::MemoryAllocator::MemAlloc(0x28, 8));
    if (!node) return nullptr;
    node->element = *key;
    node->parent = parent;
    node->left = nullptr;
    node->right = nullptr;
    node->balance = 0;   // balance + height: one dword store in retail
    node->height = 0;
    return node;
}

// ============================================================
// Registered helpers
// ============================================================

// Reimpl of CitizenAI_impl_1a30 (FUN_140021a30) — AVL insert.
// Original prototype: void* fn(longlong* root, void* outResult, IBP* keyIBP).
// key = *keyIBP; the inserted (or pre-existing) node is written to *outResult; the
// function returns outResult. Most callers typedef it `void fn(root, outResult, key)`
// (ignoring the return); some as `void* fn(treeRoot, outResult, keyData)`.
void* WorkBST_RegisterWork_1a30(uintptr_t* root, void* outResult, void* keyIBP) {
    KeySet* set = reinterpret_cast<KeySet*>(root);   // one body over every caller's 8-byte-key set (banner)  // CAST-OK: elem_stride
    const uint64_t* key = reinterpret_cast<const uint64_t*>(keyIBP);   // one body over every caller's 8-byte-key set (banner)  // CAST-OK: elem_stride
    Node** pOut = reinterpret_cast<Node**>(outResult);   // one body over every caller's 8-byte-key set (banner)  // CAST-OK: elem_stride
    if (set->_root == nullptr) {
        Node* node = AllocNode_1b40(key, nullptr);
        set->_root = node;
        *pOut = node;
    } else {
        Node* cur = set->_root;
        Node* newLeaf = nullptr;
        for (;;) {
            // descend left while key < node->key
            for (;;) {
                if (*key < cur->element) {
                    if (cur->left == nullptr) {
                        newLeaf = AllocNode_1b40(key, cur);
                        cur->left = newLeaf;
                        goto done_insert;
                    }
                    cur = cur->left;
                } else {
                    break;
                }
            }
            if (*key <= cur->element) {
                *pOut = cur;   // key already present
                return outResult;
            }
            if (cur->right == nullptr) {
                newLeaf = AllocNode_1b40(key, cur);
                cur->right = newLeaf;
                goto done_insert;
            }
            cur = cur->right;
        }
    done_insert:
        Node* retraceFrom = cur;   // parent of the new leaf
        WorkBST_Retrace(&set->_root, &retraceFrom);
        *pOut = newLeaf;
    }
    set->_count += 1;   // count++
    return outResult;
}

// 0x14001a6e0 (remove) and 0x140025520 (find-remove): templates over the node type,
// Work/KeyBst.h (B8 D5).

// Reimpl of CitizenAI_impl_96b0 (FUN_1400296b0) — serialize the simple BST.
// Stream order: count(int), then `count` IBP entries. LOAD path reads each IBP
// (type-checked vs CitizenComponent 0x1401c7158) and inserts via RegisterWork. SAVE
// path is an iterative in-order traversal using parent pointers, writing each node's
// IBP (at node+0). Serializer read/write mode = *(int*)(*serializer + 8) == 0 (read).
// Caller: void fn(void* bstRoot, void* serializer).
void WorkBST_BST_Serialize_296b0(uintptr_t* root, IO::Serializer* ser) {
    KeySet* set = reinterpret_cast<KeySet*>(root);   // one body over every caller's 8-byte-key set (banner)  // CAST-OK: elem_stride
    int count = set->_count;
    ser->Serialize(count, nullptr);
    bool loading = (static_cast<int>(ser->GetDirection()) == 0);   // IO::Serializer::GetDirection, retail 0x180006210
    uint32_t citTid = CitizenComponent::TypeId();  // DAT_1401c7158

    if (loading) {
        for (int i = 0; i < count; i++) {
            // The key is an InternalPtr retail serializes through InternalBasePtr::Serialize.
            Object::InternalPtr<CitizenComponent> keyIBP;
            keyIBP.InternalBasePtr::Serialize(*ser);
            if (keyIBP.Get() != nullptr && citTid &&
                !Object::IsDerivedFromNullSafe(keyIBP.Get(), citTid))
                keyIBP = Object::InternalPtr<CitizenComponent>();
            Node* out = nullptr;
            WorkBST_RegisterWork_1a30(root, &out, &keyIBP);
        }
        return;
    }

    // SAVE: iterative in-order traversal via parent links.
    Node* pIVar6 = set->_root;
    if (pIVar6 == nullptr) return;
    Node* pIVar3;
    Node* pIVar1;
    do { pIVar3 = pIVar6; pIVar6 = pIVar3->left; } while (pIVar6 != nullptr);  // leftmost
joined:
    while ((pIVar6 = pIVar3) != nullptr) {
        // The key is an InternalPtr retail serializes through InternalBasePtr::Serialize.
        reinterpret_cast<Object::InternalBasePtr*>(&pIVar6->element)->Serialize(*ser);   // the core's Set<uint64_t> element is the InternalPtr retail serializes (banner)  // CAST-OK: elem_stride
        if (pIVar6->element != 0 && citTid &&
            !Object::IsDerivedFromNullSafe(reinterpret_cast<Object::Internal*>(pIVar6->element), citTid))   // the core's Set<uint64_t> element is the InternalPtr<CitizenComponent> retail type-checks after loading it  // CAST-OK: elem_stride
            pIVar6->element = 0;
        pIVar1 = pIVar6->right;   // right child
        if (pIVar1 == nullptr) {
            pIVar3 = pIVar6->parent;  // parent
            if (pIVar3 != nullptr) goto succ;
            goto joined;                                         // root reached (pIVar3==0) -> terminate
        }
        do { pIVar3 = pIVar1; pIVar1 = pIVar3->left; } while (pIVar1 != nullptr);  // leftmost of right subtree
    }
    return;
succ:
    if (pIVar3->left != pIVar6) {
        for (;;) {
            pIVar1 = pIVar3;
            if (pIVar1 == nullptr || pIVar1->right != pIVar6) break;
            pIVar6 = pIVar1;
            pIVar3 = pIVar1->parent;
        }
        pIVar3 = pIVar6->parent;
    }
    goto joined;
}
