# Phase D.4 plan: the SDL seam (window, pump, display, input, audio)

Written 2026-10-10 by a read-only survey of main at 4e36b7000. Decision source: `docs/history/endgame_b/NEXT_SESSION.md` §2.2,
`ENDGAME.md` §E item 3. Nothing here was built or run; every tree fact is a file:line read. SDL3 / DXVK facts were
checked online the same day against the SDL wiki, SDL's `main` source and the DXVK repo; each carries a source key
[S1]..[S23] resolved in §H. The few still unconfirmed are marked **(unverified)**.

## 0. Three corrections to the brief (they change the work)

1. **Input is DirectInput 8, not raw Win32 input.** `input_platform_startup_win.cpp:240-280` creates a DI8
   keyboard + mouse (`DISCL_FOREGROUND|DISCL_NONEXCLUSIVE`). A second "legacy" path (`Input::Configuration::_legacy`,
   persisted `Input/Legacy`, default 0) polls `GetAsyncKeyState`. Both paths must be ported.
2. **Every shipped sound is xWMA** (WAVE_FORMAT_WMAUDIO2). The modkit compiles every `.wav` through
   `ToolKit/bin/xWMAEncode.exe` (ToolKit/README.html). `PlayPlatform` submits an `XAUDIO2_BUFFER_WMA` seek table
   (`audio_play_platform.cpp:368`). The music stream is 16 xWMA packets of 0x8b6 bytes per buffer
   (`vtbatch7b_stream_run_win.cpp:50-60`). **SDL3 has no WMA decoder**: `SDL_AudioFormat` is PCM only and
   `SDL_LoadWAV` takes PCM, float, MS/IMA ADPCM, A-law and mu-law [S17]. That is the top risk (§G1).
3. **The pump exists twice.** `Application::PlatformUpdate` (`application_win.cpp:100-112`) and an inlined copy in
   `MainLoop::Frame` (`src/engine/engine/mainloop_frame.cpp:254-272` declares `PeekMessageW` etc. by hand, `:411-430`
   pumps). Row 1211 is `Debug::ForceDump`; the `EnumerateVideoModes` swallow is row **1210** and stays a renderer
   (DXGI) matter, untouched by SDL.

## A. Inventory

### A.1 Window (creation, show, destroy, native handle)

| Win32 | file:line | SDL3 | semantic difference |
|---|---|---|---|
| `RegisterClassW` + `CreateWindowExW(0,"Banished","Banished",style 0,CW_USEDEFAULT,10x10)` | `platform_startup_win.cpp:273,277` | `SDL_Init(SDL_INIT_VIDEO)`; `SDL_CreateWindow("Banished",10,10,SDL_WINDOW_HIDDEN)` | the class/register failure split becomes one failure; keep both `Error::SetText` strings, the create one on `SDL_CreateWindow`==null |
| `UpdateWindow`; `SetCursor(wc.hCursor=NULL)` | `:295-296` | none; `SDL_HideCursor()` is NOT equivalent | retail hides the OS arrow until slot 8 builds the game cursor; do the same by setting no cursor until `CreateCursor` (below) |
| `DestroyWindow` (handle not nulled) | `:364` | `SDL_DestroyWindow` | keep "not nulled" |
| `ShowWindow(SW_SHOWDEFAULT)` | `devicedata.cpp:1168`, deferred `boot_window.cpp:64` | `SDL_ShowWindow` | SW_SHOWDEFAULT honours STARTUPINFO (`main.cpp:73`); SDL ignores it. Record, do not emulate |
| HWND into swap chain | `devicedata.cpp:293 WindowOf`, `:481 OutputWindow`, `:1226 MakeWindowAssociation` | `SDL_GetPointerProperty(SDL_GetWindowProperties(w), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL)` [S6] (main thread only) | none on Windows; under dxvk-native "`HWND`s can become `SDL_Window*`s" [S20] (§C) |
| `Application::_windowHandle` (+0x00) / `Video::Device::windowHandle` (+0x90) | `Application.h:196`, `device_startup.cpp:411` | holds the **renderer's native handle** | readers other than the renderer go through `Platform::Window` (§C) |
| `CreateCursor` 32x32 AND/XOR, `SetCursor` | `devicedata.cpp:1344-1348` | `SDL_CreateCursor(data,mask,32,32,hx,hy)` + `SDL_SetCursor` [S9] | SDL: data0/mask1 white, data1/mask1 black, data0/mask0 transparent, data1/mask0 inverted; MSB-first, width a multiple of 8 [S9]. Win32 AND=1 → mask 0; XOR=1 (white) → data 0. Unit-test the conversion |
| `DestroyCursor` (slot 3) | `devicedata.cpp` slot 3 | `SDL_DestroyCursor` | none |
| `MessageBoxW` (Error::Display) / `MessageBoxA` | `error.cpp:533`, `main.cpp:1352` | `SDL_ShowSimpleMessageBox(flags,title,msg,window)` [S10] (blocks; callable before `SDL_Init`) | the watchdog detects modals by class `#32770` (`boot_state_watchdog.cpp:120-135`). SDL3 tries `TaskDialogIndirect` first and falls back to a `DialogBoxIndirectParam` template with the default dialog class [S11]. Whether a TaskDialog's top window is class `#32770` is **(unverified)**: W6 forces a modal and checks `boot_state.sh` reports it |
| `LaunchProcess` (zero callers in the tree; ported per "decompile all slots") | `application_win.cpp:318-368` | see §A.6 | see §A.6 |

### A.2 Pump and window procedure

`WindowProc` (`platform_startup_win.cpp:147-198`) walks `Application::_messageCallbacks`, an AVL Set keyed by the
**Win32 message id** (one callback per id), then a switch: WM_DESTROY posts quit, WM_SETFOCUS/KILLFOCUS write
`_running` (+0x11, read by `IsPaused`, `application.cpp:36`) and fall through, WM_ERASEBKGND swallowed, WM_SYSKEYDOWN
swallowed for VK_F10. Registered callbacks (the complete set):

