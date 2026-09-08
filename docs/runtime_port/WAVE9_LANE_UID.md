# Wave 9 lane UID — the dialog lifecycle

Branch `wave9/lane-uid`, cut from `fd30180`. Assignment: the four symbols the
DLL-absent boot still asks for around one dialog's life —
`?ShowDialog@UserInterface@UI@@`, `?CloseDialog@UserInterface@UI@@`,
`??1DialogInstance@UI@@UEAA@XZ` and `?OnClosed@DialogInstance@UI@@`.

**Result in one line: the TEARDOWN half is closed, the OPEN/CLOSE half is
declined on a blocker I measured rather than inherited, and three of the five
blockers two earlier lanes wrote down had already dissolved.**

---

## 1. What landed

| retail | symbol | door(s) | oracle | liveness |
|---|---|---|---|---|
| `0x180114d40` | `??1DialogInstance@UI@@UEAA@XZ` (ord 574) | name **and** address | shape, 6 values | `worked=3` |
| `0x1800325f0` | `Collection::LinkedList<T=8>::Clear` (unexported) | address only | shape, 4 values | `worked=2` |
| `0x18002fca0` | `Collection::Array<8>::SetSize` (unexported) — **registration only, the body was already ported** | address only | — (body is Wave 3's) | — |

Files: `runtime/src/ui/dialog.cpp`, `runtime/src/collection/linkedlist.cpp`,
`runtime/src/object/internal.cpp`.

---

## 2. The three stale blockers, and how each died

Lane UIF (row 6) and lane UIC (§7) both declined `??1DialogInstance`, and lane
Z declined `?OnClosed@`. Between them they named five blockers. I re-checked
every one against the bytes and the tree rather than carrying them
(`reference_holdback_blockers_are_stale_check_them_first.md`). **Three were no
longer true.**

### 2.1 "`0x18002fca0` (int)" — it was already ported, and had been for six waves

`ArrayPtrSetSize` in `runtime/src/object/internal.cpp:239` carries the comment
`// FUN_18002fca0 — Array<Internal*>::SetSize(int newSize), no fill value.`
directly above it. The body is the port. **What was missing was the
`RegisterRuntimeAddr(0x18002fca0, …)`**, so `ResolveAddr(Module::Runtime,
0x18002fca0)` fell through to the DLL and would have been a null call at
cutover.

This is `reference_grep_for_the_port_before_writing_it.md` read from the other
side. That memory says: grep before you write, because the tree may already
have it. The mirror-image failure is what happened here — **a HALF-REGISTERED
port is indistinguishable, from a callee census, from an absent one**, and two
lanes in a row recorded it as unported work. The fix is one line.

Registered **ungated**, and the reasoning is in the code: this body is a
container primitive operating on whatever array its caller passes. It touches
the handle table only when the caller passes `_singleton->_objects`, which only
`~Internal` does — and that one stays behind `BANISHED_RT_OBJECT_TABLE`.
Registering the primitive arms no handle-table mutation of its own.

### 2.2 "`0x1800325f0` (int, list free-all)" — ~20 lines, now ported

The fourth member of the `Collection::LinkedList<T=8>` family lane UIC opened.
Walk the chain freeing nodes, then zero tail/head/count. Three details are
load-bearing and are transcribed rather than tidied: `next` is read BEFORE the
free; it frees NODES, not the `InternalBasePtr` elements; the three stores are
unconditional, `count` is a dword, and `_pad14` is untouched.

### 2.3 The one that mattered: "the body OPEN-CODES the InternalFactory swap-remove inline … no oracle … CORRUPTS THE HANDLE TABLE"

Both lanes rested their decline on this, and both were right that it would be
the dangerous thing to reimplement. **It does not need reimplementing, because
it is not a second implementation.**

Reading `??1Internal@Object@@UEAA@XZ` (0x1800bfdb0) beside the dtor's tail
(0x180114e64) shows the SAME INSTRUCTION STREAM, register allocation aside —
the full side-by-side is in the comment above the body. Same vtable stamps
(`0x180154720`, `0x180154788`), same `[0x1801fc6d8]` singleton read, same
swap-remove, same `CALL 0x18002fca0`. Retail INLINED `~Internal` here.

So the port does what `??1DialogController@UI@@` (Wave 7, 60 lines up in the
same file) already does for the identical tail: it calls the ONE body the tree
owns, through the seam. **Exactly one handle-table mutator is ever in play**,
and it is whichever one `BANISHED_RT_OBJECT_TABLE` has armed.

⚠ **The coupling is real and is stated, not hidden.** With the DLL present and
that tier off (the default), this dtor's tail is the DLL's `~Internal`, which is
correct. At cutover the tier must be armed. WAVE9.md already records the table
tier as "Opt-in, and **MANDATORY at cutover**", so this adds no new obligation —
but it does add a caller that will fault loudly if that promise is broken.

---

## 3. Verification — what it proves and what it does not

`??1DialogInstance` is a destructor, so **there is no replay oracle and there
cannot be one**: a replay is not a comparison, it is a second destruction —
every child's deleting destructor again, both arrays freed twice, a swap-remove
of a handle that no longer exists. It ships in the no-oracle disposition lanes
M, Z and UIF established, with **no call into the shipped body at all**.

What stands in for it is a **SHAPE CHECK** in the sense Wave 7 lane B's
`VerifyDispatchLoop` established. For a destructor whose every other operation
is a store of zero, six values ARE the failure surface, and all six are literal
relocations in the shipped instruction stream:

| checked | ~DialogInstance | LinkedList::Clear |
|---|---|---|
| vtables stamped | `??_7DialogInstance` / `??_7Internal` / `??_7Base` | — |
| callees | `0x1800325f0`, `0x18002fca0` | — |
| field displacements | `_children` +0x38 | `next` +0x10, `tail` +0x08, `head` +0x00, `count` +0x10 |

Both read the shipped bytes through a **RAW** door (`ResolveExportRaw` /
`ResolveRuntimeRaw`). Raw is mandatory and is not an oversight of the port-aware
rule: an oracle resolving through the registry would read OUR OWN prologue once
the port registers and compare the port against itself — the exact defect lane
FNHOOK found in thirteen detour installers. `tools/detour_target_audit.py`
stays clean.

### The negative control, and it FIRES

Per rule 6 and `reference_probe_needs_an_effectiveness_control.md`, a shape
checker that cannot be seen to fail is worth nothing — and this wave has
already paid twice for controls that came back green because they could not
fire. `VerifyLaneUidControls()` feeds each decoder a body that is deliberately
**not** its own (`??1DialogController` for the dtor decoder — a real destructor
of the same family, so it exercises the decoder rather than bouncing off a
null; `0x18007a010` `LinkedList::Remove` for the list decoder — same file, same
container, different displacements) and **requires a mismatch**. It also
requires both control bodies to have resolved, so "the control could not run"
cannot read as "the control passed".

### Measured, gameplay cell, `BANISHED_RT_DIFF=1`

Gated on this lane's own identity line, read from that run's own log
(`standalone_debug.log`), not on "I rebuilt" and not on a timestamp:

```
[RtPort]   WAVE9 LANE UID ARMED -- ~DialogInstance (0x180114d40) + LinkedList::Clear
           (0x1800325f0) + the Array<8>::SetSize address door (0x18002fca0).
```

Cell: `BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 BANISHED_PAUSEMENU=600
BANISHED_PAUSEMENU_QUIT=120`. It reaches **gameplay**, not the menu — the
distinction WAVE9.md's own withdrawn finding turned on. Evidence in the log:
`[AutoStart] sequence fired`, `[PauseMenu] new world … at frame 1`,
`[PauseMenu] frame 601 - opening`, then quit-to-menu and quit. `rc=0`,
**zero-byte `standalone_crash.log`**, `[RtResolve] 0 misses`,
`[RtResolveEarly] 0`.

```
UI::DialogInstance::~DialogInstance[shape]         calls=1   divergences=0
Collection::LinkedList::Clear(0x1800325f0)[shape]  calls=1   divergences=0
WAVE9 lane UID shape decoders [negative control]   calls=1   divergences=0   <- PASS
UI::DialogInstance::~DialogInstance [liveness]     calls=0   worked=3
Collection::LinkedList::Clear(0x1800325f0)         calls=0   worked=2
```

`calls=0` on the two live bodies is correct and expected: nothing is compared,
so `worked=` is the whole signal (`reference_uncalled_port_is_not_verified.md`
in its second costume — this is the case where a zero `calls=` is honest, and
the doc says which column to read).

Raw `[RtDiff]` output for both cells:
`docs/runtime_port/wave9_lane_uid/uid_{gameplay,twogames}_rtdiff.txt`.

---

## 4. HONEST LIMITS

1. **`worked=3` is thin, and I could not raise it.** A second cell
   (`BANISHED_AUTOSTART_GAMES=2`) produced the identical `worked=3` / `worked=2`.
   Three destructions is real traffic and it is not much traffic. The ceiling
   looks structural, and it is the same shape as lane W's finding: **the DLL
   tears down its own dialogs through direct near-CALLs the registry cannot
   intercept**, so only the dialogs OUR code destroys — the exe-side
   `MainMenuDialog` family, whose vtable slot 0 is our `RT_DELDTOR` — reach this
   body. At cutover every dialog is ours, so the branch coverage on cutover day
   is strictly wider than anything measurable today. *That is lane W's
   "unverifiable by construction" class, and I am reporting it rather than
   arguing round it.*
2. **A shape check is not a behaviour check.** It proves the shipped body stamps
   the vtables I stamp, calls the functions I call, and reads the field I read.
   It does NOT prove my loop ordering, my re-read of `_controllers._data` inside
   the loop, or that I pass flag 1 to slot 0. Those are argued from the
   disassembly in the code comment and are not machine-checked. I did not
   manufacture an oracle for them, because the only one available is a second
   destruction.
3. **The `??1Internal` seam moves the risk, it does not remove it.** If the
   handle-table tier is armed at cutover and Wave 3's `~Internal` port is wrong,
   this dtor now delivers that wrongness to every dialog. The mitigation is that
   the tier is one body with its own shadow oracle, not two.
4. **I did not re-verify the `??_7DialogInstance` mint row.** Lane Z's ⚠ note
   that `?_typeInfo@DialogInstance@UI@@` is missing from
   `rt_data_mint_tables.inc` is untouched by this lane and still stands.
5. **The census `called` column grew.** 1003 → 1005 called, 771 → 774 ported.
   Both new `called` rows are addresses this port reaches (`0x1800325f0`,
   `0x18002fca0`) and both are registered by this same commit, so nothing moved
   into the unregistered column — but WAVE9.md's "porting can grow the cutover
   surface" applies and the numbers are given in full in §6 rather than
   summarised as "coverage went up".
6. **One divergence in both runs is NOT mine and I did not fix it:**
   `Object::Factory::Startup [probe, not diffable] ours=chain=416 nodes=415`.
   That is lane UIC's open 415-vs-416 question, present in every cell.
7. **Environment, recorded because it nearly cost the measurement.** The
   worktree this lane was first given (`.claude/worktrees/lane-addr`) is shared
   with several other lanes and was reset under this session twice, deleting an
   edit. A first round of audit numbers was taken in it and was measuring
   another lane's tree; **those numbers were discarded and every figure in §6 was
   re-taken in a private worktree with the working directory verified in the same
   command.** Lane ENGIO's files also leaked into this worktree three times
   (`input_device_update*.cpp`, and tracked edits to `rt_input_port.cpp` and
   `system/configuration.cpp`); their content is preserved at
   `scratchpad/laneENGIO_leaked_tracked.patch` and as `*.laneENGIO-leaked.bak`,
   the tracked edits were reverted, and the build measured here contains none of
   it. **This is the `reference_lane_edits_after_report_invalidate_the_measurement.md`
   hazard arriving from a direction that document does not cover: not the lane
   editing after it reports, but ANOTHER lane editing the tree being measured.**

---

## 5. DECLINED — three rows, each with the blocker I measured

### 5.1 `?OnClosed@DialogInstance@UI@@` (0x180115330) — declined, and the reason CHANGED

The body is trivial: `SaveSettings(this)` then a controller dispatch loop on
slot +0x30. Lane Z declined it because `?SaveSettings@` (0x180114240) is
unported; lane UIC added that `SaveSettings` "builds a `MemoryStream`
(0x1800ae710)" and dispatches through the `IO::Encoding` vtable, and lane W
recorded that the shared encoding vtable at `0x1801554a8` "has no exported name
and **no row the mint can carry**".

**Two of those three are now stale, and the third is the real answer:**

* `??0MemoryStream@IO@@QEAA@XZ` (0x1800ae710) **IS PORTED** — lane PKG2,
  `runtime/src/io/compressedstream.cpp:953`.
* `0x1801554a8` **IS MINTED** — carried by address, 5 slots,
  `standalone/rt_data_mint_tables.inc:1318`.
* **But all five of its slots are `RT_MINT_UNNAMED`, i.e. named traps.** The
  five bodies are `0x1800a81e0`, `0x1800a81d0`, `0x1800a81c0`, `0x1800a8120`,
  `0x1800a8080`, and **none of them is registered anywhere in the tree.**

`SaveSettings`'s first act after constructing the stream is
`CALL [encodingVtable + 0x18]` at `0x1801142cf` — **slot 3, `0x1800a8120`** —
with `+0x20` (`0x1800a8080`) on the other arm of the same branch. So porting
`SaveSettings` today would not close this row: it would reach a trap stub two
instructions later.

**This is a better lead than the row it replaces, and it is shared.** Those two
functions are `IO::Encoding`'s CreateReader/CreateWriter. Porting the pair
unblocks `SaveSettings` → `OnClosed`, **and** it is the same wall lane W
measured live on the first save and the first load
(`CompressedMemoryStream::CompressedMemoryStream`, `calls=1` in both cells).
One pair of unexported functions, three rows.

### 5.2 `?ShowDialog@UserInterface@UI@@` (0x18011fd10) — declined; **and one inherited blocker is WRONG**

~200 instructions. It allocates a **second, nested `UserInterface`** (0x128
bytes), copies eight pointer fields from the parent, registers two callbacks
into `Engine::MainLoop`'s update and draw arrays (`[0x1801fe750]+0x80/+0x88`
and `+0x90/+0x98`, both inlined array-grows with the allocator lock), and then
**PUMPS ITS OWN MODAL LOOP** — `CALL 0x1800dd100`
(`?Frame@MainLoop@Engine@@`) until the dialog leaves the open-dialog chain —
before unregistering both callbacks, calling `~UserInterface` and freeing.

