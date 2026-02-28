// Hook management and reimplementation registry.
// Low-level hooking + build number patch + reimplementation table.

#include "hooks.h"
#include "resolve.h"
#include <cstdio>
#include <cstring>

// ============================================================
// Low-level hook infrastructure (unchanged)
// ============================================================

bool InstallHook(uintptr_t targetAddr, void* detourFunc, HookInfo* info, size_t prologueSize) {
    const size_t JUMP_SIZE = 12; // mov rax, imm64 (10) + jmp rax (2)

    // prologueSize must cover at least the jump and fit in our save buffer
    if (prologueSize < JUMP_SIZE) prologueSize = JUMP_SIZE;
    if (prologueSize > sizeof(info->original)) return false;

    memcpy(info->original, (void*)targetAddr, prologueSize);
    info->target = (void*)targetAddr;
    info->patchSize = prologueSize;

    // Allocate executable trampoline: original bytes + jump back
    info->trampoline = VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!info->trampoline) return false;

    // Copy original prologue bytes to trampoline
    memcpy(info->trampoline, info->original, prologueSize);

    // Append jump back to (target + prologueSize)
    uint8_t* jmpBack = (uint8_t*)info->trampoline + prologueSize;
    jmpBack[0] = 0x48; jmpBack[1] = 0xB8; // mov rax, imm64
    *(uintptr_t*)(jmpBack + 2) = targetAddr + prologueSize;
    jmpBack[10] = 0xFF; jmpBack[11] = 0xE0; // jmp rax

    // Overwrite target with jump to detour (always 12 bytes)
    DWORD oldProtect;
    VirtualProtect((void*)targetAddr, prologueSize, PAGE_EXECUTE_READWRITE, &oldProtect);

    uint8_t* patch = (uint8_t*)targetAddr;
    patch[0] = 0x48; patch[1] = 0xB8; // mov rax, imm64
    *(uintptr_t*)(patch + 2) = (uintptr_t)detourFunc;
    patch[10] = 0xFF; patch[11] = 0xE0; // jmp rax

    // NOP any remaining bytes between the jump and the prologue boundary
    for (size_t i = JUMP_SIZE; i < prologueSize; i++)
        patch[i] = 0x90;

    VirtualProtect((void*)targetAddr, prologueSize, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), (void*)targetAddr, prologueSize);

    return true;
}

void RemoveHook(HookInfo* info) {
    if (!info->target) return;

    DWORD oldProtect;
    VirtualProtect(info->target, info->patchSize, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(info->target, info->original, info->patchSize);
    VirtualProtect(info->target, info->patchSize, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), info->target, info->patchSize);

    if (info->trampoline) {
        VirtualFree(info->trampoline, 0, MEM_RELEASE);
        info->trampoline = nullptr;
    }
    info->target = nullptr;
}

// ============================================================
// Build number patch (data patch, not a hook)
// ============================================================

static bool PatchBuildNumber() {
    HMODULE hRuntime = GetModuleHandleA("Runtime-x64.dll");
    if (!hRuntime) return false;

    BYTE* base = (BYTE*)hRuntime;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    DWORD imageSize = nt->OptionalHeader.SizeOfImage;

    const char* dateTarget = "Sep 10 2017";
    const size_t dateLen = 11;
    BYTE* dateStr = nullptr;

    for (DWORD i = 0; i < imageSize - dateLen; i++) {
        if (memcmp(base + i, dateTarget, dateLen) == 0) {
            dateStr = base + i;
            break;
        }
    }

    if (!dateStr) return false;

    DWORD oldProtect;

    // Patch date string: "Sep 10 2017" -> "Dec 56 2012"
    VirtualProtect(dateStr, 12, PAGE_READWRITE, &oldProtect);
    memcpy(dateStr, "Dec 56 2012", 11);
    VirtualProtect(dateStr, 12, oldProtect, &oldProtect);

    // Patch month number for Dec: "12" -> "34"
    BYTE* decMonthNum = dateStr + 0x70;
    if (decMonthNum[0] == '1' && decMonthNum[1] == '2') {
        VirtualProtect(decMonthNum, 4, PAGE_READWRITE, &oldProtect);
        decMonthNum[0] = '3';
        decMonthNum[1] = '4';
        VirtualProtect(decMonthNum, 4, oldProtect, &oldProtect);
    }

    // Fallback: direct buffer overwrite
    BYTE* buildBuf = dateStr + 12;
    if (memcmp(buildBuf, "170910", 6) == 0) {
        VirtualProtect(buildBuf, 6, PAGE_READWRITE, &oldProtect);
        memcpy(buildBuf, "123456", 6);
        VirtualProtect(buildBuf, 6, oldProtect, &oldProtect);
    }

#ifdef _DEBUG
    printf("[BanishedPatch] Build number patch applied -> 123456\n");
#endif
    return true;
}

