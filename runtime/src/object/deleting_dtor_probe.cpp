// ============================================================================
// runtime/src/object/deleting_dtor_probe.cpp — Wave 9 lane K's oracle (body half)
// ============================================================================
//
// WHY A DESTRUCTOR IS THE HARDEST THING TO ORACLE, AND WHAT THIS DOES INSTEAD
//
// A deleting destructor cannot be replayed at a live call site. Running ours
// and then the shipped body destroys the same object twice and frees the same
// block twice; and run on the POST-state the second run takes the already-torn-
// down path, compares nothing, and REPORTS GREEN. Lane B2 hit precisely that
// with ~DisplayMode — it is the `Scene::Pause` early-out shape, and a
// destructor probe that reports 37 green no-ops is worse than no probe at all.
//
// So: a SYNTHETIC SUBJECT (reference_synthetic_subject_beats_a_fake.md, and the
// same design lane Q used for ~Array). Two private, identical objects are built
// in FILE-SCOPE storage; OURS runs on one and the SHIPPED BODY on the other;
// the bytes are compared. Two subjects, two destructions, no shared state, and
// the comparison is against the DLL rather than against a restatement of our
// own rule.
//
// THE SHIPPED BODY IS REACHED THROUGH ResolveRuntimeRaw, NEVER ResolveAddr.
// ResolveAddr consults the port registry first
// (reference_resolveaddr_consults_port_registry_first.md) and this lane
// registers all 37 of these addresses, so ResolveAddr would hand back OUR
// function and the probe would compare ours to ours and always agree.
//
// ---------------------------------------------------------------------------
// WHICH 13 OF THE 37, AND WHY NOT ALL 37
// ---------------------------------------------------------------------------
// A subject can only be fabricated if `~Class` is safe to run on fabricated
// bytes. For 13 of the 37 it is: their destructor is a pure vtable restamp
// (`LEA RAX,[vt]; MOV [RCX],RAX; RET`) — Object::Base and the twelve
// Video::*Data companions. The other 24 destroy members, release System::String
// references, or — for Object::Internal — swap-remove a handle-table entry.
// Running one of those on a synthetic object would fabricate a crash, not a
// measurement.
//
// That is NOT a 13-of-37 coverage hole in the way it first reads, because all
// 37 share ONE body (verified per address in deleting_dtor.cpp's banner). The
// 13 subjects verify the BODY. What they cannot verify is the other 24 ROWS —
// the destructor address and element size of each — and that is what
// tools/rt_deleting_dtor_verify.py exists for, re-deriving all 37 rows from the
// DLL's machine code. The two halves are stated as one design in
// docs/runtime_port/WAVE9_LANE_K.md; neither alone verifies this lane.
//
// ---------------------------------------------------------------------------
// WHAT IS COMPARED
// ---------------------------------------------------------------------------
// The whole subject block: the eight-byte array cookie plus three elements'
// worth of storage, pre-filled with a fixed sentinel. That includes the extent
// PAST the destroyed elements, and doing so is deliberate and is NOT the
// mistake reference_oracle_must_not_compare_unwritten_bytes.md warns about:
// there, the unwritten bytes held allocator fill that differed between the two
// runs. Here the probe OWNS the storage and puts the same sentinel in both, so
// "neither side wrote here" is a comparison that passes for a reason, and an
// over-long walk is caught by it. Also compared: the returned pointer, as an
// offset from `this` (0 on the scalar path, -8 on the array path).
//
// NOT compared: whether `operator delete` was called, and with what. The
// comparator cannot see a free, and the two cases that let the body free run
// on separately allocated blocks so a wrong pointer would corrupt the heap
// rather than show up here. That half is covered by pairing a run with
// BANISHED_REDZONE=watch, exactly as lane Q had to say about ~Array's free.
//
// ---------------------------------------------------------------------------
// THREE CONTROLS
// ---------------------------------------------------------------------------
// EFFECTIVENESS. Every case reports how many elements were actually destroyed —
// counted from the bytes, not from either implementation's promise. The summary
// prints the total, and if it is zero it says the run proves nothing in four
// lines of asterisks. Wave 8.5 had 64 green comparisons of which zero did any
// work (reference_probe_needs_an_effectiveness_control.md), and a destructor
// probe is the single easiest place in this codebase to reproduce that.
//
// NEGATIVE. `=neg` additionally runs a deliberately wrong envelope through the
// same comparator: it destroys `count - 1` elements and returns `this` instead
// of `this - 8` on the array path — the two defects the assumed `??_G` shape
// would have introduced. It MUST be flagged on all three array cases of every
// subject. If it is not, the comparator is broken and the green result above it
// is worthless. Wave 8.5's negative control found a defect in the PROBE rather
// than the port; this one is aimed at the same place.
//
// ISOLATION. `=iso` re-runs every case with the SHIPPED `~Class` forced into
// our body. 12 of the 13 subjects have a destructor that is already OUR port
// (lane B2's), so a red comparison could belong to that lane rather than this
// one. If a case is red normally and green under `iso`, the defect is in the
// destructor port, not in this envelope. This is a discriminator, not a
// control, and it is here so a lane-boundary bisection costs one run instead of
// an escalation.
//
// ---------------------------------------------------------------------------
// WHEN IT RUNS
// ---------------------------------------------------------------------------
// The byte-comparing cases run from RtObjectDeletingDtorPortInit(). That is
// unusual — lane Q's probe self-schedules on the first call into a registered
// entry point, because the Runtime allocator is CONSTRUCTED BUT NOT STARTED at
// init. This probe may run there because it ALLOCATES NOTHING: the subjects are
// file-scope arrays and the 13 destructors involved write one pointer each.
// And it must, because nothing in our tree calls a deleting destructor before
// cutover, so "the first call into an entry point" never arrives.
//
// The two cases that let the body FREE do need the Runtime heap. They are
// opt-in (`=all`) and self-schedule on the first entry-point call, which means
// that today they do not run. Said here rather than left to be inferred from a
// counter that stays at zero.
//
// *** A RtDiffSite MUST HAVE STATIC STORAGE DURATION *** and this probe
// registers NONE at all — it has its own counters, its comparison is strictly
// stronger than the live check, and its traffic does not belong in a registry
// that reports on the live run. Lane Q's first draft passed stack-local Sites
// and the summary reported 144 of them as "a stray write hit the harness's own
// statics"; the account is in runtime/src/collection/array.cpp.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "deleting_dtor_internal.h"

uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

namespace rtdtor {

bool   DtorIsPorted(int i);
DtorFn RawDtor(int i);
DtorFn NormalDtor(int i);
void* DeletingDtorCoreWith(void* self, unsigned flags, int i, DtorFn dtor);

namespace {

// ---------------------------------------------------------------------------
// Subject storage. Static, 16-aligned, sized for the largest probe subject:
// eight bytes of array cookie plus three elements. The 13 subjects are 8 or
// 0x10 bytes each, so 8 + 3*0x10 = 56; 64 leaves a margin that is itself part
// of the comparison (nothing may write past the three elements).
// ---------------------------------------------------------------------------
constexpr int  kMaxElem   = 0x10;
constexpr int  kElems     = 3;
constexpr int  kBlockSize = 8 + kElems * kMaxElem;
constexpr uint8_t kSentinel = 0x5A;

alignas(16) uint8_t g_blockA[kBlockSize];
alignas(16) uint8_t g_blockB[kBlockSize];

struct Case {
    const char* name;
    unsigned    flags;
    int         count;      // array cookie; ignored when flags has no bit 1
    bool        allocating; // the body will free -- needs the Runtime heap
};

// The pre-state is the same for both sides by construction, so an "effective"
// case is one where the destructor is expected to write at least one element.
// `noop-array-0` is expected to write NOTHING and is counted as ineffective on
// purpose: it pins the `DEC EDI; JS out` edge of the iterator, and counting it
// as work would inflate the effectiveness number that exists to catch exactly
// that kind of inflation.
constexpr Case kCases[] = {
    { "scalar-destroy",   0u, 0, false },
    { "array-destroy-1",  2u, 1, false },
    { "array-destroy-3",  2u, 3, false },
    { "noop-array-0",     2u, 0, false },
    { "scalar-delete",    1u, 0, true  },
    { "array-delete-3",   3u, 3, true  },
};
constexpr int kCaseCount = static_cast<int>(sizeof(kCases) / sizeof(kCases[0]));

// ---- the deliberately wrong envelope (the negative control) ----------------
//
// Two defects, and they are not invented: they are exactly what porting the
// SCALAR deleting destructor the brief assumed would have produced on an
// array delete -- one element too few destroyed, and `this` returned where the
// block pointer belongs.
void* WrongEnvelope(void* self, unsigned flags, int i) {
    uint8_t* p = static_cast<uint8_t*>(self);
    const DelDtorSpec& s = Specs()[i];
    if (flags & 2u) {
        int n = *reinterpret_cast<const int32_t*>(p - 8) - 1;   // DEFECT 1
        EhVectorDtor(p, s.elemSize, n, RawDtor(i));
        return p;                                              // DEFECT 2
    }
    RawDtor(i)(p);
    return p;
}

// ---------------------------------------------------------------------------
int g_compared = 0, g_mismatch = 0, g_effective = 0, g_elemsDestroyed = 0;
int g_negRun = 0, g_negFlagged = 0;
int g_isoRun = 0, g_isoDisagreed = 0;
int g_skippedAllocating = 0;

void Fill(uint8_t* block, int count) {
    std::memset(block, kSentinel, kBlockSize);
    *reinterpret_cast<int32_t*>(block) = count;
    *reinterpret_cast<int32_t*>(block + 4) = 0;   // the cookie's high half
}

// How many of the three element slots stopped holding the sentinel. Counted
// from the BYTES, so it does not depend on either implementation being right.
int ElementsWritten(const uint8_t* block, uint32_t elemSize) {
    int n = 0;
    for (int e = 0; e < kElems; ++e) {
        const uint8_t* el = block + 8 + e * elemSize;
        for (uint32_t b = 0; b < elemSize; ++b)
            if (el[b] != kSentinel) { ++n; break; }
    }
    return n;
}

int FirstDiff(const uint8_t* a, const uint8_t* b, int n) {
    for (int i = 0; i < n; ++i)
        if (a[i] != b[i]) return i;
    return -1;
}

// One (subject, case) comparison. Returns true when the two agree.
bool RunOne(int i, const Case& c, bool iso, bool wrong) {
    const DelDtorSpec& s = Specs()[i];
    const int span = 8 + kElems * static_cast<int>(s.elemSize);

    using Fn = void* (*)(void*, unsigned);
    Fn theirs = reinterpret_cast<Fn>(ResolveRuntimeRaw(s.addr));
    if (!theirs) {
        std::fprintf(stderr, "[RtDelDtorProbe] 0x%09llx: ResolveRuntimeRaw "
                     "returned null -- SKIPPED\n", (unsigned long long)s.addr);
        return true;
    }

    Fill(g_blockA, c.count);
    Fill(g_blockB, c.count);

    void* oursRet;
    if (wrong)     oursRet = WrongEnvelope(g_blockA + 8, c.flags, i);
    else if (iso)  oursRet = DeletingDtorCoreWith(g_blockA + 8, c.flags, i, RawDtor(i));
    else           oursRet = DeletingDtorCoreWith(g_blockA + 8, c.flags, i,
                                                 NormalDtor(i));
    void* theirsRet = theirs(g_blockB + 8, c.flags);

    const long long oursOff = reinterpret_cast<uint8_t*>(oursRet) - (g_blockA + 8);
    const long long theirsOff = reinterpret_cast<uint8_t*>(theirsRet) - (g_blockB + 8);

    const int diff = FirstDiff(g_blockA, g_blockB, span);
    const bool equal = (diff < 0) && (oursOff == theirsOff);

    const int wrote = ElementsWritten(g_blockB, s.elemSize);
    if (!wrong && !iso) {
        ++g_compared;
        g_elemsDestroyed += wrote;
        if (wrote > 0) ++g_effective;
        if (!equal) {
            ++g_mismatch;
            std::fprintf(stderr,
                "[RtDelDtorProbe] *** MISMATCH  %s / %s: %s%s\n"
                "                 ours ret %+lld, theirs ret %+lld; "
                "~Class 0x%09llx is %s\n",
                s.name, c.name,
                diff >= 0 ? "first differing byte at +0x" : "return value only",
                diff >= 0 ? "" : "",
                oursOff, theirsOff,
                (unsigned long long)s.dtorVa,
                DtorIsPorted(i) ? "OUR PORT (try =iso -- the defect may be that "
                                  "port, not this envelope)" : "the DLL's");
            if (diff >= 0)
                std::fprintf(stderr,
                    "                 offset +0x%x: ours 0x%02x, theirs 0x%02x\n",
                    diff, g_blockA[diff], g_blockB[diff]);
        }
    }
    return equal;
}

void Summarise(const char* phase, bool neg, bool iso) {
    std::fprintf(stderr,
        "[RtDelDtorProbe] %s: %d comparisons, %d MISMATCH, "
        "%d did-work (%d elements destroyed)\n",
        phase, g_compared, g_mismatch, g_effective, g_elemsDestroyed);
    if (g_skippedAllocating)
        std::fprintf(stderr,
            "[RtDelDtorProbe] %d allocating cases NOT RUN (they need the Runtime "
            "heap; set BANISHED_RT_DELDTOR_PROBE=all and note that they only fire "
            "on a call into a registered entry point, which nothing does before "
            "cutover)\n", g_skippedAllocating);

    if (g_effective == 0) {
        std::fprintf(stderr,
            "[RtDelDtorProbe] ****************************************************\n"
            "[RtDelDtorProbe] *** EFFECTIVENESS CONTROL FAILED: not one comparison\n"
            "[RtDelDtorProbe] *** destroyed anything. This run proves NOTHING. ***\n"
            "[RtDelDtorProbe] ****************************************************\n");
    }
    if (neg) {
        std::fprintf(stderr,
            "[RtDelDtorProbe] negative control: %d wrong-body cases, %d flagged\n",
            g_negRun, g_negFlagged);
        if (g_negRun > 0 && g_negFlagged < g_negRun) {
            std::fprintf(stderr,
                "[RtDelDtorProbe] ***********************************************\n"
                "[RtDelDtorProbe] *** NEGATIVE CONTROL FAILED: a body that\n"
                "[RtDelDtorProbe] *** destroys one element too few and returns the\n"
                "[RtDelDtorProbe] *** wrong pointer compared EQUAL in %d of %d\n"
                "[RtDelDtorProbe] *** cases. The comparator is broken, so the\n"
                "[RtDelDtorProbe] *** green result above is worthless.        ***\n"
                "[RtDelDtorProbe] ***********************************************\n",
                g_negRun - g_negFlagged, g_negRun);
        }
    }
    if (iso) {
        std::fprintf(stderr,
            "[RtDelDtorProbe] isolation: %d cases re-run with the SHIPPED ~Class, "
            "%d disagreed with the DLL\n", g_isoRun, g_isoDisagreed);
        std::fprintf(stderr,
            "[RtDelDtorProbe] (a case red normally and green here is a DESTRUCTOR "
            "port defect, not an envelope defect)\n");
    }
}

const char* Mode() { return std::getenv("BANISHED_RT_DELDTOR_PROBE"); }

void Run(bool allocatingPhase) {
    const char* m = Mode();
    if (!m || std::strcmp(m, "0") == 0) return;
    const bool neg = std::strcmp(m, "neg") == 0;
    const bool all = std::strcmp(m, "all") == 0;
    const bool iso = std::strcmp(m, "iso") == 0;

    int nSubjects = 0;
    const int* subjects = ProbeSubjects(&nSubjects);

    for (int k = 0; k < nSubjects; ++k) {
        const int i = subjects[k];
        for (int c = 0; c < kCaseCount; ++c) {
            if (kCases[c].allocating != allocatingPhase) {
                // Counted HERE, in the static phase, because the allocating
                // phase does not run at all today -- a skip counter that only
                // increments inside the thing being skipped stays at zero and
                // says nothing.
                if (!allocatingPhase && kCases[c].allocating && !all)
                    ++g_skippedAllocating;
                continue;
            }
            RunOne(i, kCases[c], false, false);
            if (neg && (kCases[c].flags & 2u)) {
                ++g_negRun;
                if (!RunOne(i, kCases[c], false, true)) ++g_negFlagged;
            }
            if (iso) {
                ++g_isoRun;
                if (!RunOne(i, kCases[c], true, false)) ++g_isoDisagreed;
            }
        }
    }
    Summarise(allocatingPhase ? "allocating" : "static", neg, iso);
}

}  // namespace

void ProbeRunStatic()     { Run(false); }
void ProbeRunAllocating() { Run(true); }

}  // namespace rtdtor
