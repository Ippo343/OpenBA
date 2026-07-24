# dx11/src — clean C++ reimplementations

One file per class or per closely-related group, matching the `*Data` classes in
`../types/VideoDX11Data.h`. Each file registers its own vtable slots with
`DX11_REGISTER_SLOT` (see `../funcs/dx11_seam.h`), so a new class needs no edit
to any shared file. The Makefile globs `dx11/src/*.cpp`, so new files build with
no Makefile change.

## Landed

| File | Covers | State |
|------|--------|-------|
| `devicedata_stubs.cpp` | `DeviceData` slots 4–7 — the four one-liner stubs | default-on (verified in-game 2026-07-24) |
| `dx11_backend_access.{h,cpp}` | Shared plumbing: the `Video::Device -> DeviceData -> ID3D11Device/Context` walk, offset-pinned views of the Runtime-side owner objects (`Video::ShaderProgram`, `Video::VertexLayout`, `Video::RenderBuffer`, `Video::ImageBuffer`), and lazy resolution of the Runtime base destructors | — |
| `shaderprograms.cpp` | `VertexProgramData` (3 slots) + `PixelProgramData` (3 slots) — dtor, `PlatformCreate` (Create{Vertex,Pixel}Shader per permutation), `Bind` ({VS,PS}SetShader with the index-0 fallback) | opt-in `BANISHED_DX11_SHADERS` |
| `vertexlayoutdata.cpp` | `VertexLayoutData` (3 slots) — dtor, `Bind` (IASetInputLayout), `PlatformCreate` (CreateInputLayout; the DLL's only "TEXCOORD" user) | opt-in `BANISHED_DX11_SHADERS` |
| `smallclasses.cpp` | `VideoProgramData` (3 slots, inert) + `RenderBufferData` (4 slots — OMSetRenderTargets / ResolveSubresource) | opt-in `BANISHED_DX11_SHADERS` |

## Still to come

| File | Covers |
|------|--------|
| `device_data.cpp` | The rest of the 20-slot `DeviceData` vtable: dtor, enumerate, create device/swap chain, shutdown, cursor, and the 11 factories. |
| `vertex_buffer_data.cpp` | `VertexBufferData` (0x20) |
| `index_buffer_data.cpp` | `IndexBufferData` (0x20) |
| `constant_buffer_data.cpp` | `ConstantBufferData` (0x28) |
| `render_state_data.cpp` | `RenderStateData` (0x28) |
| `image_buffer_data.cpp` | `ImageBufferData` (0x38) |
| `context_data.cpp` | `ContextData` (0x20) — depends on most of the above |
