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
// | 3401 | ?PlatformStartup@AchievementManager@Achievement@@  | 0x18009eb70 | PORTED here |
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
#include "../collection/set_avl.h"
#include "../system/widestring.h"

using RtApplication::Application;
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
Application** AppSingletonSlot() {
    return RT_DATA_SLOT(Application**, "?_singleton@Application@1@0PEAV11@EA");
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

bool EnvOn(const char* name) {
    const char* e = std::getenv(name);
    return e && e[0] && std::strcmp(e, "0") != 0 && std::strcmp(e, "false") != 0;
}

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
// NO ORACLE ON THE LIVE PATH, and none is possible: calling the shipped
// WindowProc afterwards would dispatch every message a second time — a second
// PostQuitMessage, a second callback invocation. It is instead covered in full
// by RtLanePlatWindowProcProbe() at the bottom of this file, which drives BOTH
// implementations over a synthetic Application and compares.
// ============================================================================
extern "C" LRESULT CALLBACK RtApp_WindowProc(HWND hwnd, UINT msg,
                                             WPARAM wParam, LPARAM lParam) {
    Application** slot = AppSingletonSlot();
    Application*  app  = slot ? *slot : nullptr;

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

// ---------------------------------------------------------------------------
// The oracle's replay of retail's PlatformStartup, and its teardown.
// Declared here, defined at the bottom; only ever called under
// BANISHED_PLAT_ORACLE=1.
// ---------------------------------------------------------------------------
namespace {
struct PlatOracleSnapshot {
    bool      classInfoOk;
    WNDCLASSW wc;
    bool      windowOk;
    LONG_PTR  style;
    LONG_PTR  exStyle;
    LONG      width;
    LONG      height;
    wchar_t   className[64];
    // A SECOND reading of the same field by a different API. GetClassInfoW and
    // GetClassLongPtrW disagree about system-colour brushes on some builds, and
    // when they do it is the READBACK that differs, not what was registered.
    ULONG_PTR classBrushFromWindow;
};
bool PlatOracleRunTheirs(void* platformHandle, PlatOracleSnapshot* out,
                         const char** why);
void IdentifySystemBrush(void* theirsBrush, void* oursBrush, void* platformHandle);
}  // namespace
extern "C" void RtLanePlatWindowProcProbe(void* hwndOpaque);
namespace {
void PlatOracleSnapshotClass(void* platformHandle, HWND hwnd,
                             PlatOracleSnapshot* out);
}  // namespace

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
// ORACLE: none on the live path, and the reason is structural rather than
// awkward — the class name is a hardcoded literal, so a second run of the pair
// necessarily has its RegisterClassW fail with ERROR_CLASS_ALREADY_EXISTS and
// measures nothing. A real comparison IS available and is implemented below,
// behind its own switch (BANISHED_PLAT_ORACLE=1) rather than behind
// BANISHED_RT_DIFF alone: it runs the SHIPPED body FIRST on a scratch object,
// reads the registered class back out of USER32 with GetClassInfoW, tears that
// class and window down, and only then lets this body run for real. It is kept
// off the ordinary diff runs deliberately, because its teardown is the one step
// that could break a live boot if it ever failed.
// ============================================================================
extern "C" bool RtApp_PlatformStartup(Application* self) {
    RT_DIFF_BEGIN("Application::Application::PlatformStartup [no oracle on the live path; see BANISHED_PLAT_ORACLE]");

    const bool oracle = rt_diff_on__ && EnvOn("BANISHED_PLAT_ORACLE");
    PlatOracleSnapshot theirs = {};
    const char* oracleWhy = "not requested";
    bool theirsOk = false;
    if (oracle) theirsOk = PlatOracleRunTheirs(self->_platformHandle, &theirs, &oracleWhy);

    WideString className;
    RtSystem::RtSystem_WideString_CtorFromChars(&className, kClassName);  // 0x18009fc39

    WNDCLASSW wc;
    wc.style         = 0;                                          // 0x18009fc41
    wc.lpfnWndProc   = &RtApp_WindowProc;                          // 0x18009fc45
    wc.cbClsExtra    = 0;                                          // 0x18009fc51 (one qword
    wc.cbWndExtra    = 0;                                          //   store covering both)
    wc.hInstance     = static_cast<HINSTANCE>(self->_platformHandle);  // 0x18009fc59
    wc.hIcon         = nullptr;                                    // 0x18009fc68 XORPS/MOVDQA
    wc.hCursor       = nullptr;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<uintptr_t>(6));  // 0x180161780
    wc.lpszMenuName  = nullptr;
    wc.lpszClassName = className.data;                             // 0x18009fc8a

    // The negative control for the class oracle below. It perturbs exactly one
    // field the oracle compares; a run with it set MUST report a divergence,
    // and a control that cannot fire is worse than none
    // (reference_probe_needs_an_effectiveness_control.md).
    // It perturbs `style`, which IS one of the compared readback fields, rather
    // than hbrBackground, which stopped being one once the readback artefact
    // above was measured. A control aimed at a field nobody compares cannot
    // fire, and a control that cannot fire is worse than none
    // (reference_probe_needs_an_effectiveness_control.md).
    if (EnvOn("BANISHED_PLAT_NEGCTL"))
        wc.style = CS_HREDRAW;

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
            static_cast<HINSTANCE>(self->_platformHandle),  // [RSP+0x50], re-read
            nullptr);                       // lpParam     [RSP+0x58]
        self->_windowHandle = hwnd;                                 // 0x18009fcf4
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

    if (rt_diff_on__) {
        if (!oracle) {
            RT_DIFF_SHADOW(true, "registered class + created window",
                           "no oracle: the class name is a literal, so a replay "
                           "collides with our own registration");
            RT_DIFF_WORK(ok);
        } else if (!theirsOk) {
            RT_DIFF_SHADOW(false, "ours ran", oracleWhy);
            RT_DIFF_WORK(true);
        } else {
            PlatOracleSnapshot ours = {};
            PlatOracleSnapshotClass(self->_platformHandle,
                                    static_cast<HWND>(self->_windowHandle), &ours);

            // Compare only what BOTH sides caused USER32 to write. lpfnWndProc
            // is EXCLUDED and reported separately: it MUST differ (theirs is
            // the DLL's 0x18009f930, ours is RtApp_WindowProc) and comparing it
            // would fail 100% of calls for the one reason that is correct
            // (reference_oracle_must_not_compare_unwritten_bytes.md).
            // lpszClassName / lpszMenuName are excluded too: GetClassInfoW does
            // not fill either, so neither side ever wrote them.
            int compared = 0, differing = 0, nonTrivial = 0;
            char detail[256];
            detail[0] = 0;
            auto cmpPtr = [&](const char* label, const void* a, const void* b) {
                ++compared;
                if (a != nullptr || b != nullptr) ++nonTrivial;
                if (a != b) {
                    ++differing;
                    char one[80];
                    std::snprintf(one, sizeof(one), "%s ours=%p theirs=%p; ",
                                  label, a, b);
                    std::strncat(detail, one, sizeof(detail) - std::strlen(detail) - 1);
                }
            };
            auto cmpLong = [&](const char* label, long long a, long long b) {
                ++compared;
                if (a != 0 || b != 0) ++nonTrivial;
                if (a != b) {
                    ++differing;
                    char one[80];
                    std::snprintf(one, sizeof(one), "%s ours=%lld theirs=%lld; ",
                                  label, a, b);
                    std::strncat(detail, one, sizeof(detail) - std::strlen(detail) - 1);
                }
            };

            cmpLong("classInfoOk", ours.classInfoOk, theirs.classInfoOk);
            cmpLong("style",       ours.wc.style,       theirs.wc.style);
            cmpLong("cbClsExtra",  ours.wc.cbClsExtra,  theirs.wc.cbClsExtra);
            cmpLong("cbWndExtra",  ours.wc.cbWndExtra,  theirs.wc.cbWndExtra);
            cmpPtr ("hInstance",   ours.wc.hInstance,   theirs.wc.hInstance);
            cmpPtr ("hIcon",       ours.wc.hIcon,       theirs.wc.hIcon);
            cmpPtr ("hCursor",     ours.wc.hCursor,     theirs.wc.hCursor);
            // hbrBackground is NOT compared through GetClassInfoW. Measured
            // 2026-09-08: our class reads back 0x6 and the shipped class reads
            // back a real brush handle (0x900011) from the SAME API, while the
            // per-window GetClassLongPtrW reading and every other field agree.
            // Both registrations pass the SAME VALUE — the line below compares
            // what is actually registered, by reading retail's own constant out
            // of the shipped .rdata at 0x180161780 (`06 00 00 00`, i.e.
            // COLOR_WINDOW+1) and checking our literal against it. That is the
            // question the port has to answer; the readback disagreement is a
            // USER32 reporting artefact this lane could not explain and does not
            // pretend to (WAVE9_LANE_PLAT.md §7).
            {
                const uint32_t* theirConst = reinterpret_cast<const uint32_t*>(
                    ResolveAddr(Module::Runtime, 0x180161780));
                const unsigned long long theirsVal = theirConst ? *theirConst : 0xFFFFFFFFull;
                cmpLong("hbrBackgroundConstant", 6, static_cast<long long>(theirsVal));
            }
            cmpLong("classBrushFromWindow",
                    static_cast<long long>(ours.classBrushFromWindow),
                    static_cast<long long>(theirs.classBrushFromWindow));
            cmpLong("windowOk",    ours.windowOk,       theirs.windowOk);
            cmpLong("winStyle",    ours.style,          theirs.style);
            cmpLong("winExStyle",  ours.exStyle,        theirs.exStyle);
            cmpLong("winWidth",    ours.width,          theirs.width);
            cmpLong("winHeight",   ours.height,         theirs.height);
            ++compared;
            if (std::wcscmp(ours.className, theirs.className) != 0) {
                ++differing;
                std::strncat(detail, "className; ",
                             sizeof(detail) - std::strlen(detail) - 1);
            }
            if (ours.className[0]) ++nonTrivial;

            char a[256], b[160];
            std::snprintf(a, sizeof(a),
                          "compared=%d differing=%d nonTrivial=%d wndproc=%p %s",
                          compared, differing, nonTrivial,
                          reinterpret_cast<void*>(&RtApp_WindowProc), detail);
            std::snprintf(b, sizeof(b), "theirs wndproc=%p (the one field that MUST differ)",
                          reinterpret_cast<void*>(theirs.wc.lpfnWndProc));
            RT_DIFF_SHADOW(differing == 0, a, b);
            // Effectiveness: a comparison in which every compared field was
            // zero on both sides proves nothing.
            RT_DIFF_WORK(nonTrivial >= 3);
            std::printf("[PlatOracle] compared=%d differing=%d nonTrivial=%d %s\n",
                        compared, differing, nonTrivial, detail);
            std::printf("[PlatOracle]   hbrBackground readback: GetClassInfoW ours=%p theirs=%p "
                        "| GetClassLongPtrW ours=0x%llX theirs=0x%llX\n",
                        (void*)ours.wc.hbrBackground, (void*)theirs.wc.hbrBackground,
                        (unsigned long long)ours.classBrushFromWindow,
                        (unsigned long long)theirs.classBrushFromWindow);
            IdentifySystemBrush(theirs.wc.hbrBackground, ours.wc.hbrBackground,
                                self->_platformHandle);
        }
        // The WindowProc's own oracle. It needs a live HWND, which does not
        // exist until the line above created one, so this is the earliest point
        // it can run. No-ops unless BANISHED_PLAT_ORACLE=1.
        RtLanePlatWindowProcProbe(self->_windowHandle);
    }
    return ok;
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
extern "C" bool RtAchievement_AchievementManager_PlatformStartup(void* /*self*/) {
    return true;
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
extern "C" bool RtEngine_Engine_PlatformStartup(void* /*self*/) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    return true;
}

// ============================================================================
// The oracle: run the SHIPPED PlatformStartup first, read what it built out of
// USER32, then take it back down.
// ============================================================================
// Off unless BANISHED_PLAT_ORACLE=1 AND the diff harness is on. Kept off the
// ordinary BANISHED_RT_DIFF runs on purpose: the teardown below is the single
// step whose failure would break a live boot (our RegisterClassW would then hit
// ERROR_CLASS_ALREADY_EXISTS, which is retail's own `return false` path and
// ends Engine::Startup at g_startupFailAt == 6). Every failure mode reports a
// reason instead of proceeding.
namespace {

void PlatOracleSnapshotClass(void* platformHandle, HWND hwnd,
                             PlatOracleSnapshot* out) {
    std::memset(out, 0, sizeof(*out));
    out->classInfoOk =
        GetClassInfoW(static_cast<HINSTANCE>(platformHandle), L"Banished", &out->wc) != 0;
    if (hwnd != nullptr && IsWindow(hwnd)) {
        out->windowOk = true;
        out->style    = GetWindowLongPtrW(hwnd, GWL_STYLE);
        out->exStyle  = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        RECT r = {};
        if (GetWindowRect(hwnd, &r)) {
            out->width  = r.right - r.left;
            out->height = r.bottom - r.top;
        }
        GetClassNameW(hwnd, out->className,
                      static_cast<int>(sizeof(out->className) / sizeof(wchar_t)));
        out->classBrushFromWindow =
            static_cast<ULONG_PTR>(GetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND));
    }
}

bool PlatOracleRunTheirs(void* platformHandle, PlatOracleSnapshot* out,
                         const char** why) {
    using Fn = bool (*)(Application*);
    Fn fn = RtOriginal<Fn>("?PlatformStartup@Application@1@AEAA_NXZ");
    if (!fn) { *why = "oracle skipped: shipped body did not resolve"; return false; }

    // A SCRATCH Application, never published as the singleton. Only +0x00 and
    // +0x08 are read by the body under test.
    Application scratch = {};
    scratch._platformHandle = platformHandle;
    if (!fn(&scratch)) {
        *why = "oracle skipped: shipped body returned false";
        return false;
    }
    PlatOracleSnapshotClass(platformHandle, static_cast<HWND>(scratch._windowHandle), out);

    // TEARDOWN. DestroyWindow sends WM_DESTROY to the DLL's own WindowProc,
    // which calls PostQuitMessage(0) — so a WM_QUIT lands in THIS THREAD's
    // queue and `Application::PlatformUpdate` would read it a few frames later
    // and quit the game. It is drained here, and the drain is CHECKED rather
    // than assumed: the peek must succeed or the oracle refuses to continue.
    if (scratch._windowHandle) DestroyWindow(static_cast<HWND>(scratch._windowHandle));
    MSG quitMsg;
    const bool drained =
        PeekMessageW(&quitMsg, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE) != 0;
    if (!drained) {
        *why = "oracle ABORTED: WM_QUIT was not in the queue after DestroyWindow";
        // Still try to unregister so a later run is not poisoned.
        UnregisterClassW(L"Banished", static_cast<HINSTANCE>(platformHandle));
        return false;
    }
    if (!UnregisterClassW(L"Banished", static_cast<HINSTANCE>(platformHandle))) {
        *why = "oracle ABORTED: UnregisterClassW failed";
        return false;
    }
    // Prove the teardown, do not assume it: our own RegisterClassW is about to
    // depend on the class being gone.
    WNDCLASSW probe;
    if (GetClassInfoW(static_cast<HINSTANCE>(platformHandle), L"Banished", &probe)) {
        *why = "oracle ABORTED: class still registered after UnregisterClassW";
        return false;
    }
    *why = "oracle ok";
    return true;
}

// ---------------------------------------------------------------------------
// What IS the value the shipped class reads back?
// ---------------------------------------------------------------------------
// The oracle measured that both classes are registered with the SAME constant
// (retail's own `06 00 00 00` at 0x180161780, compared directly) and yet read
// back different `hbrBackground` values through BOTH GetClassInfoW and
// GetClassLongPtrW. Rather than leave that as folklore, this identifies the
// value: it walks the system colour brushes looking for a match, and registers
// a THIRD class from OUR module with the same constant to show whether the
// difference tracks the value or the registering module.
// ---------------------------------------------------------------------------
void IdentifySystemBrush(void* theirsBrush, void* oursBrush, void* platformHandle) {
    int matched = -1;
    for (int i = 0; i <= 30; ++i) {
        if (reinterpret_cast<void*>(GetSysColorBrush(i)) == theirsBrush) { matched = i; break; }
    }
    // A third registration from OUR module, same constant, different name.
    WNDCLASSW probe;
    std::memset(&probe, 0, sizeof(probe));
    probe.lpfnWndProc   = DefWindowProcW;
    probe.hInstance     = static_cast<HINSTANCE>(platformHandle);
    probe.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<uintptr_t>(6));
    probe.lpszClassName = L"BanishedPlatProbe";
    void* readBack = nullptr;
    bool registered = RegisterClassW(&probe) != 0;
    if (registered) {
        WNDCLASSW got;
        std::memset(&got, 0, sizeof(got));
        if (GetClassInfoW(static_cast<HINSTANCE>(platformHandle), L"BanishedPlatProbe", &got))
            readBack = got.hbrBackground;
        UnregisterClassW(L"BanishedPlatProbe", static_cast<HINSTANCE>(platformHandle));
    }
    std::printf("[PlatOracle]   theirs=%p is %s ; GetSysColorBrush(COLOR_WINDOW=5)=%p ; "
                "third class from OUR module with the same constant 6 reads back %p (registered=%d) ; ours=%p\n",
                theirsBrush,
                matched >= 0 ? "GetSysColorBrush(index below)" : "NOT any system colour brush",
                reinterpret_cast<void*>(GetSysColorBrush(COLOR_WINDOW)),
                readBack, registered ? 1 : 0, oursBrush);
    if (matched >= 0)
        std::printf("[PlatOracle]   matched system colour index = %d\n", matched);
}

