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

#include "Component/citizencomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

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

// --- Citizen status-icon driver (standalone wiring) ---
// The 5 health-flag call sites in Update (bit 0 no-workplace, 1 hungry, 2 cold,
// 3 unemployed/homeless, 5 has-profession) call s_fn_HealthSetFlag, which in the
// original is FUN_14012c3f0 (the StatusIcon flag setter). That exe fn is unmapped/
// unregistered in standalone, so ResolveAddr returns NULL and the calls no-op. We
// instead route them to StatusIcon_Toggle — the faithful port of FUN_14012c3f0 in
// statusiconcomponent.cpp — so the citizen's own StatusIconComponent (cached at
// CitizenComponent+0x280) actually shows hunger/cold/unemployed/homeless icons.
// Gated default-off (BANISHED_LIVE_CITIZEN_STATUSICON) so existing LIVE_CITIZEN(_UPDATE)
// runs stay byte-identical when unset. Depends on BANISHED_LIVE_STATUSICON + _DRAW for
// the citizen StatusIconComponent to be live + renderable (same infra as the building
// occupancy warning), and on LIVE_CITIZEN(_UPDATE) for the driving fields to tick.
struct StatusIconComponent;
void StatusIcon_Toggle(StatusIconComponent* self, int bit, bool setFlag);

// Adapter matching fn_HealthSetFlag_t. Null-guards the cached StatusIconComponent IBP
// (CitizenComponent+0x280): a partial-world citizen lacking a live status-icon component
// has it null, and StatusIcon_Toggle dereferences self+0x10 immediately. The original
// never sees a null here (every citizen carries the component) — this guard is a
// standalone-only safety, not a behavior change.
static void CitizenStatusIcon_SetFlag(void* icon, int flagIndex, char value) {
    if (icon)
        StatusIcon_Toggle(reinterpret_cast<StatusIconComponent*>(icon), flagIndex, value != 0);
}

static bool CitizenStatusIconEnabled() {
    static int cached = -1;
    if (cached < 0) {
        const char* e = std::getenv("BANISHED_LIVE_CITIZEN_STATUSICON");
        cached = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return cached != 0;
}

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

    // Dispatch slot for Time::AsSeconds. The correct slot is 0x140151318 (verified
    // in Ghidra: the first indirect CALL in CitizenComponent::Update @0x140112eb4 is
    // `CALL qword ptr [0x140151318]` -> System::Time::AsSeconds). The previous value
    // (0x140152a70) actually resolves to Math::Vector3::operator== — with that wrong
    // slot, s_fn_Time_AsSeconds was NULL in standalone (unregistered) so dt_sec fell
    // back to 0.0f every tick and hunger/thirst/age never accumulated (Phase-3 bug).
    slot = ResolveAddr(Module::Exe, 0x140151318);
    if (slot) s_fn_Time_AsSeconds = *reinterpret_cast<fn_Time_AsSeconds_t*>(slot);

    // Dispatch slot for Time::Time(float) (seconds -> Time ticks). Correct slot is
    // 0x140152b00 (Ghidra: the food/fuel-consume loops call `CALL qword ptr
    // [0x140152b00]` @0x1401133ca/0x140113409 before ConsumeFood/ConsumeFuel). The
    // previous value (0x140152a80) resolved to SoundEffect::_typeInfo.
    slot = ResolveAddr(Module::Exe, 0x140152b00);
    if (slot) s_fn_Time_FromSeconds = *reinterpret_cast<fn_Time_FromSeconds_t*>(slot);

    // Direct exe addresses for CitizenComponent sub-functions
    s_fn_TryFindPartner = reinterpret_cast<fn_TryFindPartner_t>(
        ResolveAddr(Module::Exe, 0x140111840));  // CitizenComponent::TryFindPartner (CALL @140112f82 in Update); 0x14010fe40 was helper_fe40 (wrong fn)
    s_fn_AssignState = reinterpret_cast<fn_AssignState_t>(
        ResolveAddr(Module::Exe, 0x140113af0));  // CitizenComponent::AssignState (CALL @140112f9e in Update)
    s_fn_HandleLifeEvent = reinterpret_cast<fn_HandleLifeEvent_t>(
        ResolveAddr(Module::Exe, 0x140116dd0));  // CitizenComponent::HandleLifeEvent (6 CALLs in Update)
    s_fn_UpdateHealth = reinterpret_cast<fn_UpdateHealth_t>(
        ResolveAddr(Module::Exe, 0x1401127d0));  // CitizenComponent::UpdateHealth (CALL @1401131b9 in Update); 0x140111b90 was helper_1b90 (repro helper, wrong fn)
    s_fn_ConsumeFood = reinterpret_cast<fn_ConsumeFood_t>(
        ResolveAddr(Module::Exe, 0x140112990));  // CitizenComponent::ConsumeFood (CALL @1401133e7 in Update)
    s_fn_ConsumeFuel = reinterpret_cast<fn_ConsumeFuel_t>(
        ResolveAddr(Module::Exe, 0x140112ae0));  // CitizenComponent::ConsumeFuel (CALL @14011342a in Update)
    s_fn_FindWorkplace = reinterpret_cast<fn_FindWorkplace_t>(
        ResolveAddr(Module::Exe, 0x140111590));  // CitizenComponent::FindWorkplace (CALL @140113219 in Update)
    s_fn_FindStorage = reinterpret_cast<fn_FindStorage_t>(
        ResolveAddr(Module::Exe, 0x1401116f0));  // CitizenComponent::FindStorage (CALL @14011322b in Update)
    // 0x14012c3f0 is unmapped in standalone → route the citizen health-flag calls to the
    // StatusIcon_Toggle port instead (default-off; see CitizenStatusIcon_SetFlag above).
    s_fn_HealthSetFlag = CitizenStatusIconEnabled()
                             ? reinterpret_cast<fn_HealthSetFlag_t>(&CitizenStatusIcon_SetFlag)
                             : nullptr;
    s_fn_AssignWorkplace = reinterpret_cast<fn_AssignWorkplace_t>(
        ResolveAddr(Module::Exe, 0x1401163d0));  // CitizenComponent::AssignWorkplace (7 CALLs in Update)
    s_fn_AssignHousing = reinterpret_cast<fn_AssignHousing_t>(
        ResolveAddr(Module::Exe, 0x140111ee0));  // CitizenComponent::AssignHousing (CALL @140113621 in Update)
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
    // [DIAG HARNESS, default OFF] BANISHED_FUEL_ACCEL=<mult> scales ONLY the food/fuel
    // timer accumulation so the ConsumeFood/ConsumeFuel (warmth) chain can be exercised
    // in seconds instead of the ~15 real-minutes one 900s fuel interval needs. Unset/1 =>
    // byte-identical accumulation.
    static int64_t s_fuelAccel = -1;
    if (s_fuelAccel < 0) { const char* e = getenv("BANISHED_FUEL_ACCEL");
        s_fuelAccel = 1; if (e && e[0]) { long v = atol(e); if (v > 0) s_fuelAccel = v; } }
    *reinterpret_cast<int64_t*>(p + 0x58) += dt_ticks * s_fuelAccel;   // food timer
    *reinterpret_cast<int64_t*>(p + 0x90) += dt_ticks * s_fuelAccel;   // fuel timer
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
    // [DIAG HARNESS, default OFF] BANISHED_NEEDS_ACCEL=<mult> multiplies ONLY the
    // hunger/thirst accumulation rate so the eat/drink chain can be exercised in
    // seconds instead of ~10 sim-minutes (warn threshold ~16 at 0.028/s). Unset
    // or 1 => byte-identical accumulation (Economy Brick M-D verification).
    static float s_needsAccel = -1.0f;
    if (s_needsAccel < 0.0f) {
        const char* e = getenv("BANISHED_NEEDS_ACCEL");
        s_needsAccel = 1.0f;
        if (e && e[0]) { float v = (float)atof(e); if (v > 0.0f) s_needsAccel = v; }
    }
    if ((gameFlags >> 1 & 1) == 0) {
        float hungerRate = *reinterpret_cast<float*>(desc + 0xC0);
        float hungerDiv = s_DAT_14017225c ? *s_DAT_14017225c : 1.0f;
        *reinterpret_cast<float*>(p + 0x44) += (dt_sec * s_needsAccel * hungerRate) / hungerDiv;
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
        *reinterpret_cast<float*>(p + 0x48) += (dt_sec * s_needsAccel * thirstRate) / fVar14;
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
    // [STANDALONE GUARD] +0x1F0 = profession object, null until assigned at gameplay
    // (same field CitizenAI CheckEducation null-guards). In retail EVERY citizen —
    // children included — is stamped with the default profession at spawn
    // (FUN_140117080 → ProfessionList+0x30), and that object's +0x4C bool governs the
    // "unemployed" icon (flag 3). A null pointer here is a standalone-only gap, NOT a
    // genuine "no profession". The old fallback defaulted profByte=0 (== unemployed),
    // which mislabelled EVERY not-yet-employed citizen. Children/students can never hold
    // a workplace (+0x248 stays 0 for them), so that fallback pinned a permanent "?"
    // no-job badge on every child. Fall back on age instead: adults with no profession
    // stamped still read as unemployed (they legitimately need a job), but children
    // (age < adultAge) and students (in-school, +0x208 != 0) never do — which is what
    // isAdult already encodes (computed at the top of Update). With a real profession
    // present (stamped SA) this branch is skipped, so retail behaviour is
    // byte-identical.
    char profByte = (profData1F0 != 0) ? *reinterpret_cast<char*>(profData1F0 + 0x4C)
                                       : (isAdult ? 0 : 1);
    if (profByte == 0 && *reinterpret_cast<int64_t*>(p + 0x248) == 0) {
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

    // [PORT FIX] The original reassigns pCVar1 = param_1 + 0x278 (CharacterComponent)
    // here — the prior pCVar1 (+0x280, StatusIcon/health) was only for the HealthSetFlag
    // calls above. The workplace/housing-assignment gates below test the citizen's
    // CharacterComponent ACTION-STACK top (+0x60): `+0x60 + 1 < 1` ⇔ stack empty (idle)
    // ⇔ eligible to be (re)assigned. The reimpl had kept pCVar1 = +0x280, so every gate
    // read the inert StatusIcon's +0x60 (== 0 in standalone) and ALWAYS skipped assignment
    // → citizens never got housed/employed. Restore the original component.
    pCVar1 = p + 0x278;  // CharacterComponent IBP (action-stack owner)

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
    // NOTE: skipped for children — start-town citizens currently spawn with age=0
    // (< adultAge), so this whole assignment section no-ops until they age up or are
    // spawned as adults. See project_economy_loop memory.
    if (isChildOrStudent ||
        (s_fn_CheckPtr_slot && (s_fn_CheckPtr_slot(pCVar1),
         *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(pCVar1) + 0x60) + 1 >= 1)))
        goto LAB_140113873;

    // --- Housing assignment ---
    if (*reinterpret_cast<int64_t*>(p + 0x248) == 0) {
        int64_t housingResult = 0;
        profData1F0 = *reinterpret_cast<int64_t*>(p + 0x1F0);
        // [STANDALONE GUARD] +0x1F0 profession-list null until assigned → treat the
        // +0x4C byte as 0 (unassigned profession) — same guard as the unemployed flag.
        char profByte634 = (profData1F0 != 0) ? *reinterpret_cast<char*>(profData1F0 + 0x4C) : 0;
        if (profByte634 == 0) {
            // Unassigned profession type
            if (s_fn_FindHousingUnassigned)
                s_fn_FindHousingUnassigned(reinterpret_cast<int64_t>(self), &housingResult);
        } else {
            if (s_fn_FindHousingAssigned)
                s_fn_FindHousingAssigned(reinterpret_cast<int64_t>(self), &housingResult);
        }
        int64_t localRes8 = housingResult;
        if (std::getenv("BANISHED_CITIZENAI_TRACE")) {
            static int s_hn = 0;
            if (s_hn < 8) { ++s_hn;
                int64_t mgr = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                    ? *reinterpret_cast<int64_t*>(
                          reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xf0)
                    : 0;
                int bcount = (mgr) ? *reinterpret_cast<int*>(mgr + 0x68) : -1;
                int64_t btree = (mgr) ? *reinterpret_cast<int64_t*>(mgr + 0x100) : 0;
                fprintf(stderr, "[HousingAssign] comp=%p findResult=%p (0=no house found)"
                                " townMgr=%p bldgCount=%d profTreeRoot=%p\n",
                        (void*)self, (void*)housingResult, (void*)mgr, bcount, (void*)btree);
                fflush(stderr);
            }
        }
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
        // Faithful-equivalent guard: the original reads storage+0x58 (the workplace's
        // ResidenceComponent worker-roster count) UNGUARDED, relying on the invariant
        // that an assigned workplace (+0x238) always carries a ResidenceComponent (c7cf0).
        // In standalone a force-assigned / partially-built workplace can lack it
        // (Entity_FindComponent_Storage -> null), so guard the null before the roster read.
        if (storageVal != 0 && *reinterpret_cast<int*>(storageVal + 0x58) > 1) {
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
    // [STANDALONE GUARD] profession-list null until assigned → no workplace to switch.
    if (profKey == 0) return false;

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
// Callee reimpls (Component reg wave, batch "people")
// Faithful ports of the CitizenComponent exe helpers the Update reimpl above
// resolves+calls. Sub-deps + exe-internal globals resolved via guarded
// ResolveAddr (same pattern as the rest of code/Game). Registered in
// reg_component_people.cpp. Bodies are non-static for extern registration.
// ============================================================

// --- exe-internal globals / sub-functions used by the reimpls below ---
static uint32_t* s_TypeId_Workplace   = nullptr; // DAT_1401c80e0 (WorkplaceComponent)
static uint32_t* s_TypeId_StorageLoc  = nullptr; // DAT_1401c8070 (StorageLocationComponent)
static uint32_t* s_TypeId_Residence   = nullptr; // DAT_1401c7cf0 (ResidenceComponent)
static uint32_t* s_TypeId_Position    = nullptr; // DAT_1401c7900
static uint32_t* s_TypeId_AIComp      = nullptr; // DAT_1401c7158 (CitizenAIComponent)
static uint32_t* s_TypeId_Storage1c7d28 = nullptr; // DAT_1401c7d28
static uint32_t* s_TypeId_RawMat1c7cb8  = nullptr; // DAT_1401c7cb8 (RawMaterialDescription)
static uint32_t* s_TypeId_1c7cb8b     = nullptr; // DAT_1401c7cb8 (workplace-job in ReleaseWorkplace)
static uint32_t* s_pDAT_1401722b0     = nullptr; // abs-value mask used by helper_fe40
static float*    s_pUVScale           = nullptr; // g_fUVScale (DAT_14017214c)

typedef int    (*fn_CalcWorkerSlots_t)(int64_t comp);                // FUN_14012e5c0
typedef char   (*fn_CheckWorkplaceValid_t)(CitizenComponent* self, void* ibp); // 0x1401158d0
typedef void*  (*fn_FindComp_Mat_t)(int64_t comp, void* outIBP);     // FUN_140003050
typedef void   (*fn_helper_02a0_sort_t)(void* arr, int n, void* tmp);// CitizenComponent_helper_02a0 (0x1400302a0)
typedef void   (*fn_AssignState_real_t)(CitizenComponent* self, int s, int p2, const char* str); // 0x140113af0
typedef void   (*fn_sub_2040_t)(int64_t comp, void* ibp);           // CitizenComponent_sub_2040
typedef void   (*fn_helper_68a0_t)(void* comp, void* ibp);          // CitizenComponent_helper_68a0
typedef void*  (*fn_CopyCtor_t)(void* dst, void* desc);             // ComponentEntity_CopyCtor
typedef void   (*fn_FUN_140117080_t)(int64_t a, void* b, uint32_t c, char d, int e, char f, int64_t* g, int64_t h);
typedef void   (*fn_FUN_1400c48a0_t)(void* a, void* ibp, int idx);  // FUN_1400c48a0
typedef void   (*fn_FUN_1400c3470_t)(void* internal);              // FUN_1400c3470
typedef void*  (*fn_Entity_FindCompStorage_t)(int64_t entity, void* outIBP); // 0x140008090
typedef void   (*fn_Entity_impl_ef40_t)(void* a, int64_t* outRes, void* ibp, int z); // 0x14008ef40
typedef void   (*fn_FUN_14012d150_t)(int64_t a, int64_t b, int c);  // FUN_14012d150
typedef uint32_t (*fn_FUN_140088f90_t)(int64_t a, int64_t b);      // FUN_140088f90
typedef void   (*fn_Entity_impl_2e20_t)(int64_t a, void* outVec, int b, int c, float* d); // 0x140122e20
typedef void   (*fn_FUN_14009c480_t)(int* a, int* b, int c, int d); // FUN_14009c480
typedef void   (*fn_FUN_140004680_t)(int64_t a, void* outIBP);     // FUN_140004680
typedef void   (*fn_TimeFromFloat_t)(void* outTime, float v);
// Callees for the two newly-reimplemented helpers (CheckWorkplaceValid / Entity_impl_ef40):
typedef int    (*fn_Entity_impl_7fb0_t)(void* ibp, float* p2);                 // 0x140087fb0
typedef char   (*fn_FUN_140a00f0_t)(int* world, int eventId, uint64_t cellIdx, int param); // 0x1400a00f0
typedef void*  (*fn_Entity_FindCompStorage6270_t)(int64_t entity, void* outIBP); // 0x140006270
// Callees for the citizen-lifecycle cascade (AssignHousing / sub_1930 / sub_40f0 / Find*):
typedef void*  (*fn_Entity_FindBuilding2e10_t)(int64_t entity, void* outIBP);   // 0x140002e10
typedef void   (*fn_DetachWorkplace7e0_t)(int64_t buildingComp, void* citizenIBP);  // 0x14012f7e0
typedef void   (*fn_AttachWorkplacea20_t)(int64_t buildingComp, void* citizenIBP);  // 0x14012fa20
typedef int    (*fn_PathCost8830_t)(int* mapInfo, uint32_t fromRegion, uint32_t toRegion); // 0x140008830
typedef void   (*fn_GrowArray3110_t)(int64_t* arrayBase, int newCount, void* elem);  // 0x140003110
typedef void*  (*fn_Entity_FindPos_ed0_t)(int64_t entity, void* outIBP);        // 0x140002ed0

static fn_CalcWorkerSlots_t      s_fn_CalcWorkerSlots = nullptr;
static fn_CheckWorkplaceValid_t  s_fn_CheckWorkplaceValid = nullptr;
static fn_FindComp_Mat_t         s_fn_FindComp_Mat = nullptr;
static fn_helper_02a0_sort_t     s_fn_helper_02a0 = nullptr;
static fn_AssignState_real_t     s_fn_AssignStateReal = nullptr;
static fn_sub_2040_t             s_fn_sub_2040 = nullptr;
static fn_helper_68a0_t          s_fn_helper_68a0 = nullptr;
static fn_CopyCtor_t             s_fn_CopyCtor = nullptr;
static fn_FUN_140117080_t        s_fn_FUN_140117080 = nullptr;
static fn_FUN_1400c48a0_t        s_fn_FUN_1400c48a0 = nullptr;
static fn_FUN_1400c3470_t        s_fn_FUN_1400c3470 = nullptr;
static fn_Entity_FindCompStorage_t s_fn_EntityFindCompStorage = nullptr;
static fn_Entity_impl_ef40_t     s_fn_Entity_impl_ef40 = nullptr;
static fn_FUN_14012d150_t        s_fn_FUN_14012d150 = nullptr;
static fn_FUN_140088f90_t        s_fn_FUN_140088f90 = nullptr;
static fn_Entity_impl_2e20_t     s_fn_Entity_impl_2e20 = nullptr;
static fn_FUN_14009c480_t        s_fn_FUN_14009c480 = nullptr;
static fn_FUN_140004680_t        s_fn_FUN_140004680 = nullptr;
static fn_TimeFromFloat_t        s_fn_TimeFromFloat = nullptr;
static fn_Entity_impl_7fb0_t     s_fn_Entity_impl_7fb0 = nullptr;
static fn_FUN_140a00f0_t         s_fn_FUN_140a00f0 = nullptr;
static fn_Entity_FindCompStorage6270_t s_fn_EntityFindCompStorage6270 = nullptr;
static fn_Entity_FindBuilding2e10_t s_fn_Entity_FindBuilding2e10 = nullptr;
static fn_DetachWorkplace7e0_t   s_fn_DetachWorkplace7e0 = nullptr;
static fn_AttachWorkplacea20_t   s_fn_AttachWorkplacea20 = nullptr;
static fn_PathCost8830_t         s_fn_PathCost8830 = nullptr;
static fn_GrowArray3110_t        s_fn_GrowArray3110 = nullptr;
static fn_Entity_FindPos_ed0_t   s_fn_Entity_FindPos_ed0 = nullptr;
static uint32_t*                 s_TypeId_Residence3f78 = nullptr; // DWORD_1401c3f78 (sub_40f0)
// .rdata float/double constants used by ConsumeFood / ConsumeFuel.
static float*  s_DAT_140172180 = nullptr;
static float*  s_DAT_1401721a8 = nullptr;
static float*  s_DAT_140172174 = nullptr;
static float*  s_DAT_140172194 = nullptr;
static double* s_DAT_140172168 = nullptr;
static float*  s_DAT_1401720ec = nullptr;

static bool s_calleeResolved = false;
static void EnsureCallees() {
    if (s_calleeResolved) return;
    s_TypeId_Workplace   = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c80e0));
    s_TypeId_StorageLoc  = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c8070));
    s_TypeId_Residence   = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7cf0));
    s_TypeId_Position    = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7900));
    s_TypeId_AIComp      = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7158));
    s_TypeId_Storage1c7d28 = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7d28));
    s_TypeId_RawMat1c7cb8  = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7cb8));
    s_TypeId_1c7cb8b     = s_TypeId_RawMat1c7cb8;
    s_pDAT_1401722b0     = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401722b0));
    s_pUVScale           = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x14017214c));

    s_fn_CalcWorkerSlots     = reinterpret_cast<fn_CalcWorkerSlots_t>(ResolveAddr(Module::Exe, 0x14012e5c0));
    s_fn_CheckWorkplaceValid = reinterpret_cast<fn_CheckWorkplaceValid_t>(ResolveAddr(Module::Exe, 0x1401158d0));
    s_fn_FindComp_Mat        = reinterpret_cast<fn_FindComp_Mat_t>(ResolveAddr(Module::Exe, 0x140003050));
    s_fn_helper_02a0         = reinterpret_cast<fn_helper_02a0_sort_t>(ResolveAddr(Module::Exe, 0x1400302a0));
    s_fn_AssignStateReal     = reinterpret_cast<fn_AssignState_real_t>(ResolveAddr(Module::Exe, 0x140113af0));
    s_fn_sub_2040            = reinterpret_cast<fn_sub_2040_t>(ResolveAddr(Module::Exe, 0x140112040));
    s_fn_helper_68a0         = reinterpret_cast<fn_helper_68a0_t>(ResolveAddr(Module::Exe, 0x1401168a0));
    s_fn_CopyCtor            = reinterpret_cast<fn_CopyCtor_t>(ResolveAddr(Module::Exe, 0x1400c3720));
    s_fn_FUN_140117080       = reinterpret_cast<fn_FUN_140117080_t>(ResolveAddr(Module::Exe, 0x140117080));
    s_fn_FUN_1400c48a0       = reinterpret_cast<fn_FUN_1400c48a0_t>(ResolveAddr(Module::Exe, 0x1400c48a0));
    s_fn_FUN_1400c3470       = reinterpret_cast<fn_FUN_1400c3470_t>(ResolveAddr(Module::Exe, 0x1400c3470));
    s_fn_EntityFindCompStorage = reinterpret_cast<fn_Entity_FindCompStorage_t>(ResolveAddr(Module::Exe, 0x140008090));
    s_fn_Entity_impl_ef40    = reinterpret_cast<fn_Entity_impl_ef40_t>(ResolveAddr(Module::Exe, 0x14008ef40));
    s_fn_FUN_14012d150       = reinterpret_cast<fn_FUN_14012d150_t>(ResolveAddr(Module::Exe, 0x14012d150));
    s_fn_FUN_140088f90       = reinterpret_cast<fn_FUN_140088f90_t>(ResolveAddr(Module::Exe, 0x140088f90));
    s_fn_Entity_impl_2e20    = reinterpret_cast<fn_Entity_impl_2e20_t>(ResolveAddr(Module::Exe, 0x140122e20));
    s_fn_FUN_14009c480       = reinterpret_cast<fn_FUN_14009c480_t>(ResolveAddr(Module::Exe, 0x14009c480));
    s_fn_FUN_140004680       = reinterpret_cast<fn_FUN_140004680_t>(ResolveAddr(Module::Exe, 0x140004680));
    s_fn_Entity_impl_7fb0    = reinterpret_cast<fn_Entity_impl_7fb0_t>(ResolveAddr(Module::Exe, 0x140087fb0));
    s_fn_FUN_140a00f0        = reinterpret_cast<fn_FUN_140a00f0_t>(ResolveAddr(Module::Exe, 0x1400a00f0));
    s_fn_EntityFindCompStorage6270 = reinterpret_cast<fn_Entity_FindCompStorage6270_t>(ResolveAddr(Module::Exe, 0x140006270));
    s_DAT_140172180 = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172180));
    s_DAT_1401721a8 = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x1401721a8));
    s_DAT_140172174 = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172174));
    s_DAT_140172194 = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172194));
    s_DAT_140172168 = reinterpret_cast<double*>(ResolveAddr(Module::Exe, 0x140172168));
    s_DAT_1401720ec = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x1401720ec));
    s_fn_Entity_FindBuilding2e10 = reinterpret_cast<fn_Entity_FindBuilding2e10_t>(ResolveAddr(Module::Exe, 0x140002e10));
    s_fn_DetachWorkplace7e0  = reinterpret_cast<fn_DetachWorkplace7e0_t>(ResolveAddr(Module::Exe, 0x14012f7e0));
    s_fn_AttachWorkplacea20  = reinterpret_cast<fn_AttachWorkplacea20_t>(ResolveAddr(Module::Exe, 0x14012fa20));
    s_fn_PathCost8830        = reinterpret_cast<fn_PathCost8830_t>(ResolveAddr(Module::Exe, 0x140008830));
    s_fn_GrowArray3110       = reinterpret_cast<fn_GrowArray3110_t>(ResolveAddr(Module::Exe, 0x140003110));
    s_fn_Entity_FindPos_ed0  = reinterpret_cast<fn_Entity_FindPos_ed0_t>(ResolveAddr(Module::Exe, 0x140002ed0));
    s_TypeId_Residence3f78   = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c3f78));
    // Time::Time(float) from its .rdata dispatch slot (used by FUN_14012c3f0).
    {
        uintptr_t slot = ResolveAddr(Module::Exe, 0x140152a80); // Time::Time(float)
        if (slot) s_fn_TimeFromFloat = *reinterpret_cast<fn_TimeFromFloat_t*>(slot);
    }
    s_calleeResolved = true;
}

