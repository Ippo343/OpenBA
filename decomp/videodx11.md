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

Two encoding details matter to a reimplementation and are invisible in a
mnemonic listing:

- `PUSH RDI` at `0x1800011a0` is encoded `40 57` — a REX prefix with no bits
  set, which MSVC emits anyway. Two bytes, not one.
- `+0x58` is written as a **qword** `-1`, so it initialises `cursorHotspotX`
  *and* `cursorHotspotY`; `+0x40` is written as a single **byte**, leaving
  `+0x41..+0x47` as the base ctor left them.

Both exports are reimplemented in `dx11/src/exports.cpp` (2026-07-24) and
installed as byte-verified inline detours, because `Video::Device+0xd0`/`+0xd8`
cannot be patched — see `dx11/README.md`.

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
| `+0x10` | `IDXGIFactory1*` — **confirmed** (L6). Created by slot 2's `CreateDXGIFactory1`; **released by slot 2 itself** at `0x1800050c2` and never nulled, so it dangles all session. Slot 3 does *not* release it. | slot 2 |
| `+0x18` | `IDXGISwapChain*` — released with a `SetFullscreenState(FALSE,0)` first | slot 3 |
| `+0x20` | `ID3D11Device*` | `0x1800052b0`/`0x180005350` call `[+0x20]` vtable index 5 = `CreateTexture2D` |
| `+0x28` | `ID3D11DeviceContext*` | every resource `Bind` reads `g_Device->[0x100]->[0x28]` then calls context methods |
| `+0x30` | depth-stencil `ID3D11Texture2D*` (`D24_UNORM_S8_UINT`, `BIND_DEPTH_STENCIL`) | `0x180005350` |
| `+0x38` | color/render-target `ID3D11Texture2D*` (`B8G8R8A8_UNORM`, `BIND_RENDER_TARGET`) | `0x1800052b0` |
| `+0x40` | `bool wasFullscreen` — "restore fullscreen on refocus" latch, **not** "is fullscreen". Only writer is the WM_ACTIVATE handler `0x1800042d0`. Mirrored into **`Video::Configuration::_singleton+0x40`** on shutdown (`0x18000520f` loads IAT `0x180013480`) — the earlier "`Application::_singleton+0x40`" was one of three `_singleton_exref` mix-ups corrected by L6 | slots 3, `42d0` |
| `+0x48` | `HCURSOR` | slot 8 |
| `+0x50` | cursor source `Video::ImageBuffer*` | slot 8 |
| `+0x58` / `+0x5c` | cursor hotspot X / Y | slot 8 |
| `+0x60` / `+0x68` / `+0x6c` | **tracked `Video::ImageBuffer*` list** — `{data, count, capacity}`, stride 8 | `0x180002300` + `~ImageBufferData` |

### Correction: `+0x60` is not the adapter array

The earlier note called `+0x60`/`+0x68` the `Device::GraphicsInterface` array
(stride 0x18). It is not. `0x180002300` appends an `ImageBuffer*` to it
(add-if-absent, ×3 growth, `MemAlloc(count*8, 8)`), and `~ImageBufferData`
(`0x180002430`) removes the dying buffer's `ImageBuffer*` from the same list.

**The removal is a stable shift-erase, not a swap-remove** (corrected
2026-07-24 — an earlier revision of this file said swap-remove). `0x1800024c8`–
`0x1800024ee` walks forward doing `*p = p[1]`, shifting every later element down
one, then decrements the count. Order is preserved, so a port that swap-removes
would be observably different.

Stride is 8, not 0x18. It is the registry of image buffers that must be
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
| `Windowed` | **`Video::Configuration+0x40 == 0`** (corrected — see below) |
| `SwapEffect` | 0 = `DXGI_SWAP_EFFECT_DISCARD` |
| `Flags` | 2 = `DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH` |

