# WAVE 22 CLOSED — System, and an instrument that could not say whether it had run

Opened 2026-09-11 on `wave22/base` at `37706651`, five lanes in private worktrees:
ALLOC, TIMER, ERRCFG, SYSBODY, GEOM. **All five reported. All five merged. Build clean.**

---

## 1. THE NUMBER — measured with the SAME TOOL on BOTH SIDES

```
                         base 37706651        merged tip
TOTAL       4632      1313 called 1128      1314 called 1136      85.9% -> 86.5%
System       262       103 called  76       103 called  84        73.8% -> 81.6%
gap rows                        185                    178
body rows                52 of 185 (28%)        44 of 178 (25%)
detour                            7                      7
```

**Both columns were produced by the CORRECTED census**, run against each tree. That matters
more than the movement: lane ALLOC fixed the census mid-wave, so the pre-wave figure everyone
was quoting (`1313 / 1129`, taken with the old tool) is **not** comparable to anything measured
after. Running the same tool on both sides is the only reason the `+8` above means work rather
than tooling (`reference_same_tool_both_sides_is_not_enough.md`).

**Wave 22 moved System from 76 to 84 ported.** Every one of those eight is a body or a
registration, not a re-count.

### What this close does NOT claim
- **No row was promoted.** Four of five lanes reached **zero promotions** and every one of those
  zeros is evidence-backed, not a shortfall.
- **`BANISHED_RT_ALLOC` is NOT defaulted on.** See §3.
- **The `?_typeInfo@` precondition in §6.2 is unresolved** and gates future promotion work.

---

## 2. THE FINDING — an instrument that could not report whether its own treatment applied

**Every `[RtAlloc]` line reporting the arming outcome was a `printf`, and `standalone-perf`
compiles `printf` to `((void)0)`** (`Makefile:98` → `-include standalone/log_strip.h`, whose
line 29 is `#define printf(...) ((void)0)`; its own comment at line 12 exempts
`fprintf(stderr,...)`). **The one binary an A/B is measured on was precisely the binary that
could not say whether it had armed.** A silent REFUSAL would have produced an A/A comparison
reading as "no regression" — a green result from an instrument that never applied its treatment.

Verified independently by the integrator. Both arms now attest on stderr, and all eight
reported runs named their own arm — that attestation **is** the effectiveness control.

This is the `reference_the_gate_was_never_a_control.md` family, but it is a **new variant**: the
blindness was introduced by the **build configuration**, not by the probe's logic. A probe can
be correct in source and blind in the binary it is measured on.

### 2a. And a confounder 130x the size of the effect, invisible from inside the harness
Three sibling `find /` whole-filesystem scans pinned 3 of 8 cores mid-series (~4,500 CPU-seconds
each). One binary produced 0.21 / 0.46 / 0.46 across consecutive runs. **Ten runs were
quarantined, not salvaged** — correctly, because a paired alternating design cancels only a
*stationary* confounder and this one aliased onto the treatment through within-pair run order.
`stress_run.sh` records nothing about machine load and never said a word. Fixed in CLAUDE.md by
naming the memory paths so nobody reaches for `find /` again.

---

## 3. RECOMMENDATION: DO NOT DEFAULT `BANISHED_RT_ALLOC` ON

4 clean pairs after the box went quiet: armed − unarmed = **−0.002 ms/iter** against a ~0.02
within-arm spread. That is **no resolvable effect, bounded at ±5%** — not a certified gain.

Separately and genuinely positive: one armed `BANISHED_REDZONE=watch` run gave **53,905 heap
walks, zero violations, clean exit**. **The port is SOUND. That is not the same claim as WORTH
ARMING**, and the two are kept apart deliberately. Arming is an atomic process-wide heap swap
with prologue splicing; neutral perf does not buy that risk.

The OOM/modal-dialog hazard — ERRCFG's newly ported `?Display@` sits outside the harness's only
`MessageBoxW` seam, which hooks *Runtime-x64.dll's* IAT while the new body is in the exe — is
recorded as **reasoned, not observed**.

---

## 4. "ELEVEN BODIES" WAS SIX, AND THE PLAN WAS WRONG ABOUT ITS OWN SIXTH ROW

WAVE22_PLAN §1 held line for line: all five `MemoryAllocator` bodies are written and registered
exactly where it says, behind an opt-in gate. **But the plan groups a sixth row with them and
that is wrong** — `?_singleton@MemoryAllocator@` is a **mint** row, not a gated registration;
arming neither registers nor moves it. The plan was the integrator's to write and it was wrong
on that point.

---

## 5. WHAT EACH LANE ESTABLISHED

| lane | rows | result |
|---|---|---|
| **ALLOC** | 5 BODY + 1 DATA | 0 promoted. Fixed the census **in both directions on its own subject**. Recommendation: do not arm |
| **TIMER** | 5 DATA | 0 promoted, **correctly** — promotion would strand `SleepThread`, `MainLoop::Draw`, `Video::Finish` on a frozen DLL copy: IDIV-by-zero on the next idle frame |
| **ERRCFG** | 2 BODY + 1 DTOR + 5 DATA | **3 bodies ported.** Refuted TIMER's rule for 3 of its 5 rows by answering each on its own xrefs |
| **SYSBODY** | 5 fn + 1 VTABLE | **5 rows moved.** `?StrCompare@` had a correct body since Wave 2 and read unported for **twenty waves** |
| **GEOM** | the cutover blocker | Discharged. Both sides measured at **9 slots**; the `12` had no basis in either binary |

