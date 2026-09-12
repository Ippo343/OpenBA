# WAVE 22 LANE ALLOC — the allocator gate, and the two ways the number could have been wrong

Branch `wave22/alloc`, cut from `wave22/base` at `37706651`. 2026-09-11.

Subject: the five `MemoryAllocator` BODY rows and the one `_singleton` DATA row, plus
the wave's instrument question (§6).

---

## 0. HEADLINE — the plan held; the instruments did not

**WAVE22_PLAN.md §1 is correct, line number for line number.** Its five registrations are
where it says they are and the gate is what it says it is. A lane whose first task is "go
and check whether the plan is stale" reporting "it is not" is a short §1, and §1 is short.

Everything else this lane produced is about **how confidently wrong the answer could have
been**, and there are two independent mechanisms, either of which alone would have
produced a green A/B from a measurement that never applied its treatment:

1. **The perf build cannot report whether the allocator armed.** Every `[RtAlloc]` line
   stating the outcome of arming is a `printf`, and `standalone-perf` compiles `printf` to
   `((void)0)`. The one binary an A/B is measured on was precisely the binary that could
   not say whether it had armed. A silent REFUSAL — missing platform seam, missing detour
   installer, foreign prologue patch, all of which `RtSystemAllocatorRegister` has
   refusal paths for — would have made the A/B an **A/A that reads as "no regression"**.
   Fixed: both arms now attest on `stderr`, which survives the strip.

2. **The confounder was more than twice the size of the effect and invisible from inside
   the harness.** My first three runs read 0.21 / 0.46 / 0.46 on one binary. Three
   sibling-lane `find / -iname <memory file>` processes had pinned three of eight cores.
   The harness reports FPS, update ms, memory and phase splits, and **nothing about the
   machine it is running on**.

The recorded precedent for this subject is that one `sa-stress` run cannot resolve a
5-point effect. This lane is a second, sharper instance: the treatment was ~1% and the
contamination was ~130%.

**Recommendation: `BANISHED_RT_ALLOC` should NOT become default-on yet.** §5.

**Rows moved: none promoted; two DEMOTED and one promoted, all by correcting the
instrument rather than by porting anything** (§6.2.3). The census was wrong in both
directions about this lane's own subject.

---

## 1. TASK 1 — re-deriving WAVE22_PLAN §1

Derived on my worktree tip, not inherited.

| plan claims | found | verdict |
|---|---|---|
| `allocator.cpp:1433` `?MemAlloc@` | 1433 | exact |
| `:1435` `?MemFree@` | 1435 | exact |
| `:1439` `?Get@` | 1439 | exact |
| `:1463` `?Startup@` | 1463 | exact |
| `:1465` `?Shutdown@` | 1465 | exact |
| "that Register function returns 0 unless `BANISHED_RT_ALLOC` is armed" | `rt_system_port.cpp:113-116`, opt-in, **absent env means OFF** | holds |
| "26 registrations in that file, plus 6 in `allocator_win.cpp`" | holds, but the census's gated-registration count was wrong in BOTH directions — §6.2 | **the plan quoted a broken instrument faithfully** |

All five bodies are written, registered, and unarmed. **The plan is not stale on its
headline.**

### 1.1 But the sixth row is NOT in the same boat, and the plan reads as if it were

WAVE22_PLAN §1's table puts `?_singleton@MemoryAllocator@System@@0PEAV12@EA` in the same
cluster as the five bodies — "5 BODY + 1 DATA … **ARM AND MEASURE**". That is wrong in
kind. `BANISHED_RT_ALLOC` has **nothing to do with** that row:

* it is a **minted DATA row** (`standalone/rt_data_mint.cpp`), gated by the mint's own
  switches, and deliberately **excluded** from `standalone/reg_mint_promoted.cpp`;
* arming `BANISHED_RT_ALLOC` neither registers it nor moves it;
* arming the thing that *would* move it (`BANISHED_RT_MINT=1`) is the **known-fatal**
  route with the DLL present.

So the honest shape of the ALLOC subject is **5 rows that arming moves, and 1 that it does
not touch at all.** The census now prints exactly that (§6.1): the gated-but-written table
lists five System BODY rows and not the DATA row. My verdict on it is §5.3.

---

## 2. TASK 2 — is the gate a real gate?

