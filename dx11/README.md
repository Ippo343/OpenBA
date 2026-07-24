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

**Those two slots are not usable as the seam.** The scaffold originally proposed
overwriting `+0xd0`/`+0xd8` between `InitializeInterfaces` and
`CreateVideoInterface`, with a hybrid "delegating `DeviceData`" behind them.
Implementing Phase 0 killed that plan on three counts:

1. **There is no window.** `InitializeInterfaces` *and* `CreateVideoInterface`
   both run inside a single `Engine::Startup` call. The exe never gets control
   between them, so there is no point at which the slots can be swapped.
2. **A delegate splits the object.** Every resource `Bind` in the DLL reads
   `g_Device->[0x100]->[0x28]` to find the D3D11 context — i.e. un-ported
   original code reads fields out of whichever `DeviceData` Runtime was handed.
   With two objects, ported and un-ported slots would read different state.
3. **`DeviceData+0x60` is a live registry** of `ImageBuffer*`s, appended by
   `0x180002300` and swap-removed by `~ImageBufferData`. Split across two
   objects it stops being a registry.

### The seam that works: patch the vtable slots

The twelve vtables sit in the shipped DLL's `.rdata`. Phase 0 `VirtualProtect`s
them writable, snapshots all 84 original slot targets, and overwrites individual
slots with our reimplementations.

```
Dx11RegisterSlot(Vt::IndexBufferData, 5, &IndexBufferData_Bind,
                 "BANISHED_DX11_IB_BIND", /*defaultOn=*/false,
                 "IndexBufferData::Bind");
...
Dx11SeamInstall();   // verify 84/84, then apply the enabled ones
```

This is strictly better than the delegate:

- **One object, the real one.** Our slot receives the genuine `DeviceData` with
  the genuine fields, so ported and un-ported slots share state by construction.
- **No ordering constraint.** Install any time after the DLL is loaded. We do it
  in `standalone/main.cpp` right after `Dx11FuncsInit()`, before `Engine::Startup`.
- **Works for all twelve classes** without touching any factory.
- **One slot = one unit of work**, exactly like an entry in
  `standalone/address_registry.cpp`. `Dx11SetSlotEnabled()` flips a single slot
  at runtime, so two RenderDoc captures differ by exactly one function.

`Dx11OriginalSlot(vt, slot)` returns the captured original, so a partially-ported
slot can still call through.

**Install-time verification.** `Dx11SeamInstall()` checks all 84 live slot
pointers against the address map in this file *before writing anything*, and
aborts the whole install on a single mismatch. That re-proves the vtable walk
against the shipped DLL on every boot, and makes a wrong ASLR delta or a
different build of `VideoDX11-x64.dll` a logged refusal instead of a corrupted
vtable. First run: `[Dx11Seam] Verified 84/84 vtable slots`.

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
| **0 ✅** | **Seam installer — DONE.** `dx11/funcs/dx11_seam.{h,cpp}`, installed from `standalone/main.cpp`. Verifies 84/84 slots, patches none. | Pure infrastructure. Verified in-game 2026-07-24: `Verified 84/84`, `0 slot(s) ours`, autostart reached the world build, empty crash log. |
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
| x | 4 | `0x180001190` | `ret` — no-op (shared with `VideoProgramData` slot 2) |
| x | 5 | `0x180005430` | `xorps xmm0,xmm0; ret` — returns `0.0f` |
| x | 6 | `0x180005440` | `xor al,al; ret` — returns `false` |
| x | 7 | `0x180001060` | `mov al,1; ret` — returns `true` (shared with `RenderBufferData`/`VideoProgramData` slot 1) |
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

Walked 2026-07-24 by reading `.rdata` `0x180018580`–`0x180018c60` directly. Each
vtable's extent is the run of code pointers before the next vtable or the next
string/GUID literal — the extents are **not** contiguous, string data is
interleaved (`"DirectX 11"` sits between `VideoProgramData` and
`ConstantBufferData`; `"TEXCOORD"` between `VertexBufferData` and
`VertexLayoutData`). `RenderBufferData`'s base at `0x180018588` is confirmed
independently by the `lea rax,[rip+0x17554]` vptr store in its dtor.

