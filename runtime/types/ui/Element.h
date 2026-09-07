#pragma once
// ============================================================================
// runtime/types/ui/Element.h — UI::Element, binary-compatible layout
// ============================================================================
//
// Wave 7 lane A. THIS HEADER IS THE WAVE'S LAYOUT CONTRACT: every `*Element`
// class in lanes D and E derives from `UI::Element`, and their own first field
// lands at `sizeof(UI::Element)`. Lane B's ElementDescription::CreateInstance
// builds those objects. Nobody but lane A edits this file; if you need a field
// added, say so in your lane doc and ask.
//
// ---------------------------------------------------------------------------
// SIZE IS PROVEN, NOT INFERRED:  sizeof(UI::Element) == 0xC0 (192), align 8.
// ---------------------------------------------------------------------------
// Two independent proofs, from the shipped Runtime-x64.dll:
//
//  1. `UI::ContainerDescription::CreateInstance` (retail 0x1800ccb90) allocates
//     BOTH the ContainerElement and its ClipRegionElement with
//     `MemoryAllocator::Allocate(_singleton, 0xd8, 8)`, calls
//     `Element::Element` on each, and then writes the derived fields starting
//     at +0xC0 and ending at +0xD4.
//  2. `?GetWidth@ContainerElement@UI@@QEBAHXZ` (0x180039ee0) is one
//     instruction: `MOV EAX,[RCX+0xC0]`, and `?GetHeight@` (0x180039ef0) reads
//     +0xC4. The first field a derived class owns is at +0xC0, so the base
//     ends there.
//
//  Corroboration: `Element::Element` (0x180115e50) initialises fields up to and
//  including the qword at +0xB8 and stops, and `runtime/types/ui/ImageElement.h`
//  had independently recorded "UI::Element (0xC0)" before this lane existed.
//
// ---------------------------------------------------------------------------
// THE CLASS IS POLYMORPHIC, AND ITS VTABLE IS EXPORTED.
// ---------------------------------------------------------------------------
// `??_7Element@UI@@6B@` is an exported data symbol at retail 0x180156160 —
// the exact address `Element::Element` stamps into +0x00. That is Wave 6's
// `??_7Geometry@Graphics@@6B@` lever: a ported ctor can stamp the DLL's own
// vtable byte for byte instead of minting a second one, so a half-ported
// hierarchy never puts two vtables in play (PLAN §4.3 risk 3).
//
// The table is TWELVE slots. Proof is the neighbouring export, not a guess:
// `??_7ElementDescription@UI@@6B@` is at 0x1801561c0, exactly 0x60 bytes past
// 0x180156160 (`reference_vtable_slot_count_from_the_name_string.md` — read
// the slot count from the binary, never from the base class's count).
//
//   slot  off    method                       retail       note
//   ----  -----  ---------------------------  -----------  ---------------------
//    0    0x00   `operator delete`/scalar dtor 0x180035c10  called as fn(this, 1)
//    1    0x08   TypeName()                   0x180035440
//    2    0x10   Type()                       0x180035420  used by FindElementType
//    3    0x18   Persist(IO::Stream&)         0x180116870
//    4    0x20   ComputeSize()                _purecall     PURE in Element
//    5    0x28   ComputeLayout(x,y,w,h)       0x1801168d0
//    6    0x30   Draw(SpriteContext&, byte)   0x1801168f0
//    7    0x38   Update(...)                  0x180116270
//    8    0x40   Enable(bool)                 0x180116910
//    9    0x48   Contains(x,y,UserInterface*) 0x1801169e0
//   10    0x50   SetColor(uint)               0x180035620
//   11    0x58   GetBorder()                  0x180035630
//
// Slot 4 is `_purecall` (0x18007ec28) in Element's own table: EVERY derived
// widget must override ComputeSize, and `Element::GetPaddedSize` /
// `Element::GetAlignedPosition` both call slot 4 before reading +0x60/+0x64.
//
// `??_7ContainerElement@UI@@6B@` (0x1801564b0) is 0x60 bytes as well
// (`??_7ContainerDescription@UI@@6B@` follows at 0x180156510), so
// ContainerElement adds no virtuals.
//
// ---------------------------------------------------------------------------
// INHERITANCE
// ---------------------------------------------------------------------------
//   Object::Base  ->  Object::Internal  ->  UI::Element
//
// `Element::Element` calls `Object::Internal::Internal` (0x1800bfd10) first.
// That ctor stamps `??_7Base@Object@@6B@` (0x180154788) and then immediately
// `??_7Internal@Object@@6B@` (0x180154720) over it — both are exported and both
// addresses match — writes +0x08/+0x0C, and REGISTERS THE OBJECT IN THE GLOBAL
// HANDLE TABLE at `?_singleton@InternalFactory@Object@@0PEAV12@EA`
// (0x1801fc6d8). The base therefore costs 0x10 bytes AND a live handle; see
// WAVE7_LANE_A.md §3 for why that makes the ctor un-replayable.
//
// PORTABILITY: no <windows.h>, per PLAN §4.1 decision 1.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../collection/Collection.h"
// ::UIString — the 0x10-byte UI::String value type, owned by Wave 7 lane F.
// Lane A recovered the identical {table +0x00, index +0x08, pad0C +0x0C} layout
// independently, from `?SetToolTip@Element@UI@@` (0x180035740) copying exactly
// two qwords with no refcount touch and `?IsToolTipValid@` (0x180035640)
// splitting them into a pointer test and a signed-index test. The two
// derivations agreed, and the duplicate was deleted in favour of lane F's —
// one source of truth. See WAVE7_LANE_A.md §4.
#include "String.h"

