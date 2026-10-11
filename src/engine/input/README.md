# src/engine/input — original `code\Runtime\Input\`

Port target for the Runtime-x64.dll `Input` directory. The file list below is the
ORIGINAL source tree recovered from `Runtime-x64-profile.dll` debug strings
(`docs/history/runtime_port/original_source_tree.txt`); mirror these names here, lowercased,
as `src/engine/input/<file>.cpp`, with binary-compatible types in
`src/engine/input/<Class>.h` (`static_assert` on every field offset).

No file in this directory includes an OS or SDL header. The OS half of `Input::Device`
is `Platform::Input` / `Platform::Window` (`src/platform/common/platform_input.h`,
`platform_window.h`), implemented over SDL3 in `src/platform/sdl/input_sdl.cpp` on every
host (Phase D.4 SDL-C). The DirectInput 8 backend is gone (SDL-5 deleted
`src/platform/win/input_dinput_win.cpp`), and the three files that were the `_win` seam
live here since SDL-5 under names without `_win`.
A `_compile` file is the offline asset compiler and is likely absent from retail —
confirm against the retail DLL before porting one.

## Original files (3 cpp, 2 h)

- `InputConfiguration.cpp`
- `InputConfiguration.h`
- `InputDevice.cpp`
- `InputDevice.h`
- `KeyMap.cpp`

## Ported here

This section said "_nothing yet_" until Wave 9 lane INPUT, by which point five
files had already landed here across three waves — a stale banner over ported
code, which is the failure PLAN.md §6 step 8 exists to stop.

| File | Original | Contents |
|---|---|---|
| `input.cpp` | `KeyMap.cpp` + `InputDevice.cpp` + `InputConfiguration.cpp` | Wave 8 lane D's 15 exports (KeyMap 9, Device 4, Configuration 2), plus `RegisterInput` unblocked in Wave 9 |
| `keymap_load.cpp` | `KeyMap.cpp` | `?Load@KeyMap@Input@@` (Wave 9 lane IOA) |
| `keymap_save.cpp` | `KeyMap.cpp` | `?Save@KeyMap@Input@@` + the unexported deleting destructor `0x180053440` (Wave 9 lane INPUT) |
| `input_platform_update.cpp` | `InputDevice.cpp` | `EnableMouseClipping` + `PlatformUpdate` (Wave 9 lane IOA; was `src/platform/win/input_win.cpp`) |
| `input_platform_startup.cpp` | `InputDevice.cpp` | `PlatformStartup` + the three focus / wheel message callbacks (Wave 9 lane PLAT; was `src/platform/win/input_platform_startup_win.cpp`) |
| `input_platform_shutdown.cpp` | `InputDevice.cpp` | `PlatformShutdown` (Wave 24 lane SMALLBODY; was `src/platform/win/input_platform_shutdown_win.cpp`) |
| `input_device_update.cpp` | `InputDevice.cpp` | `Device::Update` and its pollers over `Platform::Input` |
| `input_config_startup.cpp` | `InputConfiguration.cpp` | `?Startup@Configuration@Input@@` + the unexported `Load` (Wave 9 lane ENG) |

The `Collection::Set<Binding>` AVL internals this directory reaches by address
live in `src/engine/collection/set_avl_e24.cpp`, not here: they are
element-type-agnostic within the 0x18-byte instantiation and MSVC folds one copy
per matching node header, so they are not Input's to own.

`?Update@Device@Input@@` and its helpers, once listed here as not ported, are in
`input_device_update.cpp` (MainLoop::Run calls the member every frame). Members
still declared and not ported carry a `// not ported` marker in
`src/engine/input/Input.h` / `Device.h`.

## Status (ENDGAME B.3.7 closing pass, 2026-09-26)

`Input::KeyMap` (0x18; `Binding` 0x18 / `BindingNode` 0x38), `Input::Device` (exactly 0x1200) and `Input::Configuration` (0x10) are REAL non-polymorphic C++ classes in `src/engine/input/{Input,Device}.h`; every tree caller calls the members (the `fn_KeyMap_*` / `fn_Input_Device_*` globals are deleted, and the drag tool's `KeyMap::Released` path, bound to an unregistered slot before, is live). `~Configuration` and `~KeyMap` are declared, not defined: they stay doors. `KeyMap`'s two split fields stand in for a `Collection::Set` until that template has a byte-key compare policy (B37_RULES 12). Rules and history: `docs/runtime_port/endgame_b/B37_RULES.md`; numbers: `docs/runtime_port/endgame_b/b37_session/CLOSE_NUMBERS.md`.