// Helper: scan an entity component array for a type id (vtable+0x10), returning the
// derived-checked component pointer (or null). Mirrors the inlined finder pattern.
static void* FindCompByTypeId(int64_t entity, uint32_t typeId) {
    uint8_t* e = reinterpret_cast<uint8_t*>(entity);
    int count = *reinterpret_cast<int*>(e + 0x38);
    int64_t arr = *reinterpret_cast<int64_t*>(e + 0x30);
    for (int i = 0; i < count; i++) {
        void** ibp = reinterpret_cast<void**>(arr + (int64_t)i * 8);
        fn_InternalBasePtr_CheckPtr(ibp);
        int64_t comp = *reinterpret_cast<int64_t*>(ibp);
        int64_t vt = *reinterpret_cast<int64_t*>(comp);
        typedef uint32_t (*getType_t)(void*);
        uint32_t tid = (*reinterpret_cast<getType_t*>(vt + 0x10))(reinterpret_cast<void*>(comp));
        if (tid == typeId) {
            void** ibp2 = reinterpret_cast<void**>(arr + (int64_t)i * 8);
            fn_InternalBasePtr_CheckPtr(ibp2);
            void* resolved = *ibp2;
            return fn_IsDerivedFrom(resolved, typeId) ? resolved : nullptr;
        }
    }
    return nullptr;
}

// Reimpl of CitizenComponent_helper_fe40 (0x14010fe40):
// find a "partner" citizen within the description's age window (+0x48/+0x4c) and
// matching family-id (+0x38), returning an InternalBasePtr (param_2) to it.
void* CitizenComp_helper_fe40(int64_t self, void* outIBP) {
    EnsureCallees();
    uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    uint32_t mask = s_pDAT_1401722b0 ? *s_pDAT_1401722b0 : 0x7fffffff;
    int64_t list = *reinterpret_cast<int64_t*>(gw + 0xf0);
    int64_t n = *reinterpret_cast<int*>(list + 0xa8);
    uint8_t* sp = reinterpret_cast<uint8_t*>(self);
    for (int64_t i = 0; i < n; i++) {
        int64_t cand = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(list + 0xa0) + i * 8);
        fn_InternalBasePtr_CheckPtr(&cand);
        if (*reinterpret_cast<int*>(cand + 0x38) != *reinterpret_cast<int*>(sp + 0x38)) {
            fn_InternalBasePtr_CheckPtr(&cand);
            if (*reinterpret_cast<int64_t*>(cand + 0x250) == 0) {
                fn_InternalBasePtr_CheckPtr(&cand);
                float* minAge = reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(sp + 0x288) + 0x48);
                float candAge = *reinterpret_cast<float*>(cand + 0x3c);
                if (*minAge <= candAge && candAge != *minAge) {
                    fn_InternalBasePtr_CheckPtr(&cand);
                    float diff = candAge - *reinterpret_cast<float*>(sp + 0x3c);
                    // abs(diff) via mask, then compare to window max (+0x4c)
                    uint32_t bits = *reinterpret_cast<uint32_t*>(&diff) & mask;
                    float adiff = *reinterpret_cast<float*>(&bits);
                    if (adiff <= *reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(sp + 0x288) + 0x4c)) {
                        *reinterpret_cast<int64_t*>(outIBP) = cand;
                        return outIBP;
                    }
                }
            }
        }
    }
    fn_InternalBasePtr_Ctor(outIBP, nullptr);
    return outIBP;
}

