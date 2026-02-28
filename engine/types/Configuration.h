#pragma once
// System::Configuration — game configuration singleton (0x30 bytes).
// Runtime-x64.dll, constructor at 0x1800C51B0, singleton pattern.
//
// Holds platform/language/build enums, command line map (AVL tree), resource params.
// Engine accessor: Engine::GetConfiguration() returns *(engine + 0x688)
//
// Methods (13 exported):
//   REIMPLEMENTED:
//     - CurrentBuildId()      (0x1800C4C60, 62b) — static, YYMMDD from compile date
//     - CurrentVersion()      (0x1800C4F40, 37b) — static, returns "1.0.7"
//     - GetVersionString()    (0x1800C4F90, ~100b) — static, formats packed version int
//     - GetPlatformName()     (0x1800C5210, 81b) — returns "Win" for Platform::Win
//     - GetLanguageName()     (0x1800C4BC0, 81b) — returns "English" for Language::English
//     - GetBuildName()        (0x1800C5280, 103b) — returns "Data"/"Eval"
//   PASSTHROUGH WRAPPERS:
//     - Ctor                  (0x1800C51B0, 62b) — zero-init fields, set singleton
//     - Dtor                  (0x1800C5170, 49b) — destroy map, cleanup lists
//     - GetCommandLineString  (0x1800C5300, 257b) — AVL tree lookup, returns String
//     - SetCommandLine        (0x1800C47A0, 391b) — parse & insert into AVL tree
//   TOO SMALL TO HOOK (documented):
//     - AddCommandLine        (0x1800C5200, 4b) — thunk to SetCommandLine
//     - GetResourceParameter  (0x1800C4750, 10b) — array index
//     - GetResourceParameterCount (0x1800C4770, 3b) — field read

#include <cstdint>
#include <cstddef>

// Build type enum (encoded in low byte of packed version uint)
// Packed version format: [major:8][minor:8][patch:8][buildType:8]
enum ConfigBuildType : uint8_t {
    BuildType_Prototype = 0xFB,
    BuildType_PreAlpha  = 0xFC,
    BuildType_Alpha     = 0xFD,
    BuildType_Beta      = 0xFE,
    BuildType_Release   = 0xFF
};

// Platform enum
enum ConfigPlatform : int32_t {
    Platform_Win = 0
};

// Language enum
enum ConfigLanguage : int32_t {
    Language_English = 0
};

// Build enum
enum ConfigBuild : int32_t {
    Build_Data = 0,
    Build_Eval = 1
};

struct Configuration {
    // --- Resource parameters (0x00 - 0x0F) ---
    void*    m_resourceParams;      // +0x00: String*[] (resource parameter array)
    int32_t  m_resourceParamCount;  // +0x08: number of resource parameters
    int32_t  _pad0C;                // +0x0C: padding

    // --- Command line (0x10 - 0x1F) ---
    void*    m_commandLineData;     // +0x10: Map<String,String> AVL tree root
    int32_t  m_commandLineCount;    // +0x18: number of command line entries
    int32_t  _pad1C;                // +0x1C: padding/capacity

    // --- Enums (0x20 - 0x2B) ---
    int32_t  m_platform;            // +0x20: Platform enum (0 = Win)
    int32_t  m_language;            // +0x24: Language enum (0 = English)
    int32_t  m_build;               // +0x28: Build enum (0 = Data, 1 = Eval)
    int32_t  _pad2C;                // +0x2C: tail padding (8-byte alignment)
};
static_assert(sizeof(Configuration) == 0x30, "Configuration must be 0x30 bytes");
static_assert(offsetof(Configuration, m_resourceParams) == 0x00, "m_resourceParams at +0x00");
static_assert(offsetof(Configuration, m_resourceParamCount) == 0x08, "m_resourceParamCount at +0x08");
static_assert(offsetof(Configuration, m_commandLineData) == 0x10, "m_commandLineData at +0x10");
static_assert(offsetof(Configuration, m_commandLineCount) == 0x18, "m_commandLineCount at +0x18");
static_assert(offsetof(Configuration, m_platform) == 0x20, "m_platform at +0x20");
static_assert(offsetof(Configuration, m_language) == 0x24, "m_language at +0x24");
static_assert(offsetof(Configuration, m_build) == 0x28, "m_build at +0x28");

// Address constants
namespace ConfigurationAddr {
    // Static methods (version/build info)
    constexpr uintptr_t CurrentBuildId    = 0x1800C4C60;
    constexpr uintptr_t CurrentVersion    = 0x1800C4F40;
    constexpr uintptr_t GetVersionString  = 0x1800C4F90;
    constexpr uintptr_t BuildDateString   = 0x180155C50;  // "Sep 10 2017" in .rdata

    // Instance methods
    constexpr uintptr_t Ctor                       = 0x1800C51B0;  // 62 bytes
    constexpr uintptr_t Dtor                       = 0x1800C5170;  // 49 bytes
    constexpr uintptr_t GetCommandLineString       = 0x1800C5300;  // 257 bytes
    constexpr uintptr_t SetCommandLine             = 0x1800C47A0;  // 391 bytes
    constexpr uintptr_t GetPlatformName            = 0x1800C5210;  // 81 bytes
    constexpr uintptr_t GetLanguageName            = 0x1800C4BC0;  // 81 bytes
    constexpr uintptr_t GetBuildName               = 0x1800C5280;  // 103 bytes

    // Too small to hook
    constexpr uintptr_t AddCommandLine             = 0x1800C5200;  // 4 bytes (thunk)
    constexpr uintptr_t GetResourceParameter       = 0x1800C4750;  // 10 bytes
    constexpr uintptr_t GetResourceParameterCount  = 0x1800C4770;  // 3 bytes
}
