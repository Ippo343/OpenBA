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
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

typedef int (*fn_Serializer_GetVersion_t)(void* self);
static fn_Serializer_GetVersion_t s_fn_Serializer_GetVersion = nullptr;

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
typedef void (*fn_NodeListAppend_t)(void* listPtr, int newCount, void* entityPtr);
static fn_NodeListAppend_t s_fn_NodeListAppend = nullptr;

// ZoneComponent_sub_d360 (0x14009d360) — rasterize zone tiles into the world grid.
typedef void (*fn_ZoneRaster_t)(int* world, void* entityRef, int x, int y,
    int w, int h, int64_t* descField28, int kind);
static fn_ZoneRaster_t s_fn_ZoneRaster = nullptr;

// FUN_140002cc0 — finalize/cleanup tree node during OnCreate.
typedef void (*fn_NodeFinalize_t)(void* listPtr, void* entityRef);
static fn_NodeFinalize_t s_fn_NodeFinalize = nullptr;

// FUN_140004680 result consumer in OnCreate/Draw reads map fields via this lookup.
// DAT_1401c7a18 — type id checked for the zone description (IsDerivedFrom).
static uint32_t* s_pZoneDescTypeId = nullptr;

static void** s_pGameState = nullptr;

static bool s_initialized = false;

static void EnsureZoneHelpers();

// ============================================================
// ZoneComponent::Shutdown (0x1401302F0)
// ============================================================
// Serializes component, then conditionally adds entity to zone map.
// If serializer version==0 and stream is reading (mode==0),
// registers the owning entity into the zone map at GameState+0xF0.

void ZoneComponent_Shutdown(ZoneComponent* self, void* stream) {
    EnsureZoneHelpers();

    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer, stream, 1);

    if (s_fn_Serializer_GetVersion) {
        int version = s_fn_Serializer_GetVersion(serializer);
        if (version == 0 && *(int*)((uintptr_t)stream + 8) == 0) {
            void* entityPtr = self->owner;
            if (s_pGameState && *s_pGameState && s_fn_ZoneMapAdd) {
                void* zoneMap = *(void**)((uintptr_t)*s_pGameState + 0xF0);
                s_fn_ZoneMapAdd(zoneMap, &entityPtr);
            }
        }
    }

    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer);
}

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

    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(entityPtr);
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

    if (s_fn_NodeListAppend && found)
        s_fn_NodeListAppend(found + 1, static_cast<int>(found[2]) + 1, entityPtr);
}

// ============================================================
// ZoneComponent::Draw (0x1401301F0)
// ============================================================
// Reimpl of ZoneComponent::Draw (0x1401301F0): find the owner's map component,
// pull its tile rect (x,y,w,h at +0x28..+0x2e) and kind (+0x30), resolve the zone
// description via IsDerivedFrom(map+0x18, zoneDescTypeId), rasterize the zone tiles
// into the world grid, then (re)register the entity in the zone map tree.