| id | callback | registered at | its Win32 calls | SDL3 event that synthesises it |
|---|---|---|---|---|
| 5 WM_SIZE | `Video::Device::SizeWindow` | `device_startup.cpp:457` | `GetClientRect` (`device_startup_win.cpp:60`) | `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` (and RESIZED) |
| 0x232 WM_EXITSIZEMOVE | `Video::Device::MoveWindow` | `device_startup.cpp:458` | `GetWindowPlacement`, `ClientToScreen` (`device_startup_win.cpp:94-130`) | `SDL_EVENT_WINDOW_MOVED`: fires per move, not once at the end; MoveWindow only stores the final client origin, so the end state matches |
| 7 / 8 SET/KILLFOCUS | `Input::Device::OnFocusMessage` / `OnLostFocusMessage` (return 0, so the switch still runs) | `input_platform_startup_win.cpp:288-289` | none | `SDL_EVENT_WINDOW_FOCUS_GAINED` / `_LOST` |
| 0x20A WM_MOUSEWHEEL | `OnMouseWheelMessage` stores the sign only | `:290` | none | `SDL_EVENT_MOUSE_WHEEL`: wParam = `(int16)(sign*120) << 16`, sign of `y` after `direction` flip |
| 0x20 WM_SETCURSOR | `DeviceData_OnSetCursor` (`SetCursor`+`ShowCursor(TRUE)`) | `devicedata.cpp:1267` | user32 | `SDL_EVENT_MOUSE_MOTION` inside the window (Win32 sends SETCURSOR per move); also called directly by `EnableCursor` (`application_win.cpp:180-235`) |
| 6 WM_ACTIVATE | `DeviceData_OnActivate` (DXGI fullscreen drop/restore) | `devicedata.cpp:1268` | none | synthesised **before** 7/8 with wParam 1 / 0 (Win32 order: ACTIVATE then SET/KILLFOCUS; confirm with main.cpp's `[WndMsg]` logger before the switch) |
| WM_CLOSE→WM_DESTROY→WM_QUIT | switch + pump | `:181`, `application_win.cpp:109`, `mainloop_frame.cpp:424` | | `SDL_EVENT_WINDOW_CLOSE_REQUESTED` and `SDL_EVENT_QUIT` → the WM_QUIT result (`_quit = 1`) |

Pump calls: `PostQuitMessage` when `_quitRequested` (`application_win.cpp:101`, `mainloop_frame.cpp:412`,
`mainmenudialog.cpp:710` fallback), `PeekMessageW/Translate/Dispatch` loops, `SwitchToThread`
(`mainloop_frame.cpp:490`, `application_win.cpp:142,159`; threads seam). SDL3: `SDL_PumpEvents` + `SDL_PollEvent`
loop; quit request = a pushed `SDL_EVENT_QUIT` (`SDL_PushEvent`). The diag `WH_CALLWNDPROC` hook (`main.cpp:914-1100`:
black-brush swap, `[WndMsg]` logger, `ForceVideoContextReset` on refocus) becomes a pre-dispatch observer called by
our own pump loop on the main thread. Not `SDL_AddEventWatch`: its filter `bool (*)(void*, SDL_Event*)` "may run in
a different thread" [S7], and `ForceVideoContextReset` touches the render context. The brush swap disappears (SDL
paints nothing; record it as gone).

### A.3 Display / geometry (`device_adjustdisplay_win.cpp:80-195`, `device_startup_win.cpp:140-165`)

| Win32 | SDL3 | difference |
|---|---|---|
| `Get/SetWindowLongW(GWL_STYLE)`: fullscreen `&0xFF30FFFF|WS_POPUP`, windowed `|0x00CF0000` (WS_OVERLAPPEDWINDOW) | `SDL_SetWindowBordered`, `SDL_SetWindowResizable` | style bits not otherwise read; keep the two states only |
| `GetMonitorInfoW(dev->monitor)` (HMONITOR from DXGI, `devicedata.cpp:1179`) | `SDL_GetDisplayBounds(id)`, id from the DXGI output's `DesktopCoordinates` centre via `SDL_GetDisplayForPoint` | the monitor stays the **DXGI output's**, not the window's display |
| `GetWindowPlacement` showCmd maximised / minimised | `SDL_GetWindowFlags & SDL_WINDOW_MAXIMIZED / _MINIMIZED` | none |
| `AdjustWindowRect` + `SetWindowPos` of the OUTER rect at (windowX, windowY) | `SDL_GetWindowBordersSize(w,&top,&left,&bottom,&right)` [S12] + `SDL_SetWindowPosition/Size` | SDL's x/y/w/h are the CLIENT area: SDL itself runs `AdjustWindowRectEx(ForDpi)` before `SetWindowPos` [S13]. Reproduce retail's outer-rect arithmetic from the border sizes, or retail's saved-position drift (client origin saved, reused as outer origin) disappears. `GetWindowBordersSize` "may fail … immediately after SDL_CreateWindow" and then reports 0 [S12]: call `SDL_SyncWindow` [S14] after making it bordered, and on Windows compute the borders with `AdjustWindowRectEx` on the HWND's style if it still fails |
| `MonitorFromRect(probe, DEFAULTTONULL)` (probe.bottom uses offX: retail bug, keep) | loop `SDL_GetDisplays` + `SDL_GetRectIntersection` | `SDL_GetDisplayForRect` returns the display "entirely containing the rect or closest to the center", never 0 for an off-screen rect [S15]: do not use it |
| `GetWindowRect(GetDesktopWindow())` | `SDL_GetDisplayBounds(SDL_GetPrimaryDisplay())` | none |
| `GetClientRect` read-back | `SDL_GetWindowSizeInPixels` | DPI: see §G4 |
| `EnumDisplaySettingsW(ENUM_CURRENT)` (GetDefaultResolution `:161`, frame limiter `frame_limiter.cpp:53`) | `SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay())` (w, h, `refresh_rate` float) | none |
| DXGI mode list, `SetFullscreenState`, `ResizeTarget` (`devicedata.cpp:833-851,1589-1640`) | **unchanged** (DXGI, DXVK on non-Windows) | renderer plan owns it |

### A.4 Input (`src/engine/input/input_device_update.cpp`, `src/platform/win/input_*`)

