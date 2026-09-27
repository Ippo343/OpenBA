// Standalone entry point for Banished reimplementation.
// Replaces Application-x64.exe. Since ENDGAME.md A.1 it also replaces the
// engine: neither Runtime-x64.dll nor VideoDX11-x64.dll is ever loaded, and
// the game loop runs entirely on our reimplemented code.
//
// Build with: make standalone
// Deploy to: C:/GOG Games/Banished/Application-x64.exe (rename original first!)
//
// This is the long-term goal. Initially many functions will hit NOT_IMPLEMENTED
// stubs. As the address registry fills in, more gameplay systems come alive.


// ENDGAME B.3.5: Video::Context, ahead of the <windows.h> that "resolve.h"
// pulls in — <wingdi.h> declares a FUNCTION named Rectangle, which breaks
// Volume.h's Volume::Rectangle (B35 caller brief, the <windows.h> trap).
#include "video/Context.h"
#include "options.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "runtime_funcs.h"
#include "dx11_seam.h"
#include "video_interface.h"
#include "address_registry.h"
#include "renderdoc_integration.h"
#include "stall_debug.h"
#include "boot_state_watchdog.h"
#include "autostart.h"
#include "stress_harness.h"
#include "sampler.h"
#include "hw_watch.h"
#include "redzone_alloc.h"
#include "rt_port.h"                        // RtResolveMissSummary
#include "system/MemoryAllocator.h"   // RtSystemAllocatorSetCutoverArmer
#include "Game/game_main.h"
#include "Object/factory_registration.h"
#include "Goal/StatGoal.h"             // ENDGAME B.3.9 EXA: StatGoal::Vtable() for the self-test
// WAVE 11 follow-up 5 (lane TINFO): the BaseTypeInfo::factoryFn dispatch census.

// Wave 9 lane N's work-scheduler probe tally (code/Game/Action/action_base_complete.cpp).
// Declared here rather than in a header: it has exactly one caller, the crash handler.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <exception>
#include <io.h>      // _get_osfhandle, _fileno
#include <windows.h>
#include <dbghelp.h>

// WAVE 14 LANE SAVEMETA -- runtime/src/save/saveload_meta.cpp.
// ============================================================
// GPU preference exports (match original exe)
// ============================================================
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance = 1;
}

// Session-3 diagnostic installer (standalone/filename_dtor_check.cpp).
// The composed (locked) MemoryAllocator entry points (standalone/alloc_cutover_arm.cpp).
extern "C" bool RtAllocSwapArmCutover(void* ourAllocate, void* ourFree,
                                      void** outComposedAllocate, void** outComposedFree);

// ============================================================
// Mirrors Runtime DLL's BaseTypeInfo struct layout (used by both the probe
// and the External::Load crash diagnostic).
// ============================================================
struct ProbeBaseTypeInfo {
    uint32_t typeId;
    uint32_t pad;
    const char* name;
    const char* parent;
    // +0x18 is the type's FILE EXTENSION, compared with _stricmp in
    // Factory::GetTypeIdForFileExtension (Runtime 0x1800c04c0). It was called
    // `abstract` here until Wave 3 lane A confirmed the layout against the DLL
    // (runtime/types/object/TypeInfo.h). All eight OFFSETS were correct; this
    // one name was not. Nothing reads it, so the misnomer was inert.
    const char* fileExtension;
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
// Log the show-state the process was launched with. Video::Device::PlatformStartup
// (VideoDX11 slot 2) calls ShowWindow(hwnd, SW_SHOWDEFAULT), and SW_SHOWDEFAULT
// means "use STARTUPINFO.wShowWindow from whoever called CreateProcess". So the
// window's initial show state is a property of the launcher, not of the game.
// Launching with wShowWindow = SW_SHOWMINIMIZED (e.g. Start-Process
// -WindowStyle Minimized) really does dead-end in CreateColorBuffer, so this
// stays worth logging. It is NOT, however, the cause of the 136x39 collapse —
// that is an external window manager; see "Who collapses the game window?".
static void DumpStartupInfo() {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    GetStartupInfoW(&si);
    const bool usesShow = (si.dwFlags & STARTF_USESHOWWINDOW) != 0;
    const char* name = "?";
    switch (si.wShowWindow) {
    case SW_HIDE:            name = "SW_HIDE";            break;
    case SW_SHOWNORMAL:      name = "SW_SHOWNORMAL";      break;
    case SW_SHOWMINIMIZED:   name = "SW_SHOWMINIMIZED";   break;
    case SW_SHOWMAXIMIZED:   name = "SW_SHOWMAXIMIZED";   break;
    case SW_SHOWNOACTIVATE:  name = "SW_SHOWNOACTIVATE";  break;
    case SW_SHOW:            name = "SW_SHOW";            break;
    case SW_MINIMIZE:        name = "SW_MINIMIZE";        break;
    case SW_SHOWMINNOACTIVE: name = "SW_SHOWMINNOACTIVE"; break;
    case SW_SHOWNA:          name = "SW_SHOWNA";          break;
    case SW_RESTORE:         name = "SW_RESTORE";         break;
    case SW_SHOWDEFAULT:     name = "SW_SHOWDEFAULT";     break;
    }
    const bool minimizing = usesShow &&
        (si.wShowWindow == SW_SHOWMINIMIZED || si.wShowWindow == SW_MINIMIZE ||
         si.wShowWindow == SW_SHOWMINNOACTIVE);
    printf("[StartupInfo] dwFlags=0x%08lX USESHOWWINDOW=%d wShowWindow=%u (%s)%s\n",
           si.dwFlags, usesShow ? 1 : 0, si.wShowWindow, name,
           minimizing ? "  <-- SW_SHOWDEFAULT WILL MINIMIZE THE GAME WINDOW" : "");
    fflush(stdout);
}

// Read back the Video* keys exactly as they sit on disk, before anything
// overwrites them. This is what Video::Configuration::Startup will parse.
static void DumpVideoRegistry(const char* tag) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Shining Rock Software LLC\\Banished",
                      0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) {
        printf("[VideoReg %s] key absent (first run)\n", tag);
        return;
    }
    static const wchar_t* keys[] = {
        L"VideoSettings", L"VideoInterface", L"VideoFullscreen",
        L"VideoWidth", L"VideoHeight",
        L"VideoWindowX", L"VideoWindowY",
        L"VideoWindowWidth", L"VideoWindowHeight",
    };
    for (const wchar_t* k : keys) {
        wchar_t buf[64] = {0};
        DWORD cb = sizeof(buf), type = 0;
        if (RegQueryValueExW(hKey, k, nullptr, &type,
                             reinterpret_cast<BYTE*>(buf), &cb) == ERROR_SUCCESS) {
            printf("[VideoReg %s]   %-18ls = \"%ls\" (type %lu)\n", tag, k, buf, type);
        } else {
            printf("[VideoReg %s]   %-18ls = <absent>\n", tag, k);
        }
    }
    RegCloseKey(hKey);
    fflush(stdout);
}

