# Wave 10 — Lane CLOSEDLG — `UI::UserInterface::CloseDialog`

Worktree `C:/code/GIT/untitled_rev_eng_project/.claude/worktrees/agent-a5cd92c41fe2e82ef`,
branch `wave10/lane-closedlg` (off `wave10/base` @ `bed7a3f`).

One row:
`?CloseDialog@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z`,
retail `0x180120f20`, ordinal 1725.

**Deliverable, stated up front so nothing below reads as more than it is: a
byte-faithful port, plus an oracle that covers roughly half the function's
observable effects and REFUSES to run on the other half. Both halves are
counted.** The lane brief said to expect "port with a mostly-excluded oracle,
not port and verify". The oracle turned out better than that — two of the five
effects lane UI1 listed as unreachable are in fact compared — but it is still
partial, and the partition is the point of this document.

---

## 1. The handoff, and which parts of it held

Lane UI1 (`WAVE10_LANE_UI1.md` §5) left this row with its blockers triaged:

| UI1's claim | verdict here |
|---|---|
| allocator NO-GO is a **conflation** — this body's allocator use has exported doors | **held.** Both `MemFree`s go through `?MemFree@MemoryAllocator@System@@SAXPEAX@Z`; no `windows.h` in a `runtime/src` file |
| recursion is **not** a blocker — `RtPredictRound`'s state is a stack | **held for the framework, and it is still a blocker for a different reason.** See §3 |
| the real blocker is oracle **COVERAGE** | **held, but the hole is smaller than stated.** See §4 |
| "two `MemFree`s, a `DynArrayPush`, a vtable release and unlinks in another object's list, **none** expressible by a predictor" | **wrong on one of the five.** The unlinks ARE expressible. See §4 |

---

## 2. The body, and the facts the decompiled C drops

Full transcription in `runtime/src/ui/userinterface_close.cpp`; the file carries
the retail address beside every store. Read off the disassembly
`0x180120f20..0x1801213b6`, not off Ghidra's C
(`reference_ghidra_c_output_vs_disassembly.md`). Ghidra's output is unusually bad
here — it aliases the `RSP+0x80` scratch slot between an `InternalPtr` temporary
and a byte offset, and it rotates the first unlink block so its loop head sits
below a `return`, which makes one piece of code read as two unrelated fragments.

The shape, with everything retail inlines named:

1. **`0x180120f46`** — recurse into every child dialog of `*dlgPtr`, through
   `dlg+0x10` (the child's owning `UserInterface`, not necessarily `this`). The
   loop's bound is `dlg+0x48`, the very count the recursive call decrements.
2. **`0x180120f76`** — if `*dlgPtr` owns the current tooltip, close the tooltip
   and clear `this+0x38`/`this+0x40`.
3. **`0x180120fb3`** — if `*dlgPtr` is already in `_removedDialogs` (+0x18/+0x20),
   return.
4. **`0x180120fd6`** — search the live `_dialogs` intrusive list at `+0x00`.
   * found → `OnClosed`, maybe cancel the drag, unlink from the dialog's parent,
     `UpdateEvents`, append to `_removedDialogs`, then **`Play(1)` inlined**.
   * not found → **`0x1801211f6`** search `_addDialogs` (+0x28/+0x30) instead:
     `OnClosed`, unlink, `UpdateEvents`, clear the tooltip, compact the array,
     decrement the size, append to `_removedDialogs`. **No sound on this path.**

### Eight things the C output gets wrong or drops

1. **A dead compare with a LIVE load**, `0x180120f8a`:
   `MOV RAX,[RCX+0x30] ; CMP RAX,RAX ; JNZ`. `CMP RAX,RAX` is never unequal, so
   the branch is unreachable — but the **load** still happens and still faults on
   a bad `_toolTipDialog`. Same shape UI1 found in `Update`. Reproduced as a
   `volatile` read; dropping it would make our body survive a pointer retail
   dies on.
2. **The two append sites do NOT pass the same pointer.** `0x18012115f` is
   `MOV R8,RBX` — the **list node**, whose first qword is the dialog.
   `0x18012139f` is `MOV R8,R9` — the caller's `InternalPtr`. The eight bytes
   copied are the same either way, so this is not a behaviour difference; it is a
   difference Ghidra prints as one variable (`local_res10`) in both places, and a
   port that "cleans it up" loses the fact that retail had two sources.
3. **A dead store inside the compaction**, `0x18012134b`:
   `MOV [RCX],RBP` (zero) immediately followed by `MOV RAX,[RCX+8] / MOV [RCX],RAX`.
   Both reproduced. The predictor supersedes the first on **range overlap** —
   which only works because lane UI1 fixed `RangesOverlap` in `rt_predict.cpp`;
   on the pre-UI1 instrument this exact pattern is the manufactured-mismatch
   failure UI1 root-caused on `_fadeAmount`.
4. **`TEST RCX,RCX` on `base + i*8`**, `0x180121346` — a null test on an address
   that can only be null if the array base is exactly `-i*8`. Dead, and kept.
5. **The tail is `Play(1)` inlined.** `0x180121167` reads `this+0xa0`, which is
   `_sounds[1]` (the array starts at `+0x98`), then reproduces
   `?Play@UserInterface@UI@@QEAAXH@Z` (`0x1801223d0`) instruction for
   instruction — including `0.0 / _secondsPerTick`, **not** folded to zero, for
   the reason `userinterface.cpp` gives at the real `Play`: were that slot ever
   `0.0`, retail computes NaN and `CVTTSD2SI` yields `INT64_MIN`. Reached ONLY
   from the `_dialogs` path.
6. **`EDI` is 1 at the sound tail**, set at `0x18012111a`, and that 1 is what is
   stored into the channel's `+0x08`. The `_addDialogs` path reuses `EDI` as the
   array index and never reaches the tail, so there is no conflict — but a port
   that hoisted the tail out of the branch would inherit an index there.
7. **The vtable slot is `+0x28` = index 5 = `?OnClosed@DialogInstance@UI@@`**
   (`0x180115330`). Read out of `standalone/rt_data_mint_tables.inc`'s minted
   copy of `??_7DialogInstance@UI@@6B@`, not guessed from the name — it is the
   mirror of `AddDialog`'s slot 4 `OnOpened`. **It is not a refcount release**,
   so the dialog is still alive when this body reads `d+0x28` afterwards, which
   is what makes §4's unlink predictions legal.
8. **The array base is RE-LOADED after the virtual**, `0x180121246`: `OnClosed`
   can reallocate `_addDialogs`. Reproduced rather than hoisted.

### Two exports three lane docs listed as unported callees

`WAVE9_LANE_UIC.md`, `WAVE9_LANE_UIF.md` and `WAVE9_LANE_UID.md` all list
`EndDragDrop` (`0x180120af0`) among this row's unsatisfied callees. It is
`?EndDragDrop@UserInterface@UI@@QEAAXXZ`, **ordinal 2308, a plain exported
name** — confirmed by running the export table rather than grepping our source
(`reference_grep_cannot_tell_internal_from_unported_export.md`). Same for
`UpdateEvents`, ordinal 4515. Neither needed an address door.

### Field names

`runtime/types/ui/Dialog.h` names `DialogControllerConfigEntry`'s two
`ExternalPtr` fields **backwards**, and the header's own warning comment carries
the error. Nothing here touches that struct, and every offset in the port is
written as a raw byte offset with the retail address that proves it —
deliberately. Pin to addresses, never to field names, anywhere near this class.

---

## 3. The recursion IS a blocker — for the oracle, not for the port

UI1 is right that `RtPredictRound` nests: its state is a stack of `kMaxDepth`
slots and an overflow degrades to "round inactive", not to a wrong answer. That
retires the framework objection.

It does not retire this one. **In predict mode a suppressed call decrements
nothing, and this loop's termination condition is exactly what the recursive
call decrements:**

```
while (*(int*)(dlg + 0x48) > 0) { ... CloseDialog(child) ... }     0x180120f46
```

A predicting body that suppressed the recursive call would spin forever. A
predicting body that "just recursed anyway" would write to memory, which is the
one thing a predictor must never do.

So the round is **armed only on calls where neither recursion is entered** — the
dialog has no children, and the tooltip arm's condition is false. Everything else
is handed straight to the DLL and **counted**:

```
[RtPredict] UI::UserInterface::CloseDialog  armed=N  indeterminate=M
```

`indeterminate=` is not a footnote. `rounds=` structurally cannot express it —
an unarmed call never constructs a round, so it is invisible to every counter
`rt_predict.h` prints. Reporting a green board over the armed subset without the
size of the unarmed subset would be the partial-oracle-reading-as-full failure
this project keeps paying for.

---

## 4. The five side effects, and whether the oracle covers each

The lane brief asked for this table explicitly. "Covered" means: our body
predicts a value for that location, and after the DLL has run for real the
prediction is compared against what the DLL left.

**Read this table as a statement about the INSTRUMENT, not as a result.** It says
which effects the oracle is *capable* of comparing. Whether it actually compared
any of them on a given run is a separate question, answered by `armed=` and
`changed=` in §5 — and §5 is where this lane's difficulty turned out to live. A
coverage table quoted without the run counters beside it is exactly the shape of
claim this project keeps having to retract.

| # | side effect | retail | covered? | how / why not |
|---|---|---|---|---|
| 1 | `MemFree(node)` — `_dialogs` path | `0x1801210fc` | **NO** | the node is handed back to the allocator; its post-state is the allocator's, not retail's. `Exclude`d, so the `node->prev = 0` / `node->next = 0` stores retail makes two instructions earlier are recorded for read-back and counted `unverifiable`, never compared |
| 2 | `MemFree(node)` — `_addDialogs` path | `0x1801212f8` | **NO** | same |
| 3 | `DynArrayPush` onto `_removedDialogs` | `0x180121162`, `0x1801213a2` | **NO** | the element lands at an address the allocator chose. `+0x18`/`+0x20` `Exclude`d |
| 4 | the `OnClosed` vtable release | `0x180120ffe`, `0x180121243` | **NO** | a suppressed indirect call. `out.Call` records the **target** — and the target is checkable, since it is a vtable slot both sides load — but not the callee's writes |
| 5 | **the unlinks in another object's list** | `0x180121088..0x1801210cf`, `0x180121284..0x1801212cb` | **YES** | **this is the one UI1 got wrong.** `parent+0x38` (head), `parent+0x40` (tail), `parent+0x48` (count), and the two neighbour fixups `prev->next` and `next->prev` are all computed from pointers this body can read, and all land in memory retail does not free |

Three more effects UI1 did not list, for completeness:

| | side effect | covered? | |
|---|---|---|---|
| 6 | the `_addDialogs` **compaction** (`0x180121337`) | **YES** | the shift loop writes the array's own storage, which is not inside `this` at all. Watched and compared element by element, including the dead zero-store |
| 7 | `_addDialogs._size--` (`0x18012135e`) and the tooltip clear (`0x180121319`) | **YES** | scalars on `this` |
| 8 | the **`Play(1)` sound tail** (`0x1801211d0..0x1801211ee`) | **YES** | five predictions into the audio scene: the channel's sound pointer, state, start tick and handle, plus the scene's counter |
| 9 | `EndDragDrop`, `UpdateEvents`, `Array<UI::Event>::SetSize` | **NO** | three more suppressed calls; `_events` (`+0x48`) and the drag-drop block (`+0x60`) `Exclude`d, the latter **only when a drag is actually in flight**, so a stray write there is still reported on the common path |

### How the graph got under `Watch` at all

UI1's objection to covering #5 was *"a graph whose extent must be enumerated
before `Watch` can see it"*. That is a real constraint and it has a cheap answer:
**the body enumerates it, by calling `Watch` from inside itself once the walk has
found the node.** That is sound precisely because a predicting body writes
nothing — the snapshot is still taken strictly before the DLL runs. The same
trick puts the `_addDialogs` storage and the audio channel slot under the
coverage control.

### And the honest limit

`Exclude` regions are printed as `excluded=` bytes on the site, and every
suppressed call is counted in `calls=`. The effects of `OnClosed`,
`UpdateEvents`, `EndDragDrop`, `Array<UI::Event>::SetSize` and both `MemFree`s
are **not** under this oracle. What IS under it is that our body's own writes to
`this`, to the parent's list header, to the two neighbour nodes, to the
`_addDialogs` storage and to the audio channel match the DLL's byte for byte;
that we make the same suppressed calls; and that the DLL wrote nothing else into
any watched region.

---

## 5. Runs — and the finding that dominates them

Every run below deployed under `tools/game_dir_lock.sh run lane-CLOSEDLG`, with
the deployed exe's md5 compared against this worktree's build immediately before
each launch and `verify lane-CLOSEDLG` re-checked between them.

### 5a. Batch 1 — build md5 `f785a1c8e329b8d5d68a83e54422e5ab`

| run | configuration | exit | crash log | `entries=` |
|---|---|---|---|---|
| A | predict, menu (`BANISHED_AUTOQUIT=1`) | 0 | 0 bytes | **0** |
| B | A + `BANISHED_RT_PREDICT_SELFTEST=1` | 0 | 0 bytes | **0** |
| C | predict, **in-game** (`AUTOSTART`, pause menu at frame 601, Quit at 661) | 0 | 0 bytes | **0** |
| D | **default config** — registered, no detour, no oracle, in-game | 0 | 0 bytes | **0** |
| E | **detour installed** at `0x180120f20`, in-game | 0 | 0 bytes | **0** |
| F | predict + **`BANISHED_RT_ALLOC=1`** (allocator gate armed) | 0 | 0 bytes | **0** |
| G | kill switch `BANISHED_RT_UICLOSE=0` | 0 | 0 bytes | **0** |

### 5b. `entries=0` is the finding, not a footnote

**`UI::UserInterface::CloseDialog` is called ZERO times in a full unattended
session.** Not "the oracle was quiet" — the body did not run.

This is exactly the failure the invocation counter was added to make visible,
and it is worth being precise about what it does and does not mean, because
three different explanations produce the same zero and only one of them is true
here:

* **Not "registered under the wrong door."** `EngineFuncsDump` prints
  `UI::CloseDialog = 00007ff7c3ffd7da`, an address inside our own exe image, so
  `engine_funcs.cpp:1606`'s `ResolveExport` landed on the port. The ordering is
  right by construction too: `standalone/main.cpp` runs `RuntimePortInit()` at
  step 2c, **before** `EngineFuncsInit()` at step 4, and says so in a comment.
* **Not "the DLL's internal callers keep their own body."** Run E installed the
  detour at `0x180120f20` and logged it. The DLL's own callers would have
  arrived. They did not, because there were none.
* **It is "nothing dismisses a dialog on this route."** Run C reached the world
  (`[PauseMenu] frame 601 - opening`, `frame 661 - dispatching OnCommand id=6`),
  so the run is not short. But the **42 exe-side `fn_UI_CloseDialog` call sites
  across 24 files** in `code/Game/` all sit on *dismiss* actions, and `Update`'s own tooltip
  arm needs a tooltip, which needs a mouse hover that an unattended run never
  performs. Menu → New Game → world → Quit closes nothing.

Two consequences worth recording rather than discovering again:

1. **For this row the detour is not a nice-to-have, it is the only route to
   most of the callers** — and even it is dark unless something dismisses a
   dialog.
2. **A lane that had shipped this row on runs A–G would have reported seven
   green boards, a clean detour and a clean allocator column, over a body that
   never executed.** That is `reference_uncalled_port_is_not_verified.md`
   arriving in a configuration where six of the seven counters look healthy.

What batch 1 *does* establish, and it is not nothing: the port compiles into the
shipping configuration, the registration resolves, the detour installs and the
patched DLL runs a full in-game session to a clean exit with a 0-byte crash log,
under the allocator gate as well as without it.

### 5c. Batch 2 — DISCARDED, and why it is written down anyway

Batch 2 took the mods-menu click route (`BANISHED_AUTOMENU_BUTTON=7` with
`…,d0,…,101,…`), which reaches `fn_UI_CloseDialog` at `moddialog.cpp:754` and
`moddetailsdialog.cpp:424`. **Every run in it is invalid and none of its numbers
appear in this document.**

What happened: the advisory lock was **stale-reaped** and granted to this lane
while the previous holder's `reimpl-x64.exe` was **still running**. That process
held `standalone_{debug,error,crash}.log` open, so every launch printed

```
rm: cannot remove '…/standalone_debug.log': Device or resource busy
exe exit=3
```

and died within 3–17 seconds. The lock's `verify` said "still held by
lane-CLOSEDLG" the whole time, and `boot_state.sh --wait` said `RUNNING`, because
neither instrument can see a *foreign* process — they only see the lock file and
*a* running exe. Two green instruments over seven dead runs. The batch then ran
past its own hold and the last two runs failed `verify` outright.

This is worth recording because it is a new shape of the clobber the lock exists
to prevent: **the lock's stale reaper and the previous holder's process lifetime
are not coupled.** Reaping a lock does not reap the game. The lane after the
reap gets a lock it genuinely holds, over a directory somebody else is still
using.

Batch 3 therefore checks the thing the lock cannot: it waits for zero
`reimpl-x64.exe` processes before each run, and **aborts the run if the log
files are still busy** rather than launching into them. That check is the
difference between the discarded batch and a measurement.

### 5c-2. Batch 3 — the route, found

Build md5 `2d348a3e221d0bbab958bd42773436a7`, deployed and md5-matched before
each launch, pre-flight clean.

`BANISHED_PAUSEMENU_BUTTON=1` is **Resume**, and `MainMenuDialog::OnCommand`'s
`kButton_Resume` arm calls `fn_UI_CloseDialog` directly
(`code/Game/Dialog/mainmenudialog.cpp:993`). Run P, in predict mode with the
detour installed:

```
[PauseMenu] frame 601 - opening (… pausesGame=0)
[PauseMenu] frame 661 - dispatching OnCommand(msg=0x12, id=1)
[RtPort] UI::UserInterface::CloseDialog ENTERED (first call) -- the port is LIVE on this route
[PauseMenu] OnCommand returned
[PauseMenu] frame 781 - 120 frames after the id=1 click: pausesGame=0
```

**The port is reached, and the dialog really closed** — `PausesGame` flips from
1 to 0, which is the engine's own answer, read by `GameUpdateLoop::Update` to
decide whether to tick the world. Run Q reproduced it with the detour installed
and the self-test armed.

**This route cannot produce oracle counters, and that is why the probe gate
exists.** Resume resumes the game; nothing quits afterwards, so the run must be
killed, and `entries=`/`armed=` are printed by a static destructor that a killed
process never runs. Without the one-shot `ENTERED` line to stderr this run would
have reported *exactly the same empty log as batch 1's genuinely-zero runs*.
`calls=`, `rounds=` and `entries=` are all incapable of separating those two
cases; only a line written at the moment of the call can.

| run | configuration | `ENTERED` | note |
|---|---|---|---|
| P | predict + detour, in-game Resume | **yes** | `pausesGame` 1 → 0 |
| Q | P + `BANISHED_RT_PREDICT_SELFTEST=1` | **yes** | detour installed, logged |
| R | P + `BANISHED_RT_ALLOC=1` | — | **invalidated by this lane's own cleanup**: killed while clearing batch 2's leftovers, `boot_state exit=4` (STALE). Not evidence either way |
| S | **default config** — port registered, no oracle, no detour | **yes** | `pausesGame` 1 → 0 |

**Run S is the strongest single result in this document.** In the configuration
that actually ships — no predictor, no detour, our body live — the ported
`CloseDialog` ran for real on the Resume click and the dialog was really removed:
`PausesGame` goes 1 → 0, and that is the *engine's own* answer, the one
`GameUpdateLoop::Update` reads to decide whether to tick the world. It is a
behavioural check on the port's effect, not a self-check on our belief about it.

It is also, deliberately, a coarse one. It says the body closed the dialog; it
says nothing about the parent-list unlinks, the array compaction or the sound
tail. That is what §5e's predictive counters are for.

### 5e. Batch 4 — the predictive oracle on a route that exits cleanly

---

## 5d. The offline pin — a pre-flight on the INSTRUMENT, not on behaviour

`tools/closedlg_pin.py` reads the shipped `Runtime-x64.dll` and checks that the
exact instruction encoding this port claims at each cited address is really
there. **23 of 23 match.**

Said plainly, because a green board is easy to over-read: this is a check on the
**transcription**, not on the behaviour. It cannot tell you the port behaves like
retail. Green here plus red at runtime means the reading was right and the
writing was wrong; red here means the reading was wrong and every number
downstream of it is suspect.

**The negative control is built in and it discriminates.** `--control` corrupts
one byte of each expected pattern before comparing; **23 of 23 caught it, 0
survived**. A pin that stayed green under `--control` would be comparing
something structurally incapable of expressing a difference.

The pins that earn their place are the ones a transcription gets wrong: the
`MOVSXD` at `0x1801211c6` that Ghidra prints as a zero extension, the dead
zero-store at `0x18012134b`, the dead compare with a live load at `0x180120f8a`,
the two append sites at `0x18012115f`/`0x18012139f` passing *different*
registers, and the un-folded `0.0 / _secondsPerTick` divide at `0x180121187`.

---

## 6. Changed files

| file | what |
|---|---|
| `runtime/src/ui/userinterface_close.cpp` | **new** — the port, dual-mode against `RtSink` |
| `standalone/ui_close_detour.cpp` | **new** — the opt-in `0x180120f20` detour |
| `runtime/src/ui/rt_ui_port.cpp` | +6 lines: the `BANISHED_RT_UICLOSE` wave switch |
| `tools/closedlg_pin.py` | **new** — the offline byte pin and its negative control |
| `tools/runtime_census.py` | blind spot SEVEN — the detour column could not see `RuntimeDetourInstall` (§8) |
| `docs/runtime_port/WAVE10_LANE_CLOSEDLG.md` | this file |

Switches: `BANISHED_RT_UICLOSE` (default ON, `=0` falls back to the DLL),
`BANISHED_RT_UICLOSE_DETOUR` (opt-in, per `runtime_detour.h`'s rule that nothing
patches the DLL default-on unproven).

## 7. Cutover obligations

* **Two by-address doors, and NEITHER IS NEW DEBT.** `0x180033690`
  (`Array<UI::Event>::SetSize`) is already lane UI1's declared door and
  `0x18007a0d0` (the generic 8-byte `DynArrayPush`) is already `AddDialog`'s.
  Both are genuinely unexported (checked with pefile, not grepped), and both
  reach the DLL through `ResolveAddr(Module::Runtime, …)`, which consults the
  port registry first — so the day anyone ports either helper we get theirs.
---

## 8. The census set diff, and blind spot SEVEN

### 8a. The row

Measured with `tools/runtime_census.py --unported --namespace UI` run twice in
this worktree, from the same directory, with only this lane's port file moved
aside between them — the scanners walk `os.walk("runtime")` **relative to the
current directory**, so the two sides must be the same directory or the delta is
meaningless.

```
UI     1625 total   170 called   148 -> 149 ported   87.1% -> 87.6% covered
"Called but not ported"   22 -> 21
```

**The SET diff, which is the number that matters:**

* **LEFT:** `?CloseDialog@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z`
* **ENTERED:** *nothing*

One row out, none in. The port introduced no new unsatisfied callee, because
both of its by-address doors were already declared debt (§7) and its four
suppressed callees are all exported and already counted.

### 8b. Blind spot SEVEN, found while measuring the row

The `detour` column read **2 for UI both before and after** this lane's detour
landed. That looked like "correct-as-designed for an opt-in installer", and it
is not — it is the seventh counting defect in this tool.

The census matched only `InstallRuntimeDetour*`, the relocating front doors. The
tree has a **second** installer, `RuntimeDetourInstall` — the byte-copy one
`runtime_detour.h` calls "deliberately dumb" — and no regex matched it. Every
detour installed through it was **invisible**, not unresolved: `nonliteral_detours`
came back empty while four subjects were detoured, so the loud-reporting half of
the blind-spot-six cure never fired either.

That is not a marginal installer. **Lane UI1 established it is the CORRECT one
for any prologue ending in `MOV RAX,RSP`**, because the front doors apply an
`ADD RAX,8` trampoline fixup that desynchronises an RAX-relative save from an
RSP-relative restore. The census was blind to exactly the installer the hardest
subjects are obliged to use — and so **lane UI1's own `Update` detour, installed
and measured across five runs, had never appeared on the board.**

Fixed here. The set diff:

| bucket | before | after | entered | left |
|---|---|---|---|---|
| PORT detours | 6 | 8 | `0x1801213c0` `UserInterface::Update[port]` (UI1), `0x180120f20` `UserInterface::CloseDialog[port]` | nothing |
| INSTRUMENT detours | 2 | 4 | `0x1800bff10` `persistobject_probe.cpp`, `0x1800f79f0` `video_interface.cpp` | nothing |

**The control that discriminates:** in the full census diff, `called`, `ported`
and every coverage percentage are **unchanged**. An over-broad regex would have
credited the newly visible targets as called+ported and moved them. And the two
new entries landing in the *instrument* bucket rather than the port bucket shows
the label split still works on the newly visible form — `_add_detour` already
treats an unlabelled detour as an instrument, so no new rule was needed.

The negative case the regex must reject is the **definition** in
`standalone/filename_runtime_hook.cpp`, whose first parameter is a declaration
(`uintptr_t addr`) rather than an argument. Without the lookahead the scanner
reads a function signature as a call site; with it, that file produces no entry.

---

## 9. Handoffs

* **To whoever runs the wave's numbers.** `tools/runtime_census.py`'s detour
  column changed (§8b). Any lane quoting a detour count from before this commit
  is quoting a number that could not see `RuntimeDetourInstall`. The reference
  deltas are in §8b; the `called`/`ported` columns are untouched.
* **To lane UI1.** Your `Update` detour was never on the census board. It is now.
  Nothing about the detour changed — only the tool that counts it.
* **To whoever writes the next harness.** The unattended drivers cannot dismiss a
  dialog on the main path, which is why §5b happened. The routes that CAN are
  `BANISHED_AUTOMENU_BUTTON=7` + `BANISHED_AUTOMENU_CLICKS=…,d0,…,101,…`
  (ModDetailsDialog and ModDialog both close through `fn_UI_CloseDialog`) and
  `BANISHED_PAUSEMENU_BUTTON=1` (Resume → `mainmenudialog.cpp:993`). The second
  does not quit afterwards, so it needs a kill and therefore a probe that does
  not depend on static destructors. Worth writing down in CLAUDE.md's harness
  list: "which env var closes a dialog" is a question three lanes will now ask.
* **To anyone porting `UpdateEvents`, `EndDragDrop` or
  `Array<UI::Event>::SetSize`.** Each one you land converts an `Exclude` in this
  lane's oracle into a comparison. §4's table is the priority order: `UpdateEvents`
  and `EndDragDrop` together own the two largest holes (`_events` at +0x48 and
  the drag-drop block at +0x60).
