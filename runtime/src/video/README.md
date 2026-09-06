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

_nothing yet_
