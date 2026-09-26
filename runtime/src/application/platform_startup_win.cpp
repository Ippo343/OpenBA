// ============================================================================
// runtime/src/application/platform_startup_win.cpp — Wave 9 lane PLAT
//   The `PlatformStartup` family, and the window procedure that only exists
//   because one of them installs it.
// ============================================================================
//
// THE WALL
// --------
// The DLL-less cutover boot dies at
//
//     [RtUnresolved] ?PlatformStartup@Application@1@AEAA_NXZ
//     [RtResolve] MISS "?PlatformStartup@Application@1@AEAA_NXZ" -> NULL
//     Fault: EXEC of 0000000000000000     (engine_lifecycle.cpp:638)
//
// `AEAA` is PRIVATE, not unexported — this DLL exports its private members and
// this one is ordinal 3402 at 0x18009fc10. Confirmed against the shipped export
// directory with pefile BEFORE anything was written here, along with every
// other mangled name this file registers
// (reference_resolveexport_phantom_names.md).
//
// THE FAMILY, AND WHAT THIS LANE DID WITH EACH ROW
// ------------------------------------------------
// Nine exports share the name. Their status was measured, not assumed — every
// address and every mangled name below was grepped across code/, engine/,
// runtime/ and standalone/ before a line was written, because a body that is
// PORTED BUT NEVER REGISTERED reads exactly like an absent one.
//
// | Ord  | Export                                            | Retail      | This lane |
// |------|---------------------------------------------------|-------------|-----------|
// | 3401 | ?PlatformStartup@AchievementManager@Achievement@@  | 0x18009eb70 | PORTED here (member since the B.3.7 fragment) |
// | 3402 | ?PlatformStartup@Application@1@                    | 0x18009fc10 | PORTED here — THE WALL |
// | 3403 | ?PlatformStartup@Device@Audio@@                    | 0x1800de600 | DECLINED, see WAVE9_LANE_PLAT.md |
// | 3404 | ?PlatformStartup@Device@Input@@                    | 0x1800e1880 | DECLINED, see WAVE9_LANE_PLAT.md |
// | 3405 | ?PlatformStartup@Device@Video@@                    | 0x1800f8260 | already ported (lane VID2, device_startup.cpp:844) |
// | 3406 | ?PlatformStartup@Engine@1@                         | 0x1800a2100 | PORTED here |
// | 3407 | ?PlatformStartup@ExternalFactory@Object@@          | 0x1800bf6c0 | PORTED here |
// | 3408 | ?PlatformStartup@MemoryAllocator@System@@          | 0x1800c7c40 | already ported (allocator_win.cpp:129) |
// | 3409 | ?PlatformStartup@Timer@System@@                    | 0x1800ca860 | already ported (timer.cpp:318) |
//
// Only four of the nine are on the cutover boot path, and the path is our own
// `Engine::Startup` port (runtime/src/engine/engine_lifecycle.cpp): Application
// at `g_startupFailAt == 6`, Video at 11, Audio at 12, Input at 13. Achievement,
// ExternalFactory and Engine are NOT called by anything — Ghidra's xrefs for all
// three are the export address table and nothing else, and
// Application-x64.exe's import table names none of them (checked with pefile).
// They are registered anyway because each is two or seven instructions; they are
// reported honestly as UNREACHABLE, never as verified.
//
// THE CALLING CONVENTIONS ARE READ OFF THE MANGLED NAMES, NOT GUESSED
// -------------------------------------------------------------------
//   AEAA  private instance  -> `this` in RCX          (Application, Achievement,
//                                                      ExternalFactory, Audio, Input)
//   QEAA  public  instance  -> `this` in RCX          (Engine)
//   CA    static            -> NO `this`              (Timer, and WindowProc)
//
// WIN32 AT CUTOVER
// ----------------
// Every OS call below is made DIRECTLY by name against the import libraries
// this exe links (user32, ole32). None of them goes through Runtime-x64.dll's
// own IAT, which is the trap lane ADDR found in `MainLoop::Run` — the DLL's
// import thunks do not exist when the DLL is absent. The slots retail used
// were mapped with pefile so the exact USER32/ole32 entry points are named
// rather than inferred:
//   0x18014b4a0 DefWindowProcW   0x18014b4a8 UpdateWindow   0x18014b4b0 SetCursor
//   0x18014b4b8 PostQuitMessage  0x18014b4c0 CreateWindowExW
//   0x18014b4c8 RegisterClassW   0x18014b618 CoInitializeEx
// ============================================================================

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>