// ============================================================
// Unattended-run guard (harness only — NOT a settings override)
//
// Retail's launcher dialog is gated on one bit: Engine::Shutdown stores
// VideoSettings = "Engine::Startup never reached its end", and
// Engine::PlatformConfigure shows the dialog whenever that bit is true. So a
// clean run leaves it false and boots straight into the game; a run that died
// during video init leaves it true and the dialog comes back so the user can
// repair their settings. That is the whole mechanism, and the standalone now
// participates in it unmodified.
//
// The one case it does not survive is ours: during development we kill the
// process constantly, which looks exactly like a crash and leaves the bit set.
// An automated run (make sa-run / sa-run-menu) would then block forever on a
// modal Win32 dialog nobody is there to click.
//
// ---------------------------------------------------------------------------
// HarnessMemoryIsReadable — the readable-memory probe the diff oracle's
// released-pointer detector needs (Wave 9 lane M; installed in Init below).
//
// The detector reads a System::String's {length, refcount} header at [-8] to
// decide whether the block a replay scratch still points at has been released.
// It must never fault doing so: a diagnostic that crashes the process turns a
// harness defect into a crash report about the wrong thing. Same VirtualQuery
// shape as entity_comp_validator.cpp / work_ser_probe.cpp already use.
// ---------------------------------------------------------------------------
static bool HarnessMemoryIsReadable(const void* p, size_t n) {
    if (!p || n == 0) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    const DWORD noRead = PAGE_NOACCESS | PAGE_GUARD;
    if (mbi.Protect == 0 || (mbi.Protect & noRead)) return false;
    // The whole span must sit inside this one region; a span crossing into an
    // unmapped neighbour is exactly the case that would fault.
    const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
    const uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    return addr >= base && (addr + n) <= (base + mbi.RegionSize);
}

// So when — and only when — a harness env var says this run is unattended, we
// assert what we already know ("the previous run was ended deliberately") by
// clearing that one bit through the same registry key retail uses. No byte
// patching, no forced resolutions, no forced renderer.
//
// The second half repairs a non-positive VideoWindowWidth/Height. Those are
// not a user preference — they are the residue of an external window manager
// collapsing the window (PowerToys FancyZones; see the "Who collapses the game
// window?" note below). Video::Device::AdjustDisplay re-reads GetClientRect on
// every call and writes the result back into the configuration, so whatever
// size anything else forced on the window latches into the settings.
// Persisting that makes the next boot fail in
// CreateColorBuffer ("[0x0 4xAA]") and every boot after it. Retail recovers via
// the launcher dialog; an unattended run has no one to drive it, so we drop
// back to the -1 "no override" sentinel the Video::Configuration constructor
// itself uses and let the engine pick the size.
static void HarnessGuardUnattendedBoot() {
    const bool unattended = Opt().unattendedBoot;
    if (!unattended) {
        printf("[Standalone] Attended run — launcher dialog left to retail rules "
               "(it appears iff the last run did not finish Engine::Startup)\n");
    }

    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Shining Rock Software LLC\\Banished",
                      0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) {
        printf("[Standalone] HarnessGuard: settings key absent — nothing to guard\n");
        return;
    }

    // Every Banished setting is REG_SZ and parsed with atoi/strcmp. A REG_DWORD
    // here reads back as absent and the engine keeps its ctor default.
    auto setStr = [&](const wchar_t* name, const wchar_t* val) {
        DWORD bytes = (DWORD)((wcslen(val) + 1) * sizeof(wchar_t));
        RegSetValueExW(hKey, name, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(val), bytes);
    };
    auto getInt = [&](const wchar_t* name, int* out) {
        wchar_t buf[64] = {0};
        DWORD cb = sizeof(buf), type = 0;
        if (RegQueryValueExW(hKey, name, nullptr, &type,
                             reinterpret_cast<BYTE*>(buf), &cb) != ERROR_SUCCESS)
            return false;
        *out = _wtoi(buf);
        return true;
    };

    if (unattended) {
        wchar_t settings[16] = {0};
        DWORD cb = sizeof(settings), type = 0;
        if (RegQueryValueExW(hKey, L"VideoSettings", nullptr, &type,
                             reinterpret_cast<BYTE*>(settings), &cb) == ERROR_SUCCESS &&
            wcscmp(settings, L"true") == 0) {
            setStr(L"VideoSettings", L"false");
            printf("[Standalone] HarnessGuard: VideoSettings true -> false "
                   "(unattended run; suppresses the launcher dialog the previous "
                   "abnormal exit armed)\n");
        }
    }

    // Degenerate window size repair — runs on EVERY launch, attended or not.
    //
    // This is the one deliberate deviation from retail in the settings path,
    // and it is a rejection of impossible values rather than an override of
    // real ones. A collapsed window (the 120x0 client of a minimized frame)
    // gets latched into Video::Configuration by AdjustDisplay and persisted by
    // Engine::Shutdown; on the next boot the windowed override feeds it back
    // into the swap chain and CreateColorBuffer fails with "[0x0 4xAA]", so
    // Engine::Startup returns 0 and the game never boots again. No user ever
    // chose a 120x0 window, so anything below a plausible minimum goes back to
    // the -1 "no override" sentinel that Video::Configuration's own
    // constructor uses, and the engine picks the size.
    constexpr int kMinSaneWindowDim = 64;
    int w = -1, h = -1;
    const bool haveW = getInt(L"VideoWindowWidth",  &w);
    const bool haveH = getInt(L"VideoWindowHeight", &h);
    const bool degenerate = (haveW && w >= 0 && w < kMinSaneWindowDim) ||
                            (haveH && h >= 0 && h < kMinSaneWindowDim);
    if (degenerate) {
        setStr(L"VideoWindowWidth",  L"-1");
        setStr(L"VideoWindowHeight", L"-1");
        printf("[Standalone] Repaired collapsed window size %dx%d -> -1x-1 "
               "(a previous run persisted a minimized frame's client rect; see "
               "Video::Device::AdjustDisplay)\n", w, h);
    }

    RegCloseKey(hKey);
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

