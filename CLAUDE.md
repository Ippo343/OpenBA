# Banished x64 Reverse Engineering - Progressive Reimplementation

## Project Goal
Reverse engineer Banished (by Shining Rock Software) and **progressively replace game
functions with clean, readable C++ source code**. Each decompiled function gets a
binary-compatible type header and a reimplementation that hooks the original at runtime.
The end goal is a readable C++ codebase that mirrors the game's logic, similar to
projects like OpenRCT2, re3 (GTA III), and devilution (Diablo).

## Target Binaries

The game is a multi-binary architecture. The exe contains game logic; the engine runtime is in DLLs.

### Application-x64.exe (Game Logic)
- **File**: `C:\GOG Games\Banished\Application-x64.exe`
- **Architecture**: x86-64, Little Endian, Windows (MSVC)
- **Base Address**: `0x140000000`
- **Entry Point**: `0x14003b1c0`
- **Code Section**: `.text` `0x140001000` - `0x1401501ff` (~1.4 MB)
- **Size**: 1.9 MB | **Functions**: 10,476 | **Symbols**: 50,420
- **Exports**: `NvOptimusEnablement`, `AmdPowerXpressRequestHighPerformance` (GPU preference flags)
- **PE Timestamp**: `2017-09-10 16:34:27`

### Runtime-x64.dll (Engine Core)
- **File**: `C:\GOG Games\Banished\Runtime-x64.dll`
- **Size**: 2.7 MB | **Functions**: 15,290 | **Image Base**: `0x180000000`
- **PE Timestamp**: `2017-09-10 16:34:14`
- **Exports**: ~4,800+ ordinals (full engine API surface, reflection-based with consistent TypeName/Factory/Lifecycle pattern per class)
- **Role**: Complete custom game engine runtime. 18 namespaces, 100+ classes covering object model, math, rendering, UI (20+ widgets), file I/O with packages/mods, 2D collision, 3D picking, A* pathfinding, achievements, save/load, input (DirectInput), threading
- **Imports**: KERNEL32, USER32, GDI32, ADVAPI32, SHELL32, OLE32, DINPUT8, DBGHELP
- **Note**: NOT imported via PE import table. Exe uses a custom runtime dispatch mechanism (function pointer tables in .rdata filled by name resolution at startup).

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
Loop types: `LoaderLoop`, `DialogLoaderLoopT`, `DialogLoopT` (menus/loading), plus the main gameplay `UpdateLoop`.

### Launcher Dialog
`Engine::Engine::PlatformConfigure` (0x1800a1670) — Win32 launcher with Options/Video/Play buttons. Hooked to always return true (skip), since the "already configured" flag doesn't persist correctly. See `engine/reimpl/platformconfigure.cpp`.

### Threading Model
- `System::Thread` with `Start()`, `WaitForFinish()`, `OnCompleted()` virtual
- `Application::YieldToThreads()` - cooperative yielding
- Windows ThreadPool API (CreateThreadpoolTimer, CreateThreadpoolWait)
- Critical sections for synchronization (InitializeCriticalSectionAndSpinCount)

---

## Type & Function Inventory

Full decompilation status tracking is in **[DECOMP_STATUS.md](DECOMP_STATUS.md)**.

Covers all three binaries: Application-x64.exe (10,476 functions), Runtime-x64.dll (15,290 functions), VideoDX11-x64.dll (576 functions). Includes engine classes, ECS components, key identified functions, and per-binary breakdowns.

---

## Project Organization Decisions

### Ghidra is the primary documentation tool
All function renaming, type annotation, struct layouts, and per-function comments go directly in Ghidra. It's persistent, searchable, and stays in sync with the actual analysis. External markdown files are for higher-level notes that don't fit in Ghidra (architecture diagrams, algorithm analysis, session logs).

### Decompilation priority order

1. **Engine** (MainLoop/UpdateLoop) - understand the tick structure and frame budget
2. **ECS core** (ComponentEntity update loop) - find the per-frame iteration pattern
3. **AI/Pathfinding** - CitizenAIComponent + Pathfinding::Scene likely the biggest CPU sink in late-game
4. **Economy** (Storage/Market) - suspected O(n) lookups across all buildings
5. **Graphics** (Renderer/DrawItems) - batching and draw call optimization opportunities

