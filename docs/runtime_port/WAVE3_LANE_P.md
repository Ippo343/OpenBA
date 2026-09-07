# Wave 3, Lane P — the performance gate

Owner: Lane P. Ran 2026-09-06 on branch `wave/upgrade-path-complete`, base commit
`75c9592` ("Wave 3 scaffolding: RtObjectPortInit + three lane files, no ports yet").

PLAN.md §5 has an unchecked box: `sa-stress` `sim ms / iteration` within 5% of the
pre-port baseline at each wave boundary. WAVE2.md recorded it as **BREACHED and open**,
split it as "~2.5 points Wave 2's own, ~4.5 points predate it", and attributed Wave 2's
share to "the runtime registry indirection". This lane measured all three claims.

**Result in one line: the gate is breached at +7.8%, essentially all of it is the ports,
and 3.9 of the 7.8 points are ONE defect — `System::Time`'s two ported members call
`GetProcAddress` on every single call. A six-line fix brings the boundary to +2.2%, inside
the gate.**

Two of WAVE2.md's three claims were wrong, and both were wrong in the direction that made
the wave look better than it was.

---

## 1. Method, and what was controlled

Every number below is `update_ms_per_iter` averaged over the steady-state rows (rows 3-10,
the first two are warm-up), from `perf/results/<stamp>_<label>/standalone_stress.csv`.

* **One binary for the whole A/B set.** `BANISHED_RT_PORT`, `BANISHED_RT_MATH`,
  `BANISHED_RT_VOLUME`, `BANISHED_RT_SYSTEM` and the per-class `BANISHED_RT_<CLASS>`
  switches are *runtime* switches. The same compiled exe runs every configuration, so the
  comparison is purely "which implementation executed", with no compiler, layout or
  inlining differences in it. This is a much cleaner instrument than rebuilding history,
  and it is why no `git checkout` of an older tree was needed.
* **A clean worktree.** Three other Wave 3 lanes are editing `runtime/src/object/*`,
  `code/Game/*` and `standalone/main.cpp` in the primary tree *while this lane ran*. The
  first four runs (`p1`..`p6`) measured that contaminated tree; every decisive run was
  redone in `git worktree add /c/code/GIT/wave3_laneP 75c9592`, its own object dir, its own
  `build-perf`. The contaminated and clean ports-on numbers agree (0.2076 vs 0.2074), so
  the lanes' in-flight work is inert — but the clean runs are the ones quoted.
* **Status icons ON** (`GameStatusIconEnable = true` in the registry) for every run,
  matching `perf/baselines/current.csv`. Verified in the registry before the first run and
  in each run's `[GameSettings] Load: StatusIconEnable=1` line. This setting alone moves
  the number 10%, per `reference_perf_measurement_traps_icons_and_stale_exe.md`.
* **Repeats.** 3-5 runs per configuration, back to back, nothing else running. Observed
  run-to-run standard deviation was 0.0008-0.0017 ms/iter (0.4-0.9%), tighter than the
  +/-0.005 the memory note warns about — because these were sequenced rather than
  interleaved with builds. That precision is what makes a 1% effect resolvable at all;
  every single-run A/B in this project's history, including WAVE2.md's, could not resolve
  one.
* **`--no-build` only after a build in the same shell had already deployed the exe**
  (`feedback_no_build_skips_the_deploy.md`). Where a build was hand-run, the `cp` to the
  game dir was hand-run too, immediately before.
* One diagnostic at a time. The sampled runs used `BANISHED_SAMPLE=1000` with
  `BANISHED_STRESS_COMPPROF=0`, never both.

Harness for every run: `tools/stress_run.sh`, default town — medium map, seed 31337, 800
citizens, 3 sets, speed 10, 6000 frames, icons on, PerfX on. Sampled runs used
`--frames 3000`.

**One measurement trap found and worth recording:** the `build=` stamp in `report.txt` is
`__DATE__ __TIME__` of a translation unit that `ccache` reuses. After editing
`runtime/funcs/rt_diff.h` and rebuilding, the freshly deployed exe still reported
`build=Sep 6 2026 17:29:44` — the previous build's stamp. The stamp is a good check that a
build *failed*, but it does **not** prove a build changed. For that, disassemble the symbol
you changed in the deployed exe, which is what was done here.

