# Phase D.4 SDL-5 hand-in (SDL Block 5: delete the Win32 backends)

Block SDL-5 of wave 4, worktree `d4-sdl5`, base `a125c96bb`. D1a-D1d were merged to main ahead of
SDL-4. D2 (drop the COM init) followed the coordinator's go after SDL-4 merged (`909045eb1`), on
a tree with main merged in (`b3cba9238`).

## 1. Commits and B11 verdicts

B11 base: `runs/b11base_d4d/perf/obj` (main `a125c96bb`, -O2). Moved objects were paired by hand
under their new names. The rename map is `RtEnginePlatformShiftHeld -> Platform::Input::ShiftHeld`
and `RtEngineLifecyclePlatformFreeLibrary -> Platform::ReleaseLibrary`.

| commit | title | B11 |
|---|---|---|
| `fafb5b4bc` | refactor: input remnants (D1a) | `input_platform_startup.o` IDENTICAL 11, `input_platform_shutdown.o` IDENTICAL 9 + RENAMED-IDENTICAL 1 (the file-named static init), `input_platform_update.o` IDENTICAL 9; `input_device_update.o`, `input_sdl.o` and the seven `application` objects IDENTICAL; `input_dinput_win.o` gone (empty `.text`) |
| `129d42d22` | behaviour: IsVisible / ShowDefault over SDL3 (D1b) | `window_sdl.o` IDENTICAL 9 + ONLY-TIP 2 (the new SDL bodies); `window_win.o` gone, its two bodies DIFF by design; `window_hook_win.o` IDENTICAL 4; callers `devicedata.o` 37, `devicedata_stubs.o` 11, `boot_window.o` 4 IDENTICAL |
| `cb18b6759` | refactor: ShiftHeld a `Platform::Input` primitive, WidePathExists over `Platform::PathAttributes` (D1c-1) | `input_win.o` (new) ShiftHeld RENAMED-IDENTICAL; `updateloop_path_exists.o` WidePathExists DIFF, accepted shape (import -> primitive); `engine_platform.o` IDENTICAL 13; `updateloop_loadresources.o` IDENTICAL 9 |
| `82b68e66b` | refactor: FreeLibrary on the gfx seam (D1c-2) | `gfx_win.o` IDENTICAL 4 + ONLY-TIP 1, paired with `engine_lifecycle_win.o`: RENAMED-IDENTICAL; `engine_lifecycle.o` IDENTICAL 15; `factory_dtors.o` IDENTICAL 21 |
| `901cc8d7d` | refactor: the PlatformStartup family and Application's cursor / launch members to `src/engine` (D1c-3) | `application_cursor_launch.o` IDENTICAL 3; `platform_startup_family.o` IDENTICAL 9 + DIFF 1 (`Engine::PlatformStartup`, accepted shape: `CoInitializeEx` import -> `Platform::ComInit`); all `src/engine/application` (31) and `src/engine/engine` + `achievement` (289) objects IDENTICAL |
| `86f4f8a99` | refactor: THREADS' three video `_win` files (D1d) | `buffers_lock.o` IDENTICAL 4; `singleton_ctors_lock.o` IDENTICAL 2; `video_resources_platform.o` IDENTICAL 1 + DIFF 1 (`RtVideoWriteFileRaw`, accepted shape: `WriteFile` import -> `Platform::FileWrite`); `save_image_to_file.o` IDENTICAL 10; 700 objects IDENTICAL in the sweep |
| `1fbac8b8f` | merge main (GAPS) | keep-both in `known_port_issues.md` (main's row 1275, then 1277 / 1278) |
| `c84d11f02` | behaviour: drop the COM init, `Platform::ComInit` / `ComUninit` and our `-lole32` (D2) | DIFF by design: `engine_lifecycle.o` DIFF 2 (Engine::Startup, Engine::Shutdown: the calls gone) + IDENTICAL 13; `platform_startup_family.o` DIFF 1 (Engine::PlatformStartup) + IDENTICAL 9; `main.o` DIFF 1 (WinMain: the CoInitializeEx before SDL_Init gone) + IDENTICAL 14; `os_win.o` IDENTICAL 1 + ONLY-BASE 2 (ComInit, ComUninit) |

D2 ran on SDL-4's tree, so its unit count is 1000 / 0. Every commit passed the static gate: `standalone-debug`, `standalone-perf` and `unit` at `-j2`
exit 0, unit 990 / 0, 0 new warnings against the main checkout's baselines (path-mapped for the
moved files), castmetric 2, abi_audit OK. The merged tree passes the same gate. No test was
added: every changed body is an OS call or a file move.

## 2. Import table (`objdump -p build/debug/OpenBA.exe`)

Identical before and after every D1 commit:

```
ADVAPI32 GDI32 IMM32 KERNEL32 api-ms-win-crt-* (13) ole32 SETUPAPI SHELL32 USER32 VERSION WINMM
```

