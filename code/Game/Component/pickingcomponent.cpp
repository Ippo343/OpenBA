// code/Game/Component/pickingcomponent.cpp
// Clean reimplementation of PickingComponent lifecycle methods from Application-x64.exe.
//
// PickingComponent (0x38 bytes) — manages 3D picking (mouse -> entity selection).
// Fields: +0x28=pickingId (int32, -1=unregistered), +0x30=entity ref (IBP)
//
// Vtable at 0x140164040. Two hookable overrides:
//   Shutdown (0x140128bb0) — Serialize IBP + IsDerivedFrom validation
//   OnCreate (0x140128b50) — Remove picking mesh via Scene dispatch
//

#include "Component/pickingcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// Object::InternalBasePtr::Serialize
typedef void (*fn_IBP_Serialize_t)(void* ibp, void* serializer);
static fn_IBP_Serialize_t s_fn_IBP_Serialize = nullptr;

// IsDerivedFrom
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_d = nullptr;

// Picking::Scene::RemoveMesh — dispatched through [0x140152438]
typedef void (*fn_RemoveMesh_t)(void* scene, uint32_t meshId);
static fn_RemoveMesh_t s_fn_RemoveMesh = nullptr;

static uint32_t* s_pComponentEntityTypeId = nullptr;

static bool s_initialized = false;

// ============================================================
// PickingComponent::Shutdown (0x140128bb0)
// ============================================================
void PickingComponent_Shutdown(PickingComponent* self, void* stream) {
    // resolver call added at top, save/load wave 2026-08-28: Shutdown never resolved
    // its own helpers (only DetachVSlot/RegisterMesh did), so on a load-from-save path
    // where Shutdown fires first every s_fn_* below could still be null.
    PickingComponent_InitFuncs();
    uintptr_t selfAddr = (uintptr_t)self;

    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer, stream, 0);

    // Serialize IBP at +0x30
    if (s_fn_IBP_Serialize)
        s_fn_IBP_Serialize((void*)(selfAddr + 0x30), serializer);

    // Validate: if not ComponentEntity, null it out
    void* ibpVal = *(void**)(selfAddr + 0x30);
    if (ibpVal != nullptr && s_fn_IsDerivedFrom_d && s_pComponentEntityTypeId) {
        if (!s_fn_IsDerivedFrom_d(ibpVal, *s_pComponentEntityTypeId)) {
            *(uintptr_t*)(selfAddr + 0x30) = 0;
        }
    }

    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// PickingComponent::OnCreate (0x140128b50)
// ============================================================
// If pickingId >= 0, calls Picking::Scene::RemoveMesh and resets to -1.

void PickingComponent_OnCreate(PickingComponent* self) {
    if ((int)self->pickingId >= 0) {
        if (s_fn_RemoveMesh && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
            void* pickingScene = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x70);
            s_fn_RemoveMesh(pickingScene, (uint32_t)self->pickingId);
        }
        self->pickingId = -1;
    }
}

// ============================================================
// PickingComponent::OnAdded (0x140128f50) — register the picking mesh
// ============================================================
// Faithful port of the exe's picking-mesh (re)build routine. It is NOT a vtable
// slot the standalone component-add path invokes (retail calls it from vtable +0x30
// / +0x58 and the BuildComponent helpers, all of which are inert/stubbed in SA), so
// meshes never enter Picking::Scene and mesh-precise picking is dead — only the
// coarse terrain-cell fallback works, which is why buildings are "click-the-middle"
// picky. We drive this directly at spawn for the (static) start-town buildings.
//
// The probe (BANISHED_PICKMESH_PROBE, starttown.cpp) established the SA ground truth:
//   - Picking::Scene (gw+0x70) IS constructed.
//   - Building entities carry a real PickingComponent with +0x18 pick-description set
//     (and desc+0x20 mesh table populated).
//   - But +0x30 (userData) is null and +0x28 (pickingId) is an uninitialized 0
//     (should be -1) — Startup/OnAdded never ran.
// So we replicate Startup (+0x30 = +0x10 owner, +0x28 = -1) then OnAdded (AddMesh).

