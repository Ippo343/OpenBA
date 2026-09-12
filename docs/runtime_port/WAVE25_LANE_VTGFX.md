# WAVE 25 LANE VTGFX — vtable promotion, graphics side

Branch `wave25/vtgfx`, cut from `wave25/base` and rebased onto `5b848241` (the
perf-build `std::printf` fix). Private worktree
`.claude/worktrees/agent-a5033e534465681c7`. No game-directory lock — by design
(kickoff §1d).

**Every census figure in this document is BRANCH-LOCAL**, measured in this
isolated worktree. Wave 21 published a wave number assembled from seven such
figures and had to correct itself in both directions. The integrator must
re-derive on the merged tip.

---

## 0. HEADLINE

**One row promoted: `??_7MaterialInstance@Graphics@@6B@`. Eleven held.**

That is not a thin result, it is the result the sorting rule predicts.
Re-derived from the shipped bytes this lane: of my twelve rows, **three are
COMPLETE and only one of those three is CONCRETE**. The other two COMPLETE rows
are abstract bases whose table is overwritten one statement after it is stamped
— promoting them moves a pointer nothing dispatches through.

Three findings that outlive the lane:

1. **The anti-correlation reproduces in a SECOND namespace, and it inverts the
   obvious target.** Wave 23 GFXVT established it for Graphics. In Achievement,
   the row that is CHEAPEST to complete (`??_7Goal@` — 3 slots short, and all
   three are the two-instruction `TypeName`/`Type`/`GetFileVersion` accessor
   family) is **ABSTRACT**, so completing it buys nothing. §4.2.

2. **`??_7Achievement@0@6B@` is slot-for-slot IDENTICAL to
   `??_7MaterialInstance@Graphics@@6B@`** — the same nine-slot `Object::External`
   layout, the same shared `?Compile@External@Object@@` in slot 4. It is
   CONCRETE, 5 slots short, and Wave 24 GFXMAT has already written the recipe
   for the three hard ones (Construct/Reconstruct/Destruct) against the twin
   row. **That is the best-priced promotion target left in either namespace.**
   §4.3.

3. **Every promoted vtable in this tree publishes whatever the live port
   registry happened to contain, and nothing checked it.** Three wave kill
   switches (`BANISHED_RT_GRAPHICS`, `BANISHED_RT_GFXTI`, `BANISHED_RT_GFXMAT`)
   empty this row's slots at runtime; the build stays clean and links, and the
   first virtual call aborts. `RtMintVtableTrapCount()` (new, §6) lets a
   promotion **refuse itself**. The five vtables promoted in Waves 19–20 have
   the identical exposure and no guard. §6.2.

---

## 1. RE-DERIVATION OF THE BOARD — the kickoff is exact

```
cd <worktree>
tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py --unported
```

Before a line was written, at `fc56353c`:

```
TOTAL   4632   1323 called   1171 ported   9 detour   88.5%
=> 16 of 152 gap rows (11%) are a function to reimplement.
Graphics 19 | BODY 4 VTABLE 8 DATA 7      Achievement 9 | BODY 1 VTABLE 4 DATA 4
```

**Every figure in kickoff §0 reproduces character for character.** Nothing in my
inputs was stale at the board level.

### 1.1 §0b re-derived: NOT ONE of my twelve rows carries a tag

The kickoff warns that 5 of the 19 Graphics gap rows are tagged `UNPROMOTABLE`
and that registering a `HARMFUL` row breaks a DLL-present run with no build
symptom. **I re-derived which five**, by reading the census's own tag text and
`standalone/reg_mint_promoted.cpp`'s `RT_UNPROMOTABLE:` block rather than
inferring from the count:

```
?AddDrawItems@MeshInstance@Graphics@@       UNPROMOTABLE(HARMFUL)   BODY row
?_frameIndex@ParticleInstance@Graphics@@    UNPROMOTABLE(HARMFUL)   DATA row
?_rng@Particle@Graphics@@                   UNPROMOTABLE(HARMFUL)   DATA row
?_rootOnlyUpdates@MeshInstance@Graphics@@   UNPROMOTABLE(HARMFUL)   DATA row
?_singleton@AnimationManager@Graphics@@     UNPROMOTABLE(HARMFUL)   DATA row
```

