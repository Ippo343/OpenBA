# SETTINGS-1 hand-in (Phase D.4 wave 1, branch `d4-settings`, base `f0c5032d0`)

Plan: `plans/SETTINGS.md` steps E.1 and E.2 only. E.3-E.6 wait for the user's Q1 / Q2 / Q11.
Static gate only: the game was never run. `git merge main` before the final build: already up to date
(main is still `f0c5032d0`).

## Commits

| commit | title | gate |
|---|---|---|
| `97451586e` | refactor: settings file codec | debug 0 / perf 0 / unit 0; unit 933 / 0 (920 + 13 new); 0 new warnings; castmetric 2 |
| `75517c70c` | refactor: platform settings API; GetSaveDirectory over Platform::DocumentsDir | debug 0 / perf 0 / unit 0; unit 933 / 0; 0 new warnings; castmetric 2 |

## B11 verdicts (perf objects against the main checkout's `runs/b11base_d4/perf/obj`)

- **E.1:** no existing TU touched. `settings_file.o` is a new object with no caller.
- **E.2:** `b11cmp.py` over every object present in both directories: **TOTAL IDENTICAL 16926, ONLY-TIP 1**.
  - `filesystem.o`: IDENTICAL 12, ONLY-TIP 1. The extra function is the moved `GetSaveDirectory`.
  - Every `FileSystem.h` includer is IDENTICAL. The header change is comment-only and keeps the line count.
  - `GetSaveDirectory` was paired by hand: base `filesystem_win.o` against tip `filesystem.o`, with `--detail`.
    The verdict is **DIFF in the call list only**. Stores and compares are equal.
    - The base calls SHGetFolderPathW, the WideString ctor, AsUTF8, three WideString dtors, Format,
      AdjustDelimiters, SetText and MemFree.
    - The tip replaces SHGetFolderPathW, the WideString ctor / AsUTF8 and the WideString dtors with one
      `Platform::DocumentsDir` call.
    - `DocumentsDir` itself, in `settings_win.o`, calls exactly SHGetFolderPathW, the WideString ctor,
      AsUTF8, String move-assign and ~WideString.
    - This is the accepted shape from the brief. The only order change is that the WideString is now
      released inside DocumentsDir, before Format, which is not observable.
  - `settings_win.o` and `settings_file.o` are new objects.

## Deviations from the plan

1. **Names follow D4_RULES C1.** The header is `src/platform/common/platform_settings.h` and the Windows
   body is `src/platform/win/settings_win.cpp`. The plan's names were `settings_platform.h` and
   `settings_platform_win.cpp`.
2. **Include convention.** Code includes the bare name `"platform_settings.h"` through
   `-Isrc/platform/common`, the same as THREADS-1's `"platform_sync.h"`. The Makefile and CMakeLists.txt
   glob / include edits are byte-identical to THREADS-1's edits of the same lines.
3. **`ReadWholeFile` returns `std::string`, not `System::String`.** The codec consumes `std::string`, and a
   file's bytes need a counted buffer. `DocumentsDir` keeps the plan's `System::String&`, so the save path's
   UTF-16 to UTF-8 conversion is still the engine's `WideString::AsUTF8`.
4. **The codec writes one escape the plan does not list.** `\#` is written only for a key that begins with
   `#`, which would otherwise read as a comment. It is accepted anywhere on read, and no retail key begins
   with `#`.
5. **Lenient escape reading.** An unknown escape reads as itself, backslash kept. A lone trailing backslash
   reads as a backslash. The plan does not specify either case.
6. **Legacy import logging.** `ReadLegacySettings` prints one stderr line per skipped non-REG_SZ value,
   prefixed `[Settings] legacy import:`.
7. **The plan's sa-run and resave gates for E.1 and E.2 are the coordinator's.** The save list and
   `Gate.sav` are found through `GetSaveDirectory`.

## Open items for the coordinator

- **Run `sa-run` and `tools/resave_gate/resave_gate.sh Gate` at the merge.** E.2 moves the save-directory
  query.
- **No caller yet for three functions.** `ReadWholeFile`, `WriteFileAtomic` and `ReadLegacySettings` get
  theirs in E.3 (`persistence_settings.cpp`).
- **D4_RULES C3 re-base is still owed.** Once THREADS step 5's file primitives land, one `refactor:` commit
  re-bases `settings_win.cpp`'s ReadWholeFile and WriteFileAtomic on `FileOpenRead` / `FileWrite` /
  `FileMove`.
- **Stale objects in an existing build directory.** A build directory that already held
  `build/{debug,perf}/obj/src__platform__win__filesystem_win.o` keeps that object. It is not linked,
  because the link uses `link_objects.rsp` built from the source list. I removed it from this worktree's
  build directory.
- **`src/engine/system/widestring.cpp` was left untouched.** Its banner still names
  `src/platform/win/filesystem_win.cpp` in a historical paragraph about Wave 4. Changing it would only
  touch a comment.

## Shared files touched (other blocks touch them too)

- `Makefile`: the `RUNTIME_FUNC_SRCS` glob and `SA_INCLUDES` are identical to THREADS-1's lines. The
  `-lshell32` comment now names `settings_win.cpp`.
- `CMakeLists.txt`: the `PLATFORM_SRCS` glob and `BANISHED_INCLUDES` are identical to THREADS-1's lines. The
  `shell32` comment changed.
- `tests/unit/sources.mk`: one block appended at the end. `tests/unit/sources.cmake` was regenerated with
  `tools/restructure/sources_mk_to_cmake.py`. On a conflict, keep both blocks and regenerate.
- `src/engine/io/filesystem.cpp`: the banner and includes changed, and `GetSaveDirectory` was added before
  `FileSystem::_singleton`. Per C4 this merges before THREADS steps 5-6.
- `src/engine/io/FileSystem.h`: two comments changed.
- `src/platform/win/filesystem_win.cpp`: deleted.