---

## 2. Headline numbers

Clean worktree at `75c9592`, one binary, icons on, 800x10, 6000 frames.
Baseline `perf/baselines/current.csv` (2026-09-05, pre-Wave-1) = **0.19238 ms/iter**.

| Configuration | env | n | mean ms/iter | sd | vs baseline | vs ports-off |
|---|---|---|---|---|---|---|
| **all ports off** | `BANISHED_RT_PORT=0` | 4 | **0.1949** | 0.0008 | **+1.3%** | — |
| Math + Volume only (Wave 1) | `BANISHED_RT_SYSTEM=0` | 3 | 0.1969 | 0.0006 | +2.3% | +1.0% |
| System only (Wave 2) | `BANISHED_RT_MATH=0 BANISHED_RT_VOLUME=0` | 3 | 0.2043 | 0.0013 | +6.2% | +4.8% |
| **all ports on — the Wave 2 boundary as landed** | (default) | 4 | **0.2074** | 0.0013 | **+7.8%** | **+6.4%** |
| all on, diff oracle compiled out | `PERF_EXTRA=-DBANISHED_RT_DIFF_COMPILED_OUT` | 4 | 0.2056 | 0.0017 | +6.9% | +5.5% |
| **all on + Timer resolve cached (the fix)** | (default) | 5 | **0.1967** | 0.0013 | **+2.2%** | +0.9% |

Individual run values are in the table at §7. Per-wave costs are additive to within noise
(1.0 + 4.8 = 5.8 vs 6.4 measured together), which is what you want to see before trusting a
decomposition.

---

## 3. Question 1 — attributing the points that "predate Wave 2"

**WAVE2.md's split was wrong.** It said ~2.5 points were Wave 2's and ~4.5 predated it.
Measured:

| share | points of the 7.8 | evidence |
|---|---|---|
| drift — non-port work landed between 2026-09-05 and the baseline commit | **+1.3** | ports-off on today's binary vs the baseline CSV: 0.1949 vs 0.1923, n=4, sd 0.0008 |
| Wave 1 (Math + Volume) | **+1.0** | `BANISHED_RT_SYSTEM=0` (0.1969) minus ports-off (0.1949) |
| Wave 2 (System) | **+4.8** | `BANISHED_RT_MATH=0 RT_VOLUME=0` (0.2043) minus ports-off (0.1949) |
| interaction / non-additivity | +0.7 | 0.2074 measured vs 0.1949+1.0+4.8 predicted |

So **Wave 2 owns roughly 5 of the 7.8 points, not 2.5**, and only 1.3 points genuinely
predate the runtime port — all of it ordinary exe-side drift, none of it Wave 1's registry
seam. Wave 1's own cost is real but small (+1.0%).

Why WAVE2.md got the opposite answer: its `BANISHED_RT_SYSTEM=0` control was a **single
run**. Single runs at this noise level cannot separate 2.5% from 5%. Re-running that exact
control four times gives 0.1969 +/- 0.0006, and the conclusion inverts.

Wave 1 also has a directly comparable number now, which it never had: the run at
`perf/results/20260906_151553_wave1_full6000` (build Sep 6 15:14:26, the Wave 1 tree just
before `e635a93`) measured 0.1944 — statistically identical to today's ports-off 0.1949.
That independently confirms Wave 1 cost about nothing, from a different binary on a
different tree.

---

## 4. Question 2 — where the time actually goes

### 4.1 "Registry indirection" is not a thing that happens

WAVE2.md's stated mechanism — "every ported call now goes through the runtime registry
indirection, and `Random::Rand` alone is 16M calls per run" — **does not describe the
code.** `standalone/resolve_standalone.cpp:278` and `:411`: the registry map is consulted
inside `ResolveAddr` / `ResolveExport` / `ResolveOrdinal`, which callers invoke **once** and
cache in a `fn_*` static. A ported call site does not touch `RtAddrReg()` or `RtExportReg()`
at call time; it calls a function pointer, exactly as it did before, at a different target.
There is no per-call map lookup and no per-call string hash. `Random::Rand`'s 16M calls do
not appear anywhere in the sim profile (`RtSystem_Random_RandBound` is 2 samples of 5999),
because almost all of them are worldgen, not the tick.