**Denominator, stated:** every occurrence of `rtalloc::` outside `allocator.cpp` in the
tree (19 hits, 17 of them in comments), plus every `namespace rtalloc` block (3 files),
plus every registrar of the six subject symbols and of `?Allocate@`/`?Free@`, plus the
two callers of `RtSystemAllocatorRegister`. I did **not** verify this dynamically with a
breakpoint or a counter; it is a source screen, and a source screen cannot see a call
through a function pointer whose target is chosen at runtime.

### 2.1 The Wave 10 bypass is fixed, and fixed the right way

`FreeLockedIgnoringSelf` (the unconditional owner of `0x180009c30`) used to call
`rtalloc::MemFree` **directly**, so an unarmed run mutated retail's arena with our free
logic. Today (`allocator.cpp:1615-1620`) it resolves `0x1800c7ad0` through the door and
retries a null rather than caching one. The old defect is preserved as a **negative
control** behind `BANISHED_RT_ALLOC_LEGACY_DIRECTFREE=1`, which is the right shape: a
one-binary, one-env-var A/B instead of a comparison of two builds.

### 2.2 Three paths reach ported allocator code WITHOUT consulting `BANISHED_RT_ALLOC`

None of them is my subject and none of them is a defect, but "the allocator port is off by
default" is still not literally true and that should be written down rather than
rediscovered:

| path | gate it actually consults | what runs |
|---|---|---|
| `??0/??1MemoryAllocator@System@@` → `rtalloc::Construct` / `Destruct` | `BANISHED_RT_SINGLETON_CTORS`, a **kill switch (default ON)** | our ctor/dtor, on every run |
| `RtLaneXEnsureAllocatorPlatform()` → `RtSystemAllocatorInstallPlatform()` | none — called unconditionally from `RtEngineLifecycleRegister` | fills `g_plat` |
| `0x180009c30` → `FreeLockedIgnoringSelf` | none — `rt_system_port.cpp:84`, outside every wave switch | a shim that forwards through the door |

The first one matters because of a **stale comment**. `singleton_ctors.cpp:1110` argues the
fourteen ctor registrations are safe because *"our tree has NO call site for any of these
fourteen names — the callers are Engine::Engine and Engine::Startup inside the DLL"*.
That is **no longer true for this row**: we ported `Engine::Engine`, and
`engine_lifecycle.cpp:390` is
`RT_EXPORT_ONCE(CtorFn, "??0MemoryAllocator@System@@QEAA@XZ")(&self->_allocator);` —
a call site in our tree, reaching our body, on every default run.

It is harmless (the body is byte-faithful, and `RtEngineLifecycleRegister` refuses to
register at all unless it can install the platform seam first, precisely so this cannot be
an EXEC-of-0). But the *reason* it is safe is not the reason the comment gives.

**Corrected in place**, with the date and the evidence, rather than deleted — the
conclusion the paragraph supports ("safe to land the two unprobed ones") still holds; its
justification does not. The consequence worth carrying forward: *"the allocator port is
off unless `BANISHED_RT_ALLOC` is set" is a claim about the registration door for the five
`MemoryAllocator` statics, not about every line of `rtalloc::`.* Two of its functions run
unarmed on every boot.

**Also checked and clear:** the unqualified `MemAlloc`/`MemFree` in `engine_lifecycle.cpp`
and in the four `*_internal.h` headers are **not** `rtalloc::` — they are `RT_EXPORT_ONCE`
door wrappers. No `using namespace rtalloc` anywhere.

**What I did NOT check:** whether any *DLL-internal* path reaches our bodies unarmed. It
cannot through the registry (nothing is registered), and the detour is not installed
unarmed — but I verified that by reading the arming path, not by instrumenting a run.

### 2.3 Verdict

**The gate is a real gate for all six subject rows.** Armed and unarmed runs are
distinguishable from their own artefacts (§3.1), and no subject row's body executes
unarmed.

---

## 3. TASK 3 — the A/B

### 3.1 The effectiveness control, shown rather than asserted

One binary (`md5 2d4ee2c559f7990b69f1e6f3d93e7ce1`), one env var. Every run in the
reported series named its own arm in `standalone_error.log`:

```
[RtAlloc] ATTEST: ARMED (atomic swap, DLL present) (exports=27)
[RtAlloc] ATTEST: UNARMED (BANISHED_RT_ALLOC=(unset)) -- the allocator port registered NOTHING
```

8 of 8 reported runs attested, and each attestation matched its intended arm. The armed
line also carries `exports=27`, so a *partial* arm would be visible as a different number.

