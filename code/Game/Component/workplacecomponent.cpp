// code/Game/Component/workplacecomponent.cpp
// Clean reimplementation of WorkPlaceComponent lifecycle methods from Application-x64.exe.
//
// WorkPlaceComponent (0x68 bytes) is a ControllerComponent subclass managing
// worker assignments to buildings.
//
// Vtable at 0x140164d30. Two non-trivial overrides:
//   Shutdown  (0x14012f120) — Component_Serialize + versioned worker array serialization
//   OnCreate  (0x14012f090) — BST lookup to register with profession list
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/workplacecomponent_funcs.h"
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

// IO::Serializer::Serialize(this, int*, name)
typedef void (*fn_Serializer_SerializeInt_t)(void* self, int* value, const char* name);
static fn_Serializer_SerializeInt_t s_fn_Serializer_SerializeInt = nullptr;

// IO::Serializer::Serialize(this, bool*, name)
typedef void (*fn_Serializer_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_Serializer_SerializeBool_t s_fn_Serializer_SerializeBool = nullptr;

// IO::Serializer::GetVersion(this) -> int
typedef int (*fn_Serializer_GetVersion_t)(void* self);
static fn_Serializer_GetVersion_t s_fn_Serializer_GetVersion = nullptr;

// fn_IsDerivedFrom — dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

// Object::InternalBasePtr::Serialize
typedef void (*fn_IBP_Serialize_t)(void* self, void* serializer);
static fn_IBP_Serialize_t s_fn_IBP_Serialize = nullptr;

// FUN_1400032e0 — resize InternalBasePtr array
typedef void (*fn_ResizeIBPArray_t)(void* arrayPtr, int newSize);
static fn_ResizeIBPArray_t s_fn_ResizeIBPArray = nullptr;

// FUN_140018020 — serialize InternalBasePtr field (version >= 1 path)
typedef void (*fn_SerializeIBP_t)(void* fieldPtr, void* serializer);
static fn_SerializeIBP_t s_fn_SerializeIBP = nullptr;

// FUN_140002cc0 — REMOVE an entity reference from a dynamic IBP array (Ghidra-verified:
// scans base[0] for a value equal to *entityRef, shifts the tail down, decrements the
// count at base+8). It is an UNregister, not an add — the old "AddToList" name here had
// the sense backwards and made slot 7 below read like a registration.
typedef void (*fn_RemoveFromList_t)(void* listPtr, void* entityRef);
static fn_RemoveFromList_t s_fn_RemoveFromList = nullptr;

// WorkPlaceComponent_OnCreate_sub_ee60 — fallback init
typedef void (*fn_OnCreateFallback_t)(void* self);
static fn_OnCreateFallback_t s_fn_OnCreateFallback = nullptr;

// Type IDs
static uint32_t* s_TypeId_WorkerType = nullptr;  // 0x1401c7158 — for IsDerivedFrom validation
static uint32_t* s_TypeId_WorkPlaceDesc = nullptr; // 0x1401c8118 — for version < 1 fallback

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
    if (slot) s_fn_Serializer_SerializeInt = *reinterpret_cast<fn_Serializer_SerializeInt_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a58);
    if (slot) s_fn_Serializer_SerializeBool = *reinterpret_cast<fn_Serializer_SerializeBool_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a48);
    if (slot) s_fn_Serializer_GetVersion = *reinterpret_cast<fn_Serializer_GetVersion_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    s_fn_IBP_Serialize = fn_InternalBasePtr_Serialize;

    s_fn_ResizeIBPArray = reinterpret_cast<fn_ResizeIBPArray_t>(
        ResolveAddr(Module::Exe, 0x1400032e0));

    s_fn_SerializeIBP = reinterpret_cast<fn_SerializeIBP_t>(
        ResolveAddr(Module::Exe, 0x140018020));

    s_fn_RemoveFromList = reinterpret_cast<fn_RemoveFromList_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));

    s_fn_OnCreateFallback = reinterpret_cast<fn_OnCreateFallback_t>(
        ResolveAddr(Module::Exe, 0x14012ee60));

    s_TypeId_WorkerType = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7158));
    s_TypeId_WorkPlaceDesc = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c8118));

    s_initialized = true;
}

// ============================================================
// Reimpl of FUN_14012ee60 (WorkPlaceComponent_OnCreate_sub_ee60:
//   drain every worker, detaching its current job host and reattaching it here)
// ============================================================
// Worker InternalBasePtr array at +0x58 (count +0x60). For each worker (index 0,
// repeatedly — the helpers below remove it, shrinking the count): find its current
// job host (DWORD_1401c80e0 component on worker+0x248), detach via FUN_14012f7e0,
// then if a replacement was produced, reattach via FUN_14012fa20.
typedef void (*fn_WPC_Detach_t)(int64_t comp, void* workerIbp);     // 0x14012f7e0
typedef void (*fn_WPC_Attach_t)(int64_t comp, void* workerIbp);     // 0x14012fa20
static fn_WPC_Detach_t s_fn_WPC_Detach = nullptr;
static fn_WPC_Attach_t s_fn_WPC_Attach = nullptr;
static uint32_t* s_pTId_JobHostComp = nullptr;  // DWORD_1401c80e0
static bool s_wpcEe60Resolved = false;
static void EnsureEe60Helpers() {
    if (s_wpcEe60Resolved) return;
    s_fn_WPC_Detach = reinterpret_cast<fn_WPC_Detach_t>(ResolveAddr(Module::Exe, 0x14012f7e0));
    s_fn_WPC_Attach = reinterpret_cast<fn_WPC_Attach_t>(ResolveAddr(Module::Exe, 0x14012fa20));
    s_pTId_JobHostComp = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c80e0));
    s_wpcEe60Resolved = true;
}

