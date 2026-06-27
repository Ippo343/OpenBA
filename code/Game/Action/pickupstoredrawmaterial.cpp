// code/Game/Action/pickupstoredrawmaterial.cpp
// Clean C++ reimplementation of PickupStoredRawMaterial action methods.
// Source: Game/Action/PickupStoredRawMaterial.cpp (from profile build debug strings)
//
// PickupStoredRawMaterial -- 0x48 bytes. Picks up materials from a storage building.
// Inherits: Object::Internal -> Action (0x20) -> PickupStoredRawMaterial
// Factory: 0x140022d60, Vtable: 0x14015ff80
//
// Vtable (10 slots):
//   [0] +0x00 ScalarDtor    = (base Action)
//   [1] +0x08 VectorDtor    = (base Action, unhookable)
//   [2] +0x10 BaseDtor      = (base Action, unhookable)
//   [3] +0x18 Serialize     = 0x1400ca0d0
//   [4] +0x20 AbstractImpl  = 0x1400c9d40 (complex storage query/transfer)
//   [5] +0x28 Execute       = 0x1400c4a90 (base Action, returns 0)
//   [6] +0x30 Enter         = 0x1400c4b90 (base Action, void nop)
//   [7] +0x38 Exit          = 0x1400c4ba0 (base Action, void nop)
//   [8] +0x40 CheckEntity   = 0x1400c9c70 (undo pickup on entity remove)
//   [9] +0x48 GetTarget     = 0x1400c9bf0
//
// Field layout (0x48 bytes):
//   +0x00-0x1F: Action base (vtable, Internal base, entity ptr, status byte)
//   +0x20: InternalBasePtr  targetBuilding  (building with storage to pick from)
//   +0x28: InternalBasePtr  pickupEntity    (entity doing the pickup, e.g. citizen)
//   +0x30: void*            materialList    (array of {ExternalPtr, int32_t count} pairs, 0x10 each)
//   +0x38: int32_t          materialCount   (number of entries in material list)
//   +0x40: bool             partialAllowed  (if true, can pick up less than requested)
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- it is portable for standalone exe.

#include "PickupStoredRawMaterial.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h -- PickupStoredRawMaterial-specific helpers)
// ============================================================

// FUN_14012d150 -- StorageComponent::SubtractItem(storage, itemType, count)
// Subtracts count of itemType from storage. If count reaches 0, removes the slot.
typedef void (*s_fn_Storage_SubtractItem_t)(void* storageComp, int64_t itemType, int count);
static s_fn_Storage_SubtractItem_t s_fn_Storage_SubtractItem = nullptr;

// FUN_14012d000 -- StorageComponent::AddItem(storage, itemType, count)
typedef void (*s_fn_Storage_AddItem_t)(void* storageComp, int64_t itemType, int count);
static s_fn_Storage_AddItem_t s_fn_Storage_AddItem = nullptr;

// FUN_14000da70 -- Array resize for 16-byte elements (material list)
typedef void (*s_fn_ArrayResize16_t)(void* arrayBase, int newCount);
static s_fn_ArrayResize16_t s_fn_ArrayResize16 = nullptr;

// DAT_1401c7158 -- type ID for IsDerivedFrom check on +0x20 (building entity)
static uint32_t* s_typeId_1401c7158 = nullptr;

// DAT_1401c7cb8 -- RawMaterialDescription type ID
static uint32_t* s_typeId_RawMatDesc = nullptr;

// DAT_1401c7e78 -- StorageComponent type ID
static uint32_t* s_typeId_StorageComp = nullptr;

static bool s_funcs_resolved = false;

// ============================================================
// Resolution -- called once from Install
// ============================================================
bool PickupStoredRawMaterial_ResolveFuncs() {
    if (s_funcs_resolved) return true;

    s_fn_Storage_SubtractItem = reinterpret_cast<s_fn_Storage_SubtractItem_t>(
        ResolveAddr(Module::Exe, 0x14012d150));

    s_fn_Storage_AddItem = reinterpret_cast<s_fn_Storage_AddItem_t>(
        ResolveAddr(Module::Exe, 0x14012d000));

    s_fn_ArrayResize16 = reinterpret_cast<s_fn_ArrayResize16_t>(
        ResolveAddr(Module::Exe, 0x14000da70));

    s_typeId_1401c7158 = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7158));

    s_typeId_RawMatDesc = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7cb8));

    s_typeId_StorageComp = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7e78));

    bool ok = s_fn_Storage_SubtractItem && s_fn_Storage_AddItem &&
              s_fn_ArrayResize16 &&
              s_typeId_1401c7158 && s_typeId_RawMatDesc && s_typeId_StorageComp &&
              g_TypeId_ComponentEntity &&
              fn_InternalBasePtr_CheckPtr && fn_InternalBasePtr_Ctor &&
              fn_IsDerivedFrom && fn_InternalBasePtr_Serialize &&
              fn_ExternalPtr_BuildSerialize &&
              g_fn_Serializer_Ctor && g_fn_Serializer_Dtor &&
              g_fn_Serialize_Bool && g_fn_Serialize_Int;

