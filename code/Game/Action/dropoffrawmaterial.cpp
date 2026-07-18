// code/Game/Action/dropoffrawmaterial.cpp
// Clean C++ reimplementation of DropOffRawMaterial action methods.
// Source: Game/Action/DropOffRawMaterial.cpp (from profile build debug strings)
//
// DropOffRawMaterial — 0x40 bytes. Drops off raw material to storage.
// Inherits: Object::Internal -> Action (0x20) -> DropOffRawMaterial
// Factory: 0x140022600, Vtable: 0x14015fd80
//
// Vtable (10 slots):
//   [0] +0x00 ScalarDtor    = 0x140022670
//   [1] +0x08 VectorDtor    = 0x1400225f0 (returns type ptr, unhookable)
//   [2] +0x10 BaseDtor      = 0x1400225e0 (returns TypeInfo, unhookable)
//   [3] +0x18 Serialize     = 0x1400c7b10
//   [4] +0x20 AbstractImpl  = 0x1400c7300 (complex resource transfer)
//   [5] +0x28 Execute       = 0x1400c4a90 (base Action, returns 0)
//   [6] +0x30 Enter         = 0x1400c4b90 (base Action, void nop)
//   [7] +0x38 Exit          = 0x1400c4ba0 (base Action, void nop)
//   [8] +0x40 CheckEntity   = 0x1400c72f0 (JMP thunk to 0x1400c7570, unhookable)
//   [9] +0x48 GetTarget     = 0x1400c7270
//
// Field layout (0x40 bytes):
//   +0x00-0x1F: Action base (vtable, Internal base, entity ptr, status byte)
//   +0x20: uint8_t  flag          (bool: if set, create raw material on drop failure)
//   +0x24: int32_t  count         (max count to drop, -1 = unlimited)
//   +0x28: void*    resourceType  (InternalBasePtr, ExternalPtr to resource description)
//   +0x30: void*    targetStorage (InternalBasePtr, storage entity to drop into)
//   +0x38: void*    sourceEntity  (InternalBasePtr, source entity carrying resources)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "DropOffRawMaterial.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h — DropOffRawMaterial-specific helpers)
// ============================================================

// Object::Internal::~Internal(this) — via IAT at 0x140152770
static void (*s_fn_ObjectInternal_Dtor)(void* self) = nullptr;

// Object::InternalBasePtr::DeletePtr — clears entity ref during dtor
static void (*s_fn_IBP_DeletePtr)(void* self) = nullptr;

// Object::InternalBasePtr::Serialize(this, serializer) — via IAT at 0x140152aa0
static void (*s_fn_IBP_Serialize)(void* basePtr, void* serializer) = nullptr;

// FUN_140002340 — ExternalPtr_BuildSerialize (for resource type at +0x28)
// Already in engine_funcs.h as fn_ExternalPtr_BuildSerialize

// DropOffRawMaterial_Serialize_sub_7620 — actual resource transfer sub-helper
// Params: (self, actionStack, storageIBP, resourceType)
typedef uint64_t (*s_fn_DropOffSub_t)(void* self, void* actionStack, void* storageIBP, uint64_t resourceType);
static s_fn_DropOffSub_t s_fn_DropOffSub = nullptr;

// Entity_impl_7570 — drop off ALL inventory items (called when +0x38 is null)
// Also used as CheckEntity slot (slot 8 is a JMP to this)
typedef void (*s_fn_DropAllInventory_t)(void* self);
static s_fn_DropAllInventory_t s_fn_DropAllInventory = nullptr;

// Type IDs for IsDerivedFrom checks in Serialize
static uint32_t* s_typeId_1401c7158 = nullptr;  // entity type ref for +0x30

// Vtable addresses for dtor chain
static uintptr_t s_vtable_DropOffRawMaterial = 0;
static uintptr_t s_vtable_Action = 0;

// TypeInfo for BaseDtor (slot 2) — DAT_1401c5f38
static uint32_t* s_typeInfo_DropOffRawMaterial = nullptr;

