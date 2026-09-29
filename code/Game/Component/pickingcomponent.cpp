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
#include "Component/ModelComponent.h"
#include "Component/MapComponent.h"
#include "Component/PickingComponent.h"
#include "Component/PickingDescription.h"
#include "math/Math.h"
#include "object/External.h"
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G
#include "Entity/ComponentEntity.h"   // ENDGAME B.3.9 CB: ComponentEntity::TypeId()
#include "picking/Scene.h"     // ENDGAME B.3.7 callers (K2)



// ============================================================
// PickingComponent::RefreshMesh (0x140128f50) — (re)register the picking mesh
// ============================================================
// Faithful port of the exe's picking-mesh (re)build routine. Retail calls it from
// vtable +0x30 / +0x58 and the BuildComponent helpers; the tree also drives it at
// spawn for the (static) start-town buildings.
//
// The probe (BANISHED_PICKMESH_PROBE, starttown.cpp) established the SA ground truth:
//   - Picking::Scene (gw+0x70) IS constructed.
//   - Building entities carry a real PickingComponent with a pick-description set
//     (and its group table populated).
//   - But userData is null and pickingId is an uninitialized 0 (should be -1) —
//     Startup/OnAdded never ran.
// So we replicate Startup (userData = owner) then OnAdded (AddMesh).
void PickingComponent::RefreshMesh() {
    if (GameUpdateLoop::_singleton == nullptr) return;

    // --- Startup (0x140128c30): userData = owner.
    ComponentEntity* entity = _entity;
    if (!entity) return;
    // Retail SPLITS what this function fuses: Startup seats userData = owner, and OnAdded
    // (which runs much later, from EntityList::RegisterEntity) passes whatever userData
    // holds as the pick userData. BridgeTool::CalcPath uses that window — it re-points
    // each deck segment's userData at the MASTER bridge entity so clicking any piece
    // selects the whole bridge. Fusing Startup+OnAdded here must not undo that, so only
    // default userData when nothing has claimed it.
    if (!userData) userData = entity;
    ComponentEntity* pickUserData = userData;
    // Retail's OnAdded (0x140128f50) is a REFRESH, not a one-shot. Its very first act is to
    // call its OWN vtable +0x38 — PickingComponent::OnRemove, i.e. "if pickingId >= 0,
    // Picking::Scene::RemoveMesh it and reset to -1" — and only then does it re-register
    // under `meshIndex >= 0 && pickingId < 0`. So the guard is always satisfied by the
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
    OnRemove();   // retail: its own vtable +0x38
    if (pickingId >= 0) return;   // retail: `(int)param_1[5] < 0`

    // --- OnAdded body (0x140128f50) ---
    ModelComponent* model = entity->FindComponent<ModelComponent>();   // 0x140003f60
    if (!model) return;
    int16_t meshGroup = model->meshGroup;
    int16_t meshIndex = model->meshIndex;
    if (meshIndex < 0) return;                    // exe guard: -1 < meshIndex

    // Transform copied from the ModelComponent.
    const Math::Transform xform = model->transform;

    // Mesh = pickDesc->_groups[meshGroup].meshes[meshIndex]. Retail validates the
    // description IsDerivedFrom(DAT_1401c7c48) first; the probe proved it is a valid
    // pick-description with a populated table, so we trust the component's own
    // field and null-guard each deref instead of pulling in that type id.
    Description* descRaw = _description;
    if (!descRaw) return;
    PickingDescription* pickDesc = static_cast<PickingDescription*>(descRaw);
    const PickingDescription::Group* groups = pickDesc->_groups;
    if (!groups) return;
    Picking::PickingMesh** row = groups[(int)meshGroup].meshes;
    if (!row) return;
    Picking::PickingMesh* mesh = row[(int)meshIndex];
    if (!mesh) return;

    Picking::Scene* scene = GameUpdateLoop::_singleton->m_pickingScene;
    if (!scene) return;

    // Picking::Scene::AddMesh (Runtime 0x1801252e0): the ExternalPtr is a bare
    // register value (B7-5), the Transform by reference, three trailing zeros.
    uint32_t meshId = scene->AddMesh(
        Object::ExternalPtr<Picking::PickingMesh>(mesh), xform, pickUserData, 0, 0, nullptr);
    pickingId = (int32_t)meshId;

    static int s_logged = 0;
    if (s_logged < 64) { s_logged++;
        // Dump the transform we hand to AddMesh as floats, next to the owner's MapComponent
        // footprint rect. Measured 2026-07-28: a placed building's pickable region sat at the
        // FAR CORNER of its footprint (hits only at (105,12x) for a rect anchored at
        // (102,122) 4x5) while the model drew around the centre — i.e. the mesh is displaced,
        // not undersized. This line is how you tell "wrong transform" from "small mesh":
        // compare xform's translation against the rect.
        float f[7];
        std::memcpy(f, &xform, sizeof f);   // the first seven floats of the transform (log only)
        int fx = -1, fy = -1, fw = -1, fh = -1;
        {   // owner's MapComponent, for the footprint rect (log only; type match, no IsDerivedFrom)
            const uint32_t s_tidMapPC = MapComponent::TypeId();
            for (int i = 0; i < entity->_componentCount; ++i) {
                Component* c = entity->_components[i].Get();
                if (!c) continue;
                if (c->Type() == s_tidMapPC) {
                    MapComponent* m2 = static_cast<MapComponent*>(c);
                    fx = m2->tileX0; fy = m2->tileY0;
                    fw = m2->tileX1; fh = m2->tileY1;
                    break;
                }
            }
        }
        fprintf(stderr, "[PickMesh] AddMesh entity=%p mesh=%p userData=%p -> meshId=%u | "
                "variant=(%d,%d) xform[0..6]=%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f | "
                "footprint=(%d,%d %dx%d) => cellFromXform=(%.1f,%.1f)\n",
                static_cast<void*>(entity), static_cast<void*>(mesh),
                static_cast<void*>(pickUserData), meshId, (int)meshGroup, (int)meshIndex,
                f[0], f[1], f[2], f[3], f[4], f[5], f[6], fx, fy, fw, fh,
                f[4] + 128.0f, f[5] + 128.0f);
        fflush(stderr);
    }
}

