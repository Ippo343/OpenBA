// ============================================================================
// runtime/src/ui/element_vtable.cpp — WAVE 10B LANE ELEM
//   the eight remaining slots of ??_7Element@UI@@6B@
// ============================================================================
//
// WHAT THIS FILE IS FOR
// ---------------------------------------------------------------------------
// `??_7Element@UI@@6B@` (DLL 0x180156160, 12 slots) is one of the four minted
// UI vtables that `tools/runtime_census.py --unported --namespace UI` still
// lists. As lane UIDATA established and standalone/reg_mint_promoted.cpp
// restates, the row is not "unwritten data" — the table is already minted in
// standalone/rt_data_mint_tables.inc. What was unwritten is its CONTENTS: a
// minted slot is filled from the port registry and stays a NAMED ABORT TRAP
// until the body behind it is ported.
//
// At the wave-10B baseline (docs/runtime_port/wave10b/baseline_shadow_7358732.txt)
// the row read `slots=12 ported=3 TRAP=8 purecall=1`. The three that were ours
// are slot 0 (the deleting destructor, wave 9 lane K) and slots 5/6
// (ComputeLayout/Draw, wave 7 lane A, in element.cpp). Slot 4 (ComputeSize) is
// `_purecall` in `Element` itself. This file is the other eight.
//
// Registering a vtable is NOT passive: our ported `??0Element` stamps this
// table into every Element in the game, so the DLL's own
// `UI::UserInterface::Update` dispatches straight into these bodies. That is
// the whole risk and the whole point.
//
//   | slot | Export                      | Retail      | Ord  | Bytes | Oracle |
//   |------|-----------------------------|-------------|------|-------|--------|
//   |  1   | ?TypeName@Element@UI@       | 0x180035440 | 4385 |   8   | replay |
//   |  2   | ?Type@Element@UI@           | 0x180035420 | 4178 |   7   | replay |
//   |  3   | ?Persist@Element@UI@        | 0x180116870 | 3323 |  0x4b | pin    |
//   |  7   | ?Update@Element@UI@         | 0x180116270 | 4489 | 0x5ec | pin    |
//   |  8   | ?Enable@Element@UI@         | 0x180116910 | 2288 |  0xc8 | scratch|
//   |  9   | ?Contains@Element@UI@       | 0x1801169e0 | 1862 | 0x101 | replay |
//   | 10   | ?SetColor@Element@UI@       | 0x180035620 | 3847 |   4   | scratch|
//   | 11   | ?GetBorder@Element@UI@      | 0x180035630 | 2468 |  0xe  | replay |
//
// THE FOUR ORACLE DISPOSITIONS, AND WHY EACH ONE
// ---------------------------------------------------------------------------
//  * `replay`  — the body is a pure read (or writes only a caller-owned return
//    slot) and re-running the shipped export on the SAME live object changes
//    nothing. Full comparison, no qualification.
//  * `scratch` — the body mutates `this`. Ours runs on the live object; a
//    PRIVATE 0xC0-byte copy of the pre-state is handed to the shipped export
//    and all 0xC0 bytes are compared. For `Enable` the copy additionally has
//    its child array emptied — see that body for why, stated rather than
//    hidden.
//  * `pin`     — the body cannot be replayed (it emits into a stream, or it
//    raises UI events and closes dialogs), so there is no post-state to
//    compare. What IS checked, at registration time and through an ordinary
//    diff site, is that the SHIPPED BYTES this port was transcribed from are
//    the bytes in the loaded image: an FNV-1a-64 over the whole shipped body.
//    That is a PRE-FLIGHT ON THE TRANSCRIPTION, not a post-state comparison
//    against retail, and this file says so in both places rather than letting
//    a `divergences=0` row imply more than it measured.
//
// A RETAIL DEAD BRANCH THIS LANE FOUND, AND KEPT
// ---------------------------------------------------------------------------
// `Enable` and `Update` both end a flag update with the same shape:
//
//     MOV  R8D,EAX          ; old flags
//     BTR  R8D,0x17         ; clear bit 23   (or BTS 0x11 / BTR 0x11 / BTS 0x14)
//     MOV  [RCX+0x34],R8D
//     XOR  R8D,EAX          ; what changed
//     TEST R8B,0x1          ; <-- bit ZERO
//     JZ   ...              ; skip the ancestor dirty walk
//
// The XOR can only ever have the ONE bit the BTS/BTR touched set, and that bit
// is 23, 17 or 20 — never bit 0. So `TEST R8B,1` is always zero and the
// ancestor `_flags |= 0x08000000` walk that follows is UNREACHABLE in the
// shipped build. The bytes were read out of the loaded image to be sure
// (0x18011693b: `41 f6 c0 01`), not taken from Ghidra's C; Ghidra's own
// decompiler agrees on one of the five copies, emitting
// "WARNING: Removing unreachable block (ram,0x000180116390)" for Update's
// set-bit-17 arm while keeping the identical clear-bit-17 arm.
//
// The obvious reading is that the source said `if (changed & kElementFlagHidden)`
// where it meant the bit it had just written — bit 0 IS `kElementFlagHidden`,
// and `?Hide@` / `?Show@` / `?SetFlagsOnOff@` (element.cpp) do the same
// sequence with bit 0 and DO reach the walk. That is a retail bug, not a
// decoding error, and porting it out would be a behaviour change: elements
// whose enabled-state or hover-state changes do NOT get marked needs-size-calc
// in the shipped game. The walks below are written literally, so the condition
// is evaluated and provably false rather than deleted.
//
// PORTABILITY: no <windows.h>, per PLAN §4.1 decision 1.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/ui/Element.h"
#include "../../types/ui/UserInterface.h"

