// ============================================================================
// runtime/src/collection/introsort.cpp — Wave 9 lane COLL
//   0x1800264a0 — the packfile sort, and 0x180051650 / 0x180050d30, the last
//   two element copy-assign helpers lane Q left on the DLL.
// ============================================================================
//
// ============================================================================
// PART 1 — 0x1800264a0, the packfile introsort
// ============================================================================
//
// WHY IT IS HERE, GIVEN LANE PKG DELIBERATELY DECLINED IT
// -------------------------------------------------------
// runtime/src/io/filesystem_packages.cpp:753 says, and it was right at the
// time:
//
//     "DELIBERATELY NOT PORTED ... every PackageFile arrives here with the
//      SAME +0x70 ... A tie-heavy population is exactly where an unstable
//      sort's exact swap sequence decides the answer, and that answer decides
//      which package owns a colliding path in LOOP 3. A 'clean' std::sort here
//      would compile, run, and silently pick different winners. With no run
//      and no oracle available in this lane, transcribing it would be an
//      unverified guess at a precedence rule."
//
// Every clause of that is correct and none of it is an argument against
// porting; it is an argument against porting it BY RESEMBLANCE and without an
// oracle. Both objections are answerable, and this file answers them:
//
//   * It is transcribed instruction by instruction from 0x1800264a0, not
//     written as "an introsort". The pending-range stack, the median-of-3
//     order, the two equal-run scans and the push-the-larger-half rule are all
//     retail's, and each is annotated with the instruction it came from.
//   * A sort IS replayable, which makes it one of the easiest things in this
//     wave to verify: it is a pure permutation of a buffer the caller owns.
//     The probe hands the SAME array to both implementations — ours on one
//     copy, the shipped body on another — and compares the resulting
//     PERMUTATION, not the sortedness. On a tie-heavy population that is
//     exactly the "which package wins" question lane PKG could not answer, and
//     the probe's corpus is deliberately mostly ties (see introsort_probe).
//
// THE ORDER IS DESCENDING, and that is the first thing to get wrong.
// `CMOVG RDX,R8` at 0x1800264fc moves the tracked position to R8 when the
// TRACKED element's key is GREATER, so the small-range pass tracks the
// MINIMUM and swaps it to the HIGH end (0x180026509..0x180026516), then
// shrinks the high bound. The partition's scans agree: the left scan advances
// while `pivot->key <= (*i)->key`, i.e. while the element belongs on the left,
// which for a descending order means "is at least the pivot".
//
// THE COMPARATOR IS THE WHOLE OF `CMP dword ptr [RCX+0x70],EAX` — a SIGNED
// 32-bit compare of one field. There is no tiebreak, no secondary key, and no
// pointer comparison. Every ordering decision below is that one compare.
//
// THE THIRD ARGUMENT IS DEAD. The call site passes `LEA R8,[RBP+0x48]`; the
// callee never reads R8. The signature keeps two parameters, which is what the
// existing call site at filesystem_packages.cpp:767 already declares.
//
// THE PENDING-RANGE STACK IS 62 ENTRIES AND HAS NO BOUNDS CHECK
// (`SUB RSP,0x3e0` gives 0x1f0 bytes of lo and 0x1f0 of hi). That is safe only
// because the split always PUSHES THE LARGER half and iterates on the smaller
// (0x180026653 `CMP RCX,RDX / JL`), which bounds the depth by log2(n). Written
// as retail wrote it — including the absent bounds check.
//
// ============================================================================
// PART 2 — 0x180051650 and 0x180050d30
// ============================================================================
//
// runtime/src/collection/array.cpp:386 records these as the last two rows of
// its non-trivial tier that are "still the DLL's", and 0x180050d30 is a second
// unregistered door in its own right (audio.cpp:1114). Both close here.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/collection/Collection.h"   // rtcoll::DoorAlloc (the one allocator door)
#include "avl_trees.h"

// Declared locally rather than including standalone/resolve.h (PLAN §4.1
// decision 1). Byte-identical to array.cpp:133; the linker checks they agree.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace rtsort {

