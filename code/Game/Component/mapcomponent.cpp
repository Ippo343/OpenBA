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
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/mapcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// fn_IsDerivedFrom — dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

// IO::Serializer::Serialize(short*) — dispatch slot at 0x140152468
typedef void (*fn_Serialize_Short_t)(void* serializer, int16_t* field, const char* name);
static fn_Serialize_Short_t s_fn_Serialize_Short = nullptr;

// IO::Serializer::Serialize(int*) — dispatch slot at 0x140152ac8
typedef void (*fn_Serialize_Int_dispatch_t)(void* serializer, int32_t* field, const char* name);
static fn_Serialize_Int_dispatch_t s_fn_Serialize_Int_dispatch = nullptr;

// IO::Serializer::Serializer(stream, version) — dispatch slot at 0x140152ad8
typedef void (*fn_Serializer_Ctor_dispatch_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_dispatch_t s_fn_Serializer_Ctor_dispatch = nullptr;

// IO::Serializer::~Serializer() — dispatch slot at 0x140152ad0
typedef void (*fn_Serializer_Dtor_dispatch_t)(void* self);
static fn_Serializer_Dtor_dispatch_t s_fn_Serializer_Dtor_dispatch = nullptr;

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

    slot = ResolveAddr(Module::Exe, 0x140152468);
    if (slot) s_fn_Serialize_Short = *reinterpret_cast<fn_Serialize_Short_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serialize_Int_dispatch = *reinterpret_cast<fn_Serialize_Int_dispatch_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor_dispatch = *reinterpret_cast<fn_Serializer_Ctor_dispatch_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor_dispatch = *reinterpret_cast<fn_Serializer_Dtor_dispatch_t*>(slot);

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
// Pattern: IsDerivedFrom(self->_unk18, MapDescription typeId)
// Returns the component pointer if type matches, else nullptr.
static void* GetMapDescription(MapComponent* self) {
    EnsureHelpers();
    if (!s_fn_IsDerivedFrom_dispatch || !s_TypeId_MapDescription) return nullptr;

    void* obj = self->_unk18;
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
    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor_dispatch) {
        s_fn_Serializer_Ctor_dispatch(serializer, stream, 0);
    }

    // Serialize 4 uint16 tile coords as shorts
    if (s_fn_Serialize_Short) {
        s_fn_Serialize_Short(serializer, (int16_t*)&self->tileX0, nullptr);
        s_fn_Serialize_Short(serializer, (int16_t*)&self->tileY0, nullptr);
        s_fn_Serialize_Short(serializer, (int16_t*)&self->tileX1, nullptr);
        s_fn_Serialize_Short(serializer, (int16_t*)&self->tileY1, nullptr);
    }

    // Serialize extra int at +0x30
    if (s_fn_Serialize_Int_dispatch) {
        s_fn_Serialize_Int_dispatch(serializer, &self->extraField, nullptr);
    }

    // Destroy serializer
    if (s_fn_Serializer_Dtor_dispatch) {
        s_fn_Serializer_Dtor_dispatch(serializer);
    }
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
    printf("[MapComponent] Funcs resolved: IsDerivedFrom=%p Serialize_Short=%p "
           "MapUpdateOverlay=%p MapInit=%p TerrainClear=%p\n",
           (void*)s_fn_IsDerivedFrom_dispatch, (void*)s_fn_Serialize_Short,
           (void*)s_fn_MapUpdateOverlay, (void*)s_fn_MapInit,
           (void*)s_fn_TerrainClear);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
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

    static bool s_trace = std::getenv("BANISHED_MAP_TRACE") != nullptr;
    if (s_trace) {
        static int s_count = 0;
        if (++s_count <= 20 || (s_count % 500) == 0)
            printf("[MinimapOverlay] %s #%d rect(%d,%d %dx%d) color=%08x\n",
                   clear ? "clear" : "paint", s_count, x, y, w, h, color);
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

// COMPONENT_INERT_FACTORY(MapComponent, 0x38, 0x83), expanded by hand so the
// per-type vtable's +0x38 slot (index 7 — the slot FUN_1400c2ea0 dispatches at
// entity removal) routes to the ported MapComponent_OnCreate above (whose
// impl_4750 tail is the grid un-mark). Default-safe: the bit-0x100000 gate means
// it no-ops unless the spawn-side collapse wrote occupancy for this component.
static uint32_t MapComponent_InertGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("MapComponent");
    return cached;
}
static void* MapComponent_InertVtable[::ComponentFactory::kInertVtableSlots];
static void MapComponent_OnAddedVSlot(void* s) {
    // slot 6 (+0x30) — dispatched by ComponentEntity_OnAddedPass at register
    // time (MapComponent flags 0x83 have bit 0 set). Paint the minimap dot.
    MapComponent_PaintMinimapRect(reinterpret_cast<uint8_t*>(s), /*clear=*/false);
}
static void MapComponent_DetachVSlot(void* s) {
    // Retail removal (0x1401242f0) first clears the minimap overlay rect via
    // helper_4d30(minimap, 0, ...) when desc+0x32. The same branch inside the
    // ported MapComponent_OnCreate is dead in standalone (IsDerivedFrom
    // landmine), so do it here trusting the linked description.
    MapComponent_PaintMinimapRect(reinterpret_cast<uint8_t*>(s), /*clear=*/true);
    MapComponent_OnCreate(reinterpret_cast<MapComponent*>(s));
}
static void MapComponent_FinishVtable() {
    MapComponent_InertVtable[6] = reinterpret_cast<void*>(&MapComponent_OnAddedVSlot);
    MapComponent_InertVtable[7] = reinterpret_cast<void*>(&MapComponent_DetachVSlot);
}
extern "C" void* MapComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x38, 0x83,
        MapComponent_InertVtable, reinterpret_cast<void*>(&MapComponent_InertGetTypeId));
    MapComponent_FinishVtable();
    return p;
}
static void* MapComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x38, 0x83,
        MapComponent_InertVtable, reinterpret_cast<void*>(&MapComponent_InertGetTypeId));
    MapComponent_FinishVtable();
    return mem;
}
REGISTER_TYPE(MapComponent, "Component",
              &MapComponent_Factory, &MapComponent_CreateInPlace);
#endif // STANDALONE_EXE
