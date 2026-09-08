# WAVE 9 LANE DESTROY — `0x180026b50`, the attempt-17 cutover wall

**Status: CLOSED.** The wall is a door and the door is registered. One address
closed, one address declined with a reason, no other destroy-range door left
open in this tree.

---

## 1. What the wall was

Attempt 17's boot died here:

```
#5 RtIo_PackageFile_ReadPackfile
#4 RtIO::ArraySetSizeFill
#3 rtcoll::RtColl_ArraySetSizeFill_1800298f0
#2 rtcoll::SetSizeCore
#1 rtcoll::DestroyPkgEntryRange          <- EXEC of 0
```

`DestroyPkgEntryRange` (`runtime/src/collection/array.cpp`) forwarded **by
address** to `0x180026b50`, the packfile-entry range destructor. Unexported and
unregistered, so with the DLL absent `ResolveAddr` returned null and the call
executed address 0. Fourth time this wave the wall has been a by-address door
with no name at the call site — the failure class no name-based instrument can
see.

---

## 2. The body, READ — and the one sentence in the tree that was half wrong

`0x180026b50` is **Body B with `kD_Str_10`**: stride `0x10`, one `System::String*`
slot at `+0x00`. Both listings, side by side, read out of `Runtime-x64.dll` for
this commit and NOT inferred from each other:

| `0x180026b50` | `0x180005930` (`DestroyStringExt`) |
|---|---|
| `TEST R8D,R8D / JLE ret` | same |
| `MOV [RSP+8],RCX` (spilled, never read) | same |
| `PUSH RSI/RDI/R14 ; SUB RSP,0x30` | same |
| `MOV [RSP+0x20],-2` (SEH state) | same |
| `MOV RBX,RDX ; MOVSXD RSI,R8D ; XOR R14D,R14D` | same |
| **`NOP dword [RAX+RAX]`** (alignment) | *absent* |
| **`MOV [RSP+0x50],RBX` TWICE** | once |
| `MOV RAX,[RBX] / TEST / JZ` | same |
| `DEC [RAX-4] / CMP [RAX-4],0 / JNZ` | same |
| `ADD qword [RBX],-8 / MOV RBP,[RBX] / JZ` | same |
| `Enter / Free(_singleton,RBP) / Leave` | same |
| `MOV [RBX],R14` (the "still shared" arm) | same |
| `ADD RBX,0x10 / DEC RSI / JNZ` | same |

The only two differences are an alignment `NOP` and a **duplicated store of RBX
into this function's own SEH scope slot** at `[RSP+0x50]` — MSVC emitting the
frame-variable update once per inlined scope. Neither is observable: that slot
is written and never read. Every store to the SUBJECT, every branch, every
immediate, the stride and the single offset agree.

**`array.cpp` said this was "a different function from `DestroyStringExt`'s
`0x180005930`", and that sentence was doing two jobs at once.** True of the
ADDRESS, false of the BODY — and only the first was ever checked. It was
carrying the whole of "why `kSpec_PkgEntry` exists", when in fact:

* the **ASSIGN** genuinely is its own thing (`0x1800067d0`'s two trailing
  `Filename::AdjustDelimiters` calls — the third near-miss this family
  produced), so the `ArraySpec` row stays; but
* the **DESTROY** is not, so it needs **no new `DestroySpec` row**.

The comment in `array.cpp` is corrected in this commit rather than left to be
re-derived. This is not "reused by lookalike": the conclusion is drawn from the
table above, the probe compares against the **shipped `0x180026b50`** (not
against `0x180005930`), and the trailing-call trap specifically does not recur —
this listing's only `CALL`s are the allocator's Enter/Free/Leave.

---

## 3. What landed

| file | change |
|---|---|
| `runtime/src/collection/array_destroy.cpp` | `B_ENTRY(180026b50, kD_Str_10)` + `RegisterRuntimeAddr(0x180026b50, …)` + the derivation table above + a lane identity marker |
| `runtime/src/collection/array.cpp` | corrected the `kSpec_PkgEntry` destroy comment; added the two **arm-coverage Sites** for `0x1800298f0`'s two destroy branches |
| `runtime/src/collection/array_destroy_internal.h` | `kD_Str_10` now names both addresses |
| `runtime/src/collection/array_internal.h` | `kSpec_PkgEntry` declared so the probe can drive it |
| `runtime/src/collection/array_destroy_probe.cpp` | a `RunB` row against the shipped `0x180026b50` (+ its negative), and `RunShrinkArm` — the end-to-end shrink-tail exercise |

No new `DestroySpec`. No new Runtime address beyond the one registered.

---

## 4. Both arms, counted INSIDE the branch

