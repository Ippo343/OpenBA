# LINUX-1 hand-in (Phase D.4 wave 4, branch `d4-linux`, base `a125c96bb`)

Plans: `plans/RENDER.md` step 4 (C.1, C.2, C.4, C.5, section D), `plans/THREADS.md` step 8 / D,
`plans/ABI.md` C.7's sequel. D4_RULES C1, C5, C7, C10, C11; section 3 Q6, Q7, Q8.

**Result: the first Linux game build boots to the main menu and into a new game under WSL2
Ubuntu 24.04, dxvk-native 3.1.1 over lavapipe (llvmpipe), and quits cleanly.** Menu route and
new-game route exit 0 with no crash log; the new game loads `[ExtLoad] calls=5683
openFileFail=0` (the Windows invariant); the seed-12345 world is byte-identical to the Phase D
world baseline (`new.world.txt` md5 `4ff6354d`); the resave of `Gate.sav` is byte-identical to
the Windows golden (`994eb184a8e88db9372b5b9cd13a9ed9`) from a game root of the gate machine's
path length (row 1280 says why the length matters). Two of the final gate's five routes
exited 4 (HUNG): each boot started right after a two-minute route and stalled in SDL's
PulseAudio open under WSLg (row 1282, reproduced; 60 s apart it does not happen). On Windows every object this block
touched is IDENTICAL or an accepted shape (below); unit 1000 / 0 on Windows, 1010 / 0 on Linux.

Main was merged twice: `70bbdb306` (GAPS, SDL-4, SDL-5 D1 + D2) and `5fe0dacf1` (ABI C.8).
Both hosts were rebuilt and re-gated after each. The final gate ran on the second merge's tree.

## Commits

