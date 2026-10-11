# Phase D.4 block SDL-C: the cursor, input and LaunchProcess over SDL (W5, I1-I4, W7)

Branch `d4-sdlc`, base `0b11a0f85`. Main was merged at `b158f3b91`, which brought AUDIO-A
in; the merge was clean. The block covers SDL plan steps W5, I1, I2, I3, I4 and W7.

Every commit was gated statically: `standalone-debug`, `standalone-perf` and `unit` at
`-j2`, all exit 0. There are 0 new warnings against the main checkout's
`runs/warn_baseline_{debug,perf}.txt`, castmetric over `src` is 2, and `tools/abi_audit.py`
is OK with no baseline row changed. `tools/sdl_dik/gen_sdl_dik_tables.py --check` is OK.
**I never ran the game.** The only runs were the coordinator's two I3 measurement runs on
uncommitted builds.

## 1. Commits

| commit | title | unit | gate |
|---|---|---|---|
| `3847ca8bd` | behaviour: cursor over SDL (W5) | 975 / 0 | static |
| `cac6ce3b7` | refactor: Platform::Input API, DirectInput behind it (I1) | 975 / 0 | static + B11 |
| `0d9af5c0c` | behaviour: SDL keyboard (I2) | 981 / 0 | static |
| `f668c5672` | behaviour: LaunchProcess / OpenURL over SDL (W7) | 984 / 0 | static |
| `a5d17709a` | behaviour: SDL mouse, relative mode during the middle-button drag (I3) | 984 / 0 | static + measurement |
| `99037818a` | behaviour: focus via SDL, DirectInput gone (I4) | 984 / 0 | static |
| `b158f3b91` | Merge main into d4-sdlc (AUDIO-A) | 984 / 0 | static |

Each message quotes the retail call sites and says what SDL does differently.

**I1 B11.** Perf objects were compared against `runs/b11base_d4c/perf/obj` with
`b11pair.py` and an empty map, across all 957 objects. The I1 message says 937; 957 is the
correct count.
- `input_device_update.o`: PlatformGetKeyboardState and PlatformGetMouseState are DIFF in
  the accepted shape. Each COM vtable call or user32 import call became a call to its
  primitive, and the literals 0x100 / 0x14 / 0x17 moved into the primitive.
- `input_platform_shutdown_win.o`: PlatformShutdown is DIFF in the accepted shape.
- `input_platform_startup_win.o`: PlatformStartup is DIFF. The DirectInput block became
  one call to `CreateDevices`. That function's call list is the base block's exactly, in
  order: GetModuleHandleW, DirectInput8Create, then the +0x18 / +0x58 / +0x68 calls twice,
  with the literals 0x800 and 6.
- Everything else is IDENTICAL, including every includer of `Device.h`.

## 2. Deviations from the plan and the brief

1. **W5: the cursor handle is a backend slot, not a cast `SDL_Cursor*`.** A reinterpreted
   pointer would raise castmetric, and no CAST-OK category fits. `Platform::CursorObject`
   is defined only in `cursor_sdl.cpp`. Destroy clears the slot, so a destroyed handle
   behaves as Win32's dead HCURSOR does: Destroy does nothing and Set shows no cursor.
2. **W5: `Set(nullptr)` selects a transparent cursor.** That is Win32's `SetCursor(NULL)`;
   `SDL_SetCursor(NULL)` means "redraw" instead. Retail's `SetCursor(NULL)` at window
   creation, which W3 had left out, is now made, so no OS arrow shows during boot.
3. **W5: the dangling cursor after slot 3 cannot happen.** All three slot-3 callers in
   `device_startup.cpp` delete the DeviceData right after Shutdown. No caller guard was
   added; the slot behaviour above is the OS primitive's own.
4. **I1: device creation is one primitive, `CreateDevices`.** The plan named only
   ReadKeyboard / ReadMouse. Creation is DirectInput's protocol, and under SDL there are no
   devices. The null test on the factory (0x1800e18db) belongs to that protocol, so it
   moved with it. Teardown, Acquire and the reads are one call each.
5. **I2: `Platform::Input::Poll()` (SDL_PumpEvents) is a new call at the top of
   `Device::Update`.** This is plan §E rule 1: DirectInput was live at the poll, while
   SDL's state is as fresh as its last pump.
6. **I2: retail's two lifted key tables moved, unchanged, into
   `src/engine/input/input_key_tables.h`.** The generator and the unit test read the
   poller's own bytes.
