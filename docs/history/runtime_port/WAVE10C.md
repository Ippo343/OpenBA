# Wave 10C — the nine remaining UI rows, and the debts behind them

> ## ✅ CLOSED 2026-09-09 — **UI 177 / 184 = 96.2%** (from 95.0%), and all five UI vtables at `TRAP= 0`
> Four lanes, all merged. Integration verified on the merged tip that no lane
> ever built: clean boot, `[Shutdown] done`, **0-byte crash log**, the modal
> twin-subject self-test still `round0 mismatches=0, controls caught=3/3 → PASS`,
> and every new `BANISHED_*` switch silent when unset. Debug green; release green
> from a genuine `make clean` first.
>
> **The 27 abort traps lane UIDATA counted are ZERO.** Three vtables promoted;
> two green and deliberately unpromoted, each for a *separately derived* reason.
>
> Seven UI rows remain — one is `??_7Dialog`, one `??_7DialogController`, one
> `?_typeInfo@DialogController@`, and **four are `BeginToolTip`'s callees**, which
> entered this wave as the honest price of resolving by name.


**Opened 2026-09-09 on `wave10/base` @ `e868337`**, straight off Wave 10B's
close. Four lanes.

## The board

`tools/runtime_census.py --unported --namespace UI` returns **nine rows**, and
they fall into three groups that need three different kinds of work:

| rows | what it is | lane |
|---|---|---|
| `??_7Dialog@UI@@6B@` | the last BLOCKED vtable, 8 abort traps | **DLG8** |
| `?IsInsideClipRegion@`, `?BeginToolTip@`, `?EndDragDrop@`, `?EndHover@`, `?DrawStandAloneUI@`, `?UpdateStandAloneUI@` | six callees that ENTERED last wave | **UIF6** |
| `?_typeInfo@DialogController@` | one promotion, blocked on a question about ELEVEN live rows | **TIDENT** |
| `??_7DialogController@UI@@6B@` | `READY TRAP=0` and held back for want of a route | **ROUTES** |

Aggregate at the open: `UI 1625 / called 179 / ported 170 / 95.0%`,
`TOTAL 875 / 1094 = 80.0%`.

## ✅ ALL FIVE UI VTABLES ARE NOW `TRAP= 0` (2026-09-09, after lane DLG8)

```
READY  ??_7Dialog@UI@@6B@            slots= 9 ported= 9 TRAP= 0            -- held back
READY  ??_7DialogController@UI@@6B@  slots=11 ported=11 TRAP= 0            -- held back
READY  ??_7DialogInstance@UI@@6B@    slots=10 ported=10 TRAP= 0 [PROMOTED]
READY  ??_7Element@UI@@6B@           slots=12 ported=11 TRAP= 0 [PROMOTED]  (1 purecall)
READY  ??_7SpriteSheet@UI@@6B@       slots= 9 ported= 9 TRAP= 0 [PROMOTED]
```

The 27 abort traps lane UIDATA counted across four vtables are **zero**, on all
five. Every remaining question about these rows is now about ROUTES, not ports.

**Two rows are green and deliberately unpromoted, for two DIFFERENT reasons, and
the difference is the useful part:**

* `??_7DialogController` — dead for the stamp *and* the dispatch. Nothing
  reachable constructs an object carrying the Runtime base table at all.
* `??_7Dialog` — **live for the stamp, dead for the dispatch.** Lane DLG8's
  effectiveness control FIRES: arming the row moves lane DLG2's independent ctor
  differential `divergences=0 → 3`, because `DialogTempCtor` stamps
  `??_7Dialog@UI@@6B@` on the element path of every dialog the game builds. So
  the row really does reach live objects — and the seven-way attendance counter
  still prints `none of the seven bodies was entered` in every half of every
  pair, because the temp Dialog is destroyed by a DIRECT call to our ported
  `??1Dialog`, never through slot 0.

  Promoting it would put a different pointer in every Dialog's `+0x00`, buy
  nothing measurable, and leave DLG2's oracle carrying three permanent
  divergences. **A row can be live and still not worth promoting**, and only two
  instruments read together can tell that from "safe".

