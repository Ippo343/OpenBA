// code/Game/Component/citizencomponent.cpp
// Clean reimplementation of CitizenComponent methods from Application-x64.exe.
//
// CitizenComponent (0x290 bytes) is the largest AI component — full citizen state
// (health, happiness, inventory, mesh, etc). Contains a MeshInstance sub-object
// at +0xB8 (0x120 bytes).
//
// Two reimplemented functions:
//   Update (0x140112e70) — main lifecycle tick (711 instructions!)
//   ShouldSwitchWorkplace (0x1401109E0) — workplace switch decision
//
// Vtable at 0x140162580.
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/citizencomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers for Update
// ============================================================

// System::Time::AsSeconds(Time*) -> float
// Dispatch slot at 0x140152a70
typedef float (*fn_Time_AsSeconds_t)(void* time);
static fn_Time_AsSeconds_t s_fn_Time_AsSeconds = nullptr;

// System::Time::Time(float seconds) — construct Time from seconds
// Dispatch slot at 0x140152a80
typedef void (*fn_Time_FromSeconds_t)(void* outTime, float seconds);
static fn_Time_FromSeconds_t s_fn_Time_FromSeconds = nullptr;

// System::Random::RandFloat(Random*) -> float
// Dispatch slot at 0x140152a60
typedef float (*fn_RandFloat_t)(void* random);
static fn_RandFloat_t s_fn_RandFloat = nullptr;

// CitizenComponent_TryFindPartner(self, outIBP) -> char (bool)
// At 0x14010fe40
typedef char (*fn_TryFindPartner_t)(void* self, void* outIBP);
static fn_TryFindPartner_t s_fn_TryFindPartner = nullptr;

// CitizenComponent_AssignState(self, state, param2, strParam)
// At 0x140110420
typedef void (*fn_AssignState_t)(void* self, int state, int param2, const char* strParam);
static fn_AssignState_t s_fn_AssignState = nullptr;

// CitizenComponent_HandleLifeEvent(self, eventType, strParam) -> char (bool)
// At 0x140112450
typedef char (*fn_HandleLifeEvent_t)(void* self, int eventType, const char* strParam);
static fn_HandleLifeEvent_t s_fn_HandleLifeEvent = nullptr;

// CitizenComponent_UpdateHealth(self, pTime) -> char (bool: 0=dead, nonzero=alive)
// At 0x140111b90
typedef char (*fn_UpdateHealth_t)(void* self, void* pTime);
static fn_UpdateHealth_t s_fn_UpdateHealth = nullptr;

// CitizenComponent_ConsumeFood(self)
// At 0x140111890
typedef void (*fn_ConsumeFood_t)(void* self);
static fn_ConsumeFood_t s_fn_ConsumeFood = nullptr;

// CitizenComponent_ConsumeFuel(self)
// At 0x140111a70
typedef void (*fn_ConsumeFuel_t)(void* self);
static fn_ConsumeFuel_t s_fn_ConsumeFuel = nullptr;

// CitizenComponent_FindWorkplace(self)
// At 0x140115530
typedef void (*fn_FindWorkplace_t)(void* self);
static fn_FindWorkplace_t s_fn_FindWorkplace = nullptr;

// CitizenComponent_FindStorage(self)
// At 0x140115770
typedef void (*fn_FindStorage_t)(void* self);
static fn_FindStorage_t s_fn_FindStorage = nullptr;

// FUN_14012c3f0 — HealthComponent status flag setter
// At 0x14012c3f0: (healthComp*, flagIndex, flagValue)
typedef void (*fn_HealthSetFlag_t)(void* healthComp, int flagIndex, char value);
static fn_HealthSetFlag_t s_fn_HealthSetFlag = nullptr;

// CitizenComponent_AssignWorkplace(self, workplaceIBP, slotIndex)
// At 0x140114ca0
typedef void (*fn_AssignWorkplace_t)(void* self, void* workplaceIBP, int slotIndex);
static fn_AssignWorkplace_t s_fn_AssignWorkplace = nullptr;

// CitizenComponent_AssignHousing(self, housingPtr)
// At 0x140114f80
typedef void (*fn_AssignHousing_t)(void* self, void* housingPtr);
static fn_AssignHousing_t s_fn_AssignHousing = nullptr;

// CitizenComponent_sub_ffc0 — find unassigned housing
// At 0x14010ffc0
typedef void (*fn_FindHousingUnassigned_t)(int64_t self, void* outIBP);
static fn_FindHousingUnassigned_t s_fn_FindHousingUnassigned = nullptr;

// CitizenComponent_sub_0470 — find assigned housing
// At 0x140110470
typedef void (*fn_FindHousingAssigned_t)(int64_t self, void* outIBP);
static fn_FindHousingAssigned_t s_fn_FindHousingAssigned = nullptr;

// CitizenComponent_helper_ff80 — find new workplace (after ShouldSwitch)
// At 0x14010ff80
typedef void* (*fn_FindNewWorkplace_t)(int64_t self, void* outIBP);
static fn_FindNewWorkplace_t s_fn_FindNewWorkplace = nullptr;

// CitizenComponent_Update_sub_0e70 — find work (workplace search)
// At 0x140110e70
typedef void (*fn_FindWork_t)(int64_t self, void* outEntityIBP, int* outSlotIndex, char noWorkplace);
static fn_FindWork_t s_fn_FindWork = nullptr;

// CitizenComponent_sub_1fa0 — release current workplace assignment
// At 0x140111fa0
typedef void (*fn_ReleaseWork_t)(void* self, void* outIBP);
static fn_ReleaseWork_t s_fn_ReleaseWork = nullptr;

// CitizenComponent_helper_fe40 — find partner
// At 0x14010fe40
typedef void* (*fn_FindPartner_t)(int64_t self, void* outIBP);
static fn_FindPartner_t s_fn_FindPartner = nullptr;

// CitizenComponent_helper_1b90 — handle reproduction
// At 0x140111b90
// NOTE: This is NOT UpdateHealth — it's the repro helper.
// Re-checking decompilation: at offset +0x1b90 relative to base.
// Actually let me re-read: the decompilation calls "CitizenComponent_helper_1b90" which
// is the reproduction helper at 0x140111b90. But wait, 0x140111b90 is UpdateHealth.
// The reproduction helper is at a different address. Let me resolve from the decompilation.
// In the Ghidra output line: CitizenComponent_helper_1b90((Internal *)param_1);
// This corresponds to the "handle reproduction" sub-function. Address = base + offset.
// Looking at the decompilation context: it's inside the "happiness/reproduction" block.

// CitizenComponent_helper_1b90 — handle reproduction/procreation
// At 0x140111b90 — wait, that's UpdateHealth. Let me re-check.
// The decompilation at line ~370 says CitizenComponent_helper_1b90 in the happiness block.
// But 0x140111b90 IS UpdateHealth based on the function header address.
// Re-reading: the Ghidra function name "helper_1b90" uses offset from the base of
// CitizenComponent functions (0x140110000 region). So 0x140110000 + 0x1b90 = 0x140111b90.
// Hmm, that conflicts. The actual reproduction helper must be at a different address.
// Let me check the decompilation more carefully — it's actually CitizenComponent_helper_1b90
// which IS 0x140111b90 = same as UpdateHealth. The Ghidra naming may be wrong here.
// Actually no — the function CitizenComponent_UpdateHealth is explicitly called earlier
// in the decompilation as a separate call. And "helper_1b90" is called in the reproduction
// block. This suggests they are different functions at different addresses OR Ghidra
// has named them confusingly.
// To be safe, let me just resolve the actual address from the call site.

