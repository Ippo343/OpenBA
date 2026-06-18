// code/Game/Component/residencecomponent.cpp
// Clean reimplementation of ResidenceComponent lifecycle methods from Application-x64.exe.
//
// ResidenceComponent is a ControllerComponent subclass (0x60 bytes).
// Manages citizen residence assignments (housing).
//
// Vtable at 0x1401642c0. Three non-trivial overrides:
//   Shutdown  (0x14012a170) — serialization with Time + arrays
//   Serialize (0x14012a120) — IsDerivedFrom check + field init from description
//   OnCreate  (0x14012a0e0) — calls init helper + registers with game state
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/residencecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base Shutdown) at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer ctor/dtor via dispatch slots
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(int*) — dispatch slot at 0x140152ac8
typedef void (*fn_Serialize_Int_t)(void* serializer, int* field, const char* name);
static fn_Serialize_Int_t s_fn_Serialize_Int = nullptr;

// ResidenceComponent_Controller_sub_7ad0 — resize resident array at +0x50
// Takes (longlong* arrayField, int newCount)
typedef void (*fn_ResizeResidentArray_t)(void* arrayFieldPtr, int newCount);
static fn_ResizeResidentArray_t s_fn_ResizeResidentArray = nullptr;

// ResidenceComponent_helper_7c50 — serialize resident array
// Takes (Serializer*, count, arrayPtr)
typedef void (*fn_SerializeResidentArray_t)(void* serializer, int count, void* arrayPtr);
static fn_SerializeResidentArray_t s_fn_SerializeResidentArray = nullptr;

// System::Time::Serialize — dispatch slot at 0x140152a08
typedef void (*fn_Time_Serialize_t)(void* timeObj, void* serializer);
static fn_Time_Serialize_t s_fn_Time_Serialize = nullptr;

// fn_IsDerivedFrom — dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

// DAT_1401c7d28 — ResidenceDescription type ID
static uint32_t* s_TypeId_ResidenceDescription = nullptr;

// ResidenceComponent_helper_9ef0 — init helper (called before register)
typedef void (*fn_ResidenceInit_t)(void* self);
static fn_ResidenceInit_t s_fn_ResidenceInit = nullptr;

// FUN_140002cc0 — register entity with world entity list
typedef void (*fn_RegisterEntity_t)(void* listPtr, void* entityPtr);
static fn_RegisterEntity_t s_fn_RegisterEntity = nullptr;

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

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serialize_Int = *reinterpret_cast<fn_Serialize_Int_t*>(slot);

    s_fn_ResizeResidentArray = reinterpret_cast<fn_ResizeResidentArray_t>(
        ResolveAddr(Module::Exe, 0x140037ad0));

    s_fn_SerializeResidentArray = reinterpret_cast<fn_SerializeResidentArray_t>(
        ResolveAddr(Module::Exe, 0x140037c50));

    // System::Time::Serialize via dispatch slot
    slot = ResolveAddr(Module::Exe, 0x140152aa8);
    if (slot) s_fn_Time_Serialize = *reinterpret_cast<fn_Time_Serialize_t*>(slot);

    // IsDerivedFrom via dispatch slot
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    // ResidenceDescription type ID
    s_TypeId_ResidenceDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7d28));

    s_fn_ResidenceInit = reinterpret_cast<fn_ResidenceInit_t>(
        ResolveAddr(Module::Exe, 0x140129ef0));

    s_fn_RegisterEntity = reinterpret_cast<fn_RegisterEntity_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));

    s_initialized = true;
}

// ============================================================
// ResidenceComponent::Shutdown (vtable slot [3], 0x14012a170)
// ============================================================
// Serializes Component base + empty serializer block + versioned
// serializer with: resident count (+0x58), resident array resize on load,
// resident array serialize, and Time::Serialize at +0x48.
//
// Original decompilation:
//   Component_Serialize(self, stream)
//   Serializer_Ctor(local1, stream, 0); Serializer_Dtor(local1)    // empty block
//   Serializer_Ctor(local2, stream, 0)
//   local_res8 = *(int*)(self+0x58)
//   Serialize_Int(local2, &local_res8, null)
//   if loading: ResizeResidentArray(self+0x50, local_res8)
//   SerializeResidentArray(local2, local_res8, *(self+0x50))
//   Time::Serialize(self+0x48, local2)
//   Serializer_Dtor(local2)