namespace UI {

struct Element;

// ---------------------------------------------------------------------------
// The tooltip field's type, `::UIString`, comes from
// runtime/types/ui/String.h (lane F). Two facts about it that this lane proved
// and that matter to anyone diffing an Element:
//
//   * IT IS NOT REFCOUNTED. `?SetToolTip@` (0x180035740) copies exactly two
//     qwords from the incoming `String@2` into +0x48/+0x50 with no INC/DEC of
//     any refcount — unlike `?SetToolTipString@` (0x1800356e0), whose argument
//     is a `System::String` and which does the full release/acquire dance.
//     That is what makes SetToolTip replayable and SetToolTipString not.
//   * `pad0C` (Element +0x54) is NEVER WRITTEN by `Element::Element`, which
//     stores only the qword at +0x48 and the dword at +0x50 — but IT IS
//     written by `?SetToolTip@`, whose second store is a full qword at +0x50.
//     Do not compare it against a freshly built object
//     (`reference_oracle_must_not_compare_unwritten_bytes.md`).
// ---------------------------------------------------------------------------
// UI::Element  —  0xC0 bytes.
//
// Every offset below cites the instruction that proves it. A one-line exported
// accessor is the strongest kind of evidence there is, because it gives the
// offset AND the field's real engine name; where one exists it is quoted.
// Retail addresses come from the export directory of
// C:\GOG Games\Banished\Runtime-x64.dll, read with a pefile-style reader.
// ---------------------------------------------------------------------------
struct Element {
    // --- Object::Base / Object::Internal (0x00 .. 0x0F) --------------------
    void*   _vtable;            // +0x00  ??_7Element@UI@@6B@ == 0x180156160.
                                //        ctor 0x180115e50: LEA RAX,[0x180156160]
                                //                          MOV [RBX],RAX
    int32_t _handleIndex;       // +0x08  Object::Internal ctor 0x1800bfd10:
                                //        MOV EAX,[InternalFactory+0x08]
    int32_t _handleId;          // +0x0c  same ctor, return of 0x1800c7f90

