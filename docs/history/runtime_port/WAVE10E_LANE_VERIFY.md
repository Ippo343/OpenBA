# Wave 10E — lane VERIFY — finishing means verified, not merely ported

**Worktree** `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-a7ea3a0246a0e0e8d`
**Branch** `wave10e/lane-verify`, cut from `wave10/base` @ `c22d952`
(`git merge-base --is-ancestor c22d952 HEAD` verified before any work; the branch
then fast-forwarded over `fca68db`, the doc-only commit that opened the wave).

All four debts move. **Two of them closed on a route, two on an oracle, and one
of the oracles found a real port defect on its first clean run.**

| debt | counter before | counter after | verdict |
|---|---|---|---|
| `ToolTipDialog` non-empty arm | `GetToolTipString/operatorNew = 1`, `nonEmptyString=0` | `= 2`, `nonEmptyString=2` | **VERIFIED** on both arms |
| `?Update@Element@UI@@` | 541 entries, never compared | 19440 cases, 0 mismatches | **VERIFIED** over 4 of its 7 steps; 3 named uncovered |
| `?Persist@Element@UI@@` | `Persist=0`, no oracle | 4 cases, **2 mismatches → a defect → 0** | **VERIFIED**, and a real defect fixed |
| `?OnOpened@DialogInstance@UI@@` settings half | `noBuffer=5 badMagic=0 records=0` | `noBuffer=3 badMagic=1 records=9` | **VERIFIED LIVE** |

Every number in this document was produced by the runs in `lane_verify_logs/`,
all five final cells on **one** binary (`tools/verify_run.sh`, under
`tools/game_dir_lock.sh run lane-VERIFY … --snapshot ./lane_verify_logs`;
`acquire` was never piped and never backgrounded). Debug green; **release green
from a genuine `make clean` first**, and the binary is 8,185,073 bytes — the
release size, not the 30 MB debug one, which is the check the symmetric hazard
demands.

---

## 1. The `ToolTipDialog` family's NON-EMPTY arm

### 1.1 There is no element to find. The arm is dead in the shipped game.

The wave brief said: *"Find an element some controller calls
`SetToolTipString` on."* **No such element exists, and nothing in either shipped
binary calls that function.** A two-part screen, because either half alone is
blind:

* **Inside `Runtime-x64.dll`** — Ghidra reports three xrefs to
  `?SetToolTipString@Element@UI@@` (`0x1800356e0`) and **all three are DATA**
  (two export-directory entries plus the entry-point reference). **Zero CALL
  xrefs.**
* **From `Application-x64.exe`** — the import directory, read with `pefile`,
  contains exactly ONE `*ToolTip*` entry: `?SetToolTip@Element@UI@@` at
  `0x140151ce8`. That is the `+0x48` `tipTbl`/`tipIdx` setter.
  `SetToolTipString` is **absent**.

**That screen has a positive control, so "DATA-only" is not being read for more
than it can carry.** `?SetToolTip@` (`0x180035740`) is ALSO DATA-only in the DLL
and is demonstrably live, because it is in the exe's import table — so the DLL
half alone cannot see exe callers and the two halves are genuinely both needed.
And `?GetToolTipString@` (`0x1800356a0`), the same class, the same file, reports
**two UNCONDITIONAL_CALL xrefs** at `0x180120948` and `0x18012099f`, both inside
`BeginToolTip` — so the xref instrument does report calls when calls exist.
`SetToolTipString` has neither kind.

### 1.2 …and that was then MEASURED, not left as an inference

Lane MOUSE's one-line fix is applied: the probe dump now prints `+0x58` next to
the `+0x48` pair (`standalone/mouse_inject.cpp`, the `Walk` dump). On the live
in-game UI:

```
79 elements dumped with a rect or a tooltip capability
 0 of them with a non-null +0x58   (every line reads "null -> EMPTY arm")
```

Static call-graph and live measurement agree. `+0x58` is never populated.

### 1.3 The route, and what it is honest to claim for it

