// code/Game/Component/fruitcomponent.cpp
// Clean reimplementation of FruitComponent::Update from Application-x64.exe.
//
// Fruit ripeness simulation: accumulates growth progress each tick based on
// temperature sensitivity and seasonal growth factor. Uses a quadratic
// temperature curve -- growth peaks at the optimal temp and falls off as the
// temperature diverges. Result is clamped to [0, ~1] (the upper clamp uses the
// engine's g_fUVScale 0.98 threshold, snapping to exactly 1.0).
//
// Key formula (faithful to 0x140049de0):
//   resData       = IsDerivedFrom(self->_unk18, FruitDescription) ? self->_unk18 : 0
//   ripeningSpeed = g_fUVScale / (resData->+0x20 * SECS_PER_SEASON)
//   tempDelta     = |currentTemp - resData->+0x24| / resData->+0x28
//   if fruitFlag clear && (*(self->+0x28))->+0x28 >= g_fUVScale:
//     if tempDelta in [0, g_fUVScale]: growthFactor = g_fUVScale - tempDelta^2
//     else if currentTemp <= FREEZE_THRESHOLD: growthFactor = FROST_DAMAGE
//     else: growthFactor = 0.0
//   else: growthFactor = 0.0
//   effRate = min((*(self->+0x28))->+0x28, g_fUVScale)
//   ripeness += dtSeconds * effRate * ripeningSpeed * growthFactor
//   clamp ripeness to [0.0, 1.0]
//
// Original: 0x140049de0 (vtable slot 9, +0x48; vtable @ 0x140159450)
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/fruitcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>

// ============================================================
// Resolved helpers (.rdata constants + globals)
// ============================================================

// g_fUVScale (DAT_140172140 = 0.98f). Used as the "near-1.0" comparison/clamp
// value throughout Update AND as the ripeningSpeed numerator. (NOT a literal
// 1.0f -- that was the prior port's bug.)
static float* s_fUVScale = nullptr;            // 0x140172140 -> 0.98f
static float* s_fSecsPerSeason = nullptr;      // 0x140172234 -> 300.0f
static float* s_fSecsPerYear = nullptr;        // 0x14017225c -> 3600.0f
static float* s_fMonthsPerYear = nullptr;      // 0x1401721b8 -> 12.0f
static float* s_fFreezeThreshold = nullptr;    // 0x1401721d0 -> 32.0f
static float* s_fFrostDamage = nullptr;        // 0x140172288 -> -10.0f
// DAT_1401722b0 -- double mask for fabs (0x7FFFFFFFFFFFFFFF)
static uint64_t* s_dblAbsMask = nullptr;       // 0x1401722b0

// FruitDescription type-id (DAT_1401c4288), written by FruitDescription_Register.
static uint32_t* s_pFruitDescTypeId = nullptr; // 0x1401c4288

// Game world singleton (DAT_1401c3dd8 == g_pGameWorldSingleton)
//   +0x128: pointer to the season-time object (+8 = Time value)
//   +0x130: pointer to the weather/environment data (+0x18 = current temperature)
static void** s_pGameWorld = nullptr;          // 0x1401c3dd8

// FUN_1400052c0 -- find the sibling NaturalResourceComponent IBP on the owner
// entity (the slot-4 OnCreate accessor; already registered in reg_work_base.cpp).
typedef void* (*fn_FindNatResSibling_t)(void* owner, void* outIBP);
static fn_FindNatResSibling_t s_fn_FindNatResSibling = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fUVScale         = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172140));
    s_fSecsPerSeason   = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172234));
    s_fSecsPerYear     = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x14017225c));
    s_fMonthsPerYear   = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x1401721b8));
    s_fFreezeThreshold = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x1401721d0));
    s_fFrostDamage     = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172288));
    s_dblAbsMask       = reinterpret_cast<uint64_t*>(ResolveAddr(Module::Exe, 0x1401722b0));

    s_pFruitDescTypeId = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4288));
    s_pGameWorld       = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401c3dd8));

    s_fn_FindNatResSibling = reinterpret_cast<fn_FindNatResSibling_t>(
        ResolveAddr(Module::Exe, 0x1400052c0));

    s_initialized = true;
}

// fabsf via the double-abs bitmask the original uses (matches bitwise pattern).
static inline float AbsViaMask(float v) {
    double d = static_cast<double>(v);
    uint64_t bits;
    __builtin_memcpy(&bits, &d, sizeof(bits));
    bits &= s_dblAbsMask ? *s_dblAbsMask : 0x7fffffffffffffffull;
    __builtin_memcpy(&d, &bits, sizeof(d));
    return static_cast<float>(d);
}

