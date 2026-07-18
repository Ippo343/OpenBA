// code/Game/Work/tradework.cpp
// Clean C++ reimplementation of TradeWork -- all methods.
// Source: Game/Work/TradeWork.cpp (from profile build debug strings)
//
// TradeWork (0x58 bytes) -- Trade post work action.
// Inherits: Object::Internal -> Work (0x48) -> TradeWork
// Vtable: 0x140161a10
//
// BST node layout (extended, 0x30 bytes):
//   +0x00: key (InternalBasePtr, citizen entity)
//   +0x08: entity ref (trade target)
//   +0x10: quantity (int32)
//   +0x18: parent pointer
//   +0x20: left child
//   +0x28: right child
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- it is portable for standalone exe.

#include "Work/tradework_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Local function pointers (resolved once by TradeWork_ResolveFuncs)
// ============================================================

// All typedefs prefixed with TrW_ to avoid conflicts

// Work::Serialize
typedef void  (*TrW_Work_Serialize_t)(void* self, void* stream);
// IO::Serializer ctor/dtor
typedef void* (*TrW_Serializer_Ctor_t)(void* self, void* stream, int version);
typedef void  (*TrW_Serializer_Dtor_t)(void* self);
// BST serialization (extended BST at 0x14002c8a0)
typedef void  (*TrW_BST_Serialize_t)(void* bstRoot, void* serializer);
// BST find-and-remove by key (TradeWork_impl_c690 at 0x14002c690)
typedef void  (*TrW_BSTFindRemove_t)(void* bstRoot, void* keyIBP);
// BST node remove (TradeWork_helper_5740)
typedef void  (*TrW_BSTRemove_t)(void* bstRoot, void* node);
// Cancel all work items
typedef void  (*TrW_CancelAllWork_t)(void* citizenAISub);
// FUN_1400f7100 -- validate work entity
typedef bool  (*TrW_ValidateEntity_t)(uintptr_t work, void* citizenIBP, void* entityIBP, void* outTask);
// MemAlloc/MemFree
typedef void* (*TrW_MemAlloc_t)(int64_t size, int align);
typedef void  (*TrW_MemFree_t)(void* ptr);
typedef void  (*TrW_MemFreeStatic_t)(void* ptr);
// Tree node cleanup (recursive free, extended BST at 0x140014b00)
typedef void  (*TrW_TreeNodeCleanup_t)(void* bstRoot, void* node);
// FindSubEntity
typedef void* (*TrW_FindSubEntity_t)(uintptr_t entity, void* outIBP);

// Slot4 original function (too complex for clean reimpl, called via trampoline)
typedef uint32_t (*TrW_Slot4_Original_t)(void* self, void* citizenIBP, void* param3);

static TrW_Work_Serialize_t          s_fn_Work_Serialize = nullptr;
static TrW_Serializer_Ctor_t         s_fn_Serializer_Ctor = nullptr;
static TrW_Serializer_Dtor_t         s_fn_Serializer_Dtor = nullptr;
static TrW_BST_Serialize_t           s_fn_BST_Serialize = nullptr;
static TrW_BSTFindRemove_t           s_fn_BSTFindRemove = nullptr;
static TrW_BSTRemove_t               s_fn_BSTRemove = nullptr;
static TrW_CancelAllWork_t           s_fn_CancelAllWork = nullptr;
static TrW_ValidateEntity_t          s_fn_ValidateEntity = nullptr;
static TrW_MemAlloc_t                s_fn_MemAlloc = nullptr;
static TrW_MemFree_t                 s_fn_MemFree = nullptr;
static TrW_MemFreeStatic_t           s_fn_MemFreeStatic = nullptr;
static TrW_TreeNodeCleanup_t         s_fn_TreeNodeCleanup = nullptr;
static TrW_FindSubEntity_t           s_fn_FindSubEntity = nullptr;

// Vtable pointers for dtor chain
static void** s_vtable_TradeWork = nullptr;  // 0x140161a10
static void** s_vtable_Work = nullptr;        // 0x1401612e0

static bool s_trade_funcs_resolved = false;

