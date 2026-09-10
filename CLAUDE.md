# Banished x64 Reverse Engineering - Progressive Reimplementation

## Agent rules
When dispatching subagents use a lower tier subagent when appropriate

## Project Goal
Reverse engineer Banished (by Shining Rock Software) and progressively replace game
functions with clean, readable C++ source code. Each decompiled function gets a
binary-compatible type header and a reimplementation.
The end goal is a readable C++ codebase that mirrors the game's logic and can be compiled into a replacement exe, similar to projects like OpenRCT2, re3 (GTA III), and devilution (Diablo).

**Current state**: the project builds a standalone replacement exe (`reimpl-x64.exe`)
that links against the original Runtime-x64.dll and runs our reimplemented game logic
from `code/Game/`. VideoDX11-x64.dll is already fully replaced by `dx11/` (never loaded).
The old proxy-DLL/hooking mode has been removed. Remaining exe functions not yet
reimplemented are stubbed or registered individually via the address registry (see below).

**Next phase (2026-09-06): port Runtime-x64.dll.** Review, facts, wave order, risks and the
per-function recipe are in **[docs/runtime_port/PLAN.md](docs/runtime_port/PLAN.md)**. Read it
before any runtime work.

**Status 2026-09-10 — WAVE 20 CLOSED (`cac74539`): an INSTRUMENTS wave.** All three
known-broken tools fixed, the 31-row vtable-extent question closed in **both** error
directions, and `??_7BinaryFileStream@` promoted — **attendance 0 -> 18,906**, which
converts four of Wave 19's ten TRAP=0/attendance-0 bodies into bodies that actually run.
IO 133/116 -> **216 called / 200 ported**; TOTAL **1311/1124 (85.7%)**.

**DO NOT read 85.7% as 84.3% plus work.** The census was fixed mid-wave and +111 of that
movement is rows that always existed and were invisible to the old tool. **Six symbols
entered `ported`.** Read `docs/runtime_port/WAVE20_CLOSED.md` §2 (what it does NOT claim)
and §4 before quoting any figure.

**§4 is the finding that outlives the wave:** three items the previous handover named as
*cheap* or *highest-leverage* were stale — one already promoted, one ported nine waves
earlier, one "safest" only because it was pointless — while every **measured** section
held. **A cheap-wins list is the stalest section of a handover and the first one acted
on.** Wave 21 inputs are in §8, each with its own re-derivation command.

**NEXT: Graphics (99 called / 70 ported, ceiling 96.0%) is the largest untouched gap.**

**Score a namespace on the IDENTITY, never the percentage.** `ceiling = (called -
unpromotable) / called` has `called` in BOTH halves, so any lane opening a door
moves it: closing seven trap slots sent Video's ported% DOWN and its printed
ceiling UP with zero triage change.

**The cutover boot wall moved** off `DeviceData::EnumerateInterfaces` (where
attempts 23 and 24 both died) to **`?PlatformConfigure@Engine@1@`** (`0x1800a1670`,
EXEC-of-0 at `RtEngine_Engine_Startup+0x777`). That is the Win32 launcher dialog
already described under "Launcher Dialog" below, and `HarnessGuardUnattendedBoot()`
already has a seam on it — a softer wall than the last one.

**NEXT: IO and Graphics, NOT Video.** Corroborated three independent ways. Real
remaining work is **IO 34 > Graphics 29 > Video 0**, and their data rows are
BLOCKED on unported vtable slots — ordinary port work, not a ceiling. Wave 19
scoping items are listed in WAVE18_CLOSED.md §8.

## Target Binaries

The game is a multi-binary architecture. The exe contains game logic; the engine runtime is in DLLs.

