// code/Game/Component/spawncomponent.cpp
// Clean reimplementation of SpawnComponent::Update from Application-x64.exe.
//
// Periodic entity spawning system. For each spawn entry in the resource data:
//   1. Check if current growth rate falls within the entry's [minGrowth, maxGrowth] range.
//   2. If in range, accumulate delta time into per-entry timer.
//   3. Look up spawn interval from linked template data.
//   4. When accumulated time exceeds interval, subtract interval and call
//      the spawn function. Repeat while timer still exceeds interval.
//
// Original: 0x140055ad0 (114 instructions)
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/spawncomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================
// Resolved helpers
// ============================================================

static constexpr float SECS_PER_SEASON = 300.0f;

// FUN_1400052c0 — gets linked object from entity owner
typedef void* (*fn_GetLinkedObject_t)(void* entity, void* outPtr);
static fn_GetLinkedObject_t s_fn_GetLinkedObject = nullptr;

// FUN_140055780 — performs actual spawn operation
typedef void (*fn_DoSpawn_t)(void* self, void* resourceData, int entryIndex, char flag);
static fn_DoSpawn_t s_fn_DoSpawn = nullptr;

// Type IDs
static uint32_t* s_pSpawnResourceTypeId = nullptr;   // DAT_1401c4918 = SpawnDescription
static uint32_t* s_pSpawnTemplateTypeId = nullptr;    // DAT_1401c77e8 = GrowthDescription

// FUN_140001bb0 — dynamic Time-array resize (stride 8). Sizes the per-entry timer
// array at self+0x28 in the OnCreate/Startup body-init. Already registered in
// standalone (address_registry.cpp -> Reimpl_Array_Resize_Stride08); we just resolve it.
typedef void (*fn_ArrayResize8_t)(void* arrayDesc, int newCount);
static fn_ArrayResize8_t s_fn_ArrayResize8 = nullptr;

static bool s_initialized = false;

// Step-2 body-init (vtable slot [4], +0x20, original SpawnComponent_Serialize/OnCreate
// 0x140055e90). Forward-declared so the Update guard can lazily invoke it if
// ComponentEntity_StartupComponents did not run it at spawn.
void SpawnComponent_Startup(SpawnComponent* self);

