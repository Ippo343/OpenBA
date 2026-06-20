// code/Game/Component/growthcomponent.cpp
// Clean reimplementation of GrowthComponent::Update from Application-x64.exe.
//
// GrowthComponent manages tree/crop growth with seasonal temperature modulation,
// harvest threshold logic, and sub-resource spawning.
//
// Update address: 0x140120050
// Struct size: 0x50, vtable at 0x140163590
//
// Key logic:
//   1. Compute base growth rate from config (1.0 / (config+0x28 * 60.0))
//   2. Apply seasonal temperature modulation if config+0x2C flag set
//   3. Accumulate growth: resource+0x28 += dt * growthRate * tempModifier
//   4. If growth < 0 and not flagged: reset and harvest
//   5. If growth > threshold (config+0x54): spawn sub-resources on timer
//   6. If growth > growthScale and conditions met: harvest, reset growth
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/growthcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>

// ============================================================
// Resolved helpers
// ============================================================

// Object::InternalBasePtr::CheckPtr
typedef void (*fn_CheckPtr_t)(void* ptrField);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;

// FUN_1400c3470 -- flag notification (notify entity of flag change)
typedef void (*fn_FlagNotify_t)(void* entity);
static fn_FlagNotify_t s_fn_FlagNotify = nullptr;

// FUN_14011fba0 -- spawn sub-resource (growth spawning)
typedef void (*fn_SpawnSubResource_t)(void* growthComp, void* config);
static fn_SpawnSubResource_t s_fn_SpawnSubResource = nullptr;

// FUN_140120470 -- set resource amount / harvest helper
typedef void (*fn_SetResourceAmount_t)(void* growthComp, float amount);
static fn_SetResourceAmount_t s_fn_SetResourceAmount = nullptr;

// FUN_140120410 -- get resource index
typedef int (*fn_GetResourceIndex_t)(void* growthComp);
static fn_GetResourceIndex_t s_fn_GetResourceIndex = nullptr;

// FUN_140126780 -- reset growth state on entity
typedef void (*fn_ResetGrowthState_t)(void* entityPtr);
static fn_ResetGrowthState_t s_fn_ResetGrowthState = nullptr;

// FUN_140118950 -- modify component flags (self, orMask, andNotMask)
typedef void (*fn_ModifyFlags_t)(void* comp, uint32_t orMask, uint32_t andNotMask);
static fn_ModifyFlags_t s_fn_ModifyFlags = nullptr;

// Global: DAT_1401c3dd8 -- game world/simulation singleton pointer
static void** s_pWorldSingleton = nullptr;

// Float constants from .rdata
static float* s_fOne = nullptr;            // DAT_14017214c = 1.0f
static float* s_fSixty = nullptr;          // DAT_140172234 = 60.0f
static float* s_fFreezeTemp = nullptr;     // DAT_1401721d0 = freeze threshold
static float* s_fFreezeGrowth = nullptr;   // DAT_140172288 = growth rate at freeze
// DAT_1401722b0 -- double mask for fabsf (0x7FFFFFFFFFFFFFFF)
static uint64_t* s_dblAbsMask = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    s_fn_CheckPtr = reinterpret_cast<fn_CheckPtr_t>(
        ResolveAddr(Module::Runtime, 0x1800c0290));

    s_fn_FlagNotify = reinterpret_cast<fn_FlagNotify_t>(
        ResolveAddr(Module::Exe, 0x1400c3470));
    s_fn_SpawnSubResource = reinterpret_cast<fn_SpawnSubResource_t>(
        ResolveAddr(Module::Exe, 0x14011fba0));
    s_fn_SetResourceAmount = reinterpret_cast<fn_SetResourceAmount_t>(
        ResolveAddr(Module::Exe, 0x140120470));
    s_fn_GetResourceIndex = reinterpret_cast<fn_GetResourceIndex_t>(
        ResolveAddr(Module::Exe, 0x140120410));
    s_fn_ResetGrowthState = reinterpret_cast<fn_ResetGrowthState_t>(
        ResolveAddr(Module::Exe, 0x140126780));
    s_fn_ModifyFlags = reinterpret_cast<fn_ModifyFlags_t>(
        ResolveAddr(Module::Exe, 0x140118950));

    s_pWorldSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    s_fOne = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x14017214c));
    s_fSixty = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172234));
    s_fFreezeTemp = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x1401721d0));
    s_fFreezeGrowth = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172288));
    s_dblAbsMask = reinterpret_cast<uint64_t*>(ResolveAddr(Module::Exe, 0x1401722b0));

    s_helpersResolved = true;
}

// ============================================================
// Helper: do harvest/reset sequence
// ============================================================
// Shared by both negative-growth and threshold-harvest paths.
static void DoHarvestReset(uint8_t* comp, uintptr_t resourceMgr, float harvestAmount) {
    // Set resource amount
    if (s_fn_SetResourceAmount) {
        s_fn_SetResourceAmount(comp, harvestAmount);
    }

    // Update resource manager entry with owner position
    s_fn_CheckPtr(comp + 0x10);
    uint64_t ownerPos = *reinterpret_cast<uint64_t*>(
        *reinterpret_cast<uintptr_t*>(comp + 0x10) + 0x28);
    int resIdx = 0;
    if (s_fn_GetResourceIndex) {
        resIdx = s_fn_GetResourceIndex(comp);
    }
    uintptr_t resMgrArray = *reinterpret_cast<uintptr_t*>(resourceMgr + 0x80);
    *reinterpret_cast<uint64_t*>(
        *reinterpret_cast<uintptr_t*>(resMgrArray) + 0x10 + (int64_t)resIdx * 0x30) = ownerPos;

    // Reset growth to 0
    s_fn_CheckPtr(comp + 0x28);
    *reinterpret_cast<uint32_t*>(*reinterpret_cast<uintptr_t*>(comp + 0x28) + 0x28) = 0;

    // Clear growth flag byte
    s_fn_CheckPtr(comp + 0x28);
    *reinterpret_cast<uint8_t*>(*reinterpret_cast<uintptr_t*>(comp + 0x28) + 0x2D) = 0;

    // Reset growth state
    s_fn_CheckPtr(comp + 0x28);
    if (s_fn_ResetGrowthState) {
        s_fn_ResetGrowthState(*reinterpret_cast<void**>(comp + 0x28));
    }
}

