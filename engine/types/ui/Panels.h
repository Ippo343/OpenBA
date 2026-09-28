#pragma once
// Game UI Panel Controllers -- binary-compatible struct layouts.
// Application-x64.exe, build 170910.
//
// All 39 Panel Controller classes that extend UI::DialogController (base 0x30).
// Decompiled from factory functions and constructors.
//
// Pattern: UI::DialogController::DialogController(this) -> set vtable -> init subclass fields
// DialogController base occupies +0x00..+0x2F (0x30 bytes).
// Subclass-specific fields start at +0x30.

#include <cstdint>
#include <cstddef>
#include "DialogController.h"

// (B3R W2-EZ: the 38 `XUIAddr` constant namespaces that stood here -- 84
// Factory / Ctor / Vtable addresses -- had no reader and are gone.)

// ============================================================
// Struct definitions
// ============================================================

// --- AttendanceUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/AttendanceUI.h"

// --- BuildRequirementUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/BuildRequirementUI.h"

// --- BuildUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/BuildUI.h"

// --- CemeteryUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/CemeteryUI.h"

// --- CitizenUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/CitizenUI.h"

// --- ConsumeProduceUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/ConsumeProduceUI.h"

// --- ControlUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/ControlUI.h"

// --- CropUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/CropUI.h"

// --- DebugUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/DebugUI.h"

// --- DestroyUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/DestroyUI.h"

// --- EntityUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/EntityUI.h"

// --- EventLogUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/EventLogUI.h"

// --- HelpUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/HelpUI.h"

// --- HotKeyToolTipUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/HotKeyToolTipUI.h"

// --- ImmigrationUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/ImmigrationUI.h"

// --- MapUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/MapUI.h"

// --- ModBrowseUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/ModBrowseUI.h"

// --- NoiseUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/NoiseUI.h"

// --- OrchardUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/OrchardUI.h"

// --- PageToggleUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/PageToggleUI.h"

// --- PastureUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/PastureUI.h"

// --- ProductionUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/ProductionUI.h"

// --- ProfessionUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/ProfessionUI.h"

// --- RadiusUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/RadiusUI.h"

// --- ResidenceUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/ResidenceUI.h"

// --- ResourceLimitUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/ResourceLimitUI.h"

// --- SelectResourceUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/SelectResourceUI.h"

// --- SpriteCaptureUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/SpriteCaptureUI.h"

// --- StatusBarUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/StatusBarUI.h"

// --- StatusIconUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/StatusIconUI.h"

// --- StorageCapacityUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/StorageCapacityUI.h"

// --- StorageUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/StorageUI.h"

// --- TitleDescToolTipUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/TitleDescToolTipUI.h"

// --- ToolUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/ToolUI.h"

// --- TownHallUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/TownHallUI.h"

// --- TradeUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/TradeUI.h"

// --- TutorialUI --- the class: ENDGAME B.3.8 GOAL (B8-4 forwarder)
#include "Tutorial/TutorialUI.h"

// --- UpgradeUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/UpgradeUI.h"

// --- WorkPlaceUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/WorkPlaceUI.h"