**All five are BODY or DATA rows. Zero of the eight Graphics VTABLE rows is
tagged, and zero of the four Achievement VTABLE rows is tagged.** So no
adjudicated verdict was re-opened by this lane and no tag was touched, added or
edited. `tools/unpromotable_slot_check.py` reads identically before and after
(§7).

---

## 2. EXTENT — re-derived from the shipped bytes, with the bound kind stated

`tools/vtgfx25_bound.py` (new this lane) walks qwords forward from an exported
table start and classifies each by whether it points into `.text`, read from the
section table rather than a constant. It says **which bound bit**, because the
two are not interchangeable and four waves have read the soft one as exact
(`reference_adjacent_export_is_an_upper_bound.md`).

### 2.1 Controls, printed on every run

```
OK   POS   MaterialInstance, GFXVT read HARD 9    extent=9   HARD(tail-not-a-pointer)
OK   POS   SunLight,         GFXVT read HARD 11   extent=11  HARD(tail-not-a-pointer)
OK   NEG   start inside the "ImageBuf" string     extent=0   HARD(tail-not-a-pointer)
OK   SHIFT Geometry + 1 slot, expect 8            extent=8   HARD(tail-not-a-pointer)
CONTROLS: 4 of 4 green
```

The NEG and SHIFT cases are what make the POS cases mean anything: the walker can
say "not a table" and it notices a one-slot shift. **The LIMIT is restated by the
tool itself and is Wave 23 GFXVT's**: SHIFT returns a plausible `8` with no
complaint, so this measures an extent FROM A GIVEN START and cannot validate a
start. Every start here came from the export directory.

### 2.2 Six of the twelve rows rest on the SOFT bound — and a second channel

The byte walk could not harden six rows: the pointer run does not stop before
the next export. So a **different question** was asked of the same bytes — *does
a second vtable BEGIN inside this run?* Every MSVC vtable in this DLL starts with
an UNEXPORTED scalar-deleting destructor while every other slot of these
`Object::External`-family tables is an exported method. A swallowed table
therefore shows up as an unexported slot at an INTERIOR index.

That is an absence claim, so it is handed a positive shape
(`reference_absence_screen_must_be_handed_a_shape.md`): each row's own
**neighbour**, a real second table beginning at a known index.

```
row                       extent  bound   CONTROL fires at   SUBJECT interior
??_7Achievement@0@          9     SOFT    [9]   correct      none
??_7AchievementInstance@    4     SOFT    [4]   correct      none
??_7Goal@Achievement@      10     SOFT   [10]   correct      none
??_7GoalInstance@           7     SOFT    [7]   correct      none
??_7ParticleInstance@      10     SOFT   [10]   correct      none
??_7ParticleSystemInstance@10     SOFT   [10]   correct      none
```

**6 of 6 controls fire at exactly the known boundary and nowhere else.** No
subject row has an interior candidate. The bounds stay SOFT — this is
corroboration from a second channel, not a byte terminator, and the tool prints
that sentence itself.

### 2.3 An instrument defect the CONTROLS did not catch, found by the tool disagreeing with me

The first draft of the detector reported `??_7Goal@Achievement@@6B@` **SUSPECT at
[6,7,8]** and `??_7ParticleInstance@Graphics@@6B@` **SUSPECT at [1]**. The
controls were green in both cases — a swallowed-table screen scores CAUGHT either
way.

Cause: **`_purecall` (0x18007ec28) is not in this DLL's export directory**, so a
pure-virtual slot also reads as "unexported". The indices are exactly those rows'
purecall slots as counted independently by `tools/vtable_dump.py` (3 and 1), and
it was two channels naming the same indices that identified this as an instrument
defect rather than a finding. Fixed, with the cause written beside the constant.

**Instrument rule 1 in reverse, and it is the only thing that found this:** the
tool disagreed with me, and chasing the disagreement beat retuning the screen.

---

## 3. THE TWELVE ROWS — slot count, purecall count, ported count, dispatch, verdict

Slot counts and purecall counts are **mine**, from `tools/vtable_dump.py` against
`C:\GOG Games\Banished\Runtime-x64.dll` (its `_purecall` control printed FOUND),
cross-checked against the census PROMOTION BACKLOG's `slots` column. The
arithmetic identity **`extent − purecall == backlog slots` holds 12 times out of
12**, which is two channels corroborating each other, not one channel repeated.

