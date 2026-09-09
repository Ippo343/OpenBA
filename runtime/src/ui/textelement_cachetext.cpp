// ============================================================================
// runtime/src/ui/textelement_cachetext.cpp — WAVE 10, lane UIORACLE
//
//   ?SetParameterCount@TextElement@UI@@AEAAXXZ   retail 0x18011e8e0  (ord 3938)
//   ?CacheText@TextElement@UI@@AEAAXXZ           retail 0x18011ec10  (ord 1680)
//
// Both are `AEAA` — PRIVATE, and this DLL exports its private members, so
// "private" was never the reason they were not ported
// (reference_aeaa_is_private_not_unexported.md).
// ============================================================================
//
// WHY THIS FILE EXISTS AND WHAT THE RECORDED BLOCKER TURNED OUT TO BE
// -------------------------------------------------------------------
// The UI census carries `?CacheText@TextElement@UI@@AEAAXXZ` as "not replayable,
// needs the predictive oracle", parked behind the allocator NO-GO. Both halves
// of that were re-checked from scratch rather than inherited
// (reference_holdback_blockers_are_stale_check_them_first.md):
//
//  * NOT REPLAYABLE — STILL TRUE, and it is the SAME defect Wave 9 lane M
//    found on `NumberElement::SetValue`. This body swaps a refcounted
//    System::String at `this+0xF8`: ours releases the old buffer, and a replay
//    that then hands retail a shallow copy hands it a pointer ours has already
//    freed. WAVE9.md finding (1). A prefix replay here is not a weak oracle,
//    it is a use-after-free.
//
//  * NEEDS THE PREDICTIVE ORACLE — TRUE, AND IT NOW EXISTS.
//    runtime/funcs/rt_predict.h landed in this same wave. Ours predicts and
//    writes NOTHING (so nothing is released and nothing is freed), THEIRS runs
//    for real on pristine state, and we compare. That inversion is exactly
//    what dissolves the blocker above.
//
//  * THE ALLOCATOR "NO-GO" IS NOT LOad-BEARING FOR THIS ROW. This body does not
//    contain an allocator: every allocation and every free it performs is a
//    CALL into a function that is already ported and registered —
//    `String::FormatParamList` (Wave 9 lane B3), `MemoryAllocator::MemFree`
//    (Wave 2), `UI::String::operator System::String` (Wave 7 lane F) and, as of
//    this commit, `Collection::Array<System::String>::SetSize` (0x180029a80).
//    Nothing here decides where memory comes from. **This lane's conclusion
//    therefore does NOT depend on lane ALLOC's re-test of that gate.**
//
//  * ONE DEPENDENCY WAS REAL AND IS CLOSED HERE. `CacheText`'s first act is a
//    call to `SetParameterCount` (0x18011e8e0), whose own callee 0x180029a80
//    was an unported, unexported `Array<System::String>::SetSize`. Wave 9's
//    array_destroy.cpp predicted this exact arrival — "It will get a SECOND
//    caller the day 0x180029a80 … lands" — so 0x180029a80 is ported in
//    runtime/src/collection/array.cpp as `kSpec_StringRefZ` in this commit and
//    is reached through the registry, not by a private second body.
//
// LAYOUT (all offsets read from the two disassemblies, not from decompiled C)
// --------------------------------------------------------------------------
//   this+0xC8  Collection::Array<System::String>._data   (the parameters)
//   this+0xD0  ...                       ._size          <- the branch selector
//   this+0xD4  ...                       ._capacity
//   this+0xD8  UI::String (16 bytes) — the raw text reference
//   this+0xF8  char* — the OWNED, refcounted cached display text
//
// THE TWO BRANCHES, AND WHY THEY NEED DIFFERENT ORACLES
// -----------------------------------------------------
// `_size == 0`  -> the cached text becomes the System::String that
//   `UI::String::operator System::String` returns. That operator is
//   `StringTable::GetText(id)` (0x180002550) under an eight-instruction wrapper:
//   it hands back a refcounted COPY OF THE TABLE'S OWN BUFFER, so ours and
//   retail produce the IDENTICAL POINTER. This branch is exactly comparable.
//
// `_size != 0`  -> `String::FormatParamList` builds a FRESH allocation. Ours and
//   retail allocate different blocks, so the pointer at +0xF8 is NOT comparable
//   and predicting it would fail 100% of calls — the shape
//   reference_oracle_must_not_compare_unwritten_bytes.md warns about, one step
//   over. On this branch the pointer prediction is declared with `Exclude()`,
//   which makes the round record it in the `unverifiable=` column instead of
//   comparing it, and the CONTENT of the two strings is compared instead
//   against a separate RtDiff site. The hole is counted, never silent.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../funcs/rt_diff.h"
#include "../../funcs/rt_predict.h"
#include "../../types/ui/TextElement.h"

