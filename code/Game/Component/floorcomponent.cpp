// code/Game/Component/floorcomponent.cpp
// Clean reimplementation of FloorComponent lifecycle methods from Application-x64.exe.
//
// FloorComponent (0x30 bytes) -- Component subclass.
// Fields: floorFlag (byte +0x28), floorId (int32 +0x2C, init -1).
// Manages floor/ground plane rendering for buildings.
//
// Vtable at 0x140163360 (dumped; every other slot is the base Component default):
//   [3] Shutdown  = 0x14011F560 -- serialize floorFlag as bool
//   [6] OnAdded   = 0x14011F340 -- Ghidra "Draw"; place the floor iff desc `_addOnCreate`
//   [7] OnRemove  = 0x14011F2A0 -- Ghidra "OnCreate"; remove the floor mesh
//  [11] (dead)    = 0x14011F540 -- NOT a stub: a real re-place body, but nothing in the
//                                  shipped exe dispatches component slot +0x58. See the
//                                  long note at the factory below before touching it.
//  [12] Message   = 0x14011F520 -- on message 1, place the floor (the path buildings use)
//  [19]           = 0x14011F6E0
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/floorcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"          // GetTypeIdByName
#include "Component/component_factory_base.h"     // the live gate + inert factory helpers
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "picking/Scene.h"     // ENDGAME B.3.7 callers (K2)

// BANISHED_FLOOR_TRACE (code/Game/Game/mapquery.cpp) — shared with the consumer side.

// Standalone-safe type-id resolution. Both ids the retail bodies read live in raw
// .rdata (DAT_1401c7708 FloorDescription, DWORD_1401c79e0 MapComponent); ResolveAddr
// on a .rdata address returns null unless a reg_*.cpp explicitly maps it, and a
// registered-but-unfilled slot reads 0 — either way every IsDerivedFrom/GetTypeId
// compare below would silently fail. Fall back to the standalone type registry.
static uint32_t FloorTypeIdByName(const char* name, uint32_t* rdataSlot) {
    if (rdataSlot && *rdataSlot != 0 && *rdataSlot != 0xFFFFFFFFu) return *rdataSlot;
    uint32_t id = ::Object::GetTypeIdByName(name);
    return (id == 0xFFFFFFFFu) ? 0 : id;
}

static void EnsureSubHelpers();

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base class) -- at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IsDerivedFrom -- dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_d = nullptr;

// FUN_140004680 -- get entity data (owner -> entity data ptr)
typedef void* (*fn_GetEntityData_t)(void* owner, void* outPtr);
static fn_GetEntityData_t s_fn_GetEntityData = nullptr;

// FUN_14009cba0 -- remove floor mesh from picking scene
typedef void (*fn_RemoveFloorMesh_t)(void* scene, uint32_t floorId,
    int x, int y, int w, int h);
static fn_RemoveFloorMesh_t s_fn_RemoveFloorMesh = nullptr;

// FloorComponent_sub_f390 -- floor mesh placement sub-function (0x14011F390)
typedef void (*fn_FloorSubF390_t)(void* self);
static fn_FloorSubF390_t s_fn_FloorSubF390 = nullptr;

// Type ID for floor-related building type (DAT_1401c7708)
static uint32_t* s_pFloorBuildingTypeId = nullptr;

// Game state singleton
static void** s_pGameState = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    uintptr_t slot;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_GetEntityData = reinterpret_cast<fn_GetEntityData_t>(
        ResolveAddr(Module::Exe, 0x140004680));
    s_fn_RemoveFloorMesh = reinterpret_cast<fn_RemoveFloorMesh_t>(
        ResolveAddr(Module::Exe, 0x14009cba0));
    s_fn_FloorSubF390 = reinterpret_cast<fn_FloorSubF390_t>(
        ResolveAddr(Module::Exe, 0x14011F390));
    s_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));
    s_pFloorBuildingTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7708));

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_d = reinterpret_cast<fn_IsDerivedFrom_dispatch_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    s_initialized = true;
}

// ============================================================
// FloorComponent::Shutdown (vtable slot [3], 0x14011F560)
// ============================================================
// Serializes base component, then serializes floorFlag (bool at +0x28).