// ResolveAddr / ResolveExport, forward-declared rather than pulled in from
// standalone/resolve.h — resolve.h drags in <windows.h> and PLAN §4.1 requires
// runtime/src/ to stay platform-neutral. Byte-identical to resolve.h's; the
// linker checks them against it. (Same declaration block as element.cpp:126.)
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
void*     ResolveExport(const char* mangled_name);

using UI::Element;

namespace {

constexpr size_t kElementSize = sizeof(Element);   // 0xC0, static_assert'd in the header

// ---------------------------------------------------------------------------
// Mangled names. Repeated as literals at the registrations at the bottom for
// the reason element.cpp:139 gives — tools/runtime_census.py cannot see through
// a named constant and would under-count the lane
// (reference_census_cannot_see_nonliteral_registrations.md).
// ---------------------------------------------------------------------------
const char* const kTypeName  = "?TypeName@Element@UI@@UEBAPEBDXZ";
const char* const kType      = "?Type@Element@UI@@UEBAIXZ";
const char* const kPersist   = "?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z";
// NOTE THE BACKREFERENCE. Element's own Update mangles `UpdateState@12@`,
// while every DERIVED override mangles `UpdateState@Element@2@`. They are
// different strings and only this one resolves to 0x180116270; the derived
// spelling resolves to a different function entirely
// (reference_resolveexport_phantom_names.md is the general form of this trap).
const char* const kUpdate =
    "?Update@Element@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@"
    "V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@12@@Z";
const char* const kEnable    = "?Enable@Element@UI@@UEAAX_N@Z";
const char* const kContains  = "?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z";
const char* const kSetColor  = "?SetColor@Element@UI@@UEAAXI@Z";
const char* const kGetBorder = "?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ";

// ---------------------------------------------------------------------------
// The exported DATA slot 0x180035420 and 0x180035440 read.
//
// `?_typeInfo@Element@UI@@0VBaseTypeInfo@Object@@A` is at 0x1801fab30 and IS in
// the export table (ordinal 4856, confirmed with pefile). `0V…A` is the PRIVATE
// static spelling and this DLL exports its private members, so a NAME door is
// the correct door here — `reference_aeaa_is_private_not_unexported.md`.
//
//   0x180035420  MOV EAX,dword ptr [0x1801fab30]   -> typeInfo + 0x00 = typeId
//   0x180035440  MOV RAX,qword ptr [0x1801fab38]   -> typeInfo + 0x08 = name
//
// Two adjacent vtable slots reading +0x00 and +0x08 of one address is what
// makes this a BaseTypeInfo rather than a guess (dialog.cpp:836 makes the same
// argument for DialogInstance's pair).
// ---------------------------------------------------------------------------
const uint8_t* ElementTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@Element@UI@@0VBaseTypeInfo@Object@@A");
}

// ---------------------------------------------------------------------------
// Dependencies these bodies CALL rather than reimplement.
//
// Resolution rule, restated from element.cpp:172 because this file has both
// kinds: ResolveExport/RT_EXPORT_ONCE for anything with an exported name (so a
// sibling lane's port is used when there is one), ResolveAddr(Module::Runtime,
// …) only for the ONE unexported internal.
// ---------------------------------------------------------------------------

// THE ONE ADDRESS DOOR, declared rather than hidden.
//
// 0x180033690 is `Collection::Array<UI::Event>::SetSize(int, const Event*)` —
// the grow-and-fill helper every UI event push goes through. It is NOT in the
// export table (checked with pefile), so there is no name door for it. A
// by-address call is a CUTOVER OBLIGATION, not a free choice
// (reference_by_address_door_dies_at_cutover.md). It is NOT NEW DEBT: lane UI1
// (userinterface_frame.cpp:216) and lane CLOSEDLG (userinterface_close.cpp:201)
// already own this exact address for the same reason, and this is the third
// call site of the same helper, not a third obligation.
//
// It is reached through ResolveAddr(Module::Runtime, …), which consults the
// port registry FIRST — so the day anyone ports the helper we get theirs
// (reference_resolveaddr_consults_port_registry_first.md).
constexpr uintptr_t kEventsSetSizeAddr = 0x180033690;

using SetSizeFn     = void  (*)(void* array, int32_t newSize, const void* fill);
using ClipTestFn    = bool  (*)(const void* ui, int32_t x, int32_t y);
using BeginToolTipFn= void  (*)(void* ui, void* dlgPtr, void* elemPtr, int32_t x, int32_t y);
using SetUpperLeftFn= void  (*)(void* dlg, int32_t x, int32_t y, bool b);
using EndHoverFn    = void  (*)(void* dlg, void* ui);
using CloseDialogFn = void  (*)(void* ui, void* dlgPtr);
using PlayFn        = void  (*)(void* ui, int32_t id);
using EndDragDropFn = void  (*)(void* ui);

SetSizeFn EventsSetSize() {
    static SetSizeFn fn = reinterpret_cast<SetSizeFn>(
        ResolveAddr(Module::Runtime, kEventsSetSizeAddr));
    return fn;
}

