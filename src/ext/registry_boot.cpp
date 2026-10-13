// ============================================================================
// src/ext/registry_boot.cpp -- the registry's engine glue (exe only)
// ============================================================================
// NOT RETAIL. registry.h has the contract. Three things touch the engine:
//   * the settings map: IO::Persistence::_singleton->_settingsMap, filled from
//     settings.cfg by Persistence::ReadSettings during Engine::Startup;
//   * the command line: System::Configuration::_singleton, retail's own
//     `/key:value` parser (Configuration::SetCommandLine, configuration.cpp),
//     read for the one key `profile`;
//   * the write-back: Persistence::Set (the "%i" spelling) + WriteSettings.
// No getenv, no BANISHED_* flag (CLAUDE.md, DIRECTION.md §2).
// ============================================================================

#include "ext/registry.h"
#include "ext/draw_gather/draw_gather.h"   // ext.draw_gather: its engine seams follow the toggle

#include <cstdio>
#include <string>

#include <cstddef>
#include <vector>

#include "io/Persistence.h"
#include "save/SaveLoad.h"      // the pseudo-mod seam Save::SaveLoad::_extExtraMods (DESIGN.md hook rule 5)
#include "system/Configuration.h"
#include "system/String.h"
#include "ui/UserInterface.h"   // the fix.ui_scale_layout seam (DESIGN.md hook rule 5)

namespace Ext {

namespace {

// Lookup over the live settings map: the raw value string.
bool PersistenceLookup(void* /*ctx*/, const char* key, std::string& value) {
    const ::IO::Persistence* p = ::IO::Persistence::_singleton;
    if (p == nullptr) return false;
    System::String k(key);
    const RtIo::SettingsNode* node = p->_settingsMap.Find(k._data);
    if (node == nullptr) return false;
    const char* v = node->element.value._data;
    value = v ? v : "";
    return true;
}

// The command line's `/profile:<name>`, or "" when absent.
std::string CommandLineProfile() {
    const System::Configuration* cfg = System::Configuration::_singleton;
    if (cfg == nullptr) return std::string();
    const System::String value = cfg->GetCommandLineString(System::String("profile"));
    return value._data ? std::string(value._data) : std::string();
}

// The engine's record and the registry's are the same five fields in the same
// order (registry.h EXT SEAM): asserted, so a field added on one side fails
// the build. The adapter copies field by field (no cast across the two types).
using EngineRecord = ::Save::SaveLoad::ExtraModRecord;
static_assert(sizeof(EngineRecord) == sizeof(PseudoModRecord), "pseudo-mod record size");
static_assert(offsetof(EngineRecord, path) == offsetof(PseudoModRecord, path), "path");
static_assert(offsetof(EngineRecord, name) == offsetof(PseudoModRecord, name), "name");
static_assert(offsetof(EngineRecord, field10) == offsetof(PseudoModRecord, field10), "+0x10");
static_assert(offsetof(EngineRecord, field14) == offsetof(PseudoModRecord, field14), "+0x14");
static_assert(offsetof(EngineRecord, formatVersion) == offsetof(PseudoModRecord, formatVersion),
              "+0x18");

int EngineExtraMods(EngineRecord* out, int capacity) {
    if (out == nullptr || capacity <= 0) return ProvideExtraModsForSave(nullptr, 0);
    std::vector<PseudoModRecord> recs(static_cast<size_t>(capacity));
    const int n = ProvideExtraModsForSave(recs.data(), capacity);
    for (int i = 0; i < n && i < capacity; ++i) {
        out[i].path          = recs[static_cast<size_t>(i)].path;
        out[i].name          = recs[static_cast<size_t>(i)].name;
        out[i].field10       = recs[static_cast<size_t>(i)].field10;
        out[i].field14       = recs[static_cast<size_t>(i)].field14;
        out[i].formatVersion = recs[static_cast<size_t>(i)].formatVersion;
    }
    return n;
}

}  // namespace

void InitFromSettings() {
    const std::string cmd = CommandLineProfile();
    LoadReport report;
    const State requested = Resolve(&PersistenceLookup, nullptr, cmd.c_str(), &report);
    Install(requested, report.profile, report.profileFromCommandLine);
    // Engine-side fix seams: null by default, pointed at the live bool here
    // (fixes are global and live: the pointer follows the menu toggle).
    ::UI::UserInterface::_extFixScaleLayout = &Fix().ui_scale_layout;
    // The save's pseudo-mods (EXT SEAM, Save::SaveLoad::_extExtraMods). Installed
    // ONCE here: the profile is fixed for the run (settings key or /profile at
    // boot; nothing changes it later), so SaveModsProvider() is final now --
    // null under the faithful profile or while no B / C extension is registered.
    // A B / C toggle needs no re-install: the provider reports the ACTIVE set
    // and returns 0 records while none is on, so the engine appends nothing.
    ::Save::SaveLoad::_extExtraMods = SaveModsProvider() != nullptr ? &EngineExtraMods : nullptr;
    // ext.draw_gather (class A, live): the three Graphics::Scene seams (scene_ext.h).
    DrawGather::Sync();

    std::fprintf(stderr, "[Ext] profile=%s (%s)%s, %d entries, %d malformed value(s)\n",
                 ProfileName(report.profile),
                 report.profileFromCommandLine ? "command line /profile"
                                               : "settings.cfg",
                 GateProfileForced() ? " -- GATE: faithful forced, menu unchanged" : "",
                 EntryCount(), report.malformed);
    for (int i = 0; i < EntryCount(); ++i) {
        const Entry& e = EntryAt(i);
        std::fprintf(stderr, "[Ext]   %s=%d%s%s\n", SettingsKey(e).c_str(),
                     Active(e) ? 1 : 0,
                     e.tier == Tier::Extension ? " class " : "",
                     ClassLetter(e.cls));
    }
}

bool SetAndRecord(const Entry& e, bool on) {
    const bool changedNow = SetRequested(e, on);
    DrawGather::Sync();   // a class A toggle takes effect now (no-op for every other entry)
    if (::IO::Persistence* p = ::IO::Persistence::_singleton)
        p->Set(KeyPrefix(e.tier), e.id, on ? 1 : 0);   // "fix.<id>" / "ext.<id>" = "%i"
    std::fprintf(stderr, "[Ext] %s requested=%d active=%d%s\n", SettingsKey(e).c_str(),
                 on ? 1 : 0, Active(e) ? 1 : 0,
                 EntryLocked(e) ? " (locked for this game)" : "");
    return changedNow;
}

void PersistSettings() {
    if (const ::IO::Persistence* p = ::IO::Persistence::_singleton)
        p->WriteSettings();
}

}  // namespace Ext