Measured callee status:

| callee | status |
|---|---|
| `0x180120de0` `UserInterface::UserInterface` | **PORTED** (`runtime/src/ui/userinterface.cpp:1054`) |
| `0x18007a0d0` `Array<16>::SetSize(n, &v)` | **PORTED** |
| `0x1800c6e60` / `0x1800c6c00` allocator | **PORTED** |
| `0x1800dd100` `Engine::MainLoop::Frame` | **unported** — and WAVE8_LANE_B records `Run`/`Frame` as detour-required holdbacks |
| `0x180120b90` `~UserInterface` | **unported** |
| `0x1801213c0` | **unported**, unexported |
| `0x1801203f0`, `0x180120310` | callback thunks, **unported**, unexported |

⛔ **Correction to a fact both lane UIF and lane UIC published.** Their tables
list `?EndDragDrop@UserInterface@UI@@` (0x180120af0) among `ShowDialog`'s
unsatisfied callees. **`ShowDialog` does not call it.** `0x180120af0` appears
nowhere in `0x18011fd10`–`0x1801202ff`; it is called from `CloseDialog`
(`0x18012104f`), which is the row directly below it in both tables. Their
depth-4 figure for `ShowDialog` is therefore built on a callee that belongs to
the next row. The decline still stands on `Frame` and `~UserInterface`, but the
evidence for it was partly borrowed.