// ============================================================
// FruitComponent::Startup / OnCreate (vtable slot [4], +0x20, original 0x14004a030)
// ============================================================
// Body-init: populate self->field28 (+0x28) with the sibling
// NaturalResourceComponent's InternalBasePtr from the owning entity (+0x10).
//
//   CheckPtr(self+0x10);
//   ibp = FUN_1400052c0(*(self+0x10), &local);   // first sibling NatResComp
//   self+0x28 = *(void**)ibp;                     // the wrapped component ptr
//
// Called at spawn by ComponentEntity_StartupComponents (vtable+0x20). _unk18
// (+0x18, the FruitDescription) is set earlier by the base Component copy-ctor
// (ComponentEntity_CtorFromDescription).
void FruitComponent_Startup(FruitComponent* self) {
    EnsureHelpers();
    uint8_t* comp = reinterpret_cast<uint8_t*>(self);

    void* owner = *reinterpret_cast<void**>(comp + 0x10);
    if (!owner) return;   // not yet wired into an entity

    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(comp + 0x10);

    // FUN_1400052c0 returns an InternalBasePtr (out-param); its first qword is the
    // wrapped component pointer, which the original stores into self+0x28.
    int64_t ibp = 0;
    if (s_fn_FindNatResSibling)
        s_fn_FindNatResSibling(owner, &ibp);
    *reinterpret_cast<void**>(comp + 0x28) = reinterpret_cast<void*>(ibp);
}

// ============================================================
// FruitComponent::Update (vtable slot [9], +0x48, 0x140049de0)
// ============================================================
// Fruit ripening simulation with temperature-dependent growth.