// Walk the saved CONTEXT via the x64 unwind tables and emit one line per
// frame: <module>!<offset>. No symbol names — Banished ships stripped — but
// module+offset is what we'd paste into Ghidra anyway.
//
// THE ONE STACK WALKER IN THE EXE. The crash handler below and the hang
// detector in boot_state_watchdog.cpp both go through here; the walker takes a
// line sink instead of writing to the crash log directly so there is exactly
// one copy of the unwind logic to keep right. The hang path passes a sink that
// writes to standalone_hang.log, and it walks OTHER threads' contexts, which is
// why the CONTEXT is copied (RtlVirtualUnwind mutates it) and why nothing here
// touches a CRT FILE*: the hang dump runs with those threads SUSPENDED, and a
// suspended thread can be holding the CRT's file lock.
extern "C" void StandaloneWalkStack(CONTEXT* origCtx, StackLineSink sink, void* user) {
    CONTEXT ctx = *origCtx;
    char line[256];

    for (int frame = 0; frame < 32; frame++) {
        HMODULE mod = nullptr;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)(uintptr_t)ctx.Rip, &mod) && mod) {
            char path[MAX_PATH] = {0};
            GetModuleFileNameA(mod, path, MAX_PATH);
            const char* nm = strrchr(path, '\\');
            uintptr_t off = (uintptr_t)ctx.Rip - (uintptr_t)mod;
            snprintf(line, sizeof(line),
                     "  #%-2d %-22s + 0x%-8llX  (RIP=%016llX RSP=%016llX)\n",
                     frame, nm ? nm + 1 : path, (unsigned long long)off,
                     (unsigned long long)ctx.Rip, (unsigned long long)ctx.Rsp);
        } else {
            snprintf(line, sizeof(line),
                     "  #%-2d ???                    @ %p          (RSP=%016llX)\n",
                     frame, (void*)ctx.Rip, (unsigned long long)ctx.Rsp);
        }
        sink(user, line);

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

// The crash handler's view of the walker: every line straight to the
// write-through crash log.
static void CrashLogStackSink(void*, const char* s) { CrashLogf("%s", s); }

static void PrintStackTrace(CONTEXT* origCtx) {
    CrashLogf("Stack trace:\n");
    StandaloneWalkStack(origCtx, &CrashLogStackSink, nullptr);
}

// Dedicated crash log handle, opened with FILE_FLAG_WRITE_THROUGH so every
// WriteFile call commits synchronously to disk. We bypass the CRT entirely
// because fprintf-to-stderr proved racy under crashing-thread teardown:
// the kernel write was buffered and got cut mid-line by TerminateProcess.
static HANDLE g_crashLog = INVALID_HANDLE_VALUE;