`0x180026b50` has two call sites in `0x1800298f0`: the realloc arm
(`0x180029a0e`, whole old array) and the shrink-with-no-realloc arm
(`0x180029a68`, tail only). **Nothing outside `SetSizeCore` can tell them
apart** — the callee is handed `(base, count)` in both cases — so the count is
taken at the branch itself, one `RtDiffSite` per arm, so per-site `calls=` *is*
the arm count.

Measured, run `destroy-neg4`, DLL present, `BANISHED_RT_DIFF=1`:

```
[RtDiff] kSpec_PkgEntry destroy arm: REALLOC (0x180029a0e -> 0x180026b50)      calls=16   divergences=0
[RtDiff] kSpec_PkgEntry destroy arm: SHRINK-TAIL (0x180029a68 -> 0x180026b50)  calls=3    divergences=0
[RtDiff] Collection::Array elem-destroy(String) @0x180026b50                   calls=20   divergences=0
[RtDiff] Collection::Array::SetSize @0x1800298f0                               calls=5848 divergences=0
```

* `5848` independently reproduces the stated `ReadPackfile` figure.
* **REALLOC = 16 live calls.** That is the whole of `0x180026b50`'s live
  population: `ReadPackfile` grows the entry array and never shrinks it.
* **SHRINK-TAIL = 0 live.** Reported as a negative, not papered over. All 3
  counted calls come from the probe.
* Entry point `20 = 16 (realloc) + 3 (probe shrink) + 1 (the negative control's
  wrong-base call)`. The arithmetic closes.

### `RunShrinkArm` — the arm with no live population

The shrink arm hands the helper `old + newSize*stride` and `oldSize - newSize`:
a base that is **not** the buffer start and a count that is **not** the size. A
transcription that got either wrong would still be a correct destroy of
*something*, so the arm is driven explicitly.

It needs **no element assign**, which is what makes the comparison total: with
`newSize < oldSize` and no realloc, `SetSizeCore`'s copy/fill loop runs from
`oldSize` to `newSize` and never executes — no `AssignPkgEntry`, no `Filename`
copy ctor, no `MemAlloc`. Every byte is either seeded by us or written by the
destroy, so unlike the realloc arm there is **no uninitialised tail to exclude**
and the buffers are compared whole.

Two independently built subjects (a destroy frees memory, so an ours-then-theirs
replay on one subject double-frees), driven by our `SetSizeCore` and by the
**shipped `0x1800298f0`**, compared on: header, every byte of the whole buffer
(retained head *and* destroyed tail), slot class per element, and the refcount
of every block seeded to survive.

Cases `9->3`, `4->1`, `3->0`. Result: **3 cases, 12 elements, 0 mismatches.**

---

## 5. Probe result and both controls

```
[RtArrayDestroyProbe] cases=64 did-work=44 mismatches=0
[RtArrayDestroyProbe] freed=108 refcounts-decremented=76 reached-zero=41 survived=35
[RtArrayDestroyProbe] 0x1800298f0 SHRINK-TAIL arm: 3 cases, 12 elements destroyed via base=old+newSize*stride
[RtArrayDestroyProbe] negative: run=7 caught=7
```

* **Effectiveness:** 44 of 64 comparisons changed something; the 20 that did not
  are the `n==0` early-return cases, counted as ineffective on purpose. All three
  of Body B's arms fired (freed / survived / already-NULL).
* **Negative control, 7 of 7 caught**, and two of the seven are this lane's:
  * `NEG:0x180026b50` — the release arms swapped. Proves the comparator can fail
    *on this address*, not merely on a sibling.
  * `NEG shrink-arm` — destroys the **head instead of the tail**. Same helper,
    same count, wrong base. Every refcount in the buffer still moves and the
    frees still balance, so a comparator that only counted frees would pass it.

### A defect this lane's own harness produced, and how it presented

The first `RunShrinkArm` cleanup used **one** "was it destroyed" predicate for
both sides. That is right for ours and wrong for theirs: the shipped body always
destroys the TAIL, so under the negative control every refcount-1 block in
theirs' tail was freed by the shipped body and freed **again** by the cleanup.

It did not fault in the probe. It faulted three frames later inside the DLL
allocator's own `Free`, on the next unrelated allocation
(`ACCESS_VIOLATION`, `Runtime-x64.dll + 0xC74EA`, `WRITE of 000002090002118e`),
which reads exactly like a port defect in whatever ran next. **A harness
manufacturing its own crash** — `reference_longjmp_unwind_runs_retail_cleanup.md`
one rung over. Fixed by giving the two sides two predicates; the reason is
written into the loop so a tidy-up does not merge them back.

---

## 6. Identity gate

The on-disk md5 says what is on disk when you look, not what ran. This lane's
gate is a line **its own build emits**, read from the run's own log:

```
[RtPort]   Collection::Array destroy (lane DESTROY): 0x180026b50 registered, built Sep  8 2026 13:03:10
```

