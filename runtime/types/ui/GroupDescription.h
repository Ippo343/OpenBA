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
//   [0] 0x180046af0  (unexported) vector deleting dtor — the class's own (B.3.9)
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
// (Until ENDGAME B.3.9:) ABSTRACT (slot 0 `= 0`). NO REAL CTOR: retail exports no out-of-line
// `??0GroupDescription` at all — `Construct`, `Reconstruct` and the two factory
// steps each inline the same 19 stores. Every live one wears the MINTED table,
// so every door in groupdescription.cpp delegates with a QUALIFIED call.
//
// THE POD IS GONE. `RtUI::GroupDescription` had ONE user (groupdescription.cpp)
// and nothing embedded it, so the class takes the layout outright (B6-4.1);
// the two vtable-name constants stayed in `RtUI::` until B.3.9.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
//
// ENDGAME B.3 step 9 (block UI-A, B39_RULES D1/D2): CONCRETE. The real public
// ctor is the inlined store sequence (retail has no out-of-line ??0) minus its
// vptr stores; slot 0 is the class's own vector deleting dtor;
// ~GroupDescription is protected and non-virtual. The factory pair is the
// class's own; the minted `??_7GroupDescription@UI@@6B@`, its RT_DELDTOR row,
// the factory-interpreter row and the doors are retired, and with them the
// two `RtUI::kVtbl*` names this header carried.
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "ElementDescription.h"
#include "../collection/Collection.h"
#include "../object/Internal.h"   // Object::detail::InternalMemFree

namespace UI {

class Dialog;
class Element;

class GroupDescription : public ElementDescription {
public:
    // The REAL constructor. Retail exports no out-of-line ??0GroupDescription:
    // Construct, Reconstruct and both factory steps each INLINE the same
    // nineteen stores (0x180046996..0x180046a06) -- ElementDescription()'s,
    // then the child array's two QWORD zeros (Collection::Array's own default
    // ctor). Nothing else.
    GroupDescription() : ElementDescription() {}

    // ---- the ten slots, in measured order ---------------------------------
    // [0] 0x180046af0 — MSVC's VECTOR deleting destructor, both arms exactly as
    // the retired RT_DELDTOR row ran them (`MOV EDX,0x78`, the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<GroupDescription*>(self + static_cast<int64_t>(i) * 0x78)
                    ->~GroupDescription();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~GroupDescription();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    // ENDGAME B.3.9 TI-UI: ONE reflection block (B39 D6); `_typeInfo` is the
    // class's own record (DEFINE_TYPE in the class's TU), no longer minted.
    // retail: TypeName 0x180046660, Type 0x180046640, TypeId 0x180046650, _TypeName 0x180046620
    //         _ParentTypeName 0x180046630, CreateNewObject 0x180046750, CreateInPlaceObject 0x180046840
    DECLARE_TYPE(GroupDescription)
public:
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

protected:
    // `??1GroupDescription@UI@@UEAA@XZ` (0x180117c00). NON-VIRTUAL and EMPTY
    // (groupdescription.cpp): the child array's J1 ~Array (retail 0x180039440)
    // is the compiler's member ~Array; ~ElementDescription (+0x60) and
    // ~External are the compiler's chain.
    ~GroupDescription();

private:

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
static_assert(!std::is_abstract<GroupDescription>::value, "B39 D1: concrete");
static_assert(offsetof(GroupDescription, _children) == sizeof(ElementDescription),
              "GroupDescription's own field begins at the end of ElementDescription");
static_assert(offsetof(GroupDescription, _children) == 0x68,
              "GroupDescription::_children @ +0x68 (ADD RCX,0x68 at 0x180117c20)");
static_assert(offsetof(GroupDescription, _children) + offsetof(Collection::Array<void*>, _size) == 0x70,
              "GroupDescription::_children._size @ +0x70 (CMP [RSI+0x70] at 0x180117e61)");

}  // namespace UI