#include "../../funcs/rt_port.h"
#include "../../types/application/Application.h"
#include "../../types/engine/Engine.h"   // ENDGAME B.3.7: Engine::Engine::PlatformStartup
#include "../../types/achievement/AchievementManager.h"   // ENDGAME B.3.7 fragment: AchievementManager::PlatformStartup
#include "../collection/set_avl.h"
#include "../system/widestring.h"

// ENDGAME B.3.7: `Application::Application` is the real class; a global
// `using RtApplication::Application;` is ill-formed next to the namespace.
using App = Application::Application;
using RtSystem::WideString;
using rtsetavl::Node;

// Declared locally rather than by including standalone/resolve.h (PLAN §4.1
// decision 1). Byte-identical to runtime/src/collection/array.cpp:133. Used by
// the oracle ONLY, to read retail's own hbrBackground constant out of .rdata.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// The message callback retail stores in the Set: the mangled name spells it
// `P6AHPEAUHWND__@@I_K_J@Z`. Identical to message_callbacks.cpp's, which is the
// file that PUTS them in the tree; this is the file that CALLS them.
using MessageCallbackFn = int (*)(void*, unsigned, uint64_t, int64_t);

// The 16-byte element. Only the key at +0x00 and the callback at +0x08 are ever
// read here — the four padding bytes at +0x04 carry a dword of the REGISTERING
// caller's stack frame (message_callbacks.cpp documents why), so nothing may
// compare them.
struct Element {
    unsigned          message;   // +0x00
    unsigned          pad04;     // +0x04 — never read, never compared
    MessageCallbackFn callback;  // +0x08
};
static_assert(sizeof(Element) == 0x10, "Set element is 0x10 bytes");

// `?_singleton@Application@1@0PEAV11@EA` @ 0x1801fc678. The port-aware door, so
// a minted run and a DLL run both land on whatever storage the ported ctor
// (singleton_ctors.cpp) published into.
App** AppSingletonSlot() {
    return RT_DATA_SLOT(App**, "?_singleton@Application@1@0PEAV11@EA");
}

// `?SetText@Error@System@@SAXPEBD_N@Z` @ 0x1800c5fa0 — ported in Wave 2
// (runtime/src/system/error.cpp:286), reached through the port-aware door.
void ErrorSetText(const char* text, bool flag) {
    using Fn = void (*)(const char*, bool);
    Fn fn = RT_EXPORT_ONCE(Fn, "?SetText@Error@System@@SAXPEBD_N@Z");
    if (fn) fn(text, flag);
}

// The two literals retail passes, read out of the shipped .rdata rather than
// retyped: 0x180154870 and 0x1801548c0.
const char kErrRegister[] = "Application: The application failed to register a window class properly.";
const char kErrCreate[]   = "Application: The application failed to create window properly.";
// 0x180154858. The window CLASS name and the window TITLE are the same string
// object — retail passes RBX for both CreateWindowExW arguments.
const char kClassName[]   = "Banished";

}  // namespace

