// standalone/reg_dialog.cpp
// Dialog-module registration wave — closes the Dialog code-gap (2 addrs).
//
//   0x14000d6c0  WorkplaceUI FindComponent (type 0x1401c7008) — guard check.
//                Member of the identical FindComponent family; reimpl in
//                code/Game/Dialog/dialog_exe_helpers.cpp. Closure: engine_funcs
//                only (CheckPtr / IsDerivedFrom / IBP_Ctor); type-id global
//                0x1401c7008 is out-of-gap-range data (latent-null if unregistered,
//                returning a conservative no-match — never corrupts).
//   0x14005dec0  AutoSaveDialog autosave handler. Closure = Runtime exports
//                (SaveLoad::*, UI::UserInterface::PausesGame, System::Time/String,
//                UIString) + engine_funcs + singletons — all ResolveExport-resolved,
//                no further exe-internal sub-callees needing registration (the only
//                one, FUN_140099d40, is ported inline as Dialog_AutoSaveBuildName).
//
// SAFETY: both addresses verified as true function ENTRYs in Ghidra.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include "Dialog/dialog_exe_helpers.h"   // exact signatures for the reimpl bodies
#include <cstdint>

void RegisterDialogBatch() {
    RegisterExeAddr(0x14000d6c0, reinterpret_cast<uintptr_t>(&Dialog_FindComponent_d6c0));
    RegisterExeAddr(0x14005dec0, reinterpret_cast<uintptr_t>(&Dialog_AutoSaveHandler));
}

#endif // STANDALONE_EXE
