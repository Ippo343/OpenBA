// ============================================================================
// runtime/types/ui/GroupDescription.h — UI::GroupDescription, 0x78 bytes
// ============================================================================
//
// `UI::GroupDescription` is `UI::ElementDescription` plus ONE member: the array
// of child `ExternalPtr<ElementDescription>` a group lays out. It is the base
// of `UI::PageDescription` and `UI::RadioDescription` (both "GroupDescription"
// by their `_ParentTypeName` literals), so the offsets below are read by more
// than this one class.
//
// EVERY NUMBER HERE IS READ OUT OF THE SHIPPED RETAIL BYTES, three ways that
// do not depend on each other:
//
//   sizeof 0x78   `MOV EDX,0x78` at 0x180046b0f, the eh_vector_destructor
//                 iterator stride in the deleting destructor 0x180046af0.
//                 Corroborated by the factory's Allocate(0x78, 8) in
//                 runtime/src/ui/factory_ui1_tables.inc's kGroupDescription
//                 row, whose last store is +0x70.
//   +0x68 _children  `ADD RCX,0x68` before the `Array::~Array` call at
//                 0x180117c20, and `LEA RCX,[RDI+0x68]` before
//                 `Array<ExternalPtr<ElementDescription>>::SetSize`
//                 (0x1800492d0) at 0x180117d7b.
//   +0x70 _size   `MOV EAX,[RDI+0x70]` at 0x180117d4f (Serialize reads the
//                 count) and `CMP dword [RSI+0x70],0` at 0x180117e61
//                 (CreateInstance's loop bound).
//
// +0x74 is the capacity, by Collection::Array's layout
// ({_data +0x00, _size +0x08, _capacity +0x0c}); no body in this class names
// it, and it is NOT invented here.
//
// ############################################################################
// ENDGAME B.3 step 6 (block C2) — `UI::GroupDescription`, the REAL C++ class
// ############################################################################
//
// PARENT `UI::ElementDescription` — READ, not inferred:
//     ?_ParentTypeName@GroupDescription@UI@@CAPEBDXZ  @0x180046630 -> "ElementDescription"
// RULE B5-15 / B6-7 — the two literals, RECORDED, NOT USED (slots 1/2 keep
// reading the minted `?_typeInfo@GroupDescription@UI@@…`):
//     ?_TypeName@GroupDescription@UI@@CAPEBDXZ        @0x180046620 -> "GroupDescription"
//     ?_ParentTypeName@GroupDescription@UI@@CAPEBDXZ  @0x180046630 -> "ElementDescription"
//
// `??_7GroupDescription@UI@@6B@` @ 0x180157190, TEN slots (B36_SLOTS.md
// section 1):
//   [0] 0x180046af0  (unexported) vector deleting dtor — RT_DELDTOR row, kept
//   [1] 0x180046660  TypeName
//   [2] 0x180046640  Type
//   [3] 0x180117cf0  GetFileVersion  (`MOV EAX,0x104`)
//   [4] 0x1800066e0  Object::External::Compile  INHERITED — NOT declared (B6-1.5)
//   [5] 0x180117d00  Serialize
//   [6] 0x1800468e0  Reconstruct
//   [7] 0x180046a20  Destruct
//   [8] 0x180117dc0  CreateInstance(const Dialog*) -> InternalPtr<Element>  RETSLOT (B6-2)
//   [9] 0x180046980  Construct — INTRODUCED here (`virtual`, B6-1.1)
//
// ABSTRACT (slot 0 `= 0`). NO REAL CTOR: retail exports no out-of-line
// `??0GroupDescription` at all — `Construct`, `Reconstruct` and the two factory
// steps each inline the same 19 stores. Every live one wears the MINTED table,
// so every door in groupdescription.cpp delegates with a QUALIFIED call.
//
// THE POD IS GONE. `RtUI::GroupDescription` had ONE user (groupdescription.cpp)
// and nothing embedded it, so the class takes the layout outright (B6-4.1);
// the two vtable-name constants stay in `RtUI::`.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "ElementDescription.h"
#include "../collection/Collection.h"

namespace RtUI {

// The two vtables the class's bodies name. Both exported.
constexpr const char* kVtblGroupDescription = "??_7GroupDescription@UI@@6B@";   // 0x180157190
constexpr const char* kVtblGroupElement     = "??_7GroupElement@UI@@6B@";       // 0x180157130

}  // namespace RtUI

namespace UI {

class Dialog;
class Element;

class GroupDescription : public ElementDescription {
public:
    // ---- the ten slots, in measured order ---------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x180046af0
    const char* TypeName() const override;                        // [1] 0x180046660
    uint32_t    Type() const override;                            // [2] 0x180046640
    int32_t     GetFileVersion() const override;                  // [3] 0x180117cf0
    // [4] Compile — Object::External's, inherited.
    bool        Serialize(void* stream) override;                 // [5] 0x180117d00  (IO::Stream&)
    Object::External* Reconstruct() override;                     // [6] 0x1800468e0
    void        Destruct() override;                              // [7] 0x180046a20
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                     // [8] 0x180117dc0  RETSLOT
    virtual Object::External* Construct();                        // [9] 0x180046980  INTRODUCED

    // ---- exported, NOT PORTED as members ----------------------------------
    GroupDescription(const GroupDescription& src);                // 0x180046a30  not ported
    GroupDescription& operator=(const GroupDescription& src);     // 0x180046a90  not ported
    static uint32_t TypeId();                                     // 0x180046650  not ported
    static Object::Base* CreateNewObject();                       // 0x180046750  factory_ui1.cpp door
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180046840  factory_ui1.cpp door

protected:
    // NOT a retail ctor (retail has none out of line — see the banner).
    GroupDescription() = default;
    // `??1GroupDescription@UI@@UEAA@XZ` (0x180117c00). NON-VIRTUAL, NOT defined
    // in the real build (the base's is not either); the retail body is the
    // door `RtUi_GroupDescription_Dtor`.
    ~GroupDescription();

private:
    static const char* _TypeName();                               // 0x180046620 -> "GroupDescription"
    static const char* _ParentTypeName();                         // 0x180046630 -> "ElementDescription"
    static Object::BaseTypeInfo _typeInfo;                        // minted

public:
    // ---- the 0x10 bytes after ElementDescription --------------------------
    // Array<ExternalPtr<ElementDescription>>; every element is one raw pointer
    // (External.h), so `void*` is the same bytes.
    Collection::Array<void*> _children;        // +0x68 .. +0x77
};

static_assert(sizeof(GroupDescription) == 0x78,
              "sizeof(UI::GroupDescription) == 0x78 (MOV EDX,0x78 at 0x180046b0f)");
static_assert(alignof(GroupDescription) == 8, "UI::GroupDescription aligns to 8");
static_assert(!std::has_virtual_destructor<GroupDescription>::value, "ENDGAME decision 2");
static_assert(offsetof(GroupDescription, _children) == sizeof(ElementDescription),
              "GroupDescription's own field begins at the end of ElementDescription");
static_assert(offsetof(GroupDescription, _children) == 0x68,
              "GroupDescription::_children @ +0x68 (ADD RCX,0x68 at 0x180117c20)");
static_assert(offsetof(GroupDescription, _children) + offsetof(Collection::Array<void*>, _size) == 0x70,
              "GroupDescription::_children._size @ +0x70 (CMP [RSI+0x70] at 0x180117e61)");

}  // namespace UI
