# dx11/src — clean C++ reimplementations

Flat layout, one file per class, matching the `*Data` classes in
`../types/VideoDX11Data.h`. Nothing here yet.

Planned files, in the porting order from [../README.md](../README.md):

| File | Covers |
|------|--------|
| `plugin_seam.cpp` | Installs our `CreateDeviceData` / `GetDeviceName` into `Video::Device+0xd0/+0xd8`. Phase 0 — must land first. |
| `device_data.cpp` | The 20-slot `DeviceData` vtable: dtor, enumerate, create device/swap chain, shutdown, cursor, and the 11 factories. |
| `render_buffer_data.cpp` | `RenderBufferData` (0x10, no private fields) |
| `video_program_data.cpp` | `VideoProgramData` (0x10, no private fields) |
| `vertex_layout_data.cpp` | `VertexLayoutData` (0x18) |
| `vertex_buffer_data.cpp` | `VertexBufferData` (0x20) |
| `index_buffer_data.cpp` | `IndexBufferData` (0x20) |
| `constant_buffer_data.cpp` | `ConstantBufferData` (0x28) |
| `render_state_data.cpp` | `RenderStateData` (0x28) |
| `vertex_program_data.cpp` | `VertexProgramData` (0x30) |
| `pixel_program_data.cpp` | `PixelProgramData` (0x30) |
| `image_buffer_data.cpp` | `ImageBufferData` (0x38) |
| `context_data.cpp` | `ContextData` (0x20) — depends on most of the above |

The Makefile already globs `dx11/src/*.cpp`, so new files build with no
Makefile change.