#ifdef _DEBUG
    printf("[PickupStoredRawMaterial] ResolveFuncs: SubItem=%p AddItem=%p Resize16=%p\n",
           (void*)s_fn_Storage_SubtractItem, (void*)s_fn_Storage_AddItem,
           (void*)s_fn_ArrayResize16);
    printf("[PickupStoredRawMaterial]   typeId7158=%p RawMat=%p Storage=%p\n",
           (void*)s_typeId_1401c7158, (void*)s_typeId_RawMatDesc, (void*)s_typeId_StorageComp);
    if (!ok) printf("[PickupStoredRawMaterial] ERROR: some function pointers not resolved!\n");
#endif

    s_funcs_resolved = ok;
    return ok;
}

// ============================================================
// Helper: FindComponent by type ID on an entity's component array
// Generic pattern: entity +0x30 = component IBP array, +0x38 = count
// Uses vtable+0x10 (GetTypeId) and IsDerivedFrom to match.
// ============================================================
// Generic FindComponent: arrayOffset/countOffset differ by object type.
// ComponentEntity: array=+0x30, count=+0x38
// External description objects (e.g. material types): array=+0x28, count=+0x30
static void* FindComponentByTypeId(void* entity, uint32_t* typeIdPtr,
                                   int arrayOffset, int countOffset) {
    uintptr_t ent = (uintptr_t)entity;
    int compCount = *(int*)(ent + countOffset);
    uintptr_t compArray = *(uintptr_t*)(ent + arrayOffset);

    for (int i = 0; i < compCount; i++) {
        void** compIBP = (void**)(compArray + (uintptr_t)i * 8);
        fn_InternalBasePtr_CheckPtr(compIBP);

        void* comp = *compIBP;
        // Call vtable+0x10 (GetTypeId)
        typedef uint32_t (*GetTypeId_t)(void*);
        uint32_t typeId = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);

        if (typeId == *typeIdPtr) {
            fn_InternalBasePtr_CheckPtr(compIBP);
            void* found = *compIBP;
            if (fn_IsDerivedFrom(found, *typeIdPtr)) {
                return found;
            }
            return nullptr;
        }
    }
    return nullptr;
}

// ============================================================
// PickupStoredRawMaterial::Serialize (0x1400ca0d0) -- slot 3
// Serializes: status(+0x18), building(+0x20), entity(+0x28),
//             material count(+0x38), material list(+0x30), partial flag(+0x40)
// ============================================================
void PickupStoredRawMaterial_Serialize_Impl(PickupStoredRawMaterial* self, void* stream) {
    uint8_t* s = (uint8_t*)self;
    uint8_t ser1[24];
    uint8_t ser2[24];

    // Serialize status byte (+0x18)
    g_fn_Serializer_Ctor(ser1, stream, 0);
    g_fn_Serialize_Bool(ser1, s + 0x18, nullptr);
    g_fn_Serializer_Dtor(ser1);

    // Serialize entity refs, material list, and flags
    g_fn_Serializer_Ctor(ser2, stream, 0);

    // Serialize target building (+0x20) with type check
    fn_InternalBasePtr_Serialize(s + 0x20, ser2);
    if (*(void**)(s + 0x20) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x20), *s_typeId_1401c7158)) {
            *(void**)(s + 0x20) = nullptr;
        }
    }

    // Serialize pickup entity (+0x28) with ComponentEntity type check
    fn_InternalBasePtr_Serialize(s + 0x28, ser2);
    if (*(void**)(s + 0x28) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x28), *g_TypeId_ComponentEntity)) {
            *(void**)(s + 0x28) = nullptr;
        }
    }

    // Serialize material count (+0x38)
    uint32_t matCount = *(uint32_t*)(s + 0x38);
    g_fn_Serialize_Int(ser2, &matCount, nullptr);

    // If loading (version check: ser2 internal stream offset+8 == 0), resize material array
    if (*(int*)(*(uintptr_t*)ser2 + 8) == 0) {
        s_fn_ArrayResize16((void*)(s + 0x30), (int)matCount);
    }

    // Serialize each material entry (ExternalPtr + int count, 0x10 bytes each)
    uintptr_t* matList = *(uintptr_t**)(s + 0x30);
    if ((int)matCount > 0) {
        uint64_t remaining = (uint64_t)matCount;
        do {
            // Serialize ExternalPtr (material type)
            fn_ExternalPtr_BuildSerialize(matList, ser2);
            // Serialize count (int at +0x08)
            g_fn_Serialize_Int(ser2, (void*)((uintptr_t)matList + 8), nullptr);
            matList = (uintptr_t*)((uintptr_t)matList + 0x10);
            remaining--;
        } while (remaining != 0);
    }

    // Serialize partial pickup flag (+0x40)
    g_fn_Serialize_Bool(ser2, s + 0x40, nullptr);

    g_fn_Serializer_Dtor(ser2);
}

