#pragma once
// ============================================================================
// runtime/types/ui/GroupElement.h — UI::GroupElement, the REAL C++ class
// ============================================================================
//
// ENDGAME B.3 step 6, block C2. The plain stacking container: no fields of its
// own, sized to the union of its visible children, every child given the WHOLE
// group rectangle.
//
// PARENT `UI::Element` — READ, not inferred (B36_SLOTS.md section 3):
//     ?_ParentTypeName@GroupElement@UI@@CAPEBDXZ  @0x180046b80 -> "Element"
// RULE B5-15 / B6-7 — the two literals, RECORDED, NOT USED:
//     ?_TypeName@GroupElement@UI@@CAPEBDXZ        @0x180046b70 -> "GroupElement"
//     ?_ParentTypeName@GroupElement@UI@@CAPEBDXZ  @0x180046b80 -> "Element"
//
// `??_7GroupElement@UI@@6B@` @ 0x180157130, TWELVE slots:
//   [ 0] 0x180046e10  (unexported) vector deleting dtor — the class's own (B.3.9)
//   [ 1] 0x180046bb0  TypeName
//   [ 2] 0x180046b90  Type
//   [ 3] 0x180116870  Element::Persist        INHERITED
//   [ 4] 0x180117f00  ComputeSize             (also Page's and Radio's slot 4)
//   [ 5] 0x180118050  ComputeLayout           (also Page's and Radio's slot 5)
//   [ 6] 0x1801181c0  Draw                    (also Page's and Radio's slot 6)
//   [ 7] 0x180116270  Element::Update         INHERITED (Radio overrides it)
//   [ 8..11]          Element's               INHERITED
//
// TWO SUBCLASSES (RULE B6-9): `PageElement` and `RadioElement`, both
// "GroupElement". Page inherits ALL of 4..11; Radio overrides only Update.
//
// LAYOUT: 0xc0 bytes — `MOV EDX,0xc0` at 0x180046e42, exactly
// `sizeof(UI::Element)`. `??0GroupElement@UI@@QEAA@XZ` (0x180117c80) is
// `??0Element` plus one vtable stamp and nothing else. The class adds NO data.
//
// (Until ENDGAME B.3.9:) ABSTRACT: slot 0 `= 0`; no real ctor (the `??0GroupElement` door in
// groupelement.cpp and the inlined copies in the three Descriptions'
// CreateInstance build every live one, wearing the MINTED table).
//
// ENDGAME B.3 step 9 (block UI-A, B39_RULES D1/D2): CONCRETE. The real public
// ctor is retail's `??0GroupElement@UI@@QEAA@XZ` (0x180117c80) minus its vptr stores;
// slot 0 is the class's own vector deleting dtor; ~GroupElement is protected and
// non-virtual. The factory pair is the class's own; the minted
// `??_7GroupElement@UI@@6B@`, its RT_DELDTOR row, the factory-interpreter row and
// the doors are retired.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "Element.h"
#include "../object/Internal.h"   // Object::detail::InternalMemFree

namespace UI {

class GroupElement : public Element {
public:
    // ??0GroupElement@UI@@QEAA@XZ 0x180117c80 — the REAL constructor: CALL
    // ??0Element (Element()) and the stamp. The class adds no data.
    GroupElement() : Element() {}

    // ---- the twelve slots, in measured order ------------------------------
    // [0] 0x180046e10 — MSVC's VECTOR deleting destructor, both arms exactly as
    // the retired RT_DELDTOR row ran them (`MOV EDX,0xc0`, the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<GroupElement*>(self + static_cast<int64_t>(i) * 0xc0)
                    ->~GroupElement();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~GroupElement();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    // ENDGAME B.3.9 TI-UI: ONE reflection block (B39 D6); `_typeInfo` is the
    // class's own record (DEFINE_TYPE in the class's TU), no longer minted.
    // retail: TypeName 0x180046bb0, Type 0x180046b90, TypeId 0x180046ba0, _TypeName 0x180046b70
    //         _ParentTypeName 0x180046b80, CreateNewObject 0x180046ca0, CreateInPlaceObject 0x180046d30
    DECLARE_TYPE(GroupElement)
public:
    // [3] Persist — Element's, inherited.
    void ComputeSize() override;                                  // [4] 0x180117f00
    void ComputeLayout(int32_t x, int32_t y,
                       int32_t w, int32_t h) override;            // [5] 0x180118050
    void Draw(SpriteContext& context, uint8_t alpha) const override; // [6] 0x1801181c0
    // [7..11] Element's, inherited.

    // ---- exported, NOT PORTED as members ----------------------------------
    GroupElement(const GroupElement& src);                        // 0x180046d70  not ported (door retired: no caller)
    GroupElement& operator=(const GroupElement& src);             // 0x180046da0  not ported

protected:
    // `??1GroupElement@UI@@UEAA@XZ` (0x180117cc0). NON-VIRTUAL and EMPTY
    // (groupelement.cpp): restamp, then ~Element, which the compiler chains.
    ~GroupElement();

private:
};

static_assert(sizeof(GroupElement) == 0xc0,
              "UI::GroupElement is 0xc0 bytes (MOV EDX,0xc0 at 0x180046e42) — "
              "it adds nothing to UI::Element");
static_assert(alignof(GroupElement) == 8, "UI::GroupElement aligns to 8 (the vptr)");
static_assert(!std::has_virtual_destructor<GroupElement>::value, "ENDGAME decision 2");
static_assert(!std::is_abstract<GroupElement>::value, "B39 D1: concrete");

}  // namespace UI