Per tick, `MainLoop::Frame` calls `Input::Device::Update` (`mainloop_frame.cpp:383`) **before** the pump (`:419`).
Update zeroes a 0x69-key and a 0x0D-mouse scratch, calls `PlatformGetKeyboardState` / `PlatformGetMouseState`, then
`UpdateState` (edges, repeat at 0.3 s / 0.25 s, `:185-192`). Text entry has **no OS text path**: `EditElement`
reads `_keyboard[k].pressed/repeat` and `SelectCharacter` maps a key index to a US-layout char with shift from keys
61/62/66 (`editelement.cpp:280-376`, `element_bodies_b5.cpp:848`). So SDL text input must NOT be turned on. In SDL3
it is off by default and per window, `SDL_StartTextInput(SDL_Window*)` [S18], so the rule is "never call it". Key
repeat stays the engine's own timer, never OS repeat.

| Win32 / DI | file:line | SDL3 | difference |
|---|---|---|---|
| DI keyboard `GetDeviceState(256)` → `kScanCodeToIndex[256]` | `:437-453`, table `:231` | build a 256-byte **DIK buffer** from `SDL_GetKeyboardState` (`const bool*`, indexed by `SDL_Scancode`, updated by `SDL_PumpEvents` [S19]) through a lifted `SDL_Scancode→DIK` table, then run the unchanged unpack loop | keeps retail's index-104 quirk (155 unmapped codes alias index 104) byte-for-byte. Both are physical scan codes, layout-independent |
| DI failure (not foreground) → Acquire loop, buffer left zero (releases all keys) | `:439-450` | `SDL_GetKeyboardFocus() != window` → leave the buffer zero, return true | identical outcome |
| legacy `GetForegroundWindow()==hwnd` + `GetAsyncKeyState(kIndexToVirtualKey[i])` | `:418-431`, table `:206` | focus test as above; VK → `SDL_GetScancodeFromKey(vk-as-keycode)` | VK is layout-dependent; SDL keycodes are too. OEM VKs (0xBA..0xDF) need an explicit table |
| DI mouse `GetDeviceState(20)`: `lX`, `-lY` (16-bit), `lZ` sign, 8 buttons | `:556-580` | deltas `SDL_GetRelativeMouseState(float*,float*)` [S21]; buttons `SDL_GetMouseState` mask via `SDL_BUTTON_MASK` (5 buttons; 5-7 stay 0) [S21]; wheel = sign of the tick's summed `SDL_MouseWheelEvent::y`, negated when `direction == SDL_MOUSEWHEEL_FLIPPED` (`integer_y` exists only since 3.2.12) [S21] | **DI deltas are raw mickeys and keep coming at the screen edge.** Outside relative mode SDL3 on Windows sends WM_MOUSEMOVE as ABSOLUTE client coords (`SDL_SendMouseMotion(…, false, x, y)`), so `xrel` is a cursor-position difference: accelerated, stops at the edge [S22]. Raw input reaches SDL only in relative mode, which hides the cursor and confines it [S21]; no hint enables raw motion outside it [S21]. Feeds middle-drag yaw (`camera_input.cpp:134-136`). Measure (§F step I3), then §G3 |
| `GetCursorPos`+`ScreenToClient` (both paths), 16-bit wrap | `:517-518,585-586`, startup `input_platform_startup_win.cpp:299-300` | `SDL_GetGlobalMouseState` (desktop coords, queried live from the OS [S21]) minus the client origin, truncated to int | positions outside the window must stay negative / beyond size, as retail's |
| `GetSystemMetrics(SM_SWAPBUTTON)` swap of buttons 0/1 | `:500` | DI reports PHYSICAL buttons. SDL's are LOGICAL: "WM_LBUTTONDOWN and friends handle button swapping for us", and its raw path swaps with `WIN_SwapButtons()` [S22] | the SDL backend must NOT swap again: keep the call site, make the platform query report "no swap needed" under SDL. Test with swap on |
| `ClipCursor(client rect)` / `ClipCursor(NULL)` | `input_win.cpp:118-140` | `SDL_SetWindowMouseRect(w, &rect)` / `(w, NULL)`, rect in window-relative coords, active only while the window has mouse focus [S8] | retail re-clips/unclips on each focus change in PlatformUpdate; SDL's confinement follows mouse focus by itself: same end state |
| `DirectInput8Create`, `CreateDevice`, `SetDataFormat`, `SetCooperativeLevel`, `Acquire`, `Unacquire`, `Release` | `input_platform_startup_win.cpp:240-309`, `input_platform_shutdown_win.cpp:246,257` | none (SDL owns devices); fields `_directInput` +0x11d8, `_keyboardDevice` +0x11e0, `_mouseDevice` become opaque, null under SDL | the seven HRESULT error paths become unreachable: keep the `Error::SetText` strings in the Win32 backend only |
| `GetAsyncKeyState(VK_L/RSHIFT)` (shift-at-launch → launcher) | `engine_platform_win.cpp:65` | `SDL_GetModState() & SDL_KMOD_SHIFT` after the first pump | belongs to the settings seam (launcher dialog becomes file entries); listed for completeness |

Harness: the menu-driver clicks by calling dialog event handlers (`autostart.cpp:125 InjectClick`), the mouse route
writes `Input::Device` state between the poll and `UI::Update` (`mouse_inject.h`), the modal driver closes in-game
dialogs (`modal_driver.h`). **None touches Win32 input**; all three survive unchanged.

### A.5 Audio (XAudio 2.7 via COM; 13 files carry the spelling)

Object graph: `Audio::Device` (0x1298, `Device.h`) owns `_xaudio2` (+0xa88, `IXAudio27*`), `_masteringVoice`
(+0xa90), 64 `AudioChannel` (+0x0) and 64 `PlatformChannel` (+0xa98; its vtable IS `IXAudio2VoiceCallback`),
and a by-value `Audio::Stream` (+0xa00, a `System::Thread` worker with Win32 events / CRITICAL_SECTION).

