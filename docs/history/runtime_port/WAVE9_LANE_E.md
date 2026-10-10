# Wave 9, lane E — the allocator: gate steps 2 and 3

Landed 2026-09-07 on `wave/upgrade-path-complete`. This lane wrote **no allocator logic**.
It wrote the host-side code that makes the allocator port *measurable*, and it fixed the
ordering defect [WAVE2_ALLOCATOR.md](WAVE2_ALLOCATOR.md) §7.2 recorded as "a trap armed for
whoever flips the switch first".

**The verdict is still NO-GO. Nothing here has executed.** Per the round's build/run
protocol this lane compiled and stopped; the measurement runs belong to the next round and
are spelled out in §5 below. Do not read the presence of this code as evidence of anything.

---

## 1. What landed

| File | Change |
|---|---|
| `standalone/rt_alloc_swap.cpp` (new, 342 lines) | the composed detour stub (§4 of WAVE2_ALLOCATOR), the observe-only shadow driver (§3), and the post-registration fixup (correction 3 below) |
| `standalone/rt_alloc_swap.h` (new) | four `extern "C"` entry points |
| `standalone/allocator_lock.cpp` | `AllocatorLockRetireForPort()`; `AllocStats_RecordAlloc` / `AllocStats_RecordFree` exported; two null-by-default shadow observer hooks called inside the existing wrappers; saved-byte bookkeeping so the patches can be undone |
| `standalone/allocator_lock.h` | declares `AllocatorLockRetireForPort()` with the reason it exists |
| `standalone/main.cpp` | **the reorder**: `InstallAllocatorLock()` / `InstallFreeLock()` moved to before `RuntimePortInit()`; `RtSystemAllocatorSetDetourInstaller(&RtAllocSwapInstall)` and `RtAllocShadowInstall()` added there; `RtAllocSwapPostRegister()` after `RuntimePortInit()`; `RtAllocShadowSummary()` beside `RtDiffSummary()` |
| `runtime/src/system/allocator.cpp` | two harness-health counters and `RtAllocShadowHealth()` (correction 4) |
| `runtime/types/system/MemoryAllocator.h` | declares `RtAllocShadowHealth` |

Builds clean: `mingw32-make standalone-debug`, zero errors, zero new warnings, exe stamped
20:23:47 with every touched source older than it. `tools/export_name_audit.py`: **652
distinct `ResolveExport` names, 0 not in the DLL**. `tools/dispatch_slot_map.py --check`:
0 mismatches, 0 double registrations.

### Default behaviour

Everything new is off unless an env var says otherwise. **One default *does* change, on
purpose and as instructed:** the allocator lock now installs earlier in boot (right after
`ResolveInit`, instead of after `EngineFuncsInit`). It is the same patch on the same two
prologues with the same guard; the only difference is that it now covers the window between
`ResolveInit` and `EngineFuncsInit` as well. Nothing in that window allocates on a second
thread, so no behavioural change is expected — but it is a change to every run and it is
named here rather than buried.

---

## 2. The atomicity guarantee, and how it is preserved

Unchanged and untouched. `RtSystemAllocatorRegister()` (`runtime/src/system/allocator.cpp`)
still:

1. refuses if the platform seam is not installed;
2. refuses if `FindPatchedPrologue()` sees *any* foreign patch on the two entry points;
3. refuses if no detour installer was handed in;
4. calls the installer and **`return 0` — registering nothing at all — if it returns false.**

`RtAllocSwapInstall` upholds its half: it returns false without having modified the DLL in
any observable way. Concretely, its two failure paths are

* Allocate's detour fails → nothing was patched, return false;
* Free's detour fails → `RemoveDetour(&g_allocDetour)` restores Allocate's 13 bytes, return
  false.

so "false" always means "the DLL is exactly as we found it". The one asymmetry worth
stating: by the time either can fail, `AllocatorLockRetireForPort()` has already run, so a
failed swap leaves the process with **no allocator lock**. That is strictly safer than the
alternative (retiring after patching would mean restoring the lock's stub bytes over our
detour), and it is the pre-lock status quo rather than a corruption, but a failed swap
should be treated as "abort this run", not "carry on unlocked". `RtAllocSwapInstall` says so
in its log line.

---

## 3. Corrections to WAVE2_ALLOCATOR.md

The brief asked for these loudly. Four, in descending order of consequence.

### Correction 1 — §3's shadow driver design is wrong: it must not be a second detour

§3 asks for "a driver in `standalone/` that detours `MemoryAllocator::Startup`, `Allocate`
and `Free` in observe-only mode". Taken literally that is broken twice, and it is broken by
the same document's §4:

* `allocator_lock.cpp` is **default-on and already owns both prologues**. A second detour on
  those 13/12 bytes is exactly the hazard §4 spends a page on — whoever patches second
  either clobbers the first's saved trampoline bytes or is refused by its `memcmp`. The
  shadow is supposed to be the *safe* measurement; installing it would have reproduced the
  wave's headline hazard in order to measure the wave's headline hazard.
* **Serialization.** The replica must be mutated in the same order the real arena was. The
  real arena's order is imposed by the critical section the lock's wrappers take. An
  observer outside that lock replays two threads' allocations in an order the real allocator
  never saw, and the replica's offsets desynchronise for reasons that have nothing to do
  with the port. Given the design's own "a divergence never recovers" property, that would
  produce a wall of red on a correct port.

What landed instead: `Startup` gets a real detour (nobody else patches it), and `Allocate` /
`Free` are observed through two null-by-default function pointers called **inside**
`AllocateLocked` / `FreeLocked`, with the CS held. §3's next sentence — "this is the existing
`allocator_lock.cpp` shape with two extra calls" — describes this correctly; the bullet above
it does not, and the bullet is what a reader would implement.

**Consequence, now enforced:** the shadow *requires* the lock. `BANISHED_ALLOC_LOCK=0`
removes the wrappers and the observer would see nothing while reporting a clean
`divergences=0`. `RtAllocShadowInstall()` refuses to arm in that case rather than produce
that reading. It also refuses if `BANISHED_RT_ALLOC=1` is set in the same run — the shadow
is the measurement that must come *first*.

### Correction 2 — §7.1 (the `SAPEAXII` phantom) is CLOSED, not open

WAVE2_ALLOCATOR §7.1 lists six live call sites using
`?MemAlloc@MemoryAllocator@System@@SAPEAXII@Z`, a name the DLL does not carry. Re-checked
today: `grep -rn SAPEAXII` over `code/ standalone/ runtime/ engine/ dx11/` returns **nine
hits, all of them comments** saying the name does not exist. There is no live
`ResolveExport` on it anywhere, and `tools/export_name_audit.py` reports 0 phantom names
across all 652. Someone fixed it after that section was written; the section still reads as
an open defect with a six-row impact table. (Verified independently against the shipped DLL
with `pefile`: `_KH` present, `SAPEAXII` absent.)

### Correction 3 — the swap registers an UNLOCKED entry point for our own callers

Not in the doc at all, and a genuine gap in §4's design.

After the swap, the DLL's own callers reach `SwapAllocate`/`SwapFree` through the detour and
therefore run **under the critical section**. Our ~1,180 call sites reach the port through
`RegisterRuntimeExport`, and `RtSystemAllocatorRegister()` registers the **raw** ported
bodies for `?Allocate@` and `?Free@` — which are byte-faithful and therefore take no lock,
exactly as retail's do. That is two entry points to one free list with different locking:
the precise condition `allocator_lock.h` was written to eliminate.

Exposure today is zero — every consumer in `code/` uses the `MemAlloc`/`MemFree` statics,
and the port's `rtalloc::MemAlloc`/`MemFree` do take the lock (faithfully; retail's statics
do too). It is a latent trap, and it is the kind that surfaces as a wild write with no
proximate symptom.

Closed by `RtAllocSwapPostRegister()`, called from `main.cpp` immediately after
`RuntimePortInit()`: if and only if the swap took, it repoints those two export
registrations at the composed stubs. Chosen over changing the `RtAllocDetourInstaller`
typedef so that the atomicity contract in `MemoryAllocator.h` stays exactly as written.

**Not closed, and stated as a known limitation:** with `BANISHED_RT_ALLOC=1`, our own
`MemAlloc`/`MemFree` path runs `rtalloc::MemAlloc` → `rtalloc::Allocate` directly, so it is
correctly locked but is **not** seen by `BANISHED_ALLOC_STATS` or by
`RedzoneAlloc_TeardownOp`. Retail-side traffic still is (it goes through the detour).
Diagnostics coverage becomes asymmetric under the swap. Recorded rather than papered over.

### Correction 4 — the shadow's replay map could manufacture divergences

`ShadowMapInsert` gave up silently after 64 probes, and `RtAllocShadowFree` returned silently
when a pointer had no replica. Either one makes the replica keep a block the real arena
freed, and the design's "offsets desynchronise on a later call and never recover" then turns
one harness hiccup into a total failure that reads as a catastrophic port defect. This is the
`reference_probe_needs_an_effectiveness_control.md` shape, and the
`reference_truncated_tool_output_manufactures_calls_zero.md` shape, in one place.

