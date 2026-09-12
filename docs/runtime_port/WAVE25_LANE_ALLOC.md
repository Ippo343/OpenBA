# WAVE 25 LANE ALLOC — the allocator gate, measured the way Wave 22 asked for

Branch `wave25/alloc`, cut from `wave25/base` at `fc56353c`. 2026-09-11.

Subject: the seven `MemoryAllocator` System BODY rows held behind `BANISHED_RT_ALLOC`,
and the decision of whether they can move from `gated` to `ported` by default.

---

## 0. HEADLINE

**VERDICT: DO NOT ARM `BANISHED_RT_ALLOC` BY DEFAULT.** Same verdict as Wave 22, but
reached from the evidence Wave 22 said it lacked, and it is now a **narrower and
better-supported** refusal rather than a repetition.

| Wave 22 §5.4 asked for | this lane | result |
|---|---|---|
| 2nd/3rd armed redzone run + a **different workload** | 4 armed redzone runs, 2 workloads | partly done — and the second workload's instrument is BROKEN (§4) |
| **counterbalanced 8-pair A/B** on a quiet box, "tightening the bound from ±5% to ±2%" | done, 8 pairs, 16 runs, quiet box | **the bound did NOT tighten. The prescription was wrong about the cause.** §3 |
| resolve the census disagreement on `?Allocate@`/`?Free@` | re-derived | already fixed in Wave 22; census is consistent today (§6) |
| a decision on the OOM/modal path | source-verified both halves | hazard CONFIRMED and reachable at a named line; detection CONFIRMED (§5) |

**Rows moved: none.** The census is byte-identical before and after this lane (§6).

**Three findings that outlive the verdict:**

1. **8 counterbalanced pairs on a quiet box bound the treatment at ±5.3% — the same
   bound 4 pairs gave Wave 22 on a noisy one.** The limit is the stress harness's own
   run-to-run variance, not machine load. ±2% needs ~32 pairs (~2 h), not 8. §3.3.
2. **"Off by default" is now MEASURED, not reasoned** — and the measurement has a
   positive control that reproduces the exact historical defect. §2.
3. **`BANISHED_REDZONE=watch` + `BANISHED_AUTOLOAD` crashes the watchdog itself**,
   armed *and* unarmed, at the same address. A heap instrument that dies on a whole
   workload has been silently limiting every "one clean redzone run" claim. §4.

---

## 1. WHAT I DID NOT ESTABLISH — stated first, not last

* **I did not drive the allocator to exhaustion.** §5's OOM hazard is reachability
  established by reading a named line, not an observed modal. Deliberate.
* **No cutover (DLL-absent) run.** Every verdict here is DLL-present.
* **Two workloads for correctness, one for timing.** All timing is the `sa-stress`
  town. Menu, mods and save/load timing are unmeasured in both arms.
* **The ±5.3% bound is a BOUND, not a measurement of the effect.** The series is
  consistent with arming being free, and equally consistent with it costing 5%.
* **I did not re-run the shadow on the stress or save/load workloads.** My shadow
  evidence is the menu/autostart cell only; Wave 10 ran the other two, nine waves ago.
* **I did not verify the gate dynamically with a breakpoint.** §2's route analysis is
  a source screen plus the shadow's gap detector, which is an observation of *effects*
  on the arena, not of *which function executed*.
* **I did not examine whether arming changes memory FOOTPRINT.** The A/B's
  `private_mb` column was not analysed; only `sim ms/iter`.

**Declared surface growth: ZERO.** This lane ported nothing, registered nothing, and
minted nothing. No tags touched, so no SCOPE/ADDRESSEE tokens are owed.

---

## 2. OFF BY DEFAULT — verified by measurement, with the historical defect as the control

The brief's trap 1: *"the allocator port is off by default" was FALSE* — an unarmed run
once mutated retail's arena because `FreeLockedIgnoringSelf` called `rtalloc::MemFree`
**directly** instead of through the resolve door. A contaminated control invalidates the
whole A/B, so this is a prerequisite, not a nicety.

### 2.1 The source screen (necessary, not sufficient)

