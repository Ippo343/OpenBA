// code/Game/Work/foresterwork.cpp
// Clean C++ reimplementation of ForesterWork Work class methods.
// Source: Game/Work/ForesterWork.cpp (from profile build debug strings)
//
// ForesterWork — forester resource gathering work action. Size = 0x60.
// Inherits: Object::Internal -> Work (0x48) -> AutoPickupWork (0x48) -> ForesterWork
// Vtable: 0x1401613a0 (12 slots)
//
// ForesterWork fields (beyond AutoPickupWork base):
//   +0x48: void* bstRoot   — BST root node (tree of entity InternalBasePtrs)
//   +0x50: int   bstCount  — number of entries in the BST
//   +0x54: padding
//   +0x58: int   retryCount — serialized int field (iteration counter)
//   +0x5C: padding
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — it is portable for standalone exe.

#include "Work/foresterwork_funcs.h"
#include "ForesterWork.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Local function pointers resolved at init time
// ============================================================

// Object::Internal::~Internal(this) — via IAT at 0x140152770
static void (*s_fn_ObjectInternal_Dtor)(void* self) = nullptr;

// Work::Serialize(this, stream) — base class serialize at 0x1400f7400
static void (*s_fn_Work_Serialize)(void* self, void* stream) = nullptr;

// IO::Serializer ctor/dtor — via IAT
static void (*s_fn_Serializer_Ctor)(void* self, void* stream, int version) = nullptr;
static void (*s_fn_Serializer_Dtor)(void* self) = nullptr;

// BST serialize helper (0x1400296b0) — serializes InternalBasePtr BST
static void (*s_fn_BST_SerializeHelper)(void* bstRoot, void* serializer) = nullptr;

// BST FreeNodes (0x140014c30) — recursively frees BST subtree nodes
static void (*s_fn_BST_FreeNodes)(void* treeRoot, void* subtree) = nullptr;

// BST RemoveByKey (CitizenAI_impl_5520 at 0x140025520) — removes entity by key from BST
static void (*s_fn_BST_RemoveByKey)(void* treeRootPtr, void* keyIBP) = nullptr;

// BST Remove (CitizenAI_impl_a6e0) — removes a specific node from BST
static void (*s_fn_BST_Remove)(void* bstRoot, void* node) = nullptr;

// FUN_1400f7100 — Work::ValidateTask helper (checks if assigned task still valid)
static uint8_t (*s_fn_Work_ValidateTask)(void* self, void* param2, void* ibp, void** outTask) = nullptr;

// FUN_1400c5400 — task list cleanup (cancels pending work)
static void (*s_fn_TaskListCleanup)(void* taskList) = nullptr;

// FUN_140002e10 — FindComponent for Slot10 (finds component by type DWORD_1401c80e0)
static void* (*s_fn_Entity_FindComponent_BuildDesc)(uintptr_t entity, void* retIBP) = nullptr;

// Slot4 original function (forwarded to trampoline — too complex for clean reimpl)
static int (*s_fn_Slot4_Original)(void* self, void* param2, uint64_t param3, uint64_t param4) = nullptr;

// Cached vtable addresses
static void* s_vtable_ForesterWork = nullptr;
static void* s_vtable_AutoPickupWork = nullptr;
static void* s_vtable_Work = nullptr;

static bool s_foresterwork_funcs_resolved = false;

