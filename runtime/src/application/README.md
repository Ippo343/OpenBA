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
| `application_accessors.cpp`, `application_thunks.cpp` | `Get`, `GetInstance`, `GetWindow`, `IsCursorEnabled`, `PlatformIsPaused`, `PlatformDestruct`; `Update`/`Shutdown`/`Startup` tail-call thunks (Wave 12 lane A); the old `application_lifecycle_probe.cpp` oracle is gone (A.3/A.4 deleted the probes) |

`??0Application@0@QEAA@PEAX@Z` (the constructor) is ported in
`runtime/src/engine/singleton_ctors.cpp`, with the fourteen other constructors that publish a
`?_singleton@...` data export.

## Status (ENDGAME B.3.7 closing pass, 2026-09-26)

`Application::Application` (0x30, non-polymorphic) is a REAL C++ class with its 27 exported members (`runtime/types/application/Application.h`, every name spelled with MSVC's `@Application@1@` back-reference). The ctor stays `Ctor_Application` in `runtime/src/engine/singleton_ctors.cpp` (inlined in retail `Engine::Startup`) and `~Application` is declared, not defined, so `Engine::Shutdown` keeps calling its door; dx11 calls the members. Rules and history: `docs/runtime_port/endgame_b/B37_RULES.md`; numbers: `docs/runtime_port/endgame_b/b37_session/CLOSE_NUMBERS.md`.
