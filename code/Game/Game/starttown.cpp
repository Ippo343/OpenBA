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
#include "Component/storagecomponent_funcs.h"   // StorageComponent_Draw (citizen carry-capacity init)
#include "Component/component_factory_base.h"   // ComponentFactory::LiveEnabled
#include "Component/spawncomponent_funcs.h"     // SpawnComponent_InitialSpawn (5cc0 seed)
extern "C" void MapComponent_ReportDc20Evictions(const char* tag);  // keystone_a_map.cpp
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <functional>

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
// Feature-path channel widths, captured by worldgen.cpp from the 0x1C-stride SOURCE
// PathPoints (+0x14) before the copy into wg+0x1e0 narrows them to (x,y). Both return
// -1.0f when there is no data for that path index. BankDistance = halfWidth + 2.0 =
// tiles from centerline to dry land, which is what a dock placement needs.
extern "C" float WorldGen_GetPathHalfWidth(int pathIndex);
extern "C" float WorldGen_GetPathBankDistance(int pathIndex);
extern "C" void TownStorage_TraceDump();

static bool LiveStorageEnabled() {
    static int v = -1;
    if (v < 0) {
        v = ::ComponentFactory::LiveEnabled("BANISHED_LIVE_STORAGE") ? 1 : 0;
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

// (defined below; needed here to arm the MapComponent removal-clear inside MarkBuildingCells)
static uint32_t TypeId(const char* name);
static void* FindComponentOnEntity(void* entity, uint32_t typeId);

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
// Forward decl (defined near the resource-marking pass below).
static void StartTown_WriteCellType(uint8_t* game, uint8_t* cells, int idx, uint8_t newType);

static void StartTown_MarkBuildingCells(void* game, int tileX, int tileY, int fpW, int fpH,
                                        uintptr_t entIBP, int rot) {
    if (!StartTown_PickMapEnabled() || !game || !entIBP) return;
    uint8_t* cells = *(uint8_t**)((uint8_t*)game + 0x80);
    int dim = *(int32_t*)game;
    if (!cells || dim <= 0) return;

    // Faithful dc20 (FUN_14009dc20) per-cell footprint, replacing the old blanket
    // fpW x fpH mark. The MapDescription carries a per-cell character map:
    //   '.' = free ground   (cell+0x08 cleared, terrain class untouched)
    //   '#' = obstructed    (cell+0x08 = entity, cell+0x29 = desc+0x20 class)
    //   '_' = road strip    (cell+0x08 = entity, cell+0x29 = 2 = road class)
    // Blanket-marking the whole rect stamped the '.'/'_' cells in FRONT of every
    // start-town building as building-occupied, which BuildRoad_Execute (cell+0x08
    // read at buildroad.cpp ~:445) refuses forever — road segments across house
    // fronts never built. The caller chain is MapComponent::Draw -> sub_41b0 -> dc20:
    //   desc+0x20 = terrain class for '#' cells, desc+0x28 = the char-map String,
    //   desc+0x31 = skip-occupancy flag, rot from MapComponent+0x30.
    const uint8_t* desc = nullptr;
    {
        void* mapComp = FindComponentOnEntity((void*)entIBP, TypeId("MapComponent"));
        if (mapComp) desc = *(const uint8_t**)((const uint8_t*)mapComp + 0x18);
        // (No IsDerivedFrom validation: standalone's partial factory tree makes it a
        // false-negative landmine — same rationale as the resource path at ~L3100.)
    }
    if (desc && desc[0x31] != 0) return;   // 41b0: description opts out of occupancy

    const char* shape = desc ? *(const char* const*)(desc + 0x28) : nullptr;
    const int shapeLen = shape ? *(const int*)(shape - 8) : 0;   // length-prefixed String
    const uint8_t classByte = desc ? desc[0x20] : 0;

    // Unrotated string-grid dims (the .rsc footprint is authored unrotated; the
    // spawn already axis-swapped fpW/fpH for 90/270).
    const bool swapped = (rot == 0x5a || rot == 0x10e);
    const int w0 = swapped ? fpH : fpW;
    const int h0 = swapped ? fpW : fpH;

    uintptr_t nullIBP = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&nullIBP, nullptr);

    int marked = 0, freed = 0, roadCells = 0;
    for (int iy = 0; iy < fpH; ++iy)
        for (int ix = 0; ix < fpW; ++ix) {
            int cx = tileX + ix, cy = tileY + iy;
            if (cx < 0 || cx >= dim || cy < 0 || cy >= dim) continue;
            int idx = dim * cy + cx;
            uint8_t* cell = cells + (size_t)idx * 0x30;

            // dc20's char lookup (row stride w0+1, rows authored top-first).
            bool stampClass = true;          // '#'/'_' or no shape -> class stamp
            bool occupy     = true;          // anything but '.' -> entity in +0x08
            uint8_t cls     = classByte;
            if (shape && shapeLen != 0) {
                int gx, gy;
                if (rot == 0x5a)       { gy = ix;            gx = iy; }
                else if (rot == 0xb4)  { gy = iy;            gx = (w0 - 1) - ix; }
                else if (rot == 0x10e) { gy = (h0 - 1) - ix; gx = (w0 - 1) - iy; }
                else                   { gy = (h0 - 1) - iy; gx = ix; }
                const long sidx = (long)gy * (w0 + 1) + gx;
                const char c = (sidx >= 0 && sidx < shapeLen) ? shape[sidx] : 0;
                stampClass = (c == '#' || c == '_');
                occupy     = (c != '.');
                if (c == '_') cls = 2;       // road strip
                // Mirror of dc20's BANISHED_LIVE_WALKWAY_DROPCELLS deviation — this path
                // marks START-TOWN and demo-spawned buildings, so honouring it only in
                // dc20 would leave those buildings' walkway cells claimed anyway.
                if (c == '_') {
                    static int s_fw = -1;
                    if (s_fw < 0)
                        s_fw = ::ComponentFactory::LiveEnabled(
                                   "BANISHED_LIVE_WALKWAY_DROPCELLS") ? 1 : 0;
                    if (s_fw) occupy = false;
                }
            }

            if (stampClass) StartTown_WriteCellType((uint8_t*)game, cells, idx, cls);
            *(uintptr_t*)(cell + 0x08) = occupy ? entIBP : nullIBP;
            if (occupy) marked++; else freed++;
            if (stampClass && cls == 2) roadCells++;
        }

    // ARM THE REMOVAL-SIDE CLEAR (M-B parity with the resource path @ ~L2684).
    // We just wrote the building entity into cell+0x08 for click-picking, bypassing
    // the (inert) MapComponent. On removal, FUN_1400c2ea0 broadcasts vtable+0x38 to
    // the entity's removable components -> MapComponent_DetachVSlot ->
    // MapComponent_DetachOccupancy_4750, which nulls cell+0x08 over the +0x28..+0x2e
    // footprint but is GATED on the "occupancy-written" bit 0x100000 of comp+0x20.
    // Placement already stamps the footprint tile fields, but nothing set that bit for
    // buildings, so the detach no-oped and the upgraded/demolished building's cell+0x08
    // kept pointing at the FREED entity -> the CitizenAI leisure box scan (and forester
    // scan) faulted on it (FloorComponent_FindResData READ of freed compArray, crash
    // 2026-07-21). Record the footprint we actually marked and set the bit here, so the
    // invariant "cell+0x08 written <=> bit set" holds exactly, same as the resource path.
    void* mapComp = FindComponentOnEntity((void*)entIBP, TypeId("MapComponent"));
    if (mapComp) {
        uint8_t* mc = reinterpret_cast<uint8_t*>(mapComp);
        *(int16_t*)(mc + 0x28) = (int16_t)tileX;
        *(int16_t*)(mc + 0x2a) = (int16_t)tileY;
        *(int16_t*)(mc + 0x2c) = (int16_t)fpW;
        *(int16_t*)(mc + 0x2e) = (int16_t)fpH;
        *(uint32_t*)(mc + 0x20) |= 0x100000u;   // detach-clear armed
    }

    fprintf(stderr, "[startTown::pickmap] marked %d cells (%d free, %d road) @ (%d,%d) %dx%d rot=%d "
            "shape=%s entity=%p mc=%p\n",
            marked, freed, roadCells, tileX, tileY, fpW, fpH, rot,
            (shape && shapeLen) ? "yes" : "NONE(blanket)", (void*)entIBP, mapComp);
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

// World-registration for a building entity created at RUNTIME rather than by the
// start-town spawn — today the upgrade replacement (DestroyComponent state-5 tick
// 0x14011c120 clones the target template and RegisterEntity's it).
//
// Retail gets both halves for free during RegisterEntity: the add-broadcast
// (FUN_1400c2f60) calls MapComponent's vtable+0x30 (Draw -> 41b0 -> dc20, the cell
// occupancy write) and PickingComponent's (OnAdded, the pick-mesh registration).
// Standalone's MapComponent slot 6 only paints the minimap and its PickingComponent
// registers no mesh, so both are collapsed into the spawn sites — this is the entry
// point for the runtime ones. Reads the footprint the caller already stamped into the
// entity's MapComponent (+0x28 x, +0x2a y, +0x2c w, +0x2e h).
//
// Consequences of skipping it (observed): the new building is unclickable (EditTool's
// pick finds nothing in the region-map cell array), does not block navigation, and its
// removal-side cell clear stays disarmed (bit 0x100000).
extern "C" void StartTown_RegisterRuntimeBuilding(void* entity) {
    if (!entity || !g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    void* game = *(void**)((uint8_t*)*g_pGameWorldSingleton + 0xf8);
    void* mapComp = FindComponentOnEntity(entity, TypeId("MapComponent"));
    if (!game || !mapComp) {
        fprintf(stderr, "[Upgrade] RegisterRuntimeBuilding: SKIP entity=%p game=%p mapComp=%p\n",
                entity, game, mapComp);
        fflush(stderr);
        return;
    }
    uint8_t* mc = (uint8_t*)mapComp;
    int tileX = *(int16_t*)(mc + 0x28), tileY = *(int16_t*)(mc + 0x2a);
    int fpW   = *(int16_t*)(mc + 0x2c), fpH   = *(int16_t*)(mc + 0x2e);
    int rot   = *(int32_t*)(mc + 0x30);
    if (fpW <= 0 || fpH <= 0) {
        fprintf(stderr, "[Upgrade] RegisterRuntimeBuilding: entity=%p has no footprint "
                "(%d,%d %dx%d) — cells not marked\n", entity, tileX, tileY, fpW, fpH);
        fflush(stderr);
        return;
    }
    StartTown_MarkBuildingCells(game, tileX, tileY, fpW, fpH, (uintptr_t)entity, rot);
    StartTown_PickMeshProbe(entity);
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
// workplacecomponent.cpp — true when the WPC live controller (BANISHED_LIVE_WPC_CONTROLLER)
// is enabled; its state-2 ENTER (edf0) then registers workplaces through the SM itself.
extern "C" bool WorkPlaceComponent_ControllerLive();

static uint32_t TypeId(const char* name);
static void* FindComponentOnEntity(void* entity, uint32_t typeId);

// ---------------------------------------------------------------------------
// Shared "activate a pre-built workplace" replay (open E consolidation — was two
// near-identical inline blocks, one in StartTown_SpawnStructures and one in the
// ForesterLodge spawner). A spawned-already-complete building skipped both retail
// seatings: the data-load serialize that seats WPC+0x48 = WorkPlaceDescription+0x20,
// and the construction-time f520 activation that SetAccepting(true)s the site. Replay
// both for any employer (desc carries a profession; houses/storage keep key 0 and are
// skipped — the profession BST is the EMPLOYER list, houses live in townMgr+0x20).
//
// REGISTRATION into the townMgr+0x100 profession BST is the WPC controller's state-2
// ENTER (edf0) and runs by itself on the first SM tick when the live controller is
// enabled (start-town structures are stamped +0x52 = 2). When the controller is off —
// or the caller must QUERY the BST before the first tick (the ForesterLodge employ
// pass runs FindAvailableBuilding during StartTown) — register here too; the insert
// (impl_95a0) dedups, so both paths running is harmless.
// Returns the seated profession key (0 = not an employer, nothing done).
static uint64_t StartTown_ActivateWorkplace(void* entity, bool registerNow,
                                            const char* logTag) {
    if (!entity || !g_pGameWorldSingleton || !*g_pGameWorldSingleton) return 0;
    void* wpc = FindComponentOnEntity(entity, TypeId("WorkPlaceComponent"));
    if (!wpc) return 0;
    int64_t townMgr = *(int64_t*)((uint8_t*)*g_pGameWorldSingleton + 0xf0);
    uint8_t* w = reinterpret_cast<uint8_t*>(wpc);
    void* wpDesc = *reinterpret_cast<void**>(w + 0x18);
    uint64_t profKey = wpDesc
        ? *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(wpDesc) + 0x20) : 0;
    if (!townMgr || profKey == 0) return 0;

    *reinterpret_cast<uint64_t*>(w + 0x48) = profKey;
    WorkPlaceComponent_SetAccepting(reinterpret_cast<int64_t>(wpc), true);
    const bool doRegister = registerNow || !WorkPlaceComponent_ControllerLive();
    if (doRegister) TownRegistry_RegisterWorkplace(townMgr, wpc);
    fprintf(stderr, "[%s] workplace activate: WPC=%p profKey(desc+0x20)=0x%llx -> seated "
            "+0x48, accepting=1, %s\n", logTag, wpc, (unsigned long long)profKey,
            doRegister ? "registered into townMgr+0x100"
                       : "registration deferred to the state-2 enter (ctrl live)");
    fflush(stderr);
    return profKey;
}

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

// ============================================================================
// AGRICULTURE AVAILABILITY — the tail of FUN_1400a3300 (after Game_impl_1f00).
//
// The Town Hall "Agri" tab greys every crop/animal icon whose building
// ComponentDescription is NOT in the town-manager's available-agriculture list
// (townMgr(gw+0xf0)+0x120, count +0x128, cap +0x12c). TownHallUI::UpdateAgriculture
// (FUN_14007c220) only brightens an icon (SetColor via label vtable+0x50) when the
// description is found there; otherwise it stays the dark .rsc default. Retail
// seeds this list at start-town from the scenario start-condition's three agri
// pools (a RANDOM subset of each, dedup-appended), and later adds to it on trade
// purchases (FUN_140081420). Our start-town ported the sibling raw-material pool
// (+0x110 via 0x140091e60) but skipped these three loops, so +0x120 stayed empty
// and every icon rendered greyed.
//
// The three pools, with their REAL identities (the earlier labels here were shifted
// by one, which is how the starting herd went missing for so long — see below):
//
//   plants   : pool ptr sc+0xb0, pool size sc+0xb8, grant count sc+0xe0 (_plantCount)
//   trees    : pool ptr sc+0xc0, pool size sc+0xc8, grant count sc+0xe4 (_treeCount)
//   LIVESTOCK: pool ptr sc+0xd0, pool size sc+0xd8, grant count sc+0xe8 (_livestockCount)
//
// Confirmed against ToolKit/resource/Game/StartCondition/Easy.rsc, whose fields appear
// in exactly this offset order (_plantCount, _treeCount, _livestockCount,
// _livestockOffsetX/Y, _livestockRadius, _livestockGroupCount -> 0xe0..0xf8), and against
// a live run: pool sizes a8=7 b8=8 c8=8 d8=3, where 3 == len(_initialLivestockList)
// [LivestockChicken, LivestockCow, LivestockSheep].
//
// Each pool entry is the standard 0x18-stride ExternalRef; resolve it to the
// crop/animal ComponentDescription (the SAME cached External that
// InitAgriculture's ExternalFactory::FindNext keys the agri BST on, so the
// pointers match).
//
// The THIRD loop additionally calls FUN_1400a0b10 before the availability add
// (1400a35dc, then 1400a35e7). A previous comment here described that call as spawning
// "the initial orchard resource" and omitted it as unrelated to tab greying. Both halves
// of that were wrong: the pool is livestock, not orchards, and the call is the town's
// STARTING HERD — the primary source of a new town's animals. It is ported as
// StartTown_SpawnStartingHerd (code/Game/Component/pasturecomponent_spawn.cpp) and is
// now called below.

// The starting-herd spawn (retail FUN_1400a0b10), ported alongside the other animal-spawn
// bodies in code/Game/Component/pasturecomponent_spawn.cpp.
extern "C" int StartTown_SpawnStartingHerd(void* startConfig, const float* centre, void* tmpl);

// Reimplementation of 0x140091a20 ("Game_RemoveEntity" is a Ghidra mislabel — the
// body is an add-if-not-present into the +0x120 dynamic array). Kept inline because
// standalone cannot ResolveAddr an unported exe address.
// Not static: the trade checkout (code/Game/Dialog/tradeui_checkout.cpp, the
// FUN_140081420 named above) is the other caller that grows this list.
void TownMgr_AddAvailableAgri(uint8_t* townMgr, void* desc) {
    if (!townMgr || !desc || !fn_MemAlloc || !g_pMemAllocSingleton) return;
    void**  arr   = *(void***)(townMgr + 0x120);
    int     count = *(int32_t*)(townMgr + 0x128);
    // Dedup: already present -> nothing to do.
    for (int i = 0; i < count; ++i)
        if (arr && arr[i] == desc) return;

    int newCount = count + 1;
    void** oldArr = arr;
    *(int32_t*)(townMgr + 0x128) = newCount;
    if (*(int32_t*)(townMgr + 0x12c) < newCount) {
        int newCap = newCount;
        if (count != 0) newCap = newCount * 3;
        *(int32_t*)(townMgr + 0x12c) = newCap;
        void* nb = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap << 3, 8);
        *(void***)(townMgr + 0x120) = reinterpret_cast<void**>(nb);
    }
    void** dst = *(void***)(townMgr + 0x120);
    int copyStart = (oldArr == dst) ? count : 0;
    for (int i = copyStart; i < newCount; ++i) {
        if (i < count) dst[i] = oldArr[i];
        else           dst[i] = desc;
    }
    if (oldArr != dst && oldArr && fn_MemFreeStatic) fn_MemFreeStatic(oldArr);
}

static bool AgriTraceOn() {
    static int s = -1;
    if (s < 0) { const char* e = std::getenv("BANISHED_AGRI_TRACE"); s = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return s != 0;
}

// Seed one agri pool: pick `count` random entries from the `size`-entry pool and
// dedup-append each resolved description into townMgr+0x120. Faithful to the retail
// loop (System::Random::Rand(worldRng, size) per pick).
// `spawnCentre` non-null marks the LIVESTOCK pool: retail spawns the starting herd from
// the picked template before the availability add. The other two pools pass nullptr and
// register availability only, exactly as retail does.
static int SeedAgriPool(uint8_t* sc, uint8_t* townMgr, void* rng,
                        int poolOff, int sizeOff, int countOff, const char* label,
                        const float* spawnCentre = nullptr) {
    uint8_t* pool  = *(uint8_t**)(sc + poolOff);
    int      size  = *(int32_t*)(sc + sizeOff);
    int      count = *(int32_t*)(sc + countOff);
    if (AgriTraceOn())
        fprintf(stderr, "[AgriSeed] %s: pool=%p size=%d grant=%d\n", label, pool, size, count);
    if (!pool || size <= 0 || count <= 0) return 0;
    int added = 0;
    for (int i = 0; i < count; ++i) {
        uint32_t idx = (rng && fn_Random_Rand) ? fn_Random_Rand(rng, (uint32_t)size)
                                               : (uint32_t)(i % size);
        uint8_t* entry = pool + (size_t)idx * POOLENTRY_STRIDE;
        void* tmpl = StartTown_ResolveTemplate(entry);
        if (AgriTraceOn())
            fprintf(stderr, "[AgriSeed]   %s[%d] idx=%u tmpl=%p\n", label, i, idx, tmpl);
        if (!tmpl) continue;
        // Retail order: spawn FIRST (0x1400a35dc), then the availability add
        // (0x1400a35e7). Both consume the same resolved template.
        if (spawnCentre) StartTown_SpawnStartingHerd(sc, spawnCentre, tmpl);
        TownMgr_AddAvailableAgri(townMgr, tmpl);
        ++added;
    }
    return added;
}

// Ungated: registers the town's starting available crops/livestock/orchards into
// townMgr+0x120 so the Town Hall Agri tab brightens them (instead of all-greyed).
// Independent of live storage; only needs townMgr + the resolved templates.
static void StartTown_SeedAvailableAgriculture(uint8_t* sc, void* game,
                                               int spawnX, int spawnY) {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    uint8_t* townMgr = *(uint8_t**)(gw + 0xf0);
    void*    rng     = *(void**)(gw + 0xd8);
    if (!townMgr) return;

    // Town-centre world position — retail passes the same Vector3 it computed once for
    // the camera and every other spawn pass (it is FUN_1400a3300's own return value,
    // held in R13 across the loop).
    float centre[3] = { 0.0f, 0.0f, 0.0f };
    bool haveCentre = false;
    if (game && fn_MapGetTerrainPos) {
        StartTown_TileToWorld(game, spawnX, spawnY, centre);
        haveCentre = true;
    }

    int before = *(int32_t*)(townMgr + 0x128);
    int p = SeedAgriPool(sc, townMgr, rng, 0xb0, 0xb8, 0xe0, "plants");
    int t = SeedAgriPool(sc, townMgr, rng, 0xc0, 0xc8, 0xe4, "trees");
    int a = SeedAgriPool(sc, townMgr, rng, 0xd0, 0xd8, 0xe8, "livestock",
                         haveCentre ? centre : nullptr);
    if (!haveCentre)
        fprintf(stderr, "[AgriSeed] no town centre (game=%p MapGetTerrainPos=%p) — "
                "starting herd SKIPPED\n", game, (void*)fn_MapGetTerrainPos);
    fprintf(stderr, "[AgriSeed] available-agriculture seeded: plants=%d trees=%d "
            "livestock=%d; townMgr+0x120 count %d -> %d\n",
            p, t, a, before, *(int32_t*)(townMgr + 0x128));
    fflush(stderr);
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
            // BANISHED_LIVE_RESIDENCE_REGISTRY — GRADUATED to kDefaultOnFlags 2026-08-15
            // (`=0` is now the kill-switch). See the A/B in component_factory_base.cpp:
            // this one erased list silently disabled every move-out, and therefore every
            // second-generation marriage, in the whole project.
            //
            // Slot 7 (vtable +0x38, "ResidenceComponent::OnCreate") does TWO things:
            //   1. ResidenceComponent_helper_9ef0 (0x140129ef0) — init/evict. Wanted.
            //   2. exe 0x140002cc0 on townMgr+0x20 — which is ERASE-BY-VALUE, not register
            //      (the Ghidra OnCreate/OnRemove mislabel; workplacecomponent.cpp:61-66
            //      already corrected the identical misnaming in its own copy). Unwanted.
            //
            // Measured 2026-07-31, seven houses, seven times: the residence controller's
            // state-2 enter appends the house to townMgr+0x20 (count 0->1, APPENDED OK),
            // and this call erases it again (count back to 0). No guard ever fired — the
            // registry was being populated correctly and then cancelled a moment later.
            //
            // ⚠ The comment that used to sit here said townMgr+0x20 "is never populated in
            // standalone". That was TRUE WHEN WRITTEN — nothing populated the list, so the
            // erase was harmless. The state-2 enter landed 2026-07-26 and started populating
            // it, at which point this call silently became the thing that undoes it. Nobody
            // re-read the comment because it still SOUNDED right. See
            // reference_condition_shaped_comments — a comment stating a current condition
            // rather than an invariant becomes a lie and disguises the gap it creates.
            //
            // Flag ON: call the init half directly, skipping the erase. Flag off: byte-identical.
            // Read through LiveEnabled, NOT raw getenv: kDefaultOnFlags is consulted ONLY
            // inside ComponentFactory::LiveEnabled, so a raw getenv here would make the
            // table entry a silent no-op — unset would stay OFF while `=0` still worked,
            // i.e. it would look graduated, pass the kill-switch test, and not be on.
            static int s_residenceRegistry = -1;
            if (s_residenceRegistry < 0)
                s_residenceRegistry =
                    ::ComponentFactory::LiveEnabled("BANISHED_LIVE_RESIDENCE_REGISTRY") ? 1 : 0;
            if (s_residenceRegistry) {
                typedef void (*fn_ResidenceInit_t)(void*);
                uintptr_t initFn = ResolveAddr(Module::Exe, 0x140129ef0);
                if (initFn) {
                    (*(fn_ResidenceInit_t)initFn)(comp);
                } else {
                    fprintf(stderr, "[startTown::housing] RESIDENCE_REGISTRY: 0x140129ef0 "
                            "unresolved — falling back to slot 7 (erase still happens)\n");
                    fflush(stderr);
                    typedef void (*fn_OnCreate_t)(void*);
                    (*(fn_OnCreate_t*)(vt + 0x38))(comp);
                }
            } else {
                typedef void (*fn_OnCreate_t)(void*);
                (*(fn_OnCreate_t*)(vt + 0x38))(comp);            // ResidenceComponent::OnCreate
            }
            // Capture the house's owner-entity IBP (ResidenceComponent+0x10 — the exact
            // value the original registration uses) for the founding-housing pass. We keep
            // the house IBPs here and pair them to families directly; that pairing is
            // independent of townMgr+0x20 and stays correct in both flag states.
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
//   2. town storage-location registration (d230): append the entity to
//      townMgr+0x00 so the citizen eat chain (CitizenAI_helper_e370) and
//      Game_SpawnResources can find it. In retail d230 is the ENTER handler of
//      StorageLocationComponent state 2 — not an OnAdded slot (verified
//      2026-07-27: 0x14012d230 is in no vtable). Calling it here is a stand-in for
//      an SM that never reaches state 2 in this build.
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
    // RETIRED when the StorageLocationComponent controller registers by itself
    // (2026-07-27): its state-2 ENTER *is* d230, so entering state 2 registers the
    // building. Measured both ways: with the controller live and this call still in
    // place every storage building registered TWICE (townMgr+0x00 count 2 -> 4) —
    // retail's d230 does not dedup, so the stand-in has to go, not merely be tolerated.
    //
    // TIMING IS PART OF THE CONTRACT, and it is why SM_ONADDED is in this condition.
    // StartTown_SeedStartingResources -> Game_SpawnResources round-robins the starting
    // stock over the entities in townMgr+0x00, so the registration must already have
    // happened when it runs. With LAZY linking the SM first ticks a frame later:
    // measured storageLocs=0 at seed time and placed=0 for all seven masks — a town
    // whose barn starts empty. With BANISHED_LIVE_SM_ONADDED the chain is linked in
    // slot 4 and ticked in slot 6 inside RegisterEntity, i.e. still inside the spawn
    // loop, which is the retail ordering. So: drop the stand-in only when the live
    // controller can actually beat the seeder to it.
    const bool storageCtrlLive =
        ::ComponentFactory::LiveEnabled("BANISHED_LIVE_STORAGECTRL") &&
        ::ComponentFactory::LiveEnabled("BANISHED_LIVE_STATEMACHINE");
    const bool ctrlRegisters =
        storageCtrlLive && ::ComponentFactory::LiveEnabled("BANISHED_LIVE_SM_ONADDED");
    // The half-configuration double-registers and there is no faithful way to stop it
    // (retail's d230 has no dedup): the stand-in below has to run to beat the seeder,
    // and then the lazily-linked chain enters state 2 a frame later and appends the
    // same entity again. STORAGECTRL must be paired with SM_ONADDED — say so loudly
    // rather than let a duplicated town storage list look like a search bug later.
    if (storageCtrlLive && !ctrlRegisters) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            fprintf(stderr, "[StorageSeed] WARN: BANISHED_LIVE_STORAGECTRL without "
                            "BANISHED_LIVE_SM_ONADDED — every storage building will "
                            "register TWICE in townMgr+0x00 (stand-in now, state-2 "
                            "enter on the first tick). Set BANISHED_LIVE_SM_ONADDED=1.\n");
            fflush(stderr);
        }
    }
    if (slc && !ctrlRegisters) TownRegistry_RegisterStorageLocation(slc);
    if (StorageTraceOn()) {
        uint8_t* sc = (uint8_t*)storageComp;
        fprintf(stderr, "[StorageSeed] struct[%d] entity=%p storage=%p slc=%p "
                "cap=%d mask=0x%x\n", entryIdx, entity, storageComp, slc,
                *(int32_t*)(sc + 0x38), *(uint32_t*)(sc + 0x3c));
        fflush(stderr);
    }
}

// NOTE: the resource path now uses StartTown_PlaceResourceCell (the Entity_impl_6540
// port: MapComponent grid cell + jittered ModelComponent transform). The structures
// path sets its ModelComponent transform inline (see StartTown_SpawnStructures).

// Defined further down (resource model-variant section); used by the structures path
// to swap a building from its foundation mesh to the completed-build mesh.
static void ApplyResourceModelVariant(void* entity, uint32_t tidModel,
                                      int16_t variantIdx, int16_t modelVariant);

// ── Construction-demo foundation (BANISHED_LIVE_BUILDCTRL) ────────────────────────
// When the live BuildComponent state machine is on we leave exactly ONE start-town
// building at its foundation mesh (skip the force-complete below) and record its
// BuildComponent as g_pConstructionDemoSite. State1Enter then drives ONLY that site
// through gather→build→complete, so no finished building's storage gets clobbered.
// Index selectable via BANISHED_BUILD_DEMO_INDEX (default 0 = first structure).
extern "C" void* g_pConstructionDemoSite;
extern "C" void* g_pConstructionDemoEntity;   // the demo foundation's OWNER entity (force-harness work-area)
// Default-ON via kDefaultOnFlags (graduated 2026-07-22); env var is a kill-switch (=0).
// NOTE: this only enables the demo-foundation CAPABILITY — the actual "leave a start-town
// building unbuilt" behaviour still requires the opt-in test sub-gates BuildDemoIndex()>=0
// or BuildDemoWantsHouse() (both default off), so a normal default run force-completes
// every start-town building as before.
static bool BuildDemoEnabled() {
    return ::ComponentFactory::LiveEnabled("BANISHED_LIVE_BUILDCTRL");
}

// Retail's insta-build spawn path (BANISHED_LIVE_SM_ONADDED, opt-in). When on, the
// founding buildings are spawned exactly as ComponentEntity_impl_1fc0 does it —
// SetModel(variant, frame 0) inside the gw+0xd0 bit-0 bracket — and the state machine
// finishes them during RegisterEntity's OnAdded pass (StateMachineComponent slot 6 ticks
// the SM; State1Enter sees the instant bit and requests state 8; State8Enter's INSTANT
// branch snaps the meshes to their final frame, stamps delivered = target, requests
// state 2). That retires three standalone stand-ins here: the subCount-1 model frame,
// the +0x52 = 2 stamp, and the +0x48/+0x4c build-bookkeeping stamp.
// The flag lives on StateMachineComponent because that is the component whose slots it
// wires; this helper only asks whether the path is available.
static bool StartTown_InstantBuildPath() {
    static int on = -1;
    if (on < 0) on = ::ComponentFactory::LiveEnabled("BANISHED_LIVE_SM_ONADDED") ? 1 : 0;
    return on != 0;
}
static int BuildDemoIndex() {
    static int idx = -2;
    if (idx == -2) {
        const char* e = std::getenv("BANISHED_BUILD_DEMO_INDEX");
        // Default -1 = NO forced demo foundation: leaves g_pConstructionDemoSite null so
        // BuildComponent_DemoGateSkip drives any player-placed foundation organically
        // instead. A start-town building is force-completed into the demo only when an
        // explicit index (>=0) is requested. (Old default 0 seeded the StorageBarn, which
        // had no haul source anyway.)
        idx = (e && e[0]) ? std::atoi(e) : -1;
    }
    return idx;
}
// House-demo mode (BANISHED_BUILD_DEMO_HOUSE): pick the FIRST WoodHouse (residence)
// as the construction-demo foundation, by component type rather than a fragile
// numeric index. Targeting a storage building (StorageBarn, the old index-0 default)
// made the build sim reconfigure the town's main resource pile into build-INPUT
// storage → citizens had nowhere to pick materials up → haul crash. A house is a
// pure consumer, so the barn/yard survive as the haul source. Only the first
// residence is taken (g_demoHouseAssigned latches).
static bool g_demoHouseAssigned = false;
static bool BuildDemoWantsHouse() {
    static int en = -1;
    if (en < 0) {
        const char* e = std::getenv("BANISHED_BUILD_DEMO_HOUSE");
        en = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return en == 1;
}

// ── BANISHED_EXTRA_HOUSES=N — N extra COMPLETED, EMPTY houses (test scaffolding) ──
// A move-out (and therefore a marriage) needs a FREE residence. The founding town has
// none spare, and BUILD_DEMO_HOUSE's single spare starts as a foundation, so marriage
// throughput is capped by the build pipeline rather than by the marriage logic. This
// spawns ready-to-occupy copies of the first start-town house so the marriage/move-out
// path can be exercised on its own. Test-only, default 0 (byte-unchanged when unset).
static const int kMaxExtraHouses = 32;
static const int kExtraHouseSearchRadius = 40;   // tiles from the cluster centre
static int ExtraHousesRequested() {
    static int n = -1;
    if (n < 0) {
        const char* e = std::getenv("BANISHED_EXTRA_HOUSES");
        n = (e && e[0]) ? std::atoi(e) : 0;
        if (n < 0) n = 0;
    }
    return n;
}

// Defined below (template-by-path resolver used by the demo-pasture block).
static void* ResolveTemplateByPath(const char* path);

// ComponentEntity_impl_1fc0 — spawn the initial buildings around (spawnX,spawnY).
// Siting inputs of the demo gatherer hut, captured at placement so its
// accepted-resource count can be retaken after the resource scatter (see the
// ORDERING note at the capture site).
static struct {
    bool valid = false;
    int cx = 0, cy = 0, radius = 0;
    uintptr_t* acceptList = nullptr;
    int acceptN = 0;
    int scoreAtPlacement = 0;
} g_demoGatherer;

// Recount the accepted natural resources inside the demo hut's radius. Called
// after StartTown_SpawnResources, so unlike the placement-time score this one
// sees the wild food the trees' SpawnComponent seeded.
static void StartTown_RecountGatherable() {
    if (!g_demoGatherer.valid) return;
    void* gws = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    uint8_t* mapObj = gws ? *(uint8_t**)((uint8_t*)gws + 0xf8) : nullptr;
    const int width = mapObj ? *(int32_t*)mapObj : 0;
    uint8_t* grid = mapObj ? *(uint8_t**)(mapObj + 0x80) : nullptr;
    if (!grid || width <= 0 || !g_demoGatherer.acceptList || g_demoGatherer.acceptN <= 0) return;

    const int r = g_demoGatherer.radius;
    int score = 0, anyResource = 0;
    for (int dy = -r; dy < r; ++dy)
        for (int dx = -r; dx < r; ++dx) {
            if (dx * dx + dy * dy > r * r) continue;
            const int x = g_demoGatherer.cx + dx, y = g_demoGatherer.cy + dy;
            if (x < 0 || y < 0 || x >= width || y >= width) continue;
            uintptr_t re = *(uintptr_t*)(grid + (size_t)(y * width + x) * 0x30 + 8);
            if (!re) continue;
            ++anyResource;
            const uintptr_t rd = *(uintptr_t*)(re + 0x28);
            for (int t = 0; t < g_demoGatherer.acceptN; ++t)
                if (g_demoGatherer.acceptList[t] == rd) { ++score; break; }
        }
    fprintf(stderr, "[startTown::gathererhut] POST-SCATTER recount at (%d,%d) r=%d: "
            "GATHERABLE=%d (was %d at placement, before any wild food existed); "
            "%d resource cell(s) of any kind in radius\n",
            g_demoGatherer.cx, g_demoGatherer.cy, r, score,
            g_demoGatherer.scoreAtPlacement, anyResource);
    fflush(stderr);
}

// ─────────────────────────────────────────────────────────────────────────────
// Shared siting state, published out of StartTown so a DEFERRED spawn can reuse it.
//
// The hunter lodge cannot be sited at StartTown time. Its whole job is to sit among
// the deer — retail's HuntWork::Execute (0x1400eeff0) walks the hunter to a RANDOM
// reachable cell inside the lodge's work radius and lets HuntAnimal's per-frame +0x30
// tick take the kill opportunistically en route, so a lodge that is not already in
// deer country never gets a hunter within the 8.0 kill radius (measured 2026-08-14:
// nearestTyped 30.85..44.10, inR_typed=0 always).
//
// The only scorer for "deer country" is the character spatial-hash manager at gw+0x88,
// and that index is EMPTY during world build — characters register themselves during
// their own Startup. Scoring animal density at StartTown is therefore measuring an
// empty index by construction, the same shape of ordering fault as the demo gatherer
// hut scoring wild food before the resource scatter had run. So the lodge placement is
// deferred a few hundred frames and driven from the game update loop instead;
// everything it needs — the placement routine, the occupancy rects and the workability
// predicate — is published here.
// ─────────────────────────────────────────────────────────────────────────────

// Footprint rects of everything StartTown placed, so a deferred placement can avoid
// landing on top of the founding cluster.
struct STRect { int x, y, w, h; };
static std::vector<STRect> g_stOccupied;

// StartTown's placeStructure, captured BY VALUE so it outlives StartTown_SpawnStructures.
// Its captures are the entity list, the map object and two type ids, all stable for the
// life of the world — a by-reference capture would dangle the moment world build returned.
static std::function<void*(void* /*tmpl*/, int /*tileX*/, int /*tileY*/, int /*orient*/,
                           int /*w0*/, int /*h0*/, bool /*isDemoFoundation*/,
                           int /*logIndex*/)> g_stPlaceStructure;

// Reject a candidate site unless EVERY footprint cell passes the workability predicate
// the work classes themselves use (Map_FindNearestPassable with the 0xe0f mask).
// Occupancy-only siting let a demo crop field straddle a cell no citizen can ever work:
// that one cell is never planted, `growing + done == total` never holds, and the field
// parks one slot short of state 10 forever — a harness siting fault that reads exactly
// like a port bug. Hoisted to file scope (it was a lambda inside StartTown_SpawnStructures
// and captured nothing but globals) so the deferred placement applies the identical test.
static bool ST_SiteIsWorkable(int tx, int ty, int fw, int fh) {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return true;
    int* grid = *reinterpret_cast<int**>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf8);
    if (!grid) return true;
    typedef int (*FNP_t)(int*, int, int, uint32_t);
    static FNP_t s_fnp = nullptr;
    static bool s_r = false;
    if (!s_r) { s_fnp = reinterpret_cast<FNP_t>(ResolveAddr(Module::Exe, 0x14009ccf0));
                s_r = true; }
    if (!s_fnp) return true;
    const int mapW = *grid;
    for (int yy = 0; yy < fh; ++yy) {
        for (int xx = 0; xx < fw; ++xx) {
            const int cell = (ty + yy) * mapW + (tx + xx);
            if (cell < 0 || cell >= mapW * mapW) return false;
            if (s_fnp(grid, cell, cell, 0xe0f) < 0) return false;
        }
    }
    return true;
}

// Same one-tile-margin overlap test the in-StartTown ring scans use, against the
// footprints StartTown recorded.
static bool ST_ClashesWithStartTown(int tx, int ty, int fw, int fh) {
    for (const STRect& r : g_stOccupied)
        if (tx - 1 < r.x + r.w && r.x < tx + fw + 1 &&
            ty - 1 < r.y + r.h && r.y < ty + fh + 1) return true;
    return false;
}

// The deferred demo hunter lodge request, filled in by the food-demo block below and
// consumed by StartTown_DeferredSpawn_Tick once the world has been running a while.
static struct {
    bool  wanted     = false;
    bool  done       = false;
    int   fireFrame  = 300;
    int   w = 0, h = 0;
    int   townCx = 0, townCy = 0;
    int   workRadius = 0;      // RadiusDescription+0x28, in TILES
    void* tmpl = nullptr;
} g_deferredLodge;

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

    // Place ONE structure from `tmpl` at (tileX,tileY): create the entity, position its
    // MapComponent + ModelComponent world transform, show the completed model (or leave it
    // at the foundation mesh when isDemoFoundation), register it, then run the OnCreate /
    // region-cell-mark / storage-wire passes. Returns the entity (or null). Factored out of
    // the per-entry loop (as a faithful 1:1 lambda — same file-scope helpers/globals) so the
    // construction-demo foundation can be spawned as a FRESH extra building AFTER the loop
    // (follow-up #7) instead of cannibalising a real start house.
    //
    // Captured BY VALUE (entity list, map object, two type ids — all stable for the life
    // of the world) rather than by reference, so the copy published in g_stPlaceStructure
    // below can still be called frames after world build has returned. See the DEFERRED
    // SPAWN note above StartTown_SpawnStructures.
    auto placeStructure = [entityList, game, tidMapComponent, tidModelComponent](
                              void* tmpl, int tileX, int tileY, int orient,
                              int w0, int h0, bool isDemoFoundation, int logIndex) -> void* {
        // Instantiate + wrap.
        void* mem = fn_MemAlloc(g_pMemAllocSingleton, ENTITY_SIZE, 8);
        void* ent = mem ? fn_EntityFactory_CreateEntity(mem, tmpl) : nullptr;
        uintptr_t entIBP = 0;
        fn_InternalBasePtr_Ctor(&entIBP, ent);

        // 90 / -90 / 270 degrees swap the footprint axes.
        int fpW = w0, fpH = h0;
        if (orient == 0x5a || orient == -0x5a || orient == 0x10e) { fpW = h0; fpH = w0; }

        int rot = orient;
        while (rot < 0) rot += ROT_FULL;

        // ── STANDING-RESOURCE RESIDUE CHECK ─────────────────────────────────────────
        // This lambda instant-stamps a finished structure onto the map. The retail route
        // to a finished structure is placement tool -> foundation -> a ClearArea work
        // that DESTROYS the natural resources inside the footprint -> build, so by the
        // time a building exists its footprint is empty. Nothing here reproduces the
        // clear, so a start-town / demo structure can end up standing on live trees.
        //
        // That is not cosmetic. FishingWork slot 4's fallback (fishingwork.cpp) reads the
        // WorkComponent of whatever entity holds cell+8 on one of its spot cells and
        // derefs it unguarded at +0x28 — faithfully; retail does the same at
        // 0x1400ea775..0x1400ea793 and relies on the footprint being clear. A surviving
        // tree has NaturalResource/Growth/Spawn and NO WorkComponent, so it is a null
        // deref (READ of 0x28). Reproduced at seed 999: dock at (85,120), tree left
        // standing on drop cell (88,124).
        //
        // Reported unconditionally when non-zero, because a silent residue reads exactly
        // like a clean placement right up until something walks the cell.
        if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
            uint8_t* mapObj = *(uint8_t**)((uint8_t*)*g_pGameWorldSingleton + 0xf8);
            uint8_t* grid   = mapObj ? *(uint8_t**)(mapObj + 0x80) : nullptr;
            const int mw    = mapObj ? *(int32_t*)mapObj : 0;
            static uint32_t tidNat = 0;
            if (!tidNat) tidNat = TypeId("NaturalResourceComponent");
            if (grid && mw > 0 && tidNat) {
                int residue = 0, fx = -1, fy = -1;
                for (int yy = tileY; yy < tileY + fpH; ++yy) {
                    for (int xx = tileX; xx < tileX + fpW; ++xx) {
                        if (xx < 0 || yy < 0 || xx >= mw || yy >= mw) continue;
                        uintptr_t occ = *(uintptr_t*)(grid + 8 +
                                        (uintptr_t)(mw * yy + xx) * 0x30);
                        if (!occ) continue;
                        if (FindComponentOnEntity((void*)occ, tidNat)) {
                            if (!residue) { fx = xx; fy = yy; }
                            ++residue;
                        }
                    }
                }
                if (residue)
                    fprintf(stderr, "[startTown::residue] structure at (%d,%d) %dx%d is "
                            "being stamped over %d standing natural-resource cell(s) "
                            "(first (%d,%d)) — no ClearArea ran, so they survive under the "
                            "building\n", tileX, tileY, fpW, fpH, residue, fx, fy);
            }
        }

        // MapComponent grid placement (resolvable now that components carry a real
        // GetTypeId — Foundation E).
        fn_InternalBasePtr_CheckPtr(&entIBP);
        void* entity = (void*)entIBP;

        // ── INSTANT-BUILD bracket (retail ComponentEntity_impl_1fc0). ────────────────
        // Retail sets gameWorld+0xd0 bit 0 here, right before the MapComponent/
        // ModelComponent setup, and clears it immediately after RegisterEntity below:
        //     *(uint*)(g_pGameWorldSingleton + 0xd0) |= 1;
        //     ... map placement, transform, ModelComponent::SetModel(0x1401253d0) ...
        //     FUN_1400c48a0(entityList, &ibp, category);          // RegisterEntity
        //     *(uint*)(g_pGameWorldSingleton + 0xd0) &= 0xfffffffe;
        //
        // That bit is the instant-build flag (buildcomponent_state1.cpp:320,
        // buildcomponent_state8.cpp:298 — a build with it set skips the staged
        // gather/ramp and completes immediately). Crucially it is ALSO the first test in
        // BuildComponent's OnAdded (0x140108d40): with the bit set that slot returns
        // before its Stage 3 can hide the model and stamp modelVariant = -1.
        //
        // This bracket is why retail can safely hand a pre-built start-town building a
        // finished mesh BEFORE registering it — which is exactly what the code below
        // does. Without it, RegisterEntity's OnAdded pass un-frames every founding
        // building and the whole town renders as bare foundations (observed 2026-07-27
        // once BuildComponent's OnAdded slot was wired). Missing bracket = the real gap;
        // porting it retires the standalone guard that was briefly used instead.
        const uintptr_t gwFlags = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                                ? (uintptr_t)*g_pGameWorldSingleton + 0xd0 : 0;
        if (gwFlags) *(uint32_t*)gwFlags |= 1u;

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
        //
        // Construction-demo exception: the demo foundation is left at its foundation mesh
        // so the BuildComponent state machine can drive it through the real build stages.
        if (!isDemoFoundation) {
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
                    // Retail passes construction frame 0 here (XOR R8D,R8D at 0x1400a2677,
                    // disassembly-verified) and lets the instant-build path inside the
                    // bracket finish the building: State1Enter -> state 8 -> State8Enter's
                    // INSTANT branch snaps every mesh to frameCount-1. Our subCount-1 is the
                    // stand-in for that path; with BANISHED_LIVE_SM_ONADDED on, the path is
                    // live (SM chain linked at Startup + slot-6 tick at register time), so
                    // use retail's 0 and let it do the work.
                    const bool instantPath = StartTown_InstantBuildPath();
                    int modelVariant = instantPath ? 0
                                     : ((subCount > 0) ? subCount - 1 : 0);  // last sub-mesh = complete
                    ApplyResourceModelVariant(entity, tidModelComponent,
                                              (int16_t)variantIdx, (int16_t)modelVariant);
                    fprintf(stderr, "[startTown::struct] [%d] complete-model variantIdx=%d "
                            "modelVariant=%d (subCount=%d groups=%u)%s\n",
                            logIndex, variantIdx, modelVariant, subCount, groupCount,
                            instantPath ? "  [retail frame 0 + instant-build path]" : "");
                    fflush(stderr);
                }
            }

            // ── Mark the pre-placed building as BUILT for the info-panel page router. ──
            // These structures are spawned already-complete, but in standalone their
            // state-machine controllers are inert, so StateMachineComponent setup never
            // wrote the current-state byte (+0x52) — it sits at factory-zero 0. EntityUI::
            // Slot7 (0x140066d80) reads that byte as pageIdx: pageIdx==0 forces the under-
            // construction page (2, the "Remove 0 of 0" material table) for EVERY pre-built
            // structure, regardless of BuildComplete. Stamp the build graph's completed
            // state (2) so the panel routes to the operational page instead. (Demo
            // foundations are excluded — the live build sim drives their real state.)
            //
            // Resolve by NAME via FindComponentOnEntity: the exe-.rdata type-id global
            // g_TypeId_StateMachineComponent_FC is still null during world-build (raw .rdata is
            // unresolvable in SA; it's lazily filled post-Engine::Startup), so the IBP finder
            // fn_FindComponent_BuildState returns null here. The name-based lookup matches on
            // the live vtable GetTypeId, exactly like the modelComp lookup just above.
            //
            // MOSTLY RETIRED by the instant-build path (BANISHED_LIVE_SM_ONADDED): with the
            // SM chain linked at Startup and its slot-6 tick live, RegisterEntity below runs
            // the real 1 -> 8 -> 2 chain inside the instant-build bracket, so +0x52 ends
            // up 2 because the state machine ENTERED state 2 — which also fires every
            // other controller's state-2 enter (ResidenceComponent's household registry,
            // etc.) instead of leaving them to a stamped-but-never-entered state.
            // It only reaches entities that HAVE a BuildComponent, though: the stockpile
            // (start-town entry with no model group and no BuildComponent) has nothing to
            // drive its SM, and its own first controller is inert so the linker cannot read
            // a real default either. So the stamp is not deleted — it becomes a post-register
            // FALLBACK below (stamp only if the real path did not already land on 2), which
            // also measures exactly which entities still need it.
            void* bsComp = FindComponentOnEntity(entity, TypeId("StateMachineComponent"));
            if (StartTown_InstantBuildPath()) {
                /* deferred: see the post-RegisterEntity fallback */
            } else if (bsComp) {
                *(int8_t*)((uintptr_t)bsComp + 0x52) = 2;  // built/complete (build state graph state 2)
                fprintf(stderr, "[startTown::struct] [%d] stamped StateMachineComponent+0x52=2 (built)\n",
                        logIndex);
                fflush(stderr);
            } else {
                fprintf(stderr, "[startTown::struct] [%d] WARN: StateMachineComponent not found for +0x52 stamp\n",
                        logIndex);
                fflush(stderr);
            }

            // ── Stamp the build-material bookkeeping a CONSTRUCTED building carries. ──
            // Same class of gap as the +0x52 stamp above. These structures are spawned
            // already-complete, so BuildComponent_DemoGateSkip (model already at its final
            // sub-mesh) skips their build state machine and State1Enter never runs — leaving
            // the cost multiplier (+0x4c) and the delivered counter (+0x48) at factory-zero.
            // The upgrade path reads exactly those two: DestroyComponent state-5 enter takes
            // its REFUND+BUILD branch — the one that spawns the replacement foundation — only
            // when delivered(+0x48) != 0 (destroycomponent_states.cpp Yard_Delivered). With
            // both zero a starter house demolished correctly and then never got a replacement
            // (observed live 2026-07-25: "delivered(+0x48)=0 required=0 -> REMOVE(no-spawn)").
            // Stamp it as fully delivered: multiplier chosen the same way State1Enter picks it
            // (default 1, workplace +0x58 only when that is a valid positive value), then
            // delivered = required = buildDesc(+0x18)->+0x30 * multiplier.
            //
            // RETIRED by the instant-build path (BANISHED_LIVE_SM_ONADDED): State8Enter's
            // INSTANT branch writes +0x4c (via sub_8720) and +0x48 = matComp[0x30] * +0x4c
            // itself, from the real BuildDescription, during RegisterEntity below.
            void* bcBuilt = StartTown_InstantBuildPath()
                          ? nullptr : FindComponentOnEntity(entity, TypeId("BuildComponent"));
            if (bcBuilt) {
                uintptr_t bc = (uintptr_t)bcBuilt;
                uint32_t mult = 1;
                void* wpcBuilt = FindComponentOnEntity(entity, TypeId("WorkPlaceComponent"));
                if (wpcBuilt) {
                    uint32_t wpMult = *(uint32_t*)((uintptr_t)wpcBuilt + 0x58);
                    if ((int32_t)wpMult > 0) mult = wpMult;
                }
                *(uint32_t*)(bc + 0x4c) = mult;
                void* bDesc = *(void**)(bc + 0x18);
                int required = bDesc ? *(int*)((uintptr_t)bDesc + 0x30) * (int)mult : 0;
                *(int32_t*)(bc + 0x48) = required;
                fprintf(stderr, "[startTown::struct] [%d] stamped BuildComponent as built: "
                        "mult(+0x4c)=%u delivered(+0x48)=%d desc=%p%s\n",
                        logIndex, mult, required, bDesc,
                        required == 0 ? "  <-- WARN: required computed 0, upgrade will still REMOVE(no-spawn)" : "");
                fflush(stderr);
            }

            // ── Activate a pre-built EMPLOYER: seat WPC+0x48 = desc+0x20 (the key retail
            // seats at data load and f670 re-seats at build completion) + SetAccepting.
            // Registration into townMgr+0x100 is the WPC controller's state-2 enter
            // (edf0), which the +0x52 = 2 stamp above makes fire on the first SM tick
            // when BANISHED_LIVE_WPC_CONTROLLER is on; with it off, the helper registers
            // inline (the pre-controller stand-in). See StartTown_ActivateWorkplace.
            {
                uint64_t profKeyS = StartTown_ActivateWorkplace(
                    entity, /*registerNow=*/false, "startTown::struct");
                if (profKeyS != 0) {
                    fprintf(stderr, "[startTown::struct] [%d] employer activated "
                            "(profKey=0x%llx)\n", logIndex, (unsigned long long)profKeyS);
                    fflush(stderr);
                }
            }
        }

        // Register with the world entity list (category = template+0x20).
        int category = *(int32_t*)((uint8_t*)tmpl + TEMPLATE_CATEGORY);
        fn_EntityList_RegisterEntity(entityList, &entIBP, category);

        // Close the instant-build bracket opened above — retail clears the bit on the
        // very next instruction after RegisterEntity, so everything from here on (the
        // OnCreate pass, cell marking, storage wiring) sees it clear, and so does every
        // later per-frame BuildComponent tick.
        if (gwFlags) *(uint32_t*)gwFlags &= ~1u;

        // ── Instant-build path: what the real chain did NOT cover. ────────────────────
        // The bracket above just ran the faithful 1 -> 8 -> 2 completion for every
        // structure that has a BuildComponent. Anything still not at state 2 had nothing
        // to drive it — the stockpile (StorageYard) has no BuildComponent at all, and
        // measurement on 2026-07-27 killed the standing theory that our linker was merely
        // failing to read its default: the stockpile's FIRST controller in array order is
        // AbsorbResourceComponent ([9]; StorageLocationComponent is LAST at [12]), and
        // AbsorbResource's retail GetDefaultState (0x140003730) genuinely returns 0. Retail
        // stamps 0 here too, so no linker change can produce state 2 — some other retail
        // path posts it, and until that path is found this stamp is a real deviation, not a
        // gap-filler. See the LINK-DECIDE block in statemachinecomponent.cpp.
        // Those entities keep the pre-existing stand-in stamp. Logging both outcomes is the
        // measurement: a line
        // per entity that still needs the fallback, and silence for the ones the real
        // state machine finished.
        if (StartTown_InstantBuildPath() && !isDemoFoundation) {
            fn_InternalBasePtr_CheckPtr(&entIBP);
            void* postEnt = (void*)entIBP;
            void* smPost = FindComponentOnEntity(postEnt, TypeId("StateMachineComponent"));
            void* bcPost = FindComponentOnEntity(postEnt, TypeId("BuildComponent"));
            // +0x50 is the CURRENT state, +0x52 the PENDING one (0xff = nothing pending) —
            // the pair EntityUI's page router reads as `pending < 0 ? current : pending`
            // (entityui.cpp:588). The old stand-in wrote the PENDING byte so that the first
            // tick would enter state 2; when the real chain has already run, pending is
            // back to 0xff and current is 2, so "did it work?" is a +0x50 test.
            int cur = smPost ? *(int8_t*)((uintptr_t)smPost + 0x50) : -1;
            int pend = smPost ? *(int8_t*)((uintptr_t)smPost + 0x52) : -1;
            int delivered = bcPost ? *(int32_t*)((uintptr_t)bcPost + 0x48) : -1;
            if (smPost && cur != 2 && pend != 2) {
                *(int8_t*)((uintptr_t)smPost + 0x52) = 2;
                fprintf(stderr, "[startTown::struct] [%d] instant-path FALLBACK: state(+0x50)=%d "
                        "pending(+0x52)=%d -> stamped pending 2 (buildComp=%p)\n",
                        logIndex, cur, pend, bcPost);
                fflush(stderr);
            } else if (smPost) {
                fprintf(stderr, "[startTown::struct] [%d] instant-path: state(+0x50)=%d "
                        "pending(+0x52)=%d delivered(+0x48)=%d (no fallback needed)\n",
                        logIndex, cur, pend, delivered);
                fflush(stderr);
            }
            if (bcPost && delivered == 0) {
                uintptr_t bc = (uintptr_t)bcPost;
                uint32_t mult = *(uint32_t*)(bc + 0x4c);
                if ((int32_t)mult <= 0) mult = 1;
                void* bDesc = *(void**)(bc + 0x18);
                int required = bDesc ? *(int*)((uintptr_t)bDesc + 0x30) * (int)mult : 0;
                *(uint32_t*)(bc + 0x4c) = mult;
                *(int32_t*)(bc + 0x48) = required;
                fprintf(stderr, "[startTown::struct] [%d] instant-path FALLBACK: delivered was 0 "
                        "-> %d (mult=%u)\n", logIndex, required, mult);
                fflush(stderr);
            }
        }

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
        StartTown_MarkBuildingCells(game, tileX, tileY, fpW, fpH, entIBP, rot);
        StartTown_PickMeshProbe((void*)entIBP);

        if (isDemoFoundation) {
            // Record this foundation as the construction-demo site. Its storage is
            // configured for INPUT materials by State1Enter (not the finished-building
            // OUTPUT init), so skip StartTown_WireStorage here.
            void* bc = FindComponentOnEntity(entity, TypeId("BuildComponent"));
            g_pConstructionDemoSite = bc;
            g_pConstructionDemoEntity = entity;   // owner entity (work-area for the gather/build force harness)
            fprintf(stderr, "[startTown::struct] [%d] DEMO FOUNDATION seeded "
                    "entity=%p buildComp=%p tile=(%d,%d) (left at foundation mesh)\n",
                    logIndex, entity, bc, tileX, tileY);
            fflush(stderr);
        } else {
            // Brick M-D (BANISHED_LIVE_STORAGE): run the build-complete storage init
            // + town storage-location registration for this fully-built building —
            // retail's StorageLocationComponent OnAdded fires during RegisterEntity,
            // so this sits right after it.
            StartTown_WireStorage(entity, logIndex);
        }

        fprintf(stderr, "[startTown::struct] [%d] tmpl=%p tile=(%d,%d) fp=%dx%d rot=%d map=%p\n",
                logIndex, tmpl, tileX, tileY, fpW, fpH, rot, mapComp);
        fflush(stderr);
        return ent;
    };
    g_stPlaceStructure = placeStructure;   // for the deferred spawn tick

    // Fresh construction-demo foundation (follow-up #7): the OLD path converted the FIRST
    // start-town residence itself into the demo foundation, leaving the player one home
    // short. Instead capture the first residence's template + footprint during the loop and
    // spawn a COPY as the foundation AFTER the loop, clear of the whole cluster — every real
    // start house stays complete. Only the house-demo path (default) does this; an explicit
    // BANISHED_BUILD_DEMO_INDEX still marks that in-place building as the foundation.
    void* demoTmpl = nullptr;
    int demoW0 = 0, demoH0 = 0;
    int clusterMaxEdgeX = spawnX;

    // First residence template + footprint, captured unconditionally (the demo-house path
    // and BANISHED_EXTRA_HOUSES both spawn copies of it).
    void* resTmpl = nullptr;
    int resW0 = 0, resH0 = 0;

    // Footprint rects of everything placed, so the extra-house search can avoid overlaps.
    // Bounded: 8 start structures + 1 demo foundation + kMaxExtraHouses.
    struct Rect { int x, y, w, h; };
    Rect occ[8 + 1 + kMaxExtraHouses + 8];
    int  occN = 0;
    g_stOccupied.clear();
    auto Occupy = [&](int x, int y, int w, int h) {
        if (occN < (int)(sizeof(occ) / sizeof(occ[0]))) occ[occN++] = { x, y, w, h };
        g_stOccupied.push_back({ x, y, w, h });   // unbounded copy for the deferred spawn
    };
    // Cluster bounding box, to centre the extra-house search on known-good ground.
    int bbX0 = 1 << 28, bbY0 = 1 << 28, bbX1 = -(1 << 28), bbY1 = -(1 << 28);

    for (int i = 0; i < count; ++i) {
        uint8_t* entry = pool + (size_t)i * STRUCT_STRIDE;

        void* tmpl = StartTown_ResolveTemplate(entry);
        if (!tmpl) {
            fprintf(stderr, "[startTown::struct] [%d] template unresolved\n", i);
            fflush(stderr);
            continue;
        }

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

        // In-place demo only for the explicit-index path (default index -1 => never).
        const bool inPlaceDemo = BuildDemoEnabled() && !BuildDemoWantsHouse() &&
                                 (i == BuildDemoIndex());

        void* ent = placeStructure(tmpl, tileX, tileY, orient, w0, h0, inPlaceDemo, i);
        if (ent) {
            ++spawned;
            // Capture the first structure entity for the BANISHED_FORCE_WORK harness.
            if (!g_pForceWorkBuilding) g_pForceWorkBuilding = ent;
        }

        // Track the cluster's east edge (to place the fresh demo foundation clear of it) and
        // capture the first residence template as the demo-house source. Only the X-extent
        // (footprint width after the orientation axis-swap) matters for the east edge.
        const int fpWx = (orient == 0x5a || orient == -0x5a || orient == 0x10e) ? h0 : w0;
        const int fpHy = (orient == 0x5a || orient == -0x5a || orient == 0x10e) ? w0 : h0;
        if (tileX + fpWx > clusterMaxEdgeX) clusterMaxEdgeX = tileX + fpWx;
        Occupy(tileX, tileY, fpWx, fpHy);
        if (tileX < bbX0) bbX0 = tileX;
        if (tileY < bbY0) bbY0 = tileY;
        if (tileX + fpWx > bbX1) bbX1 = tileX + fpWx;
        if (tileY + fpHy > bbY1) bbY1 = tileY + fpHy;

        if (ent && !resTmpl && FindComponentOnEntity(ent, TypeId("ResidenceComponent"))) {
            resTmpl = tmpl; resW0 = w0; resH0 = h0;
        }
        if (BuildDemoEnabled() && BuildDemoWantsHouse() && !g_demoHouseAssigned && ent &&
            FindComponentOnEntity(ent, TypeId("ResidenceComponent"))) {
            demoTmpl = tmpl; demoW0 = w0; demoH0 = h0;
            g_demoHouseAssigned = true;
        }
    }

    // Spawn the FRESH WoodHouse foundation two tiles east of the cluster, in the spawn row
    // (a walkable band by construction), left at its foundation mesh and recorded as the
    // construction-demo site — so the build-labor harness drives THIS building, not a real
    // start house.
    if (demoTmpl) {
        const int dTileX = clusterMaxEdgeX + 2;
        const int dTileY = spawnY;
        void* demoEnt = placeStructure(demoTmpl, dTileX, dTileY, 0, demoW0, demoH0,
                                       /*isDemoFoundation=*/true, -1);
        if (demoEnt) ++spawned;
        Occupy(dTileX, dTileY, demoW0, demoH0);
        fprintf(stderr, "[startTown::struct] FRESH demo foundation spawned tile=(%d,%d) ent=%p\n",
                dTileX, dTileY, demoEnt);
        fflush(stderr);
    }

    // ── BANISHED_EXTRA_HOUSES=N — N extra, COMPLETED, EMPTY houses ────────────────────
    // The founding town is exactly six families in six houses, so nothing is ever spare
    // and no grown child can move out (a free residence is the precondition for marriage
    // — see CanFindHousing / candidate state 2). BUILD_DEMO_HOUSE gives exactly one, and
    // it starts as a FOUNDATION, so it only becomes a move-out target once the build
    // pipeline finishes it. This spawns N ready-to-occupy houses instead, which decouples
    // marriage/move-out testing from construction throughput.
    //
    // Placement: copies of the first start-town residence, laid at the nearest free
    // footprints to the cluster's centre (spiral outward, 1-tile gap, no overlap with any
    // recorded rect). Hugging the cluster matters — the start config only guarantees
    // buildable/walkable ground around the spawn point, so ranging far afield risks
    // houses on unreachable terrain.
    const int extraWanted = ExtraHousesRequested();
    if (extraWanted > 0 && resTmpl) {
        const int cx = (bbX0 + bbX1) / 2, cy = (bbY0 + bbY1) / 2;
        const int n = extraWanted > kMaxExtraHouses ? kMaxExtraHouses : extraWanted;
        int placedExtra = 0;

        for (int k = 0; k < n; ++k) {
            int bestX = 0, bestY = 0;
            bool found = false;
            // Chebyshev-ring scan outward from the cluster centre; first free rect wins.
            for (int r = 1; r <= kExtraHouseSearchRadius && !found; ++r) {
                for (int dy = -r; dy <= r && !found; ++dy) {
                    for (int dx = -r; dx <= r && !found; ++dx) {
                        if (dx != -r && dx != r && dy != -r && dy != r) continue;  // ring only
                        const int tx = cx + dx, ty = cy + dy;
                        // 1-tile gap on every side so buildings never touch (walk lanes).
                        bool clash = false;
                        for (int o = 0; o < occN; ++o) {
                            if (tx - 1 < occ[o].x + occ[o].w && occ[o].x < tx + resW0 + 1 &&
                                ty - 1 < occ[o].y + occ[o].h && occ[o].y < ty + resH0 + 1) {
                                clash = true; break;
                            }
                        }
                        if (!clash) { bestX = tx; bestY = ty; found = true; }
                    }
                }
            }
            if (!found) {
                fprintf(stderr, "[startTown::extraHouse] no free footprint within r=%d "
                        "after %d houses; stopping\n", kExtraHouseSearchRadius, placedExtra);
                fflush(stderr);
                break;
            }

            void* ent = placeStructure(resTmpl, bestX, bestY, 0, resW0, resH0,
                                       /*isDemoFoundation=*/false, -1);
            Occupy(bestX, bestY, resW0, resH0);
            if (ent) { ++spawned; ++placedExtra; }
            fprintf(stderr, "[startTown::extraHouse] [%d/%d] tile=(%d,%d) fp=%dx%d ent=%p\n",
                    k + 1, n, bestX, bestY, resW0, resH0, ent);
            fflush(stderr);
        }

        fprintf(stderr, "[startTown::extraHouse] placed %d/%d extra completed houses "
                "(cluster bbox x[%d,%d] y[%d,%d] centre=(%d,%d))\n",
                placedExtra, n, bbX0, bbX1, bbY0, bbY1, cx, cy);
        fflush(stderr);
    } else if (extraWanted > 0) {
        fprintf(stderr, "[startTown::extraHouse] BANISHED_EXTRA_HOUSES=%d requested but no "
                "start-town residence template was found; nothing spawned\n", extraWanted);
        fflush(stderr);
    }

    // ── BANISHED_BUILD_DEMO_PASTURE — one COMPLETED pasture (test scaffolding) ──────
    // Retail acquires pastures only through the fenced-area tool (still unported), so
    // nothing can exercise the pasture controller (adopt/reproduce/produce) without
    // this. Spawns Template\Pasture.rsc as a completed structure at the nearest free
    // footprint to the cluster centre, same ring scan as the extra houses. The pasture
    // then needs a livestock TYPE (BANISHED_PASTURE_AUTOTYPE=<Cow|Sheep|Chicken>) and
    // animals (BANISHED_PASTURE_FILL=<frame>, standalone/pasture_harness.cpp) to come
    // alive. Value: 1/on = template-default footprint, "WxH" (e.g. "10x8") overrides.
    // Test-only, default off (byte-unchanged when unset).
    //
    // ⚠ THIS SPAWNS THE MASTER ONLY -- no AnimalShelter, no fence sections. The real
    // acquisition path (FencedTool::CalcPath) creates those as MultiObject children, and
    // InteractComponent slot 6 rebuilds the master's interact child list from them. The
    // master's own InteractDescription is EMPTY in Pasture.rsc, so with no children the
    // kind-2 ("create") point count is 0 and the per-frame produce tail can never place a
    // pile: it spends the accumulator and takes the NO_FREE_CELL arm forever, which reads
    // downstream exactly like 'the pasture produces nothing' and leaves the production-
    // statistics panel permanently empty. Measured 2026-08-19. For anything about produce,
    // piles, the tracker or the production panel use BANISHED_TOOL_HARNESS=pasture (a real
    // tool-placed pasture: same rig then drops on schedule and the panel counts up).
    {
        const char* pe = std::getenv("BANISHED_BUILD_DEMO_PASTURE");
        if (pe && pe[0] && pe[0] != '0') {
            void* ptmpl = ResolveTemplateByPath("Template\\Pasture.rsc");
            int pw = 0, ph = 0;
            if (const char* xpos = std::strchr(pe, 'x')) {   // "WxH" size override
                pw = std::atoi(pe); ph = std::atoi(xpos + 1);
            }
            if (ptmpl && (pw <= 0 || ph <= 0)) {
                // Template-default footprint from CreatePlacedDescription (+0x44/+0x48);
                // pastures are variable-size so the authored default may be 0 — fall
                // back to 8x8 (capacity ~= a small starting herd).
                void* cpd = FindDescriptionOnTemplate(ptmpl, TypeId("CreatePlacedDescription"));
                if (cpd) {
                    pw = *(int32_t*)((uint8_t*)cpd + 0x44);
                    ph = *(int32_t*)((uint8_t*)cpd + 0x48);
                }
                if (pw <= 0 || ph <= 0) { pw = 8; ph = 8; }
            }
            if (!ptmpl) {
                fprintf(stderr, "[startTown::pasture] BANISHED_BUILD_DEMO_PASTURE set but "
                        "Template\\Pasture.rsc did not resolve; nothing spawned\n");
            } else {
                const int cx = (bbX0 + bbX1) / 2, cy = (bbY0 + bbY1) / 2;
                int bestX = 0, bestY = 0;
                bool found = false;
                for (int r = 1; r <= kExtraHouseSearchRadius && !found; ++r) {
                    for (int dy = -r; dy <= r && !found; ++dy) {
                        for (int dx = -r; dx <= r && !found; ++dx) {
                            if (dx != -r && dx != r && dy != -r && dy != r) continue;
                            const int tx = cx + dx, ty = cy + dy;
                            bool clash = false;
                            for (int o = 0; o < occN; ++o) {
                                if (tx - 1 < occ[o].x + occ[o].w && occ[o].x < tx + pw + 1 &&
                                    ty - 1 < occ[o].y + occ[o].h && occ[o].y < ty + ph + 1) {
                                    clash = true; break;
                                }
                            }
                            if (!clash) { bestX = tx; bestY = ty; found = true; }
                        }
                    }
                }
                if (!found) {
                    fprintf(stderr, "[startTown::pasture] no free %dx%d footprint within "
                            "r=%d of cluster centre; nothing spawned\n",
                            pw, ph, kExtraHouseSearchRadius);
                } else {
                    void* pent = placeStructure(ptmpl, bestX, bestY, 0, pw, ph,
                                                /*isDemoFoundation=*/false, -1);
                    Occupy(bestX, bestY, pw, ph);
                    if (pent) ++spawned;
                    fprintf(stderr, "[startTown::pasture] demo pasture tile=(%d,%d) "
                            "fp=%dx%d ent=%p\n", bestX, bestY, pw, ph, pent);
                }
            }
            fflush(stderr);
        }
    }

    // ── BANISHED_BUILD_DEMO_ORCHARD — one COMPLETED orchard (test scaffolding) ─────
    // Same rationale as BUILD_DEMO_PASTURE directly above: orchards are fenced areas,
    // acquired only through the still-unported fenced-area tool, so without this there
    // is no way to open an orchard panel and exercise OrchardUI (tree-type combo +
    // yield bar). Identical ring scan from the founding-cluster centre.
    //
    // The orchard still needs a TREE TYPE chosen in the panel combo — that selection is
    // exactly what OrchardUI::Slot9 -> Slot11 does, so leaving it unset is the correct
    // starting state for testing rather than something to pre-fill.
    // Value: 1/on = template-default footprint, "WxH" (e.g. "10x8") overrides.
    // Test-only, opt-in, NOT in kDefaultOnFlags; byte-unchanged when unset.
    {
        const char* oe = std::getenv("BANISHED_BUILD_DEMO_ORCHARD");
        if (oe && oe[0] && oe[0] != '0') {
            void* otmpl = ResolveTemplateByPath("Template\\Orchard.rsc");
            int ow = 0, oh = 0;
            if (const char* xpos = std::strchr(oe, 'x')) {   // "WxH" size override
                ow = std::atoi(oe); oh = std::atoi(xpos + 1);
            }
            if (otmpl && (ow <= 0 || oh <= 0)) {
                void* cpd = FindDescriptionOnTemplate(otmpl, TypeId("CreatePlacedDescription"));
                if (cpd) {
                    ow = *(int32_t*)((uint8_t*)cpd + 0x44);
                    oh = *(int32_t*)((uint8_t*)cpd + 0x48);
                }
                // Variable-size like pastures, so the authored default may be 0.
                if (ow <= 0 || oh <= 0) { ow = 8; oh = 8; }
            }
            if (!otmpl) {
                fprintf(stderr, "[startTown::orchard] BANISHED_BUILD_DEMO_ORCHARD set but "
                        "Template\\Orchard.rsc did not resolve; nothing spawned\n");
            } else {
                const int cx = (bbX0 + bbX1) / 2, cy = (bbY0 + bbY1) / 2;
                int bestX = 0, bestY = 0;
                bool found = false;
                for (int r = 1; r <= kExtraHouseSearchRadius && !found; ++r) {
                    for (int dy = -r; dy <= r && !found; ++dy) {
                        for (int dx = -r; dx <= r && !found; ++dx) {
                            if (dx != -r && dx != r && dy != -r && dy != r) continue;
                            const int tx = cx + dx, ty = cy + dy;
                            bool clash = false;
                            for (int o = 0; o < occN; ++o) {
                                if (tx - 1 < occ[o].x + occ[o].w && occ[o].x < tx + ow + 1 &&
                                    ty - 1 < occ[o].y + occ[o].h && occ[o].y < ty + oh + 1) {
                                    clash = true; break;
                                }
                            }
                            if (!clash) { bestX = tx; bestY = ty; found = true; }
                        }
                    }
                }
                if (!found) {
                    fprintf(stderr, "[startTown::orchard] no free %dx%d footprint within "
                            "r=%d of cluster centre; nothing spawned\n",
                            ow, oh, kExtraHouseSearchRadius);
                } else {
                    void* oent = placeStructure(otmpl, bestX, bestY, 0, ow, oh,
                                                /*isDemoFoundation=*/false, -1);
                    Occupy(bestX, bestY, ow, oh);
                    if (oent) ++spawned;
                    fprintf(stderr, "[startTown::orchard] demo orchard tile=(%d,%d) "
                            "fp=%dx%d ent=%p\n", bestX, bestY, ow, oh, oent);
                }
            }
            fflush(stderr);
        }
    }

    // ── BANISHED_BUILD_DEMO_TRADINGPOST — one COMPLETED trading post ON THE RIVER ──
    // Gating harness for the trade / livestock-acquisition wave. A trading post is a
    // DOCK: the merchant-arrival tick (retail 0x14005aa60) clones the TraderBoat entity
    // and hands it to the route builder (0x1400456e0), which walks river path 0 from the
    // map edge to the post's "boat" attach point. So the post MUST sit next to the river.
    // The BUILD_DEMO_PASTURE block above ring-scans from the founding-cluster centre,
    // which would drop the post inland and make the route build fail for reasons that
    // have nothing to do with the port being wrong — an expensive false negative. This
    // block therefore seeds its search from the river polylines instead.
    //
    // River geometry (same array StartTown_SpawnEnvironment pass 1 reads, and the same
    // one the retail route builder uses): worldGeom = gameWorld+0x110; the polyline array
    // is worldGeom+0x1e0 with count worldGeom+0x1e8; each entry is 0x10 bytes
    // {const float* pts (x,y float pairs), int ptCount @ +0x08}. Retail's route builder
    // uses path 0 specifically, so path 0 is tried first here and the rest only as a
    // fallback. Point coords are world-space; tile = (int)(coord + mapSize/2).
    //
    // Value: 1/on = template-default footprint (probe-observed 12x11), "WxH" overrides.
    // Test-only, opt-in, NOT in kDefaultOnFlags; byte-unchanged when unset. Every failure
    // branch prints, so a silent log means this never ran rather than that it succeeded.
    {
        const char* te = std::getenv("BANISHED_BUILD_DEMO_TRADINGPOST");
        if (te && te[0] && te[0] != '0') {
            void* ttmpl = ResolveTemplateByPath("Template\\TradingPost.rsc");
            int tw = 0, th = 0;
            if (const char* xpos = std::strchr(te, 'x')) {      // "WxH" size override
                tw = std::atoi(te); th = std::atoi(xpos + 1);
            }
            if (ttmpl && (tw <= 0 || th <= 0)) {
                void* cpd = FindDescriptionOnTemplate(ttmpl, TypeId("CreatePlacedDescription"));
                if (cpd) {
                    tw = *(int32_t*)((uint8_t*)cpd + 0x44);
                    th = *(int32_t*)((uint8_t*)cpd + 0x48);
                }
                // Observed live via BANISHED_TRADE_PROBE: CreatePlacedDescription 12x11.
                if (tw <= 0 || th <= 0) { tw = 12; th = 11; }
            }
            if (!ttmpl) {
                fprintf(stderr, "[startTown::tradingpost] BANISHED_BUILD_DEMO_TRADINGPOST set "
                        "but Template\\TradingPost.rsc did not resolve; nothing spawned\n");
            } else {
                void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                         ? *g_pGameWorldSingleton : nullptr;
                void* worldGeom = gw ? *(void**)((uint8_t*)gw + 0x110) : nullptr;
                const int mapSize = game ? *(int32_t*)game : 0;
                const int pathCount = worldGeom ? *(int32_t*)((uint8_t*)worldGeom + 0x1e8) : 0;
                uint8_t* pathArr = worldGeom ? *(uint8_t**)((uint8_t*)worldGeom + 0x1e0) : nullptr;

                if (!worldGeom || !pathArr || pathCount <= 0 || mapSize <= 0) {
                    fprintf(stderr, "[startTown::tradingpost] no river geometry "
                            "(worldGeom=%p pathArr=%p pathCount=%d mapSize=%d); nothing "
                            "spawned — the post needs a river-adjacent dock\n",
                            worldGeom, (void*)pathArr, pathCount, mapSize);
                } else {
                    const int half = mapSize / 2;
                    // Diagnostic: the first attempt converted river points to tiles as
                    // (int)(coord + mapSize/2) and found ZERO in-bounds points, so dump
                    // the raw values rather than guess which of mapSize / the coordinate
                    // space / the entry stride is wrong.
                    fprintf(stderr, "[startTown::tradingpost] geom: mapSize=%d half=%d "
                            "pathCount=%d pathArr=%p\n", mapSize, half, pathCount, (void*)pathArr);
                    for (int dp = 0; dp < pathCount && dp < 4; ++dp) {
                        uint8_t* de = pathArr + (size_t)dp * 0x10;
                        const float* dpts = *(const float**)de;
                        const int dn = *(int32_t*)(de + 8);
                        fprintf(stderr, "[startTown::tradingpost]   path[%d] pts=%p n=%d",
                                dp, (const void*)dpts, dn);
                        if (dpts && dn > 0)
                            for (int k = 0; k < dn && k < 4; ++k)
                                fprintf(stderr, "  (%.2f,%.2f)->tile(%d,%d)",
                                        dpts[2*k], dpts[2*k+1],
                                        (int)(dpts[2*k] + (float)half),
                                        (int)(dpts[2*k+1] + (float)half));
                        fprintf(stderr, "\n");
                    }
                    fflush(stderr);
                    // Try path 0 first (the one the retail route builder walks), then the
                    // rest. For each river point, step the footprint outward from the water
                    // in each cardinal direction until it stops clashing with an existing
                    // building — the first fit wins, nearest setback first.
                    //
                    // ⚠ THE SETBACK IS MEASURED FROM THE CENTERLINE, NOT THE BANK.
                    // These polylines are channel CENTERLINES; the water extends
                    // halfWidth + 2.0 tiles either side (fb00's band width). Starting the
                    // search at s=1 therefore placed the post IN THE RIVER — the measured
                    // 2026-07-30 result, tile=(1,217). halfWidth is one of the 20 bytes per
                    // 0x1C PathPoint that the wg+0x1e0 copy deliberately drops (retail drops
                    // them too), so it is read from the SOURCE records via worldgen's side
                    // table rather than by widening that copy.
                    static const int kSetbackRange = 10;   // tiles searched BEYOND the bank
                    // Fallback bank distance when the table has no entry for a path.
                    // 4.0 (coastline) + 2.0 margin is the WIDEST case, so this errs toward
                    // dry land: too far inland only costs dock adjacency, whereas too near
                    // puts the post under water and fails for a misleading reason.
                    static const int kBankFallback = 6;
                    int bestX = 0, bestY = 0, bestSetback = -1, bestPath = -1, bestPt = -1;
                    // The chosen ORIENTATION, and the footprint extents that go with it.
                    // Every other demo building in this file is placed at rotation 0; the
                    // trading post is the one that cannot be, because its dock is a fixed
                    // offset in the building's LOCAL frame — see the rotation note below.
                    int bestRot = 0, bestFpW = tw, bestFpH = th;
                    bool found = false;
                    int pointsSeen = 0;

                    // ⚠ SELECTION IS NEAREST-TO-TOWN, NOT FIRST-FIT.
                    // This used to take the first fit in path order, "path 0 first,
                    // because that is the one the retail route builder walks". Measured
                    // result: the post landed on the far side of the water, most of the
                    // map from the town — unusable for testing the panel, because no
                    // citizen will ever walk to it. Two reasons the old rule was wrong:
                    //   * path 0 is the COASTLINE RING, not a river (confirmed by its
                    //     halfWidth of 4.00 vs 1.50 for the others), so "first point on
                    //     path 0" is a point on the map edge.
                    //   * even on a real river, the first in-bounds point is an artifact
                    //     of polyline winding order, not of proximity to anything.
                    // So scan every candidate and keep the one closest to the town
                    // cluster centre. Path 0 is still preferred on ties only.
                    const int townCx = (bbX0 + bbX1) / 2, townCy = (bbY0 + bbY1) / 2;
                    long long bestDist = -1;       // ranking score (may carry penalties)
                    long long bestTrueDist = -1;   // the honest distance, for the log
                    bool bestPierOk = false;
                    int  bestPierCell = -1;
                    // TEST HARNESS, opt-in: BANISHED_TRADINGPOST_PATH=<n> considers only
                    // polyline n. The retail route builder A*s from polyline 0 to the dock,
                    // so "nearest to town" siting can (and by default does) put the post on
                    // a channel the boat never enters — which reads as a broken route
                    // builder when it is really a broken test fixture. =0 is the setup that
                    // actually exercises the arrival path. Unset = unchanged behaviour.
                    int onlyPath = -1;
                    if (const char* pe = std::getenv("BANISHED_TRADINGPOST_PATH")) {
                        if (pe[0]) onlyPath = std::atoi(pe);
                    }

                    // ── THE ACCEPTANCE TEST: can a boat actually REACH this dock? ──
                    // Measured 2026-08-22: siting by setback alone produced a post whose
                    // pier landed in an 867-cell POND — water, navigable, and in a
                    // different connected component from the river the boat arrives on.
                    // Both A* legs correctly returned 0 and the route builder took the
                    // blame for a fixture that never checked where the pier ended up.
                    //
                    // Retail does NOT protect against this: its placement comparator
                    // (0x14009f5d0) is a per-cell bit test on cell+0x29 plus bounds and
                    // occupancy, with no connectivity check anywhere — a player can put a
                    // trading post on a pond in retail too and its boat will never come.
                    // So this is not a missing retail rule being reimplemented; it is a
                    // TEST FIXTURE choosing a site that exercises the arrival path.
                    //
                    // Method: flood the water from the cell where polyline 0 enters the
                    // map (exactly where the route builder starts its first A* leg), then
                    // require a candidate's PIER CELL to be in that flood. One flood, then
                    // an O(1) test per candidate — running an A* per candidate would be
                    // thousands of searches.
                    //
                    // Nav data: map+0x80 = fine-cell array, stride 0x30, cell+0x29 low 5
                    // bits = terrain class; boat-passable classes are mask 0x380,
                    // 8-neighbour (0x1400a5ec0's admissibility test). The searcher keeps
                    // its own copy of these bytes and it is a verified byte-exact mirror.
                    int* navMap = (gw && *(void**)((uint8_t*)gw + 0xf8))
                                ? *(int**)((uint8_t*)gw + 0xf8) : nullptr;
                    uintptr_t cellBase = navMap
                                       ? *(uintptr_t*)((uint8_t*)navMap + 0x80) : 0;
                    std::vector<uint8_t> reach;          // empty = test unavailable
                    int entryCell = -1;
                    if (cellBase && mapSize > 0) {
                        // Where polyline 0 enters the map — the route builder's own start.
                        uint8_t* p0 = pathArr;
                        const float* p0pts = *(const float**)p0;
                        const int p0n = *(int32_t*)(p0 + 8);
                        for (int i = 0; i < p0n && p0pts; ++i) {
                            const int ex = (int)(p0pts[2 * i]     + (float)half);
                            const int ey = (int)(p0pts[2 * i + 1] + (float)half);
                            if (ex >= 0 && ex < mapSize && ey >= 0 && ey < mapSize) {
                                entryCell = ey * mapSize + ex;
                                break;
                            }
                        }
                    }
                    if (entryCell >= 0) {
                        const uint32_t kBoatMask = 0x380u;
                        auto navigable = [&](int c) {
                            const unsigned cls =
                                *(uint8_t*)(cellBase + (uintptr_t)c * 0x30 + 0x29) & 0x1fu;
                            return ((1u << cls) & kBoatMask) != 0;
                        };
                        if (navigable(entryCell)) {
                            reach.assign((size_t)mapSize * mapSize, 0);
                            std::vector<int> stack;
                            stack.push_back(entryCell);
                            reach[entryCell] = 1;
                            while (!stack.empty()) {
                                const int c = stack.back(); stack.pop_back();
                                const int cx2 = c % mapSize, cy2 = c / mapSize;
                                for (int dy = -1; dy <= 1; ++dy)
                                    for (int dx = -1; dx <= 1; ++dx) {
                                        if (!dx && !dy) continue;
                                        const int nx = cx2 + dx, ny = cy2 + dy;
                                        if (nx < 0 || ny < 0 || nx >= mapSize || ny >= mapSize)
                                            continue;
                                        const int nc = ny * mapSize + nx;
                                        if (reach[nc] || !navigable(nc)) continue;
                                        reach[nc] = 1;
                                        stack.push_back(nc);
                                    }
                            }
                        }
                    }

                    // The pier's offset, read from the TEMPLATE rather than hardcoded:
                    // InteractDescription -> +0x20 -> { +0x30 entries, +0x38 count },
                    // stride 0x18 = { String name @+0x00, Vector3 localPos @+0x08,
                    // float localYaw @+0x14 } — the same table retail's 0x140123880 walks.
                    // Every demo post is placed with rotation 0, so the offset needs no
                    // rotation; the entity origin is the footprint CENTRE, which is what
                    // makes the prediction below reproduce the measured attach point
                    // exactly (post tile (112,107) fp 12x11 + localPos (1.66,-6.53) ->
                    // world (-8.34,-22.03) -> tile (119,105), the value the route builder
                    // independently resolved).
                    bool havePier = false;
                    float pierX = 0.0f, pierY = 0.0f;
                    {
                        void* idesc = FindDescriptionOnTemplate(ttmpl, TypeId("InteractDescription"));
                        const uintptr_t tbl = idesc ? *(uintptr_t*)((uint8_t*)idesc + 0x20) : 0;
                        const int aCount = tbl ? *(int32_t*)(tbl + 0x38) : 0;
                        for (int i = 0; i < aCount; ++i) {
                            const uintptr_t e = *(uintptr_t*)(tbl + 0x30) + (uintptr_t)i * 0x18;
                            uint8_t nameBuf[8] = {};
                            if (!fn_String_Ctor_CStr || !fn_String_Equal) break;
                            fn_String_Ctor_CStr(nameBuf, "boat");
                            const bool hit = fn_String_Equal((const void*)e, nameBuf);
                            if (fn_String_Dtor) fn_String_Dtor(nameBuf);
                            if (!hit) continue;
                            pierX = *(const float*)(e + 0x08);
                            pierY = *(const float*)(e + 0x0c);
                            havePier = true;
                            break;
                        }
                    }
                    // Always print, including the failure arms: with no flood and no pier
                    // offset this block silently degrades to the old setback ranking, and
                    // that must never look like it ran the acceptance test.
                    fprintf(stderr, "[startTown::tradingpost] dock reachability: entryCell=%d"
                            " floodCells=%d pierOffset=%s(%.2f,%.2f)%s\n",
                            entryCell, (int)std::count(reach.begin(), reach.end(), (uint8_t)1),
                            havePier ? "" : "NONE ", pierX, pierY,
                            (reach.empty() || !havePier)
                              ? "   <-- ACCEPTANCE TEST OFF: siting falls back to setback/"
                                "distance only, and the chosen dock may be unreachable"
                              : "");

                    for (int p = 0; p < pathCount; ++p) {
                        if (onlyPath >= 0 && p != onlyPath) continue;
                        uint8_t* entry = pathArr + (size_t)p * 0x10;
                        const float* pts = *(const float**)entry;
                        const int ptCount = *(int32_t*)(entry + 8);
                        if (!pts || ptCount < 2) continue;

                        // First DRY tile off this path's centerline. -1 = no data.
                        const float bankF = WorldGen_GetPathBankDistance(p);
                        const int sMin = (bankF < 0.0f)
                                       ? kBankFallback
                                       : (int)std::ceil(bankF) + 1;   // +1 = clear of the band
                        fprintf(stderr, "[startTown::tradingpost]   path[%d] halfWidth=%.2f "
                                "bank=%.2f -> setback starts at s=%d%s\n",
                                p, WorldGen_GetPathHalfWidth(p), bankF, sMin,
                                bankF < 0.0f ? "  (NO halfWidth DATA — using fallback)" : "");

                        for (int j = 0; j < ptCount; ++j) {
                            const int rx = (int)(pts[2 * j]     + (float)half);
                            const int ry = (int)(pts[2 * j + 1] + (float)half);
                            if (rx < 0 || ry < 0 || rx >= mapSize || ry >= mapSize) continue;
                            ++pointsSeen;

                            for (int s = sMin; s <= sMin + kSetbackRange; ++s) {
                              // ── ROTATION ─────────────────────────────────────────────
                              // Rotation 0 alone cannot site this post on the main channel.
                              // The pier offset is fixed in the building's LOCAL frame
                              // (~(1.66,-6.53), pointing one way), so at a fixed rotation
                              // the setback search can only step the footprint N/S/E/W —
                              // it can never TURN the building to face the water. Measured
                              // 2026-08-22 with BANISHED_TRADINGPOST_PATH=0: no candidate on
                              // polyline 0 passed the reachability test at all, because the
                              // fixed pier direction points inland off the coastline ring.
                              //
                              // So try all four orientations per candidate and rotate the
                              // pier offset with them. placeStructure already takes the
                              // orientation (in DEGREES, raw — it is handed straight to
                              // Transform::FromYawPitchOffset as the yaw) and swaps the
                              // footprint axes for 90/270; nothing in the demo block used it
                              // until now.
                              //
                              // The rotation sense of the offset is NOT read off retail — the
                              // pier's world position comes from `modelTransform * localPos`
                              // (boatcomponent_route.cpp, 0x140123880) and we do not
                              // replicate that matrix here, we apply the CCW-about-+Z
                              // rotation below. It is MEASURED instead, and the measurement
                              // is the log pair this block already prints:
                              //   2026-08-22, seed 12345, chosen rot=90 -> predicted pier
                              //   world (49.00,-1.00); the route builder independently
                              //   resolved the dock at (49.03,-0.34).
                              // Sub-tile agreement on a 90 placement is what pins the sense —
                              // a wrong sign would put 90 where 270 belongs, tiles away and
                              // usually on dry land. If that pair ever disagrees on a 90/270
                              // placement, negate the sine terms in kPierRot.
                              static const int   kRotDeg[4]    = { 0, 90, 180, 270 };
                              static const float kPierRot[4][4] = {   // {xx,xy,yx,yy}
                                  {  1.0f,  0.0f,  0.0f,  1.0f },     //   0
                                  {  0.0f, -1.0f,  1.0f,  0.0f },     //  90 CCW
                                  { -1.0f,  0.0f,  0.0f, -1.0f },     // 180
                                  {  0.0f,  1.0f, -1.0f,  0.0f },     // 270
                              };
                              for (int ri = 0; ri < 4; ++ri) {
                                const int rotDeg = kRotDeg[ri];
                                // placeStructure swaps the axes for 90/270 (and only those);
                                // mirror that here so the footprint we test is the footprint
                                // it stamps.
                                const int fpW = (rotDeg == 90 || rotDeg == 270) ? th : tw;
                                const int fpH = (rotDeg == 90 || rotDeg == 270) ? tw : th;
                                const float prX = kPierRot[ri][0] * pierX + kPierRot[ri][1] * pierY;
                                const float prY = kPierRot[ri][2] * pierX + kPierRot[ri][3] * pierY;
                                // 4 cardinals: post footprint placed s tiles off the water.
                                const int cand[4][2] = {
                                    { rx + s,      ry - fpH / 2 },   // east of the river
                                    { rx - s - fpW, ry - fpH / 2 },  // west
                                    { rx - fpW / 2, ry + s      },   // south
                                    { rx - fpW / 2, ry - s - fpH },  // north
                                };
                                for (int d = 0; d < 4; ++d) {
                                    const int tx = cand[d][0], ty = cand[d][1];
                                    if (tx < 1 || ty < 1 ||
                                        tx + fpW >= mapSize - 1 || ty + fpH >= mapSize - 1)
                                        continue;
                                    bool clash = false;
                                    for (int o = 0; o < occN; ++o) {
                                        if (tx - 1 < occ[o].x + occ[o].w && occ[o].x < tx + fpW + 1 &&
                                            ty - 1 < occ[o].y + occ[o].h && occ[o].y < ty + fpH + 1) {
                                            clash = true; break;
                                        }
                                    }
                                    if (clash) continue;
                                    // Distance from the footprint centre to the town
                                    // centre. Strictly-less keeps the earliest path on a
                                    // tie, preserving the old path-0 preference where it
                                    // costs nothing — and, now, the earliest ROTATION, so a
                                    // run that never needed to turn the building still
                                    // places it at 0 exactly as before.
                                    const long long ddx = (long long)(tx + fpW / 2) - townCx;
                                    const long long ddy = (long long)(ty + fpH / 2) - townCy;
                                    const long long trueDist = ddx * ddx + ddy * ddy;
                                    long long dist = trueDist;
                                    // Where this candidate's pier would land, and whether a
                                    // boat can get there from the map entry.
                                    bool pierOk = false;
                                    int pierCell = -1;
                                    if (!reach.empty() && havePier) {
                                        const int px = (int)((float)tx + (float)fpW * 0.5f + prX);
                                        const int py = (int)((float)ty + (float)fpH * 0.5f + prY);
                                        if (px >= 0 && py >= 0 && px < mapSize && py < mapSize) {
                                            pierCell = py * mapSize + px;
                                            pierOk = reach[pierCell] != 0;
                                        }
                                    }
                                    // Reachable beats near. Any unreachable candidate is
                                    // still eligible — a fixture that spawns nothing is
                                    // worse than one that spawns a post and says it is not
                                    // dockable — but it always loses to a reachable one.
                                    if (!reach.empty() && havePier && !pierOk)
                                        dist += 1000000000000LL;
                                    // With BANISHED_TRADINGPOST_PATH set, rank by SETBACK
                                    // first and distance only as a tie-break. Nearest-to-
                                    // town on its own picks the most INLAND fit, because
                                    // stepping away from the water is stepping toward the
                                    // town — measured 2026-08-22: setback 17, dock 10 tiles
                                    // from the nearest water, and both A* legs correctly
                                    // returned 0. A dock the boat cannot reach makes the
                                    // route builder look broken when it is doing exactly
                                    // what it should.
                                    if (onlyPath >= 0)
                                        dist = (long long)s * 1000000LL + (ddx * ddx + ddy * ddy);
                                    if (bestDist >= 0 && dist >= bestDist) continue;
                                    bestDist = dist;
                                    bestTrueDist = trueDist;
                                    bestPierOk = pierOk;
                                    bestPierCell = pierCell;
                                    bestX = tx; bestY = ty; bestSetback = s;
                                    bestRot = rotDeg; bestFpW = fpW; bestFpH = fpH;
                                    bestPath = p; bestPt = j; found = true;
                                }
                              }
                            }
                        }
                    }

                    // FALLBACK — no usable river geometry. Measured 2026-07-30: the
                    // worldGeom+0x1e0 array has 4 entries but every one is {pts=NULL, n=0},
                    // at world-build time AND at frame 900. That is not a timing problem;
                    // worldgen.cpp:598 only RESIZES detail-group-C (its own comment: "no-op
                    // for first-light: hf detailCountA == 0 ... feature-record append loop
                    // (empty -> skipped)"), so standalone never generates the polylines.
                    //
                    // Consequence well beyond this harness: retail's boat-route builder
                    // (0x1400456e0) walks this same array, so until worldgen fills it a
                    // merchant boat has no river to follow. Placing the post inland is
                    // therefore no worse than placing it on a "river" that does not exist,
                    // and it unblocks everything that does not need the boat: the TradeUI
                    // panel and the post's WorkPlaceComponent accepting state.
                    // Ring-scan from the cluster centre exactly as BUILD_DEMO_PASTURE does,
                    // and say loudly that this post is NOT dockable.
                    if (!found) {
                        const int cx = (bbX0 + bbX1) / 2, cy = (bbY0 + bbY1) / 2;
                        for (int r = 1; r <= kExtraHouseSearchRadius && !found; ++r) {
                            for (int dy = -r; dy <= r && !found; ++dy) {
                                for (int dx = -r; dx <= r && !found; ++dx) {
                                    if (dx != -r && dx != r && dy != -r && dy != r) continue;
                                    const int tx = cx + dx, ty = cy + dy;
                                    if (tx < 1 || ty < 1 ||
                                        tx + tw >= mapSize - 1 || ty + th >= mapSize - 1) continue;
                                    bool clash = false;
                                    for (int o = 0; o < occN; ++o) {
                                        if (tx - 1 < occ[o].x + occ[o].w && occ[o].x < tx + tw + 1 &&
                                            ty - 1 < occ[o].y + occ[o].h && occ[o].y < ty + th + 1) {
                                            clash = true; break;
                                        }
                                    }
                                    if (!clash) { bestX = tx; bestY = ty; found = true; }
                                }
                            }
                        }
                        if (found)
                            fprintf(stderr, "[startTown::tradingpost] NO RIVER GEOMETRY "
                                    "(%d path(s), all empty) — fell back to a cluster-centre "
                                    "placement. The post will NOT be dockable: the retail boat "
                                    "route builder needs worldGeom+0x1e0 populated, which "
                                    "standalone worldgen never does. Fine for TradeUI / "
                                    "accepting tests, NOT for the merchant-arrival wave.\n",
                                    pathCount);
                    }

                    if (!found) {
                        fprintf(stderr, "[startTown::tradingpost] no free %dx%d footprint near "
                                "any river point (%d path(s), %d in-bounds point(s)) NOR within "
                                "r=%d of the cluster centre; nothing spawned\n",
                                tw, th, pathCount, pointsSeen, kExtraHouseSearchRadius);
                    } else {
                        // placeStructure takes the pre-rotation footprint (w0,h0) and swaps
                        // the axes itself; Occupy needs the POST-rotation extents.
                        void* tent = placeStructure(ttmpl, bestX, bestY, bestRot, tw, th,
                                                    /*isDemoFoundation=*/false, -1);
                        Occupy(bestX, bestY, bestFpW, bestFpH);
                        if (tent) ++spawned;
                        if (bestPath >= 0)
                            fprintf(stderr, "[startTown::tradingpost] demo trading post "
                                    "tile=(%d,%d) fp=%dx%d rot=%d setback=%d from river path[%d] "
                                    "point[%d] townCentre=(%d,%d) dist=%.1f ent=%p%s\n",
                                    bestX, bestY, bestFpW, bestFpH, bestRot, bestSetback,
                                    bestPath, bestPt,
                                    townCx, townCy,
                                    bestTrueDist >= 0 ? std::sqrt((double)bestTrueDist) : -1.0,
                                    tent,
                                    bestPierOk
                                      ? "  DOCK REACHABLE (pier is in the polyline-0 water body)"
                                      : (reach.empty() || !havePier)
                                        ? "  dock reachability UNTESTED"
                                        : "  <-- DOCK NOT REACHABLE: the pier is not in the "
                                          "water body the boat arrives on, so both A* legs "
                                          "will return 0 and the route will be polyline-only");
                        if (bestPierCell >= 0)
                            fprintf(stderr, "[startTown::tradingpost]   predicted pier cell "
                                    "(%d,%d) world(%.2f,%.2f) reachable=%s — compare against "
                                    "the route builder's own [BoatRoute] dock= line\n",
                                    bestPierCell % mapSize, bestPierCell / mapSize,
                                    (float)(bestPierCell % mapSize) - (float)half,
                                    (float)(bestPierCell / mapSize) - (float)half,
                                    bestPierOk ? "yes" : "NO");
                        else
                            fprintf(stderr, "[startTown::tradingpost] demo trading post "
                                    "tile=(%d,%d) fp=%dx%d rot=%d ent=%p  (cluster-centre "
                                    "fallback, NOT river-adjacent)\n",
                                    bestX, bestY, bestFpW, bestFpH, bestRot, tent);
                    }
                }
            }
            fflush(stderr);
        }
    }

    // ── Food-production demo buildings — CROP FIELD / HUNTER LODGE / GATHERER HUT ──
    // Same rationale as BUILD_DEMO_PASTURE/ORCHARD above: all three are acquired only
    // through tools that are still unported (drag-area for the crop field, placement
    // for the two lodges), so without this there is no way to exercise their
    // components or panels at all.
    //
    // Crop field is a ControllerComponent area like the orchard; hunter lodge and
    // gatherer shelter are ResourceProduceComponent buildings sharing the state
    // machine in resourceproducecomponent_states.cpp. Pair these with
    // BANISHED_LIVE_HUNTING / BANISHED_LIVE_GATHERER (and BANISHED_LIVE_CROPFIELD)
    // — spawning the building alone leaves the component inert.
    //
    // Value: 1/on = template-default footprint, "WxH" (e.g. "10x8") overrides.
    // Test-only, opt-in, NOT in kDefaultOnFlags; byte-unchanged when unset.
    {
        // The ring scan the pasture/orchard blocks above each open-code. Returns
        // false (and says so) when nothing fits, so a silent log means "never ran".
        // The workability predicate it applies per candidate is the file-scope
        // ST_SiteIsWorkable above (hoisted so the deferred hunter-lodge placement
        // applies the identical test).
        auto ringPlace = [&](const char* envVar, const char* tmplPath, const char* tag,
                             int fallbackW, int fallbackH) {
            const char* ev = std::getenv(envVar);
            if (!ev || !ev[0] || ev[0] == '0') return;

            void* tmpl = ResolveTemplateByPath(tmplPath);
            int w = 0, h = 0;
            if (const char* xpos = std::strchr(ev, 'x')) {      // "WxH" size override
                w = std::atoi(ev); h = std::atoi(xpos + 1);
            }
            if (tmpl && (w <= 0 || h <= 0)) {
                void* cpd = FindDescriptionOnTemplate(tmpl, TypeId("CreatePlacedDescription"));
                if (cpd) {
                    w = *(int32_t*)((uint8_t*)cpd + 0x44);
                    h = *(int32_t*)((uint8_t*)cpd + 0x48);
                }
                // Drag-created areas (the crop field) carry a CreateDragDescription
                // instead, so there is no authored footprint to read — hence the
                // caller-supplied fallback rather than a hard-coded one here.
                if (w <= 0 || h <= 0) { w = fallbackW; h = fallbackH; }
            }
            if (!tmpl) {
                fprintf(stderr, "[startTown::%s] %s set but %s did not resolve; "
                        "nothing spawned\n", tag, envVar, tmplPath);
                fflush(stderr);
                return;
            }

            const int cx = (bbX0 + bbX1) / 2, cy = (bbY0 + bbY1) / 2;
            int bestX = 0, bestY = 0;
            bool found = false;
            for (int r = 1; r <= kExtraHouseSearchRadius && !found; ++r) {
                for (int dy = -r; dy <= r && !found; ++dy) {
                    for (int dx = -r; dx <= r && !found; ++dx) {
                        if (dx != -r && dx != r && dy != -r && dy != r) continue;
                        const int tx = cx + dx, ty = cy + dy;
                        bool clash = false;
                        for (int o = 0; o < occN; ++o) {
                            if (tx - 1 < occ[o].x + occ[o].w && occ[o].x < tx + w + 1 &&
                                ty - 1 < occ[o].y + occ[o].h && occ[o].y < ty + h + 1) {
                                clash = true; break;
                            }
                        }
                        if (clash) continue;
                        // Every footprint cell must be WORKABLE, not merely unoccupied.
                        // The player's placement tool enforces this in retail; this ring
                        // scan did not, so a demo crop field could land with one corner on
                        // terrain that Map_FindNearestPassable rejects. That one cell can
                        // never be planted, `growing + done == total` never holds, and the
                        // field parks one slot short of state 10 forever -- a harness
                        // siting fault that reads exactly like a port bug. Measured:
                        // blocked cell terrain byte 0x07 against 0x04 on its working
                        // neighbour. Tested with the SAME predicate PlantArea uses, so a
                        // site that passes here plants every cell.
                        if (!ST_SiteIsWorkable(tx, ty, w, h)) continue;
                        { bestX = tx; bestY = ty; found = true; }
                    }
                }
            }
            if (!found) {
                fprintf(stderr, "[startTown::%s] no free %dx%d footprint within r=%d of "
                        "cluster centre; nothing spawned\n", tag, w, h,
                        kExtraHouseSearchRadius);
            } else {
                void* ent = placeStructure(tmpl, bestX, bestY, 0, w, h,
                                           /*isDemoFoundation=*/false, -1);
                Occupy(bestX, bestY, w, h);
                if (ent) ++spawned;
                fprintf(stderr, "[startTown::%s] demo %s tile=(%d,%d) fp=%dx%d ent=%p\n",
                        tag, tag, bestX, bestY, w, h, ent);
            }
            fflush(stderr);
        };

        ringPlace("BANISHED_BUILD_DEMO_CROPFIELD", "Template\\CropField.rsc",
                  "cropfield", 8, 8);

        // ── the four NON-food workplaces that carry the panel work checkbox ────────
        // Added 2026-08-15 so the slot-13/14 wave is measurable: the coverage probe in
        // food_placement_harness.cpp reads a component's LIVE vtable, and a per-type
        // vtable is only filled by the first factory call for that type — so with no
        // instance in the world there is literally nothing to measure. All four are
        // ordinary placed structures with an authored CreatePlacedDescription, so the
        // generic ring scan handles them and the fallback footprints are never used.
        ringPlace("BANISHED_BUILD_DEMO_FORESTERLODGE", "Template\\ForesterLodge.rsc",
                  "foresterlodge", 6, 6);
        ringPlace("BANISHED_BUILD_DEMO_MARKET", "Template\\Market.rsc",
                  "market", 10, 10);
        ringPlace("BANISHED_BUILD_DEMO_SCHOOLHOUSE", "Template\\SchoolHouse.rsc",
                  "schoolhouse", 6, 6);
        // Tavern is the ConsumeProduceComponent carrier (wheat -> ale). If the probe
        // reports ConsumeProduceComponent NOT PLACED with a tavern in the world, the
        // component lives on a different template — Tailor/ToolMaker are the next
        // candidates — and the table entry, not the port, is what needs correcting.
        ringPlace("BANISHED_BUILD_DEMO_TAVERN", "Template\\Tavern.rsc",
                  "tavern", 6, 6);
        // ── HUNTER LODGE — REQUESTED HERE, PLACED LATER. ──
        // Not ring-placed like the crop field, and for a sharper reason than the
        // gatherer hut's: the lodge's siting input does not exist yet at all. The only
        // thing that decides whether a lodge can ever produce is how many wild animals
        // are inside its work radius, and the index that answers that — the character
        // spatial-hash manager at gw+0x88 — is empty during world build because every
        // character registers itself during its own Startup. A ring scan from the
        // cluster centre put the lodge at tile (136,110), which is town, not deer
        // country; with slot 9 and the cellIdx fix live the hunt then ran 19 000+ times
        // and returned noTarget every single time, nearestTyped 30.85..44.10 against a
        // kill radius of 8.0. Nothing was broken — the hunter never got near a deer.
        //
        // So record the request and let StartTown_DeferredSpawn_Tick place it a few
        // hundred frames in, scoring candidates by live animal density. Set
        // BANISHED_HUNTERLODGE_AT_START=1 to take the old cluster-centre ring placement
        // instead (the A/B control for "did density siting actually matter?").
        {
            const char* he = std::getenv("BANISHED_BUILD_DEMO_HUNTERLODGE");
            const char* atStart = std::getenv("BANISHED_HUNTERLODGE_AT_START");
            const bool wantAtStart = (atStart && atStart[0] && atStart[0] != '0');
            if (wantAtStart) {
                ringPlace("BANISHED_BUILD_DEMO_HUNTERLODGE", "Template\\HunterLodge.rsc",
                          "hunterlodge", 8, 8);
            } else if (he && he[0] && he[0] != '0') {
                void* htmpl = ResolveTemplateByPath("Template\\HunterLodge.rsc");
                int hw = 0, hh = 0;
                if (const char* xpos = std::strchr(he, 'x')) {   // "WxH" size override
                    hw = std::atoi(he); hh = std::atoi(xpos + 1);
                }
                int hRadius = 0;
                if (htmpl) {
                    if (hw <= 0 || hh <= 0) {
                        if (void* cpd = FindDescriptionOnTemplate(
                                htmpl, TypeId("CreatePlacedDescription"))) {
                            hw = *(int32_t*)((uint8_t*)cpd + 0x44);
                            hh = *(int32_t*)((uint8_t*)cpd + 0x48);
                        }
                        if (hw <= 0 || hh <= 0) { hw = 8; hh = 8; }
                    }
                    if (void* rd = FindDescriptionOnTemplate(htmpl, TypeId("RadiusDescription")))
                        hRadius = *(int32_t*)((uint8_t*)rd + 0x28);
                }
                if (!htmpl) {
                    fprintf(stderr, "[startTown::hunterlodge] BANISHED_BUILD_DEMO_HUNTERLODGE "
                            "set but Template\\HunterLodge.rsc did not resolve; nothing "
                            "spawned\n");
                } else {
                    const int cx = (bbX0 + bbX1) / 2, cy = (bbY0 + bbY1) / 2;
                    g_deferredLodge.wanted     = true;
                    g_deferredLodge.tmpl       = htmpl;
                    g_deferredLodge.w          = hw;
                    g_deferredLodge.h          = hh;
                    g_deferredLodge.townCx     = cx;
                    g_deferredLodge.townCy     = cy;
                    g_deferredLodge.workRadius = hRadius > 0 ? hRadius : 34;
                    if (const char* f = std::getenv("BANISHED_HUNTERLODGE_FRAME")) {
                        int v = std::atoi(f);
                        if (v > 0) g_deferredLodge.fireFrame = v;
                    }
                    fprintf(stderr, "[startTown::hunterlodge] DEFERRED to frame %d — fp=%dx%d "
                            "workRadius=%d townCentre=(%d,%d). The animal index at gw+0x88 is "
                            "empty during world build, so density scoring is only meaningful "
                            "once the characters have started up.\n",
                            g_deferredLodge.fireFrame, hw, hh, g_deferredLodge.workRadius,
                            cx, cy);
                }
                fflush(stderr);
            }
        }
        // The Gatherer's Hut is NOT ring-placeable for the same reason the fishing dock
        // is not. HerbalistWork::Slot4 only accepts a resource whose entity+0x28 is in the
        // hut's own accept list (HerbalistDescription+0x20, count +0x28) — four wild-food
        // types. MEASURED at the cluster centre: 605 resource cells in radius 30 and
        // GATHERABLE=0, because those cells are trees and stone, not berries or roots. The
        // hut then forages forever and produces nothing, which is indistinguishable from a
        // broken port. (An all-resources count does NOT answer this — it was 851 and read
        // as "well sited".) So score candidates by the accept list itself, exactly as the
        // dock block scores by its terrain mask.
        {
            const char* ge = std::getenv("BANISHED_BUILD_DEMO_GATHERERHUT");
            if (ge && ge[0] && ge[0] != '0') {
                void* gtmpl = ResolveTemplateByPath("Template\\GathererShelter.rsc");
                int gw_ = 0, gh_ = 0;
                if (const char* xpos = std::strchr(ge, 'x')) {
                    gw_ = std::atoi(ge); gh_ = std::atoi(xpos + 1);
                }
                uintptr_t* acceptList = nullptr;
                int acceptN = 0, gRadius = 0;
                if (gtmpl) {
                    if (gw_ <= 0 || gh_ <= 0) {
                        void* cpd = FindDescriptionOnTemplate(gtmpl, TypeId("CreatePlacedDescription"));
                        if (cpd) {
                            gw_ = *(int32_t*)((uint8_t*)cpd + 0x44);
                            gh_ = *(int32_t*)((uint8_t*)cpd + 0x48);
                        }
                        if (gw_ <= 0 || gh_ <= 0) { gw_ = 8; gh_ = 8; }
                    }
                    if (void* hd = FindDescriptionOnTemplate(gtmpl, TypeId("HerbalistDescription"))) {
                        acceptList = *(uintptr_t**)((uint8_t*)hd + 0x20);
                        acceptN    = *(int32_t*)((uint8_t*)hd + 0x28);
                    }
                    if (void* rd = FindDescriptionOnTemplate(gtmpl, TypeId("RadiusDescription")))
                        gRadius = *(int32_t*)((uint8_t*)rd + 0x28);
                }

                void* gws = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                          ? *g_pGameWorldSingleton : nullptr;
                uint8_t* mapObj = gws ? *(uint8_t**)((uint8_t*)gws + 0xf8) : nullptr;
                const int width = mapObj ? *(int32_t*)mapObj : 0;
                uint8_t* grid = mapObj ? *(uint8_t**)(mapObj + 0x80) : nullptr;

                if (!gtmpl) {
                    fprintf(stderr, "[startTown::gathererhut] BANISHED_BUILD_DEMO_GATHERERHUT set "
                            "but Template\\GathererShelter.rsc did not resolve; nothing spawned\n");
                } else if (!grid || width <= 0 || !acceptList || acceptN <= 0 || gRadius <= 0) {
                    fprintf(stderr, "[startTown::gathererhut] cannot score placement "
                            "(grid=%p width=%d acceptList=%p acceptN=%d radius=%d) — placing blind "
                            "would only produce a hut with nothing to forage; nothing spawned\n",
                            (void*)grid, width, (void*)acceptList, acceptN, gRadius);
                } else {
                    const int townCx = (bbX0 + bbX1) / 2, townCy = (bbY0 + bbY1) / 2;
                    const int kWindow = 90, kStride = 2;
                    int bestX = 0, bestY = 0, bestScore = 0;
                    long long bestDist = -1;
                    bool found = false;
                    for (int ty = townCy - kWindow; ty <= townCy + kWindow; ty += kStride) {
                        for (int tx = townCx - kWindow; tx <= townCx + kWindow; tx += kStride) {
                            if (tx < 1 || ty < 1 ||
                                tx + gw_ >= width - 1 || ty + gh_ >= width - 1) continue;
                            bool clash = false;
                            for (int o = 0; o < occN; ++o) {
                                if (tx - 1 < occ[o].x + occ[o].w && occ[o].x < tx + gw_ + 1 &&
                                    ty - 1 < occ[o].y + occ[o].h && occ[o].y < ty + gh_ + 1) {
                                    clash = true; break;
                                }
                            }
                            if (clash) continue;

                            const int cx = tx + gw_ / 2, cy = ty + gh_ / 2;
                            int score = 0;
                            for (int dy = -gRadius; dy < gRadius; ++dy)
                                for (int dx = -gRadius; dx < gRadius; ++dx) {
                                    if (dx * dx + dy * dy > gRadius * gRadius) continue;
                                    const int x = cx + dx, y = cy + dy;
                                    if (x < 0 || y < 0 || x >= width || y >= width) continue;
                                    uintptr_t re = *(uintptr_t*)(grid + (size_t)(y * width + x) * 0x30 + 8);
                                    if (!re) continue;
                                    const uintptr_t rd = *(uintptr_t*)(re + 0x28);
                                    for (int t = 0; t < acceptN; ++t)
                                        if (acceptList[t] == rd) { ++score; break; }
                                }
                            if (score == 0) continue;
                            const long long ddx = (long long)cx - townCx;
                            const long long ddy = (long long)cy - townCy;
                            const long long dist = ddx * ddx + ddy * ddy;
                            if (score < bestScore) continue;
                            if (score == bestScore && bestDist >= 0 && dist >= bestDist) continue;
                            bestScore = score; bestDist = dist;
                            bestX = tx; bestY = ty; found = true;
                        }
                    }
                    if (!found) {
                        // A zero score has two very different causes — "this map has no
                        // wild food near the start" and "the key being compared is wrong" —
                        // and they are indistinguishable from the count alone. Dump BOTH
                        // sides: the hut's accept entries and the distinct resource keys
                        // actually present, so the next reader can tell which it is.
                        fprintf(stderr, "[startTown::gathererhut] no %dx%d footprint within %d "
                                "tiles of the town centre has ANY of the hut's %d accepted "
                                "resource types in radius %d; nothing spawned\n",
                                gw_, gh_, kWindow, acceptN, gRadius);
                        for (int t = 0; t < acceptN; ++t)
                            fprintf(stderr, "[startTown::gathererhut]   acceptList[%d] = %p\n",
                                    t, (void*)acceptList[t]);
                        uintptr_t seen[12] = {0};
                        int seenN = 0, sampled = 0;
                        for (int y = townCy - kWindow; y <= townCy + kWindow && seenN < 12; ++y)
                            for (int x = townCx - kWindow; x <= townCx + kWindow && seenN < 12; ++x) {
                                if (x < 0 || y < 0 || x >= width || y >= width) continue;
                                uintptr_t re = *(uintptr_t*)(grid + (size_t)(y * width + x) * 0x30 + 8);
                                if (!re) continue;
                                ++sampled;
                                const uintptr_t rd = *(uintptr_t*)(re + 0x28);
                                bool dup = false;
                                for (int k = 0; k < seenN; ++k) if (seen[k] == rd) { dup = true; break; }
                                if (!dup) seen[seenN++] = rd;
                            }
                        fprintf(stderr, "[startTown::gathererhut]   %d resource cell(s) sampled, "
                                "%d distinct key(s) at entity+0x28:\n", sampled, seenN);
                        for (int k = 0; k < seenN; ++k)
                            fprintf(stderr, "[startTown::gathererhut]     key[%d] = %p\n",
                                    k, (void*)seen[k]);
                        // Fall back to a plain ring placement so the hut still EXISTS for
                        // panel/dialog and state-machine testing — but say plainly that it
                        // can never produce, so a later reader does not read "hut present,
                        // zero output" as a port defect.
                        for (int r = 1; r <= kExtraHouseSearchRadius && !found; ++r)
                            for (int dy = -r; dy <= r && !found; ++dy)
                                for (int dx = -r; dx <= r && !found; ++dx) {
                                    if (dx != -r && dx != r && dy != -r && dy != r) continue;
                                    const int tx = townCx + dx, ty = townCy + dy;
                                    if (tx < 1 || ty < 1 ||
                                        tx + gw_ >= width - 1 || ty + gh_ >= width - 1) continue;
                                    bool clash = false;
                                    for (int o = 0; o < occN; ++o)
                                        if (tx - 1 < occ[o].x + occ[o].w && occ[o].x < tx + gw_ + 1 &&
                                            ty - 1 < occ[o].y + occ[o].h && occ[o].y < ty + gh_ + 1) {
                                            clash = true; break;
                                        }
                                    if (!clash) { bestX = tx; bestY = ty; bestScore = 0; found = true; }
                                }
                        if (found)
                            fprintf(stderr, "[startTown::gathererhut] FELL BACK to a "
                                    "cluster-centre placement. ⚠ A zero score HERE means "
                                    "nothing: this scan runs inside the structures pass, "
                                    "BEFORE StartTown_SpawnResources has scattered the trees "
                                    "whose SpawnComponent produces the wild food this hut "
                                    "accepts — so it is measuring a map with no wild food on "
                                    "it by construction. Read the POST-SCATTER recount line "
                                    "below instead; that is the count that decides whether "
                                    "the hut can produce.\n");
                    }
                    if (found) {
                        void* gent = placeStructure(gtmpl, bestX, bestY, 0, gw_, gh_,
                                                    /*isDemoFoundation=*/false, -1);
                        Occupy(bestX, bestY, gw_, gh_);
                        if (gent) ++spawned;
                        fprintf(stderr, "[startTown::gathererhut] demo gatherer hut tile=(%d,%d) "
                                "fp=%dx%d radius=%d acceptTypes=%d GATHERABLE=%d ent=%p\n",
                                bestX, bestY, gw_, gh_, gRadius, acceptN, bestScore, gent);
                        // ⚠ ORDERING: structures are placed BEFORE the resource scatter, and
                        // the wild food the hut accepts is spawned BY that scatter (the trees'
                        // SpawnComponent seed). So every score above — including a zero — was
                        // measured against a map that has no wild food on it yet, which is
                        // exactly how the old "worldgen spawns only 3 templates, none accepted"
                        // reading arose. Stash the siting inputs so the score can be RETAKEN
                        // after the scatter; that retake is the number that means anything.
                        g_demoGatherer.valid = true;
                        g_demoGatherer.cx = bestX + gw_ / 2;
                        g_demoGatherer.cy = bestY + gh_ / 2;
                        g_demoGatherer.radius = gRadius;
                        g_demoGatherer.acceptList = acceptList;
                        g_demoGatherer.acceptN = acceptN;
                        g_demoGatherer.scoreAtPlacement = bestScore;
                    }
                }
                fflush(stderr);
            }
        }
    }

    // ── BANISHED_BUILD_DEMO_FISHINGDOCK — one COMPLETED dock ON WATER ─────────────
    // The dock is the one food building whose output depends on WHERE it sits:
    // FishingComponent slot 21 (fishingcomponent_produce.cpp) rates the dock as
    // matching/(total*0.5) over the tiles in its radius, where "matching" means the
    // tile's terrain byte (cell+0x29) is set in the FishingDescription's accepted
    // terrain mask (desc+0x2c). Drop it inland and the rate is 0 — a false negative
    // that says nothing about whether the port is right.
    //
    // The trading-post block above seeds from the river POLYLINES, which is no use
    // here: standalone worldgen never populates worldGeom+0x1e0 (measured — all
    // entries {pts=NULL,n=0}), which is why that block has a cluster-centre fallback.
    // So this searches the TERRAIN directly, scoring candidate tiles by exactly the
    // predicate slot 21 will later evaluate. That makes the harness agree with the
    // thing under test by construction instead of by luck.
    {
        const char* fe = std::getenv("BANISHED_BUILD_DEMO_FISHINGDOCK");
        if (fe && fe[0] && fe[0] != '0') {
            void* ftmpl = ResolveTemplateByPath("Template\\FishermansDock.rsc");
            int fw = 0, fh = 0;
            if (const char* xpos = std::strchr(fe, 'x')) {
                fw = std::atoi(fe); fh = std::atoi(xpos + 1);
            }
            uint32_t mask = 0;
            int radius = 0;
            // FishermansDock.rsc declares `int _footprintRotation = 180`, and that value
            // is what MapComponent+0x30 feeds to dc20 as the mask rotation. Placing the
            // dock at rotation 0 therefore samples its _pathBitmap in the wrong
            // orientation, so the wrong cells get claimed. Scan the description's int
            // block for the 0/90/180/270 rotation rather than hardcoding it — the field
            // offset is not otherwise pinned in this codebase.
            int fdockRot = 0;
            if (ftmpl) {
                void* cpd = FindDescriptionOnTemplate(ftmpl, TypeId("CreatePlacedDescription"));
                if (cpd) {
                    if (fw <= 0 || fh <= 0) {
                        fw = *(int32_t*)((uint8_t*)cpd + 0x44);
                        fh = *(int32_t*)((uint8_t*)cpd + 0x48);
                    }
                    // `_footprintRotation` sits at +0x40, immediately before _width
                    // (+0x44) and _height (+0x48) — measured from the live description
                    // (the retail exe carries no resource field names to look it up by).
                    // FishermansDock.rsc sets it to 180.
                    const int rv = *(int32_t*)((uint8_t*)cpd + 0x40);
                    if (rv == 90 || rv == 180 || rv == 270) fdockRot = rv;
                }
                if (fw <= 0 || fh <= 0) { fw = 8; fh = 8; }
                if (void* fd = FindDescriptionOnTemplate(ftmpl, TypeId("FishingDescription")))
                    mask = *(uint32_t*)((uint8_t*)fd + 0x2c);
                if (void* rd = FindDescriptionOnTemplate(ftmpl, TypeId("RadiusDescription")))
                    radius = *(int32_t*)((uint8_t*)rd + 0x28);
            }

            void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                     ? *g_pGameWorldSingleton : nullptr;
            uint8_t* mapObj = gw ? *(uint8_t**)((uint8_t*)gw + 0xf8) : nullptr;
            const int width = mapObj ? *(int32_t*)mapObj : 0;
            uint8_t* grid = mapObj ? *(uint8_t**)(mapObj + 0x80) : nullptr;

            if (!ftmpl) {
                fprintf(stderr, "[startTown::fishingdock] BANISHED_BUILD_DEMO_FISHINGDOCK "
                        "set but Template\\FishermansDock.rsc did not resolve; nothing "
                        "spawned\n");
            } else if (!grid || width <= 0) {
                fprintf(stderr, "[startTown::fishingdock] no terrain grid (map=%p grid=%p "
                        "width=%d); nothing spawned\n", (void*)mapObj, (void*)grid, width);
            } else if (mask == 0 || radius <= 0) {
                fprintf(stderr, "[startTown::fishingdock] template has no usable "
                        "FishingDescription mask (0x%08x) / RadiusDescription radius (%d); "
                        "nothing spawned — placing blind would only produce a rate-0 dock\n",
                        mask, radius);
            } else {
                // Score every candidate by the slot-21 predicate and keep the best that
                // both fits and is nearest to town on ties. Bounded window + stride so
                // this stays cheap on a large map.
                const int townCx = (bbX0 + bbX1) / 2, townCy = (bbY0 + bbY1) / 2;
                const int kWindow = 80, kStride = 2;
                int bestX = 0, bestY = 0, bestScore = 0;
                long long bestDist = -1;
                bool found = false;

                // ── The dock must be in the TOWN'S CONNECTIVITY REGION. ──
                // Same predicate, same reason, as the hunter-lodge siting below:
                // `FishingWork::Slot4` gates on `Map_CheckPassable` (0x1400a00f0) from
                // the worker's cell to the dock's work spot, and that call is a
                // REGION-EQUIVALENCE test (cell+0x26 non-zero and equal), not a path
                // search. Scoring a dock purely by fishable water actively steers it to
                // the far bank of a river — the best water is on the other side — and a
                // dock there is unreachable without a bridge, so slot 4 returns at the
                // passability check on every tick and not one fishing action is ever
                // created. Measured 2026-08-15: a dock at (158,102) with 327 fishable
                // tiles produced ZERO accepted spots for a whole run.
                typedef char (*CanPathTo_t)(int*, uint32_t, uint32_t, int);
                static CanPathTo_t s_canPath = nullptr;
                static bool s_canPathResolved = false;
                if (!s_canPathResolved) {
                    s_canPath = reinterpret_cast<CanPathTo_t>(ResolveAddr(Module::Exe, 0x1400a00f0));
                    s_canPathResolved = true;
                }
                const int townCell = townCy * width + townCx;
                int candidates = 0, unreachable = 0;

                for (int ty = townCy - kWindow; ty <= townCy + kWindow; ty += kStride) {
                    for (int tx = townCx - kWindow; tx <= townCx + kWindow; tx += kStride) {
                        if (tx < 1 || ty < 1 ||
                            tx + fw >= width - 1 || ty + fh >= width - 1) continue;
                        bool clash = false;
                        for (int o = 0; o < occN; ++o) {
                            if (tx - 1 < occ[o].x + occ[o].w && occ[o].x < tx + fw + 1 &&
                                ty - 1 < occ[o].y + occ[o].h && occ[o].y < ty + fh + 1) {
                                clash = true; break;
                            }
                        }
                        if (clash) continue;

                        // The dock's own footprint must be on land, or placeStructure
                        // drops a building into the water.
                        bool footprintDry = true;
                        for (int yy = ty; yy < ty + fh && footprintDry; ++yy)
                            for (int xx = tx; xx < tx + fw; ++xx) {
                                const uint8_t t =
                                    *(grid + (size_t)(yy * width + xx) * 0x30 + 0x29);
                                if ((mask >> (t & 0x1f)) & 1) { footprintDry = false; break; }
                            }
                        if (!footprintDry) continue;
                        ++candidates;

                        // Reachable from town, or the worker can never stand on it.
                        if (s_canPath &&
                            !s_canPath(reinterpret_cast<int*>(mapObj), 0xe1f,
                                       (uint32_t)((ty + fh / 2) * width + (tx + fw / 2)),
                                       townCell)) {
                            ++unreachable;
                            continue;
                        }

                        // Score exactly as slot 21 will: matching tiles in the disk.
                        const int cx = tx + fw / 2, cy = ty + fh / 2;
                        int score = 0;
                        for (int dy = -radius; dy < radius; ++dy)
                            for (int dx = -radius; dx < radius; ++dx) {
                                if (dx * dx + dy * dy > radius * radius) continue;
                                const int x = cx + dx, y = cy + dy;
                                if (x < 0 || y < 0 || x >= width || y >= width) continue;
                                const uint8_t t =
                                    *(grid + (size_t)(y * width + x) * 0x30 + 0x29);
                                if ((mask >> (t & 0x1f)) & 1) ++score;
                            }
                        if (score == 0) continue;

                        const long long ddx = (long long)cx - townCx;
                        const long long ddy = (long long)cy - townCy;
                        const long long dist = ddx * ddx + ddy * ddy;
                        if (score < bestScore) continue;
                        if (score == bestScore && bestDist >= 0 && dist >= bestDist) continue;
                        bestScore = score; bestDist = dist;
                        bestX = tx; bestY = ty; found = true;
                    }
                }

                if (!found) {
                    fprintf(stderr, "[startTown::fishingdock] no %dx%d dry footprint with any "
                            "fishable tile in r=%d within %d tiles of the town centre "
                            "(mask=0x%08x); nothing spawned — dry candidates=%d, of which "
                            "%d were in a DIFFERENT connectivity region than town\n",
                            fw, fh, radius, kWindow, mask, candidates, unreachable);
                } else {
                    void* fent = placeStructure(ftmpl, bestX, bestY, fdockRot, fw, fh,
                                                /*isDemoFoundation=*/false, -1);
                    Occupy(bestX, bestY, fw, fh);
                    if (fent) ++spawned;
                    fprintf(stderr, "[startTown::fishingdock] demo fishing dock tile=(%d,%d) "
                            "fp=%dx%d radius=%d mask=0x%08x fishableTiles=%d "
                            "(expected rate ~%.3f) ent=%p rot=%d  [reachable siting: %d dry "
                            "candidates, %d rejected as off-region; town=(%d,%d)]\n",
                            bestX, bestY, fw, fh, radius, mask, bestScore,
                            /* the disk area slot 21 will divide by */
                            bestScore / (3.14159f * radius * radius * 0.5f), fent, fdockRot,
                            candidates, unreachable, townCx, townCy);
                }
            }
            fflush(stderr);
        }
    }

    return spawned;
}

