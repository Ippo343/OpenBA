#pragma once
// ============================================================================
// runtime/types/ui/LayoutElement.h — UI::LayoutElement, the REAL C++ class
// (ENDGAME B.3 step 6, block C4)
// ============================================================================
//
// PARENT `UI::Element` — READ, not inferred:
//     ?_ParentTypeName@LayoutElement@UI@@CAPEBDXZ @0x180043260 -> "Element"
//
// RULE B5-15 / B6-7 — THE TWO NAME LITERALS, RECORDED, NOT USED (slots 1/2 keep
// reading the minted `?_typeInfo@LayoutElement@UI@@…`, 0x1801fa0b0):
//     ?_TypeName@LayoutElement@UI@@CAPEBDXZ       @0x180043250 -> "LayoutElement"
//     ?_ParentTypeName@LayoutElement@UI@@CAPEBDXZ @0x180043260 -> "Element"
//
// `??_7LayoutElement@UI@@6B@` @ 0x180156c60, TWELVE slots (B36_SLOTS.md
// section 1; sizeof from `MOV EDX,0xd0` in slot 0 0x180043570):
//   [0] 0x180043570 deleting dtor (row)  [1] 0x180043290 TypeName
//   [2] 0x180043270 Type                 [3] Element::Persist INHERITED
//   [4] 0x1800d3100 ComputeSize          [5] 0x1800d2af0 ComputeLayout
//   [6] 0x1800d2a40 Draw                 [7] Element::Update INHERITED
//   [8]..[11] Element's, INHERITED
//
// ABSTRACT (slot 0 pure), no real ctor: every live one is built by the
// `??0LayoutElement` DOOR (layoutelement.cpp) or inline by
// LayoutDescription::CreateInstance, and wears the MINTED table.
//
// LAYOUT (RULE B6-8) — MOVED here from layoutelement.cpp's file-local constants
// and element_bodies_b5.cpp's GetChildLayout raw reads. `??0LayoutElement@UI@@
// QEAA@XZ` (0x1800d32e0), read as disassembly: `MOV qword [RBX+0xc0],0 ; MOV
// qword [RBX+0xc8],0` — one Array header and nothing else; ~LayoutElement
// (0x1800d32a0) destroys exactly that array (0x180043cb0).
//
// THE PER-CHILD RECORD, 0x44 bytes (`IMUL R10,R10,0x44` in GetChildLayout
// 0x1800d2dc0; appended by LayoutDescription::CreateInstance through
// 0x180043b40, which writes only its first 0x24 bytes). Field names are the
// ones GetChildLayout's port gives them from what it does with each; the
// four TWO-element arrays are indexed by GetChildLayout's pass argument
// (0 = layout, 1 = the sizing pass ComputeSize runs). Retail names the struct
// nowhere; `ChildRecord` is descriptive.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "Element.h"
#include "LayoutDescription.h"   // LayoutDescription::LayoutHorizontal / LayoutVertical

namespace UI {

class LayoutElement : public Element {
public:
    struct ChildRecord {
        int32_t alignX;          // +0x00  LayoutDescription::Entry::horizontal
        int32_t alignY;          // +0x04  LayoutDescription::Entry::vertical
        int32_t refX;            // +0x08  sibling record index, <0 = the bounding box
        int32_t refY;            // +0x0c
        int32_t offsetX;         // +0x10  added only when GetChildLayout's bool is true
        int32_t offsetY;         // +0x14
        uint8_t force;           // +0x18  lay the child out even while it is hidden
        uint8_t _pad19[3];       // +0x19  CreateInstance's copy carries stack residue here
        int32_t sizeX;           // +0x1c  GetPaddedSize out-param (ComputeSize)
        int32_t sizeY;           // +0x20
        int32_t left[2];         // +0x24  [pass]
        int32_t top[2];          // +0x2c
        int32_t right[2];        // +0x34
        int32_t bottom[2];       // +0x3c
    };

