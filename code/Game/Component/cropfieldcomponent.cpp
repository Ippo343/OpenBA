// code/Game/Component/cropfieldcomponent.cpp
// Clean reimplementation of CropFieldComponent lifecycle methods from Application-x64.exe.
//
// CropFieldComponent (0x68 bytes) extends ControllerComponent with crop flags
// and entity reference arrays for field tiles.
//
// Vtable at 0x140162ec0. One non-trivial hookable override:
//   Shutdown (0x14011ae70) — Component_Serialize + empty serializer + versioned block
//                            with entity ref validation, crop flags, IBP array, entity list
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/cropfieldcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer ctor/dtor via dispatch slots
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(this, bool*, name)
typedef void (*fn_Serializer_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_Serializer_SerializeBool_t s_fn_Serializer_SerializeBool = nullptr;

// IO::Serializer::Serialize(this, int*, name)
typedef void (*fn_Serializer_SerializeInt_t)(void* self, int* value, const char* name);
static fn_Serializer_SerializeInt_t s_fn_Serializer_SerializeInt = nullptr;

// FUN_14011aa20 — validate entity references
typedef void (*fn_ValidateEntityRefs_t)(uintptr_t self);
static fn_ValidateEntityRefs_t s_fn_ValidateEntityRefs = nullptr;

// FUN_140002340 — serialize InternalBasePtr array
typedef void (*fn_SerializeIBPArray_t)(void* arrayPtr, void* serializer);
static fn_SerializeIBPArray_t s_fn_SerializeIBPArray = nullptr;

// CropFieldComponent_helper_35b0 — resize entity list
typedef void (*fn_ResizeEntityList_t)(void* arrayPtr, int newSize);
static fn_ResizeEntityList_t s_fn_ResizeEntityList = nullptr;

// CropFieldComponent_helper_36c0 — serialize entity list entries
typedef void (*fn_SerializeEntityList_t)(void* serializer, int count, uintptr_t arrayPtr);
static fn_SerializeEntityList_t s_fn_SerializeEntityList = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a58);
    if (slot) s_fn_Serializer_SerializeBool = *reinterpret_cast<fn_Serializer_SerializeBool_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serializer_SerializeInt = *reinterpret_cast<fn_Serializer_SerializeInt_t*>(slot);

    s_fn_ValidateEntityRefs = reinterpret_cast<fn_ValidateEntityRefs_t>(
        ResolveAddr(Module::Exe, 0x14011aa20));

    s_fn_SerializeIBPArray = reinterpret_cast<fn_SerializeIBPArray_t>(
        ResolveAddr(Module::Exe, 0x140002340));

    // NOTE: the real helper entry points are 0x1400335b0 / 0x1400336c0
    // (CropFieldComponent_helper_35b0 / _36c0). The earlier typedefs above
    // referenced 0x1400035b0 / 0x1400036c0, which are UNRELATED functions
    // (AutoPickupComponent_ScalarDtor mid-body / BoatComponent_GetType) — a
    // dropped-digit typo. Corrected here so the guarded calls hit the right code.
    s_fn_ResizeEntityList = reinterpret_cast<fn_ResizeEntityList_t>(
        ResolveAddr(Module::Exe, 0x1400335b0));

    s_fn_SerializeEntityList = reinterpret_cast<fn_SerializeEntityList_t>(
        ResolveAddr(Module::Exe, 0x1400336c0));

    s_initialized = true;
}

// ============================================================
// Reimplemented exe helpers (callee bodies for the guarded call sites)
// ============================================================

// Extra sub-helpers needed by the reimpl bodies below.

// FUN_1400052c0 — FindComponent (BuildStateComponent variant) on a ComponentEntity.
//   RCX=entity, RDX=outIBP  (returns outIBP)
typedef void* (*fn_FindComp_BS_local_t)(int64_t entity, void* outIBP);
static fn_FindComp_BS_local_t s_fn_FindComp_BS_local = nullptr;