// ============================================================
// GrowthComponent::Update (vtable slot [9], 0x140120050)
// ============================================================

// Step-2 body-init (vtable slot [4], +0x20, original 0x1401206f0). Forward-declared
// here so the Update guard can lazily invoke it if StartupComponents did not run.
void GrowthComponent_Startup(GrowthComponent* self);

void GrowthComponent_Update(GrowthComponent* self, int64_t* deltaTime) {
    EnsureHelpers();

    uint8_t* comp = reinterpret_cast<uint8_t*>(self);

    // [LIVE-COMPONENT BRING-UP GUARD] On an inert-spawned standalone component the
    // body is zeroed unless Startup ran. config(+0x38) and the resource
    // sub-component(+0x28) are populated by GrowthComponent_Startup (step 2 of the
    // recipe), which the entity-spawn pipeline invokes via vtable+0x20 in
    // ComponentEntity_StartupComponents. As a belt-and-suspenders, if the body is
    // still zero but the description(+0x18) is present, run the body-init now
    // (idempotent). If it is still incomplete afterward, bail safely — everything
    // below derefs config/resource (and .rdata float constants) unconditionally.
    // Harmless in DLL-hook mode (body populated there → both non-null). See
    // notes/STANDALONE_REG_WAVES_PLAN.md "Recipe to bring a component to life".
    if (*(uintptr_t*)(comp + 0x38) == 0 || *(uintptr_t*)(comp + 0x28) == 0) {
        if (*(uintptr_t*)(comp + 0x18) != 0)
            GrowthComponent_Startup(self);
        if (*(uintptr_t*)(comp + 0x38) == 0 || *(uintptr_t*)(comp + 0x28) == 0)
            return;
    }

    float fOne = *s_fOne;       // 1.0f
    float fSixty = *s_fSixty;  // 60.0f

    // config = comp->field38 (component config/description pointer)
    uintptr_t config = *reinterpret_cast<uintptr_t*>(comp + 0x38);

    // Base growth rate: 1.0 / (config+0x28 * 60.0)
    float configGrowthTime = *reinterpret_cast<float*>(config + 0x28);
    float growthRate = fOne / (configGrowthTime * fSixty);

    // Get current growth value from resource component at +0x28
    s_fn_CheckPtr(comp + 0x28);
    uintptr_t resourceComp = *reinterpret_cast<uintptr_t*>(comp + 0x28);
    float currentGrowth = *reinterpret_cast<float*>(resourceComp + 0x28);

    // Apply seasonal temperature modulation
    float tempModifier = fOne;
    if (*reinterpret_cast<char*>(config + 0x2C) != '\0') {
        // Get current temperature from world singleton
        // world->weather(+0x130)->temp(+0x18)
        uintptr_t world = reinterpret_cast<uintptr_t>(*s_pWorldSingleton);
        uintptr_t weatherPtr = *reinterpret_cast<uintptr_t*>(world + 0x130);
        float currentTemp = *reinterpret_cast<float*>(weatherPtr + 0x18);

        // Compute temperature distance from optimal
        float optimalTemp = *reinterpret_cast<float*>(config + 0x30);
        float tempRange = *reinterpret_cast<float*>(config + 0x34);

        // fabsf via double abs with bitmask (matches original bitwise pattern)
        double diff = static_cast<double>(currentTemp - optimalTemp);
        uint64_t rawBits;
        __builtin_memcpy(&rawBits, &diff, sizeof(rawBits));
        rawBits &= *s_dblAbsMask;
        __builtin_memcpy(&diff, &rawBits, sizeof(diff));
        tempModifier = static_cast<float>(diff) / tempRange;

        if (tempModifier < 0.0f || fOne < tempModifier) {
            // Out of range: check freeze
            tempModifier = 0.0f;
            if (currentTemp <= *s_fFreezeTemp) {
                tempModifier = *s_fFreezeGrowth;
            }
        } else {
            // Parabolic falloff: 1 - dist^2
            tempModifier = fOne - tempModifier * tempModifier;
        }
    }

    // Get resource manager from world
    uintptr_t world = reinterpret_cast<uintptr_t>(*s_pWorldSingleton);
    uintptr_t resourceMgr = *reinterpret_cast<uintptr_t*>(world + 0xF8);

    // Compute time delta in seconds
    float dtSeconds = fn_Time_AsSeconds(deltaTime);

    // Accumulate growth
    float newGrowth = currentGrowth + dtSeconds * growthRate * tempModifier;

    // Phase-1 bring-up verification trace (env BANISHED_GROWTH_TRACE, default off
    // → byte-unchanged): follows ONE GrowthComponent across ticks and prints its
    // accumulating growth value (resource+0x28). Confirms the live recipe
    // end-to-end — Update entered via vtable +0x48, body-init populated
    // config@+0x38/resource@+0x28, and the value advances monotonically per tick.
    {
        static int s_traceState = -1;  // -1=unchecked, 0=off, 1=on
        static int s_traceCount = 0;
        static void* s_lockComp = nullptr;  // follow ONE component across ticks
        if (s_traceState < 0) {
            const char* e = std::getenv("BANISHED_GROWTH_TRACE");
            s_traceState = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
        }
        if (s_traceState == 1 && s_lockComp == nullptr) s_lockComp = comp;
        if (s_traceState == 1 && comp == s_lockComp && s_traceCount < 40) {
            fprintf(stderr,
                "[GrowthTrace] comp=%p config=%p res=%p dt=%.5f rate=%.6f temp=%.3f "
                "cur=%.6f -> new=%.6f scale=%.3f\n",
                (void*)comp, (void*)config, (void*)resourceComp, dtSeconds,
                growthRate, tempModifier, currentGrowth, newGrowth,
                *reinterpret_cast<float*>(comp + 0x4c));
            fflush(stderr);
            s_traceCount++;
        }
    }

    if (newGrowth >= 0.0f ||
        (s_fn_CheckPtr(comp + 0x28),
         (*reinterpret_cast<uint32_t*>(*reinterpret_cast<uintptr_t*>(comp + 0x28) + 0x20) >> 0x14 & 1) != 0)) {
        // Growth is positive OR entity has special flag (bit 20)

        // Store new growth value
        s_fn_CheckPtr(comp + 0x28);
        *reinterpret_cast<float*>(*reinterpret_cast<uintptr_t*>(comp + 0x28) + 0x28) = newGrowth;

        // Set flag bit 0x10 (growth active)
        uint32_t oldFlags = *reinterpret_cast<uint32_t*>(comp + 0x20);
        uint32_t newFlags = oldFlags | 0x10;
        *reinterpret_cast<uint32_t*>(comp + 0x20) = newFlags;

        // Notify entity if flags changed
        if (((newFlags ^ oldFlags) & 0xFFFF) != 0 &&
            *reinterpret_cast<void**>(comp + 0x10) != nullptr) {
            s_fn_CheckPtr(comp + 0x10);
            if (s_fn_FlagNotify) {
                s_fn_FlagNotify(*reinterpret_cast<void**>(comp + 0x10));
            }
        }

        // Sub-resource spawning check
        if (*reinterpret_cast<char*>(config + 0x44) != '\0') {
            float spawnThreshold = *reinterpret_cast<float*>(config + 0x54);
            if (spawnThreshold <= newGrowth && newGrowth != spawnThreshold) {
                // Accumulate spawn timer at comp+0x40
                *reinterpret_cast<int64_t*>(comp + 0x40) += *deltaTime;

                float spawnTimerSec = fn_Time_AsSeconds(reinterpret_cast<int64_t*>(comp + 0x40));
                float spawnInterval = static_cast<float>(
                    *reinterpret_cast<int32_t*>(config + 0x50)) * fSixty;

                if (spawnInterval < spawnTimerSec) {
                    do {
                        if (s_fn_SpawnSubResource) {
                            s_fn_SpawnSubResource(comp, reinterpret_cast<void*>(config));
                        }
                        spawnTimerSec -= spawnInterval;
                    } while (spawnInterval < spawnTimerSec);

                    // Reset spawn timer to remainder
                    int64_t remainderTime;
                    fn_Time_Ctor_Float(&remainderTime, spawnTimerSec);
                    *reinterpret_cast<int64_t*>(comp + 0x40) = remainderTime;
                }
            }
        }

        // Harvest check: growthScale threshold
        float growthScale = *reinterpret_cast<float*>(comp + 0x4C);
        if (growthScale > 0.0f && growthScale < newGrowth) {
            // Check entity doesn't have bit 20 set
            s_fn_CheckPtr(comp + 0x28);
            if ((*reinterpret_cast<uint32_t*>(*reinterpret_cast<uintptr_t*>(comp + 0x28) + 0x20) >> 0x14 & 1) == 0) {
                // Check world flag (bit 10 of world+0xD0)
                if ((*reinterpret_cast<uint32_t*>(
                    reinterpret_cast<uintptr_t>(*s_pWorldSingleton) + 0xD0) >> 10 & 1) == 0) {
                    // Clamp growth to 1.0
                    float harvestAmount = (newGrowth <= fOne) ? newGrowth : fOne;

                    // Set resource amount scaled by config+0x40
                    float configResourceScale = *reinterpret_cast<float*>(config + 0x40);
                    DoHarvestReset(comp, resourceMgr, harvestAmount * configResourceScale);

                    // Clear flags bits 3,4 via ModifyFlags(self, 0, 0x18)
                    if (s_fn_ModifyFlags) {
                        s_fn_ModifyFlags(comp, 0, 0x18);
                    }
                }
            }
        }
    } else {
        // Growth went negative -- do full harvest/reset
        float configResourceScale = *reinterpret_cast<float*>(config + 0x40);
        DoHarvestReset(comp, resourceMgr, configResourceScale);

        // Clear flags bits 3,4 directly
        uint32_t oldFlags = *reinterpret_cast<uint32_t*>(comp + 0x20);
        uint32_t newFlags = oldFlags & 0xFFFFFFE7;
        *reinterpret_cast<uint32_t*>(comp + 0x20) = newFlags;

        // Notify entity if flags changed
        if (((newFlags ^ oldFlags) & 0xFFFF) != 0 &&
            *reinterpret_cast<void**>(comp + 0x10) != nullptr) {
            s_fn_CheckPtr(comp + 0x10);
            if (s_fn_FlagNotify) {
                s_fn_FlagNotify(*reinterpret_cast<void**>(comp + 0x10));
            }
        }
    }
}

