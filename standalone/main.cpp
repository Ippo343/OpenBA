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
// Mirrors Runtime DLL's BaseTypeInfo struct layout (used by both the probe
// and the External::Load crash diagnostic).
// ============================================================
struct ProbeBaseTypeInfo {
    uint32_t typeId;
    uint32_t pad;
    const char* name;
    const char* parent;
    const char* abstract;
    void* factoryFn;
    void* inPlaceFn;
    ProbeBaseTypeInfo* next;
};

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

// Walk the saved CONTEXT via the x64 unwind tables and print one line per
// frame: <module>!<offset>. No symbol names — Banished ships stripped — but
// module+offset is what we'd paste into Ghidra anyway.
static void PrintStackTrace(CONTEXT* origCtx) {
    fprintf(stderr, "Stack trace:\n");
    CONTEXT ctx = *origCtx;

    for (int frame = 0; frame < 32; frame++) {
        HMODULE mod = nullptr;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)(uintptr_t)ctx.Rip, &mod) && mod) {
            char path[MAX_PATH] = {0};
            GetModuleFileNameA(mod, path, MAX_PATH);
            const char* nm = strrchr(path, '\\');
            uintptr_t off = (uintptr_t)ctx.Rip - (uintptr_t)mod;
            fprintf(stderr, "  #%-2d %-22s + 0x%-8llX  (RIP=%016llX RSP=%016llX)\n",
                    frame, nm ? nm + 1 : path, (unsigned long long)off,
                    (unsigned long long)ctx.Rip, (unsigned long long)ctx.Rsp);
        } else {
            fprintf(stderr, "  #%-2d ???                    @ %p          (RSP=%016llX)\n",
                    frame, (void*)ctx.Rip, (unsigned long long)ctx.Rsp);
        }

        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(ctx.Rip, &imageBase, nullptr);
        if (!fn) {
            // Leaf or no unwind data — try one manual pop of the return address.
            // Many leaves have no .pdata entry; the return value lives at [RSP].
            uintptr_t* sp = reinterpret_cast<uintptr_t*>(ctx.Rsp);
            if (IsBadReadPtr(sp, sizeof(uintptr_t))) break;
            ctx.Rip = *sp;
            ctx.Rsp += 8;
            if (!ctx.Rip) break;
            continue;
        }

        PVOID handlerData = nullptr;
        ULONG64 establisherFrame = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, ctx.Rip, fn,
                         &ctx, &handlerData, &establisherFrame, nullptr);

        if (!ctx.Rip) break;
    }
    fflush(stderr);
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
    fprintf(stderr, "RAX=%016llX RCX=%016llX RDX=%016llX RBX=%016llX\n",
            (unsigned long long)ctx->Rax, (unsigned long long)ctx->Rcx,
            (unsigned long long)ctx->Rdx, (unsigned long long)ctx->Rbx);
    fprintf(stderr, "RBP=%016llX RSI=%016llX RDI=%016llX R8 =%016llX\n",
            (unsigned long long)ctx->Rbp, (unsigned long long)ctx->Rsi,
            (unsigned long long)ctx->Rdi, (unsigned long long)ctx->R8);
    fprintf(stderr, "R9 =%016llX R10=%016llX R11=%016llX R12=%016llX\n",
            (unsigned long long)ctx->R9,  (unsigned long long)ctx->R10,
            (unsigned long long)ctx->R11, (unsigned long long)ctx->R12);
    fprintf(stderr, "RIP=%016llX RSP=%016llX\n",
            (unsigned long long)ctx->Rip, (unsigned long long)ctx->Rsp);

    // Identify the module containing RIP — saves us doing arithmetic by hand.
    {
        HMODULE mod = nullptr;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)(uintptr_t)ctx->Rip, &mod) && mod) {
            char path[MAX_PATH] = {0};
            GetModuleFileNameA(mod, path, MAX_PATH);
            uintptr_t off = (uintptr_t)ctx->Rip - (uintptr_t)mod;
            const char* nm = strrchr(path, '\\');
            fprintf(stderr, "Module:    %s + 0x%llX  (base=%p)\n",
                    nm ? nm + 1 : path, (unsigned long long)off, (void*)mod);
        } else {
            fprintf(stderr, "Module:    UNKNOWN (no module covers RIP)\n");
        }
    }

    if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
        const char* op = ep->ExceptionRecord->ExceptionInformation[0] == 0 ? "READ" :
                         ep->ExceptionRecord->ExceptionInformation[0] == 1 ? "WRITE" : "EXEC";
        fprintf(stderr, "Fault: %s of %p\n", op,
                (void*)ep->ExceptionRecord->ExceptionInformation[1]);
    }

    // Specialized handler: IsDerivedFrom-NULL-this crash. RIP at Runtime+0xC039A
    // is the very first vtable read with RCX=NULL (factory returned NULL).
    // RDX at that point holds the EXPECTED typeId (the External base check).
    uintptr_t runtimeBase = reinterpret_cast<uintptr_t>(GetModuleHandleA("Runtime-x64.dll"));
    if (runtimeBase && ctx->Rcx == 0 && ctx->Rip >= runtimeBase + 0xC0390 && ctx->Rip <= runtimeBase + 0xC03A0) {
        uint32_t expectedTypeId = (uint32_t)ctx->Rdx;
        fprintf(stderr, "IsDerivedFrom NULL-this: factory returned NULL.\n");
        fprintf(stderr, "  Expected base typeId in RDX = 0x%08X\n", expectedTypeId);
        uintptr_t headAddr = ResolveAddr(Module::Runtime, 0x1801fc6e0);
        ProbeBaseTypeInfo* head = headAddr ? *reinterpret_cast<ProbeBaseTypeInfo**>(headAddr) : nullptr;
        for (ProbeBaseTypeInfo* p = head; p; p = p->next) {
            if (p->typeId == expectedTypeId) {
                fprintf(stderr, "  Expected base = %s (parent=%s, factory=%p)\n",
                        p->name ? p->name : "?", p->parent ? p->parent : "?", p->factoryFn);
                break;
            }
        }
    }

    // Specialized handler: External::Load tree-walk crash. The instruction at
    // Runtime+0xBC73E is `MOV RAX, [RCX+0x10]` with RCX=NULL because the
    // typeId in [RBP+0x48] wasn't found in Factory::_singleton's tree.
    // Print the typeId and try to find a Runtime type whose CRC32 matches.
    if (runtimeBase && ctx->Rip == runtimeBase + 0xBC73E) {
        uint32_t* typeIdSlot = reinterpret_cast<uint32_t*>(ctx->Rbp + 0x48);
        // [RBP+0x48] is on the crashed thread's stack — same memory we just
        // tree-walked through, so the read should be safe. IsBadReadPtr is
        // racy but fine for a one-shot diagnostic.
        if (!IsBadReadPtr(typeIdSlot, sizeof(uint32_t))) {
            uint32_t typeId = *typeIdSlot;
            fprintf(stderr, "External::Load: missed typeId=0x%08X at [RBP+0x48]=%p\n",
                    typeId, (void*)typeIdSlot);

            uintptr_t headAddr = ResolveAddr(Module::Runtime, 0x1801fc6e0);
            ProbeBaseTypeInfo* head = headAddr ? *reinterpret_cast<ProbeBaseTypeInfo**>(headAddr) : nullptr;
            bool matched = false;
            for (ProbeBaseTypeInfo* p = head; p; p = p->next) {
                if (p->typeId == typeId) {
                    fprintf(stderr, "  -> matches REGISTERED type: %s (parent=%s)\n",
                            p->name ? p->name : "?", p->parent ? p->parent : "?");
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                fprintf(stderr, "  -> typeId is NOT in _firstTypeInfo (missing exe-side registration)\n");
            }
        } else {
            fprintf(stderr, "External::Load: [RBP+0x48]=%p is unreadable\n", (void*)typeIdSlot);
        }
    }

    // Check registry stats
    RegistryStats stats = GetRegistryStats();
    fprintf(stderr, "Registry: %zu registered, %zu stubs hit\n",
            stats.registered, stats.stubs);

    PrintStackTrace(ctx);

    fprintf(stderr, "=== END CRASH ===\n");
    fflush(stderr);

    return EXCEPTION_CONTINUE_SEARCH;
}

