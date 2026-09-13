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
// Resident per-slot inner-array copy on grow.
// CALLER-BUG FIX: the literal was 0x140035ec0, which is MID-FUNCTION inside the
// resize helper FUN_140035d50 (its body is 0x140035d50-0x140035ec8). The actual
// per-element copy the resize loop CALLs is FUN_140007960 (verified: the only
// CALL 0x140007960 inside FUN_140035d50 is at 0x140035e17, the inner-move site).
// FUN_140007960(dst, src) frees dst's 8-byte-stride pointer array and deep-copies
// src's. Closure-safe (only MemAlloc/MemFree). Reimplemented below as
// ResidenceComponent_CopyInnerArray and registered for 0x140007960.
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
        ResolveAddr(Module::Exe, 0x140007960));
    s_fn_Resident_ResizeInner = reinterpret_cast<fn_Resident_ResizeInner_t>(
        ResolveAddr(Module::Exe, 0x1400032e0));
    s_fn_IBP_Serialize = reinterpret_cast<fn_IBP_Serialize_t>(
        ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z"));
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
// Reimpl of FUN_140007960 (resident per-slot inner pointer-array deep copy).
// dst/src each describe an array {ptr@+0, int count@+8, int capacity@+0xc}. Frees
// dst's old buffer, copies src's count, allocates count*8 (8-byte stride, align 8),
// and copies the pointers element-by-element. No-op when dst==src. Closure-safe:
// only MemAlloc/MemFree. Faithful 1:1 port of the Ghidra decompile.
// (Registered for 0x140007960 — see CALLER-BUG FIX note above.)
// ============================================================
void ResidenceComponent_CopyInnerArray(void* dstP, void* srcP) {
    int64_t* dst = reinterpret_cast<int64_t*>(dstP);
    int64_t* src = reinterpret_cast<int64_t*>(srcP);
    if (dst == src) return;

    fn_MemFreeStatic(reinterpret_cast<void*>(dst[0]));
    dst[1] = 0;
    dst[0] = 0;
    int count = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(dst) + 8);
    *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(dst) + 8) =
        *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(src) + 8);
    count = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(dst) + 8);
    if (count > 0) {
        *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(dst) + 0xc) = count;
        void* nb = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)((int64_t)count << 3), 8);
        dst[0] = (int64_t)nb;
    }
    int n = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(dst) + 8);
    for (int i = 0; i < n; i++) {
        int64_t* slot = reinterpret_cast<int64_t*>(dst[0] + (int64_t)i * 8);
        if (slot) {
            *slot = *reinterpret_cast<int64_t*>(src[0] + (int64_t)i * 8);
        }
    }
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
            // Require the VALUE, not just the pointer: with 0x1401c7158 registered the
            // slot exists from startup but only holds the CRC after FillTypeIdGlobals_citizenai.
            // IsDerivedFrom(val, 0) matches nothing, so a bare pointer check would clear
            // every resident ref in that window.
            if (val && s_TypeId_Resident && *s_TypeId_Resident && fn_IsDerivedFrom &&
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

    // Mesh flags 6 and 7 — clear two MeshInstance status/skin flags on the house.
    // STANDALONE: skipped. In standalone the house's ModelComponent mesh was force-set
    // to the completed-build variant (starttown.cpp StartTown_SpawnStructures); this
    // SetMeshFlag op manipulates the same MeshInstance and, with our partial/inert mesh
    // state, hides the building (it assumes the real game's fully-live mesh pipeline).
    // The registration below (push into townMgr+0x20) is the part standalone needs.

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

// ===== Standalone factory (Foundation D → Keystone D states) =====
// Inert-but-valid ResidenceComponent: size 0x60 + Object::Internal base + capability
// flags +0x20=0x82 (from ResidenceComponent_Ctor) + zeroed body.
//
// LIVE bring-up (BANISHED_LIVE_RESIDENCE, default ON via the default-on table):
//   * vtable index 4 (+0x20, 0x14012a120, Serialize/Startup) — resizes the resident
//     array at +0x50 from the description capacity (desc+0x20). Dispatched at spawn
//     by the ComponentEntity StartupComponents pass (FUN_1400c30a0), gated on entity
//     flag 0x80 + comp flag bit 7 (0x82 has it).
//   * vtable index 7 (+0x38, 0x14012a0e0, Ghidra "OnCreate") — helper_9ef0 (re-assign
//     resident workplaces / evict) + FUN_140002cc0 which REMOVES the owner entity from
//     the townMgr+0x20 residence list (same body as the state-3 demolish enter
//     helper_9920). The ADD to that list is the state-2 enter below.
//   * vtable index 16/17 (GetDefaultState=2 / CreateState=&node@+0x28) + the REAL
//     controller state table — the retail mechanism (ResidenceDescription_Setup
//     0x14014f8b0 → helper_a350 0x14012a350) that makes StateMachineComponent link
//     and tick the residence node. This is what drives firewood heating: the state-2
//     per-frame handler is the heat tick 0x140129970.
//
// Residence state graph (helper_a350, decompile-verified):
//   state 2 (built):    enter = helper_9de0 (append house to townMgr+0x20);
//                       trans38 (per-frame) = 0x140129970 heat tick; transitions -> {3}
//   state 3 (demolish): enter = helper_9920 (evict + remove from townMgr+0x20);
//                       transitions -> {6, 4, 5} (targets only, no handlers)
//
// Start-town houses are stamped SM pendingState=2 (starttown.cpp), so the first
// StateMachine::Tick runs the state-2 Enter (list registration) and then the heat
// tick every frame — the retail flow; the old StartTown_RegisterTownResidences
// manual append is removed to avoid double-registration.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
#include "Component/controller_state_table.h"

// residence_heattick.cpp — state-2 per-frame handler (0x140129970 port).
extern "C" char ResidenceComponent_HeatTick(void* comp, void* sm, void* dt);
// citizen_employ_cascade.cpp — helper_9de0 port (append house IBP to townMgr+0x20).
extern "C" void TownRegistry_RegisterResidence(int64_t houseEntityIBP);

// State-2 enter — faithful helper_9de0 (0x140129de0): append the owner-entity IBP
// VALUE (comp+0x10) to the townMgr+0x20 residence list. Retail returns 0 in AL, so
// the SM Enter walk continues past this node (Build's state-2 has no enter) — keep
// that return value.
static char ResidenceComponent_EnterBuilt(void* comp, void* /*sm*/) {
    int64_t ibp = *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(comp) + 0x10);
    TownRegistry_RegisterResidence(ibp);
    {
        static int n = 0;
        if (n < 12) { ++n;
            // Reports that the state-2 enter FIRED, not that the append succeeded — the
            // registration has five silent guards and this line used to claim success
            // regardless, which is how seven "registered" lines coexisted with
            // count(+0x28)==0. The outcome is logged inside TownRegistry_RegisterResidence
            // itself ([TownRegistry] lines); read those, not this one.
            fprintf(stderr, "[Residence] state-2 enter FIRED: house comp=%p entityIBP=%llx "
                    "-> TownRegistry_RegisterResidence (see [TownRegistry] for outcome)\n",
                    comp, (unsigned long long)ibp);
            fflush(stderr);
        }
    }
    return 0;
}