// ============================================================
// Reimplemented exe helpers (callee bodies for the guarded call sites above)
// ============================================================
//
// These were previously unregistered exe addresses (ResolveAddr -> 0 -> silently
// skipped). Faithful 1:1 ports of the original FUN_* helpers so that the guarded
// calls in GrowthComponent_Update / DoHarvestReset actually fire.

// Resolved sub-helpers used by the bodies below.
// FUN_140004680 -- find NaturalResource entity-data component on owner (returns IBP).
typedef void* (*fn_FindResData_t)(void* owner, void* outIBP);
static fn_FindResData_t s_fn_FindResData = nullptr;
// FUN_140120c30 -- spawn-site validity check (growthComp/config, x, y) -> bool(low byte)
typedef int64_t (*fn_SpawnSiteValid_t)(void* config, int x, int y);
static fn_SpawnSiteValid_t s_fn_SpawnSiteValid = nullptr;
// CharacterComponent_impl_f5d0 (FUN_1400c...f5d0) -- tile placement test.
typedef bool (*fn_TilePlace_t)(void* mapInfo, int x, int y, int kind,
                               int d0, int d1, float f, char c0, char c1, int z);
static fn_TilePlace_t s_fn_TilePlace = nullptr;
// Entity_ClearPathQueue (FUN_140005200) -- IBP accessor (component fetch).
typedef void* (*fn_EntityAccessor_t)(void* entity, void* outIBP);
static fn_EntityAccessor_t s_fn_EntityAccessor = nullptr;
// ComponentEntity copy-ctor (FUN_1400...) -- via engine helper if available.
typedef void* (*fn_CompEntityCopy_t)(void* dst, void* src);
static fn_CompEntityCopy_t s_fn_CompEntityCopy = nullptr;
// FUN_1400052c0 / FUN_14000c460 -- IBP component accessors.
typedef void* (*fn_IBPAccessor_t)(void* ibp, void* outIBP);
static fn_IBPAccessor_t s_fn_IBPAccessor_52c0 = nullptr;
static fn_IBPAccessor_t s_fn_IBPAccessor_c460 = nullptr;
// Entity_impl_6540 (0x140126540) -- place a freshly-spawned sub-resource entity:
// records its tile (+0x28/+0x2a), builds a yaw transform via FromYawPitchOffset and
// writes it to the entity transform (+0x148..+0x168), then Geometry::SetTransform.
// NOTE: the literal was previously the dropped-digit typo 0x140006540 (which lands
// mid-HealerDescription_Factory at 0x1400064f0 — an unrelated tiny factory). The true
// callee in GrowthComponent_helper_fba0 is Entity_impl_6540 @ 0x140126540.
typedef void (*fn_SetTileIndex_t)(void* entity, int tileIdx);
static fn_SetTileIndex_t s_fn_SetTileIndex = nullptr;
// FUN_140120db0 -- set growth value (float) on entity.
typedef void (*fn_SetGrowthVal_t)(void* entity, float v);
static fn_SetGrowthVal_t s_fn_SetGrowthVal = nullptr;
// FUN_140125320 / FUN_1401253d0 -- variant index helpers.
typedef uint32_t (*fn_VariantRange_t)(int64_t a, int b);
static fn_VariantRange_t s_fn_VariantRange = nullptr;
typedef void (*fn_SetVariant_t)(void* a, short b, short c);
static fn_SetVariant_t s_fn_SetVariant = nullptr;
// FUN_1400c48a0 -- register new entity into world entity list.
typedef void (*fn_RegisterEntity_t)(void* entityListPtr, void* ibp, int bucket);
static fn_RegisterEntity_t s_fn_RegisterEntity = nullptr;
// FUN_140003f60 -- find sibling component of type DAT_1401c7a50 on the owner
// entity (the GrowthComponent::Startup +0x30 accessor). Returns IBP via out-param.
typedef void* (*fn_FindSibling_3f60_t)(uintptr_t owner, void* outIBP);
static fn_FindSibling_3f60_t s_fn_FindSibling_3f60 = nullptr;
// System::Random::RandFloatTol (.rdata dispatch slot 0x140151400) -- used only by
// GrowthComponent::Startup for the growthScale randomization (config+0x20 > 0 path).
typedef float (*fn_RandFloatTol_t)(void* random);
static fn_RandFloatTol_t s_fn_RandFloatTol = nullptr;
// AnimateNaturalResource_helper_6440 (FUN_140126440) -- animation kick.
typedef void (*fn_AnimateNatRes_t)(void* comp, int kind);
static fn_AnimateNatRes_t s_fn_AnimateNatRes = nullptr;
// Random on world singleton (+0xd8); world resource manager (+0xf8); world flags (+0xd0).
static float* s_fUVScale = nullptr;       // g_fUVScale = DAT_140172140
static float* s_fInst248 = nullptr;       // DAT_140172248

