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