// ============================================================
// SpawnComponent::Update (0x140055ad0)
// ============================================================
void SpawnComponent_Update(SpawnComponent* self, int64_t* pDeltaTime) {
    SpawnComponent_InitFuncs();

    uint8_t* comp = reinterpret_cast<uint8_t*>(self);

    // [LIVE-COMPONENT BRING-UP GUARD] On an inert-spawned standalone component the
    // body is zeroed unless the OnCreate/Startup ran. The per-entry timer array at
    // +0x28 (data ptr) / +0x30 (count) is allocated by SpawnComponent_Startup (step
    // 2 of the recipe), which the spawn pipeline invokes via vtable+0x20 in
    // ComponentEntity_StartupComponents. As belt-and-suspenders, if +0x28 is still
    // null but the description(+0x18) is present, run the body-init now (idempotent).
    // The loop below indexes *(self+0x28) unconditionally, so bail if still unset.
    if (*(uintptr_t*)(comp + 0x28) == 0) {
        if (*(uintptr_t*)(comp + 0x18) != 0)
            SpawnComponent_Startup(self);
        if (*(uintptr_t*)(comp + 0x28) == 0)
            return;
    }

    // CheckPtr on owner IBP, then get linked object
    uintptr_t ownerIBP = (uintptr_t)self + 0x10;
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr((void*)ownerIBP);
    uintptr_t linkedObjPtr = 0;
    if (s_fn_GetLinkedObject)
        s_fn_GetLinkedObject(*(void**)ownerIBP, &linkedObjPtr);

    // Get resource data via IsDerivedFrom check on _unk18
    void* baseObj = self->_unk18;
    if (!fn_IsDerivedFrom || !s_pSpawnResourceTypeId || !s_pSpawnTemplateTypeId) return;
    bool isDerived = fn_IsDerivedFrom(baseObj, *s_pSpawnResourceTypeId);
    void* resourceData = isDerived ? baseObj : nullptr;
    // [STANDALONE TRUST] The factory tree can be incomplete in standalone, making
    // IsDerivedFrom on a description unreliable (see growthcomponent.cpp Startup).
    // _unk18 is the SpawnDescription the copy-ctor stored at +0x18 — trust it.
    if (!resourceData && baseObj) resourceData = baseObj;
    if (!resourceData) return;

    // CheckPtr on linked object IBP before reading growth
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(&linkedObjPtr);
    // [BRING-UP GUARD] The owner needs a sibling NaturalResourceComponent (found by
    // GetLinkedObject 0x1400052c0) for the growth read; bail rather than null-deref
    // if a spawner lacks one in standalone.
    if (linkedObjPtr == 0) return;
    float currentGrowth = *(float*)(linkedObjPtr + 0x28);

    // Phase-1 bring-up verification trace (env BANISHED_SPAWN_TRACE, default off →
    // byte-unchanged): follow ONE component, printing its first per-entry timer
    // accumulating and whether a spawn fired this tick. Confirms the live recipe
    // routes through +0x48 and the body-init populated the +0x28 timer array.
    static int s_traceState = -1;
    static int s_traceCount = 0;
    static void* s_lockComp = nullptr;
    if (s_traceState < 0) {
        const char* e = std::getenv("BANISHED_SPAWN_TRACE");
        s_traceState = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
    }

    // Get spawn entry count from resourceData+0x28
    int entryCount = *(int*)((uintptr_t)resourceData + 0x28);
    if (entryCount <= 0) return;

    uintptr_t entryArrayBase = *(uintptr_t*)((uintptr_t)resourceData + 0x20);

    for (int i = 0; i < entryCount; i++) {
        uintptr_t entry = entryArrayBase + (uintptr_t)i * 0x10;

        float minGrowth = *(float*)(entry + 0x08);
        float maxGrowth = *(float*)(entry + 0x0C);

        if (currentGrowth < minGrowth || currentGrowth > maxGrowth)
            continue;

        // Accumulate delta time
        int64_t byteOffset = (int64_t)i * 8;
        int64_t* pTimer = (int64_t*)(*(uintptr_t*)((uintptr_t)self + 0x28) + byteOffset);
        *pTimer += *pDeltaTime;

        if (s_traceState == 1 && s_lockComp == nullptr) s_lockComp = self;
        if (s_traceState == 1 && self == s_lockComp && i == 0 && s_traceCount < 20) {
            fprintf(stderr,
                "[SpawnTrace] self=%p res=%p entries=%d timer[0]=%lld growth=%.5f\n",
                (void*)self, resourceData, entryCount, (long long)*pTimer, currentGrowth);
            fflush(stderr);
            s_traceCount++;
        }

        // Get spawn template data from the entry
        uintptr_t templateData = *(uintptr_t*)entry;

        // Search template's component list for the spawn interval component
        int componentCount = *(int*)(templateData + 0x30);
        void* spawnIntervalComp = nullptr;

        for (int j = 0; j < componentCount; j++) {
            void* comp = *(void**)(*(uintptr_t*)(templateData + 0x28) + (uintptr_t)j * 8);
            typedef uint32_t (*fn_GetTypeId_t)(void*);
            uint32_t typeId = ((fn_GetTypeId_t)(*(void***)(comp))[2])(comp);

            if (typeId == *s_pSpawnTemplateTypeId) {
                if (fn_IsDerivedFrom(comp, *s_pSpawnTemplateTypeId)) {
                    spawnIntervalComp = comp;
                }
                break;
            }
        }

        if (!spawnIntervalComp) continue;

        // Convert spawn interval to ticks
        float intervalSecs = *(float*)((uintptr_t)spawnIntervalComp + 0x28) * SECS_PER_SEASON;
        int64_t intervalTicks = (int64_t)((double)intervalSecs / *g_Timer_secondsPerTick);

        // Re-read timer pointer (DoSpawn may realloc)
        // [CONSERVATIVE GATE] The full DoSpawn closure (0x140055780 +
        // CanPlace/TileFree/CopyCtor/Register/…) is registered, so this path WILL
        // instantiate real child entities into the world list. That spawn path is
        // unexercised by autostart, so default-LIVE keeps it OFF (timers accumulate,
        // observable + crash-safe); set BANISHED_SPAWN_DOSPAWN=1 to enable real
        // spawning for controlled verification.
        static int s_doSpawn = -1;
        if (s_doSpawn < 0) {
            const char* e = std::getenv("BANISHED_SPAWN_DOSPAWN");
            s_doSpawn = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
        }
        pTimer = (int64_t*)(*(uintptr_t*)((uintptr_t)self + 0x28) + byteOffset);
        while (intervalTicks > 0 && *pTimer >= intervalTicks) {
            *pTimer -= intervalTicks;
            if (s_doSpawn == 1 && s_fn_DoSpawn)
                s_fn_DoSpawn(self, resourceData, i, 0);
            if (s_traceState == 1 && self == s_lockComp && i == 0 && s_traceCount < 20) {
                fprintf(stderr, "[SpawnTrace]   entry %d FIRE (doSpawn=%d)\n", i, s_doSpawn);
                fflush(stderr);
            }
            pTimer = (int64_t*)(*(uintptr_t*)((uintptr_t)self + 0x28) + byteOffset);
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void SpawnComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_GetLinkedObject = reinterpret_cast<fn_GetLinkedObject_t>(
        ResolveAddr(Module::Exe, 0x1400052c0));
    s_fn_DoSpawn = reinterpret_cast<fn_DoSpawn_t>(
        ResolveAddr(Module::Exe, 0x140055780));
    s_pSpawnResourceTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4918));
    s_pSpawnTemplateTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c77e8));
    s_fn_ArrayResize8 = reinterpret_cast<fn_ArrayResize8_t>(
        ResolveAddr(Module::Exe, 0x140001bb0));

    s_initialized = true;
}