// ---------------------------------------------------------------------------
// UI::Element::UpdateState — 0x30 bytes.
//
// NOT re-derived here: this is lane UI1's recovery, transcribed field-for-field
// from runtime/src/ui/userinterface_frame.cpp:242, which pinned it from the
// frame of `UserInterface::Update` (the function that BUILDS the object this
// one receives). Only the fields Element::Update actually reads are named
// below; the others keep UI1's names so a reader can diff the two blocks.
//
// It is duplicated rather than hoisted into a shared header on purpose: UI1's
// copy is a stack local it owns end to end, and hoisting it would make one
// lane's correction silently change the other's meaning. The static_assert on
// the size is the guard that keeps the two honest.
// ---------------------------------------------------------------------------
struct UpdateState {
    uint8_t  dragActive;      // +0x00  = (_dragDropDialog != 0)
    uint8_t  buttonPressed;   // +0x01
    uint8_t  b02;             // +0x02
    uint8_t  b03;             // +0x03
    uint8_t  b04;             // +0x04
    uint8_t  b05;             // +0x05
    uint8_t  b06;             // +0x06
    uint8_t  leftDown;        // +0x07
    uint8_t  b08;             // +0x08
    uint8_t  b09;             // +0x09
    uint8_t  pad0a[2];        // +0x0a
    int32_t  dragDx;          // +0x0c
    int32_t  dragDy;          // +0x10
    int32_t  wheel;           // +0x14
    int32_t  mouseX;          // +0x18
    int32_t  mouseY;          // +0x1c
    uint8_t  mouseCaptured;   // +0x20
    uint8_t  pad21[7];        // +0x21
    void*    config;          // +0x28
};
static_assert(sizeof(UpdateState) == 0x30, "UpdateState frame span is 0x30");
static_assert(offsetof(UpdateState, dragActive) == 0x00, "");
static_assert(offsetof(UpdateState, leftDown)   == 0x07, "");
static_assert(offsetof(UpdateState, dragDx)     == 0x0c, "");
static_assert(offsetof(UpdateState, dragDy)     == 0x10, "");
static_assert(offsetof(UpdateState, mouseX)     == 0x18, "");
static_assert(offsetof(UpdateState, mouseY)     == 0x1c, "");

// ---------------------------------------------------------------------------
// One queued UI event, 0x30 bytes — and this lane can NAME THE INTERIOR that
// runtime/types/ui/UserInterface.h:176 deliberately left opaque.
//
// UserInterface.h's `Event` says "the INTERIOR is not modelled, because nothing
// this lane ports reads a field of one", and records what its two push sites
// showed: "+0x00 is the target dialog pointer, +0x10 is an event id, +0x14 is
// -1, +0x20 copies _dragDropId and +0x28 copies _dragDropData". Element::Update
// is the site that fills the rest in: it writes ELEVEN records at nine distinct
// offsets and the frame slots line up exactly with UI1's, so
//
//      +0x00 dialog      +0x08 element     +0x10 eventId
//      +0x14 elementId   +0x18 mouseX      +0x1c mouseY
//      +0x20 dragId      +0x24 (never written by either site)
//      +0x28 dragData
//
// UI1's records are the SAME struct with element=0 and elementId=-1 (a UI-level
// event has no element). The header's opaque `Event` is left alone — this is
// the layout a future editor of it should merge, evidenced here rather than
// asserted there.
//
// +0x24 is deliberately left uninitialised: retail never writes it and the grow
// helper never copies it (userinterface_frame.cpp:277 records the same).
// ---------------------------------------------------------------------------
struct EventRecord {
    void*    dialog;     // +0x00
    void*    element;    // +0x08
    int32_t  eventId;    // +0x10
    int32_t  elementId;  // +0x14
    int32_t  mouseX;     // +0x18
    int32_t  mouseY;     // +0x1c
    uint32_t dragId;     // +0x20
    uint32_t pad24;      // +0x24  NEVER written by retail
    void*    dragData;   // +0x28
};
static_assert(sizeof(EventRecord) == 0x30, "UI::Event stride");
static_assert(offsetof(EventRecord, eventId)  == 0x10, "");
static_assert(offsetof(EventRecord, dragId)   == 0x20, "");
static_assert(offsetof(EventRecord, dragData) == 0x28, "");

// ---------------------------------------------------------------------------
// The parent dirty walk, verbatim from `?SetNeedsSizeCalc@` (0x180035470) —
// a do/while, not a while: the OR happens BEFORE the null test.
//
// EVERY CALL TO THIS IN THIS FILE IS UNREACHABLE IN THE SHIPPED BUILD; see the
// banner. It is kept so the condition is evaluated rather than deleted.
// ---------------------------------------------------------------------------
inline void NeedsSizeCalcWalk(Element* e) {
    do {
        e->_flags |= UI::kElementFlagNeedsSizeCalc;
        e = e->_parent;
    } while (e != nullptr);
}

template <typename Fn>
inline Fn VSlot(const void* obj, int index) {
    return reinterpret_cast<Fn>((*reinterpret_cast<void* const* const*>(obj))[index]);
}

constexpr int kSlotUpdate   = 7;   // +0x38
constexpr int kSlotEnable   = 8;   // +0x40
constexpr int kSlotContains = 9;   // +0x48

// ---------------------------------------------------------------------------
// The SHIPPED-BYTE PIN (the `pin` oracle disposition)
// ---------------------------------------------------------------------------
// For the two bodies that cannot be replayed, this is what is actually
// measured: FNV-1a-64 over the shipped body as it sits in the LOADED image,
}  // namespace

// ============================================================================
// The ports
// ============================================================================
// Every body is byte-faithful to the retail DISASSEMBLY, not to Ghidra's C:
// that C drops dereferences and prints MOVSXD as a zero extension, and three
// Wave 6 defects came from trusting it
// (reference_ghidra_c_output_vs_disassembly.md). Where retail derefs without a
// null check, so do we.

// ---- slot 2  ?Type@Element@UI@@UEBAIXZ  0x180035420 ------------------------
//   MOV EAX,dword ptr [0x1801fab30] / RET     — RCX is never read.
extern "C" uint32_t RtUi_Element_Type(const void* self) {
    (void)self;
    uint32_t ours = 0;
    std::memcpy(&ours, ElementTypeInfo(), sizeof ours);
    return ours;
}

