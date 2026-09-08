// ============================================================================
// runtime/src/collection/array_probe.cpp — Wave 9 lane Q's oracle
// ============================================================================
//
// THE PROBLEM THIS SOLVES
//
// `~Array` and `SetSize` cannot be replayed at a live call site. Running ours
// and then the shipped body frees the same buffer twice, and for `SetSize` the
// second run starts from the state the first one left, so it is not the same
// experiment. Lane D proposed a snapshot/restore; this does something simpler
// and stronger.
//
// SetSize is a pure function of (`_data` contents, `_size`, `_capacity`, `n`,
// `*fill`), and `~Array` is a pure function of the 16-byte header. So both can
// be run on a SYNTHETIC SUBJECT: build two private Arrays with identical
// headers and identical buffer contents, run OURS on one and the SHIPPED BODY
// on the other, and compare. Two subjects, two buffers, two frees — no double
// free, no shared state, and the comparison is against the DLL rather than
// against a restatement of our own rule.
//
// That is the shape reference_synthetic_subject_beats_a_fake.md recommends and
// the same idea as lane S's two independent sinks: do not fake the callee, give
// each implementation its own subject.
//
// THE SHIPPED BODY IS REACHED THROUGH ResolveRuntimeRaw, NOT ResolveAddr.
// `ResolveAddr(Module::Runtime, …)` consults the port registry first
// (reference_resolveaddr_consults_port_registry_first.md), and this lane
// registers all 27 of these addresses — so ResolveAddr would hand back OUR
// function and the probe would compare ours to ours and always agree. That is
// the exact failure mode this project has recorded twice; the Raw door exists
// for it.
//
// ---------------------------------------------------------------------------
// WHAT IT DOES NOT COMPARE, AND WHY
// ---------------------------------------------------------------------------
// reference_oracle_must_not_compare_unwritten_bytes.md: an oracle that memcmps
// bytes NEITHER side writes fails 100% of calls and reads like a catastrophic
// port defect. Four such gaps were found in Wave 6. This lane has three:
//
//   * `_data` itself. Two independent allocations, two different addresses.
//     Never compared; the buffer CONTENTS are compared instead.
//   * The extent past `_size`. `_capacity` never shrinks, so [size, capacity)
//     holds allocator fill in both runs — different fill after a realloc.
//   * The new tail of the two instantiations whose `else` arm is EMPTY
//     (0x180008b90 `Array<bool>` and 0x18002fea0 `Array<int32>`), and bytes
//     +0x2c..+0x2f of every new `Array<EffectSlot>` element (0x180051390).
//     Retail genuinely does not write those. Comparing them would fail every
//     case and would look exactly like a broken port.
//
// ---------------------------------------------------------------------------
// CONTROLS
// ---------------------------------------------------------------------------
// reference_probe_needs_an_effectiveness_control.md, learned the hard way in
// Wave 8.5 where 64 green comparisons included zero that did any work:
//
//   EFFECTIVENESS — every case reports whether it reallocated, ran the copy
//   loop, ran the fill loop and freed. The summary prints those totals per
//   address. A run whose `did-work` column is zero is a green nothing, and the
//   probe says so in as many words rather than printing "0 divergences".
//
//   NEGATIVE — `BANISHED_RT_ARRAY_PROBE=neg` additionally runs a DELIBERATELY
//   WRONG body (the growth rule with the `oldSize != 0` guard dropped, so an
//   append to an EMPTY array trebles when retail allocates exactly 1) through
//   the same comparator. The comparator must flag it. If the negative control
//   reports zero mismatches, the probe is broken and the whole green result is
//   worthless — which is the defect the Wave 8.5 negative control actually
//   found, in the probe rather than in the port.
//
// ---------------------------------------------------------------------------
// COVERAGE, STATED PLAINLY
// ---------------------------------------------------------------------------
//   BANISHED_RT_ARRAY_PROBE=1        the 13 trivially-copyable instantiations.
//   BANISHED_RT_ARRAY_PROBE=all      those plus the three non-trivial ones,
//                                    with ALL-ZERO element contents only.
//   BANISHED_RT_ARRAY_PROBE=neg      =all plus the negative control.
//
// The three non-trivial instantiations (0x180004e30, 0x180051390, 0x180061480)
// are opt-in because their element operations DEREFERENCE the element. Seeding
// them with a byte pattern would hand `0x180004220` / `0x180051650` / the
// inlined String assign a garbage pointer and fault. With zeroed elements those
// helpers take their null paths, so the probe exercises the CONTAINER algebra
// for those three but NOT their element semantics — in particular the
// `INC dword ptr [RAX-4]` String refcount bump inside 0x180061480's assignment
// is not reached. That gap is real and is listed in WAVE9_LANE_Q.md rather than
// hidden behind a green line.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "../../funcs/rt_port.h"
#include "array_internal.h"