// ============================================================
// PickupStoredRawMaterial::AbstractImpl (0x1400c9d40) -- slot 4
// Complex storage query and transfer logic:
// For each material in the list (+0x30):
//   1. Find RawMaterialDescription on material to get quantity per unit
//   2. If requested count <= 0, compute from storage capacity
//   3. Check available quantity in storage
//   4. If partial not allowed and insufficient: return 2 (fail)
//   5. If partial allowed, clamp to available; if 0: return 2
//   6. Subtract reserved amount from storage
//   7. Add to citizen's parent's storage via SubtractItem + AddItem
// Returns 0 on success, 2 on insufficient materials.
// ============================================================
int PickupStoredRawMaterial_AbstractImpl_Impl(PickupStoredRawMaterial* self, void* actionStack) {
    PickupStoredRawMaterial_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    (void)actionStack;
    uint8_t* s = (uint8_t*)self;

    // Find storage component on pickup entity (+0x28)
    fn_InternalBasePtr_CheckPtr(s + 0x28);
    uintptr_t storageIBP = 0;
    fn_FindComponent_Storage(*(void**)(s + 0x28), &storageIBP);

    int matIdx = 0;
    if (matIdx >= *(int*)(s + 0x38)) {
        return 0;
    }

    int64_t matOffset = 0;
    do {
        uintptr_t matList = *(uintptr_t*)(s + 0x30);
        int64_t materialType = *(int64_t*)(matList + matOffset);

        // Find RawMaterialDescription component on the material type
        // Material type is an External description object: components at +0x28/+0x30
        void* rawMatDesc = FindComponentByTypeId((void*)materialType, s_typeId_RawMatDesc, 0x28, 0x30);

        // Get requested count from material list (+0x08 in each entry)
        int requestedCount = *(int*)(matList + matOffset + 8);

        if (requestedCount < 1) {
            // Count not specified: compute from storage capacity
            fn_InternalBasePtr_CheckPtr(s + 0x20);
            void* targetBuilding = *(void**)(s + 0x20);
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)targetBuilding + 0x10));
            void* parentEntity = *(void**)((uintptr_t)targetBuilding + 0x10);

            // Find StorageComponent on parent entity
            void* parentStorage = FindComponentByTypeId(parentEntity, s_typeId_StorageComp, 0x30, 0x38);

            // Create IBP for parent storage, get max capacity and current count
            uintptr_t parentStorageIBP = 0;
            fn_InternalBasePtr_Ctor(&parentStorageIBP, parentStorage);
            fn_InternalBasePtr_CheckPtr(&parentStorageIBP);
            int maxCapacity = *(int*)(parentStorageIBP + 0x38);
            fn_InternalBasePtr_CheckPtr(&parentStorageIBP);
            int currentTotal = fn_StorageComponent_QueryCount((void*)parentStorageIBP);

            // Compute how many units we can pick up based on remaining capacity
            int quantityPerUnit = rawMatDesc ? *(int*)((uintptr_t)rawMatDesc + 0x48) : 1;
            requestedCount = (maxCapacity - currentTotal) / quantityPerUnit;
        }

        // Look up available quantity of this material in citizen's storage
        uintptr_t matList2 = *(uintptr_t*)(s + 0x30);
        fn_InternalBasePtr_CheckPtr(&storageIBP);
        int availableCount = 0;
        int slotCount = *(int*)(storageIBP + 0x30);
        if (slotCount > 0) {
            int64_t* itemArray = *(int64_t**)(storageIBP + 0x28);
            for (int i = 0; i < slotCount; i++) {
                if (itemArray[i * 2] == *(int64_t*)(matList2 + matOffset)) {
                    if (i >= 0) {
                        availableCount = (int)itemArray[i * 2 + 1];
                    }
                    break;
                }
            }
        }

        // Check availability
        bool partialAllowed = *(uint8_t*)(s + 0x40) != 0;
        if (!partialAllowed) {
            if (availableCount < requestedCount) {
                return 2;  // Insufficient, fail
            }
        } else {
            if (availableCount < requestedCount) {
                requestedCount = availableCount;
            }
            if (requestedCount == 0) {
                return 2;  // Nothing to pick up
            }
        }

        // Subtract reserved amount from citizen's storage
        uintptr_t matList3 = *(uintptr_t*)(s + 0x30);
        fn_InternalBasePtr_CheckPtr(&storageIBP);
        slotCount = *(int*)(storageIBP + 0x30);
        if (slotCount > 0) {
            int64_t* itemArray = *(int64_t**)(storageIBP + 0x28);
            for (int i = 0; i < slotCount; i++) {
                if (itemArray[i * 2] == *(int64_t*)(matList3 + matOffset)) {
                    if (i >= 0) {
                        int* reservedPtr = (int*)((uintptr_t)itemArray + (uintptr_t)i * 0x10 + 0xc);
                        *reservedPtr -= requestedCount;
                        if (*(int*)((uintptr_t)itemArray + 0xc + (uintptr_t)i * 0x10) < 0) {
                            *(int*)((uintptr_t)itemArray + 0xc + (uintptr_t)i * 0x10) = 0;
                        }
                    }
                    break;
                }
            }
        }

        // Subtract from citizen's storage via SubtractItem
        uintptr_t matList4 = *(uintptr_t*)(s + 0x30);
        fn_InternalBasePtr_CheckPtr(&storageIBP);
        s_fn_Storage_SubtractItem((void*)storageIBP, *(int64_t*)(matList4 + (int64_t)matIdx * 0x10), requestedCount);

        // Add to target building's parent storage
        uintptr_t matList5 = *(uintptr_t*)(s + 0x30);
        fn_InternalBasePtr_CheckPtr(s + 0x20);
        void* targetBuilding = *(void**)(s + 0x20);
        int64_t itemType = *(int64_t*)(matList5 + matOffset);
        fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)targetBuilding + 0x10));
        void* parentEntity = *(void**)((uintptr_t)targetBuilding + 0x10);

        // Find StorageComponent on parent entity
        void* parentStorage = FindComponentByTypeId(parentEntity, s_typeId_StorageComp, 0x30, 0x38);

        uintptr_t parentStorageIBP2[2];
        fn_InternalBasePtr_Ctor(parentStorageIBP2, parentStorage);
        fn_InternalBasePtr_CheckPtr(parentStorageIBP2);
        s_fn_Storage_AddItem((void*)parentStorageIBP2[0], itemType, requestedCount);

        matOffset += 0x10;
        matIdx++;
    } while (matIdx < *(int*)(s + 0x38));

    return 0;
}

