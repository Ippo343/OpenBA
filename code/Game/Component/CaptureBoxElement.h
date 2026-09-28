#pragma once
// ============================================================================
// code/Game/Component/CaptureBoxElement.h — ENDGAME B.3.8 block VIS
//
// `CaptureBoxElement` (the rubber-band box of the debug SpriteCaptureUI) as a
// real C++ class with a compiler-generated vtable (B38_RULES.md D1-D5), a
// subclass of the Runtime's real `UI::Element` (block RB).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "CaptureBoxElement", parent "Element", ti 0x1401c5638
//   create 0x140013eb0: MemAlloc(0xe0, 8), `??0Element@UI@@`, stamp 0x14015d440,
//                       `??0Sprite@UI@@` on +0xc0, +0xd0 (qword) = +0xd8 (qword) = 0
//   createInPlace 0x140013f20 (the same body on `this`)
//   dtor   0x140014070 (slot 0): restamp, ~Element, MemFree on &1
//   table  0x14015d440, 12 slots:
//     [0] 0x140014070 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] Element::Persist (import)          [4] 0x140078fb0 ComputeSize
//     [5] 0x140078ee0 ComputeLayout          [6] 0x140078da0 Draw (the four box lines)
//     [7]-[11] UI::Element's (imports)
//   sizeof 0xe0.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "ui/Element.h"
#include "ui/Sprite.h"
#include "Object/exe_type.h"

class CaptureBoxElement final : public UI::Element {
    DECLARE_EXE_TYPE(CaptureBoxElement)

public:
    CaptureBoxElement();                                              // 0x140079040

    // ~Element only
    ~CaptureBoxElement() override = default;                          // [0] 0x140014070
    void ComputeSize() override;                                      // [4] 0x140078fb0
    void ComputeLayout(int32_t x, int32_t y,
                       int32_t w, int32_t h) override;                // [5] 0x140078ee0
    void Draw(UI::SpriteContext& context, uint8_t alpha) const override; // [6] 0x140078da0

    // PUBLIC while the ported bodies read them (B38 decision 1).
    UI::Sprite _lineSprite;   // +0xc0 the box's line sprite
    int32_t    _x0;           // +0xd0 box corner (ctor 0)
    int32_t    _y0;           // +0xd4
    int32_t    _x1;           // +0xd8 opposite corner (ctor 0)
    int32_t    _y1;           // +0xdc
};

static_assert(sizeof(CaptureBoxElement) == 0xe0, "CaptureBoxElement is 0xe0 (create 0x140013eb0)");
static_assert(alignof(CaptureBoxElement) == 8, "CaptureBoxElement alignment");
static_assert(offsetof(CaptureBoxElement, _lineSprite) == 0xc0, "+0xc0 Sprite");
static_assert(offsetof(CaptureBoxElement, _x0) == 0xd0, "+0xd0");
static_assert(offsetof(CaptureBoxElement, _x1) == 0xd8, "+0xd8");
static_assert(offsetof(CaptureBoxElement, _y1) == 0xdc, "+0xdc");
