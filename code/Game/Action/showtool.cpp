// code/Game/Action/showtool.cpp
// Clean C++ reimplementation of ShowTool action methods.
// Source: Game/Action/ShowTool.cpp (from profile build debug strings)
//
// ShowTool -- 0x30 bytes. Shows a tool UI for a citizen.
// Inherits: Object::Internal -> Action (0x20) -> ShowTool
// Vtable at 0x1401589b0 (10 slots).
//
// Decompiled from Application-x64.exe using Ghidra.

#include "ShowTool.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "math/Math.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>

#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (SA type-id fallback)

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h -- ShowTool-specific helpers)
// ============================================================

// Object::InternalBasePtr::CheckPtr -- via IAT at 0x140152ab0
static void (*s_fn_CheckPtr)(void* ibp) = nullptr;

// Object::InternalBasePtr::Serialize -- via export
static void (*s_fn_IBP_Serialize)(void* ibp, void* serializer) = nullptr;

// FUN_1401176e0 -- hide tool on entity
static void (*s_fn_HideTool)(void* entity) = nullptr;

// FUN_1401173b0 -- show tool with specific ID on entity
static void (*s_fn_ShowToolOnEntity)(void* entity, int toolId) = nullptr;

// DAT_1401c7158 -- TypeInfo for entity type check
static uint32_t* s_TypeInfo_ShowToolEntity = nullptr;

// ---- Carry-model path (ShowTool with toolId < 0) --------------------------------
// The retail show function (Entity_impl_73b0) reads the carried resource's mesh from
// its ModelDescription (DAT_1401c7a88) and its carry-scale from its RawMaterial-
// Description (DWORD_1401c7cb8). Both type-ids are read straight from exe .rdata, so
// in standalone (where those slots are null) 73b0 never matches the mesh component and
// null-derefs. This is a faithful C++ re-port of 73b0's toolId<0 branch that resolves
// the type-ids from the runtime registry in standalone and guards the two component
// lookups, so a material template lacking either component simply shows no model
// instead of crashing. Verified 1:1 against the Ghidra decompile + disassembly of
// 0x1401173b0; dispatch-slot identities confirmed via Ghidra external xrefs.
// Math::Vector4 splat ctor (slot 0x140152958, ??0Vector4@Math@@QEAA@AEBVScalar@1@@Z) is a
// direct member of the real class (B.3.1).
typedef int          (*fn_GetNodeIndex_t)(void* graphicsMesh, const char* name); // 0x140152970
typedef void         (*fn_GeomAttach_t)(void* geom, void* attach, int nodeIdx);  // 0x140152968
typedef void         (*fn_MeshSetMesh_t)(void* meshInstance, void* mesh);        // 0x140151498
typedef unsigned int (*fn_SceneAdd_t)(void* scene, void* geom);                  // 0x140151370
typedef void         (*fn_MIDefCtor_t)(void* meshInstance);                      // 0x140151490

static fn_GetNodeIndex_t s_slot_GetNodeIndex = nullptr;
static fn_GeomAttach_t   s_slot_GeomAttach = nullptr;
static fn_MeshSetMesh_t  s_slot_SetMesh = nullptr;
static fn_SceneAdd_t     s_slot_SceneAdd = nullptr;
static fn_MIDefCtor_t    s_slot_MI_DefCtor = nullptr;

// ModelDescription type-id (DAT_1401c7a88): .rdata resolves NULL in standalone, so fall back to the runtime registry.
static uint32_t ShowTool_ModelDescTid() {
    uint32_t* p = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7a88));
    if (p && *p && *p != 0xFFFFFFFF) return *p;
    static uint32_t c = 0;
    if (!c || c == 0xFFFFFFFF) c = ::Object::GetTypeIdByName("ModelDescription");
    if (c != 0xFFFFFFFF) return c;
    return 0;
}
// RawMaterialDescription type-id (DWORD_1401c7cb8 / g_TypeId_RawMaterialDescription).
static uint32_t ShowTool_RawMatTid() {
    if (g_TypeId_RawMaterialDescription && *g_TypeId_RawMaterialDescription
        && *g_TypeId_RawMaterialDescription != 0xFFFFFFFF)
        return *g_TypeId_RawMaterialDescription;
    uint32_t* p = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7cb8));
    if (p && *p && *p != 0xFFFFFFFF) return *p;
    static uint32_t c = 0;
    if (!c || c == 0xFFFFFFFF) c = ::Object::GetTypeIdByName("RawMaterialDescription");
    if (c != 0xFFFFFFFF) return c;
    return 0;
}

