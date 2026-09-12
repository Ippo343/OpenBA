# WAVE 28 LANE FACTUI1

Branch `wave28/factui1` off `wave28/base` (`5d2594de`), private worktree.

Scope: the `?CreateNewObject@` / `?CreateInPlaceObject@` factory PAIR for every
UI class in the absent-factory roster whose name sorts strictly before
`ImageDescription`.

---

## 0. HEADLINE

**All 31 classes landed, both slots, with their constructors, registered by
default. 60 of the 62 factory slots are ours; the other 2 belong to FACTREST.**

**AND THAT IS NOT A VERIFICATION.** Every row here registers with
**attendance 0** in an ordinary run, exactly as WAVE28_KICKOFF.md §2.2 caveat 3
predicts. *If this lane closed by reporting thirty green attendance zeros it
would have measured nothing* — so it says that in those words, in the file
banner, in the runtime report the port itself prints, and here. The value of
this lane is **entirely at cutover**, and nobody has run a cutover with it.

What this lane DID measure is a **two-sided byte oracle against the shipped
DLL's own constructors**: **26/26 in-place bodies and 14/14 out-of-line
constructors byte-identical, and — in a third pass that exercises the ALLOCATING
path — 31/31 `CreateNewObject` results identical, the four abstract ones by
calling retail and observing that it really returns null. Negative control 0/26,
0/14, 4/31 (the four survivors are exactly the abstract rows, whose claim has no
free parameter to corrupt).** §7.2.

That is a strong check on the *transcription*. It is **not** a check on the
*cutover behaviour*, and nothing in this lane is.

**Read §11 before looking for a board movement: `tools/runtime_census.py` CANNOT
SEE THIS WORK AT ALL, and the TOTAL is unchanged at 1430/1277 = 89.3% by
design, not by failure.**

---

## 1. THE ROSTER, RE-DERIVED (MEASURED)

Not taken from the kickoff on faith:

```
# NOTE THE PATH. The kickoff's `tools/ghidra_mcp/.venv/...` DOES NOT EXIST in a
# lane worktree -- only requirements.txt is checked in; the venv lives in the
# MAIN checkout. The first invocation here failed with "No such file or
# directory" and was rerun against the absolute path. An absent output is not a
# clean result, and this is a trap every Wave 28 lane walks into.
<main-checkout>/tools/ghidra_mcp/.venv/Scripts/python.exe
    tools/mint_factory_census.py | sed -n '/--- ABSENT/,/--- BIAS/p'
```

Filtering that to `@UI@@` classes sorting strictly before `ImageDescription`
gives **exactly the 31 the kickoff named**, no more and no fewer. The mint rows
were then read directly out of `standalone/rt_data_mint_tables.inc` so the
slot-0 / slot-1 NAMES and ADDRESSES come from the table the mint actually
consumes, not from a naming convention.

| # | class | mint idx | `CreateNewObject` | `CreateInPlaceObject` |
|---|---|---|---|---|
| 1 | AnimatedImageDescription | 2 | 0x1800367c0 | 0x180036840 |
| 2 | AnimatedImageElement | 3 | 0x180036ba0 | 0x180036c20 |
| 3 | BorderDescription | 7 | 0x180040ad0 | 0x180040b50 |
| 4 | BorderElement | 8 | 0x1800411a0 | 0x180041220 |
| 5 | ButtonDescription | 9 | 0x180047f20 | 0x180047fa0 |
| 6 | ButtonElement | 10 | 0x180048350 | 0x1800483f0 |
| 7 | CharacterSet | 11 | 0x180037540 | 0x1800375f0 |
| 8 | CheckDescription | 12 | 0x180048770 | 0x180048800 |
| 9 | CheckElement | 13 | 0x180048b70 | 0x180048c20 |
| 10 | ClipRegionDescription | 14 | 0x180037a80 | 0x180037b80 |
| 11 | ClipRegionElement | 15 | 0x180038060 | 0x180038110 |
| 12 | ComboDescription | 17 | 0x180074bd0 | 0x180074c70 |
| 13 | ComboDialog | 18 | 0x180074850 | 0x1800748f0 |
| 14 | ComboElement | 19 | 0x180075020 | 0x1800750a0 |
| 15 | ContainerDescription | 21 | 0x1800399c0 | 0x180039a40 |
| 16 | ContainerElement | 22 | 0x180039db0 | 0x180039e50 |
| 17 | Cursor | 23 | 0x18003b070 | 0x18003b120 |
| 18 | DialogController | 25 | 0x18003b6e0 | 0x18003b6f0 |
| 19 | DialogControllerConfig | 26 | 0x18003b480 | 0x18003b490 |
| 20 | DialogInstance | 27 | 0x1800454f0 | **NOT OURS** |
| 21 | DocumentTextDescription | 28 | 0x18003b9f0 | 0x18003ba70 |
| 22 | DocumentTextElement | 29 | 0x18003bf40 | 0x18003bfc0 |
| 23 | EditDescription | 31 | 0x18003f7c0 | 0x18003f840 |
| 24 | EditElement | 32 | 0x18003fbd0 | 0x18003fc50 |
| 25 | Element | 33 | 0x180035450 | **NOT OURS** |
| 26 | ElementDescription | 34 | 0x180035100 | 0x180035110 |
| 27 | Font | 39 | 0x180031ab0 | 0x180031ba0 |
| 28 | GraphDescription | 44 | 0x180040180 | 0x180040200 |
| 29 | GraphElement | 45 | 0x180040550 | 0x1800405d0 |
| 30 | GroupDescription | 47 | 0x180046750 | 0x180046840 |
| 31 | GroupElement | 48 | 0x180046ca0 | 0x180046d30 |

**`DialogController@UI` is tagged UNPROMOTABLE as a VTABLE. Its FACTORY is
ported here and its vtable was not touched** — the asymmetry the kickoff §3.1
names, applied.

### 1.1 TWO slots in this lane's roster are NOT this lane's to port

`?_typeInfo@DialogInstance@UI@@` and `?_typeInfo@Element@UI@@` carry, in their
slot 1, **`?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z`
(0x1800070c0)** — an INHERITED factory belonging to `Object::Internal`, which is
**FACTREST's class**. Neither UI class has an in-place factory of its own; the
name is not even exported under a UI spelling. Those two slots are left alone
and are named here so they are not read as a shortfall.

**So: 62 slots in the roster, 60 owned by this lane, 60 ported.**

**FOR THE INTEGRATOR, EXPLICITLY: this lane did NOT write, port or register
`?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z` (0x1800070c0) —
neither the NAME nor the ADDRESS.** There is no merge hazard with FACTREST on
that symbol. This is not an assertion from memory: the spec table carries
`ipName == nullptr` for exactly those two rows, so the registration loop skips
them by construction, and a manual sweep of all 74 (name, address) pairs this
lane DOES register found zero collisions with anything (§11.1). FACTREST's
registration is the one that clears those two slots, and this lane expects to
see them already cleared on the merged tip.

