// code/Game/Dialog/menu_dialog_standalone_vtables.cpp
//
// Standalone vtables for the MAIN-MENU DialogInstance subclasses whose slot bodies
// were fully ported and wired into nothing.
//
// The defect is not the one the ui_dialog_stub_factories.cpp header describes. These
// dialogs are never built through the by-name factory: MainMenuDialog builds them by
// hand -- MemAlloc, the real UI::DialogInstance ctor, then a single store of the exe
// .rdata vtable pointer over slot 0, exactly as retail does:
//
//     fn_DialogInstance_Ctor(dlgMem, ui, extRes, &parentBuf, 0, 0, 0);
//     *(void**)dlgMem = (void*)g_AchievementDialog_Vtable;      // 0x14015b020
//
// In standalone that .rdata address was registered NOWHERE, so ResolveAddr returns 0,
// g_AchievementDialog_Vtable is NULL, and the store installs a null vtable on a
// perfectly good dialog. The dialog is then handed to the UserInterface, which
// dispatches Setup on it -- a call through a null vtable. So the menu's Achievements
// and Goals buttons were not "inert", they were a guaranteed crash, and the ported
// Slot4/6/7/8 bodies in achievementdialog.cpp / goaldialog.cpp had no way to run.
//
// This file supplies the arrays and registers them at the retail .rdata addresses, so
// the existing MainMenuDialog code needs no change and every other caller of the same
// global is fixed at the same time. Same shape as messagedialog.cpp and the
// FollowDialog block in followdialog.cpp.
//
// Slot layout read straight out of the exe .rdata (tools/dump_vtable.py), not guessed:
//
//   AchievementDialog @ 0x14015b020 (10 slots; the entries at index 10+ belong to the
//                                    NEXT table in .rdata -- AttendanceUI -- see below)
//     +0x00 idx0 ScalarDtor   0x1400103f0     +0x28 idx5 Shutdown   0x14003a94e (base)
//     +0x08 idx1 getter       0x140010310     +0x30 idx6 OnUpdate   0x14005d240
//     +0x10 idx2 GetTypeId    0x140010300     +0x38 idx7 OnLayout   0x14005d480
//     +0x18 idx3 Persist      0x14003a948     +0x40 idx8 OnCommand  0x14005d440
//     +0x20 idx4 Setup        0x14005d490     +0x48 idx9 OnSerial   0x14003a954 (base)
//
//   GoalDialog @ 0x14015beb0 (10 slots)
//     +0x20 idx4 Setup 0x140069810   +0x30 idx6 0x140069640
//     +0x38 idx7 0x140069800         +0x40 idx8 OnCommand 0x1400697c0
//
// WARNING recorded so it is not "fixed" back: AchievementDialog_Slot14 and
// AchievementDialog_Slot17 in achievementdialog.cpp are MISATTRIBUTED. Their retail
// addresses (0x14005dd40 / 0x14005dc50) are the entries at .rdata index 14 and 17
// counting from 0x14015b020 -- but AchievementDialog's own table ends at index 9, and
// those two are slots 4 and 7 of the table that STARTS at 0x14015b070, which is
// AttendanceUI's (its name string sits immediately after it in .rdata). They are the
// same two retail functions already ported, correctly named, as AttendanceUI_Slot4 /
// AttendanceUI_Slot7 in attendanceui.cpp, and it is the AttendanceUI copies that are
// wired. Their bodies read +0x20/+0x28/+0x30 and an AttendanceComponent, which is
// DialogController layout, not DialogInstance layout -- calling them on an
// AchievementDialog would read the wrong fields entirely. So they are deliberately NOT
// installed here. (One retail function ported twice under two names is a known trap in
// this codebase; grep the ADDRESS, not the name.)

// ENDGAME B.3.8 block DLG: both classes are real now (Dialog/AchievementDialog.h,
// Dialog/GoalDialog.h); the two hand-built tables and their wrappers are gone. The
// menu openers construct the real classes. engine_funcs.cpp still resolves both
// retail addresses into g_AchievementDialog_Vtable / g_GoalDialog_Vtable, so the
// addresses keep serving the COMPILER tables (B38 D5).

#include "Dialog/AchievementDialog.h"
#include "Dialog/GoalDialog.h"
#include "resolve.h"
#include "address_registry.h"
#include <cstdint>

extern "C" void RegisterMenuDialogVtables() {
    RegisterExeAddr(0x14015b020,
                    reinterpret_cast<uintptr_t>(AchievementDialog::Vtable()));
    RegisterExeAddr(0x14015beb0,
                    reinterpret_cast<uintptr_t>(GoalDialog::Vtable()));
}
