# runtime/src/video — original `code\Runtime\Video\`

Port target for the Runtime-x64.dll `Video` directory. The file list below is the
ORIGINAL source tree recovered from `Runtime-x64-profile.dll` debug strings
(`docs/runtime_port/original_source_tree.txt`); mirror these names here, lowercased,
as `runtime/src/video/<file>.cpp`, with binary-compatible types in
`runtime/types/video/<Class>.h` (`static_assert` on every field offset).

A `_win` file is a PLATFORM SEAM (PLAN.md §4.1 decision 1): keep `<windows.h>` out of
every other file in this directory so a `_posix`/`_sdl` twin can be written later.
A `_compile` file is the offline asset compiler and is likely absent from retail —
confirm against the retail DLL before porting one.

## Original files (11 cpp, 10 h)

- `ConstantBuffer.cpp`
- `Context.cpp`
- `Context.h`
- `ImageBuffer.h`
- `ImageBuffer_win.cpp`
- `IndexBuffer_win.cpp`
- `PixelProgram.h`
- `RenderBuffer.cpp`
- `RenderState.h`
- `ShaderProgram_win.cpp`
- `VertexBuffer_win.cpp`
- `VertexLayout.cpp`
- `VertexLayout.h`
- `VertexProgram.h`
- `VideoConfiguration.cpp`
- `VideoConfiguration.h`
- `VideoDevice.cpp`
- `VideoDevice.h`
- `VideoDevice_win.h`
- `VideoProgram.cpp`
- `VideoProgram.h`

## Ported here

The "_nothing yet_" that stood here until 2026-09-12 was already false when it was
written, and the six-line class list that replaced it was badly out of date by
ENDGAME B.3.5. **Thirty-four `.cpp` files live here and every `Video::` class in
the original tree has a real C++ class behind it.** The table below is the
directory as it is, grouped by the class that owns it, not by the wave that wrote
it — the wave and lane are in each file's own banner.

| class (`runtime/types/video/…`) | bodies |
|---|---|
| `Video::Device` + `Configuration` + the five nested value types (`Device.h`) | `device.cpp`, `device_startup.cpp`, `device_enum_types.cpp`, `video_resources.cpp`, `video_mode_sort.cpp`, `enumerate_video_modes.cpp`, and the seams `device_startup_win.cpp`, `device_adjustdisplay_win.cpp`, `video_interfaces_win.cpp`, `video_resources_win.cpp`, `buffers_win.cpp` |
| `Video::Context` (`Context.h`) | `context.cpp`, `context_ctor.cpp`, `context_lifecycle.cpp`, `context_bind.cpp`, `batchc_context_setlight.cpp` |
| `Video::Viewport` (`Context.h`) | `context.cpp` (the two comparison operators) |
| `Video::ConstantBuffer` (`ConstantBuffer.h`) | `constant_buffer.cpp`; the ctor is inlined ×7 in `context_ctor.cpp` and `~ConstantBuffer` is a door in `context_lifecycle.cpp` |
| `Video::RenderBuffer`, `IndexBuffer`, `VertexBuffer` (`Device.h`, `Buffers.h`) | `buffers.cpp`, `dynamic_resources.cpp`, `video_resources.cpp`, `device.cpp` |
| `Video::ImageBuffer` (`ImageBuffer.h`) | `imagebuffer_vtable.cpp`, `imagebuffer_platform_serialize.cpp`, `save_image_to_file.cpp`, `buffers.cpp`, `dynamic_resources.cpp`, `video_resources.cpp` |
| `Video::ShaderProgram` / `VertexProgram` / `PixelProgram` (`ShaderProgram.h`) | `shaderprogram.cpp`, `vertexprogram.cpp`, `pixelprogram.cpp`, `shader_program_names.cpp`, `closure_batch2b.cpp` |
| `Video::VideoProgram` (`VideoProgram.h`) | `video_program.cpp`, `videoprogram.cpp` |
| `Video::VertexLayout` (`VertexLayout.h`) | `vertexlayout.cpp` |
| `Video::RenderState` (`RenderState.h`) | `renderstate.cpp` (with `Stencil` / `Depth` / `AlphaBlend`) |
| the twelve abstract `Video::*Data` bases (`CompanionData.h`, `ImageBuffer.h`) | `companion_data.cpp` — all 24 ctor/dtor bodies in one file |
| reflection | `rt_factories_video.cpp` (+ `rt_factories_video.inc`) |
| registration | `rt_video_port.cpp` |

`closure_batch2b.cpp` is the one file here that is not a class's: it holds two
unexported leaves, `Video::ShaderProgram`'s blob-array serializer helper
(0x180054080) and `Volume::Frustum`'s box support-vertex selector (0x1800f8fb0).

## Conventions this directory follows

- **Doors, not ctors.** No `Video::` class here gets a real constructor
  (ENDGAME §B.2 / RULE B4b-2): every `??0…` export stays a registered DOOR,
  because it runs on raw `MemoryAllocator` bytes whose first field it then writes
  with the MINTED vtable address, and because `Context` holds seven
  `ConstantBuffer`s and two `Viewport`s by value and nine files treat its 0x2168
  bytes as a flat block. `std::is_trivially_copyable` is asserted on
  `Device`, `Configuration`, `RenderBuffer`, `IndexBuffer`, `VertexBuffer`,
  `ConstantBuffer`, `Viewport` and `Context` and is what enforces it.
- **No virtual destructor.** Slot 0 of every polymorphic class is
  `DeletingDtor(unsigned)`, whose body is a table row in
  `runtime/src/object/deleting_dtor.cpp`. `~Class()` is non-virtual and in no
  slot, so a door calls it as a QUALIFIED non-virtual
  `p->Video::C::~C()` — never through slot 0, which would re-enter.
- **The Device lock is byte-faithful** (RULE B5-6): an opaque `uint8_t lock[0x28]`
  at `Device+0x60` with a hand-rolled recursive acquire in `buffers_win.cpp`, and
  the owner-tid / depth words at pinned +0x88 / +0x8c that `dx11/` and the four
  `Platform{Construct,Destruct}` bodies read. Not a `std::mutex`, not a guard.
- **The class statics stay `RT_DATA_SLOT` reads** — `Device::_singleton`,
  `Configuration::_singleton`, `ImageBuffer::_firstSurface`,
  `{Index,Vertex}Buffer::_firstBuffer`, `RenderBuffer::_firstRenderBuffer`. The
  `MintStorageCell()` redirect that would make them C++ statics is owed at the
  class-static conversion, the END of B.3 (RULE B4b-4 / B35_RULES §9).
- **`InitializeInterfaces` keeps its DELETED `LoadLibraryW` arm** (RULE B5-9,
  `video_interfaces_win.cpp`): retail mapped `VideoDX9-x64.dll` behind
  `ResolveInit`'s back and faulted on the first heap call in the mapped image. A
  real class must not "restore" the faithful body.
- **`enumerate_video_modes.cpp`'s `__try`/`__except` is left alone** — §B.1.7 puts
  exceptions last.
