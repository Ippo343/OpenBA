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
#include "Component/ModelDescription.h"   // ENDGAME B.3.8 CB3: ModelDescription::TypeId()
#include "Component/RawMaterialDescription.h"   // ENDGAME B.3.8 CB3: RawMaterialDescription::TypeId()
#include "Component/CitizenComponent.h"   // ENDGAME B.3.8 CB3: CitizenComponent::TypeId()
// The runtime type headers come FIRST: they reach runtime/types/volume/Volume.h,
// whose global-scope `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()`
// once <windows.h> has been seen through resolve.h.  ENDGAME B.3.5 batch K1.
#include "graphics/Geometry.h"
#include "graphics/Mesh.h"
#include "graphics/GraphicsMesh.h"
#include "graphics/Scene.h"
#include "system/Value.h"       // System::Time, for Geometry::Update's argument
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "math/Math.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <new>

#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (SA type-id fallback)
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h -- ShowTool-specific helpers)
// ============================================================

// FUN_1401176e0 -- hide tool on entity
static void (*s_fn_HideTool)(void* entity) = nullptr;

// FUN_1401173b0 -- show tool with specific ID on entity
static void (*s_fn_ShowToolOnEntity)(void* entity, int toolId) = nullptr;

// DAT_1401c7158 -- TypeInfo for entity type check

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
// ENDGAME B.3.5 batch K1: the four caches that used to hold
// GraphicsMesh::GetNodeIndexFromName (slot 0x140152970), Geometry::Attach
// (0x140152968), MeshInstance::SetMesh (0x140151498) and Scene::AddObject
// (0x140151370) went with their last callers in ShowTool_ShowCarried, which now
// call the members. The MeshInstance default-CTOR door below stays: no
// Video/Graphics class has a real constructor (B35_RULES section 11 / B4b-2).


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
        uint32_t got = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge
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

    s_fn_HideTool = reinterpret_cast<void(*)(void*)>(
        ResolveAddr(Module::Exe, 0x1401176e0));
    s_fn_ShowToolOnEntity = reinterpret_cast<void(*)(void*, int)>(
        ResolveAddr(Module::Exe, 0x1401173b0));
    // ENDGAME B.3.5 batch K1: the GetNodeIndexFromName / Geometry::Attach dispatch
    // slots were NOT registered in standalone, so this used to fall back to the two
    // Runtime addresses 0x1800fee90 / 0x1800a3fa0. Both are members of real classes
    // now, so neither the slot read nor the fallback is needed.

    bool ok = s_fn_HideTool && s_fn_ShowToolOnEntity &&
              fn_IsDerivedFrom;

#ifdef _DEBUG
    printf("[ShowTool] ResolveFuncs: HideTool=%p ShowTool=%p\n",
           (void*)s_fn_HideTool,
           (void*)s_fn_ShowToolOnEntity);
    if (!ok) printf("[ShowTool] ERROR: some function pointers not resolved!\n");
#endif

    s_showtool_funcs_resolved = ok;
    return ok;
}

