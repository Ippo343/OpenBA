# dx11/ — VideoDX11-x64.dll reimplementation

Mirrors the `engine/` and `runtime/` layout for the third binary: the DX11
renderer plugin. Goal is the same as everywhere else in this project — replace
the shipped DLL's code with clean C++ that links into `reimpl-x64.exe`, one
function at a time, with the original still available as a fallback until each
piece is proven.

Reference decompilation status: [../decomp/videodx11.md](../decomp/videodx11.md)
(scope validated 2026-07-24).

```
dx11/
  README.md      — this file: scope, seam, workflow, status board
  types/         — binary-compatible structs for the 12 *Data classes + D3D11 COM shapes
  funcs/         — resolved VideoDX11 function pointers (dx11_funcs.h/.cpp)
  src/           — clean C++ reimplementations
```

---

## Why this is a tractable target

VideoDX11-x64.dll has 576 functions but only **85** are renderer code
(`0x180001000`–`0x180005b7f`, ~19 KB); the other 491 are the MSVC CRT, the C++
exception machinery, and locale tables. We never port those — the exe already
has its own CRT.

85 functions, 12 classes, 2 exports, one vtable that reaches everything. This is
by far the smallest of the three binaries and it is a clean *leaf*: Runtime
drives it, it never drives Runtime.

---

## The replacement seam

Unlike exe functions (which `ResolveAddr(Module::Exe, …)` redirects at the call
site), this DLL is loaded by Runtime at startup and reached through exactly two
function pointers. `Video::Device::InitializeInterfaces` (Runtime `0x1800f79f0`)
`LoadLibrary`s `VideoDX11-x64.dll` and stores the two exports on the
`Video::Device` singleton (Runtime `0x1801fe7a8`):

```
Device+0xc8  HMODULE          VideoDX11-x64.dll
Device+0xd0  GetDeviceName    -> "DirectX 11"
Device+0xd8  CreateDeviceData -> Video::DeviceData* (Video::Device*)
```

**So the seam is those two slots.** After `InitializeInterfaces` runs and before
`CreateVideoInterface` picks one, overwrite `+0xd0` / `+0xd8` with our own
functions. From that point every D3D11 call in the process goes through code we
compiled — no shim DLL, no renaming the shipped file, no changes to how the game
is deployed. Same spirit as `SkipPlatformConfigure()` in `standalone/main.cpp`,
which already patches the loaded Runtime image in place.

### Incremental porting: the delegating DeviceData

Replacing all 85 functions before anything runs would be a big-bang rewrite. The
plan instead is a **hybrid `DeviceData`**:

1. Our `CreateDeviceData` calls the *original* export first, keeping the real
   `DeviceData` as a delegate.
2. We allocate our own object with our own 20-slot vtable.
3. Each slot either runs our reimplementation, or tail-calls the original
   object's corresponding vtable slot.

That gives per-function granularity — the same thing `standalone/address_registry.cpp`
gives us for the exe — and every slot can be A/B tested against the original by
flipping one entry back.

The same trick nests: our `DeviceData::CreateContextData` can hand back either
our `ContextData` or the original's, so the 11 resource classes can be ported
independently of each other.

**Kill-switch convention** (per project policy — see `kDefaultOnFlags` in
`code/Game/Component/component_factory_base.cpp`): new, unproven DX11 slots start
opt-in behind `BANISHED_DX11_*`; once a slot is verified in-game it graduates to
default-on and its env var becomes a `=0` kill switch only.

---

## Verification is unusually good here

This is the one subsystem where we can diff our output against the original
frame-by-frame instead of reasoning about it:

- **RenderDoc MCP** is already wired into this repo (`standalone/renderdoc_integration.cpp`,
  the `mcp__renderdoc__*` tools, captures in `captures/`). Capture a frame with
  the original DLL, capture the same frame with a slot ported, and diff draw
  calls / pipeline state / render targets.
- `standalone/main.cpp` already has `DumpVideoState()` and `DumpSwapChainState()`
  probes that read the Device singleton and the swap chain.
- D3D11 debug layer: `D3D11CreateDevice` takes a flags word we control once
  slot 2 is ours, so `D3D11_CREATE_DEVICE_DEBUG` becomes available.

---

## Suggested porting order

Ordered by "unblocks the most, risks the least". Nothing here is committed to yet.

| Phase | What | Why first / why later |
|-------|------|-----------------------|
| 0 | Seam installer + delegating `DeviceData` with **all 20 slots forwarding** | Pure infrastructure. Verifiable by "game still looks identical". Must land before any real port. |
| 1 | `GetDeviceName`, dtor (slot 0), the 4 small stubs (slots 4–7) | Trivial, low blast radius, exercises the seam. |
| 2 | The 11 resource factories (slots 9–19) | Each is a ~15-line `MemAlloc` + base ctor + vptr store. Porting these transfers *ownership* of the 11 resource classes to us without yet porting their vtables. |
| 3 | Per-resource vtables, cheapest first: `RenderBufferData` and `VideoProgramData` (0 private bytes), then `VertexLayoutData` (0x08) | Small structs, few D3D11 objects each. |
| 4 | `VertexBufferData` / `IndexBufferData` / `ConstantBufferData` (Lock/Unlock/Bind) | The buffer-upload path — high call volume, so RenderDoc diffs are informative. |
| 5 | `ImageBufferData` (0x38) | Texture creation, formats, compressed/mip/render-target/depth variants. Biggest of the resource classes. |
| 6 | `VertexProgramData` / `PixelProgramData` / `RenderStateData` | Shader objects, input-layout matching (`"TEXCOORD"`), rasterizer/blend/depth state objects. |
| 7 | `ContextData` (slot 9 users: `SetViewport`, `SetRenderBuffer`, `BindResources`, `BindTextures`) | Depends on nearly every resource class above. |
| 8 | Slot 8 `SetImageBuffer` (`HCURSOR` from a 32×32 ImageBuffer) | Self-contained, no D3D11 — could be done any time as a warm-up. |
| 9 | Slot 1 adapter/mode enumeration (`0x180004690`, 323 lines) | Largest single function; touches `Device::GraphicsInterface` array marshalling. |
| 10 | Slot 2 device + swap-chain creation (`0x180004e10`), slot 3 shutdown | Last, because getting it wrong means no window. Also where the known fullscreen-on-focus-loss bug lives — see the `DumpSwapChainState` comment in `standalone/main.cpp`. |

