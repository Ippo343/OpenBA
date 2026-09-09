# Wave 10B — Lane SHOW — `?ShowDialog@UserInterface@UI@@`, and the route into the nested pump

Worktree `C:/code/GIT/untitled_rev_eng_project/.claude/worktrees/agent-abec24dee7b746b37`,
branch `wave10b/lane-show` (off `wave10/base` @ `453e26d`).

**Row CLOSED.** `?ShowDialog@UserInterface@UI@@QEAAHV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z`
(retail `0x18011fd10`, ordinal 4034) is ported, registered under both doors, and
verified by a differential that was **proven capable by three separate negative
controls**, one per screen.

**The route is the bigger deliverable.** `standalone/modal_driver.cpp` is a
general `MainLoop+0x90` nested-pump callback. It is not specific to `ShowDialog`
and it drives the shipped DLL's body as readily as ours — which is exactly what
made the differential possible.

---

## 1. Was the stated blocker accurate?

`WAVE10_UI_CENSUS.md` §6 and `WAVE10_LANE_UI1.md` §4 recorded:

> `BANISHED_MODAL_ANSWER` is NOT a route — it is a BYPASS. Both harness call
> sites skip `fn_UI_ShowDialog` entirely.
>
> `ShowDialog` registers `UpdateStandAloneUI` into `MainLoop+0x90`, which the
> nested pump walks every frame — a harness callback in that array reaches
> inside the loop no driver can currently touch.

**The MECHANISM was exactly right and re-derives cleanly. The ENUMERATION was
wrong, and it was wrong in the direction that made the row look harder than it
was.**

| claim | verdict |
|---|---|
| the nested pump is unreachable by any existing harness tick | **TRUE.** `MainLoop::Frame(this, param_1)` calls `MainLoop::Update` only inside `if (!param_1)` (0x1800dd28e), and `ShowDialog` calls `Frame(_singleton, true)` (0x180120167 sets `DL=1`). Every harness tick in the tree hangs off `MainLoop::Update`. |
| a callback in `MainLoop+0x90` is walked every nested frame | **TRUE**, and it is walked on every *fixed-timestep tick*, not every frame — the walk at 0x1800dd2a3..0x1800dd2d6 is inside `Frame`'s `do { } while (accum >= step)`. It runs in the outer loop too, so one arming covers the whole process. |
| "**both** call sites skip `fn_UI_ShowDialog` entirely" | **FALSE.** There are **four** call sites. |

The four:

| call site | bypassed? |
|---|---|
| `code/Game/Dialog/moddialog.cpp:703` | only when `BANISHED_MODAL_ANSWER` is set |
| `code/Game/Dialog/saveloaddialog.cpp:951` | only when `BANISHED_MODAL_ANSWER` is set **or** the run is unattended |
| `code/Game/Dialog/optionsdialog.cpp:1646` | **NO — unconditional real call** (input-bind capture) |
| `code/Game/Dialog/optionsdialog.cpp:1759` | **NO — unconditional real call** (binding-conflict confirm) |

So the body was never unreachable; both bypasses are *conditional*, and two call
sites have no bypass at all. What did not exist was a way to make the nested pump
**terminate** unattended. That is the whole of the real blocker, and it is now gone.

Method note: UI1's sentence was checked by grepping `fn_UI_ShowDialog` across
`code/`, not by inheriting the count. This is the third stale-or-partial blocker
this project has recorded in the past two waves
(`reference_holdback_blockers_are_stale_check_them_first`).

---

## 2. What `ShowDialog` actually does with `MainLoop+0x90`

Read off the shipped bytes. Twelve steps; the full trace with addresses is the
banner of `standalone/modal_driver.cpp`.

1. `MemAlloc(0x128, 4)` a **second** `UserInterface` and construct it (0x180120de0).
   Alignment is **4**, not 8 (`MOV R8D,0x4` at 0x18011fd59); this pins `sizeof` at 0x128.
2. copy `this->[+0x90 .. +0xc8]` — eight qwords — into the nested one. Retail does
   this **unconditionally**, including on the path where the allocation returned
   null. Kept unguarded.