// Scan an entity's component list (count@+0x30, IBP array@+0x28) for the first
// component whose runtime type-id (vtable+0x10 == GetTypeId) equals `tid`. Faithful to
// the two scan loops inside 73b0. Returns null if not found (SA-safe).
static void* ShowTool_FindComponentByTid(void* hostP, uint32_t tid) {
    if (!hostP || !tid) return nullptr;
    uint8_t* host = reinterpret_cast<uint8_t*>(hostP);
    int count = *reinterpret_cast<int*>(host + 0x30);
    int64_t arr = *reinterpret_cast<int64_t*>(host + 0x28);
    for (int i = 0; i < count; i++) {
        void* comp = *reinterpret_cast<void**>(arr + (int64_t)i * 8);
        if (!comp) continue;
        int64_t vt = *reinterpret_cast<int64_t*>(comp);
        typedef uint32_t (*getType_t)(void*);
        uint32_t got = (*reinterpret_cast<getType_t*>(vt + 0x10))(comp);
        if (got == tid)
            return fn_IsDerivedFrom(comp, tid) ? comp : nullptr;
    }
    return nullptr;
}

static bool s_showtool_funcs_resolved = false;

// ============================================================
// Resolution -- called once from Install
// ============================================================
bool ShowTool_ResolveFuncs() {
    if (s_showtool_funcs_resolved) return true;

    // CheckPtr via IAT at [0x140152ab0]
    uintptr_t iatEntry = ResolveAddr(Module::Exe, 0x140152ab0);
    if (iatEntry) s_fn_CheckPtr = *reinterpret_cast<void(**)(void*)>(iatEntry);

    s_TypeInfo_ShowToolEntity = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c7158);
    // DAT_1401c7158 is the CitizenComponent type-id (verified: CitizenComponent_
    // RegisterType writes it). In standalone that raw exe .rdata slot is null, so
    // resolve the id from the runtime registry and hand out a pointer to our own
    // storage. This is used ONLY by ShowTool::Serialize's entity type-check — the
    // carry path never touches it — but it's in the `ok` gate below, so leaving it
    // null makes ShowTool_ResolveFuncs fail and AbstractImpl bail before ever
    // running ShowCarried (the held-item model never appears).
    if (!s_TypeInfo_ShowToolEntity || !*s_TypeInfo_ShowToolEntity
        || *s_TypeInfo_ShowToolEntity == 0xFFFFFFFF) {
        static uint32_t s_citizenTid = 0;
        uint32_t tid = ::Object::GetTypeIdByName("CitizenComponent");
        if (tid && tid != 0xFFFFFFFF) {
            s_citizenTid = tid;
            s_TypeInfo_ShowToolEntity = &s_citizenTid;
        }
    }
    s_fn_HideTool = reinterpret_cast<void(*)(void*)>(
        ResolveAddr(Module::Exe, 0x1401176e0));
    s_fn_ShowToolOnEntity = reinterpret_cast<void(*)(void*, int)>(
        ResolveAddr(Module::Exe, 0x1401173b0));
    s_fn_IBP_Serialize = reinterpret_cast<void(*)(void*, void*)>(
        ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z"));

    // Carry-model dispatch slots (values filled by the runtime into exe .rdata slots;
    // resolve the slot address, then deref to the fn ptr). Optional for the core action
    // methods — each is null-guarded at the call site in ShowTool_ShowCarried — so they
    // are NOT part of the `ok` gate below.
    auto rslot = [](uintptr_t a) -> void* {
        uintptr_t s = ResolveAddr(Module::Exe, a);
        return s ? *reinterpret_cast<void**>(s) : nullptr;
    };
    s_slot_GetNodeIndex = reinterpret_cast<fn_GetNodeIndex_t>(rslot(0x140152970));
    s_slot_GeomAttach   = reinterpret_cast<fn_GeomAttach_t>(rslot(0x140152968));
    s_slot_SetMesh      = reinterpret_cast<fn_MeshSetMesh_t>(rslot(0x140151498));
    s_slot_SceneAdd     = reinterpret_cast<fn_SceneAdd_t>(rslot(0x140151370));
    s_slot_MI_DefCtor   = reinterpret_cast<fn_MIDefCtor_t>(rslot(0x140151490));
    // The GetNodeIndexFromName / Geometry::Attach dispatch slots are NOT registered in
    // standalone (the exe .rdata slot resolves to 0), so fall back to the Runtime-x64.dll
    // functions directly (ASLR handled by ResolveAddr). Without these the held-item mesh
    // could not be parented to the character's "tool" bone.
    if (!s_slot_GetNodeIndex)
        s_slot_GetNodeIndex = reinterpret_cast<fn_GetNodeIndex_t>(ResolveAddr(Module::Runtime, 0x1800fee90));
    if (!s_slot_GeomAttach)
        s_slot_GeomAttach = reinterpret_cast<fn_GeomAttach_t>(ResolveAddr(Module::Runtime, 0x1800a3fa0));

    bool ok = s_fn_CheckPtr && s_TypeInfo_ShowToolEntity &&
              s_fn_HideTool && s_fn_ShowToolOnEntity && s_fn_IBP_Serialize &&
              g_fn_Serializer_Ctor && g_fn_Serializer_Dtor &&
              g_fn_Serialize_Bool && g_fn_Serialize_Int &&
              fn_IsDerivedFrom;

#ifdef _DEBUG
    printf("[ShowTool] ResolveFuncs: CheckPtr=%p HideTool=%p ShowTool=%p TypeInfo=%p\n",
           (void*)s_fn_CheckPtr, (void*)s_fn_HideTool,
           (void*)s_fn_ShowToolOnEntity, (void*)s_TypeInfo_ShowToolEntity);
    printf("[ShowTool]   IBPSer=%p\n", (void*)s_fn_IBP_Serialize);
    if (!ok) printf("[ShowTool] ERROR: some function pointers not resolved!\n");
#endif

    s_showtool_funcs_resolved = ok;
    return ok;
}

