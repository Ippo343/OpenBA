# Phase D.4 block SDL-B: SDL window, pump, display, message boxes (W3, W4, W6)

Branch `d4-sdlb`, base `5f9dcb6d2`. Main was merged at `92e2606cc`, which brought
RENDER-3 in. The block covers SDL plan steps W3, W4 and W6. W5 (the cursor) and W7
(LaunchProcess) were not started, and neither were the input and audio blocks.

Every commit was gated statically only: `standalone-debug`, `standalone-perf` and `unit` at
`-j2`, all exit 0. There are 0 new warnings against the main checkout's
`runs/warn_baseline_{debug,perf}.txt`, castmetric over `src` is 2, and `tools/abi_audit.py`
is OK. **The game was never run.** All three steps are `behaviour:` commits, so B11 is not
their gate. The coordinator's run checks are in section 3.

## 1. Commits

| commit | title | unit |
|---|---|---|
| `a5b2fa580` | behaviour: SDL window + pump (SDL plan W3) | 961 / 0 (+8) |
| `f5f0d5857` | behaviour: display and geometry over SDL (SDL plan W4) | 967 / 0 (+6) |
| `17c3f0c47` | behaviour: message boxes and the diag hook over SDL (SDL plan W6) | 970 / 0 (+3) |
| `92e2606cc` | Merge main into d4-sdlb (RENDER-3 in) | 970 / 0 |

Each commit message quotes the retail call sites and says what SDL does differently.

### W3: the window and the pump

- **The window.**
  - `Platform::Window::Create` calls `SDL_CreateWindow("Banished", 10, 10, HIDDEN | VULKAN)`.
  - `NativeHandle()` returns the HWND from `SDL_PROP_WINDOW_WIN32_HWND_POINTER`, so the swap
    chain in `devicedata.cpp` and DirectInput's `SetCooperativeLevel` both get the HWND that
    SDL created.
  - `main.cpp` registers the class with `SDL_RegisterApp("Banished", 0, hInstance)`, which
    keeps retail's class name and style.
- **The pump.**
  - `Platform::Pump(&Application::WindowProc)` is called from `PlatformUpdate` and from
    `MainLoop::Frame`. It turns each SDL event into retail's `(msg, wParam, lParam)` tuples.
    The translation is the pure function `TranslateWindowEvent`, whose table is in
    `src/platform/common/window_messages.h`.
  - Focus events produce ACTIVATE first, then SET/KILLFOCUS.
  - A close request produces WM_CLOSE then WM_DESTROY. WindowProc's WM_DESTROY arm posts the
    quit.
  - The wheel produces WM_MOUSEWHEEL with ±120 in the high word. A flipped direction is
    negated, and a horizontal-only event (y == 0) produces nothing.
  - Mouse motion and button-down produce WM_SETCURSOR with HTCLIENT.
  - Minimise produces WM_SIZE(SIZE_MINIMIZED, 0). Maximise, restore and pixel-size changes
    produce WM_SIZE with the client size.
- **Hints and init order in `main.cpp`.**
  - The DPI-awareness hint is `"unaware"`. The retail manifest has trustInfo only, so retail
    is DPI-unaware.
  - The other hints: `SDL_QUIT_ON_LAST_WINDOW_CLOSE=0`, `SDL_NO_SIGNAL_HANDLERS=1`,
    `SDL_VIDEO_ALLOW_SCREENSAVER=1`, `SDL_MOUSE_AUTO_CAPTURE=0`.
  - `CoInitializeEx(MULTITHREADED)` runs before `SDL_Init(SDL_INIT_VIDEO)`.
- **The application members.** `Application::PlatformStartup`, `PlatformShutdown` and
  `PlatformUpdate` moved to `src/engine/application/application_platform.cpp`.

### W4: geometry and displays

`src/platform/sdl/display_sdl.cpp` provides every geometry primitive, each with the Win32
meaning its caller was written against. Section 2 explains the outer-rect arithmetic. The pure
helpers are in `src/platform/common/display_geometry.{h,cpp}`. The frame limiter's refresh
rate comes from `SDL_GetCurrentDisplayMode`.

### W6: message boxes and the diag hook

- `Error::Display` calls `Platform::Window::ShowErrorMessage(wtext, wcap)` with retail's
  clamped UTF-16 buffers. The SDL body is `SDL_ShowSimpleMessageBox(ERROR)`.
