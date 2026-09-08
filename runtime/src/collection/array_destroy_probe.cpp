// ============================================================================
// runtime/src/collection/array_destroy_probe.cpp — Wave 9 lane D2's oracle
// ============================================================================
//
// SYNTHETIC-SUBJECT REPLAY, the shape lane Q's array_probe.cpp established and
// reference_synthetic_subject_beats_a_fake.md recommends: build two private,
// byte-identical subjects, run OURS on one and the SHIPPED BODY on the other,
// compare. Two subjects means two buffers and two frees — a destructor cannot
// be replayed on one subject without double-freeing it.
//
// THE SHIPPED BODY IS REACHED THROUGH ResolveRuntimeRaw, NEVER ResolveAddr.
// ResolveAddr consults the port registry first
// (reference_resolveaddr_consults_port_registry_first.md) and this lane
// registers all eighteen addresses, so ResolveAddr would hand back our own
// function and every comparison would be ours-against-ours and always green.
//
// ---------------------------------------------------------------------------
// WHAT IT COVERS, AND — READ THIS BEFORE BELIEVING A GREEN RUN — WHAT IT DOES NOT
// ---------------------------------------------------------------------------
// COVERED, against the shipped body:
//   * all ten registered element-destroy helpers (Body A x5, Body B x5). This
//     is where 100% of the parameterization lives — every stride and every
//     offset — and it is the only place a transcription error can hide.
//   * 0x180003970, Array<T*>::operator=, fully replayed.
//
// NOT COVERED by replay, and the reason is structural, not laziness:
//   * the seven OUTER bodies (J2's six and J2b). An outer body's last act is to
//     FREE the element buffer, so after it returns there is nothing left to
//     compare: reading the buffer would be a use-after-free and the 16-byte
//     header is all zeroes on both sides by construction. They are covered by
//     COMPOSITION instead — each is exactly `helper(this, _data, _size)` then
//     `ArrayDtorImpl`, the helper is replayed here and ArrayDtorImpl is lane Q's
//     J1 body, already replayed by array_probe.cpp — plus the live post-state
//     check in array_destroy.cpp. That is weaker than a replay and is stated as
//     such rather than counted.
//   * 0x18005c550's recursive nested-pair destroy. Its two sub-helpers are
//     replayed; the nesting itself is not, because building a subject for it
//     means building an Array of Arrays of Strings and the failure mode of a
//     wrong inner count is a wild free, which this comparator cannot see and
//     BANISHED_REDZONE=watch can.
//
// ---------------------------------------------------------------------------
// WHAT IS COMPARED — and what is deliberately excluded
// ---------------------------------------------------------------------------
// reference_oracle_must_not_compare_unwritten_bytes.md: an oracle that memcmps
// bytes NEITHER side writes fails 100% of calls and reads like a catastrophic
// defect. Four such gaps were found in Wave 6 and lane Q had three more.
//
// BODY A — plain `memcmp` of the whole `count * stride` element buffer works,
// because every slot ends at zero on both sides and everything else must be
// UNTOUCHED. The buffer is seeded with a byte pattern precisely so that a wrong
// stride or a wrong offset writes its zeroes somewhere the pattern notices.
// That is the strongest check in this file and it is nearly free.
//
// BODY B — a raw memcmp CANNOT be used, and this is the subtle part. Retail's
// two arms leave the slot holding different things: NULL when the string
// survives, and `str - 8` — the address of the block it just freed — when it
// does not. Those addresses are per-side allocations and will never agree. So
// each slot is compared by CLASSIFICATION (null / this side's own block base /
// unchanged / something else) and every byte that is not a slot word is
// compared raw. The surviving blocks' REFCOUNTS are compared as numbers.
//
// That refcount comparison closes lane Q's largest stated gap. Lane Q probed
// its three non-trivial instantiations with ALL-ZERO elements only, so the
// String refcount path was never entered and its §"gaps" item 3 says so in as
// many words. Here the subjects carry real synthetic `str-8` blocks with real
// refcounts, both arms are driven, and the count afterwards is an assertion.
//
// 0x180003970 — `_size`, `_capacity`, and the `_size * 8` prefix of the buffer.
// NOT `_data` (two independent allocations) and NOT the extent past `_size`
// (allocator fill, never written by either side).
//
// ---------------------------------------------------------------------------
// CONTROLS
// ---------------------------------------------------------------------------
// EFFECTIVENESS (reference_probe_needs_an_effectiveness_control.md — Wave 8.5
// had 64 green comparisons of which ZERO did any work). Every case reports how
// many slots it actually freed, how many refcounts it actually decremented and
// how many of those reached zero. If the totals are zero the probe says the run
// proves nothing, in those words, instead of printing "0 divergences".
//
// NEGATIVE (`BANISHED_RT_ARRAY_DESTROY_PROBE=neg`) — three deliberately wrong
// bodies go through the SAME comparator and must all be flagged:
//   A: drop the `slot+8 = 0` store         (half the header clear)
//   B: SWAP the two arms                   (the asymmetry, backwards)
//   =: always treble the capacity          (the growth rule)
// If any of them compares EQUAL, the comparator is broken and the green result
// above it is worthless — which is what Wave 8.5's negative control found, in
// the PROBE and not in the port.
//
// A CONTROL THIS PROBE CANNOT PROVIDE, said plainly: the ORDER in which slots
// are freed. 0x1800059e0 releases +0x08 before +0x00, 0x18005bce0 walks
// 0x38/0x28/0x08 and J2b runs 0x1e0/0x1d0/0x1c0 backwards. The slots do not
// alias, so no comparator on the element buffer can see the order — only the
// allocator can. The order is reproduced from the disassembly and is checked,
// if at all, by BANISHED_REDZONE=watch.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "array_internal.h"
#include "array_destroy_internal.h"

