// code/Game/Work/work_base_reimpl.cpp
// Shared Work base-class / ECS helpers referenced from many Work classes.
//
// These exe sub-helpers are resolved via ResolveAddr(Module::Exe, 0xADDR) from
// nearly every code/Game/Work/*.cpp. In DLL mode they resolve to the original exe
// code; in standalone mode they resolve to 0 (silently skipped) unless registered.
// This file provides faithful reimplementations and a single registration point
// (RegisterWorkBaseBatch in standalone/reg_work_base.cpp) so the shared call sites
// fire real logic in the standalone exe.
//
// Wave 2 (Work) — shared base-helper sub-batch. Each function is a 1:1 port of the
// decompiled exe helper; do not "improve" behavior.
//
// This file must NOT depend on hooks.h — it is portable for the standalone exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// FindComponent family
// ============================================================
// FUN_140002ed0 / 2f90 / 3050 / 52c0 / 6270 are byte-for-byte identical except for
// the component type-ID global they compare against. Each scans the entity's
// component list (ptr array at entity+0x30, count at entity+0x38), calls each
// component's GetTypeId (vtable +0x10), and on the first type match runs an
// IsDerivedFrom check before returning the component through an out InternalBasePtr.
//
// Caller signature (matches the s_fn_FindComponent_* typedefs in the Work .cpp files
// and the fn_FindComponent_* decls in engine_funcs.h): the function returns the
// out-IBP it was handed.   void* fn(uintptr_t entity, void* retIBP)

// Generic body shared by all variants. pTypeId points at the exe's type-ID storage
// global (a uint32_t filled in by the component's RegisterType at startup).
static void* FindComponentByTypeId(uintptr_t entity, void* retIBP, const uint32_t* pTypeId) {
    void* found = nullptr;
    if (pTypeId) {
        int n = *reinterpret_cast<int*>(entity + 0x38);
        uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x30);
        for (int i = 0; i < n; i++) {
            void** slot = reinterpret_cast<void**>(list + static_cast<int64_t>(i) * 8);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
            void* comp = *slot;
            // GetTypeId is component vtable slot +0x10.
            uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
            if (t == *pTypeId) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
                void* c = *slot;
                found = (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *pTypeId)) ? c : nullptr;
                break;
            }
        }
    }
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(retIBP, found);
    return retIBP;
}

// Lazily-resolved type-ID storage globals (exe .rdata, filled by RegisterType).
static const uint32_t* s_pTid_2ed0 = nullptr;  // 0x1401c7900  (Position/Interact)
static const uint32_t* s_pTid_2f90 = nullptr;  // 0x1401c7e08  (MeshInstance/StatusIcon)
static const uint32_t* s_pTid_3050 = nullptr;  // 0x1401c8070
static const uint32_t* s_pTid_52c0 = nullptr;  // 0x1401c7b30  (NaturalResource/BuildState)
static const uint32_t* s_pTid_6270 = nullptr;  // 0x1401c7e78  (Storage)
static bool s_tidInit = false;

static void EnsureTypeIds() {
    if (s_tidInit) return;
    s_pTid_2ed0 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7900));
    s_pTid_2f90 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7e08));
    s_pTid_3050 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c8070));
    s_pTid_52c0 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7b30));
    s_pTid_6270 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7e78));
    s_tidInit = true;
}

// Reimpl of Entity_FindComponent_Position (FUN_140002ed0, type 0x1401c7900)
void* WorkBase_FindComponent_2ed0(uintptr_t entity, void* retIBP) {
    EnsureTypeIds();
    return FindComponentByTypeId(entity, retIBP, s_pTid_2ed0);
}

// Reimpl of Entity_FindComponent_MeshInstance (FUN_140002f90, type 0x1401c7e08)
void* WorkBase_FindComponent_2f90(uintptr_t entity, void* retIBP) {
    EnsureTypeIds();
    return FindComponentByTypeId(entity, retIBP, s_pTid_2f90);
}

// Reimpl of FUN_140003050 (type 0x1401c8070)
void* WorkBase_FindComponent_3050(uintptr_t entity, void* retIBP) {
    EnsureTypeIds();
    return FindComponentByTypeId(entity, retIBP, s_pTid_3050);
}

// Reimpl of FUN_1400052c0 (type 0x1401c7b30)
void* WorkBase_FindComponent_52c0(uintptr_t entity, void* retIBP) {
    EnsureTypeIds();
    return FindComponentByTypeId(entity, retIBP, s_pTid_52c0);
}

// Reimpl of Entity_FindComponent_Storage (FUN_140006270, type 0x1401c7e78)
void* WorkBase_FindComponent_6270(uintptr_t entity, void* retIBP) {
    EnsureTypeIds();
    return FindComponentByTypeId(entity, retIBP, s_pTid_6270);
}

// ============================================================
// Recursive binary-tree post-order free
// ============================================================
// Both are simple recursive post-order frees of a binary tree, differing only in
// the byte offsets of the two child pointers within a node. The first arg (tree
// root / allocator owner) is unused by the original beyond being forwarded.
// Caller signature: void fn(void* root, void* node)

// Reimpl of FUN_140014c30 (TreeNodeCleanup) — children at +0x10 and +0x18.
void WorkBase_TreeNodeFree_14c30(void* root, void* node) {
    if (node) {
        WorkBase_TreeNodeFree_14c30(root, *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + 0x10));
        WorkBase_TreeNodeFree_14c30(root, *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + 0x18));
        if (fn_MemFreeStatic) fn_MemFreeStatic(node);
    }
}

// Reimpl of FUN_14000e320 (TreeNodeFree) — children at +0x18 and +0x20.
void WorkBase_TreeNodeFree_e320(void* root, void* node) {
    if (node) {
        WorkBase_TreeNodeFree_e320(root, *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + 0x18));
        WorkBase_TreeNodeFree_e320(root, *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + 0x20));
        if (fn_MemFreeStatic) fn_MemFreeStatic(node);
    }
}
