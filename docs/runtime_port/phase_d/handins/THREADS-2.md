# THREADS-2 hand-in (Phase D.4 wave 2b, branch `d4-threads2`, base `5f9dcb6d2`)

Plan: `plans/THREADS.md` steps E.5 to E.8, then the SETTINGS C3 re-base (D4_RULES §2 wave 2b).
E.9, the POSIX implementation, is wave 3. Every commit is `refactor:`.

Static gate only. The game, `sa-run` and `sa-deploy` were never run, and the real `settings.cfg` and
the registry were never touched. Main (`0b67b6111`, with RENDER-3 and SDL-B) was merged before the
final build.

## Commits

| commit | title | gate |
|---|---|---|
| `cc374666b` | refactor: the ten file / stream `_win` files move to src/engine/io (pure move, E.5) | debug / perf / unit EXIT 0; unit 953 / 0; 0 new warnings; castmetric 2; abi OK on a raised baseline |
| `7ac0f25a3` | refactor: file I/O over Platform:: file primitives (E.5) | same; abi lowered below the block base |
| `179e5bb5a` | refactor: filesystem_find / filesystem_setupdirs / external_path move to src/engine/io (pure move, E.6) | same; one renamed baseline warning (below) |
| `8a2b69a5d` | refactor: directory ops and paths over Platform:: primitives (E.6) | same; 0 new warnings |
| `816ec4a8a` | refactor: DateTime formatting, COM, the thread-id and readable-page probes over Platform:: (E.7) | same |
| `fcb2ae217` | refactor: the Windows-only diagnostics move to src/diag/win (pure move, E.8) | same |
| `4deabe98c` | refactor: the crash handler leaves src/app/main.cpp for src/diag/win (E.8) | same |
| `4f70b83f4` | refactor: settings.cfg's file layer over the Platform:: file primitives (D4_RULES C3) | same; the 8 persistence_settings tests pass |
| `a26370e95` | Merge main into d4-threads2 (RENDER-3, SDL-B in) | debug / perf / unit EXIT 0; unit 970 / 0; 0 new warnings; castmetric 2; abi OK |

Warnings were compared the way the brief says. The baseline files are CRLF, so `\r` is stripped
before `comm`. The CMake twin was configured and not built. `cmake -S . -B build/cmake-debug -G
Ninja` exits 0, and `build.ninja` lists the five `src/diag/win` files, `file_win.cpp`, `os_win.cpp`
and the moved io files.

## What landed

- **`src/platform/common/platform_file.h` and `src/platform/win/file_win.cpp`:**
  - `Platform::PathChar` (`wchar_t`): the one line ABI C.5 flips.
  - `FileHandle` and `FindHandle`: pointers to incomplete types, 8 bytes each.
  - `kInvalidFileHandle`, `kInvalidFindHandle`, `kMaxPath`, `kDateTimeChars`.
  - E.5: `FileOpenRead / FileCreateTemp / FileRead / FileWrite / FileSeek32 / FileSize / FileClose /
    FileLastWrite / FileDelete / FileMove / DirCreate`.
  - E.6: `PathAttributes`, `ExecutablePath`, `FindFirst / FindNext / FindClose` over
    `Platform::FindData`. FindData is `WIN32_FIND_DATAW` field for field, with every offset asserted.
  - E.7: `FormatLocalDateTime`.
  - C3: `FileOpenReadShared / FileCreateWrite / FileFlush / FileReplace`.
  - Primitive results keep Win32's shapes: a BOOL is an `int32_t`, and byte counts and offsets are
    `uint32_t`. Callers therefore compile to the same tests.
- **`src/platform/common/platform_os.h` and `src/platform/win/os_win.cpp`:** `ComInit`, `ComUninit`
  and `IsReadablePage`.
- **`IO::FileInfo::winHandle` (+0x18)** is a `Platform::FileHandle`. The offset asserts are
  unchanged. `tests/unit/b39_io_classes_test.cpp` no longer includes `<windows.h>`.
