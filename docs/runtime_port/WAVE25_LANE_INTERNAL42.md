# WAVE 25 LANE INTERNAL42 — the 42 address-keyed rows and the 4 ordinal rows

Branch `wave25/internal42`, cut from `wave25/base` at `fc56353c`. Private worktree.

> **EVERY FIGURE HERE IS BRANCH-LOCAL**, measured in this worktree with the
> command printed beside it. The integrator must re-derive on the merged tip
> (kickoff §4). Wave 21 published a wave number assembled from lane figures and
> it was wrong in both directions.

---

## 0. HEADLINE

**All four `ordinal` gap rows were a REPORTING DEFECT, and so were two rows
nobody had counted as defective. The census is fixed, and the fix is worth more
than any port this lane could have written.**

**And then ten of the forty-two address-keyed rows turned out to be
registrations against machinery this tree already owns, so they were ported
too — at ZERO cutover-surface growth.**

| | at lane start | after the census fix | at lane end |
|---|---:|---:|---:|
| `called` | 1323 | **1318** | **1318** |
| `ported` | 1171 | 1170 | **1180** |
| ported % | 88.5% | 88.8% | **89.5%** |
| gap rows | 152 | 148 | **138** |
| `internal` gap | 42 | 42 | **32** |
| `ordinal` gap | 4 | **0 (bucket gone)** | 0 |

```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py --unported
```

**Read the middle column and the right column as two different kinds of
movement.** The middle column is *the same tree measured correctly*: nothing was
ported between it and the left column. The right column is ten real
registrations. Quoting `152 -> 138` as "fourteen rows of work" would be wrong by
four.

**`called` did not move.** Declared per kickoff §1f: this lane added **zero**
new address doors. Every body it registered is reached through a door that
already existed, and every callee it needs is an internal core this tree already
compiles. Wave 24 INTERNAL8 grew the surface by 2 and said so; this lane grew it
by 0 and the census confirms it rather than the author asserting it.

---

## 1. THE FOUR ORDINAL ROWS — SETTLED: ARTEFACT, NOT PORT WORK

The brief called these "a four-row positive control handed to you". They were,
and the control was **stronger than the hypothesis it was handed to test.**

### 1.1 What the four are

`load_export_ordinals()` reads the shipped export directory. All 4,926 exports
are named — there is not one NONAME export in this DLL — so every ordinal this
tree uses resolves:

| ordinal | resolves to | census verdict at lane start | truth |
|---:|---|---|---|
| 443 | `??0Thread@System@@QEAA@I@Z` | `ordinal` gap row | **PHANTOM** — an `export` row with `ported=1` already existed |
| 3214 | `?Lock@Device@Video@@QEAAXXZ` | `ordinal` gap row | **PHANTOM** |
| 4462 | `?Unlock@Device@Video@@QEAAXXZ` | `ordinal` gap row | **PHANTOM** |
| 2990 | `?GetTypeIdForFileExtension@Factory@Object@@QEBAIPEBD@Z` | `ordinal` gap row | **REAL function, ALREADY PORTED** (Wave 24 INTERNAL8), misfiled out of `Object` |

### 1.2 THE PREDICTION SAID THREE. IT IS FIVE.

**The corrected figure is FIVE phantoms, not three.** The `3` is carried by, and
should be read as superseded in, **two documents by name**:

* `docs/runtime_port/WAVE23_LANE_INTERNAL.md` §4A and residual #1 — *"removes 3
  phantom rows and refiles 1 into Object"*;
* `docs/runtime_port/WAVE24_LANE_INTERNAL8.md` §5 and residual #2 — *"It removes
  3 phantoms and refiles 1 into Object"*, restated with *"the evidence that it
  is the right fix is now four rows, not one."*

Both were **directionally right and quantitatively short**, which is the shape
that gets quoted as settled: each author enumerated the rows in the GAP list and
each found three, because the other two were in the `ported` column. **A wave
that inherits "3" and sees the board move by 5 will go looking for an error that
is not there.** It is 5.

The two nobody named were
the ordinal rows the census was already printing as *ported*:

| ordinal | resolves to | why it was invisible |
|---:|---|---|
| 2991 | `?GetTypeIdFromName@Factory@Object@@QEBAIPEBD_N@Z` | someone called `RegisterRuntimeOrdinal(2991)`, so the row printed `ported=1` — and the same function ALSO had a ported `export` row. Counted twice in **both** columns. |
| 3089 | `?IsA@Base@Object@@QEBA_NI@Z` | identical shape |

