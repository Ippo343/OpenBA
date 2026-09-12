# WAVE 26 — LANE ATTEND

Two runs were asked for. **Both were done.** The cutover half needed **five**
arms rather than one (three configurations plus two controls), because the first
clean arm moved the wall *backwards* and the reason had to be separated from the
change — and a **sixth**, attempt 28, was run after lane FACTORY reported, to
settle their own prediction (§7).

Branch `wave26/attend`, cut from `wave26/base` at `7de42dd1`.

---

## 0. RUN LEDGER — every run, its tip, its exe md5, its full env, its verdict

The game-dir lock was **held by `lane-ATTEND` for the whole session** (acquired
before RUN 1, released after the last cutover arm; `status` read `HELD by
'lane-ATTEND' for 494s` mid-session and `free` after release). The md5 was taken
on **both sides of every copy**, per the kickoff's standing warning 1.

| # | Arm | tip | exe md5 | env | verdict |
|---|---|---|---|---|---|
| 1 | W17 attendance | `7de42dd1` | `45c72978bfbc80f17935d9701fe58977` | `BANISHED_W17_ATTEND=1 BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120` | clean exit 0, 0-byte crash log, **control GREEN**, 66 subject rows reported |
| 2 | **A** — control, attempt-26 env verbatim | `7de42dd1` | `45c72978…` | `RT_NO_DLL=1 RT_MINT=1 RT_MINT_CTOR=1 RT_ALLOC=1 RT_STRICT=1 RT_MINT_SHADOW=1 AUTOSTART=1 RT_MAINLOOPFRAME=0` | **StringTable factory trap — attempt 26's wall reproduced exactly** |
| 3 | **B** — clean (no `RT_MINT`, no `MAINLOOPFRAME`) | `7de42dd1` | `45c72978…` | as A minus `RT_MINT=1` and `RT_MAINLOOPFRAME=0` | **wall moved BACKWARDS** → `RtEngine_MainLoop_Ctor+0x1f6`, WRITE of 0 |
| — | *(void)* | `7de42dd1`+1 | `866e31a6…` | same as C | **INVALID, not a cutover run** — `mv` of the DLL failed "Device or resource busy" and the old harness launched anyway. Discarded; see §5. |
| 4 | **C** — clean, **fresh manifest** | `7de42dd1`+1 | `866e31a6e49bbf343a5c859c61d30a87` | as B | wall moved forward past MainLoop → `RtObject_External_Load+0x18f`, READ of 0x18, from `Game::LoadInputNameTables` |
| 5 | **A2** — control re-taken on the new exe | `7de42dd1`+1 | `866e31a6…` | as A | **StringTable factory trap** (identical to A) |
| 6 | **D** — blanket switch ON, `MAINLOOPFRAME` override REMOVED | `7de42dd1`+1 | `866e31a6…` | as A minus `RT_MAINLOOPFRAME=0` | **StringTable factory trap** (identical to A2) |

`866e31a6…` differs from `45c72978…` by exactly one source file:
`standalone/cutover_manifest.inc`, regenerated (§4).

`Runtime-x64.dll` was **restored and verified at 2,762,752 bytes after every
cutover arm**, including the invalid one.

Logs for every arm are committed under `docs/runtime_port/wave26_attend/`.

---

## 1. RUN 1 — THE PREMISE, CHECKED BEFORE THE RUN WAS SPENT

The brief said to distrust "cheapest". The premise checked out, and here is the
check rather than the assurance:

* `standalone/wave17_door_attend.cpp` on `wave26/base` carries **67 rows** — 1
  control + **66 subject rows**, not 63. Lane DATA's three Achievement rows are
  present, and so are the four other `?_typeInfo@` rows of the seven.
* The run printed `39 of 66 subject rows` — the **66** is the live proof the
  merge landed, since Wave 25's run printed 62/63.
* All seven target literals in the watch table are **byte-identical** to the
  literals at their consumer sites (checked by grep; a spelling drift between
  consumer and watch entry would read as `post=0` while the door worked — that
  is the one (a)-shaped failure this instrument could have, and it is excluded).
* **CONTROL GREEN**: `?_singleton@MainLoop@Engine@@` `init=1 post=2`.

**Cosmetic defect, not affecting any number:** the report's header string still
reads `(62 subject rows + 1 control)` while the computed line correctly says 66.
The literal is stale; the arithmetic is not.

