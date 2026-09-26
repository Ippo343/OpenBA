#pragma once
// ============================================================================
// runtime/types/ui/ScrollDescription.h — UI::ScrollDescription, the REAL C++
// class (ENDGAME B.3 step 6, block C4)
// ============================================================================
//
// PARENT `UI::ElementDescription` — READ, not inferred:
//     ?_ParentTypeName@ScrollDescription@UI@@CAPEBDXZ @0x18003a0f0 -> "ElementDescription"
//
// RULE B5-15 / B6-7 — THE TWO NAME LITERALS, RECORDED, NOT USED (slots 1/2 keep
// reading the minted `?_typeInfo@ScrollDescription@UI@@…`, 0x1801fad60):
//     ?_TypeName@ScrollDescription@UI@@CAPEBDXZ       @0x18003a0e0 -> "ScrollDescription"
//     ?_ParentTypeName@ScrollDescription@UI@@CAPEBDXZ @0x18003a0f0 -> "ElementDescription"
//
// `??_7ScrollDescription@UI@@6B@` @ 0x180156460, TEN slots (B36_SLOTS.md
// section 1; sizeof from `MOV EDX,0xc8` in slot 0 0x18003a790):
//   [0] 0x18003a790 deleting dtor (row)  [1] 0x18003a120 TypeName
//   [2] 0x18003a100 Type                 [3] 0x18011a930 GetFileVersion
//   [4] 0x1800066e0 External::Compile INHERITED
//   [5] 0x18011a940 Serialize            [6] 0x18003a2a0 Reconstruct
//   [7] 0x18003a2d0 Destruct             [8] 0x18011aac0 CreateInstance RETSLOT
//   [9] 0x18003a2c0 Construct            INTRODUCED by the leaf (RULE B6-1.1)
//
// ABSTRACT (slot 0 pure), no real ctor: every live one is built by the
// `??0ScrollDescription` DOOR (scrolldescription.cpp) and wears the MINTED table.
//
// LAYOUT (RULE B6-8) — MOVED here from scrolldescription.cpp's raw offsets.
// Evidence: `??0ScrollDescription@UI@@QEAA@XZ` (0x18011a4e0), whose tail is
//     MOV qword [RCX+0x68..0xb0],0 (ten qwords) ; MOV byte [RCX+0xb8],1
//     MOV dword [RCX+0xbc],1 ; MOV qword [RCX+0xc0],-1
// and Serialize (0x18011a940): ExternalPtr<SpriteSheet> 0x180004590 over +0x68,
// String 0x1800c9930 over +0x70..+0xb0, kScalarBool +0xb8, kScalarInt +0xbc,
// kScalarUInt +0xc0 and +0xc4. +0xb9..+0xbb are written by nothing.
// The names are what CreateInstance (0x18011aac0) copies each field INTO on
// the ScrollElement (runtime/types/ui/ScrollElement.h).
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "ElementDescription.h"

namespace UI {

class Dialog;
class Element;

class ScrollDescription : public ElementDescription {
public:
    // ---- the ten slots, in measured order ----------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x18003a790
    const char*   TypeName() const override;                      // [1] 0x18003a120
    uint32_t      Type() const override;                          // [2] 0x18003a100
    int32_t       GetFileVersion() const override;                // [3] 0x18011a930
    bool          Serialize(void* stream) override;               // [5] 0x18011a940
    Object::External* Reconstruct() override;                     // [6] 0x18003a2a0
    void          Destruct() override;                            // [7] 0x18003a2d0
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                     // [8] 0x18011aac0 RETSLOT
    virtual Object::External* Construct();                        // [9] 0x18003a2c0

    // ---- exported, not ported: declared, deliberately undefined ------------
    ScrollDescription(const ScrollDescription& src);              // 0x18003a2e0
    ScrollDescription& operator=(const ScrollDescription& src);   // 0x18003a450
    static uint32_t      TypeId();                                // 0x18003a110
    static Object::Base* CreateNewObject();                       // 0x18003a210  ui_factory_wave28.cpp door
    static Object::Base* CreateInPlaceObject(void* place);        // 0x18003a290  ui_factory_wave28.cpp door

protected:
    // NOT retail's `??0ScrollDescription@UI@@QEAA@XZ` (0x18011a4e0, the door).
    ScrollDescription() = default;
    // `??1ScrollDescription@UI@@UEAA@XZ` (0x18011a5d0): non-virtual, not
    // defined in the real build; the door `RtUi_ScrollDescription_Dtor` is.
    ~ScrollDescription();

private:
    static const char* _TypeName();                               // 0x18003a0e0 -> "ScrollDescription"
    static const char* _ParentTypeName();                         // 0x18003a0f0 -> "ElementDescription"
    static Object::BaseTypeInfo _typeInfo;                        // 0x1801fad60 (minted)

public:
    // ---- the 0x60 bytes after ElementDescription ---------------------------
    void*    _spriteSheet;        // +0x68  ExternalPtr<UI::SpriteSheet>
    // System::String x9, in ScrollElement's sprite order: track start/middle/
    // end, thumb start/middle/end, hot start/middle/end (-> +0xc0 .. +0x140).
    char*    _spriteNames[9];     // +0x70 .. +0xb0
    uint8_t  _vertical;           // +0xb8  ctor 1; -> ScrollElement::_vertical (+0x150)
    uint8_t  _padb9[3];           // +0xb9  written by nothing
    int32_t  _inset;              // +0xbc  ctor 1; -> ScrollElement::_inset (+0x15c)
    uint32_t _barColor;           // +0xc0  ctor -1 (one qword with +0xc4); -> ScrollElement::_barColor (+0x154)
    uint32_t _unknownC4;          // +0xc4  ctor -1; -> ScrollElement::_unknown158
};

static_assert(sizeof(ScrollDescription) == 0xc8,
              "UI::ScrollDescription is 0xc8 bytes (MOV EDX,0xc8 in its slot-0 "
              "vector deleting destructor 0x18003a790)");
static_assert(alignof(ScrollDescription) == 8, "UI::ScrollDescription aligns to 8");
static_assert(offsetof(ScrollDescription, _spriteSheet) == sizeof(ElementDescription), "");
static_assert(offsetof(ScrollDescription, _spriteSheet) == 0x68, "");
static_assert(offsetof(ScrollDescription, _spriteNames) == 0x70, "");
static_assert(offsetof(ScrollDescription, _vertical)    == 0xb8, "");
static_assert(offsetof(ScrollDescription, _inset)       == 0xbc, "");
static_assert(offsetof(ScrollDescription, _barColor)    == 0xc0, "");
static_assert(offsetof(ScrollDescription, _unknownC4)   == 0xc4, "");

}  // namespace UI
