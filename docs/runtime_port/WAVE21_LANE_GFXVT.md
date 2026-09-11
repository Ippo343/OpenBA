# WAVE 21 LANE GFXVT — the 8 Graphics VTABLE rows

Branch `wave21/gfxvt`, cut from `wave21/base` at `018426f2`. Worktree
`.claude/worktrees/agent-a1ae73fd6a70ab7ee`. Closed 2026-09-11.

---

## 0. HEADLINE

**The kickoff's numbers re-derive EXACTLY. Its open question was a real census
defect, and it is BIGGER than the kickoff described — five rows, not three, and
for TWO different reasons that must not be conflated.**

Three of them (`??_7EdgeMesh@`, `??_7EdgeMeshInstance@`, `??_7MeshInstance@`, all
Graphics) are a genuine blind spot: our tree reaches those tables ONLY through
`RtVtable("…")`, a one-line project-local wrapper around `ResolveExport`, and
`tools/runtime_census.py`'s `called` scan matches door spellings by LITERAL
identifier. It has never heard of that wrapper. **These are live cutover
dependencies that the `called` column does not count.** Measured size of the
class: **57 of 2271 mangled-literal call sites** in the compiled source dirs go
through **12 wrapper identifiers the census does not know** (§3.2).

The second headline is the opposite shape and is the more useful result:

**`??_7Geometry@Graphics@@6B@` — the ONE COMPLETE row in the set, whose stamper
census is 2-of-3 ours — is HARMFUL to promote, and the reason is invisible to
both readiness gates.** It is now tagged `RT_UNPROMOTABLE(HARMFUL)` with an
addressed expiry (§5.2). Without that tag the next lane reads
`??_7Geometry@Graphics@@6B@  6  6  COMPLETE`, sees a green stamper census, and
promotes it.

---

## 1. WHAT I DID AND DID NOT DO — read this before any number below

**DID:** re-derived every kickoff figure; established each row's extent in both
error directions from two independent channels plus the raw DLL bytes;
enumerated every stamper of all 11 relevant rows from the binary and
corroborated one row against Ghidra; ported and registered 8 vtable slot bodies;
tagged one row unpromotable; ran the game twice and measured attendance.

**DID NOT — each of these is a hole, not an omission I am hiding:**

| not checked | why it matters |
|---|---|
| **No A/B on the Geometry HARMFUL verdict.** | §5.2 is derived from bytes + two source sites + ResolveAddr's documented order. It was never confirmed by promoting the row and watching it break. A lane overturning it must run the A/B, not re-read my comment. |
| **I did not fix `statusicon_geometry.cpp:341`.** | It is the one-character unlock for Geometry, prescribed in Wave 6 and never applied. It is exe game code, outside a vtable-scoped brief, and changing it changes what lands in three live vtable slots. Handed on with an addressee. |
| **I did not patch `tools/runtime_census.py`.** | Six other lanes are measuring against it this wave; moving its numbers mid-wave is exactly the "lane edits after report" failure. Defect measured and reported; fix proposed in §3.3, not applied. |
| **I ported 8 of the 29 unported non-purecall slots.** | 21 remain. §4 names each. I did not attempt any `Serialize`, `Draw`, `AddDrawItems`, `Update` or deleting-dtor slot. |
| **Attendance of my 8 bodies is 0, and I did not try to manufacture a route.** | §6. Expected: nothing dispatches an unpromoted row, and no door in our tree resolves those names. |
| **`??_7Light@` / `??_7SunLight@` slot-0 (the unnamed deleting-dtor thunks at 0x180058dd0 / 0x180064620).** | Real bodies with SEH prologues, not accessors. Untouched. |
| **The other 9 wrapper identifiers of §3.2.** | I established that `RtVtable`'s 3 symbols are invisible. I did NOT check whether the other 54 sites' symbols are reachable by some other spelling the census does know. |

---

## 2. RE-DERIVATION (kickoff §1 and the 8-row table)

Command, run in this worktree at `018426f2` + my changes' *parent*:

```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py --namespace Graphics --unported
tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py          # PROMOTION BACKLOG
```

