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

// ComponentEntity_impl_1fc0 — spawn the initial buildings around (spawnX,spawnY).
static int StartTown_SpawnStructures(uint8_t* startConfig, void* entityList,
                                     int spawnX, int spawnY) {
    const int count = *(int32_t*)(startConfig + 0x88);
    uint8_t* pool   = *(uint8_t**)(startConfig + 0x80);
    if (count <= 0 || !pool) return 0;
    if (!fn_EntityFactory_CreateEntity || !fn_EntityList_RegisterEntity ||
        !fn_InternalBasePtr_Ctor || !fn_InternalBasePtr_CheckPtr || !fn_MemAlloc)
        return 0;

    const uint32_t tidCreatePlaced = TypeId("CreatePlacedDescription");
    const uint32_t tidMapComponent = TypeId("MapComponent");

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

        // Render transform + random mesh variant (ModelComponent) deferred: those
        // dispatch on the component's Geometry sub-object, zeroed on inert
        // components. See header comment.

        // Register with the world entity list (category = template+0x20).
        int category = *(int32_t*)((uint8_t*)tmpl + TEMPLATE_CATEGORY);
        fn_EntityList_RegisterEntity(entityList, &entIBP, category);

        fprintf(stderr, "[startTown::struct] [%d] tmpl=%p tile=(%d,%d) fp=%dx%d rot=%d map=%p\n",
                i, tmpl, tileX, tileY, fpW, fpH, rot, mapComp);
        fflush(stderr);
        if (ent) ++spawned;
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
    int category = *(int32_t*)((uint8_t*)citizenTemplate + TEMPLATE_CATEGORY);
    fn_EntityList_RegisterEntity(entityList, &entIBP, category);
    return ent;
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

    int spawned = 0;
    for (int f = 0; f < famCount; ++f) {
        uint8_t* fam = families + (size_t)f * FAMILY_STRIDE;
        const int educated    = *(int8_t*)fam;
        const int maxChildren = *(int32_t*)(fam + 4);

        // Two adults (male + female).
        if (SpawnOneCitizen(citizenTemplate, entityList)) ++spawned;
        if (SpawnOneCitizen(citizenTemplate, entityList)) ++spawned;

        // Children: Rand(0.._maxChildCount).
        int kids = 0;
        if (rng && fn_Random_Rand && maxChildren >= 0)
            kids = (int)fn_Random_Rand(rng, maxChildren + 1);
        for (int c = 0; c < kids; ++c)
            if (SpawnOneCitizen(citizenTemplate, entityList)) ++spawned;

        fprintf(stderr, "[startTown::citizen] family[%d] educated=%d maxKids=%d kids=%d\n",
                f, educated, maxChildren, kids);
        fflush(stderr);
    }
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
// Safety cap: map-wide scatter over dim^2 cells x N pools can produce tens of
// thousands of entities; until the inert-instantiation path is proven fast we
// bound each pool and log if the cap is hit (no silent truncation). Per-pool
// (not total) so every resource type gets exercised.
static constexpr int RES_POOL_CAP = 400;

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

    int spawned = 0;
    for (int e = 0; e < count; ++e) {
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
                if (dy * dy + dx * dx < sizeSq) continue;                                     // town exclusion

                // Terrain height + slope from the heightmap sampler.
                const float center = (float)(dim >> 1) - kCenterBias;
                float in[3]  = { (float)(idx % dim) - center, (float)(idx / dim) - center, 0.0f };
                float pos[3] = {0,0,0}, tri[3] = {0,0,0};
                fn_MapGetTerrainPos(game, pos, in, 0, tri);
                const float h = pos[2], slope = tri[2];
                if (h < hMin || h >= hMax || slope < slopeMin) continue;

                // Instantiate + register (component grid/growth/model deferred — inert).
                void* mem = fn_MemAlloc(g_pMemAllocSingleton, ENTITY_SIZE, 8);
                void* ent = mem ? fn_EntityFactory_CreateEntity(mem, tmpl) : nullptr;
                uintptr_t entIBP = 0;
                fn_InternalBasePtr_Ctor(&entIBP, ent);
                fn_EntityList_RegisterEntity(entityList, &entIBP, category);
                if (ent) { ++spawned; ++poolSpawned; }
                if (poolSpawned >= RES_POOL_CAP) { capped = true; break; }
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
    int ns = StartTown_SpawnStructures(sc, entityList, spawnX, spawnY);
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
