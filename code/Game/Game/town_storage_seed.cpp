// code/Game/Game/town_storage_seed.cpp
// Economy Brick M-D (WORLD/STORAGE half) — town storage-location registry +
// retail starting-resource seeding, so the start-town StorageBarn/StorageYard
// become REAL stocked storage the citizen eat chain can find.
//
// Faithful ports (Ghidra, Application-x64.exe):
//   * 0x14012d230 StorageLocationDescription_helper_d230
//       -> TownRegistry_RegisterStorageLocation(comp)
//       Appends *(comp+0x10) (the owning ENTITY pointer) to the town
//       storage-location list at townMgr+0x00 (count +0x08, cap +0x0c) via the
//       grow fn FUN_140003110 (registered). townMgr = *(gameWorld+0xf0).
//       Retail fires this from the StorageLocationComponent OnAdded vtable slot.
//   * 0x140107ff0 BuildDescription_helper_7ff0
//       -> TownStorage_BuildCompleteInit_7ff0(componentWithEntityAt0x10)
//       The BUILD-COMPLETE storage init: finds the entity's StorageComponent,
//       credits any construction-slot contents to the town stock table
//       (townMgr+0x170, index via FUN_140088f90), then re-initializes from the
//       StorageDescription: slots cleared (FUN_14000f090), +0x3c = _storageFlags
//       (desc+0x28), +0x38 = _volumeLimit (desc+0x24) * (desc+0x20 _areaBasedLimit
//       ? mapW*mapH : 1). NOTE: it does NOT test desc+0x21 (_available) — that
//       byte only gates the OnAdded slot-rebuild (StorageComponent_Draw
//       0x14012c640), i.e. storage while still a construction site. This is how
//       a completed barn (_available=false, _volumeLimit=6000) gets capacity.
//   * 0x140091660 Game_SpawnResources(townMgr, materialFlagMask, count)
//       Phase A: filter the raw-material TEMPLATE entity list at townMgr+0x110
//       (count +0x118) by RawMaterialDescription(+0x5c) & mask (descriptions on a
//       template live at +0x28 array / +0x30 count, raw pointers).
//       Phase B: round-robin the matched templates over the storage-location
//       entities at townMgr+0x00, one unit at a time: accept-check
//       StorageComponent_impl_cd80 then AddItem FUN_14012d000 (registered),
//       until `count` units placed or no storage accepts.
//   * 0x1400a1f00 Game_impl_1f00 -> StartTown_SeedStartingResources(startConfig)
//       Seven Game_SpawnResources calls with masks 0x400/0x800/0x1000/0x200/
//       0x20/0x40/0x4000 and counts from startConfig+0xfc..+0x114. Verified at
//       the FUN_1400a3300 call site 0x1400a342b: RCX = RDI = a3300's param_1 =
//       the SAME startConfig object StartTown_SpawnEntities receives.
//
// Private 1:1 helper transcriptions (needed because the registered ports of
// these exe addresses — Pf_StorageCanAccept_cd80 / Pf_ResourceLimitFlagTest_c560
// / CitizenComp_StorageComponent_QueryCount / WorkBase_FindComponent_6270 — all
// read their type-ids from raw .rdata addresses that resolve to NULL in
// standalone, so their type match never fires there; these files are outside
// this brick's disjoint-file contract, so the working-type-id copies live here):
//   * 0x14012cd80 StorageComponent_impl_cd80 (accept check)
//   * 0x14012c560 StorageComponent_helper_c560 (flag-mask gate)
//   * 0x14012cf10 StorageComponent::QueryCount (volume-weighted total)
//   * 0x140006270 Entity_FindComponent_Storage (component scan)
//
// Type-ids are obtained with the documented fallback: prefer the raw .rdata
// global when resolvable AND non-zero, else ::Object::GetTypeIdByName (valid
// after Object::Factory::Startup). The shared .rdata globals are never written.
//
// Standalone-inert deviations (documented; retail dereferences unguarded):
//   * SpawnResources phase A / cd80 / c560 / QueryCount: a template with no
//     RawMaterialDescription would make retail read *(NULL+0x5c/0x48) — every
//     retail raw-material template has one; we skip/return-0 instead of faulting.
//   * 7ff0: bails (with trace) when the entity has no StorageComponent or the
//     component has no StorageDescription (retail would fault).
//
// Trace: BANISHED_STORAGE_TRACE=1 prints [StorageSeed] lines.

