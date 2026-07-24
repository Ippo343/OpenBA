// Standalone entry point for Banished reimplementation.
// Replaces Application-x64.exe — loads Runtime-x64.dll + VideoDX11-x64.dll
// and runs the game loop using our reimplemented code.
//
// Build with: make standalone
// Deploy to: C:/GOG Games/Banished/Application-x64.exe (rename original first!)
//
// This is the long-term goal. Initially many functions will hit NOT_IMPLEMENTED
// stubs. As the address registry fills in, more gameplay systems come alive.


#include "resolve.h"
#include "engine_funcs.h"
#include "runtime_funcs.h"
#include "dx11_funcs.h"
#include "address_registry.h"
#include "renderdoc_integration.h"
#include "stall_debug.h"
#include "autostart.h"
#include "redzone_alloc.h"
#include "allocator_lock.h"
#include "Game/game_main.h"
#include "Object/factory_registration.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <exception>
#include <io.h>      // _get_osfhandle, _fileno
#include <windows.h>
#include <dbghelp.h>

// ============================================================
// GPU preference exports (match original exe)
// ============================================================
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance = 1;
}

// Session-3 diagnostic installer (standalone/filename_dtor_check.cpp).
extern "C" void InstallFilenameDtorCheck();
// Session-4 diagnostic installer (standalone/filename_runtime_hook.cpp).
extern "C" void InstallFilenameRuntimeHook();
// Session-5 diagnostic installer (standalone/filename_runtime_hook.cpp).
extern "C" void InstallFindOrLoadWatch();
// Accelerator B (Phase 0): CompressedStream::Read zero-progress guard
// (standalone/filename_runtime_hook.cpp). Gate BANISHED_READ_GUARD=1.
extern "C" void InstallReadGuard();

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
// Reset Video* registry keys to known-safe values BEFORE Engine::Startup.
//
// Banished stores all settings under HKCU\Software\Shining Rock Software LLC
// \Banished. The standalone's incomplete shutdown path writes garbage back —
// VideoWindowWidth/Height end up as 0, VideoSettings flips to "true" — which
// makes ValidateConfiguration override the swap-chain dims to 120x0 on the
// next boot. DX11 then fails to create a B8G8R8A8 color buffer of that size,
// Engine::Startup returns 0, and we never reach the loader thread.
//
// Resetting WindowWidth/Height/X/Y to -1 ("skip override") and VideoSettings
// to "false" each boot breaks the cycle without touching Persistence::Save.
// Once standalone has a real shutdown path, this can go away.
// ============================================================
static void ResetVideoRegistry() {
    HKEY hKey = nullptr;
    LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER,
                            L"Software\\Shining Rock Software LLC\\Banished",
                            0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey);
    if (rc != ERROR_SUCCESS) {
        printf("[Standalone] WARN: ResetVideoRegistry: RegOpenKeyExW failed (%ld)\n", rc);
        return;
    }

    // Banished stores ALL settings as REG_SZ (strings) and parses them with
    // atoi. Writing REG_DWORD silently fails — Persistence::Load only reads
    // strings, so DWORD-typed keys appear absent and the engine keeps stale
    // values. Always write strings.
    auto setStr = [&](const wchar_t* name, const wchar_t* val) {
        DWORD bytes = (DWORD)((wcslen(val) + 1) * sizeof(wchar_t));
        RegSetValueExW(hKey, name, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(val), bytes);
    };

    setStr(L"VideoWindowWidth",  L"1280");
    setStr(L"VideoWindowHeight", L"720");
    setStr(L"VideoWindowX",      L"100");
    setStr(L"VideoWindowY",      L"100");

    // Force DX11 (interface index 1). DX9 (index 0) loses its device on idle
    // events (TestCooperativeLevel → D3DERR_DEVICELOST), which manifests as
    // BeginFrame returning false → MenuDLT::DrawFailed loop.
    setStr(L"VideoInterface", L"1");

    setStr(L"VideoSettings", L"true");

    // Windowed-mode toggle — flip to "false" for windowed (DWM-composited),
    // "true" for exclusive fullscreen. Used to A/B test whether splash flashes
    // come from DWM compositor invalidating the window during ~125ms+ stalls.
    // Windowed mode keeps DWM in the loop (so any DWM-paint glitches show);
    // fullscreen bypasses DWM (so any glitches must come from our code).
    const wchar_t* fullscreen = L"false";  // <-- change to L"true" to revert
    setStr(L"VideoFullscreen", fullscreen);

    RegCloseKey(hKey);
    printf("[Standalone] ResetVideoRegistry: VideoSettings=true VideoFullscreen=%ls "
           "VideoInterface=1 Window=1280x720 (all REG_SZ)\n", fullscreen);
}

// ============================================================
// Crash handler (simplified VEH)
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

// Forward decl — declared above the crash log helpers but defined below.
static void CrashLogf(const char* fmt, ...);
// Forward decl — defined alongside the focus-event message hook, used from
// RunStartupSelfTests and CallWndProcHook.
static void DumpSwapChainState(const char* tag);
static void CaptureBootFullscreenMode();