// ============================================================
// PROBE: walk Runtime's _firstTypeInfo (head @ Runtime 0x1801fc6e0).
// BaseTypeInfo struct layout: +0x08 name, +0x10 parent, +0x20 factoryFn,
// +0x30 next. typeId at +0x00 is 0xFFFFFFFF until Factory::Startup runs.
// ============================================================
static void DumpFirstTypeInfo(const char* tag) {
    uintptr_t headAddr = ResolveAddr(Module::Runtime, 0x1801fc6e0);
    if (!headAddr) {
        printf("[Probe %s] Could not resolve _firstTypeInfo head address\n", tag);
        return;
    }
    ProbeBaseTypeInfo* head = *reinterpret_cast<ProbeBaseTypeInfo**>(headAddr);
    int n = 0;
    printf("[Probe %s] _firstTypeInfo @ 0x%llx, head=%p\n",
           tag, (unsigned long long)headAddr, (void*)head);
    for (ProbeBaseTypeInfo* p = head; p != nullptr && n < 1000; p = p->next, n++) {
        if (n < 5) {
            printf("[Probe %s]   #%-3d name=%-32s parent=%s\n",
                   tag, n,
                   p->name ? p->name : "(null)",
                   p->parent ? p->parent : "(null)");
        }
    }
    printf("[Probe %s] Total: %d entries\n", tag, n);
}

