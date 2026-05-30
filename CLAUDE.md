# Banished x64 Reverse Engineering - Progressive Reimplementation

## Project Goal
Reverse engineer Banished (by Shining Rock Software) and **progressively replace game
functions with clean, readable C++ source code**. Each decompiled function gets a
binary-compatible type header and a reimplementation that hooks the original at runtime.
The end goal is a readable C++ codebase that mirrors the game's logic and can be compiled into a replacement exe, similar to projects like OpenRCT2, re3 (GTA III), and devilution (Diablo).

## Current Phase: Deep Reimplementation

All 232 exe classes are covered with passthrough hooks (813 enabled, 4 disabled).
Now converting passthroughs to real C++ game logic, module by module.

### Goals (in order):
1. **Deep reimplementation** — convert all passthrough wrappers in `engine/reimpl/` to real C++ in `code/Game/`, organized by module matching the original source tree. Decompile each function in Ghidra, write clean C++ equivalent.
2. **Standalone exe** — once all game logic is in `code/Game/`, compile it as a replacement Application-x64.exe that links against Runtime-x64.dll + VideoDX11-x64.dll.
3. **Runtime DLL** — after the exe is solid, start reimplementing Runtime-x64.dll functions.

### Module priority for deep reimpl:
1. **Components with Update** — CitizenAI, Citizen, StateMachine, Character, Animal, Herd, etc. (highest gameplay impact, some already partially done)
2. **Actions & Work** — StateMachine action/work classes (citizen behavior)
3. **Game systems** — Weather, Scenario, WorldGen, Tutorial, Achievement
4. **UI** — Dialogs and Panels (mostly data binding, lower priority)
5. **Tools** — Placement/building tools

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

Full decompilation status tracking is in the **[decomp/](decomp/README.md)** directory, split per-binary:

- **[decomp/application.md](decomp/application.md)** — Application-x64.exe (10,476 functions: ECS components, UI controllers, game logic)
- **[decomp/runtime.md](decomp/runtime.md)** — Runtime-x64.dll (15,290 functions: 18 namespaces, 100+ classes)
- **[decomp/videodx11.md](decomp/videodx11.md)** — VideoDX11-x64.dll (576 functions: DX11 renderer plugin)

---

## Project Organization Decisions

### Ghidra is the primary documentation tool
All function renaming, type annotation, struct layouts, and per-function comments go directly in Ghidra. It's persistent, searchable, and stays in sync with the actual analysis. External markdown files are for higher-level notes that don't fit in Ghidra (architecture diagrams, algorithm analysis, session logs).

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
# DLL mode (proxy DLL, hooks into original exe)
mingw32-make debug     # build debug DLL (with console output)
mingw32-make release   # build release DLL
mingw32-make deploy    # build + copy to Banished directory
mingw32-make run       # build + copy + launch game
mingw32-make undeploy  # remove from Banished directory

# Standalone EXE mode (replacement exe, links against Runtime DLL)
mingw32-make standalone        # build release standalone exe
mingw32-make standalone-debug  # build debug standalone exe
mingw32-make sa-deploy         # build debug + copy to Banished directory
mingw32-make sa-run            # build debug + copy + launch
# Output: standalone/build/reimpl-x64.exe

mingw32-make clean     # remove all build artifacts (both modes)
```

### Dual-Mode Build Architecture

The project supports two build modes from the same `code/Game/` source files:

**DLL mode** (default) — `make debug` / `make deploy`
- Compiles `code/Game/` + `engine/reimpl/` + `patches/src/` into `version.dll`
- Hooks original exe at runtime via trampoline patching
- `ResolveAddr(Module::Exe, addr)` applies ASLR delta to find functions in loaded exe
- Used for incremental development and testing

**Standalone mode** — `make standalone` / `make standalone-debug`
- Compiles `code/Game/` + `engine/funcs/` + `standalone/` into `Application-x64.exe`
- NO hooks, NO reimpl wrappers, NO proxy DLL
- `ResolveAddr(Module::Exe, addr)` queries an address registry of our reimplemented functions
- Still loads Runtime-x64.dll + VideoDX11-x64.dll (engine DLLs, ASLR handled normally)
- Controlled by `-DSTANDALONE_EXE` define (set automatically by Makefile)

**Key files:**
- `patches/src/resolve.h` — shared header, `#ifdef STANDALONE_EXE` switches inline implementation
- `standalone/resolve_standalone.cpp` — standalone ResolveAddr + ResolveInit (loads DLLs via LoadLibrary)
- `standalone/address_registry.cpp` — maps Ghidra exe addresses to our reimplemented code
- `standalone/main.cpp` — WinMain entry point (replaces DllMain)

