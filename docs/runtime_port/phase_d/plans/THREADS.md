# Phase D.4 plan: THREADS / SYNC / TIME / COM / FILESYSTEM seam

Survey of main @ 4e36b7000 (2026-10-10). Scope: `NEXT_SESSION.md` §2.2 bullet "Threads, events,
timer" and ENDGAME §E. Window / input / audio-device (XAudio2) / registry / save-dir query are the
SDL and SETTINGS seams; where this seam's primitives are consumed there, it is said so.

## 0. Policy (the decision this plan asks for)

1. **Windows keeps the Win32 calls, byte-for-byte; only their spelling moves.** Every Win32 call
   below becomes a call to a one-line `Platform::` primitive whose Windows body is the same call
   with the same arguments (`CRITICAL_SECTION` + spin 5000, `CreateEventW` auto/manual, QPC...).
   The gate (`sa-run`, resave, sa-stress) must not move on Windows, because nothing it runs changes.
2. **The POSIX twin is C++17 std + pthreads, not SDL3.** Reasons: (a) the engine's sync and timer
   primitives are linked by `make unit` and the resave tool, which must not need SDL; (b) the two
   semantics we need most, a MANUAL-reset event and wait-any-of-two, exist neither in std nor in
   SDL3 (SDL_Condition + SDL_Mutex is the same construction as std's), so SDL buys nothing here;
   (c) thread stack size needs `pthread_attr_setstacksize` (std::thread cannot set it; SDL3 can,
   through `SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER`, since 3.2.0:
   https://wiki.libsdl.org/SDL3/SDL_CreateThreadWithProperties; but pthreads is on every target
   incl. Android NDK and needs no dependency). SDL3 is used only for what is genuinely
   SDL's: base / pref paths (SETTINGS seam), display refresh (frame limiter), message box.
3. **Filesystem: thin primitives at the Win32-call granularity; the bodies leave `src/platform/win/`.**
   The 12 filesystem / stream `_win.cpp` files are ~90 % portable engine logic around 1-4 Win32
   calls each (`text_file_stream_win.cpp` is 1041 lines for 4 call kinds). Duplicating them as
   `_posix` twins is the per-type copy the project forbids (memory
   `feedback_templates_wrappers_over_erased_core`). The primitives take `const wchar_t*` (the
   body's existing `WideString::_data`), so every WideString construction / release stays exactly
   where known_port_issues rows 580-582 pinned it.
4. **No new flags, no doors, no behaviour change on Windows** (DIRECTION.md §2, CLAUDE.md).

## A. Inventory (call sites, not comments; addresses from each body's banner)

### A.1 Threads and events (System::Thread and its inlined copies)
| Win32 | file:line | retail | relied-on semantics | Platform:: replacement |
|---|---|---|---|---|
| `_beginthreadex(.., stackSize, ThreadFunc, this, CREATE_SUSPENDED)` + `SetThreadPriority(NORMAL)` + `ResumeThread` | platform/win/thread_win.cpp:216-226 | ctor 0x1800ca4e0 (CRT copy 0x18007fbfc) | SUSPENDED because the 3 events are created after; the stack arg is the initial COMMIT, the reserve is the PE default (GNU ld 2 MB), so every worker really has 2 MB | `ThreadCreateSuspended(fn, arg, stack)` / `ThreadResume`; POSIX: pthread, stack = max(arg, 2 MB), start latch |
| `CreateEventW(NULL,FALSE,FALSE)` x2 (hStart, hStop), `(NULL,TRUE,TRUE)` (hFinished) | thread_win.cpp:221-223 | 0x1800ca4e0 | hStart/hStop AUTO-reset; hFinished MANUAL-reset, initially SIGNALED | `EventCreate(manualReset, initial)` |
| `SetEvent(hStop)` + `WaitForSingleObject(hThread, INFINITE)` + `CloseHandle` x4 | thread_win.cpp:230-235 | dtor 0x1800ca590 | join | `EventSet`, `ThreadJoin`, `ThreadClose`, `EventClose` |
| `ResetEvent(hFinished)` + `SetEvent(hStart)` | thread_win.cpp:240; INLINED copies: platform/win/persistence_saves_win.cpp:102, persistence_worker_win.cpp:272 (IAT 0x18014b198 / 0x18014b368), engine/save/saveload_update.cpp:1429, platform/win/audio_stream_play_win.cpp:147 (0x1800a0d65 / 0x1800a0d7e) | Start 0x1800ca610 | Reset then Set | `EventReset`, `EventSet` (the call sites keep two calls: they are retail's inlined body) |
| `WaitForSingleObject(hFinished, INFINITE)` | thread_win.cpp:246; audio_stream_play_win.cpp:139 (0x1800a0d45) | WaitForFinish 0x1800ca5f0 | manual-reset: not consumed | `EventWait(e, kInfinite)` |
| `WaitForSingleObject(hFinished, 0)` | app/engine_loader_gate.cpp:45 | (port) | poll, non-consuming | `EventWait(e, 0)` |
| `WaitForMultipleObjects(2,{hStart,hStop},FALSE,INFINITE)` + `SetEvent(hFinished)` | thread_win.cpp:270-280 | ThreadFunc 0x1800ca440 | wait-ANY; returns the LOWEST signaled index; consumes only that auto-reset event; index 1 = orderly stop | `EventWaitAny(2, evs, kInfinite)` -> index |
| `CreateEventW` auto x2 (_bufferEvent, _wakeEvent), `CloseHandle` x2 | platform/win/audio_stream_win.cpp:298-317 | Stream ctor 0x1800a0ea4 / 0x1800a0eb8, dtor 0x1800a0db4 / 0x1800a0dbe | auto-reset | same API (the SDL seam rewrites Stream's XAudio2 half and uses these) |
| `WaitForMultipleObjects(2,{_bufferEvent,_wakeEvent})`, `SetEvent(_wakeEvent / bufferEvent)` | vtbatch7b_stream_run_win.cpp:344,425 (0x1800a1280 / 0x1800a1420); audio_stream_play_win.cpp:138 (0x1800a0d38); audio_voice_callback_win.cpp:218; engine/audio/audio_device.cpp:279 (Device+0xa40) | Stream::Run, Play, OnBufferEnd, StopPlatform | wait-any | same (consumer: SDL seam) |
| `WaitForSingleObject(pi.hProcess, INFINITE)` after `CreateProcessW` | platform/win/application_win.cpp:348 | 0x18009f7c3 | block on the child | stays in the Application seam (SDL owns `ShellExecuteW`; POSIX later: posix_spawn + waitpid) |
| `GetCurrentThreadId` | platform/win/buffers_win.cpp:75,124 (0x18000caa3) | Video::Device::Lock 0x18000ca90 | u32 at Device+0x88, 0xFFFFFFFF = none, equality only | `CurrentThreadId()` -> u32 (POSIX: thread_local sequential id, never 0xFFFFFFFF) |

### A.2 Locks (all `CRITICAL_SECTION`: RECURSIVE, spin 5000)
| site | file:line | retail | Platform:: |
|---|---|---|---|
| System::CriticalSection ctor / dtor / Lock / Unlock | platform/win/thread_win.cpp:97-199 | 0x18000c5c0 / 0x18000c5b0 / 0x18000c700 / 0x18000c6f0 | `MutexInit(storage, spin)`, `MutexLock / Unlock / Destroy` |
| MemoryAllocator +0x640 (`g_plat.lockInit/Enter/Leave/Delete`) | platform/win/allocator_win.cpp:57-63 | ctor inline, spin 5000 | same, through the existing `RtSysMemPlatform` table |
| Video::Device +0x60 init / lock / delete | singleton_ctors_win.cpp:57 (0x1800f7160, spin 0x1388); buffers_win.cpp:74-133; video_resources_win.cpp:28 (0x1800f72d9) | Lock / Unlock 0x18000ca90 / 0x18000cac0 | same; **recursion is relied on** (`lockRecursionCount` +0x8c) |
| Audio::Stream +0x50 | audio_stream_win.cpp:288-319 (0x1800a0e73); vtbatch7b_stream_run_win.cpp:334-435; engine/audio/audio_device.cpp:157-170 (Device+0xa50) | Stream ctor / Run, FreeChannelPlatform | same |
| res_load_lock (port addition, spin 0, `InitOnceExecuteOnce`) | app/res_load_lock.cpp:35-70 | none | no layout: `std::recursive_mutex` + `std::call_once` |
| allocator instrument seat | diag/alloc_cutover_arm.cpp:49-72; diag/redzone_alloc.cpp:630,1104 | none | `MutexLock` on `self->lock` |

### A.3 Time
| Win32 | file:line | retail | semantics | Platform:: |
|---|---|---|---|---|
| `QueryPerformanceFrequency / Counter` | engine/system/timer.cpp:205,216,266,293,304 (raw `dllimport` decls :99-100) | PlatformStartup 0x1800ca860, GetTicks 0x1800ca840, CurrentTimeTicks 0x1800ca8f0, CurrentTime 0x1800ca760 | `(now-initial)*1e6/freq` in int64 overflows after 9.2e12 ticks: 10.7 days at 10 MHz | `PerfCounter()`, `PerfFrequency()`; **POSIX reports CLOCK_MONOTONIC/100 at 10 MHz** (1 GHz overflows after 2.6 h) |
| QPC + `SwitchToThread` | engine/engine/mainloop_frame.cpp:251,272,290,490 | MainLoop::Frame, 0x1800dd398 | yield | `PerfCounter`, `YieldThread()` |
| unused QPC decl | engine/engine/mainloop_run.cpp:163 | | | delete |
| QPC spin + `SwitchToThread` | platform/win/application_win.cpp:132-159 | SleepThread 0x18009f850, YieldToThreads 0x18009f920 | busy-wait in engine ticks | body moves to `src/engine/application/` over `PerfCounter` / `YieldThread` |
| QPC stopwatches, `GetTickCount64`, `Sleep(14)` | game/Entity/entitylist.cpp:522-685; game/Game/gameupdateloop.cpp:411-460; gameupdateloop_serialize.cpp:70; splash_loop_stubs.cpp:331,371 | port probes | elapsed only | `PerfCounter / PerfFrequency`, `SleepMs` |
| QPC + high-res waitable timer + `EnumDisplaySettingsW` | app/frame_limiter.cpp:44,66,149 | port | sub-ms sleep | `SleepUntil(perfTick)` (Win: the waitable timer as now; POSIX: `clock_nanosleep(TIMER_ABSTIME)`); refresh rate -> SDL seam |

### A.4 COM
| call | file:line | retail | consumers |
|---|---|---|---|
| `CoInitializeEx(NULL, 0 = COINIT_MULTITHREADED)` | platform/win/engine_lifecycle_win.cpp:30 | Engine::Startup 0x1800dbc12 (IAT 0x18014b618) | XAudio2 `CoCreateInstance` (audio_platform_startup_win.cpp:98), `DirectInput8Create` (input_platform_startup_win.cpp:240), `ShellExecuteW` (application_win.cpp:358): all three are the SDL seam's. D3D11 / DXGI need none. |
| `CoInitializeEx(NULL, MT)` | platform/win/platform_startup_win.cpp:424 | Engine::PlatformStartup 0x1800a2100 (unreachable) | same |
| `CoUninitialize` | engine_lifecycle_win.cpp:34 (0x1800db88b); game/Game/game_main.cpp | Engine::Shutdown | |

Replacement: `Platform::ComInit() / ComUninit()` (the name SDL.md uses); Windows body unchanged; POSIX no-op. It stays
as long as the Windows build has a COM consumer. SDL.md removes all three consumers and then drops
the init in its own `behaviour:` commit; this seam only provides the wrapper.

**Cross-seam order:** SDL.md step A5 (Audio::Stream over `EventHandle` / `WaitAny` / `LockStorage`)
needs steps 1-3 below landed first.

### A.5 Filesystem (paths are `WideString::_data`: UTF-16, `\` delimiters, forced by `Filename::AdjustDelimiters`)
| Win32 | file:line | retail | semantics | Platform:: |
|---|---|---|---|---|
| `CreateFileW(GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, NORMAL)` + `SetFilePointer(0)` + `GetFileSize` | filesystem_openfile_win.cpp:265-281; filesystem_timestamp_win.cpp:126; package_file_read_win.cpp:158 | OpenFile 0x1800addb3, GetFileTimeStamp 0x1800ae2c0 | callers reject BOTH 0 and -1 (`LEA RAX,[h-1]; CMP RAX,-3`) | `FileOpenRead(w)`; POSIX handle = fd+1 (never 0, never -1) |
| `ReadFile` / `WriteFile` / `SetFilePointer(pos, NULL, FILE_BEGIN)` | file_stream_win.cpp:97,113,153; filesystem_openfile_win.cpp:296,316; text_file_stream_win.cpp:249,763,812,837,881,921; external_savetext_win.cpp:293; package_file_read_win.cpp:459; closure_batch2b_win.cpp:105,172; persistence_worker_win.cpp:600-613 | FileStream 0x1800aaba5 / 0x1800aac33, ReadFile 0x1800acf2a, TextFileStream 0x1800ba9ab.., SaveJob 0x1800b6e00 | 32-bit offsets; `got == n` tests | `FileRead / FileWrite / FileSeek32`; POSIX lseek + read keeps retail's non-atomic shape on the shared pack handle (res_load_lock serialises it) |
| `GetTempPathW` + `GetTempFileNameW(prefix NULL)` + `CreateFileW(GENERIC_WRITE, share 0, CREATE_ALWAYS)` | filesystem_createfile_win.cpp:309-319 | PlatformCreateFile 0x1800ade91.. | write into %TEMP%, exclusive | `FileCreateTemp(out wchar[260])`; POSIX `mkstemp` |
| `CloseHandle` + `DeleteFileW(dst)` + `MoveFileW(tmp, dst)` | filesystem_createfile_win.cpp:415-436 | PlatformCloseFile 0x1800ae020 | **MoveFileW copies across volumes** | `FileClose`, `FileDelete`, `FileMove`; POSIX `rename`, copy + unlink on EXDEV |
| `CreateDirectoryW` (result discarded) | filesystem_createfile_win.cpp:295 | 0x1800ae54a | | `DirCreate` |
| `DeleteFileW` | closure_batch2b_win.cpp:84 | DeleteJob 0x1800b69c0 | | `FileDelete` |
| `FindFirstFileW(dir+pattern)` / `FindNextFileW` / `FindClose`, `FILE_ATTRIBUTE_DIRECTORY`, `cFileName[0]=='.'` | filesystem_find_win.cpp:614-675 | FindFiles 0x1800ad7a0 | patterns in use: `*.pkg`, `*.pkm`, the save pattern, `*`; case-INsensitive; NTFS yields upper-case collation order | `FindFirst / FindNext / FindClose` over `{attrs, wchar name[260]}`; POSIX: opendir + case-insensitive glob + **sort like NTFS** |
| `GetFileAttributesW != INVALID` (existence) | filesystem_find_win.cpp:575; filesystem_setupdirs_win.cpp:325; external_path_win.cpp:448 (ExpandRelativePath 0x1800bd360); engine/io/modification_setupfilesystem.cpp:285 | | exists? | `PathAttributes(w)` (returns -1 like Win32) |
| `GetFileTime(lastWrite)` -> raw FILETIME | filesystem_timestamp_win.cpp:134 | 0x1800ae30c | 100 ns since 1601 UTC; 0 on any failure | `FileLastWrite(handle)` -> u64; POSIX `(st_mtim + 11644473600 s) * 1e7` |
| `GetModuleFileNameW(NULL, buf, 0x1000)` | filesystem_setupdirs_win.cpp:256 | GetRootDirectory | exe path | `ExecutablePath(buf, n)`; POSIX `/proc/self/exe`, macOS `_NSGetExecutablePath` |
| `FileTimeToLocalFileTime` + `FileTimeToSystemTime` + `GetDateFormatW(0x400, DATE_SHORTDATE)` / `GetTimeFormatW(0x400, TIME_NOSECONDS)` | engine/system/value_types.cpp:422-429 (`<windows.h>` in an engine file) | DateAsString 0x1800c56a0 / TimeAsString 0x1800c55b0 | user locale; retail formats uninitialised stack on failure | `FormatLocalDateTime(filetime, timePart, wchar[256])`; POSIX `localtime_r` + `strftime("%x" / "%H:%M")` |
| `SHGetFolderPathW(CSIDL_PERSONAL)` | filesystem_win.cpp:126 | | | **SETTINGS seam**, not here |

### A.6 Allocator pages (`allocator_win.cpp`, seam table `RtSysMemPlatform`)
`VirtualAlloc(MEM_RESERVE[|COMMIT], PAGE_READWRITE)` 0x1800c7b60, `VirtualFree(MEM_RELEASE)`
0x1800c7bb0, `GetSystemInfo().dwPageSize` 0x1800c7c40 (stored at +0x668). allocator.cpp reserves
ranges at `nullptr` and commits / decommits page-rounded prefixes (allocator.cpp:309-366,
489-495): no fixed addresses and no 64 KB allocation-granularity assumption, only `pageSize`
(16 KB on Apple Silicon is fine). POSIX twin `allocator_posix.cpp`: `mmap(PROT_NONE)` /
`mprotect(RW)` / `madvise(DONTNEED)` + `mprotect(NONE)` / `munmap`. **`release(addr)` carries no
size and `munmap` needs one**: the twin keeps a small addr -> size map, or the table grows a size
argument (both callers know it: `MemoryRange::end`, the range table's capacity).

### A.7 The CRT subset (`src/engine/crt/`)
`msvc_libm.cpp` (lifted coefficient tables), `msvc_qsort.cpp` (MSVC's permutation order),
`msvc_stricmp.cpp` (ASCII fold, pack-index order). **Nothing in it is OS-specific**, and all three
must be KEPT on every platform (glibc's qsort / strcasecmp / libm give other orders and ULPs). The
non-portable part is the ISA: `msvc_libm.cpp:17,50,53,180` use SSE2 intrinsics
(`_mm_cvttsd_si32`, `_mm_cvtsd_si32` under MXCSR round-to-nearest, `_mm_sqrt_ss`). Fine on any
x86-64 OS; ARM64 (Apple Silicon, Android) needs scalar twins with identical rounding. No
`_controlfp` / MXCSR change exists anywhere in `src/`. `diag/stress_harness.h:59` `__rdtsc` is the
same ISA class.

## B. Layout `static_assert` impact: the central question

Rule: **no layout changes.** Every OS object the engine embeds already sits in an 8-byte `void*`
slot (handles) or an opaque `alignas(8) uint8_t[0x28]` (critical sections). Both stay:

| class (header) | offset : field | today | proposal |
|---|---|---|---|
| System::Thread (engine/system/Thread.h) and its 4 subclasses: Engine::Loader (UpdateLoop.h, stack 0x40000), IO::PersistenceWorker (Persistence.h, Persistence+0x20..0x38, 0x10000), Audio::Stream (Stream.h, 0x10000), PathfinderThread (game, 0x10000) | +0x10 _hThread, +0x18 _hStart, +0x20 _hStop, +0x28 _hFinished | `void*` HANDLE | `Platform::ThreadHandle` / `Platform::EventHandle`: pointers to INCOMPLETE structs, `static_assert(sizeof == 8)`. Windows: the value IS the Win32 HANDLE (cast only inside `platform/win`). POSIX: a heap object |
| Audio::Stream | +0x40 _wakeEvent, +0x48 _bufferEvent (Device+0xa40 / +0xa48; PlatformChannel+0x18 copies it) | `void*` | `Platform::EventHandle` |
| Audio::Stream | +0x50 _lock[0x28] (Device+0xa50) | opaque CS | `Platform::LockStorage` |
| System::CriticalSection (Thread.h; PathfinderThread+0x30) | +0x00 _storage[0x28] | opaque CS | `Platform::LockStorage` |
| rt::system::MemoryAllocator (MemoryAllocator.h) | +0x640 lock[0x28], +0x668 pageSize | opaque CS | `Platform::LockStorage` |
| Video::Device (video/Device.h) | +0x60 lock[0x28], +0x88 lockOwnerThreadId u32, +0x8c lockRecursionCount | opaque CS + Win32 tid | `LockStorage` + `CurrentThreadId()` u32 |
| IO::FileInfo (io/FileSystem.h) | +0x18 winHandle (-1 = none; 0 also rejected) | `void*` HANDLE | `Platform::FileHandle` (8 bytes); POSIX fd+1 |
| System::DateTime (Value.h) | two u32 = FILETIME | value | unchanged: POSIX converts INTO FILETIME units |
| System::Timer statics | int64 freq / initial tick | QPC units | unchanged: POSIX supplies a 10 MHz counter |

`Platform::LockStorage` = `struct alignas(8) LockStorage { uint8_t bytes[0x28]; }`: the same 40
bytes, constructed IN PLACE by `MutexInit`. Fit per target, asserted in each impl file: Windows
`CRITICAL_SECTION` 40 (today's asserts in allocator_win.cpp:42 / buffers_win.cpp:65 move there);
Linux / Android (x86-64 and arm64) `pthread_mutex_t` 40, initialised `PTHREAD_MUTEX_RECURSIVE`;
macOS `pthread_mutex_t` is 64 and does NOT fit, so the macOS impl is a 16-byte
`{os_unfair_lock, owner, depth}` recursive lock. In place beats a heap pointer here because the
allocator's own lock cannot allocate from the allocator it guards. Events and threads are
heap-side on POSIX (plain `new`, never through MemoryAllocator).

## C. API shape and directory layout

```
src/platform/common/platform_sync.h    LockStorage, Mutex{Init,Lock,Unlock,Destroy}, EventHandle,
                                       Event{Create,Set,Reset,Wait,WaitAny,Close}, kInfinite
src/platform/common/platform_thread.h  ThreadHandle, ThreadCreateSuspended/Resume/Join/Close,
                                       YieldThread, SleepMs, CurrentThreadId
src/platform/common/platform_time.h    PerfCounter, PerfFrequency, SleepUntil, NowFileTime
src/platform/common/platform_file.h    FileHandle, FileOpenRead/CreateTemp/Read/Write/Seek32/Size/
                                       Close/LastWrite, FileDelete/Move, DirCreate, PathAttributes,
                                       FindFirst/Next/Close, ExecutablePath, FormatLocalDateTime
src/platform/common/platform_os.h      ComInit / ComUninit (COM)
src/platform/common/event_std.cpp      POSIX events: one process-wide std::mutex + condition_variable,
                                       {signaled, manual} per event; WaitAny scans lowest index first
src/platform/win/{sync,thread,time,file,os}_win.cpp        one-line Win32 bodies
src/platform/posix/{sync,thread,time,file,os}_posix.cpp    pthread / clock_gettime / POSIX fs
```
Headers include no OS header (PLAN §4.1 decision 1 stays true). Build: Makefile:32
(`RUNTIME_FUNC_SRCS`) and CMakeLists.txt:69 (`PLATFORM_SRCS`) add `src/platform/common/*.cpp`;
`posix/` is globbed only off Windows.

**Leaves `src/platform/win/` (becomes portable engine code calling `Platform::`):** thread_win.cpp
(-> engine/system/thread.cpp); file_stream, filesystem_createfile, filesystem_find,
filesystem_openfile, filesystem_setupdirs, filesystem_timestamp, text_file_stream, external_path,
external_savetext, package_file_read, closure_batch2b, persistence_saves, persistence_worker
(-> engine/io/, basename without `_win`); buffers_win, video_resources_win (-> engine/video/);
singleton_ctors_win (-> engine/engine/); application_win's SleepThread / YieldToThreads (the rest
of that file is SDL's). **Stays in `win/` as an impl:** allocator_win (it already IS the
`RtSysMemPlatform` impl; gains an `allocator_posix.cpp` twin), engine_lifecycle_win (FreeLibrary is
the renderer seam's), platform_startup_win. **Loses `<windows.h>`:** engine/system/value_types.cpp,
engine/save/saveload_update.cpp, engine/audio/audio_device.cpp,
engine/io/modification_setupfilesystem.cpp, engine/engine/engine_lifecycle.cpp, and the 23
`src/game/` files that include it (most for nothing, five for a QPC / GetCurrentThreadId probe:
`grep -rl "<windows.h>" src/game`).

## D. src/diag + src/app classification

| file | Win32 used | class | reason |
|---|---|---|---|
| diag/boot_state_watchdog.cpp | CreateThread, Interlocked*, GetTickCount, state-file write; Toolhelp + SuspendThread + GetThreadContext stack dump; EnumWindows / GetWindowTextW modal detect | **split**: heartbeat + state file PORTABLE (std::thread, atomics, steady_clock); stack dump + modal detect stay in `diag/win/` | `tools/boot_state.sh` (the gate's poller) reads the state file on every OS; modal detection dies with the launcher dialog (SETTINGS seam) |
| app/main.cpp crash handler (AddVectoredExceptionHandler, SetUnhandledExceptionFilter, RtlVirtualUnwind walk, exit 3) | | **contract portable, impl per OS**: extract to `diag/win/crash_handler_win.cpp`; later `crash_handler_posix.cpp` (sigaction + backtrace -> standalone_crash.log, exit 3) | exit codes 3 / 4 are the triage contract |
| app/main.cpp SetWindowsHookExW focus diag | | Windows-only (SDL seam decides) | window messages |
| diag/hw_watch.cpp | debug registers via SetThreadContext | Windows-only, `diag/win/`; POSIX stub returns -1 | DR0-3 through Win32 |
| diag/sampler.cpp | SuspendThread, GetThreadContext, RtlVirtualUnwind, waitable timer | Windows-only, `diag/win/` | use `perf` on Linux |
| diag/redzone_alloc.cpp | VirtualQuery, IsBadReadPtr, Toolhelp, CreateThread | Windows-only, `diag/win/` (its two lock uses go through `Mutex*`) | heap probing by OS |
| diag/alloc_stats.cpp | GetTickCount64, VirtualAlloc | portable later; untouched now | flag-only, no gate reads it |
| diag/alloc_cutover_arm.cpp | Enter / LeaveCriticalSection on the allocator lock | **portable** (`MutexLock`) | it is the instrument seat's lock |
| diag/stall_debug.cpp, diag/stress_harness.cpp, diag/saveload_harness.cpp | QPC, GetLocalTime, GetSystemTimeAsFileTime, __rdtsc | **portable** (`PerfCounter`, `NowFileTime`; rdtsc under `__x86_64__`) | sa-stress is a gate |
| app/frame_limiter.cpp | QPC, waitable timer, EnumDisplaySettingsW | **portable** (`SleepUntil`; refresh rate via SDL) | on in every run |
| app/res_load_lock.cpp | CRITICAL_SECTION + InitOnce | **portable** (std::recursive_mutex + call_once) | taken by ported FindOrLoad |
| app/engine_loader_gate.cpp | WaitForSingleObject(.., 0) | **portable** (`EventWait(e, 0)`) | |

Nothing is deleted outright.

## E. Steps for the implementing agent

One commit each, the gate after each. All are `refactor:` (no behaviour change on Windows).

1. **Primitives + Windows impl + unit tests.** New `src/platform/common/*.h`,
   `src/platform/win/{sync,thread,time,file,os}_win.cpp`, `event_std.cpp`; nothing moved yet.
   Makefile:32, CMakeLists.txt:69, tests/unit/sources.mk (then regenerate sources.cmake with
   `tools/restructure/sources_mk_to_cmake.py`). New `tests/unit/platform_sync_test.cpp` in the
   `UNIT_TEST` pattern of `b39_sys_thread_test.cpp`: auto vs manual reset, initially signaled,
   WaitAny returns the lowest index and consumes only it, `EventWait(e, 0)` does not consume a
   manual event, recursive MutexLock x2, create-suspended / resume / join with a 0x10000 stack,
   PerfCounter monotonic, FILETIME of a known epoch. The event tests run against BOTH the Win32 and
   the `event_std` impl (std builds on the Windows host). Verify: `make unit`, `sa-run`.
2. **System::Thread / CriticalSection over Platform.** thread_win.cpp -> engine/system/thread.cpp;
   Thread.h fields -> `ThreadHandle` / `EventHandle` (offset asserts unchanged); the four inlined
   Start copies (A.1) and engine_loader_gate. The unit stubs in tests/unit/system_abi_stubs.cpp keep
   `InitFields` / `ReleaseFields` recording. Verify: `sa-run`, resave gate (persistence worker),
   sa-stress (pathfinder thread; record the fps, never bisect).
3. **The 0x28 locks.** `LockStorage` in Thread.h, MemoryAllocator.h, video/Device.h,
   audio/Stream.h; allocator_win's four lock functions, singleton_ctors_win, buffers_win
   (+ `CurrentThreadId`), video_resources_win, audio_device.cpp, alloc_cutover_arm, redzone_alloc;
   res_load_lock. Verify: `sa-run`, sa-stress.
4. **Time.** timer.cpp (drop its dllimport decls), mainloop_frame.cpp, mainloop_run.cpp (dead
   decl), SleepThread / YieldToThreads -> engine/application, the game probes, splash `Sleep(14)`,
   stall_debug, stress_harness, frame_limiter. Verify: `sa-run`, sa-stress fps recorded.
5. **File I/O.** `FileOpenRead / Read / Write / Seek32 / Size / Close / CreateTemp / Delete / Move /
   LastWrite` into file_stream, openfile, createfile, timestamp, text_file_stream,
   external_savetext, package_file_read, closure_batch2b, persistence_worker / saves. `git mv` each
   file to `src/engine/io/` in a pure-move commit first, then the edit commit, so history follows.
   Verify: resave gate, `phase_d/worldcmp.sh` identical, the saveload route.
6. **Directory ops + paths.** FindFirst / Next / Close, PathAttributes, DirCreate, ExecutablePath
   into find, setupdirs, external_path, modification_setupfilesystem. Verify:
   `phase_d/mods_route.sh` (`*.pkm`), resave gate, `sa-run`.
7. **DateTime + COM.** value_types.cpp -> `FormatLocalDateTime`; `ComInit / ComUninit`.
   Verify: save-list dates through the saveload route, `sa-run`.
8. **Diag split.** `src/diag/win/` for the Windows-only files (both build globs), the crash handler
   out of app/main.cpp, the watchdog split. Verify: `sa-run`; one deliberate crash through an
   existing route proves exit 3 + standalone_crash.log; `tools/boot_state.sh --wait 300` still
   reads every state.
9. **POSIX impl** (`src/platform/posix/*`, `allocator_posix.cpp`, `crash_handler_posix.cpp`). It
   cannot link into a game until the SDL + DXVK seams land, so until then only the unit target
   builds it, on a Linux / WSL host (user question 2). `event_std.cpp` is already host-tested by
   step 1.

Rules to pass to the agent: no `getenv` (read `Opt()`), no null guard on a faithful deref, poll
with `tools/boot_state.sh`, never `taskkill` or `git stash`, take the game-dir lock, build at -j2.

## F. Risks and user questions

Risks, each with its planned answer above, listed so a reviewer checks them:
1. **Saves on Linux fail silently when `/tmp` is tmpfs.** `MoveFileW` moves a FILE across volumes
   ("A new file may be on a different file system or drive"; only directory moves fail:
   https://learn.microsoft.com/en-us/windows/desktop/api/winbase/nf-winbase-movefile), while
   `rename` returns EXDEV, so `FileMove` must copy + unlink on EXDEV (step 9; a unit test with two
   directories).
2. **Directory order.** `FindFirstFileW` on NTFS yields upper-case-collated order; ext4 / APFS do
   not. Packages are re-sorted (`SortPackfiles` 0x1800264a0), but the mod list (`*.pkm`) and the
   save list take FindFiles order. The POSIX FindFirst sorts NTFS-style and matches patterns
   case-insensitively.
3. **Thread stacks.** POSIX must give 2 MB, not the literal 0x10000: Windows treats that argument
   as the commit, not the reserve. A literal port overflows the pathfinder.
4. **Timer overflow.** A 1 GHz POSIX counter overflows `CurrentTimeTicks` after 2.6 hours; the
   twin must report 10 MHz.
5. **Absolute paths.** `ExpandRelativePath` (0x1800bd360) and `Filename` treat a `:` as the
   absolute / package marker and force `\`. A POSIX `/home/...` path becomes `\home\...` with no
   `:`. Step 6 must pin, with a unit test on ExpandRelativePath, how the POSIX root and save
   directories are spelled so they still read as absolute; the primitives turn `\` into `/`.
6. **Case-sensitive filesystems.** Loose-file names not produced by FindFiles (mod-referenced
   paths) may differ in case from the disk. Packed lookups are immune (msvc_stricmp index).
7. File-share modes (`FILE_SHARE_READ`, the exclusive temp) have no POSIX equivalent; harmless
   given temp + move.

User questions (only those that change the work):
1. **std + pthreads (recommended) or SDL3 for threads / sync / time?** SDL3 would make `make unit`
   and the resave tool link SDL; it changes step 1's dependency and all of step 9.
2. **Is a Linux / WSL host acceptable for compiling `src/platform/posix/` before the game links
   there?** Without one, step 9 lands untested; the alternative is to hold step 9 until the SDL +
   DXVK seams give a Linux exe.
3. **ARM64 targets (Apple Silicon, Android) now or later?** If now, the SSE2 intrinsics in
   `msvc_libm.cpp` need bit-identical scalar twins and the macOS lock joins step 3. If later
   (recommended), both are only recorded.
