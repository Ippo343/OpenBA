// code/Game/Action/consumerawmaterial.cpp
// Clean C++ reimplementation of ConsumeRawMaterial action methods.
// Source: Game/Action/ConsumeRawMaterial.cpp (from profile build debug strings)
//
// ConsumeRawMaterial -- 0x40 bytes. Consumes raw materials from entity's inventory.
// Inherits: Object::Internal -> Action (0x20) -> ConsumeRawMaterial
// Factory: 0x140022090, Vtable: 0x14015fc20
//
// Vtable (10 slots):
//   [0] +0x00 ScalarDtor    = (base Action)
//   [1] +0x08 VectorDtor    = (base Action, unhookable)
//   [2] +0x10 BaseDtor      = (base Action, unhookable)
//   [3] +0x18 Serialize     = 0x1400c6120 (versioned, v0 legacy + v2 consume flag)
//   [4] +0x20 AbstractImpl  = 0x1400c5f50 (consume or check materials)
//   [5] +0x28 Execute       = 0x1400c4a90 (base Action, returns 0)
//   [6] +0x30 Enter         = 0x1400c4b90 (base Action, void nop)
//   [7] +0x38 Exit          = 0x1400c4ba0 (base Action, void nop)
//   [8] +0x40 CheckEntity   = 0x1400c5e30 (undo consumption if consume mode)
//   [9] +0x48 GetTarget     = 0x1400c5f10 (simple compare with +0x20)
//
// Field layout (0x40 bytes):
//   +0x00-0x1F: Action base (vtable, Internal base, entity ptr, status byte)
//   +0x20: InternalBasePtr  targetEntity    (entity whose storage to consume from)
//   +0x28: void*            materialList    (array of {ExternalPtr, int32_t count} pairs, 0x10 each)
//   +0x30: int32_t          materialCount   (number of entries in material list)
//   +0x38: bool             consumeMode     (true = subtract, false = just check availability)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "ConsumeRawMaterial.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdio>
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h -- ConsumeRawMaterial-specific helpers)
// ============================================================

// FUN_14012d150 -- StorageComponent::SubtractItem(storage, itemType, count)
typedef void (*s_fn_Storage_SubtractItem_t)(void* storageComp, int64_t itemType, int count);
static s_fn_Storage_SubtractItem_t s_fn_Storage_SubtractItem = nullptr;

// FUN_140088f90 -- GetResourceCategoryIndex(storageComp, materialExternalPtr) -> int
// Returns the resource category index for the given material, or -1 if not found.
typedef int (*s_fn_GetResourceCategoryIndex_t)(void* storageComp, int64_t materialPtr);
static s_fn_GetResourceCategoryIndex_t s_fn_GetResourceCategoryIndex = nullptr;

// FUN_14000da70 -- Array resize for 16-byte elements (material list)
typedef void (*s_fn_ArrayResize16_t)(void* arrayBase, int newCount);
static s_fn_ArrayResize16_t s_fn_ArrayResize16 = nullptr;

// DAT_1401c7158 -- type ID for CitizenAI-like component (used for entity type check)
// (Not directly used here, but included for reference)

static bool s_funcs_resolved = false;

// ============================================================
// Resolution -- called once from Install
// ============================================================
bool ConsumeRawMaterial_ResolveFuncs() {
    if (s_funcs_resolved) return true;

    s_fn_Storage_SubtractItem = reinterpret_cast<s_fn_Storage_SubtractItem_t>(
        ResolveAddr(Module::Exe, 0x14012d150));

    s_fn_GetResourceCategoryIndex = reinterpret_cast<s_fn_GetResourceCategoryIndex_t>(
        ResolveAddr(Module::Exe, 0x140088f90));

    s_fn_ArrayResize16 = reinterpret_cast<s_fn_ArrayResize16_t>(
        ResolveAddr(Module::Exe, 0x14000da70));

    bool ok = s_fn_Storage_SubtractItem && s_fn_GetResourceCategoryIndex &&
              s_fn_ArrayResize16 &&
              g_TypeId_ComponentEntity && g_pGameWorldSingleton &&
              fn_IsDerivedFrom &&
              fn_ExternalPtr_BuildSerialize;

#ifdef _DEBUG
    printf("[ConsumeRawMaterial] ResolveFuncs: SubItem=%p GetResCat=%p Resize16=%p\n",
           (void*)s_fn_Storage_SubtractItem, (void*)s_fn_GetResourceCategoryIndex,
           (void*)s_fn_ArrayResize16);
    if (!ok) printf("[ConsumeRawMaterial] ERROR: some function pointers not resolved!\n");
#endif

    s_funcs_resolved = ok;
    return ok;
}

