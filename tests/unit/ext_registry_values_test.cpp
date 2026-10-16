// ============================================================================
// tests/unit/ext_registry_values_test.cpp -- the registry's VALUED KEYS (block AOMENU)
// ============================================================================
// src/ext/registry.{h,cpp} on their own: the table (ext.ao_quality 1..3 default
// 2, ext.ao_opacity 0..30 default 20), ParseValue (well-formed numbers are
// clamped, anything else is malformed = the default), Resolve over a settings
// text (absent / clamped / malformed / the profile rule: faithful and all leave
// every valued key at its default), SetValue (clamp, requested == active, never
// locked), and the "%i" round trip through IO::SettingsFile.
// ============================================================================
#include "unit.h"
#include "ext/registry.h"
#include "io/settings_file.h"

#include <cstdint>
#include <string>

namespace {

using IO::SettingsFile::Entries;

bool EntriesLookup(void* ctx, const char* key, std::string& value) {
    const Entries* entries = static_cast<const Entries*>(ctx);
    for (const IO::SettingsFile::Entry& e : *entries) {
        if (e.key == key) { value = e.value; return true; }
    }
    return false;
}

Ext::State ResolveText(const char* text, const char* cmd = nullptr,
                       Ext::LoadReport* report = nullptr) {
    Entries entries = IO::SettingsFile::Parse(std::string(text));
    return Ext::Resolve(&EntriesLookup, &entries, cmd, report);
}

const Ext::ValueKey& K(const char* id) {
    const Ext::ValueKey* k = Ext::FindValueKey(id);
    if (k == nullptr) { std::printf("  no valued key %s\n", id); std::abort(); }
    return *k;
}

}  // namespace

UNIT_TEST(ext_values_table_shape) {
    CHECK_EQ(Ext::ValueKeyCount(), 3);   // ao_quality, ao_opacity, foliage_aa_mode (ext_foliage_aa_test.cpp)
    const Ext::ValueKey& q = K("ao_quality");
    const Ext::ValueKey& o = K("ao_opacity");
    CHECK_EQ(q.minValue, 1); CHECK_EQ(q.maxValue, 3); CHECK_EQ(q.defaultValue, 2);
    CHECK_EQ(o.minValue, 0); CHECK_EQ(o.maxValue, 30); CHECK_EQ(o.defaultValue, 20);
    CHECK_STREQ(Ext::SettingsKey(q).c_str(), "ext.ao_quality");
    CHECK_STREQ(Ext::SettingsKey(o).c_str(), "ext.ao_opacity");
    CHECK(Ext::FindValueKey("no_such_key") == nullptr);
    CHECK(Ext::FindValueKey(nullptr) == nullptr);
    const Ext::State boot;   // the struct initialisers
    for (int i = 0; i < Ext::ValueKeyCount(); ++i) {
        const Ext::ValueKey& k = Ext::ValueKeyAt(i);
        CHECK(Ext::ValidId(k.id));
        CHECK(Ext::FindValueKey(k.id) == &k);
        // The settings key space is shared with the bool rows: no id in both.
        CHECK(Ext::FindEntry(k.id) == nullptr);
        // Every key belongs to a registered extension row.
        const Ext::Entry* owner = Ext::FindEntry(k.owner);
        CHECK(owner != nullptr && owner->tier == Ext::Tier::Extension);
        CHECK(k.minValue <= k.defaultValue && k.defaultValue <= k.maxValue);
        Ext::State s = boot;
        CHECK_EQ(k.slot(s), k.defaultValue);   // initialiser == table default
    }
    // The owner row the menu mirrors: class A, default off.
    const Ext::Entry* ao = Ext::FindEntry("ambient_occlusion");
    CHECK(ao != nullptr && ao->cls == Ext::Class::A && !ao->defaultOn);
}

UNIT_TEST(ext_values_parse_and_clamp) {
    const Ext::ValueKey& q = K("ao_quality");
    const Ext::ValueKey& o = K("ao_opacity");
    bool ok = false;
    CHECK_EQ(Ext::ParseValue(q, "1", ok), 1); CHECK(ok);
    CHECK_EQ(Ext::ParseValue(q, "3", ok), 3); CHECK(ok);
    CHECK_EQ(Ext::ParseValue(q, "0", ok), 1); CHECK(ok);      // below: clamped up
    CHECK_EQ(Ext::ParseValue(q, "7", ok), 3); CHECK(ok);      // above: clamped down
    CHECK_EQ(Ext::ParseValue(q, "-5", ok), 1); CHECK(ok);
    CHECK_EQ(Ext::ParseValue(o, "15", ok), 15); CHECK(ok);
    CHECK_EQ(Ext::ParseValue(o, "55", ok), 30); CHECK(ok);     // above the 30 ceiling: clamped
    CHECK_EQ(Ext::ParseValue(o, "007", ok), 7); CHECK(ok);
    CHECK_EQ(Ext::ParseValue(o, "999999999", ok), 30); CHECK(ok);
    CHECK_EQ(Ext::ParseValue(o, "-999999999", ok), 0); CHECK(ok);
    // Malformed: the default, ok false.
    for (const char* bad : {"", "-", "+5", "5x", "x5", "0x10", "1.5", " 5", "5 ", "1e2",
                            "1234567890", "--1", "fifty"}) {
        ok = true;
        CHECK_EQ(Ext::ParseValue(o, bad, ok), 20);
        CHECK(!ok);
        ok = true;
        CHECK_EQ(Ext::ParseValue(q, bad, ok), 2);
        CHECK(!ok);
    }
    CHECK_EQ(Ext::ClampValue(o, -(INT64_C(1) << 40)), 0);
    CHECK_EQ(Ext::ClampValue(o, INT64_C(1) << 40), 30);
    CHECK_EQ(Ext::ClampValue(o, 12), 12);
}