// ----------------------------------------------------------------------------
// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// (same reasoning as runtime/src/ui/widgets_layout.cpp and rt_port.h itself:
// that header drags in <windows.h>, which collides with engine type/member
// names). This declaration must stay byte-identical to resolve.h's.
// ----------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

namespace TE = UI::TextElementOffsets;

inline uint8_t* B(void* p, int off) { return reinterpret_cast<uint8_t*>(p) + off; }

// System::String is ONE POINTER (runtime/types/system/String.h). The block in
// front of it is { int32 length; int32 refcount; } and the object's value is
// block+8.
struct Str { char* data; };

inline int32_t& RefCount(void* data) {
    return *reinterpret_cast<int32_t*>(static_cast<char*>(data) - 4);
}
inline int32_t Length(const void* data) {
    return data ? *reinterpret_cast<const int32_t*>(static_cast<const char*>(data) - 8) : 0;
}

// ---- The callees. Every one goes through the port registry ----------------
//
// RT_EXPORT_ONCE, never RT_ORIGINAL_ONCE: these are REAL DEPENDENCIES (the body
// calls them to do work, not to compare against), and the raw door is
// ResolveExportRaw, which returns NULL once Runtime-x64.dll is gone and which
// walks straight past our own ports while it is present
// (reference_bypassed_port_needs_a_source_checker.md).

// ??BString@UI@@QEBA?AV0System@@XZ (0x180035020) — PORTED, runtime/src/ui/
// sprite.cpp. Hidden return slot: RCX=this, RDX=&ret (0x180035020's own
// `MOV qword ptr [RSP+0x10],RDX` / `MOV RAX,RBX`), so the typedef carries it
// (reference_hidden_retslot_wild_write.md).
using UiStringToStringFn = Str* (*)(const void* self, Str* ret);
inline UiStringToStringFn UiStringToString() {
    return RT_EXPORT_ONCE(UiStringToStringFn, "??BString@UI@@QEBA?AV0System@@XZ");
}

// ?FormatParamCount@String@System@@SAHPEBD@Z (0x1800c8d20) — PORTED, Wave 2,
// runtime/src/system/string.cpp. Counts the "@1".."@9" slots in the format.
using FormatParamCountFn = int (*)(const char*);
inline FormatParamCountFn FormatParamCount() {
    return RT_EXPORT_ONCE(FormatParamCountFn, "?FormatParamCount@String@System@@SAHPEBD@Z");
}

// ?FormatParamList@String@System@@SA?AV12@AEBV12@PEBV12@@Z (0x1800c8dc0) —
// PORTED, Wave 9 lane B3. STATIC with a hidden return slot, so the argument
// order is (ret, fmt, params) and NOT (fmt, params). Ghidra's decompiled C
// prints this call with TWO arguments and DROPS the third
// (`MOV R8,qword ptr [RDI+0xc8]` at 0x18011ecbd) — the exact
// reference_ghidra_c_output_vs_disassembly.md failure. The params pointer is
// the parameter Array's DATA pointer, not the Array header.
using FormatParamListFn = Str* (*)(Str* ret, const Str* fmt, const Str* params);
inline FormatParamListFn FormatParamList() {
    return RT_EXPORT_ONCE(FormatParamListFn,
        "?FormatParamList@String@System@@SA?AV12@AEBV12@PEBV12@@Z");
}

// ?MemFree@MemoryAllocator@System@@SAXPEAX@Z (0x1800c7ad0) — PORTED, Wave 2.
using MemFreeFn = void (*)(void*);
inline MemFreeFn MemFree() {
    return RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z");
}

