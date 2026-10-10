# WAVE 29 — LANE VERIFY

Branch `wave29/verify`, cut from `wave29/base` at `c11511a1`.
Partner: lane **INSTR**. Subject: the ~280-body verification debt, defect #7, and
the FACTUI1 oracle promotion decision.

**Every figure below is from a run I made this session, on a build I made this
session, under `tools/game_dir_lock.sh`.** Where a figure is inherited I say so.

---

## 0. HEADLINE

1. **Defect #7 is real, it was DEFLATING, and it is fixed.** Three lane
   `*Summary()` functions were written and **nothing called them**. Wiring them
   produced attendance numbers no run in this project has ever printed —
   `Renderer::Draw = 13,125`, `AnimationState::Update = 17,950`,
   `BeginDrawGroup = 26,249`, `Camera::operator= = 52,486` on one in-game run.
   **Wave 28 §5's "GFXBODY: six of eight printed nothing" is partly SILENCE, not
   zero.**
2. **The debt is now MEASURED and DECOMPOSED rather than estimated.** A new
   per-row port-door attendance instrument (`BANISHED_RT_ATTEND=1`) splits the
   registry into asked/silent per row. On an in-game run: **816 of 1526 name
   rows and 1424 of 1576 address rows were never asked for**, and the silent set
   splits four ways with measured sizes (§3).
3. **The FACTUI1 oracle is PROMOTED — but not as written, and the change is the
   whole point.** Its ours-side replays the table; the port is a second
   interpreter over that same table, so it could not see an interpreter defect.
   `standalone/factory_byte_oracle.cpp` calls **the registered function pointer**
   instead. It ran live: **18 OK / 5 FAIL / 2 VACUOUS of 25 concrete rows**, with
   negative control 18/18 and abstract control 10/10.
4. **That 5-FAIL set is NOT a defect list and I do not present it as one** — it
   is unstable across runs (§5.3). What is solid is that FACTUI1's clean sheet
   **does not reproduce when the ours-side is the real port.**

---

## 1. DEFECT #7 — every lane's `*Summary()` and nothing calls it

### 1.1 The screen

`verify_reporter_census.py` (scratch): every 0-argument free function in
`runtime/ standalone/ code/ engine/ dx11/` whose name ends in
`Summary/Report/Attendance/Census/Counters/Attend/Tally`, checked against three
reachability channels **scored separately** (warning 9) — a `Name();` statement,
`atexit(&Name)`, and `&Name` taken as a pointer.

```
population: 46 distinct names, 48 definitions, 1581 files
            (comments AND string literals blanked before scanning)
controls  : known POSITIVE RtDiffSummary  expect CALL   -> PASS
            known POSITIVE TextSummary    expect ATEXIT -> PASS
            known NEGATIVE (name absent)  expect 0/0/0  -> PASS
```

### 1.2 What it found — 3 UNREACHABLE-BY-SOURCE

| reporter | file | rows it reports on |
|---|---|---|
| `RtGraphicsWave24GfxmatSummary` | `runtime/src/graphics/wave24_lane_gfxmat.cpp:811` | 6 |
| `RtGraphicsWave27GfxbigSummary` | `runtime/src/graphics/wave27_lane_gfxbig.cpp:1124` | 4 |
| `RtGraphicsWave28GfxbodySummary` | `runtime/src/graphics/wave28_lane_gfxbody.cpp:899` | 8 |

**18 registered rows across three waves**, every one behind a DEFAULT-ON kill
switch (`RtWaveEnabled` is a kill switch, not an opt-in), whose attendance line
had never been printed by any run. Each of the three carries the standard *"a run
that entered none of these says so rather than staying silent"* comment directly
above it.

Fixed in `standalone/main.cpp`, beside the three sibling summaries that WERE
wired (Wave 23 GFXBODY, Wave 27 GFXPART, Wave 28 PARTSYS).