// ============================================================
// Resolution -- called once during hook installation
// ============================================================
bool TradeWork_ResolveFuncs() {
    if (s_trade_funcs_resolved) return true;

    // Direct exe function addresses
    s_fn_Work_Serialize = reinterpret_cast<TrW_Work_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400f7400));
    s_fn_BST_Serialize = reinterpret_cast<TrW_BST_Serialize_t>(
        ResolveAddr(Module::Exe, 0x14002c8a0));
    s_fn_BSTFindRemove = reinterpret_cast<TrW_BSTFindRemove_t>(
        ResolveAddr(Module::Exe, 0x14002c690));
    s_fn_BSTRemove = reinterpret_cast<TrW_BSTRemove_t>(
        ResolveAddr(Module::Exe, 0x140025740));
    s_fn_CancelAllWork = reinterpret_cast<TrW_CancelAllWork_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_ValidateEntity = reinterpret_cast<TrW_ValidateEntity_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));
    s_fn_TreeNodeCleanup = reinterpret_cast<TrW_TreeNodeCleanup_t>(
        ResolveAddr(Module::Exe, 0x140014b00));
    s_fn_FindSubEntity = reinterpret_cast<TrW_FindSubEntity_t>(
        ResolveAddr(Module::Exe, 0x140002e10));

    // Dispatch slots (indirect through .rdata)
    uintptr_t iatSerCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    if (iatSerCtor) s_fn_Serializer_Ctor = *reinterpret_cast<TrW_Serializer_Ctor_t*>(iatSerCtor);
    uintptr_t iatSerDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatSerDtor) s_fn_Serializer_Dtor = *reinterpret_cast<TrW_Serializer_Dtor_t*>(iatSerDtor);
    uintptr_t iatMemAlloc = ResolveAddr(Module::Exe, 0x140151460);
    if (iatMemAlloc) s_fn_MemAlloc = *reinterpret_cast<TrW_MemAlloc_t*>(iatMemAlloc);
    uintptr_t iatMemFree = ResolveAddr(Module::Exe, 0x140151468);
    if (iatMemFree) s_fn_MemFree = *reinterpret_cast<TrW_MemFree_t*>(iatMemFree);

    // fn_MemFreeStatic from engine_funcs
    s_fn_MemFreeStatic = reinterpret_cast<TrW_MemFreeStatic_t>(fn_MemFreeStatic);

    // Vtable pointers for dtor
    s_vtable_TradeWork = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140161a10));
    s_vtable_Work = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401612e0));

    // Verify critical pointers
    bool ok = s_fn_Work_Serialize && s_fn_Serializer_Ctor && s_fn_Serializer_Dtor &&
              s_fn_BST_Serialize && s_fn_BSTFindRemove && s_fn_BSTRemove &&
              s_fn_CancelAllWork && s_fn_ValidateEntity &&
              s_fn_MemAlloc && s_fn_MemFree &&
              s_fn_TreeNodeCleanup && s_fn_FindSubEntity &&
              fn_InternalBasePtr_CheckPtr &&
              s_vtable_TradeWork && s_vtable_Work;

    s_trade_funcs_resolved = ok;

#ifdef _DEBUG
    printf("[TradeWork] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
        if (!s_fn_Work_Serialize) printf("  MISSING: Work_Serialize\n");
        if (!s_fn_Serializer_Ctor) printf("  MISSING: Serializer_Ctor\n");
        if (!s_fn_Serializer_Dtor) printf("  MISSING: Serializer_Dtor\n");
        if (!s_fn_BST_Serialize) printf("  MISSING: BST_Serialize\n");
        if (!s_fn_BSTFindRemove) printf("  MISSING: BSTFindRemove\n");
        if (!s_fn_BSTRemove) printf("  MISSING: BSTRemove\n");
        if (!s_fn_CancelAllWork) printf("  MISSING: CancelAllWork\n");
        if (!s_fn_ValidateEntity) printf("  MISSING: ValidateEntity\n");
        if (!s_fn_MemAlloc) printf("  MISSING: MemAlloc\n");
        if (!s_fn_MemFree) printf("  MISSING: MemFree\n");
        if (!s_fn_TreeNodeCleanup) printf("  MISSING: TreeNodeCleanup\n");
        if (!s_fn_FindSubEntity) printf("  MISSING: FindSubEntity\n");
    }
