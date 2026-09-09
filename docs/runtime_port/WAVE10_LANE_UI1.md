# Wave 10 — Lane UI1 — the four `UI::UserInterface` frame/dialog rows

Worktree `C:/code/GIT/untitled_rev_eng_project/.claude/worktrees/agent-a39354e233f4c133e`,
branch `wave10/lane-ui1` (off `wave10/base` @ 3318690).

Four rows, each carrying a recorded blocker. The lane brief said to re-test every
blocker from scratch rather than inherit it. **Three of the four recorded
blockers were stale or wrong, one was right for the wrong reason, and one row
closed.**

| row | retail | recorded blocker | verdict |
|---|---|---|---|
| `?Update@UserInterface@UI@@QEAAHAEBVTime@System@@@Z` | 0x1801213c0 | "SPLIT BRAIN, needs a detour" | **CLOSED** — ported, detoured, verified |
| `?Draw@UserInterface@UI@@QEAAXAEAVContext@Video@@@Z` | 0x180121890 | "SPLIT BRAIN, needs a detour" | blocker EXPIRED; **not ported — size only.** Scoped below |
| `?ShowDialog@…@QEAAHV?$InternalPtr@…@Z` | 0x18011fd10 | "no oracle possible: nested `MainLoop::Frame`" | **still blocked**, and the stated reason is right but incomplete. Precise blocker below |
| `?CloseDialog@…@QEAAXV?$InternalPtr@…@Z` | 0x180120f20 | "allocator NO-GO" | blocker EXPIRED — it was a **conflation**. Real blocker below |

Runs are listed at the end with their numbers so the claims here are checkable.

---

## 1. The three blockers that expired, and why each was wrong

### 1a. "SPLIT BRAIN, needs a detour" — STALE, and the split brain already exists

`WAVE7.md`'s closing claim ("the detour was again not needed and the count is
still 0") was already corrected once, by `standalone/ui_detour.cpp`. It is stale
twice over now:

* `standalone/runtime_detour.{h,cpp}` is a mature relocating installer with an
  HDE64 prologue decoder and five live `[port]` detours.
* `standalone/ui_detour.cpp` already carries a **UI** subject on it
  (`TabElement::UpdateContainers`, 0x1800d9710).

And the hazard is not hypothetical any more. **Wave 9 landed `AddDialog`,
`SetCursor`, `EnableCursor` and `BringDialogToFront` as ports.** All four mutate
the object `Update` drains and walks, and none of them can reach the DLL's own
`MainLoop`, which is `Update`'s only caller that matters. So the split brain lane
C described as a future consequence of porting `Update` **is the state of the
tree today**, with our mutators writing an object the DLL's `Update` consumes.
Porting `Update` behind a detour is what closes it, not what causes it.

### 1b. "`EnterCriticalSection` cannot appear in a non-`_win` file, and the allocator is Wave 2's NO-GO" — A CONFLATION

This one blocked `Update`, `Draw` and `CloseDialog` at once, and it is two
separate claims welded together.

Retail's inline sequence in `Update` at 0x180121429..0x18012144b is

```
EnterCriticalSection(MemoryAllocator::_singleton + 0x640)
Allocate(MemoryAllocator::_singleton, 0x18, 8)
LeaveCriticalSection(...)
```

which is **byte-for-byte the body of the exported static**
`?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` (0x1800c7a50) — decompiled and
compared, not assumed. `?MemFree@…SAXPEAX@Z` (0x1800c7ad0) is its `Free` twin.
Calling those exports reproduces the lock discipline exactly, with no
`windows.h` anywhere near a `runtime/src` file. About thirty ported files
already do it.

The Wave 2 NO-GO is a verdict on **replacing the allocator arena**
(`standalone/rt_alloc_swap.cpp`), which is lane ALLOC's gate. It says nothing
about a ported body *calling* the shipped allocator, and reading it as if it did
held three rows for two waves.

### 1c. "No oracle" — the predictive oracle did not exist when that was written

Lane C's instrument was the REPLAY oracle: snapshot the object, run ours live,
run theirs on the copy, memcmp. Its refusal of `Update` is still correct — a
replay would drain the same `_addDialogs._data` twice, `MemFree` it twice, and
pump a second round of `DialogInstance::Update` into live dialogs.