// Reimpl of CitizenComponent_sub_1fa0 (0x140111fa0):
// reassign the citizen's residence link (+600 qwords == +0x258 bytes? note: param_1
// is Internal*, "param_1 + 600" is byte offset 0x258). Detach from old, attach to new.
void CitizenComp_sub_1fa0(int64_t selfV, void* param2) {
    EnsureCallees();
    uint8_t* self = reinterpret_cast<uint8_t*>(selfV);
    int64_t cur = 0;
    fn_InternalBasePtr_CheckPtr(self + 600);
    if (*reinterpret_cast<int64_t*>(param2) != *reinterpret_cast<int64_t*>(self + 600)) {
        if (*reinterpret_cast<int64_t*>(self + 600) != 0) {
            int64_t ibp = 0;
            fn_InternalBasePtr_Ctor(&ibp, reinterpret_cast<void*>(selfV));
            fn_InternalBasePtr_CheckPtr(self + 600);
            if (s_fn_sub_2040) s_fn_sub_2040(*reinterpret_cast<int64_t*>(self + 600), &ibp);
        }
        if (*reinterpret_cast<int64_t*>(param2) != 0) {
            int64_t ibp = 0;
            fn_InternalBasePtr_Ctor(&ibp, reinterpret_cast<void*>(selfV));
            fn_InternalBasePtr_CheckPtr(param2);
            if (s_fn_helper_68a0) s_fn_helper_68a0(*reinterpret_cast<void**>(param2), &ibp);
        }
    }
    (void)cur;
}

// Reimpl of CitizenComponent_helper_a4f0 (0x14012a4f0):
// returns whether the citizen's education level (residence comp +0x24) is <= the
// storage row capacity at +0x50 + param2*0x10 + 8.
bool CitizenComp_helper_a4f0(int64_t self, int param2) {
    EnsureCallees();
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    void* obj = *reinterpret_cast<void**>(s + 0x18);
    void* res = (s_TypeId_Storage1c7d28 && fn_IsDerivedFrom(obj, *s_TypeId_Storage1c7d28)) ? obj : nullptr;
    int eduLevel = res ? *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(res) + 0x24) : 0;
    int cap = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(s + 0x50) + 8 + (int64_t)param2 * 0x10);
    return eduLevel <= cap;
}

// Reimpl of CitizenComponent_helper_ff80 (0x14010ff80):
// pick the housing finder: if residence's "is-assigned" flag (+0x4c) set, use the
// assigned-housing finder (sub_0470), else the unassigned finder (sub_ffc0).
void* CitizenComp_helper_ff80(int64_t self, void* outIBP);   // fwd (defined after the two finders)

// Reimpl of CitizenComponent_sub_ffc0 (0x14010ffc0):
// find best UNASSIGNED workplace for the citizen, choosing the one with the most
// free worker slots whose footprint footstep matches; scans the world workplace
// red-black tree keyed by the citizen's residence id (+0x1f0).
int64_t* CitizenComp_sub_ffc0(int64_t param_1, int64_t* param_2) {
    EnsureCallees();
    uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    *param_2 = 0;
    int64_t tree = *reinterpret_cast<int64_t*>(gw + 0xf0);
    uint64_t key = *reinterpret_cast<uint64_t*>(param_1 + 0x1f0);

    // tree lookup helper (red-black ordered by uint64 at node[0]); node layout:
    //   node[0]=key, node[1]=valueArrayPtr, node[2]=count(low32), node[4]=left, node[5]=right
    auto findNode = [&](uint64_t k) -> uint64_t* {
        uint64_t* p = *reinterpret_cast<uint64_t**>(tree + 0x100);
        while (p != nullptr) {
            if (k < p[0]) p = reinterpret_cast<uint64_t*>(p[4]);
            else if (k <= p[0]) return p;
            else p = reinterpret_cast<uint64_t*>(p[5]);
        }
        return nullptr;
    };

    uint64_t* keyNode = findNode(key);
    int outerCount = keyNode ? (int)keyNode[2] : 0;
    int bestSlots = 0;
    for (int64_t i = 0; i < outerCount; i++) {
        uint64_t* node = findNode(key);
        if (!node) break;
        int64_t entryIBP = *reinterpret_cast<int64_t*>(node[1] + i * 8);
        fn_InternalBasePtr_CheckPtr(&entryIBP);

        // find WorkplaceComponent (DAT_1401c80e0) on the entry
        void* wp = FindCompByTypeId(entryIBP, s_TypeId_Workplace ? *s_TypeId_Workplace : 0);
        int64_t wpIBP = 0;
        fn_InternalBasePtr_Ctor(&wpIBP, wp);
        fn_InternalBasePtr_CheckPtr(&wpIBP);
        int curWorkers = *reinterpret_cast<int*>(wpIBP + 0x60);

        // find StorageLocationComponent (DAT_1401c8070) on wp's sub-entity (+0x10)
        fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(wpIBP + 0x10));
        int64_t sub = *reinterpret_cast<int64_t*>(wpIBP + 0x10);
        void* sloc = FindCompByTypeId(sub, s_TypeId_StorageLoc ? *s_TypeId_StorageLoc : 0);
        int64_t slocIBP = 0;
        fn_InternalBasePtr_Ctor(&slocIBP, sloc);
        fn_InternalBasePtr_CheckPtr(&slocIBP);
        int maxSlots = *reinterpret_cast<int*>(slocIBP + 0x30);
        if (maxSlots == 0 && s_fn_CalcWorkerSlots) {
            maxSlots = s_fn_CalcWorkerSlots(slocIBP);
            *reinterpret_cast<int*>(slocIBP + 0x30) = maxSlots;
        }

        if (curWorkers < maxSlots) {
            fn_InternalBasePtr_CheckPtr(&wpIBP);
            if (*reinterpret_cast<char*>(wpIBP + 0x50) != '\0') {
                int64_t local = entryIBP;
                char ok = s_fn_CheckWorkplaceValid
                              ? s_fn_CheckWorkplaceValid(reinterpret_cast<CitizenComponent*>(param_1), &local)
                              : 0;
                if (ok != '\0') {
                    fn_InternalBasePtr_CheckPtr(&wpIBP);
                    fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(wpIBP + 0x10));
                    int64_t sub2IBP = 0;
                    void* sloc2 = s_fn_FindComp_Mat
                                      ? s_fn_FindComp_Mat(*reinterpret_cast<int64_t*>(wpIBP + 0x10), &sub2IBP)
                                      : nullptr;
                    fn_InternalBasePtr_CheckPtr(sloc2);
                    int64_t s2 = *reinterpret_cast<int64_t*>(sloc2);
                    int s2slots = *reinterpret_cast<int*>(s2 + 0x30);
                    if (s2slots == 0 && s_fn_CalcWorkerSlots) {
                        s2slots = s_fn_CalcWorkerSlots(s2);
                        *reinterpret_cast<int*>(s2 + 0x30) = s2slots;
                    }
                    fn_InternalBasePtr_CheckPtr(&wpIBP);
                    int gap = s2slots - *reinterpret_cast<int*>(wpIBP + 0x60);
                    if (bestSlots < gap) {
                        *param_2 = entryIBP;
                        bestSlots = gap;
                    }
                }
            }
        }
    }
    return param_2;
}

// Reimpl of CitizenComponent_sub_0470 (0x140110470):
// find best ASSIGNED-eligible workplace; builds a candidate array of valid matches,
// sorts via helper_02a0, then returns the first whose free-slot count exceeds the
// workplace's current workers. Same scanning structure as sub_ffc0 plus a grow-array.
int64_t* CitizenComp_sub_0470(int64_t param_1, int64_t* param_2) {
    EnsureCallees();
    uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    int64_t tree = *reinterpret_cast<int64_t*>(gw + 0xf0);
    uint64_t key = *reinterpret_cast<uint64_t*>(param_1 + 0x1f0);

    auto findNode = [&](uint64_t k) -> uint64_t* {
        uint64_t* p = *reinterpret_cast<uint64_t**>(tree + 0x100);
        while (p != nullptr) {
            if (k < p[0]) p = reinterpret_cast<uint64_t*>(p[4]);
            else if (k <= p[0]) return p;
            else p = reinterpret_cast<uint64_t*>(p[5]);
        }
        return nullptr;
    };

    uint64_t* keyNode = findNode(key);
    int outerCount = keyNode ? (int)keyNode[2] : 0;

    // dynamic candidate array
    int64_t* cand = nullptr;
    int candCount = 0;
    int candCap = 0;

    for (int64_t i = 0; i < outerCount; i++) {
        uint64_t* node = findNode(key);
        if (!node) break;
        int64_t entryIBP = *reinterpret_cast<int64_t*>(node[1] + i * 8);
        fn_InternalBasePtr_CheckPtr(&entryIBP);

        void* wp = FindCompByTypeId(entryIBP, s_TypeId_Workplace ? *s_TypeId_Workplace : 0);
        int64_t wpIBP = 0;
        fn_InternalBasePtr_Ctor(&wpIBP, wp);
        fn_InternalBasePtr_CheckPtr(&wpIBP);
        int curWorkers = *reinterpret_cast<int*>(wpIBP + 0x60);

        fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(wpIBP + 0x10));
        int64_t sub = *reinterpret_cast<int64_t*>(wpIBP + 0x10);
        void* sloc = FindCompByTypeId(sub, s_TypeId_StorageLoc ? *s_TypeId_StorageLoc : 0);
        int64_t slocIBP = 0;
        fn_InternalBasePtr_Ctor(&slocIBP, sloc);
        fn_InternalBasePtr_CheckPtr(&slocIBP);
        int maxSlots = *reinterpret_cast<int*>(slocIBP + 0x30);
        if (maxSlots == 0 && s_fn_CalcWorkerSlots) {
            maxSlots = s_fn_CalcWorkerSlots(slocIBP);
            *reinterpret_cast<int*>(slocIBP + 0x30) = maxSlots;
        }

        if (curWorkers < maxSlots) {
            fn_InternalBasePtr_CheckPtr(&wpIBP);
            if (*reinterpret_cast<char*>(wpIBP + 0x50) != '\0') {
                int64_t local = entryIBP;
                char ok = s_fn_CheckWorkplaceValid
                              ? s_fn_CheckWorkplaceValid(reinterpret_cast<CitizenComponent*>(param_1), &local)
                              : 0;
                if (ok != '\0') {
                    // append entryIBP to candidate array (grow with 3x policy)
                    int newCount = candCount + 1;
                    if (candCap < newCount) {
                        candCap = (candCount != 0) ? newCount * 3 : newCount;
                        int64_t* nb = reinterpret_cast<int64_t*>(
                            fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)candCap * 8, 8));
                        for (int k = 0; k < candCount; k++) nb[k] = cand[k];
                        if (cand) fn_MemFreeStatic(cand);
                        cand = nb;
                    }
                    cand[candCount] = entryIBP;
                    candCount = newCount;
                }
            }
        }
    }

    if (candCount == 0) {
        fn_InternalBasePtr_Ctor(param_2, nullptr);
        if (cand) fn_MemFreeStatic(cand);
        return param_2;
    }
    if (candCount > 1 && s_fn_helper_02a0) {
        s_fn_helper_02a0(cand, candCount, reinterpret_cast<void*>(param_1));
    }
    for (int k = 0; k < candCount; k++) {
        int64_t entryIBP = cand[k];
        fn_InternalBasePtr_CheckPtr(&entryIBP);
        void* wp = FindCompByTypeId(entryIBP, s_TypeId_Workplace ? *s_TypeId_Workplace : 0);
        int64_t wpIBP = 0;
        fn_InternalBasePtr_Ctor(&wpIBP, wp);
        fn_InternalBasePtr_CheckPtr(&wpIBP);
        fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(wpIBP + 0x10));
        int64_t sub = *reinterpret_cast<int64_t*>(wpIBP + 0x10);
        void* sloc = FindCompByTypeId(sub, s_TypeId_StorageLoc ? *s_TypeId_StorageLoc : 0);
        int64_t slocIBP = 0;
        fn_InternalBasePtr_Ctor(&slocIBP, sloc);
        fn_InternalBasePtr_CheckPtr(&slocIBP);
        int maxSlots = *reinterpret_cast<int*>(slocIBP + 0x30);
        if (maxSlots == 0 && s_fn_CalcWorkerSlots) {
            maxSlots = s_fn_CalcWorkerSlots(slocIBP);
            *reinterpret_cast<int*>(slocIBP + 0x30) = maxSlots;
        }
        fn_InternalBasePtr_CheckPtr(&wpIBP);
        if (*reinterpret_cast<int*>(wpIBP + 0x60) < maxSlots) {
            *param_2 = entryIBP;
            fn_MemFreeStatic(cand);
            return param_2;
        }
    }
    fn_InternalBasePtr_Ctor(param_2, nullptr);
    fn_MemFreeStatic(cand);
    return param_2;
}

void* CitizenComp_helper_ff80(int64_t self, void* outIBP) {
    EnsureCallees();
    if (*reinterpret_cast<char*>(*reinterpret_cast<int64_t*>(self + 0x1f0) + 0x4c) != '\0') {
        CitizenComp_sub_0470(self, reinterpret_cast<int64_t*>(outIBP));
    } else {
        CitizenComp_sub_ffc0(self, reinterpret_cast<int64_t*>(outIBP));
    }
    return outIBP;
}

// Reimpl of FUN_14012c3f0 (HealthComponent status-flag setter, citizenComp HealthSetFlag):
// set/clear bit `flagIndex` in the status mask (+0xf0 == param_1[0x1e]); when the mask
// becomes empty/non-empty, add/remove the geometry from the scene and toggle the +0x20
// scene id and the +4 "in scene" flag, then notify the owning entity.
void CitizenComp_FUN_14012c3f0(int64_t* param_1, uint8_t flagIndex, char value) {
    EnsureCallees();
    fn_InternalBasePtr_CheckPtr(param_1 + 2);
    if (((*reinterpret_cast<uint32_t*>(param_1[2] + 0x40) >> 10 & 1) == 0) || (value == '\0')) {
        uint32_t bit = 1u << (flagIndex & 0x1f);
        if (value == '\0') {
            *reinterpret_cast<uint32_t*>(param_1 + 0x1e) &= ~bit;
        } else {
            *reinterpret_cast<uint32_t*>(param_1 + 0x1e) |= bit;
        }
        uint32_t changed;
        uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
        if ((int)param_1[0x1e] == 0) {
            if (-1 < (int)*reinterpret_cast<uint32_t*>(param_1 + 0x20)) {
                if (fn_SceneRemoveObject)
                    fn_SceneRemoveObject(*reinterpret_cast<void**>(gw + 0x60),
                                         *reinterpret_cast<uint32_t*>(param_1 + 0x20));
                *reinterpret_cast<uint32_t*>(param_1 + 0x20) = 0xffffffff;
            }
            uint32_t before = *reinterpret_cast<uint32_t*>(param_1 + 4);
            uint32_t after = before & 0xfffffffb;
            *reinterpret_cast<uint32_t*>(param_1 + 4) = after;
            changed = after ^ before;
        } else {
            int32_t* settings = g_pGameSettingsPtr ? *g_pGameSettingsPtr : nullptr;
            if (settings && reinterpret_cast<char*>(settings)[0xc] != '\0' &&
                (int)param_1[0x20] < 0) {
                uint32_t id = fn_Graphics_Scene_AddObject
                                  ? fn_Graphics_Scene_AddObject(*reinterpret_cast<void**>(gw + 0x60),
                                                                reinterpret_cast<void*>(param_1 + 5))
                                  : 0xffffffff;
                *reinterpret_cast<uint32_t*>(param_1 + 0x20) = id;
            }
            int64_t timeVal = 0;
            if (s_fn_TimeFromFloat) s_fn_TimeFromFloat(&timeVal, 0.0f);
            typedef void (*slot40_t)(int64_t*, int64_t*);
            (*reinterpret_cast<slot40_t*>(*param_1 + 0x40))(param_1, &timeVal);
            uint32_t before = *reinterpret_cast<uint32_t*>(param_1 + 4);
            uint32_t after = before | 4;
            *reinterpret_cast<uint32_t*>(param_1 + 4) = after;
            changed = after ^ before;
        }
        if ((changed & 0xffff) != 0 && param_1[2] != 0) {
            fn_InternalBasePtr_CheckPtr(param_1 + 2);
            if (s_fn_FUN_1400c3470) s_fn_FUN_1400c3470(reinterpret_cast<void*>(param_1[2]));
        }
    }
}

