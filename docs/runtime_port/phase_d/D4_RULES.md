# Phase D.4 -- the portable platform layer: rule sheet (written 2026-10-10, start of the step)

Phase D.1-D.3 (the restructure) and CMake are DONE (`GATE_TABLE.md`). D.4 is `NEXT_SESSION.md` §2.2 /
`ENDGAME.md` §E items 2-5: settings file, SDL3, threads / time / filesystem primitives, DXVK, the ABI audit.
Five read-only planning agents surveyed main `4e36b7000` and wrote one plan each under `plans/`:

| seam | plan | commits | what it found that changes the work |
|---|---|---|---|
| SETTINGS | `plans/SETTINGS.md` | 6 | one flat map, two registry functions; retail's delete pass deletes nothing, so WriteSettings MERGES; one-time read-only HKCU import |
| THREADS | `plans/THREADS.md` | 9 | no layout changes: handles stay 8-byte opaque, the 0x28 critical sections become in-place `LockStorage`; the 14 filesystem `_win` files are 90 % portable and MOVE to `src/engine/io` |
| ABI | `plans/ABI.md` | 8 | the tree is already Itanium-ABI (MinGW): 0 pinned `long`, 1 pinned `wchar_t` (WideString's unit), 0 bit-fields, the save format fixed-width; unit tests are 4 items from host-portable |
| SDL | `plans/SDL.md` | ~19 in 6 blocks | input is DirectInput 8 + a legacy GetAsyncKeyState path (not raw input); EVERY shipped sound is xWMA, which SDL3 cannot decode; the pump exists twice |
| RENDER | `plans/RENDER.md` | 4 steps | d3d11 / dxgi are LoadLibrary'd, 0 COM spellings, 0 WIC / D3DCompile: the DXVK DLL drop-in is a zero-code smoke test; the real seam is 22 window / cursor sites in `devicedata.cpp` |

Machine facts measured at the start: RTX 5080, Vulkan 1.4 (DXVK 2.x needs 1.3); WSL2 Ubuntu present; no SDL,
no Vulkan SDK, no meson in MSYS2 (`pacman` forbidden); CMake 4.4 + ninja via pip.

Session kit: `phase_d/` (the D.1-D.3 kit: `gate.sh`, `mergegate.sh`, `stepclose.sh`, `worldcmp.sh`, the routes,
`b11pair.py`, `keepboth.py`, `RESOLVE_CACHE_GONE`). B11 base for D.4 blocks: `runs/b11base_d4/perf` = main
`4e36b7000`'s `build/perf/obj` (903 objects, cut 2026-10-10; `runs/` is gitignored, so blocks read it from the
MAIN checkout's absolute path). Warning baselines `runs/warn_baseline_{debug,perf}.txt` (path-mapped, D.3).
World baseline `runs/POSTBLOCKS_worldcmp`. Invariants: new game 5683 / 0, tutorial 5773 / 12, typetable
`965b527a`, resave 5/5 == golden `994eb184a8e88db9372b5b9cd13a9ed9`, unit 920 / 0, 0 new warnings, castmetric 2.

## 0. Scope

Every block commit is `refactor:` (Windows behaviour unchanged, B11 IDENTICAL / RENAMED-IDENTICAL on every
touched TU, verdicts in the message), `behaviour:` (a Windows-observable change: the retail call site quoted,
what the new backend does differently and why, what was RUN), or `build:` (vendoring / build rules only).
Device-lost stays DEFERRED (NEXT_SESSION §3). A retail difference a block FINDS outside its seam is a new
`known_port_issues.md` row, not a fix. Android stays recorded, not planned.

## 1. Cross-seam decisions (the coordinator's, from reading the five plans together)

C1. **One platform header convention.** All seam APIs live in `src/platform/common/platform_<seam>.h`
    (`platform_sync.h`, `platform_thread.h`, `platform_time.h`, `platform_file.h`, `platform_os.h`,
    `platform_settings.h`, `platform_window.h`, `platform_input.h`, `platform_audio.h`), namespace `Platform`,
    free functions, NO OS header and NO SDL header included by any of them. Implementations:
    `src/platform/win/<seam>_win.cpp`, `src/platform/sdl/<seam>_sdl.cpp`, `src/platform/posix/<seam>_posix.cpp`.
    SETTINGS' proposed `src/platform/settings_platform.h` and SDL's `src/platform/platform_window.h` take this
    spelling. Build globs: Makefile:32 `RUNTIME_FUNC_SRCS` and CMakeLists.txt:69 `PLATFORM_SRCS` add
    `src/platform/common/*.cpp`; `sdl/` arrives with SDL Block 0; `posix/` is globbed only off Windows.
C2. **The path character is `Platform::PathChar`**, a typedef in `platform_file.h`: `wchar_t` today, flipped to
    `char16_t` by ABI step C.5 (WideString over `char16_t`) in ONE line. THREADS' file primitives take
    `const Platform::PathChar*`, never a bare `wchar_t*`, so the ABI flip touches no primitive signature; the
    Win32 bodies cast at the call (`// CAST-OK: win32`, the `error.cpp:534` pattern). SETTINGS' four functions
    take UTF-8 `const char*` (they are new code with no WideString lifetime to keep).
C3. **SETTINGS does not wait for THREADS' file primitives.** Its `ReadWholeFile` / `WriteFileAtomic` are
    implemented directly in `settings_win.cpp` now; when THREADS step 5 lands, a one-commit `refactor:` re-bases
    them on `FileOpenRead` / `FileWrite` / `FileMove` (the coordinator assigns it to whichever block is open).
C4. **Shared-file merge order.** THREADS steps 2-3 (Thread.h, the 0x28 locks incl. `audio/Stream.h`) merge BEFORE
    SDL Block 3 A1 (Device.h fields opaque): both touch the audio headers. THREADS step 4 (time: `mainloop_frame.cpp`
    QPC, `application_win.cpp` SleepThread) merges BEFORE SDL W3 (the pump in the same two files). SETTINGS step 2
    (GetSaveDirectory into `src/engine/io/filesystem.cpp`) merges BEFORE THREADS step 5-6 (the filesystem file
    moves into the same directory). `Makefile` / `CMakeLists.txt` conflicts are keep-both, resolved by the
    coordinator at the merge.
C5. **The SDL window is created with `SDL_WINDOW_VULKAN`** from W3 on, and `Platform::Window::NativeHandle()`
    returns the HWND on Windows (SDL window property) and the `SDL_Window*` under dxvk-native. `Video::Device::
    windowHandle` (+0x590, `void*`) and `Application::_windowHandle` keep their type; `devicedata.cpp`'s swap-chain
    code does not change. RENDER step 3 (the 22 window / cursor sites) runs after SDL W1 lands the API.
C6. **xWMA decoding is a user decision (§3 Q5); until it is taken, SDL Block 4 does not start.** Blocks 0-3 and 5's
    non-audio half do not depend on it. The format census (A2) runs in Block 3 regardless.
C7. **Thread / sync / time on POSIX = C++17 std + pthreads, not SDL3** (THREADS §0 policy, confirmed by the
    coordinator pending §3 Q3): `make unit` and the resave tool must not link SDL. SDL3 is used only for what is
    SDL's: window, pump, display, input, audio device, pref path, message box.
C8. **The ABI retype gate is B11 IDENTICAL on Windows** (`long` -> `int32_t` and `wchar_t` -> `char16_t` are no-ops
    on Win64 except for mangling: `b11pair.py` with a rename map). Any DIFF in an ABI commit is a finding.
C9. **DXVK step 1 (the DLL drop-in) is the coordinator's alone**: it uses the game directory under the lock and
    must remove the DLLs on every exit path (retail shares the directory). It needs the user's download go-ahead
    (§3 Q6). Its result settles whether RenderDoc records D3D11 or Vulkan under DXVK before any Linux plan relies
    on the trace oracle.
C10. **`src/diag` stays Windows-only for the first Linux build** (ABI Q2, THREADS step 8): the Windows-only files
    move to `src/diag/win/`, globbed only on Windows. The crash / hang handlers get a POSIX twin only when a
    Linux exe exists.
C11. **No new flags, no doors, no guard retail lacks, no `getenv`** (CLAUDE.md, DIRECTION §2). DXVK's own env
    vars are DXVK's, read by DXVK, not by our code. A settings file entry is not an env flag.

## 2. Waves and blocks (at most four building agents, `-j2`; the coordinator alone runs the game)

**Wave 1 (no user decision needed; started 2026-10-10):**

| block | worktree / branch | plan steps | files owned |
|---|---|---|---|
| **ABI** | `d4-abi` | ABI C.1 (audit script), C.2 (`abi.h`), C.3 (`long` -> `int32_t`, class (b)), C.4 (dead `<windows.h>`) | `tools/abi_audit.py` + baseline, `src/engine/abi.h`, `global_types.h`, `exe_type.h:131`, the class-(b) lines in `src/engine` / `src/game` / `tests` / `src/app` / `src/diag`, the ~14 dead includes in `src/game` |
| **THREADS-1** | `d4-threads` | THREADS E.1 (primitives + Win32 impl + `event_std.cpp` + unit tests), E.2 (System::Thread), E.3 (the 0x28 locks), E.4 (time) | `src/platform/common/platform_{sync,thread,time}.h`, `event_std.cpp`, `src/platform/win/{sync,thread,time}_win.cpp`, `engine/system/Thread.h` + `thread.cpp`, `MemoryAllocator.h`, `video/Device.h`, `audio/Stream.h`, `allocator_win.cpp`, `singleton_ctors_win.cpp`, `buffers_win.cpp`, `video_resources_win.cpp`, `audio_device.cpp`, `timer.cpp`, `mainloop_frame.cpp` / `mainloop_run.cpp` (QPC lines only), `application_win.cpp` (SleepThread / YieldToThreads only), `frame_limiter`, `res_load_lock`, the game QPC probes, `tests/unit/platform_sync_test.cpp` |
| **SETTINGS-1** | `d4-settings` | SETTINGS E.1 (codec + unit tests), E.2 (`platform_settings.h`, `settings_win.cpp`, GetSaveDirectory into `src/engine/io/filesystem.cpp`, delete `filesystem_win.cpp`) | `src/engine/io/settings_file.{h,cpp}`, `tests/unit/settings_file_test.cpp`, `src/platform/common/platform_settings.h`, `src/platform/win/settings_win.cpp`, `src/engine/io/filesystem.cpp`, `src/platform/win/filesystem_win.cpp` (deleted) |

Merge order for wave 1: SETTINGS-1, THREADS-1, ABI (ABI's retypes may touch lines the other two move; it
merges main last and re-B11s its TUs).

**Wave 2 (after the §3 answers):** SETTINGS-2 (E.3-E.6: the Persistence switch, the harness guard, the launcher
entries, the Registry deletion), THREADS-2 (E.5-E.8: file I/O, directory ops, DateTime + COM, the diag split),
SDL-0/1 (vendor SDL3; window + pump + display W1-W6), ABI C.5-C.6 (WideString `char16_t`, `_stricmp`). The
coordinator runs RENDER step 1 (DXVK drop-in) and step 2 (ResolveGfxImport move, `GetMonitorInfoW` ->
`DeviceName`) between merges.

**Wave 3:** SDL-2 (input I1-I4), SDL-3 (audio seam A1-A2), RENDER step 3 (window / cursor over `Platform::Window`),
THREADS E.9 (POSIX impl, unit-only, WSL), ABI C.7 (the Linux dry run: unit 920 / 0 under Linux GCC).

**Wave 4:** SDL-4 (xWMA decoder + SDL audio, per Q5), SDL-5 (delete the Win32 backends), RENDER step 4 (first
Linux build over dxvk-native), ABI C.8 (close).

## 3. User decisions (asked 2026-10-10; the recommended default in bold; waves 2+ wait for them)

| # | question | options | plan |
|---|---|---|---|
| Q1 | launcher entries (`LauncherDisableMods` / `LauncherResetSettings`) | **one-shot, honoured every boot**; persistent; only when retail would show the dialog | SETTINGS E.5 |
| Q2 | registry migration | **import HKCU once, read-only, when `settings.cfg` is absent**; start from defaults (the gate machine loses its mod list) | SETTINGS E.3 |
| Q3 | threads / sync / time on POSIX | **std + pthreads**; SDL3 (then `make unit` and the resave tool link SDL) | THREADS E.1, E.9 |
| Q4 | SDL version | **SDL3 only, static, submodule pinned to a release-3.2.N tag**; an SDL2 layer somewhere | SDL B0 |
| Q5 | xWMA audio | **(b) a WMAv2 decoder extracted from FFmpeg (LGPL), decoding per packet at play time**; (a) libavcodec built with only wmav2; (c) transcode the shipped packages once (changes assets, breaks xWMA mods); (d) FAudio (the XAudio2 API over SDL: keeps the audio seam's shape, but its xWMA path relies on GStreamer / Media Foundation -- to verify) | SDL Block 4 |
| Q6 | DXVK download | **fetch the pinned release by script with a sha256, binaries not committed**; commit under `third_party/` | RENDER step 1 |
| Q7 | Linux test bed | **WSL2 Ubuntu (an agent may install g++ / meson there); correctness only, lavapipe or dozen**; a real Linux box with a Vulkan GPU | THREADS E.9, ABI C.7, RENDER step 4 |
| Q8 | a Vulkan arm for the trace oracle (`dump_state.py`) on Linux | **no: the Linux gate is boot + resave + `worldcmp.sh`** (DIRECTION §2 no new instruments); yes | RENDER step 4 |
| Q9 | `_stricmp` | **the ported retail body everywhere** (`rtcrt::StrICmp`, `behaviour:`); host CRT on Windows + `strcasecmp` on POSIX | ABI C.6 |
| Q10 | ARM64 (Apple Silicon, Android) | **later: recorded only**; now (scalar twins for `msvc_libm`'s SSE2, the macOS 16-byte lock, the AArch64 PMF decode) | THREADS, ABI |
| Q11 | names | **`<Documents>\Banished\settings.cfg` beside `Save\`; `LauncherDisableMods` / `LauncherResetSettings`** | SETTINGS |

Deferred until a step measures it: SDL I3's mouse-delta source (DirectInput mickeys vs SDL `xrel`), asked
only after the side-by-side log.

## 4. Per-block recipe

1. `git log -1` in the fresh worktree: it must be the NAMED base commit (`4e36b7000`). Read `D4_RULES.md`,
   your plan, CLAUDE.md, `.claude/rules/type-headers.md`.
2. One commit per plan step. Build `-j2` debug + perf + unit in the foreground (600000 ms timeout, re-invoke
   if cut; read the EXIT CODE). 0 new warnings against the main checkout's
   `docs/runtime_port/phase_d/runs/warn_baseline_{debug,perf}.txt` (`gate.sh`'s `norm()` shape: `grep warning: |
   sed line:col away | sort -u`, then `comm -23`). Unit 920 / 0 (plus the tests you add). `castmetric.py`
   (`endgame_b/b4_session/castmetric.py`) 2, nothing new. B11 per touched TU against
   `docs/runtime_port/phase_d/runs/b11base_d4/perf/obj` (`b11pair.py BASE TIP MAP tu...`; `b11cmp.py` with no
   map), verdicts in the commit message.
3. Commit title `refactor:` / `behaviour:` / `build:` + what; body: files, B11 verdicts per TU, what was RUN
   ("static gate only: ..."), what the plan said that you did NOT do and why. The two attribution lines.
4. Hand in: `docs/runtime_port/phase_d/handins/<BLOCK>.md` (never a root `HANDIN.md`): per commit the verdicts,
   the deviations from the plan, open items for the coordinator, shared files appended.
5. Merge main before the final build. Never `git stash`, amend, rebase, push; no sub-agents; never `taskkill
   /IM`; never run the game; never `getenv`; no null guard on a faithful deref (CLAUDE.md).

## 5. The gate (every merge, coordinator only)

`phase_d/mergegate.sh <branch> <label> "<title>" [ui] [tools]` then the GATE_TABLE row (written BEFORE the gate
runs). Settings merges add `mods_route.sh` and `sa-run-menu`; threads merges add the resave gate, sa-stress (fps
recorded, never bisected) and `worldcmp.sh`; window / input merges add the menu-driver routes; audio merges a
listen. The DXVK drop-in's gate is RENDER §D step 1.