static bool s_subHelpersResolved = false;
static void EnsureSubHelpers() {
    if (s_subHelpersResolved) return;
    s_fn_FindResData     = reinterpret_cast<fn_FindResData_t>(ResolveAddr(Module::Exe, 0x140004680));
    s_fn_SpawnSiteValid  = reinterpret_cast<fn_SpawnSiteValid_t>(ResolveAddr(Module::Exe, 0x140120c30));
    s_fn_EntityAccessor  = reinterpret_cast<fn_EntityAccessor_t>(ResolveAddr(Module::Exe, 0x140005200));
    s_fn_IBPAccessor_52c0= reinterpret_cast<fn_IBPAccessor_t>(ResolveAddr(Module::Exe, 0x1400052c0));
    s_fn_IBPAccessor_c460= reinterpret_cast<fn_IBPAccessor_t>(ResolveAddr(Module::Exe, 0x14000c460));
    s_fn_SetTileIndex    = reinterpret_cast<fn_SetTileIndex_t>(ResolveAddr(Module::Exe, 0x140126540));
    s_fn_SetGrowthVal    = reinterpret_cast<fn_SetGrowthVal_t>(ResolveAddr(Module::Exe, 0x140120db0));
    s_fn_VariantRange    = reinterpret_cast<fn_VariantRange_t>(ResolveAddr(Module::Exe, 0x140125320));
    s_fn_SetVariant      = reinterpret_cast<fn_SetVariant_t>(ResolveAddr(Module::Exe, 0x1401253d0));
    s_fn_RegisterEntity  = reinterpret_cast<fn_RegisterEntity_t>(ResolveAddr(Module::Exe, 0x1400c48a0));
    s_fn_AnimateNatRes   = reinterpret_cast<fn_AnimateNatRes_t>(ResolveAddr(Module::Exe, 0x140126440));
    s_fn_CompEntityCopy  = reinterpret_cast<fn_CompEntityCopy_t>(ResolveAddr(Module::Exe, 0x1400c3720));
    s_fn_FindSibling_3f60= reinterpret_cast<fn_FindSibling_3f60_t>(ResolveAddr(Module::Exe, 0x140003f60));
    // RandFloatTol lives behind a .rdata dispatch slot (slot storage holds the
    // resolved Runtime fn ptr); deref the slot, matching herdcomponent.cpp.
    uintptr_t randSlot = ResolveAddr(Module::Exe, 0x140151400);
    if (randSlot) s_fn_RandFloatTol = *reinterpret_cast<fn_RandFloatTol_t*>(randSlot);
    s_fUVScale           = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172140));
    s_fInst248           = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172248));
    s_subHelpersResolved = true;
}

