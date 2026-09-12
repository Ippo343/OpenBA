# WAVE 24 — lane TINFO: are promoted `?_typeInfo@` rows INERT, or WRONG?

Branch `wave24/tinfo`, cut from `wave24/base` at `f5d70de2`. No game-directory
lock (none was allocated; CUTOVER, DIFF and TRACE are queued for it), so nothing
below is a run I performed — every measurement is either static on this tip, or a
committed run whose tip is named.

---

## 0. VERDICT

**NEITHER. The question's premise is false, and both halves of the standing
reading are false in opposite directions.**

| the claim | verdict | what it actually is |
|---|---|---|
| "a promoted `?_typeInfo@` row publishes a **ZEROED** `BaseTypeInfo`" | **FALSE** | `MintTypeInfos()` seeds all six fields, unconditionally, inside `RtDataMintInit()` **before** any promotion registers. §1 |
| "promoted rows are **inert** — promoted but never dispatched" | **FALSE** | **27 of 27** promoted rows have a name-door consumer; two of the consumer sites are default-ON and read the node every frame-ish. §2 |
| "so every further promotion on this path is a latent corruption" | **NOT SUPPORTED** | the field the consumers read (+0x00 `typeId`) is measured EQUAL to the loaded DLL's on 104/104 rows. §3 |

**The ~10 further `?_typeInfo@` gap rows across Achievement, Graphics, IO and
Object are NOT gated by this.** They are gated by the ordinary promotion warrant
(READY on the exit shadow + a consumer), not by a zero-seeding defect, because
there is no zero-seeding defect.

**What I did NOT establish** is in §5, and the single largest item is that I
could not run the instrument I built. §4 names the run that closes it, in one
line.

---

## 1. THE PREMISE IS FALSE — `kTypeInfoStorage` IS SEEDED

