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
World baseline `runs/POSTBLOCKS_worldcmp`. Invariants: new game 5683 / 0 (5694 / 0 from `e9f7a978f`, row 1286; resave golden `9c18dd25f2a8` / 23,453,453 bytes since the same commit), tutorial 5773 / 12, typetable
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

**Wave 1 CLOSED 2026-10-10** (`GATE_TABLE.md` "Phase D.4"): SETTINGS-1 `9fded30c3`, THREADS-1 `1c27606dd`, ABI
`240956960`, RENDER-2 `c5f2a0f7f`, SDL-A (B0 + W1 + W2, SDL3 3.4.18) `ffc86f69c`, SETTINGS-2 `2b485f5a8`, all at 0
problems; RENDER step 1 (the DXVK drop-in) done by the coordinator; the libwinpthread import fixed (`-static`,
`938472544`). Unit 920 -> 953.

**Wave 2b CLOSED 2026-10-10:** RENDER-3 `fa4bf2c66`, SDL-B `688814044` + its focus fix `749a84508` (SDL reports focus
only for the foreground window; a subclassed WndProc forwards WM_ACTIVATE / SETFOCUS / KILLFOCUS as retail got them),
THREADS-2 `eb4107806`, ABI-2 `83d8e4d30`, all at 0 problems; unit 953 -> 971; abi_audit pinned 0; the trace oracle
below the noise floor after the focus fix. Open: the VEH exit-3 proof after the diag split has no route to force it.

**Wave 3 CLOSED 2026-10-11:** AUDIO-A `bf4d4c620` (+ row 1274: looping ambience now actually loops; the census: EVERY sound is
xWMA), SDL-C `c2ad9ea93` + its Esc fix `0f6aa2095` (a pre-existing pause-menu port gap, row 1275 filed; DirectInput gone;
mouse relative mode while the middle button is held, from the user's measured drags), XWMA `02a093c6b` (the decoder within
1 LSB of XAudio2), POSIX `2adeccca2` (Linux unit 1000 / 0 under GCC 13.3; a Linux LINK leaves 143 undefined symbols =
the first-Linux-game-build list in `handins/POSIX.md`). Unit 971 -> 990 (+10 Linux-only). Open: rows 1273 (Error::Display
on a Startup failure), 1275 (the rest of MainMenuDialog::OnOpened / OnClosed), the Tornado noise-burst row when A4 lands,
the VEH exit-3 proof, the stress p95 (deferred to the end).

**Wave 4 (started 2026-10-11, four blocks; B11 base `runs/b11base_d4d/perf` = main `a125c96bb`'s -O2 objects, 1967
objects, the SDL-C-fix gate build):**

