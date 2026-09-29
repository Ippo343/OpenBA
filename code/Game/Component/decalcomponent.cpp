// code/Game/Component/decalcomponent.cpp
// Clean reimplementation of DecalComponent lifecycle methods.
//
// DecalComponent (0x38 bytes) manages ground decals for buildings (shadows, footprints).
// Fields: +0x28=decalState (uint32), +0x2C=decalId (int32, -1=none), +0x30=decalScale (float, 1.0)
//
// Vtable at 0x140162fd0. Non-trivial overrides:
//   Shutdown  (0x14011b820) -- Serializes decalScale and decalState
//   Serialize (0x14011b7d0) -- Reads building data, copies scale from data+0x20
//   Draw      (0x14011b560) -- Complex terrain decal rendering (111 instrs)
//   OnCreate  (0x14011b510) -- Removes decal from terrain if decalId >= 0
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/decalcomponent_funcs.h"
#include "Component/ModelComponent.h"   // B.3.8 CB1a (TypeId)
#include "Component/DecalDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/coll_bridge.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Game/exe_constants.h"   // kRdata_* (B3R Leftovers: the .rdata constants)
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (SA type-id fallback)
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Component/MapComponent.h"
#include "Entity/ComponentEntity.h"
#include "Game/WorldGeometry.h"
#include <cstring>

// ============================================================
// Helpers, called directly: IsDerivedFrom (import slot 0x140152a98) is
// Object::Base::IsDerivedFrom; the terrain-decal remove FUN_1400b0d00 is
// DecalComponent_TerrainDecalRemove below.
// ============================================================

// Sub-callees of the terrain-decal-slot release (FUN_1400b0d00) were
// Collection::Array<T>::SetSize monomorphised into the exe (ENDGAME B.3.2
// step 2): FUN_14001ee40 stride 0x14 align 2 (elsewhere in code/Game/, its
// body — Pf_VectorResize14_ee40 in action_pf_pools.cpp — has NO zero-tail
// write at all, but both call sites HERE pass newCount==0, so the tail loop
// never executes and no init policy is ever exercised: any correctly
// sized/aligned trivial element type gives byte-identical behaviour at
// count==0). FUN_14001c890 is the already-catalogued U16 SetSize (stride 2,
// align 2, zero tail — also never exercised at count==0). Both are converted
// below; the two typedefs and resolves are gone.

// ============================================================
// Reimpl of FUN_1400b0d00 (TerrainDecalSystem release one decal slot)
// ============================================================
// Shared terrain-decal helper (also called from RadiusUI/PlacementTool).
// The decal system at terrainDecalSys+0x1F0 is an array of 0x60-byte slots.
// If slot[decalId].id (+0x28) still equals decalId, mark it free (-1), clear
// the slot's owner ptr (+0), and shrink the two sub-lists embedded at +8 and
// +0x18 to zero length. 1:1 port of the exe body.
// WorldGeometry::RemoveDecal. The 0x60-byte decal element is untyped (WorldGeometry's
// `_arrays[5]`, GAME-c): its reads below are LEFT.
void WorldGeometry::RemoveDecal(int decalId) {
    uintptr_t base = reinterpret_cast<uintptr_t>(_arrays[5].data);   // +0x1F0
    uintptr_t slot = base + static_cast<uintptr_t>(decalId) * 0x60;
    if (*reinterpret_cast<int*>(slot + 0x28) == decalId) {
        *reinterpret_cast<uint32_t*>(slot + 0x28) = 0xffffffff;
        *reinterpret_cast<uint64_t*>(slot) = 0;
        // B.3.2 bridge: exe 0x14001ee40 / 0x14001c890 ARE
        // Collection::Array<T>::SetSize(0) — see the note above the (removed)
        // typedefs for why the init-policy mismatch never fires at count 0.
        GameColl::AsArray<GameColl::Bytes<0x14, 2>>(slot + 8).SetSize(0);
        GameColl::AsArray<GameColl::U16>(slot + 0x18).SetSize(0);
    }
}

// B4 FORWARD: radiusui.cpp, placementtool.cpp (they hold the WorldGeometry as void*).
void DecalComponent_TerrainDecalRemove(void* terrainDecalSys, int decalId) {
    reinterpret_cast<WorldGeometry*>(terrainDecalSys)->RemoveDecal(decalId);
}

