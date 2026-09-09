# Wave 10C — lane ROUTES

**Worktree** `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-a10ca14d38b4ef57b`
**Branch** `wave10c/lane-routes`, based on `wave10/base` @ `e868337`.

> The worktree was seeded at `965361d`, an unrelated commit — the exact failure
> the dispatch warned about. `git merge-base --is-ancestor e868337 HEAD` returned
> NOT-ANCESTOR before any work was done, and the branch was cut from `e868337`
> explicitly. Verified ancestor after.

This lane owns the harness and the `BANISHED_*` route surface. Its job is to turn
last wave's *capabilities* into *measurements*. Every claim below names the
counter that supports it.

## Summary

| debt | counter before | counter after | verdict |
|---|---|---|---|
| `??_7DialogController@UI@@6B@`'s nine bodies | `ATTENDANCE: none of the nine bodies was entered` | `total=17` → `51` (OnOpened, OnClosed) | **ROUTE FOUND**; bodies exercised. The **DATA row still must not be promoted** — for a different reason than the one on record. |
| `?Update@Element@UI@@` | *no counter existed*; the published `calls=0` was the absence of a site | `[Element] ATTENDANCE Update=541` | **was live all along**; still ported-not-verified (no replay oracle) |
| `?Persist@Element@UI@@` | same (unmeasured) | `Persist=0` | genuinely not entered — now a real zero |
| `?OnOpened@DialogInstance@UI@@` settings half | `entries=3 noBuffer=3 badMagic=0 records=0` | `entries=5 noBuffer=5 badMagic=0 records=0` | **no route**; blocker is upstream and named exactly |
| `?Serialize@SpriteSheet@UI@@` | `calls=0` | `7151 emissions identical`, `24 compared, 0 diverged` | **VERIFIED** against the shipped export on the real boot-time sheet |

Every run: debug build, DLL present, under
`tools/game_dir_lock.sh run lane-ROUTES 1800 --snapshot …`, logs read from the
snapshot. All runs reached gameplay, self-quit, and left a **0-byte crash log**.

---

## 1. `??_7DialogController@UI@@6B@` — the recorded blocker was the wrong mechanism

### The re-derivation

Lane DLGV held the row at READY/TRAP=0 because its nine-way ATTENDANCE counter
printed *"none of the nine bodies was entered"* in both halves of both A/B pairs.
Its recorded diagnosis:

> Every `UI::DialogController` subclass in `code/Game/Dialog/` carries its own
> exe-side vtable, so nothing constructs an object holding the RUNTIME BASE
> table.

**The first clause is true. The conclusion does not follow from it**, and the
counter-example was already written down in this tree —
`code/Game/Dialog/destroyui.cpp:547`, describing **retail's** exe-side table:

> Retail vtable @ `0x14015bb00` (11 slots): overrides 0,1,2,4,7,9; slots
> 3,5,6,8,10 are **inherited `UI::DialogController` base thunks in the exe**.

A derived vtable does not omit the virtuals it does not override — it **copies
the base entry**. Holding the base table was never the precondition for entering
a base body; **inheriting the slot is**, and every one of these subclasses
inherits most of them.

The real cause is ours and is fixable. All 27 standalone subclass vtables are
built by one idiom:

```cpp
for (int i = 0; i < kSlots; ++i) g_vt[i] = (void*)&StubNop;
g_vt[0] = &Dtor;  g_vt[2] = &GetTypeId;  g_vt[4] = ...;  g_vt[7] = ...;
```

— every slot the subclass does not override is a **file-local no-op**, not the
inherited base thunk. Measured live: **33 of 34** `UI::DialogController` tables
follow the idiom, and 91 slots were stubs where retail inherits.

Nobody noticed because the substitution is invisible: `StubNop` does nothing and
the seven inherited base bodies are literally `C2 00 00` (`RET 0`, verified
byte-wise and one disassembly at a time by lane DLGV). It changes only **whose**
no-op runs — and therefore whether the port is exercised at all.

### The route

`standalone/dlgctrl_route.cpp` (new). **Off by default, silent when unset.**

```
BANISHED_DLGCTRL_ROUTE=probe   report only, patch nothing
BANISHED_DLGCTRL_ROUTE=1       report and install the route
```

