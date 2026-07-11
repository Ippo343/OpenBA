// code/Game/Game/starttown.cpp
// Start-town spawn-tile finder + camera placement — the CAMERA portion of
// FUN_1400a3300 (steps 1-3). The ECS spawn steps (town center, initial
// buildings, citizens, resource cull) are deferred to a later slice; see
// project_starttown_a3300.
//
// Pipeline (all read the live tile grid built by maptilegrid.cpp):
//   * FUN_1400a2720 — spiral search from the map center: rings of 4, 8, 16, ...
//     points, radius growing by floor(townSize*3/4) per ring, angle =
//     i*(360/points) DEGREES (DAT_140172238 = 360.0). The first tile whose
//     townSize-radius disk is fully buildable wins. (The original has NO
//     termination guard; we cap the ring count defensively.)
//   * FUN_1400a2880 — validity: every cell in the townSize-radius disk around
//     the candidate must be in-bounds AND tile type 2 (buildable).
//   * FUN_140002080 — tile -> world: idx -> world (x,z) -> fn_MapGetTerrainPos
//     -> camera position {worldX, worldZ, height}.
//
// Load then writes the result to PlayerController+0x1c/+0x20/+0x24 (verified at
// the a3300 call site, 0x14009761d).

#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include "Component/ambientenvironmentcomponent_funcs.h"  // InitRegion (ambient biome region/pos)
#include "Component/pickingcomponent_funcs.h"   // PickingComponent_RegisterMesh (mesh-precise picking)
#ifdef STANDALONE_EXE
#include "Component/component_factory_base.h"   // ComponentFactory::LiveEnabled
#endif
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>

// ============================================================================
// Economy Brick M-D — live town storage (BANISHED_LIVE_STORAGE, default OFF).
// Ports in code/Game/Game/town_storage_seed.cpp (Ghidra 1:1, see that file):
//   * TownRegistry_RegisterStorageLocation  = 0x14012d230 (townMgr+0x00 ADD;
//     retail fires it from the StorageLocationComponent OnAdded vtable slot,
//     which our inert components never run — so the spawn path calls it).
//   * TownStorage_BuildCompleteInit_7ff0    = 0x140107ff0 (build-complete
//     StorageComponent init: +0x38 = _volumeLimit * area, +0x3c = _storageFlags.
//     Faithful for our start town because it spawns fully-BUILT buildings).
//   * StartTown_SeedStartingResources       = 0x1400a1f00 (seven-mask seeding).
// The raw-material TEMPLATE registration below mirrors the FUN_1400a3300 loop
// at 0x1400a33b4 (pool startConfig+0xa0 / count +0xa8, stride 0x18 ExternalRef
// -> FUN_140091e60 dedup-append into townMgr+0x110), which retail runs AFTER
// the spawn passes and BEFORE Game_impl_1f00 — same order kept here.
// ============================================================================
extern "C" void TownRegistry_RegisterStorageLocation(void* storageLocationComp);
extern "C" void TownStorage_BuildCompleteInit_7ff0(void* componentWithEntity);
extern "C" void StartTown_SeedStartingResources(void* startConfig);
extern "C" void TownStorage_TraceDump();

static bool LiveStorageEnabled() {
    static int v = -1;
    if (v < 0) {
        const char* e = getenv("BANISHED_LIVE_STORAGE");
        v = (e && *e && e[0] != '0') ? 1 : 0;
    }
    return v == 1;
}
static bool StorageTraceOn() {
    static int v = -1;
    if (v < 0) {
        const char* e = getenv("BANISHED_STORAGE_TRACE");
        v = (e && *e && e[0] != '0') ? 1 : 0;
    }
    return v == 1;
}

static constexpr int   CELL_STRIDE = 0x30;
static constexpr int   CELL_TYPE   = 0x29;     // tile type byte
static constexpr int   GAME_CELLS  = 0x80;     // Game+0x80 = {void* data, int count, int cap}
static constexpr int   TILE_BUILDABLE = 2;
static constexpr float kCenterBias = 0.5f;     // DAT_140172114
static constexpr float kDeg2Rad    = 3.14159265358979323846f / 180.0f;

// Force-work harness (env BANISHED_FORCE_WORK): the first spawned start-town
// structure (a StorageBarn = a real workplace building entity) is captured here so
// the CitizenAI force-assign harness can point a citizen's workplace(+0x238) /
// housed-flag(+0x68) at it, exercising the Work loop before the town-manager
// building registry is brought up. Set in StartTown_SpawnStructures.
void* g_pForceWorkBuilding = nullptr;

// Phase 4 ASSIGN half: the forester Profession key (= the lodge's WorkPlaceDescription
// desc+0x20 Profession*) captured in StartTown_SpawnWorkplace, consumed by the deferred
// StartTown_EmployForesters pass (which seats laborers at the lodge -> citizen+0x238).
static uint64_t g_foresterProfKey = 0;
// AssignWorkplace port (brick 4-2) — sets citizen+0x238/+0x240 + rosters. 0x1401163d0.
extern "C" void CitizenComponent_AssignWorkplace(void* self, void* workplaceIBP, int slotIndex);

// First resolved natural-resource (tree) TEMPLATE, captured in the resource-scatter spawn
// loop (StartTown_SpawnResources). A resource template is a description container
// ({sub-desc array @+0x28, count @+0x30}) carrying the produced material's
// RawMaterialDescription — i.e. exactly the descEntity a CreateRawMaterial action clones.
// The Phase B production harness (production_harness.cpp, BANISHED_LIVE_PRODUCTION) uses it
// as a real produced-material description to drive the ported RegisterRawMaterial count.
void* g_pSampleResourceTemplate = nullptr;

// Owner-entity IBP values of the spawned residence (WoodHouse) buildings, captured by
// StartTown_RunOnCreatePass when each house's ResidenceComponent::OnCreate fires. Used by
// StartTown_AssignFoundingHousing to pair families to houses (townMgr+0x20 is never
// populated in standalone — see the capture site). Cleared at the start of each spawn.
static std::vector<int64_t> g_foundingHouseIBPs;
// Phase 4: founding-family CitizenComponents captured during AssignFoundingHousing;
// the deferred employ pass seats idle adults from this list at the lodge.
static std::vector<void*> g_foundingCitizens;

// GUI select harness: expose a captured founding-house building ENTITY (the value is the
// ComponentEntity ptr stored at ResidenceComponent+0x10) so the EditTool select harness can
// open a real building's info dialog without depending on the region-map/cursor pick. Returns
// null if index is out of range.
extern "C" void* StartTown_GetFoundingHouseEntity(int index) {
    if (index < 0 || (size_t)index >= g_foundingHouseIBPs.size()) return nullptr;
    return reinterpret_cast<void*>(g_foundingHouseIBPs[(size_t)index]);
}
extern "C" int StartTown_GetFoundingHouseCount() {
    return (int)g_foundingHouseIBPs.size();
}

// GUI click-to-select: mark a building's footprint cells as occupied by the building ENTITY
// (cell+0x08), reproducing the net effect of the real game's MapComponent::Draw ->
// FUN_14009dc20. The region-map cell array lives at Game+0x80 (0x30 bytes/cell, dim from
// Game+0x00); the reader FUN_1400a0280 returns cell+0x00 then cell+0x08. Standalone's
// MapComponent is inert so Draw never fires and buildings are ABSENT from this array — which
// is what EditTool's click-pick reads to resolve the entity under the cursor. Without this,
// clicking a building finds nothing. Mirrors the resource path's identical write (~L1724).
// Also makes footprints navigation-blocking (faithful). Default ON (opt out BANISHED_LIVE_PICKMAP=0).
static bool StartTown_PickMapEnabled() {
    static int on = -1;
    if (on < 0) {
        const char* e = std::getenv("BANISHED_LIVE_PICKMAP");
        on = (e && (e[0] == '0' || strcmp(e, "false") == 0)) ? 0 : 1;
    }
    return on;
}
static void StartTown_MarkBuildingCells(void* game, int tileX, int tileY, int fpW, int fpH,
                                        uintptr_t entIBP) {
    if (!StartTown_PickMapEnabled() || !game || !entIBP) return;
    uint8_t* cells = *(uint8_t**)((uint8_t*)game + 0x80);
    int dim = *(int32_t*)game;
    if (!cells || dim <= 0) return;
    int marked = 0;
    for (int iy = 0; iy < fpH; ++iy)
        for (int ix = 0; ix < fpW; ++ix) {
            int cx = tileX + ix, cy = tileY + iy;
            if (cx < 0 || cx >= dim || cy < 0 || cy >= dim) continue;
            int idx = dim * cy + cx;
            *(uintptr_t*)(cells + (size_t)idx * 0x30 + 0x08) = entIBP;  // dc20: cell+0x08 = entity
            marked++;
        }
    fprintf(stderr, "[startTown::pickmap] marked %d footprint cells @ (%d,%d) %dx%d entity=%p\n",
            marked, tileX, tileY, fpW, fpH, (void*)entIBP);
    fflush(stderr);
}

// MESH-PRECISE BUILDING PICKING. Retail lets you click anywhere on a building because
// picking ray-tests the 3D model mesh via Picking::Scene; in standalone that mesh is
// never registered (PickingComponent::OnAdded/Startup — 0x140128f50/c30 — are inert),
// so EditTool falls back to the coarse terrain-cell footprint pick (StartTown_Mark-
// BuildingCells), which only hits when the ray lands on the ground footprint => the
// "click-the-middle" pickiness. We drive the faithful registration here for each static
// start-town building (PickingComponent_RegisterMesh). Default ON; opt out with
// BANISHED_LIVE_PICKMESH=0. BANISHED_PICKMESH_PROBE=1 additionally dumps the component's
// fields + scene pointer (the diagnostic that established the SA ground truth).
static uint32_t TypeId(const char* name);
static void* FindComponentOnEntity(void* entity, uint32_t typeId);
static void StartTown_PickMeshProbe(void* entity) {
    static bool s_probe = [](){ const char* e = std::getenv("BANISHED_PICKMESH_PROBE");
                                return e && e[0] && e[0] != '0'; }();
    static bool s_enable = [](){ const char* e = std::getenv("BANISHED_LIVE_PICKMESH");
                                 return !(e && (e[0]=='0' || strcmp(e,"false")==0)); }();
    if (!entity || (!s_probe && !s_enable)) return;
    uint32_t tidPick = TypeId("PickingComponent");
    void* pc = FindComponentOnEntity(entity, tidPick);
    if (s_probe) {
        void* scene = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                    ? *(void**)((uint8_t*)*g_pGameWorldSingleton + 0x70) : nullptr;
        fprintf(stderr, "[PickMesh] entity=%p Picking::Scene(gw+0x70)=%p tidPick=%u pc=%p\n",
                entity, scene, tidPick, pc);
        if (pc) {
            uint8_t* p = (uint8_t*)pc;
            void* f18 = *(void**)(p + 0x18);
            void* meshTbl = f18 ? *(void**)((uint8_t*)f18 + 0x20) : nullptr;
            fprintf(stderr, "[PickMesh]   +0x10(owner)=%p +0x18(desc)=%p desc+0x20(tbl)=%p "
                    "+0x28(pickId)=%d +0x30(userData)=%p\n", *(void**)(p+0x10), f18, meshTbl,
                    *(int32_t*)(p+0x28), *(void**)(p+0x30));
        }
        fflush(stderr);
    }
    if (s_enable && pc) PickingComponent_RegisterMesh(pc);
}

// Faithful CitizenComponent::AssignHousing (0x140111ee0, citizencomponent.cpp) — the MISNAMED
// WORKPLACE assigner: sets citizen +0x248 (job) AND rosters into the house's WorkPlaceComponent
// (c80e0) occupant list (+0x58/+0x60). Retained for reference / the gameplay employ path; the
// founding-housing pass now uses CitizenComponent_AssignWorkplace instead (the RESIDENCE
// assigner, +0x238 / c7cf0), so founding citizens are housed, not employed-at-home.
struct CitizenComponent;
extern void CitizenComp_AssignHousing(CitizenComponent* self, int64_t* housingPtr);

// Brick M-C — the real town-citizen-list registrar (CitizenComponent_OnAdd 0x140114000)
// + the headless ProfessionUI worker-seat cascade driver (citizen_employ_cascade.cpp).
// CitizenEmploy_SeatWorkers runs the exact FindAvailableBuilding + cbf0->ca20->c5f0->c4b0
// fallback dd10 uses, seating each worker via +0x1f0=prof + AssignHousing(+0x248 roster).
extern "C" void CitizenComponent_OnAdd(void* citizenComp);
extern "C" int  CitizenEmploy_SeatWorkers(int64_t townMgr, uint64_t professionKey, int count);
// Residence-list registrar (ResidenceDescription_helper_9de0 port, citizen_employ_cascade.cpp):
// appends a built house's owner-entity IBP into townMgr+0x20 (count +0x28) — the list the
// town-hall "Homes" stat reads. Retail fires it from the residence build-state callback.
extern "C" void TownRegistry_RegisterResidence(int64_t houseEntityIBP);

// WorkPlaceComponent attach/detach roster-only mode (workplacecomponent.cpp). Set true for
// the build-time founding-housing pass so the roster is populated WITHOUT the gameplay-time
// Scene mesh-visual toggle (which deadlocks mid-build). See the flag's definition.
extern bool g_wpcRosterOnly;

// Apply the occupied end-state visual for a rostered house (ModelKick mode 2 + StatusIcon
// toggle). MUST run at gameplay time (live scene). See its definition in workplacecomponent.cpp.
extern void WorkplaceComponent_ApplyOccupiedVisual(int64_t comp);
// Faithful port of the workplace activation tail Entity_impl_fc00(WPC, 1) (0x14012fc00):
// sets the accepting flag (+0x50) AND raises the "no workers" warning status-icon when the
// workplace is unstaffed. See its definition in workplacecomponent.cpp.
extern void WorkPlaceComponent_SetAccepting(int64_t comp, bool accepting);
// Phase 1 economy: register a workplace into the townMgr+0x100 profession BST
// (code/Game/Game/town_workplace_registry.cpp).
extern "C" void TownRegistry_RegisterWorkplace(int64_t townMgr, void* workplaceComp);
// Phase-4 reader: FindAvailableBuilding (0x14008d100) — returns (in *out) an available
// workplace for a profession key from the townMgr+0x100 BST. Used to verify the lodge
// is findable after seating its profession key + registering it.
extern "C" void* FindAvailableBuilding(int64_t townMgr, void* out, uint64_t professionKey);

// WorkPlaceComponent pointers of the founding houses rostered in roster-only mode, whose
// occupied visual was deferred to gameplay time. Drained once by
// StartTown_ApplyPendingOccupiedVisuals on an early GameUpdateLoop tick (scene live).
static std::vector<int64_t> g_pendingOccupiedVisuals;

// FUN_1400a2880 — is the townSize-radius disk around (tileX, tileY) fully
// buildable (in-bounds + type 2)?
static bool StartTown_DiskBuildable(void* gameVoid, int tileX, int tileY, int size, int sizeSq) {
    uint8_t* game = (uint8_t*)gameVoid;
    const int dim = *(int32_t*)game;
    const uint8_t* cells = *(const uint8_t**)(game + GAME_CELLS);
    if (!cells || dim <= 0) return false;
    for (int iy = -size; iy <= size; ++iy) {
        for (int ix = -size; ix <= size; ++ix) {
            if (ix * ix + iy * iy > sizeSq) continue;          // outside the disk
            const int cx = tileX + ix, cy = tileY + iy;
            if (cx < 0 || cy < 0 || cx >= dim || cy >= dim) return false;
            if (cells[(size_t)(dim * cy + cx) * CELL_STRIDE + CELL_TYPE] != TILE_BUILDABLE)
                return false;
        }
    }
    return true;
}

// FUN_1400a2720 — spiral outward from the map center; return the first tile
// whose disk passes StartTown_DiskBuildable.
static bool StartTown_FindSpawnTile(void* gameVoid, int size, int* outX, int* outY) {
    const int dim    = *(int32_t*)gameVoid;
    const int half   = dim >> 1;
    const int sizeSq = size * size;
    const int step   = (size * 3) >> 2;        // floor(size*3/4); size > 0
    const float fullCircle = 360.0f;           // DAT_140172238

    float radius = 0.0f;
    int   points = 4;
    const int kMaxRings = 4096;                // defensive cap (original loops forever)
    for (int ring = 0; ring < kMaxRings; ++ring) {
        for (int i = 0; i < points; ++i) {
            const float ang = (float)i * (fullCircle / (float)points) * kDeg2Rad;
            const int tx = (int)(cosf(ang) * radius) + half;   // local_res8 (cos) * r + dim/2
            const int ty = (int)(sinf(ang) * radius) + half;   // local_res10 (sin) * r + dim/2
            if (StartTown_DiskBuildable(gameVoid, tx, ty, size, sizeSq)) {
                *outX = tx; *outY = ty;
                return true;
            }
        }
        points *= 2;
        radius += (float)step;
    }
    *outX = half; *outY = half;                // fallback: map center (should not happen)
    return false;
}

// FUN_140002080 — spawn tile -> camera world position {worldX, worldZ, height}.
static void StartTown_TileToWorld(void* gameVoid, int tileX, int tileY, float out3[3]) {
    const int dim = *(int32_t*)gameVoid;
    const int idx = dim * tileY + tileX;
    const float center = (float)(dim >> 1) - kCenterBias;
    float in[3] = { (float)(idx % dim) - center, (float)(idx / dim) - center, 0.0f };
    fn_MapGetTerrainPos(gameVoid, out3, in, 0, nullptr);   // flag=0 (our f3e0 defers picking refine)
}

// Camera portion of FUN_1400a3300: locate the start-town spawn tile and return
// its world position. camPosOut = {worldX, worldZ, height}; the caller (Load)
// writes it to PlayerController+0x1c/+0x20/+0x24.
extern "C" void StartTown_ComputeCamera(void* startConfig, void* game, float camPosOut[3]) {
    camPosOut[0] = camPosOut[1] = camPosOut[2] = 0.0f;
    if (!startConfig || !game || !fn_MapGetTerrainPos) return;
    int size = *(int32_t*)((uint8_t*)startConfig + 0x3c);   // townSize (mapGen+0x3c)
    if (size <= 0) size = 1;

    int tileX = (*(int32_t*)game) >> 1, tileY = tileX;      // default = map center
    StartTown_FindSpawnTile(game, size, &tileX, &tileY);
    StartTown_TileToWorld(game, tileX, tileY, camPosOut);
}

// ============================================================================
// ECS SPAWN — the spawn portion of FUN_1400a3300 (steps 4-7). Built up one
// pool at a time. The shared keystones already exist:
//   * ResolveExternalByName_Impl  — resolve a pool entry's description template
//   * fn_EntityFactory_CreateEntity (= ComponentEntity_CopyCtor, 0x1400c3720)
//       — instantiate a fresh ComponentEntity from a template
//   * fn_InternalBasePtr_Ctor     — wrap the new entity in a tracked IBP
//   * fn_EntityList_RegisterEntity (FUN_1400c48a0) — register with the world
//       entity list at world+0xe8 under a category id (= *(template+0x20))
//
// The world base (= g_pGameWorldSingleton in the original) is the GameUpdateLoop
// instance, passed in explicitly from Load. world+0xf8 = Game, world+0xe8 =
// entity list.
// ============================================================================

// A start-config pool entry is an ExternalRef (0x18 bytes):
//   +0x00 System::String name   +0x08 int gen   +0x10 External* template
static constexpr int POOLENTRY_STRIDE = 0x18;
static constexpr int EXTREF_TEMPLATE  = 0x10;
static constexpr int ENTITY_SIZE      = 0x48;   // ComponentEntity allocation
static constexpr int TEMPLATE_CATEGORY = 0x20;  // *(template+0x20) = entity-list category
static constexpr int WORLD_ENTITYLIST = 0xe8;   // world+0xe8 = entity list

// Instantiate one entity from a resolved template and register it. Returns true
// if an entity was created. Mirrors the inner body shared by every spawn loop.
static bool SpawnAndRegister(void* tmpl, void* entityList) {
    if (!tmpl) return false;
    // Foundation B (ECS instantiate/register) not yet wired in standalone: these
    // exe primitives resolve to NULL until reimplemented. Guard so a populated
    // pool can't fault here before B lands.
    if (!fn_EntityFactory_CreateEntity || !fn_EntityList_RegisterEntity ||
        !fn_InternalBasePtr_Ctor || !fn_MemAlloc) return false;
    void* mem = fn_MemAlloc(g_pMemAllocSingleton, ENTITY_SIZE, 8);
    void* ent = mem ? fn_EntityFactory_CreateEntity(mem, tmpl) : nullptr;
    void* ibp = nullptr;
    fn_InternalBasePtr_Ctor(&ibp, ent);
    int category = *(int32_t*)((uint8_t*)tmpl + TEMPLATE_CATEGORY);
    fn_EntityList_RegisterEntity(entityList, &ibp, category);
    return ent != nullptr;
}

// Resolve a pool entry's description template. ResolveExternalByName uses
// find-only Find (templates aren't preloaded in standalone), so it misses; use
// FindOrLoad (load-on-miss) which loads the Template\*.rsc from the mounted
// packages now that the entity-description factories are registered (Foundation
// C). Caches the result in entry+0x10.
static void* StartTown_ResolveTemplate(uint8_t* entry) {
    ResolveExternalByName_Impl(entry);                       // try the cheap cached path first
    void* tmpl = *(void**)(entry + EXTREF_TEMPLATE);
    if (tmpl) return tmpl;
    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton ||
        !fn_Filename_CtorString || !fn_ExternalFactory_FindOrLoad || !fn_Filename_Dtor)
        return nullptr;
    void* factory = *g_pExternalFactorySingleton;
    uint8_t fname[8] = {0};
    fn_Filename_CtorString(fname, entry);                    // String -> Filename
    tmpl = fn_ExternalFactory_FindOrLoad(factory, fname);
    fn_Filename_Dtor(fname);
    *(void**)(entry + EXTREF_TEMPLATE) = tmpl;               // cache
    return tmpl;
}

// ComponentEntity_impl_09e0 (0x1400a09e0): the simplest spawn pass. For each
// entry in the pool at startConfig+0x90 (count at +0x98), resolve its template
// and spawn+register a copy. No transforms, no terrain queries.
static int StartTown_SpawnSimplePool(uint8_t* startConfig, void* entityList) {
    const int count = *(int32_t*)(startConfig + 0x98);
    uint8_t* pool = *(uint8_t**)(startConfig + 0x90);
    if (count <= 0 || !pool) return 0;
    int spawned = 0;
    for (int i = 0; i < count; ++i) {
        uint8_t* entry = pool + (size_t)i * POOLENTRY_STRIDE;
        void* tmpl = StartTown_ResolveTemplate(entry);
        fprintf(stderr, "[startTown::spawn] simple[%d] tmpl=%p\n", i, tmpl);
        fflush(stderr);
        if (SpawnAndRegister(tmpl, entityList)) ++spawned;
    }
    return spawned;
}