// Find the first component on `host` whose GetTypeId (vtable+0x10) == typeId.
static void* WPC_FindJobHostComponent(int64_t host, uint32_t* pTypeId) {
    if (!pTypeId) return nullptr;
    int count = *(int*)(host + 0x38);
    int64_t arr = *(int64_t*)(host + 0x30);
    for (int i = 0; i < count; i++) {
        int64_t* slot = (int64_t*)(arr + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* comp = (void*)*slot;
        typedef uint32_t (*GetTypeId_t)(void*);
        uint32_t tid = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
        if (tid == *pTypeId) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
            void* c = (void*)*slot;
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *pTypeId)) return c;
            return nullptr;
        }
    }
    return nullptr;
}

// ============================================================
// Reimpl bodies for the detach/attach helpers (0x14012f7e0 / 0x14012fa20).
// Their full exe FUN_ closure became registered in the gap wave, so they are now
// closure-safe to port. Sub-helpers used:
//   FUN_140002cc0 (worker-list remove)         -> s_fn_RemoveFromList (already resolved)
//   ModelComponent_helper_2b70 (0x1400c2b70)   -> mesh visibility/state kick
//   Entity_FindComponent_MeshInstance (0x140002f90)
//   FUN_14012c3f0 (mesh-instance flag setter)
//   FUN_140005780 (IBP accessor)               + FUN_14011e710 (bool getter)
//   FUN_140003110 (worker-list append)         + FUN_1400c3470 (entity flag notify)
typedef void  (*fn_WPC_RemoveFromList_t)(int64_t* listField, void* ibp);   // 0x140002cc0
typedef void  (*fn_WPC_AppendToList_t)(int64_t* listField, int newCount, void* ibp); // 0x140003110
typedef void  (*fn_WPC_ModelKick_t)(int64_t entity, int mode);             // 0x1400c2b70
typedef void* (*fn_WPC_FindMesh_t)(int64_t entity, void* outIbp);          // 0x140002f90
typedef void  (*fn_WPC_MeshFlag_t)(int64_t* mesh, int mode, char flag);    // 0x14012c3f0
typedef void* (*fn_WPC_Accessor5780_t)(int64_t entity, void* outIbp);      // 0x140005780
typedef char  (*fn_WPC_Getter_e710_t)(int64_t obj);                        // 0x14011e710
typedef void  (*fn_WPC_FlagNotify_t)(void* entity);                        // 0x1400c3470
static fn_WPC_RemoveFromList_t s_fn_WPC_RemoveFromList = nullptr;
static fn_WPC_AppendToList_t   s_fn_WPC_AppendToList = nullptr;
static fn_WPC_ModelKick_t      s_fn_WPC_ModelKick = nullptr;
static fn_WPC_FindMesh_t       s_fn_WPC_FindMesh = nullptr;
static fn_WPC_MeshFlag_t       s_fn_WPC_MeshFlag = nullptr;
static fn_WPC_Accessor5780_t   s_fn_WPC_Accessor5780 = nullptr;
static fn_WPC_Getter_e710_t    s_fn_WPC_Getter_e710 = nullptr;
static fn_WPC_FlagNotify_t     s_fn_WPC_FlagNotify = nullptr;
static bool s_wpcBodyResolved = false;
static void EnsureWpcBodyHelpers() {
    if (s_wpcBodyResolved) return;
    s_fn_WPC_RemoveFromList = reinterpret_cast<fn_WPC_RemoveFromList_t>(ResolveAddr(Module::Exe, 0x140002cc0));
    s_fn_WPC_AppendToList   = reinterpret_cast<fn_WPC_AppendToList_t>(ResolveAddr(Module::Exe, 0x140003110));
    s_fn_WPC_ModelKick      = reinterpret_cast<fn_WPC_ModelKick_t>(ResolveAddr(Module::Exe, 0x1400c2b70));
    s_fn_WPC_FindMesh       = reinterpret_cast<fn_WPC_FindMesh_t>(ResolveAddr(Module::Exe, 0x140002f90));
    s_fn_WPC_MeshFlag       = reinterpret_cast<fn_WPC_MeshFlag_t>(ResolveAddr(Module::Exe, 0x14012c3f0));
    s_fn_WPC_Accessor5780   = reinterpret_cast<fn_WPC_Accessor5780_t>(ResolveAddr(Module::Exe, 0x140005780));
    s_fn_WPC_Getter_e710    = reinterpret_cast<fn_WPC_Getter_e710_t>(ResolveAddr(Module::Exe, 0x14011e710));
    s_fn_WPC_FlagNotify     = reinterpret_cast<fn_WPC_FlagNotify_t>(ResolveAddr(Module::Exe, 0x1400c3470));
    s_wpcBodyResolved = true;
}

// Build-time roster-only mode (set by StartTown_AssignFoundingHousing). When true,
// Attach/Detach do ONLY the occupant-roster list mutation (+0x58/+0x60) and skip the
// gameplay-time Scene mesh-visual toggle (ModelKick + FUN_14012c3f0). The founding-housing
// shim runs during the StartTown world-BUILD, but the mesh toggle calls Graphics::Scene::
// RemoveObject/AddObject — a live-gameplay operation that DEADLOCKS when driven mid-build
// (the renderer scene isn't in a removable state and the inert StatusIconComponent's
// sceneObjId is stale). Vanilla houses founders per-TICK during gameplay (scene live), so
// suppressing the visual here is faithful to *when* the toggle is meant to run; the roster
// still tracks occupants. Default false → the per-tick gameplay AssignHousing path is
// unchanged (full faithful visual).
bool g_wpcRosterOnly = false;