---

## Future Hotspot Hypotheses
1. **Citizen AI / Pathfinding** - CitizenAIComponent + Pathfinding::PointList likely dominate CPU in large towns
2. **Storage/Market lookups** - StorageComponent + MarketComponent scans may be O(n) across all buildings
3. **State machine ticks** - StateMachineComponent updates every citizen every frame
4. **Rendering batching** - Renderer::AddDrawItem per-object overhead with many buildings
5. **Shadow mapping** - SunLight::DrawShadowMap with large visible areas

## Modding Toolkit Reference
- Toolkit: https://banishedinfo.com/mods/resources/gettingstarted
- Developer build: `Application-x64-profile.exe` (has profiling hooks)
- Resource compiler: `Tools-x64.exe` compiles `.rsc` -> binary in `/bin/WinData`
- Mod packages: `.pkm` files (delta-packed against base game data)
- Resource loading order: loose files -> `.pkg` (game) -> `.pkm` (mods)
- The toolkit provides a **profile build** that may expose timing/debug info

## Patching Strategy
Proxy DLL (`version.dll`) dropped next to the game exe. Auto-loads with the game,
forwards all real version.dll calls to system32, and runs our hook code on startup.

### Build & Deploy
```bash
mingw32-make debug     # build debug DLL (with console output)
mingw32-make release   # build release DLL
mingw32-make deploy    # build + copy to Banished directory
mingw32-make run       # build + copy + launch game
mingw32-make undeploy  # remove from Banished directory
mingw32-make clean     # remove build artifacts
```

### Reimplementation Workflow (per function)
1. **Decompile** in Ghidra → rename, annotate, document struct layout
2. **Type header** — add binary-compatible struct in `engine/types/` with `static_assert` on every field offset
3. **Resolve deps** — add any engine functions the reimpl needs to `engine/funcs/engine_funcs.h`
4. **Reimplement** — write clean C++ in `engine/reimpl/`, hook original via `InstallHook`
5. **Register** — add `ReimplEntry` to the table in `hooks.cpp` (extern-declare Install/Remove in hooks.cpp)
6. **Test** — `mingw32-make deploy`, launch game, read `C:/GOG Games/Banished/patch_debug.log` for debug output
7. **Enable incrementally** — start with `enabled=true` for the core function, `false` for others. Flip on one at a time.

### Address Resolution
- `ResolveAddr(Module::Exe, ghidra_addr)` — exe addresses (base 0x140000000)
- `ResolveAddr(Module::Runtime, ghidra_addr)` — Runtime-x64.dll (base 0x180000000)
- `ResolveExport("?mangled@name")` — resolve by MSVC mangled name from Runtime DLL exports
- `ResolveOrdinal(N)` — resolve by ordinal from Runtime DLL exports
- All resolved at runtime after DLL loads; handles ASLR automatically

### Reimplementation Pattern
```cpp
// 1. Typedef the original function signature
typedef bool (*OrigFunc_t)(MainLoop* self, bool drawOnly);
// 2. HookInfo for trampoline to original
static HookInfo g_hook;
// 3. Our reimplementation
static bool Reimpl(MainLoop* self, bool drawOnly) {
    // Clean C++ using typed struct access
    self->m_accumulator += elapsed;
    // Can call original via trampoline for A/B testing:
    // return ((OrigFunc_t)g_hook.trampoline)(self, drawOnly);
}
// 4. Install/Remove registered in hooks.cpp ReimplEntry table
```

### ABI Compatibility Notes
- MinGW GCC x64 uses the same Microsoft x64 ABI as MSVC (one calling convention on Win64)
- Type headers use plain structs with explicit padding (no C++ inheritance/vtables)
- `static_assert(sizeof/offsetof)` on every struct catches layout mismatches at compile time
- Virtual calls go through raw function pointers read from vtable at offset 0

### Debug Log
- In debug builds, stdout is redirected to `C:/GOG Games/Banished/patch_debug.log` (unbuffered)
- Read this file after `mingw32-make deploy && start Application-x64.exe` to check hook output
- The console window still opens (AllocConsole) but output goes to the file for automation