    // --- UI::Element -------------------------------------------------------
    // System::String: a pointer to the character body. The refcount is the
    // dword at [-4] and the allocation base is [-8] — that is exactly what
    // ~Element (0x1801161a0) and SetName (0x1801154f0) do:
    //   DEC [p-4]; if now 0 { p -= 8; MemFree(p) } else field = 0
    void*   _name;              // +0x10  ?SetName@   0x1801154f0
                                //        (released in ~Element 0x1801161a0)

    int32_t _leftPad;           // +0x18  ?GetLeftPad@    0x1800355e0
    int32_t _rightPad;          // +0x1c  ?GetRightPad@   0x180035600
    int32_t _topPad;            // +0x20  ?GetTopPad@     0x1800355f0
    int32_t _bottomPad;         // +0x24  ?GetBottomPad@  0x180035610
    // *** NOTE THE ORDER: left, RIGHT, TOP, bottom. ***
    // `?SetPadding@Element@UI@@QEAAXHHHH@Z` (0x180115480) stores its four
    // arguments as EDX->+0x18, R8D->+0x1c, R9D->+0x20, [RSP+0x28]->+0x24, so
    // the parameter order is (left, right, top, bottom) — NOT the
    // (left, top, right, bottom) that `fn_Element_SetPadding_t`
    // (engine/funcs/engine_funcs.h:1579) declares. See WAVE7_LANE_A.md §6.

    int32_t _minWidth;          // +0x28  ?SetMinSize@ 0x1801154a0 (arg 1)
    int32_t _minHeight;         // +0x2c  ?SetMinSize@ 0x1801154a0 (arg 2)
                                //        both are floors in ?GetPaddedSize@

    int32_t _alignment;         // +0x30  ?SetAlignment@ 0x180115750
                                //        ElementDescription::Alignment enum;
                                //        Element::Element defaults it to 3
    uint32_t _flags;            // +0x34  ?GetElementFlags@ 0x1800354d0
                                //        Element::Element defaults 0x08000000
    uint32_t _color;            // +0x38  ?SetColor@ 0x180035620 (vtable slot 10)
                                //        Element::Element defaults 0xFFFFFFFF
    int32_t _mouseCursorIndex;  // +0x3c  ?GetMouseCursorIndex@ 0x180115740

    void*   _toolTipDialog;     // +0x40  ExternalPtr<UI::Dialog>, 8 bytes.
                                //        ?GetToolTipDialog@ 0x180035670 returns
                                //        exactly this one qword.
    ::UIString _toolTip;        // +0x48  ?SetToolTip@ 0x180035740 (16 bytes);
                                //        type owned by runtime/types/ui/String.h
    void*   _toolTipString;     // +0x58  System::String.
                                //        ?SetToolTipString@ 0x1800356e0

    // The computed size, filled by virtual slot 4 (ComputeSize).
    // *** NEITHER IS INITIALISED BY Element::Element. *** The ctor writes the
    // qword at +0x58 and then jumps straight to +0x68; +0x60 and +0x64 keep
    // whatever the allocator left. Excluded from any byte-compare of a fresh
    // object.
    int32_t _layoutWidth;       // +0x60  ?GetLayoutWidth@   0x1800355c0
    int32_t _layoutHeight;      // +0x64  ?GetLayoutHeight@  0x1800355d0

    // The final screen rectangle, written by ComputeLayout implementations and
    // read by Draw / Contains / AdjustUpperLeft.
    int32_t _layoutLeft;        // +0x68  ?GetLayoutLeft@    0x180035580
    int32_t _layoutTop;         // +0x6c  ?GetLayoutTop@     0x1800355a0
    int32_t _layoutRight;       // +0x70  ?GetLayoutRight@   0x180035590
    int32_t _layoutBottom;      // +0x74  ?GetLayoutBottom@  0x1800355b0

    int32_t _unknown78;         // +0x78  Element::Element sets it to -1 and no
                                //        exported accessor names it. NOT a
                                //        guess and NOT a hole: the store is at
                                //        0x180115ebb. Named honestly rather
                                //        than invented.
    uint32_t _pad7c;            // +0x7c  NEVER written by Element::Element
                                //        (0x180115ebb writes 4 bytes at +0x78,
                                //        the next store is the qword at +0x80)

