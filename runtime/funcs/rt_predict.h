#pragma once
// ============================================================================
// runtime/funcs/rt_predict.h — the PREDICTIVE shadow oracle
// ============================================================================
//
// WHY THIS EXISTS, AND WHY `RT_DIFF_SHADOW` IS NOT IT.
//
// Wave 7 lane C held back `UI::UserInterface::AddDialog` with one sentence:
// "A shadow oracle over the post-state is possible and is the obvious next
// step for whoever wants it; this lane did not write one it could not run."
// The same sentence, in other words, sits on `UpdateContainers@TabElement`,
// `AddTab@TabElement` and `AddDialog`.
//
// The tree already has ~250 `RT_DIFF_SHADOW` sites. **Not one of them is that
// oracle.** Every one has the same shape:
//
//     ours runs FOR REAL on the live object
//     ... then a hand-written predicate asserts something about the result
//     RT_DIFF_SHADOW(predicate, "ours", "theirs")
//
// That compares our body against OUR OWN EXPECTATION. The DLL is never
// consulted. It is a post-condition self-check — genuinely useful, and it is
// what `rt_diff.h` means by "ours computes into a private replica" — but the
// only thing it can catch is our body disagreeing with our own belief about
// our body. A shared misunderstanding of retail passes it every time.
//
// THE INVERSION THIS FILE IS.
//
//     ours runs in PREDICT mode and writes NOTHING — it records, for each
//         location it would have written, the value it would have written
//     THEIRS runs FOR REAL on the live object, on pristine state
//     we compare every predicted value against what the DLL actually left
//
// That is an oracle: the authority is the shipped DLL, not our expectation.
//
// WHY IT UNLOCKS WHAT THE REPLAY ORACLE CANNOT.
//
// The replay oracle (`RT_DIFF_BYTES`) needs a scratch COPY: run ours on the
// live object, memcpy a scratch, run theirs on the scratch, compare. Two things
// defeat it, and they are exactly the two that hold the UI mutators back:
//
//  1. **The mutated state is a GRAPH, not a buffer.** `UpdateContainers` walks
//     `child->parent` chains and writes flag words in objects that are not
//     inside any copyable extent. `SetUpperLeft` (lane B, §2b) died on the same
//     rock: "there is no scratch copy to run the original against, because the
//     state it mutates is a tree of objects the DialogInstance only points at."
//     A predictor needs no copy — it never writes, so there is nothing to undo.
//
//  2. **Guarded side effects go untested under replay, and the compare still
//     passes.** This is the sharp one. `UpdateContainers` propagates a dirty
//     bit up the parent chain, guarded by *did this bit actually change*:
//
//         if ((uVar1 & 1) != 0) { do { parent->flags |= 0x8000000; ... } }
//
//     Under replay, OUR body has already set the bit, so when the DLL's body
//     runs second the guard is false and it never walks the chain at all. The
//     byte compare is green and the only risky path was never executed. Lane C
//     named this shape precisely, on `EnableCursor`: "our body would have
//     already set the Application flag, so the DLL's replay takes the early-out
//     and never exercises the tree walk at all. The object compare would pass
//     while the only thing that can go wrong went untested."
//
//     In predict mode our body writes nothing, so the DLL runs on PRISTINE
//     state and the guard fires exactly as it does in a normal frame. The path
//     is exercised, and it is exercised by the authority, not by us.
//
// HOW A BODY SERVES BOTH MODES WITHOUT BEING WRITTEN TWICE.
//
// Writing the body twice — once live, once as a predictor — would be its own
// divergence hazard, and a predictor that drifts from the body verifies
// nothing. So the body is written ONCE against an `RtSink`, exactly the way
// lane S's save-trace oracle made the STREAM synthetic instead of duplicating
// `Serialize`:
//
//     void UpdateContainers(TabElement* self, RtSink& out) {
//         ...
//         out.U32(&child->flags, newFlags, "child.flags");
//     }
//
// A live sink stores; a predicting sink records. One body, one set of offsets,
// one control flow. The live path costs one predictable branch per write.
//
// WHAT IS CHECKED, AND THE THREE CONTROLS THAT MAKE THE GREEN MEAN SOMETHING
//
//  * **Every predicted location** is compared against what the DLL left, and a
//    mismatch names the LABEL, the pre-value, ours and theirs — never just
//    "they differ". (A verdict computed over N terms whose message names none
//    of them is the defect `reference_shadow_predicate_must_name_its_failing_term`
//    was written about.)
//
//  * **`changed=` — the effectiveness control.** A prediction whose predicted
//    value equals the pre-value is satisfied by the DLL doing NOTHING. A round
//    made entirely of those is a green board that proves nothing. `Verify()`
//    counts predictions where `predicted != pre` and the summary prints
//    `changed=N/M`. **A site whose lifetime `changed` is 0 is reported as
//    UNPROVEN, not as passing**, for the same reason `worked=?` prints as `?`.
//
//  * **`unpredicted=` — the coverage control.** Predictions alone are a lower
//    bound: they cannot catch a write the DLL makes that our body does not know
//    about. `Watch(base, n, label)` snapshots a region before the DLL runs;
//    afterwards, any byte inside it that CHANGED and is not covered by a
//    prediction is reported as an unpredicted write. This is what makes the
//    round an oracle rather than a spot check. Regions the body deliberately
//    does not model are declared with `Exclude(base, n, why)` and are counted
//    separately, so "not covered" can never quietly read as "covered".
//
//  * **The negative control.** `BANISHED_RT_PREDICT_SELFTEST=1` corrupts one
//    prediction per site by one bit. Every site must then report a mismatch.
//    A site that stays green under self-test is STRUCTURALLY INCAPABLE of
//    failing, and its clean run was never evidence — the question
//    `reference_diagnostic_field_cannot_express_the_answer` says to ask of any
//    instrument: *could this field express the answer if the answer were
//    different?* The summary prints the self-test verdict per site.
//
// WHAT IT STILL CANNOT DO — stated because a partial oracle that reads like a
// full one is worse than none:
//
//  * **An indirect call the body would make is not executed in predict mode**,
//    so its EFFECT is not predicted. `out.Call()` records the target and the
//    arguments, and the target is checkable (it is a vtable slot both sides
//    load). The callee's writes are not, and the region it writes must be
//    declared with `Exclude()`. Porting the callee is what closes that, and
//    until then the summary prints `excluded=` on the site.
//
//  * **The port does not run in predict mode.** A predicting round hands the
//    frame to the DLL. So predict mode is a VERIFICATION mode, like
//    `BANISHED_RT_MINT_SHADOW`, not a shipping configuration. In a normal run
//    the live sink is used and there is no oracle — which is exactly the trade
//    the replay oracle also makes, one level over.
//
// GATE: BANISHED_RT_PREDICT=1. Off by default and free when off.
// ============================================================================