| commit | title | Windows B11 (-O2, vs `runs/b11base_d4d/perf/obj` unless said) |
|---|---|---|
| `31d50d659` | build: tools/fetch_dxvk_native.sh (sha256-pinned) | no compiled file |
| `ab7720002` | port: the three pending TUs OS-free | main.o IDENTICAL 15; game_main.o IDENTICAL 8 + RunGameMain RENAMED-DIFF (accepted: `call *__imp_FreeLibrary` x2 -> a direct call to the seam, the mangled name `HINSTANCE__*` -> `void*`); force_work.o IDENTICAL 13 + DIFF 3 (accepted: VirtualQuery and its mask compares moved into Platform::QueryMemoryRegion); citizen_assignworkplace.o IDENTICAL 8 + DIFF 1 (accepted: GetModuleHandleW -> Platform::ExecutableImageBase); os_win.o IDENTICAL 3 + 3 new primitives |
| `9adc16821` | port: src/app on Linux, the POSIX crash handler | main.o IDENTICAL 15 (the exit summaries' new static function inlines back); runtime_init.o IDENTICAL 10; video_interface.o IDENTICAL 4 |
| `e2eaffc04` | port: src/diag on Linux | alloc_stats IDENTICAL 6, renderdoc_integration IDENTICAL 6, saveload_harness IDENTICAL 11, stress_harness IDENTICAL 35 (+1 static-init pairing artefact, shown base vs base too); entity_comp_validator IDENTICAL 8 + DIFF 3 + Readable inlined, scene_watch IDENTICAL 32 + DIFF 1 + SAME 1 (accepted: VirtualQuery -> QueryMemoryRegion) |
| `d82bf83ed` | port: the Video::Device window bodies on Linux | the two moved objects against their old names: IDENTICAL 3 / IDENTICAL 1; device_startup IDENTICAL 14, device_enum_types IDENTICAL 23, video_interfaces_win IDENTICAL 2; a full-tree B11 showed only the accepted rows above |
| `1581e4c41` | refactor: RENDER C.4, DXGI's WCHAR texts as UTF-16 | devicedata.o IDENTICAL 36 + SAME 1 (EnumerateDxgiInterfaceInto: two registers swapped, same 643 instructions) |
| `4f5a01ad9` | build: the Linux reimpl-x64 target | six diag objects (dead `<windows.h>` removed) IDENTICAL; WIN32 CMake unchanged |
| `70bbdb306` | Merge main (GAPS, SDL-4, SDL-5 D1 + D2) | one conflict, src/engine/video/README.md, kept both |
| `7031be5da` | refactor: one FreeLibrary seam (SDL-5's Platform::ReleaseLibrary) | vs the branch's previous build with map ReleaseModule -> ReleaseLibrary: game_main IDENTICAL 10, video_interface IDENTICAL 4, os_win IDENTICAL 3 + ReleaseModule gone |
| `c0ac621e3` | port: the three Linux-only defects the first boots hit | allocator IDENTICAL 30, factory_registration IDENTICAL 15; event_std DIFF 2 + 2 static dtors gone (accepted: not called by the Windows game) |
| `c78bb7487` | build: linux-check retired | main.o: WinMain DIFF is SDL-5 D2's removed CoInitializeEx (main's change); the commit itself edits a comment |
| `2507c5d19` | build: tools/linux_gamedir.sh, tools/linux_run.sh | no compiled file |
| `5fe0dacf1` | Merge main (ABI C.8) | clean; its message lacks the two attribution lines (amend is forbidden) |
| (this) | docs: LINUX-1 hand-in, known_port_issues rows 1280-1285 | no compiled file |

Final full-tree B11 (the second merge's tree, map RunGameMain): TOTAL IDENTICAL 16716,
RENAMED-IDENTICAL 397, DIFF 20, RENAMED-DIFF 1, SAME 4, RENAMED-SAME 1, ONLY-BASE 18,
ONLY-TIP 8. Every non-identical object is one of this block's accepted shapes above
(force_work, citizen_assignworkplace, game_main, entity_comp_validator, scene_watch,
event_std, devicedata, os_win, the two moved device objects) or a file main changed (main.o's
WinMain, the audio objects, engine_lifecycle, engine_platform, factory_dtors, mainmenudialog,
input_win, gfx_win, window_sdl and the files the merged blocks added).

Gate per commit (all in the commit bodies): Windows `-j2` standalone-debug / standalone-perf /
unit EXIT 0, unit 990 / 0 before the first merge and 1000 / 0 after, 0 new warnings against
`runs/warn_baseline_{debug,perf}.txt`, castmetric `src` 2, abi_audit OK (its baseline ratcheted
DOWN only: src/app win32_typedef 15 -> 14, src/game win_include 4 -> 0 and win32_typedef 8 -> 0,
src/diag callconv 9 -> 1, win_include 14 -> 6, win32_typedef 8 -> 7). Linux: unit 1000 / 0 then
1010 / 0, and 0 Linux-only warnings (the Linux warning set minus the Windows one, gate.sh's
`norm()` shape). Each commit's own tree was checked on Linux from `git archive` of the staged
tree, not from the working copy. The Windows CMake configure ran at steps 7 and the end.

## The Linux recipe

One-time, inside WSL2 Ubuntu 24.04 (g++ 13.3, cmake 3.28, ninja): the POSIX hand-in's apt line
(the SDL3 X11 / Wayland / audio dev packages). Nothing else was installed: `libvulkan1`
1.3.275 and `mesa-vulkan-drivers` 25.2.8 (lavapipe) were already present; DXVK 3.1.1 runs on
that loader. `gdb` was present (used for a probe; ptrace attach is blocked by Yama, so it launches the exe).

```bash
# one-way copy of the worktree (the Windows checkout is the only place anything is edited)
rsync -a --delete --exclude=/build/ --exclude=/.git --exclude=/decomp/ --exclude=/images/ \
  --exclude=/perf/ --exclude=/docs/ --exclude=/third_party/dxvk-native/ \
  /mnt/c/<repo>/.claude/worktrees/d4-linux/ ~/d4-linux/
cd ~/d4-linux
bash <(tr -d '\r' < tools/fetch_dxvk_native.sh)          # once: third_party/dxvk-native/3.1.1
cmake -S . -B build/linux -G Ninja -DBANISHED_FLAVOUR=debug
cmake --build build/linux -t reimpl-x64 unit -j8 && build/linux/unit     # 1010 tests, 0 failures
bash <(tr -d '\r' < tools/linux_gamedir.sh) build/linux ~/banishedLx "/mnt/c/GOG Games/Banished"
bash <(tr -d '\r' < tools/linux_run.sh) menu    ~/banishedLx ~/lx_runs/menu
bash <(tr -d '\r' < tools/linux_run.sh) newgame ~/banishedLx ~/lx_runs/newgame
bash <(tr -d '\r' < tools/linux_run.sh) world   ~/banishedLx ~/lx_runs/world
bash <(tr -d '\r' < tools/linux_run.sh) resave  ~/banishedLx ~/lx_runs/resave
```

The checkout's shell scripts are CRLF on Windows (`core.autocrlf=true`, no `.gitattributes`
rule), hence the `tr -d '\r'`. The game directory holds `reimpl-x64` (copied: `/proc/self/exe`
is the game root), `libSDL3.so.0`, `libdxvk_d3d11.so.0`, `libdxvk_dxgi.so.0` and a `WinData`
link to the retail install (read-only; nothing is written there). Settings and saves live under
`~/.local/share/Banished` (`settings.cfg`, `Save/`). For the world and resave comparisons the
Linux `settings.cfg` carries the gate machine's non-video entries (its `GameLastMapSize=1`
decides the new game's map size; a fresh file says 0 = small, and the world then differs) and
`Save/Gate.sav` is a copy of the gate machine's (md5 `b197edf5`).

`linux_run.sh` sets, per route, the Windows route's flags plus DXVK's own variables
(`DXVK_WSI_DRIVER=SDL3 VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
DXVK_LOG_LEVEL=info DXVK_LOG_PATH=<gamedir> DXVK_SHADER_CACHE=0`, and `LD_LIBRARY_PATH`):
menu `BANISHED_AUTOMENU_BUTTON=6`; newgame `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600
BANISHED_PAUSEMENU_QUIT=120`; world the same plus `BANISHED_SEED=12345`; resave
`BANISHED_MENU_SCENE=0 BANISHED_AUTOLOAD=Gate BANISHED_SAVE_TEST=roundtrip BANISHED_PAUSEMENU=300
BANISHED_PAUSEMENU_QUIT=120`. It polls `standalone_boot_state.txt` (no log grep) and prints the
exit code, the crash / hang artefacts, the frame rate from the boot-state heartbeat, the
`[ExtLoad]` line, the Quit line and DXVK's device lines.

## Linux results (final exe `0821c425`, the second merge's tree)

| route | exit | Engine::Startup | frames after RUNNING | result |
|---|---|---|---|---|
| menu (Quit button) | 0, no crash log | 16 ms | 245 in 14.1 s (57.5 ms/frame) | `[ExtLoad] calls=3416 openFileFail=0`; `App::Quit` |
| newgame (autostart, pause-menu Quit) | 0, no crash log | 3.6 s (row 1282) | 1807 in 129.4 s (71.5 ms/frame) | `[ExtLoad] calls=5683 openFileFail=0` (the Windows invariant); `App::Quit` |
| world (seed 12345) | **4, HUNG in BOOTING** | 21.6 s (row 1282) | n/a | the world was still written before the watchdog's exit: `new.world.txt` md5 `4ff6354d`, IDENTICAL to `runs/POSTBLOCKS_worldcmp`. The clean run (`world2`, before the merge of ABI C.8; same sources otherwise): exit 0, 1800 frames in 97.7 s, the same md5 |
| resave, 22-character root | 0, no crash log | 6.8 s (row 1282) | 1383 in 117.4 s | resaved image md5 `994eb184` = the golden (byte-identical, 23,452,937 bytes) |
| resave, 26-character root | **4, HUNG at frame 0** | 19.3 s (row 1282) | n/a | no image. Earlier runs from this root (2 / 2): the golden but for three profession buckets in another order (row 1280) |

Across the block's runs the resave from the 22-character root matched the golden 4 / 4 (`resave3`, `resave4`, `resave6`, final). Both final hangs are row 1282: each boot started right after a two-minute route and sat in SDL's PulseAudio open until the 20 s no-frame budget ran out. A probe reproduced it (a menu boot at once after a new game: 17.9 s in Engine::Startup; after a 60 s pause: 4 ms).

The DXVK device: `Found device: llvmpipe (LLVM 20.1.2, 256 bits) (llvmpipe 25.2.8)`,
`D3D11InternalCreateDevice: Using feature level D3D_FEATURE_LEVEL_11_0` (maximum 11_1), WSI
surface `VK_KHR_xlib_surface` (SDL chose X11 under WSLg). DXVK's `err:` lines are two per
output, `getMonitorEdid not implemented on this platform` / `Failed to parse display metadata`
(no EDID under WSLg); its warnings: monitors not associated with an adapter (fallback used),
`MakeWindowAssociation: Ignoring flags`, one unknown `QueryInterface`, `CloseHandle not
implemented`, the built-in frame-rate limiter enabled. No shader-model or hash-validation line.

Frame times are the boot-state heartbeat (main-loop iterations over wall time after RUNNING,
including the load screens): menu 43.8-57.5 ms/frame (17-23 fps), new game 54-88 ms/frame
(11-18 fps) for a debug (-O0) exe at the fullscreen 2560x1440 the Linux settings default to, on
llvmpipe with 16 threads. They measure the bed, not the port (DIRECTION: no perf chasing).

## The first Linux boots: three defects, each root-caused (commit `c0ac621e3`, row 1285)

1. **SIGSEGV, EXEC of 0, just after `[GameMain] Constructing Engine...`** with `[RtAlloc] ATTEST:
   REFUSED: platform seam incomplete`. `RtSystemAllocatorArm` installed the allocator seam only
   under `#if defined(_WIN32)` (Wave 2, before any POSIX allocator); off Windows the arm refused,
   `MemoryAllocator::_singleton` stayed null and the first allocation called through it. The
   guard is gone.
2. **SIGABRT `[FactoryReg] FATAL: no exe type record in the .data$exetype_b section`.** The exe
   type records were bracketed by PE grouped sections, which an ELF linker neither sorts nor
   merges. Off Windows they go to the section `exe_type_records`, walked between the
   linker-defined `__start_` / `__stop_` symbols (weak).
3. **HUNG in QUITTING after the menu's Quit** (hang dump: the main thread in `exit()`'s
   handlers inside `pthread_cond_destroy`, two System::Thread workers parked in
   `StdEvent::WaitAny`). event_std.cpp's process-wide mutex / condition variable were
   function-local statics destroyed by `exit()` under their waiters; glibc waits for the
   waiters. Retail exits with those workers parked on Win32 events, which exit never destroys.
   The pair is immortal now.