// ============================================================================
// Application::Application::WindowProc(HWND, UINT, WPARAM, LPARAM)
// ?WindowProc@Application@1@CA_JPEAUHWND__@@I_K_J@Z    retail 0x18009f930
//                                                      ord 4546  (CA = static)
// ============================================================================
// This is not an optional extra. `PlatformStartup` stores 0x18009f930 into
// `WNDCLASSW::lpfnWndProc`, and at cutover that address is not mapped — a port
// of PlatformStartup that did not also supply the procedure would register a
// class whose window procedure is a null dereference on the first message.
//
// Retail's shape, transcribed:
//
//   1. read `Application::_singleton`; if it is null, or its callback Set is
//      empty, skip straight to the switch;
//   2. otherwise descend the AVL tree by UNSIGNED comparison of the message id
//      against the key at node+0x00, following +0x18 (less) and +0x20
//      (greater) — the same descent RegisterMessageCallback uses;
//   3. on a hit, CALL the stored callback with the four arguments UNCHANGED and
//      return 0 if it returns non-zero (the callback claimed the message);
//   4. the switch: WM_DESTROY posts quit; WM_SETFOCUS/WM_KILLFOCUS write the
//      one byte at +0x11 and then STILL fall through to DefWindowProcW;
//      WM_ERASEBKGND is swallowed; WM_SYSKEYDOWN is swallowed only for VK_F10;
//      everything else is DefWindowProcW.
//
// TWO THINGS READ FROM THE DISASSEMBLY, NOT FROM THE DECOMPILER
// -------------------------------------------------------------
//   * The equal-key branch at 0x18009f98e RE-TESTS `RAX` for null even though
//     the loop just tested it. Kept — it is retail's, it is free, and removing
//     it is the kind of tidying that makes a transcription stop being one.
//   * `CMP RSI,0x79` at 0x18009f9b9 is a SIXTY-FOUR-BIT compare of the whole
//     WPARAM against VK_F10, not a compare of its low dword.
//
// ============================================================================
int64_t Application::Application::WindowProc(void* hwndArg, uint32_t msg,
                                             uint64_t wParamArg, int64_t lParamArg) {
    HWND   hwnd   = static_cast<HWND>(hwndArg);
    WPARAM wParam = static_cast<WPARAM>(wParamArg);
    LPARAM lParam = static_cast<LPARAM>(lParamArg);
    App** slot = AppSingletonSlot();
    App*  app  = slot ? *slot : nullptr;

    if (app != nullptr) {                                   // 0x18009f95c
        Node* n = static_cast<Node*>(app->_messageCallbackRoot);
        if (n != nullptr) {                                 // 0x18009f965
            Node* found = nullptr;
            while (n != nullptr) {
                const unsigned key = reinterpret_cast<const Element*>(n)->message;
                if (msg < key) {                            // 0x18009f976 JC
                    n = n->left;
                } else if (key < msg) {                     // 0x18009f97b JNC
                    n = n->right;
                } else {
                    found = n;
                    break;
                }
            }
            if (found != nullptr) {                         // 0x18009f98e, retail re-tests
                MessageCallbackFn cb =
                    reinterpret_cast<const Element*>(found)->callback;
                if (cb(hwnd, msg, static_cast<uint64_t>(wParam),
                       static_cast<int64_t>(lParam)) != 0) {
                    return 0;                               // 0x18009f99b -> 0x18009f9f0
                }
            }
        }
    }

    switch (msg) {
        case WM_DESTROY:                                    // 2, 0x18009f9e8
            PostQuitMessage(0);
            return 0;
        case WM_SETFOCUS:                                   // 7, 0x18009f9cc
            if (app) app->_running = 1;
            break;                                          // falls to DefWindowProcW
        case WM_KILLFOCUS:                                  // 8, 0x18009f9c1
            if (app) app->_running = 0;
            break;                                          // falls to DefWindowProcW
        case WM_ERASEBKGND:                                 // 0x14, 0x18009f9b0
            return 0;
        case WM_SYSKEYDOWN:                                 // 0x104, 0x18009f9b9
            if (wParam == 0x79) return 0;                   // VK_F10, 64-bit compare
            break;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);       // 0x18009f9e0
}

extern "C" LRESULT CALLBACK RtApp_WindowProc(HWND hwnd, UINT msg,
                                             WPARAM wParam, LPARAM lParam) {
    return static_cast<LRESULT>(Application::Application::WindowProc(
        hwnd, msg, static_cast<uint64_t>(wParam), static_cast<int64_t>(lParam)));
}