// ============================================================
// Helper: the component's DecalDescription (IsDerivedFrom 0x1401c75b8, else null)
// ============================================================
static DecalDescription* GetDecalDescription(DecalComponent* self) {
    Description* obj = self->_description;
    if (!obj)
        return nullptr;
    bool derived = obj->IsDerivedFrom(DecalDescription::TypeId());
    return derived ? static_cast<DecalDescription*>(obj) : nullptr;
}

// ============================================================
// DecalComponent::Serialize (vtable slot [4], 0x14011b7d0)
// ============================================================
// Gets DecalDescription via IsDerivedFrom(+0x18, DAT_1401c75b8),
// reads scale from building data+0x20 and stores in decalScale (+0x30).
void DecalComponent_Serialize(DecalComponent* self) {
    DecalDescription* config = GetDecalDescription(self);
    if (config) {
        self->decalScale = config->_initialAlpha;
    }
}

// ============================================================
// DecalComponent::Draw (vtable slot [6], 0x14011b560)
// ============================================================
// Real terrain-decal rendering (was a delegate to the exe body). Faithful 1:1 port of
// the 0x14011b560 disassembly: read the building footprint (MapComponent tile bbox +
// rotation), the model variant index (ModelComponent+0x164 → per-variant UV rect), and
// the DecalDescription (material list + tiled/uv params), then submit one decal to the
// terrain-decal system (gameWorld+0x110) via AddDecal (TradeWork_impl_0d80 @ b0d80).
// AddDecal fills a slot in the +0x1f0 array and tessellates it (ace40); the slot is then
// rendered every frame by GeomDraw_CollectDraw's +0x1f0 loop (geometry_draw.cpp). The
// resulting decalId is stored at self+0x2c so OnCreate can release it.
//
// This is the brown ground foundation under a StorageYard (and any building carrying a
// DecalDescription). STANDALONE: reached via the live vtable / driver below when
// BANISHED_LIVE_DECAL is set; the b0d80 → ace40 closure must be registered
// (reg_player_highlight.cpp) and the Transform2/Vector2 slots wired.

// AddDecal (0x1400b0d80, tool_base_highlight.cpp) — 19-arg tile-slot manager + tessellate.
extern "C" int TradeWork_impl_0d80(int64_t sys, int minX, int minY, int maxX, int maxY,
                                   uint8_t alpha, uint32_t rot, int64_t material,
                                   uint8_t tiledFlag, uint32_t p10, uint32_t uv0, uint32_t uv1,
                                   uint32_t uv2, uint32_t uv3, char p15, int p16, int p17,
                                   uint64_t p18, uint8_t p19);

// Draw-path helpers, called directly: 0x140004680 FindComponent(MapComponent)
// (FloorComponent_FindResData), 0x140003f60 FindComponent(ModelComponent)
// (ActCNR_FindComponent_3f60), DAT_1401c3dd8 = g_pGameWorldSingleton.
// RefreshEntityFlags (0x1400c3470, ActHunt_NotifyEntityChanged_c3470) — re-aggregate
// the owning entity's component flags and (un)register it in the update tree; used to
// bucket the decal's owner via the draw flag so the tree walk's +0x40 slot drives the
// build-once, then un-bucket once placed.

// The float parameters AddDecal (0x1400b0d80) takes through its uint32_t slots.
static uint32_t FloatBits(float f) {
    uint32_t u;
    std::memcpy(&u, &f, sizeof(u));
    return u;
}

// ============================================================================
// ENDGAME B.3.8 VIS — DecalComponent as a REAL class (DecalComponent.h).
//
// Before: a 48-slot DecalComponent_LiveVtable filled per create, Inert_ScalarDtor
// in slot 0 (only ~Internal), the ctor scalars patched in after the zero-fill.
// After: the compiler's 16-slot vtable and a real ctor/dtor chain (B38_RULES
// D1, D2); slot bodies are the ones the old table installed.
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/DecalComponent.h"