UNIT_TEST(ext_values_resolve) {
    Ext::LoadReport r;
    // Absent: the defaults, nothing counted.
    Ext::State s = ResolveText("", nullptr, &r);
    CHECK_EQ(s.val.ao_quality, 2);
    CHECK_EQ(s.val.ao_opacity, 20);
    CHECK_EQ(r.malformedValues, 0);
    CHECK_EQ(r.clampedValues, 0);
    // Present and in range.
    s = ResolveText("ext.ao_quality=3\next.ao_opacity=15\n", nullptr, &r);
    CHECK_EQ(s.val.ao_quality, 3);
    CHECK_EQ(s.val.ao_opacity, 15);
    CHECK_EQ(r.malformedValues, 0);
    CHECK_EQ(r.clampedValues, 0);
    // Out of range: clamped and counted; malformed: default and counted.
    s = ResolveText("ext.ao_quality=9\next.ao_opacity=lots\n", nullptr, &r);
    CHECK_EQ(s.val.ao_quality, 3);
    CHECK_EQ(s.val.ao_opacity, 20);
    CHECK_EQ(r.clampedValues, 1);
    CHECK_EQ(r.malformedValues, 1);
    CHECK_EQ(r.malformed, 0);   // the bool rows' counter is separate
    // A valued key is not a toggle and a toggle is not a valued key.
    s = ResolveText("ext.ambient_occlusion=1\next.ao_quality=1\n", nullptr, &r);
    CHECK(s.ext.ambient_occlusion);
    CHECK_EQ(s.val.ao_quality, 1);
    CHECK_EQ(r.malformed, 0);
}

UNIT_TEST(ext_values_profile_rule) {
    // faithful / all (settings key or command line) leave every valued key at its default,
    // whatever settings.cfg holds; the toggles follow the profile as before.
    const char* text = "ext.ao_quality=1\next.ao_opacity=25\next.ambient_occlusion=1\n";
    Ext::LoadReport r;
    Ext::State s = ResolveText((std::string("profile=faithful\n") + text).c_str(), nullptr, &r);
    CHECK(r.profile == Ext::Profile::Faithful);
    CHECK(!s.ext.ambient_occlusion);
    CHECK_EQ(s.val.ao_quality, 2);
    CHECK_EQ(s.val.ao_opacity, 20);
    s = ResolveText((std::string("profile=all\n") + text).c_str(), nullptr, &r);
    CHECK(s.ext.ambient_occlusion);
    CHECK_EQ(s.val.ao_quality, 2);
    CHECK_EQ(s.val.ao_opacity, 20);
    s = ResolveText(text, "faithful", &r);
    CHECK(r.profileFromCommandLine);
    CHECK_EQ(s.val.ao_quality, 2);
    CHECK_EQ(s.val.ao_opacity, 20);
    // Per-entry: the keys.
    s = ResolveText(text, nullptr, &r);
    CHECK(s.ext.ambient_occlusion);
    CHECK_EQ(s.val.ao_quality, 1);
    CHECK_EQ(s.val.ao_opacity, 25);
}

UNIT_TEST(ext_values_set_and_round_trip) {
    const Ext::ValueKey& q = K("ao_quality");
    const Ext::ValueKey& o = K("ao_opacity");
    Ext::Install(Ext::State(), Ext::Profile::PerEntry, false);
    CHECK_EQ(Ext::Value(q), 2);
    CHECK_EQ(Ext::SetValue(q, 3), 3);
    CHECK_EQ(Ext::Val().ao_quality, 3);
    CHECK_EQ(Ext::SetValue(q, 0), 1);       // clamped
    CHECK_EQ(Ext::SetValue(o, 250), 30);    // clamped to the ceiling
    CHECK_EQ(Ext::SetValue(o, 10), 10);
    // Class A: a game lock does not hold a value back.
    int owner = 0;
    Ext::LockForGame(&owner);
    CHECK_EQ(Ext::SetValue(o, 25), 25);
    CHECK_EQ(Ext::Value(o), 25);
    Ext::UnlockForGame(&owner);
    CHECK_EQ(Ext::Value(o), 25);            // the unlock copies requested -> active: same value

    // The round trip: what the write-back records ("%i", Persistence::Set) reads back equal.
    Entries out;
    for (int i = 0; i < Ext::ValueKeyCount(); ++i) {
        const Ext::ValueKey& k = Ext::ValueKeyAt(i);
        out.push_back({Ext::SettingsKey(k), std::to_string(Ext::Value(k))});
    }
    const std::string text = IO::SettingsFile::Serialize(out);
    Ext::LoadReport r;
    const Ext::State back = ResolveText(text.c_str(), nullptr, &r);
    CHECK_EQ(back.val.ao_quality, 1);
    CHECK_EQ(back.val.ao_opacity, 25);
    CHECK_EQ(r.malformedValues, 0);
    CHECK_EQ(r.clampedValues, 0);
    Ext::Install(Ext::State(), Ext::Profile::PerEntry, false);   // leave the live state clean
}
