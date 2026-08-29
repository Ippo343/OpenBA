// code/Game/Action/huntanimal.cpp
// Clean C++ reimplementation of HuntAnimal action methods.
// Source: Game/Action/HuntAnimal.cpp (from profile build debug strings)
//
// HuntAnimal -- 0x40 bytes. Handles hunting behavior: finding nearby animal,
// spawning a corpse entity, starting herd migration.
// Inherits: Object::Internal -> Action (0x20) -> HuntAnimal
// Factory: 0x140022700, Vtable: 0x14015fde0
//
// Vtable (10 slots):
//   [0] +0x00 ScalarDtor    = 0x140022770
//   [1] +0x08 VectorDtor    = 0x1400226f0 (returns type ptr, unhookable)
//   [2] +0x10 BaseDtor      = 0x1400226e0 (returns TypeInfo, unhookable)
//   [3] +0x18 Serialize     = 0x1400c8260
//   [4] +0x20 AbstractImpl  = 0x1400c8250 (9 bytes, unhookable)
//   [5] +0x28 Execute       = 0x1400c8240 (9 bytes, unhookable)
//   [6] +0x30 Enter         = 0x1400c7de0 (~250 instructions, grid math + entity queries)
//   [7] +0x38 Exit          = 0x1400c4ba0 (base Action, void nop)
//   [8] +0x40 CheckEntity   = 0x1400c4a30 (base Action)
//   [9] +0x48 GetTarget     = 0x1400c7d60
//
// Field layout (0x40 bytes):
//   +0x00-0x1F: Action base (vtable, Internal base, entity ptr, status byte)
//   +0x20: void*    animalEntity   (InternalBasePtr -- the animal being hunted)
//   +0x28: void*    herdEntity     (InternalBasePtr -- the herd the animal belongs to)
//   +0x30: void*    corpseEntity   (InternalBasePtr -- unused/zeroed field)
//   +0x38: uint8_t  huntStarted    (flag: 0=not started, 1=hunt completed)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "HuntAnimal.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>

// ============================================================
// Local function pointers resolved at init time
// ============================================================

// Object::Internal::~Internal(this) -- via IAT at 0x140152770
static void (*s_fn_ObjectInternal_Dtor)(void* self) = nullptr;

// Object::InternalBasePtr::Serialize(this, serializer) -- via IAT at 0x140152aa0
static void (*s_fn_IBP_Serialize)(void* basePtr, void* serializer) = nullptr;

// HuntAnimal_helper_2a80 (0x140122a80) -- checks if animal has been held long enough
// Returns true if entity's time at +0x50 >= description's holdTime * 300.0
static bool (*s_fn_IsAnimalReady)(void* animalEntity) = nullptr;

// FUN_140092260 -- spatial entity query (finds nearby entity matching criteria)
// Params: gridPtr, outResult, position, radius, typeId, outIBP
typedef void* (*s_fn_SpatialQuery_t)(void* gridPtr, void* outResult, float* position,
                                      float* radius, int typeId, void* outIBP);
static s_fn_SpatialQuery_t s_fn_SpatialQuery = nullptr;

// FUN_140005500 -- get model component from entity
static void (*s_fn_GetModelComp)(void* entity, void* outIBP) = nullptr;

// FUN_14009ccf0 -- find path cost between two grid cells
// Params: mapPtr, fromCell, toCell, flags(0xe1f)
typedef int (*s_fn_FindPathCost_t)(int* mapPtr, int fromCell, int toCell, uint32_t flags);
static s_fn_FindPathCost_t s_fn_FindPathCost = nullptr;

// FUN_1400a00f0 -- check path validity
// Params: mapPtr, flags(0xe1f), cell, destCell
typedef uint64_t (*s_fn_CheckPathValid_t)(int* mapPtr, uint64_t flags, uint64_t cell, int destCell);
static s_fn_CheckPathValid_t s_fn_CheckPathValid = nullptr;

// CharacterComponent_impl_f3e0 -- convert grid pos to world pos
// Params: mapPtr, outWorldPos, inGridPos, flag, extra
typedef float* (*s_fn_GridToWorld_t)(int* mapPtr, float* outWorldPos, float* inGridPos,
                                     char flag, void* extra);
static s_fn_GridToWorld_t s_fn_GridToWorld = nullptr;

// RemoveAnimal_Action_Ctor (0x1400bbea0) -- creates the corpse/removal action
// Params: herdIBP, animIndex(-1), worldPos, fromCell, corpseIBP, descPtr, flags
typedef void (*s_fn_SpawnCorpse_t)(void* herdIBP, int animIndex, float* worldPos,
                                   int fromCell, void* corpseIBP, void* descPtr, uint32_t flags);
static s_fn_SpawnCorpse_t s_fn_SpawnCorpse = nullptr;

// HuntAnimal_helper_27e0 (0x1400227e0) -- finds HerdComponent from entity's component list
// Params: entity, outIBP
typedef void* (*s_fn_FindHerdComp_t)(void* entity, void* outIBP);
static s_fn_FindHerdComp_t s_fn_FindHerdComp = nullptr;

