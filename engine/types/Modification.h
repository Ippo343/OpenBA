#pragma once
// IO::Modification — engine mod management system (0x28 bytes).
// Runtime-x64.dll, constructor at 0x1800b04b0, singleton at 0x1801fc6b0.
//
// Manages .pkm mod packages: discovery, enable/disable, sort order,
// persistence of settings, and filesystem mounting.
//
// Engine accessor: Engine::GetModification() returns *(engine + 0x680)
//
// Lifecycle:
//   1. Engine::Startup allocates 0x28 bytes, inline-constructs, calls RefreshList
//   2. RefreshList scans FileSystem::GetPackDirectory() for *.pkm files
//   3. SetupFileSystem mounts enabled mod packages into FileSystem
//   4. SaveList/Shutdown persists enabled/order state to Persistence
//
// Methods reimplemented:
//   - Find          (0x1800aea10) — linear search by filename
//   - Enable        (0x1800aeab0) — toggle mod enabled state
//   - DisableAll    (0x1800b05b0) — disable all mods
//
// Methods too small to hook (understood, trivial):
//   - Get           (0x18000dab0, 7 bytes) — singleton accessor
//   - GetCount      (0x18000da90, 4 bytes) — returns m_count
//   - IsDirty       (0x18000daa0, 4 bytes) — returns m_dirty
//   - GetModification (0x18000da80, 12 bytes) — array index
//   - AreExternalModsUpToDate (0x1800ae9f0, 3 bytes) — STUB: always true
//   - UpdateExternalMods (0x1800aea00, 3 bytes) — STUB: always false
//
// Methods blocked on dependencies:
//   - RefreshList      (~1500 bytes) — needs Persistence, FileSystem, PackageFile
//   - SaveList         (~300 bytes)  — needs Persistence
//   - SetupFileSystem  (~1500 bytes) — needs FileSystem, PackageFile

#include <cstdint>
#include <cstddef>

// Individual mod entry (0x80 = 128 bytes per entry)
struct Mod {
    // --- String fields (refcounted System::String) ---
    void*    m_name;            // +0x00: mod name
    void*    m_author;          // +0x08: mod author
    void*    m_description;     // +0x10: mod description

    // --- Version / metadata ---
    int32_t  m_field18;         // +0x18: unknown
    int32_t  m_versionMinor;    // +0x1C: minor version (compared in CheckMod)
    int32_t  m_sortIndex;       // +0x20: display/load order
    int32_t  m_field24;         // +0x24: unknown
    int32_t  m_field28;         // +0x28: unknown
    int32_t  m_versionMajor;    // +0x2C: major version (compared in CheckMod)

    // --- Dynamic arrays ---
    void*    m_intArrayData;    // +0x30: int array data pointer
    int32_t  m_intArrayCount;   // +0x38: count
    int32_t  m_intArrayCap;     // +0x3C: capacity

    void*    m_byteArrayData;   // +0x40: byte array data pointer
    int32_t  m_byteArrayCount;  // +0x48: count
    int32_t  m_byteArrayCap;    // +0x4C: capacity

    // --- Enabled flag ---
    uint8_t  m_enabled;         // +0x50: 1 = enabled, 0 = disabled
    uint8_t  _pad51[7];         // +0x51: padding

    // --- File path ---
    void*    m_filename;        // +0x58: System::Filename (refcounted, .pkm path)

    // --- Additional collections ---
    void*    m_collection60Data;    // +0x60: collection data pointer
    int64_t  m_collection60Size;    // +0x68: collection size/count

    void*    m_filenameArrayData;   // +0x70: Filename array data pointer
    int32_t  m_filenameArrayCount;  // +0x78: count
    int32_t  m_filenameArrayCap;    // +0x7C: capacity
};
static_assert(sizeof(Mod) == 0x80, "Mod must be 0x80 bytes");
static_assert(offsetof(Mod, m_name) == 0x00, "m_name at +0x00");
static_assert(offsetof(Mod, m_author) == 0x08, "m_author at +0x08");
static_assert(offsetof(Mod, m_description) == 0x10, "m_description at +0x10");
static_assert(offsetof(Mod, m_versionMinor) == 0x1C, "m_versionMinor at +0x1C");
static_assert(offsetof(Mod, m_sortIndex) == 0x20, "m_sortIndex at +0x20");
static_assert(offsetof(Mod, m_versionMajor) == 0x2C, "m_versionMajor at +0x2C");
static_assert(offsetof(Mod, m_enabled) == 0x50, "m_enabled at +0x50");
static_assert(offsetof(Mod, m_filename) == 0x58, "m_filename at +0x58");

// IO::Modification class (0x28 bytes)
struct Modification {
    uint8_t  m_dirty;           // +0x00: dirty flag (settings changed)
    uint8_t  m_savingEnabled;   // +0x01: persistence saving enabled
    uint8_t  _pad02[6];         // +0x02: padding

    Mod*     m_mods;            // +0x08: array of Mod structs
    int32_t  m_count;           // +0x10: number of mods
    int32_t  m_capacity;        // +0x14: array capacity

    Mod*     m_savedMods;       // +0x18: saved copy of mod array
    int32_t  m_savedCount;      // +0x20: saved mod count
    int32_t  m_savedCapacity;   // +0x24: saved array capacity
};
static_assert(sizeof(Modification) == 0x28, "Modification must be 0x28 bytes");
static_assert(offsetof(Modification, m_dirty) == 0x00, "m_dirty at +0x00");
static_assert(offsetof(Modification, m_savingEnabled) == 0x01, "m_savingEnabled at +0x01");
static_assert(offsetof(Modification, m_mods) == 0x08, "m_mods at +0x08");
static_assert(offsetof(Modification, m_count) == 0x10, "m_count at +0x10");
static_assert(offsetof(Modification, m_capacity) == 0x14, "m_capacity at +0x14");
static_assert(offsetof(Modification, m_savedMods) == 0x18, "m_savedMods at +0x18");
static_assert(offsetof(Modification, m_savedCount) == 0x20, "m_savedCount at +0x20");
static_assert(offsetof(Modification, m_savedCapacity) == 0x24, "m_savedCapacity at +0x24");

// Address constants
namespace ModificationAddr {
    constexpr uintptr_t Find                  = 0x1800aea10;
    constexpr uintptr_t Enable                = 0x1800aeab0;
    constexpr uintptr_t DisableAll            = 0x1800b05b0;
    constexpr uintptr_t EnableSavingSettings  = 0x1800aead0;
    constexpr uintptr_t Swap                  = 0x1800aeb20;
    constexpr uintptr_t RefreshList           = 0x1800af5f0;
    constexpr uintptr_t SaveList              = 0x1800b05f0;
    constexpr uintptr_t SetupFileSystem       = 0x1800aebb0;
}