### 1.1 THE SEVEN ROWS — PER-ROW VERDICT

Two ATTENDED, five `never`. **Every `never` below is (b): the route does not
reach it.** Not one is (a).

| # | Row | measured | verdict | why |
|---|---|---|---|---|
| 1 | `?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A` | `init=0 post=1` | **ATTENDED** | door opened after init on the New Game route |
| 2 | `?_typeInfo@GoalInstance@Achievement@@0V…A` | `init=0 post=1` | **ATTENDED** | door opened after init (`goal.cpp:169`) |
| 3 | `?_typeInfo@AchievementInstance@Achievement@@0V…A` | `init=0 post=0` | `never` — **(b)** | see §1.2 |
| 4 | `?_typeInfo@PackageFile@IO@@0V…A` | `init=0 post=0` | `never` — **(b)** | see §1.3 |
| 5 | `?_typeInfo@AnimationEvent@Graphics@@0V…A` | `init=0 post=0` | `never` — **(b)** | see §1.4 |
| 6 | `?_typeInfo@MaterialInstance@Graphics@@0V…A` | `init=1 post=0` | `never` — **(b)**, strongest case | see §1.4 |
| 7 | `?_typeInfo@Particle@Graphics@@0V…A` | `init=1 post=0` | `never` — **(b)**, strongest case | see §1.4 |

> ### ⚠ READ THIS BEFORE ACTING ON ANY ROW IN THE TABLE ABOVE
>
> **This instrument counts DOORS, NOT EXECUTIONS.** It increments on
> `ResolveExport(<mangled>)` and on nothing else. Therefore:
>
> * **`never` cannot distinguish "the route never reaches this class" from "the
>   route reaches it constantly, but only through calls that are not doors."**
>   An in-DLL `rel32` call and a RIP-relative `LEA` stamp are not doors, and this
>   project's standing trap is exactly that. For a *promotion* decision the
>   consequence is the same — the zero is not evidence about the row — but
>   **`never` is NOT proof that a class is unused**, and it must never be quoted
>   as one.
> * **A row whose only door is `ResolveAddr(Module::Runtime, <va>)` is
>   STRUCTURALLY INVISIBLE here and prints the same `never` as a dead row.**
>   Measured independently by lane PROMOGFX on its own boot:
>   `??_7Geometry@Graphics@@6B@` and `??_7AnimationEvent@Graphics@@6B@` both read
>   `never init=0 post=0` while being reached by address at `scene.cpp:212`
>   (`0x180154e60`) and `animation.cpp:156` (`0x180154bf0`). The screen watches a
>   channel those rows do not use. **Before reading any `never` in §1.5, check
>   whether that row's consumers ask by NAME at all.**
> * `post==1` does **not** mean "ran once" — nearly every consumer caches in a
>   function-local static.
>
> What would lift the first limitation: an execution counter inside the consumer
> bodies (for the three Graphics rows, `graphics_typeinfo_accessors.cpp:132/136/140`).
> Re-running the autostart route will keep returning the same answer.

**The general (a)-refutation, which applies to all five.** In this same run,
rows reached through the *identical* `RT_DATA_SLOT → RtResolveCached →
ResolveExport` idiom read `post>0` — `?_typeInfo@Material@Graphics@@` (post=1)
and `?_typeInfo@GeneratedImageBuffer@Graphics@@` (post=1) are the same shape in
the same file family. The idiom is therefore proven live *in this run*, so a
zero cannot be blamed on the door mechanism. What remains of (a) is row-specific
spelling drift, excluded above.

### 1.2 `?_typeInfo@AchievementInstance@` — (b), and the route is named

Its **only** consumer in the tree is `gameupdateloop_serialize.cpp:195`, inside
`EnsureResolved()`, which is called from the GameUpdateLoop serialize family.
**That function never ran**: its `[GUL::Serialize] resolved: …` stderr line has
**0 occurrences** in both logs. Two independent lines in the same log corroborate
that the whole save/load family was dormant:

```
[SaveMetaOracle] bodyCalls=0 calls=0 sizeDiffs=0 byteDiffs=0
[RefTable] calls=0 -- the body never ran. That is EXPECTED on a run that neither saved nor loaded
```