This is not decoration. It caught a live mislabelling immediately: my run summariser
buckets the `redzone_on` run as `unarmed` from its label while the attestation reads
`ARMED`. The label was wrong; the binary was right.

### 3.2 What the measurement is

`sa-stress` default town (800 citizens, 3 sets, 10x, medium, seed 31337, 6000 frames).
Headline is `update_ms_per_iter`. Per run I take the **median of the 8 reported windows
after dropping the first two** (warm-up), per `perf/README.md`. Runs alternated
ARMED/unarmed in adjacent pairs.

### 3.3 The clean series — 4 pairs, 8 runs

Taken in a quiet window (load ~9%, no sibling builds or searches).

| pair | ARMED median | unarmed median | on − off |
|---|---|---|---|
| r1 | 0.212 | 0.202 | **+0.010** |
| r2 | 0.196 | 0.217 | **−0.021** |
| r3 | 0.200 | 0.197 | **+0.003** |
| r4 | 0.195 | 0.196 | **−0.001** |

```
ARMED    n=4   mean of medians 0.201   min 0.195   max 0.212   spread 0.017
unarmed  n=4   mean of medians 0.203   min 0.196   max 0.217   spread 0.021
paired difference (on - off): mean -0.002, and it changes SIGN twice in four pairs
```

**Result: no resolvable effect.** The mean difference is −0.002 ms/iter (−1.1%) against a
within-arm spread of ~0.02 — the noise is roughly **ten times** the treatment. What four
pairs support is a *bound*: arming does not cost or save more than about ±5% of sim time.
It does **not** support "arming is free to within 5%", which is the claim a 5-point gate
would need, and I am not making it.

Absolute levels here (~0.20) sit slightly above the committed 0.19 baseline in **both**
arms, which is a property of this machine/session, not of the treatment.

### 3.4 Heap validity

One armed run with `BANISHED_REDZONE=watch`
(`perf/results/20260911_201519_alloc_redzone_on/`):

```
[RtAlloc] ATTEST: ARMED (atomic swap, DLL present) (exports=27)
[REDZONE] WATCH-ONLY mode: no shim, watchdog=1ms
[REDZONE-HEAP] watchdog thread started (interval=1ms)
[REDZONE-HEAP] watchdog stopped at exit (acknowledged) after 53905 completed heap walks
```

**53,905 completed heap walks, zero violations, clean self-quit, no `standalone_crash.log`.**
This run's timing (0.33) is not a perf datum — it carries both the watchdog's cost and a
sibling build's load.

### 3.5 The runs I am throwing away, and why you cannot salvage them

Quarantined under `perf/results/_contaminated_findload/` (3 runs) and
`_contaminated_siblingbuild/` (7 runs). They are kept rather than deleted because the
contamination is the finding.

* **`_contaminated_findload`** — three sibling `find / -iname <memory-file>` processes,
  ~4,500 CPU-seconds each, still consuming a core apiece when measured (5.02s CPU per 5s
  wall, each). Runs read 0.21 / 0.46 / 0.46 on one binary.
* **`_contaminated_siblingbuild`** — a sibling `mingw32-make standalone-debug -j4`, then
  Wave 23 opening with ten lanes. Load 80% → 100% with 34 build/search processes. Runs
  read 0.23-0.49.

**A paired alternating design cancels a STATIONARY confounder. This one was not
stationary** — `off_1`'s first window read 0.19 and its second 0.46, inside one run. A
rising confounder also **aliases onto the treatment through run order**: I ran `on` first
in every pair, so the second (`off`) run absorbed more of the ramp, and pairs r5/r6 showed
a spurious "arming is 20% faster". Counterbalancing (off-first) was the right correction
and I started it, but the box never went quiet again.

**Two lessons, both cheap to act on:**
1. `stress_run.sh` should record system-wide CPU load (or at least a process count) in the
   CSV header. It records everything about the game and nothing about the machine, and a
   reader of `report.txt` alone cannot tell a regression from a neighbour.
2. A perf A/B must **counterbalance order**, not just alternate. Alternating with a fixed
   within-pair order converts drift into apparent treatment effect.

---

## 4. What arming actually does — the risk surface

Read out of the arming path rather than assumed. With the DLL present,
`RtSystemAllocatorRegister()`:

1. installs the platform seam if absent;
2. **retires our own allocator-lock prologue patches** (`g_retireLock`), then hard-refuses
   if anything still owns those two prologues;