void ResidenceComponent_Shutdown(ResidenceComponent* self, void* stream) {
    EnsureHelpers();

    // Call base Component_Serialize
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Empty serializer block (version 0)
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        uint8_t serBuf1[24];
        s_fn_Serializer_Ctor(serBuf1, stream, 0);
        s_fn_Serializer_Dtor(serBuf1);
    }

    // Versioned serializer block (version 0) with resident data
    uint8_t serBuf2[24];
    int64_t* pSerData = (int64_t*)serBuf2;

    if (!s_fn_Serializer_Ctor || !s_fn_Serializer_Dtor) return;
    s_fn_Serializer_Ctor(serBuf2, stream, 0);

    // Serialize resident count via local copy
    // (same pattern as ConsumeProduceComponent — local copy needed so resize
    // sees old vs new count correctly)
    int localCount = *(int*)((uintptr_t)self + 0x58);
    if (s_fn_Serialize_Int) {
        s_fn_Serialize_Int(serBuf2, &localCount, nullptr);
    }

    // On load (direction == 0): resize resident array
    if (*(int*)(pSerData[0] + 8) == 0) {
        if (s_fn_ResizeResidentArray) {
            s_fn_ResizeResidentArray((void*)((uintptr_t)self + 0x50), localCount);
        }
    }

    // Serialize resident array
    if (s_fn_SerializeResidentArray) {
        void* arrayPtr = *(void**)((uintptr_t)self + 0x50);
        s_fn_SerializeResidentArray(serBuf2, localCount, arrayPtr);
    }

    // Serialize Time at +0x48
    if (s_fn_Time_Serialize) {
        s_fn_Time_Serialize((void*)((uintptr_t)self + 0x48), serBuf2);
    }

    s_fn_Serializer_Dtor(serBuf2);
}

// ============================================================
// ResidenceComponent::Serialize (vtable slot [4], 0x14012a120)
// ============================================================
// Checks IsDerivedFrom on +0x18 with ResidenceDescription type,
// reads resident capacity from description at +0x20, resizes
// resident array at +0x50 to that capacity.
//
// Original decompilation:
//   obj = *(self+0x18)
//   derived = IsDerivedFrom(obj, DAT_1401c7d28)
//   pvVar2 = derived ? obj : nullptr
//   ResizeResidentArray(self+0x50, *(pvVar2+0x20))

void ResidenceComponent_Serialize(ResidenceComponent* self) {
    EnsureHelpers();

    if (!s_fn_IsDerivedFrom_dispatch || !s_TypeId_ResidenceDescription ||
        !s_fn_ResizeResidentArray)
        return;

    void* obj = self->_unk18;
    if (!obj) return;

    bool derived = s_fn_IsDerivedFrom_dispatch(obj, *s_TypeId_ResidenceDescription);
    void* desc = derived ? obj : nullptr;
    if (!desc) return;

    // Read resident capacity from description at +0x20
    int capacity = *(int*)((uintptr_t)desc + 0x20);

    // Resize resident array at +0x50
    s_fn_ResizeResidentArray((void*)((uintptr_t)self + 0x50), capacity);
}

// ============================================================
// ResidenceComponent::OnCreate (vtable slot [7], 0x14012a0e0)
// ============================================================
// Calls init helper (ResidenceComponent_helper_9ef0), then registers
// owning entity with game world residence list at worldMgr+0x20.
//
// Original decompilation:
//   ResidenceComponent_helper_9ef0(self)
//   local_res8 = *(self+0x10)    // owner entity
//   FUN_140002cc0(*(g_pGameWorldSingleton+0xf0)+0x20, &local_res8)

void ResidenceComponent_OnCreate(ResidenceComponent* self) {
    EnsureHelpers();

    // Call init helper
    if (s_fn_ResidenceInit) {
        s_fn_ResidenceInit(self);
    }

    if (!s_fn_RegisterEntity || !g_pGameWorldSingleton || !*g_pGameWorldSingleton)
        return;

    // Build an InternalBasePtr on the stack with the owner entity
    uint64_t entityPtr = (uint64_t)(uintptr_t)self->owner;

    // Get the world manager from game state
    uintptr_t gameState = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xf0);

    // Register entity at worldMgr+0x20 (residence entity list)
    s_fn_RegisterEntity((void*)(worldMgr + 0x20), &entityPtr);
}

// ============================================================
// Reimpl helper resolution for the resident-array functions
// ============================================================
// ResidenceComponent_helper_5ec0(dst, src) — per-element move on grow (0x140035ec0)
typedef void (*fn_Resident_MoveElem_t)(void* dst, void* src);
static fn_Resident_MoveElem_t s_fn_Resident_MoveElem = nullptr;
// FUN_1400032e0(entry, innerCount) — resize inner IBP array on load (0x1400032e0)
typedef void (*fn_Resident_ResizeInner_t)(void* entry, int innerCount);
static fn_Resident_ResizeInner_t s_fn_Resident_ResizeInner = nullptr;
// Object::InternalBasePtr::Serialize(this, serializer)
typedef void (*fn_IBP_Serialize_t)(void* self, void* serializer);
static fn_IBP_Serialize_t s_fn_IBP_Serialize = nullptr;
// DWORD_1401c7158 — CitizenComponent (resident) type ID
static uint32_t* s_TypeId_Resident = nullptr;
// FindComponent(MeshInstance/...) + worker assignment helpers used by ResidenceInit
typedef void* (*fn_FindComp_t)(uintptr_t entity, void* retbuf);
static fn_FindComp_t s_fn_Entity_FindComponent_MeshInstance = nullptr;
typedef void (*fn_SetMeshFlag_t)(void* comp, int idx, char param3);
static fn_SetMeshFlag_t s_fn_SetMeshFlag = nullptr;   // FUN_14012c3f0
typedef void (*fn_AssignWorkplace_t)(void* citizen, void* ibp, int slot);
static fn_AssignWorkplace_t s_fn_AssignWorkplace = nullptr;  // CitizenComponent_AssignWorkplace

