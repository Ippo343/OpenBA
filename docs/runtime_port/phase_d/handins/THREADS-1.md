# THREADS-1 hand-in (Phase D.4 wave 1, branch `d4-threads`, base `f0c5032d0`)

Plan: `plans/THREADS.md` steps E.1-E.4 only. E.5-E.9 are later waves. Every commit is `refactor:`.
Static gate only: the game was never run. Main (`9fded30c3`, SETTINGS-1 merged) was merged into the
branch before the final build.

## Commits

| commit | title | gate |
|---|---|---|
| `605c0e222` | refactor: thread_win.cpp -> src/engine/system/thread.cpp (pure move) | debug / perf / unit EXIT 0; unit 920 / 0; 0 new warnings; castmetric 2 |
| `a69d7411f` | refactor: Platform:: thread / sync / time primitives (E.1) | same; unit 931 / 0 (+11) |
| `4213022be` | refactor: System::Thread / CriticalSection over the primitives (E.2) | same; unit 931 / 0 |
| `490c080cd` | refactor: the 0x28 critical sections become Platform::LockStorage (E.3) | same; unit 932 / 0 (+1) |
| `7595bbf8d` | refactor: time over the Platform:: primitives (E.4) | same; unit 932 / 0 |
| `6f48b47da` | Merge main into d4-threads | same; unit 945 / 0 (920 + 12 THREADS + 13 SETTINGS) |

Warnings are compared as the brief says, with the baselines' CR line endings stripped first. The
baseline files are CRLF and a raw `comm` reports every line as new.

## B11 verdicts (perf objects against the main checkout's `runs/b11base_d4/perf/obj`)

Every object was compared with `b11pair.py` (empty map) after each of E.2, E.3 and E.4. Moved
functions were paired by hand. Every DIFF function was checked call by call: each differing call is an
import call (`call *__imp_X`) that became the matching `Platform::` call, in the same order.

**The accepted shape.** The Win32 call became a call or tail jump to an out-of-line primitive. An
`always_inline` body would need the Win32 declarations in the common header, which C1 forbids. Three
by-products follow from that substitution and nothing else:

- Import pointers are no longer cached in callee-saved registers, so there are fewer push / pop pairs.
  Seen in ThreadFunc and SleepThread.
- EventWaitAny takes 3 arguments, not 4, because bWaitAll=FALSE sits inside the primitive. The timeout
  moves from R9 to R8.
- A two-call Win32 pair became one primitive call:
  - InitializeCriticalSection + SetCriticalSectionSpinCount became MutexInit. Seen in the CriticalSection
    ctor, the Stream ctor (8 calls to 7), LockInit and RtLaneXPlatformLockInit.
  - The frame limiter's two waitable-timer pairs became WaitTimerCreate and WaitTimerSleep. Tick went from
    25 calls to 23.

| step | object | verdict |
|---|---|---|
| move | `thread_win.o` -> `engine/system/thread.o` | IDENTICAL 10 |
| E.2 | `thread.o` | DIFF 6, SAME 3, IDENTICAL 1 (accepted shape) |
| E.2 | `saveload_update.o` UpdateList; `persistence_saves_win.o` BeginGetSaveGameInfo; `persistence_worker_win.o` EnqueueDelete / Load / Save; `audio_stream_play_win.o` Stream::Play; `engine_loader_gate.o` IsBusy | DIFF, accepted shape |
| E.3 | `res_load_lock.o` Acquire / Armed (OnceRun), InitOnce callback re-signatured; `alloc_cutover_arm.o` x2; `redzone_alloc.o` HeapCheckLocked, RedzoneSelfTest; `audio_device.o` FreeChannelPlatform; `audio_stream_win.o` ctor / dtor; `buffers_win.o` Lock / LockThis (Unlocks SAME); `vtbatch7b_stream_run_win.o` Stream::Run (11 substitutions) | DIFF, accepted shape |
| E.3 | `allocator_win.o` LockInit renamed by parameter type; Enter / Leave / Delete now name the primitives in the table. `singleton_ctors_win.o` RtLaneXPlatformLockInit renamed. `video_resources_win.o` SAME | RENAMED / SAME |
| E.3 | `allocator.o`, `singleton_ctors.o`, `video_resources.o`, every includer of the retyped headers | IDENTICAL |
| E.4 | `timer.o` 5; `mainloop_frame.o` 2; `entitylist.o`; `gameupdateloop.o` 2; `gameupdateloop_load.o`; `splash_loop_stubs.o`; `stall_debug.o` 6; `stress_harness.o` 5; `frame_limiter.o` Tick | DIFF, accepted shape (note 1) |
| E.4 | `application_win.o` SleepThread / YieldToThreads ONLY-BASE equals `application.o` ONLY-TIP; `application.o` Quit SAME | MOVED, accepted shape by disassembly; Quit is an `.rdata` offset |
| all | every other object | IDENTICAL |

Note 1: `StallDebug::Phase::~Phase` now calls `NowMs` instead of inlining it. The perf build strips
printf arguments, so the base kept only the inlined QPC call. The same calls happen at run time.

## Deviations from the plan