The hypothesis was plausible and it was never checked. It is false.

### 4.2 The real cost, named: `GetProcAddress` on the sim's hottest value type

`runtime/funcs/rt_port.h:57`:

```cpp
template <typename Fn>
inline Fn RtOriginal(const char* mangled) {
    return reinterpret_cast<Fn>(ResolveExportRaw(mangled));
}
```

and `ResolveExportRaw` is `GetProcAddress(runtimeHandle, mangled_name)` — a binary search
over Runtime-x64.dll's ~4,800-name export table, by string, under the loader lock.

`runtime/src/system/value_types.cpp:237-251` calls it **in the function body, on every
call**, for both ported members of `System::Time`:

```cpp
Time* RtSystem_Time_Ctor(Time* self, float seconds) {
    using SecondsToTicksFn = int64_t(*)(float);
    self->ticks = RtOriginal<SecondsToTicksFn>("?SecondsToTicks@Timer@System@@SA_JM@Z")(seconds);
    return self;
}

float RtSystem_Time_AsSeconds(const Time* self) {
    using TicksToSecondsFn = float(*)(int64_t);
    return RtOriginal<TicksToSecondsFn>("?TicksToSeconds@Timer@System@@SAM_J@Z")(self->ticks);
}
```

`System::Time` is the value type every `Update(const Time&)` in the engine passes and every
dt conversion constructs. `Time(float)` and `AsSeconds()` are two of the most-called
functions in the tick, and each of them was doing a string-keyed export lookup.

Three independent lines of evidence:

1. **A/B, per-class.** Bisecting the System wave with its own sub-switches (one run each,
   then confirmed by the fix below):

   | switch turned off | ms/iter |
   |---|---|
   | `BANISHED_RT_STRING=0` | 0.208 |
   | `BANISHED_RT_FILENAME=0` | 0.207 |
   | `BANISHED_RT_THREAD=0` | 0.209 |
   | **`BANISHED_RT_VALUE=0`** | **0.198** |

   Only `RT_VALUE` (Random / Time / DateTime / Color / Configuration) moves the number, and
   it accounts for essentially the whole System wave.

2. **The sampler, and it is not where you would look.** `BANISHED_SAMPLE=1000`,
   `--tag 4,8,6,1,3`, 3000 frames. `RtSystem_Time_Ctor` and `RtSystem_Time_AsSeconds`
   barely appear in *self* time — the cost is not in our code, it is in the callee. What
   moves is **ntdll**:

   | run | ntdll self time |
   |---|---|
   | ports ON (as landed) | **8.58%** (515 samples) |
   | ports OFF | 5.06% (285) |
   | ports ON + the fix | 4.88% (276) |

   +3.5 points of ntdll appearing and disappearing exactly with the ports is the loader
   lock and the export-table search. This is the reason CLAUDE.md tells you to run the
   sampler before the component table: the `[COMPPROF]` table sees only vtable dispatch and
   would have shown nothing at all here.

3. **The fix.** Resolve each pointer once into a function-local static:

```diff
 Time* RtSystem_Time_Ctor(Time* self, float seconds) {
     using SecondsToTicksFn = int64_t(*)(float);
-    self->ticks = RtOriginal<SecondsToTicksFn>("?SecondsToTicks@Timer@System@@SA_JM@Z")(seconds);
+    // RtOriginal is GetProcAddress; calling it per Time(float) put an
+    // export-table binary search on the sim's hottest value type.
+    static SecondsToTicksFn s_fn = nullptr;
+    if (!s_fn) s_fn = RtOriginal<SecondsToTicksFn>("?SecondsToTicks@Timer@System@@SA_JM@Z");
+    self->ticks = s_fn(seconds);
     return self;
 }

 float RtSystem_Time_AsSeconds(const Time* self) {
     using TicksToSecondsFn = float(*)(int64_t);
-    return RtOriginal<TicksToSecondsFn>("?TicksToSeconds@Timer@System@@SAM_J@Z")(self->ticks);
+    static TicksToSecondsFn s_fn = nullptr;   // resolved ONCE, see above.
+    if (!s_fn) s_fn = RtOriginal<TicksToSecondsFn>("?TicksToSeconds@Timer@System@@SAM_J@Z");
+    return s_fn(self->ticks);
 }
```

   **0.2074 -> 0.1967, five runs, sd 0.0013.** +7.8% becomes +2.2%. The gate passes.

   Behaviour is unchanged: the resolved target is the same DLL function, and the port was
   never diffed here anyway (the comment in the file says so — both "ours" and "theirs"
   would call the identical export).