static uintptr_t WorldSingleton() {
    return s_pWorldSingleton ? reinterpret_cast<uintptr_t>(*s_pWorldSingleton) : 0;
}

// ============================================================
// GrowthComponent::Startup (vtable slot [4], +0x20, original 0x1401206f0)
// ============================================================
// Body-init: populate the component fields from the owning entity (+0x10) and
// the GrowthDescription template (+0x18). Called at spawn time by
// ComponentEntity_StartupComponents (which dispatches vtable+0x20 on every
// component whose capability flags have bit 7 — GrowthComponent's 0xcb does).
//
//   resource@+0x28 = first sibling component of the natural-resource type
//                    (FUN_1400052c0, by type id DAT_1401c7b30).
//   +0x30          = sibling of type DAT_1401c7a50 (FUN_140003f60).
//   config@+0x38   = the GrowthDescription (comp+0x18). The original validates
//                    IsDerivedFrom(desc, GrowthDescription typeId 0x1401c77e8);
//                    in standalone the factory tree is incomplete so IsDerivedFrom
//                    is unreliable (see ComponentEntity_CtorFromDescription's
//                    IsDerivedFromOrTrust note) — trust comp+0x18, which the
//                    copy-ctor set to this component's own GrowthDescription.
//   growthScale@+0x4c = config+0x20 <= 0 ? -1.0 (never harvest)
//                       : RandFloatTol(rng) * config+0x24 + config+0x20.
void GrowthComponent_Startup(GrowthComponent* self) {
    EnsureHelpers();
    EnsureSubHelpers();
    uint8_t* comp = reinterpret_cast<uint8_t*>(self);

    void* owner = *reinterpret_cast<void**>(comp + 0x10);
    if (!owner) return;   // not yet wired into an entity

    // resource@+0x28 — sibling natural-resource component.
    if (s_fn_CheckPtr) s_fn_CheckPtr(comp + 0x10);
    int64_t resIBP = 0;
    if (s_fn_IBPAccessor_52c0) s_fn_IBPAccessor_52c0(owner, &resIBP);
    *reinterpret_cast<void**>(comp + 0x28) = reinterpret_cast<void*>(resIBP);

    // +0x30 — sibling of type DAT_1401c7a50.
    if (s_fn_CheckPtr) s_fn_CheckPtr(comp + 0x10);
    int64_t res30IBP = 0;
    if (s_fn_FindSibling_3f60)
        s_fn_FindSibling_3f60(reinterpret_cast<uintptr_t>(owner), &res30IBP);
    *reinterpret_cast<void**>(comp + 0x30) = reinterpret_cast<void*>(res30IBP);

    // config@+0x38 — trust the description the copy-ctor stored at +0x18.
    void* config = *reinterpret_cast<void**>(comp + 0x18);
    *reinterpret_cast<void**>(comp + 0x38) = config;

    // growthScale@+0x4c.
    float minScale = config
        ? *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(config) + 0x20)
        : -1.0f;
    if (minScale <= 0.0f) {
        *reinterpret_cast<float*>(comp + 0x4c) = -1.0f;   // 0xbf800000
    } else {
        float range = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(config) + 0x24);
        float r = 0.0f;
        uintptr_t world = WorldSingleton();
        if (s_fn_RandFloatTol && world) {
            void* rng = *reinterpret_cast<void**>(world + 0xd8);
            r = s_fn_RandFloatTol(rng);
        }
        *reinterpret_cast<float*>(comp + 0x4c) = r * range + minScale;
    }
}