### Application-x64.exe (Game Logic)
- **File**: `C:\GOG Games\Banished\Application-x64.exe`
- **Architecture**: x86-64, Little Endian, Windows (MSVC)
- **Base Address**: `0x140000000`
- **Entry Point**: `0x14003b1c0`
- **Code Section**: `.text` `0x140001000` - `0x1401501ff` (~1.4 MB)
- **Size**: 1.9 MB | **Functions**: 11,473 (Ghidra today; 10,476 was the initial import count) | **Symbols**: 50,420
- **Exports**: `NvOptimusEnablement`, `AmdPowerXpressRequestHighPerformance` (GPU preference flags)
- **PE Timestamp**: `2017-09-10 16:34:27`

### Runtime-x64.dll (Engine Core)
- **File**: `C:\GOG Games\Banished\Runtime-x64.dll`
- **Size**: 2.7 MB | **Functions**: 15,290 | **Image Base**: `0x180000000`
- **PE Timestamp**: `2017-09-10 16:34:14`
- **Exports**: ~4,800+ ordinals (full engine API surface, reflection-based with consistent TypeName/Factory/Lifecycle pattern per class)
- **Role**: Complete custom game engine runtime. 18 namespaces, 100+ classes covering object model, math, rendering, UI (20+ widgets), file I/O with packages/mods, 2D collision, 3D picking, A* pathfinding, achievements, save/load, input (DirectInput), threading
- **Imports**: KERNEL32, USER32, GDI32, ADVAPI32, SHELL32, OLE32, DINPUT8, DBGHELP
- **Imported via a NORMAL PE import table** (corrected 2026-09-04; this entry
  previously claimed the opposite and that error was load-bearing for a long
  time). `pefile` parses it directly: **833 imports from Runtime-x64.dll** in
  slots `0x140151218`-`0x140152c18`, plus 66 from KERNEL32.dll in
  `0x140151000`-`0x140151208`. The `.rdata` "dispatch slots" we register with
  `RegisterDispatchSlot(addr, ResolveExport(name))` ARE that import table, so
  **every one of them has a known name that can be read out of the binary** --
  no Ghidra archaeology needed:
  ```
  tools/ghidra_mcp/.venv/Scripts/python.exe tools/dispatch_slot_map.py 0x140151458
  tools/ghidra_mcp/.venv/Scripts/python.exe tools/dispatch_slot_map.py --check
  ```
  `--check` validates our registrations against the binary and must stay at
  zero mismatches. It also reports SHADOWED addresses (a slot registered twice,
  where the later binding wins and the checked line is dead code).
  **Registering an unregistered slot is not automatically safe**: some are left
  null on purpose so a dispatch-or-fallback path takes the fallback, and a few
  are read under the WRONG name and work only while null. Grep the address
  across `code/`, `engine/` and `standalone/` and confirm every consumer wants
  the same function before registering. See
  `docs/waves/unregistered_slots_wave.md`.

### VideoDX11-x64.dll (DX11 Renderer Plugin)
- **Size**: 145 KB | **Functions**: 576 (~86 rendering, rest CRT/exception)
- **Image Base**: `0x180000000`
- **Exports**: `CreateDeviceData` (Ordinal_1), `GetDeviceName` (Ordinal_2) - plugin factory pattern
- **Role**: DX11 backend implementing Strategy/Bridge pattern. Provides `*Data` companion classes for engine abstractions (DeviceData, ContextData, VertexBufferData, etc.)
- **Specs**: Backbuffer `DXGI_FORMAT_B8G8R8A8_UNORM`, Depth `DXGI_FORMAT_D24_UNORM_S8_UINT`, min `D3D_FEATURE_LEVEL_10_0`

### VideoDX9-x64.dll (DX9 Fallback)
- DirectX 9 fallback renderer (same plugin interface, not loaded in Ghidra)

## Engine Architecture

### Custom C++ Engine (not Unity/Unreal)
Banished uses a bespoke engine. Namespaces recovered from MSVC mangled symbols.

### Main Loop Flow
```
Engine::MainLoop::Run(UpdateLoop*)
  -> UpdateLoop::Update(Time)      // game logic tick
  -> UpdateLoop::Draw(float)       // render frame
  -> MainLoop::SetNextLoop()       // state transitions (menu/load/game)
```
Loop types: `LoaderLoop`, `DialogLoaderLoopT`, `DialogLoopT` (menus/loading), plus the main gameplay `UpdateLoop`. MainLoop singleton at Runtime `0x1801fe750`.