Stamper figures are from `tools/mint_stamper_census.py` re-run this lane (decoder
controls 3 of 3, ownership positive + negative both green). `ours` is an **UPPER
BOUND** — a registration is a door, not a guarantee every caller uses it.

| # | row | extent | bound | purecall | concrete slots | ours | COMPLETE | abstract? | stampers ours/retail/unattr | **promotion changes a dispatch?** | **verdict** |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | `??_7MaterialInstance@Graphics@@6B@` | 9 | HARD | **0** | 9 | **9** | **YES** | **CONCRETE** | 10: **5**/5/0 | **YES** | **PROMOTE** |
| 2 | `??_7AnimationEvent@Graphics@@6B@` | 9 | HARD | 5 | 4 | 4 | YES | abstract | 5: 3/2/0 | **NO** (measured no-op) | HOLD |
| 3 | `??_7Geometry@Graphics@@6B@` | 9 | HARD | 3 | 6 | 6 | YES | abstract | 3: 2/1/0 | **NO** + live blocker | HOLD |
| 4 | `??_7Light@Graphics@@6B@` | 11 | HARD | 5 | 6 | 5 | no | abstract | 9: 1/**8**/0 | NO, even if completed | HOLD |
| 5 | `??_7Particle@Graphics@@6B@` | 9 | HARD | 3 | 6 | 5 | no | abstract | 4: 1/3/0 | NO, even if completed | HOLD |
| 6 | `??_7ParticleInstance@Graphics@@6B@` | 10 | SOFT+corrob | 1 | 9 | 5 | no | abstract | 8: 3/5/0 | NO, even if completed | HOLD |
| 7 | `??_7ParticleSystemInstance@Graphics@@6B@` | 10 | SOFT+corrob | 0 | 10 | 5 | no | CONCRETE | 3: 1/2/0 | YES **once completed** (5 short) | HOLD |
| 8 | `??_7SunLight@Graphics@@6B@` | 11 | HARD | 0 | 11 | 5 | no | CONCRETE | 3: 2/1/0 | YES **once completed** (6 short) | HOLD |
| 9 | `??_7Achievement@0@6B@` | 9 | SOFT+corrob | 0 | 9 | 4 | no | CONCRETE | 7: 2/5/0 | YES **once completed** (5 short) | HOLD |
| 10 | `??_7AchievementInstance@Achievement@@6B@` | 4 | SOFT+corrob | 0 | 4 | 1 | no | CONCRETE | 7: 3/4/0 | YES **once completed** (3 short) | HOLD |
| 11 | `??_7Goal@Achievement@@6B@` | 10 | SOFT+corrob | 3 | 7 | 4 | no | abstract | 5: 3/2/0 | NO, even if completed | HOLD |
| 12 | `??_7GoalInstance@Achievement@@6B@` | 7 | SOFT+corrob | 0 | 7 | 3 | no | CONCRETE | 6: 3/3/0 | YES **once completed** (4 short) | HOLD |

**Zero unattributed stampers across all twelve rows.** So **no row in my set is
stamper-blocked**, and the permanent-unpromotability shape
(`reference_a_ported_row_can_be_permanently_unpromotable.md` — an in-DLL rel32
call or a RIP-relative LEA stamp we do not own) **does not occur anywhere here**.
Consequently **Wave 24 SMALLBODY's technique — having our ported ctor do the
stamping, which converted `??_7Stream@Audio@@6B@`'s LEA stamp into a real door —
is INAPPLICABLE to all twelve of my rows.** They are not blocked by a missing
door; they are blocked by unported slot bodies. That is a different problem and
the conversion does not touch it. Stated plainly because the kickoff named the
technique and a lane that tried it here would spend a day proving a negative.

### 3.1 Instrument rule 1, honoured explicitly, once

While `vtable_dump.py` and `rt_data_mint_tables.inc` were agreeing with me about
`??_7GoalInstance@Achievement@@6B@`, I read the seven qwords out of the DLL by
hand with `pefile`:

```
0x180154678: 0x1800020c0 0x180001e60 0x180001e40 0x18009ef70
             0x18009f000 0x18009f010 0x18009f060
```

Byte for byte what both channels claim, in the order they claim. Three channels,
and the hand-check was done **while the tools were agreeing**, which is the only
time it is worth anything.

---