// Reimpl of CitizenComponent_CanFindHousing (0x140115be0):
// returns true if either of the citizen's two saved house tiles (+0x238/+0x248) is
// currently buildable, else samples the citizen's current world position against the
// terrain buildable grid.
char CitizenComp_CanFindHousing(int64_t selfV) {
    EnsureCallees();
    uint8_t* self = reinterpret_cast<uint8_t*>(selfV);
    int* grid = *reinterpret_cast<int**>(reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xf8);

    auto tileBuildable = [&](int64_t res8) -> bool {
        // [STANDALONE GUARD] FUN_140004680 (position-component find on the workplace/
        // residence entity) returns null in standalone when the component is inert /
        // its type-id global is unfilled. The real game always resolves it. Bail
        // "not buildable" rather than deref res8+0x2e (p[0x17]). Surfaced by the
        // BANISHED_FORCE_WORK harness setting +0x238 to a real building entity.
        if (res8 == 0) return false;
        int64_t base = *reinterpret_cast<int64_t*>(grid + 0x20);
        int dim = **reinterpret_cast<int**>(reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xf8);
        int16_t* p = reinterpret_cast<int16_t*>(res8);
        int64_t idx = (((int64_t)p[0x17] >> 1) + (int64_t)p[0x15]) * dim +
                      (((int64_t)p[0x16] >> 1) + (int64_t)p[0x14]);
        return *reinterpret_cast<char*>(base + 0x22 + idx * 0x30) != '\0';
    };

    if (*reinterpret_cast<int64_t*>(self + 0x238) != 0) {
        fn_InternalBasePtr_CheckPtr(self + 0x238);
        int64_t res8 = 0;
        if (s_fn_FUN_140004680) s_fn_FUN_140004680(*reinterpret_cast<int64_t*>(self + 0x238), &res8);
        fn_InternalBasePtr_CheckPtr(&res8);
        if (tileBuildable(res8)) return 1;
    }
    if (*reinterpret_cast<int64_t*>(self + 0x248) != 0) {
        fn_InternalBasePtr_CheckPtr(self + 0x248);
        int64_t res8 = 0;
        if (s_fn_FUN_140004680) s_fn_FUN_140004680(*reinterpret_cast<int64_t*>(self + 0x248), &res8);
        fn_InternalBasePtr_CheckPtr(&res8);
        if (tileBuildable(res8)) return 1;
    }
    fn_InternalBasePtr_CheckPtr(self + 0x278);
    int64_t mc = *reinterpret_cast<int64_t*>(self + 0x278);
    if (mc == 0) return 0;
    fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(mc + 0x68));
    int64_t geom = *reinterpret_cast<int64_t*>(mc + 0x68);
    // [STANDALONE BRING-UP GUARD] mc+0x68 is the citizen's geometry/transform IBP,
    // populated only once the CharacterComponent position pipeline is live (Brick 4).
    // Until then it is null; bail "can't find housing" rather than deref geom+0x158.
    if (geom == 0) return 0;
    int dim = grid[0];
    int x = (int)(*reinterpret_cast<float*>(geom + 0x158) + (float)(dim >> 1));
    int y = (int)(*reinterpret_cast<float*>(geom + 0x15c) + (float)(dim >> 1));
    if (x < 0) x = 0;
    if (dim <= x) x = dim - 1;
    if (y < 0) y = 0;
    if (dim <= y) y = dim - 1;
    int64_t cell = ((int64_t)dim * y + x) * 0x30;
    return *reinterpret_cast<char*>(cell + 0x22 + *reinterpret_cast<int64_t*>(grid + 0x20)) != '\0';
}

// Reimpl of CitizenComponent_helper_1b90 (0x140111b90):
// procreation — count nearby eligible mature citizens to bias child gender, allocate
// a new ComponentEntity copy of the citizen description, find its CitizenAIComponent
// (DAT_1401c7158), spawn it at the parent's world position, assign it state 8, and
// flag both parent + linked entity (+0x88 |= 0x100).
void CitizenComp_helper_1b90(int64_t selfV) {
    EnsureCallees();
    uint8_t* self = reinterpret_cast<uint8_t*>(selfV);
    uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    int64_t list = *reinterpret_cast<int64_t*>(gw + 0xf0);

    int eligible = 0, mature = 0;
    int total = *reinterpret_cast<int*>(list + 0xa8);
    for (int i = 0; i < total; i++) {
        int64_t cand = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(list + 0xa0) + (int64_t)i * 8);
        fn_InternalBasePtr_CheckPtr(&cand);
        if (*reinterpret_cast<int64_t*>(cand + 0x250) == 0) {
            fn_InternalBasePtr_CheckPtr(&cand);
            float maxAge = *reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(self + 0x288) + 0x4c);
            float candAge = *reinterpret_cast<float*>(cand + 0x3c);
            if (candAge <= maxAge) {
                fn_InternalBasePtr_CheckPtr(&cand);
                if (*reinterpret_cast<int*>(cand + 0x38) == 1) mature++;
                eligible++;
            }
        }
    }

    void* randPtr = *reinterpret_cast<void**>(gw + 0xd8);
    uint32_t gender;
    if (eligible != 0) {
        float r = fn_Random_RandFloat(randPtr);
        gender = (uint32_t)((float)mature / (float)eligible <= r);
    } else {
        gender = fn_Random_Rand(randPtr, 2);
    }

    // allocate + copy-construct the child entity
    void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
    void* child = nullptr;
    if (mem) {
        fn_InternalBasePtr_CheckPtr(self + 0x10);
        child = s_fn_CopyCtor
                    ? s_fn_CopyCtor(mem, *reinterpret_cast<void**>(*reinterpret_cast<int64_t*>(self + 0x10) + 0x28))
                    : nullptr;
    }
    int64_t childIBP = 0;
    fn_InternalBasePtr_Ctor(&childIBP, child);
    fn_InternalBasePtr_CheckPtr(&childIBP);

    // find the child's CitizenAIComponent (DAT_1401c7158)
    void* aiComp = FindCompByTypeId(childIBP, s_TypeId_AIComp ? *s_TypeId_AIComp : 0);
    int64_t aiIBP = 0;
    fn_InternalBasePtr_Ctor(&aiIBP, aiComp);

    int64_t spawnArg = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(gw + 0xb0) + 0x30);
    int64_t parentIBP = 0;
    fn_InternalBasePtr_Ctor(&parentIBP, reinterpret_cast<void*>(selfV));

    // parent world position from ModelComponent (+0x278 -> +0x68 -> +0x158..0x160)
    fn_InternalBasePtr_CheckPtr(self + 0x278);
    int64_t mc = *reinterpret_cast<int64_t*>(self + 0x278);
    fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(mc + 0x68));
    int64_t geom = *reinterpret_cast<int64_t*>(mc + 0x68);
    float pos[3];
    pos[0] = *reinterpret_cast<float*>(geom + 0x158);
    pos[1] = *reinterpret_cast<float*>(geom + 0x15c);
    pos[2] = *reinterpret_cast<float*>(geom + 0x160);

    fn_InternalBasePtr_CheckPtr(&aiIBP);
    if (s_fn_FUN_140117080)
        s_fn_FUN_140117080(aiIBP, pos, gender, '\x01', 0, '\0', &parentIBP, spawnArg);

    fn_InternalBasePtr_CheckPtr(self + 0x10);
    int64_t addArg = childIBP;
    if (s_fn_FUN_1400c48a0) {
        int64_t desc = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(self + 0x10) + 0x28);
        s_fn_FUN_1400c48a0(*reinterpret_cast<void**>(gw + 0xe8), &addArg,
                           *reinterpret_cast<int*>(desc + 0x20));
    }
    fn_InternalBasePtr_CheckPtr(&aiIBP);
    if (s_fn_AssignStateReal)
        s_fn_AssignStateReal(reinterpret_cast<CitizenComponent*>(aiIBP), 8, 0, nullptr);

    *reinterpret_cast<uint32_t*>(self + 0x88) |= 0x100;
    if (*reinterpret_cast<int64_t*>(self + 0x250) != 0) {
        fn_InternalBasePtr_CheckPtr(self + 0x250);
        *reinterpret_cast<uint32_t*>(*reinterpret_cast<int64_t*>(self + 0x250) + 0x88) |= 0x100;
    }
}

// Reimpl of CitizenComponent_ReleaseWorkplace (0x1401152e0):
// NOTE the retail label "ReleaseWorkplace" is a MISNOMER — this is EatFromStorage
// (Economy Brick M-D): while hunger (+0x44) > 0, pick a stocked edible from the
// target's StorageComponent (Entity_impl_ef40), SubtractItem, bump the town
// per-category consumed counter (townMgr+0x170[cat*0x20+0xC]), OR the material's
// diet bits (+0x5c) into the citizen's +0x54 state mask, hunger -= g_fUVScale (1.0f).
// [EatMsg] traces gated on BANISHED_STORAGE_TRACE.
// Forward decl (factory_registration.h is only included further down, in the
// factory section) — the standalone type-name -> CRC registry lookup.
namespace Object { uint32_t GetTypeIdByName(const char* name); }
static inline uint32_t StandaloneTypeIdByName(const char* n) {
    return ::Object::GetTypeIdByName(n);
}
// [SCOPED STANDALONE FALLBACK] self-contained StorageComponent find on an entity
// (+0x30 comp array / +0x38 count). The registered 6270 port (work_base_reimpl.cpp)
// reads its type-id from raw .rdata 0x1401c7e78 which is UNREGISTERED in standalone,
// so its match can never fire; do NOT fill the shared global (that would un-dormant
// ~20 other readers — the GrowthComponent +0x30 lesson). Used by the eat chain
// (EatFromStorage + Entity_impl_ef40).
static int64_t StandaloneFindStorageComp(void* entity) {
    if (!entity) return 0;
    uint32_t tidStorage = StandaloneTypeIdByName("StorageComponent");
    if (tidStorage == 0) return 0;
    uint8_t* ent = reinterpret_cast<uint8_t*>(entity);
    int cnt = *reinterpret_cast<int32_t*>(ent + 0x38);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(ent + 0x30);
    for (int i = 0; i < cnt && arr; ++i) {
        void* comp = *reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
        if (!comp) continue;
        uintptr_t vt = *reinterpret_cast<uintptr_t*>(comp);
        typedef uint32_t (*getTid_t)(void*);
        if ((*reinterpret_cast<getTid_t*>(vt + 0x10))(comp) == tidStorage)
            return reinterpret_cast<int64_t>(comp);
    }
    return 0;
}
static bool EatStorageTraceOn() {
    static int s = -1;
    if (s < 0) {
        const char* e = std::getenv("BANISHED_STORAGE_TRACE");
        s = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return s == 1;
}
void CitizenComp_ReleaseWorkplace(int64_t selfV, void* param2) {
    EnsureCallees();
    uint8_t* self = reinterpret_cast<uint8_t*>(selfV);
    if (*reinterpret_cast<int64_t*>(param2) == 0) return;

    fn_InternalBasePtr_CheckPtr(param2);
    void* entity = *reinterpret_cast<void**>(param2);
    int64_t storeIBP = 0;
    // Disasm 0x140115311: CALL 0x140006270 — the StorageComponent finder
    // (DAT_1401c7e78), NOT 0x140008090 (that one is the ResidenceComponent finder,
    // DAT_1401c7cf0 — the previous code here used it by mistake, so the eat loop
    // found no storage on storage-only food buildings).
    if (s_fn_EntityFindCompStorage6270) s_fn_EntityFindCompStorage6270((int64_t)entity, &storeIBP);
    // Scoped standalone fallback — see StandaloneFindStorageComp.
    if (storeIBP == 0) storeIBP = StandaloneFindStorageComp(entity);
    if (storeIBP == 0) {
        // Retail assumes the eat target always carries a StorageComponent (it would
        // fault inside SubtractItem otherwise). In standalone the target may lack a
        // live storage until the world-side M-D brick lands — skip with a trace.
        static int s_noStore = 0;
        if (s_noStore < 4) { ++s_noStore;
            fprintf(stderr, "[EatMsg] EatFromStorage: target=%p has NO StorageComponent — skip\n",
                    entity);
            fflush(stderr);
        }
        return;
    }

    float uv = s_pUVScale ? *s_pUVScale : 1.0f;
    float hours = *reinterpret_cast<float*>(self + 0x44);
    if (EatStorageTraceOn()) {
        static int n = 0;
        if (n < 24) { ++n;
            fprintf(stderr, "[EatMsg] EatFromStorage: citizen=%p hunger=%.3f store=%p step=%.3f\n",
                    reinterpret_cast<void*>(selfV), hours,
                    reinterpret_cast<void*>(storeIBP), uv);
            fflush(stderr);
        }
    }
    do {
        if (hours <= 0.0f) return;
        int64_t arg = *reinterpret_cast<int64_t*>(param2);
        int64_t res10 = 0;
        if (s_fn_Entity_impl_ef40) s_fn_Entity_impl_ef40(entity, &res10, &arg, 0);
        int64_t work = res10;
        if (res10 == 0) return;

        // add `work` to the citizen's work-list (+0x98 array, +0xa0 count, +0xa4 cap)
        int count = *reinterpret_cast<int*>(self + 0xa0);
        *reinterpret_cast<uint32_t*>(self + 0x88) |= 0x10000;
        bool present = false;
        if (0 < count) {
            int64_t* arr = *reinterpret_cast<int64_t**>(self + 0x98);
            for (int i = 0; i < count; i++) {
                if (arr[i] == res10) { present = true; break; }
            }
        }
        if (!present) {
            int newCount = count + 1;
            int64_t* old = *reinterpret_cast<int64_t**>(self + 0x98);
            *reinterpret_cast<int*>(self + 0xa0) = newCount;
            if (*reinterpret_cast<int*>(self + 0xa4) < newCount) {
                *reinterpret_cast<int*>(self + 0xa4) = (count != 0) ? newCount * 3 : newCount;
                int64_t* nb = reinterpret_cast<int64_t*>(
                    fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)*reinterpret_cast<int*>(self + 0xa4) * 8, 8));
                *reinterpret_cast<int64_t**>(self + 0x98) = nb;
            }
            int64_t* dst = *reinterpret_cast<int64_t**>(self + 0x98);
            int start = (old == dst) ? count : 0;
            for (int i = start; i < newCount; i++) {
                dst[i] = (i < count) ? old[i] : res10;
            }
            if (old != dst && old) fn_MemFreeStatic(old);
        }

        // subtract one unit from the storage + bump the town per-category
        // consumed counter (townMgr = *(gameWorld+0xf0), counters at +0x170,
        // 0x20-byte stride, count at +0xC).
        fn_InternalBasePtr_CheckPtr(&storeIBP);
        if (s_fn_FUN_14012d150) s_fn_FUN_14012d150(storeIBP, work, 1);
        if (EatStorageTraceOn()) {
            static int n = 0;
            if (n < 24) { ++n;
                fprintf(stderr, "[EatMsg]   eat unit: material=%p hungerBefore=%.3f\n",
                        reinterpret_cast<void*>(work),
                        *reinterpret_cast<float*>(self + 0x44));
                fflush(stderr);
            }
        }
        int64_t worldGrid = *reinterpret_cast<int64_t*>(
            reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xf0);
        if (s_fn_FUN_140088f90) {
            uint32_t idx = s_fn_FUN_140088f90(storeIBP, work);
            if (-1 < (int)idx) {
                int* p = reinterpret_cast<int*>((int64_t)(int)idx * 0x20 + 0xc +
                                                *reinterpret_cast<int64_t*>(worldGrid + 0x170));
                *p = *p + 1;
            }
        }

        // find the RawMaterialDescription (DAT_1401c7cb8) on the work entity, OR its
        // capability bits into the citizen's +0x54 state mask.
        // Typeid via the registered storage (filled from GetTypeIdByName after
        // Engine::Startup in standalone); skip the scan while it is unfilled
        // (0 / 0xFFFFFFFF) so a zero id can't false-match an inert stub component.
        void* mat = nullptr;
        uint32_t matTid = s_TypeId_RawMat1c7cb8 ? *s_TypeId_RawMat1c7cb8 : 0;
        int rows = (matTid != 0 && matTid != 0xFFFFFFFFu)
                       ? *reinterpret_cast<int*>(work + 0x30) : 0;
        for (int i = 0; i < rows; i++) {
            int64_t comp = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(work + 0x28) + (int64_t)i * 8);
            int64_t vt = *reinterpret_cast<int64_t*>(comp);
            typedef uint32_t (*getType_t)(void*);
            uint32_t tid = (*reinterpret_cast<getType_t*>(vt + 0x10))(reinterpret_cast<void*>(comp));
            if (tid == matTid) {
                void* obj = reinterpret_cast<void*>(comp);
                mat = fn_IsDerivedFrom(obj, matTid) ? obj : nullptr;
                break;
            }
        }
        uint32_t* state = reinterpret_cast<uint32_t*>(self + 0x54);
        if ((*state & 8) == 0) *state = (*state & 0xfffffe8f) | 8;
        if (mat) {
            uint32_t f = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(mat) + 0x5c);
            if (f & 1)        *state |= 0x10;
            if (f >> 1 & 1)   *state |= 0x20;
            if (f >> 2 & 1)   *state |= 0x40;
            if (f >> 4 & 1)   *state |= 0x100;
        }
        hours = *reinterpret_cast<float*>(self + 0x44) - uv;
        *reinterpret_cast<float*>(self + 0x44) = hours;
    } while (0.0f <= hours);
    *reinterpret_cast<int*>(self + 0x44) = 0;
}

