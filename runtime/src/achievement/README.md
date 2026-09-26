# runtime/src/achievement — original `code\Runtime\Achievement\`

Port target for the Runtime-x64.dll `Achievement` directory. The file list below is the
ORIGINAL source tree recovered from `Runtime-x64-profile.dll` debug strings
(`docs/runtime_port/original_source_tree.txt`); mirror these names here, lowercased,
as `runtime/src/achievement/<file>.cpp`, with binary-compatible types in
`runtime/types/achievement/<Class>.h` (`static_assert` on every field offset).

A `_win` file is a PLATFORM SEAM (PLAN.md §4.1 decision 1): keep `<windows.h>` out of
every other file in this directory so a `_posix`/`_sdl` twin can be written later.
A `_compile` file is the offline asset compiler and is likely absent from retail —
confirm against the retail DLL before porting one.

## Original files (2 cpp, 3 h)

- `Achievement.cpp`
- `Achievement.h`
- `AchievementInstance.h`
- `AchievementManager.cpp`
- `GoalInstance.h`

## Ported here

(This section said "_nothing yet_" until the ENDGAME B.3.7 close; the
status paragraph at the end of this file says what the files hold.)

- `achievement_manager.cpp`
- `achievement_serialize.cpp`
- `achievement_state.cpp`
- `achievement_typeinfo_accessors.cpp`
- `achievement_vtable.cpp`
- `batchc_goallist_dtor.cpp`
- `goal.cpp`
- `goal_list.cpp`
- `goal_serialize.cpp`
- `rt_achievement_port.cpp`
- `rt_factories_achievement.cpp`
- `rt_factories_achievement.inc`
