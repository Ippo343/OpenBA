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

#include <cstdio>
#include <string>

#include "io/Persistence.h"
#include "system/Configuration.h"
#include "system/String.h"

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

}  // namespace

void InitFromSettings() {
    const std::string cmd = CommandLineProfile();
    LoadReport report;
    const State requested = Resolve(&PersistenceLookup, nullptr, cmd.c_str(), &report);
    Install(requested, report.profile, report.profileFromCommandLine);

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