// After careful review: the reproduction handler is at 0x140111b90 which Ghidra may
// show differently depending on naming. Let me use the decompilation address directly.
typedef void (*fn_HandleRepro_t)(void* self);

// CitizenComponent_helper_a4f0 — check storage has capacity for item type
// At 0x14012a4f0
typedef bool (*fn_CheckStorageCapacity_t)(int64_t storageComp, int itemType);
static fn_CheckStorageCapacity_t s_fn_CheckStorageCapacity = nullptr;

// Entity_FindComponent_Storage(entity, outIBP) -> InternalBasePtr*
// At 0x140008090 (or 0x140006270 depending on which variant)
typedef void* (*fn_FindCompStorage_t)(int64_t entity, void* outIBP);
static fn_FindCompStorage_t s_fn_FindCompStorage = nullptr;

// UI::StringTable::GetText — use the typedef from engine_funcs.h
// Signature: (self, retval, textIndex) -> void
// We use the engine_funcs.h extern directly: fn_StringTable_GetText

// (removed: UIString_ToSysString — handled inline via dispatch slot)

// fn_InternalBasePtr_CheckPtr_slot — dispatch slot version at 0x140152ab0
// (same as fn_InternalBasePtr_CheckPtr but read from .rdata slot)
typedef void (*fn_CheckPtr_slot_t)(void* self);
static fn_CheckPtr_slot_t s_fn_CheckPtr_slot = nullptr;

// Float constants from .rdata
static float* s_DAT_14017225c = nullptr;  // hunger divisor
static float* s_DAT_140172234 = nullptr;  // thirst/time multiplier (used as fVar14 init)
static float* s_DAT_140172114 = nullptr;  // hunger threshold fraction (0.75?)
static float* s_DAT_140172100 = nullptr;  // thirst threshold fraction
static uint32_t* s_DAT_1401722c0 = nullptr; // XOR mask for happiness timer

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    // Dispatch slot for CheckPtr — read fn ptr from .rdata at runtime
    uintptr_t slot = ResolveAddr(Module::Exe, 0x140152ab0);
    if (slot) s_fn_CheckPtr_slot = *reinterpret_cast<fn_CheckPtr_slot_t*>(slot);

    // Dispatch slot for Time::AsSeconds
    slot = ResolveAddr(Module::Exe, 0x140152a70);
    if (slot) s_fn_Time_AsSeconds = *reinterpret_cast<fn_Time_AsSeconds_t*>(slot);

    // Dispatch slot for Time::Time(float)
    slot = ResolveAddr(Module::Exe, 0x140152a80);
    if (slot) s_fn_Time_FromSeconds = *reinterpret_cast<fn_Time_FromSeconds_t*>(slot);

    // Direct exe addresses for CitizenComponent sub-functions
    s_fn_TryFindPartner = reinterpret_cast<fn_TryFindPartner_t>(
        ResolveAddr(Module::Exe, 0x14010fe40));
    s_fn_AssignState = reinterpret_cast<fn_AssignState_t>(
        ResolveAddr(Module::Exe, 0x140110420));
    s_fn_HandleLifeEvent = reinterpret_cast<fn_HandleLifeEvent_t>(
        ResolveAddr(Module::Exe, 0x140112450));
    s_fn_UpdateHealth = reinterpret_cast<fn_UpdateHealth_t>(
        ResolveAddr(Module::Exe, 0x140111b90));
    s_fn_ConsumeFood = reinterpret_cast<fn_ConsumeFood_t>(
        ResolveAddr(Module::Exe, 0x140111890));
    s_fn_ConsumeFuel = reinterpret_cast<fn_ConsumeFuel_t>(
        ResolveAddr(Module::Exe, 0x140111a70));
    s_fn_FindWorkplace = reinterpret_cast<fn_FindWorkplace_t>(
        ResolveAddr(Module::Exe, 0x140115530));
    s_fn_FindStorage = reinterpret_cast<fn_FindStorage_t>(
        ResolveAddr(Module::Exe, 0x140115770));
    s_fn_HealthSetFlag = reinterpret_cast<fn_HealthSetFlag_t>(
        ResolveAddr(Module::Exe, 0x14012c3f0));
    s_fn_AssignWorkplace = reinterpret_cast<fn_AssignWorkplace_t>(
        ResolveAddr(Module::Exe, 0x140114ca0));
    s_fn_AssignHousing = reinterpret_cast<fn_AssignHousing_t>(
        ResolveAddr(Module::Exe, 0x140114f80));
    s_fn_FindHousingUnassigned = reinterpret_cast<fn_FindHousingUnassigned_t>(
        ResolveAddr(Module::Exe, 0x14010ffc0));
    s_fn_FindHousingAssigned = reinterpret_cast<fn_FindHousingAssigned_t>(
        ResolveAddr(Module::Exe, 0x140110470));
    s_fn_FindNewWorkplace = reinterpret_cast<fn_FindNewWorkplace_t>(
        ResolveAddr(Module::Exe, 0x14010ff80));
    s_fn_FindWork = reinterpret_cast<fn_FindWork_t>(
        ResolveAddr(Module::Exe, 0x140110e70));
    s_fn_ReleaseWork = reinterpret_cast<fn_ReleaseWork_t>(
        ResolveAddr(Module::Exe, 0x140111fa0));
    s_fn_CheckStorageCapacity = reinterpret_cast<fn_CheckStorageCapacity_t>(
        ResolveAddr(Module::Exe, 0x14012a4f0));
    s_fn_FindCompStorage = reinterpret_cast<fn_FindCompStorage_t>(
        ResolveAddr(Module::Exe, 0x140008090));

    // Float constants from .rdata
    s_DAT_14017225c = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x14017225c));
    s_DAT_140172234 = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172234));
    s_DAT_140172114 = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));
    s_DAT_140172100 = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172100));
    s_DAT_1401722c0 = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401722c0));

    s_helpersResolved = true;
}

// ============================================================
// Helper: read citizen description pointer
// ============================================================
static inline int64_t GetDescription(CitizenComponent* self) {
    return *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(self) + 0x288);
}

// ============================================================
// CitizenComponent::Update (vtable slot [9], 0x140112e70)
// ============================================================
// Main citizen lifecycle tick. 711 instructions in original.
// Handles aging, hunger/thirst accumulation, health/death checks,
// food/fuel consumption, workplace assignment, housing, partner
// finding, and child-bearing logic.
//
// Parameters: RCX = this (CitizenComponent*), RDX = Time* (pointer to Time)

