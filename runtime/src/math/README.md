# runtime/src/math — original `code\Runtime\Math\`

Port target for the Runtime-x64.dll `Math` directory. The file list below is the
ORIGINAL source tree recovered from `Runtime-x64-profile.dll` debug strings
(`docs/runtime_port/original_source_tree.txt`); mirror these names here, lowercased,
as `runtime/src/math/<file>.cpp`, with binary-compatible types in
`runtime/types/math/<Class>.h` (`static_assert` on every field offset).

A `_win` file is a PLATFORM SEAM (PLAN.md §4.1 decision 1): keep `<windows.h>` out of
every other file in this directory so a `_posix`/`_sdl` twin can be written later.
A `_compile` file is the offline asset compiler and is likely absent from retail —
confirm against the retail DLL before porting one.

## Original files (0 cpp, 0 h)


## Ported here

- `vector.cpp` / `vector.h` — Math::Scalar, Math::Vector2, Math::Vector3, Math::Vector4
  (Wave 1 Lane 1). 28 retail functions registered from `rt_math_port.cpp`'s
  `RtMathPortInit()`. Byte-faithful.
- `rt_math_port.cpp` — `RtMathPortInit()`, the Math namespace entry
  point. Registers Lane 1's exports then calls Lane 2's
  `RtMathTransformRegister()` (Matrix/Quaternion/Transform family).
- `transform.cpp` / `transform.h` — Math::Matrix, Math::Quaternion,
  Math::Transform, Math::Transform2, Math::Transform3 (Wave 1 Lane 2). 28
  retail functions registered (25 assigned + 3 dependencies that are genuine
  Math:: exports in their own right: `Quaternion::operator*(Quaternion)`,
  the 3-`Vector3` basis `Quaternion` ctor, and `Transform::FromDirection`).
  Byte-faithful.
  `Graphics::InterpolatedState<Transform>::Lerp` (0x18001aff0) was left
  unported — its call site does not pin `InterpolatedState<T>`'s layout or
  `Transform::Lerp`'s own signature unambiguously; see the comment above
  `RtMathTransformRegister()` in transform.cpp.
