# Wave 9, lane E2 — the allocator: gate step 2, the measurement

Run 2026-09-07 on `wave/upgrade-path-complete` (worktree, base `ca7ce73`).

**The port has now executed. 12,749,680 allocator operations were replayed into a
private replica and compared against the shipped DLL's live arena, bit for bit, with
zero divergences — and the oracle was proved able to fail on the same binary.**

That closes the single line that has stood under every allocator verdict since Wave 2:
*"The port has never executed a single instruction."* It no longer stands.

**Verdict: GATE STEP 2 IS MET. `BANISHED_RT_ALLOC` is READY TO ARM. It is NOT armed,
and this lane did not arm it.** One coverage gap and one untouched gate are named in
§5 and both must be closed before step 4.

---

## 1. The four GO conditions

| # | Condition | Verdict | Evidence |
|---|---|---|---|
| C1 | `divergences=0` across runs 1–3 | **MET** | 0 / 0 / 0 — §3 |
| C2 | `observed=` in the **millions** on the stress run | **MET** | 4,565,696 (and 1.77M / 6.41M on the other two) |
| C3 | `skipped_pre_startup` / `other_allocator` / `map_full` / `free_unmapped` all **zero** | **MET** | 0 / 0 / 0 / 0 on all three |
| C4 | The negative control goes **red** | **MET** | 392,243 divergences, §4 — but *not* by the control the brief prescribed |

All four measured on **one binary, exe stamped 2026-09-07 23:10:18**, every source file
older than it. Runs differ only by environment variables.

---

## 2. Two harness defects had to be fixed before any number could be believed

Both were found by taking the measurement, and both produced readings that looked
exactly like a catastrophic port defect. Neither was in the port.

### Defect 1 — the oracle was waiting on a function that is never called

The first run reported:

```
[RtAllocShadow] SUMMARY armed=1 observed=0 divergences=0
                skipped_pre_startup=1788875 other_allocator=0 map_full=0 free_unmapped=0
```

`observed=0` with 1.79M allocations skipped. The driver waited on
`?Startup@MemoryAllocator@System@@QEAA_N_K0@Z` (`0x1800c7620`) — and **that function is
never called in the shipped game.** Proved two independent ways over the shipped
binaries, not inferred from names:

* a full rel32 scan of `Runtime-x64.dll`'s `.text` for `CALL`/`JMP` to `0x1800c7620`
  finds **zero** sites. The same scan finds 3 for `AddNewMemoryRange`, 348 for
  `Allocate` and 1 for the ctor, so the scan works;
* `Application-x64.exe`'s PE import table carries exactly **two** `MemoryAllocator`
  symbols, `MemAlloc` and `MemFree`. `Startup` is not imported.

MSVC **inlined** it into `?Startup@Engine@1@QEAAIPEBDII_N@Z` (`0x1800db8a0`), which
writes `minRangeMB<<20` to `+0x10`, `maxTotalMB<<20` to `+0x18`, the page size to
`+0x668`, and calls `AddNewMemoryRange` at `0x1800db905`. That is the only place the
arena is ever created. The trigger moved there (19-byte prologue; `MOV RAX,RSP` is an
RSP capture that `InstallDetour` detects and corrects).

**And it must arm on the way IN, not the way out.** Arming after the original returned
gave `real arena live=12765 bytes=906440 ranges=1` with 17,017 calls already skipped:
`Engine::Startup` does not merely start the allocator, it boots the whole engine, and
~12.8k of the process's allocations happen inside it. That produced 1,684,998
"divergences" — 100% of comparisons — from a replica that started where the real arena
had already been.

Entry to `Engine::Startup` is the one virgin instant, and that is **structural, not
lucky**: `AddNewMemoryRange` divides by `self->pageSize`, and the ctor leaves `pageSize`
at 0 (`allocator.cpp` `Construct`), so no allocation can precede `Engine::Startup`
without dividing by zero. The driver now asserts it anyway — `rangeCount==0 &&
allocCount==0 && allocatedBytes==0 && skipped==0` — and declares the run **INVALID**
rather than measuring if it ever fails. It held on all six runs:

```
[RtAllocShadow] Engine::Startup ENTRY (min=512MB max=0MB): real arena live=0 bytes=0 ranges=0, skipped so far=0
```

### Defect 2 — the replay map deleted by writing the probe terminator

