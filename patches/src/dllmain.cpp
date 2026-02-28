// Banished Reimplementation - Proxy DLL Entry Point
// This DLL masquerades as version.dll. Drop it next to Application-x64.exe
// and it loads automatically with the game. All version.dll calls are
// forwarded to the real system DLL (see proxy.cpp).
//
// Init sequence:
//   1. ProxyInit() — load real version.dll
//   2. PatchInit() — start background thread that waits for Runtime-x64.dll
//   3. Background thread: ResolveInit() -> EngineFuncsInit() -> InstallHooks()

#include "proxy.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>
#include <string>
#include <windows.h>
#include <dbghelp.h>

// Legacy wrapper — use ResolveAddr(Module::Exe, addr) in new code.
uintptr_t ghidra_addr(uintptr_t ghidra_address) {
    return ResolveAddr(Module::Exe, ghidra_address);
}

// Build an absolute path in the game directory (next to the exe).
// This ensures log files always land in C:/GOG Games/Banished/ regardless of CWD.
static std::string GameDirPath(const char* filename) {
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    // Truncate to directory
    char* lastSlash = strrchr(exePath, '\\');
    if (!lastSlash) lastSlash = strrchr(exePath, '/');
    if (lastSlash) *(lastSlash + 1) = '\0';
    return std::string(exePath) + filename;
}

// Background init thread — waits for Runtime-x64.dll, then resolves everything.
static HANDLE g_initThread = nullptr;
static volatile bool g_initRunning = false;

static DWORD WINAPI InitThread(LPVOID) {
    g_initRunning = true;

    // Poll for Runtime-x64.dll (up to 30 seconds)
    for (int i = 0; i < 300 && g_initRunning; i++) {
        if (ResolveInit()) {
            // Runtime loaded — resolve engine functions
            if (!EngineFuncsInit()) {
#ifdef _DEBUG
                printf("[BanishedPatch] WARNING: EngineFuncsInit failed (non-critical funcs missing)\n");
#endif
            }

            // Install hooks and reimplementation redirects
            InstallHooks();

            g_initRunning = false;
            return 0;
        }
        Sleep(100);
    }

#ifdef _DEBUG
    printf("[BanishedPatch] WARNING: Runtime-x64.dll not loaded after 30s\n");
#endif
    g_initRunning = false;
    return 1;
}

// ============================================================
// Crash handler — catches unhandled exceptions, writes to log
// ============================================================

static const char* ExceptionCodeToString(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:         return "ACCESS_VIOLATION";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_BREAKPOINT:               return "BREAKPOINT";
    case EXCEPTION_DATATYPE_MISALIGNMENT:    return "DATATYPE_MISALIGNMENT";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_OVERFLOW:             return "FLT_OVERFLOW";
    case EXCEPTION_ILLEGAL_INSTRUCTION:      return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR:            return "IN_PAGE_ERROR";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:             return "INT_OVERFLOW";
    case EXCEPTION_PRIV_INSTRUCTION:         return "PRIV_INSTRUCTION";
    case EXCEPTION_STACK_OVERFLOW:           return "STACK_OVERFLOW";
    default:                                 return "UNKNOWN";
    }
}