// ============================================================
// ShowTool::Serialize (0x140042780)
// Serializes: status(+0x18), entity ptr(+0x20) with type check, int(+0x28)
// ============================================================
void ShowTool::Persist(void* stream) {
    // Reached through the registered vtable, which the factory path never
    // has to touch — an exe-ctor-built action arrives here with the file
    // statics still null. Lazy-resolve like the other entry points do.
    ShowTool_ResolveFuncs();
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    alignas(8) uint8_t ser1[24];
    alignas(8) uint8_t ser2[24];

    ::new (static_cast<void*>(ser1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(_status, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();   // B.3.4 bridge

    ::new (static_cast<void*>(ser2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<Object::InternalBasePtr*>(s + 0x20)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (_field20 != nullptr) {
        if (!fn_IsDerivedFrom(_field20, CitizenComponent::TypeId())) {   // cell 0x1401c7158
            _field20 = nullptr;
        }
    }
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(s + 0x28), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser2)->~Serializer();   // B.3.4 bridge
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
    void* inner = *(void**)(e + 0x10);
    if (!inner) return;
    if (((*(uint32_t*)((uint8_t*)inner + 0x40)) >> 10) & 1) return;

    // Hide the previously-shown item first, if any.
    if (*(int*)(e + 0x1dc) >= 0) s_fn_HideTool(entityV);

    // Character mesh holder = StateMachineComponent(+0x278) -> +0x68.
    void* smComp = *(void**)(e + 0x278);
    if (!smComp) return;
    uintptr_t charMeshHolder = *(uintptr_t*)((uint8_t*)smComp + 0x68);
    if (!charMeshHolder) return;

    // --- Storage scan: pick the highest-count item in the citizen's storage. ---
    uint64_t storageIBP[2] = {0, 0};
    fn_FindComponent_Storage(*(void**)(e + 0x10), storageIBP);
    uintptr_t storageComp = (uintptr_t)storageIBP[0];
    if (!storageComp) return;
    int itemCount = *(int*)(storageComp + 0x30);

    if (itemCount < 1) return;
    void* bestItem = nullptr;
    int bestCount = 0;
    for (int i = 0; i < itemCount; i++) {
        uintptr_t itemArr = *(uintptr_t*)(storageComp + 0x28);
        int cnt = *(int*)(itemArr + (int64_t)i * 0x10 + 8);
        if (bestCount < cnt) {
            bestCount = cnt;
            bestItem = *(void**)(itemArr + (int64_t)i * 0x10);
        }
    }

    if (!bestItem) return;
    // --- Resolve the display mesh (ModelDescription) + carry-scale (RawMaterial). ---
    void* modelDesc = ShowTool_FindComponentByTid(bestItem, ModelDescription::TypeId());   // cell 0x1401c7a88
    if (!modelDesc) return;                       // SA guard: no display model -> nothing shown
    void* rawMat = ShowTool_FindComponentByTid(bestItem, RawMaterialDescription::TypeId());   // cell 0x1401c7cb8
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
    if (!heldVt) {
        ::new (static_cast<void*>(e + 0xb8)) Graphics::MeshInstance();   // B.3.9 EXA: the real class
        *(uint32_t*)(e + 0xb8 + 0x120) = 0xffffffff;   // sceneId = -1 (not yet in scene)
    }


    // --- Assign the mesh to the citizen's held-item MeshInstance (entity+0xb8). ---
    reinterpret_cast<Graphics::MeshInstance*>(e + 0xb8)
        ->SetMesh(Graphics::ExternalPtrHandle{(void*)mesh});   // B.3.5 bridge

    // Scale Vector4 (splat carryScale) -> entity+0x1c4..0x1d0.
    Math::Vector4 scaleVec((Math::Scalar(carryScale)));
    *(float*)(e + 0x1c4) = scaleVec.x;
    *(float*)(e + 0x1c8) = scaleVec.y;
    *(float*)(e + 0x1cc) = scaleVec.z;
    *(float*)(e + 0x1d0) = scaleVec.w;

    // Attach the held-item geometry to the character mesh's "tool" node/bone.
    void* graphicsMesh = *(void**)(charMeshHolder + 0xe8);
    int nodeIdx = graphicsMesh
                ? reinterpret_cast<const Graphics::GraphicsMesh*>(graphicsMesh)
                      ->GetNodeIndexFromName("tool")   // B.3.5 bridge
                : -1;
    reinterpret_cast<Graphics::Geometry*>(charMeshHolder + 0x28)
        ->Attach(reinterpret_cast<Graphics::Geometry*>(e + 0xb8), nodeIdx);   // B.3.5 bridge

    // MeshInstance::Update(entity+0xb8, &time0) — vtable slot idx1 (+0x8).
    // B.3.5 bridge — slot 1 of `Graphics::Geometry` IS `Update(const System::Time&)`,
    // and the receiver is a PROVED `Graphics::MeshInstance`: entity+0xb8 was just run
    // through `??0MeshInstance@Graphics@@` above and SetMesh'd as one. The `meshVt`
    // test stays: it tests the OBJECT (a never-constructed held-item slot is zeroed),
    // not a door pointer.
    uint8_t time0[16] = {0};
    void** meshVt = *(void***)(e + 0xb8);
    if (meshVt) {
        reinterpret_cast<Graphics::Geometry*>(e + 0xb8)->Update(
            *reinterpret_cast<const System::Time*>(time0));   // B.3.5 bridge
    }

    // Add the held-item geometry to the live Graphics::Scene (gameWorld+0x60).
    void* scene = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                ? *(void**)((uint8_t*)*g_pGameWorldSingleton + 0x60) : nullptr;
    uint32_t objId = scene
                   ? reinterpret_cast<Graphics::Scene*>(scene)
                         ->AddObject(reinterpret_cast<Graphics::Geometry*>(e + 0xb8))   // B.3.5 bridge
                   : 0xffffffff;
    *(int*)(e + 0x1d8) = -2;                       // toolId (carry sentinel)
    *(uint32_t*)(e + 0x1dc) = objId;               // scene object handle
}

// ============================================================
// ShowTool::AbstractImpl / _purecall (0x140042710)
// If toolId > 10 and != 0xFFFFFFFE, hide tool. Otherwise show tool.
// Always returns 1.
// ============================================================

int  ShowTool::AbstractImpl(void* actionStack) {
    (void)actionStack;
    // Lazy resolve: when this action is built via the exe action-ctor path (not our
    // factory) ShowTool_ResolveFuncs never ran, so s_fn_* are null -> EXEC-of-null when
    // the executor runs it (e.g. a forester's PerformWork cut sequence shows the axe).
    // Same fix as the PerformWork vtable methods. Bail "done" if deps still missing.
    if (!ShowTool_ResolveFuncs()) return 1;
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    uint32_t toolId = *(uint32_t*)(s + 0x28);


    if (toolId > 10 && toolId != 0xFFFFFFFE) {
        void* entity = _field20;
        if (!entity) return 1;  // entity removed, nothing to do
        s_fn_HideTool(entity);
        return 1;
    }

    void* entity = _field20;
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
// ShowTool::CheckEntity (0x1400426d0)
// Hides tool on the entity at +0x20.
// ============================================================
void ShowTool::CheckEntity() {
    if (!ShowTool_ResolveFuncs()) return;   // lazy resolve (exe-ctor-built action)
    void* entity = _field20;
    if (!entity) return;  // entity already gone
    s_fn_HideTool(entity);
}

// ============================================================
// ShowTool::GetTarget (0x140042680)
// Returns true if entity at +0x20's parent (+0x10) matches the param.
// ============================================================
bool ShowTool::GetTarget(void* param) {
    if (!ShowTool_ResolveFuncs()) return false;   // lazy resolve (exe-ctor-built action)
    void* entity = _field20;
    if (!entity) return false;  // entity gone, no match
    void* entityParent = *(void**)((uint8_t*)entity + 0x10);
    return entityParent == *(void**)param;
}

// ============================================================
// class ShowTool (ShowTool.h) — ENDGAME B.3.8 ACT.
// The hand-built g_ShowTool_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

// create 0x1400013b0. Retail's default ctor (inlined, 0x14000140c) stores +0x28 = -1;
// the tree's factory zero-filled every field instead. B38 B8-1: the retail stores are kept; the
// fields retail leaves unwritten stay zero-filled (MemAlloc gives no guarantee).
ShowTool::ShowTool()
    : _field20(nullptr),
      _field28(-1),
      _pad2C{} {}

// [3] 0x140042780
// [4] 0x140042710
// [7] 0x140042700 — the leaf's own `RET 0` (same bytes as the base body)
void ShowTool::Exit(void*) {}
// [8] 0x1400426d0
// [9] 0x140042680

void* ShowTool::CreateNewObject() {
    ShowTool_ResolveFuncs();
    void* p = Object::detail::InternalMemAlloc(sizeof(ShowTool), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ShowTool());
}
void* ShowTool::CreateInPlaceObject(void* storage) {
    ShowTool_ResolveFuncs();
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ShowTool());
}


DEFINE_EXE_TYPE(ShowTool, "Action")