The list is also unchanged after D2. DINPUT8 is absent throughout. **ole32 stays after D2**, as
the coordinator accepted: SDL3's `.pc` Libs carry `-lole32`, and every ole32 function the exe
imports after D2 is SDL's. They are CLSIDFromString, CoCreateInstance, CoInitializeEx,
CoTaskMemFree, CoUninitialize, OleInitialize, OleUninitialize, PropVariantClear, RegisterDragDrop,
ReleaseStgMedium and RevokeDragDrop (WASAPI / IMMDevice, OLE drag-and-drop, WIN_CoInitialize).
Our `-lole32` (Makefile `SA_LIBS`, CMake's WIN32 target) is gone.

## 3. Behaviour differences recorded

- **Row 1277: the first show of the game window.** `ShowDefault` is now `SDL_ShowWindow`, which
  is `ShowWindow(hwnd, SW_SHOW)` on Windows. Retail's `SW_SHOWDEFAULT` (VideoDX11 0x180004e10,
  CreateDevice step 6) honoured the STARTUPINFO show state. ShowWindow's documentation says a
  process's first ShowWindow ignores nCmdShow when the launcher supplied a show state, which would
  make the two equal. That is not verified by a run. A normal launch is identical.
- **Row 1278: Shift at launch on a POSIX host.** It is false there. Windows keeps retail's two
  `GetAsyncKeyState` calls (section 4).
- **Row 1279: the window accepts drops (D2 finding).** SDL enables its drop events by default,
  so the window is a drop target; the pump discards the drops, and retail's window accepts none.
  Before D2 that went through DragAcceptFiles, because the main thread was MTA and SDL's
  OleInitialize failed. Since D2 SDL's video init makes the main thread an STA, OleInitialize
  succeeds, and SDL uses RegisterDragDrop. The difference predates SDL-5; only the mechanism
  changed. The fix is two `SDL_SetEventEnabled(SDL_EVENT_DROP_*, false)` lines in
  `src/app/main.cpp`, which is LINUX-1's file, so SDL-5 did not make it.
- `IsVisible` reads SDL's `SDL_WINDOW_HIDDEN` flag. SDL keeps that flag from `SDL_ShowWindow` and
  WM_SHOWWINDOW, and nothing in the tree shows the window another way. No row: the answer is the
  same on every path the tree takes.

## 4. Deviations from the brief

1. **ShiftHeld is not over SDL on Windows.** SDL cannot answer this question.
   PlatformConfigure runs inside `Engine::Startup`, after the hidden window exists and before
   it is shown or focused. SDL's keyboard state comes only from key events delivered to its
   focused window. On Windows, `SDL_windowsevents.c` only clears a stuck Shift with GetKeyState
   and never sets one. On X11, SDL reconciles with XQueryKeymap only when the window gains focus.
   Over SDL every launch would read "not held". The fix:
   - The call became `Platform::Input::ShiftHeld`.
   - The Win32 twin keeps retail's calls (`src/platform/win/input_win.cpp`).
   - The POSIX twin (`src/platform/posix/input_posix.cpp`) is a pump plus `SDL_GetModState`.
     POSIX has no physical-key query at all; Wayland has none.
   - The commit is `refactor:`.
   - **Proposal:** keep this split. The user decides whether POSIX needs more; the settings.cfg
     entries and `-launcher` already cover the launcher there.
2. **EnableCursor is not changed to `Platform::Cursor::Show`.** Retail's EnableCursor makes no OS
   call. It stores `_cursorEnabled` and tail-calls the WM_SETCURSOR callback, which is
   `DeviceData_OnSetCursor`, and SDL-C already put that on `Platform::Cursor::Set` / `Show`. A
   direct Show would add a call retail does not make. The file moved unchanged as `refactor:`.
3. **`singleton_ctors_win.cpp` went to `src/engine/engine/`, not `src/engine/video/`.** Its
   callers are in `src/engine/engine`, and THREADS-1's hand-in placed it there.
4. **FreeLibrary is live, not dead.** Engine::Shutdown frees the renderer slots' sentinel module,
   which `src/app/video_interface.cpp` LoadLibraryW's. The call became `Platform::ReleaseLibrary`
   on the gfx seam: FreeLibrary on Windows, dlclose on POSIX. LINUX-1's POSIX sentinel must be a
   dlopen handle.
5. **`Engine::PlatformStartup`'s CoInitializeEx** (the unreachable export) is on
   `Platform::ComInit` now, so that file could move. D2 removes the call.
6. New file names, each without `_win`:

   | old | new |
   |---|---|
   | `input_win.cpp` | `input/input_platform_update.cpp` |
   | `engine_platform_win.cpp` | `engine/updateloop_path_exists.cpp` |
   | `platform_startup_win.cpp` | `engine/platform_startup_family.cpp` |
   | `application_win.cpp` | `application/application_cursor_launch.cpp` |
   | `buffers_win.cpp` | `video/buffers_lock.cpp` |
   | `video_resources_win.cpp` | `video/video_resources_platform.cpp` |

   The plain names already exist in those directories.