// HerdComponent_StartMigration (0x140122340) -- triggers herd migration after kill
static void (*s_fn_StartMigration)(void* herdComp) = nullptr;

// Type IDs
static uint32_t* s_typeId_AnimalEntity = nullptr;     // DAT_1401c7890 -- for animal entity check
static uint32_t* s_typeId_HerdEntity = nullptr;        // DWORD_1401c7158 -- for herd entity check
static uint32_t* s_typeId_ModelDesc = nullptr;          // DWORD_1401c6fd0 -- ModelDescription type
static int* s_typeId_HuntAnimalSearch = nullptr;       // DAT_1401c8000 -- search type for spatial query

// TypeInfo for slots 1/2
static uint64_t* s_dat_VectorDtorReturn = nullptr;    // DAT_1401c5f70
static uint32_t* s_typeInfo_HuntAnimal = nullptr;      // DAT_1401c5f68

// Vtable addresses for dtor chain
static uintptr_t s_vtable_HuntAnimal = 0;
static uintptr_t s_vtable_Action = 0;

// Float constants
static float* s_searchRadius = nullptr;     // DAT_1401721a8 = 8.0f
static float* s_halfOffset = nullptr;       // DAT_140172114 = 0.5f

static bool s_funcs_resolved = false;

// ============================================================
// Resolution -- called once from Install
// ============================================================
bool HuntAnimal_ResolveFuncs() {
    if (s_funcs_resolved) return true;

    // Object::Internal dtor via IAT
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152770);
    if (iatDtor) s_fn_ObjectInternal_Dtor = *reinterpret_cast<void(**)(void*)>(iatDtor);

    // InternalBasePtr::Serialize via IAT at 0x140152aa0
    uintptr_t iatIBPSer = ResolveAddr(Module::Exe, 0x140152aa0);
    if (iatIBPSer) s_fn_IBP_Serialize = *reinterpret_cast<void(**)(void*, void*)>(iatIBPSer);

    // Helpers
    s_fn_IsAnimalReady = (bool(*)(void*))ResolveAddr(Module::Exe, 0x140122a80);
    s_fn_SpatialQuery = (s_fn_SpatialQuery_t)ResolveAddr(Module::Exe, 0x140092260);
    s_fn_GetModelComp = (void(*)(void*, void*))ResolveAddr(Module::Exe, 0x140005500);
    s_fn_FindPathCost = (s_fn_FindPathCost_t)ResolveAddr(Module::Exe, 0x14009ccf0);
    s_fn_CheckPathValid = (s_fn_CheckPathValid_t)ResolveAddr(Module::Exe, 0x1400a00f0);
    s_fn_GridToWorld = (s_fn_GridToWorld_t)ResolveAddr(Module::Exe, 0x14009f3e0);
    s_fn_SpawnCorpse = (s_fn_SpawnCorpse_t)ResolveAddr(Module::Exe, 0x1400bbea0);
    s_fn_FindHerdComp = (s_fn_FindHerdComp_t)ResolveAddr(Module::Exe, 0x1400227e0);
    s_fn_StartMigration = (void(*)(void*))ResolveAddr(Module::Exe, 0x140122340);

    // Type IDs
    s_typeId_AnimalEntity = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7890));
    s_typeId_HerdEntity = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7158)); // 0x1401c7158 = CitizenComponent id (retail validates the herd ref against CitizenComponent)
    s_typeId_ModelDesc = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c6fd0));
    s_typeId_HuntAnimalSearch = reinterpret_cast<int*>(ResolveAddr(Module::Exe, 0x1401c8000));

    // TypeInfo for slots 1/2
    s_dat_VectorDtorReturn = reinterpret_cast<uint64_t*>(ResolveAddr(Module::Exe, 0x1401c5f70));
    s_typeInfo_HuntAnimal = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c5f68));

    // Vtable addresses
    s_vtable_HuntAnimal = ResolveAddr(Module::Exe, 0x14015fde0);
    s_vtable_Action = ResolveAddr(Module::Exe, 0x14015faa0);

    // Float constants
    s_searchRadius = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x1401721a8));
    s_halfOffset = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));

    bool ok = s_fn_ObjectInternal_Dtor && s_fn_IBP_Serialize &&
              s_fn_IsAnimalReady && s_fn_SpatialQuery &&
              s_fn_GetModelComp && s_fn_FindPathCost &&
              s_fn_CheckPathValid && s_fn_GridToWorld &&
              s_fn_SpawnCorpse && s_fn_FindHerdComp && s_fn_StartMigration &&
              s_typeId_AnimalEntity && s_typeId_HerdEntity &&
              s_typeId_ModelDesc && s_typeId_HuntAnimalSearch &&
              s_dat_VectorDtorReturn && s_typeInfo_HuntAnimal &&
              s_vtable_HuntAnimal && s_vtable_Action &&
              s_searchRadius && s_halfOffset &&
              fn_InternalBasePtr_CheckPtr && fn_InternalBasePtr_DeletePtr &&
              fn_InternalBasePtr_Ctor && fn_IsDerivedFrom &&
              fn_MemFreeStatic && fn_Vector3_Ctor &&
              g_fn_Serializer_Ctor && g_fn_Serializer_Dtor &&
              g_fn_Serialize_Bool && g_pGameWorldSingleton;

