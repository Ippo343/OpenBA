# Phase D.4 block SDL-A: SDL3 vendored and linked; Platform::Window (W1, W2)

Branch `d4-sdl`, base `62b5d4a32`, main merged at `967aabf59` (THREADS-1 and SETTINGS-1 in).
Plan steps B0.1, B0.2, W1 and W2 of `plans/SDL.md`. W3 onward was not started: it shares
`mainloop_frame.cpp` / `application_win.cpp` with THREADS step 4 (D4_RULES C4), which is now
on main, so W3 can start from this branch's tip.

Static gate only, per commit: `standalone-debug`, `standalone-perf` and `unit` at `-j2`, all
exit 0. There are 0 new warnings against the main checkout's `runs/warn_baseline_{debug,perf}.txt`,
and castmetric over `src` is 2. The game was never run.

## Commits

| commit | title | unit | B11 (perf objects vs `runs/b11base_d4/perf/obj`) |
|---|---|---|---|
| `60d9d3e36` | build: vendor SDL3 (submodule, pinned tag release-3.2.30) | 920 / 0 | no TU touched |
| `73f5cf7da` | build: link SDL3 static, SDL_SetMainReady + SDL_Init(0) | 920 / 0 | only `src/app/main.cpp` recompiled (the change itself) |
| `e96e5d2f6` | refactor: Platform::Window API with a Win32 implementation | 920 / 0 | 6 DIFF, all the accepted shape (below) |
| `18cd635dd` | refactor: WindowProc to src/engine/application, message ids named | 920 / 0 | moved body equal but for the 2 accepted calls; 29 TUs otherwise IDENTICAL |
| `967aabf59` | Merge branch 'main' into d4-sdl | 945 / 0 | Makefile / CMakeLists glob conflicts resolved keep-both |
| `23e715811` | refactor: platform_window.h included by its bare name | 945 / 0 | 8 TUs IDENTICAL (91 functions) vs the merge's objects |
| (this) | build: re-pin SDL3 to release-3.4.18 | 945 / 0 | `main.o` IDENTICAL (28 functions) vs its 3.2.30 build; no other TU recompiled |

### B11 verdicts in detail

The accepted shape: an indirect call through the import slot became a direct call to the
one-line primitive (`call *__imp_GetClientRect` became `call Platform::Window::GetClientRect`).
`b11pair.py` was run with an `__imp_<Win32> -> Platform::Window::<primitive>` map. The full
call lists and instruction text were read with a scratch script over `b11cmp.parse`, because
`b11pair` truncates call lists at 40 entries.

- **W1, `device_startup_win.o`:** SizeWindow, MoveWindow and GetDefaultResolution are DIFF,
  all three in the accepted shape. GetDefaultResolution's one store-list change is
  `dmSize = 0xdc`, which moved into the primitive with its memset.
- **W1, `device_adjustdisplay_win.o`:** AdjustDisplay is DIFF in the accepted shape. Its twelve
  import calls became the twelve primitives in the same order. The MONITORINFO / WINDOWPLACEMENT
  set-up and the GWL_STYLE / HWND_NOTOPMOST constants moved into the primitives. The tool
  reports new stores at +0x14 / +0x18 / +0x1c and a compare at +0x40. Those are the same
  Configuration accesses: `cfg` moved from `%rbp` to `%r12`, and b11cmp treats `%rbp`-relative
  accesses as stack.
- **W1, `input_win.o`:** PlatformUpdate is DIFF in the accepted shape (four calls).
  EnableMouseClipping is IDENTICAL.
- **W1, `game_main.o`:** RunGameMain is DIFF in the accepted shape, at both shutdown log sites.
  The other 8 functions are IDENTICAL.
- **W2, `window_proc.o`:** new, paired by hand with the base's WindowProc. The text is equal
  except `jmp *__imp_DefWindowProcW` became `jmp Platform::Window::DefaultWindowProc`, and
  `call *__imp_PostQuitMessage` became `call Platform::Window::PostQuitMessage`.
- **W2, `platform_startup_win.o`:** RtApp_WindowProc is SAME. It is the same `jmp WindowProc`,
  now a cross-object relocation.
- **W2, everything else:** IDENTICAL, including `devicedata.o` and `device_startup.o`, whose
  ids were named. Read these with an empty map: the W1 map makes untouched direct
  `PostQuitMessage` / `GetClientRect` callers show as SAME.

## SDL pin and build

