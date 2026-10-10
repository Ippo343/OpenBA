# WAVE 23 CLOSED — ten lanes, and the body gap halved

Opened 2026-09-11 on `wave23/base` at `a60351e3`. Ten lanes, private worktrees:
GFXBODY, GFXVT, GFXDATA, ACHIEVE, VIDVT, IOPORT, OBJ, ENGINE, TAIL, INTERNAL.
**All ten reported. All ten merged. Build clean. Checkers green.**

---

## 1. THE NUMBER — same tool both sides, measured on the merged tip

```
                       base a60351e3         merged tip
TOTAL      4632     1314 called 1136      1319 called 1159     86.5% -> 87.9%
gap rows                     178                   160
BODY rows          44 of 178 (25%)       22 of 160 (14%)
detour                         7                     8
```

**The headline is not 87.9%. It is 44 -> 22.** The remaining work that is *a function to
reimplement* halved. Both columns were produced by the **same (Wave 22 corrected) census**, run
against each tree — the Wave 23 lanes themselves branched before that fix and their in-doc
figures are therefore on the older tool and **are not comparable to this table.**

| namespace | base | merged |
|---|---|---|
| Graphics | 100 / 75 (75.0%) | **101 / 80 (79.2%)** |
| IO | 216 / 200 (92.6%) | **216 / 202 (93.5%)** |
| Achievement | 52 / 36 (69.2%) | **52 / 43 (82.7%)** |
| Engine | 33 / 23 (69.7%) | **33 / 25 (75.8%)** |
| Audio | 30 / 23 (76.7%) | **30 / 25 (83.3%)** |
| Input | 29 / 25 (86.2%) | **29 / 26 (89.7%)** |
| Video | 159 / 140 (88.1%) | **159 / 141 (88.7%)** AT CEILING |
| **Save** | 30 / 29 (96.7%) | **30 / 30 (100.0%)** |

`called` grew 1314 -> 1319 and every increment was **declared by the lane that caused it**.
Porting a body that reaches DLL internals by address grows the cutover surface; lane GFXBODY
measured its own cluster growing it by **zero**, and separately noted Graphics BODY went
`8 - 3 + 1`, not `8 - 3`, because porting `AnimationManager::Update` pulled
`AnimationState::Update` into `called`.

### The tag population moved further than the rows
```
RT_UNPROMOTABLE expiries:   1 scoped / 1 addressed / 40 grandfathered
                        -> 37 scoped / 37 addressed /  9 grandfathered
                           0 MISSING, 0 malformed, 0 STALE
```
No lane was asked to do this. OBJ, VIDVT and TAIL each did their own rows.

---

## 2. THE CUTOVER WALL — three waves of silence, and the mechanism was never written down

`?PlatformConfigure@Engine@1@` has been `[NOT RE-DERIVED]` since Wave 20. Lane ENGINE closed it.

**It is not a DLL feature we must reproduce. The EXEC-of-0 is inside OUR code.**
`engine_lifecycle.cpp:710` is our own ported `Engine::Startup` calling `PlatformConfigure`
through `RT_EXPORT_ONCE`, which is null with the DLL gone — **verified independently by the
integrator**. It is also the *first* unsatisfied door in `Startup`; everything else in that file
is in `Shutdown`.

**The wall moved, by a measured amount.** `PlatformConfigure` splits:
- **Half A** (`configure==false`, and the branch that declines the dialog) had every dependency
  already ported. **Ported.** It is the only half an unattended boot takes — measured
  DLL-present at `calls=1 dialog-skipped=1`, clean boot, exit 0.
- **Half B** needs Win32 dialog templates `0x66`/`0x68` and bitmap `0x2711` from the DLL's
  `.rsrc`, three unexported dialog procs, four unexported statics. Not portable in one lane.
  It now **refuses by name at cutover instead of jumping to 0.**

**No DLL-absent boot was run, so the next wall is NOT named.** That is residual, not result.

---

## 3. THE NEGATIVE RESULT WORTH MORE THAN ANY PORT

**The screenshot pixel oracle does not work, and CLAUDE.md says it does.**

Lane GFXBODY ran five runs at identical seed, camera pin and command line. They split into two
clusters **~98% apart**, and **the split is independent of the flag under test**: two *armed*
runs differed by 97.98%, while an armed and an unarmed run differed by 0.79%.

`BANISHED_SEED` + `BANISHED_CAMERA_PIN` are **not sufficient for a reproducible frame.**
CLAUDE.md's "Camera Pinning (deterministic screenshots)" section reads as though they are.
**Corrected in CLAUDE.md by this close.**

