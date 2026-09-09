// ============================================================================
// runtime/src/ui/element_aligned_probe.cpp — Wave 10
// An INPUT SWEEP for ?GetAlignedPosition@Element@UI@@SAX... (static, 0x1801158e0)
// ============================================================================
//
// WHY A SWEEP RATHER THAN WAITING FOR THE GAME.
//
// The port landed registered and correct-looking, and then reported `calls=0`
// in two separate runs: a menu run and a full in-game autostart that logged
// 16,915,090 diffed calls across 542 sites. Neither reached it. The member
// overload that forwards into it did not run either, and the one exe caller
// (code/Game/Component/frustumoutlineelement.cpp, the minimap frustum outline)
// needs the minimap open.
//
// A clean board from a port with no callers is not evidence
// (reference_uncalled_port_is_not_verified.md). The usual answer would be a
// detour, so retail's own layout callers arrive at us — but for a function like
// this one that is the WRONG tool, and reaching for it would have been reflex:
//
//   **This function is PURE.** No calls, no allocation, no globals, no `this`.
//   Every output is a function of seventeen scalar arguments. So we do not need
//   the game to produce inputs at all — we can produce them ourselves, feed the
//   SAME inputs to our body and to the SHIPPED export, and compare. That is a
//   stronger oracle than incidental gameplay coverage, not a weaker substitute
//   for it: gameplay would exercise whatever handful of alignments the current
//   dialogs happen to use, while a sweep covers every branch on purpose,
//   including the ones no dialog reaches today.
//
// WHAT IS SWEPT, and why each axis is there:
//
//   alignment 0..9   all nine cells of the row-major 3x3, PLUS 9, which is out
//                    of range for both the X jump table (0x180115a14, indexed
//                    by alignment-1 with `JA -> default`) and the Y compare
//                    chain. 9 must land on left/top from two independently
//                    encoded dispatches; that agreement is the evidence the
//                    enum mapping is right.
//   box vs container OVERSIZED boxes included on purpose. They drive the
//                    container clamp AND make `avail` NEGATIVE, which is the
//                    only region where the centring arithmetic can be got
//                    wrong (see the negative control).
//   pads             asymmetric, so a left/right or top/bottom swap shows up.
//   stretch          both bools, because they REWRITE *outW/*outH before the
//                    placement reads them back, and that ordering is load
//                    bearing.
//
// THE NEGATIVE CONTROL IS TARGETED, NOT RANDOM.
// BANISHED_RT_ALIGNED_PROBE_NEGCTL=1 swaps the arithmetic shift for a division:
// `avail / 2` instead of `avail >> 1`. Those are IDENTICAL for non-negative
// values and differ by one for odd negatives, because SAR rounds toward
// negative infinity and `/` rounds toward zero. That is precisely the trap a
// tidied port falls into, so the control asks the one question worth asking of
// this probe: *can it see the mistake I was most likely to make?* A random bit
// flip would have proved only that comparison works at all.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "../../funcs/rt_port.h"

// The ported body, shared with element.cpp so the probe cannot drift from the
// thing it verifies. `useDivide` selects the negative control's wrong variant.
void RtUi_Element_GetAlignedPositionStatic_Body(
        int alignment, int x, int y, int containerW, int containerH,
        int offsetX, int offsetY,
        int leftPad, int rightPad, int topPad, int bottomPad,
        int* outX, int* outY, int* outW, int* outH,
        bool stretchX, bool stretchY, bool useDivide);

namespace {

using StaticFn = void (*)(int, int, int, int, int, int, int,
                          int, int, int, int,
                          int*, int*, int*, int*, bool, bool);

const char* const kStatic =
    "?GetAlignedPosition@Element@UI@@SAXW4Alignment@ElementDescription@2@HHHHHHHHHHAEAH111_N2@Z";

struct Counters {
    int cases        = 0;
    int mismatches   = 0;
    int negAvail     = 0;   // cases with a negative available extent  (SAR region)
    int stretched    = 0;   // cases where the stretch actually rewrote a size
    int clamped      = 0;   // cases where the container clamp fired
    int perAlign[10] = {0};
};

void Report(const Counters& c, bool negctl, int shown) {
    std::fprintf(stderr,
        "[AlignedProbe] %s: %d case(s), %d mismatch(es)\n",
        negctl ? "NEGATIVE CONTROL (avail/2 instead of avail>>1)" : "sweep",
        c.cases, c.mismatches);
    std::fprintf(stderr,
        "[AlignedProbe]   discriminating coverage: %d case(s) with a NEGATIVE "
        "available extent (the only region where >>1 and /2 differ), %d that "
        "stretched, %d that hit the container clamp\n",
        c.negAvail, c.stretched, c.clamped);
    std::fprintf(stderr, "[AlignedProbe]   per-alignment cases:");
    for (int a = 0; a <= 9; a++) std::fprintf(stderr, " %d:%d", a, c.perAlign[a]);
    std::fprintf(stderr, "\n");

    if (negctl) {
        // The control's verdict, stated as a verdict rather than left to the
        // reader to infer from a number.
        if (c.mismatches == 0)
            std::fprintf(stderr,
                "[AlignedProbe]   *** CONTROL FAILED: the deliberately WRONG "
                "arithmetic was not caught. This probe cannot see the mistake it "
                "exists to rule out, so its clean runs are not evidence. ***\n");
        else
            std::fprintf(stderr,
                "[AlignedProbe]   control OK: the wrong arithmetic was caught in "
                "%d case(s) -- the sweep can discriminate >>1 from /2.\n",
                c.mismatches);
    } else if (c.mismatches == 0 && c.negAvail == 0) {
        std::fprintf(stderr,
            "[AlignedProbe]   *** WEAK: no case produced a negative available "
            "extent, so the shift arithmetic was never actually exercised. ***\n");
    }
    (void)shown;
    std::fflush(stderr);
}

} // namespace