**A phantom that prints as PORTED is invisible to every screen aimed at the gap
list**, because the gap list is where people look for defects. Two waves of
readers looked straight at this table and saw four rows to fix, because the
other two were sitting in the column that reads as success. That is why `called`
fell by 5 and `ported` by only 1: `-5` called, `-2` ported for the phantoms,
`+1` ported for the new `Object` row.

### 1.3 Why the runtime was never broken by this

The reporting defect never cost a call site anything, and that is verifiable
rather than assumed: `standalone/resolve_standalone.cpp:ResolveOrdinal` misses
`RtOrdinalReg()`, then falls through to `MangledForOrdinal()` and looks the NAME
up in `RtExportReg()`. `kOrdinalNames` carries a row for all seven ordinals this
tree uses. `RtObjectFactoryFileExtensionRegister()` registers 2990's body by
name **and** by address, ungated. So the exe's two ordinal-2990 call sites have
been reaching our port since Wave 24.

**One stale comment corrected as a side effect and worth naming**, because it is
the shape that made this defect durable: `factory_file_extension.cpp:227` says
the name door "is the door `tools/runtime_census.py` counts". It was not. The
census only mints a row for a name something in the tree *resolves*, and nothing
resolves that name — the call sites use the ordinal. A registration with no
matching call site lands nowhere.

### 1.4 The fix, and its controls

`tools/runtime_census.py`:

* **`load_export_ordinals()`** — `{ordinal -> (name, address)}` from the export
  directory, degrading exactly like `load_export_addrs()`.
* **the ORDINAL FOLD-IN** in `main()`, modelled line-for-line on the address
  fold-in above it. `ported` consults **all three doors** — `ported_ordinals`,
  `ported_exports`, `ported_addrs` — because `ResolveOrdinal` does.
* an ordinal with **no** export-directory row keeps its own `ordinal` row.
* the header prints the fold as a **set-diff** (`ord N -> name`), not a delta,
  so a reader sees which rows moved.

```
tools/runtime_census.py --selftest-ordinal-fold    # 11 assertions, 0 failed
```

| control | shape | result |
|---|---|---|
| 7 in-population positives | each ordinal's name checked against `kOrdinalNames`, an **independent transcription** of the same directory written three waves earlier by a different lane | 7/7 |
| fold-target check | `namespace_of()` of the resolved name must be the real namespace, or the fold would mint a second row instead of joining one | 2/2 (`Object`, `Video`) |
| **HANDED-IN NEGATIVE** | ordinal 65535 must come back unnameable | pass |
| directory-size floor | ≥ 4000 named ordinals, so a stub map cannot pass | pass (4926) |

**The negative control exists because the fold's `unresolved` list is
ABSENCE-SHAPED.** Every ordinal this tree uses is nameable, so a real run prints
no unresolved rows — which is exactly what a **dead lookup** prints. Handing the
path a synthetic ordinal is the only thing that separates the two (kickoff
§3.4). The positive/negative pair is what makes "0 unresolved" mean something:
if the lookup were dead, control 1 would fail too.

**Instrument rule 1, applied while the tool was agreeing with me.** All 11
assertions passed on the first run, so I hand-checked the map against a
*separate* `pefile` script written before the census change
(`ordprobe.py`, scratchpad), which printed ordinal, address and name straight
out of the directory. `2990 -> 0x1800c04c0` matches the address
`factory_file_extension.cpp:240` registers, from a third source. Agreement
across three readings is corroboration; agreement with itself would not have
been.

### 1.4b AUDIT OF THE REST OF THE PARSER — asked for by the coordinator

Lane MANIFEST found that `load_unpromotable()`'s **reason class** is spelled
`(\S+)`, so a class naming two routes (`HARMFUL (DLL-present) / REQUIRED
(cutover)`) fails the optional group, **fails the whole regex, and drops the
entire tag** — five rows printed as ordinary unported work for waves. Same
family as §1.1: a parser silently mis-attributing rows in the file every wave
quotes. I was asked to sweep the rest of the file for the same shape.

```
grep -n '\\S+' tools/runtime_census.py
```

**Result: there are exactly TWO `(\S+)` captures in the whole script, and both
are on the same two lines** (1845–1846, `load_unpromotable`). Every other
capture in the file is `(\d+)`, `(0x[0-9a-fA-F]+)`, `"([^"]*)"`, `([^|]+?)` or
an identifier class — none of which can be truncated by a space. So the family
is confined to one regex, and the two members are:

| capture | field | verdict |
|---|---|---|
| `(\S+)` group 5 | reason class | **MANIFEST's live defect.** Not fixed here — that file is MANIFEST's this wave and a second edit would conflict. |
| `(\?\S+)` group 1 | the symbol key | **A LATENT DEFECT OF THE SAME FAMILY, one field to the left.** |

**Group 1 is anchored on a literal `?`.** Every mangled MSVC name starts with
one, so a space cannot truncate it — but **an ADDRESS-KEYED row cannot match it
at all.** Write `RT_UNPROMOTABLE: 0x18002ac40 | …` and the regex fails, the tag
is dropped whole, and the row prints as ordinary unported work — the *identical
symptom* MANIFEST just spent a lane diagnosing, from a different cause, in the
same regex.

**It is latent, not live, and I measured that rather than assuming it**: all
current `RT_UNPROMOTABLE:` lines in `standalone/reg_mint_promoted.cpp` carry a
`?`-prefixed symbol, so **0 rows are affected today**. But the population that
would hit it is **exactly this lane's bucket** — `internal` is 190 rows, every
one of them keyed by an address — and lane MANIFEST is cataloguing tagged rows
right now. The first address-keyed row anybody tags disappears silently.

Note also that the fix is **two** changes, not one: the capture must admit
`0x…`, **and** the resulting key must be spelled the way the row set spells it
(`"0x%x" % addr`), or the tag parses and then matches nothing.

Nothing else in the parser has this shape. The three `([^|]+?)` fields absorb
spaces correctly and would only break on a value containing a literal `|`; no
current tag does.

### 1.5 Notice to the other lanes

The board figure moved **mid-wave** on a shared instrument, which the brief
authorised and which the other lanes are quoting concurrently. A message went to
`main` at the moment of the change with the before/after and the re-derivation
command. **`1323/1171/88.5%` and `1318/1170/88.8%` are the same tree.**

---

## 2. THE 42 ADDRESS-KEYED ROWS — CLASSIFIED

Starting point is Wave 23's decomposition and Wave 24's, per the brief. What
follows re-derives membership (not classification) and records what moved.

### 2.1 The population moved again — the bucket refills every wave

Wave 24 left 41. This tip has 42. The entrant is **`0x18000c4a0`**, a door Wave
24 lane SMALLBODY opened in `runtime/src/audio/audio_stream_win.cpp`.

**Third consecutive wave in which this bucket refilled from another lane's
faithful port** (Wave 23 →4 from Achievement, Wave 24 →2 from INTERNAL8,
Wave 25 →1 from SMALLBODY). It is not noise and it is not a leak: a faithful
port makes its callees ours, and `internal` is where an unexported callee lands.

### 2.2 The classification, and what this lane did with each class

| class | rows | disposition |
|---|---:|---|
| **`Collection<T>` registrations against cores we already own** | **10** | **PORTED THIS LANE** (§3) |
| `Collection<T>` registrations still to do — `SetSize` with a non-trivial element | 8 | §5.1, recipe given |
| element-serialize walkers | 4 | real bodies, not started |
| destroy-range with its `DestroySpec`s already written (`0x180010f00`) | 1 | §5.1 |
| **the one large body** — `0x180028760`, `Array<Mod>` introsort, 1381 B | 1 | **lane BODY's**, not mine |
| statically-linked MSVC CRT (`qsort`, `_beginthreadex`, `exit` thunk) | 3 | **OUT OF SCOPE**, unchanged |
| unexported vtables | 4 | DATA-shaped; not this lane |
| DirectInput `DIDATAFORMAT` blobs from the platform SDK | 2 | **NOT PORT WORK**, unchanged |
| small `.rdata` literals (a UTF-16 `"SRS"`, a `06 00 00 00`) | 2 | **NOT PORT WORK** |
| mutable statics (2 loader fields, 1 `HMODULE` slot) | 3 | statics |
| CRT locale flag | 1 | **CRT** |
| new entrant `0x18000c4a0` | 1 | **PORTED THIS LANE** — it was an `~Array` all along (§3.2) |
| | **42** | |

**So: of 42 rows, at most 13 are a function to reimplement** (8 SetSize + 4
walkers + the introsort), 11 are not port work at all (CRT ×4, SDK blobs ×2,
literals ×2, statics ×3), 4 are vtables, and the rest were registrations. Wave
23's headline — *"a gap bucket printed as UNKNOWN is not unknown to the
project"* — re-derived and still true.