#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ============================================================
// Env + trace helpers
// ============================================================
static bool StorageTraceEnabled() {
    static int v = -1;
    if (v < 0) {
        const char* e = getenv("BANISHED_STORAGE_TRACE");
        v = (e && *e && e[0] != '0') ? 1 : 0;
    }
    return v == 1;
}

#define STG_TRACE(...)                                              \
    do {                                                            \
        if (StorageTraceEnabled()) {                                \
            fprintf(stderr, "[StorageSeed] " __VA_ARGS__);          \
            fprintf(stderr, "\n");                                  \
            fflush(stderr);                                         \
        }                                                           \
    } while (0)

// ============================================================
// Type-ids (raw .rdata preferred, GetTypeIdByName fallback)
// ============================================================
static uint32_t Tid(uintptr_t rdataAddr, const char* name) {
    const uint32_t* p =
        reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, rdataAddr));
    if (p && *p) return *p;
    return ::Object::GetTypeIdByName(name);
}
static uint32_t TidRawMaterialDescription() { return Tid(0x1401c7cb8, "RawMaterialDescription"); }
static uint32_t TidStorageComponent()       { return Tid(0x1401c7e78, "StorageComponent"); }
static uint32_t TidStorageDescription()     { return Tid(0x1401c7eb0, "StorageDescription"); }
static uint32_t TidMapComponent()           { return Tid(0x1401c79e0, "MapComponent"); }

// ============================================================
// Resolved exe helpers (all already registered in standalone)
// ============================================================
typedef void (*fn_GrowAppend8_t)(void* list, int newCount, void* value);       // 0x140003110
typedef void (*fn_SeedAddItem_t)(uintptr_t storage, int64_t mat, int n);       // 0x14012d000
typedef void (*fn_SlotArrayResize_t)(void* arrayBase, int newCount);           // 0x14000f090
typedef int  (*fn_StockTableIndex_t)(int64_t a, int64_t material);             // 0x140088f90

static fn_GrowAppend8_t    s_fnGrowAppend8    = nullptr;
static fn_SeedAddItem_t    s_fnAddItem        = nullptr;
static fn_SlotArrayResize_t s_fnSlotResize    = nullptr;
static fn_StockTableIndex_t s_fnStockIndex    = nullptr;
static bool s_fnInit = false;

static void EnsureFns() {
    if (s_fnInit) return;
    s_fnGrowAppend8 = reinterpret_cast<fn_GrowAppend8_t>(ResolveAddr(Module::Exe, 0x140003110));
    s_fnAddItem     = reinterpret_cast<fn_SeedAddItem_t>(ResolveAddr(Module::Exe, 0x14012d000));
    s_fnSlotResize  = reinterpret_cast<fn_SlotArrayResize_t>(ResolveAddr(Module::Exe, 0x14000f090));
    s_fnStockIndex  = reinterpret_cast<fn_StockTableIndex_t>(ResolveAddr(Module::Exe, 0x140088f90));
    s_fnInit = true;
}

static inline void* TownMgr() {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return nullptr;
    return *reinterpret_cast<void**>(
        reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xf0);
}

static inline void CheckPtr(void* p) {
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p);
}
static inline uint32_t GetTypeIdOf(void* obj) {
    typedef uint32_t (*fn_GetTypeId_t)(void*);
    return (*reinterpret_cast<fn_GetTypeId_t*>(*reinterpret_cast<uintptr_t*>(obj) + 0x10))(obj);
}

