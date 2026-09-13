// code/Game/Component/consumeproducecomponent.cpp
// Clean reimplementation of ConsumeProduceComponent lifecycle methods from Application-x64.exe.
//
// ConsumeProduceComponent extends ResourceProduceComponent (0x70 bytes).
// Two non-trivial vtable overrides:
//   Shutdown  (0x140118e00) — parent Shutdown + versioned serialization of consume fields
//   Serialize (0x140118ef0) — init consume data from building template
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/consumeproducecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// ResourceProduceComponent_Shutdown (parent) at 0x14012b4a0
typedef void (*fn_ParentShutdown_t)(void* self, void* stream);
static fn_ParentShutdown_t s_fn_ParentShutdown = nullptr;

// IO::Serializer ctor/dtor via dispatch slots
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(int*) — dispatch slot at 0x140152ac8
typedef void (*fn_Serialize_Int_t)(void* serializer, int* value, const char* name);
static fn_Serialize_Int_t s_fn_Serialize_Int = nullptr;

// IO::Serializer::GetVersion — dispatch slot at 0x140152a48
typedef int (*fn_Serializer_GetVersion_t)(void* self);
static fn_Serializer_GetVersion_t s_fn_Serializer_GetVersion = nullptr;

// FUN_140023a00 — resize float array (takes address of float* and new count)
typedef void (*fn_ResizeFloatArray_t)(float** arrayPtr, int newSize);
static fn_ResizeFloatArray_t s_fn_ResizeFloatArray = nullptr;

// IO::Serializer::Serialize(float array) — dispatch slot at 0x140151ea8
typedef void (*fn_Serialize_FloatArray_t)(void* self, int count, float* array, const char* name);
static fn_Serialize_FloatArray_t s_fn_Serialize_FloatArray = nullptr;

// fn_InternalBasePtr_CheckPtr — dispatch slot at entity owner
typedef void (*fn_CheckPtr_t)(void* self);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;

// ConsumeProduceComponent_sub_3a00 — resize consume rate array at +0x58
// Takes (void* arrayFieldPtr, int newCount)
typedef void (*fn_ResizeConsumeArray_t)(void* arrayFieldPtr, int newCount);
static fn_ResizeConsumeArray_t s_fn_ResizeConsumeArray = nullptr;

// DWORD_1401c7388 — ConsumeProduceDescription type ID
static uint32_t* s_TypeId_ConsumeProduceDescription = nullptr;

// fn_IsDerivedFrom — dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_ParentShutdown = reinterpret_cast<fn_ParentShutdown_t>(
        ResolveAddr(Module::Exe, 0x14012b4a0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serialize_Int = *reinterpret_cast<fn_Serialize_Int_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a48);
    if (slot) s_fn_Serializer_GetVersion = *reinterpret_cast<fn_Serializer_GetVersion_t*>(slot);

    s_fn_ResizeFloatArray = reinterpret_cast<fn_ResizeFloatArray_t>(
        ResolveAddr(Module::Exe, 0x140023a00));

    slot = ResolveAddr(Module::Exe, 0x140151ea8);
    if (slot) s_fn_Serialize_FloatArray = *reinterpret_cast<fn_Serialize_FloatArray_t*>(slot);

    // CheckPtr dispatch slot at 0x140152ab0
    slot = ResolveAddr(Module::Exe, 0x140152ab0);
    if (slot) s_fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(slot);

    s_fn_ResizeConsumeArray = reinterpret_cast<fn_ResizeConsumeArray_t>(
        ResolveAddr(Module::Exe, 0x140023a00));

    // ConsumeProduceDescription type ID at 0x1401c7388
    s_TypeId_ConsumeProduceDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7388));

    // IsDerivedFrom via dispatch slot
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    s_initialized = true;
}

// ============================================================
// ConsumeProduceComponent::Shutdown (vtable slot [3], 0x140118e00)
// ============================================================
// Calls parent ResourceProduceComponent_Shutdown, then serializes:
//   - consumeResourceId (+0x50)
//   - consumeCount (+0x60) via local copy, with array resize on load
//   - float array at consumeRates (+0x58)
//   - consumeState (+0x68)
//   - Version-0 migration: if consumeResourceId < 0 on load, reset to 0