#endif

    return ok;
}


// ============================================================
// TradeWork::Slot4 -- Trade assignment (0x1400f5d40)
// ============================================================
// This is extremely complex (~700 lines decompiled): iterates trade partners,
// calculates supply/demand for each resource type, finds storage components,
// checks capacities, finds pathfinding routes, creates trade work actions.
//
// Hook is DISABLED (bug class 3 -- MOV RAX,RSP in prologue).
// For standalone exe, this will need a full reimplementation from the decompiled
// code. For now, provide a stub that returns 1 (no work) -- the hook wrapper
// will continue to use the trampoline.
//
int TradeWork_Slot4_Impl(TradeWork* self, void* citizenIBP, uint64_t /*param3*/, uint64_t /*param4*/) {
    // Stub -- hook wrapper uses trampoline for this function
    // since the hook is DISABLED (bug class 3).
    // Full reimpl requires ~700 lines of trade supply/demand logic.
    (void)self;
    (void)citizenIBP;
    return 1;
}


// ============================================================
// TradeWork::Slot6 -- BSTFindRemove + return false (0x1400f5d20)
// ============================================================
bool TradeWork_Slot6_Impl(TradeWork* self, void* param2) {
    s_fn_BSTFindRemove((void*)((uintptr_t)self + 0x48), param2);
    return false;
}


// ============================================================
// TradeWork::Slot7 -- BSTFindRemove thunk (0x1400f5d10)
// ============================================================
int TradeWork_Slot7_Impl(TradeWork* self, void* param2) {
    s_fn_BSTFindRemove((void*)((uintptr_t)self + 0x48), param2);
    return 0;
}


// ============================================================
// TradeWork::Slot8 / CancelAll (0x1400f6be0)
// Loops through BST at +0x48 while count > 0.
// Extended BST nodes: left child at +0x20 (not +0x10).
// ============================================================
int TradeWork_Slot8_Impl(TradeWork* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    while (*(int*)(selfAddr + 0x50) > 0) {
        // Find leftmost node in BST (following +0x20 = left child for extended nodes)
        void* node = *(void**)(selfAddr + 0x48);
        void* leftmost = node;
        while (node != nullptr) {
            leftmost = node;
            node = *(void**)((uintptr_t)node + 0x20);
        }

        // Cancel the work item
        fn_InternalBasePtr_CheckPtr(leftmost);
        uintptr_t citizenEntity = *(uintptr_t*)leftmost;

        fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
        s_fn_CancelAllWork((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));
    }
    return 0;
}