**A second instance, one level down.** `RtGraphicsWave28GfxbodySummary`'s printf
covered 6 of its lane's 8 rows. `g_updateRootCalls`, `g_updateRootLayers` and
`g_cameraAssignCalls` were incremented by `Attend()` and **printed nowhere**.
`Camera::operator=` is the row the kickoff §2.2 warns about by name. Added.

### 1.3 How many attendance zeros were actually silence — MEASURED

In-game run (autostart → world → pause-menu Quit), all three newly wired:

```
[GFXMAT24]  Construct=0 Reconstruct=0 Destruct=0 MatSer=0 MeshSer=0 AnimSer=0
[GFXMAT24]  differential-oracle runs (effectiveness counter) = 0
[GFXBIG27]  BuildParameters=0  Draw=13125 (entries=26249 sorts=39373)
            AnimStateUpdate=17950 (layers=17950 expired=0)
[GFXBODY28] GetConstantName=0 GetTextureName=0 FindConstant=0 FindTexture=0
            ApplyEvents=0 BeginDrawGroup=26249 (appends=6)
[GFXBODY28] UpdateRootOnly=0 (layers=0) CameraAssign=52486
```

**Of the 18 rows, 4 were LIVE and had never been counted in a published figure**
(`Renderer::Draw`, `AnimationState::Update`, `BeginDrawGroup`,
`Camera::operator=`). The other 14 are genuine zeros — **and those zeros had also
never been printed**, including GFXMAT24's oracle effectiveness counter, which is
the single number that says its differential oracle compared nothing at all.

Partial mitigation, stated so the finding is not overclaimed: `Attend()` in the
Wave 27/28 lanes prints a milestone line at n = 1, 10, 1000 and every 100k, so
those four rows were not *totally* invisible — a reader grepping the log could
have seen `calls=1000`. What was invisible is the **totals**, the **zeros**, and
every row whose counter had no milestone printer at all.

### 1.4 Direction and blast radius

**DEFLATING.** A zero from an unreached printf and a zero from an unentered body
are the same character in a log — the instrument could not express the
difference. Any wave that read "attendance 0" for a GFXMAT24 / GFXBIG27 /
GFXBODY28 row read silence.

### 1.5 My own screen's defect, caught by its own control

First run reported **6** unreachable. Three were false: `atexit(&RtObject::DumpAddCounters)`
— a **qualified** name — did not match my unqualified regex. Direction:
**INFLATING** (3 false positives, manufactured work). Fixed; the note is in the
script. `TextSummary` was a fourth false positive from the same class
(`std::atexit(&TextSummary)`), caught by the same fix.

---

## 2. THE NEW INSTRUMENT — per-row port-door attendance

`RuntimeRegistryStats::hits` was **one global counter**. It could say 52,538
resolutions happened and could not say which of ~3100 registered rows got even
one. That is why "~280 bodies registered and never executed" was an accumulated
estimate for three waves rather than a measurement.

`standalone/resolve_standalone.cpp` now counts **per row** at all four registry
hit sites (name, address, ordinal, and the ordinal→name bridge), armed with
`BANISHED_RT_ATTEND=1`, reported from the process exit path by
`RtRowAttendanceReport()`.

**WHAT IT MEASURES, EXACTLY.** How many times the registry HANDED OUT our
function pointer. Not calls, not executions:

- `count > 0` is **not** N calls. `RT_EXPORT_ONCE` and every caching door
  resolve once and call forever; a body that ran 16.9M times can report 1.
- `count == 0` is **not** always "never ran". A row reached through a minted
  vtable slot or a static-initialiser stamp never passes this door at all.
  **That is the whole "a stamp is not a DOOR" finding** and it is why the
  factory family is attendance-0 by design.

What it *does* establish, and nothing in the tree established before: **the set
of ported rows for which nothing in the process ever asked.** For a row whose
only door is this registry, `SILENT` is a **proof of non-execution**.

