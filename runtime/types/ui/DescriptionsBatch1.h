#pragma once
// ============================================================================
// runtime/types/ui/DescriptionsBatch1.h — the ten UI::*Description layouts
// ported by runtime/src/ui/{border,button,check,clipregion,combo,container,
// documenttext,edit,graph,label}description.cpp.
// ============================================================================
//
// One header rather than ten, because these are ten members of ONE family:
// every one of them is `UI::ElementDescription` (0x68) plus a short tail, and
// the only thing that varies is that tail. Splitting the family across ten
// two-field headers would have repeated the same base assert ten times and
// hidden the one fact worth seeing — that the tails are all that differ.
//
// EVERY NUMBER BELOW IS READ OUT OF THE SHIPPED RETAIL Runtime-x64.dll, and
// each carries the instruction address it came from. Two independent sources
// agree on each `sizeof`:
//
//   (a) `MOV EDX,<size>` in the class's vector deleting destructor, the
//       eh_vector_destructor_iterator stride;
//   (b) the `Allocate(<size>, 8)` in `?CreateNewObject@<Class>@UI@@`, already
//       transcribed in runtime/src/ui/factory_ui1_tables.inc (and, for
//       LabelDescription, runtime/src/ui/ui_factory_wave28.cpp).
//
//   class                      deldtor            (a)    factory row  (b)
//   BorderDescription          0x180040ff0 +0x1f  0xb8   0x180040ad0  0xb8
//   ButtonDescription          0x1800481a0 +0x1f  0xa0   0x180047f20  0xa0
//   CheckDescription           0x1800489c0 +0x1f  0xa8   0x180048770  0xa8
//   ClipRegionDescription      0x180037eb0 +0x1f  0x78   0x180037a80  0x78
//   ComboDescription           0x180074e70 +0x1f  0xb8   0x180074bd0  0xb8
//   ContainerDescription       0x180039c00 +0x1f  0x90   0x1800399c0  0x90
//   DocumentTextDescription    0x18003bd90 +0x1f  0xe8   0x18003b9f0  0xe8
//   EditDescription            0x18003fa20 +0x1f  0x88   0x18003f7c0  0x88
//   GraphDescription           0x1800403a0 +0x1f  0x88   0x180040180  0x88
//   LabelDescription           0x180042730 +0x1f  0xb8   0x1800424a0  0xb8
//
// FIELD NAMES ARE DESCRIPTIVE, NOT RECOVERED. Retail exports no member names
// for these classes; what IS recovered per field is its WIDTH and the
// serializer or applier that touches it, and that is what the comment on each
// line records. Where a body reads a field through an offset the struct does
// not name (retail's overlapping and unaligned stores), the .cpp says so at
// the store; the struct is the alignment/size assertion, not a claim that
// every access goes through it.
//
// LabelDescription is the one that is NOT a direct ElementDescription: it
// derives from `UI::TextDescription` (its Serialize calls 0x18011ee30 and its
// dtor tail-jumps to 0x18011eae0, both TextDescription's). TextDescription is
// another agent's class, so the base is spelled here as an opaque 0xa8-byte
// prefix with only the fields LabelDescription's own bodies touch named.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "ElementDescription.h"