`runtime/funcs/rt_predict.h` inverts it: **ours predicts and writes nothing,
THEIRS runs for real on pristine state**, and every predicted location is
compared against what the DLL left. Nothing is drained twice, freed twice or
pumped twice, because our body does not run. That is what made row 1 closable.

**It is not the `RT_DIFF_SHADOW` marker.** The ~250 of those in the tree take the
port's own verdict and never consult the DLL; a shared misunderstanding of retail
passes every one of them. This lane used `rt_predict` throughout.

---

## 2. Row 1 — `Update` 0x1801213c0: CLOSED

Port: `runtime/src/ui/userinterface_frame.cpp`.
Detour: `standalone/ui_frame_detour.cpp`.
Registration: `runtime/src/ui/rt_ui_port.cpp`, switch `BANISHED_RT_UIFRAME`
(default ON); detour switch `BANISHED_RT_UIFRAME_DETOUR` (opt-in).

Registered under **both** the mangled name and the address 0x1801213c0, per
WAVE7.md §3 — a name-only registration can be dead at a call site that reaches
the DLL through the address fallback while reporting a clean oracle.

### Facts Ghidra's decompiled C dropped

Read off the disassembly, not the C
(`reference_ghidra_c_output_vs_disassembly.md`):

1. **`Update` returns the mouse cursor index.** Ghidra prints the prototype as
   `void`. The last call on the main path is `GetMouseCursorIndex` at
   0x18012185e and the epilogue is reached by a bare `JMP` that never touches
   EAX, so its return value **is** `Update`'s. The fullscreen-fade early-out at
   0x180121865 is `XOR EAX,EAX` — **return 0**. `export_name_verdicts.md` had
   already corrected the declaration from `X` to `H`; this is what the `H` means.
2. **`CloseDialog(this)` is `CloseDialog(this, &tmp)`.** The second argument is
   dropped entirely by the decompiler. 0x1801214b2 stores `_toolTipDialog` into a
   stack slot and 0x1801214b6 passes its address — the by-value `InternalPtr`
   ABI.
3. **A dead compare with a live load.** 0x1801214a9 is
   `MOV RAX,[RDX+0x30]; CMP RAX,RAX; JNZ`. `CMP RAX,RAX` is never unequal so the
   branch is unreachable, but the **load** still happens and still faults on a
   bad `_toolTipDialog`. Reproduced as a `volatile` read; dropping it would make
   our body survive a pointer retail dies on.
4. **`node->prev = tail` is unguarded.** 0x18012147e writes `[RDI+8]` on the path
   where 0x180121456 has just set RDI = 0 because the allocation failed. Kept
   faithful.
5. **The fade's zero-out is one QWORD, not two DWORDs.** 0x1801215d7 clears
   `_fadeVelocity` **and** `_fadeAmount` together; the `>= 1.0f` arm writes two
   separate DWORDs instead. A typed-field port would silently unify them — and
   this exact asymmetry is what exposed an instrument defect (§2b).
6. `DAT_1801616e4` = **4.0f** (read out of `.rdata`): the fade step is
   `amount += (float)(ticks * _secondsPerTick) * velocity * 4.0f`.
7. **`UI::Element::UpdateState` is 0x30 bytes**, recovered from this function's
   own frame (base RBP-0x51 to the last field at RBP-0x29). Layout in the port
   file. `DialogInstance::Update` **writes back** into it — at least +0x07 (the
   mouse-down byte reloaded at 0x180121781) and +0x20 (stored to
   `_mouseCaptured`). That write-back is exactly what bounds the oracle.

### 2a. The detour, and the one instruction that chose the installer

`Update`'s prologue opens with an **RSP capture**:

```
1801213c0  48 8B C4        MOV RAX,RSP
1801213c3  55 56 57        PUSH RBP/RSI/RDI
1801213c6  41 54 41 55 41 56   PUSH R12/R13/R14      = 12 bytes exactly
1801213cc  41 57           PUSH R15                  <- trampoline resumes here
1801213ce  48 8D 68 A1     LEA RBP,[RAX-0x5f]        <- RBP DERIVED FROM RAX
```

Twelve bytes is both the patch minimum and an exact instruction boundary, and
none of the twelve is PC-relative.

`InstallDetour` — and therefore every `InstallRuntimeDetour*` front door —
detects that `MOV RAX,RSP` and rewrites the trampoline copy to
`MOV RAX,RSP; ADD RAX,8` ("Bug Class 3", `runtime_detour.cpp:62-94`).
**That correction is wrong for this function and would corrupt RBX on return.**