**Correction (L6): `Windowed` comes from `Video::Configuration+0x40`, not
`Device+0x40`.** `0x1800045af` loads IAT `0x180013480`
(`?_singleton@Configuration@Video@@`), then `0x18000465b` does
`CMP byte [RDI+0x40],AL; SETZ AL`. `Video::Device+0x40` is not a flag at all —
it is the base pointer of the `Device::GraphicsInterface` array that slot 1
fills (`0x180004dca` passes `Device+0x40` and `[Device+0x48]+1` to the array
grower at `0x180005b00`). Reading a bool out of that pointer would be garbage.

`ALLOW_MODE_SWITCH` plus `Windowed` derived from the *configuration's*
fullscreen flag is the configuration behind the fullscreen-on-focus-loss bug
documented in `../standalone/main.cpp`.

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

### Beware: two different globals both render as `_singleton_exref`

Ghidra shows `0x180013480` and `0x1800134b8` with the same display name. They are
**different imports** and conflating them will send you to the wrong object:

| Address | Actual global | Fields used by this DLL |
|---------|---------------|-------------------------|
| `0x180013310` | `Application::Application::_singleton` | Register/UnregisterMessageCallback, GetInstance |
| `0x180013480` | `Video::Configuration::_singleton` | `+0x1c/+0x20` window size, `+0x24` multisample, `+0x28` anisotropy, `+0x40` fullscreen, `+0x41` vsync, `+0x42` skip-ShowWindow |
| `0x1800134b8` | `Video::Device::_singleton` | `+0x98` caps, `+0x100` active `DeviceData*` |

The Bind chain above is the `0x1800134b8` one. **There are three, not two** —
Ghidra renders the `Application` one as `_singleton_exref` too, and an earlier
revision of this file attributed `DeviceData+0x40`'s shutdown mirror to it. It
is the `Configuration` one.

## `Video::Configuration` field map (recovered by L6)

Every offset proven twice: by the registry key `Video::Configuration::Startup`
(Runtime `0x1800f6680`) loads it from, and by the default
`Video::Configuration::Configuration` (Runtime `0x1800f6f10`) writes. Keys are
built as `String::Format("%s%s", "Video", <suffix>)` from the suffix table at
Runtime `0x1801587c0`, and live under
`HKCU\Software\Shining Rock Software LLC\Banished` as `REG_SZ`.

| Offset | Registry key | Ctor default |
|--------|--------------|--------------|
| `+0x00` | `VideoSettings` (bool) | `true` |
| `+0x04` | `VideoInterface` | 1 |
| `+0x08` | `VideoAdapter` | 0 |
| `+0x0c` | `VideoWidth` | −1 |
| `+0x10` | `VideoHeight` | −1 |
| `+0x14` | `VideoWindowX` | −1 |
| `+0x18` | `VideoWindowY` | −1 |
| `+0x1c` | `VideoWindowWidth` | **−1** ("skip override" sentinel) |
| `+0x20` | `VideoWindowHeight` | **−1** ("skip override" sentinel) |
| `+0x24` | `VideoMultisample` | 1 |
| `+0x28` | `VideoTextureFilter` | 0 |
| `+0x2c` | `VideoShadowScalar` | 1 |
| `+0x30` | `VideoShaderDetail` | 2 |
| `+0x34` | `VideoReflectionDetail` | 1 |
| `+0x38` | `VideoRefreshN` | 60 |
| `+0x3c` | `VideoRefreshD` | 1 |
| `+0x40` | `VideoFullscreen` (bool) | `true` |
| `+0x41` | `VideoVSync` (bool) | `true` |
| `+0x42` | *not persisted* | `false` — suppresses slot 2's `ShowWindow` |

The −1 sentinel is consumed in **VideoDX11 slot 2**, not in
`Video::Device::ValidateConfiguration`: `0x180004f12`/`0x180004f21` are sign
tests (`TEST ECX,ECX; JS skip`) guarding the windowed size override.

### Where the swap-chain dimensions actually come from