// Walk the saved CONTEXT via the x64 unwind tables and print one line per
// frame: <module>!<offset>. No symbol names — Banished ships stripped — but
// module+offset is what we'd paste into Ghidra anyway. Each line goes to
// the dedicated crash log via CrashLogf, which writes synchronously.
static void PrintStackTrace(CONTEXT* origCtx) {
    CrashLogf("Stack trace:\n");
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
            CrashLogf("  #%-2d %-22s + 0x%-8llX  (RIP=%016llX RSP=%016llX)\n",
                      frame, nm ? nm + 1 : path, (unsigned long long)off,
                      (unsigned long long)ctx.Rip, (unsigned long long)ctx.Rsp);
        } else {
            CrashLogf("  #%-2d ???                    @ %p          (RSP=%016llX)\n",
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
}

// Dedicated crash log handle, opened with FILE_FLAG_WRITE_THROUGH so every
// WriteFile call commits synchronously to disk. We bypass the CRT entirely
// because fprintf-to-stderr proved racy under crashing-thread teardown:
// the kernel write was buffered and got cut mid-line by TerminateProcess.
static HANDLE g_crashLog = INVALID_HANDLE_VALUE;

static void OpenCrashLog() {
    if (g_crashLog != INVALID_HANDLE_VALUE) return;  // idempotent — early init may have opened already
    g_crashLog = CreateFileA(
        "standalone_crash.log",
        GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        nullptr);
}

// Raw WriteFile of a literal string — CRT-free, used as a progress marker so
// that even if formatted CrashLogf calls die later, we know how far we got.
static void CrashLogRaw(const char* msg) {
    if (g_crashLog == INVALID_HANDLE_VALUE) return;
    DWORD len = 0;
    while (msg[len]) ++len;
    DWORD wrote = 0;
    WriteFile(g_crashLog, msg, len, &wrote, nullptr);
}

// Synchronous write to the dedicated crash log. Each call commits to disk
// before returning thanks to FILE_FLAG_WRITE_THROUGH. No stderr mirroring —
// stderr writes go through the CRT's per-FILE lock which the main thread
// may be holding, deadlocking us inside the kernel callback.
static void CrashLogf(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    if (n > (int)sizeof(buf)) n = sizeof(buf);
    if (g_crashLog != INVALID_HANDLE_VALUE) {
        DWORD wrote = 0;
        WriteFile(g_crashLog, buf, (DWORD)n, &wrote, nullptr);
    }
}

// Sink for RedzoneAlloc_HeapDump — writes each already-formatted line straight
// to the write-through crash log so the dump survives a re-entrant fault.
static void CrashLogSink(const char* s) {
    if (g_crashLog == INVALID_HANDLE_VALUE || !s) return;
    DWORD len = 0; while (s[len]) ++len;
    DWORD wrote = 0;
    WriteFile(g_crashLog, s, len, &wrote, nullptr);
}

// Force the OS-level file buffers behind stderr/stdout to disk. Used at
// the END of the crash handler to flush the convenience-mirror to stderr;
// the crash log itself is FILE_FLAG_WRITE_THROUGH so each CrashLogf is
// already on disk.
static void FlushLogsToDisk() {
    fflush(stderr);
    fflush(stdout);
    HANDLE he = (HANDLE)_get_osfhandle(_fileno(stderr));
    if (he != INVALID_HANDLE_VALUE) FlushFileBuffers(he);
    HANDLE ho = (HANDLE)_get_osfhandle(_fileno(stdout));
    if (ho != INVALID_HANDLE_VALUE) FlushFileBuffers(ho);
}

// Installed via SetUnhandledExceptionFilter — runs ONLY for second-chance
// exceptions that no SEH __try/__except in the call chain caught. This
// avoids logging benign first-chance exceptions raised and absorbed by
// Banished's normal control flow (lazy-commit reads, internal SEH probes).
static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    // Filter out benign codes raised + absorbed by Banished's SEH chains
    // (e.g., FP-control-word probes). Everything else gets logged so we
    // catch C++ exceptions, breakpoints, and unusual fault codes during
    // teardown, not just classic AV/IllegalInst/StackOverflow.
    if (code == 0x40010005 ||           // DBG_CONTROL_C
        code == DBG_CONTROL_C ||
        code == DBG_PRINTEXCEPTION_C ||
        code == 0x4001000A ||           // DBG_PRINTEXCEPTION_WIDE_C
        code == 0x406D1388) {           // MS_VC_THREADNAME (debugger thread name)
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // No CS — during late teardown the CRT may have torn down thread-safe
    // statics already, and we'd rather risk interleaved output than hang here.
    CrashLogRaw("\n=== STANDALONE CRASH ===\n");
    CrashLogf("Exception: 0x%08lX (%s)\n", code, ExceptionCodeToString(code));
    CrashLogf("Address:   %p\n", ep->ExceptionRecord->ExceptionAddress);

    CONTEXT* ctx = ep->ContextRecord;
    CrashLogf("RAX=%016llX RCX=%016llX RDX=%016llX RBX=%016llX\n",
              (unsigned long long)ctx->Rax, (unsigned long long)ctx->Rcx,
              (unsigned long long)ctx->Rdx, (unsigned long long)ctx->Rbx);
    CrashLogf("RBP=%016llX RSI=%016llX RDI=%016llX R8 =%016llX\n",
              (unsigned long long)ctx->Rbp, (unsigned long long)ctx->Rsi,
              (unsigned long long)ctx->Rdi, (unsigned long long)ctx->R8);
    CrashLogf("R9 =%016llX R10=%016llX R11=%016llX R12=%016llX\n",
              (unsigned long long)ctx->R9,  (unsigned long long)ctx->R10,
              (unsigned long long)ctx->R11, (unsigned long long)ctx->R12);
    CrashLogf("RIP=%016llX RSP=%016llX\n",
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
            CrashLogf("Module:    %s + 0x%llX  (base=%p)\n",
                      nm ? nm + 1 : path, (unsigned long long)off, (void*)mod);
        } else {
            CrashLogf("Module:    UNKNOWN (no module covers RIP)\n");
        }
    }

    if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
        const char* op = ep->ExceptionRecord->ExceptionInformation[0] == 0 ? "READ" :
                         ep->ExceptionRecord->ExceptionInformation[0] == 1 ? "WRITE" : "EXEC";
        CrashLogf("Fault: %s of %p\n", op,
                  (void*)ep->ExceptionRecord->ExceptionInformation[1]);
    }

    // Specialized handler: EXEC-of-0 through a virtual call (RIP==0). The faulting
    // instruction was `CALL [vtable + N]` where that slot is NULL. RCX = the object
    // (this). Dump its vtable (module + offset identifies the type) + the first 13
    // slots so we can see WHICH slot is null. Used to diagnose the minimap
    // UpdateEvents crash (target->vtable[+0x40] == 0).
    if (code == EXCEPTION_ACCESS_VIOLATION &&
        ep->ExceptionRecord->ExceptionInformation[0] == 8 /*EXEC*/ &&
        ep->ExceptionRecord->ExceptionInformation[1] == 0 &&
        ctx->Rcx && !IsBadReadPtr((void*)ctx->Rcx, 8)) {
        void* target = (void*)ctx->Rcx;
        uintptr_t vt = *(uintptr_t*)target;
        CrashLogf("VCALL-of-0: this=%p vtable=%p\n", target, (void*)vt);
        HMODULE vmod = nullptr;
        if (vt && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                     GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                     (LPCSTR)vt, &vmod) && vmod) {
            char path[MAX_PATH] = {0};
            GetModuleFileNameA(vmod, path, MAX_PATH);
            const char* nm = strrchr(path, '\\');
            CrashLogf("  vtable in %s + 0x%llX  (base=%p)\n",
                      nm ? nm + 1 : path, (unsigned long long)(vt - (uintptr_t)vmod),
                      (void*)vmod);
        } else {
            CrashLogf("  vtable in UNKNOWN module (heap? bad ptr?)\n");
        }
        if (vt && !IsBadReadPtr((void*)vt, 13 * 8)) {
            for (int i = 0; i < 13; i++)
                CrashLogf("  vtable[+0x%02X] = %p\n", i * 8, *((void**)vt + i));
        }
        if (!IsBadReadPtr(target, 0x40)) {
            for (int i = 0; i < 8; i++)
                CrashLogf("  this[+0x%02X] = 0x%016llX\n", i * 8,
                          (unsigned long long)*((uint64_t*)target + i));
        }
    }

    // Specialized handler: IsDerivedFrom-NULL-this crash. RIP at Runtime+0xC039A
    // is the very first vtable read with RCX=NULL (factory returned NULL).
    // RDX at that point holds the EXPECTED typeId (the External base check).
    uintptr_t runtimeBase = reinterpret_cast<uintptr_t>(GetModuleHandleA("Runtime-x64.dll"));
    if (runtimeBase && ctx->Rcx == 0 && ctx->Rip >= runtimeBase + 0xC0390 && ctx->Rip <= runtimeBase + 0xC03A0) {
        uint32_t expectedTypeId = (uint32_t)ctx->Rdx;
        CrashLogf("IsDerivedFrom NULL-this: factory returned NULL.\n");
        CrashLogf("  Expected base typeId in RDX = 0x%08X\n", expectedTypeId);
        uintptr_t headAddr = ResolveAddr(Module::Runtime, 0x1801fc6e0);
        ProbeBaseTypeInfo* head = headAddr ? *reinterpret_cast<ProbeBaseTypeInfo**>(headAddr) : nullptr;
        for (ProbeBaseTypeInfo* p = head; p; p = p->next) {
            if (p->typeId == expectedTypeId) {
                CrashLogf("  Expected base = %s (parent=%s, factory=%p)\n",
                          p->name ? p->name : "?", p->parent ? p->parent : "?", p->factoryFn);
                break;
            }
        }
    }

    // Specialized handler: External::Load tree-walk crash. The instruction at
    // Runtime+0xBC73E is `MOV RAX, [RCX+0x10]` with RCX=NULL because the
    // typeId in [RBP+0x48] wasn't found in Factory::_singleton's tree.
    if (runtimeBase && ctx->Rip == runtimeBase + 0xBC73E) {
        uint32_t* typeIdSlot = reinterpret_cast<uint32_t*>(ctx->Rbp + 0x48);
        if (!IsBadReadPtr(typeIdSlot, sizeof(uint32_t))) {
            uint32_t typeId = *typeIdSlot;
            CrashLogf("External::Load: missed typeId=0x%08X at [RBP+0x48]=%p\n",
                      typeId, (void*)typeIdSlot);

            uintptr_t headAddr = ResolveAddr(Module::Runtime, 0x1801fc6e0);
            ProbeBaseTypeInfo* head = headAddr ? *reinterpret_cast<ProbeBaseTypeInfo**>(headAddr) : nullptr;
            bool matched = false;
            for (ProbeBaseTypeInfo* p = head; p; p = p->next) {
                if (p->typeId == typeId) {
                    CrashLogf("  -> matches REGISTERED type: %s (parent=%s)\n",
                              p->name ? p->name : "?", p->parent ? p->parent : "?");
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                CrashLogf("  -> typeId is NOT in _firstTypeInfo (missing exe-side registration)\n");
            }
        } else {
            CrashLogf("External::Load: [RBP+0x48]=%p is unreadable\n", (void*)typeIdSlot);
        }
    }

    // Check registry stats
    RegistryStats stats = GetRegistryStats();
    CrashLogf("Registry: %zu registered, %zu stubs hit\n",
              stats.registered, stats.stubs);

    // Free-list poisoning diagnosis: walk the engine allocator's boundary tags
    // and dump the corrupted block + its physical predecessor's vtable (= the
    // class whose buffer overflowed). Output is mirrored to the WRITE-THROUGH
    // crash log via the sink (stdout/debug.log output was lost — the dump faults
    // mid-walk in crash context before its unbuffered lines commit).
    // Works on a fully-native run (no redzone shim needed).
    CrashLogf("(heap-dump follows; also mirrored to standalone_debug.log)\n");
    RedzoneAlloc_SetDumpSink(&CrashLogSink);
    RedzoneAlloc_HeapDump("CrashHandler");
    RedzoneAlloc_SetDumpSink(nullptr);

    PrintStackTrace(ctx);

    CrashLogf("=== END CRASH ===\n");
    FlushLogsToDisk();

    // Avoid the OS unhandled-exception-filter race: terminate the process
    // ourselves on our own terms. ExitProcess skips CRT shutdown but still
    // runs DLL_PROCESS_DETACH; TerminateProcess is more aggressive but
    // can leave kernel state cluttered. _exit was problematic during late
    // teardown — go with ExitProcess.
    ExitProcess(3);
    return EXCEPTION_EXECUTE_HANDLER;
}