### Launcher Dialog
`Engine::Engine::PlatformConfigure` (Runtime 0x1800a1670) opens a Win32 launcher
dialog (Options/Video/Play) whenever the registry's `VideoSettings` bit says the
previous run never finished `Engine::Startup` — which in development means every
time the process is killed. The standalone exe no longer patches this call site
(the old `SkipPlatformConfigure()` is gone). Instead `HarnessGuardUnattendedBoot()`
(`standalone/main.cpp:184`, called from `Init` at `standalone/main.cpp:1559`) clears
that one `VideoSettings` registry bit through `HKCU\Software\Shining Rock Software
LLC\Banished` — but only when a harness env var (`BANISHED_AUTOSTART`,
`BANISHED_AUTOQUIT`, `BANISHED_AUTOLOAD`, `BANISHED_AUTOTUTORIAL`,
`BANISHED_AUTOMENU_BUTTON`, or `BANISHED_PAUSEMENU_BUTTON`) says this run is
unattended. An attended run is left to retail's own rules and can still hit the
dialog. The same function also repairs a degenerate persisted window size
(see the comment above `HarnessGuardUnattendedBoot` in `standalone/main.cpp`).

### Threading Model
- `System::Thread` with `Start()`, `WaitForFinish()`, `OnCompleted()` virtual
- `Application::YieldToThreads()` - cooperative yielding
- Windows ThreadPool API (CreateThreadpoolTimer, CreateThreadpoolWait)
- Critical sections for synchronization (InitializeCriticalSectionAndSpinCount)

## Type & Function Inventory

Full decompilation status tracking is in the **[decomp/](decomp/README.md)** directory, split per-binary:

- **[decomp/application.md](decomp/application.md)** — Application-x64.exe (11,473 functions: ECS components, UI controllers, game logic)
- **[decomp/runtime.md](decomp/runtime.md)** — Runtime-x64.dll (15,290 functions: 18 namespaces, 100+ classes)
- **[decomp/videodx11.md](decomp/videodx11.md)** — VideoDX11-x64.dll (576 functions: DX11 renderer plugin)

---

## Project Organization Decisions

### Ghidra is the primary documentation tool
All function renaming, type annotation, struct layouts, and per-function comments go directly in Ghidra. 
It's persistent, searchable, and stays in sync with the actual analysis. 
External markdown files are for higher-level notes that don't fit in Ghidra (architecture diagrams, algorithm analysis, session logs).

---

## Modding Toolkit Reference
- Toolkit: https://banishedinfo.com/mods/resources/gettingstarted
- Developer build: `Application-x64-profile.exe` (has profiling hooks)
- Resource compiler: `Tools-x64.exe` compiles `.rsc` -> binary in `/bin/WinData`
- Mod packages: `.pkm` files (delta-packed against base game data)
- Resource loading order: loose files -> `.pkg` (game) -> `.pkm` (mods)
- The toolkit provides a **profile build** that may expose timing/debug info

### Build & Deploy
```bash

# Standalone EXE mode (replacement exe, links against Runtime DLL)
mingw32-make standalone        # build release standalone exe
mingw32-make standalone-debug  # build debug standalone exe
mingw32-make sa-deploy         # build debug + copy to Banished directory
mingw32-make sa-run            # build debug + copy + launch (AUTO-STARTS a new game)
mingw32-make sa-run-menu       # same, but stops at the main menu (no autostart)
# Live features are DEFAULT-ON in code (kDefaultOnFlags in
# code/Game/Component/component_factory_base.cpp). BANISHED_LIVE_* env vars are
# kill-switches (=0) for A/B runs; new unproven features start opt-in outside the table.
# Output: standalone/build/reimpl-x64.exe

mingw32-make clean     # remove all build artifacts (both modes)
```

### Standalone Test Driver (default testing path)