- `main.cpp` uses `SDL_ShowSimpleMessageBox` for the init-failure box.
- The WH_CALLWNDPROC hook is now `WndMsgObserver`, installed with
  `Platform::Window::SetMessageObserver` and called by the pump before WindowProc on the main
  thread.
- The black-brush swap is deleted.

## 2. Deviations from the plan and the brief

1. **The quit is a flag, not an `SDL_PushEvent`.** Win32's `PostQuitMessage` raises a flag
   that becomes one WM_QUIT once the queue is drained, however often it was called. Both
   pumps call it every tick while `_quitRequested` is set. Pushing an event per call would
   leave stale QUITs behind. `Pump` reports the flag after the drain. SDL's own
   `SDL_EVENT_QUIT` is honoured where it sits in the queue.
2. **WM_EXITSIZEMOVE comes from a message hook, not from `SDL_EVENT_WINDOW_MOVED`.** MOVED
   also fires for AdjustDisplay's own placement and for DXGI mode switches. `MoveWindow`
   would then persist the client origin after every programmatic move, and the saved X/Y
   would drift by one frame on every run. Retail persists only after a user drag.
   - On Windows, `src/platform/win/window_hook_win.cpp` uses `SDL_SetWindowsMessageHook`.
     SDL calls the hook for every message inside the modal move / size loop, WM_EXITSIZEMOVE
     included, and the hook queues a private SDL event.
   - On other platforms MOVED is still the mapping.
   - The hook file sits in `win/` because it reads a `MSG`.
3. **The video subsystem is initialised in `main.cpp`, with an MTA COM init first.** SDL's
   video init asks for an STA. Without the earlier `CoInitializeEx(MULTITHREADED)` the main
   thread would become an STA, and Engine::Startup's MTA request (0x1800dbc12, which runs
   after the window exists) would be refused. With it SDL's `OleInitialize` is refused with
   RPC_E_CHANGED_MODE. SDL tolerates that and falls back to its own drag-and-drop, which
   nothing here uses.
4. **W4 changed the API shape.**
   - `GetStyle` / `SetStyle` became `SetFramed(window, bool)`.
   - `AdjustWindowRect(rect, style, menu)` became `AdjustWindowRect(window, rect)`, using the
     window's current frame.
   - `GetDesktopWindow` + `GetWindowRect` became `GetPrimaryDisplayRect`.
   - `MonitorFromRect` + `kMonitorDefaultToNull` became `RectOnAnyDisplay`.
   - `GetCurrentDisplayRefreshHz` was added.
   - The arithmetic in `AdjustDisplay` is untouched. Retail's calls are quoted at each line.
5. **The outer-rect arithmetic is kept on purpose.** SDL places the client area, so outer
   rects are converted with the frame from `SDL_GetWindowBordersSize`. The last good value
   is kept per style for the minimised and not-yet-applied cases. This keeps retail's drift:
   the saved client origin is reused as the outer origin. A unit test covers it.
