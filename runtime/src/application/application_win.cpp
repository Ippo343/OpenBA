// ============================================================================
// runtime/src/application/application_win.cpp — Wave 8 lane E, Wave 12 lane C
//   Application::Application::PlatformUpdate / SleepThread / YieldToThreads /
//   EnableCursor / LaunchProcess
// ============================================================================
// The members of this class that touch Win32 directly, so they live behind
// the PLAN.md §4.1 decision 1 platform seam: <windows.h> must never appear in
// application.cpp (IsPaused/Quit).
//
//   ?PlatformUpdate@Application@1@AEAA_NXZ    retail 0x18009fdf0   ord 3410  (private)
//   ?SleepThread@Application@1@QEAAXH@Z       retail 0x18009f850   ord 4069
//   ?YieldToThreads@Application@1@QEAAXXZ     retail 0x18009f920   ord 4568
//   ?EnableCursor@Application@1@QEAAX_N@Z     retail 0x18009f800   ord 2292
//   ?LaunchProcess@Application@1@QEAA_NPEBD_N@Z retail 0x18009f5e0 ord 3179
//
// NONE OF THE FIRST THREE HAS AN ORACLE, and WAVE8.md §5 says so explicitly
// before any lane started: "running both pumps two message loops or sleeps
// twice."
//   * PlatformUpdate drains the live Win32 message queue with PeekMessageW(…,
//     PM_REMOVE) — a second call would eat the NEXT message, not the same one.
//   * SleepThread and YieldToThreads block/yield the calling thread; calling
//     the DLL's copy afterward would sleep or yield a second time for real.
// All three are registered anyway (, matching the
// ~SavedState precedent in runtime/src/save/saveload.cpp — a site that is
// counted by `calls=` but never compared) because each is unambiguous, short,
// and reproduces retail's own Win32 call sequence exactly rather than
// approximating it.
//
// EnableCursor and LaunchProcess are Wave 12 lane C's two, and each needed a
// DIFFERENT answer to "how do you oracle a function with a real side effect,
// without performing that side effect twice":
//   * EnableCursor's own body has NO direct Win32 call at all — see its own
//     banner below — and is written against the live sink (rt_sink.h).
//   * LaunchProcess spawns a real process or opens a real shell document; a
//     predicting oracle cannot help because the state it changes (the OS
//     process table, or whatever ShellExecuteW launches) is not a memory
//     region either side could snapshot or restore. It is the diff oracle'd
//     like the three above, for the same reason: calling the DLL's copy
//     afterward would launch (or open) a SECOND time for real.
// ============================================================================

#include <windows.h>

#include <cstdint>
#include <cstring>
#include "../../funcs/rt_sink.h"   // RtSink, the write sink EnableCursorBody takes
#include "../../types/application/Application.h"
#include "../collection/set_avl.h"
#include "../system/timer.h"
#include "../system/widestring.h"

// ENDGAME B.3.7: `Application::Application` is the real class; a global
// `using RtApplication::Application;` is ill-formed next to the namespace.
using App = Application::Application;
using RtSystem::WideString;

namespace {

// System::Timer's four data exports.
//
// These were four RAW-DOOR data reads (`the raw DLL door(double*, "?_secondsPerTick@…")`
// and its three siblings) — WAVE9_LANE_V.md §4's DATA READ row for
// `?_secondsPerTick@Timer@System@@0NA` names this file at :45. Wave 9 lane B3
// ported System::Timer and the port OWNS the storage behind all five statics
// (runtime/src/system/timer.h), so the reads come from us now and the row is
// closed by a port rather than by lane C2 minting a value.
//
// Two behavioural notes, both improvements and both stated rather than assumed:
//   * `_ticksPerSecond` and `_secondsPerTick` are compile-time constants
//     (1000000 and 1e-6) — see timer.cpp's header for the disassembly.
//   * the DLL's `_systemInitialTick`/`_systemTicksPerSecond` are ZERO until
//     Engine::Startup runs, and QpcToEngineTicks below divided by the latter.
//     Our accessors run PlatformStartup on first use, so that divide is now
//     safe at every point in boot.
//
// The epoch consequence: SleepThread's engine-tick readings are now measured
// from OUR PlatformStartup sample, not the DLL's. It only ever computes a
// DIFFERENCE of two readings taken inside the same call (`elapsed`), so a
// shifted origin cancels exactly.
int64_t QpcToEngineTicks(int64_t qpc) {
    return (qpc - RtSystem::TimerSystemInitialTick()) * RtSystem::TimerTicksPerSecond()
           / RtSystem::TimerSystemTicksPerSecond();
}

}  // namespace

