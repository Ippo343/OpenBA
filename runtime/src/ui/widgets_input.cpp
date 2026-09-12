// ============================================================================
// runtime/src/ui/widgets_input.cpp — Wave 7 lane D — Spinner/Combo/NumberEdit/Number/Edit/Slider/Radio/Progress
// ============================================================================
//
// Ports 17 of the 27 UI::*Element exports this lane owns (PLAN.md §4.2 wave 7,
// §6). Layouts and their evidence are in runtime/types/ui/{SpinnerElement,
// ComboElement,NumberEditElement,NumberElement,EditElement,SliderElement,
// RadioElement,ProgressElement}.h. The full function table, the oracle
// argument per function, everything held back and why, and the
// `EditElement::SetText` phantom-name resolution are in
// docs/runtime_port/WAVE7_LANE_D.md.
//
// Not run by this lane, by instruction: the game, `make`, `tools/rt_run.sh`,
// `sa-run`, `git commit`. Every number in the lane doc is a *prediction of
// what the oracle will report*, not a measurement.
//
// ---------------------------------------------------------------------------
// THE SHARED "FIRST CHILD" PATTERN
// ---------------------------------------------------------------------------
// SpinnerElement, SliderElement and ProgressElement's own exports do not hold
// a dedicated pointer to the widget they wrap — they read
// `Element::_children._data[0]` (the FIRST child in the ordinary, inherited
// Element child array; Element.h, lane A) and IsDerivedFrom-check it before
// using it. There is NO NULL CHECK on `_children._data` itself in retail, and
// none is added here: a SliderElement (whose _children is empty) faults on
// that load in retail too — CLAUDE.md's "don't fix a faithful crash" rule.
//
// ---------------------------------------------------------------------------
// THE ORACLE
// ---------------------------------------------------------------------------
// Two shapes, argued per function in WAVE7_LANE_D.md §2:
//
//  (a) REPLAY for the seven pure getters: call ours, call the DLL's own body
//      on the SAME (unmodified) object, RT_DIFF_SCALAR the results. Safe
//      because nothing here writes.
//
//  (b) PREFIX REPLAY for the ten mutators: memcpy the live object into a
//      same-sized LOCAL scratch of this lane's own struct BEFORE our body
//      runs, run our body on the real object, run the DLL's original on the
//      scratch copy, and RT_DIFF_BYTES over sizeof(OurStruct). Because both
//      copies start byte-identical (a real memcpy of the SAME live bytes, not
//      a freshly-constructed guess), ANY divergence in that range is a real
//      behavioural difference regardless of how many of those bytes this lane
//      has NAMED — same argument WAVE6_LANE_A.md gives for Video::Context.
//      The one caveat, same shape as that lane's AddVertex note: where the
//      mutator reaches a CHILD object through a shared pointer (Spinner's
//      wrapped NumberEditElement, Radio's checkbox array), the scratch copy's
//      pointer is the SAME address, so replaying the original touches the
//      REAL child a second time. Every case here is checked to be idempotent
//      under a repeated identical call before being ported this way; see
//      WAVE7_LANE_D.md §2 for the per-function argument.
//
//      NOT done this way: anything that swaps a REFCOUNTED pointer (a
//      shared string, a shared allocation). Replaying the original there
//      would decrement/increment the SAME refcount a second time — wrong
//      regardless of whether the first decrement freed it — which is exactly
//      why `EditElement::SetText` is a documented NO-ORACLE port rather than
//      a prefix-replay one (WAVE7_LANE_D.md §5).
//
//      *** THAT RULE WAS BROKEN IN THREE PLACES IN THIS FILE, and Wave 9
//      lanes M and N found it the expensive way — the oracle handed the DLL a
//      pointer our own call had already released, on every diff run of
//      tutorials 1-3. The three were
//      NumberElement::SetValue (both overloads), which reach the private
//      NumberElement::CacheText, and NumberEditElement::SetMinMax, which
//      reaches EditElement::SetText. All three are now NO-ORACLE ports; the
//      full evidence is in the block above RtUi_NumberElement_SetValueInt and
//      in docs/runtime_port/WAVE9_LANE_N.md. The rule was broken because
//      NumberElement.h called the owning pointer at +0xD0 "padding, not
//      reversed by this lane" — a layout gap is not a documentation problem
//      when an oracle decision is taken from the header.
//
//      THE SWEEP (lane N, 2026-09-08). Every prefix replay in this file and in
//      the rest of runtime/src was re-checked against ONE criterion: does the
//      replayed body release or reassign an owning refcounted pointer that is
//      COPIED INTO the scratch? Reaching such a pointer through a shared
//      pointer to a LIVE object is safe — the refcounts stay correct and the
//      work is merely done twice (the idempotency argument this block already
//      makes). Holding a COPY of the pointer is not: ours may already have
//      freed it. Results:
//        * SpinnerElement::SetMinMax / SetValue — SAFE. They reach the
//          refcounted `_text` through `_children._data[0]`, a live child, not
//          through a copied field. SpinnerElement itself is {Element, int}.
//        * SpinnerElement::SetElementId / SetIncrement, RadioElement::
//          SetActiveRadio, ProgressElement::SetMax / SetCurrent — SAFE. No
//          refcounted field is released by any of those bodies.
//        * Element's ReplayGuard family (element.cpp) — SAFE; the two bodies
//          that DO touch a refcounted string (SetName, SetToolTipString) were
//          already shadow oracles with the same reasoning.
//        * widgets_layout.cpp's TextElement::SetParameter / SetText — already
//          NO-ORACLE, naming CacheText's refcount management as the reason.
//        * sprite.cpp's replay* family — already balances its extra reference
//          with ReleaseStringRef.
//      Nothing else in runtime/src prefix-replays an object that owns a
//      System::String. The three fixed here were the whole set.
//
//      sizeof(OurStruct) is a LOWER BOUND on the true retail object (this
//      lane has not reversed every byte of any of its eight classes), so the
//      oracle proves everything IN that range and is silent about anything
//      past it — stated once here rather than on every function.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/system/String.h"
#include "../../types/ui/SpinnerElement.h"
#include "../../types/ui/ComboElement.h"
#include "../../types/ui/NumberEditElement.h"
#include "../../types/ui/NumberElement.h"
#include "../../types/ui/EditElement.h"
#include "../../types/ui/RadioElement.h"
#include "../../types/ui/ProgressElement.h"
// WAVE 9 LANE UIW2. SliderElement/ProgressElement/ComboElement all write
// into a wrapped TEXT widget's own parameter array, so this lane uses lane
// E's TextElement offsets rather than making a second, independent reading
// of them. Sprite.h is for ComboElement::UpdateText's 16-byte sprite copy.
#include "../../types/ui/TextElement.h"
#include "../../types/ui/Sprite.h"
// WAVE 9 LANE UIW2 PART 2. The String primitives the two newly ported
// address-door bodies call as ordinary C++ rather than through a door:
// RtString_StrCompare, RtString_ReleaseInline and RtSystem_String_Assign.
#include "../system/string.h"

using UI::Element;
using UI::SpinnerElement;
using UI::ComboElement;
using UI::NumberEditElement;
using UI::NumberElement;
using UI::EditElement;
using UI::RadioElement;
using UI::ProgressElement;
using UI::kElementFlagNeedsSizeCalc;

// ----------------------------------------------------------------------------
// ResolveAddr / ResolveExport, forward-declared rather than pulled in from
// standalone/resolve.h — same reasoning rt_port.h gives for declaring
// RegisterRuntimeExport locally (windows.h leakage; PLAN §4.1 platform
// neutrality). These declarations must stay byte-identical to resolve.h's.
// ----------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// ============================================================================
// Dependencies this lane calls rather than reimplements.
// ============================================================================

// `?IsDerivedFrom@Base@Object@@QEBA_NI@Z` (0x1800c0390, ord 3110, exported).
// Every "first child" lookup below uses it exactly as retail does.
using IsDerivedFromFn = bool (*)(void* self, unsigned typeId);
IsDerivedFromFn IsDerivedFrom() {
    // ResolveExport, NOT ResolveExportRaw (changed 2026-09-07, Wave 9 lane R).
    // Raw skips the port registry on purpose, for the diff oracle; on a LIVE
    // path it means every "first child" lookup in this file ran the DLL's body
    // while Wave 3's port of IsDerivedFrom sat registered and unreached.
    static IsDerivedFromFn fn = reinterpret_cast<IsDerivedFromFn>(
        ResolveExport("?IsDerivedFrom@Base@Object@@QEBA_NI@Z"));
    return fn;
}

// The cached NumberEditElement type id every SpinnerElement export reads as
// `dword ptr [0x1801fa190]` (disassembly of 0x1800d7cb0 and siblings) — a
// plain Runtime .data global, not a function; read directly via ResolveAddr
// exactly like the exe side's `g_TypeId_*` exrefs read the equivalent slot.
// Retail never lazily initialises this before reading it, so this lane does
// not either — reading it at the same point in the call retail would is
// faithful, not a race.
uint32_t NumberEditElementTypeId() {
    static uint32_t* p = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Runtime, 0x1801fa190));
    return p ? *p : 0;
}

// `?ToInteger@String@System@@SAHPEBD@Z` (0x18003f620, ord 4137, exported,
// static). Takes the raw char* a System::String IS at the ABI level in this
// engine (see EditElement.h), not a String object.
using ToIntegerFn = int (*)(const char*);
// ResolveExport, not ResolveExportRaw, since Wave 9 lane B3: String::ToInteger
// is now ported (runtime/src/system/string.cpp) and the raw door would walk
// past it into the DLL -- the port-bypass shape WAVE9_LANE_V.md §1c catalogues.
ToIntegerFn StringToInteger() {
    static ToIntegerFn fn = reinterpret_cast<ToIntegerFn>(
        ResolveExport("?ToInteger@String@System@@SAHPEBD@Z"));
    return fn;
}

// `?SetValue@NumberEditElement@UI@@QEAAXH@Z` (0x1800d4bb0), used as a
// dependency by SpinnerElement::SetMinMax/SetValue and
// NumberEditElement::SetMinMax.
//
// **THE HOLDBACK IS OVER, AND THE STATED BLOCKER WAS ALREADY STALE.** Wave 7
// lane D held this body back because it calls the variadic
// `System::String::Format`, "which this lane declined to reproduce"
// (WAVE7_LANE_D.md §3). `?Format@String@System@@SA?AV12@PEBDZZ` has in fact
// been PORTED since Wave 2 (runtime/src/system/string.cpp registers it), so the
// blocker had already dissolved before Wave 9 opened -- the same shape as
// reference_holdback_blockers_are_stale_check_them_first.md, three for three in
// one session. It is ported below as NumberEditElement_SetValue; this accessor
// stays as the name the three call sites already use and now goes through
// ResolveExport so it reaches OUR body rather than bypassing it.
using NumberEditSetValueFn = void (*)(NumberEditElement*, int);
NumberEditSetValueFn RealNumberEditElementSetValue() {
    static NumberEditSetValueFn fn = reinterpret_cast<NumberEditSetValueFn>(
        ResolveExport("?SetValue@NumberEditElement@UI@@QEAAXH@Z"));
    return fn;
}

// ?SetText@EditElement@UI@@QEAAXAEBVString@System@@@Z  (0x1800d0db0) -- exported,
// not ported by any wave; SetValue's second call. Resolved through the port
// registry so a later port of it is reached without editing this line.
using EditSetTextFn = void (*)(void*, const RtSystem::String*);
EditSetTextFn EditElementSetText() {
    static EditSetTextFn fn = reinterpret_cast<EditSetTextFn>(
        ResolveExport("?SetText@EditElement@UI@@QEAAXAEBVString@System@@@Z"));
    return fn;
}

// ---------------------------------------------------------------------------
// WAVE 9 LANE UIW2, PART 2 — two of the tree's unregistered ADDRESS doors, both
// in this file, both now PORTED rather than resolved.
// ---------------------------------------------------------------------------
//
// An unregistered address door is the WORST failure shape at cutover: no
// `[RtUnresolved]` line, no named residue row, just a call through null. The
// integrator's triage named three of them in this cluster; two were these, at
// what were lines 220 and 232 of this file.
//
// *** BOTH SITES WERE JUSTIFIED BY A COMMENT THAT WAS FACTUALLY WRONG, AND THE
// WRONGNESS IS WHY THEY WERE ADDRESS DOORS AT ALL. *** The old text said
// 0x1801189f0 was "an unexported private helper" and that for 0x18003ffe0
// "neither showed up as an exported mangled name in this lane's pefile check".
// Re-run against the shipped export directory, 2026-09-08, both ARE exported:
//
//     0x1801189f0  ?CacheText@NumberElement@UI@@AEAAXXZ      ord 1679
//     0x18003ffe0  ??9String@System@@QEBA_NAEBV01@@Z         ord 1290
//     0x1800da130  ?AddTab@TabElement@UI@@QEAA?AV…@Z         ord 1589  (declined, see the lane doc)
//
// `AEAA` means PRIVATE, not unexported — this DLL exports its private members,
// which is exactly why `?CacheText@TextElement@UI@@AEAAXXZ` a few lines above
// resolves by NAME and works. A private method and an unexported one look the
// same in a decompiler and are not the same thing in an export table.
//
// This is the same defect as the `?_typeInfo@` finding in
// docs/runtime_port/WAVE9_LANE_UIW2.md §3: an address door on a symbol with a
// perfectly good name, protected from review by a comment asserting it has
// none. Neither site is converted to a name door — a converted door on an
// UNPORTED symbol only changes which list it prints in
// (reference_converting_a_door_is_not_satisfying_it.md). Both are PORTED, so
// the door closes rather than moves.

// ?CacheText@NumberElement@UI@@AEAAXXZ (0x1801189f0) — PORTED below as
// RtUi_NumberElement_CacheText. Both NumberElement::SetValue bodies call it.
// This forwarder keeps the name the two call sites already use.
void RtUi_NumberElement_CacheText(void* self);
using CacheTextFn = void (*)(void*);
inline CacheTextFn NumberElementCacheText() { return &RtUi_NumberElement_CacheText; }