---

## 3. WHAT THIS LANE PORTED — 10 ROWS, ZERO NEW DOORS

Every one is an entry point into a body this tree already has. None is a new
reimplementation; all ten are what Wave 23 residual #6 described, and this lane
is that residual's addressee.

### 3.1 Two `~Array` (J1) addresses — the eighteenth and nineteenth

`0x18002a930` (IO, Persistence) and `0x18000c4a0` (Audio::Stream +0x30).

**Proved by an ADDRESS-NORMALISED identity check, not by eye and not by a raw
byte diff.** The probe reads 0x73 bytes at each address, diffs against the
canonical J1 body at `0x180004890`, and requires **both**: every differing byte
inside one of the five 4-byte displacement fields, **and** all five *resolved*
absolute targets equal.

```
canonical 0x180004890 -> 0x1801fc718  0x18014b390  0x1800c6c00  0x18014b388
0x18002a930   IDENTICAL   raw-diff=15/115 bytes   outside-disp=0   targets-equal=True
0x18000c4a0   IDENTICAL   raw-diff=13/115 bytes   outside-disp=0   targets-equal=True
0x18002ac40   DIFFERENT   raw-diff=83/115 bytes   outside-disp=63  targets-equal=False   [CONTROL]
```

* **The raw diff is the lesson, printed rather than hidden:** two copies of ONE
  115-byte function differ in 13–15 bytes (11–13%), because every rel32/RIP
  field encodes a distance. A raw byte diff would have called these different.
* **The negative control is in-population and it is the hard case**: the J2 body
  `0x18002ac40` is the same family with one extra `CALL` at the top. If the
  screen passed that, "IDENTICAL" would mean "in this neighbourhood". It fails
  it on both halves of the conjunction.

### 3.2 A stale blocker re-checked and discharged — and it was dependency-shaped

`audio_stream_win.cpp` recorded `0x18000c4a0` as *"NOT re-implemented here: it
is 0x18 bytes of somebody else's object and this lane has no witness for what
lives at +0x30."*

**The observation is true and the inference is false.** The body does not need a
witness for `+0x30`, because it is not a bespoke release: it is byte-for-byte
`Collection::Array<T>::~Array`, and **freeing a buffer does not need to know
what is in it** — which is precisely the reason `array.cpp` states for why J1
takes no stride and no parameters at all. The answer was in the file that
already held the answer.

That is kickoff §1a scoring again: **the blocker was DEPENDENCY-shaped, and it
was false.** It was also **one wave old**, written by a careful lane that
declared its own costs honestly. The correction is left in place in the source
banner rather than deleted, so the next reader sees which half was wrong.

### 3.3 Three `~Array`-with-element-destroy (J2) addresses — and the trap they carry

`0x18002ac40`, `0x180039360`, `0x180039280`.

Wave 23 established by address-normalised opcode hash that these three are **ONE
body** (127 B, hash `089ee8`). **That is true, and it does not mean they take the
same spec.**

```
0x18002ac40 -> CALL 0x18002afc0   LEA RBX,[RDX+0x08] ... ADD RBX,0x10   stride 0x10, String at +0x08
0x180039360 -> CALL 0x180039640   LEA RBX,[RDX+0x08] ... ADD RBX,0x10   stride 0x10, String at +0x08
0x180039280 -> CALL 0x180039580   LEA RBX,[RDX+0x18] ... ADD RBX,0x20   stride 0x20, String at +0x18
```

**An address-normalised hash erases the rel32 of the `CALL` at the top — which
is the only thing that distinguishes a J2 entry from its siblings.** "Byte
identical" here means "identical except for the one operand that decides the
answer". Taking the hash as authority would have run a stride-0x20 array at
stride 0x10 and walked off the end of its buffer, and nothing in the build or
the census would have said so.

Two new `DestroySpec` rows (`kD_Str_10at08`, `kD_Str_20at18`). The suffix in
that family's names is the **stride**, so a row at an existing stride and a new
offset needs a new name or the next reader reuses the wrong one.

### 3.4 Five `SetSize` (J3) addresses