static bool s_funcs_resolved = false;

// ============================================================
// Resolution — called once from Install
// ============================================================
bool DropOffRawMaterial_ResolveFuncs() {
    if (s_funcs_resolved) return true;

    // Object::Internal dtor via IAT
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152770);
    if (iatDtor) s_fn_ObjectInternal_Dtor = *reinterpret_cast<void(**)(void*)>(iatDtor);

    // InternalBasePtr::DeletePtr
    s_fn_IBP_DeletePtr = (void(*)(void*))fn_InternalBasePtr_DeletePtr;

    // InternalBasePtr::Serialize via IAT at 0x140152aa0
    uintptr_t iatIBPSer = ResolveAddr(Module::Exe, 0x140152aa0);
    if (iatIBPSer) s_fn_IBP_Serialize = *reinterpret_cast<void(**)(void*, void*)>(iatIBPSer);

    // DropOffRawMaterial_Serialize_sub_7620 — resource transfer helper
    s_fn_DropOffSub = (s_fn_DropOffSub_t)ResolveAddr(Module::Exe, 0x1400c7620);

    // Entity_impl_7570 — drop all inventory
    s_fn_DropAllInventory = (s_fn_DropAllInventory_t)ResolveAddr(Module::Exe, 0x1400c7570);

    // Type IDs
    s_typeId_1401c7158 = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7158));

    // Vtable addresses
    s_vtable_DropOffRawMaterial = ResolveAddr(Module::Exe, 0x14015fd80);
    s_vtable_Action = ResolveAddr(Module::Exe, 0x14015faa0);  // Action base vtable

    // TypeInfo for BaseDtor — read from [0x1401c5f38]
    s_typeInfo_DropOffRawMaterial = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5f38));

    bool ok = s_fn_ObjectInternal_Dtor && s_fn_IBP_DeletePtr &&
              s_fn_IBP_Serialize && s_fn_DropOffSub &&
              s_fn_DropAllInventory &&
              s_typeId_1401c7158 && g_TypeId_ComponentEntity &&
              s_vtable_DropOffRawMaterial && s_vtable_Action &&
              s_typeInfo_DropOffRawMaterial &&
              fn_InternalBasePtr_CheckPtr && fn_IsDerivedFrom &&
              fn_MemFreeStatic && fn_ExternalPtr_BuildSerialize &&
              g_fn_Serializer_Ctor && g_fn_Serializer_Dtor &&
              g_fn_Serialize_Bool && g_fn_Serialize_Int;

#ifdef _DEBUG
    printf("[DropOffRawMaterial] ResolveFuncs: ObjDtor=%p IBPDel=%p IBPSer=%p DropSub=%p\n",
           (void*)s_fn_ObjectInternal_Dtor, (void*)s_fn_IBP_DeletePtr,
           (void*)s_fn_IBP_Serialize, (void*)s_fn_DropOffSub);
    printf("[DropOffRawMaterial]   DropAll=%p typeId7158=%p CompEnt=%p\n",
           (void*)s_fn_DropAllInventory,
           (void*)s_typeId_1401c7158, (void*)g_TypeId_ComponentEntity);
    if (!ok) printf("[DropOffRawMaterial] ERROR: some function pointers not resolved!\n");
#endif

    s_funcs_resolved = ok;
    return ok;
}