**`mingw32-make sa-run` is the default way to test the standalone build.** It sets
`BANISHED_AUTOSTART=1`, which makes the menu loop auto-drive **menu → New Game →
Start** with no manual clicking, landing directly in the in-game world build. This
is the path to use for verifying any rendering / gameplay / world-build change.

- **Mechanism**: `standalone/autostart.cpp` ticks a state machine from
  `MenuDLT::Stub_Update`. It injects the two already-reimplemented button-click
  events via the dialog vtable slot-8 handlers (`MainMenuDialog::OnCommand` id=0
  "buttonNew", then `NewGameDialog::OnEvent` data=2 "buttonOk") — no input
  simulation. The existing Stage-C transition in `MenuDLT::Update` then advances
  to the queued GameUpdateLoop chain, exactly like a manual click.
- **Enable**: `BANISHED_AUTOSTART` env var (any value except `0`/`false`). A numeric
  value > 1 overrides the menu settle-frame count (default 60). `make sa-run` sets it;
  a direct launch of `reimpl-x64.exe` leaves it unset and stops at the menu.
- **Trace**: grep `standalone_debug.log` for `[AutoStart]` lines (each step logs).
  A 600-frame (~10s) timeout aborts the Start injection if `NewGameDialog::buttonOk`
  (+0xF0) never populates, rather than crashing on half-built combos.
- **VS Code**: launch config **"Launch Standalone (debug, autostart new game)"**
  sets the env var for the debuggee; "Launch Standalone (debug)" stops at the menu.
- To exercise the menu itself (not auto-advance), use `make sa-run-menu`.
- **Clicking menu buttons unattended**: `BANISHED_AUTOMENU_BUTTON=<id>` clicks one
  front-end button (Mods=7, Achievements=8, Goals=10, Quit=6) and stops there.
  `BANISHED_AUTOMENU_CLICKS=<tok>,<tok>,...` then delivers follow-up clicks one settle
  interval apart: `<id>` → the last-opened ModDialog, `d<id>` → ModDetailsDialog,
  `m<id>` → the MainMenuDialog again. The in-game half is `BANISHED_PAUSEMENU=600
  BANISHED_PAUSEMENU_QUIT=120 BANISHED_PAUSEMENU_BUTTON=7 BANISHED_PAUSEMENU_CLICKS=...`
  (extra tokens `w` wait, `p` dump the mod/package state; the feed survives the
  world reload a mods Ok triggers). `BANISHED_MODAL_ANSWER=<n>` answers a modal
  `UI::ShowDialog` (which pumps its own loop no driver can reach) with `n`.
  Example, the Mods menu: `BANISHED_AUTOMENU_BUTTON=7
  BANISHED_AUTOMENU_CLICKS=1001,5001,d0,4001,101,m6 BANISHED_MODAL_ANSWER=1`.
  **THIS EXAMPLE IS CURRENTLY BROKEN (verified 2026-09-10 on `3da1503` and on the
  Wave 17 merge).** It dies ~5s in with an ACCESS_VIOLATION, `READ of 0xd0`, at
  `reimpl-x64.exe+0x144575` (`ModDialog_ToggleMod_Impl+0x43`) <- `ModDialog_Slot8`
  <- `AutoStart::TickMenu`. Found by Wave 17 lane CEIL with a control (same crash,
  same address, with its own env var unset) and reproduced independently by the
  integrator; evidence in `docs/runtime_port/wave17_modsroute_crash.txt`. Do not
  plan a lane around this route until it is fixed, and per project doctrine the
  fix is to find the upstream null, NOT to guard the deref.

### Stress Test / Performance Baseline