### 5.1 The census was wrong in BOTH directions, on the wave's own subject
- **Understated by one** — `0x180009c30`, registered unconditionally inside a file-level-gated file.
- **Overstated by two** — `?Allocate@`/`?Free@`, reachable by no path unarmed. A **FOURTH gating
  shape**: a guard clause (`if (!g_swapTaken) return;`) at the top of the only registrar.

**Integrator ruling: the count moves.** Five `MemoryAllocator` rows are excluded from `ported`
precisely because a gated registration is not a port; counting these two while excluding their
five siblings is not conservatism, it is an inconsistency that flatters the number. Verified
before ruling (`rt_alloc_swap.cpp:382`; `allocator.cpp:1371`/`:1428` are the cutover and ARMED
branches). Gated on **both** the registration and detour sides, because a detour target confers
`ported` and gating one alone would have moved nothing while looking like it had.

New `GATED-BUT-WRITTEN GAP ROWS` section turns the plan's §1 grep into a printed line:
**7 of 178 gap rows (4%) are written and held back by a gate.**

---

## 6. TWO FACTS THAT OUTLIVE THE WAVE

### 6.1 A stale WARNING and a stale PRESCRIPTION, in one wave, mirror images
- **GEOM** found `statusicon_geometry.cpp` carrying a prescription that read as a completion —
  the comment named the fix (`i < 9`) and argued for it while the code still said `12`. It had
  talked at least two prior readers out of acting since Wave 6.
- **ERRCFG** found `error.cpp` §2 carrying a completion that read as a prescription — a ⛔
  "must not be armed until lane C2 widens the mint row from 8" when the row has read 8192 for
  some time. Verified independently by the integrator.

**A blocking warning that no longer applies is worse than no warning**, because the next lane
obeys it and stops. Neither has a detector. Both were corrected **in place with date and
evidence**, not deleted.

### 6.2 `kTypeInfoStorage` is never written — a precondition on 104 rows
Reported by Wave 23 lane ACHIEVE, **mechanism verified by the integrator**:
`rt_data_mint_tables.inc:1607` declares `BaseTypeInfo kTypeInfoStorage[104]`, zero-initialised;
every reference is a range test or address-take; and `RtMintPromotionStorage`
(`rt_data_mint.cpp:793-814`) validates symbol and VA then returns `r.storage` **without seeding
it**. So **any `?_typeInfo@` promotion, in any namespace, publishes a zeroed `BaseTypeInfo`.**

Rows are already promoted on that path and UI is not broken, so the coherent reading is that
those promotions are **inert — promoted but never dispatched**. That is a hypothesis with
corroboration, **not a measurement**. It must be settled before further `?_typeInfo@` promotion.
**ADDRESSEE: the Wave 24 kickoff writer**, as a kickoff item, not a lane task.

---

## 7. HANDOVER — each item has a NAME, not a condition

1. **`?LoadResources@UpdateLoop@Engine@@` (`0x1800dd7d0`)** plus an 8-byte accessor are the
   *complete* unported reader set of `?_singleton@Configuration@System@@`. One body unblocks a
   DATA row in another namespace. **ADDRESSEE: Wave 23 lane ENGINE** (dispatched; carry forward
   if it does not land).
2. **`?_typeInfo@` zeroed-storage precondition** — §6.2. **ADDRESSEE: Wave 24 kickoff writer.**
3. **Nothing auto-discovers a gate.** All four detectors are hand-declared, so every undeclared
   gate reads clean. `BANISHED_RT_DLGFACTORY` (Wave 23 lane OBJ) is the **second instance in two
   waves in two namespaces: it is a class.** **ADDRESSEE: Wave 24 kickoff writer.**
4. **`stress_run.sh` records nothing about machine load.** Two measurement sessions were
   destroyed by neighbouring processes and the harness never reported it.
   **ADDRESSEE: Wave 24 kickoff writer.**
5. **`??_7Thread@System@@6B@`** must be registered **at cutover, not before**.
   **ADDRESSEE: the integrator**, carried here rather than invented as a fictional lane name.
6. Wave 20 items 7, 9, 10 remain `[NOT RE-DERIVED]`. Item 9 (`?PlatformConfigure@Engine@1@`) is
   dispatched to Wave 23 lane ENGINE and its three-wave silence ends there either way.

---

## 8. THE RULE THIS WAVE ADDS

**Re-check a stated blocker before acting on it — including your own, however recent.**
Lane SYSBODY discharged a nine-wave-old holdback that was false by mechanism, then **retracted
two of its own residuals that were one turn old**, and retracting them required re-checking them
— which is the only reason it found them false. Wave 23 lane ACHIEVE then found a
**fourteen-wave-old** blocker whose discharge had been sitting in the same directory, in
identical words, for two sibling functions. **A discharge does not propagate. Nothing carries it.**
