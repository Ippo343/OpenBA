# dx11/src — clean C++ reimplementations

One file per class or per closely-related group, matching the `*Data` classes in
`../types/VideoDX11Data.h`. Each file registers its own vtable slots with
`DX11_REGISTER_SLOT` (see `../funcs/dx11_seam.h`), so a new class needs no edit
to any shared file. The Makefile globs `dx11/src/*.cpp`, so new files build with
no Makefile change.

## Landed — all twelve classes, all 84 slots, unconditional

Slot counts are `grep -c DX11_REGISTER_SLOT` per file; the total is 84, which is
the number `Dx11SeamInstall()`'s completeness check requires (it refuses a hole).
Nothing here is opt-in any more: the `BANISHED_DX11_SHADERS` gate this table used
to cite **no longer exists** — it is not in `standalone/options.h` and not in any
`dx11/` source — and every slot is registered on every boot. That is DIRECTION §2's
rule ("a port registers unconditionally"), and the last four rows of the table are
verified per-draw against the shipped DLL in RenderDoc — see `dx11/README.md`
§"Per-draw pipeline-state equality".

| File | Covers | Slots |
|------|--------|-------|
| `devicedata.cpp` | `DeviceData` — dtor, `EnumerateInterfaces`, `CreateDevice`, `Shutdown`, `SetImageBuffer` (cursor) and the eleven companion factories | 16 |
| `devicedata_stubs.cpp` | `DeviceData` slots 4–7 — the four one-liner stubs | 4 |
| `contextdata.cpp` | `ContextData` — the whole 20-slot command/draw surface, including `Finish`'s **device-lost recovery** (`Present` → `DXGI_ERROR_DEVICE_REMOVED`/`_RESET` → shutdown + recreate → `RecreateDynamic`), testable with `BANISHED_FORCE_DEVICE_LOST` | 20 |
| `buffers.cpp` | `VertexBufferData` (7) + `IndexBufferData` (7) + `ConstantBufferData` (4). One `Lock`/`Unlock` body serves the first two, which are byte-identical in layout; also the `CbRegistry` mutex (see below) | 18 |
| `imagebufferdata.cpp` | `ImageBufferData` — dtor, attach colour/depth, `Create`, `Bind`, `Update`, `Read` | 7 |
| `renderstatedata.cpp` | `RenderStateData` — dtor, `Create`, `Bind` | 3 |
| `shaderprograms.cpp` | `VertexProgramData` (3) + `PixelProgramData` (3) — dtor, `PlatformCreate` (`Create{Vertex,Pixel}Shader` per permutation), `Bind` (`{VS,PS}SetShader` with the index-0 fallback) | 6 |
| `vertexlayoutdata.cpp` | `VertexLayoutData` — dtor, `Bind` (`IASetInputLayout`), `PlatformCreate` (`CreateInputLayout`; the DLL's only "TEXCOORD" user) | 3 |
| `smallclasses.cpp` | `VideoProgramData` (3, inert) + `RenderBufferData` (4 — `OMSetRenderTargets` / `ResolveSubresource`) | 7 |
| `dx11_backend_access.{h,cpp}` | Shared plumbing: the `Video::Device -> DeviceData -> ID3D11Device/Context` walk and lazy resolution of the Runtime base destructors | — |
| `exports.cpp` | The plugin entry points, and `Dx11_CreateDeviceData` (the readable example of how dx11 allocates a `*Data` and overwrites the base vptr) | — |

## What is deliberately NOT here (ENDGAME B.3.5, RULE B5-8)

The twelve **concrete** `*Data` classes stay C-style structs with a
`DX11_REGISTER_SLOT` table rather than becoming twelve C++ classes, and the
84-slot seam stays. `Dx11SeamInstall()`'s completeness check is the only guarantee
those slots are non-null, and `ContextData_Finish`'s device-lost recovery
dispatches through `DeviceData`'s slots **by index**; converting them means
re-deriving that guarantee. Runtime's twelve **abstract** bases did convert
(`runtime/types/video/CompanionData.h`).

Two consequences a reader should know about:

- A dx11 scalar deleting destructor calls its Runtime base as a **qualified
  non-virtual** `self->Video::XData::~XData()` through a `// B.3.5 bridge` cast
  (RULE B5-3) — never `DeletingDtor(0)`, which would re-enter slot 0.
- The `Imports` / `g_deps` rows for those base destructors **stay** even though
  nothing calls them any more: the row is what makes `g_imOk` / `EnsureDeps()`
  false when the Runtime side is absent, and that all-or-nothing test is the
  seam's only fallback guarantee.

## The Runtime headers this directory includes

There is **no** `DX11_FLAGS` in the Makefile. `dx11/src/*.cpp` and
`dx11/funcs/*.cpp` are folded into `SA_SRCS` and compile with the single shared
include set, which already carries `-Iruntime/types -Iruntime/funcs`. So dx11
includes real Runtime types rather than keeping private views of them, and
B.3.2/B.3.5 have been collapsing those views one owner class at a time:

| included header | replaced this directory's view of |
|---|---|
| `collection/Collection.h` | `RtArray` (B.3.2 step 2) |
| `system/{String,WideString,Error}.h` | — (B.3.4b) |
| `video/CompanionData.h` | the twelve base `*Data` vptr/owner pairs |
| `video/Buffers.h` | `GpuBufferOwner`, a two-field prefix of two classes at once |
| `video/ImageBuffer.h` | `ImageBufferOwner` |
| `video/Device.h` | `RenderBufferOwner`, and `devicedata.cpp`'s four mirrors of `Device::{DisplayMode,RefreshRate,Adapter,GraphicsInterface}` |
| `video/ShaderProgram.h` | `ShaderProgramOwner` |
| `video/VertexLayout.h` | `VertexLayoutOwner` + `VertexLayoutElement` — the one layout this directory was the ONLY home of, until block A authored the header |
| `video/ConstantBuffer.h` | `ConstantBufferOwner` (block VC) |
| `video/Context.h` | `contextdata.cpp`'s local `struct Viewport` (block VC) |
| `io/Serializer.h` | `SerializerStreamMode`'s hand-read stream-direction offsets (block VC, B35_RULES §5.5) |

**Every duplicate view B35_RULES §1.3 tabulated is now collapsed**, and the
`static_assert`s that used to repeat the Runtime-side offsets now check dx11's own
independently-recovered constants AGAINST the shared definition — so a future
divergence breaks this directory's build instead of reading the wrong field. The
one false witness in the old set, `static_assert(sizeof(RenderBufferOwner) ==
0x10)` on a prefix view of a 0x30-byte class, is gone.

## The one non-retail lock in the port

`buffers.cpp`'s `CbRegistryLock()` / `CbRegistry()` and
`Dx11TrackConstantBuffer` / `Dx11Untrack…` / `Dx11InvalidateConstantBufferBindings`
fix a **port-introduced** dangling-binding bug after `ClearState()` on a device
reset. They are touched on create, on destroy and once per device reset — never
per draw. RULE B5-8 keeps them and their comment as they are.