// ============================================================
// Resolution — called once from Install
// ============================================================
bool ForesterWork_ResolveFuncs() {
    if (s_foresterwork_funcs_resolved) return true;

    // Object::Internal::~Internal via IAT
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152770);
    if (iatDtor) s_fn_ObjectInternal_Dtor = *reinterpret_cast<void(**)(void*)>(iatDtor);

    // Work::Serialize
    s_fn_Work_Serialize = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x1400f7400));

    // IO::Serializer ctor/dtor via IAT
    uintptr_t iatSerCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    uintptr_t iatSerDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatSerCtor) s_fn_Serializer_Ctor = *reinterpret_cast<void(**)(void*, void*, int)>(iatSerCtor);
    if (iatSerDtor) s_fn_Serializer_Dtor = *reinterpret_cast<void(**)(void*)>(iatSerDtor);

    // BST serialize helper (0x1400296b0)
    s_fn_BST_SerializeHelper = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x1400296b0));

    // BST FreeNodes (0x140014c30) — recursive node freeing
    s_fn_BST_FreeNodes = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x140014c30));

    // BST RemoveByKey (0x140025520) — removes entity from BST by key comparison
    s_fn_BST_RemoveByKey = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x140025520));

    // BST Remove (0x14001a6e0) — removes a specific node from BST
    s_fn_BST_Remove = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x14001a6e0));

    // Work::ValidateTask (FUN_1400f7100)
    s_fn_Work_ValidateTask = reinterpret_cast<uint8_t(*)(void*, void*, void*, void**)>(
        ResolveAddr(Module::Exe, 0x1400f7100));

    // Task list cleanup (FUN_1400c5400)
    s_fn_TaskListCleanup = reinterpret_cast<void(*)(void*)>(
        ResolveAddr(Module::Exe, 0x1400c5400));

    // FindComponent_BuildDesc (0x140002e10)
    s_fn_Entity_FindComponent_BuildDesc = reinterpret_cast<void*(*)(uintptr_t, void*)>(
        ResolveAddr(Module::Exe, 0x140002e10));

    // Slot4 original — direct exe address for forwarding
    s_fn_Slot4_Original = reinterpret_cast<int(*)(void*, void*, uint64_t, uint64_t)>(
        ResolveAddr(Module::Exe, 0x1400EAEC0));

    // Vtables
    s_vtable_ForesterWork = (void*)ResolveAddr(Module::Exe, 0x1401613a0);
    s_vtable_AutoPickupWork = (void*)ResolveAddr(Module::Exe, 0x140160fa0);
    s_vtable_Work = (void*)ResolveAddr(Module::Exe, 0x140161a80);

    bool ok = s_fn_ObjectInternal_Dtor && s_fn_Work_Serialize &&
              s_fn_Serializer_Ctor && s_fn_Serializer_Dtor &&
              s_fn_BST_SerializeHelper && s_fn_BST_FreeNodes &&
              s_fn_BST_RemoveByKey && s_fn_BST_Remove &&
              s_fn_Work_ValidateTask && s_fn_TaskListCleanup &&
              s_fn_Entity_FindComponent_BuildDesc &&
              fn_InternalBasePtr_Ctor && fn_InternalBasePtr_CheckPtr &&
              fn_MemFreeStatic && g_fn_Serialize_Int &&
              s_vtable_ForesterWork && s_vtable_AutoPickupWork && s_vtable_Work;

#ifdef _DEBUG
    printf("[ForesterWork] ResolveFuncs: ObjDtor=%p WorkSer=%p SerCtor=%p SerDtor=%p\n",
           (void*)s_fn_ObjectInternal_Dtor, (void*)s_fn_Work_Serialize,
           (void*)s_fn_Serializer_Ctor, (void*)s_fn_Serializer_Dtor);
    printf("[ForesterWork]   BSTSer=%p BSTFree=%p BSTRemKey=%p BSTRem=%p\n",
           (void*)s_fn_BST_SerializeHelper, (void*)s_fn_BST_FreeNodes,
           (void*)s_fn_BST_RemoveByKey, (void*)s_fn_BST_Remove);
    printf("[ForesterWork]   ValidTask=%p TaskClean=%p FindBuildDesc=%p\n",
           (void*)s_fn_Work_ValidateTask, (void*)s_fn_TaskListCleanup,
           (void*)s_fn_Entity_FindComponent_BuildDesc);
    if (!ok) printf("[ForesterWork] ERROR: some function pointers not resolved!\n");
#endif

    s_foresterwork_funcs_resolved = ok;
    return ok;
}

// ============================================================
// ScalarDtor (0x14002A370)
// Calls dtor body (0x1400EBA00) which:
// 1. Sets vtable to ForesterWork
// 2. Frees BST nodes at +0x48 (left subtree, right subtree, root)
// 3. Zeros BST root and count
// 4. Sets vtable chain down: AutoPickupWork -> Work
// 5. Calls Object::Internal::~Internal
// Then conditionally frees memory if flags & 1.
// ============================================================
void* ForesterWork_ScalarDtor_Impl(ForesterWork* self, uint32_t flags) {
    // Dtor body — mirrors 0x1400EBA00

    // Set ForesterWork vtable (for correct virtual dispatch during destruction)
    self->vtable = s_vtable_ForesterWork;

    // Free BST nodes at +0x48
    void* bstRoot = self->_field48;
    if (bstRoot != nullptr) {
        // Free left subtree: node+0x10
        void* leftChild = *(void**)((uintptr_t)bstRoot + 0x10);
        s_fn_BST_FreeNodes(&self->_field48, leftChild);

        // Free right subtree: node+0x18
        void* rightChild = *(void**)((uintptr_t)bstRoot + 0x18);
        s_fn_BST_FreeNodes(&self->_field48, rightChild);

        // Free root node itself
        fn_MemFreeStatic(bstRoot);
    }

    // Zero BST fields
    self->_field48 = nullptr;
    self->_field50 = 0;

    // Set vtable chain down through base classes
    self->vtable = s_vtable_AutoPickupWork;
    self->vtable = s_vtable_Work;

    // Call Object::Internal::~Internal (tail call in original)
    s_fn_ObjectInternal_Dtor(self);

    // Conditional free (from ScalarDtor wrapper at 0x14002A370)
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }

    return self;
}