`BANISHED_MOUSE_TIPSTR=<text>` (off by default, silent unset) calls the **real
exported `?SetToolTipString@Element@UI@@`** with a **real refcounted
`System::String`** on the live element the injector is about to hover, once per
element, and then lets retail's own `BeginToolTip` choose the arm from the field
it actually reads. Nothing about the arm selection is faked.

It is a **harness-created route**, and it is labelled as one: gameplay does not
reach this arm and no change to the harness would make it. What the route buys
is coverage of the *ported bodies* on that arm.

### 1.4 The counters — RATIOS against `operatorNew`, never absolutes

`operatorNew = 2` in **both** cells, which is what makes the comparison
script-independent: the two cells differ in exactly one environment variable.

| cell | `operatorNew` | `GetToolTipString` | ratio | `nonNull` | `SetText` | ratio | `BeginToolTip nonEmptyString` |
|---|---|---|---|---|---|---|---|
| `tip_empty` | 2 | 2 | **1** | 0 | 2 | **1** | 0 |
| `tip_nonempty` | 2 | 4 | **2** | 4 | 2 | **1** | 2 |

That is `WAVE10D_LANE_TIP.md` §4's corrected prediction, met on **both** arms:
`GetToolTipString/operatorNew == 1` on the empty arm and `== 2` on the non-empty
arm, `SetText/operatorNew == 1` throughout.

> **One row of that warrant is wrong and is corrected here.** It predicts
> `bytes / operatorNew == 192 (0xc0)`. **`bytes` is not a ratio quantity.**
> `tooltipdialog.cpp:257` is `g_newBytes = bytes;` — a plain ASSIGNMENT, not an
> accumulation — so the counter holds the size of the LAST allocation, not a
> total. It reads `bytes=192` with `operatorNew=2`, and it would read `192` with
> `operatorNew=200`. The correct predicate is a CONSTANT: **`bytes == 0xc0`
> whenever `operatorNew > 0`**, which is exactly what `0xc0` is doing there — the
> comment beside the counter already says "the size retail asks for (0xc0)".
> Dividing it by the anchor is the ratio rule applied to a quantity that does not
> scale; the measured value (192, both cells) happens to agree with the predicted
> number only because `operatorNew` was 2 in the cell it was written for. This is
> the same harm lane TIP itself flagged one row above: a wrong number in a warrant
> makes a correct run read as broken the moment the script changes.

`nonNull` is the effectiveness control that lane TIP shipped beside
`GetToolTipString`, and it **varies with the thing being gated**: 0 on the empty
arm, 4 on the non-empty arm. A counter that could not move with the arm would
not be evidence about the arm.