#ifdef _DEBUG
    printf("[HuntAnimal] ResolveFuncs: ObjDtor=%p IBPSer=%p IsReady=%p SpatialQ=%p\n",
           (void*)s_fn_ObjectInternal_Dtor, (void*)s_fn_IBP_Serialize,
           (void*)s_fn_IsAnimalReady, (void*)s_fn_SpatialQuery);
    printf("[HuntAnimal]   GetModel=%p PathCost=%p PathValid=%p GridToWorld=%p\n",
           (void*)s_fn_GetModelComp, (void*)s_fn_FindPathCost,
           (void*)s_fn_CheckPathValid, (void*)s_fn_GridToWorld);
    printf("[HuntAnimal]   SpawnCorpse=%p FindHerd=%p StartMig=%p\n",
           (void*)s_fn_SpawnCorpse, (void*)s_fn_FindHerdComp, (void*)s_fn_StartMigration);
    if (!ok) printf("[HuntAnimal] ERROR: some function pointers not resolved!\n");
#endif

    s_funcs_resolved = ok;
    return ok;
}

// ============================================================
// Slot 0: ScalarDeletingDtor (0x140022770)
// Same pattern as RemoveAnimal: set vtables, cleanup +0x10, ~Internal, free.
// ============================================================
void* HuntAnimal_ScalarDtor_Impl(HuntAnimal* self, uint32_t flags) {
    uint8_t* s = (uint8_t*)self;

    // Set HuntAnimal vtable then Action vtable for proper dtor dispatch
    *(uintptr_t*)s = s_vtable_HuntAnimal;
    *(uintptr_t*)s = s_vtable_Action;

    // If entity ref at +0x10 is non-null, delete it
    if (*(void**)(s + 0x10) != nullptr) {
        fn_InternalBasePtr_DeletePtr(s + 0x10);
    }

    // Call Object::Internal::~Internal
    s_fn_ObjectInternal_Dtor(self);

    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// Slot 1: VectorDeletingDtor (0x1400226f0) -- unhookable (7 bytes, RIP-relative)
// Returns DAT_1401c5f70 (type factory pointer).
// ============================================================
uint64_t HuntAnimal_VectorDtor_Impl() {
    return *s_dat_VectorDtorReturn;
}

// ============================================================
// Slot 2: BaseDtor (0x1400226e0) -- unhookable (7 bytes, RIP-relative)
// Returns TypeInfo for HuntAnimal (DAT_1401c5f68).
// ============================================================
uint32_t HuntAnimal_BaseDtor_Impl() {
    return *s_typeInfo_HuntAnimal;
}

// ============================================================
// Slot 3: Serialize (0x1400c8260)
// Serializes: status(+0x18), animal entity(+0x20) with type check,
//             herd entity(+0x28) with type check, huntStarted flag(+0x38).
// ============================================================
void HuntAnimal_Serialize_Impl(HuntAnimal* self, void* stream) {
    // Reached through the registered vtable, which the factory path never
    // has to touch — an exe-ctor-built action arrives here with the file
    // statics still null. Lazy-resolve like the other entry points do.
    HuntAnimal_ResolveFuncs();
    uint8_t* s = (uint8_t*)self;
    uint8_t ser1[16];
    uint8_t ser2[24];

    // Serialize status byte (+0x18)
    g_fn_Serializer_Ctor(ser1, stream, 0);
    g_fn_Serialize_Bool(ser1, s + 0x18, nullptr);
    g_fn_Serializer_Dtor(ser1);

    // Serialize entity refs and hunt flag
    g_fn_Serializer_Ctor(ser2, stream, 0);

    // Serialize animal entity ref (+0x20) with AnimalEntity type check
    s_fn_IBP_Serialize(s + 0x20, ser2);
    if (*(void**)(s + 0x20) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x20), *s_typeId_AnimalEntity)) {
            *(void**)(s + 0x20) = nullptr;
        }
    }

    // Serialize herd entity ref (+0x28) with HerdEntity type check
    s_fn_IBP_Serialize(s + 0x28, ser2);
    if (*(void**)(s + 0x28) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x28), *s_typeId_HerdEntity)) {
            *(void**)(s + 0x28) = nullptr;
        }
    }

    // Serialize hunt started flag (+0x38)
    g_fn_Serialize_Bool(ser2, s + 0x38, nullptr);

    g_fn_Serializer_Dtor(ser2);
}

// ============================================================
// Slot 4: AbstractImpl (0x1400c8250) -- unhookable (9 bytes)
// Returns 1 if hunt not started (keep running), 0 if hunt completed.
// ============================================================
int HuntAnimal_AbstractImpl_Impl(HuntAnimal* self, void* actionStack) {
    // Does the kill step run at all? Hunting fires NEITHER deposit meter, which means
    // the produce step is not happening rather than food failing to be delivered. This
    // action is the kill; counting it splits "the hunter never reaches an animal" from
    // "it kills and the yield goes nowhere".
    {
        static int s_t = -1;
        if (s_t < 0) { const char* e = std::getenv("BANISHED_CROPFIELD_TRACE");
                       s_t = (e && e[0] && e[0] != '0') ? 1 : 0; }
        static long long s_n = 0;
        if (s_t && ((s_n++ % 5) == 0)) {
            std::fprintf(stderr, "[HuntKill] HuntAnimal AbstractImpl run %lld\n", s_n);
            std::fflush(stderr);
        }
    }
    HuntAnimal_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    (void)actionStack;
    uint8_t* s = (uint8_t*)self;
    return (*(uint8_t*)(s + 0x38) == 0) ? 1 : 0;
}

