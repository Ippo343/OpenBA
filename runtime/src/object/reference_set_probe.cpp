// ============================================================================
// runtime/src/object/reference_set_probe.cpp — Wave 11 follow-up 3, lane AVL
//   The lockstep synthetic-subject oracle for the seven
//   `Collection::Set<ReferenceEntry>` bodies in reference_set.cpp.
// ============================================================================
//
// WHY A PROBE AND NOT A LIVE DIFF
// -------------------------------
// Every body under test MUTATES ONE SHARED TREE. Running ours and then the
// shipped one at a live call site inserts the same id twice — and retail's
// second insert RETURNS EARLY (0x18002d218), making the comparison vacuously
// green. So the subject is not the live tree: two PRIVATE `Set`s with
// byte-identical starting state, one script applied to both, ours through the
// ported bodies and theirs through the shipped ones
// (reference_synthetic_subject_beats_a_fake.md).
//
// HOW EACH SIDE REACHES ITS OWN BODIES
// ------------------------------------
//   ours   : the `rtrefset::` functions, called directly by symbol.
//   theirs : `ResolveRuntimeRaw(0x18002cb90)`. RAW, never `ResolveAddr` —
//            `ResolveAddr` consults the PORT REGISTRY FIRST and would hand back
//            our own body, so the probe would compare ours with ours and always
//            agree (reference_resolveaddr_consults_port_registry_first.md).
//
// The shipped insert calls its own core, retrace, rotations, node allocator and
// pair constructor as NEAR calls inside the DLL, so nothing of ours can
// intercept them: driving the shipped 0x18002cb90 exercises the shipped
// 0x18002d180 / 0x18002d4b0 / 0x18002dac0 / 0x18002dbf0 / 0x18002e5d0 /
// 0x18002e570 as one unit. **All six of the other bodies are covered
// TRANSITIVELY, and that is stated rather than implied** — this probe drives
// exactly one entry point.
//
// WHY LOCKSTEP AND NOT A FINAL COMPARISON
// ---------------------------------------
// **AVL defects cancel.** A rotation wrong in one direction and wrong again in
// the other leaves the same key set and frequently the same final shape while
// every intermediate state differed. Both trees are fingerprinted and compared
// AFTER EVERY STEP, and the fingerprint carries SHAPE — which side of its
// parent each node hangs off, its balance and its height — not just the keys.
// Parent linkage is checked separately, because a fingerprint built by walking
// left/right cannot see a wrong `parent`, and a wrong parent is exactly what a
// mis-transcribed rotation produces and exactly what breaks the NEXT retrace.
//
// WHAT IS AND IS NOT COMPARED IN THE ELEMENT
// ------------------------------------------
// `PairCtor` writes the id as a DWORD and never touches `ReferenceEntry::_pad`
// at +0x04, so those four bytes are whatever the allocator left. Comparing the
// element with `memcmp` would therefore fail on essentially every node and read
// like a catastrophic port
// (reference_oracle_must_not_compare_unwritten_bytes.md). The fingerprint
// compares the `id` and the path's CHARACTERS. It does NOT compare the path
// POINTER: the two sides take references through their own copy constructors
// and, for the same source string, both end up aliasing the probe's own buffer
// — a pointer comparison would be testing the probe, not the port.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "reference_set.h"
#include "../../funcs/rt_port.h"
#include "../system/string.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

namespace RtSystem {
void RtSystem_Filename_AdjustDelimiters(Filename* self);
}

