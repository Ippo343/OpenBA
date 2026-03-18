// code/Game/Work/firework.cpp
// Clean C++ reimplementation of FireWork Work class methods.
// Source: Game/Work/FireWork.cpp (from profile build debug strings)
//
// FireWork -- fire suppression work action. Size = 0x60.
// Inherits: Object::Internal -> Work (0x48) -> FireWork
// Ctor: 0x1400e9b50, Vtable: 0x1401612a0
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- it is portable for standalone exe.

#include "Work/firework_funcs.h"
#include "FireWork.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Local function pointers resolved at init time
// ============================================================

// Work::Serialize base (0x1400f7400)
static void (*s_fn_Work_Serialize)(void* self, void* stream) = nullptr;

// IO::Serializer::Serialize(this, int*, name) -- via IAT at 0x140152ac8
static void (*s_fn_Serializer_Serialize)(void* self, int* value, const char* name) = nullptr;

// BST serialize helper (0x1400296b0)
static void (*s_fn_BST_Serialize)(void* bst, void* serializer) = nullptr;

// Tree remove by key (0x140025520) -- used by Slot6
// Already available as fn_TreeRemoveByKey in engine_funcs.h

// StateMachine release tasks (0x1400c5400) -- used by Slot8
// Already available as fn_StateMachine_ReleaseTasks in engine_funcs.h

// Work::ValidateTask (0x1400f7100) -- used by Slot9
static uint8_t (*s_fn_Work_ValidateTask)(void* self, void* param2, void* ibp, void** outTask) = nullptr;

// BST node removal (0x14001a6e0) -- used by Slot9
// Already available as fn_BST_Remove in engine_funcs.h

// System::MemoryAllocator::MemAlloc -- via dispatch slot
static void* (*s_fn_MemAlloc)(int64_t size, int alignment) = nullptr;

// Slot4 original function (0x1400e9620) -- too complex to fully reimplement now
typedef int (*fn_Slot4_Orig_t)(void* self, void* citizenIBP, void* time);
static fn_Slot4_Orig_t s_fn_Slot4_Orig = nullptr;

// Slot9 original function (0x1400e93c0) -- too complex to fully reimplement now
typedef int (*fn_Slot9_Orig_t)(void* self, void* citizenIBP);
static fn_Slot9_Orig_t s_fn_Slot9_Orig = nullptr;

// Object::InternalBasePtr::DeletePtr -- via dispatch slot
typedef void (*fn_IBP_DeletePtr_t)(void* self);
static fn_IBP_DeletePtr_t s_fn_IBP_DeletePtr = nullptr;

static bool s_firework_resolved = false;

// ============================================================
// Resolution -- called once
// ============================================================
bool FireWork_ResolveFuncs() {
    if (s_firework_resolved) return true;

    // Work::Serialize
    s_fn_Work_Serialize = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x1400f7400));

    // IO::Serializer::Serialize via IAT at [0x140152ac8]
    uintptr_t iatSer = ResolveAddr(Module::Exe, 0x140152ac8);
    if (iatSer)
        s_fn_Serializer_Serialize = *reinterpret_cast<void(**)(void*, int*, const char*)>(iatSer);

    // BST serialize helper
    s_fn_BST_Serialize = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x1400296b0));

    // Work::ValidateTask (FUN_1400f7100)
    s_fn_Work_ValidateTask = reinterpret_cast<uint8_t(*)(void*, void*, void*, void**)>(
        ResolveAddr(Module::Exe, 0x1400f7100));

    // System::MemoryAllocator::MemAlloc via dispatch slot 0x140152748
    uintptr_t iatMemAlloc = ResolveAddr(Module::Exe, 0x140152748);
    if (iatMemAlloc)
        s_fn_MemAlloc = *reinterpret_cast<void*(**)(int64_t, int)>(iatMemAlloc);

    // Slot4 original (direct call, not via hook trampoline)
    s_fn_Slot4_Orig = reinterpret_cast<fn_Slot4_Orig_t>(
        ResolveAddr(Module::Exe, 0x1400e9620));

    // Slot9 original (direct call, not via hook trampoline)
    s_fn_Slot9_Orig = reinterpret_cast<fn_Slot9_Orig_t>(
        ResolveAddr(Module::Exe, 0x1400e93c0));

    // Object::InternalBasePtr::DeletePtr via dispatch slot 0x140152a98
    uintptr_t iatDeletePtr = ResolveAddr(Module::Exe, 0x140152a98);
    if (iatDeletePtr)
        s_fn_IBP_DeletePtr = *reinterpret_cast<fn_IBP_DeletePtr_t*>(iatDeletePtr);

    bool ok = s_fn_Work_Serialize && g_fn_Serializer_Ctor && g_fn_Serializer_Dtor &&
              s_fn_Serializer_Serialize && s_fn_BST_Serialize &&
              fn_TreeRemoveByKey && fn_StateMachine_ReleaseTasks &&
              fn_InternalBasePtr_CheckPtr && fn_BST_Remove &&
              s_fn_Work_ValidateTask && s_fn_MemAlloc &&
              fn_MemFreeStatic;

