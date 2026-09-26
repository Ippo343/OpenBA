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

// --- CitizenUI (0xE8) ---
// Has separate Ctor (0x140065a40). Inits +0x30..+0xD8=0, InternalBasePtr@+0xE0.
struct CitizenUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint64_t data38;           // +0x38
    uint64_t data40;           // +0x40
    uint64_t data48;           // +0x48
    uint64_t data50;           // +0x50
    uint64_t data58;           // +0x58
    uint64_t data60;           // +0x60
    uint64_t data68;           // +0x68
    uint64_t data70;           // +0x70
    uint64_t data78;           // +0x78
    uint64_t data80;           // +0x80
    uint64_t data88;           // +0x88
    uint64_t data90;           // +0x90
    uint64_t data98;           // +0x98
    uint64_t dataA0;           // +0xA0
    uint64_t dataA8;           // +0xA8
    uint64_t dataB0;           // +0xB0
    uint64_t dataB8;           // +0xB8
    uint64_t dataC0;           // +0xC0
    uint64_t dataC8;           // +0xC8
    uint64_t dataD0;           // +0xD0
    uint64_t dataD8;           // +0xD8
    uint64_t internalBasePtr;  // +0xE0: Object::InternalBasePtr (init to null)
};
static_assert(sizeof(CitizenUI) == 0xE8, "CitizenUI size mismatch");
static_assert(offsetof(CitizenUI, data30) == 0x30, "");
static_assert(offsetof(CitizenUI, internalBasePtr) == 0xE0, "");

// --- ConsumeProduceUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/ConsumeProduceUI.h"

// --- ControlUI (0x40) ---
struct ControlUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint64_t data38;           // +0x38: zeroed
};
static_assert(sizeof(ControlUI) == 0x40, "ControlUI size mismatch");

// --- CropUI (0x60) --- extends SelectResourceUI
// Has separate Ctor (0x1400b2c90). Base vtable 0x14015d330 (SelectResourceUI).
// Ctor: InternalBasePtr@+0x30, then +0x38..+0x58=0
struct CropUI {
    DialogController base;     // +0x00
    uint64_t internalBasePtr;  // +0x30: Object::InternalBasePtr (init to null)
    uint64_t data38;           // +0x38: zeroed
    uint64_t data40;           // +0x40: zeroed
    uint64_t data48;           // +0x48: zeroed
    uint64_t data50;           // +0x50: zeroed
    uint64_t data58;           // +0x58: zeroed
};
static_assert(sizeof(CropUI) == 0x60, "CropUI size mismatch");
static_assert(offsetof(CropUI, internalBasePtr) == 0x30, "");