The crash dump also gained a `Caller:` line from `[RSP]` on an EXEC-of-0 (the unwinder cannot
leave a frame at address 0).

## Deviations from the brief, and why

- **SDL3 is linked SHARED on Linux** (row 1284). dxvk-native's SDL3 WSI dlopens `libSDL3.so.0`
  and calls SDL through that handle; a static SDL in the exe would be a second SDL with no
  window. The same pinned submodule, `SDL_SHARED` forced on off Windows only.
- **`force_work.cpp` is on a new `Platform::QueryMemoryRegion`, not `IsReadablePage`.**
  `Readable` caches the REGION (base, size) a query returns, which a per-page bool cannot give,
  and it runs per citizen in the live game. The same primitive took the two diag probes'
  `VirtualQuery`.
- **`Platform::ExecutableImageBase` and `Platform::ReleaseModule`** were added to
  `platform_os.h` in step 2; after the main merge `ReleaseModule` duplicated SDL-5's
  `Platform::ReleaseLibrary` and was removed (`7031be5da`).
- **`device_startup_win.cpp` / `device_adjustdisplay_win.cpp` were MOVED to
  `src/engine/video/device_window.cpp` / `device_adjustdisplay.cpp`, not twinned.** Since SDL
  W1 / W4 they hold no Win32 at all (every call is `Platform::Window`, SDL3-backed), so a POSIX
  twin would have been a verbatim copy. The objects are IDENTICAL under their new names.
  `video_interfaces_win.cpp` got the trivial POSIX twin asked for (no `SetErrorMode`).