- **Moved files.** Thirteen files left `src/platform/win` for `src/engine/io`. Two took a different
  basename because `src/engine/io` already holds the name:
  - `file_stream_win.cpp` became `file_stream_write.cpp`.
  - `persistence_saves_win.cpp` became `persistence_begin_saveinfo.cpp`.
- **`src/diag/win/`** holds `boot_state_watchdog`, `hw_watch`, `redzone_alloc`, `sampler` and the new
  `crash_handler_win.cpp`. Their headers stay in `src/diag`. Both build files glob the directory on
  Windows only.

## B11 verdicts

All perf objects (-O2) were compared against the main checkout's `runs/b11base_d4b/perf/obj`. Moved
objects were compared against their base objects copied under the new names. Every DIFF function was
re-checked over its FULL call list: b11pair prints only the first 40 calls. The scratch tools that
did this are `fullcalls.py`, `calldiff.py` and `posdiff.sh`; none is committed.

| commit | verdict |
|---|---|
| E.5 move | 10 objects RENAMED-IDENTICAL, 118 functions |
| E.5 edit | All 17 DIFFs are the import call becoming a call to the primitive, with stores and compares unchanged. Three by-products, described below the table. Every FileSystem.h / FileStream.h includer is IDENTICAL, so `kInvalidFileHandle` folds to the same immediate. |
| E.6 move | 3 objects RENAMED-IDENTICAL, 36 functions |
| E.6 edit | All 6 DIFFs are the accepted shape: FindFilesImpl, PlatformExists, GetRootDirectory, SetupDirectories, ExpandRelativePath, Modification::SetupFileSystem |
| E.7 | With the map `RtEngineLifecyclePlatformCo{Init,Uninit} -> Platform::Com{Init,Uninit}`, engine_lifecycle is RENAMED-IDENTICAL, and the two wrapper bodies are byte-identical in os_win.o. Two DIFFs, described below the table. gameupdateloop_load is IDENTICAL: the perf build strips printf arguments. |
| E.8 move | 4 objects RENAMED-IDENTICAL, 68 functions |
| E.8 edit | Every moved crash function is SAME, and identical once in-object addresses are normalised. main.o's WinMain changes only by the substitutions listed below the table. WatchdogThread, RZOut and RZBoth are position-only. |
| C3 | settings_win's ReadWholeFile (3 import calls) and WriteFileAtomic (8 import calls, same order) became primitive calls, with stores and compares unchanged. Utf8ToWide became Utf8ToPath with the same 92 instructions. |

The E.5 by-products:

- **PlatformCreateFile.** Three import calls became one call to FileCreateTemp. This is the accepted
  multi-call fold.
- **PlatformOpenFile.** The frame shrinks from 0x70 to 0x50, because FileOpenRead takes one argument
  where CreateFileW took seven. The locals shift, the stores into the stack temporary FileInfo are
  rescheduled, and the two exception cleanup pads swap places. The stores and calls are the same.
- **GetFileTimeStamp.** The FILETIME arrives as one u64 and is split into DateTime's two dwords.

The two E.7 DIFFs:

- **FormatPart.** Four import calls became one call to FormatLocalDateTime. This is the fold.
- **ComponentEntity::Persist.** The VirtualQuery import call became a call to IsReadablePage. The
  MEM_COMMIT compare moved into the primitive with it.

The E.8 WinMain substitutions:

- OpenCrashLog became CrashHandler_OpenLog, at both call sites.
- The SetUnhandledExceptionFilter and AddVectoredExceptionHandler import calls became one call to
  CrashHandler_Install.
- `std::set_terminate` became CrashHandler_InstallTerminate.

## Deviations from the plan and the brief

1. **Two moved files are renamed.** The brief said "with its basename", but `src/engine/io` already
   has `file_stream.cpp` and `persistence_saves.cpp`.