void FloorComponent_Shutdown(FloorComponent* self, void* stream) {
    EnsureHelpers();

    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(&self->floorFlag), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// FloorComponent::Draw (vtable slot [6], 0x14011F340)
// ============================================================
// Checks IsDerivedFrom on _unk18 with floor building type ID.
// If building[0x28] != 0, calls FloorComponent_sub_f390 for floor mesh placement.
//
// Original disassembly:
//   MOV RBX,[RCX+0x18]              ; get sibling component
//   MOV EDX,DAT_1401c7708           ; floor building type ID
//   CALL [0x140152a98]              ; IsDerivedFrom
//   CMOVNZ RDX,RBX                 ; building = derived ? obj : nullptr
//   CMP byte [RDX+0x28],0          ; check building flag
//   JZ skip
//   CALL FloorComponent_sub_f390

void FloorComponent_Draw(FloorComponent* self) {
    EnsureHelpers();
    EnsureSubHelpers();

    const uint32_t descTid = FloorTypeIdByName("FloorDescription", s_pFloorBuildingTypeId);
    if (!s_fn_IsDerivedFrom_d || !descTid) {
        return;
    }

    void* obj = self->_description;
    bool isDerived = s_fn_IsDerivedFrom_d(obj, descTid);
    void* building = isDerived ? obj : nullptr;

    // Retail dereferences `building+0x28` unconditionally (CMOVNZ leaves RDX=0 on a
    // failed cast and it reads [0x28]) because its data guarantees a FloorComponent's
    // +0x18 IS a FloorDescription. Keep the null test — a standalone entity can carry
    // the component without the description surviving the copy-ctor.
    if (!building) {
        return;
    }

    // desc+0x28 is the FloorDescription bool — "this structure has a walkable floor".
    uint8_t flag = *(uint8_t*)((uintptr_t)building + 0x28);
    if (flag != 0) {
        if (s_fn_FloorSubF390) {
            s_fn_FloorSubF390(self);
        }
    }
}

// ============================================================
// FloorComponent::OnCreate (vtable slot [7], 0x14011F2A0)
// ============================================================
// If floorId >= 0, removes the existing floor mesh from the picking
// scene, then resets floorId to -1 and floorFlag to 0.

void FloorComponent_OnCreate(FloorComponent* self) {
    EnsureHelpers();

    if (self->floorId >= 0) {
        // Get scene from game state
        int* pScene = *(int**)((uintptr_t)*s_pGameState + 0xF8);

        // Check owner pointer

        // Get entity data from owner
        uintptr_t entityData = 0;
        if (s_fn_GetEntityData) {
            s_fn_GetEntityData(*(void**)((uintptr_t)self + 0x10), &entityData);
        }

        // [SA-GUARD] Same MapComponent-may-be-missing case as f390: without the
        // footprint we cannot decrement the per-cell counters, but we must still drop
        // the mesh and reset the id or the scene keeps a dangling deck.
        if (entityData != 0) {
            int16_t x = *(int16_t*)(entityData + 0x28);
            int16_t y = *(int16_t*)(entityData + 0x2a);
            int16_t w = *(int16_t*)(entityData + 0x2c);
            int16_t h = *(int16_t*)(entityData + 0x2e);

            if (s_fn_RemoveFloorMesh) {
                s_fn_RemoveFloorMesh(pScene, (uint32_t)self->floorId,
                    (int)x, (int)y, (int)w, (int)h);
            }
        }

        self->floorId = -1;
        self->floorFlag = 0;
    }
}

// ============================================================
// Reimplemented exe helpers (callee bodies for the guarded call sites above)
// ============================================================

// Type ID globals used by the helpers.
// DWORD_1401c79e0 -- NaturalResource entity-data component type ID (FindComponentByType).
static uint32_t* s_pNatResTypeId = nullptr;
// DAT_1401c7708 -- FloorDescription type ID (already resolved as s_pFloorBuildingTypeId).
// Picking::Scene::AddMesh (Runtime export) + the ModelComponent finder.
//
// ⚠ The finder is Entity_ClearPathQueue = FUN_140003f60 (type id DWORD_1401c7a50 =
// ModelComponent), NOT 0x140005200. This file resolved 0x140005200 for a long time —
// that address is a DIFFERENT same-shaped finder that searches for DAT_1401c4720
// (ParticleComponent). Ghidra prints the label "Entity_ClearPathQueue" in f390's
// pseudo-C but the CALL at 0x14011f41c targets 0x140003f60; the wrong one would have
// returned a ParticleComponent (or null) and f390 would then have read a Transform out
// of the wrong object's +0x148. Disassembly-verified 2026-08-29.
typedef void* (*fn_EntityAccessor_t)(void* entity, void* outIBP);
static fn_EntityAccessor_t s_fn_EntityAccessor = nullptr;

static bool s_subResolved = false;
static void EnsureSubHelpers() {
    if (s_subResolved) return;
    s_pNatResTypeId   = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c79e0));
    s_fn_EntityAccessor = reinterpret_cast<fn_EntityAccessor_t>(ResolveAddr(Module::Exe, 0x140003f60));
    // Picking::Scene::AddMesh / RemoveMesh — Runtime exports.
    // 0x1801252e0, NOT 0x1800f8d80. The latter is int3 PADDING, not a function entry —
    // calling it raises a breakpoint exception. It survived only because floors are never
    // placed on the autostart path (the note at citizenai_executor.cpp:316 has said so for
    // a while); that assumption expired on 2026-08-01 when player building placement started
    // working, so this was a live landmine on the first hand-placed floor.
    // Signature verified from exe Entity_impl_8f50 (0x140128f50):
    //   uint AddMesh(Scene*, ExternalPtr<PickingMesh>, Transform*, ownerIBP, int, int, cb*)
    // ⚠ OPEN: the call below passes 0 for the OWNER argument, so a floor's pick mesh has no
    // owner entity. That is a separate, untested question from the address — flagging rather
    // than silently changing it.
    // 0x180125290 = Picking::Scene::RemoveMesh (Ghidra-verified function START).
    // The previous 0x1800f8f50 was MID-FUNCTION inside Frustum::Contains — every
    // floor removal jumped into its middle with garbage registers (found in the
    // save/load-wave mid-function address sweep, 2026-08-28).
    s_subResolved = true;
}

