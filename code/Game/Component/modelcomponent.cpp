// code/Game/Component/modelcomponent.cpp
// Clean reimplementation of ModelComponent lifecycle methods from Application-x64.exe.
//
// ModelComponent (0x170 bytes) is the largest component. It embeds a
// Graphics::MeshInstance sub-object at +0x28 (0x120 bytes) and a
// Math::Transform at +0x148 (0x1C bytes).
//
// Vtable at 0x140163ba0. Non-trivial overrides:
//   Shutdown  (0x140124b70) — Serializes mesh state, transform, model variant data
//   Serialize (0x140124ac0) — Reads building data, sets mesh from resource arrays
//   Draw      (0x140124a30) — Adds mesh to Graphics::Scene if valid variant
//   OnCreate  (0x1401249d0) — Removes mesh from Scene, resets modelId
//   OnRemove  (0x140124960) — Calls MeshInstance update, checks animating, clears flag
//
// Decompiled from Application-x64.exe using Ghidra.

#include "graphics/Scene.h"
#include "graphics/Mesh.h"
#include "Component/modelcomponent_funcs.h"
#include "scene_watch.h"
#include "Object/internal_factory_audit.h"
#include "object/External.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "math/Math.h"
#include "Component/ModelDescription.h"   // ENDGAME B.3.9 CB: 0x1401c7a88 cell class
#include "Entity/ComponentEntity.h"
#include "perf_experimental.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <atomic>
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/exe_bodies.h"
// graphics/*.h are included at the TOP of this file (before resolve.h) because
// they pull runtime/types/volume/Volume.h, whose global-scope `struct Rectangle`
// is hidden by <wingdi.h>'s `Rectangle()` once <windows.h> has been seen.

// Helpers, called by name (B3R): RtSystem_Time_Ctor (import slot 0x140152b00,
// ??0Time@System@@QEAA@M@Z), ComponentEntity::SendEvent (0x1400c2b70, below),
// ComponentEntity::RefreshEntityFlags (0x1400c3470).
//
// LEFT (B.4 COMP-d): GameUpdateLoop::m_graphicsScene is void* (W0 header).
//
// ENDGAME B.3.9 GFX (coordinator ruling 4, B9-13 foreign site): the
// embedded Graphics::MeshInstance at +0x28 is built and destroyed by the real
// class (`::new` / `->~MeshInstance()`), not through import slots 0x140151490
// / 0x1401514a8, so those two resolutions are gone from this file.

// ============================================================
// Reimpl of FUN_1400c2b70 (notify each child object of a model change)
// ============================================================
// Iterates the component array (the count read once). For each entry: CheckPtr
// the IBP, then dispatch the component's slot 12 (+0x60) with `eventId`.
// 1:1 port of the exe helper.
void ComponentEntity::SendEvent(int eventId) {
    int count = _componentCount;
    for (int64_t i = 0; i < count; ++i) {
        _components[i].Get()->OnEvent(eventId);   // slot 12
    }
}

// B4 FORWARD: 7 outside caller files (action_base_cnr.cpp, buildcomponent_state8.cpp,
// citizen_assignworkplace.cpp, destroycomponent_state6.cpp, workplacecomponent.cpp,
// tool_base_tile.cpp; declared in Component/exe_bodies.h).
extern "C" void ModelComponentNotify_Reimpl(void* entity, int param) {
    reinterpret_cast<ComponentEntity*>(entity)->SendEvent(param);   // LEFT: SIG ModelComponentNotify_Reimpl (B4 FORWARD, wave X)
}

// ============================================================
// Helper: the linked ModelDescription
// ============================================================
// The standalone Runtime factory tree is incomplete, so fn_IsDerivedFrom
// returns false for legit descriptions. ComponentEntity::CtorFromDescription
// has already linked _description to this component's ModelDescription template,
// so trust the non-null pointer (matches the IsDerivedFromOrTrust policy used
// throughout the spawn path).
static ModelDescription* LinkedModelDescription(const ModelComponent* self) {
    Description* obj = self->_description;
    return static_cast<ModelDescription*>(obj);
}

// ============================================================
// Helper: the mesh of a group from the description's mesh-group array
// ============================================================
// _meshGroups[meshGroup].meshes[meshIndex]
static Graphics::GraphicsMesh* GetMeshFromDescription(const ModelDescription* desc, int16_t meshGroup,
                                                      int16_t meshIndex) {
    if (!desc || meshIndex < 0 || meshGroup < 0) return nullptr;
    const ModelMeshGroup* groupArray = desc->_meshGroups;
    if (!groupArray) return nullptr;            // description carries no meshes
    Graphics::GraphicsMesh** group = groupArray[meshGroup].meshes;
    if (!group) return nullptr;
    return group[meshIndex];
}