// ============================================================
// Pure-virtual call handler. libstdc++ exports __cxa_pure_virtual as a
// weak symbol that aborts via std::terminate; MSVC's CRT exports _purecall
// with similar behavior. Both bypass our SetUnhandledExceptionFilter so a
// pure-virtual call shows nothing in standalone_crash.log unless we
// override them here. Both variants converge on the same body.
// ============================================================
static void LogPureVirtualCall(const char* tag, void* retAddr) {
    if (g_crashLog == INVALID_HANDLE_VALUE) OpenCrashLog();
    CrashLogRaw("\n=== STANDALONE PURE-VIRTUAL CALL ===\n");
    CrashLogf("Tag:            %s\n", tag);
    CrashLogf("Return address: %p\n", retAddr);

    HMODULE mod = nullptr;
    if (retAddr && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                       (LPCSTR)retAddr, &mod) && mod) {
        char path[MAX_PATH] = {0};
        GetModuleFileNameA(mod, path, MAX_PATH);
        const char* nm = strrchr(path, '\\');
        uintptr_t off = (uintptr_t)retAddr - (uintptr_t)mod;
        CrashLogf("Module:         %s + 0x%llX  (base=%p)\n",
                  nm ? nm + 1 : path, (unsigned long long)off, (void*)mod);
    }

    CONTEXT ctx;
    RtlCaptureContext(&ctx);
    PrintStackTrace(&ctx);

    CrashLogf("=== END PURE-VIRTUAL ===\n");
    FlushLogsToDisk();
    ExitProcess(4);
}

extern "C" void __cxa_pure_virtual() {
    LogPureVirtualCall("__cxa_pure_virtual", __builtin_return_address(0));
}

extern "C" void _purecall() {
    LogPureVirtualCall("_purecall", __builtin_return_address(0));
}

static void TerminateHandler() {
    if (g_crashLog == INVALID_HANDLE_VALUE) OpenCrashLog();
    CrashLogRaw("\n=== STANDALONE std::terminate() ===\n");
    CONTEXT ctx;
    RtlCaptureContext(&ctx);
    PrintStackTrace(&ctx);
    CrashLogf("=== END terminate ===\n");
    FlushLogsToDisk();
    ExitProcess(5);
}

// ============================================================
// MessageBoxW IAT hook for Runtime-x64.dll. MSVC's _purecall handler in
// Runtime DLL's statically-linked CRT pops MessageBoxW("R6025: pure virtual
// function call") and BLOCKS until the user clicks OK before abort() runs.
// Looks like a freeze: log activity stops, no crash log written, and our
// own __cxa_pure_virtual / std::set_terminate handlers don't fire because
// the pure-virtual call lives inside the Runtime CRT, not ours.
//
// _set_purecall_handler can't help because it only affects the calling
// CRT. We patch Runtime's IAT entry for user32!MessageBoxW so every call
// from Runtime DLL routes through our wrapper. If the popup looks like
// a pure-virtual notice, log + ExitProcess. Otherwise pass through.
// ============================================================
typedef int (WINAPI *MessageBoxW_t)(HWND, LPCWSTR, LPCWSTR, UINT);
static MessageBoxW_t g_realMessageBoxW = nullptr;

static int WINAPI HookedMessageBoxW(HWND hWnd, LPCWSTR text, LPCWSTR caption, UINT type) {
    char textBuf[1024]    = {0};
    char captionBuf[256]  = {0};
    if (text)    WideCharToMultiByte(CP_UTF8, 0, text,    -1, textBuf,    sizeof(textBuf),    nullptr, nullptr);
    if (caption) WideCharToMultiByte(CP_UTF8, 0, caption, -1, captionBuf, sizeof(captionBuf), nullptr, nullptr);

    // R6025 = MSVC pure-virtual; "pure virtual" covers other CRT variants.
    bool isPureCall =
        strstr(textBuf,    "pure virtual") != nullptr ||
        strstr(textBuf,    "R6025")        != nullptr ||
        strstr(captionBuf, "R6025")        != nullptr;

    if (isPureCall) {
        if (g_crashLog == INVALID_HANDLE_VALUE) OpenCrashLog();
        CrashLogRaw("\n=== STANDALONE PURE-VIRTUAL (MessageBoxW intercepted) ===\n");
        CrashLogf("Caption: %s\n", captionBuf[0] ? captionBuf : "(none)");
        CrashLogf("Text:    %s\n", textBuf[0]    ? textBuf    : "(none)");

        // Stack trace from inside the hook — caller chain leads back to
        // Runtime's _purecall stub and from there to the bad vtable call.
        CONTEXT ctx;
        RtlCaptureContext(&ctx);
        PrintStackTrace(&ctx);

        CrashLogf("=== END (intercepted) ===\n");
        FlushLogsToDisk();
        ExitProcess(6);
    }

    // Legit popup (launcher dialogs, etc.) — pass through.
    return g_realMessageBoxW
        ? g_realMessageBoxW(hWnd, text, caption, type)
        : MessageBoxW(hWnd, text, caption, type);
}