// ---------------------------------------------------------------------------
// EFFECTIVENESS COUNTERS — branches taken INSIDE this body.
//
// A sort is the easiest place in the world to write a control that measures
// nothing: "it came out sorted" is true of a bubble sort, a std::sort and a
// broken transcription alike, and on the four-element all-ties population the
// boot actually runs it is true of doing NOTHING AT ALL. So these count the
// structural branches whose absence would make a green permutation comparison
// vacuous — the big-range path, the pivot fixup, the two equal-run walks and
// the pending stack. The probe FAILS LOUDLY if any of them is zero across its
// whole corpus, because that means the corpus, not the code, was the problem.
// ---------------------------------------------------------------------------
struct SortCounters {
    uint64_t calls, smallPasses, smallSwaps, medianSwaps;
    uint64_t partitions, partitionSwaps, pivotFixups;
    uint64_t eqRunA, eqRunB, pushes, pops;
};
SortCounters g_sc = {};

// The one field the comparator reads. `elem` is a `PackageFile*`; +0x70 is a
// SIGNED dword the PackageFile constructor seeds with 0x2710 and only
// ReadPackfile changes (filesystem_packages.cpp §"the comparator").
static inline int32_t Key(void* elem) {
    return *reinterpret_cast<const int32_t*>(static_cast<uint8_t*>(elem) + 0x70);
}

