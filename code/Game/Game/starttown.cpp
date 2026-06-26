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
#ifdef STANDALONE_EXE
#include "Component/component_factory_base.h"   // ComponentFactory::LiveEnabled
#endif
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>

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

// Owner-entity IBP values of the spawned residence (WoodHouse) buildings, captured by
// StartTown_RunOnCreatePass when each house's ResidenceComponent::OnCreate fires. Used by
// StartTown_AssignFoundingHousing to pair families to houses (townMgr+0x20 is never
// populated in standalone — see the capture site). Cleared at the start of each spawn.
static std::vector<int64_t> g_foundingHouseIBPs;

// Faithful CitizenComponent::AssignHousing (0x140111ee0, citizencomponent.cpp). Sets the
// citizen's residence ref (+0x248) AND rosters the citizen into the house's
// WorkPlaceComponent occupant list (+0x58/+0x60) via the attach helper (FUN_14012fa20).
// Used by StartTown_AssignFoundingHousing.
struct CitizenComponent;
extern void CitizenComp_AssignHousing(CitizenComponent* self, int64_t* housingPtr);

// WorkPlaceComponent attach/detach roster-only mode (workplacecomponent.cpp). Set true for
// the build-time founding-housing pass so the roster is populated WITHOUT the gameplay-time
// Scene mesh-visual toggle (which deadlocks mid-build). See the flag's definition.
extern bool g_wpcRosterOnly;