Retail's entry contract is RSP = E, return address at [E], caller home space at
[E+8]. When our wrapper CALLs the trampoline the trampoline sees RSP = R, return
address at [R], **our** home space at [R+8] — the same contract with E := R.
Nothing is shifted. Apply the +8 and follow one register:

```
save     0x1801213e1  MOV [RAX+0x10],RBX   ->  R+0x18   (RAX was fixed up)
restore  0x180121867  MOV RBX,[RSP+0xf8]   ->  R+0x10   (RSP was not)
```

The save is RAX-relative and the restore is RSP-relative; correcting only RAX
desynchronises them by exactly 8. So this subject uses `RuntimeDetourInstall`,
the older byte-copy installer `runtime_detour.h` calls "deliberately dumb": it
copies the prologue verbatim, inserts nothing, and refuses on any byte mismatch.

The prediction is not just an argument — **it was measured.** Run E ran a full
in-game session with the detour live and no crash; a desynchronised RBX restore
would have died in the first frames.

**A question this lane did NOT answer, left for lane ALLOC.**
`rt_alloc_swap.cpp` detours `MemoryAllocator::Free` (0x1800c6c00), whose 12-byte
prologue **also** ends in `MOV RAX,RSP`, through the correcting path. Whether the
same reasoning applies there was not tested here and is not asserted. That detour
is opt-in. Somebody should ask.

The installer takes the **un-resolved** Ghidra address and resolves it on its own
side with `ResolveRuntimeRaw`, which is the fix for the defect that shipped
thirteen times (`reference_a_rule_on_the_callers_side_gets_broken.md`). The call
site must never learn to "help" by resolving first.

`RT_ORIGINAL_ONCE` is **not** used inside the detoured body. It resolves through
`ResolveExportRaw`, i.e. the address we patched, so it would recurse to a stack
overflow rather than give a wrong answer. `RtUiUserInterfaceUpdateTrampoline()`
is the only correct "theirs" once the detour is in; a null there means no detour,
never no original.

### 2b. The oracle, its coverage boundary, and the instrument defect it found

Configuration: our body predicts (writes nothing), retail runs for real, then
every prediction is compared and the whole 0x128-byte object is watched for
writes we neither predicted nor excluded.

**Predicted and compared:** `_addDialogs` clear (+0x28/+0x30), `_clipRegion`
(+0xfc..+0x10c), the fade advance (+0xf0/+0xf4), `_mouseDown{X,Y}`
(+0x58/+0x5c), and the drag-drop block (+0x60..+0x89) **on the frames where the
dialog list is empty and its governing condition is therefore determined**.

**Excluded, each naming the suppressed callee that owns it:** `_dialogs`
(+0x00..+0x18 — `CleanRemovedDialogs` x3 unlinks here and the drain's node
pointers are addresses `MemAlloc` chose), `_removedDialogs` (+0x18),
the tooltip pair (+0x38/+0x40 — the suppressed `CloseDialog` arm), `_events`
(+0x48 — `UpdateEvents`, `CleanRemovedDialogs`, and the two suppressed
`Array<Event>::SetSize` pushes), the `_mouseCaptured` **exit** store (+0xd9 —
it is `UpdateState+0x20`), and the drag-drop block whenever the list is
non-empty.

That last one is the design decision worth naming: the drag-drop teardown is
gated on a byte `DialogInstance::Update` writes back into our stack local. With
that call suppressed the condition is **unknowable**, so the body **abstains**
rather than guessing. A predictor that guesses turns a coverage hole into a
false red.

The return value is deliberately **not** compared in predict mode: our body sees
the pre-drain list and retail's sees the post-drain list, so a difference would
be correct rather than a defect. The detour returns retail's value.

#### The instrument defect (fixed here)

The first oracle run reported **3 mismatches**, all on `_fadeAmount +=`, with
`pre=0x38281000 ours=0xbd887221 theirs=0x0`. None was a port defect.

`RtPredictRound::Verify` treated a prediction as superseded only when a later
prediction had **the same address and the same size**. Retail's fade teardown
writes two *different widths* to overlapping addresses in one call — `MOVSS
[RSI+0xf4]` then `MOV qword[RSI+0xf0],0` — so the later 8-byte prediction covers
the earlier 4-byte one without matching it. The intermediate value was compared
against final memory and printed as a divergence three times: the
manufactured-mismatch failure that check exists to prevent, arriving through the
one door it did not cover.

