// ============================================================================
// src/ext/registry.h -- the fixes and extensions registry (docs/extensions/DESIGN.md §3)
// ============================================================================
// NOT RETAIL. The one table of every non-faithful behaviour the exe can switch
// on, and the two read-once structs code tests them through:
//
//   if (Ext::Fix().icon_slide) { corrected } else { retail }       (a fix's lever)
//   if (Ext::Ext().<id>)       { the extension's body }            (an extension)
//
// WHERE THE VALUES COME FROM. settings.cfg (IO::Persistence's map), read ONCE
// at boot by Ext::InitFromSettings (registry_boot.cpp), after
// Persistence::ReadSettings:
//   fix.<id>=0|1     one per fix         absent or not 0/1 = the entry's default
//   ext.<id>=0|1     one per extension   absent or not 0/1 = the entry's default
//   profile=<name>   optional; overrides every per-entry key:
//                      faithful  every fix and every extension OFF (retail)
//                      all       every fix and every extension ON
//                    anything else (or absent) = the per-entry keys.
// The command line's `/profile:<name>` (System::Configuration, retail's own
// `/key:value` parser) overrides the settings key for that one run. It is
// how the gate scripts force the faithful profile (tools/build.sh run,
// tools/resave_gate/resave_gate.sh) without touching anyone's settings.cfg
// and without an environment flag. A run forced faithful from the command
// line also leaves the main menu exactly retail's (no Extensions button):
// see Ext::GateProfileForced.
//
// CLASSES (DESIGN.md §2). A fix has no class and toggles live. An extension's
// class is part of its entry and decides what a toggle costs:
//   A  live, any time
//   B  sim-divergent: locked for the game's lifetime
//   C  format-extending: locked for the game's lifetime (+ appended save chunk)
// The lock is taken by Ext::LockForGame at new game / load (the EXT SEAM in
// GameUpdateLoop::Load) and released with that loop. While a game holds the
// lock, Ext() keeps the B / C values the game started with; the dialog's
// toggles change only the REQUESTED value (settings.cfg), which the next new
// game / load picks up.
//
// This header and registry.cpp are standard C++ only (the unit tests link
// them on their own). The engine glue -- settings map, command line, the
// write-back -- is registry_boot.cpp.
// ============================================================================
#pragma once

#include <cstdint>
#include <string>