// Apply the occupied end-state visual for a rostered house (ModelKick mode 2 + StatusIcon
// toggle). MUST run at gameplay time (live scene). See its definition in workplacecomponent.cpp.
extern void WorkplaceComponent_ApplyOccupiedVisual(int64_t comp);

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
// standalone-safe. Still sets the 0x400 "OnCreate done" sentinel.
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
    *entFlags |= 0x400;                                // OnCreate-done sentinel
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

        // Foundation F — set the ModelComponent world transform so its OnAdded
        // (ModelComponent_Draw, fired by RegisterEntity -> OnAddedPass below) places
        // the mesh at the building footprint centre. Mirrors the transform block of
        // ComponentEntity_impl_1fc0 (call site 0x1400a24bc): footprint centre =
        // midpoint of the origin/far corner terrain positions; yaw = raw orient;
        // pitch = 0. ModelComponent_Draw then calls Geometry::SetTransform(+0x28,
        // +0x148). Without this the mesh would render at the world origin.
        if (game && fn_MapGetTerrainPos && fn_Transform_FromYawPitchOffset) {
            void* modelComp = FindComponentOnEntity(entity, tidModelComponent);
            if (modelComp) {
                uintptr_t mcIBP = 0;
                fn_InternalBasePtr_Ctor(&mcIBP, modelComp);
                fn_InternalBasePtr_CheckPtr(&mcIBP);
                uint8_t* mc = (uint8_t*)mcIBP;
                int dim = *(int32_t*)game;
                if (mc && dim > 0) {
                    const float half = (float)(dim >> 1);
                    // Origin corner (tileX, tileY) and far corner (tileX+fpW, tileY+fpH).
                    int idx1 = dim * tileY + tileX;
                    float in1[3] = { (float)(idx1 % dim) - half, (float)(idx1 / dim) - half, 0.0f };
                    float p1[3] = {0,0,0};
                    fn_MapGetTerrainPos(game, p1, in1, 0, nullptr);
                    int idx2 = (tileY + fpH) * dim + tileX + fpW;
                    float in2[3] = { (float)(idx2 % dim) - half, (float)(idx2 / dim) - half, 0.0f };
                    float p2[3] = {0,0,0};
                    fn_MapGetTerrainPos(game, p2, in2, 0, nullptr);
                    float center[3] = { (p1[0]+p2[0])*0.5f, (p1[1]+p2[1])*0.5f, (p1[2]+p2[2])*0.5f };
                    float yaw = (float)orient, pitch = 0.0f;
                    // Writes the Transform straight into ModelComponent+0x148.
                    fn_Transform_FromYawPitchOffset((void*)(mc + 0x148), &yaw, &pitch, center);
                    *(float*)(mc + 0x168) = yaw;
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
extern "C" int ModelComponent_RandomizeVariant(void* modelComp);

// Gate: BANISHED_CITIZEN_VARIANT. Default off => citizens render byte-identical
// to before (all share the description's fixed variant).
static bool CitizenVariantOn() {
    static int en = -1;
    if (en < 0) {
        const char* e = std::getenv("BANISHED_CITIZEN_VARIANT");
        en = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return en == 1;
}

// Give a freshly-instantiated citizen entity a per-citizen model variant so the
// colonists are not all visually identical. The original picks this in the
// deferred CitizenComponent init (guarded off in standalone); here we reproduce
// only the safe mesh-swap, before the entity is registered so its OnAdded adds
// the chosen mesh. No-op unless BANISHED_CITIZEN_VARIANT is set.
static void SetCitizenModelVariant(void* entity, uint32_t tidModel) {
    if (!CitizenVariantOn() || !entity || tidModel == 0 ||
        !fn_InternalBasePtr_Ctor || !fn_InternalBasePtr_CheckPtr)
        return;
    void* mc = FindComponentOnEntity(entity, tidModel);
    if (!mc) return;
    uintptr_t mcIBP = 0;
    fn_InternalBasePtr_Ctor(&mcIBP, mc);
    fn_InternalBasePtr_CheckPtr(&mcIBP);
    if (!mcIBP) return;
    int chosen = ModelComponent_RandomizeVariant((void*)mcIBP);
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
}

// Instantiate one colonist entity from the resolved citizen template and register
// it with the world. Returns the raw entity pointer (for optional later linking)
// or nullptr. CitizenComponent initialisation (FUN_140117080) is deferred — see
// the section header.
static void* SpawnOneCitizen(void* citizenTemplate, void* entityList) {
    if (!citizenTemplate) return nullptr;
    void* mem = fn_MemAlloc(g_pMemAllocSingleton, ENTITY_SIZE, 8);
    void* ent = mem ? fn_EntityFactory_CreateEntity(mem, citizenTemplate) : nullptr;
    uintptr_t entIBP = 0;
    fn_InternalBasePtr_Ctor(&entIBP, ent);

    // Per-citizen model variant — after instantiation (Serialize ran, +0x166 set)
    // and BEFORE registration (so OnAdded adds the chosen mesh to the scene).
    if (ent) {
        fn_InternalBasePtr_CheckPtr(&entIBP);
        SetCitizenModelVariant((void*)entIBP, TypeId("ModelComponent"));
    }

    int category = *(int32_t*)((uint8_t*)citizenTemplate + TEMPLATE_CATEGORY);
    fn_EntityList_RegisterEntity(entityList, &entIBP, category);
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
// itself is never populated in standalone) and set each member's residence reference
// (CitizenComponent+0x248 = house). That is the citizen-side half of AssignHousing
// (0x140111ee0) — the field the per-tick Update reads as "housed". The faithful roster-ATTACH
// half (the house tracking its occupants + "occupied" mesh visuals) is a separate type-id/
// visual domino chain, documented at the assignment site. Requires BANISHED_LIVE_RESIDENCE
// (else no houses are captured). Gated on BANISHED_LIVE_HOUSING; default build is a no-op.
static bool HousingEnvOn() {
    static int en = -1;
    if (en < 0) {
        const char* e = std::getenv("BANISHED_LIVE_HOUSING");
        en = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return en == 1;
}

static void StartTown_AssignFoundingHousing(const std::vector<std::vector<void*>>& families) {
    if (!HousingEnvOn()) return;
    uint32_t tidCitizen = TypeId("CitizenComponent");                  // 0x1401c7158
    fprintf(stderr, "[startTown::housing] families=%zu capturedHouses=%zu tidCitizen=%u\n",
            families.size(), g_foundingHouseIBPs.size(), tidCitizen);
    fflush(stderr);
    if (g_foundingHouseIBPs.empty() || tidCitizen == 0) {
        fprintf(stderr, "[startTown::housing] no captured residences "
                "(need BANISHED_LIVE_RESIDENCE) — skip\n"); fflush(stderr); return;
    }

    // Drive the FULL faithful AssignHousing in roster-only mode: it sets each citizen's
    // residence ref (+0x248) AND appends the citizen to the house's WorkPlaceComponent
    // occupant roster (+0x58/+0x60), but skips the gameplay-time Scene mesh-visual toggle
    // (which deadlocks when driven during this build-time pass — see g_wpcRosterOnly).
    g_wpcRosterOnly = true;
    for (size_t f = 0; f < families.size() && f < g_foundingHouseIBPs.size(); ++f) {
        int64_t houseIBP = g_foundingHouseIBPs[f];
        if (houseIBP == 0) continue;
        int housed = 0;
        for (void* ent : families[f]) {
            void* citComp = FindComponentOnEntity(ent, tidCitizen);
            if (!citComp) continue;
            // FULL faithful AssignHousing (0x140111ee0): detaches the citizen from any prior
            // residence then (a) sets the residence reference CitizenComponent+0x248 — the
            // field the per-tick Update reads as "housed" — AND (b) rosters the citizen into
            // the house's WorkPlaceComponent occupant list (+0x58/+0x60) via the attach helper
            // (FUN_14012fa20). The gates were the c80e0 (WorkPlaceComponent) + c8118
            // (WorkPlaceDescription) type-id globals, now filled in reg_economy_typeids.cpp.
            // So the house now TRACKS its occupants, not just the citizen pointing at it.
            int64_t houseIBPLocal = houseIBP;
            CitizenComp_AssignHousing(reinterpret_cast<CitizenComponent*>(citComp), &houseIBPLocal);
            int64_t assigned = *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(citComp) + 0x248);
            if (assigned != 0) ++housed;
        }
        // Read back the house's WorkPlaceComponent occupant roster count (+0x60) to
        // verify the FULL AssignHousing attach rostered the family (not just set +0x248).
        int roster = -1;
        uint32_t tidWP = TypeId("WorkPlaceComponent");                  // 0x1401c80e0
        void* wp = FindComponentOnEntity(reinterpret_cast<void*>(houseIBP), tidWP);
        if (wp) {
            roster = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(wp) + 0x60);
            // Queue this house's occupied visual for the deferred gameplay-time pass (the
            // Scene mesh-visual can't run now without deadlocking — see g_wpcRosterOnly).
            if (roster > 0) g_pendingOccupiedVisuals.push_back(reinterpret_cast<int64_t>(wp));
        }
        fprintf(stderr, "[startTown::housing] family[%zu] -> house#%zu (IBP=%llx): housed %d/%zu members "
                "(citizen+0x248); house WorkPlaceComponent roster(+0x60)=%d\n",
                f, f, (unsigned long long)houseIBP, housed, families[f].size(), roster);
        fflush(stderr);
    }
    g_wpcRosterOnly = false;   // restore: gameplay-time AssignHousing keeps the full visual
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

// CitizenComponent_impl_0dd0 — spawn the founding families.
static int StartTown_SpawnCitizens(uint8_t* startConfig, void* entityList) {
    const int famCount  = *(int32_t*)(startConfig + FAMILY_CNT);
    uint8_t*  families  = *(uint8_t**)(startConfig + FAMILY_ARR);
    if (famCount <= 0 || !families) return 0;
    if (!fn_EntityFactory_CreateEntity || !fn_EntityList_RegisterEntity ||
        !fn_InternalBasePtr_Ctor || !fn_MemAlloc) return 0;

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

    int spawned = 0;
    for (int f = 0; f < famCount; ++f) {
        uint8_t* fam = families + (size_t)f * FAMILY_STRIDE;
        const int educated    = *(int8_t*)fam;
        const int maxChildren = *(int32_t*)(fam + 4);
        famEntities.emplace_back();
        std::vector<void*>& fe = famEntities.back();

        // Two adults (male + female).
        if (void* a = SpawnOneCitizen(citizenTemplate, entityList)) { ++spawned; fe.push_back(a); }
        if (void* b = SpawnOneCitizen(citizenTemplate, entityList)) { ++spawned; fe.push_back(b); }

        // Children: Rand(0.._maxChildCount).
        int kids = 0;
        if (rng && fn_Random_Rand && maxChildren >= 0)
            kids = (int)fn_Random_Rand(rng, maxChildren + 1);
        for (int c = 0; c < kids; ++c)
            if (void* k = SpawnOneCitizen(citizenTemplate, entityList)) { ++spawned; fe.push_back(k); }

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
//   * Position jitter / random yaw use Random::RandFloat (wired) rather than
//     RandFloatTol (its dispatch slot 0x140151400 is unfilled in standalone →
//     would no-op to 0 anyway). RandFloat∈[0,1] → remap to [-1,1] for jitter.
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
            // Clear the cell entity-link (cell+0x08) — dee0 always does this.
            uintptr_t nullIBP = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
            *(uintptr_t*)(cell + 0x08) = nullIBP;
        }
    }
}

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
    }

    // terrain position for the cell centre (matches StartTown_TileToWorld).
    const float center = (float)(dim >> 1) - kCenterBias;
    float in[3]  = { (float)(cellIdx % dim) - center, (float)(cellIdx / dim) - center, 0.0f };
    float pos[3] = {0,0,0};
    fn_MapGetTerrainPos(game, pos, in, 0, nullptr);

    // RandFloat∈[0,1] → [-1,1] tolerance (= RandFloatTol) for x/y jitter AND yaw.
    auto rtol = [&]() -> float { return rng && fn_Random_RandFloat ? fn_Random_RandFloat(rng) * 2.0f - 1.0f : 0.0f; };
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
    void* gComp = FindComponentOnEntity(entity, tidGrowth);
    if (!gComp || !fn_InternalBasePtr_Ctor || !fn_InternalBasePtr_CheckPtr) return;
    uintptr_t gIBP = 0;
    fn_InternalBasePtr_Ctor(&gIBP, gComp);
    fn_InternalBasePtr_CheckPtr(&gIBP);
    uint8_t* s = (uint8_t*)gIBP;
    if (!s) return;
    // Sibling links populated only when GrowthComponent::Startup ran (LIVE build).
    if (!*(void**)(s + 0x28) || !*(void**)(s + 0x30) || !*(void**)(s + 0x38))
        return;   // inert → faithful no-op (matches the rest of the inert spawn path)
    const float progress = (rng && fn_Random_RandFloat) ? fn_Random_RandFloat(rng) : 0.0f;
    ActCNR_SetGrowthProgress_120db0((int64_t)gIBP, progress);
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

    int spawned = 0;
    for (int e = 0; e < count; ++e) {
        if (onlyPool >= 0 && e != onlyPool) continue;
        uint8_t* entry = pool + (size_t)e * RES_STRIDE;
        const char* ename = *(char**)entry ? *(char**)entry : "(null)";
        fprintf(stderr, "[startTown::res] begin [%d] name=\"%.48s\"\n", e, ename);
        fflush(stderr);
        void* tmpl = StartTown_ResolveTemplate(entry);
        if (!tmpl) continue;
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

        int s1 = fn_Random_RandTol(rng, 0x7ffffff);
        int s2 = fn_Random_RandTol(rng, 0x7ffffff);
        uint8_t perlin[0x100] = {};
        fn_PerlinNoise_Ctor(perlin, persist, octaves, s2, s1);

        // Per-pool growth-variant setup (FUN_1400a12b0's NaturalResourceDescription
        // +0x48 block). When set (trees/crops), build one Perlin field per growth
        // variant group so each cell can pick a biome-clustered variant; rocks/ore
        // have +0x48 clear and keep the default variant.
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
        if (variantCount > MAX_VARIANTS) variantCount = MAX_VARIANTS;
        uint8_t vPerlin[MAX_VARIANTS][0x100];
        for (int vi = 0; vi < variantCount; ++vi) {
            int vs1 = fn_Random_RandTol(rng, 0x7ffffff);
            int vs2 = fn_Random_RandTol(rng, 0x7ffffff);
            fn_PerlinNoise_Ctor(vPerlin[vi], vFreqX, vFreqY, vs2, vs1);
        }
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

    // Pass 2 — ambient environment volumes on a chunk grid.
    void* ambTmpl = StartTown_ResolveTemplate(startConfig + 0x138);
    const int chunk = *(int32_t*)(startConfig + 0x150);
    const int dim   = game ? *(int32_t*)game : 0;
    if (ambTmpl && chunk > 0 && dim > 0) {
        for (int y = 0; y < dim && n2 < ENV_PASS_CAP; y += chunk)
            for (int x = 0; x < dim && n2 < ENV_PASS_CAP; x += chunk)
                if (SpawnAndRegister(ambTmpl, entityList)) ++n2;
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

    // CITIZENS (impl_0dd0) — the founding families.
    int nc = StartTown_SpawnCitizens(sc, entityList);
    int afterCitizens = *(int32_t*)((uint8_t*)entityList + 0x10);
    fprintf(stderr, "[startTown::spawn] citizens: spawned %d; entityList count %d -> %d\n",
            nc, afterStruct, afterCitizens);
    fflush(stderr);

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
}