It walks the factory registry for registrations whose parent is
`"UI::DialogController"` (via a new read-only `Object::ForEachRegistration` — the
only handle on a file-local vtable is an object's vptr), builds one instance per
type, identifies that file's stub as the **mode** of slots 0..23, and repoints
slots **5 / 6 / 10** — `OnOpened` (+0x28), `OnClosed` (+0x30),
`OnSerializeSettings` (+0x50) — at the ported base bodies.

Deliberate restraints, each stated rather than left as an omission:

* **It refuses any table whose mode is not a clear majority** and reports it,
  so a table that does not follow the idiom is visible rather than mispatched.
  A "mode" without a majority is not evidence of anything.
* **Slot 1 (`TypeName`) is not patched**, although most of these files stub it:
  retail *overrides* slot 1 for DestroyUI. "Our slot is a stub" is not evidence
  retail inherits it, and this lane has retail's override map for exactly one of
  the 27 subclasses. Slots 5/6/10 are safe under that same doubt only because
  the base body there is a no-op either way.
* **Slot 8 (`OnUpdate`) is not patched.** Retail leaves it inherited for
  DestroyUI, but our panels deliberately drive their per-frame refresh from it.
  Overwriting it would break live panels to feed a counter — the exact inversion
  this lane exists to prevent.

### Two counters, scored separately

One number cannot separate the three outcomes, and they invert the conclusion:

| `dispatches` | `ATTENDANCE` | meaning |
|---|---|---|
| 0 | 0 | nothing ever dispatches these slots — controller lists empty. NO ROUTE. |
| >0 | 0 | dispatched, but resolution handed back something other than our port — a **broken** route |
| >0 | >0 | ROUTE FOUND |

A single union counter reads the first two identically
(`reference_union_control_hides_a_blind_screen`).

### Measured

`BANISHED_AUTOSTART=1 BANISHED_LIVE_TOOLS=1 BANISHED_TOOL_HARNESS=housepanel`
+ pause-menu Quit:

```
control  [DlgCtrl] ATTENDANCE: none of the nine bodies was entered in this run.
route    [DlgCtrl] ATTENDANCE ... total=17 OnOpened=9 OnClosed=8
         [DlgCtrlRoute] dispatches=17 OnOpened=9 OnClosed=8
```

**Both screens agree at 17**, which is what rules out the broken-route case. With
the §3 close-and-reopen leg the same counters read **51 / 27 / 24**. The control
half reproduces lane DLGV's readings exactly (`OnOpened entries=3 noBuffer=3`,
`[HousePanel] OPEN house[0]=…`), so the two runs are comparable.

### Can the DATA row be promoted? **No — and the reason has changed.**

