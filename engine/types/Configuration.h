#pragma once
// System::Configuration — game configuration singleton (0x2C bytes).
// Runtime-x64.dll, constructor at 0x1800C51B0, singleton pattern.
//
// Holds platform/language/build enums, command line map, resource params.
// Three key static methods provide version/build info:
//   - CurrentBuildId()      (0x1800C4C60) — YYMMDD from compile date
//   - CurrentVersion()      (0x1800C4F40) — "1.0.7" (calls GetVersionString)
//   - GetVersionString()    (0x1800C4F90) — formats packed version int
//
// Engine accessor: Engine::GetConfiguration() returns *(engine + 0x688)
//
// 29 methods total exported. The three static methods above are the
// reimplementation targets; the rest are trivial accessors or use
// complex Map/String infrastructure.

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

struct Configuration {
    // --- Resource parameters (0x00 - 0x0F) ---
    void*    m_resourceParams;      // +0x00: String*[] (resource parameter array)
    int32_t  m_resourceParamCount;  // +0x08: number of resource parameters
    int32_t  _pad0C;                // +0x0C: padding

    // --- Command line (0x10 - 0x1F) ---
    void*    m_commandLineData;     // +0x10: Map<String,String> data pointer
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
    constexpr uintptr_t CurrentBuildId    = 0x1800C4C60;
    constexpr uintptr_t CurrentVersion    = 0x1800C4F40;
    constexpr uintptr_t GetVersionString  = 0x1800C4F90;
    constexpr uintptr_t BuildDateString   = 0x180155C50;  // "Sep 10 2017" in .rdata
}