// ============================================================
// Slot 5: Execute (0x1400c8240) -- unhookable (9 bytes)
// Same logic as AbstractImpl: returns 1 if hunt not started, 0 if completed.
// ============================================================
int HuntAnimal_Execute_Impl(HuntAnimal* self, void* actionStack, void* time) {
    (void)actionStack;
    (void)time;
    uint8_t* s = (uint8_t*)self;
    return (*(uint8_t*)(s + 0x38) == 0) ? 1 : 0;
}

// ============================================================
// Slot 6: Enter (0x1400c7de0) -- complex hunting logic
//
// Algorithm:
// 1. If huntStarted is already set, return early
// 2. Check if the animal at +0x20 is ready (held long enough)
// 3. Get herd's CharacterComponent position from +0x28 -> +0x278 -> +0x68 -> +0x158
// 4. Do a spatial query to find a nearby entity of the hunt type
// 5. Verify the found entity's model component doesn't have bit 20 set (not in use)
// 6. Get the found entity's model component
// 7. Calculate grid cell positions for path finding
// 8. Check path cost and validity between animal and found entity
// 9. If path is valid:
//    a. Get model description from found entity's model component
//    b. Convert grid position to world position
//    c. Spawn a corpse action (RemoveAnimal_Action_Ctor)
//    d. Find HerdComponent from the found entity
//    e. Start herd migration
//    f. Clear animal entity's time at +0x50
//    g. Set huntStarted flag at +0x38
// ============================================================
// HuntAnimal::Enter exit census. RemoveAnimal is never called and the hunt never
// completes, so Enter must bail before it sets huntStarted (+0x38). Its five exits need
// five different fixes, and "Enter ran" says nothing about which one fired -- exactly the
// shape that hid the crop field's blockers. Count each.
static long long g_hae_started = 0, g_hae_notReady = 0, g_hae_noTarget = 0,
                 g_hae_inUse = 0, g_hae_noPath = 0, g_hae_pathInvalid = 0, g_hae_ok = 0,
                 g_hae_found = 0;
static void HAE_Report() {
    static int s_t = -1;
    if (s_t < 0) { const char* e = std::getenv("BANISHED_CROPFIELD_TRACE");
                   s_t = (e && e[0] && e[0] != '0') ? 1 : 0; }
    static long long s_n = 0;
    ++s_n;
    if (s_t && (s_n <= 3 || (s_n % 20) == 0)) {
        std::fprintf(stderr, "[HuntEnter] alreadyStarted=%lld notReady=%lld noTarget=%lld "
                     "TARGETFOUND=%lld targetInUse=%lld noPath=%lld pathInvalid=%lld "
                     "REACHED_KILL=%lld\n",
                     g_hae_started, g_hae_notReady, g_hae_noTarget, g_hae_found, g_hae_inUse,
                     g_hae_noPath, g_hae_pathInvalid, g_hae_ok);
        std::fflush(stderr);
    }
}

