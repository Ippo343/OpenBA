# WAVE 23 lane OBJ — all ten Object rows

Branch `wave23/obj`, cut from `wave23/base` at `a60351e3`, private worktree
`.claude/worktrees/agent-a4857c84c1b76972b`. **Every figure below is
BRANCH-LOCAL** (kickoff rule 13); the integrator re-measures on the merged tip.

## 0. Headline

**Zero rows promoted, zero counts moved, and that is the correct answer for all
ten — but FOUR of the ten tags INVERT at cutover and none of the ten said so.**

Object's ten rows were already tagged `UNPROMOTABLE` by Wave 11 lanes OBJDATA
and TINFO. Both verdicts survive re-derivation on this tip: I could not find a
promotion that should be made. What did not survive is the **route scope**. All
ten expiries read `NONE, EVER` (seven of them) or `NONE while the DLL is
present` (three), none with a `[SCOPE:]` token. Scored against cutover:

* **Four INVERT outright** — the three `?_singleton@…Factory@Object@@` rows and
  `?_firstTypeInfo@BaseTypeInfo@Object@@`, all tagged `HARMFUL | NONE, EVER`.
  Their harm is caused by in-DLL writers and readers that **do not exist at
  cutover**, where each row is not merely safe but REQUIRED. This is the
  `?_singleton@FileSystem@IO@@` shape Wave 21 lane SHOT paid for, reproduced
  here in a different namespace and on four rows at once.
* **Four keep their NO-OP verdict but still need publishing** — the vtables.
  Nothing dispatches through them on any route, cutover included; but 28 named
  doors in this tree resolve them BY NAME, and with no DLL an unpublished row
  resolves `nullptr`. "Unpromotable" is not "nothing to do".
* **Two are genuinely scoped as written** — the `?_typeInfo@` pair already said
  "while the DLL is present"; they needed the token and an addressee, not a new
  verdict.

All ten tags are now re-derived, route-scoped and addressed:

```
CHECK 3 before: 41 tags -- 1 scoped, 1 addressed, 40 grandfathered
CHECK 3 after : 41 tags -- 11 scoped, 11 addressed, 30 grandfathered
CHECK 2 before: 20 numeric tags -- 1 declared, 19 grandfathered
CHECK 2 after : 20 numeric tags -- 5 declared, 15 grandfathered
CHECK 1 before/after: 20 of 20 scoreable tags fit, 0 STALE   (unchanged)
```

The four CONV declarations are not paperwork: a declared tag is checked under
that reading ONLY, so declaring them turned check 1's accept-either blind spot
off for four more rows, and they still fit.

**Census: identical, by design.** `Object 191 / 61 called / 51 ported / 83.6% /
AT CEILING`, `TOTAL 4632 1314 1137 86.5%` — byte-identical before and after.
Zero rows entered the unported set, zero left it.

## 1. The four vtables

### 1.1 Extent — measured, and which bound each one is

Derived from `Runtime-x64.dll` with `pefile`, not read out of a doc. The mint
row's slot count is in `standalone/rt_data_mint_tables.inc`; I re-derived it a
second way and say which kind of bound each is, because **adjacent-export
sizing is an UPPER bound routinely read as exact** and `.pdata` is a LOWER one.

| table | VA | slots | bound used | quality |
|---|---|---|---|---|
| `??_7Base@Object@@6B@` | `0x180154788` | 3 | adjacent export (`??_7External@` at `0x1801547a0`) | **UPPER** |
| `??_7External@Object@@6B@` | `0x1801547a0` | 8 | **first non-pointer qword** — slot 8 is `0x6c616e7265747845`, the ASCII `"External"` | **HARD** |
| `??_7CompiledExternal@Object@@6B@` | `0x180154a30` | 8 | first non-pointer qword (string data) | **HARD** |
| `??_7Internal@Object@@6B@` | `0x180154720` | 4 | adjacent export (`??_7Achievement@0@6B@` at `0x180154740`) | **UPPER** |

These tables are packed **contiguously with no terminator**, so the naive
"count qwords pointing into `.text`" method overshoots badly and must not be
used: from `??_7Base` it runs 11 slots, straight through `??_7External`; from
`??_7Internal` it runs 24. I used it only to confirm the *purecall* positions.

The two UPPER bounds are corroborated, not merely inherited:

* `??_7External` (extent HARD = 8) has 4 purecall slots at indices 3, 5, 6, 7
  and a concrete slot 4 = `0x1800066e0`. `??_7CompiledExternal` (extent HARD =
  8) has purecalls at the **same** indices and the **same** `0x1800066e0` at
  slot 4 — i.e. `CompiledExternal : External`, overriding 0/1/2 and inheriting
  the rest. Both hard bounds agree on an 8-slot `External` layout.
* If `External : Base`, `Base` supplies the first 3 slots and `External` adds 5
  (1 concrete + 4 purecall) = 8. That is exactly what both hard-bounded rows
  show. `??_7Internal` is 3 inherited + 1 added (`0x1800bfda0`) = 4.

**What I did not do:** I did not find an independent LOWER bound for `Base` or
`Internal`. This binary has no RTTI (the qword before each table is `.text`,
not a Complete Object Locator), so the usual COL-delimiter trick is unavailable.
If an *unexported* table began inside either row, both would be over-sized and
nothing here would notice. The purecall-index congruence above is what makes
that unlikely, not a proof.

### 1.2 Readiness — `ours N / slots M`, from the census PROMOTION BACKLOG

```
??_7Base@Object@@6B@                2  2  COMPLETE
??_7CompiledExternal@Object@@6B@    4  4  COMPLETE
??_7External@Object@@6B@            4  4  COMPLETE
??_7Internal@Object@@6B@            4  4  COMPLETE
```

All four COMPLETE. The denominator is **non-purecall slots**, which is why the
tags now declare `[CONV: M=NONPURECALL]`: 2+1=3, 4+4=8, 4+4=8, 4+0=4, matching
the extents above exactly. A green readiness screen is not a route; see 1.3.

### 1.3 Promotability — the stamper split, re-derived, with a control

The question is who **STAMPS** the table. I scanned every byte of the DLL's
`.text` for RIP-relative operands targeting each table's first slot and
separated `LEA` (a stamp) from everything else:

| table | in-DLL `LEA` stamps (not doors) | our by-name doors (doors) |
|---|---|---|
| `??_7Base@Object@@6B@` | **219** | 13 |
| `??_7External@Object@@6B@` | **196** | 13 |
| `??_7CompiledExternal@Object@@6B@` | **26** | 1 |
| `??_7Internal@Object@@6B@` | **9** | 1 |

Nobody had published these counts before; OBJDATA and TINFO argued the same
conclusion from abstractness and attendance. A promotion moves only the
right-hand column. **Our tree's 13 doors against retail's 219 stamps is the
whole verdict for `??_7Base`**, and it is not close.

I also re-ran OBJDATA §2's other half — "no qword in any shipped binary holds
any of the four table addresses" — across all four binaries, **with a positive
control OBJDATA did not state**: the same scan finds 165 hits for the purecall
thunk VA `0x18007ec28`, so a zero is the scan's answer and not its silence.

```
Runtime-x64.dll  0   Application-x64.exe  0   VideoDX11  0   VideoDX9  0
CONTROL (a VA that IS present as a qword): 165 hits
```

**The doors are 13, not 12.** TINFO §6.4 lists twelve files reaching
`??_7Base@Object@@6B@`; adding `internal.cpp` itself makes 13. For
`??_7External` the thirteenth door is `runtime/src/ui/dialog.cpp:1412`, which
resolves through the **named constant** `RtUI::kVtblExternal`
(`runtime/types/ui/ElementDescription.h:172`) rather than a literal — the
literal-grep blindness this project has now recorded four times, hit again.

**Verdict, route-scoped.**

* **DLL-present:** all four NO-OP. Nothing can dispatch through them
  (`Base`/`External`/`CompiledExternal` are abstract; `Internal`'s attendance
  is 0 even in the half where 124,395 objects carried the minted table), and a
  promotion cannot reach 219/196/26/9 in-DLL stamps anyway.
* **Cutover:** the NO-OP argument is a property of C++ and survives, but
  **"unpromotable" must not be read as "nothing to do"**. With no DLL loaded,
  `ResolveExport` on an unpublished row returns `nullptr`
  (`standalone/resolve_standalone.cpp:836-856`, and the miss is recorded), so
  all 28 of our doors hand back null. `runtime/src/object/internal.cpp:256-258`
  already states this for `??_7Internal` in as many words. Every tag now
  carries `[SCOPE: DLL-PRESENT]` and an addressee.