// ---------------------------------------------------------------------------
// 0x1800264a0 — sort `count` pointers at `first`, DESCENDING by +0x70.
//
// The register mapping, kept in the local names so the disassembly can be read
// next to this:
//
//   R11 -> lo     the low bound of the range being worked, inclusive
//   R10 -> hi     the high bound, INCLUSIVE (retail seeds it first+count-1)
//   RBX -> depth  the pending-range stack cursor
//   R9  -> pivot  a POINTER TO A SLOT, not a value — the partition moves it
//   RAX -> i      the forward cursor
//   R8  -> j      the backward cursor
// ---------------------------------------------------------------------------
void SortByKey70(void** first, int32_t count) {
    ++g_sc.calls;
    // Self-scheduled probe, same contract as the tree entry points: it
    // allocates, so it cannot run from a *PortInit. In practice THIS is the
    // first of the lane's entry points the boot reaches, because LoadPackages
    // runs before Object::Factory::Startup.
    void** stackLo[62];
    void** stackHi[62];
    int64_t depth = 0;

    void** lo = first;
    void** hi = first + (static_cast<int64_t>(count) - 1);   // MOVSXD / DEC

    for (;;) {
        // 0x1800264c0 — `(hi - lo)/8 + 1`, truncated to 32 bits by `INC EAX`.
        const int32_t n = static_cast<int32_t>(hi - lo) + 1;

        if (n <= 8) {
            // ---- small range: selection sort, minimum to the HIGH end -----
            // 0x1800264d1 `CMP R10,R11 / JBE` — an empty or single-element
            // range pops immediately. The comparison is UNSIGNED on the
            // pointers, which is why it is written as `>` on pointers here.
            if (hi > lo) {
                void** const scanFrom = lo + 1;      // 0x1800264d6, HOISTED —
                                                     // it is NOT recomputed as
                                                     // `hi` shrinks
                do {
                    ++g_sc.smallPasses;
                    void** best = lo;                // 0x1800264e0
                    for (void** p = scanFrom; p <= hi; ++p) {
                        // 0x1800264f0: CMOVG — move to p only when the tracked
                        // element is STRICTLY greater, so among equal keys the
                        // EARLIEST position wins. That tie rule is the whole
                        // of the "which package" question.
                        if (Key(*best) > Key(*p)) best = p;
                    }
                    // 0x180026509 — swap *best with *hi, then hi--.
                    ++g_sc.smallSwaps;
                    void* a = *hi;
                    void* b = *best;
                    --hi;
                    *best     = a;
                    *(hi + 1) = b;
                } while (hi > lo);
            }
            // 0x18002651f — pop.
            --depth;
            if (depth < 0) return;
            ++g_sc.pops;
            lo = stackLo[depth];
            hi = stackHi[depth];
            continue;
        }

        // ---- median of three: lo, mid, hi ---------------------------------
        // 0x180026539 `CDQE / SAR RAX,1` — mid is `lo + n/2`, an ARITHMETIC
        // shift of the SIGN-EXTENDED count, not `lo + (hi-lo)/2`. For a
        // positive count they agree; the sign extension is transcribed because
        // a negative `count` is already a corrupt call and this port should
        // fail the same way.
        void** pivot = lo + (static_cast<int64_t>(n) >> 1);

        {   // 0x180026542 — if (mid > lo) swap
            void* a = *lo; void* b = *pivot;
            if (Key(b) > Key(a)) { ++g_sc.medianSwaps; *lo = b; *pivot = a; }
        }
        {   // 0x180026553 — if (hi > lo) swap
            void* a = *lo; void* b = *hi;
            if (Key(b) > Key(a)) { ++g_sc.medianSwaps; *lo = b; *hi = a; }
        }
        {   // 0x180026567 — if (hi > mid) swap
            void* a = *pivot; void* b = *hi;
            if (Key(b) > Key(a)) { ++g_sc.medianSwaps; *pivot = b; *hi = a; }
        }

        void** i = lo;    // RAX
        void** j = hi;    // R8

        // ---- the partition loop, 0x180026581 ------------------------------
        ++g_sc.partitions;
        for (;;) {
            bool toJ2 = false;      // reached 0x1800265c0 directly

            if (pivot > i) {
                // 0x180026586 — advance i while the element belongs left,
                // bounded by the PIVOT slot.
                for (;;) {
                    ++i;
                    if (i >= pivot) break;                  // JNC 0x1800265a2
                    if (Key(*pivot) <= Key(*i)) continue;   // JLE, keep going
                    // 0x18002659d — stopped on an element that belongs right.
                    if (pivot > i) { toJ2 = true; }
                    break;
                }
            }

            if (!toJ2) {
                // 0x1800265a2 — the same advance, bounded by `hi`. Retail
                // reaches this both by falling out of the loop above and by
                // the `pivot <= i` test at the top of the partition.
                for (;;) {
                    ++i;
                    if (i > hi) break;                      // JA 0x1800265c0
                    if (Key(*pivot) <= Key(*i)) continue;   // JLE, keep going
                    break;
                }
            }

            // 0x1800265c0 — retreat j while the element belongs right.
            for (;;) {
                --j;
                if (j <= pivot) break;                      // JBE 0x1800265d7
                if (Key(*pivot) > Key(*j)) continue;        // JG, keep going
                break;
            }

            // 0x1800265d7
            if (j < i) break;                               // JC 0x1800265f2

            ++g_sc.partitionSwaps;
            void* a = *j;
            void* b = *i;
            *i = a;
            *j = b;
            // 0x1800265e8 — if the pivot SLOT was the one just moved out of
            // `j`, follow it to `i`. There is no matching fixup for the pivot
            // sitting at `i`; retail has none and neither does this.
            if (pivot == j) { ++g_sc.pivotFixups; pivot = i; }
        }

        // ---- walk j back over the run of keys EQUAL to the pivot ----------
        // 0x1800265f2. Both loops below continue only while the two keys are
        // EQUAL (`JG` out on greater, `JLE` back on less-or-equal, which after
        // the first test can only be equality).
        ++j;
        bool split = false;
        if (pivot < j) {
            for (;;) {
                --j;
                if (j <= pivot) break;                      // JBE 0x180026622
                ++g_sc.eqRunA;
                const int32_t a = Key(*j), b = Key(*pivot);
                if (a > b) break;                           // JG 0x18002661d
                if (b <= a) continue;                       // JLE 0x180026600
                break;
            }
            // 0x18002661d
            if (pivot < j) split = true;                    // JC 0x18002663f
        }
        if (!split) {
            // 0x180026622 — the same equal-run walk, bounded by `lo`.
            for (;;) {
                --j;
                if (j <= lo) break;                         // JBE 0x18002663f
                ++g_sc.eqRunB;
                const int32_t a = Key(*j), b = Key(*pivot);
                if (a > b) break;                           // JG 0x18002663f
                if (b <= a) continue;                       // JLE 0x180026622
                break;
            }
        }

        // ---- 0x18002663f: push the larger half, iterate on the smaller ----
        // The two lengths are computed in BYTES and masked with `AND ...,-8`,
        // which is a no-op on well-formed pointers and is kept because it is
        // what fixes the sign of the comparison retail then makes with `JL`
        // (SIGNED) rather than `JB`.
        const int64_t rightBytes = (reinterpret_cast<int64_t>(hi) -
                                    reinterpret_cast<int64_t>(i)) & ~static_cast<int64_t>(7);
        const int64_t leftBytes  = (reinterpret_cast<int64_t>(j) -
                                    reinterpret_cast<int64_t>(lo)) & ~static_cast<int64_t>(7);

        if (leftBytes >= rightBytes) {
            // 0x180026658
            if (lo < j) { ++g_sc.pushes; stackLo[depth] = lo; stackHi[depth] = j; ++depth; }
            if (i >= hi) {                       // 0x18002666c JNC -> pop
                --depth;
                if (depth < 0) return;
                ++g_sc.pops;
                lo = stackLo[depth];
                hi = stackHi[depth];
                continue;
            }
            lo = i;
            continue;
        }

        // 0x18002667d
        if (i < hi) { ++g_sc.pushes; stackLo[depth] = i; stackHi[depth] = hi; ++depth; }
        if (lo >= j) {                           // 0x180026691 JNC -> pop
            --depth;
            if (depth < 0) return;
            ++g_sc.pops;
            lo = stackLo[depth];
            hi = stackHi[depth];
            continue;
        }
        hi = j;
    }
}