// Reimpl of FUN_140004680 (FindComponentByType on owner+0x30 array; returns IBP wrapping found component)
void* FloorComponent_FindResData(void* owner, void* outIBP) {
    EnsureHelpers(); EnsureSubHelpers();
    uintptr_t o = reinterpret_cast<uintptr_t>(owner);
    void* found = nullptr;
    uint32_t wantTypeId = FloorTypeIdByName("MapComponent", s_pNatResTypeId);
    int count = *reinterpret_cast<int*>(o + 0x38);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(o + 0x30);
    for (int i = 0; i < count; ++i) {
        void** slot = reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
        void* comp = *slot;
        // Virtual GetTypeId at vtable+0x10.
        uintptr_t vt = *reinterpret_cast<uintptr_t*>(comp);
        typedef uint32_t (*fn_GetTypeId_t)(void* self);
        fn_GetTypeId_t getId = *reinterpret_cast<fn_GetTypeId_t*>(vt + 0x10);
        uint32_t tid = getId ? getId(comp) : 0;
        if (tid == wantTypeId) {
            void* c = *slot;
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, wantTypeId)) found = c;
            break;
        }
    }
    if (fn_InternalBasePtr_Ctor) new (outIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
    return outIBP;
}

// Reimpl of FUN_14009cba0 (decrement per-tile floor counter over w*h footprint, then Scene::RemoveMesh)
void FloorComponent_RemoveFloorMesh(void* scene, uint32_t floorId,
                                    int x, int y, int w, int h) {
    EnsureHelpers(); EnsureSubHelpers();
    int* mapInfo = reinterpret_cast<int*>(scene);
    int mapW = *mapInfo;
    uintptr_t grid = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(mapInfo) + 0x80);
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            int cell = (j + y) * mapW + i;
            uint8_t* p = reinterpret_cast<uint8_t*>(grid + 0x2d + (uintptr_t)(cell + x) * 0x30);
            *p = (uint8_t)(*p - 1);
        }
    }
    // Picking::Scene is at (mapInfo + 2 ints) = scene + 8.
    reinterpret_cast<Picking::Scene*>(reinterpret_cast<uintptr_t>(mapInfo) + 8)->RemoveMesh(floorId);   // B.3.7 bridge
}

