// code/Game/Dialog/tradeui_sort.cpp
// Wave A — the TradeUI table sorts: two introsort drivers, their two comparators,
// and their two small-range sorts. Six exe functions, one shared algorithm.
//
// The gate in ui_dialog_stub_factories.cpp names only the two DRIVERS
// (0x1400162d0 fn_IntroSort_ByName, 0x1400165d0 fn_IntroSort_ByComponent), which
// undersells the job: neither driver's comparator or small-range sort was ported
// either, and all four were unregistered. What the gate can see is not the same as
// what has to exist.
//
// ── ABI, taken from the DISASSEMBLY (the decompile is wrong about arity) ──────
// Ghidra gives these functions phantom trailing parameters, because MSVC reuses the
// caller's register-home slots as String locals and the decompiler reads those slots
// as incoming arguments (`local_res8`, `local_res10`, ... are home slots, not
// locals). Every signature below was read off the prologue instead:
//
//   0x1400162d0  ByName driver        RCX=array, EDX=count            — 2 params.
//                                     R8/R9 are never read. The existing call site
//                                     in tradeui.cpp passes 4 args; the extra two
//                                     are harmless on Win64 and are left alone.
//   0x1400165d0  ByComponent driver   RCX=array, EDX=count, R8=ctx    — 3 params,
//                                     with an entry guard `CMP EDX,1 ; JLE ret`.
//   0x140015ad0  ByName small sort    RCX=first, RDX=last             — 2 params.
//   0x140016920  ByComponent small    RCX=first, RDX=last, R8=ctx     — 3 params.
//   0x1400845d0  ByName comparator    RCX=this(UNUSED), RDX=a, R8=b   — a stateless
//                                     functor: RCX is homed at 0x1400845d0 and then
//                                     never read.
//   0x140084430  ByComponent compare  RCX=ctx, RDX=a, R8=b            — this one
//                                     really does use its context.
//
// ── Element layout (stride 0x18, confirmed by the 0x2aaaaaaaaaaaaaab magic
//    divisor for /24 in both drivers) ────────────────────────────────────────────
//   +0x00  uint64  key         (the BST lookup key used by the ByComponent compare)
//   +0x08  void*   descriptor  (+0x28 = string table, +0x30 = string name)
//   +0x10  uint64  payload     (opaque; copied by the swaps)
//
// ── Why one body covers both drivers ─────────────────────────────────────────
// 0x1400162d0 and 0x1400165d0 are the same generic introsort: median-of-3, an
// explicit 62-entry stack instead of recursion, recurse-on-the-smaller-side, and a
// cutoff to a max-selection sort at 8 elements or fewer. They differ only in which
// comparator and small-range sort they call. The partition is transcribed
// step-for-step rather than replaced with std::sort, because an introsort is
// UNSTABLE: for rows that compare equal the final order is a property of the exact
// partition sequence, and substituting a different sort would reorder equal-priced
// trade rows relative to retail. That is cosmetic, but it is not nothing, and it
// would be invisible in testing.
#include "ui/UserInterface.h"
#include "object/External.h"
#include "system/String.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "runtime/types/ui/String.h"       // ENDGAME B.3.6 UI callers (K1)
#include "runtime/types/ui/StringTable.h"  // ENDGAME B.3.6 UI callers (K1)
#include "Dialog/exe_bodies.h"

namespace {

constexpr int kStride = 0x18;               // bytes per element
typedef uint8_t* Elem;

inline Elem At(Elem base, int64_t i) { return base + i * kStride; }
inline int64_t Count(Elem first, Elem last) { return (last - first) / kStride; }

inline void Swap24(Elem a, Elem b) {
    uint64_t* x = reinterpret_cast<uint64_t*>(a);
    uint64_t* y = reinterpret_cast<uint64_t*>(b);
    uint64_t t0 = x[0], t1 = x[1], t2 = x[2];
    x[0] = y[0]; x[1] = y[1]; x[2] = y[2];
    y[0] = t0;   y[1] = t1;   y[2] = t2;
}

// ── The name key ─────────────────────────────────────────────────────────────
// Both comparators derive a System::String from an element the same way:
//   P = *(void**)(elem + 8)
//   UIString ctor(buf, stringTable = *(uint64*)(P + 0x28), stringName = P + 0x30)
//   -> UIString::operator System::String -> copy-construct into the caller's slot
// The intermediate System::String is destroyed immediately; the copy is what gets
// compared. A System::String is 8 bytes here (the three live slots in the retail
// frame are 8 bytes apart), which is why `uint64_t` is the right storage.
//
// Retail gives the UIString 40 bytes of frame (RSP+0x28..RSP+0x50). 64 is used here
// purely as headroom; over-reserving a stack buffer cannot change behaviour.
// Note retail never destroys the UIString itself — only the System::String temps.
// That is reproduced rather than "fixed": adding a dtor call retail does not make
// would be a behaviour change smuggled in under the heading of correctness.
bool BuildNameKey(const void* elem, System::String* outString) {
    const uint8_t* p = *reinterpret_cast<const uint8_t* const*>(
        reinterpret_cast<const uint8_t*>(elem) + 8);
    if (!p) { return false; }

    alignas(8) uint8_t uiBuf[64];
    void* ui = ::new (static_cast<void*>(uiBuf)) UI::String(
        ::Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(
            *reinterpret_cast<const uint64_t*>(p + 0x28))),
        *reinterpret_cast<const System::String*>(p + 0x30));  // B.3.6 bridge
    System::String tmp;
    ::new (static_cast<void*>(&tmp)) System::String(reinterpret_cast<const UI::String*>(ui)->operator System::String());  // B.3.6 bridge
    *outString = tmp;
    return true;
}