// --- DebugUI (0x130) ---
// Has separate Ctor (0x140064630). Inits 30 InternalBasePtrs at +0x30..+0x118.
// Element names recovered from Slot4 decompilation (0x1400619E0).
// Slot4 assigns elements via FindElement; Slot7/Slot8 read slider/number values.
struct DebugUI {
    DialogController base;     // +0x00
    void* diseaseCombo;        // +0x30: "diseaseCombo" ComboElement
    void* drawCombo;           // +0x38: "drawCombo" ComboElement
    void* numberDrawCalls;     // +0x40: "numberDrawCalls" NumberElement
    void* numberTriangles;     // +0x48: "numberTriangles" NumberElement
    void* numberVertices;      // +0x50: "numberVertices" NumberElement
    void* numberBatches;       // +0x58: "numberBatches" NumberElement
    void* numberFPS;           // +0x60: "numberFPS" NumberElement (smoothed FPS, int)
    void* numberFrameMS;       // +0x68: "numberFrameMS" NumberElement (smoothed frame ms, float)
    void* numberTotalMS;       // +0x70: "numberTotalMS" NumberElement (smoothed total ms, float)
    void* sliderDay;           // +0x78: "sliderDay" SliderElement (day of year)
    void* valueSunR;           // +0x80: "valueSunR" SliderElement
    void* valueSunG;           // +0x88: "valueSunG" SliderElement
    void* valueSunB;           // +0x90: "valueSunB" SliderElement
    void* valueAmbR;           // +0x98: "valueAmbR" SliderElement
    void* valueAmbG;           // +0xA0: "valueAmbG" SliderElement
    void* valueAmbB;           // +0xA8: "valueAmbB" SliderElement
    void* valueFogR;           // +0xB0: "valueFogR" SliderElement
    void* valueFogG;           // +0xB8: "valueFogG" SliderElement
    void* valueFogB;           // +0xC0: "valueFogB" SliderElement
    void* valueFogN;           // +0xC8: "valueFogN" SliderElement (fog near)
    void* valueFogF;           // +0xD0: "valueFogF" SliderElement (fog far)
    void* valueFogM;           // +0xD8: "valueFogM" SliderElement (fog max density)
    void* valueSunH;           // +0xE0: "valueSunH" SliderElement (sun heading)
    void* valueSunP;           // +0xE8: "valueSunP" SliderElement (sun pitch)
    void* imageColorSun;       // +0xF0: "imageColorSun" ImageElement
    void* imageColorAmb;       // +0xF8: "imageColorAmb" ImageElement
    void* imageColorFog;       // +0x100: "imageColorFog" ImageElement
    void* comboWeather;        // +0x108: "comboWeather" ComboElement
    void* buttonSave;          // +0x110: "buttonSave" ButtonElement
    void* buttonMenuScene;     // +0x118: "buttonMenuScene" ButtonElement
    float smoothedFPS;         // +0x120: EMA FPS accumulator
    float smoothedFrameMS;     // +0x124: EMA frame time accumulator
    float smoothedTotalMS;     // +0x128: EMA total time accumulator
    uint8_t _pad12C[4];        // +0x12C: gap to reach 0x130
};
static_assert(sizeof(DebugUI) == 0x130, "DebugUI size mismatch");
static_assert(offsetof(DebugUI, diseaseCombo) == 0x30, "");
static_assert(offsetof(DebugUI, numberDrawCalls) == 0x40, "");
static_assert(offsetof(DebugUI, numberFPS) == 0x60, "");
static_assert(offsetof(DebugUI, sliderDay) == 0x78, "");
static_assert(offsetof(DebugUI, valueSunH) == 0xE0, "");
static_assert(offsetof(DebugUI, imageColorSun) == 0xF0, "");
static_assert(offsetof(DebugUI, comboWeather) == 0x108, "");
static_assert(offsetof(DebugUI, smoothedFPS) == 0x120, "");

// --- DestroyUI (0x78) ---
// Factory inits: +0x30..+0x70=0 (9 qwords)
struct DestroyUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint64_t data38;           // +0x38
    uint64_t data40;           // +0x40
    uint64_t data48;           // +0x48
    uint64_t data50;           // +0x50
    uint64_t data58;           // +0x58
    uint64_t data60;           // +0x60
    uint64_t data68;           // +0x68
    uint64_t data70;           // +0x70
};
static_assert(sizeof(DestroyUI) == 0x78, "DestroyUI size mismatch");

// --- EntityUI (0x48) ---
struct EntityUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint64_t data38;           // +0x38: zeroed
    uint64_t data40;           // +0x40: zeroed
};
static_assert(sizeof(EntityUI) == 0x48, "EntityUI size mismatch");

// --- EventLogUI (0x80) ---
// Factory inits: +0x30=0, (gap +0x48), +0x50..+0x78=0
struct EventLogUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint64_t data38;           // +0x38: zeroed
    uint64_t data40;           // +0x40: zeroed
    uint8_t  _pad48[8];        // +0x48: gap (not initialized by factory)
    uint64_t data50;           // +0x50: zeroed
    uint64_t data58;           // +0x58
    uint64_t data60;           // +0x60
    uint64_t data68;           // +0x68
    uint64_t data70;           // +0x70
    uint64_t data78;           // +0x78
};
static_assert(sizeof(EventLogUI) == 0x80, "EventLogUI size mismatch");

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

// --- OrchardUI (0x48) --- extends SelectResourceUI
// Factory: SelectResourceUI vtable -> InternalBasePtr@+0x30 -> override vtable, +0x38=0, +0x40=0
struct OrchardUI {
    DialogController base;     // +0x00
    uint64_t entityRef;        // +0x30: InternalBasePtr (initialized null)
    uint64_t data38;           // +0x38: zeroed
    uint64_t data40;           // +0x40: zeroed
};
static_assert(sizeof(OrchardUI) == 0x48, "OrchardUI size mismatch");