// Collection::Array<System::String>::SetSize(int) — retail 0x180029a80, PORTED
// in this commit (runtime/src/collection/array.cpp, kSpec_StringRefZ). Not
// exported (a header-only template instantiation), so it is reached by address,
// the same idiom widgets_layout.cpp uses for 0x1800764c0 — and through
// ResolveAddr, which consults the PORT REGISTRY FIRST
// (reference_resolveaddr_consults_port_registry_first.md), so this reaches our
// body and not the DLL's.
inline void ArrayStringSetSize(void* arrayHdr, int32_t n) {
    using Fn = void (*)(void*, int32_t);
    static Fn fn = nullptr;                 // cache a hit, retry a miss
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180029a80));
    fn(arrayHdr, n);
}

// Retail's inlined String::Release, ASYMMETRIC exactly as retail leaves it
// (runtime/types/system/String.h): the freeing branch leaves `data - 8` in the
// object, the sharing branch leaves null. Both are reproduced because both are
// observable to a caller that re-reads the slot.
inline void ReleaseInto(Str* slot) {
    char* d = slot->data;
    if (!d) return;
    if (--RefCount(d) == 0) { slot->data = d - 8; MemFree()(slot->data); }
    else                    { slot->data = nullptr; }
}

// ============================================================================
// ?SetParameterCount@TextElement@UI@@AEAAXXZ — retail 0x18011e8e0
// ============================================================================
//
// Resize the parameter Array to however many "@n" slots the current format
// string contains. Straight-line; the only subtlety is that retail takes a
// SECOND reference on the same buffer (RBX at 0x18011e91a-0x18011e922) before
// calling FormatParamCount and releases it afterwards, which is the compiler's
// materialisation of passing the String by value. It is reproduced because the
// refcount traffic is observable, not because the count depends on it.
//
// IT IS IDEMPOTENT, and the CacheText oracle below depends on that: a second
// call computes the same n and 0x180029a80's first instruction pair
// (`CMP EDI,ESI; JZ`) returns immediately.
void SetParameterCountBody(void* self) {
    Str tmp{nullptr};
    UiStringToString()(B(self, TE::kTextString), &tmp);      // 0x18011e903

    char* held = nullptr;                                     // RBX
    if (tmp.data) { held = tmp.data; ++RefCount(tmp.data); }  // 0x18011e91a

    const int n = FormatParamCount()(held);                   // 0x18011e928
    ArrayStringSetSize(B(self, TE::kParamArrayData), n);      // 0x18011e936

    if (held) {                                               // 0x18011e93c
        if (--RefCount(held) == 0) MemFree()(held - 8);
    }
    ReleaseInto(&tmp);                                        // 0x18011e962
}