// Object::Base::IsA dispatch slot at 0x140151410 — RCX=obj, EDX=typeId -> bool
typedef bool (*fn_Base_IsA_t)(void* obj, uint32_t typeId);
static fn_Base_IsA_t s_fn_Base_IsA = nullptr;

// IO::Serializer::Serialize(int*) dispatch slot at 0x140152ac8 (already loaded as
// s_fn_Serializer_SerializeInt) and Serialize(bool*) at 0x140152a58 (SerializeBool).
// Object::InternalBasePtr::Serialize at dispatch slot 0x140152a90.
typedef void (*fn_IBP_Serialize_t)(void* self, void* serializer);
static fn_IBP_Serialize_t s_fn_IBP_Serialize2 = nullptr;

// Type-id globals consulted by the helpers.
static uint32_t* s_pTypeId_ComponentEntity = nullptr;  // DWORD_1401c5d70
static uint32_t* s_pTypeId_BuildStateComp = nullptr;   // DAT_1401c7b30

static bool s_cropExtraInit = false;
static void EnsureCropExtras() {
    if (s_cropExtraInit) return;
    s_fn_FindComp_BS_local = reinterpret_cast<fn_FindComp_BS_local_t>(
        ResolveAddr(Module::Exe, 0x1400052c0));
    uintptr_t slot = ResolveAddr(Module::Exe, 0x140151410);
    if (slot) s_fn_Base_IsA = *reinterpret_cast<fn_Base_IsA_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a90);
    if (slot) s_fn_IBP_Serialize2 = *reinterpret_cast<fn_IBP_Serialize_t*>(slot);
    s_pTypeId_ComponentEntity = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5d70));
    s_pTypeId_BuildStateComp = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7b30));
    s_cropExtraInit = true;
}

// Reimpl of FUN_14011aa20 (validate field-tile entity refs: drop stale/dead refs)
// Iterates the +0x58 array (stride 0x20, count at +0x60). For each "assigned"
// entry (state==1 at +0x4) it looks up the world entity by id; if it is gone, not
// a ComponentEntity, or its build-state component link no longer matches, the
// entry is reset to state 2 with null IBPs.
void CropFieldComponent_ValidateEntityRefs(uintptr_t self) {
    EnsureHelpers();
    EnsureCropExtras();
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t world = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t map = *(uintptr_t*)(world + 0xf8);

    int count = *(int*)(self + 0x60);
    uintptr_t arr = *(uintptr_t*)(self + 0x58);
    for (int i = 0; i < count; i++) {
        uint8_t* entry = (uint8_t*)(arr + (int64_t)i * 0x20);
        if (*(int*)(entry + 0x4) != 1) continue;

        int id = *(int*)entry;
        void* obj = *(void**)(*(uintptr_t*)(map + 0x80) + 8 + (int64_t)id * 0x30);
        bool keep = false;
        if (obj != nullptr) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&obj);
            if (s_fn_Base_IsA && s_pTypeId_ComponentEntity &&
                s_fn_Base_IsA(obj, *s_pTypeId_ComponentEntity)) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&obj);
                uint8_t outIBP[8] = {0};
                if (s_fn_FindComp_BS_local)
                    s_fn_FindComp_BS_local((int64_t)obj, outIBP);
                if (*(void**)outIBP != nullptr) {
                    if (fn_InternalBasePtr_CheckPtr)
                        fn_InternalBasePtr_CheckPtr((void*)(entry + 0x8));
                    if (obj == *(void**)(entry + 0x8)) keep = true;
                }
            }
        }
        if (keep) continue;

        // Stale: reset entry to state 2 and null the two IBP slots.
        *(int*)(entry + 0x4) = 2;
        uint64_t nullIBP = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
        *(uint64_t*)(entry + 0x8) = nullIBP;
        nullIBP = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
        *(uint64_t*)(entry + 0x10) = nullIBP;
    }
}