void ZoneComponent_Draw(ZoneComponent* self) {
    EnsureZoneHelpers();

    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(self);
    int64_t* ownerSlot = reinterpret_cast<int64_t*>(selfAddr + 0x10);

    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(ownerSlot);

    int64_t mapIBP = 0;
    if (s_fn_FindMapComponent)
        s_fn_FindMapComponent(*ownerSlot, &mapIBP);
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(&mapIBP);
    if (!mapIBP) {
        if (std::getenv("BANISHED_REGENTITY_TRACE")) {
            fprintf(stderr, "[ZoneAdd] self=%p ABORT: no MapComponent\n", (void*)self);
            fflush(stderr);
        }
        return;   // standalone-defensive: retail data guarantees the map comp
    }

    int16_t x = *reinterpret_cast<int16_t*>(mapIBP + 0x28);
    int16_t y = *reinterpret_cast<int16_t*>(mapIBP + 0x2a);
    int16_t w = *reinterpret_cast<int16_t*>(mapIBP + 0x2c);
    int16_t h = *reinterpret_cast<int16_t*>(mapIBP + 0x2e);
    int kind = *reinterpret_cast<int*>(mapIBP + 0x30);

    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(&mapIBP);
    // Retail gates the description through IsDerivedFrom(DAT_1401c7a18); in
    // standalone's partial factory tree that check rejects valid descriptions
    // (the documented FUN_1401253d0-class landmine, same accommodation as
    // MapComponent_PaintMinimapRect) and d360 would then deref (null+0x28).
    // Trust the linked description — it comes straight from the entity template.
    void* desc = *reinterpret_cast<void**>(mapIBP + 0x18);
    if (!desc) {
        if (std::getenv("BANISHED_REGENTITY_TRACE")) {
            fprintf(stderr, "[ZoneAdd] self=%p ABORT: map+0x18 description is NULL\n", (void*)self);
            fflush(stderr);
        }
        return;
    }

    if (!s_pGameState || !*s_pGameState) {
        if (std::getenv("BANISHED_REGENTITY_TRACE")) {
            fprintf(stderr, "[ZoneAdd] self=%p ABORT: no game state\n", (void*)self);
            fflush(stderr);
        }
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
    if (std::getenv("BANISHED_REGENTITY_TRACE")) {
        fprintf(stderr, "[ZoneAdd] self=%p owner=%p map=%p desc=%p rect=(%d,%d) %dx%d "
                        "kind=%d townMgr=%p fnZoneMapAdd=%p\n",
                (void*)self, (void*)*ownerSlot, (void*)mapIBP, desc,
                (int)x, (int)y, (int)w, (int)h, kind,
                *reinterpret_cast<void**>(world + 0xf0), (void*)s_fn_ZoneMapAdd);
        fflush(stderr);
    }
}

// ============================================================
// ZoneComponent::OnCreate (0x140130080)
// ============================================================
// Reimpl of ZoneComponent::OnCreate (0x140130080): like Draw, find the map
// component and zone description, rasterize the zone tiles, then walk the zone-map
// red-black tree for the owner's entity id and finalize the matching node.

void ZoneComponent_OnCreate(ZoneComponent* self) {
    EnsureZoneHelpers();

    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(self);
    int64_t* ownerSlot = reinterpret_cast<int64_t*>(selfAddr + 0x10);

    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(ownerSlot);

    int64_t mapIBP = 0;
    if (s_fn_FindMapComponent)
        s_fn_FindMapComponent(*ownerSlot, &mapIBP);
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(&mapIBP);
    if (!mapIBP) return;   // standalone-defensive: retail data guarantees the map comp

    int16_t x = *reinterpret_cast<int16_t*>(mapIBP + 0x28);
    int16_t y = *reinterpret_cast<int16_t*>(mapIBP + 0x2a);
    int16_t w = *reinterpret_cast<int16_t*>(mapIBP + 0x2c);
    int16_t h = *reinterpret_cast<int16_t*>(mapIBP + 0x2e);
    int kind = *reinterpret_cast<int*>(mapIBP + 0x30);

    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(&mapIBP);
    // Same IsDerivedFrom landmine accommodation as Draw above: trust the linked
    // description or d360 derefs (null+0x28).
    void* desc = *reinterpret_cast<void**>(mapIBP + 0x18);
    if (!desc) return;

    if (!s_pGameState || !*s_pGameState) return;
    uintptr_t world = reinterpret_cast<uintptr_t>(*s_pGameState);

    int64_t entityRef = 0;
    if (fn_InternalBasePtr_Ctor)
        fn_InternalBasePtr_Ctor(&entityRef, nullptr);
    if (s_fn_ZoneRaster)
        s_fn_ZoneRaster(*reinterpret_cast<int**>(world + 0xf8), &entityRef,
            (int)x, (int)y, (int)w, (int)h,
            reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(desc) + 0x28), kind);

    // Walk the zone-map tree for the owner's entity id (key at owner_deref+0x28).
    int64_t ownerRef = *ownerSlot;
    uintptr_t zoneMap = *reinterpret_cast<uintptr_t*>(world + 0xf0);
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(&ownerRef);
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

// ============================================================
// Init: resolve function pointers
// ============================================================
static void EnsureZoneHelpers() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_ZoneMapAdd = reinterpret_cast<fn_ZoneMapAdd_t>(
        ResolveAddr(Module::Exe, 0x140088e20));
    s_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    // Sub-helpers for the reimplemented OnCreate / Draw / ZoneMapAdd
    s_fn_FindMapComponent = reinterpret_cast<fn_FindMapComponent_t>(
        ResolveAddr(Module::Exe, 0x140004680));
    s_fn_TreeInsert = reinterpret_cast<fn_TreeInsert_t>(
        ResolveAddr(Module::Exe, 0x140017230));
    s_fn_NodeListAppend = reinterpret_cast<fn_NodeListAppend_t>(
        ResolveAddr(Module::Exe, 0x140003110));
    s_fn_ZoneRaster = reinterpret_cast<fn_ZoneRaster_t>(
        ResolveAddr(Module::Exe, 0x14009d360));
    s_fn_NodeFinalize = reinterpret_cast<fn_NodeFinalize_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));
    s_pZoneDescTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7a18));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a48);
    if (slot) s_fn_Serializer_GetVersion = *reinterpret_cast<fn_Serializer_GetVersion_t*>(slot);

    s_initialized = true;
}

