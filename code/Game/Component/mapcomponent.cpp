// code/Game/Component/mapcomponent.cpp
// Clean reimplementation of MapComponent lifecycle methods from Application-x64.exe.
//
// MapComponent (0x38 bytes) manages tile map coordinates for entities.
// Fields: +0x28..+0x2E = four uint16 tile coords (initialized to 0xFFFF = invalid)
//         +0x30 = int32 extra field (serialized)
//
// Vtable at 0x140163aa0. Three non-trivial overrides:
//   Shutdown  (0x1401243e0) — Serializes tile coords as shorts + extra int
//   Draw      (0x140124370) — Checks building flag, calls map update helper
//   OnCreate  (0x1401242f0) — Reads building data, calls terrain clear + map init
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/mapcomponent_funcs.h"
#include "options.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>

// ============================================================
// Resolved helpers
// ============================================================

// fn_IsDerivedFrom — dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

// Component_Serialize (base class) — at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// MapComponent_sub_41b0 — map overlay update helper (0x1401241b0)
typedef void (*fn_MapUpdateOverlay_t)(void* self);
static fn_MapUpdateOverlay_t s_fn_MapUpdateOverlay = nullptr;

// CreateNaturalResource_impl_4750 — map initialization (0x140124750)
typedef void (*fn_MapInit_t)(void* self);
static fn_MapInit_t s_fn_MapInit = nullptr;

// MapComponent_helper_4d30 — terrain clear helper (0x1400a4d30)
typedef void (*fn_TerrainClear_t)(void* terrainPtr, int param2, int x0, int y0, int x1, int y1);
static fn_TerrainClear_t s_fn_TerrainClear = nullptr;

// TypeId for MapDescription — at 0x1401c7a18
static uint32_t* s_TypeId_MapDescription = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    uintptr_t slot;

    // Dispatch slots — read function pointer from .rdata slot at runtime
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    // Direct exe addresses
    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    s_fn_MapUpdateOverlay = reinterpret_cast<fn_MapUpdateOverlay_t>(
        ResolveAddr(Module::Exe, 0x1401241b0));

    s_fn_MapInit = reinterpret_cast<fn_MapInit_t>(
        ResolveAddr(Module::Exe, 0x140124750));

    s_fn_TerrainClear = reinterpret_cast<fn_TerrainClear_t>(
        ResolveAddr(Module::Exe, 0x1400a4d30));

    // Type ID pointer
    s_TypeId_MapDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7a18));

    s_helpersResolved = true;
}

// ============================================================
// Reimpl of FUN_1400a4d30 (MapComponent terrain-clear / rect fill)
// ============================================================
// Marks the grid dirty (*grid = 1) and writes `value` into the 32-bit cell grid at
// grid+0x28 over a (w x h) rectangle anchored at (x, y). Row index is flipped:
//   cell = (((width - y) - row - 1) * width + x + col), width @ grid+4.
// Pure data write, no external deps. 1:1 port of the exe helper.
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
// Helper: Get MapDescription sibling component
// ============================================================
// Pattern: IsDerivedFrom(self->_description, MapDescription typeId)
// Returns the component pointer if type matches, else nullptr.
static void* GetMapDescription(MapComponent* self) {
    EnsureHelpers();
    if (!s_fn_IsDerivedFrom_dispatch || !s_TypeId_MapDescription) return nullptr;

    void* obj = self->_description;
    if (!obj) return nullptr;

    bool derived = s_fn_IsDerivedFrom_dispatch(obj, *s_TypeId_MapDescription);
    return derived ? obj : nullptr;
}

// ============================================================
// MapComponent::Shutdown (vtable slot [3], 0x1401243e0)
// ============================================================
// Serializes tile coordinates and extra int field.
//
// Original disassembly:
//   CALL Component_Serialize (0x1401189b0)
//   CALL Serializer_Ctor(local, stream, 0)
//   CALL Serialize_Short(&tileX0, nullptr) x4
//   CALL Serialize_Int(&extraField, nullptr)
//   CALL Serializer_Dtor(local)

