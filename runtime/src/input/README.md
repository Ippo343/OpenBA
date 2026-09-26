# runtime/src/input — original `code\Runtime\Input\`

Port target for the Runtime-x64.dll `Input` directory. The file list below is the
ORIGINAL source tree recovered from `Runtime-x64-profile.dll` debug strings
(`docs/runtime_port/original_source_tree.txt`); mirror these names here, lowercased,
as `runtime/src/input/<file>.cpp`, with binary-compatible types in
`runtime/types/input/<Class>.h` (`static_assert` on every field offset).

A `_win` file is a PLATFORM SEAM (PLAN.md §4.1 decision 1): keep `<windows.h>` out of
every other file in this directory so a `_posix`/`_sdl` twin can be written later.
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
| `input_win.cpp` | `InputDevice.cpp` (the `_win` seam) | `EnableMouseClipping` + `PlatformUpdate` (Wave 9 lane IOA) |
| `input_config_startup.cpp` | `InputConfiguration.cpp` | `?Startup@Configuration@Input@@` + the unexported `Load` (Wave 9 lane ENG) |

The `Collection::Set<Binding>` AVL internals this directory reaches by address
live in `runtime/src/collection/set_avl_e24.cpp`, not here: they are
element-type-agnostic within the 0x18-byte instantiation and MSVC folds one copy
per matching node header, so they are not Input's to own.

`?Update@Device@Input@@` and its helpers, once listed here as not ported, are in
`input_device_update.cpp` (MainLoop::Run calls the member every frame). Members
still declared and not ported carry a `// not ported` marker in
`runtime/types/input/Input.h` / `Device.h`.
