# runtime/src/object — original `code\Runtime\Object\`

Port target for the Runtime-x64.dll `Object` directory. The file list below is the
ORIGINAL source tree recovered from `Runtime-x64-profile.dll` debug strings
(`docs/runtime_port/original_source_tree.txt`); mirror these names here, lowercased,
as `runtime/src/object/<file>.cpp`, with binary-compatible types in
`runtime/types/object/<Class>.h` (`static_assert` on every field offset).

A `_win` file is a PLATFORM SEAM (PLAN.md §4.1 decision 1): keep `<windows.h>` out of
every other file in this directory so a `_posix`/`_sdl` twin can be written later.
A `_compile` file is the offline asset compiler and is likely absent from retail —
confirm against the retail DLL before porting one.

## Original files (7 cpp, 6 h)

- `CompiledExternal.cpp`
- `External.cpp`
- `ExternalCompiler.cpp`
- `ExternalCompiler.h`
- `ExternalFactory.cpp`
- `ExternalFactory.h`
- `ExternalList.cpp`
- `ExternalList.h`
- `Factory.cpp`
- `Factory.h`
- `InternalFactory.cpp`
- `InternalFactory.h`
- `InternalPtr.h`

## Ported here

_nothing yet_