Its two controls print with it and both PASSED on every armed run:
`CONTROL 1 (effectiveness)` non-zero handouts; `CONTROL 2 (known positive
?Update@Device@Audio@@)` registered=yes asked=yes. The positive was chosen
because it is resolved unconditionally from `EngineFuncsInit()`
(`engine/funcs/engine_funcs.cpp:1558`) on every boot — my first choice, a Wave 2
`String` ctor, had its only literal resolve site in `standalone/mouse_inject.cpp`,
which an unattended run need not reach. **A "known positive" that is not actually
known is a control that manufactures a FAIL.**

It is **OFF by default**: it adds a hash lookup to the registry-hit path and
`ResolveExport` sits inside hot bodies here (the Wave 2 perf breach was two
resolves in `System::Time`). The report prints its **arm state unconditionally**,
so an unarmed run cannot be read as a run in which nothing resolved.

---

## 3. THE DEBT, RE-DERIVED AND DECOMPOSED

**Re-derived: I did not take 280 on trust, and 280 is not the shape of the
problem.** Measured on an in-game run at my tip (`wave29/verify`, built and run
2026-09-12):

```
[RtAttend] ARM=ARMED  name 710/1526 asked  addr 152/1576 asked  ord 0/2 asked
```

**816 silent name rows, 1424 silent address rows, 2 silent ordinals.** The
registry is far larger than the census's 1294 `ported` because the address door
holds rows that are not DLL exports.

### 3.1 By FAMILY (name door)

| family | silent | asked | total |
|---|---:|---:|---:|
| BODY `?` | 491 | 510 | 1001 |
| CTOR `??0` | 69 | 118 | 187 |
| DTOR `??1` | 31 | 48 | 79 |
| **FACTORY** (`?CreateNewObject@` / `?CreateInPlaceObject@`) | **200** | **0** | **200** |
| `operator=` `??4` | 3 | 5 | 8 |
| op new/delete | 9 | 2 | 11 |
| other `??` | 4 | 17 | 21 |
| VTABLE `??_7` | 9 | 10 | 19 |
| **TOTAL** | **816** | **710** | **1526** |

**The factory family is 200 rows, silent 200 / asked 0, measured.** That is the
kickoff's "~206" confirmed independently and from the process rather than from a
table.

### 3.2 By REASON — "a namespace gap of N is not N bodies", and neither is this

`verify_debt_decompose.py`, classifying each silent row by what the tree does
with its literal:

| class | name door | addr door | meaning |
|---|---:|---:|---|
| **A NO DOOR EXISTS** (only site is a mint/factory table — a stamp, not a door) | 224 | 406 | SILENT is expected and carries **no information** |
| **B NO ROUTE WALKED THIS RUN** (a real consumer exists elsewhere in the tree) | 471 | 420 | payable by building or running a route |
| **C ROUTE EXISTS, IN A HARNESS/PROBE** this run left unarmed | 15 | 31 | **payable today, by setting an env var** |
| **D NO CONSUMER SITE IN THE TREE AT ALL** | 106 | 567 | not a run problem; nothing consumes the row |

**So "~280 unexecuted bodies" is not a work item of 280.** On the name door,
224 rows cannot inform on execution at all, 15 are payable by arming an existing
probe, 106 have no consumer to run, and the real "build or walk a route" set is
**471**, dominated by IO (205) and UI (135) — namespace split of the silent
non-factory rows:

```
IO 205 · UI 135 · Graphics 62 · Video 53 · System 26 · Object 24 · Engine 22
Achievement 21 · Math 18 · Save 14 · Audio 5 · Input 5 · Volume 3 · Picking 1
```

### 3.3 The coordinator's item — multi-line registration spellings

Checked, and the answer has two halves:

- **The attendance instrument is IMMUNE by construction.** It reads the LIVE
  REGISTRY, not source. A registration whose name literal is split across lines
  is one string to the compiler and therefore one key in the map. The blind spot
  that made `mint_slot_sweep.py` report 16 unported slots where 7 was true
  cannot reach any number in §3.1.
- **My source-side CLASSIFIER did have it**, and its own control measured it at
  6 rows before I fixed it (§5.1). Class D is the one it inflates.

### 3.4 IOLANE's "a promotion has a LIFETIME" — checked against my classes