// ============================================================
// ShowTool::Serialize (0x140042780)
// Serializes: status(+0x18), entity ptr(+0x20) with type check, int(+0x28)
// ============================================================
void ShowTool_Serialize_Impl(void* self, void* stream) {
    // Reached through the registered vtable, which the factory path never
    // has to touch — an exe-ctor-built action arrives here with the file
    // statics still null. Lazy-resolve like the other entry points do.
    ShowTool_ResolveFuncs();
    uint8_t* s = (uint8_t*)self;
    uint8_t ser1[24];
    uint8_t ser2[24];

    g_fn_Serializer_Ctor(ser1, stream, 0);
    g_fn_Serialize_Bool(ser1, s + 0x18, nullptr);
    g_fn_Serializer_Dtor(ser1);

    g_fn_Serializer_Ctor(ser2, stream, 0);
    s_fn_IBP_Serialize(s + 0x20, ser2);
    if (*(void**)(s + 0x20) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x20), *s_TypeInfo_ShowToolEntity)) {
            *(void**)(s + 0x20) = nullptr;
        }
    }
    g_fn_Serialize_Int(ser2, s + 0x28, nullptr);
    g_fn_Serializer_Dtor(ser2);
}

// ============================================================
// ShowTool_ShowCarried — faithful port of Entity_impl_73b0's toolId<0 branch.
// Attaches the model of the resource the citizen is carrying to the "tool" bone of
// the character mesh. The carried item = the highest-count entry in the citizen's
// StorageComponent; its mesh comes from that item's ModelDescription and its scale
// from the item's RawMaterialDescription (+0x50 carry-scale). Every lookup is guarded
// so a partially-materialized standalone material (missing ModelDescription/
// RawMaterialDescription, which is exactly what crashed the retail function in SA)
// simply shows no model instead of faulting.
// ============================================================
static void ShowTool_ShowCarried(void* entityV) {
    uint8_t* e = reinterpret_cast<uint8_t*>(entityV);


    // Skip if the entity is flagged for destruction (bit 10 of inner+0x40).
    s_fn_CheckPtr(e + 0x10);
    void* inner = *(void**)(e + 0x10);
    if (!inner) return;
    if (((*(uint32_t*)((uint8_t*)inner + 0x40)) >> 10) & 1) return;

    // Hide the previously-shown item first, if any.
    if (*(int*)(e + 0x1dc) >= 0) s_fn_HideTool(entityV);

    // Character mesh holder = StateMachineComponent(+0x278) -> +0x68.
    s_fn_CheckPtr(e + 0x278);
    void* smComp = *(void**)(e + 0x278);
    if (!smComp) return;
    uintptr_t charMeshHolder = *(uintptr_t*)((uint8_t*)smComp + 0x68);
    if (!charMeshHolder) return;

    // --- Storage scan: pick the highest-count item in the citizen's storage. ---
    s_fn_CheckPtr(e + 0x10);
    uint64_t storageIBP[2] = {0, 0};
    fn_FindComponent_Storage(*(void**)(e + 0x10), storageIBP);
    s_fn_CheckPtr(storageIBP);
    uintptr_t storageComp = (uintptr_t)storageIBP[0];
    if (!storageComp) return;
    int itemCount = *(int*)(storageComp + 0x30);

    if (itemCount < 1) return;
    void* bestItem = nullptr;
    int bestCount = 0;
    for (int i = 0; i < itemCount; i++) {
        s_fn_CheckPtr(storageIBP);
        uintptr_t itemArr = *(uintptr_t*)(storageComp + 0x28);
        int cnt = *(int*)(itemArr + (int64_t)i * 0x10 + 8);
        if (bestCount < cnt) {
            bestCount = cnt;
            bestItem = *(void**)(itemArr + (int64_t)i * 0x10);
        }
    }

    if (!bestItem) return;
    // --- Resolve the display mesh (ModelDescription) + carry-scale (RawMaterial). ---
    void* modelDesc = ShowTool_FindComponentByTid(bestItem, ShowTool_ModelDescTid());
    if (!modelDesc) return;                       // SA guard: no display model -> nothing shown
    void* rawMat = ShowTool_FindComponentByTid(bestItem, ShowTool_RawMatTid());
    if (!rawMat) return;                          // SA guard
    float carryScale = *(float*)((uint8_t*)rawMat + 0x50);

    // mesh = **( *(modelDesc+0x20) + 8 )  (faithful double-deref from 73b0).
    uintptr_t meshGroup = *(uintptr_t*)((uint8_t*)modelDesc + 0x20);
    if (!meshGroup) return;
    uintptr_t meshSlot = *(uintptr_t*)(meshGroup + 8);
    if (!meshSlot) return;
    uint64_t mesh = *(uint64_t*)meshSlot;

    // Ensure the held-item MeshInstance (entity+0xb8) is a constructed object.
    // Retail treats +0xb8 as an embedded, already-constructed Graphics::MeshInstance
    // and only SetMesh's it; but in standalone the character's held-item slot may
    // never have been initialized (this is the first carried item), leaving it
    // zeroed. SetMesh/Attach tolerate a zeroed object, but MeshInstance::Update is
    // then skipped (null vtable) and Graphics::Scene::AddObject faults on the
    // uninitialized geometry (READ of +0x40). Default-construct it in place — the
    // same 0x128-byte MeshInstance default-ctor the StorageDisplayComponent pile
    // renderer uses before its own SetMesh+AddObject — with sceneId(+0x120) = -1.
    void** heldVt = *(void***)(e + 0xb8);
    if (!heldVt && s_slot_MI_DefCtor) {
        s_slot_MI_DefCtor(e + 0xb8);
        *(uint32_t*)(e + 0xb8 + 0x120) = 0xffffffff;   // sceneId = -1 (not yet in scene)
    }


    // --- Assign the mesh to the citizen's held-item MeshInstance (entity+0xb8). ---
    if (s_slot_SetMesh) s_slot_SetMesh(e + 0xb8, (void*)mesh);

    // Scale Vector4 (splat carryScale) -> entity+0x1c4..0x1d0.
    Math::Vector4 scaleVec((Math::Scalar(carryScale)));
    *(float*)(e + 0x1c4) = scaleVec.x;
    *(float*)(e + 0x1c8) = scaleVec.y;
    *(float*)(e + 0x1cc) = scaleVec.z;
    *(float*)(e + 0x1d0) = scaleVec.w;

    // Attach the held-item geometry to the character mesh's "tool" node/bone.
    s_fn_CheckPtr(&charMeshHolder);
    void* graphicsMesh = *(void**)(charMeshHolder + 0xe8);
    s_fn_CheckPtr(&charMeshHolder);
    int nodeIdx = (s_slot_GetNodeIndex && graphicsMesh)
                ? s_slot_GetNodeIndex(graphicsMesh, "tool") : -1;
    if (s_slot_GeomAttach) s_slot_GeomAttach((void*)(charMeshHolder + 0x28), e + 0xb8, nodeIdx);

    // MeshInstance::Update(entity+0xb8, &time0) — vtable slot idx1 (+0x8).
    uint8_t time0[16] = {0};
    void** meshVt = *(void***)(e + 0xb8);
    if (meshVt) {
        typedef void (*upd_t)(void*, void*);
        ((upd_t)meshVt[1])(e + 0xb8, time0);
    }

    // Add the held-item geometry to the live Graphics::Scene (gameWorld+0x60).
    void* scene = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                ? *(void**)((uint8_t*)*g_pGameWorldSingleton + 0x60) : nullptr;
    uint32_t objId = (scene && s_slot_SceneAdd) ? s_slot_SceneAdd(scene, e + 0xb8) : 0xffffffff;
    *(int*)(e + 0x1d8) = -2;                       // toolId (carry sentinel)
    *(uint32_t*)(e + 0x1dc) = objId;               // scene object handle
}

