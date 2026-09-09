# Wave 10 — Lane UIDRAW — `?Draw@UserInterface@UI@@QEAAXAEAVContext@Video@@@Z`

Worktree `C:/code/GIT/untitled_rev_eng_project/.claude/worktrees/agent-a7c72459bb75eea3e`,
branch `wave10/lane-uidraw`, cut from `wave10/base` and rebased onto it at
`df37677` (so it carries lane ALLOC, UI1, UIDATA, EXTPTR, PERFFIX, UI4 and the
hardened `game_dir_lock.sh`).

**Row closed.** Retail 0x180121890, 614 instructions. Ported, detoured with the
byte-copy installer at a **13-byte** patch, verified with the predictive oracle
plus an A/B screenshot, and gone from the UI census's unported list.

Every run below was made with `standalone/build/reimpl-x64.exe`
**md5 `59de98f69ccd94ee1e21d72ad4795dcc`**, and the run script re-`md5sum`s the
deployed copy immediately before each launch and refuses to run on a mismatch —
the deployed md5 was identical for every run in the table.

| file | what |
|---|---|
| `runtime/src/ui/userinterface_draw.cpp` | **new** — the port, dual-mode against `RtSink` |
| `standalone/ui_draw_detour.cpp` | **new** — the 0x180121890 detour and its installer argument |
| `runtime/src/ui/rt_ui_port.cpp` | +6 lines: the `BANISHED_RT_UIDRAW` wave switch |
| `tools/runtime_census.py` | **census blind spot SEVEN** — §5. Declared loudly because five other lanes measured UI tonight with the unmodified tool |
| `docs/runtime_port/WAVE10_LANE_UIDRAW.md` | this file |

Switches: `BANISHED_RT_UIDRAW` (default ON, `=0` falls back to the DLL),
`BANISHED_RT_UIDRAW_DETOUR` (**opt-in**, per `runtime_detour.h`'s rule that
nothing patches the shipped DLL default-on unproven).

---

## 1. What the handoff got right, and the one thing it did not say

Lane UI1 left this row with *"nothing survives but SIZE"* and handed over the
prologue bytes, the 13-byte patch length and the persistent write set. **All
three were correct and all three were re-derived here from the shipped bytes
rather than inherited** — the prologue was re-read with `read_memory`, the
instruction boundaries were re-counted, and the installer argument was re-worked
on `Draw`'s own registers rather than cited from `Update`'s.

The one thing the handoff did not say, and it changes what "closing the row"
means:

> **Nothing in the tree calls `fn_UI_Draw`.** `engine/funcs/engine_funcs.cpp:1593`
> resolves `?Draw@UserInterface@UI@@…` and the only other mention of the symbol
> is a `printf` of its address at `engine_funcs.cpp:3753`.

So for this row the registration on its own is inert, and the **detour is the
entire delivery mechanism**: with `BANISHED_RT_UIDRAW_DETOUR` unset, our body has
zero callers and the oracle would have reported `rounds=0` — the
`reference_uncalled_port_is_not_verified.md` failure, arriving through a port
that looks fully registered. Every oracle run below therefore has the detour ON,
and the default shipping configuration (run F) leaves `Draw` on the DLL.

---

## 2. The function, and the six things the decompiled C drops

`Draw` computes the UI-space screen size, resets `_clipRegion`, forces the
context colour to opaque white, builds an **orthographic** `Volume::Frustum` over
the UI rect and a `Graphics::Camera` from it with an **identity**
`Math::Transform3`, pushes the camera into the context, builds a
`UI::SpriteContext` on the stack, walks the dialog list once per layer painting
backdrops and element geometry, then paints the fade quad and the mouse cursor,
flushes, closes the sprite capture, restores the context scissor and frees the
four buffers the `SpriteContext` owns.

Read off the **disassembly**, per
`reference_ghidra_c_output_vs_disassembly.md`:

1. **A dropped dereference that changes the value, twice in a row.** The layer
   pre-pass at 0x180121d33 is
   `MOV ECX,[RDX+0x20]; SHR ECX,4; TEST CL,1; … MOV RCX,[RDX+0x18]; TEST RCX,RCX;
   JZ +5; MOV ECX,[RCX+0x34]; CMP ECX,R8D`. When `dialog+0x18` is **null** the
   load is skipped and ECX still holds **`flags >> 4`** — so the value compared
   against the running maximum is the shifted flags word, not a layer and not
   zero. The very next block re-loads `+0x18` and, on the same null, assigns
   `XOR R8D,R8D` = **0**. Two different answers for the same null, three
   instructions apart. A readable rewrite unifies them and is wrong.
2. **The `>> 1` halvings are `SAR`, arithmetic** (0x180121e44/e48/e68/e70).
3. **`-(float)width` is a sign-bit `XORPS`** against four copies of 0x80000000
   at 0x1801617c0, and the plane-4 constant at 0x18016174c is **`-0.0f`**, not
   `0.0f`. Both are reproduced through a bit XOR (`NegBits`) rather than a C
   negation, because the value lands in a `Frustum` the `Camera` ctor copies
   dword by dword and the sign of a zero is therefore observable.
4. **The `.rdata` constants**, read out of the shipped bytes:
   `0x180161694 = 0.5f`, `0x18016169c = 1.0f`, `0x18016174c = -0.0f`,
   `0x18016175c = -1.0f`, `0x180161718 = 255.0f`, `0x1801617c0` = the sign mask.
5. **`SpriteContext+0x50` is never initialised.** 0x180121cd8 takes its *address*
   and stores that into a home slot; the qword itself is left as stack garbage
   while every other field of the object is written. Reproduced by leaving it
   uninitialised — which is also what `runtime/types/ui/SpriteContext.h` predicted
   when it declined to model that slot.
6. **The `Frustum`'s `isOrthographic` is written as a BYTE and copied as a
   DWORD.** The ctor's `MOV EAX,[RDX+0x60]` copies 25 dwords, so the three
   padding bytes above the bool are stack garbage in retail too.