`SetText`'s own controls (`found=2 wrote=2`) are non-zero in both cells, so the
`SetText` body reached its String assignment tail on both paths — the second
`SetText` call site (`0x1801209d6`, the non-empty arm's copy) is now covered.

**Verdict: `ToolTipDialog` family — VERIFIED on both arms.** Still ENTRY
counters, not a differential; none of the five bodies is replayable. What
changed is that the arm that had never executed now has.

---

## 2. `?Update@Element@UI@@` — the recorded "no replay oracle is possible" is
## half true, and the other half is a 19440-case differential

### 2.1 Re-checking the claim, as instructed

`element_vtable.cpp`'s header declines an oracle because the body *"pushes
events into a live array, opens and closes tooltip dialogs and can call
CloseDialog; running the shipped export a second time would double every one of
those."* `WAVE10B_LANE_ELEM.md` turned that into "no replay oracle is possible".

**The reasoning is exactly right about REPLAY on a LIVE element, and it does not
settle the question, because the objection is not uniform across the body.**
Three of the four named side effects sit behind ONE flag bit the caller
controls:

```
if ((f & kElementFlagToolTip16) == 0) return;                 // BT 0x10
if ((f & kElementFlagToolTipValid) != 0) { …tooltip block… }  // BT 0x13
```

With **bit 16 SET and bit 19 CLEAR**, the whole tooltip block is skipped —
`BeginToolTip`, `SetUpperLeft`, `EndHover` and `CloseDialog` are all unreachable
— and the body falls through to the hover-enter/leave events and the input
events. With `state->dragActive == 0` the drop arm (`Play` + `EndDragDrop`) is
unreachable too. In that regime the body makes **no call outside itself** except
`Array<UI::Event>::SetSize` on an array the CALLER owns, and the children's own
vtable slot 7. Both are satisfiable by a subject we build.

(The re-check the brief asked for — whether `??_7Element` being PROMOTED changed
what dispatches — is answered separately and negatively: it changes *who* calls
the body, not whether the body's side effects can be replayed. The regime
argument above is what makes the oracle possible, and it does not depend on the
promotion.)

### 2.2 `runtime/src/ui/element_update_probe.cpp`

A synthetic subject per side — parent element, three children (one **hidden**,
one **null slot**, so both `continue` arms run in every case), a border element,
and a `UI::UserInterface` carrying only the fields the body reads. One instance
per side; the child stub is SHARED, so it cannot manufacture a difference.
`BANISHED_RT_ELEMUPDATE_PROBE=1`, off and silent by default.

Swept: 9 mouse X × 9 mouse Y (including exactly ON every rect edge) × 3 clip
regions (one containing, one cutting, one the degenerate `(0,0)-(-1,-1)` lane
MOUSE found live) × 4 starting flag combinations × 10 UpdateStates × border/no
border = **19440 cases**.

Compared, on three screens: the Element's `_flags` plus its whole scalar tail;
the `_events` array record by record and field by field; and the child-recursion
trace.

### 2.3 The numbers

**Subject:**
```
[ElemUpdateProbe] sweep: 19440 case(s), 0 mismatch(es)
  effectiveness: hovered=480 hoverEdge=9720 casesThatPushed=9912 events=10344
                 childDispatches=38880 borderRect=9720 clipRejected=9480
  events per id: 0:240 1:9480 2:96 3:96 4:96 5:96 6:96 7:96 11:48
  catches PER SCREEN (not a union): elementFlags=0 eventCount=0 eventField=0 childTrace=0
```

`events per id` is the screen that matters. The body's own banner warns that the
ids are **not sequential** — `state+0x01 → 2, +0x02 → 4, +0x03 → 3, +0x04 → 5,
+0x05 → 7, +0x06 → 6` — and that *"the temptation to write `id = 2 + i` would
silently swap two of them"*. **Every id the body can emit on this path is
present and non-zero**: 0 and 1 (hover enter/leave), 2–7 (the six buttons, each
96 records), and 0xb (the drag-delta). That defect class is now excluded by
measurement. `0xc` (drop) and `0xf`/`0x10` (tooltip) are absent because they are
in the arms this probe does not cover, and the probe says so in its own output.

**Negative control** (`…_NEGCTL=1`: OUR side sees `mouseX + 1`, nothing else
changes — targeted at the strict-vs-inclusive confusion, since the hover test is
strict on all four sides and the clip test one line earlier is inclusive):

```
NEGATIVE CONTROL (ours sees mouseX+1): 19440 case(s), 9984 mismatch(es)
  catches PER SCREEN (not a union): elementFlags=240 eventCount=0 eventField=9744 childTrace=0
```

**Both live screens fire, and they are scored separately, never on the union.**
That distinction is not decoration: `eventField` alone accounts for 9744 of the
catches, and a union number of 9984 would have said nothing about whether
`elementFlags` — the screen that actually tests the hover predicate — works at
all. It does: 240 independent catches.

`eventCount` and `childTrace` are reported as **not exercised by this control**
rather than as passing. This perturbation cannot change how many events are
pushed in most cases, and cannot change the child set at all. Stated, not scored.

**The control's own observability is reported as an UPPER BOUND**, and this is a
correction to the first version of the file, which printed *"caught 9984 of the
2160 changed case(s)"* — a sentence nobody could have computed. It had modelled
only boundary crossings and forgotten that `rec.mouseX = mx` copies the
perturbed coordinate into **every** event record. The corrected line splits the
two reasons and says the totals are an upper bound (the predicate model is
X-axis only) so **no shortfall can be read off it**.

**Verdict: `?Update@Element@UI@@` — VERIFIED over steps 1, 2, 3/4, 6 and 7**
(child recursion, the inclusive clip test, the strict hover test and the border
indirection, the hover-enter/leave edge, and all seven input-event pushes).
**Steps 5 (the tooltip block) and 7's drag arm remain PORTED, NOT
DIFFERENTIALLY VERIFIED**, for the reason the original header gave, which is
still true of *those* arms: they call into a live `UserInterface` and cannot be
run twice. That is not an open debt to hide — it is printed by the probe on
every run.

---

## 3. `?Persist@Element@UI@@` — a real zero, no gameplay route, and the
## differential that found a defect

### 3.1 Why the counter is zero, and why no route will move it

Slot 3 is `Object::Internal`'s Persist hook. What dispatches it is
`?PersistObject@InternalFactory@Object@@` (`0x1800bff10`), whose two arms both
end in `(**(code **)(*vptr + 0x18))(…)` — byte offset `0x18` is slot 3. So
`Element::Persist` runs exactly when an `InternalPtr<Element>` is walked by the
object-graph persister, and no saved graph holds one.

**It is not dead in the shipped binaries**, which is a different claim and was
checked separately: `?Persist@Element@UI@@` is in the exe's import directory at
`0x140151940`, reached through the thunk at `0x14003a9b4`, and that thunk's
address appears in exactly **two** exe `.rdata` vtables — `0x14015c2b8` and
`0x14015d458`, which are slot 3 (`+0x18`) of `FrustumOutlineElement`
(`0x14015c2a0`) and `CaptureBoxElement` (`0x14015d440`). Both inherit the base
hook. The slot is live in the shipped tables and simply never dispatched.

**So: no gameplay route, and the reason is structural rather than "nobody
looked".** But "no route" is not "no oracle".

### 3.2 `runtime/src/ui/element_persist_probe.cpp`

The body reads nothing from `this` (RCX is untouched in the shipped listing).
Its entire input is the stream. So the subject is **two real `IO::MemoryStream`s
built through the exported ctor**, ours into one and the shipped export into the
other, comparing the emitted record and the cursor. The Encoding, the scope
object and its deleting destructor are all the DLL's own. **Nothing is stubbed.**

**The self-consistency screens run and are scored FIRST** — ours-vs-ours and
theirs-vs-theirs — because "2 of 4 cases differ" and "my harness is unstable"
are the same reading until something separates them. On this body they came back
clean (`firstDiff = -1` on both, cursors 16/16), which is what made the next
sentence sayable.

### 3.3 What it found

```
ours    aa aa aa aa 04 00 00 00 80 80 07 ab bb bb bb bb
retail  aa aa aa aa 04 00 00 00 00 00 00 00 bb bb bb bb
```

Reproducibly, on both the read and the write slot. **The port declared the
encoding factory as `void* (*)(void*, void*)` and passed TWO arguments where
both slots take `(Encoding*, Serializer*, int version)`.** R8 carried whatever
the caller had left and the codec wrote it into the record as the version.

The shipped body's `XOR R8D,R8D` at `0x180116889` is that `0`: it sets up the
`CMP dword ptr [RDX+0x8],R8D` on the next line **and stays live across the call**
at `0x180116897` / `0x18011689c`. The transcription in the port's own header
showed two arguments and the code matched the header.

`runtime/types/io/Serializer.h:104` documents the three-argument signature, and
**every other encoding call site in the tree already passed three** —
`goal_serialize.cpp:176`, `serializer.cpp:453`, `dialog_vtable.cpp:656`,
`elementdescription_serialize.cpp:171`, `spritesheet_vtable.cpp:493`. The defect
was isolated to this one row, which is exactly why nothing else caught it: it
was the only row with no oracle.

**A byte pin cannot see this** — it hashes the SHIPPED body, not ours. **An
entry counter cannot see this.** This row had nothing else.

### 3.4 After the fix

```
SELF-CONSISTENCY (write path): ours-vs-ours 16/16 firstDiff=-1 ; theirs-vs-theirs 16/16 firstDiff=-1
  both sides are reproducible call-to-call, so the WHOLE 16-byte record is comparable.
real-stream differential: 4 case(s), 0 mismatch(es) (compared: the cursor, plus bytes [0,16))
  effectiveness: bytes emitted on the WRITE cases=32, cases whose cursor moved=4/4
```

Negative control (ours gets the opposite `direction`, targeted at the body's
only decision — the `0x18` / `0x20` slot choice):

```
NEGATIVE CONTROL: 4 case(s), 4 mismatch(es)
  control OK: 4 of 4 perturbed case(s) caught -- the comparator can tell slot 0x18 from slot 0x20.
```

`bytes = 32` over two write cases is the effectiveness screen: the comparison is
over bytes that were actually written, not over a buffer neither side touched.

**Verdict: `?Persist@Element@UI@@` — VERIFIED, and a real divergence found and
fixed.** `Persist` attendance moves `0 → 6` in the probe cells (2 from the
self-consistency screens, 4 from the differential). There is still **no gameplay
route**, and §3.1 is the proof rather than an absence of effort.

---

## 4. `?OnOpened@DialogInstance@UI@@`'s settings half

### 4.1 A correction to the stated search, derived before running anything

The wave brief says `records>0` needs an entity whose `.rsc` UIDescription
declares controllers. **The first gate is not controllers.** All five recorded
entries died at `noBuffer` — `_userData == nullptr` — and `_userData` (`+0xa8`)
is a pure **ctor argument** (`dialog_create.cpp:806`). Of the sixteen exe-side
`fn_DialogInstance_Ctor` call sites, fifteen pass `nullptr, 0`; exactly one
passes a real buffer, `code/Game/Dialog/edittool_select.cpp:233`, and what it
passes is the **selected entity's UIComponent settings buffer** (`+0x28`) and
count (`+0x30`).

So the search is "an entity with a **UIComponent**". Controllers matter only
after that, and only for the size of `records`.

### 4.2 A census, because guessing costs a run per guess

`BANISHED_TOOL_HARNESS=uicompcensus` walks the whole `EntityList` once and
reports, per entity, whether a UIComponent is present and what its buffer looks
like. It carries its own instrument control — it prints the type id it searched
for and the number of components it actually walked — because "no UIComponent
found" and "my type-id lookup is wrong" produce the identical answer, and the
second would indict a sound conclusion.

```
[UICompCensus] entity[11]=… first=UIComponent components=22 UIComponent=… buffer=0000000000000000 count=0 cap=0
[UICompCensus] SUMMARY entities=23175 componentsWalked=101865 searchedTypeId=0x988e1422
               withUIComponent=1 withNonEmptyBuffer=0
```

**Exactly one entity out of 23175, over 101865 components walked.** Founding
houses were the wrong subject, not the wrong idea — and lane ROUTES's own
component dump had already proved that about founding houses specifically.

(The cell had the four `BANISHED_BUILD_DEMO_*` buildings enabled to widen the
entity variety, so **which** entity type this is has not been established by a
control and is deliberately not named here. What is established is that one
exists and that it is reachable.)

### 4.3 The route, and its single-variable control

`BANISHED_TOOL_HARNESS=dlgsettings` runs the census, then points lane ROUTES's
own three-select sequence at the entity the census found:

```
select(A)  buffer freshly ZEROED -> magic != 0xDEADBEEF -> the badMagic exit
select(B)  closing A runs OnClosed -> SaveSettings, which WRITES the magic and
           one framed record per controller into A's buffer
select(A)  reopens over a valid magic -> the Serializer, the count read and the
           per-record loop
```

B must itself be an entity that opens a dialog — EditTool's select returns early
when the entity has no UIDescription (`edittool_select.cpp:209`) and an early
return never reaches the close of the previous panel. A founding house is used,
because `FireHousePanel` already proves a founding house opens one.

The buffer's head dword is read back and printed after the close, so
"SaveSettings never ran" and "OnOpened did not read it" are distinguishable:

```
[DlgSettings:route] after close, A's buffer=… head=deadbeef
```

### 4.4 The numbers, and the gate tested before it was trusted

| cell | `entries` | `noBuffer` | `badMagic` | `records` |
|---|---|---|---|---|
| **before** (any prior cell, e.g. `tip_empty`) | 4 | **4** | **0** | **0** |
| `dlgsettings_ctl` — same selects, buffer install **OFF** | 5 | **5** | **0** | **0** |
| `dlgsettings` — buffer install **ON** | 5 | **3** | **1** | **9** |

The control is the point. `dlgsettings_ctl` runs the **identical** three selects
on the **identical** subject with `BANISHED_DLGSETTINGS_ROUTE` unset, so
`DlgSettings_InstallBuffer` no-ops. `entries` is 5 in both — which is exactly why
`entries` is not the counter to read; it was already non-zero while the half
this route exists for had never executed once. **`badMagic` and `records` move
only with the buffer install**, i.e. the quantity being measured genuinely
varies with the thing being gated.

`records=9` means the MemoryStream, the `0xDEADBEEF` magic read, the Serializer,
the count read and **nine iterations of the per-record loop** all ran — the
entire body that had never executed.

**Verdict: `?OnOpened@DialogInstance@UI@@` settings half — VERIFIED LIVE.**
Still an entry counter, not a differential; the body is not replayable (it
dispatches into live controllers). What changed is that it is no longer
unexercised.

---

## 5. Files touched

| file | what |
|---|---|
| `runtime/src/ui/element_update_probe.cpp` | NEW — the 19440-case synthetic differential |
| `runtime/src/ui/element_persist_probe.cpp` | NEW — the real-stream differential + self-consistency screens |
| `runtime/src/ui/element_vtable.cpp` | **the three-argument fix**, its evidence, and the corrected transcription |
| `runtime/src/io/filesystem_packages.cpp` | invokes both probes beside the four IO probes (allocator liveness), **outside** the `rt_diff_on__` guard |
| `standalone/mouse_inject.cpp` | `+0x58` in the probe dump; `BANISHED_MOUSE_TIPSTR` |
| `standalone/tool_harness.cpp` | `uicompcensus`, `dlgsettings` |
| `tools/verify_run.sh` | NEW — the cell driver |

New switches, **all off by default and silent when unset**:
`BANISHED_RT_ELEMUPDATE_PROBE`, `BANISHED_RT_ELEMUPDATE_PROBE_NEGCTL`,
`BANISHED_RT_ELEMPERSIST_PROBE`, `BANISHED_RT_ELEMPERSIST_PROBE_NEGCTL`,
`BANISHED_MOUSE_TIPSTR`, `BANISHED_UICOMP_CENSUS_SHOW`, and the two
`BANISHED_TOOL_HARNESS` modes.

## 6. What cost a round, recorded so it does not cost another

* **The first placement of both probes was inside `if (rt_diff_on__)`.** Both
  cells printed nothing at all, which reads exactly like a probe that ran and
  found nothing — the switch the reader set was not the switch that gated the
  code. They are now outside it: each has its own switch and adding a second,
  undocumented condition buys nothing.
* **The first Persist run's 2/4 mismatches would have been publishable as a port
  defect without a self-consistency screen — and would have been publishable as
  a harness artefact *with* one that came back dirty.** The screen is what
  turned a number into a verdict, in both directions.
* **The first control number was wrong by 4.6x and read as authoritative**
  ("caught 9984 of the 2160 changed"). It modelled one of the two mechanisms by
  which the perturbation is observable. Splitting the count by mechanism and
  labelling the modelled half an upper bound is the fix; scoring the catches per
  screen is what makes the control mean anything at all.