// ??9String@System@@QEBA_NAEBV01@@Z (0x18003ffe0) — System::String::operator!=,
// PORTED below as StringNotEqualImpl. Its only consumer tree-wide is
// RtUi_EditElement_SetText in this file.
//
// REGISTERED under both doors, and not counted in the UI tally. The body
// belongs to `System`, and runtime/src/system/string.cpp is where it should
// eventually live -- see the registration comment for the hand-over note and
// for why leaving it unregistered was measured and rejected. Deleting the DOOR
// is what closes the cutover hole, and that is done either way.



bool StringNotEqualImpl(const void* lhs, const void* rhs);
using StringNotEqualFn = bool (*)(const void*, const void*);
inline StringNotEqualFn StringNotEqual() { return &StringNotEqualImpl; }

// The TRUE size of a UI::NumberElement, and it is NOT `sizeof(NumberElement)`.
//
// `runtime/types/ui/NumberElement.h` says in its own text that the object is
// **0xE8** bytes — `CreateNewObject` (0x18004a910) calls
// `MemoryAllocator::Allocate(_singleton, 0xE8, 8)` — and that an earlier 0xE0
// struct made the DLL's `CacheText` read `+0xE0`, the `_showSign` byte, off the
// end of a stack replay and choose its format string from garbage. **The struct
// in that header was never widened**: its last member is `_floatValue` at
// +0xDC, so `sizeof(NumberElement)` is still 0xE0 and using it for the replay
// below would re-create exactly that defect, in exactly that function.
//
// This lane does NOT widen the header — adding trailing padding to a shared
// binary-compatible struct mid-wave is a change other lanes would inherit
// without asking for it. It uses the documented size here and asserts the
// relationship, so if the header is ever widened this constant is checked
// against it rather than silently diverging.
constexpr size_t kNumberElementTrueSize = 0xE8;
static_assert(sizeof(NumberElement) <= kNumberElementTrueSize,
              "NumberElement grew past the 0xE8 CreateNewObject allocates");
}  // namespace  (reopened immediately below)

// `?Format@String@System@@SA?AV12@PEBDZZ` (0x1800c9730) — PORTED in Wave 2 and
// registered by address in runtime/src/system/string.cpp, but NOT exposed by
// runtime/src/system/string.h (no caller outside that file needed it until
// now). Declared locally rather than by editing that lane's header, exactly as
// runtime/src/io/filesystem_win.cpp:70-82 already does and for the same stated
// reason. This declaration must stay identical to string.cpp's definition.
namespace RtSystem {
String* RtSystem_String_Format(String* ret, const char* fmt, ...);   // 0x1800c9730
}  // namespace RtSystem

namespace {

// `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` (0x1800c7ad0) — the static free
// EditElement::SetText itself calls. Resolved by ADDRESS, and that address IS
// registered (the Wave 2 allocator port), so this is not one of the doors
// lane UIW2 had to close.
using MemFreeFn = void (*)(void*);
MemFreeFn MemFreeStatic() {
    // Retry a miss rather than caching it — at cutover this address arrives
    // with the allocator's arming path. See runtime/src/collection/array.cpp.
    static MemFreeFn fn = nullptr;
    if (!fn) fn = reinterpret_cast<MemFreeFn>(ResolveAddr(Module::Runtime, 0x1800c7ad0));
    return fn;
}

// ----------------------------------------------------------------------------
// WAVE 9 LANE UIW — the Collection::Array entry points ComboElement and
// RadioElement reach.
//
// Called BY ADDRESS, exactly as retail does, and that lands on OUR ports rather
// than the DLL because `ResolveAddr` consults the port registry first
// (reference_resolveaddr_consults_port_registry_first.md). All five addresses
// used below are registered in runtime/src/collection/array.cpp by this same
// lane, so after cutover they resolve to our bodies instead of to null.
//
// SEVEN NEARLY-IDENTICAL FUNCTIONS INSTEAD OF ONE TEMPLATE, and the repetition
// is the point. The first draft here was `ArraySetSizeFillAt<0x180034860>()`,
// which is shorter, gives each address its own cache for free, and is WRONG for
// this tree: `tools/port_bypass_audit.py` and `tools/runtime_census.py` scan the
// SOURCE for the literal `ResolveAddr(Module::Runtime, 0x…)`, so a template
// parameter makes every one of these call sites invisible to both. That is the
// same blind spot that hid twelve live ports for a full wave
// (reference_census_cannot_see_nonliteral_registrations.md), manufactured here
// by a lane rather than inherited. Spelled out, each site is countable.
//
// One cache PER ADDRESS is also load-bearing: a single shared static would
// resolve whichever address was asked for first and then hand that same pointer
// to every other site — which would produce a combo box that appends sprites
// into the string array and looks like it works.
//
// CACHE A HIT, RETRY A MISS (the RtResolveCached contract): at cutover these
// arrive with the Collection port's init, and a caller that ran earlier would
// otherwise freeze a null in for the life of the process.
// ----------------------------------------------------------------------------
using ArraySetSizeFillFn = void (*)(void*, int, const void*);
using ArrayCleanupFn     = void (*)(void*);

// ComboElement::_strings — Array<System::String>::SetSize(n, &fill)
ArraySetSizeFillFn ArraySetSizeFill_180034860() {
    static ArraySetSizeFillFn fn = nullptr;
    if (!fn) fn = reinterpret_cast<ArraySetSizeFillFn>(ResolveAddr(Module::Runtime, 0x180034860));
    return fn;
}
// ComboElement::_sprites — Array<UI::Sprite>::SetSize(n, &fill), stride 0x10
ArraySetSizeFillFn ArraySetSizeFill_18004bfc0() {
    static ArraySetSizeFillFn fn = nullptr;
    if (!fn) fn = reinterpret_cast<ArraySetSizeFillFn>(ResolveAddr(Module::Runtime, 0x18004bfc0));
    return fn;
}
// ComboElement::_userData — Array<void*>::SetSize(n, &fill)
ArraySetSizeFillFn ArraySetSizeFill_180024fe0() {
    static ArraySetSizeFillFn fn = nullptr;
    if (!fn) fn = reinterpret_cast<ArraySetSizeFillFn>(ResolveAddr(Module::Runtime, 0x180024fe0));
    return fn;
}
// RadioElement::_checkBoxes — Array<InternalPtr<Element>>::SetSize(n, &fill)
ArraySetSizeFillFn ArraySetSizeFill_180048fe0() {
    static ArraySetSizeFillFn fn = nullptr;
    if (!fn) fn = reinterpret_cast<ArraySetSizeFillFn>(ResolveAddr(Module::Runtime, 0x180048fe0));
    return fn;
}
// ComboElement::_strings — the String-array cleanup (destroy every element,
// free the buffer, zero the header). NOT `~Array`: the elements are refcounted.
ArrayCleanupFn ArrayCleanup_180010ab0() {
    static ArrayCleanupFn fn = nullptr;
    if (!fn) fn = reinterpret_cast<ArrayCleanupFn>(ResolveAddr(Module::Runtime, 0x180010ab0));
    return fn;
}
// ComboElement::_sprites — ~Array (J1)
ArrayCleanupFn ArrayCleanup_180037310() {
    static ArrayCleanupFn fn = nullptr;
    if (!fn) fn = reinterpret_cast<ArrayCleanupFn>(ResolveAddr(Module::Runtime, 0x180037310));
    return fn;
}
// ComboElement::_userData — ~Array (J1)
ArrayCleanupFn ArrayCleanup_180024aa0() {
    static ArrayCleanupFn fn = nullptr;
    if (!fn) fn = reinterpret_cast<ArrayCleanupFn>(ResolveAddr(Module::Runtime, 0x180024aa0));
    return fn;
}

// Resolve SpinnerElement/SliderElement/ProgressElement's wrapped "value"
// widget: Element::_children._data[0], cast to T only if IsDerivedFrom(typeId)
// agrees, else null — exactly retail's shape. NO NULL CHECK on `_data` itself:
// an empty _children array (a SliderElement passed where a SpinnerElement was
// expected, say) faults here in retail too.
template <typename T>
inline T* FirstChildAs(Element* self, uint32_t typeId) {
    Element* first = self->_children._data[0];
    if (IsDerivedFrom()(first, typeId)) return reinterpret_cast<T*>(first);
    return nullptr;
}

// The dirty-flag parent walk every SetX below that touches display state
// performs: `do { p->_flags |= NeedsSizeCalc; p = p->_parent; } while (p);`
// Written once here since five different functions share the exact shape.
inline void MarkNeedsSizeCalcUpToRoot(Element* self) {
    for (Element* p = self; p != nullptr; p = p->_parent) {
        p->_flags |= kElementFlagNeedsSizeCalc;
    }
}

// ---- Mangled names, spelled out again at each registration site rather than
// reused from these constants: runtime_census.py cannot see through a named
// constant (reference_census_cannot_see_nonliteral_registrations.md). These
// exist only for the diff oracle's RT_ORIGINAL_ONCE lookups. ----------------
const char* const kSpinnerGetMaxValue  = "?GetMaxValue@SpinnerElement@UI@@QEBAHXZ";
const char* const kSpinnerGetValue     = "?GetValue@SpinnerElement@UI@@QEBAHXZ";
const char* const kSpinnerIsActive     = "?IsActive@SpinnerElement@UI@@QEBA_NXZ";
const char* const kSpinnerSetElementId = "?SetElementId@SpinnerElement@UI@@QEAAXH@Z";
const char* const kSpinnerSetIncrement = "?SetIncrement@SpinnerElement@UI@@QEAAXH@Z";
const char* const kSpinnerSetMinMax    = "?SetMinMax@SpinnerElement@UI@@QEAAXHH@Z";
const char* const kSpinnerSetValue     = "?SetValue@SpinnerElement@UI@@QEAAXH@Z";
const char* const kComboGetCurrentItem = "?GetCurrentItem@ComboElement@UI@@QEBAHXZ";
const char* const kComboGetItemCount   = "?GetItemCount@ComboElement@UI@@QEBAHXZ";
const char* const kComboGetItemData    = "?GetItemData@ComboElement@UI@@QEBAPEAXH@Z";
const char* const kNumEditGetValue     = "?GetValue@NumberEditElement@UI@@QEBAHXZ";
const char* const kNumEditSetMinMax    = "?SetMinMax@NumberEditElement@UI@@QEAAXHH@Z";
const char* const kNumberSetValueInt   = "?SetValue@NumberElement@UI@@QEAAXH@Z";
const char* const kNumberSetValueFloat = "?SetValue@NumberElement@UI@@QEAAXM@Z";
const char* const kEditSetText         = "?SetText@EditElement@UI@@QEAAXAEBVString@System@@@Z";
const char* const kRadioSetActiveRadio = "?SetActiveRadio@RadioElement@UI@@QEAAXH@Z";
const char* const kProgressSetMax      = "?SetMaximumValue@ProgressElement@UI@@QEAAXM@Z";
const char* const kProgressSetCurrent  = "?SetCurrentValue@ProgressElement@UI@@QEAAXM@Z";

// ============================================================
// UI::NumberEditElement::SetValue  retail 0x1800d4bb0  (Wave 9 lane B3)
// ============================================================
//
//   int v = this->_min;                        // +0xF8
//   if (value >= this->_min) {
//       v = value;
//       if (value > this->_max) v = this->_max;    // +0xFC, CMOVG
//   }
//   String tmp; String::Format(&tmp, "%i", v);     // 0x1800c9730, literal 0x180154848
//   EditElement::SetText(this, tmp);               // 0x1800d0db0
//   tmp.Release();
//
// The clamp is written the way retail branches it rather than as
// `std::clamp(value, min, max)`, because the two are NOT the same function when
// `_min > _max`: retail's below-min branch (0x1800d4bcc `JL`) returns `_min`
// without consulting `_max` at all, so a table configured min=10 max=5 answers
// 10 for `value=0` and 5 for `value=20`. A clamp() would answer 5 for both.
// Nothing in the game is known to configure an inverted range; the point is
// that the port must not decide that for it.
//
// The format literal at 0x180154848 reads "%i" (bytes 25 69 00), confirmed by
// direct memory read against the shipped DLL rather than assumed from context.
//
// NO ORACLE. SetText mutates the element's text, rebuilds its cached sprite
// text and can resize the widget; calling ours then theirs would run all of
// that twice on the one live element. There is no independent replica to
// build -- a scratch NumberEditElement would need a live UI parent, a sprite
// context and a string table. The site below is an EXECUTION WITNESS only --
// see the comment at it for why a shadow predicate over the clamp would have
// been a tautology rather than a check. This body is UNVERIFIED; what its
// diff row establishes is that it ran, and how often.
void NumberEditElement_SetValue(NumberEditElement* self, int value) {
    RT_DIFF_BEGIN("UI::NumberEditElement::SetValue [no oracle: SetText mutates the live element]");

    int clamped = self->_min;
    if (value >= self->_min) {
        clamped = value;
        if (value > self->_max) clamped = self->_max;
    }

    using FormatFn = RtSystem::String* (*)(RtSystem::String*, const char*, int);
    RtSystem::String tmp{nullptr};
    RT_EXPORT_ONCE(FormatFn, "?Format@String@System@@SA?AV12@PEBDZZ")(&tmp, "%i", clamped);

    EditElementSetText()(self, &tmp);

    // retail's inlined String::Release, the asymmetric one (runtime/types/
    // system/String.h): the freeing branch leaves `data - 8` in the object.
    using ReleaseFn = void (*)(RtSystem::String*);
    RT_EXPORT_ONCE(ReleaseFn, "?Release@String@System@@AEAAXXZ")(&tmp);

    // An EXECUTION WITNESS, not a verdict. A shadow predicate here would have
    // to be a second spelling of the clamp three lines above, and two spellings
    // of the same arithmetic agree by construction -- N green rows that did no
    // work, which is exactly what
    // reference_probe_needs_an_effectiveness_control.md says not to ship. This
    // site therefore claims only that the body RAN, and its calls= column is
    // the number worth reading. Same shape and same honesty as
    // Application::SleepThread in runtime/src/application/application_win.cpp.
    RT_DIFF_SHADOW(true, "set", "not diffable");
}


// ============================================================================
// WAVE 9 LANE UIW2 — dependencies for ComboElement::SetCurrentItem/UpdateText
// and the two "parameter into the wrapped TextElement" setters.
// ============================================================================
//
// EVERY ONE OF THESE REUSES A DOOR THE TREE ALREADY HAD. That is what makes the
// set-diff in docs/runtime_port/WAVE9_LANE_UIW2.md come out the right way:
// three exported names close and NO new unsatisfied name and NO new
// unregistered address open. If you add a dependency here, re-run
// tools/unsatisfied_export_scan.py and tools/addr_door_audit.py and put the
// delta in that document — a lane that closes 3 doors and opens 4 has gone
// backwards, and only the set-diff can tell you which happened.

// ?CacheText@TextElement@UI@@AEAAXXZ (0x18011ec10) — TextElement's OWN
// CacheText, the SHARED base implementation. NOT NumberElement's private
// 0x1801189f0 above; Wave 7 lane D disambiguated the two with get_xrefs_to and
// this file already carries that note at NumberElementCacheText(). Not ported
// by any wave (its remaining callee is ?SetParameterCount@TextElement@
// 0x18011e8e0), so it is a door that resolves to null at cutover — but NOT A
// NEW ONE: runtime/src/ui/widgets_layout.cpp:102 already opened it under this
// exact mangled name for TextElement::SetParameter/SetText. Spelling it
// identically keeps the two on ONE row of tools/unsatisfied_export_scan.py.
using TextCacheTextFn = void (*)(void*);
inline TextCacheTextFn TextElementCacheText() {
    return RT_EXPORT_ONCE(TextCacheTextFn, "?CacheText@TextElement@UI@@AEAAXXZ");
}

// ??4String@System@@QEAAAEAV01@AEBV01@@Z (0x1800c9810) — System::String's
// refcounted assignment, PORTED in Wave 2 (runtime/src/system/string.cpp
// registers it under both name and address). Reached through the port registry
// so this file does not walk past our own body into the DLL's
// (reference_bypassed_port_needs_a_source_checker.md). Its post-condition,
// `dst->data == src->data`, is what the shadow predicates below assert.
using StringAssignFn = void* (*)(void*, const void*);
inline StringAssignFn StringAssign() {
    return RT_EXPORT_ONCE(StringAssignFn, "??4String@System@@QEAAAEAV01@AEBV01@@Z");
}

// ?FindElementType@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z
// (0x180115760) — PORTED in Wave 7 lane A as RtUi_Element_FindElementType
// (runtime/src/ui/element.cpp, registered under name AND address). EXACT
// type-id equality, not IsDerivedFrom.
//
// *** `QEAA`, NOT `QEBA`, AND THIS LANE GOT IT WRONG FIRST. *** The first draft
// spelled it `QEBA` (const member) on the assumption that a finder is const. It
// is not, and `?FindElementType@Element@UI@@QEBA…` IS NOT IN THE EXPORT TABLE —
// it would have resolved to NULL in silence, the port would still have
// registered, and the census would still have counted it
// (reference_resolveexport_phantom_names.md, the same defect Wave 7 lane B
// found twice in one file).
//
// WHAT CAUGHT IT WAS THE SET-DIFF, NOT REVIEW. tools/unsatisfied_export_scan.py
// went 129 -> 127 where 126 was expected, and the entered/left comparison named
// the one symbol that had appeared: this typedef's own phantom. A lane that had
// only read the "-2, good, went down" headline would have shipped it. That is
// the whole argument for diffing the SET rather than the COUNT, and it is why
// the header of this block asks the next person to re-run both audits.
//
// The hidden return slot IS real here and is in the typedef: `?AV?$InternalPtr@…`
// returns by value, so the second argument is the return slot and the type id is
// third. tools/abi_retslot_audit.py is what enforces that
// (reference_hidden_retslot_wild_write.md).
using FindElementTypeFn = void* (*)(void* self, void** ret, uint32_t typeId);
inline FindElementTypeFn FindElementType() {
    return RT_EXPORT_ONCE(FindElementTypeFn,
        "?FindElementType@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z");
}

// ?CloseDialog@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z
// (0x180120f20) — NOT ported. WAVE9_LANE_UIF.md row 4 declined it at depth 3
// and that verdict stands; this lane re-checked the blocker rather than
// inheriting it (reference_holdback_blockers_are_stale_check_them_first.md) and
// it is still real. The DOOR, however, is engine/funcs/engine_funcs.cpp:1607's
// and is reused verbatim, so SetCurrentItem adds no second row for it.
//
// ABI: the InternalPtr<DialogInstance> is passed BY ADDRESS even though the
// mangled name spells it by value — InternalPtr<T> has a non-trivial copy
// constructor, so MSVC x64 passes it indirectly. Retail's `LEA RDX,[RSP+0x30]`
// at 0x180112564 is that indirection, and engine_funcs.h's typedef records the
// same conclusion from the other direction.
using CloseDialogFn = void (*)(void* ui, void* dialogPtrSlot);
inline CloseDialogFn CloseDialog() {
    return RT_EXPORT_ONCE(CloseDialogFn,
        "?CloseDialog@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z");
}

// The two type ids these bodies read — AND THE FINDING THAT MADE THIS LANE
// CHEAP. Both are EXPORTED DATA. Checked with pefile against the shipped export
// directory, 2026-09-08:
//
//   0x1801faee8  ?_typeInfo@TextElement@UI@@0VBaseTypeInfo@Object@@A        ord 4922
//   0x1801fa040  ?_typeInfo@LabelElement@UI@@0VBaseTypeInfo@Object@@A       ord 4877
//   0x1801fa190  ?_typeInfo@NumberEditElement@UI@@0VBaseTypeInfo@Object@@A  ord 4886
//
// All three are already carried by lane C2's mint, so a NAMED read costs
// nothing at cutover while an ADDRESS read books a row in the unregistered
// -address bucket. The sibling accessor NumberEditElementTypeId() above resolves
// its slot by ADDRESS (0x1801fa190) and therefore pays that cost for a symbol
// that has a perfectly good name. This lane does NOT change that live accessor —
// retuning another lane's working call site is not this lane's to do — but it is
// recorded here and in WAVE9_LANE_UIW2.md as a one-line fix at source
// (PLAN §6 step 8), because the next person to copy the idiom from this file
// should copy the named form.
//
// The FIRST DWORD of a BaseTypeInfo is the type id: the same reading
// runtime/src/achievement/achievement_manager.cpp:274 uses, and the one retail's
// own `MOV EDX,dword ptr [0x1801faee8]` at 0x1800d6cb9 proves.
inline uint32_t TextElementTypeId() {
    const uint32_t* p = RT_DATA_SLOT(const uint32_t*,
        "?_typeInfo@TextElement@UI@@0VBaseTypeInfo@Object@@A");
    return p ? *p : 0u;
}
inline uint32_t LabelElementTypeId() {
    const uint32_t* p = RT_DATA_SLOT(const uint32_t*,
        "?_typeInfo@LabelElement@UI@@0VBaseTypeInfo@Object@@A");
    return p ? *p : 0u;
}

// The address of parameter slot `index` of a TextElement's own
// Collection::Array<System::String> (data +0xC8, size +0xD0 — see
// runtime/types/ui/TextElement.h, derived from TextElement::SetParameter's own
// call site at 0x18011e9b0). Factored out because it is the ONE piece of
// arithmetic in this lane that can be silently wrong in a way no run would
// notice, and RtUiW2SelfTest() at the bottom of this file exercises it with a
// negative control that is verified to fire.
inline void* TextParamSlot(void* textElem, int32_t index) {
    auto* data = *reinterpret_cast<uint8_t**>(
        static_cast<uint8_t*>(textElem) + UI::TextElementOffsets::kParamArrayData);
    return data + static_cast<size_t>(index) * 8;
}
inline int32_t TextParamCount(const void* textElem) {
    return *reinterpret_cast<const int32_t*>(
        static_cast<const uint8_t*>(textElem) + UI::TextElementOffsets::kParamCount);
}

} // namespace