// Building ground-decal rendering (BANISHED_LIVE_DECAL, default OFF — like
// BANISHED_LIVE_STORAGEDISPLAY). A building's DecalDescription (e.g. StorageYard's
// brown dirt "foundation" — StorageYard.rsc `decal` -> Terrain/TiledDecals/
// StorageYardMaterial.rsc) is painted onto the terrain by DecalComponent::Draw, which
// AddDecal's it into the terrain-decal system at gameWorld+0x110. That slot array is
// walked + submitted every frame by GeomDraw_CollectDraw (geometry_draw.cpp), so the
// decal only needs to be BUILT ONCE.
//
// DRIVER NOTE (tree-driven, commit 85845b9): the retail decal build is driven by the
// component lifecycle (OnCreate/OnAdded via the Fourth UpdatePriority bucket). In
// standalone that bucket IS reachable once the owner entity is bucketed, so the build is
// driven through the faithful tree walk (no explicit per-frame drain):
//   * Startup slot (index 4 / +0x20) — dispatched by ComponentEntity::StartupComponents
//     at spawn — sets the draw flag (+0x20 bit 0x4) + c3470-rebuckets the owner, letting
//     the OnAdded pass dispatch slot 6 (Draw = AddDecal).
//   * The new +0x40 slot (index 8) retries the build while decalId < 0 (a still-loading
//     material leaves decalId == -1), then un-buckets once placed (clears bit 0x4 + c3470)
//     — build-once. Once AddDecal returns a slot index, decalId >= 0 and it is skipped.
// Default (env off) => nothing wired => the default sa-run stays byte-identical.
// 0x14011b8f0: Component() is the CALL 0x1401188b0; the 0x140162fd0 stamp is
// the compiler's; then
//     c7 43 28 00000000   mov [rbx+0x28], 0        decalState = 0
//     c7 43 2c ffffffff   mov [rbx+0x2c], -1       decalId    = -1
//     c7 43 30 0000803f   mov [rbx+0x30], 1.0f     decalScale = 1.0
// and flags |= 0xc3. decalId -1 matters: 0 means "I own terrain decal slot 0",
// and decalId is NOT in the save stream (slot 3 serializes only decalScale and
// decalState), so a stream-restored decal with 0 would TerrainDecalRemove(sys, 0)
// at world teardown. See reference_ctor_scalars_in_startup_are_unset_after_load.
DecalComponent::DecalComponent()
    : decalState(0), decalId(-1), decalScale(1.0f), _pad34{} {
    OrFlagsInCtor(0xc3);
}

// Bucket (or un-bucket) this decal's owning entity into the update tree via the draw flag
// (component +0x20 bit 0x4), so the EntityList tree walk dispatches the component's +0x40
// slot. c3470 (RefreshEntityFlags) re-aggregates the owner's flags and (de)registers it;
// it no-ops safely if the entityList isn't set yet (the flag itself is then picked up by
// the later EntityList registration). Setting the flag ⇒ eventual bucketing either way.
static void DecalComponent_SetDrawFlag(DecalComponent* c, bool on) {
    uint32_t oldFlags = c->_flags;
    uint32_t newFlags = on ? (oldFlags | 0x4u) : (oldFlags & ~0x4u);
    if (newFlags == oldFlags) return;
    c->_flags = newFlags;
    if (((newFlags ^ oldFlags) & 0xFFFF) != 0 && c->_entity) {
        ActHunt_NotifyEntityChanged_c3470(c->_entity);
    }
}

