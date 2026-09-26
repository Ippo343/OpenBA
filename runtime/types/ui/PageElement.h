#pragma once
// ============================================================================
// runtime/types/ui/PageElement.h — UI::PageElement
// ============================================================================
//
// Two things live here:
//   * `UI::PageElementOffsets` — Wave 7 lane E's offsets-only namespace, which
//     runtime/src/ui/widgets_layout.cpp (GetActivePage / SetActivePage, a
//     fragment-pass file) still reads. Unchanged.
//   * `UI::PageElement` — the REAL C++ class (ENDGAME B.3 step 6, block C2).
//
// Evidence for the offsets (Runtime-x64.dll addresses):
//   GetActivePage()    0x1800d5100  return *(int*)(this+0xc0)
//   SetActivePage(i)   0x1800d5110  guard: -2<i<*(int*)(this+0xa8) (the count
//                                   of Element::_children) AND i != current.
//                                   Sets this+0xc0=i, dirty-chain walk on
//                                   `this`, then for every child: SET bit 0 of
//                                   the child's flags (+0x34) for every index
//                                   != the new page, CLEAR it for the one that
//                                   is, and dirty-chain-walk that child
//                                   whenever bit 0 actually flipped. Bit 0 of a
//                                   child's flags is `kElementFlagHidden`.
//
// ---------------------------------------------------------------------------
// THE CLASS
// ---------------------------------------------------------------------------
// PARENT `UI::GroupElement` — READ, not inferred (B36_SLOTS.md section 3):
//     ?_ParentTypeName@PageElement@UI@@CAPEBDXZ  @0x180047240 -> "GroupElement"
// RULE B5-15 / B6-7 — the two literals, RECORDED, NOT USED:
//     ?_TypeName@PageElement@UI@@CAPEBDXZ        @0x180047230 -> "PageElement"
//     ?_ParentTypeName@PageElement@UI@@CAPEBDXZ  @0x180047240 -> "GroupElement"
//
// `??_7PageElement@UI@@6B@` @ 0x180157080, TWELVE slots. Only 0/1/2 are its
// own; 4/5/6 are GroupElement's (0x180117f00 / 0x180118050 / 0x1801181c0) and
// 3, 7..11 are Element's — Page inherits ALL of GroupElement's 4..11:
//   [0] 0x180047540  (unexported) vector deleting dtor — RT_DELDTOR row, kept
//   [1] 0x180047270  TypeName
//   [2] 0x180047250  Type
//
// LAYOUT — 0xc8 bytes (`MOV EDX,0xc8` in the deleting dtor 0x180047540; the
// same 0xc8 PageDescription::CreateInstance allocates at 0x1800d52f8).
// `??0PageElement@UI@@QEAA@XZ` (0x1800d5260) is ??0Element, the GroupElement
// stamp, the PageElement stamp and ONE `MOV dword [RBX+0xc0],0`, so:
//   +0xc0  _activePage  ctor 0; `?GetActivePage@` / `?SetActivePage@` name it
//   +0xc4  _padC4       written by nothing (alignment tail)
//
// ABSTRACT (slot 0 `= 0`); no real ctor — the `??0PageElement` door in
// pageelement.cpp and PageDescription::CreateInstance's inlined copy build
// every live one, wearing the MINTED table.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "Element.h"
#include "GroupElement.h"

namespace UI { namespace PageElementOffsets {

constexpr int kActivePage    = 0xC0;  // int32_t -- PageElement's own field
constexpr int kChildrenData  = offsetof(UI::Element, _children);       // confirmed by Element.h (lane A)
constexpr int kChildrenCount = kChildrenData + 8;                       // Collection::Array<T>::_size
constexpr int kDirtyFlags    = offsetof(UI::Element, _flags);           // 0x34; bit 0 here means "not the active page" (UI::kElementFlagHidden)
constexpr int kParent        = offsetof(UI::Element, _parent);          // 0x98
constexpr uint32_t kDirtyBit  = UI::kElementFlagNeedsSizeCalc;
constexpr uint32_t kHiddenBit = UI::kElementFlagHidden;

} } // namespace UI::PageElementOffsets

namespace UI {

class PageElement : public GroupElement {
public:
    // ---- the slots it owns ------------------------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x180047540
    const char* TypeName() const override;                        // [1] 0x180047270
    uint32_t    Type() const override;                            // [2] 0x180047250
    // [3..11] GroupElement's / Element's, inherited.

    // ---- non-virtual ------------------------------------------------------
    // 0x1800d5100 — `MOV EAX,[RCX+0xc0] ; RET`.
    int32_t GetActivePage() const { return _activePage; }
    // 0x1800d5110 — PORTED, but the body is the registered door
    // `PageElement_SetActivePage` in widgets_layout.cpp (fragment pass). Declared
    // here so no later block edits this header; undefined until then.
    void SetActivePage(int32_t page);

    // ---- exported, NOT PORTED as members ----------------------------------
    PageElement(const PageElement& src);                          // 0x180047450  not ported
    PageElement& operator=(const PageElement& src);               // 0x1800474b0  not ported
    static uint32_t TypeId();                                     // 0x180047260  not ported
    static Object::Base* CreateNewObject();                       // 0x180047360  factory door
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180047400  factory door

protected:
    // NOT retail's `??0PageElement@UI@@QEAA@XZ` (0x1800d5260, a door).
    PageElement() = default;
    // `??1PageElement@UI@@UEAA@XZ` (0x1800d5220). NON-VIRTUAL, NOT defined in
    // the real build; the retail body is the door `RtUi_PageElement_Dtor`.
    ~PageElement();

private:
    static const char* _TypeName();                               // 0x180047230 -> "PageElement"
    static const char* _ParentTypeName();                         // 0x180047240 -> "GroupElement"
    static Object::BaseTypeInfo _typeInfo;                        // minted

public:
    int32_t  _activePage;     // +0xc0  ctor 0 (0x1800d5260)
    uint32_t _padC4;          // +0xc4  written by nothing
};

static_assert(sizeof(PageElement) == 0xc8,
              "UI::PageElement is 0xc8 bytes (MOV EDX,0xc8 in 0x180047540)");
static_assert(alignof(PageElement) == 8, "UI::PageElement aligns to 8");
static_assert(!std::has_virtual_destructor<PageElement>::value, "ENDGAME decision 2");
static_assert(offsetof(PageElement, _activePage) == sizeof(GroupElement),
              "PageElement's own field begins at the end of GroupElement");
static_assert(offsetof(PageElement, _activePage) == PageElementOffsets::kActivePage, "");

}  // namespace UI
