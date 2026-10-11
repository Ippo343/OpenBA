# POSIX hand-in (Phase D.4 wave 3, branch `d4-posix`, base `0b11a0f85`)

Plans: `plans/THREADS.md` step E.9 and section F (the POSIX twins), `plans/ABI.md` step C.7 (the
Linux dry run). D4_RULES C1, C2, C7, C10, C11; section 3 answers Q3 (std + pthreads), Q7 (WSL2
Ubuntu, apt allowed), Q10 (ARM64 later).

Static gate only. The game, `sa-run` and `sa-deploy` were never run. Main was merged three times
before the final build: `a1599c13a` (AUDIO-A), `c2ad9ea93` (SDL-C), then `02a093c6b` (XWMA). Both
hosts were re-gated after each merge.

**Result.** Unit 971 / 0 under Linux GCC 13.3.0 at the block's own tip. On the final merged tree it
is 1000 / 0: main's 990 plus 10 POSIX-only tests. 880 of the 883 src/engine + src/game +
src/platform TUs compile on Linux with 0 errors and 0 Linux-only warnings. On Windows every object
that holds a function is IDENTICAL to main's.

## Commits

| commit | title | gate |
|---|---|---|
| `7acc8e0f0` | port: src/platform/posix, the POSIX twins of the THREADS / SETTINGS / RENDER / window primitives | Windows: nothing compiled there; debug / perf / unit EXIT 0, unit 971 / 0, 0 new warnings, castmetric 2, abi OK, B11 IDENTICAL |
| `cea089ce1` | build: the CMake non-WIN32 branch (unit + linux-check) and src/platform/stub | same; Windows CMake configure checked (icon.rc built, no posix / stub source) |
| `2cb8fd008` | port: the Linux dry run, unit 971 / 0 under Linux GCC (the blockers it hit, cleared) | same; B11 IDENTICAL on error.o, thread.o, external_add / load / reftable.o |
| `77e6ca107` | port: tests/unit/platform_posix_test.cpp, the POSIX twins pinned (Linux only) | same; Linux 981 / 0 |
| `ed7b6287a` | Merge branch 'main' into d4-posix (AUDIO-A) | clean auto-merge |
| `d8854cda7` | port: after the AUDIO-A merge, linux-check takes the audio TUs; engine_lifecycle's dead `<windows.h>` | Windows B11 against main's build/perf/obj: all IDENTICAL; Linux 981 / 0, 873 TUs |
| `18b8d486e` | Merge main (SDL-C) into d4-posix | one keep-both conflict, `platform_os.h` (ExitProcess beside SDL-C's `Process`) |
| `88696beb9` | port: after the SDL-C merge, window_posix.cpp keeps only IsVisible / ShowDefault; input_device_update leaves the pending list | Windows 984 / 0, B11 against main's SDL-C gate build: all IDENTICAL; Linux 994 / 0, 879 TUs |
| `b06bcb33a` | docs: POSIX hand-in (first version) | none |
| `25f105237` | Merge main (XWMA) into d4-posix | one keep-both conflict, CMakeLists.txt's `unit` link block (POSIX's flags and platform split, XWMA's `wmadec`) |
| `498772a33` | build: linux-check links wmadec for its include directory (XWMA's xwma_decode.cpp) | Windows 990 / 0, B11 against main's XWMA gate build: all IDENTICAL; Linux 1000 / 0, 880 TUs |
| (this update) | docs: POSIX hand-in, after the XWMA merge | none |

The first merge commit carries git's default message without the attribution lines (amend is
forbidden).

Gate details, both hosts, on the final tree:

| check | Windows (MinGW, `-j2`) | Linux (GCC 13.3.0, WSL2, `-j8`) |
|---|---|---|
| build | standalone-debug, standalone-perf, unit: EXIT 0 | `-t unit`, `-t linux-check`: EXIT 0 (clean build) |
| unit | 990 / 0 | 1000 / 0 |
| warnings | 0 new vs main's `warn_baseline_{debug,perf}.txt` | 0 that Windows does not also emit |
| castmetric (`src`) | 2 | n/a |
| abi_audit | OK, baseline ratcheted DOWN only (below) | n/a |
| B11 | all IDENTICAL, 0 SAME / DIFF / ONLY | n/a |

B11 was measured three times:

- before the merges, against `runs/b11base_d4c/perf/obj`;
- after AUDIO-A, against main's own `build/perf/obj`, built by the AUDIO-A merge gate;
- after SDL-C, against main's `build/perf/obj` built at 23:07:59, after `c2ad9ea93`;
- after XWMA, against main's `build/perf/obj` built at 23:16:27, after `02a093c6b`.

Four objects get an empty verdict: `debug.o`, `ui_operator_delete.o`, `tool_base_edit2.o` and
SDL-C's new `input_dinput_win.o`. Each has an empty `.text`, and they differ in debug-info offsets
only. The first three show the same way in `runs/STEP{2,3,4}_*/b11_mapped.txt`.

abi_audit baseline, ratcheted down only:

| row | from | to |
|---|---|---|
| callconv src/engine | 3 | 1 |
| callconv tests | 1 | 0 |
| win_include src/engine (after AUDIO-A, SDL-C) | 3 | 0 |
| win32_typedef src/engine (after SDL-C) | 5 | 0 |

## The Linux build recipe

One-time setup inside WSL2 Ubuntu 24.04 (g++ 13.3, cmake 3.28, ninja 1.11 were already there).
SDL3's configure refuses to run without X11 or Wayland headers, so these were apt-installed:

```bash
sudo apt-get install libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev \
  libxss-dev libxtst-dev libxkbcommon-dev libwayland-dev wayland-protocols libegl-dev libgles-dev \
  libdrm-dev libgbm-dev libdecor-0-dev libasound2-dev libpulse-dev libpipewire-0.3-dev \
  libdbus-1-dev libudev-dev pkg-config
```

Builds over `/mnt/c` are slow, so the tree is copied in. The Windows worktree stays the only place
anything is edited, and the copy is one-way (Windows to WSL) before every build:

```bash
rsync -a --delete --exclude=/build/ --exclude=/.git --exclude=/decomp/ --exclude=/images/ \
  --exclude=/perf/ --exclude=/docs/ /mnt/c/<repo>/.claude/worktrees/d4-posix/ ~/d4-posix/
cd ~/d4-posix
cmake -S . -B build/linux-check -G Ninja -DBANISHED_FLAVOUR=debug
cmake --build build/linux-check -t unit -j8 && ./build/linux-check/unit     # 1000 tests, 0 failures
cmake --build build/linux-check -t linux-check -j8                          # 880 TUs, 0 errors
```

Flags on Linux: `-std=gnu++17 -fexceptions -fno-strict-aliasing -fno-lifetime-dse -fsigned-char
-mms-bitfields -Wall -Wextra -Wno-cast-function-type -Wno-invalid-offsetof`, plus the flavour's.
`unit` links glibc dynamically (getpwuid_r needs NSS at run time) and pulls in Threads::Threads. It
never links SDL (C7); it links XWMA's `wmadec` (C, built by its own CMakeLists). `linux-check`
links SDL3 and `wmadec` for their include directories; the wayland-scanner "validity error" lines
in that log are SDL's XML validation noise, not errors.

## What landed

- **`src/platform/posix/`**, one twin per `src/platform/win/<seam>_win.cpp`:
  - `sync_posix.cpp`: `LockStorage` holds a recursive `pthread_mutex_t` built in place, with
    `static_assert(sizeof(pthread_mutex_t) <= 0x28)` (40 bytes on Linux x86-64). The spin count is
    ignored. `OnceRun` is a three-state word, and events forward to `common/event_std.cpp`.
  - `thread_posix.cpp`: pthread plus a start latch for CREATE_SUSPENDED. The stack is
    max(arg, 2 MB) (F.3). `ResumeThread` returns the previous count. Thread ids are sequential and
    never 0 or 0xFFFFFFFF.
  - `time_posix.cpp`: a 10 MHz counter from `CLOCK_MONOTONIC` (F.4), `CLOCK_BOOTTIME` for
    TickCount64, and `clock_nanosleep(TIMER_ABSTIME)` for the waitable timer.
  - `file_posix.cpp`: the handle is fd + 1. Paths go UTF-16 to UTF-8 and `\` to `/` at the syscall
    (`posix_path.h`). FileMove refuses an existing destination and copies then unlinks on EXDEV
    (F.1). FindFirst returns NTFS upcase order with `.` and `..` first, matched case-insensitively
    (F.2). Temp files use mkstemp, ExecutablePath reads `/proc/self/exe`, and FormatLocalDateTime
    uses `localtime_r` with strftime `%x` / `%H:%M`.
  - `os_posix.cpp`: COM is a no-op. `IsReadablePage` reads `/proc/self/maps`, so a PROT_NONE
    reserved page reads as unreadable. `Platform::ExitProcess` is `_exit` after a flush.
  - `settings_posix.cpp`: `DocumentsDir` is `getpwuid_r(getuid())->pw_dir + "/.local/share"`, with
    no env var. The file layer is allocator-free over open / fsync / rename. `ReadLegacySettings`
    returns false.
  - `allocator_posix.cpp`: mmap PROT_NONE + MAP_NORESERVE to reserve, mprotect to commit,
    madvise(DONTNEED) + PROT_NONE to decommit, munmap to release. A fixed addr-to-size table
    supplies munmap's size, which the seam's `release(addr)` does not carry.
  - `gfx_posix.cpp`: `dlopen` of `libdxvk_d3d11.so.0` / `libdxvk_dxgi.so.0`, then the unversioned
    names.
  - `window_posix.cpp`: what `window_win.cpp` still does with Win32 (IsVisible, ShowDefault), over
    SDL3. The cursor and ClipCursor are SDL-C's in `src/platform/sdl/`, so the POSIX copies written
    before SDL-C landed were deleted after the merge.
- **`src/platform/stub/seam_stubs.cpp`** (never compiled on Windows) holds one PORT GAP:
  `_purecall`, the MSVC CRT's pure-virtual handler named by `CompanionData.h`. It prints and aborts
  until a POSIX crash handler exists.
- **CMakeLists.txt, non-WIN32 branch:**
  - The `RC` language is enabled only on Windows.
  - `PLATFORM_SRCS` = posix + common + sdl + stub, and the unit's `_win.cpp` entries are swapped for
    their posix twins with `list(TRANSFORM)`.
  - `linux-check` is an OBJECT library over src/engine + src/game + src/platform minus
    `BANISHED_LINUX_PENDING`.
  - The Windows configuration is unchanged.
- **`tests/unit/platform_posix_test.cpp`** has 10 tests and is empty on Windows. It covers EXDEV, a
  refused existing destination, FindFirst order and case, the 2 MB stack, the 10 MHz counter,
  DocumentsDir, the path spelling, file round trips, the in-place lock and once, and the mmap seam.

### F.5, the path spelling, pinned

On POSIX, `GetRootDirectory` sees `/proc/self/exe` as `/home/...`, and `Filename` turns it into
`\home\...\`. That spelling has a leading `\` and no `:` anywhere. The primitives turn `\` back into
`/`, so it stays absolute. Three consequences:

- The drive colon at index 1, which `GetPathExtension` skips with `c > 1` and `GetPathIdentifer`
  stops at, simply does not occur.
- An expanded candidate under the data directory has no identifier, as on Windows.
- `ExpandRelativePath` itself cannot be linked into a unit test, because it needs the FileSystem
  singleton and the ExternalFactory path stack. The test pins the four Filename splits it runs and
  the PathAttributes probe it makes.

## Blockers the dry run hit, per seam

| # | seam | blocker | cleared by | Windows |
|---|---|---|---|---|
| 1 | THREADS (System::Thread) | `__stdcall` on `Thread::ThreadFunc` (Thread.h, thread.cpp, tests/unit/system_abi_stubs.cpp): not a keyword off Windows | dropped; a no-op on x64 | B11 IDENTICAL |
| 2 | OS (System::Error) | `extern "C" __declspec(dllimport) void __stdcall ExitProcess` in error.cpp | kept verbatim under `#if defined(_WIN32)`; else `Platform::ExitProcess` (os_posix.cpp) | B11 IDENTICAL |
| 3 | CRT / diag | `_purecall` (CompanionData.h's CompanionPureCall): glibc has none | src/platform/stub PORT GAP | not compiled |
| 4 | format widths | `%llu` given a `uint64_t` (`unsigned long` on Linux) in the counter dumps of external_add / load / reftable.cpp | cast like their neighbours | B11 IDENTICAL |
| 5 | unit tests (host heap) | filename_test read `_data` of a destroyed temporary (3 tests, 8 checks); array_test assumed a freed block is not reused | `CHECK_FN_STREQ` holds the Filename; the address check stays Windows-only, the capacity proves it elsewhere | unit unchanged |
| 6 | unit tests (stack UB) | graphics_gcc_sret_precedes_this_unlike_msvc: a 28-byte wild write through a 16-byte object hit `out` on Linux | AbiProbe gets 32 bytes of room | unit unchanged |
| 7 | SETTINGS (test) | settings test read back `...\Banished\settings.cfg` through std::filesystem with a raw `\` | `HostPath` in the test (no-op on Windows) | unit unchanged |
| 8 | RENDER / OS | engine_lifecycle.cpp's dead `#include <windows.h>` | removed (ABI C.4 rule) | B11 IDENTICAL |
| 9 | build | CMake `project(... RC)` has no RC compiler on Linux; SDL3 needs X11 / Wayland headers | `enable_language(RC)` under WIN32; apt packages above | configure checked |

Expected by the plan and not hit: `long long` vs `int64_t` overload misses, Win32 typedefs leaking
outside the `<windows.h>` TUs, headers that do not compile standalone. The pending list started with
eight TUs: AUDIO-A freed three (the audio TUs), SDL-C freed `input_device_update.cpp`, and
`engine_lifecycle.cpp`'s include was dead.

**Still pending: the three TUs `linux-check` leaves out** (`BANISHED_LINUX_PENDING`):

| TU | uses | owner |
|---|---|---|
| src/game/Game/game_main.cpp (game_main.h) | WinMain, HINSTANCE, FreeLibrary | APP seam (src/app on Linux) |
| src/game/Game/force_work.cpp | VirtualQuery | diag probe; `Platform::IsReadablePage` already exists |
| src/game/Component/citizen_assignworkplace.cpp | GetModuleHandleW in a log line | diag probe |

## What stays Windows-only

- src/platform/win (every `_win.cpp`), src/render/dx11 (needs the dxvk-native headers),
  src/diag/win (C10), all of src/diag and src/app for now, and the Makefile (it never sees posix/
  or stub/).
- `Platform::ExitProcess` has no Windows body: error.cpp's Windows arm keeps its own kernel32
  declaration until a later `refactor:` moves it, with its own B11 verdict.

## Open items for the first Linux game build

The 880 `linux-check` objects leave 144 names undefined once SDL3 and `wmadec` are counted as
linked. One is `atexit`, which resolves from glibc's `libc_nonshared.a`. The other 143 are listed below by the Windows object that defines each:

| owner | count | what |
|---|---|---|
| src/diag | 72 | harnesses, PerfX terrain, stress, scene_watch, stall_debug, renderdoc, autostart with the standalone dialog globals, modal / mouse drivers, ach_clock: mostly portable files not yet built on Linux |
| src/platform/win | 30 | see the list below |
| src/app | 25 | `Opt()`, BootWindow, FrameLimiter, the EngineLoader gate, `ResLoadLockAcquire / Release`, PerfX (14) |
| the three pending TUs | 8 | ForceWork (6), `CitizenComponent::SetHome`, `g_gameSettings` |
| src/diag/win | 8 | RedzoneAlloc (6), BootState_MarkFrame, the sampler tag (C10: POSIX twins only with a Linux exe) |

The 30 `src/platform/win` names, by owner:

- **SDL Block 4 (audio):** `Platform::Audio*`, 11 names, from audio_xaudio2_win.
- **THREADS:** `RtVideoDeviceLock*`, `RtVideoDeleteDeviceLock`, `RtLaneXPlatformLockInit` and
  `RtVideoWriteFileRaw`, from buffers / singleton_ctors / video_resources_win. THREADS C planned to
  move these to src/engine/video as portable code.
- **RENDER:** `Video::Device::MoveWindow / SizeWindow / GetDefaultResolution /
  InitializeInterfaces`, from device_startup / video_interfaces_win.
- **INPUT:** `Input::Device::PlatformStartup / PlatformShutdown / PlatformUpdate /
  EnableMouseClipping`.
- **Application seam:** `Application::EnableCursor`.
- **Engine platform files:** `RtEnginePlatformShiftHeld / WidePathExists` and
  `RtEngineLifecyclePlatformFreeLibrary`, from engine_platform / engine_lifecycle_win.

Beyond the symbols:

1. **dxvk-native headers** for src/render/dx11, then the renderer target on Linux (RENDER step 4).
   `gfx_posix.cpp` already resolves the two entry points by dlopen.
2. **src/app on Linux**: main, Options, the frame limiter, res_load_lock (std::recursive_mutex, per
   THREADS D) and a POSIX crash handler (`crash_handler_posix.cpp`: sigaction + backtrace, exit 3,
   standalone_crash.log). That handler takes over `_purecall` from src/platform/stub.
3. **Two copies of the settings file layer**: settings_win.cpp over the Platform primitives, and
   settings_posix.cpp over POSIX calls. Moving one body into common/ would change settings_win.o on
   Windows, so it was not done here.
4. **Notes, not defects:**
   - FormatLocalDateTime uses the C locale's `%x` (`10/10/26`, where Windows' en-US short date
     shows `10/10/2026`), because there is no `setlocale` (it reads env vars).
   - FindFirst implements `*` / `?` plus the `*.*` rule, not 8.3 short-name matching.
   - The POSIX save folder sits under `~/.local`, a dotted directory. `GetPathFilename(true)` on a
     save path still finds the `.sav` dot first, because every hit matches `*.sav`. An
     extension-less name under that folder would split at `.local`.
   - Case-sensitive file systems (THREADS F.6) remain open for loose, mod-referenced paths.
5. **ARM64** (Q10, later): `-fsigned-char` is already on the Linux flavour. The macOS
   `pthread_mutex_t` is 64 bytes, and sync_posix.cpp's static_assert stops that build.

## Shared files

- `CMakeLists.txt`: the non-WIN32 branch, `enable_language(RC)` under WIN32, and the unit's
  posix / stub substitution. The WIN32 branch is textually unchanged except for that
  `project()` / RC split. At the XWMA merge the `unit` link block was kept both ways: POSIX's
  platform split, then XWMA's `wmadec` for both hosts.
- `src/platform/common/platform_os.h`: `Platform::ExitProcess`, kept beside SDL-C's `Process` at
  the merge.
- `tools/castmetric.py`: the floor category `posix`, the twin of
  `win32`.
- `tools/abi_audit.baseline`: ratcheted down only.
- `tests/unit/sources.mk` / `sources.cmake` are untouched; the posix swap happens in CMake.
- Tests touched: `filename_test.cpp`, `array_test.cpp`, `graphics_geometry_abi_test.cpp`,
  `persistence_settings_test.cpp`, `system_abi_stubs.cpp`, and the new `platform_posix_test.cpp`.
- Engine files touched, each B11 IDENTICAL: `system/Thread.h`, `system/thread.cpp`,
  `system/error.cpp`, `object/external_{add,load,reftable}.cpp`, `engine/engine_lifecycle.cpp`.
