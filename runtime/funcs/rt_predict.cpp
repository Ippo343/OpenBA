// ============================================================================
// runtime/funcs/rt_predict.cpp — the PREDICTIVE shadow oracle
// ============================================================================
// See rt_predict.h for the contract, for why the ~250 existing RT_DIFF_SHADOW
// sites are not this, and for the three controls that make a green run mean
// something.
//
// ALLOCATION: none, anywhere in this file. Rounds run inside ported Runtime
// bodies, which execute before static constructors and — once the allocator is
// ours — inside allocation itself. Everything lives in fixed pools, exactly as
// rt_diff.cpp's site array does and for the same reason.
// ============================================================================

#include "rt_predict.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---- Enable flags ----------------------------------------------------------

namespace {

bool EnvOn(const char* name) {
    const char* e = getenv(name);
    return e && *e && strcmp(e, "0") != 0 && e[0] != 'f' && e[0] != 'F';
}

} // namespace

bool RtPredict::Enabled() {
    static int on = -1;
    if (on < 0) {
        on = EnvOn("BANISHED_RT_PREDICT") ? 1 : 0;
        if (on) {
            fprintf(stderr,
                "[RtPredict] ON — ported bodies run as PREDICTORS and write nothing; "
                "the DLL does the real work and we compare. This is a verification "
                "mode, not a shipping one: our ports are not live while it is set.\n");
            fflush(stderr);
        }
    }
    return on != 0;
}

bool RtPredict::SelfTestArmed() {
    static int on = -1;
    if (on < 0) {
        on = EnvOn("BANISHED_RT_PREDICT_SELFTEST") ? 1 : 0;
        if (on) {
            fprintf(stderr,
                "[RtPredict] SELF-TEST ARMED — one prediction per site is corrupted by "
                "one bit. EVERY site must report a mismatch. A site that stays green "
                "here cannot express a failure, and its clean runs were never evidence.\n");
            fflush(stderr);
        }
    }
    return on != 0;
}

// ---- Site registry ---------------------------------------------------------

