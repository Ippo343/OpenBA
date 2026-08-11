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
static uint32_t* s_pTypeId_CropFieldDescription = nullptr;  // DAT_1401c7548

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
    s_pTypeId_CropFieldDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7548));
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

// Promoted off COMPONENT_INERT_FACTORY to a live ControllerComponent, so the state
// machine in cropfieldcomponent_states.cpp actually runs.
//
// Macro arguments, byte-verified against retail rather than inferred:
//   Size 0x68 / Flags 0x80 — CropFieldComponent_Ctor (0x14011afd0) is allocated at 0x68
//     by CropFieldComponent_Factory (0x140032ed0) and ORs 0x80 into +0x20.
//   NodeOff 0x28 — the ctor writes vtable_CropFieldComponent_Controller (0x140162e70)
//     to +0x28 and self to +0x40 (= NodeOff+0x18); primary vtable slot 17 (CreateState)
//     is the shared 0x140004080 = `LEA RAX,[RCX+0x28]; RET`.
//   DefState 2 — primary slot 16 (GetDefaultState) is the shared 0x1400026c0 =
//     `MOV EAX,2; RET`.
//
// The ctor also seeds +0x48 = 0x0100 (two bytes: +0x48 = 0, +0x49 = 1) and zeroes
// +0x50/+0x58/+0x60. MakeInertTyped zeroes the body, so +0x49 currently starts 0 rather
// than 1; that difference is only observable once the state handlers that read it are
// ported, and is noted here so it is not rediscovered as a mystery.
//
// Opt-in: the state table registers null handlers on this first pass, so the machine
// runs its real structure and performs no work yet. Nothing to graduate until the
// handler bodies land.
#include "Component/controller_live_base.h"
#include "Component/controller_state_table.h"

extern "C" void** CropFieldCtrl_NodeVtable();     // cropfieldcomponent_states.cpp
extern "C" void   CropFieldCtrl_PopulateTable();

// Slots 13/14/15 are the panel command path (CanCommand / Command / OnMessage) at retail
// 0x14011ada0 / 0x14011acc0 / 0x14011ab50, and 18 is 0x140033040. They are NOT wired yet
// — their bodies are unported. When they are, write them from INSIDE this hook, never at
// static init: FillTypedVtable rewrites slots 1..N on the first factory call and would
// silently erase anything written earlier.

