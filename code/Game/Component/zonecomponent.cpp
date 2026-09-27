// code/Game/Component/zonecomponent.cpp
// Clean reimplementation of ZoneComponent lifecycle methods from Application-x64.exe.
//
// ZoneComponent (0x28 bytes) — minimal component, manages zone rendering.
// No additional fields beyond Component base.
//
// Vtable at 0x140164e30. Three non-trivial overrides:
//   Shutdown (0x1401302F0) — Serialize + conditional zone map registration
//   Draw     (0x1401301F0) — Zone rendering (complex, many deps)
//   OnCreate (0x140130080) — Zone creation + tree walk (complex)
//
// Draw and OnCreate are deeply coupled to zone rendering subsystem.
// We call the original exe functions for these.
//

#include "Component/zonecomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include "io/Stream.h"      // ENDGAME B.3.4 callers batch H: IO::Stream::GetDirection
#include <new>

// ============================================================
// Resolved helpers
// ============================================================

// FUN_140088e20 — zone map registration
typedef void (*fn_ZoneMapAdd_t)(void* zoneMap, void* entityPtr);
static fn_ZoneMapAdd_t s_fn_ZoneMapAdd = nullptr;

// ---- sub-helpers used by the reimplemented OnCreate / Draw / ZoneMapAdd ----

// FUN_140004680 — find the map/tile component on an entity (returns InternalBasePtr*)
typedef void* (*fn_FindMapComponent_t)(int64_t owner, void* outIBP);
static fn_FindMapComponent_t s_fn_FindMapComponent = nullptr;

// ResidenceComponent_impl_7230 (0x140017230) — red-black tree node insert/lookup.
// Returns ptr-to-(ptr-to-node).
typedef void** (*fn_TreeInsert_t)(void* treeRoot, void* outNode, void* key, void* scratch);
static fn_TreeInsert_t s_fn_TreeInsert = nullptr;

// FUN_140003110 — append an entity ref to a tree node's child list.

// ZoneComponent_sub_d360 (0x14009d360) — rasterize zone tiles into the world grid.
typedef void (*fn_ZoneRaster_t)(int* world, void* entityRef, int x, int y,
    int w, int h, int64_t* descField28, int kind);
static fn_ZoneRaster_t s_fn_ZoneRaster = nullptr;

// FUN_140002cc0 — finalize/cleanup tree node during OnCreate.
typedef void (*fn_NodeFinalize_t)(void* listPtr, void* entityRef);
static fn_NodeFinalize_t s_fn_NodeFinalize = nullptr;

static void** s_pGameState = nullptr;

static bool s_initialized = false;

static void EnsureZoneHelpers();