// Reimpl of FUN_140118950 (GrowthComponent flag modify: comp+0x20 = (f|or)&~andNot; notify)
void GrowthComponent_ModifyFlags(void* comp, uint32_t orMask, uint32_t andNotMask) {
    EnsureHelpers();
    uint8_t* c = reinterpret_cast<uint8_t*>(comp);
    uint32_t oldF = *reinterpret_cast<uint32_t*>(c + 0x20);
    uint32_t newF = (oldF | orMask) & ~andNotMask;
    *reinterpret_cast<uint32_t*>(c + 0x20) = newF;
    if (((newF ^ oldF) & 0xFFFF) != 0 && *reinterpret_cast<void**>(c + 0x10) != nullptr) {
        if (s_fn_CheckPtr) s_fn_CheckPtr(c + 0x10);
        if (s_fn_FlagNotify) s_fn_FlagNotify(*reinterpret_cast<void**>(c + 0x10));
    }
}

// Reimpl of FUN_140120410 (GrowthComponent get tile index = y*mapW + x from NatResource data)
int GrowthComponent_GetResourceIndex(void* comp) {
    EnsureHelpers(); EnsureSubHelpers();
    uintptr_t world = WorldSingleton();
    if (!world) return 0;
    int* mapW = *reinterpret_cast<int**>(world + 0xF8);
    uint8_t* c = reinterpret_cast<uint8_t*>(comp);
    if (s_fn_CheckPtr) s_fn_CheckPtr(c + 0x10);
    int64_t resIBP = 0;
    if (s_fn_FindResData) s_fn_FindResData(*reinterpret_cast<void**>(c + 0x10), &resIBP);
    if (s_fn_CheckPtr) s_fn_CheckPtr(&resIBP);
    int16_t x = *reinterpret_cast<int16_t*>(resIBP + 0x28);
    int16_t y = *reinterpret_cast<int16_t*>(resIBP + 0x2a);
    return (int)y * (*mapW) + (int)x;
}

// Reimpl of FUN_140120470 (GrowthComponent set per-tile growth byte at grid +0x2c)
void GrowthComponent_SetResourceAmount(void* comp, float amount) {
    EnsureHelpers(); EnsureSubHelpers();
    uintptr_t world = WorldSingleton();
    if (!world) return;
    int* mapW = *reinterpret_cast<int**>(world + 0xF8);
    uint8_t* c = reinterpret_cast<uint8_t*>(comp);
    if (s_fn_CheckPtr) s_fn_CheckPtr(c + 0x10);
    int64_t resIBP = 0;
    if (s_fn_FindResData) s_fn_FindResData(*reinterpret_cast<void**>(c + 0x10), &resIBP);
    if (s_fn_CheckPtr) s_fn_CheckPtr(&resIBP);
    uintptr_t grid = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(mapW) + 0x80);
    int16_t x = *reinterpret_cast<int16_t*>(resIBP + 0x28);
    int16_t y = *reinterpret_cast<int16_t*>(resIBP + 0x2a);
    int64_t cell = (int64_t)((int)x + (int)y * (*mapW));
    // DAT_14017222c is the byte<->float scale (255.0)
    float* pScale = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x14017222c));
    float divisor = pScale ? *pScale : 255.0f;
    uint8_t* cellByte = reinterpret_cast<uint8_t*>(grid + 0x2c + cell * 0x30);
    float fv = (float)(*cellByte) / divisor;
    if (0.0f <= amount) fv = fv * fv * amount;
    else fv = fv - amount;
    int iv = (int)(fv * divisor);
    if (iv < 0) { *cellByte = 0; return; }
    *cellByte = (iv > 0xFF) ? 0xFF : (uint8_t)iv;
}

// Reimpl of FUN_140126780 (GrowthComponent reset growth: set bit 0x200000, notify, animate)
void GrowthComponent_ResetGrowthState(void* entityPtr) {
    EnsureHelpers(); EnsureSubHelpers();
    uint8_t* c = reinterpret_cast<uint8_t*>(entityPtr);
    uint32_t oldF = *reinterpret_cast<uint32_t*>(c + 0x20);
    uint32_t newF = oldF | 0x200000;
    *reinterpret_cast<uint32_t*>(c + 0x20) = newF;
    if (((newF ^ oldF) & 0xFFFF) != 0 && *reinterpret_cast<void**>(c + 0x10) != nullptr) {
        if (s_fn_CheckPtr) s_fn_CheckPtr(c + 0x10);
        if (s_fn_FlagNotify) s_fn_FlagNotify(*reinterpret_cast<void**>(c + 0x10));
    }
    if (s_fn_AnimateNatRes) s_fn_AnimateNatRes(c, 2);
}