void MapComponent_Shutdown(MapComponent* self, void* stream) {
    EnsureHelpers();

    // Call base Component_Serialize
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Create local IO::Serializer (24 bytes on stack)
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize 4 uint16 tile coords as shorts
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<short*>(&self->tileX0), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<short*>(&self->tileY0), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<short*>(&self->tileX1), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<short*>(&self->tileY1), nullptr);   // B.3.4 bridge

    // Serialize extra int at +0x30
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->extraField, nullptr);   // B.3.4 bridge

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// MapComponent::Draw (vtable slot [6], 0x140124370)
// ============================================================
// Checks building data flag at +0x30 (byte), calls map update overlay if nonzero.
//
// Original disassembly:
//   MOV RBX,[RCX+0x18]              ; get sibling component
//   MOV EDX,[0x1401c7a18]           ; MapDescription type ID
//   CALL [0x140152a98]              ; IsDerivedFrom
//   CMOVNZ RDX,RBX                 ; building = derived ? obj : nullptr
//   CMP byte [RDX+0x30],0          ; check building flag
//   JZ skip
//   CALL MapComponent_sub_41b0     ; update map overlay

void MapComponent_Draw(MapComponent* self) {
    EnsureHelpers();

    void* building = GetMapDescription(self);
    if (!building) return;

    // Check building[0x30] != 0 (byte flag: hasMapOverlay)
    uint8_t flag = *(uint8_t*)((uintptr_t)building + 0x30);
    if (flag != 0) {
        if (s_fn_MapUpdateOverlay) {
            s_fn_MapUpdateOverlay(self);
        }
    }
}

// ============================================================
// MapComponent::OnCreate (vtable slot [7], 0x1401242f0)
// ============================================================
// Sets up map tiles. If building[0x32] != 0, clears terrain for the tile area.
// Always calls map initialization at the end.
//
// Original disassembly:
//   Get building via IsDerivedFrom
//   CMP byte [RDX+0x32],0
//   JZ skip_clear
//   MOVSX sign-extend tile coords from int16 to int32
//   MOV RCX,[g_pGameWorldSingleton]
//   MOV RCX,[RCX+0x148]            ; terrain subsystem pointer
//   XOR EDX,EDX                    ; param2 = 0
//   CALL 0x1400a4d30               ; terrain clear helper
// skip_clear:
//   JMP CreateNaturalResource_impl_4750  ; tail call to map init