- **The dxvk headers are on `src/render/dx11`'s include path only** (`-isystem` per source);
  that exposed six dead `<windows.h>` in src/diag (removed, IDENTICAL).
- **`src/platform/stub` is gone**: its one PORT GAP (`_purecall`) is the crash handler's; the
  unit's own `_purecall` is `tests/unit/posix_crt_stubs.cpp` (empty on Windows).
- **`linux-check` is retired** (`c78bb7487`): the exe compiles every TU it did and links.
- **The swap-chain probes in `main.cpp` stay Windows-only** (RENDER.md C.5 allows it), which
  keeps `main.cpp` free of Win32 / DXGI headers on Linux.
- No scratch stub TU was needed: the three blocks had merged by the time the exe was ready to
  link, so every Linux run used the merged tree.

## Still Windows-only, and why

- `src/platform/win/*`, `src/diag/win/*`: the Win32 bodies; each has a POSIX twin or a portable
  move now except the instruments of row 1283.
- The Makefile: the gate's build; it never sees posix/ (unchanged).
- `main.cpp`'s DumpStartupInfo (STARTUPINFO), the swap-chain probes and FSE refocus recovery
  (DXGI exclusive fullscreen), the NvOptimus / AmdPowerXpress exports, SDL_RegisterApp.
- The redzone allocator, the sampler and hardware watchpoints (row 1283).