// Reimpl of FUN_14011f390 (place floor mesh: increment footprint counters, add mesh to Picking::Scene)
void FloorComponent_SubF390(void* selfV) {
    EnsureHelpers(); EnsureSubHelpers();
    uintptr_t* self = reinterpret_cast<uintptr_t*>(selfV);
    uint8_t* sb = reinterpret_cast<uint8_t*>(selfV);

    // If floorId (+0x2c) >= 0, remove the existing mesh first via vtable slot +0x38.
    if (*reinterpret_cast<int*>(sb + 0x2c) >= 0) {
        uintptr_t vt = *self;
        typedef void (*fn_slot38_t)(void* self);
        fn_slot38_t slot38 = *reinterpret_cast<fn_slot38_t*>(vt + 0x38);
        if (slot38) slot38(selfV);
    }

    uintptr_t world = reinterpret_cast<uintptr_t>(*s_pGameState);
    int* mapInfo = *reinterpret_cast<int**>(world + 0xF8);

    int64_t resIBP = 0;
    if (s_fn_GetEntityData) s_fn_GetEntityData(*reinterpret_cast<void**>(self + 2), &resIBP);
    // [SA-GUARD] Retail reads the footprint shorts unguarded — its MapComponent is always
    // there. In standalone the find can miss (the type registry can lag), and a null here
    // would stamp cell records at address 0x28.
    if (resIBP == 0) {
        return;
    }
    int16_t baseX = *reinterpret_cast<int16_t*>(resIBP + 0x28);
    int16_t baseY = *reinterpret_cast<int16_t*>(resIBP + 0x2a);
    int16_t fw    = *reinterpret_cast<int16_t*>(resIBP + 0x2c);
    int16_t fh    = *reinterpret_cast<int16_t*>(resIBP + 0x2e);

    int64_t entAcc = 0;
    if (s_fn_EntityAccessor) s_fn_EntityAccessor(*reinterpret_cast<void**>(self + 2), &entAcc);
    uintptr_t ent = *reinterpret_cast<uintptr_t*>(&entAcc);

    // [SA-SCOPED FALLBACK] The finder's type id is the shared raw .rdata 0x1401c7a50
    // (ModelComponent), which reg_pipeline_typeids fills only behind BANISHED_LIVE_DECAL.
    // Scan the owner ourselves when it comes back empty — the transform below is the
    // floor mesh's world placement, so a null here parks the deck at address 0x148.
    if (ent == 0) {
        static uint32_t s_tidModel = 0;
        if (s_tidModel == 0 || s_tidModel == 0xFFFFFFFFu)
            s_tidModel = ::Object::GetTypeIdByName("ModelComponent");
        uintptr_t owner = *reinterpret_cast<uintptr_t*>(self + 2);
        if (owner && s_tidModel && s_tidModel != 0xFFFFFFFFu) {
            int n = *reinterpret_cast<int*>(owner + 0x38);
            uintptr_t arr = *reinterpret_cast<uintptr_t*>(owner + 0x30);
            for (int k = 0; k < n && arr; ++k) {
                void* c = *reinterpret_cast<void**>(arr + (uintptr_t)k * 8);
                if (!c) continue;
                uintptr_t vt = *reinterpret_cast<uintptr_t*>(c);
                typedef uint32_t (*getTid_t)(void*);
                if ((*reinterpret_cast<getTid_t*>(vt + 0x10))(c) == s_tidModel) {
                    ent = reinterpret_cast<uintptr_t>(c);
                    break;
                }
            }
        }
    }
    if (ent == 0) {
        return;
    }

    // Copy the 7-uint transform block from ent+0x148..+0x160.
    uint32_t xform[7];
    for (int k = 0; k < 7; ++k)
        xform[k] = *reinterpret_cast<uint32_t*>(ent + 0x148 + (uintptr_t)k * 4);

    void* building = *reinterpret_cast<void**>(self + 3);
    const uint32_t descTid = FloorTypeIdByName("FloorDescription", s_pFloorBuildingTypeId);
    void* meshOwner = (descTid && fn_IsDerivedFrom &&
                       fn_IsDerivedFrom(building, descTid)) ? building : nullptr;
    void* mesh = meshOwner ? *reinterpret_cast<void**>((uintptr_t)meshOwner + 0x20) : nullptr;
    // desc+0x20 is the PickingMesh ExternalPtr (FishermansDockFloor.rsc = the dock's
    // "floor" sub-object). Without it there is nothing for the height ray to hit, and
    // bumping the per-cell counters would then make every f3e0 on those cells pay for a
    // ray-cast that can only miss — so bail before touching the grid.
    if (!mesh) {
        return;
    }

    uintptr_t grid = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(mapInfo) + 0x80);
    int mapW = *mapInfo;
    for (int j = 0; j < (int)fh; ++j) {
        for (int i = 0; i < (int)fw; ++i) {
            int cell = (j + baseY) * mapW + i + (int)baseX;
            uint8_t* p = reinterpret_cast<uint8_t*>(grid + 0x2d + (uintptr_t)cell * 0x30);
            *p = (uint8_t)(*p + 1);
        }
    }

    uint32_t meshId = 0;
    meshId = reinterpret_cast<Picking::Scene*>(reinterpret_cast<uintptr_t>(mapInfo) + 8)->AddMesh(   // B.3.7 bridge
        Object::ExternalPtr<Picking::PickingMesh>(reinterpret_cast<Picking::PickingMesh*>(mesh)),
        *reinterpret_cast<const Math::Transform*>(xform), nullptr, 0, 0, nullptr);
    *reinterpret_cast<uint8_t*>(self + 5) = 1;
    *reinterpret_cast<uint32_t*>(sb + 0x2c) = meshId;

}