**Denominator: the 29 called-but-unported Graphics rows.** All 29 re-derive
character for character, including the 13/2/2/8/8 decomposition by symbol kind.
**The kickoff is not stale on any figure I was given.**

The 8 VTABLE rows, re-derived. `ours / slots` are NON-PURECALL slots (this is the
backlog's convention and it differs from the mint table's total, which is why
both are shown):

| row | DLL VA | backlog ours/slots | mint total slots | purecall | kickoff said | verdict |
|---|---|---|---|---|---|---|
| `??_7AnimationEvent@Graphics@@6B@` | 0x180154bf0 | 2 / 4 | 9 | 5 | 2 / 4 | ✔ exact |
| `??_7Geometry@Graphics@@6B@` | 0x180154e60 | 6 / 6 COMPLETE | 9 | 3 | 6 / 6 COMPLETE | ✔ exact |
| `??_7Light@Graphics@@6B@` | 0x180158d30 | 5 / 6 | 11 | 5 | 5 / 6 | ✔ exact |
| `??_7MaterialInstance@Graphics@@6B@` | 0x1801545e0 | 2 / 9 | 9 | 0 | 2 / 9 | ✔ exact |
| `??_7Particle@Graphics@@6B@` | 0x180155130 | 2 / 6 | 9 | 3 | 2 / 6 | ✔ exact |
| `??_7ParticleInstance@Graphics@@6B@` | 0x1801550e0 | 5 / 9 | 10 | 1 | 5 / 9 | ✔ exact |
| `??_7ParticleSystemInstance@Graphics@@6B@` | 0x180155040 | 5 / 10 | 10 | 0 | 5 / 10 | ✔ exact |
| `??_7SunLight@Graphics@@6B@` | 0x180158e10 | 5 / 11 | 11 | 0 | 5 / 11 | ✔ exact |

Total unported non-purecall slots across the 8 rows at kickoff: **29**.

---

## 3. THE EdgeMesh / EdgeMeshInstance / MeshInstance ASYMMETRY

### 3.1 It is FIVE rows, and TWO causes

Set-diff of the census's own two lists (denominator: the 51 rows the PROMOTION
BACKLOG prints — 48 named `??_7` rows plus 3 address-keyed rows):

```
in BACKLOG, absent from "called but not ported":
  ??_7EdgeMesh@Graphics@@6B@            <- kickoff named
  ??_7EdgeMeshInstance@Graphics@@6B@    <- kickoff named
  ??_7MeshInstance@Graphics@@6B@        <- kickoff named
  ??_7ElementDescription@UI@@6B@        <- kickoff did NOT name
  ??_7Loader@Engine@@6B@                <- kickoff did NOT name
  0x1801554a8 / 0x180155860 / 0x180157c60   (address-keyed, no export name)
```

**CAUSE A — a census defect, and it is the three Graphics rows.** All three are
resolved exclusively through `RtVtable(…)`:

```
runtime/src/graphics/mesh.cpp:305   void* RtVtable(const char* m) { return ResolveExport(m); }
runtime/src/graphics/mesh.cpp:308   RtVtable("??_7MeshInstance@Graphics@@6B@")        // 0x180154dc0
runtime/src/graphics/mesh.cpp:312   RtVtable("??_7EdgeMeshInstance@Graphics@@6B@")    // 0x180154d70
runtime/src/graphics/mesh.cpp:316   RtVtable("??_7EdgeMesh@Graphics@@6B@")            // 0x180154c70
```

`runtime_census.py`'s `RE_EXPORT` is `ResolveExport(?:Raw)?\s*\(\s*(?=")` — a
LITERAL identifier match. `RtVtable("…")` is not that spelling, and no other
pattern in the file covers it, so the symbol never enters `called`.

