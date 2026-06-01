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
#include <cstdint>
#include <cstdio>
#include <cmath>

static constexpr int   CELL_STRIDE = 0x30;
static constexpr int   CELL_TYPE   = 0x29;     // tile type byte
static constexpr int   GAME_CELLS  = 0x80;     // Game+0x80 = {void* data, int count, int cap}
static constexpr int   TILE_BUILDABLE = 2;
static constexpr float kCenterBias = 0.5f;     // DAT_140172114
static constexpr float kDeg2Rad    = 3.14159265358979323846f / 180.0f;

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

// Spawn entry point — called from Load after the camera is framed. Builds up the
// start-town entities. Currently: the simple pool (impl_09e0) only.
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

    // EntityList main-array count lives at entityList+0x10 (Foundation B); log it
    // before/after to confirm registration actually adds entities to the world.
    int before = *(int32_t*)((uint8_t*)entityList + 0x10);
    int n = StartTown_SpawnSimplePool(sc, entityList);
    int after = *(int32_t*)((uint8_t*)entityList + 0x10);
    fprintf(stderr, "[startTown::spawn] simple pool: spawned %d entities; "
            "entityList count %d -> %d\n", n, before, after);
    fflush(stderr);
}
