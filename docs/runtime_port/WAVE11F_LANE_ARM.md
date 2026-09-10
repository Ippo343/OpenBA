# Wave 11 follow-up #1 — lane ARM: `BANISHED_RT_INTERNAL_TABLE` is now default-on

**Worktree** `C:/code/GIT/wt-w11f-arm` · **branch** `wave11f/arm` · cut from `bd0f184`
· **date** 2026-09-09

**VERDICT: ARMED. The cost is not measurable — it is smaller than the run-to-run
spread of the instrument that would have to see it, in both directions.**

---

## 1. What changed

`runtime/src/object/internal.cpp`, `RtObjectInternalRegister()`:

```c
// before
const bool table = RtOptIn("BANISHED_RT_INTERNAL_TABLE") || noDll;   // opt-IN

// after
const bool noDll       = RtOptIn("BANISHED_RT_NO_DLL");
const bool tableForced = RtOptIn("BANISHED_RT_INTERNAL_TABLE");
const bool tableKilled = !RtWaveEnabled("BANISHED_RT_INTERNAL_TABLE");
const bool table = noDll || (!tableKilled && (tableForced || !shadow));
```

Three rows move from gated to live on every ordinary run:
`??0Internal@Object@@QEAA@XZ`, `??1Internal@Object@@UEAA@XZ`,
`?ResetPersistState@InternalFactory@Object@@QEAAXAEAVStream@IO@@@Z`.
`?ClearPersistState@` was moved out of the tier by Wave 11 lane OBJINT and is
**untouched** — it stays registered exactly once, in the `if (!shadow)` block.

Two details that are not cosmetic:

* **The shadow tier still works.** The two tiers claim the same doors and are
  mutually exclusive. A naive default-on would have made
  `BANISHED_RT_INTERNAL_SHADOW=1` hit the "REFUSED … nothing registered" path and
  silently delete the very oracle that produced the evidence for arming. So an
  **explicit** arm still conflicts and is refused loudly; the **default** arm
  yields to shadow.
* **`noDll` still overrides the kill switch.** With Runtime-x64.dll absent there
  is no DLL body to fall through to and an unregistered `??0Internal` is a null
  call on the first object constructed.

## 2. The `RtOriginal` check — clean, and this is what was actually read

The brief's first-order risk is Wave 2's: `RtOriginal()` **is** `GetProcAddress`,
an export-table binary search under the loader lock, and two of them in a hot
ctor cost 5.6 points. Every door the two hot bodies reach was read, not assumed:

| helper on the hot path | door | per-call cost |
|---|---|---|
| `Factory()` | `ResolveExport` → `static g_ppFactory` | cached, once |
| `VtblInternal()` / `VtblBase()` | `ResolveExport` → statics | cached, once |
| `RandomRand()` | `ResolveExport` → `static g_rand` | cached, once |
| `MemAlloc` / `MemFree` | `RtOriginal` → `static g_memAlloc/g_memFree` | cached, once; and only reached on an array **grow**, not per call |

`RtOriginal` appears in this file **only** inside those two `if (!g_x)`
first-call initialisers and inside the shadow-tier wrappers, which are a
different tier and mutually exclusive with this one. **No `RT_DIFF` / shadow hook
sits on the tier-2 path**: the ported bodies carry two unconditional `++counter`
increments and nothing else. There was nothing to report as a finding here, and
nothing was measured around.

## 3. Perf — 6 `sa-stress` runs, interleaved, one lock hold

Medium map, 800 citizens, 3 sets, speed 10, 6000 frames, seed 31337, perf exe
(`-O2`, logs stripped). Order run was `on, off, on, off, on, off` so drift cannot
line up with the arm. Headline metric `sim ms / iteration`, per-interval mean with
the first (warm-up) interval dropped.

| arm | run 1 | run 2 | run 3 | mean | within-arm spread |
|---|---|---|---|---|---|
| **ARMED** (default) | 0.18922 | 0.19056 | 0.18589 | **0.18856** | 0.00467 |
| **KILLED** (`=0`) | 0.19244 | 0.19122 | 0.18733 | **0.19033** | 0.00511 |

```
difference of means (armed − killed) = −0.00178 ms/iter  (−0.93 %)
within-arm spread                    =  0.00467 / 0.00511 ms/iter
```

**The effect is inside the noise.** The difference between the arms is ~2.7×
*smaller* than the spread within either arm, the two sets of three runs overlap
completely (armed 0.18589–0.19056, killed 0.18733–0.19244), and the sign is
*negative* — the armed runs came out nominally faster, which no mechanism
predicts and which is exactly what a null result looks like at this sample size.

Stating it the way the brief asked for: **this measurement cannot resolve the
cost of ~125,000 calls/run through our bodies. It can only bound it, and the
bound is well inside the PLAN 5 % gate** (every individual run reported `PLAN 5%
gate (sim ms/iter): PASS`, armed and killed alike, at −3.3 % to +0.4 % against
`perf/baselines/current.csv`). Do **not** quote −0.93 % as a speed-up.

The reason a null result is plausible rather than suspicious: the two bodies are
a handful of stores plus an amortised array grow, they replace DLL bodies doing
the *same* work, and the per-call helper doors are all one-time-cached statics —
so the ported path is not doing more work than the path it replaced. The Wave 2
breach was not "our body ran", it was "our body called `GetProcAddress`", and
§2 shows this one does not.

`draw ms / frame` reads `+20.6 %` against the stored baseline in every run of
both arms. It is arm-independent, therefore not this lane's, and it is a GPU-side
metric the baseline predates by four days.

## 4. Functional verdict — the arming actually took