## Linux-only differences found (`docs/known_port_issues.md`)

| row | what | status |
|---|---|---|
| 1280 | the resave's pointer-keyed profession bucket tree follows the heap layout, which follows the game-root path's length: identical to the golden from a 22-character root, three buckets rotated from a 26-character one | OPEN, faithful (retail and the Windows exe share it) |
| 1281 | the POSIX watchdog never publishes BLOCKED_DIALOG; an SDL message box reads HUNG | OPEN, recorded |
| 1282 | a boot within about a minute of a long route's exit stalls 4-22 s in SDL's PulseAudio open (WSLg); past 20 s the watchdog exits 4. Reproduced; 60 s apart it does not happen | OPEN, the stream close at exit is the next read |
| 1283 | the Windows-only instruments are inert on Linux | OPEN, recorded |
| 1284 | SDL3 shared on Linux, static on Windows | OPEN, by necessity |
| 1285 | the three first-boot defects | FIXED `c0ac621e3` |

Notes that are not rows: glibc prints a null `%p` as `(nil)` (MinGW: zeros), so the world
comparison adds one sed; the `[Unassign]` line's "static" address on Linux is the PIE offset
+ 0x140000000; `/mnt/c` is case-insensitive, so the case-sensitivity question (THREADS F.6)
was not exercised; the Linux "(Display n)" adapter suffix is not logged anywhere a route reads.

## Open items for the coordinator

1. Row 1282: read whether our exit closes the SDL audio stream and device before `exit()` (src/platform/sdl/audio_sdl.cpp, SDL-4's file). Until then a Linux gate spaces its long routes 60 s apart.
2. Row 1280: whether the Linux gate keeps the 22-character root (as here) or its own golden.
3. Row 1279 (SDL-5's): its fix (`SDL_SetEventEnabled(SDL_EVENT_DROP_FILE / DROP_TEXT, false)`
   after `SDL_Init(VIDEO)` in `main.cpp`) lands in src/app, now LINUX-1's file; not made here
   (a `behaviour:` change outside this block's brief).
4. The hang watchdog's per-thread dump: llvmpipe's worker threads block the dump signal and
   print "no answer within 1s" (16 x 1 s added to a hang dump on lavapipe).
5. A release / perf flavour of the Linux exe was not built or run (debug only).
6. The device-lost path, fullscreen toggling and alt-tab were not exercised on Linux.

## Shared files touched

`CMakeLists.txt` (non-WIN32 branch; the WIN32 branch textually unchanged), `.gitignore`,
`src/platform/common/platform_os.h`, `src/platform/common/event_std.cpp`,
`src/engine/system/allocator.cpp`, `src/engine/video/{Device.h, README.md, device_startup.cpp,
device_enum_types.cpp}` (comments naming the moved files), `src/game/Object/{exe_type.h,
factory_registration.cpp}`, `tools/abi_audit.baseline` (down only), `docs/known_port_issues.md`
(rows 1280-1285).