// ============================================================
// Per-entity model-variant pick (faithful core of FUN_1401253d0)
// ============================================================
// Citizens all render identically because ModelComponent::Startup sets the
// mesh group from a FIXED description field and the per-citizen pick that the
// original performs in the deferred citizen init (FUN_140117080 -> FUN_1401253d0)
// is guarded off in standalone. This helper reproduces the safe core of
// FUN_1401253d0: choose a random mesh group within the description's valid range,
// then SetMesh the matching mesh into the MeshInstance (+0x28). It touches ONLY the
// same fields/SetMesh path ModelComponent::Startup already uses for buildings — no
// risky vtable Draw/Remove calls.
//
// MUST be called AFTER the component's Startup (so meshIndex is populated) and
// BEFORE the entity is registered (so the entity's OnAdded adds the new mesh to
// the scene). The description's mesh-group count is the same field the building
// randomize path uses as its Rand range, so the pick is always in-bounds.
//
// Returns the chosen group index, or -1 if no change was made. `forceIdx` picks
// a specific GROUP (e.g. gender: retail uses group=(gender==0)?1:0);
// pass -1 to keep the original random pick over the group count.
extern "C" int ModelComponent_RandomizeVariant(void* modelComp, int forceIdx) {
    if (!modelComp) return -1;
    if (GameUpdateLoop::_singleton == nullptr) return -1;
    // LEFT: starttown.cpp hands the component as void*.
    ModelComponent* self = reinterpret_cast<ModelComponent*>(modelComp);

    ModelDescription* desc = LinkedModelDescription(self);
    if (!desc) return -1;

    // Mesh-group count. Nothing to randomize over <= 1 group.
    uint32_t count = static_cast<uint32_t>(desc->_meshGroupCount);
    if (count <= 1) return -1;

    int16_t meshGroup;
    if (forceIdx >= 0) {
        meshGroup = (int16_t)((uint32_t)forceIdx % count);   // clamp into range
    } else {
        System::Random* random = GameUpdateLoop::_singleton->m_random;
        if (!random) return -1;
        meshGroup = (int16_t)random->Rand(static_cast<uint32_t>(count));
    }
    self->meshGroup = meshGroup;

    // Keep the mesh index Startup already chose; skip if it has no renderable
    // mesh ( < 0 ).
    int16_t meshIndex = self->meshIndex;
    if (meshIndex < 0) return -1;

    Graphics::GraphicsMesh* mesh = GetMeshFromDescription(desc, meshGroup, meshIndex);
    if (!mesh) return -1;
    self->meshInstance.SetMesh(Graphics::ExternalPtrHandle{mesh});
    return meshGroup;
}







// ============================================================================
// ENDGAME B.3.8 VIS — ModelComponent as a REAL class (ModelComponent.h).
//
// Before: a 24-slot g_ModelComponent_RealVtable filled on first use, every
// unset slot a file-local stub, ModelComponent_InitReal as the ctor. After: the
// compiler's 16-slot vtable and a real ctor/dtor chain (B38_RULES D1, D2). The
// spawn chain still drives rendering: ComponentEntity::CtorFromDescription
// calls slot 4 (Startup = ModelComponent_Serialize, sets the mesh from the
// description) and OnAddedPass calls slot 6 (OnAdded = ModelComponent_Draw,
// registers the geometry in Graphics::Scene).
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/ModelComponent.h"

// 0x140124d20: Component() is the CALL 0x1401188b0 (the Object::Internal base
// registers the object — the dtor's ~Internal must match it); the 0x140163ba0
// stamp is the compiler's. The members: MeshInstance() on +0x28 (retail CALL
// [??0MeshInstance]), Transform() on +0x148 (slot 0x140151328), mesh group /
// index / yaw = 0 (one qword store), modelId = -1; flags |= 0x83.
ModelComponent::ModelComponent()
    : meshGroup(0), meshIndex(0), yaw(0.0f), modelId(-1) {
    OrFlagsInCtor(0x83);
}

// 0x140124cd0: ~MeshInstance(+0x28); the compiler then chains ~Component and
// ~Internal (the old slot 0 guarded ~Internal with ObjAudit::IsRegistered for
// never-registered instances; the real ctor always registers).
// The member's ~MeshInstance is retail's CALL [??1MeshInstance] on +0x28.
ModelComponent::~ModelComponent() = default;

