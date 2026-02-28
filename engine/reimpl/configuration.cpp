// engine/reimpl/configuration.cpp
// Reimplementation of System::Configuration methods from Runtime-x64.dll.
//
// Static methods returning String by value use MSVC non-trivial return ABI:
// hidden pointer in RCX, even though String is only 8 bytes.
//
// Methods fully reimplemented:
//   - CurrentBuildId       (0x1800C4C60) — parses compile date → YYMMDD build ID
//   - CurrentVersion       (0x1800C4F40) — returns "1.0.7"
//   - GetVersionString     (0x1800C4F90) — formats packed version int
//   - GetPlatformName      (0x1800C5210) — returns platform name string
//   - GetLanguageName      (0x1800C4BC0) — returns language name string
//   - GetBuildName         (0x1800C5280) — returns build name string
//   - GetCommandLineString (0x1800C5300) — AVL tree lookup via Map::Find
//   - SetCommandLine       (0x1800C47A0) — parse + AVL tree insert via ParseCommand/ParseValue
//
// Methods hooked (passthrough wrapper with logging):
//   - Ctor                  (0x1800C51B0) — zero-init + set singleton
//   - Dtor                  (0x1800C5170) — destroy map + cleanup

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
static HookInfo g_hookCtor;
static HookInfo g_hookDtor;
static HookInfo g_hookGetCommandLineString;
static HookInfo g_hookSetCommandLine;
static HookInfo g_hookGetPlatformName;
static HookInfo g_hookGetLanguageName;
static HookInfo g_hookGetBuildName;

// ============================================================
// GetVersionString (0x1800C4F90) — FULL REIMPLEMENTATION
// Unpacks a version uint into "major.minor.patch" with optional suffix.
//
// Parameters (MSVC non-trivial return):
//   RCX = hidden return pointer (String*)
//   EDX = packed version uint
//   R8B = bool shortSuffix
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

    const char* suffix = nullptr;
    if (buildType != BuildType_Release) {
        switch (buildType) {
            case BuildType_Beta:      suffix = shortSuffix ? "B" : " Beta"; break;
            case BuildType_Alpha:     suffix = shortSuffix ? "A" : " Alpha"; break;
            case BuildType_PreAlpha:  suffix = shortSuffix ? "P" : " Pre Alpha"; break;
            case BuildType_Prototype: suffix = shortSuffix ? "R" : " Prototype"; break;
        }
    }

    *(uint64_t*)retval = 0;
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
// CurrentVersion (0x1800C4F40) — FULL REIMPLEMENTATION
// Returns "1.0.7" via GetVersionString(0x010007FF, false).
// ============================================================

static void* Reimpl_CurrentVersion(void* retval) {
#ifdef _DEBUG
    printf("[Configuration::CurrentVersion] called\n");
#endif
    return Reimpl_GetVersionString(retval, 0x010007FF, false);
}

// ============================================================
// CurrentBuildId (0x1800C4C60) — FULL REIMPLEMENTATION
// Parses compile date from .rdata → YYMMDD build ID string.
// ============================================================

static void* Reimpl_CurrentBuildId(void* retval) {
    const char* buildDate = reinterpret_cast<const char*>(
        ResolveAddr(Module::Runtime, ConfigurationAddr::BuildDateString));

#ifdef _DEBUG
    printf("[Configuration::CurrentBuildId] buildDate=\"%.11s\"\n", buildDate);
#endif

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

    char d0 = (buildDate[4] == ' ') ? '0' : buildDate[4];
    char d1 = buildDate[5];
    char y0 = buildDate[9];
    char y1 = buildDate[10];

    char buildId[7];
    buildId[0] = y0;
    buildId[1] = y1;
    buildId[2] = '0' + (month / 10);
    buildId[3] = '0' + (month % 10);
    buildId[4] = d0;
    buildId[5] = d1;
    buildId[6] = '\0';

    *(uint64_t*)retval = 0;
    fn_String_Set(retval, buildId);

#ifdef _DEBUG
    printf("[Configuration::CurrentBuildId] result=\"%s\"\n", buildId);
#endif

    return retval;
}