// --- PageToggleUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/PageToggleUI.h"

// --- PastureUI (0x40) --- extends SelectResourceUI
// Factory: SelectResourceUI vtable -> InternalBasePtr@+0x30 -> override vtable, +0x38=0
struct PastureUI {
    DialogController base;     // +0x00
    uint64_t entityRef;        // +0x30: InternalBasePtr (initialized null)
    uint64_t data38;           // +0x38: zeroed
};
static_assert(sizeof(PastureUI) == 0x40, "PastureUI size mismatch");

// --- ProductionUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/ProductionUI.h"

// --- ProfessionUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/ProfessionUI.h"

// --- RadiusUI (0x38) ---
// Factory inits: +0x30(u32)=0xFFFFFFFF
struct RadiusUI {
    DialogController base;     // +0x00
    int32_t  data30;           // +0x30: initialized to -1
    uint8_t  _pad34[4];        // +0x34: gap
};
static_assert(sizeof(RadiusUI) == 0x38, "RadiusUI size mismatch");
static_assert(offsetof(RadiusUI, data30) == 0x30, "");

// --- ResidenceUI (0x40) ---
struct ResidenceUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint64_t data38;           // +0x38: zeroed
};
static_assert(sizeof(ResidenceUI) == 0x40, "ResidenceUI size mismatch");

// --- ResourceLimitUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/ResourceLimitUI.h"

// --- SelectResourceUI (abstract) ---
// No instantiation. Placeholder factory. Base for CropUI, OrchardUI, PastureUI.
// vtable = 0x14015d330

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

// --- StatusBarUI (0x1D0) ---
// Has separate Ctor (0x14007af20). Largest panel.
// Original source: C:\Projects\Buildy\code\Game\Dialog/StatusBarUI.h
struct StatusBarUI {
    DialogController base;     // +0x00
    void* labelTitleParam;     // +0x30: "labelTitleParam" TextElement
    void* textSeason;          // +0x38: "textSeason" TextElement
    void* numberYear;          // +0x40: "numberYear" NumberElement
    void* textPopulation;      // +0x48: "textPopulation" TextElement
    void* numberTemperature;   // +0x50: "numberTemperature" NumberElement
    void* numberWood;          // +0x58: "numberWood" NumberElement
    void* numberStone;         // +0x60: "numberStone" NumberElement
    void* numberIron;          // +0x68: "numberIron" NumberElement
    void* numberFireWood;      // +0x70: "numberFireWood" NumberElement
    void* numberTools;         // +0x78: "numberTools" NumberElement
    void* numberFood;          // +0x80: "numberFood" NumberElement
    void* numberHerbs;         // +0x88: "numberHerbs" NumberElement
    void* numberClothes;       // +0x90: "numberClothes" NumberElement
    void* numberAlcohol;       // +0x98: "numberAlcohol" NumberElement
    void* numberCoal;          // +0xA0: "numberCoal" NumberElement
    void* numberTextile;       // +0xA8: "numberTextile" NumberElement
    void* numberCustom[10];    // +0xB0..+0xF8: "numberCustom0".."numberCustom9"
    uint32_t statusCitizenIndex; // +0x100: cycling index for next-citizen-with-status navigation
    uint8_t  _pad104[4];         // +0x104: gap
    uint8_t  heartsIconArray[0x20]; // +0x108..+0x127: hearts icon array (built by FUN_14006abf0)
    uint8_t  starsIconArray[0x20];  // +0x128..+0x147: stars icon array
    void* imageStatusHome;     // +0x148
    void* imageStatusFood;     // +0x150
    void* imageStatusCold;     // +0x158
    void* imageStatusJob;      // +0x160
    void* imageStatusTool;     // +0x168
    void* imageStatusDisease;  // +0x170
    void* numberStatusHome;    // +0x178
    void* numberStatusFood;    // +0x180
    void* numberStatusCold;    // +0x188
    void* numberStatusJob;     // +0x190
    void* numberStatusTool;    // +0x198
    void* numberStatusDisease; // +0x1A0
    void* imageRain;           // +0x1A8
    void* imageCloudy;         // +0x1B0
    void* imagePartlySunny;    // +0x1B8
    void* imageSunny;          // +0x1C0
    void* imageSnow;           // +0x1C8
};
static_assert(sizeof(StatusBarUI) == 0x1D0, "StatusBarUI size mismatch");
static_assert(offsetof(StatusBarUI, labelTitleParam) == 0x30, "");
static_assert(offsetof(StatusBarUI, statusCitizenIndex) == 0x100, "");
static_assert(offsetof(StatusBarUI, imageStatusHome) == 0x148, "");
static_assert(offsetof(StatusBarUI, imageRain) == 0x1A8, "");