// B4 FORWARD: outside callers in Component/buildcomponent.cpp, buildcomponent_state8.cpp,
// destroycomponent_state6.cpp -> PickingComponent::RefreshMesh
void PickingComponent_RegisterMesh(PickingComponent* pc) {
    if (!pc) return;
    pc->RefreshMesh();
}

// Entity-level convenience (the tree's): find the entity's PickingComponent and refresh
// its mesh. Used by callers that hold an entity rather than the component (player
// placement, the bridge tool).
void PickingComponent_RegisterMeshForEntity(void* entityV) {
    if (!entityV) return;
    ComponentEntity* entity = reinterpret_cast<ComponentEntity*>(entityV);   // B4 FORWARD receiver
    PickingComponent* pc = entity->FindComponent<PickingComponent>();
    if (pc) pc->RefreshMesh();
}

// ============================================================================
// ENDGAME B.3.8 VIS — PickingComponent as a REAL class (PickingComponent.h).
//
// Before: a 48-slot PickingComponent_InertVtable filled per create,
// Inert_ScalarDtor in slot 0 (only ~Internal), pickingId = -1 patched in after
// the zero-fill.
// After: the compiler's 16-slot vtable and a real ctor/dtor chain
// (B38_RULES D1, D2); slot bodies are the ones the old table installed.
// ============================================================================
#include "Object/factory_registration.h"

// 0x140128c80: Component() is the CALL 0x1401188b0; the 0x140164040 stamp is
// the compiler's; pickingId = -1 (so OnRemove's `pickingId >= 0` guard skips the
// RemoveMesh for a component that never registered a mesh), userData = 0,
// flags |= 0xc3.
PickingComponent::PickingComponent() : pickingId(-1), _pad2C{}, userData(nullptr) {
    OrFlagsInCtor(0xc3);
}

// ============================================================
// PickingComponent::Persist (vtable slot [3], 0x140128bb0)
// ============================================================
void PickingComponent::Persist(void* stream) {
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // Persist's stream is void*

    // Serialize the userData InternalPtr
    reinterpret_cast<Object::InternalBasePtr*>(&userData)->Serialize(ser);   // H5: the member is the raw pointer the IBP holds

    // Validate: if not ComponentEntity, null it out
    // ENDGAME B.3.9 CB: retail validates this IBP with IsDerivedFrom(ptr, [0x1401c5d70]) =
    // ComponentEntity::TypeId() (Persist 0x140128bb0, at 0x140128bf9). The tree read 0x1401c72e8 instead -- the NAME
    // field (+8) of Description's record, whose only retail reader is Description's
    // TypeName slot 0x140031c60 -- which is unregistered, so the check never ran.
    if (userData != nullptr) {
        // ?IsDerivedFrom@Base@Object@@ (import slot 0x140152a98)
        if (!Object::IsDerivedFromNullSafe(userData, ComponentEntity::TypeId())) {
            userData = nullptr;
        }
    }
}

// [4] 0x140128c30. PORT GAP (retail 0x140128c30) — B38 D9: retail's 9-byte
// Startup body was never ported or installed (inert stub); kept inert here.
void PickingComponent::Startup() {}

// [6] 0x140128b90 = OnAdded, `JMP 0x140128f50` — dispatched by
// ComponentEntity_OnAddedPass at EntityList_RegisterEntity time, in COMPONENT
// ARRAY ORDER. That order is the whole point: on a WoodHouse, Picking is index
// [5] and Build is index [16], so retail registers the pick mesh while the model
// still carries the FOUNDATION decal (meshIndex 0) and only then does
// BuildComponent's own OnAdded Stage 3 un-frame it to -1.
void PickingComponent::OnAdded() { RefreshMesh(); }

// [7] 0x140128b50 — entity mark-removal FUN_1400c2ea0 broadcasts vtable+0x38 to
// every component whose flags have bit 1 BEFORE the entity is freed; the body
// removes the picking mesh from Picking::Scene (gw+0x70) and resets pickingId.
// Without it a removed building's mesh dangles with userData = the freed entity.
// ============================================================
// PickingComponent::OnRemove (vtable slot [7], 0x140128b50; Ghidra "OnCreate")
// ============================================================
// If pickingId >= 0, calls Picking::Scene::RemoveMesh and resets to -1.

void PickingComponent::OnRemove() {
    if ((int)pickingId >= 0) {
        if (GameUpdateLoop::_singleton != nullptr) {
            Picking::Scene* pickingScene = GameUpdateLoop::_singleton->m_pickingScene;
            // Picking::Scene::RemoveMesh (import slot 0x140152438, Runtime 0x180125290).
            pickingScene->RemoveMesh((uint32_t)pickingId);
        }
        pickingId = -1;
    }
}

// [11] 0x140128ba0. PORT GAP (retail 0x140128ba0, `JMP 0x140128f50`, the same
// body as slot 6) — B38 D9: the tree's table left it inert; kept inert here.
void PickingComponent::Slot11() {}

// create 0x140036da0: MemAlloc(0x38, 8), then the ctor.
void* PickingComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PickingComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) PickingComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* PickingComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) PickingComponent());
}

DEFINE_EXE_TYPE(PickingComponent, "Component")