// ============================================================
// Serialize (0x1400EB970)
// Calls Work::Serialize, creates AutoPickupWork version block (empty),
// then ForesterWork version block with BST serialize + int field.
// ============================================================
void ForesterWork_Serialize_Impl(ForesterWork* self, void* stream) {
    // Serialize Work base fields
    s_fn_Work_Serialize(self, stream);

    // AutoPickupWork serializer block (version 0, empty — same as base class)
    uint8_t serBuf1[24];
    s_fn_Serializer_Ctor(serBuf1, stream, 0);
    s_fn_Serializer_Dtor(serBuf1);

    // ForesterWork serializer block (version 0)
    uint8_t serBuf2[24];
    s_fn_Serializer_Ctor(serBuf2, stream, 0);

    // Serialize the BST at +0x48 (entity reference tree)
    s_fn_BST_SerializeHelper(&self->_field48, serBuf2);

    // Serialize int field at +0x58 (retry counter)
    g_fn_Serialize_Int(serBuf2, &self->_field58, nullptr);

    s_fn_Serializer_Dtor(serBuf2);
}

// ============================================================
// Slot4 (0x1400EAEC0) — Main work-assignment logic
// 599 instructions / 377 lines decompiled. Extremely complex:
// - Checks auto-pickup assignments for entity
// - Finds forester workplace, radius component, material description
// - Calculates search area based on workplace radius
// - Random iteration over grid cells looking for valid tree spots
// - Checks terrain type, ownership, existing resources
// - Creates PerformWork actions for tree cutting or planting
// - Falls back to idle animation if no work found
//
// Forwarded to original exe function. The clean reimpl would need
// 20+ additional engine function pointers and deep understanding
// of the map grid, terrain, and resource systems.
// ============================================================
int ForesterWork_Slot4_Impl(ForesterWork* self, void* entityIBP, uint64_t param3, uint64_t param4) {
    // Forward to original — too complex for safe reimpl at this stage
    if (s_fn_Slot4_Original) {
        return s_fn_Slot4_Original(self, entityIBP, param3, param4);
    }
    return 1; // fallback: not ready
}

// ============================================================
// Slot6 (0x1400EAEA0) — Draw/remove helper
// Thunk: add rcx,0x48; call CitizenAI_impl_5520 (BST RemoveByKey)
// Removes an entity from the BST at +0x48 by key comparison.
// NOT HOOKED (RIP-relative CALL, bug class 2) — exists for standalone exe.
// ============================================================
void ForesterWork_Slot6_Impl(ForesterWork* self, void* entityIBP) {
    // Pass address of BST root at +0x48 and the entity key
    s_fn_BST_RemoveByKey(&self->_field48, entityIBP);
}

// ============================================================
// Slot8 / CancelAll (0x1400EB8A0)
// Loops through BST while count > 0. For each iteration,
// walks to rightmost node (left-most in BST terms), then:
// 1. CheckPtr on the node's entity reference
// 2. Gets entity's StateMachineComponent (+0x278)
// 3. Calls task list cleanup (+0x48 of SM component)
// Repeats until count is 0.
// ============================================================
void ForesterWork_Slot8_Impl(ForesterWork* self) {
    int count = self->_field50;
    while (count > 0) {
        // Walk to the leftmost (minimum) node in the BST
        void* node = self->_field48;
        void* current = self->_field48;
        while (current != nullptr) {
            node = current;
            // node+0x10 = left child pointer
            current = *(void**)((uintptr_t)current + 0x10);
        }

        // node now points to the leftmost BST node
        // The node's value is an InternalBasePtr (entity reference)
        fn_InternalBasePtr_CheckPtr(node);
        uintptr_t entity = *(uintptr_t*)node;

        // Get StateMachineComponent at entity + 0x278
        fn_InternalBasePtr_CheckPtr((void*)(entity + 0x278));
        uintptr_t smComp = *(uintptr_t*)(entity + 0x278);

        // Cancel all work: cleanup task list at smComp + 0x48
        s_fn_TaskListCleanup((void*)(smComp + 0x48));

        // Re-read count (cleanup may modify it via BST removal)
        count = self->_field50;
    }
}

