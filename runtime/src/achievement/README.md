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

## Status (ENDGAME B.3.7 closing pass, 2026-09-26)

`Achievement::Achievement` (External, 9 slots, 0x58), `AchievementInstance` (Internal, 0x28), `Goal` (External, abstract, 10 slots, 0x30; slot 9 `GetInstanceType` is the one retslot virtual), `GoalInstance` (Internal, 0x20), the value type `GoalList` (0x10) and the non-polymorphic `AchievementManager` (0x48) are REAL C++ classes in `runtime/types/achievement/{AchievementManager,Goal}.h`. Their ctor/dtor doors stay: the exe subclasses (`Scenario`, five Goals, two GoalInstances, the unregistered AchievementInstance table) construct through them until step 8; the 4 `??_7`, 4 `?_typeInfo` and 4 `RT_DELDTOR` rows retire at the end of B.3. Rules and history: `docs/runtime_port/endgame_b/B37_RULES.md`; numbers: `docs/runtime_port/endgame_b/b37_session/CLOSE_NUMBERS.md`.