### 4.3 Other live-path `RtOriginal` sites — a Wave 3 hazard, not just a Wave 2 bug

`RtOriginal` reads like a cheap accessor and is spelled like one. Every use of it outside
an `if (rt_diff_on__)` block is a `GetProcAddress` on the live path. Auditing all 92 call
sites in `runtime/src/`, these are the ones **not** inside a diff guard:

| site | frequency | verdict |
|---|---|---|
| `system/value_types.cpp:240, 248` — `Time` | per tick, enormous | **the defect above; fixed** |
| `system/string.cpp:69, 75` — MemAlloc/MemFree | once | fine, assigned to file statics |
| `system/value_types.cpp:295-377` — `DateTime::AsString`, `Configuration::*` | UI / startup | acceptable, not hot |
| `system/string.cpp:597, 705, 730` — `ToFloat`, `FormatParamCount`, `FormatParamList` | per format call | **worth caching**; not measurable in this town but it is the same shape |
| `system/filename.cpp:168`, `system/string.cpp:413, 430, 450` | comparison operators | inside `RT_DIFF_*`, only evaluated when diffing |
| `system/allocator.cpp:128, 129` — `Error::SetText`/`Display` | fatal path | fine |
| `math/*`, `volume/*` | all inside `RT_DIFF_*` | fine |

**Recommendation for Wave 3 lanes A/B/C:** never call `RtOriginal` from a function body on
the live path. Resolve into a file-scope or function-local static. Consider renaming it
`RtResolveOriginal`, or adding a `RT_ORIGINAL_ONCE(name)` helper that makes the caching the
default shape, so the next lane cannot repeat this by writing the obvious thing.

### 4.4 The diff oracle's frame overhead — real, small, and NOT the story

`RT_DIFF_BEGIN` puts `const bool rt_diff_on__ = RtDiffEnabled();` at the top of every ported
function, and `RtDiffEnabled()` is defined in another translation unit with no LTO, so it is
a real call. It also forces a stack frame on functions that would otherwise be frameless
leaves: `RtMath_Transform_Assign`, whose whole body is a 28-byte `memcpy`, compiles to four
pushes, `sub $0x48,%rsp`, the copy, four pops. It shows in the sampler at 1.60% self against
the DLL's `??4Transform@Math@@` at 0.76% — a 2.2x on the same function with the same call
count, which is what put this on the suspect list first.

Two experiments:

* Making the flag a global so `RtDiffEnabled()` inlines to a load-and-test: **no change**
  (0.2074 -> 0.209 on the same instrument). The frame survives, because the cold diff path
  still needs the callee-saved registers.
* Compiling the oracle out entirely (`-DBANISHED_RT_DIFF_COMPILED_OUT`, verified by
  disassembly — `RtMath_Transform_Assign` becomes a frameless five-instruction leaf,
  instruction-for-instruction what the DLL does): **0.2074 -> 0.2056, about 0.9%**, n=4,
  sd 0.0017. Real but marginal, and about a fifth of the size the sampler's 2.2x suggested.

So the sampler's per-function ratio overstated it: at this granularity a 28-byte copy's
samples land partly on the caller. **Recorded as a negative result.** It is not worth
adding a build switch for 0.9% while the oracle is the wave's main correctness instrument —
revisit only if a later wave's ported leaves become hot enough to matter.

---

## 5. Question 3 — the gate

### Do not re-baseline