// ============================================================
// Private 1:1 helper transcriptions
// ============================================================

// FUN_140002ff0-family description scan: find the RawMaterialDescription on a
// TEMPLATE entity (descriptions at +0x28 raw-pointer array, count +0x30) — the
// exact inline loop of Game_SpawnResources / cd80 / c560 / QueryCount.
static void* FindRawMatDescOnTemplate(int64_t tmplEntity) {
    if (!tmplEntity || !fn_IsDerivedFrom) return nullptr;
    const uint32_t tid = TidRawMaterialDescription();
    int count = *reinterpret_cast<int*>(tmplEntity + 0x30);
    int64_t arr = *reinterpret_cast<int64_t*>(tmplEntity + 0x28);
    for (int i = 0; i < count; i++) {
        void* desc = *reinterpret_cast<void**>(arr + static_cast<int64_t>(i) * 8);
        if (GetTypeIdOf(desc) == tid) {
            // Original: IsDerivedFrom-narrow, break either way.
            return fn_IsDerivedFrom(desc, tid) ? desc : nullptr;
        }
    }
    return nullptr;
}

// Entity_FindComponent_Storage (FUN_140006270) equivalent: scan a live entity's
// component-IBP array (+0x30 array / +0x38 count, CheckPtr each slot) for the
// StorageComponent. Returns raw component pointer or null.
static void* FindStorageOnEntity(int64_t entity) {
    if (!entity) return nullptr;
    const uint32_t tid = TidStorageComponent();
    int count = *reinterpret_cast<int*>(entity + 0x38);
    for (int i = 0; i < count; i++) {
        void** slot = reinterpret_cast<void**>(
            *reinterpret_cast<int64_t*>(entity + 0x30) + static_cast<int64_t>(i) * 8);
        CheckPtr(slot);
        void* comp = *slot;
        if (!comp) continue;
        if (GetTypeIdOf(comp) == tid) {
            return (fn_IsDerivedFrom && fn_IsDerivedFrom(comp, tid)) ? comp : nullptr;
        }
    }
    return nullptr;
}

// StorageComponent::QueryCount (0x14012cf10) — VOLUME-weighted total:
// sum over slots (comp+0x28, stride 0x10) of RawMaterialDescription(+0x48) *
// slotCount(+0x08). (Slot material entity at slot+0x00.)
static int Seed_QueryCount(int64_t storageComp) {
    int total = 0;
    int slots = *reinterpret_cast<int*>(storageComp + 0x30);
    int64_t arr = *reinterpret_cast<int64_t*>(storageComp + 0x28);
    for (int i = 0; i < slots; i++) {
        int64_t slot = arr + static_cast<int64_t>(i) * 0x10;
        void* desc = FindRawMatDescOnTemplate(*reinterpret_cast<int64_t*>(slot));
        // Standalone-inert guard: retail reads desc+0x48 unguarded.
        int unitVol = desc ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(desc) + 0x48) : 0;
        total += unitVol * *reinterpret_cast<int*>(slot + 0x08);
    }
    return total;
}

// StorageComponent_helper_c560 (0x14012c560): does the storage's material-flag
// mask (+0x3c) intersect the material's RawMaterialDescription flags (+0x5c)?
static int Seed_FlagTest_c560(int64_t storageComp, int64_t matTemplEntity) {
    void* desc = FindRawMatDescOnTemplate(matTemplEntity);
    // Standalone-inert guard: retail reads *(NULL+0x5c) when no description.
    if (!desc) return 0;
    uint32_t compMask  = *reinterpret_cast<uint32_t*>(storageComp + 0x3c);
    uint32_t descFlags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(desc) + 0x5c);
    return ((compMask & descFlags) != 0) ? 1 : 0;
}