A New-Game-and-quit route neither saves nor loads. **What would change this
verdict:** a run that saves (`BANISHED_PAUSEMENU` → save, or an autosave tick).
Until then this row's silence says nothing about the row.

### 1.3 `?_typeInfo@PackageFile@IO@@` — (b), with a sharp positive nearby

Its only consumers are the three reflection bodies `RtIo_PackageFile_Type` /
`_TypeId` / `_TypeName` (`package_file.cpp:531/542/552`). Meanwhile
**`??_7PackageFile@IO@@6B@` itself reads `ATTENDED post=1`** in the same run — so
`PackageFile` objects *are* constructed through our door on this route, and
nobody asks them their type. That is as clean a (b) as this instrument can
produce: the class is live, the reflection triple is not called.

### 1.4 The three Graphics rows — (b), and rows 6 and 7 prove their own door works

All three are read only by vtable-slot bodies in
`graphics_typeinfo_accessors.cpp:132/136/140` (`Type` / `TypeName` /
`GetFileVersion`).

* **Rows 6 and 7 (`MaterialInstance`, `Particle`) have `init=1`.** The symbol
  resolved successfully once, from `reg_unreg_lane3.cpp:164/195`. **The door
  demonstrably works for these two rows specifically** — `post=0` can only mean
  no post-init consumer ran. This is the tightest (b) of the seven.
* **Row 5 (`AnimationEvent`) has `init=0`** — it has no eager resolve, so it
  rests on the general (a)-refutation plus the corroboration below.
* **Corroboration for all three:** in the same run `??_7AnimationEvent@Graphics@@6B@`,
  `??_7MaterialInstance@Graphics@@6B@`, `??_7Particle@Graphics@@6B@`,
  `??_7ParticleInstance@` and `??_7ParticleSystemInstance@` **all** read
  `never post=0`. Nothing fetched any of those tables through a door, so the
  slot bodies that read these nodes never dispatched.

This is the documented `??_7MaterialInstance@Graphics@@` precedent the brief
named, now measured on the `?_typeInfo@` twin as well.

**One honest caveat, stated because it changes what (b) means for a porting
lane.** "The route does not reach it" and "the route reaches these objects but
only through in-DLL calls that are not doors" are both consistent with
`post=0`, and this instrument cannot separate them — it counts *doors*, not
executions. For a promotion decision the consequence is the same (the zero is
not evidence about the row), but a lane hoping this run proves the class is
*unused* would be over-reading it. **What would separate them:** an execution
counter inside the three accessor bodies, not a door counter.

### 1.5 Everything else the run measured (for the lanes that asked)

Full table: `docs/runtime_port/wave26_attend/run1_w17attend.txt`. Same run, same
GREEN control.

IO vtables — `FileStream` post=2 · `CompressedStream` post=2 ·
`CompressedMemoryStream` post=2 · `CompressedFileStream` post=2 ·
`Stream` post=6 · `PackageFile` post=1 · `BinaryFileStream` post=2 ·
`MemoryStream` post=1 · `VisitorStream` post=2 — all **ATTENDED**;
`ReaderWriter` post=0 · `TextFileStream` post=0 · `BinaryReader` post=0 ·
`BinaryWriter` post=0 — `never`.

Video — **all twelve `??_7*Data@Video@@6B@` rows ATTENDED** (post=1, except
`ImageBufferData` and `RenderStateData` post=2); all four `_first*` statics
ATTENDED; `Configuration` post=9, `Device` post=12. **All six
`?_typeInfo@…@Video@@` rows `never`.**

Graphics — `Light` and `SunLight` ATTENDED; `Geometry`, `AnimationEvent`,
`MaterialInstance`, `Particle`, `ParticleInstance`, `ParticleSystemInstance`
`never`.

**UNMEASURED, not zero:** `??_7Thread@System@@6B@` is **not in the watch table**,
so no run has ever produced an attendance figure for it (raised by lane PROMOIO;
confirmed here by reading the table). It appears as `[RtUnresolved]` at cutover
in arms B/C, which is a different fact.

---

## 2. RUN 2(a) — THE `BANISHED_RT_MINT=1` / C3 DISCREPANCY, SETTLED FROM SOURCE

**C3 is RIGHT, and the mechanism is an ordering one.** Settled by reading the
code, then confirmed in a log.

