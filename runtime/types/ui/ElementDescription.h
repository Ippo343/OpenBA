#pragma once
// ============================================================================
// runtime/types/ui/ElementDescription.h — UI::ElementDescription layout
// ============================================================================
//
// Wave 7 lane B (docs/runtime_port/WAVE7_LANE_B.md). `UI::ElementDescription`
// is the SERIALIZED, resource-side description of one UI element: the thing a
// `.rsc` file parses into, and the thing `CreateInstance` stamps onto a live
// `UI::Element`. It is an `Object::External` (a resource with a path), not an
// `Object::Internal`.
//
// Inheritance, read out of the ctor's three successive vtable stores
// (0x180115a40, disassembled — the decompiler prints them as three writes to
// `*(void**)this` in a row, which is exactly what a base-chain ctor looks like):
//
//     Object::Base            ??_7Base@Object@@6B@             0x180154788
//       -> Object::External   ??_7External@Object@@6B@         0x1801547a0
//         -> UI::ElementDescription
//                             ??_7ElementDescription@UI@@6B@   0x1801561c0
//
// ALL THREE VTABLES ARE EXPORTED. That is the `??_7Geometry@Graphics@@6B@`
// lever from Wave 6 (WAVE7.md fact 2): a ported ctor stamps the DLL's own
// vtable byte for byte instead of minting one, so a half-ported class never
// puts two vtables in play. `runtime/src/ui/dialog.cpp`'s ported ctor resolves
// all three by name and stamps them; it does not synthesise a table.
//
// SIZE: 0x68, and it is PROVEN by the constructor rather than inferred — the
// ctor's last store is a qword at +0x60, and `??1ElementDescription`
// (0x180116100) releases exactly one String, the one at +0x60, before chaining
// to `Object::External::~External`. No member is ever touched above +0x67.
//
// ---------------------------------------------------------------------------
// EVIDENCE FOR EVERY OFFSET
// ---------------------------------------------------------------------------
// Three functions pin this layout between them, and they agree field for field:
//
//   ??0ElementDescription@UI@@QEAA@XZ   0x180115a40  ord  173  — initialiser
//   ?Serialize@ElementDescription@UI@@UEAA_NAEAVStream@IO@@@Z
//                                       0x180116b00  ord 3687  — field TYPES
//   ?CreateInstance@ElementDescription@UI@@QEBAXV?$InternalPtr@VElement@UI@@@Object@@@Z
//                                       0x180115ad0  ord 2004  — field MEANINGS
//
// `Serialize` is the strongest of the three, because the Encoding vtable slot
// it uses names the field's type outright (retail numbers, and note that the
// PROFILE build's Encoding vtable puts these somewhere else — see
// runtime/types/object/Internal.h's warning):
//
//     +0x98  Serialize(uint&, const char*)      kEncSerializeUint
//     +0xa0  Serialize(int&,  const char*)      kEncSerializeInt
//
//   off   field           type      proven by
//   ----  --------------  --------  --------------------------------------------
//   0x00  Object::External base     ctor (3 vtable stores + zeroes at 8/0x10/0x18)
//   0x20  _x              int32     Serialize +0xa0; CreateInstance -> elem+0x18
//   0x24  _y              int32     Serialize +0xa0; CreateInstance -> elem+0x1c
//   0x28  _width          int32     Serialize +0xa0; CreateInstance -> elem+0x20
//   0x2c  _height         int32     Serialize +0xa0; CreateInstance -> elem+0x24
//   0x30  _minWidth       int32     Serialize +0xa0; CreateInstance -> elem+0x28
//   0x34  _minHeight      int32     Serialize +0xa0; CreateInstance -> elem+0x2c
//   0x38  _tabOrder       int32     Serialize +0xa0; CreateInstance -> elem+0x3c
//   0x3c  _id             int32     ctor sets -1; Serialize +0x98 (as uint);
//                                   CreateInstance -> elem+0x38
//   0x40  _elementType    uint32    ctor sets 3; Serialize +0x98;
//                                   CreateInstance -> elem+0x30
//   0x44  _flags          uint32    Serialize +0x98; CreateInstance ORs into
//                                   elem+0x34 (never assigns — see below)
//   0x48  _childDesc      External* CreateInstance calls its vtable +0x40 and
//                                   stores the result in elem+0xb0
//   0x50  _userData       uint64    CreateInstance -> elem+0x40 (qword copy)
//   0x58  _stringTable    StringTable*  CreateInstance: null -> index -1, else
//                                   StringTable::GetIndexFromName(_stringTable,
//                                   _textName); stored to elem+0x48
//   0x60  _textName       String    CreateInstance passes &this[0x60] to
//                                   GetIndexFromName; ~ElementDescription
//                                   releases exactly this one String
//
// The names for +0x20..+0x34 are inferred from what CreateInstance copies them
// INTO on the Element, not from a debug string. They are labelled as such here
// rather than presented as recovered: lane A owns `UI::Element` and its header
// is the authority on what elem+0x18..+0x2c mean. If lane A's names disagree,
// lane A wins and these should be renamed to match.
//
// TWO THINGS THE CTOR DOES NOT DO, both of which matter to a port:
//
//   * `_flags` (+0x44) is left ZERO by the ctor and CreateInstance ORs it into
//     the element rather than assigning, so a description with no flags cannot
//     clear a flag the element already has. That is retail behaviour, not a
//     transcription slip.
//   * The ctor writes EVERY byte of 0x00..0x67 — there is no padding hole. That
//     is what makes the replay oracle in dialog.cpp a full oracle for it rather
//     than a spot check (reference_oracle_must_not_compare_unwritten_bytes.md:
//     an oracle that compares bytes neither side writes fails 100% of calls).
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../object/External.h"