void RtUiElementAlignedProbe() {
    if (!RtWaveEnabled("BANISHED_RT_ALIGNED_PROBE")) return;
    if (std::getenv("BANISHED_RT_ALIGNED_PROBE") == nullptr) return;

    StaticFn theirs = reinterpret_cast<StaticFn>(ResolveExportRaw(kStatic));
    if (!theirs) {
        std::fprintf(stderr, "[AlignedProbe] SKIPPED: the shipped export did not "
                             "resolve, so there is no authority to compare against "
                             "(this is the cutover value, not a pass)\n");
        std::fflush(stderr);
        return;
    }

    const bool negctl = RtWaveEnabled("BANISHED_RT_ALIGNED_PROBE_NEGCTL") &&
                        std::getenv("BANISHED_RT_ALIGNED_PROBE_NEGCTL") != nullptr;

    // Deliberately awkward values: asymmetric pads, an oversized box, odd and
    // even extents (parity is what makes >>1 and /2 differ), and negatives.
    static const int kSizes[]  = { 0, 1, 7, 8, 33, 200 };
    static const int kConts[]  = { 0, 5, 40, 41, 100 };
    static const int kOffs[]   = { -9, 0, 3 };
    static const int kPads[][4] = { {0,0,0,0}, {1,2,3,4}, {7,0,0,11}, {-3,5,-2,6} };

    Counters c;
    int shown = 0;

    for (int align = 0; align <= 9; align++)
    for (size_t si = 0; si < sizeof(kSizes)/sizeof(kSizes[0]); si++)
    for (size_t ci = 0; ci < sizeof(kConts)/sizeof(kConts[0]); ci++)
    for (size_t oi = 0; oi < sizeof(kOffs)/sizeof(kOffs[0]); oi++)
    for (size_t pi = 0; pi < sizeof(kPads)/sizeof(kPads[0]); pi++)
    for (int sx = 0; sx <= 1; sx++)
    for (int sy = 0; sy <= 1; sy++) {
        const int boxW = kSizes[si], boxH = kSizes[(si + 2) % 6];
        const int cw   = kConts[ci], ch   = kConts[(ci + 1) % 5];
        const int ox   = kOffs[oi],  oy   = kOffs[(oi + 1) % 3];
        const int lp = kPads[pi][0], rp = kPads[pi][1];
        const int tp = kPads[pi][2], bp = kPads[pi][3];
        const int x = 4, y = 6;

        int ax = 0, ay = 0, aw = boxW, ah = boxH;
        RtUi_Element_GetAlignedPositionStatic_Body(
            align, x, y, cw, ch, ox, oy, lp, rp, tp, bp,
            &ax, &ay, &aw, &ah, sx != 0, sy != 0, negctl);

        int bx = 0, by = 0, bw = boxW, bh = boxH;
        theirs(align, x, y, cw, ch, ox, oy, lp, rp, tp, bp,
               &bx, &by, &bw, &bh, sx != 0, sy != 0);

        c.cases++;
        c.perAlign[align]++;

        // Coverage bookkeeping, recomputed independently of the body so it
        // describes the CASE rather than trusting the thing under test.
        {
            int cx = x + ox, cy = y + oy;
            if (cw < boxW + cx) { cx = cw - boxW; c.clamped++; }
            if (ch < boxH + cy) { cy = ch - boxH; }
            cx += lp; cy += tp;
            const int availW = (cw - rp) - cx;
            const int availH = (ch - bp) - cy;
            if (availW < 0 || availH < 0) c.negAvail++;
            if ((sx && availW != boxW) || (sy && availH != boxH)) c.stretched++;
        }

        if (ax != bx || ay != by || aw != bw || ah != bh) {
            c.mismatches++;
            if (shown < 5) {
                shown++;
                std::fprintf(stderr,
                    "[AlignedProbe] MISMATCH align=%d box=%dx%d cont=%dx%d "
                    "off=(%d,%d) pad=(l%d r%d t%d b%d) stretch=(%d,%d)\n"
                    "[AlignedProbe]     ours  x=%d y=%d w=%d h=%d\n"
                    "[AlignedProbe]     theirs x=%d y=%d w=%d h=%d\n",
                    align, boxW, boxH, cw, ch, ox, oy, lp, rp, tp, bp, sx, sy,
                    ax, ay, aw, ah, bx, by, bw, bh);
            }
        }
    }

    Report(c, negctl, shown);
}