3. **splices detours over retail's `Allocate` (0x1800c6e60, 13 bytes) and `Free`
   (0x1800c6c00, 12 bytes)**, so the DLL's *own internal callers* arrive at our bodies;
4. registers 27 exports (and repoints `?Allocate@`/`?Free@` at the composed locked stubs);
5. rolls all of it back and registers nothing if any step fails.

This is an **atomic, process-wide heap swap with prologue patching**, not a feature flag.
The refusal paths are well built and now attest. But the failure mode of a mistake here is
not a wrong pixel or a crash at a known address — it is a block on the free list the
allocator never handed out, surfacing arbitrarily later in an unrelated allocation. The
project has already paid for exactly that once.

---

## 5. RECOMMENDATION

### 5.1 Do NOT make `BANISHED_RT_ALLOC` default-on yet

**The evidence that carries the decision is that there is no positive case, not that there
is a negative one.**

* Perf: **neutral**, and only bounded at ±5% by 4 pairs. Neutral does not buy the risk in
  §4. A default-on flip should be justified by something it *gains*; this one gains a
  census row and 5 `ported` counts.
* Heap: one clean armed run (53,905 walks, 0 violations). That is **one** run of **one**
  workload. It is necessary, not sufficient.
* The measurement environment was hostile for most of the session, and I would not want a
  process-wide heap swap defaulted on for every other lane on the strength of a series
  taken in a 40-minute window.

### 5.2 The honest risk case, including what argues FOR flipping it

Arguments **for** default-on, stated fairly because a recommendation that only lists its
own side is not a risk case:

* The bodies are byte-faithful and have existed since Wave 2; the arming path's refusals
  are thorough and now self-attesting.
* Memory records 901,685 allocations through our bodies with zero redzone violations, and
  my own run corroborates that at 53,905 heap walks.
* It is the **cutover path**. Everything that stays opt-in is untested on the route the
  project is actually heading for, and "we will find out at cutover" is how boot walls get
  built.
* Five census rows and a `ported` figure move.

Arguments **against**, which I judge to win today:

* §4: prologue splicing plus a process-wide heap swap, whose mistakes are silent.
* The OOM path changed under this lane's feet. `?Display@Error@System@@` is now ported
  into the exe (Wave 22 lane ERRCFG), and `standalone/main.cpp:728` hooks
  **Runtime-x64.dll's** IAT `MessageBoxW`. An out-of-memory on an unattended armed run
  could therefore raise a modal dialog nothing intercepts, which per CLAUDE.md is
  indistinguishable from a slow load. **This risk is REASONED, NOT OBSERVED** — I
  deliberately did not drive the allocator to exhaustion, and nothing here should be read
  as having tested it.
* Defaulting it on makes every other lane's run an allocator experiment. With ten Wave 23
  lanes live, a silent corruption would be attributed to whatever lane happened to see it.
* The census **overstates** `?Allocate@`/`?Free@` today (§6.2). Flipping the default while
  an instrument disagrees with reality about the same subject is the wrong order.

### 5.3 `?_singleton@MemoryAllocator@System@@0PEAV12@EA` — ROUTE-SCOPED, and both halves observed

| route | verdict | evidence |
|---|---|---|
| **DLL present** | **UNPROMOTABLE — HARMFUL** | publishing our zeroed storage over the DLL's live singleton dies in `EnterCriticalSection` on a null self before the UI exists. This is the blocker that makes `BANISHED_RT_MINT=1` fatal and the reason per-row mint selectivity was built. **Observed.** |
| **Cutover (DLL absent)** | **REQUIRED** | `engine/funcs/engine_funcs.cpp:1629` dereferences `ResolveAddr(Module::Runtime, 0x1801fc718)` with no guard. The first controlled DLL-absent boot faulted there because the row was unregistered (`standalone/resolve_standalone.cpp` records it). **Observed.** |

The verdict **inverts** between routes. Tagged in `standalone/reg_mint_promoted.cpp` with
`[SCOPE: DLL-PRESENT]` and an addressee; `tools/unpromotable_slot_check.py` is green
(43 tags, CHECK 1/2/3 all 0 failures, all six synthetic controls GREEN).

