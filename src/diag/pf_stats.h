// src/diag/pf_stats.h -- the pathfinding benchmark's per-window aggregation (PFBENCH).
//
// Pure data and arithmetic, no clock, no lock, no game type: the stress harness
// (stress_harness.cpp) feeds one PfStats::Request per completed path request (the
// worker's command 5) and one PfStats::Shadow per shadow comparison, and turns the
// window into the CSV's pf_* columns with Summarize() at each report row. Header-only
// so tests/unit/pf_stats_test.cpp checks the same code the harness runs.
//
// Times are collected in raw clock TICKS (the harness's RDTSC) and converted once, in
// Summarize(), with the harness's microseconds-per-tick: the worker thread never does
// the conversion and never reads the calibration.
//
// Percentiles are the harness's existing rule (stress_harness.cpp Pct): the element at
// index floor((n - 1) * p) of the samples, so p50 of {1,2,3,4} is 2 and p95 of 20
// samples is the 19th smallest. Samples beyond kMaxSamples in one window still count in
// every total, mean, max and fail column; only the percentiles see the first kMaxSamples
// (a stress-town window holds a few thousand requests).
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace PfStats {

constexpr size_t kMaxSamples = 1u << 17;

// One completed command 5, as the worker saw it.
struct Request {
    long long searchTicks;   // the search body (grid.Run, or the extension's)
    long long waitTicks;     // enqueue -> start of search; < 0 = not stamped
    int       expanded;      // nodes expanded, read from the search's own state after the run
    int       cells;         // cells in the result path; 0 = no path (a failure)
};

// One shadow comparison (ext.fast_paths's shadow mode; StressHarness_PathShadow).
struct Shadow {
    long long retailTicks, extTicks;
    int       retailExpanded, extExpanded;
    long long retailCost;   // sum of step costs along retail's cells
    long long extCost;      // the same along the extension's cells
    int       sameCells;    // 1 = the two paths are cell-for-cell identical
};

// The nearest-rank percentile the harness uses everywhere (index floor((n-1)*p)).
// Reorders `v`. 0 for an empty vector.
template <class T>
inline double Percentile(std::vector<T>& v, double p) {
    if (v.empty()) return 0.0;
    if (p < 0.0) p = 0.0;
    if (p > 1.0) p = 1.0;
    const size_t k = static_cast<size_t>(static_cast<double>(v.size() - 1) * p);
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k), v.end());
    return static_cast<double>(v[k]);
}

// One report window of worker-side samples. The harness keeps one under its lock and
// swaps it out at the report row.
struct Window {
    std::vector<long long> searchTicks;
    std::vector<long long> waitTicks;
    std::vector<int>       expanded;
    long long requests = 0, fails = 0, cellsSum = 0, expandedSum = 0;
    long long busyTicks = 0, searchMaxTicks = 0;
    long long shadowN = 0, shadowEqualCost = 0, shadowSameCells = 0, shadowWorse = 0;
    long long shadowRetailTicks = 0, shadowExtTicks = 0;

    void Add(const Request& r) {
        ++requests;
        if (r.cells <= 0) ++fails;
        cellsSum    += r.cells > 0 ? r.cells : 0;
        expandedSum += r.expanded > 0 ? r.expanded : 0;
        busyTicks   += r.searchTicks > 0 ? r.searchTicks : 0;
        if (r.searchTicks > searchMaxTicks) searchMaxTicks = r.searchTicks;
        if (searchTicks.size() < kMaxSamples) {
            searchTicks.push_back(r.searchTicks);
            expanded.push_back(r.expanded);
        }
        if (r.waitTicks >= 0 && waitTicks.size() < kMaxSamples) waitTicks.push_back(r.waitTicks);
    }
    void AddShadow(const Shadow& s) {
        ++shadowN;
        if (s.extCost == s.retailCost) ++shadowEqualCost;
        if (s.sameCells) ++shadowSameCells;
        if (s.extCost > s.retailCost) ++shadowWorse;
        shadowRetailTicks += s.retailTicks > 0 ? s.retailTicks : 0;
        shadowExtTicks    += s.extTicks > 0 ? s.extTicks : 0;
    }
};

// The main thread's own window: end-to-end latency (enqueue -> TakePath), the deepest
// queue seen at an enqueue, and the three main-thread brackets.
struct MainWindow {
    std::vector<long long> e2eTicks;
    // E2E SPLIT: enqueue -> ready (queue wait + search + publish) and ready -> TakePath (the
    // walker's poll), plus how many Update frames the request crossed (0, 1, 2, 3+).
    std::vector<long long> readyTicks, pollTicks;
    long long frames[4] = {0, 0, 0, 0};
    int       queueMax = 0;
    long long pathDistanceTicks = 0, fillZonesTicks = 0, syncWaitTicks = 0;

    void Enqueued(int depth) { if (depth > queueMax) queueMax = depth; }
    void Taken(long long ticks) { if (e2eTicks.size() < kMaxSamples) e2eTicks.push_back(ticks); }
    void TakenSplit(long long toReady, long long toTaken, int framesCrossed) {
        if (toReady >= 0 && readyTicks.size() < kMaxSamples) readyTicks.push_back(toReady);
        if (toTaken >= 0 && pollTicks.size() < kMaxSamples) pollTicks.push_back(toTaken);
        if (framesCrossed < 0) framesCrossed = 0;
        ++frames[framesCrossed > 3 ? 3 : framesCrossed];
    }
};