Both now count (`RtAllocShadowHealth`) and both are printed by `RtAllocShadowSummary()` with
an explicit "HARNESS DEFECT … not the port's" line. The summary also carries the
effectiveness control the wave standard demands: `observed=` (how many calls actually reached
a comparison), `skipped_pre_startup=` (traffic that arrived before the replica armed — any
non-zero value makes the run **invalid**, not clean) and `other_allocator=`.

---

## 4. Re-read of §5's evidence-against list

| §5 "evidence against" | Status after this lane |
|---|---|
| "The port has never executed a single instruction." | **STILL STANDS.** This lane ran nothing. Closing it is the whole point of the next round. |
| "The shadow harness has never run either." | **STILL STANDS**, for the same reason — but it can now be run, which it could not be this morning. |
| "The swap installer and the shadow driver do not exist; two `standalone/` files must land." | **CLOSED.** Both landed in one file, plus a header. |
| "`allocator_lock.cpp` is default-on and currently installs after `RuntimePortInit`, so the very first `BANISHED_RT_ALLOC=1` run would silently lose the lock." | **CLOSED.** The reorder landed, `AllocatorLockRetireForPort()` exists and the swap installer calls it, so there is exactly one owner of those prologues at every instant. |
| "1,778 inbound call edges … a wrong bit in the free-list prev split is a wild write with no proximate symptom." | **STILL STANDS**, and is untouched by anything here. This is what the shadow exists to catch, and it will only be caught by a run with `observed=` in the millions. |

Gate status (§5 "Gate to flip to GO"): step 1 was already done; **steps 2 and 3 are now
code-complete but step 2's *measurement* is not taken**; step 4 is untouched and must not be
attempted before step 2 reports.

---

## 5. Exactly what the measurement round must run

All four of these are `BANISHED_RT_ALLOC` **unset**. The shadow refuses to arm otherwise.

```bash
# 1. Full autostart game, menu -> new game -> gameplay.
BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1 BANISHED_AUTOSTART=1 \
  mingw32-make sa-run

# 2. The stress town (the only run that reaches millions of allocations).
BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1 mingw32-make sa-stress

# 3. A save/load cycle — the allocator's large-block and coalescing paths are
#    exercised hardest by world load, and Wave 4 lived here.
BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1 BANISHED_AUTOLOAD=Auto \
  mingw32-make sa-run

# 4. NEGATIVE CONTROL — prove the oracle can fail. Perturb one thing in the
#    replica (e.g. temporarily start g_shadow.minBin at 0xbe in
#    RtAllocShadowStartup) and confirm the run goes red within a few hundred
#    calls. A probe that has never reported a divergence has not been shown to
#    be able to.
```

Read out of `standalone_error.log`, in this order:

1. `[RtAllocShadow] observing MemoryAllocator …` — the driver armed. Absent ⇒ the run
   measured nothing regardless of what follows.
2. `[RtAllocShadow] SUMMARY armed=1 observed=N divergences=D skipped_pre_startup=0
   other_allocator=0 map_full=0 free_unmapped=0`.
3. The `[RtDiff]` per-site lines for
   `System::MemoryAllocator::Allocate [shadow]` / `Free [shadow]`. **Read the `calls=`
   column, not just `divergences=`** (`reference_uncalled_port_is_not_verified.md`), and read
   the RAW log, not a tail of it (`reference_truncated_tool_output_manufactures_calls_zero.md`).

### What flips the verdict to GO

All of the following, together:

* `divergences=0` across runs 1–3, with `observed=` **in the millions** on run 2;
* `skipped_pre_startup=0`, `other_allocator=0`, `map_full=0`, `free_unmapped=0` on all three
  — any non-zero makes that run invalid, not clean;
* run 4 goes red, proving the oracle can fail.

Anything less is NO-GO. A green summary with a small `observed=` is the Wave 1 defect #4 trap
and must be rejected.

### What comes after GO, and must not be skipped

Gate step 4: a first `BANISHED_RT_ALLOC=1` run behind PLAN.md §5's four wave-boundary gates —
tutorials 1–4, the achievement census, `Save9.sav`, and `sim ms/iteration` within 5%
(three runs per config, `reference_one_perf_run_cannot_resolve_the_gate.md`). On that run,
expect and check for these three lines in order:

```
[AllocLock] MemoryAllocator::Allocate is now thread-safe …
[AllocLock] patches RETIRED for the ported allocator (Allocate=1 Free=1)
[RtAllocSwap] ATOMIC SWAP INSTALLED …
[RtAllocSwap] ?Allocate@/?Free@ registrations repointed at the composed (locked) stubs
```

If the second line is missing, the reorder did not take and the run is unlocked — kill it.