namespace {

constexpr size_t kMaxSites = 256;
RtPredictSite*   g_sites[kMaxSites];
size_t           g_siteCount = 0;

// How many mismatches to log in full per site before falling back to counting.
constexpr uint64_t kMaxVerbose = 3;

void Register(RtPredictSite* s) {
    if (!s || s->linked) return;
    if (g_siteCount < kMaxSites) {
        g_sites[g_siteCount++] = s;
        s->linked = true;
    }
}

// ---- Round state pool ------------------------------------------------------
//
// Rounds do not nest in the intended use (one verified call at a time), but a
// ported body that itself calls another ported body would nest, so the pool is
// a small stack rather than a single slot. Overflow degrades to "round
// inactive" — never to a wrong answer.

constexpr size_t kMaxPredictions = 512;   // per round
constexpr size_t kMaxWatches     = 32;
constexpr size_t kMaxExcludes    = 32;
constexpr size_t kMaxCalls       = 64;
constexpr size_t kSnapshotBytes  = 64 * 1024;  // per round, across all watches

struct Prediction {
    void*       addr;
    const char* label;
    size_t      size;
    uint64_t    predicted;   // value the body says it would have written
    uint64_t    pre;         // value at addr when the prediction was made
    bool        corrupted;   // this one was bit-flipped by the self-test
};

// Does a LATER prediction `b` make an EARLIER prediction `a` uncomparable?
//
// It does whenever their byte ranges OVERLAP AT ALL — not only when the address
// and width match exactly, which is what this test used to require.
//
// WHY THE EXACT-MATCH RULE WAS WRONG (Wave 10 lane UI1, found by the first run
// of the UI::UserInterface::Update oracle, which reported three mismatches that
// were not defects). Retail's fade teardown writes two DIFFERENT WIDTHS to
// overlapping addresses inside one call:
//
//     0x1801215ae  MOVSS dword[RSI+0xf4], XMM1   ; _fadeAmount = f
//     ...
//     0x1801215d7  MOV   qword[RSI+0xf0], 0      ; _fadeVelocity AND _fadeAmount
//
// The second store COVERS the first. The port predicts both, faithfully, and
// its final claim about +0xf4 is 0 — which is exactly what the DLL leaves. But
// the earlier 4-byte prediction at +0xf4 has neither the same address nor the
// same size as the later 8-byte one at +0xf0, so the old rule did not see it as
// superseded, compared an INTERMEDIATE value against final memory, and printed
// `pre=0x38281000 ours=0xbd887221 theirs=0x0` three times. That is the
// manufactured-mismatch failure this check exists to prevent, arriving through
// the one door the check did not cover.
//
// THE DIRECTION OF THE RISK, stated because it matters: this rule can only move
// a prediction from COMPARED to NOT COMPARED, so a bug in it hides defects
// rather than inventing them. Two things keep that honest. First, every
// suppressed prediction is counted and printed as `superseded=`, so the hole is
// visible. Second — and this is what makes the widened rule safe rather than
// merely convenient — the LATER, WIDER prediction is itself compared, so the
// post-state of those bytes is still under the oracle. In the case above the
// 8-byte prediction at +0xf0 passed; nothing stopped being verified, only the
// intermediate stopped being misread as a post-state claim.
//
// A PARTIAL overlap (the later write covers only some of the earlier bytes)
// leaves the earlier prediction's full-width value uncomparable too, and is
// treated the same way. It has no instance in the tree today; if one appears it
// will show up as `superseded` rather than as a false red, which is the correct
// side to fail on.
inline bool RangesOverlap(const void* aAddr, size_t aSize,
                          const void* bAddr, size_t bSize) {
    const uint8_t* a0 = static_cast<const uint8_t*>(aAddr);
    const uint8_t* b0 = static_cast<const uint8_t*>(bAddr);
    return a0 < b0 + bSize && b0 < a0 + aSize;
}

struct WatchRegion {
    const uint8_t* base;
    size_t         size;
    const char*    label;
    size_t         snapOffset;   // into RoundState::snapshot
};

struct ExcludeRegion {
    const uint8_t* base;
    size_t         size;
    const char*    why;
};

struct CallRecord {
    const char* label;
    const void* target;
    int64_t     a[4];
};

struct RoundState {
    bool          inUse;
    Prediction    preds[kMaxPredictions];
    size_t        predCount;
    bool          predOverflow;
    // Added 2026-09-09. The prediction and watch pools already flagged their
    // overflow; the call and exclude pools silently dropped records, which is
    // the same class of defect as a reporting cap manufacturing a calls=0
    // signal (reference_truncated_tool_output_manufactures_calls_zero.md).
    bool          callOverflow;
    bool          excludeOverflow;

    WatchRegion   watches[kMaxWatches];
    size_t        watchCount;
    bool          watchOverflow;

    ExcludeRegion excludes[kMaxExcludes];
    size_t        excludeCount;

    CallRecord    calls[kMaxCalls];
    size_t        callCount;

    uint8_t       snapshot[kSnapshotBytes];
    size_t        snapUsed;

    // The self-test picks one prediction per ROUND to corrupt. Chosen as the
    // first prediction that predicts an actual change, because corrupting a
    // no-op prediction would be caught only by luck and would make the control
    // itself unreliable.
    bool          selfTestApplied;
};

constexpr size_t kMaxDepth = 4;
RoundState g_rounds[kMaxDepth];

RoundState* AcquireRound() {
    for (size_t i = 0; i < kMaxDepth; i++) {
        if (!g_rounds[i].inUse) {
            RoundState* r = &g_rounds[i];
            r->inUse = true;
            r->predCount = 0;      r->predOverflow  = false;
            r->callOverflow = false; r->excludeOverflow = false;
            r->watchCount = 0;     r->watchOverflow = false;
            r->excludeCount = 0;
            r->callCount = 0;
            r->snapUsed = 0;
            r->selfTestApplied = false;
            return r;
        }
    }
    return nullptr;
}

bool InAnyExclude(const RoundState* r, const uint8_t* p) {
    for (size_t i = 0; i < r->excludeCount; i++) {
        const ExcludeRegion& e = r->excludes[i];
        if (p >= e.base && p < e.base + e.size) return true;
    }
    return false;
}

bool InAnyPrediction(const RoundState* r, const uint8_t* p) {
    for (size_t i = 0; i < r->predCount; i++) {
        const uint8_t* a = static_cast<const uint8_t*>(r->preds[i].addr);
        if (p >= a && p < a + r->preds[i].size) return true;
    }
    return false;
}

uint64_t ReadValue(const void* addr, size_t size) {
    uint64_t v = 0;
    memcpy(&v, addr, size);
    return v;
}

} // namespace

