# Wave 10, lane UI3 — the four allocating `UI::DialogInstance` census rows

**Scope:** the four rows `WAVE10_UI_CENSUS.md` §1 class B assigned to this lane —
`??0DialogInstance`, both `?AddController@DialogInstance@` overloads, and
`?StartTimer@DialogInstance@`. All four were held back by
`WAVE7_LANE_B.md` §2a under one heading: *"eight allocate or free — split ownership,
and no oracle can be built."*

**Branch** `wave10/lane-ui3`, cut from `wave10/base` at `94ca43a`
("WAVE 10: merge lane ALLOC").
**Worktree** `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-a647525f903e05f94`.

**Verdict: all four PORTED and REGISTERED.** Verification status per row is in §6, and it
is not uniform — read that table before reading this one as four green ticks.

---

## 0. Which conclusions depend on `BANISHED_RT_ALLOC=1`

**None of them.** This is the first thing to state because the lane brief asks for it and
because the answer is structural rather than empirical.

Nothing in `runtime/src/ui/dialog_create.cpp` reads `BANISHED_RT_ALLOC`. The one
allocation these four bodies make (`??0DialogInstance`'s 0x18-byte `LinkedList` node) and
the three array growths they trigger all go through

```
?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z   0x1800c7a50
?MemFree@MemoryAllocator@System@@SAXPEAX@Z      0x1800c7ad0
```

**reached BY ADDRESS**, which is the discipline `runtime/src/collection/array.cpp`
established three waves ago and states in its own banner. That address resolves to

* the **DLL's** allocator when `BANISHED_RT_ALLOC` is unset, and
* **our ported** allocator when it is armed — and in *that* configuration lane ALLOC's
  atomic swap has patched the DLL's own `Allocate`/`Free` prologues, so the DLL's
  internal frees follow us too.

**There is no configuration in which our body and the DLL's `~DialogInstance` see
different heaps.** That is the entire "split ownership" objection, and it does not survive
either setting of the flag. What lane ALLOC's GO buys this lane is not correctness — it is
the *removal of a standing NO-GO verdict* on the code path we were told not to depend on.

The lane's runs were nonetheless done under both settings (§5), because "structurally
fine" is a prediction and this project has paid for treating predictions as measurements.

---

## 1. All four names, read out of the shipped export directory FIRST

Before a line of body was written, with `tools/pe_exports.py` against
`C:\GOG Games\Banished\Runtime-x64.dll` (4,926 exports). A name the DLL does not contain
resolves to NULL in silence, the port still registers, and the census still counts it
(`reference_resolveexport_phantom_names.md`).

| address | exported name |
|---|---|
| `0x180114870` | `??0DialogInstance@UI@@QEAA@PEAVUserInterface@1@PEBVDialog@1@V?$InternalPtr@VDialogInstance@UI@@@Object@@PEAVBase@5@PEAXH@Z` |
| `0x180113470` | `?AddController@DialogInstance@UI@@QEAAXIPEAVBase@Object@@V?$ExternalPtr@VDialogControllerConfig@UI@@@4@V?$ExternalPtr@VElementDescription@UI@@@4@AEBVString@System@@@Z` |
| `0x1801133b0` | `?AddController@DialogInstance@UI@@QEAAXIPEAVBase@Object@@V?$ExternalPtr@VDialogControllerConfig@UI@@@4@V?$InternalPtr@VElement@UI@@@4@@Z` |
| `0x1801137c0` | `?StartTimer@DialogInstance@UI@@QEAAXHVTime@System@@@Z` |

All four present, at exactly the addresses `WAVE7_LANE_B.md` §1 recorded. **No phantom
this time.**

The same query resolved every callee, and it changed one plan: **four of the "unexported
internals" this family reaches are named exports**
(`reference_grep_cannot_tell_internal_from_unported_export.md`):

| address | name | disposition |
|---|---|---|
| `0x1800bfd10` | `??0Internal@Object@@QEAA@XZ` | called by name |
| `0x1800031f0` | `?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z` | called by name |
| `0x1800c0390` | `?IsDerivedFrom@Base@Object@@QEBA_NI@Z` | **already ported** (Wave 3 lane A); called through the seam |
| `0x1800c98a0` | `?StrCompare@String@System@@SAHPEBD0@Z` | called by name |
| `0x180115650` | `?FindElement@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z` | called by name |

**Also checked, and the reason it matters:** `?_typeInfo@DialogController@UI@@0VBase…` and
`?_typeInfo@GroupElement@UI@@0VBase…` — the two statics these bodies read a type id out of
— **are both in the export table.** `0V…A` is the mangling for a PRIVATE static, and this
DLL exports its private members; a comment justifying an address door with the word
"unexported" would have been wrong here exactly as it was three times in one file in Wave 9
(`reference_aeaa_is_private_not_unexported.md`). There is **no address door** in this file
that is not one of the four subjects or the two allocator statics.

**Grepped for an existing port before writing one, by ADDRESS and by MANGLED NAME**, over
`code/`, `engine/`, `runtime/` and `standalone/`: none of the four is registered anywhere.
(`reference_grep_for_the_port_before_writing_it.md` — three times in one wave a lane ported
what the tree already had, and a duplicate registration is last-binding-wins, silently
making one body dead code.)

---

## 2. Two unexported grow helpers — and why registering them was not new work

`?StartTimer@` tail-calls `0x180076250`; the 4-arg `?AddController@` tail-calls
`0x180076370`. Neither is an export. **Neither needed porting**, because each is
byte-identical to a body `runtime/src/collection/array.cpp` already registers:

| candidate | reference (already ported) | spec | differing instructions |
|---|---|---|---|
| `0x180076250` | `0x18005fb00` | `kSpec_Node18` (stride 0x18) | **0** |
| `0x180076370` | `0x180076900` | `kSpec_Fill8Z` (stride 8) | **0** |

"Identical" is a **measurement**, not an impression. `tools/ui3_normdiff.py` strips the
leading instruction address and rewrites every **internal** branch target as a
body-relative delta — so every operand naming anything *outside* the body has to match
**absolutely**. That is the `reference_position_only_diffs_hide_one_body.md` discipline:
a raw byte diff of two copies of one function reports ~23 % different because every
rel32/RIP field encodes a *distance*.

All five relocatable operands in each pair resolve to the same four absolutes:
`0x1800c7a50` (MemAlloc), `0x1801fc718` (`?_singleton@MemoryAllocator@`), `0x18014b390`
and `0x18014b388` (Enter/LeaveCriticalSection), `0x1800c6c00` (Free).

### The instrument was controlled

```
--- SUBJECT: 0x180076370 vs ported 0x180076900 (expect 0) ---
A insns=77  B insns=77
differing instructions: 0
--- NEGATIVE CONTROL: 0x180076370 (stride 8) vs 0x180076250 (stride 0x18) (expect >0) ---
A insns=77  B insns=83
differing instructions: 62
```

A comparator that reports 0 for everything is not a comparator. It reports **62** for two
bodies that differ only in element stride, so it is not structurally incapable of saying
"different" (`reference_diagnostic_field_cannot_express_the_answer.md`: the test is *could
this field express the answer if the answer were different?*).

The script and all four listings are archived beside the numbers, in
`docs/runtime_port/wave10_lane_ui3/` and `tools/ui3_normdiff.py`
(`reference_same_tool_both_sides_is_not_enough.md`).

The two `RegisterRuntimeAddr` lines live **in `array.cpp`, beside the specs they reuse**,
not in this lane's file — the same call lane GFX3 and lane UIF each made, for the same
reason: a private second copy of `SetSizeCore` in the UI tree is the drift that
parameterisation exists to prevent.

---

## 3. Three findings from the disassembly that a plan on paper would have got wrong

### 3a. The 6-arg `AddController`'s stack temporary is a `UI::Dialog`, **not** an `ElementDescription`

The body inlines a whole default construction and destruction at `RSP+0x40`. The obvious
reading — this is the `ElementDescription` the argument names — is **wrong**, and one byte
says so: the third vtable it stamps is

```
0x1801562a0  = ??_7Dialog@UI@@6B@
```

whereas `??0ElementDescription@UI@@QEAA@XZ` (0x180115a40, already ported in `dialog.cpp`)
stamps `0x1801561c0 = ??_7ElementDescription@UI@@6B@`. Getting it wrong would have put an
object of the wrong class on the description's slot-8 call.

The inlined ctor and dtor were then compared against the two exported bodies:

* `??0Dialog@UI@@QEAA@XZ` (0x180112db0) — identical field-for-field; the only difference is
  that the inlined form zeroes `+0x20`/`+0x28` with one `MOVDQA` instead of two `MOV`s.
* `??1Dialog@UI@@UEAA@XZ` (0x180112e40) — identical: same four `~Array` calls at `+0x78`,
  `+0x68`, `+0x58`, `+0x48`, the same `_spriteName` release at `+0x28`, the same tail-jump
  to `~External`.

So the port calls those two exports. **Calling them IS retail's body minus the inlining** —
the identical argument `array.cpp` makes for `MemAlloc`.

This also raises `Dialog`'s known extent: the ctor writes a qword at **+0x80**, past the
last field `runtime/types/ui/Dialog.h` declares (`_references` at +0x78). Recorded, not
acted on — this lane does not own that header.

### 3b. Two of the "unexported internals" `WAVE9_LANE_UIF.md` declined `CreateInstance` on are J1 array dtors already in the tree

`0x180009460` and `0x180039440` — reached from `0x18011363f` / `0x180113649` inside the
6-arg body — are already `J1_ENTRY` registrations in `array.cpp`, put there by Wave 9 lane
UIC *from reading this very function*. They are `~Array`, not opaque internals. The other
two (`0x180039360`, `0x180039280`) are the same family and are reached only through
`??1Dialog@UI@@`, which we now call rather than re-inline — so they never became this
lane's problem.

### 3c. `System::Time` is passed BY VALUE in R8, and both timer fields get the same value

`?StartTimer@` stores R8 to `record+0x00` and then reloads **the same home slot**
(`[RSP+0x60]`) for `record+0x08` (0x1801137eb / 0x1801137f3). Two fields, one value —
which is what `DialogInstance.h` means by calling the second one "the reload".

---

## 4. Faithfulness: five things a tidy port would have destroyed

1. **`?StartTimer@` appends a duplicate on a hit.** It finds an existing entry with the
   same id, rewrites its two `Time` fields, and **falls through and appends a second entry
   anyway.** No `return`, no `else`. `WAVE7_LANE_B.md` §6c wrote this down for whoever
   landed the body; this is that landing, and the behaviour is reproduced.
2. **The 4-arg `AddController` dereferences a null on a factory miss** (`0x1801133f4`
   loads the zero register into RAX and `0x1801133f7` immediately reads `[RAX+0x10]`) **and
   again when the constructed object is not a `DialogController`** (the `CMOVNZ` at
   `0x18011341e` leaves the target at zero and `0x180113422` writes through it). Both are
   reproduced. CLAUDE.md: retail's unguarded derefs are load-bearing — a guard does not
   restore the behaviour, it skips it.
3. **The 6-arg `AddController` calls `SetSize` on `target+0xa0` and then ORs
   `[target+0x34]` with `target` possibly null**, and the OR is a **do-while**, so a null
   target faults rather than skipping.
4. **The 6-arg's name test compares lengths first**, and its "lengths equal" arm re-tests
   the pointer and the length a second time before calling `StrCompare` — so an
   empty-but-equal-length name matches without a compare. Transcribed as written.
5. **`?StartTimer@`'s appended record leaves bytes +0x14..0x17 unwritten.** Retail builds
   the fill on the stack with three stores; the 0x18-byte copy in the grow helper carries
   whatever the fourth word held. Ours does the same, and the oracle **excludes** those
   four bytes per element — comparing them would fail 100 % of cases and read as a
   catastrophic port defect (`reference_oracle_must_not_compare_unwritten_bytes.md`).

### And one defect this port actually committed, found before the first run

**`runtime/types/ui/Dialog.h` NAMES `DialogControllerConfigEntry`'s two `ExternalPtr`
FIELDS THE WRONG WAY ROUND, and the first draft of the ctor's controller loop trusted the
names.** The header calls +0x08 `_config` and +0x10 `_elementDesc`. The shipped bytes say
the opposite about where they go:

```
180114a13  MOV RAX,[R9+0x08]  ->  180114a17  MOV [RSP+0x20],RAX   = ARGUMENT 5
180114a1c  MOV R9,[R9+0x10]                                       = ARGUMENT 4
```

Argument 4 is the `ExternalPtr<DialogControllerConfig>` and argument 5 is the
`ExternalPtr<ElementDescription>`. So **+0x10 is the config and +0x08 is the
description** — the reverse of the field names.

`Dialog.h`'s own note above the struct warns that this is "a transcription slip" **while
itself carrying one**, which is why the warning did not prevent the mistake: it says *the
config ExternalPtr at +0x08 is passed after the element-description ExternalPtr at +0x10*,
which describes the argument order correctly and the field identities backwards. A reader
who takes the field names as the fact and the comment as confirmation gets it wrong twice
and feels checked.

**The port swaps them at the call site, in the open, with the two `MOV`s quoted**, rather
than renaming another lane's fields. And the mapping is now pinned to the shipped bytes by
three entries in the ctor's `[shape]` table, added *because* of this — so it cannot
silently regress and the next reader is told which of the two sources is load-bearing.

**For whoever owns `Dialog.h`:** `_config` and `_elementDesc` should swap names, and the
comment above the struct should be rewritten to name the fields by OFFSET rather than by
role. Not done here; it is a different lane's header and it has other readers.

---

## 5. The oracles

`RT_DIFF_SHADOW` was **not** used. All ~250 of those sites take the port's own verdict and
never consult the DLL, so a shared misunderstanding of retail passes every time
(`reference_shadow_marker_is_not_an_oracle.md`).

### (a) Synthetic-subject differential — `?StartTimer@` only

Two private, byte-identical subjects. **Ours** runs on one, **the shipped body at
`0x1801137c0`** runs on the other, and the `_timers` header plus every element is compared.
The DLL is the authority and it runs *for real*; nothing live is touched, so there is no
"our body already did it, so retail's guard took the early-out" hole.

The "theirs" door is **`ResolveRuntimeRaw`, deliberately** — going through the seam would
resolve to *our own body* once registration has happened, turning the differential into a
tautology (`reference_resolveaddr_consults_port_registry_first.md`).

Nine cases, driving arms a live boot does not reach: first append into an empty array, the
capacity-growth arm, an id **already present** (retail's duplicate append), the duplicate
hit twice, two more growth crossings, and a negative id with a negative time.

**It carries a negative control**: the same nine cases are re-run with a deliberately
defective body (the id comparison inverted) and the round must be caught. A probe whose
corruption slips through was never measuring anything
(`reference_probe_needs_an_effectiveness_control.md`). The control's subject is chosen
**inside the compared set** — the `_timers` array is exactly what the comparator reads —
rather than from a superset of it (`reference_control_must_pick_a_compared_subject.md`).

**Scheduling:** the differential allocates, so it must not run from the register function —
that executes inside `RuntimePortInit()`, right after `LoadLibrary`, when the DLL's
`MemoryAllocator` is *constructed but not started*. It is **armed** at registration and
runs on the first call into any of the four ports, which is the self-scheduling
`array_internal.h` already uses and for the identical reason.

### (b) Shipped-byte displacement pins — all four

Every structure offset, immediate and callee address these bodies compile in is a literal
in the shipped instruction stream. `PinDisp` locates each **by pattern**, requires the
pattern to occur **exactly once** in the body, and compares the decoded value against the
constant the port actually uses. The authority is the DLL.

Locating by pattern rather than by a hardcoded byte offset is the whole point: a hardcoded
table is exactly what made `uic_bytediff.py`'s first version decode four bytes one position
off and print a confident wrong mismatch about a body that was known good
(`reference_recheck_must_match_its_own_proof.md`). Where a short pattern turned out to be
ambiguous inside the body — the AVL `MOV RAX,[RAX+disp8]` pair is the same three opening
bytes for LEFT and RIGHT — the pattern was **lengthened with the neighbouring
instruction**, never shortened into an offset.

Where even that fails (`MOV EDX,0x18` — a bare `BA` byte is everywhere), the check
degrades to `PinPresent`, which puts the value *inside* the pattern and asserts the
instruction occurs exactly once. That is weaker, it says so in its own message, and it is
not dressed up as the stronger check.

#### The pins are pre-flighted OFFLINE, and that is not ceremony

`tools/ui3_pin_preflight.py` reads `Runtime-x64.dll` straight off disk and runs the same
search over the same four bodies — no game directory, no lock, no launch:

```
40 pins, 0 FAILED

-- NEGATIVE CONTROL (read this before the 40 lines above) --
   REJECTED   control: wrong value (off by one)
   REJECTED   control: absent pattern
   REJECTED   control: ambiguous pattern
   the checker DISCRIMINATES (3/3 rejected) -- the pins above mean something
```

The reason this exists: **a NOT-UNIQUE pin reports as a FAILURE OF THE PORT**, so shipping
one costs an evening debugging the instrument instead of the code. It had already paid for
itself twice by hand before it was written — the `_ui` pin matched **seven** instructions
and the `Dialog::_rootDescription` pin matched **two**. The script makes that repeatable
instead of a habit.

It duplicates the **search**, not the oracle: the in-process `PinDisp` still runs against
the loaded DLL. And it exits **2**, not 0, if its own control fails, so a broken instrument
cannot report success.

**What this oracle can and cannot do**, stated so a green board is not over-read: it
catches the entire *transcription* surface — every wrong offset, wrong immediate, wrong
callee, wrong vtable slot. It **cannot** catch a control-flow error. It is a complement to
(a) and (c), not a substitute.

**It carries its own negative control.** `VerifyPinControls()` re-runs `PinDisp` against
the same shipped bytes with (i) a value deliberately off by one, (ii) a pattern that is
absent, (iii) a pattern that occurs many times — and all three must be **rejected**. The
control line prints before the four `[shape]` lines and says outright to ignore them if it
fails.

Each `[shape]` site reports **once**, at registration. A `calls=N` row on one of them is
**N pins, not N invocations** — the note `WAVE7_LANE_B.md` §4 attaches to its shape checks,
repeated because it is misread every time.

### (c) What is NOT covered

Said out loud rather than left to be inferred from silence:

* The ctor's `dialog->_rootDescription->vtbl[8]` call and the 6-arg's `desc->vtbl[8]` call
  build whole Element trees **inside the DLL**. Nothing here models their writes; only the
  pointer they hand back and what we do with it are checked.
* **`??0DialogInstance` and both `AddController` overloads have NO differential oracle in
  this lane.** They are covered by (b) plus live exercise. §6 records that as
  *unverified-by-differential*, which is not the same as verified-clean
  (`reference_uncalled_port_is_not_verified.md`).
* The `??0Dialog@UI@@` / `??1Dialog@UI@@` temporary is verified as *being* those two
  exported bodies (§3a), not re-derived.

---

## 6. Verification status, per row — **read this before the verdict**

| row | ported | registered | shape pins (b) | differential (a) | live exercise |
|---|---|---|---|---|---|
| `?StartTimer@` | ✅ | ✅ | ✅ | ✅ **9 cases + negative control** | see §7 |
| `?AddController@ (…IntPtr<Element>)` | ✅ | ✅ | ✅ | ⛔ none | see §7 |
| `?AddController@ (…ExtPtr<ED>, String&)` | ✅ | ✅ | ✅ | ⛔ none | see §7 |
| `??0DialogInstance` | ✅ | ✅ | ✅ | ⛔ none | see §7 |

The three ⛔ rows are the honest result of the constraint that defeated the replay oracle
in the first place: running any of them twice constructs two controllers or two Element
trees. A synthetic-subject differential for them is *possible* — it needs a real type id
and a real `ExternalPtr<ElementDescription>` harvested at a live call, and it leaks two
controllers per round — and it is the obvious next step for whoever wants it. **This lane
did not write one it could not also run**, which is the same sentence Wave 7 lane C used
and the same reason.

---

## 7. Runs

`docs/runtime_port/wave10_lane_ui3/run_ui3.sh`, invoked as

```
tools/game_dir_lock.sh run lane-UI3 2400 -- docs/runtime_port/wave10_lane_ui3/run_ui3.sh
```

**The measured binary is FROZEN at commit `0036201`, debug exe md5
`e7f948d327ce1aafa4553877ddff02e2`.** No source is edited after this point, because a lane
that keeps improving after it starts measuring invalidates its own numbers and the
integrator's build (`reference_lane_edits_after_report_invalidate_the_measurement.md`).
`run_ui3.sh` re-derives and prints both md5s at every launch anyway, so the log says which
binary produced each number rather than this doc asserting it.

| # | label | configuration | what it establishes |
|---|---|---|---|
| 1 | `default` | `BANISHED_RT_DIFF=1 BANISHED_AUTOSTART=1`, `BANISHED_RT_ALLOC` **unset** | the ports are correct in the **default** configuration — the run the §0 claim stands or falls on |
| 2 | `armed` | same **+ `BANISHED_RT_ALLOC=1`** | the ports are correct with the ported allocator armed |
| 3 | `killswitch` | `BANISHED_RT_DLGCREATE=0` | **the attribution control.** If the switch does nothing, a clean run 1 says nothing about what was live |

### Two run-hygiene rules this lane follows, and why they are not boilerplate

**All three launches share ONE lock hold.** Three separate `run` invocations would give up
the lock between them, and another lane deploying in a gap would make runs 2 and 3 measure
a different binary from run 1.

**`verify` and the md5 are asked before every launch, and they are different questions.**
`verify lane-UI3` says *we still hold the lock*; `md5sum` on the deployed exe says *the
bytes in the game directory are still our build*. Both have failed in this project. A
mismatch **aborts** rather than being caveated.

> **Lock-pipeline audit (integrator advisory, 2026-09-08 23:0x).** This lane did use the
> unsafe `game_dir_lock.sh acquire … | tail` form twice, and it did produce the misleading
> `exit code 0` on a run that had actually printed `TIMEOUT`. **No clobber resulted**,
> because the printed text was read rather than the status, and because this lane had
> deployed and launched nothing at that point. Everything from that point on goes through
> `run` / `verify` on the hardened script (`wave10/base` `f9a9a9c`), which is taken into
> this branch.

---

## 7b. The census re-measure — **the count moved by the wrong amount, and the SET says why**

`tools/runtime_census.py --unported --namespace UI`, A/B'd by moving
`runtime/src/ui/dialog_create.cpp` out of the tree and back:

```
BEFORE: 29 UI unported rows
AFTER:  27 UI unported rows
```

Four rows were closed. The count moved by **two**. A closure count that moves the right
direction by the wrong amount reads as success and hides whatever opened
(`reference_diff_the_set_not_the_count.md` — 129→127 where 126 was expected hid a phantom
mangling a lane had just invented). So the **set** was diffed, not the count:

**LEFT (4 — this lane's entire brief):**

```
??0DialogInstance@UI@@QEAA@PEAVUserInterface@1@PEBVDialog@1@V?$InternalPtr@…@Z
?AddController@DialogInstance@UI@@QEAAXIPEAVBase@Object@@V?$ExternalPtr@…@4@V?$ExternalPtr@VElementDescription@UI@@@4@AEBVString@System@@@Z
?AddController@DialogInstance@UI@@QEAAXIPEAVBase@Object@@V?$ExternalPtr@…@4@V?$InternalPtr@VElement@UI@@@4@@Z
?StartTimer@DialogInstance@UI@@QEAAXHVTime@System@@@Z
```

**ENTERED (2):**

```
??0Dialog@UI@@QEAA@XZ     0x180112db0
??1Dialog@UI@@UEAA@XZ     0x180112e40
```

**This is `reference_porting_can_grow_the_cutover_surface.md`, and it is not a regression.**
Those two bodies were *always executed* on this path — retail **inlines** them into the
6-arg `AddController` (§3a). They were invisible to the census only because nothing in our
tree named them. The port replaces an **inlined, unnamed** dependency with a **named,
scopeable export**, which is strictly the better position: the census can now see it, and
it is two small, well-understood bodies rather than an opaque stretch of another function.

**Recipe for whoever closes them** (deliberately not done here — it is not this lane's
brief and doing it would have meant landing two more `J1_ENTRY` registrations on a
different file's oracle):

* `??0Dialog@UI@@QEAA@XZ` is 0x2b instructions of field zeroing plus three vtable stamps
  (`??_7Base@Object@@6B@`, `??_7External@Object@@6B@`, `??_7Dialog@UI@@6B@`, all exported).
  It writes a qword at **+0x80**, past the last field `runtime/types/ui/Dialog.h` declares,
  so that header needs one more member first.
* `??1Dialog@UI@@UEAA@XZ` calls four `~Array` bodies: `0x180009460` and `0x180039440` are
  **already registered** `J1_ENTRY`s in `array.cpp`; `0x180039360` and `0x180039280` are the
  same family and are not. Then a `System::String` release and a tail-jump to `~External`
  (`0x1800bc950`).

Net for the UI namespace: **29 → 27**, and the two entrants are a smaller, better-defined
problem than the four that left.

## 8. Registration, switch, and blast radius

Registered from `RtUiDialogCreateRegister()` in
`runtime/src/ui/dialog_create.cpp`, called from `runtime/src/ui/rt_ui_port.cpp` behind
**`BANISHED_RT_DLGCREATE`** (unset = ON, the convention every other UI lane in that file
uses; `=0` is the kill switch).

Each row is registered under **both** doors — `RegisterRuntimeExport(<literal>)` and
`RegisterRuntimeAddr(<address>)` — because both are live in this tree:
`engine/funcs/engine_funcs.cpp` resolves `fn_DialogInstance_Ctor` (2869),
`fn_DialogInstance_AddControllerTab` (2974) and `fn_DialogInstance_AddController6` (2977)
through `ResolveAddr(Module::Runtime, …)`, which consults the port registry **first**. So
registering is what makes the sixteen exe-side `fn_DialogInstance_Ctor(...)` call sites in
`code/Game/Dialog/` run our body.

Every mangled name in the registration block is repeated **as its own string literal**
rather than through a named constant: `tools/runtime_census.py` cannot see through a named
constant and would under-count the lane
(`reference_census_cannot_see_nonliteral_registrations.md`).

**Blast radius is large and it is stated rather than minimised.** `??0DialogInstance` is on
the construction path of every dialog in the game, and the 6-arg `AddController` runs for
every controller of every one of them. That is precisely why the row has its own switch and
why §6 does not claim more verification than it has.