// [3] 0x14011b820 — decalScale/decalState round-trip here.
// ============================================================
// DecalComponent::Shutdown (vtable slot [3], 0x14011b820)
// ============================================================
// Serializes:
//   1. Component_Serialize base
//   2. Serializer(version=1): serialize decalScale (float at +0x30)
//   3. If version > 0: serialize decalState (int at +0x28)
void DecalComponent::Persist(void* stream) {
    Component::Persist(stream);   // retail's direct CALL 0x1401189b0
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.8 bridge (Persist takes void*)

    // Serialize decalScale (float at +0x30)
    ser.Serialize(decalScale, nullptr);

    // Version-gated: if version > 0, also serialize decalState
    if (ser.GetVersion() > 0) {
        ser.Serialize(decalState, nullptr);
    }
}
// [4] TREE DIVERGENCE (retail 0x14011b7d0): bucket the owner so the tree walk
// drives the build-once.
void DecalComponent::Startup() { DecalComponent_SetDrawFlag(this, true); }
// [6] 0x14011b560 — Draw: build + submit the terrain decal.
void DecalComponent::OnAdded() {
    if (GameUpdateLoop::_singleton == nullptr) {
        return;
    }
    // CheckPtr(owner) then FindComponent(MapComponent) for the tile footprint.
    MapComponent* mapComp = _entity->FindComponent<MapComponent>();   // 0x140004680
    if (!mapComp) {
        return;
    }

    int minX   = static_cast<int16_t>(mapComp->tileX0);
    int minY   = static_cast<int16_t>(mapComp->tileY0);
    int width  = static_cast<int16_t>(mapComp->tileX1);
    int height = static_cast<int16_t>(mapComp->tileY1);
    uint32_t rotation = static_cast<uint32_t>(mapComp->extraField);

    // FindComponent(ModelComponent) (0x140003f60) → the mesh-group index (+0x164). This
    // is the per-building visual style StartTown randomises (ComponentEntity_impl_1fc0
    // in retail); it selects which atlas cell of the tiled-decal material this
    // building's foundation samples. Defaults to 0 if no ModelComponent is found.
    int variant = 0;
    ModelComponent* modelComp = _entity->FindComponent<ModelComponent>();
    if (modelComp != nullptr) {
        variant = modelComp->meshGroup;
    }

    // DecalDescription = IsDerivedFrom(_description, DecalDescription typeId) ? it : null.
    DecalDescription* decalDesc = GetDecalDescription(this);
    if (!decalDesc) {
        return;
    }

    // Decal alpha (+0x30 decalScale, used as decalScale*255 -> the vertex alpha /
    // shader lightFogAlpha.w). Retail lifecycle:
    //   * Serialize seeds it from DecalDescription._initialAlpha (= 0.5) — the
    //     DURING-CONSTRUCTION (half-built, translucent foundation) value.
    //   * When the building COMPLETES, DecalComponent Slot12(msg=1) sets +0x30 = 1.0
    //     (0x3f800000) and re-adds the decal -> a fully-opaque finished foundation.
    // This used to hardcode 1.0 because nothing drove the completion ramp, and
    // _initialAlpha=0.5 then left every FINISHED foundation washed out. The ramp is now
    // wired (DecalComponent_EnterBuiltState below, driven from BuildComponent's 8->2
    // State8Finalize and from start-town's force-complete stamp), so seed the authored
    // value like retail: a building under construction paints its footprint at
    // _initialAlpha and goes fully opaque when it completes. Templates that are never
    // "built" author _initialAlpha=1.0 themselves (e.g. Pasture), so they are unaffected.
    //
    // This is what the 2026-08-23 "rebuilt house shows the OLD decal" report was: the
    // replacement is a fresh FOUNDATION, but it painted the completed building's
    // full-opacity footprint immediately, so the ground looked unchanged from before the
    // fire. Material/tile/rotation/variant were all already correct (traced).
    if (decalScale == 0.0f) {
        float initialAlpha = decalDesc->_initialAlpha;
        decalScale = (initialAlpha > 0.0f) ? initialAlpha : 1.0f;
    }

    // Per-variant UV rect (defaults {0,0,uvScale,uvScale} when variant >= count).
    float uvScale = kRdata_14017214c;
    DecalUVRect uv = {0.0f, 0.0f, uvScale, uvScale};
    if (variant < decalDesc->_uvRectCount) {
        if (decalDesc->_uvRects) {
            uv = decalDesc->_uvRects[variant];
        }
    }

    WorldGeometry* decalSys = GameUpdateLoop::_singleton->m_worldGeometry;
    if (!decalSys) {
        return;
    }

    // material = DecalDescription material-array[decalState].
    if (!decalDesc->_materials) {
        return;
    }
    void* material = decalDesc->_materials[decalState];

    uint8_t  tiledFlag = decalDesc->_tiled;
    uint8_t  p19       = decalDesc->_flagB;
    uint32_t p10       = FloatBits(decalDesc->_sizeB);
    int alpha = static_cast<int>(decalScale * 255.0f);   // DAT_14017222c = 255.0f

    // AddDecal (tool_base_highlight.cpp) takes the WorldGeometry and the material as
    // int64_t: those two bridges are LEFT.
    int id = TradeWork_impl_0d80(
        reinterpret_cast<int64_t>(decalSys), minX, minY,
        width - 1 + minX, height - 1 + minY,
        static_cast<uint8_t>(alpha), rotation, reinterpret_cast<int64_t>(material), tiledFlag,
        p10,
        FloatBits(uv.u0), FloatBits(uv.v0), FloatBits(uv.u1), FloatBits(uv.v1),
        '\0', 0, 0, 0, p19);
    decalId = id;
}
// [7] 0x14011b510 — OnCreate: release the decal slot.
// ============================================================
// DecalComponent::OnCreate (vtable slot [7], 0x14011b510)
// ============================================================
// If decalId >= 0, removes the decal from terrain, resets decalId to -1.
void DecalComponent::OnRemove() {
    if (decalId >= 0) {
        // Remove decal from terrain decal subsystem
        if (GameUpdateLoop::_singleton != nullptr) {
            WorldGeometry* terrainDecalSys = GameUpdateLoop::_singleton->m_worldGeometry;
            if (terrainDecalSys)
                terrainDecalSys->RemoveDecal(decalId);   // 0x1400b0d00
        }
    }
    decalId = -1;
}
// index 8 (+0x40) — per-frame draw-tick, dispatched by the EntityList tree walk once the
// owner is bucketed. Build-once: attempt the build until placed (Draw self-guards if the
// scene/material aren't ready), then un-bucket so the tree stops ticking this component
// (the placed decal is thereafter rendered by GeomDraw_CollectDraw, not the update tree).
// TREE DIVERGENCE: retail's slot 8 is Component's `RET`.
void DecalComponent::Tick(void* /*time*/) {
    if (decalId < 0) DecalComponent::OnAdded();                  // retry the build until the material is ready
    if (decalId >= 0) DecalComponent_SetDrawFlag(this, false);   // placed -> un-bucket
}
// [11] 0x14011b810. PORT GAP (retail 0x14011b810, 7 B) — B38 D9: never ported;
// the tree's table left it inert; kept inert here.
void DecalComponent::Slot11() {}
// [12] 0x14011b760. PORT GAP (retail 0x14011b760, the msg-1 "building completed"
// ramp) — B38 D9: the tree drives that ramp explicitly through
// DecalComponent_MarkOwnerBuilt below; the slot stays inert here.
void DecalComponent::OnEvent(int) {}
// Retail DecalComponent Slot12(msg=1) — "the building completed": set decalScale
// (+0x30) to 1.0 and re-add the decal so the half-built translucent footprint becomes
// the fully-opaque finished one. Our DecalComponent is inert (no message slot), so this
// is driven explicitly from the two places a building becomes complete:
//   * BuildComponent_State8Finalize (the 8->2 transition) — the normal construction path
//   * StartTown's force-complete stamp — start-town houses skip the build states
// Same collapse, and for the same reason, as WorkPlaceComponent_EnterBuiltState.
static void DecalComponent_EnterBuiltState(DecalComponent* cc) {
    if (!cc) return;
    if (cc->decalScale == 1.0f) return;          // already opaque — nothing to redo
    cc->decalScale = 1.0f;
    cc->DecalComponent::OnRemove();              // release the current slot (decalId = -1)
    DecalComponent_SetDrawFlag(cc, true);        // re-bucket: the tick rebuilds it at 1.0
}

// Find the owner entity's DecalComponent and run the completion ramp on it.
// B4 FORWARD: buildcomponent_state8.cpp, starttown.cpp (they pass the entity as void*).
extern "C" void DecalComponent_MarkOwnerBuilt(void* entityV) {
    if (!entityV) return;
    uint32_t tid = DecalComponent::TypeId();
    if (!tid) return;
    ComponentEntity* entity = reinterpret_cast<ComponentEntity*>(entityV);
    if (!entity->_components) return;
    for (int i = 0; i < entity->_componentCount; ++i) {
        Component* comp = entity->_components[i].Get();
        if (!comp) continue;
        if (comp->Type() != tid) continue;
        DecalComponent_EnterBuiltState(static_cast<DecalComponent*>(comp));
        return;
    }
}

// create 0x1400337c0: MemAlloc(0x38, 8), then the ctor.
void* DecalComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(DecalComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) DecalComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* DecalComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) DecalComponent());
}

DEFINE_EXE_TYPE(DecalComponent, "Component")