// --- StatusIconUI (0x40) ---
struct StatusIconUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint64_t data38;           // +0x38: zeroed
};
static_assert(sizeof(StatusIconUI) == 0x40, "StatusIconUI size mismatch");

// --- StorageCapacityUI (0x38) ---
struct StorageCapacityUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
};
static_assert(sizeof(StorageCapacityUI) == 0x38, "StorageCapacityUI size mismatch");

// --- StorageUI (0x40) ---
struct StorageUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint64_t data38;           // +0x38: zeroed
};
static_assert(sizeof(StorageUI) == 0x40, "StorageUI size mismatch");

// --- TitleDescToolTipUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/TitleDescToolTipUI.h"

// --- ToolUI --- the class: ENDGAME B.3.8 PNL (B8-4 forwarder)
#include "Dialog/ToolUI.h"

// --- TownHallUI (0x1E8) ---
// Has separate Ctor (0x140081280). Second-largest panel.
// Ctor inits: +0x30..+0x58 (mixed u32/u64), +0x60..+0x138=0,
//   +0x140(u32)=0, +0x150..+0x1E0=0
//
// Tab layout:
//   General:     +0x60..+0xD8 (numberYear, textMonth, numberHomes, etc., groupHearts/Stars)
//   Professions: +0x40..+0x58 (profession BST + entry array with 0x40-byte entries)
//   Production:  +0xF0..+0x110 (limits table entries + combo + header elems)
//   Inventory:   +0x118..+0x130 (table entries array + inventoryCombo + sortCombo)
//   Graph:       +0x148..+0x1C8 (graphData, timeCombo, graphCombo, number0-4, label0-3, stringTables)
//   Agriculture: +0x1C8..+0x1E0 (agri label array + agri BST + count)
struct TownHallUI {
    DialogController base;     // +0x00
    uint64_t tabElement;       // +0x30: InternalBasePtr to TabElement
    uint32_t profButtonIdx;    // +0x38: current profession button cycle index
    uint8_t  _pad3C[4];        // +0x3C: gap
    uint64_t profEntryArray;   // +0x40: dynamic array ptr (0x40-byte profession entries)
    uint64_t profEntryMeta;    // +0x48: [low32]=count, [high32]=capacity
    uint64_t profBST;          // +0x50: profession BST root (sorted by type ID)
    uint32_t profBSTCount;     // +0x58: profession BST node count
    uint8_t  _pad5C[4];        // +0x5C: gap
    // --- General tab elements ---
    uint64_t numberYear;       // +0x60: NumberElement
    uint64_t textMonth;        // +0x68: TextElement (label with sprite)
    uint64_t numberHomes;      // +0x70: NumberElement
    uint64_t numberFamily;     // +0x78: NumberElement
    uint64_t numberCitizens;   // +0x80: NumberElement
    uint64_t numberAdults;     // +0x88: NumberElement
    uint64_t numberStudents;   // +0x90: NumberElement
    uint64_t numberChildren;   // +0x98: NumberElement
    uint64_t percentClothed;   // +0xA0: TextElement (formatted %)
    uint64_t percentEducated;  // +0xA8: TextElement (formatted %)
    uint64_t groupHearts[4];   // +0xB0: {wholeArray, wholeCount|wholeCap, halfArray, halfCount|halfCap}
    uint64_t groupStars[4];    // +0xD0: {wholeArray, wholeCount|wholeCap, halfArray, halfCount|halfCap}
    // --- Production tab elements ---
    uint64_t prodLimitsArray;  // +0xF0: dynamic array ptr (0x28-byte limit entries)
    uint64_t prodLimitsMeta;   // +0xF8: [low32]=count, [high32]=capacity
    uint64_t productionCombo;  // +0x100: ComboElement
    uint64_t limitPrevUsedHdr; // +0x108: TextElement
    uint64_t limitPrevProdHdr; // +0x110: TextElement
    // --- Inventory tab elements ---
    uint64_t invTableArray;    // +0x118: dynamic array ptr (0x10-byte entries: label, number)
    uint32_t invTableCount;    // +0x120: count
    uint32_t invTableCap;      // +0x124: capacity
    uint64_t inventoryCombo;   // +0x128: ComboElement
    uint64_t sortCombo;        // +0x130: ComboElement (sort order)
    uint64_t invBST;           // +0x138: inventory BST root
    uint32_t invBSTDummy;      // +0x140: zeroed (u32 write)
    uint8_t  _pad144[4];       // +0x144: gap
    int32_t  cachedGameTick;   // +0x148: cached game time tick for graph refresh
    uint8_t  _pad14C[4];       // +0x14C: gap
    // --- Graph tab elements ---
    uint64_t graphYearsTable;  // +0x150: StringTable (graphYears)
    uint64_t graphTypesTable;  // +0x158: StringTable (graphTypes)
    uint64_t graphData;        // +0x160: GraphElement
    uint64_t timeCombo;        // +0x168: ComboElement (time range)
    uint64_t graphCombo;       // +0x170: ComboElement (graph data type)
    uint64_t textXMin;         // +0x178: TextElement (x-axis min label)
    uint64_t number0;          // +0x180: NumberElement (y-axis max)
    uint64_t number1;          // +0x188: NumberElement (y-axis 3/4)
    uint64_t number2;          // +0x190: NumberElement (y-axis 1/2)
    uint64_t number3;          // +0x198: NumberElement (y-axis 1/4)
    uint64_t number4;          // +0x1A0: NumberElement (y-axis min) — unused?
    uint64_t label0;           // +0x1A8: LabelElement (graph legend 0)
    uint64_t label1;           // +0x1B0: LabelElement (graph legend 1)
    uint64_t label2;           // +0x1B8: LabelElement (graph legend 2)
    uint64_t label3;           // +0x1C0: LabelElement (graph legend 3)
    // --- Agriculture tab elements ---
    uint64_t agriLabelArray;   // +0x1C8: dynamic array ptr (8-byte label ptrs)
    uint32_t agriLabelCount;   // +0x1D0: count
    uint32_t agriLabelCap;     // +0x1D4: capacity
    uint64_t agriBST;          // +0x1D8: agriculture BST root (sorted by entity ptr)
    uint32_t agriBSTCount;     // +0x1E0: agriculture BST item count
    uint8_t  _pad1E4[4];       // +0x1E4: gap
};
static_assert(sizeof(TownHallUI) == 0x1E8, "TownHallUI size mismatch");
static_assert(offsetof(TownHallUI, tabElement) == 0x30, "");
static_assert(offsetof(TownHallUI, numberYear) == 0x60, "");
static_assert(offsetof(TownHallUI, productionCombo) == 0x100, "");
static_assert(offsetof(TownHallUI, inventoryCombo) == 0x128, "");
static_assert(offsetof(TownHallUI, cachedGameTick) == 0x148, "");
static_assert(offsetof(TownHallUI, graphData) == 0x160, "");
static_assert(offsetof(TownHallUI, agriBSTCount) == 0x1E0, "");

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

// --- UpgradeUI (0x38) ---
struct UpgradeUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
};
static_assert(sizeof(UpgradeUI) == 0x38, "UpgradeUI size mismatch");

// --- WorkPlaceUI (0x68) ---
// Factory inits: +0x30..+0x58=0, +0x60(u32)=0
struct WorkPlaceUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint64_t data38;           // +0x38
    uint64_t data40;           // +0x40
    uint64_t data48;           // +0x48
    uint64_t data50;           // +0x50
    uint64_t data58;           // +0x58
    uint32_t data60;           // +0x60: zeroed (u32 write)
    uint8_t  _pad64[4];        // +0x64: gap
};
static_assert(sizeof(WorkPlaceUI) == 0x68, "WorkPlaceUI size mismatch");