`RtDataMintInit()` (`standalone/rt_data_mint.cpp:1003`) runs, in this order:

```
if (EnvOn("BANISHED_RT_MINT")) RegisterMintedData();       // line 1003
...
RtCutoverPublishManifest(EnvOn("BANISHED_RT_CUTOVER_MANIFEST"));   // line 1025
```

`RegisterMintedData()` (line 940) walks **all three tables with no skip list**
and calls `RegisterRuntimeExport(sym, storage)` **plus**
`RegisterRuntimeAddr(dllVa, storage)` for every row that has a symbol.
`??_7Dialog@UI@@6B@` is `kMintVtables` entry at `rt_data_mint_tables.inc:1567`
with a symbol, so it is published — **by name and by address** — before the
manifest is even consulted. The manifest's `continue` on a WITHHELD row does not
un-register anything; it only declines to re-register.

**Confirmed in the arm-A log, in the emitted order:**

```
[RtMint] registered 202 by name, 208 by address          <-- Dialog published HERE
[CutoverManifest] ==== DLL ABSENT (cutover) ==== 46 tagged gap rows
[CutoverManifest] WITHHELD    UI        ??_7Dialog@UI@@6B@
[CutoverManifest]   ^ NOT published, and 5 by-name door(s) … WILL resolve NULL.
[CutoverManifest] published 44, withheld 1, conditional 1, unminted 0, failed 0 (of 46)
```

**Therefore, of attempt 26's report:**

* `withheld 1` describes the manifest's **intent**. In that process the row
  **was** published.
* The "5 by-name doors WILL resolve NULL" prediction was **false in that run** —
  they resolved to our mint.
* The attempt-26 write-up's "It did not fault there… a wall that announces
  itself in advance and is then ruled out" **ruled out nothing**: the prediction
  was never in force. That sentence should be corrected.

This is not a defect in `cutover_manifest.cpp`, which is correct on its own
terms. It is a defect in **running it behind a sledgehammer that had already
fired**.

---

## 3. RUN 2(b) — `BANISHED_RT_MAINLOOPFRAME=0`: NOT LOAD-BEARING

Isolated on **one exe**, one variable, with its own control:

| arm | exe | `RT_MINT` | `MAINLOOPFRAME=0` | wall |
|---|---|---|---|---|
| A2 (control) | `866e31a6…` | on | **set** | StringTable factory trap |
| D | `866e31a6…` | on | **absent** | StringTable factory trap |

**Identical.** Removing the override does not move the wall. Lane BODY's
`?Frame@MainLoop@Engine@@` is not implicated in attempt 26's failure, and
attempt 28 need not carry the override. Arm A on the *other* exe gives the same
wall a third time, so the StringTable wall is stable across 3 runs and 2 exes.

---

## 4. RUN 2(c) — THE CLEAN ARM, AND THE THING IT UNCOVERED

### 4.1 `standalone/cutover_manifest.inc` was STALE on `wave26/base` — MEASURED

```
$ python tools/cutover_manifest.py --check
STALE: standalone\cutover_manifest.inc does not match a fresh derivation
cutover_manifest: 60 tagged gap rows -- CONDITIONAL 1, REQUIRED 58, WITHHELD 1
```

The committed `.inc` held **46 rows / REQUIRED 44**. A fresh derivation holds
**60 / REQUIRED 58**. The missing **14 are exactly lane DATA's 14**, including
`?_singleton@MainLoop@Engine@@1PEAV12@EA`, the three `Error@System` rows, five
`Timer@System` rows, `?_disableHandler@Debug@System@@`,
`?_singleton@Configuration@System@@`, `?_singleton@Engine@1@`,
`?_loader@LoaderLoop@Engine@@` and
`?_singleton@AchievementManager@Achievement@@`.

**So the attempt-26 write-up's claim that its numbers "held after lane DATA added
14 more tagged rows *after* MANIFEST had finished measuring — which is what the
generator (rather than a list) was for" is exactly backwards.** The generator was
never re-run after DATA landed. `44 = 46 − 1 − 1` is MANIFEST's own pre-DATA
count. The generator's whole selling point went unexercised, and the number that
would have revealed it sat in the log looking correct.

**This was invisible while the blanket switch was on**, because the blanket
switch published those 14 rows anyway. Removing the switch is what exposed it —
a generated file going stale with no detector, caught only by removing the thing
that was masking it. I have regenerated it (`--inc`) and committed it; `--check`
is now clean.