## 2. The six DATA rows — decided on their own xrefs

Method: a decoder-independent RIP-relative sweep of `.text` (every byte offset
× trailing-immediate widths {0,1,2,4}), classifying each hit by its REX/opcode
pair — `48 8B` READ, `48 89` WRITE, `48 8D` LEA — and bucketing anything else
separately rather than folding it into either side.

| row | VA | in-DLL READ | in-DLL WRITE | LEA | other |
|---|---|---|---|---|---|
| `?_firstTypeInfo@BaseTypeInfo@Object@@` | `0x1801fc6e0` | 106 | 105 | 0 | 0 |
| `?_singleton@ExternalFactory@Object@@` | `0x1801fc6c8` | 149 | 2 | 0 | 0 |
| `?_singleton@Factory@Object@@` | `0x1801fc6e8` | 18 | 2 | 0 | 0 |
| `?_singleton@InternalFactory@Object@@` | `0x1801fc6d8` | 5 | 1 | 0 | 0 |
| `?_typeInfo@CompiledExternal@Object@@` | `0x1801f9b00` | 2 | 0 | 1 | — |
| `?_typeInfo@Internal@Object@@` | `0x1801f9c18` | 5 | 0 | 1 | — |

Totals reproduce Wave 15's reference CSV independently (151 / 20 / 6) and
OBJDATA §4's "~104 read/write pairs" for the chain head.

**Controls (rule 9).**
*Negative*: the only thing this scan compares is the target VA, so I corrupted
exactly that — each subject re-scanned at `VA-8` and at `VA+0x10000`. **6 of 6
subjects CAUGHT** (counts differ both ways). The `VA-8` run is the sharper half:
`Factory` at `VA-8` returns 211 hits, which is `_firstTypeInfo`'s own count,
because the two statics are adjacent — proof the scan is address-exact rather
than range-matching.
*Positive*: the raw bytes behind one labelled READ and one labelled WRITE per
row are printed (`48 8b 05 …` / `48 89 0d …`), so the classification is visible,
not asserted.

**A blind spot the control did NOT catch, found by reading the `other` bucket.**
My classifier keyed on `REX.W`, so it filed the two `?_typeInfo@` rows' real
consumers as OTHER: they are `8b 05 <d32> c3` — a **non-REX 32-bit**
`MOV EAX,[rip+x]; RET`, the two-instruction typeId accessors. They are reads.
Had I only read the totals, I would have reported "0 readers" for both rows.
The table above is after that correction.

### 2.1 The three factory singletons — HARMFUL is a DLL-PRESENT verdict

OBJDATA's mechanism holds and I re-derived it: the harm is not the read
distribution, it is the **writer**. All three writers are in-DLL
(`0x1800be82e`+`0x1800dbba0`, `0x1800c09ee`+`0x1800dbafd`, `0x1800c00a0`),
reached by an in-DLL `rel32` from `Engine::Startup` — not a door. Our ported
ctors (`runtime/src/engine/singleton_ctors.cpp`) publish through `RT_DATA_SLOT`
but never run on a DLL-present boot, so a promoted slot stays **zero** while the
live factory sits in the DLL's.

**And the premise dies at cutover.** All 151 / 20 / 6 of those sites are inside
the DLL. With the DLL gone there is no reader and no writer except ours, and
`singleton_ctors.cpp:139-146` says so in its own words, written by a different
lane: *"While the DLL is loaded these resolve to the DLL's own `.data`; at
cutover they resolve to the storage lane C2's mint owns. THAT SWITCH IS THE
WHOLE POINT."* So the row is not merely safe at cutover, it is **REQUIRED** —
and `NONE, EVER` said the opposite.

I decided these three separately rather than by analogy (Wave 22 lane ERRCFG
refuted the general rule for 3 of its 5 rows that way). They do differ:
`InternalFactory` is 5 reads / 1 write and OBJDATA proved its single unported
reader dead; `ExternalFactory` is 149 reads / 2 writes. They reach the same
verdict for the same reason, but the reason is the writer in all three, which
is a column the reference-ownership CSV does not have.

### 2.2 `?_firstTypeInfo@BaseTypeInfo@Object@@` — the head of the chain