// ============================================================
// TradeWork::Slot9 / RemoveInvalid (0x1400f5ae0)
// Iterates extended BST, validates each entry, removes invalid ones.
// Extended BST: +0x18=parent, +0x20=left, +0x28=right
// Returns true if all valid, false otherwise.
// ============================================================
bool TradeWork_Slot9_Impl(TradeWork* self, void* param2) {
    uintptr_t selfAddr = (uintptr_t)self;
    void** bstRootPtr = (void**)(selfAddr + 0x48);

    void* root = *bstRootPtr;
    uintptr_t* removeArr = nullptr;
    uint32_t removeCount = 0;
    uint32_t removeCapacity = 0;

    if (root == nullptr) {
        return true;
    }

    // Find leftmost node (minimum) -- extended BST uses +0x20 for left child
    void* node = root;
    while (*(void**)((uintptr_t)node + 0x20) != nullptr) {
        node = *(void**)((uintptr_t)node + 0x20);
    }

    // In-order traversal of extended BST
    while (node != nullptr) {
        uintptr_t entityIBP = *(uintptr_t*)node;

        // Validate this entity
        bool stillValid = s_fn_ValidateEntity(selfAddr, param2, (void*)&entityIBP, nullptr);

        if (!stillValid) {
            // Add to removal list -- grow array if needed
            uint32_t newCount = removeCount + 1;
            if (newCount != removeCount) {
                uintptr_t* newArr = removeArr;
                if ((int)removeCapacity < (int)newCount) {
                    uint32_t newCap = newCount;
                    if (removeCount != 0) {
                        newCap = newCount * 3;
                    }
                    removeCapacity = newCap;
                    newArr = (uintptr_t*)s_fn_MemAlloc((int64_t)newCap * 8, 8);
                }

                // Copy existing entries if buffer changed
                uint32_t copyStart = 0;
                if (removeArr == newArr) {
                    copyStart = removeCount;
                }
                for (uint32_t i = copyStart; i < newCount; i++) {
                    uintptr_t val;
                    if (i < removeCount) {
                        val = removeArr[i];
                    } else {
                        val = entityIBP;
                    }
                    newArr[i] = val;
                }
                if (removeArr != newArr) {
                    fn_MemFreeStatic(removeArr);
                }
                removeArr = newArr;
                removeCount = newCount;
            }
        }

        // In-order successor: right child's leftmost descendant, or parent
        // Extended BST: +0x28=right, +0x20=left, +0x18=parent
        void* rightChild = *(void**)((uintptr_t)node + 0x28);
        if (rightChild != nullptr) {
            node = rightChild;
            while (*(void**)((uintptr_t)node + 0x20) != nullptr) {
                node = *(void**)((uintptr_t)node + 0x20);
            }
        } else {
            void* parent = *(void**)((uintptr_t)node + 0x18);
            if (parent != nullptr && *(void**)((uintptr_t)parent + 0x20) != node) {
                while (parent != nullptr && *(void**)((uintptr_t)parent + 0x28) == node) {
                    node = parent;
                    parent = *(void**)((uintptr_t)parent + 0x18);
                }
                node = parent;
            } else {
                node = parent;
            }
        }
    }

    // Remove all invalid entries from BST
    // Extended BST: left at +0x20, right at +0x28
    if (removeCount > 0) {
        for (uint32_t i = 0; i < removeCount; i++) {
            void* searchNode = *bstRootPtr;
            while (searchNode != nullptr) {
                fn_InternalBasePtr_CheckPtr(searchNode);
                if (removeArr[i] < *(uintptr_t*)searchNode) {
                    searchNode = *(void**)((uintptr_t)searchNode + 0x20);
                } else {
                    fn_InternalBasePtr_CheckPtr(&removeArr[i]);
                    if (removeArr[i] <= *(uintptr_t*)searchNode) {
                        s_fn_BSTRemove(bstRootPtr, searchNode);
                        break;
                    }
                    searchNode = *(void**)((uintptr_t)searchNode + 0x28);
                }
            }
        }
    }

    fn_MemFreeStatic(removeArr);
    return removeCount == 0;
}


// ============================================================
// TradeWork::Slot10 / GetWorkEntity (0x1400f6c50)
// Returns entity from this->_field10's sub-object at +0x48.
// ============================================================
void* TradeWork_Slot10_Impl(TradeWork* self, void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)self;

    fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
    uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

    uintptr_t subIBP[2] = {0};
    void* result = s_fn_FindSubEntity(entityRef, subIBP);

    fn_InternalBasePtr_CheckPtr(result);

    uintptr_t resolvedEntity = *(uintptr_t*)result;
    *(uintptr_t*)outPtr = *(uintptr_t*)(resolvedEntity + 0x48);

    return outPtr;
}


// ============================================================
// TradeWork::Slot11 / HasWorkers (0x140029860)
// Returns (workerCount > 0). Shared with many work classes.
// ============================================================
bool TradeWork_Slot11_Impl(TradeWork* self) {
    return self->workerCount > 0;
}


// ============================================================
// TradeWork::Execute (Slot5) -- 3 bytes, returns false.
// ============================================================
bool TradeWork_Execute_Impl(TradeWork* /*self*/) {
    return false;
}


// ============================================================
// TradeWork::Serialize (0x1400f6cb0)
// Work::Serialize + Serializer(v0) + BST serialize.
// No extra int field (unlike TendArea).
// ============================================================
void TradeWork_Serialize_Impl(TradeWork* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Call Work::Serialize base
    s_fn_Work_Serialize(self, stream);

    // Create serializer (version block, version=0)
    uint8_t serializerBuf[24] = {0};
    s_fn_Serializer_Ctor(serializerBuf, stream, 0);

    // Serialize extended BST at +0x48
    s_fn_BST_Serialize((void*)(selfAddr + 0x48), serializerBuf);

    s_fn_Serializer_Dtor(serializerBuf);
}