// Walk Runtime-x64.dll's import descriptors, find user32!MessageBoxW, and
// swap the IAT slot to point at HookedMessageBoxW. Returns true on success.
// The hook stays installed for the process lifetime — no restore needed.
static bool HookRuntimeMessageBoxW() {
    HMODULE runtime = GetModuleHandleA("Runtime-x64.dll");
    if (!runtime) return false;

    auto base = reinterpret_cast<uint8_t*>(runtime);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

    auto& importDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!importDir.VirtualAddress || !importDir.Size) return false;

    auto* desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + importDir.VirtualAddress);
    for (; desc->Name; ++desc) {
        const char* dllName = reinterpret_cast<const char*>(base + desc->Name);
        if (_stricmp(dllName, "user32.dll") != 0) continue;

        // OriginalFirstThunk holds the name table; FirstThunk is the IAT
        // (loader fills it with the actual function addresses). If the DLL
        // was bound, OriginalFirstThunk may still be valid for name lookup.
        DWORD nameTableRVA = desc->OriginalFirstThunk
                           ? desc->OriginalFirstThunk
                           : desc->FirstThunk;
        auto* nameThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + nameTableRVA);
        auto* iatThunk  = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->FirstThunk);

        for (; nameThunk->u1.AddressOfData; ++nameThunk, ++iatThunk) {
            if (nameThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64) continue;
            auto* imp = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                base + nameThunk->u1.AddressOfData);
            if (strcmp(imp->Name, "MessageBoxW") != 0) continue;

            DWORD oldProtect = 0;
            if (!VirtualProtect(&iatThunk->u1.Function, sizeof(uintptr_t),
                                PAGE_READWRITE, &oldProtect)) {
                return false;
            }
            g_realMessageBoxW = reinterpret_cast<MessageBoxW_t>(iatThunk->u1.Function);
            iatThunk->u1.Function = reinterpret_cast<uintptr_t>(&HookedMessageBoxW);
            VirtualProtect(&iatThunk->u1.Function, sizeof(uintptr_t),
                           oldProtect, &oldProtect);
            return true;
        }
    }
    return false;
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
    int descMatches = 0;
    for (ProbeBaseTypeInfo* p = head; p != nullptr && n < 1000; p = p->next, n++) {
        if (n < 5) {
            printf("[Probe %s]   #%-3d name=%-32s parent=%s\n",
                   tag, n,
                   p->name ? p->name : "(null)",
                   p->parent ? p->parent : "(null)");
        }
        // Spot-check: do the description types referenced in Loading.rsc exist?
        if (p->name) {
            const char* targets[] = {
                "GroupDescription", "RibbonDescription", "ImageDescription",
                "TextDescription", "AnimatedImageDescription", "ElementDescription",
                "Dialog", nullptr
            };
            for (int i = 0; targets[i]; ++i) {
                if (strcmp(p->name, targets[i]) == 0) {
                    printf("[Probe %s] FOUND #%d %s parent=%s\n",
                           tag, n, p->name, p->parent ? p->parent : "(null)");
                    ++descMatches;
                    break;
                }
            }
        }
    }
    printf("[Probe %s] Total: %d entries, %d Loading.rsc target descs found\n",
           tag, n, descMatches);
}

// ============================================================
// Skip the launcher's DialogBoxParamW call inside Engine::PlatformConfigure
// (Runtime 0x1800a17d4) without bypassing the rest of the function.
//
// Original instruction:
//     1800a17d4: FF 15 F6 9C 0A 00      CALL [DialogBoxParamW IAT slot]
// We overwrite with:
//     1800a17d4: B8 01 00 00 00 90      MOV EAX,1 ; NOP
// so DialogBoxParamW returns 1 (i.e. user pressed "Play"), but everything
// else in PlatformConfigure runs normally — most importantly:
//   - Video::Device::ValidateConfiguration (0x1800a17b5, before the call)
//   - Video::Configuration::Save / IO::Persistence::WriteSettings (after)
//
// PlatformConfigure's tail returns `IVar6 == 1`, so after the patch the
// function returns true. The big "reset" branches at the end are gated on
// global flags (DAT_1801fc698/_x699) that the dialog handler would have
// flipped — with the dialog skipped they stay 0 and the resets don't run.
//
// This is the M1-friendly replacement for the previous 6-byte entry patch
// (MOV EAX,1; RET) which short-circuited ValidateConfiguration and may have
// left Device::_singleton+0x100 pointing at the wrong renderer.
// ============================================================
static void SkipPlatformConfigure() {
    uintptr_t patchSite = ResolveAddr(Module::Runtime, 0x1800a17d4);
    if (!patchSite) {
        printf("[Standalone] WARN: could not resolve PlatformConfigure dialog call site — popup may appear\n");
        return;
    }
    // Sanity check: refuse to patch if the bytes don't match the indirect CALL we expect.
    static const uint8_t expected[] = { 0xFF, 0x15, 0xF6, 0x9C, 0x0A, 0x00 };
    if (memcmp((void*)patchSite, expected, sizeof(expected)) != 0) {
        printf("[Standalone] WARN: PlatformConfigure dialog call site (%p) mismatched — refusing to patch\n",
               (void*)patchSite);
        return;
    }
    static const uint8_t patch[] = { 0xB8, 0x01, 0x00, 0x00, 0x00, 0x90 };
    DWORD oldProtect = 0;
    if (!VirtualProtect((LPVOID)patchSite, sizeof(patch), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        printf("[Standalone] WARN: VirtualProtect failed for PlatformConfigure (err=%lu)\n",
               GetLastError());
        return;
    }
    memcpy((void*)patchSite, patch, sizeof(patch));
    DWORD dummy = 0;
    VirtualProtect((LPVOID)patchSite, sizeof(patch), oldProtect, &dummy);
    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)patchSite, sizeof(patch));
    printf("[Standalone] PlatformConfigure dialog call (%p) patched to MOV EAX,1; NOP — popup skipped, ValidateConfiguration runs\n",
           (void*)patchSite);
}

// ============================================================
// Factory self-test: walk _firstTypeInfo to find a registered type by name,
// invoke its factoryFn (the same pointer External::Load would dispatch to),
// and validate the resulting object's vtable + ctor-set field defaults.
//
// This proves the per-class factory path end-to-end without requiring the
// loader thread to reach the type. As more types get real factories, add
// one of these per type so we can validate locally.
// ============================================================
struct FieldCheck {
    uintptr_t offset;
    uint32_t  expected;
    int       width;          // 1, 2, or 4 bytes
    const char* name;
};