// ============================================================
// ModelComponent::Persist (vtable slot [3], 0x140124b70)
// ============================================================
// Serializes mesh state: mesh group / index, transform, yaw, mesh instance data,
// and a boolean flag for whether modelId is valid.
//
// On load (serializer direction == 0): if meshIndex >= 0, calls SetMesh to
// restore the mesh from the description's mesh groups.
// Also conditionally calls OnAdded (+0x30) to re-add to scene.

void ModelComponent::Persist(IO::Stream& stream) {
    // Base class serialize
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    IO::Serializer ser(stream, 0);

    ser.Serialize(meshGroup, nullptr);
    ser.Serialize(meshIndex, nullptr);

    // Math::Transform at +0x148 (slot 0x1401522e0).
    transform.Serialize(&ser);

    ser.Serialize(yaw, nullptr);

    Graphics::MeshInstance* mesh = &meshInstance;

    // On load: if the mesh index is set, restore the mesh from the description
    if (static_cast<int>(ser.GetDirection()) == 0) {   // IO::Serializer::GetDirection, retail 0x180006210
        if (meshIndex >= 0) {
            ModelDescription* desc = LinkedModelDescription(this);
            Graphics::GraphicsMesh* meshResource = GetMeshFromDescription(desc, meshGroup, meshIndex);
            if (meshResource) {
                mesh->SetMesh(Graphics::ExternalPtrHandle{meshResource});
            }
        }
    }

    // Serialize the MeshInstance sub-object
    mesh->Serialize(ser);

    // Serialize bool flag: modelId >= 0
    bool hasModelId = (modelId >= 0);
    ser.Serialize(hasModelId, nullptr);

    // On load: if flag was set, re-register in the scene
    if (static_cast<int>(ser.GetDirection()) == 0 && hasModelId) {   // IO::Serializer::GetDirection, retail 0x180006210
        // Slot 6 (+0x30, OnAdded) on this, a virtual dispatch in retail.
        OnAdded();
    }
}
// ============================================================
// ModelComponent::Startup (vtable slot [4], 0x140124ac0)
// ============================================================
// Reads the default mesh group / index from the ModelDescription, optionally
// randomizes the group, calls SetMesh if the index >= 0.

void ModelComponent::Startup() {
    ModelDescription* desc = LinkedModelDescription(this);
    if (!desc) return;

    meshGroup = static_cast<int16_t>(desc->_defaultMeshGroup);

    // If the description asks for it, randomize the mesh group
    if (desc->_randomMeshGroup) {
        if (GameUpdateLoop::_singleton != nullptr) {
            System::Random* random = GameUpdateLoop::_singleton->m_random;
            uint32_t range = static_cast<uint32_t>(desc->_meshGroupCount);
            uint32_t randVal = random->Rand(static_cast<uint32_t>(range));
            meshGroup = (int16_t)randVal;
        }
    }

    int16_t index = static_cast<int16_t>(desc->_defaultMeshIndex);
    meshIndex = index;

    // If the index >= 0, set the mesh
    if (index >= 0) {
        Graphics::GraphicsMesh* meshResource = GetMeshFromDescription(desc, meshGroup, index);
        if (meshResource) {
            meshInstance.SetMesh(Graphics::ExternalPtrHandle{meshResource});
        }
    }

}
// ============================================================
// ModelComponent::OnAdded (vtable slot [6], 0x140124a30)
// ============================================================
// Adds mesh geometry to the Graphics::Scene for rendering.
// Only acts if meshIndex >= 0 and not yet in scene (modelId < 0).

#if defined(_DEBUG)
// [ModelComponent::OnAdded] trace budget (B6-5: namespace-scope atomic).
static std::atomic<int> s_modelDrawLog{0};
#endif