**This independently confirms lane TIMER's rule on a different subject.** Unpromoted, our
writer (`rtalloc::Construct`) publishes through `RT_DATA_SLOT → ResolveExport →` the DLL's
real slot, so the many unported DLL internals that read `0x1801fc718` as a hard-coded RVA
(`MOV RCX,[0x1801fc718]`, documented at `runtime/src/collection/{array,linkedlist,
avl_trees,set_avl_e24}.cpp` and `runtime/src/ui/dialog_factory.cpp`) stay correct **by
construction**. Promoting freezes their copy. For an allocator that is the worst possible
split-brain: a DLL internal freeing against retail's arena a pointer our arena handed out
is silent corruption, not a clean crash. TIMER's IDIV-by-zero was the *kind* outcome.

I reached this on my row's own reader/writer evidence, and it happens to agree with TIMER
rather than with ERRCFG's refutation. Both are correct for their rows; the rule is not
universal, which is the point of deciding per row.

### 5.4 What would change my answer

1. A second and third `BANISHED_REDZONE=watch` armed run on a quiet box, plus one on a
   *different* workload (a long menu/mods session, not just the stress town).
2. A counterbalanced 8-pair A/B on a quiet box, tightening the bound from ±5% to ±2%.
3. Resolving §6.2 so the census and reality agree about `?Allocate@`/`?Free@`.
4. A decision about the OOM/modal-dialog path in §5.2 — either extend the `MessageBoxW`
   seam to the exe, or confirm the harness can detect it (`boot_state.sh` reports
   `BLOCKED_DIALOG`, so it probably can; unverified).

That is a lane's worth of work, not a wave's.

---

## 6. TASK 4 — the instrument question, and the census was wrong in BOTH directions

Committed at `f653ae07`.

### 6.1 `GATED-BUT-WRITTEN GAP ROWS, PER NAMESPACE`

The census printed both halves of a fact and never the join: a per-namespace gap, and
further down `40 registration(s) are present in the source but NOT counted above` with its
own rule *"A gated registration is not a port. Arm it, measure it, then move it."*
Nothing connected the second to the first, so a reader could not tell which gap rows are
UNWRITTEN and which are WRITTEN-AND-UNARMED. **Those are not the same work and not even
the same kind of work: one is a port, the other is a risk decision.**

Wave 21's gap-kind decomposition could not have caught this — all five allocator rows
classify correctly as `BODY`. They are not unwritten, they are unarmed.

It now prints, per namespace, itemised, with the gate named:

```
GATED-BUT-WRITTEN GAP ROWS, PER NAMESPACE  (Wave 22 lane ALLOC)
  namespace       gap   gated |     BODY  ...
  System           25       5 |        5  ...
  ITEMISED -- the rows, and the gate that holds each one back.
    System
      BODY    ?Get@MemoryAllocator@System@@SAPEAV12@XZ       BANISHED_RT_ALLOC
      BODY    ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z  BANISHED_RT_ALLOC
      BODY    ?MemFree@MemoryAllocator@System@@SAXPEAX@Z     BANISHED_RT_ALLOC
      BODY    ?Shutdown@MemoryAllocator@System@@QEAAXXZ      BANISHED_RT_ALLOC
      BODY    ?Startup@MemoryAllocator@System@@QEAA_N_K0@Z   BANISHED_RT_ALLOC
  => 5 of 183 gap rows (3%) are WRITTEN and held back by a gate, not by an absent body.
```

**WAVE22_PLAN §1's finding is now a printed line rather than a grep.** It moves no count.

**It generalises, and Wave 23 lane OBJ has already produced the second instance** —
`??_7Dialog@UI@@6B@`'s expiry reads as satisfied to a grep but is gated behind
`BANISHED_RT_DLGFACTORY`. The screen covers `if (<flag>)` blocks through the existing
`GATED_OFF_BLOCKS`, so OBJ's case will print under UI with the gate named **as soon as
that file/gate pair is declared**. See §7 residual 1: nothing auto-discovers gates.

### 6.2 Building it found the census wrong in both directions, on my own subject

**UNDERSTATED by one — FIXED here.** The new join printed
`internal / 0x180009c30 / BANISHED_RT_ALLOC`, a row I knew to be live on every run.
`GATED_OFF_FILES` is coarse: it asks *which file* and cannot ask *which Register
function*. `0x180009c30` is `FreeLockedIgnoringSelf`, registered **deliberately
unconditionally** from `RtSystemAllocatorRegisterSharedAddresses()`, which
`rt_system_port.cpp:84` calls outside every wave switch. New `GATED_OFF_EXCEPTIONS`
records it. **TOTAL ported 1129 → 1130 (86.0% → 86.1%), gap 184 → 183, `internal` 41 → 40.
System is untouched.**

