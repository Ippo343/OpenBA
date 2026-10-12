// src/diag/char_prof.h -- OFFSCREEN: where a character's per-iteration tick goes, by what
// the character is doing and whether the camera can see it (BANISHED_STRESS_COMPPROF=2).
//
// The question (docs/extensions/handins/OFFSCREEN.md): how much of EntityList would an
// "off-screen citizens skip their walk / their work wait" extension remove? The /tree rows
// already say the CharacterComponent slot-8 tick (move tick + action executor) is ~49% of
// EntityList; these rows split that tick, and the citizen's own ModelComponent tick (the
// animation clock), by
//   visibility  on / off screen (PerfX's frustum test with its 6-unit margin, read whether
//               or not ext.render_culling is on)
//   class       what the top entry of the action stack is at the START of the tick:
//                 idle      the stack is empty (the executor does not run)
//                 walk      the chain holds a MoveTo that is traversing (pathIndex >= 0)
//                 pathwait  the chain holds a MoveTo with no path yet (pathIndex -1: the
//                           worker has not answered, or the precheck re-queries)
//                 timed     the chain holds a timer / clock wait: PlayAnimation, PerformWork
//                           (time->_ticks accumulated against a target) or
//                           AnimateNaturalResource (the resource model's IsAnimating)
//                 other     anything else (instantaneous nodes: pickup, drop-off, create,
//                           complete, message; HuntAnimal)
// and, per tick, whether the character MOVED (model translation changed), stayed bit-for-
// bit STILL (translation, rotation and yaw all unchanged: the move tick recomputed exactly
// what was there), or TRANSITIONED (the stack's current index or top chain changed).
//
// Pure data and arithmetic, no clock, no game type: the stress harness (stress_harness.cpp)
// reads the game objects and feeds the window; header-only so tests/unit/char_prof_test.cpp
// checks the same code the harness runs. Ticks are the harness's RDTSC ticks.
#pragma once
#include <cstdint>

namespace CharProf {

enum Class : int { kIdle = 0, kWalk, kPathWait, kTimed, kOther, kClassCount };
enum Vis : int { kOn = 0, kOff = 1, kVisCount };

inline const char* ClassName(int c) {
    switch (c) {
    case kIdle:     return "idle";
    case kWalk:     return "walk";
    case kPathWait: return "pathwait";
    case kTimed:    return "timed";
    case kOther:    return "other";
    default:        return "?";
    }
}

// What the harness read from the top chain of the action stack before the tick.
struct Chain {
    bool empty = true;           // stack current < 0 (or a null top entry)
    bool hasMoveTo = false;      // a MoveTo node anywhere in the chain
    int  moveToPathIndex = -1;   // that MoveTo's pathIndex (+0x60)
    bool hasTimed = false;       // a PlayAnimation / PerformWork / AnimateNaturalResource node
};

// The rule: MoveTo decides first (a chain that walks is a walker whatever rides with it),
// then a timed wait, else other.
inline int Classify(const Chain& c) {
    if (c.empty) return kIdle;
    if (c.hasMoveTo) return c.moveToPathIndex >= 0 ? kWalk : kPathWait;
    if (c.hasTimed) return kTimed;
    return kOther;
}

struct Cell {
    long long chars = 0;        // CharacterComponent ticks counted
    long long ticks = 0;        // their RDTSC ticks
    long long lastCalls = 0;    // of which on the last speed iteration (rootOnly clear)
    long long lastTicks = 0;
    long long moved = 0;        // translation changed in the tick
    long long still = 0;        // translation, rotation and yaw bit-identical after the tick
    long long lastStill = 0;    // of which on the last speed iteration (the terrain z re-read)
    long long stillTicks = 0;   // the RDTSC ticks of the still ticks (E1's ceiling)
    long long transitions = 0;  // stack index or top chain changed in the tick
    long long modelCalls = 0;   // the same characters' ModelComponent ticks
    long long modelTicks = 0;
};

// One row of the per-node table: the node type that decided the class (the MoveTo, the
// timed node) or else the chain head's type; typeId 0 = empty stack.
struct NodeRow {
    uint32_t typeId = 0;
    long long chars = 0, ticks = 0;
    long long offChars = 0, offTicks = 0;
};

constexpr int kMaxNodes = 48;

struct Window {
    Cell cell[kVisCount][kClassCount];
    NodeRow nodes[kMaxNodes];
    int nNodes = 0;
    long long nodeOverflow = 0;   // node types past kMaxNodes (counted in `cell`, not in `nodes`)