#include <cstdint>
#include <cstddef>

// ---------------------------------------------------------------------------
// The write sink. One body, two modes.
//
// Deliberately NOT virtual: these calls sit in ported Runtime bodies that run
// per frame per element, and the live path must stay a store behind one
// well-predicted branch. `log_` is null in live mode and the branch is
// perfectly predicted in both.
// ---------------------------------------------------------------------------

struct RtPredictRound;

class RtSink {
public:
    // Live sink — writes go through to memory. This is the shipping mode.
    RtSink() : round_(nullptr) {}

    // Predicting sink — writes are recorded, memory is untouched.
    explicit RtSink(RtPredictRound* r) : round_(r) {}

    bool predicting() const { return round_ != nullptr; }

    // Record-or-store. `label` must be a string literal or otherwise outlive
    // the round; it is stored by pointer, never copied.
    void U8 (void* addr, uint8_t     v, const char* label);
    void U16(void* addr, uint16_t    v, const char* label);
    void U32(void* addr, uint32_t    v, const char* label);
    void U64(void* addr, uint64_t    v, const char* label);
    void Ptr(void* addr, const void* v, const char* label);

    // ---- READ-BACK: a predictor must be self-consistent --------------------
    //
    // In live mode a body that stores to X and later re-reads X sees its own
    // store. In predict mode nothing was stored, so a plain `*(uint32_t*)X`
    // would read the PRE value and the body would compute from stale state.
    //
    // This is not hypothetical: `UpdateContainers` stores the child's new flag
    // word and then, in the guarded branch, ORs the dirty bit into *the same
    // word* — retail re-reads it from memory. A predictor that missed this
    // would predict `old | 0x8000000` where retail leaves
    // `(old & ~1) | 0x8000000`, and would do it on exactly the path the guard
    // makes rare.
    //
    // So reads that MAY hit a location this body has already written must go
    // through these: live mode reads memory, predict mode returns the most
    // recent prediction for that address if there is one and memory otherwise.
    // Reads of locations the body never writes can stay plain loads.
    uint8_t     ReadU8 (const void* addr) const;
    uint16_t    ReadU16(const void* addr) const;
    uint32_t    ReadU32(const void* addr) const;
    uint64_t    ReadU64(const void* addr) const;
    void*       ReadPtr(const void* addr) const;

    // An indirect (usually vtable) call the body would make.
    //
    // Live mode: nothing is recorded and the function returns true, meaning
    //            "make the call". The body makes it.
    // Predict mode: the target and up to four integer arguments are recorded,
    //            and the function returns FALSE, meaning "do not make it".
    //
    // So the body reads:
    //     if (out.Call("Element::SetBounds", slot, a, b, c, d))
    //         slot(child, a, b, c, d);
    //
    // The callee's writes are NOT predicted; declare its footprint with
    // RtPredict::Exclude or the coverage control will report them as
    // unpredicted, which is the correct and intended complaint.
    bool Call(const char* label, const void* target,
              int64_t a0 = 0, int64_t a1 = 0, int64_t a2 = 0, int64_t a3 = 0);

private:
    RtPredictRound* round_;
};