void ZoneComponent_InitFuncs() {
    EnsureZoneHelpers();
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// In standalone the original exe is not mapped, so Object::Factory::CreateObject
// must resolve to a C++ factory here. Produces an inert-but-valid ZoneComponent
// (size 0x28, +0x20 capability flags 0x83, Object::Internal base + shared stub
// vtable, zeroed body). Size from ZoneComponent_Factory MemAlloc; flags from the
// Component_Ctor (+0x20=0x80) chain OR'd with the leaf ctor bits.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// ============================================================
// Vtable slots 6/7 — the CELL+0x00 occupant write (placed-building click-pick)
// ============================================================
// Retail dispatch: EntityList::RegisterEntity -> ComponentEntity_OnAddedPass ->
// ZoneComponent vtable slot 6 = Draw (0x1401301f0, xref'd ONLY from the vtable at
// 0x140164e60) -> sub_d360 (0x14009d360), which writes the owning entity into
// **cell + 0x00** for every non-'.' footprint cell, notifies the pathfinder mirror
// per cell (a6f00 cmd 3), and bumps the '.'-cell reserve counter at cell+0x28.
// Slot 7 = OnCreate (0x140130080), dispatched by MarkForRemoval's vtable+0x38
// walk, rasterizes the same rect with a NULL ref — the symmetric teardown.
//
// cell+0x00 is the slot EditTool's cell pick (FUN_1400a0280) resolves FIRST —
// before the cell+0x08 solid layer buildings only claim when their clearing
// finishes (MapComponent slot 12). This is what makes a just-placed foundation
// clickable in retail while the trees still hold +0x08 (user-verified in the
// shipped game 2026-07-28; recorded as the "cell+0x00 writer" open).
//
// Everything below Draw was already ported AND registered (d360 in
// keystone_a_zone.cpp, a6f00, the 88e20 zone-map insert) — but the inert factory
// shipped with the shared stub vtable, so OnAddedPass dispatched slot 6 into a
// stub and no placed building ever reached cell+0x00. Same one-guard-away shape
// as PickingComponent slot 6 and MapComponent slot 12 before it.
static uint32_t ZoneComponent_InertGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ZoneComponent");
    return cached;
}
static void* ZoneComponent_InertVtable[::ComponentFactory::kInertVtableSlots];
static bool ZoneRasterLive() {
    // Default-on via kDefaultOnFlags (2026-07-28); BANISHED_LIVE_ZONE_RASTER=0 is
    // the kill-switch for A/B runs.
    return ::ComponentFactory::LiveEnabled("BANISHED_LIVE_ZONE_RASTER");
}
static void ZoneComponent_TraceRaster(void* s, const char* which) {
    static bool s_trace = [](){ const char* e = std::getenv("BANISHED_ZONE_TRACE");
                               return e && e[0] && e[0] != '0'; }();
    if (!s_trace) return;
    static int s_count = 0;
    ++s_count;
    if (s_count > 40 && (s_count % 200) != 0) return;
    std::fprintf(stderr, "[ZoneRaster] #%d %s comp=%p live=%d\n",
                 s_count, which, s, ZoneRasterLive() ? 1 : 0);
    std::fflush(stderr);
}
static void ZoneComponent_OnAddedVSlot(void* s) {
    // slot 6 (+0x30) — flags 0x83 bit 0 puts every ZoneComponent in the
    // OnAddedPass dispatch, exactly as retail.
    ZoneComponent_TraceRaster(s, "attach(Draw)");
    if (!ZoneRasterLive()) return;
    ZoneComponent_Draw(reinterpret_cast<ZoneComponent*>(s));
}
static void ZoneComponent_DetachVSlot(void* s) {
    // slot 7 (+0x38) — the removal walk. Must mirror slot 6 or a removed
    // building leaves a dangling entity ref in cell+0x00 (the exact
    // use-after-free shape MapComponent_DetachOccupancy_4750 exists to prevent
    // on the +0x08 layer).
    ZoneComponent_TraceRaster(s, "detach(OnCreate)");
    if (!ZoneRasterLive()) return;
    ZoneComponent_OnCreate(reinterpret_cast<ZoneComponent*>(s));
}
static void ZoneComponent_SerVSlot(void* s, void* stream) {
    ZoneComponent_Shutdown(reinterpret_cast<ZoneComponent*>(s), stream);
}
static void ZoneComponent_FinishVtable() {
    ZoneComponent_InertVtable[6] = reinterpret_cast<void*>(&ZoneComponent_OnAddedVSlot);
    ZoneComponent_InertVtable[7] = reinterpret_cast<void*>(&ZoneComponent_DetachVSlot);
    // slot 3 wired unconditionally, save/load wave 2026-08-28 (fields previously reset silently on load).
    ZoneComponent_InertVtable[3] = reinterpret_cast<void*>(&ZoneComponent_SerVSlot);
}
extern "C" void* ZoneComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x28, 0x83,
        ZoneComponent_InertVtable, reinterpret_cast<void*>(&ZoneComponent_InertGetTypeId));
    ZoneComponent_FinishVtable();
    return p;
}
static void* ZoneComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x28, 0x83,
        ZoneComponent_InertVtable, reinterpret_cast<void*>(&ZoneComponent_InertGetTypeId));
    ZoneComponent_FinishVtable();
    return mem;
}
REGISTER_TYPE(ZoneComponent, "Component",
              &ZoneComponent_Factory, &ZoneComponent_CreateInPlace);