- **Tag:** `release-3.4.18` (the current stable, 2026-10-02), commit
  `829a65d769d935c4852f8159e964312c0957260a`, per the coordinator's update and the revised
  plan §B. B0.1 first pinned `release-3.2.30` (`f5e5f658`, the brief's "newest 3.2.x"); the
  re-pin is a separate commit, nothing amended.
- **Submodule:** `third_party/SDL`, with `shallow = true` in `.gitmodules`.
- **Toolchain:** GCC 13.1 ucrt64 built both tags as is, exit 0 with 0 warnings, from an empty
  `build/third_party`. No fallback was needed. The cmake line that works on 3.4.18:

```
cmake -S third_party/SDL -B build/third_party/sdl3 -G Ninja -DCMAKE_BUILD_TYPE=Release
      -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
      -DSDL_GPU=OFF -DSDL_RENDER=OFF -DSDL_CAMERA=OFF -DSDL_JOYSTICK=OFF -DSDL_HAPTIC=OFF
      -DSDL_HIDAPI=OFF -DSDL_SENSOR=OFF -DSDL_POWER=OFF -DSDL_DIALOG=OFF -DSDL_TRAY=OFF
      -DCMAKE_INSTALL_PREFIX=<abs>/build/third_party/install
cmake --build build/third_party/sdl3 -j2 --target install
```

- **Options:** 14 of the plan's 15 option names exist in 3.4.18. `SDL_NOTIFICATION` does not
  (it is on SDL's `main` only), so it is not passed. `SDL_HAPTIC` stays OFF with
  `SDL_JOYSTICK` OFF (`CMakeLists.txt:1327` fails configure otherwise).
- **System libraries:** the installed `sdl3.pc` lists kernel32 user32 gdi32 winmm imm32 ole32
  oleaut32 version uuid advapi32 setupapi shell32. The plan's `hid` and `mincore` are absent
  because joystick / HIDAPI are off. The link reads the `.pc`, so nothing is retyped.
- **CMake twin:** fresh configure plus `-t SDL3-static` both exit 0 on 3.4.18. The full twin
  exe was not built.
- **New DLL imports from SDL:** IMM32, SETUPAPI, VERSION and WINMM, all system DLLs. The
  build is static, so `sa-deploy` copies nothing new for SDL.

## The seven "(verify)" SDL3 names, against `build/third_party/install/include/SDL3/`

Checked on 3.2.30 (line numbers below) and again on 3.4.18: every name and signature is
unchanged; only the line numbers moved.

| name | pinned header | finding |
|---|---|---|
| `SDL_SetAudioStreamGain` | SDL_audio.h:1222 | `bool (SDL_AudioStream*, float gain)`, as the plan says |
| `SDL_SetWindowMouseRect` | SDL_video.h:2543 | `bool (SDL_Window*, const SDL_Rect*)` |
| `SDL_GetWindowBordersSize` | SDL_video.h:1899 | `bool (SDL_Window*, int* top, int* left, int* bottom, int* right)`: the order is top, left, bottom, right |
| `SDL_SyncWindow` | SDL_video.h:2266 | `bool (SDL_Window*)` |
| `SDL_AddEventWatch` callback | SDL_events.h:1379 / 1471 | `SDL_EventFilter` = `bool (*)(void* userdata, SDL_Event*)`; the doc says it "may run in a different thread" |
| `SDL_PROP_WINDOW_WIN32_HWND_POINTER` | SDL_video.h:1563 | `"SDL.window.win32.hwnd"` |
| `SDL_SetAudioStreamGetCallback` | SDL_audio.h:1711 / 1757 | callback is `void (void*, SDL_AudioStream*, int additional_amount, int total_amount)`. Setting it "obtains the stream's lock, which means any existing callback ... in progress will finish running before setting the new callback"; NULL turns it off. That confirms the plan's Unbind, then NULL callback, then Destroy order |

Also checked:

- **Wheel fields:** `SDL_MouseWheelEvent::integer_y` is present ("added in 3.2.12"), and so is
  `SDL_MOUSEWHEEL_FLIPPED`.
- **DPI hint (plan risk 4):** there is NO `SDL_HINT_WINDOWS_DPI_AWARENESS` define in 3.2.30 or 3.4.18.
  `WIN_InitDPIAwareness` reads the undocumented hint string `"SDL_WINDOWS_DPI_AWARENESS"`, with
  values `permonitorv2` (the default when unset), `permonitor`, `system` and `unaware`. W3
  must set it by that string to keep retail's awareness.

## Deviations from the plan and the brief

1. **SDL_main.h is included in B0.2,** against the brief's "do not include SDL_main.h". With
   `#define SDL_MAIN_HANDLED` before it, the pinned header neither `#define`s `main` nor
   includes `SDL_main_impl.h`, so it cannot touch our WinMain. `nm` shows one WinMain, ours. It
   is the only header that declares `SDL_SetMainReady`. The alternative is a hand-written
   extern of a library function, which is the local-extern pattern CLAUDE.md forbids. The
   brief's purpose, keeping our WinMain, holds. Re-checked on 3.4.18: the same guards
   (`SDL_main.h:147`, `:263`, `:696`).
2. **`SDL_HINT_TIMER_RESOLUTION = "0"` (OVERRIDE) before `SDL_Init(0)`.** This was not in the
   plan. By default `SDL_Init(0)` calls `timeBeginPeriod(1)` through `SDL_InitTicks`
   (`src/timer/SDL_timer.c:549-597` in 3.2.30, `:571-605` in 3.4.18). That is a process-wide clock change the tree avoids on
   purpose: `frame_limiter.cpp` uses a high-resolution waitable timer instead. At "0" SDL makes
   no call.
3. **W1: AdjustDisplay's GetWindowPlacement now runs on a fully zeroed WINDOWPLACEMENT,**
   because GetShowCmd is shared with MoveWindow and game_main. Before, only `length` was set.
   The two differ only if the call fails, where showCmd reads garbage before and 0 now.
4. **W1: game_main's shutdown log format went from `%ldx%ld` to `%dx%d`.** The operands are
   int32 now, and the printed values are the same.
5. **W2: `PostQuitMessage` became a primitive too.** WindowProc's WM_DESTROY arm calls it, and
   the engine file must not include `<windows.h>`.
6. **W2: ids named at the registration sites as well,** in `device_startup.cpp` and
   `devicedata.cpp`. Those are no-op substitutions, B11 IDENTICAL. `input_platform_startup_win.cpp`
   already spells `WM_*` and is left for SDL step I1.

## Open items for the coordinator

- **W3 has to answer four things:**
  - the DPI hint string above;
  - SDL video init's `WIN_CoInitialize` + `OleInitialize`, since `Engine::Startup` already ran
    `CoInitializeEx(MULTITHREADED)`, so OleInitialize's STA request will fail with
    RPC_E_CHANGED_MODE (SDL tolerates the failure; check that nothing in SDL needs OLE: drag-drop);
  - `SDL_INIT_VIDEO` / `SDL_INIT_EVENTS`, which B0.2's `SDL_Init(0)` leaves to W3;
  - `DefaultWindowProc`, which W3 deletes because SDL owns the real WndProc.
- **The three W1 callers** (`device_startup_win.cpp`, `device_adjustdisplay_win.cpp`,
  `input_win.cpp`) no longer include `<windows.h>` but still live in `src/platform/win/` under
  `_win` names. Moving them to `src/engine/video` / `src/engine/input` is a rename commit,
  RENAMED-IDENTICAL.
- **RENDER step 3** (the window / cursor sites in `devicedata.cpp`) can build on
  `platform_window.h` now (C5). `TraceGeom` / `DeviceData_Resize` still call `GetClientRect` /
  `GetWindowRect` directly.
- **`libwinpthread-1.dll` (not SDL's):** the perf exe imports it, and so does main's own
  SDL-free perf exe (`build/perf` in the main checkout). It arrived with THREADS-1, not this
  block. The game directory has no copy, so a launch outside an MSYS2 PATH may fail to start.
- **Merge commit attribution:** `967aabf59` carries git's default message, without the
  attribution lines (no amend allowed).

## Shared files touched

- `Makefile`: the `third_party` rule, `SA_INCLUDES` / `SA_LIBS`, the stamp prerequisites, and
  `src/platform/sdl/*.cpp` in `RUNTIME_FUNC_SRCS`, which was merged keep-both with main's
  `src/platform/common/*.cpp`.
- `CMakeLists.txt`: the SDL block, `SDL3::SDL3-static`, and `src/platform/sdl/*.cpp` in
  `PLATFORM_SRCS`, merged keep-both.
- `.gitmodules` and `third_party/SDL`: new.
- `docs/reference/environment.md`: the SDL entry.
- `src/app/main.cpp`: the SDL includes, plus `SDL_SetMainReady` / hint / `SDL_Init(0)` before
  `Init()` and `SDL_Quit()` on both returns.
- `src/game/Game/game_main.cpp`: the shutdown window log, lines 141-157.
- `src/engine/application/Application.h`: one comment.
- `src/engine/video/device_startup.cpp` and `src/render/dx11/devicedata.cpp`: named message ids
  plus one include each.
- `src/platform/win/{device_startup_win, device_adjustdisplay_win, input_win, platform_startup_win}.cpp`.
- New files: `src/platform/common/platform_window.h`, `src/platform/win/window_win.cpp`,
  `src/engine/application/window_proc.cpp`.