// Reimpl of CropFieldComponent_helper_35b0 @0x1400335b0
// (grow/shrink the +0x58 entity-ref array; element stride 0x20)
// Matches the standard grow-and-copy: count at [+8], capacity at [+0xc]. On grow,
// allocates cap*0x20, copies old [int,int,ptr,ptr,bool] entries, zero-inits new
// ones, frees old. On shrink, just updates count (no realloc).
void CropFieldComponent_ResizeEntityList(void* arrayPtr, int newSize) {
    EnsureHelpers();
    int64_t* base = reinterpret_cast<int64_t*>(arrayPtr);
    int oldCount = (int)base[1];
    if (newSize == oldCount) return;

    void* oldData = (void*)base[0];
    *(int*)(base + 1) = newSize;
    int* pCap = (int*)((uintptr_t)base + 0xc);
    if (*pCap < newSize) {
        *pCap = newSize;
        if (newSize == oldCount + 1 && oldCount != 0) {
            *pCap = newSize * 3;
        }
        void* newData = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)(*pCap) * 0x20, 8);
        base[0] = (int64_t)newData;
    }

    int startIdx = (oldData == (void*)base[0]) ? oldCount : 0;
    for (int64_t idx = startIdx; idx < newSize; idx++) {
        uint8_t* dst = (uint8_t*)(base[0] + idx * 0x20);
        if (idx < oldCount) {
            uint8_t* src = (uint8_t*)((uintptr_t)oldData + idx * 0x20);
            *(uint32_t*)(dst + 0) = *(uint32_t*)(src + 0);
            *(uint32_t*)(dst + 4) = *(uint32_t*)(src + 4);
            *(uint64_t*)(dst + 8) = 0;
            *(uint64_t*)(dst + 8) = *(uint64_t*)(src + 8);
            *(uint64_t*)(dst + 16) = 0;
            *(uint64_t*)(dst + 16) = *(uint64_t*)(src + 16);
            *(uint8_t*)(dst + 24) = *(uint8_t*)(src + 24);
        } else {
            *(uint64_t*)(dst + 8) = 0;
            *(uint64_t*)(dst + 16) = 0;
        }
    }

    if (oldData != (void*)base[0]) {
        fn_MemFreeStatic(oldData);
    }
}

// Reimpl of CropFieldComponent_helper_36c0 @0x1400336c0
// (serialize the entity-ref array entries; element stride 0x20)
// Per entry: int id, int state, IBP(ComponentEntity), IBP(BuildStateComponent),
// bool. After each IBP is read it is type-validated and nulled if the type is wrong.
void CropFieldComponent_SerializeEntityList(void* serializer, int count, uintptr_t arrayPtr) {
    EnsureHelpers();
    EnsureCropExtras();
    if (count <= 0) return;
    uintptr_t entry = arrayPtr;
    for (int i = 0; i < count; i++) {
        if (s_fn_Serializer_SerializeInt) {
            s_fn_Serializer_SerializeInt(serializer, (int*)(entry + 0), nullptr);
            s_fn_Serializer_SerializeInt(serializer, (int*)(entry + 4), nullptr);
        }
        // IBP #1 -> ComponentEntity
        if (s_fn_IBP_Serialize2) s_fn_IBP_Serialize2((void*)(entry + 8), serializer);
        if (*(void**)(entry + 8) != nullptr && s_pTypeId_ComponentEntity &&
            !fn_IsDerivedFrom(*(void**)(entry + 8), *s_pTypeId_ComponentEntity)) {
            *(uint64_t*)(entry + 8) = 0;
        }
        // IBP #2 -> BuildStateComponent
        if (s_fn_IBP_Serialize2) s_fn_IBP_Serialize2((void*)(entry + 0x10), serializer);
        if (*(void**)(entry + 0x10) != nullptr && s_pTypeId_BuildStateComp &&
            !fn_IsDerivedFrom(*(void**)(entry + 0x10), *s_pTypeId_BuildStateComp)) {
            *(uint64_t*)(entry + 0x10) = 0;
        }
        if (s_fn_Serializer_SerializeBool)
            s_fn_Serializer_SerializeBool(serializer, (bool*)(entry + 0x18), nullptr);
        entry += 0x20;
    }
}

