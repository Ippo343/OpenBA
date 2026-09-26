# runtime/src/save — original `code\Runtime\Save\`

Port target for the Runtime-x64.dll `Save` directory. The file list below is the
ORIGINAL source tree recovered from `Runtime-x64-profile.dll` debug strings
(`docs/runtime_port/original_source_tree.txt`); mirror these names here, lowercased,
as `runtime/src/save/<file>.cpp`, with binary-compatible types in
`runtime/types/save/<Class>.h` (`static_assert` on every field offset).

A `_win` file is a PLATFORM SEAM (PLAN.md §4.1 decision 1): keep `<windows.h>` out of
every other file in this directory so a `_posix`/`_sdl` twin can be written later.
A `_compile` file is the offline asset compiler and is likely absent from retail —
confirm against the retail DLL before porting one.

## Original files (2 cpp, 0 h)

- `SavedState.cpp`
- `SaveLoad.cpp`

## Ported here

(This section said "_nothing yet_" until the ENDGAME B.3.7 close; the
status paragraph at the end of this file says what the files hold.)

- `closure_batch2b.cpp`
- `rt_save_port.cpp`
- `savedstate_setupload.cpp`
- `saveload.cpp`
- `saveload_draw.cpp`
- `saveload_internal.h`
- `saveload_loadmeta.cpp`
- `saveload_meta.cpp`
- `saveload_saveinfo.cpp`
- `saveload_update.cpp`