**These are real cutover dependencies, and mesh.cpp says so itself** (the comment
above `RtVtable` explains that a raw door here "writes a NULL vtable into every
object of these classes and the first virtual call is an EXEC-of-0"). The census
is therefore understating Graphics `called` by 3.

**The control that makes this a diagnosis rather than a guess:** `RtVtable` has
exactly **5** call sites, and the other two — `??_7Base@Object@@6B@` and
`??_7External@Object@@6B@` — DO appear in the unported list. They survive because
they are *also* reached through spellings the census knows
(`RT_DATA_SLOT(void*, "??_7Base@Object@@6B@")` at
`runtime/src/io/package_file.cpp:311` and `:314`). So the blind spot is real and
its effect is masked wherever a second door happens to exist. 3 of 5 exposed.

**CAUSE B — not a defect. `??_7ElementDescription@UI@@6B@` and
`??_7Loader@Engine@@6B@`.** Our tree does not resolve either table at all (grep
over `code/ engine/ runtime/ standalone/ dx11/` finds `Loader` only inside
`standalone/rt_data_mint.cpp`'s own diagnostic string, and `ElementDescription`
nowhere outside the generated `.inc`). They are in the backlog because one or
more of their SLOT BODIES is registered, which is the backlog's key. **The two
lists key on different things — backlog on slot bodies, `called` on table
resolution — so this asymmetry is correct accounting** and the same is true of
the three address-keyed rows, which have no export name to appear under.

### 3.2 How big is the blind spot? (with its denominator)

Scan of every `<Identifier>("<mangled>")` call site in `runtime/ engine/ code/
standalone/ dx11/`, C and C++ comments stripped first:

```
files scanned: 1521
mangled-literal call sites: 2271
  through spellings runtime_census.py knows : 2214
  through an UNKNOWN wrapper identifier     :   57   (12 distinct identifiers)
```

The 12, by site count: `RtResolveExportForProbe` 11, `Dx11ResolveRuntimeImport`
9, `AppPair` 8, `RT_VTABLE_ONCE` 6, `RtVtable` 5, `VerifyBody` 3, `VerifyFailed`
3, `CallVideoCallback` 2, `RawVtable` 2, `RtInternalVtblPinRow` 2,
`RuntimeExportIsRegistered` 2, `TypeIdOfStatic` 2, `RtPortLookupExport` 2.

**Several of those are correctly NOT doors** (`VerifyBody`/`VerifyFailed`/
`RuntimeExportIsRegistered` are probes and predicates). **I did not classify the
other nine** — see §1. 57 is the upper bound on the blind spot, 3 is what I
proved it currently costs.

### 3.3 The fix I propose and did NOT apply

Not "teach the census about `RtVtable`" — the next wrapper repeats the failure
and announces nothing, which is the recorded shape of
`reference_literal_grep_blind_to_variables` (four instances) and
`reference_census_cannot_see_the_macro_doors` (the fifth). **This is the sixth.**

The generalisable fix is a SCREEN, not another pattern: have the census report
every `<Identifier>("<mangled Runtime symbol>")` site whose identifier is not in
its known-door set, so an unknown wrapper is *named on every run* instead of
silently dropping symbols. Scanner and measured output above; it is ~30 lines.
Handed to **lane CLASS**, which owns the census this wave.

---

## 4. EXTENT — how each bound was established, and how confident, BOTH directions

`tools/rt_mint_gen.py` sizes a row by decoding forward from its start, bounded by
the next EXPORTED symbol, **stopping early on a zero or a qword that is not an
image pointer**. The proof string it records says which bound actually bit:

* `adjacent-export:X` — the decode ran all the way to the next export. **This is
  the shape `reference_adjacent_export_is_an_upper_bound` burned two rows on.**
* `tail-not-a-pointer` — the pointer run ended before the next export.

**These are not equally dangerous, and the kickoff's warning applies mainly to
the first.** For a `tail-not-a-pointer` row, UNDER-sizing is ruled out by the
decode rule itself (a vtable slot is always an image pointer, and the qword after
the last slot is not one). OVER-sizing remains possible only if an *unexported*
table starts inside the run — which requires that table's start to be named by no
instruction in the image.

Two independent channels, re-run in this worktree:

```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/mint_extent_audit.py
tools/ghidra_mcp/.venv/Scripts/python.exe tools/mint_stamper_census.py --rows …
```
Decoder control 3 of 3 found in both. Whole-file totals over **62 mint rows**:
**0 OVER-SIZED, 47 CORROBORATED-EXACT, 15 CONFIRMED-NO-INTERIOR-STAMP, 0
COULD-NOT-DERIVE.**

| row | mint proof | audit verdict | my added check (raw DLL bytes) | confidence |
|---|---|---|---|---|
| `AnimationEvent` 9 | tail-not-a-pointer | CORROBORATED-EXACT — next named table starts at 0x180154c38 = row end | — | high, both directions |
| `Geometry` 9 | tail-not-a-pointer | CORROBORATED-EXACT — next named start 0x180154ea8 = row end | 0x180154ea8 = `49 6d 61 67 65 42 75 66` **"ImageBuf"**, a string run, not a table | high; and the tail matters — see §5.2 |
| `Light` 11 | tail-not-a-pointer | **CONFIRMED-NO-INTERIOR-STAMP** (writer channel bounds it at ≤16 only) | 0x180158d88 onward: four ZERO qwords then `0x1` — not a vtable | high: the zero run makes swallowing impossible |
| `MaterialInstance` 9 | tail-not-a-pointer | **CONFIRMED-NO-INTERIOR-STAMP** (≤10 only) | 0x180154628 = `0x0000000000000000`; next table begins 0x180154630 | high: one zero qword of padding is a hard terminator |
| `Particle` 9 | tail-not-a-pointer | CORROBORATED-EXACT — next named start 0x180155178 | — | high |
| `ParticleInstance` 10 | **adjacent-export**`:??_7Particle@` | CORROBORATED-EXACT — next named start 0x180155130 = row end | — | good; this is the at-risk shape, and the audit cleared it |
| `ParticleSystemInstance` 10 | **adjacent-export**`:??_7ParticleSystem@` | CORROBORATED-EXACT — next named start 0x180155090 = row end | — | good; same, cleared |
| `SunLight` 11 | tail-not-a-pointer | CORROBORATED-EXACT — next named start 0x180158e68 = row end | — | high |

**Both error directions, stated:**

* **OVER-sizing (a row swallowing an unexported neighbour — the recorded
  failure).** Ruled out for all 8: two of them by a zero-qword terminator I read
  myself, six by the writer channel finding the next *named* table start exactly
  at the row's end. Residual risk: a table whose start no instruction in the
  image names. The audit measures that failure mode on the only set it can — 62
  of 62 known row starts ARE named by a real instruction, **0 instances** — but
  that is a completeness argument, not a proof, and I am not converting it into
  one. `CONFIRMED-NO-INTERIOR-STAMP IS NOT 'EXACT'`, as the tool itself prints.
* **UNDER-sizing (a row losing slots with no symptom — the *silent* direction).**
  Structurally excluded here: no row in this set carries a hand-written
  `VTABLE_EXTENT` override, and `VTABLE_EXTENT` is the ONLY mechanism in the
  generator that can shorten a row. The two entries that exist
  (`??_7ReaderWriter@`, `??_7TextFileStream@`) are IO's, not mine. Every one of
  my 8 lengths is what the bytes decode to.

**So: no extent correction is needed on any of the 8, and the kickoff's caution,
while right in general, does not bite on this set.**

---

## 5. PROMOTABILITY — the stamper question, per row

### 5.1 The stamper census

`tools/mint_stamper_census.py` groups every RIP-relative-LEA-then-store of each
table address by the `.pdata`-bounded function containing it, then joins against
a comments-stripped literal registration scan. Run in this worktree: decoder
control 3/3, ownership positive control REGISTERED, negative control
not-registered. **`ours-capable` is an UPPER BOUND** (a registration is a door,
not a guarantee every caller uses it) and the tool says so.

| row | stamp sites | ours-capable | retail | unattributed |
|---|---|---|---|---|
| `AnimationEvent` | 5 | 3 | 2 | 0 |
| `Geometry` | 3 | 2 | 1 | 0 |
| `Light` | 9 | 1 | 8 | 0 |
| `MaterialInstance` | 10 | 3 | 7 | 0 |
| `Particle` | 4 | 1 | 3 | 0 |
| `ParticleInstance` | 8 | 3 | 5 | 0 |
| `ParticleSystemInstance` | 3 | 1 | 2 | 0 |
| `SunLight` | 3 | 2 | 1 | 0 |
| *(context)* `EdgeMesh` | 3 | 1 | 2 | 0 |
| *(context)* `EdgeMeshInstance` | 2 | 2 | 0 | 0 |
| *(context)* `MeshInstance` | 2 | 2 | 0 | 0 |

**GOOD NEWS, and it is the answer to the kickoff's step-3 question: NOT ONE of
the 8 rows is unstampable.** Every row has at least one stamper we own, and
**zero** sites across all 11 rows are unattributed. There is no
`??_7DialogController@UI@@`-shaped row here — nothing in this set is
"permanently unpromotable because we intercept only at doors".

**Corroborated in Ghidra for one row** (Runtime-x64.dll, confirmed active before
the read). `get_xrefs_to(0x180154e60)` returns exactly the three functions the
writer channel named — `Geometry` 0x180015b7e (copy ctor), `~Geometry`
0x1800a41d5, `Geometry` 0x1800a4281 (default ctor) — each twice (the LEA and the
store), plus the export-table entry. **Two derivations by different means,
agreeing.** I did not repeat this for the other 10 rows.

### 5.2 `??_7Geometry@Graphics@@6B@` — COMPLETE, STAMPABLE, and HARMFUL

This is the row that mattered, because it passes both gates and is still wrong to
promote. Full evidence is in the `RT_UNPROMOTABLE` block now in
`standalone/reg_mint_promoted.cpp`; the chain in brief, each link measured:

1. `statusiconcomponent.cpp:611` calls the Geometry base ctor on `s+0x28`, reads
   back the stamped vtable as `baseV`, hands it to
   `StatusIconGeometry_BuildVtable`. Gated on `BANISHED_LIVE_STATUSICON_DRAW`,
   which is in `kDefaultOnFlags` (`component_factory_base.cpp:189`) — **default
   on**.
2. `statusicon_geometry.cpp:341` copies **`i < 12`** slots out of that table.
3. The table is **9** slots. I read 0x180154e60 from the shipped DLL: slots 0..8
   are image pointers, and 0x180154ea8 is `"ImageBuf"`. So `baseVtbl[9..11]` are
   `0x6675426567616d49`, `0x726566`, `0x65746172656e6547` — all non-zero, so the
   `baseVtbl[i]` guard passes and **all three are installed as function
   pointers**. (WAVE6_LANE_D §6.1 found exactly this in Wave 6 and prescribed
   `i < 9`. It was never applied.)
4. Our ported Geometry ctor stamps `ResolveAddr(Module::Runtime, 0x180154e60)`
   (`scene.cpp:176`), and **ResolveAddr consults the port registry FIRST**.
   Promoting the row — which, following the `??_7BinaryFileStream@` precedent,
   registers by name AND address — redirects that stamp to `kVTable28`, a
   `static void* kVTable28[9]`.
5. Step 2 then reads **24 bytes past the end of that 9-element array**, out of
   our image's adjacent static storage, into a live exe-side vtable.

Promotion does not create the overrun; it **moves it off known-dormant DLL bytes
onto unanalysed storage of ours.** Tagged `HARMFUL`, not `NO-OP`: the two are
near-opposites and this one is the "would change behaviour for the worse" kind.

**Expiry, with an addressee:** `statusicon_geometry.cpp:341` must read `i < 9`.
The addressee is **the next lane that owns `code/Game/Component/`** — explicitly
not "whoever next counts slots on this row", because that phrasing satisfies a
checklist and then waits forever
(`reference_expiry_clause_needs_an_addressee`). When that line is fixed the tag
is void and the row should be re-scored; its stamper census is already green.

`tools/unpromotable_slot_check.py` after the tag: **21 of 21 scoreable tags fit,
0 STALE**, both controls green.

### 5.3 The other seven

None is unpromotable. All seven are simply **INCOMPLETE** — ordinary port work,
which is §4's list. Promoting any of them today would put a named trap in a live
table. No RT_UNPROMOTABLE tag is warranted for any.

---

## 6. WHAT I PORTED, AND ITS ATTENDANCE

New file `runtime/src/graphics/graphics_typeinfo_accessors.cpp`, following
`runtime/src/object/object_typeinfo_accessors.cpp` (Wave 11 lane OBJDATA) —
same door, same oracle idiom. Registered from `rt_graphics_port.cpp` behind
`BANISHED_RT_GFXTI` (unset = on).

**Grep-before-port:** all eight mangled names AND all eight addresses searched
across `code/ engine/ runtime/ standalone/ dx11/` before a line was written.
**Zero hits.** This file is the first body for each.

| slot body | DLL VA | shipped bytes | ours |
|---|---|---|---|
| `?Type@AnimationEvent@Graphics@@UEBAIXZ` | 0x1800116d0 | `MOV EAX,[0x1801f9978]` | node+0x00 |
| `?TypeName@AnimationEvent@Graphics@@UEBAPEBDXZ` | 0x1800116f0 | `MOV RAX,[0x1801f9980]` | node+0x08 |
| `?Type@MaterialInstance@Graphics@@UEBAIXZ` | 0x1800027f0 | `MOV EAX,[0x1801fa778]` | node+0x00 |
| `?TypeName@MaterialInstance@Graphics@@UEBAPEBDXZ` | 0x180002810 | `MOV RAX,[0x1801fa780]` | node+0x08 |
| `?GetFileVersion@MaterialInstance@Graphics@@UEBAHXZ` | 0x180103a60 | `MOV EAX,4 ; RET` | literal 4 |
| `?Type@Particle@Graphics@@UEBAIXZ` | 0x18001ff10 | `MOV EAX,[0x1801fa7b0]` | node+0x00 |
| `?TypeName@Particle@Graphics@@UEBAPEBDXZ` | 0x18001ff30 | `MOV RAX,[0x1801fa7b8]` | node+0x08 |
| `?GetFileVersion@Particle@Graphics@@UEBAHXZ` | 0x180104080 | `MOV EAX,0x1000 ; RET` | literal 0x1000 |

Every RIP displacement resolved by hand against the NEXT instruction address, and
**the +0x00/+0x08 reading is cross-checked, not merely decoded**: all three node
VAs appear in `standalone/rt_data_mint_tables.inc` as the minted `?_typeInfo@`
rows at exactly those addresses (lines 1833, 2057, 2089). `this` is read by none
of the eight — that is from the encodings, which contain no RCX operand, not
from a decompile.

**Registered by NAME and by ADDRESS**, both, for all eight — a body ported but
not registered is indistinguishable from an absent one to the census.

### 6.1 Census movement, and it is NOT all upward

Denominator: the Graphics namespace row of `tools/runtime_census.py`, before and
after, same tool, same tree, my change the only variable.

```
                 total  called  ported  covered  ceiling
before             599      99      70    70.7%    96.0%
after slot ports   599     100      70    70.0%    96.0%
after Geometry tag 599     100      70    70.0%    95.0%
```

**`covered` went DOWN and `ceiling` went DOWN. Both are correct.** Three separate
effects, stated separately because merging them is how a percentage lies:

* **`ported` did not move (+0).** A minted vtable slot body lands in the
  PROMOTION BACKLOG, not in `ported`, until its ROW is promoted. Eight bodies,
  zero column movement. This is the census working as designed.
* **`called` went UP by 1**, and it is my doing: my `RT_DATA_SLOT` door on
  `?_typeInfo@AnimationEvent@Graphics@@0VBaseTypeInfo@Object@@A` is a new cutover
  dependency that did not exist before. **Porting grew the cutover surface**
  (`reference_porting_can_grow_the_cutover_surface`). The Graphics gap is
  therefore **29 → 30**, and the new row is a **DATA** row — *i.e. it lands in
  lane GFXDATA's category, not mine.* **GFXDATA and the integrator should know
  this.** The door is nonetheless correct: reading through it is what makes these
  bodies FOLLOW a promotion of that `?_typeInfo@` row.
* **`ceiling` 96.0% → 95.0%** is entirely the Geometry tag. `ceiling = (called −
  unpromotable) / called` has `called` in both halves, so proving a row
  unpromotable moves it DOWN
  (`reference_the_ceiling_percentage_is_a_moving_target`). **Score the identity,
  not the percentage.**

What DID move, and it is the number this lane should be read on — the backlog:

```
??_7AnimationEvent@Graphics@@6B@       2/4  ->  4/4  COMPLETE
??_7MaterialInstance@Graphics@@6B@     2/9  ->  5/9
??_7Particle@Graphics@@6B@             2/6  ->  5/6   (one slot from COMPLETE)
```

Unported non-purecall slots across the 8 rows: **29 → 21.**

### 6.2 ATTENDANCE: zero, and the run that says so

Build `ba7d47ea7b2f887e797b778590d71e65`, deployed under
`tools/game_dir_lock.sh acquire GFXVT` (lock held for both runs, released after).
Identity line in both logs: `[RtPort] Wave 21 lane GFXVT: 8 typeinfo-accessor
exports registered`.

**Run 1 was INVALID and I am recording it because the trap is recorded and I
walked into it anyway.** `BANISHED_AUTOSTART=1 BANISHED_AUTOQUIT=1800` — AUTOQUIT
**overrides** autostart's New Game path (`[AutoStart] menu settled — injecting
Quit click`), so that run never left the menu
(`reference_autoquit_overrides_autostart_newgame`). A menu-only run cannot
measure a Graphics vtable.

**Run 2, the one the number comes from:**
`BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120
BANISHED_PAUSEMENU_BUTTON=6 BANISHED_RT_DIFF=1`. It reached
`[StartNewGame] ENTER … seed=663701748`, built the world, opened the in-game
pause menu at frame 1021, clicked Quit, exited cleanly. **`standalone_crash.log`
0 bytes.** 625 RtDiff sites registered, 33 of them `Graphics::*`.

**None of my eight bodies appears among the 625.** An `RT_DIFF_BEGIN` site
registers on first entry, so absence from the site list means the body was never
entered: **attendance 0 / 8.**

**This is the expected result, not a failure, and it is the distinction the
kickoff's rule 4 exists for.** `??_7AnimationEvent@` is now COMPLETE — *safe to
stamp*. Nothing stamps it, because the row is not promoted and no door in our
tree resolves those eight names. A green readiness screen is not a route. I am
explicitly **not** reporting a promotion or a live route.

**What that run DOES establish:** the eight registrations do not break the boot,
the world build, or a clean shutdown.

---

## 7. FOR THE INTEGRATOR

1. **`tools/runtime_census.py` has a sixth door blind spot**, costing 3 real
   Graphics dependencies today and reaching 12 unknown wrapper identifiers / 57
   sites at most. Fix proposed in §3.3, **not applied** — lane CLASS's call.
2. **The Graphics gap is now 30, not 29**, and the new row
   `?_typeInfo@AnimationEvent@Graphics@@0VBaseTypeInfo@Object@@A` is **lane
   GFXDATA's kind**, opened by my port.
3. **`??_7Geometry@` is now tagged HARMFUL** and Graphics' printed ceiling moved
   96.0% → 95.0% because of it. Expected, and the tag moved no other count.
4. **The unlock is one character** — `statusicon_geometry.cpp:341` `i < 12` →
   `i < 9`, prescribed in Wave 6, never applied. Whoever takes it should expect
   the `RT_UNPROMOTABLE` tag to be deleted in the same commit.
5. **Files touched:** `runtime/src/graphics/graphics_typeinfo_accessors.cpp`
   (new), `runtime/src/graphics/rt_graphics_port.cpp`,
   `standalone/reg_mint_promoted.cpp` (comment block only),
   `docs/runtime_port/WAVE21_LANE_GFXVT.md`. **No file owned by GFXBODY or
   GFXDATA was edited.**