### The recipe broke in four places — which is why the check goes first

Lane DLG8 was told to port the CHECK before the code, because `??_7Dialog` has
the same slot SHAPE as `??_7SpriteSheet` and lane SS's file was the obvious
template. Copying `ConstructInPlace` would have:

* set `+0x30` to `0` instead of `0xFFFFFFFF` — a transparent-black background on
  every dialog in the game, **with no crash to find it by**;
* zero-filled the `+0x44` hole retail leaves alone;
* used the wrong oracle window;
* substituted an unrelated `Serialize`.

Same shape as `reference_a_written_recipe_can_be_wrong`, second occurrence in two
waves, and the second time the "obvious template" was a sibling lane's *correct*
code.

## Why six of the nine rows are NEW, and why that is not a setback

Wave 10B's `called` column moved 170 → 179. Nothing regressed: four faithful
ports pulled in nine callees retail itself calls, and every one was resolved
**by name** rather than through a silent address door
(`reference_porting_can_grow_the_cutover_surface`, now measured four times).
The UI percentage *fell*, 96.5% → 95.0%, while the tree got strictly more
correct. **Read the set, never the ratio.**

Lane UIF6 is the follow-through on six of those nine. Its own ports may pull in
more, and that is an acceptable outcome stated in advance rather than discovered
at the merge.

> **✅ DONE (lane UIF6, merged).** All six LEFT. Full census 196 → 194, UI 9 → 7.
> Four entered, all of them `BeginToolTip`'s callees — the honest price of not
> opening four address doors — and **no new address doors**.
>
> | symbol | addr | entries | verdict |
> |---|---|---|---|
> | `IsInsideClipRegion` | `0x180120830` | **1014** | **VERIFIED**, sweep, 0 divergences |
> | `DrawStandAloneUI` | `0x1801203f0` | 28 | live, not differentially verified |
> | `UpdateStandAloneUI` | `0x180120310` | 12 | live, not differentially verified |
> | `EndDragDrop` | `0x180120af0` | 0 | ported, NOT verified |
> | `EndHover` | `0x180113d10` | 0 | ported, NOT verified |
> | `BeginToolTip` | `0x180120860` | 0 | ported, NOT verified |
>
> The three zeros are **mouse-driven**, and an unattended run has no mouse. The
> lane ruled out the alternatives instead of assuming: `[RtResolveEarly] 0`,
> `[RtResolve] 0 misses`, and `IsInsideClipRegion` — same file, same registration
> path — entered 1014 times. It also **rejected a detour**, on the grounds that a
> detour changes *who* reaches the body, not whether the game hovers.
>
> `IsInsideClipRegion` is pure, so it was swept against the shipped export rather
> than waiting for a caller (`project_pure_functions_verify_by_sweep`): 1014
> cases, 0 divergences, both controls scored SEPARATELY — effectiveness (205
> cases return true) and discrimination (an inclusive→exclusive right-edge defect
> caught on 37 cases).

### The regression lane UIF6 caused, found and fixed — read this before touching `modal_driver.cpp`

Its first modal cell **hung to its timeout with no error line**.
`standalone/modal_driver.cpp` identified the DLL-installed callback through
`ResolveAddr(Module::Runtime, 0x180120310)` — **which consults the port registry
first** (`reference_resolveaddr_consults_port_registry_first`). UIF6's own
address-door registration therefore made it resolve to OUR function while the
DLL's `ShowDialog` kept installing the DLL's; nothing matched, and the modal was
never dismissed. The file's comment claimed detection worked either way — true
only while nothing was registered at that address. Fixed by matching **both**
doors (`ResolveRuntimeRaw` + registry).

**Keeping these ports while reverting `modal_driver.cpp` re-hangs the modal
test.** Verified on the integrator's merged tip (DLG8 + TIDENT + UIF6):
`round0 mismatches=0, controls caught=3/3 → PASS`, dismissed on both subjects.

**Note for readers of that test:** its output goes to `standalone_error.log`, not
`standalone_debug.log`. Grepping the debug log for `[ModalDriver]` returns
nothing and looks exactly like "the driver never armed".