// ============================================================
// SpawnComponent::Startup (vtable slot [4], +0x20, original SpawnComponent_Serialize
// / OnCreate 0x140055e90)
// ============================================================
// Body-init: size the per-entry timer array at self+0x28 to the SpawnDescription's
// entry count (desc+0x28), via the registered stride-8 Time-array resizer
// (FUN_140001bb0). The array descriptor occupies self+0x28 (data ptr) / +0x30
// (count) / +0x34 (capacity) — the two pointers the Ctor zeroes. Called at spawn
// by ComponentEntity_StartupComponents (SpawnComponent flags 0x88 have bit 7).
//
// The original gates the count on IsDerivedFrom(_unk18, SpawnDescription); in
// standalone that can be unreliable, so we trust comp+0x18 (the description the
// copy-ctor stored), matching growthcomponent.cpp's documented precedent.
void SpawnComponent_Startup(SpawnComponent* self) {
    SpawnComponent_InitFuncs();
    uint8_t* comp = reinterpret_cast<uint8_t*>(self);

    void* desc = *reinterpret_cast<void**>(comp + 0x18);   // _unk18 = SpawnDescription
    if (!desc) return;                                     // not yet wired to a description

    int entryCount = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(desc) + 0x28);
    if (entryCount <= 0) return;

    // Resize the timer array (descriptor at comp+0x28). Zero-initialised entries.
    if (s_fn_ArrayResize8)
        s_fn_ArrayResize8(reinterpret_cast<void*>(comp + 0x28), entryCount);
}

