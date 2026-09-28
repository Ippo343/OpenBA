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

#include "Component/growthcomponent_funcs.h"
#include "Component/ModelComponent.h"   // B.3.8 CB1a (TypeId)
#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (standalone sibling-id fallback)
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

// ============================================================
// Resolved helpers
// ============================================================

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

// Visual-scale smoothing constants used by slot 10 (0x14011fea0), read from .rdata so
// they stay tied to the binary rather than being re-typed here.
static float* s_fScaleEps      = nullptr;  // DAT_1401720b8 = 0.001f  (snap threshold)
static float* s_fScaleOldW     = nullptr;  // DAT_140172144 = 0.99f   (previous weight)
static float* s_fScaleNewW     = nullptr;  // DAT_1401720c0 = 0.01f   (target weight)
static float* s_fCropSpreadDiv = nullptr;  // DAT_1401720f4 = 0.15f   (crop x/y ramp)

// Math::Vector4::Vector4(const Scalar&) via .rdata dispatch slot 0x140152958 — broadcasts
// one float into all four lanes.
typedef void (*fn_Vec4FromScalar_t)(void* out16, const float* scalar);
static fn_Vec4FromScalar_t s_fn_Vec4FromScalar = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

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

    s_fScaleEps      = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x1401720b8));
    s_fScaleOldW     = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172144));
    s_fScaleNewW     = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x1401720c0));
    s_fCropSpreadDiv = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x1401720f4));

    {
        uintptr_t slot = ResolveAddr(Module::Exe, 0x140152958);
        if (slot) s_fn_Vec4FromScalar = *reinterpret_cast<fn_Vec4FromScalar_t*>(slot);
    }

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
    uint64_t ownerPos = *reinterpret_cast<uint64_t*>(
        *reinterpret_cast<uintptr_t*>(comp + 0x10) + 0x28);
    int resIdx = 0;
    if (s_fn_GetResourceIndex) {
        resIdx = s_fn_GetResourceIndex(comp);
    }
    // grid base = *(resourceMgr+0x80) = the Map cell grid (game+0x80, built in
    // standalone by MapTileGrid_Build: dim*dim records, stride 0x30). Write the
    // owner position 8-byte handle into this tile's record at +0x10. The original
    // (GrowthComponent::Update 0x140120050) is:
    //   *(*(resourceMgr+0x80) + 0x10 + resIdx*0x30) = ownerPos
    // i.e. ONE deref of resourceMgr+0x80, then index by resIdx*0x30. A prior port
    // had an extra deref (*(uintptr_t*)gridBase), which read the zeroed first qword
    // of tile-record 0 and wrote to ~0x10 → access violation (misattributed to a
    // "missing resourceMgr grid"; the grid exists — see maptilegrid.cpp).
    uintptr_t gridBase = *reinterpret_cast<uintptr_t*>(resourceMgr + 0x80);
    *reinterpret_cast<uint64_t*>(
        gridBase + 0x10 + (int64_t)resIdx * 0x30) = ownerPos;

    // Reset growth to 0
    *reinterpret_cast<uint32_t*>(*reinterpret_cast<uintptr_t*>(comp + 0x28) + 0x28) = 0;

    // Clear growth flag byte
    *reinterpret_cast<uint8_t*>(*reinterpret_cast<uintptr_t*>(comp + 0x28) + 0x2D) = 0;

    // Reset growth state
    if (s_fn_ResetGrowthState) {
        s_fn_ResetGrowthState(*reinterpret_cast<void**>(comp + 0x28));
    }
}

// ============================================================
// GrowthComponent::Update (vtable slot [9], 0x140120050)
// ============================================================

// Step-2 body-init (vtable slot [4], +0x20, original 0x1401206f0). Forward-declared
// here so the Update guard can lazily invoke it if StartupComponents did not run.

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
    s_fUVScale           = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172140));
    s_fInst248           = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172248));
    s_subHelpersResolved = true;
}

static uintptr_t WorldSingleton() {
    return s_pWorldSingleton ? reinterpret_cast<uintptr_t>(*s_pWorldSingleton) : 0;
}