    void*   _userData;          // +0x80  Object::Base*.
                                //        ?GetUserData@ 0x180115550 /
                                //        ?SetUserData@ 0x180115560
    int32_t _elementId;         // +0x88  ?GetElementId@ 0x180115830 /
                                //        ?SetElementId@ 0x180115820; ctor -1
    uint32_t _pad8c;            // +0x8c  NEVER written by Element::Element

    void*   _dialogParent;      // +0x90  InternalPtr<UI::DialogInstance>.
                                //        ?SetDialogParent@ 0x180115630.
                                //        ?GetPaddedSize@ reaches the UI scale
                                //        through the ROOT element's copy of
                                //        this: root->_dialogParent[+0x10][+0xdc]
    Element* _parent;           // +0x98  ?SetParent@ 0x1801154d0.
                                //        The dirty walk in SetNeedsSizeCalc
                                //        (0x180035470) follows it to the root.

    // The child list. `?ChildCount@` (0x180035530) is `MOV EAX,[RCX+0xa8]` and
    // `?Child@` (0x180035540) is `MOV RAX,[RCX+0xa0]; MOV RCX,[RAX+idx*8]`, so
    // this is the standard {data,size,capacity} Array<Element*>. ~Element
    // (0x1801161a0) destroys it with the Array dtor at 0x180037240, which reads
    // +0x00 as the buffer and zeroes +0x00/+0x08.
    Collection::Array<Element*> _children;   // +0xa0 .. +0xaf

    void*   _border;            // +0xb0  InternalPtr<UI::Element>.
                                //        ?GetBorder@ 0x180035630 (vtable 11).
                                //        Element::ComputeLayout (0x1801168d0)
                                //        and Element::Draw (0x1801168f0) are
                                //        three-instruction forwarders onto
                                //        THIS object's vtable slots 5 and 6.
                                //        ~Element destroys it via slot 0 (1).
    void*   _description;       // +0xb8  ExternalPtr<UI::ElementDescription>.
                                //        ?GetDescription@ 0x1800357d0
};

static_assert(sizeof(Element) == 0xC0, "UI::Element is 0xC0 bytes");
static_assert(alignof(Element) == 8,   "UI::Element is 8-aligned");

static_assert(offsetof(Element, _vtable)           == 0x00, "");
static_assert(offsetof(Element, _handleIndex)      == 0x08, "");
static_assert(offsetof(Element, _handleId)         == 0x0c, "");
static_assert(offsetof(Element, _name)             == 0x10, "");
static_assert(offsetof(Element, _leftPad)          == 0x18, "");
static_assert(offsetof(Element, _rightPad)         == 0x1c, "");
static_assert(offsetof(Element, _topPad)           == 0x20, "");
static_assert(offsetof(Element, _bottomPad)        == 0x24, "");
static_assert(offsetof(Element, _minWidth)         == 0x28, "");
static_assert(offsetof(Element, _minHeight)        == 0x2c, "");
static_assert(offsetof(Element, _alignment)        == 0x30, "");
static_assert(offsetof(Element, _flags)            == 0x34, "");
static_assert(offsetof(Element, _color)            == 0x38, "");
static_assert(offsetof(Element, _mouseCursorIndex) == 0x3c, "");
static_assert(offsetof(Element, _toolTipDialog)    == 0x40, "");
static_assert(offsetof(Element, _toolTip)          == 0x48, "");
static_assert(offsetof(Element, _toolTipString)    == 0x58, "");
static_assert(offsetof(Element, _layoutWidth)      == 0x60, "");
static_assert(offsetof(Element, _layoutHeight)     == 0x64, "");
static_assert(offsetof(Element, _layoutLeft)       == 0x68, "");
static_assert(offsetof(Element, _layoutTop)        == 0x6c, "");
static_assert(offsetof(Element, _layoutRight)      == 0x70, "");
static_assert(offsetof(Element, _layoutBottom)     == 0x74, "");
static_assert(offsetof(Element, _unknown78)        == 0x78, "");
static_assert(offsetof(Element, _userData)         == 0x80, "");
static_assert(offsetof(Element, _elementId)        == 0x88, "");
static_assert(offsetof(Element, _dialogParent)     == 0x90, "");
static_assert(offsetof(Element, _parent)           == 0x98, "");
static_assert(offsetof(Element, _children)         == 0xa0, "");
static_assert(offsetof(Element, _border)           == 0xb0, "");
static_assert(offsetof(Element, _description)      == 0xb8, "");

// ---------------------------------------------------------------------------
// Element::_flags (+0x34) — every bit here is named by the exported one-line
// predicate that reads it, or by the setter that writes it. Bits NOT listed are
// NOT guessed.
// ---------------------------------------------------------------------------
enum ElementFlag : uint32_t {
    // bit 0. `?IsVisible@` (0x1800354e0) is `NOT EAX; AND EAX,1`, so the bit
    // means HIDDEN, not visible. `?Hide@` (0x180035830) sets it when its
    // argument is true and clears it when false; `?Show@` (0x1800357e0) is the
    // mirror. It is also the ONE bit whose change triggers the parent dirty
    // walk in Hide / Show / SetFlagsOnOff / Enable / SetToolTip.
    kElementFlagHidden        = 0x00000001,