Fixed in `runtime/funcs/rt_predict.cpp` (`RangesOverlap`): supersede on **byte
range overlap**, not exact match. Both the compare loop and the self-test
subject picker use it.

**The direction of the risk is stated in the code**: this rule can only move a
prediction from COMPARED to NOT COMPARED, so a bug in it hides defects rather
than inventing them. Two things keep it honest — every suppressed prediction is
counted and printed as `superseded=`, and the later, wider prediction is itself
compared, so those bytes stay under the oracle. In this case the 8-byte
prediction at +0xf0 passed; nothing stopped being verified.

**The arithmetic check on the fix**, because a count that moves the right
direction by the wrong amount reads as success
(`reference_diff_the_set_not_the_count.md`):

| | before fix | after fix | delta |
|---|---|---|---|
| `wrong` | 3 | 0 | −3 |
| `superseded` | 165 | 168 | +3 |
| `changed` | 93 | 90 | −3 |
| `rounds` / `predictions` / `unpredicted` | 243 / 1704 / 0 | 243 / 1704 / 0 | 0 |

Exactly the three moved, and nothing else did.

#### The negative control

`BANISHED_RT_PREDICT_SELFTEST=1` corrupts one compared, changing prediction per
round and every site must then report a mismatch.

```
UI::UserInterface::Update  rounds=243  wrong=78  self-test caught
SELF-TEST: ... had no compared, changing prediction in this round  x165
```

`243 − 165 = 78`. **Every round that had an armable subject was caught, and none
escaped.** Note that only 3 MISMATCH lines print — `kMaxVerbose` caps the
listing, and reading the cap as the count is exactly
`reference_truncated_tool_output_manufactures_calls_zero.md`. The summary line
is the number.

165 rounds had no armable subject: a quiet frame with no fade, no dialog added
and no resolution change predicts only values already in place. That is honest,
not a gap — and it is why `changed=` matters more than `predictions=` here.

#### Results

| run | configuration | result |
|---|---|---|
| A | predict, no detour, menu (`BANISHED_AUTOQUIT=1`) | rounds=243 predictions=1704 changed=93 **wrong=3** unpredicted=0 — the 3 instrument artefacts |
| B | same, after the `RangesOverlap` fix | rounds=243 predictions=1704 changed=90 **wrong=0** unpredicted=0 |
| C | B + `BANISHED_RT_PREDICT_SELFTEST=1` | **wrong=78 = 243−165, self-test caught** |
| D | detour live, predict OFF, menu | detour installed, clean exit, 0-byte crash log |
| E | detour live, predict OFF, **full in-game** (`BANISHED_AUTOSTART=1`) | world built, rendering, 0-byte crash log |
| F | detour live + predict, in-game | **rounds=2145 predictions=15024 changed=166 wrong=0 unpredicted=0** |
| G | **default config** (registered, no detour, no oracle), menu + in-game | clean, 0-byte crash log, `[RtPort] UI: 139 exports registered` |

`unpredicted=0` across 2145 in-game rounds with the entire object watched is the
coverage control passing: retail wrote nothing outside the predicted-or-excluded
set in a full session.

Run G exists because the default configuration is the one that ships and the
oracle runs do not exercise it — in predict mode our body is not live at all.

**What is NOT verified, stated plainly:** `excluded=246465 bytes` and
`16207 suppressed indirect calls` in run F. The effects of
`CleanRemovedDialogs`, `UpdateEvents`, `ComputeLayout`, `DialogInstance::Update`,
`Play`, `CloseDialog` and the two event pushes are not under this oracle. What
*is* verified is that our body's own writes to `UserInterface` match the DLL's
byte for byte, that it makes the same 16207 calls, and that the DLL wrote nothing
else into the object.

### 2c. One address door, declared

`0x180033690` is `Collection::Array<UI::Event>::SetSize(int, const Event*)`, the
grow-and-fill helper both drag-drop event pushes call. Checked against the
shipped export directory with pefile: **it is the only one of the ten callees
that is not exported** (the other nine are, including the `AEAA`-private
`CleanRemovedDialogs` — `AEAA` means private, not unexported). So there is no
name door and the port reaches it by address.