**Not attempted, deliberately.** Porting the modal pump means owning
`MainLoop::Frame`, which Wave 8 held back for a detour reason, plus
`~UserInterface`, which is another lane's class. It is a lane, not a row.

### 5.3 `?CloseDialog@UserInterface@UI@@` (0x180120f20) — declined

~200 instructions, recursive over child dialogs, with **two separate inlined
linked-list unlinks each followed by an allocator free**, a 256-entry tooltip
table walk, and a swap-down compaction of the dialog array. Measured callees:
`0x18011fc60` `?UpdateEvents@UserInterface@UI@@` (twice, **unported**),
`0x180120af0` `?EndDragDrop@` (**unported**), `0x180033690` (**unported**,
unexported, an `Array<0x30>::SetSize(n+1,&event)` posting a synthetic event
`{id 0xd, -1, …}`), against `0x18007a0d0` and `MemFree` which are ours.

Declined on `UpdateEvents` specifically: it is the head of the whole UI event
dispatch and is lane UIC/UIW territory, not a helper I can close inside this
lane. Porting `CloseDialog` without it relocates the null by one frame — the
failure lane UIF's row 14 is the clearest illustration of.

---

## 6. Gate numbers — before and after, same worktree, both re-taken

Every figure below was produced in this lane's private worktree with `pwd`
verified in the same command, and the "before" set was taken with the three
files stashed (`grep -c RtUiDialogInstance_Dtor dialog.cpp` = 0 confirmed in
the same command).