// ============================================================================
// STRUCTURES pool — ComponentEntity_impl_1fc0 (0x1400a1fc0). Spawns the initial
// buildings (e.g. StorageBarn x2 on Medium) around the start-town spawn tile.
//
// For each entry in the pool at startConfig+0x80 (count +0x88, stride 0x30):
//   * instantiate the template (CopyCtor) and wrap it in an IBP
//   * read the placement footprint from the template's CreatePlacedDescription
//     (+0x44 width, +0x48 height), falling back to the entry's own +0x20/+0x24
//   * derive the tile position: spawnTile + entry offset (+0x18 dx, +0x1c dy),
//     swapping width/height for the 90/270-degree orientations (+0x28)
//   * find the MapComponent on the new entity and write its grid placement
//     (+0x28 tileX, +0x2a tileY, +0x2c w, +0x2e h, +0x30 rotation)
//   * register the entity in the world list (category = template+0x20)
//
// The original then sets up the ModelComponent render transform + a random mesh
// variant (Graphics::Geometry::SetTransform / MeshInstance::SetMesh). Those
// dispatch on the component's Geometry sub-object, which is zeroed on our inert
// components (Foundation D/E) — so the rendering block is guarded off until
// ModelComponent graduates to a real renderable. The entity still exists in the
// world at its correct grid footprint.
// ============================================================================

static constexpr int STRUCT_STRIDE   = 0x30;   // structure pool entry stride
static constexpr int ENTRY_OFFX      = 0x18;
static constexpr int ENTRY_OFFY      = 0x1c;
static constexpr int ENTRY_DEF_W     = 0x20;   // entry footprint fallback width
static constexpr int ENTRY_DEF_H     = 0x24;   // entry footprint fallback height
static constexpr int ENTRY_ORIENT    = 0x28;   // entry orientation code (degrees)
static constexpr int ENT_COMP_ARR    = 0x30;   // entity component-IBP array
static constexpr int ENT_COMP_CNT    = 0x38;   // entity component count
static constexpr int DESC_SUB_ARR    = 0x28;   // template/description sub-block array
static constexpr int DESC_SUB_CNT    = 0x30;   // template/description sub-block count
static constexpr int ROT_FULL        = 0x168;  // 360 (rotation modulus)

// Cached type id by name (after Object::Factory::Startup these are stable CRC32s).
static uint32_t TypeId(const char* name) {
    return ::Object::GetTypeIdByName(name);
}

// Component find-loop over a freshly-instantiated entity. Each entity component
// slot holds an IBP (CheckPtr resolves it); match on vtable+0x10 (GetTypeId).
// Returns the resolved component pointer, or nullptr.
static void* FindComponentOnEntity(void* entity, uint32_t typeId) {
    if (!entity || typeId == 0 || !fn_InternalBasePtr_CheckPtr) return nullptr;
    uint8_t* e = (uint8_t*)entity;
    int count = *(int32_t*)(e + ENT_COMP_CNT);
    uintptr_t arr = *(uintptr_t*)(e + ENT_COMP_ARR);
    for (int i = 0; i < count; ++i) {
        void** slot = (void**)(arr + (uintptr_t)i * 8);
        fn_InternalBasePtr_CheckPtr(slot);
        void* comp = *slot;
        if (!comp) continue;
        uintptr_t vt = *(uintptr_t*)comp;
        typedef uint32_t (*fn_GetTypeId_t)(void*);
        uint32_t id = (*(fn_GetTypeId_t*)(vt + 0x10))(comp);
        if (id == typeId) return comp;
    }
    return nullptr;
}

// Description find-loop over a template's sub-block list. Each slot is a raw
// description pointer (no IBP); match on vtable+0x10 (GetTypeId). Returns the
// description pointer or nullptr.
static void* FindDescriptionOnTemplate(void* tmpl, uint32_t typeId) {
    if (!tmpl || typeId == 0) return nullptr;
    uint8_t* t = (uint8_t*)tmpl;
    int count = *(int32_t*)(t + DESC_SUB_CNT);
    uintptr_t arr = *(uintptr_t*)(t + DESC_SUB_ARR);
    for (int i = 0; i < count; ++i) {
        void* desc = *(void**)(arr + (uintptr_t)i * 8);
        if (!desc) continue;
        uintptr_t vt = *(uintptr_t*)desc;
        typedef uint32_t (*fn_GetTypeId_t)(void*);
        uint32_t id = (*(fn_GetTypeId_t*)(vt + 0x10))(desc);
        if (id == typeId) return desc;
    }
    return nullptr;
}

// Faithful 1:1 port of FUN_1400c2ea0 — the ComponentEntity OnCreate pass. The
// original game runs this on each entity during a deferred world-activation phase
// (NOT in the CopyCtor / EntityList::RegisterEntity spawn flow, and NOT in the
// structures spawn pass ComponentEntity_impl_1fc0). In standalone that activation
// phase is not wired, so the start-town structures spawn invokes this directly,
// right after RegisterEntity — at which point the entity is fully registered, the
// exact precondition OnCreate expects.
//
// Disassembly (0x1400c2ea0):
//   if (entity+0x40 & 2):                       // entity has OnCreate-capable components
//     for each component IBP (count @ entity+0x38, array @ entity+0x30):
//       CheckPtr; if (comp+0x20 >> 1 & 1):      // component flag bit 1
//         CheckPtr; CALL [comp_vtable + 0x38]   // OnCreate
//   entity+0x40 |= 0x400                         // "OnCreate done" sentinel (always set)
//
// For a WoodHouse this calls ResidenceComponent::OnCreate (+0x38), which (once the
// BANISHED_LIVE_RESIDENCE factory routes that slot) pushes the house into
// townMgr+0x20. Gated on the env so the default build never runs it.
#ifdef STANDALONE_EXE
// SURGICAL standalone variant: the faithful FUN_1400c2ea0 fires OnCreate(+0x38) on
// EVERY bit-1 component, but several of those are only partially live in standalone
// (notably the renderable ModelComponent, whose OnCreate re-initialises the mesh and
// wipes the completed-build variant we hand-set in StartTown_SpawnStructures → the
// building goes invisible). So we run OnCreate ONLY on the ResidenceComponent — the
// one component brought up for the townMgr+0x20 residence registration. The full
// faithful pass is restored once the other components' OnCreate are verified
// standalone-safe.
//
// IMPORTANT — we do NOT set the 0x400 entity-flag bit here. FUN_1400c2ea0 is the
// entity *removal* pass (its real callers are death / migration / removal-tool /
// resource-depletion — see xrefs), and 0x400 is its "removed from world" marker.
// The faithful per-frame update system treats bit 0x400 as "skip this entity":
// EntityList::Update's 5-bucket +0x48 tick and the +0x40 tree walk both bail on
// `(entity+0x40 >> 10) & 1`. Retail never runs c2ea0 on a live start-town building,
// so those buildings keep bit 0x400 CLEAR and tick normally. We only borrow c2ea0's
// component-walk shape to dispatch ResidenceComponent::OnCreate (townMgr registration);
// stamping the removal sentinel would (wrongly) mark the live building removed and
// permanently exclude its components — e.g. StorageDisplayComponent::Update — from the
// native tick, which is exactly the bug this omission fixes.
static void StartTown_RunOnCreatePass(void* entityV) {
    if (!entityV || !fn_InternalBasePtr_CheckPtr) return;
    if (!::ComponentFactory::LiveEnabled("BANISHED_LIVE_RESIDENCE")) return;

    static uint32_t s_tidResidence = 0;
    if (s_tidResidence == 0) s_tidResidence = TypeId("ResidenceComponent");

    uint8_t* ent = (uint8_t*)entityV;
    uint32_t* entFlags = (uint32_t*)(ent + 0x40);
    if ((*entFlags & 2) != 0 && s_tidResidence != 0) {
        int count = *(int32_t*)(ent + ENT_COMP_CNT);
        uintptr_t arr = *(uintptr_t*)(ent + ENT_COMP_ARR);
        for (int i = 0; i < count; ++i) {
            void** slot = (void**)(arr + (uintptr_t)i * 8);
            fn_InternalBasePtr_CheckPtr(slot);
            void* comp = *slot;
            if (!comp) continue;
            if (((*(uint32_t*)((uint8_t*)comp + 0x20)) >> 1 & 1) == 0) continue;  // flag bit 1
            uintptr_t vt = *(uintptr_t*)comp;
            typedef uint32_t (*fn_GetTypeId_t)(void*);
            uint32_t tid = (*(fn_GetTypeId_t*)(vt + 0x10))(comp);
            if (tid != s_tidResidence) continue;                 // ONLY ResidenceComponent
            fn_InternalBasePtr_CheckPtr(slot);
            comp = *slot;
            if (!comp) continue;
            vt = *(uintptr_t*)comp;
            typedef void (*fn_OnCreate_t)(void*);
            (*(fn_OnCreate_t*)(vt + 0x38))(comp);                // ResidenceComponent::OnCreate
            // Capture the house's owner-entity IBP (ResidenceComponent+0x10 — the exact
            // value the original registration uses) for the founding-housing pass. NB the
            // original OnCreate's "registration" (FUN_140002cc0 on townMgr+0x20) is actually
            // a remove-by-value → townMgr+0x20 is never populated in standalone (residences
            // are listed at building-PLACEMENT time, which we don't run). So we keep the
            // house IBPs here and pair them to families directly.
            g_foundingHouseIBPs.push_back(*reinterpret_cast<int64_t*>((uint8_t*)comp + 0x10));
            fprintf(stderr, "[startTown::residence] ResidenceComponent::OnCreate fired "
                            "(house #%zu captured, ownerIBP=%llx)\n",
                            g_foundingHouseIBPs.size() - 1,
                            (unsigned long long)g_foundingHouseIBPs.back());
            fflush(stderr);
        }
    }
    // (Deliberately NOT setting *entFlags |= 0x400 — see the note above: that is
    // c2ea0's "removed from world" marker and would exclude this live building from
    // the faithful +0x48 / +0x40 tick.)
    (void)entFlags;
}

// TODO #2 gating diagnostic — does the workplace profession key resolve LIVE in
// standalone? For each spawned building's WorkPlaceComponent: desc = comp+0x18
// (WorkPlaceDescription), profKey = desc+0x20 (Profession*, the key the registry
// node is keyed by). Also logs the world profession list (gameWorld+0xb0) and the
// component's own +0x48 key (populated only by the inert OnCreate, so expected 0
// here). If profKey is NULL, the registry port (TODO #2) would seed nodes with
// garbage keys → ProfessionList/Profession.rsc must be wired first. Read-only,
// never mutates. Gated on BANISHED_PROBE_WORKPLACE_KEY.
// WAVE 2 helper: read an Object::External's resource filename (base+8 = Filename
// String*; engine String = char data with int length at ptr-8). Bounded/safe.
static void ProbeExtFilename(void* ext, char* out, int outcap) {
    if (outcap <= 0) return;
    out[0] = '\0';
    if (!ext) { snprintf(out, outcap, "(null-ext)"); return; }
    char* str = *reinterpret_cast<char**>(reinterpret_cast<uint8_t*>(ext) + 8);
    if (!str) { snprintf(out, outcap, "(none)"); return; }
    int len = *reinterpret_cast<int*>(str - 8);
    if (len <= 0 || len > 4096) { snprintf(out, outcap, "(badlen=%d)", len); return; }
    int n = len < outcap - 1 ? len : outcap - 1;
    for (int i = 0; i < n; ++i) { char c = str[i]; out[i] = (c >= 32 && c < 127) ? c : '?'; }
    out[n] = '\0';
}

static void StartTown_ProbeWorkplaceKey(void* entityV) {
    if (!entityV || !fn_InternalBasePtr_CheckPtr) return;
    if (!::ComponentFactory::LiveEnabled("BANISHED_PROBE_WORKPLACE_KEY")) return;

    static uint32_t s_tidWork = 0;
    if (s_tidWork == 0) s_tidWork = TypeId("WorkPlaceComponent");
    if (s_tidWork == 0) return;

    void* profList = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                   ? *(void**)((uint8_t*)*g_pGameWorldSingleton + 0xb0) : nullptr;

    uint8_t* ent = (uint8_t*)entityV;
    int count = *(int32_t*)(ent + ENT_COMP_CNT);
    uintptr_t arr = *(uintptr_t*)(ent + ENT_COMP_ARR);
    for (int i = 0; i < count; ++i) {
        void** slot = (void**)(arr + (uintptr_t)i * 8);
        fn_InternalBasePtr_CheckPtr(slot);
        void* comp = *slot;
        if (!comp) continue;
        uintptr_t vt = *(uintptr_t*)comp;
        typedef uint32_t (*fn_GetTypeId_t)(void*);
        uint32_t tid = (*(fn_GetTypeId_t*)(vt + 0x10))(comp);
        if (tid != s_tidWork) continue;
        void* desc    = *(void**)((uint8_t*)comp + 0x18);
        void* profKey = desc ? *(void**)((uint8_t*)desc + 0x20) : nullptr;
        uint64_t curKey = *(uint64_t*)((uint8_t*)comp + 0x48);
        // WAVE 2: the live desc's External filename. Compare to [probe::wpdesc-serialize]
        // extFile: SAME path => same resource loaded twice (FindOrLoad dedup-key mismatch);
        // "(none)" => this live desc was factory-created and NEVER loaded (no External
        // filename) => the duplication is inline/copy, not a second External::Load.
        char descFile[96]; ProbeExtFilename(desc, descFile, sizeof descFile);
        fprintf(stderr, "[probe::workplace] comp=%p desc=%p profKey(desc+0x20)=%p "
                        "compKey(+0x48)=0x%llx professionList(gw+0xb0)=%p descFile='%s'\n",
                comp, desc, profKey, (unsigned long long)curKey, profList, descFile);
        fflush(stderr);
    }
}

// Economy Brick M-D — bring up the storage on a freshly spawned FULLY-BUILT
// building (StorageBarn/StorageYard). Mirrors what retail does for the same
// entity state:
//   1. build-complete storage init (BuildDescription_helper_7ff0): capacity
//      +0x38 = _volumeLimit(desc+0x24) * (_areaBasedLimit(desc+0x20) ? mapW*mapH
//      : 1), mask +0x3c = _storageFlags(desc+0x28). Passing the StorageComponent
//      itself is layout-equivalent to retail's BuildComponent (both hold the
//      entity IBP at +0x10).
//   2. town storage-location registration (StorageLocationComponent OnAdded →
//      d230): append the entity to townMgr+0x00 so the citizen eat chain
//      (CitizenAI_helper_e370) and Game_SpawnResources can find it.
// Our CopyCtor already wires comp+0x10 (entity) and +0x18 (description).
static void StartTown_WireStorage(void* entity, int entryIdx) {
    if (!LiveStorageEnabled() || !entity) return;
    void* storageComp = FindComponentOnEntity(entity, TypeId("StorageComponent"));
    if (!storageComp) return;                      // not a storage building
    TownStorage_BuildCompleteInit_7ff0(storageComp);

    void* slc = FindComponentOnEntity(entity, TypeId("StorageLocationComponent"));
    // FAITHFUL registration set: retail's townMgr+0x00 ADD (d230) fires from the
    // StorageLocationComponent lifecycle, so ONLY `storagelocation`-bearing
    // templates (StorageBarn/StorageYard/StorageCart/Tavern) join the list. The
    // WoodHouses carry a plain StorageComponent (food/fuel at home) but are NOT
    // storage locations — registering them made the citizen food-search + the
    // start-resource seeder treat houses as barns (unfaithful; also crashed the
    // town-list searches pre-fallback). Init (7ff0) still runs for them above.
    if (slc) TownRegistry_RegisterStorageLocation(slc);
    if (StorageTraceOn()) {
        uint8_t* sc = (uint8_t*)storageComp;
        fprintf(stderr, "[StorageSeed] struct[%d] entity=%p storage=%p slc=%p "
                "cap=%d mask=0x%x\n", entryIdx, entity, storageComp, slc,
                *(int32_t*)(sc + 0x38), *(uint32_t*)(sc + 0x3c));
        fflush(stderr);
    }
}
#endif // STANDALONE_EXE

// NOTE: the resource path now uses StartTown_PlaceResourceCell (the Entity_impl_6540
// port: MapComponent grid cell + jittered ModelComponent transform). The structures
// path sets its ModelComponent transform inline (see StartTown_SpawnStructures).

// Defined further down (resource model-variant section); used by the structures path
// to swap a building from its foundation mesh to the completed-build mesh.
static void ApplyResourceModelVariant(void* entity, uint32_t tidModel,
                                      int16_t variantIdx, int16_t modelVariant);

// ComponentEntity_impl_1fc0 — spawn the initial buildings around (spawnX,spawnY).
static int StartTown_SpawnStructures(uint8_t* startConfig, void* entityList,
                                     void* game, int spawnX, int spawnY) {
    const int count = *(int32_t*)(startConfig + 0x88);
    uint8_t* pool   = *(uint8_t**)(startConfig + 0x80);
    if (count <= 0 || !pool) return 0;
    if (!fn_EntityFactory_CreateEntity || !fn_EntityList_RegisterEntity ||
        !fn_InternalBasePtr_Ctor || !fn_InternalBasePtr_CheckPtr || !fn_MemAlloc)
        return 0;

    const uint32_t tidCreatePlaced  = TypeId("CreatePlacedDescription");
    const uint32_t tidMapComponent  = TypeId("MapComponent");
    const uint32_t tidModelComponent = TypeId("ModelComponent");

    int spawned = 0;
    for (int i = 0; i < count; ++i) {
        uint8_t* entry = pool + (size_t)i * STRUCT_STRIDE;

        void* tmpl = StartTown_ResolveTemplate(entry);
        if (!tmpl) {
            fprintf(stderr, "[startTown::struct] [%d] template unresolved\n", i);
            fflush(stderr);
            continue;
        }

        // Instantiate + wrap.
        void* mem = fn_MemAlloc(g_pMemAllocSingleton, ENTITY_SIZE, 8);
        void* ent = mem ? fn_EntityFactory_CreateEntity(mem, tmpl) : nullptr;
        uintptr_t entIBP = 0;
        fn_InternalBasePtr_Ctor(&entIBP, ent);

        // Footprint: CreatePlacedDescription on the template, else entry defaults.
        int w0, h0;
        void* placed = FindDescriptionOnTemplate(tmpl, tidCreatePlaced);
        if (placed) {
            w0 = *(int32_t*)((uint8_t*)placed + 0x44);
            h0 = *(int32_t*)((uint8_t*)placed + 0x48);
        } else {
            w0 = *(int32_t*)(entry + ENTRY_DEF_W);
            h0 = *(int32_t*)(entry + ENTRY_DEF_H);
        }

        const int offX   = *(int32_t*)(entry + ENTRY_OFFX);
        const int offY   = *(int32_t*)(entry + ENTRY_OFFY);
        const int orient = *(int32_t*)(entry + ENTRY_ORIENT);
        const int tileX  = spawnX + offX;
        const int tileY  = spawnY + offY;

        // 90 / -90 / 270 degrees swap the footprint axes.
        int fpW = w0, fpH = h0;
        if (orient == 0x5a || orient == -0x5a || orient == 0x10e) { fpW = h0; fpH = w0; }

        int rot = orient;
        while (rot < 0) rot += ROT_FULL;

        // MapComponent grid placement (resolvable now that components carry a real
        // GetTypeId — Foundation E).
        fn_InternalBasePtr_CheckPtr(&entIBP);
        void* entity = (void*)entIBP;

        void* mapComp = FindComponentOnEntity(entity, tidMapComponent);
        if (mapComp) {
            uintptr_t mapIBP = 0;
            fn_InternalBasePtr_Ctor(&mapIBP, mapComp);
            fn_InternalBasePtr_CheckPtr(&mapIBP);
            uint8_t* mc = (uint8_t*)mapIBP;
            if (mc) {
                *(int16_t*)(mc + 0x28) = (int16_t)tileX;
                *(int16_t*)(mc + 0x2a) = (int16_t)tileY;
                *(int16_t*)(mc + 0x2c) = (int16_t)fpW;
                *(int16_t*)(mc + 0x2e) = (int16_t)fpH;
                *(int32_t*)(mc + 0x30) = rot;
            }
        }

        // ── ModelComponent world TRANSFORM — faithful 1:1 port of the transform
        // block of ComponentEntity_impl_1fc0 (0x1400a218b..0x1400a2513).
        //
        // Retail derives the footprint-centre world position by terrain-sampling the
        // footprint's origin corner and its far corner (both as flat tile indices
        // into the map grid), midpointing the two world positions, then building a
        // yaw-only Transform there:
        //   dim      = *mapGrid                         (mapGrid = *(gw+0xf8) = `game`)
        //   corner1  = dim*tileY + tileX                (footprint origin)
        //   corner2  = (tileY+fpH)*dim + tileX + fpW    (footprint far corner)
        //   worldXY  = f3e0(mapGrid, {idx%dim - (dim>>1), idx/dim - (dim>>1), 0})
        //              == fn_MapGetTerrainPos (0x14009f3e0, verified identical)
        //   centre   = (world1 + world2) * DAT_140172114  (=0.5f, kCenterBias)
        //   yaw      = (float)orient   (raw entry+0x28, pre-normalisation); pitch = 0
        // Retail then: FromYawPitchOffset(tmp,&yaw,&pitch,&centre) ->
        //   Transform::operator=(model+0x148,tmp) -> model+0x168 = yaw ->
        //   Graphics::Geometry::SetTransform(model+0x28, model+0x148)  (0x1400a250f).
        // `game` is the already-deref'd mapGrid (caller: game = *(gw+0xf8), line ~2790),
        // so dim = *(int*)game and f3e0's first arg is `game`, exactly retail's *local_b0.
        if (game && fn_MapGetTerrainPos && fn_Transform_FromYawPitchOffset) {
            void* modelComp = FindComponentOnEntity(entity, tidModelComponent);
            if (modelComp) {
                uintptr_t mcIBP = 0;
                fn_InternalBasePtr_Ctor(&mcIBP, modelComp);
                fn_InternalBasePtr_CheckPtr(&mcIBP);
                uint8_t* mc = (uint8_t*)mcIBP;
                const int dim = *(int32_t*)game;
                if (mc && dim > 0) {
                    const float half = (float)(dim >> 1);
                    // corner1 — footprint origin (tileX, tileY).
                    int idx1 = dim * tileY + tileX;
                    float in1[3] = { (float)(idx1 % dim) - half, (float)(idx1 / dim) - half, 0.0f };
                    float p1[3] = {0,0,0};
                    fn_MapGetTerrainPos(game, p1, in1, '\0', nullptr);
                    // corner2 — footprint far corner (tileX+fpW, tileY+fpH).
                    int idx2 = (tileY + fpH) * dim + tileX + fpW;
                    float in2[3] = { (float)(idx2 % dim) - half, (float)(idx2 / dim) - half, 0.0f };
                    float p2[3] = {0,0,0};
                    fn_MapGetTerrainPos(game, p2, in2, '\0', nullptr);
                    float center[3] = { (p1[0]+p2[0])*kCenterBias,     // DAT_140172114 = 0.5f
                                        (p1[1]+p2[1])*kCenterBias,
                                        (p1[2]+p2[2])*kCenterBias };
                    float yaw = (float)orient, pitch = 0.0f;
                    // FromYawPitchOffset writes the Transform straight into +0x148 (byte-
                    // identical to retail's tmp-retbuf + Transform::operator=(+0x148,tmp)).
                    fn_Transform_FromYawPitchOffset((void*)(mc + 0x148), &yaw, &pitch, center);
                    *(float*)(mc + 0x168) = yaw;   // MOVSS [model+0x168], (float)orient
                    // Graphics::Geometry::SetTransform(model+0x28, model+0x148) — the piece the
                    // old approximation dropped (it assumed ModelComponent_Draw would push it
                    // later). Without this the Geometry/MeshInstance at +0x28 (and its derived
                    // world-pos +0x158) never picks up the +0x148 matrix, leaving it stale/0.
                    if (fn_Geometry_SetTransform)
                        fn_Geometry_SetTransform((void*)(mc + 0x28), (void*)(mc + 0x148));
                }
            }
        }

        // --- Show the COMPLETED building, not its foundation. -----------------
        // A building MeshGroup encodes its construction stages as the sub-mesh list
        // [Build01, Build02, ..., complete]; ModelComponent::Startup leaves it at the
        // foundation (modelVariant 0) and the real game advances it to the last
        // sub-mesh when its BuildComponent finishes — but that component is inert in
        // standalone, so InitialStructure buildings would sit at the foundation mesh.
        // Mirror ComponentEntity_impl_1fc0: pick a random visual style (variantIdx via
        // the world RNG, like 1fc0) but force the LAST sub-mesh = the finished build.
        {
            void* modelComp = FindComponentOnEntity(entity, tidModelComponent);
            if (modelComp) {
                uintptr_t mIBP = 0;
                fn_InternalBasePtr_Ctor(&mIBP, modelComp);
                fn_InternalBasePtr_CheckPtr(&mIBP);
                uint8_t* m = (uint8_t*)mIBP;
                void* modelDesc = m ? *(void**)(m + 0x18) : nullptr;
                void* groupArray = modelDesc ? *(void**)((uint8_t*)modelDesc + 0x20) : nullptr;
                uint32_t groupCount = modelDesc ? *(uint32_t*)((uint8_t*)modelDesc + 0x28) : 0;
                if (groupArray && groupCount > 0) {
                    int variantIdx = 0;
                    void* rng = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                              ? *(void**)((uint8_t*)*g_pGameWorldSingleton + 0xd8) : nullptr;
                    if (rng && fn_Random_Rand && groupCount > 1)
                        variantIdx = (int)fn_Random_Rand(rng, groupCount);
                    int subCount = *(int32_t*)((uint8_t*)groupArray + (int64_t)variantIdx * 0x18 + 0x10);
                    int modelVariant = (subCount > 0) ? subCount - 1 : 0;  // last sub-mesh = complete
                    ApplyResourceModelVariant(entity, tidModelComponent,
                                              (int16_t)variantIdx, (int16_t)modelVariant);
                    fprintf(stderr, "[startTown::struct] [%d] complete-model variantIdx=%d "
                            "modelVariant=%d (subCount=%d groups=%u)\n",
                            i, variantIdx, modelVariant, subCount, groupCount);
                    fflush(stderr);
                }
            }
        }

        // Register with the world entity list (category = template+0x20).
        int category = *(int32_t*)((uint8_t*)tmpl + TEMPLATE_CATEGORY);
        fn_EntityList_RegisterEntity(entityList, &entIBP, category);

#ifdef STANDALONE_EXE
        // Run the OnCreate pass on the freshly-registered entity (deferred-activation
        // equivalent — see StartTown_RunOnCreatePass). For a WoodHouse this fires
        // ResidenceComponent::OnCreate → registers the house into townMgr+0x20.
        // The entity IBP was re-resolved above (entity = (void*)entIBP); re-resolve
        // once more in case any intervening CheckPtr moved it.
        fn_InternalBasePtr_CheckPtr(&entIBP);
        StartTown_RunOnCreatePass((void*)entIBP);
        StartTown_ProbeWorkplaceKey((void*)entIBP);   // TODO #2 gating diagnostic

        // Click-to-select: register this building's footprint in the region-map cell array so
        // EditTool's click-pick (FUN_1400a0280) resolves it under the cursor -> opens its info
        // dialog. (Inert MapComponent never marks it; standalone-only, DLL uses the real dc20.)
        fn_InternalBasePtr_CheckPtr(&entIBP);
        StartTown_MarkBuildingCells(game, tileX, tileY, fpW, fpH, entIBP);
        StartTown_PickMeshProbe((void*)entIBP);

        // Brick M-D (BANISHED_LIVE_STORAGE): run the build-complete storage init
        // + town storage-location registration for this fully-built building —
        // retail's StorageLocationComponent OnAdded fires during RegisterEntity,
        // so this sits right after it.
        StartTown_WireStorage(entity, i);
#endif

        fprintf(stderr, "[startTown::struct] [%d] tmpl=%p tile=(%d,%d) fp=%dx%d rot=%d map=%p\n",
                i, tmpl, tileX, tileY, fpW, fpH, rot, mapComp);
        fflush(stderr);
        if (ent) {
            ++spawned;
            // Capture the first structure entity for the BANISHED_FORCE_WORK harness.
            if (!g_pForceWorkBuilding) g_pForceWorkBuilding = entity;
        }
    }
    return spawned;
}