// ============================================================
// GetPlatformName (0x1800C5210) — FULL REIMPLEMENTATION
// Returns platform name string for the given Platform enum value.
// Only known value: Platform::Win (0) → "Win"
//
// MSVC non-trivial return: RCX=hidden return ptr, RDX=this, R8D=Platform enum
// ============================================================

static void* Reimpl_GetPlatformName(void* retval, void* /*self*/, int platform) {
    *(uint64_t*)retval = 0;
    if (platform == Platform_Win) {
        fn_String_Set(retval, "Win");
    }

#ifdef _DEBUG
    static int count = 0;
    if (count < 3) {
        char* s = *(char**)retval;
        printf("[Configuration::GetPlatformName] platform=%d -> \"%s\"\n",
               platform, s ? s : "(null)");
        count++;
    }
#endif

    return retval;
}

// ============================================================
// GetLanguageName (0x1800C4BC0) — FULL REIMPLEMENTATION
// Returns language name string for the given Language enum value.
// Only known value: Language::English (0) → "English"
//
// MSVC non-trivial return: RCX=hidden return ptr, RDX=this, R8D=Language enum
// ============================================================

static void* Reimpl_GetLanguageName(void* retval, void* /*self*/, int language) {
    *(uint64_t*)retval = 0;
    if (language == Language_English) {
        fn_String_Set(retval, "English");
    }

#ifdef _DEBUG
    static int count = 0;
    if (count < 3) {
        char* s = *(char**)retval;
        printf("[Configuration::GetLanguageName] language=%d -> \"%s\"\n",
               language, s ? s : "(null)");
        count++;
    }
#endif

    return retval;
}

// ============================================================
// GetBuildName (0x1800C5280) — FULL REIMPLEMENTATION
// Returns build name string for the given Build enum value.
// Known values: Build::Data (0) → "Data", Build::Eval (1) → "Eval"
//
// MSVC non-trivial return: RCX=hidden return ptr, RDX=this, R8D=Build enum
// ============================================================

static void* Reimpl_GetBuildName(void* retval, void* /*self*/, int build) {
    *(uint64_t*)retval = 0;
    if (build == Build_Data) {
        fn_String_Set(retval, "Data");
    } else if (build == Build_Eval) {
        fn_String_Set(retval, "Eval");
    }

#ifdef _DEBUG
    static int count = 0;
    if (count < 3) {
        char* s = *(char**)retval;
        printf("[Configuration::GetBuildName] build=%d -> \"%s\"\n",
               build, s ? s : "(null)");
        count++;
    }
#endif

    return retval;
}

// ============================================================
// Constructor (0x1800C51B0) — PASSTHROUGH WRAPPER
// Zero-inits all 0x30 bytes, sets _singleton pointer.
// ============================================================

typedef void* (*Configuration_Ctor_t)(Configuration* self);

static void* Reimpl_Ctor(Configuration* self) {
#ifdef _DEBUG
    printf("[Configuration::Ctor] self=%p\n", (void*)self);
#endif
    void* result = ((Configuration_Ctor_t)g_hookCtor.trampoline)(self);
#ifdef _DEBUG
    printf("[Configuration::Ctor] done, platform=%d language=%d build=%d\n",
           self->m_platform, self->m_language, self->m_build);
#endif
    return result;
}

// ============================================================
// Destructor (0x1800C5170) — PASSTHROUGH WRAPPER
// Destroys command line map and resource parameter list.
// ============================================================

typedef void (*Configuration_Dtor_t)(Configuration* self);

static void Reimpl_Dtor(Configuration* self) {
#ifdef _DEBUG
    printf("[Configuration::Dtor] self=%p\n", (void*)self);
#endif
    ((Configuration_Dtor_t)g_hookDtor.trampoline)(self);
#ifdef _DEBUG
    printf("[Configuration::Dtor] done\n");
#endif
}

