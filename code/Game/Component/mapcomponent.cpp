// code/Game/Component/mapcomponent.cpp
// Clean reimplementation of MapComponent lifecycle methods from Application-x64.exe.
//
// MapComponent (0x38 bytes) manages tile map coordinates for entities.
// Fields: +0x28..+0x2E = four int16 tile coords (x, y, w, h)
//         +0x30 = int32 rotation (serialized)
//
// Vtable at 0x140163aa0. Three non-trivial overrides:
//   Persist   (0x1401243e0) — Serializes tile coords as shorts + rotation int
//   OnAdded   (0x140124370) — Checks the description flag, calls AddToMap
//   OnRemove  (0x1401242f0) — Clears the minimap rect + the map occupancy
//
// Decompiled from Application-x64.exe using Ghidra.
//
// LEFT (B.4 COMP-d): GameUpdateLoop m_game / m_imageCapture are void* (W0 header);
// the tile rect is read through int16 (retail's MOVSX) until the fields take
// retail's int16 type (a later commit).

#include "Component/mapcomponent_funcs.h"
#include "Component/MapDescription.h"
#include "Entity/ComponentEntity.h"
#include "options.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "Component/exe_bodies.h"

// Helpers, called by name (B3R): Object::Base::IsDerivedFrom (import slot 0x140152a98),
// MapComponent::AddToMap (0x1401241b0), MapComponent_DetachOccupancy_4750 (0x140124750 —
// the registered body; keystone_a_map.cpp's CreateNaturalResource_impl_4750 is not),
// MapTerrainClear_Reimpl (0x1400a4d30, below).

// ============================================================
// Reimpl of FUN_1400a4d30 (MapComponent terrain-clear / rect fill)
// ============================================================
// Marks the grid dirty (*grid = 1) and writes `value` into the 32-bit cell grid at
// grid+0x28 over a (w x h) rectangle anchored at (x, y). Row index is flipped:
//   cell = (((width - y) - row - 1) * width + x + col), width @ grid+4.
// Pure data write, no external deps. 1:1 port of the exe helper.
// LEFT: the minimap overlay object (GameUpdateLoop +0x148) has no class yet.
extern "C" void MapTerrainClear_Reimpl(void* gridPtr, int value, int x, int y,
                                       int w, int h) {
    uint8_t* grid = reinterpret_cast<uint8_t*>(gridPtr);
    *grid = 1;
    int width = *(int*)(grid + 4);
    uint32_t* cells = *(uint32_t**)(grid + 0x28);
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            int64_t idx = (int64_t)(((int64_t)width - y - row - 1) * width + x + col);
            cells[idx] = (uint32_t)value;
        }
    }
}

// ============================================================
// Helper: the component's MapDescription
// ============================================================
// Pattern: IsDerivedFrom(self->_description, MapDescription typeId)
// Returns the description if the type matches, else nullptr.
static MapDescription* GetMapDescription(MapComponent* self) {

    Description* obj = self->_description;
    if (!obj) return nullptr;

    bool derived = obj->IsDerivedFrom(MapDescription::TypeId());   // slot 0x140152a98
    return derived ? static_cast<MapDescription*>(obj) : nullptr;
}


// ============================================================
// MapComponent_Draw — retail's slot 6 body (0x140124370)
// ============================================================
// Checks the description's +0x30 flag, runs AddToMap if nonzero.
//
// Original disassembly:
//   MOV RBX,[RCX+0x18]              ; the description
//   MOV EDX,[0x1401c7a18]           ; MapDescription type ID
//   CALL [0x140152a98]              ; IsDerivedFrom
//   CMOVNZ RDX,RBX                 ; desc = derived ? obj : nullptr
//   CMP byte [RDX+0x30],0          ; _claimOnAdd
//   JZ skip
//   CALL 0x1401241b0               ; AddToMap
//
// LEFT: nothing calls this body. The tree's MapComponent::OnAdded (below) is a
// DIFFERENT body of the same slot: it paints the minimap rect and writes the
// occupancy half of AddToMap separately, trusting the linked description (the
// tree's accommodation for IsDerivedFrom in its partial factory tree). Making the
// member this body is a behaviour change, not done here.
void MapComponent_Draw(MapComponent* self) {
    MapDescription* building = GetMapDescription(self);
    if (!building) return;

    if (building->_claimOnAdd) {
        self->AddToMap();   // 0x1401241b0
    }
}


// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/MapComponent.h"
#include <atomic>
#include <cstdlib>

// ============================================================
// GUI Keystone F — minimap OVERLAY paint (tree/stone/building dots)
// ============================================================
// Retail: EntityList::RegisterEntity -> ComponentEntity_OnAddedPass dispatches
// MapComponent vtable+0x30 = MapComponent::Draw (0x140124370), which gates on
// MapDescription+0x30 and calls AddToMap (0x1401241b0): if desc+0x32 it paints
// the footprint rect into the minimap overlay pixel array (gameWorld+0x148,
// array +0x28) with the description's map color (desc+0x34) via helper_4d30
// (0x1400a4d30 = MapTerrainClear_Reimpl above), setting the minimap dirty
// flag; GameUpdateLoop::Update "Step 16" then re-uploads the overlay texture.
// Removal (vtable+0x38, 0x1401242f0) paints the same rect with 0 (transparent).
//
// In standalone the inert vtable's +0x30 slot was a stub, so entities never
// painted onto the minimap: terrain rendered, but no trees/stone/buildings.
// This wires the PAINT half into slot 6. The occupancy half (41b0 -> dc20)
// stays collapsed into the spawn sites (starttown.cpp), which pre-set bit
// 0x100000 — the faithful 41b0 skips EVERYTHING (paint included) on that bit,
// so the paint here must ignore it. The AddToMap port is not called:
// its IsDerivedFrom(MapDescription) returns false in standalone's partial
// factory tree (documented landmine) and it null-derefs; we trust the linked
// description (mc+0x18), same as MapComponent_DetachOccupancy_4750 below.
static void MapComponent_PaintMinimapRect(MapComponent* mc, bool clear) {
    // The tree's accommodation: the linked description is trusted, no IsDerivedFrom.
    Description* linked = mc->_description;
    MapDescription* desc = static_cast<MapDescription*>(linked);
    if (!desc) return;
    // 41b0's paint gate (and 2f0's clear gate) = desc+0x32; the attach path
    // additionally has MapComponent::Draw's desc+0x30 gate.
    if (!desc->_showOnMap) return;
    if (!clear && !desc->_claimOnAdd) return;
    if (GameUpdateLoop::_singleton == nullptr) return;
    // LEFT: the minimap overlay (GameUpdateLoop +0x148) has no class yet.
    uint8_t* minimap = reinterpret_cast<uint8_t*>(GameUpdateLoop::_singleton->m_imageCapture);
    if (!minimap || !*(void**)(minimap + 0x28)) return;  // overlay array not built yet
    int width = *(int32_t*)(minimap + 4);
    int x = static_cast<int16_t>(mc->tileX0), y = static_cast<int16_t>(mc->tileY0);
    int w = static_cast<int16_t>(mc->tileX1), h = static_cast<int16_t>(mc->tileY1);
    // Unplaced components keep the 0xFFFF ctor sentinel in the tile fields, and
    // helper_4d30 has no bounds checks — a bad rect would scribble outside the
    // pixel array. Skip anything not fully inside the grid.
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > width || y + h > width) return;
    uint32_t color = clear ? 0u : desc->_color._value;
    MapTerrainClear_Reimpl(minimap, (int)color, x, y, w, h);

    static bool s_trace = Opt().mapTrace != nullptr;
    if (s_trace) {
        static int s_count = 0;
        if (++s_count <= 20 || (s_count % 500) == 0)
            printf("[MinimapOverlay] %s #%d rect(%d,%d %dx%d) color=%08x\n",
                   clear ? "clear" : "paint", s_count, x, y, w, h, color);
    }
}