| XAudio2 | file:line | SDL3 | difference |
|---|---|---|---|
| `CoCreateInstance(CLSID_XAudio2 2.7)` + `Initialize`; failure → silent, returns true | `audio_platform_startup_win.cpp:96-145` | `SDL_InitSubSystem(SDL_INIT_AUDIO)` + `SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL)` (not paused at open, unlike `SDL_OpenAudioDeviceStream` [S16]); optional `SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES` = 480 for XAudio2's 10 ms quantum, a request many platforms ignore [S16]; failure → null engine, return true | XAudio 2.7 needs the DirectX June 2010 runtime; today a machine without it is silently mute. SDL removes that dependency |
| `CreateMasteringVoice(defaults)` | `:135` | the opened device id | none |
| `CreateSourceVoice(fmt, maxRatio 2.0, callback=record)` | `audio_play_platform.cpp` PlayPlatform | `SDL_CreateAudioStream(&srcSpec, NULL)` + `SDL_BindAudioStream(dev, s)`: "data for all bound streams will be mixed together", no documented stream limit, binding an already-bound stream is an error [S16] | srcSpec from the WAVEFORMATEX at SoundBuffer+0x54 **after decode** (xWMA → S16 PCM, §G1) |
| `SubmitSourceBuffer(buf, wma)` with `PlayBegin`, `LoopCount` infinite / 0, END_OF_STREAM | same | `SDL_PutAudioStreamData` of decoded PCM from the PlayBegin packet; loop = re-put from a get-callback (`SDL_SetAudioStreamGetCallback`) | PlayBegin is a packet-aligned seek through the WMA cumulative table; decode from that packet |
| `SetVolume(v)` | PlayPlatform; `audio_scene_update.cpp:538` (a non-`_win` file calls the COM method) | `SDL_SetAudioStreamGain(s, float)` (since 3.2.0; 1.0 unchanged, "larger gain means a louder output", no stated cap; applied in `SDL_GetAudioStreamData`, takes the stream lock) [S16] | XAudio2 ramps at the next 10 ms pass; SDL applies at the next pull. Route Scene's call through `Device::SetVolumePlatform` |
| `SetFrequencyRatio(2^(pitch/12))` | PlayPlatform | `SDL_SetAudioStreamFrequencyRatio` ("between 0.01 and 100", changes speed and pitch) [S16] | none in range (retail caps 2.0) |
| `Start(0)` | PlayPlatform | `SDL_ResumeAudioDevice` once at startup; per-voice: bind = start | none |
| `Stop` / `DestroyVoice` | `audio_device.cpp:163,283`, `audio.cpp:781` | `SDL_UnbindAudioStream` + `SDL_DestroyAudioStream` (Destroy unbinds and frees queued audio; "Do not call SDL_DestroyAudioStream() on `stream` during this callback") [S16] | DestroyVoice blocks until the callback thread is out of that voice. SDL documents no such wait for Destroy **(unverified)**; Unbind first, then `SDL_SetAudioStreamGetCallback(s, NULL, NULL)`, which "waits for a running one to finish" [S16], then Destroy |
| callbacks `OnStreamEnd` → `flags |= 2`; `OnBufferEnd` → `SetEvent(bufferEvent)` | `audio_voice_callback_win.cpp:201-218` | the get-callback `void (void*, SDL_AudioStream*, int additional, int total)` [S16] calls the SAME `PlatformChannel` virtuals when consumed bytes cross a buffer boundary / reach 0 after the last put. Use `SDL_GetAudioStreamAvailable` for output left, not `SDL_GetAudioStreamQueued`, which counts INPUT bytes [S16]; `SDL_FlushAudioStream` marks the final put [S16] | "may run from any thread", stream lock held during it [S16]: the same model as XAudio2's engine thread. The non-atomic `flags |= 2` race is retail's, keep it |
| `GetState().BuffersQueued` (vs 3 and 0) | `vtbatch7b_stream_run_win.cpp` | a per-voice count of put-but-unconsumed buffers kept by the backend | none |
| mastering voice `DestroyVoice` + engine `Release`, inlined in two shutdowns | `game_main.cpp:198-208`, `engine_lifecycle.cpp:735-741` | one `Device::PlatformReleaseEngine()` called from both sites | the fields keep their offsets |
| reap loop (`flags & 2` → FreeChannelPlatform → onComplete) | `mainloop_frame.cpp:384-396` | unchanged | none |
| Stream worker's Win32 events + CRITICAL_SECTION: wake event Stream+0x40, buffer event +0x48, lock +0x50 (Device +0xa40/+0xa48/+0xa50, asserted in `Device.h`); `WaitForMultipleObjects(2, …, FALSE, INFINITE)` | `vtbatch7b_stream_run_win.cpp:344,425`, `audio_stream_win.cpp`, `audio_stream_play_win.cpp`, `audio_device.cpp` (FreeChannelPlatform / StopPlatform) | **the THREADS plan's primitives, not SDL's**: `Platform::EventHandle` (8-byte slot, auto/manual reset) with `EventSet/Reset/Wait/WaitAny(2, …)` (lowest index wins, consumes only that one, as Win32's auto-reset WaitAny), and `Platform::LockStorage` (the same in-place 0x28 bytes, recursive) (`phase_d/plans/THREADS.md`) | none: the layout asserts stay. `OnBufferEnd` calls `EventSet` from the SDL audio thread where it called `SetEvent` from XAudio2's |

### A.6 Handed over by the THREADS plan

