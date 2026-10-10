// ============================================================================
// src/engine/io/persistence_settings.cpp — IO::Persistence's settings store
// (Phase D.4 SETTINGS E.3; replaces src/platform/win/persistence_registry_win.cpp)
// ============================================================================
//
//   | Export                                   | Retail      | Ord  |
//   |------------------------------------------|-------------|------|
//   | ?ReadSettings@Persistence@IO@@QEAAXXZ    | 0x1800b84b0 | 3462 |
//   | ?WriteSettings@Persistence@IO@@QEBAXXZ   | 0x1800b88e0 | 4561 |
//
// The map's two readers are untouched (persistence.cpp): `Get(int&)`
// 0x1800b81e0 is `strtol` on the value, `Get(bool&)` 0x1800b8290 is
// `strcmp(value, "true") == 0`, and a miss leaves the out-param alone. Every
// key is `Format("%s%s", section, key)`. What changes is only WHERE the map
// lives between runs: retail's two members walked
// HKCU\Software\Shining Rock Software LLC\Banished through IO::Registry
// (ctor 0x1800b9500: RegOpenKeyExW, else RegCreateKeyExW, KEY_ALL_ACCESS).
// They now read and write `<SaveDirectory>settings.cfg` through
// IO::SettingsStore (settings_store.h) — ENDGAME §E item 2, the user's
// decision; docs/runtime_port/phase_d/plans/SETTINGS.md §B, §D.
//
// ---------------------------------------------------------------------------
// RETAIL'S ReadSettings (0x1800b84b0), and what is kept
// ---------------------------------------------------------------------------
//   0x1800b84db  Map_Clear(this)                         ; KEPT, first
//   0x1800b84e7  Registry reg(nullptr)                   ; -> SettingsStore::Load
//   loop @ 0x1800b8510
//     RegEnumValueW(reg._key, index++, nameBuf[0x400 wchar])
//     RegQueryValueExW(reg._key, nameBuf, ..., valueBuf, 0x400 BYTES) ; result ignored
//     name = utf8(nameBuf); value = utf8(valueBuf)       ; AsUTF8
//     node = Map_Insert(this, name, value)               ; KEPT, in file order
//     if (node->value.data != value.data) node->value = value   ; KEPT (see below)
//   ~Registry: RegCloseKey
//
// Map_Insert (0x18002a090) leaves an ALREADY-PRESENT key's value untouched, so
// retail re-seats it by hand behind a self-assignment guard on the buffer
// (0x1800b87b2-0x1800b882d). The file's Parse collapses duplicates (last
// wins), so a duplicate never reaches the insert from a well-formed file; the
// re-seat stays because it is retail's and costs nothing.
//
// ---------------------------------------------------------------------------
// RETAIL'S WriteSettings (0x1800b88e0), and what is kept
// ---------------------------------------------------------------------------
//   phase 1  Registry reg(nullptr); for each node in order:
//              RegSetValueExW(reg._key, wide(key), REG_SZ, wide(value))
//   phase 2  collect every registry VALUE name absent from the map, then
//              RegDeleteKeyW(reg._key, name)   ; IAT 0x18014b008 is RegDeleteKeyW,
//                                              ; not RegDeleteValueW: it deletes
//                                              ; NOTHING (no such subkey)
// The observable outcome is "every map entry is written, every value already
// in the store survives". Store's Merge is exactly that: the file is re-read,
// the map is laid over it (map wins) and the union is written. It matters
// after `-reset` (ResetConfigurations clears the map): retail kept `Ach<N>`,
// `ModEnabled_*` and the key binds in the registry, and so does the file.
//
// ---------------------------------------------------------------------------
// WHAT IS DIFFERENT, AND WHY (declared; none reachable from retail data)
// ---------------------------------------------------------------------------
//  * The store is a UTF-8 text file, written atomically (tmp + replace), not
//    a registry key written value by value. Its writers are serialised by one
//    mutex inside SettingsStore::Store (plans/SETTINGS.md §F).
//  * File absent: ReadSettings imports the retail key ONCE, READ-ONLY
//    (KEY_READ, never created, REG_SZ values only; the user's Q2 answer). The
//    first WriteSettings — every boot reaches one in Engine::PlatformConfigure
//    — materialises the file; the registry is never written again. Retail's
//    ctor CREATED the key; nothing here creates one.
//  * No 0x400-byte value cap (retail's RegQueryValueExW buffer truncated a
//    value at 511 characters); keys compare case-sensitively (registry value
//    names do not; no retail key pair differs only by case).
//  * Documents folder unknown: ReadSettings leaves the map empty and
//    WriteSettings writes nothing — retail's `reg._key == 0` outcome.
// ============================================================================

