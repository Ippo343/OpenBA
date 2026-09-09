# runtime/src/application — original `code\Runtime\Application\`

Port target for the Runtime-x64.dll `Application` directory. The file list below is the
ORIGINAL source tree recovered from `Runtime-x64-profile.dll` debug strings
(`docs/runtime_port/original_source_tree.txt`); mirror these names here, lowercased,
as `runtime/src/application/<file>.cpp`, with binary-compatible types in
`runtime/types/application/<Class>.h` (`static_assert` on every field offset).

A `_win` file is a PLATFORM SEAM (PLAN.md §4.1 decision 1): keep `<windows.h>` out of
every other file in this directory so a `_posix`/`_sdl` twin can be written later.
A `_compile` file is the offline asset compiler and is likely absent from retail —
confirm against the retail DLL before porting one.

## Original files (1 cpp, 1 h)

- `Application.cpp`
- `Application.h`

## Ported here

`Application.cpp` is split across the platform seam rather than mirrored one-to-one, because
`_win` files may include `<windows.h>` and the rest of the directory may not:

| file | exports |
|---|---|
| `application.cpp` | `IsPaused`, `Quit` |
| `application_win.cpp` | `PlatformUpdate`, `SleepThread`, `YieldToThreads`, `PlatformConstruct` |
| `message_callbacks.cpp` | `RegisterMessageCallback`, `UnregisterMessageCallback`, + `0x18000a5b0` |
| `platform_startup_win.cpp` | `PlatformStartup`, `WindowProc`, `PlatformShutdown` (+ four cross-namespace `PlatformStartup` rows) |
| `application_lifecycle.cpp` | `~Application`, copy ctor, `operator=`, + the Set helpers `0x18000a4f0` / `0x18000a530` / `0x18000a440` |
| `application_lifecycle_probe.cpp` | not a port — the lockstep oracle for the file above (`BANISHED_RT_APPB_PROBE=1`) |

`??0Application@0@QEAA@PEAX@Z` (the constructor) is ported in
`runtime/src/engine/singleton_ctors.cpp`, with the fourteen other constructors that publish a
`?_singleton@...` data export.