// Reimpl of FUN_140118950 (GrowthComponent flag modify: comp+0x20 = (f|or)&~andNot; notify)
void GrowthComponent_ModifyFlags(void* comp, uint32_t orMask, uint32_t andNotMask) {
    EnsureHelpers();
    uint8_t* c = reinterpret_cast<uint8_t*>(comp);
    uint32_t oldF = *reinterpret_cast<uint32_t*>(c + 0x20);
    uint32_t newF = (oldF | orMask) & ~andNotMask;
    *reinterpret_cast<uint32_t*>(c + 0x20) = newF;
    if (((newF ^ oldF) & 0xFFFF) != 0 && *reinterpret_cast<void**>(c + 0x10) != nullptr) {
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
    int64_t resIBP = 0;
    if (s_fn_FindResData) s_fn_FindResData(*reinterpret_cast<void**>(c + 0x10), &resIBP);
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
    int64_t resIBP = 0;
    if (s_fn_FindResData) s_fn_FindResData(*reinterpret_cast<void**>(c + 0x10), &resIBP);
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

    float roll = reinterpret_cast<System::Random*>(rng)->RandFloat();   // B.3.4b bridge
    float uvScale = s_fUVScale ? *s_fUVScale : 1.0f;
    int spawnChanceDenom = *reinterpret_cast<int*>(cfg + 0x48);
    if (spawnChanceDenom == 0) return;
    if (roll > uvScale / (float)spawnChanceDenom) return;

    uint8_t* gc = reinterpret_cast<uint8_t*>(growthComp);
    int64_t resIBP = 0;
    if (s_fn_FindResData) s_fn_FindResData(*reinterpret_cast<void**>(gc + 0x10), &resIBP);
    int16_t baseX = *reinterpret_cast<int16_t*>(resIBP + 0x28);
    int16_t baseY = *reinterpret_cast<int16_t*>(resIBP + 0x2a);

    int tol = *reinterpret_cast<int*>(cfg + 0x4c);
    int x = (int)baseX + reinterpret_cast<System::Random*>(rng)->RandTol(static_cast<int32_t>(tol));   // B.3.4b bridge
    int y = (int)baseY + reinterpret_cast<System::Random*>(rng)->RandTol(static_cast<int32_t>(tol));   // B.3.4b bridge
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
    int64_t parentIBP = 0;
    if (s_fn_EntityAccessor) s_fn_EntityAccessor(*reinterpret_cast<void**>(gc + 0x10), &parentIBP);
    int16_t variantSeed = *reinterpret_cast<int16_t*>(*reinterpret_cast<uintptr_t*>(&parentIBP) + 0x164);

    // Allocate + copy-construct the new ComponentEntity from this entity's description.
    void* mem = (fn_MemAlloc && g_pMemAllocSingleton)
                    ? fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8) : nullptr;
    void* newEnt = nullptr;
    if (mem) {
        void* srcDesc = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(gc + 0x10) + 0x28);
        if (s_fn_CompEntityCopy) newEnt = s_fn_CompEntityCopy(mem, srcDesc);
        else newEnt = mem;
    }
    int64_t newIBP = 0;
    if (fn_InternalBasePtr_Ctor) new (&newIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(newEnt));   // B.3.3 bridge

    int64_t acc1 = 0;
    if (s_fn_IBPAccessor_52c0) s_fn_IBPAccessor_52c0(reinterpret_cast<void*>(newIBP), &acc1);
    if (s_fn_SetTileIndex) s_fn_SetTileIndex(*reinterpret_cast<void**>(&acc1), mapWv * y + x);

    int64_t acc2 = 0;
    if (s_fn_IBPAccessor_c460) s_fn_IBPAccessor_c460(reinterpret_cast<void*>(newIBP), &acc2);
    if (s_fn_SetGrowthVal) s_fn_SetGrowthVal(*reinterpret_cast<void**>(&acc2), 0.0f);

    int64_t acc3 = 0;
    if (s_fn_EntityAccessor) s_fn_EntityAccessor(reinterpret_cast<void*>(newIBP), &acc3);
    int64_t variantTarget = *reinterpret_cast<int64_t*>(&acc3);

    int64_t acc4 = 0;
    if (s_fn_EntityAccessor) s_fn_EntityAccessor(reinterpret_cast<void*>(newIBP), &acc4);
    void* variantWrite = *reinterpret_cast<void**>(&acc4);

    uint32_t range = s_fn_VariantRange ? s_fn_VariantRange(variantTarget, (int)variantSeed) : 0;
    // Retail's call site (FUN_14011fba0 at 0x14011fe2c) is `CALL [0x1401512e8]`,
    // the import slot for ?Rand@Random@System@@QEAAII@Z — the uint overload.
    uint32_t roll2 = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(range));   // B.3.4b bridge
    if (s_fn_SetVariant) s_fn_SetVariant(variantWrite, variantSeed, (short)roll2);

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
    printf("[GrowthComponent] Funcs resolved: FlagNotify=%p "
           "SpawnSub=%p SetResAmt=%p GetResIdx=%p\n",
           (void*)s_fn_FlagNotify,
           (void*)s_fn_SpawnSubResource, (void*)s_fn_SetResourceAmount,
           (void*)s_fn_GetResourceIndex);
    printf("[GrowthComponent]   WorldSingleton=%p -> %p\n",
           (void*)s_pWorldSingleton,
           s_pWorldSingleton ? *s_pWorldSingleton : nullptr);
