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

## Launcher dialog and settings
Retail `Engine::Engine::PlatformConfigure` (Runtime `0x1800a1670`) opens a Win32 launcher
dialog (Options/Video/Play) whenever the `VideoSettings` bit says the previous run never
finished `Engine::Startup` (Half B), or when `-launcher` or shift is given. Its two check
boxes (bytes `0x1801fc699` "disable mods", `0x1801fc698` "reset settings", per the ported
code's reading) are ORed with `-nomods` / `-reset` after the dialog closes.

This exe has no launcher dialog (Phase D.4 SETTINGS, the user's decision). Half B validates
the configuration and proceeds as Play. The check boxes are two entries of the settings file,
`LauncherDisableMods` and `LauncherResetSettings` (`true` / `false`). They are one-shot:
`PlatformConfigure` reads them and sets them back to `false` on every boot, Half A
included, and ORs them where retail ORs the two bytes.

The settings themselves (retail: `HKCU\Software\Shining Rock Software LLC\Banished`, every
value a string) live in `<Documents>\Banished\settings.cfg`, next to `Save\`: one
`key=value` per line, UTF-8, sorted (`src/engine/io/settings_file.h` has the format).
`IO::Persistence::ReadSettings` reads it (`src/engine/io/persistence_settings.cpp`). When it
is absent, the registry key is imported once, read-only, and the first `WriteSettings`
writes the file. `WriteSettings` merges the map over the file, so keys the map no longer
holds survive, as they did in the registry. The registry is never written again; delete
`settings.cfg` to re-import it. `tools/ach_reset.sh` clears the `Ach<N>` entries.

`HarnessGuardUnattendedBoot()` in `src/app/main.cpp` edits the same file before
`Engine::Startup`. When a harness flag (`BANISHED_AUTOSTART`, `BANISHED_AUTOQUIT`,
`BANISHED_AUTOLOAD`, `BANISHED_AUTOTUTORIAL`, `BANISHED_AUTOMENU_BUTTON`,
`BANISHED_PAUSEMENU_BUTTON`) marks the run unattended, it clears `VideoSettings`, which keeps
the run on Half A. On every run it repairs a degenerate persisted window size (see its
comment).

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
`standalone/address_registry.cpp` + `standalone/reg_*.cpp`, `src/app/main.cpp`
(WinMain, unattended-boot guard, VEH crash handler, log redirection).