// ============================================================================
// SpinnerElement — 7/7 ported
// ============================================================================

// ---- ?GetMaxValue@SpinnerElement@UI@@QEBAHXZ  0x1800d7cb0 ------------------
int RtUi_SpinnerElement_GetMaxValue(SpinnerElement* self) {
    RT_DIFF_BEGIN("UI::SpinnerElement::GetMaxValue");
    NumberEditElement* child = FirstChildAs<NumberEditElement>(&self->base, NumberEditElementTypeId());
    int ours = child->_max;   // NO NULL CHECK — retail has none either.
    if (rt_diff_on__) {
        using Fn = int (*)(SpinnerElement*);
        RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, kSpinnerGetMaxValue)(self));
    }
    return ours;
}

// ---- ?GetValue@SpinnerElement@UI@@QEBAHXZ  0x1800d7cf0 ---------------------
int RtUi_SpinnerElement_GetValue(SpinnerElement* self) {
    RT_DIFF_BEGIN("UI::SpinnerElement::GetValue");
    NumberEditElement* child = FirstChildAs<NumberEditElement>(&self->base, NumberEditElementTypeId());
    int ours = StringToInteger()(child->base._text);
    if (rt_diff_on__) {
        using Fn = int (*)(SpinnerElement*);
        RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, kSpinnerGetValue)(self));
    }
    return ours;
}

// ---- ?IsActive@SpinnerElement@UI@@QEBA_NXZ  0x1800d7c30 --------------------
bool RtUi_SpinnerElement_IsActive(SpinnerElement* self) {
    RT_DIFF_BEGIN("UI::SpinnerElement::IsActive");
    NumberEditElement* child = FirstChildAs<NumberEditElement>(&self->base, NumberEditElementTypeId());
    bool ours = child->base._flagE0 != 0;
    if (rt_diff_on__) {
        using Fn = bool (*)(SpinnerElement*);
        RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, kSpinnerIsActive)(self));
    }
    return ours;
}

// ---- ?SetElementId@SpinnerElement@UI@@QEAAXH@Z  0x1800d7e10 ----------------
// Writes the CHILD's elementId AND the spinner's own — Element::_elementId is
// the same inherited field on both (Element.h +0x88), so no dedicated
// SpinnerElement field is needed for this.
void RtUi_SpinnerElement_SetElementId(SpinnerElement* self, int id) {
    RT_DIFF_BEGIN("UI::SpinnerElement::SetElementId");
    SpinnerElement replay;
    std::memcpy(&replay, self, sizeof(SpinnerElement));

    NumberEditElement* child = FirstChildAs<NumberEditElement>(&self->base, NumberEditElementTypeId());
    child->base.base._elementId = id;
    self->base._elementId = id;

    if (rt_diff_on__) {
        using Fn = void (*)(SpinnerElement*, int);
        RT_ORIGINAL_ONCE(Fn, kSpinnerSetElementId)(&replay, id);
        RT_DIFF_BYTES(self, &replay, sizeof(SpinnerElement));
    }
}

// ---- ?SetIncrement@SpinnerElement@UI@@QEAAXH@Z  0x1800d7d30 ----------------
//
// *** THE FIRST ADOPTER OF RT_DIFF_REPLAY_PRE/POST (Wave 25 lane SWEEP). ***
//
// Wave 24 lane DIFF measured that the invert mode documented at length in
// rt_diff.h had ZERO adopters tree-wide: `BANISHED_RT_DIFF_INVERT=1` was a flag
// that changed nothing, and the header's instructions described a conversion
// nobody had performed. Inert documented machinery is how a future lane loses a
// day, so the kickoff asked for "adopt or delete".
//
// WHY THIS SITE. The invert order exists to expose a body that double-manages
// an allocation, and the way to prove the machinery works is a site where
// inverting is KNOWN SAFE. This body writes one int into a field neither side
// allocates, refcounts or frees, so PRE and POST are equivalent here by
// construction: whichever order runs, both sides end with `_increment` set and
// nothing else touched. `SetMinMax` and `SetValue` immediately below are NOT
// converted -- they go through NumberEditElement::SetValue, which reformats
// text through a REFCOUNTED buffer, and that is Wave 9 lane M's Finding 1
// exactly. Converting those would be arming a crash, not adopting a tool.
//
// WHAT THIS ADOPTION DOES NOT ESTABLISH: this site did not register on Wave
// 24's route and did not register on Wave 25 lane SWEEP's either (no menu path
// reaches a spinner), so the PRE arm is COMPILED and SELECTED but has not been
// OBSERVED to run. `[RtDiff] INVERT arm: UI::SpinnerElement::SetIncrement` in
// standalone_error.log under BANISHED_RT_DIFF=1 BANISHED_RT_DIFF_INVERT=1 is
// the line that would prove it, on a route that touches a spinner.
void RtUi_SpinnerElement_SetIncrement(SpinnerElement* self, int increment) {
    RT_DIFF_BEGIN("UI::SpinnerElement::SetIncrement");
    SpinnerElement replay;
    std::memcpy(&replay, self, sizeof(SpinnerElement));
    using Fn = void (*)(SpinnerElement*, int);

    // Exactly one of the two blocks runs. PRE only under INVERT; POST is the
    // shipped order and is what every unconverted site does.
    RT_DIFF_REPLAY_PRE  { RT_ORIGINAL_ONCE(Fn, kSpinnerSetIncrement)(&replay, increment); }

    self->_increment = increment;

    RT_DIFF_REPLAY_POST { RT_ORIGINAL_ONCE(Fn, kSpinnerSetIncrement)(&replay, increment); }
    RT_DIFF_BYTES(self, &replay, sizeof(SpinnerElement));
}

// ---- ?SetMinMax@SpinnerElement@UI@@QEAAXHH@Z  0x1800d7d40 ------------------
// Writes the child's min/max directly, then re-clamps the child's CURRENT
// text value through it by calling the REAL NumberEditElement::SetValue —
// not reimplementing it (that body is held back; see the comment on
// RealNumberEditElementSetValue above).
void RtUi_SpinnerElement_SetMinMax(SpinnerElement* self, int minVal, int maxVal) {
    RT_DIFF_BEGIN("UI::SpinnerElement::SetMinMax");
    SpinnerElement replay;
    std::memcpy(&replay, self, sizeof(SpinnerElement));

    NumberEditElement* child = FirstChildAs<NumberEditElement>(&self->base, NumberEditElementTypeId());
    child->_min = minVal;
    child->_max = maxVal;
    int clamped = StringToInteger()(child->base._text);
    RealNumberEditElementSetValue()(child, clamped);

    if (rt_diff_on__) {
        using Fn = void (*)(SpinnerElement*, int, int);
        RT_ORIGINAL_ONCE(Fn, kSpinnerSetMinMax)(&replay, minVal, maxVal);
        RT_DIFF_BYTES(self, &replay, sizeof(SpinnerElement));
    }
}

