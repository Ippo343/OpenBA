# WAVE 26 LANE DOORS — making the board honest

Branch `wave26/doors`, cut from `wave26/base` at `7de42dd1`.
**This lane ported nothing.** It changed one file — `tools/runtime_census.py` —
plus this document. Every number every other lane in this wave quotes comes out
of that file, so read §1 before quoting a Wave 26 figure.

---

## 1. THE BOARD, AS A SET DIFF

```
BEFORE (wave26/base 7de42dd1)   TOTAL  1333 called / 1185 ported / 88.9%   gap 148
AFTER  (this branch)            TOTAL  1351 called / 1204 ported / 89.1%   gap 147
```

**DO NOT READ +19 PORTED AS PORTING WORK. This lane wrote no C++.** Eleven of
those nineteen were registered unconditionally in the process before this lane
existed and the census could not see them; eight were already-registered ports
that had no visible caller. Re-derive with:

```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py
tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py --selftest-all
```

The diff is given as a SET because a count that moves the right direction by the
wrong amount reads as success — this project lost a wave to 129→127 where 126
was expected. Both CSVs are reproducible: `--csv` on this tip, and `git show
wave26/base:tools/runtime_census.py` run with `--csv` for the baseline.

### 1a. 18 rows ENTERED `called` (0 left it)

| row | ns | now ported? | which mechanism |
|---|---|---|---|
| `??_7MeshInstance@Graphics@@6B@` | Graphics | no | `RtVtable` wrapper door |
| `??_7EdgeMeshInstance@Graphics@@6B@` | Graphics | no | `RtVtable` wrapper door |
| `??_7EdgeMesh@Graphics@@6B@` | Graphics | no | `RtVtable` wrapper door |
| `??_7ElementDescription@UI@@6B@` | UI | no | cross-file name constant |
| `?_typeInfo@GroupElement@UI@@0VBaseTypeInfo@Object@@A` | UI | no | `TypeIdOfStatic` wrapper door |
| `??1CompressedFileStream@IO@@UEAA@XZ` | IO | no | `RtResolveExportForProbe` wrapper door |
| `??1TextFileStream@IO@@UEAA@XZ` | IO | no | `RtResolveExportForProbe` |
| `??1VisitorStream@IO@@UEAA@XZ` | IO | **yes** | `RtResolveExportForProbe` |
| `?GetObjectName@TextFileStream@IO@@…` | IO | no | `RtResolveExportForProbe` |
| `?GetObjectType@TextFileStream@IO@@…` | IO | no | `RtResolveExportForProbe` |
| `?GetObjectVersion@TextFileStream@IO@@QEBAHXZ` | IO | no | `RtResolveExportForProbe` |
| `?Get@Application@1@SAPEAV11@XZ` | Application | **yes** | `AppPair` wrapper door |
| `?GetWindow@Application@1@…` | Application | **yes** | `AppPair` |
| `?EnableCursor@Application@1@QEAAX_N@Z` | Application | **yes** | `AppPair` |
| `?IsCursorEnabled@Application@1@QEAA_NXZ` | Application | **yes** | `AppPair` |
| `?PlatformIsPaused@Application@1@AEAA_NXZ` | Application | **yes** | `AppPair` |
| `?PlatformDestruct@Application@1@AEAAXXZ` | Application | **yes** | `AppPair` |
| `?LaunchProcess@Application@1@QEAA_NPEBD_N@Z` | Application | **yes** | `AppPair` |

The three Graphics rows are **exactly** the three Wave 21 lane GFXVT named and
the census has printed as its own known defect for five waves. They are now
visible, and they are UNPORTED — so Graphics `called` went 108 → 111 and its
ported% went **down**, 83.3% → 81.1%. That is the ceiling-percentage trap this
project has already documented: `called` sits in both halves, so opening a door
can only move the percentage the unflattering way. Score the identity, not the
percentage.