// ============================================================
// The OCCUPANCY half of AddToMap — cell entity-links + stale-occupant eviction
// ============================================================
// `MapComponent::AddToMap` (keystone_a_map.cpp) is a complete port of 0x1401241b0 and
// `MapComponent_overlay_dc20` a complete port of 0x14009dc20, but nothing calls the
// pair: AddToMap's `IsDerivedFrom(MapDescription)` returns false in standalone's partial
// factory tree, leaving `pvVar5` null and the very next line reading `null+0x32`. So
// the occupancy write has been collapsed into hand-rolled per-site helpers
// (starttown.cpp) that each creation path must remember to call — and the PLAYER
// PLACEMENT path never did (measured 2026-07-28: a hand-placed building holds 0 of its
// 16 footprint cells, so EditTool's cell pick finds nothing and it cannot be selected).
//
// This is AddToMap's second half with the one standalone accommodation this file already
// makes twice (PaintMinimapRect, DetachOccupancy_4750): trust the description linked at
// mc+0x18 instead of re-deriving it through IsDerivedFrom. The paint half is skipped —
// slot 6 already ran it at register time.
//
// dc20 does MORE than write cells: any existing occupant that is not the new owner and
// is not already removal-marked (entity+0x40 bit 0x400) is MarkForRemoval'd
// (0x1400c2ea0) and pushed onto the world removal queue at EntityList+0xd0. That is
// retail's answer to "a building was placed on top of trees" — the trees are destroyed,
// not harvested. Calling only the cell-write half would leave those trees standing but
// absent from the grid, so the eviction is not optional if this runs at all.
void MapComponent_overlay_dc20(int* param_1, ComponentEntity* const* owner, uint8_t param_3,
                               int param_4, int param_5, int param_6, int param_7,
                               const System::String* footprint, int param_9);   // keystone_a_map.cpp

static void MapComponent_ApplyOccupancy_41b0(MapComponent* mc) {
    if (!mc || GameUpdateLoop::_singleton == nullptr) return;
    if ((mc->_flags >> 0x14) & 1) return;                     // occupancy already written
    // The tree's accommodation: the linked description is trusted, no IsDerivedFrom.
    Description* linked = mc->_description;
    MapDescription* desc = static_cast<MapDescription*>(linked);
    if (!desc) return;
    if (desc->_noOccupancy) return;                           // description opts out
    // LEFT: GameUpdateLoop::m_game is void* (W0 header); the Map functions take int*.
    int* grid = reinterpret_cast<int*>(GameUpdateLoop::_singleton->m_game);
    if (!grid) return;

    ComponentEntity* owner = mc->_entity;
    MapComponent_overlay_dc20(grid, &owner, (uint8_t)desc->_cellType,
                              (int)static_cast<int16_t>(mc->tileX0), (int)static_cast<int16_t>(mc->tileY0),
                              (int)static_cast<int16_t>(mc->tileX1), (int)static_cast<int16_t>(mc->tileY1),
                              &desc->_footprint, mc->extraField);

    // AddToMap's tail: arm the removal-side clear (the bit MapComponent_DetachOccupancy_4750
    // gates on) and, if the low-16 flags changed, RefreshEntityFlags. The BTS never
    // touches the low 16 bits, so the notify is dead here exactly as it is in retail —
    // kept for shape.
    uint32_t before = mc->_flags;
    uint32_t after  = before | 0x100000u;
    mc->_flags = after;
    if (((after ^ before) & 0xffff) != 0 && mc->_entity != nullptr) {
        mc->_entity->RefreshEntityFlags();   // 0x1400c3470
    }
}

// ============================================================
// Economy brick M-B — grid occupancy UN-mark on entity removal
// ============================================================
// Standalone body for 0x140124750, the removal-side mirror of the occupancy write.
// Retail dispatch: entity mark-removal FUN_1400c2ea0 walks the component list and
// calls vtable+0x38 on removable components -> MapComponent::OnRemove (0x1401242f0)
// -> tail-calls 0x140124750 -> dee0 (clear the footprint's cell entity-links +
// reclassify terrain). Gated on comp+0x20 bit 0x100000 ("occupancy currently
// written"), set by the spawn-side collapse (starttown.cpp
// StartTown_PlaceResourceCell) exactly when cell+0x08 was written. Without
// this, removing a produce-harvested tree leaves the cell grid pointing at the
// FREED entity (CheckPtr is a release no-op) and any grid reader — the
// CitizenAI leisure box scan, the forester scan — faults on it.
//
// dee0 body: StartTown_UnmarkResourceCellGrid (starttown.cpp) — the verified
// shape/classify/write logic shared with the spawn-side mark.
// LEFT: a second body of 0x140124750 (keystone_a_map.cpp's CreateNaturalResource_impl_4750,
// the faithful one) exists; this is the one the tree runs.
extern "C" void StartTown_UnmarkResourceCellGrid(void* game, void* ownerEntity, int tileX,
                                                 int tileY, int w, int h, const char* shape, int rot);