It names the **address** rather than a count of this file's registrations, for
lane PKG2's reason one level down: a count moves whenever any other lane adds a
row to the same init, so it would fire falsely on a good run and pass silently on
a bad one where two changes cancelled.

**One honest caveat.** `__DATE__ __TIME__` here is `array_destroy.cpp`'s
compile stamp, so it identifies **that translation unit**, not the whole exe —
with ccache, a later commit touching only `array_destroy_probe.cpp` leaves the
stamp unchanged. That is the right granularity for "did my registration land",
and it is the wrong thing to read as "this is the newest exe". The two
independent confirmations that the *probe* changes were in the measured run are
the `SHRINK-TAIL arm: 3 cases` line (which does not exist in the previous build)
and the absence of the crash that build produced.

---

## 7. Control: the kill switch

Run `destroy-killswitch`, `BANISHED_RT_ARRAY_DESTROY=0`, everything else equal:

* the identity line is **absent** — the registration really is gated, and the
  marker really is downstream of it;
* the two `kSpec_PkgEntry` arm Sites still appear (they live in `SetSizeCore`,
  not in the gated file), with the destroy going to the DLL;
* the boot completes identically.

### The one divergence in both runs is NOT this lane's

```
[RtDiff] Object::Factory::Startup [probe, not diffable]  calls=1  divergences=1
```

It is present with the destroy registrations **on** (486 sites, 15,413,128
calls, 1 divergence in 1 site) and with them **off** (476 sites, 17,869,332
calls, 1 divergence in 1 site). Pre-existing, out of lane, and the WAVE9.md
`Factory::Startup` self-check entry is where it belongs. Every other site in
both runs is `divergences=0`.

---

## 8. Address-door sweep

`tools/addr_door_audit.py`:

```
before   UNREGISTERED 42   (registered 1125 = 596 literal + 529 mint-supplied)
after    UNREGISTERED 41   (registered 1126 = 597 literal + 529 mint-supplied)
```

Set-diff of the unregistered lists, whole output:

```
13d12
< 0x180026b50
```

**Exactly one address closed, and it is the one this lane ported.** No other
door moved in either direction — worth stating, because a registration wave that
quietly changes an unrelated row is how a shadowed address gets in.

### The rest of the destroy-range family is already closed

The eleven destroy-helper addresses lane D2 identified are `0x18001ce90`,
`0x1800419b0`, `0x180052110`, `0x180074100`, `0x18005bce0`, `0x180011010`,
`0x180005930`, `0x180014a40`, `0x180034440`, `0x1800059e0`, `0x18005c550`.
**None of them appears in the 41 remaining unregistered doors.** Five are
registered because our tree reaches them by address; the other six are ported
but deliberately unregistered because nothing in our tree calls them, and
registering a port with no caller dilutes the one signal this wave has for
finding a dead port. That reasoning is unchanged and this lane did not disturb
it.

### DECLINED, with the reason

**`0x180051650`** (`array.cpp:427`, `kSpec_EffectSlot`'s element copy-assign).
It is in the same file and it is the nearest thing left to this family, so it
needs an explicit refusal rather than silence.

* It is **not a destroy-range** — it is the element *assign*, so it is outside
  this lane's subject in the first place.
* Its disassembly contains exactly one `CALL`, to **`0x180050d30`** — Audio, and
  a Wave 8 Audio holdback (itself unregistered at `audio.cpp:1114`). Registering
  `0x180051650` would put a live edge into the held-back Audio cluster on the
  strength of a lane that read neither side of it.
* Lane Q and lane D2 both left it where it is for this reason. Nothing this lane
  measured changes that, so it stays.

Everything else in the 41 belongs to `io`, `input`, `graphics/scene`, `audio`,
`ui`, `engine` or `engine_funcs.cpp` — other lanes' subjects, and none of them a
destroy-range helper.

---

## 9. Instruments re-run, clean

* `tools/port_bypass_audit.py` — ADDRESS door: 299 sites, **0** reach a function
  already ported by name; NAME door: 469 raw-door sites, **0** on a live path.
* `tools/export_name_audit.py` — 666 distinct names, 1 not in the DLL, the
  pre-existing documented `?_singleton@…` in `rt_resolve_cache.cpp`'s comment.

---

## 10. Kill switches

Unchanged from lane D2, and the new registration sits under both:

```
BANISHED_RT_COLLECTION=0              the whole Collection lane
BANISHED_RT_ARRAY_DESTROY=0           only array_destroy.cpp's addresses (now 19)
BANISHED_RT_ARRAY_DESTROY_PROBE=1|neg the oracle, off by default
```

---

## 11. For the integrator

`0x180026b50` was the attempt-17 fault address and it is now registered, so
attempt 18 should get past it. **This lane did NOT run a DLL-less boot** — the
rename is global to the one shared game directory and other lanes were running.
What is verified is DLL-present equivalence against the shipped body, on both
call sites, with a negative control on each.