1. **The pure move came first.** E.1 needs the filename `src/platform/win/thread_win.cpp` (D4_RULES
   C1), so System::Thread moved out in its own commit before E.1.
2. **CriticalSection::_storage became LockStorage in E.2, not E.3.** `thread.cpp` loses `<windows.h>`
   in E.2.
3. **Primitives added beyond the plan's list:**
   - `TickCount64`, `LocalTimeNow`, `WaitTimerCreate` / `WaitTimerSleep` and `CpuRelax`. Each call site
     keeps the Win32 call it made; the plan proposed folding these into PerfCounter / NowFileTime /
     SleepUntil.
   - `OnceFlag` / `OnceRun`, for res_load_lock.
   - `ThreadSetNormalPriority`.
4. **res_load_lock keeps its CRITICAL_SECTION and InitOnce, through LockStorage + OnceRun.** The plan's
   std::recursive_mutex + call_once would change which Win32 calls run, so it is not a refactor. The
   file also documents why it avoids the CRT once machinery.
5. **MutexInit skips SetCriticalSectionSpinCount when spin == 0.** That is res_load_lock's case, which
   never set a spin count. Every retail lock passes 5000.
6. **Primitive results use Win32's numbering.** EventWait / EventWaitAny return the Win32 wait results
   (0 = WAIT_OBJECT_0, 0x102, 0xFFFFFFFF). PerfCounter / PerfFrequency keep QPC's out-parameter and
   success flag. Timer's statics stay in counter units.
7. **New include root.** `src/platform/common` is on the include path (Makefile SA_INCLUDES, CMake
   BANISHED_INCLUDES), so callers write `#include "platform_sync.h"`. SETTINGS-1 landed the identical
   lines on main.

## Not done, and why

- **Audio::Stream's own events** (+0x40 `_wakeEvent`, +0x48 `_bufferEvent`, and their SetEvent /
  WaitForMultipleObjects calls) stay `void*` + Win32. Plan A.1 assigns their consumers to the SDL audio
  seam, and E.2 does not list them. The affected files are `audio_stream_win.cpp`,
  `vtbatch7b_stream_run_win.cpp`, `audio_voice_callback_win.cpp` and `audio_device.cpp`.
- **Plan section C's file moves** are not in E.2-E.4's lists:
  - `buffers_win` and `video_resources_win` to `engine/video`.
  - `singleton_ctors_win` to `engine/engine`.
  - The `persistence_*_win` files to `engine/io`. These belong to E.5.
  - `buffers_win.cpp` and `singleton_ctors_win.cpp` have no `<windows.h>` left, so either can move with
    a pure `git mv`.
- **`src/diag/saveload_harness.cpp`** keeps GetSystemTimeAsFileTime. It is not in E.4's list.
  `Platform::NowFileTime` exists for it.
- **Diag files left for E.8:** `boot_state_watchdog`, `sampler`, `hw_watch`, `alloc_stats`, and
  `redzone_alloc`'s Sleep / Toolhelp.
- **frame_limiter's EnumDisplaySettingsW** keeps `<windows.h>` there. The refresh rate is the SDL seam's.
- **The CMake twin was not built**; the Makefile was. CMakeLists.txt only gained the glob and include
  root that main already carries.

## Open items for the coordinator (merge gate)

- **sa-run.** Every Thread (loader, persistence worker, audio stream, pathfinder) and every lock now
  goes through the primitives. Check boot, a new game and a clean quit.
- **The resave gate.** The persistence worker's Start copies and the save list (UpdateList) are covered.
- **sa-stress.** The pathfinder thread, the allocator lock (now table -> Platform::MutexLock), the frame
  limiter, and stress_harness's CSV header date (LocalTimeNow). Record the fps; never bisect.
- **worldcmp.sh.**
- The splash loop's `Sleep(14)` and SleepThread(14) only changed spelling. A boot to the menu exercises
  both.

## Shared files touched (other blocks touch them too)

- `Makefile` and `CMakeLists.txt`: the glob and include root, identical to main's.
- `tests/unit/sources.mk` and `tests/unit/sources.cmake`: keep-both with SETTINGS, already merged.
- `src/engine/engine/mainloop_frame.cpp`:
  - The QPC declaration and the `SwitchToThread` dllimport line were removed from the pump's `extern "C"`
    block.
  - The two call sites were changed: CurrentTimeTicksViaGlobals and Frame's 0x1800dd398.
  - The pump declarations are untouched. SDL W3 rebases on this (C4).
- `src/engine/engine/mainloop_run.cpp`: the dead QPC declaration was deleted.
- `src/platform/win/application_win.cpp`: SleepThread / YieldToThreads and QpcToEngineTicks moved out to
  `src/engine/application/application.cpp`, and the header comment was updated. PlatformUpdate (the
  pump), EnableCursor and LaunchProcess are untouched.
- `src/engine/audio/Stream.h`: `_lock` became `Platform::LockStorage`, and `platform_sync.h` is now
  included. Must merge before SDL Block 3 A1 (C4).
- `src/engine/video/Device.h` and `src/engine/system/MemoryAllocator.h`: the lock field type, plus the
  `RtSysMemPlatform` lock entries now take `LockStorage*`.
- `src/engine/system/Thread.h`: the handle and lock field types.