static bool s_residentHelpersResolved = false;
static void EnsureResidentHelpers() {
    if (s_residentHelpersResolved) return;
    s_fn_Resident_MoveElem = reinterpret_cast<fn_Resident_MoveElem_t>(
        ResolveAddr(Module::Exe, 0x140035ec0));
    s_fn_Resident_ResizeInner = reinterpret_cast<fn_Resident_ResizeInner_t>(
        ResolveAddr(Module::Exe, 0x1400032e0));
    s_fn_IBP_Serialize = reinterpret_cast<fn_IBP_Serialize_t>(
        ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z"));
    if (!s_fn_IBP_Serialize)
        s_fn_IBP_Serialize = reinterpret_cast<fn_IBP_Serialize_t>(ResolveOrdinal(2990));
    s_TypeId_Resident = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7158));
    s_fn_Entity_FindComponent_MeshInstance = reinterpret_cast<fn_FindComp_t>(
        ResolveAddr(Module::Exe, 0x140002f90));
    s_fn_SetMeshFlag = reinterpret_cast<fn_SetMeshFlag_t>(
        ResolveAddr(Module::Exe, 0x14012c3f0));
    s_fn_AssignWorkplace = reinterpret_cast<fn_AssignWorkplace_t>(
        ResolveAddr(Module::Exe, 0x1401163d0));
    s_residentHelpersResolved = true;
}

// ============================================================
// Reimpl of FUN_140037ad0 (ResidenceComponent_Controller_sub_7ad0:
//   grow/shrink the 16-byte-stride resident-slot array at the field this points to)
// ============================================================
// param_1 = address of {void* ptr (+0), int count (+8), int capacity (+0xc)}.
// On grow, new tail slots are zeroed; existing slots are moved with helper_5ec0
// when the buffer was reallocated. On shrink (param_2 < count) the dropped slots'
// inner buffers (slot[0]) are MemFree'd. When realloc happens, every old slot is
// freed and the old buffer released.
void ResidenceComponent_ResizeResidentArray(void* arrayFieldPtr, int newCount) {
    EnsureResidentHelpers();
    int64_t* field = (int64_t*)arrayFieldPtr;   // field[0]=ptr, [1] low=count, high=capacity
    int oldCount = (int)field[1];
    if (newCount == oldCount) return;

    int64_t* oldBuf = (int64_t*)field[0];
    *(int*)((uintptr_t)field + 8) = newCount;
    if (*(int*)((uintptr_t)field + 0xc) < newCount) {
        *(int*)((uintptr_t)field + 0xc) = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            *(int*)((uintptr_t)field + 0xc) = newCount * 3;
        }
        void* nb = fn_MemAlloc(g_pMemAllocSingleton,
                               (uint64_t)((int64_t)*(int*)((uintptr_t)field + 0xc) << 4), 8);
        field[0] = (int64_t)nb;
    }

    int start = (oldBuf == (int64_t*)field[0]) ? oldCount : 0;
    if (start < newCount) {
        int64_t* srcSlot = oldBuf + (int64_t)start * 2;
        for (int i = start; i < newCount; i++) {
            int64_t* slot = (int64_t*)(field[0] + (int64_t)i * 0x10);
            if (i < oldCount) {
                if (slot) {
                    slot[0] = 0;
                    slot[1] = 0;
                    if (s_fn_Resident_MoveElem) s_fn_Resident_MoveElem(slot, srcSlot);
                }
            } else {
                if (slot) { slot[0] = 0; slot[1] = 0; }
            }
            srcSlot += 2;
        }
    }

    if (oldBuf == (int64_t*)field[0]) {
        // No realloc: free inner buffers of the shrunk-off tail
        if (newCount < oldCount) {
            int64_t* p = oldBuf + (int64_t)newCount * 2;
            for (int i = 0; i < oldCount - newCount; i++) {
                fn_MemFreeStatic((void*)p[0]);
                p[0] = 0; p[1] = 0;
                p += 2;
            }
        }
    } else {
        // Reallocated: free every old slot's inner buffer, then the old buffer
        int64_t* p = oldBuf;
        for (int i = 0; i < oldCount; i++) {
            fn_MemFreeStatic((void*)p[0]);
            p[0] = 0; p[1] = 0;
            p += 2;
        }
        if (oldBuf) fn_MemFreeStatic(oldBuf);
    }
}