static void InstallBuildPatch() { PatchBuildNumber(); }
static void RemoveBuildPatch()  { /* data patch, no undo */ }

// ============================================================
// Reimplementation registry
// ============================================================
// Add entries here as functions are reimplemented.
// Each entry in engine/reimpl/*.cpp exposes Install/Remove functions.

// --- MainLoop reimplementations (engine/reimpl/mainloop.cpp) ---
extern void InstallMainLoopFrame();
extern void RemoveMainLoopFrame();
extern void InstallMainLoopUpdate();
extern void RemoveMainLoopUpdate();
extern void InstallMainLoopDraw();
extern void RemoveMainLoopDraw();
extern void InstallMainLoopSetNextLoop();
extern void RemoveMainLoopSetNextLoop();
extern void InstallMainLoopRun();
extern void RemoveMainLoopRun();
extern void InstallMainLoopIsRunningML();
extern void RemoveMainLoopIsRunningML();
extern void InstallMainLoopMarkFrameUpdateTime();
extern void RemoveMainLoopMarkFrameUpdateTime();
extern void InstallMainLoopAddPostUpdate();
extern void RemoveMainLoopAddPostUpdate();
extern void InstallMainLoopAddPostDraw();
extern void RemoveMainLoopAddPostDraw();
extern void InstallMainLoopRemovePostUpdate();
extern void RemoveMainLoopRemovePostUpdate();
extern void InstallMainLoopRemovePostDraw();
extern void RemoveMainLoopRemovePostDraw();
extern void InstallMainLoopOnVideoErrorStatic();
extern void RemoveMainLoopOnVideoErrorStatic();
extern void InstallMainLoopOnVideoRestartStatic();
extern void RemoveMainLoopOnVideoRestartStatic();
extern void InstallMainLoopCreateVideoCtx();
extern void RemoveMainLoopCreateVideoCtx();
extern void InstallMainLoopReleaseVideoCtx();
extern void RemoveMainLoopReleaseVideoCtx();
extern void InstallMainLoopCtor();
extern void RemoveMainLoopCtor();
extern void InstallMainLoopDtor();
extern void RemoveMainLoopDtor();

// --- UpdateLoop reimplementations (engine/reimpl/updateloop.cpp) ---
extern void InstallUpdateLoopIsRunning();
extern void RemoveUpdateLoopIsRunning();
extern void InstallUpdateLoopCtor();
extern void RemoveUpdateLoopCtor();
extern void InstallUpdateLoopDtor();
extern void RemoveUpdateLoopDtor();

// --- DialogLoaderLoopT reimplementations (engine/reimpl/dialogloaderloop.cpp) ---
extern void InstallDialogLoaderLoopStartup();
extern void RemoveDialogLoaderLoopStartup();
extern void InstallDialogLoaderLoopShutdown();
extern void RemoveDialogLoaderLoopShutdown();
extern void InstallDialogLoaderLoopUpdate();
extern void RemoveDialogLoaderLoopUpdate();
extern void InstallDialogLoaderLoopDraw();
extern void RemoveDialogLoaderLoopDraw();
extern void InstallDialogLoaderLoopLoad();
extern void RemoveDialogLoaderLoopLoad();