// ============================================================================
// Application::Application::PlatformStartup()
// ?PlatformStartup@Application@1@AEAA_NXZ              retail 0x18009fc10
//                                                      ord 3402  (AEAA, private)
// ============================================================================
//   WideString className("Banished");                  // 0x1800cb110
//   WNDCLASSW wc = { 0, WindowProc, 0, 0, this->_platformHandle,
//                    NULL, NULL, (HBRUSH)6, NULL, className };
//   if (!RegisterClassW(&wc)) { Error::SetText(<register msg>, false); ok = false; }
//   else {
//       this->_windowHandle = CreateWindowExW(0, className, className, 0,
//                                             CW_USEDEFAULT, CW_USEDEFAULT, 10, 10,
//                                             NULL, NULL, this->_platformHandle, NULL);
//       if (!this->_windowHandle) { Error::SetText(<create msg>, false); ok = false; }
//       else { UpdateWindow(hwnd); SetCursor(wc.hCursor); ok = true; }
//   }
//   <inline WideString release>
//   return ok;
//
// FIVE DETAILS THAT ARE NOT OBVIOUS, ALL OF THEM RETAIL'S
// -------------------------------------------------------
//  1. It is a WNDCLASSW and RegisterClassW, NOT the Ex pair. The struct at
//     RSP+0x70 has no cbSize and no hIconSm, `TEST AX,AX` at 0x18009fc9d tests
//     a 16-bit ATOM, and import slot 0x18014b4c8 is literally
//     USER32!RegisterClassW.
//  2. `hbrBackground` is the raw constant 6, loaded as the 16-byte blob at
//     0x180161780 (`06 00 00 00` then twelve zero bytes, so `lpszMenuName` is
//     null) — i.e. `(HBRUSH)(COLOR_WINDOW + 1)`, a SYSTEM COLOR INDEX and not a
//     brush handle. Passed through as the integer it is.
//  3. `dwStyle` is ZERO and the window is 10x10 at CW_USEDEFAULT. This is a
//     placeholder; `Video::Device::PlatformStartup` is what gives the window
//     its real style and size later in `Engine::Startup`.
//  4. The FINAL `SetCursor` argument is re-read out of the WNDCLASSW's hCursor
//     field at RSP+0x98 (0x18009fd18), which the XORPS zeroed — so this is
//     `SetCursor(NULL)`, hiding the OS arrow over the game's own cursor. It is
//     written as a read-back of `wc.hCursor` rather than as a literal NULL,
//     because that is what the instruction does.
//  5. On the RegisterClassW failure path `this->_windowHandle` is NOT written.
//     Only the CreateWindowExW path stores (0x18009fcf4), and it stores the
//     result BEFORE testing it — so a failed creation leaves an explicit null
//     in the field rather than the ctor's zero.
//
// THE WIDESTRING RELEASE IS INLINE IN RETAIL (0x18009fd29..0x18009fd51) and is
// byte-for-byte `??1WideString@System@@QEAA@XZ`, asymmetry included. Calling our
// own ported dtor directly, rather than through the seam, is the faithful
// spelling: retail makes no call there at all.
//
// ============================================================================
bool Application::Application::PlatformStartup() {

    WideString className;
    RtSystem::RtSystem_WideString_CtorFromChars(&className, kClassName);  // 0x18009fc39

    WNDCLASSW wc;
    wc.style         = 0;                                          // 0x18009fc41
    wc.lpfnWndProc   = &RtApp_WindowProc;                          // 0x18009fc45
    wc.cbClsExtra    = 0;                                          // 0x18009fc51 (one qword
    wc.cbWndExtra    = 0;                                          //   store covering both)
    wc.hInstance     = static_cast<HINSTANCE>(_platformHandle);  // 0x18009fc59
    wc.hIcon         = nullptr;                                    // 0x18009fc68 XORPS/MOVDQA
    wc.hCursor       = nullptr;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<uintptr_t>(6));  // 0x180161780
    wc.lpszMenuName  = nullptr;
    wc.lpszClassName = className.data;                             // 0x18009fc8a

    bool ok;
    if (RegisterClassW(&wc) == 0) {                                 // 0x18009fc97
        ErrorSetText(kErrRegister, false);                          // 0x18009fd05
        ok = false;                                                 // 0x18009fd0a
    } else {
        HWND hwnd = CreateWindowExW(
            0,                              // dwExStyle      ECX  = 0
            className.data,                 // lpClassName    RDX  = RBX
            className.data,                 // lpWindowName   R8   = RBX
            0,                              // dwStyle        R9D  = 0
            static_cast<int>(0x80000000),   // X   [RSP+0x20] CW_USEDEFAULT
            static_cast<int>(0x80000000),   // Y   [RSP+0x28] CW_USEDEFAULT
            10,                             // nWidth  [RSP+0x30]
            10,                             // nHeight [RSP+0x38]
            nullptr,                        // hWndParent  [RSP+0x40]
            nullptr,                        // hMenu       [RSP+0x48]
            static_cast<HINSTANCE>(_platformHandle),  // [RSP+0x50], re-read
            nullptr);                       // lpParam     [RSP+0x58]
        _windowHandle = hwnd;                                 // 0x18009fcf4
        if (hwnd == nullptr) {                                      // 0x18009fcf7
            ErrorSetText(kErrCreate, false);
            ok = false;
        } else {
            UpdateWindow(hwnd);                                     // 0x18009fd12
            SetCursor(wc.hCursor);                                  // 0x18009fd20, reads back +0x98
            ok = true;                                              // 0x18009fd26
        }
    }

    // 0x18009fd29..0x18009fd51 — the inlined WideString release.
    RtSystem::RtSystem_WideString_Dtor(&className);

    return ok;
}