// ============================================================
// Init: resolve function pointers
// ============================================================
void FloorComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[FloorComponent] Funcs resolved: Serialize=%p FloorSub=%p RemoveMesh=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_FloorSubF390,
           (void*)s_fn_RemoveFloorMesh);
#endif
}

// ===== Standalone factory =====
//
// Hand-rolled rather than COMPONENT_LIVE_FACTORY_ONADDED because this component needs
// THREE real slots plus a non-zero field default, and no macro covers that shape:
//
//   [3]  +0x18  Serialize   FloorComponent_Shutdown  (floorFlag round-trips a save)
//   [6]  +0x30  OnAdded     FloorComponent_Draw      (Ghidra's "Draw" — dispatched by
//                                                     ComponentEntity_OnAddedPass; the
//                                                     ctor's flags bit 0 is what enrols it)
//   [7]  +0x38  OnRemove    FloorComponent_OnCreate  (Ghidra's "OnCreate" — the usual
//                                                     OnCreate/OnRemove off-by-one; the
//                                                     body REMOVES the mesh, and f390
//                                                     calls this very slot itself)
//  [12]  +0x60  Message     FloorComponent_Slot12    (0x14011f520 — on msg 1, place)
//
// SLOT 11 (+0x58, 0x14011f540) IS THE LOAD-TIME RE-PLACE — WIRED (corrected 2026-08-29).
// Body: `if (floorFlag == 0) return;` then a verbatim inline f390 — "the save said this
// component had a floor placed, so place it again with the current transform". That is
// exactly why floorFlag is the ONE FloorComponent field the serializer persists.
//
// ⚠ THIS BLOCK PREVIOUSLY CLAIMED THE SLOT WAS DEAD IN RETAIL. That was wrong, and the
// mistake is instructive: the earlier sweep looked only for ENTITY-level dispatchers of
// +0x58, found FUN_1400c2bf0 (which really does have zero callers — its lone xref
// 0x1401ebc20 is its own .pdata unwind record), and stopped. A byte-pattern scan for
// `FF 50 58` finds a SECOND dispatcher: 0x1400c47c5, inside FUN_1400c4730 — the
// load-only EntityList post-load pass (`if (stream+8 == 0)`), which
// GameUpdateLoop::Serialize (0x140095a90) calls at 0x140095d3f. We already port and call
// that pass (EntityList_PostLoad_c4730, gameupdateloop_serialize.cpp), so the dispatcher
// was live all along and only the slot was missing.
//
// Symptom while unwired: after a LOAD, dock/bridge walkers fall through to the riverbed.
// cell+0x2d (the per-cell floor counter) IS restored — FUN_14001ac10 serializes it — so
// f3e0 (0x14009f3e0) enters its refine branch, but the deck mesh was never re-added to
// the map's Picking::Scene, so Intersect misses and Z falls back to terrain height.
// New games were unaffected because slot 12 / msg 1 places the floor at build time.
//
// SLOT 6 IS NOT THE PATH FOR BUILDINGS. It gates on the FloorDescription bool at +0x28,
// which the .rsc calls `_addOnCreate`, and ONLY the bridge templates set it:
//   WoodBridge*.rsc      FloorDescription { _mesh = ...; bool _addOnCreate = true; }
//   FishermansDock.rsc   FloorDescription { _mesh = ...; }          <- absent => false
// (same for IronMine / SchoolHouse / TradingPost). A dock's deck is placed by slot 12
// instead, on message 1 — the "this structure is now solid" broadcast that
// ModelComponent_helper_2b70(entity, 1) sends from the ClearArea state-2 enter, and the
// same message MapComponent's slot 12 uses to claim its footprint cells. Wiring only
// slot 6 logs `enabled=0` and places nothing, which is what the first run of this change
// did.
//
// floorId defaults to -1, not 0. The inert factory zeroes the body, and f390 opens with
// `if (floorId >= 0) <remove the existing mesh>` — a zeroed id would make the very first
// placement try to remove picking-mesh 0 (someone else's mesh) before adding its own.
//
// Flags: FloorComponent_Ctor 0x14011f636 is `OR EAX,0x43` over Component's ctor value 0x80
// (0x1401188e6), i.e. 0xC3 like every other component's bit 7. (An earlier note here
// read the OR alone as the whole word and stored 0x43.)
//
// Gated on BANISHED_LIVE_FLOOR, DEFAULT-ON since 2026-08-29 (the default-on table) — the env
// var is now a kill-switch (=0) for A/B runs. With it off this is byte-for-byte the old
// inert component. What it turns on: docks/bridges register their
// deck mesh into the map's Picking::Scene and bump each covered cell's floor counter
// (+0x2d), which is what makes the height query in mapquery.cpp stand a citizen ON the
// deck instead of on the terrain beneath it.
//
// ENDGAME B.3.8 VIS — FloorComponent is a REAL class now (FloorComponent.h): the
// 48-slot FloorComponent_LiveVtable, its per-create fill and InitBody patch are
// gone; the compiler's 16-slot vtable carries the slots above.
#include "Object/factory_registration.h"
#include "Component/FloorComponent.h"