| metric | before | after | Δ |
|---|---:|---:|---|
| `port_bypass_audit` ADDRESS door bypasses | 0 | **0** | — |
| `port_bypass_audit` NAME door bypasses | 0 | **0** | — |
| `rawdoor_audit` UNGUARDED-CALL | 0 | **0** | — |
| `rawdoor_audit` sites / DEFERRED-CALL | 608 / 67 | 617 / 66 | +9 / −1 |
| `detour_target_audit` | clean | **clean** | — |
| `addr_door_audit` **UNREGISTERED** | 16 | **16** | **0** |
| `addr_door_audit` call sites / distinct / registered | 295 / 198 / 1186 | 297 / 200 / 1189 | +2 / +2 / +3 |
| `unsatisfied_export_scan` unsatisfied | 129 | **128** | **−1** |
| `unsatisfied_export_scan` live (outside a diff guard) | 126 | 125 | −1 |
| `runtime_census` called / ported / coverage | 1003 / 771 / 76.9% | 1005 / 774 / 77.0% | +2 / +3 / +0.1 |

**Set-diff of the unsatisfied-name list, computed rather than eyeballed:**

```
removed: ??1DialogInstance@UI@@UEAA@XZ
added:   (none)
```

**One door closed, zero opened.** The two new `called` addresses are the two
this port reaches, and both are registered by the same commit — which is why
`UNREGISTERED` is flat at 16 rather than 18.