// ---------------------------------------------------------------------------
// The round. One construction = one verified call of one function.
// ---------------------------------------------------------------------------

namespace RtPredict {

// True when BANISHED_RT_PREDICT is set to a non-"0" value. Cached.
bool Enabled();

// True when BANISHED_RT_PREDICT_SELFTEST is set — the negative control.
bool SelfTestArmed();

// Per-site totals, printed beside [RtDiff] at shutdown.
void Summary();

} // namespace RtPredict

// One site's lifetime counters. A function-local static, like RtDiffSite.
//
// APPEND ONLY, for the same reason RtDiffSite is: sites initialise this as
// `= { NAME }` and rely on C++ value-initialising the rest.
struct RtPredictSite {
    const char* name          = nullptr;
    uint64_t    rounds        = 0;  // rounds entered
    uint64_t    verified      = 0;  // rounds that reached Verify()
    uint64_t    predictions   = 0;  // total predicted locations
    uint64_t    changed       = 0;  // of those, where predicted != pre  (EFFECTIVENESS)
    uint64_t    wrong         = 0;  // of those, where predicted != actual
    uint64_t    unpredicted   = 0;  // changed bytes in a watched region with no prediction
    uint64_t    excludedBytes = 0;  // bytes declared out of model
    uint64_t    calls         = 0;  // indirect calls suppressed in predict mode
    // Appended after the first real subject. Both exist because the first run
    // of UI::TabElement::UpdateContainers reported two mismatches that were
    // artefacts of the HARNESS, not of the port.
    uint64_t    superseded    = 0;  // predictions overwritten by a later one to
                                    // the same address: an INTERMEDIATE value,
                                    // never a claim about the post-state
    uint64_t    unverifiable  = 0;  // predictions landing in an Exclude region:
                                    // recorded for read-back, not compared
    bool        linked        = false;
    bool        selfTestSeen  = false;  // a self-test round ran on this site
    bool        selfTestCaught= false;  // ...and it was caught. Both must be true.
};

// A verification round. Construct, hand `sink()` to the body, run the original,
// then Verify(). The destructor verifies if you did not, so an early return
// cannot silently drop a round.
struct RtPredictRound {
    RtPredictRound(RtPredictSite* site);
    ~RtPredictRound();

    // The sink to hand to the body being verified.
    RtSink& sink() { return sink_; }

    // Snapshot a region so that changes to it the body did NOT predict are
    // reported. Call before running the body. Regions may overlap predictions.
    void Watch(const void* base, size_t n, const char* label);

    // Declare a region the body deliberately does not model (typically the
    // footprint of a callee suppressed by RtSink::Call). Bytes here are not
    // reported as unpredicted, and the count is printed on the site so the
    // hole stays visible.
    void Exclude(const void* base, size_t n, const char* why);

    // Compare. Safe to call once; the destructor calls it if you did not.
    void Verify();

    // True EXACTLY ONCE per round, and only when the oracle is on. This is the
    // loop condition behind RT_PREDICT_ROUND: it makes the `for` a one-shot
    // scope whose increment expression is Verify(), so an early `return` or
    // `break` out of the block still verifies (via the destructor) instead of
    // silently dropping the round.
    bool pending();

    // Internal — used by RtSink.
    void Note(void* addr, const void* value, size_t size, const char* label);
    // Most recent prediction covering [addr, addr+size), or false if none.
    bool Peek(const void* addr, size_t size, uint64_t* out) const;
    void NoteCall(const char* label, const void* target,
                  int64_t a0, int64_t a1, int64_t a2, int64_t a3);

private:
    RtPredictSite* site_;
    RtSink         sink_;
    bool           verified_;
    bool           entered_;  // pending() has been asked once
    bool           active_;   // false when the oracle is off — everything no-ops
    void*          state_;    // opaque, points into a fixed pool (no allocation)
};

// ---------------------------------------------------------------------------
// Macros
// ---------------------------------------------------------------------------

// Declare the site and a live sink. Zero cost when the oracle is off.
//
//     RT_PREDICT_SITE("UI::TabElement::UpdateContainers");
//     if (RT_PREDICT_ON) { ... predicting round ... }
//     else               { Body(self, RtSink()); }
#define RT_PREDICT_SITE(NAME)                                                  \
    static RtPredictSite rt_predict_site__ = { NAME };                         \
    const bool rt_predict_on__ = RtPredict::Enabled()

#define RT_PREDICT_ON (rt_predict_on__)

// The whole predicting round in one statement, for the common shape:
// predict with ours, run theirs for real, verify.
//
//     RT_PREDICT_ROUND(round) {
//         round.Watch(self, sizeof(*self), "self");
//         Body(self, round.sink());
//         theirs(self);
//     }   // Verify() runs here
#define RT_PREDICT_ROUND(VAR)                                                  \
    for (RtPredictRound VAR(&rt_predict_site__); VAR.pending(); VAR.Verify())