**That is a cutover obligation**, recorded here rather than left for a residue
run to rediscover (`reference_by_address_door_dies_at_cutover.md`). It goes
through `ResolveAddr(Module::Runtime, …)`, which consults the port registry
first, so the day anyone ports the helper we get theirs.

---

## 3. Row 2 — `Draw` 0x180121890: blocker expired, not ported, scoped

**No blocker survives except size.** The detour question is answered, the
allocator question is answered, and the oracle question is answered by the
instrument that just closed `Update`. This lane did not port it because it is a
~90-local body that builds a `Graphics::Camera` and a `SpriteContext` and
submits every dialog's geometry, and half-porting it would have produced an
unverified registration — which WAVE2's allocator verdict already settled is
worse than a hold-back with a reason.

Everything a follow-on lane needs that this lane established:

* **Prologue and patch length.** Bytes read out of the shipped DLL:
  `48 8B C4 | 48 89 48 08 | 55 | 53 | 56 | 57 | 41 54` —
  `MOV RAX,RSP; MOV [RAX+8],RCX; PUSH RBP/RBX/RSI/RDI; PUSH R12`.
  **12 is NOT an instruction boundary here** (the boundaries are 11 and 13), so
  the patch must be **13** bytes; `RuntimeDetourInstall` NOPs the thirteenth.
  Same RSP-capture reasoning as §2a applies, so the same installer — and note
  `MOV [RAX+8],RCX` writes the caller's home slot, which our wrapper's shadow
  space provides.
* **The persistent write set is tiny**, which is why the predictive oracle suits
  it. Into `this`: only `_clipRegion` (+0xfc..+0x10c). Into the
  `Video::Context` argument: +0x408..+0x414 (four floats), `+0x10 |= 0x1000`,
  `+0x10 |= 0x400`, and +0x1e0..+0x1ec (the viewport). Everything else is stack
  local (the `Camera`, the `SpriteContext`) or is sprite submission that the
  body flushes and frees before returning.
* **The honest limit of that oracle**: `Draw`'s actual product is the submitted
  geometry, and a predictor that suppresses `SpriteContext::AddSpriteDraw` /
  `Flush` verifies none of it. The complement is an A/B screenshot under
  `BANISHED_CAMERA_PIN`, which exists for exactly this. Say both, or the
  ~10 verified scalars will read as "Draw is verified".

---

## 4. Row 3 — `ShowDialog` 0x18011fd10: STILL BLOCKED, with a sharper reason

The recorded blocker ("running it twice pumps two frame loops") is **right for
the replay oracle and understates the problem.** Running it twice would also
allocate a **second nested `UserInterface`** (the `Allocate(_singleton, 0x128, 4)`
that pins our `sizeof`), register **two more** `MainLoop` callbacks, and pump a
second nested frame loop that only exits when the dialog is dismissed a second
time.

Three things this lane can now say that lane C could not.

**(a) A predictive oracle is structurally available — and nearly worthless
here.** Our body would write nothing and the DLL would run for real, so the
"twice" objection evaporates. But look at what `ShowDialog` writes: a freshly
allocated nested `UserInterface` (an address `MemAlloc` chose), two `MainLoop`
callback arrays it grows with the `*3` policy and then compacts in place (data
pointers, also allocated), and exactly **two** predictable things —
`dialog+0x10 = nestedUI` (a pointer, so not predictable either) and
`dialog+0x20 |= 0x20` (**one flag bit**). The return is
`*(int*)(dialog+0x24)`. So the compared set is one bit, and recording that as
"verified" is precisely the partial-oracle-reading-as-full failure.

**(b) `BANISHED_MODAL_ANSWER` is NOT a route — it is a BYPASS.** The brief asked
whether it gives one; it does the opposite. Both harness call sites
(`code/Game/Dialog/moddialog.cpp:697-703`,
`code/Game/Dialog/saveloaddialog.cpp:938-942`) **skip `fn_UI_ShowDialog`
entirely** and substitute the answer — saveloaddialog's own log line says
"ShowDialog skipped". It exists *because* nothing can drive the nested loop
unattended. The rest of `standalone/` (including `tool_harness.cpp`) and the
`BANISHED_*` list were grepped: there is no other route.