namespace Ext {

enum class Tier : uint8_t { Fix, Extension };

// `None` is a fix's class: fixes carry none (DESIGN.md §2).
enum class Class : uint8_t { None, A, B, C };

// One bool per registered fix. The names ARE the ids (settings key `fix.<id>`).
// The initialisers are the boot-less defaults; the table's `defaultOn` is the
// one InitFromSettings applies.
struct Fixes {
    bool icon_slide     = true;   // docs/retail_bugs.md §1
    bool residence_goto = true;   // docs/retail_bugs.md §2
    bool tavern_counter = true;   // docs/retail_bugs.md §3
    bool start_barn_height = true;   // docs/retail_bugs.md §4
    bool ui_scale_layout   = true;   // docs/retail_bugs.md §5 (engine seam UI::UserInterface::_extFixScaleLayout)
};

// One bool per registered extension (settings key `ext.<id>`). Each extension
// adds its field here and its row to the table in registry.cpp.
struct Extensions {
    bool eye_camera = false;   // class A: src/ext/eye_camera/ (docs/extensions/handins/EYECAM.md)
    bool sharp_ui   = false;   // class A: src/ext/sharp_ui/ (docs/extensions/handins/SHARPUI.md)
#ifdef BANISHED_UNIT_TEST
    // tests/unit/ext_registry_test.cpp only: one extension per class, so the
    // lock semantics are tested before a real extension exists. Not in the exe.
    bool unit_a = false;
    bool unit_b = false;
    bool unit_c = false;
#endif
};

// Both halves together: what the table's `slot` accessors index.
struct State {
    Fixes      fix;
    Extensions ext;
};

struct Entry {
    const char* id;             // settings key suffix: `fix.<id>` / `ext.<id>`
    Tier        tier;
    Class       cls;            // Class::None for a fix
    bool        defaultOn;      // fixes default ON, extensions OFF (DESIGN.md §2)
    const char* displayName;    // the dialog's row title
    const char* description;    // the dialog's row text
    int32_t     formatVersion;  // class B and C: >= 1 (pseudo-mod ModInfo +0x18, the save manifest); 0 otherwise
    bool& (*slot)(State& s);    // the entry's bool in a State
};

// The table, in menu order: every fix, then every extension.
int EntryCount();
const Entry& EntryAt(int index);
const Entry* FindEntry(const char* id);   // by id alone; null when unknown

// "fix." / "ext." for the entry's tier.
const char* KeyPrefix(Tier tier);
std::string SettingsKey(const Entry& e);  // e.g. "fix.icon_slide"

// "A" / "B" / "C", "" for a fix.
const char* ClassLetter(Class cls);
bool IsLockedClass(Class cls);            // B or C

// ---------------------------------------------------------------------------
// Profiles
// ---------------------------------------------------------------------------
enum class Profile : uint8_t { PerEntry, Faithful, All };

// "faithful" / "all" (exact, lower case); anything else, null or "" = PerEntry.
Profile ParseProfile(const char* text);
const char* ProfileName(Profile p);

// ---------------------------------------------------------------------------
// Loading (pure: the unit tests drive these)
// ---------------------------------------------------------------------------
// The settings source: true + `value` when `key` is present.
using Lookup = bool (*)(void* ctx, const char* key, std::string& value);

// "1" = on, "0" = off; anything else is not a toggle value (`ok` false).
bool ParseToggle(const std::string& value, bool& ok);

struct LoadReport {
    Profile profile          = Profile::PerEntry;
    bool    profileFromCommandLine = false;
    int     malformed        = 0;   // registered keys whose value was not 0/1 (default kept)
};

// The values `lookup` gives every registered entry, under the profile:
// `commandLineProfile` (null / "" = not given) wins over the settings key
// `profile`, which wins over the per-entry keys. Keys of ids not in the table
// are never asked for, so an unknown `fix.<x>` / `ext.<x>` is ignored.
State Resolve(Lookup lookup, void* ctx, const char* commandLineProfile, LoadReport* report);

// ---------------------------------------------------------------------------
// The live state
// ---------------------------------------------------------------------------
// What code reads. Valid from boot (InitFromSettings) on; before that, and in
// the unit tests until Install, the structs' initialisers.
const Fixes&      Fix();
const Extensions& Ext();

// The value the player asked for (settings / the dialog), which differs from
// the active one only for a B / C extension while a game holds the lock.
bool Requested(const Entry& e);
bool Active(const Entry& e);

// Replace both the requested and the active state (boot; the unit tests).
// Clears the lock.
void Install(const State& requested, Profile profile, bool profileFromCommandLine);

Profile CurrentProfile();
// True when the profile came from the command line (`/profile:...`): the gate.
bool GateProfileForced();

// The dialog's toggle: records `on` as requested. A fix, a class A extension,
// or any extension while no game holds the lock, takes effect at once; a
// B / C extension under the lock waits for the next LockForGame. Returns
// whether the active value changed now. (Persisting it is SetAndPersist's
// job, registry_boot.cpp.)
bool SetRequested(const Entry& e, bool on);

// ---------------------------------------------------------------------------
// The game lock (B / C)
// ---------------------------------------------------------------------------
// `owner` is the game that takes it (the GameUpdateLoop); UnlockForGame from
// any other owner is ignored, so a reload whose new loop locks before the old
// one is destroyed keeps the new lock.
void LockForGame(const void* owner);
void UnlockForGame(const void* owner);
bool Locked();
// Locked AND the entry is B / C: "locked for this game" in the dialog.
bool EntryLocked(const Entry& e);

// ---------------------------------------------------------------------------
// EXT SEAM (block SAVETAIL): the save's mod list
// ---------------------------------------------------------------------------
// Every ACTIVE class B / C extension, as the pseudo-mod the save must carry
// (DESIGN.md §2 as amended by SAVETAIL, main 1a180d42f): ModInfo name = the
// display name, path = `ext:<id>`, +0x10 = 0 (the package CRC, never compared
// for a missing mod), +0x14 = 0x010007ff, +0x18 = the extension's
// formatVersion (the field retail's modsMissing modal displays). Every B and
// C entry carries a formatVersion >= 1. Ids are [a-z0-9_]+ (never a `.`, so
// `ext:<id>` cannot name a file); ValidId checks one, the unit tests check
// the whole table.
//
// No save code is changed by this block. The engine's writer
// (Save::SaveLoad::SaveMetaData, src/engine/save/saveload_meta.cpp) must not
// include src/ext, so the seam is a null-by-default function pointer the game
// side installs at boot (e.g. `Save::SaveLoad::SetExtraModsProvider(...)`,
// the next block's to add and call): SaveModsProvider() below is the value to
// install -- null under the faithful profile and while no B / C extension is
// registered, so the engine body runs byte-identical when nothing is enabled.
constexpr uint32_t kPseudoModField14 = 0x010007ffu;

struct PseudoMod {
    std::string path;           // "ext:<id>"
    std::string name;           // the display name
    int32_t     field10;        // ModInfo +0x10: 0
    uint32_t    field14;        // ModInfo +0x14: kPseudoModField14
    int32_t     formatVersion;  // ModInfo +0x18
};
int PseudoModsForSave(PseudoMod* out, int capacity);   // returns the count (all, even past capacity)

// The engine-facing form: plain pointers, no std types. `path` / `name` stay
// valid until the next call (the registry owns them).
struct PseudoModRecord {
    const char* path;
    const char* name;
    int32_t     field10;
    uint32_t    field14;
    int32_t     formatVersion;
};
using ExtraModsProvider = int (*)(PseudoModRecord* out, int capacity);   // count, as above
int ProvideExtraModsForSave(PseudoModRecord* out, int capacity);
// What to install as the engine's provider at boot (after InitFromSettings):
// ProvideExtraModsForSave, or null under the faithful profile or when no
// B / C extension is registered.
ExtraModsProvider SaveModsProvider();

// [a-z0-9_]+
bool ValidId(const char* id);

// ---------------------------------------------------------------------------
// Engine glue (registry_boot.cpp; the exe only, not the unit tests)
// ---------------------------------------------------------------------------
// Boot: Resolve over IO::Persistence's settings map and the command line's
// `/profile:<name>`, then Install. Called once from Game::RunGameMain after
// Engine::Startup (which ran Persistence::ReadSettings). One `[Ext]` line per
// entry goes to stderr (standalone_error.log).
void InitFromSettings();

// The dialog's write-back: SetRequested, then the entry's `fix.<id>` /
// `ext.<id>` into the settings map (Persistence::Set, "%i"). The caller
// flushes once with PersistSettings (Persistence::WriteSettings).
bool SetAndRecord(const Entry& e, bool on);
void PersistSettings();

}  // namespace Ext