// --- DialogLoopT reimplementations (engine/reimpl/dialogloop.cpp) ---
extern void InstallDialogLoopStartup();
extern void RemoveDialogLoopStartup();
extern void InstallDialogLoopShutdown();
extern void RemoveDialogLoopShutdown();
extern void InstallDialogLoopUpdate();
extern void RemoveDialogLoopUpdate();
extern void InstallDialogLoopDraw();
extern void RemoveDialogLoopDraw();
extern void InstallDialogLoopLoad();
extern void RemoveDialogLoopLoad();

// --- PlatformConfigure (engine/reimpl/platformconfigure.cpp) ---
extern void InstallPlatformConfigure();
extern void RemovePlatformConfigure();

// --- Configuration reimplementations (engine/reimpl/configuration.cpp) ---
extern void InstallConfigCurrentBuildId();
extern void RemoveConfigCurrentBuildId();
extern void InstallConfigCurrentVersion();
extern void RemoveConfigCurrentVersion();
extern void InstallConfigGetVersionString();
extern void RemoveConfigGetVersionString();
extern void InstallConfigCtor();
extern void RemoveConfigCtor();
extern void InstallConfigDtor();
extern void RemoveConfigDtor();
extern void InstallConfigGetCommandLineString();
extern void RemoveConfigGetCommandLineString();
extern void InstallConfigSetCommandLine();
extern void RemoveConfigSetCommandLine();
extern void InstallConfigGetPlatformName();
extern void RemoveConfigGetPlatformName();
extern void InstallConfigGetLanguageName();
extern void RemoveConfigGetLanguageName();
extern void InstallConfigGetBuildName();
extern void RemoveConfigGetBuildName();

// --- Modification reimplementations (engine/reimpl/modification.cpp) ---
extern void InstallModificationFind();
extern void RemoveModificationFind();
extern void InstallModificationEnable();
extern void RemoveModificationEnable();
extern void InstallModificationDisableAll();
extern void RemoveModificationDisableAll();
extern void InstallModificationSwap();
extern void RemoveModificationSwap();
extern void InstallModificationEnableSavingSettings();
extern void RemoveModificationEnableSavingSettings();
extern void InstallModificationSaveList();
extern void RemoveModificationSaveList();
extern void InstallModificationRefreshList();
extern void RemoveModificationRefreshList();
extern void InstallModificationSetupFileSystem();
extern void RemoveModificationSetupFileSystem();

// --- LoaderLoop reimplementations (engine/reimpl/loaderloop.cpp) ---
extern void InstallLoaderLoopStartup();
extern void RemoveLoaderLoopStartup();
extern void InstallLoaderLoopShutdown();
extern void RemoveLoaderLoopShutdown();
extern void InstallLoaderLoopUpdate();
extern void RemoveLoaderLoopUpdate();
extern void InstallLoaderLoopDraw();
extern void RemoveLoaderLoopDraw();
extern void InstallLoaderLoopExit();
extern void RemoveLoaderLoopExit();
extern void InstallLoaderLoopIsFinished();
extern void RemoveLoaderLoopIsFinished();
extern void InstallLoaderLoopOnVideoError();
extern void RemoveLoaderLoopOnVideoError();

