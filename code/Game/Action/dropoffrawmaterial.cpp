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
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G
#include "Component/CitizenComponent.h"   // ENDGAME B.3.9 C4: CitizenComponent::TypeId()
#include "Entity/ComponentEntity.h"

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h — DropOffRawMaterial-specific helpers)
// ============================================================


// Object::InternalBasePtr::DeletePtr — clears entity ref during dtor
static void (*s_fn_IBP_DeletePtr)(void* self) = nullptr;

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

// ENDGAME B.3.9 CB: retail reads this record field ONLY in the class's own TypeName
// slot (0x1401c5f38 = the NAME field (+8) of DropOffRawMaterial's record, read by
// DropOffRawMaterial_Slot1 0x1400225f0); DECLARE_EXE_TYPE answers that slot, so the tree's copy of it is gone.

static bool s_funcs_resolved = false;

// ============================================================
// Resolution — called once from Install
// ============================================================
bool DropOffRawMaterial_ResolveFuncs() {
    if (s_funcs_resolved) return true;


    // InternalBasePtr::DeletePtr
    s_fn_IBP_DeletePtr = (void(*)(void*))fn_InternalBasePtr_DeletePtr;

    // DropOffRawMaterial_Serialize_sub_7620 — resource transfer helper
    s_fn_DropOffSub = (s_fn_DropOffSub_t)ResolveAddr(Module::Exe, 0x1400c7620);

    // Entity_impl_7570 — drop all inventory
    s_fn_DropAllInventory = (s_fn_DropAllInventory_t)ResolveAddr(Module::Exe, 0x1400c7570);

    bool ok = s_fn_IBP_DeletePtr &&
              s_fn_DropOffSub &&
              s_fn_DropAllInventory &&
              fn_IsDerivedFrom &&
              fn_MemFreeStatic && fn_ExternalPtr_BuildSerialize;

#ifdef _DEBUG
    printf("[DropOffRawMaterial] ResolveFuncs: IBPDel=%p DropSub=%p\n",
           (void*)s_fn_IBP_DeletePtr,
           (void*)s_fn_DropOffSub);
    printf("[DropOffRawMaterial]   DropAll=%p CompEnt=%08x\n",
           (void*)s_fn_DropAllInventory, ComponentEntity::TypeId());
    if (!ok) printf("[DropOffRawMaterial] ERROR: some function pointers not resolved!\n");
#endif

    s_funcs_resolved = ok;
    return ok;
}