void ModelComponent::OnAdded() {
    const int16_t index = meshIndex;
    const int32_t id0 = modelId;

    if (index >= 0 && id0 < 0) {
        // IsDerivedFrom check (result unused in original — just side effect?)
        // cell 0x1401c7a88 = ModelDescription (the old "MultiObjDesc" name lied)
        Description* desc = _description;
        (void)::Object::IsDerivedFromNullSafe(desc, ModelDescription::TypeId());

        Graphics::MeshInstance* mesh = &meshInstance;
        mesh->SetTransform(transform);

        // MeshInstance::Update with Time(0) (Geometry slot 1)
        {
            System::Time timeObj{0};
            RtSystem_Time_Ctor(&timeObj, 0.0f);   // slot 0x140152b00
            mesh->Update(timeObj);
        }

        // Add to scene
        if (GameUpdateLoop::_singleton) {
            Graphics::Scene* scene = GameUpdateLoop::_singleton->m_graphicsScene;   // +0x60
            if (scene) {
                uint32_t id = scene->AddObject(mesh);
                modelId = static_cast<int32_t>(id);
                SceneWatch_Note(id, mesh, "ModelComponent::Draw");
#if defined(_DEBUG)
                if (s_modelDrawLog.fetch_add(1, std::memory_order_relaxed) < 10) {
                    fprintf(stderr, "[ModelComponent::OnAdded] self=%p scene=%p "
                            "modelVariant=%d sceneObjId=%u\n",
                            (void*)this, static_cast<void*>(scene), (int)index, id);
                    fflush(stderr);
                }
#endif
            }
        }
    }
}
// ============================================================
// ModelComponent::OnRemove (vtable slot [7], 0x1401249d0)
// ============================================================
// Removes mesh from Scene and resets modelId. Notifies owner entity.

void ModelComponent::OnRemove() {
    const int16_t index = meshIndex;
    const int32_t id0 = modelId;

    if (index >= 0 && id0 >= 0) {
        // Remove from scene
        if (GameUpdateLoop::_singleton) {
            Graphics::Scene* scene = GameUpdateLoop::_singleton->m_graphicsScene;   // +0x60
            if (scene) {
                scene->RemoveObject((uint32_t)id0);
            }
        }

        modelId = -1;

        // Check owner and notify
        if (_entity) {
            _entity->SendEvent(0);   // 0x1400c2b70
        }
    }
}
// ============================================================
// ModelComponent::Tick (vtable slot [8], 0x140124960)
// ============================================================
// Calls MeshInstance Update (Geometry slot 1), checks IsAnimating.
// If not animating, clears draw flag bit 2 from _flags, notifies entity.
// NOTE: RDX (time) is forwarded implicitly by the caller.

void ModelComponent::Tick(void* time) {
    // The embedded MeshInstance (+0x28) is always constructed (the ctor builds it), so
    // the old null-slot test of its table is gone with the by-name call.
    Graphics::MeshInstance* mesh = &meshInstance;
    {
        // [PerfX ANIM_CULL -- NON-RETAIL] On the last speed-iteration retail poses every
        // animated mesh (AnimationState::UpdateNodes). A mesh outside the camera frustum
        // is never drawn, so its pose is left root-only (the animation clock and the
        // root-motion offset still advance exactly as retail's intermediate iterations
        // do); PerfX flushes the pose with a zero-time update if the sim pauses, and the
        // first visible tick recomputes it from the animation time. Off by default
        // outside the perf build (BANISHED_PERF_ANIM_CULL=0 kills it).
        // [PerfX ANIM_BATCH -- NON-RETAIL] at speed N retail runs N root-only updates per
        // frame per model only to sample the root-motion offset every tick. For a model
        // whose offset comes from the AnimationManager pool (instanced animation, ticked
        // every iteration anyway) or is zero (no root motion) the intermediate ones are
        // skipped and the last iteration advances the animation by the skipped ticks in
        // one call: the clock, the drawn pose and the walk speed are exact. A blending or
        // non-instanced root-motion model is ticked normally (PerfX::AnimBatchDecide).
        int64_t batchedTime = 0;
        if (PerfX::AnimBatchDecide(this, time, &batchedTime)) return;
        const System::Time t{batchedTime};
        if (PerfX::AnimCullSkip(this)) {
            uint8_t* flag = PerfX::RootOnlyFlag();
            *flag = 1;
            mesh->Update(t);
            *flag = 0;
        } else {
            mesh->Update(t);
        }
    }

    // Check if still animating
    {
        bool animating = mesh->IsAnimating();
        if (!animating) {
            // Clear flag bit 2 (0x04)
            uint32_t oldFlags = _flags;
            uint32_t newFlags = oldFlags & ~0x04u;
            _flags = newFlags;

            // If flags actually changed (low 16 bits) and owner is non-null, notify
            if (((newFlags ^ oldFlags) & 0xFFFF) != 0) {
                if (_entity != nullptr) {
                    _entity->RefreshEntityFlags();   // 0x1400c3470
                }
            }
        }
    }
}

// create 0x1400358d0: MemAlloc(0x170, 8), then the ctor.
void* ModelComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ModelComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ModelComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* ModelComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ModelComponent());
}

DEFINE_EXE_TYPE(ModelComponent, "Component")