**Address registry workflow:** As each exe function gets reimplemented in `code/Game/`, register it
in `standalone/address_registry.cpp` so standalone mode can find it. Three categories:
1. **Code functions** (0x140001xxx-0x14014Fxxx): `RegisterExeAddr(addr, (uintptr_t)&OurFunc)`
2. **IAT dispatch slots** (0x14015xxxx): `RegisterDispatchSlot(addr, runtime_fn_ptr)` — provides .rdata slot storage
3. **Vtables + type IDs** (0x14015Exxx-0x1401Cxxxx): static vtable arrays + uint32_t storage

**Progress:** 582 unique exe addresses referenced by `code/Game/`, 0 currently registered (0%).
Run standalone exe to see current coverage stats.

### Deep Reimplementation Workflow (per function)
1. **Decompile** in Ghidra → rename, annotate, document struct layout
2. **Type header** — add/update binary-compatible struct in `engine/types/` with `static_assert` on every field offset
3. **Resolve deps** — add any engine functions the reimpl needs to `engine/funcs/engine_funcs.h`
4. **Write clean C++** in `code/Game/<Module>/` (e.g., `code/Game/Component/animal.cpp`)
5. **Update hook wrapper** in `engine/reimpl/` to call the clean src function instead of trampoline
6. **Test** — `mingw32-make deploy`, launch game, read `C:/GOG Games/Banished/patch_debug.log` for debug output

### Code Organization (two-layer architecture)