// ---- ?SetValue@SpinnerElement@UI@@QEAAXH@Z  0x1800d7dc0 --------------------
void RtUi_SpinnerElement_SetValue(SpinnerElement* self, int value) {
    RT_DIFF_BEGIN("UI::SpinnerElement::SetValue");
    SpinnerElement replay;
    std::memcpy(&replay, self, sizeof(SpinnerElement));

    NumberEditElement* child = FirstChildAs<NumberEditElement>(&self->base, NumberEditElementTypeId());
    RealNumberEditElementSetValue()(child, value);

    if (rt_diff_on__) {
        using Fn = void (*)(SpinnerElement*, int);
        RT_ORIGINAL_ONCE(Fn, kSpinnerSetValue)(&replay, value);
        RT_DIFF_BYTES(self, &replay, sizeof(SpinnerElement));
    }
}

// ============================================================================
// ComboElement — 3/6 ported (pure getters only; AddItem/RemoveAllItems/
// SetCurrentItem held back — WAVE7_LANE_D.md §3)
// ============================================================================

// ---- ?GetCurrentItem@ComboElement@UI@@QEBAHXZ  0x180075160 -----------------
int RtUi_ComboElement_GetCurrentItem(ComboElement* self) {
    RT_DIFF_BEGIN("UI::ComboElement::GetCurrentItem");
    int ours = self->_currentItem;
    if (rt_diff_on__) {
        using Fn = int (*)(ComboElement*);
        RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, kComboGetCurrentItem)(self));
    }
    return ours;
}

// ---- ?GetItemCount@ComboElement@UI@@QEBAHXZ  0x180075180 -------------------
int RtUi_ComboElement_GetItemCount(ComboElement* self) {
    RT_DIFF_BEGIN("UI::ComboElement::GetItemCount");
    int ours = self->_strings._size;
    if (rt_diff_on__) {
        using Fn = int (*)(ComboElement*);
        RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, kComboGetItemCount)(self));
    }
    return ours;
}

// ---- ?GetItemData@ComboElement@UI@@QEBAPEAXH@Z  0x180075170 ----------------
void* RtUi_ComboElement_GetItemData(ComboElement* self, int index) {
    RT_DIFF_BEGIN("UI::ComboElement::GetItemData");
    void* ours = self->_userData._data[index];   // NO BOUNDS CHECK — retail has none.
    if (rt_diff_on__) {
        using Fn = void* (*)(ComboElement*, int);
        RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, kComboGetItemData)(self, index));
    }
    return ours;
}

// ============================================================================
// ?AddItem@ComboElement@UI@@QEAAHAEBVSprite@2@AEBVString@System@@PEAX@Z
//                                   retail 0x1800750b0   (WAVE 9 LANE UIW)
// ============================================================================
//
// Wave 7 lane D held this back "for the allocator question", because its three
// growth helpers reach `System::MemoryAllocator::Allocate` under its own
// critical section. THAT QUESTION IS SETTLED: the allocator is ported (Wave 9
// lane E) and registered by address as of attempt 14 (commit 309c015), and all
// three helpers are now ports of ours registered at their own addresses
// (runtime/src/collection/array.cpp, lane UIW's five J3 rows). So the row
// closes rather than moving one frame deeper.
//
// THE BODY, 0x1800750b0..0x180075112, is eleven instructions of orchestration:
//
//     AddItem(this, Sprite const& sprite, String const& str, void* userData):
//         0x180034860(&this->_strings,  _strings._size  + 1, &str);
//         0x18004bfc0(&this->_sprites,  _sprites._size  + 1, &sprite);
//         0x180024fe0(&this->_userData, _userData._size + 1, &userData);
//         return this->_strings._size - 1;
//
// THE ARGUMENT SHUFFLE IS THE ONE THING TO GET RIGHT, and it is easy to read
// backwards. `R8` is NEVER SET before the first call — retail relies on the
// incoming `String const&` already being in it. `RDX` (the Sprite) is saved to
// RBX at entry and becomes the SECOND call's R8. `R9` (userData) is spilled to
// its home slot at entry and the THIRD call's R8 is the ADDRESS of that slot,
// not the value: the element type is `void*` and the helper takes a POINTER to
// the fill. Getting that last one wrong stores a stack address as the item's
// user data and nothing complains until the caller reads it back.
//
// THE RETURN IS READ FROM MEMORY, not computed. `MOV EAX,[RDI+0xf0]; DEC EAX`
// re-reads `_strings._size` AFTER all three appends, so it is the post-append
// size minus one — which equals the index just written. Reproduced as a re-read
// for the same reason lane N's `RemoveChildren` re-reads its base: if a growth
// helper had failed to grow, retail would return the stale index and so does
// this.
//
// NO ORACLE, and the reason is specific. A replay APPENDS A SECOND TIME to all
// three arrays, reallocating them, and bumps the `System::String`'s refcount a
// second time — the exact shape the file-level oracle note forbids. The shadow
// below is NOT a comparison against retail; it is an INDEPENDENT check against
// the SOURCE data (this function did not write the caller's `str`/`userData`),
// which is the strongest thing available here.
int RtUi_ComboElement_AddItem(ComboElement* self, const void* sprite,
                              const void* str, void* userData) {
    RT_DIFF_BEGIN("UI::ComboElement::AddItem [no oracle: appends + bumps a refcount]");

    const int32_t before = self->_strings._size;

    ArraySetSizeFill_180034860()(&self->_strings,  self->_strings._size  + 1, str);
    ArraySetSizeFill_18004bfc0()(&self->_sprites,  self->_sprites._size  + 1, sprite);
    ArraySetSizeFill_180024fe0()(&self->_userData, self->_userData._size + 1, &userData);

    const int ours = self->_strings._size - 1;

    if (rt_diff_on__) {
        // Independent of this body's own rule: the three arrays must have grown
        // in lockstep, the returned index must be the pre-call size, and the
        // two elements whose SOURCE we still hold must have landed. The sprite
        // is not checked — its 0x10 bytes are copied verbatim by the helper and
        // comparing them here would only restate that helper's own contract.
        const bool ok =
            self->_strings._size  == before + 1 &&
            self->_sprites._size  == before + 1 &&
            self->_userData._size == before + 1 &&
            ours == before &&
            self->_userData._data[ours] == userData &&
            self->_strings._data[ours] == *static_cast<void* const*>(str);
        char oursDesc[128];
        std::snprintf(oursDesc, sizeof(oursDesc),
                      "idx=%d sizes=%d/%d/%d", ours, self->_strings._size,
                      self->_sprites._size, self->_userData._size);
        char theirsDesc[128];
        std::snprintf(theirsDesc, sizeof(theirsDesc),
                      "idx=%d sizes=%d/%d/%d", before, before + 1, before + 1, before + 1);
        RT_DIFF_SHADOW(ok, oursDesc, theirsDesc);
    }
    return ours;
}

// ============================================================================
// ?RemoveAllItems@ComboElement@UI@@QEAAXXZ  retail 0x180075120  (LANE UIW)
// ============================================================================
//
//     RemoveAllItems(this):
//         0x180010ab0(&this->_strings);    ; destroy every String, free, zero
//         0x180037310(&this->_sprites);    ; ~Array
//         0x180024aa0(&this->_userData);   ; ~Array
//         this->_currentItem = -1;
//
// Lane D read the last store correctly as -1 and it is -1 (`MOV dword ptr
// [RBX+0x118],0xffffffff`) — worth confirming explicitly, because the SIBLING
// `SetCurrentItem` resets an out-of-range index to 0 rather than -1, and a
// reader who assumes the two agree gets one of them wrong.
//
// **`_openDialog` (+0x120) IS NOT TOUCHED.** Removing every item while a combo
// popup is open leaves the popup pointer dangling in retail. That looks like an
// omission and it is not this port's business to repair it — CLAUDE.md's rule
// on faithful crashes applies to faithful *leaks* for the same reason: the
// caller may be relying on it, and inventing a close here would change what the
// UI does.
//
// NO ORACLE: the function frees three buffers and releases every String
// reference. A replay double-frees. The shadow is the post-condition retail
// guarantees, which is weaker than a diff and labelled as one.
void RtUi_ComboElement_RemoveAllItems(ComboElement* self) {
    RT_DIFF_BEGIN("UI::ComboElement::RemoveAllItems [no oracle: frees three buffers]");

    ArrayCleanup_180010ab0()(&self->_strings);
    ArrayCleanup_180037310()(&self->_sprites);
    ArrayCleanup_180024aa0()(&self->_userData);
    self->_currentItem = -1;

    if (rt_diff_on__) {
        const bool ok =
            self->_strings._data  == nullptr && self->_strings._size  == 0 &&
            self->_sprites._data  == nullptr && self->_sprites._size  == 0 &&
            self->_userData._data == nullptr && self->_userData._size == 0 &&
            self->_currentItem == -1;
        RT_DIFF_SHADOW(ok, ok ? "all three emptied, current=-1"
                              : "NOT emptied", "all three emptied, current=-1");
    }
}

// ============================================================================
// NumberEditElement — 2/3 ported (SetValue held back — variadic
// System::String::Format, WAVE7_LANE_D.md §3)
// ============================================================================

// ---- ?GetValue@NumberEditElement@UI@@QEBAHXZ  0x1800d4b50 ------------------
int RtUi_NumberEditElement_GetValue(NumberEditElement* self) {
    RT_DIFF_BEGIN("UI::NumberEditElement::GetValue");
    int ours = StringToInteger()(self->base._text);
    if (rt_diff_on__) {
        using Fn = int (*)(NumberEditElement*);
        RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, kNumEditGetValue)(self));
    }
    return ours;
}

// ---- ?SetMinMax@NumberEditElement@UI@@QEAAXHH@Z  0x1800d4b70 ---------------
// Same shape as SpinnerElement::SetMinMax, minus the child indirection:
// write min/max on `self` directly, then re-clamp through the REAL SetValue.
//
// NO REPLAY ORACLE (Wave 9 lane N; see the block above NumberElement::SetValue
// for the whole argument and the evidence). SpinnerElement::SetMinMax reaches
// the refcounted string through a pointer to a LIVE CHILD, which stays valid;
// this one reaches `EditElement::_text` at +0xC8, which is INSIDE the copied
// object, so the replay would hold the pointer our own SetValue had just
// released. Never observed, because this export is not called anywhere in lane
// M's 10-cell matrix (`calls=` never even registered a site) — a latent heap
// hazard, removed rather than left armed for the first caller to find.
// The site stays registered so RtDiffSummary() still lists the function.
void RtUi_NumberEditElement_SetMinMax(NumberEditElement* self, int minVal, int maxVal) {
    RT_DIFF_BEGIN("UI::NumberEditElement::SetMinMax [no oracle: replay would double-release _text]");
    (void)rt_diff_on__;

    self->_min = minVal;
    self->_max = maxVal;
    int clamped = StringToInteger()(self->base._text);
    RealNumberEditElementSetValue()(self, clamped);
}

// ============================================================================
// NumberElement — 2/2 ported
// ============================================================================

// ---------------------------------------------------------------------------
// BOTH NumberElement SETTERS ARE NO-ORACLE PORTS (Wave 9, lane M's Finding 1;
// applied here by lane N). They were prefix-replayed until 2026-09-08, and
// that replay was CORRUPTING THE HEAP it was measuring.
//
// WHY. Both bodies end in the DLL's own private NumberElement::CacheText
// (0x1801189F0), which REPLACES the refcounted System::String at +0xD0 with
// retail's inlined String::operator=: release the old (`dec [p-4]`; on zero
// `p -= 8`, MemFree), store the new, `inc [p-4]`. The struct called that field
// padding until this lane read the disassembly, which is the only reason it
// was ever prefix-replayed at all — the rule against it was already written in
// this file's own header block (lines 53-58) by the lane that ported it, and
// the sibling lanes applied it: widgets_layout.cpp's two TextElement bodies
// and element.cpp's SetName/SetToolTipString are all NO-ORACLE for exactly
// this reason.
//
// `memcpy(&replay, self, ...)` is a SHALLOW copy, so `replay`'s +0xD0 was the
// SAME buffer `self` held. Ours ran CacheText on `self` and released that
// buffer; theirs then ran CacheText on `replay` from the pointer we had just
// released. **Stated narrowly, because that is what was measured:** in the
// SHIPPED order the oracle hands the DLL's own body a DANGLING POINTER — a
// use-after-free on every call that re-formats text. Whether retail then frees
// that block a SECOND time in the shipped order is NOT established; no
// shipped-order run crashed. Allocator free-list corruption was demonstrated
// only under the order-inversion control below.
//
// THE EVIDENCE (lane M, measured, not argued):
//   * effectiveness control: of 12,896 calls, exactly the 10 that reached
//     CacheText diverged — 1:1, 100% of the calls that touch the string and
//     0% of the 12,886 that take the unchanged-value early-out. The varying
//     9/16/10 across cells is just how many counters changed in 900 frames.
//   * ours pointed at a live block (len=1, rc=1, "4" for value 4, correct for
//     all ten inputs); theirs pointed at a block whose header was all zeros —
//     freed. Retail did not compute a different answer; it was handed a buffer
//     our call had already released.
//   * the order-inversion control did not merely move the divergence, it
//     CRASHED: ACCESS_VIOLATION in the Runtime allocator, "bucket 8: free
//     block has CORRUPTED forward link", with the clobbered free-block body
//     reading 6f6e53003237322b = "+272\0Sno" — "272" being the input of the
//     third diverging call. Caught in the act, with its own digits.
//
// WHY NOT AN EXCLUSION OF +0xD0..+0xD7. It would make the cell green while
// leaving the double release — and therefore the heap corruption — in place on
// every diff run, and it would hide a real future defect in the field. The
// inverse of reference_oracle_must_not_compare_unwritten_bytes.md: here BOTH
// sides write, through the same DLL body, and what they write is an address.
//
// WHY NOT A DEEP COPY / SNAPSHOT-RESTORE either. A prefix replay of any body
// that reaches CacheText is unsound in PRINCIPLE, not merely badly
// implemented: the field is a reference into globally shared refcounted state
// that no per-object snapshot isolates.
//
// WHAT IS LOST. These two functions now have no cross-implementation check, on
// the same terms as UI::EditElement::SetText (WAVE7_LANE_D.md §5):
// correctness rests on matching the disassembly instruction for instruction.
// It does — 0x180118D50 and 0x180118DB0 are transcribed field for field below,
// including retail's `ucomiss`/`jp` NaN behaviour in the float guard (an
// unordered compare falls through to the write, which `!(a == b)` reproduces).
// The sites stay REGISTERED so both functions still appear in RtDiffSummary()
// rather than silently vanishing from the audit.
// ---------------------------------------------------------------------------