// ---- slot 1  ?TypeName@Element@UI@@UEBAPEBDXZ  0x180035440 -----------------
//   MOV RAX,qword ptr [0x1801fab38] / RET     — typeInfo + 8, RCX unread.
//
// Returning the literal "Element" was rejected for dialog.cpp:849's reason: it
// would compare equal under every strcmp and be a DIFFERENT POINTER from the
// one Factory::Startup CRC'd and from the one every other reader sees. A port
// whose only failure mode is pointer identity fails silently and late.
//
// WAVE 10C lane TIDENT — TWO CORRECTIONS TO THAT PARAGRAPH, both measured.
//
//  1. Reading the field does NOT avoid the pointer difference it warns about.
//     `?_typeInfo@Element@UI@@` is a PROMOTED minted row, so `ElementTypeInfo()`
//     resolves to OUR node and `ours` is a literal in OUR image just the same.
//  2. "Fails silently and late" is not what the pointer difference does, and
//     the reason is now a census rather than a worry: NOTHING in either shipped
//     binary consumes a BaseTypeInfo::name by pointer. All 525 exe references
//     to the imported `?_typeInfo@` slots read the dword at +0x00; the DLL's
//     only static readers of +0x08 are the 103 `?TypeName@` accessors, and not
//     one of them is called anywhere in the DLL. Addresses:
//     docs/runtime_port/WAVE10C_LANE_TIDENT.md.
//
// Reading the field is still the right body — it is what the two instructions
// do — but it is right because it is FAITHFUL, not because it dodges identity.
extern "C" const char* RtUi_Element_TypeName(const void* self) {
    (void)self;
    const char* ours = nullptr;
    std::memcpy(&ours, ElementTypeInfo() + 8, sizeof ours);
    return ours;
}

// ---- slot 10  ?SetColor@Element@UI@@UEAAXI@Z  0x180035620 ------------------
//   MOV dword ptr [RCX + 0x38],EDX / RET
//
// One store. NO dirty walk, NO clamp, NO notification: it is the whole body,
// which is worth saying because every OTHER mutator in this class ends with the
// ancestor walk and a reader who assumes symmetry would add one.
//
// Oracle `scratch`: the shipped export is given a private copy of the
// PRE-state and all 0xC0 bytes are compared, so a wrong offset or a stray
// write into padding is caught, not just the field we meant to write.
extern "C" void RtUi_Element_SetColor(Element* self, uint32_t color) {

    alignas(16) uint8_t before[kElementSize];

    self->_color = color;

}

// ---- slot 11  ?GetBorder@Element@UI@@UEBA…  0x180035630 --------------------
//   MOV RAX,qword ptr [RCX + 0xb0] / MOV qword ptr [RDX],RAX
//   MOV RAX,RDX / RET
//
// The hidden return slot is in RDX (an `InternalPtr<Element>` the caller owns),
// and the function returns that same pointer — the MSVC convention for a
// class return. Getting the RETURN wrong here is the classic hidden-ret-slot
// defect (reference_hidden_retslot_wild_write.md), so the oracle compares both
// the value written into the slot AND the returned pointer.
extern "C" void** RtUi_Element_GetBorder(const Element* self, void** ret) {

    *ret = self->_border;

    return ret;
}

// ---- slot 8  ?Enable@Element@UI@@UEAAX_N@Z  0x180116910 --------------------
//
//   old = _flags
//   _flags = enable ? (old & ~0x800000) : (old | 0x800000)   [bit 23, kDisabled]
//   if (((_flags ^ old) & 1) != 0) NeedsSizeCalcWalk(this);  [DEAD — see banner]
//   for (i = 0; i < _children._size; ++i)
//       children[i]->vt[8](children[i], enable)
//
// THE ARGUMENT MEANS WHAT ITS NAME SAYS. `Enable(true)` CLEARS bit 23
// (0x180116928: `TEST DL,DL / JZ`, true arm 0x18011692f: `BTR R8D,0x17`), and bit 23 is
// `kElementFlagDisabled` (`?IsEnabled@` 0x180035500 is `TEST 0x1800000; SETZ`).
// An earlier revision of this port had the sense INVERTED, with a banner
// insisting the inversion was retail's. It was not, and the defect was
// invisible with the DLL present: the exe's dialogs reach slot 8 through the
// DLL's own vtables, so this body only ran under the diff oracle. The first
// DLL-absent Load dialog (2026-09-12) called SetEnabled(button, 1) on
// Load/Cancel/Delete through the minted vtable and all three came up greyed.
//
// The child recursion goes through the CHILD's own slot 8, not through this
// body, so a ButtonElement child reaches `?Enable@ButtonElement@` (0x180110cc0)
// — hard-coding this function would break that silently.
//
// NO NULL CHECK ON THE CHILD (0x1801169ab derefs it immediately), unlike
// `Contains` two functions below, which does test. Retail is asymmetric here
// and the asymmetry is preserved: CLAUDE.md's rule is that an unguarded deref
// is load-bearing.
//
// ORACLE `scratch`, WITH ONE STATED EXCLUSION. The shipped body is run on a
// private copy of the pre-state whose CHILD ARRAY IS EMPTIED
// (`_children = {null,0,0}`), and ours is run a second time on an identically
// emptied copy; the two 0xC0-byte results are compared. Emptying is not
// cosmetic and it is not free:
//   * it is NECESSARY — the shipped body would otherwise recurse into the REAL
//     children through their real vtables, running a second Enable on live
//     objects whose derived overrides (ButtonElement's, for one) are not known
//     to be idempotent. That is the Wave-7 replay argument failing, and it
//     fails here rather than being waved through.
//   * it means the oracle covers the FLAG ARITHMETIC and nothing else. The
//     six-instruction child loop is covered by the shipped-byte pin and by the
//     live A/B, and this comment is the only place that says so.
extern "C" void RtUi_Element_Enable(Element* self, bool enable) {

    alignas(16) uint8_t oursCopy[kElementSize];
    alignas(16) uint8_t theirsCopy[kElementSize];

    const uint32_t old = self->_flags;
    if (enable) self->_flags = old & ~UI::kElementFlagDisabled;
    else        self->_flags = old |  UI::kElementFlagDisabled;
    // DEAD IN THE SHIPPED BUILD (`TEST R8B,0x1` on a diff that can only hold
    // bit 23). Written literally so it is evaluated, not deleted.
    if (((self->_flags ^ old) & UI::kElementFlagHidden) != 0) NeedsSizeCalcWalk(self);

    for (int32_t i = 0; i < self->_children._size; ++i) {
        Element* child = self->_children._data[i];
        using Fn = void (*)(Element*, bool);
        VSlot<Fn>(child, kSlotEnable)(child, enable);
    }

}

