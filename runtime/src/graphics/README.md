# runtime/src/graphics — original `code\Runtime\Graphics\`

Port target for the Runtime-x64.dll `Graphics` directory. The file list below is the
ORIGINAL source tree recovered from `Runtime-x64-profile.dll` debug strings
(`docs/runtime_port/original_source_tree.txt`); mirror these names here, lowercased,
as `runtime/src/graphics/<file>.cpp`, with binary-compatible types in
`runtime/types/graphics/<Class>.h` (`static_assert` on every field offset).

A `_win` file is a PLATFORM SEAM (PLAN.md §4.1 decision 1): keep `<windows.h>` out of
every other file in this directory so a `_posix`/`_sdl` twin can be written later.
A `_compile` file is the offline asset compiler and is likely absent from retail —
confirm against the retail DLL before porting one.

## Original files (18 cpp, 10 h)

- `Animation.cpp`
- `AnimationGroup.h`
- `AnimationGroup_compile.cpp`
- `AnimationManager.cpp`
- `AnimationManager.h`
- `AnimationState.cpp`
- `EdgeMesh.h`
- `EdgeMesh_compile.cpp`
- `FrameCopyImageBuffer.cpp`
- `FrameCopyImageBuffer.h`
- `Geometry.cpp`
- `GraphicsMesh.h`
- `GraphicsMesh_pc.cpp`
- `GraphicsMeshInstance.cpp`
- `GraphicsScene.cpp`
- `GraphicsScene.h`
- `Material.h`
- `MaterialInstance.h`
- `Particle.cpp`
- `ParticleInstance.cpp`
- `ParticleSystem.cpp`
- `ParticleSystem.h`
- `ParticleSystemInstance.cpp`
- `ReflectionImageBuffer.cpp`
- `ReflectionImageBuffer.h`
- `Renderer.cpp`
- `SpotLight.cpp`
- `SunLight.cpp`

## Ported here

_nothing yet_
