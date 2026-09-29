#pragma once
// ============================================================================
// code/Game/Dialog/StatusBarUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "StatusBarUI", parent
//   "UI::DialogController", table 0x14015d810, 11 slots, sizeof 0x1d0.
//   create 0x1400140e0: MemAlloc(0x1d0, 8), ??0DialogController, stamp, fields.
//   [0] 0x140014130 scalar deleting dtor (~StatusBarUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [4] retail inherits the base; the tree overrides it (STANDALONE DEVIATION)
//   [5] 0x14007a3d0  OnOpened
//   [6] 0x14007a3c0  RET 0
//   [7] 0x1400798d0  OnUpdateLayout
//   [8] retail inherits the base; the tree overrides it (STANDALONE DEVIATION)
//   [9] 0x14007a3a0  OnElementEvent
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"
#include "runtime/types/collection/Collection.h"

namespace UI { class Element; class TextElement; class NumberElement; }

class StatusBarUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(StatusBarUI)

public:
    StatusBarUI() = default;                             // 0x1400140e0 (ctor inlined in the factory)

    // 0x140014130 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // Retail frees the icon arrays at +0x138, +0x128, +0x118, +0x108: the four
    // Array members' dtors, in that order.
    ~StatusBarUI() override = default;                            // [0] 0x140014130
    // [4] STANDALONE DEVIATION: retail inherits DialogController::OnCreated (RET); the tree drives this panel from here.
    void OnCreated() override;
    void OnOpened() override;   // [5] 0x14007a3d0
    void OnClosed() override {}   // [6] 0x14007a3c0  RET 0 in retail
    void OnUpdateLayout() override;   // [7] 0x1400798d0
    // [8] STANDALONE DEVIATION: retail inherits DialogController::OnUpdate (RET); the tree drives this panel's per-frame refresh here.
    void OnUpdate(const System::Time& time) override;
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x14007a3a0

    // ---- fields: OnOpened 0x14007a3d0 finds each by its .rsc name --------
    UI::TextElement*   labelTitleParam = nullptr;     // +0x30 "labelTitleParam" (retail checks LabelElement)
    UI::TextElement*   textSeason = nullptr;          // +0x38 "textSeason": Month%i of its own table
    UI::NumberElement* numberYear = nullptr;          // +0x40 "numberYear"
    UI::TextElement*   textPopulation = nullptr;      // +0x48 "textPopulation": adults, students, children
    UI::NumberElement* numberTemperature = nullptr;   // +0x50 "numberTemperature"
    // +0x58..+0xA8: the stock counts, CitizenBehavior::professionResources[0..10]
    UI::NumberElement* numberWood = nullptr;          // +0x58 [0]
    UI::NumberElement* numberStone = nullptr;         // +0x60 [1]
    UI::NumberElement* numberIron = nullptr;          // +0x68 [2]
    UI::NumberElement* numberFireWood = nullptr;      // +0x70 [3]
    UI::NumberElement* numberTools = nullptr;         // +0x78 [4]
    UI::NumberElement* numberFood = nullptr;          // +0x80 [5]
    UI::NumberElement* numberHerbs = nullptr;         // +0x88 [6]
    UI::NumberElement* numberClothes = nullptr;       // +0x90 [7]
    UI::NumberElement* numberAlcohol = nullptr;       // +0x98 [8]
    UI::NumberElement* numberCoal = nullptr;          // +0xA0 [9]
    UI::NumberElement* numberTextile = nullptr;       // +0xA8 [10]
    UI::NumberElement* numberCustom[10] = {};         // +0xB0..+0xF8 "numberCustom0".."9": [11..20]
    uint32_t statusCitizenIndex = 0;  // +0x100 the next-citizen-with-status cycle (0x14007a120)
    // +0x104: a gap in retail; the tree's OnUpdate uses it as its refresh throttle.
    uint32_t refreshThrottle = 0;
    // +0x108 / +0x128: the hearts and stars icon groups (CitizenUI_BuildIconArray
    // 0x14006abf0): each the "whole" and the "half" icon of every child of the
    // group. ~StatusBarUI 0x140014130 frees +0x138, +0x128, +0x118, +0x108.
    Collection::Array<UI::Element*> heartsWhole;      // +0x108
    Collection::Array<UI::Element*> heartsHalf;       // +0x118
    Collection::Array<UI::Element*> starsWhole;       // +0x128
    Collection::Array<UI::Element*> starsHalf;        // +0x138
    // +0x148..+0x170: the status icons, element ids 100..105; bit n of
    // StatusIconComponent's activeMask
    UI::Element*     imageStatusHome = nullptr;       // +0x148 bit 0
    UI::Element*     imageStatusFood = nullptr;       // +0x150 bit 1
    UI::Element*     imageStatusCold = nullptr;       // +0x158 bit 2
    UI::Element*     imageStatusJob = nullptr;        // +0x160 bit 3
    UI::Element*     imageStatusTool = nullptr;       // +0x168 bit 4
    UI::Element*     imageStatusDisease = nullptr;    // +0x170 bit 5
    // +0x178..+0x1A0: each status's citizen count (TextElement::SetParameter 0)
    UI::TextElement* numberStatusHome = nullptr;      // +0x178
    UI::TextElement* numberStatusFood = nullptr;      // +0x180
    UI::TextElement* numberStatusCold = nullptr;      // +0x188
    UI::TextElement* numberStatusJob = nullptr;       // +0x190
    UI::TextElement* numberStatusTool = nullptr;      // +0x198
    UI::TextElement* numberStatusDisease = nullptr;   // +0x1A0
    // +0x1A8..+0x1C8: WeatherSimulation::field_0x10 0..3, 4 = snow
    UI::Element*     imageRain = nullptr;             // +0x1A8 0
    UI::Element*     imageCloudy = nullptr;           // +0x1B0 1
    UI::Element*     imagePartlySunny = nullptr;      // +0x1B8 2
    UI::Element*     imageSunny = nullptr;            // +0x1C0 3
    UI::Element*     imageSnow = nullptr;             // +0x1C8 4
};