### 1b. 11 rows FLIPPED `called` → `ported` (0 flipped the other way)

```
??_7BinaryFileStream@IO@@6B@   ??_7BinaryReader@IO@@6B@   ??_7BinaryWriter@IO@@6B@
??_7MemoryStream@IO@@6B@       ??_7Visitor@IO@@6B@        ??_7VisitorStream@IO@@6B@
??_7DialogInstance@UI@@6B@     ??_7Element@UI@@6B@        ??_7SpriteSheet@UI@@6B@
??_7ToolTipDialog@UI@@6B@      ??_7ImageBuffer@Video@@6B@
```

### 1c. Gap decomposition, before → after

```
BEFORE  gap 148 |  BODY 15  DTOR 0  VTABLE 54  DATA 46  UNKNOWN 33
AFTER   gap 147 |  BODY 18  DTOR 2  VTABLE 47  DATA 47  UNKNOWN 33
```

Fully reconciled: VTABLE −11 (the promotions) +4 (3 Graphics + ElementDescription)
= 47; DATA +1 (GroupElement); BODY +3 and DTOR +2 (the TextFileStream /
CompressedFileStream probe rows). No row moved that this table does not name.

---

## 2. THE FOUR ADMISSIONS, EACH CLOSED

### 2.1 The wrapper-door screen's 70 unclassified sites — ADJUDICATED

Wave 21 built the screen and deliberately refused to judge, because judging
mid-wave moves numbers other lanes quote. All 15 identifiers were judged here
**by reading the wrapper's body**, file:line recorded in the census itself
(`WRAPPER_LOCAL_DOORS` / `WRAPPER_LOCAL_NOTDOORS`), and the screen now prints
each verdict with its reason on every run. **UNCLASSIFIED is 0.**

**THE RULE APPLIED IS MECHANISM, NOT PURPOSE**, because that is the rule the
script already uses: `RE_EXPORT` matches `ResolveExport(?:Raw)?` and counts a
literal site wherever it stands — in a port, an oracle, or a probe file. A
wrapper is a DOOR iff its body reaches Runtime-x64.dll **by name** with the
caller's string.

DOOR (8): `RtVtable`, `RT_VTABLE_ONCE`, `Dx11ResolveRuntimeImport`, `RawVtable`,
`TypeIdOfStatic`, `RtResolveExportForProbe`, `AppPair`, `VerifyBody`.

NOT a door (7): `PromoteVtableGuarded` (registration side), `RtPortLookupExport`
and `RuntimeExportIsRegistered` (registry queries that deliberately do not fall
through to the DLL), `RtMintVtableTrapCount` (reads our own mint table),
`RtInternalVtblPinRow` and `CallVideoCallback` (the literal is a printf label;
the real door is the adjacent argument, already counted), `VerifyFailed`
(printf only).

`Dx11ResolveRuntimeImport` is a door that **adds no row** — `RE_DX11_MANGLED`
already reached all nine sites. Recorded anyway: a classification that moves
nothing is still a classification, and the next lane should not re-derive it.

### 2.2 The ONE invisible registration — it was worth ELEVEN ROWS

The warning said "every number above UNDERSTATES". It was right, and by more
than its own "1 call site" suggested. `reg_mint_promoted.cpp:214` is
`RegisterRuntimeExport(symbol, st)` inside `PromoteVtableGuarded`, and
**measured on `wave26/base`, all eleven guarded promotions printed under
`--unported`** while `RtMintRegisterPromotedRows()` registers every one of them
unconditionally. That is the Wave 4 lane D defect (IO reported 30/41 when 36
were registered) alive again, in the file whose job is promoting rows.

**FIXED BY TEACHING THE CENSUS, not by respelling the call site**, and saying so
as the brief asked. The literal already *is* at the call site —
`PromoteVtableGuarded("??_7…", va, &n)` — and `mint_promoted_symbols()` already
parses both promotion doors and is already controlled by
`--selftest-promoted-doors`. Pushing a second copy of the name down into the
helper would duplicate it and let the two drift.