// ============================================================================
// PHASE A — real PRODUCTION workplace (env BANISHED_LIVE_WORKPLACE, default off).
//
// The economy WORK-half stalled because the force-work harness pointed a citizen's
// workplace (+0x238) at a StorageBarn, but `Entity_FindComponent_Storage` wants a
// ResidenceComponent worker-ROSTER (c7cf0) on the workplace — which a storage-only
// building lacks. A producer template (ForesterLodge, the simplest) carries the full
// set: ResidenceComponent (roster) + WorkPlaceComponent (c80e0) + a RawMaterial/Forester
// producer + Attendance. KEY LEVER: inert factories still produce VALID instances
// (correct type-id, valid memory), so spawning the template should let FindStorage
// resolve the (inert) ResidenceComponent with ZERO component graduation.
//
// Phase A: spawn it, place it like an InitialStructure (so its cell + mesh resolve),
// DUMP its component type-ids to confirm the set, and re-point g_pForceWorkBuilding at
// it (so BANISHED_FORCE_WORK assigns a citizen here instead of the StorageBarn).
// Phase B then graduates the producer components → real production.
// ============================================================================
static bool WorkplaceEnvOn() {
    static int en = -1;
    if (en < 0) {
        const char* e = std::getenv("BANISHED_LIVE_WORKPLACE");
        en = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return en == 1;
}

// Resolve a template by its full "Template\Name.rsc" path (FindOrLoad-by-cstring),
// mirroring StartTown_ResolveTemplate but without a pool-entry String.
static void* ResolveTemplateByPath(const char* path) {
    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton ||
        !fn_Filename_CtorCStr || !fn_ExternalFactory_FindOrLoad || !fn_Filename_Dtor)
        return nullptr;
    void* factory = *g_pExternalFactorySingleton;
    uint8_t fname[8] = {0};
    fn_Filename_CtorCStr(fname, path);
    void* tmpl = fn_ExternalFactory_FindOrLoad(factory, fname);
    fn_Filename_Dtor(fname);
    return tmpl;
}

#ifdef STANDALONE_EXE
// Exposed for the standalone tool-injection harness (standalone/tool_harness.cpp):
// resolve a building DESCRIPTION template ("Template\\<Name>.rsc") to point a
// PlacementTool's descriptionPtr (+0x20) at. Thin extern "C" wrapper over the
// static resolver so the harness need not re-plumb the ExternalFactory singleton.
extern "C" void* StartTown_ResolveTemplateByPath(const char* path) {
    return ResolveTemplateByPath(path);
}
#endif

static void StartTown_SpawnWorkplace(uint8_t* startConfig, void* entityList,
                                     void* game, int spawnX, int spawnY) {
    (void)startConfig;
    if (!WorkplaceEnvOn()) return;
    if (!fn_EntityFactory_CreateEntity || !fn_EntityList_RegisterEntity ||
        !fn_InternalBasePtr_Ctor || !fn_InternalBasePtr_CheckPtr || !fn_MemAlloc) return;

    const char* path = "Template\\ForesterLodge.rsc";
    void* tmpl = ResolveTemplateByPath(path);
    fprintf(stderr, "[startTown::workplace] resolve \"%s\" -> tmpl=%p\n", path, tmpl);
    fflush(stderr);
    if (!tmpl) return;

    const uint32_t tidCreatePlaced   = TypeId("CreatePlacedDescription");
    const uint32_t tidMapComponent   = TypeId("MapComponent");
    const uint32_t tidModelComponent = TypeId("ModelComponent");

    // Instantiate + wrap.
    void* mem = fn_MemAlloc(g_pMemAllocSingleton, ENTITY_SIZE, 8);
    void* ent = mem ? fn_EntityFactory_CreateEntity(mem, tmpl) : nullptr;
    if (!ent) { fprintf(stderr, "[startTown::workplace] instantiate failed\n"); fflush(stderr); return; }
    uintptr_t entIBP = 0;
    fn_InternalBasePtr_Ctor(&entIBP, ent);

    // Footprint from CreatePlacedDescription (else 4x4).
    int w0 = 4, h0 = 4;
    void* placed = FindDescriptionOnTemplate(tmpl, tidCreatePlaced);
    if (placed) { w0 = *(int32_t*)((uint8_t*)placed + 0x44); h0 = *(int32_t*)((uint8_t*)placed + 0x48); }

    // Place a few tiles to the side of the town centre so it doesn't overlap the
    // InitialStructure footprints. (Diagnostic placement; exact tile is unimportant.)
    const int tileX = spawnX + 8, tileY = spawnY;
    const int fpW = w0, fpH = h0, rot = 0;

    fn_InternalBasePtr_CheckPtr(&entIBP);
    void* entity = (void*)entIBP;

    // MapComponent grid placement (the cell resolver Entity_impl_7fb0 PATH 2 reads
    // these packed tile coords to give the workplace a navigable anchor cell).
    void* mapComp = FindComponentOnEntity(entity, tidMapComponent);
    if (mapComp) {
        uintptr_t mapIBP = 0;
        fn_InternalBasePtr_Ctor(&mapIBP, mapComp);
        fn_InternalBasePtr_CheckPtr(&mapIBP);
        uint8_t* mc = (uint8_t*)mapIBP;
        if (mc) {
            *(int16_t*)(mc + 0x28) = (int16_t)tileX;
            *(int16_t*)(mc + 0x2a) = (int16_t)tileY;
            *(int16_t*)(mc + 0x2c) = (int16_t)fpW;
            *(int16_t*)(mc + 0x2e) = (int16_t)fpH;
            *(int32_t*)(mc + 0x30) = rot;
        }
    }

    // Foundation-F world transform (footprint-centre terrain position) so the
    // building's OnAdded places its mesh, not at the world origin.
    if (game && fn_MapGetTerrainPos && fn_Transform_FromYawPitchOffset) {
        void* modelComp = FindComponentOnEntity(entity, tidModelComponent);
        if (modelComp) {
            uintptr_t mcIBP = 0;
            fn_InternalBasePtr_Ctor(&mcIBP, modelComp);
            fn_InternalBasePtr_CheckPtr(&mcIBP);
            uint8_t* m = (uint8_t*)mcIBP;
            int dim = *(int32_t*)game;
            if (m && dim > 0) {
                const float half = (float)(dim >> 1);
                int idx1 = dim * tileY + tileX;
                float in1[3] = { (float)(idx1 % dim) - half, (float)(idx1 / dim) - half, 0.0f };
                float p1[3] = {0,0,0};
                fn_MapGetTerrainPos(game, p1, in1, 0, nullptr);
                int idx2 = (tileY + fpH) * dim + tileX + fpW;
                float in2[3] = { (float)(idx2 % dim) - half, (float)(idx2 / dim) - half, 0.0f };
                float p2[3] = {0,0,0};
                fn_MapGetTerrainPos(game, p2, in2, 0, nullptr);
                float center[3] = { (p1[0]+p2[0])*0.5f, (p1[1]+p2[1])*0.5f, (p1[2]+p2[2])*0.5f };
                float yaw = (float)rot, pitch = 0.0f;
                fn_Transform_FromYawPitchOffset((void*)(m + 0x148), &yaw, &pitch, center);
                *(float*)(m + 0x168) = yaw;
            }
        }
    }

    // Show the COMPLETED building mesh (last sub-mesh), same as the InitialStructures.
    {
        void* modelComp = FindComponentOnEntity(entity, tidModelComponent);
        if (modelComp) {
            uintptr_t mIBP = 0;
            fn_InternalBasePtr_Ctor(&mIBP, modelComp);
            fn_InternalBasePtr_CheckPtr(&mIBP);
            uint8_t* m = (uint8_t*)mIBP;
            void* modelDesc = m ? *(void**)(m + 0x18) : nullptr;
            void* groupArray = modelDesc ? *(void**)((uint8_t*)modelDesc + 0x20) : nullptr;
            uint32_t groupCount = modelDesc ? *(uint32_t*)((uint8_t*)modelDesc + 0x28) : 0;
            if (groupArray && groupCount > 0) {
                int variantIdx = 0;
                void* rng = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                          ? *(void**)((uint8_t*)*g_pGameWorldSingleton + 0xd8) : nullptr;
                if (rng && fn_Random_Rand && groupCount > 1)
                    variantIdx = (int)fn_Random_Rand(rng, groupCount);
                int subCount = *(int32_t*)((uint8_t*)groupArray + (int64_t)variantIdx * 0x18 + 0x10);
                int modelVariant = (subCount > 0) ? subCount - 1 : 0;
                ApplyResourceModelVariant(entity, tidModelComponent,
                                          (int16_t)variantIdx, (int16_t)modelVariant);
            }
        }
    }

    // Register with the world entity list (category = template+0x20).
    int category = *(int32_t*)((uint8_t*)tmpl + TEMPLATE_CATEGORY);
    fn_EntityList_RegisterEntity(entityList, &entIBP, category);

#ifdef STANDALONE_EXE
    fn_InternalBasePtr_CheckPtr(&entIBP);
    StartTown_RunOnCreatePass((void*)entIBP);   // fires ResidenceComponent::OnCreate (LIVE_RESIDENCE)
    // Click-to-select: mark the workplace footprint in the region-map cell array (see
    // StartTown_MarkBuildingCells) so EditTool's click-pick resolves it under the cursor.
    fn_InternalBasePtr_CheckPtr(&entIBP);
    StartTown_MarkBuildingCells(game, tileX, tileY, fpW, fpH, entIBP);
#endif

    // ---- THE PHASE A DIAGNOSTIC: dump the component set. --------------------
    fn_InternalBasePtr_CheckPtr(&entIBP);
    entity = (void*)entIBP;
    {
        uint8_t* e = (uint8_t*)entity;
        int count = *(int32_t*)(e + ENT_COMP_CNT);
        uintptr_t arr = *(uintptr_t*)(e + ENT_COMP_ARR);
        uint32_t tidRes = TypeId("ResidenceComponent");
        uint32_t tidWP  = TypeId("WorkPlaceComponent");
        uint32_t tidRaw = TypeId("RawMaterialComponent");
        uint32_t tidAtt = TypeId("AttendanceComponent");
        uint32_t tidWork = TypeId("WorkComponent");
        fprintf(stderr, "[startTown::workplace] component dump (count=%d) want "
                "Residence=%u WorkPlace=%u RawMat=%u Attend=%u Work=%u\n",
                count, tidRes, tidWP, tidRaw, tidAtt, tidWork);
        for (int i = 0; i < count; ++i) {
            void** slot = (void**)(arr + (uintptr_t)i * 8);
            fn_InternalBasePtr_CheckPtr(slot);
            void* comp = *slot;
            if (!comp) { fprintf(stderr, "[startTown::workplace]   comp[%d] (null)\n", i); continue; }
            uintptr_t vt = *(uintptr_t*)comp;
            typedef uint32_t (*gt_t)(void*);
            uint32_t id = (*(gt_t*)(vt + 0x10))(comp);
            const char* tag = id==tidRes?" RESIDENCE(roster)":id==tidWP?" WORKPLACE":
                              id==tidRaw?" RAWMATERIAL":id==tidAtt?" ATTENDANCE":
                              id==tidWork?" WORK":"";
            fprintf(stderr, "[startTown::workplace]   comp[%d] tid=%u%s\n", i, id, tag);
        }
        fflush(stderr);
    }

    StartTown_PickMeshProbe(entity);

    // ---- Phase 1 infra: register the workplace into the townMgr+0x100 PROFESSION BST
    // (the list FindAvailableBuilding scans), keyed by WorkPlaceComponent+0x48. This is
    // the faithful list for a WORKPLACE — townMgr+0x20 is the RESIDENCE/house list
    // (scanned by sub_0e70, which correctly ignores a lodge since a lodge has no
    // ResidenceComponent). This only makes the lodge FINDABLE; actual worker ASSIGNMENT
    // is player-driven via the Phase-5 ProfessionUI worker-count widget (there is no
    // organic auto-assigner in retail). Mirrors the exe placement-path registration
    // (FUN_14012f520 / WorkPlaceDescription_helper_edf0 -> AddWorkplaceToProfessionTree).
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        int64_t townMgr = *(int64_t*)((uint8_t*)*g_pGameWorldSingleton + 0xf0);
        void* wpc = FindComponentOnEntity(entity, TypeId("WorkPlaceComponent"));
        if (townMgr && wpc) {
            // Phase 4: seat the WorkPlaceComponent profession key (WPC+0x48) from the
            // lodge's own WorkPlaceDescription (WPC+0x18 -> desc+0x20 = the forester
            // Profession*, the value ProfessionList/townMgr+0x100 key by). The retail
            // seater FUN_14012f520 reads it from the ProfessionList iteration cursor
            // (gw+0xb0+0x38); at spawn we take the equivalent value off our own desc so
            // registration + a FindAvailableBuilding query use the SAME key by
            // construction. Skip if the desc carries no profession (key stays 0).
            uint8_t* w = reinterpret_cast<uint8_t*>(wpc);
            void* wpDesc = *reinterpret_cast<void**>(w + 0x18);
            uint64_t profKey = wpDesc ? *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(wpDesc) + 0x20) : 0;
            if (profKey != 0) *reinterpret_cast<uint64_t*>(w + 0x48) = profKey;
            g_foresterProfKey = profKey;   // for the deferred employ pass

            // Enable the workplace via the faithful activation tail Entity_impl_fc00(WPC, 1)
            // (0x14012fc00): sets the accepting flag WPC+0x50 = 1 AND raises the "no workers"
            // warning status-icon (bit 4) since the lodge spawns unstaffed on its designated
            // job. This is the genuine build-complete -> accepting EVENT that drives the
            // warning; WorkplaceComponent_Attach clears it when the employ pass seats a
            // forester. (Replaces the old per-frame StatusIcon occupancy poll.) The accepting
            // flag also gates FindAvailableBuilding's reachability check (FUN_140002e10 ->
            // WPC+0x50 != 0); the inert lodge never ran the activation so it stayed 0.
            WorkPlaceComponent_SetAccepting(reinterpret_cast<int64_t>(wpc), true);

            TownRegistry_RegisterWorkplace(townMgr, wpc);
            fprintf(stderr, "[Phase4Employ] lodge WPC=%p desc=%p profKey(desc+0x20)=0x%llx "
                    "-> seated WPC+0x48=0x%llx; registered into townMgr+0x100\n",
                    wpc, wpDesc, (unsigned long long)profKey,
                    (unsigned long long)*(uint64_t*)(w + 0x48));

            // Verify the reader finds it: FindAvailableBuilding(townMgr, &out, profKey)
            // returns this lodge entity (its WorkPlaceComponent worker count < WorkComponent
            // capacity + the WPC+0x50 enable flag). Proves the townMgr+0x100 registry + the
            // FindAvailableBuilding reader work end-to-end (the FIND half of producer
            // employment). The remaining ASSIGN half = the ProfessionUI worker-count seat
            // cascade (impl_dd10 0x14007dd10 -> cbf0/ca20/c5f0/c4b0 -> AssignWorkplace).
            if (profKey != 0) {
                int64_t foundIBP = 0;
                FindAvailableBuilding(townMgr, &foundIBP, profKey);
                fprintf(stderr, "[Phase4Employ] FindAvailableBuilding(profKey=0x%llx) -> %p "
                        "(lodge entity=%p) %s\n",
                        (unsigned long long)profKey, (void*)foundIBP, entity,
                        (foundIBP == reinterpret_cast<int64_t>(entity)) ? "MATCH" :
                        (foundIBP != 0 ? "other-building" : "NOT-FOUND"));
            }
            fflush(stderr);
        }
    }

    // Re-point the force-work harness at this REAL production workplace.
    g_pForceWorkBuilding = entity;
    fprintf(stderr, "[startTown::workplace] ForesterLodge spawned tile(%d,%d) fp=%dx%d "
            "-> g_pForceWorkBuilding=%p\n", tileX, tileY, fpW, fpH, entity);
    fflush(stderr);
}

// ============================================================================
// Town-manager registry passes (ungated).
//
// Retail keeps two lists on the TownManager (gw+0xf0) that the town-hall
// statistics dialog (TownHallUI::UpdateGeneral) and the Goal/StatValue counters
// read: the town citizen list (+0xa0 array / +0xa8 count, appended by the real
// registrar CitizenComponent_OnAdd 0x140114000) and the residence list
// (+0x20/+0x28, appended by ResidenceDescription_helper_9de0 — wired by retail
// as the residence build-state callback, i.e. when a house becomes BUILT).
// Standalone start-town spawns citizens/houses without running either
// registrar, so both lists stayed empty and the town-hall General tab rendered
// all zeros (0 citizens/adults/children/families/homes, flat hearts/stars).
//
// OnAdd is a plain append (no dedup), so the citizen pass keeps a once-guard
// shared with StartTown_EmployForesters (which needs the same registration and
// previously did it inline, gated behind BANISHED_LIVE_WORKPLACE).
static bool s_townCitizensRegistered = false;
static void StartTown_RegisterTownCitizens() {
    if (s_townCitizensRegistered) return;
    s_townCitizensRegistered = true;
    int n = 0;
    for (void* c : g_foundingCitizens) {
        if (!c) continue;
        CitizenComponent_OnAdd(c);
        ++n;
    }
    fprintf(stderr, "[startTown::registry] registered %d founding citizens into townMgr+0xa0\n", n);
    fflush(stderr);
}