1. Slot 2 seeds `Device+0x04/+0x08` from `modeTable[Device+0x0c]` — the
   *fullscreen* resolution (`VideoWidth`/`VideoHeight` after validation).
2. If `Configuration+0x40` (fullscreen) is false, each of
   `Configuration+0x1c/+0x20` overrides it **when non-negative**.
3. `ShowWindow(Device+0x90, SW_SHOWDEFAULT)` unless `Configuration+0x42`.
4. `Video::Device::AdjustDisplay` (Runtime `0x1800f7e20`) physically
   `SetWindowPos`es the HWND and then, in the windowed branch, **re-reads
   `Device+0x04/+0x08` from `GetClientRect` and writes the result back into
   `Configuration+0x1c/+0x20`**.
5. Only then does `FillSwapChainDesc` run.

So the swap chain always matches the settled client rect, and
`VideoWindowWidth`/`Height` are rewritten from the real window on every
`AdjustDisplay` — which is why the −1 sentinel only ever survives the first boot.

`Video::Device::ValidateConfiguration` (Runtime `0x1800f7370`) does **not**
touch `VideoWindow*` or `VideoSettings` at all. Its eight `int&` parameters are
interface index, adapter index, width, height, refresh numerator, refresh
denominator, multisample index and anisotropy — it clamps each against the
enumerated `Adapter` record slot 1 built.

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

---

# RenderState → D3D11 state objects

Full recovery of `RenderStateData` (vtable `0x180018818`, 3 slots), the class
that translates the engine's render-state *description* into the three
immutable D3D11 state objects. Reimplementation:
[`dx11/src/renderstatedata.cpp`](../dx11/src/renderstatedata.cpp).

Recovered 2026-07-24 from `VideoDX11-x64.dll` disassembly plus five
corroborating `Runtime-x64.dll` functions. Every value below is transcribed
from an instruction, not inferred from behaviour.

## The three slots

| Slot | Address | Role | Driven from (Runtime) |
|------|---------|------|-----------------------|
| 0 | `0x1800036e0` | scalar deleting dtor: `Release` ×3, base dtor, `MemFree` | `Video::RenderState::PlatformDestruct` `0x1800f4e80` → `vtable[0](this, 1)` |
| 1 | `0x180003760` | **Create** — the three `CreateXxxState` calls | `Video::RenderState::PlatformCreate` `0x1800f4ea0` → `vtable[1]()` |
| 2 | `0x180003a90` | **Bind** — `OMSetBlendState` / `OMSetDepthStencilState` / `RSSetState` | `Video::RenderState::Bind(Context&)` `0x1800f4ec0` → `vtable[2]()` |

`Bind` is declared as taking a `Video::Context&`, and Runtime does pass it in
RDX, but slot 2 never reads it — it walks to the global immediate context like
every other resource `Bind` in the DLL.

## `RenderStateData` layout (0x28 bytes)

| Offset | Type | Written by | Read by |
|--------|------|-----------|---------|
| `+0x00` | vptr | slot 0 restores `0x180018818` | — |
| `+0x08` | `Video::RenderState*` | base ctor | slots 1, 2 |
| `+0x10` | `ID3D11BlendState*` | `CreateBlendState` (`0x180003a3f`) | `OMSetBlendState` (`0x180003ad9`) |
| `+0x18` | `ID3D11DepthStencilState*` | `CreateDepthStencilState` (`0x180003a68`) | `OMSetDepthStencilState` (`0x180003af2`) |
| `+0x20` | `ID3D11RasterizerState*` | `CreateRasterizerState` (`0x180003a53`) | `RSSetState` (`0x180003b03`) |

Slot 0 releases in *field* order (blend, depth-stencil, rasterizer), which is
**not** the creation order (blend, rasterizer, depth-stencil). Neither slot 0
nor slot 1 nulls a field, and slot 1 does not release a previous object.

## COM vtable offsets used

Index = position in `ID3D11DeviceVtbl` / `ID3D11DeviceContextVtbl` counted from
`QueryInterface` = 0. All confirmed against mingw-w64's `d3d11.h`.