// ============================================================
// Patch Engine::Engine::PlatformConfigure (Runtime 0x1800a1670) to always
// return true. Mirrors the DLL-mode hook in engine/reimpl/platformconfigure.cpp:
// the launcher's "first run / configure" dialog enables a video-mode probe
// path that crashes inside VideoDX9+0x267E. Skipping it pins the loader to
// the deterministic StatGoal-and-beyond code path.
// ============================================================
static void SkipPlatformConfigure() {
    uintptr_t addr = ResolveAddr(Module::Runtime, 0x1800a1670);
    if (!addr) {
        printf("[Standalone] WARN: could not resolve PlatformConfigure — popup may appear\n");
        return;
    }
    // MOV EAX, 1 ; RET  (6 bytes). Caller checks return as bool, so EAX=1 satisfies
    // both byte-test and dword-test paths. Overwrites the SEH-cookie prologue, but
    // we never unwind through this function so .pdata mismatch is harmless.
    static const uint8_t patch[] = { 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3 };
    DWORD oldProtect = 0;
    if (!VirtualProtect((LPVOID)addr, sizeof(patch), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        printf("[Standalone] WARN: VirtualProtect failed for PlatformConfigure (err=%lu)\n",
               GetLastError());
        return;
    }
    memcpy((void*)addr, patch, sizeof(patch));
    DWORD dummy = 0;
    VirtualProtect((LPVOID)addr, sizeof(patch), oldProtect, &dummy);
    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)addr, sizeof(patch));
    printf("[Standalone] PlatformConfigure (%p) patched to return true (launcher skipped)\n",
           (void*)addr);
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

    // 5. PROBE: dump _firstTypeInfo state right after Runtime DLL load.
    //    Confirms Runtime self-registered its types via inlined CRT-init stubs
    //    at 0x180147xxx. Should print 150+ Runtime types (Dialog, Description,
    //    Element subclasses, etc.).
    DumpFirstTypeInfo("BEFORE InvokeAllRegistrations");

    // 6. Drain pending Factory registrations into Runtime _firstTypeInfo.
    //    Static-init ctors collected them; now we flush by calling
    //    Object::BaseTypeInfo::BaseTypeInfo for each. Engine::Engine::Startup
    //    -> Object::Factory::Startup will later walk the list and populate
    //    the typeId tree.
    printf("[Standalone] Invoking factory registrations...\n");
    Object::InvokeAllRegistrations();

    DumpFirstTypeInfo("AFTER InvokeAllRegistrations");

    // 7. Skip the launcher dialog — see SkipPlatformConfigure note above.
    SkipPlatformConfigure();

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
