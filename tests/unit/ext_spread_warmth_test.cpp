// tests/unit/ext_spread_warmth_test.cpp -- ext.spread_warmth (class B, HITCH_PFBURST.md):
//   the registry row (class B, format 1, default off, locked for the game, the pseudo-mod
//   ext:spread_warmth), and the per-frame budget: off never defers; on, the first scan of a
//   frame always runs, later scans run while the charged total stays within kBudget, the rest
//   are deferred until the next BeginFrame, and the streak counter follows frames with a
//   deferral.
#include "unit.h"
#include "ext/spread_warmth/spread_warmth.h"
#include "ext/registry.h"

#include <cstring>
#include <string>
#include <vector>

namespace {
const Ext::Entry& E(const char* id) {
    const Ext::Entry* e = Ext::FindEntry(id);
    CHECK(e != nullptr);
    static Ext::Entry none{};
    return e ? *e : none;
}
}  // namespace

UNIT_TEST(ext_spread_warmth_registry_row) {
    const Ext::Entry& e = E("spread_warmth");
    CHECK(e.tier == Ext::Tier::Extension);
    CHECK(e.cls == Ext::Class::B);           // a deferred citizen diverges from retail
    CHECK(!e.defaultOn);
    CHECK(e.formatVersion == 1);
    CHECK(std::strstr(e.description, "iverges") != nullptr);
    CHECK(Ext::SettingsKey(e) == "ext.spread_warmth");

    Ext::State on{};
    e.slot(on) = true;
    Ext::Install(on, Ext::Profile::PerEntry, false);
    int owner = 0;
    Ext::LockForGame(&owner);
    CHECK(Ext::EntryLocked(e));
    CHECK(!Ext::SetRequested(e, false));     // locked: the game keeps it
    CHECK(Ext::Ext().spread_warmth);
    std::vector<Ext::PseudoMod> mods((size_t)Ext::EntryCount());
    const int n = Ext::PseudoModsForSave(mods.data(), (int)mods.size());
    int saw = 0;
    for (int k = 0; k < n && k < (int)mods.size(); ++k)
        if (mods[(size_t)k].path == "ext:spread_warmth") { ++saw; CHECK(mods[(size_t)k].formatVersion == 1); }
    CHECK_EQ(saw, 1);
    Ext::UnlockForGame(&owner);
    Ext::Install(Ext::State{}, Ext::Profile::Faithful, false);
    CHECK(!Ext::Ext().spread_warmth);
    Ext::Install(Ext::State{}, Ext::Profile::PerEntry, false);
}

UNIT_TEST(ext_spread_warmth_budget) {
    using namespace SpreadWarmth;
    // Off: never defers, charges nothing.
    Ext::Install(Ext::State{}, Ext::Profile::PerEntry, false);
    BeginFrame();
    ResetStats();
    for (int i = 0; i < 100; ++i) CHECK(!Defer(kBudget));
    CHECK_EQ(GetStats().scans, 0);

    Ext::State on{};
    on.ext.spread_warmth = true;
    Ext::Install(on, Ext::Profile::PerEntry, false);
    BeginFrame();
    ResetStats();
    // The first scan always runs, even over the budget.
    CHECK(!Defer(kBudget * 3));
    CHECK(Defer(1));
    BeginFrame();                             // frame 2: had a deferral last frame
    const int per = 300;
    int served = 0;
    while (!Defer(per)) ++served;
    CHECK_EQ(served, kBudget / per);          // 6 x 300 = 1800 <= 2048, a 7th would pass it
    CHECK(Defer(per));                         // still deferred this frame
    BeginFrame();                             // frame 3
    CHECK(!Defer(per));
    BeginFrame();                             // frame 4: frame 3 had no deferral
    const Stats& st = GetStats();
    CHECK_EQ(st.deferrals, 3);
    CHECK_EQ(st.framesDeferring, 2);
    CHECK_EQ(st.maxStreak, 2);
    CHECK_EQ(st.scans, 1 + served + 1);
    Ext::Install(Ext::State{}, Ext::Profile::PerEntry, false);
    BeginFrame();
}