// 0x1400845d0 — ByName comparator. `ctx` is the ignored functor `this`.
bool NameLess(void* /*ctx*/, const void* a, const void* b) {
    System::String sa, sb;
    const bool okA = BuildNameKey(a, &sa);
    const bool okB = BuildNameKey(b, &sb);
    bool result = false;
    if (okA && okB) result = sa < sb;
    return result;
}

// The BST lookup both halves of the ByComponent comparator perform. Same node
// layout as the storage AVL ported in tradeui_leaves.cpp (+0x00 key, +0x08 value,
// +0x18 left, +0x20 right) and, importantly, the same UNSIGNED key comparison.
// Returns the low 32 bits of the node's value, or 0 when the key is absent.
int32_t TreeCountFor(void* ctx, uint64_t key) {
    if (!ctx) return 0;
    const uint8_t* obj = *reinterpret_cast<const uint8_t* const*>(ctx);
    if (!obj) return 0;
    const uint64_t* node = *reinterpret_cast<const uint64_t* const*>(obj + 0x60);
    while (node) {
        if (key < node[0])      node = reinterpret_cast<const uint64_t*>(node[3]);
        else if (node[0] < key) node = reinterpret_cast<const uint64_t*>(node[4]);
        else                    return static_cast<int32_t>(node[1]);
    }
    return 0;
}

// 0x140084430 — ByComponent comparator.
//
// ⚠ The name undersells it. This is not "compare by component"; it looks each
// element's key up in the context's tree, compares the resulting COUNTS DESCENDING,
// and falls back to name order only when the counts tie. Reading it as a plain
// component-identity compare would produce a table sorted by something else
// entirely while still looking sorted.
bool ComponentLess(void* ctx, const void* a, const void* b) {
    const int32_t ca = TreeCountFor(ctx, *reinterpret_cast<const uint64_t*>(a));
    const int32_t cb = TreeCountFor(ctx, *reinterpret_cast<const uint64_t*>(b));
    if (cb < ca) return true;                // higher count sorts first
    if (ca < cb) return false;
    return NameLess(nullptr, a, b);          // tie -> name ascending
}

typedef bool (*LessFn)(void* ctx, const void* a, const void* b);

// ── The small-range sort (0x140015ad0 / 0x140016920) ─────────────────────────
// Both are the SAME max-selection sort; 0x140016920 is merely 109 lines instead of
// 56 because its comparator got inlined. Each pass walks [first, last] tracking the
// running maximum and swaps it to `last`, then shrinks the range.
//
// The comparison is `less(runningMax, candidate)` — strict — so a candidate EQUAL to
// the running maximum does not displace it. That detail decides where equal elements
// land, so it is transcribed rather than tidied.
void SmallSort(Elem first, Elem last, void* ctx, LessFn less) {
    if (last <= first) return;               // retail: `CMP RDX,RCX ; JBE ret`
    for (; first < last; last -= kStride) {
        Elem best = first;
        Elem cur  = first;
        for (Elem next = cur + kStride; next <= last; next = cur + kStride) {
            if (less(ctx, best, next)) best = next;
            cur = next;
        }
        Swap24(best, last);
    }
}

