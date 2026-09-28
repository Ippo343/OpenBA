#pragma once
// ============================================================================
// code/Game/Component/FrustumOutlineElement.h — ENDGAME B.3.8 block VIS
//
// `FrustumOutlineElement` (the minimap's camera-frustum outline) as a real C++
// class with a compiler-generated vtable (B38_RULES.md D1-D5), a subclass of
// the Runtime's real `UI::Element` (block RB).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "FrustumOutlineElement", parent "Element", ti 0x1401c51a0
//   create 0x140012400: MemAlloc(0xd8, 8), `??0Element@UI@@`, stamp 0x14015c2a0,
//                       `??0Sprite@UI@@` on +0xc0, +0xd0 (qword) = -1
//   createInPlace 0x140012470 (the same body on `this`)
//   dtor   0x140012560 (slot 0): restamp, ~Element, MemFree on &1
//   table  0x14015c2a0, 12 slots:
//     [0] 0x140012560 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] Element::Persist (import)          [4] 0x14006c290 ComputeSize
//     [5] 0x14006c1c0 ComputeLayout          [6] 0x14006be90 Draw
//     [7]-[11] UI::Element's (imports: Update, Enable, Contains, SetColor, GetBorder)
//   sizeof 0xd8.
//
// Before this class the tree COPIED 24 entries of the minted UI::Element table
// (12 past its end — B38 D9 #11) and patched slots 4/5/6 (a D5 slot-copy site).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "ui/Element.h"
#include "ui/Sprite.h"
#include "Object/exe_type.h"

class FrustumOutlineElement final : public UI::Element {
    DECLARE_EXE_TYPE(FrustumOutlineElement)

public:
    FrustumOutlineElement();                                          // 0x14006c2f0

    // ~Element only
    ~FrustumOutlineElement() override = default;                      // [0] 0x140012560
    void ComputeSize() override;                                      // [4] 0x14006c290
    void ComputeLayout(int32_t x, int32_t y,
                       int32_t w, int32_t h) override;                // [5] 0x14006c1c0
    void Draw(UI::SpriteContext& context, uint8_t alpha) const override; // [6] 0x14006be90

    // PUBLIC while the ported bodies read them (B38 decision 1).
    UI::Sprite _lineSprite;   // +0xc0 the outline's line sprite
    int32_t    _width;        // +0xd0 minimap width  (ctor -1)
    int32_t    _height;       // +0xd4 minimap height (ctor -1)
};

static_assert(sizeof(FrustumOutlineElement) == 0xd8, "FrustumOutlineElement is 0xd8 (create 0x140012400)");
static_assert(alignof(FrustumOutlineElement) == 8, "FrustumOutlineElement alignment");
static_assert(offsetof(FrustumOutlineElement, _lineSprite) == 0xc0, "+0xc0 Sprite (0x14006c390)");
static_assert(offsetof(FrustumOutlineElement, _width) == 0xd0, "+0xd0 (0x14006c39d)");
static_assert(offsetof(FrustumOutlineElement, _height) == 0xd4, "+0xd4");