| Win32 | file:line | replacement | difference |
|---|---|---|---|
| `CoInitializeEx(NULL, 0 = MTA)` in `Engine::Startup` (inlined) and `Engine::PlatformStartup` (unreachable export) | `engine_lifecycle_win.cpp:29-35`, `platform_startup_win.cpp:424` | keep on Windows behind `Platform::ComInit()` / `ComUninit()` (the names THREADS.md uses), no-ops elsewhere. Its three consumers: XAudio2's `CoCreateInstance` and `DirectInput8Create` go with Blocks 4 / 2. `ShellExecuteW` becomes `SDL_OpenURL`, which does its own `WIN_CoInitialize` / `CoUninitialize` around `ShellExecuteW` [S23] | after Block 5 nothing in the tree needs COM. Delete the init then, in its own `behaviour:` commit, because retail's MTA init on the main thread is observable by any COM user SDL loads. Whether SDL's STA attempt on an MTA thread tolerates `RPC_E_CHANGED_MODE` is **(unverified)**; test LaunchProcess's open arm once |
| `ShellExecuteW(NULL, L"open", file, NULL, NULL, 10 = SW_SHOWDEFAULT)`, success = result > 32 | `application_win.cpp:358` | `SDL_OpenURL(url)`. It takes `file:///` URLs "if supported" and directories may open a file manager; main thread only [S23] | SDL passes `SW_SHOWNORMAL`, not SW_SHOWDEFAULT [S23]. A bare path must be turned into a `file:///` URL. Retail's result-code test becomes SDL's bool. Record in known_port_issues |
| `CreateProcessW(NULL, cmdline≤0x103 chars, …)` + `WaitForSingleObject(INFINITE)`, `hThread` leaked | `application_win.cpp:321-349` | `SDL_CreateProcessWithProperties` with `SDL_PROP_PROCESS_CREATE_CMDLINE_STRING` ("passed directly to CreateProcess on Windows") [S23], then `SDL_WaitProcess(p, true, &code)` + `SDL_DestroyProcess` | the raw command line survives only on Windows. Elsewhere it must be split into `SDL_PROP_PROCESS_CREATE_ARGS_POINTER`, the required property [S23]. The 0x104 truncation stays in our code. The handle leak goes; SDL owns its handles |
| `EnumDisplaySettingsW(ENUM_CURRENT)` refresh rate | `frame_limiter.cpp:53` | `SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay())->refresh_rate` (already in §A.3; ours, not retail's) | float Hz, 0 when unknown: keep the `<= 1 → 60` fallback |

## B. Vendoring and build

- **Recommendation: SDL3 as a git submodule** at `third_party/SDL`, pinned to **`release-3.4.18`**, the current
  "Latest" stable release, published 2026-10-02 [S1]. Record the tag and commit in `docs/reference/environment.md`.
  A submodule keeps SDL's source out of this repo's object store; add `.gitmodules` with `shallow = true`.
- **Toolchain fit:** SDL needs `cmake_minimum_required(VERSION 3.16...4.4)` [S2], and our CMake is 4.4. Its minimum
  tested MinGW-w64 is 8.0.3 [S4], and ours is newer (GCC 13.1 ucrt64). No blocker is known, but none has been
  measured; B0.1 is the measurement.
- **Static link, one exe.** SDL is zlib-licensed. By default only the shared library is built [S3], so ask for static:
  `cmake -S third_party/SDL -B build/third_party/sdl3 -G Ninja -DCMAKE_BUILD_TYPE=Release -DSDL_SHARED=OFF -DSDL_STATIC=ON
  -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_GPU=OFF -DSDL_RENDER=OFF -DSDL_CAMERA=OFF
  -DSDL_JOYSTICK=OFF -DSDL_HAPTIC=OFF -DSDL_HIDAPI=OFF -DSDL_SENSOR=OFF -DSDL_POWER=OFF -DSDL_DIALOG=OFF -DSDL_TRAY=OFF
  -DSDL_NOTIFICATION=OFF -DCMAKE_INSTALL_PREFIX=build/third_party/install` then `cmake --build … --target install`.
  Every option name exists on `main` [S2]. HAPTIC must be off whenever JOYSTICK is, or configure fails with
  "SDL_HAPTIC requires SDL_JOYSTICK" [S2]. Re-check the names against the 3.4.18 tag when pinning, because `main` is 3.5.0.
  One SDL build serves every flavour (debug/og/perf/release link the same Release lib).
- **CMake:** `add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)` with the options above set as cache vars before it;
  `target_link_libraries(OpenBA PRIVATE SDL3::SDL3-static)`. The alias exists (`add_library(SDL3::SDL3-static
  ALIAS SDL3-static)` [S2]), but README-cmake says neither target is "guaranteed to exist" [S3], so test `if(TARGET …)`.
- **Makefile:** a `third_party` rule with a stamp file `build/third_party/sdl3.stamp` running the cmake lines above;
  `SA_INCLUDES += -Ibuild/third_party/install/include`; `SA_LIBS += build/third_party/install/lib/libSDL3.a` plus the
  `Libs.private` of the installed `lib/pkgconfig/sdl3.pc`, read with `sed` at make time and never retyped. The source
  list is `kernel32 user32 gdi32 winmm imm32 ole32 oleaut32 version uuid advapi32 setupapi shell32 hid mincore`, plus
  `dinput8` only for the joystick [S2]. All of them are in `/c/msys64/ucrt64/lib` (checked locally: `libmincore.a`,
  `libhid.a`, `libsetupapi.a` …). The `.pc` also adds `-mwindows` [S2], which we already pass. `standalone*` depend on
  the stamp. Add `src/platform/sdl/*.cpp` to `RUNTIME_FUNC_SRCS` and the CMake glob. Drop `-ldinput8 -ldxguid` when
  input lands.
- **Entry point:** keep our `WinMain`. `#define SDL_MAIN_HANDLED` before any SDL include, and call
  `SDL_SetMainReady()` (`<SDL3/SDL_main.h>`, since 3.2.0, "circumvent failure of SDL_Init() when not using SDL_main")
  before `SDL_Init` [S5]. **`sa-deploy` copies nothing new** (static).
- **Fallback if SDL3 will not build under GCC 13.1 / ucrt64:** (1) the official `SDL3-devel-3.4.18-mingw.tar.gz`
  (5.05 MB) [S1]. Whether it carries a static `libSDL3.a` or only `libSDL3.dll.a` + `SDL3.dll` is **(unverified)**; if it
  is shared, `sa-deploy` copies `SDL3.dll`. (2) Cross-build in WSL with mingw-w64 and check the `.a` into
  `third_party/prebuilt/` (a few MB, build script beside it). Never `pacman -S`.
- **The xWMA decoder** (§G1) is a second vendored dependency with the same shape (submodule + one stamp rule).

## C. Platform API shape

- New `src/platform/platform_window.h`, `platform_input.h`, `platform_audio.h`: small free-function namespaces
  (`Platform::Window`, `Platform::Input`, `Platform::Audio`), no `<windows.h>`, no SDL headers. Engine files call
  these instead of user32 / DI / XAudio2. Implementations in **`src/platform/sdl/`**: `window_sdl.cpp`,
  `pump_sdl.cpp`, `display_sdl.cpp`, `input_sdl.cpp`, `audio_sdl.cpp`, `audio_stream_sdl.cpp`, `xwma_decode.cpp`.
- **The message-id vocabulary stays.** `Application::_messageCallbacks` (ABI-pinned, `Application.h`) keeps Win32
  ids as keys, and `WindowProc` keeps its walk + switch; it moves to `src/engine/application/` with the
  `DefWindowProcW` tail deleted (SDL owns the real WndProc). `pump_sdl.cpp` translates SDL events into
  `(msg, wParam, lParam)` tuples per the A.2 table and calls `WindowProc`. Every callback body keeps its signature;
  their Win32 calls (`GetClientRect`, `GetWindowPlacement`, `ClientToScreen`) become `Platform::Window` calls.
  The ids get named constants in `src/platform/platform_window.h` (`kMsgSize = 5`, …).
- **What stays in `src/platform/win/` after D.4:** nothing for window/input/audio. The Win32 backends are deleted
  once their SDL twin has passed the gate (Block 5). The threads/events/timer files, filesystem, registry
  persistence and `video_interfaces_win.cpp` (LoadLibrary of renderer plugins) are other seams.
- **The renderer's handle:** `Platform::Window::NativeHandle()` returns the HWND on Windows (window property
  above) and the `SDL_Window*` on dxvk-native. DXVK has an `sdl3` WSI next to `sdl2`, `glfw` and `win32`
  (`src/wsi/`) [S20]. Its README says "`HWND`s can become `SDL_Window*`s", and the backend is chosen at run time with
  `DXVK_WSI_DRIVER=SDL3` [S20]. v2.6.1's notes fix "various issues with the SDL3 backend" [S20], but the release that
  added it is **(unverified)**. Tell the RENDER planner. That handle is what `Application::_windowHandle` / `Video::Device::windowHandle`
  hold, so `devicedata.cpp` (`WindowOf`, `OutputWindow`, `MakeWindowAssociation`) is unchanged. The `SDL_Window*` lives
  in `window_sdl.cpp`; no engine field changes type. The `TraceGeom` / `SwapProbe` diagnostics that call
  `GetWindowRect` move to `Platform::Window` too, or stay Windows-only under `src/diag/`.