// ============================================================
// Reimpl of FUN_140037c50 (ResidenceComponent_helper_7c50:
//   serialize the nested resident IBP arrays)
// ============================================================
// For each of `count` outer entries (16-byte stride at arrayPtr): serialize the
// inner element count; on load grow the inner array; then serialize each inner
// InternalBasePtr and clear it if not derived from the resident type.
void ResidenceComponent_SerializeResidentArray(void* serializer, int count, void* arrayPtr) {
    EnsureHelpers();
    EnsureResidentHelpers();
    if (count <= 0) return;
    int64_t* entry = (int64_t*)arrayPtr;
    for (int o = 0; o < count; o++) {
        int innerCount = (int)entry[1];
        if (s_fn_Serialize_Int) s_fn_Serialize_Int(serializer, &innerCount, nullptr);
        // On load (serializer internal +8 == 0): resize the inner array
        if (*(int*)(*(int64_t*)serializer + 8) == 0) {
            if (s_fn_Resident_ResizeInner) s_fn_Resident_ResizeInner(entry, innerCount);
        }
        uintptr_t inner = (uintptr_t)entry[0];
        for (int i = 0; i < innerCount; i++) {
            void* ibp = (void*)(inner + (uintptr_t)i * 8);
            if (s_fn_IBP_Serialize) s_fn_IBP_Serialize(ibp, serializer);
            void* val = *(void**)ibp;
            if (val && s_TypeId_Resident && fn_IsDerivedFrom &&
                !fn_IsDerivedFrom(val, *s_TypeId_Resident)) {
                *(uint64_t*)ibp = 0;
            }
        }
        entry += 2;   // 16-byte stride
    }
}

// ============================================================
// Reimpl of FUN_140129ef0 (ResidenceComponent_helper_9ef0:
//   on create, refresh mesh flags and re-assign resident workplaces)
// ============================================================
void ResidenceComponent_ResidenceInit(void* selfP) {
    EnsureResidentHelpers();
    uintptr_t self = (uintptr_t)selfP;
    uint8_t ibpBuf[8];

    // Mesh flags 6 and 7
    for (int flag = 6; flag <= 7; flag++) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(self + 0x10));
        if (!s_fn_Entity_FindComponent_MeshInstance) break;
        uint8_t compBuf[8] = {0};
        s_fn_Entity_FindComponent_MeshInstance(*(uintptr_t*)(self + 0x10), compBuf);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(compBuf);
        void* comp = *(void**)compBuf;
        if (comp && s_fn_SetMeshFlag) s_fn_SetMeshFlag(comp, flag, '\0');
    }

    // Re-assign workplaces for each resident slot at +0x50 (count +0x58, stride 0x10)
    int slotCount = *(int*)(self + 0x58);
    for (int s = 0; s < slotCount; s++) {
        uintptr_t base = *(uintptr_t*)(self + 0x50);
        uintptr_t entry = base + (uintptr_t)s * 0x10;
        int innerCount = *(int*)(entry + 8);
        while (innerCount > 0) {
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(ibpBuf, nullptr);
            uintptr_t cur = *(uintptr_t*)(self + 0x50) + (uintptr_t)s * 0x10;
            void* residentSlotPtr = *(void**)cur;
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(residentSlotPtr);
            void* citizen = *(void**)residentSlotPtr;
            if (s_fn_AssignWorkplace) s_fn_AssignWorkplace(citizen, ibpBuf, -1);
            innerCount = *(int*)(*(uintptr_t*)(self + 0x50) + (uintptr_t)s * 0x10 + 8);
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void ResidenceComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[ResidenceComponent] Funcs resolved: Component_Serialize=%p "
           "ResizeArray=%p SerializeArray=%p TimeSer=%p Init=%p Register=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_ResizeResidentArray,
           (void*)s_fn_SerializeResidentArray, (void*)s_fn_Time_Serialize,
           (void*)s_fn_ResidenceInit, (void*)s_fn_RegisterEntity);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// Inert-but-valid ResidenceComponent: size 0x60 + Object::Internal base + shared stub
// vtable + capability flags +0x20=0x82 (from ResidenceComponent_Ctor) + zeroed body.
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(ResidenceComponent, 0x60, 0x82)
REGISTER_TYPE(ResidenceComponent, "ControllerComponent",
              &ResidenceComponent_Factory, &ResidenceComponent_CreateInPlace);
#endif // STANDALONE_EXE