**Gating was checked, not assumed.** These sit in the same function, under the
same DEFAULT-ON kill switch (`BANISHED_RT_MINT_PROMOTED`), as the twelve rows
whose literal `RegisterRuntimeExport` the script has always counted. Counting
one group and not the other was never a gating judgement — it was a regex.
`??_7BinaryFileStream@IO@@6B@` additionally has its own default-on per-row
switch (`BANISHED_RT_PROMOTE_BFS`), the same shape.

### 2.3 The 15 unfollowable `ResolveExport` identifiers — 14 resolved, 1 left, 2 recognised as fakes

Three distinct causes, and the two deliberate fakes are **recognised and
excluded, never silently folded**:

| identifier(s) | cause | disposition |
|---|---|---|
| `RtUI::kVtblBase`, `kVtblExternal`, `kVtblElementDescription` | constant defined in `runtime/types/ui/ElementDescription.h`, used in `runtime/src/ui/dialog.cpp`; the lookup was SAME-FILE only | cross-file table added → 3 real doors; 1 new row |
| `kGetValue`, `kPressed`, `kReleased` | a THIRD spelling, `const char* kX = "…";` — two of three spellings matched, one did not | `RE_NAME_CONST` extended; all 3 symbols verified PRESENT in the DLL (pefile, individually) |
| `kFake` (`standalone/rt_selftest.cpp`) | deliberate selftest fixture, `?RtSelfTestOnly@@YAXXZ` | **EXCLUDED** via `DELIBERATE_FAKES`, printed every run |
| `phantom` (`runtime/src/engine/loops.cpp`) | VTIO's S1 negative control, assembled by `snprintf` | **EXCLUDED** via `DELIBERATE_FAKES`, printed every run |
| `row0` (`standalone/tinfo_door_screen.cpp`) | read out of the mint table at RUN TIME | **still unfollowable**, still warned, with the reason stated |

A defect was found *inside this fix* and is recorded because it is the kind that
survives review: the parameter-name exemption (`mangled`/`name`/`sym`/…) sat in
the `elif`, which is harmless for a same-file table and **immediately wrong** for
a cross-file one — `AppPair` forwards a parameter called `name` to
`ResolveExport`, another file defines `const char* name = …`, and the global
table bound them. It fired twice on `standalone/rt_orphan_probe.cpp` before the
exemption was moved to the front. The "picks the last writer" failure the
cross-file table refuses by design arrived through the back door.

### 2.4 The phantom-literal sweep — LANE SWEEP DID DO IT; I re-ran and extended it

**Checked `docs/runtime_port/WAVE25_LANE_SWEEP.md` first, as instructed: §4 item 9
says it ran.** `tools/phantom_symbol_scan.py` exists and is alive. Re-run on this
tip: 1,545 files, 1,701 distinct mangled literals against 4,926 DLL exports, both
controls green, 38 concatenation prefixes discharged and counted, **3 absent
literals, 0 of them census-visible**.

SWEEP's stated caveat — *"a phantom passed through a named constant is invisible
to BOTH the census and to this scan"* — is the one this lane had to close,
because following named constants is exactly what §2.3 added. Closed by a
**PHANTOM GUARD**: a name reached through a followed constant is checked against
the DLL's export set, and if absent it is **not counted** and is printed loudly.

Each name was checked **individually** with `pefile` against
`C:\GOG Games\Banished\Runtime-x64.dll` — not one check written up as covering a
population:

| symbol | verdict | where |
|---|---|---|
| `?RtSelfTestOnly@@YAXXZ` | **ABSENT** | `standalone/rt_selftest.cpp` `kFake` — deliberate |
| `?NoSuchSlot@UpdateLoop@Engine@@UEAAXXZ` | **ABSENT** | `runtime/src/engine/loops.cpp` — deliberate (VTIO) |
| `?OnElementEvent@DialogInstance@UI@@UEAAXAEBVElementEvent@2@@Z` | **ABSENT** | `code/Game/Player/tool_base_toolui.cpp:79` — see below |
| `?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z` | PRESENT | its NEIGHBOUR at line 78 — this is what proves the absence above is per-symbol and not per-file |

**`kSym_OnElementEvent` is a live landmine, not a live defect.** The DLL really
does not export it (the source comment at `tool_base_toolui.cpp:136` says so and
is correct), and the constant is **currently unused** — as is `kSym_OnUpdate` and
the rest of that block. So it mints no row today. The moment anyone wires that
constant to a resolve call it would mint a gap row for a symbol that cannot be
ported, and the phantom guard is what will stop it.

---

## 3. WHAT I CHANGED

`tools/runtime_census.py` (all of the below), plus `nul_byte_screen()` in
`tools/phantom_symbol_scan.py` (§8.2) and this document. **No C++ file was
touched.**

In `tools/runtime_census.py`:

* `WRAPPER_LOCAL_DOORS` / `WRAPPER_LOCAL_NOTDOORS` — the 15 verdicts, each with
  file:line and the reason, printed on every run. `RE_WRAPPER_DOOR` is **built
  from** the door table, so a wrapper cannot be classified in one place and
  matched in another.
* `global_name_consts()` — cross-file constant table; **refuses to choose** when
  two files define one identifier differently (value `None`, site stays in the
  warning).
* `RE_NAME_CONST` — the third spelling.
* `DELIBERATE_FAKES` + the phantom guard + `load_export_names()`.
* The guarded-promotion fold at the end of `scan()`, with its inherited caveat
  and the reconciliation of the two proposed fix sites written at the fix (§8.1).
* The `VTABLE GAP, DECOMPOSED` printed block (§8.1c).
* `INSTRUMENT_FILE_MARKERS` — see §4. Changes no count.
* Four new selftests and `--selftest-all`.

No export, address or ordinal was registered. No `RT_UNPROMOTABLE` tag was
added, removed or re-adjudicated.

---

## 4. THE CONSEQUENCE I AM NOT HIDING: 6 instrument rows entered the gap

The mechanism rule puts three probe-file wrappers in the door set, so **13 rows
whose only caller is an instrument** entered `called`; 6 of them are unported and
therefore **entered the gap**, which is where porting lanes are pointed.

```
GAP(instrument) IO   ??1CompressedFileStream@IO@@UEAA@XZ
GAP(instrument) IO   ??1TextFileStream@IO@@UEAA@XZ
GAP(instrument) IO   ?GetObjectName@TextFileStream@IO@@QEBAAEBVString@System@@XZ
GAP(instrument) IO   ?GetObjectType@TextFileStream@IO@@QEBAAEBVString@System@@XZ
GAP(instrument) IO   ?GetObjectVersion@TextFileStream@IO@@QEBAHXZ
GAP(instrument) System ?Get@MemoryAllocator@System@@SAPEAV12@XZ      (pre-existing)
GAP(instrument) internal 0x180010f00                                  (pre-existing)
```

A probe's resolve **is** a real dependency of our tree (which is why it counts)
and **is not** a port obligation (it is env-gated and dies with the DLL). Both
are true, so the census now prints the population under
`INSTRUMENT-SOURCED` on every run rather than leaving a future lane to discover
it by spending a day on `?GetObjectVersion@TextFileStream@IO@@`.

---

## 5. VERIFICATION — every control RUN, not asserted

`tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py --selftest-all`
→ **ALL GREEN**, captured below. Four new screens in the existing
`--selftest-*` shape, plus every pre-existing one re-run.