## D. Order

1. **Block 0, build:** vendor SDL3, link it, call `SDL_Init(0)` / `SDL_Quit` only. Gate: `sa-run` unchanged.
2. **Block 1, window + pump + display.** Must come first: input and audio both need the SDL window (keyboard
   focus, mouse rect) and the SDL event loop (wheel, focus). SDL then owns the WndProc, so DI's
   `SetCooperativeLevel(hwnd)` keeps working on the SDL-created HWND during the overlap.
3. **Block 2, input.** Depends on Block 1. DI still works in parallel until removed.
4. **Block 3, audio seam refactor** (XAudio2 behind `Platform::Audio`, no behaviour change) can run **in parallel
   with Blocks 1-2**: it touches only `src/engine/audio`, the audio `_win` files and two shutdown sites.
5. **Block 4, xWMA decoder + SDL audio backend.** Needs Block 0 and Block 3, plus the user's decoder decision (§G1).
6. **Block 5, delete the Win32 backends**, drop `-ldinput8 -ldxguid`, `<xaudio2.h>`, `<dinput.h>`.

Parallel: Block 3 alongside 1-2 (separate file sets; the shared files are only `Makefile`/`CMakeLists.txt`, merge
those first). At most two implementing agents at once (memory: game dir contention, -j2 each).

## E. Fidelity rules

1. **Tick order is retail's:** poll (`Input::Update`) → reap → pump → `Update` (`mainloop_frame.cpp:383-439`). DI and
   `GetAsyncKeyState` are LIVE at poll time; `SDL_GetKeyboardState` is only as fresh as the last pump. So
   `input_sdl.cpp` calls `SDL_PumpEvents()` at the top of the poll (it updates state and queues events; dispatch
   still happens at the pump site). Without it every key lands one tick late. As with retail's live polling, a key
   pressed and released between two pumps is never seen [S19].
2. **Key repeat** is `UpdateState`'s 0.3 s / 0.25 s, never OS repeat: ignore `SDL_KeyboardEvent::repeat`.
3. **Text input** is key-index + `SelectCharacter`'s US table. No `SDL_StartTextInput`, no `SDL_EVENT_TEXT_INPUT`.
4. **The index-104 alias** (`input_device_update.cpp:219-230`) is preserved by feeding the unchanged 256-entry loop.
5. **Focus:** WM_SETFOCUS/KILLFOCUS reach both the Input callback (returns 0) and the switch (`_running`); losing
   focus pauses (`IsPaused`) and releases all keys (zero buffer). WM_ACTIVATE precedes them.
6. **Wheel** is sign-only, consumed and cleared per poll on the legacy path; DI path sign of `lZ` per poll.
7. **16-bit wrapping deltas, Y negated** on both paths (`:524-528`, `:578`); keep the int16 casts.
8. **Frame limiter** (`frame_limiter.cpp`) is ours, not retail's: only its refresh-rate query moves to SDL; its
   waitable timer belongs to the timer seam.
9. **Window close** = the WM_QUIT result in the same tick, as retail's WM_CLOSE→WM_DESTROY→WM_QUIT chain.
10. **Audio:** completion still fires from the audio thread into `PlatformChannel` virtuals; the main thread reaps
    `flags & 2` (`mainloop_frame.cpp:384`). Do not add locking retail lacks. Failed device open = silent game.
11. **No guards (CLAUDE.md hard rule).** `OnFocusMessage` derefs `Device::_singleton` unguarded
    (`input_platform_startup_win.cpp:148`); `PlayPlatform` derefs the SoundBuffer; `WindowProc` derefs found nodes.
    If the SDL pump delivers a focus event before `Input::Device::PlatformStartup`, that is OUR ordering bug: retail
    registers the callbacks first, so drop events with no registered callback (as an empty Set does) rather than
    null-checking inside a callback. A guard is legitimate only where SDL genuinely differs, with a comment saying so.

## F. Steps (one commit each; `behaviour:` where a seam's behaviour moves)

Every step: `mingw32-make standalone-debug`, `mingw32-make unit`, `mingw32-make sa-run` + `tools/boot_state.sh --wait 300`,
`tools/resave_gate/resave_gate.sh Gate`; window/input steps also `sa-run-menu` and the menu-driver routes
(`.claude/skills/menu-driver/`: AUTOMENU_BUTTON=7 Mods route, a PAUSEMENU route, MODAL_ANSWER).

**Block 0**
- B0.1 `build: vendor SDL3 (submodule, pinned tag)` — `.gitmodules`, `third_party/SDL`, `Makefile`, `CMakeLists.txt`,
  `docs/reference/environment.md`.
