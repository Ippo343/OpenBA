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
#include "object/External.h"
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "Entity/exe_bodies.h"      // fn_FindComponent_Storage (the guarded copy)
#include "Component/CharacterComponent.h"
#include "Component/ComponentDescription.h"
#include "Component/Description.h"
#include "Component/ModelComponent.h"
#include "Component/StorageComponent.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Action/exe_bodies.h"   // ActCt_HideTool_1176e0, ActCt_ShowToolOnEntity_1173b0
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "math/Math.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <new>

#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (SA type-id fallback)
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G

// ============================================================
// Helpers, called by name (B3R)
//   0x1401176e0 ActCt_HideTool_1176e0         -- hide tool on entity
//   0x1401173b0 ActCt_ShowToolOnEntity_1173b0 -- show tool with specific ID on entity
// ============================================================

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


// Scan a material template's sub-descriptions for the first one whose runtime type-id
// (slot 2 Type()) equals `tid`. Faithful to the two scan loops inside 73b0, with the
// tree's null-host / zero-tid / null-slot skips. Returns null if not found (SA-safe).
static Description* ShowTool_FindComponentByTid(const ComponentDescription* host, uint32_t tid) {
    if (!host || !tid) return nullptr;
    int count = host->_subDescCount;
    Description** arr = host->_subDescs;
    for (int i = 0; i < count; i++) {
        Description* comp = arr[i];
        if (!comp) continue;
        uint32_t got = comp->Type();   // slot 2
        if (got == tid)
            return ::Object::IsDerivedFromNullSafe(comp, tid) ? comp : nullptr;
    }
    return nullptr;
}

