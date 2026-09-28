#pragma once
// ============================================================================
// code/Game/Dialog/DebugUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "DebugUI", parent
//   "DialogController", table 0x14015b870, 11 slots, sizeof 0x130.
//   create 0x140010ec0: MemAlloc(0x130, 8), ??0DialogController, stamp, fields.
//   [0] 0x140010f10 scalar deleting dtor (~DebugUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [4] 0x1400619e0  OnCreated
//   [6] 0x1400645f0  RET 0
//   [7] 0x140061280  OnUpdateLayout
//   [8] 0x1400610c0  OnUpdate
//   [9] 0x140060bc0  OnElementEvent
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class DebugUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(DebugUI)

public:
    DebugUI() = default;                                 // 0x140010ec0 (ctor inlined in the factory)

    // 0x140010f10 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    ~DebugUI() override = default;                                // [0] 0x140010f10
    void OnCreated() override;   // [4] 0x1400619e0
    void OnClosed() override {}   // [6] 0x1400645f0  RET 0 in retail
    void OnUpdateLayout() override;   // [7] 0x140061280
    void OnUpdate(const System::Time& time) override;   // [8] 0x1400610c0
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x140060bc0

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    void* diseaseCombo = nullptr;        // +0x30: "diseaseCombo" ComboElement
    void* drawCombo = nullptr;           // +0x38: "drawCombo" ComboElement
    void* numberDrawCalls = nullptr;     // +0x40: "numberDrawCalls" NumberElement
    void* numberTriangles = nullptr;     // +0x48: "numberTriangles" NumberElement
    void* numberVertices = nullptr;      // +0x50: "numberVertices" NumberElement
    void* numberBatches = nullptr;       // +0x58: "numberBatches" NumberElement
    void* numberFPS = nullptr;           // +0x60: "numberFPS" NumberElement (smoothed FPS, int)
    void* numberFrameMS = nullptr;       // +0x68: "numberFrameMS" NumberElement (smoothed frame ms, float)
    void* numberTotalMS = nullptr;       // +0x70: "numberTotalMS" NumberElement (smoothed total ms, float)
    void* sliderDay = nullptr;           // +0x78: "sliderDay" SliderElement (day of year)
    void* valueSunR = nullptr;           // +0x80: "valueSunR" SliderElement
    void* valueSunG = nullptr;           // +0x88: "valueSunG" SliderElement
    void* valueSunB = nullptr;           // +0x90: "valueSunB" SliderElement
    void* valueAmbR = nullptr;           // +0x98: "valueAmbR" SliderElement
    void* valueAmbG = nullptr;           // +0xA0: "valueAmbG" SliderElement
    void* valueAmbB = nullptr;           // +0xA8: "valueAmbB" SliderElement
    void* valueFogR = nullptr;           // +0xB0: "valueFogR" SliderElement
    void* valueFogG = nullptr;           // +0xB8: "valueFogG" SliderElement
    void* valueFogB = nullptr;           // +0xC0: "valueFogB" SliderElement
    void* valueFogN = nullptr;           // +0xC8: "valueFogN" SliderElement (fog near)
    void* valueFogF = nullptr;           // +0xD0: "valueFogF" SliderElement (fog far)
    void* valueFogM = nullptr;           // +0xD8: "valueFogM" SliderElement (fog max density)
    void* valueSunH = nullptr;           // +0xE0: "valueSunH" SliderElement (sun heading)
    void* valueSunP = nullptr;           // +0xE8: "valueSunP" SliderElement (sun pitch)
    void* imageColorSun = nullptr;       // +0xF0: "imageColorSun" ImageElement
    void* imageColorAmb = nullptr;       // +0xF8: "imageColorAmb" ImageElement
    void* imageColorFog = nullptr;       // +0x100: "imageColorFog" ImageElement
    void* comboWeather = nullptr;        // +0x108: "comboWeather" ComboElement
    void* buttonSave = nullptr;          // +0x110: "buttonSave" ButtonElement
    void* buttonMenuScene = nullptr;     // +0x118: "buttonMenuScene" ButtonElement
    float smoothedFPS = 0;         // +0x120: EMA FPS accumulator
    float smoothedFrameMS = 0;     // +0x124: EMA frame time accumulator
    float smoothedTotalMS = 0;     // +0x128: EMA total time accumulator
    uint8_t _pad12C[4] = {};        // +0x12C: gap to reach 0x130
};

static_assert(sizeof(DebugUI) == 0x130, "DebugUI is 0x130 (create 0x140010ec0 MemAlloc(0x130))");
static_assert(alignof(DebugUI) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(DebugUI, diseaseCombo) == 0x30, "diseaseCombo @ 0x30");
static_assert(offsetof(DebugUI, numberDrawCalls) == 0x40, "numberDrawCalls @ 0x40");
static_assert(offsetof(DebugUI, numberFPS) == 0x60, "numberFPS @ 0x60");
static_assert(offsetof(DebugUI, sliderDay) == 0x78, "sliderDay @ 0x78");
static_assert(offsetof(DebugUI, valueSunH) == 0xE0, "valueSunH @ 0xE0");
static_assert(offsetof(DebugUI, imageColorSun) == 0xF0, "imageColorSun @ 0xF0");
static_assert(offsetof(DebugUI, comboWeather) == 0x108, "comboWeather @ 0x108");
static_assert(offsetof(DebugUI, smoothedFPS) == 0x120, "smoothedFPS @ 0x120");