// StorageComponent_impl_cd80 (0x14012cd80): accept check —
// c560 flag gate && (unitVolume * n + QueryCount) <= capacity(+0x38).
static int Seed_CanAccept_cd80(int64_t storageComp, int64_t matTemplEntity, int n) {
    if (!Seed_FlagTest_c560(storageComp, matTemplEntity)) return 0;
    int current = Seed_QueryCount(storageComp);
    void* desc = FindRawMatDescOnTemplate(matTemplEntity);
    if (!desc) return 0;  // standalone-inert guard (retail reads *(NULL+0x48))
    int unitVol = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(desc) + 0x48);
    return (unitVol * n + current <= *reinterpret_cast<int*>(storageComp + 0x38)) ? 1 : 0;
}

// ============================================================
// 0x14012d230 — TownRegistry_RegisterStorageLocation
// ============================================================
// 1:1: appends the component's owning ENTITY pointer (*(comp+0x10)) to the
// town storage-location list (townMgr+0x00 base / +0x08 count / +0x0c cap)
// via FUN_140003110. Retail does NOT dedup and does NOT CheckPtr here.
extern "C" void TownRegistry_RegisterStorageLocation(void* storageLocationComp) {
    EnsureFns();
    void* townMgr = TownMgr();
    if (!townMgr || !s_fnGrowAppend8 || !storageLocationComp) {
        STG_TRACE("RegisterStorageLocation SKIP (townMgr=%p grow=%p comp=%p)",
                  townMgr, reinterpret_cast<void*>(s_fnGrowAppend8), storageLocationComp);
        return;
    }
    uint64_t entityRef = *reinterpret_cast<uint64_t*>(
        reinterpret_cast<uintptr_t>(storageLocationComp) + 0x10);
    int newCount = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(townMgr) + 8) + 1;
    s_fnGrowAppend8(townMgr, newCount, &entityRef);
    STG_TRACE("RegisterStorageLocation entity=%p -> townMgr+0x00 count=%d",
              reinterpret_cast<void*>(entityRef),
              *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(townMgr) + 8));
}