// ============================================================================
// The CacheText body, written ONCE against an RtSink.
// ============================================================================
//
// `out` carries the ONE store this function makes to the element: the new
// cached-text pointer at +0xF8. In live mode it is a store; in predict mode it
// is a recorded prediction and memory is untouched.
//
// WHAT PREDICT MODE DELIBERATELY DOES NOT DO, and why each is right:
//
//  * IT DOES NOT RELEASE THE OLD BUFFER. A release is a refcount decrement and
//    possibly a free — a real mutation, and the very one that makes the replay
//    oracle a use-after-free. The predictor instead PREDICTS the decremented
//    refcount, but only when the pre-count is > 1, i.e. only when retail will
//    not free the block; when the pre-count is 1 the location is about to stop
//    existing and is declared with Exclude() rather than compared.
//
//  * IT DOES CALL SetParameterCount FOR REAL. That is not an exception to
//    "predict mode writes nothing", it is a stated modelling choice with a
//    reason: the params array is FormatParamList's third argument, so a
//    predictor that skipped the resize would format from a stale (possibly
//    null) params pointer and predict a string retail never produces. Running
//    it is safe because it is idempotent — retail's own call, moments later,
//    finds the size already correct and returns at its first branch. The
//    array header is declared with Exclude() so the write is COUNTED, not
//    silently tolerated.
//
// `outNew` receives the String the body decided on, still holding its
// reference. The caller releases it AFTER Verify(), so the comparison never
// reads a buffer either side has dropped.
void CacheTextBody(void* self, RtSink& out, Str* outNew) {
    SetParameterCountBody(self);                              // 0x18011ec29

    Str tmp{nullptr};
    UiStringToString()(B(self, TE::kTextString), &tmp);       // 0x18011ec42 / ec9c

    Str result{nullptr};                                      // [RBP+0x38]
    Str param0{nullptr};                                      // [RBP+0x40]
    char* newData = nullptr;

    // The branch selector is the parameter Array's _size, which the call above
    // has just set. Retail reads it at 0x18011ec39 — BEFORE the operator call,
    // which is immaterial (the operator does not touch `self`) but is why the
    // decompiled C shows the CMP above both arms.
    if (*reinterpret_cast<int32_t*>(B(self, TE::kParamCount)) == 0) {
        newData = tmp.data;
    } else {
        if (tmp.data) { param0.data = tmp.data; ++RefCount(tmp.data); }  // 0x18011ecb3
        FormatParamList()(&result, &param0,
                          *reinterpret_cast<Str**>(B(self, TE::kParamArrayData)));
        newData = result.data;                                // 0x18011ecd1: R14 = &result
    }

    char* cur = *reinterpret_cast<char**>(B(self, TE::kCachedText));
    if (cur != newData) {                                     // 0x18011ec55 / ecdb
        if (!out.predicting()) {
            if (cur) {
                if (--RefCount(cur) == 0) {
                    // Retail stores the decremented pointer back into the
                    // MEMBER before freeing (`ADD qword ptr [RDI+0xf8],-0x8;
                    // MOV RCX,[RDI+0xf8]`), so MemFree's argument comes out of
                    // the element, not out of a register. Reproduced literally.
                    *reinterpret_cast<char**>(B(self, TE::kCachedText)) = cur - 8;
                    MemFree()(*reinterpret_cast<void**>(B(self, TE::kCachedText)));
                } else {
                    *reinterpret_cast<char**>(B(self, TE::kCachedText)) = nullptr;
                }
            }
        } else if (cur && RefCount(cur) > 1) {
            // The refcount arm, predicted rather than performed. Only where the
            // block survives — see the banner above.
            out.U32(cur - 4, static_cast<uint32_t>(RefCount(cur) - 1),
                    "old cached text: refcount");
        }

        out.Ptr(B(self, TE::kCachedText), newData, "TextElement::_cachedText");

        if (!out.predicting()) {
            if (newData) ++RefCount(newData);
        }
    }

    // Retail releases `result` and `param0` here (0x18011ed1d, 0x18011ed46) and
    // `tmp` last (0x18011ed66). The predictor keeps `newData` alive instead and
    // hands it back; whichever of the three owns it is released by the caller
    // after Verify().
    if (out.predicting()) {
        // Hand back EXACTLY ONE of the references the body already holds on
        // `newData` and release the rest. Taking a fresh reference instead
        // would be simpler and WRONG: `newData` can alias `cur` (the cache was
        // already correct), and a net +1 on a refcount word retail then never
        // touches would read as a divergence produced by the instrument.
        // Handing over one existing reference leaves the count exactly where
        // the live body would have left it.
        outNew->data = newData;
        Str* slots[3] = { &result, &param0, &tmp };
        bool kept = false;
        for (int i = 0; i < 3; ++i) {
            if (!kept && slots[i]->data && slots[i]->data == newData) {
                kept = true;
                slots[i]->data = nullptr;   // ownership moved to outNew
                continue;
            }
            ReleaseInto(slots[i]);
        }
        if (!kept) outNew->data = nullptr;  // newData was null; nothing to hold
        return;
    }

    ReleaseInto(&result);
    ReleaseInto(&param0);
    ReleaseInto(&tmp);
}

// ============================================================================
// The CONTENT oracle for the format branch.
// ============================================================================
//
// It is a REAL oracle, not an RT_DIFF_SHADOW: `theirs` is read out of the
// element AFTER the shipped DLL's own CacheText has run on pristine state, and
// `ours` is the string our predictor built. Nothing here consults our own
// expectation (reference_shadow_marker_is_not_an_oracle.md).
//
// TWO CONTROLS, reported separately, because a green board with neither means
// nothing:
//
//  * EFFECTIVENESS. `changed` counts rounds where the predicted text DIFFERS
//    from the text the element already held. A round where the cache was
//    already correct is satisfied by retail doing nothing at all, and a site
//    whose lifetime `changed` is 0 is reported UNPROVEN, exactly the rule
//    RtPredict::Verify() applies to its own predictions
//    (reference_probe_needs_an_effectiveness_control.md).
//
//  * NEGATIVE CONTROL. Under BANISHED_RT_PREDICT_SELFTEST the predicted text
//    is corrupted by one byte before the comparison, and the comparison must
//    then fail. A round whose predicted text is EMPTY has no byte to corrupt;
//    those are counted as `notApplicable` rather than as passes, because a
//    control that could not have fired is not evidence
//    (reference_diagnostic_field_cannot_express_the_answer.md).
RtDiffSite g_textSite = { "UI::TextElement::CacheText [text content vs DLL]" };