IOLANE's `Reconstruct` finding (retail re-stamps its own table over an object we
built, via an in-DLL `rel32`) is a **fifth class** that my decomposition does not
separate: a door that EXISTED and was then EVICTED. My class A ("no door exists")
would absorb such a row and call it permanent when it is not.

I did not measure it and I am not claiming it is empty.
**[ADDRESSEE: the Wave 30 kickoff writer.]** Class A above is stated as "no door
exists"; it should be read as "no door was walked and the only site is a stamp",
which does not distinguish never-had-a-door from door-evicted.

---

## 4. THE FACTUI1 ORACLE — the decision, and why

### 4.1 DECISION: PROMOTE, and change the one thing that makes it an oracle

**I did not promote the file into `tools/`.** I wrote
`standalone/factory_byte_oracle.cpp`, which is the same idea with its ours-side
replaced.

**The intrinsic property that makes copying it wrong.** FACTUI1's oracle is
sound about what it measures, and what it measures is not the port.
Its THEIRS side is `GetProcAddress` — the shipped body. Its OURS side is
`OursCtor()`, which walks `sp.stores` and **replays the table itself**. It never
calls the port. And the port (`runtime/src/ui/factory_ui1.cpp:262`,
`rt_factories_core.cpp:RunSteps`) is *also* a table interpreter over that same
table. So the oracle's ours-side is a **second copy of the port's interpreter,
reading the same data**. It proves the TABLE matches retail. It cannot see a
defect in the interpreter, because both copies share it.

That is the same structural shape as the three failures this project has already
paid for: the ~250 `RT_DIFF_SHADOW` self-checks, a probe whose ours-side reaches
the body by address, and two checks with one hidden premise. **Promoting it
verbatim would have added a fourth, with 200 rows behind it.**

### 4.2 What the promotion changes

| | FACTUI1 offline | `standalone/factory_byte_oracle.cpp` |
|---|---|---|
| OURS | replays `sp.stores` | **`RtFactoryReg::inPlaceFn` — the registered pointer** |
| THEIRS | `GetProcAddress` | `ResolveExportRaw` (never `ResolveExport`, which returns ours) |
| exclusion | hand-written `+0x08..+0x0f`, wrong in both directions on different rows by its own post-mortem | **derived**: construct each side TWICE and mask every byte on which a side disagrees with itself |
| runs | separate short-lived process | in the live game, on the engine's own allocator |

Supporting change: `RtFactoryRoster()` in `runtime/src/object/rt_factories.h` /
`rt_factories_core.cpp`. Each namespace's `kRegs` was file-static, so **nothing
could enumerate the family at runtime** — which is *why* the only evidence was
offline. It returns the TRUE row count so a caller with too small a buffer can
say so instead of reporting on a prefix.

### 4.3 PLACEMENT WAS MEASURED, AND THE TWO OBVIOUS PLACEMENTS BOTH FAILED

Recorded because "just run it earlier" is the obvious fix and it is wrong twice:

| placement | outcome |
|---|---|
| process exit path | **ACCESS_VIOLATION** writing `+0x648` inside `Runtime-x64.dll`'s `MemAlloc` on the **third** row — the allocator lock at `singleton+0x640` with the singleton already NULL. Some of these ctors allocate an inner `Collection::Array`; by exit the engine has torn its allocator down. |
| before `GameMain` | singleton does not exist yet (`Engine::Startup` makes it); the screen aborted having compared **0** rows. |
| bootstrap our own allocator at exit | `Startup` **returned 0** and the slot was populated anyway — it *looked* like it worked and the next allocation died. **A populated singleton slot is not a started allocator.** |
| **`ToolHarness_Tick`, in-game frame 120, one shot** | the engine's own allocator is live — and it is the allocator these bodies will use at cutover, so it is the right one rather than merely a working one. |

**Cost, stated not buried:** FACTUI1 stayed offline partly because an
Element-derived ctor registers a handle that would dangle in-game. Half is
defused (buffers are file-scope statics that outlive the process, so nothing
dangles). The other half is **not**: this does insert roster-class handles into a
live `InternalFactory` mid-game. Paid only on an explicitly armed run.

