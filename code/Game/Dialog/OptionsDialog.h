#pragma once
// ============================================================================
// code/Game/Dialog/OptionsDialog.h — ENDGAME B.3.8 block DLG
//
// The Options dialog (video / audio / input / game settings).
// Original source: C:\Projects\Buildy\code\Game\Dialog/OptionsDialog.h
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "OptionsDialog", parent literal "public UI::DialogInstance"
//   (sic — B38_RULES section 1.3), ti 0x1401c5360
//   create 0x140012df0 (MemAlloc(0x1c0, 8)), createInPlace 0x140012e30
//   ctors 0x140075150 (the door the tree registers), 0x140075280
//   table 0x14015ceb0, 10 slots:
//     [0] 0x140012f30 scalar deleting dtor  [1] TypeName  [2] Type
//     [3] DialogInstance::Persist (import)  [4] 0x1400742d0 OnOpened
//     [5] 0x140073480 OnClosed `RET 0`      [6] DialogInstance::OnUpdateLayout
//     [7] DialogInstance::OnUpdate          [8] 0x140072f20 OnElementEvent
//     [9] DialogInstance::OnSerializeSettings
//   sizeof 0x1c0.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "runtime/types/ui/DialogInstance.h"
#include "Object/exe_type.h"

class OptionsDialog final : public UI::DialogInstance {
    DECLARE_EXE_TYPE(OptionsDialog)

public:
    OptionsDialog();                                                // create 0x140012df0
    OptionsDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                  Object::InternalPtr<UI::DialogInstance> parent);  // 0x140075150

    Object::Base* DeletingDtor(unsigned flags) override;           // [0] 0x140012f30
    void OnOpened() override;                                      // [4] 0x1400742d0
    void OnClosed() override;                                      // [5] 0x140073480 RET
    // [6] / [7]: retail INHERITS DialogInstance's bodies; the hand-built table
    // had a print-once no-op in both. Kept (PORT GAP, B38 D3.1).
    void OnUpdateLayout() override;                                // [6] PORT GAP
    void OnUpdate(const System::Time& time) override;              // [7] PORT GAP
    void OnElementEvent(Object::InternalPtr<UI::Element>* element,
                        int32_t a, int32_t b, int32_t c,
                        int32_t d, int32_t e, void* userData) override;  // [8] 0x140072f20

protected:
    ~OptionsDialog() = default;

private:
    void ZeroFields();                                             // +0xB8..+0x1B8 = 0

public:
    void* comboRenderer;       // +0xB8: "comboRenderer" ComboElement (ID 4)
    void* comboAdapter;        // +0xC0: "comboAdapter" ComboElement (ID 5)
    void* comboResolution;     // +0xC8: "comboResolution" ComboElement (ID 6)
    void* comboRefresh;        // +0xD0: "comboRefresh" ComboElement (ID 7)
    void* comboAntialias;      // +0xD8: "comboAntialias" ComboElement (ID 8)
    void* checkFullscreen;     // +0xE0: "checkFullscreen" CheckboxElement
    void* checkVSync;          // +0xE8: "checkVSync" CheckboxElement
    void* comboFilter;         // +0xF0: "comboFilter" ComboElement (ID 9)
    void* comboShadowRes;      // +0xF8: "comboShadowResolution" ComboElement
    void* comboShadowQuality;  // +0x100: "comboShadowQuality" ComboElement
    void* comboReflectionQuality; // +0x108: "comboReflectionQuality" ComboElement
    void* inputList;           // +0x110: "inputList" Element (key bind list)
    void* checkSoftwareMouse;  // +0x118: "checkSoftwareMouse" CheckboxElement
    void* sliderUI;            // +0x120: "sliderUI" SliderElement (ID 0)
    void* sliderMusic;         // +0x128: "sliderMusic" SliderElement (ID 1)
    void* sliderAmbient;       // +0x130: "sliderAmbient" SliderElement (ID 2)
    void* sliderEffect;        // +0x138: "sliderEffect" SliderElement (ID 3)
    void* sliderUIScale;       // +0x140: "sliderUIScale" SliderElement
    void* comboAutosave;       // +0x148: "comboAutosave" ComboElement
    void* checkStatusIcon;     // +0x150: "checkStatusIcon" CheckboxElement
    void* sliderStatusIcon;    // +0x158: "sliderStatusIcon" SliderElement
    void* sliderStatusIconScale; // +0x160: "sliderStatusIconScale" SliderElement
    void* checkEdgeScroll;     // +0x168: "checkEdgeScroll" CheckboxElement
    void* checkShowCelsius;    // +0x170: "checkShowCelsius" CheckboxElement
    void* checkShowWeather;    // +0x178: "checkShowWeather" CheckboxElement
    void* checkAutoslow;       // +0x180: "checkAutoslow" CheckboxElement
    void* checkAutopause;      // +0x188: "checkAutopause" CheckboxElement
    uint32_t savedRenderer;    // +0x190: saved renderer index (VideoConfig+0x04)
    uint32_t savedAdapter;     // +0x194: saved adapter index
    uint32_t savedWidth;       // +0x198: saved resolution width
    uint32_t savedHeight;      // +0x19C: saved resolution height
    uint32_t savedRefreshNum;  // +0x1A0: saved refresh rate numerator
    uint32_t savedRefreshDen;  // +0x1A4: saved refresh rate denominator
    uint32_t savedAntialias;   // +0x1A8: saved antialias mode
    uint32_t savedFilter;      // +0x1AC: saved texture filter mode
    void* checkMouseClip;      // +0x1B0: "checkMouseClip" CheckboxElement
    void* tabOptions;          // +0x1B8: "tabOptions" TabElement
};

static_assert(offsetof(OptionsDialog, comboRenderer) == 0xB8, "OptionsDialog +0xb8");
static_assert(offsetof(OptionsDialog, checkAutopause) == 0x188, "OptionsDialog +0x188");
static_assert(offsetof(OptionsDialog, savedRenderer) == 0x190, "OptionsDialog +0x190");
static_assert(offsetof(OptionsDialog, checkMouseClip) == 0x1B0, "OptionsDialog +0x1b0");
static_assert(offsetof(OptionsDialog, tabOptions) == 0x1B8, "OptionsDialog +0x1b8");
static_assert(sizeof(OptionsDialog) == 0x1C0, "OptionsDialog is 0x1c0 (create 0x140012df0)");
static_assert(alignof(OptionsDialog) == 8, "OptionsDialog is qword-aligned");