static ReimplEntry g_reimplementations[] = {
    // --- Data patches ---
    { "BuildNumber",           InstallBuildPatch,          RemoveBuildPatch,          true  },

    // --- Launcher skip ---
    { "PlatformConfigure",     InstallPlatformConfigure,   RemovePlatformConfigure,   true  },

    // --- MainLoop reimplementations ---
    { "MainLoop::Frame",       InstallMainLoopFrame,       RemoveMainLoopFrame,       true  },
    { "MainLoop::Update",      InstallMainLoopUpdate,      RemoveMainLoopUpdate,      true  },
    { "MainLoop::Draw",        InstallMainLoopDraw,        RemoveMainLoopDraw,        true  },
    { "MainLoop::SetNextLoop", InstallMainLoopSetNextLoop, RemoveMainLoopSetNextLoop, true  },
    { "MainLoop::Run",         InstallMainLoopRun,         RemoveMainLoopRun,         true  },
    { "MainLoop::IsRunning",   InstallMainLoopIsRunningML, RemoveMainLoopIsRunningML, true  },
    { "MainLoop::MarkFrameUpdateTime", InstallMainLoopMarkFrameUpdateTime, RemoveMainLoopMarkFrameUpdateTime, true },
    { "MainLoop::AddPostUpdate",    InstallMainLoopAddPostUpdate,    RemoveMainLoopAddPostUpdate,    true  },
    { "MainLoop::AddPostDraw",      InstallMainLoopAddPostDraw,      RemoveMainLoopAddPostDraw,      true  },
    { "MainLoop::RemovePostUpdate", InstallMainLoopRemovePostUpdate, RemoveMainLoopRemovePostUpdate, true  },
    { "MainLoop::RemovePostDraw",   InstallMainLoopRemovePostDraw,   RemoveMainLoopRemovePostDraw,   true  },
    { "MainLoop::OnVideoError_s",   InstallMainLoopOnVideoErrorStatic,   RemoveMainLoopOnVideoErrorStatic,   true },
    { "MainLoop::OnVideoRestart_s", InstallMainLoopOnVideoRestartStatic, RemoveMainLoopOnVideoRestartStatic, true },
    { "MainLoop::CreateVideoCtx",   InstallMainLoopCreateVideoCtx,   RemoveMainLoopCreateVideoCtx,   true  },
    { "MainLoop::ReleaseVideoCtx",  InstallMainLoopReleaseVideoCtx,  RemoveMainLoopReleaseVideoCtx,  true  },
    { "MainLoop::Ctor",             InstallMainLoopCtor,             RemoveMainLoopCtor,             true  },
    { "MainLoop::Dtor",             InstallMainLoopDtor,             RemoveMainLoopDtor,             true  },

    // --- UpdateLoop reimplementations ---
    { "UpdateLoop::IsRunning", InstallUpdateLoopIsRunning, RemoveUpdateLoopIsRunning, true },
    { "UpdateLoop::Ctor",      InstallUpdateLoopCtor,      RemoveUpdateLoopCtor,      true },
    { "UpdateLoop::Dtor",      InstallUpdateLoopDtor,      RemoveUpdateLoopDtor,      true },

    // --- DialogLoaderLoopT reimplementations ---
    { "DialogLoaderLoopT::Startup",  InstallDialogLoaderLoopStartup,  RemoveDialogLoaderLoopStartup,  true  },
    { "DialogLoaderLoopT::Shutdown", InstallDialogLoaderLoopShutdown, RemoveDialogLoaderLoopShutdown, true  },
    { "DialogLoaderLoopT::Update",   InstallDialogLoaderLoopUpdate,   RemoveDialogLoaderLoopUpdate,   true  },
    { "DialogLoaderLoopT::Draw",     InstallDialogLoaderLoopDraw,     RemoveDialogLoaderLoopDraw,     true  },
    { "DialogLoaderLoopT::Load",     InstallDialogLoaderLoopLoad,     RemoveDialogLoaderLoopLoad,     true  },

    // --- DialogLoopT reimplementations ---
    { "DialogLoopT::Startup",  InstallDialogLoopStartup,  RemoveDialogLoopStartup,  true  },
    { "DialogLoopT::Shutdown", InstallDialogLoopShutdown, RemoveDialogLoopShutdown, true  },
    { "DialogLoopT::Update",   InstallDialogLoopUpdate,   RemoveDialogLoopUpdate,   true  },
    { "DialogLoopT::Draw",     InstallDialogLoopDraw,     RemoveDialogLoopDraw,     true  },
    { "DialogLoopT::Load",     InstallDialogLoopLoad,     RemoveDialogLoopLoad,     true  },

    // --- LoaderLoop reimplementations ---
    { "LoaderLoop::Startup",      InstallLoaderLoopStartup,      RemoveLoaderLoopStartup,      true  },
    { "LoaderLoop::Shutdown",     InstallLoaderLoopShutdown,      RemoveLoaderLoopShutdown,      true  },
    { "LoaderLoop::Update",       InstallLoaderLoopUpdate,        RemoveLoaderLoopUpdate,        true  },
    { "LoaderLoop::Draw",         InstallLoaderLoopDraw,          RemoveLoaderLoopDraw,          true  },
    { "LoaderLoop::Exit",         InstallLoaderLoopExit,          RemoveLoaderLoopExit,          true  },
    { "LoaderLoop::IsFinished",   InstallLoaderLoopIsFinished,    RemoveLoaderLoopIsFinished,    true  },
    { "LoaderLoop::OnVideoError", InstallLoaderLoopOnVideoError,  RemoveLoaderLoopOnVideoError,  true  },

    // --- Configuration reimplementations ---
    { "Configuration::CurrentBuildId",   InstallConfigCurrentBuildId,   RemoveConfigCurrentBuildId,   true  },
    { "Configuration::CurrentVersion",   InstallConfigCurrentVersion,   RemoveConfigCurrentVersion,   true  },
    { "Configuration::GetVersionString", InstallConfigGetVersionString, RemoveConfigGetVersionString, true  },
    { "Configuration::Ctor",             InstallConfigCtor,             RemoveConfigCtor,             true  },
    { "Configuration::Dtor",             InstallConfigDtor,             RemoveConfigDtor,             true  },
    { "Configuration::GetCommandLineString", InstallConfigGetCommandLineString, RemoveConfigGetCommandLineString, true },
    { "Configuration::SetCommandLine",   InstallConfigSetCommandLine,   RemoveConfigSetCommandLine,   true  },
    { "Configuration::GetPlatformName",  InstallConfigGetPlatformName,  RemoveConfigGetPlatformName,  true  },
    { "Configuration::GetLanguageName",  InstallConfigGetLanguageName,  RemoveConfigGetLanguageName,  true  },
    { "Configuration::GetBuildName",     InstallConfigGetBuildName,     RemoveConfigGetBuildName,     true  },

    // --- Modification reimplementations ---
    { "Modification::Find",       InstallModificationFind,       RemoveModificationFind,       true  },
    { "Modification::Enable",     InstallModificationEnable,     RemoveModificationEnable,     true  },
    { "Modification::DisableAll", InstallModificationDisableAll, RemoveModificationDisableAll, true  },
    { "Modification::Swap",       InstallModificationSwap,       RemoveModificationSwap,       true  },
    { "Modification::EnableSavingSettings", InstallModificationEnableSavingSettings, RemoveModificationEnableSavingSettings, true },
    { "Modification::SaveList",        InstallModificationSaveList,        RemoveModificationSaveList,        true },
    { "Modification::RefreshList",     InstallModificationRefreshList,     RemoveModificationRefreshList,     true },
    { "Modification::SetupFileSystem", InstallModificationSetupFileSystem, RemoveModificationSetupFileSystem, true },

    // Add new entries as you reimplement functions...
};

static constexpr int g_reimplCount = sizeof(g_reimplementations) / sizeof(g_reimplementations[0]);

void InstallHooks() {
#ifdef _DEBUG
    printf("[BanishedPatch] Installing %d hooks/patches...\n", g_reimplCount);
#endif

    for (int i = 0; i < g_reimplCount; i++) {
        auto& r = g_reimplementations[i];
        if (r.enabled) {
            r.install();
#ifdef _DEBUG
            printf("[BanishedPatch]   [%d] %s: installed\n", i, r.name);
#endif
        } else {
#ifdef _DEBUG
            printf("[BanishedPatch]   [%d] %s: DISABLED\n", i, r.name);
#endif
        }
    }

#ifdef _DEBUG
    printf("[BanishedPatch] All hooks installed.\n");
#endif
}

void RemoveHooks() {
#ifdef _DEBUG
    printf("[BanishedPatch] Removing hooks...\n");
#endif

    for (int i = g_reimplCount - 1; i >= 0; i--) {
        g_reimplementations[i].remove();
    }

#ifdef _DEBUG
    printf("[BanishedPatch] Hooks removed.\n");
#endif
}
