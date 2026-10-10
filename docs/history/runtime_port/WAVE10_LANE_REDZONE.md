# WAVE 10 lane REDZONE — is `BANISHED_REDZONE=watch` a blind instrument?

**VERDICT: (c). The crash is real, and it is strictly DOWNSTREAM of the check.
Lane ALLOC's GO verdict STANDS.** The `watch` watchdog demonstrably reports
violations during the sim — measured, not assumed — and the crash UIF saw
happens at process teardown, after the last sim frame, after the quit click and
after every `atexit` census. It was fixed here anyway, and the fix is verified
by a deliberate perturbation that the instrument CATCHES.

Worktree: `.claude/worktrees/agent-a0f72b5c250d76e38` · branch `wave10/lane-redzone`
(from `wave10/base` @ `cd5ca81`).

---

## 1. The report, and what was actually true in it

Lane UIF reported:

> `BANISHED_REDZONE=watch` is BROKEN on `wave10/base` independently of my lane.
> It crashed inside the watchdog's own free-list walk … `EXEC of 00000001e39a3c70`,
> module UNKNOWN, **mid-run**.

Two of the three claims hold, one does not:

| UIF's claim | Finding |
|---|---|
| `watch` crashes on `wave10/base` with RT_DIFF/RT_ALLOC unset | **TRUE** — reproduced, §2 |
| It is the watchdog's own walk | **TRUE** — the walk is the only thing running there, and removing the instrument removes the crash (§2 control) |
| **mid-run** | **FALSE** — it is at process TEARDOWN, after the quit click, after the last CSV row, and after every `atexit` census (§2) |

That third one is the whole question. "Mid-run" would mean the instrument died
while it was supposed to be watching, and the green reading would be suspect.
"At teardown" means every walk of the measured window completed.

---

## 2. Reproduction, and the control that localizes it

Debug exe built from `wave10/base` @ `cd5ca81`, **unmodified**.
md5 `e0cd48004dcc36a44a5bd735b5098508` (build and deployed, quoted by the run
script before each launch; every launch went through
`tools/game_dir_lock.sh run lane-REDZONE`, with `verify` immediately before and
after the deploy).

**Run A — repro.** `BANISHED_REDZONE=watch`, `BANISHED_RT_DIFF` and
`BANISHED_RT_ALLOC` unset, `stress_run.sh --debug --frames 3000`
→ `perf/results/20260908_232324_wave10base_redzone_repro`.

`Segmentation fault`. But the ordering in `standalone_error.log` places it:

```
line 5090  [PauseMenu] frame 3001 - opening
line 5094  [PauseMenu] dispatching OnCommand(msg=0x12, id=6)      <- the quit click
line 5095  [PauseMenu] OnCommand returned
...
line 5167  [FNHook]    census: ...                                 <- atexit handlers
line 5168  [ResLoad]   census: FindOrLoad=23713 External::Load=5686
line 5169  [ReadGuard] census: Read=0
           <segfault — standalone_crash.log is ZERO BYTES>
```

The final CSV row is frame 3000. All 3,000 sim frames ran, the harness clicked
quit at frame 3061, and the crash lands **after the last `atexit` handler**. The
crash log is empty because the VEH handler is gone by then — which is why UIF's
`EXEC of 00000001e39a3c70 / module UNKNOWN` could not be symbolized here either;
my repro produced no crash log at all to symbolize. The address shape is
consistent with the teardown story (a bare VirtualAlloc'd/unloaded region, hence
"module UNKNOWN" — the same shape as the `[AllocLock] tramp=000001e549420000`
this run reports at line 2), but I did not confirm the faulting instruction and
do not claim to have.

**Run B — the control that says whose crash it is.**
Same exe, same config, `BANISHED_REDZONE` **unset**
→ `perf/results/20260908_232916_wave10base_NOredzone_control`.

Clean. No segfault, zero-byte crash log, same shutdown census. So the crash is
caused by the instrument, not a pre-existing teardown bug that the instrument
merely witnesses.

**Mechanism.** The watchdog was a `for(;;)` with no way to stop it
(`standalone/redzone_alloc.cpp`). After `atexit`, the CRT tears the process down
— unloading `Runtime-x64.dll`, destroying the engine allocator — while that
thread is still dereferencing `g_pMemAllocSingleton` and entering the
`CRITICAL_SECTION` at `alloc+0x640` in memory that has just gone away.

