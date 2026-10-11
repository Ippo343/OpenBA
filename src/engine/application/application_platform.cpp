// ============================================================================
// src/engine/application/application_platform.cpp -- the Application's window and pump
// ============================================================================
// Phase D.4 SDL step W3. PlatformStartup / PlatformShutdown (were
// src/platform/win/platform_startup_win.cpp) and PlatformUpdate (was
// src/platform/win/application_win.cpp) over Platform::Window and Platform::Pump; no OS
// header. The Win32 window class, its WNDPROC thunk (RtApp_WindowProc) and the
// PeekMessageW loop are gone: SDL owns the window procedure, and the class is
// registered with SDL by src/app/main.cpp (SDL_RegisterApp, retail's name and style).
//
//   ?PlatformStartup@Application@1@AEAA_NXZ   retail 0x18009fc10   ord 3402
//   ?PlatformShutdown@Application@1@AEAAXXZ   retail 0x18009fd70   ord 3393
//   ?PlatformUpdate@Application@1@AEAA_NXZ    retail 0x18009fdf0   ord 3410
// ============================================================================

#include <cstdint>

#include "application/Application.h"
#include "platform_window.h"
#include "system/Error.h"

using App = Application::Application;

namespace {

// The two literals retail passes, read out of the shipped .rdata: 0x180154870 and
// 0x1801548c0.
const char kErrRegister[] = "Application: The application failed to register a window class properly.";
const char kErrCreate[]   = "Application: The application failed to create window properly.";
// 0x180154858: the window class name and the window title are the same string object in
// retail (RBX for both CreateWindowExW arguments). The class half is main.cpp's now.
const char kWindowTitle[] = "Banished";

}  // namespace

// ============================================================================
// Application::Application::PlatformStartup()          retail 0x18009fc10
// ----------------------------------------------------------------------------
//   WideString className("Banished");
//   if (!RegisterClassW(&wc)) { Error::SetText(<register msg>, false); ok = false; }
//   else {
//       this->_windowHandle = CreateWindowExW(0, className, className, 0,
//                                             CW_USEDEFAULT, CW_USEDEFAULT, 10, 10,
//                                             NULL, NULL, this->_platformHandle, NULL);
//       if (!this->_windowHandle) { Error::SetText(<create msg>, false); ok = false; }
//       else { UpdateWindow(hwnd); SetCursor(wc.hCursor /* NULL */); ok = true; }
//   }
//
// Over SDL (src/platform/sdl/window_sdl.cpp says what differs in the window itself):
//  * the REGISTER arm is "the video subsystem is not up": the class is registered with
//    SDL before SDL_Init(SDL_INIT_VIDEO) (src/app/main.cpp), so its failure and the video
//    init's are one failure, reported with retail's register text. As in retail, that arm
//    does not write `_windowHandle`.
//  * the CREATE arm stores the result BEFORE testing it, as retail's 0x18009fcf4 does, so a
//    failed creation leaves an explicit null.
//  * the className WideString (an allocation made only to hand USER32 a UTF-16 name) is
//    not built: SDL takes the UTF-8 title.
// ============================================================================
bool Application::Application::PlatformStartup() {
    if (!Platform::Window::VideoAvailable()) {
        System::Error::SetText(kErrRegister, false);
        return false;
    }

    void* const hwnd = Platform::Window::Create(kWindowTitle, 10, 10);
    _windowHandle = hwnd;                                        // 0x18009fcf4
    if (hwnd == nullptr) {
        System::Error::SetText(kErrCreate, false);
        return false;
    }
    return true;
}

// ============================================================================
// Application::Application::PlatformShutdown()         retail 0x18009fd70
// ----------------------------------------------------------------------------
//   if (this->_windowHandle) DestroyWindow(this->_windowHandle);
//   UnregisterClassW(L"Banished", this->_platformHandle);
//
// `_windowHandle` is NOT nulled afterwards (retail's; Engine::Shutdown frees the
// Application a few lines later). Neither result was tested. The class is SDL's and is
// unregistered by SDL_UnregisterApp after SDL_Quit (src/app/main.cpp), the pair of the
// registration there.
// ============================================================================
void Application::Application::PlatformShutdown() {
    if (_windowHandle) Platform::Window::Destroy(_windowHandle);  // 0x18009fd8a
}

// ============================================================================
// Application::Application::PlatformUpdate()           retail 0x18009fdf0
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
//
// The loop is Platform::Pump's (src/platform/sdl/pump_sdl.cpp): it dispatches every
// translated message to WindowProc and reports WM_QUIT. TranslateMessage made WM_CHAR
// from key messages; nothing registered reads WM_CHAR (text entry is the key index +
// SelectCharacter, SDL plan §A.4), so SDL's text input stays off and nothing replaces it.
// ============================================================================
bool Application::Application::PlatformUpdate() {
    if (_quitRequested) Platform::Window::PostQuitMessage(0);
    return !Platform::Pump(&Application::WindowProc);
}