namespace RtUI {

struct ElementDescription {
    RtObject::External _base;          // +0x00 .. +0x1f
    int32_t  _x;                       // +0x20
    int32_t  _y;                       // +0x24
    int32_t  _width;                   // +0x28
    int32_t  _height;                  // +0x2c
    int32_t  _minWidth;                // +0x30
    int32_t  _minHeight;               // +0x34
    int32_t  _tabOrder;                // +0x38
    int32_t  _id;                      // +0x3c  ctor: -1
    uint32_t _elementType;             // +0x40  ctor: 3
    uint32_t _flags;                   // +0x44
    void*    _childDesc;               // +0x48  Object::External* (child desc)
    uint64_t _userData;                // +0x50
    void*    _stringTable;             // +0x58  UI::StringTable*
    char*    _textName;                // +0x60  System::String (refcounted char*)
};

static_assert(offsetof(ElementDescription, _x)           == 0x20, "ElementDescription::_x @ +0x20");
static_assert(offsetof(ElementDescription, _y)           == 0x24, "ElementDescription::_y @ +0x24");
static_assert(offsetof(ElementDescription, _width)       == 0x28, "ElementDescription::_width @ +0x28");
static_assert(offsetof(ElementDescription, _height)      == 0x2c, "ElementDescription::_height @ +0x2c");
static_assert(offsetof(ElementDescription, _minWidth)    == 0x30, "ElementDescription::_minWidth @ +0x30");
static_assert(offsetof(ElementDescription, _minHeight)   == 0x34, "ElementDescription::_minHeight @ +0x34");
static_assert(offsetof(ElementDescription, _tabOrder)    == 0x38, "ElementDescription::_tabOrder @ +0x38");
static_assert(offsetof(ElementDescription, _id)          == 0x3c, "ElementDescription::_id @ +0x3c");
static_assert(offsetof(ElementDescription, _elementType) == 0x40, "ElementDescription::_elementType @ +0x40");
static_assert(offsetof(ElementDescription, _flags)       == 0x44, "ElementDescription::_flags @ +0x44");
static_assert(offsetof(ElementDescription, _childDesc)   == 0x48, "ElementDescription::_childDesc @ +0x48");
static_assert(offsetof(ElementDescription, _userData)    == 0x50, "ElementDescription::_userData @ +0x50");
static_assert(offsetof(ElementDescription, _stringTable) == 0x58, "ElementDescription::_stringTable @ +0x58");
static_assert(offsetof(ElementDescription, _textName)    == 0x60, "ElementDescription::_textName @ +0x60");
static_assert(sizeof(ElementDescription) == 0x68,
              "sizeof(UI::ElementDescription) == 0x68 (ctor 0x180115a40 last store +0x60; "
              "~ElementDescription 0x180116100 touches nothing above)");

// The size the ctor's replay oracle compares. Every byte of it is written by
// the ctor, so a full-buffer comparison is legitimate here.
constexpr size_t kElementDescriptionSize = sizeof(ElementDescription);

// The three vtables the ctor stamps, in the order it stamps them. Exported, so
// the port resolves them by name instead of minting a table (WAVE7.md fact 2).
constexpr const char* kVtblBase               = "??_7Base@Object@@6B@";               // 0x180154788
constexpr const char* kVtblExternal           = "??_7External@Object@@6B@";           // 0x1801547a0
constexpr const char* kVtblElementDescription = "??_7ElementDescription@UI@@6B@";     // 0x1801561c0

}  // namespace RtUI
