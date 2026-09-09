# Wave 10, lane ALLOC — `BANISHED_RT_ALLOC=1` with Runtime-x64.dll PRESENT

**VERDICT: GO.** The port now arms with the DLL present, and it did not before this lane.
The recorded blocker was not stale in the sense of "already fixed" — it was **live, and the
document that recorded it as fixed was wrong**. WAVE9.md states the ordering deadlock was
"Done with a third seam (`RtSystemAllocatorSetLockRetirer`)". **No such symbol existed
anywhere in the tree.** The seam was specified in prose and never written.

Branch `wave10/lane-alloc`, worktree
`C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-a4dc88fea6f958618`.
All runs under `tools/game_dir_lock.sh acquire lane-ALLOC`, debug exe
`8f035b2c986848a928b954715ac828d9`, stamp `2026-09-08 22:06:19`, `Runtime-x64.dll` present
(2,762,752 bytes, 2017-09-14) throughout.

---

## 1. Every condition a DLL-PRESENT `BANISHED_RT_ALLOC=1` run must satisfy

`runtime/src/system/rt_system_port.cpp:47` reads the env var and calls
`RtSystemAllocatorRegister()` (`runtime/src/system/allocator.cpp`), which gates on, in order:

| # | Condition | Status BEFORE this lane | Status NOW |
|---|---|---|---|
| 1 | `g_platInstalled` — all ten members of the Win32 platform seam non-null (`PlatformSeamMissing()`) | ✅ satisfied; `runtime/src/system/allocator_win.cpp` landed in Wave 2 | ✅ |
| 2 | `RuntimeDllPresent()` is **true**, so the DLL-present branch is taken and not the cutover branch | ✅ (structural, via `ResolveExportRaw`; not an env var) | ✅ |
| 3 | `g_installDetours != nullptr` — the detour installer seam | ✅ `standalone/main.cpp` → `RtSystemAllocatorSetDetourInstaller(&RtAllocSwapInstall)` | ✅ |
| 4 | `FindPatchedPrologue()` returns nullptr — nobody else owns `Allocate` (13 bytes @ `0x1800c6e60`) or `Free` (12 @ `0x1800c6c00`) | ⛔ **FAILED, ALWAYS** — see §2 | ✅ retire-then-guard |
| 5 | `g_installDetours(...)` returns true — BOTH detours install | never reached | ✅ both installed |

Conditions 1–3 were already met. **Condition 4 was unsatisfiable by construction.**

## 2. The blocker: an ordering deadlock, and a doc that recorded its own fix as done

`standalone/main.cpp:1688` calls `InstallAllocatorLock()` / `InstallFreeLock()` **before**
`RuntimePortInit()` at `:1716`. That order is correct and deliberate (it closed the
silent-lock-loss trap in WAVE2_ALLOCATOR.md §7.2). Its consequence:

* By the time `RtSystemAllocatorRegister()` runs, the two prologues **are** patched by
  `allocator_lock.cpp`, which is default-on.
* `FindPatchedPrologue()` therefore names `MemoryAllocator::Allocate (0x1800c6e60)` and the
  function returns 0 **having registered nothing**.
* The step that would satisfy the guard, `AllocatorLockRetireForPort()`, lives **inside**
  `RtAllocSwapInstall` — which is only reached *after* the guard passes.

**The guard prevented us from ever reaching the thing that would satisfy it.**

WAVE9.md §"The allocator could not arm: an ORDERING DEADLOCK, found before the run" diagnoses
this correctly and then says: *"Done with a third seam (`RtSystemAllocatorSetLockRetirer`)."*
It was not done. `grep -rn "LockRetirer" --include=*.cpp --include=*.h .` over `wave10/base`
(tip `3318690`) returns **nothing**. The lane that found the deadlock wrote the prescription
into the wave doc and the prescription was read as the cure.

### The measured refusal — verbatim, and it is on **stdout**

`BANISHED_RT_ALLOC=1 BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60`,
DLL present, exe `d9fb6ab6bf0b39fd943b77ef4d3f7f37` (the unmodified `wave10/base` tree).
`C:/GOG Games/Banished/standalone_debug.log`, archived at
`docs/runtime_port/wave10_lane_alloc/before_debug.log`:

```
[RtPort] MemoryAllocator: REFUSING to arm -- MemoryAllocator::Allocate (0x1800c6e60) is already patched. standalone/allocator_lock.cpp is default-on and holds these two prologues; detouring over it would leave the ported allocator running WITHOUT the critical section that Runtime's own direct calls to Free rely on. Retire that patch first (docs/runtime_port/WAVE2_ALLOCATOR.md section 4).
```