- B0.2 `build: link SDL3 static, SDL_SetMainReady + SDL_Init(0)` — `src/app/main.cpp`. Unit: none. Gate.

**Block 1 (window, pump, display)**
- W1 `refactor: Platform::Window API with a Win32 implementation` — new `src/platform/platform_window.h`; move every
  Win32 call in `device_startup_win.cpp`, `device_adjustdisplay_win.cpp`, `input_win.cpp` geometry, `game_main.cpp:141-153`
  behind it. Pure move. Gate.
- W2 `refactor: WindowProc to src/engine/application, message ids named` — `platform_startup_win.cpp`, `Application.h`.
- W3 `behaviour: SDL window + pump` — `src/platform/sdl/window_sdl.cpp`, `pump_sdl.cpp`; `Application::PlatformStartup`,
  `PlatformShutdown`, `PlatformUpdate`, `mainloop_frame.cpp:254-272,411-430` call `Platform::Pump()`; quit → `SDL_PushEvent`.
  Unit: `tests/unit/sdl_event_translate_test.cpp` (SDL event → msg tuple table; wheel sign; ACTIVATE before SETFOCUS).
- W4 `behaviour: display and geometry over SDL` — `display_sdl.cpp`; AdjustDisplay / SizeWindow / MoveWindow /
  GetDefaultResolution / frame-limiter refresh. Unit: outer-rect arithmetic and the null-monitor probe on fake bounds.
  Check: windowed launch, `BANISHED_*` window size options, a resize, a minimise/restore, the saved X/Y across two runs.
- W5 `behaviour: cursor over SDL` — `devicedata.cpp:927-933,1344-1348`, `EnableCursor`. Unit:
  `tests/unit/cursor_planes_test.cpp` (AND/XOR → data/mask). Check the in-game cursor and hotspot.
- W6 `behaviour: message boxes and diag hook over SDL` — `error.cpp`, `main.cpp` (hook → event watch; brush swap deleted).
  Check `boot_state.sh` still reports a modal (force one via a missing data dir).

**Block 2 (input)**
- I1 `refactor: Platform::Input API, DI behind it` — `input_device_update.cpp` pollers call
  `Platform::Input::ReadKeyboard(uint8_t dik[256]) / ReadMouse(...)`; DI code moves wholesale. Gate.
- I2 `behaviour: SDL keyboard` — `input_sdl.cpp`, lifted `kSdlScancodeToDik` (generated by a script from SDL's
  `SDL_scancode.h` and the DIK list, asserted round-trip). Unit: `tests/unit/sdl_scancode_dik_test.cpp` (every index
  0..0x68 reachable; unmapped → 104 alias; legacy VK table). Check: build menu hotkeys, typing a town name/seed, Esc/Enter.
- I3 `behaviour: SDL mouse` — absolute, deltas, buttons, wheel, clip. **Before** committing, log DI `lX` vs SDL
  `xrel` side by side over the same middle-drag (both backends are present at this point) and record the ratio in the
  commit message; if they differ materially, ask the user (§G3). Check: middle-drag rotate, edge scroll, wheel zoom,
  left-handed swap ON and OFF, clipping option ON.
- I4 `behaviour: focus via SDL` — callbacks fire from the SDL pump; alt-tab in windowed and fullscreen.

**Block 3 (audio seam, parallel)**
- A1 `refactor: Platform::Audio API, XAudio2 behind it` — `Device.h` fields become opaque (`void*`, offsets kept,
  `static_assert`s kept, `tests/unit/audio_abi_test.cpp` passes), `audio_scene_update.cpp:538` →
  `SetVolumePlatform`, `game_main.cpp:198-208` and `engine_lifecycle.cpp:735-741` → `PlatformReleaseEngine()`. No `<xaudio2.h>`
  outside `src/platform/win/`. Gate with audio audible.
- A2 `diag: SoundBuffer format census` — one-shot count of `wFormatTag` / channels / rate over every SoundBuffer
  loaded in a new game + the music, printed to `standalone_debug.log` from an existing route (no new flag). Confirms §0.2.

**Block 4 (SDL audio)**
- A3 `build: vendor the xWMA decoder` (per §G1) + `xwma_decode.cpp` with a packet API (decode packet range → S16).
  Unit: decode a short xWMA fixture to PCM and compare against a reference produced once by XAudio2 on Windows
  (store the fixture under `tests/unit/data/`, a few KB).
- A4 `behaviour: SDL one-shot voices` — `audio_sdl.cpp`: PlayPlatform/StopPlatform/FreeChannelPlatform/SetVolumePlatform.
- A5 `behaviour: SDL music stream` — `audio_stream_sdl.cpp`; the Stream worker's GetState/Submit/OnBufferEnd contract,
  its waits and lock on THREADS' `Platform::EventHandle` / `WaitAny` / `LockStorage` (needs THREADS steps 1-3 landed: primitives, Thread, locks).
  Check: menu music, track change, in-game ambience, 64-voice saturation (a storm/fire route), quit with music playing.

- W7 `behaviour: LaunchProcess over SDL` — §A.6: `SDL_OpenURL` and `SDL_CreateProcessWithProperties`. It has no
  caller, so test it with a unit-level call from a diag route: open a `file:///` folder, and run `cmd /c exit 3` with wait.