// Ground truth for noTarget, independent of the quadtree. Walks the char manager's slot
// array directly (146 entries -- cheap, and no traversal to overflow, unlike the earlier
// radius-400 probe). For each live slot: distance from the hunter, and whether its owner
// entity carries the searched component type. This separates "the walk is still missing
// cells" from "nothing of that type is anywhere near the hunter".
static void HAE_BruteScan(void* gridPtr, const float* pos, int wantType, float radius) {
    static int s_t = -1;
    if (s_t < 0) { const char* e = std::getenv("BANISHED_CROPFIELD_TRACE");
                   s_t = (e && e[0] && e[0] != '0') ? 1 : 0; }
    if (!s_t || !gridPtr) return;
    static long long s_n = 0;
    const long long callNo = s_n++;

    const uint8_t* m = (const uint8_t*)gridPtr;
    const uint8_t* slots = *(const uint8_t* const*)(m + 0x10);
    int slotCount = *(const int*)(m + 0x18);
    if (!slots) return;
    if (slotCount > 4096) slotCount = 4096;

    // ---- TEMPORARY INSTRUMENTATION (2026-08-15 hunt-radius investigation) ----
    // The old version sampled only every 900th tick, so it described the distance
    // distribution at 132 instants out of 114560 -- it could not answer the question
    // that matters: does the nearest deer EVER dip under 8.0 on ANY tick? The distance
    // pass below now runs on every tick; only the expensive part (the per-entity
    // component-type probe) is cached per slot and refreshed every 512 calls, so the
    // steady-state cost is ~132 float distances per tick. Printing is still every 900,
    // but it reports the minimum and a histogram accumulated over ALL ticks between
    // prints, plus the hunter's world position so a sample lines up with [HuntWork].
    //
    // BLIND SPOT — read the "<8" bin with this in mind: this function is only reached
    // from the noTarget arm, i.e. on ticks where the query found NOTHING inside 8.0. So
    // the "<8" bin is 0 and minEver floors at exactly 8.00 BY CONSTRUCTION, not because
    // anything repels the deer. A tick with a deer inside 8.0 takes the other branch and
    // is never sampled here. Use [HuntEnter] TARGETFOUND for that half of the picture.
    static uint8_t s_typedCache[4096];
    static int s_cachedSlots = 0;
    if ((callNo % 512) == 0) {
        s_cachedSlots = slotCount;
        for (int i = 0; i < slotCount; ++i) {
            s_typedCache[i] = 0;
            const uint8_t* slot = slots + (int64_t)i * 0x18;
            void* ent = *(void* const*)(slot + 0x00);
            if (!ent || wantType == 0) continue;
            // same component-type test the query applies: owner at +0x10, component array
            // at +0x30 with count at +0x38, identity via vtable slot +0x10 (GetTypeId).
            void* owner = *(void* const*)((const uint8_t*)ent + 0x10);
            if (!owner) continue;
            int cnt = *(const int*)((const uint8_t*)owner + 0x38);
            void** comps = *(void** const*)((const uint8_t*)owner + 0x30);
            for (int c = 0; comps && c < cnt && c < 64; ++c) {
                void* comp = comps[c];
                if (!comp) continue;
                typedef int (*getId_t)(void*);
                getId_t g = *reinterpret_cast<getId_t*>(*(uintptr_t*)comp + 0x10);
                if (g && g(comp) == wantType) { s_typedCache[i] = 1; break; }
            }
        }
    }
    if (slotCount > s_cachedSlots) slotCount = s_cachedSlots;

    static float s_minEver = 1e30f;
    static float s_minWindow = 1e30f;
    static float s_minEverPos[2] = {0.0f, 0.0f};
    static long long s_bin[6] = {0, 0, 0, 0, 0, 0};
    static long long s_ticks = 0;

    int live = 0, typed = 0, inRadiusAny = 0;
    int n8 = 0, n12 = 0, n16 = 0, n24 = 0, n34 = 0;
    float bestAny = 1e30f, bestTyped = 1e30f;

    for (int i = 0; i < slotCount; ++i) {
        const uint8_t* slot = slots + (int64_t)i * 0x18;
        if (*(const int*)(slot + 0x10) < 0) continue;          // not placed in a cell
        void* ent = *(void* const*)(slot + 0x00);
        if (!ent) continue;
        ++live;
        void* holder = *(void* const*)((const uint8_t*)ent + 0x68);
        if (!holder) continue;
        float dx = *(const float*)((const uint8_t*)holder + 0x158) - pos[0];
        float dy = *(const float*)((const uint8_t*)holder + 0x15c) - pos[1];
        float dz = *(const float*)((const uint8_t*)holder + 0x160) - pos[2];
        float d = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (d < bestAny) bestAny = d;
        if (d < radius) ++inRadiusAny;

        if (!s_typedCache[i]) continue;
        ++typed;
        if (d < bestTyped) bestTyped = d;
        if (d <  8.0f) ++n8;
        if (d < 12.0f) ++n12;
        if (d < 16.0f) ++n16;
        if (d < 24.0f) ++n24;
        if (d < 34.0f) ++n34;
    }

    ++s_ticks;
    if (bestTyped < s_minWindow) s_minWindow = bestTyped;
    if (bestTyped < s_minEver) {
        s_minEver = bestTyped; s_minEverPos[0] = pos[0]; s_minEverPos[1] = pos[1];
    }
    if      (bestTyped <  8.0f) ++s_bin[0];
    else if (bestTyped < 12.0f) ++s_bin[1];
    else if (bestTyped < 16.0f) ++s_bin[2];
    else if (bestTyped < 24.0f) ++s_bin[3];
    else if (bestTyped < 34.0f) ++s_bin[4];
    else                        ++s_bin[5];

    if ((callNo % 900) != 0) return;

    std::fprintf(stderr,
        "[HuntScan] slots=%d live=%d typedTotal=%d hunter=(%.1f,%.1f) | now near=%.2f "
        "n8=%d n12=%d n16=%d n24=%d n34=%d inR_any=%d nearAny=%.2f\n",
        slotCount, live, typed, pos[0], pos[1],
        bestTyped > 1e29f ? -1.0f : bestTyped, n8, n12, n16, n24, n34,
        inRadiusAny, bestAny > 1e29f ? -1.0f : bestAny);
    std::fprintf(stderr,
        "[HuntScanAgg] ticks=%lld minWindow=%.2f minEver=%.2f at=(%.1f,%.1f) | tick bins "
        "<8:%lld 8-12:%lld 12-16:%lld 16-24:%lld 24-34:%lld >=34:%lld\n",
        s_ticks, s_minWindow > 1e29f ? -1.0f : s_minWindow,
        s_minEver > 1e29f ? -1.0f : s_minEver, s_minEverPos[0], s_minEverPos[1],
        s_bin[0], s_bin[1], s_bin[2], s_bin[3], s_bin[4], s_bin[5]);
    std::fflush(stderr);
    s_minWindow = 1e30f;
}