`perf/baselines/current.csv` stands. With the Timer fix the boundary is **+2.2%**, inside
the 5% gate, so there is nothing to re-baseline *around* — the recommendation to move the
baseline would only ever have been a way to hide the defect this lane found. The 1.3 points
of genuine pre-port drift are well inside the gate on their own and do not justify moving
it either.

The one thing worth doing eventually: when the runtime port reaches a milestone where the
tree is *deliberately* different (the cutover in PLAN §4.1, when the DLL stops being
loaded), record a new baseline as `perf/baselines/<date>_<commit>.csv` **and keep
`current.csv` alongside it**, so a regression can still be bisected against the pre-port
number. Not now.

### The harness now states the gate

`tools/stress_report.py` compares `sim ms / iteration` at a **10%** REGRESSION threshold
while PLAN's gate is **5%**. That is why the Wave 2 boundary printed "0 regressions over
threshold" while breaching the gate by 7.8% — a reporter that says "clean" on a failing
boundary is a measurement defect, so it is fixed here. The reporter now prints, after the
baseline table:

```
  PLAN 5% gate (sim ms/iter): +7.8%  FAIL
```

`PLAN_GATE_PCT = 5.0` at the top of `COMPARE`. The 10% thresholds are unchanged — they are
useful as a "something moved a lot" alarm across many metrics; the gate line is the
definition-of-done. Verified both ways: the as-landed run prints FAIL at +7.8%, the fixed
run prints PASS at +1.6%.

### What to measure against, going forward

* Headline: `sim ms / iteration` from `sa-stress`, default town, icons ON, against
  `perf/baselines/current.csv` = 0.19238.
* **At least three runs per configuration.** One run cannot resolve the effects this wave
  is producing; the wrong Wave 2 split came from a single control run and stood for a day.
* Always take the ports-off A/B (`BANISHED_RT_PORT=0`) at a wave boundary as well as the
  ports-on number. It is free — same binary — and it separates "our port is slow" from
  "something else drifted" in one extra run. Neither Wave 1 nor Wave 2 did this.

---

## 6. Should anything block Wave 3 from landing?

**No hard blocker.** One thing that should land with or before Wave 3:

1. **Apply the Timer resolve cache** (§4.2). Without it the Wave 2 boundary is a genuine
   +7.8% gate failure that Wave 3 would inherit and be blamed for. It is six lines, no
   behaviour change, and it is the whole difference between FAIL and PASS. This lane did
   not apply it to the primary tree because `runtime/src/system/value_types.cpp` is outside
   the files this lane was given and three lanes are editing the tree concurrently; the
   patch is in §4.2 verbatim, and as a file at
   `perf/results/laneP_timer_resolve_cache.patch` (gitignored directory, so copy it out
   before relying on it).
2. **Tell lanes A/B/C about `RtOriginal`** (§4.3) before their code lands, not after.

---

## 7. Every run

All under `perf/results/`. Clean-worktree runs were produced in
`/c/code/GIT/wave3_laneP` and copied into the primary tree's `perf/results/` (a gitignored
directory). Command in every case:
`tools/stress_run.sh [--no-build] --label <label>`, with the env prefix shown.