// ============================================================
// ShowTool::AbstractImpl / _purecall (0x140042710)
// If toolId > 10 and != 0xFFFFFFFE, hide tool. Otherwise show tool.
// Always returns 1.
// ============================================================

int ShowTool_AbstractImpl_Impl(void* self, void* actionStack) {
    (void)actionStack;
    // Lazy resolve: when this action is built via the exe action-ctor path (not our
    // factory) ShowTool_ResolveFuncs never ran, so s_fn_* are null -> EXEC-of-null when
    // the executor runs it (e.g. a forester's PerformWork cut sequence shows the axe).
    // Same fix as the PerformWork vtable methods. Bail "done" if deps still missing.
    if (!ShowTool_ResolveFuncs()) return 1;
    uint8_t* s = (uint8_t*)self;
    uint32_t toolId = *(uint32_t*)(s + 0x28);


    if (toolId > 10 && toolId != 0xFFFFFFFE) {
        s_fn_CheckPtr(s + 0x20);
        void* entity = *(void**)(s + 0x20);
        if (!entity) return 1;  // entity removed, nothing to do
        s_fn_HideTool(entity);
        return 1;
    }

    s_fn_CheckPtr(s + 0x20);
    void* entity = *(void**)(s + 0x20);
    if (!entity) return 1;  // entity removed, nothing to do

    // toolId < 0 (e.g. 0xFFFFFFFE = -2) is the "carry" path: show the model of the
    // resource the citizen is hauling. The retail helper (Entity_impl_73b0) reads the
    // ModelDescription/RawMaterialDescription type-ids straight from exe .rdata, which
    // is null in standalone — so it can never match the mesh component and null-derefs.
    // Use our faithful, guarded re-port instead.
    if ((int)toolId < 0) {
        ShowTool_ShowCarried(entity);
        return 1;
    }

    s_fn_ShowToolOnEntity(entity, (int)toolId);
    return 1;
}