3. `dialog->_ui (+0x10) = nestedUI`; `dialog->_flags (+0x20) |= 0x20`.
4. call the dialog's vtable slot 4 (`CALL [RAX+0x20]`, 0x18011fe2a).
5. `Array::SetSize` push the dialog into `nestedUI->_addDialogs (+0x28)`.
6. `UserInterface::Update(nestedUI, Time(0))`.
7. append `{DrawStandAloneUI(0x1801203f0), nestedUI}` to **MainLoop+0x80**.
8. append `{UpdateStandAloneUI(0x180120310), nestedUI}` to **MainLoop+0x90**.
9. **while** the dialog is still in `nestedUI->_dialogs`: `MainLoop::Frame(_singleton, true)`.
10. compact both arrays, removing its own two entries.
11. `~UserInterface(nestedUI)` (0x180120b90) + `MemFree`.
12. **return `*(int*)(dialog + 0x24)`**.

### Facts Ghidra's C dropped or misleads on

* **`|= 0x20` is NOT the modal bit.** `?IsModal@DialogInstance@UI@@QEBA_NXZ`
  (0x180114170) reads `(flags >> 1) & 1`. The bit `ShowDialog` sets is read by
  `?DeleteOnClose@DialogInstance@UI@@QEBA_NXZ` (0x180114110) as
  `!((flags >> 5) & 1)` — **inverted**. Setting it makes `DeleteOnClose` FALSE:
  the modal dialog survives its own close so the caller can read the result out
  of it. `optionsdialog.cpp` depends on exactly that, reading `dialog+0xC0/+0xC4`
  *after* `ShowDialog` returns and only then `DeletePtr`ing.
* **`+0x24` is the modal RESULT, not `_flags2`.** `?GetResult@DialogInstance@UI@@QEBAHXZ`
  (0x1800455c0) is two instructions, `MOV EAX,[RCX+0x24]; RET`;
  `?SetDialogResult@` (0x1800455d0) is `MOV [RCX+0x24],EDX; RET`. Every
  `MessageDialog` button writes it (`messagedialog.cpp` Slot10..13 → 1/0/2/3).
  `runtime/types/ui/DialogInstance.h` called it *"`_flags2` … no ported body
  reads it"*; the comment is corrected there. The **identifier** is deliberately
  left alone rather than churn another lane's `dialog_create.cpp` mid-wave —
  a one-line follow-up.
* **The return re-reads the InternalPtr.** 0x1801202e2 is
  `MOV RAX,[RDI]; MOV EAX,[RAX+0x24]` with RDI the reloaded argument, not a
  value cached at entry. Likewise the loop test reloads it every iteration
  (`MOV RCX,[RDI]` at 0x180120150, *inside* the loop).
* **The loop has two exits**, not one: the dialog leaving the list, **and**
  `Frame` returning true. The second is why a quit request does not wedge behind
  a modal.

### The retail quirk that dictated the driver's shape

`Frame`'s walk of the callback array, at 0x1800dd2b0:

```
MOV RDX, [RDI+0x80]        ; DRAW array base
MOV RAX, [RDI+0x90]        ; UPDATE array base
LEA RCX, [RDI+0x48]        ; arg1 = &fixedTimestep
MOV RDX, [RBX+RDX+8]       ; arg2 = DRAW[i].ctx     <-- the DRAW array
CALL [RBX+RAX]             ; fn   = UPDATE[i].fn    <-- the UPDATE array
```

**The function comes from `+0x90` and its context from `+0x80`, at the same
index.** This is in the disassembly, not a decompiler artefact. It is invisible
in retail because `ShowDialog` is the array's only writer and appends to both in
lockstep with the same context, so `DRAW[i].ctx == UPDATE[i].ctx` for every `i`.

Two consequences, both load-bearing for anything that ever uses this route:

* **(a)** a harness callback in `+0x90` **cannot receive a context**. It gets
  whatever sits at the same index of `+0x80`. State must live elsewhere.
* **(b)** a harness callback in `+0x90` **must be paired with an entry in
  `+0x80`**. If `+0x90` is longer, `MOV RDX,[RBX+RDX+8]` reads past the draw
  array — and if `+0x80` was never allocated it is NULL, making that a read of
  `0x8 + i*0x10` and an access violation **inside retail's frame loop**.

---

## 3. The route, and how it is switched

`standalone/modal_driver.{h,cpp}`, armed from `MenuDLT::Stub_Update`
(`standalone/menudialogloopt_stubs.cpp`, beside the existing `AutoStart::TickMenu`).

