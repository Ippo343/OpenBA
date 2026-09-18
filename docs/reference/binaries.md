# Retail binaries (reference)

Facts about the original game binaries in `C:\GOG Games\Banished\`. Our exe
(`reimpl-x64.exe`) loads none of the DLLs below; they matter as the thing being
reimplemented and as Ghidra programs. Per-binary decompilation inventories are in
[decomp/](../../decomp/README.md).

## Application-x64.exe (game logic)
- x86-64, little endian, MSVC. Base `0x140000000`, entry `0x14003b1c0`
- `.text` `0x140001000`-`0x1401501ff` (~1.4 MB); file 1.9 MB
- 11,473 functions in Ghidra (10,476 at initial import), 50,420 symbols
- Exports: `NvOptimusEnablement`, `AmdPowerXpressRequestHighPerformance`
- PE timestamp `2017-09-10 16:34:27`
- Inventory: [decomp/application.md](../../decomp/application.md)

### Its import table = our "dispatch slots"
The exe imports Runtime-x64.dll through a normal PE import table: 833 imports in slots
`0x140151218`-`0x140152c18`, plus 66 from KERNEL32.dll in `0x140151000`-`0x140151208`.
The `.rdata` dispatch slots we register with `RegisterDispatchSlot` ARE that table, so
every slot has a name readable from the binary (`tools/dispatch_slot_map.py`). Rules
for registering them: `.claude/rules/address-registry.md`.

## Runtime-x64.dll (engine core) — fully ported, no longer loaded
- 2.7 MB, 15,290 functions, image base `0x180000000`, PE timestamp `2017-09-10 16:34:14`
- ~4,800 exported ordinals: reflection-based API with a TypeName/Factory/Lifecycle
  pattern per class. 18 namespaces, 100+ classes: object model, math, rendering, UI
  (20+ widgets), file I/O with packages/mods, 2D collision, 3D picking, A* pathfinding,
  achievements, save/load, input (DirectInput), threading
- Imports: KERNEL32, USER32, GDI32, ADVAPI32, SHELL32, OLE32, DINPUT8, DBGHELP
- Inventory: [decomp/runtime.md](../../decomp/runtime.md). Our port lives in `runtime/`.
- `Runtime-x64-profile.dll` (ToolKit build: debug strings + 5,182 exports) is the primary
  source for naming unexported internals.

## VideoDX11-x64.dll (DX11 renderer plugin) — fully replaced by `dx11/`
- 145 KB, 576 functions (~86 rendering, rest CRT/exception), image base `0x180000000`
- Exports `CreateDeviceData` (ordinal 1), `GetDeviceName` (ordinal 2): plugin factory
- Strategy/Bridge backend providing `*Data` companions for engine abstractions
  (DeviceData, ContextData, VertexBufferData, ...)
- Backbuffer `DXGI_FORMAT_B8G8R8A8_UNORM`, depth `DXGI_FORMAT_D24_UNORM_S8_UINT`,
  min `D3D_FEATURE_LEVEL_10_0`
- Inventory: [decomp/videodx11.md](../../decomp/videodx11.md)

## VideoDX9-x64.dll
DirectX 9 fallback renderer, same plugin interface. Not loaded in Ghidra.

## Modding toolkit
- https://banishedinfo.com/mods/resources/gettingstarted
- `Application-x64-profile.exe`: developer build with profiling hooks; its debug strings
  gave us the original source tree (codename "Buildy", `C:\Projects\Buildy\code\Game\`)
- `Tools-x64.exe` compiles `.rsc` -> binary in `/bin/WinData`
- Mods are `.pkm` packages, delta-packed against base game data
- Resource load order: loose files -> `.pkg` (game) -> `.pkm` (mods)