// ============================================================
// PickupStoredRawMaterial::CheckEntity (0x1400c9c70) -- slot 8
// Undoes pickup when entity is removed.
// Iterates material list, subtracts reserved amounts from citizen's storage.
// ============================================================
void PickupStoredRawMaterial_CheckEntity_Impl(PickupStoredRawMaterial* self) {
    PickupStoredRawMaterial_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    uint8_t* s = (uint8_t*)self;

    // Find storage component on pickup entity (+0x28)
    fn_InternalBasePtr_CheckPtr(s + 0x28);
    uintptr_t storageIBP = 0;
    fn_FindComponent_Storage(*(void**)(s + 0x28), &storageIBP);

    int matCount = *(int*)(s + 0x38);
    if (matCount <= 0) return;

    int64_t matOffset = 0;
    for (int i = 0; i < matCount; i++) {
        uintptr_t matList = *(uintptr_t*)(s + 0x30);
        fn_InternalBasePtr_CheckPtr(&storageIBP);

        int slotCount = *(int*)(storageIBP + 0x30);
        if (slotCount > 0) {
            int64_t* itemArray = *(int64_t**)(storageIBP + 0x28);
            for (int j = 0; j < slotCount; j++) {
                if (itemArray[j * 2] == *(int64_t*)(matList + matOffset)) {
                    if (j >= 0) {
                        // Subtract the requested count from the reserved field (+0xc)
                        int* reservedPtr = (int*)((uintptr_t)itemArray + (uintptr_t)j * 0x10 + 0xc);
                        *reservedPtr -= *(int*)(matList + matOffset + 8);
                        if (*(int*)((uintptr_t)itemArray + 0xc + (uintptr_t)j * 0x10) < 0) {
                            *(int*)((uintptr_t)itemArray + 0xc + (uintptr_t)j * 0x10) = 0;
                        }
                    }
                    break;
                }
            }
        }

        matOffset += 0x10;
    }
}