static bool RunFactorySelfTest(const char* typeName,
                               uintptr_t expectedVtableExeAddr,
                               size_t expectedSize,
                               const FieldCheck* fields, size_t fieldCount) {
    printf("\n[SelfTest %s] start\n", typeName);

    // Refresh g_pMemAllocSingleton from its Runtime DLL slot. The slot is
    // populated by Engine::Engine::Engine; EngineFuncsInit ran before that
    // and cached NULL. Without this refresh, factory MemAlloc returns NULL.
    {
        void** slot = reinterpret_cast<void**>(
            ResolveAddr(Module::Runtime, 0x1801fc718));
        if (slot && *slot && !g_pMemAllocSingleton) {
            g_pMemAllocSingleton = *slot;
            printf("[SelfTest %s]   refreshed g_pMemAllocSingleton = %p\n",
                   typeName, g_pMemAllocSingleton);
        }
    }

    uintptr_t headAddr = ResolveAddr(Module::Runtime, 0x1801fc6e0);
    if (!headAddr) {
        printf("[SelfTest %s] FAIL: cannot resolve _firstTypeInfo head\n", typeName);
        return false;
    }
    ProbeBaseTypeInfo* head = *reinterpret_cast<ProbeBaseTypeInfo**>(headAddr);

    ProbeBaseTypeInfo* match = nullptr;
    for (ProbeBaseTypeInfo* p = head; p; p = p->next) {
        if (p->name && std::strcmp(p->name, typeName) == 0) { match = p; break; }
    }
    if (!match) {
        printf("[SelfTest %s] FAIL: not found in _firstTypeInfo (registration didn't run)\n",
               typeName);
        return false;
    }
    printf("[SelfTest %s]   found entry @ %p (factoryFn=%p, parent=%s)\n",
           typeName, (void*)match,
           match->factoryFn,
           match->parent ? match->parent : "(null)");

    if (!match->factoryFn) {
        printf("[SelfTest %s] FAIL: factoryFn is NULL\n", typeName);
        return false;
    }

    typedef void* (*Factory_t)(void);
    void* obj = ((Factory_t)match->factoryFn)();
    if (!obj) {
        printf("[SelfTest %s] FAIL: factory returned NULL\n", typeName);
        return false;
    }
    printf("[SelfTest %s]   factory returned %p\n", typeName, obj);

    // Validate vtable
    uintptr_t expectedVt = ResolveAddr(Module::Exe, expectedVtableExeAddr);
    void* actualVt = *(void**)obj;
    if ((uintptr_t)actualVt != expectedVt) {
        printf("[SelfTest %s] FAIL: vtable %p != expected %p (exe 0x%llx)\n",
               typeName, actualVt, (void*)expectedVt,
               (unsigned long long)expectedVtableExeAddr);
        return false;
    }
    printf("[SelfTest %s]   vtable OK (%p)\n", typeName, actualVt);

    // Validate field defaults
    bool allOk = true;
    for (size_t i = 0; i < fieldCount; i++) {
        const FieldCheck& f = fields[i];
        uint32_t actual = 0;
        switch (f.width) {
        case 1: actual = *(uint8_t*) ((uintptr_t)obj + f.offset); break;
        case 2: actual = *(uint16_t*)((uintptr_t)obj + f.offset); break;
        case 4: actual = *(uint32_t*)((uintptr_t)obj + f.offset); break;
        default:
            printf("[SelfTest %s]   FAIL: bad field width %d for %s\n",
                   typeName, f.width, f.name);
            allOk = false; continue;
        }
        if (actual != f.expected) {
            printf("[SelfTest %s]   FAIL: %s @+0x%llx = 0x%X, expected 0x%X\n",
                   typeName, f.name, (unsigned long long)f.offset, actual, f.expected);
            allOk = false;
        }
    }
    if (!allOk) return false;

    printf("[SelfTest %s] PASS (sizeof=%zu, %zu fields verified)\n",
           typeName, expectedSize, fieldCount);
    // Object intentionally leaked — one-shot startup test, OS reclaims at exit.
    // Calling the dtor would require the full Goal::~Goal/String chain to be
    // valid, which is out of scope for the factory test.
    return true;
}

// ============================================================
// Probe the Video subsystem state after Engine::Startup. Tells us:
//   - Configuration::_singleton[+4]: requested interface index (0=DX9, 1=DX11, 2=GL)
//   - Device::_singleton[+0xf8]:     active interface index (CreateVideoInterface picked this)
//   - Device::_singleton[+0x100]:    active DeviceData pointer (read by ImageBuffer::PlatformConstruct)
//   - Device plugin slots (HMODULEs) at +0xb0, +0xc8, +0xe0
//
// The active DeviceData's vtable will reveal the actual renderer in use:
// pointers in VideoDX11-x64.dll vs. VideoDX9-x64.dll. This pins down the
// "DX9 or DX11" question that drives the +0x267E crash.
// ============================================================
static void DumpVideoState() {
    void** devSlot = reinterpret_cast<void**>(
        ResolveAddr(Module::Runtime, 0x1801fe7a8));
    void** cfgSlot = reinterpret_cast<void**>(
        ResolveAddr(Module::Runtime, 0x1801fe7a0));

    void* device = devSlot ? *devSlot : nullptr;
    void* config = cfgSlot ? *cfgSlot : nullptr;
    printf("\n[VideoProbe] Configuration::_singleton = %p\n", config);
    if (config) {
        int interfaceIdx = *reinterpret_cast<int*>((uintptr_t)config + 4);
        int adapterIdx   = *reinterpret_cast<int*>((uintptr_t)config + 8);
        int width        = *reinterpret_cast<int*>((uintptr_t)config + 0x0c);
        int height       = *reinterpret_cast<int*>((uintptr_t)config + 0x10);
        const char* iname =
            interfaceIdx == 0 ? "VideoDX9" :
            interfaceIdx == 1 ? "VideoDX11" :
            interfaceIdx == 2 ? "VideoGL" : "?";
        printf("[VideoProbe]   +0x04 interface = %d (%s)\n", interfaceIdx, iname);
        printf("[VideoProbe]   +0x08 adapter   = %d\n", adapterIdx);
        printf("[VideoProbe]   +0x0c width     = %d\n", width);
        printf("[VideoProbe]   +0x10 height    = %d\n", height);
    }

    printf("[VideoProbe] Device::_singleton        = %p\n", device);
    if (device) {
        int activeIdx = *reinterpret_cast<int*>((uintptr_t)device + 0xf8);
        void* activeDD = *reinterpret_cast<void**>((uintptr_t)device + 0x100);
        int interfaceCount = *reinterpret_cast<int*>((uintptr_t)device + 0x48);
        HMODULE dx9   = *reinterpret_cast<HMODULE*>((uintptr_t)device + 0xb0);
        void* dx9_GetName  = *reinterpret_cast<void**>((uintptr_t)device + 0xb8);
        void* dx9_Create   = *reinterpret_cast<void**>((uintptr_t)device + 0xc0);
        HMODULE dx11  = *reinterpret_cast<HMODULE*>((uintptr_t)device + 0xc8);
        void* dx11_GetName = *reinterpret_cast<void**>((uintptr_t)device + 0xd0);
        void* dx11_Create  = *reinterpret_cast<void**>((uintptr_t)device + 0xd8);
        HMODULE gl    = *reinterpret_cast<HMODULE*>((uintptr_t)device + 0xe0);
        printf("[VideoProbe]   +0x48  interface count = %d\n", interfaceCount);
        printf("[VideoProbe]   +0xf8  active idx  = %d\n", activeIdx);
        printf("[VideoProbe]   +0x100 active DeviceData = %p\n", activeDD);
        printf("[VideoProbe]   slot0 DX9:  HMOD=%p GetName=%p Create=%p\n", dx9, dx9_GetName, dx9_Create);
        printf("[VideoProbe]   slot1 DX11: HMOD=%p GetName=%p Create=%p\n", dx11, dx11_GetName, dx11_Create);
        printf("[VideoProbe]   slot2 GL:   HMOD=%p\n", gl);
        if (activeDD) {
            void* vtbl = *reinterpret_cast<void**>(activeDD);
            printf("[VideoProbe]   active DeviceData vtable = %p\n", vtbl);
            HMODULE owner = nullptr;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCSTR)vtbl, &owner) && owner) {
                char path[MAX_PATH] = {0};
                GetModuleFileNameA(owner, path, MAX_PATH);
                const char* nm = strrchr(path, '\\');
                printf("[VideoProbe]   active vtable lives in: %s\n", nm ? nm + 1 : path);
            } else {
                printf("[VideoProbe]   active vtable in unknown module\n");
            }
        }
    }
    fflush(stdout);
}