| Status | Class | Vtable | Slots | Extent | Notes |
|--------|-------|--------|-------|--------|-------|
| x | `RenderBufferData` | `0x180018588` | **4** | `…588`–`…5a7` | 0 private bytes — cheapest |
| x | `VideoProgramData` | `0x1800185a8` | **3** | `…5a8`–`…5bf` | 0 private bytes; slots 1–2 are shared stubs |
| x | `ConstantBufferData` | `0x1800185d0` | **4** | `…5d0`–`…5ef` | |
| x | `ContextData` | `0x1800186f0` | **20** | `…6f0`–`…78f` | biggest vtable of the resource classes |
| x | `ImageBufferData` | `0x180018790` | **7** | `…790`–`…7c7` | largest resource *struct* (0x38) |
| x | `IndexBufferData` | `0x1800187c8` | **7** | `…7c8`–`…7ff` | slots 3–4 shared with `VertexBufferData` |
| x | `PixelProgramData` | `0x180018800` | **3** | `…800`–`…817` | |
| x | `RenderStateData` | `0x180018818` | **3** | `…818`–`…82f` | |
| x | `VertexBufferData` | `0x180018830` | **7** | `…830`–`…867` | slots 3–4 shared with `IndexBufferData` |
| x | `VertexLayoutData` | `0x180018878` | **3** | `…878`–`…88f` | |
| x | `VertexProgramData` | `0x180018890` | **3** | `…890`–`…8a7` | |
| x | `DeviceData` | `0x180018bb0` | **20** | `…bb0`–`…c4f` | (listed slot-by-slot above) |

**84 vtable slots** across 12 classes, resolving to **79 distinct addresses**
(five targets are shared between vtables).

### Function roster — all 85 attributed

Ghidra defines 85 functions in `0x180001000`–`0x180005b7f`: one import thunk
(`MemFree`) plus 84 real functions. **67** of them are vtable targets; the other
**18** are the two exports, four shared utilities, and twelve class-private
helpers. Every one is attributed below.

A further **12 vtable targets are entry points Ghidra never split into their own
functions** — they are 3-to-30-byte tails living inside a preceding function's
body. They must be treated as real functions when porting. All twelve are
decoded here:

| Address | Bytes | Meaning | Used by |
|---------|-------|---------|---------|
| `0x180001060` | `b0 01 c3` | `return true` | `RenderBufferData`[1], `VideoProgramData`[1], `DeviceData`[7] |
| `0x180001190` | `c2 00 00` | `return` (void) | `VideoProgramData`[2], `DeviceData`[4] |
| `0x180001450` | `48 8b 41 08 / 8b 40 14 / ff c0 / c3` | `return owner->[0x14] + 1` | `ConstantBufferData`[3] |
| `0x180001d10` | tail-jmp `[ctx+0x98]` | `IASetIndexBuffer(NULL, R16_UINT, 0)` | `ContextData`[10] |
| `0x180001ef0` | `b8 34 00 00 00 / c3` | `return 52` | `ContextData`[13] |
| `0x180002020` | `e9 2b 01 00 00` | `jmp 0x180002150` | `ContextData`[17] (alias of slot 19) |
| `0x180003380` | tail-jmp `[ctx+0x98]` | `IASetIndexBuffer(this->[0x10], R16_UINT, 0)` | `IndexBufferData`[5] |
| `0x1800036a0` | tail-jmp `[ctx+0x48]` | `PSSetShader(this->variants[i ?: 0], NULL, 0)` | `PixelProgramData`[2] |
| `0x180003fc0` | tail-jmp `[ctx+0x88]` | `IASetInputLayout(this->[0x10])` | `VertexLayoutData`[1] |
| `0x180004230` | tail-jmp `[ctx+0x58]` | `VSSetShader(this->variants[i ?: 0], NULL, 0)` | `VertexProgramData`[2] |
| `0x180005430` | `0f 57 c0 / c3` | `return 0.0f` | `DeviceData`[5] |
| `0x180005440` | `32 c0 / c3` | `return false` | `DeviceData`[6] |

So the real entry-point count for the port is **96** (84 functions + 12 stubs),
not 85.

#### Shared / infrastructure (4)