    // bit 1. Tested together with bit 0 by `?Contains@` (0x1801169e0):
    // `TEST [RCX+0x34],0x3 / JNZ -> return false`. Not named by any accessor.
    kElementFlagNoHitTest     = 0x00000002,

    // bits 2 and 3, both read by the member `?GetAlignedPosition@`
    // (0x180115ff0), each OR'd with one of the two incoming bool arguments
    // before being forwarded to the static overload. Sticky "stretch"
    // overrides; not named by any accessor.
    kElementFlagAlignArg1     = 0x00000004,
    kElementFlagAlignArg2     = 0x00000008,

    // bits 4 and 5, tested as the mask 0x30 by `?GetPaddedSize@`
    // (0x180115f10): when set, the padded width / height is replaced by
    // Video::Device::_singleton's screen width / height scaled by the root
    // dialog's UI scale.
    kElementFlagSizeToScreenH = 0x00000010,
    kElementFlagSizeToScreenW = 0x00000020,

    // bit 18, tested by `?ComputeColor@` (0x180035490): halves the incoming
    // alpha before modulating.
    kElementFlagDimColor      = 0x00040000,

    // bits 16 and 19, set together (0x90000) by `?SetToolTip@` (0x180035740)
    // when the new tooltip is valid; bit 19 alone is cleared (BTR 0x13) when it
    // is not.
    kElementFlagToolTip16     = 0x00010000,
    kElementFlagToolTipValid  = 0x00080000,

    // bit 22. `?WasActivated@` (0x1800354f0).
    kElementFlagWasActivated  = 0x00400000,

    // bit 23. `?IsDisabled@` (0x180035520), and the bit `?Enable@`
    // (0x180116910, vtable slot 8) sets/clears. `?IsToolTipValid@`
    // (0x180035640) also refuses a tooltip while it is set.
    kElementFlagDisabled      = 0x00800000,

    // bit 24. `?IsSystemDisabled@` (0x180035510) reads BYTE [RCX+0x37] & 1.
    // `?IsEnabled@` (0x180035500) is `TEST [RCX+0x34],0x1800000; SETZ` — i.e.
    // enabled means NEITHER bit 23 nor bit 24.
    kElementFlagSystemDisabled = 0x01000000,

    // bit 27. `?NeedsSizeCalc@` (0x180035460) is `SHR EAX,0x1b; AND AL,1`.
    // Element::Element sets it, and `?SetNeedsSizeCalc@` (0x180035470) ORs it
    // into this element AND every ancestor through +0x98.
    kElementFlagNeedsSizeCalc = 0x08000000,
};

} // namespace UI
