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

// NumberElement's OWN `CacheText`, retail 0x1801189f0 — an unexported private
// helper, disambiguated from `TextElement::CacheText` (0x18011ec10, a
// DIFFERENT function used by SliderElement/ProgressElement's held-back
// exports) by checking `get_xrefs_to` on both: only NumberElement::SetValue's
// two bodies call 0x1801189f0. Called by address, per this project's standing
// convention for internal, non-exported Runtime functions (e.g.
// runtime/src/achievement/goal.cpp).
using CacheTextFn = void (*)(void*);
CacheTextFn NumberElementCacheText() {
    static CacheTextFn fn = reinterpret_cast<CacheTextFn>(ResolveAddr(Module::Runtime, 0x1801189f0));
    return fn;
}

// `?operator!=@String@System@@` (0x18003ffe0) and the static `MemFree`
// (0x1800c7ad0) EditElement::SetText itself calls. Used only by
// RtUi_EditElement_SetText, which is a NO-ORACLE port (WAVE7_LANE_D.md §5) —
// these are resolved by address for the same non-exported-internal reason as
// CacheText above (neither showed up as an exported mangled name in this
// lane's pefile check).
using StringNotEqualFn = bool (*)(const void*, const void*);
StringNotEqualFn StringNotEqual() {
    static StringNotEqualFn fn = reinterpret_cast<StringNotEqualFn>(ResolveAddr(Module::Runtime, 0x18003ffe0));
    return fn;
}
using MemFreeFn = void (*)(void*);
MemFreeFn MemFreeStatic() {
    static MemFreeFn fn = reinterpret_cast<MemFreeFn>(ResolveAddr(Module::Runtime, 0x1800c7ad0));
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
void RtUi_SpinnerElement_SetIncrement(SpinnerElement* self, int increment) {
    RT_DIFF_BEGIN("UI::SpinnerElement::SetIncrement");
    SpinnerElement replay;
    std::memcpy(&replay, self, sizeof(SpinnerElement));

    self->_increment = increment;

    if (rt_diff_on__) {
        using Fn = void (*)(SpinnerElement*, int);
        RT_ORIGINAL_ONCE(Fn, kSpinnerSetIncrement)(&replay, increment);
        RT_DIFF_BYTES(self, &replay, sizeof(SpinnerElement));
    }
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
void RtUi_ProgressElement_SetCurrentValue(ProgressElement* self, float value) {
    RT_DIFF_BEGIN("UI::ProgressElement::SetCurrentValue");
    ProgressElement replay;
    std::memcpy(&replay, self, sizeof(ProgressElement));

    self->_currentValue = value;

    if (rt_diff_on__) {
        using Fn = void (*)(ProgressElement*, float);
        RT_ORIGINAL_ONCE(Fn, kProgressSetCurrent)(&replay, value);
        RT_DIFF_BYTES(self, &replay, sizeof(ProgressElement));
    }
}

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

    RegisterRuntimeExport(kProgressSetMax,      (void*)&RtUi_ProgressElement_SetMaximumValue); ++count;
    RegisterRuntimeAddr(0x180077ec0, (void*)&RtUi_ProgressElement_SetMaximumValue);
    RegisterRuntimeExport(kProgressSetCurrent,  (void*)&RtUi_ProgressElement_SetCurrentValue); ++count;
    RegisterRuntimeAddr(0x180077ed0, (void*)&RtUi_ProgressElement_SetCurrentValue);

    return count;
}