void ConsumeProduceComponent_Shutdown(ConsumeProduceComponent* self, void* stream) {
    EnsureHelpers();

    // Call parent ResourceProduceComponent::Shutdown
    if (s_fn_ParentShutdown) {
        s_fn_ParentShutdown(self, stream);
    }

    if (!s_fn_Serializer_Ctor || !s_fn_Serializer_Dtor) return;

    // Create versioned serializer (version 1)
    uint8_t serBuf[24];
    int64_t* pSerData = (int64_t*)serBuf;
    s_fn_Serializer_Ctor(serBuf, stream, 1);

    // Serialize consumeResourceId at +0x50
    if (s_fn_Serialize_Int) {
        s_fn_Serialize_Int(serBuf, &self->consumeResourceId, nullptr);
    }

    // Copy consume count to local (original uses local_res8, NOT direct field access).
    // On load, the resize function reads +0x60 internally as old count to decide
    // whether reallocation is needed. Writing directly to +0x60 before resize would
    // make old == new, causing resize to skip reallocation.
    int localCount = self->consumeCount;
    if (s_fn_Serialize_Int) {
        s_fn_Serialize_Int(serBuf, &localCount, nullptr);
    }

    // On load (direction == 0): resize array if needed
    if (*(int*)(pSerData[0] + 8) == 0) {
        if (s_fn_ResizeFloatArray) {
            s_fn_ResizeFloatArray(&self->consumeRates, localCount);
        }
    }

    // Serialize float array using local count and rates pointer
    if (s_fn_Serialize_FloatArray) {
        s_fn_Serialize_FloatArray(serBuf, localCount, self->consumeRates, nullptr);
    }

    // Serialize consumeState at +0x68
    if (s_fn_Serialize_Int) {
        s_fn_Serialize_Int(serBuf, &self->consumeState, nullptr);
    }

    // Version-0 migration: on load, if consumeResourceId < 0, reset to 0
    if (s_fn_Serializer_GetVersion) {
        int version = s_fn_Serializer_GetVersion(serBuf);
        if (version == 0 && self->consumeResourceId < 0) {
            // Check if we're loading (stream direction == 0)
            if (*(int*)((uintptr_t)stream + 8) == 0) {
                self->consumeResourceId = 0;
            }
        }
    }

    s_fn_Serializer_Dtor(serBuf);
}

// ============================================================
// ConsumeProduceComponent::Serialize (vtable slot [4], 0x140118ef0)
// ============================================================
// Looks up entity's component list via owner (+0x10), iterates components
// to find one matching ConsumeProduceDescription type ID (0x1401c7388),
// then copies consume slot info (count at +0x30, resourceId at +0x4c),
// and initializes the consumeRates float array to 1.0f per slot.
//
// Original decompilation:
//   CheckPtr(self+0x10)
//   entity = *(self+0x10)
//   compList = entity->+0x28 (component list: +0x28=array, +0x30=count)
//   for i in range(compList.count):
//     comp = compList[i]
//     typeId = comp->vtable[+0x10]()  // GetTypeId virtual
//     if typeId == DWORD_1401c7388:
//       desc = compList[i]
//       if IsDerivedFrom(desc, DWORD_1401c7388): desc is valid
//       break
//   ResizeConsumeArray(self+0x58, *(desc+0x30))   // count from description
//   self->consumeResourceId = *(desc+0x4c)        // resource ID from description
//   for i in range(self->consumeCount):
//     consumeRates[i] = 1.0f

