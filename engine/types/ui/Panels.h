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

// --- AttendanceUI (0x38) ---
struct AttendanceUI {
    DialogController base;     // +0x00 (0x30)
    uint64_t data30;           // +0x30: zeroed
};
static_assert(sizeof(AttendanceUI) == 0x38, "AttendanceUI size mismatch");
static_assert(offsetof(AttendanceUI, data30) == 0x30, "");

// --- BuildRequirementUI (0x60) ---
// Factory inits: +0x30=0, +0x38(u32)=1, +0x40..+0x58=0
struct BuildRequirementUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint32_t data38;           // +0x38: initialized to 1
    uint8_t  _pad3C[4];        // +0x3C: gap
    uint64_t data40;           // +0x40: zeroed
    uint64_t data48;           // +0x48: zeroed
    uint64_t data50;           // +0x50: zeroed
    uint64_t data58;           // +0x58: zeroed
};
static_assert(sizeof(BuildRequirementUI) == 0x60, "BuildRequirementUI size mismatch");
static_assert(offsetof(BuildRequirementUI, data38) == 0x38, "");

// --- BuildUI (0x50) ---
struct BuildUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint64_t data38;           // +0x38: zeroed
    uint64_t data40;           // +0x40: zeroed
    uint64_t data48;           // +0x48: zeroed
};
static_assert(sizeof(BuildUI) == 0x50, "BuildUI size mismatch");

// --- CemeteryUI (0x38) ---
struct CemeteryUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
};
static_assert(sizeof(CemeteryUI) == 0x38, "CemeteryUI size mismatch");

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

// --- ConsumeProduceUI (0x40) ---
struct ConsumeProduceUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint64_t data38;           // +0x38: zeroed
};
static_assert(sizeof(ConsumeProduceUI) == 0x40, "ConsumeProduceUI size mismatch");

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

// --- HelpUI (0x78) ---
// Has separate Ctor (0x14006a920). Inits +0x30..+0x58=0, +0x60=-1, +0x68=0, +0x70=0.
struct HelpUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint64_t data38;           // +0x38: zeroed
    uint64_t data40;           // +0x40: zeroed
    uint64_t data48;           // +0x48: zeroed
    uint64_t data50;           // +0x50: zeroed
    uint64_t data58;           // +0x58: zeroed
    int64_t  data60;           // +0x60: initialized to -1 (0xFFFFFFFFFFFFFFFF)
    uint64_t data68;           // +0x68: zeroed
    uint64_t data70;           // +0x70: zeroed
};
static_assert(sizeof(HelpUI) == 0x78, "HelpUI size mismatch");
static_assert(offsetof(HelpUI, data60) == 0x60, "");

// --- HotKeyToolTipUI (0x30) ---
// No subclass fields (size == base).
struct HotKeyToolTipUI {
    DialogController base;     // +0x00
};
static_assert(sizeof(HotKeyToolTipUI) == 0x30, "HotKeyToolTipUI size mismatch");

// --- ImmigrationUI (0x50) ---
struct ImmigrationUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint64_t data38;           // +0x38
    uint64_t data40;           // +0x40
    uint64_t data48;           // +0x48
};
static_assert(sizeof(ImmigrationUI) == 0x50, "ImmigrationUI size mismatch");

// --- MapUI (0x50) ---
struct MapUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint64_t data38;           // +0x38
    uint64_t data40;           // +0x40
    uint64_t data48;           // +0x48
};
static_assert(sizeof(MapUI) == 0x50, "MapUI size mismatch");

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

// --- PageToggleUI (0x48) ---
// Factory inits: +0x30=0, +0x38=0, +0x40(u32)=0
struct PageToggleUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint64_t data38;           // +0x38: zeroed
    uint32_t data40;           // +0x40: zeroed (u32 write)
    uint8_t  _pad44[4];        // +0x44: gap
};
static_assert(sizeof(PageToggleUI) == 0x48, "PageToggleUI size mismatch");

// --- PastureUI (0x40) --- extends SelectResourceUI
// Factory: SelectResourceUI vtable -> InternalBasePtr@+0x30 -> override vtable, +0x38=0
struct PastureUI {
    DialogController base;     // +0x00
    uint64_t entityRef;        // +0x30: InternalBasePtr (initialized null)
    uint64_t data38;           // +0x38: zeroed
};
static_assert(sizeof(PastureUI) == 0x40, "PastureUI size mismatch");

// --- ProductionUI (0x50) ---
// Factory uses _eh_vector_constructor_iterator_ for 2 items of 0x10 bytes at +0x30
struct ProductionUI {
    DialogController base;     // +0x00
    uint8_t  vectorItems[0x20];// +0x30: 2x 0x10-byte items (constructor-initialized)
};
static_assert(sizeof(ProductionUI) == 0x50, "ProductionUI size mismatch");

// --- ProfessionUI (0x78) ---
// Has separate Ctor (0x1400775a0). Inits +0x30..+0x60=0, +0x68(u32)=0, +0x70(u32)=0.
struct ProfessionUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint64_t data38;           // +0x38: zeroed
    uint64_t data40;           // +0x40: zeroed
    uint64_t data48;           // +0x48: zeroed
    uint64_t data50;           // +0x50: zeroed
    uint64_t data58;           // +0x58: zeroed
    uint64_t data60;           // +0x60: zeroed
    uint32_t data68;           // +0x68: zeroed (u32 write)
    uint8_t  _pad6C[4];        // +0x6C: gap
    uint32_t data70;           // +0x70: zeroed (u32 write)
    uint8_t  _pad74[4];        // +0x74: gap
};
static_assert(sizeof(ProfessionUI) == 0x78, "ProfessionUI size mismatch");
static_assert(offsetof(ProfessionUI, data30) == 0x30, "");
static_assert(offsetof(ProfessionUI, data68) == 0x68, "");

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