#ifdef _DEBUG
    printf("[FireWork] ResolveFuncs: WorkSer=%p SerSer=%p BSTSer=%p\n",
           (void*)s_fn_Work_Serialize, (void*)s_fn_Serializer_Serialize,
           (void*)s_fn_BST_Serialize);
    printf("[FireWork]   TreeRemove=%p SMRelease=%p ValidateTask=%p\n",
           (void*)fn_TreeRemoveByKey, (void*)fn_StateMachine_ReleaseTasks,
           (void*)s_fn_Work_ValidateTask);
    printf("[FireWork]   Slot4Orig=%p Slot9Orig=%p MemAlloc=%p\n",
           (void*)s_fn_Slot4_Orig, (void*)s_fn_Slot9_Orig, (void*)s_fn_MemAlloc);
    if (!ok) printf("[FireWork] ERROR: some function pointers not resolved!\n");
#endif

    s_firework_resolved = ok;
    return ok;
}

// ============================================================
// Serialize (vtable +0x18, 0x1400e9a50)
// Pattern: Work_Serialize + Serializer(v0) + BST_Serialize(+0x50) +
//          Serializer::Serialize(+0x48 as int)
// ============================================================
void FireWork_Serialize_Impl(FireWork* self, void* stream) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[FireWork::Serialize] self=%p stream=%p field58=%d\n",
               (void*)self, stream, self->_field58);
        callCount++;
    }
#endif

    // Serialize Work base fields
    s_fn_Work_Serialize(self, stream);

    // Create serializer block (version 0)
    uint8_t serializerBuf[24];
    g_fn_Serializer_Ctor(serializerBuf, stream, 0);

    // Serialize BST at +0x50
    s_fn_BST_Serialize(&self->_field50, serializerBuf);

    // Serialize int at +0x48
    int* field48_ptr = reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(self) + 0x48);
    s_fn_Serializer_Serialize(serializerBuf, field48_ptr, nullptr);

    g_fn_Serializer_Dtor(serializerBuf);
}

// ============================================================
// Slot4 (vtable +0x20, 0x1400e9620)
// Fire suppression work logic -- extremely complex function with many
// sub-calls to fire system helpers. Delegates to original exe code for now.
// TODO: Full reimplementation once all fire system helpers are resolved.
// ============================================================
int FireWork_Slot4_Impl(FireWork* self, void* citizenIBP, void* time) {
    // Early exit check matching the original:
    // if (_field48 <= _field58) return 1;
    // (decompiled: if (param_1[9] <= param_1[0xb]) return 1)
    int32_t field48_val = *reinterpret_cast<int32_t*>(
        reinterpret_cast<uintptr_t>(self) + 0x48);
    if (static_cast<int32_t>(self->_field58) >= field48_val) {
        return 1;
    }

    // Complex fire suppression logic with many dependencies.
    // Delegate to original code at 0x1400e9620.
    // Note: s_fn_Slot4_Orig points to the original function body, NOT through hook.
    if (s_fn_Slot4_Orig) {
        return s_fn_Slot4_Orig(self, citizenIBP, time);
    }
    return 1;
}