```
BANISHED_MODAL_DRIVER=<answer>[,<ticks>]     off unless set
```

* Appends a **pair** — a no-op into `MainLoop+0x80` and `Tick` into
  `MainLoop+0x90` — through the shipped allocator, in lockstep, per quirk (b).
* `Tick` ignores both arguments, per quirk (a).
* Each tick it scans `+0x90` for an entry whose fn is `UpdateStandAloneUI`;
  that entry's ctx **is** the nested `UserInterface`, and its `_dialogs` head
  gives the front dialog. Detection is by signature, so it works identically
  whether the `ShowDialog` that registered it was the DLL's or ours — the
  property the differential depends on.
* After `<ticks>` (default 3) consecutive sightings it dismisses the modal with
  **exactly what a real button click does** (`messagedialog.cpp` Slot10..13):

  ```
  *(uint32_t*)(dialog + 0x24) = answer;
  fn_UI_CloseDialog(*(void**)(dialog + 0x10), InternalPtr(dialog));
  ```

  `*(void**)(dialog+0x10)` is the nested UI, because step 3 put it there — so
  `CloseDialog` reaches the nested UI the same way a real button does. Retail's
  `CloseDialog`, removal list, `CleanRemovedDialogs` and loop-exit test all run
  untouched. This is a synthetic **click**, not a synthetic dialog system.
* `<answer>` may be `0`: unlike every other `BANISHED_*` switch, `=0` **arms**
  rather than disables, because 0 is a legitimate dialog result (Cancel).
  Unset it to disable.
* Cost when unset: `Enabled()` short-circuits before anything else, and the
  in-game cell below confirms zero `[ModalDriver]` lines and no behaviour change.

Reusable beyond this lane: any work that needs to reach inside a nested
`MainLoop::Frame` — other modal call sites, the `optionsdialog` input-bind path,
`CloseDialog` coverage — now has a place to stand.

### `autostart.cpp` was NOT touched, and here is why

The census doc lists as still-open:

> `autostart.cpp` checks `BANISHED_AUTOQUIT` before `BANISHED_AUTOSTART` and
> returns early, so "get in-game" and "quit cleanly" are mutually exclusive.

**The env-order fact is TRUE** (`AutoStart::Init`, `standalone/autostart.cpp:129`,
returns inside the `BANISHED_AUTOQUIT` block before `BANISHED_AUTOSTART` is ever
read). **Its stated consequence is FALSE.** "Get in-game and quit cleanly" is
already routine: `standalone/pausemenu_harness.cpp` drives the in-game pause menu
to Quit Game, and `sa-stress` uses precisely that on every run
(`stress_harness.cpp:508` sets `BANISHED_PAUSEMENU_QUIT=60`,
`BANISHED_PAUSEMENU_BUTTON=6`). This lane's in-game cell used
`BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=60
BANISHED_PAUSEMENU_BUTTON=6` and exited 0 with a 0-byte crash log.

So the fix was **not** what unblocked the clean exit, and `autostart.cpp` — which
four other lanes' runs go through — is left untouched. The env-order oddity
should be recorded as a wart, not as a blocker on coverage.

---

## 4. The port

`runtime/src/ui/userinterface_showdialog.cpp`, registered by
`RtUiShowDialogRegister()` from `runtime/src/ui/rt_ui_port.cpp` under switch
**`BANISHED_RT_UISHOW`** (default ON, `=0` falls back to the DLL), under **both**
the mangled name and the address `0x18011fd10` — a name-only registration is dead
at any call site reaching the DLL through the address fallback while an oracle
over it still reports clean (WAVE7.md §3).

Grepped before writing, by address and by mangled name across `code/`, `runtime/`,
`engine/`, `standalone/`: the only prior reference was the `ResolveExport` in
`engine/funcs/engine_funcs.cpp:4029`, i.e. a call site, not a port. No duplicate
registration.

### Doors

Every callee was checked against the shipped export directory with `pefile`
rather than assumed — `UpdateStandAloneUI` and `DrawStandAloneUI` mangle as
`CAXA…`, i.e. private **statics**, and this DLL exports its private members
(`reference_aeaa_is_private_not_unexported`). Eight of nine have name doors and
are reached by name.

**One address door, declared as a cutover obligation**
(`reference_by_address_door_dies_at_cutover`):