This invalidates a proposed instrument before it was built, and it **blocks every pixel-level
verification**, including Wave 21 lane SHOT's reason to exist.

---

## 4. WHAT EACH LANE ESTABLISHED

| lane | moved | result |
|---|---|---|
| **GFXBODY** | **5 of 10** | all five exercised on a live in-world run; a rebuild reproduces the run exe's md5 exactly |
| **ACHIEVE** | **7 of 16** | a **14-wave-old** blocker was stale and its discharge already sat in the same directory, in identical words |
| **TAIL** | **6 of 18** | Save to 100%. Then closed its own self-flagged sweep gap: 22 classes, 0 divergences |
| **IOPORT** | **2 of 16** | `?SetupFileSystem@` is **not dependency-blocked** — 13 of 16 callees ported. Two waves of "unscoped" implied a callee wall that does not exist |
| **ENGINE** | **2 of 10** | §2 |
| **VIDVT** | **1 of 19** | Video has **no body work left**; the twelve `*Data` tables are one-slot abstract bases, every other slot one shared `_purecall` |
| **INTERNAL** | instrument | the 42 unknowns are **30 unexported functions + 12 statics, all already named in our own source comments.** Real work: **8 bodies** |
| **OBJ** | 0 of 10 | correct — but **4 of 10 tags INVERT at cutover** and none said so |
| **GFXVT** | 0 of 8 | **readiness and value are ANTI-CORRELATED**; the sorting axis is the purecall count |
| **GFXDATA** | 0 of 7 | correct — **promotion safety is governed by unported READERS, not writers** |

**Five lanes promoted zero rows and every zero is evidence-backed.** That is the wave working.

---

## 5. THE INSTRUMENT LESSONS — six lanes caught bugs in their OWN tools

1. **TAIL, and the sharpest rule of the wave:** *"When a tool's output supports the conclusion
   you already expected, you have received no evidence that the tool works."* Its census-derived
   reader classification was wrong on **seven** rows, and **every error pushed toward the
   conclusion it reached anyway** — a wrong instrument agreeing with the right answer leaves no
   symptom. Caught by hand-checking one row *while the tool was agreeing with it*.
2. **TAIL again — a green control can leave most of the instrument untested.** Control 1 passed
   **22/22** while proving `_width` and nothing else: with `_width` corrupted the comparison
   short-circuits *before* the entry loop, and that run printed `entry-fields compared=0`. The
   248 entry-field comparisons were validated by nothing. Only the **effectiveness counter**
   exposed it. This is beyond "a control must pick a compared subject".
3. **ENGINE refuted its own claim.** It wrote *"a green detoured boot IS the test"*; its own
   negative control drove `ownList` 3 -> 0 and the game still built a 24-citizen world and exited
   0. **NOT CAUGHT** — so the probe is blind, not the instrument. Corrected in both banners.
4. **GFXBODY's oracle controls caught a defect in themselves** — 3 of 4 fired, but all three
   fired on the same one of two screens, leaving the byte-compare screen with **no control**.
5. **OBJ's near-miss the controls could not reach.** Its reader classifier keyed on `REX.W`, so
   two rows' real readers (non-REX `MOV EAX,[rip]; RET`) fell into an `other` bucket. **Reading
   only the totals would have reported "0 readers".** Found by reading the unclassified bucket.
6. **IOPORT's first reader scan was wrong by 20x** — objdump emits AT&T syntax here, so an
   Intel-syntax WRITE regex matched nothing and every site fell through to READ. **VIDVT's**
   decoder had an off-by-one rejecting the commonest `mov` form. Both caught by positive
   controls; both would have shipped as confident under-counts.
7. **INTERNAL: `Runtime-x64.dll` is `/GR-` — RTTI is OFF.** Any vtable screen built on RTTI has
   no positive case in this binary and can only return 0, which reads as "none are vtables".
   **Barred from driving a verdict, anywhere in this project.**
8. **INTERNAL: the `.pdata` trap bites the OPPOSITE way from how it is recorded.** The *entry*
   fragment is the one reading `chained=False`, so a chained function looks complete.
   Understatement up to **20.4x**. Fix: union *forward*. Also **52.9% of `.text` exports have no
   `.pdata` at all** (leaf functions).
9. **A build check cannot detect a misregistered port** (ACHIEVE's phantom-mangling control was
   caught by the census, not the build, which stayed clean and linked).

---

## 6. THE DEFECT CLASS THIS WAVE NAMED: A DISCHARGE DOES NOT PROPAGATE