void MapComponent::RemoveFromMap() {
    MapComponent* self = this;
    uint32_t flags = self->_flags;
    if (((flags >> 20) & 1) == 0) return;          // occupancy not written
    if (GameUpdateLoop::_singleton == nullptr) return;
    void* game = GameUpdateLoop::_singleton->m_game;
    if (!game) return;

    // Owner entity (comp+0x10) — dee0 compares it against cell+0x00 for
    // the buildable(2) override.
    ComponentEntity* ownerEntity = self->_entity;

    // MapDescription shape (desc+0x28 String buffer). The original IsDerivedFrom
    // guard rejects valid descriptions in standalone's partial factory tree (the
    // documented FUN_1401253d0-class landmine), so trust the linked description —
    // it comes straight from the entity template. Null desc => no shape (1x1).
    Description* linked = self->_description;
    MapDescription* desc = static_cast<MapDescription*>(linked);
    const char* shape = desc ? desc->_footprint._data : nullptr;

    StartTown_UnmarkResourceCellGrid(game, ownerEntity,
                                     (int)static_cast<int16_t>(self->tileX0),
                                     (int)static_cast<int16_t>(self->tileY0),
                                     (int)static_cast<int16_t>(self->tileX1),
                                     (int)static_cast<int16_t>(self->tileY1),
                                     shape, self->extraField);

    // Clear the occupancy-written bit (4750: flags &= ~0x100000). The original's
    // low-16 changed -> RefreshEntityFlags notify can never fire from this bit,
    // so it is faithfully omitted.
    self->_flags = flags & ~0x100000u;
}

// B4 FORWARD: 1 outside caller (cropfieldcomponent_states.cpp).
// The tree's null test is kept here (the member has none; retail's 0x140124750 has none).
extern "C" void MapComponent_DetachOccupancy_4750(void* selfV) {
    if (!selfV) return;
    reinterpret_cast<MapComponent*>(selfV)->RemoveFromMap();
}

// ============================================================================
// ENDGAME B.3.8 VIS — MapComponent as a REAL class (MapComponent.h).
//
// Before: a 48-slot MapComponent_InertVtable filled per create, Inert_ScalarDtor
// in slot 0 (only ~Internal). After: the compiler's 16-slot vtable and a real
// ctor/dtor chain (B38_RULES D1, D2); slot bodies are the ones the old table
// installed. Slot 7 (the slot FUN_1400c2ea0 dispatches at entity removal)
// routes to the removal body below, whose 0x140124750 tail is the grid
// un-mark; the bit-0x100000 gate means it no-ops unless occupancy was written.
// ============================================================================

// Diagnostic counters of the BANISHED_MAP_TRACE lines (B6-5: namespace-scope
// atomics, not function-local statics — OnAdded runs on the loader thread too).
static std::atomic<int> s_mapSpawnClaimTraceCount{0};

// 0x1401244d0: Component() is the CALL 0x1401188b0; the 0x140163aa0 stamp is
// the compiler's; flags |= 3 (0x83). PORT GAP (retail 0x1401244d0) — B38 D1:
// retail writes 0xFFFF into the four rect words; the tree's zero-fill is kept
// here and transcribing the sentinel is its own titled commit.
MapComponent::MapComponent()
    : tileX0(0), tileY0(0), tileX1(0), tileY1(0), extraField(0), _pad34{} {
    OrFlagsInCtor(0x3);
}

// ============================================================
// MapComponent::Persist (vtable slot [3], 0x1401243e0)
// ============================================================
// Serializes tile coordinates and the rotation int.
//
// Original disassembly:
//   CALL Component_Serialize (0x1401189b0)
//   CALL Serializer_Ctor(local, stream, 0)
//   CALL Serialize_Short(&tileX0, nullptr) x4
//   CALL Serialize_Int(&extraField, nullptr)
//   CALL Serializer_Dtor(local)