| label | env prefix | tree | ms/iter |
|---|---|---|---|
| `p1_on` | — | primary (3 lanes editing) | 0.2076 |
| `p2_allportsoff` | `BANISHED_RT_PORT=0` | primary | 0.1936 |
| `p3_sysoff` | `BANISHED_RT_SYSTEM=0` | primary | 0.1988 |
| `p4_mathvoloff` | `BANISHED_RT_MATH=0 BANISHED_RT_VOLUME=0` | primary | 0.2052 |
| `p5_sample_on` | `BANISHED_SAMPLE=1000 BANISHED_STRESS_COMPPROF=0`, `--frames 3000` | primary | sampled, not comparable |
| `p6_sample_off` | as above + `BANISHED_RT_PORT=0` | primary | sampled, not comparable |
| `w1_clean_on` | — | worktree `75c9592` | 0.2092 |
| `w2_clean_off` | `BANISHED_RT_PORT=0` | worktree | 0.1956 |
| `w3_diffinline_on` | — (patched `RtDiffEnabled` to inline) | worktree | 0.209 |
| `w4`,`w5`,`w6`,`w7`_diffout | — (`PERF_EXTRA=-DBANISHED_RT_DIFF_COMPILED_OUT`) | worktree | 0.2034 0.2065 0.2054 0.2072 |
| `w8_on`,`w9_on`,`w10_on` | — | worktree | 0.2064 0.2072 0.2066 |
| `wo1`,`wo2`,`wo3_off` | `BANISHED_RT_PORT=0` | worktree | 0.1939 0.1946 0.1955 |
| `ws1`,`ws2`,`ws3_sysoff` | `BANISHED_RT_SYSTEM=0` | worktree | 0.1963 0.1969 0.1975 |
| `wm1`,`wm2`,`wm3_mathvoloff` | `BANISHED_RT_MATH=0 BANISHED_RT_VOLUME=0` | worktree | 0.2034 0.2037 0.2057 |
| `wsub_STRING` | `BANISHED_RT_STRING=0` | worktree | 0.208 |
| `wsub_VALUE` | `BANISHED_RT_VALUE=0` | worktree | **0.198** |
| `wsub_FILENAME` | `BANISHED_RT_FILENAME=0` | worktree | 0.207 |
| `wsub_THREAD` | `BANISHED_RT_THREAD=0` | worktree | 0.209 |
| `wfix1`..`wfix5_timecache` | — (Timer resolve cached) | worktree | 0.1954 0.1954 0.1966 0.1980 0.1981 |
| `wfix_sample` | `BANISHED_SAMPLE=1000`, `--frames 3000` | worktree | sampled, not comparable |

Builds: the primary-tree runs used `build=Sep 6 2026 17:14`; the worktree used
`Sep 6 2026 17:29:44` and later rebuilds (see the `ccache` stamp caveat in §1 — the
compiled-out and Timer-fix builds were verified by disassembling the changed symbol in the
deployed exe, not by the stamp).

---

## 8. What remains unmeasured

Stated explicitly, because a perf report that does not say what it did not look at is worth
less than one that does.

* **Only one town.** 800 citizens, 10x, medium map, seed 31337, 6000 frames. The ports'
  cost at 1x, at 50x, on a large map, or in a town with heavy trade/serialisation traffic
  is not measured. `System::String` and `System::Filename` in particular are almost inert in
  this town (`RT_STRING=0` and `RT_FILENAME=0` moved nothing) but would dominate a
  save/load or a mod-mount benchmark. **A ported String that is 2x slow would not show up in
  any number in this document.**
* **Only the sim.** `update_ms_per_iter` is the sim tick. Draw CPU moved within noise
  (1.26-1.30 ms across everything) and was not analysed; GPU time is not measured at all.
  Nothing in the ports is on the render path yet, which will change.
* **Boot, world build and teardown are excluded** — they are the warm-up rows the report
  drops. `Random::Rand`'s 16M calls are mostly there. If the Timer defect had been in
  worldgen instead of the tick it would have been invisible to this whole instrument.
* **The 1.3 points of pre-port drift are not decomposed.** They are measured (ports-off
  today vs the 2026-09-05 baseline, n=4, sd 0.0008) but not attributed to a specific commit.
  Bisecting 1.3% would take ~8 runs per step and it is well inside the gate; not worth it
  now, but it is the reason "drift" is a label here and not an explanation.
* **The `RtOriginal` audit is by inspection, not by instrumentation.** 92 sites were read
  and classified; the "acceptable, not hot" verdicts in §4.3 are judgements about call
  frequency, not counts. A counter on `ResolveExportRaw` would settle them and does not
  exist.
* **`-flto` was not tried.** It would let `RtDiffEnabled()` inline across TUs and might
  recover the 0.9% of §4.4 without a build switch, but the optimisation log already records
  `-flto` as neutral overall on this build, and it was not retested here.
* **The allocator port is still not live** (`RtSystemAllocatorRegister` refuses to arm), so
  none of these numbers say anything about what it will cost. Given that the allocator
  critical section is already ~13% of the bucket phase, that is the next boundary to watch.