**OVERSTATED by two — ALSO FIXED, on an integrator ruling.** Having found it wrong one
way, I asked the same question the other way and found a **fourth gating shape** none of
the three existing mechanisms can see: a **guard clause at the top of the registering
function.**

```cpp
extern "C" void RtAllocSwapPostRegister() {
    if (!g_swapTaken) return;                       // <-- the gate
    RegisterRuntimeExport("?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z", ...);
    RegisterRuntimeExport("?Free@MemoryAllocator@System@@QEAAXPEAX@Z", ...);
```

That is the **only** registrar of those two names (`g_swapTaken` is defined false and set
true only on the arming path; the other two `RegisterAllocatorExports` call sites are the
cutover and ARMED branches). Their two `[port]` detours sit in `RtAllocSwapInstall`, which
only the arming path calls. Both rows were counted `ported` and neither is live unarmed.

I first landed this as a **report-only** screen, on the reasoning that these two are in
System, System is this wave's subject, and Wave 23 lanes are quoting the figures. **The
integrator overruled that and directed both corrections be applied, and the reasoning is
better than mine:**

* Five `MemoryAllocator` rows are excluded from `ported` *precisely because* a gated
  registration is not a port. `?Allocate@`/`?Free@` are gated by the same flag through a
  different shape. **Counting them while excluding their five siblings is not
  conservatism — it is an inconsistency that happens to flatter the number.**
* A known-wrong authority figure is the worst artefact to ship: the next wave inherits it
  as truth with no way to find the caveat.
* My stated reason did not hold on the facts. The Wave 23 lanes quoting `1314/1137`
  branched from `a60351e3`, which **predates this change** — they are already on a
  different tool version, so moving the count cannot invalidate a figure they took.

So `GATED_OFF_FUNCS` now feeds `scan()`, on **both** sides:

* the **registration** side, through the same `in_gated_block()` predicate the
  `if (<flag>)` shape already used;
* the **detour** side, because a detour target confers `ported`. Gating one without the
  other **would have moved nothing and looked like it had** — which is the same defect
  class as everything else in this doc.

### 6.2.1 The fourth gating shape, stated so it can be recognised again

> **A registration is not live if it sits inside a function whose first statement is a
> guard clause on a global that only an arming path sets.** The file is not opt-in, there
> is no `if (<flag>) { ... }` around the call, and it is not the mint — so all three
> existing detectors read it as an ordinary port.

The four known shapes are now: opt-in **FILE** (`GATED_OFF_FILES`), opt-in **BLOCK**
(`GATED_OFF_BLOCKS`), the **MINT**, and opt-in **FUNCTION** (`GATED_OFF_FUNCS`).

### 6.2.2 The screen that replaced the report

The report-only screen became `GATED-FUNCTION EXCLUSIONS`, a **liveness** report rather
than an all-clear — because once the exclusion works there is no suspect left, and *a
screen whose success state and whose dead state print the same thing cannot be trusted in
either*. It prints what the declared spans actually matched and where each key ended up:

```
    0x1800c6c00                                     excluded
        in standalone/rt_alloc_swap.cpp::RtAllocSwapInstall -- BANISHED_RT_ALLOC (only the arming path calls it)
    0x1800c6e60                                     excluded
    ?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z excluded
        in standalone/rt_alloc_swap.cpp::RtAllocSwapPostRegister -- BANISHED_RT_ALLOC (guard: if (!g_swapTaken) return;)
    ?Free@MemoryAllocator@System@@QEAAXPEAX@Z       excluded
    => 4 key(s) matched; 4 excluded, 0 still counted PORTED.
```

A declaration matching **zero** keys prints a loud DEAD warning instead of a quiet zero,
and a key that is excluded here but still reaches `ported` by some other registration
prints as a **LEAK**.

### 6.2.3 The figures, each labelled with the tool that produced it

**Do not read these as a wave headline.** Per the integrator, the merged-tip figure is
re-measured after merge; Wave 21 published a summed figure and had to correct itself.

| tool version | called | ported | detour | TOTAL % | gap | System called/ported | System gap |
|---|---|---|---|---|---|---|---|
| `wave22/base` census, my tree | 1313 | 1129 | 9 | 86.0% | 184 | 103 / 78 | 25 |
| **this lane's census, my tree** | 1313 | **1128** | **7** | **85.9%** | **185** | 103 / **76** | **27** |