// Houses: ResidenceComponent::OnCreate's list op on townMgr+0x20 is a REMOVE
// (FUN_140002cc0); the ADD retail runs at build-complete never fires in
// standalone. Register the captured founding-house IBPs (the exact +0x10 value
// helper_9de0 appends) once, after the OnCreate pass has run.
static void StartTown_RegisterTownResidences() {
    static bool s_done = false;
    if (s_done) return;
    s_done = true;
    int n = 0;
    for (int64_t ibp : g_foundingHouseIBPs) {
        if (!ibp) continue;
        TownRegistry_RegisterResidence(ibp);
        ++n;
    }
    fprintf(stderr, "[startTown::registry] registered %d founding houses into townMgr+0x20\n", n);
    fflush(stderr);
}

// ============================================================================
// Phase 4 ASSIGN half (env BANISHED_LIVE_WORKPLACE) — seat laborers at the lodge.
//
// The faithful producer-employment flow is player-driven (ProfessionUI worker count
// -> FindAvailableBuilding -> the cbf0/ca20/c5f0/c4b0 seat cascade). Here we drive the
// same OUTCOME programmatically (as if the player set the forester worker count to the
// lodge capacity): pick idle adult laborers and employ them via the ported
// CitizenComponent::AssignWorkplace (0x1401163d0), which sets citizen+0x238 (workplace)
// + citizen+0x240 (slot) and rosters the worker. We also set citizen+0x1f0 (profession)
// to the forester Profession* exactly as the cbf0 cascade does. Once +0x238 is set, a
// healthy adult's CitizenAI candidate set gains Gather(6/7)/Work(5) and the citizen
// paths to the lodge. Runs after citizens + founding housing exist. Env-gated + null-
// guarded; a no-op when no lodge/citizens/key.
static void StartTown_EmployForesters() {
    if (!WorkplaceEnvOn()) return;
    if (g_foresterProfKey == 0 || !g_pForceWorkBuilding) return;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton || !fn_InternalBasePtr_CheckPtr) return;
    int64_t townMgr = *reinterpret_cast<int64_t*>(
        reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xf0);
    if (!townMgr) return;

    // Lodge worker capacity (WorkComponent+0x30; FindAvailableBuilding already cached it
    // via CalcMaxWorkers). Default to the ForesterLodge template _maxWorkerCount=4.
    void* wc = FindComponentOnEntity(g_pForceWorkBuilding, TypeId("WorkComponent"));
    int capacity = (wc && *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(wc) + 0x30) > 0)
                 ? *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(wc) + 0x30) : 4;

    // --- Brick M-C: drive the REAL ProfessionUI worker-seat cascade instead of the old
    // +0x1f0/+0x238 field-poke. Two faithful prerequisites the inert standalone citizen
    // init skips (FUN_140117080 is guarded off, so +0x1f0 stays 0): the founding colonists
    // must be (1) LABORERS — +0x1f0 = ProfessionList(gw+0xb0)+0x30, the default profession
    // retail colonists carry — and (2) registered into the town citizen list townMgr+0xa0
    // via the real registrar CitizenComponent_OnAdd (0x140114000), the list the cascade
    // strategies scan. Then CitizenEmploy_SeatWorkers runs the exact FindAvailableBuilding
    // + cbf0->ca20->c5f0->c4b0 fallback dd10 uses per worker, seating each via
    // +0x1f0=forester + AssignHousing (+0x248 = the lodge's WorkPlaceComponent roster).
    uint64_t laborerKey = 0;
    {
        int64_t profList = *reinterpret_cast<int64_t*>(
            reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xb0);
        if (profList) laborerKey = *reinterpret_cast<uint64_t*>(
            reinterpret_cast<uint8_t*>(profList) + 0x30);
    }
    // Register into the real townMgr+0xa0 town-citizen list (shared once-guard;
    // normally already done by the ungated StartTown_RegisterTownCitizens pass).
    StartTown_RegisterTownCitizens();
    int seeded = 0;
    for (void* c : g_foundingCitizens) {
        if (!c) continue;
        int64_t cz   = reinterpret_cast<int64_t>(c);
        int64_t desc = *reinterpret_cast<int64_t*>(cz + 0x288);
        bool adult = desc && *reinterpret_cast<float*>(desc + 0x44) <= *reinterpret_cast<float*>(cz + 0x3c);
        // Seed the laborer profession on idle adults so the cascade's +0x1f0==laborerKey
        // filter matches them (retail init does this; ours is guarded off).
        if (laborerKey && adult && *reinterpret_cast<int64_t*>(cz + 0x1f0) == 0)
            { *reinterpret_cast<uint64_t*>(cz + 0x1f0) = laborerKey; ++seeded; }
    }

    // The cascade's seat epilogue calls AssignHousing -> the roster attach into the
    // lodge's WorkPlaceComponent (+0x58/+0x60), whose gameplay-time Scene mesh-visual
    // toggle deadlocks during this build-time pass — same guard the founding-housing
    // pass uses (g_wpcRosterOnly).
    bool prevRosterOnly = g_wpcRosterOnly;
    g_wpcRosterOnly = true;
    int seated = CitizenEmploy_SeatWorkers(townMgr, g_foresterProfKey, capacity);
    g_wpcRosterOnly = prevRosterOnly;

    fprintf(stderr, "[Phase4Employ] M-C cascade: foundingCitizens=%d laborerKey=0x%llx seeded=%d "
            "-> seated %d/%d forester(s) profKey=0x%llx\n",
            (int)g_foundingCitizens.size(), (unsigned long long)laborerKey, seeded,
            seated, capacity, (unsigned long long)g_foresterProfKey);
    fflush(stderr);
}

// ---------------------------------------------------------------------------
// Seed the default "Laborer" profession on founding adults (ungated).
//
// Retail's per-citizen init FUN_140117080 sets CitizenComponent+0x1f0 =
// ProfessionList(gw+0xb0)+0x30 (the default Laborer Profession*) for every adult
// colonist. That initializer is guarded off in standalone, so +0x1f0 stays 0 and the
// citizen info panel's Job line reads a null profession -> renders blank. This pass
// reproduces just the laborer seed for the founding colonists WITHOUT the forester-
// employment gating (StartTown_EmployForesters only runs under BANISHED_LIVE_WORKPLACE).
// Children keep +0x1f0 == 0 (their panel branch uses ProfessionList+0x40 "Student").
// Idempotent (sets only when +0x1f0 == 0), so it composes with EmployForesters' own
// seed. Depends on the founding-housing pass having collected g_foundingCitizens and on
// ProfessionList (gw+0xb0) being live (LoadProfessionList_Step2, default-on). Standalone-
// only build-time pass -> no-op in DLL mode (retail runs the real FUN_140117080).
static void StartTown_SeedLaborerProfessions() {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    int64_t profList = *reinterpret_cast<int64_t*>(
        reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xb0);
    if (!profList) return;
    uint64_t laborerKey = *reinterpret_cast<uint64_t*>(
        reinterpret_cast<uint8_t*>(profList) + 0x30);
    if (!laborerKey) {
        fprintf(stderr, "[startTown::laborer] ProfessionList+0x30 (default profession) null "
                "— Profession.rsc not live; Job line stays blank\n"); fflush(stderr);
        return;
    }
    int seeded = 0;
    for (void* c : g_foundingCitizens) {
        if (!c) continue;
        int64_t cz   = reinterpret_cast<int64_t>(c);
        int64_t desc = *reinterpret_cast<int64_t*>(cz + 0x288);
        bool adult = desc && *reinterpret_cast<float*>(desc + 0x44) <= *reinterpret_cast<float*>(cz + 0x3c);
        if (adult && *reinterpret_cast<int64_t*>(cz + 0x1f0) == 0) {
            *reinterpret_cast<uint64_t*>(cz + 0x1f0) = laborerKey; ++seeded;
        }
    }
    fprintf(stderr, "[startTown::laborer] seeded Laborer on %d/%zu founding adults "
            "(laborerKey=0x%llx)\n", seeded, g_foundingCitizens.size(),
            (unsigned long long)laborerKey);
    fflush(stderr);
}

// ============================================================================
// CITIZENS pool — CitizenComponent_impl_0dd0 (0x1400a0dd0) + sub_1040 (0x1400a1040).
// Spawns the initial colonists. For each family entry (startConfig+0x60, count
// +0x68, stride 8: {bool _educated@+0, int _maxChildCount@+4}) the original:
//   * spawns 2 adults (one male, one female) and links them as spouses
//     (CitizenComponent+0x250 cross-reference)
//   * spawns Rand(0.._maxChildCount) children attached to the mother
//     (CitizenComponent_helper_68a0)
//
// Every colonist is instantiated from the shared citizen template — an
// ExternalRef at startConfig+0x48 ({String name@+0x48, int gen@+0x50,
// External* template@+0x58}) — via CopyCtor, then the original runs
// FUN_140117080 to initialise the CitizenComponent: random name, age, gender,
// model variant + world transform, education, housing assignment, movement
// speed, AnimalComponent animation state.
//
// That init dereferences the CitizenComponent's real sub-objects (owner entity
// IBP +0x10, AnimalComponent IBP +0x278, CitizenDescription +0x288, housing
// +0x1f0/+0x248) — all ZEROED on our inert components (Foundation D/E) — and
// calls a chain of exe helpers (Entity_ClearPathQueue, FUN_1401253d0,
// FUN_140003870, FUN_1400a4c60, CitizenComponent_Setup{Young,Elder},
// CitizenComponent_AssignHousing, AnimalComponent_impl_a1f0) that are NULL in
// standalone. So the init block — and the spouse/child linking, which walks the
// same CitizenComponent fields — is GUARDED OFF, exactly like the structures'
// render transform. The citizen entities still exist in the world with their
// full (inert) component sets; animating them needs a real CitizenComponent
// (the "Foundation F"-class gate, shared with visible buildings).
//
// Type ids confirmed via RegisterType DATA xrefs: CitizenComponent = 1401c7158,
// CitizenDescription = 1401c7190 (the "CitizenAI" naming on the exe helpers is a
// misnomer; the component is CitizenComponent).
// ============================================================================

static constexpr int CITIZEN_REF    = 0x48;    // startConfig+0x48 = citizen ExternalRef
static constexpr int FAMILY_ARR     = 0x60;    // startConfig+0x60 = family array
static constexpr int FAMILY_CNT     = 0x68;    // startConfig+0x68 = family count
static constexpr int FAMILY_STRIDE  = 8;       // {bool _educated@+0, int _maxChildCount@+4}

// Per-citizen model-variant pick (modelcomponent.cpp). Faithful core of the
// FUN_1401253d0 call inside the deferred citizen init (FUN_140117080): chooses a
// random variant group within the citizen description's range and re-SetMesh'es.
// Returns the chosen index or -1 (no change). Must run after Serialize, before
// RegisterEntity.
extern "C" int ModelComponent_RandomizeVariant(void* modelComp, int forceIdx);

// Faithful port of the citizen name generator (citizen_names.cpp). Fills a C-string
// (>=0x100 byte buffer) with a 4-gram Markov name from Game/Names/{Male,Female}.ndb.
extern "C" bool GenerateCitizenName(void* rng, bool female, char* out, int cap);

// Gate: BANISHED_CITIZEN_VARIANT. Default ON (user-verified: gives per-citizen
// male/female models instead of every colonist sharing the description's fixed
// variant); opt out with BANISHED_CITIZEN_VARIANT=0. NB this only varies the mesh
// variant GROUP (gender). Clothing colour (+0x34) and age/height (+0x40/+0x3c),
// which the retail deferred init FUN_140117080 also randomizes, are still
// uniform — that init is guarded off in standalone (housing/animation deps).
static bool CitizenVariantOn() {
    static int en = -1;
    if (en < 0) {
        const char* e = std::getenv("BANISHED_CITIZEN_VARIANT");
        en = (e && (e[0] == '0' || e[0] == '\0')) ? 0 : 1;
    }
    return en == 1;
}

// Give a freshly-instantiated citizen entity its gender mesh variant GROUP
// (+0x164) via ModelComponent_RandomizeVariant. The original derives this from the
// citizen's gender (retail variantIdx=(gender==0)?1:0); we pass an explicit index
// so a founding family's two adults get opposite-gender models (a couple) and kids
// alternate — matching CitizenComponent_impl_0dd0 — instead of a random pick. Runs
// before the entity is registered so its OnAdded adds the chosen mesh. Returns the
// resolved render ModelComponent (for the colour/scale pass) or 0. No-op unless
// BANISHED_CITIZEN_VARIANT is set (default on).
static uintptr_t SetCitizenModelVariant(void* entity, uint32_t tidModel, int variantIdx) {
    if (!CitizenVariantOn() || !entity || tidModel == 0 ||
        !fn_InternalBasePtr_Ctor || !fn_InternalBasePtr_CheckPtr)
        return 0;
    void* mc = FindComponentOnEntity(entity, tidModel);
    if (!mc) return 0;
    uintptr_t mcIBP = 0;
    fn_InternalBasePtr_Ctor(&mcIBP, mc);
    fn_InternalBasePtr_CheckPtr(&mcIBP);
    if (!mcIBP) return 0;
    int chosen = ModelComponent_RandomizeVariant((void*)mcIBP, variantIdx);
#if defined(STANDALONE_EXE) && defined(_DEBUG)
    static int s_varLog = 0;
    if (s_varLog < 12) {
        ++s_varLog;
        fprintf(stderr, "[startTown::citizen] variant model=%p variantIdx=%d\n",
                (void*)mcIBP, chosen);
        fflush(stderr);
    }
#else
    (void)chosen;
#endif
    return mcIBP;
}

// Per-citizen clothing colour + age-size, applied through the SAME path retail's
// CitizenComp_SetupAgeMesh uses: the render ModelComponent reached via
// CitizenComponent+0x278 (character sub-component InternalBasePtr) deref +0x68.
//   * Colour — retail (Rand()+Rand())&7 into citizen+0x34; a single Rand(rng,8) is
//     distributionally identical (uniform 0-7). Written as float to mesh+0x140
//     (= MeshInstance+0x118); GetInstanceTransform (Runtime 0x180101280) packs it
//     into instance-matrix element [15] where the citizen shader reads it.
//   * Age size — uniform scale to mesh+0x134/138/13c (= MeshInstance+0x10c/110/114):
//     grown 1.0 (0x3f800000), child 0.66 (0x3f28f5c3). HARDCODED literals — the
//     Setup*_Reimpl helpers read the .rdata scale constants via ResolveAddr, which
//     is NULL in standalone (would collapse everyone to the 1.0 fallback).
// Runs AFTER RegisterEntity: the citizen+0x278/+0x68 character-model link and the
// scene mesh instance are only fully wired once OnAdded has run, and writing the
// instance fields post-OnAdded avoids OnAdded re-initialising them under us.
// `genderMc` is the ModelComponent SetCitizenModelVariant re-pointed, logged only
// to confirm it is the same instance this path resolves.
static void ApplyCitizenColorScale(void* entity, bool isChild, void* rng,
                                   uintptr_t genderMc, int variantIdx) {
    if (!CitizenVariantOn() || !entity || !fn_InternalBasePtr_Ctor ||
        !fn_InternalBasePtr_CheckPtr)
        return;
    void* cc = FindComponentOnEntity(entity, TypeId("CitizenComponent"));
    if (!cc) return;
    uintptr_t ccIBP = 0;
    fn_InternalBasePtr_Ctor(&ccIBP, cc);
    fn_InternalBasePtr_CheckPtr(&ccIBP);
    uint8_t* citizen = (uint8_t*)ccIBP;
    if (!citizen) return;

    if (!rng)
        rng = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                  ? *(void**)((uint8_t*)*g_pGameWorldSingleton + 0xd8) : nullptr;

    // Colour index → citizen+0x34 (source of truth) AND the mesh instance slot.
    int colorIdx = (rng && fn_Random_Rand) ? (int)fn_Random_Rand(rng, 8) : 0;
    *(int32_t*)(citizen + 0x34) = colorIdx;

    // Age — the REAL driver of the render scale. The per-frame OnUpdate2
    // (0x1401140f0) re-derives the model scale from age every frame:
    //   age(+0x3c) <  thresh(desc+0x44) && !dead(+0x208==0) -> SetupElder 0.66 (child)
    //   age(+0x3c) >= thresh                                 -> SetupYoung 1.0  (adult)
    // Our deferred citizen init (FUN_140117080) is off, so age is 0 for everyone and
    // OnUpdate2 shrinks ALL colonists to the child scale. Reproduce the retail
    // founding-family age draw (CitizenComponent_impl_0dd0) so adults span a real
    // range and children fall below the threshold:
    //   adult age = RandFloat*(desc+0x54*0.5 - desc+0x50) + desc+0x50   (DAT_140172114=0.5)
    //   child age = RandFloat*(desc+0x44)  = uniform [0, threshold)
    int64_t descP = *(int64_t*)(citizen + 0x288);
    float thresh  = descP ? *(float*)(descP + 0x44) : 2.0f;
    float age;
    if (!descP || !(rng && fn_Random_RandFloat)) {
        age = isChild ? 0.0f : (thresh + 1.0f);          // fallback
    } else if (isChild) {
        age = fn_Random_RandFloat(rng) * thresh;         // [0, threshold)
    } else {
        float amin = *(float*)(descP + 0x50);
        float amax = *(float*)(descP + 0x54);
        age = fn_Random_RandFloat(rng) * (amax * 0.5f - amin) + amin;
        if (age < thresh) age = thresh;                  // keep adults adult-sized
    }
    *(float*)(citizen + 0x3c) = age;
    // Guard against an age>=lifespan death check: keep maxAge(+0x40) above age.
    if (*(float*)(citizen + 0x40) <= age)
        *(float*)(citizen + 0x40) = age + 100.0f;

    // Resolve the character sub-component (+0x278) then its ModelComponent (+0x68),
    // exactly like CitizenComp_SetupAgeMesh. Direct colour + matching initial scale
    // avoid a one-frame pop before OnUpdate2 first runs; OnUpdate2 then keeps the
    // scale in sync with the age set above.
    fn_InternalBasePtr_CheckPtr(citizen + 0x278);
    uintptr_t charIBP = *(uintptr_t*)(citizen + 0x278);
    uintptr_t mesh = charIBP ? *(uintptr_t*)(charIBP + 0x68) : 0;
    if (mesh) fn_InternalBasePtr_CheckPtr(&mesh);

    float scale = isChild ? 0.66f : 1.0f;
    if (mesh) {
        uint8_t* m = (uint8_t*)mesh;
        *(float*)(m + 0x134) = scale;
        *(float*)(m + 0x138) = scale;
        *(float*)(m + 0x13c) = scale;
        *(float*)(m + 0x140) = (float)colorIdx;
    }

    // Gender field (citizen+0x38) + name String (citizen+0x28). The retail deferred
    // init (FUN_140117080) sets +0x38 = gender (0 = female, nonzero = male) and
    // assigns a generated name to +0x28; both are read by the CitizenUI dialog
    // (gender icon, textName). Our variant scheme maps variantIdx 0 -> male model
    // (retail variantIdx = (gender==0)), so gender = (variantIdx==0) ? 1 : 0 and the
    // name uses Female.ndb when variantIdx != 0. Done LAST so the colour/age RNG
    // stream above (user-verified) is unchanged.
    bool female = (variantIdx != 0);
    *(int32_t*)(citizen + 0x38) = female ? 0 : 1;
    // The name at +0x28 is an uninitialized System::String on a standalone citizen
    // (garbage buffer ptr). Construct a fresh String in place from the generated
    // C-string — a constructor doesn't release the (garbage) old value, which is the
    // correct behaviour for uninitialized storage.
    if (fn_String_Ctor_CStr) {
        char nameBuf[0x100];
        if (GenerateCitizenName(rng, female, nameBuf, (int)sizeof(nameBuf))) {
            fn_String_Ctor_CStr((void*)(citizen + 0x28), nameBuf);
#if defined(STANDALONE_EXE) && defined(_DEBUG)
            static int s_nameLog = 0;
            if (s_nameLog < 12) {
                ++s_nameLog;
                fprintf(stderr, "[startTown::citizen] name='%s' female=%d\n",
                        nameBuf, (int)female);
                fflush(stderr);
            }
#endif
        }
    }

#if defined(STANDALONE_EXE) && defined(_DEBUG)
    static int s_appLog = 0;
    if (s_appLog < 12) {
        ++s_appLog;
        fprintf(stderr, "[startTown::citizen] colorscale citizen=%p mesh=%p same=%d "
                "colour=%d scale=%.2f age=%.2f thresh=%.2f%s\n",
                (void*)citizen, (void*)mesh, (int)(mesh && mesh == genderMc),
                colorIdx, scale, age, thresh, isChild ? " (child)" : "");
        fflush(stderr);
    }
#endif
}

// ---- Citizen Foundation-F — spawn-cell world transform (FUN_140003870) ------
// The deferred citizen init (FUN_140117080) calls FUN_140003870 to set each
// colonist's ModelComponent world transform; with the init guarded off the
// transform stays zeroed, so a colonist whose mesh is added to the scene (and
// which never subsequently moves) renders at the world ORIGIN. This reproduces
// just that one position-setter so founding colonists stand on their spawn cell.
//
// FUN_140003870(ModelComponent* model, Vector3* offset, float yaw):
//   Transform::FromYawPitchOffset(out, &yaw, &pitch=0, offset)   // RDX=yaw,
//        R8=pitch, R9=offset — Ghidra drops the R9 dispatch-slot arg, so the
//        decompile shows a zero local instead of `offset`; verified in disasm.
//   write the 7-float Transform to model+0x148 (pos = +0x158/15c/160),
//   model+0x168 = yaw, then Geometry::SetTransform(model+0x28, model+0x148).
// FUN_140117080 always passes yaw=0, so colonists spawn facing +X. We run this
// BEFORE RegisterEntity (like the structure Foundation-F at SpawnStructures), so
// ModelComponent::Draw fired by OnAdded performs the Geometry::SetTransform —
// no separate SetTransform call needed.
//
// The spawn cell is the founding-family jitter from sub_1040 (0x1400a1040):
//   pos = { base.x + RandFloatTol*r, base.y + RandFloatTol*r, base.z }
// where base = the town-centre world position and r = the spawn jitter radius
// at startConfig+0x40 (the float just before the citizen ExternalRef at +0x48).
static bool CitizenSpawnPosOn() {
    // Faithful core behaviour (the structure Foundation-F is likewise
    // unconditional); default ON, opt out with BANISHED_CITIZEN_SPAWNPOS=0.
    static int en = -1;
    if (en < 0) {
        const char* e = std::getenv("BANISHED_CITIZEN_SPAWNPOS");
        en = (e && (e[0] == '0' || e[0] == '\0')) ? 0 : 1;
    }
    return en == 1;
}