// ---- slot 9  ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z  0x1801169e0
//
//   if (_flags & 3) return false;                  [hidden | no-hit-test]
//   for (i = 0; i < _children._size; ++i) {
//       child = children[i];
//       if (child && ui->IsInsideClipRegion(x, y) && child->vt[9](child,x,y,ui))
//           return true;
//   }
//   if (_children._size != 0) return false;        [a container never hits itself]
//   return x >= clip.left && x <= clip.right && y >= clip.top && y <= clip.bottom
//       && x >= _layoutLeft && x < _layoutRight
//       && y >= _layoutTop  && y < _layoutBottom;
//
// TWO ASYMMETRIES THAT LOOK LIKE TYPOS AND ARE NOT, both read off the
// disassembly rather than off Ghidra's C:
//   * the CLIP test is INCLUSIVE on all four sides (JL / JG at 0x180116a87,
//     0x180116a91, 0x180116a9b, 0x180116aa5) while the ELEMENT RECT test is
//     half-open — `>= left`, `< right` (JL / JGE at 0x180116aab, 0x180116ab1).
//     Element::Update's own hover test, forty lines below, is EXCLUSIVE on all
//     four element sides. Three different conventions in one class.
//   * `IsInsideClipRegion` is re-evaluated INSIDE the child loop, on the same
//     x/y, once per child. Hoisting it would be a strict improvement and a
//     divergence; it stays where retail put it.
//
// The child null test IS present here (0x180116a3b) — the opposite of Enable.
//
// ORACLE `replay`: the whole call chain is `UEBA` (const) — this body, the
// child dispatch and IsInsideClipRegion all only read — so re-running the
// shipped export on the SAME live object is free of side effects and the bool
// is compared directly. That argument depends on every override of slot 9
// being const too, which the export table confirms for the one override that
// exists (`?Contains@ClipRegionElement@UI@@UEBA…`, 0x1800cbcc0).
extern "C" bool RtUi_Element_Contains(const Element* self, int32_t x, int32_t y,
                                      void* ui) {

    bool ours;
    if ((self->_flags & (UI::kElementFlagHidden | UI::kElementFlagNoHitTest)) != 0) {
        ours = false;
    } else {
        ours = false;
        bool hit = false;
        ClipTestFn inClip = RT_EXPORT_ONCE(ClipTestFn,
            "?IsInsideClipRegion@UserInterface@UI@@QEBA_NHH@Z");
        for (int32_t i = 0; i < self->_children._size; ++i) {
            Element* child = self->_children._data[i];
            if (!child) continue;
            if (!inClip(ui, x, y)) continue;
            using Fn = bool (*)(const Element*, int32_t, int32_t, void*);
            if (VSlot<Fn>(child, kSlotContains)(child, x, y, ui)) { hit = true; break; }
        }
        if (hit) {
            ours = true;
        } else if (self->_children._size != 0) {
            ours = false;
        } else {
            const UI::UserInterface* u = static_cast<const UI::UserInterface*>(ui);
            ours = x >= u->_clipRegion.left && x <= u->_clipRegion.right &&
                   y >= u->_clipRegion.top  && y <= u->_clipRegion.bottom &&
                   x >= self->_layoutLeft   && x <  self->_layoutRight &&
                   y >= self->_layoutTop    && y <  self->_layoutBottom;
        }
    }

    return ours;
}