## 4. THE ACHIEVEMENT FOUR — never analysed before, and they change the shape of the set

Wave 23 GFXVT did the eight Graphics rows. **Nobody has ever measured the four
Achievement rows.** Three results.

### 4.1 The Achievement set is BETTER than the Graphics set on the axis that matters

**3 of 4 Achievement rows are CONCRETE** (`Achievement`, `AchievementInstance`,
`GoalInstance`) against **3 of 8** in Graphics. So per row dealt, Achievement
carries roughly twice the promotable value — and it has been triaged as an
afterthought in every wave that named it.

### 4.2 The cheapest row to complete is the one worth least — the anti-correlation, reproduced

`??_7Goal@Achievement@@6B@` is **3 slots short and all three are the trivial
accessor family**: `?TypeName@Goal@`, `?Type@Goal@`, `?GetFileVersion@Goal@` —
the exact two-instruction shape Wave 21 ported eleven of in
`runtime/src/graphics/graphics_typeinfo_accessors.cpp`. An afternoon's work, and
a readiness screen would then read `7/7 COMPLETE`.

**It is ABSTRACT** — slots 6, 7, 8 (`Reconstruct`/`Destruct`/`Construct`) are
`_purecall` — so the finished promotion changes nothing. Wave 23 GFXVT's finding
is not a Graphics accident; it is a property of these class hierarchies, and it
catches the second namespace exactly as hard.

### 4.3 `??_7Achievement@0@6B@` is `??_7MaterialInstance@Graphics@@6B@`, slot for slot

Read side by side from `vtable_dump.py`:

| slot | `??_7MaterialInstance@Graphics@@6B@` | `??_7Achievement@0@6B@` |
|---|---|---|
| 0 | (unexported deleting dtor) | (unexported deleting dtor) |
| 1 | `?TypeName@MaterialInstance@` | `?TypeName@Achievement@1@` |
| 2 | `?Type@MaterialInstance@` | `?Type@Achievement@1@` |
| 3 | `?GetFileVersion@MaterialInstance@` | `?GetFileVersion@Achievement@1@` |
| 4 | `?Compile@External@Object@@` | `?Compile@External@Object@@` *(the same function)* |
| 5 | `?Serialize@MaterialInstance@` | `?Serialize@Achievement@1@` |
| 6 | `?Reconstruct@MaterialInstance@` | `?Reconstruct@Achievement@1@` |
| 7 | `?Destruct@MaterialInstance@` | `?Destruct@Achievement@1@` |
| 8 | `?Construct@MaterialInstance@` | `?Construct@Achievement@1@` |

Identical nine-slot `Object::External` layout, 0 purecall in both, and slot 4 is
literally the same address (`0x1800066e0`) in both tables.

`??_7Achievement@0@6B@` is at **4/9**. Missing: slots 1, 2 (the trivial accessor
pair) and slots 6, 7, 8 — **the exact trio Wave 24 GFXMAT ported for the twin row
this wave**, with the transcription traps already paid for and written down
(`wave24_lane_gfxmat.cpp` §3.1: the two-byte `MOV word ptr [RCX+0x24],DX` store,
and the THREE stamped base vtables that a tidied port would collapse into one).
`?Destruct@` there was **eight bytes**.

**This is a written recipe with a worked precedent on an identical subject.** It
is the best-priced promotion left in either of my namespaces.

### 4.4 What is ported today in the Achievement four, by slot

```
??_7Achievement@0@6B@        4/9  ours: slot0 (by ADDR, deleting_dtor.cpp), slot3, slot4, slot5
                                  missing: 1 TypeName, 2 Type, 6 Reconstruct, 7 Destruct, 8 Construct
??_7AchievementInstance@     1/4  ours: slot0 (by ADDR)
                                  missing: 1 TypeName, 2 Type, 3 Persist
??_7Goal@Achievement@        4/7  ours: slot0 (by ADDR), slot4, slot5, slot9
                                  missing: 1 TypeName, 2 Type, 3 GetFileVersion   (6,7,8 purecall)
??_7GoalInstance@            3/7  ours: slot0 (by ADDR), slot3 Persist, slot5 Update
                                  missing: 1 TypeName, 2 Type, 4 Startup, 6 CheckGoalState
```

