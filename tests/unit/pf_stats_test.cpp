// ============================================================================
// tests/unit/pf_stats_test.cpp -- PFBENCH: the pathfinding benchmark's window math
// ============================================================================
// src/diag/pf_stats.h is what the stress harness turns into the pf_* CSV columns:
// the nearest-rank percentile (index floor((n-1)*p), the harness's Pct rule), the
// per-request window, the main-thread window, the shadow tallies, and the unit
// conversion (ticks -> us, ms per frame, busy %, requests per sim second).
// ============================================================================
#include "unit.h"
#include "pf_stats.h"

#include <cmath>
#include <vector>

namespace {

bool Near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }

PfStats::Request Req(long long search, long long wait, int expanded, int cells) {
    PfStats::Request r;
    r.searchTicks = search; r.waitTicks = wait; r.expanded = expanded; r.cells = cells;
    return r;
}

}  // namespace

UNIT_TEST(pf_stats_percentile_nearest_rank) {
    std::vector<int> empty;
    CHECK(Near(PfStats::Percentile(empty, 0.5), 0.0));

    std::vector<int> one{7};
    CHECK(Near(PfStats::Percentile(one, 0.0), 7.0));
    CHECK(Near(PfStats::Percentile(one, 0.95), 7.0));
    CHECK(Near(PfStats::Percentile(one, 1.0), 7.0));

    // floor((4-1)*0.5) = 1 -> the 2nd smallest, whatever the input order.
    std::vector<int> four{4, 1, 3, 2};
    CHECK(Near(PfStats::Percentile(four, 0.5), 2.0));
    std::vector<int> four2{4, 1, 3, 2};
    CHECK(Near(PfStats::Percentile(four2, 1.0), 4.0));
    std::vector<int> four3{4, 1, 3, 2};
    CHECK(Near(PfStats::Percentile(four3, 0.0), 1.0));

    // 1..20 shuffled: p95 -> index floor(19*0.95) = 18 -> 19; p50 -> index 9 -> 10.
    std::vector<long long> twenty{13, 2, 20, 7, 1, 18, 5, 11, 16, 3, 9, 14, 19, 4, 8, 12, 17, 6, 10, 15};
    CHECK(Near(PfStats::Percentile(twenty, 0.95), 19.0));
    CHECK(Near(PfStats::Percentile(twenty, 0.50), 10.0));

    // Out-of-range p clamps instead of indexing past the end.
    std::vector<int> three{5, 6, 7};
    CHECK(Near(PfStats::Percentile(three, 1.5), 7.0));
    CHECK(Near(PfStats::Percentile(three, -1.0), 5.0));
}

UNIT_TEST(pf_stats_window_totals) {
    PfStats::Window w;
    w.Add(Req(100, 10, 50, 20));
    w.Add(Req(300, 30, 150, 40));
    w.Add(Req(200, -1, 400, 0));   // a failure, and no wait stamp
    CHECK_EQ(w.requests, 3LL);
    CHECK_EQ(w.fails, 1LL);
    CHECK_EQ(w.cellsSum, 60LL);
    CHECK_EQ(w.expandedSum, 600LL);
    CHECK_EQ(w.busyTicks, 600LL);
    CHECK_EQ(w.searchMaxTicks, 300LL);
    CHECK_EQ(w.searchTicks.size(), (size_t)3);
    CHECK_EQ(w.expanded.size(), (size_t)3);
    CHECK_EQ(w.waitTicks.size(), (size_t)2);   // the unstamped request has no wait sample
}

UNIT_TEST(pf_stats_summarize_units) {
    PfStats::Window w;
    w.Add(Req(100, 10, 50, 20));
    w.Add(Req(300, 30, 150, 40));
    w.Add(Req(200, -1, 400, 0));
    PfStats::MainWindow m;
    m.Enqueued(3); m.Enqueued(9); m.Enqueued(4);
    m.Taken(1000); m.Taken(3000);
    m.pathDistanceTicks = 20000;
    m.fillZonesTicks    = 4000;
    m.syncWaitTicks     = 0;

    // 2 us per tick, a 0.5 s window, 6 simulated seconds, 10 Update frames.
    const PfStats::Summary s = PfStats::Summarize(w, m, 2.0, 0.5, 6.0, 10);
    CHECK_EQ(s.requests, 3LL);
    CHECK(Near(s.reqPerSimS, 0.5));
    CHECK(Near(s.searchUsP50, 400.0));       // ticks {100,200,300}: p50 = 200 -> 400 us
    CHECK(Near(s.searchUsP95, 400.0));       // floor(2*0.95) = 1 -> 200 ticks
    CHECK(Near(s.searchUsMax, 600.0));
    CHECK(Near(s.expandedMean, 200.0));
    CHECK(Near(s.expandedP95, 150.0));       // {50,150,400}: index 1
    CHECK(Near(s.waitUsP50, 20.0));          // {10,30}: index 0 -> 10 ticks
    CHECK(Near(s.waitUsP95, 20.0));          // floor(1*0.95) = 0
    CHECK_EQ(s.queueMax, 9);
    CHECK_EQ(s.fail, 1LL);
    // busy: 600 ticks * 2 us = 1200 us = 0.0012 s of 0.5 s = 0.24 %.
    CHECK(Near(s.workerBusyPct, 0.24, 1e-12));
    CHECK(Near(s.lenMean, 30.0));            // (20 + 40) / the two that found a path
    CHECK(Near(s.e2eUsP50, 2000.0));
    CHECK(Near(s.e2eUsP95, 2000.0));
    // per frame: 20000 ticks * 2 us = 40 ms over 10 frames = 4 ms; 4000 -> 0.8 ms.
    CHECK(Near(s.pathDistanceMs, 4.0));
    CHECK(Near(s.fillZonesMs, 0.8));
    CHECK(Near(s.syncWaitMs, 0.0));
    // No shadow calls: the reserved columns are 0.
    CHECK_EQ(s.shadowN, 0LL);
    CHECK(Near(s.shadowEqualCostPct, 0.0));
    CHECK(Near(s.shadowSameCellsPct, 0.0));
    CHECK_EQ(s.shadowWorse, 0LL);
    CHECK(Near(s.shadowSpeedup, 0.0));
}