extern "C" bool RtApp_PlatformStartup(App* self) {
    return self->Application::PlatformStartup();
}

// ============================================================================
// Application::Application::PlatformShutdown()
// ?PlatformShutdown@Application@1@AEAAXXZ      retail 0x18009fd70   ord 3393
//                                              (AEAA == PRIVATE, and the export
//                                              directory lists it — private is
//                                              not unexported in this DLL)
// ============================================================================
// WAVE 12 LANE B. It lives HERE rather than in application_win.cpp for one
// reason: it is the exact inverse of the function above and it consumes the
// SAME `kClassName` literal. Two copies of "Banished" in two translation units
// is a divergence waiting to happen, and nothing would catch it — the class
// name is what ties RegisterClassW to UnregisterClassW, and a mismatch simply
// leaves the class registered forever with no error either side would report.
//
//   if (this->_windowHandle) DestroyWindow(this->_windowHandle);   // 0x18009fd8a
//   WideString className("Banished");                              // 0x18009fd9c
//   UnregisterClassW(className, this->_platformHandle);            // 0x18009fda9
//   <inline WideString release>                                    // 0x18009fdb0
//
// The two import slots were read out of the shipped import directory with
// pefile rather than named from the call shape: 0x18014b5e0 is
// USER32!DestroyWindow and 0x18014b5d8 is USER32!UnregisterClassW.
//
// FOUR DETAILS THAT ARE RETAIL'S, NOT TIDYING:
//  1. **`this->_windowHandle` is NOT nulled afterwards.** DestroyWindow is
//     called and the field keeps the now-dead HWND. `Engine::Shutdown`
//     (engine_lifecycle.cpp:901) calls this and then destroys and frees the
//     Application a few lines later, so nothing reads it again — but a port
//     that "helpfully" nulled it would differ from retail on a path no oracle
//     covers.
//  2. **Neither Win32 return value is tested.** A failed DestroyWindow and a
//     failed UnregisterClassW are both ignored.
//  3. **The class name is rebuilt from the char literal every call**, through
//     `??0WideString@System@@QEAA@PEBD@Z` (0x1800cb110), i.e. it allocates and
//     releases a UTF-16 buffer purely to pass a string USER32 will only read.
//     Reproduced; it is one allocation on the shutdown path.
//  4. **The release is INLINE in retail** (0x18009fdb0..0x18009fde1) and is
//     byte-for-byte `??1WideString@System@@QEAA@XZ` (0x18000a730), asymmetry
//     included — the refcount-still-positive branch nulls the local, the
//     refcount-hit-zero branch stores `data - 8` into the local and frees it.
//     Calling our own ported dtor is the faithful spelling, exactly as
//     PlatformStartup does above.
//
// ORACLE: NONE IS POSSIBLE AT THE CALL SITE, and the reason is the same
// structural one PlatformStartup records, one step worse. Both effects are
// one-shot and destructive: a replay's DestroyWindow gets an invalid handle and
// its UnregisterClassW finds no class, so the second run measures the failure
// of the first run's success. The comparison would be green and meaningless.
//
// What it IS verified by: it has a LIVE ROUTE. `Engine::Shutdown` calls it by
// name through RT_EXPORT_ONCE, so `calls=` is non-zero on any clean exit, and
// the two Win32 results are logged under the diff harness so a run can show
// that the window really was destroyed and the class really was unregistered.
// That is an EXECUTION record, not a comparison, and is labelled as one.
// ============================================================================
void Application::Application::PlatformShutdown() {

    BOOL destroyed = FALSE;
    DWORD destroyErr = 0;
    if (_windowHandle) {                                   // 0x18009fd82/85
        destroyed = DestroyWindow(static_cast<HWND>(_windowHandle));
        if (!destroyed) destroyErr = GetLastError();             // 0x18009fd8a
    }

    WideString className;
    RtSystem::RtSystem_WideString_CtorFromChars(&className, kClassName);  // 0x18009fd9c

    const BOOL unregistered =                                    // 0x18009fda9
        UnregisterClassW(className.data,
                         static_cast<HINSTANCE>(_platformHandle));
    const DWORD unregisterErr = unregistered ? 0 : GetLastError();

    RtSystem::RtSystem_WideString_Dtor(&className);              // 0x18009fdb0..fde1

}