Re-derived by one literal grep per slot symbol across
`runtime/ code/ standalone/ engine/ dx11/` (scoped, kickoff §1e), matching each
row's `ours` column exactly.

---

## 5. THE PROMOTION — `??_7MaterialInstance@Graphics@@6B@`

The full warrant is written beside the registration in
`standalone/reg_mint_promoted.cpp`. Summary of what it rests on:

* **9/9, 0 purecall, 0 traps.** All nine registrations re-verified by literal,
  one grep per slot, and every `*_PortInit()` involved runs BEFORE
  `RtDataMintInit()` (`resolve_standalone.cpp:318-406`), so the mint fills all
  nine from the live registry rather than trapping.
* **Wave 23's stated blocker was "4 slots short" and nothing else.** Wave 24
  GFXMAT closed it and left an explicit residual (`WAVE24_LANE_GFXMAT.md` §7 R1)
  naming this promotion as the next step. GFXVT's cutover verdict for this row
  was already **REQUIRED**.
* **Stampers moved 3 → 5 ours-capable since Wave 23** (GFXMAT's `Reconstruct`
  and the slot-0 deleting dtor). 0 unattributed. The 5 retail stampers are
  in-DLL and are not doors, so the honest reading is a **MIXED population**:
  some MaterialInstance objects carry our table, some carry retail's. Both are
  the same nine methods; that is a coverage limit, not a hazard.
* **ATTENDANCE IS MEASURED, and the measurement already existed in the tree.**
  `standalone/wave17_door_attend.cpp` watches this exact symbol.
  `docs/runtime_port/wave17_lane_ceil/attend_{load,save}.txt:49` both read
  `ATTENDED init=0 post=1`, with the `?_singleton@MainLoop@` control **GREEN**
  in both; `attend_mods.txt:49` reads `never post=0`. `post` counts DOOR OPENS,
  not dispatches, and `post==1` is not "ran once" — every consumer caches in a
  function-local static. **What it proves is that a consumer of this symbol RAN
  on the save and load routes.** (`reference_grep_the_harness_before_declaring_no_route.md`
  — I was about to write "unverifiable, no run" and the harness already held the
  measurement.)
* **The consumer that makes `??_7Geometry@` HARMFUL was checked here and does
  NOT apply.** Geometry is harmful because `statusicon_geometry.cpp` copies slots
  out of the stamped table with a fixed bound. MaterialInstance has the same
  shape of consumer — `code/Game/Dialog/thumb_vtables.cpp` builds the four
  dialogs' thumbnail vtable by copying out of whatever the Runtime ctor
  installed — and it is **width-correct**: `kSlots = 9` (:38), `for (i = 1; i <
  kSlots; i++)` (:74), against `static void* kVTable38[9]`
  (`rt_data_mint_tables.inc:1197`). 9 against 9, indices 1..8. No overrun in
  either direction, which is precisely the fact that is false for Geometry.
* **No consumer reads this symbol as an identity token.** Grepped across the
  five source dirs: all four consumers STAMP the pointer; none compares an
  object's vptr against it. That is the `standalone/scene_watch.cpp` shape that
  makes `?AddDrawItems@MeshInstance@` permanently HARMFUL, and it is absent.
* **A/B control:** `BANISHED_RT_PROMOTE_MATINST=0` turns this row off and
  nothing else, following the `??_7BinaryFileStream@` precedent — one variable,
  not the file-wide `BANISHED_RT_MINT_PROMOTED=0`.

### 5.1 Why this was promoted without a run, when Wave 23 refused Geometry without one

They are different refusals and it matters that they not be conflated.

GFXVT refused **Geometry** because a promotion there converts six
never-virtually-dispatched bodies into DLL-dispatched ones **and** permanently
dirties one of the most-exercised oracle surfaces in the tree, *in exchange for
nothing retail's own table is not already doing* — an abstract base, no live
final vptr. That is a bet with a cost and no measurable upside, and the right
answer without a run is no.

GFXVT's verdict on **MaterialInstance** was `blocked, 4 slots short` and
`at cutover: required`. It is concrete, it is a live final vptr, its door is
measured ATTENDED on two routes, its one copying consumer is width-correct, and
Wave 19 lane IOBASE's standing precedent in this same file promoted two rows at
`post=0` on the argument that *TRAP=0 means no slot can abort and leaving a row
unpromoted does not verify it — it only keeps the slots unreachable*. This row
clears a strictly higher bar than that precedent.