// ---- Round -----------------------------------------------------------------

RtPredictRound::RtPredictRound(RtPredictSite* site)
    : site_(site), sink_(), verified_(false), entered_(false),
      active_(false), state_(nullptr)
{
    if (!site || !RtPredict::Enabled()) return;
    Register(site);
    RoundState* rs = AcquireRound();
    if (!rs) {
        // Pool exhausted. Degrade to inactive rather than to a wrong answer:
        // an inactive round makes no predictions, so it reports nothing and
        // cannot manufacture a pass.
        fprintf(stderr, "[RtPredict] %s: round pool exhausted (depth %zu) — "
                        "THIS CALL IS NOT VERIFIED\n",
                site->name ? site->name : "(unnamed)", kMaxDepth);
        return;
    }
    state_  = rs;
    active_ = true;
    sink_   = RtSink(this);
    site_->rounds++;
}

RtPredictRound::~RtPredictRound() {
    // An early return or a `break` out of RT_PREDICT_ROUND must not silently
    // drop the round — that would read in the summary as "fewer calls", which
    // is the calls=0 ambiguity one level up.
    if (active_ && !verified_) Verify();
    if (state_) static_cast<RoundState*>(state_)->inUse = false;
}

bool RtPredictRound::pending() {
    if (entered_) return false;
    entered_ = true;
    return active_;
}

void RtPredictRound::Note(void* addr, const void* value, size_t size,
                          const char* label) {
    if (!active_ || !addr || size == 0 || size > 8) return;
    RoundState* r = static_cast<RoundState*>(state_);
    if (r->predCount >= kMaxPredictions) { r->predOverflow = true; return; }

    Prediction& p = r->preds[r->predCount++];
    p.addr      = addr;
    p.label     = label ? label : "(unlabelled)";
    p.size      = size;
    p.predicted = 0;
    memcpy(&p.predicted, value, size);
    p.pre       = ReadValue(addr, size);
    p.corrupted = false;

    // THE NEGATIVE CONTROL IS NOT APPLIED HERE -- see Verify(). It used to be,
    // and it was WRONG in a way only the control itself could reveal: at Note()
    // time we do not yet know whether this prediction will be COMPARED. In
    // UI::TabElement::UpdateContainers the first changing prediction is always
    // either superseded by the dirty walk or inside an Exclude region, so the
    // corrupted one was never compared and the self-test reported NOT CAUGHT on
    // a site whose comparison was in fact working. A control that picks an
    // uncompared subject measures nothing.
}

bool RtPredictRound::Peek(const void* addr, size_t size, uint64_t* out) const {
    if (!active_ || !out) return false;
    const RoundState* r = static_cast<const RoundState*>(state_);
    // Most recent wins: a body may write the same location twice, and the
    // second write is what a live re-read would see.
    for (size_t i = r->predCount; i-- > 0; ) {
        const Prediction& p = r->preds[i];
        if (p.addr == addr && p.size == size) {
            // A self-test-corrupted prediction is deliberately wrong. Hand the
            // body the UNCORRUPTED value: the control is meant to break the
            // COMPARISON, not to make the predictor compute different
            // downstream values — that would turn a one-bit probe into an
            // avalanche and stop it isolating the comparison.
            *out = p.corrupted ? (p.predicted ^ 1u) : p.predicted;
            return true;
        }
    }
    return false;
}