namespace {

using rtrefset::Node;
using rtrefset::Set;
using rtrefset::Elem;
using RtSystem::Filename;
using RtSystem::String;

using InsertFn = Node** (*)(Set*, Node**, const uint32_t*, const Filename*);

bool g_armed   = false;
bool g_running = false;

// ---------------------------------------------------------------------------
// THE POPULATION IS ADVERSARIAL ON PURPOSE.
//
// A straight 1..N run — which is the only population the LIVE caller ever
// produces, because SerializeReferenceTable numbers its rows 1..N in order —
// exercises exactly one rotation case and never the equal-key path. That is a
// population that cannot fail, and a green run on it says nothing
// (reference_uniform_population_is_not_a_verification.md).
//
// So the script below is built to force each rebalance case at least once:
//   * a strictly ASCENDING run   -> right-heavy, plain rotate LEFT
//   * a strictly DESCENDING run  -> left-heavy, plain rotate RIGHT
//   * zig-zags                   -> the two DOUBLE rotations (pre-rotate then
//                                   rotate), which are the cases a transcription
//                                   error most easily hides in
//   * REPEATS                    -> the equal-key early return, and with it the
//                                   release-on-every-path property
//   * id 0 and 0x7fffffff/0x80000000 -> the signed-compare boundary. The
//     comparator is `JL`/`JGE` on a field declared `uint32`, so a port that
//     used unsigned `<` orders 0x80000000 differently. Nothing in the live
//     caller can reach that, which is exactly why the probe must.
// ---------------------------------------------------------------------------
const uint32_t kIds[] = {
    50, 51, 52, 53, 54,                     // ascending  -> rotate LEFT
    40, 39, 38, 37, 36,                     // descending -> rotate RIGHT
    10, 30, 20,                             // zig-zag    -> LR double
    90, 70, 80,                             // zig-zag    -> RL double
    55, 45, 65, 35, 75, 25, 85, 15, 95,     // interleaved
    52, 40, 20, 95,                         // REPEATS    -> equal-key path
    0,                                      // the zero key
    0x7fffffffu, 0x80000000u, 0xffffffffu,  // the signed-compare boundary
    0x80000001u, 1u,
};
constexpr int kIdCount = static_cast<int>(sizeof(kIds) / sizeof(kIds[0]));

const char* const kPaths[] = {
    "Models/Building/House.rsc",  "Audio/Effects/Wind.rsc",
    "UI/Dialog/MainMenu.rsc",     "Materials/Terrain/Grass.rsc",
    "Models/Citizen/Adult.rsc",   "Audio/Music/Spring.rsc",
    "UI/Font/Body.rsc",           "Materials/Water/River.rsc",
};
constexpr int kPathCount = static_cast<int>(sizeof(kPaths) / sizeof(kPaths[0]));

// ---------------------------------------------------------------------------
// The fingerprint. Pre-order with explicit null markers, so SHAPE is encoded
// and not just the key multiset.
// ---------------------------------------------------------------------------
struct Fp {
    char buf[24 * 1024];
    int  len;
};

void Emit(Fp& f, const char* s) {
    while (*s && f.len < static_cast<int>(sizeof(f.buf)) - 1) f.buf[f.len++] = *s++;
    f.buf[f.len] = 0;
}

void Walk(Fp& f, const Node* n) {
    if (!n) { Emit(f, "."); return; }
    char line[256];
    const RtObject::ReferenceEntry* e = Elem(n);
    std::snprintf(line, sizeof line, "(%u,b%d,h%d,'%.80s'",
                  static_cast<unsigned>(e->id),
                  static_cast<int>(n->balance), static_cast<int>(n->height),
                  e->path.data ? e->path.data : "<null>");
    Emit(f, line);
    Walk(f, n->left);
    Walk(f, n->right);
    Emit(f, ")");
}

void Fingerprint(Fp& f, const Set* s) {
    f.len = 0; f.buf[0] = 0;
    char hdr[64];
    std::snprintf(hdr, sizeof hdr, "n=%d:", static_cast<int>(s->count));
    Emit(f, hdr);
    Walk(f, s->root);
}

// A fingerprint built by walking left/right cannot see a wrong `parent`.
bool ParentsOk(const Node* n, const Node* expected) {
    if (!n) return true;
    if (n->parent != expected) return false;
    return ParentsOk(n->left, n) && ParentsOk(n->right, n);
}

// ---------------------------------------------------------------------------
// NEGATIVE CONTROLS.
//
// Each is SCORED SEPARATELY and each reports its own catch rate, because a
// score on the UNION of several screens reads 3/3 and hides that one screen
// caught 1/3 (reference_union_control_hides_a_blind_screen.md). Each control
// corrupts a value the comparison provably LOOKS AT — a control that corrupts
// something nothing compares indicts a sound instrument
// (reference_control_must_pick_a_compared_subject.md) — and each is REVERSIBLE,
// because a control that leaves the subject broken kills every control after it
// (a double free once killed a whole run that way).
//
// Every one of them mutates ONE side only, which is what makes a catch mean
// "the comparison sees this field" rather than "the trees drifted".
// ---------------------------------------------------------------------------
int16_t g_savedBalance = 0;
int16_t g_savedHeight  = 0;
uint32_t g_savedId     = 0;
char*   g_savedPath    = nullptr;
Node*   g_savedParent  = nullptr;
Node*   g_parentVictim = nullptr;

bool BreakBalance(Set* s) {
    if (!s->root) return false;
    g_savedBalance = s->root->balance;
    s->root->balance = static_cast<int16_t>(s->root->balance ^ 1);
    return true;
}
void RestoreBalance(Set* s) { if (s->root) s->root->balance = g_savedBalance; }

bool BreakHeight(Set* s) {
    if (!s->root) return false;
    g_savedHeight = s->root->height;
    s->root->height = static_cast<int16_t>(s->root->height ^ 1);
    return true;
}
void RestoreHeight(Set* s) { if (s->root) s->root->height = g_savedHeight; }

bool BreakId(Set* s) {
    if (!s->root) return false;
    g_savedId = Elem(s->root)->id;
    Elem(s->root)->id ^= 1u;
    return true;
}
void RestoreId(Set* s) { if (s->root) Elem(s->root)->id = g_savedId; }

// The PATH control targets the value half of the element — the half
// `PairCtor`'s copy constructor produces and the half 0x18002ebf0 releases.
// It swaps the pointer for another live string rather than editing characters
// through a shared refcounted buffer, which both sides alias.
bool BreakPath(Set* s, char* other) {
    if (!s->root || !other) return false;
    g_savedPath = Elem(s->root)->path.data;
    Elem(s->root)->path.data = other;
    return true;
}
void RestorePath(Set* s) { if (s->root) Elem(s->root)->path.data = g_savedPath; }

bool BreakParent(Set* s) {
    if (!s->root || !s->root->left) return false;
    g_parentVictim = s->root->left;
    g_savedParent  = g_parentVictim->parent;
    g_parentVictim->parent = nullptr;
    return true;
}
void RestoreParent() { if (g_parentVictim) g_parentVictim->parent = g_savedParent; }

// The probe holds its own reference on each source string. Dropping it WITHOUT
// freeing at zero is deliberate: both trees still hold references, so this
// local is never the last owner.
void DropOurRef(Filename* f) {
    if (f->data) { int32_t* rc = reinterpret_cast<int32_t*>(f->data) - 1; --(*rc); }
}

}  // namespace