// ---- slot 3  ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z  0x180116870 ------
//
//   scope.stream = &stream;                        [rsp+0x20]
//   scope.node   = nullptr;                        [rsp+0x28]
//   Encoding* enc = stream[+0x10];
//   node = (stream[+0x08] == 0) ? enc->vt[+0x18](enc, &scope, 0)
//                               : enc->vt[+0x20](enc, &scope, 0);
//     ^ THREE arguments. The 0 is retail's `XOR R8D,R8D` at 0x180116889, and
//       it is the codec VERSION. This transcription originally showed two and
//       the port matched it; see the body for what that cost and how it was
//       caught.
//   scope.node = node;
//   if (node) node->vt[0](node, 1);                [deleting destructor]
//
// Element itself persists NOTHING. The body opens an encoding SCOPE and closes
// it immediately; every derived element that has state overrides this slot.
// The base still has to exist, and dialog.cpp:806 already argues why an empty
// or near-empty override is a real port: what the cutover needs is a non-null
// address to stamp into slot 3, and a null there is indistinguishable from a
// correct override right up until it is called.
//
// THE TWO ENCODING SLOTS ARE READ FROM THE OBJECT, NOT NAMED. +0x18 and +0x20
// are byte displacements into whatever vtable `stream[+0x10]` carries, taken at
// run time. That sidesteps the blocker that held `?Serialize@SpriteSheet@` back
// for a whole lane — retail's IO::Encoding vtable has DIFFERENT offsets from
// the profile build's, so a profile-build symbol cannot name a retail slot
// (reference_encoding_vtable_offsets_differ_between_builds.md). Here nothing
// needs naming: the displacement is the datum.
//
// ORACLE: `pin`, and NOT replay. Running the shipped body a second time on the
// same stream would open and close a SECOND scope — on the write path that is
// an extra record in the file. There is no post-state to compare that is not
// itself the side effect. What is measured is the shipped-byte pin below; the
// per-slot table and WAVE10B_LANE_ELEM.md both say `ported, not differentially
// verified` for this row rather than letting a green pin read as more.
// ---------------------------------------------------------------------------
// ATTENDANCE COUNTERS for Persist and Update -- Wave 10C lane ROUTES
// ---------------------------------------------------------------------------
// These count ENTRIES, unconditionally, on every build -- the
// runtime/src/io/encoding.cpp / dialog_controller_base.cpp idiom.
//
// NEITHER COUNTER IS A VERIFICATION. Both bodies remain PORTED, NOT
// DIFFERENTIALLY VERIFIED for the reasons their own headers give (neither is
// replayable). A non-zero count here says only that the port is on a live path.
namespace {
struct ElemCounts {
    unsigned long long update = 0, persist = 0;
    ~ElemCounts() {
        if (update == 0 && persist == 0) {
            printf("[Element] ATTENDANCE: neither Update nor Persist was entered "
                   "in this run.\n");
            return;
        }
        printf("[Element] ATTENDANCE (entries, not comparisons): Update=%llu "
               "Persist=%llu\n", update, persist);
    }
};
ElemCounts g_elemCounts;
}  // namespace

extern "C" void RtUi_Element_Persist(void* self, void* stream) {
    ++g_elemCounts.persist;
    (void)self;
    struct Scope { void* stream; void* node; };
    Scope scope;
    scope.stream = stream;
    scope.node   = nullptr;

    uint8_t* const s = static_cast<uint8_t*>(stream);
    void* const enc = *reinterpret_cast<void**>(s + 0x10);
    void* const* const encVt = *reinterpret_cast<void* const* const*>(enc);
    const int32_t mode = *reinterpret_cast<const int32_t*>(s + 0x08);

    // THE THIRD ARGUMENT IS NOT OPTIONAL, and leaving it off was a real defect
    // that survived from Wave 10B until the differential in
    // runtime/src/ui/element_persist_probe.cpp (Wave 10E lane VERIFY) ran.
    //
    // The two slots are `IO::Encoding::CreateReader` (0x1800a8120, +0x18) and
    // `IO::Encoding::CreateWriter` (0x1800a8080, +0x20), and both are
    // `(Encoding*, Serializer*, int32_t version)` — runtime/src/io/encoding.cpp
    // :285 ports them with that signature, and goal_serialize.cpp:156 already
    // spelled the call `enc->vtable[+0x18](enc, &ser, 0)`.
    //
    // The shipped body passes the 0: `XOR R8D,R8D` at 0x180116889 sets up the
    // `CMP dword ptr [RDX+0x8],R8D` on the line after AND leaves R8D = 0 across
    // the call at 0x180116897 / 0x18011689c. A two-argument typedef leaves R8
    // holding whatever the caller left, the codec writes it into the record as
    // the version, and the emitted bytes diverge from retail's in exactly that
    // dword — measured, not argued: ours `aa aa aa aa 04 00 00 00 80 80 07 ab
    // bb bb bb bb`, retail's `aa aa aa aa 04 00 00 00 00 00 00 00 bb bb bb bb`,
    // reproducibly, on both the read and the write slot.
    using ScopeFn = void* (*)(void*, void*, int32_t);
    ScopeFn open = reinterpret_cast<ScopeFn>(
        encVt[(mode == 0 ? 0x18 : 0x20) / sizeof(void*)]);
    void* const node = open(enc, &scope, 0);

    scope.node = node;
    if (node) {
        // vt[0](node, 1) — the scalar deleting destructor with the delete flag,
        // exactly as 0x1801168b4 spells it.
        using DeleteFn = void (*)(void*, int32_t);
        VSlot<DeleteFn>(node, 0)(node, 1);
    }
}