2. **The pure moves raise `tools/abi_audit.baseline`.** Each pure-move commit raises the src/engine
   rows and states why. The edit commit after it lowers them. The net change against the block base
   is a fall in every row.
3. **One new audit row: `wchar_t src/platform 1`.** It is the `using PathChar = wchar_t;` line that C2
   designates. `src/platform/common` is outside the audit's Windows-backend exclusion.
4. **`src/diag/win/` is excluded in `tools/abi_audit.py` section (ii).** It joins src/platform/win and
   src/render/dx11. C10 makes it the Windows-only directory.
5. **DirCreate landed in E.5, not E.6.** CreatePath lives in filesystem_createfile.cpp, which E.5
   converted.
6. **FormatLocalDateTime is in `platform_file.h`,** where plan section C puts it.
7. **The watchdog moved whole to `src/diag/win`.** The brief lists it among the Windows-only files.
   Plan D's split (a portable heartbeat and state file, Windows-only stack dump and modal detection)
   is left for the first Linux exe, per C10.
8. **C3 used four new primitives where C3 names FileOpenRead / FileMove.** Those two are retail-shaped
   and would change the settings file's behaviour:
   - FileOpenRead shares READ only. Without FILE_SHARE_DELETE, a reader makes a concurrent replace
     fail.
   - FileMove is MoveFileW, which fails when the destination exists.

   The new primitives (FileOpenReadShared, FileCreateWrite, FileFlush, FileReplace) carry the exact
   arguments settings_win.cpp used before.
9. **Extra cleanups:**
   - Two dead `dllimport` declarations in `persistence_begin_saveinfo.cpp`, unused since THREADS-1.
   - The dead `RtIO::{anon}::SameText` in filesystem_setupdirs.cpp. It was the baseline's
     `-Wunused-function` warning.
   - `persistence_saveinfo_job.cpp`'s SetFilePointer, which was already in `src/engine/io` but was
     the same E.5 seam.
   - `tools/boot_state_controls.sh`'s D1 control `mkdir`'d a stale `standalone/` directory and wrote
     into one it never created. It now creates `src/diag/win`.

## Not done, and why

- `src/platform/win/vtbatch7b_stream_run_win.cpp` keeps its direct SetFilePointer, which is the SDL
  audio seam's.
- `video_resources_win.cpp`'s RtVideoWriteFileRaw keeps its WriteFile. It is a backend file and not
  on E.5's list.
- `platform_startup_win.cpp`'s unreachable Engine::PlatformStartup keeps its CoInitializeEx. It is a
  backend file.
- `src/app/video_interface.cpp`'s GetModuleFileNameW stays. It belongs to the renderer loader in
  src/app and is not on E.6's list.
- `game/Game/force_work.cpp`'s VirtualQuery probes stay. They are a harness with a region cache and a
  different protection test, not the componententity probe.
- The other Win32-using `src/diag` files stay in `src/diag`, not on E.8's list: mouse_inject,
  modal_driver, renderdoc_integration, scene_watch, entity_comp_validator, alloc_stats,
  stress_harness, saveload_harness.
- E.9, the POSIX implementation, is wave 3. It covers fd + 1 handles, copy + unlink on EXDEV,
  NTFS-ordered case-insensitive FindFirst, and the POSIX crash handler.

## What `tools/boot_state.sh` reads, and what changed