// ============================================================
// Reimpl of FUN_140088e20 (ZoneComponent: register an entity into the zone map).
// ============================================================
// param_2 is an InternalBasePtr*; key = *(*param_2 + 0x28). Walk the red-black tree
// at zoneMap+0x160 for that key; if no node matches, insert one via TreeInsert; then
// append the entity ref to the node's child list (node+8, count = node[2]+1).
void ZoneComponent_ZoneMapAdd(void* zoneMapV, void* entityPtrV) {
    EnsureZoneHelpers();

    uintptr_t zoneMap = reinterpret_cast<uintptr_t>(zoneMapV);
    void* entityPtr = entityPtrV;

    uint64_t key = *reinterpret_cast<uint64_t*>(
        *reinterpret_cast<uintptr_t*>(entityPtr) + 0x28);

    // Retail (0x140088e20) is `while (puVar3 = 0, puVar1 != 0)` — the reset is part
    // of the loop CONDITION, so it runs before every test including the one that
    // ends the walk. `found` is therefore non-null ONLY on the break path, i.e.
    // only on an exact key match.
    //
    // This was written as a `while (node)` with `found = nullptr` as the first
    // statement of the BODY, which is not the same program: the body does not run
    // on the iteration that exits, so `found` kept the last node the walk descended
    // RIGHT from — a lower bound, not a match. A non-null `found` then skips the
    // insert below and appends the entity to that other node's list, so the zone
    // was filed under a different description entirely and its own key never
    // appeared in the tree.
    //
    // Symptom: TutorialGoalCount's Zone mode counts this registry, so a drag-placed
    // StorageYard was placed and solid while stepFirstPile2's goal counted 0
    // forever. Intermittent, because whether the final descent goes left or right
    // depends on the ordering of heap description pointers — which differs run to
    // run. Reached step 18 by a jump and the tree was small enough to work; reached
    // it organically, with WoodHouse and Clear already registered, and it did not.
    uint64_t* node = *reinterpret_cast<uint64_t**>(zoneMap + 0x160);
    uint64_t* found = nullptr;
    for (;;) {
        found = nullptr;
        if (node == nullptr) break;
        if (key < *node) {
            node = reinterpret_cast<uint64_t*>(node[4]);
        } else {
            found = node;
            if (key <= *node) break;   // equal key => exact match, stop
            node = reinterpret_cast<uint64_t*>(node[5]);
        }
    }

    if (found == nullptr) {
        int64_t outNode[2] = {0, 0};
        int64_t scratch[3] = {0, 0, 0};
        if (s_fn_TreeInsert) {
            void** ret = s_fn_TreeInsert(reinterpret_cast<void*>(zoneMap + 0x160),
                outNode, &key, scratch);
            if (ret) found = reinterpret_cast<uint64_t*>(*ret);
        }
        if (fn_MemFreeStatic) fn_MemFreeStatic(nullptr);
    }

    // B.3.2 bridge: FUN_140003110 is Collection::Array<void*>::SetSize(n, fill)
    if (found)
        GameColl::AsArray<GameColl::Ptr8>((void*)(found + 1))
            .SetSizeFill(static_cast<int>(found[2]) + 1, entityPtr);
}

// 0x1401301f0 door: registered by address (standalone/reg_component_nature.cpp).
void ZoneComponent_Draw(ZoneComponent* self) {
    self->ZoneComponent::OnAdded();
}


// 0x140130080 door: registered by address (standalone/reg_component_nature.cpp).
void ZoneComponent_OnCreate(ZoneComponent* self) {
    self->ZoneComponent::OnRemove();
}


// ============================================================
// Init: resolve function pointers
// ============================================================
static void EnsureZoneHelpers() {
    if (s_initialized) return;

    s_fn_ZoneMapAdd = reinterpret_cast<fn_ZoneMapAdd_t>(
        ResolveAddr(Module::Exe, 0x140088e20));
    s_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    // Sub-helpers for the reimplemented OnCreate / Draw / ZoneMapAdd
    s_fn_FindMapComponent = reinterpret_cast<fn_FindMapComponent_t>(
        ResolveAddr(Module::Exe, 0x140004680));
    s_fn_TreeInsert = reinterpret_cast<fn_TreeInsert_t>(
        ResolveAddr(Module::Exe, 0x140017230));
    s_fn_ZoneRaster = reinterpret_cast<fn_ZoneRaster_t>(
        ResolveAddr(Module::Exe, 0x14009d360));
    s_fn_NodeFinalize = reinterpret_cast<fn_NodeFinalize_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));

    s_initialized = true;
}

void ZoneComponent_InitFuncs() {
    EnsureZoneHelpers();
}

// ============================================================================
// ENDGAME B.3.8 VIS — ZoneComponent as a REAL class (ZoneComponent.h).
//
// Before: a 48-slot ZoneComponent_InertVtable filled per create, Inert_ScalarDtor in
// slot 0 (only ~Internal).
// After: the compiler's 16-slot vtable and a real ctor/dtor chain
// (B38_RULES D1, D2); slot bodies are the ones the old table installed.
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/ZoneComponent.h"