// This registration path depends on the standalone type registry (GetTypeIdByName)
// and is only exercised from the SA start-town spawn.
namespace Object { uint32_t GetTypeIdByName(const char* name); }

// Picking::Scene::AddMesh — member fn (this=RCX): RDX=mesh ExternalPtr, R8=&Transform,
// R9=userData, then 3 trailing args (int,int,cb) all 0. Returns uint32 meshId.
typedef uint32_t (*fn_PickingAddMesh_t)(void* scene, void* meshExtPtr, const void* xform,
                                        void* userData, int a, int b, void* cb);
static fn_PickingAddMesh_t s_fn_AddMesh = nullptr;
static uint32_t s_tidModelComponent = 0;
static bool s_addMeshInit = false;

// forward decl
namespace { uint32_t PC_ModelTypeId(); }

// Self-contained ModelComponent find over the entity's component array (entity+0x30
// array ptr / +0x38 count; each slot is an IBP). Mirrors ComponentEntity::FindComponent
// (the mislabeled Entity_ClearPathQueue, 0x140003f60).
static void* PC_FindModelComponent(void* entity) {
    if (!entity) return nullptr;
    uint32_t tid = PC_ModelTypeId();
    if (tid == 0) return nullptr;
    uint8_t* e = (uint8_t*)entity;
    int count = *(int32_t*)(e + 0x38);
    uintptr_t arr = *(uintptr_t*)(e + 0x30);
    for (int i = 0; i < count; ++i) {
        void** slot = (void**)(arr + (uintptr_t)i * 8);
        void* comp = *slot;
        if (!comp) continue;
        uintptr_t vt = *(uintptr_t*)comp;
        typedef uint32_t (*gt_t)(void*);
        if ((*(gt_t*)(vt + 0x10))(comp) == tid) return comp;
    }
    return nullptr;
}