void FruitComponent_Update(FruitComponent* self, int64_t* pDeltaTime) {
    EnsureHelpers();

    uint8_t* comp = reinterpret_cast<uint8_t*>(self);

    // [LIVE-COMPONENT BRING-UP GUARD] On an inert-spawned standalone component the
    // body is zeroed unless Startup ran. _unk18(+0x18) is the FruitDescription set
    // by the copy-ctor; field28(+0x28) is the sibling NaturalResourceComponent IBP
    // populated by FruitComponent_Startup (slot 4), which the spawn pipeline
    // invokes via vtable+0x20 in ComponentEntity_StartupComponents. Belt-and-
    // suspenders: if +0x28 is still null but the description(+0x18) is present, run
    // the body-init now (idempotent); if still incomplete, bail safely -- the math
    // below divides by resData->+0x20 and derefs *(self+0x28). Harmless in
    // DLL-hook mode (body populated there). See growthcomponent.cpp for the
    // worked example / notes/STANDALONE_REG_WAVES_PLAN.md.
    if (*(uintptr_t*)(comp + 0x28) == 0) {
        if (*(uintptr_t*)(comp + 0x18) != 0)
            FruitComponent_Startup(self);
        if (*(uintptr_t*)(comp + 0x28) == 0)
            return;
    }

    const float fUV = s_fUVScale ? *s_fUVScale : 0.98f;  // ~1.0 comparison/clamp

    // resData = IsDerivedFrom(self->_unk18, FruitDescription) ? self->_unk18 : 0
    void* baseObj = self->_unk18;
    void* resData = nullptr;
    if (baseObj && fn_IsDerivedFrom && s_pFruitDescTypeId) {
        if (fn_IsDerivedFrom(baseObj, *s_pFruitDescTypeId))
            resData = baseObj;
    }
    // [STANDALONE TRUST] The factory/type tree can be incomplete in standalone, so
    // IsDerivedFrom on a description is unreliable (see growthcomponent.cpp / the
    // SpawnComponent precedent). _unk18 is the FruitDescription the copy-ctor stored
    // at +0x18 — trust it as a fallback so a present fruit entity still simulates.
    // DLL-hook mode is unchanged (the IsDerivedFrom branch already set resData).
    if (!resData && baseObj) resData = baseObj;
    if (!resData) return;  // original derefs resData->+0x20/+0x24/+0x28 below

    // ripeningSpeed = g_fUVScale / (resData->+0x20 * SECS_PER_SEASON)
    float secsPerSeason = s_fSecsPerSeason ? *s_fSecsPerSeason : 300.0f;
    float ripeningSpeed = fUV / (*(float*)((uintptr_t)resData + 0x20) * secsPerSeason);

    // Seasonal flag-clear: when fruitFlag(+0x34) set, clear it once the month index
    // leaves the fruit-bearing window.  month = (seasonSecs/3600)*12 - 7; clear if
    // (uint)month > 4.
    if (self->fruitFlag != 0) {
        if (s_pGameWorld && *s_pGameWorld) {
            uintptr_t world = (uintptr_t)*s_pGameWorld;
            void* seasonTime = (void*)(*(uintptr_t*)(world + 0x128) + 8);
            float seasonSecs = fn_Time_AsSeconds ? fn_Time_AsSeconds(seasonTime) : 0.0f;
            float secsPerYear   = s_fSecsPerYear   ? *s_fSecsPerYear   : 3600.0f;
            float monthsPerYear = s_fMonthsPerYear ? *s_fMonthsPerYear : 12.0f;
            unsigned int monthIdx =
                (unsigned int)((int)((seasonSecs / secsPerYear) * monthsPerYear) - 7);
            if (monthIdx > 4)
                self->fruitFlag = 0;
        }
    }

    // currentTemp = *(*(world+0x130)+0x18)
    float currentTemp = 0.0f;
    if (s_pGameWorld && *s_pGameWorld) {
        uintptr_t world = (uintptr_t)*s_pGameWorld;
        currentTemp = *(float*)(*(uintptr_t*)(world + 0x130) + 0x18);
    }

    // tempDelta = |currentTemp - resData->+0x24| / resData->+0x28
    float optimalTemp = *(float*)((uintptr_t)resData + 0x24);
    float tempRange   = *(float*)((uintptr_t)resData + 0x28);
    float tempDelta   = AbsViaMask(currentTemp - optimalTemp) / tempRange;

    float growthFactor = 0.0f;
    if (self->fruitFlag == 0) {
        if (fn_InternalBasePtr_CheckPtr)
            fn_InternalBasePtr_CheckPtr(comp + 0x28);
        // growth rate lives at (*(self+0x28))->+0x28 (the linked NatResComp).
        float growthRate = *(float*)(*(uintptr_t*)(comp + 0x28) + 0x28);
        if (growthRate >= fUV) {
            if (tempDelta >= 0.0f && tempDelta <= fUV) {
                // Quadratic falloff: g_fUVScale - delta^2
                growthFactor = fUV - tempDelta * tempDelta;
            } else {
                growthFactor = 0.0f;
                float freezeTemp = s_fFreezeThreshold ? *s_fFreezeThreshold : 32.0f;
                if (currentTemp <= freezeTemp)
                    growthFactor = s_fFrostDamage ? *s_fFrostDamage : -10.0f;
            }
        }
        // growthRate < g_fUVScale -> growthFactor stays 0.0
    }

    // effRate = min((*(self+0x28))->+0x28, g_fUVScale)
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(comp + 0x28);
    float effRate = *(float*)(*(uintptr_t*)(comp + 0x28) + 0x28);
    if (effRate >= fUV) effRate = fUV;

    // ripeness(+0x30) += dtSeconds * effRate * ripeningSpeed * growthFactor
    float dtSeconds = fn_Time_AsSeconds ? fn_Time_AsSeconds(pDeltaTime) : 0.0f;
    float* pRipeness = reinterpret_cast<float*>(&self->field30);
    float newRipeness = dtSeconds * effRate * ripeningSpeed * growthFactor + *pRipeness;
    *pRipeness = newRipeness;

    // Phase-1 bring-up verification trace (env BANISHED_FRUIT_TRACE, default off
    // -> byte-unchanged). Follows ONE FruitComponent across ticks, printing its
    // accumulating ripeness (self->field30 / +0x30). Confirms the live recipe
    // end-to-end: Update entered via vtable +0x48, Startup populated field28@+0x28,
    // and the value advances per tick.
    {
        static int s_traceState = -1;   // -1=unchecked, 0=off, 1=on
        static int s_traceCount = 0;
        static void* s_lockComp = nullptr;  // follow ONE component across ticks
        if (s_traceState < 0) {
            const char* e = std::getenv("BANISHED_FRUIT_TRACE");
            s_traceState = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
        }
        if (s_traceState == 1 && s_lockComp == nullptr) s_lockComp = comp;
        if (s_traceState == 1 && comp == s_lockComp && s_traceCount < 40) {
            fprintf(stderr,
                "[FruitTrace] comp=%p resData=%p flag=%d dt=%.5f speed=%.6f "
                "temp=%.3f delta=%.4f gf=%.4f eff=%.4f -> ripe=%.6f\n",
                (void*)comp, (void*)resData, (int)self->fruitFlag, dtSeconds,
                ripeningSpeed, currentTemp, tempDelta, growthFactor, effRate,
                newRipeness);
            fflush(stderr);
            s_traceCount++;
        }
    }

    // Clamp to [0.0, 1.0]: < 0 -> 0; > g_fUVScale -> exactly 1.0.
    if (*pRipeness < 0.0f)
        *pRipeness = 0.0f;
    if (*pRipeness > fUV)
        *pRipeness = 1.0f;
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void FruitComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[FruitComponent] Funcs resolved: GameWorld=%p FruitDescTypeId=%p "
           "FindNatResSibling=%p UVScale=%p\n",
           (void*)s_pGameWorld, (void*)s_pFruitDescTypeId,
           (void*)s_fn_FindNatResSibling, (void*)s_fUVScale);
#endif
}

// ===== Standalone factory (Phase 1 inert->live component bring-up) =====
// By DEFAULT FruitComponent stays inert exactly like every other standalone
// component (default run byte-for-byte unchanged). Set BANISHED_LIVE_FRUIT=1 (or
// the global BANISHED_LIVE_COMPONENTS=1) to graduate it to a real per-type vtable:
//   * slot 4 (+0x20 Startup) -> FruitComponent_Startup  (body-init field28@+0x28)
//   * slot 9 (+0x48 Update)  -> FruitComponent_Update    (per-tick ripening sim)
// Size 0x38, capability flags +0x20 = 0x88 (from FruitComponent_Ctor).
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_LIVE_FACTORY_S(FruitComponent, 0x38, 0x88, "BANISHED_LIVE_FRUIT",
                         FruitComponent_Update, FruitComponent_Startup)
REGISTER_TYPE(FruitComponent, "Component",
              &FruitComponent_Factory, &FruitComponent_CreateInPlace);