void RtPredictRound::NoteCall(const char* label, const void* target,
                              int64_t a0, int64_t a1, int64_t a2, int64_t a3) {
    if (!active_) return;
    RoundState* r = static_cast<RoundState*>(state_);
    site_->calls++;
    if (r->callCount >= kMaxCalls) { r->callOverflow = true; return; }
    CallRecord& c = r->calls[r->callCount++];
    c.label  = label ? label : "(unlabelled)";
    c.target = target;
    c.a[0] = a0; c.a[1] = a1; c.a[2] = a2; c.a[3] = a3;
}

void RtPredictRound::Watch(const void* base, size_t n, const char* label) {
    if (!active_ || !base || n == 0) return;
    RoundState* r = static_cast<RoundState*>(state_);
    if (r->watchCount >= kMaxWatches || r->snapUsed + n > kSnapshotBytes) {
        r->watchOverflow = true;
        return;
    }
    WatchRegion& w = r->watches[r->watchCount++];
    w.base       = static_cast<const uint8_t*>(base);
    w.size       = n;
    w.label      = label ? label : "(unlabelled)";
    w.snapOffset = r->snapUsed;
    memcpy(r->snapshot + r->snapUsed, base, n);
    r->snapUsed += n;
}

void RtPredictRound::Exclude(const void* base, size_t n, const char* why) {
    if (!active_ || !base || n == 0) return;
    RoundState* r = static_cast<RoundState*>(state_);
    if (r->excludeCount >= kMaxExcludes) { r->excludeOverflow = true; return; }
    ExcludeRegion& e = r->excludes[r->excludeCount++];
    e.base = static_cast<const uint8_t*>(base);
    e.size = n;
    e.why  = why ? why : "(no reason given)";
    site_->excludedBytes += n;
}