| block | worktree / branch | plan steps | files owned |
|---|---|---|---|
| **SDL-4** | `d4-sdl4` | SDL A4 (one-shot voices over SDL audio, decode AT LOAD per Q5), A5 (the music stream: `DecodeChunk` on the Stream worker), the XAudio2 backend deleted in the same commit as A4 (one engine handle; both backends cannot link), Stream::Run's `SetFilePointer` -> the file primitive and the file to `src/engine/audio`, the Tornado noise-burst row | `src/platform/sdl/audio_sdl.cpp` (new), `src/platform/win/audio_xaudio2_win.cpp` (deleted), `src/platform/common/platform_audio.h`, `src/engine/audio/*` (SoundBuffer serialize / PlayPlatform / Stream / Device), `src/platform/win/vtbatch7b_stream_run_win.cpp` (moved), `docs/known_port_issues.md` (one row), `src/engine/audio/README.md` |
| **SDL-5** | `d4-sdl5` | SDL Block 5 D1 (delete the Win32 input backend remnants; the OS-free input files to `src/engine/input`; window IsVisible / ShowDefault over SDL, `window_win.cpp` + `window_posix.cpp` gone; `engine_platform_win.cpp` ShiftHeld over SDL + WidePathExists over the file primitive; `Application::EnableCursor`; THREADS C's three `_win` video files portable in `src/engine/video`), D2 (drop the COM init, AFTER SDL-4 merges: XAudio2 is its last consumer; `-lole32`) | `src/platform/win/{input_*,window,engine_platform,engine_lifecycle,platform_startup,application,buffers,singleton_ctors,video_resources}_win.cpp`, `src/platform/posix/window_posix.cpp`, `src/engine/input/*`, `src/engine/video/*` (the moved bodies), `src/platform/common/platform_os.h` (ComInit), `src/engine/input/README.md`, abi_audit baseline (down only) |
| **LINUX-1** | `d4-linux` | RENDER step 4 + ABI C.7's sequel: dxvk-native 3.1.1 fetched by script (sha256), the CMake non-WIN32 `OpenBA`, `src/app` on Linux (a `main` beside WinMain, the POSIX crash handler = exit 3 + `standalone_crash.log`, takes `_purecall` from the stub), the portable `src/diag` files on Linux + the 8 `src/diag/win` twins, the three pending TUs, the RENDER-owned `Video::Device` window twins under `src/platform/posix` (over SDL3; Windows B11 IDENTICAL), the `WCHAR` 4 -> 2 conversion (RENDER C.4); merges main as SDL-4 / SDL-5 land for the audio / input symbols; the gate = boot to the menu and a new game under WSL2 lavapipe, resave, `worldcmp.sh`, in a Linux game directory of its own (never `C:/GOG Games/Banished`) | `CMakeLists.txt` (non-WIN32 branch), `tools/fetch_dxvk_native.sh`, `src/app/*` (POSIX twins / splits), `src/diag/*` (POSIX twins), `src/platform/posix/*` (new twins), `src/render/dx11/*` (the WCHAR reads only), `src/game/Game/game_main.{h,cpp}` (the WinMain / HINSTANCE split only), `force_work.cpp`, `citizen_assignworkplace.cpp` (one log line) |
| **GAPS** | `d4-gaps` | rows 1273 (Error::Display on a Startup failure, retail GameMain's arm) and 1275 (MainMenuDialog Slot4 / Slot5 installed as OnOpened / OnClosed) as `behaviour:` commits; the only block that uses Ghidra | `src/game/Game/game_main.cpp` (the Startup-failure arm only), `src/game/Dialog/mainmenudialog.cpp` + its header, `docs/known_port_issues.md` (rows 1273 / 1275 -> FIXED) |

Merge order: GAPS, SDL-4, SDL-5 (D2 after SDL-4), LINUX-1 (merges main last; its Linux gate runs on the merged tree).
Then ABI C.8 (the close: abi_audit into the gate kit; STATUS numbers) and the end-of-D.4 items: the clean-rebuild
stress re-measure, the VEH proof decision, the acceptance script.

**Wave 4 CLOSED 2026-10-11:** (GAPS `420d6cea2`, SDL-5 D1 `d5c90621c`, SDL-4 `909045eb1`, SDL-5 D2 `31a37222f`, LINUX-1 `80859a14b` + LINUX-1b `d389b2e36`, ABI C.8 `c1cac6135`, every merge at 0 problems; unit 1000 / 0 on Windows, 1010 / 0 on Linux; the SDL3 audio backend plays every shipped sound from PCM decoded at load by the vendored WMAv2 decoder and streams the music through DecodeChunk on the Stream worker; `audio_xaudio2_win.cpp`, `input_dinput_win`, `window_win.cpp` DELETED, DINPUT8 gone from the imports, no COM init of ours, no `-lole32` of ours; **the first Linux game build** (CMake non-WIN32 `OpenBA` over dxvk-native 3.1.1, SDL3 shared on Linux only) boots to the menu and into a new game under WSL2 lavapipe: 5683 / 0, the seed-12345 world byte-identical to the Windows baseline, the resave byte-identical to the golden; rows 1273 / 1275 / 1279 FIXED, 1276-1278 and 1280-1285 filed; abi_audit and castmetric are gate checks; acceptance 0 off target; stress from a CLEAN rebuild on a quiet machine: update p95 2.54 ms vs 2.40 baseline (the 3.49 / 4.16 readings were incremental-build artefacts), sim ms/iter -8 %, fps 139 -- recorded, not chased). USER DECISIONS OWED: the VEH exit-3 proof (no route forces it); `System::Error::_hideDisplay` for unattended Startup-failure boots; the music loop-wrap 512-sample drop (measure on Windows or accept); the by-hand checks (audio LISTEN list in `phase_d/handins/SDL-4.md`, SDL-5 window / Shift / cursor checks, GAPS pause-menu sound pause, drag a file onto the window = no drop cursor); Linux: row 1282 (PulseAudio open stall under WSLg on a back-to-back boot), a perf / release Linux exe, device-lost / fullscreen / alt-tab on Linux. NEXT: `docs/history/endgame_b/NEXT_SESSION.md` §2.3 (the decisions owed, asked FIRST), then its §4 "After that".**

**POST-CLOSE AUDIO HUNT -- 2026-10-11, same day, with the user listening (the decisions taken):** decisions: (a) the VEH exit-3 proof CLOSED as proven by history (B-phase crashes, the Linux SIGSEGV crash log); (b) `System::Error::_hideDisplay` NOT taken: a Startup-failure box on an unattended run stays a finding; (c) the music loop wrap ACCEPTED (clean by ear, no 512-sample drop wanted); (d) the LISTEN / window / GAPS by-hand checks all PASSED by the user (menu music + wrap, no-drop cursor, new game ambience, pause-menu pause, quit). The listen then found the river silent, which opened five fixes on main, every one gated at 0 problems: `e9f7a978f` row 1286 (AmbientEmitterDescription::Serialize serialized its count INTO the array header so the on-load SetSize never allocated -- no emitter description ever bound a sound; start-town passes 1 / 3 never positioned the river / global emitters; the audio listener was the camera-to-world X AXIS, never the position: retail = PlayerController target x / y + camera translation z), `46abe31cc` the gate kit (new-game invariant 5694 / 0, resave golden `9c18dd25` 23,453,453 bytes: payload byte-identical, +11 resource-table rows = the six sounds), `3be0d99f9` ANIMAUDIO row 1287 (AnimAudioEvent::Construct 0x140087e40: every animation-triggered sound -- work sounds, animals -- was silent), `1920a1a3b` CLICK row 1289 (the END path and all 56 sound tails proven clean with unit tests + an SDL pull harness; one START defect fixed: SDL's first pull omits the resampler padding, a 0.15 ms notch 21 ms into every resampled voice; row 1276 WITHDRAWN: the tornado burst was the XWMA scan reading FastLZ-compressed packets, in-game decode is 56 / 56 short=0), `8ac08ae08` AUDIOCOMP rows 1288 / 1290 (Entity_FindAudioComponent = FindComponent<AmbientEmitterComponent>; residence AddResident / RemoveResident = the CROWD loop of an occupied house -- never the town hall; fire Ignite / Extinguish = the building crackle; tornado Tick = the loop follows the funnel; OnRemove stops every slot; fire + tornado routes clean), `ce6fba40d` SetSlot records only on a zero handle as retail 0x140042a10 (the tree's on-demand PlaySlot deviation removed). Unit 1002 / 0. By ear (user): river fades with distance, wind clear, work sounds, animals, birdsong, the house murmur and its stop on demolition. STILL OWED: the END click heard once (no our-side cause shown; listen again on the perf exe), livestock sounds (need traded animals), `debugui.cpp:271`'s audio arm, Linux row 1282 (PulseAudio open stall on a back-to-back boot under WSLg), a perf / release Linux exe, device-lost / fullscreen / alt-tab on Linux.**

**Wave 3 (started 2026-10-10, four blocks; B11 base `runs/b11base_d4c` = main 83d8e4d30's -O2 objects):**
SDL-C (W5 the cursor over SDL + I1-I4 input: DirectInput behind `Platform::Input`, then the SDL keyboard via the
lifted scancode -> DIK table, the SDL mouse with the I3 side-by-side delta log, focus via the pump; W7 LaunchProcess /
OpenURL), AUDIO-A (A1 the audio seam refactor: XAudio2 behind `Platform::Audio`, `Device.h` fields opaque,
`SetVolumePlatform`, `PlatformReleaseEngine`; A2 the SoundBuffer format census from an existing route), XWMA (A3 the
decoder only: the FFmpeg WMAv2 extract vendored under `third_party/`, `xwma_decode.cpp` with a packet-range -> S16 API
sized for DECODE-AT-LOAD per Q5, a unit fixture decoded against an XAudio2 reference; no integration), POSIX (THREADS
E.9 `src/platform/posix/*` + `allocator_posix.cpp` and ABI C.7 the Linux dry run, both under WSL2 Ubuntu 24.04 with its
g++ 13: unit 971 / 0 under Linux GCC is the pass; the agent may apt-install what the build needs). Merge order: AUDIO-A,
SDL-C, XWMA, POSIX (POSIX touches only new files + CMake's non-WIN32 branch).

**Wave 2b (started 2026-10-10, four blocks):** THREADS-2 (E.5-E.8: file I/O, directory ops, DateTime + COM, the
diag split), SDL-B (W3 SDL window + pump, W4 display / geometry, W6 message boxes; NOT W5: the cursor sites are
RENDER-3's first), ABI-2 (C.5 WideString over `char16_t` + `Platform::PathChar` flip, C.6 `_stricmp` ->
`rtcrt::StrICmp`), RENDER-3 (the 22 `devicedata.cpp` window / cursor sites behind `Platform::Window` with the Win32
implementation, pure move; SDL W5 then swaps the cursor implementation). Merge order: THREADS-2, RENDER-3, SDL-B,
ABI-2 (ABI-2's `char16_t` flip touches THREADS-2's file primitives through `PathChar`: it merges main last). The
SETTINGS C3 re-base (settings_win.cpp's file functions over THREADS' primitives, allocator-free) is THREADS-2's
last commit.

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

**Answers (the user, 2026-10-10):** Q2 import once; Q3 std + pthreads; Q4 SDL3 only, static; Q5 **the FFmpeg
WMAv2 extract, but decoding AT LOAD, chunked, not per packet at play time**: a SoundBuffer decodes to PCM when
the resource loads (the PCM replaces the xWMA payload in memory; the seek table is then a sample offset), and the
music stream decodes chunk-ahead on the Stream worker thread, never on the audio callback; Q6 fetch DXVK by
script with a sha256; Q7 WSL2 Ubuntu, an agent may apt-install the toolchain there. Q1, Q8, Q9, Q10, Q11 stand at
their recommended defaults (not objected to). SDL Block 4's plan (A3-A5) is re-shaped by the Q5 answer before it
starts.

**RESOLVED 2026-10-11 at the wave-4 close: the clean-rebuild re-measure read p95 2.54 ms (baseline 2.40; fps 139; sim ms/iter -8 %), `perf/results/20261011_022957_d4wave4_cleanrebuild`; the 3.49 / 4.16 readings were incremental-build artefacts; nothing to bisect.** **The sa-stress update p95 (2.40 -> 3.49 ms after THREADS-1, fps unchanged, sim ms/iter within the 5 % gate) is DEFERRED by the user (2026-10-10) to the END of D.4:** re-measured once from a clean rebuild on a quiet machine (it may be a compile-time / incremental-build artefact) before anything is bisected; no block touches it.

Deferred until a step measures it: SDL I3's mouse-delta source (DirectInput mickeys vs SDL `xrel`), asked
only after the side-by-side log.

## 4. Per-block recipe

1. `git log -1` in the fresh worktree: it must be the NAMED base commit (`4e36b7000`). Read `D4_RULES.md`,
   your plan, CLAUDE.md, `.claude/rules/type-headers.md`.
2. One commit per plan step. Build `-j2` debug + perf + unit in the foreground (600000 ms timeout, re-invoke
   if cut; read the EXIT CODE). 0 new warnings against the main checkout's
   `docs/runtime_port/phase_d/runs/warn_baseline_{debug,perf}.txt` (`gate.sh`'s `norm()` shape: `grep warning: |
   sed line:col away | sort -u`, then `comm -23`). Unit 920 / 0 (plus the tests you add). `castmetric.py`
   (`tools/castmetric.py`) 2, nothing new. B11 per touched TU against
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