// ============================================================================
// DEFERRED SPAWN — demo hunter lodge, sited by live ANIMAL DENSITY.
//
// Driven from GameUpdateLoop::Update once per frame; everything below is a no-op
// until the request recorded by the food-demo block above comes due. See the
// DEFERRED SPAWN note above StartTown_SpawnStructures for why this cannot run at
// world-build time.
//
// The scorer is the [HuntScan] slot walk from huntanimal.cpp, reused verbatim: it
// reads the SAME character spatial-hash manager the hunt's proximity query walks
// (gw+0x88; +0x10 slot array, stride 0x18 of {IBP, next+prev, cellIdx}, +0x18 count),
// resolves each slot's CharacterComponent, and applies the SAME component-type test
// (owner at +0x10, component array +0x30 / count +0x38, identity via vtable slot
// +0x10 GetTypeId). That is deliberate: it means a site that scores N here is a site
// where the query the hunter actually runs can see N animals.
//
// Scored in WORLD units, not tiles. The animals' positions come out of their
// ModelComponent (+0x158..0x160) in world units, the work radius comes out of
// RadiusDescription in tiles, and the two are only interchangeable if the tile->world
// map is 1:1 — which is an assumption, not a fact. So measure the scale once from the
// map itself (the world distance between two tiles ten apart) and convert. A wrong
// scale here would silently shrink or inflate every radius test and look exactly like
// "there are no deer near town".
// ============================================================================
namespace {

struct DL_AnimalPos { float x, y, z; };

// Walk the char manager slot array and collect the world position of every live
// character whose owner entity carries `wantType`.
int DL_CollectAnimals(void* gridPtr, int wantType, std::vector<DL_AnimalPos>& out,
                      int* outLive) {
    *outLive = 0;
    if (!gridPtr) return 0;
    const uint8_t* m = (const uint8_t*)gridPtr;
    const uint8_t* slots = *(const uint8_t* const*)(m + 0x10);
    const int slotCount = *(const int*)(m + 0x18);
    if (!slots || slotCount <= 0) return 0;

    for (int i = 0; i < slotCount && i < 4096; ++i) {
        const uint8_t* slot = slots + (int64_t)i * 0x18;
        if (*(const int*)(slot + 0x10) < 0) continue;      // not placed in a cell
        void* charComp = *(void* const*)(slot + 0x00);
        if (!charComp) continue;
        ++(*outLive);
        void* model = *(void* const*)((const uint8_t*)charComp + 0x68);
        if (!model) continue;
        void* owner = *(void* const*)((const uint8_t*)charComp + 0x10);
        if (!owner || wantType == 0) continue;
        const int cnt = *(const int*)((const uint8_t*)owner + 0x38);
        void** comps = *(void** const*)((const uint8_t*)owner + 0x30);
        bool match = false;
        for (int c = 0; comps && c < cnt && c < 64; ++c) {
            void* comp = comps[c];
            if (!comp) continue;
            typedef int (*getId_t)(void*);
            getId_t g = *reinterpret_cast<getId_t*>(*(uintptr_t*)comp + 0x10);
            if (g && g(comp) == wantType) { match = true; break; }
        }
        if (!match) continue;
        out.push_back({ *(const float*)((const uint8_t*)model + 0x158),
                        *(const float*)((const uint8_t*)model + 0x15c),
                        *(const float*)((const uint8_t*)model + 0x160) });
    }
    return (int)out.size();
}

}  // namespace