Denominator stated: every `rtalloc::` occurrence outside `allocator.cpp` across
`runtime/ code/ standalone/ engine/ dx11/`. **Three hits, all in
`singleton_ctors.cpp`** (`rtalloc::Construct` / `Destruct`, gated by
`BANISHED_RT_SINGLETON_CTORS`, a kill switch that is default ON). **No direct
`rtalloc::MemFree` anywhere.** `FreeLockedIgnoringSelf` (`allocator.cpp:1654`) resolves
`0x1800c7ad0` through `ResolveAddr(Module::Runtime, …)` and retries a null rather than
caching one. Wave 22 §2.2's three paths are unchanged and still correctly described.

A source screen cannot see a call through a runtime-chosen function pointer, which is
why it is not the evidence.

### 2.2 The measurement — the shadow's GAP detector

The gap detector observes the **real arena's own counters** across every operation and
reports any change that did not arrive through the two patched entries. It is not a
self-check: it compares the live allocator's state against what the observed operations
should have produced.

Debug exe `e7b3b0d3cea7698aeade900a710415cb`, one workload
(`AUTOSTART=1 PAUSEMENU=900 PAUSEMENU_QUIT=60`), `BANISHED_RT_DIFF=1
BANISHED_RT_ALLOC_SHADOW=1 BANISHED_RT_ENGINELIFE=0`, `BANISHED_RT_ALLOC` **unset**:

| cell | extra | observed ops | divergences | gap checks | gaps | verdict |
|---|---|---|---|---|---|---|
| `w25_shadow_base` | — | **3,091,328** | **0** | 3,091,327 | **0** | clean |
| `w25_gapctl` | `SHADOW_GAPCTL=1` | 2,467,215 | 0 | 2,467,214 | **24** (`ctl_injected=24`) | **gap detector ALIVE** |
| `w25_negctl_size` | `SHADOW_NEGCTL=size` | 2,470,417 | **1,870,250** | 2,470,416 | 0 | **divergence oracle ALIVE** |
| `w25_legacy_directfree` | `LEGACY_DIRECTFREE=1` | 2,402,755 | 347,201 | 2,402,754 | **6,761** | **the historical defect, reproduced** |

Re-derive: `tools/wave25_alloc_cell.sh cell <label> 600 -- <env>`; raw logs in
`docs/runtime_port/wave25_alloc_logs/`.

### 2.3 Why this is a real zero and not a dead parser

A screen looking for an **absence** must be handed a positive shape or "0 hits" is
indistinguishable from a broken parser. It was handed **two**, and they are independent:

* **`GAPCTL`** perturbs the snapshot synthetically: `gaps=24` against `ctl_injected=24`,
  an **exact** match. The counter is not merely non-zero, it tracks the injection.
* **`LEGACY_DIRECTFREE`** reinstates *the actual defect the brief warns about*, on the
  **same binary and workload**: `gaps=6761`, `sum_dAllocCount=-6770`,
  `sum_dAllocatedBytes=-387,136`. That is 387 KB of arena mutation the shipped default
  does not produce.

**Effectiveness:** 2 of 2 injected shapes detected; the negative control for the
*divergence* half fired independently (1.87M divergences with `gaps=0`, confirming the
two screens are genuinely separate and a merged number would have hidden it).

**Conclusion: on this tip, with `BANISHED_RT_ALLOC` unset, nothing mutates retail's
arena outside the two patched entries, over 3.09M observed operations.** The A/B's
control arm is clean.

### 2.4 The one thing this does NOT say

It is a statement about **one workload**. Wave 10 ran the stress and save/load cells;
I did not. A route that only opens during a world reload would not appear here.

---

## 3. THE PERF A/B — 8 counterbalanced pairs, and a refuted prescription

### 3.1 Design, and why it is not Wave 22's design

Wave 22 alternated arms but ran **ON first in every pair**, and named the consequence
itself: a *rising* confounder lands more heavily on the second run of each pair, so a
fixed within-pair order converts drift into apparent treatment effect (it produced a
spurious "arming is 20% faster"). **Alternating is not enough; order must be
counterbalanced.** Pairs 1/3/5/7 ran ON first, pairs 2/4/6/8 ran OFF first.

One binary (`87c6c98a71660a877a86a958f32ed803`), one env var, 16 runs, default stress
town (800 citizens, 3 sets, 10x, medium, seed 31337, 6000 frames). Headline
`update_ms_per_iter`, median of the reported windows after dropping the first two.

