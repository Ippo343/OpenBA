// ============================================================================
// tests/unit/char_prof_test.cpp -- OFFSCREEN: the [COMPPROF] /char window
// ============================================================================
// src/diag/char_prof.h is what the stress harness turns into the /char rows: the class
// rule (MoveTo first, then a timed wait, else other; an empty stack is idle), the
// per-cell accumulation, the per-node table, and the skippable bound arithmetic.
// ============================================================================
#include "unit.h"
#include "char_prof.h"

#include <cmath>

namespace {
bool Near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }

CharProf::Chain MakeChain(bool empty, bool moveTo, int pathIndex, bool timed) {
    CharProf::Chain c;
    c.empty = empty; c.hasMoveTo = moveTo; c.moveToPathIndex = pathIndex; c.hasTimed = timed;
    return c;
}
}  // namespace

UNIT_TEST(char_prof_classify_rule) {
    using namespace CharProf;
    CHECK_EQ(Classify(MakeChain(true, false, -1, false)), (int)kIdle);
    // An empty stack is idle whatever else the reader filled in.
    CHECK_EQ(Classify(MakeChain(true, true, 3, true)), (int)kIdle);
    CHECK_EQ(Classify(MakeChain(false, true, 0, false)), (int)kWalk);
    CHECK_EQ(Classify(MakeChain(false, true, 17, false)), (int)kWalk);
    CHECK_EQ(Classify(MakeChain(false, true, -1, false)), (int)kPathWait);
    // MoveTo decides before a timed node riding in the same chain.
    CHECK_EQ(Classify(MakeChain(false, true, 2, true)), (int)kWalk);
    CHECK_EQ(Classify(MakeChain(false, true, -1, true)), (int)kPathWait);
    CHECK_EQ(Classify(MakeChain(false, false, -1, true)), (int)kTimed);
    CHECK_EQ(Classify(MakeChain(false, false, -1, false)), (int)kOther);
    CHECK_STREQ(ClassName(kWalk), "walk");
    CHECK_STREQ(ClassName(kPathWait), "pathwait");
    CHECK_STREQ(ClassName(kTimed), "timed");
    CHECK_STREQ(ClassName(99), "?");
}

UNIT_TEST(char_prof_window_cells) {
    using namespace CharProf;
    Window w;
    w.AddChar(kOff, kWalk, 100, false, true, false, false);
    w.AddChar(kOff, kWalk, 300, true, true, false, true);
    w.AddChar(kOn, kTimed, 50, true, false, true, false);
    w.AddModel(kOff, kWalk, 20);
    w.AddModel(kOff, kWalk, 22);
    w.AddModel(kOn, kTimed, 7);
    const Cell& ow = w.cell[kOff][kWalk];
    CHECK_EQ(ow.chars, 2LL);
    CHECK_EQ(ow.ticks, 400LL);
    CHECK_EQ(ow.lastCalls, 1LL);
    CHECK_EQ(ow.lastTicks, 300LL);
    CHECK_EQ(ow.moved, 2LL);
    CHECK_EQ(ow.still, 0LL);
    CHECK_EQ(ow.transitions, 1LL);
    CHECK_EQ(ow.modelCalls, 2LL);
    CHECK_EQ(ow.modelTicks, 42LL);
    const Cell& nt = w.cell[kOn][kTimed];
    CHECK_EQ(nt.chars, 1LL);
    CHECK_EQ(nt.still, 1LL);
    CHECK_EQ(nt.lastStill, 1LL);
    CHECK_EQ(nt.stillTicks, 50LL);
    CHECK_EQ(ow.stillTicks, 0LL);
    CHECK_EQ(w.StillTicks(kAllClasses, kBothVis), 50LL);
    CHECK_EQ(w.StillTicks(kAllClasses, kOffOnly), 0LL);
    CHECK_EQ(ow.lastStill, 0LL);
    CHECK_EQ(nt.modelTicks, 7LL);

    CHECK_EQ(w.CharTicks(kAllClasses, kBothVis), 450LL);
    CHECK_EQ(w.CharTicks(kWalkClasses, kOffOnly), 400LL);
    CHECK_EQ(w.CharTicks(kTimedClasses, kOffOnly), 0LL);
    CHECK_EQ(w.ModelTicks(kAllClasses, kBothVis), 49LL);
    CHECK_EQ(w.Chars(kWalkClasses | kTimedClasses, kBothVis), 3LL);

    w.Reset();
    CHECK_EQ(w.Chars(kAllClasses, kBothVis), 0LL);
    CHECK_EQ(w.nNodes, 0);
}

UNIT_TEST(char_prof_bound_adds_model_and_walk) {
    using namespace CharProf;
    Window w;
    w.AddChar(kOff, kWalk, 1000, false, true, false, false);
    w.AddChar(kOff, kPathWait, 200, false, false, true, false);
    w.AddChar(kOn, kWalk, 5000, false, true, false, false);   // visible: not in the off bound
    w.AddModel(kOff, kWalk, 100);
    // off-screen walkers: 1000 + 200 chars, 100 model, 2 visits x 10 walk ticks
    CHECK(Near(BoundTicks(w, kWalkClasses, kOffOnly, 10.0), 1320.0));
    // every walker, as if all were off-screen: + the visible one (5000 + 10)
    CHECK(Near(BoundTicks(w, kWalkClasses, kBothVis, 10.0), 6330.0));
    CHECK(Near(BoundTicks(w, kTimedClasses, kBothVis, 10.0), 0.0));
}

UNIT_TEST(char_prof_node_table) {
    using namespace CharProf;
    Window w;
    w.AddNode(7, kOff, 10);
    w.AddNode(7, kOn, 30);
    w.AddNode(0, kOff, 5);   // the empty-stack row
    CHECK_EQ(w.nNodes, 2);
    CHECK_EQ(w.nodes[0].typeId, 7u);
    CHECK_EQ(w.nodes[0].chars, 2LL);
    CHECK_EQ(w.nodes[0].ticks, 40LL);
    CHECK_EQ(w.nodes[0].offChars, 1LL);
    CHECK_EQ(w.nodes[0].offTicks, 10LL);
    CHECK_EQ(w.nodes[1].typeId, 0u);
    // Past the table: counted as overflow, never written out of bounds.
    for (uint32_t t = 100; t < 100 + kMaxNodes; ++t) w.AddNode(t, kOn, 1);
    CHECK_EQ(w.nNodes, kMaxNodes);
    CHECK_EQ(w.nodeOverflow, 2LL);
}