// Called from game_main.cpp after Engine::Startup completes (so that the
// MemoryAllocator singleton is populated). Linked via the extern decl below.
extern "C" void RunStartupSelfTests() {
    printf("\n[Standalone] === Running factory self-tests ===\n");

    static const FieldCheck statGoalFields[] = {
        { 0x30, 5,    4, "statTypeA" },
        { 0x34, 5,    4, "statTypeB" },
        { 0x38, 10,   4, "comparisonType" },
        { 0x3c, 1,    4, "fixedValue" },
        { 0x40, 0,    1, "requireOtherGoalsMet" },
        { 0x50, 0xFFFFFFFFu, 4, "statAGeneration" },
        { 0x68, 0xFFFFFFFFu, 4, "statBGeneration" },
        { 0x78, 0x20, 4, "statFlags" },
        { 0x7c, 1,    4, "statSubType" },
    };
    RunFactorySelfTest("StatGoal", 0x1401604c0, 0x80,
                       statGoalFields, sizeof(statGoalFields)/sizeof(*statGoalFields));

    DumpVideoState();
    DumpSwapChainState("AFTER_ENGINE_STARTUP");
    CaptureBootFullscreenMode();
}

// ============================================================
// Swap-chain state probe — reads IDXGISwapChain at
// Device::_singleton[+0x100][+0x18] (the active DeviceData's swap chain
// pointer, populated by VideoDX11!FUN_180004e10 during Engine::Startup),
// then calls GetFullscreenState (vtable+0x58) and GetDesc (vtable+0x60).
//
// The fullscreen-on-focus-loss bug shows up here: standalone enters DXGI
// exclusive fullscreen, focus-loss makes DXGI silently drop FSE, the
// window snaps to VideoWindow{Width,Height} (1280x720) — about 2/3 of
// a 1080p desktop — and Present succeeds but renders to a stale surface.
// Logging the swap chain's Windowed flag + DXGI_MODE_DESC across focus
// events tells us whether the engine's recovery (FUN_180004340) is
// firing AND whether SetFullscreenState(TRUE) succeeds on refocus.
// ============================================================

// IDXGISwapChain vtable offsets (from dxgi.h; standard COM layout):
//   +0x00 QueryInterface  +0x08 AddRef          +0x10 Release
//   +0x18 SetPrivateData  +0x20 SetPDInterface  +0x28 GetPrivateData
//   +0x30 GetParent       +0x38 GetDevice
//   +0x40 Present         +0x48 GetBuffer
//   +0x50 SetFullscreenState  +0x58 GetFullscreenState  +0x60 GetDesc
//   +0x68 ResizeBuffers   +0x70 ResizeTarget   +0x78 GetContainingOutput
//   +0x80 GetFrameStatistics  +0x88 GetLastPresentCount

// Natural alignment (no #pragma pack) — matches dxgi.h. HWND is 8-byte
// aligned on x64, so the compiler inserts 4 bytes of padding after
// BufferCount, putting OutputWindow at offset 48 and total size at 72.
struct ProbeDxgiRational { uint32_t Numerator; uint32_t Denominator; };
struct ProbeDxgiModeDesc {
    uint32_t Width;
    uint32_t Height;
    ProbeDxgiRational RefreshRate;
    uint32_t Format;
    uint32_t ScanlineOrdering;
    uint32_t Scaling;
};
struct ProbeDxgiSampleDesc { uint32_t Count; uint32_t Quality; };
struct ProbeDxgiSwapChainDesc {
    ProbeDxgiModeDesc   BufferDesc;       // 28 B (offset 0..27)
    ProbeDxgiSampleDesc SampleDesc;       // 8 B  (offset 28..35)
    uint32_t            BufferUsage;      // offset 36
    uint32_t            BufferCount;      // offset 40
    // 4 bytes implicit padding here
    uint64_t            OutputWindow;     // offset 48 (HWND)
    int32_t             Windowed;         // offset 56
    uint32_t            SwapEffect;       // offset 60
    uint32_t            Flags;            // offset 64
    // 4 bytes implicit trailing padding (alignment to 8)
};
static_assert(sizeof(ProbeDxgiSwapChainDesc) == 72,
              "DXGI_SWAP_CHAIN_DESC layout mismatch (expected 72 bytes)");
static_assert(offsetof(ProbeDxgiSwapChainDesc, OutputWindow) == 48,
              "OutputWindow offset mismatch");
static_assert(offsetof(ProbeDxgiSwapChainDesc, Windowed) == 56,
              "Windowed offset mismatch");

static void DumpSwapChainState(const char* tag) {
    if (!g_pVideoDeviceSingleton || !*g_pVideoDeviceSingleton) {
        printf("[SwapProbe %s] device singleton not populated\n", tag);
        return;
    }
    void* device = *g_pVideoDeviceSingleton;
    void* activeDD = *reinterpret_cast<void**>((uintptr_t)device + 0x100);
    if (!activeDD) {
        printf("[SwapProbe %s] activeDeviceData NULL (Device+0x100)\n", tag);
        return;
    }
    void* swapChain = *reinterpret_cast<void**>((uintptr_t)activeDD + 0x18);
    if (!swapChain) {
        printf("[SwapProbe %s] swapChain NULL (activeDD+0x18)\n", tag);
        return;
    }
    void** vt = *reinterpret_cast<void***>(swapChain);

    typedef long (__stdcall *GetFullscreenState_t)(void*, int*, void**);
    typedef long (__stdcall *GetDesc_t)(void*, ProbeDxgiSwapChainDesc*);

    int fullscreen = -1;
    void* output = nullptr;
    long hr1 = ((GetFullscreenState_t)vt[11])(swapChain, &fullscreen, &output);

    ProbeDxgiSwapChainDesc desc{};
    long hr2 = ((GetDesc_t)vt[12])(swapChain, &desc);

    HWND hwnd = reinterpret_cast<HWND>(desc.OutputWindow);
    RECT cr{}, wr{};
    if (hwnd) { GetClientRect(hwnd, &cr); GetWindowRect(hwnd, &wr); }
    BOOL visible = hwnd ? IsWindowVisible(hwnd) : FALSE;
    BOOL minimized = hwnd ? IsIconic(hwnd) : FALSE;

    printf("[SwapProbe %s] sc=%p vt=%p\n", tag, swapChain, vt);
    printf("[SwapProbe %s]   GetFullscreenState hr=0x%08lX fs=%d output=%p\n",
           tag, hr1, fullscreen, output);
    printf("[SwapProbe %s]   GetDesc hr=0x%08lX Windowed=%d %ux%u fmt=%u "
           "BufferCount=%u SwapEffect=%u Flags=0x%X\n",
           tag, hr2, desc.Windowed, desc.BufferDesc.Width, desc.BufferDesc.Height,
           desc.BufferDesc.Format, desc.BufferCount, desc.SwapEffect, desc.Flags);
    printf("[SwapProbe %s]   hwnd=%p visible=%d iconic=%d "
           "client=%ldx%ld window=%ld,%ld %ldx%ld\n",
           tag, (void*)hwnd, visible, minimized,
           cr.right - cr.left, cr.bottom - cr.top,
           wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top);
    fflush(stdout);
}

// ============================================================
// Window-message hook — log focus / activation messages so we can see what
// the engine's WindowProc receives when the user clicks outside the window.
// Installed after Engine::Startup creates the window. Returns false from
// the hook callback to let the message dispatch normally.
// ============================================================
static HHOOK g_msgHook = nullptr;

static const char* WMNameForFocus(UINT m) {
    switch (m) {
    case 0x0006: return "WM_ACTIVATE";
    case 0x001C: return "WM_ACTIVATEAPP";
    case 0x0007: return "WM_SETFOCUS";
    case 0x0008: return "WM_KILLFOCUS";
    case 0x0086: return "WM_NCACTIVATE";
    case 0x0024: return "WM_GETMINMAXINFO";
    case 0x0046: return "WM_WINDOWPOSCHANGING";
    case 0x0047: return "WM_WINDOWPOSCHANGED";
    case 0x001A: return "WM_SETTINGCHANGE";
    default:     return nullptr;
    }
}