The gate DLGV set ("it expires the moment any route makes `[DlgCtrl] ATTENDANCE`
print a non-zero total") is now satisfied. **Satisfying it revealed that the
counter was never a control for the ROW.** It counts entries into the nine
BODIES, and those are registered unconditionally by `RegisterRuntimeAddr` — body
entry does not depend on the row at all.

The row's only consumer in this tree is `runtime/src/ui/dialog.cpp:1227`, inside
the ported base destructor `??1DialogController@UI@@` (0x1800cd2f0):

```cpp
static void* const vtbl = ResolveExport("??_7DialogController@UI@@6B@");
*reinterpret_cast<void**>(self) = vtbl;
```

and **no subclass in `code/Game/Dialog/` ever calls it** — each carries its own
`Dtor` that stamps `g_vt` and frees (e.g. `residenceui.cpp:618`,
`destroyui.cpp:577`). Grepping `0x1800cd2f0` and `DialogController_Dtor` across
`code/Game/Dialog/` returns nothing.

Measured rather than argued — the same A/B with
`BANISHED_RT_MINT_ROWS=??_7DialogController@UI@@6B@` added on top of the route:

```
[DlgCtrl] ATTENDANCE ... total=17 OnOpened=9 OnClosed=8   (identical both ways)
```

and a whole-log frequency diff of the pair finds **exactly two** non-timing
differing lines, both of them the `[RtMint] ROW ON (env)` lines the candidate
half prints; everything else that moves is splash-loader tick and fade frame
counts. Both halves: 0-byte crash log.

**Verdict: the nine bodies are now exercised and safe on a live path; the DATA
row's promotion remains unexercised, because nothing reads it.** Promoting it
would be a promotion no run can distinguish from an absent one — the same
"WHY NOT ALL 104" rule, now resting on a *correct* mechanism instead of a
mistaken one. It expires when something calls the ported base destructor.

**Scope for another lane** (this lane does not port): making the 27 subclass
vtables reproduce retail's inherited slots by default, rather than behind
`BANISHED_DLGCTRL_ROUTE`. It is a pure fidelity fix — the substitution is
provably behaviour-preserving for slots 5/6/10 — and it would make this
attendance non-zero on every run without a switch.

---

## 2. `?Update@Element@UI@@` and `?Persist@Element@UI@@`

### The published `calls=0` was not a measurement

`WAVE10B_LANE_ELEM.md` §6 items 1–2 recorded both as `calls=0`. **There is no
site that could have printed anything else.** Neither body carries
`RT_DIFF_BEGIN` — omitted deliberately by lane ELEM, since neither is replayable
— so the only site with that stem is the registration-time byte pin
`"UI::Element::Update[bytes]"`, and `RtDiffReport` increments `site->calls`
(`runtime/funcs/rt_diff.cpp:84`). Under `BANISHED_RT_DIFF` that row prints
`calls=1` for the one-off boot hash check whether the body ran a million times or
never. A run in which `Update` executed constantly and a run in which it never
executed printed **identical text**.

That is a field being read for something it is structurally incapable of
carrying (`reference_diagnostic_field_cannot_express_the_answer`), and it is the
second instance in this lane — the first being §1's ATTENDANCE counter, which
cannot express anything about the row it gates.

Lane ELEM confirmed this independently and asked for the correction; it is now
written into `WAVE10B_LANE_ELEM.md` §6 as a marked correction.

### The instrument, and the reading

Added a plain unconditional entries counter in the
`encoding.cpp` / `dialog_controller_base.cpp` idiom (print at static
destruction; **not** an RtDiff site, because an RtDiff site's `calls` moves only
from inside a comparison —
`reference_calls_zero_can_mean_always_took_the_uncovered_branch`). File ownership
was handed over explicitly by lane ELEM, which had closed and was under its
no-edits rule.

`BANISHED_TOOL_HARNESS=utility` + autostart + pause-menu Quit:

```
[Element] ATTENDANCE (entries, not comparisons): Update=541 Persist=0
```

**`Update` was live all along.** The reachability is exactly §1's mechanism seen
from the other side: `code/Game/Component/frustumoutlineelement.cpp:258-275`
copies 24 entries out of the live object *after our ported `??0Element` ran*,
overriding only slots 4/5/6 — so slot 7 carries forward verbatim and **is**
`RtUi_Element_Update`. `captureboxelement.cpp` keeps the base table outright.
What stays unreachable is the DLL's own widgets: their derived constructors call
`??0Element` by rel32, which no registration can redirect.

**The counter is route-dependent, and that is worth recording rather than
averaging away.** The `housepanel` route that drives §1 and §3 reports
`[Element] ATTENDANCE: neither Update nor Persist was entered in this run.` on
the very same binary. `Update=541` belongs specifically to
`BANISHED_TOOL_HARNESS=utility`, which is what builds a `FrustumOutlineElement`
through our ported `??0Element`. So the counter is discriminating between routes
rather than reporting a constant — which is the cheapest available evidence that
it is measuring entries and not something ambient.

**Verdict: `Update` — still ported, not differentially verified**, but the reason
is now the honest one (no replay oracle is possible: the body writes into live
arrays, opens and closes tooltip dialogs and can call `CloseDialog`; running the
shipped export a second time would double all of it). It is no longer *also*
unentered. `Persist` — **entries=0 is now a real zero**, and it too remains
ported-not-verified.

---

## 3. `?OnOpened@DialogInstance@UI@@` — the settings half

### What populates `_userData`, read rather than guessed

`_userData` (+0xa8) and `_userFlags` (+0xb0) are pure constructor arguments
(`runtime/src/ui/dialog_create.cpp:781`). Of the sixteen exe-side
`fn_DialogInstance_Ctor` call sites, **fifteen pass `nullptr, 0`**. Exactly one
passes a real buffer — `code/Game/Dialog/edittool_select.cpp:233`:

```cpp
ctrlCount = *(uint32_t*)(uiCompIBP + 0x30);
ctrlData  = *(void**)  (uiCompIBP + 0x28);
fn_DialogInstance_Ctor(dlg, ui, extRes, &nullParent, entityPtr, ctrlData, (int)ctrlCount);
```

So `_userData` is the **selected entity's UIComponent settings buffer**
(`UIComponent+0x28..+0x37` is a growable byte array — data@0x28, count@0x30,
cap@0x34 — which `code/Game/Component/uicomponent.cpp:127` already identifies as
real, serialized state). The EditTool/`housepanel` route DLGV used was the right
one; the buffer at the far end of it is what is missing.

### The route

`BANISHED_DLGSETTINGS_ROUTE=<bytes>` in `standalone/tool_harness.cpp` (off by
default, silent when unset): allocate a real buffer through the game's own
allocator, publish it into the field the select path actually reads, then
**select(A) → select(B) → select(A)** so the first open hits `badMagic`, closing A
runs `SaveSettings` (writing the magic and one framed record per controller,
dispatching each controller's `OnSerializeSettings`), and the reopen reaches the
record loop. The subject is built where the guard reads it, not faked
(`reference_synthetic_subject_beats_a_fake`,
`reference_port_must_reproduce_side_effects`).

### Measured — and a control on the instrument

The sequence works: it took §1's attendance from 17 to 51. But the buffer was
never installed:

```
[DlgSettings:route] house[0] NO UIComponent. searched tid=0x988e1422 components=19:
  MapComponent ZoneComponent DecalComponent ModelComponent HighlightComponent
  PickingComponent InteractComponent ParticleComponent StorageComponent
  WorkComponent StatusIconComponent AmbientEmitterComponent FireComponent
  StateMachineComponent ClearAreaComponent WorkPlaceComponent BuildComponent
  DestroyComponent ResidenceComponent
[DlgSettings] ATTENDANCE ... OnOpened entries=5 noBuffer=5 badMagic=0 records=0
```

**That component dump is a control, not decoration.** "The entity has no
UIComponent" and "my type-id lookup is wrong" produce the identical null, and the
second would indict a sound conclusion. Nineteen components resolving to real
names off a real searched tid says the finding is the entity's.

### Verdict: no route, and the blocker is named exactly

`code/Game/Component/uidescription.cpp:206` — `UIDescription`'s
`GetComponentTypeId` returns the `UIComponent` type id **only when the
description's +0x20 count is > 0**, otherwise 0 (= no component). A founding
house's UIDescription declares none, so retail itself gives it no UIComponent and
therefore no settings buffer.

Reaching `records>0` needs an entity whose `.rsc` UIDescription declares
controllers with saved settings. That is a template/data question and **scope for
another lane**.

`noBuffer=5` is the counter that says the settings half is unexercised.
`entries=5` is the counter that must **not** be read as coverage.

---

## 4. `?Serialize@SpriteSheet@UI@@` — VERIFIED

### The promotion did not supply the caller, and that `calls=0` is real

Lane SS expected `??_7SpriteSheet@UI@@6B@`'s promotion to supply the caller.
Measured on the promoted tip with `BANISHED_RT_SAVE_TRACE=1`:

```
[RtSaveTrace] ==== 22 compared, 0 diverged ====
```

and **not one** of the 22 is `UI::SpriteSheet::Serialize`. The boot-time sheet is
built by the DLL's own `ExternalFactory`, which dispatches slot 5 off the DLL's
table.

**Unlike §2's, this `calls=0` is a real measurement**: the same run's oracle made
22 comparisons on two other bodies, so it was demonstrably live and reporting
while this row contributed nothing. The two zeros look identical in a lane doc
and mean opposite things.

### The route: replay, not a hunt for a caller

`standalone/spritesheet_serialize_probe.cpp` (new, off by default; needs
`BANISHED_SPRITESHEET_SERIALIZE_PROBE=1` **and** `BANISHED_RT_SAVE_TRACE=1`).

On the write path a `Serialize` is a pure read of the object, so ours and
retail's can both run over the same object into different sinks and the emission
sequences compared (`reference_write_path_serialize_is_replayable`).
`RtSaveTrace::Compare` already builds the sinks; the missing piece was a **call,
on a real subject**. The subject is the **real boot-time sheet**
(`Dialog/SpriteSheet.rsc`, parked at KeyMap+0xa0), fully populated — the same run
logs `Sprite("SaveLoadEmpty") -> idx=211` — so every arm of the body is driven by
real data rather than by our idea of a sheet.

The retail side is `ResolveExportRaw`, deliberately: `ResolveExport` would hand
back our own port and the oracle would compare the port with itself. The probe
checks `ours != theirs` and **refuses to report a pass** if they are equal.

### The first reading was an artefact, and a side-swap control proved it

```
[RtSaveTrace] pass UI::SpriteSheet::Serialize: 7151 emissions identical
[RtSaveTrace] DIVERGE ...: same prefix but ours emitted 7151 and theirs 14302
```

14302 is **exactly 2 × 7151**. A clean factor of two over an identical prefix is
not a port defect — it is one side running the body twice. Two causes fit equally
well and they invert the conclusion: (a) the ported body carries its **own**
probe block, which fires whenever the trace is armed, so calling it from inside
an outer `Compare` re-enters the oracle while the outer sinks are live; or (b)
the oracle's sink handling is simply not re-entrant, and whichever body runs
second is double-counted regardless of which it is.

`BANISHED_SPRITESHEET_SERIALIZE_PROBE=swap` runs the identical comparison with
the two sides exchanged:

| arrangement | `ours` side | `theirs` side | verdict |
|---|---|---|---|
| naive (port as `ours`) | port **7151** | retail **14302** | DIVERGE |
| swap (port as `theirs`) | retail **7151** | port **7151** | **MATCH** |

**The doubling did not follow the port** — in the swapped run the port sits on
the `theirs` slot and emits 7151, not 14302. It follows the **arrangement**: it
appears only when the body carrying its own probe block runs in the `ours`
position. So 14302 was never a property of either body, and only the side swap
separates "a port that emits half of retail's records" from "an oracle that
counts one side twice".

The swapped arrangement is therefore the **default** (retail's body contains no
probe block, so nothing re-enters); `=naive` is kept only so the artefact can be
reproduced rather than taken on trust.

### Measured

```
[SheetSer] replays=1 swap=1 verdict=MATCH
[RtSaveTrace] pass UI::SpriteSheet::Serialize: 7151 emissions identical
[RtSaveTrace] pass UI::SpriteSheet::Serialize[swapped]: 7151 emissions identical
[RtSaveTrace] ==== 24 compared, 0 diverged ====
```

Both the body's own oracle and the outer one agree at 7151 identical emissions.

**Verdict: VERIFIED on the write path** — our body emits the identical sequence
of scalars, strings and nested records as the shipped export, over 7151
emissions, on the real boot-time sheet. `stopAfter` is 0, so nothing is truncated.

**What this does not licence:** the READING direction, whose `Array::SetSize` arm
retail guards off the write path, is untouched by this and stays unverified.

---

## 5. Files touched

| file | change |
|---|---|
| `standalone/dlgctrl_route.cpp` | **new** — the DialogController inherited-slot route |
| `standalone/spritesheet_serialize_probe.cpp` | **new** — the write-path replay + side-swap control |
| `standalone/tool_harness.cpp` | `BANISHED_DLGSETTINGS_ROUTE` + the component-list control |
| `standalone/menudialogloopt_stubs.cpp` | two arm calls (both no-ops when unset) |
| `code/Game/Object/factory_registration.{h,cpp}` | `ForEachRegistration` — read-only walk |
| `runtime/src/ui/element_vtable.cpp` | `[Element] ATTENDANCE` counter (ownership handed over by lane ELEM) |
| `docs/runtime_port/WAVE10B_LANE_ELEM.md` | §6 correction, at lane ELEM's request |

`standalone/autostart.cpp` and `standalone/modal_driver.cpp` were **not** touched,
so the known `BANISHED_AUTOQUIT`-before-`BANISHED_AUTOSTART` early return is
unchanged and no A/B of the two env orders was needed.

Every new switch is off by default and prints nothing when unset.