    void Reset() { *this = Window(); }

    void AddChar(int vis, int cls, long long t, bool last, bool moved, bool still, bool transition) {
        Cell& c = cell[vis][cls];
        ++c.chars; c.ticks += t;
        if (last) { ++c.lastCalls; c.lastTicks += t; }
        if (moved) ++c.moved;
        if (still) { ++c.still; c.stillTicks += t; if (last) ++c.lastStill; }
        if (transition) ++c.transitions;
    }
    void AddModel(int vis, int cls, long long t) {
        Cell& c = cell[vis][cls];
        ++c.modelCalls; c.modelTicks += t;
    }
    void AddNode(uint32_t typeId, int vis, long long t) {
        NodeRow* r = nullptr;
        for (int i = 0; i < nNodes; ++i) if (nodes[i].typeId == typeId) { r = &nodes[i]; break; }
        if (!r) {
            if (nNodes >= kMaxNodes) { ++nodeOverflow; return; }
            r = &nodes[nNodes++];
            r->typeId = typeId;
        }
        ++r->chars; r->ticks += t;
        if (vis == kOff) { ++r->offChars; r->offTicks += t; }
    }

    // Sums over a set of classes (bit c = class c) and visibilities (bit v = vis v).
    long long CharTicks(unsigned classes, unsigned vis) const {
        long long s = 0;
        for (int v = 0; v < kVisCount; ++v) if (vis & (1u << v))
            for (int c = 0; c < kClassCount; ++c) if (classes & (1u << c)) s += cell[v][c].ticks;
        return s;
    }
    long long ModelTicks(unsigned classes, unsigned vis) const {
        long long s = 0;
        for (int v = 0; v < kVisCount; ++v) if (vis & (1u << v))
            for (int c = 0; c < kClassCount; ++c) if (classes & (1u << c)) s += cell[v][c].modelTicks;
        return s;
    }
    long long StillTicks(unsigned classes, unsigned vis) const {
        long long s = 0;
        for (int v = 0; v < kVisCount; ++v) if (vis & (1u << v))
            for (int c = 0; c < kClassCount; ++c) if (classes & (1u << c)) s += cell[v][c].stillTicks;
        return s;
    }
    long long Chars(unsigned classes, unsigned vis) const {
        long long s = 0;
        for (int v = 0; v < kVisCount; ++v) if (vis & (1u << v))
            for (int c = 0; c < kClassCount; ++c) if (classes & (1u << c)) s += cell[v][c].chars;
        return s;
    }
};

constexpr unsigned kAllClasses = (1u << kClassCount) - 1;
constexpr unsigned kWalkClasses = (1u << kWalk) | (1u << kPathWait);
constexpr unsigned kTimedClasses = 1u << kTimed;
constexpr unsigned kOffOnly = 1u << kOff;
constexpr unsigned kBothVis = (1u << kOn) | (1u << kOff);

// The skippable bound in ticks: the character ticks + their model ticks of `classes` /
// `vis`, plus the walk's own per-entity cost, estimated as `walkTicksPerEntity` for each
// counted character tick (the snapshot walk's flag tests, Get() and dispatch, which a
// character that left the update tree would also stop paying). An UPPER bound: it assumes
// the skipped tick is replaced by nothing at all.
inline double BoundTicks(const Window& w, unsigned classes, unsigned vis, double walkTicksPerEntity) {
    return (double)w.CharTicks(classes, vis) + (double)w.ModelTicks(classes, vis) +
           walkTicksPerEntity * (double)w.Chars(classes, vis);
}

} // namespace CharProf