// Reimpl of FUN_14012f7e0 (WorkplaceComponent worker DETACH): remove the worker IBP
// from the comp's worker list (+0x58), then if the comp is in its initial state
// (+0x60==0): kick the model (mode 3), set the mesh flags 2 and 3, and — when the
// worker's job slot matches the comp's description job (desc+0x20) and the entity is
// not already culled — hide the entity's mesh (clear AddObject / set update flag).
// Faithful 1:1 port; closure fully registered.
void WorkplaceComponent_Detach(int64_t comp, void* workerIbp) {
    EnsureHelpers();
    EnsureWpcBodyHelpers();

    if (s_fn_WPC_RemoveFromList) s_fn_WPC_RemoveFromList((int64_t*)(comp + 0x58), workerIbp);
    // Build-time roster-only mode (see g_wpcRosterOnly): stop after the roster removal,
    // skipping the gameplay-time Scene mesh-visual toggle that deadlocks mid-build.
    if (g_wpcRosterOnly) return;
    if (*(int*)(comp + 0x60) != 0) return;

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(comp + 0x10));
    if (s_fn_WPC_ModelKick) s_fn_WPC_ModelKick(*(int64_t*)(comp + 0x10), 3);

    // Partial-world guard (see WorkplaceComponent_Attach): skip the status-icon visual
    // toggles when the standalone entity lacks the renderable StatusIconComponent (c7e08).
    uint64_t meshIbp = 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(comp + 0x10));
    if (s_fn_WPC_FindMesh) s_fn_WPC_FindMesh(*(int64_t*)(comp + 0x10), &meshIbp);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&meshIbp);
    if (s_fn_WPC_MeshFlag && *(int64_t**)&meshIbp) s_fn_WPC_MeshFlag(*(int64_t**)&meshIbp, 2, '\0');

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(comp + 0x10));
    if (s_fn_WPC_FindMesh) s_fn_WPC_FindMesh(*(int64_t*)(comp + 0x10), &meshIbp);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&meshIbp);
    if (s_fn_WPC_MeshFlag && *(int64_t**)&meshIbp) s_fn_WPC_MeshFlag(*(int64_t**)&meshIbp, 3, '\0');

    if (*(char*)(comp + 0x50) == '\0') return;

    uint64_t accIbp = 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(comp + 0x10));
    if (s_fn_WPC_Accessor5780) s_fn_WPC_Accessor5780(*(int64_t*)(comp + 0x10), &accIbp);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&accIbp);
    // Partial-world guard: on an inert standalone workplace, Accessor5780 yields a null
    // accessor object; passing it to Getter_e710 dereferences null+0x10 (READ of 0x10),
    // which crashed when a citizen switched workplaces during the per-frame update (that
    // path doesn't hold g_wpcRosterOnly). A null accessor means "nothing renderable to
    // toggle", so skip the visual tail — same as g != 0 (entity already culled).
    if (accIbp == 0) return;
    char g = s_fn_WPC_Getter_e710 ? s_fn_WPC_Getter_e710(*(int64_t*)&accIbp) : 1;
    if (g != '\0') return;

    int64_t jobSlot = *(int64_t*)(comp + 0x48);
    if (jobSlot == 0) return;

    void* descObj = *(void**)(comp + 0x18);
    // Guard descObj before IsDerivedFrom (Runtime IsDerivedFrom has no null-check) and
    // desc before the +0x20 deref: an inert standalone workplace can carry a null/absent
    // description. A null desc means the job can't match -> skip the visual tail.
    void* desc = (descObj && fn_IsDerivedFrom && s_TypeId_WorkPlaceDesc &&
                  fn_IsDerivedFrom(descObj, *s_TypeId_WorkPlaceDesc)) ? descObj : nullptr;
    if (!desc || jobSlot != *(int64_t*)((uintptr_t)desc + 0x20)) return;

    uint64_t meshIbp2 = 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(comp + 0x10));
    if (s_fn_WPC_FindMesh) s_fn_WPC_FindMesh(*(int64_t*)(comp + 0x10), &meshIbp2);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&meshIbp2);
    int64_t* mesh = *(int64_t**)&meshIbp2;
    if (!mesh) return;   // partial-world guard: no StatusIconComponent renderable
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(mesh + 2);
    if ((*(uint32_t*)(mesh[2] + 0x40) >> 10 & 1) != 0) return;

    uint32_t* pFlagsE = (uint32_t*)(mesh + 0x1e);
    *pFlagsE |= 0x10;
    // Each branch reads mesh+0x20 FRESH right before the OR/AND (the original reads
    // *(uint*)(plVar2+4) at the assignment, NOT a value captured before the branch —
    // the add-branch's MeshInstance::Update (vtable +0x40) can mutate this flags word).
    uint32_t changed;
    if ((int)mesh[0x1e] == 0) {
        if ((int)*(uint32_t*)(mesh + 0x20) >= 0) {
            if (fn_SceneRemoveObject && g_pGameWorldSingleton)
                fn_SceneRemoveObject(*(void**)((uintptr_t)*g_pGameWorldSingleton + 0x60),
                                     *(uint32_t*)(mesh + 0x20));
            *(uint32_t*)(mesh + 0x20) = 0xffffffff;
        }
        uint32_t old4 = *(uint32_t*)(mesh + 4);
        uint32_t new4 = old4 & 0xfffffffb;
        *(uint32_t*)(mesh + 4) = new4;
        changed = new4 ^ old4;
    } else {
        if (g_pGameSettingsPtr && *g_pGameSettingsPtr &&
            ((uint8_t*)*g_pGameSettingsPtr)[0xc] != '\0' && (int)mesh[0x20] < 0) {
            if (fn_SceneAddObject && g_pGameWorldSingleton) {
                uint32_t id = fn_SceneAddObject(*(void**)((uintptr_t)*g_pGameWorldSingleton + 0x60),
                                                (void*)(mesh + 5));
                *(uint32_t*)(mesh + 0x20) = id;
            }
        }
        uint64_t t = 0;
        if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&t, 0.0f);
        // (**(code**)(*mesh + 0x40))(mesh, &time) — MeshInstance::Update vtable slot +0x40.
        typedef void (*VFnUpdate_t)(int64_t* self, void* time);
        VFnUpdate_t vfn = (VFnUpdate_t)(*(uintptr_t*)(*(uintptr_t*)mesh + 0x40));
        if (vfn) vfn(mesh, &t);
        uint32_t old4 = *(uint32_t*)(mesh + 4);   // FRESH after Update
        uint32_t new4 = old4 | 4;
        *(uint32_t*)(mesh + 4) = new4;
        changed = new4 ^ old4;
    }
    if ((changed & 0xffff) != 0 && mesh[2] != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(mesh + 2);
        if (s_fn_WPC_FlagNotify) s_fn_WPC_FlagNotify((void*)mesh[2]);
    }
}