### Known issue to keep in view

`standalone/main.cpp` documents a fullscreen bug: standalone enters DXGI
exclusive fullscreen, focus loss makes DXGI silently drop FSE, the window snaps
to 1280×720, and `Present` succeeds against a stale surface. That lives in slots
2/3 and the `WM_ACTIVATE` handler pair (`0x1800042d0` → `0x180004340`). Owning
those slots is the fix path, which argues for pulling phase 10 forward if the
bug becomes a priority.

---

## Status board

Status key matches `decomp/README.md`: `-` not started, `~` partial,
`x` decompiled & documented, `R` reimplemented & live.

### Exports

| Status | Address | Name |
|--------|---------|------|
| x | `0x1800011a0` | `CreateDeviceData` |
| x | `0x180001230` | `GetDeviceName` |

### `DeviceData` vtable (20 slots @ `0x180018bb0`)

| Status | Slot | Address | Role |
|--------|------|---------|------|
| x | 0 | `0x180004270` | scalar deleting dtor |
| ~ | 1 | `0x180004690` | enumerate adapters / outputs / display modes |
| - | 2 | `0x180004e10` | create D3D11 device + swap chain + buffers |
| x | 3 | `0x1800051c0` | shutdown / release |
| - | 4 | `0x180001190` | stub (unclassified) |
| - | 5 | `0x180005430` | stub (unclassified) |
| - | 6 | `0x180005440` | stub (unclassified) |
| - | 7 | `0x180001060` | stub (unclassified) |
| x | 8 | `0x180005450` | `SetImageBuffer` → `HCURSOR` |
| x | 9 | `0x180005650` | create `ContextData` |
| x | 10 | `0x1800056c0` | create `ImageBufferData` |
| x | 11 | `0x180005730` | create `ConstantBufferData` |
| x | 12 | `0x1800057a0` | create `IndexBufferData` |
| x | 13 | `0x180005810` | create `VertexBufferData` |
| x | 14 | `0x180005880` | create `VertexLayoutData` |
| x | 15 | `0x1800058f0` | create `VertexProgramData` |
| x | 16 | `0x180005960` | create `PixelProgramData` |
| x | 17 | `0x1800059d0` | create `RenderStateData` |
| x | 18 | `0x180005a40` | create `VideoProgramData` |
| x | 19 | `0x180005aa0` | create `RenderBufferData` |

### Resource class vtables

| Status | Class | Vtable | Slots | Notes |
|--------|-------|--------|-------|-------|
| - | `RenderBufferData` | `0x180018588` | ? | 0 private bytes — cheapest |
| - | `VideoProgramData` | `0x1800185a8` | ? | 0 private bytes |
| - | `ConstantBufferData` | `0x1800185d0` | ? | |
| - | `ContextData` | `0x1800186f0` | ? | |
| - | `ImageBufferData` | `0x180018790` | ? | largest resource class |
| - | `PixelProgramData` | `0x180018800` | ? | |
| - | `RenderStateData` | `0x180018818` | ? | |
| - | `VertexBufferData` | `0x180018830` | ? | |
| - | `VertexLayoutData` | `0x180018878` | ? | |
| - | `VertexProgramData` | `0x180018890` | ? | |
| - | `IndexBufferData` | `0x1800187c8` | ? | |

Slot counts are unfilled — the vtable extents have not been walked yet. That is
task #1 for the next session on this branch.

### Remaining unclassified renderer functions

Of the 85 renderer functions, 32 are named above. The other ~53 (mostly in
`0x180001240`–`0x180004270`, the resource-vtable implementations) have not been
attributed to a class yet.

---

## Workflow (per function)

Same as the project-wide loop in `../CLAUDE.md`, with the DX11 specifics:

1. **Decompile** in Ghidra (`switch_program("VideoDX11-x64.dll")` first — the
   MCP server defaults to Runtime), rename to `VideoDX11_<Class>_<Method>`.
2. **Type header** in `dx11/types/` with `static_assert` on every offset.
   Sizes are already pinned by the `MemAlloc` in each factory.
3. **Resolve deps** — Runtime-side calls go in `engine/funcs/engine_funcs.h`
   (existing convention, already has `Video::Device::Lock/Unlock` etc.);
   VideoDX11-internal calls go in `dx11/funcs/dx11_funcs.h`.
4. **Write clean C++** in `dx11/src/`.
5. **Wire the slot** in the seam installer's vtable, behind a `BANISHED_DX11_*`
   opt-in until proven.
6. **Test** — `mingw32-make sa-run`, then read
   `C:/GOG Games/Banished/standalone_debug.log`, and RenderDoc-diff the frame
   against a capture taken with the slot forwarding to the original.

### Ghidra note

`ResolveAddr(Module::VideoDX11, addr)` already exists
(`standalone/resolve.h`, `standalone/resolve_standalone.cpp` — the DLL is
registered with preferred base `0x180000000` and resolved optionally). Both
Runtime-x64.dll and VideoDX11-x64.dll share preferred base `0x180000000`, so
**always pass the right `Module::` enum** — the addresses are otherwise
indistinguishable.
