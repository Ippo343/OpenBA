// engine/reimpl/configuration.cpp
// Reimplementation of System::Configuration static methods from Runtime-x64.dll.
//
// These are static methods that return System::String by value. In MSVC x64 ABI,
// String has a user-defined destructor so it's returned via hidden pointer in RCX,
// even though it's only 8 bytes. Our hooks receive the hidden return pointer as
// the first parameter.
//
// Methods reimplemented:
//   - CurrentBuildId    (0x1800C4C60) — parses compile date → YYMMDD build ID
//   - CurrentVersion    (0x1800C4F40) — returns "1.0.7" (wrapper for GetVersionString)
//   - GetVersionString  (0x1800C4F90) — formats packed version int to "X.Y.Z [suffix]"

#include "Configuration.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================

static HookInfo g_hookCurrentBuildId;
static HookInfo g_hookCurrentVersion;
static HookInfo g_hookGetVersionString;

// ============================================================
// GetVersionString (0x1800C4F90)
// Unpacks a version uint into "major.minor.patch" with optional
// build type suffix. Called by CurrentVersion and by exe UI code.
//
// Packed version format: [major:8][minor:8][patch:8][buildType:8]
//   0x010007FF = version 1.0.7 Release
//
// Build type suffixes:
//   0xFF = Release (no suffix)
//   0xFE = Beta    → "B" or " Beta"
//   0xFD = Alpha   → "A" or " Alpha"
//   0xFC = PreAlpha → "P" or " Pre Alpha"
//   0xFB = Prototype → "R" or " Prototype"
//
// Parameters (MSVC x64 ABI for static returning non-trivial type):
//   RCX = hidden return pointer (String*, 8 bytes)
//   EDX = packed version uint
//   R8B = bool shortSuffix (true = 1-letter, false = full word)
// ============================================================

static void* Reimpl_GetVersionString(void* retval, uint32_t packedVersion, bool shortSuffix) {
    uint8_t buildType = packedVersion & 0xFF;
    uint8_t patch     = (packedVersion >> 8) & 0xFF;
    uint8_t minor     = (packedVersion >> 16) & 0xFF;
    uint8_t major     = (packedVersion >> 24) & 0xFF;

#ifdef _DEBUG
    printf("[Configuration::GetVersionString] packed=0x%08X -> %d.%d.%d type=0x%02X short=%d\n",
           packedVersion, major, minor, patch, buildType, shortSuffix);
#endif

    // Determine suffix for non-release builds
    const char* suffix = nullptr;
    if (buildType != BuildType_Release) {
        switch (buildType) {
            case BuildType_Beta:      suffix = shortSuffix ? "B" : " Beta"; break;
            case BuildType_Alpha:     suffix = shortSuffix ? "A" : " Alpha"; break;
            case BuildType_PreAlpha:  suffix = shortSuffix ? "P" : " Pre Alpha"; break;
            case BuildType_Prototype: suffix = shortSuffix ? "R" : " Prototype"; break;
        }
    }

    // Format the version string in one call
    *(uint64_t*)retval = 0;  // zero-init String (m_data = NULL)
    if (suffix) {
        fn_String_Format(retval, "%i.%i.%i%s", (int)major, (int)minor, (int)patch, suffix);
    } else {
        fn_String_Format(retval, "%i.%i.%i", (int)major, (int)minor, (int)patch);
    }

#ifdef _DEBUG
    char* resultData = *(char**)retval;
    printf("[Configuration::GetVersionString] result=\"%s\"\n", resultData ? resultData : "(null)");
#endif

    return retval;
}

// ============================================================
// CurrentVersion (0x1800C4F40)
// Returns the game's version string: "1.0.7"
// Just a thin wrapper calling GetVersionString(0x010007FF, false).
//
// Parameters:
//   RCX = hidden return pointer (String*)
// ============================================================

static void* Reimpl_CurrentVersion(void* retval) {
#ifdef _DEBUG
    printf("[Configuration::CurrentVersion] called\n");
#endif

    // Banished v1.0.7 Release — packed as 0x010007FF
    return Reimpl_GetVersionString(retval, 0x010007FF, false);
}