// Reimpl of FUN_14012fa20 (WorkplaceComponent worker ATTACH): if the comp is in its
// initial state (+0x60==0) and the worker's job slot matches the comp's description
// job (desc+0x20), kick the model (mode 2); always set the mesh flag (mode 4) and
// append the worker IBP to the worker list (+0x58, new count = +0x60+1).
// Faithful 1:1 port; closure fully registered.
void WorkplaceComponent_Attach(int64_t comp, void* workerIbp) {
    EnsureHelpers();
    EnsureWpcBodyHelpers();

    if (!g_wpcRosterOnly) {
        if (*(int*)(comp + 0x60) == 0) {
            void* descObj = *(void**)(comp + 0x18);
            int64_t jobSlot = *(int64_t*)(comp + 0x48);
            // Guard descObj before IsDerivedFrom (no null-check) and desc before the +0x20
            // deref — an inert standalone workplace may have a null description.
            void* desc = (descObj && fn_IsDerivedFrom && s_TypeId_WorkPlaceDesc &&
                          fn_IsDerivedFrom(descObj, *s_TypeId_WorkPlaceDesc)) ? descObj : nullptr;
            if (desc && jobSlot == *(int64_t*)((uintptr_t)desc + 0x20)) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(comp + 0x10));
                if (s_fn_WPC_ModelKick) s_fn_WPC_ModelKick(*(int64_t*)(comp + 0x10), 2);
            }
        }

        uint64_t meshIbp = 0;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(comp + 0x10));
        if (s_fn_WPC_FindMesh) s_fn_WPC_FindMesh(*(int64_t*)(comp + 0x10), &meshIbp);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&meshIbp);
        // Faithful-equivalent partial-world guard: the original always toggles the building's
        // status-icon mesh (FUN_14012c3f0) because every real building carries the renderable
        // StatusIconComponent (c7e08). A standalone-spawned entity may lack it (inert factory /
        // template not fully materialised) → the finder returns a null component and
        // FUN_14012c3f0 faults reading mesh+0x10. Skip only the visual toggle when absent; the
        // occupant roster append below still runs, so the house tracks its occupants regardless.
        int64_t* mesh = *(int64_t**)&meshIbp;
        if (s_fn_WPC_MeshFlag && mesh) s_fn_WPC_MeshFlag(mesh, 4, '\0');
    }

    if (s_fn_WPC_AppendToList)
        s_fn_WPC_AppendToList((int64_t*)(comp + 0x58), *(int*)(comp + 0x60) + 1, workerIbp);
}

// ============================================================
// WorkPlaceComponent::SetAccepting — faithful port of Entity_impl_fc00 (0x14012fc00),
// accepting=true path only (the genuine build-complete -> accepting transition).
// ============================================================
// Retail fires this when a workplace comes online. For accepting=true it: stores the
// accepting flag (+0x50), clears the "closed"/"manually-disabled" status-icon bits (1,0),
// then — when the workplace is UNSTAFFED on its designated job (roster +0x60 == 0 &&
// jobSlot +0x48 == desc.job desc+0x20) — RAISES the "no workers" warning bit 4 (0x10) on
// the building's StatusIconComponent. This is the SOLE faithful raiser of that warning;
// once a worker is attached, WorkplaceComponent_Attach clears the bit again
// (MeshFlag(mesh,4,0)). The 2D panel (statusiconui.cpp) reads the resulting +0xF0 bitfield.
//
// The status-icon mesh is located and toggled through the SAME registered exe helpers
// Attach/Detach use — the finder Entity_FindComponent_MeshInstance (0x140002f90, keyed off
// the StatusIconComponent type id) and the flag setter FUN_14012c3f0 (0x14012c3f0). Both
// resolve in standalone (seeded by reg_economy_typeids / reg_component_people), so the
// warning is driven by the real toggle code path, not a second port. The raise's cull-guard
// and Scene Add/position/notify all live inside FUN_14012c3f0, so a plain MeshFlag(mesh,4,1)
// reproduces the original's inlined raise byte-for-byte.
//
// The accepting=false path (evict every worker + re-home them) is deliberately NOT ported:
// no standalone caller ever deactivates a workplace (start-town only activates the lodge).
void WorkPlaceComponent_SetAccepting(int64_t comp, bool accepting) {
    EnsureHelpers();
    EnsureWpcBodyHelpers();

    *(char*)(comp + 0x50) = accepting ? 1 : 0;
    if (!accepting) return;   // eviction path unported — see note above.

    // Locate the building's StatusIconComponent (the "mesh" the icon toggles hang off).
    uint64_t meshIbp = 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(comp + 0x10));
    if (s_fn_WPC_FindMesh) s_fn_WPC_FindMesh(*(int64_t*)(comp + 0x10), &meshIbp);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&meshIbp);
    int64_t* mesh = *(int64_t**)&meshIbp;
    if (!mesh || !s_fn_WPC_MeshFlag) return;   // partial-world: no renderable icon.

    // Clear the "closed"/"disabled" indicator bits (1 then 0) — a now-accepting workplace
    // is no longer showing the closed sign.
    s_fn_WPC_MeshFlag(mesh, 1, '\0');
    s_fn_WPC_MeshFlag(mesh, 0, '\0');

    // Raise the "no workers" warning (bit 4) only for an UNSTAFFED workplace still on its
    // designated job (roster empty && jobSlot == desc.job) — the faithful raise condition.
    if (*(int*)(comp + 0x60) != 0) return;                 // already staffed → no warning.
    void* descObj = *(void**)(comp + 0x18);
    void* desc = (fn_IsDerivedFrom && s_TypeId_WorkPlaceDesc &&
                  fn_IsDerivedFrom(descObj, *s_TypeId_WorkPlaceDesc)) ? descObj : nullptr;
    if (!desc) return;                                     // no description → cannot key job.
    if (*(int64_t*)(comp + 0x48) != *(int64_t*)((uintptr_t)desc + 0x20)) return;  // not the job.

    // Re-find the mesh (mirrors the original's per-call finder) and raise bit 4. The
    // cull-guard + Scene Add + positioning + entity-notify all execute inside the toggle.
    meshIbp = 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(comp + 0x10));
    if (s_fn_WPC_FindMesh) s_fn_WPC_FindMesh(*(int64_t*)(comp + 0x10), &meshIbp);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&meshIbp);
    mesh = *(int64_t**)&meshIbp;
    if (mesh) s_fn_WPC_MeshFlag(mesh, 4, '\x01');
}