#include <cstdio>
#include <string>

#include "io/Persistence.h"
#include "io/settings_file.h"
#include "io/settings_store.h"
#include "system/String.h"
#include "collection/avl_trees.h"    // rtavl::StringMapInsert (0x18002a090)
#include "collection/Collection.h"   // Collection::Set (the settings map)

using RtIo::SettingsNode;

namespace {

// The in-order walk retail's phase 1 makes (0x1800b8918 leftmost, then the
// successor at 0x1800b8a80..0x1800b8acb): the links are the Set node's own
// (Persistence.h: parent +0x10, left +0x18, right +0x20).
const SettingsNode* Leftmost(const SettingsNode* node) {
    if (!node) return nullptr;
    while (node->left) node = node->left;
    return node;
}

const SettingsNode* InorderSuccessor(const SettingsNode* node) {
    if (node->right) return Leftmost(node->right);
    const SettingsNode* p = node->parent;
    while (p && p->right == node) {
        node = p;
        p = node->parent;
    }
    return p;
}

const char* SourceName(IO::SettingsStore::Source s) {
    switch (s) {
    case IO::SettingsStore::Source::File:   return "file";
    case IO::SettingsStore::Source::Legacy: return "registry import (read-only)";
    case IO::SettingsStore::Source::None:   return "none (defaults)";
    }
    return "?";
}

}  // namespace

namespace IO {

// ?ReadSettings@Persistence@IO@@QEAAXXZ                    retail 0x1800b84b0
void Persistence::ReadSettings() {

    _settingsMap.Clear();                                      // 0x1800b84db -> 0x18000f7d0

    std::string path;
    SettingsFile::Entries entries;
    SettingsStore::Source source = SettingsStore::Source::None;
    if (SettingsStore::DefaultPath(path))
        source = SettingsStore::Load(path, entries);

    // One line per read: the boot read and the options dialog's cancel /
    // restore. stderr, so it lands in standalone_error.log.
    std::fprintf(stderr, "[Settings] ReadSettings: %d entries from %s (%s)\n",
                 static_cast<int>(entries.size()), SourceName(source),
                 path.empty() ? "<no documents folder>" : path.c_str());

    for (const SettingsFile::Entry& e : entries) {
        System::String name(e.key.c_str());
        System::String value(e.value.c_str());

        SettingsNode* node = nullptr;
        rtavl::StringMapInsert(&_settingsMap, &node,           // 0x1800b87ad -> 0x18002a090
                               &name._data, &value._data);

        // 0x1800b87b2-0x1800b882d: re-seat an already-present key's value;
        // the `!=` is retail's self-assignment guard (String::operator=,
        // 0x1800c9810).
        if (node != nullptr && node->element.value._data != value._data)
            node->element.value = value;
        // ~value, ~name: retail's two releases per iteration.
    }
}

// ?WriteSettings@Persistence@IO@@QEBAXXZ                   retail 0x1800b88e0
void Persistence::WriteSettings() const {
    std::string path;
    if (!SettingsStore::DefaultPath(path))
        return;                                                // retail: reg._key == 0

    // Phase 1's walk: every entry, in the map's (strcmp) order.
    SettingsFile::Entries entries;
    entries.reserve(static_cast<size_t>(_settingsMap._count));
    for (const SettingsNode* n = Leftmost(_settingsMap._root); n != nullptr;
         n = InorderSuccessor(n)) {
        const char* k = n->element.key._data;
        const char* v = n->element.value._data;
        // A null String buffer is the empty string (retail's WideString of it
        // writes an empty REG_SZ).
        entries.push_back(SettingsFile::Entry{k ? k : "", v ? v : ""});
    }

    // Phase 2's outcome (nothing deleted) is Store's merge.
    if (!SettingsStore::Store(path, entries))
        std::fprintf(stderr, "[Settings] WriteSettings: FAILED to write %s\n", path.c_str());
}

}  // namespace IO