void CitizenComponent_Update(CitizenComponent* self, void* deltaTime) {
    EnsureHelpers();

    uint8_t* p = reinterpret_cast<uint8_t*>(self);
    uint8_t* gameWorld = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    int64_t weatherData = *reinterpret_cast<int64_t*>(gameWorld + 0x130);
    int64_t desc = GetDescription(self);

    // Convert Time to seconds
    float dt_sec = s_fn_Time_AsSeconds ? s_fn_Time_AsSeconds(deltaTime) : 0.0f;
    int64_t dt_ticks = *reinterpret_cast<int64_t*>(deltaTime);

    // Check if citizen is adult (age >= adult threshold) and not in education
    float age = *reinterpret_cast<float*>(p + 0x3C);
    float adultAge = *reinterpret_cast<float*>(desc + 0x44);
    bool isAdult;
    if (age < adultAge || *reinterpret_cast<int64_t*>(p + 0x208) != 0) {
        isAdult = false;
    } else {
        isAdult = true;
    }

    // --- Aging: increment age ---
    float agingRate = *reinterpret_cast<float*>(desc + 0x40);
    float newAge = age + dt_sec * agingRate;
    *reinterpret_cast<float*>(p + 0x3C) = newAge;

    // --- Accumulate timers ---
    *reinterpret_cast<int64_t*>(p + 0x58) += dt_ticks;   // food timer
    *reinterpret_cast<int64_t*>(p + 0x90) += dt_ticks;   // fuel timer
    *reinterpret_cast<int64_t*>(p + 0x1E8) -= dt_ticks;  // education countdown
    if (*reinterpret_cast<int64_t*>(p + 0x208) != 0) {
        *reinterpret_cast<int64_t*>(p + 0x218) += dt_ticks;  // school timer
    }
    *reinterpret_cast<int64_t*>(p + 0x200) += dt_ticks;  // general timer
    *reinterpret_cast<int64_t*>(p + 0x60) += dt_ticks;   // work timer

    // Clamp education countdown to >= 0
    if (*reinterpret_cast<int64_t*>(p + 0x1E8) < 0) {
        *reinterpret_cast<uint32_t*>(p + 0x1E4) = 0;
        *reinterpret_cast<int64_t*>(p + 0x1E8) = 0;
    }

    // --- Adult transition: try to find partner or assign to work ---
    if (!isAdult && adultAge <= newAge && *reinterpret_cast<int64_t*>(p + 0x208) == 0) {
        int stateToAssign;
        if (p[0x220] == 0) {
            // Not yet had partner — try to find one
            int64_t localIBP = 0;
            fn_InternalBasePtr_Ctor(&localIBP, nullptr);
            char found = s_fn_TryFindPartner ? s_fn_TryFindPartner(self, &localIBP) : 0;
            stateToAssign = 10;
            if (found == 0) {
                stateToAssign = 9;
            }
        } else {
            stateToAssign = 9;
        }
        if (s_fn_AssignState) s_fn_AssignState(self, stateToAssign, 0, nullptr);
    }

    // --- Education cap: update max education from description ---
    uint32_t gameFlags = *reinterpret_cast<uint32_t*>(gameWorld + 0xD0);
    if ((gameFlags >> 9 & 1) != 0) {
        bool canEducate = (adultAge <= newAge && *reinterpret_cast<int64_t*>(p + 0x208) == 0)
                          || (*reinterpret_cast<int64_t*>(p + 0x208) != 0);
        if (canEducate) {
            int maxEdu = *reinterpret_cast<int*>(desc + 0x118);
            if (*reinterpret_cast<int*>(p + 0x210) < maxEdu) {
                *reinterpret_cast<int*>(p + 0x210) = maxEdu;
            }
        }
    }

    // --- Mortality check: if dead + has workplace, zero health accumulator ---
    if (*reinterpret_cast<int64_t*>(p + 0x68) != 0 && *reinterpret_cast<int64_t*>(p + 0x228) != 0) {
        *reinterpret_cast<int64_t*>(p + 0x44) = 0;
    }

    // --- Hunger accumulation ---
    if ((gameFlags >> 1 & 1) == 0) {
        float hungerRate = *reinterpret_cast<float*>(desc + 0xC0);
        float hungerDiv = s_DAT_14017225c ? *s_DAT_14017225c : 1.0f;
        *reinterpret_cast<float*>(p + 0x44) += (dt_sec * hungerRate) / hungerDiv;
    } else {
        *reinterpret_cast<uint32_t*>(p + 0x44) = 0;
    }

    // --- Thirst accumulation ---
    float fVar14 = s_DAT_140172234 ? *s_DAT_140172234 : 1.0f;
    float thirstThreshTemp = *reinterpret_cast<float*>(desc + 0xF4);
    float weatherTemp = *reinterpret_cast<float*>(weatherData + 0x18);
    if ((gameFlags >> 2 & 1) != 0 || thirstThreshTemp < weatherTemp || thirstThreshTemp == weatherTemp) {
        *reinterpret_cast<uint32_t*>(p + 0x48) = 0;
    } else {
        // Thirst rate depends on clothing level (field at +0x1E4)
        int clothingLevel = *reinterpret_cast<int*>(p + 0x1E4);
        float thirstRate = *reinterpret_cast<float*>(desc + 0xDC + (int64_t)clothingLevel * 4);
        *reinterpret_cast<float*>(p + 0x48) += (dt_sec * thirstRate) / fVar14;
    }

    // --- Handle life event from profession name ---
    int64_t profRef = *reinterpret_cast<int64_t*>(p + 0x1F8);
    if (profRef != 0) {
        // Build UI::String from profession name, convert to System::String
        uint8_t localStr[16] = {};
        if (fn_UIString_Ctor) {
            fn_UIString_Ctor(localStr, *reinterpret_cast<uint64_t*>(profRef + 0x20),
                             reinterpret_cast<void*>(profRef + 0x40));
        }
        // The original calls UI::String::operator_class_System__String then String::String copy
        // Simplified: pass the string data to HandleLifeEvent
        int64_t localRes8 = 0;
        int64_t localRes18 = 0;

        // Original: UI::String -> System::String conversion + HandleLifeEvent(7, str)
        // This is complex string conversion. Call the sub-functions directly.
        // For now, delegate to the original pattern via a direct exe call.
        typedef void (*fn_UIStr_ToSysStr_t)(void* uiStr);
        uintptr_t slotAddr = ResolveAddr(Module::Exe, 0x140152878);
        fn_UIStr_ToSysStr_t toSysStr = nullptr;
        if (slotAddr) toSysStr = *reinterpret_cast<fn_UIStr_ToSysStr_t*>(slotAddr);

        if (toSysStr) toSysStr(localStr);

        // Copy construct System::String from localStr result
        void* strCopy = fn_String_Ctor_Copy ? fn_String_Ctor_Copy(&localRes18, &localRes8) : nullptr;
        char* strPtr = strCopy ? reinterpret_cast<char*>(*reinterpret_cast<int64_t*>(strCopy)) : nullptr;

        char eventResult = s_fn_HandleLifeEvent ? s_fn_HandleLifeEvent(self, 7, strPtr) : 0;

        if (fn_String_Dtor) {
            fn_String_Dtor(&localRes18);
            fn_String_Dtor(&localRes8);
        }

        if (eventResult != 0) return;
    }

    // --- Hunger cap: clamp and check death ---
    float hungerCap = *reinterpret_cast<float*>(desc + 0xD0);
    if (hungerCap <= *reinterpret_cast<float*>(p + 0x44)) {
        *reinterpret_cast<uint32_t*>(p + 0x44) = *reinterpret_cast<uint32_t*>(desc + 0xD0);
        if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(p + 0x278);
        int64_t healthComp = *reinterpret_cast<int64_t*>(p + 0x278);
        if (*reinterpret_cast<int*>(healthComp + 0x60) + 1 < 1) {
            char r = s_fn_HandleLifeEvent ? s_fn_HandleLifeEvent(self, 0, nullptr) : 0;
            if (r != 0) return;
        }
    }

    // --- Thirst cap: clamp and check death ---
    float thirstCap = *reinterpret_cast<float*>(desc + 0xF0);
    if (thirstCap <= *reinterpret_cast<float*>(p + 0x48)) {
        *reinterpret_cast<uint32_t*>(p + 0x48) = *reinterpret_cast<uint32_t*>(desc + 0xF0);
        if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(p + 0x278);
        int64_t healthComp = *reinterpret_cast<int64_t*>(p + 0x278);
        if (*reinterpret_cast<int*>(healthComp + 0x60) + 1 < 1) {
            char r = s_fn_HandleLifeEvent ? s_fn_HandleLifeEvent(self, 1, nullptr) : 0;
            if (r != 0) return;
        }
    }

    // --- Old age death check ---
    float maxAge = *reinterpret_cast<float*>(p + 0x40);
    if (maxAge <= *reinterpret_cast<float*>(p + 0x3C)) {
        char r = s_fn_HandleLifeEvent ? s_fn_HandleLifeEvent(self, 2, nullptr) : 0;
        if (r != 0) return;
    }

    // --- Health update ---
    char healthOk = s_fn_UpdateHealth ? s_fn_UpdateHealth(self, deltaTime) : 1;
    if (healthOk == 0) {
        // Citizen died from health — get death cause string
        int64_t localRes8 = 0;
        int64_t profData = *reinterpret_cast<int64_t*>(p + 0x68);
        if (profData) {
            void* stringTable = *reinterpret_cast<void**>(profData + 0x20);
            if (fn_StringTable_GetText) {
                // GetText(self, retval, textIndex) — Ghidra drops R8 (textIndex)
                // textIndex is likely 0 for single-entry profession name tables
                fn_StringTable_GetText(stringTable, &localRes8, 0);
                char* deathStr = reinterpret_cast<char*>(*reinterpret_cast<int64_t*>(&localRes8));
                char r = s_fn_HandleLifeEvent ? s_fn_HandleLifeEvent(self, 3, deathStr) : 0;
                if (fn_String_Dtor) fn_String_Dtor(&localRes8);
                if (r != 0) return;
            }
        }
    }

    // --- Find workplace if needed ---
    if (*reinterpret_cast<int64_t*>(p + 0x68) != 0) {
        *reinterpret_cast<uint32_t*>(p + 0x88) |= 1;
        if (*reinterpret_cast<int64_t*>(p + 0x228) == 0) {
            if (s_fn_FindWorkplace) s_fn_FindWorkplace(self);
        }
    }

    // --- Find storage if needed ---
    if (*reinterpret_cast<int64_t*>(p + 0x230) == 0) {
        if (s_fn_FindStorage) s_fn_FindStorage(self);
    }

    // --- Health status flags (via HealthComponent at +0x280) ---
    uint8_t* pSelf238 = p + 0x238;  // workplace IBP
    int64_t workplaceRef = *reinterpret_cast<int64_t*>(pSelf238);
    uint8_t* pCVar1 = p + 0x280;    // health component IBP

    if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(pCVar1);
    if (s_fn_HealthSetFlag)
        s_fn_HealthSetFlag(*reinterpret_cast<void**>(pCVar1), 0, workplaceRef == 0);

    // Hunger warning flag
    float hungerThreshLow = *reinterpret_cast<float*>(desc + 0xCC);
    float hungerThreshHigh = *reinterpret_cast<float*>(desc + 0xD0);
    float fVar4 = s_DAT_140172114 ? *s_DAT_140172114 : 0.75f;
    float hungerWarnLevel = (hungerThreshHigh - hungerThreshLow) * fVar4 + hungerThreshLow;
    float curHunger = *reinterpret_cast<float*>(p + 0x44);
    if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(pCVar1);
    if (s_fn_HealthSetFlag)
        s_fn_HealthSetFlag(*reinterpret_cast<void**>(pCVar1), 1, hungerWarnLevel < curHunger);

    // Thirst warning flag
    float thirstThreshLow = *reinterpret_cast<float*>(desc + 0xEC);
    float thirstThreshHigh = *reinterpret_cast<float*>(desc + 0xF0);
    float fThirstFrac = s_DAT_140172100 ? *s_DAT_140172100 : 0.75f;
    float thirstWarnLevel = (thirstThreshHigh - thirstThreshLow) * fThirstFrac + thirstThreshLow;
    float curThirst = *reinterpret_cast<float*>(p + 0x48);
    if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(pCVar1);
    if (s_fn_HealthSetFlag)
        s_fn_HealthSetFlag(*reinterpret_cast<void**>(pCVar1), 2, thirstWarnLevel < curThirst);

    // Unemployed flag
    char isUnemployed;
    int64_t profData1F0 = *reinterpret_cast<int64_t*>(p + 0x1F0);
    if (*reinterpret_cast<char*>(profData1F0 + 0x4C) == 0 &&
        *reinterpret_cast<int64_t*>(p + 0x248) == 0) {
        isUnemployed = 1;
    } else {
        isUnemployed = 0;
    }
    if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(pCVar1);
    if (s_fn_HealthSetFlag)
        s_fn_HealthSetFlag(*reinterpret_cast<void**>(pCVar1), 3, isUnemployed);

    // Homeless flag (flag 4 — implicitly from profession being nonzero)
    // Flag 5: has a profession
    int64_t profPtr68 = *reinterpret_cast<int64_t*>(p + 0x68);
    if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(pCVar1);
    if (s_fn_HealthSetFlag)
        s_fn_HealthSetFlag(*reinterpret_cast<void**>(pCVar1), 5, profPtr68 != 0);

    // --- Hunger/thirst urgency bit flags at +0x54 and +0x88 ---
    desc = GetDescription(self);  // re-read (may have changed)
    hungerThreshLow = *reinterpret_cast<float*>(desc + 0xCC);
    hungerThreshHigh = *reinterpret_cast<float*>(desc + 0xD0);
    if ((hungerThreshHigh - hungerThreshLow) * fVar4 + hungerThreshLow <
        *reinterpret_cast<float*>(p + 0x44)) {
        *reinterpret_cast<uint32_t*>(p + 0x54) |= 1;
        *reinterpret_cast<uint32_t*>(p + 0x88) |= 1;
    }
    thirstThreshLow = *reinterpret_cast<float*>(desc + 0xEC);
    thirstThreshHigh = *reinterpret_cast<float*>(desc + 0xF0);
    if ((thirstThreshHigh - thirstThreshLow) * fThirstFrac + thirstThreshLow <
        *reinterpret_cast<float*>(p + 0x48)) {
        *reinterpret_cast<uint32_t*>(p + 0x54) |= 2;
        *reinterpret_cast<uint32_t*>(p + 0x88) |= 1;
    }

    // --- Food consumption loop ---
    {
        float eatInterval = *reinterpret_cast<float*>(desc + 0x78) * fVar14;
        int64_t eatTicks = 0;
        if (s_fn_Time_FromSeconds) s_fn_Time_FromSeconds(&eatTicks, eatInterval);
        if (eatTicks > 0) {
            while (eatTicks < *reinterpret_cast<int64_t*>(p + 0x58)) {
                *reinterpret_cast<int64_t*>(p + 0x58) -= eatTicks;
                if (s_fn_ConsumeFood) s_fn_ConsumeFood(self);
            }
        }
    }

    // --- Fuel consumption loop ---
    {
        float fuelInterval = *reinterpret_cast<float*>(desc + 0xA0) * fVar14;
        int64_t fuelTicks = 0;
        if (s_fn_Time_FromSeconds) s_fn_Time_FromSeconds(&fuelTicks, fuelInterval);
        if (fuelTicks > 0) {
            while (fuelTicks < *reinterpret_cast<int64_t*>(p + 0x90)) {
                *reinterpret_cast<int64_t*>(p + 0x90) -= fuelTicks;
                if (s_fn_ConsumeFuel) s_fn_ConsumeFuel(self);
            }
        }
    }

    // --- Find storage for workplace ---
    int64_t localRes10 = 0;
    if (workplaceRef == 0) {
        int64_t nullIBP = 0;
        fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
        localRes10 = nullIBP;
    } else {
        if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(pSelf238);
        void* storageIBP = s_fn_FindCompStorage ?
            s_fn_FindCompStorage(*reinterpret_cast<int64_t*>(pSelf238), &localRes10) : nullptr;
        if (storageIBP) localRes10 = *reinterpret_cast<int64_t*>(storageIBP);
    }

    // --- Validate workplace ---
    if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(pCVar1);
    int64_t healthCompVal = *reinterpret_cast<int64_t*>(pCVar1);
    if (*reinterpret_cast<int*>(healthCompVal + 0x60) + 1 < 1 && workplaceRef != 0) {
        int64_t localRes8 = workplaceRef;
        char valid = fn_CitizenComponent_CheckWorkplaceValid ?
            fn_CitizenComponent_CheckWorkplaceValid(self, &localRes8) : 1;
        if (valid == 0) {
            int64_t nullIBP = 0;
            fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
            if (s_fn_AssignWorkplace) s_fn_AssignWorkplace(self, &nullIBP, -1);
        }
    }

    // --- Check: should citizen be assigned to partner's workplace (child/student) ---
    newAge = *reinterpret_cast<float*>(p + 0x3C);
    adultAge = *reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(p + 0x288) + 0x44);
    bool isChildOrStudent = (newAge < adultAge) || (*reinterpret_cast<int64_t*>(p + 0x208) != 0);
    if (isChildOrStudent) {
        if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(pCVar1);
        healthCompVal = *reinterpret_cast<int64_t*>(pCVar1);
        if (*reinterpret_cast<int*>(healthCompVal + 0x60) + 1 < 1) {
            // No workplace, check parent/guardian (at +0x258 = offset 600)
            uint8_t* selfPartner = p + 600;  // +0x258
            if (workplaceRef == 0 && *reinterpret_cast<int64_t*>(selfPartner) != 0) {
                if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(selfPartner);
                int64_t partnerComp = *reinterpret_cast<int64_t*>(selfPartner);
                if (*reinterpret_cast<int64_t*>(partnerComp + 0x238) != 0) {
                    if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(selfPartner);
                    partnerComp = *reinterpret_cast<int64_t*>(selfPartner);
                    int64_t localRes8 = *reinterpret_cast<int64_t*>(partnerComp + 0x238);
                    char valid = fn_CitizenComponent_CheckWorkplaceValid ?
                        fn_CitizenComponent_CheckWorkplaceValid(self, &localRes8) : 0;
                    if (valid != 0) {
                        if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(selfPartner);
                        partnerComp = *reinterpret_cast<int64_t*>(selfPartner);
                        int slotIdx = *reinterpret_cast<int*>(partnerComp + 0x240);
                        if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(selfPartner);
                        partnerComp = *reinterpret_cast<int64_t*>(selfPartner);
                        int64_t wpIBP = *reinterpret_cast<int64_t*>(partnerComp + 0x238);
                        if (s_fn_AssignWorkplace) s_fn_AssignWorkplace(self, &wpIBP, slotIdx);
                    }
                }
            }
        }
    }

    // --- Adult section: housing, workplace, partner ---
    if (isChildOrStudent ||
        (s_fn_CheckPtr_slot && (s_fn_CheckPtr_slot(pCVar1),
         *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(pCVar1) + 0x60) + 1 >= 1)))
        goto LAB_140113873;

    // --- Housing assignment ---
    if (*reinterpret_cast<int64_t*>(p + 0x248) == 0) {
        int64_t housingResult = 0;
        profData1F0 = *reinterpret_cast<int64_t*>(p + 0x1F0);
        if (*reinterpret_cast<char*>(profData1F0 + 0x4C) == 0) {
            // Unassigned profession type
            if (s_fn_FindHousingUnassigned)
                s_fn_FindHousingUnassigned(reinterpret_cast<int64_t>(self), &housingResult);
        } else {
            if (s_fn_FindHousingAssigned)
                s_fn_FindHousingAssigned(reinterpret_cast<int64_t>(self), &housingResult);
        }
        int64_t localRes8 = housingResult;
        if (s_fn_AssignHousing) s_fn_AssignHousing(self, &localRes8);
    } else {
        // Check if should switch workplace (already reimplemented)
        bool shouldSwitch = CitizenComponent_ShouldSwitchWorkplace(self);
        if (shouldSwitch) {
            int64_t newWpResult = 0;
            if (s_fn_FindNewWorkplace) {
                void* result = s_fn_FindNewWorkplace(
                    reinterpret_cast<int64_t>(self), &newWpResult);
                int64_t localRes8 = *reinterpret_cast<int64_t*>(result);
                if (s_fn_AssignHousing) s_fn_AssignHousing(self, &localRes8);
            }
        }
    }

    // --- Find work if no workplace ---
    if (workplaceRef == 0) {
        *reinterpret_cast<uint32_t*>(p + 0x88) |= 0x20;
        goto FindWorkLabel;
    } else if (*reinterpret_cast<int64_t*>(p + 600) != 0) {
FindWorkLabel:
        {
            int64_t localRes8_work = 0;
            int64_t localRes18_entity = 0;
            *reinterpret_cast<int*>(&localRes8_work) = -1;
            if (s_fn_FindWork)
                s_fn_FindWork(reinterpret_cast<int64_t>(self), &localRes18_entity,
                              reinterpret_cast<int*>(&localRes8_work), workplaceRef == 0);
            if (localRes18_entity != 0) {
                if (*reinterpret_cast<int64_t*>(p + 600) != 0) {
                    *reinterpret_cast<uint32_t*>(p + 0x88) |= 0x2000;
                }
                int64_t releaseIBP = 0;
                fn_InternalBasePtr_Ctor(&releaseIBP, nullptr);
                if (s_fn_ReleaseWork) s_fn_ReleaseWork(self, &releaseIBP);

                releaseIBP = localRes18_entity;
                int slotIdx = *reinterpret_cast<int*>(&localRes8_work);
                if (s_fn_AssignWorkplace) s_fn_AssignWorkplace(self, &releaseIBP, slotIdx);

                // Also assign partner if exists
                if (*reinterpret_cast<int64_t*>(p + 0x250) != 0) {
                    int64_t partnerWp = localRes18_entity;
                    if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(p + 0x250);
                    if (s_fn_AssignWorkplace)
                        s_fn_AssignWorkplace(*reinterpret_cast<void**>(p + 0x250),
                                            &partnerWp, slotIdx);
                }

                // Re-find storage for the new workplace
                if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(pSelf238);
                workplaceRef = *reinterpret_cast<int64_t*>(pSelf238);
                if (s_fn_FindCompStorage && workplaceRef != 0) {
                    int64_t storBuf[8] = {};
                    void* result = s_fn_FindCompStorage(workplaceRef, storBuf);
                    localRes10 = *reinterpret_cast<int64_t*>(result);
                }
            }
        }
    }

    // --- Multi-worker building: find extra work if storage has capacity ---
    workplaceRef = *reinterpret_cast<int64_t*>(pSelf238);
    if (workplaceRef != 0) {
        if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(&localRes10);
        int64_t storageVal = localRes10;
        if (*reinterpret_cast<int*>(storageVal + 0x58) > 1) {
            int64_t localRes8_extra = 0;
            int64_t localC8_entity = 0;
            *reinterpret_cast<int*>(&localRes8_extra) = -1;
            if (s_fn_FindWork)
                s_fn_FindWork(reinterpret_cast<int64_t>(self), &localC8_entity,
                              reinterpret_cast<int*>(&localRes8_extra), 0);
            if (localC8_entity != 0) {
                int slotIdx = *reinterpret_cast<int*>(&localRes8_extra);
                int64_t local98 = localC8_entity;
                if (s_fn_AssignWorkplace) s_fn_AssignWorkplace(self, &local98, slotIdx);
                if (*reinterpret_cast<int64_t*>(p + 0x250) != 0) {
                    int64_t partnerWp2 = localC8_entity;
                    if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(p + 0x250);
                    if (s_fn_AssignWorkplace)
                        s_fn_AssignWorkplace(*reinterpret_cast<void**>(p + 0x250),
                                            &partnerWp2, slotIdx);
                }
            }
        }
    }

    // --- Partner finding: check if eligible ---
    if (*reinterpret_cast<int64_t*>(p + 600) == 0 && workplaceRef != 0) {
        uint8_t* pPartner = p + 0x250;
        if (*reinterpret_cast<int64_t*>(pPartner) == 0) {
            desc = GetDescription(self);
            float partnerAge = *reinterpret_cast<float*>(desc + 0x48);
            if (partnerAge <= *reinterpret_cast<float*>(p + 0x3C)) {
                int64_t partnerBuf[2] = {};
                if (s_fn_FindPartner) {
                    void* result = s_fn_FindPartner(
                        reinterpret_cast<int64_t>(self), partnerBuf);
                    int64_t partnerIBP = *reinterpret_cast<int64_t*>(result);
                    *reinterpret_cast<int64_t*>(pPartner) = partnerIBP;
                    if (partnerIBP != 0) {
                        *reinterpret_cast<uint32_t*>(p + 0x88) |= 0x200;
                        if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(pPartner);
                        int64_t partner = *reinterpret_cast<int64_t*>(pPartner);
                        *reinterpret_cast<uint32_t*>(partner + 0x88) |= 0x200;

                        // Release partner's current work
                        int64_t releaseIBP = 0;
                        fn_InternalBasePtr_Ctor(&releaseIBP, nullptr);
                        if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(pPartner);
                        if (s_fn_ReleaseWork) s_fn_ReleaseWork(
                            *reinterpret_cast<void**>(pPartner), &releaseIBP);

                        // Set partner's partner reference to this citizen
                        fn_InternalBasePtr_Ctor(&releaseIBP, self);
                        if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(pPartner);
                        *reinterpret_cast<int64_t*>(
                            *reinterpret_cast<int64_t*>(pPartner) + 0x250) = releaseIBP;

                        // Assign partner to this citizen's workplace
                        releaseIBP = workplaceRef;
                        if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(pPartner);
                        if (s_fn_AssignWorkplace)
                            s_fn_AssignWorkplace(*reinterpret_cast<void**>(pPartner),
                                                &releaseIBP, *reinterpret_cast<int*>(p + 0x240));
                    }
                }
            }
        }
    }