extern "C" void StartTown_DeferredSpawn_Tick(int frameNum) {
    if (!g_deferredLodge.wanted || g_deferredLodge.done) return;
    if (frameNum < g_deferredLodge.fireFrame) return;
    g_deferredLodge.done = true;      // one shot either way — never retry into a half-state

    void* gws = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton
                                                                 : nullptr;
    void* game = gws ? *(void**)((uint8_t*)gws + 0xf8) : nullptr;
    void* gridPtr = gws ? *(void**)((uint8_t*)gws + 0x88) : nullptr;
    if (!gws || !game || !g_stPlaceStructure || !fn_MapGetTerrainPos) {
        fprintf(stderr, "[startTown::hunterlodge] deferred placement ABORTED at frame %d "
                "(world=%p map=%p placeStructure=%s terrainPos=%s); nothing spawned\n",
                frameNum, gws, game, g_stPlaceStructure ? "yes" : "NO",
                fn_MapGetTerrainPos ? "yes" : "NO");
        fflush(stderr);
        return;
    }

    const int mapW = *(int32_t*)game;
    const int w = g_deferredLodge.w, h = g_deferredLodge.h;

    // The search type the hunt itself uses: *DAT_1401c8000, read exactly as
    // huntanimal.cpp reads it, so the scorer cannot disagree with the query about what
    // it is looking for. Cross-checked against the by-name id, because a by-name resolve
    // of a misnamed global returns a real id of the WRONG class and every null test
    // still passes.
    int wantType = 0;
    if (int* p = reinterpret_cast<int*>(ResolveAddr(Module::Exe, 0x1401c8000))) wantType = *p;
    const uint32_t byName = ::Object::GetTypeIdByName("WildAnimalComponent");

    std::vector<DL_AnimalPos> animals;
    int live = 0;
    DL_CollectAnimals(gridPtr, wantType, animals, &live);

    // World units per tile, measured off this map rather than assumed. Ten tiles apart
    // so terrain height noise on either sample cannot dominate the ratio.
    float wpt = 1.0f;
    {
        const int ax = g_deferredLodge.townCx, ay = g_deferredLodge.townCy;
        const int bx = (ax + 10 < mapW - 1) ? ax + 10 : ax - 10;
        float pa[3] = {0,0,0}, pb[3] = {0,0,0};
        StartTown_TileToWorld(game, ax, ay, pa);
        StartTown_TileToWorld(game, bx, ay, pb);
        const float d = std::sqrt((pb[0]-pa[0])*(pb[0]-pa[0]) + (pb[1]-pa[1])*(pb[1]-pa[1]));
        if (d > 0.01f) wpt = d / 10.0f;
    }
    const float radiusWorld = (float)g_deferredLodge.workRadius * wpt;

    fprintf(stderr, "[startTown::hunterlodge] deferred scan at frame %d: charSlots live=%d "
            "wantType=%d (byName WildAnimalComponent=%u) animals=%d worldPerTile=%.3f "
            "workRadius=%d tiles = %.1f world\n",
            frameNum, live, wantType, byName, (int)animals.size(), wpt,
            g_deferredLodge.workRadius, radiusWorld);

    // ── Score every candidate footprint by how many animals sit inside the work
    // radius of its centre. Same window/stride as the gatherer hut's resource scan;
    // BANISHED_HUNTERLODGE_WINDOW widens it, because how far out the deer live is a
    // property of the map, not a constant (first measured run: the densest site within
    // 90 tiles held only 17 of 113 animals).
    int kWindow = 90;
    const int kStride = 2;
    if (const char* wv = std::getenv("BANISHED_HUNTERLODGE_WINDOW")) {
        int v = std::atoi(wv);
        if (v > 0) kWindow = v;
    }
    int bestX = 0, bestY = 0, bestScore = 0;
    long long bestDist = -1;
    bool found = false;
    const float r2 = radiusWorld * radiusWorld;
    // Density landscape, so "17" can be read as "the best on offer" rather than "the
    // scorer picked badly". A strictly-better score always wins below, so bestScore IS
    // the window maximum — these two say how much of the window was even a candidate.
    int candidates = 0, scoring = 0, unreachable = 0;

    // ── The site must be in the TOWN'S CONNECTIVITY REGION. ──
    // MEASURED 2026-08-14, and it is the whole reason a well-scored lodge still never
    // hunted. `HuntWork::Slot4`'s fourth cell check is `Map_CanPathTo` (0x1400a00f0),
    // which is NOT an A* — it is a region-equivalence test: both cells' region word at
    // `cell+0x26` must be non-zero and EQUAL (the ids are flood-filled by ZoneRefresh
    // 0x14009c480, gated on the dirty byte at map+0xa2 that Load sets). Scoring purely
    // by animal density with a 200-tile window put the lodge at (184,49) across a region
    // boundary from the town at (147,108): every one of the 35 candidate cells that
    // reached that check failed it (22 terrain + 17 distance + 6 occupied + 35 noPath =
    // 80 runs, MOVETO=0), so Execute took the in-place fallback every time and the
    // hunter stood in town forever. The same build with the lodge inside the town's
    // region scored noPath=0 and MOVETO=11/30.
    //
    // So apply the identical predicate at siting time. Without it, density scoring is
    // actively harmful: the denser ground is often the far side of a river.
    typedef char (*CanPathTo_t)(int*, uint32_t, uint32_t, int);
    static CanPathTo_t s_canPath = nullptr;
    static bool s_canPathResolved = false;
    if (!s_canPathResolved) {
        s_canPath = reinterpret_cast<CanPathTo_t>(ResolveAddr(Module::Exe, 0x1400a00f0));
        s_canPathResolved = true;
    }
    const int townCell = g_deferredLodge.townCy * mapW + g_deferredLodge.townCx;

    if (!animals.empty()) {
        for (int ty = g_deferredLodge.townCy - kWindow;
             ty <= g_deferredLodge.townCy + kWindow; ty += kStride) {
            for (int tx = g_deferredLodge.townCx - kWindow;
                 tx <= g_deferredLodge.townCx + kWindow; tx += kStride) {
                if (tx < 1 || ty < 1 || tx + w >= mapW - 1 || ty + h >= mapW - 1) continue;
                if (ST_ClashesWithStartTown(tx, ty, w, h)) continue;
                if (!ST_SiteIsWorkable(tx, ty, w, h)) continue;
                ++candidates;

                const int cell = (ty + h / 2) * mapW + (tx + w / 2);
                if (s_canPath && !s_canPath(reinterpret_cast<int*>(game), 0xe1f,
                                            (uint32_t)cell, townCell)) {
                    ++unreachable;
                    continue;
                }

                float c[3] = {0,0,0};
                StartTown_TileToWorld(game, tx + w / 2, ty + h / 2, c);
                int score = 0;
                for (const DL_AnimalPos& a : animals) {
                    const float dx = a.x - c[0], dy = a.y - c[1];
                    if (dx*dx + dy*dy <= r2) ++score;
                }
                if (score == 0) continue;
                ++scoring;

                // Best density wins; ties go to the site closest to town, so the lodge
                // is no further out than the deer actually require.
                const long long ddx = (long long)(tx + w / 2) - g_deferredLodge.townCx;
                const long long ddy = (long long)(ty + h / 2) - g_deferredLodge.townCy;
                const long long dist = ddx * ddx + ddy * ddy;
                if (score < bestScore) continue;
                if (score == bestScore && bestDist >= 0 && dist >= bestDist) continue;
                bestScore = score; bestDist = dist;
                bestX = tx; bestY = ty; found = true;
            }
        }
    }

    if (!found) {
        // Say which of the two very different causes this is. "No animal is within
        // radius of any candidate" and "the index/type test found nothing at all" both
        // land here and need opposite fixes.
        if (animals.empty())
            fprintf(stderr, "[startTown::hunterlodge] NO ANIMALS VISIBLE at frame %d "
                    "(live char slots=%d, wantType=%d). Either the deferral is still too "
                    "early for the herds to have started up, or the search type id is "
                    "wrong — a lodge placed now could not hunt whatever it is sited on.\n",
                    frameNum, live, wantType);
        else
            fprintf(stderr, "[startTown::hunterlodge] %d animal(s) visible but NO free, "
                    "workable %dx%d footprint within %d tiles of the town centre has any of "
                    "them inside %.1f world units; falling back to a plain ring placement\n",
                    (int)animals.size(), w, h, kWindow, radiusWorld);

        // Place it anyway so the lodge EXISTS for panel / state-machine / slot-coverage
        // testing — but the log above already says it cannot produce, so a later reader
        // does not read zero kills as a port defect.
        for (int r = 1; r <= kExtraHouseSearchRadius && !found; ++r)
            for (int dy = -r; dy <= r && !found; ++dy)
                for (int dx = -r; dx <= r && !found; ++dx) {
                    if (dx != -r && dx != r && dy != -r && dy != r) continue;
                    const int tx = g_deferredLodge.townCx + dx;
                    const int ty = g_deferredLodge.townCy + dy;
                    if (tx < 1 || ty < 1 || tx + w >= mapW - 1 || ty + h >= mapW - 1) continue;
                    if (ST_ClashesWithStartTown(tx, ty, w, h)) continue;
                    if (!ST_SiteIsWorkable(tx, ty, w, h)) continue;
                    const int fcell = (ty + h / 2) * mapW + (tx + w / 2);
                    if (s_canPath && !s_canPath(reinterpret_cast<int*>(game), 0xe1f,
                                                (uint32_t)fcell, townCell)) continue;
                    bestX = tx; bestY = ty; bestScore = 0; found = true;
                }
    }

    if (!found) {
        fprintf(stderr, "[startTown::hunterlodge] no free workable %dx%d footprint at all; "
                "nothing spawned\n", w, h);
        fflush(stderr);
        return;
    }

    void* ent = g_stPlaceStructure(g_deferredLodge.tmpl, bestX, bestY, 0, w, h,
                                   /*isDemoFoundation=*/false, -1);
    g_stOccupied.push_back({ bestX, bestY, w, h });

    // Distance to the nearest animal is the number that predicts whether this lodge can
    // ever kill: HuntAnimal's kill radius is 8.0 world units and the hunter only walks
    // to a random cell inside the work radius, so a nearest-animal distance far outside
    // the work radius means "sited wrong", not "hunt broken".
    float nearest = -1.0f;
    if (!animals.empty()) {
        float c[3] = {0,0,0};
        StartTown_TileToWorld(game, bestX + w / 2, bestY + h / 2, c);
        float best = 1e30f;
        for (const DL_AnimalPos& a : animals) {
            const float dx = a.x - c[0], dy = a.y - c[1];
            const float d = std::sqrt(dx*dx + dy*dy);
            if (d < best) best = d;
        }
        nearest = best;
    }
    fprintf(stderr, "[startTown::hunterlodge] DEFERRED demo hunter lodge tile=(%d,%d) "
            "fp=%dx%d frame=%d ANIMALS_IN_RADIUS=%d of %d total nearestAnimal=%.2f "
            "(work radius %.1f world; %d candidate site(s) in a %d-tile window, %d "
            "outside the town's connectivity region, %d with any animal at all) "
            "ent=%p%s\n",
            bestX, bestY, w, h, frameNum, bestScore, (int)animals.size(), nearest,
            radiusWorld, candidates, kWindow, unreachable, scoring, ent,
            bestScore == 0 ? "  <-- ZERO animals in radius: this lodge cannot produce"
                           : "");
    fflush(stderr);
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

// Exposed for the standalone tool-injection harness (standalone/tool_harness.cpp):
// resolve a building DESCRIPTION template ("Template\\<Name>.rsc") to point a
// PlacementTool's descriptionPtr (+0x20) at. Thin extern "C" wrapper over the
// static resolver so the harness need not re-plumb the ExternalFactory singleton.
extern "C" void* StartTown_ResolveTemplateByPath(const char* path) {
    return ResolveTemplateByPath(path);
}

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

    fn_InternalBasePtr_CheckPtr(&entIBP);
    StartTown_RunOnCreatePass((void*)entIBP);   // fires ResidenceComponent::OnCreate (LIVE_RESIDENCE)
    // Click-to-select: mark the workplace footprint in the region-map cell array (see
    // StartTown_MarkBuildingCells) so EditTool's click-pick resolves it under the cursor.
    fn_InternalBasePtr_CheckPtr(&entIBP);
    StartTown_MarkBuildingCells(game, tileX, tileY, fpW, fpH, entIBP, rot);

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

    // ---- Phase 1 infra: activate the lodge as an employer (seat WPC+0x48 from the
    // desc, SetAccepting) via the shared StartTown_ActivateWorkplace helper. The lodge
    // passes registerNow=true even when the WPC live controller would register it on
    // the first SM tick, because StartTown_EmployForesters queries the townMgr+0x100
    // BST (FindAvailableBuilding) BEFORE any tick runs; the insert dedups, so the
    // state-2 enter re-registering later is harmless. Worker ASSIGNMENT itself is the
    // Phase-5 employ cascade — this only makes the lodge FINDABLE.
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        int64_t townMgr = *(int64_t*)((uint8_t*)*g_pGameWorldSingleton + 0xf0);
        uint64_t profKey = StartTown_ActivateWorkplace(
            entity, /*registerNow=*/true, "Phase4Employ");
        g_foresterProfKey = profKey;   // for the deferred employ pass

        // Verify the reader finds it: FindAvailableBuilding(townMgr, &out, profKey)
        // returns this lodge entity (worker count < capacity + the WPC+0x50 enable
        // flag). Proves the townMgr+0x100 registry + reader end-to-end (the FIND half
        // of producer employment); the ASSIGN half is the ProfessionUI seat cascade.
        if (townMgr && profKey != 0) {
            int64_t foundIBP = 0;
            FindAvailableBuilding(townMgr, &foundIBP, profKey);
            fprintf(stderr, "[Phase4Employ] FindAvailableBuilding(profKey=0x%llx) -> %p "
                    "(lodge entity=%p) %s\n",
                    (unsigned long long)profKey, (void*)foundIBP, entity,
                    (foundIBP == reinterpret_cast<int64_t>(entity)) ? "MATCH" :
                    (foundIBP != 0 ? "other-building" : "NOT-FOUND"));
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
//
// [2026-07-26] OnAdd is now ALSO reachable the retail way: CitizenComponent's vtable
// slot 6 (+0x30) is wired under BANISHED_LIVE_CITIZEN_ONADD, so any citizen that goes
// through EntityList::RegisterEntity registers itself. Since OnAdd is a plain append
// with no dedup, this pass would double-count anyone the broadcast already added — so
// it now checks the list first. 24 entries, once per run; the scan is free.
static bool s_townCitizensRegistered = false;
static void StartTown_RegisterTownCitizens() {
    if (s_townCitizensRegistered) return;
    s_townCitizensRegistered = true;
    int64_t townMgr = 0;
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton)
        townMgr = *reinterpret_cast<int64_t*>(
            reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xf0);
    auto alreadyListed = [&](void* c) -> bool {
        if (!townMgr) return false;
        int64_t* arr = *reinterpret_cast<int64_t**>(townMgr + 0xa0);
        int n = *reinterpret_cast<int32_t*>(townMgr + 0xa8);
        for (int i = 0; i < n && arr; i++)
            if (reinterpret_cast<void*>(arr[i]) == c) return true;
        return false;
    };
    int n = 0, skipped = 0;
    for (void* c : g_foundingCitizens) {
        if (!c) continue;
        if (alreadyListed(c)) { ++skipped; continue; }
        CitizenComponent_OnAdd(c);
        ++n;
    }
    fprintf(stderr, "[startTown::registry] registered %d founding citizens into townMgr+0xa0"
            " (%d already registered by the OnAdd broadcast)\n", n, skipped);
    fflush(stderr);
}

// Houses: registration into townMgr+0x20 is now RETAIL-DRIVEN. The stamped SM
// pendingState=2 (see the +0x52 stamp in StartTown_SpawnStructures) makes the
// first StateMachine::Tick run the ResidenceComponent state-2 Enter — the
// helper_9de0 append (ResidenceComponent_EnterBuilt in residencecomponent.cpp).
// The former StartTown_RegisterTownResidences manual append was removed: with
// the real state table wired it would double-register every founding house.

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
// ProfessionList (gw+0xb0) being live (LoadProfessionList_Step2, default-on). Build-time
// pass that reproduces what retail runs at runtime in FUN_140117080.
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
#if defined(_DEBUG)
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
#if defined(_DEBUG)
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

#if defined(_DEBUG)
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

#if defined(_DEBUG)
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
// ---- Citizen carry-storage capacity init (BANISHED_LIVE_CITIZEN_CARRY) ----------
// A citizen's carry StorageComponent has a valid StorageDescription (maxPerSlot +0x24
// = 100, not area-based) but its capacity (+0x38) stays 0 in standalone: the component's
// OnAdded/Draw slot (StorageComponent_Draw 0x14012c640, which sets +0x38 = maxPerSlot *
// tileArea) never runs for citizens (inert in SA). With capacity 0 a citizen can carry
// nothing, so every haul aborts — GatherRawMaterials::Execute recomputes remaining=0 and
// replans, and no construction (or general economy) hauling ever completes. Run the
// faithful Draw setup once at spawn. The carry storage is located exactly as
// GatherRawMaterials does (Entity_FindComponent_Storage 0x140006270 on entity+0x10).
// Gated default-OFF (A/B); fold into SA_LIVE_ENV once verified.
//
// STANDS DOWN when BANISHED_LIVE_STORAGE_ONADDED wires StorageComponent's real slot 6
// (storagecomponent.cpp): that runs for EVERY entity at RegisterEntity time, including
// citizens born after world start, which these four founding-colonist call sites never
// covered. It still runs as a VERIFIER in that mode — it reads the capacity the slot
// left behind and logs it instead of calling Draw again, so "the slot ran" is an
// observed outcome rather than an assumption.
static void CitizenCarryStorageInit(void* ent) {
    static int s_on = -1;
    static int s_slotLive = -1;
    if (s_on < 0) {
        s_on = ::ComponentFactory::LiveEnabled("BANISHED_LIVE_CITIZEN_CARRY") ? 1 : 0;
        s_slotLive = ::ComponentFactory::LiveEnabled("BANISHED_LIVE_STORAGE_ONADDED") ? 1 : 0;
    }
    if ((s_on != 1 && s_slotLive != 1) || !ent || !fn_InternalBasePtr_CheckPtr) return;

    static bool s_res = false;
    static void* (*s_find6270)(uintptr_t, void*) = nullptr;
    if (!s_res) {
        s_find6270 = reinterpret_cast<void* (*)(uintptr_t, void*)>(ResolveAddr(Module::Exe, 0x140006270));
        s_res = true;
    }
    static int s_log = 0;
    bool logThis = (s_log < 6);
    if (!s_find6270) { if (logThis) { ++s_log; fprintf(stderr, "[CarryInit] BAIL find6270 null\n"); fflush(stderr); } return; }

    // `ent` IS the citizen entity; find its StorageComponent directly. (GRM Execute's
    // `citizenEnt` is instead a component whose +0x10 dereferences to this same entity,
    // hence GRM's extra +0x10 hop — which does NOT apply here.)
    uintptr_t stIbp = 0;
    s_find6270(reinterpret_cast<uintptr_t>(ent), &stIbp);
    if (!stIbp) { if (logThis) { ++s_log; fprintf(stderr, "[CarryInit] BAIL no storage (ent=%p)\n", ent); fflush(stderr); } return; }
    fn_InternalBasePtr_CheckPtr(&stIbp);
    void* stComp = reinterpret_cast<void*>(stIbp);   // FindComponent writes the component ptr into the IBP

    int before = *reinterpret_cast<int*>((uintptr_t)stComp + 0x38);
    if (s_slotLive == 1) {
        // Verify-only: slot 6 already ran inside RegisterEntity. Report what it left.
        if (logThis) { ++s_log;
            fprintf(stderr, "[CarryInit] VERIFY (slot6 live) citizen storage=%p capacity(+0x38)=%d%s\n",
                    stComp, before, (before == 0) ? "  <-- STILL ZERO, slot did not run" : "");
            fflush(stderr);
        }
        return;
    }
    StorageComponent_Draw(reinterpret_cast<StorageComponent*>(stComp));
    int after = *reinterpret_cast<int*>((uintptr_t)stComp + 0x38);
    if (logThis) { ++s_log;
        fprintf(stderr, "[CarryInit] citizen storage=%p capacity(+0x38) %d->%d\n", stComp, before, after);
        fflush(stderr);
    }
}

// ---- Founding-family links (BANISHED_LIVE_CITIZEN_FAMILY) --------------------
// The tail of CitizenComponent_impl_0dd0 (0x1400a0dd0) that this spawner replaces:
// after the two adults are created it cross-links them as spouses, then splices each
// child into the second adult's family ring:
//     res18 = sub_1040(..., param_5='\0',   educated);   // gender = (param_5=='\0') = 1
//     res8  = sub_1040(..., param_5='\x01', educated);   // gender = 0
//     res18->+0x250 = res8;  res8->+0x250 = res18;       // spouse cross-link
//     for each child: helper_68a0(res8, &child)          // ring join @0x1400a0fcd
// Our SpawnOneCitizen replaces sub_1040 (the retail deferred init FUN_140117080 is
// guarded off in standalone), so it also has to reproduce the gender field the ring
// and the marriage finder depend on:
//   [STANDALONE GUARD] CitizenComponent+0x38 (gender) is written by FUN_140117080 from
//   sub_1040's `(uint)(param_5 == '\0')`. With that init off, +0x38 stayed 0 for every
//   colonist, which means (a) helper_fe40's `cand->+0x38 != self->+0x38` opposite-gender
//   test can never pass, so nobody can ever marry, and (b) the Update reproduction gate
//   `+0x38 == 0` (female) is true for the entire town. Derive it from the mesh variant
//   the same way retail does in reverse: FUN_1401253d0 gets `(ushort)(gender == 0)`, so
//   variantIdx 0 <=> gender 1 and variantIdx 1 <=> gender 0.
// Opt-in: this changes population dynamics (marriages, gendered births), so it is NOT
// in kDefaultOnFlags.
static bool FamilyLinksOn() {
    static int en = -1;
    if (en < 0) en = ::ComponentFactory::LiveEnabled("BANISHED_LIVE_CITIZEN_FAMILY") ? 1 : 0;
    return en == 1;
}

// ---- Retail founding spawn (BANISHED_LIVE_FOUNDING_SPAWN) -------------------
// The faithful port of sub_1040 / impl_0dd0 lives in founding_spawn.cpp. When the flag
// is on it REPLACES SpawnOneCitizen for the founding cohort: FUN_140117080 reproduces
// gender / mesh variant / spawn transform / name / colour / age / lifespan / housing,
// all of which SpawnOneCitizen also does by hand, so running both double-applies them
// and burns two RNG streams. The family CROSS-LINKS below are part of impl_0dd0 (not of
// sub_1040) and are unconditional in retail, so the retail path performs them too — the
// FamilyLinksOn() opt-in only governs the hand-rolled path.
extern "C" bool  StartTown_FoundingSpawnEnabled();
extern "C" float StartTown_FoundingAdultAge(void* citizenTemplate);
extern "C" float StartTown_FoundingChildAge(void* citizenTemplate);
extern "C" void* StartTown_SpawnColonistRetail(uint8_t* startConfig, void* citizenTemplate,
                                               void* entityList, const float base[3],
                                               float age, char genderSel, char educated,
                                               uint8_t** outCitizenComp);

// CitizenComponent_helper_68a0 (0x1401168a0) — splice `member` into `host`'s family
// ring (host +0x260 head, member +0x268/+0x270 links, member +0x258 host back-ref).
// Ported as CitizenComp_JoinRoster_68a0 and registered by reg_citizen_25.cpp.
static void (*s_fn_JoinFamily_68a0)(void* host, void* memberIBP) = nullptr;
static bool s_familyFuncsResolved = false;
static void EnsureFamilyFuncs() {
    if (s_familyFuncsResolved) return;
    s_familyFuncsResolved = true;
    s_fn_JoinFamily_68a0 = reinterpret_cast<void (*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x1401168a0));
}