// Apply the OCCUPIED end-state visual for a house whose roster was filled in roster-only
// mode (g_wpcRosterOnly suppressed the Scene mesh-visual during the build-time founding
// pass — see StartTown_AssignFoundingHousing). This reproduces the visual the attach would
// have driven for the FIRST occupant: ModelKick(mode 2) — notify the building's components
// to switch to their occupied appearance (e.g. smoke/lit windows via ModelComponent
// vtable +0x60) — plus the StatusIconComponent mesh-flag toggle. It is roster-INDEPENDENT
// (forces the kick once for the now-occupied house) because by call time the roster is
// already full (+0x60 != 0), so the original's "first occupant" gate no longer holds.
// MUST be called at GAMEPLAY time (live scene), never during world-build, or Scene::Remove
// Object deadlocks. All visual calls are null-guarded for partial-world entities.
void WorkplaceComponent_ApplyOccupiedVisual(int64_t comp) {
    EnsureHelpers();
    EnsureWpcBodyHelpers();

    // ModelKick(mode 2) — notify the building's components of the occupied state, gated (as
    // the original attach is) on this WorkPlaceComponent being the residence/job the
    // description designates (jobSlot == desc+0x20). This is the faithful notify; it is
    // crash-free. NOTE on visibility: ModelComponent's notify slot (vtable +0x60 =
    // 0x140118ab0) is an EMPTY no-op even in the original exe, and the houses' other
    // visual-bearing components (AmbientEmitterComponent smoke, StatusIconComponent icon)
    // are INERT in standalone (spawned but never scene-registered), so this notify produces
    // no visible change yet — it's correct scaffolding awaiting those components' bring-up.
    void* descObj = *(void**)(comp + 0x18);
    int64_t jobSlot = *(int64_t*)(comp + 0x48);
    void* desc = (fn_IsDerivedFrom && s_TypeId_WorkPlaceDesc &&
                  fn_IsDerivedFrom(descObj, *s_TypeId_WorkPlaceDesc)) ? descObj : nullptr;
    if (desc && jobSlot == *(int64_t*)((uintptr_t)desc + 0x20)) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(comp + 0x10));
        if (s_fn_WPC_ModelKick) s_fn_WPC_ModelKick(*(int64_t*)(comp + 0x10), 2);
    }

    // The StatusIconComponent mesh-flag toggle (FUN_14012c3f0) is DELIBERATELY NOT driven
    // here. It clears the icon flag → flags==0 → Graphics::Scene::RemoveObject(sceneObjId).
    // In standalone the inert StatusIconComponent was never scene-registered, so its mesh
    // sceneObjId (+0x20) holds an uninitialised non-negative value; RemoveObject then SPINS
    // forever on a scene-object id that isn't in the scene (verified by gdb stack-sampling a
    // hung run, even at gameplay frame 3). The original is safe only because the icon is a
    // real registered scene object. Re-enable once StatusIconComponent::OnAdded scene-
    // registers the icon (a rendering follow-up) — the icon is already invisible while inert,
    // so skipping its "hide" toggle changes nothing observable.
}

