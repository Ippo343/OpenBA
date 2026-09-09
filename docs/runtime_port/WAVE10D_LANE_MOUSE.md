# Wave 10D — Lane MOUSE — the hover / tooltip / drag route

**Worktree** `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-a37eb276226754786`
**Branch** `wave10d/lane-mouse`, cut from `wave10/base` @ `190e91c`
(`git merge-base --is-ancestor 190e91c HEAD` verified before any work).

All three rows move. **`EndDragDrop 0 → 1`, `EndHover 0 → 2`, `BeginToolTip 0 → 2`**
in one run, on a hover the game performed itself. Debug and release both green,
the modal self-test still `PASS`, every cell `rc=0` with a 0-byte crash log.

The route is the deliverable; the counters are its receipt.

---

## 1. The recorded blocker was HALF TRUE, and the other half was the story

`WAVE10C_LANE_UIF6.md` §8 recorded:

> They need mouse input. The cheapest real route would be a
> mouse-position/click injector in `standalone/` — none exists today.

**The "none exists today" half re-verified exactly.** Nothing in the tree writes
the `Input::Device` singleton's mouse bytes; `tool_harness.cpp` says so about
itself in eight separate comments ("DragTool_OnKey's drag needs a live OS cursor
over the map, which the harness has none of"), and every existing "click" is a
dialog-vtable dispatch with a numeric element id and a **null element pointer**.
`BANISHED_MOUSE*` / `HOVER*` / `CURSOR*` / `DRAG*` were all free names. The
obligation to grep before building was met and this time the grep found nothing
(`reference_grep_the_harness_before_writing_unverifiable.md` cuts both ways).

**The "they need mouse input" half was incomplete in two ways, and both were
load-bearing.** A real mouse on a real machine would ALSO have failed:

| # | what actually blocked the three rows | how it was found |
|---|---|---|
| 1 | the in-game UI was laid out at **`_uiScale = 0`**, so the clip region was `(0,0)-(-1,-1)` and NO cursor position on earth could hover | the probe printed the clip region instead of inferring it from the element rects |
| 2 | our `Element::Update` port — the ONLY caller of all three rows — **never runs**; the DLL's does | `element_vtable.cpp`'s own attendance line, in every cell |

Neither is a mouse problem. Section 3 and section 5 are those two findings.

---

## 2. The injector

`standalone/mouse_inject.{h,cpp}`, plus two call lines each in
`code/Game/Game/gameupdateloop.cpp` (in-game) and
`standalone/menudialogloopt_stubs.cpp` (menu).

### Where it injects, and why nothing else would do

`UI::UserInterface::Update` does not take the mouse as an argument — it reads it
out of the `Input::Device` singleton, at the offsets
`runtime/src/ui/userinterface_frame.cpp:442-461` decode:

```
in + kMouseOffset + 0*0x18 + 4   int16   MouseX raw
in + kMouseOffset + 1*0x18 + 4   int16   MouseY raw
in + kMouseOffset + 4*0x18 + 4   int16   left button hold count
                         + 6     bool    pressed  (down edge)
                         + 7     bool    released (up edge)   <- Element::Update's `b04`
```

So the injector writes **the input device**, not the UI. That is `autostart.cpp`'s
shape — inject at a seam the game already reads, rather than simulate OS input —
one layer lower. Three properties follow, and they are the reason this is the
right layer:

* **It is implementation-agnostic.** The DLL's `UserInterface::Update` and our
  port read the same bytes, so nothing here depends on what is registered. That
  is the property lane UIF6 lost in `modal_driver.cpp` by resolving a callback
  through the port registry (`WAVE10C_LANE_UIF6.md` §3), and it is the property
  that made this lane's before/after comparable.
* **Everything downstream is retail**: the clip test, the strict rect test, the
  0.25 s dwell timer, `_hovering`, the tooltip flag bits, the event queue.
* **It cannot manufacture a pass.** If retail disagrees that the cursor is on the
  element, the hover bit stays clear and screen S3 says so — see `ctl_offtarget`.

The write must land after the frame's input poll and before the UI decodes it,
which is why the seam is the two lines around the `fn_UI_Update` call.
`PostUpdate` reads the element flags back once retail has had its say.

### The scale round trip

The UI coordinate is not the raw value: `mx = (int32)((float)rawX * ui->_uiScale)`.
The injector does not invert that and hope — it searches the candidates around
`x/scale` and keeps the one whose **forward evaluation of the exact shipped
expression** equals the request, and reports a miss on its own screen (S2) rather
than silently aiming elsewhere. Section 4 proves that screen can fail.

### Switches — all off, all silent when unset

```
BANISHED_MOUSE_INJECT=<step>[,<step>…]   arms it; unset = no output, no writes
     probe    dump the live element table (clip region, scale, every dialog)
     hover    hold the cursor on the target
     off      park it at (30000,30000) — this is what makes EndHover reachable
     press / release      button edges
     drag     UI::BeginDragDrop on the target — a REAL drag starts
     dropnow  BeginDragDrop + the release edge in ONE frame (see §6)
     drop / w
BANISHED_MOUSE_TARGET=auto|tip|any|#<name>|@<x>,<y>     default `tip`
BANISHED_MOUSE_HOLD / _START / _TRACE / _CONTROL
BANISHED_RT_UIHOVER_DETOUR=1                            §5, opt-in, independent
```

Target selection asks **the same six questions the subject asks** — the four
guards inside `BeginToolTip` (`+0x40`, `+0x48`, `+0x50`, `+0x34`) plus the two
flag bits `Element::Update` tests before calling it — and prefers the **smallest**
matching element, because a full-screen container would be "hovered" by any
coordinate at all and would pass S3 for the wrong reason.

Driver: `tools/mouse_run.sh <cell>…`, always under
`tools/game_dir_lock.sh run lane-MOUSE … --snapshot ./lane_mouse_logs`. `acquire`
was never piped and never backgrounded. Raw logs in `lane_mouse_logs/`.

---

## 3. FINDING ONE: the in-game UI was scaled to ZERO, so hover was impossible

The very first probe printed:

```
[MouseInject/probe] ui=… clip=(0,0)-(-1,-1) uiScale=0.0000 rawMouse=(0,0)
[MouseInject/probe]   name=time  id=0  flags=00090000  rect=(-445,-49)-(-405,-9)  <TIP-CAPABLE>
```

Every toolbar button laid out at **negative** coordinates, and a clip region of
`(0,0)-(-1,-1)`. `Element::Update`'s `inClip` is
`!(mx < left || mx > right || …)`, so with `right = -1` it demands `mx >= 0 AND
mx <= -1` — **false for every possible mouse position**. No element could hover,
no tooltip could open, and the three rows were unreachable *by a real mouse too*.

The chain, each link measured:

1. `HKCU\…\Banished` holds `GameUIScale = 0` (also `StatusIconOpacity`,
   `StatusIconScale`, `AutosaveInterval` = 0). Read with `reg query`.
2. `game_main.cpp:670` default-fills `settings[0] = 100`, then calls
   `GameSettings_Load(settings)` **immediately after**, which overwrites it from
   Persistence — `[GameSettings] Load: … (UIScale=0 …)`.
3. `GameUpdateLoop::Startup` → `SetupUIRefs` → `ApplyKeyMapToUI_Impl`
   (`optionsdialog.cpp:152`) writes `ui+0xDC = settings[0] / 100.0f` = **0.0f**.
   The tree's own probe line already printed this and had been read past:
   `[Startup] SetupUIRefs: … uiScale=0.000`, beside a comment that says in so
   many words *"uiScale 0 == never applied"*.
4. `UserInterface::Update` computes `w = videoWidth * scale = 0`, so
   `clipRegion.right = w-1 = -1`, and lays every right-anchored element out at
   `w - k`.

It is **persisted poison, not a code defect on a clean machine**: the exe does
not even import `?SetUIScale@UserInterface@UI@@` (checked with `pefile`), so
retail relies on the constructor's `1.0f`, and the ported constructor
(`runtime/src/ui/userinterface.cpp:408`) writes `0x3f800000` correctly. The zero
came from an earlier `GameSettings_Save` writing a struct that held zeros, and
once written it is self-sustaining: Load reads 0, Save writes 0 back.

**What this lane did about it: repaired the registry, and nothing else.** The
four poisoned values were set to the retail defaults (`100 / 100 / 100 / 1`),
which is exactly what the Options dialog would write (its slider range is
75–150, so 0 is out of range and cannot be a legitimate persisted value). No
shared game code was touched — `game_settings_persist.cpp` and `optionsdialog.cpp`
belong to other lanes, and a defensive clamp there would be a deviation from
retail that this lane cannot justify.

**A/B, same binary, one variable:**

| `GameUIScale` | `_uiScale` | clip region | `time` button rect |
|---|---|---|---|
| `0` | `0.0000` | `(0,0)-(-1,-1)` | `(-445,-49)-(-405,-9)` |
| `100` | `1.0000` | `(0,0)-(2559,1376)` | `(2115,1328)-(2155,1368)` |

`lane_mouse_logs/registry_before.txt` archives the pre-change values.

> **Integrator: this is bigger than this lane.** Every in-game run on this
> machine, for as long as that key has held 0, has had a zero-scale UI — no
> hover, no tooltips, no UI hit-testing, and every right/bottom-anchored element
> off-screen. Anything that concluded "the in-game UI does not respond" was
> measuring this. **Scope for whoever owns `game_settings_persist.cpp`:** decide
> whether `GameSettings_Save` should refuse to persist an out-of-range `UIScale`,
> and whether `GameSettings_Load` should keep the default when the stored value
> is one retail could never have written. This lane deliberately did not.

---

## 4. The screens, and the negative controls scored SEPARATELY

This is a route, not a differential, so its capability question is "can each
screen be made to read false by a wrong input?"
(`reference_probe_needs_an_effectiveness_control.md`).

| screen | what it counts |
|---|---|
| **S1 injected** | frames on which the input bytes were written |
| **S2 coordMatch** | frames on which the forward evaluation of those bytes equals the UI coordinate requested |
| **S3 hoverEcho** | frames on which the target came back from retail with **hover bit 17 set** — the effectiveness counter |
| **S4 dlgHover** | frames on which the target's dialog had `_hovering` (+0x98) |
| **S5 tipShown** | frames on which the target had bit 20, "a tooltip is up" |

**The screens NEST** (S5 ⊂ S3∧S4 ⊂ S2 ⊂ S1). That is stated rather than hidden,
because a control scored on the union of a nested set proves nothing about its
inner members (`reference_union_control_hides_a_blind_screen.md`). Every control
therefore prints **all five**, so a blind screen shows up as one that never moves.

### Results — one row per run, all five screens every time

| run | control | S1 | S2 | S3 | S4 | S5 | verdict |
|---|---|---|---|---|---|---|---|
| `all_det` | none | 240 | 240 | 160 | 129 | 129 | subject |
| `ctl_nowrite` | skip the store | **0** | 0 | 0 | 0 | 0 | **S1 caught** |
| `ctl_offtarget` | solve exactly, for a point 8 px OUTSIDE the rect | 200 | 200 | **0** | 184 | 0 | **S3 caught** |
| `ctl_notip` | aim at a hit-testable element that is not tooltip-capable | 200 | 200 | 120 | **0** | **0** | **S5 caught** |
| `ctl_badscale` @ scale 1.00 | treat the UI coord as the raw value | 200 | 200 | 120 | 89 | 89 | **VACUOUS — see below** |
| `ctl_badscale` @ scale 1.25 | same defect | 200 | **0** | 0 | 0 | 0 | **S2 caught** |
| `tooltip_det` @ scale 1.25 | none | 200 | 200 | 120 | 89 | 89 | subject at a non-unity scale |

**Per screen, and what each proof rests on:**

* **S1** — `nowrite` drives S1 to 0 while `ticked=200`, i.e. the state machine
  still ran and found its target. S1 measures the store and nothing else.
* **S2** — **at scale 1.00 the `badscale` control CANNOT FIRE, and it did not.**
  `raw = x` and `raw = solve(x)` are the same number when the scale is 1, so the
  run reported `S2=200` and caught nothing. The file's own comment predicted this
  ("a control that cannot fire must not be scored as passing"), so rather than
  record S2 as passing on a vacuous control, the pair was re-run at
  `GameUIScale=125`: the subject still reads `S2=200` (the round-trip solve is
  right at a non-unity scale) and the control collapses to `S2=0` with `S1=200`
  intact. **S2 is proven only by the 1.25 pair; the 1.00 row is reported as
  vacuous, not as a pass.**
* **S3** — `offtarget` is the load-bearing control. It is the only one that
  separates *"we can place the cursor precisely"* from *"retail agreed the
  cursor is on the element"*: the coordinates are delivered exactly (`S2=200`)
  and yet `S3=0`. Without it, S2 passing would have been read as S3 passing.
* **S4** — shown to vary independently of S3 **in both directions**, which one
  control could not do: `notip` gives `S4=0` while `S3=120`, and `offtarget`
  gives `S4=184` while `S3=0` (8 px outside the button is still inside the
  dialog). `notip` fires S4 and S5 together, and that is stated rather than
  scored as a single-screen catch.
* **S5** — caught by `offtarget` and by `notip`, in both cases with S1 and S2
  alive.

**The effectiveness control is S3**, and it is printed in every configuration.
A run whose bytes were written but which retail never agreed was a hover has
injected nothing the UI can act on; the injector prints `INEFFECTIVE` and says so
rather than letting `S1=200` be read as coverage.

---

## 5. FINDING TWO: registration cannot reach these rows — and why the detour is right NOW

With the scale repaired, the injector produced a genuine hover and a genuine
tooltip — `S4 dlgHover=89`, `S5 tipShown=89` — and the three counters were
**still 0**. The reason is one line, printed by `element_vtable.cpp`'s own
attendance counter in every cell:

```
[Element] ATTENDANCE: neither Update nor Persist was entered in this run.
```

`UI::Element::Update` (0x180116270) is the **only** caller of all three rows. It
is ported and registered under both doors — and it is reached through the
element's **vtable**, from the DLL's own `DialogInstance::Update`. A vtable
dispatch inside the DLL never goes through the export table, so registration
redirects our call sites and nothing else. The DLL called its own
`BeginToolTip`, and `entries=0` meant "the port is not on the path", not "the
game never hovered". That is the exact failure shape
`reference_half_registered_port_reads_as_absent.md` names, one level up: not a
body that is ported but unregistered, but a body that is ported AND registered
and still unreachable.

### The stale blocker

`WAVE10C_LANE_UIF6.md` §8 rejected a detour on these three:

> it would change *who* reaches the body, not whether the game hovers, so it
> adds DLL-prologue-patching risk and buys no coverage.

**That was correct when written, and the injector is what made it stale.** With
no mouse, the DLL's `Element::Update` never took its tooltip or drop arm either,
so detoured bodies would have sat at `entries=0` exactly as registered ones did.
With a mouse they take those arms for real, and the detour stops being a
substitute for coverage and becomes the delivery mechanism for coverage that now
exists.

**That claim is not an argument, it is a measured cell.** `det_only` — the
detours armed, the injector never armed:

```
EndDragDrop entries=0   EndHover entries=0   BeginToolTip entries=0
```

The detour alone buys **nothing**, precisely as UIF6 said. It is the injector
that is the necessary half; the detour is the sufficient half.

### `standalone/ui_hover_detour.{h,cpp}` — `BANISHED_RT_UIHOVER_DETOUR=1`

**Opt-in, never default-on**, like `BANISHED_RT_UIFRAME_DETOUR`: `RtWaveEnabled`
is default-ON and that is the wrong default for a code patch into the shipped
DLL. Unset, the file patches nothing and prints nothing. It is deliberately
independent of the injector's script, so a later lane reaching these bodies
another way can arm it alone.

Each detour points **straight at the ported body** — full replacement, no
wrapper, the trampoline is never entered:

| symbol | addr | prologue displaced | note |
|---|---|---|---|
| `EndDragDrop` | `0x180120af0` | 14 (`5+1+4+4`) | — |
| `EndHover` | `0x180113d10` | 13 (`2+4+7`) | — |
| `BeginToolTip` | `0x180120860` | 18 (`3+1+1+2+2+2+3+4`) | opens `MOV RAX,RSP`, an RSP capture — the Bug Class 3 hazard, and it cannot matter here because nothing enters the trampoline |

Every guard byte was read out of the shipped image with `objdump`, not from a
mnemonic listing — this compiler emits redundant REX prefixes (`PUSH RBX` is
`40 53`) and a listing would have produced a guard that never matches. The three
installs are reported **separately**: a combined "3 installed" line could not say
which one refused, and a refused install leaves the counter at 0, which reads
exactly like a dead port.

Checked, not assumed: none of the three calls `RT_ORIGINAL_ONCE` on **itself**,
so none can recurse into the address just patched
(`reference_detour_machinery_exists_wave7_zero_is_stale.md`). The installer is
given **un-resolved Ghidra addresses**, because all three ARE in the port
registry and `ResolveAddr(Module::Runtime, …)` would have aimed the patch at our
own prologue (`reference_a_rule_on_the_callers_side_gets_broken.md`).

---

## 6. The drag, and one retail fact recovered by a crash

`EndDragDrop` needs `_dragDropDialog` (+0x60) non-null **and** the release edge,
both observed by one `UserInterface::Update`.

The first attempt passed `nullptr` as `BeginDragDrop`'s `InternalPtr<Element>`,
to avoid arming `EndDragDrop`'s "destroy the drag element" arm. It crashed
inside retail at `0x18011f60e`, `SUB R9D,[RAX+0x68]` with RAX null. **The crash
is the finding**, and it was not papered over with a null check (CLAUDE.md): it
sent the lane to read the body, which is eleven instructions and fully legible:

```
+0x60 = *dlgPtr      +0x70 = id      +0x78 = data
+0x80 = x - elem->_layoutLeft (+0x68)     <- a GRAB OFFSET, not a pointer
+0x84 = y - elem->_layoutTop  (+0x6c)
EDX = 2 ; tail-JMP 0x1801223d0 == ?Play@UserInterface@UI@@   (the drag-start sound)
```

So the element argument is **read-only**, `BeginDragDrop` **never writes
`_dragDropElement`**, no ownership is taken, and handing it the live hovered
element destroys nothing. (Two consequences worth recording elsewhere: `+0x80`
`_dragDropUserData` is used here as a pair of int32 grab offsets, and
`_dragDropElement` must be set by some other API.)

**Why `dropnow` exists.** Holding a drag across frames does not work here, and
that was measured, not guessed. With `drag` on one step and `drop` on the next,
the dwell trace shows `dragDlg=0` **already on the frame the drag started**:

```
f=340 … uiMouseCaptured=1 … dragDlg=0000000000000000
```

The toolbar's `DialogInstance::Update` captures the mouse and writes
`UpdateState+0x07` back as 0, so `UserInterface::Update`'s own end-drag block
(`!leftDown && _dragDropDialog`) tears the drag down **inline** — and that block
does the teardown itself and **never calls `EndDragDrop`**
(`userinterface_frame.cpp:506-539`). `dropnow` therefore builds the drop frame's
state in the order retail has it *at that frame*: `_dragDropDialog` set through
the real exported `BeginDragDrop` (not by writing +0x60), the cursor over a
hovering element, and the release edge raised — all before the single
`UserInterface::Update` that must observe them. Only the wall-clock gap between
press and release is compressed, and nothing in retail reads that gap.

### The hover dwell timer, recovered on the way

`DialogInstance::Update` (0x180113980) sets `_hovering` only after the pointer
has sat on the **same pixel** for **0.25 s** (the double at `0x1801616b8`,
divided by `_secondsPerTick`), with `leftDown`, `b08`, `b09` and `mouseCaptured`
all clear and the dialog's own hit test passing.

**`runtime/types/ui/DialogInstance.h` calls `+0x90` `_timerHandle`, a single
field, and it is not.** `0x180113a1b` and `0x180113a26` compare `+0x90` and
`+0x94` against `UpdateState` mouseX and mouseY **separately**, and
`0x180113a60`/`0x180113a69` store the new pair. It is the **last hover
position**. `+0xa0` — already correctly named `_hoverTime` by lane UIF6 — is the
dwell accumulator that pair gates. *Scope for whoever owns that header; this
lane did not edit another lane's type file mid-wave.*

---

## 7. The numbers

**Before** (`baseline`, injector never armed, `wave10/base` behaviour):

```
[RtPort] UI::UserInterface::EndDragDrop  entries=0 destroyedElement=0
[RtPort] UI::DialogInstance::EndHover    entries=0 pushedEvent0x11=0
[RtPort] UI::UserInterface::BeginToolTip entries=0 rejected=0 …
```

**After** (`all_det`: `BANISHED_MOUSE_INJECT=hover,hover,off,hover,dropnow,off`
`BANISHED_MOUSE_TARGET=#time` `BANISHED_RT_UIHOVER_DETOUR=1`):

| row | before | after | which arm |
|---|---|---|---|
| `?EndDragDrop@UserInterface@UI@@` | **0** | **1** | `destroyedElement=0` — the destroy arm is NOT covered (see §6: `BeginDragDrop` never sets `_dragDropElement`) |
| `?EndHover@DialogInstance@UI@@` | **0** | **2** | `pushedEvent0x11=2` — both entries took the real `_hovering` arm and pushed event 0x11 |
| `?BeginToolTip@UserInterface@UI@@` | **0** | **2** | `rejected=0` — all four guards passed, so both entries allocated a `ToolTipDialog`, ran the DLL's `DialogInstance` ctor and stamped the vtable. `nonEmptyString=0`: the empty-string arm; `closedPrev=0`; `allocFailed=0` |

`IsInsideClipRegion` stays at `entries=0` in gameplay and that is correct — our
`Element::Update` inlines the clip test rather than calling it, so nothing on
this path reaches it. Its 1014 entries came from lane UIF6's input sweep.

Entries are 2 rather than 89 because a tooltip opens **once per hover episode**,
not once per hovering frame: the script has two hover episodes, and `S5=129`
counts the frames during which the tooltip was up. The two numbers are
consistent, and neither is the other.

**Coverage this does NOT claim.** These are ENTRY counters, not oracles. All
three rows remain **PORTED, NOT DIFFERENTIALLY VERIFIED** — none is replayable
(each mutates a live UI, allocates, or pushes into a live event array). What
changed is that they are no longer *unexercised*: `BeginToolTip` in particular,
UIF6's declared risk row — "it allocates, runs the DLL's `DialogInstance` ctor,
stamps a vtable, and does two `System::String` releases — all unexercised" —
has now done all of that twice, under the real allocator, with a clean shutdown
and a 0-byte crash log. Three arms are still uncovered and named above.

---

## 8. Build, runs, and the modal self-test

* **Debug** green. **Release** green from a genuine `make clean` first — a
  `make standalone` straight after a debug build says "Nothing to be done" and
  proves nothing. `standalone/log_strip.h` `#define`s `printf`, so this file uses
  the unqualified spelling throughout.
* Every cell: `rc=0`, `[Shutdown] done`, **no crash log** (the one crash in §6
  was the null-element probe and is fixed).
* **The modal self-test still PASSes** — lane UIF6's `modal_driver.cpp` two-door
  fix is NOT regressed:
  ```
  [ModalDriver/TEST] THEIRS(raw DLL)=00007ffcab5afd10  OURS(registry)=00007ff78489c35c  (distinct)
  [ModalDriver/TEST] VERDICT: round0 mismatches=0, controls caught=3/3 -> PASS
  ```
  Note that this verdict is in **`standalone_error.log`, not the debug log** —
  grepping the debug log returns nothing and reads exactly like "the driver never
  armed". It fooled the integrator once.
* `autostart.cpp` was **not** touched. Its `BANISHED_AUTOQUIT`-before-
  `BANISHED_AUTOSTART` early return is real but was not on this lane's path —
  every in-game cell used the `pausemenu` route lane SHOW established, and
  in-game + clean-quit worked on the first try.
* Raw logs: `lane_mouse_logs/<cell>.{out,err,rt_run}.log`. Driver:
  `tools/mouse_run.sh`.

---

## 9. What the injector can and cannot reach

**Can:**
* place the cursor at an exact UI coordinate at any `_uiScale` (S2 proven at 1.25)
* address a target by **name** (`#time`), by **coordinate** (`@x,y`), or by an
  **auto-scan** that applies the subject's own tooltip guards
* hold a hover long enough to clear retail's 0.25 s dwell, so `_hovering`, the
  tooltip and `BeginToolTip` all fire
* end a hover (`off`), so `EndHover` fires
* start a real drag through the exported `BeginDragDrop` and drop it, so
  `EndDragDrop` fires
* raise button down/up edges
* run in the **menu** as well as in-game (both seams are wired; only the in-game
  seam was exercised — the menu is untested and reported as such)
* dump the whole live element tree, with the clip region and scale, which is how
  §3 was found

**Cannot:**
* hold a drag across frames over a dialog that captures the mouse — §6
* reach `EndDragDrop`'s destroy arm, because `BeginDragDrop` never sets
  `_dragDropElement`; whatever API does is unidentified
* drive the map/world (it writes the same bytes the camera and tools read, so a
  press over the world will act like a real click — cells here stay on the UI)
* reach anything inside a nested `ShowDialog` pump — that is
  `modal_driver.cpp`'s `MainLoop+0x90` route, which remains the tool for that
* turn an entry counter into a differential oracle

**For lane TIP:** this is your route, as planned. `BANISHED_MOUSE_INJECT=hover,hover,off`
`BANISHED_MOUSE_TARGET=#time` `BANISHED_RT_UIHOVER_DETOUR=1` on the in-game cell
puts `BeginToolTip` through `??2ToolTipDialog@UI@@SAPEAX_K@Z`, the
`DialogInstance` ctor, `??_7ToolTipDialog@UI@@6B@` and `SetText` **twice per
run**, with `rejected=0`. Your four rows are all on that path. Note
`nonEmptyString=0` today: the toolbar tooltip takes the **empty-string arm**, so
`SetText`'s non-empty path needs a different target — the probe dump lists every
tooltip-capable element with its `tipTbl`/`tipIdx`, which is where to pick one.