// Reimpl of FUN_14011fba0 (GrowthComponent spawn sub-resource onto a nearby tile)
void GrowthComponent_SpawnSubResource(void* growthComp, void* config) {
    EnsureHelpers(); EnsureSubHelpers();
    uintptr_t world = WorldSingleton();
    if (!world) return;
    uint8_t* cfg = reinterpret_cast<uint8_t*>(config);
    int* mapW = *reinterpret_cast<int**>(world + 0xF8);
    void* rng = *reinterpret_cast<void**>(world + 0xD8);
    // world flag bit 10 must be clear
    if ((*reinterpret_cast<uint32_t*>(world + 0xD0) >> 10 & 1) != 0) return;

    float roll = fn_Random_RandFloat ? fn_Random_RandFloat(rng) : 1.0f;
    float uvScale = s_fUVScale ? *s_fUVScale : 1.0f;
    int spawnChanceDenom = *reinterpret_cast<int*>(cfg + 0x48);
    if (spawnChanceDenom == 0) return;
    if (roll > uvScale / (float)spawnChanceDenom) return;

    uint8_t* gc = reinterpret_cast<uint8_t*>(growthComp);
    void* self10 = gc + 0x10;
    if (s_fn_CheckPtr) s_fn_CheckPtr(self10);
    int64_t resIBP = 0;
    if (s_fn_FindResData) s_fn_FindResData(*reinterpret_cast<void**>(gc + 0x10), &resIBP);
    if (s_fn_CheckPtr) s_fn_CheckPtr(&resIBP);
    int16_t baseX = *reinterpret_cast<int16_t*>(resIBP + 0x28);
    int16_t baseY = *reinterpret_cast<int16_t*>(resIBP + 0x2a);

    int tol = *reinterpret_cast<int*>(cfg + 0x4c);
    int x = (int)baseX + (fn_Random_RandTol ? fn_Random_RandTol(rng, tol) : 0);
    int y = (int)baseY + (fn_Random_RandTol ? fn_Random_RandTol(rng, tol) : 0);
    if (!(x >= 0 && y >= 0 && x < *mapW && y < *mapW)) return;

    float inst248 = s_fInst248 ? *s_fInst248 : 0.0f;
    float inst140 = s_fUVScale ? *s_fUVScale : 0.0f;
    bool placed = s_fn_TilePlace
        ? s_fn_TilePlace(mapW, x, y, 4, (int)inst140, (int)inst248, 0.0f, '\0', '\0', 0)
        : false;
    if (!placed) return;

    if (!s_fn_SpawnSiteValid) return;
    int64_t valid = s_fn_SpawnSiteValid(config, x, y);
    if ((char)valid == '\0') return;

    int mapWv = *mapW;
    if (s_fn_CheckPtr) s_fn_CheckPtr(self10);
    int64_t parentIBP = 0;
    if (s_fn_EntityAccessor) s_fn_EntityAccessor(*reinterpret_cast<void**>(gc + 0x10), &parentIBP);
    if (s_fn_CheckPtr) s_fn_CheckPtr(&parentIBP);
    int16_t variantSeed = *reinterpret_cast<int16_t*>(*reinterpret_cast<uintptr_t*>(&parentIBP) + 0x164);

    // Allocate + copy-construct the new ComponentEntity from this entity's description.
    void* mem = (fn_MemAlloc && g_pMemAllocSingleton)
                    ? fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8) : nullptr;
    void* newEnt = nullptr;
    if (mem) {
        if (s_fn_CheckPtr) s_fn_CheckPtr(self10);
        void* srcDesc = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(gc + 0x10) + 0x28);
        if (s_fn_CompEntityCopy) newEnt = s_fn_CompEntityCopy(mem, srcDesc);
        else newEnt = mem;
    }
    int64_t newIBP = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&newIBP, newEnt);

    if (s_fn_CheckPtr) s_fn_CheckPtr(&newIBP);
    int64_t acc1 = 0;
    if (s_fn_IBPAccessor_52c0) s_fn_IBPAccessor_52c0(reinterpret_cast<void*>(newIBP), &acc1);
    if (s_fn_CheckPtr) s_fn_CheckPtr(&acc1);
    if (s_fn_SetTileIndex) s_fn_SetTileIndex(*reinterpret_cast<void**>(&acc1), mapWv * y + x);

    if (s_fn_CheckPtr) s_fn_CheckPtr(&newIBP);
    int64_t acc2 = 0;
    if (s_fn_IBPAccessor_c460) s_fn_IBPAccessor_c460(reinterpret_cast<void*>(newIBP), &acc2);
    if (s_fn_CheckPtr) s_fn_CheckPtr(&acc2);
    if (s_fn_SetGrowthVal) s_fn_SetGrowthVal(*reinterpret_cast<void**>(&acc2), 0.0f);

    if (s_fn_CheckPtr) s_fn_CheckPtr(&newIBP);
    int64_t acc3 = 0;
    if (s_fn_EntityAccessor) s_fn_EntityAccessor(reinterpret_cast<void*>(newIBP), &acc3);
    if (s_fn_CheckPtr) s_fn_CheckPtr(&acc3);
    int64_t variantTarget = *reinterpret_cast<int64_t*>(&acc3);

    if (s_fn_CheckPtr) s_fn_CheckPtr(&newIBP);
    int64_t acc4 = 0;
    if (s_fn_EntityAccessor) s_fn_EntityAccessor(reinterpret_cast<void*>(newIBP), &acc4);
    if (s_fn_CheckPtr) s_fn_CheckPtr(&acc4);
    void* variantWrite = *reinterpret_cast<void**>(&acc4);

    uint32_t range = s_fn_VariantRange ? s_fn_VariantRange(variantTarget, (int)variantSeed) : 0;
    uint32_t roll2 = 0;
    // System::Random::Rand(this, range) — resolve once.
    typedef uint32_t (*fn_Rand_t)(void* self, uint32_t n);
    static fn_Rand_t s_fn_Rand = reinterpret_cast<fn_Rand_t>(ResolveAddr(Module::Runtime, 0x1800c7cf0));
    if (s_fn_Rand) roll2 = s_fn_Rand(rng, range);
    if (s_fn_SetVariant) s_fn_SetVariant(variantWrite, variantSeed, (short)roll2);

    if (s_fn_CheckPtr) s_fn_CheckPtr(self10);
    int64_t regArg = newIBP;
    if (s_fn_RegisterEntity) {
        s_fn_RegisterEntity(*reinterpret_cast<void**>(world + 0xE8), &regArg,
            *reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(gc + 0x10) + 0x28 + 0x20));
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void GrowthComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[GrowthComponent] Funcs resolved: CheckPtr=%p FlagNotify=%p "
           "SpawnSub=%p SetResAmt=%p GetResIdx=%p\n",
           (void*)s_fn_CheckPtr, (void*)s_fn_FlagNotify,
           (void*)s_fn_SpawnSubResource, (void*)s_fn_SetResourceAmount,
           (void*)s_fn_GetResourceIndex);
    printf("[GrowthComponent]   WorldSingleton=%p -> %p\n",
           (void*)s_pWorldSingleton,
           s_pWorldSingleton ? *s_pWorldSingleton : nullptr);