| Interface | Idx | Byte off | Method | Call site |
|-----------|-----|----------|--------|-----------|
| `ID3D11Device` | 20 | `+0xa0` | `CreateBlendState` | `0x180003a3f` |
| `ID3D11Device` | 21 | `+0xa8` | `CreateDepthStencilState` | `0x180003a68` |
| `ID3D11Device` | 22 | `+0xb0` | `CreateRasterizerState` | `0x180003a53` |
| `ID3D11DeviceContext` | 35 | `+0x118` | `OMSetBlendState` | `0x180003ad9` |
| `ID3D11DeviceContext` | 36 | `+0x120` | `OMSetDepthStencilState` | `0x180003af2` |
| `ID3D11DeviceContext` | 43 | `+0x158` | `RSSetState` | `0x180003b03` |
| `IUnknown` | 2 | `+0x10` | `Release` | `0x180003713`/`722`/`731` |

## `Video::RenderState` field map

The description object at `RenderStateData+0x08`. Layout proven independently
of the DX11 reads by:

- `Video::RenderState::RenderState()` `0x1800f4b60` — default values
- `Video::RenderState::Serialize(IO::Stream&)` `0x1800f4c90` — field order + widths
- `…::AlphaBlend::Serialize` `0x1800f4aa0`, `…::Depth::Serialize` `0x1800f4a00`,
  `…::Stencil::Serialize` `0x1800f48e0`
- the public signature `RenderState::Initialize(const AlphaBlend&, const Depth&,
  const Stencil&, CullMode, FillMode, bool)` (mangled string at `0x1801dfe97`)

Serializer virtual → width: `+0x78` = bool, `+0x80` = float, `+0x98` = int/enum,
`+0xb8` = byte.

| Offset | Width | Field | Default | Consumed as |
|--------|-------|-------|---------|-------------|
| `+0x20` | bool | `alphaBlend.enabled` | `false` | `RenderTarget[0].BlendEnable` |
| `+0x24` | enum | `alphaBlend.srcBlend` | `1` (One) | `SrcBlend` **and** `SrcBlendAlpha` |
| `+0x28` | enum | `alphaBlend.destBlend` | `0` (Zero) | `DestBlend` **and** `DestBlendAlpha` |
| `+0x2c` | enum | `alphaBlend.blendOp` | `0` (Add) | `BlendOp` **and** `BlendOpAlpha` |
| `+0x30` | int | `alphaBlend.colorWriteMask` | `0xF` | `RenderTargetWriteMask` (low nibble only) |
| `+0x34` | bool | `depth.enabled` | `true` | `DepthEnable` |
| `+0x35` | bool | `depth.writeEnabled` | `true` | `DepthWriteMask` |
| `+0x38` | enum | `depth.compareFunc` | `4` (LessEqual) | `DepthFunc` |
| `+0x3c` | float | `depth.bias` | `0.0f` | `DepthBias` (×16777215) |
| `+0x40` | float | `depth.slopeScaledBias` | `0.0f` | `SlopeScaledDepthBias` (×4096) |
| `+0x44` | bool | `stencil.enabled` | `false` | `StencilEnable` |
| `+0x45` | bool | `stencil.twoSided` | `false` | selects the back-face branch |
| `+0x48` | enum | `stencil.compareFunc` | `0` | `FrontFace.StencilFunc` **and** `BackFace.StencilFunc` |
| `+0x4c` | byte | `stencil.reference` | `0` | `OMSetDepthStencilState` StencilRef |
| `+0x50` | enum | `stencil.front.failOp` | `0` (Keep) | `FrontFace.StencilFailOp` |
| `+0x54` | enum | `stencil.front.depthFailOp` | `0` | `FrontFace.StencilDepthFailOp` |
| `+0x58` | enum | `stencil.front.passOp` | `0` | `FrontFace.StencilPassOp` |
| `+0x5c` | enum | `stencil.back.failOp` | `0` | `BackFace.StencilFailOp` (two-sided only) |
| `+0x60` | enum | `stencil.back.depthFailOp` | `0` | `BackFace.StencilDepthFailOp` (two-sided only) |
| `+0x64` | enum | `stencil.back.passOp` | `0` | `BackFace.StencilPassOp` (two-sided only) |
| `+0x68` | enum | `cullMode` | `1` (Back) | `CullMode` |
| `+0x6c` | enum | `fillMode` | `0` (Solid) | `FillMode` |
| `+0x70` | bool | `scissorEnabled` | `false` | `ScissorEnable` |
| `+0x78` | ptr | `platformData` | — | the `RenderStateData*` itself |