All runs debug build, `tools/boot_state.sh --wait 300` polled to `RUNNING`,
`standalone_crash.log` **0 bytes** in all three.

| run | env | `[RtInternalTable]` line |
|---|---|---|
| A — default | `BANISHED_AUTOSTART=1`, **no `BANISHED_RT_INTERNAL_TABLE` at all** | `ctorRuns=125618 dtorRuns=125612 clearRuns=0 resetRuns=0` — *"our ported bodies drove the LIVE handle table this many times"* |
| B — kill switch | `BANISHED_RT_INTERNAL_TABLE=0` | `ctorRuns=0 … not registered on this run`, and `9 exports registered` instead of `12` |
| C — live load | `BANISHED_AUTOLOAD=1` | `ctorRuns=127509 dtorRuns=127504 clearRuns=1 resetRuns=1` |

Run A is the proof required: **non-zero `ctorRuns` with no env var set**. Run B
is the kill switch verified **by running it**, not by reading the code — the
registration count drops by exactly the three rows and the attendance counter
returns to its "not registered" reading, so the switch disables the tier
completely rather than merely quieting it.

**Run C closes a debt this arming created and the brief did not ask for.** The
comment on `ResetPersistState` says the row was VERIFIED-BUT-UNARMED and that
what it still owed was *"a LIVE TIER-2 LOAD RUN — this tier armed, a save loaded,
our `ResetPersistState` driving the real handle table with the DLL present."*
Default-on makes that row live for everyone, so shipping the arming without that
run would have handed every lane an untested load path. `clearRuns=1 resetRuns=1`
with a clean exit is that run. It is one transaction, not a campaign — it is
evidence the path works once, not that it is now as well covered as the ctor.

**The undesigned cross-check reproduced a third time.** Ctor and dtor differ by
exactly **6** in run A (125618/125612), as they did in the shadow run
(126764/126758) and the armed run (122030/122024) that lane OBJLIFE recorded —
three runs, two tiers, three different totals, the same residue of six live
objects at exit. Run C's residue is **5**, not 6, and that is not a contradiction:
a load run ends with a different set of objects alive. The invariant that keeps
its value is *ctor − dtor is a small constant*, i.e. our bodies leak nothing.

## 5. Census set-diff — and the entry that had to move with it

Measured with `./tools/runtime_census.py` **from inside this worktree** (via the
main checkout's venv interpreter, since `.venv` is not tracked — the script
resolves `REPO` from `__file__`, so it measured *this* tree).

```
ENTERED (3):
  export  ??0Internal@Object@@QEAA@XZ
  export  ??1Internal@Object@@UEAA@XZ
  export  ?ResetPersistState@InternalFactory@Object@@QEAAXAEAVStream@IO@@@Z
LEFT (0):  —

Object   44 -> 47 ported   (72.1% -> 77.0% of called)
TOTAL   930 -> 933 ported  (80.7% -> 81.0%)
```

**The first measurement of this diff was empty, and the empty answer was the
tool's, not the tree's.** `tools/runtime_census.py` carried
`GATED_OFF_BLOCKS = {"runtime/src/object/internal.cpp": ("table", "shadow",
"checkPtr")}` — a hardcoded list of gate *variable names* whose `if (<var>) {…}`
bodies it refuses to count. The registrations are still inside `if (table) {…}`;
only the *meaning* of `table` changed, and no amount of reading the source tells
the script that. So the census read **identical** before and after the arming.

That list exists to stop an unarmed row being counted as a port. Left as it was,
it would have done the exact opposite here — refused to count an **armed** one —
which is this project's own inverted failure again
(`reference_census_counted_gated_and_missed_dx11.md`). `"table"` is removed;
`"shadow"` and `"checkPtr"` stay, both still genuinely opt-in. The removal is
recorded here rather than made quietly, because a tools edit that moves a
headline number in the same commit as the change it measures is exactly the shape
that should be stated out loud.

## 6. Contradicting the brief

**`make standalone-perf` was already broken at `bd0f184`, before this lane touched
anything.** Six files call `std::printf`, and `standalone/log_strip.h` — which the
perf target force-includes — defines `printf(...)` as a macro and documents in its
own header comment that *"the preprocessor can't replace qualified names like
`std::printf`, so all call sites must use unqualified `printf`."* Every
`std::printf` in a perf build is therefore a hard compile error. Verified as
pre-existing by stashing this lane's changes and rebuilding: same 34 errors.

Fixed here (`std::printf` → `printf`, mechanical, 43 sites across
`runtime/src/object/external_compiled.cpp`, `external_path_stack.cpp`,
`external_path_stack_probe.cpp`, `internalfactory_persist_probe.cpp`,
`runtime/src/ui/dialog_factory.cpp`) because the perf verdict is this lane's
deliverable and could not be produced without it. It is unrelated to the arming
and should be read as a separate repair riding along.

The consequence worth flagging: **`sa-stress` has not been runnable since those
files landed**, so any Wave 11 statement of the form "no perf regression" that
predates this commit was not measured on the perf exe.

## 7. What this does not establish

* The perf runs are on the **stripped** perf exe, where the attendance printf is
  compiled out — so those six runs carry no `ctorRuns` readout. The arming is
  env-gated, not build-gated, and was proven in the debug build (§4); the perf A/B
  is a timing comparison, not a second attendance proof.
* Three runs per arm bounds the cost; it does not measure it. A real point
  estimate for an effect this small needs a different instrument than
  `sa-stress` (the sampling profiler, `BANISHED_SAMPLE`), not more runs of this one.
* `resetRuns=1` is one load transaction. The row is now live for every lane on
  every load, and that is a larger exposure than one run tests.