Four unguarded dereferences are kept faithful (CLAUDE.md, *"Don't 'fix' a
faithful crash with a null check"*): `[[node]+0x18]+0x40` at 0x180121f28,
`[[dialog+0x18]]+0x30` at 0x180121e80, the cursor sheet's sprite table at
0x180122183, and the element root's vtable at 0x180121f52.

### The frustum, decoded

The 24 floats + bool at `RBP+0x14` are six `{normal.xyz, d}` planes:

```
{0, 1, 0, 0} {0,-1, 0,-1}    ->  0 <= y <= 1
{1, 0, 0, 0} {-1,0, 0,-w}    ->  0 <= x <= width
{0, 0,-1,-0.0} {0,0, 1, h}   ->  0 <= z <= height      isOrthographic = true
```

and the 12 floats at `RSP+0x28` are an identity `Math::Transform3`. Grouping
those 12 as four rows of four instead of three of three makes them look
degenerate; the export name
`??0Camera@Graphics@@QEAA@AEBVFrustum@Volume@@AEBVTransform3@Math@@_N@Z` settles
the grouping, and `runtime/types/graphics/Camera.h` already had the 25/12/16
dword split from the ctor.

---

## 3. The installer, and the mechanism — derived on THIS body

**The patch is 13 bytes, because 12 is not an instruction boundary.** Prologue
read with `read_memory` at 0x180121890:

```
48 8B C4        MOV RAX,RSP           0..3
48 89 48 08     MOV [RAX+0x8],RCX     3..7
55 53 56 57     PUSH RBP/RBX/RSI/RDI  7..11
41 54           PUSH R12             11..13     <- boundaries are 11 and 13
```

Patching 12 would leave a lone `0x54` behind. `RuntimeDetourInstall` NOPs the
thirteenth byte after the 12-byte absolute jump. None of the 13 bytes is
PC-relative.

**`InstallDetour`'s automatic `ADD RAX,8` correction is wrong here, and the
register it corrupts is not the one it corrupts in `Update`.** The mechanism,
re-derived on `Draw`'s own instructions:

* Retail's entry contract is RSP = E, return address at `[E]`, caller home space
  at `[E+8]`. When our wrapper CALLs the trampoline, the trampoline sees RSP = R
  with the return address at `[R]` and **our** home space at `[R+8]` — the same
  contract with E := R. Nothing is shifted, which is also why
  `MOV [RAX+8],RCX` lands harmlessly in our own shadow space.
* `Draw` then **saves the ten callee-saved XMM registers RAX-relative and
  restores them RSP-relative**:

  | | |
  |---|---|
  | save | `0x1801218bc MOVAPS [RAX-0x58],XMM6` … through `XMM15` at `[RAX-0xe8]` |
  | frame | `LEA RBP,[RAX-0x258]`; 8 pushes → RSP = E−0x40; `SUB RSP,0x318` → E−0x358 |
  | restore | `0x180122376 LEA R11,[RSP+0x318]` → E−0x40; `MOVAPS XMM6,[R11-0x18]` → **E−0x58** |

  The two agree exactly, and **only because RAX is the entry RSP**. Apply the +8
  and the saves move to E−0x50 while the restores still read E−0x58: all ten XMM
  registers come back holding a stack word that was never theirs, and
  `Engine::MainLoop`'s frame keeps using them.
* What the correction does *not* break is why the bug would be subtle: the
  epilogue's `MOV RSP,R11` / 8 `POP`s / `RET` are all RSP-derived and stay
  correct, so the function returns cleanly to a caller whose registers are
  silently wrong.

So the subject uses **`RuntimeDetourInstall`**, the deliberately dumb byte-copy
installer: it copies the prologue verbatim, inserts nothing, and refuses the
patch on any byte mismatch. The general shape — *a frame that mixes RAX-relative
and RSP-relative addressing cannot tolerate a fix applied to only one of them* —
is offered to lane FREEPRO as a second data point (`Update` had it on RBX,
`Draw` has it on the whole XMM save area); this lane asserts it only for
0x180121890. FREEPRO had not reported when this was written.

The installer takes the **un-resolved** Ghidra address and resolves it on its own
side (`reference_a_rule_on_the_callers_side_gets_broken.md`). That matters more
here than usual: `RtUiDrawRegister` calls
`RegisterRuntimeAddr(0x180121890, …)` on the line immediately before the install,
so a call site that "helpfully" resolved first would hand the installer our own
GCC prologue and the byte guard would refuse in silence.

`RT_ORIGINAL_ONCE` is not used inside the detoured body: it resolves through
`ResolveExportRaw`, i.e. the address we patched, so it would recurse to a stack
overflow. `RtUiUserInterfaceDrawTrampoline()` is the only correct "theirs" once
the detour is in, and a null there means *no detour*, never *no original*.

---

## 4. The oracle — its numbers, its effectiveness, and what it does not cover

### 4.1 What is compared

`Draw`'s persistent scalar write set is **ten locations** and that is all a
predictive oracle can reach:

* **compared** — `this+0x0fc..+0x10c` (`_clipRegion`) and `ctx+0x408..+0x417`
  (`Context::_color`). Both are written *before the first call*, so no suppressed
  callee can move them, and `Flush` writes `_scissor`, never `_color`
  (`runtime/src/ui/spritecontext_flush.cpp:502-508`).
* **predicted but declared unverifiable** — `ctx+0x10` (`_dirty`). We predict our
  own `|= kDirtyMaterialConstants`, but the suppressed `SetCamera` ORs
  `kDirtyGlobalConstants` and the suppressed `Flush` ORs `kDirtyScissor` into the
  same word. `Exclude`d, so the instrument counts it as `unverifiable=` rather
  than silently dropping it.
* **abstained from** — `ctx+0x1e0..+0x1ef` (`_scissor`). The tail's reset is
  *gated* on the current scissor and `Flush` overwrites it on every batch, so with
  `Flush` suppressed the branch condition is unknowable. The body abstains rather
  than guessing, exactly as lane UI1's `Update` abstains on the drag-drop
  teardown; a predictor that guesses turns a coverage hole into a false red.

### 4.2 The runs

| run | configuration | result |
|---|---|---|
| A | menu (`BANISHED_AUTOQUIT=1`), **detour ON**, predict ON | `rounds=361 predictions=1088 changed=4 wrong=0 unpredicted=0` |
| B | A + `BANISHED_RT_PREDICT_SELFTEST=1` | `rounds=350 wrong=1`; **1 armable round, 1 caught** — §4.4 |
| C | menu, detour ON, predict OFF | detour installed, rc=0, **0-byte crash log** |
| D | in-game (`BANISHED_AUTOSTART=1` + pause-menu + quit), detour ON, predict OFF | rc=0, **0-byte crash log** |
| E | D + predict ON | `rounds=2875 predictions=8630 changed=4 wrong=0 unpredicted=0`, `superseded=0`, `unverifiable=1`, `excluded=11500 bytes`, `29928 suppressed indirect calls` |
| F | in-game, **default config** (registered, no detour, no oracle) | rc=0, 0-byte crash log |

**The arithmetic, checked rather than admired.** `predictions = 3·rounds + 5` in
both A (3·361+5 = 1088) and E (3·2875+5 = 8630). Three per round is
`_clipRegion`; the extra five are the four `_color` components plus the one
`_dirty` OR, and they occur in **exactly one round of each session**.
`unverifiable=1` is that `_dirty` prediction; `excluded=11500` is 4 bytes ×
2875 rounds.

`unpredicted=0` across 2875 in-game rounds with the whole 0x128-byte
`UserInterface` and the 16-byte `_color` under `Watch` is the coverage control
passing: retail wrote nothing into either region that we neither predicted nor
excluded. In particular nothing wrote `_captureViewport` (+0x118), which was the
one region this lane was unsure about before running.

**`superseded=0`**, so the `rt_predict.cpp` overlap-supersede change lane UI1
made does not touch this site, and the trap the brief warned about (counts
shifting from `wrong`/`changed` into `superseded`) is inapplicable here. The
reason is structural, not luck: no two predictions in this body have different
widths over overlapping addresses.

### 4.3 EFFECTIVENESS — the honest weakness, stated as a number

**`changed=4` in a 361-round session and `changed=4` in a 2875-round session.**
Both compared regions are idempotent in a steady frame:

* `_color` is forced to white once and then matches on every later frame;
* `_clipRegion` never changes at all, because the *already detoured*
  `UserInterface::Update` (lane UI1) writes the identical `{0,0,w-1,h-1}` earlier
  in the same frame.

So of 8630 compared predictions in run E, **four** ever differed from the value
already in memory. `wrong=0` over the other 8626 is a statement that two
idempotent writes stayed idempotent. That is real but weak, and it is why this
section exists rather than a `wrong=0` headline
(`reference_probe_needs_an_effectiveness_control.md`).

### 4.4 The negative control, scored against armable rounds only

`BANISHED_RT_PREDICT_SELFTEST=1` corrupts one **compared, changing** prediction
per round; a site with nothing changing that round has no armable subject and
says so.

```
[RtPredict] MISMATCH UI::UserInterface::Draw  Context::_color.x = 1.0f
            pre=0x0 ours=0x3f800001 theirs=0x3f800000   [SELF-TEST: ... caught]
[RtPredict] SELF-TEST: UI::UserInterface::Draw had no compared, changing
            prediction in this round -- control not applicable here    x349
[RtPredict]   UI::UserInterface::Draw   rounds=350 ... wrong=1
```

**350 − 349 = 1 armable round, and it was caught: 1/1.** No
`*** SELF-TEST NOT CAUGHT ***` line appears anywhere in the run.

Scoring against the total (1/350) would be the mistake lane UI1 warned about, and
scoring against the union of the two UI sites would be
`reference_union_control_hides_a_blind_screen.md`. The honest reading is both
halves at once: **the instrument can express a failure at this site, and this
site offers it exactly one opportunity per session.** The same denominator that
makes the control pass is the one that makes §4.3 weak.

### 4.5 What the oracle does NOT cover — and the A/B that partially does

`Draw`'s product is **submitted geometry**, and a predictive oracle that
suppresses `AddSpriteDraw`, `Flush` and the element vtable `Draw` verifies none
of it. Run E suppressed **29928 indirect calls**. The layer walk, the letterbox
arithmetic, the batch-key switching, the fade colour packing and the cursor
placement are all outside it.

The complement is the screenshot harness, which exists for exactly this
(`BANISHED_SCREENSHOT_AT` + `BANISHED_SEED` + `BANISHED_CAMERA_PIN`, added in
Wave 6 because *"none of them can answer 'is it actually on the screen'"*):

Six runs, identical except for `BANISHED_RT_UIDRAW_DETOUR`: same exe, same
`BANISHED_SEED=12345`, same `BANISHED_CAMERA_PIN=0,0,50`, screenshot taken at
frame 400. Three with the detour OFF (retail's own `Draw`) and three with it ON
(ours), then every pair compared by differing-pixel count over the 2560x1377
frame (3,525,120 pixels).

**The first version of this test was a single OFF/ON pair, and it "failed":
the two md5s differed.** They differ because two OFF runs differ too — the
status bar at the top of the screen carries live sim values and the frame-400
state is not bit-reproducible. **The within-group control is the whole test**,
and without it this section would have indicted a correct port
(`reference_probe_needs_an_effectiveness_control.md`, inverted: a probe also
needs to know what a NULL result looks like).

```
WITHIN retail (detour OFF x3)      20417   21808   24306        -> 20417..24306
WITHIN port   (detour ON  x3)      20541   42185   44745        -> 20541..44745
BETWEEN       (3 x 3 = 9 pairs)    17487 .. 44274   mean 28140

within-group range : 20417 .. 44745
between-group range: 17487 .. 44274      RANGES OVERLAP
```

The between-group range lies **entirely inside** the within-group range, and the
*smallest* difference among all fifteen pairs (17487) is a **between** pair. One
ON run (`ON3`) is an outlier against everything including its own group, which is
what a noisy run looks like and not what a group effect looks like. **Our `Draw`
is indistinguishable from retail's `Draw` on this instrument.**

Where the differences are is consistent with that reading: a row histogram puts
16406/21808 of the noise and 16790/17487 of the OFF-vs-ON difference in the top
eighth of the frame — the status bar, whose contents change with sim time.

#### The positive control — because "indistinguishable" and "blind" look identical

An A/B that reports no difference proves nothing until the instrument is shown
capable of reporting one. So: the same scene, same seed, same pin, same frame,
**with the pause dialog open** (`BANISHED_PAUSEMENU=300` and the shot at 400) —
a whole extra dialog on screen is the smallest change a UI-geometry defect could
plausibly be smaller than.

**On the whole frame the positive control FAILS.** A dialog appearing moves
23,424–29,675 pixels, which is *inside* the 20,417–44,745 noise band. On that
statistic the instrument cannot see a whole dialog, and its silence about the
port would have been worth nothing. Reporting the port result without this would
have been the failure mode itself.

Restricting the comparison to the frame **below the top eighth** — dropping the
status bar, which is where the row histogram says three quarters of the noise
lives — the instrument becomes sensitive:

```
region: 2560 x 1204 (top eighth excluded), 3,082,240 px

within-OFF (retail x3)        2833   5397   6919
within-ON  (ours   x3)        2863  23678  24594          <- ON3
between    (9 pairs)           695 .. 5294   (excl. ON3)   25792 max (incl.)
POSITIVE CONTROL (dialog)     7661   .. 12725
```

`ON3` is an outlier **against every other run including its own group** (23,678
from ON and 24,594 from ON2, and 22,988–25,792 from the three OFF runs). A port
defect would show up in all three ON runs and would not make ON3 differ from ON
and ON2 by the same amount it differs from OFF; a run whose frame 400 landed on a
different sim/lighting state does exactly this. It is excluded on that argument,
which is stated here so the exclusion can be disagreed with rather than hidden.

With ON3 excluded the reading is clean and in the right direction:

* **positive control 7,661–12,725 clears the retail noise floor 2,833–6,919** —
  the instrument demonstrably sees a dialog-sized UI change;
* **between-group 695–5,294 sits at or below that noise floor** — no port effect
  is visible;
* the single smallest distance among all fifteen frame pairs is **695, a
  BETWEEN pair** (`OFF` vs `ON`): a retail frame and a ported frame that resemble
  each other more closely than any two retail frames do.

**Sensitivity, stated so the result is not over-read:** on the restricted region
the instrument resolves changes above ~7,000 pixels (0.23% of that region) and
nothing below. It can refute a missing, shifted, mis-coloured or mis-layered UI.
It cannot see a one-sprite error, and nothing here should be read as saying it
can.

Runs C, D and F remain what they are — **stability checks, not rendering
checks**. The unattended drivers click buttons by element id, not by pixel, so a
completely blank UI would still have produced `rc=0` and a 0-byte crash log in
all three. Nothing in this document should be read as "Draw is verified"; what is
verified is named in §4.1, §4.4 and the A/B above, and the rest is named here.

---

## 5. `tools/runtime_census.py` — BLIND SPOT SEVEN, and its controls

**This lane modified the census tool. Say so before quoting any number from it.**
Five other lanes reported UI counts tonight against the tool as it stands on
`wave10/base`; the archived copies of both revisions and all four output files
are in the lane's scratchpad, and the four-cell matrix below reconciles them.

### 5.1 The defect

Every detour pattern in the census matches the `InstallRuntimeDetour*` **front
doors** only. The tree has a second installer — `RuntimeDetourInstall`, the dumb
byte-copy one — and the census could not see a single one of its call sites.

That is not a random gap. The front doors route through `InstallDetour`, whose
`MOV RAX,RSP → ADD RAX,8` correction is wrong for a prologue that saves
registers RAX-relative and restores them RSP-relative (§3). So
`RuntimeDetourInstall` is precisely the installer the **hardest** subjects are
required to use, and the column was blind exactly where a detour is most
delicate: `UI::UserInterface::Update` (lane UI1, merged and live) and
`UI::UserInterface::Draw` (this lane) were both invisible. **UI printed
`detour 2` while four `[port]` detours were installed and logging.** The argument
order is inverted relative to the front doors (address first, label last), which
is why copying the existing patterns would not have worked.

The fix is `bytecopy_detour_calls()`, which **parenthesis-matches** rather than
regex-terminating the argument list. Its first version ended the arguments at the
first `)` a non-greedy match could reach — the one inside
`static_cast<int>(kUiDrawPrologue)` — so it found the call, found no label, and
filed a `[port]` detour as an unlabelled instrument: a silent downgrade in the
very column it was added to fix. The `[port]`-suffix rule in `_add_detour` is
untouched, so the two unlabelled instrument users of this installer
(`persistobject_probe.cpp`, `video_interface.cpp`) stay instruments.

### 5.2 The four-cell matrix, and the count the fix must NOT move

|  | base tool | this lane's tool |
|---|---|---|
| **base tree** | `UI 1625 166 140 detour 2 84.3%` | `UI 1625 166 140 detour 3 84.3%` |
| **my tree** | `UI 1625 166 141 detour 2 84.9%` | `UI 1625 166 141 detour 4 84.9%` |

*Method note, because it bit once:* the base tool must be run **from
`tools/`**, not from a scratchpad copy — it resolves the repo root from its own
`__file__` and a copy elsewhere measures nothing while still printing a table
with a `TOTAL` line. That is `reference_scanner_walks_cwd_not_script.md` one
level up, and the first attempt at this matrix produced two invalid cells before
the traceback under the table was read.

**CONTROL — the tool effect alone** (both cells on the *base* tree, so the tool is
the only variable): the diff over the whole 19-line summary table is **one line**,

```
< UI  1625  166  140  2  84.3%
> UI  1625  166  140  3  84.3%
```

Every other namespace and every other column — `total`, `called`, `ported`,
coverage — is byte-identical. The fix moves the `detour` column and nothing else.

**CONTROL — the unported SET under the tool change** (base tree, both tools). Two
rows **enter**, and neither is a defect:

```
>   addr    0x1800bff10    standalone/persistobject_probe.cpp
>   addr    0x1800f79f0    standalone/video_interface.cpp
```

These are the two unlabelled instrument detours whose targets the census
previously could not see at all. `called` does not move (166 both sides), so they
were already counted as reached; what changed is that they are now *listed*. That
is a strict improvement and it is reported here rather than left to surprise the
next reader of the unported list.

**The port effect, as a set-diff and not a count** (my tool, base tree → my
tree). Exactly one row leaves and nothing enters:

```
<   export   UI   ?Draw@UserInterface@UI@@QEAAXAEAVContext@Video@@@Z
<            engine/funcs/engine_funcs.cpp
```

Whole-DLL `TOTAL` with this lane's tool and tree, for the integrator's ledger:
`4632 called=1070 ported=839 detour=8 78.4%`.

Reconciliation for the integrator: the wave headline **UI 166/140** is confirmed
by cell 1. This lane takes it to **166/141 (84.9%)**, and if the census fix is
merged the UI detour column reads **4** where it previously read 2 — of which +1
is this lane's `Draw`, +1 is lane UI1's already-merged `Update` becoming visible.

---

## 6. Doors, and one cutover obligation

Eight callees. **Seven are exported**, checked against the shipped export
directory with `pefile` rather than assumed — `AEAA` means *private*, not
unexported, and both `Flush` and `AddSpriteDraw` are `AEAA` names the DLL exports
all the same (`reference_aeaa_is_private_not_unexported.md`):

| callee | retail | door |
|---|---|---|
| `Graphics::Camera::Camera` | 0x1800fe960 | export; **ported** (`renderer.cpp:682`) |
| `Video::Context::SetCamera` | 0x18001a150 | export; **ported** (`context.cpp:913`) |
| `SpriteContext::Begin` | 0x18011d2b0 | export; not ported |
| `SpriteContext::Flush` | 0x18011ba40 | export; **ported** (`spritecontext_flush.cpp`) |
| `SpriteContext::AddSpriteDraw` | 0x18011c890 | export; **ported** (`spritecontext.cpp:621`) |
| `SpriteCapture::EndAddingSprites` | 0x1800d8a60 | export; not ported |
| `MemoryAllocator::MemFree` | 0x1800c7ad0 | export; **ported** |
| `Array<SpriteDraw>::SetSize(0)` | 0x1800783d0 | **NOT EXPORTED — address door** |

The one address door is a **cutover obligation**, recorded here rather than left
for a residue run to rediscover
(`reference_by_address_door_dies_at_cutover.md`) — but it is the benign kind: it
is already ported and registered at that address by Wave 9 lane UIF
(`runtime/src/collection/array.cpp:1638`), and `ResolveAddr(Module::Runtime, …)`
consults the port registry first, so today it resolves to **our** body
(`reference_resolveaddr_consults_port_registry_first.md`).

`MemFree` is reached through the exported static rather than by inlining
`EnterCriticalSection`/`Free`/`LeaveCriticalSection`, per lane UI1 §1b: the
export's body *is* that sequence, so the lock discipline is reproduced exactly
with no `<windows.h>` in a `runtime/src` file.

---

## 7. Branch that was NOT exercised, and what it would take

`SpriteCapture::EndAddingSprites` (0x1800d8a60) is called only when
`SpriteContext::_spriteCapture` is non-null, and that field is copied from
`UserInterface+0x110`, which only
`?BeginSpriteCapture@UserInterface@UI@@…` sets. It is **null in every run in
§4.2**, so the tail's capture arm never executed here.

Lane UI4 (merged at `0346d36`) established that this is *unrequested*, not
unreachable: the exe already contains a caller — `SpriteCaptureUI::OnEvent`
(exe 0x1400792b0), the handler for the `buttonCapture` element — and every
ingredient is an export. So a follow-on that wants this branch covered has a
**real** subject to drive rather than a fake to build
(`reference_synthetic_subject_beats_a_fake.md`). This lane did not build that
driver; the branch is faithful and untested, and it is one `if (capture)` deep.

---

## 8. Handoffs

* **Lane FREEPRO** — a second, independent data point for the general rule:
  0x180121890 mixes RAX-relative XMM saves with RSP-relative restores, so
  `InstallDetour`'s `ADD RAX,8` desynchronises them by 8 while leaving the
  RSP-derived epilogue correct. §3 has the addresses. `Update` showed the same
  shape on RBX.
* **Whoever runs the census** — `tools/runtime_census.py` changed here. §5 has the
  four-cell matrix, the control that must not move, and the two rows that enter
  the unported listing.
* **Cutover** — one address door, 0x1800783d0, currently satisfied by our own
  port (§6).
* **Anyone re-costing the UI frame pair** — with `Update` and `Draw` both ported
  and detoured, the DLL's `MainLoop` now runs our code for both halves of the UI
  frame whenever both detour switches are on. They are still two independent
  opt-ins, deliberately, so a bisect can isolate either half.