The `code/Game/` directory mirrors the original source tree structure discovered from
profile build debug strings (project codename "Buildy", `C:\Projects\Buildy\code\Game\`).

```
code/Game/           — Clean game logic (mirrors original source tree, future standalone exe)
  Action/            — StateMachine actions (17 classes: MoveTo, PerformWork, etc.)
  Component/         — ECS components (60 classes: AI, economy, building, etc.)
  Dialog/            — UI dialogs + panels (50+ classes)
  Entity/            — ComponentEntity, CitizenBehavior, EntityList
  Game/              — Core systems (Game, Map, Pathfinder, StateMachine, Weather, etc.)
  Goal/              — Goal classes (BuildingGoal, PopulationGoal, etc.)
  Player/            — Placement tools (9 classes: BridgeTool, PlacementTool, etc.)
  Tutorial/          — Tutorial classes (17 classes)
  Work/              — Work classes (21 classes: BuildRoad, FishingWork, etc.)

engine/reimpl/       — Hook wrappers (thin, delegate to code/Game/ or trampoline)
engine/types/        — Binary-compatible struct definitions (142 headers)
engine/funcs/        — Resolved engine function pointers
```

The reimpl layer is temporary scaffolding. As each function gets a real implementation
in code/Game/, the reimpl wrapper switches from calling the trampoline to calling
the clean code. When ALL functions are reimplemented, code/Game/ compiles as a
standalone exe without the reimpl/hooks layer.

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
- **Ghidra ADD vs dereference**: `ADD RCX, 0x48` is pointer arithmetic (passes address of field), NOT `MOV RCX, [RCX+0x48]` (dereference). Critical distinction — getting this wrong passes the wrong pointer level (e.g., StateMachineComponent::Update).
- **Hook chaining**: When multiple MainLoop methods are hooked, calling one from another through the resolved runtime address correctly chains through hooks. Frame calls Update/Draw via `g_addr_Update`/`g_addr_Draw` — if those are hooked, our reimpl runs; if not, the original runs.
- **Virtual call helpers**: Use typed inline wrappers (e.g., `VCall_Update`, `VCall_Draw`) for UpdateLoop vtable calls. Explicit `this` pointer as first arg, correct Win64 parameter types.
- **Engine sub-objects**: EngineInstance (at `MainLoop::m_engine`) holds pointers to subsystems at fixed offsets: `+0x690` = Application, `+0x6d0` = Audio::Device, `+0x6e0` = Input::Device.
- **Non-trivial return ABI for member functions**: MSVC x64 member functions returning structs >8 bytes (e.g., `InternalPtr<T>`) use a hidden return pointer, but the parameter order is **RCX=this, RDX=hidden_retval, R8+=args**. This is different from free/static functions where **RCX=hidden_retval, RDX+=args**. When writing typedefs for these, the first param must be `self`, second must be `retval`. Getting this backwards causes hardfaults. Affected functions: `fn_FindElement`, `fn_Element_FindElementType`, `fn_Element_Child`, `fn_DialogInstance_FindElement`, `fn_DialogInstance_GetController`, `fn_DialogInstance_FindElementType`. Verify in Ghidra disassembly: `MOV RCX,[self]` then `LEA RDX,[retval]` confirms member function order.
- **External reference indirection levels**: External references (Object::ExternalPtr) stored in .rdata dispatch slots require careful deref counting. `g_pKeyMapObjectExref` needs **triple deref** (`***(void****)ptr`) to reach the actual object: slot→External→wrapped ptr→object. Always verify in disassembly: count the chain of `MOV REG,[REG]` instructions. Getting the level wrong passes an External wrapper or container as the actual object, causing field reads to hit garbage.
- **Element field vs vtable field access**: `*(void**)(element + 0xB8)` reads field at offset 0xB8 of the element. `*(void**)(*(uintptr_t*)element + 0xB8)` reads offset 0xB8 from the VTABLE (wrong!). The `*(uintptr_t*)element` dereferences the vtable pointer first. Telltale sign: reading ASCII string data like "itDescri" as a pointer — vtables sit in .rdata near type name strings.
- **fn_UIString_Ctor dropped R8**: Ghidra frequently shows `fn_UIString_Ctor(retbuf, externalPtr)` with only 2 visible args, but the function ALWAYS takes 3: `(retbuf, externalPtr, stringName)`. R8 is set by a prior `LEA R8,[addr]` that Ghidra doesn't associate with the call. **Always check disassembly** for `LEA R8` before any `fn_UIString_Ctor` call. Using the wrong 2-arg `fn_UIString_Ctor_Entry` crashes in `StringTable::GetIndexFromName`.
- **FindComponent type IDs**: Different component subtypes have different type IDs. ResidenceComponent (0x1401C7CF0) ≠ StorageComponent (0x1401C7E78) even though Residence extends Storage. Always verify the type ID in the original disassembly (`MOV EDX,[addr]` before the FindComponent call). Wrong type ID finds the wrong component with different field offsets.
- **fn_MemFree is never resolved**: `fn_MemFree(singleton, ptr)` is declared but never initialized — always NULL. Use `fn_MemFreeStatic(ptr)` instead (the static wrapper that gets the singleton internally). Both do the same thing.
- **MSVC const mangling**: `QEAA` = public non-const, `QEBA` = public const. `ResolveExport` silently returns NULL on mismatch. Always add a fallback `ResolveAddr` after `ResolveExport` for critical functions.
- **Table element child access**: `UI::TableElement` children are accessed via **flat index** `child(colCount*row + col)`, where the **column-count stride is at `+0xC4`** and the **row count is at `+0xC0`** (SetSize stores `ceil(count/colCount)` at +0xC0 and `colCount` at +0xC4; `code/Game/Dialog/tradeui.cpp` exposes these as `ReadTableRowCount`/`ReadTableColCount`). NOT two-level row→column. The index stride is `colCount` (+0xC4). Verify child indices in disassembly — they may skip indices (e.g., 0,1,3,4 not 0,1,2,3).
- **InternalBasePtr CheckPtr before field access**: Always call `fn_InternalBasePtr_CheckPtr` on an IBP before reading fields through it (the original code does this). CheckPtr may resolve lazy/weak pointers. Skipping it can give stale or zero values.

### Trampoline Bug Classes (9 discovered, all fixed)
Hooks copy N prologue bytes to a trampoline, overwrite original with JMP. See `notes/NOTES030126.md` for full details.
1. **Split instructions** — prologue size doesn't land on instruction boundary. Fix: verify in Ghidra.
2. **Relative branches/calls** — PC-relative instructions (JNZ, CALL E8, JMP rel) break when relocated. **FIXED**: `InstallHook()` auto-detects relative instructions via HDE64 length disassembler (from MinHook, BSD-2) and routes to `InstallHookRelocating()` which relocates CALL rel32, JMP rel32/rel8, Jcc rel8/rel32, and RIP-relative addressing. Uses a data area at the end of the 256-byte trampoline buffer for absolute address indirection. 41 hooks re-enabled.
3. **RSP captures (MOV RAX/R11,RSP)** — captures wrong RSP value, used for absolute addressing. **FIXED**: `InstallHook()` scans prologue for `MOV RAX,RSP` (48 8B C4) / `MOV R11,RSP` (4C 8B DC) and inserts `ADD RAX,8` / `ADD R11,8` after each to compensate for the CALL-induced RSP shift. Also applied in `InstallHookRelocating()` for functions with both bug classes. 18 hooks re-enabled.
4. **RSP-relative stores (MOV [RSP+xx],reg)** — safe for passthroughs (write/read internally consistent). No fix needed.
5. **RAX clobbered by JMP-back** — trampoline's `MOV RAX, imm64; JMP RAX` overwrites values prologue set in RAX. Fix: changed to `MOV R10; JMP R10`.
6. **Implicit register forwarding** — original function passes a volatile register (e.g., RDX) through to a sub-call without explicitly referencing it. The C passthrough's compiler-generated code may clobber that register. Fix: declare and forward all implicitly-passed parameters in the passthrough typedef. Examples: ModelComponent vtable slot +0x40 forwards RDX (Time*) to MeshInstance::Update — passthrough must be `(void* self, void* time)`, not `(void* self)`. Action executor (FUN_1400c4c50) passes RDX=actionStack to AbstractImpl(+0x20)/Exit(+0x38), and RDX=actionStack + R8=time to Execute(+0x28) — all action passthroughs must declare these params.
7. **Bool return type on action reimpl** — action reimpl functions returning `bool` leave garbage in upper EAX bytes. The action executor (FUN_1400c4c50) checks returns as `int` (comparing with 0, 1, 2). A `bool` return only guarantees the low byte of EAX, so upper bytes may contain garbage causing wrong branch decisions and infinite loops. Fix: use `int` return type with explicit `? 1 : 0` conversions.
8. **SEH cookie functions** — MSVC functions with SEH have a `MOV [RBP-0x20], -2` state cookie in their prologue, tracked by `.pdata`/`.xdata` unwind tables keyed to original addresses. Overwriting the prologue breaks exception unwind, killing the thread. **FIXED**: `InstallHookVTable()` swaps the function pointer in the vtable instead of overwriting the prologue — zero code modification, SEH fully preserved. `MenuScene::Slot5` re-enabled. Use for any future SEH-affected virtual functions.
9. **Shared vtable functions** — a function shared across multiple `Object::External` subclass vtables must NOT be prologue-hooked: the reimpl's type-specific logic (sets the wrong vtable, zeroes the wrong fields) then corrupts ALL callers, not just the target class. **FIXED**: use `InstallHookVTable()` (same fix as #8, different trigger). Detect by checking Ghidra DATA xrefs before `InstallHook()` — multiple xrefs from different `.rdata` addresses (different vtables) ⇒ shared. Object::External base methods (Cleanup/ScalarDtor/OnDestroy, ~`0x14002xxxx`) are frequently shared; the `0x1401a1xxx`/`0x1401e4xxx` `.rdata` ranges hold the revealing vtables. Symptom when missed: TutorialGoalDialog's vtable got applied to unrelated External objects, breaking UI-element flag bit 22 at +0x34 on clicked buttons.

### Remaining Disabled Hooks (4)
These are NOT trampoline encoding issues — they are functional/logic bugs:
- `Modification::SetupFileSystem` — crashes; 709-instruction function, likely SEH/unwind issue
- `WorkComponent::Shutdown` — causes tutorial load crash due to serialization mismatch
- `FinishTutorialInstance::Slot5` — reason unclear, needs investigation
- `ComponentEntity::Slot3` — trampoline shifts local_res vars into reimpl's frame, corrupts serialization

### Crash Diagnostics
- VEH handler catches ACCESS_VIOLATION/ILLEGAL_INSTRUCTION, logs to `patch_debug.log`
- `g_lastHook` breadcrumb set by every Reimpl_*/Passthrough_* function
- Combined: instant crash → hook name + fault address + registers + stack trace

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
| **Launch Banished (debug with patch)** | launch | Builds, deploys DLL, launches game with GDB attached |
| **Launch Banished (no debug)** | launch | Builds, deploys DLL, launches game without stopping on signals |

### Compiler & Toolchain
- GCC 13.1 (MSYS2 UCRT64) at `C:\msys64\ucrt64\bin\`
- `mingw32-make` for builds (full path: `/c/msys64/ucrt64/bin/mingw32-make.exe`)
- C++17 standard, static-linked runtime

### Debugging via MCP
- Use `vscode-debugger` MCP for breakpoints, stepping, variables, stack traces
- Use `vscode` MCP's `start_debug_session` to launch (config: "Launch Banished (with patch)")
- **Important**: `workspaceFolder` must use lowercase drive letter (`c:\...`)
- See `.vscode/launch.json` for full debug configurations

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
Key directories: `code/Game/` (clean reimpl), `engine/reimpl/` (hook wrappers), `engine/types/` (structs), `engine/funcs/` (function pointers), `patches/src/` (DLL infrastructure). See filesystem for full layout.