static void OpenCrashLog() {
    if (g_crashLog != INVALID_HANDLE_VALUE) return;  // idempotent
    g_crashLog = CreateFileA(
        "standalone_crash.log",
        GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        nullptr);
    if (g_crashLog != INVALID_HANDLE_VALUE) return;

    // A previous crashed run can linger as a zombie that never releases its
    // handles (taskkill reports "no running instance" while tasklist still
    // lists it). It holds standalone_crash.log with FILE_SHARE_READ only, so
    // this GENERIC_WRITE open is denied -- and then EVERY CrashLogf silently
    // drops, which loses the fault address and stack for the run that matters.
    // Fall back to a pid-suffixed name so a dump is always written somewhere.
    char alt[64];
    wsprintfA(alt, "standalone_crash_%lu.log", GetCurrentProcessId());
    g_crashLog = CreateFileA(
        alt,
        GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        nullptr);
    if (g_crashLog != INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[CrashLog] standalone_crash.log is locked (stale zombie "
                        "process?) -- this run's dump goes to %s\n", alt);
        fflush(stderr);
    }
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

    // Check registry stats
    RegistryStats stats = GetRegistryStats();
    CrashLogf("Registry: %zu registered, %zu stubs hit\n",
              stats.registered, stats.stubs);

    // B34B_RULES 6.3 (open defect): the allocator "LockEnter on a NULL lock" crash is
    // Get() returning null, i.e. EnterCriticalSection(0x640). Two windows fit that stack
    // and only this value separates them: a NULL cell with a sane heap is W2 (Destruct
    // nulled it while another thread still allocates); a NULL cell with a corrupted free
    // list is W2' (a wild write landed on the minted .bss cell). Diagnostic only.
    {
        void** const cell = reinterpret_cast<void**>(
            ResolveAddr(Module::Runtime, 0x1801fc718));  // ?_singleton@MemoryAllocator@System@@
        CrashLogf("MemoryAllocator::_singleton cell=%p value=%p\n",
                  static_cast<void*>(cell), cell ? *cell : nullptr);
    }

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

    // The DX11 verification evidence, printed here as well as from atexit:
    // this handler ends in ExitProcess, which skips atexit entirely, and a
    // session that dies on the way out is exactly the one whose "did any
    // shipped renderer code run?" answer is worth having.
    VideoDX11::Dx11SeamReport();

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

// `expectedVtable` is the class's COMPILER table (`X::Vtable()`): the test asks
// "did the factory build THIS class", an identity question (B38_RULES D5). It
// used to take the retail table address and read it back through the address
// registry, which only served the compiler table because a RegisterExeAddr row
// existed for this test alone (ENDGAME B.3.9 EXA deleted it).
static bool RunFactorySelfTest(const char* typeName,
                               const void* expectedVtable,
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
    void* actualVt = *(void**)obj;
    if (actualVt != expectedVtable) {
        printf("[SelfTest %s] FAIL: vtable %p != expected %p (the class's compiler table)\n",
               typeName, actualVt, expectedVtable);
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

        // The window block. Video::Device::AdjustDisplay's windowed branch
        // rewrites +0x1c/+0x20 from GetClientRect on every call, so any size an
        // external window manager forces on the window ends up here. Whatever
        // sits here now is what Video::Configuration::Save writes to the
        // registry at shutdown, so a 0 in either field means the next boot is
        // poisoned.
        const int cfgWinX = *reinterpret_cast<int*>((uintptr_t)config + 0x14);
        const int cfgWinY = *reinterpret_cast<int*>((uintptr_t)config + 0x18);
        const int cfgWinW = *reinterpret_cast<int*>((uintptr_t)config + 0x1c);
        const int cfgWinH = *reinterpret_cast<int*>((uintptr_t)config + 0x20);
        printf("[VideoProbe]   +0x00 settings  = %d  (true => launcher dialog next boot)\n",
               *reinterpret_cast<uint8_t*>((uintptr_t)config + 0x00));
        printf("[VideoProbe]   +0x14 windowX   = %d\n", cfgWinX);
        printf("[VideoProbe]   +0x18 windowY   = %d\n", cfgWinY);
        printf("[VideoProbe]   +0x1c windowW   = %d%s\n", cfgWinW,
               cfgWinW <= 0 ? "   <-- POISONED (AdjustDisplay latched an externally-forced size)" : "");
        printf("[VideoProbe]   +0x20 windowH   = %d%s\n", cfgWinH,
               cfgWinH <= 0 ? "   <-- POISONED (AdjustDisplay latched an externally-forced size)" : "");
        printf("[VideoProbe]   +0x40 fullscreen= %d  +0x41 vsync=%d  +0x42 skipShow=%d\n",
               *reinterpret_cast<uint8_t*>((uintptr_t)config + 0x40),
               *reinterpret_cast<uint8_t*>((uintptr_t)config + 0x41),
               *reinterpret_cast<uint8_t*>((uintptr_t)config + 0x42));
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
                nm = nm ? nm + 1 : path;
                printf("[VideoProbe]   active vtable lives in: %s\n", nm);

                // The active vtable must be one of ours, i.e. in this exe.
                //
                // The failure this catches: if Video::Device cannot bring up the
                // configured interface it falls back to interface 0 and loads
                // VideoDX9-x64.dll, and the game then renders correctly on a
                // completely original DX9 plugin. Verified 2026-07-24; the only
                // sign in a 10k-line log was one "seam not installed" line,
                // which is not enough to stop someone A/B-ing our port against
                // a run that never used it.
                const char* expected = "reimpl-x64.exe";
                if (_stricmp(nm, expected) != 0) {
                    fprintf(stderr,
                            "[VideoProbe] *** THE DX11 PORT IS NOT RUNNING ***\n"
                            "[VideoProbe] The active renderer's vtable lives in %s; "
                            "it should be %s.\n"
                            "[VideoProbe] Video::Device fell back to another plugin, so "
                            "every draw is ORIGINAL code.\n"
                            "[VideoProbe] Anything measured from this run is meaningless.\n",
                            nm, expected);
                    fflush(stderr);
                }
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
    RunFactorySelfTest("StatGoal", StatGoal::Vtable(), 0x80,
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
    if (!ctx) return;
    *(ml + 0x18) = 0;  // engine clears m_videoContextDirty before Reset
    reinterpret_cast<::Video::Context*>(ctx)->Reset();  // B.3.5 bridge
    printf("[CtxReset %s] Video::Context::Reset(ctx=%p) — FSE refocus recovery\n",
           tag, ctx);
    fflush(stdout);
}

// Swap the engine's window-class background brush from the default WHITE_BRUSH
// to a black brush, so the window is black — not white — for the ~1s between
// ShowWindow (VideoDX11 CreateDevice step 6) and the first splash Present.
//
// This used to fire on the FIRST message from ANY window on this thread and
// then latch, which was a race: the engine window is not the only top-level
// window the main thread owns, and whichever one happened to be messaged first
// got the black brush. When the engine window lost that race it kept the class
// default (COLOR_WINDOW+1, i.e. white) for the whole boot — the intermittent
// white box. Identify the engine window explicitly instead, and don't latch
// until we have actually found it.
//
// Primary test is the window class name, which Runtime registers as "Banished".
// Fallback is Video::Device::_singleton+0x90, the HWND the renderer itself
// uses — populated before CreateDevice runs, so it is available in time.
static bool IsEngineWindow(HWND hwnd) {
    wchar_t cls[64] = {0};
    if (GetClassNameW(hwnd, cls, 64) > 0 && wcscmp(cls, L"Banished") == 0)
        return true;

    void** devSlot = reinterpret_cast<void**>(
        ResolveAddr(Module::Runtime, 0x1801fe7a8));
    void* device = devSlot ? *devSlot : nullptr;
    if (!device) return false;
    HWND devHwnd = *reinterpret_cast<HWND*>((uintptr_t)device + 0x90);
    return devHwnd && devHwnd == hwnd;
}

static void EnsureBlackWindowBrush(HWND hwnd) {
    static bool done = false;
    if (done || !hwnd) return;
    if (!IsEngineWindow(hwnd)) return;   // not ours — keep looking
    HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
    if (!black) return;
    HBRUSH old = (HBRUSH)SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)black);
    InvalidateRect(hwnd, nullptr, TRUE);  // force a repaint with the new brush
    UpdateWindow(hwnd);
    done = true;
    printf("[Standalone] Swapped window class background brush from %p to "
           "BLACK_BRUSH (%p) on engine hwnd=%p\n", (void*)old, (void*)black, (void*)hwnd);
}

static LRESULT CALLBACK CallWndProcHook(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0) {
        auto* msg = reinterpret_cast<CWPSTRUCT*>(lParam);
        // Swap the class brush the first time we see a message for the ENGINE
        // window specifically. Other top-level windows on this thread are
        // ignored — see EnsureBlackWindowBrush for why matching on "first
        // message from any window" was wrong.
        EnsureBlackWindowBrush(msg->hwnd);
        // WM_SIZE / WM_WINDOWPOSCHANGING carry the dimensions that end up in
        // Video::Device+0x04/+0x08 and then in VideoWindowWidth/Height. Decode
        // them so a collapse to a minimum-size window is attributable to the
        // message that caused it.
        if (msg->message == 0x0005 /*WM_SIZE*/) {
            printf("[WndMsg] WM_SIZE               hwnd=%p type=%llu %ux%u\n",
                   (void*)msg->hwnd, (unsigned long long)msg->wParam,
                   (unsigned)(msg->lParam & 0xFFFF),
                   (unsigned)((msg->lParam >> 16) & 0xFFFF));
            fflush(stdout);
        } else if (msg->message == 0x0046 /*WM_WINDOWPOSCHANGING*/) {
            auto* wp = reinterpret_cast<WINDOWPOS*>(msg->lParam);
            if (wp) {
                printf("[WndMsg] WM_WINDOWPOSCHANGING hwnd=%p %d,%d %dx%d flags=0x%X\n",
                       (void*)msg->hwnd, wp->x, wp->y, wp->cx, wp->cy, wp->flags);
                fflush(stdout);
            }
        }
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

// ============================================================
// Who collapses the game window?  ANSWERED 2026-07-24: PowerToys FancyZones.
//
// A SetWindowPos to 136x39 at an off-screen origin lands on the engine window
// about 1.6s in, carrying SWP_ASYNCWINDOWPOS. Video::Device::AdjustDisplay then
// latches the resulting 120x0 client rect into Video::Configuration, and every
// later call re-applies it (240 times in the captured run).
//
// The caller is an EXTERNAL PROCESS: FancyZones, with no app exclusions and
// openWindowOnActiveMonitor set, snapping the newly created window into a zone
// on another monitor. Adding reimpl-x64.exe to fancyzones_excluded_apps takes
// the collapse from 240 occurrences to 0 with no code change. Nothing in this
// exe ever asked for that size.
//
// Two readings that wasted time here, recorded so they are not repeated:
//   * 136x39 is SM_CXMIN x SM_CYMIN -- the OS *minimum tracking size*, i.e. the
//     clamp on a degenerate request. It is NOT SM_CXMINIMIZED x SM_CYMINIMIZED
//     (160x28). WM_SIZE carries type=0 (SIZE_RESTORED) and IsIconic() is 0, so
//     the window is never minimized at any point.
//   * SWP_ASYNCWINDOWPOS is passed by the CALLER, not added by the OS. It is
//     the tell for a cross-process caller.
//
// The in-process user32 IAT interposers that chased this lived here; they hooked
// Runtime-x64.dll's imports and died with the DLL (ENDGAME.md A.1). They could
// never have seen the culprit anyway. The cheap discriminator is in
// dx11/src/devicedata.cpp: if DeviceData_Resize already sees a degenerate client
// rect on entry, the caller is external.
// ============================================================

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
static void HwWatch_LogStack(void* ctx) {
    if (g_crashLog == INVALID_HANDLE_VALUE) OpenCrashLog();
    CrashLogRaw("\n=== HWWATCH HIT (data breakpoint) ===\n");
    PrintStackTrace(reinterpret_cast<CONTEXT*>(ctx));
    CrashLogf("=== END HWWATCH ===\n");
    FlushLogsToDisk();
    // Mirror the frames to stderr too (the run harness keeps stderr per run).
    CONTEXT c = *reinterpret_cast<CONTEXT*>(ctx);
    for (int frame = 0; frame < 24; frame++) {
        HMODULE mod = nullptr;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)(uintptr_t)c.Rip, &mod) && mod) {
            char path[MAX_PATH] = {0};
            GetModuleFileNameA(mod, path, MAX_PATH);
            const char* nm = strrchr(path, '\\');
            fprintf(stderr, "[HWWATCH]   #%-2d %-18s + 0x%llX\n", frame, nm ? nm + 1 : path,
                    (unsigned long long)((uintptr_t)c.Rip - (uintptr_t)mod));
        } else fprintf(stderr, "[HWWATCH]   #%-2d ??? @ %p\n", frame, (void*)c.Rip);
        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(c.Rip, &imageBase, nullptr);
        if (!fn) {
            uintptr_t* sp = reinterpret_cast<uintptr_t*>(c.Rsp);
            if (IsBadReadPtr(sp, sizeof(uintptr_t))) break;
            c.Rip = *sp; c.Rsp += 8;
            if (!c.Rip) break;
            continue;
        }
        PVOID hd = nullptr; ULONG64 est = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, c.Rip, fn, &c, &hd, &est, nullptr);
        if (!c.Rip) break;
    }
    fflush(stderr);
}