`mingw32-make sa-stress` (details: **[perf/README.md](perf/README.md)**) builds the perf exe
(`standalone-perf`: `-O2 -g`, logs stripped, own object dir `standalone/build-perf/`), then runs
`BANISHED_STRESS=1`: a medium map with 3 sets of every placeable building, 800 citizens in
their own houses, game speed 10x, every workplace staffed through the real seat cascade, camera pinned on the
town, FPS cap off, and a clean self-quit after 6000 sim frames (60k ticks at 10x). Output per run in
`perf/results/<stamp>_<label>/`: `standalone_stress.csv` (one row per 600 frames: update/draw
ms, SIMPROF phase split, population/employment/entities, memory), the per-component-type Update
profile, and `report.txt` from `tools/stress_report.py`, auto-diffed against
`perf/baselines/current.csv`. Knobs: `STRESS_ARGS="--label x --speed 5 --citizens 600 --sets 3
--frames 6000"`. `make sa-stress-debug` = same town on the -O0 exe (functional check only).
Headline metric is `sim ms / iteration` (speed- and GPU-independent). Also added by this
harness: `BANISHED_MAP_SIZE=small|medium|large`, `BANISHED_CLIMATE=mild|fair|harsh`,
`BANISHED_CAMERA_PIN=town[,dist]`. Profiling: `BANISHED_SAMPLE=1000` = sampling profiler
(`standalone/sampler.cpp`, report with `tools/sample_report.py`) — use it before the
per-component table, which cannot see libm/Runtime/walk time. Heap: `BANISHED_REDZONE=watch`
(allocator watchdog, no shim) + `standalone/hw_watch.cpp` hardware write watchpoints.
`getenv`/`std::getenv` are cached process-wide (`standalone/env_cache.h`, force-included), so a
per-call `if (std::getenv("BANISHED_X_TRACE"))` in tick code is cheap; env changes must go
through `_putenv_s` (wrapped, invalidates the cache).

### Camera Pinning (deterministic screenshots)

`BANISHED_CAMERA_PIN=x,y[,dist]` (`code/Game/Game/camera_input.cpp`) pins the camera
focus to a world position every frame, overriding pan/follow — the autostart camera
otherwise roams and lands differently every run, which makes screenshot comparisons
meaningless. Use it to park the view on the thing under test, screenshot, and get the
identical framing on every run of the same seed.

- **World coords**: `world = buildCell − mapDim/2` (e.g. small map: cell − 128). For a
  placed structure, take the tile from its `[startTown::…] tile=(x,y) fp=WxH` log line
  and pin the footprint center: `x + W/2 − 128, y + H/2 − 128`.
  (The 512 terrain grid has a 128-cell non-playable border; build cells map 1:1 onto its
  center — there is NO 2-cells-per-tile scaling.)
- `dist` defaults to 40; ~50 frames a 21x17 building with context.
- Env is read once at startup — relaunch to move the pin. Placement is deterministic
  per seed+flags, so a wrong guess costs one probe run: read the tile from the log,
  relaunch with the corrected pin.
- Example (the demo quarry verification):
  `BANISHED_SEED=12345 BANISHED_BUILD_DEMO_QUARRY=1 BANISHED_CAMERA_PIN=9.5,-0.5,50`

### Standalone Build Architecture

`make standalone` / `make standalone-debug`
- Compiles `code/Game/` + `engine/funcs/` + `runtime/` + `standalone/` into `reimpl-x64.exe`
- NO hooks, NO reimpl wrappers, NO proxy DLL — the original Application-x64.exe is not involved
- `ResolveAddr(Module::Exe, addr)` queries an address registry of our reimplemented functions
- Still loads Runtime-x64.dll + VideoDX11-x64.dll (engine DLLs, ASLR handled normally)

**Key files:**
- `standalone/resolve.h` — ResolveAddr/ResolveExport/ResolveOrdinal declarations
- `standalone/resolve_standalone.cpp` — standalone ResolveAddr + ResolveInit (loads DLLs via LoadLibrary)
- `standalone/address_registry.cpp` — maps Ghidra exe addresses to our reimplemented code
- `standalone/main.cpp` — WinMain entry point (unattended-boot registry guard, VEH crash handler, log redirection)
- `standalone/autostart.cpp` — menu→New Game auto-driver (see Standalone Test Driver above)