// --- ResourceLimitUI (0x40) ---
struct ResourceLimitUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint64_t data38;           // +0x38: zeroed
};
static_assert(sizeof(ResourceLimitUI) == 0x40, "ResourceLimitUI size mismatch");

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
    uint32_t _pad100;          // +0x100: zeroed (u32)
    uint8_t  _pad104[4];       // +0x104: gap
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
static_assert(offsetof(StatusBarUI, _pad100) == 0x100, "");
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

// --- TitleDescToolTipUI (0x30) ---
// No subclass fields.
struct TitleDescToolTipUI {
    DialogController base;     // +0x00
};
static_assert(sizeof(TitleDescToolTipUI) == 0x30, "TitleDescToolTipUI size mismatch");

// --- ToolUI (0x58) ---
// Factory inits: +0x30=0, +0x38=0, +0x40(u16)=0, +0x44(u64)=0, +0x4C(u16)=0, +0x50=0
struct ToolUI {
    DialogController base;     // +0x00
    void*    componentData;    // +0x30: BuildComponent or sub-component ptr
    void*    labelTitle;       // +0x38: TextElement "labelTitle"
    uint8_t  hasBorderTitle;   // +0x40: borderTitle element exists (enables title drag)
    uint8_t  isDragging;       // +0x41: currently dragging dialog
    uint8_t  _pad42[2];        // +0x42: gap
    int32_t  dragOffsetX;      // +0x44: mouse offset from dialog left
    int32_t  dragOffsetY;      // +0x48: mouse offset from dialog top
    uint8_t  hasTitleDragZone; // +0x4C: borderTitle has drag zone enabled
    uint8_t  isDragPending;    // +0x4D: waiting for mouse movement threshold
    uint8_t  _pad4E[2];        // +0x4E: gap
    int32_t  dragStartX;       // +0x50: drag start mouse X
    int32_t  dragStartY;       // +0x54: drag start mouse Y
};
static_assert(sizeof(ToolUI) == 0x58, "ToolUI size mismatch");

// --- TownHallUI (0x1E8) ---
// Has separate Ctor (0x140081280). Second-largest panel.
// Ctor inits: +0x30..+0x58 (mixed u32/u64), +0x60..+0x138=0,
//   +0x140(u32)=0, +0x150..+0x1E0=0
struct TownHallUI {
    DialogController base;     // +0x00
    uint64_t data30;           // +0x30: zeroed
    uint32_t data38;           // +0x38: zeroed (u32 write)
    uint8_t  _pad3C[4];        // +0x3C: gap
    uint64_t data40;           // +0x40
    uint64_t data48;           // +0x48
    uint64_t data50;           // +0x50
    uint32_t data58;           // +0x58: zeroed (u32 write)
    uint8_t  _pad5C[4];        // +0x5C: gap
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
    uint64_t dataE0;           // +0xE0
    uint64_t dataE8;           // +0xE8
    uint64_t dataF0;           // +0xF0
    uint64_t dataF8;           // +0xF8
    uint64_t data100;          // +0x100
    uint64_t data108;          // +0x108
    uint64_t data110;          // +0x110
    uint64_t data118;          // +0x118
    uint64_t data120;          // +0x120
    uint64_t data128;          // +0x128
    uint64_t data130;          // +0x130
    uint64_t data138;          // +0x138
    uint32_t data140;          // +0x140: zeroed (u32 write)
    uint8_t  _pad144[0xC];     // +0x144: gap
    uint64_t data150;          // +0x150
    uint64_t data158;          // +0x158
    uint64_t data160;          // +0x160
    uint64_t data168;          // +0x168
    uint64_t data170;          // +0x170
    uint64_t data178;          // +0x178
    uint64_t data180;          // +0x180
    uint64_t data188;          // +0x188
    uint64_t data190;          // +0x190
    uint64_t data198;          // +0x198
    uint64_t data1A0;          // +0x1A0
    uint64_t data1A8;          // +0x1A8
    uint64_t data1B0;          // +0x1B0
    uint64_t data1B8;          // +0x1B8
    uint64_t data1C0;          // +0x1C0
    uint64_t data1C8;          // +0x1C8
    uint64_t data1D0;          // +0x1D0
    uint64_t data1D8;          // +0x1D8
    uint32_t data1E0;          // +0x1E0: zeroed (u32 write)
    uint8_t  _pad1E4[4];       // +0x1E4: gap
};
static_assert(sizeof(TownHallUI) == 0x1E8, "TownHallUI size mismatch");
static_assert(offsetof(TownHallUI, data30) == 0x30, "");
static_assert(offsetof(TownHallUI, data140) == 0x140, "");
static_assert(offsetof(TownHallUI, data1E0) == 0x1E0, "");

// --- TradeUI (0x148) ---
// Has separate Ctor (0x140086b80). Inits +0x30..+0x140=0 (all zeroed qwords).
struct TradeUI {
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
    uint64_t dataE0;           // +0xE0
    uint64_t dataE8;           // +0xE8
    uint64_t dataF0;           // +0xF0
    uint64_t dataF8;           // +0xF8
    uint64_t data100;          // +0x100
    uint64_t data108;          // +0x108
    uint64_t data110;          // +0x110
    uint64_t data118;          // +0x118
    uint64_t data120;          // +0x120
    uint64_t data128;          // +0x128
    uint64_t data130;          // +0x130
    uint64_t data138;          // +0x138
    uint64_t data140;          // +0x140
};
static_assert(sizeof(TradeUI) == 0x148, "TradeUI size mismatch");
static_assert(offsetof(TradeUI, data30) == 0x30, "");

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