static void SetCitizenSpawnTransform(void* entity, uint32_t tidModel,
                                     const float base[3], float jitter, void* rng) {
    if (!CitizenSpawnPosOn() || !entity || tidModel == 0 ||
        !fn_Transform_FromYawPitchOffset ||
        !fn_InternalBasePtr_Ctor || !fn_InternalBasePtr_CheckPtr)
        return;
    // Guard a garbage jitter radius (the field is read raw from the start config).
    if (!(jitter == jitter) || jitter < 0.0f || jitter > 1.0e4f) jitter = 0.0f;

    void* mc = FindComponentOnEntity(entity, tidModel);
    if (!mc) return;
    uintptr_t mcIBP = 0;
    fn_InternalBasePtr_Ctor(&mcIBP, mc);
    fn_InternalBasePtr_CheckPtr(&mcIBP);
    uint8_t* m = (uint8_t*)mcIBP;
    if (!m) return;

    // sub_1040 jitter: RandFloatTol() == RandFloat()*2-1 (same approximation the
    // resource scatter uses). Draw order matches vanilla: first draw -> y, second -> x.
    auto rtol = [&]() -> float {
        return (rng && fn_Random_RandFloat) ? fn_Random_RandFloat(rng) * 2.0f - 1.0f : 0.0f;
    };
    float jy = rtol() * jitter;
    float jx = rtol() * jitter;
    float pos[3] = { base[0] + jx, base[1] + jy, base[2] };

    float yaw = 0.0f, pitch = 0.0f;
    fn_Transform_FromYawPitchOffset((void*)(m + 0x148), &yaw, &pitch, pos);
    *(float*)(m + 0x168) = yaw;

#if defined(STANDALONE_EXE) && defined(_DEBUG)
    static int s_posLog = 0;
    if (s_posLog < 8) {
        ++s_posLog;
        const float* wt = (const float*)(m + 0x158);   // written transform pos
        fprintf(stderr, "[startTown::citizen] spawnpos model=%p base=(%.1f,%.1f,%.1f) "
                "jitter=%.2f -> pos=(%.1f,%.1f,%.1f) transform.pos=(%.1f,%.1f,%.1f)\n",
                (void*)m, base[0], base[1], base[2], jitter, pos[0], pos[1], pos[2],
                wt[0], wt[1], wt[2]);
        fflush(stderr);
    }
#endif
}

// Instantiate one colonist entity from the resolved citizen template and register
// it with the world. Returns the raw entity pointer (for optional later linking)
// or nullptr. CitizenComponent initialisation (FUN_140117080) is deferred — see
// the section header; only the spawn-cell transform (Foundation-F) is reproduced.
static void* SpawnOneCitizen(void* citizenTemplate, void* entityList,
                             const float base[3], float jitter, void* rng,
                             bool isChild, int variantIdx) {
    if (!citizenTemplate) return nullptr;
    void* mem = fn_MemAlloc(g_pMemAllocSingleton, ENTITY_SIZE, 8);
    void* ent = mem ? fn_EntityFactory_CreateEntity(mem, citizenTemplate) : nullptr;
    uintptr_t entIBP = 0;
    fn_InternalBasePtr_Ctor(&entIBP, ent);

    // Per-citizen gender mesh variant + spawn-cell transform — after instantiation
    // (Serialize ran, +0x166 set) and BEFORE registration (so OnAdded adds the
    // chosen mesh to the scene at the spawn-cell transform).
    uintptr_t genderMc = 0;
    if (ent) {
        fn_InternalBasePtr_CheckPtr(&entIBP);
        uint32_t tidModel = TypeId("ModelComponent");
        genderMc = SetCitizenModelVariant((void*)entIBP, tidModel, variantIdx);
        SetCitizenSpawnTransform((void*)entIBP, tidModel, base, jitter, rng);
    }

    int category = *(int32_t*)((uint8_t*)citizenTemplate + TEMPLATE_CATEGORY);
    fn_EntityList_RegisterEntity(entityList, &entIBP, category);

    // Colour + randomized age (→ age-size) — applied AFTER registration, once
    // OnAdded has wired the character-model link (citizen+0x278/+0x68) and the
    // scene mesh instance.
    if (ent) {
        fn_InternalBasePtr_CheckPtr(&entIBP);
        ApplyCitizenColorScale((void*)entIBP, isChild, rng, genderMc, variantIdx);
    }
    return ent;
}

// ---- Founding-family housing (BANISHED_LIVE_HOUSING) ------------------------
// Vanilla has NO dedicated founding-family→house placement: the founding spawner
// (CitizenComponent_impl_0dd0) only spawns + spouse/child-links citizens, and houses
// are assigned per-tick by CitizenComponent::Update via the townMgr+0x100
// profession/category BST (a house registers there through its WorkPlaceComponent's
// residence-category key). That +0x100 registry — BST node-seed + WorkPlaceComponent::
// OnCreate + the WorkPlaceDescription Serialize that fills the key — is the larger
// economy frontier and is empty in the standalone Easy town, so the per-tick finders
// correctly return 0.
//
// As a self-contained founding placement we pair each founding family with one spawned
// WoodHouse (captured at ResidenceComponent::OnCreate, see g_foundingHouseIBPs — townMgr+0x20
// itself is never populated in standalone) and make each member a RESIDENT of that house.
//
// CORRECTED field semantics (the two exe assigner names are INVERTED vs effect — verified
// from the faithful citizen-panel button port + the retail housing solver FUN_1400d62a0):
//   * CitizenComponent+0x238 = RESIDENCE / HOME. Set by CitizenComponent_AssignWorkplace
//     (0x1401163d0, MISNAMED). It rosters the citizen into the house's ResidenceComponent
//     (c7cf0, found via Entity_FindComponent_Storage 0x140008090 which matches c7cf0 exactly)
//     slot[0] resident array (+0x50). The citizen-panel Home button (id 0x226) reads +0x238.
//   * CitizenComponent+0x248 = WORKPLACE / JOB. Set by CitizenComponent_AssignHousing
//     (0x140111ee0, MISNAMED) into the WorkPlaceComponent (c80e0) roster. The Profession
//     button (id 0x227) reads +0x248.
//
// The founding default is an UNEMPLOYED laborer (retail spawns no starting job building), so
// we assign ONLY the residence: AssignWorkplace(citizen, houseEntityIBP, 0) — exactly as the
// retail housing solver FUN_1400d62a0 does (slotIndex 0, IBP = ResidenceComponent+0x10). This
// sets +0x238 = home, rosters into c7cf0 slot[0], and leaves +0x248 = 0 so the Profession
// button correctly greys out. The occupied-house mesh visual (residence a710 model-notify
// 2b70 = an empty no-op even in retail; AmbientEmitter smoke = inert in standalone) is a
// separate rendering follow-up. Requires BANISHED_LIVE_RESIDENCE (else no houses are captured).
// Gated on BANISHED_LIVE_HOUSING; default build is a no-op.
static bool HousingEnvOn() {
    static int en = -1;
    if (en < 0) {
        const char* e = std::getenv("BANISHED_LIVE_HOUSING");
        en = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return en == 1;
}

// Ensure the house's ResidenceComponent (c7cf0) resident-roster array (+0x50/+0x58) is
// allocated. Retail sizes it in ResidenceComponent::Serialize/Slot14 (0x14012a120):
//   capacity = ResidenceDescription(comp+0x18)->+0x20 (max occupants);
//   ResidenceComponent_Controller_sub_7ad0(comp+0x50, capacity)  [= ResizeResidentArray
//   0x140037ad0] allocates `capacity` zeroed {data,count,cap}[] slots and sets +0x58=capacity.
// In standalone that slot bails: it is gated on IsDerivedFrom(desc, c7d28 ResidenceDescription),
// which returns FALSE (the SA description's type-id isn't recognised), so the array is never
// allocated (+0x50 null / +0x58 0) and a710's Roster(+0x50)[0] would deref null. Reproduce the
// sizing here, TRUSTING the linked description at +0x18 (the established SA derive-or-trust
// pattern). Idempotent; returns true when +0x50 is allocated with >=1 slot.
static bool StartTown_EnsureResidenceRoster(void* resComp) {
    if (!resComp) return false;
    uint8_t* rc = reinterpret_cast<uint8_t*>(resComp);
    if (*reinterpret_cast<int64_t*>(rc + 0x50) != 0 && *reinterpret_cast<int32_t*>(rc + 0x58) >= 1)
        return true;                                   // already sized

    void* desc = *reinterpret_cast<void**>(rc + 0x18); // ResidenceDescription (trusted, no IsDerivedFrom)
    if (!desc) {
        fprintf(stderr, "[startTown::housing] residence roster: comp=%p has no description (+0x18) — "
                "cannot size\n", resComp); fflush(stderr);
        return false;
    }
    int capacity = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(desc) + 0x20);  // desc+0x20 = max occupants
    if (capacity < 1) capacity = 1;
    if (capacity > 16) capacity = 16;                  // sanity clamp (guards a garbage read)

    typedef void (*fn_Resize_t)(void* arrayFieldPtr, int newCount);   // 0x140037ad0 (registered)
    fn_Resize_t resize = reinterpret_cast<fn_Resize_t>(ResolveAddr(Module::Exe, 0x140037ad0));
    if (!resize) {
        fprintf(stderr, "[startTown::housing] residence roster: ResizeResidentArray(140037ad0) "
                "unresolved\n"); fflush(stderr);
        return false;
    }
    resize(reinterpret_cast<void*>(rc + 0x50), capacity);

    int64_t sa = *reinterpret_cast<int64_t*>(rc + 0x50);
    int sc = *reinterpret_cast<int32_t*>(rc + 0x58);
    fprintf(stderr, "[startTown::housing] residence roster sized: comp=%p desc=%p capacity=%d -> "
            "+0x50=%llx +0x58=%d\n", resComp, desc, capacity, (unsigned long long)sa, sc);
    fflush(stderr);
    return sa != 0 && sc >= 1;
}

static void StartTown_AssignFoundingHousing(const std::vector<std::vector<void*>>& families) {
    if (!HousingEnvOn()) return;
    uint32_t tidCitizen = TypeId("CitizenComponent");                  // 0x1401c7158
    fprintf(stderr, "[startTown::housing] families=%zu capturedHouses=%zu tidCitizen=%u\n",
            families.size(), g_foundingHouseIBPs.size(), tidCitizen);
    fflush(stderr);
    uint32_t tidResidence = TypeId("ResidenceComponent");             // 0x1401c7cf0
    if (g_foundingHouseIBPs.empty() || tidCitizen == 0 || tidResidence == 0) {
        fprintf(stderr, "[startTown::housing] no captured residences "
                "(need BANISHED_LIVE_RESIDENCE; tidResidence=%u) — skip\n",
                tidResidence); fflush(stderr); return;
    }

    // Defensive: keep any incidental WorkPlaceComponent attach in roster-only mode during the
    // build (the residence path AssignWorkplace->a710 does no Scene op, but a family member's
    // stale +0x248 detach could route through the c80e0 path — see g_wpcRosterOnly).
    bool prevRosterOnly = g_wpcRosterOnly;
    g_wpcRosterOnly = true;
    for (size_t f = 0; f < families.size() && f < g_foundingHouseIBPs.size(); ++f) {
        int64_t houseIBP = g_foundingHouseIBPs[f];
        if (houseIBP == 0) continue;

        // Locate the house's ResidenceComponent (c7cf0) — the RESIDENT roster the faithful
        // residence assigner (AssignWorkplace 0x1401163d0 -> a710) appends into. Its slot
        // array (+0x50) is sized to the residence capacity by ResidenceComponent::Serialize
        // during spawn (BANISHED_LIVE_RESIDENCE). Guard against an un-serialized roster
        // (null slot-array base / zero slots) so a710's Roster(+0x50)[0] can't deref null.
        void* resComp = FindComponentOnEntity(reinterpret_cast<void*>(houseIBP), tidResidence);
        if (!resComp) {
            fprintf(stderr, "[startTown::housing] family[%zu] house#%zu has no ResidenceComponent "
                    "(c7cf0) — skip\n", f, f); fflush(stderr);
            continue;
        }
        int64_t slotArr = *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(resComp) + 0x50);
        int slotCnt = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(resComp) + 0x58);
        if (slotArr == 0 || slotCnt < 1) {
            // Roster array not sized (SA ResidenceComponent::Serialize bailed on the
            // IsDerivedFrom gate) — size it faithfully now, else a710 would deref null.
            if (!StartTown_EnsureResidenceRoster(resComp)) {
                fprintf(stderr, "[startTown::housing] family[%zu] house#%zu residence roster "
                        "bring-up failed — skip\n", f, f); fflush(stderr);
                continue;
            }
            slotArr = *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(resComp) + 0x50);
            slotCnt = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(resComp) + 0x58);
        }

        int housed = 0, misfiled = 0;
        for (void* ent : families[f]) {
            void* citComp = FindComponentOnEntity(ent, tidCitizen);
            if (!citComp) continue;
            g_foundingCitizens.push_back(citComp);   // Phase 4: source for the employ pass
            // Faithful residence assignment — the retail housing solver FUN_1400d62a0 calls
            // AssignWorkplace(citizen, houseOwnerEntityIBP, 0). It (a) sets CitizenComponent
            // +0x238 = home and (b) rosters the citizen into the house's ResidenceComponent
            // (c7cf0) slot[0] resident array (+0x50). It leaves +0x248 (workplace) UNTOUCHED
            // = 0, so the citizen is the retail founding default (unemployed laborer) and the
            // citizen-panel Profession button (id 0x227, reads +0x248) correctly greys out.
            int64_t houseIBPLocal = houseIBP;
            CitizenComponent_AssignWorkplace(citComp, &houseIBPLocal, 0);
            int64_t home = *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(citComp) + 0x238);
            int64_t work = *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(citComp) + 0x248);
            if (home != 0) ++housed;
            if (work != 0) ++misfiled;   // expected 0 for an unemployed founding laborer
        }

        // Read back the residence roster: slot[0].count = residents actually attached
        // (retail reads *(*(c7cf0+0x50)+8) in FUN_1400d62a0's re-house loop).
        int residents = -1;
        {
            int64_t sa = *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(resComp) + 0x50);
            if (sa) residents = *reinterpret_cast<int32_t*>(sa + 8);
        }
        fprintf(stderr, "[startTown::housing] family[%zu] -> house#%zu (IBP=%llx): housed %d/%zu "
                "members (citizen+0x238); ResidenceComponent(c7cf0) slot[0] residents=%d slots=%d; "
                "+0x248!=0 (should be 0) on %d members\n",
                f, f, (unsigned long long)houseIBP, housed, families[f].size(),
                residents, slotCnt, misfiled);
        fflush(stderr);
    }
    g_wpcRosterOnly = prevRosterOnly;   // restore
}

// Drive the deferred occupied-house visuals once the scene is live. Called every
// GameUpdateLoop tick; runs the visual pass exactly once, on the first tick at/after
// frame `kDeferFrame` (a couple of frames in so the scene has drawn and is in a
// removable state — the visual touches Graphics::Scene::Add/RemoveObject). After it
// runs, the pending list is cleared so it never repeats. No-op when nothing is queued.
extern "C" void StartTown_ApplyPendingOccupiedVisuals(int frameNum) {
    static bool s_applied = false;
    if (s_applied || g_pendingOccupiedVisuals.empty()) return;
    const int kDeferFrame = 3;
    if (frameNum < kDeferFrame) return;
    fprintf(stderr, "[startTown::housing] applying %zu deferred occupied visuals at frame %d\n",
            g_pendingOccupiedVisuals.size(), frameNum);
    fflush(stderr);
    for (int64_t wp : g_pendingOccupiedVisuals) {
        if (wp) WorkplaceComponent_ApplyOccupiedVisual(wp);
    }
    g_pendingOccupiedVisuals.clear();
    s_applied = true;
    fprintf(stderr, "[startTown::housing] deferred occupied visuals applied (crash-free)\n");
    fflush(stderr);
}

// ---- Founding-house household inventory + home-food reserve (BANISHED_LIVE_HOUSING) --------
// Two edible-food seeds per founding WoodHouse, using AddItem FUN_14012d000 (append/increment
// a storage slot at comp+0x28; no Scene op, build-time safe). MUST run AFTER
// StartTown_SeedStartingResources so the town raw-material TEMPLATE pool (townMgr+0x110) — the
// same edible templates the barn seeder uses — is populated. Gated on BANISHED_LIVE_HOUSING.
//
// (A) DISPLAY container — the plain StorageComponent (c7e78, cap/flag-inited to 1000/mask 0x60
//     by StartTown_WireStorage's 7ff0 pass): a small token stock so a clicked house shows a
//     non-empty inventory in whichever panel reads c7e78.
//
// (B) HOME-FOOD reserve — the ResidenceComponent (c7cf0) OWN storage slots at +0x28. This is
//     the container CitizenComp_HasSufficientResources (0x1401156e0) actually reads: it finds
//     c7cf0 via 0x140008090, then Storage_GetResourceCount (0x14012a9d0) sums the EDIBLE units
//     in c7cf0+0x28 and compares `iVar7(occupiedResidentSlots=1) * iVar2(residentsInSlot) *
//     (int)(CitizenDesc+0xc4)` against it. Once we roster residents (+0x50 slot[0]), that
//     product is nonzero, so an EMPTY c7cf0 store would return "insufficient" and spam
//     GatherResources(id6). We seed c7cf0 with `residents * (int)(CitizenDesc+0xc4)` edible
//     units + a margin so the check returns "sufficient" → NO Gather spam. Crash-safe:
//     GetResourceCount matches the food's RawMaterialDescription via DWORD_1401c7cb8 (filled
//     in SA by reg_action_child_ctors2), so no null-deref on the +0x5c edible-flag read.
static void StartTown_SeedHouseInventory(int unitsPerHouse) {
    if (!HousingEnvOn() || g_foundingHouseIBPs.empty()) return;
    void* townMgr = nullptr;
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton)
        townMgr = *(void**)((uint8_t*)*g_pGameWorldSingleton + 0xf0);
    if (!townMgr) return;

    typedef void (*fn_AddItem_t)(int64_t storage, int64_t material, int n);   // 0x14012d000
    fn_AddItem_t addItem = reinterpret_cast<fn_AddItem_t>(ResolveAddr(Module::Exe, 0x14012d000));
    if (!addItem) { fprintf(stderr, "[startTown::housing] AddItem(14012d000) unresolved — no house food\n"); fflush(stderr); return; }

    // Pick an edible raw-material template (RawMaterialDescription+0x5c & 0x20 = edible)
    // from the town template pool (townMgr+0x110 base / +0x118 count).
    uint32_t tidRawMat = TypeId("RawMaterialDescription");
    int tmplCount = *(int32_t*)((uint8_t*)townMgr + 0x118);
    int64_t tmplArr = *(int64_t*)((uint8_t*)townMgr + 0x110);
    int64_t foodTmpl = 0;
    uint32_t foodFlags = 0;
    for (int t = 0; t < tmplCount && foodTmpl == 0; ++t) {
        int64_t tmpl = tmplArr ? *(int64_t*)(tmplArr + (int64_t)t * 8) : 0;
        void* desc = tmpl ? FindDescriptionOnTemplate(reinterpret_cast<void*>(tmpl), tidRawMat) : nullptr;
        if (!desc) continue;
        uint32_t flags = *(uint32_t*)((uint8_t*)desc + 0x5c);
        if (flags & 0x20) { foodTmpl = tmpl; foodFlags = flags; }   // edible
    }
    if (foodTmpl == 0) {
        fprintf(stderr, "[startTown::housing] no edible raw-material template (templates=%d) — no house food\n", tmplCount);
        fflush(stderr); return;
    }

    uint32_t tidStorage = TypeId("StorageComponent");                 // 0x1401c7e78
    uint32_t tidResidence = TypeId("ResidenceComponent");             // 0x1401c7cf0
    // Storage_GetResourceCount (0x14012a9d0) — for the home-food read-back verification.
    typedef int (*fn_GetResCount_t)(int64_t comp);
    fn_GetResCount_t getResCount = reinterpret_cast<fn_GetResCount_t>(ResolveAddr(Module::Exe, 0x14012a9d0));
    int seeded = 0;
    for (int64_t houseIBP : g_foundingHouseIBPs) {
        if (!houseIBP) continue;
        void* sc = FindComponentOnEntity(reinterpret_cast<void*>(houseIBP), tidStorage);
        if (sc) {
            // (A) DISPLAY container c7e78. Faithful accept gate (StorageComponent_helper_c560):
            // the storage flag mask (+0x3c) must intersect the material's flags (+0x5c).
            uint32_t mask = *(uint32_t*)((uint8_t*)sc + 0x3c);
            if ((mask & foodFlags) != 0) {
                addItem(reinterpret_cast<int64_t>(sc), foodTmpl, unitsPerHouse);
                int rows = *(int32_t*)((uint8_t*)sc + 0x30);
                int64_t rowArr = *(int64_t*)((uint8_t*)sc + 0x28);
                int slot0cnt = (rows > 0 && rowArr) ? *(int32_t*)(rowArr + 8) : 0;
                ++seeded;
                fprintf(stderr, "[startTown::housing] house food(c7e78 display): storage=%p +%d "
                        "units -> rows=%d slot0count=%d cap(+0x38)=%d mask=0x%x\n",
                        sc, unitsPerHouse, rows, slot0cnt, *(int32_t*)((uint8_t*)sc + 0x38), mask);
                fflush(stderr);
            }
        }

        // (B) HOME-FOOD reserve into the ResidenceComponent (c7cf0) OWN storage slots (+0x28) —
        // the container HasSufficientResources reads. Seed `residents * (int)(CitizenDesc+0xc4)`
        // + margin edible units so the hunger check returns "sufficient" (no Gather spam).
        void* rescomp = FindComponentOnEntity(reinterpret_cast<void*>(houseIBP), tidResidence);
        if (!rescomp) continue;
        int64_t slotArr = *(int64_t*)((uint8_t*)rescomp + 0x50);
        int residents = slotArr ? *(int32_t*)(slotArr + 8) : 0;   // slot[0].count
        int reqInt = 0;                                            // (int)(CitizenDescription+0xc4)
        if (slotArr) {
            int64_t slot0data = *(int64_t*)slotArr;               // slot[0].data (resident ptr array)
            int64_t resident = slot0data ? *(int64_t*)slot0data : 0;   // data[0] = first resident comp
            int64_t cd = resident ? *(int64_t*)(resident + 0x288) : 0; // CitizenDescription
            if (cd) reqInt = (int)*(float*)(cd + 0xc4);
        }
        int threshold = residents * reqInt;                       // iVar7(1) * residents * factor
        int seedN = threshold + (residents > 0 ? residents : 0);  // strictly exceed the threshold
        if (seedN < 8) seedN = 8;                                 // minimum visible reserve
        if (seedN > 500) seedN = 500;                             // sanity clamp
        addItem(reinterpret_cast<int64_t>(rescomp), foodTmpl, seedN);
        int have = getResCount ? getResCount(reinterpret_cast<int64_t>(rescomp)) : -1;
        fprintf(stderr, "[startTown::housing] house food(c7cf0 home): residence=%p residents=%d "
                "reqPerResident=%d threshold=%d seeded=%d -> GetResourceCount=%d %s\n",
                rescomp, residents, reqInt, threshold, seedN, have,
                (have >= threshold) ? "(SUFFICIENT: no Gather)" : "(WARN: still insufficient)");
        fflush(stderr);
    }
    fprintf(stderr, "[startTown::housing] seeded food into %d/%zu houses (%d units each, "
            "edibleTmpl=%llx flags=0x%x)\n",
            seeded, g_foundingHouseIBPs.size(), unitsPerHouse,
            (unsigned long long)foodTmpl, foodFlags);
    fflush(stderr);
}