// ============================================================
// 0x140107ff0 — TownStorage_BuildCompleteInit_7ff0
// ============================================================
// param = any component whose +0x10 holds the building's entity IBP (retail
// passes the BuildComponent; the StorageComponent itself works — same layout).
extern "C" void TownStorage_BuildCompleteInit_7ff0(void* componentWithEntity) {
    EnsureFns();
    if (!componentWithEntity) return;
    uintptr_t p = reinterpret_cast<uintptr_t>(componentWithEntity);

    CheckPtr(reinterpret_cast<void*>(p + 0x10));
    int64_t entity = *reinterpret_cast<int64_t*>(p + 0x10);
    void* storageComp = FindStorageOnEntity(entity);
    if (!storageComp) {
        STG_TRACE("BuildCompleteInit: no StorageComponent on entity %p",
                  reinterpret_cast<void*>(entity));
        return;
    }
    int64_t sc = reinterpret_cast<int64_t>(storageComp);
    void* townMgr = TownMgr();

    // Credit existing (construction-delivery) slot contents to the town stock
    // table at townMgr+0x170 (+0xc field per 0x20-stride row, index FUN_140088f90).
    // No-op for a freshly spawned building (0 slots).
    int slots = *reinterpret_cast<int*>(sc + 0x30);
    if (slots > 0 && s_fnStockIndex && townMgr &&
        *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(townMgr) + 0x170)) {
        int64_t arr = *reinterpret_cast<int64_t*>(sc + 0x28);
        for (int i = 0; i < slots; i++) {
            int64_t slot = arr + static_cast<int64_t>(i) * 0x10;
            int cnt = *reinterpret_cast<int*>(slot + 8);
            int idx = s_fnStockIndex(arr, *reinterpret_cast<int64_t*>(slot));
            if (idx >= 0) {
                int* cell = reinterpret_cast<int*>(
                    static_cast<int64_t>(idx) * 0x20 + 0xc +
                    *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(townMgr) + 0x170));
                *cell += cnt;
            }
        }
    }

    // StorageDescription (comp+0x18, IsDerivedFrom-narrowed).
    void* descObj = *reinterpret_cast<void**>(sc + 0x18);
    void* desc = nullptr;
    if (descObj && fn_IsDerivedFrom &&
        fn_IsDerivedFrom(descObj, TidStorageDescription()))
        desc = descObj;
    if (!desc) {
        STG_TRACE("BuildCompleteInit: no StorageDescription on comp %p (desc=%p)",
                  storageComp, descObj);
        return;  // standalone-inert guard (retail reads *(NULL+0x20))
    }
    uintptr_t d = reinterpret_cast<uintptr_t>(desc);

    // _areaBasedLimit (+0x20): multiply _volumeLimit by the MapComponent footprint.
    int tileArea = 1;
    if (*reinterpret_cast<uint8_t*>(d + 0x20) != 0) {
        CheckPtr(reinterpret_cast<void*>(sc + 0x10));
        int64_t ent2 = *reinterpret_cast<int64_t*>(sc + 0x10);
        // Find the MapComponent (typeid 0x1401c79e0) on the entity.
        void* mapComp = nullptr;
        const uint32_t tidMap = TidMapComponent();
        int compCount = ent2 ? *reinterpret_cast<int*>(ent2 + 0x38) : 0;
        for (int i = 0; i < compCount; i++) {
            void** slot = reinterpret_cast<void**>(
                *reinterpret_cast<int64_t*>(ent2 + 0x30) + static_cast<int64_t>(i) * 8);
            CheckPtr(slot);
            void* comp = *slot;
            if (comp && GetTypeIdOf(comp) == tidMap) {
                mapComp = (fn_IsDerivedFrom && fn_IsDerivedFrom(comp, tidMap)) ? comp : nullptr;
                break;
            }
        }
        // Original wraps in an IBP (fn_InternalBasePtr_Ctor) then CheckPtr's it.
        uintptr_t mapIBP = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&mapIBP, mapComp);
        CheckPtr(&mapIBP);
        if (mapIBP) {
            tileArea = static_cast<int>(*reinterpret_cast<int16_t*>(mapIBP + 0x2e)) *
                       static_cast<int>(*reinterpret_cast<int16_t*>(mapIBP + 0x2c));
        } else {
            STG_TRACE("BuildCompleteInit: _areaBasedLimit set but no MapComponent (area=1)");
        }
    }

    int volumeLimit    = *reinterpret_cast<int*>(d + 0x24);
    uint32_t storageFlags = *reinterpret_cast<uint32_t*>(d + 0x28);
    if (s_fnSlotResize) s_fnSlotResize(reinterpret_cast<void*>(sc + 0x28), 0);
    *reinterpret_cast<uint32_t*>(sc + 0x3c) = storageFlags;
    *reinterpret_cast<int*>(sc + 0x38) = volumeLimit * tileArea;

    STG_TRACE("BuildCompleteInit comp=%p desc(+0x20 area=%d, +0x21 avail=%d, "
              "+0x24 vol=%d, +0x28 flags=0x%x) -> cap(+0x38)=%d mask(+0x3c)=0x%x",
              storageComp,
              *reinterpret_cast<uint8_t*>(d + 0x20), *reinterpret_cast<uint8_t*>(d + 0x21),
              volumeLimit, storageFlags,
              *reinterpret_cast<int*>(sc + 0x38), *reinterpret_cast<uint32_t*>(sc + 0x3c));
}

