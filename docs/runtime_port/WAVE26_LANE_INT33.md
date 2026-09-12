# WAVE 26 LANE INT33 — the 33 address-keyed UNKNOWN gap rows

Branch `wave26/int33`, cut from `wave26/base` at `7de42dd1`. Private worktree.

> **EVERY FIGURE IN THIS DOCUMENT IS BRANCH-LOCAL**, measured in this worktree
> with the command printed beside it. The integrator must re-derive on the merged
> tip. Wave 21 published a wave number assembled from lane figures and it was
> wrong in both directions.
>
> **MEASURED and INFERRED are separated throughout.** §1–§4 are measured. §7 is
> inferred and says so.

---

## 0. HEADLINE

**Three rows ported, `called` unmoved — and the bucket's remaining mystery is now
closed by a STRUCTURAL fact rather than by a tool that does not exist.**

| | at lane start | at lane end |
|---|---:|---:|
| `called` | 1333 | **1333** (unchanged — zero new address doors) |
| `ported` | 1185 | **1188** |
| ported % | 88.9% | **89.1%** |
| gap rows | 148 | **145** |
| `internal` gap | 33 | **30** |
| UNKNOWN, `.text` half | 21 | **18** |

```
tools/runtime_census.py --namespace internal --unported     # run from THIS worktree
tools/runtime_census.py --unported
```

Two findings outlive the three ports:

1. **All 33 rows are genuinely unexported in retail — re-derived, not
   inherited**, by a screen carrying a *positive* AEAA specimen (§2). Wave 23's
   claim holds.
2. **`Runtime-x64-profile.dll` cannot name ANY of them, and the reason recorded
   for three waves was the wrong reason.** Wave 23 filed it as *"a tool this lane
   did not have time to build"* and handed it forward as a TODO twice. **I built
   the tool. It does not help, and a third tool proves no tool could:** all 362
   exports the profile build has and retail does not are debug/ToolKit symbols —
   `System::Debug`, allocator tracing, `PickingDebug`/`PathfindingDebug`, the FBX
   and RSC compilers, and 105 debug `operator new` overloads. **Zero are
   `Collection`/`ExternalPtr` template instantiations**, which is what 19 of the
   21 `.text` rows are. The route is closed structurally, not for want of
   tooling (§3).

---

## 1. THE POPULATION, RE-DERIVED

```
tools/runtime_census.py --namespace internal --unported
  internal   195 called / 162 ported   gap 33
  .text 21 · .rdata 8 · .data 4
```

33 addresses, listed in full in §5. The section split is re-derived independently
in §2 from the PE section table and agrees exactly.

### 1.1 Why the count moved 42 → 33, and what ENTERED

Wave 25 lane INTERNAL42 ported 10 and left 32; Wave 24's four entrants and one
Wave 25 entrant churned the rest. **One row in my 33 appears in NO predecessor
lane's classification table**, and finding it was the point of re-deriving
membership rather than inheriting it:

| | |
|---|---|
| **`0x1800269a0`** | entered from `runtime/src/io/package_file_dtor.cpp` (Wave 25 lane SMALLBODY's `~PackageFile`). Wave 25's residual list does not mention it. |

**Fourth consecutive wave in which this bucket refilled from another lane's
faithful port** (Wave 23 → 4, Wave 24 → 2, Wave 25 → 1, Wave 26 → 1). Wave 25
called this structural. It is, and the mechanism is exact: a faithful port makes
its callees ours, and an unexported callee lands in `internal`.

---

## 2. ARE THEY REALLY UNEXPORTED? — RE-DERIVED, WITH A LIVE POSITIVE

`tools/int33_export_screen.py` (new, this lane). **4/4 controls pass.**

```
retail export directory: 4926 named exports
SUBJECT: 33 address-keyed rows
  rows WITH an export name: 0 of 33
  .text 21 · .rdata 8 · .data 4        <- re-derived from the PE section table

CONTROLS
  PASS POS 400 known export VAs are named by this lookup      400/400
  PASS POS a PRIVATE ('0...A') datum IS found as an export
         ?_typeInfo@SpriteSheet@UI@@0VBaseTypeInfo@Object@@A
  PASS NEG 400 export VAs shifted +3 resolve to nothing        0 unexpected hits
  PASS FLOOR the directory holds >= 4000 named exports         4926
```

**The second positive control is the one that matters and it is not decorative.**
`AEAA`/`0…A` mean PRIVATE, not unexported, and this DLL exports its privates —
three silent address doors survived review on exactly that confusion. Handing the
screen a private static that IS exported is what separates "these 33 have no
name" from "this lookup cannot see private symbols". Without it, "0 of 33" and a
dead parser print the same line.

**This re-derives Wave 23's finding on a different instrument and agrees.** I did
not inherit it.

### 2.1 The 12 data rows, re-derived from the relocation directory

`scratchpad/datarows.py` walked the DIR64 base-relocation set (3,183 entries) and
measured each data row's run of relocated qwords and their target section. **Every
number Wave 23 §4C published is reproduced:** 22 slots at `0x1801554a8`, 12 at
`0x1801559f8`, 7 at `0x180157c60`, 2 at `0x180155860`, all into `.text`; the two
`DIDATAFORMAT` headers read `dwSize=0x20 dwObjSize=0x18 dwFlags=2` with
`dwDataSize/dwNumObjs` = `0x100/0x100` (keyboard) and `0x14/0x0b` (`DIMOUSESTATE2`),
and both carry their `rgodf` pointer relocated at **+0x18**, not +0x00 — which is
why a reloc-run measured from +0 reads zero for them and why Wave 23's
`DATA/SCALAR-OR-STRUCT` verdict was correctly hedged.

**One refinement on Wave 23.** It recorded the four `.data` rows as *"zero at
rest, no relocs"*. Measured here: `.data` has `VirtualSize 0xa998` but
`SizeOfRawData 0x5400`, so raw file bytes end at `0x1801fb400` and **all four rows
lie past it**. They are not "zero at rest" — they have no file bytes at all. They
are BSS-style zero-initialised statics, which is a stronger and more useful
statement: there is nothing to mint, only storage to provide.

---

## 3. THE PROFILE BUILD — A STALE BLOCKER RE-CHECKED, AND THE RIGHT ANSWER FOUND

**This is the largest finding of the lane.** The brief named the profile DLL as
*"exactly what it is for"*. Three waves have handed the question forward:

* Wave 23 §5: *"For this population it supplies nothing, and the reason is
  structural"* — then gave a **tooling** reason (*"a different build at different
  addresses… matching by opcode hash is the sound method and it is a tool this
  lane did not have time to build. This is a TODO, not a blocker"*) and addressed
  it to **Wave 24 lane INTERNAL2**.
* Wave 24 and Wave 25 did not take it.

The project's recorded rule is that a stated blocker must be re-checked, not
inherited. Re-checked, on three instruments:

### 3.1 The file is not where the record says it is (MEASURED)

`C:\GOG Games\Banished\ToolKit\bin\x64\Runtime-x64-profile.dll` — the path in
`WAVE23_LANE_INTERNAL.md` — **does not exist**; there is no `ToolKit` directory
under the game at all. The DLL is at
`C:\code\GIT\untitled_rev_eng_project\ToolKit\bin\x64\Runtime-x64-profile.dll`
(3,188,224 bytes, 5,182 exports). A lane that took the residual at face value
would have spent its first hour on a missing file.

### 3.2 I built the matcher. It is too weak to carry a negative (MEASURED)

`tools/int33_profile_match.py`, two independent matchers, each with a **recall
control on bodies whose answer is already known**:

| matcher | what it compares | recall on retail `.text` exports | verdict on the 21 rows |
|---|---|---:|---|
| masked-byte | body bytes with every rel32/RIP-disp wildcarded | **6/60 = 10.0%** | 0 matched |
| mnemonic-sequence | opcodes only, all operands discarded (Wave 23's "opcode hash") | **64/200 = 32.0%** | 0 matched |

**`0 of 21` is exactly what a 10%-recall matcher prints on any population.** The
control is what stops that number being published as a finding. Reported as a
measured limitation, not as evidence.

### 3.3 The recall-free screen — and this one IS decisive (MEASURED)

`tools/int33_profile_match.py --population` does not match anything. It asks what
the profile build's extra exports **are**:

```
retail-only export names : 106
profile-only export names: 362
    132  ToolKit-only resource compiler / text IO
    105  debug operator new/delete taking (file, line)
     45  ToolKit-only Utility:: (FBX / mesh / image compile)
     38  IO codec slots that retail has but does not EXPORT
     36  ToolKit-only debug / assert / dev helpers
      6  allocation TRACING (debug-build allocator)
      0  UNCLASSIFIED

  profile-only exports that are Collection/ExternalPtr template
  instantiations -- the shape 19 of the 21 .text rows are: 4
```

and all **4** of those are functions that merely *take* an `Array` as a
**parameter** (`?GetSkinData@FBXMeshCompile@Utility@@…AEAV?$Array@VVector4@Math@@…`,
`?DrawClippedTriangles@PickingDebug@Debug@@…`, two more). **Not one is an
`Array<T>::SetSize`, a `~Array<T>`, or an `ExternalPtr<T>::Serialize`.**

> **THE CONCLUSION, AND WHY IT IS DIFFERENT IN KIND FROM WAVE 23'S.**
> The profile build cannot name these rows **not because they are at different
> addresses and not because we lack a matcher, but because the extra exports are
> a different POPULATION.** A perfect matcher at 100% recall would return the
> same zero. Wave 23's verdict was right; its stated reason sent two waves
> looking for a tool, and the tool — now written, committed, and controlled — is
> not what was missing.
>
> **RESIDUAL 3 OF WAVE 23 IS DISCHARGED. Nobody should carry it forward again.**

### 3.4 One thing the profile build CAN still do, handed forward with an addressee

The 38 "IO codec slots that retail has but does not EXPORT" are exactly
`?BeginArray@Visitor@IO@@`, `?Serialize@BinaryWriter@IO@@`, `?EndBlock@…` and
their siblings — **named bodies for the slots of the three unexported encoding
vtables in this very bucket** (`0x1801554a8` 22 slots, `0x1801559f8` 12 slots).
That is a real, unexploited lead for the *data* half of the bucket.

**It carries a known trap and the trap is already recorded:**
`reference_encoding_vtable_offsets_differ_between_builds.md` — the IO Encoding
vtable OFFSETS DIFFER between retail and the profile build (+0x78/98/a0 vs
+0x90/b0/b8). So the profile build can supply the **names of the slot bodies**
but **not** their **slot indices**. Anyone using it must take order from retail.

**ADDRESSEE: a Wave 27 lane scoped on the IO encoding vtables** (see §7, residual
#2). Not "whoever touches IO next" — that addressee never fires.

---

## 4. WHAT THIS LANE PORTED — 3 ROWS, ZERO NEW DOORS

`called` is **1333 before and after**. Declared, and the census confirms it rather
than the author asserting it. Every callee these three need was already ours.

### 4.1 `0x1800269a0` — `~Array<IO::PackageFile::Entry>` (the unclassified entrant)

`runtime/src/collection/array_destroy.cpp`, one `J2_ENTRY` line + one
registration.

**MEASURED** with `tools/int33_normdiff.py` (new, 9/9 controls):

```
0x1800269a0 vs 0x18002ac40   extent 127/127   raw byte diff 13
  the ONLY normalised difference:  +0x23 CALL 0x180026b50  vs  CALL 0x18002afc0
--calls 0x1800269a0
  +0x23 CALL 0x180026b50    the element-destroy helper
  +0x46 CALL 0x18014b390 }
  +0x57 CALL 0x1800c6c00 }  the allocator Enter / Free / Leave triple,
  +0x60 CALL 0x18014b388 }  at the SAME offsets as registered J2 0x180010ab0
```

`0x180026b50` is **already ported and already registered** in that same file as
`RtColl_DestroyStrings_180026b50` under `kD_Str_10`, and its `B_ENTRY` line
already names the element as `IO::PackageFile::Entry`. So this is an entry point
for machinery that exists — **no new body, no new `DestroySpec`**.

**The spec is read from this body's own CALL operand, never from a hash.** Wave
25 paid for that rule: three siblings share Wave 23's address-normalised hash
`089ee8` and take two different specs, because a hash erases the one operand that
decides the answer. My tool's controls encode the rule — all three
hash-`089ee8` pairs must come back DIFFERENT, and they do.

**A stale claim corrected as a side effect.** My positive control was first
written against `0x18002ac40` / `0x180039360` on the strength of Wave 23's *"ONE
body, hash 089ee8"* and **failed**. The tool was right: those two call
0x18002afc0 and 0x180039640 respectively. The "one body" language has been
repeated by two waves; the control is now on a genuinely identical pair
(`0x18002f070` / `0x1800492d0`, raw diff 18 bytes, normalised diff 0) and the
correction is written into the tool where the next reader meets it.

### 4.2 `0x180004590` — `ExternalPtr<UI::SpriteSheet>::Serialize` (a SELF-CHECK becomes an ORACLE)

`runtime/src/object/external_ptr_serialize.cpp`, one `EXTPTR_THUNK` + one
registration. The **fifth** of that file's 25-member family.

Wave 10 left 21 of the 25 deliberately unregistered, and its reason was
**specific and checkable, not blanket**: *"the oracle route below does not cover
them."* For this one that is no longer true:

* Wave 23 lane ACHIEVE built a **FULL** write-path probe (`stopAfter = 0`,
  everything compared) for `?Serialize@Achievement@1@UEAA_NAEAVStream@IO@@@Z`
  (`0x18009e070`).
* That body CALLs `0x180004590` over `+0x38` at `0x18009e0e6`, and
  `achievement_serialize.cpp:156` reaches it through
  `ResolveAddr(Module::Runtime, 0x180004590)` — **registry-first**.
* So **before this line the probe's OURS side and its THEIRS side both ran
  retail's copy of this body and compared retail with itself.** That is
  `reference_registering_an_address_converts_a_selfcheck_into_an_oracle`, where
  exactly this went undetected for five waves.

**MEASURED, on this tip, two independent readings:**

```
python tools/extptr_family_sweep.py      # re-run, not quoted from the Wave 10 table
  0x180004590 -> 0x1801fadd0 ?_typeInfo@SpriteSheet@UI@@0VBaseTypeInfo@Object@@A
pefile, straight out of the export directory
  ?_typeInfo@SpriteSheet@UI@@0VBaseTypeInfo@Object@@A -> 0x1801fadd0
```

**Zero cutover-surface growth.** The type-info symbol is a plain EXPORT
(`…0VBaseTypeInfo…A` is *private*, not unexported), so `RT_DATA_SLOT` opens a
NAME door, not an address door.

### 4.3 `0x180010f00` — `Video::Device` per-`GraphicsInterface` element destroy

`runtime/src/video/video_resources.cpp`: a retail-ABI entry point +
registration. **This is Wave 25 residual #3, whose addressee was "Wave 26, a
named lane". This is that lane.**

The body has existed and been **oracled** since Wave 18 (`DestroyInterfaceRange`,
driven by `video_resources_probe.cpp` case 3 against the shipped body). Only the
registration was missing. The file's own banner said these walkers were
deliberately unregistered because *"nothing in this tree resolves them"* — **and
that clause stopped being true when the probe landed**, which is why the census
has carried it as a `called`-but-unported row ever since.

Signature read from `0x180010f00`'s own prologue, not from the C++ shape:
`TEST R8D,R8D / JLE` → R8D is `count`; `MOV [RSP+8],RCX` → RCX is spilled and
**never read**; RDX is the element base.

**The mirror trap, checked rather than assumed.** The probe's THEIRS side reaches
the shipped body through `ResolveRuntimeRaw(0x180010f00)` — the RAW door, which
does **not** consult the port registry — so it keeps calling the DLL and the
oracle survives this registration. Had it used `ResolveAddr(Module::Runtime, …)`,
this change would have converted a working oracle into a self-check.

**A pre-existing duplication found while doing this, NOT resolved by this lane.**
`0x180010da0` (the nested per-Adapter walker) has **TWO independent ports** in
this tree: `video_resources.cpp::DestroyAdapterRange` (composed from `rtcoll`
specs) and `device_enum_types.cpp::RtVideo_ArrayAdapterElemDestroy` (open-coded).
**The open-coded one is the registered one** (`device_enum_types.cpp:632`), while
my newly-registered `0x180010f00` calls the *other* one directly, exactly as
retail's rel32 does. Retail-faithful, and two bodies that can drift. See §7
residual #3.

### 4.4 Build and checks

```
mingw32-make standalone-debug        CLEAN, no new warnings
tools/int33_normdiff.py --selftest    9/9 controls pass
tools/int33_export_screen.py          4/4 controls pass
```

---

## 5. THE FULL TABLE — all 33, address → what it is → verdict → addressee

`verdict` legend: **PORTED** this lane · **BODY** a function to reimplement ·
**SPEC** a registration against machinery we already own · **DATA** mint/storage
work · **NOT PORT WORK** · **CRT** out of scope.

All "what it is" entries are MEASURED unless marked *(inferred)*.

| # | address | sect | what it is | verdict | addressee |
|---:|---|---|---|---|---|
| 1 | `0x180004590` | .text | `ExternalPtr<UI::SpriteSheet>::Serialize` (family sweep, re-run) | **PORTED** §4.2 | — |
| 2 | `0x1800269a0` | .text | `~Array<PackageFile::Entry>`, J2 + `kD_Str_10` | **PORTED** §4.1 | — |
| 3 | `0x180010f00` | .text | `Device` per-`GraphicsInterface` destroy | **PORTED** §4.3 | — |
| 4 | `0x180005220` | .text | serialize N `ExternalPtr<Goal>`, 835 B, 17 callees | **BODY** | W27 lane ACH |
| 5 | `0x180009a20` | .text | serialize N `InternalPtr<AchievementInstance>`, 195 B, ONE callee `0x1800c0180` | **BODY** (smallest) | W27 lane ACH |
| 6 | `0x18002f170` | .text | `_references` element walker, 843 B | **BODY** | W27 lane UIWALK |
| 7 | `0x1800493d0` | .text | `_templates` element walker, 840 B (≢ #6, extents differ) | **BODY** | W27 lane UIWALK |
| 8 | `0x180078ad0` | .text | per-`SpriteInfo` record walker, 285 B | **BODY** | W27 lane UIWALK |
| 9 | `0x180052640` | .text | Audio bulk element serialize, 352 B | **BODY** | W27 lane UIWALK |
| 10 | `0x180028760` | .text | `Array<Mod>` introsort, 1381 B — the single largest | **BODY** | W27, see §7 #1 |
| 11 | `0x18002c6a0` | .text | `Array<ExternalArrayElement>::SetSize`, assign `0x1800067d0`, destroy `0x180006ac0` | **SPEC** | W27 lane ARRSPEC |
| 12 | `0x1800332d0` | .text | `Array<SaveInfo>::SetSize`, assign `0x180032f70`, destroy `0x1800c3f40` | **SPEC** | W27 lane ARRSPEC |
| 13 | `0x180034010` | .text | `Array<ModInfo>::SetSize`, helper `0x1800341e0` | **SPEC** | W27 lane ARRSPEC |
| 14 | `0x18004bde0` | .text | `Array<…>::SetSize` stride 0x28, helper `0x18003f070` | **SPEC** | W27 lane ARRSPEC |
| 15 | `0x180053f00` | .text | `Array<Array<uint8>>::SetSize`, helpers `0x180011220`, `0x18004ff80` | **SPEC** | W27 lane ARRSPEC |
| 16 | `0x180075b20` | .text | `Array<DialogControllerConfigEntry>::SetSize`, destroy `0x180039580` = **`kD_Str_20at18`, already written** | **SPEC — cheapest of the eight** | W27 lane ARRSPEC |
| 17 | `0x180075d20` | .text | `Array<DialogNamedTemplate>::SetSize`, destroy `0x180039640` = **`kD_Str_10at08`, already written** | **SPEC — cheapest of the eight** | W27 lane ARRSPEC |
| 18 | `0x180078950` | .text | `Array<SpriteSheet::SpriteInfo>::SetSize`; assign `0x180004220` **and** destroy `0x1800059e0` are **BOTH already ported AND registered** | **SPEC — cheapest of all 33** | W27 lane ARRSPEC |
| 19 | `0x18007f050` | .text | MSVC `qsort`, 743 B | **CRT — out of scope** | nobody |
| 20 | `0x18007f8a8` | .text | `noreturn` `exit(-1)` thunk | **CRT — out of scope** | nobody |
| 21 | `0x18007fbfc` | .text | MSVC `_beginthreadex`, 237 B | **CRT — out of scope** | nobody |
| 22 | `0x1801554a8` | .rdata | **vtable**, 22 relocated slots → `.text` (BinaryEncoding) | **DATA** | W27 lane IOVT, §3.4 |
| 23 | `0x1801559f8` | .rdata | **vtable**, 12 slots → `.text` (Visitor encoding) | **DATA** | W27 lane IOVT, §3.4 |
| 24 | `0x180157c60` | .rdata | **vtable**, 7 slots → `.text` (a Video vtable) | **DATA** | W27 lane IOVT |
| 25 | `0x180155860` | .rdata | **2-slot vtable** → `0x1800b7260`, `0x18000c590` (Persistence worker override at `this+0x10`) | **DATA** | W27 lane IOVT |
| 26 | `0x18014bc20` | .rdata | `c_dfDIKeyboard` — `dwSize=0x20 dwObjSize=0x18 dwFlags=2 dwDataSize=0x100 dwNumObjs=0x100`, `rgodf` relocated at +0x18 | **NOT PORT WORK** (platform SDK) | nobody |
| 27 | `0x18014be30` | .rdata | `c_dfDIMouse2` — `dwDataSize=0x14 dwNumObjs=0x0b` = `DIMOUSESTATE2` | **NOT PORT WORK** (platform SDK) | nobody |
| 28 | `0x1801557e8` | .rdata | UTF-16 `"SRS"` (`53 00 52 00 53 00 00 00`), the `GetTempFileNameW` prefix | **NOT PORT WORK** (literal) | nobody |
| 29 | `0x180161780` | .rdata | `06 00 00 00` — `WNDCLASS.hbrBackground` | **NOT PORT WORK** (literal) | nobody |
| 30 | `0x1801fba28` | .data | CRT "a locale has been selected" flag — **past `SizeOfRawData`, BSS-style** | **CRT / storage only** | nobody |
| 31 | `0x1801fc6d0` | .data | Modification DLL `HMODULE` slot — BSS-style | **DATA (storage only)** | W27 lane IOVT |
| 32 | `0x1801fe7c0` | .data | `_loader.m_flags` — BSS-style | **DATA (storage only)** | W27 lane IOVT |
| 33 | `0x1801fe7e0` | .data | `_loader`'s `Thread::hFinished` (`_loader+0x28`) — BSS-style | **DATA (storage only)** | W27 lane IOVT |

### 5.1 The decomposition, so the next wave does not re-derive it

| class | rows | is it a body to write? |
|---|---:|---|
| **PORTED this lane** | **3** | done |
| BODY — a real function to reimplement | **7** | **yes** |
| SPEC — a registration against cores we already own | 8 | no |
| DATA — vtables + storage | 8 | no |
| NOT PORT WORK — SDK blobs, literals | 4 | no |
| CRT — out of scope | 3 | no |

> **The 33-row bucket contains SEVEN functions to write.** Not 33, and not the
> 21 the `.text` hint suggests. A lane scoped on "33 unknown rows" is scoped
> against a population that does not exist — the same shape the Wave 26 kickoff
> flags for the 148.

### 5.2 The three cheapest rows left, each with the fact that makes it cheap

Named because the kickoff's own §4 warns that a cheap-wins list is the stalest
section of any handover. **Each of these carries the command that re-derives it**,
so a Wave 27 lane checks rather than trusts:

```
python tools/int33_normdiff.py --map 0x180078950 0x180075d20 0x180075b20
```

| row | why cheap | re-check this |
|---|---|---|
| `0x180078950` | its assign `0x180004220` **and** its destroy `0x1800059e0` are both already registered in `array_destroy.cpp` | that both registrations are still live |
| `0x180075d20` | destroy `0x180039640` == `kD_Str_10at08`, written Wave 25 | that the spec row still exists |
| `0x180075b20` | destroy `0x180039580` == `kD_Str_20at18`, written Wave 25 | ditto |

All three still need their `assign` / `init` arms and their **per-arm written
extents** read out of their own bodies (Wave 25 §5.1's recipe). **"Cheap" here
means the destroy half is free, not that the row is free.**

---

## 6. WHAT I DID NOT ESTABLISH — plainly

1. **I RAN NO GAME.** No `game_dir_lock.sh` acquire, no exe md5 check, because
   there was no launch to guard. **Nothing in this document claims any of these
   three registrations has ever executed.** There is no `calls=` column, no
   attendance figure and no divergence count for any of them. This is the single
   largest gap in the lane and it is named first.
   * `0x180004590` and `0x180010f00` each sit on an **existing oracle route**
     (§4.2, §4.3), so the instrument to prove they run exists and was not run.
     `0x180004590`'s is `BANISHED_RT_ACHMGR`; `0x180010f00`'s is the
     `video_resources` probe.
   * `0x1800269a0` is reached from `~PackageFile`, which
     `package_file_dtor.cpp` §3 already probes. Same statement: not run here.
2. **I wrote no new oracle.** All three rows route into bodies that already carry
   probes (`ReleaseStringsCore`/`ArrayDtorImpl`, the ExternalPtr body,
   `DestroyInterfaceRange`). **They therefore inherit whatever those probes do and
   do not cover, and I did not re-audit that coverage.**
3. **The `0x180010da0` double-port (§4.3) is REPORTED, NOT RESOLVED.** I did not
   diff the two ports against each other. If they disagree, my registration of
   `0x180010f00` makes the composed one reachable from a new entry point.
4. **The two matchers in `int33_profile_match.py` have 10% / 32% recall** and I
   did not try to raise them. §3.3 made that unnecessary for this question; it
   does **not** make them a usable tool for a cross-build question generally, and
   nobody should quote a miss from them without re-reading the control.
5. **I did not re-derive the `what it is` column for rows 4–21 from scratch.** I
   re-derived the **section**, the **extent**, and the **call-target map** of
   every one of them (§5, `int33_normdiff.py --map`); the *type names* come from
   our own tree's call-site comments, as Wave 23 established. Where my measurement
   and the record disagree I say so (row 7: `0x18002f170` and `0x1800493d0` were
   published as byte-identical; their extents measure 843 and 840).
6. **My extent walker is an OVER-estimate**, stated in the tool and in its
   control: it stops at the first `RET`/`JMP` followed by `0xCC`/`0x90`, so an
   interior padded `RET` reads long (measured 843 where Ghidra says 818; 285 where
   Ghidra says 265). It is used only to refuse `.pdata` sizing and to give the
   comparator a common length. **No function size in this document is quoted from
   `.pdata`**, which is a lower bound.
7. **`0x180028760`'s "not covered by introsort.cpp" blocker is now THREE waves
   old and I did not re-check it either.** Wave 23 measured it (165 insns / 523 B
   vs 422 / 1381) and Waves 24 and 25 both declined the row. I am the fourth.
8. **RTTI was not used and could not have been** — `Runtime-x64.dll` is `/GR-`.

---

## 7. RESIDUALS, EACH WITH A SCOPE AND A NAMED ADDRESSEE

A condition with no addressee never fires, and *"the next lane that touches X"*
satisfies a checklist and then waits forever.

| # | item | SCOPE | ADDRESSEE |
|---:|---|---|---|
| 1 | **`0x180028760`, the `Array<Mod>` introsort, 1381 B.** Declined by Waves 23, 24, 25 and 26. Its stated blocker has never been re-checked by anyone; §6.7. **It is now the ONLY body in this bucket over 900 bytes**, and the other six are 195–843 B. | one body | **Wave 27, a lane named at kickoff — NOT "lane BODY"**, which is what Wave 25 wrote and which never fired |
| 2 | **The profile build's 38 IO-codec slot names can name the slots of `0x1801554a8` (22) and `0x1801559f8` (12).** §3.4. Trap: the Encoding vtable OFFSETS DIFFER between builds, so take NAMES from profile and ORDER from retail. | those two vtables | **Wave 27 lane IOVT** |
| 3 | **`0x180010da0` has TWO ports in this tree** and the open-coded one is the registered one, while the composed one is now reachable from my `0x180010f00`. Diff them; keep one. §4.3. | `video_resources.cpp` + `device_enum_types.cpp` | **Wave 27 lane PROMOGFX's successor**, i.e. whoever owns Video/Graphics at Wave 27 kickoff |
| 4 | **None of my three ports has ever been shown to RUN.** §6.1. All three have an existing probe route and all three need one run each: `BANISHED_RT_ACHMGR`, the `video_resources` probe, and `package_file_dtor`'s. | 3 probe runs, one lock | **Wave 26 lane ATTEND** — it is already taking the lock for attempt 27 and for the W17 attendance run, so this is 3 env vars on a run it is making anyway |
| 5 | **Wave 23 residual #3 (build the cross-build matcher) is DISCHARGED** — built, controlled, and shown by §3.3 to be the wrong instrument for the question. **Do not carry it forward.** Its recorded PATH was also wrong (§3.1) and is corrected here. | the record | **the Wave 26 integrator**, to stop it reappearing in the Wave 27 kickoff |
| 6 | **The `internal` bucket refills every wave from other lanes' faithful ports** — 4, 2, 1, 1 over four waves. Wave 23 residual #2 (fold `internal_row_classify.py`'s SECTION/EXPORT/PDATA columns into the census) was addressed to the Wave 24 integrator and is **still not done** three waves later; the census now prints the SECTION hint but not the rest. `tools/int33_export_screen.py` is a second, controlled implementation of the EXPORT column and could be folded in cheaply. | `tools/runtime_census.py` | **the Wave 26 integrator**, explicitly — the previous addressee has been passed over three times |
| 7 | **The eight `SetSize` rows (Wave 25 residual #1) are NOT done.** Its addressee was "Wave 26, a named lane". I am that lane and I did **three** of the bucket's rows, none of them a `SetSize`: each needs an `assign`, an `init` and its per-arm written extents, which is a body-reading job per row, not a registration. §5.2 names the three cheapest and the fact that makes each cheap. | `runtime/src/collection/array.cpp` only | **Wave 27 lane ARRSPEC**, to be created at kickoff |

---

## 8. TOOLS ADDED BY THIS LANE (all committed, all with controls)

| tool | what it answers | controls |
|---|---|---|
| `tools/int33_normdiff.py` | Is body A the same as body B, once every encoded DISTANCE is resolved? What does body A CALL? | **9/9** — incl. all three hash-`089ee8` siblings as handed-in negatives, and a negative for the absence-shaped `--calls` screen |
| `tools/int33_export_screen.py` | Is an address-keyed row really unexported? | **4/4** — incl. a PRIVATE-but-exported positive (the AEAA trap) and a shifted-address negative |
| `tools/int33_profile_match.py` | Can the profile build name a retail internal? | a RECALL control on each matcher (10%, 32%) that **refuses** to let a miss read as a finding, plus the recall-free `--population` screen that actually settles it |