| screen | result | what its control corrupts |
|---|---|---|
| `--selftest-unpromotable-key` (pre-existing) | 7/7 | — |
| `--selftest-promoted-doors` (pre-existing) | 4/4 | — |
| `--selftest-classifier` (pre-existing) | all | — |
| `--selftest-ordinal-fold` (pre-existing) | pass | — |
| `--selftest-diff-sites` (pre-existing) | ALIVE | — |
| `--selftest-wrapper-doors` | **21/21** | a not-door handed to the door regex; a name that merely *contains* a door name |
| `--selftest-name-consts` | **11/11** | a `const int*`; two conflicting definitions of one identifier |
| `--selftest-phantom-guard` | **10/10** | a name really absent from the export set — the exact value the guard compares |
| `--selftest-guarded-promotions` | **16/16** | one symbol removed from the promoted SET — the single input to the fold; plus the MaterialInstance literal-door fixture (§8.1) |

Doctrine applied, explicitly:

* **An absence screen was HANDED a shape.** The phantom guard reports 0 on a
  clean tree, which is indistinguishable from a dead lookup, so it is handed a
  fabricated name (must be caught) *and* a name read out of the export table
  (must not be). Same for `RE_WRAPPER_DOOR`: every door in the table gets a
  synthetic positive case and every not-door a synthetic negative, generated
  **from the tables**, so a wrapper added without a verdict cannot slip past.