// ============================================================
// 0x140091660 — Game_SpawnResources(townMgr, mask, count)
// ============================================================
extern "C" void TownStorage_SpawnResources_91660(void* townMgrV, uint32_t mask, int count) {
    EnsureFns();
    int64_t* townMgr = reinterpret_cast<int64_t*>(townMgrV);
    if (!townMgr) return;

    // ---- Phase A: filter the raw-material template list (+0x110/+0x118) ----
    int64_t* matched = nullptr;   // local_48
    int matchedCount = 0;         // local_40 low dword
    int matchedCap = 0;           // local_40 high dword

    int tmplCount = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(townMgr) + 0x118);
    int64_t tmplArr = *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(townMgr) + 0x110);
    for (int t = 0; t < tmplCount; t++) {
        int64_t tmpl = *reinterpret_cast<int64_t*>(tmplArr + static_cast<int64_t>(t) * 8);
        void* desc = FindRawMatDescOnTemplate(tmpl);
        // Standalone-inert guard: retail reads *(desc+0x5c) unguarded.
        if (!desc) continue;
        if ((*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(desc) + 0x5c) & mask) == 0)
            continue;
        // Faithful grow-append (8-byte entries; cap = n+1, tripled when growing
        // a non-empty array; copy survivors; free the old buffer).
        int newCount = matchedCount + 1;
        int64_t* oldBuf = matched;
        if (matchedCap < newCount) {
            matchedCap = newCount;
            if (matchedCount != 0) matchedCap = newCount * 3;
            matched = reinterpret_cast<int64_t*>(
                fn_MemAlloc(g_pMemAllocSingleton,
                            static_cast<uint64_t>(matchedCap) << 3, 8));
        }
        int start = (oldBuf == matched) ? matchedCount : 0;
        for (int i = start; i < newCount; i++) {
            if (!matched) break;
            matched[i] = (i < matchedCount) ? oldBuf[i] : tmpl;
        }
        if (oldBuf != matched && oldBuf) fn_MemFreeStatic(oldBuf);
        matchedCount = newCount;
    }

    // ---- Phase B: round-robin the matched templates over townMgr+0x00 ----
    int placed = 0;
    if (matchedCount > 0 && count > 0 && s_fnAddItem) {
        for (;;) {
            bool progress = false;
            int locCount = static_cast<int>(townMgr[1]);   // +0x08
            if (locCount < 1) break;
            for (int i = 0; i < locCount; i++) {
                int64_t entityRef = *reinterpret_cast<int64_t*>(
                    townMgr[0] + static_cast<int64_t>(i) * 8);
                CheckPtr(&entityRef);
                void* storage = FindStorageOnEntity(entityRef);
                // Original wraps the found component in an IBP before use.
                uintptr_t compIBP = 0;
                if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&compIBP, storage);
                if (placed < count) {
                    int64_t tmpl = matched[placed % matchedCount];
                    CheckPtr(&compIBP);
                    if (compIBP &&
                        Seed_CanAccept_cd80(static_cast<int64_t>(compIBP), tmpl, 1)) {
                        CheckPtr(&compIBP);
                        s_fnAddItem(compIBP, tmpl, 1);
                        placed++;
                        progress = true;
                    }
                }
            }
            if (!progress || placed >= count) break;
        }
    }
    STG_TRACE("SpawnResources mask=0x%x want=%d matchedTemplates=%d placed=%d",
              mask, count, matchedCount, placed);
    if (matched) fn_MemFreeStatic(matched);
}