#endif
}

// ===== Standalone factory =====
// WORKED EXAMPLE of the inert->live component bring-up (Track C in
// notes/STANDALONE_REG_WAVES_PLAN.md). By DEFAULT GrowthComponent is inert exactly
// like every other standalone component (all 48 vtable slots are no-op stubs except
// +0x00 dtor / +0x10 GetTypeId), so the default run is byte-for-byte unchanged.
//
// Set BANISHED_LIVE_GROWTH=1 to graduate it to a REAL per-type vtable whose per-tick
// Update slot — vtable +0x48 (slot 9), the slot the EntityList bucket round-robin
// (entitylist.cpp EntityList_Update Step 4) actually calls — routes to the real
// GrowthComponent_Update. (NOT +0x40/slot 8: that's the empty *_OnRemove, which is
// the slot the tree-walk Step 3 calls — verified vs Ghidra vtable 0x140163590:
// +0x40=0x140118a70 empty, +0x48=0x140120050 the growth Update.)
//
// STATUS — this implements STEP 1 (vtable routing) of the 4-step recipe only.
// Steps still TODO for actual growth simulation:
//   2. Real Startup(+0x10)/Serialize(+0x20) to populate the body from the
//      Description (config@+0x38, resource sub-component@+0x28, growthScale@+0x4c).
//      Until then the LIVE-COMPONENT BRING-UP GUARD in GrowthComponent_Update
//      early-returns (body is zeroed), so flipping the env var is crash-safe.
//   3. Register the exe helper addrs GrowthComponent_Update/_SpawnSubResource call
//      (EnsureHelpers/EnsureSubHelpers lists) in standalone/address_registry.cpp.
//   4. Confirm GrowthComponent entities land in an update bucket with entity flag
//      bit 3 set (capability flags 0xcb already have component bit 3) + processCount>0.
// VERIFY (next session): BANISHED_LIVE_GROWTH=1, breakpoint GrowthComponent_Update,
// confirm it is ENTERED each tick via the +0x48 slot (proves routing); then do step 2.
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
#include <cstdlib>
#include <cstring>

static uint32_t GrowthComponent_GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("GrowthComponent");
    return cached;
}
static void* GrowthComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

// vtable slot +0x48 (slot 9) is invoked as (component, const Time* delta).
static void GrowthComponent_UpdateVSlot(void* self, void* deltaTime) {
    GrowthComponent_Update(reinterpret_cast<GrowthComponent*>(self),
                           reinterpret_cast<int64_t*>(deltaTime));
}

// vtable slot +0x20 (slot 4 = Startup) is invoked as (component). Called by
// ComponentEntity_StartupComponents at spawn to populate the body (step 2).
static void GrowthComponent_StartupVSlot(void* self) {
    GrowthComponent_Startup(reinterpret_cast<GrowthComponent*>(self));
}

static bool GrowthComponent_LiveEnabled() {
    const char* e = std::getenv("BANISHED_LIVE_GROWTH");
    return e && e[0] && std::strcmp(e, "0") != 0 && std::strcmp(e, "false") != 0;
}

// Called after InitInertTyped has filled the inert stubs + real GetTypeId. When the
// env opt-in is set, graduate slot 9 (+0x48 Update) to the real reimpl. Idempotent:
// the shared per-type vtable is filled once, this re-points slot 9 to the same fn.
static void GrowthComponent_FinishVtable() {
    if (GrowthComponent_LiveEnabled()) {
        // slot 4 (+0x20) = Startup (step-2 body-init), slot 9 (+0x48) = Update.
        GrowthComponent_LiveVtable[4] =
            reinterpret_cast<void*>(&GrowthComponent_StartupVSlot);
        GrowthComponent_LiveVtable[9] =
            reinterpret_cast<void*>(&GrowthComponent_UpdateVSlot);
    }
}

extern "C" void* GrowthComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x50, 0xcb,
        GrowthComponent_LiveVtable,
        reinterpret_cast<void*>(&GrowthComponent_GetTypeId));
    GrowthComponent_FinishVtable();
    return p;
}
static void* GrowthComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x50, 0xcb,
        GrowthComponent_LiveVtable,
        reinterpret_cast<void*>(&GrowthComponent_GetTypeId));
    GrowthComponent_FinishVtable();
    return mem;
}
REGISTER_TYPE(GrowthComponent, "Component",
              &GrowthComponent_Factory, &GrowthComponent_CreateInPlace);
#endif // STANDALONE_EXE