// Retail dispatch: EntityList::RegisterEntity -> ComponentEntity_OnAddedPass ->
// ZoneComponent slot 6 = Draw (0x1401301f0) -> sub_d360 (0x14009d360), which
// writes the owning entity into **cell + 0x00** for every non-'.' footprint
// cell, notifies the pathfinder mirror per cell (a6f00 cmd 3), and bumps the
// '.'-cell reserve counter at cell+0x28. Slot 7 = OnCreate (0x140130080),
// dispatched by MarkForRemoval's vtable+0x38 walk, rasterizes the same rect
// with a NULL ref — the symmetric teardown.
//
// cell+0x00 is the slot EditTool's cell pick (FUN_1400a0280) resolves FIRST —
// before the cell+0x08 solid layer buildings only claim when their clearing
// finishes (MapComponent slot 12). This is what makes a just-placed foundation
// clickable in retail while the trees still hold +0x08.

// 0x1401303c0: Component() is the CALL 0x1401188b0; the 0x140164e30 stamp is
// the compiler's; flags |= 3 (0x83 with Component's 0x80 — bit 0 puts every
// ZoneComponent in the OnAddedPass dispatch, exactly as retail).
ZoneComponent::ZoneComponent() {
    OrFlagsInCtor(0x3);
}

// ============================================================
// ZoneComponent::Persist (0x1401302F0)
// ============================================================
// Serializes component, then conditionally adds entity to zone map.
// If serializer version==0 and stream is reading (mode==0),
// registers the owning entity into the zone map at GameState+0xF0.

void ZoneComponent::Persist(void* stream) {
    EnsureZoneHelpers();

    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge

    int version = reinterpret_cast<IO::Serializer*>(serializer)->GetVersion();   // B.3.4 bridge
    if (version == 0 && static_cast<int>(reinterpret_cast<IO::Stream*>(stream)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Stream::GetDirection, retail 0x180006200)
        void* entityPtr = _entity;
        if (s_pGameState && *s_pGameState && s_fn_ZoneMapAdd) {
            void* zoneMap = *(void**)((uintptr_t)*s_pGameState + 0xF0);
            s_fn_ZoneMapAdd(zoneMap, &entityPtr);
        }
    }

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}
// ============================================================
// ZoneComponent::OnAdded (0x1401301F0)
// ============================================================
// Reimpl of ZoneComponent::Draw (0x1401301F0): find the owner's map component,
// pull its tile rect (x,y,w,h at +0x28..+0x2e) and kind (+0x30), resolve the zone
// description via IsDerivedFrom(map+0x18, zoneDescTypeId), rasterize the zone tiles
// into the world grid, then (re)register the entity in the zone map tree.

void ZoneComponent::OnAdded() {
    EnsureZoneHelpers();

    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(this);
    int64_t* ownerSlot = reinterpret_cast<int64_t*>(selfAddr + 0x10);


    int64_t mapIBP = 0;
    if (s_fn_FindMapComponent)
        s_fn_FindMapComponent(*ownerSlot, &mapIBP);
    if (!mapIBP) {
        return;   // standalone-defensive: retail data guarantees the map comp
    }

    int16_t x = *reinterpret_cast<int16_t*>(mapIBP + 0x28);
    int16_t y = *reinterpret_cast<int16_t*>(mapIBP + 0x2a);
    int16_t w = *reinterpret_cast<int16_t*>(mapIBP + 0x2c);
    int16_t h = *reinterpret_cast<int16_t*>(mapIBP + 0x2e);
    int kind = *reinterpret_cast<int*>(mapIBP + 0x30);

    // Retail gates the description through IsDerivedFrom(DAT_1401c7a18); in
    // standalone's partial factory tree that check rejects valid descriptions
    // (the documented FUN_1401253d0-class landmine, same accommodation as
    // MapComponent_PaintMinimapRect) and d360 would then deref (null+0x28).
    // Trust the linked description — it comes straight from the entity template.
    void* desc = *reinterpret_cast<void**>(mapIBP + 0x18);
    if (!desc) {
        return;
    }

    if (!s_pGameState || !*s_pGameState) {
        return;
    }
    uintptr_t world = reinterpret_cast<uintptr_t>(*s_pGameState);

    int64_t entityRef = *ownerSlot;
    if (s_fn_ZoneRaster)
        s_fn_ZoneRaster(*reinterpret_cast<int**>(world + 0xf8), &entityRef,
            (int)x, (int)y, (int)w, (int)h,
            reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(desc) + 0x28), kind);

    entityRef = *ownerSlot;
    if (s_fn_ZoneMapAdd)
        s_fn_ZoneMapAdd(*reinterpret_cast<void**>(world + 0xf0), &entityRef);

    // BANISHED_REGENTITY_TRACE — this call is the ONLY writer of the
    // per-description zone registry at (world+0xf0)+0x160, which is what
    // TutorialGoalCount's Zone mode counts. A drag-placed StorageYard reaches
    // OnAdded with the same armed component set as a Clear zone (Map, Zone,
    // Decal, Work, StateMachine) and the correct description, yet never gains a
    // node — so either one of the two early returns above fires for it, or this
    // add runs and does not insert. Say which.
}
// ============================================================
// ZoneComponent::OnRemove (0x140130080)
// ============================================================
// Reimpl of ZoneComponent::OnCreate (0x140130080): like Draw, find the map
// component and zone description, rasterize the zone tiles, then walk the zone-map
// red-black tree for the owner's entity id and finalize the matching node.