`+0x00`–`+0x1f` is the `Object::External` base (vptr, `System::Filename` at
`+0x08`, an `{ptr,count}` array at `+0x10`/`+0x18`).

**There is exactly one stencil compare function.** The back face re-reads
`+0x48` (`0x180003a10`), the same field the front face uses. That is not a bug
in the DX11 layer — `Stencil::Serialize` shows a single int for the whole
sub-struct.

**Alpha blending is not separately configurable.** The alpha triplet re-loads
`+0x24`/`+0x28`/`+0x2c` (`0x180003935`, `0x180003940`, `0x18000394b`).

## Enum translation tables

Six lookup tables built on the stack at the top of `0x180003760`. Values are
exact; the engine-side *names* are inferred from the D3D11 constant each slot
maps to, corroborated by the ctor defaults and by
`Video::RenderState::IsWireframe` (`0x18004b730`, literally `return fillMode == 1`).

None of the six lookups is bounds-checked.

### Blend factor — `[RSP+0x20]`, 10 entries, written `0x180003783`–`0x1800037cb`

| Engine | D3D11 value | `D3D11_BLEND` |
|--------|-------------|---------------|
| 0 Zero | 1 | `ZERO` |
| 1 One | 2 | `ONE` |
| 2 SrcColor | 3 | `SRC_COLOR` |
| 3 InvSrcColor | 4 | `INV_SRC_COLOR` |
| 4 SrcAlpha | 5 | `SRC_ALPHA` |
| 5 InvSrcAlpha | 6 | `INV_SRC_ALPHA` |
| 6 DestColor | 9 | `DEST_COLOR` |
| 7 InvDestColor | 10 | `INV_DEST_COLOR` |
| 8 DestAlpha | 7 | `DEST_ALPHA` |
| 9 InvDestAlpha | 8 | `INV_DEST_ALPHA` |

`SRC_ALPHA_SAT`, `BLEND_FACTOR` and the `SRC1_*` dual-source factors are
unreachable from the engine enum.

### Blend op — `[RSP+0x78]`, 3 entries, written `0x18000382c`/`834`/`83c`

| Engine | D3D11 value | `D3D11_BLEND_OP` |
|--------|-------------|------------------|
| 0 Add | 1 | `ADD` |
| 1 Subtract | 2 | `SUBTRACT` |
| 2 ReverseSubtract | 3 | `REV_SUBTRACT` |

`MIN`/`MAX` unreachable.

### Compare func — `[RBP-0x78]`, 8 entries, written `0x180003857`–`0x1800038ae`

Used for both `DepthFunc` and `StencilFunc`.

| Engine | D3D11 value | `D3D11_COMPARISON_FUNC` |
|--------|-------------|-------------------------|
| 0 Never | 1 | `NEVER` |
| 1 Always | 8 | `ALWAYS` |
| 2 Equal | 3 | `EQUAL` |
| 3 Less | 2 | `LESS` |
| 4 LessEqual | 4 | `LESS_EQUAL` |
| 5 Greater | 5 | `GREATER` |
| 6 GreaterEqual | 7 | `GREATER_EQUAL` |
| 7 NotEqual | 6 | `NOT_EQUAL` |