`rawdoor_audit`'s +9 sites are the two shape oracles and the negative control;
all nine are inside `if (!RtDiffEnabled()) return;` and the audit classifies
them as guarded (UNGUARDED-CALL stays 0).

---

## 7. Next moves in this cluster, in the order I would take them

1. **`IO::Encoding`'s CreateReader/CreateWriter — `0x1800a8120` and
   `0x1800a8080`.** Two unexported functions behind three rows: `SaveSettings`
   → `?OnClosed@` (this lane), and lane W's measured first-save / first-load
   cutover blocker. **The highest-leverage pair left in the UI/IO seam**, and
   the reason is now measured rather than argued: the vtable is minted, the
   stream ctor is ported, and only these five slots are traps.
2. **`?SaveSettings@DialogInstance@` (0x180114240)**, immediately after item 1.
   It becomes a one-function job at that point, and `?OnClosed@` becomes a
   ten-line job after it.
3. **`?UpdateEvents@UserInterface@UI@@` (0x18011fc60)** gates `CloseDialog` and
   `UserInterface::Update` (lane UIF rows 1 and 4) together.
4. **`?Frame@MainLoop@Engine@@` (0x1800dd100) + `~UserInterface` (0x180120b90)**
   are what `ShowDialog` is really waiting on. Both are other lanes' classes.