### 4.4 COVERAGE, MEASURED — and it is not 200

The roster the in-process oracle can enumerate is **FACTREST's 35 non-UI
classes**, not the whole 200-row family. Of those 35: **25 concrete rows checked,
10 abstract rows checked, 0 skipped.**

```
RESULT  : 25 concrete rows -- 18 byte-identical, 5 FAIL, 2 VACUOUS, 0 skipped
COVERAGE: 1823 bytes compared, 25 MASKED as nondeterministic, over 1944 bytes
CONTROL negative: 18/18 passing rows caught a one-bit corruption of a COMPARED byte
CONTROL abstract: 10/10 rows whose table says allocSize=0 had retail return NULL
```

FACTUI1's 31 UI classes are **not** covered: they register through
`runtime/src/ui/factory_ui1.cpp`, which has no equivalent roster accessor.
**[ADDRESSEE: whichever Wave 30 lane touches `factory_ui1.cpp`.]** Add the same
three-line roster accessor there and this screen covers them with no other
change.

### 4.5 THE RESULT I AM NOT CLAIMING

**The 5 FAIL rows are NOT a defect list.** The set is unstable across runs of
the same build:

| run | FAILs | EdgeMesh first-bad |
|---|---|---|
| oracle4 (one-sided mask) | 7 | `+0x60`, ours `60` theirs `68` |
| oracle5 (two-sided mask, broken control) | 3 | *passes* |
| oracle6 (final) | 5 | `+0x73`, ours `84` theirs `83` |

A byte whose offset moves between runs is not a port defect, it is residual
nondeterminism that two constructions inside one run do not mask. What IS solid:
**FACTUI1's clean sheet does not reproduce when the ours-side is the real port**,
the 18 PASSING rows are corroborated by a live negative control, and the two
VACUOUS rows (`AchievementInstance`, `GoalInstance`) are reported as **NOT
verified** rather than as passes — 0 bytes compared.

**[ADDRESSEE: the Wave 30 lane that next touches `rt_factories_*.inc`.]** Before
treating any of the 5 as a defect, run the screen N times and keep only bytes
that fail in EVERY run. The stability screen is the missing piece, not the
comparison.

---

## 5. INSTRUMENT DEFECTS, WITH DIRECTION

Deflating outranks inflating.

### 5.1 Mine

| # | defect | direction |
|---|---|---|
| 1 | reporter-reachability screen missed `atexit(&Ns::Name)` (qualified) — 3+1 false positives | **INFLATING** |
| 2 | debt classifier v1 enumerated DOOR SPELLINGS; its own control found 57/710 process-confirmed rows with "no site in the tree", one of them resolved by `Dx11ResolveRuntimeImport(...)` | INFLATING class D, **DEFLATING** B/C |
| 3 | classifier v2 still line-oriented → blind to C adjacent-string concatenation; 6 residual, all multi-line literals. Fixed in v3 by joining `"A" "B"` before searching | INFLATING class D |
| 4 | factory oracle's mask was ONE-SIDED (retail-vs-retail only). Measured: `EdgeMesh +0x60` read ours `98`/`90`/`60` across three runs while retail read `68` every time — scored as a divergence three times | **INFLATING** |
| 5 | fixing #4 broke the negative control: it flipped only `g_ours`, which made the victim byte look nondeterministic and MASKED it. Read **0/20 NOT CAUGHT** on a sound screen | INFLATING (and it announced itself loudly, which is why it cost minutes) |

### 5.2 My partner's, found by running their screens against my population

| tool | defect | direction |
|---|---|---|
| `runtime_census.py` factory screen | its family regex `^?Create(New\|InPlace)?Object@` has an **optional** group, so it matches `?CreateObject@Factory@Object@@` — the AVL **dispatcher**, not a per-class factory. That row is in the board population, so the screen fires its `*** IN THE BOARD POPULATION IS NON-ZERO … part 1 no longer holds ***` alarm. **Part 1 does still hold.** My live data corroborates both halves: `?CreateObject@Factory@Object@@` was **ASKED 2** (a real door, and not a factory slot), while `?CreateNewObject@`/`?CreateInPlaceObject@` were **SILENT 200 / ASKED 0** | **INFLATING** (false alarm) |