106 reads and 105 writes, of which ~104 are the self-linking inlined
`??0BaseTypeInfo` in one static initialiser per type, plus the out-of-line ctor
at `0x180006320`/`0x18000634f` and `Factory::Startup`'s read at `0x1800c065b`.
Today there is ONE chain (416 entries, 312 of them ours) **because** the row is
unpromoted and resolving by name hands both sides the DLL's cell. Promoting
splits it into 312 + 104. Unchanged verdict: **do not promote**, DLL-present.

At cutover none of those 104 static initialisers exists — they are DLL code.
There is one chain, ours, and the row is REQUIRED. Same inversion.

### 2.3 The two `?_typeInfo@` nodes — one correction to TINFO

`?_typeInfo@CompiledExternal@Object@@`: 2 in-DLL readers, and **both are the
row's own vtable accessors** (`0x18000b68f`, `0x18000b69f`). Plus the single
`LEA` at `0x180147c97`, the static initialiser linking the node onto the chain.
No third consumer exists. TINFO's NO-OP is exactly right.

`?_typeInfo@Internal@Object@@`: 5 in-DLL readers, and **only two are vtable
accessors** (`0x1800064cf`, `0x18002fc4f`). The other three —
`0x18002fb35`, `0x1800bfb74`, `0x1800bff92` — load this node into `RDX`/`RCX`
inside ordinary in-DLL bodies. TINFO §3 reasoned about this row purely through
the vtable route ("those accessors are reached only through the four
`??_7…@Object@@6B@` vtables"), and for this row that consumer set is
**incomplete**.

It does not overturn the verdict, and I want to be exact about why: those three
readers are in-DLL rel32-reachable code, so a promotion cannot redirect them
either; and TINFO measured the minted node content-identical to the DLL's after
`Factory::Startup` (`idcmp=yes`, `strs=0`). So promoting is still a no-op —
**but because the two nodes agree, not because nothing reads it.** The tag now
says that.

## 3. Task 0 — recorded facts, re-derived

1. **`BaseTypeInfo::name` is content-only.** NOT re-derived. My rows never
   needed the `+0x08` question: the six DATA verdicts turn on writers and on
   in-DLL reader counts, neither of which the name field enters. Stated rather
   than assumed-inherited.
2. **"An address-based *is it registered?* check reads factory-registered types
   as DEAD."** Correct, and it is why I used **no** address-registration check
   anywhere in this lane. The two `?_typeInfo@` rows are exactly that
   population. My instrument is the binary's own xref graph plus
   `tools/runtime_census.py`'s name-keyed scan, neither of which asks "is this
   address in the registry".
3. **The Wave 11 "object creation path" plan is PARTLY STALE.** Its two headline
   figures on this tip:
   * *"Object is the lowest-completion namespace (32/56 = 57.1%)"* — **stale**.
     Object is 51/61 = 83.6%, AT CEILING, and **seven** namespaces are now lower
     (Achievement 69.2%, Engine 69.7%, Graphics 75.0%, Audio 76.7%, internal
     77.0%, System 83.5%, ordinal 33.3%).
   * *"202 `Create*Object` exports, ZERO referenced in our source outside the
     mint table"* — **stale in the letter, true in effect**. 205 such symbols
     are now spelled in the tree, and two of them (`?CreateNewObject@Dialog@UI@@`,
     `?CreateInPlaceObject@Dialog@UI@@`) have ported bodies at
     `runtime/src/ui/dialog_factory.cpp:289,293` — the "cheap probe" that plan
     named. **But the registration sits behind `BANISHED_RT_DLGFACTORY`**
     (`dialog_factory.cpp:285-286`, and the doc block at `:96`), so on a default run nothing is registered.
   * Consequence, and I nearly got this wrong: `??_7Dialog@UI@@6B@`'s expiry is
     *"either Dialog factory ported+registered"*, which reads as **satisfied**
     after the first grep and is **NOT**, because of that env gate. Recorded as
     a residual in §6 rather than acted on — it is lane TAIL's namespace, not
     mine.

## 4. Checker output, before and after

```
tools/unpromotable_slot_check.py   BEFORE
  CHECK 1: 20 of 20 scoreable tags fit, 0 STALE
  CHECK 2: 20 numeric tags -- 1 declared, 19 grandfathered, 0 UNDECLARED, 0 malformed
  CHECK 3: 41 tags -- 1 scoped, 1 addressed, 40 grandfathered, 0 MISSING, 0 malformed
  FREEZE : 20 pairs, 19 matched
  all 6 built-in controls GREEN

tools/unpromotable_slot_check.py   AFTER
  CHECK 1: 20 of 20 scoreable tags fit, 0 STALE          <- unchanged, and now 4 of them
                                                            are checked under a DECLARED reading
  CHECK 2: 20 numeric tags -- 5 declared, 15 grandfathered, 0 UNDECLARED, 0 malformed
  CHECK 3: 41 tags -- 11 scoped, 11 addressed, 30 grandfathered, 0 MISSING, 0 malformed
  FREEZE : 20 pairs, 15 matched (the 4 Object vtable pairs unfroze BY DESIGN, which is
           what forced the CONV declarations)
  all 6 built-in controls GREEN

tools/dispatch_slot_map.py --check
  110 ResolveExport registrations, 0 name mismatch(es), 0 shadowed   (before AND after)

g++ -std=c++17 -fsyntax-only standalone/reg_mint_promoted.cpp        exit 0
```

## 5. Denominator, and what I did NOT check

**Denominator.** 10 of 10 assigned Object rows examined. The vtable extents and
all xref counts are over `Runtime-x64.dll`'s `.text` only; the qword scan is
over all four shipped binaries whole. Door counts are over
`code/ engine/ runtime/ standalone/ dx11/`.

**Not checked, explicitly:**

* **No game run.** Per kickoff §1b I treated this as binary-and-source work and
  did not contend for the game directory. **Nothing here rests on a run** — the
  live measurements I cite (`entries=0`, `ctorRuns=124,395`, `minted=zero`,
  `0 of 102,914 dispatches`) are Wave 11's, quoted as theirs, and my
  contribution is static. The one thing a run would add is confirmation that
  the four vtables still have `entries=0`; it would not change a verdict.
* **No lower bound on the `Base` / `Internal` extents** (§1.1).
* **No re-derivation of `BaseTypeInfo::name` content-only** (§3.1).
* **Non-`MOV` reference shapes.** My xref classifier covers `MOV` r/m64 and
  32-bit `MOV r32,[rip]`; a `CMP [rip+x],imm` or a reference reached through a
  computed address would be filed OTHER or missed. The `other` bucket was
  non-empty and I read it — that is how §2's blind spot was found — but the
  sweep is not a disassembler and I am not claiming it is one.
* **`tools/port_bypass_audit.py`, `rt_data_door_audit.py`** and the rest of the
  audit family were not run: this lane changed no code path, only comments.

## 6. Residuals, each with a NAMED addressee

1. **The ten Object rows are the cutover-publish list.** Every one now reads
   `[SCOPE: DLL-PRESENT]`, and six of them invert at cutover (three factory
   singletons + the chain head become REQUIRED; the four vtables need
   publishing or 28 named doors resolve null). **ADDRESSEE: the Wave 23
   integrator**, to carry this list into the Wave 24 kickoff as a named input.
   It is written into all ten tags in `standalone/reg_mint_promoted.cpp`.
2. **`??_7Dialog@UI@@6B@`'s expiry is not met, and reads as if it were.** Its
   condition ("either Dialog factory ported+registered") is satisfied in the
   source and defeated by the `BANISHED_RT_DLGFACTORY` gate (§3.3). Anyone who
   greps will conclude it lapsed. **ADDRESSEE: lane TAIL of Wave 23** (UI is its
   namespace), or failing that the Wave 23 integrator. I did not edit a tag
   outside my own rows.
3. **Thirty grandfathered expiries remain** with neither scope nor addressee,
   down from forty. The ten I removed were mine to re-measure; the rest are
   other lanes' verdicts and §6.1 forbids me rewriting them. **ADDRESSEE: the
   Wave 23 integrator**, as a wave-level backlog figure — `CHECK 3` prints it
   on every run, so it cannot go silent.

## 7. What contradicts the kickoff

Nothing in §1/§4 was wrong. One scoping remark is worth correcting: the kickoff
calls Object *"the reflection/factory core … high-blast-radius"*, and warns that
a split brain at `?_firstTypeInfo@` means "two type chains and types that cannot
find each other". That is exactly right **and** the row is already safe by
construction today — the danger is not in touching it, it is in a future lane
promoting it as part of a bulk cutover switch without noticing that the switch
must land *with* our `Factory::Startup`, not before it. That is recorded in §2.2
and in the tag.