static_assert(sizeof(StatusBarUI) == 0x1d0, "StatusBarUI is 0x1d0 (create 0x1400140e0 MemAlloc(0x1d0))");
static_assert(alignof(StatusBarUI) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(StatusBarUI, labelTitleParam) == 0x30, "labelTitleParam @ 0x30");
static_assert(offsetof(StatusBarUI, statusCitizenIndex) == 0x100, "statusCitizenIndex @ 0x100");
static_assert(offsetof(StatusBarUI, textSeason) == 0x38, "textSeason @ 0x38");
static_assert(offsetof(StatusBarUI, numberYear) == 0x40, "numberYear @ 0x40");
static_assert(offsetof(StatusBarUI, textPopulation) == 0x48, "textPopulation @ 0x48");
static_assert(offsetof(StatusBarUI, numberTemperature) == 0x50, "numberTemperature @ 0x50");
static_assert(offsetof(StatusBarUI, numberWood) == 0x58, "numberWood @ 0x58");
static_assert(offsetof(StatusBarUI, numberTextile) == 0xA8, "numberTextile @ 0xA8");
static_assert(offsetof(StatusBarUI, numberCustom) == 0xB0, "numberCustom @ 0xB0");
static_assert(offsetof(StatusBarUI, refreshThrottle) == 0x104, "refreshThrottle @ 0x104");
static_assert(offsetof(StatusBarUI, heartsWhole) == 0x108, "hearts @ 0x108 (0x1400798d0)");
static_assert(offsetof(StatusBarUI, heartsHalf) == 0x118, "hearts half @ 0x118");
static_assert(offsetof(StatusBarUI, starsWhole) == 0x128, "stars @ 0x128 (0x1400798d0)");
static_assert(offsetof(StatusBarUI, starsHalf) == 0x138, "stars half @ 0x138");
static_assert(offsetof(StatusBarUI, imageStatusHome) == 0x148, "imageStatusHome @ 0x148");
static_assert(offsetof(StatusBarUI, numberStatusHome) == 0x178, "numberStatusHome @ 0x178");
static_assert(offsetof(StatusBarUI, imageRain) == 0x1A8, "imageRain @ 0x1A8");