Four independent instances, three lanes, one wave:
- **ACHIEVE** — a Wave 5 holdback ("no runnable oracle of any kind") discharged by Wave 9 in
  *identical words* for two sibling functions, in a file whose banner says so in its first
  paragraph. **Fourteen waves.**
- **TAIL** — the Wave 8 Audio holdback was an *oracle* verdict whose dependency had been ported
  **nine waves** earlier.
- **ENGINE** — `engine.cpp:173-182` states a guard that is not implemented.
- **IOPORT** — `?SetupFileSystem@`'s implied callee wall does not exist.

Together with Wave 22's mirror pair (GEOM's prescription-read-as-completion, ERRCFG's
completion-read-as-prescription), that is **six instances in two waves**. Nothing carries a
discharge forward, and nothing detects a stale blocker. **Re-check a stated blocker before
acting on it — including your own, however recent.**

---

## 7. HANDOVER — every item has a NAME

**To the Wave 24 kickoff writer:**
1. **`?_typeInfo@` promotions publish a ZEROED `BaseTypeInfo`** — `kTypeInfoStorage[104]` is
   never written and `RtMintPromotionStorage` returns storage without seeding it (mechanism
   verified by the integrator). Rows are already promoted on that path and UI is not broken, so
   the reading is that they are **inert — promoted but never dispatched**. A hypothesis with
   corroboration, **not a measurement.** Settle it before any further `?_typeInfo@` promotion.
2. **Frame non-determinism (§3)** blocks all pixel-level verification.
3. **Nothing auto-discovers a gate.** All four detectors are hand-declared. `BANISHED_RT_ALLOC`
   and `BANISHED_RT_DLGFACTORY` in two waves in two namespaces: **a class, not a quirk.**
4. **`?Draw@Renderer@` + `?BuildParameters@`** as one lane. Explicitly **not** a cheap win.
5. **Both Graphics `Serialize` rows belong with IOPORT**, not Graphics — same Serializer problem.
6. **`??_7MaterialInstance@`**: "four slot bodies" is right in count, **wrong in cost** —
   `Serialize` dispatches through the Encoding vtable at `+0xa0`, calls four unexported
   internals, and calls `BuildParameters`. Three of four unblocks nothing; the mint needs all
   nine. Scope it with item 5.
7. **Name the probe shape in the kickoff** so ten lanes do not each independently conclude "no
   run, therefore STRUCTURAL".
8. **`stress_run.sh` records nothing about machine load** (Wave 22).

**To the integrator / next cutover attempt:**
9. **Cutover attempt 25** — ENGINE moved the wall but did not run a DLL-absent boot.
10. **The ten Object rows are a cutover-publish list**; **six Video rows flip HARMFUL ->
    REQUIRED** at cutover; **`??_7Thread@System@@6B@`** registers at cutover, not before.
11. **One armed `BANISHED_RT_DIFF=1` run on the merged tip** covers every lane's oracles at once
    (IOPORT R4) — the single highest-value action available, and it needs one game-directory slot.
12. **9 grandfathered expiries** still declare neither scope nor addressee.
13. **`?Setup@MaterialInstance@Graphics@@` needs a DETOUR, not a registration**, to take
    retail's rel32 traffic — otherwise it and `SetupShaders` stay dead in play.

---

## 8. MECHANICS — what ten parallel lanes actually cost

**Two conflicts, both in files many lanes touch, both resolved by keeping BOTH sides.**
`reg_mint_promoted.cpp` (OBJ's re-derived Object rows vs TAIL's corrections and four new tags)
and `main.cpp` (ENGINE and GFXBODY each appending a summary call).

**One defect was visible ONLY at integration:** two of TAIL's scope fields were written as prose,
which nothing rejected in its own worktree, because the canonical vocabulary is enforced by
checker work that arrived from a *different* lane. Canonicalised with TAIL's original wording
preserved beside the token.

**The game directory served four lanes, not one.** ALLOC, then ENGINE, then VIDVT, then GFXBODY —
each acquiring **bare** (never piped) and releasing. Six lanes got no run and **said so plainly**
rather than hedging, which the kickoff named as an acceptable outcome up front. TAIL then proved
the distinction matters: its sweep needed the **DLL but not the game directory**, so the lock
never excused that gap — and it closed it when told.

**Three integrator corrections were made by lanes, not by the integrator:** the lock holder was
ENGINE/VIDVT and not ALLOC; the `UNMATCHED ??_7ReaderWriter@` line is the checker **working as
designed** (the freeze is on the `(symbol, exact field)` pair, so a correct re-derivation
necessarily unmatches) rather than a stale artefact; and the Video `*Data` tables are Runtime's,
not `dx11/`'s.