| it reads | after this block |
|---|---|
| `standalone_boot_state.txt`: `STATE=%s pid=… uptimeSec=%u frame=%u stale=%u detail=%s` | Same line and same digits. pid is now printed with `%u` from a `uint32_t` instead of `%lu` from `unsigned long`. |
| `WATCHDOG_SRC` (the publish interval and hang budgets, derived from source) | The path changed to `src/diag/win/boot_state_watchdog.cpp` (`fcb2ae217`). `Sleep(1000);`, `kHungRunningSec = 20` and `kHungQuittingSec = 45` are untouched. The script's own sed lines still read 1000 / 20 / 45, and `bash -n` passes on both scripts. |
| the `[BootState]` lines in standalone_error.log | Same text. HUNG and QUITTING now print their frame count with `%d` from an `int32_t`, the same digits. |
| exit 3 + standalone_crash.log; exit 4 + standalone_hang.log | The code is unchanged and was moved verbatim (B11 above). |

## The coordinator's run list

Run each under the game-dir lock.

1. **The resave gate** (`tools/resave_gate/resave_gate.sh Gate`). **It matters here.** E.5 moved
   every save-path file call onto the primitives:
   - SaveJob's four writes.
   - LoadJob's seek, through FileSystem::ReadFile.
   - GetSaveInfoJob's seek.
   - PlatformCreateFile's %TEMP% file and PlatformCloseFile's delete + move.

   Expect 5 / 5 == golden `994eb184…`.
2. **The saveload route.** It loads a save, and the save list's dates and times are E.7's
   FormatLocalDateTime.
3. **`phase_d/mods_route.sh`.** It exercises `*.pkm` through FindFiles (E.6) and the settings write
   through WriteFileAtomic (C3).
4. **`worldcmp.sh`**, for the package reads: ReadPackfile and the FileSystem::ReadFile seek.
5. **`sa-run` + `tools/boot_state.sh --wait 300`.** BOOTING then RUNNING, then QUITTING on a clean
   quit. Check that the script's header prints `derived from boot_state_watchdog.cpp Sleep(1000)` and
   the 20 s / 45 s budgets, not a FALLBACK. ComInit at Engine::Startup is on this path.
6. **`sa-run-menu`.** The harness guard and ReadSettings go through the re-based ReadWholeFile before
   RuntimeInit, the allocator-free path.
7. **One deliberate crash through an existing route.** It should show exit 3, a
   `standalone_crash.log` with `=== STANDALONE CRASH ===`, a stack and `=== END CRASH ===`, and no
   standalone_hang.log. The crash handler now lives in `src/diag/win/crash_handler_win.cpp`.

## Shared files touched (other blocks touch them too)

- **`Makefile` and `CMakeLists.txt`:** the Windows-only `src/diag/win` glob, appended after the diag
  glob.
- **`tests/unit/sources.mk` and the regenerated `sources.cmake`:** `file_win.cpp`, kept beside SDL-B's
  two lines in the merge.
- **`src/app/main.cpp`:** the crash-handler blocks are cut, `crash_handler.h` is included, and three
  install calls replace the installs. SDL-B's edits merged clean.
- **`src/engine/io/*`:**
  - the 13 moved files.
  - `FileSystem.h`: the winHandle type and the platform_file.h include.
  - `persistence_saveinfo_job.cpp` and `modification_setupfilesystem.cpp`.
  - comment-only path fixes in FileStream.h, PackageFile.h, Persistence.h, PersistenceWorker.h,
    file_stream.cpp, filesystem.cpp, modification*.cpp, persistence_save*.cpp and text_codec.cpp.
- **`src/platform/common/platform_file.h`:** ABI-2's C.5 flips the PathChar line. The Win32 bodies
  already convert at the call (`Wide()`), and settings_win.cpp's paths are
  `std::basic_string<PathChar>`.
- **`tools/abi_audit.py` and `tools/abi_audit.baseline`:** the exclusion and the lowered rows. The
  baseline was rewritten from the merged tree; no row exceeds either parent.
- **`tools/boot_state.sh`, `tools/boot_state_controls.sh`, `.claude/skills/triage-run`,
  `.claude/skills/stress-test`, `perf/README.md`:** paths only.
- **Comment-only path fixes in shared headers:** Thread.h, Crc.h, MemoryAllocator.h, External.h,
  options.h.