// ============================================================
// GetCommandLineString (0x1800C5300) — FULL REIMPLEMENTATION
// Looks up a command-line key in the AVL tree map, returns value as String.
//
// MSVC member non-trivial return: RCX=this, RDX=hidden return ptr, R8=key
// Map::Find returns a BST node: [0]=key.m_data, [8]=value.m_data, ...
// ============================================================

static void* Reimpl_GetCommandLineString(Configuration* self, void* retval, void* key) {
    // Look up key in the AVL tree map at &self->m_commandLineData
    void** node = (void**)fn_Map_Find(&self->m_commandLineData, key);

    // Initialize return String to empty
    *(void**)retval = nullptr;

    if (node != nullptr) {
        // Node[1] = value String's m_data pointer (at offset +0x08 in BST node)
        void* valueData = node[1];
        if (valueData != nullptr) {
            *(void**)retval = valueData;
            // Increment refcount (int32 at m_data - 4)
            *reinterpret_cast<int32_t*>(static_cast<char*>(valueData) - 4) += 1;
        }
    }

#ifdef _DEBUG
    const char* keyStr = key ? *(const char**)key : nullptr;
    char* val = *(char**)retval;
    printf("[Configuration::GetCommandLineString] key=\"%s\" -> \"%s\"\n",
           keyStr ? keyStr : "(null)", val ? val : "(empty)");
#endif

    return retval;
}

// ============================================================
// SetCommandLine (0x1800C47A0) — FULL REIMPLEMENTATION
// Parses "/key value" or "/key:value" pairs from a command-line
// string and inserts them into the AVL tree map.
//
// MSVC member: RCX=this, RDX=commandLine(const String&)
// Uses ParseCommand/ParseValue (static, non-trivial return) to tokenize.
// Uses Map::Insert to add key/value pairs to the AVL tree.
// ============================================================

static void Reimpl_SetCommandLine(Configuration* self, void* commandLine) {
    // Get command line string data and length
    void* cmdData = *(void**)commandLine;
    if (cmdData == nullptr) return;
    int cmdLen = *reinterpret_cast<int32_t*>(static_cast<char*>(cmdData) - 8);
    if (cmdLen <= 0) return;

#ifdef _DEBUG
    printf("[Configuration::SetCommandLine] cmd=\"%s\" len=%d\n",
           (const char*)cmdData, cmdLen);
#endif

    int pos = 0;
    do {
        // Parse next "/command" token (static, non-trivial return: RCX=retval)
        void* cmdStr = nullptr;
        fn_ParseCommand(&cmdStr, &pos, commandLine);

        // Empty/null command = end of parsing
        if (cmdStr == nullptr) return;
        int cmdNameLen = *reinterpret_cast<int32_t*>(static_cast<char*>(cmdStr) - 8);
        if (cmdNameLen == 0) {
            fn_String_Dtor(&cmdStr);
            return;
        }

        // Parse the value token
        void* valStr = nullptr;
        fn_ParseValue(&valStr, &pos, commandLine);

        // Insert key=value into the AVL tree map
        uint64_t nodeTemp = 0;
        fn_Map_Insert(&self->m_commandLineData, &nodeTemp, &cmdStr, &valStr);

        // Destroy temp strings
        fn_String_Dtor(&valStr);
        fn_String_Dtor(&cmdStr);

    } while (pos < cmdLen);

#ifdef _DEBUG
    printf("[Configuration::SetCommandLine] done, mapCount=%d\n", self->m_commandLineCount);
#endif
}

// ============================================================
// Install / Remove
// ============================================================