    // ---- the twelve slots, in measured order -------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x180043570
    const char* TypeName() const override;                        // [1] 0x180043290
    uint32_t    Type() const override;                            // [2] 0x180043270
    void ComputeSize() override;                                  // [4] 0x1800d3100
    void ComputeLayout(int32_t x, int32_t y,
                       int32_t right, int32_t bottom) override;   // [5] 0x1800d2af0
    void Draw(SpriteContext& context, uint8_t alpha) const override; // [6] 0x1800d2a40
    // [7] Update and [8]..[11] — Element's, inherited.

    // ---- non-virtual members (private in retail) ----------------------------
    // PORTED, body in ANOTHER BLOCK'S FILE (element_bodies_b5.cpp,
    // `RtUi_LayoutElement_GetChildLayout`) — fragment pass.
    void GetChildLayout(int32_t index, int32_t pass, int32_t left, int32_t top,
                        int32_t right, int32_t bottom, bool applyOffsets);  // 0x1800d2dc0
    // Exported, not ported (GetChildLayout's port inlines them): declared, undefined.
    int32_t GetChildLayoutX(LayoutDescription::LayoutHorizontal align, int32_t nearEdge,
                            int32_t farEdge, int32_t size, int32_t offset) const;  // 0x1800d3090
    int32_t GetChildLayoutY(LayoutDescription::LayoutVertical align, int32_t nearEdge,
                            int32_t farEdge, int32_t size, int32_t offset) const;  // 0x1800d3020

    LayoutElement(const LayoutElement& src);                      // 0x180043470  not ported
    LayoutElement& operator=(const LayoutElement& src);           // 0x1800434d0  not ported
    static uint32_t      TypeId();                                // 0x180043280  not ported
    static Object::Base* CreateNewObject();                       // 0x180043380  ui_factory_wave28.cpp door
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180043420  ui_factory_wave28.cpp door

protected:
    // NOT retail's `??0LayoutElement@UI@@QEAA@XZ` (0x1800d32e0, the door).
    LayoutElement() = default;
    // `??1LayoutElement@UI@@UEAA@XZ` (0x1800d32a0): non-virtual, not defined in
    // the real build; the retail body is the door `RtUi_LayoutElement_Dtor`.
    ~LayoutElement();

private:
    static const char* _TypeName();                               // 0x180043250 -> "LayoutElement"
    static const char* _ParentTypeName();                         // 0x180043260 -> "Element"
    static Object::BaseTypeInfo _typeInfo;                        // 0x1801fa0b0 (minted)

public:
    // ---- the 0x10 bytes after Element --------------------------------------
    // One record per child, kept parallel to Element::_children by
    // LayoutDescription::CreateInstance; the loops count `_children`, not this.
    Collection::Array<ChildRecord> _childRecords;   // +0xc0 .. +0xcf
};

static_assert(sizeof(LayoutElement::ChildRecord) == 0x44, "IMUL R10,R10,0x44 (0x1800d2dc0)");
static_assert(offsetof(LayoutElement::ChildRecord, refX)    == 0x08, "");
static_assert(offsetof(LayoutElement::ChildRecord, offsetX) == 0x10, "");
static_assert(offsetof(LayoutElement::ChildRecord, force)   == 0x18, "");
static_assert(offsetof(LayoutElement::ChildRecord, sizeX)   == 0x1c, "");
static_assert(offsetof(LayoutElement::ChildRecord, left)    == 0x24, "");
static_assert(offsetof(LayoutElement::ChildRecord, top)     == 0x2c, "");
static_assert(offsetof(LayoutElement::ChildRecord, right)   == 0x34, "");
static_assert(offsetof(LayoutElement::ChildRecord, bottom)  == 0x3c, "");
static_assert(sizeof(LayoutElement) == 0xd0,
              "UI::LayoutElement is 0xd0 bytes (MOV EDX,0xd0 in its slot-0 "
              "vector deleting destructor 0x180043570)");
static_assert(alignof(LayoutElement) == 8, "UI::LayoutElement aligns to 8");
static_assert(offsetof(LayoutElement, _childRecords) == sizeof(Element), "");
static_assert(offsetof(LayoutElement, _childRecords) == 0xc0, "");

}  // namespace UI
