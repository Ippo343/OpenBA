# runtime/src/collision2d — original `code\Runtime\Collision2D\`

Port target for the Runtime-x64.dll `Collision2D` directory. The file list below is the
ORIGINAL source tree recovered from `Runtime-x64-profile.dll` debug strings
(`docs/runtime_port/original_source_tree.txt`); mirror these names here, lowercased,
as `runtime/src/collision2d/<file>.cpp`, with binary-compatible types in
`runtime/types/collision2d/<Class>.h` (`static_assert` on every field offset).

A `_win` file is a PLATFORM SEAM (PLAN.md §4.1 decision 1): keep `<windows.h>` out of
every other file in this directory so a `_posix`/`_sdl` twin can be written later.
A `_compile` file is the offline asset compiler and is likely absent from retail —
confirm against the retail DLL before porting one.

## Original files (0 cpp, 2 h)

- `Collision2DMesh.h`
- `Surface.h`

## Ported here

_nothing yet_