// ============================================================
// CurrentBuildId (0x1800C4C60)
// Generates a YYMMDD build ID from the compile date string
// embedded in Runtime-x64.dll's .rdata ("Sep 10 2017" → "170910").
//
// Reads the date string from the DLL's memory so the BuildNumber
// patch (which changes it to "Dec 56 2012") is respected, producing
// "123456" when the patch is active.
//
// Parameters:
//   RCX = hidden return pointer (String*)
// ============================================================

static void* Reimpl_CurrentBuildId(void* retval) {
    // Resolve the compile date string from Runtime-x64.dll .rdata
    const char* buildDate = reinterpret_cast<const char*>(
        ResolveAddr(Module::Runtime, ConfigurationAddr::BuildDateString));

#ifdef _DEBUG
    printf("[Configuration::CurrentBuildId] buildDate=\"%.11s\"\n", buildDate);
#endif

    // Month abbreviation → 2-digit number
    static const char* months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    int month = 0;
    for (int i = 0; i < 12; i++) {
        if (buildDate[0] == months[i][0] &&
            buildDate[1] == months[i][1] &&
            buildDate[2] == months[i][2]) {
            month = i + 1;
            break;
        }
    }

    // Day digits (positions 4-5), space-padded → zero-padded
    char d0 = (buildDate[4] == ' ') ? '0' : buildDate[4];
    char d1 = buildDate[5];

    // Year: last 2 digits (positions 9-10 of "Sep 10 2017")
    char y0 = buildDate[9];
    char y1 = buildDate[10];

    // Build YYMMDD string
    char buildId[7];
    buildId[0] = y0;
    buildId[1] = y1;
    buildId[2] = '0' + (month / 10);
    buildId[3] = '0' + (month % 10);
    buildId[4] = d0;
    buildId[5] = d1;
    buildId[6] = '\0';

    *(uint64_t*)retval = 0;  // zero-init String
    fn_String_Set(retval, buildId);

#ifdef _DEBUG
    printf("[Configuration::CurrentBuildId] result=\"%s\"\n", buildId);
#endif

    return retval;
}

// ============================================================
// Install / Remove
// ============================================================

void InstallConfigCurrentBuildId() {
    uintptr_t target = ResolveAddr(Module::Runtime, ConfigurationAddr::CurrentBuildId);
    // Prologue: mov [rsp+8],rcx(5) + push rbp(1) + push rsi(1) + push rdi(1) + lea rbp,[rsp-47h](5) = 13
    InstallHook(target, (void*)Reimpl_CurrentBuildId, &g_hookCurrentBuildId, 13);
#ifdef _DEBUG
    printf("[Configuration] CurrentBuildId hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveConfigCurrentBuildId() { RemoveHook(&g_hookCurrentBuildId); }

void InstallConfigCurrentVersion() {
    uintptr_t target = ResolveAddr(Module::Runtime, ConfigurationAddr::CurrentVersion);
    // Prologue: mov [rsp+8],rcx(5) + push rbx(1) + sub rsp,30h(4) + mov [rsp+28h],-2(8) = 13 safe
    InstallHook(target, (void*)Reimpl_CurrentVersion, &g_hookCurrentVersion, 13);
#ifdef _DEBUG
    printf("[Configuration] CurrentVersion hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveConfigCurrentVersion() { RemoveHook(&g_hookCurrentVersion); }

void InstallConfigGetVersionString() {
    uintptr_t target = ResolveAddr(Module::Runtime, ConfigurationAddr::GetVersionString);
    // Prologue: mov rax,rsp(3) + mov [rax+8],rcx(4) + push rdi(1) + sub rsp,60h(4) + mov [rax-18h],-2(5) = 13 safe
    InstallHook(target, (void*)Reimpl_GetVersionString, &g_hookGetVersionString, 13);
#ifdef _DEBUG
    printf("[Configuration] GetVersionString hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveConfigGetVersionString() { RemoveHook(&g_hookGetVersionString); }
