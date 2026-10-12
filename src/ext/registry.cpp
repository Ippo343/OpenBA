// ============================================================================
// src/ext/registry.cpp -- the fixes and extensions table and its state
// ============================================================================
// NOT RETAIL. registry.h has the contract. Standard C++ only: the unit tests
// (tests/unit/ext_registry_test.cpp) link this file alone.
// ============================================================================

#include "ext/registry.h"

#include <cstring>

namespace Ext {

namespace {

// ---------------------------------------------------------------------------
// THE TABLE. Fixes first (default ON), then extensions (default OFF), in the
// order the dialog lists them. A fix's description says what the player saw
// in retail; docs/retail_bugs.md carries the mechanism and the lever.
// ---------------------------------------------------------------------------
const Entry kEntries[] = {
    { "icon_slide", Tier::Fix, Class::None, true,
      "Status icons appear in place",
      "Retail draws a new building status icon at the map centre for one sim "
      "tick, so it slides across the map to its building. Fixed: the icon "
      "appears on its building.",
      0, [](State& s) -> bool& { return s.fix.icon_slide; } },

    { "residence_goto", Tier::Fix, Class::None, true,
      "House panel 'go to' button",
      "Retail's 'go to' button on a house panel reads the wrong component, so "
      "it can centre on nothing or the wrong citizen. Fixed: it centres on the "
      "resident.",
      0, [](State& s) -> bool& { return s.fix.residence_goto; } },

    { "tavern_counter", Tier::Fix, Class::None, true,
      "Tavern consumption statistics",
      "When a visit spends the last unit of a good, retail counts the next good "
      "in storage as consumed. Fixed: the good that was spent is counted.",
      0, [](State& s) -> bool& { return s.fix.tavern_counter; } },

    { "start_barn_height", Tier::Fix, Class::None, true,
      "Starting buildings sit on the ground",
      "Retail positions the town's starting buildings at the terrain height sampled "
      "BEFORE the ground under them is flattened, so the storage barn can float above "
      "the ground. Fixed: they sit at the flattened level, as player-built ones do.",
      0, [](State& s) -> bool& { return s.fix.start_barn_height; } },

    { "ui_scale_layout", Tier::Fix, Class::None, true,
      "Dialogs stay put when the UI scale changes",
      "When the user interface scale is applied in Options, retail draws one "
      "frame with every open dialog laid out for the old scale, so dialogs "
      "flash to a corner before centring. Fixed: they are laid out at the new "
      "scale on that frame.",
      0, [](State& s) -> bool& { return s.fix.ui_scale_layout; } },

    // Extensions. Each one adds a field to Ext::Extensions and a row here:
    // Tier::Extension, its Class, default false, and for class B or C its
    // formatVersion (>= 1). The id is [a-z0-9_]+ (the unit tests check every row).
    { "eye_camera", Tier::Extension, Class::A, false,
      "Eye-level camera",
      "Follow a citizen at eye level, looking where they look.",
      0, [](State& s) -> bool& { return s.ext.eye_camera; } },

    { "sharp_ui", Tier::Extension, Class::A, false,
      "Sharp UI scaling",
      "Crisp text at any UI scale: the game's fonts drawn from copies rasterised "
      "at the screen's pixel size, and pixel-snapped sprites. Layout is unchanged.",
      0, [](State& s) -> bool& { return s.ext.sharp_ui; } },

    { "fast_paths", Tier::Extension, Class::B, false,
      "Faster pathfinding",
      "Paths are found with a sharper search: every route costs exactly what "
      "the original game's would, found with far less work. Where two routes "
      "cost the same, a citizen may take the other one, so a game played with "
      "it diverges from the original. Locked for the game.",
      1, [](State& s) -> bool& { return s.ext.fast_paths; } },

#ifdef BANISHED_UNIT_TEST
    // The unit tests' three classes (registry.h). Not compiled into the exe.
    { "unit_a", Tier::Extension, Class::A, false, "Unit A", "test only", 0,
      [](State& s) -> bool& { return s.ext.unit_a; } },
    { "unit_b", Tier::Extension, Class::B, false, "Unit B", "test only", 1,
      [](State& s) -> bool& { return s.ext.unit_b; } },
    { "unit_c", Tier::Extension, Class::C, false, "Unit C", "test only", 3,
      [](State& s) -> bool& { return s.ext.unit_c; } },
#endif
};

constexpr int kEntryCount = static_cast<int>(sizeof(kEntries) / sizeof(kEntries[0]));

// The two states. `g_requested` is what settings.cfg / the dialog asked for;
// `g_active` is what code reads (Fix() / Ext()). They differ only for a B / C
// extension while a game holds the lock.
State       g_requested;
State       g_active;
Profile     g_profile = Profile::PerEntry;
bool        g_profileFromCommandLine = false;
const void* g_lockOwner = nullptr;

}  // namespace

int EntryCount() { return kEntryCount; }

const Entry& EntryAt(int index) { return kEntries[index]; }

const Entry* FindEntry(const char* id) {
    if (id == nullptr) return nullptr;
    for (const Entry& e : kEntries)
        if (std::strcmp(e.id, id) == 0) return &e;
    return nullptr;
}

const char* KeyPrefix(Tier tier) { return tier == Tier::Fix ? "fix." : "ext."; }

std::string SettingsKey(const Entry& e) { return std::string(KeyPrefix(e.tier)) + e.id; }

const char* ClassLetter(Class cls) {
    switch (cls) {
    case Class::A: return "A";
    case Class::B: return "B";
    case Class::C: return "C";
    case Class::None: break;
    }
    return "";
}

bool IsLockedClass(Class cls) { return cls == Class::B || cls == Class::C; }

Profile ParseProfile(const char* text) {
    if (text == nullptr) return Profile::PerEntry;
    if (std::strcmp(text, "faithful") == 0) return Profile::Faithful;
    if (std::strcmp(text, "all") == 0) return Profile::All;
    return Profile::PerEntry;
}

const char* ProfileName(Profile p) {
    switch (p) {
    case Profile::Faithful: return "faithful";
    case Profile::All:      return "all";
    case Profile::PerEntry: break;
    }
    return "per-entry";
}

bool ParseToggle(const std::string& value, bool& ok) {
    ok = true;
    if (value == "1") return true;
    if (value == "0") return false;
    ok = false;
    return false;
}

State Resolve(Lookup lookup, void* ctx, const char* commandLineProfile, LoadReport* report) {
    LoadReport local;
    LoadReport& r = report ? *report : local;
    r = LoadReport();

    // The profile: the command line first, then the settings key.
    if (commandLineProfile != nullptr && commandLineProfile[0] != '\0') {
        r.profile = ParseProfile(commandLineProfile);
        r.profileFromCommandLine = true;
    } else {
        std::string value;
        if (lookup != nullptr && lookup(ctx, "profile", value))
            r.profile = ParseProfile(value.c_str());
    }

    State s;
    for (const Entry& e : kEntries) {
        bool on = e.defaultOn;
        if (r.profile == Profile::Faithful) {
            on = false;
        } else if (r.profile == Profile::All) {
            on = true;
        } else {
            std::string value;
            if (lookup != nullptr && lookup(ctx, SettingsKey(e).c_str(), value)) {
                bool ok = false;
                const bool parsed = ParseToggle(value, ok);
                if (ok) on = parsed;
                else ++r.malformed;
            }
        }
        e.slot(s) = on;
    }
    return s;
}

const Fixes&      Fix() { return g_active.fix; }
const Extensions& Ext() { return g_active.ext; }

bool Requested(const Entry& e) { return e.slot(g_requested); }
bool Active(const Entry& e)    { return e.slot(g_active); }

void Install(const State& requested, Profile profile, bool profileFromCommandLine) {
    g_requested = requested;
    g_active = requested;
    g_profile = profile;
    g_profileFromCommandLine = profileFromCommandLine;
    g_lockOwner = nullptr;
}

Profile CurrentProfile() { return g_profile; }

bool GateProfileForced() { return g_profileFromCommandLine && g_profile == Profile::Faithful; }

bool SetRequested(const Entry& e, bool on) {
    e.slot(g_requested) = on;
    if (EntryLocked(e)) return false;   // waits for the next LockForGame
    const bool changed = e.slot(g_active) != on;
    e.slot(g_active) = on;
    return changed;
}

void LockForGame(const void* owner) {
    // The game starts with what the player asked for; from here on a B / C
    // toggle only changes the request.
    for (const Entry& e : kEntries)
        if (e.tier == Tier::Extension && IsLockedClass(e.cls))
            e.slot(g_active) = e.slot(g_requested);
    g_lockOwner = owner;
}

bool LockForGameWithSet(const void* owner, const std::vector<std::string>& ids) {
    for (const std::string& id : ids) {
        const Entry* e = FindEntry(id.c_str());
        if (e == nullptr || e->tier != Tier::Extension || !IsLockedClass(e->cls)) return false;
    }
    // The save decides: exactly its B / C set, the request notwithstanding.
    for (const Entry& e : kEntries) {
        if (e.tier != Tier::Extension || !IsLockedClass(e.cls)) continue;
        bool on = false;
        for (const std::string& id : ids)
            if (id == e.id) { on = true; break; }
        e.slot(g_active) = on;
    }
    g_lockOwner = owner;
    return true;
}

void UnlockForGame(const void* owner) {
    if (owner == nullptr || owner != g_lockOwner) return;
    g_lockOwner = nullptr;
    // Back at the menu nothing holds the lock: the active state is the request.
    for (const Entry& e : kEntries)
        e.slot(g_active) = e.slot(g_requested);
}

bool Locked() { return g_lockOwner != nullptr; }

bool EntryLocked(const Entry& e) {
    return Locked() && e.tier == Tier::Extension && IsLockedClass(e.cls);
}

int PseudoModsForSave(PseudoMod* out, int capacity) {
    int n = 0;
    for (const Entry& e : kEntries) {
        if (e.tier != Tier::Extension || !IsLockedClass(e.cls)) continue;
        if (!e.slot(g_active)) continue;
        if (out != nullptr && n < capacity) {
            out[n].path = std::string("ext:") + e.id;
            out[n].name = e.displayName;
            out[n].field10 = 0;
            out[n].field14 = kPseudoModField14;
            out[n].formatVersion = e.formatVersion;
        }
        ++n;
    }
    return n;
}

int ProvideExtraModsForSave(PseudoModRecord* out, int capacity) {
    // The registry owns the strings until the next call.
    static PseudoMod held[kEntryCount > 0 ? kEntryCount : 1];
    const int n = PseudoModsForSave(held, kEntryCount);
    for (int i = 0; out != nullptr && i < n && i < capacity; ++i) {
        out[i].path = held[i].path.c_str();
        out[i].name = held[i].name.c_str();
        out[i].field10 = held[i].field10;
        out[i].field14 = held[i].field14;
        out[i].formatVersion = held[i].formatVersion;
    }
    return n;
}

ExtraModsProvider SaveModsProvider() {
    if (g_profile == Profile::Faithful) return nullptr;
    for (const Entry& e : kEntries)
        if (e.tier == Tier::Extension && IsLockedClass(e.cls))
            return &ProvideExtraModsForSave;
    return nullptr;
}

bool IsPseudoModPath(const char* path) {
    return path != nullptr &&
           std::strncmp(path, kPseudoModPrefix, std::strlen(kPseudoModPrefix)) == 0;
}

int PseudoModLoadStatus(const char* path, int32_t savedFormatVersion) {
    if (!IsPseudoModPath(path)) return 1;
    const Entry* e = FindEntry(path + std::strlen(kPseudoModPrefix));
    if (e == nullptr || e->tier != Tier::Extension || !IsLockedClass(e->cls)) return 1;
    if (savedFormatVersion > e->formatVersion) return 1;
    if (!Requested(*e)) return 1;
    return 0;
}

bool ValidId(const char* id) {
    if (id == nullptr || id[0] == '\0') return false;
    for (const char* p = id; *p; ++p) {
        const char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

}  // namespace Ext