extern "C" void RtApp_PlatformShutdown(App* self) {
    self->Application::PlatformShutdown();
}

// ============================================================================
// The three that are two instructions, and the one that is seven
// ============================================================================
// All four are quoted in full because they are short enough that a summary
// would be longer than the transcription.

// ?PlatformStartup@AchievementManager@Achievement@@AEAA_NXZ   0x18009eb70 ord 3401
//     18009eb70: MOV AL,0x1
//     18009eb72: RET
// `this` is never touched. There is no achievement backend to bring up in this
// build; the class's platform hook is a stub in the shipped DLL.
// ENDGAME B.3.7 fragment: the body is the member's; the registered door
// delegates with a QUALIFIED call (the class is non-polymorphic, so the
// qualifier is only the house shape here).
bool Achievement::AchievementManager::PlatformStartup() {
    return true;
}

extern "C" bool RtAchievement_AchievementManager_PlatformStartup(void* self) {
    return reinterpret_cast<Achievement::AchievementManager*>(self)->Achievement::AchievementManager::PlatformStartup();  // B.3.7 bridge
}

// ?PlatformStartup@ExternalFactory@Object@@AEAA_NXZ           0x1800bf6c0 ord 3407
//     1800bf6c0: MOV AL,0x1
//     1800bf6c2: RET
// Consistent with `Engine::Startup`, which constructs an ExternalFactory and
// then calls NO Startup on it at all (engine_lifecycle.cpp, the 0x1800dbb47
// block).
extern "C" bool RtObject_ExternalFactory_PlatformStartup(void* /*self*/) {
    return true;
}