void ConsumeProduceComponent_Serialize(ConsumeProduceComponent* self) {
    EnsureHelpers();

    if (!s_fn_CheckPtr || !s_fn_IsDerivedFrom_dispatch || !s_TypeId_ConsumeProduceDescription)
        return;

    // CheckPtr on the owner InternalBasePtr at +0x10
    s_fn_CheckPtr((void*)((uintptr_t)self + 0x10));

    void* entity = self->owner;
    if (!entity) return;

    // Walk the entity DESCRIPTION's child list, not the component array.
    //
    // ComponentEntity keeps its description at +0x28 and its COMPONENT array at +0x30
    // (count +0x38); a description keeps its CHILD list at +0x28 with the count at
    // +0x30. What is being searched for here is ConsumeProduce*Description*, so retail
    // costs one extra deref: 0x140118ef0 does `RBP = *(entity+0x28)` and only then
    // reads [RBP+0x30] as the count and [RBP+0x28] as the array. Its slot-18 sibling
    // (0x140118d00) does exactly the same.
    //
    // This read the two fields straight off the entity, so `compCount` was the low half
    // of the component-array POINTER (a huge positive number) and `compArray` was the
    // description object -- the very first iteration would have called GetTypeId on the
    // description's own vtable pointer. It never faulted only because slot 4 was wired
    // nowhere and this whole body was dead. See the two-lists note in the wave memo.
    uintptr_t entityAddr = (uintptr_t)entity;
    uintptr_t descRoot = *(uintptr_t*)(entityAddr + 0x28);
    if (!descRoot) return;
    void** compArray = *(void***)(descRoot + 0x28);
    int compCount = *(int*)(descRoot + 0x30);

    // Find ConsumeProduceDescription in component list
    void* desc = nullptr;
    uint32_t targetTypeId = *s_TypeId_ConsumeProduceDescription;

    for (int i = 0; i < compCount; i++) {
        void* comp = compArray[i];
        if (!comp) continue;

        // Call GetTypeId virtual at vtable+0x10
        uintptr_t vtable = *(uintptr_t*)comp;
        typedef uint32_t (*fn_GetTypeId_t)(void*);
        fn_GetTypeId_t getTypeId = *(fn_GetTypeId_t*)(vtable + 0x10);
        uint32_t typeId = getTypeId(comp);

        if (typeId == targetTypeId) {
            // Verify with IsDerivedFrom
            if (s_fn_IsDerivedFrom_dispatch(comp, targetTypeId)) {
                desc = comp;
            }
            break;
        }
    }

    if (!desc) return;

    // Resize consume array from description count at +0x30
    int descCount = *(int*)((uintptr_t)desc + 0x30);
    if (s_fn_ResizeConsumeArray) {
        s_fn_ResizeConsumeArray((void*)((uintptr_t)self + 0x58), descCount);
    }

    // Copy resource ID from description at +0x4c
    self->consumeResourceId = *(int*)((uintptr_t)desc + 0x4c);

    // Initialize all consume rates to 1.0f
    float* rates = self->consumeRates;
    if (rates && self->consumeCount > 0) {
        for (int i = 0; i < self->consumeCount; i++) {
            rates[i] = 1.0f;
        }
    }
}

// ============================================================
// Reimpl of FUN_140023a00 (ConsumeProduceComponent_sub_3a00:
//   grow/shrink a 4-byte-element (int/float) array; copies only the overlap,
//   new tail elements are left for the caller to initialise)
// ============================================================
void ConsumeProduceComponent_ResizeArray(void* arrayFieldPtr, int newCount) {
    int64_t* field = (int64_t*)arrayFieldPtr;   // [0]=ptr, [1] low=count, high=capacity
    int oldCount = (int)field[1];
    if (newCount == oldCount) return;

    int32_t* oldBuf = (int32_t*)field[0];
    *(int*)((uintptr_t)field + 8) = newCount;
    if (*(int*)((uintptr_t)field + 0xc) < newCount) {
        *(int*)((uintptr_t)field + 0xc) = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            *(int*)((uintptr_t)field + 0xc) = newCount * 3;
        }
        void* nb = fn_MemAlloc(g_pMemAllocSingleton,
                               (uint64_t)((int64_t)*(int*)((uintptr_t)field + 0xc) << 2), 4);
        field[0] = (int64_t)nb;
    }

    int start = (oldBuf == (int32_t*)field[0]) ? oldCount : 0;
    int32_t* dst = (int32_t*)field[0];
    for (int i = start; i < newCount; i++) {
        if (i < oldCount && (dst + i) != nullptr) {
            dst[i] = oldBuf[i];
        }
    }

    if (oldBuf != (int32_t*)field[0]) {
        if (oldBuf) fn_MemFreeStatic(oldBuf);
    }
}