---

## 8. Addendum — the three `DialogInstance` ADDRESS doors, and a re-run under the fixed audit

Added after the lane's first commit (`7e3726f`), in response to three follow-ups
from the integrator. Recorded as a **follow-up commit, never an amend**.

### 8.1 Provenance and worktree

* Worktree: `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\lane-uid-089e6eb4`
  — **private, created by this session**, not the shared `lane-addr` checkout.
* Branch `wave9/lane-uid`, tip `7e3726f`, **parent `fd30180`** — verified before
  and after committing.
* `git diff --stat fd30180 wave9/lane-uid` contains **only this lane's six
  files**; no neighbour's work is in it.

### 8.2 All three doors are LIVE CODE, not prose — checked by reading them

The integrator's warning about `addr_door_audit.py` matching comments was worth
acting on: `engine/funcs/engine_funcs.cpp` mentions two of these addresses in a
comment block at lines **2964 and 2966**, which is exactly the shape that
produced 25 phantom "call sites" tree-wide. The live sites are elsewhere:

| address | line | verdict |
|---|---|---|
| `0x180114870` | `engine_funcs.cpp:2869` | **live** — `fn_DialogInstance_Ctor = …ResolveAddr(Module::Runtime, 0x180114870));` |
| `0x1801133b0` | `engine_funcs.cpp:2974` | **live** — inside `if (!fn_DialogInstance_AddControllerTab)` |
| `0x180113470` | `engine_funcs.cpp:2977` | **live** — inside `if (!fn_DialogInstance_AddController6)` |

The fixed audit agrees: all three appear in its 13-row `--csv` UNREGISTERED set
at those three line numbers, and the comment rows at 2964/2966 are gone.

### 8.3 DECISION: all three DECLINED, with the closure measured

Measured by disassembling each body and checking every direct callee against
the tree's registrations. `(unreg)` = no `RegisterRuntimeAddr`/`RegisterRuntimeExport` anywhere.

**`?AddController@…@InternalPtr<Element>@…` — 0x1801133b0, ord 1542 — the near miss.**

Callees: `0x1800c0390` `IsDerivedFrom` **(ours)**, `0x180076370` **(unreg)**,
and two virtual dispatches. So it is a **depth-1 row**, and the one blocker is
small:

> **`0x180076370` is a THIRD MSVC instantiation of `Collection::Array<T=8>::SetSize(int, const T& fill)`** — the same template the tree already ports twice (`ArrayPtrSetSizeFill` = FUN_18002ffa0 in `runtime/src/object/internal.cpp:213`, and `0x18007a0d0` for the 16-byte element). Transcribing it side by side, it is `ArrayPtrSetSizeFill` **plus one dead `*slot = 0` store before the real one** (`MOV [RDX],R11` at `0x180076406`, immediately overwritten at `0x18007640c`) — the same dead-store-kept shape `0x180032750` has. It is ~30 lines of already-understood code.

**I still declined it, and the reason is not depth.** `0x1801133fb` is
`CALL qword ptr [RCX + 0x20]` — the Object::Factory creator's vtable slot 4 —
and **its arity is undetermined**: `RDX`, `R8` and `R9` are all live with
leftovers from the AVL search at the call, so a port that passes one argument
and a port that passes four are both consistent with the call site. That is
lane UIF row 11's rule verbatim (*"declining an unidentified vtable slot is
cheaper than guessing one"*), and it is the same hazard
`reference_hidden_retslot_wild_write.md` is about. Resolving it means reading
the creator implementations, which belong to `Object::Factory`, not here.

Two facts a future port of this row **must** keep, recorded because both look
like bugs:
* **A missing type id is a retail HARD FAULT.** When the AVL search falls off
  the end, `0x1801133f4` sets `RAX = 0` and `0x1801133f7` immediately does
  `MOV RCX,[RAX+0x10]` — a read of address `0x10`.
* **A failed `IsDerivedFrom` is also a hard fault.** `CMOVNZ RDI,RBX` at
  `0x18011341e` leaves `RDI = 0`, and the next instruction writes
  `[RDI+0x10]`. Retail asserts the factory returns a `DialogController`.
  Neither may be "fixed" with a null check.