void MapComponent::Persist(void* stream) {
    // Call base Component_Serialize
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    // LEFT: Component::Persist takes void* (COMP-c's header).
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);

    // The 4 tile coords, Serialize(short&) as retail.
    ser.Serialize(tileX0, nullptr);
    ser.Serialize(tileY0, nullptr);
    ser.Serialize(tileX1, nullptr);
    ser.Serialize(tileY1, nullptr);

    // Serialize the rotation at +0x30
    ser.Serialize(extraField, nullptr);
}

// [6] 0x140124370
void MapComponent::OnAdded() {
    // slot 6 (+0x30) — dispatched by ComponentEntity_OnAddedPass at register
    // time (MapComponent flags 0x83 have bit 0 set). Paint the minimap dot.
    MapComponent_PaintMinimapRect(this, /*clear=*/false);

    // ...and then the SPAWN-TIME CELL CLAIM, the other half of what this slot does.
    // Retail slot 6 is `MapComponent::Draw` (0x140124370), verified from the decompile:
    //     if (MapDescription(mc+0x18)->+0x30) AddToMap(mc);
    // and AddToMap (0x1401241b0) is paint (desc+0x32) AND occupancy (desc+0x31 == 0) under
    // one bit-0x100000 gate. Our port split the two and wired only the paint, so nothing
    // wrote the entity into its tile record on any register-time path.
    //
    // That is the missing writer behind the orchard bind crash: PlantOrchard ->
    // CreateNaturalResource spawns the tree, ActCNR_SetupModelAtCell_126540 fills THIS
    // component's rect (+0x28/+0x2a and 1x1 at +0x2c — 0x140004680 is the MapComponent
    // finder, not a "position" one), RegisterEntity dispatches slot 6 — and retail claims
    // the cell right there. OrchardComponent::OnMessage(0) then reads
    // *(mapGrid+0x80)[tile] + 8 and hands it to a component finder; with no claim that
    // field is 0 and the faithful deref faults. Find the writer, don't guard the deref.
    //
    // desc+0x30 is precisely the "claim at spawn vs claim later" switch: natural resources
    // set it and take their cell immediately, while a building leaves it clear and claims
    // its footprint only when its ClearArea finishes (slot 12 / msg 1, see below).
    // Trust the linked description instead of re-deriving it through IsDerivedFrom — the
    // same standalone accommodation PaintMinimapRect and DetachOccupancy_4750 already make
    // (MapDescription is not in standalone's partial factory tree, so retail's cast yields
    // null and every desc read here would fault).
    Description* linked = _description;
    MapDescription* desc = static_cast<MapDescription*>(linked);
    if (!desc) return;
    const bool trace = Opt().mapTrace != nullptr;
    if (trace) {
        const int s_n = ++s_mapSpawnClaimTraceCount;
        if (s_n <= 400)
            fprintf(stderr, "[MapSpawnClaim] #%d comp=%p rect=(%d,%d %dx%d) desc+0x30=%d "
                    "desc+0x31=%d alreadyWritten=%d -> %s\n",
                    s_n, static_cast<void*>(this),
                    (int)static_cast<int16_t>(tileX0), (int)static_cast<int16_t>(tileY0),
                    (int)static_cast<int16_t>(tileX1), (int)static_cast<int16_t>(tileY1),
                    (int)desc->_claimOnAdd, (int)desc->_noOccupancy,
                    (int)((_flags >> 0x14) & 1),
                    !desc->_claimOnAdd ? "SKIP (desc opts out of spawn-time claim)"
                                       : "CLAIMING CELLS");
        fflush(stderr);
    }
    if (!desc->_claimOnAdd) return;              // retail: MapComponent::Draw's gate
    MapComponent_ApplyOccupancy_41b0(this);      // AddToMap's occupancy half (+ the 0x100000 bit)
}
// ============================================================
// MapComponent::OnRemove (vtable slot [7], 0x1401242f0)
// ============================================================
// If the description's +0x32 is set, clears the minimap rect.
// Always calls the 0x140124750 map un-mark at the end.
//
// Original disassembly:
//   Get the description via IsDerivedFrom
//   CMP byte [RDX+0x32],0
//   JZ skip_clear
//   MOVSX sign-extend tile coords from int16 to int32
//   MOV RCX,[g_pGameWorldSingleton]
//   MOV RCX,[RCX+0x148]            ; the minimap overlay
//   XOR EDX,EDX                    ; param2 = 0
//   CALL 0x1400a4d30               ; rect fill
// skip_clear:
//   JMP 0x140124750                ; tail call to the map un-mark