// ============================================================================
// Application::Application::PlatformUpdate()   retail 0x18009fdf0   ord 3410
// ?PlatformUpdate@Application@1@AEAA_NXZ
// ----------------------------------------------------------------------------
//   if (_quitRequested) PostQuitMessage(0);
//   got = PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE);
//   for (;;) {
//       if (!got) return true;
//       TranslateMessage(&msg); DispatchMessageW(&msg);
//       if (msg.message == WM_QUIT) break;
//       got = PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE);
//   }
//   return false;
// ============================================================================
bool Application::Application::PlatformUpdate() {
    if (_quitRequested) PostQuitMessage(0);

    MSG msg;
    BOOL got = PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE);
    for (;;) {
        if (!got) return true;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (msg.message == WM_QUIT) break;
        got = PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE);
    }
    return false;
}

bool RtApp_PlatformUpdate(App* self) {
    return self->Application::PlatformUpdate();
}

// ============================================================================
// Application::Application::SleepThread(int ms)   retail 0x18009f850   ord 4069
// ?SleepThread@Application@1@QEAAXH@Z
// ----------------------------------------------------------------------------
// Busy-waits (SwitchToThread, not Sleep) until at least `ms` milliseconds of
// engine time, measured through the same QPC-derived tick conversion
// System::Timer uses elsewhere, have elapsed. DAT_18016172c (retail's
// millisecond divisor) reads as 1000.0f (bytes 00 00 7a 44), confirmed by
// direct memory read against the shipped DLL rather than assumed.
//
// `this` is never read — retail's SleepThread ignores the object entirely,
// which is why the parameter below is unused and untyped-through only for
// signature fidelity.
// ============================================================================
void Application::Application::SleepThread(int32_t ms) {

    LARGE_INTEGER t0;
    QueryPerformanceCounter(&t0);
    const int64_t startTicks = QpcToEngineTicks(t0.QuadPart);

    const double targetTicks =
        (static_cast<double>(static_cast<float>(ms) / 1000.0f)) / RtSystem::TimerSecondsPerTick();

    LARGE_INTEGER t1;
    QueryPerformanceCounter(&t1);
    int64_t elapsed = QpcToEngineTicks(t1.QuadPart) - startTicks;
    while (elapsed < static_cast<int64_t>(targetTicks)) {
        SwitchToThread();
        QueryPerformanceCounter(&t1);
        elapsed = QpcToEngineTicks(t1.QuadPart) - startTicks;
    }
}

void RtApp_SleepThread(App* self, int ms) {
    self->Application::SleepThread(ms);
}

// ============================================================================
// Application::Application::YieldToThreads()   retail 0x18009f920   ord 4568
// ?YieldToThreads@Application@1@QEAAXXZ
// ----------------------------------------------------------------------------
// `SwitchToThread(); return;` — five bytes past the prologue. Ghidra's
// decompile prints a spurious "could not recover jumptable / treating
// indirect jump as call" warning above it; the disassembly shows a plain
// direct CALL to the imported SwitchToThread thunk, nothing indirect. `this`
// is not read.
// ============================================================================
void Application::Application::YieldToThreads() {
    SwitchToThread();
}

void RtApp_YieldToThreads(App* self) {
    self->Application::YieldToThreads();
}