// ============================================================
// Reimpl of FUN_140055780 (ComponentEntity_impl_5780: probabilistically spawn one
//   child entity (animal/resource) near the owner from a spawn-table entry)
// ============================================================
// Faithful port of the Ghidra decompilation. resourceData+0x20 holds the spawn
// table (0x10 stride); entryIndex selects the template. A random roll gated by the
// template's density (g_fUVScale / templ+0x48) decides whether to spawn; the tile is
// chosen by RandTol jitter around the owner tile and validated, then a copy of the
// template entity is instantiated, placed, oriented, and registered.
typedef float (*fn_SC_RandFloat_t)(void* random);                 // slot 0x1401512f8
typedef int   (*fn_SC_RandTol_t)(void* random, int tol);          // slot 0x1401512f0
typedef void* (*fn_SC_FindMap_t)(uintptr_t entity, void* retbuf); // 0x140004680
typedef char  (*fn_SC_CanPlace_t)(int* grid, int x, int y, int kind,
                                  float a, float b, float c,
                                  char d, char e, int f);          // 0x14009f5d0
typedef char  (*fn_SC_TileFree_t)(void* templ, int x, int y);     // 0x140120c30
typedef void* (*fn_SC_CopyCtor_t)(void* dst, int64_t srcEntity);  // 0x1400c3720
typedef void* (*fn_SC_FindLinkA_t)(int64_t ibp, void* retbuf);    // 0x1400052c0
typedef void* (*fn_SC_FindLinkB_t)(int64_t ibp, void* retbuf);    // 0x14000c460
typedef void  (*fn_SC_SetTile_t)(int64_t entity, int tileIndex);  // 0x140126540
typedef void  (*fn_SC_SetOrient_t)(int64_t entity, float v);      // 0x140120db0
typedef void  (*fn_SC_Register_t)(void* list, void* ibp, int idx); // 0x1400c48a0
static fn_SC_RandFloat_t s_sc_RandFloat = nullptr;
static fn_SC_RandTol_t   s_sc_RandTol = nullptr;
static fn_SC_FindMap_t   s_sc_FindMap = nullptr;
static fn_SC_CanPlace_t  s_sc_CanPlace = nullptr;
static fn_SC_TileFree_t  s_sc_TileFree = nullptr;
static fn_SC_CopyCtor_t  s_sc_CopyCtor = nullptr;
static fn_SC_FindLinkA_t s_sc_FindLinkA = nullptr;
static fn_SC_FindLinkB_t s_sc_FindLinkB = nullptr;
static fn_SC_SetTile_t   s_sc_SetTile = nullptr;
static fn_SC_SetOrient_t s_sc_SetOrient = nullptr;
static fn_SC_Register_t  s_sc_Register = nullptr;
static float* s_sc_pUVScale = nullptr;   // 0x14017214c
static float* s_sc_pConstA = nullptr;    // 0x140172140
static float* s_sc_pConstB = nullptr;    // 0x140172248
static bool s_scSpawnResolved = false;
static void EnsureSpawnHelpers() {
    if (s_scSpawnResolved) return;
    // RandFloat (0x1401512f8) / RandTol (0x1401512f0) are .rdata dispatch slots that
    // the original CALLs indirectly. Those slots are NOT registered in standalone, so
    // prefer the slot if present, else fall back to the backing Runtime exports via
    // engine_funcs (System::Random::RandFloat / RandTol — the same path
    // growthcomponent.cpp's SpawnSubResource uses). Without this the density roll would
    // default to 1.0 and DoSpawn would never fire even when BANISHED_SPAWN_DOSPAWN=1.
    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x1401512f8);
    if (slot) s_sc_RandFloat = *reinterpret_cast<fn_SC_RandFloat_t*>(slot);
    if (!s_sc_RandFloat) s_sc_RandFloat = reinterpret_cast<fn_SC_RandFloat_t>(fn_Random_RandFloat);
    slot = ResolveAddr(Module::Exe, 0x1401512f0);
    if (slot) s_sc_RandTol = *reinterpret_cast<fn_SC_RandTol_t*>(slot);
    if (!s_sc_RandTol) s_sc_RandTol = reinterpret_cast<fn_SC_RandTol_t>(fn_Random_RandTol);
    s_sc_FindMap   = reinterpret_cast<fn_SC_FindMap_t>(ResolveAddr(Module::Exe, 0x140004680));
    s_sc_CanPlace  = reinterpret_cast<fn_SC_CanPlace_t>(ResolveAddr(Module::Exe, 0x14009f5d0));
    s_sc_TileFree  = reinterpret_cast<fn_SC_TileFree_t>(ResolveAddr(Module::Exe, 0x140120c30));
    s_sc_CopyCtor  = reinterpret_cast<fn_SC_CopyCtor_t>(ResolveAddr(Module::Exe, 0x1400c3720));
    s_sc_FindLinkA = reinterpret_cast<fn_SC_FindLinkA_t>(ResolveAddr(Module::Exe, 0x1400052c0));
    s_sc_FindLinkB = reinterpret_cast<fn_SC_FindLinkB_t>(ResolveAddr(Module::Exe, 0x14000c460));
    s_sc_SetTile   = reinterpret_cast<fn_SC_SetTile_t>(ResolveAddr(Module::Exe, 0x140126540));
    s_sc_SetOrient = reinterpret_cast<fn_SC_SetOrient_t>(ResolveAddr(Module::Exe, 0x140120db0));
    s_sc_Register  = reinterpret_cast<fn_SC_Register_t>(ResolveAddr(Module::Exe, 0x1400c48a0));
    s_sc_pUVScale  = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x14017214c));
    s_sc_pConstA   = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172140));
    s_sc_pConstB   = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172248));
    s_scSpawnResolved = true;
}