void HuntAnimal_Enter_Impl(HuntAnimal* self) {
    uint8_t* s = (uint8_t*)self;

    // If hunt already started, return early
    if (*(uint8_t*)(s + 0x38) != 0) {
        ++g_hae_started; HAE_Report();
        return;
    }

    // Check if the animal at +0x20 is ready to be hunted
    fn_InternalBasePtr_CheckPtr(s + 0x20);
    void* animalEntity = *(void**)(s + 0x20);
    if (!s_fn_IsAnimalReady(animalEntity)) {
        ++g_hae_notReady; HAE_Report();
        return;
    }

    // Set up search radius
    float searchRadius = *s_searchRadius;  // 8.0f

    // Initialize output IBP for spatial query
    void* queryOutIBP = nullptr;
    fn_InternalBasePtr_Ctor(&queryOutIBP, nullptr);

    // Get the search type ID
    int searchTypeId = *s_typeId_HuntAnimalSearch;

    // Get herd entity's CharacterComponent position
    // Follow chain: herdEntity(+0x28) -> +0x278 -> +0x68 -> position at +0x158
    fn_InternalBasePtr_CheckPtr(s + 0x28);
    void* herdEntity = *(void**)(s + 0x28);

    fn_InternalBasePtr_CheckPtr((uint8_t*)herdEntity + 0x278);
    void* charComp = *(void**)((uint8_t*)herdEntity + 0x278);

    fn_InternalBasePtr_CheckPtr((uint8_t*)charComp + 0x68);
    void* posHolder = *(void**)((uint8_t*)charComp + 0x68);

    // Read position (3 floats at +0x158, +0x15c, +0x160)
    float searchPos[3];
    searchPos[0] = *(float*)((uint8_t*)posHolder + 0x158);
    searchPos[1] = *(float*)((uint8_t*)posHolder + 0x15c);
    searchPos[2] = *(float*)((uint8_t*)posHolder + 0x160);

    // Spatial query: find nearby entity
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    void* gridPtr = *(void**)(gameWorld + 0x88);
    void* queryResult = nullptr;

    s_fn_SpatialQuery(gridPtr, &queryResult, searchPos, &searchRadius, searchTypeId, &queryOutIBP);

    // If no result found, bail
    if (queryResult == nullptr) {
        ++g_hae_noTarget; HAE_Report();
        HAE_BruteScan(gridPtr, searchPos, searchTypeId, searchRadius);
        return;
    }

    ++g_hae_found;   // FIX 2026-08-15: the query DOES succeed; nothing used to record it
    // Get the found entity from query result
    fn_InternalBasePtr_CheckPtr(&queryResult);
    void* foundEntityParent = *(void**)((uint8_t*)queryResult + 0x10);

    // Check if the found entity's model component has bit 20 set (in use)
    void* tempIBP = nullptr;
    tempIBP = foundEntityParent;
    fn_InternalBasePtr_CheckPtr(&tempIBP);
    void* modelCompIBP1[2] = {nullptr, nullptr};
    s_fn_GetModelComp(tempIBP, modelCompIBP1);
    fn_InternalBasePtr_CheckPtr(modelCompIBP1);
    uint32_t modelFlags = *(uint32_t*)((uint8_t*)(modelCompIBP1[0]) + 0x20);

    if ((modelFlags >> 20) & 1) {
        // Entity already in use, bail
        ++g_hae_inUse; HAE_Report();   // FIX 2026-08-15: exit was never counted
        // IN-USE CONTENTION CENSUS (BANISHED_TRACKER_TRACE=1). Bit 20 is a RESERVATION
        // that HuntAnimal::Enter takes on its target (0x140107260, from the RemoveAnimal
        // ctor) and that is consumed when the hunter's chain finally runs RemoveAnimal.
        // A high inUse count is FAITHFUL contention if it is a handful of deer being
        // re-found while their kill is in flight, and a LEAK if the distinct-entity
        // count keeps climbing. The raw counter cannot tell those apart, so track
        // distinct targets and the per-target hit counts.
        {
            static int t = -1;
            if (t < 0) { const char* e = std::getenv("BANISHED_TRACKER_TRACE");
                         t = (e && e[0] && e[0] != '0') ? 1 : 0; }
            if (t) {
                static void* ents[32] = {nullptr};
                static long long hits[32] = {0};
                static int nEnts = 0;
                static long long total = 0, overflow = 0;
                void* key = *(void**)((uint8_t*)queryResult + 0x10);
                ++total;
                int slot = -1;
                for (int i = 0; i < nEnts; i++) if (ents[i] == key) { slot = i; break; }
                if (slot < 0) {
                    if (nEnts < 32) { slot = nEnts++; ents[slot] = key; }
                    else ++overflow;
                }
                if (slot >= 0) ++hits[slot];
                if ((total % 200) == 0) {
                    std::fprintf(stderr, "[HuntInUse] hits=%lld distinctTargets=%d "
                                 "(cap 32, unrecorded=%lld) top:", total, nEnts, overflow);
                    for (int i = 0; i < nEnts && i < 6; i++)
                        std::fprintf(stderr, " %p=%lld", ents[i], hits[i]);
                    std::fprintf(stderr, "\n");
                    std::fflush(stderr);
                }
            }
        }
        return;
    }

    // Get model component from the found entity (second call for later use)
    fn_InternalBasePtr_CheckPtr(&queryResult);
    foundEntityParent = *(void**)((uint8_t*)queryResult + 0x10);
    tempIBP = foundEntityParent;
    fn_InternalBasePtr_CheckPtr(&tempIBP);
    void* modelCompIBP2[2] = {nullptr, nullptr};
    s_fn_GetModelComp(tempIBP, modelCompIBP2);

    // Get map pointer from game world for grid calculations
    int* mapPtr = *(int**)(gameWorld + 0xf8);

    // Get found entity's position for grid cell calculation
    fn_InternalBasePtr_CheckPtr(&queryResult);
    void* foundEntity2 = queryResult;
    fn_InternalBasePtr_CheckPtr((uint8_t*)foundEntity2 + 0x68);
    void* foundPosHolder = *(void**)((uint8_t*)foundEntity2 + 0x68);

    float foundPosX = *(float*)((uint8_t*)foundPosHolder + 0x158);
    float foundPosY = *(float*)((uint8_t*)foundPosHolder + 0x15c);

    // Get herd position again for the herd's grid cell
    fn_InternalBasePtr_CheckPtr(s + 0x28);
    herdEntity = *(void**)(s + 0x28);
    fn_InternalBasePtr_CheckPtr((uint8_t*)herdEntity + 0x278);
    charComp = *(void**)((uint8_t*)herdEntity + 0x278);
    fn_InternalBasePtr_CheckPtr((uint8_t*)charComp + 0x68);
    posHolder = *(void**)((uint8_t*)charComp + 0x68);

    int mapSize = *mapPtr;
    float herdPosX2 = *(float*)((uint8_t*)posHolder + 0x158);
    float herdPosY2 = *(float*)((uint8_t*)posHolder + 0x15c);

    // Convert world positions to grid cell indices
    float halfSize = (float)(mapSize >> 1);

    // Herd position -> grid cell (toCell)
    int herdCellX = (int)(herdPosX2 + halfSize);
    int herdCellY = (int)(herdPosY2 + halfSize);
    if (herdCellX < 0) herdCellX = 0;
    if (herdCellX >= mapSize) herdCellX = mapSize - 1;
    if (herdCellY < 0) herdCellY = 0;
    if (herdCellY >= mapSize) herdCellY = mapSize - 1;
    int toCell = mapSize * herdCellY + herdCellX;

    // Found entity position -> grid cell (fromCell)
    int foundCellX = (int)(foundPosX + halfSize);
    int foundCellY = (int)(foundPosY + halfSize);
    if (foundCellX < 0) foundCellX = 0;
    if (foundCellX >= mapSize) foundCellX = mapSize - 1;
    if (foundCellY < 0) foundCellY = 0;
    if (foundCellY >= mapSize) foundCellY = mapSize - 1;
    int fromCell = mapSize * foundCellY + foundCellX;

    // Check path cost between the two cells
    uint32_t pathCost = (uint32_t)s_fn_FindPathCost(mapPtr, fromCell, toCell, 0xe1f);
    if ((int)pathCost < 0) {
        // No valid path, bail
        ++g_hae_noPath; HAE_Report();   // FIX 2026-08-15: exit was never counted
        return;
    }

    // Check path validity
    uint64_t pathValid = s_fn_CheckPathValid(mapPtr, 0xe1f, (uint64_t)pathCost, toCell);
    if ((char)pathValid == 0) {
        // Path not valid, bail
        ++g_hae_pathInvalid; HAE_Report();   // FIX 2026-08-15: exit was never counted
        return;
    }

    // Path is valid -- proceed with corpse spawning

    // Get description from the found entity's model component
    fn_InternalBasePtr_CheckPtr(modelCompIBP2);
    void* descObj = *(void**)((uint8_t*)(modelCompIBP2[0]) + 0x18);
    bool isModelDesc = fn_IsDerivedFrom(descObj, *s_typeId_ModelDesc);
    void* modelDesc = isModelDesc ? descObj : nullptr;

    // Store model component IBP for corpse entity ref
    void* corpseModelIBP = modelCompIBP2[0];

    // Convert grid cell to world position
    float gridPos[3];
    float worldPos[3];
    float halfFloat = (float)(mapSize >> 1) - *s_halfOffset;
    int cellIdx = (int)pathCost;
    gridPos[0] = (float)(cellIdx % mapSize) - halfFloat;  // x = col
    gridPos[1] = (float)(cellIdx / mapSize) - halfFloat;  // y = row
    gridPos[2] = 0.0f;

    // Use Vector3 ctor -- note: this is called at 0x140152a30 slot
    fn_Vector3_Ctor(gridPos, gridPos[0], gridPos[1], 0.0f);

    s_fn_GridToWorld(mapPtr, worldPos, gridPos, 0, nullptr);

    // Spawn the corpse -- call RemoveAnimal_Action_Ctor
    // Params: herdIBP(+0x28), animIndex(-1), worldPos, fromCell, corpseIBP, descPtr, flags(0)
    void* herdRef = *(void**)(s + 0x28);
    void* herdIBP_local[2];
    herdIBP_local[0] = herdRef;

    void* corpseIBP_local[2];
    corpseIBP_local[0] = corpseModelIBP;

    s_fn_SpawnCorpse(herdIBP_local, -1, worldPos, fromCell, corpseIBP_local, modelDesc, 0);

    // Find HerdComponent from the found entity and start migration
    fn_InternalBasePtr_CheckPtr(&queryResult);
    foundEntityParent = *(void**)((uint8_t*)queryResult + 0x10);
    tempIBP = foundEntityParent;
    fn_InternalBasePtr_CheckPtr(&tempIBP);

    void* herdCompIBP[2] = {nullptr, nullptr};
    s_fn_FindHerdComp(tempIBP, herdCompIBP);

    fn_InternalBasePtr_CheckPtr(herdCompIBP);

    // Read herd component IBP at +0x28 from the resolved object
    // herdCompIBP[0] IS the resolved object pointer after CheckPtr (no extra deref)
    void* migrationTarget = *(void**)((uintptr_t)herdCompIBP[0] + 0x28);

    fn_InternalBasePtr_CheckPtr(&migrationTarget);
    s_fn_StartMigration(migrationTarget);

    // Clear the animal entity's time at +0x50 and set hunt started flag
    fn_InternalBasePtr_CheckPtr(s + 0x20);
    animalEntity = *(void**)(s + 0x20);
    *(uint64_t*)((uint8_t*)animalEntity + 0x50) = 0;

    *(uint8_t*)(s + 0x38) = 1;
    ++g_hae_ok; HAE_Report();   // FIX 2026-08-15: the success arm was never counted
}

