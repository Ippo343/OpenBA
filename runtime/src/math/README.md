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


## ENDGAME B.3.1 step 1 — real classes (2026-09-13)

The Math value types are no longer PODs plus `RtMath_*` free functions. They are
real C++ classes under retail's names in `namespace Math`
(`runtime/types/math/Math.h`): `Scalar`, `Vector2`, `Vector3`, `Vector4`,
`Quaternion`, `Matrix`, `Transform`, `Transform2`, `Transform3`. Every body in
this directory MOVED onto them — same expressions, same term order, same
branches; only the DLL boundary's hidden return slot is gone, so the methods
return by value. Retail's address stays as a comment on each body.

The `RtMath_*` names survive as one-line shims at the bottom of each file, and
the `RegisterRuntimeExport` rows register the shims, so no caller had to move in
the same commit. **They die in step 2** (`B31_RULES.md`), when each directory's
call sites become member calls. The global type names (`Vector3`, `Transform`,
…) are transitional aliases: `Vector2/3/4` in `engine/types/common.h`, the rest
at the bottom of `Math.h`.

Host unit tests: `tests/unit/math_test.cpp`, run by `mingw32-make unit`.

## Ported here

- `vector.cpp` / `vector.h` — Math::Scalar, Math::Vector2, Math::Vector3, Math::Vector4
  (Wave 1 Lane 1). Byte-faithful.
- `transform.cpp` — Math::Matrix, Math::Quaternion,
  Math::Transform, Math::Transform2, Math::Transform3 (Wave 1 Lane 2), including 3
  dependencies that are genuine Math:: exports in their own right: `Quaternion::operator*(Quaternion)`,
  the 3-`Vector3` basis `Quaternion` ctor, and `Transform::FromDirection`.
  Byte-faithful.
  `Graphics::InterpolatedState<Transform>::Lerp` (0x18001aff0) was left
  unported — its call site does not pin `InterpolatedState<T>`'s layout or
  `Transform::Lerp`'s own signature unambiguously.