6. **The Win32 bodies replaced by SDL are gone from `window_win.cpp`.** One symbol can have
   only one body. What stays Win32 is `ClipCursor` (the input block's), RENDER-3's
   `IsVisible` / `ShowDefault` / `Cursor`, and the id / layout asserts. No `_win` file was
   deleted.
7. **Behaviour differences recorded in the commit messages** (none of them is a guard):
   - Sent messages arrive at the next pump instead of synchronously. WM_SIZE caused by
     AdjustDisplay's own SetWindowPos used to be swallowed by `field9D`. It now arrives after
     `field9D` is cleared, and SizeWindow finds the size already equal.
   - Focus events produced during Engine::Startup now reach callbacks registered later in
     Startup: DeviceData_OnActivate (a no-op unless `wasFullscreen`) and Input's
     OnFocusMessage.
   - SDL swallows every SC_KEYMENU. An Alt tap no longer enters the system-menu mode it did
     in retail; F10 was already swallowed.
   - The game cursor no longer shows over the title bar. SDL handles non-client SETCURSOR
     itself.
   - SDL sets its arrow on each client-area WM_SETCURSOR before the synthesised callback
     re-applies the game cursor. A flicker is possible until W5.
   - A maximised window ignores the position and size requests.
   - The fullscreen branch's HWND_TOP raise is not made.
   - Retail's leftover WS_POPUP on a window that had been fullscreen is dropped.
   - The window class has no background brush. SDL paints black on the first erase.
   - `SDL_WINDOW_VULKAN` makes SDL load `vulkan-1.dll` when the window is created, and
     creation fails without it. A machine with D3D11 but no Vulkan runtime now fails with
     retail's "failed to create window" text (D4_RULES C5).
8. **Row 1273 in `docs/known_port_issues.md` (CONFIRMED in Ghidra).** Retail GameMain calls
   `System::Error::Display()` when Engine::Startup fails: import slot 0x140152b58, xref
   0x140041f59. Our `game_main.cpp` only logs the error and returns 3. The fix is left for
   the coordinator, because it makes every failed unattended boot block on a modal.

## 3. The coordinator's run checks (the merge gate)

Every line below goes to `standalone_debug.log` unless it is marked `error.log`.

| # | check | how | expected |
|---|---|---|---|
| 1 | boot + new game | `sa-run`, `tools/boot_state.sh --wait 300` | RUNNING. `error.log` has no `[SDL] SDL_Init(SDL_INIT_VIDEO) failed`, `[SDL] SDL_RegisterApp failed` or `[SDL] SDL_CreateWindow failed`. Debug log has `[Standalone] Window-message observer installed (focus diag)`. New game 5683 / 0. |
| 2 | menu | `sa-run-menu` | It reaches the menu. `[WndMsg] WM_ACTIVATE  ... w=1` is followed by `[WndMsg] WM_SETFOCUS`, ACTIVATE before SETFOCUS. |
| 3 | menu-driver routes | `phase_d/uiroutes.sh` (Mods button 7, a PAUSEMENU route, MODAL_ANSWER) | Unchanged from the base. The driver injects through dialog handlers and Input state, not window messages (`autostart.cpp`, `mouse_inject.h`, `modal_driver.h`). |
| 4 | resave | `tools/resave_gate/resave_gate.sh Gate` | 5 / 5 == golden. |
| 5 | world | `phase_d/worldcmp.sh` | Matches `runs/POSTBLOCKS_worldcmp`. |
| 6 | windowed launch | `VideoFullscreen=0` in settings.cfg, launch | It opens framed and resizable at the saved place. `[VideoReg ...] VideoWindowX` / `Y` are logged. |
| 7 | resize | Drag a frame edge, release | `[WndMsg] WM_SIZE ... type=0 WxH` lines, then the back buffer follows. One `WM_EXITSIZEMOVE` tuple at release. It is not logged by name, so check the next run's saved X/Y. |
| 8 | minimise / restore | Taskbar | `[WndMsg] WM_SIZE ... type=1 0x0` then `WM_ACTIVATE w=0` and `WM_KILLFOCUS`. The game pauses. On restore: `WM_SIZE type=0 WxH`, `WM_ACTIVATE w=1`, `WM_SETFOCUS`. |
| 9 | saved X/Y across two runs | Run 1: drag the window, quit cleanly, note `VideoWindowX/Y` in settings.cfg. Run 2: quit without moving. | After run 1 X/Y is the client origin after the drag; retail's one-frame drift shows when run 2 opens. Run 2 must leave X/Y **unchanged**. If X/Y grows by the frame on every run, deviation 2 has failed. |
| 10 | alt-tab | windowed and fullscreen | Focus pairs as in rows 2 and 8. In exclusive fullscreen: `[CtxReset WM_ACTIVATE(refocus)]` on return, and DeviceData_OnActivate drops and restores fullscreen as before. |
| 11 | wheel | Zoom in / out in game | The direction is as before. |
| 12 | close button / Alt+F4 | Windowed | Clean quit within the tick. `[BootState] QUITTING`. No second quit. |
| 13 | F10 / Alt tap | In game | No menu-mode freeze. Alt no longer enters menu mode (deviation 7). |
| 14 | DPI | On a 125 % or 150 % display, if available | The window is bitmap-scaled like retail (DPI-unaware). Coordinates and the clip rect line up with the cursor. |
| 15 | a modal | See below | `boot_state.sh` exits 2 with BLOCKED_DIALOG and `detail=` the title. |

**Forcing a modal.**
- A missing data directory does **not** show a box on this tree (row 1273): the boot exits 3
  with `[GameMain] System::Error string after Startup failure:` in the debug log.
- Until row 1273 is fixed, the only reachable boxes are these:
  - `Error::Display` on the allocator's out-of-memory path with `stopApplication`, which
    Engine::Startup never passes.
  - `main.cpp`'s init-failure box, reached when `Runtime-x64.dll` is mapped into the process.
- Neither is practical to force. If row 1273 is fixed first, the missing data directory route
  works: rename the data directory and boot.
- What the watchdog looks for: a visible top-level window of this process whose class is
  `#32770` (`src/diag/boot_state_watchdog.cpp` EnumProc). The SDL box is exactly that:
  - The exe's manifest has no common-controls v6 dependency (dumped from
    `build/debug/OpenBA.exe`), so SDL's `TaskDialogIndirect` lookup fails.
  - SDL then falls back to `DialogBoxIndirectParam` on a template with "No custom class"
    (`SDL_windowsmessagebox.c`), and its title is the caption.
  - The unit test `error_display_test.cpp` checks the text and caption it is handed.