**Block 5** — D1 `cleanup: delete Win32 window/input/audio backends` (`platform_startup_win.cpp`, `input_*_win.cpp`,
`audio_*_win.cpp`, `vtbatch7b_stream_run_win.cpp`'s XAudio2 half, `device_*_win.cpp`); `-ldinput8 -ldxguid` gone;
`docs/known_port_issues.md` rows for each recorded difference; `src/engine/input/README.md`, `src/engine/audio/README.md`.
D2 `behaviour: drop the COM init` once nothing needs it (§A.6).

About 21 commits in 6 blocks.

## G. Risks and user questions (only those that change the work)

1. **xWMA decoding (TOP RISK, user decision).** Options: (a) FFmpeg `libavcodec` built with only the `wmav2`
   decoder (`--disable-everything --enable-decoder=wmav2 --disable-asm`), static, LGPL-2.1 (dynamic linking or
   providing object files satisfies LGPL; a static exe needs the relink offer); (b) a standalone WMA decoder extracted
   from FFmpeg sources into `third_party/` and built by CMake (same licence, no FFmpeg build system); (c) transcode
   the shipped packages once to PCM / Ogg Vorbis at install time (smaller runtime, but changes assets and breaks
   mods that ship xWMA). Recommendation: (b), decode per packet at play time. **Ask: is an LGPL dependency acceptable,
   and is decoding at play time preferred over a transcode step?**
2. **SDL3 vs SDL2.** SDL3 everywhere listed (Windows, Linux, macOS, Android). No target needs SDL2 today; do not
   build a dual layer. **Confirm.**
3. **Mouse delta source (now confirmed from source).** Outside relative mode SDL3's `xrel` is the cursor-position
   difference [S22]: accelerated by the pointer-speed setting, and zero at the screen edge. DirectInput gives raw
   mickeys, so middle-drag rotation will feel different. Relative mode (raw, unscaled by default
   `SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE=0` [S21]) hides and confines the cursor [S21], so it could be enabled only
   while the middle button is held, which is a behaviour change of its own. Options: (a) accept and record;
   (b) relative mode during the middle drag; (c) keep a raw-input reader per platform. **I3 measures; then ask.**
4. **DPI.** SDL3 on Windows declares per-monitor-v2 DPI awareness by default. A string hint
   `"SDL_WINDOWS_DPI_AWARENESS"` takes `permonitorv2` (the default), `permonitor`, `system` or `unaware` [S13]. It has
   no `SDL_HINT_` macro, and the wiki page for that name is 404 [S13]. On Windows SDL coordinates "always" are physical
   pixels [S13]. Retail with no DPI manifest would be bitmap-stretched by Windows on a 150 % display, and the port
   would render at native size. Read the retail exe's manifest before W3, then set the hint to `unaware` or
   `system` to match, through `SDL_SetHint` before `SDL_Init`.
5. **Static linking.** Recommended (one exe, no deploy change). Only changes if the xWMA choice forces a DLL.
6. **Android** (not a question now): no HWND, no DXGI. The `Platform::*` API above has no Windows type in it, so it
   does not block Android; the renderer route is the open item (NEXT_SESSION §2.2).
7. **Still unverified:** TaskDialog's window class (§A.1); whether `SDL_DestroyAudioStream` waits for a running
   callback (§A.5, worked around); the contents of the mingw devel package (§B); which DXVK release added the SDL3
   WSI (§C). None blocks Block 0.

## H. Sources (fetched 2026-10-10)

| key | URL |
|---|---|
| S1 | https://github.com/libsdl-org/SDL/releases |
| S2 | https://raw.githubusercontent.com/libsdl-org/SDL/main/CMakeLists.txt |
| S3 | https://github.com/libsdl-org/SDL/blob/main/docs/README-cmake.md |
| S4 | https://github.com/libsdl-org/SDL/blob/main/docs/README-windows.md |
| S5 | https://wiki.libsdl.org/SDL3/SDL_SetMainReady |
| S6 | https://wiki.libsdl.org/SDL3/SDL_GetWindowProperties |
| S7 | https://wiki.libsdl.org/SDL3/SDL_AddEventWatch , https://wiki.libsdl.org/SDL3/SDL_EventFilter |
| S8 | https://wiki.libsdl.org/SDL3/SDL_SetWindowMouseRect |
| S9 | https://wiki.libsdl.org/SDL3/SDL_CreateCursor |
| S10 | https://wiki.libsdl.org/SDL3/SDL_ShowSimpleMessageBox |
| S11 | https://raw.githubusercontent.com/libsdl-org/SDL/main/src/video/windows/SDL_windowsmessagebox.c |
| S12 | https://wiki.libsdl.org/SDL3/SDL_GetWindowBordersSize |
| S13 | https://raw.githubusercontent.com/libsdl-org/SDL/main/src/video/windows/SDL_windowswindow.c , https://raw.githubusercontent.com/libsdl-org/SDL/main/src/video/windows/SDL_windowsvideo.c , https://wiki.libsdl.org/SDL3/README-highdpi , https://wiki.libsdl.org/SDL3/CategoryHints |
| S14 | https://wiki.libsdl.org/SDL3/SDL_SyncWindow |
| S15 | https://wiki.libsdl.org/SDL3/SDL_GetDisplayForRect |
| S16 | https://wiki.libsdl.org/SDL3/ pages SDL_SetAudioStreamGain, SDL_SetAudioStreamFrequencyRatio, SDL_SetAudioStreamGetCallback, SDL_AudioStreamCallback, SDL_OpenAudioDevice, SDL_OpenAudioDeviceStream, SDL_BindAudioStream, SDL_UnbindAudioStream, SDL_DestroyAudioStream, SDL_GetAudioStreamQueued, SDL_FlushAudioStream, SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES |
| S17 | https://wiki.libsdl.org/SDL3/SDL_AudioFormat , https://wiki.libsdl.org/SDL3/SDL_LoadWAV_IO |
| S18 | https://wiki.libsdl.org/SDL3/SDL_StartTextInput , https://wiki.libsdl.org/SDL3/SDL_TextInputEvent |
| S19 | https://wiki.libsdl.org/SDL3/SDL_GetKeyboardState |
| S20 | https://github.com/doitsujin/dxvk/blob/master/README.md , https://github.com/doitsujin/dxvk/tree/master/src/wsi , https://github.com/doitsujin/dxvk/releases |
| S21 | https://wiki.libsdl.org/SDL3/ pages SDL_GetRelativeMouseState, SDL_MouseMotionEvent, SDL_SetWindowRelativeMouseMode, SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, SDL_HINT_WINDOWS_RAW_MOUSE_NOLEGACY, SDL_MouseWheelEvent, SDL_GetGlobalMouseState, SDL_GetMouseState |
| S22 | https://github.com/libsdl-org/SDL/blob/main/src/video/windows/SDL_windowsevents.c |
| S23 | https://wiki.libsdl.org/SDL3/SDL_CreateProcessWithProperties , https://wiki.libsdl.org/SDL3/SDL_WaitProcess , https://wiki.libsdl.org/SDL3/SDL_OpenURL , https://raw.githubusercontent.com/libsdl-org/SDL/main/src/misc/windows/SDL_sysurl.c |