// ============================================================
// ScalarDtor (0x140022670)
// Set vtables, cleanup entity ref at +0x10, call ~Internal, optionally free.
// ============================================================
void* DropOffRawMaterial_ScalarDtor_Impl(DropOffRawMaterial* self, uint32_t flags) {
    uint8_t* s = (uint8_t*)self;

    // Set DropOffRawMaterial vtable (for correct virtual dispatch during destruction)
    *(uintptr_t*)s = s_vtable_DropOffRawMaterial;
    // Set Action vtable (base class destructor phase)
    *(uintptr_t*)s = s_vtable_Action;

    // If entity ref at +0x10 is non-null, delete it
    if (*(void**)(s + 0x10) != nullptr) {
        s_fn_IBP_DeletePtr(s + 0x10);
    }

    // Call Object::Internal::~Internal
    s_fn_ObjectInternal_Dtor(self);

    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// BaseDtor (0x1400225e0) — slot 2
// Returns TypeInfo for DropOffRawMaterial (tiny, unhookable).
// ============================================================
uint32_t DropOffRawMaterial_BaseDtor_Impl() {
    return *s_typeInfo_DropOffRawMaterial;
}

// ============================================================
// Serialize (0x1400c7b10)
// Serializes: status(+0x18), storage(+0x30), source(+0x38), resource(+0x28), flag(+0x20), count(+0x24)
// ============================================================
void DropOffRawMaterial_Serialize_Impl(DropOffRawMaterial* self, void* stream) {
    uint8_t* s = (uint8_t*)self;
    uint8_t ser1[24];
    uint8_t ser2[24];

    // Serialize status byte (+0x18)
    g_fn_Serializer_Ctor(ser1, stream, 0);
    g_fn_Serialize_Bool(ser1, s + 0x18, nullptr);
    g_fn_Serializer_Dtor(ser1);

    // Serialize entity refs, resource type, flag, and count
    g_fn_Serializer_Ctor(ser2, stream, 0);

    // Serialize target storage entity ref (+0x30)
    s_fn_IBP_Serialize(s + 0x30, ser2);
    if (*(void**)(s + 0x30) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x30), *s_typeId_1401c7158)) {
            *(void**)(s + 0x30) = nullptr;
        }
    }

    // Serialize source entity ref (+0x38)
    s_fn_IBP_Serialize(s + 0x38, ser2);
    if (*(void**)(s + 0x38) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x38), *g_TypeId_ComponentEntity)) {
            *(void**)(s + 0x38) = nullptr;
        }
    }

    // Serialize resource type ExternalPtr (+0x28)
    fn_ExternalPtr_BuildSerialize(s + 0x28, ser2);

    // Serialize flag (+0x20) and count (+0x24)
    g_fn_Serialize_Bool(ser2, s + 0x20, nullptr);
    g_fn_Serialize_Int(ser2, s + 0x24, nullptr);

    g_fn_Serializer_Dtor(ser2);
}

