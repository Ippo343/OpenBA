// ============================================================================
// runtime/types/ui/GroupDescription.h — UI::GroupDescription, 0x78 bytes
// ============================================================================
//
// `UI::GroupDescription` is `UI::ElementDescription` plus ONE member: the array
// of child `ExternalPtr<ElementDescription>` a group lays out. It is the base
// of `UI::PageDescription` and `UI::RadioDescription` (both named as its
// children in the minted BaseTypeInfo table), so the offsets below are read by
// more than this one class.
//
// EVERY NUMBER HERE IS READ OUT OF THE SHIPPED RETAIL BYTES, three ways that
// do not depend on each other:
//
//   sizeof 0x78   `MOV EDX,0x78` at 0x180046b0f, the eh_vector_destructor
//                 iterator stride in the deleting destructor 0x180046af0.
//                 Corroborated by `MOV EDX,0xc0` NOT appearing here (that is
//                 GroupElement's) and by the factory's Allocate(0x78, 8)
//                 in runtime/src/ui/factory_ui1_tables.inc's kGroupDescription
//                 row, whose last store is +0x70.
//   +0x68 _data   `ADD RCX,0x68` before the `Array::~Array` call at
//                 0x180117c20, and `LEA RCX,[RDI+0x68]` before
//                 `Array<ExternalPtr<ElementDescription>>::SetSize`
//                 (0x1800492d0) at 0x180117d7b.
//   +0x70 _size   `MOV EAX,[RDI+0x70]` at 0x180117d4f (Serialize reads the
//                 count) and `CMP dword [RSI+0x70],0` at 0x180117e61
//                 (CreateInstance's loop bound).
//
// +0x74 is the capacity, by Collection::Array's layout
// ({_data +0x00, _size +0x08, _capacity +0x0c}); no body in this class names
// it, and it is NOT invented here — it is the third field of the container
// header runtime/types/collection/Collection.h pins.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>

#include "ElementDescription.h"
#include "../collection/Collection.h"

namespace RtUI {

struct GroupDescription {
    ElementDescription _base;                  // +0x00 .. +0x67
    Collection::Array<void*> _children;        // +0x68 Array<ExternalPtr<ElementDescription>>
};

static_assert(offsetof(GroupDescription, _children) == 0x68,
              "GroupDescription::_children @ +0x68 (ADD RCX,0x68 at 0x180117c20)");
static_assert(offsetof(GroupDescription, _children._size) == 0x70,
              "GroupDescription::_children._size @ +0x70 (CMP [RSI+0x70] at 0x180117e61)");
static_assert(sizeof(GroupDescription) == 0x78,
              "sizeof(UI::GroupDescription) == 0x78 (MOV EDX,0x78 at 0x180046b0f)");

// The four vtables the constructor stamps, in the order it stamps them. All
// four are exported, so a port resolves them by name and the mint supplies them
// at cutover.
constexpr const char* kVtblGroupDescription = "??_7GroupDescription@UI@@6B@";   // 0x180157190
constexpr const char* kVtblGroupElement     = "??_7GroupElement@UI@@6B@";       // 0x180157130

}  // namespace RtUI