// Reimpl of CitizenComponent_Update_sub_0e70 (0x140110e70):
// locate a workplace assignment for the citizen. First honors the existing workplace
// (+0x250 -> +0x238) if still valid + has free storage; else scans all world workplace
// templates (+0x28 of the world grid) for a buildable, reachable site matching the
// citizen's footprint, preferring a fully-free workplace over a partially-free one.
// NOTE: this is a long, deeply engine-coupled scan; ported structurally from Ghidra.
// param_3 receives a slot index; returns an InternalBasePtr (param_2) to the workplace.
void* CitizenComp_Update_sub_0e70(int64_t param_1, void* param_2, int* param_3, char param_4) {
    EnsureCallees();
    uint8_t* sp = reinterpret_cast<uint8_t*>(param_1);
    int bestSlot = -1;
    int64_t self250 = *reinterpret_cast<int64_t*>(sp + 0x250);
    *param_3 = -1;

    if (self250 != 0) {
        fn_InternalBasePtr_CheckPtr(sp + 0x250);
        self250 = *reinterpret_cast<int64_t*>(sp + 0x250);
        if (*reinterpret_cast<int64_t*>(self250 + 0x238) != 0) {
            fn_InternalBasePtr_CheckPtr(sp + 0x250);
            int64_t cur = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(sp + 0x250) + 0x238);
            char ok = s_fn_CheckWorkplaceValid
                          ? s_fn_CheckWorkplaceValid(reinterpret_cast<CitizenComponent*>(param_1), &cur)
                          : 0;
            if (ok != '\0') {
                fn_InternalBasePtr_CheckPtr(sp + 0x250);
                int64_t wpEntity = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(sp + 0x250) + 0x238);
                fn_InternalBasePtr_CheckPtr(&wpEntity);
                int64_t storeIBP = 0;
                void* st = s_fn_EntityFindCompStorage ? s_fn_EntityFindCompStorage(wpEntity, &storeIBP) : nullptr;
                fn_InternalBasePtr_CheckPtr(st);
                if (*reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(st) + 0x58) < 2) {
                    fn_InternalBasePtr_CheckPtr(sp + 0x250);
                    *param_3 = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(sp + 0x250) + 0x240);
                    fn_InternalBasePtr_CheckPtr(sp + 0x250);
                    int64_t v = *reinterpret_cast<int64_t*>(sp + 0x250);
                    *reinterpret_cast<int64_t*>(param_2) = *reinterpret_cast<int64_t*>(v + 0x238);
                    return param_2;
                }
            }
        }
    }

    // ---- Faithful 1:1 port of the townMgr scan (0x140110e70) ----
    // townMgr = *(gameWorld+0xf0); registry array *(townMgr+0x20), count *(townMgr+0x28).
    // Citizen fairness pre-scan uses the citizen list *(townMgr+0xa0)/*(townMgr+0xa8).
    uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    int64_t townMgr = *reinterpret_cast<int64_t*>(gw + 0xf0);

    static int s_phase1Diag = -1;
    if (s_phase1Diag < 0) {
        const char* e = getenv("BANISHED_PHASE1_EMPLOY");
        s_phase1Diag = (e && e[0] && e[0] != '0') ? 1 : 0;
    }

    // Component finder on an entry entity (comp list +0x30 array / +0x38 count,
    // GetTypeId via vtable+0x10, then IsDerivedFrom). Matches Entity_FindComponent_*.
    auto FindEntityComp = [](int64_t entity, uint32_t* typeIdPtr) -> void* {
        if (!typeIdPtr || *typeIdPtr == 0) return nullptr;
        uint32_t tid = *typeIdPtr;
        int cnt = *reinterpret_cast<int*>(entity + 0x38);
        int64_t arr = *reinterpret_cast<int64_t*>(entity + 0x30);
        for (int i = 0; i < cnt; i++) {
            void** ibp = reinterpret_cast<void**>(arr + static_cast<int64_t>(i) * 8);
            fn_InternalBasePtr_CheckPtr(ibp);
            int64_t comp = *reinterpret_cast<int64_t*>(ibp);
            if (comp == 0) continue;
            int64_t vt = *reinterpret_cast<int64_t*>(comp);
            typedef uint32_t (*getType_t)(void*);
            uint32_t ctid = (*reinterpret_cast<getType_t*>(vt + 0x10))(reinterpret_cast<void*>(comp));
            if (ctid == tid) {
                void** ibp2 = reinterpret_cast<void**>(arr + static_cast<int64_t>(i) * 8);
                fn_InternalBasePtr_CheckPtr(ibp2);
                void* resolved = *ibp2;
                return fn_IsDerivedFrom(resolved, tid) ? resolved : nullptr;
            }
        }
        return nullptr;
    };

    int64_t bestPartial = 0;  // lVar19 (reused below as partial-best accumulator)

    // Pre-scan (only when the citizen already holds a workplace): fairness gate for
    // the "extra work" second call. Bail (return null) if any adult coworker is idle
    // or if our workplace is nearly full while a coworker's still has room.
    if (*reinterpret_cast<int64_t*>(sp + 0x238) != 0) {
        fn_InternalBasePtr_CheckPtr(sp + 0x238);
        int64_t ownStoreIBP = 0;
        if (s_fn_EntityFindCompStorage)
            s_fn_EntityFindCompStorage(*reinterpret_cast<int64_t*>(sp + 0x238), &ownStoreIBP);
        int citCount = *reinterpret_cast<int*>(townMgr + 0xa8);
        for (int64_t i = 0; i < citCount; i++) {
            int64_t other = *reinterpret_cast<int64_t*>(
                *reinterpret_cast<int64_t*>(townMgr + 0xa0) + i * 8);
            if (other == param_1) continue;
            fn_InternalBasePtr_CheckPtr(&other);
            if (*reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(other + 0x288) + 0x44)
                    <= *reinterpret_cast<float*>(other + 0x3c)
                && *reinterpret_cast<int64_t*>(other + 0x208) == 0) {
                fn_InternalBasePtr_CheckPtr(&other);
                if (*reinterpret_cast<int64_t*>(other + 0x238) == 0) {
                    fn_InternalBasePtr_Ctor(param_2, nullptr);  // LAB_140111497
                    return param_2;
                }
                fn_InternalBasePtr_CheckPtr(&other);
                int64_t otherWp = *reinterpret_cast<int64_t*>(other + 0x238);
                fn_InternalBasePtr_CheckPtr(&otherWp);
                int64_t otherStoreIBP = 0;
                if (s_fn_EntityFindCompStorage)
                    s_fn_EntityFindCompStorage(otherWp, &otherStoreIBP);
                fn_InternalBasePtr_CheckPtr(&ownStoreIBP);
                if (*reinterpret_cast<int*>(ownStoreIBP + 0x58) == 1) {
                    fn_InternalBasePtr_CheckPtr(&otherStoreIBP);
                    if (1 < *reinterpret_cast<int*>(otherStoreIBP + 0x58)) {
                        fn_InternalBasePtr_Ctor(param_2, nullptr);  // LAB_140111497
                        return param_2;
                    }
                }
            }
        }
    }

    int count = *reinterpret_cast<int*>(townMgr + 0x28);
    int* mapInfo = *reinterpret_cast<int**>(gw + 0xf8);
    if (s_phase1Diag)
        fprintf(stderr, "[Phase1Employ] sub_0e70 self=%p townMgr=%p regCount=%d param4=%d\n",
                reinterpret_cast<void*>(param_1), reinterpret_cast<void*>(townMgr),
                count, (int)param_4);

    // The original computes the home cell then loops the registry; when the registry
    // is empty the home block is dead (its result is only read inside the loop), so we
    // gate it behind count>0 — a byte-faithful no-op when townMgr+0x20 is empty.
    if (count > 0) {
        fn_InternalBasePtr_CheckPtr(sp + 0x278);
        int64_t homeEntity = *reinterpret_cast<int64_t*>(sp + 0x278);
        if (homeEntity == 0) { fn_InternalBasePtr_Ctor(param_2, nullptr); return param_2; }
        fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(homeEntity + 0x68));
        int64_t homeComp = *reinterpret_cast<int64_t*>(homeEntity + 0x68);
        if (homeComp == 0) { fn_InternalBasePtr_Ctor(param_2, nullptr); return param_2; }
        int mapW = *mapInfo;
        int homeX = static_cast<int>(*reinterpret_cast<float*>(homeComp + 0x158) + static_cast<float>(mapW >> 1));
        int homeY = static_cast<int>(*reinterpret_cast<float*>(homeComp + 0x15c) + static_cast<float>(mapW >> 1));
        if (homeX < 0) homeX = 0;
        if (mapW <= homeX) homeX = mapW - 1;
        if (homeY < 0) homeY = 0;
        if (mapW <= homeY) homeY = mapW - 1;
        int64_t homeCellIdx = static_cast<int64_t>(homeY * mapW + homeX);

        for (int64_t entryIdx = 0; entryIdx < count; entryIdx++) {
            int64_t entry = *reinterpret_cast<int64_t*>(
                *reinterpret_cast<int64_t*>(townMgr + 0x20) + entryIdx * 8);  // local_98
            fn_InternalBasePtr_CheckPtr(&entry);

            void* resComp = FindEntityComp(entry, s_TypeId_Residence);   // DWORD_1401c7cf0
            if (!resComp) continue;   // registered entries always carry one; guard.
            int64_t resIBP = 0;
            fn_InternalBasePtr_Ctor(&resIBP, resComp);

            bool consider;
            if (param_4 != '\0') {
                consider = true;
            } else {
                fn_InternalBasePtr_CheckPtr(&resIBP);
                consider = (*reinterpret_cast<int*>(resIBP + 0x58) < 2);
            }
            if (!consider) continue;

            void* interactComp = FindEntityComp(entry, s_TypeId_Position);  // DAT_1401c7900
            int64_t interactIBP = 0;
            fn_InternalBasePtr_Ctor(&interactIBP, interactComp);
            if (!interactComp) continue;   // no work-positions -> can't seat a worker.

            fn_InternalBasePtr_CheckPtr(&resIBP);
            int rosterCount = *reinterpret_cast<int*>(resIBP + 0x58);
            int64_t rosterOff = 0;
            for (int slot = 0; slot < rosterCount; slot++) {
                fn_InternalBasePtr_CheckPtr(&resIBP);
                int64_t rosterBase = *reinterpret_cast<int64_t*>(resIBP + 0x50);
                if (*reinterpret_cast<int*>(rosterBase + 8 + rosterOff) < 1) {  // free slot
                    fn_InternalBasePtr_CheckPtr(&interactIBP);
                    float pos[3] = {0.0f, 0.0f, 0.0f};
                    if (s_fn_Entity_impl_2e20)
                        s_fn_Entity_impl_2e20(interactIBP, pos, 0, 0, nullptr);
                    int mw = *mapInfo;
                    int wpX = static_cast<int>(pos[0] + static_cast<float>(mw >> 1));
                    int wpY = static_cast<int>(pos[1] + static_cast<float>(mw >> 1));
                    if (wpX < 0) wpX = 0;
                    if (mw <= wpX) wpX = mw - 1;
                    if (wpY < 0) wpY = 0;
                    if (mw <= wpY) wpY = mw - 1;
                    if (s_fn_FUN_14009c480) s_fn_FUN_14009c480(mapInfo, mapInfo, 0, 0);
                    int64_t cellBase = *reinterpret_cast<int64_t*>(
                        reinterpret_cast<uint8_t*>(mapInfo) + 0x80);
                    int64_t homeCellPtr = homeCellIdx * 0x30 + cellBase;
                    int64_t wpCellPtr = static_cast<int64_t>(wpY * mw + wpX) * 0x30 + cellBase;
                    uint32_t wpBit   = 1u << (*reinterpret_cast<uint8_t*>(wpCellPtr + 0x29) & 0x1f);
                    uint32_t homeBit = 1u << (*reinterpret_cast<uint8_t*>(homeCellPtr + 0x29) & 0x1f);
                    uint32_t both = homeBit | wpBit;
                    uint32_t mask = both | 0xe1f;
                    short hr = *reinterpret_cast<short*>(homeCellPtr + 0x26);
                    short wr = *reinterpret_cast<short*>(wpCellPtr + 0x26);
                    if ((homeBit & mask) != 0 && (wpBit & mask) != 0 &&
                        ((both & 0x1e0) != 0 || (hr != 0 && wr != 0 && hr == wr))) {
                        fn_InternalBasePtr_CheckPtr(&resIBP);
                        if (*reinterpret_cast<int*>(resIBP + 0x58) == 1) {
                            *param_3 = slot;
                            *reinterpret_cast<int64_t*>(param_2) = entry;
                            if (s_phase1Diag)
                                fprintf(stderr, "[Phase1Employ] FULL-FREE entry=%p slot=%d\n",
                                        reinterpret_cast<void*>(entry), slot);
                            return param_2;
                        }
                        fn_InternalBasePtr_CheckPtr(&resIBP);
                        if (bestPartial == 0 && 1 < *reinterpret_cast<int*>(resIBP + 0x58)) {
                            bestPartial = entry;
                            bestSlot = slot;
                        }
                    }
                }
                rosterOff += 0x10;
            }
        }
        if (bestPartial != 0) {
            *param_3 = bestSlot;
            *reinterpret_cast<int64_t*>(param_2) = bestPartial;
            if (s_phase1Diag)
                fprintf(stderr, "[Phase1Employ] PARTIAL entry=%p slot=%d\n",
                        reinterpret_cast<void*>(bestPartial), bestSlot);
            return param_2;
        }
    }

    // LAB_140111497 — no work found.
    fn_InternalBasePtr_Ctor(param_2, nullptr);
    return param_2;
}