namespace RtUI {

// ---------------------------------------------------------------------------
// UI::BorderDescription — 0xb8
// ---------------------------------------------------------------------------
// One ExternalPtr<SpriteSheet> and NINE System::String sprite names: the nine
// pieces of a nine-slice border. Serialize (0x18010fe20) runs 0x180004590 over
// +0x68 and 0x1800c9930 over +0x70 .. +0xb0 in ascending order;
// ~BorderDescription (0x18010fb20) releases the nine in DESCENDING order.
// RtUI::BorderDescription is now an ALIAS of the real class UI::BorderDescription (BorderDescription.h);
// see the alias block at the bottom of this file.

// ---------------------------------------------------------------------------
// UI::ButtonDescription — 0xa0
// ---------------------------------------------------------------------------
// Five ExternalPtr<ElementDescription> child descriptions (the five visual
// states a button owns) and four ints. Serialize (0x1801113e0) runs
// 0x180038f90 over +0x68 .. +0x88 and the kScalarUInt slot (+0x98) over the
// four dwords at +0x90 .. +0x9c.
// RtUI::ButtonDescription is now an ALIAS of the real class UI::ButtonDescription (ButtonDescription.h);
// see the alias block at the bottom of this file.

// ---------------------------------------------------------------------------
// UI::CheckDescription — 0xa8 — ButtonDescription + one child description
// ---------------------------------------------------------------------------
// RtUI::CheckDescription is now an ALIAS of the real class UI::CheckDescription (CheckDescription.h);
// see the alias block at the bottom of this file.

// ---------------------------------------------------------------------------
// UI::ClipRegionDescription — 0x78
// ---------------------------------------------------------------------------
// Two ints that both start at 0x64 (`MOV dword [RCX+0x68],0x64` /
// `[RCX+0x6c],0x64` at 0x180037d66/0x180037d6d) and one child description.
// RtUI::ClipRegionDescription is now an ALIAS of the real class UI::ClipRegionDescription (ClipRegionDescription.h);
// see the alias block at the bottom of this file.

// ---------------------------------------------------------------------------
// UI::ComboDescription — 0xb8 — ButtonDescription + two child descs + a table
// ---------------------------------------------------------------------------
// RtUI::ComboDescription is now an ALIAS of the real class UI::ComboDescription (ComboDescription.h);
// see the alias block at the bottom of this file.

// ---------------------------------------------------------------------------
// UI::ContainerDescription — 0x90
// ---------------------------------------------------------------------------
// TWO of the fields are single BYTES (`CALL [RAX+0x78]`, the kScalarBool slot,
// at 0x1800ccf9d and 0x1800ccfaf, over +0x70 and +0x71) and the applier copies
// them with MOVZX/MOV byte at 0x1800ccdb4/0x1800ccdbf. +0x72/+0x73 are never
// named by any body.
// RtUI::ContainerDescription is now an ALIAS of the real class UI::ContainerDescription (ContainerDescription.h);
// see the alias block at the bottom of this file.

// ---------------------------------------------------------------------------
// UI::DocumentTextDescription — 0xe8
// ---------------------------------------------------------------------------
// The widest of the ten. Four separate arrays, each with its OWN
// Array<T>::SetSize and Array<T>::Serialize pair, plus a fifth container at
// +0xe0 serialized whole by 0x18003dc90.
struct DocumentTextDescription {
    ElementDescription _base;      // +0x00
    void*   _stringTable;          // +0x68  0x1800042a0 @ 0x1800cf8f7
    char*   _textName;             // +0x70  0x1800c9930 @ 0x1800cf904
    void*   _spriteSheet;          // +0x78  0x180004590 @ 0x1800cf911
    char*   _spriteName;           // +0x80  0x1800c9930 @ 0x1800cf967
    void*   _names;                // +0x88  Array<System::String> data
    int32_t _namesSize;            // +0x90  kScalarInt @ 0x1800cf91e
    int32_t _namesCapacity;        // +0x94
    uint32_t _word98;              // +0x98  kScalarUInt @ 0x1800cf97e
    uint32_t _word9c;              // +0x9c  kScalarUInt @ 0x1800cf995
    uint32_t _worda0;              // +0xa0  kScalarUInt @ 0x1800cf9ac
    int32_t  _worda4;              // +0xa4  kScalarInt  @ 0x1800cf9c3
    int32_t  _worda8;              // +0xa8  kScalarInt  @ 0x1800cf9da
    int32_t  _padac;               // +0xac
    void*   _arrayB0;              // +0xb0  SetSize 0x18003e600 / ser 0x18003e710
    int32_t _arrayB0Size;          // +0xb8  kScalarInt @ 0x1800cf9ea
    int32_t _arrayB0Capacity;      // +0xbc
    void*   _arrayC0;              // +0xc0  SetSize 0x18003ea50 / ser 0x18003eb60
    int32_t _arrayC0Size;          // +0xc8  kScalarInt @ 0x1800cfa33
    int32_t _arrayC0Capacity;      // +0xcc
    int32_t* _arrayD0;             // +0xd0  SetSize 0x18003eea0, ints serialized inline
    int32_t _arrayD0Size;          // +0xd8  kScalarInt @ 0x1800cfa7c
    int32_t _arrayD0Capacity;      // +0xdc
    void*   _containerE0;          // +0xe0  0x18003dc90 @ 0x1800cfade
};
static_assert(offsetof(DocumentTextDescription, _names)       == 0x88, "DocText+0x88");
static_assert(offsetof(DocumentTextDescription, _namesSize)   == 0x90, "DocText+0x90");
static_assert(offsetof(DocumentTextDescription, _arrayB0)     == 0xb0, "DocText+0xb0");
static_assert(offsetof(DocumentTextDescription, _arrayC0)     == 0xc0, "DocText+0xc0");
static_assert(offsetof(DocumentTextDescription, _arrayD0)     == 0xd0, "DocText+0xd0");
static_assert(offsetof(DocumentTextDescription, _containerE0) == 0xe0, "DocText+0xe0");
static_assert(sizeof(DocumentTextDescription) == 0xe8, "sizeof(UI::DocumentTextDescription)==0xe8");

// ---------------------------------------------------------------------------
// UI::EditDescription — 0x88
// ---------------------------------------------------------------------------
// RtUI::EditDescription is now an ALIAS of the real class UI::EditDescription (EditDescription.h);
// see the alias block at the bottom of this file.

// ---------------------------------------------------------------------------
// UI::GraphDescription — 0x88
// ---------------------------------------------------------------------------
struct GraphDescription {
    ElementDescription _base;      // +0x00
    int32_t  _word68;              // +0x68  kScalarInt  @ 0x1800d1a87
    int32_t  _word6c;              // +0x6c  kScalarInt  @ 0x1800d1a9c
    int32_t  _word70;              // +0x70  kScalarInt  @ 0x1800d1ab1
    uint32_t _word74;              // +0x74  kScalarUInt @ 0x1800d1ac6
    void*    _spriteSheet;         // +0x78  0x180004590 @ 0x1800d1ad3
    char*    _spriteName;          // +0x80  0x1800c9930 @ 0x1800d1ae1
};
static_assert(offsetof(GraphDescription, _spriteSheet) == 0x78, "Graph+0x78");
static_assert(offsetof(GraphDescription, _spriteName)  == 0x80, "Graph+0x80");
static_assert(sizeof(GraphDescription) == 0x88, "sizeof(UI::GraphDescription)==0x88");

// ---------------------------------------------------------------------------
// UI::LabelDescription — 0xb8 — UI::TextDescription (0xa8) + two fields
// ---------------------------------------------------------------------------
// The base is another agent's class; only the six dwords LabelDescription's own
// Serialize (0x1800d2820) touches are named, and the prefix below them is kept
// opaque so this header claims nothing about TextDescription's own layout.
// `_base3c` is `ElementDescription`'s +0x3c, which LabelDescription's applier
// copies to the element's +0x128 (`MOV EAX,[RSI+0x3c]` @ 0x1800d27f1) — the
// one field of the base this class reads.
// RtUI::LabelDescription is now an ALIAS of the real class UI::LabelDescription (LabelDescription.h);
// see the alias block at the bottom of this file.

// ---------------------------------------------------------------------------
// The exported vtables and BaseTypeInfo statics these ten classes stamp and
// read. Every name was read out of the retail export directory by RVA (pefile
// against C:\GOG Games\Banished\Runtime-x64.dll), not guessed from the
// mangling pattern.
// ---------------------------------------------------------------------------
constexpr const char* kVtblBorderDescription       = "??_7BorderDescription@UI@@6B@";        // 0x180156980
constexpr const char* kVtblButtonDescription       = "??_7ButtonDescription@UI@@6B@";        // 0x180157370
constexpr const char* kVtblCheckDescription        = "??_7CheckDescription@UI@@6B@";         // 0x1801572c0
constexpr const char* kVtblClipRegionDescription   = "??_7ClipRegionDescription@UI@@6B@";    // 0x180156350
constexpr const char* kVtblComboDescription        = "??_7ComboDescription@UI@@6B@";         // 0x180159090
constexpr const char* kVtblContainerDescription    = "??_7ContainerDescription@UI@@6B@";     // 0x180156510
constexpr const char* kVtblDocumentTextDescription = "??_7DocumentTextDescription@UI@@6B@";  // 0x180156750
constexpr const char* kVtblEditDescription         = "??_7EditDescription@UI@@6B@";          // 0x180156880
constexpr const char* kVtblGraphDescription        = "??_7GraphDescription@UI@@6B@";         // 0x180156a30
constexpr const char* kVtblLabelDescription        = "??_7LabelDescription@UI@@6B@";         // 0x180156b30

constexpr const char* kVtblButtonElement      = "??_7ButtonElement@UI@@6B@";      // 0x180157310
constexpr const char* kVtblCheckElement       = "??_7CheckElement@UI@@6B@";       // 0x180157260
constexpr const char* kVtblClipRegionElement  = "??_7ClipRegionElement@UI@@6B@";  // 0x1801562f0
constexpr const char* kVtblContainerElement   = "??_7ContainerElement@UI@@6B@";   // 0x1801564b0

}  // namespace RtUI