### 4.2 The wall moved BACKWARDS in **both** clean arms — and that is about the switch

| arm | manifest | rows published at cutover | wall |
|---|---|---|---|
| A / A2 / D | either | `registered 202 by name` (blanket) | `?CreateNewObject@StringTable@UI@@` factory trap |
| **B** | stale (44) | 40 promoted + 44 manifest | `RtEngine_MainLoop_Ctor+0x1f6` — **WRITE of 0** |
| **C** | fresh (58) | 40 promoted + 58 manifest | `RtObject_External_Load+0x18f` — **READ of 0x18** |

Both clean arms are **backwards** relative to the blanket-switch arms, and each
names its own cause in the log immediately before the fault:

* **B** — `[RtResolve] MISS "?_singleton@MainLoop@Engine@@1PEAV12@EA" -> NULL`,
  then the MainLoop ctor writes through the null cell.
  Stack: `WinMain → Game::RunGameMain+0x84b → RtEngine_MainLoop_Ctor+0x1f6`.
  Cause: the stale manifest, §4.1. **Fixed by regenerating it** — arm C gets past
  this point, which is the cleanest possible confirmation of the diagnosis.
* **C** — `[RtResolve] MISS "??_7FileStream@IO@@6B@"` and
  `"??_7CompressedFileStream@IO@@6B@"`, then a null-vptr read.
  Stack: `WinMain → Game::RunGameMain+0x968 → Game::LoadInputNameTables+0x177 →
  ExternalFactory::FindOrLoad+0x2b1 → RtObject_External_Load+0x18f`.

### 4.3 THE STRUCTURAL FINDING — stated as a finding, not a plan

`BANISHED_RT_MINT=1` publishes **202 rows by name**. Without it, cutover
publishes **40 promoted + 58 manifest ≈ 98**. The manifest was scoped to the
**tagged gap rows only** — it was never a publisher for the other ~104 minted
rows, and nothing else is either.

**So "do not use the blanket switch at cutover" (C3) is correct about the one
WITHHELD row and, as written, leaves cutover with no publisher for ~104 rows.**
Attempt 26's boot depth was bought by the sledgehammer, not by the manifest. The
two clean arms are what that costs today, measured: two walls, both null-data,
both in rows the switch was covering.

The narrow gap between the two positions is exactly **one row**
(`??_7Dialog@UI@@6B@`). Naming the shape of a fix is not the same as having
measured it, and I did not measure it: this lane reports that the blanket switch
and the manifest are **not** substitutes, and that the choice between them is
currently a choice between *one knowingly-harmful row published* and *~104 rows
unpublished*.

### 4.4 WHAT ATTEMPT 27 IS, ON THE RECORD

**The wall is `?CreateNewObject@StringTable@UI@@SAPEAVBase@Object@@XZ`,
unchanged from attempt 26**, on the only configuration that boots as deep as
attempt 26 did (3 runs, 2 exes). Lane FACTORY's subject is unaffected by
anything here. I ran on `wave26/base` + my own manifest regeneration; FACTORY's
port had **not** landed on my branch.

**A cutover boot is a GATE.** Arms B and C each named ONE new symbol. Neither is
a roadmap and neither is written here as one.

---

## 5. AN INSTRUMENT DEFECT I HIT, AND FIXED — `tools/cutover_attempt27.sh`

The first arm-C launch printed `mv: cannot move … Device or resource busy`
(a stale handle on `Runtime-x64.dll`) **and launched anyway**. `set -u` without
`set -e`, and no check on the move. That run was **not a cutover run at all** —
the DLL was present the whole time — yet it produced a 90-second hang and a full
set of plausible logs. It is discarded, and it is in the ledger above because a
discarded run that is not recorded is indistinguishable from one that never
happened.

`tools/cutover_attempt27.sh` now **hard-aborts (exit 2) if `Runtime-x64.dll` is
still present after the move, before launching anything**. The harness otherwise
keeps attempt 26's properties exactly: the unconditional `EXIT/INT/TERM` restore
trap with a byte-count re-check (verified 2,762,752 bytes after all five arms),
and it additionally takes the **env set as an argument** so a control arm can be
re-run on the same exe — which is what made §3 answerable at all.

---