// ============================================================
// Reimpl of FUN_14012b4a0 (ResourceProduceComponent_Controller_Slot13:
//   base Shutdown/serialize — Component base + empty v0 block + bool at +0x48)
// ============================================================
void ResourceProduceComponent_BaseShutdown(void* self, void* stream) {
    EnsureHelpers();
    // Component base serialize
    typedef void (*fn_CompSer_t)(void*, void*);
    static fn_CompSer_t s_compSer = nullptr;
    if (!s_compSer)
        s_compSer = reinterpret_cast<fn_CompSer_t>(ResolveAddr(Module::Exe, 0x1401189b0));
    if (s_compSer) s_compSer(self, stream);

    if (!s_fn_Serializer_Ctor || !s_fn_Serializer_Dtor) return;

    // Empty version-0 block
    uint8_t ser0[24];
    s_fn_Serializer_Ctor(ser0, stream, 0);
    s_fn_Serializer_Dtor(ser0);

    // Version-0 block with a single bool at +0x48
    uint8_t ser1[24];
    s_fn_Serializer_Ctor(ser1, stream, 0);
    // bool serialize: reuse the int-serialize dispatch is wrong; use the serializer
    // vtable bool path via the dedicated dispatch slot 0x140152a58.
    {
        typedef void (*fn_SerBool_t)(void* ser, bool* field, const char* name);
        static fn_SerBool_t s_serBool = nullptr;
        if (!s_serBool) {
            uintptr_t slot = ResolveAddr(Module::Exe, 0x140152a58);
            if (slot) s_serBool = *reinterpret_cast<fn_SerBool_t*>(slot);
        }
        if (s_serBool) s_serBool(ser1, (bool*)((uintptr_t)self + 0x48), nullptr);
    }
    s_fn_Serializer_Dtor(ser1);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void ConsumeProduceComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[ConsumeProduceComponent] Funcs resolved: ParentShutdown=%p "
           "Ctor=%p Dtor=%p SerInt=%p GetVer=%p ResizeFloat=%p SerFloat=%p "
           "CheckPtr=%p ResizeConsume=%p TypeId=%p IsDerived=%p\n",
           (void*)s_fn_ParentShutdown, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_Serializer_Dtor, (void*)s_fn_Serialize_Int,
           (void*)s_fn_Serializer_GetVersion, (void*)s_fn_ResizeFloatArray,
           (void*)s_fn_Serialize_FloatArray, (void*)s_fn_CheckPtr,
           (void*)s_fn_ResizeConsumeArray,
           (void*)s_TypeId_ConsumeProduceDescription,
           (void*)s_fn_IsDerivedFrom_dispatch);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// Hand-expanded from COMPONENT_INERT_FACTORY(ConsumeProduceComponent, 0x70, 0x80) so
// vtable slots 13/14 — the panel command pair — can carry real bodies.
//
// Why this was dead: the "work" checkbox on a building panel is a ControlUI (element id
// 50000) whose click dispatches subComp->vtable[14] and whose per-frame enable test reads
// subComp->vtable[13]. Both slots held the inert stub, so the click was a literal no-op
// AND slot 13's 0 return greyed the checkbox every frame via ControlUI_Slot7. That is the
// whole "the work checkbox does nothing on the non-food buildings" symptom.
//
// Byte-verified 2026-08-15 by reading vtable_ConsumeProduceComponent (0x140162a80):
//   slot 13 = 0x14012b520   slot 14 = 0x14012b5a0
// Those are EXACTLY the entries on vtable_ResourceProduceComponent (0x1401643f0), i.e.
// ConsumeProduce declares no override and inherits the base pair verbatim. Both bodies
// have been ported for some time as ResourceProduceComponent_CommandMask / _Command
// (resourceproducecomponent_states.cpp) — this type simply never pointed at them.
//
// ⚠ Ghidra names those two bodies "HerbalistComponent_Slot13/14". That is a subclass
// label on a BASE-vtable body; do not read it as "these belong to the herbalist".
//
// Opt-in until measured (BANISHED_LIVE_CONSUMEPRODUCE_CMD). A quiet run cannot tell a
// working checkbox from a stubbed one, so this graduates only on a positive observation.
// ⚠ MEASURED 2026-08-15: wiring 13/14 alone is NOT enough on this class. With only the
// command pair filled, the panel probe read `command value = 0x02` (checkbox drawn
// enabled, no longer greyed) but a click moved nothing — `mask 0x02 -> 0x02`. The base
// slot 13 derives its mask from the STATE MACHINE, not from a component flag:
//     cur  = smc+0x52 (pending) or smc+0x50 (current)
//     mask = (cur != 14) ? 2 : 0;  if (self+0x48 == 0) mask |= ((unsigned)(cur-13) < 2)
// and the base slot 14 posts its event to the component's own controller NODE. With the
// component inert, slots 16/17 (GetDefaultState/CreateState) were stubs, the embedded
// node at +0x28 was never linked into the entity's StateMachineComponent, and the post
// had nowhere to land. So the promotion below is required, not optional — the same one
// fishingcomponent.cpp already does.
//
// Macro arguments, byte-verified from vtable_ConsumeProduceComponent (0x140162a80):
//   Size 0x70 / Flags 0x80 — unchanged from the previous inert factory.
//   NodeOff 0x28  — slot 17 is the shared 0x140004080 `LEA RAX,[RCX+0x28]; RET`.
//   DefState 2    — slot 16 is the shared 0x1400026c0 `MOV EAX,2; RET`.
#include "Component/controller_live_base.h"
#include "Component/controller_state_table.h"