### 1.2 ABSTRACT ROWS IN THIS SLICE: 4 classes, 7 exported slots

Reported explicitly because the integrator wants two independent readings.
**4 of 31 classes (13%) are abstract in BOTH slots** — `DialogController`,
`DialogControllerConfig`, `Element`, `ElementDescription` — accounting for
**7 exported factory addresses** (`Element` has no in-place export of its own;
its slot 1 is `Internal@Object`'s). All seven were read as BYTES individually.

**Every one of the 62 slots in this roster was classified from its own
instruction stream, not by family resemblance** — the transcriber asserts it can
account for every instruction of every body, so a slot that is `xor eax,eax;
ret` produces an empty store list and no call, and one that is not cannot. No
row in this slice is unchecked for that shape. It also holds in the other
direction: there is **no** class here whose `CreateNewObject` is concrete while
its `CreateInPlaceObject` is a stub — that is one of the three assertions in
§3.2, and it held for all 27 concrete classes.

FACTREST's final figure is 12 classes abstract in both slots (24 slots of 70). The two
readings agree on the shape and disagree on the rate, which is what one would
expect: the UI slice is weighted towards leaf widgets and the non-UI slice
towards base classes.

---

## 2. WHAT THE FAMILY ACTUALLY LOOKS LIKE (MEASURED)

Read with `objdump -d` / `pefile` against `C:\GOG Games\Banished\Runtime-x64.dll`
on disk. **Ghidra was not used for any fact in this lane — not a byte, not an
address, not a name.** Every symbol came out of the export directory by RVA,
which is also warning 5's check performed up front rather than after the fact.

The 31 classes fall into **three** shapes, not one:

| shape | n | `CreateNewObject` | `CreateInPlaceObject` |
|---|---|---|---|
| **out-of-line ctor** | 14 | lock/Allocate/unlock, null check, `call ??0X@UI@@QEAA@XZ` | `test rcx,rcx; jne <ctor>; xor eax,eax` |
| **inlined ctor** | 13 | lock/Allocate/unlock, null check, the ctor's stores emitted in line | null check, the SAME stores emitted in line |
| **abstract** | 4 | `xor eax,eax; ret` | `xor eax,eax; ret` |

**The four abstract classes are `DialogController`, `DialogControllerConfig`,
`Element` and `ElementDescription`.** Both their factories are two bytes and a
`ret`. This was read as BYTES for each of the seven addresses individually
(0x18003b6e0, 0x18003b6f0, 0x18003b480, 0x18003b490, 0x180035450, 0x180035100,
0x180035110), *not* inferred from one member of the group — the shape is so
cheap to check that inferring it would have been the recognise-the-algorithm-
and-stop-reading error of warning 4 in miniature.

That third shape is the finding the kickoff's uniform-shape argument did not
predict: **§2.2's "every `CreateNewObject` is locked `MemAlloc(N,8)` + null
check + inlined ctor" is true of 27 of these 31 and FALSE of 4.** A lane that
had written the generic body for all 31 without reading each one would have
made four abstract classes allocate and construct.

### 2.1 The allocation, checked rather than inherited

`stringtable_factory.cpp` transcribes the inlined lock/Allocate/unlock triple as
a call to the out-of-line wrapper `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z`
(0x1800c7a50). **That claim was re-checked instruction by instruction against
0x1800c7a50's own disassembly** rather than carried over by resemblance
(`reference_a_written_recipe_can_be_wrong.md`): same `?_singleton@MemoryAllocator@`,
same `+0x640` critical section, same two import slots (`*0x18014b390` enter,
`*0x18014b388` leave), same order, `?Allocate@` between them. It holds.

The SIZE is read off the ALLOCATION (`mov $<size>,%edx`), never inferred from
the stores. Two spellings of the ALIGNMENT argument occur and **both are
decoded**: `mov $0x8,%r8d` (23 classes) and `lea -0x<size-8>(%rdx),%r8d`
(4 classes: CharacterSet 0x28, ClipRegionDescription 0x78, Cursor 0x30,
EditDescription 0x88, GraphDescription 0x88, GroupDescription 0x78 — MSVC
deriving 8 from the size already in edx). All 27 concrete classes are align 8.

---

## 3. WHAT WAS PORTED

Two files, both new:

* `runtime/src/ui/factory_ui1.cpp` — the bodies, the control and the
  registration. Read its banner; it carries the per-decision reasoning.
* `runtime/src/ui/factory_ui1_tables.inc` — 393 store rows and 31 specs,
  **mechanically transcribed**, each store row citing the retail instruction
  address it came from so any line can be re-checked with one `objdump`.

One file edited: `runtime/src/ui/rt_ui_port.cpp` (one `extern`, one call).

### 3.1 Why a store TABLE and not 31 type headers

The project convention is a binary-compatible struct with a `static_assert` per
field. That convention exists for bodies that **read** fields. These bodies read
nothing: they write a fixed byte pattern whose only authority is the
disassembly. Thirty-one invented headers with invented field names would have
added thirty-one chances to mistype an offset and **no check the store table
does not already carry** — the table IS the layout assertion and it cites a
source address per row.

It also expresses things a header cannot. Several of these constructors perform
**unaligned and overlapping** stores:

* `??0AnimatedImageElement@UI@@` writes **8 bytes** of `0xFFFFFFFFFFFFFFFF`
  at **+0xd4** (0x1800cb42f), straddling two dwords;
* `?CreateNewObject@ClipRegionElement@UI@@` writes 8 bytes of `0x64` at +0xc4
  and then 8 bytes of 0 at +0xcc;
* `??0ContainerDescription@UI@@` writes 2 bytes at +0x70 and 4 at +0x74,
  leaving +0x72..0x73 untouched.

A field-by-field port would have had to guess at these, and **a byte oracle
comparing only the aligned fields would not have noticed** — warning 3's shape
(a mismatch between the writes a body performs and the writes its named helper
accounts for), arriving through the port rather than through the helper.

### 3.2 The transcription was checked for DROPPED instructions, not just for
### plausible output

The scratchpad interpreter that produced the table **asserts that it can
classify every instruction of every body it walks**, and the generator refuses
to emit anything if an assertion fails. It fired twice during development and
both were real: a `lea -0x20(%rdx),%r8d` alignment derivation, and the
self-pointer aliasing in the inlined-ctor shape where `self` lives in `%rdi`
rather than `%rcx`. Had it silently skipped either, the first would have cost an
alignment argument and the second would have dropped **every field store** of
7 classes and produced a port that allocates and constructs nothing — and the
attendance counter would still have read 0, which is what this family's counter
reads when it is correct.

Three assertions held across all 31 and are load-bearing evidence, not decoration:

1. for all 13 inlined-ctor classes, the store list in `CreateNewObject` and the
   one in `CreateInPlaceObject` are **identical** offset-for-offset,
   width-for-width, value-for-value;
2. for all 14 out-of-line-ctor classes, **both** factories call that one ctor and
   perform **no** stores of their own;
3. for all 4 abstract classes there are **no** stores and **no** call.

### 3.3 What warning 8 found, and it changed the plan twice

Greps by NAME **and** by ADDRESS, scoped to `runtime/ code/ standalone/ engine/
dx11/`, before a line was written:

* **`??0Element@UI@@QEAA@XZ` (0x180115e50) is ALREADY PORTED** —
  `RtUi_Element_Ctor`, `runtime/src/ui/element.cpp:1895`, Wave 7 lane A.
  **Fourteen of these 31 constructors call it.** This lane therefore neither
  re-implements nor re-registers it; it calls it by name through the port-aware
  door, which reaches our body today and at cutover. Re-porting it would have
  been the third-time-in-one-wave defect
  (`reference_grep_for_the_port_before_writing_it.md`).
* **`??0ElementDescription@UI@@QEAA@XZ` is already ported** (`dialog.cpp:2664`).
  Harmless, because `ElementDescription` turns out to be abstract and no factory
  here constructs one — but it would have been a duplicate registration.

The other 14 ctor names are absent in both directions and are registered here.

An independent corroboration of the transcription fell out of this: the already
ported `RtUi_Element_Ctor` was written in Wave 7 from the same disassembly by a
different method (named fields, a header, a build oracle). Its 24 field writes
**agree offset-for-offset and value-for-value** with what this lane's
interpreter produced for 0x180115e50 — `_alignment = 3` at +0x30,
`_flags = 0x08000000` at +0x34, `_color = 0xFFFFFFFF` at +0x38,
`_toolTip.index = -1` at +0x50, and the five fields Wave 7 records as
*deliberately untouched* are exactly the five gaps in the interpreter's list.
**MEASURED, on a body neither side ported for the other's benefit.**

---

## 4. REGISTRATION — DEFAULT ON

Kill switch **`BANISHED_RT_UIFACT1`, default ON**. `=0` leaves all 60 slots on
the DLL; `=2` additionally runs the control in §5;
`BANISHED_RT_UIFACT1_VERBOSE` adds a per-class attendance line.

This follows `stringtable_factory.cpp` and deliberately **not**
`dialog_factory.cpp`, whose opt-in `BANISHED_RT_DLGFACTORY` is why a Wave 11
port contributed nothing to the live count for seventeen waves
(WAVE28_KICKOFF.md §2.2 caveat 1). A registration that does not happen cannot
clear a trap.

**Ordering was READ, not assumed** (caveat 2): in this worktree
`standalone/resolve_standalone.cpp` calls `RtUiPortInit()` at **:333** and
`RtDataMintInit()` at **:406**, and the comment at the mint call states the
dependency explicitly ("a vtable slot is filled with our port only if that port
is registered by now"). `RtUiFactory1Register()` is called from
`RtUiPortInit()`, unconditionally, with its own switch checked inside it so that
switch is the only one governing it.

**BOTH doors** are registered for every row — `RegisterRuntimeExport(name)` and
`RegisterRuntimeAddr(va)`. The mint tries the NAME first and the ADDRESS second;
a name-only registration leaves any by-address consumer on the DLL, and an
address-only one is invisible to the census
(`reference_by_address_door_dies_at_cutover.md`).

**Every registered name was verified present in the export directory by RVA with
pefile** before it was written (warning 5). This was not a post-hoc audit: the
generator looks each name up in the parsed export map and throws `KeyError`
rather than emit a phantom, so a name the DLL lacks could not have reached the
table. The 32 `??_7...@6B@` vtable names in the store rows came out of that same
map by address→name lookup, so they are exports by construction.

**Row count: 74 registrations.** 31 `CreateNewObject` + 29 `CreateInPlaceObject`
+ 14 out-of-line constructors = 74. (31 − 2 in-place slots that are FACTREST's.)

---

## 5. THE CONTROL — what it checks and what it cannot

`BANISHED_RT_UIFACT1=2`. **In-place only**, on a caller-owned stack buffer: it
allocates nothing, so it cannot perturb the arena of the run it is validating.

It is scored on a subject the screen actually reads
(`reference_control_must_pick_a_compared_subject.md`): the buffer is prefilled
with `0xA5`, `CreateInPlaceObject` is called, and **every byte the store table
claims to write must have changed away from poison**. That is precisely this
lane's claim and nothing wider. It is **not** scored on the vptr VALUE — a DLL
pointer today and a minted pointer at cutover, so a screen on it would report an
expected disagreement as a defect
(`reference_oracle_must_not_compare_unwritten_bytes.md`, one step over). It is
**not** scored on bytes retail leaves alone.

**It reaches 12 of 31 classes (MEASURED live, §7.3 — the estimate written here
first was 13 and was wrong by one), and the exclusions are principled, not
convenient.** The 14 Element-derived classes plus DialogInstance and ComboDialog
are excluded because their base constructor chain ends in
`??0Internal@Object@@`, which **registers a handle in
`?_singleton@InternalFactory@Object@@` pointing at the buffer**. That entry
would dangle the moment the buffer left scope: the control would have corrupted
the process in order to measure itself. The 4 abstract classes are excluded
because they have nothing to write.

**The number to read is "fired N of M", and M is printed.** If M were 0 the
control had nothing to bite on and the attendance zeros would prove nothing
either way (`reference_probe_needs_an_effectiveness_control.md`). See §7 for
the run.

---

## 6. MEASURED vs INFERRED

**MEASURED** (a command was run, its output read):

* The 31-class roster and its 62 slots, from `tools/mint_factory_census.py` and
  `standalone/rt_data_mint_tables.inc` on this tip.
* Every allocation size, alignment, store offset, store width, store value,
  vtable symbol and call target — `objdump`/`pefile` against the retail DLL.
* The three shapes, including the 4 abstract classes read as bytes individually.
* `?MemAlloc@` 0x1800c7a50 being the exact out-of-line form of the inlined
  triple.
* `??0Element@UI@@QEAA@XZ` and `??0ElementDescription@UI@@QEAA@XZ` already in
  the tree; the other 14 ctor names and all 60 factory addresses absent.
* The agreement between this lane's interpreter output for 0x180115e50 and
  Wave 7's independently written `RtUi_Element_Ctor` (§3.3).
* The `RtUiPortInit` :333 / `RtDataMintInit` :406 ordering, read in this
  worktree.
* The build gate: `mingw32-make standalone-debug` exit 0, zero warnings (§7.1).
* **The offline byte oracle, three passes and both directions: 26/26 in-place,
  14/14 constructors, 31/31 `CreateNewObject` (allocating path) identical to the
  shipped DLL's own; negative control 0/26, 0/14, 4/31** (§7.2).
* That the committed store table re-derives byte-for-byte from a clean
  regeneration in a lane-private directory (§7.0).
* That `tools/runtime_census.py` contains **zero** `CreateNewObject` /
  `CreateInPlaceObject` rows and its TOTAL is unchanged by this lane's 74
  registrations (§11.0).
* That `tools/mint_factory_census.py`'s slot-0 PRESENT count moves 2 -> 33, and
  that the 31 new names are exactly the roster **as a set** (§11.1).
* Zero duplicate registrations across all 74 (name, address) pairs (§11.1).
* That `?Startup@MemoryAllocator@System@@` takes MEGABYTES and its second
  argument is a maximum that must be >= the first (§7.2).

**INFERRED** (reasoning, not a measurement — treat accordingly):

* That these 60 registrations will clear 60 of the mint's factory traps at
  cutover. The mechanism is understood and `stringtable_factory.cpp` is the
  worked precedent, but **no cutover boot has been run with this branch.**
* That the four abstract classes are abstract *in the C++ sense*. What was
  measured is that their factories return null. That is all a factory slot
  needs, and the port reproduces exactly what was measured, so nothing rests on
  the inference.
* That calling the out-of-line `?MemAlloc@` instead of open-coding the triple is
  indistinguishable to any observer. The instruction sequences match; a
  difference could only appear to something inspecting the return address inside
  `Allocate`, and nothing does.

---

## 7. RESULTS

### 7.0 THE COMMITTED TABLE RE-DERIVES FROM SCRATCH — and it had to be checked

**The session scratchpad is shared between all seven Wave 28 lanes, and one of
this lane's analysis scripts was silently overwritten by a sibling lane
mid-run.** Measured, not hypothetical: `exp.py` — the script that builds the
export map through which every address, name and vtable symbol in the store
table was resolved — was found holding *another lane's* content, a different
program under the same basename.

The integrator flagged the hazard generally; this lane then had to answer
whether its committed artefact was contaminated. **The answer is no, and it was
established by re-derivation rather than by argument.** The whole pipeline was
re-run in a lane-private `scratchpad/factui1/` against a freshly built export
map, and the result diffed against what had already been committed:

```
regenerated  637 lines
committed    637 lines
diff         IDENTICAL, exit 0
```

The §7.2 oracle was then rebuilt in that private directory against the committed
table and re-run, reproducing 26/26 + 14/14 and 0/26 + 0/14.

**The general form, which outlives this wave: a shared scratchpad makes a
basename a shared global, and the collision is silent in BOTH directions** — the
clobberer gets no error and the clobbered lane gets a confident wrong answer with
no diagnostic. What cleared this lane is that its committed artefact is a pure
function of two inputs, the retail DLL and the roster, so re-derivation was a
one-command check. **An artefact that cannot be cheaply re-derived cannot be
cleared of this at all**, and several lanes' outputs this wave are of that kind.

### 7.1 BUILD GATE — MEASURED, AND THE ARTEFACT CHECKED, NOT JUST THE EXIT CODE

A sibling lane measured a backgrounded `mingw32-make` writing a **zero-byte**
output file and still reporting **exit 0**. An exit code alone is a gate that
can pass vacuously, so this one names the artefact:

```
mingw32-make standalone-debug            -> EXIT 0   (FOREGROUND, not backgrounded)
standalone/build/reimpl-x64.exe          33,434,116 bytes
                                         mtime 2026-09-12 13:07:58
newer-than-exe source files              NONE
                                         (find <3 lane sources> -newer <exe> -> empty)
warnings under -Wall -Wextra             0
nm  | grep -c RtUiFactory1Register       2
strings | grep -c 'CreateNewObject@GroupElement@UI'   2
strings | grep -c '??0DialogInstance@UI@@QEAA@XZ'     1
```

The last three lines are the ones that matter: **the registered symbol names are
IN THE LINKED IMAGE**, not merely in an object file that might not have been
linked. This lane's FIRST build attempt was backgrounded and produced no output
file at all, which is how the hazard was already known here.

### 7.2 THE OFFLINE BYTE ORACLE — MEASURED, AND TWO-SIDED

**This is the finding that makes the lane more than "ported + registered".**

A separate short-lived process `LoadLibrary`s the retail `Runtime-x64.dll`,
stands up the two singletons the constructors reach
(`??0MemoryAllocator@System@@` + `?Startup@`, then `??0InternalFactory@Object@@`
+ `?Startup@`), and for each class runs **our** in-place constructor and **the
DLL's own exported one** on two buffers poisoned with **different** bytes
(0xA5 and 0x5A), then compares.

**The subject is the bytes EITHER side wrote** — a byte still holding its own
poison in both buffers is skipped, because comparing bytes neither side writes
fails on padding and reads like a catastrophic port
(`reference_oracle_must_not_compare_unwritten_bytes.md`; this screen DID report
exactly that on its first run, on 6 classes, and every one was inter-field
padding — the finding is that the screen was wrong, not the port). Different
poisons on the two sides is what lets the screen also catch a **dropped** store
and an **extra** store, not only a wrong value. No store value in the 27 tables
contains an 0xA5 or 0x5A byte, so a real write can never read as untouched.

**THE VPTR IS COMPARED, AND THAT IS NOW WITNESSED RATHER THAN ASSUMED.**
Lane FACTREST adapted this harness to its own tables and its first run scored
**0/11, "3 bytes differ, first +0x00" on every class** — maximally consistent,
and it reads exactly like a catastrophic port. **It was the instrument:** its
copy dereferenced the `??_7` export, and a `??_7` export's ADDRESS *is* the
vtable, so the deref yields `vtable[0]`, the first virtual function, instead of
the vptr. FACTREST's port was right throughout.

That line was inherited from this file, so FACTREST correctly asked whether the
same defect was inflating **this lane's** 26/26 rather than assuming it benign
here because it was benign to fix there. The answer, checked rather than
argued:

* the helper carrying the deref (`VT()`) exists in this copy but **is never
  called** — dead code, now deleted with a comment in its place;
* the live `SK_VTABLE` branch has always used `Sym(s.sym)` directly, i.e. the
  address, which is what the port does and what retail's
  `lea rax,[rip+X] ; mov rax,(rcx)` does;
* and a **vptr witness** was added to print ours-vs-theirs at +0x00:

```
[vptr witness] AnimatedImageDescription ours=00007fff54976060 theirs=…6060  AGREE
[vptr witness] AnimatedImageElement     ours=00007fff54976000 theirs=…6000  AGREE
[vptr witness] BorderDescription        ours=00007fff54976980 theirs=…6980  AGREE
```

#### The witness then had to be armed, and arming it found TWO more defects

FACTREST's sharper framing, which is theirs and is the better statement:
**an equality test between two sides that share a defect cannot see the
defect.** "Do ours and theirs agree at +0x00" could never have answered the
question they asked, because a screen where BOTH sides dereferenced would print
AGREE while comparing `vtable[0]` on both sides. The discriminating property is
not equality, it is the **PE section**: a vtable lives in `.rdata`, and
`vtable[0]` points into `.text`.

Two defects came out of acting on that, and **both were found by ARMING the
screen (`--derefbug` reintroduces the deref on our side only), not by reading
it**:

1. **The first witness printed the addresses and left a human to read the
   range.** That makes the discriminating half of the check a predicate nobody
   evaluates (`reference_a_predicate_nobody_evaluates.md`). It now resolves the
   section BY NAME from the loaded module's headers and states its own verdict.
2. **`SectionOf` returned a pointer into a function-local `static char` buffer.**
   The witness calls it twice — ours, then theirs — so both pointers aliased the
   one buffer and both printed what the SECOND call wrote. The predicate built
   on it, `os == ".rdata" && ts == ".rdata"`, was **testing one value twice and
   could not distinguish the two sides at all.** That is the same defect class
   the witness exists to rule out, one level down again, and it survived being
   written immediately below a comment explaining the defect.

The armed run is what exposed #2: it printed `*** DIFFER *** / .rdata BOTH` —
the equality half fired, the discriminating half did not. Run only in its normal
state the screen would have said `.rdata BOTH` forever and looked like a working
check.

```
ARMED  (--derefbug)   ours=…69e0(.text)  theirs=…6060(.rdata)
                      *** DIFFER *** / *** NOT .rdata ***          both halves fire
NORMAL                ours=…6060(.rdata) theirs=…6060(.rdata)
                      AGREE / .rdata BOTH                          both halves clean
```

#### The arming is SPECIFIC, and that is a third check, not a detail

Raised by FACTREST and **run here rather than predicted**: an arming aimed at
vptrs must move the rows that stamp a vptr and **must NOT move the rows that do
not.** Under `--derefbug`:

```
CREATENEWOBJECT: 4/31 byte-identical
   DialogController / DialogControllerConfig / Element / ElementDescription
      new OK (both null, abstract)     <- the 4 that stamp no vptr, UNMOVED
   the other 27                        <- all fail
```

**4/31, not 0/31.** An arming that moves everything is corrupting the run rather
than the subject, and would read as a working control while proving nothing
about aim. This one moves exactly the 27 concrete classes.

#### The two armings do not overlap, and assuming they did would have skipped one

`--negative` corrupts a store **value in the table**; `--derefbug` corrupts
**how a vtable address is resolved**, between the table and the buffer. The
first is structurally incapable of detecting the second — it perturbs the data
the screen reads, not the step the screen was suspected of getting wrong.
**"I already have a negative control" is exactly the reasoning that would have
skipped `--derefbug`**, and this lane nearly used it.

**Two findings worth carrying, and the second is FACTREST's:**

* **A uniform, maximally-consistent failure is evidence about the INSTRUMENT at
  least as often as about the subject.** The very consistency that makes it look
  systematic is the tell.
* **When checking for a defect the two sides might SHARE, test the property that
  distinguishes them, not their agreement** — and then arm it, because a screen
  whose normal answer is "clean" is indistinguishable from a dead screen until
  something makes it fire
  (`reference_absence_screen_must_be_handed_a_shape.md`).

#### The exclusion was TESTED, not recorded — and it was firing too widely

FACTREST raised the shared-premise risk (residual 4) and then, instead of
recording it, **tested whether the premise fires.** That is the sharper
question: *not whether a premise is true, but whether it BITES, and where.*
Applied to this lane's exclusion it found a defect in the **opposite direction**
to theirs.

`handles` — the flag that skips +0x08..+0x0f — was `sp.baseCtor != nullptr`.
That is a **different and wider question** than "is this object Internal-rooted",
and only an Internal-rooted object has a handle id at all. It bit two classes it
had no business biting: **`CheckDescription` and `ComboDescription` derive from
`??0ButtonDescription@UI@@`, which stamps `??_7Base` then `??_7External`
(0x1801547a0 at 0x180110b2b, read in the disassembly) and never reaches
`Internal`.** Their +0x08..+0x0f is a plain nulled field, deterministic on both
sides, and the screen was skipping it for nothing — **16 bytes of leniency
across two classes.**

Tightened to an explicit chain test. **All three passes still green with the 16
bytes now compared**, and both armings still fire:

```
NORMAL      26/26   14/14   31/31      <- CheckDescription / ComboDescription OK
--negative   0/26    0/14    4/31
--derefbug   0/26    0/14    4/31
```

So the port was right and the screen was weak. **The two lanes' defects are
mirror images:** FACTREST's premise never fired and left two whole bodies
uncompared; this lane's fired where it did not apply and left bytes uncompared.
Same question, opposite sign, and both answered by **measuring the premise
against the SUBJECT** rather than assuming its scope.

**FACTREST's check applied to this lane came back clean**, and is recorded
because a negative result from a good question is still the question working:
they asked whether `DialogInstance` and `Element` — the two UI rows whose slot 1
is the shared `Internal` null stub — had an uncompared slot 0, as theirs did.
They do not. **Pass 3 iterates the whole spec table and routes on `sp.size`, not
on the in-place slot**, so `DialogInstance`'s real allocating body is compared
(0xb8 bytes from the arena) and `Element`'s null return is checked. Verified by
reading the pass-3 output for those two names, not by reasoning about the code.

**Excluded, with the reason:** `Object::Internal`'s handle id at +0x08..+0x0f of
every Element-derived object. `??0Internal@Object@@` draws it from
`?Rand@Random@System@@` and the factory hands the second caller a DIFFERENT one
**by design**; comparing it would report the correct behaviour as a divergence.
That is the ONLY field both sides write that they are required to write
differently. **The vptr IS compared** — with the DLL present both sides must
resolve the same `??_7` export, and a disagreement there would be real.

**Three passes, because two of them leave a gap.** Pass 1 is
`CreateInPlaceObject`; pass 2 is the exported constructor (the only door
`DialogInstance` has here, since its slot 1 is FACTREST's); **pass 3 is the
ALLOCATING `CreateNewObject` path** — ours allocates through the same
out-of-line `?MemAlloc@` wrapper the port calls, theirs through its own inlined
triple, from the same arena, and the two blocks are compared. Pass 3 exists
because the allocation size and alignment arguments are otherwise only INFERRED,
and because it is **the only pass that can check the 4 abstract classes** — it
CALLS retail's `CreateNewObject` and observes that it really returns null,
rather than reading two bytes and concluding it.

```
ORACLE   pass 1  in-place bodies      26/26 byte-identical
         pass 2  out-of-line ctors    14/14 byte-identical
         pass 3  CreateNewObject      31/31 byte-identical  (27 concrete +
                                            4 abstract, both sides null)
NEGATIVE CONTROL     0/26      0/14      4/31
   (one bit flipped in the last immediate store of each class; where a class
    has NO immediate store -- GroupElement, whose whole ctor is one vtable
    stamp -- the control corrupts the vtable value instead, because a control
    that cannot be ARMED on a subject says nothing about that subject. That
    fallback was added AFTER the first negative run scored 25/26 and named
    GroupElement as the one it could not arm.)

    PASS 3's NEGATIVE IS 4/31, NOT 0/31, AND THAT IS HONEST, NOT A HOLE: the
    four survivors are exactly the four ABSTRACT classes. Their claim is "both
    sides return null" and it has no free parameter to corrupt, so the control
    cannot be armed on them at all. They are carried by pass 3's POSITIVE arm,
    which is a direct observation of retail's return value.
```

5 classes are outside this screen: the 4 abstract ones (nothing to compare) and
`DialogInstance`, whose in-place slot is FACTREST's — but `DialogInstance`'s
**constructor** is in the 14/14, which is the door this lane actually registers
for it.

**What this does NOT show.** It is a screen on the TRANSCRIPTION. It says our
byte pattern equals retail's byte pattern for 26 in-place bodies and 14
constructors. It says nothing about `CreateNewObject`'s allocation path (the
probe never calls it), nothing about the registration reaching the mint, and
nothing about cutover.

The probe is a scratchpad artefact and is **not committed** — standing warning
1 forbids this lane building tooling. Its inputs are the committed
`factory_ui1_tables.inc` and the retail DLL, so it is reconstructible from
§7.2's description in well under an hour, and the MemoryAllocator `Startup`
argument convention (below) is the only non-obvious part.

**THE PROBE SOURCE IS PRESERVED, AS DOCUMENTATION, NOT AS A TOOL.**
`docs/runtime_port/wave28_factui1_oracle.cpp.txt` — the complete 221-line
source, verbatim. It is deliberately **not** under `tools/` and deliberately
carries a `.txt` extension: standing warning 1 forbids this lane from shipping
an instrument, and promoting this one is **Wave 29's decision, not this lane's**.
Lane FACTREST asked for it by name (its 70 slots are recorded UNVERIFIED with
this harness as the route), so it is recorded where the next lane will reach it
rather than left to die with the session.

To point it at another lane's tables:

```
# 1. copy the lane's <tables>.inc next to the .cpp; the Store/Spec structs at
#    the top of the .cpp must match that lane's field order.
cp docs/runtime_port/wave28_factui1_oracle.cpp.txt /tmp/oracle.cpp
cp runtime/src/<ns>/<lane_tables>.inc /tmp/factory_ui1_tables.inc
g++ -std=c++17 -O0 -g -o /tmp/oracle.exe /tmp/oracle.cpp
A0=8 A1=256 /tmp/oracle.exe              # positive: expect N/N
A0=8 A1=256 /tmp/oracle.exe --negative   # MUST be 0/N, or the screen is dead
A0=8 A1=256 /tmp/oracle.exe --derefbug   # MUST fire BOTH halves of the vptr
                                         # witness (DIFFER *and* NOT .rdata)
```

`A0`/`A1` are the MemoryAllocator `Startup` arguments in MEGABYTES; `A1` is a
maximum total and must be >= `A0`.

**Run the negative pass every time.** Its first run here scored 25/26 and named
the one class it could not arm; without it that class would have been counted as
verified by a screen that could not have failed on it.

**A fact worth carrying out of the probe, and it is the whole reason the
bootstrap is not obvious:**
`?Startup@MemoryAllocator@System@@QEAA_N_K0@Z`'s two arguments are in
**MEGABYTES** — it does `shl $0x14` on both — and the second is a **maximum
total** that must be >= the first, which is a chunk size. Passing byte counts
makes it return false silently and the next allocation dies. Read at
0x1800c7626 and 0x1800c76a5.

### 7.3 THE LIVE BOOT AND THE IN-TREE CONTROL — MEASURED

Run under `tools/game_dir_lock.sh run lane-factui1` (deployed exe md5 verified
identical to the built one), `BANISHED_AUTOQUIT=1 BANISHED_RT_UIFACT1=2`:

```
exe exit = 0        standalone_crash.log EMPTY        BootState RUNNING

[RtPort] UI FACTUI1: 74 rows registered across 31 classes
[RtPort] UI FACTUI1 CONTROL: fired 12 of 12 reachable classes
         (19 of 31 excluded: abstract, or an Element/Internal base ctor that
          would register a handle to the scratch buffer)
[RtPort] UI FACTUI1 ATTENDANCE: CreateNewObject=0 CreateInPlaceObject=12
         ctor=12 (control fired 12 of 12)
[RtPort] UI FACTUI1 VERDICT: the ONLY entries were the control's own. Nothing
         in this run reached these ports through a door -- EXPECTED with the DLL
         present (Wave 11 lane OBJINT), and NOT evidence any body is correct.
         THESE ZEROS MEASURE NOTHING.
```

**Four things this settles, and one it does not.**

Settled: (a) 74 registrations do not break boot; (b) the count is **74**, printed
by the process rather than counted from source; (c) the control fires on
**12 of 12** reachable classes — the estimate in §5 said 13 and was wrong by one,
which is why the measured number replaced it; (d) **the report actually prints.**
That last one matters because a sibling lane measured that every lane's
`*Summary()` is written and nothing calls it. Mine hangs off a static
destructor, which is *weaker* than a call site — it would not run on a killed
process. It ran here, so this lane's counters are not structurally invisible.
That was **checked, not inherited from `stringtable_factory.cpp`'s precedent.**

Not settled: anything about cutover. `CreateNewObject=0` is the expected
attendance and the port says so itself.

---

## 8. UNVERIFIED

In that word, per WAVE28_KICKOFF.md §4:

* **Every one of the 60 factory slots and 14 constructors is UNVERIFIED as
  cutover behaviour.** No cutover boot was run. The registrations are present
  and were confirmed to REGISTER in a live boot (§7.3); whether the mint
  CONSUMES them at cutover is inferred from the mechanism and from lane
  FACTORY's precedent, not measured here.
* The in-tree control reaches only 12 of 31 classes, but **that gap is closed by
  the offline oracle, which reaches all 31** (§7.2) — so no class in this slice
  is UNVERIFIED as transcription. This bullet previously claimed 18 classes were,
  and it was written before pass 3 existed; it is corrected rather than deleted
  because the correction is the point.
* **The ALLOCATOR ITSELF is UNVERIFIED under the port.** Pass 3 exercises
  `?MemAlloc@` as the shipped DLL implements it. With `BANISHED_RT_ALLOC=1` that
  address resolves to our own allocator instead, and nothing here has run that
  combination.
* **No gameplay route exercises any of these bodies with the DLL present**, by
  construction (§0). "No route" is not "no oracle"
  (`reference_no_route_is_not_no_oracle.md`) and a synthesised oracle for the 18
  excluded classes is a real residual — see §9.

---

## 9. RESIDUALS, EACH WITH AN ADDRESSEE

1. **CLOSED DURING THE LANE, recorded because the closing is the useful part.**
   This slot originally read "the 18 control-excluded classes need a synthesised
   oracle" and named Wave 29 as the addressee. The blocker — that
   `??0Internal@Object@@` registers a handle pointing at the subject buffer —
   was real for an IN-PROCESS control and evaporated for a SEPARATE, short-lived
   probe process, where the dangling entry is never read. **A blocker stated
   against one execution context is not a blocker in another**, and the habit of
   re-checking a stated blocker before deferring it
   (`reference_holdback_blockers_are_stale_check_them_first.md`) is what turned
   19 UNVERIFIED classes into 31 verified ones. **No residual remains here.**

2. **Two vtable symbols outside this lane's slice are READ by its store tables:
   `??_7ImageDescription@UI@@6B@` and `??_7ImageElement@UI@@6B@`** — dead
   intermediate stores in `??0AnimatedImageDescription@UI@@` and
   `??0AnimatedImageElement@UI@@`. They resolve through `ResolveExport` and today
   land on the DLL, which is retail-correct. **At cutover they must exist as
   minted rows or those two constructors will stamp a null vptr and immediately
   overwrite it with the correct leaf vptr — harmless in the straight-line case
   and NOT harmless under an interruption.** Both classes are **FACTUI2's**.
   **Addressee: WAVE 28 lane FACTUI2** — confirm both `??_7` rows are minted
   before the integrator runs cutover attempt 31.

3. **`?CreateInPlaceObject@Internal@Object@@` (0x1800070c0) is slot 1 of TWO of
   this lane's mint rows** (`Element@UI`, `DialogInstance@UI`) as well as of its
   own. Porting it clears three traps, not one.
   **Addressee: WAVE 28 lane FACTREST**, which owns `Internal@Object`.

4. **THE CROSS-LANE PRACTICE HAS AN UNTESTED BLIND SPOT, and it must be
   recorded WITH the practice or the practice gets oversold.**
   Raised by FACTREST, and it is the honest limit of everything §7.2 claims.
   Eight instrument defects were found this wave by one lane running the other's
   instrument against a subject its author had not seen; **zero were found by
   self-review.** But every one of the eight was *one lane's* mistake that the
   other did not share. **Neither lane has tested the practice against a premise
   BOTH lanes hold and both hold wrongly** — two lanes adapting the same oracle
   inherit the same assumptions about the DLL and would both come back green.
   That is `reference_two_checks_one_hidden_premise.md`, and running each
   other's instruments is **no defence at all** against it.
   A concrete candidate in this lane, named so it is not hypothetical: both
   oracles exclude `Object::Internal`'s handle id at +0x08..+0x0f from the
   comparison on the same reasoning. If that reasoning is wrong, **both lanes
   are green and neither screen can see it.**
   **Addressee: the integrator**, as a sentence attached to the practice
   wherever it is adopted — not as a reason to decline it.

   **PARTIALLY DISCHARGED, and the discharge is the useful part.** Both lanes
   then tested the named premise instead of leaving it recorded, and **both
   found a defect in their own half of it** — FACTREST's exclusion had never
   fired at all (leaving two bodies uncompared), this lane's fired on two
   classes it did not apply to (leaving 16 bytes uncompared). The premise
   itself survived; the SCOPE of it was wrong in both harnesses, in opposite
   directions. So the practice needs a second clause, and it is a **different
   action** from the first: cross-running instruments could not have found
   either of these, because both harnesses agreed. **Measure the premise against
   the SUBJECT, not against another instrument.** What remains genuinely
   untested is a premise that is wrong *in the same way* in both lanes.

5. **The scratchpad transcriber is not committed.** Per standing warning 1 this
   lane built no tooling; the interpreter lives in the session scratchpad and
   dies with it. The table it produced is committed, is human-readable, and
   cites a retail address per row, so it is re-checkable without the tool — but
   it is not re-*generatable* without rewriting it. WAVE26_LANE_FACTORY §4.4
   already prescribes a committed generator and WAVE28_KICKOFF.md §2.1 records
   this wave overriding that prescription on purpose.
   **Addressee: WAVE 29, the generator lane FACTORY §4 is the brief for.**

---

## 10. INSTRUMENT DEFECT OBSERVED, NOT FIXED

**1. `tools/mint_factory_census.py` — the ABSENT list gives no way to tell an
ABSTRACT factory from an unported one.**
File: `tools/mint_factory_census.py`, the `--- ABSENT ---` section.
Four of the 31 rows it hands a lane as work (`DialogController`,
`DialogControllerConfig`, `Element`, `ElementDescription`) are two bytes and a
`ret` in retail. The census prints them identically to `BorderElement`, whose
constructor writes 20 fields. A lane sizing its work from this list overstates
it, and — worse in the other direction — a lane that writes the generic body for
every row it is handed produces four factories that allocate and construct where
retail returns null. **The information is present in the binary and cheap
(`objdump` two bytes at the slot-0 address); the tool does not look.**
Not fixed, per standing warning 1.

**2. `tools/mint_factory_census.py` — a slot-1 name belonging to ANOTHER
namespace is not flagged.**
Same file. `?_typeInfo@Element@UI@@` and `?_typeInfo@DialogInstance@UI@@` carry
`?CreateInPlaceObject@Internal@Object@@` in slot 1. A lane scoped to "UI classes
before ImageDescription" reading only the census would either port an
`Object@` symbol it does not own or silently drop two slots from its count
without noticing. The inherited-factory case is visible in
`rt_data_mint_tables.inc` and invisible in the census's own output.
Not fixed.

**3. `tools/mint_factory_census.py` prints slot-0 only; the family is 2 slots
wide.**
Same file, the `slot-0 factory PRESENT/ABSENT` counters. WAVE26_LANE_FACTORY §0
states the family is 104 rows × 2 slots = 208 and the kickoff repeats it, but
the census's PRESENT/ABSENT figures are a count of **rows**, i.e. of slot 0
only. Quoting its "PRESENT" number as progress against a 206-slot wall
understates by a factor of about two. The number is correct for what it counts
and its label says `slot-0`; the hazard is that every consuming document
compares it to a slot-denominated total.
Not fixed.

**4. `tools/dup_runtime_export.py` reports `0 DEFECT` on a tree it did not
examine, and does not say so in its headline.**
File: `tools/dup_runtime_export.py`. It matches only LITERAL name and address
arguments. This lane registers through a spec table, so all 74 of its rows pass
`sp.newVa` / `sp.ipVa` / `sp.ctorVa` — variables. The tool does list them under
`NOT EXAMINED: 12 RegisterRuntimeAddr call(s) whose address argument is not a
literal`, which is honest, **but its summary line still reads `0 DEFECT` and
that is the line anyone quotes.** The NOT-EXAMINED list also names only the
three *call sites*, not the 74 *rows* behind them, so the headline understates
its own blind spot by a factor of 25.
This is `reference_literal_grep_blind_to_variables.md` for the fifth time, and
the general form is that **a screen must state its SUBJECT beside its VERDICT**
(`reference_a_screen_that_cannot_name_its_subject.md`).
Worked around by hand (§11.1), not fixed.

**5. `tools/runtime_census.py` cannot see the factory family at all, and a lane
told to attribute its census delta will conclude its work is phantom.**
File: `tools/runtime_census.py`, the `called` scan. Measured on this branch with
74 registrations landed: the CSV's 1430 rows contain **zero** matches for
`CreateNewObject` or `CreateInPlaceObject`, and TOTAL is byte-identical to
`wave28/base`. The family is outside the population because the census counts
resolve doors and nothing in our tree resolves these symbols. See §11.0.
Originally measured by lane FACTUI2; reproduced here independently and in the
stronger direction (a zero delta measured *after* landing the rows).
Not fixed.

**7. AN EDIT THAT DOES NOT MATCH IS A SILENT NO-OP, AND IT LOOKS LIKE A FIX.**
Not a tool defect, a method one, and it bit this lane at least four times:
a shell heredoc rewrote `\` at end of line so two comment fixes did not apply
and the `-Wcomment` warning survived two rebuilds; a section I believed I had
added to this document (§7.0) was simply absent; two Python replacements matched
nothing and reported success. FACTREST hit the same shape from the other side —
their "ARMED" banner silently failed to insert, so an armed run printed `0/11`
with nothing saying the failure was intended, which **in a log is
indistinguishable from a genuine catastrophic regression.**
FACTREST's `SectionOf` counterpart in this lane (§7.2 defect 2) is the same
error in different clothes: **a fix that was written but never verified to have
taken effect.**
**THE ONE-LINE FIX, and it is FACTREST's finding independently reproduced
here.** They noticed that across four such defects in their lane, the *only*
difference between the three that failed silently and the one that failed
loudly was a single `assert old in s` in the replacement script. **That
correlation is exact in this lane too:**

| edit | precondition? | outcome |
|---|---|---|
| the two `-Wcomment` comment fixes | none | silent; warning survived two rebuilds |
| the §7.0 section insert | none | silent; section simply absent |
| the §6/§7.1 rewrites | `assert old in s` | **AssertionError in under a second** |

The loud one is how the heredoc backslash-mangling was found at all. So the
actionable rule is not "verify your edits" — it is **put a precondition on the
replacement**, one line, zero cost, and the failure announces itself instead of
being reported as success.

**For the memory file, not just this doc:**
`reference_heredoc_eats_backslashes.md` covers the QUOTING half of this and does
**not** cover the SILENCE. It reads as a text-mangling problem, when the
expensive half is that the mangling is **reported as success** — which is what
makes stopping to look feel unnecessary. **Addressee: the integrator**, for the
memory file, since that is where the next lane meets this rather than in two
lane docs.

**This lane's printed counts cannot drift the same way.** FACTREST's worst
instance of this defect was in COMMITTED PORT CODE — a `printf` that kept
reporting 70 slots after a dedupe made it 68, so the shipped build lied about
itself in its own log. Checked here: `RtUiFactory1Register` prints
`int n = RegisterAll(...)`, a value **accumulated by the registration loop
itself** (`++n` per registered row), not a literal. The live run printed **74**,
which matches the 74 derived from the spec table independently. A derived count
cannot disagree with what it counts.

**6. THE SESSION SCRATCHPAD IS A SHARED NAMESPACE AND A COLLISION IS SILENT.**
Not a tool defect — a harness one, and it is live. This lane's `exp.py` was
overwritten mid-session by a sibling lane writing the same basename, and was
found holding a different program. **A lane that merely READS such a file gets a
confident wrong answer with no diagnostic in either direction.** Cleared here
only because the committed artefact is a pure function of the retail DLL and the
roster, so re-derivation was a one-command check (§7.0) — **an artefact that
cannot be cheaply re-derived cannot be cleared of this at all.**
Mitigation applied locally (a lane-private `scratchpad/factui1/`), not fixed
globally.

---

## 11. CENSUS DELTA, ATTRIBUTED ROW BY ROW

### 11.0 `tools/runtime_census.py` DELTA IS ZERO, AND THAT IS CORRECT

**MEASURED on this branch, with all 74 registrations landed:**

```
TOTAL   1430 called / 1277 ported / 89.3%     <- IDENTICAL to wave28/base
runtime_census.py --csv | grep -c CreateNewObject      -> 0
runtime_census.py --csv | grep -c CreateInPlaceObject  -> 0
(1430 data rows in the CSV)
```

**The entire 206-slot factory family is outside the census population.** Not
under-counted — absent. Nothing in our tree *resolves* those symbols, and the
census's `called` scan is a scan of resolve doors, so no factory has ever
appeared in it: not this lane's 60, not Wave 26's `StringTable@UI`, not Wave
11's `Dialog@UI`.

This was flagged by the integrator (originally measured by lane FACTUI2) and is
**reproduced here independently, on a branch that has the work in it** — which
is the stronger direction, because a zero delta measured after landing 74 rows
cannot be explained by the rows being absent.

**Consequence, and it inverts the closing instruction this lane was given:**
"attribute your census delta row by row" has no referent for this lane. A lane
hunting for the missing movement would eventually conclude its registrations
were phantom names (standing warning 5's failure mode) and start deleting
correct work. **The instrument that CAN see this family is
`tools/mint_factory_census.py`**, and §11.1 uses it.

Logged as §10 defect 5. Not fixed.

### 11.1 `tools/mint_factory_census.py`, re-derived on this branch after the port

```
before (wave28/base)   slot-0 factory name PRESENT in tree :  2
after  (wave28/factui1) slot-0 factory name PRESENT in tree : 33
```

**+31, and the attribution is exact: one per class in §1's table, every one
pointing at `runtime\src\ui\factory_ui1_tables.inc`.** There is no unexplained
movement — "about the right amount" is not a check (standing warning 5), so the
31 names the census lists as newly PRESENT were compared against the 31 in §1
as a SET, not as a count.

The 2 pre-existing rows (`Dialog@UI`, `StringTable@UI`) are unchanged.

**What that census number does NOT say**, and it matters because every document
in this family quotes it against a 206-slot wall: **it counts ROWS, i.e. slot 0
only.** This lane's real contribution is **60 slots** (31 slot-0 + 29 slot-1),
plus 14 constructors those slots depend on = **74 registrations**. The
`PRESENT: 33` figure is correct for what it counts and its own label says
`slot-0`; see §10 defect 3.

**Registry-side delta, counted from the source rather than from the census:**

| what | n |
|---|---|
| `?CreateNewObject@` registered (name + address) | 31 |
| `?CreateInPlaceObject@` registered (name + address) | 29 |
| `??0X@UI@@QEAA@XZ` registered (name + address) | 14 |
| **total `RegisterRuntime*` pairs** | **74** |
| slots in the roster left to FACTREST | 2 |
| abstract rows whose registered body returns null, faithfully | 8 (4 classes x 2 slots) |

**Duplicate check, done by hand because the tool cannot see this lane.**
`tools/dup_runtime_export.py` reports `0 DEFECT` but also reports this lane's
three registration sites under `NOT EXAMINED: address argument is not a
literal` — my registrations pass `sp.newVa` / `sp.ipVa` / `sp.ctorVa`, i.e.
variables, and the tool matches literals only. **So its clean verdict does not
cover me.** A manual sweep of all 74 (name, address) pairs against
`runtime/ code/ standalone/ engine/ dx11/` (excluding build artefacts and this
lane's own two files) returned **one hit, and it is a comment**
(`engine/types/ui/DialogInstance.h:15` mentions 0x180114ca0 in prose). **Zero
real collisions.** See §10 defect 4.