// ============================================================
// Slot 9: GetTarget (0x1400c7d60)
// Returns true if param matches the herd entity's parent (+0x28->+0x10)
// or the animal entity's parent (+0x20->+0x10).
// ============================================================
bool HuntAnimal_GetTarget_Impl(HuntAnimal* self, void* param) {
    HuntAnimal_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    uint8_t* s = (uint8_t*)self;

    // Check herd entity's parent at +0x28 -> +0x10
    fn_InternalBasePtr_CheckPtr(s + 0x28);
    void* herdEntity = *(void**)(s + 0x28);
    void* herdParent = *(void**)((uint8_t*)herdEntity + 0x10);

    fn_InternalBasePtr_CheckPtr(param);
    if (herdParent == *(void**)param) {
        return true;
    }

    // Check animal entity's parent at +0x20 -> +0x10
    fn_InternalBasePtr_CheckPtr(s + 0x20);
    void* animalEntity = *(void**)(s + 0x20);
    void* animalParent = *(void**)((uint8_t*)animalEntity + 0x10);

    fn_InternalBasePtr_CheckPtr(param);
    if (animalParent == *(void**)param) {
        return true;
    }

    return false;
}

// ============================================================
// Standalone factory + vtable — Action family (notes/FACTORY_CASCADE_145_BRIEF.md).
// Vtable @ 0x14015fde0 (10 slots), size 0x40, parent "Action".
// Shared slot helpers live in action_base_standalone.{h,cpp}.
// ============================================================
#include "action_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t HuntAnimal_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("HuntAnimal");
    return c;
}