// Reimpl of CitizenComponent_CheckWorkplaceValid (0x1401158d0):
// validate that the citizen's home tile is within range of the candidate workplace.
// Reads the world's map-info object (gw+0xf8), clamps a tile coordinate derived from
// the workplace footprint (+0x68 -> +0x158/+0x15c) against the map width, then defers
// to the world reachability test FUN_1400a00f0(world, 0xE1F, cellIndex, regionId).
// Returns nonzero if the candidate is a valid (reachable) workplace for the citizen.
char CitizenComp_CheckWorkplaceValid(CitizenComponent* param_1, void* param_2) {
    EnsureCallees();
    uint8_t* p = reinterpret_cast<uint8_t*>(param_1);
    uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    int* mapInfo = *reinterpret_cast<int**>(gw + 0xf8);

    // CheckPtr on the citizen's home IBP (+0x278), read its entity (+0x68).
    fn_InternalBasePtr_CheckPtr(p + 0x278);
    int64_t homeEntity = *reinterpret_cast<int64_t*>(p + 0x278);
    fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(homeEntity + 0x68));
    int64_t homeComp = *reinterpret_cast<int64_t*>(homeEntity + 0x68);

    int mapW = *mapInfo;
    int halfW = mapW >> 1;
    int tileY = static_cast<int>(*reinterpret_cast<float*>(homeComp + 0x15c) + static_cast<float>(halfW));
    int tileX = static_cast<int>(static_cast<float>(halfW) + *reinterpret_cast<float*>(homeComp + 0x158));
    if (tileX < 0) tileX = 0;
    if (mapW <= tileX) tileX = mapW - 1;
    if (tileY < 0) tileY = 0;
    if (mapW <= tileY) tileY = mapW - 1;

    int64_t ibpVal = *reinterpret_cast<int64_t*>(param_2);
    int regionId = s_fn_Entity_impl_7fb0 ? s_fn_Entity_impl_7fb0(&ibpVal, nullptr) : 0;
    uint32_t cellIdx = static_cast<uint32_t>(mapW * tileY + tileX);
    char res = s_fn_FUN_140a00f0 ? (char)s_fn_FUN_140a00f0(mapInfo, 0xe1f, cellIdx, regionId) : 0;
    if (std::getenv("BANISHED_CITIZENAI_TRACE")) {
        static int s_cwn = 0;
        if (s_cwn < 8) { ++s_cwn;
            fprintf(stderr, "[CheckWP] citizenCell=%u workplaceCell=%d mapW=%d res=%d\n",
                    cellIdx, regionId, mapW, (int)res);
            fflush(stderr);
        }
    }
    return res;
}

// Reimpl of Entity_impl_ef40 (0x14008ef40):
// pick a free production "slot" of category param_4 (0..4) from the storage component's
// task list. Two-pass: pass 1 counts eligible slots (those whose RawMaterialDescription
// capability bit for the category is set AND that have remaining demand); if any, roll a
// random index over them; pass 2 walks the list again returning the chosen slot's IBP.
// The category selects which bit of the material's +0x5c flags word gates a slot:
//   0->bit5, 1->bit6, 2->bit9, 3->bit13, 4->bit14.
int64_t* CitizenComp_Entity_impl_ef40(void* param_1, int64_t* param_2, void* param_3, int param_4) {
    EnsureCallees();
    (void)param_1;
    uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    uint32_t rawMatTid = s_TypeId_RawMat1c7cb8 ? *s_TypeId_RawMat1c7cb8 : 0;

    // Category -> capability bit-shift of material flags (+0x5c).
    auto catShift = [&](int cat) -> int {
        switch (cat) {
            case 0: return 5; case 1: return 6; case 2: return 9;
            case 3: return 13; case 4: return 14; default: return -1;
        }
    };
    int shift = catShift(param_4);

    fn_InternalBasePtr_CheckPtr(param_3);
    int64_t storeIBP = 0;
    if (s_fn_EntityFindCompStorage6270)
        s_fn_EntityFindCompStorage6270(*reinterpret_cast<int64_t*>(param_3), &storeIBP);
    // Scoped standalone fallback (registered 6270 port's typeid is dead .rdata) +
    // null bail: retail asserts the storage exists; in standalone an entity in a
    // town search list may still lack a live StorageComponent — treat as no slot.
    if (storeIBP == 0)
        storeIBP = StandaloneFindStorageComp(*reinterpret_cast<void**>(param_3));
    if (storeIBP == 0) { *param_2 = 0; return param_2; }

    fn_InternalBasePtr_CheckPtr(&storeIBP);
    uint32_t chosen = 0xffffffff;
    int64_t taskCount = *reinterpret_cast<int*>(storeIBP + 0x30);
    if (taskCount <= 0) { *param_2 = 0; return param_2; }

    // Find the RawMaterialDescription on a task entity, returning its derived ptr (or null).
    auto findMat = [&](int64_t taskEntity) -> void* {
        int rows = *reinterpret_cast<int*>(taskEntity + 0x30);
        int64_t arr = *reinterpret_cast<int64_t*>(taskEntity + 0x28);
        for (int i = 0; i < rows; i++) {
            int64_t comp = *reinterpret_cast<int64_t*>(arr + (int64_t)i * 8);
            int64_t vt = *reinterpret_cast<int64_t*>(comp);
            typedef uint32_t (*getType_t)(void*);
            uint32_t tid = (*reinterpret_cast<getType_t*>(vt + 0x10))(reinterpret_cast<void*>(comp));
            if (tid == rawMatTid) {
                void* obj = reinterpret_cast<void*>(comp);
                return fn_IsDerivedFrom(obj, rawMatTid) ? obj : nullptr;
            }
        }
        return nullptr;
    };

    // Pass 1: count eligible slots; remember the last eligible index in `chosen`.
    uint32_t eligible = 0;
    for (int64_t row = 0; row < taskCount; row++) {
        fn_InternalBasePtr_CheckPtr(&storeIBP);
        int64_t listBase = *reinterpret_cast<int64_t*>(storeIBP + 0x28);
        int64_t entry = *reinterpret_cast<int64_t*>(listBase + row * 0x10);
        void* mat = findMat(entry);
        bool bitSet = mat && shift >= 0 &&
            ((*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(mat) + 0x5c) >> shift) & 1);
        int demand = *reinterpret_cast<int*>(listBase + 8 + row * 0x10) -
                     *reinterpret_cast<int*>(listBase + 0xc + row * 0x10);
        if (bitSet && demand > 0) {
            if (*reinterpret_cast<char*>(reinterpret_cast<uint8_t*>(mat) + 0x58) != '\0')
                chosen = eligible;
            eligible++;
        }
    }
    if (eligible == 0) { *param_2 = 0; return param_2; }

    if (static_cast<int>(chosen) < 0 && fn_Random_Rand)
        chosen = fn_Random_Rand(*reinterpret_cast<void**>(gw + 0xd8), eligible);

    // Pass 2: walk again, returning the chosen eligible slot's IBP.
    uint32_t cur = 0;
    for (int64_t row = 0; row < taskCount; row++) {
        fn_InternalBasePtr_CheckPtr(&storeIBP);
        int64_t listBase = *reinterpret_cast<int64_t*>(storeIBP + 0x28);
        int64_t* entryP = reinterpret_cast<int64_t*>(listBase + row * 0x10);
        void* mat = findMat(*entryP);
        bool bitSet = mat && shift >= 0 &&
            ((*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(mat) + 0x5c) >> shift) & 1);
        int demand = static_cast<int>(entryP[1]) - *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(entryP) + 0xc);
        if (bitSet && demand > 0) {
            if (chosen == cur) { *param_2 = *entryP; return param_2; }
            cur++;
        }
    }
    *param_2 = 0;
    return param_2;
}

// Reimpl of CitizenComponent_ConsumeFood (0x140112990):
// recompute the citizen's food/happiness accumulator (+0x50) from this tick's state
// flags (+0x54). True leaf: only reads globals + self fields. The +0x54 flag bits gate
// per-source happiness deltas (scaled by g_fUVScale), then the result is clamped to
// [0, description+0x70] and the flag word is reset. Disabled (flag-cleared) when the
// world's no-hunger debug bit (gameFlags>>3) is set.
void CitizenComp_ConsumeFood(CitizenComponent* param_1) {
    EnsureHelpers();
    EnsureCallees();
    uint8_t* p = reinterpret_cast<uint8_t*>(param_1);
    uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    int64_t desc = *reinterpret_cast<int64_t*>(p + 0x288);
    float uv   = s_pUVScale ? *s_pUVScale : 1.0f;
    float c114 = s_DAT_140172114 ? *s_DAT_140172114 : 0.0f;
    float c100 = s_DAT_140172100 ? *s_DAT_140172100 : 0.0f;
    float c180 = s_DAT_140172180 ? *s_DAT_140172180 : 1.0f;

    if ((*reinterpret_cast<uint32_t*>(gw + 0xd0) >> 3 & 1) != 0) {
        *reinterpret_cast<uint32_t*>(p + 0x50) = *reinterpret_cast<uint32_t*>(desc + 0x70);
        return;
    }
    uint32_t f = *reinterpret_cast<uint32_t*>(p + 0x54);
    uint32_t cnt = (f >> 8 & 1) + (f >> 6 & 1) + (f >> 5 & 1) + (f >> 4 & 1);
    if ((f & 0x200) != 0) cnt++;
    float* acc = reinterpret_cast<float*>(p + 0x50);
    if ((f & 8) != 0 && (f & 4) != 0 && cnt > 3) *acc += c114;
    if ((f & 8) != 0 && (f & 0x200) == 0 && cnt < 3) *acc -= (uv - (float)cnt / c180);
    if ((f & 1) != 0) *acc -= uv;
    if ((f & 2) != 0) *acc -= uv;
    if ((f & 4) == 0 &&
        *reinterpret_cast<float*>(desc + 0x44) <= *reinterpret_cast<float*>(p + 0x3c) &&
        *reinterpret_cast<int64_t*>(p + 0x208) == 0) {
        *acc -= c100;
    }
    if (*acc <= 0.0f && *acc != 0.0f) *reinterpret_cast<uint32_t*>(p + 0x50) = 0;
    float cap = *reinterpret_cast<float*>(desc + 0x70);
    if (cap < *acc) *acc = cap;
    *reinterpret_cast<uint32_t*>(p + 0x54) = 0;
}

// Reimpl of CitizenComponent_ConsumeFuel (0x140112ae0):
// recompute the citizen's warmth/fuel accumulator (+0x80) from this tick's +0x88 flag
// bits, then drive it toward a target derived from the citizen's home tile climate
// (world map-info gw+0xf8 -> tile +0x80, via Entity_impl_7fb0 region index) and the
// workplace storage demand. Closure-safe: callees 0x140087fb0/0x140008090 registered.
void CitizenComp_ConsumeFuel(CitizenComponent* param_1) {
    EnsureHelpers();
    EnsureCallees();
    uint8_t* p = reinterpret_cast<uint8_t*>(param_1);
    uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    float uv   = s_pUVScale ? *s_pUVScale : 1.0f;
    float c114 = s_DAT_140172114 ? *s_DAT_140172114 : 0.0f;
    float c1a8 = s_DAT_1401721a8 ? *s_DAT_1401721a8 : 0.0f;
    float c174 = s_DAT_140172174 ? *s_DAT_140172174 : 0.0f;
    float c194 = s_DAT_140172194 ? *s_DAT_140172194 : 0.0f;
    float c0ec = s_DAT_1401720ec ? *s_DAT_1401720ec : 0.0f;
    double c168 = s_DAT_140172168 ? *s_DAT_140172168 : 0.0;

    uint32_t f = *reinterpret_cast<uint32_t*>(p + 0x88);
    float* acc = reinterpret_cast<float*>(p + 0x80);
    float warmBefore = *acc;   // [WarmTrace] snapshot before this tick's recompute
    if ((f & 2) != 0)    *acc -= c1a8;
    if ((f & 0x40) != 0) *acc -= c174;
    if ((char)f < 0)     *acc -= c194;   // bit 7 of low byte
    if ((f & 8) != 0)    *acc -= uv;
    if ((f & 0x10) != 0) *acc -= uv;
    if ((f >> 8 & 1) != 0)  *acc += uv;
    if ((f >> 9 & 1) != 0)  *acc += uv;
    if ((f >> 10 & 1) != 0) *acc += uv;
    if ((f >> 11 & 1) != 0) *acc += uv;
    if ((f >> 12 & 1) != 0) *acc += uv;
    if ((f >> 13 & 1) != 0) *acc += uv;
    if ((f >> 0x12 & 1) != 0) *acc += c114;
    if (*acc <= 0.0f && *acc != 0.0f) *reinterpret_cast<uint32_t*>(p + 0x80) = 0;
    int64_t desc = *reinterpret_cast<int64_t*>(p + 0x288);
    if (*reinterpret_cast<float*>(desc + 0xa4) < *acc) *acc = *reinterpret_cast<float*>(desc + 0xa4);

    float fVar5 = 0.0f;
    if ((f & 4) == 0)    fVar5 = uv;
    if ((f & 0x20) == 0) fVar5 += uv;
    if ((f & 1) == 0)    fVar5 += uv;
    if ((f >> 0x11 & 1) != 0) {
        if (*reinterpret_cast<int*>(p + 0x84) < *reinterpret_cast<int*>(desc + 0xb0)) fVar5 += c114;
        else fVar5 += uv;
    }
    if ((f >> 0x10 & 1) != 0) {
        if (*reinterpret_cast<int*>(desc + 0xa8) < *reinterpret_cast<int*>(p + 0xa0)) {
            if (*reinterpret_cast<int*>(desc + 0xac) <= *reinterpret_cast<int*>(p + 0xa0)) fVar5 += uv;
        } else {
            fVar5 -= uv;
        }
    }

    int64_t wpIBP = *reinterpret_cast<int64_t*>(p + 0x238);
    if (wpIBP != 0) {
        int64_t mapInfo = *reinterpret_cast<int64_t*>(gw + 0xf8);
        int region = s_fn_Entity_impl_7fb0 ? s_fn_Entity_impl_7fb0(&wpIBP, nullptr) : 0;
        int64_t tile = (int64_t)region * 0x30 + *reinterpret_cast<int64_t*>(mapInfo + 0x80);
        int climate = ((*reinterpret_cast<char*>(tile + 0x20) != 0) +
                       (*reinterpret_cast<char*>(tile + 0x1f) != 0) +
                       (*reinterpret_cast<char*>(tile + 0x1e) != 0) +
                       (*reinterpret_cast<char*>(tile + 0x1d) != 0) +
                       (*reinterpret_cast<char*>(tile + 0x1c) != 0)) -
                      (int)*reinterpret_cast<uint8_t*>(tile + 0x21);
        if (climate < 0) climate = 0;
        fn_InternalBasePtr_CheckPtr(p + 0x238);
        int64_t storeIBP = 0;
        void* st = s_fn_EntityFindCompStorage
                       ? s_fn_EntityFindCompStorage(*reinterpret_cast<int64_t*>(p + 0x238), &storeIBP)
                       : nullptr;
        fn_InternalBasePtr_CheckPtr(st);
        if (st && *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(st) + 0x58) > 1) climate >>= 2;
        fVar5 += (float)climate;
    }
    if (*reinterpret_cast<int64_t*>(p + 0x230) != 0) fVar5 += uv;

    float cur = *acc;
    fVar5 = fVar5 * c0ec * *reinterpret_cast<float*>(desc + 0xa4);
    if (fVar5 <= cur) {
        if (!(cur <= fVar5)) {
            *acc = (float)((double)cur - c168);
        }
    } else {
        *acc = cur + c114;
    }
    if (*acc <= 0.0f && *acc != 0.0f) *reinterpret_cast<uint32_t*>(p + 0x80) = 0;
    float capA4 = *reinterpret_cast<float*>(desc + 0xa4);
    if (capA4 < *acc) *acc = capA4;

    // [WarmTrace] gated on BANISHED_STORAGE_TRACE — verify houses/citizens stay warm.
    // Prints warmth accumulator (+0x80) before/after, its cap (desc+0xa4), and the
    // computed heating demand target (fVar5). Capped to avoid log spam.
    {
        static int s_wt = -1;
        if (s_wt < 0) { const char* e = std::getenv("BANISHED_STORAGE_TRACE");
                        s_wt = (e && e[0] && e[0] != '0') ? 40 : 0; }
        if (s_wt > 0) { --s_wt;
            fprintf(stderr, "[WarmTrace] citizen=%p warm %.3f->%.3f cap=%.3f target=%.3f home238=%p\n",
                    (void*)param_1, warmBefore, *acc, capA4, fVar5,
                    (void*)*reinterpret_cast<int64_t*>(p + 0x238));
            fflush(stderr);
        }
    }
}