// The CSV row. Field names are the column names without the pf_ / ph_ prefix.
struct Summary {
    long long requests = 0;
    double reqPerSimS = 0;
    double searchUsP50 = 0, searchUsP95 = 0, searchUsMax = 0;
    double expandedMean = 0, expandedP95 = 0;
    double waitUsP50 = 0, waitUsP95 = 0;
    int    queueMax = 0;
    long long fail = 0;
    double workerBusyPct = 0;
    double lenMean = 0;            // over the requests that found a path
    double e2eUsP50 = 0, e2eUsP95 = 0;
    double readyUsP50 = 0, readyUsP95 = 0;   // enqueue -> result published
    double pollUsP50 = 0, pollUsP95 = 0;     // published -> TakePath (the walker's poll)
    double frames0Pct = 0, frames1Pct = 0, frames2Pct = 0, frames3pPct = 0;   // Update frames crossed
    long long shadowN = 0;
    double shadowEqualCostPct = 0, shadowSameCellsPct = 0;
    long long shadowWorse = 0;
    double shadowSpeedup = 0;      // retail ticks / extension ticks over the window
    double syncWaitMs = 0, pathDistanceMs = 0, fillZonesMs = 0;   // per Update frame
};

// usPerTick: the clock's microseconds per tick. windowWallS: wall seconds the window
// covered (worker busy %). simS: simulated seconds in it (requests per sim second).
// frames: Update calls in it (the per-frame ms columns). Reorders the sample vectors.
inline Summary Summarize(Window& w, MainWindow& m, double usPerTick, double windowWallS,
                         double simS, int frames) {
    Summary s;
    s.requests     = w.requests;
    s.reqPerSimS   = simS > 0.0 ? static_cast<double>(w.requests) / simS : 0.0;
    s.searchUsP50  = Percentile(w.searchTicks, 0.50) * usPerTick;
    s.searchUsP95  = Percentile(w.searchTicks, 0.95) * usPerTick;
    s.searchUsMax  = static_cast<double>(w.searchMaxTicks) * usPerTick;
    s.expandedMean = w.requests ? static_cast<double>(w.expandedSum) / static_cast<double>(w.requests) : 0.0;
    s.expandedP95  = Percentile(w.expanded, 0.95);
    s.waitUsP50    = Percentile(w.waitTicks, 0.50) * usPerTick;
    s.waitUsP95    = Percentile(w.waitTicks, 0.95) * usPerTick;
    s.queueMax     = m.queueMax;
    s.fail         = w.fails;
    s.workerBusyPct = windowWallS > 0.0
        ? 100.0 * (static_cast<double>(w.busyTicks) * usPerTick / 1e6) / windowWallS : 0.0;
    const long long found = w.requests - w.fails;
    s.lenMean      = found > 0 ? static_cast<double>(w.cellsSum) / static_cast<double>(found) : 0.0;
    s.e2eUsP50     = Percentile(m.e2eTicks, 0.50) * usPerTick;
    s.e2eUsP95     = Percentile(m.e2eTicks, 0.95) * usPerTick;
    s.readyUsP50   = Percentile(m.readyTicks, 0.50) * usPerTick;
    s.readyUsP95   = Percentile(m.readyTicks, 0.95) * usPerTick;
    s.pollUsP50    = Percentile(m.pollTicks, 0.50) * usPerTick;
    s.pollUsP95    = Percentile(m.pollTicks, 0.95) * usPerTick;
    {
        const long long fn = m.frames[0] + m.frames[1] + m.frames[2] + m.frames[3];
        if (fn > 0) {
            const double d = static_cast<double>(fn);
            s.frames0Pct  = 100.0 * static_cast<double>(m.frames[0]) / d;
            s.frames1Pct  = 100.0 * static_cast<double>(m.frames[1]) / d;
            s.frames2Pct  = 100.0 * static_cast<double>(m.frames[2]) / d;
            s.frames3pPct = 100.0 * static_cast<double>(m.frames[3]) / d;
        }
    }
    s.shadowN      = w.shadowN;
    if (w.shadowN) {
        const double n = static_cast<double>(w.shadowN);
        s.shadowEqualCostPct = 100.0 * static_cast<double>(w.shadowEqualCost) / n;
        s.shadowSameCellsPct = 100.0 * static_cast<double>(w.shadowSameCells) / n;
    }
    s.shadowWorse  = w.shadowWorse;
    s.shadowSpeedup = w.shadowExtTicks > 0
        ? static_cast<double>(w.shadowRetailTicks) / static_cast<double>(w.shadowExtTicks) : 0.0;
    const double perFrameMs = usPerTick / 1000.0 / (frames > 0 ? static_cast<double>(frames) : 1.0);
    s.syncWaitMs     = static_cast<double>(m.syncWaitTicks) * perFrameMs;
    s.pathDistanceMs = static_cast<double>(m.pathDistanceTicks) * perFrameMs;
    s.fillZonesMs    = static_cast<double>(m.fillZonesTicks) * perFrameMs;
    return s;
}

}  // namespace PfStats