// ============================================================
// Slot9 / RemoveInvalid (0x1400EAC60)
// Iterates BST in-order, validates each entry via FUN_1400F7100.
// Invalid entries are collected into an array, then batch-removed
// from the BST after traversal.
// Returns: true (1) if count == 0 after removal, false (0) otherwise.
//
// NOTE: This hook is DISABLED (bug class 3 — MOV RAX,RSP prologue).
// Implementation exists for standalone exe compilation.
// ============================================================
int ForesterWork_Slot9_Impl(ForesterWork* self, void* param2) {
    // BST root address at +0x48
    void** bstRootPtr = &self->_field48;
    void* bstRoot = *bstRootPtr;

    if (bstRoot == nullptr) {
        // Empty tree — count should be 0
        return (self->_field50 == 0) ? 1 : 0;
    }

    // Phase 1: Walk BST in-order, collect invalid entries
    // We need a dynamic array of invalid entity keys to remove
    uint32_t invalidCount = 0;
    uint32_t invalidCapacity = 0;
    uintptr_t* invalidList = nullptr;

    // Walk to leftmost node first
    void* node = bstRoot;
    while (*(void**)((uintptr_t)node + 0x10) != nullptr) {
        node = *(void**)((uintptr_t)node + 0x10);
    }

    // In-order traversal
    while (node != nullptr) {
        // Read entity key from node
        uintptr_t entityKey = *(uintptr_t*)node;
        uintptr_t ibpCopy[1] = { entityKey };

        // Validate via FUN_1400f7100
        uint8_t valid = s_fn_Work_ValidateTask(self, param2, ibpCopy, nullptr);

        if (valid == 0) {
            // Invalid — add to removal list
            uint32_t newCount = invalidCount + 1;
            if (newCount > invalidCapacity) {
                uint32_t newCap = (invalidCount == 0) ? newCount : newCount * 3;
                uintptr_t* newList = (uintptr_t*)fn_MemAlloc(g_pMemAllocSingleton, (int64_t)newCap * 8, 8);
                // Copy existing entries
                if (invalidList != newList && invalidCount > 0) {
                    for (uint32_t i = 0; i < invalidCount; i++) {
                        newList[i] = invalidList[i];
                    }
                }
                // Add new entry
                newList[invalidCount] = entityKey;
                if (invalidList != nullptr && invalidList != newList) {
                    fn_MemFreeStatic(invalidList);
                }
                invalidList = newList;
                invalidCapacity = newCap;
            } else {
                invalidList[invalidCount] = entityKey;
            }
            invalidCount = newCount;
        }

        // Move to next in-order successor
        void* right = *(void**)((uintptr_t)node + 0x18); // right child
        if (right != nullptr) {
            // Go to leftmost of right subtree
            node = right;
            while (*(void**)((uintptr_t)node + 0x10) != nullptr) {
                node = *(void**)((uintptr_t)node + 0x10);
            }
        } else {
            // Walk up: find first ancestor where we came from left child
            void* parent = *(void**)((uintptr_t)node + 0x08); // parent
            while (parent != nullptr && *(void**)((uintptr_t)parent + 0x18) == node) {
                node = parent;
                parent = *(void**)((uintptr_t)parent + 0x08);
            }
            node = parent;
        }
    }

    // Phase 2: Remove all invalid entries from BST
    if (invalidCount > 0) {
        for (uint32_t i = 0; i < invalidCount; i++) {
            // Find and remove node by key
            void* searchNode = *bstRootPtr;
            while (searchNode != nullptr) {
                fn_InternalBasePtr_CheckPtr(searchNode);
                uintptr_t nodeKey = *(uintptr_t*)searchNode;

                fn_InternalBasePtr_CheckPtr(&invalidList[i]);
                uintptr_t targetKey = invalidList[i];

                if (targetKey < nodeKey) {
                    searchNode = *(void**)((uintptr_t)searchNode + 0x10); // left
                } else if (targetKey > nodeKey) {
                    searchNode = *(void**)((uintptr_t)searchNode + 0x18); // right
                } else {
                    // Found — remove this node
                    s_fn_BST_Remove(bstRootPtr, searchNode);
                    break;
                }
            }
        }
    }

    // Free temporary list
    fn_MemFreeStatic(invalidList);

    // Return true if no entries remain
    return (invalidCount == 0) ? 1 : 0;
}

// ============================================================
// Slot10 / GetWorkEntity (0x1400EB910)
// Hidden return ptr pattern: RCX=this, RDX=outPtr
// Looks up entity from self+0x10 (Work base), finds the
// BuildDescription component, reads +0x48 field from it.
// ============================================================
void* ForesterWork_Slot10_Impl(ForesterWork* self, void* outPtr) {
    // CheckPtr on entity ref at self+0x10
    uintptr_t* entityRef = (uintptr_t*)((uintptr_t)self + 0x10);
    fn_InternalBasePtr_CheckPtr(entityRef);

    // Find BuildDescription component on the entity
    uintptr_t ibpBuf[1] = {0};
    s_fn_Entity_FindComponent_BuildDesc(*entityRef, ibpBuf);

    // CheckPtr on found component
    fn_InternalBasePtr_CheckPtr(ibpBuf);

    // Read entity ref at component+0x48 and store to output
    uintptr_t comp = ibpBuf[0];
    *(uintptr_t*)outPtr = *(uintptr_t*)(comp + 0x48);

    return outPtr;
}