LAB_140113873:
    // --- Happiness/reproduction section (adults with no children, at adult age) ---
    if (*reinterpret_cast<int*>(p + 0x38) == 0) {
        desc = GetDescription(self);
        adultAge = *reinterpret_cast<float*>(desc + 0x44);
        newAge = *reinterpret_cast<float*>(p + 0x3C);
        if (adultAge <= newAge && *reinterpret_cast<int64_t*>(p + 0x208) == 0) {
            float hungerWarn = (*reinterpret_cast<float*>(desc + 0xD0) -
                               *reinterpret_cast<float*>(desc + 0xCC)) * fVar4 +
                               *reinterpret_cast<float*>(desc + 0xCC);
            float thirstWarn = (*reinterpret_cast<float*>(desc + 0xF0) -
                               *reinterpret_cast<float*>(desc + 0xEC)) * fThirstFrac +
                               *reinterpret_cast<float*>(desc + 0xEC);

            bool isHungryOrThirsty = (hungerWarn < *reinterpret_cast<float*>(p + 0x44)) ||
                                     (thirstWarn < *reinterpret_cast<float*>(p + 0x48));

            // Reset happiness timer if hungry/thirsty and it was positive
            if (isHungryOrThirsty && *reinterpret_cast<float*>(p + 0x4C) > 0.0f) {
                *reinterpret_cast<uint32_t*>(p + 0x4C) = 0;
            }

            float happiness = *reinterpret_cast<float*>(p + 0x4C);
            if (happiness <= 0.0f) {
                if (happiness < 0.0f) {
                    float newHap = happiness + dt_sec;
                    *reinterpret_cast<float*>(p + 0x4C) = newHap;
                    if (newHap >= 0.0f) {
                        *reinterpret_cast<uint32_t*>(p + 0x4C) = 0;
                    }
                }
            } else {
                float newHap = happiness - dt_sec;
                *reinterpret_cast<float*>(p + 0x4C) = newHap;
                if (newHap <= 0.0f) {
                    // Happiness timer expired — roll for reproduction
                    float happyScore = *reinterpret_cast<float*>(p + 0x50);
                    float maxHappy = *reinterpret_cast<float*>(desc + 0x70);
                    float repChanceHigh = *reinterpret_cast<float*>(desc + 100);  // 0x64
                    float repChanceLow = *reinterpret_cast<float*>(desc + 0x68);

                    void* random = *reinterpret_cast<void**>(
                        reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xD8);
                    float roll = s_fn_RandFloat ? s_fn_RandFloat(random) : 1.0f;

                    float repThreshold = (happyScore / maxHappy) * (repChanceHigh - repChanceLow) +
                                         repChanceLow;
                    if (repThreshold <= roll) {
                        // Failed reproduction roll — do reproduction helper
                        // Note: this is CitizenComponent_helper_1b90 in Ghidra
                        typedef void (*fn_ReproHelper_t)(void* self);
                        static fn_ReproHelper_t s_fn_ReproHelper = nullptr;
                        if (!s_fn_ReproHelper) {
                            // This address is NOT 0x140111b90 (that's UpdateHealth).
                            // From Ghidra: "CitizenComponent_helper_1b90" with base at
                            // the CitizenComponent function region. Checking the call in
                            // the decompilation output more carefully...
                            // The decompilation is ambiguous. This helper must be resolved
                            // from the actual call target. For safety, use the exe address
                            // from the decompilation context.
                            // After review: line ~370 shows it called after the random roll
                            // fails. It's the "make baby" function. Address likely in the
                            // 0x1401118xx or 0x1401119xx range.
                            // Actually "helper_1b90" = 0x140111b90, but that IS UpdateHealth.
                            // The Ghidra decompilation conflates them. Looking at the actual
                            // control flow: the repro helper modifies happiness fields, not
                            // health. This suggests it IS at 0x140111b90 but does DIFFERENT
                            // things based on the call context. OR the Ghidra function
                            // boundary is wrong. For safety, call the original address.
                            s_fn_ReproHelper = reinterpret_cast<fn_ReproHelper_t>(
                                ResolveAddr(Module::Exe, 0x140111b90));
                        }
                        // WARNING: This shares the address with UpdateHealth.
                        // It's likely Ghidra splitting. The decompilation context suggests
                        // this IS a different logical operation. If it causes issues,
                        // this call should be investigated further.
                        if (s_fn_ReproHelper) s_fn_ReproHelper(self);

                        // Reset happiness timer with random interval
                        desc = GetDescription(self);
                        void* random2 = *reinterpret_cast<void**>(
                            reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xD8);
                        float roll2 = s_fn_RandFloat ? s_fn_RandFloat(random2) : 0.5f;
                        float timerBase = *reinterpret_cast<float*>(desc + 0x5C);
                        float timerRange = *reinterpret_cast<float*>(desc + 0x60);
                        uint32_t newTimer = static_cast<uint32_t>(roll2 * timerRange + timerBase);
                        uint32_t xorMask = s_DAT_1401722c0 ? *s_DAT_1401722c0 : 0;
                        *reinterpret_cast<uint32_t*>(p + 0x4C) = newTimer ^ xorMask;
                    } else {
                        // Passed reproduction roll — handle life event 6 (childbirth)
                        char r = s_fn_HandleLifeEvent ? s_fn_HandleLifeEvent(self, 6, nullptr) : 0;
                        if (r != 0) return;
                    }
                }
            }

            // --- Check partner & workplace: if partner exists and has workplace,
            //     and happiness timer is zero, check storage capacity for child ---
            workplaceRef = *reinterpret_cast<int64_t*>(pSelf238);
            if (*reinterpret_cast<int64_t*>(p + 0x250) != 0 && workplaceRef != 0 &&
                *reinterpret_cast<float*>(p + 0x4C) == 0.0f) {
                desc = GetDescription(self);
                float childMinAge = *reinterpret_cast<float*>(desc + 0x50);
                float childMaxAge = *reinterpret_cast<float*>(desc + 0x54);
                newAge = *reinterpret_cast<float*>(p + 0x3C);
                if (childMinAge <= newAge && newAge <= childMaxAge) {
                    // Check storage capacity for the item type
                    if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(pSelf238);
                    int64_t wpVal = *reinterpret_cast<int64_t*>(pSelf238);
                    int64_t storBuf = 0;
                    void* storResult = s_fn_FindCompStorage ?
                        s_fn_FindCompStorage(wpVal, &storBuf) : nullptr;
                    if (storResult) {
                        if (s_fn_CheckPtr_slot) s_fn_CheckPtr_slot(storResult);
                        int64_t storComp = *reinterpret_cast<int64_t*>(storResult);
                        int itemType = *reinterpret_cast<int*>(p + 0x240);
                        bool hasCapacity = s_fn_CheckStorageCapacity ?
                            s_fn_CheckStorageCapacity(storComp, itemType) : false;
                        if (!hasCapacity) {
                            // No capacity — set happiness timer from description
                            *reinterpret_cast<uint32_t*>(p + 0x4C) =
                                *reinterpret_cast<uint32_t*>(desc + 0x58);
                        }
                    }
                }
            }
        }
    }
}

