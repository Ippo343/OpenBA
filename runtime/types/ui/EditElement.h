#pragma once
// ============================================================================
// runtime/types/ui/EditElement.h — UI::EditElement, binary-compatible layout
// ============================================================================
// Wave 7 lane D. Only the fields this lane actually reversed and proved are
// named; the gap between Element's end (+0xC0, lane A) and these offsets is
// EditElement/TextElement territory this lane did not fully reverse (TextElement
// itself is lane E's scope per docs/runtime_port/WAVE7.md) and is left as
// explicit, undescribed padding rather than guessed.
//
// PROVEN FIELDS, each cited by the instruction that proves it:
//
//   +0xC8  _text    char*    The element's current value, AS TEXT. Read by
//                            `?GetValue@NumberEditElement@UI@@QEBAHXZ`
//                            (0x1800d4b50, "*(char**)(this+0xc8)" fed to
//                            System::String::ToInteger) and read/written by
//                            `?SetText@EditElement@UI@@QEAAXAEBVString@System@@@Z`
//                            (0x1800d0db0, this lane's phantom-name fix target
//                            — see WAVE7_LANE_D.md §5).
//                            NOTE: this IS a raw char* to the String's
//                            character buffer (refcount at [-4], allocation
//                            base at [-8]) — System::String is exactly ONE
//                            POINTER at the ABI level in this engine; both
//                            bodies above prove it independently.
//   +0xE0  _flagE0  uint8_t  A one-byte flag EditElement::SetText gates its
//                            write on (`if (this[0xe0] == 0)`), and which
//                            SpinnerElement::IsActive (0x1800d7c30) returns
//                            UNNEGATED as its own result when its child is an
//                            EditElement-derived widget. Preserved byte for
//                            byte; its semantic sense (0 == "active"? 0 ==
//                            "not read-only"?) is NOT asserted here, only its
//                            offset and the two behaviors above.
//
// sizeof(EditElement) is NOT asserted to be a specific "real" retail size:
// the struct below is sized only by its own members, and the compiler's
// natural 8-byte tail padding (from Element's alignof 8) happens to put
// _flagE0's containing region at 0xE8 — NumberEditElement.h relies on that via
// a static_assert, so if this file's shape ever changes, that assert will
// fail loudly rather than silently drifting.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Element.h"

namespace UI {

struct EditElement {
    Element  base;              // +0x00 .. +0xBF
    uint8_t  _pad_c0_c8[0x08];  // +0xC0 .. +0xC7 — TextElement's own leading
                                 //   field(s); not reversed by this lane.
    char*    _text;              // +0xC8
    uint8_t  _pad_d0_e0[0x10];  // +0xD0 .. +0xDF — not reversed by this lane.
    uint8_t  _flagE0;            // +0xE0
};

static_assert(offsetof(EditElement, base)    == 0x00, "EditElement::base @ +0x00");
static_assert(offsetof(EditElement, _text)   == 0xC8, "EditElement::_text @ +0xC8");
static_assert(offsetof(EditElement, _flagE0) == 0xE0, "EditElement::_flagE0 @ +0xE0");

} // namespace UI