| address | symbol | status |
|---|---|---|
| `0x18007a0d0` | `Collection::Array<InternalPtr<DialogInstance>>::SetSize` | **not exported.** Already ported and registered by `runtime/src/collection/array.cpp` (`J3_ENTRY3(18007a0d0, kSpec_Fill8Z)`), reached through `ResolveAddr(Module::Runtime, …)`, which consults the port registry first. **Satisfied today** — unlike a fresh door. |

The census confirms this cost nothing: `called` stayed at **170** (UI) and
**1083** (total). The port added no new cutover surface.

---

## 5. Verification — the twin-subject differential

A **predictive** oracle over this body is worth almost nothing, and UI1 was right
about why: everything it writes other than one flag bit is an address `MemAlloc`
chose, so the compared set would be one bit. Recording that as "verified" is the
partial-oracle-read-as-full failure.

The route makes a better instrument available. With a driver that terminates the
modal, `ShowDialog` becomes **repeatable**, so we build two identically-constructed
`MessageDialog`s and show one through `ResolveExportRaw` (the shipped DLL body,
bypassing the port registry) and one through `ResolveAddr` (whatever the registry
resolves), then compare the post-state each leaves. Two distinct dialog objects —
nothing aliased, nothing replayed onto dirty state
(`reference_oracle_must_snapshot_aliased_inputs`).

`BANISHED_MODAL_DRIVER_TEST=1`. Subject: `Dialog/SaveLoad.rsc:confirmDelete`.

### Compared

| screen | why it is a real signal |
|---|---|
| **`return`** | the whole contract of step 12, and it is end-to-end: it can only match if the nested UI was built, the dialog reached its list, the pump ran, both callbacks were registered and walked, and the loop exited on the right test |
| **`flags`** | step 3's `\| 0x20` — the one bit a predictive oracle could have reached |
| **`drawCount` / `updCount`** | back at their pre-call values, i.e. step 10's compaction. A leaked entry would have retail's `Frame` calling into a **freed** nested UI on every subsequent tick |
| **`dismissed`** | the effectiveness counter (below) |
| **`ui!=0`** | step 3 wrote the nested UI into `dialog+0x10`; compared for non-nullness only, since the value is an address `MemAlloc` chose and *must* differ |

### NOT compared — stated so the pass is not over-read

The nested `UserInterface`'s 0x128 bytes (destroyed and freed before return —
unreadable by construction), the allocator arena, and everything the vtable
slot-4 call and `DialogInstance::Update` did to the dialog's interior. The
dialog-construction path itself is shared by both cells and so cancels out rather
than being verified.

### Results

Run `diff` (`BANISHED_AUTOQUIT=1 BANISHED_MODAL_DRIVER=2 BANISHED_MODAL_DRIVER_TEST=1`),
exe exit 0, **0-byte crash log**:

```
THEIRS(raw DLL)=00007ffcab5afd10  OURS(registry)=00007ff7...  (distinct -- a real differential)
round 0: 6/6 screens equal; both cells returned the injected result 2; dismissed theirs=1 ours=1
```

**`dismissed theirs=1 ours=1` is the effectiveness control**
(`reference_probe_needs_an_effectiveness_control`). It is the counter that
distinguishes "the differential is clean" from "nothing ever entered the nested
pump and equal-and-unexercised read as a pass". Had it been `0/0` the instrument
prints `INCONCLUSIVE` and refuses the pass. **It is the `calls=`-column analogue
for this instrument, and it is non-zero in both cells.**

### The negative controls — scored per screen, not on the union

Three defects injected into **our** body only (`g_RtShowDialogDefect`, zero in
every shipping configuration), each naming the single screen that must catch it.
Scoring the union would read 3/3 while hiding a blind screen
(`reference_union_control_hides_a_blind_screen`).

| defect | what it does | screens that fired |
|---|---|---|
| 1 | leak one **benign** callback pair (a no-op, not the real entries — a control that crashes the subject measures nothing) | `drawCount` `updCount` |
| 2 | return the dword at `+0x28` instead of the result at `+0x24` | `return` |
| 3 | set an extra flag bit alongside `0x20` | `flags` |

```
VERDICT: round0 mismatches=0, controls caught=3/3 -> PASS (differential clean AND proven capable)
```

Each control fired **only** its intended screen — no screen is blind, and none is
so noisy that it fires on everything.

### The instrument's own control

