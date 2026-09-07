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
//      sizeof(OurStruct) is a LOWER BOUND on the true retail object (this
//      lane has not reversed every byte of any of its eight classes), so the
//      oracle proves everything IN that range and is silent about anything
//      past it — stated once here rather than on every function.
// ============================================================================

#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
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
// ResolveAddr / ResolveExportRaw, forward-declared rather than pulled in from
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
    static IsDerivedFromFn fn = reinterpret_cast<IsDerivedFromFn>(
        ResolveExportRaw("?IsDerivedFrom@Base@Object@@QEBA_NI@Z"));
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
ToIntegerFn StringToInteger() {
    static ToIntegerFn fn = reinterpret_cast<ToIntegerFn>(
        ResolveExportRaw("?ToInteger@String@System@@SAHPEBD@Z"));
    return fn;
}

// The REAL `?SetValue@NumberEditElement@UI@@QEAAXH@Z` (0x1800d4bb0), used as
// an opaque dependency by SpinnerElement::SetMinMax/SetValue and
// NumberEditElement::SetMinMax — exactly the way WAVE6_LANE_A.md's SetCamera
// calls Math::Matrix::Inverse through ResolveExport rather than reimplementing
// it. NumberEditElement::SetValue's OWN body is HELD BACK by this lane (it
// calls the variadic `System::String::Format`, which this lane declined to
// reproduce — WAVE7_LANE_D.md §3) but calling the DLL's real implementation
// here is a plain, non-variadic call site and carries none of that risk.
using NumberEditSetValueFn = void (*)(NumberEditElement*, int);
NumberEditSetValueFn RealNumberEditElementSetValue() {
    static NumberEditSetValueFn fn = reinterpret_cast<NumberEditSetValueFn>(
        ResolveExportRaw("?SetValue@NumberEditElement@UI@@QEAAXH@Z"));
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
void RtUi_NumberEditElement_SetMinMax(NumberEditElement* self, int minVal, int maxVal) {
    RT_DIFF_BEGIN("UI::NumberEditElement::SetMinMax");
    NumberEditElement replay;
    std::memcpy(&replay, self, sizeof(NumberEditElement));

    self->_min = minVal;
    self->_max = maxVal;
    int clamped = StringToInteger()(self->base._text);
    RealNumberEditElementSetValue()(self, clamped);

    if (rt_diff_on__) {
        using Fn = void (*)(NumberEditElement*, int, int);
        RT_ORIGINAL_ONCE(Fn, kNumEditSetMinMax)(&replay, minVal, maxVal);
        RT_DIFF_BYTES(self, &replay, sizeof(NumberEditElement));
    }
}

// ============================================================================
// NumberElement — 2/2 ported
// ============================================================================

// ---- ?SetValue@NumberElement@UI@@QEAAXH@Z  0x180118d50 ---------------------
void RtUi_NumberElement_SetValueInt(NumberElement* self, int value) {
    RT_DIFF_BEGIN("UI::NumberElement::SetValueInt");
    NumberElement replay;
    std::memcpy(&replay, self, sizeof(NumberElement));

    if (!(self->_mode == 0 && self->_intValue == value)) {
        self->_intValue = value;
        self->_floatValue = 0.0f;   // retail zeroes the raw dword; 0 == 0.0f bitwise.
        self->_mode = 0;
        MarkNeedsSizeCalcUpToRoot(&self->base);
        NumberElementCacheText()(self);
    }

    if (rt_diff_on__) {
        using Fn = void (*)(NumberElement*, int);
        RT_ORIGINAL_ONCE(Fn, kNumberSetValueInt)(&replay, value);
        RT_DIFF_BYTES(self, &replay, sizeof(NumberElement));
    }
}

// ---- ?SetValue@NumberElement@UI@@QEAAXM@Z  0x180118db0 ---------------------
void RtUi_NumberElement_SetValueFloat(NumberElement* self, float value) {
    RT_DIFF_BEGIN("UI::NumberElement::SetValueFloat");
    NumberElement replay;
    std::memcpy(&replay, self, sizeof(NumberElement));

    if (!(self->_mode == 1 && self->_floatValue == value)) {
        self->_floatValue = value;
        self->_intValue = 0;
        self->_mode = 1;
        MarkNeedsSizeCalcUpToRoot(&self->base);
        NumberElementCacheText()(self);
    }

    if (rt_diff_on__) {
        using Fn = void (*)(NumberElement*, float);
        RT_ORIGINAL_ONCE(Fn, kNumberSetValueFloat)(&replay, value);
        RT_DIFF_BYTES(self, &replay, sizeof(NumberElement));
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