**The residual risk is real and is declared, not argued away:** no lane-VTGFX
game run happened, so nothing here observes the promoted table being dispatched.
§8 and the two ADDRESSEE clauses in the source say so.

---

## 6. THE GUARD THIS LANE ADDED — `RtMintVtableTrapCount()`

`standalone/rt_data_mint.cpp` / `.h`. Records, per minted vtable row, how many
slots `MintVtables()` had to fill with a named abort trap because no port was
registered by the time `RtDataMintInit()` ran.

### 6.1 The hazard, which no build check can see

A promoted vtable's slots are filled from the **live** port registry. Three wave
kill switches — `BANISHED_RT_GRAPHICS=0`, `BANISHED_RT_GFXTI=0`,
`BANISHED_RT_GFXMAT=0` — suppress registrations this row depends on. With the
row promoted and any one of them set, the promotion publishes a table of named
abort traps into a live object's vptr. **The build is clean, the link succeeds,
and the first virtual call aborts** — kickoff §3.6 exactly.

The promotion now consults the counter and **refuses itself**, printing which
kill switch class is responsible. The return is `-1` for "not a minted VTABLE
row", deliberately distinguished from `0`, so a typo in the symbol cannot read as
a green light (`reference_a_screen_that_cannot_name_its_subject.md`).

### 6.2 The exposure is NOT new and NOT confined to my row

`??_7BinaryFileStream@IO@@6B@`, `??_7BinaryReader@`, `??_7Visitor@`,
`??_7MemoryStream@`, `??_7VisitorStream@` and `??_7BinaryWriter@` have carried
the identical exposure unguarded since Waves 19–20. **I did not extend the guard
to them** — that is a six-row edit to other lanes' warrants and is SWEEP-sized.
It is named as an addressed residual in the source and in §9.

---

## 7. CHECKERS AND BUILD

```
mingw32-make standalone-debug                        CLEAN  (reimpl-x64.exe, 32M)
tools/check_log_strip.py                             exit 0
tools/dispatch_slot_map.py --check
    checked 110 ResolveExport registrations, 0 name mismatch(es);
    0 address(es) registered more than once
tools/unpromotable_slot_check.py
    CHECK 1: 20 of 20 scoreable tags fit, 0 STALE
    CHECK 2: 20 numeric tags -- 6 declared, 14 grandfathered, 0 UNDECLARED, 0 malformed
    CHECK 3: 46 tags with an expiry -- 37 scoped, 37 addressed, 9 grandfathered,
             0 MISSING, 0 malformed
    CONTROLS 1pos/1neg/2pos/2neg/3pos/3neg/4pos/4neg all GREEN
```

**Identical before and after**, because this lane added, edited and deleted **no
tag**. The 9 grandfathered expiries are pre-existing and are the kickoff's
MANIFEST lane's item, not mine.

---

## 8. DECLARED: MY SURFACE GROWTH (kickoff §1f)

**The flattering number is the wrong one, so both are given.**

```
                      before (fc56353c)     after      delta
TOTAL called               1323             1331        +8
TOTAL ported               1171             1180        +9
TOTAL                     88.5%            88.7%
TOTAL gap rows              152              151        -1
Graphics called             101              108        +7
Graphics ported              82               90        +8
Graphics gap                 19               18        -1   (VTABLE 8 -> 7)
internal called             190              191        +1
internal ported             148              149        +1
promoted minted vtables      11               12        +1
minted slot rows folded in  143              151        +8   (111 -> 119 newly visible)
```

**Read this as ONE gap row closed, not as +9 ported.** Promoting a vtable folds
its slot rows into the census population: 8 rows that our tree already resolved
but the census could not see entered `called`, and all 8 were already ported, so
they entered `ported` in the same breath. The ninth is the vtable row itself
flipping from gap to ported. Net movement of the thing that matters: **Graphics
gap 19 → 18**.

**Address doors added: ZERO.** The promotion registers one address
(`0x1801545e0`) and that address is an **exported DATA symbol**, registered
alongside its name — it is not an address door in the sense of
`reference_by_address_door_dies_at_cutover.md`, which is about unexported
internals reached only by `ResolveAddr`.

---

## 9. RESIDUALS — each with SCOPE and a NAMED ADDRESSEE