// ---------------------------------------------------------------------------
// ENDGAME B.3.6 fragment pass F2 (RULE B6-4.4) — the PODs that became ALIASES.
// ---------------------------------------------------------------------------
// Each of these had a field-identical real class under the retail name by the
// end of wave 2 (blocks C1, C3, C4 cross-asserted every offset against the
// POD), and nothing embeds any of them by value, so the POD is retired and the
// old spelling names the class. The class headers carry the absolute offsets
// the PODs used to pin. A FORWARD declaration is enough for the alias, which
// keeps this header free of the class headers (they include this one for the
// kVtbl* names above).
namespace UI {
class BorderDescription;
class ButtonDescription;
class CheckDescription;
class ClipRegionDescription;
class ComboDescription;
class ContainerDescription;
class EditDescription;
class LabelDescription;
}  // namespace UI

namespace RtUI {
using BorderDescription = UI::BorderDescription;
using ButtonDescription = UI::ButtonDescription;
using CheckDescription = UI::CheckDescription;
using ClipRegionDescription = UI::ClipRegionDescription;
using ComboDescription = UI::ComboDescription;
using ContainerDescription = UI::ContainerDescription;
using EditDescription = UI::EditDescription;
using LabelDescription = UI::LabelDescription;
}  // namespace RtUI