With the trigger fixed, the run reported `free_unmapped=207` and **1,765,043
divergences whose first instance was a drift of exactly ONE**
(`ours live=13702` vs `theirs live=13701`) — while the range offsets, the term that
actually proves the port, agreed to the byte (`ours=r0+0x18def0` == `theirs=r0+0x18def0`).

`ShadowMapTake` emptied a slot by writing `nullptr`, which is also the linear-probe
terminator. Deleting an entry therefore **truncated the probe chain** of every key that
had collided into a later slot; those keys became unfindable, their `Free` was dropped,
the replica kept a block the real arena had released, and — by this design's own
"a divergence never recovers" property — every subsequent comparison was red.

Replaced with **backward-shift deletion** (Knuth 6.4 algorithm R), which keeps the
no-tombstone invariant and cannot silt up over a run's ~900k insert/erase pairs the way
a tombstone scheme would. `free_unmapped` went 207 → **0** and divergences 1,765,043 →
**0** in one change.

This is the shape `WAVE9_LANE_E.md` correction 4 anticipated — it added the counter that
made the defect visible, which is why it took minutes to find rather than a bisection.
It counted the symptom; this lane fixed the cause.

---

## 3. The three measurement runs

Exe: `C:/GOG Games/Banished/reimpl-x64.exe`, **stamp 2026-09-07 23:10:18**, `-O0` debug.
`BANISHED_RT_ALLOC` **unset** in every one — the shadow refuses to arm otherwise, and
the heap was never taken.

| Run | Command | `observed` | `divergences` | `skipped_pre_startup` | `other_allocator` | `map_full` | `free_unmapped` |
|---|---|---:|---:|---:|---:|---:|---:|
| 1 autostart | `BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1 BANISHED_AUTOSTART=1` | **1,771,964** | **0** | 0 | 0 | 0 | 0 |
| 2 stress town | `… tools/stress_run.sh --debug --frames 1500` | **4,565,696** | **0** | 0 | 0 | 0 | 0 |
| 3 save load | `… BANISHED_AUTOLOAD=Save9` | **6,413,213** | **0** | 0 | 0 | 0 | 0 |
| | **total** | **12,750,873** | **0** | | | | |

Per-site `[RtDiff]` rows, read out of the **raw** `standalone_error.log` (never a
`tail -N` — `reference_truncated_tool_output_manufactures_calls_zero.md`):

```
run 1  System::MemoryAllocator::Allocate [shadow]  calls=946779   divergences=0
       System::MemoryAllocator::Free     [shadow]  calls=825185   divergences=0
       [RtDiff] ==== 308 sites, 30285618 calls, 0 divergences in 0 sites ====
run 2  System::MemoryAllocator::Allocate [shadow]  calls=2350226  divergences=0
       System::MemoryAllocator::Free     [shadow]  calls=2215470  divergences=0
       [RtDiff] ==== 347 sites, 132418387 calls, 3 divergences in 1 sites ====
run 3  System::MemoryAllocator::Allocate [shadow]  calls=3276755  divergences=0
       System::MemoryAllocator::Free     [shadow]  calls=3136458  divergences=0
       [RtDiff] ==== 332 sites, 58426267 calls, 0 divergences in 0 sites ====
```

The compared invariant is `(rangeIndex, offsetWithinRange, allocatedBytes, allocCount,
freeTagCount, minBin)`. `offsetWithinRange` is the load-bearing term: it is the exact
byte the port chose to hand out, so agreement on it exercises bin selection, the
first-fit walk, the alignment split, the tail split, forward and backward coalescing and
the 41-bit split free-list encoding — and a single wrong bit desynchronises it forever.

**The 3 divergences in run 2 are NOT the allocator.** They are
`Picking::Scene::Intersect`, 3 of 38,020 calls, last-bit float differences in
`outPos.z` (`-0.244819641` vs `-0.244815826`). Pre-existing, Wave 5's lane, unrelated
and unchanged by anything here.

### On threading — the brief's live limitation, answered

`rt_diff.cpp` genuinely has no mutex, atomic, `Interlocked` or `CRITICAL_SECTION`, and
the allocator genuinely runs on multiple threads. **The shadow is nonetheless
serialized**, and not by luck: `RtAllocShadowAlloc`/`Free` are called from inside
`AllocateLocked`/`FreeLocked` **while those hold the allocator's own
`CRITICAL_SECTION` at `self+0x640`** — the same one on the same object for both entry
points. The replica, the replay map, `g_shadowCalls`, `g_shadowDivergences`,
`g_shadowSkipped` and the two `RT_DIFF` site counters are all inside that section, so
none of them is racy. `ShadowStartup` runs before any second thread exists.