#endif
}

// ===== ENDGAME B.3.8 CIT — GrowthComponent as a REAL class (GrowthComponent.h) =====
// Before: a 48-slot hand-built GrowthComponent_LiveVtable filled at every create
// (slots 3 Serialize, 4 Startup, 9 Update, 10 PostUpdate), Inert_ScalarDtor in
// slot 0. After: the compiler's 16-slot vtable and a real ctor/dtor chain
// (B38_RULES D1, D2).
#include "Object/factory_registration.h"
#include "Component/GrowthComponent.h"


// 0x1401208f0. Component() is the CALL 0x1401188b0; the 0x140163590 stamp is
// the compiler's; +0x28..+0x40 = 0; +0x48 (the smoothed displayed scale) =
// +0x4c (growthScale) = 1.0f (0x14012092d / 0x140120934); flags |= 0xcb.
GrowthComponent::GrowthComponent()
    : field28(nullptr), field30(nullptr), field38(nullptr), field40(nullptr),
      growthRate(1.0f), growthScale(1.0f) {
    OrFlagsInCtor(0xcb);
}

// [0] 0x140034c50: restamp + ~Internal (no own work), MemFree on &1.
Object::Base* GrowthComponent::DeletingDtor(unsigned flags) {
    this->~GrowthComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
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
void GrowthComponent::Startup() {
    EnsureHelpers();
    EnsureSubHelpers();
    uint8_t* comp = reinterpret_cast<uint8_t*>(this);

    void* owner = *reinterpret_cast<void**>(comp + 0x10);
    if (!owner) return;   // not yet wired into an entity

    // resource@+0x28 — sibling natural-resource component.
    int64_t resIBP = 0;
    if (s_fn_IBPAccessor_52c0) s_fn_IBPAccessor_52c0(owner, &resIBP);
    *reinterpret_cast<void**>(comp + 0x28) = reinterpret_cast<void*>(resIBP);

    // +0x30 — sibling ModelComponent (type DAT_1401c7a50), used by FUN_140120db0 to
    // write the growth-staged mesh scale. The shared FUN_140003f60 reimpl finds it via
    // the raw .rdata ModelComponent type-id (0x1401c7a50), which resolves NULL in
    // standalone → it returned null, so +0x30 never linked and tree growth-staging was a
    // no-op (BANISHED_MATURE_TREES stood in). Prefer the shared finder; if it yields
    // null, fall back to a
    // SELF-CONTAINED ModelComponent scan via GetTypeIdByName. Scoped here so it cannot
    // change the shared produce/animation callers of 3f60 (verified paths).
    int64_t res30IBP = 0;
    if (s_fn_FindSibling_3f60)
        s_fn_FindSibling_3f60(reinterpret_cast<uintptr_t>(owner), &res30IBP);
    if (res30IBP == 0 && fn_InternalBasePtr_Ctor) {
        const uint32_t s_tidModel = ModelComponent::TypeId();   // B.3.8: was a lazy by-name cache (B6-5)
        if (s_tidModel) {
            int n = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(owner) + 0x38);
            uintptr_t arr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uint8_t*>(owner) + 0x30);
            for (int i = 0; i < n && arr; ++i) {
                void** slot = reinterpret_cast<void**>(arr + static_cast<int64_t>(i) * 8);
                void* c = *slot;
                if (!c) continue;
                uint32_t t = reinterpret_cast<Object::Base*>(c)->Type();   // B.3.8 bridge (slot 2)
                if (t == s_tidModel) { new (&res30IBP) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>(c)); break; }   // B.3.3 bridge
            }
        }
    }
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
        if (world) {
            void* rng = *reinterpret_cast<void**>(world + 0xd8);
            r = reinterpret_cast<System::Random*>(rng)->RandFloatTol();   // B.3.4b bridge
        }
        *reinterpret_cast<float*>(comp + 0x4c) = r * range + minScale;
    }
}
void GrowthComponent::OnAdded() {}                                  // [6] 0x140120690: retail's own `RET`