### Two header facts corrected

`DialogInstance+0x98` is `_hovering`, **not** `_closing` — the header said "NAME
IS A GUESS", and `?IsHovering@` (`0x1800455a0`) is the entire function
`MOVZX EAX,[RCX+0x98] / RET`. `+0xa0` is a `System::Time`, not a `void*`.

## The two lanes that are not porting lanes

Half this wave is spent on things already shipped, because two of Wave 10B's
findings say the instruments under them are weaker than the row counts suggest.

### TIDENT — ✅ ANSWERED: CONTENT-ONLY. The eleven live rows are safe as shipped.

> **Verdict (lane TIDENT, merged):** ZERO pointer-identity consumers of
> `BaseTypeInfo::name` in either shipped binary. In `Runtime-x64.dll` the only
> static reads of any node's `+0x08` are 103 `?TypeName@` accessors — and not one
> of those 103 is *called* anywhere in the DLL; `Factory::GetTypeName` has no
> caller at all. In `Application-x64.exe` all **525** references to the 42
> imported `?_typeInfo@` slots read the dword at `+0x00` (typeId) and **not one
> reads `+0x08`**; the exe does not even import `GetTypeName` — it imports
> `GetTypeIdFromName` and `IsDerivedFrom`. **The question is retired for all
> twelve rows and none should be backed out.**
>
> The instrument was blind exactly as charged, and the two screens were scored
> SEPARATELY: under an injected 5-row alias the pre-existing `strs=` content
> screen did not move at all (624 comparisons, 0 mismatch, both halves) while the
> new `ptr=` screen moved by exactly 5. The control had to run *toward* identity —
> `name` is our own literal, so "the pointer differs" is true by construction, and
> a screen that always says "differs" proves nothing.
>
> **The finding worth more than the row:** the tree already had three
> pointer-identity screens and nobody read them as such.
> `UI::SpriteSheet::TypeName` has been reporting `calls=1 divergences=1` — `ours`
> in our image, `theirs` in the DLL — live since that row was promoted last wave.
> The A/B moves it `1 → 0` at `calls=1` in **both** halves (the value moved, not
> the coverage), and a same-shape unpromoted sibling sits at `calls=4 div=0` in
> both, ruling out a broken oracle. Harmless per the audit, now documented beside
> the body, and the comparison deliberately left as a pointer.
>
> `?_typeInfo@DialogController@` is still **not promoted** — but for a different
> reason than lane DLGV gave. Its only consumer is `TypeIdOfStatic` in
> `dialog_create.cpp`, and the new `[TidStatic]` attendance line printed zero
> across five routes. A port with no caller. The observable to watch is named.

The original framing, kept because the reasoning is the reusable part:


Lane DLGV declined `?_typeInfo@DialogController@` because promoting it makes
`?TypeName@DialogController@` return **our node's `name` pointer**, and the mint
shadow compares **content, not identity**. That reasoning applies unchanged to
the eleven `?_typeInfo@` rows **already promoted and default-on**. So the
question is not "may we promote a twelfth" — it is *"is the screen that
green-lit eleven live rows structurally capable of expressing the failure mode
those rows are exposed to?"* (`reference_diagnostic_field_cannot_express_the_answer`).

TIDENT answers it from the binaries: every consumer of a `BaseTypeInfo::name`,
in retail, in the exe and in our tree, classified as a POINTER consumer or a
CONTENT consumer. One pointer consumer changes the verdict for all twelve rows;
zero retires the question permanently. Either way the screen gets a
pointer-identity check with a **negative control proving it can fail**.

### ROUTES — ✅ three debts closed, one named, and a GATE THAT WAS NEVER A CONTROL

| debt | before | after | verdict |
|---|---|---|---|
| `??_7DialogController` attendance | 0 | **17 → 51** | route FOUND; row still not promoted |
| `?Update@Element@` | *no counter existed* | **541** | was LIVE all along; still no replay oracle |
| `?Persist@Element@` | unmeasured | **0** | a real zero now |
| `?OnOpened@` settings half | `noBuffer 3/3` | `noBuffer 5/5` | no route; blocker named exactly |
| `?Serialize@SpriteSheet@` | `calls=0` | **7151 emissions, 24 compared, 0 diverged** | **VERIFIED** |