// ============================================================================
// Application::Application::EnableCursor(bool)   retail 0x18009f800   ord 2292
// ?EnableCursor@Application@1@QEAAX_N@Z
// ----------------------------------------------------------------------------
//   if (this->_cursorEnabled == enabled) return;         // 0x18009f803 JE
//   this->_cursorEnabled = enabled;                       // 0x18009f809
//   walk the message-callback Set (this+0x18, the SAME tree WindowProc
//   descends in platform_startup_win.cpp) for key == WM_SETCURSOR (0x20);
//   if found, TAIL-CALL it as (hwnd = this->_windowHandle, msg = 0x20,
//   wParam = 0, lParam = 0).                              // 0x18009f82b..838
//
// NOT A DIRECT WIN32 CALL. There is no ShowCursor/ClipCursor/SetCursor
// anywhere in this function's own body (confirmed both from the disassembly
// and from decompile_function — the only CALL-shaped instruction is the tail
// JMP through the found node's callback pointer). The Win32 calls the wave
// brief's note pointed at happen one hop downstream, in whatever is
// registered at WM_SETCURSOR: `dx11/src/devicedata.cpp:1105`
// (`DeviceData_OnSetCursor`) IS that registration —
// `SetCursor(dd->cursor); ShowCursor(TRUE);` — and it ignores hwnd/msg/
// wParam/lParam entirely, so the SAME two calls fire whether `enabled` is
// going true or false. That is retail's own behaviour (the dispatch never
// reads `enabled`, only the tree), not a defect this port introduces.
//
// THE TREE DESCENT is byte-identical in shape to WindowProc's — unsigned
// compare against the target key, `< key` goes to `+0x18` (left), `> key`
// goes to `+0x20` (right), equal dispatches — just keyed on the fixed
// constant WM_SETCURSOR instead of the live message id. Verified against
// `rtsetavl::Node`'s pinned layout (set_avl.h) rather than re-derived.
//
// The body is written against the live sink (rt_sink.h); every store below
// goes through it.
// ============================================================================
namespace {

constexpr unsigned kWmSetCursor = 0x20;

// Application's message-callback element: `{unsigned message; int(*)(HWND,
// unsigned, WPARAM, LPARAM) callback;}` at node+0x00/+0x08 — the same shape
// message_callbacks.cpp's `Element` and platform_startup_win.cpp's `Element`
// declare, re-declared locally here for the same reason platform_startup_win
// gives for its own copy: this file must not depend on either of theirs.
using CursorCallbackFn = int (*)(void*, unsigned, uint64_t, int64_t);
struct CursorCallbackElement {
    unsigned         message;   // +0x00
    unsigned         pad04;     // +0x04 — never read, never compared
    CursorCallbackFn callback;  // +0x08
};

// The body, written against the live sink (rt_sink.h): every store goes
// through `out` and lands for real.
void EnableCursorBody(App* self, bool enabled, RtSink& out) {
    const uint8_t prev = out.ReadU8(&self->_cursorEnabled);
    if (prev == static_cast<uint8_t>(enabled)) return;          // 0x18009f803

    out.U8(&self->_cursorEnabled, static_cast<uint8_t>(enabled),
           "Application._cursorEnabled");                        // 0x18009f809

    using rtsetavl::Node;
    Node* n = static_cast<Node*>(self->_messageCallbackRoot);
    const Node* found = nullptr;
    while (n != nullptr) {
        const unsigned key = reinterpret_cast<const CursorCallbackElement*>(n)->message;
        if (kWmSetCursor < key)      n = n->left;                // 0x18009f820
        else if (key < kWmSetCursor) n = n->right;                // 0x18009f81a
        else { found = n; break; }                                // 0x18009f826
    }
    if (found != nullptr) {
        CursorCallbackFn cb =
            reinterpret_cast<const CursorCallbackElement*>(found)->callback;
        if (out.Call("Application message callback @ WM_SETCURSOR",
                     reinterpret_cast<const void*>(cb),
                     reinterpret_cast<int64_t>(self->_windowHandle),
                     static_cast<int64_t>(kWmSetCursor), 0, 0)) {
            cb(self->_windowHandle, kWmSetCursor, 0, 0);          // 0x18009f838
        }
    }
}

}  // namespace

void Application::Application::EnableCursor(bool enabled) {
    RtSink live;
    EnableCursorBody(this, enabled, live);
}

void RtApp_EnableCursor(App* self, bool enabled) {
    self->Application::EnableCursor(enabled);
}