// ?PlatformStartup@Engine@1@QEAA_NXZ                          0x1800a2100 ord 3406
//     1800a2100: SUB RSP,0x28
//     1800a2104: XOR EDX,EDX                 ; pvReserved = NULL
//     1800a2106: XOR ECX,ECX                 ; dwCoInit   = COINIT_MULTITHREADED (0)
//     1800a2108: CALL [0x18014b618]          ; ole32!CoInitializeEx
//     1800a210e: MOV AL,0x1                  ; the HRESULT is DISCARDED
//     ...
// NOTE THE ARGUMENT ORDER. RCX is the FIRST argument (`pvReserved`) and RDX the
// second (`dwCoInit`); both are zero, so the two XORs are indistinguishable at
// the call site and the naming above is by position, not by value.
// COINIT_MULTITHREADED is 0, so this is `CoInitializeEx(NULL, COINIT_MULTITHREADED)`.
//
// `Engine::Startup` does NOT call this — it INLINES the same two-argument
// CoInitializeEx at 0x1800dbc12, which our port already reproduces as
// `RtEngineLifecyclePlatformCoInit()` (engine_lifecycle_win.cpp:29). This body
// is therefore the exported spelling of a thing the tree already does; it is
// registered so the NAME resolves at cutover, and it is reported as
// UNREACHABLE rather than as verified.
bool Engine::Engine::PlatformStartup() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    return true;
}

extern "C" bool RtEngine_Engine_PlatformStartup(void* self) {
    return reinterpret_cast<Engine::Engine*>(self)->Engine::Engine::PlatformStartup();  // B.3.7 bridge
}

// ============================================================================
// Registration
// ============================================================================
int RtApplicationPlatformStartupRegister() {
    int n = 0;

    // Every name below was verified against Runtime-x64.dll's export directory
    // with pefile before use; the ordinals are quoted so a future reader can
    // re-check without re-deriving. Literals, never named constants
    // (reference_census_cannot_see_nonliteral_registrations.md).
    RegisterRuntimeExport("?PlatformStartup@Application@1@AEAA_NXZ",           // 3402
                          (void*)&RtApp_PlatformStartup); ++n;
    RegisterRuntimeAddr(0x18009fc10, (void*)&RtApp_PlatformStartup);

    RegisterRuntimeExport("?WindowProc@Application@1@CA_JPEAUHWND__@@I_K_J@Z", // 4546
                          (void*)&RtApp_WindowProc); ++n;
    RegisterRuntimeAddr(0x18009f930, (void*)&RtApp_WindowProc);

    RegisterRuntimeExport("?PlatformStartup@AchievementManager@Achievement@@AEAA_NXZ", // 3401
                          (void*)&RtAchievement_AchievementManager_PlatformStartup); ++n;
    RegisterRuntimeAddr(0x18009eb70, (void*)&RtAchievement_AchievementManager_PlatformStartup);

    RegisterRuntimeExport("?PlatformStartup@ExternalFactory@Object@@AEAA_NXZ", // 3407
                          (void*)&RtObject_ExternalFactory_PlatformStartup); ++n;
    RegisterRuntimeAddr(0x1800bf6c0, (void*)&RtObject_ExternalFactory_PlatformStartup);

    RegisterRuntimeExport("?PlatformStartup@Engine@1@QEAA_NXZ",               // 3406
                          (void*)&RtEngine_Engine_PlatformStartup); ++n;
    RegisterRuntimeAddr(0x1800a2100, (void*)&RtEngine_Engine_PlatformStartup);

    // Wave 9 lane PLAT also took the NEXT boot wall after Video's, which lives
    // in another namespace: `?PlatformStartup@Device@Audio@@AEAA_NXZ`
    // (engine_lifecycle.cpp:730, g_startupFailAt == 12). Registered from here
    // so the whole family lands behind ONE switch and one identity line.
    {
        extern int RtAudioPlatformStartupRegister();
        n += RtAudioPlatformStartupRegister();
    }

    // ...and the wall after Audio's: `?PlatformStartup@Device@Input@@AEAA_NXZ`
    // (engine_lifecycle.cpp:741, g_startupFailAt == 13), plus the three
    // exported message callbacks it installs, which are equally unreachable
    // at cutover if only the outer function is ported.
    {
        extern int RtInputPlatformStartupRegister();
        n += RtInputPlatformStartupRegister();
    }

    // THE BUILD IDENTITY LINE. Every measurement this lane reports is gated on
    // finding this exact string in that run's OWN log — not on "I rebuilt",
    // not on a file timestamp. Bump the revision when the bodies change.
    printf("[LanePLAT] rev=PLAT-r5 registered=%d\n", n);

    return n;
}