### Reimpl Gotchas (lessons learned)
- **MSVC mangled names**: Always verify against actual DLL exports using Ghidra `search_functions`. Access specifier matters: `QEAA` = public, `AEAA` = private. Return type matters: `X` = void, `_N` = bool. The `@1@` backreference means "same name as class" (e.g., `Application::Application`).
- **Null function pointers**: `ResolveExport()` returns NULL if the mangled name is wrong. Always null-check function pointers before calling, or the game crashes with no useful error.
- **Ghidra decompilation quirks**: `this[0x19]` in Ghidra output means byte access at offset 0x19, NOT array indexing. Empty `do {} while` loops are usually decompiler artifacts for simple conditionals. Missing function arguments in virtual calls are common (Ghidra loses `this` pointer tracking).
- **Hook chaining**: When multiple MainLoop methods are hooked, calling one from another through the resolved runtime address correctly chains through hooks. Frame calls Update/Draw via `g_addr_Update`/`g_addr_Draw` — if those are hooked, our reimpl runs; if not, the original runs.
- **Virtual call helpers**: Use typed inline wrappers (e.g., `VCall_Update`, `VCall_Draw`) for UpdateLoop vtable calls. Explicit `this` pointer as first arg, correct Win64 parameter types.
- **Engine sub-objects**: EngineInstance (at `MainLoop::m_engine`) holds pointers to subsystems at fixed offsets: `+0x690` = Application, `+0x6d0` = Audio::Device, `+0x6e0` = Input::Device.

---

## MCP Server Stack

Three MCP servers configured in `.mcp.json`. All are available to Claude Code automatically.

| Server | Source | Transport | Port | Purpose |
|--------|--------|-----------|------|---------|
| `ghidra-mcp` | bethington/ghidra-mcp v3.0.0 | stdio | 8089 | 179 RE tools: decompile, rename, analyze, xrefs, scripts, batch ops |
| `vscode-debugger` | rauschit/vscode-debugger-mcp-server v1.0.8 | SSE | 10101 | Full debugger control via DAP: breakpoints, stepping, variables, stack traces, watches, expression eval. Works with any VS Code debug adapter including cppdbg/GDB. |
| `vscode` | acomagu/vscode-as-mcp-server v0.0.25 | stdio (npx) | - | Execute any VS Code command: run tasks, launch debug, open files |

### Prerequisites for MCP servers
- **ghidra-mcp**: Ghidra must be open with binary loaded + GhidraMCP server started (Tools > GhidraMCP > Start MCP Server). See `HOW_TO_START_GHIDRA.md`.
- **vscode-debugger**: VS Code must be open with extension installed (`rauschit.vscode-debugger-mcp-server`). Auto-starts SSE on port 10101.
- **vscode**: VS Code must be open with this project. Connects via npx.