Run `killsw`, same command plus `BANISHED_RT_UISHOW=0`:

```
THEIRS(raw DLL)=00007ffcab5afd10  OURS(registry)=00007ffcab5afd10
  *** IDENTICAL -- the port is NOT registered; this run compares retail with itself
      and proves nothing ***
VERDICT: round0 mismatches=0, controls caught=0/3 -> INCONCLUSIVE
[RtPort] UI: 165 exports registered     (vs 166 with the switch on)
```

This is the shape that has burned this project repeatedly — an "ours" side that
reaches the DLL and reports a clean oracle over nothing
(`reference_registering_an_address_converts_a_selfcheck_into_an_oracle`). The
instrument **detects and announces it** rather than printing a green line, and
`controls caught=0/3` is the corroborating signal (with no port registered, the
defect injection is dead code). It also demonstrates the kill switch works.

### Integration

Run `ingame` (`BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=60
BANISHED_PAUSEMENU_BUTTON=6`), the **default** configuration — port on, driver
**off**:

* world built, 1,284 draw-side log lines, pause menu opened at frame 601, Quit
  Game dispatched at 661, exe exit 0, **0-byte crash log**
* `[RtResolve] 0 misses`
* **zero `[ModalDriver]` lines** — the route costs nothing when unset

All three targets build clean: `standalone-debug`, `standalone` (release).

### The honest limit

`ShowDialog` is **verified against retail on the six compared screens, with the
instrument proven capable on each of them separately and proven non-vacuous by
`dismissed=1/1`.** It is *not* a claim that the nested `UserInterface`'s interior
matches, nor that the four production call sites were exercised — the subject was
a synthetic (but entirely real) `MessageDialog` built the same way
`SaveLoad_ShowConfirmDialog` builds its own.

---

## 6. Census set-diff

`tools/runtime_census.py --unported --namespace UI`

| | before | after |
|---|---|---|
| rows | 6 | **5** |

**LEFT (1):** `?ShowDialog@UserInterface@UI@@QEAAHV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z`

**ENTERED (0):** none.

Remaining five, all `??_7` vtables owned by other lanes: `??_7Dialog@UI@@6B@`,
`??_7DialogController@UI@@6B@`, `??_7DialogInstance@UI@@6B@`, `??_7Element@UI@@6B@`,
`??_7SpriteSheet@UI@@6B@`.

Full census (the set-diff above is a **lower bound** on residue — `--namespace UI`
cannot see an unexported callee at all, per lane DLG2):

```
UI      total 1625  called 170  ported 165  detour 5   97.1%   (was 164 / 96.5%)
TOTAL   total 4632  called 1083 ported 870  detour 9   80.3%   (was 869 / 80.2%)
```

`called` did **not** move on either row — the port opened no new cutover surface.

---

## 7. Changed files

| file | what |
|---|---|
| `standalone/modal_driver.h` / `.cpp` | **new** — the `MainLoop+0x90` route and the twin-subject differential |
| `standalone/menudialogloopt_stubs.cpp` | +14 lines: arm the driver / run the self-test from the menu tick |
| `runtime/src/ui/userinterface_showdialog.cpp` | **new** — the port |
| `runtime/src/ui/rt_ui_port.cpp` | +9 lines: `BANISHED_RT_UISHOW` |
| `runtime/types/ui/DialogInstance.h` | comment only: `+0x24` is the modal RESULT |
| `docs/runtime_port/WAVE10B_LANE_SHOW.md` | this file |

Switches: `BANISHED_RT_UISHOW` (default ON), `BANISHED_MODAL_DRIVER` (off),
`BANISHED_MODAL_DRIVER_TEST` (off).

## 8. Handoffs

* **Anyone doing nested-pump work** (`CloseDialog` coverage, the `optionsdialog`
  input-bind path, in-game modals): the route exists. Read quirks (a) and (b) in
  §2 before adding a second callback to those arrays — a `+0x90` entry without a
  `+0x80` partner faults inside retail's own `Frame`.
* **Whoever owns `dialog_create.cpp`**: `DialogInstance::_flags2` should be
  renamed `_result`. One line there, one in the header.
* **The census doc**: §6's "get in-game and quit cleanly are mutually exclusive"
  is false — `pausemenu_harness.cpp` already does it and `sa-stress` relies on it.
  The `autostart.cpp` env-order oddity is real but is a wart, not a blocker.