7. `RtVideoWriteFileRaw` takes a `Platform::FileHandle`, the type of `IO::FileInfo::winHandle`.
   That keeps castmetric at 2. The caller's local in `save_image_to_file.cpp` follows;
   `save_image_to_file.o` is IDENTICAL.

## 5. What is left in `src/platform/win`, and why each file stays

| file | why |
|---|---|
| `allocator_win.cpp`, `file_win.cpp`, `os_win.cpp`, `settings_win.cpp`, `sync_win.cpp`, `thread_win.cpp`, `time_win.cpp`, `gfx_win.cpp` | the Win32 bodies of the `Platform::` seams; each has a `src/platform/posix` twin |
| `input_win.cpp` (new) | `Platform::Input::ShiftHeld`, the physical-key query SDL cannot make (section 4.1); POSIX twin `input_posix.cpp` |
| `window_hook_win.cpp` | the Windows-only message observer the SDL pump needs (WM_EXITSIZEMOVE, the focus subclass); it now also carries the Win32 id asserts |
| `device_adjustdisplay_win.cpp`, `device_startup_win.cpp`, `video_interfaces_win.cpp` | LINUX-1's (the RENDER four names) |

Of POSIX's list of 30 `src/platform/win` names, SDL-5 made these portable:

- **INPUT 4.** `PlatformStartup`, `PlatformShutdown`, `PlatformUpdate` and `EnableMouseClipping`
  are in `src/engine/input`.
- **Application 1.** `EnableCursor` is in `src/engine/application`.
- **Engine platform 3.** ShiftHeld has a POSIX twin, WidePathExists is in `src/engine/engine`, and
  FreeLibrary has a gfx twin.
- **THREADS 5.** They are in `src/engine/video` and `src/engine/engine`.

## 6. Open items for the coordinator

- **Route checks** (static gate only here; the game was never run):
  - the launcher Shift hold: hold Shift at launch, expect the Half B counter in the
    `[RtPort] Engine::PlatformConfigure` summary line;
  - the window shown at boot: the BootWindow reveal and the resize after it;
  - a minimised and a maximised launch for row 1277: read `[StartupInfo]` and the window state;
  - cursor enable / disable through the menus;
  - a clean quit, which runs Engine::Shutdown's two `ReleaseLibrary` calls on the sentinel.
- **D2 route checks:**
  - boot to the menu and a new game, with the main thread now an STA;
  - audio on (WASAPI runs its own COM init on SDL's management thread);
  - a clean quit (SDL_Quit balances SDL's CoInitialize / OleInitialize);
  - a LaunchProcess / URL open if a route reaches one.
- **D2's three sites, all removed:**
  - `src/app/main.cpp`, the MTA init before `SDL_Init`; the file is LINUX-1's, and the brief
    assigned this line to D2;
  - `engine_lifecycle.cpp`, ComInit / ComUninit;
  - `platform_startup_family.cpp`.
- POSIX twins `input_posix.cpp` and `gfx_posix.cpp` were syntax-checked only
  (`g++ -fsyntax-only` under WSL Ubuntu). LINUX-1's build is their first link.

## 7. Shared files

- **Brief-listed shared files:** `docs/known_port_issues.md` gained rows 1277 and 1278 after
  1275, so SDL-4's 1276 goes between them. `src/platform/common/platform_input.h` gained
  `ShiftHeld` and a MouseState size assert.
- **Outside the row, named by the brief's "the gfx seam":** `src/platform/common/platform_gfx.h`,
  `src/platform/win/gfx_win.cpp` and `src/platform/posix/gfx_posix.cpp`, one function each.
- **Outside the row, comment-only path updates:** `src/engine/application/Application.h`,
  `src/engine/engine/Engine.h`, `src/engine/achievement/AchievementManager.h`,
  `src/engine/save/saveload_{saveinfo,update}.cpp`, `src/engine/video/*` and
  `tests/unit/video_device_abi_test.cpp`.
- **D2 edited:** `src/app/main.cpp` (one line plus its comment), `Makefile` `SA_LIBS` and
  CMakeLists.txt's WIN32 `target_link_libraries` (ole32 out), and `platform_os.h` /
  `os_win.cpp` / `os_posix.cpp` (ComInit out). `os_posix.cpp` was syntax-checked under WSL.
- **D1 needed no build rule:** The globs cover `src/engine/*/*.cpp`,
  `src/platform/{win,sdl}`, and `src/platform/posix` off Windows. `tools/abi_audit.baseline` is
  unchanged: `src/platform/win` is excluded from the audit, and the moved files add no counted
  pattern.