// ============================================================================
// PART 2 — the two element copy-assign helpers
// ============================================================================

// `Collection::Array<T>` header, 16 bytes. Redeclared rather than pulled from
// lane Q's array_internal.h; only the three fields are needed.
struct ArrayHdr16 {
    uint8_t* _data;      // +0x00
    int32_t  _size;      // +0x08
    int32_t  _capacity;  // +0x0c
};
static_assert(sizeof(ArrayHdr16) == 0x10, "Collection::Array is 16 bytes");

// ---------------------------------------------------------------------------
// 0x180050d30 — `Collection::Array<T16>::operator=`, stride 0x10, ALIGN 4.
//
// Structurally the body lane UIW ported at 0x18001cdf0 and this lane ported at
// 0x180009350, with three differences read off the instructions rather than
// assumed:
//
//   * `SHL RCX,0x4` — stride 0x10, not 4 or 8.
//   * `MOV EDX,0x4`  — ALIGN 4, on a 16-byte element. That is the kind of
//     immediate this wave's near-misses differed in, so it is called out: it
//     is 4, not 0x10 and not 8.
//   * the destructor is 0x180051d80 (registered by array.cpp:1343), not
//     0x18001c750.
//
// The element copy is FOUR DWORD MOVs (0x180050d99..0x180050db6), not a
// 16-byte vector move and not a memcpy — it writes +0x00, +0x04, +0x08, +0x0c
// in that order and touches nothing else.
//
// The `old`-treble branch is dead here for the same reason it is dead in the
// other two (the destructor has already zeroed `_size`), and is kept.
// ---------------------------------------------------------------------------
void* Array16Assign(void* dstRaw, const void* srcRaw) {
    ArrayHdr16*       dst = static_cast<ArrayHdr16*>(dstRaw);
    const ArrayHdr16* src = static_cast<const ArrayHdr16*>(srcRaw);
    if (dst == src) return dst;

    using DtorFn = void* (*)(void*);
    static DtorFn dtor = nullptr;          // cache a hit, retry a miss
    if (!dtor) dtor = reinterpret_cast<DtorFn>(ResolveAddr(Module::Runtime, 0x180051d80));
    dtor(dst);

    const int32_t n   = src->_size;
    const int32_t old = dst->_size;        // always 0 here
    dst->_size = n;
    if (n > dst->_capacity) {
        dst->_capacity = n;
        if (n == static_cast<int32_t>(old + 1) && old != 0)
            dst->_capacity = static_cast<int32_t>(n * 3);
        // ENDGAME B.3.2 closing pass: through the one collection allocator door
        // (runtime/src/collection/alloc_door.cpp) rather than a private
        // resolution of 0x1800c7a50 — same allocator, but the channel
        // `BANISHED_REDZONE` can see. `4` is this instantiation's own align.
        dst->_data = static_cast<uint8_t*>(rtcoll::DoorAlloc()(
            static_cast<uint64_t>(static_cast<int64_t>(dst->_capacity)) * 0x10, 4));
    }
    for (int32_t k = 0; k < dst->_size; ++k) {
        uint8_t* p = dst->_data + static_cast<int64_t>(k) * 0x10;
        if (p == nullptr) continue;
        const uint8_t* s = src->_data + static_cast<int64_t>(k) * 0x10;
        *reinterpret_cast<uint32_t*>(p + 0x0) = *reinterpret_cast<const uint32_t*>(s + 0x0);
        *reinterpret_cast<uint32_t*>(p + 0x4) = *reinterpret_cast<const uint32_t*>(s + 0x4);
        *reinterpret_cast<uint32_t*>(p + 0x8) = *reinterpret_cast<const uint32_t*>(s + 0x8);
        *reinterpret_cast<uint32_t*>(p + 0xc) = *reinterpret_cast<const uint32_t*>(s + 0xc);
    }
    return dst;
}