### 5.3 Other tools

`rt_deleting_dtor_verify.py` — INSTR asked whether `UNRECOGNISED` is right for
`EdgeMeshInstance`, `MeshInstance`, `Light`. **`UNRECOGNISED` is the right
verdict and there ARE real third shapes.** Read from the retail disassembly:

- **`Graphics::EdgeMeshInstance` (0x1800175d0).** The scalar branch at
  `0x180017621` calls **`0x1800a41c0`**, not the `0x1800174d0` the array branch
  passes as its element callback. `0x1800174d0` is a frame-setup thunk that tail
  `jmp`s to `0x1800a41c0` (`0x1800174e6: jmp 0x1800a41c0`), and `0x1800a41c0` is
  the real dtor — it restamps `0x180154e60` at `+0x00`. **Shape: the scalar
  branch calls the thunk's TARGET.** Following a one-instruction `jmp` tail
  resolves this row.
- **`Graphics::MeshInstance` (0x180017330).** The scalar branch at `0x180017381`
  **inlines the whole dtor body** — `add rcx,0xf0 ; call 0x18001c550 ; lea
  rcx,[rbx+0xd8] ; … ; call 0x18001c480 ; … call 0x180014d00` — which is the
  opening of `0x180017060` verbatim. **Shape: the dtor is inlined, so there is
  no call to find.** `Light` (0x180058dd0) is not a thunk either and is most
  likely this same shape; I did not read its scalar branch.

---

## 6. WHAT I RE-DERIVED AND WHAT REPRODUCED

| claim | source | my result |
|---|---|---|
| "~280 bodies registered and never executed" | kickoff §1.5 | **NOT REPRODUCED AS A COUNT.** 816 silent name rows + 1424 silent addr rows, decomposed in §3.2. 280 was an accumulated estimate. |
| "~206 are factory slots" | kickoff §1.5 | **REPRODUCED, 200**, measured from a live run |
| "Exactly ONE body in Wave 28 was exercised on a live route" | WAVE28_CLOSED §5 | **REFUTED.** At least 4 more were live and unprinted (§1.3) |
| "GFXBODY: six of eight printed nothing" | WAVE28_CLOSED §5 | **PARTLY SILENCE.** 2 of the 8 were live at 26,249 and 52,486 |
| factory attendance 0 by design (stamp ≠ door) | WAVE28_CLOSED §5 | **REPRODUCED**, and now measured rather than argued |
| board 1453/1294 89.1% | kickoff §1 | superseded by INSTR to **1460/1295 88.7%**; §3 is keyed to the LIVE REGISTRY, not the board, so it is unaffected |
| INSTR's 26 recovered macro-door names | INSTR | **11 CONFIRMED as real doors by live execution**, 5 silent (no information), 10 unported (invisible to a registry-hit counter — not a refutation). **0 false doors**, and the screen is structurally unable to find one |
| INSTR `--selftest-macro-doors` | INSTR | 14/14 |
| INSTR `--selftest-factory-blind` | INSTR | 6/6, but its live section has the regex defect in §5.2 |
| INSTR `mint_stamper_census.py --selftest` | INSTR | 5/5 |

---

## 7. MY OWN ERRORS

1. I built the reporter screen and ran it **before** writing its controls. The
   controls then found 4 false positives. Right order, wrong sequence.
2. My first debt classifier enumerated door spellings — **the exact defect the
   census has had for three waves**, reinvented in a fresh tool in under an hour.
   Its control caught it; I did not.
3. My oracle's mask was one-sided and I did not notice until the FAIL set moved
   between runs. INSTR's design review named the *other* direction (masked
   defects) and the one that actually bit me was the one neither of us named
   first: **ours-side nondeterminism**.
