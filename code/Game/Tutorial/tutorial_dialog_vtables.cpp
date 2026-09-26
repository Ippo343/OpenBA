// code/Game/Tutorial/tutorial_dialog_vtables.cpp
//
// The factory registrations of the four tutorial dialog / controller classes,
// in the order the old REGISTER_TYPE lines had (ENDGAME B.3.8 B8-7: the
// registration stays where the REGISTER_TYPE was).
//
// ENDGAME B.3.8 block GOAL: TutorialUI, TutorialGoalDialog, TutorialSetupDialog
// and TutorialHighlightDialog are real C++ classes with compiler vtables
// (Tutorial/<Class>.h, bodies in their own .cpp). What this file used to hold
// is gone: the four hand-built 48-slot tables, their alloc + zero + stamp
// factories (which skipped the base ctors, B38_RULES D9 #6), the guarded
// ~Internal "dtor", the hand-rolled TutorialHighlightDialog / TutorialSetupDialog
// ctors, and the RegisterExeAddr vtable storage in reg_tutorial_wave.cpp.

#include "Tutorial/TutorialUI.h"
#include "Tutorial/TutorialGoalDialog.h"
#include "Tutorial/TutorialSetupDialog.h"
#include "Tutorial/TutorialHighlightDialog.h"
#include "Object/factory_registration.h"

DEFINE_EXE_TYPE(TutorialUI, "UI::DialogController")
DEFINE_EXE_TYPE(TutorialGoalDialog, "TutorialGoal")
DEFINE_EXE_TYPE(TutorialSetupDialog, "TutorialSetup")
DEFINE_EXE_TYPE(TutorialHighlightDialog, "UI::DialogInstance")