**R1 — The promoted row has never been observed dispatching.**
`[SCOPE: DLL-PRESENT]` `[ADDRESSEE: WAVE 25 lane SWEEP, which holds the second
game-directory slot]` — run one autostart boot with `BANISHED_W17_ATTEND=1
BANISHED_RT_DIFF=1`, and a second with `BANISHED_RT_PROMOTE_MATINST=0`, and
report three things separately: (a) that `??_7MaterialInstance@Graphics@@6B@`
still reads `ATTENDED`; (b) the `Graphics::MaterialInstance::Construct[ORACLE]`
vptr screen, which **must move and only move** — a permanent `+0x00` divergence
is this promotion's signature, not a regression; (c) whether ANY of the nine slot
bodies records `calls>0`, which is the dispatch evidence nobody in this project
yet has. **Read GFXMAT's `g_oracleRuns` effectiveness counter in the `[GFXMAT24]`
line first: if it prints 0, nothing was compared regardless of what the
attendance line says.**

**R2 — Five vtables promoted in Waves 19–20 can publish a trap table and nothing
checks.** `[SCOPE: DLL-PRESENT]` `[ADDRESSEE: WAVE 25 lane SWEEP]` — wrap
`??_7BinaryFileStream@`, `??_7BinaryReader@`, `??_7Visitor@`,
`??_7MemoryStream@`, `??_7VisitorStream@` and `??_7BinaryWriter@` in the same
`RtMintVtableTrapCount() != 0` refusal added in §6. Mechanical; the pattern is in
the file.

**R3 — `??_7Achievement@0@6B@` is the best-priced promotion left in my two
namespaces, and it has a worked recipe.** `[SCOPE: PERMANENT — this is a
scoping fact, not a condition]` `[ADDRESSEE: the WAVE 26 KICKOFF WRITER, to deal
to a body lane]` — 5 slots, of which 2 are the trivial accessor pair and 3 are
the `Construct`/`Reconstruct`/`Destruct` trio Wave 24 GFXMAT ported against a
**slot-for-slot identical table** (§4.3), traps and all, in
`runtime/src/graphics/wave24_lane_gfxmat.cpp` §3.1. CONCRETE, 0 purecall, 2 of 7
stampers ours, 0 unattributed. **Do not deal `??_7Goal@Achievement@@6B@` instead
because it is 3 slots rather than 5 — it is abstract and the finished promotion
is worth nothing (§4.2).**

**R4 — `??_7SunLight@Graphics@@6B@` remains the cheapest CONCRETE Graphics row
and Wave 23 flagged it unclaimed.** `[SCOPE: PERMANENT]` `[ADDRESSEE: the WAVE 26
KICKOFF WRITER]` — 11 slots, 0 purecall, 6 to source, only 1 retail stamper.
Re-derived unchanged this lane. Flagged by GFXVT, unclaimed for two waves.