void WorkPlaceComponent_OnCreateInit(void* selfP) {
    EnsureHelpers();
    EnsureEe60Helpers();
    uintptr_t self = (uintptr_t)selfP;

    while (*(int*)(self + 0x60) > 0) {
        // Termination depends on the detach below removing worker[0] from this
        // component's array (+0x58 / count +0x60) — which it only does when the
        // worker's own job-host back-pointer (+0x248) still names this workplace.
        // Retail holds that invariant; standalone seats workers through several
        // paths (WorkPlaceUI spinners, the builder pool auto-seat), so a stale
        // back-pointer would spin here forever. A HANG is far worse than a leaked
        // roster slot, so drop the entry ourselves if an iteration made no progress.
        const int countBefore = *(int*)(self + 0x60);

        int64_t emptyIbp = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&emptyIbp, nullptr);

        int64_t* workerSlot = *(int64_t**)(self + 0x58);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(workerSlot);
        int64_t worker = *workerSlot;
        int64_t* pJobHost = (int64_t*)(worker + 0x248);

        if (*pJobHost != 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(pJobHost);
            int64_t host = *pJobHost;
            void* comp = WPC_FindJobHostComponent(host, s_pTId_JobHostComp);
            int64_t compIbp = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&compIbp, comp);
            int64_t workerIbp = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&workerIbp, (void*)worker);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&compIbp);
            if (s_fn_WPC_Detach) s_fn_WPC_Detach(compIbp, &workerIbp);
        }

        *pJobHost = emptyIbp;
        if (emptyIbp != 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(pJobHost);
            int64_t host = *pJobHost;
            void* comp = WPC_FindJobHostComponent(host, s_pTId_JobHostComp);
            int64_t compIbp = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&compIbp, comp);
            int64_t workerIbp = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&workerIbp, (void*)worker);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&compIbp);
            if (s_fn_WPC_Attach) s_fn_WPC_Attach(compIbp, &workerIbp);
        }

        if (*(int*)(self + 0x60) >= countBefore) {
            // No progress — shift worker[0] out by hand and keep going.
            int n = *(int*)(self + 0x60);
            int64_t* arr = *(int64_t**)(self + 0x58);
            if (arr) for (int i = 1; i < n; ++i) arr[i - 1] = arr[i];
            *(int*)(self + 0x60) = n - 1;
            static int s_reported = 0;
            if (s_reported++ < 10) {
                printf("[WorkPlaceComponent] release: worker[0]=%p did not detach "
                            "(its +0x248 job host does not name this workplace %p); "
                            "dropped it by hand, roster %d -> %d\n",
                            (void*)worker, selfP, n, n - 1);
            }
        }
    }
}

// town_workplace_registry.cpp — 0x1400895a0, add the owner entity to the profession
// bucket for `key` in the town BST (townMgr+0x100), creating the bucket if absent.
extern "C" void ResidenceComponent_impl_95a0(int64_t townMgr, void* ownerRefLocal, uint64_t key);

// ============================================================
// WorkPlaceComponent_impl_f670 (0x14012f670) — leave the BUILDER profession
// ============================================================
// The construction half of a building's life makes its WorkPlaceComponent a BUILDER
// workplace: BuildComponent state-1 sets WPC+0x48 = the Builder key and registers the
// owner into the town profession BST (townMgr+0x100) so laborers can be seated as
// builders. f670 is the undo: gated on "+0x48 is CURRENTLY the Builder key"
// (*(gameWorld+0xb0)+0x38), it removes the owner from the Builder bucket, refreshes the
// WorkComponent capacity, hands any pending work over to the natural profession, resets
// +0x48 to the WorkPlaceDescription's own profession (+0x20 — zero for a house), and
// releases every seated worker (ee60).
//
// Callers in retail (Ghidra xrefs): the DestroyComponent state-5 tick (0x14011c120) and
// its bcc0 transition action, plus edf0 below.
//
// FUN_1400f8400 (the work-scheduler re-key: move the pending task node from the old
// profession bucket to the new one, mgr+0x120 keyed by the work id) is NOT ported. It is
// reachable only when the WorkComponent still holds a work id (+0x28 >= 0); at build
// completion State8OneShot has already dequeued the BuildStructure work, so there is
// normally nothing to re-key. Logged once if the branch is ever taken.
extern "C" void WorkPlaceComponent_LeaveBuilderProfession(void* selfP) {
    EnsureHelpers();
    EnsureEe60Helpers();
    if (!selfP || !g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t self = reinterpret_cast<uintptr_t>(selfP);
    uintptr_t gw   = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);

    uint64_t key = *reinterpret_cast<uint64_t*>(self + 0x48);
    uintptr_t profReg = *reinterpret_cast<uintptr_t*>(gw + 0xb0);
    uint64_t builderKey = profReg ? *reinterpret_cast<uint64_t*>(profReg + 0x38) : 0;
    if (key == 0 || key != builderKey) return;   // not a construction workplace

    // Remove the owner entity from the Builder bucket in the profession BST.
    int64_t ownerRef = *reinterpret_cast<int64_t*>(self + 0x10);
    uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(gw + 0xf0);
    uint64_t* node = townMgr ? *reinterpret_cast<uint64_t**>(townMgr + 0x100) : nullptr;
    while (node) {
        if (key < node[0]) {
            node = reinterpret_cast<uint64_t*>(node[4]);          // left
        } else {
            if (key <= node[0]) {
                if (s_fn_RemoveFromList) {
                    uint64_t localRef[4] = { static_cast<uint64_t>(ownerRef), 0, 0, 0 };
                    s_fn_RemoveFromList(reinterpret_cast<void*>(node + 1), localRef);
                }
                break;
            }
            node = reinterpret_cast<uint64_t*>(node[5]);          // right
        }
    }

    // Refresh the WorkComponent's worker capacity (+0x30 = CalcMaxWorkers).
    static void* s_find3050 = nullptr;
    static void* s_calcMaxWorkers = nullptr;
    if (!s_find3050) s_find3050 = reinterpret_cast<void*>(ResolveAddr(Module::Exe, 0x140003050));
    if (!s_calcMaxWorkers)
        s_calcMaxWorkers = reinterpret_cast<void*>(ResolveAddr(Module::Exe, 0x14012e5c0));
    uintptr_t workComp = 0;
    if (s_find3050) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x10));
        reinterpret_cast<void*(*)(int64_t, void*)>(s_find3050)(
            *reinterpret_cast<int64_t*>(self + 0x10), &workComp);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&workComp);
    }
    if (workComp && s_calcMaxWorkers)
        *reinterpret_cast<int32_t*>(workComp + 0x30) =
            reinterpret_cast<int(*)(int64_t)>(s_calcMaxWorkers)(static_cast<int64_t>(workComp));

    // The natural profession comes from the linked WorkPlaceDescription (+0x20). Retail
    // guards it with IsDerivedFrom(desc, DAT_1401c8118); in standalone that check rejects
    // valid descriptions in the partial factory tree (the documented landmine — see
    // MapComponent_DetachOccupancy_4750), so trust the linked description. A null desc
    // yields key 0 = "no profession", which is the correct end state for a house.
    void* desc = *reinterpret_cast<void**>(self + 0x18);
    uint64_t naturalKey = desc
        ? *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(desc) + 0x20) : 0;

    if (workComp && *reinterpret_cast<int32_t*>(workComp + 0x28) >= 0) {
        static bool s_warned = false;
        if (!s_warned) {
            s_warned = true;
            printf("[WorkPlaceComponent] NOT-IMPLEMENTED FUN_1400f8400 (work-scheduler "
                        "profession re-key) — workComp+0x28=%d still holds a work id; the "
                        "pending task keeps its old Builder key\n",
                        *reinterpret_cast<int32_t*>(workComp + 0x28));
        }
    }

    *reinterpret_cast<uint64_t*>(self + 0x48) = naturalKey;
    WorkPlaceComponent_OnCreateInit(selfP);   // ee60 — release every seated worker
}