// ---- slot 7  ?Update@Element@UI@@UEAA…  0x180116270 ------------------------
//
// THE HOT ONE. The DLL's own `UI::UserInterface::Update` reaches this slot for
// every element of every open dialog, every frame, so a regression here is a
// whole-UI regression rather than a widget one. It is A/B'd explicitly in
// docs/runtime_port/WAVE10B_LANE_ELEM.md.
//
// Structure, in the order retail runs it:
//
//   1. recurse into every non-null, non-HIDDEN child through the child's own
//      slot 7. The InternalPtr argument is re-read from the caller's slot on
//      EVERY iteration (0x1801162c6 is inside the loop) and copied into a
//      local the child is given the address of.
//   2. `inClip` = the mouse position lies within ui->_clipRegion, INCLUSIVE.
//   3. `hovering` = inClip AND the mouse lies STRICTLY inside the layout rect
//      — of the BORDER element if there is one, else of this element. All four
//      comparisons are strict (`left < x < right`), which is a third convention
//      again after Contains' two.
//   4. write hover bit 17, plus the dead ancestor walk (see banner).
//   5. tooltip, gated on bit 16 (enabled) and bit 19 (valid): begin or move it
//      while hovering, end it when not. Bit 20 is "tooltip currently shown".
//   6. raise the hover-enter (id 0) / hover-leave (id 1) event from the OLD
//      bit 17, which is why `oldFlags` is captured before step 4.
//   7. the button/wheel events, or — if the state says a drag is active — the
//      drop event (id 0xc), a Play(3) and EndDragDrop, and nothing else.
//
// THE EVENT IDS ARE NOT SEQUENTIAL AND THE ORDER IS NOT THE STATE ORDER:
// state+0x01 -> 2, +0x02 -> 4, +0x03 -> 3, +0x04 -> 5, +0x05 -> 7, +0x06 -> 6.
// Transcribed one at a time from 0x18011663a..0x1801167e5; the temptation to
// write `id = 2 + i` would silently swap two of them.
//
// ORACLE: `pin`, and there is no honest alternative. The body pushes events
// into a live array, opens and closes tooltip dialogs and can call CloseDialog;
// running the shipped export a second time would double every one of those.
// Recorded as PORTED, NOT DIFFERENTIALLY VERIFIED.
extern "C" void RtUi_Element_Update(Element* self, const void* time, void* ui,
                                    void** dlgPtr, const UpdateState* state) {
    ++g_elemCounts.update;
    // ---- 1. children ------------------------------------------------------
    for (int32_t i = 0; i < self->_children._size; ++i) {
        Element* child = self->_children._data[i];
        if (!child) continue;
        if ((child->_flags & UI::kElementFlagHidden) != 0) continue;
        void* local = *dlgPtr;                       // re-read every iteration
        using Fn = void (*)(Element*, const void*, void*, void**, const UpdateState*);
        VSlot<Fn>(child, kSlotUpdate)(child, time, ui, &local, state);
    }

    UI::UserInterface* const u = static_cast<UI::UserInterface*>(ui);
    const int32_t mx = state->mouseX;
    const int32_t my = state->mouseY;

    // ---- 2. clip test (INCLUSIVE) -----------------------------------------
    const bool inClip = !(mx < u->_clipRegion.left || mx > u->_clipRegion.right ||
                          my < u->_clipRegion.top  || my > u->_clipRegion.bottom);

    // ---- 3/4. hover bit ---------------------------------------------------
    const uint32_t oldFlags = self->_flags;
    const Element* rect = self->_border ? reinterpret_cast<const Element*>(self->_border)
                                        : self;
    const int32_t l = rect->_layoutLeft, r = rect->_layoutRight;
    const int32_t t = rect->_layoutTop,  b = rect->_layoutBottom;

    bool hovering;
    if (l < mx && mx < r && t < my && my < b && inClip) {
        hovering = true;
        self->_flags = oldFlags | 0x00020000u;                 // BTS 0x11
        // DEAD (see banner); Ghidra itself folds this arm away.
        if ((((oldFlags | 0x00020000u) ^ oldFlags) & UI::kElementFlagHidden) != 0)
            NeedsSizeCalcWalk(self);
    } else {
        hovering = false;
        self->_flags = oldFlags & ~0x00020000u;                // BTR 0x11
        if ((((oldFlags & ~0x00020000u) ^ oldFlags) & UI::kElementFlagHidden) != 0)
            NeedsSizeCalcWalk(self);
    }

    // ---- 5. tooltip -------------------------------------------------------
    const uint32_t f = self->_flags;
    if ((f & UI::kElementFlagToolTip16) == 0) return;          // BT 0x10 / JNC exit

    if ((f & UI::kElementFlagToolTipValid) != 0) {             // BT 0x13
        void* const dlg = *dlgPtr;
        if (hovering && dlg && *(reinterpret_cast<const uint8_t*>(dlg) + 0x98) != 0) {
            EventRecord rec;
            rec.dialog    = dlg;
            rec.element   = self;
            rec.elementId = self->_elementId;
            rec.mouseX    = mx;
            rec.mouseY    = my;
            rec.dragId    = 0;
            rec.dragData  = nullptr;
            if ((f & 0x00100000u) == 0) {                      // BT 0x14 clear
                rec.eventId = 0xf;                            // tooltip BEGIN
                EventsSetSize()(&u->_events, u->_events._size + 1, &rec);
                void* dlgLocal  = *dlgPtr;
                void* elemLocal = self;
                RT_EXPORT_ONCE(BeginToolTipFn,
                    "?BeginToolTip@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@"
                    "V?$InternalPtr@VElement@UI@@@4@HH@Z")(u, &dlgLocal, &elemLocal, mx, my);
            } else {
                rec.eventId = 0x10;                           // tooltip MOVE
                EventsSetSize()(&u->_events, u->_events._size + 1, &rec);
                void* const tip = u->_toolTipDialog;
                if (tip) {
                    // y is offset by the tooltip ROOT ELEMENT's layout height
                    // (dlg+0x50 is the root, element+0x64 is _layoutHeight), so
                    // the tip sits ABOVE the cursor.
                    void* const root = *reinterpret_cast<void* const*>(
                        reinterpret_cast<const uint8_t*>(tip) + 0x50);
                    const int32_t h = *reinterpret_cast<const int32_t*>(
                        reinterpret_cast<const uint8_t*>(root) + 0x64);
                    RT_EXPORT_ONCE(SetUpperLeftFn,
                        "?SetUpperLeft@DialogInstance@UI@@QEAAXHH_N@Z")(tip, mx, my - h, false);
                }
            }
            const uint32_t o = self->_flags;
            self->_flags = o | 0x00100000u;                    // BTS 0x14
            if ((((o | 0x00100000u) ^ o) & UI::kElementFlagHidden) != 0)
                NeedsSizeCalcWalk(self);                       // DEAD

        } else if ((f & 0x00100000u) != 0) {
            RT_EXPORT_ONCE(EndHoverFn,
                "?EndHover@DialogInstance@UI@@QEAAXPEAVUserInterface@2@@Z")(*dlgPtr, u);
            void* const tip = u->_toolTipDialog;
            bool clear = false;
            if (!tip) {
                clear = true;
            } else if (self == *reinterpret_cast<Element* const*>(
                                   reinterpret_cast<const uint8_t*>(tip) + 0x30)) {
                // Retail re-tests `tip` here after already branching on it
                // (0x1801164fc). Kept: it is the body.
                if (tip) {
                    void* local = tip;
                    RT_EXPORT_ONCE(CloseDialogFn,
                        "?CloseDialog@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z")
                        (u, &local);
                }
                clear = true;
            }
            if (clear) {
                u->_toolTipDialog = nullptr;
                u->_toolTipOwner  = nullptr;
            }
            const uint32_t o = self->_flags;
            self->_flags = o & ~0x00100000u;                   // BTR 0x14
            if ((((o & ~0x00100000u) ^ o) & UI::kElementFlagHidden) != 0)
                NeedsSizeCalcWalk(self);                       // DEAD
        }
    }

    // ---- 6. hover enter / leave, from the OLD bit 17 ----------------------
    EventRecord rec;
    rec.dialog    = *dlgPtr;
    rec.element   = self;
    rec.elementId = self->_elementId;
    rec.mouseX    = mx;
    rec.mouseY    = my;
    rec.dragId    = 0;
    rec.dragData  = nullptr;

    const bool wasHovering = (oldFlags & 0x00020000u) != 0;
    if (!wasHovering) {
        if (!hovering) return;
        rec.eventId = 0;                                       // hover ENTER
        EventsSetSize()(&u->_events, u->_events._size + 1, &rec);
    } else if (!hovering) {
        rec.eventId = 1;                                       // hover LEAVE
        EventsSetSize()(&u->_events, u->_events._size + 1, &rec);
        return;
    }

    // ---- 7. the input events ---------------------------------------------
    auto push = [&](int32_t id) {
        rec.dialog    = *dlgPtr;
        rec.element   = self;
        rec.eventId   = id;
        rec.elementId = self->_elementId;
        rec.mouseX    = mx;
        rec.mouseY    = my;
        rec.dragId    = 0;
        rec.dragData  = nullptr;
        EventsSetSize()(&u->_events, u->_events._size + 1, &rec);
    };

    if (state->dragActive) {
        // A drop only happens on the button the drag was started with; retail
        // returns without raising ANY other event when +0x04 is clear.
        if (!state->b04) return;
        rec.dialog    = *dlgPtr;
        rec.element   = self;
        rec.eventId   = 0xc;                                   // DROP
        rec.elementId = self->_elementId;
        rec.mouseX    = mx;
        rec.mouseY    = my;
        rec.dragId    = u->_dragDropId;
        rec.dragData  = u->_dragDropData;
        EventsSetSize()(&u->_events, u->_events._size + 1, &rec);
        RT_EXPORT_ONCE(PlayFn, "?Play@UserInterface@UI@@QEAAXH@Z")(u, 3);
        RT_EXPORT_ONCE(EndDragDropFn, "?EndDragDrop@UserInterface@UI@@QEAAXXZ")(u);
        return;
    }

    if (state->buttonPressed) push(2);
    if (state->b02)           push(4);
    if (state->b03)           push(3);
    if (state->b04)           push(5);
    if (state->b05)           push(7);
    if (state->b06)           push(6);
    if (state->leftDown && (state->dragDx != 0 || state->dragDy != 0)) push(0xb);
}