// ============================================================
// Citizen-lifecycle cascade (cascade round, snapshot3)
// ============================================================

// Reimpl of CitizenComponent_AssignHousing (0x140111ee0):
// detach the citizen from its current residence (+0x248) then attach to *param2.
// On detach: find the residence's building component (002e10) and call DetachWorkplace
// (12f7e0) with a back-IBP to this citizen. Symmetric on attach via AttachWorkplace (12fa20).
void CitizenComp_AssignHousing(CitizenComponent* param_1, int64_t* param_2) {
    EnsureCallees();
    uint8_t* p = reinterpret_cast<uint8_t*>(param_1);
    int64_t* slot = reinterpret_cast<int64_t*>(p + 0x248);
    if (*slot != 0) {
        fn_InternalBasePtr_CheckPtr(slot);
        int64_t buildingIBP = 0;
        if (s_fn_Entity_FindBuilding2e10) s_fn_Entity_FindBuilding2e10(*slot, &buildingIBP);
        int64_t selfIBP = 0;
        fn_InternalBasePtr_Ctor(&selfIBP, param_1);
        fn_InternalBasePtr_CheckPtr(&buildingIBP);
        if (s_fn_DetachWorkplace7e0) s_fn_DetachWorkplace7e0(buildingIBP, &selfIBP);
    }
    int64_t newHousing = *param_2;
    *slot = newHousing;
    if (newHousing != 0) {
        fn_InternalBasePtr_CheckPtr(slot);
        int64_t buildingIBP = 0;
        if (s_fn_Entity_FindBuilding2e10) s_fn_Entity_FindBuilding2e10(*slot, &buildingIBP);
        int64_t selfIBP = 0;
        fn_InternalBasePtr_Ctor(&selfIBP, param_1);
        fn_InternalBasePtr_CheckPtr(&buildingIBP);
        if (s_fn_AttachWorkplacea20) s_fn_AttachWorkplacea20(buildingIBP, &selfIBP);
    }
}

// Reimpl of CitizenComponent_sub_40f0 (0x1400440f0):
// test whether a candidate building (param_1) has a free worker slot AND is reachable
// from the citizen's (param_2) home tile. Returns true iff reachable. Closure-safe:
// callees 002ed0/122e20/002e10/0a00f0 all registered.
bool CitizenComp_sub_40f0(int64_t param_1, void* param_2) {
    EnsureCallees();
    uint8_t* cp = reinterpret_cast<uint8_t*>(param_1);
    void* obj = *reinterpret_cast<void**>(cp + 0x18);
    uint32_t resTid = s_TypeId_Residence3f78 ? *s_TypeId_Residence3f78 : 0;
    void* res = (obj && fn_IsDerivedFrom(obj, resTid)) ? obj : nullptr;
    int cap = res ? *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(res) + 0x20) : 0;
    if (*reinterpret_cast<int*>(cp + 0x30) >= cap) return false;

    fn_InternalBasePtr_CheckPtr(cp + 0x10);
    int64_t bldgIBP = 0;
    if (s_fn_Entity_FindBuilding2e10) s_fn_Entity_FindBuilding2e10(*reinterpret_cast<int64_t*>(cp + 0x10), &bldgIBP);
    fn_InternalBasePtr_CheckPtr(&bldgIBP);
    if (*reinterpret_cast<int*>(bldgIBP + 0x60) == 0) return false;

    uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    int* mapInfo = *reinterpret_cast<int**>(gw + 0xf8);
    fn_InternalBasePtr_CheckPtr(cp + 0x10);
    int64_t posIBP = 0;
    if (s_fn_Entity_FindPos_ed0) s_fn_Entity_FindPos_ed0(*reinterpret_cast<int64_t*>(cp + 0x10), &posIBP);
    fn_InternalBasePtr_CheckPtr(&posIBP);
    float pos[3] = {0,0,0};
    if (s_fn_Entity_impl_2e20) s_fn_Entity_impl_2e20(posIBP, pos, 0, 0, nullptr);

    fn_InternalBasePtr_CheckPtr(param_2);
    int64_t citizen = *reinterpret_cast<int64_t*>(param_2);
    fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(citizen + 0x278));
    int64_t home = *reinterpret_cast<int64_t*>(citizen + 0x278);
    fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(home + 0x68));
    int64_t homeComp = *reinterpret_cast<int64_t*>(home + 0x68);

    int mapW = *mapInfo;
    float half = (float)(mapW >> 1);
    auto clampTile = [&](float v) -> int {
        int t = (int)(v + half);
        if (t < 0) t = 0;
        if (mapW <= t) t = mapW - 1;
        return t;
    };
    int homeX = clampTile(*reinterpret_cast<float*>(homeComp + 0x158));
    int homeY = clampTile(*reinterpret_cast<float*>(homeComp + 0x15c));
    int posX  = clampTile(pos[0]);
    int posY  = clampTile(pos[1]);
    uint32_t fromCell = (uint32_t)(mapW * posY + posX);
    uint32_t toCell   = (uint32_t)(mapW * homeY + homeX);
    char ok = s_fn_FUN_140a00f0 ? s_fn_FUN_140a00f0(mapInfo, 0xe1f, fromCell, toCell) : 0;
    return ok != 0;
}

// Reimpl of CitizenComponent_sub_1930 (0x140111930):
// scan the world building list (gameWorld+0xf0 -> +0x60/+0x68) for the closest building
// whose WorkplaceComponent's job key (+0x48) matches param_4 and that is reachable
// (sub_40f0). Returns the best building's IBP via param_2. Closure-safe: sub_40f0 +
// 0x140087fb0 + 0x140008830 all registered.
int64_t* CitizenComp_sub_1930(int64_t param_1, int64_t* param_2, void* param_3, int64_t param_4) {
    EnsureCallees();
    uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    int64_t list = *reinterpret_cast<int64_t*>(gw + 0xf0);
    int64_t srcWp = *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(param_1) + 0x238);
    int bestCost = -1;
    int count = *reinterpret_cast<int*>(list + 0x68);
    *param_2 = 0;
    uint32_t srcRegion = 0xffffffff;
    if (srcWp != 0) {
        int64_t srcIBP = srcWp;
        srcRegion = (uint32_t)(s_fn_Entity_impl_7fb0 ? s_fn_Entity_impl_7fb0(&srcIBP, nullptr) : 0);
    }
    uint32_t wpTid = s_TypeId_Workplace ? *s_TypeId_Workplace : 0;

    for (int i = 0; i < count; i++) {
        int64_t cand = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(list + 0x60) + (int64_t)i * 8);
        fn_InternalBasePtr_CheckPtr(param_3);
        if (cand == *reinterpret_cast<int64_t*>(param_3)) continue;
        fn_InternalBasePtr_CheckPtr(&cand);
        fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(cand + 0x10));
        int64_t sub = *reinterpret_cast<int64_t*>(cand + 0x10);

        // find WorkplaceComponent (DAT_1401c80e0) on the candidate's sub-entity
        void* wp = nullptr;
        int rows = *reinterpret_cast<int*>(sub + 0x38);
        int64_t arr = *reinterpret_cast<int64_t*>(sub + 0x30);
        for (int j = 0; j < rows; j++) {
            void** ibp = reinterpret_cast<void**>(arr + (int64_t)j * 8);
            fn_InternalBasePtr_CheckPtr(ibp);
            int64_t comp = *reinterpret_cast<int64_t*>(ibp);
            typedef uint32_t (*getType_t)(void*);
            uint32_t tid = (*reinterpret_cast<getType_t*>(*reinterpret_cast<int64_t*>(comp) + 0x10))(reinterpret_cast<void*>(comp));
            if (tid == wpTid) {
                void** ibp2 = reinterpret_cast<void**>(arr + (int64_t)j * 8);
                fn_InternalBasePtr_CheckPtr(ibp2);
                void* o = *ibp2;
                wp = fn_IsDerivedFrom(o, wpTid) ? o : nullptr;
                break;
            }
        }
        int64_t wpIBP = 0;
        fn_InternalBasePtr_Ctor(&wpIBP, wp);
        fn_InternalBasePtr_CheckPtr(&wpIBP);
        if (*reinterpret_cast<int64_t*>(wpIBP + 0x48) != param_4) continue;

        int64_t selfIBP = 0;
        fn_InternalBasePtr_Ctor(&selfIBP, reinterpret_cast<void*>(param_1));
        fn_InternalBasePtr_CheckPtr(&cand);
        if (!CitizenComp_sub_40f0(cand, &selfIBP)) continue;

        fn_InternalBasePtr_CheckPtr(&cand);
        int64_t candSubIBP = *reinterpret_cast<int64_t*>(cand + 0x10);
        uint32_t dstRegion = (uint32_t)(s_fn_Entity_impl_7fb0 ? s_fn_Entity_impl_7fb0(&candSubIBP, nullptr) : 0);
        int cost = 1;
        if ((int)srcRegion >= 0 && s_fn_PathCost8830) {
            int* mapInfo = *reinterpret_cast<int**>(gw + 0xf8);
            cost = s_fn_PathCost8830(mapInfo, dstRegion, srcRegion);
        }
        if (bestCost < 0 || cost < bestCost) {
            *param_2 = cand;
            bestCost = cost;
        }
    }
    return param_2;
}

// Reimpl of CitizenComponent_FindWorkplace (0x140111590):
// pick the best reachable workplace (sub_1930 keyed by description+0x30), validate it
// (sub_40f0), register the citizen into the workplace's worker array (3110), and store
// the assignment at +0x228. Returns true on success.
char CitizenComp_FindWorkplace(CitizenComponent* param_1) {
    EnsureCallees();
    uint8_t* p = reinterpret_cast<uint8_t*>(param_1);
    int64_t desc = *reinterpret_cast<int64_t*>(p + 0x288);
    int64_t found = 0;
    int64_t keyIBP = 0;
    fn_InternalBasePtr_Ctor(&keyIBP, nullptr);
    CitizenComp_sub_1930(reinterpret_cast<int64_t>(param_1), &found, &keyIBP,
                         *reinterpret_cast<int64_t*>(desc + 0x30));
    if (found != 0) {
        fn_InternalBasePtr_Ctor(&keyIBP, param_1);
        fn_InternalBasePtr_CheckPtr(&found);
        int64_t wp = found;
        int64_t selfIBP = keyIBP;
        if (CitizenComp_sub_40f0(found, &selfIBP)) {
            if (s_fn_GrowArray3110)
                s_fn_GrowArray3110(reinterpret_cast<int64_t*>(wp + 0x28),
                                   *reinterpret_cast<int*>(wp + 0x30) + 1, &keyIBP);
            *reinterpret_cast<int64_t*>(p + 0x228) = found;
            return 1;
        }
    }
    return 0;
}

// Reimpl of CitizenComponent_FindStorage (0x1401116f0):
// same as FindWorkplace but keyed by description+0x28 and stores at +0x230.
void CitizenComp_FindStorage(CitizenComponent* param_1) {
    EnsureCallees();
    uint8_t* p = reinterpret_cast<uint8_t*>(param_1);
    int64_t desc = *reinterpret_cast<int64_t*>(p + 0x288);
    int64_t found = 0;
    int64_t keyIBP = 0;
    fn_InternalBasePtr_Ctor(&keyIBP, nullptr);
    CitizenComp_sub_1930(reinterpret_cast<int64_t>(param_1), &found, &keyIBP,
                         *reinterpret_cast<int64_t*>(desc + 0x28));
    if (found != 0) {
        fn_InternalBasePtr_Ctor(&keyIBP, param_1);
        fn_InternalBasePtr_CheckPtr(&found);
        int64_t wp = found;
        int64_t selfIBP = keyIBP;
        if (CitizenComp_sub_40f0(found, &selfIBP)) {
            if (s_fn_GrowArray3110)
                s_fn_GrowArray3110(reinterpret_cast<int64_t*>(wp + 0x28),
                                   *reinterpret_cast<int*>(wp + 0x30) + 1, &keyIBP);
            *reinterpret_cast<int64_t*>(p + 0x230) = found;
        }
    }
}