struct TextControl {
    uint64_t rounds = 0;
    // *** THE VERDICT, AND IT WAS INVISIBLE IN THE FIRST DRAFT. *** The
    // divergence was registered with RtDiffReport only, and the [RtDiff]
    // summary prints under BANISHED_RT_DIFF -- a DIFFERENT switch from the
    // BANISHED_RT_PREDICT this oracle runs under. So on a predict-only run
    // every column of this instrument printed except the one that says whether
    // it FAILED: a board that could not express a failure
    // (reference_diagnostic_field_cannot_express_the_answer.md). Counted here
    // and printed unconditionally now.
    uint64_t wrong = 0;
    uint64_t changed = 0;        // predicted text != the text already cached
    uint64_t selfTestSeen = 0;
    uint64_t selfTestCaught = 0;
    uint64_t selfTestNotApplicable = 0;
    uint64_t selfTestMissed = 0;  // corrupted on purpose and STILL agreed
    uint64_t naMismatch = 0;      // mismatch on a self-test round the control could not touch
    uint64_t interned = 0;        // rounds that took the _size == 0 branch
    uint64_t formatted = 0;       // rounds that took the FormatParamList branch
    bool     summaryArmed = false;
} g_text;

void TextSummary() {
    std::fprintf(stderr,
        "[UiOracle] TextElement::CacheText content oracle: rounds=%llu "
        "WRONG=%llu interned=%llu formatted=%llu changed=%llu/%llu "
        "selftest=%llu/%llu (n/a %llu missed %llu na-mismatch %llu)%s\n",
        (unsigned long long)g_text.rounds,
        (unsigned long long)g_text.wrong,
        (unsigned long long)g_text.interned,
        (unsigned long long)g_text.formatted,
        (unsigned long long)g_text.changed, (unsigned long long)g_text.rounds,
        (unsigned long long)g_text.selfTestCaught,
        (unsigned long long)g_text.selfTestSeen,
        (unsigned long long)g_text.selfTestNotApplicable,
        (unsigned long long)g_text.selfTestMissed,
        (unsigned long long)g_text.naMismatch,
        g_text.changed == 0 ? "  *** UNPROVEN: no round changed anything ***" : "");
    std::fflush(stderr);
}

// A private copy of the predicted text, so the comparison survives our own
// release. 4 KB covers every UI string in this game by a wide margin; a longer
// one is reported as TRUNCATED rather than compared, so the cap can never
// quietly manufacture a pass.
char  g_ourText[4096];
bool  g_ourTruncated = false;

// The PRE text, copied before the round. See the effectiveness note at its use:
// `pre` itself is the buffer retail may free, so it cannot be read afterwards.
// g_preLen < 0 means "there was no cached text at all".
char  g_preText[4096];
int   g_preLen = -1;

void SnapshotPre(const char* p) {
    if (!p) { g_preLen = -1; g_preText[0] = '\0'; return; }
    size_t n = std::strlen(p);
    if (n >= sizeof(g_preText)) n = sizeof(g_preText) - 1;
    std::memcpy(g_preText, p, n);
    g_preText[n] = '\0';
    g_preLen = static_cast<int>(n);
}

void SnapshotOurs(const char* p) {
    g_ourTruncated = false;
    if (!p) { g_ourText[0] = '\0'; return; }
    const size_t n = std::strlen(p);
    if (n >= sizeof(g_ourText)) { g_ourTruncated = true; }
    const size_t c = g_ourTruncated ? sizeof(g_ourText) - 1 : n;
    std::memcpy(g_ourText, p, c);
    g_ourText[c] = '\0';
}

} // namespace

// ============================================================================
// The registered entry points.
// ============================================================================