**The recorded reason for holding `??_7DialogController` was wrong twice over.**
Lane DLGV's diagnosis — "nothing constructs an object holding the RUNTIME BASE
table" — is true, but does not imply the conclusion: a derived vtable **copies**
the base entry for every virtual it does not override, and `destroyui.cpp:547`
says exactly that about retail's own table, three files away. The real cause was
**ours**: all 27 standalone subclass vtables replace those inherited entries with
a file-local `StubNop`. Wiring slots 5/6/10 back took attendance to 17, then 51.

**And satisfying the gate revealed the gate was never a control for the row.**
That attendance counter measures the nine *bodies*, which are registered
unconditionally — body entry never depended on the row at all. The row's only
consumer is `dialog.cpp:1227` inside the ported base destructor, which no
subclass in `code/Game/Dialog/` ever calls. A/B with the row armed: attendance
identical at 17, whole-log frequency diff exactly two non-timing lines, both
`[RtMint] ROW ON`. So the row stays off — for a **third** reason, correctly
derived this time. Same family as
`reference_control_must_pick_a_compared_subject`.

**Two published `calls=0` readings meant OPPOSITE things.** Element's was the
*absence of a site* — the only matching site is a byte pin whose `calls` counts
the boot hash check. SpriteSheet's was real: the same run's oracle made 22
comparisons on other bodies. `WAVE10B_LANE_ELEM.md` §6 corrected in place, at
lane ELEM's own request. A fifth distinct cause of `calls=0`, on top of the four
already recorded.

**The SpriteSheet oracle's first reading was an artefact**: `theirs = 14302` is
exactly `2 × 7151`. A side-swap control showed the doubling follows the
*arrangement*, not the port — the ported body carries its own probe block and
re-enters the oracle when placed on the `ours` side. Swapped is now the default;
`=naive` reproduces the artefact on demand.

**Scope handed on, not taken** (the lane was told not to port): make the 27
subclass vtables reproduce retail's inherited slots by default (provably
behaviour-preserving for 5/6/10), and find an entity whose `.rsc` UIDescription
declares controllers — `uidescription.cpp:206` gates the UIComponent on
`+0x20 > 0`, and founding houses declare none, proven by a component dump naming
all 19.

The original framing:

### ROUTES — a body nobody enters reads exactly like a body that is absent

Wave 10B shipped four bodies that are registered, default-on, and either never
entered or entered only on their trivial path. Each reads GREEN on every
instrument we have:

* the nine `DialogController` bodies — `[DlgCtrl] ATTENDANCE` zero in **both
  halves of both A/B pairs**;
* `Element::Update` — dispatched every frame in retail, entered zero times here,
  because the DLL's derived ctors call `??0Element` by **rel32** and nothing it
  builds sees our table;
* `DialogInstance::OnOpened` — five entries, all of them the `noBuffer` early
  exit, so the settings-reading half is untouched;
* `SpriteSheet::Serialize` — `calls=0`, the boot sheet dispatching slot 5 off
  the DLL's own table.

ROUTES owns `tool_harness.cpp`, `autostart.cpp`, `modal_driver.cpp` and the
`BANISHED_*` surface for the wave, so four lanes do not collide on the harness.
It is explicitly allowed to come back with "no route exists, here is the
evidence" — that beats a promotion nothing exercises.

## Standing rules

Unchanged from `WAVE10B.md`, plus two the last wave earned:

* **A green readiness screen is not a route.** `READY / TRAP=0` says the table
  is safe to stamp, not that anything dispatches it. Attendance counters are the
  separate instrument and must be read alongside.
* **The release check must be a real one.** `make standalone` straight after a
  debug build reports "Nothing to be done" and proves nothing — shared object
  dir. Clean first.
* Lane set-diffs are re-measured at the merge. In Wave 10B one lane's reported
  diff disagreed with the tip, so lanes now state where and when they measured.
