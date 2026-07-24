# VideoDX11-x64.dll (DX11 Renderer Plugin)

**Scope validated 2026-07-24** against the Ghidra database (image base `0x180000000`,
576 functions total). See [../dx11/README.md](../dx11/README.md) for the reimplementation
plan and status board.

## Size of the real work

| Bucket | Address range | Count | Notes |
|--------|---------------|-------|-------|
| Renderer code | `0x180001000` – `0x180005b7f` | **85** | Ghidra-defined: 84 real functions + the `MemFree` import thunk |
| …plus sub-function vtable stubs | (inside the above) | **12** | 3–30-byte tails Ghidra never split out; each is a live vtable slot |
| CRT / MSVC EH / locale | `0x180005c24` – `0x180012fff` | 491 | `entry`, `_initterm`, `__CxxFrameHandler`, locale tables — never port these |

`.text` is `0x180001000`–`0x1800129ff` (~72 KB); the renderer half is only ~19 KB.

**The port's real entry-point count is 96**, not 85: 84 Ghidra functions plus 12
vtable targets that share a function body with their neighbour (e.g. the
`mov al,1; ret` at `0x180001060`, which is slot 1 of two vtables *and* slot 7 of
`DeviceData`). The full attributed roster — every slot of every vtable mapped to
an address, and every address mapped to a class — is the status board in
[../dx11/README.md](../dx11/README.md).

## Exports — the entire plugin ABI

| Export | Ordinal | Address | Signature |
|--------|---------|---------|-----------|
| `CreateDeviceData` | 1 | `0x1800011a0` | `Video::DeviceData* (Video::Device*)` |
| `GetDeviceName` | 2 | `0x180001230` | `const char* ()` → `"DirectX 11"` |

`CreateDeviceData` `MemAlloc`s 0x70 bytes, runs the **Runtime-side**
`Video::DeviceData::DeviceData` base ctor, then overwrites the vptr with the
DX11 vtable at `0x180018bb0` and zeroes the DX11 fields. Everything else in the
DLL is reached through that one vtable.

## How Runtime loads it

`Video::Device::InitializeInterfaces` (Runtime `0x1800f79f0`) calls
`Video::Device::GetDLLForInterface` (Runtime `0x1800f7b20`), which maps
interface index → base name:

| idx | name |
|-----|------|
| 0 | `VideoDX9` |
| 1 | `VideoDX11` |
| 2 | `VideoGL` |

then appends the platform/extension suffixes, `LoadLibrary`s the result, and
`GetProcAddress`es the two exports into a per-interface triplet on the
`Video::Device` singleton (Runtime `0x1801fe7a8`):

| Device offset | Contents |
|---------------|----------|
| `+0x48` | interface count |
| `+0xb0` / `+0xb8` / `+0xc0` | DX9 `HMODULE` / `GetDeviceName` / `CreateDeviceData` |
| `+0xc8` / `+0xd0` / `+0xd8` | **DX11** `HMODULE` / `GetDeviceName` / `CreateDeviceData` |
| `+0xe0` … | GL slot |
| `+0xf8` | active interface index |
| `+0x100` | active `DeviceData*` |

`standalone/main.cpp::DumpVideoState()` already dumps all of this at runtime.

## Class inventory — 12 `*Data` subclasses (verified)

The base `Video::*Data` classes live in **Runtime-x64.dll**; VideoDX11 only
subclasses them. Sizes come from the `MemAlloc` in each factory; the first 0x10
bytes are the Runtime base (vptr + owner back-pointer).

| Class | Size | DX11 vtable | Created by | DX11-private bytes |
|-------|------|-------------|------------|--------------------|
| `DeviceData` | 0x70 | `0x180018bb0` | `CreateDeviceData` (export) | 0x60 |
| `ContextData` | 0x20 | `0x1800186f0` | `0x180005650` | 0x10 |
| `ImageBufferData` | 0x38 | `0x180018790` | `0x1800056c0` | 0x28 |
| `ConstantBufferData` | 0x28 | `0x1800185d0` | `0x180005730` | 0x18 |
| `IndexBufferData` | 0x20 | `0x1800187c8` | `0x1800057a0` | 0x10 |
| `VertexBufferData` | 0x20 | `0x180018830` | `0x180005810` | 0x10 |
| `VertexLayoutData` | 0x18 | `0x180018878` | `0x180005880` | 0x08 |
| `VertexProgramData` | 0x30 | `0x180018890` | `0x1800058f0` | 0x20 |
| `PixelProgramData` | 0x30 | `0x180018800` | `0x180005960` | 0x20 |
| `RenderStateData` | 0x28 | `0x180018818` | `0x1800059d0` | 0x18 |
| `VideoProgramData` | 0x10 | `0x1800185a8` | `0x180005a40` | 0 (vtable only) |
| `RenderBufferData` | 0x10 | `0x180018588` | `0x180005aa0` | 0 (vtable only) |