// ---- ?SetValue@NumberElement@UI@@QEAAXH@Z  0x180118d50 ---------------------
void RtUi_NumberElement_SetValueInt(NumberElement* self, int value) {
    RT_DIFF_BEGIN("UI::NumberElement::SetValueInt [no oracle: CacheText owns a refcounted String]");
    (void)rt_diff_on__;   // registered for visibility; no oracle call below.

    if (!(self->_mode == 0 && self->_intValue == value)) {
        self->_intValue = value;
        self->_floatValue = 0.0f;   // retail zeroes the raw dword; 0 == 0.0f bitwise.
        self->_mode = 0;
        MarkNeedsSizeCalcUpToRoot(&self->base);
        NumberElementCacheText()(self);
    }
}

// ---- ?SetValue@NumberElement@UI@@QEAAXM@Z  0x180118db0 ---------------------
void RtUi_NumberElement_SetValueFloat(NumberElement* self, float value) {
    RT_DIFF_BEGIN("UI::NumberElement::SetValueFloat [no oracle: CacheText owns a refcounted String]");
    (void)rt_diff_on__;   // registered for visibility; no oracle call below.

    if (!(self->_mode == 1 && self->_floatValue == value)) {
        self->_floatValue = value;
        self->_intValue = 0;
        self->_mode = 1;
        MarkNeedsSizeCalcUpToRoot(&self->base);
        NumberElementCacheText()(self);
    }
}

// ============================================================================
// EditElement — 1/2 ported, NO ORACLE (the phantom-name row is not a real
// function — see WAVE7_LANE_D.md §5 for the full story and the engine_funcs.cpp
// source fix already applied)
// ============================================================================

// ---- ?SetText@EditElement@UI@@QEAAXAEBVString@System@@@Z  0x1800d0db0 -----
// NO REPLAY ORACLE. Unlike every mutator above, this one swaps a REFCOUNTED
// shared string: it decrements the OLD value's refcount (freeing it via
// MemFree if that count hits zero) and increments the NEW one's. Replaying
// the DLL's original body a second time on a byte-copy would decrement/
// increment those SAME shared refcounts again — wrong regardless of whether
// the first decrement already freed the old buffer (a use-after-free if it
// did) — exactly PLAN §4.3 risk 2's territory, not the idempotent-rewrite
// shape the rest of this lane relies on. Ported anyway (the alternative is
// leaving the phantom-name bug's behaviour in place) with NO cross-
// implementation check; correctness rests on matching the decompile
// instruction-for-instruction, recorded in WAVE7_LANE_D.md §5.
void RtUi_EditElement_SetText(EditElement* self, const void* newText) {
    RT_DIFF_BEGIN("UI::EditElement::SetText");   // registered for visibility in RtDiffSummary(); never reported — no oracle call below.

    if (self->_flagE0 == 0) {
        void* thisStr = &self->_text;
        bool notEqual = StringNotEqual()(thisStr, newText);
        if (notEqual) {
            MarkNeedsSizeCalcUpToRoot(&self->base);
        }

        char* oldPtr = self->_text;
        char* newPtr = *reinterpret_cast<char* const*>(newText);
        if (oldPtr != newPtr) {
            if (oldPtr != nullptr) {
                int32_t* refcount = reinterpret_cast<int32_t*>(oldPtr - 4);
                *refcount -= 1;
                if (*refcount == 0) {
                    MemFreeStatic()(oldPtr - 8);
                }
            }
            self->_text = newPtr;
            if (newPtr != nullptr) {
                *reinterpret_cast<int32_t*>(newPtr - 4) += 1;
            }
        }
    }
}

// ============================================================================
// RadioElement — 1/3 ported (both FindCheckBoxes held back — WAVE7_LANE_D.md §3)
// ============================================================================

// ---- ?SetActiveRadio@RadioElement@UI@@QEAAXH@Z  0x1800d56f0 ----------------
// Clears the "checked" byte (+0xd8, a field of the CHECKBOX child class, not
// RadioElement's own — see RadioElement.h) on every checkbox except the new
// active index. IDEMPOTENT under a repeated identical call: the early-out
// guard makes a second call with the same index a no-op, and even without the
// guard, re-clearing an already-zero byte is a no-op — so replaying the
// original on the child array a second time (shared with the live object, per
// the file-level oracle note) is safe.
void RtUi_RadioElement_SetActiveRadio(RadioElement* self, int index) {
    RT_DIFF_BEGIN("UI::RadioElement::SetActiveRadio");
    RadioElement replay;
    std::memcpy(&replay, self, sizeof(RadioElement));

    if (index != self->_activeIndex) {
        self->_activeIndex = index;
        const int count = self->_checkBoxes._size;
        for (int i = 0; i < count; ++i) {
            if (i != self->_activeIndex) {
                uint8_t* box = reinterpret_cast<uint8_t*>(self->_checkBoxes._data[i]);
                box[0xd8] = 0;
            }
        }
    }

    if (rt_diff_on__) {
        using Fn = void (*)(RadioElement*, int);
        RT_ORIGINAL_ONCE(Fn, kRadioSetActiveRadio)(&replay, index);
        RT_DIFF_BYTES(self, &replay, sizeof(RadioElement));
    }
}

// ============================================================================
// ?FindCheckBoxes@RadioElement@UI@@  — BOTH overloads   (WAVE 9 LANE UIW)
//   public  0x180047c00  ?FindCheckBoxes@RadioElement@UI@@QEAAXXZ
//   private 0x1800d5750  ?FindCheckBoxes@RadioElement@UI@@AEAAXV?$InternalPtr@…
// ============================================================================
//
// Wave 7 lane D held these back for the allocator plus "an InternalPtr
// parameter and a recursive tree walk this lane did not fully verify against
// Element::_children's exact iteration order". Both are answered: the growth
// helper 0x180048fe0 is now ours (array.cpp, kSpec_Fill8Z — the SAME
// instantiation `Element::AddChild` uses), and the iteration order is read out
// of the disassembly below rather than inferred.
//
// THE PRIVATE ONE, 0x1800d5750..0x1800d5812:
//
//     FindCheckBoxes(this, InternalPtr<Element>* p):
//         typeId = *(uint32*)0x1801fa9a8;          ; READ FIRST, before the vcall
//         e = *p;
//         if (typeId == e->vtbl[2](e)) {           ; Type(), a VIRTUAL call
//             e2 = *p;                              ; RE-READ
//             found = IsDerivedFrom(e2, *(uint32*)0x1801fa9a8) ? e2 : nullptr;
//             0x180048fe0(&this->_checkBoxes, _checkBoxes._size + 1, &found);
//         }
//         for (i = 0; i < ((Element*)*p)->_children._size; ++i) {
//             child = ((Element*)*p)->_children._data[i];
//             if (!child) continue;
//             InternalPtr<Element> tmp = child;     ; zeroed, then stored
//             FindCheckBoxes(this, &tmp);           ; RECURSE
//         }
//
// FOUR THINGS THAT ARE NOT TIDINESS:
//
//  1. **The type test is a VIRTUAL `Type()` call, not `IsDerivedFrom`** — and
//     then `IsDerivedFrom` runs anyway, on the same object, against the same
//     id, purely to decide whether the stored pointer is the element or NULL.
//     That is not redundant: `Type()` is EXACT equality (this class), while
//     `IsDerivedFrom` walks the base chain, so the second call can only ever
//     say yes here. Retail does both and so does this. Lane D flagged the
//     virtual call as "note the difference from every other type check in this
//     lane" and was right to.
//  2. **The global is read TWICE**, at 0x1800d575f and again at 0x1800d577b.
//     Reproduced as two reads. It cannot change between them today, but a
//     single cached read is a different instruction sequence for no gain.
//  3. **`*p` is RE-READ at every use** — before the vcall, after it, before the
//     loop, and again at the bottom of every iteration (0x1800d57ed
//     `MOV RAX,[RSI]`). The loop BOUND is re-read with it. So a child whose
//     recursion reparented the tree is seen by the next iteration. Caching `*p`
//     in a local would be the obvious cleanup and would change behaviour under
//     exactly the mutation this shape exists to survive.
//  4. **NO NULL CHECK on `*p` or on its vtable.** A null element faults on the
//     `MOV RAX,[RCX]` in retail too. Not guarded here — CLAUDE.md's rule.
//
// NO ORACLE. The function APPENDS to `_checkBoxes` and recurses over the whole
// subtree; a replay appends every checkbox a second time and reallocates the
// array out from under the first pass. The shadow records liveness plus the one
// post-condition that is independent of this body's own rule: every entry it
// left in `_checkBoxes` must be a pointer the tree actually contains — checked
// as "non-null entries only", since a null entry is exactly what the
// `IsDerivedFrom` arm above stores when it disagrees with `Type()`, and seeing
// one would mean the two type tests disagreed, which is the interesting failure.

// The cached checkbox type id, `dword ptr [0x1801fa9a8]` — a plain Runtime
// .data global, read the same way SpinnerElement's `NumberEditElementTypeId`
// above reads its own. Retail never lazily initialises it before reading, and
// neither does this.
uint32_t CheckBoxTypeId() {
    static uint32_t* p = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Runtime, 0x1801fa9a8));
    return p ? *p : 0;
}

void RtUi_RadioElement_FindCheckBoxesPrivate(RadioElement* self, void** p) {
    RT_DIFF_BEGIN("UI::RadioElement::FindCheckBoxes(InternalPtr) [no oracle: appends + recurses]");

    const uint32_t typeId = CheckBoxTypeId();          // read BEFORE the vcall
    void* e = *p;
    void** vtbl = *reinterpret_cast<void***>(e);
    const uint32_t actual =
        reinterpret_cast<uint32_t (*)(void*)>(vtbl[2])(e);   // slot 2 == Type()

    bool appended = false;
    if (typeId == actual) {
        void* e2 = *p;                                  // retail re-reads
        void* found = IsDerivedFrom()(e2, CheckBoxTypeId()) ? e2 : nullptr;
        ArraySetSizeFill_180048fe0()(&self->_checkBoxes,
                                          self->_checkBoxes._size + 1, &found);
        appended = true;
    }

    for (int32_t i = 0; i < reinterpret_cast<Element*>(*p)->_children._size; ++i) {
        Element* child = reinterpret_cast<Element*>(*p)->_children._data[i];
        if (child == nullptr) continue;
        void* tmp = nullptr;                            // retail zeroes it first
        tmp = child;
        RtUi_RadioElement_FindCheckBoxesPrivate(self, &tmp);
    }

    if (rt_diff_on__) {
        bool ok = true;
        for (int32_t i = 0; i < self->_checkBoxes._size; ++i)
            if (self->_checkBoxes._data[i] == nullptr) { ok = false; break; }
        char oursDesc[96];
        std::snprintf(oursDesc, sizeof(oursDesc), "%d checkboxes, %s",
                      self->_checkBoxes._size, ok ? "none null" : "A NULL ENTRY");
        RT_DIFF_SHADOW(ok, oursDesc, "no null entry");
        RT_DIFF_WORK(appended);
    }
}

// The public overload is six instructions: it puts `this` into a stack slot and
// hands the ADDRESS of that slot to the private one. Note it starts the walk at
// the RadioElement ITSELF, so a RadioElement that were somehow its own checkbox
// type would add itself — retail does not special-case it and neither does this.
//
// This is the overload on the cutover residue list. It is also the one
// `engine/funcs/engine_funcs.cpp:2855` names, after Wave 9 lane R corrected that
// fallback from 0x1800d5750 (the PRIVATE 1-arg sibling, a real entry point that
// would have run tree surgery with whatever the caller left in RDX) to
// 0x180047c00. Both are registered below, each at its own address, so neither
// door can pick up the other's body.
void RtUi_RadioElement_FindCheckBoxes(RadioElement* self) {
    RT_DIFF_BEGIN("UI::RadioElement::FindCheckBoxes()");
    void* slot = self;
    RtUi_RadioElement_FindCheckBoxesPrivate(self, &slot);
    RT_DIFF_WORK(true);
}

// ============================================================================
// ProgressElement — 2/3 ported (SetProgressTextParameter held back — WAVE7_LANE_D.md §3)
// ============================================================================

// ---- ?SetMaximumValue@ProgressElement@UI@@QEAAXM@Z  0x180077ec0 -----------
void RtUi_ProgressElement_SetMaximumValue(ProgressElement* self, float value) {
    RT_DIFF_BEGIN("UI::ProgressElement::SetMaximumValue");
    ProgressElement replay;
    std::memcpy(&replay, self, sizeof(ProgressElement));

    self->_maxValue = value;

    if (rt_diff_on__) {
        using Fn = void (*)(ProgressElement*, float);
        RT_ORIGINAL_ONCE(Fn, kProgressSetMax)(&replay, value);
        RT_DIFF_BYTES(self, &replay, sizeof(ProgressElement));
    }
}

// ---- ?SetCurrentValue@ProgressElement@UI@@QEAAXM@Z  0x180077ed0 -----------
// SECOND ADOPTER of RT_DIFF_REPLAY_PRE/POST. Same argument as
// SpinnerElement::SetIncrement above: one float written into a field neither
// side allocates or frees, so the two orders are equivalent here and the
// conversion cannot arm a use-after-free. Chosen as the second adopter because
// it is the replay-shaped site most likely to be reached on a normal route (the
// loading bar), which is the one that would actually exercise the PRE arm.
void RtUi_ProgressElement_SetCurrentValue(ProgressElement* self, float value) {
    RT_DIFF_BEGIN("UI::ProgressElement::SetCurrentValue");
    ProgressElement replay;
    std::memcpy(&replay, self, sizeof(ProgressElement));
    using Fn = void (*)(ProgressElement*, float);

    RT_DIFF_REPLAY_PRE  { RT_ORIGINAL_ONCE(Fn, kProgressSetCurrent)(&replay, value); }

    self->_currentValue = value;

    RT_DIFF_REPLAY_POST { RT_ORIGINAL_ONCE(Fn, kProgressSetCurrent)(&replay, value); }
    RT_DIFF_BYTES(self, &replay, sizeof(ProgressElement));
}


