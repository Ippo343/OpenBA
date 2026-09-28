#pragma once
// Game UI Dialog Controllers -- binary-compatible struct layouts.
// Application-x64.exe, build 170910.
//
// All 22 Dialog Controller classes that extend UI::DialogInstance (base 0xB8).
// Decompiled from factory functions and constructors.
//
// Pattern: UI::DialogInstance::DialogInstance(this) -> set vtable -> init subclass fields
// DialogInstance base occupies +0x00..+0xB7 (0xB8 bytes).
// Subclass-specific fields start at +0xB8.

#include <cstdint>
#include <cstddef>
#include "DialogInstance.h"

// (B3R W2-EZ: the 22 `XDialogAddr` constant namespaces that stood here -- 54
// Factory / Ctor / Vtable addresses -- had no reader and are gone.)

// ============================================================
// Struct definitions
// ============================================================

// --- AchievementDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/AchievementDialog.h"

// --- AutoSaveDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/AutoSaveDialog.h"

// --- EventNotifyDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/EventNotifyDialog.h"

// --- EventPopupDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/EventPopupDialog.h"

// --- FollowDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/FollowDialog.h"

// --- GoalDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/GoalDialog.h"

// --- LoaderDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/LoaderDialog.h"

// --- MainMenuDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/MainMenuDialog.h"

// --- MapParamsDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/MapParamsDialog.h"

// --- MessageDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/MessageDialog.h"

// --- ModDetailsDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/ModDetailsDialog.h"

// --- ModDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/ModDialog.h"

// --- ModUploadDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/ModUploadDialog.h"

// --- NewGameDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/NewGameDialog.h"

// --- OptionsDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/OptionsDialog.h"

// --- OptionInputBindDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/OptionInputBindDialog.h"

// --- SaveLoadDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/SaveLoadDialog.h"

// --- ScenarioDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/ScenarioDialog.h"

// --- ToolbarDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/ToolbarDialog.h"

// --- TutorialGoalDialog --- ENDGAME B.3.8 GOAL: a real class now (B8-4 forwarder)
#include "Tutorial/TutorialGoalDialog.h"

// --- TutorialHighlightDialog --- ENDGAME B.3.8 GOAL: a real class now (B8-4 forwarder)
#include "Tutorial/TutorialHighlightDialog.h"

// --- TutorialSetupDialog --- ENDGAME B.3.8 GOAL: a real class now (B8-4 forwarder)
#include "Tutorial/TutorialSetupDialog.h"