// ============================================================
// ConsumeRawMaterial::Serialize (0x1400c6120) -- slot 3
// Versioned serialization (version 2):
//   v0: legacy format (single ExternalPtr + int)
//   v1+: entity ref(+0x20), material count(+0x30), material list(+0x28)
//   v2+: consume mode flag(+0x38)
// ============================================================
void ConsumeRawMaterial::Persist(void* stream) {
    // Lazy-resolve like every other method on this exe-ctor-built action. Serialize
    // was the one that never got it, so a save taken while one of these actions was
    // in flight called a null serializer pointer (EXEC of 0). Found live on
    // CompleteRoad once roads started being built; these four are the same shape.
    ConsumeRawMaterial_ResolveFuncs();
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    alignas(8) uint8_t ser1[24];
    alignas(8) uint8_t ser2[24];

    // Serialize status byte (+0x18)
    ::new (static_cast<void*>(ser1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(_status, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();   // B.3.4 bridge

    // Versioned serializer (version 2)
    ::new (static_cast<void*>(ser2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 2);   // B.3.4 bridge

    // Serialize target entity (+0x20) with ComponentEntity type check
    reinterpret_cast<Object::InternalBasePtr*>(s + 0x20)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (_field20 != nullptr) {
        if (!fn_IsDerivedFrom(_field20, *g_TypeId_ComponentEntity)) {
            _field20 = nullptr;
        }
    }

    // Check version for format selection
    int version = reinterpret_cast<IO::Serializer*>(ser2)->GetVersion();   // B.3.4 bridge

    if (version == 0) {
        // Legacy v0 format: single ExternalPtr + int (no material list)
        uintptr_t legacyExtPtr = 0;
        fn_ExternalPtr_BuildSerialize(&legacyExtPtr, ser2);
        uint32_t legacyCount;
        reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(&legacyCount), nullptr);   // B.3.4 bridge
    } else {
        // v1+ format: material count + material list array
        uint32_t matCount = *(uint32_t*)(s + 0x30);
        reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(&matCount), nullptr);   // B.3.4 bridge

        // If loading, resize the material array
        if (static_cast<int>(reinterpret_cast<IO::Serializer*>(ser2)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
            s_fn_ArrayResize16((void*)(s + 0x28), (int)matCount);
        }

        // Serialize each material entry (ExternalPtr + int count, 0x10 bytes each)
        uintptr_t* matList = *(uintptr_t**)(s + 0x28);
        if ((int)matCount > 0) {
            uint64_t remaining = (uint64_t)matCount;
            do {
                fn_ExternalPtr_BuildSerialize(matList, ser2);
                reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(((uintptr_t)matList + 8)), nullptr);   // B.3.4 bridge
                matList = (uintptr_t*)((uintptr_t)matList + 0x10);
                remaining--;
            } while (remaining != 0);
        }
    }

    // v2+: serialize consume mode flag (+0x38)
    int ver2 = reinterpret_cast<IO::Serializer*>(ser2)->GetVersion();   // B.3.4 bridge
    if (ver2 > 1) {
        reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<bool*>(s + 0x38), nullptr);   // B.3.4 bridge
    }

    reinterpret_cast<IO::Serializer*>(ser2)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// ConsumeRawMaterial::AbstractImpl (0x1400c5f50) -- slot 4
// If material count is 0: return 2 (nothing to consume).
// For each material in list:
//   If consumeMode == false (check only):
//     Look up available count in storage, subtract reserved.
//     If available < requested: return 2 (insufficient).
//   If consumeMode == true (consume):
//     Subtract reserved amount from storage reserved field.
//   Then: SubtractItem from storage, update global resource tracker.
// Returns 0 on success, 2 on failure.
// ============================================================
int  ConsumeRawMaterial::AbstractImpl(void* actionStack) {
    ConsumeRawMaterial_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    (void)actionStack;
    uint8_t* s = reinterpret_cast<uint8_t*>(this);

    int matCount = *(int*)(s + 0x30);
    if (matCount == 0) {
        return 2;
    }

    // Find StorageComponent on target entity (+0x20)
    uintptr_t storageIBP = 0;
    fn_FindComponent_Storage(_field20, &storageIBP);

    int matIdx = 0;
    int64_t matOffset = 0;

    if (matCount <= 0) {
        return 0;
    }

    do {
        uintptr_t matList = *(uintptr_t*)(s + 0x28);
        int64_t* matEntry = (int64_t*)(matList + matOffset);
        int requestedCount = (int)matEntry[1];

        if (_field38 == 0) {
            // Check mode: verify sufficient availability
            int availableCount = 0;
            int slotCount = *(int*)(storageIBP + 0x30);
            if (slotCount > 0) {
                int64_t* itemArray = *(int64_t**)(storageIBP + 0x28);
                for (int i = 0; i < slotCount; i++) {
                    if (itemArray[i * 2] == matEntry[0]) {
                        if (i >= 0) {
                            // available = count - reserved
                            availableCount = (int)itemArray[i * 2 + 1] -
                                           *(int*)((uintptr_t)itemArray + (uintptr_t)i * 0x10 + 0xc);
                        }
                        break;
                    }
                }
            }
            if (availableCount < requestedCount) {
                return 2;  // Insufficient
            }
        } else {
            // Consume mode: subtract from reserved field
            int slotCount = *(int*)(storageIBP + 0x30);
            if (slotCount > 0) {
                int64_t* itemArray = *(int64_t**)(storageIBP + 0x28);
                for (int i = 0; i < slotCount; i++) {
                    if (itemArray[i * 2] == matEntry[0]) {
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
        }

        // SubtractItem from storage
        void* storageComp = (void*)storageIBP;
        s_fn_Storage_SubtractItem(storageComp, matEntry[0], requestedCount);

        // Update global resource tracker
        uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
        uintptr_t resourceTracker = *(uintptr_t*)(gameWorld + 0xf0);
        int64_t consumedCount = matEntry[1];

        int categoryIndex = s_fn_GetResourceCategoryIndex(storageComp, matEntry[0]);
        if (categoryIndex >= 0) {
            int* trackerEntry = (int*)(*(uintptr_t*)(resourceTracker + 0x170) +
                                       (int64_t)categoryIndex * 0x20 + 0xc);
            *trackerEntry += (int)consumedCount;
        }

        matIdx++;
        matOffset += 0x10;
    } while (matIdx < *(int*)(s + 0x30));

    return 0;
}

// ============================================================
// ConsumeRawMaterial::CheckEntity (0x1400c5e30) -- slot 8
// Undoes consumption when entity is removed (only if consume mode is true).
// Iterates material list, subtracts reserved amounts from storage.
// ============================================================
void ConsumeRawMaterial::CheckEntity() {
    ConsumeRawMaterial_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    uint8_t* s = reinterpret_cast<uint8_t*>(this);

    // Only undo if consume mode is active
    if (_field38 == 0) {
        return;
    }

    // Find StorageComponent on target entity (+0x20)
    uintptr_t storageIBP = 0;
    fn_FindComponent_Storage(_field20, &storageIBP);

    int matCount = *(int*)(s + 0x30);
    if (matCount <= 0) return;

    int64_t matOffset = 0;
    for (int i = 0; i < matCount; i++) {
        uintptr_t matList = *(uintptr_t*)(s + 0x28);

        int slotCount = *(int*)(storageIBP + 0x30);
        if (slotCount > 0) {
            int64_t* itemArray = *(int64_t**)(storageIBP + 0x28);
            for (int j = 0; j < slotCount; j++) {
                if (itemArray[j * 2] == *(int64_t*)(matList + matOffset)) {
                    if (j >= 0) {
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
// ConsumeRawMaterial::GetTarget (0x1400c5f10) -- slot 9
// Simple compare: returns true if param matches entity at +0x20.
// ============================================================
bool ConsumeRawMaterial::GetTarget(void* param) {
    ConsumeRawMaterial_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    uint8_t* s = reinterpret_cast<uint8_t*>(this);

    return *(uintptr_t*)param == *(uintptr_t*)(s + 0x20);
}

// ============================================================
// class ConsumeRawMaterial (ConsumeRawMaterial.h) — ENDGAME B.3.8 ACT.
// The hand-built g_ConsumeRawMaterial_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

// create 0x1400220c0 (inlined ctor). The tree's factory zeroed every field above
// +0x10; kept (not a behaviour change of this step).
ConsumeRawMaterial::ConsumeRawMaterial()
    : _field20(nullptr),
      _field28(nullptr),
      _field30(nullptr),
      _field38(0),
      _pad39{} {}

// The dtor body of 0x140022130, between its own stamp and the Action stamp:
//   MOV RCX,[this+0x28]; CALL MemFree (IAT 0x140152b68, unconditional);
//   [this+0x28] = 0; [this+0x30] = 0 (the qword: count +0x30 and capacity +0x34).
// The tree's shared Action_ScalarDtor skipped it and leaked the consume list.
ConsumeRawMaterial::~ConsumeRawMaterial() {
    Object::detail::InternalMemFree(_field28);
    _field28 = nullptr;
    _field30 = nullptr;
}

// [0] 0x140022130
Object::Base* ConsumeRawMaterial::DeletingDtor(unsigned flags) {
    this->~ConsumeRawMaterial();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [3] 0x1400c6120
// [4] 0x1400c5f50
// [8] 0x1400c5e30
// [9] 0x1400c5f10

void* ConsumeRawMaterial::CreateNewObject() {
    ConsumeRawMaterial_ResolveFuncs();
    void* p = Object::detail::InternalMemAlloc(sizeof(ConsumeRawMaterial), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ConsumeRawMaterial());
}
void* ConsumeRawMaterial::CreateInPlaceObject(void* storage) {
    ConsumeRawMaterial_ResolveFuncs();
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ConsumeRawMaterial());
}

DEFINE_EXE_VTABLE(ConsumeRawMaterial, "_ZTV18ConsumeRawMaterial")

// B38 D5: every by-address reader of 0x14015fc20 was inside this block and now
// builds the object with the real ctor, so the row is gone. The function stays
// until the coordinator's address_registry hunk drops its call.
extern "C" void RegisterConsumeRawMaterialVtable() {}

DEFINE_EXE_TYPE(ConsumeRawMaterial, "Action")