void SpawnComponent_DoSpawn(void* selfP, void* resourceData, int entryIndex, char flag) {
    EnsureSpawnHelpers();
    uintptr_t self = (uintptr_t)selfP;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t gw = (uintptr_t)*g_pGameWorldSingleton;

    int* grid = *(int**)(gw + 0xf8);
    void* random = *(void**)(gw + 0xd8);
    if ((*(uint32_t*)(gw + 0xd0) >> 10) & 1) return;

    int64_t* entry = (int64_t*)((int64_t)entryIndex * 0x10 + *(int64_t*)((uintptr_t)resourceData + 0x20));
    int64_t host = *entry;

    // Find the template component (type DAT_1401c77e8) on the entry entity
    void* templ = nullptr;
    if (s_pSpawnTemplateTypeId) {
        int count = *(int*)(host + 0x30);
        int64_t arr = *(int64_t*)(host + 0x28);
        for (int i = 0; i < count; i++) {
            void* comp = *(void**)(arr + (int64_t)i * 8);
            typedef uint32_t (*GetTypeId_t)(void*);
            uint32_t tid = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
            if (tid == *s_pSpawnTemplateTypeId) {
                if (fn_IsDerivedFrom && fn_IsDerivedFrom(comp, *s_pSpawnTemplateTypeId)) templ = comp;
                break;
            }
        }
    }
    if (!templ) return;

    // Density roll
    float roll = s_sc_RandFloat ? s_sc_RandFloat(random) : 1.0f;
    float density = s_sc_pUVScale ? (*s_sc_pUVScale / (float)*(int*)((uintptr_t)templ + 0x48)) : 0.0f;
    if (roll > density) return;

    // Owner tile
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(self + 0x10));
    uint8_t mapBuf[8] = {0};
    if (s_sc_FindMap) s_sc_FindMap(*(uintptr_t*)(self + 0x10), mapBuf);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(mapBuf);
    int64_t mapComp = *(int64_t*)mapBuf;
    int16_t baseX = *(int16_t*)(mapComp + 0x28);
    int16_t baseY = *(int16_t*)(mapComp + 0x2a);

    int tol = *(int*)((uintptr_t)templ + 0x4c);
    int x = baseX + (s_sc_RandTol ? s_sc_RandTol(random, tol) : 0);
    int y = baseY + (s_sc_RandTol ? s_sc_RandTol(random, tol) : 0);
    if (x < 0 || y < 0) return;
    int mapSize = *grid;
    if (x >= mapSize || y >= mapSize) return;

    float ca = s_sc_pConstA ? *s_sc_pConstA : 0.0f;
    float cb = s_sc_pConstB ? *s_sc_pConstB : 0.0f;
    bool canPlace = s_sc_CanPlace &&
        s_sc_CanPlace(grid, x, y, 4, ca, cb, 0.0f, '\0', '\0', 0) != '\0';
    if (!canPlace) return;
    if (!(s_sc_TileFree && s_sc_TileFree(templ, x, y) != '\0')) return;

    int placeMapSize = *grid;
    void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
    void* newEntity = nullptr;
    if (mem && s_sc_CopyCtor) newEntity = s_sc_CopyCtor(mem, *entry);

    int64_t entityIbp = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&entityIbp, newEntity);

    // Place at tile index
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entityIbp);
    int64_t linkBufA = 0;
    void* pa = s_sc_FindLinkA ? s_sc_FindLinkA(entityIbp, &linkBufA) : nullptr;
    if (pa) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(pa);
        if (s_sc_SetTile) s_sc_SetTile(*(int64_t*)pa, placeMapSize * y + x);
    }

    // Reset orientation
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entityIbp);
    int64_t linkBufB = 0;
    void* pb = s_sc_FindLinkB ? s_sc_FindLinkB(entityIbp, &linkBufB) : nullptr;
    if (pb) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(pb);
        if (s_sc_SetOrient) s_sc_SetOrient(*(int64_t*)pb, 0.0f);
    }

    // Register with world entity list
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(self + 0x10));
    int64_t regIbp = entityIbp;
    if (s_sc_Register)
        s_sc_Register(*(void**)(gw + 0xe8), &regIbp,
                      *(int*)(*(int64_t*)(*(int64_t*)(self + 0x10) + 0x28) + 0x20));

    // Optional random orientation
    if (flag != '\0') {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entityIbp);
        int64_t b2 = 0;
        void* p2 = s_sc_FindLinkB ? s_sc_FindLinkB(entityIbp, &b2) : nullptr;
        if (p2 && *(int64_t*)p2 != 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entityIbp);
            int64_t b3 = 0;
            void* p3 = s_sc_FindLinkB ? s_sc_FindLinkB(entityIbp, &b3) : nullptr;
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p3);
            float rf = s_sc_RandFloat ? s_sc_RandFloat(random) : 0.0f;
            if (p3 && s_sc_SetOrient) s_sc_SetOrient(*(int64_t*)p3, rf);
        }
    }
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// size 0x38 (SpawnComponent_Factory MemAlloc), flags 0x88 (Ctor 0x140056030:
// Component base sets +0x20=0x80, leaf OR 0x88 => 0x88).
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// Live bring-up (Phase 1, env BANISHED_LIVE_SPAWN or global BANISHED_LIVE_COMPONENTS).
// All four recipe steps wired:
//   * slot 4 (+0x20 Startup) -> SpawnComponent_Startup — sizes the +0x28 per-entry
//     timer array from the SpawnDescription at spawn (ComponentEntity_StartupComponents).
//   * slot 9 (+0x48 Update)  -> SpawnComponent_Update — the per-tick growth-gated
//     timer accumulation (the slot the EntityList bucket round-robin calls).
// The two type-id globals it reads (DAT_1401c4918 SpawnDescription, DAT_1401c77e8
// GrowthDescription) are registered + filled by reg_component_spawn.cpp. The full
// DoSpawn closure is already registered, so real spawning is available behind
// BANISHED_SPAWN_DOSPAWN=1 (default OFF — see the CONSERVATIVE GATE in Update).
COMPONENT_LIVE_FACTORY_S(SpawnComponent, 0x38, 0x88,
                         "BANISHED_LIVE_SPAWN", SpawnComponent_Update, SpawnComponent_Startup)
REGISTER_TYPE(SpawnComponent, "Component",
              &SpawnComponent_Factory, &SpawnComponent_CreateInPlace);
