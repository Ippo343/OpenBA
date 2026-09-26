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

// ============================================================
// Address constants for all Panel Controllers
// ============================================================

namespace AttendanceUIAddr {
    constexpr uintptr_t Factory = 0x1400104b0;
    constexpr uintptr_t Vtable  = 0x14015b070;
}
namespace BuildRequirementUIAddr {
    constexpr uintptr_t Factory = 0x1400107f0;
    constexpr uintptr_t Vtable  = 0x14015b1d0;
}
namespace BuildUIAddr {
    constexpr uintptr_t Factory = 0x1400209e0;
    constexpr uintptr_t Vtable  = 0x14015f210;
}
namespace CemeteryUIAddr {
    constexpr uintptr_t Factory = 0x140010900;
    constexpr uintptr_t Vtable  = 0x14015b240;
}
namespace CitizenUIAddr {
    constexpr uintptr_t Factory = 0x140010f80;
    constexpr uintptr_t Ctor    = 0x140065a40;
    constexpr uintptr_t Vtable  = 0x14015ba00;
}
namespace ConsumeProduceUIAddr {
    constexpr uintptr_t Factory = 0x140010ae0;
    constexpr uintptr_t Vtable  = 0x14015b2e0;
}
namespace ControlUIAddr {
    constexpr uintptr_t Factory = 0x140010c00;
    constexpr uintptr_t Vtable  = 0x14015b380;
}
namespace CropUIAddr {
    constexpr uintptr_t Factory = 0x140020b10;
    constexpr uintptr_t Ctor    = 0x1400b2c90;
    constexpr uintptr_t Vtable  = 0x14015d330; // SelectResourceUI base vtable
}
namespace DebugUIAddr {
    constexpr uintptr_t Factory = 0x140010ec0;
    constexpr uintptr_t Ctor    = 0x140064630;
    constexpr uintptr_t Vtable  = 0x14015b870;
}
namespace DestroyUIAddr {
    constexpr uintptr_t Factory = 0x140011130;
    constexpr uintptr_t Vtable  = 0x14015bb00;
}
namespace EntityUIAddr {
    constexpr uintptr_t Factory = 0x140011270;
    constexpr uintptr_t Vtable  = 0x14015bb90;
}
namespace EventLogUIAddr {
    constexpr uintptr_t Factory = 0x1400113e0;
    constexpr uintptr_t Vtable  = 0x14015bc40;
}
namespace HelpUIAddr {
    constexpr uintptr_t Factory = 0x140011ca0;
    constexpr uintptr_t Ctor    = 0x14006a920;
    constexpr uintptr_t Vtable  = 0x14015bf30;
}
namespace HotKeyToolTipUIAddr {
    constexpr uintptr_t Factory = 0x140011e20;
    constexpr uintptr_t Vtable  = 0x14015bfc0;
}
namespace ImmigrationUIAddr {
    constexpr uintptr_t Factory = 0x140011ee0;
    constexpr uintptr_t Vtable  = 0x14015c080;
}
namespace MapUIAddr {
    constexpr uintptr_t Factory = 0x140012240;
    constexpr uintptr_t Vtable  = 0x14015c350;
}
namespace ModBrowseUIAddr {
    constexpr uintptr_t Factory = 0x1400126d0;
    constexpr uintptr_t Vtable  = 0x14015c500;
}
namespace NoiseUIAddr {
    constexpr uintptr_t Factory = 0x140012d30;
    constexpr uintptr_t Vtable  = 0x14015ca80;
}
namespace OrchardUIAddr {
    constexpr uintptr_t Factory = 0x140012ff0;
    constexpr uintptr_t Vtable  = 0x14015cf30; // final vtable (base SelectResourceUI = 0x14015d330)
}
namespace PageToggleUIAddr {
    constexpr uintptr_t Factory = 0x140013160;
    constexpr uintptr_t Vtable  = 0x14015cfb0;
}
namespace PastureUIAddr {
    constexpr uintptr_t Factory = 0x140020e10;
    constexpr uintptr_t Vtable  = 0x14015f610; // final vtable (base SelectResourceUI = 0x14015d330)
}
namespace ProductionUIAddr {
    constexpr uintptr_t Factory = 0x140013230;
    constexpr uintptr_t Vtable  = 0x14015d070;
}
namespace ProfessionUIAddr {
    constexpr uintptr_t Factory = 0x140013400;
    constexpr uintptr_t Ctor    = 0x1400775a0;
    constexpr uintptr_t Vtable  = 0x14015d0f0;
}
namespace RadiusUIAddr {
    constexpr uintptr_t Factory = 0x140013590;
    constexpr uintptr_t Vtable  = 0x14015d160;
}
namespace ResidenceUIAddr {
    constexpr uintptr_t Factory = 0x140020f60;
    constexpr uintptr_t Vtable  = 0x14015f6d0;
}
namespace ResourceLimitUIAddr {
    constexpr uintptr_t Factory = 0x1400136b0;
    constexpr uintptr_t Vtable  = 0x14015d1f0;
}
namespace SpriteCaptureUIAddr {
    constexpr uintptr_t Factory = 0x140013d10;
    constexpr uintptr_t Vtable  = 0x14015d4f0;
}
namespace StatusBarUIAddr {
    constexpr uintptr_t Factory = 0x1400140e0;
    constexpr uintptr_t Ctor    = 0x14007af20;
    constexpr uintptr_t Vtable  = 0x14015d810;
}
namespace StatusIconUIAddr {
    constexpr uintptr_t Factory = 0x140014180;
    constexpr uintptr_t Vtable  = 0x14015d8a0;
}
namespace StorageCapacityUIAddr {
    constexpr uintptr_t Factory = 0x1400143f0;
    constexpr uintptr_t Vtable  = 0x14015d950;
}
namespace StorageUIAddr {
    constexpr uintptr_t Factory = 0x140021330;
    constexpr uintptr_t Vtable  = 0x14015f970;
}
namespace TitleDescToolTipUIAddr {
    constexpr uintptr_t Factory = 0x1400144c0;
    constexpr uintptr_t Vtable  = 0x14015d9f0;
}
namespace ToolUIAddr {
    constexpr uintptr_t Factory = 0x140014580;
    constexpr uintptr_t Vtable  = 0x14015da70;
}
namespace TownHallUIAddr {
    constexpr uintptr_t Factory = 0x140014660;
    constexpr uintptr_t Ctor    = 0x140081280;
    constexpr uintptr_t Vtable  = 0x14015dce0;
}
namespace TradeUIAddr {
    constexpr uintptr_t Factory = 0x140015f80;
    constexpr uintptr_t Ctor    = 0x140086b80;
    constexpr uintptr_t Vtable  = 0x14015de80;
}
namespace TutorialUIAddr {
    constexpr uintptr_t Factory = 0x140029400;
    constexpr uintptr_t Vtable  = 0x140160f30;
}
namespace UpgradeUIAddr {
    constexpr uintptr_t Factory = 0x140016b30;
    constexpr uintptr_t Vtable  = 0x14015df00;
}
namespace WorkPlaceUIAddr {
    constexpr uintptr_t Factory = 0x140016dc0;
    constexpr uintptr_t Vtable  = 0x14015e030;
}

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