## 6. `[VideoInterface] *** DETOUR FAILED ***` — SWEEP FIXED IT, VERIFIED

The known false alarm **does not appear** in any of the five cutover arms. What
appears instead is accurate and carries its own detector:

```
[VideoInterface] detour not installed for Video::Device::InitializeInterfaces (0x1800F79F0) -- EXPECTED, NOT A FAULT.
[VideoInterface]   VideoInterfacePrefilled() is still the detector: if it reads false AFTER Engine::Startup, the slot really was not filled and THAT is the fault to chase.
```

No session time need be budgeted for it.

---

## 7. ATTEMPT 28 — FACTORY MERGED. THE WALL MOVED, AND NAMED ONE SYMBOL.

Run after lane FACTORY reported. **Scratch branch `wave26/attend-x28-scratch`**
= `wave26/attend` + `wave26/factory` (`4d4402dd`), merged clean; the scratch
branch is **not** part of this lane's deliverable and `wave26/attend` does not
contain FACTORY's work. Exe md5 `386f64dc3a12b7c86ba7da243483b941`, md5-checked
on both sides of the copy, lock held, DLL restored and verified at 2,762,752
bytes. Env = attempt 26's, plus FACTORY's `BANISHED_RT_MINT_FACTORYLIST=1`.

**FACTORY's registration reaches `RtDataMintInit` on the cutover path** — the
log confirms their own screen from inside a real cutover boot:

```
[RtMint] BaseTypeInfo factories: 104 rows x 2 slots = 208; 2 PORTED, 204 ABORT-TRAP, 2 absent in retail too
```

**The StringTable FATAL is gone.** It is replaced by an ACCESS_VIOLATION further
along the *same* call path, and the log names the cause on the line before it:

```
[RtUnresolved] ??_7StringTable@UI@@6B@
[RtResolve] MISS "??_7StringTable@UI@@6B@" -> NULL. … If this is cutover, this symbol still needs minting.

Fault: READ of 0x10 at RtObject::GetTypeIdVirtual+0x1f
  <- RtObject_Base_IsDerivedFrom+0x4e
  <- RtObject_External_Load+0x33c
  <- ExternalFactory::FindOrLoad+0x2b1
  <- Game::LoadInputNameTables+0x177
```

**Confirmed by source:** `standalone/rt_data_mint_tables.inc` carries
`?_typeInfo@StringTable@UI@@` (line 2196) and its two factory slots, and carries
**no `??_7StringTable@UI@@6B@` vtable row at all**. So the newly-ported
`CreateNewObject` runs, its object is stamped with a **null vptr**, and the very
next thing retail does — `IsDerivedFrom` → a virtual `GetTypeId` — reads through
it. This is the Wave 24 `??_7Stream@Audio@@6B@` shape: **a row nothing owns.**

**Against FACTORY's stated prediction:** they predicted the FATAL would move
onto *another minted factory*. It moved **off** StringTable (their half is
confirmed, and it is not their bug — their two slots are PORTED in a live
cutover boot), but **not** onto another factory: onto the missing *vtable* row
for the very class they ported. The next name is `??_7StringTable@UI@@6B@`.

**ONE NAME. NOT A ROADMAP.** A cutover boot is a gate; it can only name what it
reached. Nothing here says `??_7StringTable@UI@@6B@` is the last thing between
the tree and a booting cutover, and it is not written as a plan.

---

## 8. THE DURABLE FIX — the stale `.inc` class is now UNMERGEABLE, not merely detectable

§4.1 found a generated file that had gone stale with no detector wired to
anything. `--check` existed the whole time and answers in a second; it did not
help, because **a detector only helps someone who runs it.** Worse, the defect
was *masked*: the blanket switch published the 14 missing rows anyway, so
attempt 26 booted deep and `published 44, withheld 1` read as confirmation —
**the number matched its prediction BECAUSE it was stale.**

So the fix is not another detector. `tools/cutover_manifest.py` gains **`--gate`**
and the Makefile gains **`check-cutover-manifest`**, on `standalone`,
`standalone-debug` **and** `standalone-perf` — beside `check-log-strip`, which
exists for the same reason and whose own history (Wave 25 integration) is that a
guard missing from `standalone-debug` is a guard that does not fire.