// ============================================================================
// Registration
// ============================================================================
// BOTH DOORS ON EVERY ROW, for the reason spritesheet_vtable.cpp:262 gives: a
// minted vtable slot is filled by NAME, but sites in this tree reach exported
// code by ADDRESS, and a name-only registration leaves those walking past the
// port into the DLL.
//
// Each mangled name is spelled as a LITERAL here even where a constant exists
// above, so tools/runtime_census.py can see it
// (reference_census_cannot_see_nonliteral_registrations.md).
int RtUiElementVtableRegister() {
    int n = 0;

    RegisterRuntimeExport("?TypeName@Element@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_Element_TypeName); ++n;
    RegisterRuntimeAddr(0x180035440, (void*)&RtUi_Element_TypeName);

    RegisterRuntimeExport("?Type@Element@UI@@UEBAIXZ",
                          (void*)&RtUi_Element_Type); ++n;
    RegisterRuntimeAddr(0x180035420, (void*)&RtUi_Element_Type);

    RegisterRuntimeExport("?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z",
                          (void*)&RtUi_Element_Persist); ++n;
    RegisterRuntimeAddr(0x180116870, (void*)&RtUi_Element_Persist);

    RegisterRuntimeExport(
        "?Update@Element@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@"
        "V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@12@@Z",
        (void*)&RtUi_Element_Update); ++n;
    RegisterRuntimeAddr(0x180116270, (void*)&RtUi_Element_Update);

    RegisterRuntimeExport("?Enable@Element@UI@@UEAAX_N@Z",
                          (void*)&RtUi_Element_Enable); ++n;
    RegisterRuntimeAddr(0x180116910, (void*)&RtUi_Element_Enable);

    RegisterRuntimeExport("?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z",
                          (void*)&RtUi_Element_Contains); ++n;
    RegisterRuntimeAddr(0x1801169e0, (void*)&RtUi_Element_Contains);

    RegisterRuntimeExport("?SetColor@Element@UI@@UEAAXI@Z",
                          (void*)&RtUi_Element_SetColor); ++n;
    RegisterRuntimeAddr(0x180035620, (void*)&RtUi_Element_SetColor);

    RegisterRuntimeExport("?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ",
                          (void*)&RtUi_Element_GetBorder); ++n;
    RegisterRuntimeAddr(0x180035630, (void*)&RtUi_Element_GetBorder);

    return n;
}
