#pragma once
// ============================================================================
// runtime/types/ui/ScrollElement.h — UI::ScrollElement, the REAL C++ class
// (ENDGAME B.3 step 6, block C4)
// ============================================================================
//
// PARENT `UI::Element` — READ, not inferred:
//     ?_ParentTypeName@ScrollElement@UI@@CAPEBDXZ @0x18003a820 -> "Element"
//
// RULE B5-15 / B6-7 — THE TWO NAME LITERALS, RECORDED, NOT USED (slots 1/2 keep
// reading the minted `?_typeInfo@ScrollElement@UI@@…`, 0x1801fad98):
//     ?_TypeName@ScrollElement@UI@@CAPEBDXZ       @0x18003a810 -> "ScrollElement"
//     ?_ParentTypeName@ScrollElement@UI@@CAPEBDXZ @0x18003a820 -> "Element"
//
// `??_7ScrollElement@UI@@6B@` @ 0x180156400, TWELVE slots (B36_SLOTS.md
// section 1; sizeof from `MOV EDX,0x178` in slot 0 0x18003adc0):
//   [0] 0x18003adc0 deleting dtor (row)  [1] 0x18003a850 TypeName
//   [2] 0x18003a830 Type                 [3] Element::Persist INHERITED
//   [4] 0x18011ad80 ComputeSize          [5] 0x18011af20 ComputeLayout
//   [6] 0x18011b0b0 Draw                 [7] 0x18011b620 Update
//   [8]..[11] Element's, INHERITED
//
// ABSTRACT (slot 0 pure), no real ctor: every live one is built by the
// `??0ScrollElement` DOOR (scrollelement.cpp) and wears the MINTED table.
//
// LAYOUT (RULE B6-8) — MOVED here from scrollelement.cpp's file-local
// constants and element_bodies_b5.cpp's GetBarLayout raw reads. Evidence:
// `??0ScrollElement@UI@@QEAA@XZ` (0x18011a7f0), read as disassembly:
//     nine { MOV qword [RBX+off],0 ; MOV dword [RBX+off+8],-1 }  off = 0xc0..0x140
//     MOV byte  [RBX+0x150],1
//     MOV qword [RBX+0x154],-1          (UNALIGNED: +0x154 and +0x158)
//     MOV dword [RBX+0x160],0 ; MOV dword [RBX+0x164],0x3eaaaaab (0.33333334f)
//     MOV qword [RBX+0x168],0x40800000  (+0x168 = 4.0f, +0x16c = 0)
//     MOV dword [RBX+0x170],0
// NOT written by the ctor: the nine sprites' +0xc pads, +0x151..+0x153,
// +0x15c (`ScrollDescription::CreateInstance` writes it) and +0x174.
// The one-line setters (0x18003a9d0 SetMaxValue: +0x168, clamped to >= 0;
// 0x18003a9f0 SetDisplayPercent: +0x164, clamped to <= 1.0f; 0x18003aa10
// SetCurrentValue / 0x18003aa20 GetCurrentValue: +0x160) name the floats.
// ~ScrollElement (0x18011a900) destroys nothing of its own.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "Element.h"
#include "Sprite.h"   // ::Sprite (block D folds it into UI::Sprite with a global alias)

namespace UI {

class ScrollElement : public Element {
public:
    // ---- the twelve slots, in measured order -------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x18003adc0
    const char* TypeName() const override;                        // [1] 0x18003a850
    uint32_t    Type() const override;                            // [2] 0x18003a830
    void ComputeSize() override;                                  // [4] 0x18011ad80
    void ComputeLayout(int32_t x, int32_t y,
                       int32_t right, int32_t bottom) override;   // [5] 0x18011af20
    void Draw(SpriteContext& context, uint8_t alpha) const override; // [6] 0x18011b0b0
    void Update(const System::Time& time, UserInterface* ui,
                Object::InternalPtr<DialogInstance>* dialog,
                const UpdateState& state) override;               // [7] 0x18011b620

    // ---- non-virtual members -----------------------------------------------
    float GetCurrentValue() const       { return _currentValue; } // 0x18003aa20 MOVSS XMM0,[RCX+0x160]
    void  SetCurrentValue(float value)  { _currentValue = value; }// 0x18003aa10 MOVSS [RCX+0x160],XMM1
    // Exported, not ported (nothing in the tree calls them): declared, undefined.
    void  SetMaxValue(float value);                               // 0x18003a9d0
    void  SetDisplayPercent(float percent);                       // 0x18003a9f0
    // PORTED, body in ANOTHER BLOCK'S FILE (element_bodies_b5.cpp,
    // `RtUi_ScrollElement_GetBarLayout`) — fragment pass. Retail: private.
    void  GetBarLayout(int32_t& left, int32_t& top,
                       int32_t& right, int32_t& bottom) const;    // 0x18011a2d0