**(c) THE SPECIFIC THING THAT WOULD EXPIRE THIS.** The nested loop exits only
when the dialog leaves the nested UI's list, i.e. when something dismisses it.
Our unattended drivers all tick from `MenuDLT::Stub_Update`, which the nested
`MainLoop::Frame` pump never reaches. But `ShowDialog` itself registers
`UpdateStandAloneUI` into `MainLoop+0x90`'s callback array, and **that array is
walked every nested frame**. A harness that registers a third callback into the
same array — one that injects the dismiss after N nested frames — reaches inside
the pump and makes `ShowDialog` drivable unattended. That is a concrete,
buildable piece of harness, and it is the whole blocker.

Until it exists: **held back, with an oracle that would compare one flag bit and
no way to run it.**

---

## 5. Row 4 — `CloseDialog` 0x180120f20: allocator blocker EXPIRED, real blocker named

The recorded blocker was the allocator NO-GO. That is the conflation of §1b:
`CloseDialog`'s allocator use is two inlined `MemFree`s and a `DynArrayPush`, and
both have exported doors (`?MemFree@MemoryAllocator@System@@SAXPEAX@Z` at
0x1800c7ad0). Nothing about Wave 2's arena verdict touches it.

Two further things that look like blockers and are not:

* **The recursion is fine.** `CloseDialog` recurses into the dialog's own child
  list and then into the owning `UserInterface`. `RtPredictRound`'s state is a
  **stack of `kMaxDepth` slots**, not a single one, precisely so a ported body
  that calls another ported body can nest, and overflow degrades to "round
  inactive" rather than to a wrong answer.
* **`Update` already calls it**, so the two ports compose rather than conflict.

**The blocker that actually survives** is oracle *coverage*, and it is worse here
than for `Update` in a specific way. `CloseDialog`'s dominant effects are things
a predictor structurally cannot express:

1. two `MemFree(node)` calls — an address handed back to the allocator;
2. a `DynArrayPush` onto `_removedDialogs` — an address the allocator chose;
3. a vtable `+0x28` release on the dialog;
4. the unlink writes into **another `DialogInstance`'s** list (`lVar+0x38`,
   `+0x40`, `+0x48`), a graph whose extent must be enumerated before `Watch` can
   see it;
5. `EndDragDrop`, `UpdateEvents`, and a tail that queues a sound into the audio
   scene at `this+0xa0`/`+0x90`, touching two more objects.

So a predictive round over `CloseDialog` would compare a handful of scalars on
`this` (+0x38/+0x40 tooltip clear, +0x30 array shrink) against a very large
`Exclude` set. **That is portable work, not a wall** — it is the same shape as
`Update`, one lane's worth — but it must be planned as "port with a declared,
mostly-excluded oracle", not as "port and verify". Recording it as blocked on the
allocator sent nobody at it for two waves.

---

## 6. Changed files

| file | what |
|---|---|
| `runtime/src/ui/userinterface_frame.cpp` | **new** — the `Update` port, dual-mode against `RtSink` |
| `standalone/ui_frame_detour.cpp` | **new** — the 0x1801213c0 detour and its installer argument |
| `runtime/src/ui/rt_ui_port.cpp` | +2 lines: `BANISHED_RT_UIFRAME` wave switch |
| `runtime/funcs/rt_predict.cpp` | `RangesOverlap` — supersede on overlap, not exact match (§2b) |
| `docs/runtime_port/WAVE10_LANE_UI1.md` | this file |

Switches: `BANISHED_RT_UIFRAME` (default ON, `=0` falls back to the DLL),
`BANISHED_RT_UIFRAME_DETOUR` (opt-in, per `runtime_detour.h`'s rule that nothing
patches the DLL default-on unproven).

## 7. Handoffs

* **Lane ALLOC**: `rt_alloc_swap.cpp`'s `MemoryAllocator::Free` detour
  (0x1800c6c00) goes through the `ADD RAX,8` correcting installer on a prologue
  that also ends in `MOV RAX,RSP`. §2a shows why that correction is wrong for
  `UserInterface::Update`. Whether it is wrong there was **not** tested here.
* **Whoever runs the census**: `rt_predict.cpp`'s supersede rule changed. Any
  site with overlapping different-width predictions will move counts from
  `wrong`/`changed` into `superseded`. The `Update` deltas in §2b are the
  reference numbers.
* **Cutover**: one new by-address door, 0x180033690
  (`Array<UI::Event>::SetSize`), unexported, §2c.
