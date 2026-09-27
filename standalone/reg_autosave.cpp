// standalone/reg_autosave.cpp — AutoSaveDialog registrations (LANE1 rows 13/14).
//
// OptionsDialog::ApplySettings (optionsdialog.cpp) walks the open dialogs looking for
// one whose type is AutoSaveDialog and calls AutoSaveDialog_Slot4 on it to restart the
// autosave timer with the newly-chosen interval. The type test reads
// AutoSaveDialog::TypeId() (ENDGAME B.3.9 TR: the type-id storage cells 0x1401c4d08 /
// 0x1401c5b08 this file used to register and fill are gone).

#include "address_registry.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include "Dialog/ui_dialogs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "Dialog/AutoSaveDialog.h"
#include "Game/Scenario.h"

void RegisterAutoSaveTypeIds() {

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
}