extern "C" void** RPCtrl_NodeVtable();      // resourceproducecomponent_states.cpp
extern "C" void   RPCtrl_PopulateTable();
extern "C" uint8_t ResourceProduceComponent_CommandMask(void* self);  // 0x14012b520
extern "C" void    ResourceProduceComponent_Command(void* self);      // 0x14012b5a0

// Slot 18 (+0x90) CreateWork -- retail 0x140118d00, ported in
// resourceproduce_creatework.cpp alongside its five siblings. Slot 18 is _purecall on
// the ResourceProduceComponent base vtable, so leaving it as the inert stub is not
// "inherits the base" -- it is "the building never mints a work". Measured on a staffed
// demo tavern: a four-minute soak logged ZERO "[ConsumeProduce]" lines, so the work
// class was never instantiated and the tavern brewed nothing, which in turn is what
// kept HappinessComponent's goods arm (desc+0x25) permanently inactive -- the tavern's
// storage held only its potato input (RawMaterial flags 0x22 against wantMask 0x20000).
extern "C" uintptr_t* ConsumeProduceComponent_CreateWork(void*, uintptr_t*);  // slot 18

// Slots 13/14 must be written from INSIDE InstallLive: MakeInertTyped -> FillTypedVtable
// rewrites slots 1..N with the inert stub on the first factory call, so a static-init
// write is silently erased. The macro calls PopulateFn from InstallLive AFTER
// FillTypedVtable has run, which is the one surviving place.
static void ConsumeProduceCtrl_PopulateAndWireCmd();

CONTROLLER_LIVE_FACTORY_STATES(ConsumeProduceComponent, 0x70, 0x80, 0x28, 2,
                               RPCtrl_NodeVtable, ConsumeProduceCtrl_PopulateAndWireCmd)

// void*-ABI adapters for the two lifecycle slots. Slot 3 takes the stream; slot 4
// takes only self (it reads the template, it does not serialize).
static void ConsumeProduceComponent_ShutdownVSlot(void* self, void* stream) {
    ConsumeProduceComponent_Shutdown(reinterpret_cast<ConsumeProduceComponent*>(self), stream);
}
static void ConsumeProduceComponent_StartupVSlot(void* self) {
    ConsumeProduceComponent_Serialize(reinterpret_cast<ConsumeProduceComponent*>(self));
}

static void ConsumeProduceCtrl_PopulateAndWireCmd() {
    ConsumeProduceComponent_LiveVtable[13] =
        reinterpret_cast<void*>(&ResourceProduceComponent_CommandMask);
    ConsumeProduceComponent_LiveVtable[14] =
        reinterpret_cast<void*>(&ResourceProduceComponent_Command);
    ConsumeProduceComponent_LiveVtable[18] =
        reinterpret_cast<void*>(&ConsumeProduceComponent_CreateWork);
    // Slots 3 (Shutdown/serialize, 0x140118e00) and 4 (Startup -- init the consume
    // config from the building template, 0x140118ef0). Both bodies have been ported in
    // this file for a long time and were registered and wired NOWHERE, so the inert stub
    // ran instead: consumeResourceId (+0x50) kept the ctor zero instead of the
    // description's +0x4c, and the consume-rate array at +0x58 was never sized. Most of
    // the component family already wires this pair (attendance, cropfield, decal,
    // destroy, disease...); this class simply never pointed at its own bodies.
    ConsumeProduceComponent_LiveVtable[3] =
        reinterpret_cast<void*>(&ConsumeProduceComponent_ShutdownVSlot);
    ConsumeProduceComponent_LiveVtable[4] =
        reinterpret_cast<void*>(&ConsumeProduceComponent_StartupVSlot);
    RPCtrl_PopulateTable();
}
REGISTER_TYPE(ConsumeProduceComponent, "ResourceProduceComponent",
              &ConsumeProduceComponent_Factory, &ConsumeProduceComponent_CreateInPlace);