static LONG WINAPI FirstChanceFailFast(EXCEPTION_POINTERS* ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_SINGLE_STEP && HwWatch_Armed()) {
        if (HwWatch_OnSingleStep(ep, &HwWatch_LogStack)) return EXCEPTION_CONTINUE_EXECUTION;
    }
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
    HwWatch_Init();
    Sampler_Init();   // BANISHED_SAMPLE=<hz>: sim-thread sampling profiler

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

    // 1b. Log the Video* keys exactly as they sit on disk, then apply the
    //     unattended-run guard (no-op for a normal launch). Everything the
    //     engine reads comes from the registry via Video::Configuration::Startup.
    DumpVideoRegistry("AT-BOOT");
    HarnessGuardUnattendedBoot();
    // Start publishing boot state BEFORE Engine::Startup: the launcher dialog the guard
    // above tries to suppress opens inside Startup, and if it does appear the process
    // stays alive pumping messages while the log goes silent -- which reads exactly like
    // a slow load. See boot_state_watchdog.cpp.
    BootState::Start();
    StallDebug::Mark("registry probe + harness guard");

    // 2. Address resolution. No engine DLL is loaded (ENDGAME.md A.1).
    printf("[Standalone] Resolving addresses (no engine DLLs are loaded)...\n");
    StallDebug::Mark("(pre ResolveInit)");
    if (!ResolveInit()) {
        fprintf(stderr, "[Standalone] FATAL: ResolveInit failed -- Runtime-x64.dll is "
                        "mapped in this process and must not be (ENDGAME A.1).\n");
        return false;
    }

    StallDebug::Mark("ResolveInit");

    // 2b-i. Allocator ownership, in the ONE order that is safe.
    //
    //     The DLL-ABSENT arming path for the ported allocator: with no DLL there
    //     are no DLL-internal callers to redirect, so registering the exports IS
    //     the complete swap -- but the composed (locked) Allocate/Free entry
    //     points still have to come from somewhere, and they come from here.
    //     Without this seam RtSystemAllocatorRegister() refuses and
    //     MemAlloc/MemFree are left unresolved.
    RtSystemAllocatorSetCutoverArmer(&RtAllocSwapArmCutover);


    // 2c. Runtime-x64.dll port registry (docs/runtime_port/PLAN.md §4.1).
    //     Must run BEFORE AddressRegistryInit and EngineFuncsInit: both cache
    //     Runtime function pointers through ResolveExport/ResolveAddr, and a
    //     port registered after they run would be resolved by nobody.
    RuntimePortInit();
    StallDebug::Mark("RuntimePortInit");

    // 3. Populate address registry (exe dispatch slots, code fns, vtables).
    //    Must happen BEFORE EngineFuncsInit, which reads from these slots.
    printf("[Standalone] Initializing address registry...\n");
    AddressRegistryInit();
    StallDebug::Mark("AddressRegistryInit");

    // 4. Resolve engine function pointers from Runtime DLL
    printf("[Standalone] Resolving engine functions...\n");
    if (!EngineFuncsInit()) {
        printf("[Standalone] WARNING: Some engine functions not resolved (non-critical)\n");
    }
    RuntimeFuncsInit();
    StallDebug::Mark("EngineFuncsInit + RuntimeFuncsInit");

    // 4b. Install the renderer seam: verify all 84 vtable slots against the
    //     map in dx11/README.md and patch in our reimplementations.
    //
    //     VideoDX11-x64.dll is never loaded, so the seam's vtables are our own
    //     exe-owned arrays. This must run before Engine::Startup, so no *Data
    //     object has been created yet. There is no window inside
    //     Engine::Startup: Video::Device::InitializeInterfaces and
    //     CreateVideoInterface both run within that one call, which is why the
    //     seam is the vtables rather than the Device+0xd0/+0xd8 pointers.
    VideoDX11::Dx11SeamInstall();

    //     Our twelve vtables live in this exe and VideoDX11-x64.dll is never
    //     loaded. The DX11 interface slot (Video::Device+0xc8/+0xd0/+0xd8) is
    //     filled by VideoInterfaceFillDx11Slot, called as the first statement of
    //     our ported Video::Device::InitializeInterfaces.
    StallDebug::Mark("dx11 seam");

    // 4a. Optional heap-overflow detector (BANISHED_REDZONE env var). Wraps
    //     fn_MemAlloc/fn_MemFreeStatic with tail-canary redzones so an OOB
    //     write in the menu/geometry deserialize is caught at the source via
    //     RedzoneAlloc_ValidateAll() checkpoints, instead of crashing later in
    //     the allocator free-list. No-op unless the env var is set.
    RedzoneAlloc_Install();

    // 4c. The BANISHED_FNDTOR_CHECK use-after-free interposer used to install
    //     here. It worked by swapping the global `fn_Filename_Dtor` function
    //     pointer for a checking wrapper; ENDGAME B.3.1 deleted that pointer
    //     along with the rest of the C seam (System::Filename is a real class
    //     with a real destructor now), so the diagnostic has no seam to sit in
    //     and the flag is gone from standalone/options.h with it.

    // 4d. THE MENU CRASH FIX (session 5) used to install here as a pair of DLL
    //     byte patches. It now lives in the ported bodies themselves: the
    //     recursive CS is standalone/res_load_lock.cpp, taken by
    //     ExternalFactory::FindOrLoad and External::Load directly. See the
    //     banner in standalone/res_load_lock.h for why an install site was the
    //     wrong home for it.
    StallDebug::Mark("redzone/alloc-lock/diagnostic installers");

    // 5. PROBE: dump _firstTypeInfo state right after Runtime DLL load.
    //    Confirms Runtime self-registered its types via inlined CRT-init stubs
    //    at 0x180147xxx. Should print 150+ Runtime types (Dialog, Description,
    //    Element subclasses, etc.).
    DumpFirstTypeInfo("BEFORE InvokeAllRegistrations");
    StallDebug::Mark("DumpFirstTypeInfo (before)");

    // 6. Drain pending Factory registrations into Runtime _firstTypeInfo.
    //    Static-init ctors collected them; now we flush by calling
    //    Object::BaseTypeInfo::BaseTypeInfo for each. Engine::Engine::Startup
    //    -> Object::Factory::Startup will later walk the list and populate
    //    the typeId tree.
    printf("[Standalone] Invoking factory registrations...\n");
    Object::InvokeAllRegistrations();
    StallDebug::Mark("Object::InvokeAllRegistrations");

    DumpFirstTypeInfo("AFTER InvokeAllRegistrations");
    StallDebug::Mark("DumpFirstTypeInfo (after)");

    // NOTE: component type-ID .rdata globals (FillComponentTypeIdGlobals_nature)
    // are filled later, in RunGameMain right after Engine::Startup —
    // Object::Factory::Startup assigns the real CRC type ids only then (here they
    // are still the 0xFFFFFFFF ctor sentinel). See code/Game/Game/game_main.cpp.

    // 7. (was SkipPlatformConfigure) The launcher dialog is no longer patched
    //    out. Engine::PlatformConfigure skips it on its own whenever
    //    VideoSettings is false, which a clean Engine::Shutdown now stores —
    //    see HarnessGuardUnattendedBoot for the whole mechanism.

    // 8. Install thread-local CALLWNDPROC hook so we can see WM_ACTIVATE /
    //    WM_KILLFOCUS / WM_SETFOCUS messages reaching the engine WindowProc.
    //    Diagnoses fullscreen focus-loss black-screen issue.
    InstallWndMsgHook();

    // 9. Latch the BANISHED_AUTOSTART env var. When set, the menu update loop
    //    auto-drives New Game -> Start (see standalone/autostart.cpp).
    AutoStart::Init();
    StallDebug::Mark("WndMsgHook + AutoStart::Init");

    // RunStartupSelfTests() runs from game_main.cpp after Engine::Startup,
    // when the MemoryAllocator singleton is finally populated.

    printf("[Standalone] Init complete.\n");
    return true;
}