// ── slot 15 — OnMessage — retail 0x14011ab50 ──────────────────────────────────────
// OnMessage(self, msgId, sender, slotIndex). Two messages, both addressed at ONE slot
// of the field's grid (self+0x58, stride 0x20, indexed by the 4th argument):
//
//   msg 0 — PLANT BOUND. The PlantArea work has just created the crop entity on this
//           slot's cell, so bind it: state (+0x04) = 1 "growing", stash the entity at
//           +0x08 and its NaturalResource component at +0x10, then copy the slot's
//           plant-mark byte (+0x18) into that component's +0x2d. This is what moves a
//           slot from "empty" to "growing", which is in turn what the state-9
//           per-frame handler counts to decide the field is planted and advance to
//           state 10. Without it a field plants and never progresses.
//
//   msg 1 — GROW TICK. Find the slot entity's growth component, then advance it by
//           Time(growthComp+0x28 * 300.0f * CropFieldDescription+0x24) through that
//           component's own vtable slot 9 (+0x48). 300.0f is 0x140172234.
//
// Retail's profile build carries two assertion strings for this function — "message
// send to planter harvester is wrong" and "map componment isn't initalized correctly!"
// [sic] — i.e. it treats a bad slot index or an uninitialised map as programmer error
// and derefs regardless. Kept unguarded to match, except where our standalone can
// legitimately have no entity on the cell (msg 1 already tests that in retail too).
extern "C" void CropFieldComponent_OnMessageVSlot(void* selfV, int msgId,
                                                  void* /*sender*/, unsigned int slotIdx) {
    EnsureHelpers();
    EnsureCropExtras();
    const uintptr_t self = reinterpret_cast<uintptr_t>(selfV);
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;

    const uintptr_t map = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf8);
    if (!map) return;
    const uintptr_t grid = *reinterpret_cast<uintptr_t*>(map + 0x80);
    const uintptr_t slots = *reinterpret_cast<uintptr_t*>(self + 0x58);
    if (!grid || !slots) return;

    uint8_t* slot = reinterpret_cast<uint8_t*>(slots + (uintptr_t)slotIdx * 0x20);
    const int cell = *reinterpret_cast<int32_t*>(slot + 0x00);
    const uintptr_t entity = *reinterpret_cast<uintptr_t*>(grid + 8 + (uintptr_t)cell * 0x30);

    if (msgId == 0) {
        uintptr_t nrIbp = 0;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)&entity);
        if (s_fn_FindComp_BS_local) s_fn_FindComp_BS_local((int64_t)entity, &nrIbp);
        *reinterpret_cast<int32_t*>(slot + 0x04)   = 1;          // growing
        *reinterpret_cast<uintptr_t*>(slot + 0x08) = entity;
        *reinterpret_cast<uintptr_t*>(slot + 0x10) = nrIbp;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot + 0x10);
        if (nrIbp)
            *reinterpret_cast<char*>(nrIbp + 0x2d) = static_cast<char>(slot[0x18]);
        return;
    }

    if (msgId == 1) {
        if (!entity) return;
        static void (*s_findGrowth)(int64_t, void*) = nullptr;
        static const float* s_f300 = nullptr;
        static bool s_r = false;
        if (!s_r) {
            s_findGrowth = reinterpret_cast<void(*)(int64_t, void*)>(
                ResolveAddr(Module::Exe, 0x14000c460));
            s_f300 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172234));
            s_r = true;
        }
        if (!s_findGrowth || !s_f300 || !fn_Time_Ctor_Float) return;
        uintptr_t growth = 0;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)&entity);
        s_findGrowth((int64_t)entity, &growth);
        if (!growth) return;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&growth);

        void* desc = *reinterpret_cast<void**>(self + 0x18);
        const uint32_t tidCropDesc =
            s_pTypeId_CropFieldDescription ? *s_pTypeId_CropFieldDescription : 0;
        if (!(desc && fn_IsDerivedFrom && tidCropDesc && fn_IsDerivedFrom(desc, tidCropDesc)))
            return;

        const float rate = *reinterpret_cast<float*>(
            *reinterpret_cast<uintptr_t*>(growth + 0x38) + 0x28);
        const float mult = *reinterpret_cast<float*>(
            reinterpret_cast<uintptr_t>(desc) + 0x24);
        int64_t t = 0;
        fn_Time_Ctor_Float(&t, rate * (*s_f300) * mult);

        void** gvt = *reinterpret_cast<void***>(growth);
        if (gvt && gvt[9])
            reinterpret_cast<void (*)(void*, void*)>(gvt[9])(
                reinterpret_cast<void*>(growth), &t);
    }
}