static LPTOP_LEVEL_EXCEPTION_FILTER g_prevExceptionFilter = nullptr;

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ep) {
    // Write to both stdout (patch_debug.log) and a dedicated crash file
    std::string crashPath = GameDirPath("patch_crash.log");
    FILE* crashFile = fopen(crashPath.c_str(), "w");
    FILE* outputs[] = { stdout, crashFile };
    int numOutputs = crashFile ? 2 : 1;

    for (int o = 0; o < numOutputs; o++) {
        FILE* f = outputs[o];
        if (!f) continue;

        fprintf(f, "\n========================================\n");
        fprintf(f, "[BanishedPatch] *** CRASH DETECTED ***\n");
        fprintf(f, "========================================\n");

        DWORD code = ep->ExceptionRecord->ExceptionCode;
        fprintf(f, "Exception: 0x%08lX (%s)\n", code, ExceptionCodeToString(code));
        fprintf(f, "Address:   %p\n", ep->ExceptionRecord->ExceptionAddress);

        if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
            const char* op = ep->ExceptionRecord->ExceptionInformation[0] == 0 ? "READ" :
                             ep->ExceptionRecord->ExceptionInformation[0] == 1 ? "WRITE" : "EXEC";
            fprintf(f, "Fault:     %s of address %p\n", op,
                    (void*)ep->ExceptionRecord->ExceptionInformation[1]);
        }

        // Registers
        CONTEXT* ctx = ep->ContextRecord;
        fprintf(f, "\nRegisters:\n");
        fprintf(f, "  RAX=%016llX  RBX=%016llX  RCX=%016llX  RDX=%016llX\n",
                (unsigned long long)ctx->Rax, (unsigned long long)ctx->Rbx,
                (unsigned long long)ctx->Rcx, (unsigned long long)ctx->Rdx);
        fprintf(f, "  RSI=%016llX  RDI=%016llX  RBP=%016llX  RSP=%016llX\n",
                (unsigned long long)ctx->Rsi, (unsigned long long)ctx->Rdi,
                (unsigned long long)ctx->Rbp, (unsigned long long)ctx->Rsp);
        fprintf(f, "  R8 =%016llX  R9 =%016llX  R10=%016llX  R11=%016llX\n",
                (unsigned long long)ctx->R8,  (unsigned long long)ctx->R9,
                (unsigned long long)ctx->R10, (unsigned long long)ctx->R11);
        fprintf(f, "  R12=%016llX  R13=%016llX  R14=%016llX  R15=%016llX\n",
                (unsigned long long)ctx->R12, (unsigned long long)ctx->R13,
                (unsigned long long)ctx->R14, (unsigned long long)ctx->R15);
        fprintf(f, "  RIP=%016llX  RFLAGS=%08lX\n",
                (unsigned long long)ctx->Rip, (unsigned long)ctx->EFlags);

        // Module context — which module owns the crash address
        HMODULE hMod = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)ep->ExceptionRecord->ExceptionAddress, &hMod);
        if (hMod) {
            char modName[MAX_PATH];
            GetModuleFileNameA(hMod, modName, MAX_PATH);
            uintptr_t offset = (uintptr_t)ep->ExceptionRecord->ExceptionAddress - (uintptr_t)hMod;
            fprintf(f, "Module:    %s + 0x%llX\n", modName, (unsigned long long)offset);
        }

        // Stack trace using DbgHelp
        fprintf(f, "\nStack trace:\n");
        HANDLE process = GetCurrentProcess();
        SymInitialize(process, NULL, TRUE);

        STACKFRAME64 frame = {};
        frame.AddrPC.Offset    = ctx->Rip;
        frame.AddrPC.Mode      = AddrModeFlat;
        frame.AddrFrame.Offset = ctx->Rbp;
        frame.AddrFrame.Mode   = AddrModeFlat;
        frame.AddrStack.Offset = ctx->Rsp;
        frame.AddrStack.Mode   = AddrModeFlat;

        CONTEXT ctxCopy = *ctx;
        for (int i = 0; i < 32; i++) {
            if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(),
                             &frame, &ctxCopy, NULL,
                             SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
                break;
            }
            if (frame.AddrPC.Offset == 0) break;

            // Try to get module name for this frame
            hMod = nullptr;
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)frame.AddrPC.Offset, &hMod);

            char modShort[64] = "???";
            if (hMod) {
                char modPath[MAX_PATH];
                GetModuleFileNameA(hMod, modPath, MAX_PATH);
                // Extract just the filename
                const char* slash = strrchr(modPath, '\\');
                strncpy(modShort, slash ? slash + 1 : modPath, sizeof(modShort) - 1);
                modShort[sizeof(modShort) - 1] = '\0';
            }

            uintptr_t base = hMod ? (uintptr_t)hMod : 0;
            fprintf(f, "  [%2d] %p  %s+0x%llX\n", i,
                    (void*)frame.AddrPC.Offset, modShort,
                    (unsigned long long)(frame.AddrPC.Offset - base));
        }

        SymCleanup(process);
        fprintf(f, "========================================\n\n");
        fflush(f);
    }

    if (crashFile) fclose(crashFile);

    // Chain to previous handler if any
    if (g_prevExceptionFilter)
        return g_prevExceptionFilter(ep);

    return EXCEPTION_CONTINUE_SEARCH;
}

static void InstallCrashHandler() {
    g_prevExceptionFilter = SetUnhandledExceptionFilter(CrashHandler);
#ifdef _DEBUG
    printf("[BanishedPatch] Crash handler installed\n");
#endif
}

static void RemoveCrashHandler() {
    SetUnhandledExceptionFilter(g_prevExceptionFilter);
    g_prevExceptionFilter = nullptr;
}

static void PatchInit() {
#ifdef _DEBUG
    AllocConsole();
    std::string logPath = GameDirPath("patch_debug.log");
    freopen(logPath.c_str(), "w", stdout);
    freopen("CONOUT$", "w", stderr);
    setvbuf(stdout, NULL, _IONBF, 0);  // unbuffered so we can read in real-time
    printf("[BanishedPatch] Loaded. Starting init thread...\n");
#endif

    InstallCrashHandler();

    // Try immediate init (Runtime-x64.dll might already be loaded)
    if (ResolveInit()) {
        EngineFuncsInit();
        InstallHooks();
    } else {
        // Not loaded yet — start background thread
        g_initThread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    }
}

static void PatchShutdown() {
    // Stop init thread if still running
    g_initRunning = false;
    if (g_initThread) {
        WaitForSingleObject(g_initThread, 2000);
        CloseHandle(g_initThread);
        g_initThread = nullptr;
    }

    RemoveHooks();
    RemoveCrashHandler();
    ProxyShutdown();

#ifdef _DEBUG
    printf("[BanishedPatch] Unloaded.\n");
    FreeConsole();
#endif
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        if (!ProxyInit()) return FALSE;
        PatchInit();
        break;
    case DLL_PROCESS_DETACH:
        PatchShutdown();
        break;
    }
    return TRUE;
}
