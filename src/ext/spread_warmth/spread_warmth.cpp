// src/ext/spread_warmth/spread_warmth.cpp -- NOT RETAIL. ext.spread_warmth (class B); see the
// header and docs/extensions/handins/HITCH_PFBURST.md.
#include "ext/spread_warmth/spread_warmth.h"
#include "ext/registry.h"   // Ext::Ext().spread_warmth (B: locked for the game)

namespace SpreadWarmth {

bool g_on = false;

namespace {
int s_used = 0;              // evaluations charged this frame
bool s_deferredThisFrame = false;
int s_streak = 0;            // consecutive frames with a deferral, up to the last frame
Stats s_stats;
}  // namespace

void BeginFrame() {
    if (s_deferredThisFrame) {
        ++s_stats.framesDeferring;
        ++s_streak;
        if (s_streak > s_stats.maxStreak) s_stats.maxStreak = s_streak;
    } else {
        s_streak = 0;
    }
    s_deferredThisFrame = false;
    s_used = 0;
    g_on = ::Ext::Ext().spread_warmth;
}

bool DeferSlow(int candidates) {
    if (s_used > 0 && s_used + candidates > kBudget) {
        ++s_stats.deferrals;
        s_deferredThisFrame = true;
        return true;
    }
    s_used += candidates;
    ++s_stats.scans;
    s_stats.charged += candidates;
    return false;
}

const Stats& GetStats() { return s_stats; }
void ResetStats() { s_stats = Stats(); }

}  // namespace SpreadWarmth