// State-3 enter — faithful helper_9920 (0x140129920): evict via helper_9ef0 + remove
// from townMgr+0x20 via FUN_140002cc0. That is exactly the ported OnCreate body.
static char ResidenceComponent_EnterDemolish(void* comp, void* /*sm*/) {
    // Discriminator (2026-07-31): this body ERASES the house from townMgr+0x20, and
    // something is running it during StartTown right after each state-2 append, which is
    // what keeps the residence count pinned at 0. Two call sites can reach it — this
    // state-3 enter and the slot-7 removal hook — so each says which one fired.
    static int n = 0;
    if (n < 12) { ++n;
        fprintf(stderr, "[Residence] ERASE via STATE-3 DEMOLISH enter: comp=%p\n", comp);
        fflush(stderr);
    }
    ResidenceComponent_OnCreate(reinterpret_cast<ResidenceComponent*>(comp));
    return 0;
}

namespace { struct ResidenceCtrlTag {}; }
using ResidenceCtrlDispatch = ::ControllerLive::Dispatcher<ResidenceCtrlTag>;

static void ResidenceComponent_PopulateStateTable() {
    using namespace ::ControllerLive;
    StateTable& t = ResidenceCtrlDispatch::table;
    if (t.count != 0) return;  // idempotent
    TableRegisterState(t, 2, reinterpret_cast<void*>(&ResidenceComponent_EnterBuilt),
                       nullptr, reinterpret_cast<void*>(&ResidenceComponent_HeatTick),
                       nullptr);
    TableAddTransition(t, 3, nullptr);
    TableRegisterState(t, 3, reinterpret_cast<void*>(&ResidenceComponent_EnterDemolish),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 6, nullptr);
    TableAddTransition(t, 4, nullptr);
    TableAddTransition(t, 5, nullptr);
}