// ============================================================
// Entry point
// ============================================================
// Wave 9 lane ENG (runtime/src/engine/engine_lifecycle.cpp). A linkage
// specification is not allowed at block scope, so it is declared here.
extern "C" void RtEngineLifecycleSummary();
// Wave 23 lane ENGINE. PlatformConfigure is the cutover boot wall and
// LoadResources runs on the loader thread; both are boot/load-once bodies whose
// only coverage question is "did it run, and which branch".
extern "C" void RtEnginePlatformSummary();
extern "C" void RtEngineLoadResourcesSummary();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int) {
    // Redirect stdout/stderr to log files.
    //
    // No AllocConsole. Retail is a GUI-subsystem process with no console, and
    // so are we (-mwindows) — a console here was purely a leftover: the two
    // freopen calls below rebind stdout/stderr to files immediately, so the
    // console never received a single byte. It also had a real cost: an extra
    // top-level window shifts the CW_USEDEFAULT cascade that places the engine
    // window at creation time, and it sat on top of the game window during the
    // pre-Present part of boot.
    freopen("standalone_debug.log", "w", stdout);
    freopen("standalone_error.log", "w", stderr);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    // Open dedicated crash log (FILE_FLAG_WRITE_THROUGH) — survives the OS
    // tearing the crashing thread down before stderr buffers commit.
    OpenCrashLog();

    // ENDGAME C.3: the ONE read of the environment. Every BANISHED_* flag lands
    // in the Options struct here and nothing else in standalone/ calls getenv.
    // It also expands BANISHED_STRESS=1 into its bundle (autostart, medium map,
    // uncapped, pinned camera, demo buildings, in-game quit), which is why it
    // must come before every other reader — HarnessGuardUnattendedBoot below
    // keys off the autostart flag that bundle sets.
    OptionsInit();

    // The stress town's own state (home-loss watch, the [Stress] banner).
    StressHarness_Init();

    printf("[Standalone] Banished Reimplementation - Standalone EXE\n");
    printf("[Standalone] Build: %s %s\n", __DATE__, __TIME__);

    // Must be logged before anything creates a window — see DumpStartupInfo.
    DumpStartupInfo();

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
            "Failed to initialize. Check standalone_debug.log for details.",
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
    // Unregistered-address census (BANISHED_UNREG_CENSUS=1). Dumped here and
    // not from atexit: the crash handler ends in ExitProcess, which skips it.
    DumpUnregCensus();
    // Wave 9 lane F: symbols that resolved to NOTHING. Always on, prints only
    // when the list is non-empty. After lane G this is the missing-symbol list.
    DumpUnresolvedSymbols();
    // WAVE 11 LANE OBJDATA: gate (b) for the four ??_7…@Object@@6B@ minted rows
    // — entries, and which table each entering object carried. Called HERE and
    // not from a static destructor because it asks the PORT REGISTRY which door
    // a symbol resolves to, and by static-destructor time the registry is torn
    // down and every row reads "unregistered" (its own screen0 caught that).
    // WAVE 11 FOLLOW-UP 5 (lane TINFO): every BaseTypeInfo::factoryFn dispatch
    // this run, classified by WHOSE node it went through. Follow-up 2 made the
    // 104 minted factory abort traps reachable in principle; this says whether
    // any of them is reachable in fact. Always printed, including zero, and it
    // carries its own two controls -- read them before trusting the zero.
    { extern void RtObjectTypeInfoAccessorsReport();
      RtObjectTypeInfoAccessorsReport(); }
    // WAVE 11 LANE OBJLIFE: attendance for Object::Internal's TABLE tier. One
    // line, ALWAYS, including zero -- a tier that is armed and never entered
    // looks exactly like a tier that is armed and correct if nothing counts
    // entries, and "it booted fine" is not a measurement of a body that never
    // ran (reference_a_green_readiness_screen_is_not_a_route).
    { extern void RtObjectInternalAttendance(); RtObjectInternalAttendance(); }
    // WAVE 11 FOLLOW-UP 5 (lane TINFO): which vtable our Internal ctor stamps.
    // Condition (b) of the WAVE11.md advisor amendment, as a log line.
    { extern void RtObjectInternalVtblPin(); RtObjectInternalVtblPin(); }
    // Wave 9 lane FLUSH: UI::SpriteContext::Flush's per-branch counts. A
    // `divergences=0` with no idea WHICH branches ran is the shape lane UIF's
    // own section 11.3 mis-read, so the branch split is printed next to it.
    { extern void RtUiSpriteContextFlushReport(); RtUiSpriteContextFlushReport(); }
    // Wave 9 lane ENG: did Engine::Engine's ctor / Startup / dtor / Shutdown
    // actually RUN? Each executes at most once per process, so the diff
    // harness's divergences= column says nothing on its own; this is the
    // effectiveness control. Always on -- it prints only if one of them ran.
    RtEngineLifecycleSummary();
    RtEnginePlatformSummary();
    RtEngineLoadResourcesSummary();
    // Wave 23 lane GFXBODY: attendance for the four FrameCopyImageBuffer bodies
    // and AnimationManager::Update. Always on and unconditional, because the
    // number this lane needs is the ZERO — a run that never entered a ported
    // body must SAY so rather than stay silent
    // (reference_uncalled_port_is_not_verified.md). `Generate` reports its BLIT
    // count separately from its entry count: its whole body sits behind one
    // equality test, so an entry count cannot tell you the copy happened.
    { extern void RtGraphicsWave23GfxbodySummary(); RtGraphicsWave23GfxbodySummary(); }
    // Wave 27 lane GFXPART: attendance for the six particle vtable slot bodies.
    // Unconditional for the same reason as the line above -- the number this lane
    // needs most is the ZERO, and four of its six rows dispatch only through a
    // vtable that is NOT promoted today, so a zero here is the EXPECTED reading
    // and must be printed rather than inferred from silence.
    { extern void RtGraphicsWave27GfxpartSummary(); RtGraphicsWave27GfxpartSummary(); }
    // Wave 28 lane PARTSYS. UNCONDITIONAL, so the expected zeros are PRINTED
    // rather than inferred from silence (reference_uncalled_port_is_not_verified.md).
    { extern void RtGraphicsWave28PartsysSummary(); RtGraphicsWave28PartsysSummary(); }
    // WAVE 29 LANE VERIFY — defect #7 from the Wave 28 ledger, FIXED HERE.
    //
    // These three summaries were WRITTEN, each with the standard "a run that
    // entered none of these must SAY so rather than stay silent" comment above
    // it, and NOTHING CALLED THEM. Three waves of Graphics attendance numbers
    // (Wave 24 GFXMAT 6 rows, Wave 27 GFXBIG 4 rows, Wave 28 GFXBODY 8 rows —
    // 18 registered rows, all DEFAULT-ON kill-switch waves) were therefore
    // never printed by any run, and the silence was read as attendance 0.
    //
    // A zero from an unreached printf and a zero from an unentered body are the
    // same character in a log. That is the defect: the instrument could not
    // express the difference (reference_diagnostic_field_cannot_express_the_answer.md).
    // Reachability of every *Summary/*Report/*Census in the tree is now a screen
    // of its own, recorded in docs/runtime_port/WAVE29_LANE_VERIFY.md §2.
    { extern void RtGraphicsWave24GfxmatSummary();  RtGraphicsWave24GfxmatSummary(); }
    { extern void RtGraphicsWave27GfxbigSummary();  RtGraphicsWave27GfxbigSummary(); }
    { extern void RtGraphicsWave28GfxbodySummary(); RtGraphicsWave28GfxbodySummary(); }
    // Wave 29 lane GFXBODY. UNCONDITIONAL, and WIRED ON PURPOSE: Wave 28 defect
    // #7 is that every lane's *Summary() is written and nothing calls it, which
    // makes a zero mean "never printed" rather than "never called" and made a
    // lane unable to read its own BRANCH counters at all. This lane's doc
    // reports that these lines were SEEN TO PRINT before any zero in them was
    // believed (reference_uncalled_port_is_not_verified.md).
    { extern void RtGraphicsWave29GfxbodySummary(); RtGraphicsWave29GfxbodySummary(); }
    // Wave 9 lane H: 0 misses == every cached Runtime resolve found its symbol.
    // A non-zero count names the symbols that would be unminted at cutover.
    RtResolveMissSummary();
    // Wave 9 lane H: is the early-snapshot port-bypass class LIVE? A number,
    // not an argument -- see runtime/funcs/rt_resolve_cache.cpp.
    RtResolveEarlySummary();
    // Wave 11 follow-up 3 lane AVL: ExternalFactory::SerializeReferenceTable's
    // attendance counters and its MUTATION A post-condition. Printed here, from
    // the process exit path rather than a static destructor, because an
    // instrument outside its subject's live window prints a plausible ZERO
    // rather than an error.
    { extern void RtObjectExternalRefTableSummary();      RtObjectExternalRefTableSummary(); }

    return rc;
}