**R5 — `?_typeInfo@MaterialInstance@Graphics@@0VBaseTypeInfo@Object@@A` is now
unblocked, and it is lane DATA's row.** `[SCOPE: DLL-PRESENT]` `[ADDRESSEE:
WAVE 25 lane DATA]` — Wave 21 GFXDATA measured it `never init=1 post=0` and
Wave 23 GFXVT §5.3a identified `??_7MaterialInstance@` as **the one table of the
eight whose promotion would genuinely produce attendance on it**. That table is
now promoted. GFXVT's warning stands for the other two rows in that group:
promoting `??_7AnimationEvent@` or `??_7Particle@` would NOT produce attendance
on their `?_typeInfo@` rows, because those tables are abstract and nothing
dispatches them — a 0 there would indict a sound DATA row.

**R6 — Wave 23 GFXVT residual 2 is still open.**
`runtime/src/graphics/scene.cpp:94-99` bullet 3 of the `VTABLE OWNERSHIP` note is
STALE (Wave 22 lane GEOM's `i < 9` fix voided it) and was flagged for Wave 23
lane GFXBODY, which did not strike it. `[SCOPE: source hygiene, no behaviour]`
`[ADDRESSEE: WAVE 25 lane BODY, which owns runtime/src/graphics/ this wave]` —
strike bullet 3; **keep bullets 1 and 2**, which are the live argument against
promoting Geometry. I did not edit it: it is another lane's file this wave and a
comment edit from a second lane is a merge conflict for no gain.

---

## 10. WHAT THIS LANE DID **NOT** ESTABLISH — plainly

* **NO GAME RUN. NONE.** No lock, by design. **Every verdict above is derived
  from bytes, from source, and from measurements other waves recorded.** No
  attendance, no dispatch and no frame was measured by this lane.
* **The attendance that carries the promotion is WAVE 17's, on a WAVE 17 TREE.**
  Its control was green and it is a real recorded boot, but it establishes that
  the ROUTE existed eight waves ago — not that it still runs identically today.
  `reference_a_cross_wave_comparison_reads_as_an_instrument_disagreement.md`
  cuts the other way here and I am flagging it against myself.
* **NO SLOT BODY IS VERIFIED BY THIS PROMOTION.** GFXMAT's synthetic oracle
  covers slots 5–8 against the shipped export; **slots 1, 2, 3 (Type / TypeName /
  GetFileVersion) and slot 4 (the shared `Compile@External`) are covered by
  neither that probe nor this lane.** A promotion installs bodies; it does not
  check them.
* **`Graphics::MaterialInstance::RenderTextures` measured worked=201,103 /
  calls=0 / divergences=0** (Wave 24 lane DIFF). **No existing MaterialInstance
  green may be read as coverage of this row.**
* **Six of twelve extents rest on the SOFT adjacent-export bound.** §2.2's
  second channel corroborates them and its controls fire 6 of 6, but it is
  corroboration, not a byte terminator. An unexported table beginning inside one
  of those runs whose slot 0 happened to be an exported function would still pass.
* **`ours-capable` is an upper bound everywhere in §3.** A registration is a
  door. I did not measure which callers actually use it for any row.
* **I ported no slot body.** 32 unported non-purecall slots remain across the
  eleven held rows. §4.3 and R4 price the two best targets; neither was taken.
* **I did not re-derive the five tagged Graphics rows' tags themselves.** I
  established that **none of my twelve is among them** (§1.1) and stopped there.
  Whether those five tags are still correct is lane MANIFEST's question.
* **The `RtMintVtableTrapCount()` refusal path has never fired.** It is written
  and it compiles; no run has exercised it, and no negative control was
  constructed for it (that would mean building with `BANISHED_RT_GFXMAT=0` and
  booting, which needs the lock). **It is untested machinery**, which this
  project has repeatedly found costly, and I am saying so rather than counting it
  as a check that passed.
* **I did not classify the 13 unknown-wrapper identifiers** the census's own
  screen names on every run. Wave 21 and Wave 23 left the same hole.

---

## 11. DENOMINATOR

* **12 rows** — the 8 `??_7…@Graphics@@6B@` and 4 `??_7…@Achievement@@6B@` VTABLE
  rows in this tip's Graphics-19 and Achievement-9 gaps. Not the 11 Wave 21
  reported on (it added `EdgeMesh`, `EdgeMeshInstance`, `MeshInstance` as
  context; those are still census-invisible and still unclaimed, and I measured
  none of them).
* **Extent**: 12 of 12 measured myself from the shipped bytes; **6 HARD in both
  directions, 6 SOFT with a second-channel corroboration whose control fires 6
  of 6.**
* **Purecall**: 12 of 12 from the bytes via `tools/vtable_dump.py` (`_purecall`
  control FOUND). Cross-checked against the census backlog: `extent − purecall
  == slots`, **12 of 12 agree**.
* **Ported-per-slot**: 12 of 12 by literal grep across
  `runtime/ code/ standalone/ engine/ dx11/` (scoped). Agrees with the census
  `ours` column 12 of 12.
* **Stampers**: 70 sites over 12 rows, one channel (`mint_stamper_census.py`),
  decoder controls 3/3, ownership pos+neg green, `ours-capable` explicitly an
  upper bound. **I did not re-run the Ghidra `get_xrefs_to` cross-channel** that
  Wave 21 used — I have one channel, freshly run.
* **Abstract/concrete**: from the purecall count alone for all 12. Wave 23
  corroborated 2 of its 5 abstract rows by reading our own ctor/dtor bodies; I
  added no such corroboration and did not re-do theirs.
* **Runtime attendance**: **zero measurements by this lane.** The two cited are
  Wave 17 lane CEIL's, quoted with their file and line and their control state.