The one way this could break is a *second* `MemoryAllocator` instance, whose wrappers
would take a different CS. That is what `other_allocator` counts, and it is **0** on
every run — there is exactly one allocator in the process.

---

## 4. The negative control — and the prescribed one does not work

Both are permanent and env-gated (`BANISHED_RT_ALLOC_SHADOW_NEGCTL`), not a temporary
edit, so the next reader can re-run them against the same binary.

### `=minbin` — the control the brief and `WAVE9_LANE_E.md` §5 prescribe — is a NO-OP

Starting the replica's `minBin` at `0xbe` instead of `0xbf` came back **GREEN**:
`observed=1690459 divergences=0`. It cannot fail, by construction. `LinkFreeTag`
(`allocator.cpp:252`) drives `minBin` **down** — `if (bin < self->minBin) self->minBin = bin`
— and `AddNewMemoryRange`, which `RtAllocShadowStartup` calls while arming, links the
initial 512 MB block whose bin is 148. The perturbation is overwritten before the first
comparison ever runs.

**Had this been accepted as the negative control, the whole gate would have rested on a
probe shown only to be green when broken.** It is the
`reference_probe_needs_an_effectiveness_control.md` shape again: the control found a
defect in the *control*.

### `=size` — a geometry perturbation — goes RED, hard

Inflate every 100,000th replica request by 8 bytes:

```
[RtAllocShadow] NEGATIVE CONTROL ACTIVE (size) — this run is REQUIRED to go red
[RtAllocShadow] SUMMARY armed=1 observed=1698807 divergences=392243
                skipped_pre_startup=0 other_allocator=0 map_full=0 free_unmapped=0
  Allocate [shadow] calls=913100 divergences=178235
  Free     [shadow] calls=785707 divergences=214008
```

Caught on the **very first perturbed call** (`bytes=2226360` vs `2226352`), with the
geometry term desynchronising on the next one (`ours=r0+0x19cc00` vs
`theirs=r0+0x19cbf8`, exactly 8 apart) and never recovering. Same binary as run 1, one
environment variable apart: **1,771,964 clean → 392,243 red.**

That is the half that matters. An 8-byte error anywhere in the allocator is detected
immediately and permanently, which is precisely the "wrong bit in the free-list prev
split is a wild write with no proximate symptom" risk the gate exists for.

---

## 5. What is still NOT proved — read this before step 4

### 5.1 The arena-growth path never executed. This is a real gap.

The coverage control added by this lane reports, on **all three** runs:

```
[RtAllocShadow] COVERAGE ranges ours=1 theirs=1  reserved ours=512MB theirs=512MB
```

One range. `AddNewMemoryRange` ran exactly **once** (during arming),
`ExpandMemoryRangeTable` once. The multi-range paths — a second `MemoryRange`, the
range-table realloc-and-copy, `GetMemoryRange`'s multi-range search, and every
`rangeIndex != 0` comparison — are **entirely unexercised by these 12.75M operations**.
`divergences=0` says nothing about a path never taken; that is Wave 1 defect #4 one
level down, and the reason this control was added rather than assumed.

The 512 MB initial range is simply never filled: the stress town peaks around 700 MB
*private* but well under 512 MB of *arena*. Closing this needs a run that forces a
second range — a smaller `minRangeMB` at `Engine::Startup`, or a bigger town. **It
should be closed before step 4, not after.**

### 5.2 Functions the shadow does not reach

The oracle drives `Allocate` and `Free` and everything they call. It does **not**
exercise `Shutdown`, `~MemoryAllocator`, `operator=`, `GetAllocationSize`,
`GetAllocatedSize` or `Get`. Those remain in the Wave 1 defect #4 category.

### 5.3 The 5% performance gate is untouched

Every run here is the `-O0` debug exe with `BANISHED_RT_DIFF=1`, which the harness
itself says must not be benchmarked. The `sim ms/iteration` gate is gate step 4's, needs
three runs per config (`reference_one_perf_run_cannot_resolve_the_gate.md`), and nothing
in this lane speaks to it.

### 5.4 `BANISHED_REDZONE=watch` could not add information — for a pre-existing reason