// ============================================================
// 0x1400a1f00 — StartTown_SeedStartingResources(startConfig)
// ============================================================
extern "C" void StartTown_SeedStartingResources(void* startConfig) {
    void* townMgr = TownMgr();
    if (!startConfig || !townMgr) {
        STG_TRACE("SeedStartingResources SKIP (config=%p townMgr=%p)", startConfig, townMgr);
        return;
    }
    uint8_t* sc = reinterpret_cast<uint8_t*>(startConfig);
    // Mask labels corrected 2026-07-19 (binary-verified flag values, see
    // engine/types/RawMaterialDescription.h): 0x20 = EDIBLE food, 0x40 = FUEL
    // firewood. The 0x400/0x800/0x1000/0x200/0x4000 class names below are the
    // best current guesses for the remaining start-resource classes
    // (logs/stone/iron/tools/clothing) — amounts and masks are exe-faithful
    // regardless of naming.
    STG_TRACE("SeedStartingResources counts: m400=%d m800=%d "
              "m1000=%d m200=%d food(0x20)=%d firewood(0x40)=%d m4000=%d",
              *reinterpret_cast<int*>(sc + 0xfc),  *reinterpret_cast<int*>(sc + 0x100),
              *reinterpret_cast<int*>(sc + 0x104), *reinterpret_cast<int*>(sc + 0x108),
              *reinterpret_cast<int*>(sc + 0x10c), *reinterpret_cast<int*>(sc + 0x110),
              *reinterpret_cast<int*>(sc + 0x114));
    TownStorage_SpawnResources_91660(townMgr, 0x400,  *reinterpret_cast<int*>(sc + 0xfc));
    TownStorage_SpawnResources_91660(townMgr, 0x800,  *reinterpret_cast<int*>(sc + 0x100));
    TownStorage_SpawnResources_91660(townMgr, 0x1000, *reinterpret_cast<int*>(sc + 0x104));
    TownStorage_SpawnResources_91660(townMgr, 0x200,  *reinterpret_cast<int*>(sc + 0x108));
    TownStorage_SpawnResources_91660(townMgr, 0x20,   *reinterpret_cast<int*>(sc + 0x10c));
    TownStorage_SpawnResources_91660(townMgr, 0x40,   *reinterpret_cast<int*>(sc + 0x110));
    TownStorage_SpawnResources_91660(townMgr, 0x4000, *reinterpret_cast<int*>(sc + 0x114));
}

// ============================================================
// Trace dump — town storage state (called from starttown wiring)
// ============================================================
extern "C" void TownStorage_TraceDump() {
    if (!StorageTraceEnabled()) return;
    int64_t* townMgr = reinterpret_cast<int64_t*>(TownMgr());
    if (!townMgr) { STG_TRACE("dump: townMgr NULL"); return; }
    int locCount = static_cast<int>(townMgr[1]);
    int tmplCount = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(townMgr) + 0x118);
    STG_TRACE("dump: storage-locations(+0x00)=%d rawmat-templates(+0x110)=%d",
              locCount, tmplCount);
    for (int i = 0; i < locCount; i++) {
        int64_t entityRef = *reinterpret_cast<int64_t*>(
            townMgr[0] + static_cast<int64_t>(i) * 8);
        CheckPtr(&entityRef);
        void* storage = FindStorageOnEntity(entityRef);
        if (!storage) {
            STG_TRACE("dump: loc[%d] entity=%p (no StorageComponent)", i,
                      reinterpret_cast<void*>(entityRef));
            continue;
        }
        int64_t sc = reinterpret_cast<int64_t>(storage);
        int slots = *reinterpret_cast<int*>(sc + 0x30);
        STG_TRACE("dump: loc[%d] entity=%p storage=%p slots=%d cap(+0x38)=%d "
                  "mask(+0x3c)=0x%x volumeUsed=%d",
                  i, reinterpret_cast<void*>(entityRef), storage, slots,
                  *reinterpret_cast<int*>(sc + 0x38),
                  *reinterpret_cast<uint32_t*>(sc + 0x3c),
                  Seed_QueryCount(sc));
        int64_t arr = *reinterpret_cast<int64_t*>(sc + 0x28);
        for (int s = 0; s < slots; s++) {
            int64_t slot = arr + static_cast<int64_t>(s) * 0x10;
            int64_t mat = *reinterpret_cast<int64_t*>(slot);
            void* desc = FindRawMatDescOnTemplate(mat);
            STG_TRACE("dump:   slot[%d] material=%p flags=0x%x unitVol=%d count=%d reserved=%d",
                      s, reinterpret_cast<void*>(mat),
                      desc ? *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(desc) + 0x5c) : 0,
                      desc ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(desc) + 0x48) : 0,
                      *reinterpret_cast<int*>(slot + 0x08),
                      *reinterpret_cast<int*>(slot + 0x0c));
        }
    }
}