UNIT_TEST(pf_stats_empty_window_is_all_zero) {
    PfStats::Window w;
    PfStats::MainWindow m;
    const PfStats::Summary s = PfStats::Summarize(w, m, 0.3, 0.0, 0.0, 0);
    CHECK_EQ(s.requests, 0LL);
    CHECK(Near(s.reqPerSimS, 0.0));
    CHECK(Near(s.searchUsP95, 0.0));
    CHECK(Near(s.expandedMean, 0.0));
    CHECK(Near(s.workerBusyPct, 0.0));
    CHECK(Near(s.lenMean, 0.0));
    CHECK_EQ(s.queueMax, 0);
    CHECK(Near(s.pathDistanceMs, 0.0));
}

UNIT_TEST(pf_stats_shadow_tallies) {
    PfStats::Window w;
    PfStats::Shadow sh{};
    sh.retailTicks = 800; sh.extTicks = 100; sh.retailCost = 400; sh.extCost = 400; sh.sameCells = 1;
    w.AddShadow(sh);                          // equal cost, same cells
    sh.retailTicks = 400; sh.extTicks = 100; sh.retailCost = 400; sh.extCost = 400; sh.sameCells = 0;
    w.AddShadow(sh);                          // equal cost, different cells
    sh.retailTicks = 0;   sh.extTicks = 0;   sh.retailCost = 400; sh.extCost = 410; sh.sameCells = 0;
    w.AddShadow(sh);                          // the extension's path costs MORE: a bug
    sh.retailCost = 400; sh.extCost = 390; sh.sameCells = 0;
    w.AddShadow(sh);                          // cheaper than retail: not "worse", not equal
    PfStats::MainWindow m;
    const PfStats::Summary s = PfStats::Summarize(w, m, 1.0, 1.0, 1.0, 1);
    CHECK_EQ(s.shadowN, 4LL);
    CHECK(Near(s.shadowEqualCostPct, 50.0));
    CHECK(Near(s.shadowSameCellsPct, 25.0));
    CHECK_EQ(s.shadowWorse, 1LL);
    CHECK(Near(s.shadowSpeedup, 6.0));       // (800 + 400) / (100 + 100)
}

UNIT_TEST(pf_stats_windows_are_independent) {
    // The harness swaps the live window out at each report row; the next window starts
    // empty and the swapped-out one keeps its own samples.
    PfStats::Window live;
    live.Add(Req(500, 5, 10, 3));
    PfStats::Window taken;
    std::swap(taken, live);
    live.Add(Req(100, 1, 2, 1));
    CHECK_EQ(taken.requests, 1LL);
    CHECK_EQ(live.requests, 1LL);
    CHECK_EQ(taken.searchMaxTicks, 500LL);
    CHECK_EQ(live.searchMaxTicks, 100LL);
    PfStats::MainWindow m;
    const PfStats::Summary a = PfStats::Summarize(taken, m, 1.0, 1.0, 1.0, 1);
    const PfStats::Summary b = PfStats::Summarize(live, m, 1.0, 1.0, 1.0, 1);
    CHECK(Near(a.searchUsP50, 500.0));
    CHECK(Near(b.searchUsP50, 100.0));
}

UNIT_TEST(pf_stats_sample_cap_keeps_totals) {
    // Past kMaxSamples the percentiles stop growing their sample, but every total,
    // the max and the fail count still see every request.
    PfStats::Window w;
    const long long n = static_cast<long long>(PfStats::kMaxSamples) + 10;
    for (long long i = 0; i < n; ++i) w.Add(Req(1, 1, 1, 1));
    w.Add(Req(999, 1, 1, 0));
    CHECK_EQ(w.requests, n + 1);
    CHECK_EQ(w.fails, 1LL);
    CHECK_EQ(w.searchMaxTicks, 999LL);
    CHECK_EQ(w.searchTicks.size(), PfStats::kMaxSamples);
    CHECK_EQ(w.waitTicks.size(), PfStats::kMaxSamples);
    CHECK_EQ(w.busyTicks, n + 999);
}