// ============================================================================
// Application::Application::LaunchProcess(const char*, bool)
// ?LaunchProcess@Application@1@QEAA_NPEBD_N@Z          retail 0x18009f5e0
//                                                       ord 3179
// ----------------------------------------------------------------------------
// `this` IS NEVER READ — confirmed by decompile_function, exactly like
// SleepThread/YieldToThreads above; kept as an unused parameter for
// signature fidelity.
//
// TWO ENTIRELY DIFFERENT WIN32 PATHS, selected by `wait`. Both resolved by
// mapping each `call [rip+…]` thunk to its import-table entry with pefile
// (never inferred from the argument shape):
//
//   wait == false                                      (0x18009f612..0x18009f6bd)
//     WideString file(cmd);
//     WideString verb("open");                          // literal @ 0x180154900
//     HINSTANCE r = ShellExecuteW(NULL, verb.data, file.data, NULL, NULL,
//                                 10 /* SW_SHOWDEFAULT */);
//                                     // import slot 0x18014b488 -> SHELL32!ShellExecuteW
//     <release both WideStrings>
//     return (INT_PTR)r > 32;         // ShellExecuteW's own success threshold
//
//   wait == true                                        (0x18009f6c2..0x18009f7d4)
//     STARTUPINFOW si = {}; si.cb = sizeof(si);           // 0x68, memset then
//                                                          // one store
//     PROCESS_INFORMATION pi = {};
//     size_t len = strlen(cmd) + 1;                       // NUL included
//     char buf[260];                                      // local_128[259] +
//                                                          // local_25, MAX_PATH
//     memcpy(buf, cmd, min(len, 260));
//     if (len > 260) buf[259] = 0;                         // force-terminate a
//                                                           // truncated command
//     WideString cmdLine(buf);                             // ANSI -> UTF-16,
//                                                           // from the
//                                                           // (possibly
//                                                           // truncated) COPY,
//                                                           // never from cmd
//                                                           // itself
//     BOOL ok = CreateProcessW(NULL, cmdLine.data, NULL, NULL, FALSE, 0, NULL,
//                              NULL, &si, &pi);
//                                     // import slot 0x18014b3c8 -> KERNEL32!CreateProcessW
//     <release cmdLine>
//     if (!ok) return false;
//     WaitForSingleObject(pi.hProcess, INFINITE);
//                                     // import slot 0x18014b3c0 -> KERNEL32!WaitForSingleObject
//     CloseHandle(pi.hProcess);
//                                     // import slot 0x18014b3b8 -> KERNEL32!CloseHandle
//     return true;
//
// RETAIL LEAKS `pi.hThread` ON THE WAIT PATH. Confirmed by decompile: hThread
// is stored by CreateProcessW and never closed or read again. Faithful, not
// "fixed" — the port doesn't get to repair a leak retail always had.
//
// THE 260-BYTE CLAMP is retail's own MAX_PATH-shaped stack buffer; the
// constant is read directly off `CMP EBX,R14D` / `MOV R14D,0x104` at
// 0x18009f703/0x18009f709, not assumed from the name MAX_PATH.
//
// BOTH WideString CONSTRUCTIONS (the literal "open", `cmd` itself on the
// no-wait path, and the truncated ANSI buffer on the wait path) go through
// the SAME already-ported call target, 0x1800cb110
// (`RtSystem_WideString_CtorFromChars`, Wave 9 lane PLAT) — confirmed by
// reading the same `CALL 0x1800cb110` at all three call sites
// (0x18009f617, 0x18009f62d, 0x18009f73d). Its dtor is
// `RtSystem_WideString_Dtor`, already used by platform_startup_win.cpp in
// this same directory.
//
// THE ORACLE: NONE, and the reason is structural, not awkward — running the
// DLL's own copy afterward would spawn a SECOND real child process (wait
// path) or open a SECOND real shell document (no-wait path). Neither side
// effect lands in any memory region either implementation could snapshot or
// restore.
// the diff oracle'd like PlatformUpdate/SleepThread/YieldToThreads above, for
// the identical reason.
// ============================================================================
namespace {
constexpr char kLaunchVerbOpen[] = "open";  // literal @ 0x180154900
}  // namespace