**Address registry workflow:** As each exe function gets reimplemented in `code/Game/`, register it
in `standalone/address_registry.cpp` so standalone mode can find it. Three categories:
1. **Code functions** (0x140001xxx-0x14014Fxxx): `RegisterExeAddr(addr, (uintptr_t)&OurFunc)`
2. **IAT dispatch slots** (0x14015xxxx): `RegisterDispatchSlot(addr, runtime_fn_ptr)` — provides .rdata slot storage.
   These are real PE imports: `tools/dispatch_slot_map.py <addr>` tells you the exact
   mangled name, and `--check` verifies every registration (keep it at zero mismatches).
   Read the call sites before registering a null slot — some are null on purpose.
3. **Vtables + type IDs** (0x14015Exxx-0x1401Cxxxx): static vtable arrays + uint32_t storage

### Code Organization

The `code/Game/` directory mirrors the original source tree structure discovered from
profile build debug strings (project codename "Buildy", `C:\Projects\Buildy\code\Game\`).
Alongside it: `engine/types/` (binary-compatible structs), `engine/funcs/` (resolved
engine function pointers), `runtime/` (Runtime-x64.dll types/funcs + early reimpl),
`standalone/` (exe entry point, address registry, reg_*.cpp registration waves).

### Address Resolution
- `ResolveAddr(Module::Exe, ghidra_addr)` — exe addresses (base 0x140000000)
- `ResolveAddr(Module::Runtime, ghidra_addr)` — Runtime-x64.dll (base 0x180000000)
- `ResolveExport("?mangled@name")` — resolve by MSVC mangled name from Runtime DLL exports
- `ResolveOrdinal(N)` — resolve by ordinal from Runtime DLL exports
- All resolved at runtime after DLL loads; handles ASLR automatically

### Reimplementation Workflow (per function)
1. **Decompile** in Ghidra → rename, annotate, document struct layout
2. **Type header** — add/update binary-compatible struct in `engine/types/` with `static_assert` on every field offset
3. **Resolve deps** — add any engine functions the reimpl needs to `engine/funcs/engine_funcs.h`
4. **Write clean C++** in `code/Game/<Module>/` (e.g., `code/Game/Component/animal.cpp`)
5. **Register** the exe address in `standalone/address_registry.cpp` (or a `standalone/reg_*.cpp` wave file) so `ResolveAddr(Module::Exe, addr)` finds it
6. **Test** — `mingw32-make sa-run`, read `C:/GOG Games/Banished/standalone_debug.log`

### Don't "fix" a faithful crash with a null check

The port is mature enough that when a byte-faithful function crashes on an unguarded
deref, **retail is not wrong — something upstream handed us a null.** Find that instead.
Retail's unguarded derefs are load-bearing: it relies on the value always being there,
so a guard doesn't restore the behaviour, it skips it.

Add a guard only where our standalone genuinely differs from retail (e.g. an entity that
legitimately lacks a component because the panel template is shared), and say so in the
comment. Otherwise fix the upstream null and leave the deref faithful.

### ABI Compatibility Notes
- MinGW GCC x64 uses the same Microsoft x64 ABI as MSVC (one calling convention on Win64)
- Type headers use plain structs with explicit padding (no C++ inheritance/vtables)
- `static_assert(sizeof/offsetof)` on every struct catches layout mismatches at compile time
- Virtual calls go through raw function pointers read from vtable at offset 0

### Debug Logs
All logs land in `C:/GOG Games/Banished/` (the working directory at launch):
- `standalone_debug.log` — stdout (TRACE/printf output, unbuffered)
- `standalone_error.log` — stderr (survives abnormal exit; env-gated diagnostics print here)
- `standalone_crash.log` — VEH crash dumps (see below)
- `standalone_boot_state.txt` — one-line machine-readable boot state (see below)

### Boot State — check this before concluding "it's still loading"

**A quiet log does not mean the game is working.** The launcher dialog
(`Engine::PlatformConfigure`) is no longer patched out — retail shows it whenever the
previous run did not finish `Engine::Startup`, which in development means every time the
process is killed. When it appears the process stays **alive and pumping messages** while
`standalone_error.log` stops mid-boot, which looks exactly like a slow load. Any other
modal Win32 dialog (assert, driver prompt) blocks boot identically.

```bash
tools/boot_state.sh              # one status line
tools/boot_state.sh --wait 300   # block until RUNNING; bails INSTANTLY if blocked
```

Exit codes: `0` RUNNING · `1` NOT_RUNNING · `2` BLOCKED_DIALOG · `3` BOOTING ·
`4` STALE (state file left by a pid that is gone).

**Always poll boot with this rather than `grep -q` on the log** — a `grep`-until-timeout
loop against a blocked boot burns the entire timeout for nothing. Published every second
by `BootState::Start` (`standalone/boot_state_watchdog.cpp`); also logged as
`[BootState]` lines, repeated every 15s while blocked so a log tail never goes silent.

`HarnessGuardUnattendedBoot` already clears the arming bit when `BANISHED_AUTOSTART` /
`BANISHED_AUTOQUIT` is set, so unattended runs normally boot clean — the watchdog is for
when that guard does not apply (attended runs) or does not hold.

### Crash Diagnostics
- VEH handler (`standalone/main.cpp`) catches ACCESS_VIOLATION/ILLEGAL_INSTRUCTION and fails fast to `standalone_crash.log` with fault address, registers, and stack trace
- **Read `standalone_crash.log` FIRST** when the exe dies — `[REDZONE-DUMP]` lines in the debug log can mislead; `EXEC-of-0` means a null function pointer was called
- Symbolize addresses with `nm` against `standalone/build/reimpl-x64.exe` (image base 0x140000000)

## VS Code Launch Configurations

Debug/attach configurations live in `.vscode/launch.json`.

### Compiler & Toolchain
- GCC 13.1 (MSYS2 UCRT64) at `C:\msys64\ucrt64\bin\`
- `mingw32-make` for builds (full path: `/c/msys64/ucrt64/bin/mingw32-make.exe`)
- C++17 standard, static-linked runtime

### Debugging via MCP
- Use `vscode-debugger` MCP for breakpoints, stepping, variables, stack traces
- Use `vscode` MCP's `start_debug_session` to launch (config: "Launch Standalone (debug, autostart new game)")
- **Important**: `workspaceFolder` must use lowercase drive letter (`c:\...`)
- See `.vscode/launch.json` for full debug configurations

---

## Ghidra Setup
- **Ghidra 12.0.3** at `C:\tools\ghidra_12.0.3_PUBLIC`
- Project holds 7 programs, including `Runtime-x64-profile.dll` (ToolKit build, debug strings
  + 5,182 exports) — the primary source for naming unexported Runtime internals
- **GhidraMCP** (bethington/ghidra-mcp v3.0.0): 179 MCP tools
- Bridge: `tools/ghidra_mcp/bridge_mcp_ghidra.py`
- Python 3.10 venv: `tools/ghidra_mcp/.venv/`
- Extension ZIP: `tools/ghidra_mcp/GhidraMCP-3.0.0.zip`
- See `HOW_TO_START_GHIDRA.md` for startup steps

## Naming Conventions for Ghidra
When renaming functions and data, use the recovered namespace style:
- Functions: `Namespace_ClassName_MethodName` (e.g., `Engine_MainLoop_Run`)
- Vtable entries: `vftable_ClassName` or `vtable_Namespace_ClassName`
- Globals: `g_Namespace_variableName`
- Components: preserve the full component name (e.g., `CitizenAIComponent_Update`)

## Environment Notes
- JAVA_HOME: `C:\Program Files\Java\jdk-24` (for Ghidra)
- Python 3.9.10 system-wide, 3.10.11 in project venv (`tools/ghidra_mcp/.venv/`)
- `uv` v0.9.24 available for Python management
- Node.js + npx available (for vscode MCP server)
- MSYS2 packages are outdated (stale signatures) - avoid `pacman -S` for new installs