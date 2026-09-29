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
#include "Component/MapComponent.h"   // B.3.8 CB1a (TypeId)
#include "Component/FloorDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/ModelComponent.h"   // B.3.8 CB1a (TypeId)
#include "math/Math.h"
#include "object/External.h"
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Object/factory_registration.h"          // GetTypeIdByName
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "picking/Scene.h"     // ENDGAME B.3.7 callers (K2)
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Entity/ComponentEntity.h"   // ComponentEntity::FindComponent<T>
#include "Game/Map.h"
#include <cstring>

// BANISHED_FLOOR_TRACE (code/Game/Game/mapquery.cpp) — shared with the consumer side.

// ============================================================
// Helpers, called directly (B.3R): 0x140004680 FloorComponent_FindResData,
// 0x14009cba0 FloorComponent_RemoveFloorMesh, 0x14011f390 FloorComponent_SubF390
// (all three defined below), the import slot 0x140152a98 Object::Base::IsDerivedFrom,
// and the game-world cell 0x1401c3dd8 read through g_pGameWorldSingleton.
// ============================================================

// ============================================================
// FloorComponent::Shutdown (vtable slot [3], 0x14011F560)
// ============================================================
// Serializes base component, then serializes floorFlag (bool at +0x28).

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

// ============================================================
// FloorComponent::OnCreate (vtable slot [7], 0x14011F2A0)
// ============================================================
// If floorId >= 0, removes the existing floor mesh from the picking
// scene, then resets floorId to -1 and floorFlag to 0.

// ============================================================
// Reimplemented exe helpers (callee bodies for the guarded call sites above)
// ============================================================

// ENDGAME B.3.9 C1 (D5): 0x1401c79e0 is MapComponent (INVENTORY_C Table 1, not
// "NaturalResource" as the old comment/local name guessed) -- resolved but never
// dereferenced anywhere in this file, so removed outright rather than converted.
// Picking::Scene::AddMesh (Runtime export) + the ModelComponent finder.
//
// ⚠ The finder is Entity_ClearPathQueue = FUN_140003f60 (type id DWORD_1401c7a50 =
// ModelComponent), NOT 0x140005200. This file resolved 0x140005200 for a long time —
// that address is a DIFFERENT same-shaped finder that searches for DAT_1401c4720
// (ParticleComponent). Ghidra prints the label "Entity_ClearPathQueue" in f390's
// pseudo-C but the CALL at 0x14011f41c targets 0x140003f60; the wrong one would have
// returned a ParticleComponent (or null) and f390 would then have read a Transform out
// of the wrong object's +0x148. Disassembly-verified 2026-08-29.
// It is called directly: ActCNR_FindComponent_3f60.
//
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

// Reimpl of FUN_140004680 (FindComponentByType on owner+0x30 array; returns IBP wrapping found component)
void* FloorComponent_FindResData(void* owner, void* outIBP) {
    return new (outIBP) ::Object::InternalPtr<MapComponent>(
        reinterpret_cast<ComponentEntity*>(owner)->FindComponent<MapComponent>());   // B4 FORWARD: receiver typed by the callers' blocks
}

// Map::RemoveFloor — FUN_14009cba0: decrement the per-cell floor counter (cell +0x2d)
// over the w*h footprint, then Picking::Scene::RemoveMesh. Map's cells (0x30-byte
// records) and its embedded Picking::Scene are raw storage (GAME-b, H5): LEFT.
void Map::RemoveFloor(uint32_t floorId, int x, int y, int w, int h) {
    uint8_t* grid = reinterpret_cast<uint8_t*>(cells);
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            int cell = (j + y) * size + i;
            uint8_t* p = grid + 0x2d + static_cast<uintptr_t>(cell + x) * 0x30;
            *p = static_cast<uint8_t>(*p - 1);
        }
    }
    reinterpret_cast<Picking::Scene*>(pickingScene)->RemoveMesh(floorId);   // +0x08
}