// Captured at boot: was the swap chain created with Windowed=0 (DXGI
// exclusive fullscreen)? Drives whether ForceVideoContextReset fires on
// refocus — borderless/windowed swap chains never lose their back buffer
// across focus events, so the recovery is unnecessary (and the Reset call
// would visibly flicker the menu for no reason).
static bool g_bootedExclusiveFullscreen = false;

static void CaptureBootFullscreenMode() {
    if (!g_pVideoDeviceSingleton || !*g_pVideoDeviceSingleton) return;
    void* device = *g_pVideoDeviceSingleton;
    void* activeDD = *reinterpret_cast<void**>((uintptr_t)device + 0x100);
    if (!activeDD) return;
    void* swapChain = *reinterpret_cast<void**>((uintptr_t)activeDD + 0x18);
    if (!swapChain) return;
    void** vt = *reinterpret_cast<void***>(swapChain);
    typedef long (__stdcall *GetDesc_t)(void*, ProbeDxgiSwapChainDesc*);
    ProbeDxgiSwapChainDesc desc{};
    if (((GetDesc_t)vt[12])(swapChain, &desc) == 0) {
        g_bootedExclusiveFullscreen = (desc.Windowed == 0);
        printf("[Standalone] Boot mode: %s\n",
               g_bootedExclusiveFullscreen ? "DXGI EXCLUSIVE FULLSCREEN"
                                           : "borderless/windowed");
    }
}

// Recovery for the DXGI-exclusive-fullscreen focus-loss bug: on refocus
// the engine's WM_ACTIVATE handler (VideoDX11!FUN_1800042d0 → FUN_180004340)
// rebuilds the swap chain + DeviceData RTV/DSV, but the per-frame
// Video::Context cache at Context+0x2160 still holds stale references to
// the old back-buffer's RTV/DSV. Calling Video::Context::Reset (the same
// function the engine itself invokes when BeginFrame fails) flushes that
// cache so the next Draw rebinds the rebuilt resources. Without this,
// Present succeeds (returns S_OK) but pixels stay black.
//
// No-op when booted in borderless/windowed since DXGI doesn't drop the
// back buffer across focus events in that mode.
static void ForceVideoContextReset(const char* tag) {
    if (!g_bootedExclusiveFullscreen) return;
    if (!g_pMainLoopSingleton || !*g_pMainLoopSingleton) return;
    uint8_t* ml = reinterpret_cast<uint8_t*>(*g_pMainLoopSingleton);
    void* ctx = *reinterpret_cast<void**>(ml + 0x10);
    if (!ctx || !fn_VideoCtx_Reset) return;
    *(ml + 0x18) = 0;  // engine clears m_videoContextDirty before Reset
    fn_VideoCtx_Reset(ctx);
    printf("[CtxReset %s] Video::Context::Reset(ctx=%p) — FSE refocus recovery\n",
           tag, ctx);
    fflush(stdout);
}

// Swap the engine's window-class background brush from the default WHITE_BRUSH
// to a black brush, so the brief moment between window creation and the first
// DX11 Present doesn't show as a white flash. We do this once on the first
// engine-window message we observe in CallWndProcHook (don't have access to
// the HWND any earlier, since the Runtime DLL creates the window).
static void EnsureBlackWindowBrush(HWND hwnd) {
    static bool done = false;
    if (done || !hwnd) return;
    HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
    if (!black) return;
    HBRUSH old = (HBRUSH)SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)black);
    InvalidateRect(hwnd, nullptr, TRUE);  // force a repaint with the new brush
    UpdateWindow(hwnd);
    done = true;
    printf("[Standalone] Swapped window class background brush from %p to "
           "BLACK_BRUSH (%p) on hwnd=%p\n", (void*)old, (void*)black, (void*)hwnd);
}

static LRESULT CALLBACK CallWndProcHook(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0) {
        auto* msg = reinterpret_cast<CWPSTRUCT*>(lParam);
        // Swap the class brush on the first message we ever see (any window).
        // The engine's main HWND is whichever one gets messages first.
        EnsureBlackWindowBrush(msg->hwnd);
        if (const char* name = WMNameForFocus(msg->message)) {
            printf("[WndMsg] %-20s hwnd=%p w=%llx l=%llx\n",
                   name, (void*)msg->hwnd,
                   (unsigned long long)msg->wParam,
                   (unsigned long long)msg->lParam);
            // Refocus recovery: WM_ACTIVATE w!=0 fires before WM_SETFOCUS but
            // (importantly) after the engine's own WM_ACTIVATE handler. Calling
            // Reset here lands on the next Draw frame rather than waiting until
            // WM_SETFOCUS, eliminating the one-frame flicker we'd otherwise see.
            // Engine-level FUN_1800042d0 also runs on this same message, but
            // since WH_CALLWNDPROC fires *before* WindowProc, the engine's
            // recovery completes between our hook returning and the next frame.
            // Our Reset clears Video::Context's cached state — which the engine
            // doesn't touch — so by next frame both sides are fresh.
            if (msg->message == 0x0006 /*WM_ACTIVATE*/ &&
                (msg->wParam & 0xFFFF) != 0) {
                ForceVideoContextReset("WM_ACTIVATE(refocus)");
            }
            fflush(stdout);
        }
    }
    return CallNextHookEx(g_msgHook, code, wParam, lParam);
}

static void InstallWndMsgHook() {
    g_msgHook = SetWindowsHookExW(WH_CALLWNDPROC, CallWndProcHook,
                                  nullptr, GetCurrentThreadId());
    if (!g_msgHook) {
        printf("[Standalone] WARN: SetWindowsHookExW failed (%lu) — focus diag disabled\n",
               GetLastError());
    } else {
        printf("[Standalone] Window-message hook installed (focus diag)\n");
    }
}