// ============================================================
// Slot6 (vtable +0x30, 0x1400e9600)
// Simple: calls TreeRemoveByKey on BST at +0x50 to remove citizen.
// Returns 0 (false).
//
// Original decompilation:
//   CitizenAI_impl_5520((longlong *)(param_1 + 0x50), param_2);
//   return 0;
// ============================================================
void FireWork_Slot6_Impl(FireWork* self, void* citizenIBP) {
    // Remove citizen entry from the BST at +0x50
    void* bstRoot = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(self) + 0x50);
    fn_TreeRemoveByKey(bstRoot, citizenIBP);
}

// ============================================================
// Slot8 (vtable +0x40, 0x1400e99a0)
// Cleanup: iterates BST at +0x50, for each entry:
//   1. CheckPtr on the entry
//   2. Dereference to get entity
//   3. CheckPtr on entity+0x278 (citizen AI component)
//   4. Call StateMachine_ReleaseTasks on citizenAI+0x48
//
// After cleanup, the BST entries are removed via fn_StateMachine_ReleaseTasks
// which handles clearing the work assignment state.
//
// Original walks a BST (binary search tree) -- nodes have:
//   +0x00: key (entity pointer)
//   +0x08: parent
//   +0x10: left child
//   +0x18: right child
//
// The outer loop repeatedly finds the rightmost node (last in-order),
// then processes and removes it.
// ============================================================
void FireWork_Slot8_Impl(FireWork* self) {
    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(self);
    void** bstRootField = reinterpret_cast<void**>(selfAddr + 0x50);

    // The original code walks the BST in a loop:
    // 1. Get root node from *bstRootField
    // 2. Walk to rightmost (last) node: follow +0x10 (left child) until null
    //    Then walk back following +0x08 (parent) and check +0x18 (right child)
    // 3. For that node: CheckPtr, get entity, CheckPtr on entity+0x278,
    //    call StateMachine_ReleaseTasks on (entity+0x278)+0x48
    //
    // This is a standard BST iteration pattern. The decompiler made it look
    // convoluted due to the tree traversal becoming nested do/while loops.

    while (true) {
        uintptr_t root = *reinterpret_cast<uintptr_t*>(bstRootField);
        if (root == 0) return;

        // Find rightmost (last) node by traversing right children
        uintptr_t node = root;
        uintptr_t rightChild;
        while ((rightChild = *reinterpret_cast<uintptr_t*>(node + 0x10)) != 0) {
            node = rightChild;
        }

        // Walk back up to find a processable node
        // The original code has a nested loop that traverses parent pointers
        // Looking for a non-null node to process
        if (node == 0) break;

        // Walk to find last node following parent chain
        uintptr_t current = root;
        uintptr_t lastNonNull = 0;
        while (current != 0) {
            lastNonNull = current;
            current = *reinterpret_cast<uintptr_t*>(current + 0x10);
        }

        if (lastNonNull == 0) break;

        // Process this node: CheckPtr, get entity, release tasks
        fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(lastNonNull));
        uintptr_t entity = *reinterpret_cast<uintptr_t*>(lastNonNull);

        // entity+0x278 = citizen AI sub-component
        fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(entity + 0x278));
        uintptr_t citizenAI = *reinterpret_cast<uintptr_t*>(entity + 0x278);

        // Release tasks from citizenAI+0x48 (task list)
        fn_StateMachine_ReleaseTasks(reinterpret_cast<void*>(citizenAI + 0x48));
    }
}

// ============================================================
// Slot9 (vtable +0x48, 0x1400e93c0)
// Validates all assigned citizens in the BST at +0x50.
// Walks the BST in-order, calls Work_ValidateTask on each.
// Invalid entries are collected and then removed from the BST.
// Returns: 1 if all entries were invalid (BST empty after cleanup),
//          0 if some valid entries remain.
//
// This is a complex function with BST traversal, dynamic array
// management, and multiple helper calls. Delegates to original for now.
// TODO: Full reimplementation once tree manipulation is verified.
// ============================================================
int FireWork_Slot9_Impl(FireWork* self, void* citizenIBP) {
    // Delegate to original code at 0x1400e93c0.
    if (s_fn_Slot9_Orig) {
        return s_fn_Slot9_Orig(self, citizenIBP);
    }
    return 0;
}