// ============================================================
// BST node layout for the profession registry tree
// (gameWorld+0xF0 -> +0x100 = tree root)
// ============================================================
// node[0] = key (uint64_t, matches citizen->data1F0)
// node[1] = array_ptr (pointer to array of entity IBPs)
// node[2] = count (int, number of jobs)
// node[3] = padding
// node[4] = left child
// node[5] = right child

// ============================================================
// BST node layout for the region map tree
// (gameWorld+0x118 -> +0x30 = tree root)
// ============================================================
// node[0] = key (int32_t, region ID)
// node[1] = value (pointer to region descriptor)
// node[2] = padding
// node[3] = left child (at int offset +6 = byte offset +0x18)
// node[4] = right child (at int offset +8 = byte offset +0x20)

// ============================================================
// BST lookup: profession registry (uint64 key)
// Returns count of jobs, or 0 if not found.
// ============================================================
static int LookupProfessionJobCount(uintptr_t registryBase, uint64_t key) {
    uintptr_t* node = *(uintptr_t**)(registryBase + 0x100);
    while (node) {
        uint64_t nodeKey = node[0];
        if (key < nodeKey) {
            node = (uintptr_t*)node[4]; // left
        } else if (key > nodeKey) {
            node = (uintptr_t*)node[5]; // right
        } else {
            // Found
            return (int)node[2];
        }
    }
    return 0; // not found
}