// CitizenComponent_impl_0dd0 — spawn the founding families. `game` (Game+0x0 =
// map dim) and the start-town spawn tile (spawnX, spawnY) give the town-centre
// world position the colonists are jittered around (citizen Foundation-F).
static int StartTown_SpawnCitizens(uint8_t* startConfig, void* entityList,
                                   void* game, int spawnX, int spawnY) {
    const int famCount  = *(int32_t*)(startConfig + FAMILY_CNT);
    uint8_t*  families  = *(uint8_t**)(startConfig + FAMILY_ARR);
    if (famCount <= 0 || !families) return 0;
    if (!fn_EntityFactory_CreateEntity || !fn_EntityList_RegisterEntity ||
        !fn_InternalBasePtr_Ctor || !fn_MemAlloc) return 0;

    // Town-centre world position (same tile->world mapping as the camera) +
    // founding-family spawn jitter radius (startConfig+0x40). FUN_1400a3300 found
    // this tile once and reused it for both camera and the spawn passes.
    float base[3] = { 0.0f, 0.0f, 0.0f };
    if (game && fn_MapGetTerrainPos)
        StartTown_TileToWorld(game, spawnX, spawnY, base);
    const float jitter = *(float*)(startConfig + 0x40);

    // Resolve the shared citizen template once (ExternalRef at startConfig+0x48,
    // same {name,gen,template} layout the pool entries use).
    void* citizenTemplate = StartTown_ResolveTemplate(startConfig + CITIZEN_REF);
    if (!citizenTemplate) {
        fprintf(stderr, "[startTown::citizen] citizen template unresolved\n");
        fflush(stderr);
        return 0;
    }

    // Child count uses the world RNG (g_pGameWorldSingleton+0xd8), as the original
    // does. The exact sequence already diverges from vanilla (we skip the many
    // RandFloat/RandFloatTol draws inside the deferred init), so this only needs
    // to produce a plausible population.
    void* rng = nullptr;
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton)
        rng = *(void**)((uint8_t*)*g_pGameWorldSingleton + 0xd8);

    // Collect each family's spawned citizen entities so the founding-housing pass
    // (below) can pair family f with one registered WoodHouse. Only used when
    // BANISHED_LIVE_HOUSING is set; otherwise the vector is simply discarded.
    std::vector<std::vector<void*>> famEntities;

    // Kid gender alternates across the whole spawn (as retail's `bVar12` does),
    // persisting between families. Starts on gender 0.
    int kidVariant = 0;

    int spawned = 0;
    for (int f = 0; f < famCount; ++f) {
        uint8_t* fam = families + (size_t)f * FAMILY_STRIDE;
        const int educated    = *(int8_t*)fam;
        const int maxChildren = *(int32_t*)(fam + 4);
        famEntities.emplace_back();
        std::vector<void*>& fe = famEntities.back();

        // Two adults — a couple: opposite gender models (variant groups 0 and 1),
        // random adult age, full-size. Matches CitizenComponent_impl_0dd0.
        if (void* a = SpawnOneCitizen(citizenTemplate, entityList, base, jitter, rng, /*isChild=*/false, /*variantIdx=*/0)) { ++spawned; fe.push_back(a); }
        if (void* b = SpawnOneCitizen(citizenTemplate, entityList, base, jitter, rng, /*isChild=*/false, /*variantIdx=*/1)) { ++spawned; fe.push_back(b); }

        // Children: Rand(0.._maxChildCount) — alternating gender, child scale (0.66).
        int kids = 0;
        if (rng && fn_Random_Rand && maxChildren >= 0)
            kids = (int)fn_Random_Rand(rng, maxChildren + 1);
        for (int c = 0; c < kids; ++c) {
            if (void* k = SpawnOneCitizen(citizenTemplate, entityList, base, jitter, rng, /*isChild=*/true, /*variantIdx=*/kidVariant)) { ++spawned; fe.push_back(k); }
            kidVariant ^= 1;
        }

        fprintf(stderr, "[startTown::citizen] family[%d] educated=%d maxKids=%d kids=%d\n",
                f, educated, maxChildren, kids);
        fflush(stderr);
    }

    // Founding-family housing: roster each family into a registered WoodHouse via the
    // faithful AssignHousing primitive (env BANISHED_LIVE_HOUSING; default no-op).
    StartTown_AssignFoundingHousing(famEntities);

    return spawned;
}

// ============================================================================
// RESOURCES pool — FUN_1400a12b0 (0x1400a12b0). Perlin-noise scatter of natural
// resources (trees / stone / iron) across the whole map, excluding the town site.
//
// For each entry in the pool at startConfig+0x70 (count +0x78, stride 0x18):
//   * resolve the template (e.g. Template\NaturalResourceRock.rsc) and find its
//     NaturalResourceDescription (the scatter config)
//   * build a Perlin octave from the desc's frequency fields and walk every map
//     cell: place a resource where  perlin*0.5+0.5 >= density-threshold (+0x34)
//     AND Random::RandFloat <= density (+0x38)
//   * reject cells that are already occupied (Game+0x80 cell +0x08 != 0) or within
//     the town exclusion radius (sizeSq), or whose terrain height is outside
//     [+0x3c, +0x40) or whose slope < +0x44
//   * instantiate the resource entity (CopyCtor) and register it
//
// NaturalResourceDescription scatter fields (verified vs naturalresourcedescription.cpp):
//   +0x28/+0x2c perlin persistance/octaves   +0x30 coord scale divisor
//   +0x34 density threshold   +0x38 random density
//   +0x3c/+0x40 height min/max   +0x44 slope min
//
// The original then sets the resource's grid cell + growth/age + model variant via
// NaturalResourceComponent helpers (Entity_impl_6540, FUN_140120db0,
// ComponentEntity_impl_5cc0) + a per-variant model selection (FUN_1401253d0). All
// dispatch on real (non-inert) components, so that block is GUARDED OFF — the
// resource entities exist + are registered but carry inert components and don't
// render, same gate as structures/citizens. a12b0 also queries height/slope via a
// Picking-ray variant of QuarryComponent_helper_aa20 (NULL in standalone); we use
// the heightmap sampler (fn_MapGetTerrainPos, which fills height + slope) instead.
// ============================================================================

static constexpr int RES_ARR      = 0x70;     // startConfig+0x70 resource pool array
static constexpr int RES_CNT      = 0x78;     // startConfig+0x78 resource pool count
static constexpr int RES_STRIDE   = 0x18;     // pool entry stride (ExternalRef)
static constexpr int CELL_OCCUPANT = 0x08;    // cell +0x08 = occupant ptr (0 = free)
// Safety cap (per pool). The original has NO cap — it scatters the full perlin
// distribution across the whole map (~9k trees on a medium map). A LOW cap is
// actively harmful: the scatter scans y=0..dim, so a small cap fills up in the
// map's top strip and never reaches the camera (this is why trees were invisible).
// Set well above the natural per-pool count so it never clips in practice but
// still bounds a runaway. Override at runtime with BANISHED_RES_CAP=<n>.
static constexpr int RES_POOL_CAP = 16384;

// ---- Natural-resource model-variant selection (FUN_1401253d0 + the
// FUN_1400a12b0 NaturalResourceDescription+0x48 tail) -----------------------
// Trees/crops carry growth-stage model variants; ModelComponent::Startup only
// sets the default variant (0,0) which for a tree is the invisible "seed" mesh,
// so worldgen runs this override to pick a real growth-stage mesh. Rocks/ore
// have the +0x48 flag clear and render correctly with the default variant.

// Graphics::MeshInstance::SetMesh — dispatch slot 0x140151498.
static void (*s_fn_ResSetMesh)(void*, void*) = nullptr;
static bool s_resSetMeshInit = false;
static void EnsureResSetMesh() {
    if (s_resSetMeshInit) return;
    s_resSetMeshInit = true;
    uintptr_t sl = ResolveAddr(Module::Exe, 0x140151498);
    if (sl) s_fn_ResSetMesh = *reinterpret_cast<void (**)(void*, void*)>(sl);
}

// Mesh lookup mirroring ModelComponent::GetMeshFromDescription and the inline
// formula in FUN_1401253d0: groupArray[variantIdx].subArray[modelVariant].
static void* ResMeshFromModelDesc(void* modelDesc, int variantIdx, int modelVariant) {
    if (!modelDesc || variantIdx < 0 || modelVariant < 0) return nullptr;
    void* groupArray = *(void**)((uint8_t*)modelDesc + 0x20);
    if (!groupArray) return nullptr;
    void* sub = *(void**)((uint8_t*)groupArray + (int64_t)variantIdx * 0x18 + 8);
    if (!sub) return nullptr;
    return *(void**)((uint8_t*)sub + (int64_t)modelVariant * 8);
}

// Standalone-safe port of FUN_1401253d0: set the ModelComponent's growth-stage
// variant indices (+0x164/+0x166) and re-point the MeshInstance mesh so the
// register-time OnAdded/Draw adds the correct mesh to Graphics::Scene. (The
// original gates the mesh lookup behind IsDerivedFrom(desc, ModelDescription),
// which returns false in standalone's partial factory tree — so we trust the
// linked +0x18 description, the IsDerivedFromOrTrust policy used throughout the
// spawn path. Called BEFORE registration, so we just set fields + mesh; OnAdded
// then does the single scene-add, rather than the original's remove/re-add.)
static void ApplyResourceModelVariant(void* entity, uint32_t tidModel,
                                      int16_t variantIdx, int16_t modelVariant) {
    EnsureResSetMesh();
    if (!s_fn_ResSetMesh || !fn_InternalBasePtr_Ctor || !fn_InternalBasePtr_CheckPtr) return;
    void* mc = FindComponentOnEntity(entity, tidModel);
    if (!mc) return;
    uintptr_t mcIBP = 0;
    fn_InternalBasePtr_Ctor(&mcIBP, mc);
    fn_InternalBasePtr_CheckPtr(&mcIBP);
    uint8_t* m = (uint8_t*)mcIBP;
    if (!m) return;
    void* modelDesc = *(void**)(m + 0x18);
    *(int16_t*)(m + 0x164) = variantIdx;
    *(int16_t*)(m + 0x166) = modelVariant;
    if (modelVariant >= 0) {
        void* mesh = ResMeshFromModelDesc(modelDesc, variantIdx, modelVariant);
        if (mesh) s_fn_ResSetMesh((void*)(m + 0x28), mesh);
    }
}

// ---- Natural-resource grid placement + growth setup ------------------------
// Faithful, standalone-safe port of the FUN_1400a12b0 per-cell setup tail:
//   Entity_impl_6540(NRComp, cellIdx)  — grid placement + jittered model transform
//   FUN_140120db0(GrowthComp, RandFloat) — growth/age + mesh scale init
// (ComponentEntity_impl_5cc0/SpawnComponent — the initial sub-resource spawner —
//  is DEFERRED: it recursively spawns more entities via ComponentEntity_impl_5780
//  and reads GrowthComponent+0x28 which is only linked when GrowthComponent is LIVE.
//  Not needed for grid placement / economy; left guarded off.)
//
// Standalone deviations from the disassembly (all documented project policies):
//   * No IsDerivedFrom gate on the linked descriptions — IsDerivedFrom is
//     unreliable in the partial standalone factory tree (returns false → null d →
//     crash). We trust comp+0x18 (the copy-ctor stored the resource's own desc)
//     and null-guard every deref (IsDerivedFromOrTrust policy).
//   * Position jitter / random yaw use Random::RandFloatTol when its Runtime export
//     is resolved (byte-faithful, matches Entity_impl_6540); if unresolved, fall back
//     to the RandFloat∈[0,1] → [-1,1] remap (same draw count, approximate values).
//   * The 120db0 growth/scale block only runs when the GrowthComponent's sibling
//     links (+0x28 MeshInstance host, +0x30 ModelComponent, +0x38 GrowthDescription)
//     are populated — i.e. when GrowthComponent is LIVE (BANISHED_LIVE_GROWTH) and
//     its Startup has run. Default-inert build → those are zero → growth is a
//     no-op (never a null deref), exactly as the rest of the inert spawn path.
static float* s_pYaw180 = nullptr;     // DAT_140172220 = 180.0
static bool   s_pYaw180Init = false;
static float ResYawScale() {
    if (!s_pYaw180Init) {
        s_pYaw180Init = true;
        s_pYaw180 = (float*)ResolveAddr(Module::Exe, 0x140172220);
    }
    return s_pYaw180 ? *s_pYaw180 : 180.0f;
}

// ---- CreateNaturalResource grid marking (impl_dee0 chain) ------------------
// Faithful, standalone-safe port of CreateNaturalResource_impl_dee0 (0x14009dee0),
// the cell-grid TERRAIN-TYPE writer that the real game runs from
// MapComponent::OnCreate -> CreateNaturalResource_impl_4750 -> impl_dee0. In
// standalone MapComponent is COMPONENT_INERT_FACTORY (OnCreate is a stub) and
// impl_4750 is unregistered (ResolveAddr -> 0), so that chain never fires — we
// invoke this directly from the resource spawn path instead.
//
// What it does, per the disassembly:
//   for (iy=0..h) for (ix=0..w):
//     cellIdx = (iy+tileY)*dim + ix+tileX
//     occupied = shape==0 || *(int*)(shape-8)==0 ? true : (rot-aware shape char is '#'|'_')
//     cell = Game_cells + cellIdx*0x30
//     if (occupied):
//       t = helper_e2c0(game, cellIdx)            // current natural terrain type
//       if ((1<<(t&0x1f) & 0x1e0) && cell[+0x00]!=0 && cell[+0x00]!=*ownerIBP) t = 2
//       FUN_14009f2f0(game, cellIdx, t)           // WRITE the type byte (+0x29) + dirty flag
//     cell[+0x08] = null-IBP                       // CLEAR the cell entity-link
//
// helper_e2c0 (0x14009e2c0) == MapTileGrid_Classify (maptilegrid.cpp): classify a
// cell into a terrain type from its terrain height + slope. Replicated inline here
// (identical exe .rdata thresholds) so starttown stays self-contained.
//
// FUN_14009f2f0 (0x14009f2f0): writes cell+0x29 = the new type byte, ORs a
// "type-class changed" bit into Game+0xa2 (dirty flag), and — when the type's
// walkable-class flips — fires two notify side effects: FUN_1400a8c90 (region/
// minimap dirty-rect, via Game+0x70) and FUN_1400a6fe0 (a worker-thread message
// queue, via Game+0x68). Both touch live subsystems that are inert/unbuilt in
// standalone (Game+0x68/0x70 are zero); we port the core type-byte write + the
// +0xa2 flag and GUARD OFF the two notify calls (null-guarded — a no-op).
//
// dee0 sets the cell TERRAIN-TYPE/walkability (the real "a tree-type cell exists
// here" marking that pathfinding + harvest-search read) and CLEARS cell+0x08; it
// does NOT store the entity in cell+0x08 (that is the building path, FUN_14009dc20).
// A natural resource is non-blocking — citizens walk to it to harvest.

// e2c0 classifier thresholds (exe .rdata, identical to maptilegrid.cpp).
static constexpr float kClassBuildableMax = 0.75f;   // DAT_140172124
static constexpr float kClassSlopeMin     = 0.9f;     // _DAT_140172134
static constexpr float kClassMountainMax  = 2.0f;     // DAT_140172174
static constexpr float kClassShoreMax     = -0.25f;   // DAT_140172274
static constexpr float kClassDeepWaterMax = -1.4f;    // DAT_140172284

// helper_e2c0 — classify cell `idx` into a terrain type byte.
static int StartTown_ClassifyCell(void* game, int dim, int idx) {
    const float center = (float)(dim >> 1) - kCenterBias;
    float in[3]  = { (float)(idx % dim) - center, (float)(idx / dim) - center, 0.0f };
    float out[3] = { 0, 0, 0 };
    float tri[4] = { 0, 0, 0, 0 };
    fn_MapGetTerrainPos(game, out, in, 1, tri);   // flag=1 => height-only (matches e2c0)
    const float h = out[2], slope = tri[2];
    if (h < kClassDeepWaterMax) return 8;
    if (h < kClassShoreMax)     return 7;
    if (h >= kClassShoreMax && h <= kClassBuildableMax && slope >= kClassSlopeMin) return 2;
    return (h < kClassMountainMax) ? 10 : 6;
}

// FUN_14009f2f0 — write cell+0x29 = `type` and set the Game+0xa2 dirty flag. The
// type-class-flip notify calls (a8c90 region-dirty, a6fe0 worker-queue) require
// Game+0x68/+0x70 live subsystems (zero in standalone) and are guarded off here.
static void StartTown_WriteCellType(uint8_t* game, uint8_t* cells, int idx, uint8_t newType) {
    uint8_t* cell = cells + (size_t)idx * CELL_STRIDE;
    const uint8_t oldType = cell[0x29];
    const uint32_t oldMask = 1u << (oldType & 0x1f);
    const uint32_t newMask = 1u << (newType & 0x1f);
    // dirty bit: set when the cell crosses the buildable<->water/mountain boundary
    // (matches f2f0's "((old&0xe1f && new&0x1e0) || (old&0x1e0 && new&0xe1f))").
    uint8_t dirty = 0;
    if (((oldMask & 0xe1f) && (newMask & 0x1e0)) || ((oldMask & 0x1e0) && (newMask & 0xe1f)))
        dirty = 1;
    game[0xa2] |= dirty;
    cell[0x29] = newType;
    // (region/minimap + worker-thread notifies deliberately omitted — inert subsystems)
}

// G3 — write the resource entity into its map cell (cell+0x08) so the ForesterWork
// harvest scan can find it. Default ON; opt out with BANISHED_RES_OCCUPANCY=0. See
// the long note at the cell+0x08 write below for the dee0/dc20 two-pass rationale.
static bool g_resOccupancy = []() {
    const char* e = std::getenv("BANISHED_RES_OCCUPANCY");
    return !(e && e[0] == '0');   // default ON; only "0" disables
}();

// CreateNaturalResource_impl_dee0 — mark the (w x h) footprint at (tileX,tileY) on
// the Game cell grid for this natural resource. `ownerEntity` = the spawned
// resource entity (compared against cell+0x00 for the type-2 override). `shape` =
// the MapDescription+0x28 char-grid String buffer (null/empty for a 1x1 resource).
static void StartTown_MarkResourceCellGrid(void* gameVoid, void* ownerEntity, int tileX,
                                           int tileY, int w, int h, const char* shape, int rot) {
    if (!gameVoid || w <= 0 || h <= 0 || !fn_MapGetTerrainPos) return;
    uint8_t* game = (uint8_t*)gameVoid;
    const int dim = *(int32_t*)game;
    uint8_t* cells = *(uint8_t**)(game + GAME_CELLS);
    if (dim <= 0 || !cells) return;
    // shape length header sits at shape-8 (length-prefixed buffer); 0 => no shape.
    const int shapeLen = shape ? *(const int*)(shape - 8) : 0;

    for (int iy = 0; iy < h; ++iy) {
        for (int ix = 0; ix < w; ++ix) {
            const int cx = ix + tileX, cy = iy + tileY;
            if (cx < 0 || cy < 0 || cx >= dim || cy >= dim) continue;  // OOB guard (std-safe)
            const int cellIdx = cy * dim + cx;

            // Occupancy from the rot-aware shape grid (dee0). No shape => all cells.
            bool occupied;
            if (!shape || shapeLen == 0) {
                occupied = true;
            } else {
                // rot-aware index into the shape grid (matches dee0's index math).
                int gx, gy, gw;
                if (rot == 0x5a)          { gw = h; gx = iy;            gy = (w - ix) - 1; }
                else if (rot == 0xb4)     { gw = w; gx = (w - ix) - 1;  gy = (h - iy) - 1; }
                else if (rot == 0x10e)    { gw = h; gx = (h - iy) - 1;  gy = ix; }
                else                      { gw = w; gx = ix;            gy = iy; }
                const long sidx = (long)gy * (gw + 1) + gx;
                const char c = (sidx >= 0 && sidx < shapeLen) ? shape[sidx] : 0;
                occupied = (c == '#' || c == '_');
            }

            uint8_t* cell = cells + (size_t)cellIdx * CELL_STRIDE;
            if (occupied) {
                uint8_t t = (uint8_t)StartTown_ClassifyCell(gameVoid, dim, cellIdx);
                // override to buildable(2) only if the natural type is water/mountain
                // AND the cell already holds a *different* owner (cell+0x00).
                void* cellOwner = *(void**)cell;   // cell+0x00
                if ((1u << (t & 0x1f) & 0x1e0u) && cellOwner && cellOwner != ownerEntity)
                    t = 2;
                StartTown_WriteCellType(game, cells, cellIdx, t);
            }
            // Cell entity-link (cell+0x08).
            //
            // The real game writes occupancy in TWO passes: MapComponent::OnCreate runs
            // dee0 (which CLEARS cell+0x08), then MapComponent::Draw lazily runs
            // sub_41b0 -> dc20 (FUN_14009dc20), which writes cell+0x08 = the owner entity
            // (for an occupied/shape-'#' cell). The NET end-state is cell+0x08 = entity.
            // In standalone MapComponent is inert, so Draw never fires and the occupancy
            // is never written -> the ForesterWork harvest scan finds no tree at cell+0x08
            // (foresterwork.cpp:745) and idles. We collapse the two passes into this one:
            // when occupancy is enabled, write the entity (dc20's net effect) instead of
            // clearing. We deliberately KEEP dee0's classified buildable(2) terrain type
            // (set above) rather than dc20's MapDescription+0x20 stamp — both are non-0x1e0
            // and passable, so the forester pre-check + zone gate behave identically, and
            // dee0's classification is the value already proven safe in standalone. We
            // cannot call the ported sub_41b0/dc20 directly: their IsDerivedFrom(MapDesc)
            // returns FALSE in standalone's partial factory tree -> null-deref (the same
            // landmine documented for FUN_1401253d0); here we trust the linked entity.
            if (occupied && g_resOccupancy && ownerEntity) {
                uintptr_t entIBP = 0;
                if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&entIBP, ownerEntity);
                *(uintptr_t*)(cell + 0x08) = entIBP;   // dc20: *(cell+8) = owner entity
            } else {
                // dee0's clear (occupancy disabled, or non-occupied footprint cell).
                uintptr_t nullIBP = 0;
                if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
                *(uintptr_t*)(cell + 0x08) = nullIBP;
            }
        }
    }
}