### Stencil op — `[RSP+0x58]`, 8 entries, written `0x1800038b8`–`0x1800038f0`

| Engine | D3D11 value | `D3D11_STENCIL_OP` |
|--------|-------------|--------------------|
| 0 Keep | 1 | `KEEP` |
| 1 Zero | 2 | `ZERO` |
| 2 Increment | 7 | `INCR` (wrapping) |
| 3 Decrement | 8 | `DECR` (wrapping) |
| 4 IncrementClamp | 4 | `INCR_SAT` |
| 5 DecrementClamp | 5 | `DECR_SAT` |
| 6 Replace | 3 | `REPLACE` |
| 7 Invert | 6 | `INVERT` |

Note 2/3 are the *wrapping* variants and 4/5 the saturating ones — easy to swap.

### Cull mode — `[RBP-0x58]`, 3 entries, written `0x1800037f7`/`808`/`80f`

| Engine | D3D11 value | `D3D11_CULL_MODE` |
|--------|-------------|-------------------|
| 0 Front | 2 | `FRONT` |
| 1 Back | 3 | `BACK` (ctor default) |
| 2 None | 1 | `NONE` |

The engine's ordering is Front/Back/None, not the more usual None-first.

### Fill mode — `[RBP+0x100]` (the *caller's shadow space*), 2 entries, written `0x1800037dc`/`7e6`

| Engine | D3D11 value | `D3D11_FILL_MODE` |
|--------|-------------|-------------------|
| 0 Solid | 3 | `SOLID` (ctor default) |
| 1 Wireframe | 2 | `WIREFRAME` (== `IsWireframe()`) |

## Slot 1 — the three descs, field by field

`D3D11_BLEND_DESC` — `memset` 0x108 bytes at `0x180003821`
(`mov r8d,0x108` → `sizeof(D3D11_BLEND_DESC)`), then nine fields.
`RenderTarget[1..7]` stay zero and are unused (`IndependentBlendEnable` FALSE).

| Field | Source | Site |
|-------|--------|------|
| `AlphaToCoverageEnable` | hard `FALSE` | `0x180003905` |
| `IndependentBlendEnable` | hard `FALSE` | `0x180003905` |
| `RenderTarget[0].BlendEnable` | `RS+0x20 != 0` | `0x18000390b` |
| `RenderTarget[0].SrcBlend` | `blendTbl[RS+0x24]` | `0x180003914` |
| `RenderTarget[0].DestBlend` | `blendTbl[RS+0x28]` | `0x18000391f` |
| `RenderTarget[0].BlendOp` | `opTbl[RS+0x2c]` | `0x18000392a` |
| `RenderTarget[0].SrcBlendAlpha` | `blendTbl[RS+0x24]` (again) | `0x180003935` |
| `RenderTarget[0].DestBlendAlpha` | `blendTbl[RS+0x28]` (again) | `0x180003940` |
| `RenderTarget[0].BlendOpAlpha` | `opTbl[RS+0x2c]` (again) | `0x18000394b` |
| `RenderTarget[0].RenderTargetWriteMask` | `(byte)RS+0x30 & 0xF` | `0x180003956` |

`D3D11_RASTERIZER_DESC` — no `memset`, all ten fields assigned.

| Field | Source | Site |
|-------|--------|------|
| `FillMode` | `fillTbl[RS+0x6c]` | `0x1800037d3` |
| `CullMode` | `cullTbl[RS+0x68]` | `0x180003801` |
| `FrontCounterClockwise` | hard `FALSE` | `0x180003805` |
| `DepthBias` | `(INT)(RS+0x3c × 16777215.0f)` via `cvttss2si` | `0x180003827`–`0x18000384b` |
| `DepthBiasClamp` | hard `0.0f` (`xorps`) | `0x18000385e` |
| `SlopeScaledDepthBias` | `RS+0x40 × 4096.0f` | `0x180003863`–`0x180003885` |
| `DepthClipEnable` | hard `TRUE` | `0x180003868` |
| `ScissorEnable` | `RS+0x70 != 0` | `0x18000388a` |
| `MultisampleEnable` | hard `TRUE` | `0x1800038f8` (`mov qword,1`) |
| `AntialiasedLineEnable` | hard `FALSE` | `0x1800038f8` (same store) |