## 3b. Fix after the merge: focus and activation on Windows

The coordinator's routes after the merge (`runs/SDLB/*`) logged no WM_ACTIVATE and no
WM_SETFOCUS. The RENDER-3 run one merge earlier logged `WM_NCACTIVATE w=0`,
`WM_ACTIVATE w=1` and `WM_SETFOCUS` at the window's reveal.

**Cause.** The route is launched from a background shell, so the foreground lock denies the
window the foreground. Windows still ACTIVATES it: an inactive title bar
(`WM_NCACTIVATE(0)`), `WM_ACTIVATE(WA_ACTIVE)` and `WM_SETFOCUS`. Retail's WindowProc
received all three. SDL raises FOCUS_GAINED only when `GetForegroundWindow() == hwnd`
(`WIN_UpdateFocus`, SDL_windowsevents.c:336-341), so it queued nothing. The window being
created hidden is not the cause: SDL's WM_WINDOWPOSCHANGED clears the hidden flag on any
SWP_SHOWWINDOW, including a raw `ShowWindow`.

**Fix.** `src/platform/win/window_hook_win.cpp` subclasses the window procedure ahead of
SDL's, using `SetWindowLongPtrW`. Its procedure chains to SDL's with `CallWindowProcW`. It
queues WM_ACTIVATE, WM_SETFOCUS and WM_KILLFOCUS exactly as received (message, wParam,
lParam), in order with SDL's events, through `QueueRawMessage`, which uses a private SDL
event and a FIFO. The pump dispatches them as `RawMessage` tuples. On Windows the pump drops
SDL's foreground-based FOCUS_GAINED / LOST; on other platforms they remain the mapping. SDL's
own procedure is restored before `SDL_DestroyWindow`.

**Expected now, in an unattended route.** These lines appear in `standalone_debug.log` right
after `[BootWindow] revealing ...`, or at the first pump after it:

```
[WndMsg] WM_ACTIVATE          hwnd=<h> w=1 l=0
[WndMsg] WM_SETFOCUS          hwnd=<h> w=0 l=0
```

They come in that order, ACTIVATE before SETFOCUS: the order Win32 sent them. `l` and `w`
are what Windows passed, so `w` can be 2 for a click activation. `WM_SIZE ... type=0 1024x640`
appears where SDL reports a size change. A same-size WM_SIZE that retail received during the
show is not repeated (SDL drops unchanged sizes), and SizeWindow is idempotent. WM_NCACTIVATE
and WM_ACTIVATEAPP are not dispatched: nothing registers them and WindowProc does not
switch on them.

## 4. Open items

- **W5, after RENDER-3, which is now merged:**
  - `Platform::Cursor::{Create, Set, Show, Destroy}` over SDL. RENDER-3's hand-in has the
    plane mapping and the dangling-handle question.
  - Once SDL owns the cursor, `SDL_SetCursor` makes SDL's WM_SETCURSOR apply the game cursor,
    and the flicker in deviation 7 goes.
  - The synthesised WM_SETCURSOR stays because `EnableCursor` calls the same callback.
- **W7:** `LaunchProcess` over SDL.
- **The input block (I1-I4):**
  - `ClipCursor` and the `GetCursorPos` / `ScreenToClient` / `GetForegroundWindow` reads
    are still Win32 on the SDL HWND.
  - The keyboard focus test should become `SDL_GetKeyboardFocus`.