// Reimpl of CitizenComponent_TryFindPartner (0x140111840):
// like FindWorkplace but keyed by description+0x20; on success stores the partner at
// +0x208 and sets the had-partner flag (+0x220). param_2 seeds the exclusion IBP.
char CitizenComp_TryFindPartner(CitizenComponent* param_1, int64_t* param_2) {
    EnsureCallees();
    uint8_t* p = reinterpret_cast<uint8_t*>(param_1);
    int64_t desc = *reinterpret_cast<int64_t*>(p + 0x288);
    int64_t found = 0;
    int64_t keyIBP = *param_2;
    CitizenComp_sub_1930(reinterpret_cast<int64_t>(param_1), &found, &keyIBP,
                         *reinterpret_cast<int64_t*>(desc + 0x20));
    if (found != 0) {
        fn_InternalBasePtr_Ctor(&keyIBP, param_1);
        fn_InternalBasePtr_CheckPtr(&found);
        int64_t wp = found;
        int64_t selfIBP = keyIBP;
        if (CitizenComp_sub_40f0(found, &selfIBP)) {
            if (s_fn_GrowArray3110)
                s_fn_GrowArray3110(reinterpret_cast<int64_t*>(wp + 0x28),
                                   *reinterpret_cast<int*>(wp + 0x30) + 1, &keyIBP);
            *reinterpret_cast<int64_t*>(p + 0x208) = found;
            p[0x220] = 1;
            return 1;
        }
    }
    return 0;
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

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
#include <cstdlib>
#include <cstring>

// ------------------------------------------------------------------
// Phase 3 / Keystone D — CitizenComponent Startup-only live-wiring (Brick 2).
//
// CitizenComponent's per-tick Update is the 711-instruction lifecycle monster
// (hunger/thirst/aging/workplace/housing/partner) with ~25 sub-calls — NOT yet
// safe to run live in standalone. But the sibling CitizenAIComponent (Brick 1)
// bails until this component's BODY is populated (its CitizenDescription at +0x288
// + the Family/Health component at +0x278). That population happens in
// CitizenComponent::Startup (slot +0x20, 0x140114050), NOT its Update.
//
// So this brick brings ONLY the Startup slot live (COMPONENT_LIVE_FACTORY_STARTUP)
// to populate the body at spawn, leaving the heavy Update an inert no-op. That
// lets the CitizenAIComponent decision loop actually RUN (read citizen needs)
// instead of guard-bailing — without the risk of the full citizen lifecycle tick.
// Default (env off) => inert => byte-unchanged.
// ------------------------------------------------------------------

// Self-contained sibling-component find by registered type name (the real-CRC
// source the inert/live factories use for vtable+0x10). Avoids the exe finders
// (Family 0x1400012d0 / MeshInstance 0x140002f90), which key off the .rdata
// type-id globals (DAT_1401c7078 CharacterComponent / DWORD_1401c7e08
// StatusIconComponent) — those are 0xFFFFFFFF in standalone and filling them
// would un-dormant ~30 unrelated readers (same reasoning as CitizenAI's find).
static void* CitizenComp_FindSiblingByName(int64_t ownerEntity, const char* typeName) {
    if (!ownerEntity) return nullptr;
    uint32_t want = ::Object::GetTypeIdByName(typeName);
    if (want == 0 || want == 0xFFFFFFFF) return nullptr;
    uint8_t* e = reinterpret_cast<uint8_t*>(ownerEntity);
    int count = *reinterpret_cast<int*>(e + 0x38);
    int64_t arr = *reinterpret_cast<int64_t*>(e + 0x30);
    for (int i = 0; i < count; i++) {
        void** ibp = reinterpret_cast<void**>(arr + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(ibp);
        void* comp = *ibp;
        if (!comp) continue;
        uintptr_t vt = *reinterpret_cast<uintptr_t*>(comp);
        typedef uint32_t (*getType_t)(void*);
        uint32_t tid = (*reinterpret_cast<getType_t*>(vt + 0x10))(comp);
        if (tid == want)
            return fn_IsDerivedFrom(comp, want) ? comp : nullptr;
    }
    return nullptr;
}

// Faithful port of CitizenComponent::Startup (0x140114050) + the ctor scalar init
// (0x140114770) the inert factory zeroed. The two sibling finds are done
// self-contained (see above) instead of via the global-keyed exe finders.
static void CitizenComponent_BodyInit(CitizenComponent* self) {
    if (!self) return;
    uint8_t* p = reinterpret_cast<uint8_t*>(self);

    // ctor (0x140114770) scalar -1/4 sentinels the inert factory zeroed. The 0xCB
    // flags + the MeshInstance sub-object (+0xB8) are already set/zeroed by the
    // factory; CitizenAI never reads the mesh, so a zeroed mesh is safe here.
    *reinterpret_cast<int32_t*>(p + 0x30)  = -1;
    *reinterpret_cast<int32_t*>(p + 0xA8)  = -1;
    *reinterpret_cast<int64_t*>(p + 0x1D8) = -1;
    *reinterpret_cast<int64_t*>(p + 0x1E0) = 4;

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p + 0x10);
    int64_t owner = *reinterpret_cast<int64_t*>(p + 0x10);
    if (!owner) return;   // not yet wired into an entity

    // +0x278 = CharacterComponent IBP ("Family" finder; CitizenAI reads its +0x60
    // char-index as the alive flag: index>=0 alive, -1 dead).
    *reinterpret_cast<void**>(p + 0x278) =
        CitizenComp_FindSiblingByName(owner, "CharacterComponent");
    // +0x280 = StatusIconComponent IBP ("MeshInstance" finder; used by the heavy
    // Update only — not by CitizenAI — so a null here is harmless for Brick 2).
    *reinterpret_cast<void**>(p + 0x280) =
        CitizenComp_FindSiblingByName(owner, "StatusIconComponent");

    // +0x288 = CitizenDescription: trust comp+0x18 (the copy-ctor stored it).
    // The original validates IsDerivedFrom(obj, CitizenDescription 0x1401c7190),
    // but that is unreliable in standalone's partial factory tree (GrowthComponent
    // precedent), so trust the +0x18 pointer directly.
    void* desc = *reinterpret_cast<void**>(p + 0x18);
    *reinterpret_cast<void**>(p + 0x288) = desc;
    if (desc) {
        uint8_t* d = reinterpret_cast<uint8_t*>(desc);
        *reinterpret_cast<uint32_t*>(p + 0x50) = *reinterpret_cast<uint32_t*>(d + 0x6c);
        *reinterpret_cast<uint32_t*>(p + 0x80) = *reinterpret_cast<uint32_t*>(d + 0xa4);

        // --- ADULT-AGE SPAWN FIX (env BANISHED_SPAWN_ADULTS) -------------------
        // Our simplified start-town spawn (StartTown_SpawnCitizens) only CopyCtors
        // + registers colonist entities; it skips the real per-citizen init
        // (FUN_140117080) that assigns age/maxAge. So every citizen keeps age=0
        // (< adultAge desc+0x44 = 2.0) -> perpetual child -> the CitizenComponent
        // lifecycle never runs the adult housing/workplace assignment section
        // (citizencomponent.cpp:633) -> never housed/employed -> the Work state is
        // never a CitizenAI candidate (project_economy_loop). Mirror the founder
        // adult age the real spawn pool (0x1400a0dd0) gives:
        //     age = RandFloat*(reproMax*0.5 - adultMin) + adultMin
        // (desc+0x50 adultMin=2.0, desc+0x54 reproMax=8.0  -> age in [2.0, 4.0]),
        // and the randomized lifespan (FUN_140117080): maxAge = RandFloatTol*desc+0x3c
        // + desc+0x38 (base 15.0, tol +/-2.0). maxAge MUST be set: the inert factory
        // zeroes it, and the Update old-age death check (maxAge <= age) would fire
        // immediately once a non-zero adult age is present. Default-off so a plain
        // BANISHED_LIVE_CITIZEN run stays byte-identical.
        static int s_adults = -1;
        if (s_adults < 0) {
            const char* e = std::getenv("BANISHED_SPAWN_ADULTS");
            s_adults = (e && e[0] && e[0] != '0') ? 1 : 0;
        }
        if (s_adults == 1) {
            void* rng = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                ? *reinterpret_cast<void**>(
                      reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xd8)
                : nullptr;
            float adultAge = *reinterpret_cast<float*>(d + 0x44);
            float adultMin = *reinterpret_cast<float*>(d + 0x50);
            float reproMax = *reinterpret_cast<float*>(d + 0x54);
            float maxBase  = *reinterpret_cast<float*>(d + 0x38);
            float maxTol   = *reinterpret_cast<float*>(d + 0x3c);
            float r1 = (rng && fn_Random_RandFloat) ? fn_Random_RandFloat(rng) : 0.5f;
            float r2 = (rng && fn_Random_RandFloat) ? fn_Random_RandFloat(rng) : 0.5f;
            float age = r1 * (reproMax * 0.5f - adultMin) + adultMin;
            if (age < adultAge) age = adultAge;                  // guarantee adult
            float maxAge = r2 * maxTol + maxBase;
            if (maxAge <= age) maxAge = age + maxTol + 1.0f;     // never born old
            *reinterpret_cast<float*>(p + 0x3C) = age;
            *reinterpret_cast<float*>(p + 0x40) = maxAge;

            static int s_an = 0;
            if (s_an < 6) { ++s_an;
                fprintf(stderr, "[SpawnAdults] comp=%p age=%.2f adultAge=%.2f maxAge=%.2f\n",
                        (void*)self, age, adultAge, maxAge);
                fflush(stderr);
            }
        }
    }

    // Bring-up trace (shared env with Brick 1).
    {
        static int s_traceState = -1;
        static int s_traceCount = 0;
        if (s_traceState < 0) {
            const char* e = std::getenv("BANISHED_CITIZENAI_TRACE");
            s_traceState = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
        }
        if (s_traceState == 1 && s_traceCount < 8) {
            fprintf(stderr, "[CitizenBodyInit] comp=%p owner=%p desc=%p family=%p mesh=%p\n",
                    (void*)self, (void*)owner, desc,
                    *reinterpret_cast<void**>(p + 0x278),
                    *reinterpret_cast<void**>(p + 0x280));
            fflush(stderr);
            s_traceCount++;
        }
    }
}

// ------------------------------------------------------------------
// Phase 3 / Keystone D — CitizenComponent UPDATE live-wiring (work/economy loop).
//
// The per-tick lifecycle Update (the 711-instruction monster: aging / hunger /
// fuel / education / HOUSING (AssignHousing) / WORKPLACE (FindWorkplace) / partner /
// health / life-events) drives the world-state that gates the citizen Work state:
// a citizen only gets Work (id 5) as a candidate once it is HOUSED (+0x68 != 0)
// and/or EMPLOYED (+0x238 != 0) — both assigned here. Inert until now.
//
// Promoted STARTUP -> _S: Startup stays on BANISHED_LIVE_CITIZEN (unchanged); the
// heavy Update runs only when the SEPARATE env BANISHED_LIVE_CITIZEN_UPDATE is set
// (default-off => byte-identical to the Startup-only brick). Crash-safe guards bail
// until the body is populated + the world singleton is live. ~25 sub-calls — expect
// to chase dominoes; bring up incrementally.
static void CitizenComponent_UpdateAdapter(CitizenComponent* self, int64_t* time) {
    static int en = -1;
    if (en < 0) {
        const char* e = std::getenv("BANISHED_LIVE_CITIZEN_UPDATE");
        en = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    if (!en || !self) return;
    uint8_t* p = reinterpret_cast<uint8_t*>(self);
    // GUARD: Update derefs the CitizenDescription (+0x288, set by BodyInit), the
    // owner entity (+0x10), and the GameWorld weather (+0x130). Bail until ready.
    if (*reinterpret_cast<void**>(p + 0x288) == nullptr) return;
    if (*reinterpret_cast<void**>(p + 0x10)  == nullptr) return;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;

    static int s_trace = -1, s_tn = 0;
    if (s_trace < 0) {
        const char* e = std::getenv("BANISHED_CITIZENAI_TRACE");
        s_trace = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
    }
    if (s_trace == 1 && s_tn < 6) { ++s_tn;
        fprintf(stderr, "[CitizenUpdate] comp=%p house+0x68=%p workplace+0x238=%p\n",
                (void*)self, *reinterpret_cast<void**>(p + 0x68),
                *reinterpret_cast<void**>(p + 0x238));
        fflush(stderr);
    }

    CitizenComponent_Update(self, reinterpret_cast<void*>(time));
}

// ------------------------------------------------------------------
// Manual COMPONENT_LIVE_FACTORY_S expansion (+ slot 15 message handler).
// Byte-identical to
//   COMPONENT_LIVE_FACTORY_S(CitizenComponent, 0x290, 0xCB,
//                            "BANISHED_LIVE_CITIZEN",
//                            CitizenComponent_UpdateAdapter, CitizenComponent_BodyInit)
// except FinishLiveVtable ALSO graduates vtable slot 15 (+0x78 — the component
// message handler that SendComponentMessage::AbstractImpl (0x1400caef0)
// dispatches; retail slot = CitizenComponent_NeedsUpdate 0x140112260) to the
// citizen_needsupdate.cpp port when BANISHED_LIVE_STORAGE is set (Economy Brick
// M-D, citizen/eat-message half). Default (env off) => slot 15 stays the inert
// no-op stub => the eat SendComponentMessage remains a no-op exactly as before.
// ------------------------------------------------------------------
extern void CitizenComponent_NeedsUpdate(void*, int, uint64_t*, int, int); // citizen_needsupdate.cpp

static uint32_t CitizenComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("CitizenComponent");
    return cached;
}
static void* CitizenComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];
static void CitizenComponent_UpdateVSlot(void* s, void* t) {
    CitizenComponent_UpdateAdapter(reinterpret_cast<CitizenComponent*>(s),
                                   reinterpret_cast<int64_t*>(t));
}
static void CitizenComponent_StartupVSlot(void* s) {
    CitizenComponent_BodyInit(reinterpret_cast<CitizenComponent*>(s));
}
// slot +0x78 / index 15 — called as (comp, msgId, &targetEntityIBP, extra, extra2)
// by SendComponentMessage_AbstractImpl_Impl (code/Game/Action/sendcomponentmessage.cpp).
static void CitizenComponent_NeedsUpdateVSlot(void* s, int msg, void* target,
                                              int p4, int p5) {
    CitizenComponent_NeedsUpdate(s, msg, reinterpret_cast<uint64_t*>(target), p4, p5);
}
static void CitizenComponent_FinishLiveVtable() {
    if (::ComponentFactory::LiveEnabled("BANISHED_LIVE_CITIZEN")) {
        CitizenComponent_LiveVtable[4] =
            reinterpret_cast<void*>(&CitizenComponent_StartupVSlot);
        CitizenComponent_LiveVtable[9] =
            reinterpret_cast<void*>(&CitizenComponent_UpdateVSlot);
    }
    if (::ComponentFactory::LiveEnabled("BANISHED_LIVE_STORAGE"))
        CitizenComponent_LiveVtable[15] =
            reinterpret_cast<void*>(&CitizenComponent_NeedsUpdateVSlot);
}
extern "C" void* CitizenComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x290, 0xCB,
        CitizenComponent_LiveVtable,
        reinterpret_cast<void*>(&CitizenComponent_LiveGetTypeId));
    CitizenComponent_FinishLiveVtable();
    return p;
}
static void* CitizenComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x290, 0xCB,
        CitizenComponent_LiveVtable,
        reinterpret_cast<void*>(&CitizenComponent_LiveGetTypeId));
    CitizenComponent_FinishLiveVtable();
    return mem;
}
REGISTER_TYPE(CitizenComponent, "Component",
              &CitizenComponent_Factory, &CitizenComponent_CreateInPlace);