This also explains UIF's other observation for free: `BANISHED_REDZONE=1` is
clean because **the shim mode starts no watchdog at all**. `g_watchIntervalMs`
defaults to `-1`, the shim path only sets it from `BANISHED_REDZONE_WATCH`, and
the thread is created only when it is `>= 0`. See §6 — that green is the
genuinely vacuous one.

---

## 3. When did it break? It never worked.

Not by bisect — by source identity, which is stronger here:

```
git diff --stat 56ab01c wave10/base -- standalone/redzone_alloc.cpp \
    standalone/redzone_alloc.h standalone/hw_watch.cpp standalone/hw_watch.h \
    standalone/allocator_lock.cpp
(empty)
```

The instrument is **byte-identical** between lane ALLOC's tested tip
(`56ab01c`) and `wave10/base`. No commit in `56ab01c..wave10/base` touches any
of those files — not UI1, not EXTPTR, not UI4, not UIDATA, not PERFFIX. The
unterminated `for(;;)` is original.

So **possibility (a) is excluded**: no merge broke it. The shutdown race was
present, unchanged, during lane ALLOC's own run. Whether it happened to fault on
that build (heap layout differs between merges) is not knowable without
rebuilding `56ab01c`, and does not matter — see §4.

**Commits actually built and run** (not reasoned about): `wave10/base` @
`cd5ca81` unmodified (runs A, B); `cd5ca81` + this lane's fix (runs C, D, E).
I did **not** rebuild `56ab01c`, because the source-identity argument settles the
instrument question and the teardown localization settles the verdict question.

---

## 4. The discriminating mechanism, and why it is (c) not (b)

The three possibilities differ in one testable fact: **could the walk have
reported a violation during the measured window?**

Possibility (b) — the blind-instrument case — needs the walk to be incapable of
reporting. I did not infer this from the fact that the process survived the
frames; that is exactly the reasoning this lane exists to distrust. I tested the
capability directly.

**Run C — negative control, in lane ALLOC's own configuration.**
`BANISHED_REDZONE=watch` **+ `BANISHED_RT_ALLOC=1`** (DLL present, armed —
`[RtAllocSwap] ATOMIC SWAP INSTALLED`), plus a new
`BANISHED_REDZONE_SELFTEST=100000`, which clobbers **the exact qword the walk
reads**: one engine boundary tag, zeroed under the allocator's own critical
section, 100 s into the run.
→ `perf/results/20260908_233427_POSCTRL_allocarmed`

```
[REDZONE-SELFTEST] armed: will clobber one boundary tag at t=100000ms
[REDZONE-SELFTEST] INJECTED at t=100000ms: block @0000023680000fd8
                   tag 000000000000000e -> 0000000000000000
[REDZONE-HEAP] ==== corruption at checkpoint 'watchdog' (seq=0) ====
[REDZONE-HEAP] range 0: bad block @0000023680000fd8 tag=0x0000000000000000 size=0 (0x0)
[REDZONE-HEAP] preceding block @0000023680000fa0 raw: tag=000000000000000e ...
[REDZONE-HEAP] sim context: world#1 frame=3439 inUpdate=- comp=0 citizen=0
[REDZONE-HEAP] aborting cleanly at heap corruption          <- ExitProcess(0x123)
```

**Count: 1 injection, 1 detection, 0 missed.** Caught **inside the sim** at
frame 3439, with the full report and the distinctive abort.

*Effectiveness control on the control.* The injector prints the victim address
and the tag before/after, and prints a distinct `NO INJECTION — <reason>` line
if it cannot write. Without that, a null result would have had two causes — "the
watchdog is blind" and "nothing was ever corrupted" — and could not tell them
apart. It is the same defect class as the one under investigation, one level up.

**Therefore (c).** The walk can express a failure, and it ran throughout the
window; the crash is after the window. Lane ALLOC's zero was a real zero.

---

## 5. The fix, and its verification

`standalone/redzone_alloc.cpp`:

* Watchdog loop `for(;;)` → `while (!InterlockedCompareExchange(&g_watchStop,0,0))`.
* `RedzoneStartWatchdog()` (used by BOTH install paths, so the stop can never be
  wired for only one) creates the thread and registers `atexit(&RedzoneStopWatchdog)`.
* `RedzoneStopWatchdog()` sets the flag and joins with a **2 s bound**, so a hung
  walk can never wedge process exit, and says which happened.
* `atexit` is LIFO and `RedzoneAlloc_Install()` runs very early in `WinMain`, so
  this handler runs **last** — the watchdog deliberately stays live through the
  whole world-teardown cascade (which is what the 800×10 teardown hunt uses it
  for) and is silenced only once nothing is left to watch.
* New `g_watchWalks` counter, printed at exit.

**Run D — UIF's exact failing config, with the fix.**
`BANISHED_REDZONE=watch`, RT_DIFF/RT_ALLOC unset, 3000 frames.
Exe md5 `7945da31072669ea76508b6f903ccc45`.
→ `perf/results/20260908_234125_FIXVERIFY_uif_config`

```
[ReadGuard] census: Read=0
[REDZONE-HEAP] watchdog stopped at exit (acknowledged) after 52122 completed heap walks
```
No segfault, zero-byte crash log, clean self-quit. Note the stop line lands
**after** the last census, as designed.

**Run E — lane ALLOC's pass condition, re-established on `wave10/base`.**
`BANISHED_REDZONE=watch` + `BANISHED_RT_ALLOC=1`, 3000 frames, no injection.
→ `perf/results/20260908_234738_PASSCOND_allocarmed_clean`

```
[AllocLock] patches RETIRED for the ported allocator (Allocate=1 Free=1)
[RtAllocSwap] ATOMIC SWAP INSTALLED — every Allocate/Free in the process now runs our port
[REDZONE] WATCH-ONLY mode: no shim, watchdog=1ms
[REDZONE-HEAP] watchdog stopped at exit (acknowledged) after 41349 completed heap walks
```
3000 frames, clean self-quit, zero-byte crash log, **41,349 completed heap
walks, 0 violations**.

That is the same reading lane ALLOC recorded, now with the denominator attached.
"0 violations" alone cannot distinguish a clean heap from a walk that never ran;
"0 violations in 41,349 completed walks, and a planted violation is caught" can.

**No perf numbers from any of these runs are reportable.** Every one of them
carries a 1 ms whole-heap polling watchdog; the `sim ms/iteration` deltas in
their `report.txt` are the watchdog's own cost, not a regression.

---

## 6. Stated limits — what a green `watch` reading still does NOT cover

1. **Only egregious tag corruption.** The walk flags `blkBytes < 8` or a block
   that overruns its range. A clobber that leaves a *plausible* size field walks
   straight through undetected. "Zero violations" means "no egregious
   boundary-tag corruption", never "the heap is clean".
2. **Watch mode has no `g_live` table**, so the report's owner-attribution
   sections are empty (visible in run C: "most-recent 8 live allocations"
   printed nothing, and the victim came back "NOT in our table"). Detection is
   sound; attribution is weak.
3. **`BANISHED_REDZONE=1` performs no heap walk at all** unless
   `BANISHED_REDZONE_WATCH=<ms>` is also set. UIF's "the shim is fine, rc=0,
   0 violations" is a reading from a configuration that never walks the heap —
   it is the vacuous green, and it is the mirror image of the defect we were
   hunting. Anyone reaching for the shim as a substitute for `watch` must set
   `BANISHED_REDZONE_WATCH` too.
4. **The teardown window was never watched before this fix** (the instrument
   crashed there). It now is, up to the final `atexit`. Past runs' silence about
   teardown was the crash, not a clean bill.
5. One injection, one detection. The control establishes that the instrument
   *can* report; it is not a sensitivity study.

---

## 7. Consequences for `WAVE10_LANE_ALLOC.md`

Row (ii) of its pass condition — *"the stated pass condition, DLL PRESENT:
`BANISHED_REDZONE=watch` + `sa-stress`"* — **stands**, and is re-measured here
on `wave10/base` (§5, run E) with the instrument's capability demonstrated
(§4, run C) rather than assumed. No re-run of the GO verdict is required.

The `BANISHED_RT_ALLOC=1` arming decision is unchanged by this lane.
