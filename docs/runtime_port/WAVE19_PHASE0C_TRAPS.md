# WAVE 19 phase 0c — lane TRAPS: the trap counts, re-measured

> **SEALED at `9013067`. Annotated, not altered.** A post-report tranche —
> [`WAVE19_PHASE0C_TRAPS_TRANCHE2.md`](WAVE19_PHASE0C_TRAPS_TRANCHE2.md) —
> measured this document's `slots` column against the DLL in Ghidra and found
> **one extent error**: `??_7ReaderWriter@IO@@6B@` is **31 slots / TRAP=1**, not
> 36 / TRAP=4, so IO's minted trap total is **24, not 27**. That tranche also
> **REFUTES §4.2 item 2 and §6 item 4** of this file, and supplies measured slot
> counts for four of the five vtables §4.1 and §6 record as UNMEASURED.
> **Read the tranche before scoping anything off §4 or §6.**

Worktree `C:/code/GIT/wt-w19-traps`, branch `wave19/traps`, cut from
`wave19/base` @ `ebbec6e`. **Every figure below was measured on `ebbec6e`**,
debug build (`mingw32-make standalone-debug`), DLL present, under
`tools/game_dir_lock.sh run lane-TRAPS 1800 -- …` (the self-releasing form, as
its own statement; `assert-held lane-TRAPS` gates each deploy inside the runner).

**Nothing was ported. Nothing was promoted. Nothing outside `docs/` was edited.**
This cell produces a denominator for the four Wave 19 porting lanes.

## 0. Headline, in one paragraph

The re-measurement **confirms every IO figure CEIL §3.4 published** and refutes
one Video figure. Of the **17 rows CEIL §3.4 states explicitly, 16 are unchanged
and exactly 1 moved**: `??_7ImageBuffer@Video@@6B@` went `10/3/TRAP=7` →
`10/10/TRAP=0`, **READY and [PROMOTED]** (Wave 18 lane VRES's `SaveImageToFile`
work). The plan's three named expectations all **hold**: `??_7BinaryReader@`
**1 trap of 26**, `??_7Stream@` **1 of 7**, `??_7BinaryWriter@` **26/26 TRAP=0**.

The scoping news is not in the trap counts, it is in their **overlap**: the eight
blocked minted IO rows carry **27 trap slots but only 20 distinct functions**,
and one of those functions — `?GetSource@Stream@IO@@` — is the trap in **four
different rows**. §6 is the revised cheapest-rows list.

## 1. What was measured, and what the instrument's subject actually is

`BANISHED_RT_MINT_SHADOW=1`, exit phase, on the gameplay route
(`BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120`),
`rc=0`, **0-byte crash log**, `[RtMintShadow] ==== clean: 0 problem(s) ====`.
Boot polled with `tools/boot_state.sh --wait 300` → `RUNNING`, never a `grep -q`
loop. `BANISHED_AUTOQUIT` was **not** set, so autostart's New Game path was not
overridden (`reference_autoquit_overrides_autostart_newgame.md`) — the log shows
the whole route: `MainMenuDialog::OnCommand id=0 -> OpenNewGame`, the world
build, then the in-game `MainMenuDialog::OnCommand id=6 -> App::Quit`.

Raw evidence: `docs/runtime_port/wave19_phase0c/` — `run1/` (default),
`run2_attend/`, `run3_ctrl/`, each with its full `standalone_debug.log`, plus
`readiness_exit_run1.txt` (the exit-phase report on its own).

### 1.1 The screen's subject, stated — and the one thing it cannot name

Wave 18 §4 asks whether an instrument can state its own subject. This one **can,
and does**: it prints `vtables : 57, 546 slots`, prints **all 57 rows**
(READY *and* BLOCKED, not just failures), and for every BLOCKED row prints
**each trapped slot by mangled name**, falling back to `DLL 0x…` where the slot
is unexported. Its verdict comes from the **live port registry** (`OurPort()` →
`RtPortLookupExport` / `RtPortLookupAddr`), not from a source grep, so a
registration whose name arrives through a variable is visible to it
(`reference_literal_grep_blind_to_variables.md`).

**Its subject is `kMintVtables` — the harvested mint table — NOT a namespace.**
That distinction is load-bearing for this wave, and the screen does not say it:

> The plan's DATA bucket lists **13 IO vtables**. The readiness screen contains
> **9 IO rows**. The five with **no readiness row at all** are exactly
> `??_7{BinaryFileStream, CompressedFileStream, FileStream, TextFileStream,
> VisitorStream}@IO@@6B@` — **independently reproducing §0b's harvest gap from a
> different instrument** — and the ninth row, `??_7Visitor@IO@@6B@`
> (26/25/**TRAP=1**), is an IO row the plan's 13 does **not** contain.

A reader scoping IO off this table alone would see 9 rows where the plan has 13,
and would miss `??_7Visitor@`. This is a **deflating** blind spot, not an
inflating one (`reference_a_screen_that_cannot_name_its_subject.md` is about the
inflating kind, which manufactures work) — but it is a blind spot, and it is why
every diff in this document is over **named symbols**, never over counts.

I did **not** change the instrument. The fix is one line in the readiness header
naming `kMintVtables` as the subject, and it belongs with `tools/rt_mint_gen.py`'s
harvest gap (§0b): fixing the harvest makes the screen's subject *correct*
rather than merely better documented.

### 1.2 Parser reconciliation — the denominator on my own extraction

My extraction script parsed **54** of the 57 rows. The 3 it missed are the rows
labelled `(unexported @0x…)` — a label containing a space, which the symbol
regex rejected. **This was caught, not shipped**: the 54 named rows carry 532
slots, the 3 unnamed rows carry 5 + 2 + 7 = 14, and **532 + 14 = 546**, the
screen's own total. Both halves reconcile exactly, so the extraction covers
**57 of 57 rows and 546 of 546 slots, 0 unaudited**.

An earlier version of that script also silently dropped every `READY` row — the
same padding problem — and would have reported "8 IO minted rows" while omitting
`??_7BinaryWriter@`, the one row this wave cares most about. The reconciliation
above is what caught it; nothing else would have.

## 2. Controls

### C1 — positive control, required to read non-zero (run3)

`BANISHED_RT_BINWRITER=0` de-registers `RtIoBinaryWriterRegister()`
(`runtime/src/io/rt_io_port.cpp:191`). If the screen prints a constant, nothing
moves.

| | run1 (default) | run3 (`BANISHED_RT_BINWRITER=0`) |
|---|---|---|
| `??_7BinaryWriter@IO@@6B@` | **READY** 26 / 26 / TRAP=0 | **BLOCKED** 26 / 23 / **TRAP=3** |
| the other **56** rows | — | **identical** |

**Scored on applicable cases only** (doctrine §3): the gate has **exactly one
applicable row**; that row moved, by the mechanism the control names, in the
predicted direction — **1 of 1, GREEN**. The 56 inapplicable rows are scored
separately and are a *specificity* result, not part of the control's
denominator: the screen is not globally noisy.

Byproduct for lane IOBASE: the switch accounts for only **3 of BinaryWriter's
26** slots; the other 23 come from a different registrar
(`BANISHED_RT_ENCODING`). "`??_7BinaryWriter@` is complete" is **not** the same
statement as "`BANISHED_RT_BINWRITER` completes it".

### C2 — insensitivity control (run2)

run2 adds `BANISHED_W17_ATTEND=1`, unrelated to the port registry. Its readiness
table is **identical to run1's across all 57 rows**. So run3's movement is
attributable to the gate it names, and not to "a second run differs".

### C3 — phase control: RED as a control, and reported rather than dropped

CEIL's attendance instrument earned its keep on a phase boundary, so I ran the
same test here. The `init` table and the `exit` table are **identical across all
57 rows**. That is expected — `RtDataMintInit()` runs inside `RuntimePortInit()`,
and the `AddressRegistryInit()` that follows registers *exe* addresses that no
minted runtime slot reads — but it means **the phase boundary is not a control
for this screen** and must not be quoted as one. C1 is the control that works.

### C4 — attendance control (run2)

`[W17Attend] CONTROL ?_singleton@MainLoop@Engine@@1PEAV12@EA: init=1 post=2 ->
GREEN`. That instrument suppresses its whole table when the control reads zero,
so the zeros in §5 are trusted only because this printed first.

### What could still be wrong

The readiness verdict is a function of the **live registry under this run's env**.
Both measured runs used the default flag set (`kDefaultOnFlags`, no kill
switches) — the same condition CEIL measured under. **A lane running with any
`BANISHED_RT_*=0` will see different, higher trap counts**, as C1 demonstrates on
purpose. Quote these as "default flags", never as "the tree's".

## 3. Set-diff against CEIL §3.4 [INHERITED @ `3da1503`]

Diffed as a **set of named symbols**, not as counts
(`reference_diff_the_set_not_the_count.md`). CEIL states 17 rows explicitly: its
15-row BLOCKED table, plus `??_7BinaryWriter@` (26/26/0) and `??_7Geometry@`
(9/6/0/3) from its blocked-on-attendance list.

**16 of 17 UNCHANGED · 1 MOVED · 0 ABSENT.**

| row | CEIL @ `3da1503` | now @ `ebbec6e` | |
|---|---|---|---|
| `??_7ImageBuffer@Video@@6B@` | 10 / 3 / **7** / 0 BLOCKED | 10 / 10 / **0** / 0 **READY [PROMOTED]** | **MOVED** |
| `??_7CompressedStream@IO@@6B@` | 9 / 1 / 8 / 0 | 9 / 1 / **8** / 0 | same |
| `??_7MaterialInstance@Graphics@@6B@` | 9 / 2 / 7 / 0 | 9 / 2 / **7** / 0 | same |
| `??_7CompressedMemoryStream@IO@@6B@` | 9 / 3 / 6 / 0 | 9 / 3 / **6** / 0 | same |
| `??_7SunLight@Graphics@@6B@` | 11 / 5 / 6 / 0 | 11 / 5 / **6** / 0 | same |
| `??_7ParticleSystemInstance@Graphics@@6B@` | 10 / 5 / 5 / 0 | 10 / 5 / **5** / 0 | same |
| `??_7PackageFile@IO@@6B@` | 9 / 5 / 4 / 0 | 9 / 5 / **4** / 0 | same |
| `??_7ReaderWriter@IO@@6B@` | 36 / 2 / 4 / 30 | 36 / 2 / **4** / 30 | same |
| `??_7Particle@Graphics@@6B@` | 9 / 2 / 4 / 3 | 9 / 2 / **4** / 3 | same |
| `??_7ParticleInstance@Graphics@@6B@` | 10 / 5 / 4 / 1 | 10 / 5 / **4** / 1 | same |
| `??_7MemoryStream@IO@@6B@` | 8 / 6 / 2 / 0 | 8 / 6 / **2** / 0 | same |
| `??_7AnimationEvent@Graphics@@6B@` | 9 / 2 / 2 / 5 | 9 / 2 / **2** / 5 | same |
| `??_7BinaryReader@IO@@6B@` | 26 / 25 / 1 / 0 | 26 / 25 / **1** / 0 | same |
| `??_7Stream@IO@@6B@` | 7 / 1 / 1 / 5 | 7 / 1 / **1** / 5 | same |
| `??_7Light@Graphics@@6B@` | 11 / 5 / 1 / 5 | 11 / 5 / **1** / 5 | same |
| `??_7BinaryWriter@IO@@6B@` | 26 / 26 / 0 / 0 READY | 26 / 26 / **0** / 0 READY | same |
| `??_7Geometry@Graphics@@6B@` | 9 / 6 / 0 / 3 READY | 9 / 6 / **0** / 3 READY | same |

**Independent corroboration of the single move.** CEIL reported "**249** slots
minted from our own code"; this run reports **256**, and `256 − 249 = 7` is
exactly ImageBuffer's seven newly-ported slots. An aggregate that was not used to
build the table agrees with the table.

**Confirm / refute, as the brief demanded — no figure inherited:**

* `??_7BinaryReader@` **1 trap of 26** — **CONFIRMED**. The trap is **slot 0**,
  unexported, `DLL 0x180023a10`.
* `??_7Stream@` **1 of 7** — **CONFIRMED**, and CEIL's presentation is worth
  sharpening: the trap is **slot 1,
  `?GetSource@Stream@IO@@UEBA?AVString@System@@XZ`**, a *named export*. Slot 0
  (`0x18000c610`, the deleting destructor) is **already ported**.
* `??_7BinaryWriter@` **26/26 TRAP=0** — **CONFIRMED**; still the only fully
  complete row in CEIL's subject, and still `post=0` (§5).
* `??_7ReaderWriter@` at 30-of-36 purecall and `??_7Stream@` at 5-of-7, the
  premises of phase **0a** — **CONFIRMED unchanged**. 0a's disassembly question
  is untouched by this cell and remains open.

## 4. POSSIBLE? — the re-measured table (57 of 57 rows, 546 of 546 slots)

`slots / ported / TRAP / purecall`. READY means every non-purecall slot is our
port. Measured on **`ebbec6e`, debug, default flags, DLL present**.

### 4.1 The nine minted IO rows — Wave 19's subject

| row | slots | ported | TRAP | purecall | verdict |
|---|---|---|---|---|---|
| `??_7BinaryWriter@IO@@6B@` | 26 | 26 | **0** | 0 | READY |
| `??_7BinaryReader@IO@@6B@` | 26 | 25 | **1** | 0 | BLOCKED |
| `??_7Visitor@IO@@6B@` | 26 | 25 | **1** | 0 | BLOCKED |
| `??_7Stream@IO@@6B@` | 7 | 1 | **1** | 5 | BLOCKED |
| `??_7MemoryStream@IO@@6B@` | 8 | 6 | **2** | 0 | BLOCKED |
| `??_7ReaderWriter@IO@@6B@` | 36 | 2 | **4** | 30 | BLOCKED |
| `??_7PackageFile@IO@@6B@` | 9 | 5 | **4** | 0 | BLOCKED |
| `??_7CompressedMemoryStream@IO@@6B@` | 9 | 3 | **6** | 0 | BLOCKED |
| `??_7CompressedStream@IO@@6B@` | 9 | 1 | **8** | 0 | BLOCKED |

**Plus the five with NO readiness row** (§1.1, plan §0b): `??_7BinaryFileStream@`,
`??_7CompressedFileStream@`, `??_7FileStream@`, `??_7TextFileStream@`,
`??_7VisitorStream@`. Their trap counts are **not zero — they are UNMEASURED**;
the screen has no opinion about a row it never minted. Any lane quoting "IO's
DATA traps" must say **27 across 9 minted rows, 5 rows unmeasured**.

### 4.2 The trap slots named, and the overlap that changes the scoping

27 trap slots across those nine rows resolve to **20 distinct functions** —
**11 named exports** and **9 unexported, address-only**:

| distinct unported function | closes N trap slots | in rows |
|---|---|---|
| `?GetSource@Stream@IO@@UEBA?AVString@System@@XZ` | **4** | Stream, MemoryStream, CompressedStream, CompressedMemoryStream |
| `?Write@CompressedStream@IO@@UEAA_NHPEBX@Z` | 2 | CompressedStream, CompressedMemoryStream |
| `?SetCurrentPos@CompressedStream@IO@@UEAA_NH@Z` | 2 | ″ |
| `?GetCurrentPos@CompressedStream@IO@@UEBAIXZ` | 2 | ″ |
| `?GetSize@CompressedStream@IO@@UEBAIXZ` | 2 | ″ |
| `?Close@CompressedStream@IO@@UEAAXXZ` | 1 | CompressedStream |
| `?PreCommit@CompressedStream@IO@@UEAAXXZ` | 1 | CompressedStream |
| `?PreCommit@CompressedMemoryStream@IO@@UEAAXXZ` | 1 | CompressedMemoryStream |
| `?Serialize@PackageFile@IO@@UEAA_NAEAVStream@2@@Z` | 1 | PackageFile |
| `?Reconstruct@PackageFile@IO@@UEAAPEAVExternal@Object@@XZ` | 1 | PackageFile |
| `?Destruct@PackageFile@IO@@UEAAXXZ` | 1 | PackageFile |
| `0x180023a10` (slot 0) | 1 | BinaryReader |
| `0x18002b710` (slot 0) | 1 | Visitor |
| `0x1800242a0` (slot 0) | 1 | MemoryStream |
| `0x180023fd0` (slot 0) | 1 | CompressedStream |
| `0x1800259d0` (slot 0) | 1 | PackageFile |
| `0x180023bd0` (slot 0) | 1 | ReaderWriter |
| `0x1800a81e0`, `0x1800a81d0`, `0x1800a81c0` (slots 31–33) | 3 here, +3 elsewhere | ReaderWriter |

Two structural facts fall out of naming the slots, and neither is visible in a
TRAP column:

1. **Slot 0 of every IO stream row is the same shape** — unexported, in the
   class's own `0x18002xxxx` neighbourhood: MSVC's deleting destructor, which it
   places first. `??_7Stream@`'s slot 0 (`0x18000c610`) is **already ported**, so
   the pattern has a worked precedent in this tree. Six rows each owe one.
2. **`??_7ReaderWriter@` slots 31–33 (`0x1800a81e0/d0/c0`) are the SAME three
   addresses as slots 0–2 of the unnamed minted row `(unexported @0x1801554a8)`**
   (5 slots, 2 ported, TRAP=3). Porting those three closes **6 trap slots across
   two rows**. `0x1801554a8` sits `0x28` below `??_7BinaryFileStream@`'s
   `0x1801554d0`, which is consistent with a secondary
   (multiple-inheritance) table belonging to the FileStream family — **lane FILE
   should confirm that from the bytes, not from this sentence**
   (`reference_a_written_recipe_can_be_wrong.md`).

### 4.3 Every other row — Wave 20's denominator

Measured in the same run under the same conditions, because the build and run
cost was already paid.

| row | slots | ported | TRAP | purecall | verdict |
|---|---|---|---|---|---|
| `??_7Achievement@0@6B@` | 9 | 3 | **6** | 0 | BLOCKED |
| `??_7AchievementInstance@Achievement@@6B@` | 4 | 1 | **3** | 0 | BLOCKED |
| `??_7AnimationEvent@Graphics@@6B@` | 9 | 2 | **2** | 5 | BLOCKED |
| `??_7Base@Object@@6B@` | 3 | 2 | **0** | 1 | READY |
| `??_7CompiledExternal@Object@@6B@` | 8 | 4 | **0** | 4 | READY |
| `??_7ConstantBufferData@Video@@6B@` | 4 | 1 | **0** | 3 | READY |
| `??_7ContextData@Video@@6B@` | 20 | 1 | **0** | 19 | READY |
| `??_7DeviceData@Video@@6B@` | 20 | 1 | **0** | 19 | READY |
| `??_7Dialog@UI@@6B@` | 9 | 9 | **0** | 0 | READY |
| `??_7DialogController@UI@@6B@` | 11 | 11 | **0** | 0 | READY |
| `??_7DialogInstance@UI@@6B@` | 10 | 10 | **0** | 0 | READY |
| `??_7DialogLoaderLoopT@Engine@@6B@` | 13 | 3 | **9** | 1 | BLOCKED |
| `??_7DialogLoopT@Engine@@6B@` | 13 | 4 | **8** | 1 | BLOCKED |
| `??_7EdgeMesh@Graphics@@6B@` | 9 | 0 | **9** | 0 | BLOCKED |
| `??_7EdgeMeshInstance@Graphics@@6B@` | 9 | 4 | **5** | 0 | BLOCKED |
| `??_7Element@UI@@6B@` | 12 | 11 | **0** | 1 | READY |
| `??_7ElementDescription@UI@@6B@` | 9 | 3 | **3** | 3 | BLOCKED |
| `??_7External@Object@@6B@` | 8 | 4 | **0** | 4 | READY |
| `??_7Geometry@Graphics@@6B@` | 9 | 6 | **0** | 3 | READY |
| `??_7Goal@Achievement@@6B@` | 10 | 4 | **3** | 3 | BLOCKED |
| `??_7GoalInstance@Achievement@@6B@` | 7 | 3 | **4** | 0 | BLOCKED |
| `??_7ImageBuffer@Video@@6B@` | 10 | 10 | **0** | 0 | READY |
| `??_7ImageBufferData@Video@@6B@` | 7 | 1 | **0** | 6 | READY |
| `??_7IndexBufferData@Video@@6B@` | 7 | 1 | **0** | 6 | READY |
| `??_7Internal@Object@@6B@` | 4 | 4 | **0** | 0 | READY |
| `??_7Light@Graphics@@6B@` | 11 | 5 | **1** | 5 | BLOCKED |
| `??_7Loader@Engine@@6B@` | 2 | 1 | **1** | 0 | BLOCKED |
| `??_7LoaderLoop@Engine@@6B@` | 12 | 1 | **11** | 0 | BLOCKED |
| `??_7MaterialInstance@Graphics@@6B@` | 9 | 2 | **7** | 0 | BLOCKED |
| `??_7MeshInstance@Graphics@@6B@` | 9 | 2 | **7** | 0 | BLOCKED |
| `??_7Particle@Graphics@@6B@` | 9 | 2 | **4** | 3 | BLOCKED |
| `??_7ParticleInstance@Graphics@@6B@` | 10 | 5 | **4** | 1 | BLOCKED |
| `??_7ParticleSystemInstance@Graphics@@6B@` | 10 | 5 | **5** | 0 | BLOCKED |
| `??_7PixelProgramData@Video@@6B@` | 3 | 1 | **0** | 2 | READY |
| `??_7RenderBufferData@Video@@6B@` | 4 | 1 | **0** | 3 | READY |
| `??_7RenderStateData@Video@@6B@` | 3 | 1 | **0** | 2 | READY |
| `??_7SpriteSheet@UI@@6B@` | 9 | 9 | **0** | 0 | READY |
| `??_7SunLight@Graphics@@6B@` | 11 | 5 | **6** | 0 | BLOCKED |
| `??_7Thread@System@@6B@` | 2 | 1 | **0** | 1 | READY |
| `??_7ToolTipDialog@UI@@6B@` | 10 | 10 | **0** | 0 | READY |
| `??_7UpdateLoop@Engine@@6B@` | 12 | 1 | **6** | 5 | BLOCKED |
| `??_7VertexBufferData@Video@@6B@` | 7 | 1 | **0** | 6 | READY |
| `??_7VertexLayoutData@Video@@6B@` | 3 | 1 | **0** | 2 | READY |
| `??_7VertexProgramData@Video@@6B@` | 3 | 1 | **0** | 2 | READY |
| `??_7VideoProgramData@Video@@6B@` | 3 | 1 | **0** | 2 | READY |
| `(unexported @0x1801554a8)` | 5 | 2 | **3** | 0 | BLOCKED |
| `(unexported @0x180155860)` | 2 | 1 | **1** | 0 | BLOCKED |
| `(unexported @0x180157c60)` | 7 | 0 | **7** | 0 | BLOCKED |

Whole-screen totals, quoted with their denominators: **57 vtables, 546 slots;
26 of 57 complete; 256 of 398 non-purecall slots carry our own code; 546 of 546
slot comparisons against the live DLL, 0 MISMATCH; 0 slot-count suspects; 0
symbols absent from the DLL; 4 rows carried by address, for which the
export-location cross-check does not apply and was NOT counted as a pass.**

A cross-lane note the Engine rows make obvious: the five `UpdateLoop@Engine@`
methods `?DrawFailed@`, `?SaveRestore@`, `?OnVideoRestart@`,
`?GetResourceFilename@` and `?AllowsModChanges@` are each a trap in **four**
rows (UpdateLoop, LoaderLoop, DialogLoopT, DialogLoaderLoopT). The same overlap
economics as §4.2 apply there — for Wave 20, not this wave.

## 5. WOULD IT MATTER? — attendance, asked and scored separately

The brief's central rule: **a trap-carrying row is UNFINISHED, not
unpromotable.** §4 answers *possible?*. This section answers *matter?*, and the
two must not be merged — CEIL predicted 40 NO-OPs and measured 12 precisely
because it asked *matter?* first. A green readiness row means **safe to stamp**,
not that anything dispatches it
(`reference_a_green_readiness_screen_is_not_a_route.md`).

Measured with `BANISHED_W17_ATTEND=1` on run2, **gameplay route only** (see §7).
`post > 0` proves the consumer ran; `post == 0` proves it did not run *on this
route*; `post == 1` says nothing about frequency, because nearly every consumer
caches in a function-local static.

| IO row | possible? (slots/ported/TRAP) | matter? (gameplay `post`) |
|---|---|---|
| `??_7Stream@IO@@6B@` | 7 / 1 / **1** | ATTENDED `post=3` |
| `??_7CompressedStream@IO@@6B@` | 9 / 1 / **8** | ATTENDED `post=2` |
| `??_7CompressedMemoryStream@IO@@6B@` | 9 / 3 / **6** | ATTENDED `post=2` |
| `??_7MemoryStream@IO@@6B@` | 8 / 6 / **2** | ATTENDED `post=1` |
| `??_7PackageFile@IO@@6B@` | 9 / 5 / **4** | ATTENDED `post=1` |
| `??_7BinaryReader@IO@@6B@` | 26 / 25 / **1** | never (`post=0`) |
| `??_7BinaryWriter@IO@@6B@` | 26 / 26 / **0** READY | never (`post=0`) |
| `??_7ReaderWriter@IO@@6B@` | 36 / 2 / **4** | never (`post=0`) |
| `??_7Visitor@IO@@6B@` | 26 / 25 / **1** | **not in the instrument's 62-row subject — UNMEASURED** |
| `??_7CompressedFileStream@IO@@6B@` | **unminted — UNMEASURED** | ATTENDED `post=2` |
| `??_7VisitorStream@IO@@6B@` | **unminted — UNMEASURED** | ATTENDED `post=1` |
| `??_7BinaryFileStream@IO@@6B@` | **unminted — UNMEASURED** | never (`post=0`) |
| `??_7FileStream@IO@@6B@` | **unminted — UNMEASURED** | never (`post=0`) |
| `??_7TextFileStream@IO@@6B@` | **unminted — UNMEASURED** | never (`post=0`) |
| `?_typeInfo@PackageFile@IO@@` | (typeinfo, READY, `idcmp=yes`) | never (`post=0`) |

Whole-instrument denominator: **35 of 62 subject rows attended on this route,
27 never**, control GREEN. `??_7BinaryWriter@`'s `post=0` is corroborated by a
second, independent instrument in the same run — `[Encoding] ATTENDANCE: none of
the four bodies was entered in this run` — in both run1 and run2.

**Two rows are complete-and-unattended and two are attended-and-unmeasurable.**
`??_7BinaryWriter@` is *possible today* and *would not matter on any route this
harness drives*; `??_7CompressedFileStream@` and `??_7VisitorStream@` are the
mirror image — demonstrably live, and the screen cannot say whether promoting
them is possible because they were never minted. **"No route driven" is not "no
route exists"** (Wave 18 refuted that four times), and "unminted" is not "zero
traps".

## 6. Revised cheapest-rows list for the four porting lanes

Ordered by **distinct functions a lane must write**, not by TRAP column, because
§4.2 shows those differ by nearly a third.

1. **`?GetSource@Stream@IO@@UEBA?AVString@System@@XZ` — one named export,
   four rows.** It is the *entire* remaining trap in `??_7Stream@` (7/1/1) and
   in `??_7MemoryStream@` it leaves only slot 0. Cheapest single action in IO's
   whole DATA set, and it is **attended** (`??_7Stream@` `post=3`), so it is on
   a live route. **Lane IOBASE, first.**
2. **`??_7BinaryReader@` and `??_7Visitor@` — one trap each, both slot 0, both
   the deleting-destructor shape**, with `??_7Stream@`'s already-ported slot 0
   as the in-tree precedent. `??_7Visitor@` is a **bonus row not in the plan's
   36** — it costs a lane nothing extra once BinaryReader's slot 0 is understood.
3. **The six IO slot-0 destructors as one pattern**, not six investigations:
   `0x180023a10` (BinaryReader), `0x18002b710` (Visitor), `0x1800242a0`
   (MemoryStream), `0x180023fd0` (CompressedStream), `0x1800259d0` (PackageFile),
   `0x180023bd0` (ReaderWriter).
4. **`0x1800a81e0/d0/c0` — three functions, six trap slots, two rows**
   (`??_7ReaderWriter@` slots 31–33 and `(unexported @0x1801554a8)` slots 0–2).
   Best trap-slots-per-function ratio in the table. **Confirm the shared identity
   from the bytes before scoping on it.**
5. **`??_7CompressedStream@` + `??_7CompressedMemoryStream@` are ONE lane's
   work, not two rows' worth**: 14 trap slots, **8 distinct functions**, six of
   them shared. Both are ATTENDED. **Lane CSTREAM should scope 8, not 14.**
6. **`??_7BinaryWriter@` needs no porting at all** — it is 26/26 TRAP=0. Its
   only open question is attendance, and that is a *route* problem for lane
   IOBASE, not a port problem.
7. **`??_7PackageFile@`** — 4 traps, 4 distinct functions, no overlap with
   anything else. Straight port work, ATTENDED.

**Lane FILE gets no help from this cell**, and should know that going in: all
five of its vtable rows (`??_7FileStream@`, `??_7BinaryFileStream@`,
`??_7TextFileStream@`, plus `??_7CompressedFileStream@` and `??_7VisitorStream@`
in other lanes) are **unminted, so their trap counts are UNMEASURED**. Phase 0b
(fix `tools/rt_mint_gen.py`'s harvest) is a **prerequisite for scoping lane
FILE's DATA rows**, not an optional cleanup — and this cell reproduced that gap
from an instrument that has nothing to do with §0b's grep.

## 7. What this does NOT claim

* **It does not re-verify §0a.** `??_7ReaderWriter@` at 30-of-36 purecall and
  `??_7Stream@` at 5-of-7 are confirmed *as counts*; the abstract-base NO-OP
  question still rests on `encoding.cpp`'s comment and still needs the two
  Encoding constructors read as disassembly. Nothing here settles it.
* **Attendance was measured on ONE route** (gameplay), not CEIL's four. The save,
  load and mods routes were **not** run. Any `post=0` here means "not on the
  gameplay route", strictly weaker than CEIL's "not on four routes". Rows CEIL
  found attended only on save/load (it names `??_7MaterialInstance@Graphics@@6B@`)
  will read `post=0` here for that reason and not because anything regressed.
* **The mods route was not attempted at all.** CLAUDE.md and CEIL §6 both record
  it as crashing on `3da1503`; this cell did not re-test that and makes no claim
  either way.
* **Five IO vtables and `??_7Visitor@`'s attendance are UNMEASURED, not zero.**
  §4.1 and §5 mark each one. An unmeasured cell is not a green cell.
* **`(unexported @0x1801554a8)`'s identity is a hypothesis**, from address
  adjacency alone. It is offered to lane FILE as something to check, not as a
  finding.
* **These counts hold under default flags only.** C1 changes one row by design;
  a lane running kill switches will measure something else, correctly.
* **`storage` and `constructed statics` rows were not audited.** The screen
  reports 39 storage symbols and `0 of 3` constructed statics built
  (`BANISHED_RT_MINT_CTOR=1` was not set). CEIL's HARMFUL triage of the storage
  rows is neither confirmed nor challenged here.
* **No claim about correctness of any ported slot.** READY says "no abort-trap
  in the table"; the correctness evidence is the `546 of 546 comparisons, 0
  MISMATCH` ground-truth line, which compares *addresses* against the DLL's
  table, not behaviour.

## 8. Effect on the identity

The Wave 19 success criterion is
`ported >= called - unpromotable` (`83 >= 122 - 3 = 119`, **36 short**).

**This cell moves nothing in it.** Nothing was ported (`ported` unchanged),
nothing was registered or resolved (`called` unchanged), and no row was tagged
NO-OP or HARMFUL (`unpromotable` unchanged). The IO identity stands exactly as
the plan states it.

**No percentage is quoted anywhere in this document, and none should be derived
from it.** `called` sits in both halves of the ceiling fraction
(`reference_the_ceiling_percentage_is_a_moving_target.md`), so a lane opening a
door moves the ceiling without changing anything real.

The one figure elsewhere that this cell **does** move is Wave 18's Video
accounting: `??_7ImageBuffer@Video@@6B@` is no longer a 7-trap BLOCKED row. That
is a row leaving the *Video* worklist, already reflected in Wave 18's close, and
it is **not** an IO change.