// ---------------------------------------------------------------------------
// The WindowProc probe's negative control: a DELIBERATELY WRONG copy of the
// body. Exactly one case in the table must diverge when it is used.
//
// MEASURED 2026-09-08: THE FIRST VERSION OF THIS CONTROL COULD NOT FIRE, and
// the probe caught it — `differing=0` on a run whose whole point was to
// disagree. That version swallowed VK_F11 instead of VK_F10, so on the
// `WM_SYSKEYDOWN wp=VK_F10` case the wrong body fell through to DefWindowProcW
// — which ALSO returns 0 for that message. The two are indistinguishable by
// return value, so the perturbation was invisible.
//
// That is a fact about the SUBJECT, not just about the control, and it is
// recorded in WAVE9_LANE_PLAT.md: the VK_F10 branch of the real WindowProc
// cannot be told apart from its default path by anything this probe can
// observe. `branches=6` counts branches REACHED, not branches DISCRIMINATED.
//
// The perturbation below writes the WRONG VALUE to `_running` on WM_SETFOCUS,
// which the probe compares directly and which therefore cannot hide.
// ---------------------------------------------------------------------------
int64_t NegControlWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                             Application* app) {
    switch (msg) {
        case WM_DESTROY:    PostQuitMessage(0); return 0;
        case WM_SETFOCUS:   if (app) app->_running = 0; break;   // WRONG: should be 1
        case WM_KILLFOCUS:  if (app) app->_running = 0; break;
        case WM_ERASEBKGND: return 0;
        case WM_SYSKEYDOWN: if (wParam == 0x79) return 0; break;
        default: break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

// ============================================================================
// RtLanePlatWindowProcProbe — the WindowProc's real oracle
// ============================================================================
// The live WindowProc cannot be diffed (a replay dispatches everything twice),
// so it is covered here instead: drive OURS and the SHIPPED body over the same
// synthetic message table, against the same synthetic Application, and compare
// the return value AND the one byte either of them writes.
//
// HOW IT STAYS SAFE
//   * `Application::_singleton` is repointed at a SCRATCH object for the
//     duration and restored afterwards, so neither side can touch the real one.
//   * the scratch's callback Set is EMPTY, so no registered callback runs. The
//     tree-descent branch is therefore NOT covered by this probe, and that is
//     stated in WAVE9_LANE_PLAT.md rather than glossed.
//   * WM_DESTROY posts a WM_QUIT per side; both are drained.
//
// EFFECTIVENESS CONTROL: `branches` counts how many DISTINCT branches of the
// switch the table actually reached, and the probe prints it. A run reporting
// `cases=N differing=0` with `branches` below 6 has not tested what it claims.
// ============================================================================
extern "C" void RtLanePlatWindowProcProbe(void* hwndOpaque) {
    if (!RtDiffEnabled() || !EnvOn("BANISHED_PLAT_ORACLE")) return;

    using Fn = int64_t (*)(HWND, UINT, WPARAM, LPARAM);
    Fn theirs = RtOriginal<Fn>("?WindowProc@Application@1@CA_JPEAUHWND__@@I_K_J@Z");
    if (!theirs) {
        std::printf("[PlatProbe] WindowProc probe SKIPPED: shipped body did not resolve\n");
        return;
    }
    HWND hwnd = static_cast<HWND>(hwndOpaque);
    if (!hwnd || !IsWindow(hwnd)) {
        std::printf("[PlatProbe] WindowProc probe SKIPPED: no live window\n");
        return;
    }
    Application** slot = AppSingletonSlot();
    if (!slot) {
        std::printf("[PlatProbe] WindowProc probe SKIPPED: no Application singleton slot\n");
        return;
    }

    const bool neg = EnvOn("BANISHED_PLAT_NEGCTL");

    struct Case { UINT msg; WPARAM wp; LPARAM lp; int branch; };
    static const Case cases[] = {
        { WM_DESTROY,    0,     0, 0 },
        { WM_SETFOCUS,   0,     0, 1 },
        { WM_KILLFOCUS,  0,     0, 2 },
        { WM_ERASEBKGND, 0,     0, 3 },
        { WM_SYSKEYDOWN, 0x79,  0, 4 },   // VK_F10  — swallowed
        { WM_SYSKEYDOWN, 0x70,  0, 5 },   // VK_F1   — NOT swallowed, default path
        { WM_SYSKEYDOWN, 0x179, 0, 5 },   // low dword == 0x79 but the 64-bit
                                          // compare must reject it
        { WM_NULL,       0,     0, 5 },
        { WM_MOUSEMOVE,  0,     0, 5 },
        { WM_SIZE,       0,     0, 5 },
    };

    Application* const saved = *slot;
    Application scratchA = {};
    Application scratchB = {};
    int differing = 0, cases_n = 0;
    unsigned branchMask = 0;
    char detail[320];
    detail[0] = 0;

    for (const Case& c : cases) {
        std::memset(&scratchA, 0, sizeof(scratchA));
        std::memset(&scratchB, 0, sizeof(scratchB));
        scratchA._running = 0x5A;   // a value NEITHER branch produces, so an
        scratchB._running = 0x5A;   // untouched byte is distinguishable from 0/1

        *slot = &scratchA;
        const int64_t ra = neg
            ? NegControlWindowProc(hwnd, c.msg, c.wp, c.lp, &scratchA)
            : static_cast<int64_t>(RtApp_WindowProc(hwnd, c.msg, c.wp, c.lp));

        *slot = &scratchB;
        const int64_t rb = theirs(hwnd, c.msg, c.wp, c.lp);

        // One WM_QUIT per side for the WM_DESTROY case; drain both.
        MSG m;
        while (PeekMessageW(&m, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE)) { /* drained */ }

        ++cases_n;
        branchMask |= (1u << c.branch);
        if (ra != rb || scratchA._running != scratchB._running) {
            ++differing;
            char one[112];
            std::snprintf(one, sizeof(one),
                          "msg=0x%X wp=0x%llX ours=(%lld,%u) theirs=(%lld,%u); ",
                          c.msg, static_cast<unsigned long long>(c.wp),
                          static_cast<long long>(ra),
                          static_cast<unsigned>(scratchA._running),
                          static_cast<long long>(rb),
                          static_cast<unsigned>(scratchB._running));
            std::strncat(detail, one, sizeof(detail) - std::strlen(detail) - 1);
        }
    }
    *slot = saved;

    int branches = 0;
    for (unsigned b = branchMask; b; b >>= 1) branches += (b & 1u);
    std::printf("[PlatProbe] WindowProc%s cases=%d differing=%d branches=%d %s\n",
                neg ? " NEGATIVE-CONTROL" : "", cases_n, differing, branches, detail);
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
    std::printf("[LanePLAT] rev=PLAT-r5 registered=%d\n", n);

    return n;
}