| Address | Role |
|---------|------|
| `0x180001000` | `MemFree` import thunk (not ported — Runtime's allocator) |
| `0x180001520` | free-and-null an array `{void* data; int count;}` — reached **only** from EH unwind funclets |
| `0x180001550` | generic `Array<T*>::SetLength(int)` — grow ×3, `MemAlloc`/copy/`MemFree` |
| `0x180003060` | tail-thunk to `Video::Device::Unlock(*g_Device)` — EH unwind path |

#### Exports (2)

`0x1800011a0` `CreateDeviceData`, `0x180001230` `GetDeviceName`.

#### Per-class attribution

| Class | Vtable slot targets (in slot order) | Private helpers |
|-------|-------------------------------------|-----------------|
| `RenderBufferData` | `1010`, `1060`*, `1070`, `10e0` | — |
| `VideoProgramData` | `1140`, `1060`*, `1190`* | — |
| `ConstantBufferData` | `1240`, `1300`, `1460`, `1450`* | `1270` (`~ConstantBufferData`) |
| `ContextData` | `1600`, `1650`, `1720`, `17c0`, `1920`, `1ad0`, `1b60`, `1be0`, `1c40`, `1cb0`, `1d10`*, `1d30`, `1d80`, `1ef0`*, `1e60`, `1f00`, `2000`, `2020`*, `2030`, `2150` | — |
| `ImageBufferData` | `2400`, `2580`, `2650`, `2870`, `2da0`, `2e10`, `2ef0` | `2430` (`~ImageBufferData`), `2720` |
| `IndexBufferData` | `3080`, `30f0`, `31c0`, `3290`†, `3340`†, `3380`*, `33b0` | — |
| `PixelProgramData` | `3580`, `3610`, `36a0`* | — |
| `RenderStateData` | `36e0`, `3760`, `3a90` | — |
| `VertexBufferData` | `3b20`, `3b90`, `3c60`, `3290`†, `3340`†, `3d30`, `3d90` | — |
| `VertexLayoutData` | `3f60`, `3fc0`*, `3ff0` | — |
| `VertexProgramData` | `4110`, `41a0`, `4230`* | — |
| `DeviceData` | `4270`, `4690`, `4e10`, `51c0`, `1190`*, `5430`*, `5440`*, `1060`*, `5450`, `5650`, `56c0`, `5730`, `57a0`, `5810`, `5880`, `58f0`, `5960`, `59d0`, `5a40`, `5aa0` | `42d0`, `4340`, `4510`, `4590`, `52b0`, `5350`, `53f0`, `5b00`, and `2300` (shared with `ImageBufferData`) |

`*` = one of the 12 sub-function stubs above. `†` = shared between
`IndexBufferData` and `VertexBufferData` (identical Lock/Unlock semantics).
All addresses are `0x180000000`-relative, `0x1800…` prefix elided.

#### `DeviceData` helper roles (recovered)

| Address | Role |
|---------|------|
| `0x1800042d0` | Win32 message callback — installed by slot 2 via `Application::RegisterMessageCallback`; calls `0x180004340` twice |
| `0x180004340` | swap-chain resize / buffer recreate; also called directly by `ContextData` slot 1 |
| `0x180004510` | `ResolveMultisampleMode()` — clamps `Device+0x24` against `GetMultisampleModeCount`, walks down to a supported mode, returns `sampleCount` (index+1) |
| `0x180004590` | `FillSwapChainDesc(DXGI_SWAP_CHAIN_DESC*)` — see field notes in `../decomp/videodx11.md` |
| `0x1800052b0` | `CreateColorBuffer()` → `CreateTexture2D(B8G8R8A8_UNORM, BIND_RENDER_TARGET)` into `+0x38` |
| `0x180005350` | `CreateDepthBuffer()` → `CreateTexture2D(D24_UNORM_S8_UINT, BIND_DEPTH_STENCIL)` into `+0x30` |
| `0x1800053f0` | second Win32 message callback installed by slot 2 |
| `0x180005b00` | adapter/output enumeration helper for slot 1 |
| `0x180002300` | `TrackImageBuffer(ImageBuffer*)` — add-if-absent into the `DeviceData+0x60` list; the matching remove lives in `~ImageBufferData` |

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