// Per-type GetTypeId (vtable +0x10) — required by the spawn-pool component find-loops.
static uint32_t ResidenceComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ResidenceComponent");
    return cached;
}

// Per-type vtable (filled from the shared stub table on first use, then patched).
static void* ResidenceComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

// Slot adapters: the inert vtable stores raw void* slots; these forward to the typed
// reimpls with the correct signatures.
static void ResidenceComponent_SerializeVSlot(void* s) {   // vtable +0x20 (index 4)
    ResidenceComponent_Serialize(reinterpret_cast<ResidenceComponent*>(s));
}
static void ResidenceComponent_OnCreateVSlot(void* s) {    // vtable +0x38 (index 7)
    // See the note on ResidenceComponent_EnterDemolish — this is the other route into
    // the townMgr+0x20 erase, dispatched by FUN_1400c2ea0 at entity removal.
    static int n = 0;
    if (n < 12) { ++n;
        fprintf(stderr, "[Residence] ERASE via SLOT-7 removal hook: comp=%p\n", s);
        fflush(stderr);
    }
    ResidenceComponent_OnCreate(reinterpret_cast<ResidenceComponent*>(s));
}
static int ResidenceComponent_GetDefaultState(void*) { return 2; }  // 0x1400026c0
static void* ResidenceComponent_CreateState(void* self) {           // 0x140004080
    return reinterpret_cast<uint8_t*>(self) + 0x28;
}

// Graduate the live slots + install the real state-node dispatcher. Idempotent
// (called from every Factory/CreateInPlace, like CONTROLLER_LIVE_FACTORY_STATES).
static void ResidenceComponent_ShutdownVSlot(void* self, void* stream) {
    ResidenceComponent_Shutdown(reinterpret_cast<ResidenceComponent*>(self), stream);
}
static void ResidenceComponent_FinishLiveVtable(void* mem) {
    // Slot 3 (stream Serialize, retail 0x14012a170) wired UNCONDITIONALLY, same
    // rationale as StorageComponent: serialize round-trip is not an opt-in
    // feature. Ported since the residence wave but never written into the live
    // vtable — a loaded save's residence kept the base-Component serialize and
    // the resident array (+0x50/+0x58) stayed null, crashing the first post-load
    // HasSufficientResources (reads rows+8 == address 0x8). Save/load wave 2026-08-28.
    ResidenceComponent_LiveVtable[3] =
        reinterpret_cast<void*>(&ResidenceComponent_ShutdownVSlot);
    ResidenceComponent_LiveVtable[4] =
        reinterpret_cast<void*>(&ResidenceComponent_SerializeVSlot);
    ResidenceComponent_LiveVtable[7] =
        reinterpret_cast<void*>(&ResidenceComponent_OnCreateVSlot);
    ResidenceComponent_LiveVtable[16] =
        reinterpret_cast<void*>(&ResidenceComponent_GetDefaultState);
    ResidenceComponent_LiveVtable[17] =
        reinterpret_cast<void*>(&ResidenceComponent_CreateState);
    ResidenceComponent_PopulateStateTable();
    if (mem) {
        // Embedded node at +0x28: dispatcher vtable + the node+0x18 back-pointer
        // (= comp+0x40 selfPtr, set by the real ctor) the tail-call handlers need.
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(mem) + 0x28) =
            reinterpret_cast<void*>(ResidenceCtrlDispatch::Vtable());
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(mem) + 0x28 + 0x18) = mem;
    }
}

extern "C" void* ResidenceComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x60, 0x82,
        ResidenceComponent_LiveVtable,
        reinterpret_cast<void*>(&ResidenceComponent_LiveGetTypeId));
    ResidenceComponent_FinishLiveVtable(p);
    return p;
}
static void* ResidenceComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x60, 0x82,
        ResidenceComponent_LiveVtable,
        reinterpret_cast<void*>(&ResidenceComponent_LiveGetTypeId));
    ResidenceComponent_FinishLiveVtable(mem);
    return mem;
}

REGISTER_TYPE(ResidenceComponent, "ControllerComponent",
              &ResidenceComponent_Factory, &ResidenceComponent_CreateInPlace);