// [7] 0x1401242f0
void MapComponent::OnRemove() {
    // Retail removal (0x1401242f0) first clears the minimap overlay rect via
    // helper_4d30(minimap, 0, ...) when desc+0x32. The same branch below is dead in
    // standalone (IsDerivedFrom landmine), so do it here trusting the linked description.
    MapComponent_PaintMinimapRect(this, /*clear=*/true);

    // B.3.8 CB1b: the ported retail removal body (0x1401242f0).
    MapDescription* building = GetMapDescription(this);
    if (building) {
        if (building->_showOnMap) {
            if (GameUpdateLoop::_singleton != nullptr) {
                void* terrainPtr = GameUpdateLoop::_singleton->m_imageCapture;   // +0x148
                if (terrainPtr) {
                    MapTerrainClear_Reimpl(terrainPtr, 0,   // 0x1400a4d30
                        (int)(int16_t)tileX0,
                        (int)(int16_t)tileY0,
                        (int)(int16_t)tileX1,
                        (int)(int16_t)tileY1);
                }
            }
        }
    }

    // Always the map un-mark (tail call in original): 0x140124750
    RemoveFromMap();
}
// Slot 12 (+0x60) — the per-component MESSAGE slot, retail `MapComponent_Slot12`
// (0x1401243c0), which is only three instructions:
//
//     CMP EDX,1 ; JZ 0x1401241b0 ; RET
//
// i.e. "on message 1, run the occupancy write". The broadcaster is
// ComponentEntity::SendEvent (0x1400c2b70), which walks the entity's
// components and calls each one's +0x60 with the id — and the caller that passes **1** is
// `ClearAreaDescription_helper_77f0`, the ClearArea **state-2 ENTER**
// (`MOV EDX,1; CALL c2b70` at 0x140117807). Every other +0x60 dispatch in the exe passes
// 0 (verified: all three BuildComponent sites and both sub_8720 sites are `XOR EDX,EDX`).
//
// So retail's answer to "when does a building claim its footprint cells" is: **when its
// ClearArea finishes**, not when it is placed. That is why a fresh foundation is pickable
// only by its decal mesh (fiddly), while a cleared/among-completed building is cell-picked
// and blocks navigation — and it is what makes dc20's stale-occupant eviction safe: by the
// time it runs, the trees have been harvested by citizens, so it only sweeps stragglers.
//
// Measured 2026-07-28: standalone ports the state-2 handler and does broadcast event 1, but
// this slot was an unwired stub, so the message landed on nothing and no player-placed
// building ever registered its cells.
void MapComponent::OnEvent(int msgId) {
    // [diag] BANISHED_MAP_TRACE — this is the only observable point for "the building just
    // became solid". dc20 itself logs nothing on clear ground (it only reports evictions),
    // so without this line a successful solidify is indistinguishable from a no-op.
    const bool trace = Opt().mapTrace != nullptr;
    if (trace && msgId == 1) {
        bool already = (_flags >> 0x14) & 1;
        fprintf(stderr, "[MapSolidify] slot12 msg=1 comp=%p rect=(%d,%d %dx%d) alreadyWritten=%d -> %s\n",
                static_cast<void*>(this),
                (int)static_cast<int16_t>(tileX0), (int)static_cast<int16_t>(tileY0),
                (int)static_cast<int16_t>(tileX1), (int)static_cast<int16_t>(tileY1), (int)already,
                already ? "SKIP (bit already set)" : "REGISTERING CELLS");
        fflush(stderr);
    }
    if (msgId != 1) return;                       // retail: CMP EDX,1 / JZ / RET
    MapComponent_ApplyOccupancy_41b0(this);
}

// create 0x140035700: MemAlloc(0x38, 8), then the ctor.
void* MapComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(MapComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) MapComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* MapComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) MapComponent());
}

DEFINE_EXE_TYPE(MapComponent, "Component")