void RtPredictRound::Verify() {
    if (!active_ || verified_) return;
    verified_ = true;

    RoundState* r = static_cast<RoundState*>(state_);
    const char* name = site_->name ? site_->name : "(unnamed)";
    site_->verified++;

    // ---- (0) The negative control, applied where the compared set is known --
    //
    // Pick the first prediction that will ACTUALLY BE COMPARED and that
    // predicts a real change, and flip one bit of it. Verify() must then report
    // a mismatch for this site. Choosing the subject at Note() time instead --
    // the first draft -- picked predictions that were later superseded or
    // excluded, so the control could not fire even though the comparison was
    // sound, and the summary said the site "CANNOT FAIL". The control caught
    // its own miscalibration, which is the whole reason to have one.
    size_t selfTestIdx = (size_t)-1;
    if (RtPredict::SelfTestArmed()) {
        for (size_t i = 0; i < r->predCount; i++) {
            const Prediction& p = r->preds[i];
            if (p.predicted == p.pre) continue;                       // no-op
            if (InAnyExclude(r, static_cast<const uint8_t*>(p.addr))) continue;
            bool superseded = false;
            for (size_t j = i + 1; j < r->predCount; j++)
                if (RangesOverlap(p.addr, p.size, r->preds[j].addr, r->preds[j].size)) {
                    superseded = true; break;
                }
            if (superseded) continue;
            selfTestIdx = i;
            r->preds[i].corrupted = true;
            r->preds[i].predicted ^= 1u;
            r->selfTestApplied  = true;
            site_->selfTestSeen = true;
            break;
        }
        if (selfTestIdx == (size_t)-1) {
            // Nothing compared AND changing in this round: the control has no
            // subject. Say so rather than letting the round read as a pass.
            fprintf(stderr, "[RtPredict] SELF-TEST: %s had no compared, changing "
                            "prediction in this round -- control not applicable here\n",
                    name);
            fflush(stderr);
        }
    }

    // ---- (1) Every predicted location against what the DLL actually left ----
    uint64_t wrongHere = 0, changedHere = 0;
    for (size_t i = 0; i < r->predCount; i++) {
        const Prediction& p = r->preds[i];
        site_->predictions++;

        // (a) SUPERSEDED. A location this body writes more than once carries an
        // INTERMEDIATE value in every prediction but the last, and an
        // intermediate value is not a claim about the post-state. Comparing it
        // against final memory manufactures a mismatch out of a correct port.
        // UpdateContainers writes child+0x34 twice -- once directly, once as
        // the first step of the dirty walk -- and the first real run of this
        // oracle reported BOTH as divergences. Neither was one.
        //
        // OVERLAP, NOT EXACT MATCH — see RangesOverlap above. A later write of a
        // DIFFERENT WIDTH that covers this location supersedes it just as
        // completely as an identical one, and requiring an exact match is what
        // made UI::UserInterface::Update's fade teardown report three
        // mismatches that were not defects.
        bool superseded = false;
        for (size_t j = i + 1; j < r->predCount; j++) {
            if (RangesOverlap(p.addr, p.size, r->preds[j].addr, r->preds[j].size)) {
                superseded = true; break;
            }
        }
        if (superseded) { site_->superseded++; continue; }

        // (b) UNVERIFIABLE. A prediction landing in an Exclude region describes
        // memory whose final state is NOT ours to predict, because a callee we
        // suppressed also writes it. Recorded (so read-back stays consistent),
        // never compared, and counted so the hole is visible rather than silent.
        if (InAnyExclude(r, static_cast<const uint8_t*>(p.addr))) {
            site_->unverifiable++;
            continue;
        }

        // The effectiveness control. A prediction that predicts no change is
        // satisfied by the DLL doing nothing at all; counting it as evidence
        // is how a probe reads green without having exercised anything.
        // `p.corrupted` predictions were changes before we flipped a bit.
        if (p.predicted != p.pre || p.corrupted) { site_->changed++; changedHere++; }

        const uint64_t actual = ReadValue(p.addr, p.size);
        if (actual == p.predicted) continue;

        site_->wrong++;
        wrongHere++;
        if (site_->wrong <= kMaxVerbose) {
            fprintf(stderr,
                "[RtPredict] MISMATCH %s  %s @%p (%zu bytes)  "
                "pre=0x%llx ours=0x%llx theirs=0x%llx%s\n",
                name, p.label, p.addr, p.size,
                (unsigned long long)p.pre,
                (unsigned long long)p.predicted,
                (unsigned long long)actual,
                p.corrupted ? "   [SELF-TEST: this one was corrupted on purpose "
                              "— catching it is the control passing]" : "");
            fflush(stderr);
        }
        if (p.corrupted) site_->selfTestCaught = true;
    }

    // A self-test round whose corrupted prediction was NOT caught is the
    // finding this control exists for, and it must be loud at the moment it
    // happens rather than only in the summary.
    if (r->selfTestApplied && !site_->selfTestCaught) {
        fprintf(stderr,
            "[RtPredict] *** SELF-TEST NOT CAUGHT *** %s corrupted a prediction "
            "and still compared equal. This site cannot express a failure; treat "
            "every clean run of it as unproven.\n", name);
        fflush(stderr);
    }

    // ---- (2) Coverage: changed bytes in a watched region with no prediction --
    uint64_t unpredictedHere = 0;
    const char* firstLabel = nullptr;
    const uint8_t* firstAddr = nullptr;
    for (size_t i = 0; i < r->watchCount; i++) {
        const WatchRegion& w = r->watches[i];
        const uint8_t* now = w.base;
        const uint8_t* was = r->snapshot + w.snapOffset;
        for (size_t b = 0; b < w.size; b++) {
            if (now[b] == was[b]) continue;
            const uint8_t* p = w.base + b;
            if (InAnyPrediction(r, p)) continue;
            if (InAnyExclude(r, p))    continue;
            unpredictedHere++;
            site_->unpredicted++;
            if (!firstLabel) { firstLabel = w.label; firstAddr = p; }
        }
    }
    if (unpredictedHere && site_->unpredicted <= kMaxVerbose * 8) {
        fprintf(stderr,
            "[RtPredict] UNPREDICTED WRITE %s  %llu byte(s) changed inside watched "
            "state that our body did not predict and did not exclude; first at %s "
            "+%lld (@%p). Predictions are a LOWER BOUND — this is the half of the "
            "oracle that catches what we do not know about.\n",
            name, (unsigned long long)unpredictedHere,
            firstLabel ? firstLabel : "?",
            firstAddr ? (long long)(firstAddr - r->watches[0].base) : 0LL,
            (const void*)firstAddr);
        fflush(stderr);
    }

    // ---- (3) Structural warnings that would otherwise read as a pass --------
    if (r->predOverflow) {
        fprintf(stderr, "[RtPredict] %s: prediction pool overflowed (>%zu) — "
                        "THE TAIL OF THIS ROUND IS UNVERIFIED\n", name, kMaxPredictions);
        fflush(stderr);
    }
    if (r->callOverflow) {
        // The suppression still HAPPENED (RtSink::Call returns false regardless);
        // only the RECORD is lost. That is the dangerous direction: for a body
        // whose oracle IS the argument trace -- OnVideoRestart says so in as many
        // words -- a truncated trace reads exactly like a complete one.
        fprintf(stderr, "[RtPredict] %s: call pool overflowed (>%zu suppressed calls) — "
                        "THE CALL TRACE FOR THIS ROUND IS INCOMPLETE; a body whose "
                        "evidence is the trace is UNVERIFIED past that point\n",
                name, kMaxCalls);
        fflush(stderr);
    }
    if (r->excludeOverflow) {
        // Opposite direction, and loud rather than silent once flagged: a dropped
        // Exclude means the region is NOT declared out of model, so its changed
        // bytes surface as `unpredicted`. That is a FALSE ALARM, and without this
        // line someone would go looking for a port defect that is not there.
        fprintf(stderr, "[RtPredict] %s: exclude pool overflowed (>%zu regions) — "
                        "later Exclude() calls were DROPPED, so any `unpredicted` "
                        "below may be a HARNESS ARTEFACT, not a port defect\n",
                name, kMaxExcludes);
        fflush(stderr);
    }
    if (r->watchOverflow) {
        fprintf(stderr, "[RtPredict] %s: watch pool/snapshot overflowed — "
                        "COVERAGE IS INCOMPLETE FOR THIS ROUND\n", name);
        fflush(stderr);
    }
    (void)wrongHere; (void)changedHere;
}