// ============================================================
// AbstractImpl (0x1400c7300) — slot 4
// Complex resource transfer logic.
// If source entity (+0x38) is null: drops all inventory via Entity_impl_7570.
// If resource type (+0x28) is specified: transfer that one type via sub_7620.
// If resource type is null: enumerate all resource types in source entity's
//   parent's storage, build array, transfer each via sub_7620.
// Returns 1 (needs more work) or 2 (action pushed, e.g. MoveTo for full storage).
// ============================================================
int DropOffRawMaterial_AbstractImpl_Impl(DropOffRawMaterial* self, void* actionStack) {
    DropOffRawMaterial_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    uint8_t* s = (uint8_t*)self;

    { // [HAUL-DIAG] gated: did the citizen's executor actually RUN a dropoff?
        static int t = -1;
        if (t < 0) {
            const char* e = std::getenv("BANISHED_BUILD_TRACE");
            const char* d = std::getenv("BANISHED_DELIVER_TRACE");
            t = ((e && e[0] && e[0] != '0') || (d && d[0] && d[0] != '0')) ? 1 : 0;
        }
        if (t == 1) {
            std::fprintf(stderr, "[BuildTrace] DropOffRawMaterial::AbstractImpl RUN self=%p src(+0x38)=%p tgtStorage(+0x30)=%p resType(+0x28)=%p count(+0x24)=%d\n",
                         (void*)self, *(void**)(s + 0x38), *(void**)(s + 0x30), *(void**)(s + 0x28), *(int*)(s + 0x24));
            std::fflush(stderr);
        }
    }

    // If source entity is null, drop all inventory items
    if (*(void**)(s + 0x38) == nullptr) {
        s_fn_DropAllInventory(self);
        return 1;
    }

    // Find StorageComponent on source entity at +0x38
    fn_InternalBasePtr_CheckPtr(s + 0x38);
    uintptr_t storageIBP = 0;
    fn_FindComponent_Storage(*(void**)(s + 0x38), &storageIBP);

    // If resource type (+0x28) is specified, transfer just that type
    if (*(void**)(s + 0x28) != 0) {
        uint64_t result = s_fn_DropOffSub(self, actionStack, &storageIBP,
                                          *(uint64_t*)(s + 0x28));
        if ((int)result == 2) {
            return 2;
        }
        return 1;
    }

    // Resource type not specified: enumerate all items in the source entity's
    // parent's storage component, then transfer each.
    fn_InternalBasePtr_CheckPtr(s + 0x30);
    uintptr_t targetEntity = *(uintptr_t*)(s + 0x30);
    uintptr_t parentEntity = *(uintptr_t*)(targetEntity + 0x10);
    fn_InternalBasePtr_CheckPtr(&parentEntity);

    uintptr_t parentStorageIBP = 0;
    fn_FindComponent_Storage((void*)parentEntity, &parentStorageIBP);

    // Build dynamic array of (resourceType, count) pairs from parent's storage
    uint64_t* itemArray = nullptr;
    uint32_t itemCount = 0;
    uint32_t itemCapacity = 0;

    fn_InternalBasePtr_CheckPtr(&parentStorageIBP);
    int numItems = *(int*)(parentStorageIBP + 0x30);

    int srcIdx = 0;
    if (numItems > 0) {
        do {
            fn_InternalBasePtr_CheckPtr(&parentStorageIBP);
            uintptr_t inventoryArray = *(uintptr_t*)(parentStorageIBP + 0x28);

            uint32_t newCount = itemCount + 1;
            if (newCount != itemCount) {
                // Grow array if needed
                if ((int)itemCapacity < (int)newCount) {
                    itemCapacity = newCount;
                    if (itemCount != 0) {
                        itemCapacity = newCount * 3;
                    }
                    uint64_t* newArray = (uint64_t*)fn_MemAlloc(
                        g_pMemAllocSingleton, (int64_t)(int)itemCapacity * 16, 8);

                    // Copy existing and new entries
                    uint32_t copyStart = 0;
                    if (itemArray == newArray) {
                        copyStart = itemCount;
                    }
                    for (uint32_t i = copyStart; (int)i < (int)newCount; i++) {
                        if (newArray != nullptr) {
                            uint64_t* dst = newArray + (int64_t)(int)i * 2;
                            uint64_t* src;
                            if ((int)i < (int)itemCount) {
                                src = itemArray + (int64_t)(int)i * 2;
                            } else {
                                // New entry: copy from inventory
                                src = (uint64_t*)(inventoryArray + (int64_t)srcIdx * 0x10);
                            }
                            dst[0] = src[0];
                            dst[1] = src[1];
                        }
                    }

                    if (itemArray != newArray) {
                        fn_MemFreeStatic(itemArray);
                    }
                    itemArray = newArray;
                } else {
                    // Array has capacity, copy new entry at the end
                    for (uint32_t i = itemCount; (int)i < (int)newCount; i++) {
                        if (itemArray != nullptr) {
                            uint64_t* dst = itemArray + (int64_t)(int)i * 2;
                            uint64_t* src = (uint64_t*)(inventoryArray + (int64_t)srcIdx * 0x10);
                            dst[0] = src[0];
                            dst[1] = src[1];
                        }
                    }
                }
                itemCount = newCount;
            }

            srcIdx++;
            fn_InternalBasePtr_CheckPtr(&parentStorageIBP);
        } while (srcIdx < *(int*)(parentStorageIBP + 0x30));
    }

    // Transfer each resource type that has positive count
    bool anyPushed = false;
    if ((int)itemCount > 0) {
        uint64_t remaining = (uint64_t)itemCount;
        uint64_t* ptr = itemArray;
        do {
            if ((int)ptr[1] > 0) {
                uint64_t result = s_fn_DropOffSub(self, actionStack, &storageIBP, ptr[0]);
                anyPushed = anyPushed || ((int)result == 2);
            }
            ptr += 2;
            remaining--;
        } while (remaining != 0);

        if (anyPushed) {
            fn_MemFreeStatic(itemArray);
            return 2;
        }
    }

    fn_MemFreeStatic(itemArray);
    return 1;
}

