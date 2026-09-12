# WAVE 23 LANE INTERNAL — the 42 `internal` rows and the 4 `ordinal` rows

Branch `wave23/internal`, cut from `wave23/base` at `a60351e3`. Private worktree.
**Every figure in this document is BRANCH-LOCAL** (rule 13); the integrator must
re-measure on the merged tip.

This is an **INSTRUMENT lane**. No body was ported. Nothing is merged.

---

## 0. HEADLINE

**The 42 `internal` rows are not 42 unknowns. They are 30 unexported functions
and 12 statics, and our own tree already names every one of them in a source
comment — the census simply cannot read a comment.**

Decomposed against the binary and against our tree:

| what they are | rows | distinct things |
|---|---:|---:|
| `Collection<T>` template instantiations against cores **we already have** | 16 | 13 bodies, **~0 new cores** |
| Serialize element walkers (one shape, 3 T's) | 4 | 3 bodies |
| **Genuinely new engine bodies — the real work** | **7** | **7** |
| Statically-linked MSVC CRT (`qsort`, `_beginthreadex`, an `exit` thunk) | 3 | 3 — **out of scope** |
| Unexported vtables | 4 | 4 |
| DirectInput `DIDATAFORMAT` blobs available from the platform SDK | 2 | 0 — **not port work** |
| Small `.rdata` literals (a wide-string prefix, a `06 00 00 00`) | 2 | 0 |
| Mutable statics (2 loader fields, 1 HMODULE slot) | 3 | 3 |
| CRT locale flag | 1 | 0 |
| **total** | **42** | |

And separately, the 4 `ordinal` rows:

**Three of the four `ordinal` rows are PHANTOMS — the same already-PORTED
function counted a second time under its ordinal.** The fourth is a real row
misfiled out of `Object`. The 177-row board is therefore **174**, and the
inflation is in the direction the project has been bitten by before.

> **Real remaining engine port work in this whole 46-row bucket: 8 function
> bodies** (7 from `internal` + 1 from `ordinal`), plus 4 vtables, 3 statics and
> ~16 spec-table entries against existing parameterised cores.

---

## 1. DENOMINATOR, AND WHAT I DID NOT CHECK

**Denominator: 42 `internal` rows + 4 `ordinal` rows = 46**, re-derived at
lane start on `a60351e3`:

```
tools/runtime_census.py --csv w23.csv        # 1314 called / 1137 ported / 86.5%
awk -F, '$3=="internal" && $4=="0"' w23.csv  # 42 rows  -- matches the kickoff
awk -F, '$3=="ordinal"  && $4=="0"' w23.csv  #  4 rows  -- matches the kickoff
```

The kickoff board re-derived **unchanged**. No staleness to report there.

### Is the population complete? Measured on one axis, unmeasured on another.

The kickoff warned that `runtime_census.py` builds its row set from **macro-door
literals**, so Wave 22 lane TIMER found `?PlatformStartup@Timer@System@@` live
and registered with **no CSV row at all**. That would make my denominator
incomplete in an unmeasured direction.

I measured the one side I could — `tools/internal_row_denominator.py`:

```
literal Runtime address doors in source (comments stripped): 314
  of which the census has an addr row for:                   173
  of which NO addr row:                                      141
      ... but the address IS an export:                      141
          and the census carries that export row:            141   correctly covered
          export with NO census row at all:                    0
      ... unexported AND no row anywhere:                      0   <- INVISIBLE rows
```

**Zero invisible unexported address doors.** On this axis the census population
is complete, and my 42 is the whole set.

*I nearly published the raw "141 doors missing from the census" figure.* It is
true and it is meaningless: all 141 are exports the census already carries under
their name. A screen that could not name its subject ("missing" — missing as
*what*?) produced a 141-row alarm that a one-line refinement reduced to zero.
That is the inflating-blind-spot failure mode again, caught before publication
rather than after.

### What I did NOT check

1. **The non-literal axis remains unmeasured.** My scanner matches
   `ResolveAddr(Module::Runtime, 0x...)` / `ResolveRuntimeRaw(0x...)` / `RT_ADDR(0x...)`
   with a **literal**. A call through a variable, a named constant, or a plain
   C++ call to an imported symbol is invisible to it exactly as it is invisible
   to the census — the same blind spot, not an independent check of it. The
   TIMER finding is **not** discharged by anything here. Sizing that needs a
   linker-map or symbol-level pass, not a grep.
2. **I did not name the 30 functions from `Runtime-x64-profile.dll`.** See §5 —
   the route the kickoff assumed exists does not, and I say why rather than
   claiming a negative I did not earn.
3. **I ran no game.** This is binary-and-source work throughout; I never
   acquired the game-directory lock and never needed it. No runtime attendance,
   no `calls=` column, no oracle. Nothing here claims a body *runs*.
4. **I did not verify any of the 7 real bodies is actually portable** — no
   blocker was re-checked except where §4 says so explicitly.

---

## 2. THE INSTRUMENT — `tools/internal_row_classify.py`

New tool, this lane. `runtime_census.py` is **untouched** (Wave 22 lane ALLOC
owns it, kickoff §1a).

It answers from the **binary** what the census cannot answer from a **name**:

| screen | question | how it is decided |
|---|---|---|
| SECTION | code or data? | PE section containing the RVA |
| EXPORT | does it have a name after all? | exact export-table RVA match |
| PDATA | is it a function, and how big? | `RUNTIME_FUNCTION`, chain-resolved (§3) |
| PADDING | is it a function with no unwind data? | preceding byte is `0xCC`/`0x90` |
| RELOC | vtable or scalar? | run of base-relocated qwords into `.text` |
| RTTI | vtable by structure? | CompleteObjectLocator at `table-8` — **dead in this binary, see §3.3** |

```
tools/internal_row_classify.py --selftest              # 19 positive controls
tools/internal_row_classify.py --csv w23.csv --out x.csv
tools/internal_row_controls_handed.py                  # in-population EXPORT control
tools/internal_row_denominator.py                      # denominator bound
```

**Follow-up, with a named addressee:** folding these columns into
`runtime_census.py` so no future wave rediscovers this is handed to the **Wave 24
integrator**, once lane ALLOC has landed and the file is free. It is not handed
to "the next lane that touches the census" — that addressee never fires.

---

## 3. WHAT THE CONTROLS CAUGHT

Every screen has a positive control and they are committed, not described
(rule 9). All 19 pass now. Three caught real defects on the way.

### 3.1 `.pdata` presence is NOT "is a function" — 52.9% of this DLL's functions have none

The first `PDATA finds a function` control **FAILED**. It handed the screen the
lowest `.text` export and got "no `.pdata`". That was not a dead parser; it was
the truth:

```
text exports: 4632, without any .pdata entry: 2449 (52.9%)
  0x180001000 ?_TypeName@Achievement@1@CAPEBDXZ   488d05d9341500 c3   <- LEA/RET, a leaf
```

**Over half of Runtime-x64.dll's exported functions are leaf functions with no
unwind data at all.** Any instrument that reads "has `.pdata`" as "is a
function" misses half the binary. The control was wrong, the screen was right,
and the fix was a second screen (PADDING) for the leaf case — which is what
classified `0x18007f8a8` and `0x1800c3db0`, the latter a function **Ghidra has
no function object for at all**.

### 3.2 The `.pdata` lower-bound trap bites the OPPOSITE way round from how it is recorded

The recorded lesson is *"a `.pdata` entry is a LOWER bound on function extent —
MSVC chains fragments via `UNW_FLAG_CHAININFO`"*. That is true and it is not the
part that bites. Observed in this binary:

```
=== 0x180078ad0   Ghidra body = 265 bytes
   pdata 0x180078ad0-0x180078add ( 13 bytes) chained=False   <- the ENTRY
   pdata 0x180078add-0x180078bd7 (250 bytes) chained=True    <- chains BACK to it
   pdata 0x180078bd7-0x180078bd9 (  2 bytes) chained=True
```

**The entry fragment is precisely the one whose chained flag reads `False`.**
Look a function up by its own address and you land on the entry, read
`chained=no`, and conclude the 13 bytes are the whole thing. A walker that
follows chains *upward* from the address you asked about therefore never widens
anything **for the only case that matters**. My first implementation did exactly
that and reported 13 bytes for a 265-byte function.

The extent of a function is the entry fragment **unioned with every fragment
that chains back to it**. Fixed; the control now hands the screen two addresses
whose true extent is known from an independent instrument (Ghidra's function
body) and checks the number:

```
PDATA understatement 0x180078ad0  PASS  walker says 265 bytes, Ghidra body is 265
PDATA understatement 0x180052640  PASS  walker says 186 bytes, Ghidra body is 186
PDATA no false widening           PASS  single-fragment fn stays 99 bytes   <- the mirror
PDATA fragmented population       PASS  384 functions split across >1 fragment (932 fragments)
```

Scale of the error on this lane's own rows:

| row | fragment-only | true extent | understated by |
|---|---:|---:|---:|
| `0x180078ad0` | 13 B | 265 B | **20.4x** |
| `0x180052640` | 13 B | 186 B | **14.3x** |
| `0x18007f050` | 80 B | 743 B | **9.3x** |

3 of 42. **Understating makes work look cheap**, and the direction of the error
is the dangerous one.

### 3.3 The RTTI vtable screen is DEAD in this binary — and a dead screen returns 0

I added a CompleteObjectLocator test as a better vtable screen than "3+ relocated
`.text` pointers". Its control failed: `0 of 147 exported ??_7 tables carry a
locator`. Probing further:

```
exported ??_7 tables: 147
with ANY relocated qword at table-8: 75      <- and those are the PREVIOUS table's last slot
'.?AV' type-descriptor signatures in the whole image: 12     ('.?AU': 0)
```

**Runtime-x64.dll is built `/GR-` — RTTI is off.** The dozen `.?AV` strings are
CRT exception machinery, not a class hierarchy. The screen has **no positive
case in this binary**, so it can only ever return 0 — and a 0 from a dead screen
reads exactly like *"none of these addresses are vtables"*. It is kept as a
column (it costs nothing and would fire on a binary that has RTTI) but it is
**barred from driving a verdict**, and the control now asserts its
inapplicability rather than its success.

*Consequence for other lanes:* **no vtable-identification work anywhere in
Runtime-x64.dll can use RTTI.** Length-of-run plus target-section is the only
structural screen available.

### 3.4 The EXPORT screen returned 0/42 — and that negative is earned

"None of the 42 has an export name" is an absence claim, and the `AEAA means
PRIVATE, not unexported` trap says such claims are often false. Three controls:

- **generic**: a known export RVA resolves; the same RVA +3 does not. PASS.
- **population-matched**: the 141 **ported** address-keyed census rows —
  `0 of 141` have export names either. This produced **no positive case**, so it
  does *not* carry the screen. Reported as a failed control, not as support.
- **handed-in, in-population** (`tools/internal_row_controls_handed.py`): take
  every census row keyed by **name**, look its address up, and push that address
  through the *same* `classify()` path an internal row takes:

```
HANDED-IN control: 1125 census EXPORT rows pushed through the ADDRESS path
  screen recovered the correct name: 1125  (100.0%)   misses: 0
NEGATIVE control: 0 of 2000 addresses shifted +3 produced a name  (expected 0)
```

The screen is live on this exact population at 100% recall and 0 false
positives. **So the `AEAA` trap does not bite here: all 183 address-keyed
census rows (42 unported + 141 ported) are genuinely unexported.** Our tree's
address doors are address doors because there is no name, not because someone
mistook `AEAA` for "unexported".

---

## 4. THE 46 ROWS

Legend — `verdict` is `internal_row_classify.py`'s; `identified as` is what our
own tree's call sites already say, with the file that says it; `hash` is
Ghidra's address-normalised opcode hash (equal hash = **byte-identical body**).

### 4A. The `ordinal` rows — 3 phantoms and 1 misfiling

Ordinals resolve to names straight out of the export table. This is not a guess
and it costs one table lookup.

| ordinal | resolves to | verdict | status |
|---|---|---|---|
| 443 | `??0Thread@System@@QEAA@I@Z` | CODE/EXPORTED | **PHANTOM** — an `export` row for this name exists with `ported=1` |
| 3214 | `?Lock@Device@Video@@QEAAXXZ` | CODE/EXPORTED | **PHANTOM** — `export` row, `ported=1` |
| 4462 | `?Unlock@Device@Video@@QEAAXXZ` | CODE/EXPORTED | **PHANTOM** — `export` row, `ported=1` |
| 2990 | `?GetTypeIdForFileExtension@Factory@Object@@QEBAIPEBD@Z` | CODE/EXPORTED | **REAL** — no other row; belongs in **Object**, and is a BODY |

Verified by `grep` on the CSV: each of the first three has exactly one `export`
row carrying `ported=1`.

**Effect on the board: 177 gap rows → 174. `ordinal` → 0. `Object` 10 → 11.**
This is the inflating direction. It is three rows, not a crisis, but it is
exactly the class of defect this lane exists to find, and it is free to fix: the
census already parses the export table.

### 4B. The 30 `.text` rows — 26 distinct bodies

Four rows are **byte-identical duplicates** of another row (MSVC emitted the
same COMDAT twice and the linker did not fold it). Proved by hash equality in
the DLL, **independently of our source comments**:

- `089ee807…` 32 insns / 127 B — **`0x18002ac40` ≡ `0x180039280` ≡ `0x180039360`**
- `4645ec38…` 73 insns / 255 B — **`0x18002f070` ≡ `0x1800492d0`**
- `6485cec7…` 234 insns / 818 B — **`0x18002f170` ≡ `0x1800493d0`**

| addr | extent | hash | identified as (our tree) |
|---|---:|---|---|
| **B2 — `Collection<T>` instantiations: spec entries, not bodies** ||||
| `0x180009c30` | 98 B | `ceb130` | AVL `SetNodeFree` — `collection/avl_trees.cpp:65,610,1164` |
| `0x180010f00` | 257 B | `7d1480` | destroy a range of `GraphicsInterface` — `video/video_resources.cpp:236`; **`DestroySpec`s already written** at :186,:188 |
| `0x18002a930` | 115 B | `7bb61a` | `~Array` clear-on-null — `io/persistence_saves_win.cpp:49` |
| `0x18002ac40` | 127 B | `089ee8` | `~Array` (Persistence+0x80) — `save/saveload_update.cpp:1329` |
| `0x180039280` | 127 B | `089ee8` | ≡ above. `~Array` on Dialog+0x48 — `ui/dialog_ctor_dtor.cpp:127` |
| `0x180039360` | 127 B | `089ee8` | ≡ above. `~Array` on Dialog+0x58 — `ui/dialog_ctor_dtor.cpp:126` |
| `0x18002c6a0` | 417 B | `255cd1` | `Array<ExternalArrayElement>::SetSize` — `object/external_load.cpp:147` |
| `0x18002f070` | 255 B | `4645ec` | `Array<ExternalPtr<External>>::SetSize` stride 8 — `ui/dialog_vtable.cpp:177` |
| `0x1800492d0` | 255 B | `4645ec` | ≡ above. `Array<ExternalPtr<ElementDescription>>::SetSize` — `ui/dialog_vtable.cpp:176` |
| `0x1800332d0` | 328 B | `644b4f` | `Array<SaveInfo>::SetSize(n, const T&)` — `save/saveload_update.cpp:1310` |
| `0x180034010` | 376 B | `01ac25` | `Array<ModInfo>::SetSize` stride 0x20 — `save/saveload_meta.cpp:267`, `types/collection/Collection.h:114` |
| `0x18004bde0` | 381 B | `8d92c2` | `Array<…>::SetSize` stride 0x28 — `types/ui/SpriteCapture.h:37,63,97` |
| `0x180053f00` | 369 B | `ddbf39` | `Array<Array<uint8>>::SetSize` — `video/imagebuffer_platform_serialize.cpp:92` |
| `0x180075b20` | 369 B | `12a630` | `Array<DialogControllerConfigEntry>::SetSize` stride 0x20 — `ui/dialog_vtable.cpp:175` |
| `0x180075d20` | 349 B | `689a90` | `Array<DialogNamedTemplate>::SetSize` stride 0x10 — `ui/dialog_vtable.cpp:174` |
| `0x180078950` | 381 B | `c11b90` | `Array<SpriteSheet::SpriteInfo>::SetSize` stride 0x38 — `ui/spritesheet_vtable.cpp:397` |
| **B3 — element-serialize walkers: one shape, 3 T's** ||||
| `0x18002f170` | 818 B | `6485ce` | `_references` element walker — `ui/dialog_vtable.cpp:182` |
| `0x1800493d0` | 818 B | `6485ce` | ≡ above. `_templates` element walker — `ui/dialog_vtable.cpp:181` |
| `0x180078ad0` | **265 B** | `9328a5` | per-`SpriteInfo` record walker — `ui/spritesheet_vtable.cpp:405`. **extent understated 20.4x by `.pdata`** |
| `0x180052640` | **186 B** | `369cc2` | Audio bulk element serialize — `audio/audio.cpp:739`. **understated 14.3x** |
| **B4 — REAL new engine bodies (7)** ||||
| `0x180028760` | 1381 B / 422 insns | `7e9910` | `Array<Mod>` introsort — `io/modification_refresh.cpp:77` (declined at :541). **The single largest body in this bucket.** |
| `0x1800b7390` | 188 B | `bc2a3a` | `Persistence` worker enqueue, command 2 (delete) — `io/persistence_saves.cpp:92` |
| `0x1800b7460` | 343 B | `111d50` | worker enqueue, command 1 (load) — `io/persistence_saves.cpp:97` |
| `0x1800b75c0` | 364 B | `2d7469` | worker enqueue (save) — `save/saveload_update.cpp:324` |
| `0x1800b7740` | 183 B | `291de6` | embedded worker **destructor** — `io/persistence_saves.cpp:102`. **Has a stated blocker** (joins and closes a real thread), `persistence_saves_probe.cpp:499` |
| `0x1800c3db0` | ~22 B | *(none)* | `qsort` comparator thunk — `save/saveload_update.cpp:1342`. **Ghidra has NO function object here**; my PADDING screen found it (`prev=0xCC`, 16-aligned) |
| `0x1800c3de0` | 338 B | `cb1c25` | `SaveInfo::SetThumbnail(row, px, w, h)` — `save/saveload_update.cpp:1320` |
| **B1 — statically-linked MSVC CRT, OUT OF SCOPE (3)** ||||
| `0x18007f050` | **743 B** | *Ghidra names it* `qsort` | `save/saveload_update.cpp:1334`. **understated 9.3x by `.pdata`** |
| `0x18007fbfc` | 237 B | *Ghidra names it* `_beginthreadex` | `system/thread_win.cpp:207` |
| `0x18007f8a8` | 12 B / 3 insns | `03730a` | `noreturn` `exit(-1)` thunk — `system/error.cpp:196` |

### 4C. The 12 data rows

| addr | verdict | evidence | what it is |
|---|---|---|---|
| `0x1801554a8` | DATA/VTABLE-SHAPED | 22 relocated slots, all `.text` | shared **BinaryEncoding** vtable, unexported — `io/compressedstream.cpp:491` |
| `0x1801559f8` | DATA/VTABLE-SHAPED | 12 slots, all `.text` | **Visitor encoding** vtable — `io/iobase_vtables.cpp:204` (explicitly *not* `0x1801554a8`) |
| `0x180157c60` | DATA/VTABLE-SHAPED | 7 slots, all `.text` | a Video vtable — `engine/singleton_ctors.cpp:692,720` |
| `0x180155860` | DATA/POINTER *(see below)* | **2** slots → `0x1800b7260`, `0x18000c590`, both `.text` | **Persistence's worker vtable override** at `this+0x10` — `types/io/Persistence.h:88` |
| `0x18014bc20` | DATA/SCALAR-OR-STRUCT | header reads `dwSize=32 dwObjSize=24 dwFlags=2 dwDataSize=256 dwNumObjs=256`; `rgodf` relocated at **+0x18** | **`c_dfDIKeyboard`** — a `DIDATAFORMAT` from the DirectInput SDK. **Not port work.** |
| `0x18014be30` | DATA/SCALAR-OR-STRUCT | `dwDataSize=20 dwNumObjs=11` (3 axes + 8 buttons = `DIMOUSESTATE2`) | **`c_dfDIMouse2`** — confirms `input_platform_startup_win.cpp:62`'s reading, *not* `c_dfDIMouse`. **Not port work.** |
| `0x1801557e8` | DATA/SCALAR-OR-STRUCT | bytes `53 00 52 00 53 00 00 00` | UTF-16 `"SRS"`, the `GetTempFileNameW` prefix — `io/filesystem_createfile_win.cpp:301` |
| `0x180161780` | DATA/SCALAR-OR-STRUCT | bytes `06 00 00 00`, then zeros | `WNDCLASS.hbrBackground` = 6 — `application/platform_startup_win.cpp:285` |
| `0x1801fc6d0` | DATA/SCALAR-OR-STRUCT | `.data`, zero at rest, no relocs | the Modification DLL `HMODULE` slot — `engine/engine_lifecycle.cpp:143` |
| `0x1801fe7c0` | DATA/SCALAR-OR-STRUCT | `.data`, zero at rest | `_loader.m_flags` — `types/engine/UpdateLoop.h:150` |
| `0x1801fe7e0` | DATA/SCALAR-OR-STRUCT | `.data`, zero at rest | `_loader`'s `Thread::hFinished` (`_loader+0x28`) — `engine/funcs/engine_funcs.h:384` |
| `0x1801fba28` | DATA/SCALAR-OR-STRUCT | `.data`, zero at rest | CRT "a locale has been selected" flag — `crt/msvc_stricmp.cpp:66`. **CRT.** |

**Known limitation, stated rather than hidden.** `0x180155860` is a genuine
**2-entry vtable** and my `>= 3 slots` threshold calls it `DATA/POINTER`. I did
not raise the threshold's sensitivity, because at 2 slots "vtable" and "pointer
pair" are not separable by length and the structural test that *would* separate
them (RTTI) is dead in this binary (§3.3). Likewise `DATA/SCALAR-OR-STRUCT`
asserts only that **slot +0 is not a relocated pointer** — a struct whose first
field is scalar and whose later fields are pointers (both `DIDATAFORMAT` rows,
relocated at +0x18) lands there too. The verdict is renamed to say so.

---

## 5. THE PROFILE DLL — the route the kickoff assumed does not exist

The kickoff said `Runtime-x64-profile.dll` (5,182 exports, ToolKit build) is
*"the project's primary source for naming unexported Runtime internals"* and
that *"many of these may have real names there."* **For this population it
supplies nothing, and the reason is structural, not a shortage of effort.**

- It is on disk (`ToolKit/bin/x64/Runtime-x64-profile.dll`) and open in Ghidra
  (16,002 functions). Not a missing-file problem.
- It is a **different build at different addresses**. Recovering a name for
  retail `0x180078ad0` requires *matching* the body into the profile build
  first. There is no address identity to exploit.
- **All 30 of these are unexported in retail.** The profile build's extra ~550
  exports would have to include the profile counterpart of *this* body, and I
  have no evidence any of them do.
- Matching by opcode hash across the two builds is the sound method and it is a
  **tool this lane did not have time to build** — `get_bulk_function_hashes`
  paginates the whole 16,002-function binary, and `diff_functions` is **broken**
  (returns a list where the MCP wrapper declares a string; it raises a pydantic
  validation error on every call).

**This is a TODO, not a blocker** — it is buildable and it would name all 30 at
once. Handed to **Wave 24 lane INTERNAL2** (§7).

What I used instead: retail Ghidra confirms all 30 are `FUN_*` (no project
naming) except two the CRT analyser already named (`qsort`, `_beginthreadex`),
and Ghidra's body extents agreed with my chain-resolved `.pdata` extents on
**10 of 10** spot checks — an independent corroboration of §3.2's fix.

---

## 6. THE SCOPING FACT — 16 of these rows are spec entries, not bodies

Rule 2 says grep before you port. Doing so changes the size of this bucket more
than anything else in the report.

`runtime/src/collection/array.cpp:1187` already contains:

```cpp
void SetSizeCore(ArrayHdr* a, int newSize, const uint8_t* fill,
                 const ArraySpec* spec, Site* site);
```

— a **stride-parameterised, probe-verified** `Array<T>::SetSize`, driven by
`ArraySpec { name, stride, align, assign, init, destroy, byteCopy, written-extents }`
(`array_internal.h:29`). It already carries **two** such instantiations
(`0x18007b960`, `0x1800783d0`, registered at `array.cpp:1562,1595`). And
`array_destroy_internal.h:32` has the matching `DestroySpec { name, stride,
nOffs, offs[3] }` with two cores (`FreeInnerArraysCore`, `ReleaseStringsCore`).

**So the 10 `SetSize` rows and 4 `~Array` rows in §4B are 14 spec-table entries
against machinery that exists and has a probe — not 14 function bodies.** The
`0x180010f00` row already has its `DestroySpec`s written in
`video/video_resources.cpp:186,188`.

Two independent lines support this, not one premise twice:
1. our tree's call-site analysis (the `identified as` column), and
2. the fact that a **single parameterised body already reproduces two members of
   the same family** under a probe — which is a property of the DLL, not of our
   comments.

**Counter-evidence I am obliged to report:** exact-hash equality does **not**
bind the family. The two already-ported instantiations hash differently from
each other (`7e06ad`, `45150b`) and from all ten rows here. Hash equality proves
**sameness**; its absence does **not** prove difference. The family claim rests
on (1) and (2), not on hashing, and a lane acting on it should expect to write
per-`T` `assign`/`init`/`destroy` callbacks, not just a stride.

**And a negative on the same reasoning:** I checked whether
`runtime/src/collection/introsort.cpp` (which already transcribes the packfile
introsort `0x1800264a0` with a permutation probe) covers `0x180028760`. **It
does not** — 165 insns / 523 B versus 422 insns / 1381 B. `0x180028760` is a
genuinely separate, large body and stays in B4. I report this because the
analogy was tempting and wrong.

---

## 7. RESIDUALS, EACH WITH A NAMED ADDRESSEE

| # | item | addressee |
|---|---|---|
| 1 | Fold the `ordinal → export name` lookup into `runtime_census.py`. It removes 3 phantom rows and refiles 1 into `Object`; the census already parses the export table. | **Wave 24 integrator** (after lane ALLOC lands and the file is free) |
| 2 | Fold `internal_row_classify.py`'s SECTION / EXPORT / PDATA / PADDING / RELOC columns into `runtime_census.py` so `internal` stops printing as 42 UNKNOWN. | **Wave 24 integrator** |
| 3 | Build the cross-build opcode-hash matcher (retail ↔ `Runtime-x64-profile.dll`) and name all 30 at once. Buildable; §5 says why this lane did not. | **Wave 24 lane INTERNAL2** |
| 4 | **`mcp__ghidra-mcp__diff_functions` is BROKEN** — returns a list where the wrapper declares a string; every call raises a pydantic validation error. It is in the toolbox and unusable. | **Wave 24 integrator**, to file against the GhidraMCP bridge |
| 5 | Scope the **7 real bodies** in §4B/B4 as a porting lane. `0x1800b7740`'s thread-join blocker is **stated, not re-checked** — rule 3 applies to it. | **Wave 24 lane, to be named at kickoff** |
| 6 | Write the 14 `ArraySpec`/`DestroySpec` entries of §6 against the existing cores. | **Wave 24 lane, to be named at kickoff** |
| 7 | The non-literal census blind spot (TIMER's finding) is **still unmeasured**; §1 measures only the literal axis. Needs a symbol-level pass, not a grep. | **Wave 24 integrator**, to scope |

---

## 8. FACTS THIS LANE ESTABLISHED THAT OUTLIVE IT

1. **52.9% of Runtime-x64.dll's `.text` exports have no `.pdata` entry.** "No
   unwind data" is not evidence against being a function.
2. **The `.pdata` understatement trap bites through the ENTRY fragment, which
   always reads `chained=False`.** Union forward from the entry; never size from
   the fragment you landed on. Observed error up to **20.4x**. 384 functions in
   this DLL are split across more than one fragment.
3. **Runtime-x64.dll is built `/GR-`.** No RTTI, 12 `.?AV` descriptors
   image-wide, 0 of 147 exported vtables carry a CompleteObjectLocator. **No
   vtable work anywhere in this DLL can use RTTI.**
4. **All 183 address-keyed census rows are genuinely unexported** — proved by a
   screen with 100% recall and 0 false positives on 1125 in-population cases.
   The `AEAA` trap does not bite this population.
5. **The census's address population is complete** with respect to literal
   address doors in our source: 0 invisible rows.
6. **A gap bucket printed as `UNKNOWN` is not unknown to the project.** All 42
   of these are named in our own source comments. The census reads names, and a
   comment is not a name. That is the whole of the `internal` mystery.