// Resolve the CitizenComponent behind a spawned colonist (entity already CheckPtr'd).
static uint8_t* StartTown_CitizenComp(void* entity) {
    if (!entity || !fn_InternalBasePtr_Ctor || !fn_InternalBasePtr_CheckPtr) return nullptr;
    void* cc = FindComponentOnEntity(entity, TypeId("CitizenComponent"));
    if (!cc) return nullptr;
    uintptr_t ibp = 0;
    fn_InternalBasePtr_Ctor(&ibp, cc);
    fn_InternalBasePtr_CheckPtr(&ibp);
    return reinterpret_cast<uint8_t*>(ibp);
}

static void* SpawnOneCitizen(void* citizenTemplate, void* entityList,
                             const float base[3], float jitter, void* rng,
                             bool isChild, int variantIdx,
                             uint8_t** outCitizen = nullptr) {
    if (outCitizen) *outCitizen = nullptr;
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

        // Gender (+0x38) + the CitizenComponent handle the caller needs to build the
        // family links — see the FamilyLinksOn() note above.
        //
        // The gender write is UNGATED. It used to sit inside FamilyLinksOn(), but gender
        // is not a family-links feature: retail's per-citizen init writes +0x38 for every
        // colonist, unconditionally, and that initializer is registered and live for every
        // citizen BORN in game (citizencomponent.cpp -> CitizenComp_DeferredInit_117080).
        // Only the founding colonists come through this hand-written spawner, so leaving
        // the write behind an opt-in made the founding town the one cohort with no gender
        // at all: +0x38 == 0 for all of them, which means (a) helper_fe40's opposite-gender
        // test `cand->+0x38 != self->+0x38` can never pass, so no founding colonist can ever
        // marry, and (b) the Update reproduction gate `+0x38 == 0` (female) reads true for
        // the entire founding generation. What stays gated is the family CROSS-LINKING
        // (spouse +0x250, child ring) below, which genuinely changes population dynamics.
        uint8_t* cc = StartTown_CitizenComp((void*)entIBP);
        if (cc) {
            *reinterpret_cast<int32_t*>(cc + 0x38) = (variantIdx == 0) ? 1 : 0;
            if (outCitizen) *outCitizen = cc;
        }
    }

    // Give the citizen its carry-storage capacity (StorageComponent OnAdded is inert in
    // SA) so the citizen can actually pick up/haul materials. Default-OFF (A/B).
    CitizenCarryStorageInit(ent);
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
        en = ::ComponentFactory::LiveEnabled("BANISHED_LIVE_HOUSING") ? 1 : 0;
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
        // ResidenceDescription carries TWO different numbers and they are easy to confuse:
        //   +0x20 = how many HOUSEHOLD SLOTS the house has (1 for a wood house; it is what
        //           Serialize resizes +0x50 to, and what +0x58 then holds)
        //   +0x24 = how many OCCUPANTS one slot may hold — the real capacity.
        // +0x24 is the cap the BIRTH gate reads: CitizenComponent::Update only starts a
        // couple's reproduction timer when helper_a4f0 (0x14012a4f0) reports
        // `desc+0x24 > slot[i].count`, i.e. the household is not yet full. So a founding
        // family rostered up to or past +0x24 can never have a child, and one rostered PAST
        // it also breaks the invariant ResidenceUI::Slot7's unbounded row walk relies on.
        // Log both next to the actual resident count so over-filling is visible.
        int occupantCap = -1;
        {
            void* rdesc = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(resComp) + 0x18);
            if (rdesc) occupantCap = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(rdesc) + 0x24);
        }
        fprintf(stderr, "[startTown::housing] family[%zu] -> house#%zu (IBP=%llx): housed %d/%zu "
                "members (citizen+0x238); ResidenceComponent(c7cf0) slot[0] residents=%d slots=%d "
                "occupantCap(desc+0x24)=%d%s; +0x248!=0 (should be 0) on %d members\n",
                f, f, (unsigned long long)houseIBP, housed, families[f].size(),
                residents, slotCnt, occupantCap,
                (occupantCap >= 0 && residents >= occupantCap)
                    ? "  <-- FULL: this household can never have a child" : "",
                misfiled);
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