// ============================================================================
// WAVE 9 LANE UIW2 — SliderElement::SetParameter,
//                    ProgressElement::SetProgressTextParameter,
//                    ComboElement::UpdateText + ::SetCurrentItem
// ============================================================================
// Full write-up, closure set-diff and HONEST LIMITS:
//   docs/runtime_port/WAVE9_LANE_UIW2.md
//
// ---------------------------------------------------------------------------
// ARGUMENT COUNT, CHECKED AGAINST THE MANGLED NAME BEFORE ANYTHING ELSE
// ---------------------------------------------------------------------------
// The brief's warning for this lane, and the failure family behind it
// (reference_hidden_retslot_wild_write.md): a String-RETURNING member has a
// hidden return slot between `this` and its arguments, and a pointer parameter
// typed `int` only goes wrong from the 5th argument on. Neither applies here,
// and that is a checked conclusion rather than an assumption:
//
//   ?SetParameter@SliderElement@UI@@QEAAXHAEBVString@System@@@Z
//   ?SetProgressTextParameter@ProgressElement@UI@@QEAAXHAEBVString@System@@@Z
//       QEAA X  H  AEBVString@System@@  -> returns VOID (no hidden slot),
//       takes (this, int, const String&) = RCX, EDX, R8. Three integer slots,
//       all register-passed. The disassembly agrees: `MOVSXD RDI,EDX`
//       sign-extends the int, `MOV RSI,R8` keeps the reference.
//
//   ?SetCurrentItem@ComboElement@UI@@QEAAXH@Z
//       QEAA X H -> void (this, int) = RCX, EDX.
//
// tools/abi_retslot_audit.py covers this file and is run in the lane doc.
//
// ---------------------------------------------------------------------------
// WHY THERE IS NO REPLAY ORACLE ON ANY OF THE FOUR — AND WHAT REPLACES IT
// ---------------------------------------------------------------------------
// Every one of them ends in `System::String::operator=` on a slot of a CHILD
// object reached through a pointer that a byte-for-byte scratch copy would
// share (the "shared-child caveat" this file's Wave 7 header already states).
// A replay would therefore run the refcount acquire/release TWICE on the one
// live string, and restoring the slot's raw pointer to undo our write can make
// retail's subsequent access a use-after-free rather than a diagnostic. That is
// exactly the argument runtime/src/ui/widgets_layout.cpp's TextElement::
// SetParameter makes for the identical tail, and it is why lane M's Finding 1
// says a body of this shape must NOT be prefix-replayed.
//
// What is here instead, and what each part is worth:
//
//   * RT_DIFF_WORK — the DID-WORK counter WAVE9.md asks for beside `calls=`.
//     A large `calls=` reads as coverage and is the same trap as `calls=0`
//     reading as clean, one level up: 12,886 of NumberElement::SetValueInt's
//     12,896 calls took an early-out. Every body below counts the calls that
//     REACHED THE ASSIGNMENT, separately from the calls that entered.
//   * RT_DIFF_SHADOW on a post-condition that is FALSIFIABLE, not a second
//     spelling of the code above it: after `String::operator=(dst, src)` the
//     ported body of that function guarantees `dst->data == src->data`
//     (runtime/src/system/string.cpp:360). That predicate FAILS if the slot
//     address is computed wrong — the wrong base offset, the wrong element
//     stride, the wrong index — which is the whole of what this lane could get
//     wrong that a run would not otherwise notice.
//   * RtUiW2SelfTest() at the bottom: a positive AND a negative control for
//     that arithmetic, run once per diff-enabled process, printing ALIVE or
//     BROKEN. It exists for the reason lane M's released-pointer detector
//     needed one — a predicate that can no longer fire is indistinguishable
//     from a predicate that has nothing to report
//     (reference_probe_needs_an_effectiveness_control.md).
//
// The shadow predicate is NOT a substitute for a differential oracle and this
// lane does not claim it is. It is silent about the dirty-flag walk, about
// CacheText's effects, and about anything past the offsets it names.
// ============================================================================

namespace {

// The tail both parameter setters run once the child is resolved and the index
// is in range. Retail 0x1800d6cf0-0x1800d6d1c and 0x180119b80-0x180119bac are
// the SAME SEVEN INSTRUCTIONS in the same order:
//   dirty-walk from the CHILD (not from `this`) up the parent chain,
//   String::operator=(&child->_params[index], str),
//   TextElement::CacheText(child).
// Returns the assigned slot so the caller can state a post-condition about it.
void* WrappedTextSetParam(void* child, int32_t index, const void* str) {
    MarkNeedsSizeCalcUpToRoot(reinterpret_cast<Element*>(child));
    void* slot = TextParamSlot(child, index);
    StringAssign()(slot, str);
    TextElementCacheText()(child);
    return slot;
}

// The shadow predicate shared by every assignment site below: our ported
// String::operator= shares the buffer pointer rather than copying, so after the
// call the destination slot's data pointer IS the source's.
inline bool SlotSharesBuffer(const void* slot, const void* src) {
    return *static_cast<const void* const*>(slot) == *static_cast<const void* const*>(src);
}

} // namespace

// ---- ?SetParameter@SliderElement@UI@@QEAAXHAEBVString@System@@@Z  0x1800d6ca0
//
// Wave 7 lane D held this back for three stated reasons and ALL THREE HAVE
// EXPIRED — checked, not assumed:
//   (a) "the target object belongs to a class this lane does not own or model"
//       — runtime/types/ui/TextElement.h has existed since Wave 7 lane E and is
//       derived from TextElement::SetParameter's own call site.
//   (b) "String::operator='s exact refcount sequence was not independently
//       confirmed" — it is PORTED and diff-verified
//       (runtime/src/system/string.cpp, Wave 2).
//   (c) "a refcount swap has no safe replay oracle regardless" — STILL TRUE,
//       and it is the one reason this port ships without one. (b) expiring does
//       not make (c) expire; they are different claims.
//
// The child is `_children._data[0]` cast through IsDerivedFrom against
// TextElement's type id, i.e. FirstChildAs<> — the same shape SpinnerElement's
// seven exports use, and the reason this file already has that helper.
void RtUi_SliderElement_SetParameter(Element* self, int32_t index, const void* str) {
    RT_DIFF_BEGIN("UI::SliderElement::SetParameter [no oracle: refcounted String swap on a shared child]");

    void* child = FirstChildAs<void>(self, TextElementTypeId());

    // Retail 0x1800d6ccf and 0x1800d6cd4 are TWO separate exits — `JZ` on the
    // cast result and then `JZ` on the pointer itself. The second is redundant
    // (a successful IsDerivedFrom implies non-null) and is transcribed anyway,
    // because it is what makes this function DIFFER from its Progress sibling
    // below. See that function's header.
    if (child == nullptr) { RT_DIFF_WORK(false); return; }
    if (index < 0) { RT_DIFF_WORK(false); return; }
    if (index >= TextParamCount(child)) { RT_DIFF_WORK(false); return; }

    void* slot = WrappedTextSetParam(child, index, str);

    RT_DIFF_WORK(true);
    RT_DIFF_SHADOW(SlotSharesBuffer(slot, str),
                   "param slot shares the source String buffer",
                   "not diffable (see the header)");
}

// ---- ?SetProgressTextParameter@ProgressElement@UI@@QEAAXHAEBVString@System@@@Z
//      0x180119b30
//
// THE SAME PROGRAM AS THE SLIDER ABOVE EXCEPT FOR ONE BRANCH, AND THAT
// DIFFERENCE IS THE INTERESTING PART OF THIS PORT.
//
//   Slider   0x1800d6ccd  TEST AL,AL   / JZ done   <- cast failed, bail
//            0x1800d6cd1  TEST RBX,RBX / JZ done   <- and bail on a null child
//   Progress 0x180119b62  XOR EDI,EDI
//            0x180119b66  CMOVNZ RDI,RBX          <- child = ok ? first : NULL
//            0x180119b6c  TEST ESI,ESI / JS done  <- only the NEGATIVE index exits
//            0x180119b6e  CMP ESI,dword ptr [RDI+0xd0]   <- READS THROUGH IT ANYWAY
//
// *** RETAIL HARD-FAULTS HERE WHERE THE SLIDER RETURNS QUIETLY. *** With a
// first child that is not a TextElement and a non-negative index,
// SetProgressTextParameter reads address 0x00000000000000d0 and dies. The
// CMOVNZ is not a null check; it is a cast, and retail then asserts the cast
// succeeded by dereferencing.
//
// This port REPRODUCES THAT, which means the load below is deliberately through
// a possibly-null pointer. It is spelled as byte arithmetic on a `uint8_t*`
// rather than `child->_paramCount` so the compiler emits the load instead of
// concluding the pointer cannot be null and deleting the branch around it.
//
// DO NOT "FIX" THIS WITH A NULL CHECK. Retail's unguarded deref is the
// assertion that a ProgressElement's first child is a TextElement, and a guard
// would keep us running in a state retail treats as impossible — the exact
// inversion WAVE9.md's Finding 3 records, where a null check had been MASKING a
// fault for a whole port rather than preventing one. If this faults, the thing
// to find is who built a ProgressElement whose first child is not a text
// widget.
void RtUi_ProgressElement_SetProgressTextParameter(Element* self, int32_t index, const void* str) {
    RT_DIFF_BEGIN("UI::ProgressElement::SetProgressTextParameter [no oracle: refcounted String swap on a shared child]");

    void* child = FirstChildAs<void>(self, TextElementTypeId());

    if (index < 0) { RT_DIFF_WORK(false); return; }
    // Faithful to 0x180119b6e — see the header. `child` may be null here.
    const int32_t count = *reinterpret_cast<const int32_t*>(
        static_cast<const uint8_t*>(child) + UI::TextElementOffsets::kParamCount);
    if (index >= count) { RT_DIFF_WORK(false); return; }

    void* slot = WrappedTextSetParam(child, index, str);

    RT_DIFF_WORK(true);
    RT_DIFF_SHADOW(SlotSharesBuffer(slot, str),
                   "param slot shares the source String buffer",
                   "not diffable (see the header)");
}

// ============================================================================
// ComboElement::UpdateText — private, UNEXPORTED, retail 0x180112180
// ============================================================================
// Wave 7 lane D named this as one of two things blocking SetCurrentItem and did
// not resolve its address; WAVE9_LANE_UIF.md row 16 pinned it at 0x180112180
// and still declined. It is ported here because it is the CHEAPER half: all
// four of its callees are already ours or already have a door —
// Element::FindElementType (ported, Wave 7 lane A), Object::Base::IsDerivedFrom
// (ported, Wave 3), System::String::operator= (ported, Wave 2), and
// TextElement::CacheText (an existing door, see the accessor above). Porting it
// therefore opens NOTHING, and leaving it unported would have meant
// SetCurrentItem calling 0x180112180 by address — one brand-new row in the
// hardest bucket, which is precisely the trap
// reference_porting_can_grow_the_cutover_surface.md describes.
//
// It finds the LABEL widget the combo displays its current item in, then pushes
// both the current item's STRING and its SPRITE into it.
//
// TWO THINGS A READER WILL WANT TO CHECK AGAINST THE LISTING, because both look
// like transcription slips and neither is:
//
//   1. `_children._data[4]`, NOT [0]. 0x18011219b/0x1801121a7 are
//      `MOV RBX,[RCX+0xa0]` then `MOV RBX,[RBX+0x20]` — 0x20/8 = 4. Every other
//      wrapped-widget lookup in this file is children[0]; this one is the fifth
//      child. Ghidra's C prints both loads as one expression, which is the
//      shape reference_ghidra_c_output_vs_disassembly.md warns about.
//   2. NEITHER index into `_strings` NOR into `_sprites` is bounds-checked
//      against the arrays' own sizes. Retail guards only the ITEM COUNT at the
//      top (+0xf0) and, for the sprite, the sprite's own validity. A
//      `_currentItem` of -1 — which ?RemoveAllItems@ sets — would index [-1].
//      That is unreachable in practice only because the +0xf0 guard makes the
//      array non-empty and SetCurrentItem never leaves -1 behind; it is not a
//      check retail performs, so it is not one this port performs.
void RtUi_ComboElement_UpdateText(ComboElement* self) {
    RT_DIFF_BEGIN("UI::ComboElement::UpdateText [no oracle: refcounted String swap on a shared child]");

    if (self->_strings._size <= 0) { RT_DIFF_WORK(false); return; }

    const uint32_t labelType = LabelElementTypeId();

    // children[4], then: EXACT type match short-circuits the search entirely
    // (0x1801121bb `CMP EAX,R12D` against the virtual Type(), slot 2).
    Element* start = self->base._children._data[4];
    void* found = nullptr;
    {
        using TypeFn = uint32_t (*)(const Element*);
        void** vtbl = *reinterpret_cast<void***>(start);
        if (reinterpret_cast<TypeFn>(vtbl[2])(start) == labelType) {
            found = start;
        } else {
            // Retail re-reads `start->_children._data` on every iteration
            // (0x1801121f0 `MOV RAX,[R15]`) while holding `&_size` fixed in
            // R14; transcribed with the same re-read for the same reason
            // widgets_layout.cpp's Element_RemoveChildren does.
            for (int32_t i = 0; i < start->_children._size; ++i) {
                Element* c = start->_children._data[i];
                if (c == nullptr) continue;
                void* out = nullptr;
                FindElementType()(c, &out, labelType);
                found = out;
                if (found != nullptr) break;
            }
        }
    }

    // 0x180112234: the found node is re-tested with IsDerivedFrom, which is
    // redundant after an exact-type FindElementType and is transcribed anyway.
    // It is NOT redundant on the short-circuit path above, where `start` was
    // matched on its own virtual Type().
    if (!IsDerivedFrom()(found, labelType)) { RT_DIFF_WORK(false); return; }
    if (found == nullptr) { RT_DIFF_WORK(false); return; }

    bool didWork = false;

    // ---- the STRING half (0x180112258-0x1801122a5) -------------------------
    if (TextParamCount(found) > 0) {
        MarkNeedsSizeCalcUpToRoot(reinterpret_cast<Element*>(found));
        void* slot = TextParamSlot(found, 0);
        const void* src = reinterpret_cast<const uint8_t*>(self->_strings._data)
                        + static_cast<size_t>(self->_currentItem) * 8;
        StringAssign()(slot, src);
        TextElementCacheText()(found);
        didWork = true;
        RT_DIFF_SHADOW(SlotSharesBuffer(slot, src),
                       "label param[0] shares the current item's String buffer",
                       "not diffable (see the header)");
    }

    // ---- the SPRITE half (0x1801122aa-0x180112306) -------------------------
    // `_sprites` is Array<UI::Sprite>, 16 bytes per element — retail computes
    // the offset as `(currentItem + currentItem) * 8`, which is the same thing
    // and is why the listing shows an `ADD RCX,RCX` before an `RCX*8` scale.
    // The two guards ARE UI::Sprite::IsValid() taken apart: index >= 0
    // (0x1801122bb `JL`) and sheet != null (0x1801122c2 `JZ`).
    {
        auto* sprites = reinterpret_cast<uint8_t*>(self->_sprites._data);
        auto* src = reinterpret_cast<const ::Sprite*>(
            sprites + static_cast<size_t>(self->_currentItem) * 16);
        if (src->spriteIndex >= 0 && src->spriteSheet != nullptr) {
            auto* dst = reinterpret_cast<::Sprite*>(
                reinterpret_cast<uint8_t*>(found) + 0x100);
            // Retail compares the sheet pointer and the index, then stores all
            // 16 bytes EITHER WAY — the dirty walk is what the comparison
            // gates, not the store (0x1801122ee `JZ` jumps straight to the
            // MOVUPS at 0x180112306). pad0C rides along in the 16-byte store
            // and is deliberately not part of the comparison, matching both
            // retail and runtime/types/ui/Sprite.h's note that no ctor writes
            // it (reference_oracle_must_not_compare_unwritten_bytes.md).
            if (dst->spriteSheet != src->spriteSheet || dst->spriteIndex != src->spriteIndex)
                MarkNeedsSizeCalcUpToRoot(reinterpret_cast<Element*>(found));
            *dst = *src;
            didWork = true;
        }
    }

    RT_DIFF_WORK(didWork);
}

