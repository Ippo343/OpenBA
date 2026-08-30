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
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static uint32_t g_tid_AutoSaveDialog = 0;   // storage for 0x1401c4d08

extern "C" void FillTypeIdGlobals_autosave() {
    g_tid_AutoSaveDialog = ::Object::GetTypeIdByName("AutoSaveDialog");
    if (std::getenv("BANISHED_AUTOSAVE_TRACE")) {
        fprintf(stderr, "[AutoSave] typeid slot c4d08 reg=%p val=%u\n",
                (void*)ResolveAddr(Module::Exe, 0x1401c4d08), g_tid_AutoSaveDialog);
        fflush(stderr);
    }
}

void RegisterAutoSaveTypeIds() {
    RegisterExeAddr(0x1401c4d08, reinterpret_cast<uintptr_t>(&g_tid_AutoSaveDialog));
}
