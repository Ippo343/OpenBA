#pragma once
// ============================================================================
// runtime/types/ui/ClipRegionElement.h — UI::ClipRegionElement, the REAL class
// ============================================================================
//
// ENDGAME B.3 step 6, block C1. Layout MOVED here from the file-local
// constants of runtime/src/ui/clipregionelement.cpp (RULE B6-8).
//
// PARENT `UI::Element` — READ (B36_SLOTS.md section 3):
//     ?_ParentTypeName@ClipRegionElement@UI@@CAPEBDXZ  @0x180037f40 -> "Element"
// RULE B5-15 / B6-7 — RECORDED, NOT USED (`?_typeInfo@ClipRegionElement@UI@@…`
// 0x1801f9d68 stays the minted record):
//     ?_TypeName@ClipRegionElement@UI@@CAPEBDXZ        @0x180037f30 -> "ClipRegionElement"
//     ?_ParentTypeName@ClipRegionElement@UI@@CAPEBDXZ  @0x180037f40 -> "Element"
//
// `??_7ClipRegionElement@UI@@6B@` @ 0x1801562f0, TWELVE slots (sizeof from
// `MOV EDX,0xd8` in slot 0 0x180038370):
//
//   [ 0] 0x180038370  (unexported) vector deleting dtor — RT_DELDTOR row, kept
//   [ 1] 0x180037f70  TypeName
//   [ 2] 0x180037f50  Type
//   [ 3] 0x180116870  Element::Persist     INHERITED
//   [ 4] 0x1800cbc60  ComputeSize
//   [ 5] 0x1800cba00  ComputeLayout
//   [ 6] 0x1800cb900  Draw
//   [ 7] 0x1800cbd70  Update
//   [ 8] 0x180116910  Element::Enable      INHERITED
//   [ 9] 0x1800cbcc0  Contains             (the one leaf in this block that overrides 9)
//   [10] 0x180035620  Element::SetColor    INHERITED
//   [11] 0x180035630  Element::GetBorder   INHERITED (RETSLOT)
//
// ABSTRACT, no real ctor: live ones are built by the `??0ClipRegionElement`
// door (0x1800cbe60), by ClipRegionDescription::CreateInstance's inlined
// copy, and by ContainerDescription::CreateInstance (block C4's) — all wearing
// the MINTED table.
//
// ---------------------------------------------------------------------------
// LAYOUT — 0xd8 bytes, six dwords past UI::Element
// ---------------------------------------------------------------------------
// `??0ClipRegionElement@UI@@QEAA@XZ` (0x1800cbe60) and its two inlined copies
// write ALL six, with overlapping stores (kClipRegionElement rows in
// runtime/src/ui/factory_ui1_tables.inc, and ClipRegionDescription::
// CreateInstance 0x1800cbf42..0x1800cbf60):
//     MOV dword [+0xc0],0x64 ; MOV qword [+0xc4],0x64 (+0xc4 = 100, +0xc8 = 0)
//     MOV qword [+0xcc],0    ; MOV dword [+0xd4],0
// +0xc8/+0xcc are what `UI::ContainerElement` reads back to size its scroll
// bars; +0xd0/+0xd4 are what `?SetOffsets@ContainerElement@UI@@` (0x1800cc1e0)
// writes. The names are the ported bodies' (clipregionelement.cpp).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "Element.h"

namespace UI {

class ClipRegionElement : public Element {
public:
    // ---- the twelve slots, in measured order ------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x180038370
    const char* TypeName() const override;                        // [1] 0x180037f70
    uint32_t    Type() const override;                            // [2] 0x180037f50
    void ComputeSize() override;                                  // [4] 0x1800cbc60
    void ComputeLayout(int32_t x, int32_t y,
                       int32_t w, int32_t h) override;            // [5] 0x1800cba00
    void Draw(SpriteContext& context, uint8_t alpha) const override; // [6] 0x1800cb900
    void Update(const System::Time& time, UserInterface* ui,
                Object::InternalPtr<DialogInstance>* dialog,
                const UpdateState& state) override;               // [7] 0x1800cbd70
    bool Contains(int32_t x, int32_t y,
                  UserInterface* ui) const override;              // [9] 0x1800cbcc0

    // ---- exported, NOT PORTED anywhere in the tree: declared, deliberately
    // undefined (nothing in the tree calls them through a member).
    void    GetContentSize(int32_t& w, int32_t& h);               // 0x1800381e0
    int32_t GetHeight() const;                                    // 0x180038180
    int32_t GetWidth() const;                                     // 0x180038170
    void    SetOffset(int32_t x, int32_t y);                      // 0x1800381d0
    void    SetSize(int32_t w, int32_t h);                        // 0x180038190

    ClipRegionElement(const ClipRegionElement& src);              // 0x180038220  door (clipregionelement.cpp)
    ClipRegionElement& operator=(const ClipRegionElement& src);   // 0x1800382b0  not ported

    static uint32_t TypeId();                                     // 0x180037f60  not ported
    static Object::Base* CreateNewObject();                       // 0x180038060  factory_ui1.cpp door
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180038110  factory_ui1.cpp door

protected:
    // NOT retail's `??0ClipRegionElement@UI@@QEAA@XZ` (0x1800cbe60, the door
    // `RtUi_ClipRegionElement_Ctor`).
    ClipRegionElement() = default;
    // `??1ClipRegionElement@UI@@UEAA@XZ` (0x1800cbe30), declared only; the
    // retail body is the door `RtUi_ClipRegionElement_Dtor`.
    ~ClipRegionElement();

private:
    static const char* _TypeName();                               // 0x180037f30 -> "ClipRegionElement"
    static const char* _ParentTypeName();                         // 0x180037f40 -> "Element"
    static Object::BaseTypeInfo _typeInfo;                        // 0x1801f9d68 (minted)

public:
    int32_t _reqWidth;       // +0xc0  ctor 100. The viewport's own size: ComputeSize
    int32_t _reqHeight;      // +0xc4  ctor 100. copies both into _layoutWidth/Height.
    int32_t _innerWidth;     // +0xc8  ctor 0. The CHILD's padded size — an OUTPUT of
    int32_t _innerHeight;    // +0xcc  ctor 0. ComputeSize (GetPaddedSize of child 0).
    int32_t _offsetX;        // +0xd0  ctor 0. Scroll offset, subtracted from the
    int32_t _offsetY;        // +0xd4  ctor 0. child's layout origin.
};

static_assert(sizeof(ClipRegionElement) == 0xd8,
              "UI::ClipRegionElement is 0xd8 bytes (MOV EDX,0xd8 in 0x180038370)");
static_assert(alignof(ClipRegionElement) == 8, "UI::ClipRegionElement aligns to 8");
static_assert(!std::has_virtual_destructor<ClipRegionElement>::value, "ENDGAME decision 2");
static_assert(offsetof(ClipRegionElement, _reqWidth) == sizeof(Element),
              "ClipRegionElement's own fields begin at the end of UI::Element");
static_assert(offsetof(ClipRegionElement, _reqWidth)    == 0xc0, "");
static_assert(offsetof(ClipRegionElement, _reqHeight)   == 0xc4, "");
static_assert(offsetof(ClipRegionElement, _innerWidth)  == 0xc8, "");
static_assert(offsetof(ClipRegionElement, _innerHeight) == 0xcc, "");
static_assert(offsetof(ClipRegionElement, _offsetX)     == 0xd0, "");
static_assert(offsetof(ClipRegionElement, _offsetY)     == 0xd4, "");

}  // namespace UI