void InstallConfigCurrentBuildId() {
    uintptr_t target = ResolveAddr(Module::Runtime, ConfigurationAddr::CurrentBuildId);
    InstallHook(target, (void*)Reimpl_CurrentBuildId, &g_hookCurrentBuildId, 13);
#ifdef _DEBUG
    printf("[Configuration] CurrentBuildId hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveConfigCurrentBuildId() { RemoveHook(&g_hookCurrentBuildId); }

void InstallConfigCurrentVersion() {
    uintptr_t target = ResolveAddr(Module::Runtime, ConfigurationAddr::CurrentVersion);
    InstallHook(target, (void*)Reimpl_CurrentVersion, &g_hookCurrentVersion, 13);
#ifdef _DEBUG
    printf("[Configuration] CurrentVersion hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveConfigCurrentVersion() { RemoveHook(&g_hookCurrentVersion); }

void InstallConfigGetVersionString() {
    uintptr_t target = ResolveAddr(Module::Runtime, ConfigurationAddr::GetVersionString);
    InstallHook(target, (void*)Reimpl_GetVersionString, &g_hookGetVersionString, 13);
#ifdef _DEBUG
    printf("[Configuration] GetVersionString hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveConfigGetVersionString() { RemoveHook(&g_hookGetVersionString); }

void InstallConfigCtor() {
    uintptr_t target = ResolveAddr(Module::Runtime, ConfigurationAddr::Ctor);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 13);
#ifdef _DEBUG
    printf("[Configuration] Ctor hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveConfigCtor() { RemoveHook(&g_hookCtor); }

void InstallConfigDtor() {
    uintptr_t target = ResolveAddr(Module::Runtime, ConfigurationAddr::Dtor);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 13);
#ifdef _DEBUG
    printf("[Configuration] Dtor hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveConfigDtor() { RemoveHook(&g_hookDtor); }

void InstallConfigGetCommandLineString() {
    uintptr_t target = ResolveAddr(Module::Runtime, ConfigurationAddr::GetCommandLineString);
    InstallHook(target, (void*)Reimpl_GetCommandLineString, &g_hookGetCommandLineString, 13);
#ifdef _DEBUG
    printf("[Configuration] GetCommandLineString hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveConfigGetCommandLineString() { RemoveHook(&g_hookGetCommandLineString); }

void InstallConfigSetCommandLine() {
    uintptr_t target = ResolveAddr(Module::Runtime, ConfigurationAddr::SetCommandLine);
    InstallHook(target, (void*)Reimpl_SetCommandLine, &g_hookSetCommandLine, 13);
#ifdef _DEBUG
    printf("[Configuration] SetCommandLine hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveConfigSetCommandLine() { RemoveHook(&g_hookSetCommandLine); }

void InstallConfigGetPlatformName() {
    uintptr_t target = ResolveAddr(Module::Runtime, ConfigurationAddr::GetPlatformName);
    InstallHook(target, (void*)Reimpl_GetPlatformName, &g_hookGetPlatformName, 13);
#ifdef _DEBUG
    printf("[Configuration] GetPlatformName hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveConfigGetPlatformName() { RemoveHook(&g_hookGetPlatformName); }

void InstallConfigGetLanguageName() {
    uintptr_t target = ResolveAddr(Module::Runtime, ConfigurationAddr::GetLanguageName);
    InstallHook(target, (void*)Reimpl_GetLanguageName, &g_hookGetLanguageName, 13);
#ifdef _DEBUG
    printf("[Configuration] GetLanguageName hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveConfigGetLanguageName() { RemoveHook(&g_hookGetLanguageName); }

void InstallConfigGetBuildName() {
    uintptr_t target = ResolveAddr(Module::Runtime, ConfigurationAddr::GetBuildName);
    InstallHook(target, (void*)Reimpl_GetBuildName, &g_hookGetBuildName, 13);
#ifdef _DEBUG
    printf("[Configuration] GetBuildName hooked at %p (prologue=13)\n", (void*)target);
#endif
}
void RemoveConfigGetBuildName() { RemoveHook(&g_hookGetBuildName); }