The brief asks for `sa-stress` under `BANISHED_REDZONE=watch`. It ran and crashed
(`ACCESS_VIOLATION` in `MemoryAllocator::Free+0x5f`). **It is not this lane's**, proved
by two controls on the same binary and the same town:

| config | outcome |
|---|---|
| stress + shadow + redzone=watch | crash at ~frame 1200–1500 |
| stress + redzone=watch, **shadow and RT_DIFF off** | crash at ~frame 1200–1500 |
| stress, **everything off** (plain control) | **crash at ~frame 1200–1500** |
| stress + shadow, no redzone | **completed 1500 frames, report.txt written, no crash** |

The `-O0` debug stress town crashes near the end of a 1500-frame run with every
mechanism from this lane switched off, so `redzone=watch` cannot be used here to say
anything about the allocator port. Recorded as a pre-existing instability of
`sa-stress-debug`, which the Makefile already labels "functional check only".

The wild-write risk is not unaddressed, though: the port writes **only** into the
replica arena, so a bad 41-bit prev-link would corrupt the replica and desynchronise the
offsets — which is exactly what the `=size` control demonstrates the oracle catches on
the first occurrence.

---

## 6. Verdict, and what GO does and does not mean

**`WAVE2_ALLOCATOR.md` §5's verdict moves from NO-GO to GO on gate step 2.** Its
"evidence against" list now reads:

| §5 "evidence against" | Status |
|---|---|
| "The port has never executed a single instruction." | **CLOSED.** 12,750,873 operations, bit-identical. |
| "The shadow harness has never run either." | **CLOSED.** Six runs, and it is proved able to fail. |
| "The swap installer and the shadow driver do not exist." | **CLOSED** by lane E. |
| "`allocator_lock.cpp` installs after `RuntimePortInit`." | **CLOSED** by lane E's reorder. |
| "1,778 inbound edges … a wrong bit in the free-list prev split is a wild write." | **LARGELY CLOSED** for the single-range case; **OPEN** for multi-range (§5.1). |

**GO means "ready to arm". It does not mean armed, and this lane did not arm it.**
`BANISHED_RT_ALLOC` was never set in any run here; the shadow refuses to arm if it is.
Flipping the heap is a separate decision, and it must not be taken while other lanes are
mid-edit — a heap cannot be half ours.

Before gate step 4 (`BANISHED_RT_ALLOC=1`):

1. close §5.1 — force a second `MemoryRange` and re-run the shadow;
2. then PLAN.md §5's four wave-boundary gates: tutorials 1–4, the achievement census,
   `Save9.sav`, and `sim ms/iteration` within 5% over three runs per config;
3. on that run, check for lane E's four `[AllocLock]`/`[RtAllocSwap]` lines in order.
   If `patches RETIRED for the ported allocator` is missing, the run is unlocked — kill it.

---

## 7. Files changed by this lane

| File | Change |
|---|---|
| `standalone/rt_alloc_swap.cpp` | arming trigger moved to `Engine::Startup` (`0x1800db8a0`, 19 bytes) and moved to **before** the original body; virginity assertion; the `Engine::Startup ENTRY` effectiveness line; the `COVERAGE` line |
| `runtime/src/system/allocator.cpp` | replay map: backward-shift deletion (`ShadowMapErase`); `ShadowMapInsert` rejects a null key; the two env-gated negative controls; `RtAllocShadowCoverage` |
| `runtime/types/system/MemoryAllocator.h` | declares `RtAllocShadowCoverage` |
| `docs/runtime_port/WAVE9_LANE_E2.md` | this file |

Nothing default-on changed. Every new knob is off unless an env var sets it, and
`BANISHED_RT_ALLOC` was not touched.

### Reproducing

```bash
mingw32-make standalone-debug && mingw32-make sa-deploy
tools/rt_run.sh a  -- BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1 BANISHED_AUTOSTART=1 \
                     BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60
tools/rt_run.sh s  -- BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1 BANISHED_AUTOLOAD=Save9 \
                     BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60
BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1 tools/stress_run.sh --debug --frames 1500
# negative control — REQUIRED to go red:
tools/rt_run.sh n  -- BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1 \
                     BANISHED_RT_ALLOC_SHADOW_NEGCTL=size BANISHED_AUTOSTART=1 \
                     BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60
```

Read `[RtAllocShadow] SUMMARY` and `COVERAGE` out of the raw `standalone_error.log`, and
read `observed=`/`calls=`, not just `divergences=`.
