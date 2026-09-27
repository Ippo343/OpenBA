// standalone/reg_tutorial_typeid.cpp
// Storage + fill for the tutorial .rdata type-id globals.
//
//   0x1401c6240  TypeInfo_FinishTutorialGoal      (writer FinishTutorialGoal_RegisterType
//                0x14014d550, factory_registrations.csv:168)
//   0x1401c6668  TypeInfo_TutorialInstance        (writer 0x14014da10, csv:187)
//
// All three existed only as CSV rows — ResolveAddr(Exe, ...) returned 0 for each, so
// engine_funcs' g_TypeId_FinishTutorialGoal was a NULL POINTER, not a wrong value.
//
// Why it mattered: FinishTutorialInstance::Slot4 (exe 0x1400d7a90) narrows its goal
// pointer with `IsDerivedFrom(self+0x18, TypeInfo_FinishTutorialGoal)` and only copies
// the Tutorial definition into TutorialInstance+0x30 when that passes. With the id
// unavailable the narrow failed, +0x30 stayed NULL, and the step driver
// (0x1400e1db0) hit its first silent early-return on every single tick — the tutorial
// world loaded and played as a sandbox with no popup, no goals and no steps. Measured
// as `[FinTutInst] Slot4 INIT ... ftGoal=0 tutDef=0 (goalTid=0)`.
//
// ⚠ Registering the slot is only half the job — guard the VALUE, not the pointer
// (reference_registered_but_filled_with_zero), which is why the fill reports both arms.
//
// Two-phase, matching reg_market_typeid.cpp exactly:
//   RegisterTutorialTypeIds()     — from AddressRegistryInit, BEFORE EngineFuncsInit,
//                                   so engine_funcs globals point at our storage.
//   FillTypeIdGlobals_tutorial()  — from game_main.cpp AFTER Object::Factory::Startup,
//                                   the only point at which the real CRC ids exist.
//                                   Filling earlier stores the 0xFFFFFFFF sentinel.
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>

static uint32_t g_tid_FinishTutorialGoal     = 0;  // 0x1401c6240
static uint32_t g_tid_TutorialInstance       = 0;  // 0x1401c6668

// ── Profession (0x1401c5a98) — was registered NOWHERE ────────────────────────
// Written by Profession_RegisterType (0x14014ccb5) in retail, and read four times
// by TutorialGoalCount::Check (0x1400df39e/3fd/54b/590), where it is the type
// FILTER on the ExternalPtr->type resolve that every profession-flavoured count
// mode runs before it compares anything. Our ResolveTypePtr returns 0 when the id
// storage is unregistered, so the target profession came out NULL, no citizen's
// +0x1f0 could match it, and the count was a hard 0 — with the panel, the spinner
// and the citizen conversion all working perfectly.
//
// Measured on stepBuilders2 ("assign 2 builders"): two spinner clicks converted
// two citizens ([ProfUI] HandleSpinner desired=2 current=1, found != 0) while
// [GoalCount] mode=1 count=0 never moved once.
//
// Not tutorial-only — ProfessionList::Slot12 (0x14001e510) and
// FinishTutorialInstance_helper_9cd0 (0x1400d9cf7) read the same slot. Across the
// shipped scenario files 23 goals go through this filter (the Profession,
// Workplace, WorkplaceCleared and AvailableJobs count modes); every one of them
// was answering 0. The Zone-mode goals worked only because their sibling filter
// id, ComponentDescription (0x1401c7318), IS registered
// (reg_tradeui_typeids.cpp).
static uint32_t g_tid_Profession            = 0;  // 0x1401c5a98

extern "C" void FillTypeIdGlobals_tutorial() {
    struct { const char* name; uint32_t* slot; uintptr_t addr; } kIds[] = {
        { "FinishTutorialGoal",     &g_tid_FinishTutorialGoal,     0x1401c6240 },
        { "TutorialInstance",       &g_tid_TutorialInstance,       0x1401c6668 },
        { "Profession",             &g_tid_Profession,             0x1401c5a98 },
    };
    for (auto& e : kIds) {
        *e.slot = ::Object::GetTypeIdByName(e.name);
        if (*e.slot == 0)
            std::fprintf(stderr, "[TutorialTypeId] ERROR: GetTypeIdByName(\"%s\") returned 0 "
                                 "- 0x%llX stays a silent permanent miss\n",
                         e.name, (unsigned long long)e.addr);
        else
            std::fprintf(stderr, "[TutorialTypeId] 0x%llX %s = %u (0x%08X)\n",
                         (unsigned long long)e.addr, e.name, *e.slot, *e.slot);
    }
    std::fflush(stderr);
}

void RegisterTutorialTypeIds() {
    RegisterExeAddr(0x1401c6240, reinterpret_cast<uintptr_t>(&g_tid_FinishTutorialGoal));
    RegisterExeAddr(0x1401c6668, reinterpret_cast<uintptr_t>(&g_tid_TutorialInstance));
    RegisterExeAddr(0x1401c5a98, reinterpret_cast<uintptr_t>(&g_tid_Profession));
}
