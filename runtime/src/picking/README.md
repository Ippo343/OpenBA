# runtime/src/picking — original `code\Runtime\Picking\`

Port target for the Runtime-x64.dll `Picking` directory. The file list below is the
ORIGINAL source tree recovered from `Runtime-x64-profile.dll` debug strings
(`docs/runtime_port/original_source_tree.txt`); mirror these names here, lowercased,
as `runtime/src/picking/<file>.cpp`, with binary-compatible types in
`runtime/types/picking/<Class>.h` (`static_assert` on every field offset).

A `_win` file is a PLATFORM SEAM (PLAN.md §4.1 decision 1): keep `<windows.h>` out of
every other file in this directory so a `_posix`/`_sdl` twin can be written later.
A `_compile` file is the offline asset compiler and is likely absent from retail —
confirm against the retail DLL before porting one.

## Original files (1 cpp, 1 h)

- `PickingMesh.h`
- `PickingMesh_compile.cpp`

## Ported here

(This section said "_nothing yet_" until the ENDGAME B.3.7 close; the
status paragraph at the end of this file says what the files hold.)

- `closure_batch2b.cpp`
- `pickingmesh.cpp`
- `rt_factories_picking.cpp`
- `rt_factories_picking.inc`
- `rt_picking_port.cpp`
- `scene.cpp`
- `scene_grid.cpp`
- `scene_grid.h`
