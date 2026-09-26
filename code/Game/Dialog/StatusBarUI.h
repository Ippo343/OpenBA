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

class StatusBarUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(StatusBarUI)

public:
    StatusBarUI() = default;                             // 0x1400140e0 (ctor inlined in the factory)

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x140014130
    // [4] STANDALONE DEVIATION: retail inherits DialogController::OnCreated (RET); the tree drives this panel from here.
    void OnCreated() override;
    void OnOpened() override;   // [5] 0x14007a3d0
    void OnClosed() override {}   // [6] 0x14007a3c0  RET 0 in retail
    void OnUpdateLayout() override;   // [7] 0x1400798d0
    // [8] STANDALONE DEVIATION: retail inherits DialogController::OnUpdate (RET); the tree drives this panel's per-frame refresh here.
    void OnUpdate(const System::Time& time) override;
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x14007a3a0

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    void* labelTitleParam = nullptr;     // +0x30: "labelTitleParam" TextElement
    void* textSeason = nullptr;          // +0x38: "textSeason" TextElement
    void* numberYear = nullptr;          // +0x40: "numberYear" NumberElement
    void* textPopulation = nullptr;      // +0x48: "textPopulation" TextElement
    void* numberTemperature = nullptr;   // +0x50: "numberTemperature" NumberElement
    void* numberWood = nullptr;          // +0x58: "numberWood" NumberElement
    void* numberStone = nullptr;         // +0x60: "numberStone" NumberElement
    void* numberIron = nullptr;          // +0x68: "numberIron" NumberElement
    void* numberFireWood = nullptr;      // +0x70: "numberFireWood" NumberElement
    void* numberTools = nullptr;         // +0x78: "numberTools" NumberElement
    void* numberFood = nullptr;          // +0x80: "numberFood" NumberElement
    void* numberHerbs = nullptr;         // +0x88: "numberHerbs" NumberElement
    void* numberClothes = nullptr;       // +0x90: "numberClothes" NumberElement
    void* numberAlcohol = nullptr;       // +0x98: "numberAlcohol" NumberElement
    void* numberCoal = nullptr;          // +0xA0: "numberCoal" NumberElement
    void* numberTextile = nullptr;       // +0xA8: "numberTextile" NumberElement
    void* numberCustom[10] = {};    // +0xB0..+0xF8: "numberCustom0".."numberCustom9"
    uint32_t statusCitizenIndex = 0; // +0x100: cycling index for next-citizen-with-status navigation
    uint8_t _pad104[4] = {};         // +0x104: gap
    uint8_t heartsIconArray[0x20] = {}; // +0x108..+0x127: hearts icon array (built by FUN_14006abf0)
    uint8_t starsIconArray[0x20] = {};  // +0x128..+0x147: stars icon array
    void* imageStatusHome = nullptr;     // +0x148
    void* imageStatusFood = nullptr;     // +0x150
    void* imageStatusCold = nullptr;     // +0x158
    void* imageStatusJob = nullptr;      // +0x160
    void* imageStatusTool = nullptr;     // +0x168
    void* imageStatusDisease = nullptr;  // +0x170
    void* numberStatusHome = nullptr;    // +0x178
    void* numberStatusFood = nullptr;    // +0x180
    void* numberStatusCold = nullptr;    // +0x188
    void* numberStatusJob = nullptr;     // +0x190
    void* numberStatusTool = nullptr;    // +0x198
    void* numberStatusDisease = nullptr; // +0x1A0
    void* imageRain = nullptr;           // +0x1A8
    void* imageCloudy = nullptr;         // +0x1B0
    void* imagePartlySunny = nullptr;    // +0x1B8
    void* imageSunny = nullptr;          // +0x1C0
    void* imageSnow = nullptr;           // +0x1C8

protected:
    // 0x140014130 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // Retail frees the icon arrays at +0x138, +0x128, +0x118, +0x108; the tree's
    // teardown body (moved into ~StatusBarUI unchanged) does the same.
    ~StatusBarUI();   // the tree's teardown body (see the .cpp)
};

static_assert(sizeof(StatusBarUI) == 0x1d0, "StatusBarUI is 0x1d0 (create 0x1400140e0 MemAlloc(0x1d0))");
static_assert(alignof(StatusBarUI) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(StatusBarUI, labelTitleParam) == 0x30, "labelTitleParam @ 0x30");
static_assert(offsetof(StatusBarUI, statusCitizenIndex) == 0x100, "statusCitizenIndex @ 0x100");
static_assert(offsetof(StatusBarUI, imageStatusHome) == 0x148, "imageStatusHome @ 0x148");
static_assert(offsetof(StatusBarUI, imageRain) == 0x1A8, "imageRain @ 0x1A8");