// 0x14011f600: Component() is the CALL 0x1401188b0; the 0x140163360 stamp is
// the compiler's; +0x28 = 0, +0x2c = -1 (0x14011f62f), flags |= 0x43.
// flags |= 0x43 over Component's 0x80 (=> 0xc3), as retail 0x14011f636.
FloorComponent::FloorComponent() : floorFlag(0), _pad29{}, floorId(-1) {
    OrFlagsInCtor(0x43);
}

// [3] 0x14011f560 — floorFlag round-trips a save.
void FloorComponent::Persist(void* stream) { FloorComponent_Shutdown(this, stream); }
// [6] 0x14011f340
void FloorComponent::OnAdded() { FloorComponent_Draw(this); }
// [7] 0x14011f2a0
void FloorComponent::OnRemove() { FloorComponent_OnCreate(this); }
// [12] 0x14011f520: `if (msg != 1) return;` followed by a verbatim inline copy
// of f390 — so delegate rather than duplicate the body.
void FloorComponent::OnEvent(int msgId) {
    if (msgId != 1) return;
    FloorComponent_SubF390(this);
}

// FloorComponent slot 11 (+0x58, 0x14011f540): the LOAD-TIME re-place. Retail's body is
// `if (floorFlag == 0) return;` followed by a verbatim inline copy of f390 — so delegate
// rather than duplicate, exactly as slot 12 does.
void FloorComponent::Slot11() {
    if (floorFlag == 0) return;
    FloorComponent_SubF390(this);
}

// [0] 0x1400347f0: ~FloorComponent (0x14011f5c0: restamp + ~Internal), MemFree on &1.
Object::Base* FloorComponent::DeletingDtor(unsigned flags) {
    this->~FloorComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140034600: MemAlloc(0x30, 8), then the ctor.
void* FloorComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(FloorComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) FloorComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* FloorComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) FloorComponent());
}

DEFINE_EXE_VTABLE(FloorComponent, "_ZTV14FloorComponent")
DEFINE_EXE_TYPE(FloorComponent, "Component")