// ---------------------------------------------------------------------------
// 0x180051650 — the 0x40-byte `Audio::EffectSlot` element copy-assign.
//
// array.cpp:427's `Assign40Call` reaches this address; it is the last of lane
// Q's five "new rows" still on the DLL together with 0x180004220.
//
//   dst[+0x00..+0x0f] = src, as FOUR dword MOVs
//   dst[+0x10] = src[+0x10]   (QWORD)
//   dst[+0x18] = src[+0x18]   (QWORD)
//   dst[+0x20], [+0x24], [+0x28] = src, as THREE dword MOVs
//   dst[+0x30] = 0 ; dst[+0x38] = 0
//   Array<T16>::operator=(dst+0x30, src+0x30)      ; 0x180050d30
//   return dst
//
// *** BYTES +0x2c .. +0x2f ARE NEVER WRITTEN. *** The scalar block stops at
// +0x2b and the nested Array starts at +0x30. array.cpp:437 records the same
// hole from the `else` arm's side ("BYTES +0x2c .. +0x2f ARE NEVER WRITTEN"),
// which is an independent second reading of it. Any oracle over this element
// must exclude that window or it fails 100% of calls
// (reference_oracle_must_not_compare_unwritten_bytes.md).
//
// The nested Array header is ZEROED BEFORE the operator= runs, and that is
// load-bearing: on the realloc arm the destination is raw MemAlloc memory and
// `operator=` starts by destroying it.
// ---------------------------------------------------------------------------
void* EffectSlotAssign(void* dstRaw, const void* srcRaw) {
    uint8_t*       d = static_cast<uint8_t*>(dstRaw);
    const uint8_t* s = static_cast<const uint8_t*>(srcRaw);

    *reinterpret_cast<uint32_t*>(d + 0x00) = *reinterpret_cast<const uint32_t*>(s + 0x00);
    *reinterpret_cast<uint32_t*>(d + 0x04) = *reinterpret_cast<const uint32_t*>(s + 0x04);
    *reinterpret_cast<uint32_t*>(d + 0x08) = *reinterpret_cast<const uint32_t*>(s + 0x08);
    *reinterpret_cast<uint32_t*>(d + 0x0c) = *reinterpret_cast<const uint32_t*>(s + 0x0c);
    *reinterpret_cast<uint64_t*>(d + 0x10) = *reinterpret_cast<const uint64_t*>(s + 0x10);
    *reinterpret_cast<uint64_t*>(d + 0x18) = *reinterpret_cast<const uint64_t*>(s + 0x18);
    *reinterpret_cast<uint32_t*>(d + 0x20) = *reinterpret_cast<const uint32_t*>(s + 0x20);
    *reinterpret_cast<uint32_t*>(d + 0x24) = *reinterpret_cast<const uint32_t*>(s + 0x24);
    *reinterpret_cast<uint32_t*>(d + 0x28) = *reinterpret_cast<const uint32_t*>(s + 0x28);
    /* +0x2c .. +0x2f deliberately untouched — retail does not write them */
    *reinterpret_cast<uint64_t*>(d + 0x30) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x38) = 0;

    // Through the port-aware address door, so the 0x180050d30 registration
    // below is what runs and the census can see the edge.
    using AssignFn = void* (*)(void*, const void*);
    static AssignFn assign = nullptr;
    if (!assign)
        assign = reinterpret_cast<AssignFn>(ResolveAddr(Module::Runtime, 0x180050d30));
    assign(d + 0x30, s + 0x30);

    return d;
}

}  // namespace rtsort

// ============================================================================
// Registration. ADDRESS DOORS ONLY — none of the three is exported.
// ============================================================================
int RtCollectionSortRegister() {
    int n = 0;
    RegisterRuntimeAddr(0x1800264a0, (void*)&rtsort::SortByKey70);      ++n;
    RegisterRuntimeAddr(0x180050d30, (void*)&rtsort::Array16Assign);    ++n;
    RegisterRuntimeAddr(0x180051650, (void*)&rtsort::EffectSlotAssign); ++n;
    return n;
}