and in `before_error.log`, no retire, no swap:

```
[AllocLock] MemoryAllocator::Allocate is now thread-safe (takes self+0x640, tramp=000001dbe5420000, wrapper=00007ff6b24a03e4)
[AllocLock] MemoryAllocator::Free is now thread-safe (takes self+0x640, tramp=000001dbe5430000, wrapper=00007ff6b24a058e)
```

**Is the refusal correct by its own logic, or stale?** By its own logic it is **correct**: a
patch really was on the prologue and the guard cannot tell ours from a foreign one. It is
nonetheless a **stale guard** in the operative sense — the hazard it names ("retire that patch
first") is one *we* know how to discharge, and the code that discharges it sits downstream of
the guard that demands it. Exactly the shape lane E4 found on the cutover path
(`"unresolved (Runtime-x64.dll not loaded?)"` — correct by its own logic, wrong for the
situation). **This is the third time in this project's history that a stated allocator blocker
turned out to be a fact about the guard rather than about the port.**

### ⚠️ The `[RtPort]` refusal is on **stdout**, and the archived Wave 9 evidence is stderr-only

`[RtPort]` goes to `standalone_debug.log`; `[AllocLock]` / `[RtAllocSwap]` / `[RtDetour]` go to
`standalone_error.log` (WAVE9.md records this split). Any check that greps one stream reports
absence in fact.

## 3. ⚠️ WAVE9.md row "E. Allocator" half (ii) rests on an UNARMED run

The row claims both halves MET, half (ii) being *"The stated pass condition, DLL PRESENT:
`BANISHED_REDZONE=watch` + `sa-stress`, run 2026-09-08
(`perf/results/20260908_040535_laneE_redzone`)"*.

That directory's `standalone_error.log` (5,175 lines) contains:

```
[AllocLock] MemoryAllocator::Allocate is now thread-safe (takes self+0x640, tramp=0000019e16a50000, wrapper=00007ff7fb0da690)
[AllocLock] MemoryAllocator::Free is now thread-safe (takes self+0x640, tramp=0000019e16d10000, wrapper=00007ff7fb0da440)
```

and **no `[AllocLock] patches RETIRED`, no `[RtAllocSwap]`, no `[RtDetour] …[port]` line.**
`grep -ri "RtAlloc" ` over the whole result directory returns zero hits. The allocator lock was
still installed for the entire run, which is only possible if the swap never installed. So
that run demonstrates **a clean heap with the port NOT owning it** — a valid control, not the
DLL-present pass condition. Given §2 this is exactly what one would predict: on that binary
the arming attempt *could not* have got past the guard.

**Half (ii) of row E should be re-recorded against §5 below.** (Its `⚠️ do not read the perf
number` caveat is correct and unaffected.)

## 4. The minimal fix (the seam WAVE9.md specified and nobody wrote)

Five files, ~115 lines, **inert unless `BANISHED_RT_ALLOC` is set** — no default flipped.

| file | change |
|---|---|
| `runtime/types/system/MemoryAllocator.h` | `RtAllocLockRetirer` / `RtAllocLockRestorer` typedefs + `RtSystemAllocatorSetLockRetirer(retire, restore)` |
| `runtime/src/system/allocator.cpp` | store the pair; **installer-check → retire → guard → detour**, with `restore` on every post-retire refusal |
| `standalone/allocator_lock.{h,cpp}` | `AllocatorLockRestoreAfterRefusal()` — re-runs `InstallAllocatorLock()` + `InstallFreeLock()` |
| `standalone/main.cpp` | `RtSystemAllocatorSetLockRetirer(&AllocatorLockRetireForPort, &AllocatorLockRestoreAfterRefusal)` beside the other two seams |

Three design points worth keeping:

1. **The guard is not weakened, it is re-ordered.** Its intent is *"refuse a FOREIGN patch"*.
   Ours is retired first, so anything surviving the retire genuinely is foreign and the
   refusal still stands — with a message that now says so.
2. **The installer-null check moved ABOVE the retire.** A build with no detour installer must
   refuse having touched nothing; retiring and *then* discovering we cannot detour would leave
   retail's unlocked bytes in place for the whole run.
3. **`restore` is load-bearing, not decoration.** Every refusal *after* the retire would
   otherwise leave the process on retail's own **unlocked** `Allocate`/`Free` — the free-list
   race `allocator_lock.cpp` exists to prevent, arrived at *by way of a safety guard*. Both
   post-retire refusal paths call it.

## 5. The runs — evidence, quoted verbatim

### 5a. ARMED, DLL present (`rt_run.sh alloc_armed`, autostart → pause-menu quit, rc=0)

`standalone_debug.log` (**stdout**) — a line no earlier binary in this tree has ever emitted:

```
[RtAlloc] ARMED (atomic swap taken, Runtime-x64.dll present) -- 27 exports registered; the DLL's own Allocate/Free now JMP to our composed stubs.
```

`standalone_error.log` (**stderr**) — the whole sequence, in order:

```
[AllocLock] MemoryAllocator::Allocate is now thread-safe (takes self+0x640, tramp=0000025fcd0b0000, wrapper=00007ff6ffd60464)
[AllocLock] MemoryAllocator::Free is now thread-safe (takes self+0x640, tramp=0000025fcd0c0000, wrapper=00007ff6ffd6060e)
[AllocLock] patches RETIRED for the ported allocator (Allocate=1 Free=1)
[RtDetour] MemoryAllocator::Allocate[port]: 00007ffca06e6e60 -> 00007ff6ffd9b5d4 (trampoline 0000025fcd0d0000, 13 bytes displaced)
[RtDetour] MemoryAllocator::Free[port]: 00007ffca06e6c00 -> 00007ff6ffd9b66d (trampoline 0000025fcd0e0000, 12 bytes displaced)
[RtAllocSwap] ATOMIC SWAP INSTALLED — every Allocate/Free in the process now runs our port under the allocator CS
[RtAllocSwap] ?Allocate@/?Free@ registrations repointed at the composed (locked) stubs
```

`standalone_crash.log` **0 bytes**. `[RtResolve] 0 misses`, `[RtResolveEarly] 0`. The run
reached gameplay (`[ResLoad] census: FindOrLoad=22590 External::Load=5686`,
`[GameSettings] Save: …`) and self-quit. Archived: `wave10_lane_alloc/after_{debug,error}.log`.

### 5b. EFFECTIVENESS CONTROL — the port really served the heap (`calls=0` ruled out)

`BANISHED_RT_ALLOC=1 BANISHED_ALLOC_STATS=1`, 30 s autostart run. `AllocStats_RecordAlloc` /
`RecordFree` are called **only** from `SwapAllocate` / `SwapFree`, and `AllocateLocked` /
`FreeLocked` are retired in this configuration, so every counted operation went through
`rtalloc::Allocate` / `rtalloc::Free`:

```
[ALLOCSTATS] live=100 MB (peak 100 MB) totalAllocated=1016 MB  allocs=901685 frees=540750 untracked_frees=39 tableFull=6454
```

**901,685 allocations and 540,750 frees executed by the ported bodies.** This is the check the
project's own standing lesson demands: an armed port with `calls=0` reads green and means
nothing.

### 5c. CORRECTNESS RUN — `BANISHED_REDZONE=watch` + stress town, ARMED

`BANISHED_REDZONE=watch BANISHED_RT_ALLOC=1 tools/stress_run.sh --no-build --debug --frames
3000` → `perf/results/20260908_220925_wave10_laneALLOC_redzone/`.

```
[REDZONE] WATCH-ONLY mode: no shim, watchdog=1ms
[REDZONE-HEAP] watchdog thread started (interval=1ms)
[AllocLock] patches RETIRED for the ported allocator (Allocate=1 Free=1)
[RtAllocSwap] ATOMIC SWAP INSTALLED — every Allocate/Free in the process now runs our port under the allocator CS
[RtResolve] 0 misses — every cached resolve in this run found its symbol.
```

3,000 sim frames, 802 citizens, 23,705 entities, clean self-quit, **zero-byte
`standalone_crash.log`, ZERO redzone violations** (`grep -ci "violation|corrupt|REDZONE-DUMP"`
= 0).

> ⚠️ **Do NOT read this run's perf numbers.** `report.txt` says `sim ms / iteration 0.19 ->
> 1.67 +766.4% REGRESSION / PLAN 5% gate FAIL`. That figure is **the `-O0` debug exe plus a
> 1 ms whole-heap polling watchdog**, not the port. A redzone run is a correctness instrument;
> its timings are not comparable to a baseline. Recording that FAIL as a perf finding is the
> documented category error, one level worse than the Wave 9 note because this run is also a
> debug build. **The 5% `sim ms/iteration` gate for the armed allocator is NOT measured by this
> lane and remains open** (see §7).

### 5d. UNARMED CONTROL — the change is inert

Same exe, `BANISHED_RT_ALLOC` unset: `grep -c "RtAlloc" standalone_debug.log` = **0**, and the
lock stays installed and un-retired:

```
[AllocLock] MemoryAllocator::Allocate is now thread-safe (takes self+0x640, tramp=000001b4a2c20000, wrapper=00007ff6ffd60464)
[AllocLock] MemoryAllocator::Free is now thread-safe (takes self+0x640, tramp=000001b4a2c30000, wrapper=00007ff6ffd6060e)
```

## 6. Stated blockers: expired vs still holding

| Blocker, as recorded | Where | Verdict |
|---|---|---|
| "`BANISHED_RT_ALLOC` appears only in comments / no arming path" | Wave 9 lane E4 brief | **EXPIRED** (E4 already corrected it) |
| "Gate step 2: the shadow has never run / the port has never executed an instruction" | WAVE2_ALLOCATOR.md §5 | **EXPIRED** — 12.75 M ops, 0 divergences (E2/E3) |
| "Gate step 3: land `AllocatorLockRetireForPort()` and the `main.cpp` reorder" | WAVE2_ALLOCATOR.md §5 | **EXPIRED for the reorder, LIVE for the wiring** — this lane |
| "Ordering deadlock — fixed with a third seam" | WAVE9.md | **NOT EXPIRED; NEVER LANDED.** Fixed here |
| "DLL-present pass condition MET (redzone stress)" | WAVE9.md row E (ii) | **NOT MET on that evidence** (§3). Met here (§5c), on a debug exe |
| "`Get` goes from never-executed to the hottest function in the game; exercise before arming" | WAVE9_LANE_E4.md §6 | **DISCHARGED BY THE RUN** — `MemAlloc`/`MemFree` call `Get` on every one of the 901,685 allocations in §5b, and `[RtResolve] 0 misses` |
| "Do NOT arm `Shutdown`/`~MemoryAllocator` into lane N's open teardown crash" | WAVE9_LANE_E4.md §6 | **STILL HOLDS.** Every run here self-quit cleanly, but that is 4 clean teardowns, not a refutation of a ~4-in-10 intermittent. Arming remains a variable in that hunt |
| "`?_singleton@MemoryAllocator` is unminted; `SingletonSlot()` caches a null at cutover" | WAVE9.md / lane E3 | **STILL HOLDS — but it is a CUTOVER blocker, not a DLL-present one.** With the DLL loaded the slot resolves and `Get()` works, as §5b proves |
| "5% `sim ms/iteration` gate, three runs per config" | PLAN.md §5.3 | **STILL OPEN** — and now blocked on §7 |

## 7. ⛔ Blocker found in passing: the perf/release exe does not build on `wave10/base`

`mingw32-make standalone-perf` fails. `standalone/log_strip.h` defines `#define printf(...)
((void)0)`, which cannot expand at a **qualified** `std::printf` call. There are **270
`std::printf` call sites across 49 files** (`runtime/src/collection/array.cpp`,
`array_destroy.cpp`, `runtime/src/audio/audio_platform_startup_win.cpp`, …).

**Pre-existing and unrelated to this lane** — none of those files is touched by it, and the
noted-as-already-broken release target is the same defect (an old commit
`965361d "Fix release build: unqualify two std::printf call sites"` shows the class recurring
one or two sites at a time). It is why §5c had to run on the `-O0` exe, and it is what blocks
the 5% perf gate for the armed allocator. **Whoever takes that gate must fix the 270 sites
first** (mechanical: `std::printf` → `printf`, or teach `log_strip.h` a qualified form).

## 8. Verdict

**GO for arming `BANISHED_RT_ALLOC=1` with Runtime-x64.dll present**, on this evidence:
it arms (§5a), the ported bodies genuinely serve the process heap at ~900 k allocations per
30 s (§5b), a redzone-watched 3,000-frame stress town reports zero violations and zero crashes
(§5c), and an unarmed run is byte-for-byte unchanged in behaviour (§5d).

Two qualifications, neither of which blocks arming:

* **Not default-on, and this lane did not propose making it so.** `BANISHED_RT_ALLOC` stays
  opt-in until the teardown question (§6) and the perf gate (§7) close.
* **The 5% perf gate is unmeasured**, blocked on §7's pre-existing build failure. GO here means
  *correct and armable*, not *perf-cleared*.