static void* g_HuntAnimal_Vtable[10] = {
    (void*)&Action_ScalarDtor,        // +0x00 ScalarDtor
    (void*)&Action_Slot1,             // +0x08 slot1 (returns null)
    (void*)&HuntAnimal_GetTypeId,           // +0x10 GetTypeId
    (void*)&HuntAnimal_Serialize_Impl,      // +0x18 Serialize
    (void*)&HuntAnimal_AbstractImpl_Impl,   // +0x20 AbstractImpl
    (void*)&HuntAnimal_Execute_Impl,   // +0x28 Execute
    (void*)&HuntAnimal_Enter_Impl,  // +0x30 Enter
    (void*)&Action_Exit,   // +0x38 Exit
    (void*)&Action_BaseCheckEntity,  // +0x40 CheckEntity
    (void*)&HuntAnimal_GetTarget_Impl, // +0x48 GetTarget
};

extern "C" void RegisterHuntAnimalVtable() {
    RegisterExeAddr(0x14015fde0, (uintptr_t)g_HuntAnimal_Vtable);
}
extern "C" void* HuntAnimal_CreateInPlace(void* mem) {
    HuntAnimal_ResolveFuncs();
    return Action_InitInPlace(mem, 0x40, g_HuntAnimal_Vtable);
}
extern "C" void* HuntAnimal_Factory(void) {
    HuntAnimal_ResolveFuncs();
    return Action_Alloc(0x40, g_HuntAnimal_Vtable);
}
REGISTER_TYPE(HuntAnimal, "Action", &HuntAnimal_Factory, &HuntAnimal_CreateInPlace);