| address | spec | read out of the body |
|---|---|---|
| `0x18002a9d0` | `kSpec_Fill8` **reused** | `SHL 3` / `MOV EDX,8` / one qword copy / empty else arm → tail UNINITIALISED |
| `0x18002a810` | `kSpec_Blk12` **new** | `LEA RCX,[RAX+RAX*2]; SHL 2` = 0xc / `MOV EDX,4` / three dword copies / tail UNINITIALISED |
| `0x18002f070` | `kSpec_U64Zero` reused | `SHL 3` / `MOV EDX,8` / plain qword copy / `MOV [RDX],R10`, R10 = 0 → tail ZEROED |
| `0x1800492d0` | `kSpec_U64Zero` reused | identical, re-derived not inherited |
| `0x180005110` | `kSpec_U64Zero` reused | identical |

`0x18002a9d0` and `0x18002a810` are **Wave 24 lane INTERNAL8's declared debt**,
paid. That lane grew the surface by 2, said so in the flattering-number
paragraph, and named its addressee; this is that addressee.

**Two spec traps avoided, both of which the file's own history says have bitten:**

* `0x18002f070`/`0x1800492d0`/`0x180005110` hold `ExternalPtr` elements and take
  `kSpec_U64Zero`, **not** `kSpec_PtrZero`. `kSpec_PtrZero` uses `Assign8Z` (a
  zero store then the value) and these bodies have no zero-first store. **The
  element type does not decide the spec; the body does** — the same rule
  `kSpec_U64ZeroA4` was written for, where two bodies agreed on everything but
  the alignment immediate.
* Where the SetSize family IS bound by hash, this lane says so and why: a
  SetSize body's stride, alignment and element ops are **immediates and
  opcodes**, which an address-normalised hash preserves, whereas a J2 body's
  spec lives in a **rel32**, which it erases. Same hash property, opposite
  conclusions, and §3.3 is the half that bites.

### 3.5 The build

`mingw32-make standalone-debug` — **clean**, no new warnings.
`tools/dispatch_slot_map.py --check` — 110 registrations, **0 mismatches**.
`tools/runtime_census.py --selftest-classifier` — **15/15**.

---

## 4. WHAT I DID NOT ESTABLISH — plainly

1. **I ran no game.** No lock, by design (kickoff §1d). **Nothing in this
   document claims any of these ten registrations has ever executed.** There is
   no `calls=` column, no attendance and no divergence count for any of them.
   Nine of the ten have call sites in our tree; **a call site is not
   attendance**.
2. **I wrote no new oracle.** Every one of these ten rows routes into
   `SetSizeCore` / `ArrayDtorImpl` / `ReleaseStringsCore`, which already carry
   lane Q's and lane D2's probes and shadow sites. The ports are new *entry
   points*, not new *bodies*, so a new instrument would have been testing the
   same core twice. **That also means these ten inherit whatever those probes
   do and do not cover, and I did not re-audit that coverage.**
3. **The two new specs (`kSpec_Blk12`, `kD_Str_10at08`, `kD_Str_20at18`) are
   verified by TRANSCRIPTION from the disassembly only.** No probe has run
   against a stride-0xc element or a String at +0x18. If one of those offsets is
   wrong, the build is clean, the census is green, and the corruption is
   silent — kickoff §3 rule 6.
4. **I did not re-derive Wave 23's classification of the 30 unexported
   functions.** The brief said not to, and I did not. What I re-derived is
   *membership* of the bucket and the specs of the ten rows I touched.
5. **I did not touch `0x180028760`** (the 1381-byte `Array<Mod>` introsort). It
   is lane BODY's row. Its "not covered by `introsort.cpp`" blocker is now TWO
   waves old and still uninspected by anyone.
6. **The `AEAA`-means-private trap: I did not re-run Wave 23's screen.** Wave 23
   proved all 183 address-keyed rows genuinely unexported with a handed-in
   control at 100% recall over 1,125 in-population cases. I inherited that and
   did not re-derive it; my own ordinal work is evidence in the *same*
   direction (every name-bearing thing in this DLL is in the export directory)
   but it is not that screen.
7. **RTTI was not used anywhere in this lane**, and could not have been:
   `Runtime-x64.dll` is `/GR-`.
8. **`.pdata` was not used to size anything.** The J1 extent (0x73 bytes) came
   from the instruction stream, bounded by `RET` then `CC` padding.
9. **The non-literal census blind spot is still unmeasured in general.** My
   ordinal fix closes blind spot NINE; blind spot "a registration whose name is
   a named constant" is untouched, and I did not survey it.

---

## 5. RESIDUALS, EACH WITH A SCOPE AND AN ADDRESSEE