The old note's "~25 `Video::*Data` classes" (still repeated in `README.md`) is
wrong — there are exactly **12**.

## `DeviceData` vtable — 20 slots at `0x180018bb0`

| # | Target | Role |
|---|--------|------|
| 0 | `0x180004270` | scalar deleting dtor — frees the adapter array at +0x60, chains `Video::DeviceData::~DeviceData` |
| 1 | `0x180004690` | **enumerate adapters / outputs / display modes** (323 decompiled lines — largest function in the DLL) |
| 2 | `0x180004e10` | **create D3D11 device + swap chain + color/depth buffers** |
| 3 | `0x1800051c0` | shutdown — unregister WM callbacks, `DestroyCursor`, release swap chain and buffers |
| 4 | `0x180001190` | small stub (unclassified) |
| 5 | `0x180005430` | small stub (unclassified) |
| 6 | `0x180005440` | small stub (unclassified) |
| 7 | `0x180001060` | small stub (unclassified) |
| 8 | `0x180005450` | `SetImageBuffer` — builds a Win32 `HCURSOR` from a 32×32 `Video::ImageBuffer` |
| 9…19 | `0x180005650` … `0x180005aa0` | the **11 resource factories** in the table above, in `ContextData, ImageBufferData, ConstantBufferData, IndexBufferData, VertexBufferData, VertexLayoutData, VertexProgramData, PixelProgramData, RenderStateData, VideoProgramData, RenderBufferData` order |

## `DeviceData` field map (complete — 0x70 bytes)

Updated 2026-07-24: `+0x20`/`+0x28`/`+0x30`/`+0x38` identified from the buffer
creators, and the `+0x60` entry **corrected** (see below).

| Offset | Contents | Proven by |
|--------|----------|-----------|
| `+0x00` | vptr | ctor |
| `+0x08` | `Video::Device*` owner (Runtime base field) | ctor |
| `+0x10` | COM interface, released on shutdown — most likely `IDXGIFactory1*` (**TBD**) | slot 3 |
| `+0x18` | `IDXGISwapChain*` — released with a `SetFullscreenState(FALSE,0)` first | slot 3 |
| `+0x20` | `ID3D11Device*` | `0x1800052b0`/`0x180005350` call `[+0x20]` vtable index 5 = `CreateTexture2D` |
| `+0x28` | `ID3D11DeviceContext*` | every resource `Bind` reads `g_Device->[0x100]->[0x28]` then calls context methods |
| `+0x30` | depth-stencil `ID3D11Texture2D*` (`D24_UNORM_S8_UINT`, `BIND_DEPTH_STENCIL`) | `0x180005350` |
| `+0x38` | color/render-target `ID3D11Texture2D*` (`B8G8R8A8_UNORM`, `BIND_RENDER_TARGET`) | `0x1800052b0` |
| `+0x40` | `bool` — mirrored into `Application::_singleton+0x40` on shutdown | slot 3 |
| `+0x48` | `HCURSOR` | slot 8 |
| `+0x50` | cursor source `Video::ImageBuffer*` | slot 8 |
| `+0x58` / `+0x5c` | cursor hotspot X / Y | slot 8 |
| `+0x60` / `+0x68` / `+0x6c` | **tracked `Video::ImageBuffer*` list** — `{data, count, capacity}`, stride 8 | `0x180002300` + `~ImageBufferData` |

### Correction: `+0x60` is not the adapter array

The earlier note called `+0x60`/`+0x68` the `Device::GraphicsInterface` array
(stride 0x18). It is not. `0x180002300` appends an `ImageBuffer*` to it
(add-if-absent, ×3 growth, `MemAlloc(count*8, 8)`), and `~ImageBufferData`
(`0x180002430`) swap-removes the dying buffer's `ImageBuffer*` from the same
list. Stride is 8, not 0x18. It is the registry of image buffers that must be
released and recreated across a device/swap-chain reset — `IsGenerated() ||
flags & 0x70 || IsRendered()` decides membership. The adapter array that slot 1
builds is a separate allocation; slot 0 frees whatever `+0x60` holds at teardown.

## Swap-chain description (`0x180004590`)

The whole fullscreen story lives in this one function:

| `DXGI_SWAP_CHAIN_DESC` field | Value |
|------------------------------|-------|
| `BufferDesc.Width` / `.Height` | `Device+0x04` / `Device+0x08` |
| `BufferDesc.RefreshRate` | from the display-mode table, indexed by `Device+0x10` |
| `BufferDesc.Format` | `0x57` = `DXGI_FORMAT_B8G8R8A8_UNORM` |
| `BufferDesc.ScanlineOrdering` / `.Scaling` | 0 / 0 |
| `SampleDesc` | `{1, 0}` — note the *swap chain* is single-sampled; MSAA lives on the `+0x38` color buffer |
| `BufferUsage` | `0x20` = `DXGI_USAGE_RENDER_TARGET_OUTPUT` |
| `BufferCount` | 2 |
| `OutputWindow` | `Device+0x90` (`HWND`) |
| `Windowed` | `Device+0x40 == 0` |
| `SwapEffect` | 0 = `DXGI_SWAP_EFFECT_DISCARD` |
| `Flags` | 2 = `DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH` |

`ALLOW_MODE_SWITCH` plus `Windowed` derived from `Device+0x40` is exactly the
configuration behind the fullscreen-on-focus-loss bug documented in
`../standalone/main.cpp`.

## The one global: `0x1800134b8`

Every resource `Bind` in the DLL starts with the same three loads:

```
mov rax, [rip+…]        ; 0x1800134b8 -> Video::Device**  (Runtime 0x1801fe7a8)
mov rdx, [rax]          ; Video::Device*
mov rax, [rdx+0x100]    ; active Video::DeviceData*
mov rcx, [rax+0x28]     ; ID3D11DeviceContext*
```

`ContextData` is the exception — it caches the context at `ContextData+0x18` and
skips the chain.

## Rendering specs (from strings at `0x1800185c0`–`0x180018b30`)

- Renderer name: `"DirectX 11"`
- Backbuffer: `DXGI_FORMAT_B8G8R8A8_UNORM` (checked for render-target support)
- Depth/stencil: `DXGI_FORMAT_D24_UNORM_S8_UINT` (checked for depth-stencil support)
- Min feature level: `D3D_FEATURE_LEVEL_10_0` (hard requirement — startup fails below it)
- MSAA: `IsMultisampleModeSupported` / `GetMultisampleModeCount` imported from Runtime
- Device-lost handling: distinct messages for `DXGI_ERROR_DEVICE_HUNG`, `DEVICE_REMOVED`,
  `DEVICE_RESET`, `DRIVER_INTERNAL_ERROR`, `INVALID_CALL`, `UNKNOWN`
- Only one semantic string in the whole DLL: `"TEXCOORD"` (input-layout element names)

## Imports

- **`d3d11.dll`**: `D3D11CreateDevice` (single entry point — everything else is COM vtable calls)
- **`dxgi.dll`**: `CreateDXGIFactory1`
- **`user32.dll`**: `CreateCursor`, `DestroyCursor`, `SetCursor`, `ShowCursor`,
  `ShowWindow`, `IsWindowVisible`, `GetMonitorInfoW`
- **`Runtime-x64.dll`** (~85 by-name imports): the 12 `Video::*Data` base ctors/dtors,
  `Video::Device::{Lock,Unlock,ReleaseDynamic,RecreateDynamic,AdjustDisplay,Adapter,
  FindOrAddMode,GetDisplayMode,IsMultisampleModeSupported,GetMultisampleModeCount}`,
  `Video::ImageBuffer::{Width,Height,IsDynamic,IsCompressed,IsRenderBuffer,IsDepthBuffer,
  GetLevelCount,IsRendered,IsGenerated,CopyFromTexture}`, `Video::Context::{SetViewport,
  SetImageBuffer,SetVertexBuffer,SetRenderBuffer,BindResources,BindTextures}`,
  `System::{String,WideString,MemoryAllocator,Serializer}`,
  `Application::Application::{GetInstance,RegisterMessageCallback,UnregisterMessageCallback}`

The `Video::Context::Bind*` / `Video::Device::*` imports are the important ones:
they show the DLL is a **leaf** — Runtime drives it, it does not drive Runtime.

## What is NOT in this DLL

The actual renderer — `Graphics::{Scene,Renderer,Camera,MeshInstance,Material,SunLight,
ParticleSystem,Sprite,…}` (641 symbols) and the `Video::` GPU abstraction itself
(`Device`, `Context`, `ImageBuffer`, `VertexBuffer`, `RenderState`, `ShaderProgram`, …
669 symbols) — all live in **Runtime-x64.dll**. See
[runtime.md](runtime.md) §Graphics / Rendering and §Video.

VideoDX11 is roughly 1% of the render-side code by symbol count. It is, however,
the *whole* platform boundary: port it and every D3D11 call in the process is ours.
