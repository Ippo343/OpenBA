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

// --- ModBrowseUI (0x30) ---
// No subclass fields.
struct ModBrowseUI {
    DialogController base;     // +0x00
};
static_assert(sizeof(ModBrowseUI) == 0x30, "ModBrowseUI size mismatch");

// --- NoiseUI (0x70) ---
// Factory inits: +0x38..+0x68=0 (7 qwords, note: +0x30 NOT initialized)
struct NoiseUI {
    DialogController base;     // +0x00
    uint8_t  _pad30[8];        // +0x30: gap (not initialized by factory)
    uint64_t data38;           // +0x38: zeroed
    uint64_t data40;           // +0x40
    uint64_t data48;           // +0x48
    uint64_t data50;           // +0x50
    uint64_t data58;           // +0x58
    uint64_t data60;           // +0x60
    uint64_t data68;           // +0x68
};
static_assert(sizeof(NoiseUI) == 0x70, "NoiseUI size mismatch");

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

// --- SpriteCaptureUI (0x70) ---
// Serialize inits:
//   editCapture@+0x30, buttonCapture@+0x38, captureBox@+0x40,
//   curX@+0x48=0, curY@+0x4C=0, anchorX@+0x50=100, anchorY@+0x54=100,
//   dragging@+0x58=0, pendingExternal@+0x60=0.
// OnCreate: tracks mouse drag; on left-click+shift starts capture region;
//   when pendingExternal@+0x60 != 0 and saveFrameCount@+0x68 == 1,
//   calls EndSpriteCapture, builds save Filename, calls External::SaveText.
struct SpriteCaptureUI {
    DialogController base;     // +0x00 (0x30)
    void*    editCapture;      // +0x30: EditElement* (InternalBasePtr)
    void*    buttonCapture;    // +0x38: Element* (InternalBasePtr)
    void*    captureBox;       // +0x40: Element* (InternalBasePtr)
    int32_t  curX;             // +0x48: current drag mouse X
    int32_t  curY;             // +0x4C: current drag mouse Y
    int32_t  anchorX;          // +0x50: drag anchor X (init 100)
    int32_t  anchorY;          // +0x54: drag anchor Y (init 100)
    uint8_t  dragging;         // +0x58: 1 = left mouse button held, dragging
    uint8_t  _pad59[7];        // +0x59: pad to +0x60
    void*    pendingExternal;  // +0x60: External* awaiting save (set externally)
    int32_t  saveFrameCount;   // +0x68: incremented each frame; triggers save at ==1
    uint8_t  _pad6C[4];        // +0x6C: pad to 0x70
};
static_assert(sizeof(SpriteCaptureUI) == 0x70, "SpriteCaptureUI size mismatch");
static_assert(offsetof(SpriteCaptureUI, editCapture) == 0x30, "");
static_assert(offsetof(SpriteCaptureUI, curX) == 0x48, "");
static_assert(offsetof(SpriteCaptureUI, anchorX) == 0x50, "");
static_assert(offsetof(SpriteCaptureUI, dragging) == 0x58, "");
static_assert(offsetof(SpriteCaptureUI, pendingExternal) == 0x60, "");
static_assert(offsetof(SpriteCaptureUI, saveFrameCount) == 0x68, "");

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

// --- TutorialUI (0x60) ---
// Factory inits: +0x30..+0x58=0 (6 qwords)
struct TutorialUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint64_t data38;           // +0x38
    uint64_t data40;           // +0x40
    uint64_t data48;           // +0x48
    uint64_t data50;           // +0x50
    uint64_t data58;           // +0x58
};
static_assert(sizeof(TutorialUI) == 0x60, "TutorialUI size mismatch");

// --- UpgradeUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/UpgradeUI.h"

// --- WorkPlaceUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/WorkPlaceUI.h"
