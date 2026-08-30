// standalone/reg_nullsweep.cpp
//
// Registrations for the "null function-pointer sweep" follow-up (three addresses
// flagged as the tutorial entry-point blocker): ScenarioDialog_CopyCtor's vtable.
//
// The ctor itself (0x1400786b0) is wired directly to ScenarioDialog_Ctor via
// fn_ScenarioDialog_Ctor in engine_funcs.cpp — no raw-address resolve needed there.
// But the ctor's disasm hardcodes a store of `&vtable_ScenarioDialog` (0x14015d2d0),
// and that .rdata address was never registered anywhere. Registering it here means
// any future code that resolves it directly (ResolveAddr(Module::Exe, 0x14015d2d0))
// also gets the correct standalone vtable, same shape as RegisterMenuDialogVtables()
// in menu_dialog_standalone_vtables.cpp.

#include "resolve.h"
#include <cstdint>

// code/Game/Dialog/scenariodialog.cpp
void* ScenarioDialog_GetStandaloneVtable();

extern "C" void RegisterNullSweepBatch() {
    RegisterExeAddr(0x14015d2d0,
                    reinterpret_cast<uintptr_t>(ScenarioDialog_GetStandaloneVtable()));
}