void PickingComponent_RegisterMesh(void* pc) {
    if (!pc) return;
    PickingComponent_InitFuncs();
    if (!s_addMeshInit) {
        s_fn_AddMesh = (fn_PickingAddMesh_t)ResolveExport(
            "?AddMesh@Scene@Picking@@QEAAIV?$ExternalPtr@VPickingMesh@Picking@@@Object@@"
            "AEBVTransform@Math@@PEAXHHP6AX2HHAEBVVector3@6@33@Z@Z");
        s_addMeshInit = true;
    }
    if (!s_fn_AddMesh) return;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;

    uint8_t* p = (uint8_t*)pc;

    // --- Startup (0x140128c30): userData(+0x30) = owner(+0x10). Also restore the
    //     unregistered sentinel +0x28 = -1 (SA leaves it a zeroed 0, not -1).
    void* entity = *(void**)(p + 0x10);           // resolved ComponentEntity ptr
    if (!entity) return;
    // Retail SPLITS what this function fuses: Startup seats +0x30 = owner, and OnAdded
    // (which runs much later, from EntityList::RegisterEntity) passes whatever +0x30
    // holds as the pick userData. BridgeTool::CalcPath uses that window — it re-points
    // each deck segment's +0x30 at the MASTER bridge entity so clicking any piece
    // selects the whole bridge. Fusing Startup+OnAdded here must not undo that, so only
    // default +0x30 when nothing has claimed it.
    void* userData = *(void**)(p + 0x30);
    if (!userData) { userData = entity; *(void**)(p + 0x30) = entity; }
    // Retail's OnAdded (0x140128f50) is a REFRESH, not a one-shot. Its very first act is to
    // call its OWN vtable +0x38 — PickingComponent::OnCreate, i.e. "if pickingId >= 0,
    // Picking::Scene::RemoveMesh it and reset to -1" — and only then does it re-register
    // under `modelVariant >= 0 && pickingId < 0`. So the guard is always satisfied by the
    // teardown that precedes it; it is not a "skip if already registered" test.
    //
    // That matters because the pick mesh must FOLLOW the model's frame: retail re-invokes
    // this from BuildComponent's sub_8720 (CALL 0x140128f50 at 0x140108b26) as a site walks
    // its construction frames, so the pickable volume tracks what is actually drawn. An
    // earlier version of this port guarded with a bare `if (pickingId >= 0) return;`, which
    // pinned the mesh to whichever frame was current at OnAddedPass time (the small
    // foundation sub-mesh) and made every later refresh a no-op — measured as a placed
    // building whose clickable area stayed a fraction of the building.
    //
    // Doing the teardown here also subsumes the double-registration concern that motivated
    // the bad guard: a second call removes the first mesh before adding its own.
    PickingComponent_OnCreate(reinterpret_cast<PickingComponent*>(pc));
    if (*(int32_t*)(p + 0x28) >= 0) return;   // retail: `(int)param_1[5] < 0`

    // --- OnAdded body (0x140128f50) ---
    void* mc = PC_FindModelComponent(entity);     // sibling ModelComponent
    if (!mc) return;
    uint8_t* m = (uint8_t*)mc;
    int16_t variantIdx   = *(int16_t*)(m + 0x164);
    int16_t modelVariant = *(int16_t*)(m + 0x166);
    if (modelVariant < 0) return;                 // exe guard: -1 < modelVariant

    // Transform (0x1C bytes) copied from ModelComponent+0x148.
    uint8_t xform[0x1C];
    for (int i = 0; i < 0x1C; ++i) xform[i] = *(m + 0x148 + i);

    // Mesh = pickDesc(+0x18)->table(+0x20)[variantIdx][modelVariant]. Retail validates
    // pickDesc IsDerivedFrom(DAT_1401c7c48) first; the probe proved +0x18 is a valid
    // pick-description with a populated +0x20 table, so we trust the component's own
    // field and null-guard each deref instead of pulling in that type id.
    void* pickDesc = *(void**)(p + 0x18);
    if (!pickDesc) return;
    void* tableBase = *(void**)((uint8_t*)pickDesc + 0x20);
    if (!tableBase) return;
    void* row = *(void**)((uint8_t*)tableBase + (int)variantIdx * 0x10);
    if (!row) return;
    void* mesh = *(void**)((uint8_t*)row + (int)modelVariant * 8);
    if (!mesh) return;

    void* scene = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x70);
    if (!scene) return;

    uint32_t meshId = s_fn_AddMesh(scene, mesh, xform, userData, 0, 0, nullptr);
    *(int32_t*)(p + 0x28) = (int32_t)meshId;      // store pickingId

    static int s_logged = 0;
    if (s_logged < 64) { s_logged++;
        // Dump the transform we hand to AddMesh as floats, next to the owner's MapComponent
        // footprint rect. Measured 2026-07-28: a placed building's pickable region sat at the
        // FAR CORNER of its footprint (hits only at (105,12x) for a rect anchored at
        // (102,122) 4x5) while the model drew around the centre — i.e. the mesh is displaced,
        // not undersized. This line is how you tell "wrong transform" from "small mesh":
        // compare xform's translation against the rect.
        const float* f = reinterpret_cast<const float*>(xform);
        int fx = -1, fy = -1, fw = -1, fh = -1;
        {   // owner's MapComponent, for the footprint rect
            static uint32_t s_tidMapPC = 0;
            if (!s_tidMapPC) s_tidMapPC = ::Object::GetTypeIdByName("MapComponent");
            uint8_t* e = (uint8_t*)entity;
            int n = *(int32_t*)(e + 0x38);
            uintptr_t arr = *(uintptr_t*)(e + 0x30);
            for (int i = 0; i < n && arr; ++i) {
                void** slot = (void**)(arr + (uintptr_t)i * 8);
                void* c = *slot; if (!c) continue;
                typedef uint32_t (*gt_t)(void*);
                if ((*(gt_t*)(*(uintptr_t*)c + 0x10))(c) == s_tidMapPC) {
                    uint8_t* m2 = (uint8_t*)c;
                    fx = *(int16_t*)(m2 + 0x28); fy = *(int16_t*)(m2 + 0x2a);
                    fw = *(int16_t*)(m2 + 0x2c); fh = *(int16_t*)(m2 + 0x2e);
                    break;
                }
            }
        }
        fprintf(stderr, "[PickMesh] AddMesh entity=%p mesh=%p userData=%p -> meshId=%u | "
                "variant=(%d,%d) xform[0..6]=%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f | "
                "footprint=(%d,%d %dx%d) => cellFromXform=(%.1f,%.1f)\n",
                entity, mesh, userData, meshId, (int)variantIdx, (int)modelVariant,
                f[0], f[1], f[2], f[3], f[4], f[5], f[6], fx, fy, fw, fh,
                f[4] + 128.0f, f[5] + 128.0f);
        fflush(stderr);
    }
}

