// Standalone entry point for Banished reimplementation.
// Replaces Application-x64.exe — loads Runtime-x64.dll + VideoDX11-x64.dll
// and runs the game loop using our reimplemented code.
//
// Build with: make standalone
// Deploy to: C:/GOG Games/Banished/Application-x64.exe (rename original first!)
//
// This is the long-term goal. Initially many functions will hit NOT_IMPLEMENTED
// stubs. As the address registry fills in, more gameplay systems come alive.

#ifdef STANDALONE_EXE

#include "resolve.h"
#include "engine_funcs.h"
#include "runtime_funcs.h"
#include "address_registry.h"
#include "Game/game_main.h"
#include "Object/factory_registration.h"
#include <cstdio>
#include <cstdint>
#include <windows.h>
#include <dbghelp.h>

// ============================================================
// GPU preference exports (match original exe)
// ============================================================
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance = 1;
}

// ============================================================
// Crash handler (simplified version of DLL mode's VEH)
// ============================================================
static const char* ExceptionCodeToString(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:    return "ACCESS_VIOLATION";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_STACK_OVERFLOW:      return "STACK_OVERFLOW";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:  return "INT_DIVIDE_BY_ZERO";
    default:                            return "UNKNOWN";
    }
}

static LONG CALLBACK CrashHandler(EXCEPTION_POINTERS* ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION &&
        code != EXCEPTION_ILLEGAL_INSTRUCTION &&
        code != EXCEPTION_STACK_OVERFLOW) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    fprintf(stderr, "\n=== STANDALONE CRASH ===\n");
    fprintf(stderr, "Exception: 0x%08lX (%s)\n", code, ExceptionCodeToString(code));
    fprintf(stderr, "Address:   %p\n", ep->ExceptionRecord->ExceptionAddress);

    CONTEXT* ctx = ep->ContextRecord;
    fprintf(stderr, "RAX=%016llX RCX=%016llX RDX=%016llX\n",
            (unsigned long long)ctx->Rax, (unsigned long long)ctx->Rcx,
            (unsigned long long)ctx->Rdx);
    fprintf(stderr, "RIP=%016llX RSP=%016llX\n",
            (unsigned long long)ctx->Rip, (unsigned long long)ctx->Rsp);

    if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
        const char* op = ep->ExceptionRecord->ExceptionInformation[0] == 0 ? "READ" :
                         ep->ExceptionRecord->ExceptionInformation[0] == 1 ? "WRITE" : "EXEC";
        fprintf(stderr, "Fault: %s of %p\n", op,
                (void*)ep->ExceptionRecord->ExceptionInformation[1]);
    }

    // Check registry stats
    RegistryStats stats = GetRegistryStats();
    fprintf(stderr, "Registry: %zu registered, %zu stubs hit\n",
            stats.registered, stats.stubs);

    fprintf(stderr, "=== END CRASH ===\n");
    fflush(stderr);

    return EXCEPTION_CONTINUE_SEARCH;
}

// ============================================================
// Initialization sequence
// ============================================================
static bool Init() {
    // 1. Install crash handler
    AddVectoredExceptionHandler(1, CrashHandler);

    // 2. Resolve Runtime-x64.dll and VideoDX11-x64.dll (ASLR)
    printf("[Standalone] Resolving DLLs...\n");
    if (!ResolveInit()) {
        fprintf(stderr, "[Standalone] FATAL: Could not load Runtime-x64.dll\n");
        fprintf(stderr, "[Standalone] Make sure Runtime-x64.dll is in the same directory.\n");
        return false;
    }

    // 3. Populate address registry (exe dispatch slots, code fns, vtables).
    //    Must happen BEFORE EngineFuncsInit, which reads from these slots.
    printf("[Standalone] Initializing address registry...\n");
    AddressRegistryInit();

    // 4. Resolve engine function pointers from Runtime DLL
    printf("[Standalone] Resolving engine functions...\n");
    if (!EngineFuncsInit()) {
        printf("[Standalone] WARNING: Some engine functions not resolved (non-critical)\n");
    }
    RuntimeFuncsInit();

    // 5. Drain pending Factory registrations into Runtime _firstTypeInfo.
    //    Static-init ctors collected them; now we flush by calling
    //    Object::BaseTypeInfo::BaseTypeInfo for each. Engine::Engine::Startup
    //    -> Object::Factory::Startup will later walk the list and populate
    //    the typeId tree.
    printf("[Standalone] Invoking factory registrations...\n");
    Object::InvokeAllRegistrations();

    printf("[Standalone] Init complete.\n");
    return true;
}

// ============================================================
// Entry point
// ============================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int) {
    // Redirect stdout/stderr to log file and console
    AllocConsole();
    freopen("standalone_debug.log", "w", stdout);
    freopen("standalone_error.log", "w", stderr);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("[Standalone] Banished Reimplementation - Standalone EXE\n");
    printf("[Standalone] Build: %s %s\n", __DATE__, __TIME__);

    if (!Init()) {
        MessageBoxA(nullptr,
            "Failed to initialize. Check standalone_debug.log for details.\n"
            "Make sure Runtime-x64.dll and VideoDX11-x64.dll are in this directory.",
            "Banished Standalone - Init Failed", MB_OK | MB_ICONERROR);
        return 1;
    }

    printf("\n[Standalone] === INIT SUCCESS ===\n");
    AddressRegistryPrintStats();
    RegistryStats stats = GetRegistryStats();
    printf("[Standalone] Coverage: %zu / 582 exe addresses registered (%.1f%%)\n",
           stats.registered, stats.registered * 100.0 / 582.0);

    printf("\n[Standalone] === ENTERING GameMain ===\n");
    int rc = Game::RunGameMain(hInstance, lpCmdLine ? lpCmdLine : "");
    printf("[Standalone] === GameMain returned %d ===\n", rc);

    FreeConsole();
    return rc;
}

#endif // STANDALONE_EXE