// ResolveRuntimeRaw — the door that does NOT consult the port registry. Locally
// declared for the same reason array.cpp declares ResolveAddr: resolve.h drags
// in <windows.h>.
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

namespace rtcoll {
namespace {

// ---------------------------------------------------------------------------
// Reporting
// ---------------------------------------------------------------------------
int g_cases = 0, g_mismatch = 0, g_didWork = 0;
int g_freed = 0, g_decremented = 0, g_reachedZero = 0, g_survived = 0;
int g_negRun = 0, g_negCaught = 0;
// Wave 9 lane DESTROY: 0x1800298f0's shrink-tail arm, counted separately from
// g_cases because it is the arm with NO live population and a reader has to be
// able to see it went above zero.
int g_shrinkArm = 0, g_shrinkElems = 0;

void Fail(const char* what, const char* detail) {
    ++g_mismatch;
    std::fprintf(stderr, "[RtArrayDestroyProbe] MISMATCH %s : %s\n", what, detail);
}

// ---------------------------------------------------------------------------
// A synthetic System::String block.
//
// Layout, from the release loop itself: the stored pointer is `str`, the
// refcount is the int32 at `str - 4`, and the block handed to Free is `str - 8`.
// So one block is MemAlloc(8 + len) and the pointer we store is base + 8.
// ---------------------------------------------------------------------------
struct StrBlock { uint8_t* base; uint8_t* str; };

StrBlock MakeString(int refcount) {
    uint8_t* base = static_cast<uint8_t*>(MemAlloc(8 + 16, 8));
    std::memset(base, 0xA5, 8 + 16);
    *reinterpret_cast<int32_t*>(base + 4) = refcount;   // str - 4
    StrBlock b = { base, base + 8 };
    return b;
}

// ===========================================================================
// BODY A — one subject, one comparison
// ===========================================================================
//
// `nullEvery` seeds every Nth slot NULL so the `if (p)` arm is driven too; a
// probe that only ever hands non-null pointers never proves the null path is
// not a free of 0.
struct SubjectA {
    uint8_t* buf;
    size_t   bytes;
};

SubjectA BuildA(const DestroySpec* s, int count, int nullEvery, int* outLive) {
    SubjectA sub;
    sub.bytes = static_cast<size_t>(count) * s->stride;
    sub.buf = static_cast<uint8_t*>(MemAlloc(sub.bytes ? sub.bytes : 1, 8));
    // A pattern, not zeros: this is what notices a wrong stride or offset.
    for (size_t i = 0; i < sub.bytes; ++i) sub.buf[i] = static_cast<uint8_t>(0x5A + (i & 0x1f));

    int live = 0, slot = 0;
    for (int i = 0; i < count; ++i) {
        for (int k = 0; k < s->nOffs; ++k, ++slot) {
            uint8_t* p = sub.buf + static_cast<size_t>(i) * s->stride + s->offs[k];
            const bool wantNull = (nullEvery > 0) && (slot % nullEvery == 0);
            void* inner = wantNull ? nullptr : MemAlloc(0x40, 8);
            *reinterpret_cast<void**>(p) = inner;
            // The inner header's _size/_capacity, cleared as one qword by both
            // sides regardless of what is in them.
            *reinterpret_cast<int32_t*>(p + 8)  = 7;
            *reinterpret_cast<int32_t*>(p + 12) = 9;
            if (inner) ++live;
        }
    }
    *outLive = live;
    return sub;
}

void RunA(const char* label, const DestroySpec* s, uintptr_t shippedAddr,
          int count, int nullEvery, bool negative) {
    using Fn = void (*)(void*, void*, int);
    Fn shipped = reinterpret_cast<Fn>(ResolveRuntimeRaw(shippedAddr));
    if (!shipped) {
        std::fprintf(stderr, "[RtArrayDestroyProbe] SKIP %s: no shipped body at 0x%llx\n",
                     label, static_cast<unsigned long long>(shippedAddr));
        return;
    }

    int liveOurs = 0, liveTheirs = 0;
    SubjectA ours   = BuildA(s, count, nullEvery, &liveOurs);
    SubjectA theirs = BuildA(s, count, nullEvery, &liveTheirs);

    ++g_cases;
    if (liveOurs > 0) { ++g_didWork; g_freed += liveOurs; }

    if (negative) {
        ++g_negRun;
        // WRONG ON PURPOSE: the header clear stops after the first qword.
        for (int i = 0; i < count; ++i)
            for (int k = 0; k < s->nOffs; ++k) {
                uint8_t* p = ours.buf + static_cast<size_t>(i) * s->stride + s->offs[k];
                void* q = *reinterpret_cast<void**>(p);
                if (q) MemFree(q);
                *reinterpret_cast<uint64_t*>(p) = 0;
                /* the `p+8 = 0` store is DROPPED */
            }
    } else {
        FreeInnerArraysCore(ours.buf, count, s);
    }
    shipped(nullptr, theirs.buf, count);

    const bool eq = (ours.bytes == 0) ||
                    (std::memcmp(ours.buf, theirs.buf, ours.bytes) == 0);
    if (negative) {
        if (!eq) ++g_negCaught;
        else Fail(label, "NEGATIVE control compared EQUAL (dropped header store)");
    } else if (!eq) {
        char d[192];
        // Name the first differing byte: a verdict without a locus is the
        // failure reference_shadow_predicate_must_name_its_failing_term.md
        // describes.
        size_t at = 0;
        while (at < ours.bytes && ours.buf[at] == theirs.buf[at]) ++at;
        std::snprintf(d, sizeof(d),
                      "%s n=%d stride=0x%x first diff at +0x%zx (elem %zu +0x%zx): "
                      "ours=%02x theirs=%02x",
                      s->name, count, s->stride, at, at / s->stride, at % s->stride,
                      ours.buf[at], theirs.buf[at]);
        Fail(label, d);
    }

    MemFree(ours.buf);
    MemFree(theirs.buf);
}

// ===========================================================================
// BODY B — classification, not memcmp
// ===========================================================================
struct SubjectB {
    uint8_t*  buf;
    size_t    bytes;
    StrBlock* blocks;    // one per slot; base==nullptr for a deliberately NULL slot
    int       nSlots;
};

SubjectB BuildB(const DestroySpec* s, int count, int* outShared, int* outLast,
                int* outNull) {
    SubjectB sub;
    sub.nSlots = count * s->nOffs;
    sub.bytes  = static_cast<size_t>(count) * s->stride;
    sub.buf    = static_cast<uint8_t*>(MemAlloc(sub.bytes ? sub.bytes : 1, 8));
    sub.blocks = static_cast<StrBlock*>(
        MemAlloc(sizeof(StrBlock) * (sub.nSlots ? sub.nSlots : 1), 8));
    for (size_t i = 0; i < sub.bytes; ++i) sub.buf[i] = static_cast<uint8_t>(0x5A + (i & 0x1f));

    int shared = 0, last = 0, nul = 0, slot = 0;
    for (int i = 0; i < count; ++i) {
        for (int k = 0; k < s->nOffs; ++k, ++slot) {
            uint8_t* p = sub.buf + static_cast<size_t>(i) * s->stride + s->offs[k];
            // Three seeds, in rotation, so every arm is driven in every case:
            //   refcount 1 -> reaches zero, block is freed, slot keeps str-8
            //   refcount 3 -> survives,     block is kept,  slot becomes NULL
            //   NULL       -> the `if (str)` guard, no work at all
            const int which = slot % 3;
            if (which == 2) {
                sub.blocks[slot].base = nullptr;
                sub.blocks[slot].str  = nullptr;
                *reinterpret_cast<void**>(p) = nullptr;
                ++nul;
            } else {
                sub.blocks[slot] = MakeString(which == 0 ? 1 : 3);
                *reinterpret_cast<void**>(p) = sub.blocks[slot].str;
                if (which == 0) ++last; else ++shared;
            }
        }
    }
    *outShared = shared; *outLast = last; *outNull = nul;
    return sub;
}

// 0 = NULL, 1 = this side's own block base (the freeing arm), 2 = unchanged,
// 3 = anything else (which is always a defect).
int Classify(const SubjectB& sub, int slot, const DestroySpec* s, int count) {
    const int perElem = s->nOffs;
    const int i = slot / perElem, k = slot % perElem;
    (void)count;
    uint8_t* p = sub.buf + static_cast<size_t>(i) * s->stride + s->offs[k];
    void* v = *reinterpret_cast<void**>(p);
    if (v == nullptr) return 0;
    if (v == sub.blocks[slot].base) return 1;
    if (v == sub.blocks[slot].str)  return 2;
    return 3;
}

// The wrong body for the negative control: the two arms, swapped.
void ReleaseStringsSwapped(uint8_t* base, int count, const DestroySpec* s) {
    if (count <= 0) return;
    for (int64_t i = 0; i < count; ++i) {
        uint8_t* e = base + i * s->stride;
        for (int k = 0; k < s->nOffs; ++k) {
            uint8_t** slot = reinterpret_cast<uint8_t**>(e + s->offs[k]);
            uint8_t* str = *slot;
            if (!str) continue;
            int32_t* rc = reinterpret_cast<int32_t*>(str - 4);
            --*rc;
            if (*rc != 0) { *slot = str - 8; continue; }   // SWAPPED
            *slot = nullptr;                                // SWAPPED
            MemFree(str - 8);
        }
    }
}

void RunB(const char* label, const DestroySpec* s, uintptr_t shippedAddr,
          int count, bool negative) {
    using Fn = void (*)(void*, void*, int);
    Fn shipped = reinterpret_cast<Fn>(ResolveRuntimeRaw(shippedAddr));
    if (!shipped) {
        std::fprintf(stderr, "[RtArrayDestroyProbe] SKIP %s: no shipped body at 0x%llx\n",
                     label, static_cast<unsigned long long>(shippedAddr));
        return;
    }

    int sh1 = 0, la1 = 0, nu1 = 0, sh2 = 0, la2 = 0, nu2 = 0;
    SubjectB ours   = BuildB(s, count, &sh1, &la1, &nu1);
    SubjectB theirs = BuildB(s, count, &sh2, &la2, &nu2);

    ++g_cases;
    if (sh1 + la1 > 0) {
        ++g_didWork;
        g_decremented += sh1 + la1;
        g_reachedZero += la1;
        g_survived    += sh1;
        g_freed       += la1;
    }

    if (negative) { ++g_negRun; ReleaseStringsSwapped(ours.buf, count, s); }
    else          { ReleaseStringsCore(ours.buf, count, s); }
    shipped(nullptr, theirs.buf, count);

    bool bad = false;
    char d[224];
    d[0] = '\0';

    // (1) per-slot classification
    for (int slot = 0; slot < ours.nSlots && !bad; ++slot) {
        const int co = Classify(ours, slot, s, count);
        const int ct = Classify(theirs, slot, s, count);
        if (co != ct || co == 3) {
            bad = true;
            std::snprintf(d, sizeof(d),
                          "%s slot %d of %d: ours=%s theirs=%s "
                          "(0=NULL 1=block-base 2=unchanged 3=UNKNOWN)",
                          s->name, slot, ours.nSlots,
                          co == 0 ? "NULL" : co == 1 ? "block-base" : co == 2 ? "unchanged" : "UNKNOWN",
                          ct == 0 ? "NULL" : ct == 1 ? "block-base" : ct == 2 ? "unchanged" : "UNKNOWN");
        }
    }

    // (2) the refcount of every SURVIVING block — the check lane Q could not
    //     make, because its subjects were all-zero and never entered this path.
    for (int slot = 0; slot < ours.nSlots && !bad; ++slot) {
        if (!ours.blocks[slot].base) continue;
        // Skip by SEED, not by classification: reading `str - 4` of a block the
        // body just freed is a use-after-free, and under the swapped negative
        // body the classification does not say which those are. slot%3==0 is
        // BuildB's refcount-1 seed.
        if (slot % 3 == 0) continue;
        const int32_t ro = *reinterpret_cast<int32_t*>(ours.blocks[slot].str - 4);
        const int32_t rt = *reinterpret_cast<int32_t*>(theirs.blocks[slot].str - 4);
        if (ro != rt) {
            bad = true;
            std::snprintf(d, sizeof(d), "%s slot %d refcount ours=%d theirs=%d",
                          s->name, slot, ro, rt);
        }
    }

    // (3) every byte that is NOT a slot word. A wrong stride or offset lands
    //     here even when the classification happens to agree.
    if (!bad) {
        for (size_t off = 0; off < ours.bytes && !bad; ++off) {
            bool isSlot = false;
            const size_t inElem = off % s->stride;
            for (int k = 0; k < s->nOffs; ++k)
                if (inElem >= s->offs[k] && inElem < s->offs[k] + 8) isSlot = true;
            if (isSlot) continue;
            if (ours.buf[off] != theirs.buf[off]) {
                bad = true;
                std::snprintf(d, sizeof(d),
                              "%s non-slot byte +0x%zx (elem %zu +0x%zx): ours=%02x theirs=%02x",
                              s->name, off, off / s->stride, inElem,
                              ours.buf[off], theirs.buf[off]);
            }
        }
    }

    if (negative) {
        if (bad) ++g_negCaught;
        else Fail(label, "NEGATIVE control compared EQUAL (release arms swapped)");
    } else if (bad) {
        Fail(label, d);
    }

    // Free what neither body freed.
    //
    // *** DO NOT infer this from the post-state. *** A block's slot classifying
    // as NULL does not mean the block survived: the SWAPPED negative body frees
    // the block and then stores NULL, so a cleanup keyed on the classification
    // would double-free every refcount-1 slot the moment the negative control
    // ran � a harness manufacturing its own crash, which is the failure
    // reference_longjmp_unwind_runs_retail_cleanup.md is about.
    //
    // It is keyed on the SEED instead, which is knowable before either body
    // ran: BuildB gives slot%3==0 a refcount of 1, and a refcount of 1 is freed
    // by the correct body and by both wrong ones.
    for (int slot = 0; slot < ours.nSlots; ++slot) {
        if (slot % 3 == 0) continue;                 // freed by the body itself
        if (ours.blocks[slot].base)   MemFree(ours.blocks[slot].base);
        if (theirs.blocks[slot].base) MemFree(theirs.blocks[slot].base);
    }
    MemFree(ours.buf);    MemFree(ours.blocks);
    MemFree(theirs.buf);  MemFree(theirs.blocks);
}

// ===========================================================================
// WAVE 9 LANE DESTROY — 0x1800298f0's SHRINK-TAIL arm, end to end
// ===========================================================================
//
// WHY THIS EXISTS AND WHAT IT IS NOT.
//
// 0x180026b50 is reached from TWO branches of `SetSizeCore`: the realloc arm
// (retail 0x180029a0e, whole old array) and the shrink-with-no-realloc arm
// (0x180029a68, tail only). A live boot drives the FIRST one 16 times and the
// SECOND one ZERO times, because `ReadPackfile` only ever grows the entry
// array. Reporting "the port is verified" off a run in which one of its two
// call sites never fired would be exactly the coverage claim
// reference_probe_needs_an_effectiveness_control.md warns about, so the arm
// with no live population gets driven here instead.
//
// This is NOT a second test of the destroy body — RunB above already replays
// that against the shipped 0x180026b50. What this adds is the CALL SHAPE: the
// shrink arm hands the helper `old + newSize*stride` and `oldSize - newSize`,
// i.e. a base that is not the buffer start and a count that is not the size,
// and a transcription that got either wrong would destroy the wrong elements
// while still being a correct destroy of *something*.
//
// THE SHRINK ARM NEEDS NO ELEMENT ASSIGN, which is what makes it cheap and
// makes the comparison total. With `newSize < oldSize` and no realloc,
// SetSizeCore's copy/fill loop runs from `oldSize` to `newSize` and therefore
// never executes: no `AssignPkgEntry`, no `System::Filename` copy ctor, no
// MemAlloc. Every byte of the subject is either seeded by us or written by the
// destroy, so unlike the realloc arm there is no uninitialised tail to exclude
// (reference_oracle_must_not_compare_unwritten_bytes.md) and the buffers can be
// compared whole.
//
// TWO INDEPENDENTLY BUILT SUBJECTS, COMPARED AFTER EACH STEP. A destroy frees
// memory, so an ours-then-theirs replay on one subject double-frees. The two
// are built by the same seeding code from the same schedule and then driven by
// OUR SetSizeCore and by the SHIPPED 0x1800298f0 respectively.
// The WRONG shrink arm, for the negative control. Identical to SetSizeCore's
// shrink limb in every respect except the BASE handed to the helper: it
// destroys the first `oldSize - newSize` elements instead of the last ones.
// Chosen over "destroy nothing" because the frees still balance and every
// refcount in the buffer still moves — a comparator that merely counted frees,
// or that only looked at the tail, would pass it.
void DestroyHeadNotTail(ArrayHdr* a, int newSize) {
    const int oldSize = a->_size;
    a->_size = newSize;
    if (oldSize <= newSize) return;
    kSpec_PkgEntry.destroy(nullptr, a->_data, oldSize - newSize);
}

void RunShrinkArm(int oldSize, int newSize, bool negative) {
    using Fn = void (*)(void*, int);
    Fn shipped = reinterpret_cast<Fn>(ResolveRuntimeRaw(0x1800298f0));
    if (!shipped) {
        std::fprintf(stderr, "[RtArrayDestroyProbe] SKIP shrink-arm: "
                             "no shipped 0x1800298f0\n");
        return;
    }
    const uint32_t stride = kSpec_PkgEntry.stride;

    // Same seeding rotation BuildB uses, so all three of Body B's arms
    // (refcount reaches zero / survives / slot already NULL) appear in the
    // destroyed tail AND in the retained head.
    StrBlock* blkO = static_cast<StrBlock*>(MemAlloc(sizeof(StrBlock) * oldSize, 8));
    StrBlock* blkT = static_cast<StrBlock*>(MemAlloc(sizeof(StrBlock) * oldSize, 8));
    uint8_t* bufO = static_cast<uint8_t*>(MemAlloc(static_cast<uint64_t>(oldSize) * stride, 8));
    uint8_t* bufT = static_cast<uint8_t*>(MemAlloc(static_cast<uint64_t>(oldSize) * stride, 8));
    for (int i = 0; i < oldSize; ++i) {
        uint8_t* eO = bufO + static_cast<size_t>(i) * stride;
        uint8_t* eT = bufT + static_cast<size_t>(i) * stride;
        // The two int32 in the element tail: distinct per element, so a wrong
        // BASE shows up as a surviving element whose tail moved.
        for (uint32_t o = 0; o < stride; ++o) {
            eO[o] = static_cast<uint8_t>(0x31 + i * 7 + o);
            eT[o] = eO[o];
        }
        const int which = i % 3;
        if (which == 2) {
            blkO[i].base = blkT[i].base = nullptr;
            blkO[i].str  = blkT[i].str  = nullptr;
            *reinterpret_cast<void**>(eO) = nullptr;
            *reinterpret_cast<void**>(eT) = nullptr;
        } else {
            blkO[i] = MakeString(which == 0 ? 1 : 3);
            blkT[i] = MakeString(which == 0 ? 1 : 3);
            *reinterpret_cast<void**>(eO) = blkO[i].str;
            *reinterpret_cast<void**>(eT) = blkT[i].str;
        }
    }

    ArrayHdr aO = { bufO, oldSize, oldSize };     // cap == size: no realloc
    ArrayHdr aT = { bufT, oldSize, oldSize };

    ++g_cases;
    if (oldSize > newSize) {
        ++g_didWork;
        if (!negative) { ++g_shrinkArm; g_shrinkElems += oldSize - newSize; }
    }

    if (negative) {
        // The wrong body for this arm, and it is wrong in the way a
        // transcription actually goes wrong: destroy the HEAD instead of the
        // TAIL. Same helper, same count, wrong base. Every refcount in the
        // buffer still changes, so a comparator that only counted frees would
        // pass it.
        ++g_negRun;
        DestroyHeadNotTail(&aO, newSize);
    } else {
        SetSizeCore(&aO, newSize, nullptr, &kSpec_PkgEntry, nullptr);
    }
    shipped(&aT, newSize);

    bool bad = false;
    char d[224];
    d[0] = '\0';

    if (aO._data != bufO || aT._data != bufT) {
        // Neither side may reallocate: cap == size and newSize < size.
        bad = true;
        std::snprintf(d, sizeof(d), "shrink %d->%d REALLOCATED (ours=%d theirs=%d)",
                      oldSize, newSize, aO._data != bufO, aT._data != bufT);
    }
    if (!bad && (aO._size != aT._size || aO._capacity != aT._capacity)) {
        bad = true;
        std::snprintf(d, sizeof(d), "shrink %d->%d header ours={%d,%d} theirs={%d,%d}",
                      oldSize, newSize, aO._size, aO._capacity, aT._size, aT._capacity);
    }
    // Every byte of the WHOLE buffer, retained head and destroyed tail alike.
    // The head is the half that catches a wrong base: destroy the head instead
    // of the tail and elements 0..newSize-1 stop matching while the frees still
    // balance.
    if (!bad) {
        for (size_t off = 0; off < static_cast<size_t>(oldSize) * stride && !bad; ++off) {
            if (bufO[off] == bufT[off]) continue;
            // The freed-arm slot holds `str - 8`, which is a different address
            // on each side by construction. Compare those slots by CLASS, not
            // by value.
            const size_t elem = off / stride, inElem = off % stride;
            if (inElem < 8) {
                void* vo = *reinterpret_cast<void**>(bufO + elem * stride);
                void* vt = *reinterpret_cast<void**>(bufT + elem * stride);
                const int co = vo == nullptr ? 0 : vo == blkO[elem].base ? 1
                             : vo == blkO[elem].str ? 2 : 3;
                const int ct = vt == nullptr ? 0 : vt == blkT[elem].base ? 1
                             : vt == blkT[elem].str ? 2 : 3;
                if (co == ct && co != 3) { off = elem * stride + 7; continue; }
                bad = true;
                std::snprintf(d, sizeof(d),
                              "shrink %d->%d elem %zu slot class ours=%d theirs=%d "
                              "(0=NULL 1=block-base 2=unchanged 3=UNKNOWN)",
                              oldSize, newSize, elem, co, ct);
                continue;
            }
            bad = true;
            std::snprintf(d, sizeof(d),
                          "shrink %d->%d elem %zu +0x%zx tail byte ours=%02x theirs=%02x",
                          oldSize, newSize, elem, inElem, bufO[off], bufT[off]);
        }
    }
    // Refcounts of the blocks that must have SURVIVED (the i%3==1 seed, rc 3).
    // Skipped by SEED, never by post-state — see the cleanup note in RunB.
    if (!bad) {
        for (int i = 0; i < oldSize && !bad; ++i) {
            if (i % 3 != 1) continue;
            const int32_t ro = *reinterpret_cast<int32_t*>(blkO[i].str - 4);
            const int32_t rt = *reinterpret_cast<int32_t*>(blkT[i].str - 4);
            if (ro == rt) continue;
            bad = true;
            std::snprintf(d, sizeof(d), "shrink %d->%d elem %d refcount ours=%d theirs=%d",
                          oldSize, newSize, i, ro, rt);
        }
    }

    if (negative) {
        if (bad) ++g_negCaught;
        else Fail("shrink-arm", "NEGATIVE control compared EQUAL (head destroyed, not tail)");
    } else if (bad) {
        Fail("shrink-arm", d);
    }

    // Cleanup keyed on the SEED and on which elements EACH SIDE'S body was
    // given to destroy — never on the post-state, which the negative body
    // deliberately falsifies.
    //
    // *** THE TWO SIDES NEED TWO PREDICATES, AND SHARING ONE IS A DOUBLE FREE.
    // *** The first draft of this loop used `negative ? head : tail` for both
    // sides. It is right for OURS and wrong for THEIRS: the shipped body always
    // destroys the TAIL, so under the negative control every refcount-1 block
    // in theirs' tail was freed by the shipped body and freed again here. It
    // did not fault in the probe — it faulted three frames later inside the
    // DLL allocator's own free, on the next unrelated allocation, which reads
    // exactly like a port defect in whatever ran next. A harness manufacturing
    // its own crash (reference_longjmp_unwind_runs_retail_cleanup.md).
    for (int i = 0; i < oldSize; ++i) {
        const bool ourDestroyed   = negative ? (i < oldSize - newSize) : (i >= newSize);
        const bool theirDestroyed = (i >= newSize);       // shipped: always the tail
        const bool bodyFrees = (i % 3 == 0);              // the refcount-1 seed
        if (blkO[i].base && !(ourDestroyed   && bodyFrees)) MemFree(blkO[i].base);
        if (blkT[i].base && !(theirDestroyed && bodyFrees)) MemFree(blkT[i].base);
    }
    MemFree(bufO); MemFree(bufT); MemFree(blkO); MemFree(blkT);
}

// ===========================================================================
// 0x180003970 — Array<T*>::operator=, fully replayable
// ===========================================================================
void RunAssign(int srcN, int dstN, int dstCap, bool negative) {
    using Fn = void* (*)(void*, const void*);
    Fn shipped = reinterpret_cast<Fn>(ResolveRuntimeRaw(0x180003970));
    if (!shipped) return;

    ArrayHdr src = { nullptr, 0, 0 };
    if (srcN > 0) {
        src._data = static_cast<uint8_t*>(MemAlloc(static_cast<uint64_t>(srcN) * 8, 8));
        for (int i = 0; i < srcN; ++i)
            reinterpret_cast<void**>(src._data)[i] =
                reinterpret_cast<void*>(static_cast<uintptr_t>(0x1000 + i * 0x40));
    }
    src._size = srcN; src._capacity = srcN;

    ArrayHdr a = { nullptr, 0, 0 }, b = { nullptr, 0, 0 };
    if (dstCap > 0) {
        a._data = static_cast<uint8_t*>(MemAlloc(static_cast<uint64_t>(dstCap) * 8, 8));
        b._data = static_cast<uint8_t*>(MemAlloc(static_cast<uint64_t>(dstCap) * 8, 8));
        std::memset(a._data, 0xC3, static_cast<size_t>(dstCap) * 8);
        std::memset(b._data, 0xC3, static_cast<size_t>(dstCap) * 8);
    }
    a._size = b._size = dstN;
    a._capacity = b._capacity = dstCap;

    ++g_cases;
    if (srcN > 0 || dstCap > 0) ++g_didWork;

    if (negative) {
        ++g_negRun;
        // WRONG ON PURPOSE: always treble. Retail trebles only when
        // newSize == oldSize+1 && oldSize != 0, and since the destructor has
        // already zeroed _size, oldSize is ALWAYS 0 here and it NEVER trebles.
        ArrayDtorImpl(&a);
        a._size = srcN;
        if (srcN > a._capacity) {
            a._capacity = srcN * 3;
            a._data = static_cast<uint8_t*>(
                MemAlloc(static_cast<uint64_t>(a._capacity) * 8, 8));
        }
        for (int i = 0; i < srcN; ++i)
            reinterpret_cast<void**>(a._data)[i] = reinterpret_cast<void**>(src._data)[i];
    } else {
        RtColl_ArrayAssign_180003970(&a, &src);
    }
    shipped(&b, &src);

    const bool eq = (a._size == b._size) && (a._capacity == b._capacity) &&
                    (a._size == 0 ||
                     std::memcmp(a._data, b._data, static_cast<size_t>(a._size) * 8) == 0);
    if (negative) {
        if (!eq) ++g_negCaught;
        else Fail("operator= @0x180003970",
                  "NEGATIVE control compared EQUAL (growth rule trebled)");
    } else if (!eq) {
        char d[192];
        std::snprintf(d, sizeof(d),
                      "src=%d dst=%d/%d -> ours size=%d cap=%d, theirs size=%d cap=%d",
                      srcN, dstN, dstCap, a._size, a._capacity, b._size, b._capacity);
        Fail("operator= @0x180003970", d);
    }

    MemFree(a._data); MemFree(b._data); MemFree(src._data);
}

}  // namespace
}  // namespace rtcoll