bool Application::Application::LaunchProcess(const char* cmd, bool wait) {

    bool ok;
    if (wait) {
        STARTUPINFOW si;
        std::memset(&si, 0, sizeof(si));                    // 0x18009f6cc
        si.cb = sizeof(si);                                  // 0x18009f6d1, 0x68

        PROCESS_INFORMATION pi;
        std::memset(&pi, 0, sizeof(pi));

        size_t len = std::strlen(cmd) + 1;                   // 0x18009f6e9..6ff
        const size_t copyLen = len > 0x104 ? 0x104 : len;     // 0x18009f709 CMOVG

        char buf[0x104];
        std::memcpy(buf, cmd, copyLen);                       // 0x18009f71a
        if (len > 0x104) buf[0x103] = 0;                       // 0x18009f72c, truncated

        WideString cmdLine;
        RtSystem::RtSystem_WideString_CtorFromChars(&cmdLine, buf);  // 0x18009f73d

        const BOOL created = CreateProcessW(
            nullptr, cmdLine.data, nullptr, nullptr, FALSE, 0,
            nullptr, nullptr, &si, &pi);                      // 0x18009f773

        RtSystem::RtSystem_WideString_Dtor(&cmdLine);          // 0x18009f78a..7ac

        if (!created) {
            ok = false;
        } else {
            WaitForSingleObject(pi.hProcess, INFINITE);        // 0x18009f7c3
            CloseHandle(pi.hProcess);                          // 0x18009f7ce
            ok = true;                                         // pi.hThread leaked, faithfully
        }
    } else {
        WideString file;
        RtSystem::RtSystem_WideString_CtorFromChars(&file, cmd);          // 0x18009f617
        WideString verb;
        RtSystem::RtSystem_WideString_CtorFromChars(&verb, kLaunchVerbOpen); // 0x18009f62d

        HINSTANCE result = ShellExecuteW(nullptr, verb.data, file.data,
                                         nullptr, nullptr, 10);            // 0x18009f64d

        RtSystem::RtSystem_WideString_Dtor(&verb);              // 0x18009f78a..7ac (shared epilogue)
        RtSystem::RtSystem_WideString_Dtor(&file);               // 0x18009f694..6b2

        ok = reinterpret_cast<INT_PTR>(result) > 32;             // 0x18009f6b7..6ba
    }

    return ok;
}

bool RtApp_LaunchProcess(App* self, const char* cmd, bool wait) {
    return self->Application::LaunchProcess(cmd, wait);
}

// ==========================================================================
// Application::Application::PlatformConstruct(void* platformHandle)
// ?PlatformConstruct@Application@1@AEAAXPEAX@Z  retail 0x18009fbe0   ord 3331
// ============================================================================
// WAVE 12 LANE B. Four instructions, and the only body in this file that makes
// no Win32 call at all — it is here because it is the `Platform*` seam by
// retail's own architecture (a `_posix` twin would have a different one), not
// because it needs <windows.h>.
//
//   18009fbe0: MOV  QWORD PTR [RCX],0x0        ; _windowHandle   = null
//   18009fbe7: MOV  QWORD PTR [RCX+0x8],RDX    ; _platformHandle = arg
//   18009fbeb: MOV  WORD  PTR [RCX+0x10],0x100 ; ONE 16-bit store: +0x10 = 0x00,
//                                              ;                   +0x11 = 0x01
//   18009fbf1: RET
//
// IT IS THE CONSTRUCTOR'S TAIL, LIFTED. `??0Application@0@QEAA@PEAX@Z`
// (0x18009f590, ported in runtime/src/engine/singleton_ctors.cpp) ends with
// exactly these three stores in exactly this order — MSVC inlined the call.
// That is why `calls=` for this site is zero on every run today: nothing in our
// tree calls it, because retail's own only caller inlined it. The probe in
// application_lifecycle_probe.cpp is what executes it, and unlike the other
// four rows of this lane it gets a FULL BYTE-DIFF oracle, because the body is
// three stores and no side effects.
//
// The 16-bit store is reproduced as a 16-bit store rather than as two byte
// assignments. Single-threaded the two are indistinguishable, but this object
// is read from the window procedure, which USER32 can re-enter, and "the two
// bytes become visible together" is a property of the instruction retail chose.
// `std::memcpy` rather than a cast-and-assign: `_cursorEnabled` and `_running` are
// two distinct `uint8_t` members and writing one `uint16_t` across them through
// a reinterpret_cast is undefined behaviour GCC is entitled to reorder.
// ============================================================================
void Application::Application::PlatformConstruct(void* platformHandle) {


    _windowHandle   = nullptr;                 // 0x18009fbe0
    _platformHandle = platformHandle;          // 0x18009fbe7
    const uint16_t stateWord = 0x0100;               // 0x18009fbeb
    std::memcpy(&_cursorEnabled, &stateWord, sizeof(stateWord));
}

extern "C" void RtApp_PlatformConstruct(App* self, void* platformHandle) {
    self->Application::PlatformConstruct(platformHandle);
}