- **Block 5:** `IsVisible` / `ShowDefault` stay Win32 on the SDL HWND. SDL tracks the show
  through WM_SHOWWINDOW. Moving them to `SDL_ShowWindow` / the flags is a `behaviour:` step,
  because SW_SHOWDEFAULT honours STARTUPINFO and SDL ignores it.
- **Row 1273:** retail's Error::Display on a failed Startup is the coordinator's decision.
- **`device_startup_win.cpp` / `device_adjustdisplay_win.cpp`** contain no Win32 any more.
  Moving them to `src/engine/video/` is a rename commit.
- **The CMake twin was not built.** The new files fall under its existing globs
  (`src/platform/common`, `src/platform/sdl`, `src/platform/win`, `src/engine` recursive,
  `tests/unit/*.cpp`). `tests/unit/sources.cmake` lists the two new common sources.

## 5. Shared files

- **`src/platform/common/platform_window.h`** (shared with RENDER-3, whose additions are
  kept):
  - **Added in W3:** `VideoAvailable`, `Create`, `Destroy`, `NativeHandle`,
    `MessageHandler`, `MessageObserver`, `SetMessageObserver`, `Platform::Pump`.
  - **Comments changed in W3:** `DefaultWindowProc` and `PostQuitMessage`. Their bodies are
    SDL's now.
  - **Added in W4:** `SetFramed`, `RectOnAnyDisplay`, `GetPrimaryDisplayRect`,
    `GetCurrentDisplayRefreshHz`.
  - **Changed in W4:** `AdjustWindowRect(void*, Rect*)`. The declarations of
    `GetClientRect`, `GetWindowRect`, `ClientToScreen`, `GetShowCmd`, `IsIconic`,
    `SetWindowPos`, `GetMonitorRect` and `GetCurrentDisplaySize` keep their signatures and
    get SDL bodies.
  - **Removed in W4:** `GetStyle`, `SetStyle`, `GetDesktopWindow`, `MonitorFromRect`,
    `kMonitorDefaultToNull`.
  - **Added in W6:** `ShowErrorMessage`.
- **`src/platform/win/window_win.cpp`** (shared with RENDER-3):
  - Removed: `DefaultWindowProc`, `PostQuitMessage` and every geometry body.
  - Kept: `ClipCursor`, RENDER-3's `IsVisible` / `ShowDefault` / `Cursor::*`, and the
    asserts. The monitor-flag assert is gone.
- **`src/app/main.cpp`** (THREADS-2's diag split touches it): the SDL init block
  (`SDL_RegisterApp`, the hints, `CoInitializeEx`, `SDL_Init(SDL_INIT_VIDEO)`,
  `SDL_UnregisterApp`), the init-failure box, and the hook region (`WndMsgObserver`,
  `InstallWndMsgHook`, with the brush swap deleted).
- **`src/engine/engine/mainloop_frame.cpp`:** the pump region only (the `extern "C"` USER32
  block, `kWmQuit` and the PeekMessage loop).
- **Other files touched:**
  - `src/platform/win/application_win.cpp`: PlatformUpdate removed.
  - `src/platform/win/platform_startup_win.cpp`: the Application pair, the class
    registration and `RtApp_WindowProc` removed.
  - `src/platform/win/device_adjustdisplay_win.cpp`
  - `src/platform/win/device_startup_win.cpp`: comment only.
  - `src/engine/application/window_proc.cpp`: comment only.
  - `src/engine/system/error.cpp`
  - `src/app/frame_limiter.cpp`
  - `src/game/Dialog/mainmenudialog.cpp`: the Quit fallback.
  - `tests/unit/sources.{mk,cmake}`
  - `tools/abi_audit.baseline`: rows only lowered.
  - `docs/known_port_issues.md`: row 1273.
- **New files:**
  - `src/platform/sdl/{window_sdl.cpp, pump_sdl.cpp, display_sdl.cpp, window_sdl_internal.h}`
  - `src/platform/common/{window_messages, display_geometry}.{h,cpp}`
  - `src/platform/win/window_hook_win.cpp`
  - `src/engine/application/application_platform.cpp`
  - `tests/unit/{sdl_event_translate_test, display_geometry_test, error_display_test}.cpp`