// ============================================================
// CropFieldComponent::Shutdown (vtable slot [3], 0x14011ae70)
// ============================================================
// Complex entity reference serialization:
//   1. Component_Serialize (base)
//   2. Empty serializer (v0, immediately destroyed)
//   3. Main serializer (v0):
//      a. FUN_14011aa20 — validate entity refs
//      b. Serialize cropFlags as 2 bools (+0x48, +0x49)
//      c. FUN_140002340 — serialize IBP array at +0x50
//      d. Serialize entity count from +0x60
//      e. On load: resize entity list at +0x58
//      f. CropFieldComponent_helper_36c0 — serialize entity list entries

void CropFieldComponent_Shutdown(CropFieldComponent* self, void* stream) {
    EnsureHelpers();

    // Base Component serialization
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Empty serializer block (version 0, immediately destroyed)
    uint8_t emptySerializer[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(emptySerializer, stream, 0);
    }
    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(emptySerializer);
    }

    // Main serializer block (version 0)
    uint8_t serializer[16];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializer, stream, 0);
    }

    // Validate entity references
    if (s_fn_ValidateEntityRefs) {
        s_fn_ValidateEntityRefs((uintptr_t)self);
    }

    // Serialize cropFlags as two bools (+0x48 and +0x49)
    if (s_fn_Serializer_SerializeBool) {
        s_fn_Serializer_SerializeBool(serializer, (bool*)((uintptr_t)self + 0x48), nullptr);
        s_fn_Serializer_SerializeBool(serializer, (bool*)((uintptr_t)self + 0x49), nullptr);
    }

    // Serialize InternalBasePtr array at +0x50
    if (s_fn_SerializeIBPArray) {
        s_fn_SerializeIBPArray((void*)((uintptr_t)self + 0x50), serializer);
    }

    // Serialize entity count from +0x60
    int entityCount = *(int*)((uintptr_t)self + 0x60);
    int countBuf = entityCount;
    if (s_fn_Serializer_SerializeInt) {
        s_fn_Serializer_SerializeInt(serializer, &countBuf, nullptr);
    }

    // On load (serializer mode == 0), resize entity list
    if (*(int*)((uintptr_t)serializer + 8) == 0) {
        if (s_fn_ResizeEntityList) {
            s_fn_ResizeEntityList((void*)((uintptr_t)self + 0x58), countBuf);
        }
    }

    // Serialize entity list entries
    if (s_fn_SerializeEntityList) {
        uintptr_t entityArray = *(uintptr_t*)((uintptr_t)self + 0x58);
        s_fn_SerializeEntityList(serializer, countBuf, entityArray);
    }

    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void CropFieldComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[CropFieldComponent] Funcs resolved: Component_Serialize=%p ValidateEntityRefs=%p "
           "SerializeIBPArray=%p ResizeEntityList=%p SerializeEntityList=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_ValidateEntityRefs,
           (void*)s_fn_SerializeIBPArray, (void*)s_fn_ResizeEntityList,
           (void*)s_fn_SerializeEntityList);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// Inert-but-valid CropFieldComponent: size 0x68 + Object::Internal base + shared stub
// vtable + capability flags +0x20=0x80 (from CropFieldComponent_Ctor) + zeroed body.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(CropFieldComponent, 0x68, 0x80)
REGISTER_TYPE(CropFieldComponent, "ControllerComponent",
              &CropFieldComponent_Factory, &CropFieldComponent_CreateInPlace);