7. **W7 landed before I3.** I3 waited on the coordinator's measurement.
8. **I3: option (b), the coordinator's decision from the measurement.** SDL's relative
   mode is on only while the middle button is held. See section 4.
9. **I4: `input_dinput_win.cpp` keeps only its layout asserts.** No Win32 input file was
   deleted or renamed; that is Block 5.

## 3. Recorded behaviour differences (all in the commit messages)

- **Cursor:**
  - SDL's arrow no longer flickers before the game cursor; SDL re-applies the game cursor
    itself.
  - An inverting pixel would be 0x00FFFFFF, SDL's Windows value. Retail never builds one.
- **Keyboard, legacy path only** (Input/Legacy, default 0):
  - VKs are the US layout's positions, where Win32 VKs follow the active layout.
  - The keypad digits read regardless of NumLock.
  - VK_OEM_8 (index 104) has no key.
- **Keyboard observation, not changed:** the lifted table's banner says an unmapped key
  "lights up index 104". The unpack assigns, so only DIK 0xFF decides index 104, and no
  key produces it. Retail behaves the same; the unit test pins it.
- **Mouse:**
  - During a middle-drag the cursor is hidden and confined to the window, and on release
    SDL puts it where its own pointer moved to. Retail's cursor stayed visible and free.
  - Outside the drag, x / y are cursor-position deltas, accelerated at non-1:1 pointer
    settings. That differs from DirectInput's raw mickeys even inside the window when
    "Enhance pointer precision" is on or the speed is not the middle notch. Option (b)
    removes the difference only while rotating.
- **LaunchProcess:**
  - The child is created with handles inherited and STARTF_USESTDHANDLES.
  - The environment is SDL's copy.
  - The hThread leak is gone.
  - "open" uses SW_SHOWNORMAL, not SW_SHOWDEFAULT.
  - An absolute path becomes a file:/// URL.
  - On POSIX the command line must be split into arguments. Not done: there is no caller.

## 4. The I3 measurement

The coordinator ran the user's drags by hand on uncommitted debug builds that logged
DirectInput's lX / lY beside SDL's xrel / yrel in the same poll. The pointer speed was the
middle notch with enhance OFF.

The first run (`runs/SDLC_i3/`) read DIERR_NOTACQUIRED on 783 of 785 DirectInput reads.
That was a harness bug: startup's Acquire had run without the foreground, and the harness
never re-acquired. The second run (`runs/SDLC_i3b/`) printed `DI acquired` once, and all
518 reads returned 0:

```
[I3Delta] DRAG polls=83  di_abs=(1245,118)  sdl_abs=(1245,118) ratio_abs_x=1.000 ratio_abs_y=1.000   slow, inside
[I3Delta] DRAG polls=47  di_abs=(268,996)   sdl_abs=(268,996)  ratio_abs_x=1.000 ratio_abs_y=1.000   fast, inside
[I3Delta] DRAG polls=64  di_abs=(1877,172)  sdl_abs=(1119,122) ratio_abs_x=0.596 ratio_abs_y=0.709   past the edge
[I3Delta] DRAG polls=322 di_abs=(11472,505) sdl_abs=(7739,495) ratio_abs_x=0.675 ratio_abs_y=0.980   long
```

SDL's deltas matched DirectInput's exactly inside the window, and lost 30-40 % once the
cursor pinned at the edge. Drag 4's net x even flips sign: -697 against +100. Hence
option (b).

## 5. The coordinator's run checks

The logs are in `C:/GOG Games/Banished/`. The settings live in `<Documents>\Banished\settings.cfg`.