void RtObjectReferenceSetProbeArm(bool on) { g_armed = on; }
bool RtObjectReferenceSetProbeArmed()      { return g_armed; }

void RtObjectReferenceSetProbeMaybeRun() {
    if (!g_armed || g_running) return;
    g_armed   = false;        // cleared BEFORE anything runs: no re-entry
    g_running = true;
    RtObjectReferenceSetProbeRun();
    g_running = false;
}

void RtObjectReferenceSetProbeRun() {
    auto theirInsert = reinterpret_cast<InsertFn>(ResolveRuntimeRaw(0x18002cb90));
    if (!theirInsert) {
        printf("[RefSet] SKIP -- the shipped 0x18002cb90 is not reachable "
                    "(cutover, or Runtime-x64.dll is absent). NO COMPARISON WAS "
                    "MADE.\n");
        std::fflush(stdout);
        return;
    }

    const rtrefset::ProbeCounters before = rtrefset::g_fc;

    Set ours   = {nullptr, 0, 0};
    Set theirs = {nullptr, 0, 0};

    int steps = 0, mismatches = 0, parentFails = 0;
    static Fp fa, fb;                       // static: 48 KB is too much for a frame
    char firstBad[720]; firstBad[0] = 0;

    auto compare = [&](const char* what, int idx) -> bool {
        ++steps;
        Fingerprint(fa, &ours);
        Fingerprint(fb, &theirs);
        const bool pOurs   = ParentsOk(ours.root,   nullptr);
        const bool pTheirs = ParentsOk(theirs.root, nullptr);
        bool bad = false;
        if (!pOurs || !pTheirs) {
            ++parentFails; bad = true;
            if (!firstBad[0])
                std::snprintf(firstBad, sizeof firstBad,
                              "parent linkage broken after %s #%d (ours=%d theirs=%d)",
                              what, idx, pOurs ? 1 : 0, pTheirs ? 1 : 0);
        }
        if (std::strcmp(fa.buf, fb.buf) != 0) {
            ++mismatches; bad = true;
            if (!firstBad[0])
                std::snprintf(firstBad, sizeof firstBad,
                              "shape/payload differ after %s #%d\n"
                              "[RefSet]     ours   : %.260s\n"
                              "[RefSet]     theirs : %.260s",
                              what, idx, fa.buf, fb.buf);
        }
        return !bad;
    };

    // A silent comparison used only by the controls: it must NOT pollute the
    // real mismatch counters, or a control that fires would read as a port
    // defect.
    auto quietAgrees = [&]() -> bool {
        Fingerprint(fa, &ours);
        Fingerprint(fb, &theirs);
        return std::strcmp(fa.buf, fb.buf) == 0 &&
               ParentsOk(ours.root, nullptr) && ParentsOk(theirs.root, nullptr);
    };

    // Per-control scores, kept SEPARATE.
    int ctlBalanceRun = 0, ctlBalanceCaught = 0;
    int ctlHeightRun  = 0, ctlHeightCaught  = 0;
    int ctlIdRun      = 0, ctlIdCaught      = 0;
    int ctlPathRun    = 0, ctlPathCaught    = 0;
    int ctlParentRun  = 0, ctlParentCaught  = 0;

    // A spare live string for the PATH control to swap in.
    Filename spare; spare.data = nullptr;
    RtSystem::RtSystem_String_Set(reinterpret_cast<String*>(&spare),
                                  "Probe/Control/Other.rsc");

    // ---- the insert script -------------------------------------------------
    for (int i = 0; i < kIdCount; ++i) {
        Filename path; path.data = nullptr;
        RtSystem::RtSystem_String_Set(reinterpret_cast<String*>(&path),
                                      kPaths[i % kPathCount]);
        RtSystem::RtSystem_Filename_AdjustDelimiters(&path);   // every caller does

        const uint32_t id = kIds[i];
        Node* outA = nullptr;
        Node* outB = nullptr;
        rtrefset::Insert(&ours,   &outA, &id, &path);
        theirInsert     (&theirs, &outB, &id, &path);

        // The two `out` slots hold nodes from two different trees, so their
        // ADDRESSES cannot be compared. What CAN be compared is whether each
        // side reported a node at all and whether the node it reported carries
        // the id just inserted — which is the whole observable contract of the
        // return, and the thing the equal-key early return changes.
        if ((outA == nullptr) != (outB == nullptr)) {
            ++mismatches;
            if (!firstBad[0])
                std::snprintf(firstBad, sizeof firstBad,
                              "insert #%d (id=%u): one side returned a node and "
                              "the other did not", i, static_cast<unsigned>(id));
        } else if (outA && outB && (Elem(outA)->id != Elem(outB)->id)) {
            ++mismatches;
            if (!firstBad[0])
                std::snprintf(firstBad, sizeof firstBad,
                              "insert #%d (id=%u): out slots carry different ids "
                              "(%u vs %u)", i, static_cast<unsigned>(id),
                              static_cast<unsigned>(Elem(outA)->id),
                              static_cast<unsigned>(Elem(outB)->id));
        }

        const bool agreed = compare("insert", i);
        DropOurRef(&path);

        // ---- run every control at three points in the script, and only when
        // the trees currently AGREE. A control fired on an already-diverged
        // pair proves nothing.
        if (agreed && (i == 6 || i == 14 || i == kIdCount - 2)) {
            if (BreakBalance(&ours)) { ++ctlBalanceRun; if (!quietAgrees()) ++ctlBalanceCaught; RestoreBalance(&ours); }
            if (BreakHeight (&ours)) { ++ctlHeightRun;  if (!quietAgrees()) ++ctlHeightCaught;  RestoreHeight(&ours); }
            if (BreakId     (&ours)) { ++ctlIdRun;      if (!quietAgrees()) ++ctlIdCaught;      RestoreId(&ours); }
            if (BreakPath   (&ours, spare.data)) { ++ctlPathRun; if (!quietAgrees()) ++ctlPathCaught; RestorePath(&ours); }
            if (BreakParent (&ours)) { ++ctlParentRun;  if (!quietAgrees()) ++ctlParentCaught;  RestoreParent(); }
            if (!quietAgrees() && !firstBad[0])
                std::snprintf(firstBad, sizeof firstBad,
                              "A CONTROL DID NOT RESTORE at step %d -- every "
                              "result after this point is suspect", i);
        }
    }

    DropOurRef(&spare);

    const rtrefset::ProbeCounters& a = rtrefset::g_fc;
    auto d = [&](uint64_t now, uint64_t then) { return static_cast<unsigned long long>(now - then); };

    printf("[RefSet] LOCKSTEP PROBE: %d steps, %d shape/payload mismatches, "
                "%d parent-linkage failures\n", steps, mismatches, parentFails);
    printf("[RefSet]   effectiveness (branches taken INSIDE the ported "
                "bodies): rootInsert=%llu descLeft=%llu descRight=%llu "
                "equalHit=%llu cmpLt=%llu cmpEq=%llu cmpGt=%llu\n",
                d(a.rootInsert, before.rootInsert), d(a.descLeft, before.descLeft),
                d(a.descRight, before.descRight),   d(a.equalHit, before.equalHit),
                d(a.cmpLt, before.cmpLt), d(a.cmpEq, before.cmpEq), d(a.cmpGt, before.cmpGt));
    printf("[RefSet]   rebalance: rotL=%llu rotR=%llu preRotL=%llu "
                "preRotR=%llu retraceIters=%llu allocFail=%llu\n",
                d(a.rotL, before.rotL), d(a.rotR, before.rotR),
                d(a.preRotL, before.preRotL), d(a.preRotR, before.preRotR),
                d(a.retraceIters, before.retraceIters), d(a.allocFail, before.allocFail));
    printf("[RefSet]   NEGATIVE CONTROLS, scored separately: "
                "balance %d/%d  height %d/%d  id %d/%d  path %d/%d  parent %d/%d\n",
                ctlBalanceCaught, ctlBalanceRun, ctlHeightCaught, ctlHeightRun,
                ctlIdCaught, ctlIdRun, ctlPathCaught, ctlPathRun,
                ctlParentCaught, ctlParentRun);

    // A control that never RAN is not a control that passed, and a rebalance
    // case that never fired means the green result does not cover it. Both are
    // announced rather than left for a reader to notice.
    if (d(a.rotL, before.rotL) == 0 || d(a.rotR, before.rotR) == 0 ||
        d(a.preRotL, before.preRotL) == 0 || d(a.preRotR, before.preRotR) == 0)
        printf("[RefSet]   *** WARNING: at least one rebalance case never "
                    "fired. The green verdict does NOT cover it.\n");
    if (d(a.equalHit, before.equalHit) == 0)
        printf("[RefSet]   *** WARNING: the equal-key early return never "
                    "fired -- the release-on-every-path property is UNTESTED.\n");
    if (ctlBalanceRun == 0 || ctlHeightRun == 0 || ctlIdRun == 0 ||
        ctlPathRun == 0 || ctlParentRun == 0)
        printf("[RefSet]   *** WARNING: a control never RAN. A control that "
                    "did not run did not pass.\n");

    if (firstBad[0])
        printf("[RefSet]   FIRST DIVERGENCE: %s\n", firstBad);
    else if (mismatches == 0 && parentFails == 0)
        printf("[RefSet]   VERDICT: the seven ported bodies agree with the "
                    "shipped 0x18002cb90 family at every step.\n");
    std::fflush(stdout);
}