// ============================================================
// ShowTool::Exit (0x140042700)
// 3 bytes -- trivial return 0. Too small to hook.
// Implemented for standalone exe completeness.
// ============================================================
int ShowTool_Exit_Impl(void* /*self*/) {
    return 0;
}

// ============================================================
// ShowTool::CheckEntity (0x1400426d0)
// Hides tool on the entity at +0x20.
// ============================================================
void ShowTool_CheckEntity_Impl(void* self) {
    if (!ShowTool_ResolveFuncs()) return;   // lazy resolve (exe-ctor-built action)
    uint8_t* s = (uint8_t*)self;
    s_fn_CheckPtr(s + 0x20);
    void* entity = *(void**)(s + 0x20);
    if (!entity) return;  // entity already gone
    s_fn_HideTool(entity);
}

// ============================================================
// ShowTool::GetTarget (0x140042680)
// Returns true if entity at +0x20's parent (+0x10) matches the param.
// ============================================================
bool ShowTool_GetTarget_Impl(void* self, void* param) {
    if (!ShowTool_ResolveFuncs()) return false;   // lazy resolve (exe-ctor-built action)
    uint8_t* s = (uint8_t*)self;
    s_fn_CheckPtr(s + 0x20);
    void* entity = *(void**)(s + 0x20);
    if (!entity) return false;  // entity gone, no match
    void* entityParent = *(void**)(entity + 0x10);
    s_fn_CheckPtr(param);
    return entityParent == *(void**)param;
}