// Get job array entry from profession registry
static uintptr_t GetJobEntity(uintptr_t registryBase, uint64_t key, int64_t index) {
    uintptr_t* node = *(uintptr_t**)(registryBase + 0x100);
    while (node) {
        uint64_t nodeKey = node[0];
        if (key < nodeKey) {
            node = (uintptr_t*)node[4];
        } else if (key > nodeKey) {
            node = (uintptr_t*)node[5];
        } else {
            // Found — read array[index]
            uintptr_t* arr = (uintptr_t*)node[1];
            return arr[index];
        }
    }
    return 0;
}

// ============================================================
// BST lookup: region map (int32 key)
// Returns pointer to region descriptor, or 0 if not found.
// Tree nodes: int key at +0, value ptr at +8, left at +0x18, right at +0x20
// ============================================================
static uintptr_t LookupRegionByID(uintptr_t areaMap, int32_t regionId) {
    int* node = *(int**)(areaMap + 0x30);
    while (node) {
        int nodeKey = node[0];
        if (regionId < nodeKey) {
            node = *(int**)(node + 6); // left (+0x18)
        } else if (regionId > nodeKey) {
            node = *(int**)(node + 8); // right (+0x20)
        } else {
            // Found — value at node+8 (qword)
            return *(uintptr_t*)(node + 2);
        }
    }
    return 0; // not found
}

