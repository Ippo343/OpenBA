#pragma once
// ============================================================================
// runtime/types/ui/LayoutDescription.h — UI::LayoutDescription, the REAL C++
// class (ENDGAME B.3 step 6, block C4)
// ============================================================================
//
// PARENT `UI::ElementDescription` — READ, not inferred:
//     ?_ParentTypeName@LayoutDescription@UI@@CAPEBDXZ @0x180042d10 -> "ElementDescription"
//
// RULE B5-15 / B6-7 — THE TWO NAME LITERALS, RECORDED, NOT USED (slots 1/2 keep
// reading the minted `?_typeInfo@LayoutDescription@UI@@…`, 0x1801fa078):
//     ?_TypeName@LayoutDescription@UI@@CAPEBDXZ       @0x180042d00 -> "LayoutDescription"
//     ?_ParentTypeName@LayoutDescription@UI@@CAPEBDXZ @0x180042d10 -> "ElementDescription"
//
// `??_7LayoutDescription@UI@@6B@` @ 0x180156cc0, TEN slots (B36_SLOTS.md
// section 1; sizeof from `MOV EDX,0x78` in slot 0 0x1800431d0):
//   [0] 0x1800431d0 deleting dtor (row)  [1] 0x180042d40 TypeName
//   [2] 0x180042d20 Type                 [3] 0x1800d3a00 GetFileVersion
//   [4] 0x1800066e0 External::Compile INHERITED
//   [5] 0x1800d3940 Serialize            [6] 0x180042fc0 Reconstruct
//   [7] 0x180043100 Destruct             [8] 0x1800d33a0 CreateInstance RETSLOT
//   [9] 0x180043060 Construct            INTRODUCED by the leaf (RULE B6-1.1)
//
// ABSTRACT (slot 0 pure), no real ctor. `??0LayoutDescription@UI@@QEAA@XZ`
// (0x1800d3a90) is ui_factory_wave28.cpp's door; Construct and Reconstruct
// INLINE the same stores (0x180043076.., 0x180042fde..).
//
// LAYOUT (RULE B6-8) — MOVED here from layoutdescription.cpp's raw offsets.
// ElementDescription (0x68) plus ONE array, `Array<Entry>` at +0x68 (the inlined
// ctor: `MOV qword [p+0x68],0 ; MOV qword [p+0x70],0` at 0x1800430e2/e6).
// Entry is 0x38 bytes; its fields are named from the entry serializer
// (0x180043f50: ExternalPtr 0x180038f90 over +0x00, System::String 0x1800c9930
// over +0x08/+0x10/+0x18, kScalarUInt +0x20/+0x24, kScalarInt +0x28/+0x2c,
// kScalarBool +0x30) and from what CreateInstance (0x1800d33a0) does with each:
// +0x08 is the name OTHER entries look up; +0x10/+0x18 are the names this entry
// is positioned relative to, gated by +0x20/+0x24 being non-zero; +0x20..+0x30
// are copied into LayoutElement's per-child record (runtime/types/ui/
// LayoutElement.h), where GetChildLayout (0x1800d2dc0) reads +0x00 as the X
// alignment and +0x04 as the Y alignment — and the exported
// `?GetChildLayoutX@LayoutElement@UI@@AEBAHW4LayoutHorizontal@LayoutDescription@2@HHHH@Z`
// / `…GetChildLayoutY@…W4LayoutVertical@…` name those two as the enums
// `LayoutDescription::LayoutHorizontal` / `LayoutVertical`. Their enumerator
// NAMES are not recoverable; no names are invented. +0x31..+0x37 are written by
// nothing.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "ElementDescription.h"
#include "../collection/Collection.h"

namespace UI {

class Dialog;
class Element;

class LayoutDescription : public ElementDescription {
public:
    // `W4` enums: four bytes, passed in a register. Values 1..5 are the five
    // alignment arms GetChildLayout's AlignAxis switches on; names unknown.
    enum LayoutHorizontal : int32_t {};
    enum LayoutVertical   : int32_t {};