// ============================================================
// Standalone factory + vtable — Action family (notes/FACTORY_CASCADE_145_BRIEF.md).
// Vtable @ 0x1401589b0 (10 slots), size 0x30, parent "Action".
// Shared slot helpers live in action_base_standalone.{h,cpp}.
// ============================================================
#include "action_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t ShowTool_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("ShowTool");
    return c;
}

static void* g_ShowTool_Vtable[10] = {
    (void*)&Action_ScalarDtor,        // +0x00 ScalarDtor
    (void*)&Action_Slot1,             // +0x08 slot1 (returns null)
    (void*)&ShowTool_GetTypeId,           // +0x10 GetTypeId
    (void*)&ShowTool_Serialize_Impl,      // +0x18 Serialize
    (void*)&ShowTool_AbstractImpl_Impl,   // +0x20 AbstractImpl
    (void*)&Action_BaseExecute,   // +0x28 Execute
    (void*)&Action_Enter,  // +0x30 Enter
    (void*)&ShowTool_Exit_Impl,   // +0x38 Exit
    (void*)&ShowTool_CheckEntity_Impl,  // +0x40 CheckEntity
    (void*)&ShowTool_GetTarget_Impl, // +0x48 GetTarget
};

extern "C" void RegisterShowToolVtable() {
    RegisterExeAddr(0x1401589b0, (uintptr_t)g_ShowTool_Vtable);
}
extern "C" void* ShowTool_CreateInPlace(void* mem) {
    ShowTool_ResolveFuncs();
    return Action_InitInPlace(mem, 0x30, g_ShowTool_Vtable);
}
extern "C" void* ShowTool_Factory(void) {
    ShowTool_ResolveFuncs();
    return Action_Alloc(0x30, g_ShowTool_Vtable);
}
REGISTER_TYPE(ShowTool, "Action", &ShowTool_Factory, &ShowTool_CreateInPlace);