// ── slot 13 — CanCommand — retail 0x14011ada0 ─────────────────────────────────────
// Decides which of the crop-field panel's three buttons are enabled. Unlike the
// ResourceProduce family's slot 13 (which ignores its argument and returns a mask),
// this one IS a per-command query:
//   cmd 0 (plant)   enabled when the SM sits in state 2  AND the field has slots
//   cmd 1 (harvest) enabled when the SM sits in state 10 AND the field has slots
//   cmd 2 (toggle)  returns a two-bit value: bit 1 = currently on (+0x48), bit 0 =
//                   selectable at all (has slots). Retail computes it as
//                   `-(+0x48 != 0) & 2 | hasSlots`, i.e. a MASK, not a boolean — the
//                   panel reads bit 1 to draw the toggle's state and bit 0 to enable
//                   it. Returning a plain bool here would collapse "on" and
//                   "available" into one and mis-draw the button.
// "Has slots" is self+0x50 != 0 — the slot array state 9 builds. An unplanted field
// therefore greys everything, which is retail behaviour, not a defect.
extern "C" uint8_t CropFieldComponent_CanCommandVSlot(void* selfV, int cmdId) {
    const uintptr_t self = reinterpret_cast<uintptr_t>(selfV);
    static void (*s_findSM)(uintptr_t, uintptr_t*) = nullptr;
    static bool s_r = false;
    if (!s_r) {
        s_findSM = reinterpret_cast<void(*)(uintptr_t, uintptr_t*)>(
            ResolveAddr(Module::Exe, 0x140003ea0));
        s_r = true;
    }
    if (!s_findSM) return 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x10));
    uintptr_t smc = 0;
    s_findSM(*reinterpret_cast<uintptr_t*>(self + 0x10), &smc);
    if (!smc) return 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&smc);

    int8_t cur = *reinterpret_cast<int8_t*>(smc + 0x52);
    if (cur < 0) cur = *reinterpret_cast<int8_t*>(smc + 0x50);
    const bool hasSlots = (*reinterpret_cast<uintptr_t*>(self + 0x50) != 0);

    switch (cmdId) {
        case 0:  return (cur == 2  && hasSlots) ? 1 : 0;
        case 1:  return (cur == 10 && hasSlots) ? 1 : 0;
        case 2:  return static_cast<uint8_t>(
                     ((*reinterpret_cast<char*>(self + 0x48) != 0) ? 2 : 0) |
                     (hasSlots ? 1 : 0));
        default: return 0;
    }
}

// ── slot 14 — Command — retail 0x14011acc0 ────────────────────────────────────────
//   cmd 0 -> post node event 3 (PLANT)    — the event state 2 registers
//   cmd 1 -> post node event 4 (HARVEST)  — the event state 10 registers
//   cmd 2 -> flip +0x48 and push it straight into the workplace accepting flag
// The two event ids line up exactly with the state table in
// cropfieldcomponent_states.cpp, so the panel drives the same paths the seasonal
// triggers do.
extern "C" void CropFieldComponent_CommandVSlot(void* selfV, int cmdId) {
    const uintptr_t self = reinterpret_cast<uintptr_t>(selfV);
    static void (*s_findSM)(uintptr_t, uintptr_t*)  = nullptr;
    static void (*s_findWpc)(uintptr_t, uintptr_t*) = nullptr;
    static void (*s_setAccepting)(uintptr_t, char)  = nullptr;
    static bool s_r = false;
    if (!s_r) {
        s_findSM = reinterpret_cast<void(*)(uintptr_t, uintptr_t*)>(
            ResolveAddr(Module::Exe, 0x140003ea0));
        s_findWpc = reinterpret_cast<void(*)(uintptr_t, uintptr_t*)>(
            ResolveAddr(Module::Exe, 0x140002e10));
        s_setAccepting = reinterpret_cast<void(*)(uintptr_t, char)>(
            ResolveAddr(Module::Exe, 0x14012fc00));
        s_r = true;
    }
    if (cmdId != 0 && cmdId != 1 && cmdId != 2) return;

    if (cmdId == 2) {
        char* master = reinterpret_cast<char*>(self + 0x48);
        *master = (*master == 0) ? 1 : 0;
        if (!s_findWpc || !s_setAccepting) return;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x10));
        uintptr_t wpc = 0;
        s_findWpc(*reinterpret_cast<uintptr_t*>(self + 0x10), &wpc);
        if (!wpc) return;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&wpc);
        s_setAccepting(wpc, *master);
        return;
    }

    if (!s_findSM) return;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x10));
    uintptr_t smc = 0;
    s_findSM(*reinterpret_cast<uintptr_t*>(self + 0x10), &smc);
    if (!smc) return;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&smc);

    const uintptr_t sm = smc + 0x48;
    void* node = *reinterpret_cast<void**>(sm);
    if (!node) return;
    void** nvt = *reinterpret_cast<void***>(node);
    if (!nvt || !nvt[2]) return;
    const int curIdx  = *reinterpret_cast<int8_t*>(smc + 0x51);
    const int eventId = (cmdId == 0) ? 3 : 4;
    reinterpret_cast<char (*)(void*, void*, int, int)>(nvt[2])(
        node, reinterpret_cast<void*>(sm), curIdx, eventId);
}