// ---- Founding-house household food inventory (BANISHED_LIVE_HOUSING) -----------------------
// Seed edible food into each founding WoodHouse's plain StorageComponent (c7e78), using
// AddItem FUN_14012d000 (append/increment a storage slot at comp+0x28; no Scene op,
// build-time safe). MUST run AFTER StartTown_SeedStartingResources so the town raw-material
// TEMPLATE pool (townMgr+0x110) — the same edible templates the barn seeder uses — is
// populated. Gated on BANISHED_LIVE_HOUSING.
//
// The StorageComponent is the ONE household container (exe-faithful): the eat loop,
// Storage_GetResourceCount (0x14012a9d0 — walks owner→c7e78 via finder 0x140006270),
// the residence heat tick's firewood burn, and CitizenComp_HasSufficientResources
// (0x1401156e0, whose GetResourceCount sums the EDIBLE units here) all read it.
// HasSufficientResources compares `occupiedRows * residentsInSlot * (int)(CitizenDesc+0xc4)`
// against that sum, so we seed `residents * factor` + margin per house — otherwise it
// returns "insufficient" and spams GatherResources(id6).
//
// [MISPORT FIX 2026-07-19] This used to ALSO AddItem into the ResidenceComponent's +0x28
// ("home-food reserve") — a misreading: the exe residence has NO own storage; +0x28 is its
// controller STATE NODE. That write only "worked" while the node was absent (inert
// factory); with the real residence state table wired (heat-tick bring-up) AddItem would
// grow/free the node vtable as if it were a slot array → Runtime FreeLocked crash.
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
    // [STAND-IN, NOT THE FIX] Firewood. The residence heat tick (0x140129970) pins the
    // warmth need (+0x48) at 0 while the outdoor temperature is above desc+0xF4 and lets
    // it climb once winter arrives, killing at 20 — and it can only hold it down by
    // BURNING fuel out of the house's own StorageComponent. Standalone has no producer of
    // any kind, so every soak ends the same way: the whole population dies within ~2000
    // frames of the first winter (~frame 16000 at FORCE_SPEED=10, seed 12345), which caps
    // every measurable rate at one pre-winter season. Seeding fuel next to the food seed
    // buys the soak time to measure birth/marriage rates; it is NOT the winter fixed. The
    // real fix is a woodcutter producing fuel plus the haul into house storage — the same
    // "missing woodcutter economy" already noted against residence heating.
    int64_t fuelTmpl = 0;
    uint32_t fuelFlags = 0;
    // Per-unit VOLUME of each material (RawMaterialDescription+0x48). A house's
    // StorageComponent capacity (+0x38) is a single volume budget shared by every
    // material, so the seeds below have to be measured in volume, not units — see the
    // capacity clamp in the per-house loop.
    int foodVol = 1, fuelVol = 1;
    for (int t = 0; t < tmplCount && (foodTmpl == 0 || fuelTmpl == 0); ++t) {
        int64_t tmpl = tmplArr ? *(int64_t*)(tmplArr + (int64_t)t * 8) : 0;
        void* desc = tmpl ? FindDescriptionOnTemplate(reinterpret_cast<void*>(tmpl), tidRawMat) : nullptr;
        if (!desc) continue;
        uint32_t flags = *(uint32_t*)((uint8_t*)desc + 0x5c);
        if (foodTmpl == 0 && (flags & 0x20)) { foodTmpl = tmpl; foodFlags = flags; foodVol = *(int32_t*)((uint8_t*)desc + 0x48); }   // edible  (bit 5)
        if (fuelTmpl == 0 && (flags & 0x40)) { fuelTmpl = tmpl; fuelFlags = flags; fuelVol = *(int32_t*)((uint8_t*)desc + 0x48); }   // fuel    (bit 6)
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
    // Firewood stand-in size. Deliberately generous: the point is to take the first winter
    // OUT of the measurement, not to model a fuel economy. Override with
    // BANISHED_SEED_FUEL=<n> (0 disables) to A/B the cold death against a real producer
    // once one exists.
    int kSeedFuelPerHouse = 400;
    if (const char* fe = std::getenv("BANISHED_SEED_FUEL")) {
        int v = std::atoi(fe);
        if (v >= 0) kSeedFuelPerHouse = v;
    }
    int seeded = 0, fuelSeeded = 0;
    for (int64_t houseIBP : g_foundingHouseIBPs) {
        if (!houseIBP) continue;

        // Per-house need: `residents * (int)(CitizenDescription+0xc4)` + margin — the
        // HasSufficientResources threshold. Resident slots at rescomp+0x50/+0x58 (the
        // state node at +0x28 is NOT touched).
        void* rescomp = FindComponentOnEntity(reinterpret_cast<void*>(houseIBP), tidResidence);
        int residents = 0, reqInt = 0;
        if (rescomp) {
            int64_t slotArr = *(int64_t*)((uint8_t*)rescomp + 0x50);
            residents = slotArr ? *(int32_t*)(slotArr + 8) : 0;   // slot[0].count
            if (slotArr) {
                int64_t slot0data = *(int64_t*)slotArr;                    // slot[0].data
                int64_t resident = slot0data ? *(int64_t*)slot0data : 0;   // data[0]
                int64_t cd = resident ? *(int64_t*)(resident + 0x288) : 0; // CitizenDescription
                if (cd) reqInt = (int)*(float*)(cd + 0xc4);
            }
        }
        int threshold = residents * reqInt;
        int seedN = threshold + (residents > 0 ? residents : 0);  // strictly exceed the threshold
        if (seedN < unitsPerHouse) seedN = unitsPerHouse;
        if (seedN < 8) seedN = 8;                                 // minimum visible reserve
        // [SOAK KNOB] The default seed is deliberately "just above the HasSufficientResources
        // threshold" so the citizens do not spam GatherResources(id6) — which is the right
        // default, but it is also only ~130 units and a long run eats it. Once the firewood
        // stand-in stopped the cold death, STARVATION became the new wall: a 2026-07-26 soak
        // (seed 12345, FORCE_SPEED=40) held warmth at ~0 and still went 24 -> 3 citizens
        // between frames 6000 and 9000 on hunger alone. Raise this floor to push the food
        // economy out of the measurement window the same way BANISHED_SEED_FUEL does for
        // fuel. Neither is a fix; both are missing producers.
        if (const char* se = std::getenv("BANISHED_SEED_FOOD")) {
            int v = std::atoi(se);
            if (v > seedN) seedN = v;
        }
        if (seedN > 20000) seedN = 20000;                         // sanity clamp

        // Seed the ONE household container (StorageComponent c7e78). Faithful accept gate
        // (StorageComponent_helper_c560): the storage flag mask (+0x3c) must intersect the
        // material's flags (+0x5c).
        void* sc = FindComponentOnEntity(reinterpret_cast<void*>(houseIBP), tidStorage);
        if (!sc) continue;
        uint32_t mask = *(uint32_t*)((uint8_t*)sc + 0x3c);
        if ((mask & foodFlags) == 0) {
            fprintf(stderr, "[startTown::housing] house storage=%p mask=0x%x rejects edible "
                    "0x%x — no food seed\n", sc, mask, foodFlags);
            fflush(stderr);
            continue;
        }
        // ── Capacity clamp (2026-07-26) ────────────────────────────────────────────
        // A house's StorageComponent has ONE volume budget (+0x38, 1000 on the start
        // house) shared by every material, and AddItem does NOT enforce it — it only
        // checks the accept mask. So a generous stand-in seed silently overfills the
        // house, and then StorageComponent::CanAdd (0x14012cd80) refuses EVERY later
        // delivery, because its test is `unitVol*qty + QueryCount <= capacity`.
        //
        // That is not a hypothetical: BANISHED_SEED_FOOD=8000 put 8000 volume into a
        // 1000-volume house, so every firewood haul was refused at the door and bounced
        // back to the barn it came from — a closed loop that looked exactly like "the
        // haul never runs". Clamp the seed to the real budget, keep one carry-load of
        // headroom free so a delivery can always land, and say so when we clamp.
        const int cap = *(int32_t*)((uint8_t*)sc + 0x38);
        const int fuelVolume = (fuelTmpl != 0 && (mask & fuelFlags) != 0)
                             ? kSeedFuelPerHouse * (fuelVol > 0 ? fuelVol : 1) : 0;
        const int carryLoad = 100;   // Gather2's per-trip budget (townCap/citCap = 100)
        // BANISHED_SEED_FOOD_FIRST — GRADUATING to kDefaultOnFlags (verified 2026-07-31:
        // food lands at default fuel settings, zero CLAMPED lines, house packs 998/1000).
        // `=0` remains the kill-switch. Read via LiveEnabled so the table entry works.
        //
        // Reservation-ordering defect (found 2026-07-31): this budget reserves the full
        // REQUESTED fuel volume (kSeedFuelPerHouse * fuelVol = 400*4 = 1600), but the fuel
        // clamp below then runs SECOND and reduces fuel to whatever food left. So food
        // reserves against ~700 volume of fuel that never lands, and with the default fuel
        // seed `cap - fuelVolume - carryLoad` goes negative => maxFoodUnits == 0. Every
        // house starts with ZERO food, on pure defaults, and the town starves.
        //
        // Note the knob to turn is the FUEL seed, not the food one: maxFoodUnits is 0
        // whenever fuelVolume + carryLoad >= cap, so BANISHED_SEED_FOOD=6000, =130 and =1
        // all clamp to 0 identically. BANISHED_SEED_FUEL=150 is the zero-code workaround.
        //
        // Since the fuel clamp already takes the remainder, dropping this one subtraction
        // IS the fix. Flag off => expression byte-identical to before.
        // Read through LiveEnabled, NOT raw getenv: kDefaultOnFlags is consulted only
        // inside ComponentFactory::LiveEnabled, so a raw getenv here would make a table
        // entry a silent no-op — unset would stay OFF while `=0` still worked, i.e. it
        // would look graduated, pass the kill-switch test, and not actually be on.
        static int s_foodFirst = -1;
        if (s_foodFirst < 0)
            s_foodFirst = ::ComponentFactory::LiveEnabled("BANISHED_SEED_FOOD_FIRST") ? 1 : 0;
        int foodBudget = cap - carryLoad - (s_foodFirst ? 0 : fuelVolume);
        if (foodBudget < 0) foodBudget = 0;
        const int maxFoodUnits = foodBudget / (foodVol > 0 ? foodVol : 1);
        // Print in BOTH states so "flag on, nothing changed" is never ambiguous.
        fprintf(stderr, "[startTown::housing] seed budget: cap=%d carry=%d fuelReserve=%d "
                "-> foodBudget=%d maxFoodUnits=%d (food-first %s)\n",
                cap, carryLoad, (s_foodFirst ? 0 : fuelVolume), foodBudget, maxFoodUnits,
                s_foodFirst ? "ON" : "off");
        fflush(stderr);
        if (seedN > maxFoodUnits) {
            fprintf(stderr, "[startTown::housing] house storage=%p CLAMPED food seed %d -> %d "
                    "units (cap=%d, fuel reserves %d, keeping %d free): an over-capacity "
                    "house makes CanAdd refuse every later delivery\n",
                    sc, seedN, maxFoodUnits, cap, fuelVolume, carryLoad);
            fflush(stderr);
            seedN = maxFoodUnits;
        }

        if (seedN > 0) addItem(reinterpret_cast<int64_t>(sc), foodTmpl, seedN);
        ++seeded;

        // Firewood stand-in (see the note at the template pick). The heat tick burns one
        // unit per burn interval per house, so this is a season count, not a per-resident
        // one; the same storage accept gate applies (mask +0x3c vs material flags +0x5c).
        if (fuelTmpl != 0 && (mask & fuelFlags) != 0) {
            // Same volume budget as the food seed above — clamp against what is left.
            int fuelUnits = kSeedFuelPerHouse;
            const int freeVol = cap - seedN * (foodVol > 0 ? foodVol : 1) - carryLoad;
            const int maxFuelUnits = (freeVol > 0 ? freeVol : 0) / (fuelVol > 0 ? fuelVol : 1);
            if (fuelUnits > maxFuelUnits) {
                fprintf(stderr, "[startTown::housing] house storage=%p CLAMPED fuel seed %d -> %d "
                        "units (cap=%d, food takes %d, keeping %d free)\n",
                        sc, fuelUnits, maxFuelUnits, cap,
                        seedN * (foodVol > 0 ? foodVol : 1), carryLoad);
                fflush(stderr);
                fuelUnits = maxFuelUnits;
            }
            if (fuelUnits > 0) {
                addItem(reinterpret_cast<int64_t>(sc), fuelTmpl, fuelUnits);
                ++fuelSeeded;
            }
        }
        // Verify through the exe-faithful read path: GetResourceCount(rescomp) walks
        // owner→StorageComponent and sums the edible units we just added.
        int have = (getResCount && rescomp) ? getResCount(reinterpret_cast<int64_t>(rescomp)) : -1;
        fprintf(stderr, "[startTown::housing] house food(c7e78): storage=%p residents=%d "
                "reqPerResident=%d threshold=%d seeded=%d -> GetResourceCount(res)=%d %s\n",
                sc, residents, reqInt, threshold, seedN, have,
                (have >= threshold) ? "(SUFFICIENT: no Gather)" : "(WARN: still insufficient)");
        fflush(stderr);
    }
    fprintf(stderr, "[startTown::housing] seeded food into %d/%zu houses (%d units each, "
            "edibleTmpl=%llx flags=0x%x)\n",
            seeded, g_foundingHouseIBPs.size(), unitsPerHouse,
            (unsigned long long)foodTmpl, foodFlags);
    fprintf(stderr, "[startTown::housing] seeded FIREWOOD STAND-IN into %d/%zu houses "
            "(%d units each, fuelTmpl=%llx flags=0x%x)%s\n",
            fuelSeeded, g_foundingHouseIBPs.size(), kSeedFuelPerHouse,
            (unsigned long long)fuelTmpl, fuelFlags,
            fuelTmpl == 0 ? "  <-- NO fuel raw-material template found (bit 6)" : "");
    fflush(stderr);
}