The two float constants: `0x180018c5c` = `0x4B7FFFFF` = `16777215.0f` (2²⁴−1 —
the D24 depth range, matching the `D24_UNORM_S8_UINT` depth buffer this backend
creates) and `0x180018c58` = `0x45800000` = `4096.0f`.

`D3D11_DEPTH_STENCIL_DESC` — no `memset`; every field written except the two
padding bytes at `+0x12`/`+0x13`, which the original leaves as uninitialised
stack (only a `WORD 0xFFFF` is stored at `+0x10`).

| Field | Source | Site |
|-------|--------|------|
| `DepthEnable` | `RS+0x34 != 0` | `0x180003961` |
| `DepthWriteMask` | `RS+0x35 != 0` (`ZERO`=0 / `ALL`=1) | `0x18000396d` |
| `DepthFunc` | `cmpTbl[RS+0x38]` | `0x180003977` |
| `StencilEnable` | `RS+0x44 != 0` | `0x180003983` |
| `StencilReadMask` / `StencilWriteMask` | hard `0xFF` / `0xFF` | `0x180003986` (one `WORD 0xFFFF`) |
| `FrontFace.StencilFailOp` | `stencilTbl[RS+0x50]` | `0x180003994` |
| `FrontFace.StencilDepthFailOp` | `stencilTbl[RS+0x54]` | `0x1800039a0` |
| `FrontFace.StencilPassOp` | `stencilTbl[RS+0x58]` | `0x1800039ac` |
| `FrontFace.StencilFunc` | `cmpTbl[RS+0x48]` | `0x1800039b8` |
| `BackFace.*` when `RS+0x45 == 0` | `KEEP, KEEP, KEEP, ALWAYS` | `0x1800039ca`–`0x1800039e2` |
| `BackFace.StencilFailOp` when two-sided | `stencilTbl[RS+0x5c]` | `0x1800039ec` |
| `BackFace.StencilDepthFailOp` when two-sided | `stencilTbl[RS+0x60]` | `0x1800039f8` |
| `BackFace.StencilPassOp` when two-sided | `stencilTbl[RS+0x64]` | `0x180003a04` |
| `BackFace.StencilFunc` when two-sided | `cmpTbl[RS+0x48]` (front's field) | `0x180003a10` |

Creation order is fixed: **Blend → Rasterizer → DepthStencil**. All three
`HRESULT`s are discarded and the slot returns `true` unconditionally
(`mov al,1` at `0x180003a76`).

## Slot 2 — the bind

```
OMSetBlendState(this[0x10], {0.0f,0.0f,0.0f,0.0f}, 0xFFFFFFFF)
OMSetDepthStencilState(this[0x18], (UINT)(BYTE)renderState[0x4c])
RSSetState(this[0x20])
```

The blend factor is sixteen zero bytes assembled at `[RSP+0x20]`
(`movss` of an `xorps`'d XMM0, then a zero qword at `+0x24` and a zero dword at
`+0x2c`). The sample mask is `or r9d,-1`. The stencil reference is a **byte**
load (`movzx r8d, byte [rax+0x4c]`), consistent with Runtime serializing that
field through the byte virtual.

## Runtime-side imports used by slot 0

Read straight out of VideoDX11's import name table, so the mangled strings are
exact:

| VideoDX11 slot | Mangled name | Runtime addr |
|----------------|--------------|--------------|
| `0x180013348` | `??1RenderStateData@Video@@UEAA@XZ` | `0x1800f8d70` |
| `0x1800134d0` | `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` | — |
| `0x1800134b8` | `?_singleton@Device@Video@@0PEAV12@EA` | `0x1801fe7a8` |
