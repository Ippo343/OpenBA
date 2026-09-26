# runtime/src/pathfinding — original `code\Runtime\Pathfinding\`

Port target for the Runtime-x64.dll `Pathfinding` directory. The file list below is the
ORIGINAL source tree recovered from `Runtime-x64-profile.dll` debug strings
(`docs/runtime_port/original_source_tree.txt`); mirror these names here, lowercased,
as `runtime/src/pathfinding/<file>.cpp`, with binary-compatible types in
`runtime/types/pathfinding/<Class>.h` (`static_assert` on every field offset).

A `_win` file is a PLATFORM SEAM (PLAN.md §4.1 decision 1): keep `<windows.h>` out of
every other file in this directory so a `_posix`/`_sdl` twin can be written later.
A `_compile` file is the offline asset compiler and is likely absent from retail —
confirm against the retail DLL before porting one.

## Original files (2 cpp, 1 h)

- `PathScene.cpp`
- `PointList.h`
- `PointList_compile.cpp`

## Ported here

(This section said "_nothing yet_" until the ENDGAME B.3.7 close; the
status paragraph at the end of this file says what the files hold.)

- `pointlist.cpp`
- `rt_factories_pathfinding.cpp`
- `rt_factories_pathfinding.inc`

## Status (ENDGAME B.3.7 closing pass, 2026-09-26)

`Pathfinding::PointList` (External, 9 slots, 0x50) is a REAL C++ class (`runtime/types/pathfinding/PointList.h`, bodies in `pointlist.cpp`) and is LIVE: every `InteractDescription` / `ParticleDescription` the game loads loads one through the factory. `Pathfinding::Scene` (0x90) and `IPathInfo` (8 bytes, two pure slots) are declarations only — dead in retail (B37_RULES 6.2, B7-9). Rules and history: `docs/runtime_port/endgame_b/B37_RULES.md`; numbers: `docs/runtime_port/endgame_b/b37_session/CLOSE_NUMBERS.md`.