Net **−1**: `internal` +1 (the understatement fix), `System` −2 (the overstatement fix).
The `detour` column drops 9 → 7 for the same reason. **Both movements are corrections in
the direction of reality, and they happen to point opposite ways — which is the point.**
Of the 185 gap rows, **7 are now printed as gated-but-written**, all seven `System`
`BODY`, all seven `BANISHED_RT_ALLOC`.

### 6.3 Two defects found while building the screens

Both are instances of shapes this project has already paid for, which is why they are
recorded rather than quietly fixed:

* **The first draft of THE MIRROR tested each row's `files`** — which is the set of files
  that **CALL** the symbol, not the ones that **REGISTER** it. It printed `(none)` with
  two known positives sitting in front of it. *A screen looking for a shape must be handed
  one.* I had two, and only that turned a confident zero into a bug report. Fixed to ask
  `d["ported_exports"]` / `d["ported_addrs"]`.
* **The run summariser read the arm from the log with `"ARMED" in line`** — which is true
  of `"UNARMED"`. Every control run was labelled armed. A substring test on a word whose
  negation contains it.

### 6.4 Verifying the census change did not break the census

The type checker flags `.items()`-on-`int` and `"int" is not iterable` at seven sites in
this file. **They are not mine and they are not bugs.** Derived, not asserted:

* the census runs end to end: 910 lines of report, exit 0, zero tracebacks, sane totals;
* every flagged line exists **verbatim on `wave22/base`** (checked against
  `git show wave22/base:tools/runtime_census.py`, not by eye);
* the cause is a union type over `scan()`'s heterogeneous return dict — values are
  sometimes `int`, sometimes `dict` — which the checker flags at every access site;
* I ran the **base-tip census against my own tree** and diffed its full output against the
  new one. The only differences are the +1 `internal` row, the two new sections, and
  `40 → 39` gated registrations. Nothing else moved.

### 6.5 WHICH CODE PATHS I ACTUALLY EXECUTED — and the near miss that prompted the question

The integrator flagged `"gated_func_spans" is not defined` at line 1493 as a possible
`NameError` in my edit. **It was a stale diagnostic** — I added the *call* before the
*definition*, and the snapshot was taken between those two edits. `gated_func_spans` is
defined at line 1357 and called once at 1535; line 1493 is unrelated code.

**But the challenge behind it was right, and I could not initially answer it.** "It ran
clean end to end" does not cover a line that did not run, and that is the same shape as
this lane's own best finding: the perf binary could not say whether it had armed, so a
silent refusal would have read as a clean result. So:

**Executed, and proven by an effect rather than by absence of a traceback:**

| path | proof it ran |
|---|---|
| `gated_func_spans()` inside `scan()` | `ported` moved 1129 → 1128 and `detour` 9 → 7. Those movements are only reachable through that call. |
| `GATED_OFF_EXCEPTIONS` lookup | `internal` moved 141 → 142. |
| `GATED-BUT-WRITTEN` table, non-empty branch | prints 7 itemised System rows. |
| `gated_func_keys()` + the LEAK column | prints `4 key(s) matched; 4 excluded, 0 still counted PORTED`. |
| the detour-skip branch | `detour` column fell by exactly the two allocator detours. |
| synthetic span-walk controls | both print GREEN on every run. |

**Two independent implementations, deliberately, and they check each other.**
`gated_func_spans()` runs inside `scan()` on scan's own text and is what *moves the
count*; `gated_func_keys()` re-reads the file at report time and is what *prints*. They
share the declaration and nothing else. The `excluded / LEAK` column is therefore one
implementation confirming the other's effect on the numbers — a key named by the printer
that still reached `ported` prints as a **LEAK**.

**And a control of the controls, because agreement between two dead implementations is
still agreement.** I broke the span walk two ways and confirmed each control goes RED for
the breakage it exists to catch:

```
baseline                        pos=True  neg=True
break 1 (wrong function name)   pos=False neg=True    <- positive control fires
break 2 (no forward-decl skip)  pos=False neg=False   <- negative control fires
```

Break 2 is the dangerous one: without the forward-declaration skip, a span opens at the
*declaration* and runs to the end of the first body it meets, silently gating every
registration in between. That is a bug that would have *moved the count in the right
direction for the wrong reason*.

**NOT executed, and named rather than left implied:**

* the `gf_unread` refusal branch (a declared file or function that cannot be read) — I did
  not construct a broken declaration to fire it;
* the `GATED_OFF_FUNCS` empty / zero-keys DEAD branches — reachable only by emptying the
  declaration, which I did not do in the shipped tool;