// [7] 0x140120560 (283 B). PORT GAP (retail 0x140120560) — B38 D9: not ported;
// the tree's table held the inert stub here, kept.
void GrowthComponent::OnRemove() {}

// [9] 0x140120050. RDX is a pointer to the elapsed System::Time (EntityList
// bucket tick 0x1400c42af).
void GrowthComponent::Update(void* time) {
    int64_t* deltaTime = static_cast<int64_t*>(time);
    EnsureHelpers();

    uint8_t* comp = reinterpret_cast<uint8_t*>(this);

    // [LIVE-COMPONENT BRING-UP GUARD] On an inert-spawned standalone component the
    // body is zeroed unless Startup ran. config(+0x38) and the resource
    // sub-component(+0x28) are populated by GrowthComponent_Startup (step 2 of the
    // recipe), which the entity-spawn pipeline invokes via vtable+0x20 in
    // ComponentEntity_StartupComponents. As a belt-and-suspenders, if the body is
    // still zero but the description(+0x18) is present, run the body-init now
    // (idempotent). If it is still incomplete afterward, bail safely — everything
    // below derefs config/resource (and .rdata float constants) unconditionally.
    // See notes/STANDALONE_REG_WAVES_PLAN.md "Recipe to bring a component to life".
    //
    // ⚠ A STREAM-RESTORED component must NOT take the full Startup path. Retail's
    // GrowthComponent::Serialize (0x1401207c0) persists exactly +0x28 (resource IBP),
    // +0x30 (model IBP), +0x48 (displayed scale), +0x4c (growthScale) and +0x40 (timer)
    // — everything Startup computes EXCEPT +0x38, which is just a copy of the
    // description already sitting at +0x18. So after a load the one genuinely missing
    // field is +0x38, and running the whole body-init to get it RE-ROLLS +0x4c from the
    // RNG (see GrowthComponent_Startup), throwing away the per-instance harvest
    // threshold the save restored. That is a silent load-fidelity bug: the plant keeps
    // its growth but changes when it becomes harvestable, differently on every load.
    //
    // A non-null +0x28 is the tell: only the stream puts a resource IBP there before
    // Startup has ever run. Fill +0x38 and leave every restored field alone.
    if (*(uintptr_t*)(comp + 0x38) == 0 || *(uintptr_t*)(comp + 0x28) == 0) {
        if (*(uintptr_t*)(comp + 0x38) == 0 && *(uintptr_t*)(comp + 0x28) != 0) {
            *(uintptr_t*)(comp + 0x38) = *(uintptr_t*)(comp + 0x18);
        } else if (*(uintptr_t*)(comp + 0x18) != 0) {
            GrowthComponent::Startup();
        }
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
    float dtSeconds = reinterpret_cast<const System::Time*>(deltaTime)->AsSeconds();   // B.3.4b bridge

    // Accumulate growth
    float newGrowth = currentGrowth + dtSeconds * growthRate * tempModifier;

    if (newGrowth >= 0.0f ||
        ((*reinterpret_cast<uint32_t*>(*reinterpret_cast<uintptr_t*>(comp + 0x28) + 0x20) >> 0x14 & 1) != 0)) {
        // Growth is positive OR entity has special flag (bit 20)

        // Store new growth value
        *reinterpret_cast<float*>(*reinterpret_cast<uintptr_t*>(comp + 0x28) + 0x28) = newGrowth;

        // Set flag bit 0x10 (growth active)
        uint32_t oldFlags = *reinterpret_cast<uint32_t*>(comp + 0x20);
        uint32_t newFlags = oldFlags | 0x10;
        *reinterpret_cast<uint32_t*>(comp + 0x20) = newFlags;

        // Notify entity if flags changed
        if (((newFlags ^ oldFlags) & 0xFFFF) != 0 &&
            *reinterpret_cast<void**>(comp + 0x10) != nullptr) {
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

                float spawnTimerSec = reinterpret_cast<const System::Time*>(reinterpret_cast<int64_t*>(comp + 0x40))->AsSeconds();   // B.3.4b bridge
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
            if ((*reinterpret_cast<uint32_t*>(*reinterpret_cast<uintptr_t*>(comp + 0x28) + 0x20) >> 0x14 & 1) == 0) {
                // Check world flag (bit 10 of world+0xD0)
                if ((*reinterpret_cast<uint32_t*>(
                    reinterpret_cast<uintptr_t>(*s_pWorldSingleton) + 0xD0) >> 10 & 1) == 0) {
                    // Clamp growth to 1.0
                    float harvestAmount = (newGrowth <= fOne) ? newGrowth : fOne;

                    // Set resource amount scaled by config+0x40
                    float configResourceScale = *reinterpret_cast<float*>(config + 0x40);
                    DoHarvestReset(comp, resourceMgr, harvestAmount * configResourceScale);

                    // Clear flags bits 3,4 via ModifyFlags(this, 0, 0x18)
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
            if (s_fn_FlagNotify) {
                s_fn_FlagNotify(*reinterpret_cast<void**>(comp + 0x10));
            }
        }
    }
}

// [10] 0x14011fea0 — dispatched by EntityList::PostUpdateNodeHandler (flags bit
// 4): ease the model's visual scale toward the accumulated growth.
// ============================================================
// GrowthComponent::ApplyVisualScale (vtable slot [10], +0x50, original 0x14011fea0)
// ============================================================
// THE per-frame link between a resource's growth value and how big it is drawn.
// EntityList::PostUpdateNodeHandler (0x1400c2c70) dispatches component slot 10 on every
// component whose flags carry bit 4, and GrowthComponent::Update is what sets that bit
// each time growth advances. Without this body the bit was set, the dispatch happened,
// and the inert stub returned — so the MeshInstance visual scale stayed at the engine
// ctor default of (1,1,1,1) and EVERY tree/crop rendered full-grown from the frame it
// was planted. Measured before the fix: a just-spawned crop at growth 0.000 already had
// visualScale=(1,1,1,1) with a mesh bound and a live scene id.
//
// Shape (1:1 with the disassembly):
//   target = min(resource+0x28 growth, 1.0), then raised to the description floor
//            (desc+0x60) so a resource never draws smaller than its minimum.
//   self+0x48 is the SMOOTHED displayed scale: snap when already within 0.001, else
//            ease 1% per tick (0.99*cur + 0.01*target) — this is what makes a tree grow
//            visibly rather than pop between sizes.
//   desc+0x64 selects the CROP form: x/y spread to full within the first 15% of growth
//            (0x1401720f4 = 0.15f) while z carries the height, versus the uniform
//            Vector4-broadcast form used by trees and every other natural resource.
//   The flag-4 clear only happens once the scale has CONVERGED, which is what keeps the
//   component in the post-update list across the whole ramp.
void GrowthComponent::PostUpdate() {
    EnsureHelpers();
    uintptr_t c = reinterpret_cast<uintptr_t>(this);

    // Retail derefs +0x28/+0x38/+0x30 unguarded — they are populated by
    // GrowthComponent::Startup, which runs on every spawned resource before the first
    // post-update pass. A null here means Startup did not run, which is the bug to fix.
    const bool gated = (*reinterpret_cast<uintptr_t*>(c + 0x28) == 0 ||
                        *reinterpret_cast<uintptr_t*>(c + 0x38) == 0 ||
                        *reinterpret_cast<uintptr_t*>(c + 0x30) == 0);
    if (gated) return;

    const float kOne          = s_fOne ? *s_fOne : 1.0f;
    const float kEpsilon      = s_fScaleEps ? *s_fScaleEps : 0.001f;
    const float kSmoothOld    = s_fScaleOldW ? *s_fScaleOldW : 0.99f;
    const float kSmoothNew    = s_fScaleNewW ? *s_fScaleNewW : 0.01f;
    const float kCropSpreadDiv = s_fCropSpreadDiv ? *s_fCropSpreadDiv : 0.15f;

    uintptr_t resource = *reinterpret_cast<uintptr_t*>(c + 0x28);
    float target = *reinterpret_cast<float*>(resource + 0x28);   // growth 0..n
    if (target > kOne) target = kOne;

    uintptr_t desc = *reinterpret_cast<uintptr_t*>(c + 0x38);
    float descFloor = *reinterpret_cast<float*>(desc + 0x60);
    if (descFloor > target) target = descFloor;

    float cur = *reinterpret_cast<float*>(c + 0x48);
    const float diff = std::fabs(target - cur);
    const bool converged = (kEpsilon > diff);
    if (converged) {
        *reinterpret_cast<float*>(c + 0x48) = target;
    } else {
        *reinterpret_cast<float*>(c + 0x48) = cur * kSmoothOld + target * kSmoothNew;
    }

    const float scale = *reinterpret_cast<float*>(c + 0x48);

    uintptr_t model = *reinterpret_cast<uintptr_t*>(c + 0x30);

    if (*reinterpret_cast<int8_t*>(desc + 0x64) != 0) {
        // Crop form: spread (x,y) reaches full early, height (z) tracks growth.
        float spread = scale / kCropSpreadDiv;
        if (spread < 0.0f)      spread = 0.0f;
        else if (spread > kOne) spread = kOne;
        *reinterpret_cast<float*>(model + 0x134) = spread;
        *reinterpret_cast<float*>(model + 0x138) = spread;
        *reinterpret_cast<float*>(model + 0x13c) = scale;
        *reinterpret_cast<float*>(model + 0x140) = kOne;
    } else {
        // Uniform form: Math::Vector4(const Scalar&) broadcasts the scale into 4 lanes.
        float scalar = scale;
        uint8_t vec4[16];
        if (s_fn_Vec4FromScalar) {
            s_fn_Vec4FromScalar(vec4, &scalar);
            const uint32_t* v = reinterpret_cast<const uint32_t*>(vec4);
            *reinterpret_cast<uint32_t*>(model + 0x134) = v[0];
            *reinterpret_cast<uint32_t*>(model + 0x138) = v[1];
            *reinterpret_cast<uint32_t*>(model + 0x13c) = v[2];
            *reinterpret_cast<uint32_t*>(model + 0x140) = v[3];
        }
    }

    if (!converged) return;   // stay registered until the ease-in finishes

    uint32_t oldFlags = *reinterpret_cast<uint32_t*>(c + 0x20);
    uint32_t newFlags = oldFlags & ~0x10u;
    *reinterpret_cast<uint32_t*>(c + 0x20) = newFlags;
    if (((newFlags ^ oldFlags) & 0xffff) != 0 &&
        *reinterpret_cast<uintptr_t*>(c + 0x10) != 0) {
        if (s_fn_FlagNotify) s_fn_FlagNotify(*reinterpret_cast<void**>(c + 0x10));
    }
}

// [11] 0x1401206a0 (64 B). PORT GAP (retail 0x1401206a0) — B38 D9: not ported;
// the tree's table held the inert stub here, kept.
void GrowthComponent::Slot11() {}

// create 0x140034b10: MemAlloc(0x50, 8), then the ctor.
void* GrowthComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(GrowthComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) GrowthComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* GrowthComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) GrowthComponent());
}

DEFINE_EXE_TYPE(GrowthComponent, "Component")