namespace {
uint32_t PC_ModelTypeId() {
    if (s_tidModelComponent == 0 || s_tidModelComponent == 0xFFFFFFFF)
        s_tidModelComponent = ::Object::GetTypeIdByName("ModelComponent");
    return s_tidModelComponent;
}
}

// Entity-level convenience: find the entity's PickingComponent and register its mesh.
// Used by callers that hold an entity rather than the component (player placement, etc.).
static uint32_t s_tidPickComponent = 0;
void PickingComponent_RegisterMeshForEntity(void* entity) {
    if (!entity) return;
    if (s_tidPickComponent == 0 || s_tidPickComponent == 0xFFFFFFFF)
        s_tidPickComponent = ::Object::GetTypeIdByName("PickingComponent");
    if (s_tidPickComponent == 0) return;
    uint8_t* e = (uint8_t*)entity;
    int count = *(int32_t*)(e + 0x38);
    uintptr_t arr = *(uintptr_t*)(e + 0x30);
    for (int i = 0; i < count; ++i) {
        void** slot = (void**)(arr + (uintptr_t)i * 8);
        void* comp = *slot;
        if (!comp) continue;
        uintptr_t vt = *(uintptr_t*)comp;
        typedef uint32_t (*gt_t)(void*);
        if ((*(gt_t*)(vt + 0x10))(comp) == s_tidPickComponent) {
            PickingComponent_RegisterMesh(comp);
            return;
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void PickingComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_pComponentEntityTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c72e8));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_d = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    // Picking::Scene::RemoveMesh. The exe dispatch slot 0x140152438 is NOT registered
    // in standalone, so ResolveAddr(Exe, slot) returned 0 and s_fn_RemoveMesh stayed
    // null — OnCreate then reset pickingId WITHOUT removing the mesh, so a removed
    // building's pick mesh dangled in Picking::Scene with userData=freed entity and the
    // foundation click crashed in EditTool_SelectEntity_Impl (2026-07-21). Resolve the
    // Runtime export directly instead (verified 0x180125290 = Picking::Scene::RemoveMesh
    // (Scene*, uint) — the pair of the AddMesh 0x1801252e0 that RegisterMesh uses; the
    // other RemoveMesh 0x18010c3c0 is Collision::Scene, wrong one).
    s_fn_RemoveMesh = reinterpret_cast<fn_RemoveMesh_t>(
        ResolveAddr(Module::Runtime, 0x180125290));
    if (!s_fn_RemoveMesh) {  // fallback: exe dispatch slot, if it ever gets registered
        slot = ResolveAddr(Module::Exe, 0x140152438);
        if (slot) s_fn_RemoveMesh = *reinterpret_cast<fn_RemoveMesh_t*>(slot);
    }

    s_fn_IBP_Serialize = reinterpret_cast<fn_IBP_Serialize_t>(
        ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z"));

    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// Hand-expanded from COMPONENT_INERT_FACTORY(PickingComponent, 0x38, 0xC3) so the
// per-type vtable's +0x38 slot (index 7 = OnRemovedFromWorld, retail 0x140128b50 =
// PickingComponent_OnCreate; verified as vtable 0x140164040[+0x38]) routes to the
// mesh teardown. Retail: entity mark-removal FUN_1400c2ea0 broadcasts vtable+0x38 to
// every component whose capability flags (+0x20) have bit 1 (PickingComponent 0xC3
// qualifies) BEFORE the entity is freed; OnCreate removes the picking mesh from
// Picking::Scene (gw+0x70) and resets pickingId to -1. Without this the removed/
// upgraded building's mesh dangled in the scene with userData = the freed entity, so
// EditTool_SelectEntity_Impl's mesh pick (Picking::Scene::GetUserData) returned the
// freed pointer and crashed when the now-empty foundation was clicked (2026-07-21).
// This is the Picking-scene twin of the map-cell-occupancy fix (MapComponent +
// starttown.cpp StartTown_MarkBuildingCells).
static uint32_t PickingComponent_InertGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("PickingComponent");
    return cached;
}
static void* PickingComponent_InertVtable[::ComponentFactory::kInertVtableSlots];
static void PickingComponent_DetachVSlot(void* s) {
    PickingComponent_InitFuncs();   // ensure s_fn_RemoveMesh (dispatch 0x140152438) resolved
    PickingComponent_OnCreate(reinterpret_cast<PickingComponent*>(s));
}
// Slot 6 (+0x30) = OnAdded, retail 0x140128f50 — dispatched by ComponentEntity_OnAddedPass
// at EntityList_RegisterEntity time, in COMPONENT ARRAY ORDER. That order is the whole
// point: on a WoodHouse, Picking is index [5] and Build is index [16], so retail registers
// the pick mesh while the model still carries the FOUNDATION decal (modelVariant 0) and
// only then does BuildComponent's own OnAdded Stage 3 un-frame it to -1.
//
// Standalone left this slot a stub and compensated with explicit
// PickingComponent_RegisterMeshForEntity calls at each creation site — but the placement
// site calls it AFTER RegisterEntity, i.e. after index [16] has already stamped -1, so
// `modelVariant < 0` bails and a hand-placed building registers no pick mesh at all.
// Measured 2026-07-28: that is why a placed foundation could not be clicked. Wiring the
// slot puts the registration back in its retail window and retires the workarounds.
static void PickingComponent_OnAddedVSlot(void* s) {
    PickingComponent_RegisterMesh(s);
}
static void PickingComponent_SerVSlot(void* s, void* stream) {
    PickingComponent_Shutdown(reinterpret_cast<PickingComponent*>(s), stream);
}
static void PickingComponent_FinishVtable() {
    PickingComponent_InertVtable[6] =
        reinterpret_cast<void*>(&PickingComponent_OnAddedVSlot);
    PickingComponent_InertVtable[7] =
        reinterpret_cast<void*>(&PickingComponent_DetachVSlot);
    // slot 3 wired unconditionally, save/load wave 2026-08-28 (fields previously reset silently on load).
    PickingComponent_InertVtable[3] =
        reinterpret_cast<void*>(&PickingComponent_SerVSlot);
}
// Retail Startup (0x140128c30) stamps pickingId(+0x28) = -1 so OnCreate's
// `pickingId >= 0` guard skips the RemoveMesh for a component that never registered a
// mesh. The inert factory zero-fills the body (pickingId = 0, which would wrongly
// RemoveMesh(0)), so replicate that -1 init per instance. RegisterMesh overwrites it
// with the real meshId when a mesh is actually added.
static inline void PickingComponent_InitBody(void* inst) {
    if (inst) *(int32_t*)((uint8_t*)inst + 0x28) = -1;
}
extern "C" void* PickingComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x38, 0xC3,
        PickingComponent_InertVtable,
        reinterpret_cast<void*>(&PickingComponent_InertGetTypeId));
    PickingComponent_FinishVtable();
    PickingComponent_InitBody(p);
    return p;
}
static void* PickingComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x38, 0xC3,
        PickingComponent_InertVtable,
        reinterpret_cast<void*>(&PickingComponent_InertGetTypeId));
    PickingComponent_FinishVtable();
    PickingComponent_InitBody(mem);
    return mem;
}
REGISTER_TYPE(PickingComponent, "Component",
              &PickingComponent_Factory, &PickingComponent_CreateInPlace);