// ---- RtSink ----------------------------------------------------------------
//
// Live mode is the default and must stay a plain store behind one predictable
// branch. Predict mode records and leaves memory alone.

#define RT_SINK_STORE(TYPE, METHOD)                                            \
    void RtSink::METHOD(void* addr, TYPE v, const char* label) {               \
        if (round_) round_->Note(addr, &v, sizeof(TYPE), label);               \
        else        *static_cast<TYPE*>(addr) = v;                             \
    }

RT_SINK_STORE(uint8_t,  U8)
RT_SINK_STORE(uint16_t, U16)
RT_SINK_STORE(uint32_t, U32)
RT_SINK_STORE(uint64_t, U64)

#undef RT_SINK_STORE

void RtSink::Ptr(void* addr, const void* v, const char* label) {
    if (round_) round_->Note(addr, &v, sizeof(void*), label);
    else        *static_cast<const void**>(addr) = v;
}

// ---- Read-back -------------------------------------------------------------
//
// Live mode: a plain load — the body's own stores already landed. Predict mode:
// the most recent prediction for this address, else memory. See rt_predict.h
// for the concrete case (UpdateContainers re-reads the flag word it just wrote)
// that makes this load-bearing rather than tidy.

#define RT_SINK_READ(TYPE, METHOD)                                             \
    TYPE RtSink::METHOD(const void* addr) const {                              \
        uint64_t v;                                                            \
        if (round_ && round_->Peek(addr, sizeof(TYPE), &v))                    \
            return static_cast<TYPE>(v);                                       \
        return *static_cast<const TYPE*>(addr);                                \
    }

RT_SINK_READ(uint8_t,  ReadU8)
RT_SINK_READ(uint16_t, ReadU16)
RT_SINK_READ(uint32_t, ReadU32)
RT_SINK_READ(uint64_t, ReadU64)

#undef RT_SINK_READ

void* RtSink::ReadPtr(const void* addr) const {
    uint64_t v;
    if (round_ && round_->Peek(addr, sizeof(void*), &v))
        return reinterpret_cast<void*>(static_cast<uintptr_t>(v));
    return *static_cast<void* const*>(addr);
}