// CreateNaturalResource_impl_dee0 — UN-mark (economy brick M-B). Faithful port of the
// dee0 semantics the REMOVAL path needs: for each footprint cell, reclassify the
// terrain type back to its natural class (occupied/shape cells only, with dee0's
// buildable(2) override when the cell is owned by a DIFFERENT entity), and null the
// cell entity-link (cell+0x08) for EVERY footprint cell (dee0 writes the null IBP
// unconditionally, outside the shape check). Retail dispatch: entity mark-removal
// (FUN_1400c2ea0) -> MapComponent vtable+0x38 (0x1401242f0) -> impl_4750 (0x140124750)
// -> dee0. Without this the removed tree's cell keeps a dangling entity pointer
// (CheckPtr is a release no-op — the IBP system is raw pointers) and any grid reader
// (CitizenAI leisure box scan, forester scan) faults on the freed entity.
extern "C" void StartTown_UnmarkResourceCellGrid(void* gameVoid, void* ownerEntity, int tileX,
                                                 int tileY, int w, int h, const char* shape, int rot) {
    if (!gameVoid || w <= 0 || h <= 0 || !fn_MapGetTerrainPos) return;
    uint8_t* game = (uint8_t*)gameVoid;
    const int dim = *(int32_t*)game;
    uint8_t* cells = *(uint8_t**)(game + GAME_CELLS);
    if (dim <= 0 || !cells) return;
    const int shapeLen = shape ? *(const int*)(shape - 8) : 0;

    for (int iy = 0; iy < h; ++iy) {
        for (int ix = 0; ix < w; ++ix) {
            const int cx = ix + tileX, cy = iy + tileY;
            if (cx < 0 || cy < 0 || cx >= dim || cy >= dim) continue;  // OOB guard (std-safe)
            const int cellIdx = cy * dim + cx;

            // Occupancy from the rot-aware shape grid (same verified math as the mark).
            bool occupied;
            if (!shape || shapeLen == 0) {
                occupied = true;
            } else {
                int gx, gy, gw;
                if (rot == 0x5a)          { gw = h; gx = iy;            gy = (w - ix) - 1; }
                else if (rot == 0xb4)     { gw = w; gx = (w - ix) - 1;  gy = (h - iy) - 1; }
                else if (rot == 0x10e)    { gw = h; gx = (h - iy) - 1;  gy = ix; }
                else                      { gw = w; gx = ix;            gy = iy; }
                const long sidx = (long)gy * (gw + 1) + gx;
                const char c = (sidx >= 0 && sidx < shapeLen) ? shape[sidx] : 0;
                occupied = (c == '#' || c == '_');
            }

            uint8_t* cell = cells + (size_t)cellIdx * CELL_STRIDE;
            if (occupied) {
                uint8_t t = (uint8_t)StartTown_ClassifyCell(gameVoid, dim, cellIdx);
                void* cellOwner = *(void**)cell;   // cell+0x00
                if ((1u << (t & 0x1f) & 0x1e0u) && cellOwner && cellOwner != ownerEntity)
                    t = 2;
                StartTown_WriteCellType(game, cells, cellIdx, t);
            }
            *(uintptr_t*)(cell + 0x08) = 0;        // dee0: null the cell entity-link, ALWAYS
        }
    }
}

static void StartTown_MatureResource(void* nrComp);   // defined below (BANISHED_MATURE_TREES)

// Entity_impl_6540 — write the MapComponent grid cell (tileX/tileY, w=1,h=1) and
// the jittered ModelComponent world transform. `cellIdx` = dim*y + x.
static void StartTown_PlaceResourceCell(void* entity, int cellIdx, void* game, void* rng,
                                        uint32_t tidNR, uint32_t tidMap, uint32_t tidModel) {
    if (!entity || !game) return;
    const int dim = *(int32_t*)game;
    if (dim <= 0) return;
    const int tileX = cellIdx % dim;
    const int tileY = cellIdx / dim;

    // --- MapComponent grid placement (priority #1: lets lookups find the resource,
    //     stops overlap once OnCreate writes occupancy). Plain data writes — always
    //     safe on the inert MapComponent (size 0x38; fields +0x28..+0x2f). ---
    void* mapComp = FindComponentOnEntity(entity, tidMap);
    if (mapComp && fn_InternalBasePtr_Ctor && fn_InternalBasePtr_CheckPtr) {
        uintptr_t mapIBP = 0;
        fn_InternalBasePtr_Ctor(&mapIBP, mapComp);
        fn_InternalBasePtr_CheckPtr(&mapIBP);
        uint8_t* mc = (uint8_t*)mapIBP;
        if (mc) {
            *(int16_t*)(mc + 0x28) = (int16_t)tileX;
            *(int16_t*)(mc + 0x2a) = (int16_t)tileY;
            *(uint64_t*)(mc + 0x2c) = 0x10001;   // w=1, h=1 (and +0x30 extra=0)

            // CreateNaturalResource_impl_dee0 — mark this cell's terrain TYPE on the
            // Game grid so pathfinding + harvest-search know a resource sits here.
            // The real game runs this from MapComponent::OnCreate (inert/unregistered
            // in standalone); we invoke it here, right after the tile fields are set.
            // shape = MapDescription(mc+0x18)+0x28 String buffer (null for a 1x1
            // resource -> dee0 treats the single cell as occupied). rot=0 (1x1).
            const char* shape = nullptr;
            void* mapDesc = *(void**)(mc + 0x18);
            if (mapDesc) shape = *(const char**)((uint8_t*)mapDesc + 0x28);  // String char*
            StartTown_MarkResourceCellGrid(game, entity, tileX, tileY, /*w*/1, /*h*/1,
                                           shape, /*rot*/0);

            // M-B: mirror MapComponent_Slot12's post-write flag — bit 0x100000 of
            // comp+0x20 = "occupancy currently written on the grid". The removal path
            // (c2ea0 -> vtable+0x38 -> impl_4750) gates the grid UN-mark on this bit,
            // so it must be set exactly when the collapse above wrote cell+0x08.
            if (g_resOccupancy)
                *(uint32_t*)(mc + 0x20) |= 0x100000u;
        }
    }

    // --- jittered ModelComponent transform (the rest of 6540). Mirrors the
    //     committed SetEntityModelTransform but with the per-resource position jitter
    //     (desc+0x60) + random yaw the original applies. ---
    if (!fn_MapGetTerrainPos || !fn_Transform_FromYawPitchOffset) return;

    // jitter radius from the NaturalResourceDescription (NRComp+0x18, +0x60).
    float jitter = 0.0f;
    void* nrComp = FindComponentOnEntity(entity, tidNR);
    if (nrComp) {
        void* desc = *(void**)((uint8_t*)nrComp + 0x18);
        if (desc) jitter = *(float*)((uint8_t*)desc + 0x60);
        StartTown_MatureResource(nrComp);   // BANISHED_MATURE_TREES: old-growth (harvestable) forest
    }

    // terrain position for the cell centre (matches StartTown_TileToWorld).
    const float center = (float)(dim >> 1) - kCenterBias;
    float in[3]  = { (float)(cellIdx % dim) - center, (float)(cellIdx / dim) - center, 0.0f };
    float pos[3] = {0,0,0};
    fn_MapGetTerrainPos(game, pos, in, 0, nullptr);

    // Per-resource jitter/yaw tolerance — byte-faithful to Entity_impl_6540, which
    // uses System::Random::RandFloatTol → [-1,1]. fn_Random_RandFloatTol is the
    // worldgen_f390_shims link shim over the Runtime export (same Random family as the
    // RandFloat/RandTol/Rand this function already requires); it returns 0 only if the
    // export can't resolve, which never happens once the engine funcs are wired.
    auto rtol = [&]() -> float { return rng ? fn_Random_RandFloatTol(rng) : 0.0f; };
    float worldX = rtol() * jitter + pos[0];
    float worldY = rtol() * jitter + pos[1];
    float worldZ = pos[2];
    float yaw = rtol() * ResYawScale();   // RandFloatTol * 180 -> full [-180,180] rotation

    void* modelComp = FindComponentOnEntity(entity, tidModel);
    if (!modelComp || !fn_InternalBasePtr_Ctor || !fn_InternalBasePtr_CheckPtr) return;
    uintptr_t mIBP = 0;
    fn_InternalBasePtr_Ctor(&mIBP, modelComp);
    fn_InternalBasePtr_CheckPtr(&mIBP);
    uint8_t* m = (uint8_t*)mIBP;
    if (!m) return;
    float pitch = 0.0f;
    float off[3] = { worldX, worldY, worldZ };
    fn_Transform_FromYawPitchOffset((void*)(m + 0x148), &yaw, &pitch, off);
    *(float*)(m + 0x168) = yaw;
    if (fn_Geometry_SetTransform)
        fn_Geometry_SetTransform((void*)(m + 0x28), (void*)(m + 0x148));
}

// FUN_140120db0 — growth/age + mesh-scale init on the GrowthComponent. Delegates to
// the verified ActCNR_SetGrowthProgress_120db0 reimpl (code/Game/Action/
// action_base_cnr.cpp), which is a 1:1 port using the real Math::Vector4 / Time
// ctors. We pre-check that the component's sibling links are populated (+0x28
// MeshInstance host, +0x30 ModelComponent, +0x38 GrowthDescription) so it is a
// faithful no-op on a default-inert GrowthComponent (those are zero until
// GrowthComponent::Startup runs, i.e. the LIVE build) rather than a null deref.
// The original passes Random::RandFloat(rng) as the growth progress (staged trees).
void ActCNR_SetGrowthProgress_120db0(int64_t self, float progress);  // action_base_cnr.cpp

static void StartTown_GrowthInitResource(void* entity, uint32_t tidGrowth, void* rng) {
    if (!entity) return;
    static const bool s_trace = std::getenv("BANISHED_GROWTH_TRACE") != nullptr;
    static int s_traced = 0;
    void* gComp = FindComponentOnEntity(entity, tidGrowth);
    if (s_trace && s_traced < 6) { ++s_traced;
        fprintf(stderr, "[GrowthInit] ent=%p tid(Growth)=%u FindComponent=%p\n",
                entity, tidGrowth, gComp);
        fflush(stderr);
    }
    if (!gComp || !fn_InternalBasePtr_Ctor || !fn_InternalBasePtr_CheckPtr) return;
    uintptr_t gIBP = 0;
    fn_InternalBasePtr_Ctor(&gIBP, gComp);
    fn_InternalBasePtr_CheckPtr(&gIBP);
    uint8_t* s = (uint8_t*)gIBP;
    if (!s) return;
    if (s_trace && s_traced <= 6) {
        fprintf(stderr, "[GrowthInit]   siblings +0x28=%p +0x30=%p +0x38=%p\n",
                *(void**)(s + 0x28), *(void**)(s + 0x30), *(void**)(s + 0x38));
        fflush(stderr);
    }
    // Sibling links populated only when GrowthComponent::Startup ran (LIVE build).
    if (!*(void**)(s + 0x28) || !*(void**)(s + 0x30) || !*(void**)(s + 0x38))
        return;   // inert → faithful no-op (matches the rest of the inert spawn path)
    const float progress = (rng && fn_Random_RandFloat) ? fn_Random_RandFloat(rng) : 0.0f;
    ActCNR_SetGrowthProgress_120db0((int64_t)gIBP, progress);
}

