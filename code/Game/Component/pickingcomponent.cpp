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
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/pickingcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

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
// and is only exercised from the SA start-town spawn — the DLL build uses the real
// hooked picking path. Compile it standalone-only.
#ifdef STANDALONE_EXE
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
    if (!entity || !fn_InternalBasePtr_CheckPtr) return nullptr;
    uint32_t tid = PC_ModelTypeId();
    if (tid == 0) return nullptr;
    uint8_t* e = (uint8_t*)entity;
    int count = *(int32_t*)(e + 0x38);
    uintptr_t arr = *(uintptr_t*)(e + 0x30);
    for (int i = 0; i < count; ++i) {
        void** slot = (void**)(arr + (uintptr_t)i * 8);
        fn_InternalBasePtr_CheckPtr(slot);
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
    if (!s_fn_AddMesh || !fn_InternalBasePtr_CheckPtr) return;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;

    uint8_t* p = (uint8_t*)pc;

    // --- Startup (0x140128c30): userData(+0x30) = owner(+0x10). Also restore the
    //     unregistered sentinel +0x28 = -1 (SA leaves it a zeroed 0, not -1).
    fn_InternalBasePtr_CheckPtr((void*)(p + 0x10));
    void* entity = *(void**)(p + 0x10);           // resolved ComponentEntity ptr
    if (!entity) return;
    *(void**)(p + 0x30) = entity;                 // userData = owner entity
    // NB: retail OnAdded only registers when pickingId(+0x28) < 0. In SA +0x28 is a
    // zeroed 0 (Startup/OnAdded never ran), NOT the -1 unregistered sentinel, so we
    // register unconditionally here — we drive this exactly once per building at spawn.

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

    uint32_t meshId = s_fn_AddMesh(scene, mesh, xform, entity, 0, 0, nullptr);
    *(int32_t*)(p + 0x28) = (int32_t)meshId;      // store pickingId

    static int s_logged = 0;
    if (s_logged < 12) { s_logged++;
        fprintf(stderr, "[PickMesh] AddMesh entity=%p mesh=%p userData=%p -> meshId=%u\n",
                entity, mesh, entity, meshId);
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
#endif // STANDALONE_EXE

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

    // Picking::Scene::RemoveMesh — dispatch slot at 0x140152438
    slot = ResolveAddr(Module::Exe, 0x140152438);
    if (slot) s_fn_RemoveMesh = *reinterpret_cast<fn_RemoveMesh_t*>(slot);

    s_fn_IBP_Serialize = reinterpret_cast<fn_IBP_Serialize_t>(
        ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z"));

    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(PickingComponent, 0x38, 0xC3)
REGISTER_TYPE(PickingComponent, "Component",
              &PickingComponent_Factory, &PickingComponent_CreateInPlace);
#endif // STANDALONE_EXE