// ============================================================
// PickupStoredRawMaterial::GetTarget (0x1400c9bf0) -- slot 9
// Returns true if param matches entity at +0x28, or
// matches the parent entity (+0x10) of the building at +0x20.
// ============================================================
bool PickupStoredRawMaterial_GetTarget_Impl(PickupStoredRawMaterial* self, void* param) {
    PickupStoredRawMaterial_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    uint8_t* s = (uint8_t*)self;

    // Check match with pickup entity at +0x28
    fn_InternalBasePtr_CheckPtr(s + 0x28);
    uintptr_t entityVal = *(uintptr_t*)(s + 0x28);
    if (*(uintptr_t*)param == entityVal) {
        return true;
    }

    // Check match with +0x20's parent (+0x10)
    fn_InternalBasePtr_CheckPtr(s + 0x20);
    uintptr_t building = *(uintptr_t*)(s + 0x20);
    uintptr_t parentEntity = *(uintptr_t*)(building + 0x10);
    fn_InternalBasePtr_CheckPtr(param);
    return parentEntity == *(uintptr_t*)param;
}

// ============================================================
// Standalone factory + vtable — Action family (notes/FACTORY_CASCADE_145_BRIEF.md).
// Vtable @ 0x14015ff80 (10 slots), size 0x48, parent "Action".
// Shared slot helpers live in action_base_standalone.{h,cpp}.
// ============================================================
#ifdef STANDALONE_EXE
#include "action_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t PickupStoredRawMaterial_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("PickupStoredRawMaterial");
    return c;
}

static void* g_PickupStoredRawMaterial_Vtable[10] = {
    (void*)&Action_ScalarDtor,        // +0x00 ScalarDtor
    (void*)&Action_Slot1,             // +0x08 slot1 (returns null)
    (void*)&PickupStoredRawMaterial_GetTypeId,           // +0x10 GetTypeId
    (void*)&PickupStoredRawMaterial_Serialize_Impl,      // +0x18 Serialize
    (void*)&PickupStoredRawMaterial_AbstractImpl_Impl,   // +0x20 AbstractImpl
    (void*)&Action_BaseExecute,   // +0x28 Execute
    (void*)&Action_Enter,  // +0x30 Enter
    (void*)&Action_Exit,   // +0x38 Exit
    (void*)&PickupStoredRawMaterial_CheckEntity_Impl,  // +0x40 CheckEntity
    (void*)&PickupStoredRawMaterial_GetTarget_Impl, // +0x48 GetTarget
};

extern "C" void RegisterPickupStoredRawMaterialVtable() {
    RegisterExeAddr(0x14015ff80, (uintptr_t)g_PickupStoredRawMaterial_Vtable);
}
extern "C" void* PickupStoredRawMaterial_CreateInPlace(void* mem) {
    PickupStoredRawMaterial_ResolveFuncs();
    return Action_InitInPlace(mem, 0x48, g_PickupStoredRawMaterial_Vtable);
}
extern "C" void* PickupStoredRawMaterial_Factory(void) {
    PickupStoredRawMaterial_ResolveFuncs();
    return Action_Alloc(0x48, g_PickupStoredRawMaterial_Vtable);
}
REGISTER_TYPE(PickupStoredRawMaterial, "Action", &PickupStoredRawMaterial_Factory, &PickupStoredRawMaterial_CreateInPlace);
#endif // STANDALONE_EXE