// ============================================================
// CheckEntity (0x1400c72f0) — slot 8
// JMP thunk to Entity_impl_7570 (drops off all inventory items).
// Only 5 bytes (JMP rel32), not hookable.
// ============================================================
void DropOffRawMaterial_CheckEntity_Impl(DropOffRawMaterial* self) {
    DropOffRawMaterial_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    s_fn_DropAllInventory(self);
}

// ============================================================
// GetTarget (0x1400c7270) — slot 9
// Returns true if param matches source entity at +0x38, or
// the target storage entity at +0x30's parent entity (+0x10).
// ============================================================
bool DropOffRawMaterial_GetTarget_Impl(DropOffRawMaterial* self, void* param) {
    DropOffRawMaterial_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    uint8_t* s = (uint8_t*)self;

    fn_InternalBasePtr_CheckPtr(s + 0x38);
    void* sourceEntity = *(void**)(s + 0x38);

    if (*(void**)param == sourceEntity) {
        return true;
    }

    fn_InternalBasePtr_CheckPtr(s + 0x30);
    uintptr_t targetStorage = *(uintptr_t*)(s + 0x30);
    uintptr_t parentEntity = *(uintptr_t*)(targetStorage + 0x10);
    fn_InternalBasePtr_CheckPtr(param);
    return parentEntity == *(uintptr_t*)param;
}

// ============================================================
// Standalone factory + vtable — Action family (notes/FACTORY_CASCADE_145_BRIEF.md).
// Vtable @ 0x14015fd80 (10 slots), size 0x40, parent "Action".
// Shared slot helpers live in action_base_standalone.{h,cpp}.
// ============================================================
#include "action_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t DropOffRawMaterial_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("DropOffRawMaterial");
    return c;
}

static void* g_DropOffRawMaterial_Vtable[10] = {
    (void*)&Action_ScalarDtor,        // +0x00 ScalarDtor
    (void*)&Action_Slot1,             // +0x08 slot1 (returns null)
    (void*)&DropOffRawMaterial_GetTypeId,           // +0x10 GetTypeId
    (void*)&DropOffRawMaterial_Serialize_Impl,      // +0x18 Serialize
    (void*)&DropOffRawMaterial_AbstractImpl_Impl,   // +0x20 AbstractImpl
    (void*)&Action_BaseExecute,   // +0x28 Execute
    (void*)&Action_Enter,  // +0x30 Enter
    (void*)&Action_Exit,   // +0x38 Exit
    (void*)&DropOffRawMaterial_CheckEntity_Impl,  // +0x40 CheckEntity
    (void*)&DropOffRawMaterial_GetTarget_Impl, // +0x48 GetTarget
};

extern "C" void RegisterDropOffRawMaterialVtable() {
    RegisterExeAddr(0x14015fd80, (uintptr_t)g_DropOffRawMaterial_Vtable);
}
extern "C" void* DropOffRawMaterial_CreateInPlace(void* mem) {
    DropOffRawMaterial_ResolveFuncs();
    return Action_InitInPlace(mem, 0x40, g_DropOffRawMaterial_Vtable);
}
extern "C" void* DropOffRawMaterial_Factory(void) {
    DropOffRawMaterial_ResolveFuncs();
    return Action_Alloc(0x40, g_DropOffRawMaterial_Vtable);
}
REGISTER_TYPE(DropOffRawMaterial, "Action", &DropOffRawMaterial_Factory, &DropOffRawMaterial_CreateInPlace);