    // One child entry, `Array<Entry>` stride 0x38 (0x180043d80's spec).
    struct Entry {
        void*            desc;         // +0x00  ExternalPtr<UI::ElementDescription>
        char*            name;         // +0x08  System::String — what siblings look up
        char*            relativeToX;  // +0x10  System::String — gated by `horizontal`
        char*            relativeToY;  // +0x18  System::String — gated by `vertical`
        LayoutHorizontal horizontal;   // +0x20  kScalarUInt; -> record +0x00
        LayoutVertical   vertical;     // +0x24  kScalarUInt; -> record +0x04
        int32_t          offsetX;      // +0x28  kScalarInt;  -> record +0x10
        int32_t          offsetY;      // +0x2c  kScalarInt;  -> record +0x14
        uint8_t          force;        // +0x30  kScalarBool; -> record +0x18 (laid out even when hidden)
        uint8_t          _pad31[7];    // +0x31  written by nothing
    };

    // ---- the ten slots, in measured order ----------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x1800431d0
    const char*   TypeName() const override;                      // [1] 0x180042d40
    uint32_t      Type() const override;                          // [2] 0x180042d20
    int32_t       GetFileVersion() const override;                // [3] 0x1800d3a00
    bool          Serialize(void* stream) override;               // [5] 0x1800d3940
    Object::External* Reconstruct() override;                     // [6] 0x180042fc0
    void          Destruct() override;                            // [7] 0x180043100
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                     // [8] 0x1800d33a0 RETSLOT
    virtual Object::External* Construct();                        // [9] 0x180043060

    // ---- exported, not ported: declared, deliberately undefined ------------
    LayoutDescription(const LayoutDescription& src);              // 0x180043110
    LayoutDescription& operator=(const LayoutDescription& src);   // 0x180043170
    static uint32_t      TypeId();                                // 0x180042d30
    static Object::Base* CreateNewObject();                       // 0x180042e30  ui_factory_wave28.cpp door
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180042f20  ui_factory_wave28.cpp door

protected:
    // NOT retail's `??0LayoutDescription@UI@@QEAA@XZ` (0x1800d3a90, the door).
    LayoutDescription() = default;
    // `??1LayoutDescription@UI@@UEAA@XZ` (0x1800d3a10): non-virtual, not
    // defined in the real build; the door `RtUi_LayoutDescription_Dtor` is.
    ~LayoutDescription();

private:
    static const char* _TypeName();                               // 0x180042d00 -> "LayoutDescription"
    static const char* _ParentTypeName();                         // 0x180042d10 -> "ElementDescription"
    static Object::BaseTypeInfo _typeInfo;                        // 0x1801fa078 (minted)

public:
    // ---- the 0x10 bytes after ElementDescription ---------------------------
    Collection::Array<Entry> _entries;   // +0x68 .. +0x77  ~Array 0x180043a00 (visits the Strings)
};

static_assert(sizeof(LayoutDescription::LayoutHorizontal) == 4, "a W4 enum is four bytes");
static_assert(sizeof(LayoutDescription::Entry) == 0x38, "Array<Entry> stride (0x180043d80)");
static_assert(offsetof(LayoutDescription::Entry, name)        == 0x08, "");
static_assert(offsetof(LayoutDescription::Entry, relativeToX) == 0x10, "");
static_assert(offsetof(LayoutDescription::Entry, relativeToY) == 0x18, "");
static_assert(offsetof(LayoutDescription::Entry, horizontal)  == 0x20, "");
static_assert(offsetof(LayoutDescription::Entry, vertical)    == 0x24, "");
static_assert(offsetof(LayoutDescription::Entry, offsetX)     == 0x28, "");
static_assert(offsetof(LayoutDescription::Entry, offsetY)     == 0x2c, "");
static_assert(offsetof(LayoutDescription::Entry, force)       == 0x30, "");
static_assert(sizeof(LayoutDescription) == 0x78,
              "UI::LayoutDescription is 0x78 bytes (MOV EDX,0x78 in its slot-0 "
              "vector deleting destructor 0x1800431d0)");
static_assert(alignof(LayoutDescription) == 8, "UI::LayoutDescription aligns to 8");
static_assert(offsetof(LayoutDescription, _entries) == sizeof(ElementDescription), "");
static_assert(offsetof(LayoutDescription, _entries) == 0x68, "");

}  // namespace UI