void MapComponent_OnCreate(MapComponent* self) {
    EnsureHelpers();

    void* building = GetMapDescription(self);
    if (building) {
        // Check building[0x32] != 0 (byte flag: needsTerrainClear)
        uint8_t clearFlag = *(uint8_t*)((uintptr_t)building + 0x32);
        if (clearFlag != 0) {
            // Get terrain subsystem from game world singleton
            if (g_pGameWorldSingleton && *g_pGameWorldSingleton && s_fn_TerrainClear) {
                uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
                void* terrainPtr = *(void**)(gameWorld + 0x148);
                if (terrainPtr) {
                    s_fn_TerrainClear(terrainPtr, 0,
                        (int)(int16_t)self->tileX0,
                        (int)(int16_t)self->tileY0,
                        (int)(int16_t)self->tileX1,
                        (int)(int16_t)self->tileY1);
                }
            }
        }
    }

    // Always call map initialization (tail call in original)
    if (s_fn_MapInit) {
        s_fn_MapInit(self);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void MapComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[MapComponent] Funcs resolved: IsDerivedFrom=%p "
           "MapUpdateOverlay=%p MapInit=%p TerrainClear=%p\n",
           (void*)s_fn_IsDerivedFrom_dispatch,
           (void*)s_fn_MapUpdateOverlay, (void*)s_fn_MapInit,
           (void*)s_fn_TerrainClear);
#endif
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
// MapDescription+0x30 and calls sub_41b0 (0x1401241b0): if desc+0x32 it paints
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
// so the paint here must ignore it. The registered 41b0 port is not called:
// its IsDerivedFrom(MapDescription) returns false in standalone's partial
// factory tree (documented landmine) and it null-derefs; we trust the linked
// description (mc+0x18), same as MapComponent_DetachOccupancy_4750 below.
static void MapComponent_PaintMinimapRect(uint8_t* mc, bool clear) {
    void* desc = *(void**)(mc + 0x18);
    if (!desc) return;
    // 41b0's paint gate (and 2f0's clear gate) = desc+0x32; the attach path
    // additionally has MapComponent::Draw's desc+0x30 gate.
    if (*(uint8_t*)((uint8_t*)desc + 0x32) == 0) return;
    if (!clear && *(uint8_t*)((uint8_t*)desc + 0x30) == 0) return;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uint8_t* minimap = *(uint8_t**)((uintptr_t)*g_pGameWorldSingleton + 0x148);
    if (!minimap || !*(void**)(minimap + 0x28)) return;  // overlay array not built yet
    int width = *(int32_t*)(minimap + 4);
    int x = *(int16_t*)(mc + 0x28), y = *(int16_t*)(mc + 0x2a);
    int w = *(int16_t*)(mc + 0x2c), h = *(int16_t*)(mc + 0x2e);
    // Unplaced components keep the 0xFFFF ctor sentinel in the tile fields, and
    // helper_4d30 has no bounds checks — a bad rect would scribble outside the
    // pixel array. Skip anything not fully inside the grid.
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > width || y + h > width) return;
    uint32_t color = clear ? 0u : *(uint32_t*)((uint8_t*)desc + 0x34);
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
// The OCCUPANCY half of sub_41b0 — cell entity-links + stale-occupant eviction
// ============================================================
// `MapComponent_sub_41b0` (keystone_a_map.cpp) is a complete port of 0x1401241b0 and
// `MapComponent_overlay_dc20` a complete port of 0x14009dc20, but nothing calls the
// pair: 41b0's `IsDerivedFrom(AreaDescription)` returns false in standalone's partial
// factory tree, leaving `pvVar5` null and the very next line reading `null+0x32`. So
// the occupancy write has been collapsed into hand-rolled per-site helpers
// (starttown.cpp) that each creation path must remember to call — and the PLAYER
// PLACEMENT path never did (measured 2026-07-28: a hand-placed building holds 0 of its
// 16 footprint cells, so EditTool's cell pick finds nothing and it cannot be selected).
//
// This is 41b0's second half with the one standalone accommodation this file already
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
extern void MapComponent_overlay_dc20(int* param_1, int64_t* param_2, uint8_t param_3,
                                      int param_4, int param_5, int param_6, int param_7,
                                      int64_t* param_8, int param_9);

extern "C" void MapComponent_ApplyOccupancy_41b0(void* mapCompV) {
    uint8_t* mc = reinterpret_cast<uint8_t*>(mapCompV);
    if (!mc || !g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    if ((*(uint32_t*)(mc + 0x20) >> 0x14) & 1) return;        // occupancy already written
    uint8_t* desc = *(uint8_t**)(mc + 0x18);
    if (!desc) return;
    if (desc[0x31] != 0) return;                              // description opts out
    int* grid = *(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);
    if (!grid) return;

    int64_t ownerIBP = *(int64_t*)(mc + 0x10);
    MapComponent_overlay_dc20(grid, &ownerIBP, (uint8_t)*(uint32_t*)(desc + 0x20),
                              (int)*(int16_t*)(mc + 0x28), (int)*(int16_t*)(mc + 0x2a),
                              (int)*(int16_t*)(mc + 0x2c), (int)*(int16_t*)(mc + 0x2e),
                              (int64_t*)(desc + 0x28), *(int32_t*)(mc + 0x30));

    // 41b0's tail: arm the removal-side clear (the bit MapComponent_DetachOccupancy_4750
    // gates on) and, if the low-16 flags changed, NotifyEntityChanged. The BTS never
    // touches the low 16 bits, so the notify is dead here exactly as it is in retail —
    // kept for shape.
    uint32_t before = *(uint32_t*)(mc + 0x20);
    uint32_t after  = before | 0x100000u;
    *(uint32_t*)(mc + 0x20) = after;
    if (((after ^ before) & 0xffff) != 0 && *(int64_t*)(mc + 0x10) != 0) {
        static void (*s_c3470)(void*) = nullptr;
        static bool s_init = false;
        if (!s_init) { s_init = true;
            s_c3470 = reinterpret_cast<void(*)(void*)>(ResolveAddr(Module::Exe, 0x1400c3470)); }
        if (s_c3470) s_c3470(*(void**)(mc + 0x10));
    }
}

// ============================================================
// Economy brick M-B — grid occupancy UN-mark on entity removal
// ============================================================
// Standalone body for CreateNaturalResource_impl_4750 (0x140124750), the
// removal-side mirror of the occupancy write. Retail dispatch: entity
// mark-removal FUN_1400c2ea0 walks the component list and calls vtable+0x38 on
// removable components -> MapComponent_OnCreate (0x1401242f0, ported above) ->
// tail-calls impl_4750 -> dee0 (clear the footprint's cell entity-links +
// reclassify terrain). Gated on comp+0x20 bit 0x100000 ("occupancy currently
// written"), set by the spawn-side collapse (starttown.cpp
// StartTown_PlaceResourceCell) exactly when cell+0x08 was written. Without
// this, removing a produce-harvested tree leaves the cell grid pointing at the
// FREED entity (CheckPtr is a release no-op) and any grid reader — the
// CitizenAI leisure box scan, the forester scan — faults on it.
//
// dee0 body: StartTown_UnmarkResourceCellGrid (starttown.cpp) — the verified
// shape/classify/write logic shared with the spawn-side mark.
extern "C" void StartTown_UnmarkResourceCellGrid(void* game, void* ownerEntity, int tileX,
                                                 int tileY, int w, int h, const char* shape, int rot);

extern "C" void MapComponent_DetachOccupancy_4750(void* selfV) {
    uint8_t* self = reinterpret_cast<uint8_t*>(selfV);
    if (!self) return;
    uint32_t flags = *(uint32_t*)(self + 0x20);
    if (((flags >> 20) & 1) == 0) return;          // occupancy not written
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    void* game = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);
    if (!game) return;

    // Owner entity (comp+0x10 IBP value) — dee0 compares it against cell+0x00 for
    // the buildable(2) override.
    void* ownerEntity = *(void**)(self + 0x10);

    // MapDescription shape (desc+0x28 String buffer). The original IsDerivedFrom
    // guard rejects valid descriptions in standalone's partial factory tree (the
    // documented FUN_1401253d0-class landmine), so trust the linked description —
    // it comes straight from the entity template. Null desc => no shape (1x1).
    void* desc = *(void**)(self + 0x18);
    const char* shape = desc ? *(const char**)((uint8_t*)desc + 0x28) : nullptr;

    StartTown_UnmarkResourceCellGrid(game, ownerEntity,
                                     (int)*(int16_t*)(self + 0x28),
                                     (int)*(int16_t*)(self + 0x2a),
                                     (int)*(int16_t*)(self + 0x2c),
                                     (int)*(int16_t*)(self + 0x2e),
                                     shape, *(int32_t*)(self + 0x30));

    // Clear the occupancy-written bit (4750: flags &= ~0x100000). The original's
    // low-16 changed -> RefreshEntityFlags notify can never fire from this bit,
    // so it is faithfully omitted.
    *(uint32_t*)(self + 0x20) = flags & ~0x100000u;
}

// ============================================================================
// ENDGAME B.3.8 VIS — MapComponent as a REAL class (MapComponent.h).
//
// Before: a 48-slot MapComponent_InertVtable filled per create, Inert_ScalarDtor
// in slot 0 (only ~Internal). After: the compiler's 16-slot vtable and a real
// ctor/dtor chain (B38_RULES D1, D2); slot bodies are the ones the old table
// installed. Slot 7 (the slot FUN_1400c2ea0 dispatches at entity removal)
// routes to MapComponent_OnCreate above, whose impl_4750 tail is the grid
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

// [3] 0x1401243e0
void MapComponent::Persist(void* stream) { MapComponent_Shutdown(this, stream); }

// [6] 0x140124370
void MapComponent::OnAdded() {
    void* s = this;
    // slot 6 (+0x30) — dispatched by ComponentEntity_OnAddedPass at register
    // time (MapComponent flags 0x83 have bit 0 set). Paint the minimap dot.
    MapComponent_PaintMinimapRect(reinterpret_cast<uint8_t*>(s), /*clear=*/false);

    // ...and then the SPAWN-TIME CELL CLAIM, the other half of what this slot does.
    // Retail slot 6 is `MapComponent::Draw` (0x140124370), verified from the decompile:
    //     if (MapDescription(mc+0x18)->+0x30) sub_41b0(mc);
    // and 41b0 (0x1401241b0) is paint (desc+0x32) AND occupancy (desc+0x31 == 0) under one
    // bit-0x100000 gate. Our port split the two and wired only the paint, so nothing wrote
    // the entity into its tile record on any register-time path.
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
    uint8_t* mc = reinterpret_cast<uint8_t*>(s);
    uint8_t* desc = *(uint8_t**)(mc + 0x18);
    if (!desc) return;
    // Trust the linked description instead of re-deriving it through IsDerivedFrom — the
    // same standalone accommodation PaintMinimapRect and DetachOccupancy_4750 already make
    // (MapDescription is not in standalone's partial factory tree, so retail's cast yields
    // null and every desc read here would fault).
    const bool trace = Opt().mapTrace != nullptr;
    if (trace) {
        const int s_n = ++s_mapSpawnClaimTraceCount;
        if (s_n <= 400)
            fprintf(stderr, "[MapSpawnClaim] #%d comp=%p rect=(%d,%d %dx%d) desc+0x30=%d "
                    "desc+0x31=%d alreadyWritten=%d -> %s\n",
                    s_n, s, (int)*(int16_t*)(mc + 0x28), (int)*(int16_t*)(mc + 0x2a),
                    (int)*(int16_t*)(mc + 0x2c), (int)*(int16_t*)(mc + 0x2e),
                    (int)desc[0x30], (int)desc[0x31],
                    (int)((*(uint32_t*)(mc + 0x20) >> 0x14) & 1),
                    desc[0x30] == 0 ? "SKIP (desc opts out of spawn-time claim)"
                                    : "CLAIMING CELLS");
        fflush(stderr);
    }
    if (desc[0x30] == 0) return;                 // retail: MapComponent::Draw's gate
    MapComponent_ApplyOccupancy_41b0(s);         // 41b0's occupancy half (+ the 0x100000 bit)
}
// [7] 0x1401242f0
void MapComponent::OnRemove() {
    void* s = this;
    // Retail removal (0x1401242f0) first clears the minimap overlay rect via
    // helper_4d30(minimap, 0, ...) when desc+0x32. The same branch inside the
    // ported MapComponent_OnCreate is dead in standalone (IsDerivedFrom
    // landmine), so do it here trusting the linked description.
    MapComponent_PaintMinimapRect(reinterpret_cast<uint8_t*>(s), /*clear=*/true);
    MapComponent_OnCreate(this);
}
// Slot 12 (+0x60) — the per-component MESSAGE slot, retail `MapComponent_Slot12`
// (0x1401243c0), which is only three instructions:
//
//     CMP EDX,1 ; JZ 0x1401241b0 ; RET
//
// i.e. "on message 1, run the occupancy write". The broadcaster is
// `ModelComponent_helper_2b70(entity, msgId)` (0x1400c2b70), which walks the entity's
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
    void* s = this;
    // [diag] BANISHED_MAP_TRACE — this is the only observable point for "the building just
    // became solid". dc20 itself logs nothing on clear ground (it only reports evictions),
    // so without this line a successful solidify is indistinguishable from a no-op.
    const bool trace = Opt().mapTrace != nullptr;
    if (trace && msgId == 1) {
        uint8_t* mc = reinterpret_cast<uint8_t*>(s);
        bool already = (*(uint32_t*)(mc + 0x20) >> 0x14) & 1;
        fprintf(stderr, "[MapSolidify] slot12 msg=1 comp=%p rect=(%d,%d %dx%d) alreadyWritten=%d -> %s\n",
                s, (int)*(int16_t*)(mc + 0x28), (int)*(int16_t*)(mc + 0x2a),
                (int)*(int16_t*)(mc + 0x2c), (int)*(int16_t*)(mc + 0x2e), (int)already,
                already ? "SKIP (bit already set)" : "REGISTERING CELLS");
        fflush(stderr);
    }
    if (msgId != 1) return;                       // retail: CMP EDX,1 / JZ / RET
    MapComponent_ApplyOccupancy_41b0(s);
}
// [0] 0x140035850: ~MapComponent (0x140124490: restamp + ~Internal), MemFree on &1.
Object::Base* MapComponent::DeletingDtor(unsigned flags) {
    this->~MapComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
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

DEFINE_EXE_VTABLE(MapComponent, "_ZTV12MapComponent")
DEFINE_EXE_TYPE(MapComponent, "Component")