// ---- ?SetCurrentItem@ComboElement@UI@@QEAAXH@Z  0x180112540 ---------------
//
// Wave 7 lane D held this back on TWO named blockers and Wave 9 lane UIW
// repeated one of them. Re-checked here rather than inherited
// (reference_holdback_blockers_are_stale_check_them_first.md):
//
//   (a) "UserInterface::CloseDialog — one of this wave's six phantom names,
//        owned by lane C and not yet fixed" — the PHANTOM is fixed. Wave 7 lane
//        C established the real mangling (by value, not `AEAV…`) and
//        engine_funcs.cpp:1607 resolves it correctly today. The function is
//        still unported, so the door is still null at cutover — but it is
//        engine_funcs' existing door, not a new one, and "unported" is a
//        different claim from "phantom".
//   (b) "a private UpdateText helper this lane did not resolve" — resolved
//        (0x180112180) and PORTED, immediately above.
//
// The body itself allocates nothing and frees nothing. Note what it does NOT
// do: after handing `_openDialog` to CloseDialog it simply ZEROES the field
// (0x18011256e) with no release and no destructor call — Object::InternalPtr is
// a handle, not a refcounted pointer, and the callee only reads the slot.
//
// The out-of-range branch resets `_currentItem` to 0, NOT to -1 (0x1801125a8,
// `MOV dword ptr [RBX+0x118],0x0`). ?RemoveAllItems@ is the only thing that
// leaves -1. Getting this backwards would put the combo on the wrong item
// silently, so it is called out rather than left to the reader.
void RtUi_ComboElement_SetCurrentItem(ComboElement* self, int32_t index) {
    RT_DIFF_BEGIN("UI::ComboElement::SetCurrentItem [no oracle: closes a dialog and rewrites a shared child]");

    if (self->_openDialog != nullptr) {
        // The local slot retail builds at [RSP+0x30] and passes by address.
        void* dialogSlot = self->_openDialog;
        void* ui = *reinterpret_cast<void**>(
            static_cast<uint8_t*>(self->_openDialog) + 0x10);   // DialogInstance::_ui
        CloseDialog()(ui, &dialogSlot);
        self->_openDialog = nullptr;
    }

    if (self->_currentItem == index) { RT_DIFF_WORK(false); return; }

    const bool inRange = (index >= 0 && index < self->_strings._size);
    self->_currentItem = inRange ? index : 0;
    RtUi_ComboElement_UpdateText(self);

    RT_DIFF_WORK(true);
    RT_DIFF_SHADOW(self->_currentItem == (inRange ? index : 0),
                   "current item committed",
                   "not diffable (see the header)");
}

// ============================================================================
// RtUiW2SelfTest — the effectiveness control, with a negative control that
// IS VERIFIED TO FIRE
// ============================================================================
// WHAT IT PROVES, stated narrowly so nobody reads more into a green line than
// is there: that TextParamSlot()/TextParamCount() address the slot retail
// addresses, using the offsets in runtime/types/ui/TextElement.h. That is the
// only arithmetic in this lane that can be wrong in a way no run would notice —
// a wrong stride or base writes a valid-looking String into the wrong place and
// the UI simply shows the wrong text.
//
// IT PROVES NOTHING ABOUT the dirty-flag walk, about CacheText, about the
// combo's child search, or about anything past the two offsets it names. It is
// a control for the arithmetic, not an oracle for the port.
//
// WHY A NEGATIVE CONTROL AT ALL. The positive half of this test cannot fail
// once the offsets are right, so on its own it is a green line that would stay
// green if the test stopped executing — the shape
// reference_probe_needs_an_effectiveness_control.md and lane M's released-
// pointer detector both had to fix the hard way. So the test ALSO runs a
// deliberately mis-scaled computation (stride 4 instead of 8, the single most
// plausible way to get this wrong) and REQUIRES it to disagree. If the negative
// control comes back green, the test says BROKEN and its silence means nothing.
void RtUiW2SelfTest() {
    if (!RtDiffEnabled()) return;

    // A synthetic TextElement: 0x120 bytes is past both offsets this lane uses.
    // Nothing here is a real UI object and nothing is called on it — the test
    // exercises the ADDRESS ARITHMETIC only, which is why it needs no String,
    // no allocator and no DLL.
    alignas(16) uint8_t fake[0x120] = {};
    alignas(8)  uint8_t params[8 * 8] = {};
    *reinterpret_cast<uint8_t**>(fake + UI::TextElementOffsets::kParamArrayData) = params;
    *reinterpret_cast<int32_t*>(fake + UI::TextElementOffsets::kParamCount) = 6;

    const bool posCount = (TextParamCount(fake) == 6);
    const bool posSlot0 = (TextParamSlot(fake, 0) == static_cast<void*>(params));
    const bool posSlot3 = (TextParamSlot(fake, 3) == static_cast<void*>(params + 24));
    const bool positive = posCount && posSlot0 && posSlot3;

    // NEGATIVE CONTROL: the same index under a 4-byte stride. It MUST disagree
    // with the real computation, and for index 0 it would not (0*4 == 0*8),
    // which is why it is taken at index 3.
    const void* misScaled = params + 3 * 4;
    const bool negativeFired = (TextParamSlot(fake, 3) != misScaled);

    if (positive && negativeFired) {
        printf("[UIW2 self-test] ALIVE -- TextElement param slot arithmetic agrees with "
               "TextElement.h (+0x%X data, +0x%X count); the 4-byte-stride negative "
               "control FIRED.\n",
               UI::TextElementOffsets::kParamArrayData, UI::TextElementOffsets::kParamCount);
    } else {
        printf("[UIW2 self-test] *** BROKEN -- its silence means NOTHING *** "
               "positive=%d (count=%d slot0=%d slot3=%d) negativeFired=%d\n",
               (int)positive, (int)posCount, (int)posSlot0, (int)posSlot3,
               (int)negativeFired);
    }
}


// ============================================================================
// WAVE 9 LANE UIW2, PART 2 — the two address doors, ported
// ============================================================================
// Both live in the file's anonymous namespace, which is where their forward
// declarations sit; the registration below takes their address from the same
// translation unit.
namespace {

// ---- ??9String@System@@QEBA_NAEBV01@@Z  0x18003ffe0 -----------------------
//
// `System::String::operator!=`. Retail inlines `operator==` and negates it, and
// the listing shows that shape twice over (`SETZ AL / TEST AL,AL / SETZ AL`):
//
//     lenA = a ? a[-2] : 0                       0x18003ffe6-0x18003fff5
//     lenB = b ? b[-2] : 0                       0x18003fff7-0x180040005
//     if (lenA != lenB)      equal = false       0x18004000b JZ / 0x18004000d
//     else if (!a || lenA==0) equal = true       0x18004001a, 0x18004001f
//     else equal = StrCompare(a, b) == 0         0x180040024
//     return !equal
//
// Note the asymmetry, which is retail's and is preserved: the "both empty"
// shortcut tests only `a`. It is reachable only when the two LENGTHS already
// agree, so a null `b` there necessarily has length 0 too and the answer is the
// same — but the instruction that would have tested `b` does not exist, and a
// port that adds it is not this function.
//
// ZERO DOORS. `RtString_StrCompare` (runtime/src/system/string.h) is our own
// ported body, called directly as C++ — no ResolveAddr, no ResolveExport, and
// therefore nothing left to resolve at cutover. That is what closes the hole:
// the door is DELETED, not moved.
//
// AND IT HAS A REAL DIFFERENTIAL ORACLE — the only one in this lane. The
// function is PURE: it reads two Strings, allocates nothing, mutates nothing.
// So the shipped body can simply be called on the same two arguments and the
// two booleans compared. Everything else this lane touched ends in a refcount
// swap and could not be diffed at all (WAVE9_LANE_UIW2.md §4); this one can,
// so it is.
bool StringNotEqualImpl(const void* lhsRaw, const void* rhsRaw) {
    RT_DIFF_BEGIN("System::String::operator!= [ported by lane UIW2 to close address door 0x18003ffe0]");

    const char* a = *static_cast<const char* const*>(lhsRaw);
    const char* b = *static_cast<const char* const*>(rhsRaw);

    const int32_t lenA = a ? RtSystem::StringLength(a) : 0;
    const int32_t lenB = b ? RtSystem::StringLength(b) : 0;

    bool equal;
    if (lenA != lenB) {
        equal = false;
    } else if (a == nullptr || lenA == 0) {
        equal = true;                       // 0x180040039: MOV AL,1
    } else {
        equal = (RtSystem::RtString_StrCompare(a, b) == 0);
    }
    const bool ours = !equal;

    if (rt_diff_on__) {
        using Fn = bool (*)(const void*, const void*);
        RT_DIFF_SCALAR(ours,
                       RT_ORIGINAL_ONCE(Fn, "??9String@System@@QEBA_NAEBV01@@Z")(lhsRaw, rhsRaw));
    }
    // DID-WORK here means "reached the byte compare", i.e. both operands were
    // non-empty and the same length. Every other path is decided by the lengths
    // alone, and a run where that counter stays 0 has exercised none of
    // StrCompare — which is the half that can be wrong.
    RT_DIFF_WORK(lenA == lenB && a != nullptr && lenA != 0);
    return ours;
}

// ---- ?CacheText@NumberElement@UI@@AEAAXXZ  0x1801189f0 --------------------
//
// NumberElement's OWN CacheText — a DIFFERENT function from
// `?CacheText@TextElement@UI@@AEAAXXZ` (0x18011ec10), which the two setters
// earlier in this file call. Both NumberElement::SetValue bodies call this one.
//
// It formats the live half of the tagged union into the owning refcounted
// System::String at +0xD0:
//
//     mode == 0 : Format(&tmp, _showSign ? "%+i"   : "%i",   _intValue)
//     mode != 0 : Format(&tmp, _showSign ? "%+.2f" : "%.2f", (double)_floatValue)
//     _cachedText = tmp;   // retail's INLINED String::operator=
//     release(tmp)
//
// The four format literals are the DLL's own `.rdata` at 0x180159144 ("%+i"),
// 0x180154848 ("%i"), 0x180159148 ("%+.2f") and 0x180159150 ("%.2f"), read out
// with read_memory and matching the table `runtime/types/ui/NumberElement.h`
// already carries. They are transcribed as C string literals rather than read
// through `ResolveAddr`: a function's own string constants are part of the
// function, and resolving them would have opened FOUR new .data doors to close
// one code door — the exact "porting can grow the cutover surface" trap.
//
// The float branch passes the value as a DOUBLE. Retail's `CVTPS2PD XMM2,XMM2`
// then `MOVQ R8,XMM2` sets BOTH the integer register and the matching XMM for
// argument 3, which is what the Microsoft x64 ABI requires of a variadic float
// and is easy to miss when only the `MOVQ` is read. Passing a `double` through
// a variadic C++ call reproduces it; passing a `float` would not.
//
// `RtSystem_String_Format` (0x1800c9730) is OUR ported body — string.cpp
// registers it — so this is an ordinary C++ call and carries none of the
// cross-toolchain MSVC-variadic risk Wave 7 lane D held NumberEditElement::
// SetValue back for. ZERO NEW DOORS: Format, StrCompare, ReleaseInline and
// Assign are all reached as C++.
//
// *** THE ORACLE IS A REDIRECTED REPLAY, and it is a real one. ***
// The body swaps a refcounted pointer, so a naive replay would run the
// acquire/release twice on one live string — the trap this lane declined
// everywhere else. The way out is the `?StopTimer@` pattern from
// runtime/src/ui/dialog.cpp: copy the object, REPOINT the copy's owning field
// at null so the shipped body allocates its own buffer and can never touch
// ours, run both, and compare the two formatted payloads. The scratch's string
// is released immediately afterwards, so the diff leaks nothing.
//
// The scratch is `sizeof(NumberElement)` = 0xE8 (CreateNewObject 0x18004a910
// allocates 0xE8). NumberElement.h records that an earlier 0xE0 struct made the
// DLL's CacheText read `+0xE0` — the `_showSign` byte — off the end of a stack
// replay and pick its format string from garbage. That is precisely this
// function, and it is why the size is asserted here rather than assumed.
void RtUi_NumberElement_CacheText(void* selfRaw) {
    RT_DIFF_BEGIN("UI::NumberElement::CacheText [redirected replay: the copy's _cachedText is nulled]");

    auto* self = static_cast<uint8_t*>(selfRaw);
    auto* cached = reinterpret_cast<RtSystem::String*>(self + 0xd0);

    // The oracle's scratch, built BEFORE our body runs.
    alignas(16) uint8_t scratch[kNumberElementTrueSize];
    if (rt_diff_on__) {
        std::memcpy(scratch, self, kNumberElementTrueSize);
        *reinterpret_cast<char**>(scratch + 0xd0) = nullptr;   // the redirect
    }

    RtSystem::String tmp{nullptr};
    const int32_t mode      = *reinterpret_cast<const int32_t*>(self + 0xc8);
    const bool    showSign  = *reinterpret_cast<const uint8_t*>(self + 0xe0) != 0;
    if (mode == 0) {
        const int32_t v = *reinterpret_cast<const int32_t*>(self + 0xd8);
        RtSystem_String_Format(&tmp, showSign ? "%+i" : "%i", v);
    } else {
        const float f = *reinterpret_cast<const float*>(self + 0xdc);
        RtSystem_String_Format(&tmp, showSign ? "%+.2f" : "%.2f", static_cast<double>(f));
    }

    const bool didWork = (cached->data != tmp.data);
    RtSystem::RtSystem_String_Assign(cached, &tmp);   // retail's inlined operator=
    RtSystem::RtString_ReleaseInline(&tmp);

    if (rt_diff_on__) {
        using Fn = void (*)(void*);
        RT_ORIGINAL_ONCE(Fn, "?CacheText@NumberElement@UI@@AEAAXXZ")(scratch);
        auto* theirs = reinterpret_cast<RtSystem::String*>(scratch + 0xd0);

        // Compare the TEXT, not the pointer: the two sides allocated separate
        // buffers by construction, so comparing addresses would fail 100% of
        // calls and read as a catastrophic port
        // (reference_oracle_must_not_compare_unwritten_bytes.md, the same
        // mistake one hat over).
        const char* o = cached->data;
        const char* t = theirs->data;
        bool same;
        if (o == nullptr || t == nullptr) {
            same = (o == t);
        } else {
            same = (RtSystem::StringLength(o) == RtSystem::StringLength(t)) &&
                   RtSystem::RtString_StrCompare(o, t) == 0;
        }
        RT_DIFF_SHADOW(same, o ? o : "(null)", t ? t : "(null)");
        RtSystem::RtString_ReleaseInline(theirs);     // the scratch owns it; free it
    }

    RT_DIFF_WORK(didWork);
}

}  // namespace