| # | check | how | expected |
|---|---|---|---|
| 1 | gates | `sa-run`, `sa-run-menu`, `phase_d/uiroutes.sh`, `tools/resave_gate/resave_gate.sh Gate`, `phase_d/worldcmp.sh` | Unchanged from main. The menu driver writes Input::Device state directly and does not use OS input. |
| 2 | no DirectInput | `objdump -p reimpl-x64.exe \| grep -i dinput` | Nothing. |
| 3 | cursor | Boot to the menu and move over the window | No OS arrow during boot. The game cursor over the menu with its hotspot on the tip. No arrow flicker. `error.log` has no `[SDL] SDL_CreateColorCursor failed`. |
| 4 | build-menu hotkeys | In game, press each build-menu hotkey | The same tools open as on main. |
| 5 | typing | New game: type a town name and a seed, including Shift + letters, digits and Backspace | The text appears; holding a key repeats after about 0.3 s. |
| 6 | Esc / Enter | Esc opens / closes the pause menu; Enter confirms dialogs. Also the keypad Enter. | As on main. |
| 7 | middle-drag rotate | Drag slowly inside the window, then push the cursor past the screen edge and keep moving | Rotation continues past the edge. The cursor is hidden during the drag and reappears inside the window on release. |
| 8 | edge scroll | Move the cursor to each window edge | The camera scrolls. |
| 9 | wheel zoom | Wheel up / down in game | Zoom direction as on main; one notch is one step. |
| 10 | left-handed | Windows "Switch primary and secondary buttons" ON, then OFF | Both times the physical button acting as primary selects and places. No double swap. |
| 11 | clipping | `Input/MouseClipping = 1` in settings.cfg | The cursor cannot leave the client area. Alt-tab away frees it; returning clips it again. |
| 12 | alt-tab | Windowed and fullscreen | `[WndMsg] WM_ACTIVATE w=0` + `WM_KILLFOCUS` on leaving, `w=1` + `WM_SETFOCUS` on return. No key or button stays held across the switch: hold W, alt-tab, release, return, and the camera stops. |
| 13 | legacy path, optional | `Input/Legacy = 1` | Keys and mouse work. On a non-US layout the letters follow US positions (section 3). |
| 14 | LaunchProcess, optional | No caller exists | It can be exercised only by a temporary call, for example `LaunchProcess("cmd /c exit 3", true)` and `LaunchProcess("C:\\Windows", false)`. |

## 6. Open items

- **Block 5:**
  - Delete `input_dinput_win.cpp` (asserts only), `input_platform_startup_win.cpp`,
    `input_platform_shutdown_win.cpp` and `input_win.cpp`, or move the last three, which
    are OS-free, to `src/engine/input/`.
  - `ClipCursor` is SDL's now, so `window_win.cpp` holds only IsVisible / ShowDefault and
    the asserts.
- **COM:** `Platform::ComInit` stays. XAudio2, AUDIO's, is its last consumer. DirectInput
  is gone, and SDL_OpenURL does its own init.
- **Audio:** the xWMA and SDL audio blocks are not this block's.
- **Shift at launch:** `engine_platform_win.cpp`'s `GetAsyncKeyState(VK_L/RSHIFT)` for the
  launcher belongs to the settings seam and is untouched.

## 7. Shared files

- **`src/platform/common/platform_window.h`** (SDL-B, RENDER-3): Cursor comments only
  (W5). `CursorObject` is "defined only by the backend". No signature changed.
- **`src/platform/win/window_win.cpp`:** W5 removed `Cursor::{Create, Set, Show, Destroy}`
  and the HCURSOR assert; I3 removed `ClipCursor`.
- **`src/platform/sdl/window_sdl.cpp`:** `Window::Create` calls `Cursor::Set(nullptr)`;
  comment.
- **`src/platform/sdl/input_sdl.cpp`** (new): it also defines `Platform::Window::ClipCursor`.
- **`pump_sdl.cpp`, `window_hook_win.cpp`:** not changed.
- **`src/render/dx11/devicedata.cpp`, `VideoDX11Data.h`:** comments only. `devicedata.o`
  is B11 IDENTICAL.
- **Input:** `src/engine/input/Device.h` has its three DirectInput fields as `void*`, same
  offsets. Also changed: `input_device_update.cpp`, `input_platform_startup_win.cpp` and
  `input_platform_shutdown_win.cpp`.
- **`src/platform/win/application_win.cpp`:** LaunchProcess; no `<windows.h>`.
- **`src/platform/common/platform_os.h`:** `Platform::Process` added.
- **Build:** `Makefile` and `CMakeLists.txt` drop `-ldinput8 -ldxguid`;
  `tests/unit/sources.{mk,cmake}` add `cursor_planes.cpp` and `open_url.cpp`.
- **New files:**
  - `src/platform/sdl/{cursor_sdl, input_sdl, process_sdl}.cpp`
  - `src/platform/common/{cursor_planes, open_url}.{h,cpp}`
  - `src/platform/common/{platform_input, sdl_dik_tables}.h`
  - `src/platform/win/input_dinput_win.cpp`
  - `src/engine/input/input_key_tables.h`
  - `tools/sdl_dik/gen_sdl_dik_tables.py`
  - `tests/unit/{cursor_planes, sdl_scancode_dik, open_url}_test.cpp`
- **The CMake twin was not built.** The new files fall under its existing globs.