// ============================================================
// Serialize (0x1400c7b10)
// Serializes: status(+0x18), storage(+0x30), source(+0x38), resource(+0x28), flag(+0x20), count(+0x24)
// ============================================================
void DropOffRawMaterial::Persist(void* stream) {
    // Reached through the registered vtable, which the factory path never
    // has to touch — an exe-ctor-built action arrives here with the file
    // statics still null. Lazy-resolve like the other entry points do.
    DropOffRawMaterial_ResolveFuncs();
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    alignas(8) uint8_t ser1[24];
    alignas(8) uint8_t ser2[24];

    // Serialize status byte (+0x18)
    ::new (static_cast<void*>(ser1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(_status, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();   // B.3.4 bridge

    // Serialize entity refs, resource type, flag, and count
    ::new (static_cast<void*>(ser2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize target storage entity ref (+0x30)
    reinterpret_cast<Object::InternalBasePtr*>(s + 0x30)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (_field30 != nullptr) {
        if (!fn_IsDerivedFrom(_field30, CitizenComponent::TypeId())) {
            _field30 = nullptr;
        }
    }

    // Serialize source entity ref (+0x38)
    reinterpret_cast<Object::InternalBasePtr*>(s + 0x38)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (_field38 != nullptr) {
        if (!fn_IsDerivedFrom(_field38, ComponentEntity::TypeId())) {
            _field38 = nullptr;
        }
    }

    // Serialize resource type ExternalPtr (+0x28)
    fn_ExternalPtr_BuildSerialize(s + 0x28, ser2);

    // Serialize flag (+0x20) and count (+0x24)
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<bool*>(s + 0x20), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(s + 0x24), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser2)->~Serializer();   // B.3.4 bridge
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
int  DropOffRawMaterial::AbstractImpl(void* actionStack) {
    DropOffRawMaterial_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    uint8_t* s = reinterpret_cast<uint8_t*>(this);

    { // [HAUL-DIAG] gated: did the citizen's executor actually RUN a dropoff?
    }

    // If source entity is null, drop all inventory items
    if (_field38 == nullptr) {
        s_fn_DropAllInventory(this);
        return 1;
    }

    // Find StorageComponent on source entity at +0x38
    uintptr_t storageIBP = 0;
    fn_FindComponent_Storage(_field38, &storageIBP);

    // If resource type (+0x28) is specified, transfer just that type
    if (_field28 != 0) {
        uint64_t result = s_fn_DropOffSub(this, actionStack, &storageIBP,
                                          *(uint64_t*)(s + 0x28));
        if ((int)result == 2) {
            return 2;
        }
        return 1;
    }

    // Resource type not specified: enumerate all items in the source entity's
    // parent's storage component, then transfer each.
    uintptr_t targetEntity = *(uintptr_t*)(s + 0x30);
    uintptr_t parentEntity = *(uintptr_t*)(targetEntity + 0x10);

    uintptr_t parentStorageIBP = 0;
    fn_FindComponent_Storage((void*)parentEntity, &parentStorageIBP);

    // Build dynamic array of (resourceType, count) pairs from parent's storage
    uint64_t* itemArray = nullptr;
    uint32_t itemCount = 0;
    uint32_t itemCapacity = 0;

    int numItems = *(int*)(parentStorageIBP + 0x30);

    int srcIdx = 0;
    if (numItems > 0) {
        do {
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
        } while (srcIdx < *(int*)(parentStorageIBP + 0x30));
    }

    // Transfer each resource type that has positive count
    bool anyPushed = false;
    if ((int)itemCount > 0) {
        uint64_t remaining = (uint64_t)itemCount;
        uint64_t* ptr = itemArray;
        do {
            if ((int)ptr[1] > 0) {
                uint64_t result = s_fn_DropOffSub(this, actionStack, &storageIBP, ptr[0]);
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
void DropOffRawMaterial::CheckEntity() {
    DropOffRawMaterial_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    s_fn_DropAllInventory(this);
}

// ============================================================
// GetTarget (0x1400c7270) — slot 9
// Returns true if param matches source entity at +0x38, or
// the target storage entity at +0x30's parent entity (+0x10).
// ============================================================
bool DropOffRawMaterial::GetTarget(void* param) {
    DropOffRawMaterial_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    uint8_t* s = reinterpret_cast<uint8_t*>(this);

    void* sourceEntity = _field38;

    if (*(void**)param == sourceEntity) {
        return true;
    }

    uintptr_t targetStorage = *(uintptr_t*)(s + 0x30);
    uintptr_t parentEntity = *(uintptr_t*)(targetStorage + 0x10);
    return parentEntity == *(uintptr_t*)param;
}

// ============================================================
// class DropOffRawMaterial (DropOffRawMaterial.h) — ENDGAME B.3.8 ACT.
// The hand-built g_DropOffRawMaterial_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

// create 0x140022600 (inlined ctor). The tree's factory zeroed every field above
// +0x10; kept (not a behaviour change of this step).
DropOffRawMaterial::DropOffRawMaterial()
    : _field20(0),
      _pad21{},
      _field24(0),
      _field28(nullptr),
      _field30(nullptr),
      _field38(nullptr) {}

// [0] 0x140022670
Object::Base* DropOffRawMaterial::DeletingDtor(unsigned flags) {
    this->~DropOffRawMaterial();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [3] 0x1400c7b10
// [4] 0x1400c7300
// [8] 0x1400c72f0 (JMP 0x1400c7570)
// [9] 0x1400c7270

void* DropOffRawMaterial::CreateNewObject() {
    DropOffRawMaterial_ResolveFuncs();
    void* p = Object::detail::InternalMemAlloc(sizeof(DropOffRawMaterial), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) DropOffRawMaterial());
}
void* DropOffRawMaterial::CreateInPlaceObject(void* storage) {
    DropOffRawMaterial_ResolveFuncs();
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) DropOffRawMaterial());
}


DEFINE_EXE_TYPE(DropOffRawMaterial, "Action")