// ============================================================
// ShowTool::Serialize (0x140042780)
// Serializes: status(+0x18), citizen(+0x20) with type check, int(+0x28)
// ============================================================
void ShowTool::Persist(void* stream) {
    Action::Persist(stream);   // inlined in retail: Serializer(stream, 0), status +0x18

    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge (Persist's stream is void*)
    _field20.InternalBasePtr::Serialize(ser);
    if (_field20.Get() != nullptr) {
        if (!::Object::IsDerivedFromNullSafe(_field20.Get(), CitizenComponent::TypeId())) {   // cell 0x1401c7158
            _field20 = nullptr;
        }
    }
    ser.Serialize(_field28, nullptr);   // +0x28 int
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
static void ShowTool_ShowCarried(CitizenComponent* citizen) {
    // Skip if the entity is flagged for destruction (bit 10 of its +0x40 flags).
    ComponentEntity* inner = citizen->_entity;
    if (!inner) return;
    if ((inner->_entityFlags >> 10) & 1) return;

    // Hide the previously-shown item first, if any.
    if (citizen->toolSceneId >= 0) HideCitizenTool(citizen);   // 0x1401176e0

    // Character mesh holder = CharacterComponent (+0x278) -> its ModelComponent (+0x68).
    CharacterComponent* smComp = citizen->character;
    if (!smComp) return;
    ModelComponent* charMeshHolder = smComp->modelComponent;
    if (!charMeshHolder) return;

    // --- Storage scan: pick the highest-count item in the citizen's storage. ---
    StorageComponent* storageComp = nullptr;
    fn_FindComponent_Storage(citizen->_entity, &storageComp);
    if (!storageComp) return;
    int itemCount = storageComp->slotCount;

    if (itemCount < 1) return;
    ComponentDescription* bestItem = nullptr;
    int bestCount = 0;
    for (int i = 0; i < itemCount; i++) {
        const StorageComponent::Item& item = storageComp->storageSlots[i];
        int cnt = item._count;
        if (bestCount < cnt) {
            bestCount = cnt;
            bestItem = item._resource;
        }
    }

    if (!bestItem) return;
    // --- Resolve the display mesh (ModelDescription) + carry-scale (RawMaterial). ---
    Description* modelFound = ShowTool_FindComponentByTid(bestItem, ModelDescription::TypeId());   // cell 0x1401c7a88
    if (!modelFound) return;                      // SA guard: no display model -> nothing shown
    const ModelDescription* modelDesc = static_cast<const ModelDescription*>(modelFound);
    Description* rawFound = ShowTool_FindComponentByTid(bestItem, RawMaterialDescription::TypeId());   // cell 0x1401c7cb8
    if (!rawFound) return;                        // SA guard
    float carryScale = static_cast<const RawMaterialDescription*>(rawFound)->_carryScale;

    // mesh = mesh 0 of mesh group 0 (faithful double-deref from 73b0).
    const ModelMeshGroup* meshGroup = modelDesc->_meshGroups;
    if (!meshGroup) return;
    Graphics::GraphicsMesh** meshSlot = meshGroup->meshes;
    if (!meshSlot) return;
    Graphics::GraphicsMesh* mesh = *meshSlot;

    // LEFT: CitizenComponent +0xB8 is raw storage for a Graphics::MeshInstance (H5, owned
    // by CIT; declaring it makes its ctor / dtor run).
    Graphics::MeshInstance* held = reinterpret_cast<Graphics::MeshInstance*>(citizen->meshInstance);

    // Ensure the held-item MeshInstance (citizen +0xb8) is a constructed object.
    // Retail treats +0xb8 as an embedded, already-constructed Graphics::MeshInstance
    // and only SetMesh's it; but in standalone the character's held-item slot may
    // never have been initialized (this is the first carried item), leaving it
    // zeroed. SetMesh/Attach tolerate a zeroed object, but MeshInstance::Update is
    // then skipped (null vtable) and Graphics::Scene::AddObject faults on the
    // uninitialized geometry (READ of +0x40). Default-construct it in place — the
    // same MeshInstance default-ctor the StorageDisplayComponent pile renderer uses
    // before its own SetMesh+AddObject — and store -1 at +0xb8 + 0x120 (which is the
    // citizen's toolId, +0x1D8).
    void* heldVt = *reinterpret_cast<void* const*>(citizen->meshInstance);   // LEFT: H5 raw storage (vptr probe)
    if (!heldVt) {
        ::new (static_cast<void*>(citizen->meshInstance)) Graphics::MeshInstance();   // B.3.9 EXA: the real class
        citizen->toolId = -1;
    }


    // --- Assign the mesh to the citizen's held-item MeshInstance (+0xb8). ---
    held->SetMesh(Graphics::ExternalPtrHandle{mesh});

    // Scale Vector4 (splat carryScale) -> MeshInstance::_color (+0x10c..+0x118).
    Math::Vector4 scaleVec((Math::Scalar(carryScale)));
    held->_color[0] = scaleVec.x;
    held->_color[1] = scaleVec.y;
    held->_color[2] = scaleVec.z;
    held->_color[3] = scaleVec.w;

    // Attach the held-item geometry to the character mesh's "tool" node/bone.
    Graphics::GraphicsMesh* graphicsMesh = charMeshHolder->meshInstance.GetMesh().Get();
    int nodeIdx = graphicsMesh
                ? graphicsMesh->GetNodeIndexFromName("tool")
                : -1;
    charMeshHolder->meshInstance.Attach(held, nodeIdx);

    // MeshInstance::Update(held, &time0) — slot 1 of `Graphics::Geometry`. The receiver is
    // a PROVED `Graphics::MeshInstance`: +0xb8 was just constructed above if it was not
    // and SetMesh'd as one. The `meshVt` test stays: it tests the OBJECT (a
    // never-constructed held-item slot is zeroed), not a door pointer.
    const System::Time time0{0};
    void* meshVt = *reinterpret_cast<void* const*>(citizen->meshInstance);   // LEFT: H5 raw storage (vptr probe)
    if (meshVt) {
        held->Update(time0);
    }

    // Add the held-item geometry to the live Graphics::Scene (GameUpdateLoop +0x60).
    Graphics::Scene* scene = (GameUpdateLoop::_singleton != nullptr)
                           ? GameUpdateLoop::_singleton->m_graphicsScene : nullptr;
    uint32_t objId = scene
                   ? scene->AddObject(held)
                   : 0xffffffff;
    citizen->toolId = -2;                          // carry sentinel
    citizen->toolSceneId = static_cast<int32_t>(objId);   // scene object handle
}

// ============================================================
// ShowTool::AbstractImpl / _purecall (0x140042710)
// If toolId > 10 and != 0xFFFFFFFE, hide tool. Otherwise show tool.
// Always returns 1.
// ============================================================

int  ShowTool::AbstractImpl(void* actionStack) {
    (void)actionStack;
    uint32_t toolId = static_cast<uint32_t>(_field28);


    if (toolId > 10 && toolId != 0xFFFFFFFE) {
        CitizenComponent* entity = _field20.Get();
        if (!entity) return 1;  // entity removed, nothing to do
        HideCitizenTool(entity);   // 0x1401176e0
        return 1;
    }

    CitizenComponent* entity = _field20.Get();
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

    ShowCitizenTool(entity, (int)toolId);   // 0x1401173b0
    return 1;
}

// ============================================================
// ShowTool::CheckEntity (0x1400426d0)
// Hides tool on the citizen at +0x20.
// ============================================================
void ShowTool::CheckEntity() {
    CitizenComponent* entity = _field20.Get();
    if (!entity) return;  // entity already gone
    HideCitizenTool(entity);   // 0x1401176e0
}

// ============================================================
// ShowTool::GetTarget (0x140042680)
// Returns true if the citizen's (+0x20) owning entity (Component +0x10) matches the param.
// ============================================================
bool ShowTool::GetTarget(ComponentEntity** param) {
    CitizenComponent* entity = _field20.Get();
    if (!entity) return false;  // entity gone, no match
    return entity->_entity == *param;
}

// ============================================================
// class ShowTool (ShowTool.h) — ENDGAME B.3.8 ACT.
// The hand-built g_ShowTool_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"

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
    void* p = Object::detail::InternalMemAlloc(sizeof(ShowTool), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ShowTool());
}
void* ShowTool::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ShowTool());
}


// ============================================================
// FUN_140042890 — ShowTool_Action_Ctor
//   RCX=self, RDX=param_2 ({inner ptr}), R8D=toolType (int @ +0x28).
//   Base Internal ctor; Action vtable -> ShowTool vtable; +0x20 = *param_2; +0x28 = toolType.
// ============================================================
void* ActChild_ShowToolCtor_42890(void* self, void* param_2, int toolType) {
    // B4 FORWARD: outside callers (Component/citizenai_actionctors.cpp, the Work action
    // builders) hold the citizen reference as void*.
    return static_cast<Object::Base*>(
        new (self) ShowTool(static_cast<CitizenComponent* const*>(param_2), toolType));
}

// 0x140042890: Action() (Internal, +0x10/+0x18 = 0), then +0x20 = *param_2 (the
// door's double store `+0x20 = 0; +0x20 = *param_2`: the InternalPtr's own ctor stores
// the 0), +0x28.
ShowTool::ShowTool(CitizenComponent* const* citizen, int toolId)
    : _field20(*citizen), _field28(toolId) {}

DEFINE_EXE_TYPE(ShowTool, "Action")