// Byte-identical to standalone/resolve.h's declaration; the linker checks them.
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

namespace {

using rtcoll::ArrayHdr;
using rtcoll::ArraySpec;

// ---------------------------------------------------------------------------
// Mode
// ---------------------------------------------------------------------------
enum Mode { kOff, kTrivial, kAll, kNeg };

Mode ProbeMode() {
    const char* e = std::getenv("BANISHED_RT_ARRAY_PROBE");
    if (!e || !*e) return kOff;
    if (std::strcmp(e, "0") == 0 || std::strcmp(e, "false") == 0) return kOff;
    if (std::strcmp(e, "neg") == 0) return kNeg;
    if (std::strcmp(e, "all") == 0) return kAll;
    return kTrivial;
}

// ---------------------------------------------------------------------------
// Counters. The effectiveness control lives here: "how many of these
// comparisons changed anything", not "how many agreed".
// ---------------------------------------------------------------------------
struct Totals {
    int cases        = 0;   // comparisons attempted
    int mismatches   = 0;
    int noop         = 0;   // early return (n == _size) — changed nothing
    int realloced    = 0;
    int ranCopyLoop  = 0;
    int ranFillLoop  = 0;
    int freed        = 0;
};
Totals g_t;
int g_negFlagged = 0;       // mismatches the negative control produced
int g_negCases   = 0;

// ---------------------------------------------------------------------------
// A synthetic subject: a heap buffer from the DLL's allocator plus a header.
// The buffer comes from rtcoll::MemAlloc so that whichever body frees it hands
// the pointer back to the allocator that produced it.
// ---------------------------------------------------------------------------
struct Subject {
    ArrayHdr hdr;
    void Build(int size, int capacity, uint32_t stride, int align,
               uint8_t seedByte, bool zeroed) {
        hdr._data     = nullptr;
        hdr._size     = size;
        hdr._capacity = capacity;
        if (capacity > 0) {
            const size_t bytes = static_cast<size_t>(capacity) * stride;
            hdr._data = static_cast<uint8_t*>(
                rtcoll::MemAlloc(static_cast<uint64_t>(bytes), align));
            if (hdr._data) {
                // Seed the FULL capacity, not just [0,size): a non-reallocating
                // case must find the same bytes in both subjects everywhere it
                // legitimately reads them.
                if (zeroed) std::memset(hdr._data, 0, bytes);
                else for (size_t i = 0; i < bytes; ++i)
                         hdr._data[i] = static_cast<uint8_t>(seedByte + (i * 7u));
            }
        }
    }
};

// ---------------------------------------------------------------------------
// The comparison. Returns true on agreement; fills `why` with the FIRST failing
// term by name (reference_shadow_predicate_must_name_its_failing_term.md — a
// verdict that is a conjunction of N terms must name all N, or a failure prints
// two strings that agree and says nothing).
// ---------------------------------------------------------------------------
// `tailWritten` says whether retail wrote elements [oldSize, newSize): true when
// the call passed a `fill`, or when the spec has a default `init`. It is FALSE
// for 0x180008b90 and 0x18002fea0, whose `else` arm is empty — for those the new
// tail holds two DIFFERENT allocator fills and comparing it would fail every
// case.
bool Compare(const Subject& ours, const Subject& theirs, int newSize, int oldSize,
             const ArraySpec* spec, bool tailWritten, char* why, size_t nwhy) {
    if (ours.hdr._size != theirs.hdr._size) {
        std::snprintf(why, nwhy, "_size ours=%d theirs=%d",
                      ours.hdr._size, theirs.hdr._size);
        return false;
    }
    if (ours.hdr._capacity != theirs.hdr._capacity) {
        std::snprintf(why, nwhy, "_capacity ours=%d theirs=%d",
                      ours.hdr._capacity, theirs.hdr._capacity);
        return false;
    }
    if ((ours.hdr._data == nullptr) != (theirs.hdr._data == nullptr)) {
        std::snprintf(why, nwhy, "_data nullness ours=%p theirs=%p",
                      static_cast<void*>(ours.hdr._data),
                      static_cast<void*>(theirs.hdr._data));
        return false;
    }
    if (!ours.hdr._data || newSize <= 0) return true;

    const uint32_t stride = spec->stride;
    // How far the ELEMENT extent is legitimately comparable: every element up
    // to newSize when the tail is written, otherwise only what was carried over.
    const int cmpTo = tailWritten ? newSize
                                  : (oldSize < newSize ? oldSize : newSize);

    for (int i = 0; i < cmpTo; ++i) {
        const uint8_t* a = ours.hdr._data   + static_cast<int64_t>(i) * stride;
        const uint8_t* b = theirs.hdr._data + static_cast<int64_t>(i) * stride;
        for (uint32_t k = 0; k < stride; ++k) {
            // 0x180051390's default element never writes +0x2c..+0x2f. Skip
            // that window on elements the default arm produced.
            if (spec->unwrittenLo != spec->unwrittenHi && i >= oldSize &&
                k >= spec->unwrittenLo && k < spec->unwrittenHi)
                continue;
            if (a[k] != b[k]) {
                std::snprintf(why, nwhy,
                              "element[%d] byte +0x%x ours=0x%02x theirs=0x%02x",
                              i, static_cast<unsigned>(k), a[k], b[k]);
                return false;
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// The case matrix. Chosen to hit every branch of the growth rule at least once,
// including the two that are easy to get wrong:
//   * the FIRST append to an EMPTY array allocates capacity 1, NOT 3 (the `* 3`
//     is gated on oldSize != 0) — case 2 and the negative control's target;
//   * a bulk SetSize(n) allocates EXACTLY n, because n != oldSize + 1 — case 5.
// ---------------------------------------------------------------------------
struct Case { const char* name; int oldSize; int capacity; int newSize; };

const Case kCases[] = {
    { "noop(n==size)",        0,  0,  0 },
    { "append-from-empty",    0,  0,  1 },   // capacity must become 1
    { "append-nonempty",      1,  1,  2 },   // capacity must treble to 6
    { "grow-within-capacity", 2,  6,  5 },   // no realloc, tail filled
    { "bulk-grow-realloc",    5,  6, 20 },   // capacity exactly 20
    { "append-at-capacity",  20, 20, 21 },   // capacity 63
    { "shrink",              20, 20,  3 },   // no realloc, capacity kept
    { "shrink-to-zero",       5,  6,  0 },
    { "bulk-from-empty",      0,  0,  4 },   // capacity exactly 4
};
const int kCaseCount = static_cast<int>(sizeof(kCases) / sizeof(kCases[0]));

// ---------------------------------------------------------------------------
// One instantiation under test.
// ---------------------------------------------------------------------------
struct Target {
    uintptr_t        addr;
    const ArraySpec* spec;
    int              arity;     // 1, 2 or 3 — see array.cpp; getting this wrong
                                // reads a register the caller never set
    int              fixedN;    // arity 1 only (0x18007b960 folds n = 10000)
    bool             zeroSeed;  // the element ops dereference: seed zeros only
};

// The three arities, reached through the shipped code.
using Shipped1 = void (*)(void*);
using Shipped2 = void (*)(void*, int);
using Shipped3 = void (*)(void*, int, const void*);

void RunShipped(const Target& t, ArrayHdr* a, int n, const void* fill) {
    void* raw = reinterpret_cast<void*>(ResolveRuntimeRaw(t.addr));
    if (!raw) return;
    if (t.arity == 1)      reinterpret_cast<Shipped1>(raw)(a);
    else if (t.arity == 2) reinterpret_cast<Shipped2>(raw)(a, n);
    else                   reinterpret_cast<Shipped3>(raw)(a, n, fill);
}

// The deliberately wrong body for the negative control: retail's rule with the
// `oldSize != 0` guard dropped. It must diverge on "append-from-empty" and
// "bulk-from-empty" and agree everywhere else; a comparator that reports zero
// mismatches here cannot be trusted to report a real one.
void WrongSetSize(ArrayHdr* a, int newSize, const uint8_t* fill,
                  const ArraySpec* spec) {
    const int32_t oldSizeI = a->_size;
    const int64_t oldSize  = oldSizeI;
    if (newSize == oldSizeI) return;
    uint8_t* const old = a->_data;
    a->_size = newSize;
    if (newSize > a->_capacity) {
        a->_capacity = newSize;
        if (newSize == static_cast<int32_t>(oldSizeI + 1))   // <-- guard dropped
            a->_capacity = static_cast<int32_t>(newSize * 3);
        a->_data = static_cast<uint8_t*>(
            rtcoll::MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(a->_capacity)) *
                                 spec->stride,
                             spec->align));
    }
    const bool realloced = (old != a->_data);
    for (int64_t i = realloced ? 0 : oldSize; i < newSize; ++i) {
        uint8_t* dst = a->_data + i * spec->stride;
        if (!dst) continue;
        if (i < oldSize)     spec->assign(dst, old + i * spec->stride);
        else if (fill)       spec->assign(dst, fill);
        else if (spec->init) spec->init(dst);
    }
    if (realloced) {
        if (spec->destroy) spec->destroy(nullptr, old, oldSizeI);
        rtcoll::MemFree(old);
    } else if (spec->destroy && oldSizeI > newSize) {
        spec->destroy(nullptr, old + static_cast<int64_t>(newSize) * spec->stride,
                      oldSizeI - newSize);
    }
}

void RunTarget(const Target& t, Mode mode) {
    const ArraySpec* spec = t.spec;
    const uint32_t stride = spec->stride;

    // The fill argument for the three-argument form. A distinguishable pattern
    // for the byte-copy specs, zeros for the ones whose assignment dereferences.
    uint8_t fill[0x40];
    if (t.zeroSeed) std::memset(fill, 0, sizeof(fill));
    else for (size_t i = 0; i < sizeof(fill); ++i) fill[i] = static_cast<uint8_t>(0xA0 + i);

    int cases = 0, work = 0, mism = 0;

    for (int c = 0; c < kCaseCount; ++c) {
        Case k = kCases[c];
        const int n = (t.arity == 1) ? t.fixedN : k.newSize;
        // arity 1 folds n; the only case that means anything is "does it do the
        // right thing from this starting state", so keep the start state and
        // replace the target size.
        if (t.arity == 1 && k.oldSize == n) continue;

        Subject ours, theirs;
        ours  .Build(k.oldSize, k.capacity, stride, spec->align, 0x11, t.zeroSeed);
        theirs.Build(k.oldSize, k.capacity, stride, spec->align, 0x11, t.zeroSeed);
        if (k.capacity > 0 && (!ours.hdr._data || !theirs.hdr._data)) continue;

        // Effectiveness bookkeeping, computed from the PRE state so it does not
        // depend on either implementation being right.
        const bool isNoop     = (n == k.oldSize);
        const bool willAlloc  = !isNoop && (n > k.capacity);
        const bool willCopy   = willAlloc && k.oldSize > 0;
        const bool willFill   = !isNoop && n > k.oldSize;
        const bool willFree   = willAlloc && k.capacity > 0;

        rtcoll::Site sink = { { "probe", 0, 0, false, false }, t.addr };
        const uint8_t* fillArg = (t.arity == 3) ? fill : nullptr;
        // Retail writes the new tail iff a fill was passed or the spec has a
        // default `init`; see Compare().
        const bool tailWritten = (fillArg != nullptr) || (spec->init != nullptr);
        rtcoll::SetSizeCore(&ours.hdr, n, fillArg, spec, &sink);
        RunShipped(t, &theirs.hdr, n, fill);

        char why[192] = { 0 };
        const bool ok = Compare(ours, theirs, n, k.oldSize, spec, tailWritten,
                                why, sizeof(why));

        ++cases;
        ++g_t.cases;
        if (isNoop)    ++g_t.noop;
        if (willAlloc) { ++g_t.realloced; }
        if (willCopy)  ++g_t.ranCopyLoop;
        if (willFill)  ++g_t.ranFillLoop;
        if (willFree)  ++g_t.freed;
        if (!isNoop)   ++work;
        if (!ok) {
            ++mism; ++g_t.mismatches;
            std::fprintf(stderr,
                "[RtArrayProbe] MISMATCH 0x%llx %-28s case=%-22s "
                "(size %d cap %d -> %d): %s\n",
                (unsigned long long)t.addr, spec->name, k.name,
                k.oldSize, k.capacity, n, why);
        }

        rtcoll::MemFree(ours.hdr._data);
        rtcoll::MemFree(theirs.hdr._data);

        // ---- negative control -------------------------------------------
        if (mode == kNeg) {
            Subject wrong, ref;
            wrong.Build(k.oldSize, k.capacity, stride, spec->align, 0x11, t.zeroSeed);
            ref  .Build(k.oldSize, k.capacity, stride, spec->align, 0x11, t.zeroSeed);
            if (k.capacity == 0 || (wrong.hdr._data && ref.hdr._data)) {
                WrongSetSize(&wrong.hdr, n, fillArg, spec);
                RunShipped(t, &ref.hdr, n, fill);
                char w2[192] = { 0 };
                ++g_negCases;
                if (!Compare(wrong, ref, n, k.oldSize, spec, tailWritten,
                             w2, sizeof(w2)))
                    ++g_negFlagged;
                rtcoll::MemFree(wrong.hdr._data);
                rtcoll::MemFree(ref.hdr._data);
            }
        }
    }

    std::fprintf(stderr,
        "[RtArrayProbe] 0x%llx %-28s arity=%d stride=0x%-3x align=%d  "
        "cases=%d did-work=%d mismatches=%d\n",
        (unsigned long long)t.addr, spec->name, t.arity, spec->stride,
        spec->align, cases, work, mism);
}

// ---------------------------------------------------------------------------
// J1 — the destructor. One body, eleven addresses, so the interesting axis is
// the STATE, not the address: a populated array, an empty-but-allocated one,
// and a null one (which must NOT reach Free).
// ---------------------------------------------------------------------------
void RunDtor(uintptr_t addr) {
    struct DCase { const char* name; int size; int capacity; };
    const DCase cases[] = {
        { "populated",       4, 8 },
        { "empty-allocated", 0, 4 },
        { "null-buffer",     0, 0 },
    };
    int n = 0, work = 0, mism = 0;
    for (int c = 0; c < 3; ++c) {
        Subject ours, theirs;
        ours  .Build(cases[c].size, cases[c].capacity, 8, 8, 0x33, false);
        theirs.Build(cases[c].size, cases[c].capacity, 8, 8, 0x33, false);

        rtcoll::ArrayDtorImpl(&ours.hdr);

        void* raw = reinterpret_cast<void*>(ResolveRuntimeRaw(addr));
        if (raw) reinterpret_cast<void* (*)(void*)>(raw)(&theirs.hdr);

        const bool ok = ours.hdr._data == theirs.hdr._data &&
                        ours.hdr._size == theirs.hdr._size &&
                        ours.hdr._capacity == theirs.hdr._capacity;
        ++n; ++g_t.cases;
        if (cases[c].capacity > 0) { ++work; ++g_t.freed; }
        if (!ok) {
            ++mism; ++g_t.mismatches;
            std::fprintf(stderr,
                "[RtArrayProbe] MISMATCH 0x%llx ~Array case=%s "
                "ours{data=%p size=%d cap=%d} theirs{data=%p size=%d cap=%d}\n",
                (unsigned long long)addr, cases[c].name,
                static_cast<void*>(ours.hdr._data), ours.hdr._size, ours.hdr._capacity,
                static_cast<void*>(theirs.hdr._data), theirs.hdr._size, theirs.hdr._capacity);
        }
    }
    std::fprintf(stderr,
        "[RtArrayProbe] 0x%llx ~Array                       "
        "cases=%d did-work=%d mismatches=%d\n",
        (unsigned long long)addr, n, work, mism);
}

const uintptr_t kJ1[] = {
    0x180004890, 0x18001bf50, 0x18001c750, 0x18001c9e0, 0x180033960, 0x180033a40,
    0x180051d80, 0x18005fc20, 0x1800737f0, 0x1800739e0, 0x180073bb0,
};

const Target kJ3Trivial[] = {
    { 0x180008b90, &rtcoll::kSpec_Bool,      2,     0, false },
    { 0x180008d60, &rtcoll::kSpec_PtrZero,   2,     0, false },
    { 0x180008ff0, &rtcoll::kSpec_U64Zero,   2,     0, false },
    { 0x18002fea0, &rtcoll::kSpec_I32,       2,     0, false },
    { 0x180059140, &rtcoll::kSpec_Transform, 2,     0, false },
    { 0x1800764c0, &rtcoll::kSpec_PtrZero,   2,     0, false },
    { 0x18007b960, &rtcoll::kSpec_I32Neg1,   1, 10000, false },
    { 0x18001f990, &rtcoll::kSpec_Fill8,     3,     0, false },
    { 0x180023830, &rtcoll::kSpec_Fill4,     3,     0, false },
    { 0x18004f790, &rtcoll::kSpec_Fill16,    3,     0, false },
    { 0x18004f8e0, &rtcoll::kSpec_Fill16,    3,     0, false },
    { 0x180076900, &rtcoll::kSpec_Fill8Z,    3,     0, false },
    { 0x18007a0d0, &rtcoll::kSpec_Fill8Z,    3,     0, false },
};

// Zero-seeded only. See the COVERAGE note in the banner.
const Target kJ3NonTrivial[] = {
    { 0x180004e30, &rtcoll::kSpec_Sprite38,   3, 0, true },
    { 0x180051390, &rtcoll::kSpec_EffectSlot, 2, 0, true },
    { 0x180061480, &rtcoll::kSpec_StringExt,  3, 0, true },
};

}  // namespace

bool RtCollectionProbeArmed() { return ProbeMode() != kOff; }

void RtCollectionProbeRun() {
    const Mode mode = ProbeMode();
    if (mode == kOff) return;

    std::fprintf(stderr,
        "[RtArrayProbe] ==== Collection::Array synthetic-subject replay, "
        "mode=%s ====\n",
        mode == kNeg ? "neg" : (mode == kAll ? "all" : "trivial"));

    for (size_t i = 0; i < sizeof(kJ1) / sizeof(kJ1[0]); ++i) RunDtor(kJ1[i]);
    for (size_t i = 0; i < sizeof(kJ3Trivial) / sizeof(kJ3Trivial[0]); ++i)
        RunTarget(kJ3Trivial[i], mode);
    if (mode == kAll || mode == kNeg)
        for (size_t i = 0; i < sizeof(kJ3NonTrivial) / sizeof(kJ3NonTrivial[0]); ++i)
            RunTarget(kJ3NonTrivial[i], mode);

    // ---- the two controls, printed as loudly as the result ------------------
    const int did = g_t.cases - g_t.noop;
    std::fprintf(stderr,
        "[RtArrayProbe] ==== %d comparisons, %d mismatches ====\n",
        g_t.cases, g_t.mismatches);
    std::fprintf(stderr,
        "[RtArrayProbe] EFFECTIVENESS: %d of %d comparisons changed something "
        "(realloc %d, copy-loop %d, fill-loop %d, free %d; %d were the "
        "n==size early return)\n",
        did, g_t.cases, g_t.realloced, g_t.ranCopyLoop, g_t.ranFillLoop,
        g_t.freed, g_t.noop);
    if (did == 0)
        std::fprintf(stderr,
            "[RtArrayProbe] *** EFFECTIVENESS CONTROL FAILED: every comparison "
            "was a no-op. This run proves NOTHING. ***\n");

    if (mode == kNeg) {
        std::fprintf(stderr,
            "[RtArrayProbe] NEGATIVE CONTROL: the wrong body was flagged in %d "
            "of %d cases\n", g_negFlagged, g_negCases);
        if (g_negFlagged == 0)
            std::fprintf(stderr,
                "[RtArrayProbe] *** NEGATIVE CONTROL FAILED: a body with the "
                "growth rule broken compared EQUAL. The comparator is broken, "
                "so the green result above is worthless. ***\n");
    }
}