extern "C" void RtUi_TextElement_SetParameterCount(void* self) {
    // NO ORACLE, and the reason is not laziness. Its whole effect is
    // 0x180029a80's, which HAS one — the Collection probe and the per-address
    // RtDiff site array.cpp registers for it. A shadow predicate here would
    // re-derive FormatParamCount's answer in a second spelling and agree with
    // itself by construction, which is the vacuous shape
    // reference_probe_needs_an_effectiveness_control.md rules out. The CacheText
    // round below covers this body's ONE observable output (the parameter
    // array) transitively: it Excludes the header and then formats from it, so
    // a wrong count shows up as a wrong STRING.
    SetParameterCountBody(self);
}

extern "C" void RtUi_TextElement_CacheText(void* self) {
    RT_PREDICT_SITE("UI::TextElement::CacheText");

    if (!RT_PREDICT_ON) {
        RtSink live;
        CacheTextBody(self, live, nullptr);
        return;
    }

    // ---- The predicting round ---------------------------------------------
    Str ours{nullptr};
    char* pre = *reinterpret_cast<char**>(B(self, TE::kCachedText));
    bool  formatted = false;
    SnapshotPre(pre);   // BEFORE the round -- retail may free this buffer

    {
        RtPredictRound round(&rt_predict_site__);

        // Coverage. Watch the whole tail of the element that either body could
        // touch, so a retail write we did not predict is REPORTED rather than
        // ignored — predictions alone are only a lower bound.
        round.Watch(B(self, 0xc0), 0x40, "TextElement +0xC0..+0xFF");

        // SetParameterCount's footprint: the parameter Array header. Ours runs
        // that call FOR REAL (see CacheTextBody's banner), so these bytes are
        // out of the prediction model by construction and are declared, which
        // is what keeps the hole in the `excluded=` column instead of silent.
        round.Exclude(B(self, TE::kParamArrayData), 0x10,
                      "SetParameterCount runs for real (idempotent); its Array "
                      "header is not modelled");

        // THE OLD BUFFER'S REFCOUNT IS PREDICTED BUT DELIBERATELY NOT WATCHED,
        // and the reason is a false positive this lane walked into and backed
        // out of. Predictions are compared whether or not a Watch covers them;
        // Watch exists only to report changes NOTHING predicted. The predictor
        // has to keep one reference on its own answer alive across retail's run
        // so the content comparison does not read a released buffer — and when
        // the cache was already correct that answer IS this same block, so the
        // held reference moves this word by +1 for a reason that belongs to the
        // instrument, not to either body. Watching it would have reported the
        // harness's own bookkeeping as an unpredicted retail write, which is
        // the artefact class RtPredictSite's `superseded`/`unverifiable`
        // columns were appended for in the first place.
        //
        // When retail is about to FREE the block (pre-refcount 1) the location
        // stops existing, so the body makes no prediction for it and the hole
        // is declared here so it prints in `excluded=` rather than vanishing.
        if (pre && RefCount(pre) == 1) {
            round.Exclude(pre - 4, 4,
                          "retail FREES this block (pre-refcount 1); the word "
                          "does not survive the round and is not predicted");
        }

        // Is this the FRESH-ALLOCATION branch? Decided before the body runs so
        // the Exclude can be in place; the body re-reads the same field after
        // SetParameterCount, and SetParameterCount is idempotent, so the two
        // reads agree except on the very first call for a given format — which
        // is why the flag is re-read afterwards and the round is only used for
        // bookkeeping when they disagree.
        SetParameterCountBody(self);   // makes _size authoritative for both
        formatted = (*reinterpret_cast<int32_t*>(B(self, TE::kParamCount)) != 0);

        if (formatted) {
            // A fresh allocation: ours and retail get different blocks, so the
            // POINTER is not comparable and predicting it would fail every
            // call. Declared, so the prediction lands in `unverifiable=` and
            // the content oracle below carries the branch instead.
            round.Exclude(B(self, TE::kCachedText), 8,
                          "FormatParamList allocates; ours and retail get "
                          "different blocks. Content compared separately.");
        }

        CacheTextBody(self, round.sink(), &ours);   // predicts, writes nothing

        // THEIRS. RT_ORIGINAL_ONCE and not RT_EXPORT_ONCE: this call must reach
        // the shipped DLL, and by the time it runs our body is registered under
        // this very name — RT_EXPORT_ONCE would resolve to ourselves and
        // recurse. There is no detour on 0x18011ec10 (it has no exe callers to
        // capture and ten DLL-internal ones we deliberately leave alone), so
        // the raw export IS retail here.
        using Fn = void (*)(void*);
        RT_ORIGINAL_ONCE(Fn, "?CacheText@TextElement@UI@@AEAAXXZ")(self);

        round.Verify();
    }

    // ---- The content oracle ------------------------------------------------
    if (!g_text.summaryArmed) {
        g_text.summaryArmed = true;
        RtDiffRegister(&g_textSite);
        std::atexit(&TextSummary);
    }
    g_text.rounds++;
    formatted ? g_text.formatted++ : g_text.interned++;

    SnapshotOurs(ours.data);

    // EFFECTIVENESS. `preText` was copied BEFORE the round, and that is not
    // fussiness: `pre` is the buffer retail is about to RELEASE, and on the
    // freeing arm retail hands it to MemFree. Comparing against `pre` HERE --
    // which the first draft did -- reads freed memory, and it showed: two runs
    // of the identical 27,198-round route reported changed=4384 and changed=7540
    // from the same deterministic input. A counter that disagrees with itself
    // across a control pair is measuring the allocator, not the port.
    const bool changed = (g_preLen < 0) ? (ours.data != nullptr)
                                        : (std::strcmp(g_preText, g_ourText) != 0);
    if (changed) g_text.changed++;

    // The negative control, and note WHEN it is applied: after `changed`, so
    // corrupting the string cannot inflate the effectiveness number.
    const bool armed = RtPredict::SelfTestArmed();
    bool corrupted = false;
    if (armed) {
        g_text.selfTestSeen++;
        if (g_ourText[0] == '\0') g_text.selfTestNotApplicable++;
        else { g_ourText[0] = static_cast<char>(g_ourText[0] ^ 0x20); corrupted = true; }
    }

    const char* theirs = *reinterpret_cast<const char**>(B(self, TE::kCachedText));
    const bool equal = g_ourTruncated
                         ? false
                         : (theirs == nullptr ? g_ourText[0] == '\0'
                                              : std::strcmp(theirs, g_ourText) == 0);
    if (corrupted) {
        if (!equal) g_text.selfTestCaught++;
        else        g_text.selfTestMissed++;   // corrupted and STILL agreed
    } else if (armed) {
        // Armed but not applicable to this round (empty predicted text). The
        // comparison still ran, but calling a mismatch here a "real divergence"
        // was WRONG in the first draft: it reported WRONG=4 on the self-test run
        // against WRONG=0 on the identical clean run, i.e. it attributed to the
        // port a difference that only the control run had. Counted apart.
        if (!equal) g_text.naMismatch++;
    } else if (!equal) {
        g_text.wrong++;                        // A REAL DIVERGENCE.
    }

    char oursDesc[160], theirsDesc[160];
    std::snprintf(oursDesc, sizeof(oursDesc), "%s\"%.100s\"",
                  g_ourTruncated ? "TRUNCATED " : "", g_ourText);
    std::snprintf(theirsDesc, sizeof(theirsDesc), "\"%.100s\"",
                  theirs ? theirs : "");
    RtDiffReport(&g_textSite, equal, oursDesc, theirsDesc);

    // OUR reference goes back only now — the comparison above read a buffer
    // both sides were still holding.
    ReleaseInto(&ours);
}

// ============================================================================
// Registration. NAME AND ADDRESS, both, for every body — the convention that
// exists because Wave 9 lane B3 registered Element::RemoveChildren by name only
// and left the tree's one address-door bypass.
// ============================================================================
int RtUiTextCacheRegister() {
    int n = 0;
    RegisterRuntimeExport("?SetParameterCount@TextElement@UI@@AEAAXXZ",
                          (void*)&RtUi_TextElement_SetParameterCount); ++n;
    RegisterRuntimeAddr(0x18011e8e0, (void*)&RtUi_TextElement_SetParameterCount);

    RegisterRuntimeExport("?CacheText@TextElement@UI@@AEAAXXZ",
                          (void*)&RtUi_TextElement_CacheText); ++n;
    RegisterRuntimeAddr(0x18011ec10, (void*)&RtUi_TextElement_CacheText);
    return n;
}
