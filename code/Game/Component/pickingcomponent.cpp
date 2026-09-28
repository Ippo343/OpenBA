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
#include "math/Math.h"
#include "object/External.h"
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "resolve.h"
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

// Self-contained ModelComponent find over the entity's component array (entity+0x30
// array ptr / +0x38 count; each slot is an IBP). Mirrors ComponentEntity::FindComponent
// (the mislabeled Entity_ClearPathQueue, 0x140003f60).
static void* PC_FindModelComponent(void* entity) {
    if (!entity) return nullptr;
    uint32_t tid = ModelComponent::TypeId();
    if (tid == 0) return nullptr;
    uint8_t* e = (uint8_t*)entity;
    int count = *(int32_t*)(e + 0x38);
    uintptr_t arr = *(uintptr_t*)(e + 0x30);
    for (int i = 0; i < count; ++i) {
        void** slot = (void**)(arr + (uintptr_t)i * 8);
        void* comp = *slot;
        if (!comp) continue;
        if (reinterpret_cast<Object::Base*>(comp)->Type() == tid) return comp;   // B.3.8 bridge (slot 2)
    }
    return nullptr;
}


void PickingComponent_RegisterMesh(void* pc) {
    if (!pc) return;
    if (GameUpdateLoop::_singleton == nullptr) return;

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
    reinterpret_cast<PickingComponent*>(pc)->OnRemove();   // B.3.8 bridge (retail: its own vtable +0x38)
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

    void* scene = *(void**)((uintptr_t)GameUpdateLoop::_singleton + 0x70);
    if (!scene) return;

    // Picking::Scene::AddMesh (Runtime 0x1801252e0): the ExternalPtr is a bare
    // register value (B7-5), the Transform by reference, three trailing zeros.
    uint32_t meshId = reinterpret_cast<Picking::Scene*>(scene)->AddMesh(   // B.3.7 bridge
        Object::ExternalPtr<Picking::PickingMesh>(reinterpret_cast<Picking::PickingMesh*>(mesh)),
        *reinterpret_cast<const Math::Transform*>(xform), userData, 0, 0, nullptr);
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
            const uint32_t s_tidMapPC = MapComponent::TypeId();
            uint8_t* e = (uint8_t*)entity;
            int n = *(int32_t*)(e + 0x38);
            uintptr_t arr = *(uintptr_t*)(e + 0x30);
            for (int i = 0; i < n && arr; ++i) {
                void** slot = (void**)(arr + (uintptr_t)i * 8);
                void* c = *slot; if (!c) continue;
                if (reinterpret_cast<Object::Base*>(c)->Type() == s_tidMapPC) {   // B.3.8 bridge (slot 2)
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

// Entity-level convenience: find the entity's PickingComponent and register its mesh.
// Used by callers that hold an entity rather than the component (player placement, etc.).
void PickingComponent_RegisterMeshForEntity(void* entity) {
    if (!entity) return;
    const uint32_t s_tidPickComponent = PickingComponent::TypeId();
    if (s_tidPickComponent == 0) return;
    uint8_t* e = (uint8_t*)entity;
    int count = *(int32_t*)(e + 0x38);
    uintptr_t arr = *(uintptr_t*)(e + 0x30);
    for (int i = 0; i < count; ++i) {
        void** slot = (void**)(arr + (uintptr_t)i * 8);
        void* comp = *slot;
        if (!comp) continue;
        if (reinterpret_cast<Object::Base*>(comp)->Type() == s_tidPickComponent) {   // B.3.8 bridge (slot 2)
            PickingComponent_RegisterMesh(comp);
            return;
        }
    }
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
#include "Component/PickingComponent.h"

// 0x140128c80: Component() is the CALL 0x1401188b0; the 0x140164040 stamp is
// the compiler's; +0x28 = -1 (so OnRemove's `pickingId >= 0` guard skips the
// RemoveMesh for a component that never registered a mesh), +0x30 = 0,
// flags |= 0xc3.
PickingComponent::PickingComponent() : pickingId(-1), _pad2C{}, field30(nullptr) {
    OrFlagsInCtor(0xc3);
}

// ============================================================
// PickingComponent::Persist (vtable slot [3], 0x140128bb0)
// ============================================================
void PickingComponent::Persist(void* stream) {
    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(this);

    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize IBP at +0x30
    reinterpret_cast<Object::InternalBasePtr*>((void*)(selfAddr + 0x30))->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge

    // Validate: if not ComponentEntity, null it out
    // ENDGAME B.3.9 CB: retail validates this IBP with IsDerivedFrom(ptr, [0x1401c5d70]) =
    // ComponentEntity::TypeId() (Persist 0x140128bb0, at 0x140128bf9). The tree read 0x1401c72e8 instead -- the NAME
    // field (+8) of Description's record, whose only retail reader is Description's
    // TypeName slot 0x140031c60 -- which is unregistered, so the check never ran.
    void* ibpVal = *(void**)(selfAddr + 0x30);
    if (ibpVal != nullptr) {
        // ?IsDerivedFrom@Base@Object@@ (import slot 0x140152a98)
        if (!reinterpret_cast<const Object::Base*>(ibpVal)->IsDerivedFrom(ComponentEntity::TypeId())) {   // B.3.x bridge
            *(uintptr_t*)(selfAddr + 0x30) = 0;
        }
    }

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// [4] 0x140128c30. PORT GAP (retail 0x140128c30) — B38 D9: retail's 9-byte
// Startup body was never ported or installed (inert stub); kept inert here.
void PickingComponent::Startup() {}

// [6] 0x140128b90 = OnAdded, `JMP 0x140128f50` — dispatched by
// ComponentEntity_OnAddedPass at EntityList_RegisterEntity time, in COMPONENT
// ARRAY ORDER. That order is the whole point: on a WoodHouse, Picking is index
// [5] and Build is index [16], so retail registers the pick mesh while the model
// still carries the FOUNDATION decal (modelVariant 0) and only then does
// BuildComponent's own OnAdded Stage 3 un-frame it to -1.
void PickingComponent::OnAdded() { PickingComponent_RegisterMesh(this); }

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
            void* pickingScene = *(void**)((uintptr_t)GameUpdateLoop::_singleton + 0x70);
            // Picking::Scene::RemoveMesh (import slot 0x140152438, Runtime 0x180125290).
            reinterpret_cast<Picking::Scene*>(pickingScene)->RemoveMesh((uint32_t)pickingId);   // B.3.7 bridge
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
