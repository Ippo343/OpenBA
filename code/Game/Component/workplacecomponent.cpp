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
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (ProfKeyOwnerClass diagnostic)
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "scene_watch.h"

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
// SetAccepting pause path (0x14012fc00): the owner's BuildComponent state finder, the two
// citizen workplace finders the eviction loop dispatches between, and AssignHousing.
typedef void* (*fn_WPC_FindBuildState_t)(int64_t entity, void* outIbp);   // 0x140003ea0
typedef void* (*fn_WPC_FindWork_t)(int64_t citizen, int64_t* outIbp);     // 0x14010ffc0 / 0x140110470
typedef void  (*fn_WPC_AssignHousing_t)(void* citizen, int64_t* housingIbp); // 0x140111ee0
static fn_WPC_FindBuildState_t s_fn_WPC_FindBuildState = nullptr;
static fn_WPC_FindWork_t       s_fn_WPC_FindWorkUnassigned = nullptr;
static fn_WPC_FindWork_t       s_fn_WPC_FindWorkAssigned = nullptr;
static fn_WPC_AssignHousing_t  s_fn_WPC_AssignHousing = nullptr;
static bool s_wpcBodyResolved = false;
static void EnsureWpcBodyHelpers() {
    if (s_wpcBodyResolved) return;
    s_fn_WPC_FindBuildState = reinterpret_cast<fn_WPC_FindBuildState_t>(ResolveAddr(Module::Exe, 0x140003ea0));
    s_fn_WPC_FindWorkUnassigned = reinterpret_cast<fn_WPC_FindWork_t>(ResolveAddr(Module::Exe, 0x14010ffc0));
    s_fn_WPC_FindWorkAssigned   = reinterpret_cast<fn_WPC_FindWork_t>(ResolveAddr(Module::Exe, 0x140110470));
    s_fn_WPC_AssignHousing  = reinterpret_cast<fn_WPC_AssignHousing_t>(ResolveAddr(Module::Exe, 0x140111ee0));
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

    // The roster decrement lives in RemoveFromList below. ee60 is NOT the only drain
    // (measured: a seat vanished with zero [SeatDrain] hits), so trace the unseat at
    // its true source, with the caller's return address. Uncapped; symbolize with
    // tools/symbolize.py.
    if (std::getenv("BANISHED_SEAT_TRACE")) {
        fprintf(stderr, "[SeatDrain] Detach wpc=%p roster(+0x60)=%d key(+0x48)=0x%llx ret=%p\n",
                (void*)comp, *(int*)(comp + 0x60),
                (unsigned long long)*(uint64_t*)(comp + 0x48),
                __builtin_return_address(0));
        fflush(stderr);
    }

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
    // NOTE this block is retail 0x14012c3f0 INLINED, and that function is ported in TWO
    // other places as well -- CitizenComp_FUN_14012c3f0 (citizencomponent.cpp, the copy
    // reg_component_people.cpp registers) and StatusIcon_Toggle (statusiconcomponent.cpp).
    // Three copies, each historically unsafe in a DIFFERENT half; see
    // [[reference_one_retail_fn_ported_twice_diverges]]. Touch one, check all three.
    //
    // Both hazards here are closed UPSTREAM by StatusIconComponent_CtorScalars
    // (statusiconcomponent.cpp), which builds the embedded geometry and sets
    // sceneObjId(+0x100) = -1 at CONSTRUCTION, so they hold for stream-restored icons too:
    //   * the clear arm below tests `>= 0` with NO sentinel repair, so an uninitialised
    //     sceneId of 0 used to PASS it and call Scene::RemoveObject(scene, 0) on a handle
    //     the scene never issued -- the non-terminating quadtree walk (d06def5), a FREEZE;
    //   * the set arm used to AddObject a geometry whose vtable was still 0 -- the corrupt
    //     scene node behind the `EXEC of <heap>` in Scene::Clean.
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
                SceneWatch_Note(id, (void*)(mesh + 5), "WorkPlaceComponent carry-mesh");
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
// Pause-path eviction: drain the roster, re-homing each worker (0x14012fc00 tail)
// ============================================================
// Retail's loop always takes roster entry [0], asks the citizen's own workplace finder
// for somewhere else to work (sub_ffc0 when the profession's +0x4c "assigned" byte is
// clear, sub_0470 when set — the same dispatch CitizenComp_helper_ff80 wraps), and
// AssignHousing's it there. AssignHousing detaches from the old workplace first, which is
// what decrements +0x60 and makes the loop terminate.
//
// TWO STANDALONE DEVIATIONS:
//  1. g_wpcRosterOnly is held across the whole loop. Without it each AssignHousing drives
//     the Detach/Attach status-icon visuals, i.e. FUN_14012c3f0 -> Graphics::Scene::
//     RemoveObject on the building's StatusIconComponent. Our components come from the
//     inert factory (zero-filled from +0x28), so a never-registered icon reads scene id 0
//     instead of -1 and RemoveObject(scene, 0) removes whichever object genuinely owns id
//     0 — the engine's list walk then spins and the game freezes in-world (the failure
//     mode fixed at source for HighlightComponent in 0f97d16). Roster-only keeps the list
//     mutation, which is all the loop's termination depends on.
//  2. A progress guard. Retail's `while (+0x60 > 0)` assumes the finder never hands back
//     THIS workplace; if it did, the count never drops. An unterminated loop HERE is an
//     in-world freeze rather than a crash — the worst failure mode this codebase has (see
//     reference_inert_component_sentinel_fields) — so it breaks out loudly instead. This
//     is the ONLY deviation left in this function; it cannot fire on well-formed state.
//
// HISTORY (2026-07-25): this loop faulted twice on live clicks — READ of 0x8, then READ of
// 0xffffffffffffffff inside AssignHousing. Neither was a bug in this port. BuildUI_Slot9
// was handing SetAccepting a StorageLocationComponent (c8070) instead of the
// WorkPlaceComponent (c80e0), because engine_funcs.h's fn_FindComponent_Work is
// MISNAMED — it is the c8070 finder. So +0x50/+0x58/+0x60 were read off the wrong object
// and "roster base == 8 with count > 0" was simply an unrelated field. Fixed at the source
// in buildui.cpp; the plausibility/entity checks that were added here to survive the
// garbage have been REMOVED rather than left as permanent scar tissue, since with the
// correct component every operand is a live object (verified live: two pause clicks, real
// citizens evicted, zero guard trips).
// component_factory_base.h is only included further down (factory section), so declare the
// one entry point needed here.
namespace ComponentFactory { bool LiveEnabled(const char* envVar); }

static void WorkPlaceComponent_EvictRoster(int64_t comp) {
    EnsureWpcBodyHelpers();
    if (!s_fn_WPC_AssignHousing) return;

    const bool savedRosterOnly = g_wpcRosterOnly;
    g_wpcRosterOnly = true;

    const bool trace = std::getenv("BANISHED_DESTROY_TRACE") != nullptr;
    int guard = 0;
    while (*(int*)(comp + 0x60) > 0) {
        int before = *(int*)(comp + 0x60);

        int64_t* roster = *(int64_t**)(comp + 0x58);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(roster);
        int64_t worker = *roster;

        // Pick the finder off the worker's profession record (+0x1f0 -> +0x4c).
        int64_t profession = *(int64_t*)(worker + 0x1f0);
        int64_t newWorkplaceIbp = 0;
        if (*(char*)(profession + 0x4c) == '\0') {
            if (s_fn_WPC_FindWorkUnassigned) s_fn_WPC_FindWorkUnassigned(worker, &newWorkplaceIbp);
        } else {
            if (s_fn_WPC_FindWorkAssigned) s_fn_WPC_FindWorkAssigned(worker, &newWorkplaceIbp);
        }

        // Re-read the roster base — the finders above can reallocate it.
        int64_t* rosterNow = *(int64_t**)(comp + 0x58);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(rosterNow);
        if (trace) {
            std::fprintf(stderr, "[WorkPlace] evict: comp=%p worker=%p cur(+0x248)=%p "
                         "target=%p roster=%d\n", (void*)comp, (void*)*rosterNow,
                         (void*)*(int64_t*)(*rosterNow + 0x248), (void*)newWorkplaceIbp, before);
            std::fflush(stderr);
        }
        s_fn_WPC_AssignHousing((void*)*rosterNow, &newWorkplaceIbp);

        if (*(int*)(comp + 0x60) >= before || ++guard > 4096) {
            std::fprintf(stderr, "[WorkPlace] pause eviction made no progress on comp=%p "
                         "(roster %d -> %d, worker=%p) — breaking to avoid a spin\n",
                         (void*)comp, before, *(int*)(comp + 0x60), (void*)worker);
            std::fflush(stderr);
            break;
        }
    }

    g_wpcRosterOnly = savedRosterOnly;
}

// ============================================================
// WorkPlaceComponent::SetAccepting — faithful port of Entity_impl_fc00 (0x14012fc00).
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
// The accepting=false (PAUSE) path does three more things, all ported below:
//   a. raise the "closed" indicator — bit 0 when the building's BuildComponent state
//      (found via 0x140003ea0, byte +0x52 or +0x50 when +0x52 is negative) is one of
//      {0,1,8} ("(s & 0xf6) == 0 && s != 9"), bit 1 otherwise
//   b. clear the worker/warning icons — flags 2, 3 and 4
//   c. EVICT the whole roster: repeatedly take entry [0], find it somewhere else to work
//      (sub_ffc0 / sub_0470, picked on profession+0x4c) and AssignHousing it there
//
// On (a): the original inlines FUN_14012c3f0's body rather than calling it, which is why
// the decompile shows an unreachable "mask == 0" arm right after an OR — that arm is the
// generic helper's CLEAR path, dead for a SET. `mask |= 1 << !normalState` plus the tail is
// bit-for-bit FUN_14012c3f0(mesh, normalState ? 0 : 1, 1), so the call below is faithful,
// not an approximation. (Same helper the citizen paths use: it is generic over the
// StatusIconComponent layout — mask +0xf0, scene id +0x100 — not citizen-specific.)
// NB: this is not the only writer of +0x50. A raw field write lives at
// buildcomponent_state1.cpp:427 (construction-site enable, no status icon), and the
// ctor default is set in WorkPlaceComponent_FinishLiveVtable above. A grep for
// "SetAccepting" will miss both — see the block at buildcomponent_state1.cpp for why
// that matters.
void WorkPlaceComponent_SetAccepting(int64_t comp, bool accepting) {
    EnsureHelpers();
    EnsureWpcBodyHelpers();

    // accepting=false is the PAUSE path: it raises the "work is disabled at this
    // location" indicator AND evicts the whole roster. Trace its caller — this is the
    // level that actually decides a workplace should close.
    if (std::getenv("BANISHED_SEAT_TRACE") && !accepting) {
        fprintf(stderr, "[SetAccept] CLOSE wpc=%p was(+0x50)=%d roster(+0x60)=%d "
                        "key(+0x48)=0x%llx ret=%p\n",
                (void*)comp, (int)*(char*)(comp + 0x50), *(int*)(comp + 0x60),
                (unsigned long long)*(uint64_t*)(comp + 0x48),
                __builtin_return_address(0));
        fflush(stderr);
    }

    *(char*)(comp + 0x50) = accepting ? 1 : 0;

    // BANISHED_LIVE_WPC_PAUSE — DEFAULT-ON via kDefaultOnFlags (graduated 2026-07-25); the
    // env var is a kill-switch (=0). The three live faults this path hit when first landed
    // were ALL downstream of BuildUI_Slot9 passing the wrong component type (a c8070
    // StorageLocationComponent instead of the c80e0 WorkPlaceComponent — see the history
    // note on EvictRoster above); fixed at the source in buildui.cpp. Verified live: pause
    // on a staffed workplace evicts real citizens with zero guard trips, and the closed
    // indicator renders both above the building and in the dialog status bar.
    static int s_pauseEnabled = -1;
    if (s_pauseEnabled < 0)
        s_pauseEnabled = ::ComponentFactory::LiveEnabled("BANISHED_LIVE_WPC_PAUSE") ? 1 : 0;
    if (!accepting && !s_pauseEnabled) return;   // kill-switch: flag-only, pre-port behaviour

    // Locate the building's StatusIconComponent (the "mesh" the icon toggles hang off).
    uint64_t meshIbp = 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(comp + 0x10));
    if (s_fn_WPC_FindMesh) s_fn_WPC_FindMesh(*(int64_t*)(comp + 0x10), &meshIbp);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&meshIbp);
    int64_t* mesh = *(int64_t**)&meshIbp;
    if (!mesh || !s_fn_WPC_MeshFlag) {
        // Partial-world: no renderable icon. The eviction below is gameplay state, not
        // visuals, so it must still run on the pause path.
        if (!accepting) WorkPlaceComponent_EvictRoster(comp);
        return;
    }

    if (accepting) {
        // Clear the "closed"/"disabled" indicator bits (1 then 0) — a now-accepting
        // workplace is no longer showing the closed sign.
        s_fn_WPC_MeshFlag(mesh, 1, '\0');
        s_fn_WPC_MeshFlag(mesh, 0, '\0');
    } else {
        // (a) raise the closed indicator, keyed on the owner's build state.
        uint64_t stateIbp = 0;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(comp + 0x10));
        if (s_fn_WPC_FindBuildState) s_fn_WPC_FindBuildState(*(int64_t*)(comp + 0x10), &stateIbp);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&stateIbp);
        // Partial-world guard: a standalone workplace can lack the BuildComponent. The
        // original derefs it unguarded; with none present treat the building as "normal"
        // (the state the start-town / force-completed buildings are actually in).
        uint8_t state = 0;
        if (stateIbp) {
            state = *(uint8_t*)(stateIbp + 0x52);
            if ((int8_t)state < 0) state = *(uint8_t*)(stateIbp + 0x50);
        }
        bool normalState = ((state & 0xf6) == 0) && (state != 9);
        s_fn_WPC_MeshFlag(mesh, normalState ? 0 : 1, '\x01');
    }

    if (!accepting) {
        // (b) clear the worker/warning icons. Each toggle re-finds the mesh in the
        // original; the finder is pure, so one lookup is equivalent.
        s_fn_WPC_MeshFlag(mesh, 2, '\0');
        s_fn_WPC_MeshFlag(mesh, 3, '\0');
        s_fn_WPC_MeshFlag(mesh, 4, '\0');
    } else if (*(int*)(comp + 0x60) == 0) {
        // Raise the "no workers" warning (bit 4) only for an UNSTAFFED workplace still on
        // its designated job (roster empty && jobSlot == desc.job) — the faithful raise.
        void* descObj = *(void**)(comp + 0x18);
        void* desc = (fn_IsDerivedFrom && s_TypeId_WorkPlaceDesc &&
                      fn_IsDerivedFrom(descObj, *s_TypeId_WorkPlaceDesc)) ? descObj : nullptr;
        if (desc && *(int64_t*)(comp + 0x48) == *(int64_t*)((uintptr_t)desc + 0x20)) {
            // Re-find the mesh (mirrors the original's per-call finder) and raise bit 4. The
            // cull-guard + Scene Add + positioning + entity-notify all run inside the toggle.
            meshIbp = 0;
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(comp + 0x10));
            if (s_fn_WPC_FindMesh) s_fn_WPC_FindMesh(*(int64_t*)(comp + 0x10), &meshIbp);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&meshIbp);
            mesh = *(int64_t**)&meshIbp;
            if (mesh) s_fn_WPC_MeshFlag(mesh, 4, '\x01');
        }
    }

    // (c) evict the roster.
    if (!accepting) WorkPlaceComponent_EvictRoster(comp);
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

    // ee60 is THE roster drain (the loop below runs to +0x60 == 0) and it has five
    // retail callers (f670, f520, ed60, OnRemove x2) plus vtable slots. When a seat
    // silently disappears, the only useful fact is WHICH caller drained it — so log
    // the return address, uncapped. Symbolize with tools/symbolize.py.
    if (std::getenv("BANISHED_SEAT_TRACE") && *(int*)(self + 0x60) > 0) {
        fprintf(stderr, "[SeatDrain] ee60 wpc=%p roster(+0x60)=%d key(+0x48)=0x%llx ret=%p\n",
                selfP, *(int*)(self + 0x60),
                (unsigned long long)*(uint64_t*)(self + 0x48),
                __builtin_return_address(0));
        fflush(stderr);
    }

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
// BANISHED_PROFKEY_TRACE - low-noise trace of the two events that decide whether a
// finished building ever gets a profession key in WPC+0x48. That field gates the whole
// workPlace ribbon (worker up/down spinner + stop sign) in WorkplaceUI_Startup, so a
// building that misses both events silently has no worker arrows. BUILD_TRACE covers
// this ground too but floods ~60k lines and slows the sim; these fire once per building.
static bool ProfKeyTrace() {
    static int on = -1;
    if (on < 0) {
        const char* e = std::getenv("BANISHED_PROFKEY_TRACE");
        on = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return on == 1;
}

// Name the building a [ProfKey] line refers to, by scanning the owner entity's
// component array for a recognisable gameplay component. Diagnostic only.
static const char* ProfKeyOwnerClass(uintptr_t self) {
    static const char* kNames[] = {
        "CropFieldComponent", "OrchardComponent", "PastureComponent",
        "FishingComponent", "HuntComponent", "HerbalistComponent",
        "ForesterComponent", "MarketComponent", "AttendanceComponent",
        "ConsumeProduceComponent", "ResidenceComponent", "StorageComponent",
    };
    int64_t ownerRef = *reinterpret_cast<int64_t*>(self + 0x10);
    if (!ownerRef) return "?";
    uintptr_t ent = static_cast<uintptr_t>(ownerRef);
    int count = *reinterpret_cast<int*>(ent + 0x38);
    int64_t arr = *reinterpret_cast<int64_t*>(ent + 0x30);
    if (!arr || count <= 0 || count > 256) return "?";
    for (unsigned k = 0; k < sizeof(kNames)/sizeof(kNames[0]); k++) {
        uint32_t want = ::Object::GetTypeIdByName(kNames[k]);
        if (!want) continue;
        for (int i = 0; i < count; i++) {
            void* comp = reinterpret_cast<void*>(*reinterpret_cast<int64_t*>(arr + (int64_t)i * 8));
            if (!comp) continue;
            typedef uint32_t (*GetTypeId_t)(void*);
            uint32_t tid = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
            if (tid == want) return kNames[k];
        }
    }
    return "(other)";
}

extern "C" void WorkPlaceComponent_LeaveBuilderProfession(void* selfP) {
    EnsureHelpers();
    EnsureEe60Helpers();
    if (!selfP || !g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t self = reinterpret_cast<uintptr_t>(selfP);
    uintptr_t gw   = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);

    uint64_t key = *reinterpret_cast<uint64_t*>(self + 0x48);
    uintptr_t profReg = *reinterpret_cast<uintptr_t*>(gw + 0xb0);
    uint64_t builderKey = profReg ? *reinterpret_cast<uint64_t*>(profReg + 0x38) : 0;
    if (key == 0 || key != builderKey) {
        if (ProfKeyTrace())
            std::fprintf(stderr, "[ProfKey] LeaveBuilder BAILED wpc=%p key(+0x48)=0x%llx builderKey=0x%llx"
                         " -- natural profession NOT assigned; ribbon stays hidden\n",
                         selfP, (unsigned long long)key, (unsigned long long)builderKey);
        return;   // not a construction workplace
    }
    if (ProfKeyTrace())
        std::fprintf(stderr, "[ProfKey] LeaveBuilder ENTER wpc=%p key(+0x48)=0x%llx == builderKey\n",
                     selfP, (unsigned long long)key);

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
    // reads it through IsDerivedFrom(desc, DAT_1401c8118) — and if that check FAILS retail
    // dereferences null+0x20, i.e. it crashes rather than resetting the key to 0. So the
    // check is an assertion, not a branch, and there is no retail behaviour to be faithful
    // to on the failing side. We run it for its diagnostic value and, when it rejects a
    // description we do have (the partial-factory-tree landmine — see
    // MapComponent_DetachOccupancy_4750), log once and trust the linked description anyway.
    // A genuinely null desc yields key 0 = "no profession", the correct end state for a house.
    void* desc = *reinterpret_cast<void**>(self + 0x18);
    if (desc && fn_IsDerivedFrom && s_TypeId_WorkPlaceDesc && *s_TypeId_WorkPlaceDesc &&
        !fn_IsDerivedFrom(desc, *s_TypeId_WorkPlaceDesc)) {
        static bool s_warnedDesc = false;
        if (!s_warnedDesc) {
            s_warnedDesc = true;
            printf("[WorkPlaceComponent] LeaveBuilderProfession: WPC+0x18=%p is not a "
                   "WorkPlaceDescription (type id %u) — retail would fault here; trusting "
                   "it and reading +0x20 anyway\n", desc, *s_TypeId_WorkPlaceDesc);
        }
    }
    uint64_t naturalKey = desc
        ? *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(desc) + 0x20) : 0;

    // Hand any in-flight work item over from Builder to the natural profession, so it stops
    // being offered to builders the moment the building is finished. FUN_1400f8400 —
    // citizenai_work_scheduler.cpp CitizenAI_WorkScheduler_Rekey.
    if (workComp && *reinterpret_cast<int32_t*>(workComp + 0x28) >= 0) {
        typedef void (*Rekey_t)(uintptr_t, uint32_t, uint64_t, uint64_t);
        static Rekey_t s_rekey = nullptr;
        static bool s_rekeyRes = false;
        if (!s_rekeyRes) {
            s_rekeyRes = true;
            s_rekey = reinterpret_cast<Rekey_t>(ResolveAddr(Module::Exe, 0x1400f8400));
        }
        uintptr_t sched = *reinterpret_cast<uintptr_t*>(gw + 0x118);
        if (s_rekey && sched)
            s_rekey(sched, *reinterpret_cast<uint32_t*>(workComp + 0x28),
                    *reinterpret_cast<uint64_t*>(self + 0x48), naturalKey);
    }

    *reinterpret_cast<uint64_t*>(self + 0x48) = naturalKey;
    if (ProfKeyTrace())
        std::fprintf(stderr, "[ProfKey] LeaveBuilder SET %-22s wpc=%p desc(+0x18)=%p descIsWPDesc=%d natural(+0x48)=0x%llx%s\n",
                     ProfKeyOwnerClass(self), selfP, desc,
                     (desc && fn_IsDerivedFrom && s_TypeId_WorkPlaceDesc && *s_TypeId_WorkPlaceDesc)
                         ? (int)fn_IsDerivedFrom(desc, *s_TypeId_WorkPlaceDesc) : -1,
                     (unsigned long long)naturalKey,
                     naturalKey == 0
                         ? (desc ? "  <-- desc PRESENT but its +0x20 profession is 0"
                                 : "  <-- desc is NULL (WPC+0x18 never linked)")
                         : "");
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
// WorkPlaceComponent_EnterBuiltState_Full — state-2 ("built") entry for ANY workplace
// ============================================================
// Retail dispatches edf0 from the WorkPlaceComponent controller's own state-2 enter, so
// EVERY workplace gets its profession seated the moment its entity's state machine
// reaches state 2. Our WorkPlaceComponent is inert (no controller node), and the port
// collapsed that dispatch into BuildComponent_State8Finalize — which is strictly
// narrower than retail's trigger. A player-placed CROP FIELD or ORCHARD completes
// through its OWN state machine and has no BuildComponent completion, so it never
// reached the hand-off at all:
//
//   MEASURED 2026-08-18, hand-built crop field: WPC+0x48 = NULL, and
//   [WorkPlaceUI] 0 shown / 5466 HIDDEN -- WorkplaceUI_Startup hides the whole
//   workPlace ribbon (worker up/down spinner AND the stop-sign icon) on +0x48 == 0.
//   The same session logged 8 construction hand-offs, all storage/houses/fishing dock,
//   and ZERO events for the crop field or orchard.
//
// TRIGGER: driven from StateMachineComponent on the entity's FIRST TICK and on any
// state change - NOT on state index 2. Retail keys the WPC controller off state 2,
// but a field has its own state space: a player-placed crop field sits in state 0
// until a crop is chosen (measured: zero transitions), so an index-keyed trigger
// never fires for the very buildings this exists to fix.
//
// EnterBuiltState alone does not fix them: it delegates to LeaveBuilderProfession, which
// bails on `key == 0`. A field never ran State1Enter, so it never held the Builder key
// there is nothing to "leave". The natural profession therefore has to be seated
// directly from the description, which is exactly what StartTown_ActivateWorkplace
// already does for spawned-complete buildings (starttown.cpp) -- and why demo-spawned
// fields have working arrows while player-built ones do not.
//
// A house or storage barn keeps key 0: their WorkPlaceDescription+0x20 is legitimately
// zero, so the natural-seat branch skips them and the profession BST (the EMPLOYER list)
// stays correct.
extern "C" void WorkPlaceComponent_EnterBuiltState_Full(void* selfP) {
    if (!selfP) return;
    uintptr_t self = reinterpret_cast<uintptr_t>(selfP);

    // 1) The construction hand-off. No-op unless the Builder key is currently installed,
    //    so buildings that DID go through BuildComponent behave exactly as before.
    WorkPlaceComponent_EnterBuiltState(selfP);

    // 2) Never-constructed workplaces (crop field / orchard): seat the natural profession.
    if (*reinterpret_cast<uint64_t*>(self + 0x48) != 0) return;
    void* desc = *reinterpret_cast<void**>(self + 0x18);
    uint64_t natural = desc
        ? *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(desc) + 0x20) : 0;
    if (natural == 0) return;   // house / storage: correctly professionless

    *reinterpret_cast<uint64_t*>(self + 0x48) = natural;
    WorkPlaceComponent_SetAccepting(reinterpret_cast<int64_t>(selfP), true);

    if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(
            reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
        if (townMgr) {
            int64_t ownerRef = *reinterpret_cast<int64_t*>(self + 0x10);
            ResidenceComponent_impl_95a0(static_cast<int64_t>(townMgr), &ownerRef, natural);
        }
    }

    if (ProfKeyTrace())
        std::fprintf(stderr, "[ProfKey] SEATED NATURAL %-22s wpc=%p desc=%p +0x48=0x%llx (no construction hand-off ran)\n",
                     ProfKeyOwnerClass(self), selfP, desc, (unsigned long long)natural);
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

    // On load, resize the worker array to the count just read.
    //
    // ⚠ This tested `*(int*)(serializer + 8) == 0`, but IO::Serializer +0x08 is
    // m_encoding — a POINTER, never 0 — so the resize NEVER ran on either leg.
    // The loading flag lives on the STREAM (MemoryStream +0x08), which is how
    // ComponentEntity_Serialize and goals_serialize.cpp test it. With the resize
    // dead, +0x58 stayed null on load and the `workerArray` guard below silently
    // skipped reading `count` worker IBPs that the save had written — and each of
    // those pulls a worker's ENTITY inline, so the save wrote hundreds of bytes
    // the load never consumed. That desynced the stream for every later object:
    // a subsequent entity's component record was read off misaligned bytes,
    // handing InternalFactory::PersistObject a bogus objId (0xAAA12C08, identical
    // across saves because it is stream content, not heap garbage) which it used
    // to index its id map out of bounds -> ACCESS_VIOLATION on load.
    const uintptr_t serStream = *(uintptr_t*)serializer;
    const bool loadingLeg = serStream && (*(int*)(serStream + 8) == 0);
    if (loadingLeg) {
        if (s_fn_ResizeIBPArray) {
            s_fn_ResizeIBPArray((void*)((uintptr_t)self + 0x58), countBuf);
        }
    }

    // Serialize each worker InternalBasePtr and validate type
    void* workerArray = *(void**)((uintptr_t)self + 0x58);
    int64_t count = (int64_t)countBuf;
    // BANISHED_COMP_VALIDATE=1 — the worker loop is the prime suspect for the
    // save/load desync: each worker IBP can pull a whole entity inline, so a leg
    // that skips the loop consumes far fewer bytes than the leg that ran it.
    // Report BOTH arms (see reference_diagnostics_must_report_outcomes): the
    // `workerArray` guard silently turns a failed resize into a skipped read.
    {
        static int s_wpc = -1;
        if (s_wpc < 0) { const char* e = std::getenv("BANISHED_COMP_VALIDATE");
                         s_wpc = (e && e[0] && e[0] != '0') ? 1 : 0; }
        static int n = 0;
        if (s_wpc && n < 40) { ++n;
            fprintf(stderr, "[WPCSer] %s self=%p count=%d arr=%p -> %s\n",
                    loadingLeg ? "LOAD" : "SAVE",
                    (void*)self, (int)count, workerArray,
                    (count == 0) ? "no workers (nothing to read)"
                                 : (workerArray ? "LOOP RUNS"
                                                : "LOOP SKIPPED — ASYMMETRIC, "
                                                  "count>0 but array null"));
            fflush(stderr);
        }
    }
    if (count > 0 && workerArray) {
        uintptr_t ptr = (uintptr_t)workerArray;
        for (int64_t i = 0; i < count; i++) {
            if (s_fn_IBP_Serialize) {
                s_fn_IBP_Serialize((void*)ptr, serializer);
            }
            // Validate: if ptr dereferences to non-null, check IsDerivedFrom
            void* obj = *(void**)ptr;
            // s_TypeId_WorkerType must hold a CRC, not merely be a registered slot —
            // IsDerivedFrom(obj, 0) never matches and would null out every worker ref.
            if (obj != nullptr && s_fn_IsDerivedFrom_dispatch &&
                s_TypeId_WorkerType && *s_TypeId_WorkerType) {
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
#include "Component/controller_state_table.h"

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
// Slot 3 = Shutdown/Serialize (0x14012f120). The body has been ported in this file
// since the original wave (WorkPlaceComponent_Shutdown, above) but nothing ever wrote
// it into the vtable, so a loaded save's WorkPlaceComponent kept the FillTypedVtable
// base-Component serialize only — the worker roster (+0x58/+0x60), isAccepting
// (+0x50) and profession-key IBP (+0x48) never round-tripped. Wired unconditionally,
// same as slot 7 above (save/load wave 2026-08-27).
static void WorkPlaceComponent_ShutdownVSlot(void* self, void* stream) {
    WorkPlaceComponent_Shutdown(reinterpret_cast<WorkPlaceComponent*>(self), stream);
}
static void WorkPlaceComponent_FinishVtable() {
    WorkPlaceComponent_InertVtable[7] =
        reinterpret_cast<void*>(&WorkPlaceComponent_DetachVSlot);
    WorkPlaceComponent_InertVtable[3] =
        reinterpret_cast<void*>(&WorkPlaceComponent_ShutdownVSlot);
}

// ── LIVE controller (BANISHED_LIVE_WPC_CONTROLLER, opt-in until soak-verified) ──
// Retail wiring, Ghidra-verified against vtable 0x140164d30 + ctor 0x14012f2e0 + the
// state-table builder WorkPlaceDescription_helper_f3a0 (0x14012f3a0):
//   * GetDefaultState (slot 16, 0x140003730) returns 0.
//   * CreateState (slot 17, 0x140004080) returns the embedded node at +0x28; the ctor
//     seats the node back-pointer +0x40 = self and defaults accepting +0x50 = 1.
//   * States: 0 enter=ee40(->f520 "enter construction": become a Builder workplace,
//     re-key pending work via f8400, register in the Builder bucket, publish capacity,
//     SetAccepting) transitions {1,3}; 2 enter=edf0 ("built": leave Builder, re-key to
//     the natural profession, register under it) transitions {3}; 3 enter=ed60
//     ("demolish": release workers + BST-erase) transitions {6,4,5}.
// Promoting this retires the two stand-in hooks that collapsed the missing controller
// into call sites: BuildComponent_State8Finalize's explicit edf0 call, and the
// registration half of the StartTown_SpawnStructures workplace block (both now gated
// on WorkPlaceComponent_ControllerLive()).

// destroycomponent_workerrelease.cpp — faithful f520 port (0x14012f520).
extern "C" void WorkPlaceComponent_EnterBuilderProfession(int64_t wpc);

// First-N bring-up trace for the three controller enters (stderr, unconditional —
// these fire a handful of times per run and are the proof the SM dispatches them).
static void WPC_TraceEnter(const char* which, void* comp) {
    static int n = 0;
    if (n >= 24) return;
    ++n;
    fprintf(stderr, "[WPCCtrl] %s enter: comp=%p key(+0x48)=0x%llx roster(+0x60)=%d\n",
            which, comp,
            (unsigned long long)*reinterpret_cast<uint64_t*>(
                reinterpret_cast<uintptr_t>(comp) + 0x48),
            *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(comp) + 0x60));
    fflush(stderr);
}

static char WPC_EnterConstructionState(void* comp, void* /*sm*/) {   // ee40 -> f520
    WPC_TraceEnter("state-0 (construction/f520)", comp);
    WorkPlaceComponent_EnterBuilderProfession(reinterpret_cast<int64_t>(comp));
    return 0;
}
static char WPC_EnterBuiltStateNode(void* comp, void* /*sm*/) {      // edf0
    WPC_TraceEnter("state-2 (built/edf0)", comp);
    WorkPlaceComponent_EnterBuiltState(comp);
    return 0;
}
static char WPC_EnterDemolishState(void* comp, void* /*sm*/) {       // ed60
    // ed60 = ee60 (release every worker) + BST-erase on the +0x48 key. The ported
    // OnCreate body is the same pair in erase-first order; the halves touch disjoint
    // state (+0x48/BST vs the +0x58 roster), so the order is immaterial.
    WPC_TraceEnter("state-3 (demolish/ed60)", comp);
    WorkPlaceComponent_OnCreate(reinterpret_cast<WorkPlaceComponent*>(comp));
    return 0;
}

namespace { struct WPCCtrlTag {}; }
using WPCCtrlDispatch = ::ControllerLive::Dispatcher<WPCCtrlTag>;

static void WorkPlaceComponent_PopulateStateTable() {
    using namespace ::ControllerLive;
    StateTable& t = WPCCtrlDispatch::table;
    if (t.count != 0) return;  // idempotent
    TableRegisterState(t, 0, reinterpret_cast<void*>(&WPC_EnterConstructionState),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 1, nullptr);
    TableAddTransition(t, 3, nullptr);
    TableRegisterState(t, 2, reinterpret_cast<void*>(&WPC_EnterBuiltStateNode),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 3, nullptr);
    TableRegisterState(t, 3, reinterpret_cast<void*>(&WPC_EnterDemolishState),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 6, nullptr);
    TableAddTransition(t, 4, nullptr);
    TableAddTransition(t, 5, nullptr);
}

static int WorkPlaceComponent_GetDefaultState(void*) { return 0; }   // 0x140003730
static void* WorkPlaceComponent_CreateState(void* self) {            // 0x140004080
    return reinterpret_cast<uint8_t*>(self) + 0x28;
}

// Query used by the retired hook sites (buildcomponent_state8.cpp, starttown.cpp).
extern "C" bool WorkPlaceComponent_ControllerLive() {
    static int s = -1;
    if (s < 0)
        s = ::ComponentFactory::LiveEnabled("BANISHED_LIVE_WPC_CONTROLLER") ? 1 : 0;
    return s == 1;
}

// BANISHED_LIVE_WPC_BORN_ACCEPTING — DEFAULT-ON via kDefaultOnFlags (graduated
// 2026-07-31); the env var is now only a kill-switch (=0).
//
// Retail's WorkPlaceComponent_Ctor (0x14012f2e0) sets the accepting byte
// UNCONDITIONALLY, as a plain ctor-body default:
//     *(Internal **)(param_1 + 0x40) = param_1;
//     *(undefined8 *)(param_1 + 0x48) = 0;
//     param_1[0x50] = (Internal)0x1;      <-- born accepting, always
//     *(undefined8 *)(param_1 + 0x58) = 0;
//     *(undefined8 *)(param_1 + 0x60) = 0;
//
// Our port had that write INSIDE the controller-gated block below, so on the default
// path (BANISHED_LIVE_WPC_CONTROLLER is opt-in) it never ran and every workplace was
// born accepting=0. That is a faithfulness bug, not a controller feature: +0x50 has no
// dependency on the controller node — it was swept into the gate only because retail's
// ctor happens to set it adjacent to the +0x28/+0x40 controller-node fields.
//
// Symptom it produces: a sweep of a default start town finds every WPC with
// +0x40..+0x5F entirely zero (no back-pointer, no accepting byte) — the whole tail of
// the un-run block — and FindAvailableBuilding, which requires +0x50 != 0, can never
// return any building that did not get an explicit SetAccepting(true) from elsewhere.
//
// Graduated 2026-07-31 on a two-arm soak (release build, identical seed and settings,
// the town-health recipe minus the seed knobs that over-cap houses):
//   * the WPC census reports 8/8 constructions taking the ctor defaults, and the sweep
//     flips accepting=0 -> 8 / accepting=1 -> 8. Every workplace in the game is reached;
//     there is no second construction path (proven by the census cross-check, which
//     reported live-but-NOT-ours=0).
//   * both arms hold a stable town — 27 citizens (+0) at frames 1500 / 3000 / 4500 —
//     with zero guard trips and no new error pattern in the flag-on arm.
//   * both arms then die IDENTICALLY just past frame 4500: ACCESS_VIOLATION, READ of
//     0000000140152a90, same seven stack frames (NomadComp_FUN_140103480 <-
//     CitizenComponent_OnRemove <- MarkEntityRemoval <- HandleLifeEvent <-
//     CitizenComponent_Update). That is a PRE-EXISTING default-path crash on the
//     citizen-death teardown, present with this flag off, so — exactly as for
//     BANISHED_LIVE_DESTROY above — it is not a reason to hold this flag. It is
//     tracked separately; do not read this note as "the soak was clean".
//
// Blast radius at defaults is nil in a start town: all 8 workplaces carry profKey 0, so
// none is in a profession bucket and FindAvailableBuilding never reads their accepting
// byte. The flag matters for workplaces that reach a bucket without passing
// BuildComponent state-1 or StartTown_ActivateWorkplace — instant-built employers,
// force/harness-placed buildings, upgrade replacements, load-from-save entities.
static bool WorkPlaceComponent_BornAcceptingOn() {
    static int s = -1;
    if (s < 0)
        s = ::ComponentFactory::LiveEnabled("BANISHED_LIVE_WPC_BORN_ACCEPTING") ? 1 : 0;
    return s == 1;
}

// ── WPC construction census (UNCONDITIONAL, diagnostic only) ────────────────
// Every WorkPlaceComponent in the standalone exe is built by the two entry points
// at the bottom of this file (Factory / CreateInPlace) — there is no other path:
// ComponentEntity_CtorFromDescription (componententity.cpp:597) and the save-load
// path (:351) both go through Object::Factory::CreateObject -> the REGISTER_TYPE
// factoryFn, no code anywhere calls the retail ctor 0x14012f2e0 (unregistered and
// unmapped), and nothing else inserts into an entity's component array.
//
// Recording every construction makes that claim FALSIFIABLE at runtime instead of
// only by reading code: the sweep in standalone/trade_probe.cpp asks
// WorkPlaceComponent_WasFactoryBuilt() per swept object, so a workplace that our
// factory never produced shows up by name. It also removes the throttle that made
// "3 lines logged" indistinguishable from "3 to 24 constructions" — the ambiguity
// that sent this investigation looking for a construction path that doesn't exist.
static const int kWpcCensusCap = 256;
static void* s_wpcCensus[kWpcCensusCap];
static int   s_wpcCensusCount = 0;

extern "C" int WorkPlaceComponent_CensusCount() { return s_wpcCensusCount; }
extern "C" bool WorkPlaceComponent_WasFactoryBuilt(const void* p) {
    if (!p) return false;
    for (int i = 0; i < s_wpcCensusCount && i < kWpcCensusCap; ++i)
        if (s_wpcCensus[i] == p) return true;
    return false;
}
// True while the census is complete enough to answer WasFactoryBuilt negatively.
// Past the cap a "no" only means "not among the first kWpcCensusCap".
extern "C" bool WorkPlaceComponent_CensusComplete() {
    return s_wpcCensusCount <= kWpcCensusCap;
}

static void WorkPlaceComponent_FinishLiveVtable(void* mem) {
    WorkPlaceComponent_FinishVtable();   // slot-7 teardown patch stays unconditional

    if (mem) {
        if (s_wpcCensusCount < kWpcCensusCap) s_wpcCensus[s_wpcCensusCount] = mem;
        ++s_wpcCensusCount;
        if (s_wpcCensusCount == kWpcCensusCap + 1)
            std::fprintf(stderr, "[WPCCensus] census FULL (>%d) — WasFactoryBuilt can now "
                         "false-negative; raise kWpcCensusCap\n", kWpcCensusCap);
    }

    // Retail ctor-body default, independent of the controller node (see above).
    if (mem && WorkPlaceComponent_BornAcceptingOn()) {
        // +0x40 = self and +0x50 = 1 are BOTH plain ctor-body defaults in retail
        // (Ghidra 0x14012f2e0, verified 2026-07-30) and are set here together. Only
        // +0x50 has a known reader on the default path (FindAvailableBuilding's
        // accepting gate, town_workplace_registry.cpp:317); +0x40 is the controller
        // node's back-pointer and has no reader while +0x28 stays null, so it is
        // included for faithfulness and to stop "+0x40..+0x5F all zero" reading as
        // evidence of an un-run constructor. If run B regresses, +0x50 is the
        // behavioural half — split them before bisecting anything else.
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(mem) + 0x40) = mem;
        reinterpret_cast<uint8_t*>(mem)[0x50] = 1;
    }
    // Unthrottled: a run constructs a few dozen of these at most, and the throttle
    // is what made the previous run unreadable. Printed for BOTH flag states so
    // "flag on, nothing changed" can never be confused with "the write never ran".
    if (mem)
        std::fprintf(stderr, "[WPCCensus] #%d wpc=%p ctor-defaults %s (+0x40=%p +0x50=%d)\n",
                     s_wpcCensusCount, mem,
                     WorkPlaceComponent_BornAcceptingOn() ? "APPLIED" : "skipped (flag off)",
                     *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(mem) + 0x40),
                     (int)reinterpret_cast<uint8_t*>(mem)[0x50]);
    std::fflush(stderr);

    if (!WorkPlaceComponent_ControllerLive()) return;
    WorkPlaceComponent_InertVtable[16] =
        reinterpret_cast<void*>(&WorkPlaceComponent_GetDefaultState);
    WorkPlaceComponent_InertVtable[17] =
        reinterpret_cast<void*>(&WorkPlaceComponent_CreateState);
    WorkPlaceComponent_PopulateStateTable();
    if (mem) {
        // Embedded controller node (0x14012f2e0): node vtable at +0x28, parent/child
        // zeroed, back-pointer +0x40 = self. The accepting byte +0x50 is NOT part of
        // this — it is a plain ctor default and now lives above, outside the gate.
        uint8_t* b = reinterpret_cast<uint8_t*>(mem);
        *reinterpret_cast<void**>(b + 0x28) =
            reinterpret_cast<void*>(WPCCtrlDispatch::Vtable());
        *reinterpret_cast<uint64_t*>(b + 0x30) = 0;
        *reinterpret_cast<uint64_t*>(b + 0x38) = 0;
        *reinterpret_cast<void**>(b + 0x40) = mem;
        b[0x50] = 1;   // controller path keeps its own write (idempotent with the above)
    }
}

extern "C" void* WorkPlaceComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x68, 0x82,
        WorkPlaceComponent_InertVtable,
        reinterpret_cast<void*>(&WorkPlaceComponent_InertGetTypeId));
    WorkPlaceComponent_FinishLiveVtable(p);
    return p;
}
static void* WorkPlaceComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x68, 0x82,
        WorkPlaceComponent_InertVtable,
        reinterpret_cast<void*>(&WorkPlaceComponent_InertGetTypeId));
    WorkPlaceComponent_FinishLiveVtable(mem);
    return mem;
}
REGISTER_TYPE(WorkPlaceComponent, "ControllerComponent",
              &WorkPlaceComponent_Factory, &WorkPlaceComponent_CreateInPlace);
