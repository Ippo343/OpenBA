// ============================================================================
// src/platform/sdl/window_sdl.cpp -- Platform::Window: the game window over SDL3
// ============================================================================
// Phase D.4 SDL step W3 (plan §A.1, §C; D4_RULES C5).
//
// RETAIL (Application::PlatformStartup, 0x18009fc10):
//     RegisterClassW(&{0, WindowProc, 0, 0, hInstance, NULL, NULL, (HBRUSH)6, NULL,
//                      L"Banished"});
//     hwnd = CreateWindowExW(0, L"Banished", L"Banished", 0, CW_USEDEFAULT,
//                            CW_USEDEFAULT, 10, 10, NULL, NULL, hInstance, NULL);
//     UpdateWindow(hwnd); SetCursor(NULL);
//
// WHAT SDL DOES DIFFERENTLY, AND WHY IT IS ACCEPTED
//  * The class is SDL's: src/app/main.cpp registers it through SDL_RegisterApp with
//    retail's NAME ("Banished"), STYLE (0) and module handle, before SDL_Init(VIDEO);
//    its procedure is SDL's WIN_WindowProc, which queues SDL events that
//    src/platform/sdl/pump_sdl.cpp turns back into retail's message tuples. The class has
//    no background brush (retail: COLOR_WINDOW + 1, never painted because WindowProc
//    swallows WM_ERASEBKGND); SDL paints the client black on its first erase
//    (SDL_HINT_WINDOWS_ERASE_BACKGROUND_MODE's default), the colour the diag hook's
//    brush swap used to force.
//  * 10 x 10 is the CLIENT size (retail: the outer size of a WS_OVERLAPPED window) and the
//    position is SDL_WINDOWPOS_UNDEFINED (SDL centres it; retail CW_USEDEFAULT cascades).
//    Neither survives: Video::Device::PlatformStartup -> AdjustDisplay gives the window
//    its real style, size and place before it is shown.
//  * The window is created HIDDEN (retail: dwStyle 0, no WS_VISIBLE: also hidden) and
//    with SDL_WINDOW_VULKAN (D4_RULES C5: the dxvk-native route needs it). On Windows that
//    flag makes SDL load the Vulkan loader (vulkan-1.dll) when the window is created, and
//    creation FAILS without one: a machine with D3D11 but no Vulkan runtime now fails at
//    "failed to create window". Recorded in the SDL-B hand-in.
//  * UpdateWindow (a WM_PAINT for an empty update region of a hidden window) has no SDL
//    spelling and is not made. SetCursor(NULL) is not made either: SDL sets its own arrow
//    on every WM_SETCURSOR over the client area, and the game cursor is re-applied after
//    it by the WM_SETCURSOR callback the pump synthesises (SDL plan W5 moves the cursor
//    itself to SDL).
// ============================================================================

#include "platform_window.h"
#include "window_sdl_internal.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

#include <cstdio>
#include <string>

namespace Platform {
namespace Window {

namespace {
SDL_Window* g_window = nullptr;
}  // namespace

namespace Sdl {
SDL_Window* GameWindow() { return g_window; }
}  // namespace Sdl

bool VideoAvailable() {
    return SDL_WasInit(SDL_INIT_VIDEO) != 0;
}

void* Create(const char* title, int32_t width, int32_t height) {
    SDL_Window* w = SDL_CreateWindow(title, width, height,
                                     SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN);
    if (w == nullptr) {
        std::fprintf(stderr, "[SDL] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return nullptr;
    }
    g_window = w;
    Sdl::InstallPumpHooks();
    return NativeHandle();
}

void Destroy(void* /*window*/) {
    // Retail's DestroyWindow on the handle it is given; there is one window, so the
    // handle and g_window are the same object.
    if (g_window == nullptr) return;
    Sdl::RemovePumpHooks();
    SDL_DestroyWindow(g_window);
    g_window = nullptr;
}

void* NativeHandle() {
    if (g_window == nullptr) return nullptr;
#if defined(SDL_PLATFORM_WINDOWS)
    // Main thread only (SDL's rule): Create and the renderer's startup run there.
    return SDL_GetPointerProperty(SDL_GetWindowProperties(g_window),
                                  SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#else
    // dxvk-native: "HWNDs can become SDL_Window*s" (plan §C, D4_RULES C5).
    return g_window;
#endif
}

int64_t DefaultWindowProc(void* /*window*/, uint32_t /*message*/, uint64_t /*wParam*/,
                          int64_t /*lParam*/) {
    // SDL's WIN_WindowProc already ran DefWindowProcW (or its own handling) on the REAL
    // message before the event reached the pump; the synthesised tuple has nothing left
    // to default. WM_CLOSE's default (DestroyWindow -> WM_DESTROY) is reproduced by the
    // translator's second tuple (window_messages.h).
    return 0;
}

// ============================================================================
// SDL step W6: System::Error::Display's box.
// ============================================================================
// RETAIL (Error::Display, 0x1800c63c3): MessageBoxW(nullptr, wtext, wcap, 0x10) --
// MB_OK | MB_ICONERROR, no owner, modal on the calling thread.
//
// SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, caption, text, nullptr): one OK button,
// the error icon, no parent. It takes UTF-8, so the UTF-16 buffers retail built (with
// its clamp) are converted here, unpaired surrogates as U+FFFD (MessageBoxW would draw
// them as boxes). On Windows SDL tries TaskDialogIndirect first, which needs comctl32 v6;
// this exe declares no common-controls dependency in its manifest, so LoadLibrary finds
// comctl32 v5 without that export and SDL builds the box from a dialog template with no
// custom class: a "#32770" window titled with the caption, which is what
// src/diag/boot_state_watchdog.cpp looks for. SDL's box also centres on the primary
// display and shows the cursor; MessageBoxW did both too. Callable before SDL_Init.
// ============================================================================
namespace {

void AppendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

std::string Utf16ToUtf8(const uint16_t* s) {
    std::string out;
    for (size_t i = 0; s[i] != 0; ++i) {
        uint32_t cp = s[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && s[i + 1] >= 0xDC00 && s[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (s[i + 1] - 0xDC00);
            ++i;
        } else if (cp >= 0xD800 && cp <= 0xDFFF) {
            cp = 0xFFFD;
        }
        AppendUtf8(out, cp);
    }
    return out;
}

}  // namespace

void ShowErrorMessage(const uint16_t* textUtf16, const uint16_t* captionUtf16) {
    const std::string text    = Utf16ToUtf8(textUtf16);
    const std::string caption = Utf16ToUtf8(captionUtf16);
    if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, caption.c_str(), text.c_str(),
                                  nullptr)) {
        std::fprintf(stderr, "[SDL] SDL_ShowSimpleMessageBox failed: %s\n", SDL_GetError());
    }
}

}  // namespace Window
}  // namespace Platform