* the `(no gap row is gated-but-written)` branch of §6.1, including its
  POSITIVE-CONTROL-FAILED warning;
* the `RE_REG_EXPORT_NONLITERAL` and `RE_DETOUR_EXPORT` paths *inside a gated function
  span* — no such registration exists in the tree today, so that combination is
  unexercised;
* `--csv` and `--namespace` were exercised; no other flag was.

---

## 7. OPEN RESIDUALS

1. **NOTHING AUTO-DISCOVERS A GATE — and this is the sharpest limitation in the lane.**
   All four detectors (`GATED_OFF_FILES`, `GATED_OFF_BLOCKS`, `GATED_OFF_FUNCS`, the mint)
   are **hand-declared**. So the screens report what somebody already noticed, and
   **every gate nobody thought to declare reads clean.**

   That is the shape of a screen that cannot name its own subject: its output is
   identical whether the tree has no undeclared gates or a hundred. A green
   GATED-BUT-WRITTEN table is **not** evidence that a namespace's gap is real body work;
   it is evidence about the declarations. Two waves have now produced one instance each
   — Wave 22 `BANISHED_RT_ALLOC` (a FILE and a FUNCTION), Wave 23 lane OBJ
   `BANISHED_RT_DLGFACTORY` on `??_7Dialog@UI@@6B@` (a BLOCK, whose expiry *reads as
   satisfied to a grep*). **Two independent instances in two waves in two namespaces
   makes it a class, not an allocator quirk.**

   The real fix is a discovery pass: find every `RtWaveEnabled(...)` / `getenv(...)`
   guard and every global an arming path sets, walk the functions they dominate, and
   report undeclared registrations inside them — i.e. derive the four lists instead of
   maintaining them. That is a lane, not a residual footnote.

   **[ADDRESSEE: the Wave 23 integrator, at Wave 23 close — to either schedule the gate
   discovery pass as a named lane or record in writing that the four lists stay
   hand-declared and that every gap figure carries that caveat. Not "the next lane that
   notices", which is how an expiry with a condition and no addressee waits forever.]**

2. **Lane OBJ's `BANISHED_RT_DLGFACTORY` case is not yet declared.** It will print under
   `UI` in the GATED-BUT-WRITTEN table the moment its file/gate pair is added to
   `GATED_OFF_BLOCKS` — which is a one-line change, and I have not made it because I have
   not read that file and would be declaring a gate I cannot vouch for.
   **[ADDRESSEE: Wave 23 lane OBJ, which owns that finding and has read the file.]**
3. ~~**`singleton_ctors.cpp:1110`'s "no call site in our tree" comment is stale**~~ —
   **DONE.** Corrected in place with the date and the evidence, not deleted: the
   conclusion it supports (safe to land) still holds, its justification does not. §2.2.
4. **`stress_run.sh` records nothing about machine load** (§3.5). Two sessions of this
   lane's measurements were destroyed by neighbours and the harness never said a word.
5. **The OOM/modal-dialog hazard** (§5.2) is reasoned, not observed.

## 8. WHAT I DID NOT CHECK

* **No dynamic verification of the gate.** §2 is a source screen. I did not set a
  breakpoint on `rtalloc::Allocate` in an unarmed run, and a source screen cannot see a
  call through a runtime-chosen function pointer.
* **No Ghidra work.** Every claim about DLL internals reading `0x1801fc718` by hard-coded
  RVA is taken from disassembly already transcribed into this tree's comments. I did not
  re-run `get_xrefs_to` on that address, so I have **no count** of those readers — only
  that they exist and are numerous. TIMER's equivalent claim is measured; mine is not.
* **No allocator shadow run** (`BANISHED_RT_ALLOC_SHADOW`). The replica comparison is the
  strongest available correctness instrument for this subject and I did not use it; my
  correctness evidence is one redzone run.
* **No exhaustion test**, deliberately (§5.2).
* **No cutover boot.** Every promotion verdict here is DLL-present unless §5.3 says
  otherwise, and §5.3's cutover half is taken from a previously recorded boot, not one I
  ran.
* **One workload only.** All timing is the `sa-stress` town. Menu, load/save and mod paths
  are unmeasured, armed or unarmed.
* **`GATED_OFF_EXCEPTIONS` has one entry and I checked one file.** There may be other
  unconditional registrations inside opt-in files; I looked at `allocator.cpp` and
  `allocator_win.cpp` and nowhere else.