    ScrollElement(const ScrollElement& src);                      // 0x18003aa30  not ported
    ScrollElement& operator=(const ScrollElement& src);           // 0x18003abe0  not ported
    static uint32_t      TypeId();                                // 0x18003a840  not ported
    static Object::Base* CreateNewObject();                       // 0x18003a940  ui_factory_wave28.cpp door
    static Object::Base* CreateInPlaceObject(void* place);        // 0x18003a9c0  ui_factory_wave28.cpp door

protected:
    // NOT retail's `??0ScrollElement@UI@@QEAA@XZ` (0x18011a7f0, the door).
    ScrollElement() = default;
    // `??1ScrollElement@UI@@UEAA@XZ` (0x18011a900): non-virtual, not defined in
    // the real build; the retail body is the door `RtUi_ScrollElement_Dtor`.
    ~ScrollElement();

private:
    static const char* _TypeName();                               // 0x18003a810 -> "ScrollElement"
    static const char* _ParentTypeName();                         // 0x18003a820 -> "Element"
    static Object::BaseTypeInfo _typeInfo;                        // 0x1801fad98 (minted)

public:
    // ---- the 0xb8 bytes after Element --------------------------------------
    ::Sprite _trackStart;        // +0xc0   the groove, three-slice
    ::Sprite _trackMiddle;       // +0xd0
    ::Sprite _trackEnd;          // +0xe0
    ::Sprite _thumbStart;        // +0xf0   the thumb, idle (GetBarLayout sums
    ::Sprite _thumbMiddle;       // +0x100   these three for its minimum size)
    ::Sprite _thumbEnd;          // +0x110
    ::Sprite _hotStart;          // +0x120  the thumb while the element is
    ::Sprite _hotMiddle;         // +0x130   interactive and has range
    ::Sprite _hotEnd;            // +0x140
    uint8_t  _vertical;          // +0x150  ctor 1
    uint8_t  _pad151[3];         // +0x151  written by nothing
    uint32_t _barColor;          // +0x154  ctor -1 (one unaligned qword with +0x158); Draw's thumb colour
    uint32_t _unknown158;        // +0x158  ctor -1; ScrollDescription::CreateInstance writes it; no body reads it
    int32_t  _inset;             // +0x15c  NOT written by the ctor; GetBarLayout recesses the track by it
    float    _currentValue;      // +0x160  ctor 0
    float    _displayPercent;    // +0x164  ctor 0.33333334f; the visible fraction
    float    _maxValue;          // +0x168  ctor 4.0f; the scrollable range
    float    _dragAnchor;        // +0x16c  ctor 0 (high half of the +0x168 qword)
    float    _dragValue;         // +0x170  ctor 0
    uint32_t _pad174;            // +0x174  written by nothing
};

static_assert(sizeof(ScrollElement) == 0x178,
              "UI::ScrollElement is 0x178 bytes (MOV EDX,0x178 in its slot-0 "
              "vector deleting destructor 0x18003adc0)");
static_assert(alignof(ScrollElement) == 8, "UI::ScrollElement aligns to 8");
static_assert(offsetof(ScrollElement, _trackStart) == sizeof(Element), "");
static_assert(offsetof(ScrollElement, _trackStart)     == 0xc0,  "");
static_assert(offsetof(ScrollElement, _trackMiddle)    == 0xd0,  "");
static_assert(offsetof(ScrollElement, _trackEnd)       == 0xe0,  "");
static_assert(offsetof(ScrollElement, _thumbStart)     == 0xf0,  "");
static_assert(offsetof(ScrollElement, _thumbMiddle)    == 0x100, "");
static_assert(offsetof(ScrollElement, _thumbEnd)       == 0x110, "");
static_assert(offsetof(ScrollElement, _hotStart)       == 0x120, "");
static_assert(offsetof(ScrollElement, _hotMiddle)      == 0x130, "");
static_assert(offsetof(ScrollElement, _hotEnd)         == 0x140, "");
static_assert(offsetof(ScrollElement, _vertical)       == 0x150, "");
static_assert(offsetof(ScrollElement, _barColor)       == 0x154, "");
static_assert(offsetof(ScrollElement, _unknown158)     == 0x158, "");
static_assert(offsetof(ScrollElement, _inset)          == 0x15c, "");
static_assert(offsetof(ScrollElement, _currentValue)   == 0x160, "");
static_assert(offsetof(ScrollElement, _displayPercent) == 0x164, "");
static_assert(offsetof(ScrollElement, _maxValue)       == 0x168, "");
static_assert(offsetof(ScrollElement, _dragAnchor)     == 0x16c, "");
static_assert(offsetof(ScrollElement, _dragValue)      == 0x170, "");

}  // namespace UI