bool RtSink::Call(const char* label, const void* target,
                  int64_t a0, int64_t a1, int64_t a2, int64_t a3) {
    if (!round_) return true;                       // live: make the call
    round_->NoteCall(label, target, a0, a1, a2, a3);
    return false;                                   // predict: suppress it
}

// ---- Summary ---------------------------------------------------------------

void RtPredict::Summary() {
    if (!RtPredict::Enabled()) return;

    fprintf(stderr, "[RtPredict] ==== %zu site(s) ====\n", g_siteCount);

    size_t unproven = 0, structurallyBlind = 0, dirty = 0;
    for (size_t i = 0; i < g_siteCount; i++) {
        const RtPredictSite* s = g_sites[i];
        if (!s || !s->name) continue;

        // `changed` is the effectiveness column and it is the one to read
        // first. predictions=N with changed=0 means every prediction this site
        // made was satisfied by the DLL doing nothing — a green board over an
        // unexercised path. It prints as UNPROVEN, never as a pass, for the
        // same reason rt_diff prints `worked=?` rather than `worked=0`.
        const bool proven = s->changed > 0;
        if (!proven) unproven++;
        if (s->wrong || s->unpredicted) dirty++;

        fprintf(stderr,
            "[RtPredict]   %-46s rounds=%-7llu predictions=%-8llu changed=%-8llu "
            "wrong=%llu unpredicted=%llu%s\n",
            s->name,
            (unsigned long long)s->rounds,
            (unsigned long long)s->predictions,
            (unsigned long long)s->changed,
            (unsigned long long)s->wrong,
            (unsigned long long)s->unpredicted,
            proven ? "" : "   [UNPROVEN: changed=0 — nothing this site predicted "
                          "required the DLL to do anything]");

        if (s->excludedBytes || s->calls) {
            fprintf(stderr,
                "[RtPredict]     %-44s excluded=%llu byte(s), %llu suppressed indirect "
                "call(s) — their effects are NOT verified here\n",
                "", (unsigned long long)s->excludedBytes,
                (unsigned long long)s->calls);
        }

        // Printed whenever non-zero, because a prediction that was NOT
        // compared must never be invisible: it is the difference between "we
        // checked it" and "we merely recorded it". Both counters exist because
        // the first real run of this oracle reported two mismatches that were
        // artefacts of the harness rather than defects in the port.
        if (s->superseded || s->unverifiable) {
            fprintf(stderr,
                "[RtPredict]     %-44s NOT COMPARED: %llu superseded (an intermediate "
                "write, not a post-state claim), %llu unverifiable (inside an Exclude "
                "region)\n",
                "", (unsigned long long)s->superseded,
                (unsigned long long)s->unverifiable);
        }

        // The negative control's verdict, per site.
        if (RtPredict::SelfTestArmed()) {
            if (!s->selfTestSeen) {
                fprintf(stderr, "[RtPredict]     %-44s SELF-TEST NEVER APPLIED — the site "
                                "made no changing prediction to corrupt\n", "");
                structurallyBlind++;
            } else if (!s->selfTestCaught) {
                fprintf(stderr, "[RtPredict]     %-44s *** SELF-TEST NOT CAUGHT — this site "
                                "CANNOT FAIL and proves nothing ***\n", "");
                structurallyBlind++;
            } else {
                fprintf(stderr, "[RtPredict]     %-44s self-test caught (the comparison can "
                                "go red)\n", "");
            }
        }
    }

    fprintf(stderr,
        "[RtPredict] ==== %zu site(s) with findings; %zu UNPROVEN (changed=0) ====\n",
        dirty, unproven);

    if (RtPredict::SelfTestArmed()) {
        fprintf(stderr,
            "[RtPredict] ==== SELF-TEST: %zu site(s) could NOT demonstrate a failure. "
            "A run is evidence only for the sites that could. ====\n",
            structurallyBlind);
    } else {
        fprintf(stderr,
            "[RtPredict] ==== negative control NOT run. Re-run with "
            "BANISHED_RT_PREDICT_SELFTEST=1: a clean board from a comparison that "
            "cannot go red is not a result. ====\n");
    }
    fflush(stderr);
}
