# runtime/src/engine — original `code\Runtime\Engine\`

Port target for the Runtime-x64.dll `Engine` directory. The file list below is the
ORIGINAL source tree recovered from `Runtime-x64-profile.dll` debug strings
(`docs/runtime_port/original_source_tree.txt`); mirror these names here, lowercased,
as `runtime/src/engine/<file>.cpp`, with binary-compatible types in
`runtime/types/engine/<Class>.h` (`static_assert` on every field offset).

A `_win` file is a PLATFORM SEAM (PLAN.md §4.1 decision 1): keep `<windows.h>` out of
every other file in this directory so a `_posix`/`_sdl` twin can be written later.
A `_compile` file is the offline asset compiler and is likely absent from retail —
confirm against the retail DLL before porting one.

## Original files (5 cpp, 0 h)

- `DialogLoaderLoop.cpp`
- `DialogLoop.cpp`
- `Engine.cpp`
- `Engine_win.cpp`
- `MainLoop.cpp`

## Ported here

- `loops.cpp` — Wave 8 lane A. `Engine::UpdateLoop` (ctor, dtor, IsRunning),
  `Engine::Loader::Load`, `Engine::LoaderLoop` (ctor), `Engine::DialogLoopT`
  (ctor, Load, Startup, Update) and `Engine::DialogLoaderLoopT` (ctor, Load,
  Startup) — 12 exports, entry point `int RtEngineLoopsRegister()`. Layouts in
  `runtime/types/engine/UpdateLoop.h`; report in
  `docs/runtime_port/WAVE8_LANE_A.md`. (`?LoadResources@UpdateLoop@Engine@@QEAAXXZ`,
  held back then, is ported: `updateloop_loadresources.cpp`.) NOT registered by design:
  `?_loader@LoaderLoop@Engine@@1VLoader@2@A` — it is the DLL's own static
  `Loader` object, and the loader thread waits on ITS events.
  There is no `_win` twin: the one Win32 idiom in the family (Loader::Load's
  inlined `Thread::Start`) is the `System::Thread::Start` member, since
  `Engine::Loader` derives from the real `System::Thread` (B.3.7).

## Status (ENDGAME B.3.7 closing pass, 2026-09-26)

Every Runtime Engine class is a REAL C++ class under its retail name (`runtime/types/engine/`): the four loops `UpdateLoop` (abstract root, 12 slots, 0x28), `LoaderLoop` (0x48), `DialogLoopT` (0x168) and `DialogLoaderLoopT` (0x188) in `UpdateLoop.h`, `Loader` (a real `System::Thread` subclass, 0x38), and the non-polymorphic `MainLoop` (0xa0) and `Engine` (0x6f8, pinned). Bodies: `loops.cpp` (the loops and `Loader`), `updateloop_loadresources.cpp`, `mainloop_run.cpp` / `mainloop_frame.cpp` / `mainloop_video.cpp`, `engine.cpp`, `engine_lifecycle*.cpp`, `engine_platform*.cpp`; `singleton_ctors*.cpp` keeps the ported ctors of the classes `Engine::Startup` builds inline (they stay doors, B37_RULES 2.4). The loop ctor/dtor doors stay registered because the exe's five hand-built loop tables construct their bases through them (step 8), and the `RtEngine::` loop PODs stay because `engine/types/` still embeds them (B37_RULES 12). Rules and history: `docs/runtime_port/endgame_b/B37_RULES.md`; numbers: `docs/runtime_port/endgame_b/b37_session/CLOSE_NUMBERS.md`.