4. I issued a 600s lock acquire. **The Bash tool auto-backgrounded it at 600s** —
   exactly the "never background a blocking acquire" hazard, imposed by the
   harness rather than by me. It timed out (`ACQ_EXIT=1`, not granted) so nothing
   was held unnoticed, **but the task-completion notice said "exit code 0"**,
   because the trailing `; echo` owns the pipeline's status. That is the piped-
   acquire trap in a different costume. Afterwards I used ≤560s acquires and
   asserted `assert-held` with my build's md5 immediately before every launch.
5. **Lock waits, recorded as asked:** queued behind WALL then MINT for a full
   600s (timed out, no grant), then re-queued and waited **~346s** behind MINT
   before `ACQUIRED by VERIFY`. Released between measurement bursts.

---

## 8. WHAT THIS DOES NOT CLAIM

- **It does not claim the 18 passing factory rows are correct.** They are
  byte-identical **on the in-place door, at construction**, on one machine, with
  25 of 1944 bytes masked as nondeterministic and therefore unscoreable. The
  masked set can never fail and the negative control is structurally unable to
  report that it is too large — `masked=` is printed per class precisely so a
  reader can price that.
- **It does not claim the 5 FAIL rows are defects.** §4.5. The set is unstable.
- **It does not claim anything CALLED the factory bodies.** Attendance is 0 by
  design; a stamp is not a door. The oracle compares constructed bytes and says
  so in its own output.
- **It does not claim the debt is paid.** It claims the debt is now *measured and
  decomposed*. 471 name-door rows remain in "a route exists somewhere and this
  run did not walk it".
- **It does not claim `SILENT` means "never ran"** for any row reached through a
  minted vtable slot or a static-init stamp. For those, SILENT is expected and
  carries no information. Only for a registry-only row is SILENT a proof of
  non-execution.
- **It does not claim `ASKED > 0` means the body executed N times.** It counts
  DOORS. A caching door resolves once and calls forever.
- **The four newly-visible attendance figures are from ONE run each.** They
  establish *live*, not a rate. `Renderer::Draw` was 5,045 on one in-game run and
  13,125 on another — both are "live", neither is a benchmark.
- **§3's class A does not distinguish "never had a door" from "had one and it was
  evicted"** — IOLANE's `Reconstruct` lifetime class. Unmeasured (§3.4).
- **The 31 UI factory classes are NOT covered** by the promoted oracle (§4.4).
  Their only evidence is still FACTUI1's offline table-replay result, with the
  structural limit §4.1 describes.
- **I did not run `sa-stress`.** The lock was contended by two priority lanes and
  a stress run is 6000 frames; the attendance instrument would gain a longer
  window and nothing else. **[ADDRESSEE: any Wave 30 lane running `sa-stress`
  anyway]** — add `BANISHED_RT_ATTEND=1` and the silent set shrinks for free.

---

## 9. FILES

| file | change |
|---|---|
| `standalone/main.cpp` | wire 3 unreached summaries; call `RtRowAttendanceReport()`; a comment recording why the factory oracle is NOT called here |
| `standalone/resolve_standalone.cpp` | per-row attendance counters at 4 hit sites + `RtRowAttendanceReport()` with 2 controls |
| `standalone/resolve.h` | declare it, with what it does and does not measure |
| `standalone/factory_byte_oracle.cpp` | **new** — the promoted oracle |
| `standalone/tool_harness.cpp` | one-shot call at in-game frame 120 (before the `s_enabled` gate; it is not a tool-harness feature) |
| `runtime/src/object/rt_factories.{h,cpp}` | `RtFactoryRoster()` — the family can be enumerated at runtime |
| `runtime/src/graphics/wave28_lane_gfxbody.cpp` | print the 2 counters that had no reader |

New env vars: **`BANISHED_RT_ATTEND=1`** (per-row port-door attendance),
**`BANISHED_FACTORY_ORACLE=1`** (the factory byte oracle). Both default OFF, both
print their arm state unconditionally.