**Why `--gate` and not `--check`.** `--check` is content-exact, and part of that
content is a tree-wide by-name **door count**. Any lane adding one
`ResolveExport()` makes the file "stale" — **measured**: merging lane FACTORY
moved `??_7Base@Object@@6B@` from `doors=16` to `17` and
`??_7External@Object@@6B@` from `15` to `16`, with every row and every
disposition identical. A gate that fails a six-lane wave's builds over a
cosmetic count is a gate that gets commented out, and then the load-bearing case
is unprotected again. **A check nobody can afford to leave on is worth the same
as one nobody runs.** So `--gate` splits the file:

| field | class | gate |
|---|---|---|
| row SET, each row's DISPOSITION, minted flag | what the process publishes at cutover | **FAIL** |
| a REQUIRED row with no minted storage | a door that resolves NULL | **FAIL** |
| an UNDECLARED scope | an unclosed tag | **FAIL** |
| by-name door COUNT | feeds a printed prediction only | WARN, exit 0 |

**Three positive controls, run, because a screen looking for the absence of a
shape must be handed one — "0 hits" is otherwise indistinguishable from a dead
parser:**

| control | result |
|---|---|
| delete `?_singleton@MainLoop@Engine@@` (i.e. reproduce the Wave 25 defect) | `GATE FAIL: row MISSING …`, exit 1 |
| flip one row `REQUIRED` → `WITHHELD` | `GATE FAIL: disposition/minting CHANGED …`, exit 1 |
| change one door count 16 → 99 | `GATE WARN … the build is NOT failed`, exit 0 |
| unmodified tree | `gate: OK (60 rows …)`, exit 0, and it prints inside a real `make standalone-debug` |

A fresh derivation producing **zero** rows is treated as a parser failure and
fails the gate rather than passing it — the "dead parser reads as a clean
result" case, closed explicitly.

**Re-run those three controls if anyone touches `_gate()`.** The Makefile
comment carries them, so the addressee is whoever edits it, not a future wave.

---

## 9. MEASURED vs INFERRED

**MEASURED** — every row of the run ledger; the 66-row watch table and its GREEN
control; all seven per-row attendance figures; the `[GUL::Serialize]`,
`[SaveMetaOracle]`, `[RefTable]` zeros; the `[RtMint] registered 202 by name`
line preceding the manifest's `withheld 1`; `cutover_manifest.py --check`
reading STALE at 46 vs 60 and clean after regeneration; all six walls and their
symbolised stacks; A2-vs-D showing `MAINLOOPFRAME` is not load-bearing; attempt
28's `2 PORTED, 204 ABORT-TRAP`, its `??_7StringTable@UI@@6B@` MISS and the
absence of that row from the mint table; the FACTORY-merge door-count drift
(16→17, 15→16) with dispositions identical; all four `--gate` control results
and the gate firing inside a real `make standalone-debug`.

**INFERRED** — the (a)/(b) classification of the five `never` rows (inference
from consumer-site reachability, with the evidence given per row and the limit
boxed under §1.1); the reading that ~104 rows have no cutover publisher
(arithmetic on two measured registration counts, not a per-row enumeration);
that the null vptr in attempt 28 comes from the missing `??_7StringTable@UI@@6B@`
row (strongly evidenced — the MISS line, the absent table row, the fault being a
virtual call one frame after the factory — but I did not step the object's vptr
in a debugger).

**NOT DONE** — no per-row check that each of the ~104 blanket-only rows is
actually needed at cutover; no arm combining the blanket switch with a
`Dialog`-only exclusion; no saving/loading route to decide
`?_typeInfo@AchievementInstance@` on its merits; **the `??_7StringTable@UI@@6B@`
mint row is not written** (attempt 28 names it; nobody has ported it). Each is
named with what it would take, not left as a condition with no addressee.

**A NOTE ON `wave26/attend-x28-scratch`.** It exists only to carry the attempt-28
measurement and merges FACTORY's branch. The integrator should merge
`wave26/attend` and `wave26/factory` independently; the scratch branch is
disposable and nothing depends on it.

**NOT DONE** — no per-row check that each of the ~104 blanket-only rows is
actually needed at cutover; no arm combining the blanket switch with a
`Dialog`-only exclusion; no saving/loading route to decide
`?_typeInfo@AchievementInstance@` on its merits. Each is named with what it
would take, not left as a condition with no addressee.