The kickoff states this as verified-at-kickoff and tells me to build on it rather
than re-litigate it. I re-checked it anyway, under §5 of the kickoff ("re-check
every stated blocker before acting on it — including your own, however recent"),
and it does not hold.

Both halves of the premise are individually true and the conclusion drawn from
them is not:

* `standalone/rt_data_mint_tables.inc:1607` **is** a bare
  `static RtObject::BaseTypeInfo kTypeInfoStorage[104];` — true.
* `RtMintPromotionStorage` (`standalone/rt_data_mint.cpp:793`) **does** return
  `r.storage` without seeding it — true.

What neither says is that *something else already seeded it*. It did:

```
standalone/rt_data_mint.cpp:909   void RtDataMintInit() {
                          :913       MintVtables();
                          :914       MintTypeInfos();     <-- seeds all 104 rows
                          :915       MintStorage();
                          :~937      RtMintRegisterPromotedRows();   <-- promotions
```

`MintTypeInfos()` (`standalone/rt_data_mint.cpp:317`) stamps, per row:

| field | value it writes | source line |
|---|---|---|
| +0x00 `typeId` | `RtObject::TypeIdFromName(t.name)` — **Factory::Startup's own hash**, run early on a node Startup will never reach | 366 |
| +0x08 `name` | `t.name` from the generated table | 369 |
| +0x10 `parentName` | `StripParent(t.parent)` | 390 |
| +0x18 `fileExtension` | `t.ext` | 391 |
| +0x20 `factoryFn` | our port if there is one, else that row's abort trap | 393-395 |
| +0x28 `inPlaceFactoryFn` | same | 397-399 |
| +0x30 `next` | our own parents-first chain, built at 452-459 | 402, 452 |

It is unconditional — no env gate on the call, and no gate inside it except
`BANISHED_RT_MINT_PTRALIAS`, which is a control that only re-points `name`.

**One near-miss I checked rather than assumed:** `MintStorage()` runs *after*
`MintTypeInfos()` and does `memset(s.storage, 0, s.size)` in a loop. It does not
wipe the seeding — it walks `kMintStorage` / `kStorageBytes`, a **different
array** (`rt_data_mint.cpp:611-631`). `kTypeInfoStorage` is not in it.

Re-derive:

```
sed -n '317,402p;452,460p;611,631p;909,940p' standalone/rt_data_mint.cpp
grep -n "kTypeInfoStorage" standalone/rt_data_mint_tables.inc standalone/rt_data_mint.cpp
```

### 1.1 And the seeding is CHECKED against the loaded DLL, not asserted

`RtDataMintShadowReport("exit")` (`rt_data_mint.cpp:1155-1224`) compares, per
row, `name` / `parentName` / `fileExtension` / `factoryFn` ground truth /
`inPlaceFactoryFn` ground truth / `typeId` against the **live DLL node**.

Most recent committed run carrying it — `8b82c107`, lane CSTREAM, autostart →
gameplay:

```
grep RtMintShadow docs/runtime_port/wave19_cstream/run_gameplay/standalone_debug.log
```
```
[RtMintShadow] BaseTypeInfo: 624 field comparisons, 0 MISMATCH, 104 typeId not-yet-comparable   <- phase=init
[RtMintShadow] BaseTypeInfo: 624 field comparisons, 0 MISMATCH,   0 typeId not-yet-comparable   <- phase=exit
```

**Arithmetic hand-check, because a number that moves the right way by the wrong
amount reads as success** (`reference_diff_the_set_not_the_count`):
`grep -c '&kTypeInfoStorage\[' standalone/rt_data_mint_tables.inc` = **104**, and
104 × 6 fields = **624**. It balances. And the `init`/`exit` pair is itself the
screen's own effectiveness evidence: at `init` all 104 typeIds are excluded
because the DLL still holds the ctor sentinel; at `exit` **zero** are excluded,
so all 104 were really compared and all 104 agreed.

**STALENESS CHECK (kickoff instrument rule 8 — two figures at different tips are
not an instrument disagreement).** That log is `8b82c107`, not my tip. I checked
what moved in between rather than assuming:

```
git diff 8b82c107..HEAD -- standalone/rt_data_mint_tables.inc | grep -c "_typeInfo@"   ->  0
git diff 8b82c107..HEAD -- standalone/rt_data_mint.cpp                                 ->  one hunk, a VTABLE size-proof branch
```

The `?_typeInfo@` half of the generated table is **byte-identical** between that
tip and mine, and the only `rt_data_mint.cpp` change is the
`ctor-stamped-neighbour` branch in the **vtable** section. What *has* changed is
that Wave 21 lane GFXDATA promoted two more Graphics rows — which moves the
*door* column in §2, not the seeded content. I am therefore carrying the
624/0-MISMATCH figure forward as a property of an unchanged mechanism, **not**
re-deriving it on this tip. That distinction is the finding's one soft edge.

---

## 2. "INERT" IS ALSO FALSE — 27 of 27 PROMOTED ROWS HAVE A NAME-DOOR CONSUMER

### 2.1 Why the existing instrument could not answer this

The kickoff points at `runtime/src/object/factory_dispatch_probe.cpp` and
`RtMintIsMintedTypeInfo`. That probe is sound and its answer is stable — same
committed run, both its controls green:

```
[FactoryDispatch] Object::External::Load          dispatches=2847   minted=0
[FactoryDispatch] Object::Factory::CreateObject   dispatches=98725  minted=0
[FactoryDispatch] UI::DialogInstance::AddControllerTab dispatches=0 -- SITE DID NOT RUN
[FactoryDispatch] CONTROL C1 (predicate) minted-node=TRUE dll-node=FALSE -> GREEN
[FactoryDispatch] CONTROL C2 (liveness) total dispatches=101572 -> GREEN
[FactoryDispatch] VERDICT: 0 of 101572 dispatches went through a MINTED BaseTypeInfo
```

**But it measures a door no promotion can reach.** All three of its sites get
`info` by walking `?_singleton@Factory@Object@@`'s AVL tree; **not one resolves a
`?_typeInfo@` symbol by name.** So `minted=0` there is a fact about the tree and
is *structurally* silent about promotion. Reading it as "promoted rows are never
dispatched" is the error this lane was sent to find: it is a green screen on the
wrong subject.

The name door has never had an instrument. §2.2 and §4 are that instrument.

### 2.2 The static census — `tools/tinfo_door_consumers.py` (new, this lane)

```
python tools/tinfo_door_consumers.py
```
```
[TInfoConsumers] SUBJECT  : 27 promoted `?_typeInfo@` symbols
[TInfoConsumers] EXAMINED : 1529 source files under runtime, code, standalone, engine, dx11
[TInfoConsumers] TOTALS   : 25 of 27 promoted rows have at least one NAME-DOOR consumer;
                            2 raw/unclassified only; 0 with no consumer at all
[TInfoConsumers] CTRL-POS ?_typeInfo@Material@Graphics@@ -> 2 NAME-door hit(s), including
                          gameupdateloop_load.cpp -> GREEN
[TInfoConsumers] CTRL-NEG (fabricated symbol) -> 0 hit(s) -> GREEN
```

The tool **states what it examined** (kickoff rule 5), is **scoped to the source
roots** (rule 1a — an unscoped grep from the repo root walks the other lanes'
worktrees), and carries both controls an absence-screen needs (rule 4): a
positive case asserted from an independent source (`WAVE17_LANE_CEIL.md` names
`?_typeInfo@Material@Graphics@@` as ATTENDED with the consumer file named), and a
fabricated symbol that must come back zero.

**Two consumers dominate, and both are live by default:**

* `runtime/src/engine/mainloop_video.cpp:303-333` — **11** of the 27 promoted
  rows, each as
  `*RT_DATA_SLOT(const uint32_t*, "?_typeInfo@…")` — a **4-byte load at +0x00**,
  exactly retail's `MOV EAX, dword ptr [<addr>]`.
* `standalone/reg_element_typeids.cpp` — the UI element type-id dispatch table:
  `g_elementTypeInfo[i] = ResolveExport(s.mangled)` at line 141, then
  `*reinterpret_cast<uint32_t*>(g_elementTypeInfo[i])` at 157 and 184. Gated on
  `BANISHED_LIVE_ELEMENT_TIDS`, which is in **`kDefaultOnFlags`**
  (`code/Game/Component/component_factory_base.cpp:302`) — **on in every ordinary
  run**. Seven promoted rows pass through it.

### 2.3 MY OWN TOOL WAS WRONG, AND THE HAND-CHECK IS WHY I KNOW

Kickoff rule 1: *"when a tool's output supports the conclusion you already
expected, you have received no evidence that the tool works — hand-check one row
while the tool is agreeing with you."*

It was agreeing with me (25 of 27, "promoted rows have consumers"), so I
hand-read the two rows it did **not** agree on —
`?_typeInfo@CheckElement@UI@@` and `?_typeInfo@NumberEditElement@UI@@`, both
reported `name=0 raw=0 uncl=…`.

Both are **name-door consumers**. The tool infers the door from a **three-line
window**, and `reg_element_typeids.cpp` holds its 20+ literals in a table
(`kElementTypeSlots[]`, lines 86-110) fifty lines above the
`ResolveExport(s.mangled)` loop that consumes them. The window cannot span that.

So the honest figure is **27 of 27**, and the tool's blind spot is **deflating** —
it can only under-report consumers, i.e. it can only make a row look *more*
inert than it is. Had I taken 25/27 at face value I would have handed the next
wave two rows falsely labelled inert. The blind spot is now printed by the tool
itself, with the hand-checked specimen named, rather than living only here.

### 2.4 A stale comment this turned up, flagged not fixed

`standalone/reg_element_typeids.cpp:55-60` and `:170-175` both reason that "the
Runtime fills the id inside its own Startup exactly as in retail, so there is no
post-Startup fill hook here and no window where the storage holds 0xFFFFFFFF."

For the **seven promoted rows in that table this is no longer the mechanism.**
Their id comes from `MintTypeInfos()`'s `TypeIdFromName`, and the DLL's
`Factory::Startup` never touches our node. The *value* is still right (§3), and
the "no 0xFFFFFFFF window" conclusion is still right — `MintTypeInfos` runs
inside `RuntimePortInit`, earlier than Startup, not later. Only the stated
*reason* is stale. Not fixed here: it is a comment, and changing text in another
lane's file mid-wave is how merges get expensive.

---

## 3. SO IS ANYTHING WRONG? — the residual hazard, bounded

Three fields on a promoted node **deliberately differ** from the DLL's, and they
are the whole remaining risk surface:

| field | minted value | is it reached? |
|---|---|---|
| +0x20 `factoryFn` | our port, or that row's **abort trap** | **No** — the only three dispatch sites walk the AVL tree; measured `0 / 101,572`, C1+C2 green (§2.1) |
| +0x28 `inPlaceFactoryFn` | same | same |
| +0x30 `next` | our private `g_mintTypeInfoHead` chain | **No** — no `Factory::Startup` walks it (`rt_data_mint.cpp:268-273`) |

And the field that *is* reached, +0x00 `typeId`, is the one measured equal to the
DLL's on **104 of 104** rows (§1.1).

**That is why the answer is "neither inert nor wrong".** The consumers are real,
they read the one field that agrees, and the fields that disagree are behind a
door measured shut.

**This is a bound, not a proof of impossibility.** It rests on the consumer set
in §2.2 reading only +0x00. I checked the two dominant consumers by hand and
they do; I did **not** check all ~40 hit sites individually (§5).

---

## 4. PROBE SHAPE (kickoff §1c), AND THE ONE RUN THAT CLOSES THIS

**Shape: shadow/diff site exercised by someone else's run.** Not an input sweep
(the subject is a data row, not a pure function); not a synthesised oracle (the
subject is *reached in play*, which is the finding); not a write-path replay (no
`Serialize`); and **not** the STRUCTURAL two-sided disassembly fallback — §2's
question is about *our* resolver, which no disassembly of either shipped binary
can answer.

**New instrument: `standalone/tinfo_door_screen.cpp`** — always on, printed from
`standalone/main.cpp:2024` beside the factory probe. Per minted row (all 104, so
the un-promoted 77 are the classifier's negative case) it prints
`door=MINTED|DLL|NULL`, `zeroed=YES|NO`, `idcmp=`, `namecmp=`, `resolves=`.

Attendance is counted in `ResolveExport` **before** its port-registry
short-circuit (`resolve_standalone.cpp:846`) — counting after it would report
`resolves=0` for exactly the promoted rows and **manufacture the "inert"
verdict**. The report also snapshots each row's counter *before* its own
`ResolveExport` call, so the instrument does not count itself as the subject's
traffic.

**Four controls, scored separately:**

| | what it proves | why it is there |
|---|---|---|
| **C1** predicate | `RtMintIsMintedTypeInfo` says TRUE for `RtMintTypeInfoStorageProbe(0)` and FALSE for the DLL's node | a predicate stuck on `false` prints `door=DLL` for all 104 and reads as *"promotion changes nothing"* — the exact wrong answer this lane exists to avoid |
| **C2** zeroed detector | `IsZeroed` says YES for a locally memset node and NO for the live DLL node | an absence screen must be handed a positive shape; `zeroed=NO ×104` is otherwise indistinguishable from a dead parser |
| **C3** discrimination / effectiveness | the MINTED/DLL split over 104 rows; all-one-value is printed **RED** | a column with one value never told you anything |
| **C4** attendance liveness | total `?_typeInfo@` name-door resolves; zero says in words that every `resolves=0` is NOT evidence | `reference_a_green_readiness_screen_is_not_a_route` |

`mingw32-make standalone-debug` is **clean** on this branch with it in.

**THE RUN THAT CLOSES THIS LANE — one line, no new harness, no new env var:**

> Any ordinary `mingw32-make sa-run` (autostart → gameplay → quit) on the merged
> tip. Then `grep TInfoDoor` the debug log. The screen is always-on and needs no
> flag. Expected if §1-§3 hold: **C1-C4 all GREEN**, `C3` reading roughly
> `MINTED=27 DLL=77`, `zeroed=0 of 27`, `idcmp` EQ on every compared row, and a
> non-zero `resolves=` on the eleven `mainloop_video.cpp` rows and the seven
> `reg_element_typeids.cpp` rows.
>
> **Pre-registered failure meanings, so the run cannot be read after the fact:**
> `C3` RED ⇒ the classifier is dead, discard the run. `zeroed>0` with C2 green ⇒
> §1 is wrong and the handover was right. `MINTED` well below 27 with C1 green ⇒
> promotion is not reaching the door at all and §2 is wrong.

DIFF's armed sweep does not surface this by itself — the screen is in *my*
worktree, not DIFF's — so this is a post-merge run, not a lock request.

---

## 5. WHAT I DID **NOT** ESTABLISH

1. **I did not run anything.** No game-directory lock was allocated to this lane
   and I did not take one. Every dynamic figure quoted is from a **committed log
   at a named older tip** (`8b82c107`), carried forward on an explicit
   unchanged-mechanism argument (§1.1), not re-derived here. The new screen has
   **never executed**. Its controls are written, not observed.
2. **I did not check all ~40 consumer sites for the field they read.** I hand-read
   the two dominant ones (`mainloop_video.cpp`, `reg_element_typeids.cpp`, 18 of
   the 27 rows between them) and both read only +0x00. The §3 bound is that
   strong and no stronger. The accessor files
   (`object_typeinfo_accessors.cpp`, `graphics_typeinfo_accessors.cpp`) return a
   whole `BaseTypeInfo*` to vtable-slot bodies that Wave 11F proved routeless —
   **that is a Wave 11F verdict I re-read but did not re-measure.**
3. **I did not touch the ~10 further `?_typeInfo@` gap rows.** §0 says the
   zero-seeding blocker on them does not exist; it does **not** say they are
   promotable. Each still needs READY on the exit shadow plus a consumer.
4. **I did not investigate `?_firstTypeInfo@BaseTypeInfo@Object@@`'s split-brain.**
   It is genuinely upstream of several `?_typeInfo@` questions and it is
   untouched by anything here — our chain and the DLL's stay separate, which is
   what keeps §3's `next` row shut.
5. **I promoted no rows and changed no behaviour.** Three files gained an
   instrument and one function; `reg_mint_promoted.cpp` is untouched.
6. **No `RT_UNPROMOTABLE` tag was created, edited or retired by this lane**, so
   the SCOPE/ADDRESSEE requirement has no subject here.

---

## 6. FILES

| file | what |
|---|---|
| `standalone/tinfo_door_screen.cpp` | **new** — the name-door screen and its four controls |
| `standalone/rt_data_mint.{h,cpp}` | `RtMintTypeInfoRowCount` / `RtMintTypeInfoRowAt` — public enumeration of the 104 rows, nothing else |
| `standalone/resolve_standalone.cpp` | one call to `RtTInfoDoorNote`, before the registry short-circuit |
| `standalone/main.cpp` | one call to `RtTInfoDoorReport`, beside the factory probe |
| `tools/tinfo_door_consumers.py` | **new** — the static consumer census, with CTRL-POS / CTRL-NEG and its own deflating blind spot printed |

## 7. TO THE INTEGRATOR — three sentences

1. **Delete handover item 1's premise.** `kTypeInfoStorage` is seeded by
   `MintTypeInfos()`; the ~10 further `?_typeInfo@` rows are not gated by a
   corruption that does not exist.
2. **Run `grep TInfoDoor` on the first post-merge `sa-run`** — that is the whole
   verification, it needs no flag, and §4 pre-registers what each outcome means.
3. **The reusable lesson is §2.1:** a sound, green, controlled instrument was
   being read as the answer to a question it is *structurally* unable to reach,
   because both questions contain the words "minted" and "dispatched". A probe's
   controls say it works; they say nothing about whether it is pointed at your
   subject.