// ── The introsort driver (0x1400162d0 / 0x1400165d0) ─────────────────────────
void IntroSort(Elem array, int count, void* ctx, LessFn less,
               void (*smallSort)(Elem, Elem, void*, LessFn)) {
    if (count < 2) return;

    // Retail's explicit recursion stack: 62 entries, NO overflow check. Depth is
    // bounded because the driver always continues on the SMALLER partition and
    // pushes the larger, so 62 frames covers any array that fits in memory.
    //
    // The `depth < 62` guards on the pushes below are standalone-only and cannot
    // fire for any reachable input (trade tables are tens of entries). They are the
    // difference between "leave a partition unsorted" and "smash the stack" if that
    // reasoning is ever wrong — a visibly wrong table beats silent corruption.
    Elem stackLo[62];
    Elem stackHi[62];
    int  depth = 0;

    Elem first = array;
    Elem last  = At(array, count - 1);

    for (;;) {
        while (Count(first, last) + 1 > 8) {
            const int64_t n = Count(first, last) + 1;
            Elem mid = At(first, n >> 1);

            // Median-of-3, in retail's exact order.
            if (less(ctx, mid,  first)) Swap24(first, mid);
            if (less(ctx, last, first)) Swap24(first, last);
            if (less(ctx, last, mid))   Swap24(mid,   last);

            Elem lo = first;
            Elem hi = last;
            for (;;) {
                // Scan up. When the bounded-by-mid scan runs off the end it falls
                // into the bounded-by-last scan, which increments lo AGAIN before
                // testing. That extra step is retail behaviour (the fallthrough to
                // LAB_140016442), not an off-by-one — it is what steps lo past the
                // pivot slot.
                bool unbounded = true;
                if (lo < mid) {
                    unbounded = false;
                    for (;;) {
                        lo += kStride;
                        if (mid <= lo)          { unbounded = true; break; }
                        if (less(ctx, mid, lo)) { break; }
                    }
                }
                if (unbounded) {
                    for (;;) {
                        lo += kStride;
                        if (last < lo)          break;
                        if (less(ctx, mid, lo)) break;
                    }
                }

                // Scan down, bounded by mid.
                Elem hiPrev = hi;
                for (;;) {
                    hiPrev = hi;
                    hi -= kStride;
                    if (hi <= mid)           break;
                    if (!less(ctx, mid, hi)) break;
                }

                if (lo <= hi) {
                    Swap24(lo, hi);
                    if (mid == hi) mid = lo;  // pivot moved with the swap
                    continue;
                }

                // lo crossed hi. Walk hiPrev back over the run of elements EQUAL to
                // the pivot — "equal" meaning neither less(x,mid) nor less(mid,x).
                bool unbounded2 = true;
                if (mid < hiPrev) {
                    unbounded2 = false;
                    for (;;) {
                        hiPrev -= kStride;
                        if (hiPrev <= mid) { unbounded2 = true; break; }
                        if (less(ctx, hiPrev, mid) || less(ctx, mid, hiPrev)) break;
                    }
                }
                if (unbounded2) {
                    for (;;) {
                        hiPrev -= kStride;
                        if (hiPrev <= first)            break;
                        if (less(ctx, hiPrev, mid))     break;
                        if (less(ctx, mid, hiPrev))     break;
                    }
                }

                // Continue on the smaller side, push the larger.
                const int64_t rightCount = Count(lo, last);
                const int64_t leftCount  = Count(first, hiPrev);
                if (rightCount <= leftCount) {
                    if (first < hiPrev && depth < 62) {
                        stackLo[depth] = first; stackHi[depth] = hiPrev; ++depth;
                    }
                    first = lo;
                } else {
                    if (lo < last && depth < 62) {
                        stackLo[depth] = lo; stackHi[depth] = last; ++depth;
                    }
                    last = hiPrev;
                }
                break;
            }

            if (last <= first) break;
        }

        if (Count(first, last) + 1 <= 8) smallSort(first, last, ctx, less);

        if (depth == 0) return;
        --depth;
        first = stackLo[depth];
        last  = stackHi[depth];
    }
}

} // namespace

// ── Registered entry points ──────────────────────────────────────────────────

// 0x1400162d0 — sort the trade table by name.
// Signature keeps the two phantom trailing parameters because the existing call
// site in tradeui.cpp passes them. They are ignored, exactly as retail ignores
// R8/R9 here.
extern "C" void TradeUI_IntroSort_ByName_162d0(void* array, int count,
                                               void* /*phantomCtx*/,
                                               int64_t /*phantom4*/) {
    if (!array || count < 2) return;
    IntroSort(static_cast<Elem>(array), count, nullptr, &NameLess, &SmallSort);
}

// 0x1400165d0 — sort the trade table by stocked count (descending), name on ties.
extern "C" void TradeUI_IntroSort_ByComponent_165d0(void* array, int count,
                                                    void* ctx) {
    if (!array || count < 2) return;         // retail: `CMP EDX,1 ; JLE ret`
    IntroSort(static_cast<Elem>(array), count, ctx, &ComponentLess, &SmallSort);
}

// The BANISHED_TRADE_SORT_SELFTEST introsort self-test was deleted with the flag
// (ENDGAME C). standalone/reg_tradeui_leaves.cpp still calls it; the stub goes with it.

