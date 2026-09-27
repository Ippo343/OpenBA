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
// ENDGAME B.3 step 9 (block UI-B, B39_RULES D1/D2) -- the paragraph below is
// HISTORY: the class is now CONCRETE. A real public ctor (retail's stores), its
// own slot 0 (the vector deleting dtor), a protected non-virtual ~ScrollDescription holding
// the retired ??1 door's body, and the factory pair as the class's own statics;
// every live one wears the COMPILER's table. The minted `??_7ScrollDescription@UI@@6B@`,
// its RT_DELDTOR row 0x18003a790 and the ctor / dtor / slot doors are retired
// (tools/rt_mint_retired/ui-b.txt).
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
    // ??0ScrollDescription@UI@@QEAA@XZ 0x18011a4e0 -- the REAL ctor:
    // ElementDescription's stores (inlined in retail), then the sheet, the
    // nine names, +0xb8 BYTE 1, +0xbc DWORD 1, and ONE QWORD of -1 over
    // +0xc0/+0xc4. +0xb9..+0xbb are written by nothing.
    ScrollDescription() : ElementDescription() {
        _spriteSheet = nullptr;
        for (int i = 0; i < 9; ++i) _spriteNames[i] = nullptr;
        _vertical  = 1;
        _inset     = 1;
        _barColor  = 0xffffffffu;
        _unknownC4 = 0xffffffffu;
    }

    // [0] 0x18003a790 -- MSVC's VECTOR deleting destructor, both arms exactly as the
    // retired RT_DELDTOR row ran them (`MOV EDX,0xc8` = the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<ScrollDescription*>(self + static_cast<int64_t>(i) * 0xc8)->~ScrollDescription();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~ScrollDescription();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
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
    static uint32_t      TypeId();          // over the minted `?_typeInfo@` record until W3
    static Object::Base* CreateNewObject();                       // (D2) the class's own
    static Object::Base* CreateInPlaceObject(void* place);        // (D2) the class's own

protected:
    // `??1ScrollDescription@UI@@UEAA@XZ` (0x18011a5d0): NON-VIRTUAL (no virtual dtor until
    // the normal-virtual cutover). Retail's body is in scrolldescription.cpp;
    // the compiler chains the base's.
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