// ── BANISHED_FOOD_TOPUP=<frames> — periodic food abundance (test scaffolding) ──────
// StartTown_SeedHouseInventory is a ONE-SHOT: it fills each house to the seed budget and
// nothing ever refills it. Measured 2026-08-15 (seed 12345, FORCE_SPEED=40, 25 citizens):
// the town eats ~600 units per 1000 frames, so a 1800-unit start stock is gone by frame
// 5000 and the whole population starves between 6000 and 8000. That die-off dominates
// EVERY long-run measurement — including the demographic ones — because it kills the
// cohort before any of them can be observed.
//
// It also silently suppresses the life-planning section in CitizenComponent::Update: that
// whole block (housing / workplace / MARRIAGE) runs only while the citizen's action stack
// is empty (*(self+0x278)+0x60 == -1). A starving citizen is permanently inside a
// food-seeking action, so it never marries and never moves out. "Nobody marries" and
// "everybody starves" are therefore the SAME observation, and the marriage path cannot be
// judged at all until food is off the table.
//
// This refills food on the same faithful path the one-shot seed uses (AddItem 0x14012d000
// into the house's own StorageComponent) and to the SAME budget, so it cannot overfill:
// `target` is the seed's own maxFoodUnits, which already reserves the fuel volume and one
// carry-load of headroom, and we only ever add `target - have`. Nothing else is touched.
//
// This is NOT a food economy and is not a fix for one — it is the "enough food" half of
// the question "given enough food and housing, does the births/deaths cycle sustain a
// population?". Test-only, opt-in, default 0 (byte-unchanged when unset).
// extern "C": the call site is a block-scope `extern void` declaration inside
// GameUpdateLoop_Update, which is itself an extern "C" function — so that declaration
// inherits C linkage and a C++-mangled definition here does not satisfy it.
extern "C" void StartTown_FoodTopUp_Tick(int frame) {
    static int s_period = -1;
    if (s_period < 0) {
        const char* e = std::getenv("BANISHED_FOOD_TOPUP");
        s_period = (e && e[0]) ? std::atoi(e) : 0;
        if (s_period > 0) {
            fprintf(stderr, "[FoodTopUp] ENABLED — refilling house food to the seed budget "
                            "every %d frames (test scaffolding, not a producer)\n", s_period);
            fflush(stderr);
        }
    }
    if (s_period <= 0 || frame <= 0 || (frame % s_period) != 0) return;
    if (g_foundingHouseIBPs.empty()) return;

    void* townMgr = nullptr;
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton)
        townMgr = *(void**)((uint8_t*)*g_pGameWorldSingleton + 0xf0);
    if (!townMgr) return;

    typedef void (*fn_AddItem_t)(int64_t storage, int64_t material, int n);   // 0x14012d000
    fn_AddItem_t addItem = reinterpret_cast<fn_AddItem_t>(ResolveAddr(Module::Exe, 0x14012d000));
    typedef int (*fn_GetResCount_t)(int64_t comp);                            // 0x14012a9d0
    fn_GetResCount_t getResCount =
        reinterpret_cast<fn_GetResCount_t>(ResolveAddr(Module::Exe, 0x14012a9d0));
    // StorageComponent::QueryCount — the OCCUPIED VOLUME of the container, the same value
    // StorageComponent::CanAdd (0x14012cd80) tests against capacity (+0x38). getResCount
    // above only sums EDIBLE units, so it cannot see firewood; this can.
    typedef uint32_t (*fn_QueryCount_t)(void* self);                          // 0x14012cf10
    fn_QueryCount_t queryCount =
        reinterpret_cast<fn_QueryCount_t>(ResolveAddr(Module::Exe, 0x14012cf10));
    if (!addItem || !getResCount) return;

    // Same edible-template pick as the one-shot seed (RawMaterialDescription+0x5c bit 5).
    const uint32_t tidRawMat = TypeId("RawMaterialDescription");
    const int   tmplCount = *(int32_t*)((uint8_t*)townMgr + 0x118);
    const int64_t tmplArr = *(int64_t*)((uint8_t*)townMgr + 0x110);
    int64_t  foodTmpl = 0, fuelTmpl = 0;
    uint32_t foodFlags = 0, fuelFlags = 0;
    int      foodVol = 1, fuelVol = 1;
    for (int t = 0; t < tmplCount && (foodTmpl == 0 || fuelTmpl == 0); ++t) {
        int64_t tmpl = tmplArr ? *(int64_t*)(tmplArr + (int64_t)t * 8) : 0;
        void* desc = tmpl ? FindDescriptionOnTemplate(reinterpret_cast<void*>(tmpl), tidRawMat) : nullptr;
        if (!desc) continue;
        const uint32_t flags = *(uint32_t*)((uint8_t*)desc + 0x5c);
        if (foodTmpl == 0 && (flags & 0x20)) { foodTmpl = tmpl; foodFlags = flags; foodVol = *(int32_t*)((uint8_t*)desc + 0x48); }
        if (fuelTmpl == 0 && (flags & 0x40)) { fuelTmpl = tmpl; fuelFlags = flags; fuelVol = *(int32_t*)((uint8_t*)desc + 0x48); }
    }
    if (foodTmpl == 0) return;

    const uint32_t tidStorage   = TypeId("StorageComponent");
    const uint32_t tidResidence = TypeId("ResidenceComponent");
    int kSeedFuelPerHouse = 400;
    if (const char* fe = std::getenv("BANISHED_SEED_FUEL")) {
        const int v = std::atoi(fe);
        if (v >= 0) kSeedFuelPerHouse = v;
    }
    static int s_foodFirst = -1;
    if (s_foodFirst < 0)
        s_foodFirst = ::ComponentFactory::LiveEnabled("BANISHED_SEED_FOOD_FIRST") ? 1 : 0;

    int houses = 0, refilled = 0, unitsAdded = 0, fuelRefilled = 0, fuelAdded = 0;
    for (int64_t houseIBP : g_foundingHouseIBPs) {
        if (!houseIBP) continue;
        void* sc = FindComponentOnEntity(reinterpret_cast<void*>(houseIBP), tidStorage);
        if (!sc) continue;
        const uint32_t mask = *(uint32_t*)((uint8_t*)sc + 0x3c);
        if ((mask & foodFlags) == 0) continue;
        ++houses;

        // Identical budget to the one-shot seed — see the clamp note there.
        const int cap = *(int32_t*)((uint8_t*)sc + 0x38);
        const int fuelVolume = (fuelTmpl != 0 && (mask & fuelFlags) != 0)
                             ? kSeedFuelPerHouse * (fuelVol > 0 ? fuelVol : 1) : 0;
        const int carryLoad = 100;
        // ⚠The top-up ALWAYS reserves the fuel volume, even though the one-shot seed skips
        // that subtraction under SEED_FOOD_FIRST. Reason: the seed runs once, at a moment
        // when fuel is added right after it, so "food first" is safe there. Here food is
        // re-topped every period, and food-first meant food re-claimed the whole budget on
        // every tick and fuel got exactly ZERO free volume forever — measured: "fuel 0/30
        // house(s) +0 unit(s)" on every line while the town marched toward freezing. A
        // sustained town needs BOTH, so both get a share.
        (void)s_foodFirst;
        int foodBudget = cap - carryLoad - fuelVolume;
        if (foodBudget < 0) foodBudget = 0;
        const int target = foodBudget / (foodVol > 0 ? foodVol : 1);
        if (target <= 0) continue;

        void* rescomp = FindComponentOnEntity(reinterpret_cast<void*>(houseIBP), tidResidence);
        const int have = rescomp ? getResCount(reinterpret_cast<int64_t>(rescomp)) : 0;
        const int add  = target - have;
        if (add > 0) {
            addItem(reinterpret_cast<int64_t>(sc), foodTmpl, add);
            ++refilled;
            unitsAdded += add;
        }

        // ── FIREWOOD ───────────────────────────────────────────────────────────────
        // Measured 2026-08-15, and it is what ended the first successful soak: food alone
        // is NOT enough to keep a town alive. The residence heat tick burns one fuel unit
        // per burn interval out of this same StorageComponent, `BANISHED_SEED_FUEL` is a
        // ONE-SHOT seed, and standalone has no woodcutter — so the reserve simply runs out.
        // A town that had grown 22 -> 77 over 32000 frames froze SOLID between frames
        // 32000 and 34000: the last census read warmth(+0x48) avg 13.2 / max 19.0 against
        // a death threshold of 20.0, i.e. the whole population was one tick from the edge.
        // Refilling food and not fuel just moves the wall.
        //
        // Sized off the container's own occupied volume (QueryCount, the value CanAdd
        // tests) rather than a unit count, because food and fuel share ONE volume budget —
        // topping fuel up blind is exactly how the original seed overfilled the house and
        // made CanAdd refuse every later delivery.
        if (fuelTmpl != 0 && (mask & fuelFlags) != 0 && queryCount) {
            const int used = static_cast<int>(queryCount(sc));
            const int freeVol = cap - carryLoad - used;
            const int addFuel = (freeVol > 0 ? freeVol : 0) / (fuelVol > 0 ? fuelVol : 1);
            if (addFuel > 0) {
                addItem(reinterpret_cast<int64_t>(sc), fuelTmpl, addFuel);
                ++fuelRefilled;
                fuelAdded += addFuel;
            }
        }
    }
    fprintf(stderr, "[FoodTopUp] frame %d: refilled %d/%d house(s) +%d food unit(s); "
            "fuel %d/%d house(s) +%d unit(s)\n",
            frame, refilled, houses, unitsAdded, fuelRefilled, houses, fuelAdded);
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

    // Kid gender alternates across the whole spawn (retail's `bVar12`), persisting
    // between families — CitizenComponent_impl_0dd0 initialises it ONCE, above the
    // family loop, and only ever toggles it (`bVar12 = bVar12 == false`) per child.
    //
    // It starts TRUE, and sub_1040 derives gender as `(uint)(param_5 == '\0')`, so the
    // first child is gender 0; SetVariant then gets `(ushort)(gender == 0)` = variantIdx
    // 1. So kidVariant starts at 1, not 0 — starting at 0 put every founding child's
    // gender/mesh one step out of phase with vanilla for the whole spawn.
    int kidVariant = 1;

    // Retail spawn (founding_spawn.cpp) vs the hand-rolled SpawnOneCitizen — mutually
    // exclusive, see the extern block above. The family cross-links are impl_0dd0's own
    // work and are unconditional in retail, so the retail path always performs them.
    const bool retailSpawn = StartTown_FoundingSpawnEnabled();
    const bool doLinks     = retailSpawn || FamilyLinksOn();
    fprintf(stderr, "[startTown::citizen] spawn path = %s (BANISHED_LIVE_FOUNDING_SPAWN), "
            "familyLinks=%d\n", retailSpawn ? "RETAIL sub_1040" : "hand-rolled SpawnOneCitizen",
            (int)doLinks);
    fflush(stderr);

    int spawned = 0;
    for (int f = 0; f < famCount; ++f) {
        uint8_t* fam = families + (size_t)f * FAMILY_STRIDE;
        const int educated    = *(int8_t*)fam;
        const int maxChildren = *(int32_t*)(fam + 4);
        famEntities.emplace_back();
        std::vector<void*>& fe = famEntities.back();

        // Two adults — a couple: opposite gender models (variant groups 0 and 1),
        // random adult age, full-size. Matches CitizenComponent_impl_0dd0.
        uint8_t* adultA = nullptr;   // retail local_res18 — variant 0 / gender 1
        uint8_t* adultB = nullptr;   // retail local_res8  — variant 1 / gender 0 (ring host)
        if (retailSpawn) {
            // Each adult gets its OWN RandFloat age draw immediately before its spawn —
            // that is one draw in the shared world RNG stream, so it must not be hoisted
            // out of the pair. genderSel '\0' then '\x01', matching impl_0dd0.
            float ageA = StartTown_FoundingAdultAge(citizenTemplate);
            if (void* a = StartTown_SpawnColonistRetail(startConfig, citizenTemplate, entityList,
                                                        base, ageA, '\0', (char)educated, &adultA)) {
                ++spawned; fe.push_back(a);
                CitizenCarryStorageInit(a);   // standalone-only: inert StorageComponent OnAdded
            }
            float ageB = StartTown_FoundingAdultAge(citizenTemplate);
            if (void* b = StartTown_SpawnColonistRetail(startConfig, citizenTemplate, entityList,
                                                        base, ageB, '\x01', (char)educated, &adultB)) {
                ++spawned; fe.push_back(b);
                CitizenCarryStorageInit(b);
            }
        } else {
            if (void* a = SpawnOneCitizen(citizenTemplate, entityList, base, jitter, rng, /*isChild=*/false, /*variantIdx=*/0, &adultA)) { ++spawned; fe.push_back(a); }
            if (void* b = SpawnOneCitizen(citizenTemplate, entityList, base, jitter, rng, /*isChild=*/false, /*variantIdx=*/1, &adultB)) { ++spawned; fe.push_back(b); }
        }

        // Spouse cross-link (impl_0dd0: res18->+0x250 = res8; res8->+0x250 = res18).
        // Retail stores the raw component pointer taken out of the CheckPtr'd IBP.
        int linkedKids = 0;
        if (doLinks && adultA && adultB) {
            *reinterpret_cast<int64_t*>(adultA + 0x250) = reinterpret_cast<int64_t>(adultB);
            *reinterpret_cast<int64_t*>(adultB + 0x250) = reinterpret_cast<int64_t>(adultA);
        }

        // Children: Rand(0.._maxChildCount) — alternating gender, child scale (0.66).
        int kids = 0;
        if (rng && fn_Random_Rand && maxChildren >= 0)
            kids = (int)fn_Random_Rand(rng, maxChildren + 1);
        for (int c = 0; c < kids; ++c) {
            uint8_t* kid = nullptr;
            if (retailSpawn) {
                // kidVariant is retail's bVar12 in the variant encoding: bVar12 true ==
                // genderSel '\x01' == gender 0 == variantIdx 1. educated is always '\0'
                // for children in impl_0dd0 (sub_1040 would force it anyway).
                float ageK = StartTown_FoundingChildAge(citizenTemplate);
                if (void* k = StartTown_SpawnColonistRetail(startConfig, citizenTemplate, entityList,
                                                            base, ageK,
                                                            (kidVariant == 1) ? '\x01' : '\0',
                                                            '\0', &kid)) {
                    ++spawned; fe.push_back(k);
                    CitizenCarryStorageInit(k);
                }
            } else {
                if (void* k = SpawnOneCitizen(citizenTemplate, entityList, base, jitter, rng, /*isChild=*/true, /*variantIdx=*/kidVariant, &kid)) { ++spawned; fe.push_back(k); }
            }
            // impl_0dd0 @0x1400a0fcd: helper_68a0(res8, &child) — the child joins the
            // SECOND adult's ring (+0x260 head, child +0x258 host back-ref).
            if (doLinks && adultB && kid) {
                EnsureFamilyFuncs();
                if (s_fn_JoinFamily_68a0) {
                    int64_t node = reinterpret_cast<int64_t>(kid);
                    s_fn_JoinFamily_68a0(adultB, &node);
                    ++linkedKids;
                }
            }
            kidVariant ^= 1;
        }

        fprintf(stderr, "[startTown::citizen] family[%d] educated=%d maxKids=%d kids=%d "
                "spouseLink=%d ringKids=%d\n",
                f, educated, maxChildren, kids,
                (doLinks && adultA && adultB) ? 1 : 0, linkedKids);
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

// f2f0 notify targets (ported + registered elsewhere; linked by name).
extern void Pf_FineCellDirty_a8c90(int64_t self, int x, int y);                       // action_pf_road.cpp
extern void Pf_EnqueueCmd2_a6fe0(int64_t self, uint32_t idx, void* type, void* p4);   // action_pf_astar.cpp

// FUN_14009f2f0 — write cell+0x29 = `type`, set the Game+0xa2 dirty flag, and run the
// two notify calls: a8c90 (region-dirty, subsystem @ game+0x70, boundary-flip only) and
// a6fe0 (pathfinder cmd-2, worker @ game+0x68, ALWAYS).
//
// ⚠ These were guarded off under "Game+0x68/+0x70 are zero in standalone" — STALE since
// Pathfinder_Seed moved before StartTown (gameupdateloop_load.cpp). Skipping a6fe0 left
// the pathfinder's byte MIRROR (pf+0x158) holding the pre-town classification, so the
// road A* (which reads the mirror, not the grid) accepted cells the preview painted red
// — the "places anywhere" validity gap. Null-subsystem checks keep the old safety.
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
    const int dim = *(int32_t*)game;
    int64_t regionObj = *(int64_t*)(game + 0x70);
    if (dirty && regionObj && dim > 0)
        Pf_FineCellDirty_a8c90(regionObj, idx % dim, idx / dim);
    int64_t pfThread = *(int64_t*)(game + 0x68);
    if (pfThread)
        Pf_EnqueueCmd2_a6fe0(pfThread, (uint32_t)idx,
                             (void*)(uintptr_t)newType, nullptr);
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
                // dee0's index math, verified against the retail instructions at
                // 0x14009dd84-0x14009ddda (dc20 uses the identical form). The shape
                // string is in UNROTATED description space, first row = LAST tile row,
                // stride = unrotatedW+1 (the newline). The old math here was mirrored
                // in all four rotations — latent because resources are almost all 1x1.
                const bool sw = (rot == 0x5a || rot == 0x10e);
                const int w0 = sw ? h : w;   // unrotated shape-grid width
                const int h0 = sw ? w : h;
                int gx, gy;
                if (rot == 0x5a)          { gy = ix;            gx = iy; }
                else if (rot == 0xb4)     { gy = iy;            gx = (w0 - 1) - ix; }
                else if (rot == 0x10e)    { gy = (h0 - 1) - ix; gx = (w0 - 1) - iy; }
                else                      { gy = (h0 - 1) - iy; gx = ix; }
                const long sidx = (long)gy * (w0 + 1) + gx;
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

            // Occupancy from the rot-aware shape grid (same verified math as the mark —
            // dee0/dc20 form, retail instructions 0x14009dd84-0x14009ddda; must stay in
            // lockstep with StartTown_MarkResourceCellGrid or mark/unmark desync).
            bool occupied;
            if (!shape || shapeLen == 0) {
                occupied = true;
            } else {
                const bool sw = (rot == 0x5a || rot == 0x10e);
                const int w0 = sw ? h : w;   // unrotated shape-grid width
                const int h0 = sw ? w : h;
                int gx, gy;
                if (rot == 0x5a)          { gy = ix;            gx = iy; }
                else if (rot == 0xb4)     { gy = iy;            gx = (w0 - 1) - ix; }
                else if (rot == 0x10e)    { gy = (h0 - 1) - ix; gx = (w0 - 1) - iy; }
                else                      { gy = (h0 - 1) - iy; gx = ix; }
                const long sidx = (long)gy * (w0 + 1) + gx;
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
// The 5cc0 initial sub-resource seed for one scattered resource, factored out so the
// correct (post-register) call site and the BANISHED_SPAWN_INITIAL_PREREG A/B arm run
// byte-identical code and differ ONLY in when they are called. See the ordering comment
// at the call site and notes/generation_deviations.md deviation #4.
static void SeedInitialSpawn(void* ent, uint32_t tidSpawnComp,
                             int& withSpawnComp, int& withSpawnEntries) {
    if (!ent) return;
    void* sc = FindComponentOnEntity(ent, tidSpawnComp);
    if (!sc) return;
    ++withSpawnComp;
    void* sd = *(void**)((uint8_t*)sc + 0x18);   // SpawnDescription
    if (sd && *(int32_t*)((uint8_t*)sd + 0x28) > 0) ++withSpawnEntries;
    if (withSpawnComp <= 3)
        fprintf(stderr, "[startTown::res]   spawnComp[%d] sc=%p desc=%p "
                "entries=%d array=%p\n", withSpawnComp, sc, sd,
                sd ? *(int32_t*)((uint8_t*)sd + 0x28) : -1,
                sd ? *(void**)((uint8_t*)sd + 0x20) : nullptr);
    static const bool s_initialSeed =
        ::ComponentFactory::LiveEnabled("BANISHED_SPAWN_INITIAL");
    if (s_initialSeed)
        SpawnComponent_InitialSpawn(reinterpret_cast<SpawnComponent*>(sc));
}

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
    // Wild-food chain diag: the gatherer's four accepted resources are spawned by the
    // TREE's SpawnComponent (NaturalResourceTree.rsc's SpawnDescription), not by any
    // worldgen pool. Count how many scattered resources actually carry that component
    // and a populated SpawnDescription, so "no wild food" can never be confused
    // between "never instantiated" and "instantiated but never ticked".
    const uint32_t tidSpawnComp = TypeId("SpawnComponent");
    int withSpawnComp = 0, withSpawnEntries = 0;

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
        int zoneSkips   = 0;   // cells refused by the ZONE-raster test below
        bool capped = false;
        for (int y = 0; y < dim && !capped; ++y) {
            for (int x = 0; x < dim; ++x) {
                float p = fn_PerlinNoise_Generate(perlin, (float)x * scale, (float)y * scale);
                if (threshold > p * 0.5f + 0.5f) continue;
                if (fn_Random_RandFloat(rng) > density) continue;

                const int idx = dim * y + x;
                if (*(void**)(cells + (size_t)idx * CELL_STRIDE + CELL_OCCUPANT)) continue;  // occupied
                // ...and skip cells claimed in the ZONE raster (cell+0) even when the
                // ENTITY raster (cell+8) is free. This pass runs AFTER the structures are
                // placed (measured: dock at log line 381, scatter at 482), so it can drop
                // a tree inside a finished building. cell+8 alone does not catch that,
                // because BANISHED_LIVE_WALKWAY_DROPCELLS (default on) deliberately leaves
                // a dock's '_' walkway cells at cell+8 == 0 — which is exactly where its
                // mode-2 drop spots sit.
                //
                // Retail needs no such test: dc20 claims every mask char but '.'
                // (`cmp $0x2e,%cl ; setne %bl`), so in retail cell+8 on those cells holds
                // the dock and the existing check suffices. This is the compensating half
                // of our walkway deviation and should be removed with it.
                //
                // It is not cosmetic. A tree on a drop cell has NaturalResource/Growth/
                // Spawn and NO WorkComponent, and FishingWork slot 4's fallback derefs
                // that component's +0x28 unguarded — faithfully (retail 0x1400ea775).
                // Reproduced at seed 999: tree on the dock's spot[3] (88,124), the one
                // spot of four that never cycled while the other three ran
                // 0 -> pile -> 0 normally.
                if (*(void**)(cells + (size_t)idx * CELL_STRIDE)) { ++zoneSkips; continue; }
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

                // [A/B HARNESS] BANISHED_SPAWN_INITIAL_PREREG=1 restores the OLD,
                // WRONG order (seed before register) purely so the crash mechanism
                // can be re-measured on demand. Never set this outside an A/B run —
                // it reproduces the Scene::Clean ACCESS_VIOLATION by design.
                static const bool s_preRegSeed =
                    std::getenv("BANISHED_SPAWN_INITIAL_PREREG") != nullptr;
                if (s_preRegSeed) SeedInitialSpawn(ent, tidSpawnComp,
                                                   withSpawnComp, withSpawnEntries);

                fn_EntityList_RegisterEntity(entityList, &entIBP, category);

                // Wild-food chain diag (see tidSpawnComp above) + the initial
                // sub-resource seed (ComponentEntity_impl_5cc0, retail call site
                // 0x1400a1cb2, guarded there on the same "does this entity carry a
                // SpawnComponent" test). This is what puts the gatherer's four
                // accepted resources on the map at t=0; it was deviation #4 in
                // notes/generation_deviations.md. Opt-in (BANISHED_SPAWN_INITIAL)
                // until measured — it runs the full DoSpawn closure thousands of
                // times during world build.
                //
                // ORDERING IS LOAD-BEARING — it must run AFTER RegisterEntity.
                // Verified in Ghidra 2026-08-13: a12b0 registers the parent at
                // 0x1400a1bf2 (CALL 0x1400c48a0) and only then finds the
                // SpawnComponent (0x1400a1c9e) and calls 5cc0 (0x1400a1cb2).
                // Registration is not bookkeeping: 0x1400c48a0 tails into
                // FUN_1400c2f60, which dispatches component vtable +0x30 (slot 6,
                // OnAdded) over the entity's component array — the pass that puts
                // the parent's ModelComponent into Graphics::Scene. Seeding children
                // first spawned them into a scene the parent had not yet joined.
                if (!s_preRegSeed)
                    SeedInitialSpawn(ent, tidSpawnComp, withSpawnComp, withSpawnEntries);

                if (ent) { ++spawned; ++poolSpawned; }
                if (poolSpawned >= poolCap) { capped = true; break; }
            }
        }
        fprintf(stderr, "[startTown::res] [%d] tmpl=%p desc=%p thr=%.3f dens=%.3f h[%.2f,%.2f) slope>=%.2f -> %d%s zoneSkips=%d\n",
                e, tmpl, desc, threshold, density, hMin, hMax, slopeMin, poolSpawned,
                capped ? " (CAPPED)" : "", zoneSkips);
        fflush(stderr);
    }
    fprintf(stderr, "[startTown::res] SpawnComponent carriers: %d of %d scattered "
            "resources have one, %d of those have a non-empty SpawnDescription "
            "(tid(SpawnComponent)=%u)\n",
            withSpawnComp, spawned, withSpawnEntries, tidSpawnComp);
    fflush(stderr);
    // Everything DoSpawn did up to here came from the worldgen seed pass, not from
    // any per-tick Update — so this line attributes the initial wild-food placement.
    SpawnComponent_ReportDoSpawnCensus("worldgen-seed");
    // dc20's stale-occupant eviction tally at the same point. A spawned child sitting
    // on a cell the parent later claims gets DESTROYED here while its ModelComponent is
    // already linked into Graphics::Scene — the dangling node Scene::Clean faults on.
    // With the correct (post-register) order the parent claims first and DoSpawn's
    // TileFree rejects those cells, so this should stay at its baseline.
    MapComponent_ReportDc20Evictions("worldgen-seed");
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
    const bool ambientLive = ::ComponentFactory::LiveEnabled("BANISHED_AMBIENT_AUDIO");
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
    // townMgr+0xa0 (the town-hall statistics list). Houses register into
    // townMgr+0x20 via the ResidenceComponent state-2 Enter on the first SM tick
    // (retail flow) — no manual append here anymore.
    StartTown_RegisterTownCitizens();

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

    // The demo gatherer hut was sited before any wild food existed; retake its
    // accepted-resource count now that the scatter + seed pass have run.
    StartTown_RecountGatherable();

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

    // AGRICULTURE availability (ungated) — the three agri-pool loops from the tail
    // of FUN_1400a3300. Registers the town's starting crops/livestock/orchards into
    // townMgr+0x120 so the Town Hall Agri tab brightens them instead of showing
    // every icon greyed. Independent of live storage; must run after the town
    // manager exists (it does — gw+0xf0 set during world build).
    // The third pool loop also spawns the town's STARTING HERD (retail 0x1400a35dc),
    // which is why this now needs the spawn tile: on Easy (_livestockCount=1,
    // _livestockGroupCount=8) it puts 8 free-roaming animals of one rolled type near the
    // town centre. Medium/Hard set _livestockCount=0 and correctly get none.
    StartTown_SeedAvailableAgriculture(sc, game, spawnX, spawnY);

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
