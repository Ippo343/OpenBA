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
//   [ 0] 0x180046e10  (unexported) vector deleting dtor — RT_DELDTOR row, kept
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
// ABSTRACT: slot 0 `= 0`; no real ctor (the `??0GroupElement` door in
// groupelement.cpp and the inlined copies in the three Descriptions'
// CreateInstance build every live one, wearing the MINTED table).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "Element.h"

namespace UI {

class GroupElement : public Element {
public:
    // ---- the twelve slots, in measured order ------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x180046e10
    const char* TypeName() const override;                        // [1] 0x180046bb0
    uint32_t    Type() const override;                            // [2] 0x180046b90
    // [3] Persist — Element's, inherited.
    void ComputeSize() override;                                  // [4] 0x180117f00
    void ComputeLayout(int32_t x, int32_t y,
                       int32_t w, int32_t h) override;            // [5] 0x180118050
    void Draw(SpriteContext& context, uint8_t alpha) const override; // [6] 0x1801181c0
    // [7..11] Element's, inherited.

    // ---- exported, NOT PORTED as members ----------------------------------
    GroupElement(const GroupElement& src);                        // 0x180046d70  door (groupelement.cpp)
    GroupElement& operator=(const GroupElement& src);             // 0x180046da0  not ported
    static uint32_t TypeId();                                     // 0x180046ba0  not ported
    static Object::Base* CreateNewObject();                       // 0x180046ca0  factory door
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180046d30  factory door

protected:
    // NOT retail's `??0GroupElement@UI@@QEAA@XZ` (0x180117c80, the door
    // `RtUi_GroupElement_Ctor`).
    GroupElement() = default;
    // `??1GroupElement@UI@@UEAA@XZ` (0x180117cc0). NON-VIRTUAL, declared and
    // NOT defined in the real build; the retail body is the door
    // `RtUi_GroupElement_Dtor`.
    ~GroupElement();

private:
    static const char* _TypeName();                               // 0x180046b70 -> "GroupElement"
    static const char* _ParentTypeName();                         // 0x180046b80 -> "Element"
    static Object::BaseTypeInfo _typeInfo;                        // minted
};

static_assert(sizeof(GroupElement) == 0xc0,
              "UI::GroupElement is 0xc0 bytes (MOV EDX,0xc0 at 0x180046e42) — "
              "it adds nothing to UI::Element");
static_assert(alignof(GroupElement) == 8, "UI::GroupElement aligns to 8 (the vptr)");
static_assert(!std::has_virtual_destructor<GroupElement>::value, "ENDGAME decision 2");

}  // namespace UI