// Mature the start-town forest so the forester can harvest it.
//
// ⚠️ CORRECTED 2026-07-01 (live-verified): the prior claim that "trees carry NO
// GrowthComponent" is FALSE. The tree template (NaturalResourceFigTree.rsc etc.)
// declares a `growth` component and the standalone entity factory DOES materialise it —
// FindComponent(GrowthComponent) returns non-null on every tree, and with
// BANISHED_LIVE_GROWTH its Startup links +0x28 (NaturalResourceComponent) and +0x38
// (GrowthDescription). The one thing that was broken: +0x30 (the ModelComponent sibling)
// never linked, because GrowthComponent::Startup finds it via ActCNR_FindComponent_3f60,
// whose ModelComponent type-id (raw .rdata 0x1401c7a50) resolved NULL in standalone.
// That is now FIXED (action_base_cnr.cpp GetTypeIdByName fallback) → all three siblings
// link and StartTown_GrowthInitResource stages growth at random progress, exactly like
// retail (Random::RandFloat → FUN_140120db0). So growth STAGING + per-tick ticking work.
//
// M-A UPDATE (2026-07-01): the "DoHarvestReset faults because resourceMgr+0x80 is
// not built" claim was a MISDIAGNOSIS. `*(gameWorld+0xF8)` is the `Game` object and
// its +0x80 per-tile grid IS built in standalone (MapTileGrid_Build = the faithful
// front-half of CreateNaturalResource_impl_ef20 0x14009ef20: dim*dim records, stride
// 0x30). The real fault was a stray extra deref in the DoHarvestReset reimpl (it read
// `*(uintptr_t*)gridBase` and wrote near-null). That is FIXED (growthcomponent.cpp),
// and BANISHED_LIVE_GROWTH now runs the growth sim through DoHarvestReset crash-free
// (verified: 8 harvest-resets, valid gridBase + in-bounds resIdx, 0-byte crash log).
// ⇒ MATURE_TREES is no longer needed to AVOID A CRASH — live growth is safe. It
// remains a default-OFF CONVENIENCE that makes trees instantly forester-harvestable
// (naturally reaching the growth threshold takes a long sim horizon).
//
// The forester's harvest gate reads the NaturalResourceComponent's growth field directly
// (natResComp+0x28 >= threshold 1.0, foresterwork.cpp), so we set that field to a mature
// value at spawn — the faithful END-STATE of a fully-grown tree (what 120db0 writes at
// progress=1.0). Gated BANISHED_MATURE_TREES (default OFF). `nrComp` is the
// NaturalResourceComponent already located by the caller (PlaceResourceCell, for jitter).
static void StartTown_MatureResource(void* nrComp) {
    static int s_mature = -1;
    if (s_mature < 0) {
        const char* e = std::getenv("BANISHED_MATURE_TREES");
        s_mature = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    if (s_mature != 1 || !nrComp) return;
    // Represent an OLD-GROWTH, HARVESTABLE tree — the faithful end-state a live GrowthComponent
    // would reach (its Update->120db0 only writes +0x28 growth; the mature/harvestable state
    // flags are set by the growth/harvest-marking layer, which is inert in standalone). Three
    // fields, mirroring what the FORCE_HARVEST harness sets on the found tree:
    //   +0x28 growth (float) = 1.0  — the forester's maturity threshold (g_fUVScale); the scan
    //          gate is `growth < threshold -> skip`, so 1.0 passes.
    //   +0x2c  mature (bool)     = 1 — RemoveNaturalResource::Main derives this from growth, and
    //          FUN_1401260f0 uses it to pick the MATURE yield field (desc+0x44, not +0x40).
    //   +0x2d  harvestable (bool)= 1 — FUN_1401260f0's produce gate (`+0x2d==0 -> return`); set
    //          organically by the (standalone-inert) AnimateNaturalResource cut-sequence step.
    // Together these let the ORGANIC forester cut sequence run the produce accounting end-to-end
    // (RegisterRawMaterial -> town stock++). Neither flag affects the forester scan (it gates on
    // +0x28 growth only), so an all-harvestable starting forest is behaviourally correct.
    *(float*)((uint8_t*)nrComp + 0x28)   = 1.0f;
    *(uint8_t*)((uint8_t*)nrComp + 0x2c) = 1;
    *(uint8_t*)((uint8_t*)nrComp + 0x2d) = 1;
}

static int StartTown_SpawnResources(uint8_t* startConfig, void* world,
                                    int spawnX, int spawnY, int sizeSq) {
    const int count = *(int32_t*)(startConfig + RES_CNT);
    uint8_t* pool   = *(uint8_t**)(startConfig + RES_ARR);
    if (count <= 0 || !pool) return 0;
    if (!fn_EntityFactory_CreateEntity || !fn_EntityList_RegisterEntity ||
        !fn_InternalBasePtr_Ctor || !fn_MemAlloc ||
        !fn_PerlinNoise_Ctor || !fn_PerlinNoise_Generate ||
        !fn_Random_RandTol || !fn_Random_RandFloat || !fn_MapGetTerrainPos) {
        fprintf(stderr, "[startTown::res] primitives unwired — skip\n"); fflush(stderr);
        return 0;
    }

    void* game = *(void**)((uint8_t*)world + 0xf8);
    if (!game) return 0;
    const int dim = *(int32_t*)game;
    uint8_t* cells = *(uint8_t**)((uint8_t*)game + GAME_CELLS);
    void* rng = *(void**)((uint8_t*)world + 0xd8);
    if (dim <= 0 || !cells || !rng) return 0;

    void* entityList = *(void**)((uint8_t*)world + WORLD_ENTITYLIST);
    const float uvScale = g_fUVScale ? *g_fUVScale : 1.0f;
    const uint32_t tidNatRes = TypeId("NaturalResourceDescription");
    const uint32_t tidModel  = TypeId("ModelComponent");
    // Type-ids for the per-cell setup (Entity_impl_6540 + FUN_140120db0).
    const uint32_t tidNRComp = TypeId("NaturalResourceComponent");  // DAT_1401c7b30
    const uint32_t tidMapComp = TypeId("MapComponent");             // DWORD_1401c79e0
    const uint32_t tidGrowth = TypeId("GrowthComponent");           // DAT_1401c77b0

    // DIAG harness: BANISHED_RES_ONLY=<poolIndex> spawns only that pool (isolate trees).
    static const char* s_onlyEnv = std::getenv("BANISHED_RES_ONLY");
    const int onlyPool = s_onlyEnv ? atoi(s_onlyEnv) : -1;
    static const char* s_capEnv = std::getenv("BANISHED_RES_CAP");
    const int poolCap = s_capEnv ? atoi(s_capEnv) : RES_POOL_CAP;

    // FAITHFUL RNG ALIGNMENT (FUN_1400a12b0 prologue). Before the pool loop the
    // original constructs THREE PerlinNoise objects (freq = g_fUVScale on both axes)
    // that are never subsequently used — but the 6 Random::RandTol draws they consume
    // offset the shared world RNG (world+0xd8). Omitting them shifts every downstream
    // draw, so the scatter can't reproduce a vanilla map seed. Replicate exactly: 6
    // RandTol + 3 deterministic ctor calls (the ctor is seeded purely by its int args
    // and never touches the RNG, so a single throwaway buffer is byte-equivalent).
    {
        uint8_t throwaway[0x100];
        for (int i = 0; i < 3; ++i) {
            int t1 = fn_Random_RandTol(rng, 0x7ffffff);
            int t2 = fn_Random_RandTol(rng, 0x7ffffff);
            fn_PerlinNoise_Ctor(throwaway, uvScale, uvScale, t2, t1);
        }
    }

    int spawned = 0;
    for (int e = 0; e < count; ++e) {
        if (onlyPool >= 0 && e != onlyPool) continue;
        uint8_t* entry = pool + (size_t)e * RES_STRIDE;
        const char* ename = *(char**)entry ? *(char**)entry : "(null)";
        fprintf(stderr, "[startTown::res] begin [%d] name=\"%.48s\"\n", e, ename);
        fflush(stderr);
        void* tmpl = StartTown_ResolveTemplate(entry);
        if (!tmpl) continue;
        // Capture the first resource template as a produced-material description for the
        // Phase B production harness (BANISHED_LIVE_PRODUCTION). Diagnostic-only.
        if (!g_pSampleResourceTemplate) g_pSampleResourceTemplate = tmpl;
        uint8_t* desc = (uint8_t*)FindDescriptionOnTemplate(tmpl, tidNatRes);
        if (!desc) { fprintf(stderr, "[startTown::res] [%d] no NaturalResourceDescription\n", e); fflush(stderr); continue; }

        const float persist  = *(float*)(desc + 0x28);
        const float octaves  = *(float*)(desc + 0x2c);
        float div            = *(float*)(desc + 0x30);
        if (div == 0.0f) div = 1.0f;
        const float scale    = uvScale / div;
        const float threshold = *(float*)(desc + 0x34);
        const float density   = *(float*)(desc + 0x38);
        const float hMin      = *(float*)(desc + 0x3c);
        const float hMax      = *(float*)(desc + 0x40);
        const float slopeMin  = *(float*)(desc + 0x44);
        const int category = *(int32_t*)((uint8_t*)tmpl + TEMPLATE_CATEGORY);

        // Per-pool growth-variant setup (FUN_1400a12b0's NaturalResourceDescription
        // +0x48 block). When set (trees/crops), build one Perlin field per growth
        // variant group so each cell can pick a biome-clustered variant; rocks/ore
        // have +0x48 clear and keep the default variant.
        //
        // FAITHFUL RNG ORDER: the original builds the per-variant Perlins FIRST (inside
        // the +0x48 block), THEN the scatter Perlin. Both are seeded from the same world
        // RNG, so the build order decides which seeds each field receives — it must run
        // variant-then-scatter or trees end up with a different distribution than vanilla
        // for a given map seed (and every later pool inherits the RNG offset).
        static const bool s_noVariant = std::getenv("BANISHED_NO_RES_VARIANT") != nullptr;
        const bool hasVariants = !s_noVariant && desc[0x48] != 0;
        void* modelDesc = hasVariants ? FindDescriptionOnTemplate(tmpl, TypeId("ModelDescription")) : nullptr;
        int   variantCount = modelDesc ? *(int32_t*)((uint8_t*)modelDesc + 0x28) : 0;
        const float vFreqX = *(float*)(desc + 0x4c);
        const float vFreqY = *(float*)(desc + 0x50);
        float vDiv         = *(float*)(desc + 0x54);
        if (vDiv == 0.0f) vDiv = 1.0f;
        const float vScale  = uvScale / vDiv;
        const float vThresh = *(float*)(desc + 0x58);
        int vOverrideDenom  = *(int32_t*)(desc + 0x5c);
        if (vOverrideDenom <= 0) vOverrideDenom = 1;
        static constexpr int MAX_VARIANTS = 8;
        uint8_t vPerlin[MAX_VARIANTS][0x100];
        // The original loops the FULL (uncapped) variant count, building one Perlin per
        // group. The stack buffer caps storage at MAX_VARIANTS, so draw RandTol for every
        // variant (keeps the RNG stream aligned even if a resource ever exceeds the cap)
        // but only Ctor into the buffer for the first MAX_VARIANTS. Trees have < 8 groups.
        if (hasVariants && variantCount > 0) {
            for (int vi = 0; vi < variantCount; ++vi) {
                int vs1 = fn_Random_RandTol(rng, 0x7ffffff);
                int vs2 = fn_Random_RandTol(rng, 0x7ffffff);
                if (vi < MAX_VARIANTS)
                    fn_PerlinNoise_Ctor(vPerlin[vi], vFreqX, vFreqY, vs2, vs1);
            }
        }
        if (variantCount > MAX_VARIANTS) variantCount = MAX_VARIANTS;

        // Scatter Perlin — built AFTER the variant Perlins, matching a12b0's draw order.
        int s1 = fn_Random_RandTol(rng, 0x7ffffff);
        int s2 = fn_Random_RandTol(rng, 0x7ffffff);
        uint8_t perlin[0x100] = {};
        fn_PerlinNoise_Ctor(perlin, persist, octaves, s2, s1);

        const bool doVariant = hasVariants && modelDesc && variantCount > 0 && fn_Random_Rand;
        fprintf(stderr, "[startTown::res] [%d] variants:has=%d count=%d thr=%.3f denom=%d "
                "(scatter div=%.2f thr=%.3f dens=%.3f)\n",
                e, (int)hasVariants, variantCount, vThresh, vOverrideDenom, div, threshold, density);
        fflush(stderr);

        int poolSpawned = 0;
        bool capped = false;
        for (int y = 0; y < dim && !capped; ++y) {
            for (int x = 0; x < dim; ++x) {
                float p = fn_PerlinNoise_Generate(perlin, (float)x * scale, (float)y * scale);
                if (threshold > p * 0.5f + 0.5f) continue;
                if (fn_Random_RandFloat(rng) > density) continue;

                const int idx = dim * y + x;
                if (*(void**)(cells + (size_t)idx * CELL_STRIDE + CELL_OCCUPANT)) continue;  // occupied
                const int dy = y - spawnY, dx = x - spawnX;
                // Verification harness (env BANISHED_RES_IN_TOWN): allow resources to
                // scatter into the town site so trees land in front of the start camera
                // (normally they're excluded from the town disk → only visible at the
                // map periphery, out of the initial view). Default off = vanilla.
                static const bool s_noExclude = std::getenv("BANISHED_RES_IN_TOWN") != nullptr;
                if (!s_noExclude && dy * dy + dx * dx < sizeSq) continue;                      // town exclusion

                // Terrain height + slope from the heightmap sampler.
                const float center = (float)(dim >> 1) - kCenterBias;
                float in[3]  = { (float)(idx % dim) - center, (float)(idx / dim) - center, 0.0f };
                float pos[3] = {0,0,0}, tri[3] = {0,0,0};
                fn_MapGetTerrainPos(game, pos, in, 0, tri);
                const float h = pos[2], slope = tri[2];
                if (h < hMin || h >= hMax || slope < slopeMin) continue;

                // Instantiate, place its model at the tile (Foundation F), register.
                void* mem = fn_MemAlloc(g_pMemAllocSingleton, ENTITY_SIZE, 8);
                void* ent = mem ? fn_EntityFactory_CreateEntity(mem, tmpl) : nullptr;
                uintptr_t entIBP = 0;
                fn_InternalBasePtr_Ctor(&entIBP, ent);
                // Entity_impl_6540 — write the MapComponent grid cell (tileX/tileY,
                // w=1,h=1) AND the jittered ModelComponent world transform. This is the
                // canonical FUN_1400a12b0 per-cell placement: the grid fields let
                // economy/lookup code find the resource (priority #1), and the transform
                // (jitter from desc+0x60, random yaw*180) replaces the prior ad-hoc
                // SetEntityModelTransform. All writes are null-guarded → safe on inert
                // components. `idx` = the flat cell index passed to 6540.
                StartTown_PlaceResourceCell(ent, idx, game, rng, tidNRComp, tidMapComp, tidModel);

                // Growth-variant override (FUN_1400a12b0 tail). Pick the variant group
                // via per-variant Perlin (first to exceed threshold, else 0), with a
                // 1/denom random override, then a random model sub-variant in that
                // group. Without this trees stay at the invisible default (0,0) seed.
                if (doVariant && ent) {
                    int variantIdx = 0;
                    for (int vi = 0; vi < variantCount; ++vi) {
                        float v = fn_PerlinNoise_Generate(vPerlin[vi], (float)x * vScale, (float)y * vScale);
                        variantIdx = vi;
                        if (vThresh <= v * 0.5f + 0.5f) break;
                        variantIdx = 0;
                    }
                    if (fn_Random_Rand(rng, (uint32_t)vOverrideDenom) == 0)
                        variantIdx = (int)fn_Random_Rand(rng, (uint32_t)variantCount);
                    void* groupArray = *(void**)((uint8_t*)modelDesc + 0x20);
                    int subCount = groupArray
                        ? *(int32_t*)((uint8_t*)groupArray + (int64_t)variantIdx * 0x18 + 0x10) : 0;
                    int modelVariant = (subCount > 0) ? (int)fn_Random_Rand(rng, (uint32_t)subCount) : 0;
                    ApplyResourceModelVariant(ent, tidModel, (int16_t)variantIdx, (int16_t)modelVariant);
                }

                // FUN_140120db0 — growth/age + mesh-scale init on the GrowthComponent.
                // No-op on the default-inert GrowthComponent (siblings unlinked); when
                // BANISHED_LIVE_GROWTH is on it stages the tree at a random growth value.
                StartTown_GrowthInitResource(ent, tidGrowth, rng);

                fn_EntityList_RegisterEntity(entityList, &entIBP, category);
                if (ent) { ++spawned; ++poolSpawned; }
                if (poolSpawned >= poolCap) { capped = true; break; }
            }
        }
        fprintf(stderr, "[startTown::res] [%d] tmpl=%p desc=%p thr=%.3f dens=%.3f h[%.2f,%.2f) slope>=%.2f -> %d%s\n",
                e, tmpl, desc, threshold, density, hMin, hMax, slopeMin, poolSpawned,
                capped ? " (CAPPED)" : "");
        fflush(stderr);
    }
    return spawned;
}

// ============================================================================
// ENVIRONMENT pool — ComponentEntity_impl_02f0 (0x1400a02f0). Three sub-passes of
// ambient/environment entities (all cosmetic — audio emitters + ambient volumes):
//   1. Ambient EMITTERS spaced along the worldGeom path/edge list
//      (worldGeom+0x1e0 array, count +0x1e8; each entry {Vector2* pts, int n}).
//      Walks each polyline accumulating segment length; every time the running
//      length passes the spacing threshold (startConfig+0x130) it drops an emitter
//      (template = ExternalRef at startConfig+0x118). The original then positions
//      the AmbientEmitterComponent + moves its audio emitter (Audio::Scene::
//      MoveEmitter).
//   2. Ambient ENVIRONMENT volumes on a coarse chunk grid (stride =
//      startConfig+0x150) over the map (template = startConfig+0x138). The
//      original inits each via AmbientEnvironmentComponent_impl_36a0.
//   3. One global ENVIRONMENT entity (template = startConfig+0x158), positioned at
//      a height offset (startConfig+0x170) above the terrain.
//
// As with the other pools, only instantiate + register is done here; the
// component positioning / audio / init dispatch on real (non-inert) components and
// are GUARDED OFF. The entities exist + are registered. Defensive guards: null
// templates, chunk stride <= 0 (would infinite-loop the grid), and per-pass caps.
// ============================================================================

// Spawn one ambient-environment entity for a grid tile and stamp its biome
// region + emitter position BEFORE registering it. Mirrors the retail order in
// ComponentEntity_impl_02f0: instantiate -> AmbientEnvironmentComponent_impl_36a0
// (region/position) -> register (which runs OnAddedPass -> the emitter's OnAdded,
// seeding its 3D handles at the now-correct position). Falls back to a plain
// spawn+register when the env component isn't found (keeps the entity present).
static bool SpawnEnvVolumeAndRegister(void* tmpl, void* entityList,
                                      int gridX, int gridY, int chunk) {
    if (!tmpl) return false;
    if (!fn_EntityFactory_CreateEntity || !fn_EntityList_RegisterEntity ||
        !fn_InternalBasePtr_Ctor || !fn_MemAlloc) return false;
    void* mem = fn_MemAlloc(g_pMemAllocSingleton, ENTITY_SIZE, 8);
    void* ent = mem ? fn_EntityFactory_CreateEntity(mem, tmpl) : nullptr;
    if (!ent) return false;

    static uint32_t s_tidEnv = 0;
    if (s_tidEnv == 0) s_tidEnv = TypeId("AmbientEnvironmentComponent");
    void* envComp = FindComponentOnEntity(ent, s_tidEnv);
    if (envComp)
        AmbientEnvironmentComponent_InitRegion(
            reinterpret_cast<AmbientEnvironmentComponent*>(envComp),
            gridX, gridY, chunk, chunk);

    void* ibp = nullptr;
    fn_InternalBasePtr_Ctor(&ibp, ent);
    int category = *(int32_t*)((uint8_t*)tmpl + TEMPLATE_CATEGORY);
    fn_EntityList_RegisterEntity(entityList, &ibp, category);
    return true;
}

static int StartTown_SpawnEnvironment(uint8_t* startConfig, void* world) {
    void* entityList = *(void**)((uint8_t*)world + WORLD_ENTITYLIST);
    void* worldGeom  = *(void**)((uint8_t*)world + 0x110);
    void* game       = *(void**)((uint8_t*)world + 0xf8);
    if (!entityList) return 0;

    int n1 = 0, n2 = 0, n3 = 0;
    static constexpr int ENV_PASS_CAP = 2000;

    // Pass 1 — emitters along worldGeom polylines.
    void* emitterTmpl = StartTown_ResolveTemplate(startConfig + 0x118);
    const float spacing = *(float*)(startConfig + 0x130);
    if (emitterTmpl && worldGeom && spacing > 0.0f) {
        const int pathCount = *(int32_t*)((uint8_t*)worldGeom + 0x1e8);
        uint8_t* pathArr    = *(uint8_t**)((uint8_t*)worldGeom + 0x1e0);
        if (pathArr) {
            for (int i = 0; i < pathCount && n1 < ENV_PASS_CAP; ++i) {
                uint8_t* entry = pathArr + (size_t)i * 0x10;
                const float* pts = *(const float**)entry;
                const int ptCount = *(int32_t*)(entry + 8);
                if (!pts || ptCount < 2) continue;
                float accum = 0.0f;
                for (int j = 1; j < ptCount && n1 < ENV_PASS_CAP; ++j) {
                    const float dx = pts[2 * j]     - pts[2 * (j - 1)];
                    const float dy = pts[2 * j + 1] - pts[2 * (j - 1) + 1];
                    accum += sqrtf(dx * dx + dy * dy);
                    if (accum >= spacing) {
                        if (SpawnAndRegister(emitterTmpl, entityList)) ++n1;
                        accum = 0.0f;
                    }
                }
            }
        }
    }

    // Pass 2 — ambient environment volumes on a chunk grid. When ambient audio is
    // live, stamp each volume's biome-sample region + emitter position from its
    // tile (gridX=x, gridY=y, rw=rh=chunk) so biome sampling varies by location and
    // the emitters aren't all stuck at the world origin. Default (env off) keeps the
    // original plain spawn+register so the run stays byte-for-byte unchanged.
    void* ambTmpl = StartTown_ResolveTemplate(startConfig + 0x138);
    const int chunk = *(int32_t*)(startConfig + 0x150);
    const int dim   = game ? *(int32_t*)game : 0;
#ifdef STANDALONE_EXE
    const bool ambientLive = ::ComponentFactory::LiveEnabled("BANISHED_AMBIENT_AUDIO");
#else
    const bool ambientLive = false;
#endif
    if (ambTmpl && chunk > 0 && dim > 0) {
        for (int y = 0; y < dim && n2 < ENV_PASS_CAP; y += chunk)
            for (int x = 0; x < dim && n2 < ENV_PASS_CAP; x += chunk) {
                bool ok = ambientLive
                    ? SpawnEnvVolumeAndRegister(ambTmpl, entityList, x, y, chunk)
                    : SpawnAndRegister(ambTmpl, entityList);
                if (ok) ++n2;
            }
    }

    // Pass 3 — the single global environment entity.
    void* envTmpl = StartTown_ResolveTemplate(startConfig + 0x158);
    if (envTmpl && SpawnAndRegister(envTmpl, entityList)) ++n3;

    fprintf(stderr, "[startTown::env] emitters=%d ambient=%d(chunk=%d) global=%d "
            "(paths=%d, emitterTmpl=%p ambTmpl=%p envTmpl=%p)\n",
            n1, n2, chunk, n3,
            worldGeom ? *(int32_t*)((uint8_t*)worldGeom + 0x1e8) : -1,
            emitterTmpl, ambTmpl, envTmpl);
    fflush(stderr);
    return n1 + n2 + n3;
}

// Spawn entry point — called from Load after the camera is framed. Builds up the
// start-town entities: structures (impl_1fc0) + citizens (impl_0dd0) + resources
// (impl_a12b0) + environment (impl_02f0) + simple pool (impl_09e0).
extern "C" void StartTown_SpawnEntities(void* startConfig, void* world) {
    if (!startConfig || !world) return;
    uint8_t* sc = (uint8_t*)startConfig;
    void* entityList = *(void**)((uint8_t*)world + WORLD_ENTITYLIST);
    if (!entityList) { fprintf(stderr, "[startTown::spawn] entityList NULL — abort\n"); fflush(stderr); return; }

    // DIAG — dump every spawn pool's count so we know what's populated.
    fprintf(stderr, "[startTown::spawn] pools: simple(+0x98)=%d bldg(+0x88)=%d "
            "resource(+0x78)=%d citizenFam(+0x68)=%d\n",
            *(int32_t*)(sc + 0x98), *(int32_t*)(sc + 0x88),
            *(int32_t*)(sc + 0x78), *(int32_t*)(sc + 0x68));
    fflush(stderr);

    // DIAG — fn-pointer state + per-pool first-entry resolution probe.
    fprintf(stderr, "[startTown::spawn] fns: CreateEntity=%p Register=%p IBPCtor=%p MemAlloc=%p ExtFactory=%p\n",
            (void*)fn_EntityFactory_CreateEntity, (void*)fn_EntityList_RegisterEntity,
            (void*)fn_InternalBasePtr_Ctor, (void*)fn_MemAlloc,
            g_pExternalFactorySingleton ? *g_pExternalFactorySingleton : nullptr);
    // Probe: resolve the first entry of each pool + the mapGen _citizen ref
    // (+0x48, a known "Template\Citizen.rsc" reference). Dump the String text so
    // we can tell a deserialize bug (garbage text) from a missing-resource issue
    // (correct text but Find fails). System::String = ptr to char buffer.
    auto dumpStr = [](void* strField) -> const char* {
        char* p = *(char**)strField;
        return p ? p : "(null)";
    };
    struct { const char* tag; int off; } probes[] = {
        { "citizenRef(+0x48)", 0x48 },
        { "resource(+0x70)", 0x70 }, { "structure(+0x80)", 0x80 }, { "simple(+0x90)", 0x90 },
    };
    for (auto& p : probes) {
        uint8_t* base;
        if (p.off == 0x48) base = sc + 0x48;                    // inline ExternalRef
        else { base = *(uint8_t**)(sc + p.off);                 // array pool
               if (!base || *(int32_t*)(sc + p.off + 8) <= 0) { fprintf(stderr, "[startTown::probe] %s empty\n", p.tag); continue; } }
        ResolveExternalByName_Impl(base);
        fprintf(stderr, "[startTown::probe] %s gen=%d tmpl=%p name=\"%.40s\"\n",
                p.tag, *(int32_t*)(base + 8), *(void**)(base + 0x10), dumpStr(base));
    }
    fflush(stderr);

    // Recompute the start-town spawn tile (deterministic: same grid + spiral as
    // StartTown_ComputeCamera). The structures/citizens/resources are placed
    // relative to this tile, exactly as FUN_1400a3300 does with the tile it found
    // once and reused for both the camera and the spawn passes.
    void* game = *(void**)((uint8_t*)world + 0xf8);
    int spawnX = (game ? *(int32_t*)game : 0) >> 1, spawnY = spawnX;
    if (game) {
        int size = *(int32_t*)(sc + 0x3c);
        if (size <= 0) size = 1;
        StartTown_FindSpawnTile(game, size, &spawnX, &spawnY);
    }

    // EntityList main-array count lives at entityList+0x10 (Foundation B); log it
    // before/after to confirm registration actually adds entities to the world.
    int before = *(int32_t*)((uint8_t*)entityList + 0x10);

    // STRUCTURES (impl_1fc0) — the initial buildings.
    int ns = StartTown_SpawnStructures(sc, entityList, game, spawnX, spawnY);
    int afterStruct = *(int32_t*)((uint8_t*)entityList + 0x10);
    fprintf(stderr, "[startTown::spawn] structures: spawned %d; entityList count %d -> %d\n",
            ns, before, afterStruct);
    fflush(stderr);

    // PHASE A — real production workplace (ForesterLodge), env BANISHED_LIVE_WORKPLACE.
    // Spawn it before citizens so g_pForceWorkBuilding points at a real producer (not
    // the StorageBarn) when the BANISHED_FORCE_WORK harness assigns a citizen.
    StartTown_SpawnWorkplace(sc, entityList, game, spawnX, spawnY);

    // CITIZENS (impl_0dd0) — the founding families.
    int nc = StartTown_SpawnCitizens(sc, entityList, game, spawnX, spawnY);
    int afterCitizens = *(int32_t*)((uint8_t*)entityList + 0x10);
    fprintf(stderr, "[startTown::spawn] citizens: spawned %d; entityList count %d -> %d\n",
            nc, afterStruct, afterCitizens);
    fflush(stderr);

    // Seed the default Laborer profession on founding adults (ungated) so the citizen
    // info panel's Job line renders "Laborer" instead of blank. Runs after citizens +
    // founding housing (g_foundingCitizens) exist and ProfessionList (gw+0xb0) is loaded.
    StartTown_SeedLaborerProfessions();

    // Town-manager registry (ungated) — register the founding citizens into
    // townMgr+0xa0 and the founding houses into townMgr+0x20, the two lists the
    // town-hall statistics dialog reads (was: General tab all zeros).
    StartTown_RegisterTownCitizens();
    StartTown_RegisterTownResidences();

    // PHASE 4 ASSIGN half — seat laborers at the lodge (env BANISHED_LIVE_WORKPLACE) now
    // that citizens + founding housing exist. Sets citizen+0x238 -> Gather/Work candidates.
    StartTown_EmployForesters();

    // RESOURCES (impl_a12b0) — perlin scatter of trees/stone/iron across the map,
    // excluding the town site (radius = townSize, sizeSq = size*size).
    int resSize = *(int32_t*)(sc + 0x3c);
    if (resSize <= 0) resSize = 1;
    int nr = StartTown_SpawnResources(sc, world, spawnX, spawnY, resSize * resSize);
    int afterRes = *(int32_t*)((uint8_t*)entityList + 0x10);
    fprintf(stderr, "[startTown::spawn] resources: spawned %d; entityList count %d -> %d\n",
            nr, afterCitizens, afterRes);
    fflush(stderr);

    // ENVIRONMENT (impl_02f0) — ambient emitters + environment volumes.
    int ne = StartTown_SpawnEnvironment(sc, world);
    int afterEnv = *(int32_t*)((uint8_t*)entityList + 0x10);
    fprintf(stderr, "[startTown::spawn] environment: spawned %d; entityList count %d -> %d\n",
            ne, afterRes, afterEnv);
    fflush(stderr);

    // SIMPLE pool (impl_09e0) — population/deer.
    int n = StartTown_SpawnSimplePool(sc, entityList);
    int after = *(int32_t*)((uint8_t*)entityList + 0x10);
    fprintf(stderr, "[startTown::spawn] simple pool: spawned %d entities; "
            "entityList count %d -> %d\n", n, afterEnv, after);
    fflush(stderr);

    // ========================================================================
    // Brick M-D (BANISHED_LIVE_STORAGE) — starting-resource seeding. Mirrors
    // the tail of FUN_1400a3300 (after all spawn passes):
    //   1. 0x1400a33b4 loop: register the raw-material TEMPLATE pool
    //      (startConfig+0xa0, count +0xa8, stride 0x18 ExternalRef) into
    //      townMgr+0x110 via FUN_140091e60 (dedup-append; already ported +
    //      registered as RawMaterialComponent_RegisterRawMaterial).
    //   2. 0x1400a342b: Game_impl_1f00(startConfig) — seed the seven start
    //      resources (wood/stone/iron/fuel/FOOD/herb/tool) into the registered
    //      storage locations (barn/yard, wired in StartTown_WireStorage above).
    // ========================================================================
    if (LiveStorageEnabled()) {
        void* townMgr = nullptr;
        if (g_pGameWorldSingleton && *g_pGameWorldSingleton)
            townMgr = *(void**)((uint8_t*)*g_pGameWorldSingleton + 0xf0);
        typedef void (*fn_RegisterRawMat_t)(void* townMgr, void* templateEntity);
        fn_RegisterRawMat_t regRawMat = reinterpret_cast<fn_RegisterRawMat_t>(
            ResolveAddr(Module::Exe, 0x140091e60));

        const int rmCount = *(int32_t*)(sc + 0xa8);
        uint8_t* rmPool   = *(uint8_t**)(sc + 0xa0);
        int registered = 0;
        if (townMgr && regRawMat && rmPool) {
            for (int i = 0; i < rmCount; ++i) {
                // Retail resolves a copy of the ExternalRef by name (FUN_14001b1d0)
                // and registers the resolved template; unresolved entries are
                // skipped here (standalone guard) instead of appending NULL.
                void* tmpl = StartTown_ResolveTemplate(rmPool + (size_t)i * POOLENTRY_STRIDE);
                if (!tmpl) {
                    fprintf(stderr, "[StorageSeed] rawmat template[%d] unresolved\n", i);
                    fflush(stderr);
                    continue;
                }
                regRawMat(townMgr, tmpl);
                ++registered;
            }
        }
        if (StorageTraceOn()) {
            fprintf(stderr, "[StorageSeed] rawmat templates: pool=%d registered=%d "
                    "townMgr=%p (+0x118 count=%d) storageLocs(+0x08)=%d\n",
                    rmCount, registered, townMgr,
                    townMgr ? *(int32_t*)((uint8_t*)townMgr + 0x118) : -1,
                    townMgr ? *(int32_t*)((uint8_t*)townMgr + 0x08) : -1);
            fflush(stderr);
        }

        // Game_impl_1f00 — the seven-mask seeding driver (counts at
        // startConfig+0xfc..+0x114).
        StartTown_SeedStartingResources(sc);

        // Founding-house household inventory (BANISHED_LIVE_HOUSING): now that the town
        // raw-material TEMPLATE pool (townMgr+0x110) is populated, seed a small edible-food
        // stock into each founding WoodHouse's StorageComponent so a clicked house shows a
        // non-empty inventory. Must run here (post-seed), not in the housing pass (which runs
        // before the template pool exists).
        StartTown_SeedHouseInventory(8);

        // Final state dump (townMgr+0x00 count, per-location capacity/slots).
        TownStorage_TraceDump();
    }
}