* **Controls corrupt what the check compares.** The promotion control removes a
  symbol from the promoted set (the fold's only input), not a slot or an address
  that the fold never reads. Corrupting the latter would report NOT CAUGHT and
  indict a sound instrument.
* **Effectiveness counters everywhere.** "43 symbols through these wrappers",
  "264 name constants", "4,926 export names", "the fold moved 11 rows". A green
  screen that was handed nothing is not a measurement.
* **No union scoring.** `_st_report` prints and counts every check separately; a
  3/3 on the union of two screens hides one screen catching 1/3.
* **Two checks sharing a premise are one check.** The three RtVtable rows are
  asserted through `wrapper_door_hits` (the scan) *and* the row set is verified
  independently through the `--csv` set diff (§1a), which reaches them by a
  different path.

---

## 6. RESIDUALS — with addressees, because a condition with no addressee never fires

1. **The mechanism-vs-purpose question is OPEN and is not mine to close
   silently.** Should a resolve that exists only to feed a probe count in
   `called` at all? Today it does — for wrapper sites (this lane) and for the
   plain literal sites the script has always counted. Changing that removes
   EXISTING rows, which is a bigger judgement than a mid-wave instrument fix.
   **ADDRESSEE: the Wave 27 kickoff author.** Decide it there, in the open, or
   state that the current rule stands. The population is printed every run under
   `INSTRUMENT-SOURCED`, so the cost of the decision is visible without a lane.
2. **`code/Game/Player/tool_base_toolui.cpp:72-80` holds ten unused
   `kSym_*` constants, one of which (`kSym_OnElementEvent`) names a symbol the
   DLL does not export.** ADDRESSEE: **any lane that wires one of those constants
   to a resolve call.** The phantom guard will refuse it and print the reason;
   delete the constant rather than "fixing" the name.
3. **`standalone/tinfo_door_screen.cpp` `row0` is still unfollowable** and is
   still in the warning. It reads the mint table at run time, so no respelling
   helps. ADDRESSEE: **any lane that edits `tinfo_door_screen.cpp`** — if you
   give it a literal, the warning goes to zero.
4. **MERGE CONFLICT NOTICE.** Three lanes edited `runtime_census.py` in Wave 25
   and it produced the wave's only conflict. I am this wave's designated owner of
   it. **If another Wave 26 lane's changes conflict with mine at merge, KEEP
   BOTH** — my changes are concentrated in five named regions (`WRAPPER_LOCAL_*`
   and `RE_WRAPPER_DOOR` near the wrapper screen; `RE_NAME_CONST`;
   `load_export_names()`; the `RE_EXPORT_NONLITERAL` branch and the promoted fold
   inside `scan()`; the four selftests and the arg wiring) and none of them
   removes existing logic. After any merge, run `--selftest-all`: if a lane's
   edit silently drops one of my regions, the corresponding screen goes RED
   rather than quietly reverting a count.

---

## 7. WHAT THIS LANE DOES **NOT** CLAIM

* It does **not** claim the port advanced. Zero functions were reimplemented.
  1185 → 1204 is instrument correction in both directions, itemised in §1.
* It does **not** claim the 18 newly-visible rows are new work. Twelve were
  already registered; the six that are genuinely unported are named in §1a, and
  five of those six are instrument-sourced (§4).
* It does **not** claim the wrapper screen is now complete for all time. It
  claims UNCLASSIFIED is 0 **today**; the screen still prints any new identifier
  by name, which is the property that made this lane possible.
* It does **not** claim a phantom-free tree. It claims 3 absent literals exist,
  each verified individually, **0 of them census-visible**, and that the guard
  which keeps it that way has been handed a positive case.

---

## 8. TWO ITEMS ROUTED IN BY THE COORDINATOR, MID-LANE

### 8.1 "15-ish promoted vtables counted as unported" — ALREADY LANDED (§2.2), reconciled

Same defect as §2.2, already fixed on this branch before the routing arrived.
What the routing added, and what I did with it:

* **Verified from the BINARY end, not by re-reading the lane reports.** The two
  finding lanes are not fully independent — both read the same census source, and
  this project has a case where distrusting a grep and re-running it *by symbol*
  reproduced the same wrong answer because both asked the same question. The
  independent confirmation used here: `??_7BinaryFileStream@IO@@6B@` is the row
  Wave 20 closed on, attendance 0 → 18,906, so it demonstrably dispatches; and
  `runtime_census.py --unported` on `wave26/base` demonstrably printed it. Those
  two cannot both be right, and only the census can be wrong.
* **The counts differ; the SET does not.** PROMOGFX said 13-of-14, PROMOIO
  15-of-16, I measure **11** on `wave26/base` — each measured after its own
  promotions landed in its own branch, so all three describe different trees.
  §1b names every row. The count will keep moving as lanes merge; the set is the
  invariant.
* **THE CONTROL WAS ALREADY IN THE DATA, and it is now a regression fixture.**
  `??_7MaterialInstance@Graphics@@6B@` is the one promoted vtable Wave 25 lane
  VTGFX hand-spelled as a literal instead of routing through the guard, and so
  the one promoted row that was NOT in the gap. Measured: `ported=1` in **both**
  `before.csv` and `after.csv` — seen vs blind differ by exactly the literal.
  `--selftest-guarded-promotions` now asserts it reads PORTED **and** that it is
  *not* one of the rows the fold moved: if it ever becomes one, the literal door
  has died and the fold would be masking it.
* **The two locations were NOT the same place, and that mattered more than the
  line number.** PROMOGFX pointed at `promoted_vt = {...}` inside the vtable
  fold-in; PROMOIO at "the fold, before the entity scan". The vtable fold-in only
  ever adds SLOT rows, so a fix there would have to special-case the row itself —
  and three other consumers (the entity scan, the orphan screen, and the fold's
  own `registered` test) read `ported_exports` directly and would each still
  disagree. I folded at the **end of `scan()`**, upstream of all of them.
  Measured consequence: orphan exports **155 → 147**, i.e. rows that were visible
  to one screen and not another became consistently visible. A fix at either
  named location would have left that inconsistency standing.
* **(b) The inherited caveat, written where the fix lives.** Both the fold in
  `scan()` and the new VTABLE GAP block carry it, and the census prints it every
  run: `PROMOTED` counts a row registered because `RtMintRegisterPromotedRows()`
  registers it with no env var set. Under `BANISHED_RT_MINT_PROMOTED=0` it
  registers **none** of them, and for that run this column — and the slot fold-in
  above it, which has always had the same caveat — **OVERSTATE**. The figure is
  STATIC: what the source registers, not what a given run did.
* **(c) The downstream correction is a PRINTED OUTPUT, not a sentence.** New
  block `VTABLE GAP, DECOMPOSED`, per namespace, from three sources (the
  `RT_UNPROMOTABLE` tags, the promoted-symbol set, the mint tables) and never
  from the name:

```
  namespace     vt gap | UNPROMOTABLE  PROMOTED  MINTED NOT MINTED
  Video            12 |           12         0       0          0
  Graphics         10 |            0         0      10          0
  IO                8 |            2         0       6          0
  Achievement       4 |            0         0       4          0
  Engine            4 |            0         0       4          0
  Object            4 |            4         0       0          0
  UI                3 |            2         0       1          0
  Audio             1 |            0         0       1          0
  System            1 |            0         0       1          0
  TOTAL            47 |           20         0      27          0
  REAL remaining vtable work = MINTED + NOT MINTED = 27, NOT 47.
```

  **Video's entire 12-row VTABLE gap is unpromotable — zero work.** UI's is 3, of
  which 1 is real. A lane scoped on "Video has 12 vtable rows" is scoped against
  nothing at all. The `PROMOTED` column is **0 everywhere**, and that is the
  regression detector for §2.2: a non-zero there means the fold has broken, and
  the block says exactly that in its own output.

### 8.2 A raw NUL byte makes `git grep` silently skip a file — CONFIRMED, POPULATION IS ONE, DETECTOR ADDED

**Measured independently on `wave26/doors`, not taken from the report:**

```
git grep -h  -E "RegisterRuntime(Export|Addr)\(" HEAD -- runtime standalone code engine dx11   2457 lines
git grep -ah -E  (otherwise identical)                                                         2495 lines
```

**38 lines suppressed**, from one file. A byte-level scan of 1,722
text-extension files under `runtime/ code/ standalone/ engine/ dx11/ tools/`
finds **exactly one** raw NUL in the whole tree:
`runtime/src/graphics/scene.cpp`, offset 7277 — the same one PROMOGFX found.
There is no second instance.

* **My sweep was NOT exposed** — stated as a measurement, not a reassurance.
  `tools/phantom_symbol_scan.py` walks with `os.walk` + `open(..., errors=
  'replace')`, never consults `git grep`, and reports 1,545 files with scene.cpp
  among them.
* **The exposed population in `tools/` is ONE tool.** I audited every script
  there: `tools/mint_slot_sweep.py:55` holds the only `git grep` invocation in
  the directory, and it is the one PROMOGFX already fixed on its branch. There is
  no second unfixed victim to hunt.
* **I did NOT duplicate the one-byte content fix**, to avoid conflicting with the
  branch where it already lives. **ADDRESSEE: the integrator.** After merging
  PROMOGFX, run `tools/phantom_symbol_scan.py`; its new NUL-BYTE SCREEN must
  print `none`. If it still names scene.cpp, the fix did not land and
  `mint_slot_sweep.py` is inflating again.
* **A permanent detector now exists**, `nul_byte_screen()` in
  `tools/phantom_symbol_scan.py` — the tool this lane owns, and itself a
  tree-wide scan. It is **handed a shape** (a synthetic buffer with a NUL and one
  without, both verdicts printed) precisely because a clean tree yields zero and
  zero is indistinguishable from a dead loop. That is the near-miss the routing
  flagged: PROMOGFX's first control compared `git grep -l` with `git grep -l -a`
  and saw no difference, because `-l` prints names only and never suppresses. A
  control that cannot observe the suppressed channel exonerates a broken
  instrument — the same family as "a control must corrupt something the check
  actually compares". This screen's control observes the byte that does the
  suppressing.