// ============================================================================
// Registration
// ============================================================================
int RtUiWidgetsInRegister() {
    int count = 0;

    // Wave 9 lane B3 -- a REAL DEPENDENCY row (WAVE9_LANE_V.md 1b); the Wave 7
    // holdback over the variadic String::Format is over, see the note at
    // RealNumberEditElementSetValue.
    RegisterRuntimeExport("?SetValue@NumberEditElement@UI@@QEAAXH@Z",
                          (void*)&NumberEditElement_SetValue); ++count;

    RegisterRuntimeExport(kSpinnerGetMaxValue,  (void*)&RtUi_SpinnerElement_GetMaxValue);  ++count;
    RegisterRuntimeAddr(0x1800d7cb0, (void*)&RtUi_SpinnerElement_GetMaxValue);
    RegisterRuntimeExport(kSpinnerGetValue,     (void*)&RtUi_SpinnerElement_GetValue);     ++count;
    RegisterRuntimeAddr(0x1800d7cf0, (void*)&RtUi_SpinnerElement_GetValue);
    RegisterRuntimeExport(kSpinnerIsActive,     (void*)&RtUi_SpinnerElement_IsActive);     ++count;
    RegisterRuntimeAddr(0x1800d7c30, (void*)&RtUi_SpinnerElement_IsActive);
    RegisterRuntimeExport(kSpinnerSetElementId, (void*)&RtUi_SpinnerElement_SetElementId); ++count;
    RegisterRuntimeAddr(0x1800d7e10, (void*)&RtUi_SpinnerElement_SetElementId);
    RegisterRuntimeExport(kSpinnerSetIncrement, (void*)&RtUi_SpinnerElement_SetIncrement); ++count;
    RegisterRuntimeAddr(0x1800d7d30, (void*)&RtUi_SpinnerElement_SetIncrement);
    RegisterRuntimeExport(kSpinnerSetMinMax,    (void*)&RtUi_SpinnerElement_SetMinMax);    ++count;
    RegisterRuntimeAddr(0x1800d7d40, (void*)&RtUi_SpinnerElement_SetMinMax);
    RegisterRuntimeExport(kSpinnerSetValue,     (void*)&RtUi_SpinnerElement_SetValue);     ++count;
    RegisterRuntimeAddr(0x1800d7dc0, (void*)&RtUi_SpinnerElement_SetValue);

    RegisterRuntimeExport(kComboGetCurrentItem, (void*)&RtUi_ComboElement_GetCurrentItem); ++count;
    RegisterRuntimeAddr(0x180075160, (void*)&RtUi_ComboElement_GetCurrentItem);
    RegisterRuntimeExport(kComboGetItemCount,   (void*)&RtUi_ComboElement_GetItemCount);   ++count;
    RegisterRuntimeAddr(0x180075180, (void*)&RtUi_ComboElement_GetItemCount);
    RegisterRuntimeExport(kComboGetItemData,    (void*)&RtUi_ComboElement_GetItemData);    ++count;
    RegisterRuntimeAddr(0x180075170, (void*)&RtUi_ComboElement_GetItemData);
    // WAVE 9 LANE UIW: two of Wave 7 lane D's three ComboElement hold-backs.
    // Both were held "for the allocator question" and that question is settled
    // (Wave 9 lane E + attempt 14's by-address registration); their five
    // Collection::Array helpers are ports of ours as of this same commit.
    // `SetCurrentItem` stays held back — it needs UserInterface::CloseDialog,
    // which belongs to another lane. See docs/runtime_port/WAVE9_LANE_UIW.md.
    RegisterRuntimeExport("?AddItem@ComboElement@UI@@QEAAHAEBVSprite@2@AEBVString@System@@PEAX@Z",
                          (void*)&RtUi_ComboElement_AddItem); ++count;
    RegisterRuntimeAddr(0x1800750b0, (void*)&RtUi_ComboElement_AddItem);
    RegisterRuntimeExport("?RemoveAllItems@ComboElement@UI@@QEAAXXZ",
                          (void*)&RtUi_ComboElement_RemoveAllItems); ++count;
    RegisterRuntimeAddr(0x180075120, (void*)&RtUi_ComboElement_RemoveAllItems);

    RegisterRuntimeExport(kNumEditGetValue,     (void*)&RtUi_NumberEditElement_GetValue);  ++count;
    RegisterRuntimeAddr(0x1800d4b50, (void*)&RtUi_NumberEditElement_GetValue);
    RegisterRuntimeExport(kNumEditSetMinMax,    (void*)&RtUi_NumberEditElement_SetMinMax); ++count;
    RegisterRuntimeAddr(0x1800d4b70, (void*)&RtUi_NumberEditElement_SetMinMax);

    RegisterRuntimeExport(kNumberSetValueInt,   (void*)&RtUi_NumberElement_SetValueInt);   ++count;
    RegisterRuntimeAddr(0x180118d50, (void*)&RtUi_NumberElement_SetValueInt);
    RegisterRuntimeExport(kNumberSetValueFloat, (void*)&RtUi_NumberElement_SetValueFloat); ++count;
    RegisterRuntimeAddr(0x180118db0, (void*)&RtUi_NumberElement_SetValueFloat);

    RegisterRuntimeExport(kEditSetText,         (void*)&RtUi_EditElement_SetText);         ++count;
    RegisterRuntimeAddr(0x1800d0db0, (void*)&RtUi_EditElement_SetText);

    RegisterRuntimeExport(kRadioSetActiveRadio, (void*)&RtUi_RadioElement_SetActiveRadio); ++count;
    RegisterRuntimeAddr(0x1800d56f0, (void*)&RtUi_RadioElement_SetActiveRadio);
    // WAVE 9 LANE UIW: BOTH FindCheckBoxes overloads, each under its own name
    // AND its own address. Registering only one of an overload set is how
    // engine_funcs.cpp came to name the private sibling for the public symbol
    // (WAVE9_LANE_R.md row 1) — a real entry point that would have run tree
    // surgery on whatever the caller left in RDX rather than faulting.
    RegisterRuntimeExport("?FindCheckBoxes@RadioElement@UI@@QEAAXXZ",
                          (void*)&RtUi_RadioElement_FindCheckBoxes); ++count;
    RegisterRuntimeAddr(0x180047c00, (void*)&RtUi_RadioElement_FindCheckBoxes);
    RegisterRuntimeExport("?FindCheckBoxes@RadioElement@UI@@AEAAXV?$InternalPtr@VElement@UI@@@Object@@@Z",
                          (void*)&RtUi_RadioElement_FindCheckBoxesPrivate); ++count;
    RegisterRuntimeAddr(0x1800d5750, (void*)&RtUi_RadioElement_FindCheckBoxesPrivate);

    RegisterRuntimeExport(kProgressSetMax,      (void*)&RtUi_ProgressElement_SetMaximumValue); ++count;
    RegisterRuntimeAddr(0x180077ec0, (void*)&RtUi_ProgressElement_SetMaximumValue);
    RegisterRuntimeExport(kProgressSetCurrent,  (void*)&RtUi_ProgressElement_SetCurrentValue); ++count;
    RegisterRuntimeAddr(0x180077ed0, (void*)&RtUi_ProgressElement_SetCurrentValue);


    // WAVE 9 LANE UIW2 — the last three of Wave 7 lane D's nine hold-backs that
    // this file could close, plus the private helper one of them needs.
    //
    // REGISTERED TWICE, NAME AND ADDRESS, for the reason widgets_layout.cpp's
    // ?RemoveChildren@ note spells out at length: a port registered under only
    // one of its two doors reads as ported to the census and still reaches the
    // DLL — and, after cutover, null — through the other. tools/
    // port_bypass_audit.py is what catches the half-registered case.
    //
    // Mangled names are spelled out literally here rather than reused from a
    // named constant: tools/runtime_census.py cannot see through one
    // (reference_census_cannot_see_nonliteral_registrations.md).
    RegisterRuntimeExport("?SetParameter@SliderElement@UI@@QEAAXHAEBVString@System@@@Z",
                          (void*)&RtUi_SliderElement_SetParameter); ++count;
    RegisterRuntimeAddr(0x1800d6ca0, (void*)&RtUi_SliderElement_SetParameter);
    RegisterRuntimeExport("?SetProgressTextParameter@ProgressElement@UI@@QEAAXHAEBVString@System@@@Z",
                          (void*)&RtUi_ProgressElement_SetProgressTextParameter); ++count;
    RegisterRuntimeAddr(0x180119b30, (void*)&RtUi_ProgressElement_SetProgressTextParameter);
    RegisterRuntimeExport("?SetCurrentItem@ComboElement@UI@@QEAAXH@Z",
                          (void*)&RtUi_ComboElement_SetCurrentItem); ++count;
    RegisterRuntimeAddr(0x180112540, (void*)&RtUi_ComboElement_SetCurrentItem);
    // ComboElement::UpdateText is PRIVATE and UNEXPORTED — checked with pefile
    // against the shipped export directory, no mangled name contains
    // "UpdateText@ComboElement". Address only, and deliberately NOT counted in
    // `count`, which is the number of EXPORTS this lane registers.
    RegisterRuntimeAddr(0x180112180, (void*)&RtUi_ComboElement_UpdateText);

    // WAVE 9 LANE UIW2, PART 2 — one of the two unregistered ADDRESS doors this
    // lane closed. Registered under NAME and ADDRESS, the usual pair.
    //
    // `?CacheText@NumberElement@UI@@AEAAXXZ` is `AEAA` — PRIVATE, and exported
    // anyway (ord 1679). The comment that used to sit at this function's
    // accessor called it "unexported" and that is why it was an address door.
    RegisterRuntimeExport("?CacheText@NumberElement@UI@@AEAAXXZ",
                          (void*)&RtUi_NumberElement_CacheText); ++count;
    RegisterRuntimeAddr(0x1801189f0, (void*)&RtUi_NumberElement_CacheText);
    // `??9String@System@@QEBA_NAEBV01@@Z` (0x18003ffe0), ported above as
    // StringNotEqualImpl. Registered under BOTH doors and NOT counted: `count`
    // is this lane file's tally of UI exports and this one is a System symbol.
    //
    // Leaving it unregistered was the first plan, on the grounds that
    // runtime/src/system/string.cpp owns that namespace. The measurement said
    // otherwise: with the registration absent, the oracle's own
    // RT_ORIGINAL_ONCE spelling shows up in tools/unsatisfied_export_scan.py as
    // an unsatisfied name (harmlessly — it is inside a diff guard and is
    // compiled out at cutover — but it is still a ported body reachable through
    // neither door). An unregistered port is how `?RemoveChildren@Element@UI@@`
    // became the tree's only address-door bypass; the convention exists for
    // that reason and is followed here.
    //
    // FOR WHOEVER PORTS `System::String` NEXT: this registration is the one to
    // grep for before writing a second body
    // (reference_grep_for_the_port_before_writing_it.md). Move the body to
    // string.cpp and delete these two lines rather than adding a second
    // binding — a duplicate registration is last-binding-wins and silently
    // makes one of the two dead code.
    RegisterRuntimeExport("??9String@System@@QEBA_NAEBV01@@Z",
                          (void*)&StringNotEqualImpl);
    RegisterRuntimeAddr(0x18003ffe0, (void*)&StringNotEqualImpl);

    // The arithmetic control for the three bodies above. No-ops unless the diff
    // harness is on; prints ALIVE or BROKEN, never nothing.
    RtUiW2SelfTest();

    return count;
}