// ============================================================
// WorkPlaceDescription_helper_edf0 (0x14012edf0) — the WorkPlaceComponent controller's
// state-2 ("built") ENTER handler.
// ============================================================
// State table proof (WorkPlaceDescription_helper_f3a0 @ 0x14012f3a0, the component's
// controller table builder): state 0 -> ee40, state 2 -> edf0, state 3 -> ed60. So the
// moment the entity's state machine enters state 2 (BuildComponent's 8->2 completion),
// retail runs f670 + re-register under whatever profession the building actually has.
//
// This is what makes the finished building's panel correct: WorkPlaceUI shows the worker
// spinner iff WPC+0x48 != 0 (workplaceui.cpp UpdateVisibility, faithful to Startup
// 0x140087080). Without this the Builder key survived completion, so a finished house
// kept showing the construction worker up/down buttons.
//
// Our WorkPlaceComponent is an inert component with no controller node, so the state
// machine cannot dispatch this. It is therefore driven from the build-completion site
// (BuildComponent_State8Finalize) — the same "collapse the missing controller into the
// site" pattern as MapComponent's cell occupancy. If WorkPlaceComponent is ever promoted
// to a live controller, move this call into its state-2 enter and delete the hook.
extern "C" void WorkPlaceComponent_EnterBuiltState(void* selfP) {
    if (!selfP) return;
    WorkPlaceComponent_LeaveBuilderProfession(selfP);

    uintptr_t self = reinterpret_cast<uintptr_t>(selfP);
    uint64_t key = *reinterpret_cast<uint64_t*>(self + 0x48);
    if (key == 0) return;   // no profession (house / storage) -> nothing to register
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
    if (!townMgr) return;
    int64_t ownerRef = *reinterpret_cast<int64_t*>(self + 0x10);
    ResidenceComponent_impl_95a0(static_cast<int64_t>(townMgr), &ownerRef, key);
}

// ============================================================
// WorkPlaceComponent::Shutdown (vtable slot [3], 0x14012f120)
// ============================================================
// Component_Serialize + empty serializer(v0) + versioned serializer(v1) with:
//   - worker count at +0x60, worker InternalBasePtr array at +0x58
//   - IsDerivedFrom validation on each worker ptr
//   - isAccepting bool at +0x50
//   - version < 1: read +0x48 from description component
//   - version >= 1: serialize +0x48 via FUN_140018020

void WorkPlaceComponent_Shutdown(WorkPlaceComponent* self, void* stream) {
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

    // Main serializer block (version 1)
    uint8_t serializer[16];  // Ghidra shows local_30[2] = 16 bytes
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializer, stream, 1);
    }

    // Serialize worker count from +0x60
    int workerCount = *(int*)((uintptr_t)self + 0x60);
    int countBuf = workerCount;
    if (s_fn_Serializer_SerializeInt) {
        s_fn_Serializer_SerializeInt(serializer, &countBuf, nullptr);
    }

    // On load (version byte in serializer+8 == 0), resize array
    if (*(int*)((uintptr_t)serializer + 8) == 0) {
        if (s_fn_ResizeIBPArray) {
            s_fn_ResizeIBPArray((void*)((uintptr_t)self + 0x58), countBuf);
        }
    }

    // Serialize each worker InternalBasePtr and validate type
    void* workerArray = *(void**)((uintptr_t)self + 0x58);
    int64_t count = (int64_t)countBuf;
    if (count > 0 && workerArray) {
        uintptr_t ptr = (uintptr_t)workerArray;
        for (int64_t i = 0; i < count; i++) {
            if (s_fn_IBP_Serialize) {
                s_fn_IBP_Serialize((void*)ptr, serializer);
            }
            // Validate: if ptr dereferences to non-null, check IsDerivedFrom
            void* obj = *(void**)ptr;
            if (obj != nullptr && s_fn_IsDerivedFrom_dispatch && s_TypeId_WorkerType) {
                bool valid = s_fn_IsDerivedFrom_dispatch(obj, *s_TypeId_WorkerType);
                if (!valid) {
                    *(uint64_t*)ptr = 0;  // Clear invalid reference
                }
            }
            ptr += 8;  // InternalBasePtr is 8 bytes
        }
    }

    // Serialize isAccepting bool at +0x50
    if (s_fn_Serializer_SerializeBool) {
        s_fn_Serializer_SerializeBool(serializer, (bool*)&self->isAccepting, nullptr);
    }

    // Version check for +0x48 field
    if (s_fn_Serializer_GetVersion) {
        int version = s_fn_Serializer_GetVersion(serializer);
        if (version < 1) {
            // Legacy path: read from description component via IsDerivedFrom
            void* obj = self->_unk18;
            if (s_fn_IsDerivedFrom_dispatch && s_TypeId_WorkPlaceDesc) {
                bool derived = s_fn_IsDerivedFrom_dispatch(obj, *s_TypeId_WorkPlaceDesc);
                void* descComp = derived ? obj : nullptr;
                if (descComp) {
                    *(uint64_t*)&self->workerData = *(uint64_t*)((uintptr_t)descComp + 0x20);
                }
            }
        } else {
            // Version >= 1: serialize via FUN_140018020
            if (s_fn_SerializeIBP) {
                s_fn_SerializeIBP((void*)((uintptr_t)self + 0x48), serializer);
            }
        }
    }

    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// WorkPlaceComponent::OnRemovedFromWorld (vtable slot [7], 0x14012f090)