A condition with no addressee never fires (`reference_expiry_clause_needs_an_addressee.md`).

| # | item | SCOPE | ADDRESSEE |
|---|---|---|---|
| 1 | **Eight `SetSize` rows remain**: `0x18002c6a0`, `0x1800332d0`, `0x180034010`, `0x18004bde0`, `0x180053f00`, `0x180075b20`, `0x180075d20`, `0x180078950`. All are the SAME registration work as §3.4 with a NON-TRIVIAL element (String refcounts / inner arrays), so each needs an `assign`/`init`/`destroy` **and** the per-arm written-extent windows the probe compares. Recipe: §5.1. | `runtime/src/collection/array.cpp` only | **Wave 26, a named lane — NOT "whoever touches array.cpp"** |
| 2 | **Four element-serialize walkers**: `0x18002f170`, `0x1800493d0`, `0x180078ad0`, `0x180052640`, plus the 3 achievement entrants `0x180004590`, `0x180005220`, `0x180009a20`. These are real bodies (234 instructions for `0x180005220`), not registrations. | the walker bodies | **Wave 26 lane, to be named at kickoff** |
| 3 | `0x180010f00` — a destroy-range whose `DestroySpec`s are **already written** at `video/video_resources.cpp:186,188`. Wave 23 said so and nobody has registered it. Cheapest row left in the bucket. | one registration | **Wave 26, same lane as #1** |
| 4 | **The bucket refills every wave from other lanes' faithful ports** (4, then 2, then 1). This is structural, not a leak. The census now decomposes `internal` no better than before. Folding `internal_row_classify.py`'s SECTION/EXPORT/PDATA/PADDING/RELOC columns into `runtime_census.py` — Wave 23 residual #2, addressed to the Wave 24 integrator, **still not done** — would stop each wave rediscovering it. | `tools/runtime_census.py` | **Wave 25 integrator**, explicitly, since the file is now open |
| 4b | **`load_unpromotable()`'s SYMBOL capture is `(\?\S+)` and cannot match an ADDRESS-KEYED row** — the tag is dropped whole and the row prints as ordinary unported work, the same symptom MANIFEST fixed one field to the right. **Latent, not live: 0 rows affected today**, measured, but `internal` is 190 address-keyed rows. The fix is TWO changes: admit `0x…` in the capture AND spell the key as the row set does (`"0x%x"`). See §1.4b. | `tools/runtime_census.py` lines 1845–1846 ONLY | **lane MANIFEST**, this wave — it owns that regex and is already editing those two lines; do not let a second lane touch them |
| 5 | **The ordinal fold is a fix to ONE door.** `ResolveOrdinal` also warns when an ordinal has no `kOrdinalNames` row; that warning fires at RUNTIME and nothing checks it statically. A static check that every `ResolveOrdinal(N)` literal in the tree has a `kOrdinalNames` row would catch the next one before a run. | `standalone/resolve_standalone.cpp` + the census | **lane SWEEP, this wave** (it is already touching instrument debt) |
| 6 | `factory_file_extension.cpp:227`'s claim that the name door "is the door `tools/runtime_census.py` counts" was **false** and is now true only because the ordinal folds onto it. Left in place; if the fold is ever reverted the comment goes stale again. | that comment | **whoever reverts the fold** — conditional, and the condition is checkable |

### 5.1 The recipe for residual #1, so it is not re-derived

For each remaining `SetSize` address, from its own disassembly and nothing else:

1. **stride** — the `SHL`/`LEA` chain feeding `RCX` at the `CALL 0x1800c7a50`.
2. **align** — the immediate in `MOV EDX,imm` immediately before that `CALL`.
3. **assign** — the stores in the `i < oldSize` arm. A zero-first store means
   `Assign8Z`, not `Assign8`. **Do not infer this from the element type.**
4. **init** — the `else` arm. *Empty* means the new tail is UNINITIALISED, which
   is a real, reproducible property, not an omission.
5. **destroy** — only if the body CALLs a helper; then read that helper's
   `LEA RBX,[RDX+off]` and `ADD RBX,stride` for its `DestroySpec`.
6. **written extents** — the per-arm unwritten byte windows. An oracle that
   compares bytes NEITHER side writes fails 100% of calls and reads like a
   catastrophic port defect.

And the two rules §3 paid for: **the hash binds a `SetSize` and does not bind a
`~Array`-with-helper**; and **a spec is read out of the body, never out of the
caller's element type.**
