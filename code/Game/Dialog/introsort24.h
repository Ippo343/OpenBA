#pragma once
// code/Game/Dialog/introsort24.h
// The one introsort retail instantiates for its 0x18-byte UI table rows, as a
// template over the row type and its comparator (B6 row 39: the core that lived in
// tradeui_sort.cpp, lifted so the TownHall sorts use it too).
//
// Retail instances (all read in Ghidra; same body, different comparator):
//   driver      small sort   comparator   rows
//   0x1400162d0 0x140015ad0  0x1400845d0  TradeUI by name            (tradeui_sort.cpp)
//   0x1400165d0 0x140016920  0x140084430  TradeUI by stocked count   (tradeui_sort.cpp)
//   0x140015200 0x140015ad0  0x14007c710  TownHall inventory, name ascending
//   0x140015500 0x140015c40  0x14007c620  TownHall inventory, count descending, name on ties
// (0x140015ad0 is shared by the TradeUI name sort and the TownHall ascending sort.)
//
// The driver: median-of-3, an explicit 62-entry stack instead of recursion, continue
// on the smaller partition and push the larger, and a cutoff to a max-selection sort
// at 8 elements or fewer. The partition is transcribed step for step (an introsort
// is UNSTABLE: rows that compare equal end where the exact partition sequence puts
// them). A row swap moves the three qwords.

#include <cstdint>

namespace IntroSort24 {

template <class T>
inline void SwapRows(T* x, T* y) {
    static_assert(sizeof(T) == 0x18, "the introsort's rows are 0x18 bytes (the /24 magic divisor)");
    T t = *x;
    *x = *y;
    *y = t;
}

// The small-range sort (0x140015ad0 / 0x140016920 / 0x140015c40): each pass walks
// [first, last] tracking the running maximum and swaps it to `last`. The comparison
// is `less(runningMax, candidate)` -- strict -- so a candidate EQUAL to the running
// maximum does not displace it.
template <class T, class Less>
void SmallSort(T* first, T* last, Less less) {
    if (last <= first) return;               // retail: `CMP RDX,RCX ; JBE ret`
    for (; first < last; --last) {
        T* best = first;
        T* cur  = first;
        for (T* next = cur + 1; next <= last; next = cur + 1) {
            if (less(best, next)) best = next;
            cur = next;
        }
        SwapRows(best, last);
    }
}

// The driver (0x1400162d0 / 0x1400165d0 / 0x140015200 / 0x140015500).
template <class T, class Less>
void Sort(T* array, int count, Less less) {
    if (count < 2) return;

    // Retail's explicit recursion stack: 62 entries, NO overflow check. Depth is
    // bounded because the driver always continues on the SMALLER partition and
    // pushes the larger, so 62 frames covers any array that fits in memory.
    //
    // The `depth < 62` guards on the pushes below are standalone-only and cannot
    // fire for any reachable input (the tables are tens of rows). They are the
    // difference between "leave a partition unsorted" and "smash the stack" if that
    // reasoning is ever wrong -- a visibly wrong table beats silent corruption.
    T*  stackLo[62];
    T*  stackHi[62];
    int depth = 0;

    T* first = array;
    T* last  = array + (count - 1);

    for (;;) {
        while ((last - first) + 1 > 8) {
            const int64_t n = (last - first) + 1;
            T* mid = first + (n >> 1);

            // Median-of-3, in retail's exact order.
            if (less(mid,  first)) SwapRows(first, mid);
            if (less(last, first)) SwapRows(first, last);
            if (less(last, mid))   SwapRows(mid,   last);

            T* lo = first;
            T* hi = last;
            for (;;) {
                // Scan up. When the bounded-by-mid scan runs off the end it falls
                // into the bounded-by-last scan, which increments lo AGAIN before
                // testing. That extra step is retail behaviour (the fallthrough to
                // LAB_140016442), not an off-by-one -- it is what steps lo past the
                // pivot slot.
                bool unbounded = true;
                if (lo < mid) {
                    unbounded = false;
                    for (;;) {
                        ++lo;
                        if (mid <= lo)     { unbounded = true; break; }
                        if (less(mid, lo)) { break; }
                    }
                }
                if (unbounded) {
                    for (;;) {
                        ++lo;
                        if (last < lo)     break;
                        if (less(mid, lo)) break;
                    }
                }

                // Scan down, bounded by mid.
                T* hiPrev = hi;
                for (;;) {
                    hiPrev = hi;
                    --hi;
                    if (hi <= mid)      break;
                    if (!less(mid, hi)) break;
                }

                if (lo <= hi) {
                    SwapRows(lo, hi);
                    if (mid == hi) mid = lo;  // pivot moved with the swap
                    continue;
                }

                // lo crossed hi. Walk hiPrev back over the run of elements EQUAL to
                // the pivot -- "equal" meaning neither less(x,mid) nor less(mid,x).
                bool unbounded2 = true;
                if (mid < hiPrev) {
                    unbounded2 = false;
                    for (;;) {
                        --hiPrev;
                        if (hiPrev <= mid) { unbounded2 = true; break; }
                        if (less(hiPrev, mid) || less(mid, hiPrev)) break;
                    }
                }
                if (unbounded2) {
                    for (;;) {
                        --hiPrev;
                        if (hiPrev <= first)       break;
                        if (less(hiPrev, mid))     break;
                        if (less(mid, hiPrev))     break;
                    }
                }

                // Continue on the smaller side, push the larger.
                const int64_t rightCount = last - lo;
                const int64_t leftCount  = hiPrev - first;
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

        if ((last - first) + 1 <= 8) SmallSort(first, last, less);

        if (depth == 0) return;
        --depth;
        first = stackLo[depth];
        last  = stackHi[depth];
    }
}

}  // namespace IntroSort24