// ============================================================
// Slot 7 (+0x38) is the REMOVAL broadcast slot — FUN_1400c2ea0 (MarkForRemoval)
// dispatches it to every component whose +0x20 capability flags have bit 1 set
// (WorkPlaceComponent's are 0x82, so it qualifies). The Ghidra name "OnCreate" is a
// misnomer, the same one MapComponent carries: this is the TEARDOWN.
//
// It reads +0x48 (the profession key), finds that key's bucket in the town's
// profession BST (gameWorld+0xf0 -> +0x100), and REMOVES this component's owner
// entity from the bucket's entity array (FUN_140002cc0). Then, unconditionally,
// ee60 releases every seated worker.
//
// Why it matters in standalone: while a building is under construction its
// WorkPlaceComponent is registered under the Builder profession (the list
// CitizenBehavior_BuildPathGraph and FindAvailableBuilding walk). With the inert
// factory's no-op slot 7 the entry survived the entity's free, and the next
// BuildPathGraph read the freed entity through it — an ACCESS_VIOLATION at
// citizenbehavior.cpp:716 (fn_Entity_GetGridId on the stale IBP), observed on the
// first successful building upgrade. Same class as the MapComponent cell-occupancy
// and PickingComponent pick-mesh teardowns.

void WorkPlaceComponent_OnCreate(WorkPlaceComponent* self) {
    EnsureHelpers();

    uint64_t professionId = *(uint64_t*)((uintptr_t)self + 0x48);
    if (professionId != 0 && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        // Build InternalBasePtr for owner
        uint64_t ownerRef = *(uint64_t*)((uintptr_t)self + 0x10);

        // Get BST root from game world +0xf0 -> +0x100
        uintptr_t gameState = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xf0);
        uint64_t* node = *(uint64_t**)(gameState + 0x100);

        bool found = false;
        while (node != nullptr) {
            if (professionId < *node) {
                node = (uint64_t*)node[4];  // left child
            } else if (professionId > *node) {
                node = (uint64_t*)node[5];  // right child
            } else {
                // Found: UNregister the owner entity from this bucket's entity array.
                if (s_fn_RemoveFromList) {
                    uint64_t localRef[4];
                    localRef[0] = ownerRef;
                    s_fn_RemoveFromList((void*)(node + 1), (void*)localRef);
                }
                found = true;
                break;
            }
        }

        if (!found && s_fn_OnCreateFallback) {
            s_fn_OnCreateFallback(self);
            return;
        }
    }

    // Always call fallback init
    if (s_fn_OnCreateFallback) {
        s_fn_OnCreateFallback(self);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void WorkPlaceComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[WorkPlaceComponent] Funcs resolved: Component_Serialize=%p Serializer_Ctor=%p "
           "ResizeIBPArray=%p AddToList=%p OnCreateFallback=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_ResizeIBPArray, (void*)s_fn_RemoveFromList,
           (void*)s_fn_OnCreateFallback);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// COMPONENT_INERT_FACTORY(WorkPlaceComponent, 0x68, 0x82), expanded by hand so the
// per-type vtable's +0x38 slot (index 7 — the slot FUN_1400c2ea0 dispatches at entity
// removal) routes to the ported removal hook above instead of a stub. Without it a
// demolished/upgraded building stays in the town profession BST and the next
// CitizenBehavior_BuildPathGraph derefs the freed entity. Mirrors the MapComponent and
// PickingComponent hand-expansions.
static uint32_t WorkPlaceComponent_InertGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("WorkPlaceComponent");
    return cached;
}
static void* WorkPlaceComponent_InertVtable[::ComponentFactory::kInertVtableSlots];
static void WorkPlaceComponent_DetachVSlot(void* s) {
    if (s) WorkPlaceComponent_OnCreate(reinterpret_cast<WorkPlaceComponent*>(s));
}
static void WorkPlaceComponent_FinishVtable() {
    WorkPlaceComponent_InertVtable[7] =
        reinterpret_cast<void*>(&WorkPlaceComponent_DetachVSlot);
}
extern "C" void* WorkPlaceComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x68, 0x82,
        WorkPlaceComponent_InertVtable,
        reinterpret_cast<void*>(&WorkPlaceComponent_InertGetTypeId));
    WorkPlaceComponent_FinishVtable();
    return p;
}
static void* WorkPlaceComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x68, 0x82,
        WorkPlaceComponent_InertVtable,
        reinterpret_cast<void*>(&WorkPlaceComponent_InertGetTypeId));
    WorkPlaceComponent_FinishVtable();
    return mem;
}
REGISTER_TYPE(WorkPlaceComponent, "ControllerComponent",
              &WorkPlaceComponent_Factory, &WorkPlaceComponent_CreateInPlace);