void ZoneComponent::OnRemove() {
    EnsureZoneHelpers();

    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(this);
    int64_t* ownerSlot = reinterpret_cast<int64_t*>(selfAddr + 0x10);


    int64_t mapIBP = 0;
    if (s_fn_FindMapComponent)
        s_fn_FindMapComponent(*ownerSlot, &mapIBP);
    if (!mapIBP) return;   // standalone-defensive: retail data guarantees the map comp

    int16_t x = *reinterpret_cast<int16_t*>(mapIBP + 0x28);
    int16_t y = *reinterpret_cast<int16_t*>(mapIBP + 0x2a);
    int16_t w = *reinterpret_cast<int16_t*>(mapIBP + 0x2c);
    int16_t h = *reinterpret_cast<int16_t*>(mapIBP + 0x2e);
    int kind = *reinterpret_cast<int*>(mapIBP + 0x30);

    // Same IsDerivedFrom landmine accommodation as Draw above: trust the linked
    // description or d360 derefs (null+0x28).
    void* desc = *reinterpret_cast<void**>(mapIBP + 0x18);
    if (!desc) return;

    if (!s_pGameState || !*s_pGameState) return;
    uintptr_t world = reinterpret_cast<uintptr_t>(*s_pGameState);

    int64_t entityRef = 0;
    if (fn_InternalBasePtr_Ctor)
        new (&entityRef) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    if (s_fn_ZoneRaster)
        s_fn_ZoneRaster(*reinterpret_cast<int**>(world + 0xf8), &entityRef,
            (int)x, (int)y, (int)w, (int)h,
            reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(desc) + 0x28), kind);

    // Walk the zone-map tree for the owner's entity id (key at owner_deref+0x28).
    int64_t ownerRef = *ownerSlot;
    uintptr_t zoneMap = *reinterpret_cast<uintptr_t*>(world + 0xf0);
    uint64_t key = *reinterpret_cast<uint64_t*>(ownerRef + 0x28);

    uint64_t* node = *reinterpret_cast<uint64_t**>(zoneMap + 0x160);
    uint64_t* found = nullptr;
    while (node != nullptr) {
        if (key < *node) {
            node = reinterpret_cast<uint64_t*>(node[4]);
        } else {
            if (key <= *node) { found = node; break; }
            node = reinterpret_cast<uint64_t*>(node[5]);
        }
    }
    if (s_fn_NodeFinalize && found)
        s_fn_NodeFinalize(found + 1, &ownerRef);
}

// [0] 0x14003a870: ~ZoneComponent (0x140130380: restamp + ~Internal), MemFree on &1.
Object::Base* ZoneComponent::DeletingDtor(unsigned flags) {
    this->~ZoneComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x14003a6a0: MemAlloc(0x28, 8), then the ctor.
void* ZoneComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ZoneComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ZoneComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* ZoneComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ZoneComponent());
}

DEFINE_EXE_VTABLE(ZoneComponent, "_ZTV13ZoneComponent")
DEFINE_EXE_TYPE(ZoneComponent, "Component")