// FloorComponent::PlaceFloor — FUN_14011f390: increment the footprint's per-cell floor
// counters and add the description's pick mesh to the map's Picking::Scene.
void FloorComponent::PlaceFloor() {
    // If floorId >= 0, remove the existing mesh first (retail: the virtual slot 7).
    if (floorId >= 0) {
        OnRemove();
    }

    // GameUpdateLoop::m_game (+0xF8) is still void* (GAME-a): LEFT.
    Map* map = reinterpret_cast<Map*>(GameUpdateLoop::_singleton->m_game);

    MapComponent* mc = _entity->FindComponent<MapComponent>();   // 0x140004680
    // [SA-GUARD] Retail reads the footprint shorts unguarded — its MapComponent is always
    // there. In standalone the find can miss (the type registry can lag), and a null here
    // would stamp cell records at address 0x28.
    if (mc == nullptr) {
        return;
    }
    int16_t baseX = static_cast<int16_t>(mc->tileX0);
    int16_t baseY = static_cast<int16_t>(mc->tileY0);
    int16_t fw    = static_cast<int16_t>(mc->tileX1);
    int16_t fh    = static_cast<int16_t>(mc->tileY1);

    // FindComponent<ModelComponent> (0x140003f60). [SA-GUARD] a missing model returns
    // before the transform read below (the tree's; retail reads it unguarded).
    ModelComponent* model = _entity->FindComponent<ModelComponent>();
    if (model == nullptr) {
        return;
    }

    // Copy the 7-uint transform block from ModelComponent +0x148..+0x160 (raw storage,
    // COMP-d H5).
    Math::Transform xform;
    static_assert(sizeof(xform) == sizeof(model->transform), "Math::Transform is 0x1C");
    std::memcpy(&xform, model->transform, sizeof(xform));

    Description* d = _description;
    const uint32_t descTid = FloorDescription::TypeId();
    FloorDescription* desc = (descTid && ::Object::IsDerivedFromNullSafe(d, descTid))
                                 ? static_cast<FloorDescription*>(d) : nullptr;
    Picking::PickingMesh* mesh = desc ? desc->_mesh : nullptr;
    // _mesh is the PickingMesh ExternalPtr (FishermansDockFloor.rsc = the dock's
    // "floor" sub-object). Without it there is nothing for the height ray to hit, and
    // bumping the per-cell counters would then make every f3e0 on those cells pay for a
    // ray-cast that can only miss — so bail before touching the grid.
    if (!mesh) {
        return;
    }

    uint8_t* grid = reinterpret_cast<uint8_t*>(map->cells);   // LEFT (GAME-b)
    int mapW = map->size;
    for (int j = 0; j < (int)fh; ++j) {
        for (int i = 0; i < (int)fw; ++i) {
            int cell = (j + baseY) * mapW + i + (int)baseX;
            uint8_t* p = grid + 0x2d + static_cast<uintptr_t>(cell) * 0x30;
            *p = static_cast<uint8_t>(*p + 1);
        }
    }

    uint32_t meshId = reinterpret_cast<Picking::Scene*>(map->pickingScene)->AddMesh(   // LEFT (Map +0x08 raw, H5)
        Object::ExternalPtr<Picking::PickingMesh>(mesh), xform, nullptr, 0, 0, nullptr);
    floorFlag = true;
    floorId = static_cast<int32_t>(meshId);
}

// B4 FORWARD: multiobjectcomponent.cpp.
void FloorComponent_SubF390(void* selfV) {
    reinterpret_cast<FloorComponent*>(selfV)->PlaceFloor();
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
FloorComponent::FloorComponent() : floorFlag(false), _pad29{}, floorId(-1) {
    OrFlagsInCtor(0x43);
}

// [3] 0x14011f560 — floorFlag round-trips a save.
void FloorComponent::Persist(void* stream) {
    Component::Persist(stream);   // retail's direct CALL 0x1401189b0

    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (Persist takes void*)
    ser.Serialize(floorFlag, nullptr);
}
// [6] 0x14011f340
void FloorComponent::OnAdded() {
    const uint32_t descTid = FloorDescription::TypeId();
    if (!descTid) {
        return;
    }

    Description* obj = _description;
    bool isDerived = obj->IsDerivedFrom(descTid);
    FloorDescription* building = isDerived ? static_cast<FloorDescription*>(obj) : nullptr;

    // Retail dereferences `building+0x28` unconditionally (CMOVNZ leaves RDX=0 on a
    // failed cast and it reads [0x28]) because its data guarantees a FloorComponent's
    // +0x18 IS a FloorDescription. Keep the null test — a standalone entity can carry
    // the component without the description surviving the copy-ctor.
    if (!building) {
        return;
    }

    // _addOnCreate — "place this structure's walkable floor on creation" (bridges).
    if (building->_addOnCreate) {
        PlaceFloor();   // 0x14011f390
    }
}
// [7] 0x14011f2a0
void FloorComponent::OnRemove() {
    if (floorId >= 0) {
        // GameUpdateLoop::m_game (+0xF8) is still void* (GAME-a): LEFT.
        Map* map = reinterpret_cast<Map*>(GameUpdateLoop::_singleton->m_game);

        MapComponent* mc = _entity->FindComponent<MapComponent>();   // 0x140004680

        // [SA-GUARD] Same MapComponent-may-be-missing case as PlaceFloor: without the
        // footprint we cannot decrement the per-cell counters, but we must still drop
        // the mesh and reset the id or the scene keeps a dangling deck.
        if (mc != nullptr) {
            int16_t x = static_cast<int16_t>(mc->tileX0);
            int16_t y = static_cast<int16_t>(mc->tileY0);
            int16_t w = static_cast<int16_t>(mc->tileX1);
            int16_t h = static_cast<int16_t>(mc->tileY1);

            map->RemoveFloor(static_cast<uint32_t>(floorId),   // 0x14009cba0
                (int)x, (int)y, (int)w, (int)h);
        }

        floorId = -1;
        floorFlag = 0;
    }
}
// [12] 0x14011f520: `if (msg != 1) return;` followed by a verbatim inline copy
// of f390 — so delegate rather than duplicate the body.
void FloorComponent::OnEvent(int msgId) {
    if (msgId != 1) return;
    PlaceFloor();
}

// FloorComponent slot 11 (+0x58, 0x14011f540): the LOAD-TIME re-place. Retail's body is
// `if (floorFlag == 0) return;` followed by a verbatim inline copy of f390 — so delegate
// rather than duplicate, exactly as slot 12 does.
void FloorComponent::Slot11() {
    if (!floorFlag) return;
    PlaceFloor();
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

DEFINE_EXE_TYPE(FloorComponent, "Component")
