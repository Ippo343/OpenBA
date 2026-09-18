# Engine architecture (reference)

Banished uses a bespoke C++ engine (not Unity/Unreal). Namespaces were recovered from
MSVC mangled symbols. Addresses below are retail Ghidra addresses.

## Main loop
```
Engine::MainLoop::Run(UpdateLoop*)
  -> UpdateLoop::Update(Time)      // game logic tick
  -> UpdateLoop::Draw(float)       // render frame
  -> MainLoop::SetNextLoop()       // state transitions (menu/load/game)
```
Loop types: `LoaderLoop`, `DialogLoaderLoopT`, `DialogLoopT` (menus/loading), plus the
gameplay `UpdateLoop`. MainLoop singleton at Runtime `0x1801fe750`.

## Launcher dialog
`Engine::Engine::PlatformConfigure` (Runtime `0x1800a1670`) opens a Win32 launcher dialog
(Options/Video/Play) whenever the registry's `VideoSettings` bit says the previous run
never finished `Engine::Startup` — in development, every time the process is killed.

We do not patch the call site. `HarnessGuardUnattendedBoot()` in `standalone/main.cpp`
clears that one bit under `HKCU\Software\Shining Rock Software LLC\Banished`, but only
when a harness flag (`BANISHED_AUTOSTART`, `BANISHED_AUTOQUIT`, `BANISHED_AUTOLOAD`,
`BANISHED_AUTOTUTORIAL`, `BANISHED_AUTOMENU_BUTTON`, `BANISHED_PAUSEMENU_BUTTON`) marks
the run unattended. Attended runs follow retail's rules and can still hit the dialog.
The same function repairs a degenerate persisted window size (see its comment).

A blocked boot looks like a slow load; detect it with `tools/boot_state.sh`
(the `triage-run` skill).

## Threading
- `System::Thread`: `Start()`, `WaitForFinish()`, virtual `OnCompleted()`
- `Application::YieldToThreads()`: cooperative yielding
- Windows ThreadPool API (`CreateThreadpoolTimer`, `CreateThreadpoolWait`)
- Critical sections (`InitializeCriticalSectionAndSpinCount`)

## Address resolution in our exe
Nothing is resolved from a DLL. All of these consult the registry
(`standalone/address_registry.cpp`, `standalone/rt_export_addrs.inc`) and the data mint:
- `ResolveAddr(Module::Exe, ghidra_addr)` — a reimplemented exe function / slot / vtable
- `ResolveAddr(Module::Runtime, ghidra_addr)` — a ported Runtime function by retail address
- `ResolveExport("?mangled@name")` — the same port by MSVC mangled name
  (name and address are one door)

Key files: `standalone/resolve.h`, `standalone/resolve_standalone.cpp`,
`standalone/address_registry.cpp` + `standalone/reg_*.cpp`, `standalone/main.cpp`
(WinMain, unattended-boot guard, VEH crash handler, log redirection).