// ===========================================================================
// Entry points
// ===========================================================================
bool RtCollectionDestroyProbeArmed() {
    const char* v = std::getenv("BANISHED_RT_ARRAY_DESTROY_PROBE");
    return v && *v && std::strcmp(v, "0") != 0;
}

void RtCollectionDestroyProbeRun() {
    using namespace rtcoll;
    const char* mode = std::getenv("BANISHED_RT_ARRAY_DESTROY_PROBE");
    if (!mode || !*mode) return;
    const bool neg = (std::strcmp(mode, "neg") == 0);

    std::fprintf(stderr, "[RtArrayDestroyProbe] start (mode=%s)\n", mode);

    // Counts chosen so every case runs both a non-empty loop and, at n=0, the
    // `TEST R8D,R8D / JLE` early return. The n=0 case is counted as INEFFECTIVE
    // on purpose: a shadow placed after an early return makes `calls=` mean
    // "was entered" instead of "did work"
    // (reference_probe_needs_an_effectiveness_control.md).
    static const int kCounts[] = { 0, 1, 3, 8 };

    for (int ci = 0; ci < 4; ++ci) {
        const int n = kCounts[ci];

        // ---- BODY A: the five registered helpers -------------------------
        RunA("0x18001ce90", &kD_InnerArr_110, 0x18001ce90, n, 4, false);
        RunA("0x1800419b0", &kD_InnerArr_18,  0x1800419b0, n, 4, false);
        RunA("0x180052110", &kD_InnerArr_40,  0x180052110, n, 4, false);
        RunA("0x180074100", &kD_InnerArr_48,  0x180074100, n, 4, false);
        RunA("0x18005bce0", &kD_InnerArr3_48, 0x18005bce0, n, 4, false);
        // Every slot NULL: the `if (p)` arm, which must not free 0.
        RunA("0x180074100/allnull", &kD_InnerArr_48, 0x180074100, n, 1, false);

        // ---- BODY B: the five registered helpers -------------------------
        RunB("0x180011010", &kD_Str_08,  0x180011010, n, false);
        RunB("0x180005930", &kD_Str_10,  0x180005930, n, false);
        RunB("0x180014a40", &kD_Str_48,  0x180014a40, n, false);
        RunB("0x180034440", &kD_Str_18,  0x180034440, n, false);
        RunB("0x1800059e0", &kD_Str2_38, 0x1800059e0, n, false);
        // WAVE 9 LANE DESTROY. The SHIPPED body compared against is
        // 0x180026b50 itself, NOT 0x180005930 — the two are claimed to be one
        // body in array_destroy.cpp and a probe that replayed the other would
        // be assuming exactly the thing under test.
        RunB("0x180026b50", &kD_Str_10,  0x180026b50, n, false);
    }

    // ---- 0x180003970 -------------------------------------------------------
    // The pre-states that matter: empty->empty, empty->n, n->1 (the case whose
    // capacity would treble if `oldSize` were read BEFORE the destructor rather
    // than after), and a dst with spare capacity that is nonetheless discarded
    // because the destructor zeroed it.
    RunAssign(0, 0, 0, false);
    RunAssign(5, 0, 0, false);
    RunAssign(1, 0, 0, false);
    RunAssign(1, 1, 1, false);
    RunAssign(4, 8, 8, false);
    RunAssign(9, 3, 4, false);

    // ---- WAVE 9 LANE DESTROY: 0x1800298f0's shrink-tail arm ----------------
    // The arm a live boot never takes. Sizes chosen so the destroyed tail
    // contains at least one of each seed (freed / survived / already-NULL) and
    // so does the retained head — 9->3 leaves head {rc1, rc3, NULL} and tail
    // {rc1, rc3, NULL, rc1, rc3, NULL}.
    RunShrinkArm(9, 3, false);
    RunShrinkArm(4, 1, false);
    RunShrinkArm(3, 0, false);      // the whole array, still via the shrink arm

    const int mismatchesBeforeNeg = g_mismatch;

    if (neg) {
        RunA("NEG:0x180074100", &kD_InnerArr_48, 0x180074100, 4, 4, true);
        RunA("NEG:0x18005bce0", &kD_InnerArr3_48, 0x18005bce0, 3, 4, true);
        RunB("NEG:0x180034440", &kD_Str_18,  0x180034440, 6, true);
        RunB("NEG:0x1800059e0", &kD_Str2_38, 0x1800059e0, 4, true);
        // The comparator must be shown to be able to FAIL on this address too:
        // a negative control run only against a sibling address proves the
        // comparator works, not that this row is wired to it.
        RunB("NEG:0x180026b50", &kD_Str_10,  0x180026b50, 5, true);
        RunShrinkArm(9, 3, true);
        RunAssign(5, 0, 0, true);
    }

    std::fprintf(stderr,
                 "[RtArrayDestroyProbe] cases=%d did-work=%d mismatches=%d\n"
                 "[RtArrayDestroyProbe] freed=%d refcounts-decremented=%d "
                 "reached-zero=%d survived=%d\n",
                 g_cases, g_didWork, mismatchesBeforeNeg,
                 g_freed, g_decremented, g_reachedZero, g_survived);

    std::fprintf(stderr,
                 "[RtArrayDestroyProbe] 0x1800298f0 SHRINK-TAIL arm: %d cases, "
                 "%d elements destroyed via base=old+newSize*stride\n"
                 "[RtArrayDestroyProbe] (the REALLOC arm has a live population "
                 "-- see the two kSpec_PkgEntry arm sites in the RtDiff summary)\n",
                 g_shrinkArm, g_shrinkElems);
    if (g_shrinkArm == 0) {
        std::fprintf(stderr,
            "[RtArrayDestroyProbe] *** 0x180026b50's SHRINK-TAIL call site was "
            "NEVER DRIVEN.\nOne of its two call sites is unverified and the "
            "green line above does not cover it. ***\n");
    }

    if (g_didWork == 0) {
        std::fprintf(stderr,
            "[RtArrayDestroyProbe] *** EFFECTIVENESS CONTROL FAILED: every "
            "comparison was a no-op.\nThis run proves NOTHING. ***\n");
    }
    if (g_freed == 0 || g_reachedZero == 0 || g_survived == 0) {
        std::fprintf(stderr,
            "[RtArrayDestroyProbe] *** EFFECTIVENESS CONTROL FAILED: an ARM was "
            "never driven\n(freed=%d reached-zero=%d survived=%d). The String "
            "asymmetry is the only thing\nthis comparator can see, and a run that "
            "does not enter both arms has not seen it. ***\n",
            g_freed, g_reachedZero, g_survived);
    }
    if (neg) {
        std::fprintf(stderr, "[RtArrayDestroyProbe] negative: run=%d caught=%d\n",
                     g_negRun, g_negCaught);
        if (g_negCaught != g_negRun) {
            std::fprintf(stderr,
                "[RtArrayDestroyProbe] *** NEGATIVE CONTROL FAILED: %d of %d "
                "deliberately WRONG bodies\ncompared EQUAL. The comparator is "
                "broken, so the green result above is worthless. ***\n",
                g_negRun - g_negCaught, g_negRun);
        }
    } else {
        std::fprintf(stderr,
            "[RtArrayDestroyProbe] negative control NOT RUN. Re-run with "
            "BANISHED_RT_ARRAY_DESTROY_PROBE=neg\nbefore reading the line above "
            "as a verification.\n");
    }
}