// ============================================================
// TradeWork::DtorBody
// Sets vtable, frees extended BST nodes, calls base dtor.
// ============================================================
void TradeWork_DtorBody(TradeWork* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Set vtable to TradeWork
    *(void**)selfAddr = s_vtable_TradeWork;

    // Free BST nodes at +0x48 (extended: left at +0x20, right at +0x28)
    void* bstRoot = *(void**)(selfAddr + 0x48);
    if (bstRoot != nullptr) {
        s_fn_TreeNodeCleanup((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x20));
        s_fn_TreeNodeCleanup((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x28));
        if (s_fn_MemFreeStatic) {
            s_fn_MemFreeStatic(bstRoot);
        }
    }

    // Zero out BST root and count
    *(void**)(selfAddr + 0x48) = nullptr;
    *(uint32_t*)(selfAddr + 0x50) = 0;

    // Set vtable to Work base
    *(void**)selfAddr = s_vtable_Work;
    fn_ObjectInternal_Dtor((void*)selfAddr);
}


// ============================================================
// TradeWork::ScalarDtor (0x14002c660)
// ============================================================
void* TradeWork_ScalarDtor_Impl(TradeWork* self, uint32_t flags) {
    TradeWork_DtorBody(self);
    if (flags & 1) {
        if (s_fn_MemFreeStatic) {
            s_fn_MemFreeStatic(self);
        }
    }
    return self;
}


// ============================================================
// Standalone factory + vtable — Work family (factory cascade).
// Vtable @ 0x140161a10 (13 slots), size 0x58, parent "Work".
// Shared slot helpers: work_base_standalone.{h,cpp}.
// Slot map ground-truthed from the real exe vtable @ 0x140161a10:
//   slot4 (Execute)=TradeWork_Slot4_Impl (0x1400f5d40, STUB — uses trampoline
//   in DLL mode). The orphaned TradeWork_Execute_Impl ("returns false") is NOT
//   in the vtable — slot5 is the shared Work_Slot5. Do NOT use it.
// ============================================================
#include "work_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t TradeWork_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("TradeWork");
    return c;
}

static void* g_TradeWork_Vtable[13] = {
    (void*)&TradeWork_ScalarDtor_Impl,  // +0x00 ScalarDtor (0x14002c660)
    (void*)&Work_Slot1,                 // +0x08 slot1 (null)
    (void*)&TradeWork_GetTypeId,        // +0x10 GetTypeId
    (void*)&TradeWork_Serialize_Impl,   // +0x18 Serialize (0x1400f6cb0)
    (void*)&TradeWork_Slot4_Impl,       // +0x20 Execute (0x1400f5d40, STUB)
    (void*)&Work_Slot5,                 // +0x28 slot5 (returns 0)
    (void*)&TradeWork_Slot6_Impl,       // +0x30 OnComplete (0x1400f5d20)
    (void*)&TradeWork_Slot7_Impl,       // +0x38 OnCancel (0x1400f5d10)
    (void*)&TradeWork_Slot8_Impl,       // +0x40 Cleanup (0x1400f6be0)
    (void*)&TradeWork_Slot9_Impl,       // +0x48 Validate (0x1400f5ae0)
    (void*)&TradeWork_Slot10_Impl,      // +0x50 GetTarget (0x1400f6c50)
    (void*)&TradeWork_Slot11_Impl,      // +0x58 GetPriority (0x140029860)
    (void*)&Work_Slot12,                // +0x60 slot12 (void nop)
};

extern "C" void RegisterTradeWorkVtable() {
    RegisterExeAddr(0x140161a10, (uintptr_t)g_TradeWork_Vtable);
}
extern "C" void* TradeWork_CreateInPlace(void* mem) {
    TradeWork_ResolveFuncs();
    return Work_GenericCtor(mem, 0x58, g_TradeWork_Vtable);
}
extern "C" void* TradeWork_Factory(void) {
    TradeWork_ResolveFuncs();
    void* p = Work_Alloc(0x58);
    return p ? Work_GenericCtor(p, 0x58, g_TradeWork_Vtable) : nullptr;
}
REGISTER_TYPE(TradeWork, "Work", &TradeWork_Factory, &TradeWork_CreateInPlace);
