# runtime/src/io — original `code\Runtime\IO\`

Port target for the Runtime-x64.dll `IO` directory. The file list below is the
ORIGINAL source tree recovered from `Runtime-x64-profile.dll` debug strings
(`docs/runtime_port/original_source_tree.txt`); mirror these names here, lowercased,
as `runtime/src/io/<file>.cpp`, with binary-compatible types in
`runtime/types/io/<Class>.h` (`static_assert` on every field offset).

A `_win` file is a PLATFORM SEAM (PLAN.md §4.1 decision 1): keep `<windows.h>` out of
every other file in this directory so a `_posix`/`_sdl` twin can be written later.
A `_compile` file is the offline asset compiler and is likely absent from retail —
confirm against the retail DLL before porting one.

## Original files (17 cpp, 5 h)

- `BinaryEncoding.cpp`
- `BinaryFileStream.cpp`
- `BinaryReader.cpp`
- `CompressedStream.cpp`
- `FileSystem.cpp`
- `FileSystem.h`
- `MemoryStream.cpp`
- `Modification.cpp`
- `PackageFile.cpp`
- `PackageFile.h`
- `PackageFile.h`
- `PackageFile_compile.cpp`
- `Persistence.cpp`
- `Persistence.h`
- `Registry_win.cpp`
- `Stream.cpp`
- `TextEncoding.cpp`
- `TextFile.h`
- `TextFileStream.cpp`
- `TextReader.cpp`
- `VisitorEncoding.cpp`
- `VisitorStream.cpp`

## Ported here

- `serializer.cpp`
- `serializer.h`