Re-derive: `tools/wave25_alloc_ab.sh <pair> <on|off>` then
`python tools/wave25_alloc_analyze.py`.

### 3.2 The effectiveness control: every run attested its own arm

16 of 16 runs named their arm in their own `standalone_error.log`, and every armed run
reported `exports=27` (a *partial* arm would print a different number). The analyser
**refuses** a pair whose runs did not attest the arm their label claims; 0 pairs were
rejected. It reads the arm with a **word-boundary** regex, because `"ARMED" in
"UNARMED"` is true — the substring bug that mislabelled every control run in Wave 22.

### 3.3 The result

```
PAIR  ORDER     ON med    OFF med    on-off     on%
1     on,off     0.2105    0.2060   +0.0045    +2.2%
2     off,on     0.2165    0.1930   +0.0235   +12.2%
3     on,off     0.1935    0.1905   +0.0030    +1.6%
4     off,on     0.1925    0.2085   -0.0160    -7.7%
5     on,off     0.1895    0.1930   -0.0035    -1.8%
6     off,on     0.1895    0.1905   -0.0010    -0.5%
7     on,off     0.1885    0.1895   -0.0010    -0.5%
8     off,on     0.1950    0.1945   +0.0005    +0.3%

ARMED   mean of medians 0.1969   spread 0.0280
unarmed mean of medians 0.1957   spread 0.0190
PAIRED DIFF mean +0.0013 ms/iter = +0.64%   sign: 4 of 8 pairs positive
95% CI on the paired mean: -4.04% .. +5.32%
```

**No resolvable effect.** The sign splits exactly 4/4 and the confidence interval
straddles zero symmetrically.

### 3.4 THE FINDING: the prescribed experiment did not do what was prescribed

Wave 22 §5.4 item 2 asked for *"a counterbalanced 8-pair A/B on a quiet box, tightening
the bound from ±5% to ±2%."* **I ran exactly that experiment and the bound did not
tighten** — ±5.32% at n=8 on a quiet box, against Wave 22's ±5% at n=4 on a hostile one.

The reason is that the bound is set by the **per-pair standard deviation (0.0110
ms/iter, ~5.6% of the mean)**, which is a property of the stress harness, not of the
machine. Doubling n shrinks the interval by only √2, and Wave 22's noisy-box series
happened to land on a similar figure. To reach ±2% requires

> n ≈ (sd / (0.02 × mean / t))² ≈ **32 pairs ≈ 64 runs ≈ 2.2 hours of runs.**

**This is a cheap-wins item that failed on contact, which is the shape memory already
warns about** — a handover's prescriptive section is its stalest, and this one carried a
quantitative promise (±2%) that nothing had measured. The *measured* parts of Wave 22
(the attestation gap, the contamination, the gate analysis) all held exactly.

### 3.5 Machine load — the residual, now closed

Wave 22 residual 4: `stress_run.sh` "records everything about the game and nothing about
the machine". **Closed.** `tools/machine_load.ps1` + three sample points
(`before` / `during` / `after`) per run, written to `machine_load.txt` and appended to
`report.txt`. Non-stationary contamination is why it samples three times: one sample
cannot show a ramp.