// ============================================================
// Initialization sequence
// ============================================================
// First-chance vectored handler that logs ONLY the fatal fail-fast /stack-cookie
// code (0xC0000409, STATUS_STACK_BUFFER_OVERRUN) — a __security_check_cookie /
// __fastfail failure bypasses SetUnhandledExceptionFilter entirely, so nothing
// reaches standalone_crash.log. Everything else (the game's normal first-chance
// SEH-absorbed AVs) passes straight through untouched.
static LONG WINAPI FirstChanceFailFast(EXCEPTION_POINTERS* ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code == 0xC0000409 /*STATUS_STACK_BUFFER_OVERRUN*/ ||
        code == 0xC0000602 /*STATUS_FAIL_FAST_EXCEPTION*/) {
        if (g_crashLog == INVALID_HANDLE_VALUE) OpenCrashLog();
        CrashLogRaw("\n=== STANDALONE FAIL-FAST (first-chance) ===\n");
        CrashLogf("Code: 0x%08lX  Address: %p\n", code,
                  ep->ExceptionRecord->ExceptionAddress);
        HMODULE mod = nullptr;
        void* addr = ep->ExceptionRecord->ExceptionAddress;
        if (addr && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                       (LPCSTR)addr, &mod) && mod) {
            char path[MAX_PATH] = {0};
            GetModuleFileNameA(mod, path, MAX_PATH);
            const char* nm = strrchr(path, '\\');
            CrashLogf("Module: %s + 0x%llX  (base=%p)\n", nm ? nm + 1 : path,
                      (unsigned long long)((uintptr_t)addr - (uintptr_t)mod), (void*)mod);
        }
        PrintStackTrace(ep->ContextRecord);
        CrashLogf("=== END FAIL-FAST ===\n");
        FlushLogsToDisk();
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static bool Init() {
    // 1. Install crash handler — only catches second-chance exceptions
    //    so first-chance AVs absorbed by Banished's SEH chains stay quiet.
    SetUnhandledExceptionFilter(CrashHandler);
    AddVectoredExceptionHandler(1, FirstChanceFailFast);

    // 1a. Open the dedicated crash log NOW (before any pure-virtual /
    //     terminate handler can be hit). Re-opens are no-ops because
    //     CREATE_ALWAYS truncates and OpenCrashLog is also called below
    //     for normal flow — explicit early call here so the pure-virtual
    //     override has somewhere to write if it fires during init.
    OpenCrashLog();

    // 1b. Install the std::terminate handler. libstdc++'s __cxa_pure_virtual
    //     and uncaught C++ exceptions land here; abort() bypasses SEH so
    //     without this, a pure-virtual call leaves zero log evidence.
    std::set_terminate(TerminateHandler);

    // 1b. Reset Video* registry keys before anything else reads them.
    //     Breaks the standalone-shutdown corruption cycle that wedges DX11.
    ResetVideoRegistry();

    // 2. Resolve Runtime-x64.dll and VideoDX11-x64.dll (ASLR)
    printf("[Standalone] Resolving DLLs...\n");
    if (!ResolveInit()) {
        fprintf(stderr, "[Standalone] FATAL: Could not load Runtime-x64.dll\n");
        fprintf(stderr, "[Standalone] Make sure Runtime-x64.dll is in the same directory.\n");
        return false;
    }

    // 2a. IAT-hook MessageBoxW in Runtime-x64.dll so MSVC _purecall popups
    //     get logged + ExitProcess instead of blocking the game on a modal.
    //     Must run AFTER ResolveInit (Runtime needs to be loaded), but
    //     BEFORE any code that could trigger a pure-virtual call.
    if (HookRuntimeMessageBoxW()) {
        printf("[Standalone] Hooked Runtime-x64.dll!user32!MessageBoxW (real=%p)\n",
               (void*)g_realMessageBoxW);
    } else {
        printf("[Standalone] WARN: failed to hook Runtime!MessageBoxW; pure-virtual popups will block\n");
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

    // 4b. Resolve the shipped VideoDX11-x64.dll entry points. Nothing calls
    //     these yet — see dx11/README.md — but resolving here validates the
    //     addresses and logs the ASLR delta, and the delegating DeviceData
    //     (phase 0) will forward through them. Failure is non-fatal: the game
    //     runs on the original plugin either way.
    Dx11FuncsInit();

    // 4a. Optional heap-overflow detector (BANISHED_REDZONE env var). Wraps
    //     fn_MemAlloc/fn_MemFreeStatic with tail-canary redzones so an OOB
    //     write in the menu/geometry deserialize is caught at the source via
    //     RedzoneAlloc_ValidateAll() checkpoints, instead of crashing later in
    //     the allocator free-list. No-op unless the env var is set.
    RedzoneAlloc_Install();

    // 4b. Make the Runtime engine allocator thread-safe. Allocate() takes no
    //     lock while Free() does; the background Loader thread (achievement /
    //     external load) allocates concurrently with the main thread's menu
    //     scene build -> free-list corruption -> intermittent heap crash. The
    //     detour funnels Allocate through the same CS Free uses. Env-gated by
    //     BANISHED_ALLOC_LOCK (default on). See allocator_lock.cpp.
    InstallAllocatorLock();
    // 4b. Also lock the raw Free (0x1800c6c00) — it mutates the same free-list
    //     with no CS, so a background-thread Free races the now-locked Allocate.
    //     Closes the residual intermittent menu heap crash. See allocator_lock.cpp.
    InstallFreeLock();

    // 4c. Diagnostic (session 3): interpose fn_Filename_Dtor to catch the
    //     System::String use-after-free that plants the bucket-12 free-list
    //     corruption. Env-gated BANISHED_FNDTOR_CHECK (default OFF), logs to
    //     stderr. See standalone/filename_dtor_check.cpp.
    InstallFilenameDtorCheck();

    // 4d. Diagnostic (session 4): detour the ACTUAL Runtime ~Filename /
    //     operator= functions (not our global pointers, which fired 0x in
    //     session 3 because the UAF is Runtime-internal) so every caller routes
    //     through a refcount==0 check before the dec/inc. Catches the
    //     Runtime-internal use-after-free that plants the bucket-12 free-list
    //     corruption, with the geometry call stack. Env-gated BANISHED_FNHOOK
    //     (default OFF). See standalone/filename_runtime_hook.cpp.
    InstallFilenameRuntimeHook();

    // 4e. THE MENU CRASH FIX (session 5): serialize the Runtime resource loader
    //     with one global recursive CS taken at BOTH ExternalFactory::FindOrLoad
    //     AND External::Load. The intermittent menu heap crash is a concurrency
    //     race between the main thread and the Runtime background loader thread,
    //     both calling the non-thread-safe FindOrLoad/External::Load (racy
    //     factory-cache Add + racy String refcount inc/dec) — corrupting both the
    //     free-list (the bucket-12 link crash, Runtime+0xC74C3) and the factory
    //     cache (FindOrLoad returns NULL for a real resource -> texture/dialog
    //     null deref). Serializing gives 0 crashes / 24 (pure default), all
    //     drawing the menu, vs ~33% baseline. Locking External::Load too covers
    //     its other callers + the in-game world-build load path. Default ON; opt
    //     out with BANISHED_NO_FOL_LOCK=1. See filename_runtime_hook.cpp.
    InstallFindOrLoadWatch();

    // 4f. Accelerator B (Phase 0): reimplement IO::CompressedStream::Read with an
    //     n<=0 progress guard so a serialize-misalignment over-read logs the
    //     drifting resource + stack and returns false instead of spinning the
    //     loader forever. Gate BANISHED_READ_GUARD=1 (default OFF → no-op). See
    //     filename_runtime_hook.cpp.
    InstallReadGuard();

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

    // NOTE: component type-ID .rdata globals (FillComponentTypeIdGlobals_nature)
    // are filled later, in RunGameMain right after Engine::Startup —
    // Object::Factory::Startup assigns the real CRC type ids only then (here they
    // are still the 0xFFFFFFFF ctor sentinel). See code/Game/Game/game_main.cpp.

    // 7. Skip the launcher dialog — see SkipPlatformConfigure note above.
    SkipPlatformConfigure();

    // 8. Install thread-local CALLWNDPROC hook so we can see WM_ACTIVATE /
    //    WM_KILLFOCUS / WM_SETFOCUS messages reaching the engine WindowProc.
    //    Diagnoses fullscreen focus-loss black-screen issue.
    InstallWndMsgHook();

    // 9. Latch the BANISHED_AUTOSTART env var. When set, the menu update loop
    //    auto-drives New Game -> Start (see standalone/autostart.cpp).
    AutoStart::Init();

    // RunStartupSelfTests() runs from game_main.cpp after Engine::Startup,
    // when the MemoryAllocator singleton is finally populated.

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

    // Open dedicated crash log (FILE_FLAG_WRITE_THROUGH) — survives the OS
    // tearing the crashing thread down before stderr buffers commit.
    OpenCrashLog();

    printf("[Standalone] Banished Reimplementation - Standalone EXE\n");
    printf("[Standalone] Build: %s %s\n", __DATE__, __TIME__);

    // Boot the stall detector so subsequent [Stall] log lines are tagged with
    // milliseconds since boot. Used to hunt remaining splash-flash sources.
    StallDebug::Initialize();
    // 16ms threshold catches anything that would drop a frame at 60Hz.
    StallDebug::SetStallThresholdMs(16.0);

    // Probe for RenderDoc. If the exe was launched via RenderDoc's UI,
    // renderdoc.dll is already loaded into the process and we can drive
    // captures programmatically. No-op when not attached.
    RenderDocIntegration::Init();

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