**`?AddController@…@ExternalPtr<ElementDescription>@…,String&` — 0x180113470, ord 1541.**
Four unsatisfied callees of its own — `0x1800c98a0`, `0x180039360`,
`0x180039280`, `0x180006b80` — plus a tail `CALL 0x1801133b0` that inherits the
row above and its blocker. `0x180039360` / `0x180039280` are the **two "J2"
array destructors** lane UIC named as the remaining blockers of
`?CreateInstance@ElementDescription@`; they gate both rows, so whoever ports
them closes two things at once.

*(On the integrator's warning that the two overloads differ only in argument
count: counted against the mangled names, ord 1542 is 5 arguments
`(uint, Base*, ExternalPtr<Config>, InternalPtr<Element>)` and ord 1541 is 6,
`(uint, Base*, ExternalPtr<Config>, ExternalPtr<ElementDescription>, String const&)`.
`engine_funcs.cpp` already binds them to two different typedefs,
`fn_DialogInstance_AddControllerTab` and `fn_DialogInstance_AddController6`,
and the six-argument one is the phantom-mangling casualty WAVE7_LANE_B.md §5
documents — so the trap is live in this exact pair and the existing typedefs
are the right side of it.)*

**`??0DialogInstance@UI@@` — 0x180114870, ord 147 — the constructor.**
Its **last call is `CALL 0x180113470`** (`0x180114a27`), so it inherits row
1541's entire closure, and adds `0x1800031f0` **(unreg)** and a virtual
`CALL [RAX+0x40]`. Its first call is `0x1800bfd10` `??0Internal@Object@@`,
registered **only inside the gated `BANISHED_RT_OBJECT_TABLE` tier** — the same
coupling this lane's destructor already carries and documents.

### 8.4 On "a dtor whose ctor still runs shipped bytes is a layout risk"

Taken seriously and answered rather than waved off. **It is not a risk created
by this port**, for three reasons:

1. The layout is not inferred at either end — `runtime/types/ui/DialogInstance.h`
   `static_assert`s every offset the destructor touches, and Wave 7 built it from
   the constructor's own disassembly.
2. **The shape check corroborates it against the shipped bytes**: the destructor's
   `_children` displacement decoded out of retail's instruction stream is `+0x38`,
   which is what the header says. That is retail's own constructor and our
   destructor agreeing on a field, measured, not assumed.
3. Wave 7 §3's "two implementations, one live object" argument applies unchanged.
   At cutover BOTH must be ours — but that is a constructor lane, which is what
   §8.3 declines, not a hazard the destructor introduces.

The genuine risk in the pair is the opposite direction and is already recorded:
if `BANISHED_RT_OBJECT_TABLE` is armed at cutover and Wave 3's `~Internal` is
wrong, this destructor delivers that to every dialog (§4 limit 3).

### 8.5 Gate numbers re-run under the FIXED `addr_door_audit`

**§6's address-door row was produced by the OLD audit** (the one with no notion
of a comment). Re-taken with the fixed script, `git show f5dbeb8:tools/addr_door_audit.py`,
both states measured in this worktree — base by `git checkout fd30180 -- <the three files>`,
then restored:

| `addr_door_audit` | before | after | Δ |
|---|---:|---:|---|
| **fixed** (`f5dbeb8`) call sites | 270 | 271 | +1 |
| **fixed** distinct addresses | 192 | 193 | +1 |
| **fixed** registered addresses | 1188 | 1191 | +3 |
| **fixed UNREGISTERED** | **13** | **13** | **0** |
| *old (for comparison)* | *295 / 198 / 1186 / 16* | *297 / 200 / 1189 / 16* | *— / 16 flat* |

**Both versions give the same verdict on this change: one door closed, zero
opened, UNREGISTERED flat.** The denominators moved a lot between tool versions
and not at all because of this port — which is the distinction
`reference_bypassed_port_needs_a_source_checker.md` exists to keep visible. All
other gate figures in §6 are unaffected by the audit fix (different tools).