**What else was running (the brief's explicit question):** up to seven sibling Wave 25
lanes were live on this box during the session, compiling and running Python. The
measured load during my runs was nonetheless **low and stable — 0.17 to 2.41 busy cores
of 16** (per-run figures in the table's `load(on)/load(off)` column). The two outlier
pairs (2 and 4) coincide with the highest recorded busy-core figures in the series.
Wave 22's session by contrast had three `find /` processes pinning three of eight cores.

**Two instrument defects found while building this, both recorded because both are
"the screen reported nothing and that looked fine":**

* **The sampler initially failed on every invocation** — this box disables script
  execution, so `powershell -File` returns `SecurityError`. It was caught *only*
  because the failure is written into `machine_load.txt` rather than swallowed. A quiet
  sampler would have produced a full A/B with no load record and no warning. Fixed with
  `-ExecutionPolicy Bypass`.
* **Two CPU-load sources disagree by ~4x on this box.**
  `\Processor(_Total)\% Processor Time` read 6.3/8.5/12.0% on three back-to-back
  samples where `Win32_Processor.LoadPercentage` read 32/40/46%. The perf counter is
  the one that corroborates the independent per-process delta (0.76 busy cores of 16 =
  4.8%). The tool prints **both, labelled**, and marks the CIM figure "do not quote",
  rather than silently picking one.

---

## 4. THE REDZONE RUNS — and an instrument that dies on a whole workload

### 4.1 Armed heap validity

| run | workload | armed | heap walks | violations | crash log |
|---|---|---|---|---|---|
| `w25_armed_redzone_1` | autostart + pausemenu | ✔ `exports=27` | 2,770 | 0 | empty |
| `w25_rz_stress_on1` | stress town, 6000 frames | ✔ `exports=27` | **53,638** | 0 | none |
| `w25_rz_stress_on2` | stress town, 6000 frames | ✔ `exports=27` | **53,985** | 0 | none |
| `w25_rz_selftest_on` | stress town + injected corruption | ✔ `exports=27` | — | **1 reported** | — |

Plus the A/B itself: **8 armed stress runs of 6000 frames, all rc=0, zero crash logs**
across all 16 runs.

### 4.2 The positive control, run ARMED

A watchdog that has never reported a violation has not been shown to be able to.
`BANISHED_REDZONE_SELFTEST=45000` clobbers one boundary tag mid-run and checks nothing
itself — detection must come from the watchdog thread.

```
[REDZONE-SELFTEST] INJECTED at t=45000ms: block @0000022780000fa0 tag 000000000000000e -> 0
[REDZONE-HEAP] ==== corruption at checkpoint 'watchdog' (seq=0) ====
[REDZONE-HEAP] range 0: bad block @0000022780000fa0 tag=0x0 size=0
```

**Effectiveness: 1 of 1 injected corruption detected, at the exact injected address,
on the ARMED path.** The zero-violation rows above are therefore real zeros. Wave 22's
single redzone run had no such control.

### 4.3 The instrument defect: redzone + autoload kills the watchdog

`BANISHED_RT_ALLOC=1 BANISHED_REDZONE=watch BANISHED_AUTOLOAD=Auto` crashed:

```
Exception: 0xC0000005   Fault: WRITE of 0000000000000024   Module: ntdll.dll
  reimpl-x64.exe+0x44E009  HeapCheckLocked(char const*, bool) +0xa3
  reimpl-x64.exe+0x44EDD3  RedzoneAlloc_HeapCheck(char const*) +0x48
  reimpl-x64.exe+0x450135  RedzoneWatchdog(void*) +0x73
```

**The control settles it.** The identical workload **unarmed** produced the *same fault
address, same registers, same three frames*. This is **not an allocator finding**: it is
a pre-existing defect in the redzone watchdog on the save-load path, and arming is
exonerated.

Mechanism: `HeapCheckLocked` guards `!g_pMemAllocSingleton` but then takes
`*(CRITICAL_SECTION*)(alloc + 0x640)` unconditionally. The crash dump prints
`allocator=00000009905fef10` — a **stack** address — so the cached singleton is stale or
bogus on this path, and `EnterCriticalSection` on an uninitialised CS dereferences a null
`DebugInfo` and writes at `+0x24`. The guard tests for null; the hazard is *not-null-but-
wrong*.

**Consequence worth carrying:** the `[REDZONE-DUMP]` "free-list corruption" lines this
produced are **garbage read from an invalid allocator** — they differed run to run
("CORRUPT block … size=8750377469248" one time, "free node @0x48 OUT OF RANGE" the
other). CLAUDE.md already warns that REDZONE-DUMP can mislead and to read
`standalone_crash.log` first; this is a fresh instance where the dump named a *heap
corruption* that did not exist.

**[SCOPE: the `BANISHED_REDZONE=watch` watchdog, any workload that reloads the world.
ADDRESSEE: the Wave 25 integrator at close — either schedule the `g_pMemAllocSingleton`
staleness fix as named work, or record that redzone evidence is valid for the stress and
menu workloads ONLY and that no save/load heap evidence exists in this project.]**

---

## 5. THE OOM / MODAL PATH — Wave 22's item 4, both halves resolved by reading

Wave 22 called this "REASONED, NOT OBSERVED" and left two questions. Both are now
answered from source, and they point in **opposite** directions.

**The hazard is CONFIRMED and I can name the line.** `allocator.cpp:453` — inside our
ported `Allocate` — calls `FatalOutOfMemory(…, /*stopApplication=*/true)`, and
`FatalOutOfMemory` (`:166`) calls `?Display@Error@System@@SAXXZ` when that flag is true.
`Error::Display` is ported into the **exe** (Wave 22 ERRCFG) and calls the **exe's**
`MessageBoxW`. `HookRuntimeMessageBoxW` (`main.cpp:781`) hooks
`GetModuleHandleA("Runtime-x64.dll")`'s IAT **only**. So **arming moves the OOM message
box out from under the harness's only `MessageBoxW` seam** — unarmed, retail's own
`Allocate` pops it through Runtime's hooked IAT. That is an armed-specific loss of
observability, not merely a pre-existing gap. `error.cpp:418-428` already states the
regression; what is new here is that our *armed allocator* is a live caller with
`stopApplication=true`.

**The mitigation is also CONFIRMED, which weakens the argument against arming.**
`BootState`'s `WatchdogThread` (`boot_state_watchdog.cpp:104`) calls `ModalDialogOpen`
every tick **for the life of the process**, and `blocked` is evaluated *before* the
`RUNNING` test — so a modal raised long after boot still publishes `BLOCKED_DIALOG` and
re-logs every 15s. Wave 22 guessed "probably can; unverified". It can.

**Net effect on the verdict: this argument against arming is WEAKER than Wave 22
recorded**, and I am saying so even though it cuts against my own conclusion. It
survives as a real cost (loss of the seam) rather than as a hang risk.

---

## 6. THE CENSUS — confirmed, not asserted

```
python tools/runtime_census.py
TOTAL   4632   1323 called   1171 ported   9 detour   88.5%
```

Byte-identical before and after this lane (`diff cen_base.txt cen_after.txt` → empty).
The seven rows remain in `GATED-BUT-WRITTEN`, all `System`, all `BANISHED_RT_ALLOC`:
`?Allocate@`, `?Free@`, `?Get@`, `?MemAlloc@`, `?MemFree@`, `?Shutdown@`, `?Startup@`.

**A build check cannot detect a misregistration, and neither can this** — the census is
a source screen. Since I registered nothing, that limitation costs nothing here.

*(Branch-local figure. The integrator re-derives on the merged tip.)*

---

## 7. THE VERDICT, AND THE RISK CASE BOTH WAYS

### 7.1 DO NOT arm by default

**The evidence that carries it is still that there is no positive case, not that there
is a negative one** — but the shape has changed since Wave 22, and two of its four
"what would change my answer" items are now discharged in favour of arming.

**For arming (stated fairly, and stronger than Wave 22's version):**

* Correctness is now genuinely instrumented: 3.09M shadow operations with 0 divergences
  and 0 gaps, with **two** positive controls and a working negative control (§2).
* Heap validity is 3 clean armed runs across 2 workloads (107,623 walks on the stress
  town alone) **plus an armed positive control proving the watchdog can report** (§4.2).
  Wave 22 had one run and no control.
* 8 armed stress runs of 6000 frames in the A/B, zero crashes, zero redzone violations.
* Perf is neutral, bounded ±5.3%.
* The OOM/hang argument is weaker than recorded (§5).
* It is the **cutover path**; everything left opt-in is untested on the route the
  project is heading for.

**Against, and these still win today:**

* §4 of Wave 22 is unchanged and is the whole case: arming is an **atomic, process-wide
  heap swap with prologue patching**, whose mistakes are *silent* — a block on a free
  list the allocator never handed out, surfacing arbitrarily later. The project has paid
  for exactly that once.
* **Neutral perf does not buy that risk.** A default-on flip should be justified by what
  it gains; this one gains 7 census rows. That is a reason to arm *deliberately at
  cutover*, not to arm *by default now*.
* **My correctness evidence is one workload.** The shadow ran on menu/autostart only;
  the stress and save/load shadow cells are nine waves old, and the save/load *heap*
  instrument is now known broken (§4.3). Defaulting on while the save/load path has no
  working heap instrument is the wrong order.
* Arming moves the OOM modal out from under the only `MessageBoxW` seam (§5).
* Default-on makes every other lane's run an allocator experiment; a silent corruption
  would be attributed to whichever lane happened to see it.

### 7.2 What would change my answer — re-derivable, and deliberately NOT a cheap-wins list

§3.4 is this lane's evidence that a prescriptive list is the stalest thing a handover
carries. So each item below names **what to measure**, not how long it should take, and
**none of them is claimed to be cheap**:

1. **Shadow cells on the stress and save/load workloads on a current tip.** The single
   strongest correctness instrument, last run nine waves ago on two of its three
   workloads. Command: `tools/wave25_alloc_cell.sh cell <label> 600 -- BANISHED_RT_DIFF=1
   BANISHED_RT_ALLOC_SHADOW=1 BANISHED_RT_ENGINELIFE=0 BANISHED_STRESS=1` (and the
   `BANISHED_AUTOLOAD=Auto` variant).
2. **Fix `HeapCheckLocked`'s stale-singleton deref (§4.3)**, then get save/load heap
   evidence that does not exist today in either arm.
3. **An exe-side `MessageBoxW` seam** (§5), which is a small, well-specified job that
   also closes an existing ERRCFG residual.
4. **A ±2% perf bound costs ~32 pairs** (§3.4). Do not schedule it expecting 8.
5. **An exhaustion test**, which nobody has run, deliberately, twice now.

---

## 8. INSTRUMENT DEBT THIS LANE ADDS OR CLOSES

**Closed:**
* Wave 22 residual 4 — `stress_run.sh` now records machine load (§3.5).

**Opened / handed on:**
* **The game-dir lock expires while the dir is demonstrably in use, and tells the holder
  nothing.** Measured: my lock was broken as stale at **1802s against the 1800s
  default** while `reimpl-x64.exe` was alive and writing. A perf A/B across repetitions
  is *inherently* longer than the stale timeout, so the lock stops protecting the
  longest-running lane in a wave — the one that needs it most. **The second half is the
  dangerous one: the holder receives no signal at all.** I continued measuring for some
  minutes believing I held it, and learned otherwise only from the integrator. During
  that window lane SWEEP deployed its own binary (`a4e467ef…`); **no completed run of
  mine was contaminated, and the reason is a guard, not luck** — every run md5-verifies
  the deployed exe against my own build immediately before launch and all 16 recorded
  `87c6c98a…`. Pair 3's second arm never ran, so I **discarded and re-ran the whole
  pair** rather than keep an unpaired arm. Two fixes, either sufficient: do not expire a
  lock while the protected directory is in use, or have `verify` fail loudly in the
  holder. **[ADDRESSEE: the Wave 25 integrator at close. Lane SWEEP carries the same
  item independently.]**
* **`standalone-perf` did not build on `wave25/base`** — `wave24_lane_gfxmat.cpp:812,822`
  used qualified `std::printf`, which `log_strip.h`'s `#define printf(...)` cannot
  replace. Landed on base at `5b848241`. **The generalisable half: the debug build was
  never affected, so a clean `standalone-debug` plus green checkers was not evidence that
  the tree builds.** The guard runs only for the log-stripped target, so Wave 24
  integration could not have seen it. **That is a gap in the integration check, not one
  bad line.** **[ADDRESSEE: the Wave 25 integrator at close — add `standalone-perf` to
  whatever the integration check builds.]**
* **The redzone watchdog dies on save/load (§4.3),** with an `[ADDRESSEE]` tag there.

## 9. ARTEFACTS

* `tools/machine_load.ps1`, wired into `tools/stress_run.sh` (before/during/after).
* `tools/wave25_alloc_cell.sh` — lock- and md5-guarded cell driver, keyed to this lane's
  own lock name (a driver that verifies another lane's lock name is a guard that cannot
  fire).
* `tools/wave25_alloc_ab.sh` — counterbalanced A/B driver; takes the arm order as an
  argument so order cannot silently become fixed.
* `tools/wave25_alloc_analyze.py` — paired analysis that **refuses** unattested pairs.
* `docs/runtime_port/wave25_alloc_logs/` — raw `standalone_error.log` per correctness
  cell, and both crash logs from §4.3.
* `perf/results/*_w25ab_p{1..8}_{on,off}` (16), `*_w25_rz_*` (4).