// ── Family-A tail slots 4 / 19 / 20 / 21 ──────────────────────────────────────────
// These four sit past the shared Component block and were the last cheap gaps the
// slot-coverage harness reported for this class. All four are tiny in retail:
//   slot 4  0x14011AE60  `*(uint64*)(self+0x50) = 0` — one store, nothing else.
//   slot 19 0x140032F20  `MOV RAX,[rip+global]; RET` — returns the class's BaseTypeInfo.
//   slot 20 0x140032F10  `MOV EAX,[rip+global]; RET` — returns the class's type id.
//   slot 21 0x14011B2F0  `MOV EAX,3; RET`.
//
// ⚠ 19/20 read globals ADJACENT to (not identical with) the ones slots 1/2 use — they
// are the controller-interface subobject's type info/id. We have no separate storage
// for that subobject, so these return the class's own type info/id. That is a
// stand-in, not byte-exact; it is strictly closer than the inert stub (which returned
// null/garbage) and is flagged here so it is not mistaken for a finished port.
static void CropFieldComponent_Slot4Clear(void* self) {
    *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(self) + 0x50) = 0;
}
static void* CropFieldComponent_Slot19GetTypeInfo(void*) {
    static void* c = nullptr;
    if (!c) c = ::Object::GetTypeInfoByName("CropFieldComponent");
    return c;
}
static uint32_t CropFieldComponent_Slot20GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("CropFieldComponent");
    return c;
}
static int CropFieldComponent_Slot21Const(void*) { return 3; }

extern "C" void* ControllerLive_ScalarDeletingDtor(void*, unsigned int);  // slot 18
static void CropFieldCtrl_Populate();

CONTROLLER_LIVE_FACTORY_STATES(CropFieldComponent, 0x68, 0x80, 0x28, 2,
                               "BANISHED_LIVE_CROPFIELD",
                               CropFieldCtrl_NodeVtable, CropFieldCtrl_Populate)

static void CropFieldCtrl_Populate() {
    CropFieldComponent_LiveVtable[15] = reinterpret_cast<void*>(&CropFieldComponent_OnMessageVSlot);
    CropFieldComponent_LiveVtable[13] = reinterpret_cast<void*>(&CropFieldComponent_CanCommandVSlot);
    CropFieldComponent_LiveVtable[14] = reinterpret_cast<void*>(&CropFieldComponent_CommandVSlot);
    // Slot 3 = Shutdown/Serialize. The body has been ported in this file since
    // the original wave and nothing ever wrote it into the live vtable, so the
    // coverage harness measured the slot as still holding the inert stub. Third
    // instance of "ported but unwired" in this component family — worth wiring
    // from InstallLive, the one place FillTypedVtable cannot erase.
    CropFieldComponent_LiveVtable[3]  = reinterpret_cast<void*>(
        static_cast<void(*)(CropFieldComponent*, void*)>(&CropFieldComponent_Shutdown));
    CropFieldComponent_LiveVtable[18] = reinterpret_cast<void*>(&ControllerLive_ScalarDeletingDtor);
    CropFieldComponent_LiveVtable[4]  = reinterpret_cast<void*>(&CropFieldComponent_Slot4Clear);
    CropFieldComponent_LiveVtable[19] = reinterpret_cast<void*>(&CropFieldComponent_Slot19GetTypeInfo);
    CropFieldComponent_LiveVtable[20] = reinterpret_cast<void*>(&CropFieldComponent_Slot20GetTypeId);
    CropFieldComponent_LiveVtable[21] = reinterpret_cast<void*>(&CropFieldComponent_Slot21Const);
    CropFieldCtrl_PopulateTable();
}
REGISTER_TYPE(CropFieldComponent, "ControllerComponent",
              &CropFieldComponent_Factory, &CropFieldComponent_CreateInPlace);