### MCP server config location
- Project-level: `.mcp.json` (shared, checked into repo)
- VS Code-level: `.vscode/mcp.json` (for VS Code's own MCP support)

---

## VS Code Launch Configurations

Defined in `.vscode/launch.json`:

| Configuration | Type | What it does |
|---------------|------|-------------|
| **Attach to Banished** | attach | Attach GDB to running game process (pick from list) |
| **Launch Banished (with patch)** | launch | Builds, deploys DLL, launches game with GDB attached |
| **Launch Banished (no debug)** | launch | Builds, deploys DLL, launches game without stopping on signals |

### VS Code Build Tasks (`.vscode/tasks.json`)
- **Build (Debug)** - default build task (Ctrl+Shift+B)
- **Build (Release)** - optimized build
- **Deploy to Banished** - build + copy DLL
- **Remove from Banished** - delete proxy DLL from game dir
- **Clean** - remove build artifacts

### Compiler & Toolchain
- GCC 13.1 (MSYS2 UCRT64) at `C:\msys64\ucrt64\bin\`
- `mingw32-make` for builds (full path: `/c/msys64/ucrt64/bin/mingw32-make.exe`)
- C++17 standard, static-linked runtime

### Debugging via MCP (Claude Code workflow)

All debugger operations go through `vscode-debugger` MCP (rauschit/vscode-debugger-mcp-server).
The `vscode` MCP is used only for launching the debug session (start_debug_session).

**1. Set breakpoints** (`vscode-debugger`):
```
mcp__vscode-debugger__set_breakpoint(
  file: "C:\\code\\GIT\\untitled_rev_eng_project\\patches\\src\\dllmain.cpp",
  line: 221
)
```

**2. Launch the game under GDB** (`vscode` MCP):
```
mcp__vscode__start_debug_session(
  workspaceFolder: "c:\\code\\GIT\\untitled_rev_eng_project",   // lowercase drive letter!
  configuration: {
    "name": "Launch Banished (with patch)",
    "type": "cppdbg",
    "request": "launch",
    "program": "C:/GOG Games/Banished/Application-x64.exe",
    "cwd": "C:/GOG Games/Banished",
    "MIMode": "gdb",
    "miDebuggerPath": "C:/msys64/ucrt64/bin/gdb.exe",
    "preLaunchTask": "Deploy to Banished",
    "additionalSOLibSearchPath": "C:/code/GIT/untitled_rev_eng_project/patches/build",
    "setupCommands": [{"description": "Enable pretty-printing", "text": "-enable-pretty-printing"}]
  }
)
```
Note: `workspaceFolder` must use **lowercase drive letter** (`c:\...`) or VS Code won't find it.

**3. Debugger control** (`vscode-debugger`):
- `continue`, `pause`, `step_over`, `step_in`, `step_out`, `stop`

**4. Inspect state** (`vscode-debugger`):
- `get_variables` — read locals/globals at current frame
- `evaluate_expression` — eval any expression (including GDB `-exec` commands)
- `get_stack_trace` — call stack
- `get_threads`, `get_scopes` — thread/scope info
- `add_watch` / `list_watches` — persistent watch expressions

**5. Cleanup**:
```
mcp__vscode-debugger__clear_all_breakpoints()
mcp__vscode__stop_debug_session(sessionName: "Launch Banished (with patch)")
```

---

## Ghidra Setup
- **Ghidra 12.0.3** at `C:\tools\ghidra_12.0.3_PUBLIC`
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

## Project Structure
```
untitled_rev_eng_project/
  CLAUDE.md                      # this file - project reference (architecture, tools, workflow)
  DECOMP_STATUS.md               # full type/function inventory & decompilation status
  HOW_TO_START_GHIDRA.md         # Ghidra startup guide
  Makefile                       # build system (mingw32-make, multi-dir with obj files)
  .mcp.json                      # MCP server config (3 servers)
  .vscode/
    tasks.json                   # build/deploy tasks
    launch.json                  # debug launch configs (3 configs)
    settings.json                # DebugMCP port, compiler settings
    c_cpp_properties.json        # IntelliSense config
    mcp.json                     # VS Code MCP server registry
  tools/
    ghidra_mcp/                  # GhidraMCP bridge + extension
      bridge_mcp_ghidra.py       # MCP<->Ghidra HTTP bridge (179 tools)
      GhidraMCP-3.0.0.zip        # Ghidra extension (install once)
      requirements.txt           # Python deps (mcp, requests)
      .venv/                     # Python 3.10 virtual environment
  patches/
    src/
      dllmain.cpp                # proxy DLL entry point + init sequence
      proxy.cpp / proxy.h        # version.dll forwarding (17 exports)
      proxy.def                  # export definitions
      resolve.cpp / resolve.h    # multi-binary address resolution (exe + DLLs)
      hooks.cpp / hooks.h        # hook infrastructure + reimplementation registry
    build/
      version.dll                # compiled patch DLL (~288K debug)
      obj/                       # intermediate object files
  engine/
    types/                       # binary-compatible struct definitions
      common.h                   # Time, Vector3, PostCallback, forward decls
      MainLoop.h                 # Engine::MainLoop (0xA0 bytes, all offsets verified)
      UpdateLoop.h               # Engine::UpdateLoop base + vtable layout
    funcs/                       # resolved engine function pointers
      engine_funcs.h             # function pointer declarations
      engine_funcs.cpp           # runtime resolution (by name, RVA, or ordinal)
    reimpl/                      # reimplemented functions (auto-compiled by Makefile)
      mainloop.cpp               # Engine::MainLoop (Run, Frame, Update, Draw, SetNextLoop)
  docs/                          # function documentation exports
  notes/                         # daily RE session notes (NOTESMMDDYY.md)
```
