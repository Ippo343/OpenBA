// standalone/reg_autosave.cpp — AutoSaveDialog type-id slot (LANE1 rows 13/14).
//
// OptionsDialog::ApplySettings (optionsdialog.cpp:936) walks the open dialogs looking
// for one whose type is AutoSaveDialog and calls AutoSaveDialog_Slot4 on it to restart
// the autosave timer with the newly-chosen interval. That loop is guarded on
// `g_TypeId_AutoSaveDialogType`, which reads the exe .rdata type-id slot 0x1401c4d08.
// The slot was never registered, so the pointer was null, the guard failed, and
// changing the Options autosave interval had no effect on a running game.
//
// Registering storage here makes the slot readable; FillTypeIdGlobals_autosave() fills
// it with the real CRC AFTER Object::Factory::Startup has assigned ids (filling at
// registration time would store 0xFFFFFFFF — see reg_citizenai_typeids.cpp for the
// same staging).
//
// The name must match the REGISTER_TYPE in code/Game/Dialog/autosavedialog.cpp, so
// that the id here and the id our standalone vtable's GetTypeId returns are the same
// value — otherwise fn_IsA would reject our own dialog.

#include "address_registry.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include "Dialog/ui_dialogs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static uint32_t g_tid_AutoSaveDialog = 0;   // storage for 0x1401c4d08

// Storage for 0x1401c5b08 = TypeInfo_Scenario's type id (LANE1 row 42 follow-up).
// ScenarioDialog::Slot4 filters the ScenarioList with
//   IsDerivedFrom(resource, *g_TypeId_Scenario)
// and our port bails out of the loop entirely when that pointer is null. The slot was
// never registered, so opening the Tutorial/Scenario dialog listed scenarioCount=4 but
// matched 0 of them and rendered an empty table. The type itself has been registered
// all along (REGISTER_TYPE(Scenario, "::Achievement::Achievement") in
// code/Game/Game/scenario.cpp) - only this .rdata slot was missing.
static uint32_t g_tid_Scenario = 0;         // storage for 0x1401c5b08

extern "C" void FillTypeIdGlobals_autosave() {
    g_tid_AutoSaveDialog = ::Object::GetTypeIdByName("AutoSaveDialog");
    g_tid_Scenario       = ::Object::GetTypeIdByName("Scenario");
}

void RegisterAutoSaveTypeIds() {
    RegisterExeAddr(0x1401c4d08, reinterpret_cast<uintptr_t>(&g_tid_AutoSaveDialog));
    RegisterExeAddr(0x1401c5b08, reinterpret_cast<uintptr_t>(&g_tid_Scenario));

    // AutoSaveDialog::RestartAutoSaveTimer. 0x14005e120 is the body and 0x14005e110 the
    // 5-byte JMP thunk in front of it (vtable slot 4); both are ported as
    // AutoSaveDialog_Slot4, and NEITHER was registered.
    //
    // That is LANE1 row 13's real cause. OptionsDialog::ApplySettings guards its whole
    // "walk the open dialogs and restart the autosave timer" loop on
    // fn_UpdateAutoSaveTimer, which engine_funcs.cpp resolves from 0x14005e120. With the
    // address unregistered that pointer was null, the guard failed, and changing the
    // autosave interval in Options never re-armed the timer - the running game kept
    // whatever interval it booted with.
    RegisterExeAddr(0x14005e120,
                    reinterpret_cast<uintptr_t>(
                        static_cast<void (*)(AutoSaveDialog*)>(&AutoSaveDialog_Slot4)));
    RegisterExeAddr(0x14005e110,
                    reinterpret_cast<uintptr_t>(
                        static_cast<void (*)(AutoSaveDialog*)>(&AutoSaveDialog_Slot4)));
}