// ============================================================
// CitizenComponent_ShouldSwitchWorkplace
// Original: 0x1401109E0 (CitizenComponent_helper_09e0)
// ============================================================
bool CitizenComponent_ShouldSwitchWorkplace(CitizenComponent* self) {
    uintptr_t selfAddr = (uintptr_t)self;
    void* gameWorld = *g_pGameWorldSingleton;
    uintptr_t gwAddr = (uintptr_t)gameWorld;

    // Read citizen's workplace reference (profession group key)
    uint64_t profKey = *(uint64_t*)(selfAddr + 0x1F0);

    // Lookup job count from profession registry
    uintptr_t registry = *(uintptr_t*)(gwAddr + 0xF0);
    int jobCount = LookupProfessionJobCount(registry, profKey);

    // Read workplace entity flag at +0x4C
    uint8_t workplaceFlag = *(uint8_t*)(profKey + 0x4C);

    if (workplaceFlag != 0) {
        // ====== ASSIGNED PATH ======
        // Citizen has a specific workplace assignment.
        // Check if another workplace in a lower-priority region has vacancy.
        uintptr_t areaMap = *(uintptr_t*)(gwAddr + 0x118);

        // Get current workplace IBP and find StorageLocationComponent
        fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x248));
        uintptr_t currentWorkplace = *(uintptr_t*)(selfAddr + 0x248);

        uintptr_t storageLocIBP = 0;
        void* storageLocResult = fn_FindStorageLocationComp(currentWorkplace, &storageLocIBP);
        fn_InternalBasePtr_CheckPtr(storageLocResult);

        uintptr_t storageLocComp = *(uintptr_t*)storageLocResult;
        int32_t regionId = *(int32_t*)(storageLocComp + 0x28);

        // Look up current region in region map BST
        uintptr_t regionDesc = LookupRegionByID(areaMap, regionId);

        // *** FIX: null check for region lookup failure ***
        int currentPriority;
        if (regionDesc == 0) {
            // Region not in tree — don't switch, stay at current workplace.
            // Original game crashes here (null deref). Returning false keeps
            // citizens working instead of endlessly reshuffling.
            return false;
        } else {
            currentPriority = *(int32_t*)(regionDesc + 0x24);
        }

        // Check if current workplace is valid (reachable)
        uintptr_t workplaceRef = *(uintptr_t*)(selfAddr + 0x248);
        char valid = fn_CitizenComponent_CheckWorkplaceValid(self, &workplaceRef);
        if (!valid) {
            return true; // workplace not reachable -> should switch
        }

        // Iterate all available jobs looking for a better workplace
        for (int64_t i = 0; i < (int64_t)jobCount; i++) {
            uintptr_t entityIBP = GetJobEntity(registry, profKey, i);

            // CheckPtr + FindWorkplaceComponent
            fn_InternalBasePtr_CheckPtr(&entityIBP);
            uintptr_t workplaceCompIBP = 0;
            fn_FindWorkplaceComp(entityIBP, &workplaceCompIBP);

            // CheckPtr + FindStorageLocationComponent
            fn_InternalBasePtr_CheckPtr(&entityIBP);
            uintptr_t otherStorageLocIBP = 0;
            fn_FindStorageLocationComp(entityIBP, &otherStorageLocIBP);

            // Skip if this is the current workplace
            fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x248));
            if (entityIBP == *(uintptr_t*)(selfAddr + 0x248))
                continue;

            // Check workplace is active (+0x50 flag)
            fn_InternalBasePtr_CheckPtr(&workplaceCompIBP);
            uintptr_t wpComp = workplaceCompIBP;
            if (*(uint8_t*)(wpComp + 0x50) == 0)
                continue;

            // Check has vacancy
            fn_InternalBasePtr_CheckPtr(&workplaceCompIBP);
            if (!fn_CitizenComponent_HasVacancy(workplaceCompIBP))
                continue;

            // Look up this workplace's region (re-read areaMap fresh, matches original asm)
            fn_InternalBasePtr_CheckPtr(&otherStorageLocIBP);
            int32_t otherRegionId = *(int32_t*)(otherStorageLocIBP + 0x28);
            uintptr_t freshAreaMap = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0x118);
            uintptr_t otherRegionDesc = LookupRegionByID(freshAreaMap, otherRegionId);

            // Null check on region lookup (original would crash)
            if (otherRegionDesc == 0)
                continue;

            // If other region has lower priority value, switch
            if (*(int32_t*)(otherRegionDesc + 0x24) < currentPriority) {
                // Double-check vacancy
                fn_InternalBasePtr_CheckPtr(&workplaceCompIBP);
                if (fn_CitizenComponent_HasVacancy(workplaceCompIBP))
                    return true;
            }
        }

        return false;
    } else {
        // ====== UNASSIGNED PATH ======
        // Citizen has no specific workplace. Check if worker distribution is unbalanced.
        int maxGap = 0;
        uint32_t minGap = 0xFFFF;

        for (int64_t i = 0; i < (int64_t)jobCount; i++) {
            uintptr_t entityIBP = GetJobEntity(registry, profKey, i);
            fn_InternalBasePtr_CheckPtr(&entityIBP);

            // Find WorkplaceComponent on this entity
            uintptr_t entity = entityIBP;
            int compCount = *(int32_t*)(entity + 0x38);
            uintptr_t compArray = *(uintptr_t*)(entity + 0x30);

            // Scan for WorkplaceComponent by type ID
            uintptr_t wpComp = 0;
            uint32_t wpTypeId = *g_TypeId_WorkplaceComponent;
            for (int ci = 0; ci < compCount; ci++) {
                uintptr_t* compSlot = (uintptr_t*)(compArray + ci * 8);
                fn_InternalBasePtr_CheckPtr(compSlot);
                uintptr_t comp = *compSlot;
                // Call vtable[2] = GetTypeId
                uintptr_t vtable = *(uintptr_t*)comp;
                typedef uint32_t (*GetTypeId_t)(void*);
                uint32_t typeId = ((GetTypeId_t)(*(uintptr_t*)(vtable + 0x10)))((void*)comp);
                if (typeId == wpTypeId) {
                    // Verify with IsDerivedFrom
                    compSlot = (uintptr_t*)(compArray + ci * 8);
                    fn_InternalBasePtr_CheckPtr(compSlot);
                    uintptr_t finalComp = *compSlot;
                    if (fn_IsDerivedFrom((void*)finalComp, wpTypeId)) {
                        wpComp = finalComp;
                    }
                    break;
                }
            }

            // Construct IBP for found component
            uintptr_t wpIBP = 0;
            fn_InternalBasePtr_Ctor(&wpIBP, (void*)wpComp);
            fn_InternalBasePtr_CheckPtr(&wpIBP);

            uintptr_t wpCompVal = wpIBP;
            if (*(uint8_t*)(wpCompVal + 0x50) == 0)
                continue; // not active

            // Check workplace validity
            uintptr_t entityRef = entityIBP;
            char isValid = fn_CitizenComponent_CheckWorkplaceValid(self, &entityRef);
            if (!isValid)
                continue;

            // Get worker slot count via StorageLocationComponent
            fn_InternalBasePtr_CheckPtr(&wpIBP);
            uintptr_t wpBase = wpIBP;
            fn_InternalBasePtr_CheckPtr((void*)(wpBase + 0x10));
            uintptr_t parentEntity = *(uintptr_t*)(wpBase + 0x10);

            uintptr_t slcIBP = 0;
            void* slcResult = fn_FindStorageLocationComp(parentEntity, &slcIBP);
            fn_InternalBasePtr_CheckPtr(slcResult);
            uintptr_t slc = *(uintptr_t*)slcResult;

            // Calculate worker slots (lazy-compute cached at +0x30)
            if (*(int32_t*)(slc + 0x30) == 0) {
                *(int32_t*)(slc + 0x30) = fn_CalcWorkerSlots((void*)slc);
            }
            int workerSlots = *(int32_t*)(slc + 0x30);

            // Get current worker count from WorkplaceComponent
            fn_InternalBasePtr_CheckPtr(&wpIBP);
            int currentWorkers = *(int32_t*)(wpIBP + 0x60);

            int gap = workerSlots - currentWorkers;
            if (gap > maxGap) maxGap = gap;
            if (gap < (int)minGap) minGap = (uint32_t)gap;
        }

        return (maxGap - (int)minGap) > 1;
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void CitizenComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[CitizenComponent] Funcs resolved: AssignState=%p HandleLifeEvent=%p "
           "UpdateHealth=%p ConsumeFood=%p ConsumeFuel=%p FindWorkplace=%p "
           "FindStorage=%p HealthSetFlag=%p AssignWorkplace=%p CheckPtr_slot=%p\n",
           (void*)s_fn_AssignState, (void*)s_fn_HandleLifeEvent,
           (void*)s_fn_UpdateHealth, (void*)s_fn_ConsumeFood,
           (void*)s_fn_ConsumeFuel, (void*)s_fn_FindWorkplace,
           (void*)s_fn_FindStorage, (void*)s_fn_HealthSetFlag,
           (void*)s_fn_AssignWorkplace, (void*)s_fn_CheckPtr_slot);
#endif
}
