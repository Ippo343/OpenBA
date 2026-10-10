# Wave 10D — lane VTFID

**Worktree** `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-a2b139fd60a978368`
**Branch** `wave10d/lane-vtfid`, cut from `wave10/base` @ `190e91c`.
`git merge-base --is-ancestor 190e91c HEAD` → ancestor, checked before any work.
The worktree was seeded at `190e91c` already, so the 24-July seeding bug did not bite.

The lane's job: make our standalone `UI::DialogController` subclass vtables
reproduce **retail's inherited slots**, as a fidelity fix rather than a promotion
hunt — then re-derive the three UI rows said to be downstream of it.

---

## 0. Summary

| | before | after |
|---|---|---|
| standalone subclass tables reproducing retail's inherited slots | 0 of 38 | **38 of 38** |
| inherited slots holding a file-local no-op | **166** | **0** |
| deliberate standalone overrides at inherited slots | 10 | 10 (**kept**, by construction) |
| `[DlgCtrl] ATTENDANCE` on an ordinary run, no switch | `none of the nine bodies was entered` | **`total=1699`** |
| minted DATA rows promoted | 14 | **15** |

Rows: `?_typeInfo@DialogController@UI@@` **PROMOTED**, with a measured observable
and a negative control. `??_7DialogController@UI@@6B@` and `??_7Dialog@UI@@6B@`
**stay unpromoted** — and the first one's blocker is now a permanent binary fact
rather than a missing route, so no future lane should hunt for one.

The count of tables is **38, not the 27 the dispatch quoted**. 27 was the number
of files carrying the `StubNop` idiom; the number of `UI::DialogController`
subclass tables is 38 (39 subclasses, of which `SelectResourceUI` has no
standalone table at all).

---

## 1. Establishing what retail actually holds — the check, before the code

The brief said to read retail's tables rather than infer the rule once and apply
it 38 times. The tool is **`tools/dlgctrl_vtable_audit.py`** (checked in), and
its first two designs were both wrong in ways that do not announce themselves.
Recording those, because the failures are the reason the third is trustworthy.

**Attempt 1 — chase the Factory function, take the longest .rdata run.**
Wrong in both directions at once: it *merged* adjacent tables (`BuildUI` came
back with 20 slots, the second half being a neighbouring `DialogInstance` table)
and it *collided* distinct classes onto one address (`CitizenUI` and `DestroyUI`
both reported `0x14015bb00`). Three of the six vtable addresses this tree had
already written down by hand came back wrong. Nothing in the output said so.

**Attempt 2 — a structural alignment scan of `.rdata`.** Score a candidate
start by how many of its 11 qwords are `FF 25` thunks whose import name is the
one belonging to that slot; reject any window with a base thunk at the *wrong*
index. This is sound as far as it goes and it found **38** tables — against a
CSV-derived closure of **38** subclasses. The counts matching is worth nothing:
the **set diff** was one-in one-out. The scan had `DebugUI`, which the closure
lost because that CSV row spells the parent `"DialogController"` without the
`UI::` namespace; the closure had `TradeUI`, which the scan lost because TradeUI
overrides all ten of its virtuals and inherits only slot 3, scoring below the
threshold. Dropping the threshold to 1 does not fix it — slot 3's thunk is
`?Persist@Internal@Object@@`, shared by *every* `Object` subclass, so at 1 the
scan admits `MainMenuDialog`, `MessageDialog`, `FrustumOutlineElement` and
friends.

**Attempt 3 — the one used.** Two screens, scored separately:

* **UNIVERSE** from `standalone/factory_registrations.csv`, transitive over the
  parent column, matching **both** spellings of the base. → 39 classes.
* **LOOKUP** by each class's own slot-1 `TypeName` stub. Retail's stub is
  `MOV rax,[rip+d]; RET` — it returns `BaseTypeInfo::name`, the `char*` at
  `storage+0x08`, and the CSV lists every type's `storage`. So **each table
  names itself**, with no scoring and no threshold. All 39 located, none
  ambiguous.

The structural scan is kept as an independent **cross-check** and its set diff
printed (`in universe, not in scan: ['TradeUI']`), never folded into one number.

**Classification** is a reading, not a guess about a no-op: an inherited slot
holds a 6-byte `FF 25 rel32` thunk, and following it lands on a PE import slot
that the import table names outright. "Slot 5 is inherited" is literally the
string `?OnOpened@DialogController@UI@@UEAAXXZ` read out of the binary.

**Corroboration.** All six retail vtable addresses previously recorded by hand in
this tree — `destroyui.cpp:547`, `controlui.cpp:239`, `orchardui.cpp:250`,
`pastureui.cpp:371`, `statusbarui.cpp:712`, `upgradeui.cpp:193` — appear in the
attempt-3 list at the same addresses. And `destroyui.cpp:547`'s slot list
("3,5,6,8,10 are inherited") equals the generated `DestroyUI` mask `0x568`.

---

## 2. The per-class, per-slot table

Slot meanings, established independently from
`runtime/src/ui/dialog_controller_base.cpp` (which ports all eleven off the DLL
table at `0x1801565f0`):
`0 dtor · 1 TypeName · 2 Type · 3 Persist · 4 OnCreated · 5 OnOpened ·
6 OnClosed · 7 OnUpdateLayout · 8 OnUpdate · 9 OnElementEvent ·
10 OnSerializeSettings`.

Legend — `.` both override · `!` **retail inherits, we stubbed** ·
`o` retail inherits, we deliberately override · `X` retail overrides, we stubbed.

```
class                  retail vt   slots 0..10   file
AttendanceUI           0x14015b070  .X.!.!!.!!!   ui_dialog_stub_factories.cpp
BuildRequirementUI     0x14015b1d0  .X.X.!!!!!!   ui_dialog_stub_factories.cpp
BuildUI                0x14015f210  .X.!.!!.!.!   ui_dialog_stub_factories.cpp
CemeteryUI             0x14015b240  .X.!.!!.!!!   ui_dialog_stub_factories.cpp
CitizenUI              0x14015ba00  .X.!.!..o.!   citizenui.cpp
ConsumeProduceUI       0x14015b2e0  .X.!.!!.!.!   ui_dialog_stub_factories.cpp
ControlUI              0x14015b380  .X.!.!!.!.!   controlui.cpp
CropUI                 0x14015f270  .X.!.!!...!   cropui_promote.cpp
DebugUI                0x14015b870  .X.!.!X...!   debugui.cpp
DestroyUI              0x14015bb00  .X.!.!!.o.!   destroyui.cpp
EntityUI               0x14015bb90  .X.X.!....!   entityui.cpp
EventLogUI             0x14015bc40  .X.!!.X.!.!   eventlogui.cpp
HelpUI                 0x14015bf30  .X.!.X!.!.!   ui_dialog_stub_factories.cpp
HotKeyToolTipUI        0x14015bfc0  .X.X.!!!!!!   ui_dialog_stub_factories.cpp
ImmigrationUI          0x14015c080  .X.!.!!.!.!   ui_dialog_stub_factories.cpp
MapUI                  0x14015c350  .X.X.!!!!.!   ui_dialog_stub_factories.cpp
ModBrowseUI            0x14015c500  .X.X!!!!!!!   ui_dialog_stub_factories.cpp
NoiseUI                0x14015ca80  .X.XX!!!!X!   ui_dialog_stub_factories.cpp
OrchardUI              0x14015cf30  .X.!.!!...!   orchardui.cpp
PageToggleUI           0x14015cfb0  .X.X.!!.!.!   ui_dialog_stub_factories.cpp
PastureUI              0x14015f610  .X.!.!!...!   pastureui.cpp
ProductionUI           0x14015d070  .X.X.!!.!!!   ui_dialog_stub_factories.cpp
ProfessionUI           0x14015d0f0  .X.!!.!!..!   ui_dialog_stub_factories.cpp
RadiusUI               0x14015d160  .X.!!..!!!!   radiusui_promote.cpp
ResidenceUI            0x14015f6d0  .X.X.!!.o.!   residenceui.cpp
ResourceLimitUI        0x14015d1f0  .X.!.!!.!.!   ui_dialog_stub_factories.cpp
SpriteCaptureUI        0x14015d4f0  .X.XX!!!XX!   ui_dialog_stub_factories.cpp
StatusBarUI            0x14015d810  .X.!o.X.o.!   statusbarui.cpp
StatusIconUI           0x14015d8a0  .X.!.!!.o!!   statusiconui.cpp
StorageCapacityUI      0x14015d950  .X.X.!!.o!!   storagecapacityui.cpp
StorageUI              0x14015f970  .X.X.!!.o!!   storageui.cpp
TitleDescToolTipUI     0x14015d9f0  .X.X.!!!!!!   ui_dialog_stub_factories.cpp
ToolUI                 0x14015da70  .X.X.!!!..!   ui_dialog_stub_factories.cpp
TownHallUI             0x14015dce0  .X.!.!!....   townhallui.cpp
TradeUI                0x14015de80  .X.!.......   ui_dialog_stub_factories.cpp
TutorialUI             0x140160f30  ...X.!!.!.!   Tutorial/tutorial_dialog_vtables.cpp
UpgradeUI              0x14015df00  .X.!.!!Xo.!   upgradeui.cpp
WorkPlaceUI            0x14015e030  .X.!.!!.o.!   workplaceui.cpp

tally: . 180   ! 166   o 10   X 62      (418 slots compared, 38 tables)
retail-only, no standalone table: SelectResourceUI
```

**The rule the brief quoted is right, but it is not uniform.** `!` is spread
over eight different slots — `OnSerializeSettings` 36, `OnOpened` 32,
`OnClosed` 31, `Persist` 23, `OnUpdate` 19, `OnElementEvent` 11,
`OnUpdateLayout` 10, `OnCreated` 4 — and no two classes have the same mask.
Applying `destroyui.cpp`'s 3/5/6/8/10 to all 38 would have been wrong for 30 of
them.

### Slots 0, 1 and 2 are inherited by NOBODY

Across all **39** retail subclass tables, no mask has bit 0, 1 or 2 set. Every
subclass overrides its own deleting destructor, its own `TypeName` and its own
`Type`. This is load-bearing twice over in §4, and it is why the fix touches
only slots 3–10.

### Where our stub was load-bearing, and is kept

The 10 `o` slots are standalone-only overrides at slots retail inherits:

| class | slot | ours |
|---|---|---|
| CitizenUI, DestroyUI, ResidenceUI, StatusBarUI, StatusIconUI, StorageCapacityUI, StorageUI, UpgradeUI, WorkPlaceUI | 8 `OnUpdate` | `W_OnUpdate` |
| StatusBarUI | 4 `OnCreated` | `W_Setup` |

These drive live panel refresh (the documented standalone pattern — retail
refreshes from a path we have not ported). Lane ROUTES flagged slot 8 explicitly:
overwriting it "would break live panels to feed a counter". **They are protected
by construction, not by a comment**: `Apply()` overwrites an inherited slot only
if it still holds *that file's own stub pointer*, which each site passes in. The
run reports `keptOverride=10`, matching this table exactly.

### The 62 `X` slots are a DIFFERENT debt, and are not touched

`X` = retail overrides, we stub. 37 of the 62 are slot 1 `TypeName`; the rest are
unported exe bodies. Filling these with a base entry would be a *new* infidelity
(retail does not inherit them), so they are left alone and recorded here. The
slot-1 case is worth naming: retail returns the class name from
`BaseTypeInfo::name`, our `StubNop` returns whatever is in RAX. Nothing dispatches
it today (`ATTENDANCE TypeName=0`), and it cannot be reached by inheritance since
no subclass inherits slot 1 — a caller would have to be an exe-side override we
have not ported.

---

## 3. The fix

* **`code/Game/Dialog/dialog_controller_vtable.{h,cpp}`** — `Register()` /
  `Apply()`, plus the generated per-class inherited-slot mask table.
* **`tools/dlgctrl_vtable_audit.py`** — the extractor above; `--gen` regenerates
  the mask table from `Application-x64.exe`.
* 20 files × 38 sites carry `DLGCTRL_VTABLE_INHERIT(g_vt, kSlots, "Cls", &StubNop)`;
  the three `UI_DIALOG_STUB` macro types are registered explicitly rather than
  from inside the macro, whose body is shared with 20 `DialogInstance` /
  `DialogControllerConfig` types that have a different base entirely.
* **`standalone/main.cpp`** — one `UIDialogControllerVtable::Apply()` after
  `AddressRegistryInit()`.

**Two design points that are load-bearing:**

*Why register-then-apply.* The tables are built by static initialisers, which run
before `ResolveInit()` loads Runtime-x64.dll and before `RuntimePortInit()`
registers the ported bodies. Resolving at static-init time fills nulls.
`Register()` only records; `Apply()` resolves and fills, and patches the *table*
so objects built either side see the same array.

*Why the base bodies resolve to ours.* Slots 4–10 go through
`ResolveAddr(Module::Runtime, 0x1800cd2x0)`, which consults the port registry
first, and `dialog_controller_base.cpp` registers all seven unconditionally. Slot
3 is deliberately different and easy to get wrong: the thunk retail holds there is
**not** a `DialogController` virtual, it is `?Persist@Internal@Object@@` —
Object::Internal's — resolved **by name**, ported in
`runtime/src/object/internal.cpp`.

### A silent-failure class this nearly shipped

The patcher assumed every site's no-op is called `StubNop`.
`tutorial_dialog_vtables.cpp` calls its `Nop`, and passing `&StubNop` there was
caught by the **compiler** only because no `StubNop` exists in that file. Had the
file contained both symbols it would have compiled and `Apply()` would have
filled **nothing** for that table — a silent no-op, since the stub pointer is the
guard. Two things close it: a static check that only two filler symbols exist in
the whole tree (`&StubNop` ×20 loops, `&Nop` ×1), and the arithmetic below.

---

## 4. Measurements

All runs: debug build, DLL present, under
`tools/game_dir_lock.sh run lane-VTFID 1800 --snapshot …`, logs read from the
snapshot (`lane_vtfid_logs/`). Every run reached gameplay, opened the house
panel, self-quit, and left a **0-byte crash log**.

### 4.1 The arithmetic was predicted before the run

Summing the popcounts of the 38 registered masks gives **176** inherited slots;
the source audit says 166 should be filled and 10 kept. The run reports:

```
[DlgCtrlVt] tables=38 unknown=0 inheritedSlotsFilled=166 keptOverride=10 unresolved=0
```

166 + 10 = 176, exactly. A wrong stub pointer at any of the 38 sites, an
unresolved base body, or a name not in the mask table would each show as a
shortfall in a specific column. None did.

### 4.2 The A/B, on ONE binary

`BANISHED_DLGCTRL_VT=0` is a kill switch on the default-on fix, so both halves
are md5-identical (`f684f60bc193e85d0e22636ce1d08dec`). With a different build as
the control, "the tables changed" and "the exe changed" are the same observation.

Route: `BANISHED_AUTOSTART=1 BANISHED_LIVE_TOOLS=1
BANISHED_TOOL_HARNESS=housepanel` + pause-menu quit — the same route lane ROUTES
used, so the readings are comparable to its.

| | control (`=0`) | fix on |
|---|---|---|
| `[DlgCtrlVt]` | `DISABLED … 38 registered tables keep their file-local stubs` | `filled=166 keptOverride=10 unresolved=0` |
| `[DlgCtrl] ATTENDANCE` | `none of the nine bodies was entered` | `total=1699 OnOpened=9 OnClosed=8 OnUpdateLayout=841 OnUpdate=841` |
| `[HousePanel] OPEN` | yes | yes |
| crash log | 0 bytes | 0 bytes |

The control half reproduces lane ROUTES' control reading exactly, so the pair is
anchored to its. `OnOpened=9 OnClosed=8` reproduces ROUTES' route-driven 9/8 —
**without `BANISHED_DLGCTRL_ROUTE`, on an ordinary run**.
`OnUpdateLayout=841` / `OnUpdate=841` are new: ROUTES deliberately did not patch
slots 7 or 8, and the per-class reading shows those are genuinely inherited where
we now fill them.

`TypeName`, `Type`, `OnCreated` and `OnElementEvent` stay at 0.
For `TypeName`/`Type` that is **not** a gap — §2 shows no subclass inherits slots
1 or 2, so they are unreachable by dispatch by construction.

### 4.3 The in-game A/B

The first pair's log diff showed 39 differing lines in `standalone_error.log` —
almost all of it different citizen names and animal paths, because the two runs
generated **different towns** ("Molensborgett" vs "Opheimera"). That is a
confound, not an effect. Re-running both halves with `BANISHED_SEED=12345`
collapses it:

```
standalone_error.log   distinct 263 / 263   differing = 5
  [DlgCtrlVt] DISABLED …            ctrl 1 / fix 0    (my instrument)
  [DlgCtrlVt] tables=…              ctrl 0 / fix 1    (my instrument)
  [NewGameDialog] town name          "Colbertown" / "Johnstonvick"
  [PlayAnimGuard] … (live)          9 / 8
standalone_debug.log   distinct 918 / 918   differing = 15
  — the two [DlgCtrl] ATTENDANCE lines, plus splash-fade frame counts,
    ChunkedSceneLoader tick counts, MenuScene camera float noise
```

Stated honestly: **`BANISHED_SEED` does not fully determinise this route** — the
town name still differs, which drags a little population/animal jitter with it,
and `[PlayAnimGuard] 9 vs 8` sits inside that residue rather than being
attributable. What the pair does establish is that pinning the seed took the
differing set from 39 to 5, and the only *structural* entries left are the two
lines this lane added. No panel, dialog, element or draw line differs; the house
panel opens and repopulates in both halves; neither half crashes.

---

## 5. The three rows, re-derived

### 5.1 `?_typeInfo@DialogController@UI@@` — **PROMOTED**

Lane TIDENT left it READY-but-held with an expiry condition stated in full: it
expires when `[TidStatic]` prints, and the observable is the **node pointer**
moving out of the DLL's data into our image. Both halves are now measured.

*The consumer is live.* On this tip and route, `[TidStatic]` prints for the row.
TIDENT recorded zero lines across five routes including `housepanel`, so it
became reachable between that tip and this one — **and not because of this
lane's fix**: the line prints identically with `BANISHED_DLGCTRL_VT=0`. That
control is the reason the fix is *not* offered as the warrant. (Which is also why
this row cannot be claimed as "downstream of the fidelity fix", as the dispatch
hypothesised — it is downstream of something else.)

*Something varies with the row*, and it is the thing TIDENT named. Same binary,
same seed, same route, one variable:

```
row off:  node=00007ffcab689e48  id=0xf2c192b8
row on:   node=00007ff672f2d638  id=0xf2c192b8
```

Runtime-x64.dll was based at `0x7ffcab490000`, so the OFF node is DLL+`0x1f9e48`
— exactly the documented `0x1801f9e48` — and the ON node is in our image. The id
is byte-identical, which is the correct outcome, and is why the pointer and not
the value is the observable.

*The negative control is in the same line pair.* `[TidStatic]` also prints
`?_typeInfo@GroupElement@UI@@`, which is **not** armed. Its node stays
`00007ffcab68abd8` in both halves. So the move follows the **armed row**, not the
run, the route or the environment — a distinction one moving pointer could not
make alone.

Promoted in `standalone/reg_mint_promoted.cpp`; the default-run count goes
14 → 15 and the node is in our image with no env set.

### 5.2 `??_7DialogController@UI@@6B@` — **stays off; blocker is now permanent**

The gate on record ("expires the moment any route makes `[DlgCtrl] ATTENDANCE`
print a non-zero total") is now **satisfied** — `total=1699` — and satisfying it
changed nothing, exactly as ROUTES predicted when it showed that counter measures
the nine unconditionally-registered *bodies* and never varied with the row.
So it was re-tested properly instead: same binary, same seed, same route, one
variable (`BANISHED_RT_MINT_ROWS=??_7DialogController@UI@@6B@`). Differing lines:
the two `[RtMint] ROW ON` lines the armed half prints, plus the usual jitter.
**Nothing varies with the row.**

The new part is *why*, and it is no longer a fact about our tree. The row's only
consumer is the ported base destructor `??1DialogController`
(`runtime/src/ui/dialog.cpp:1227`), reachable only through **vtable slot 0** —
and §2 establishes that **not one of the 39 retail subclass vtables inherits slot
0**. Retail itself never dispatches the base destructor from a subclass. No
increase in our fidelity can ever reach it; it needs a bare
`UI::DialogController` instance, which nothing constructs.

That is a stronger statement than "no subclass currently calls it". **No future
lane should spend time hunting a route for this row.**

### 5.3 `??_7Dialog@UI@@6B@` — **stays off, unchanged**

This lane's fix touches `DialogController` subclass tables; `UI::Dialog` is a
different class and a different table, and the same one-variable A/B above (the
row was armed in the same run) shows nothing varying with it. Lane DLG8's finding
stands unaltered: live for the stamp, dead for the dispatch, and arming it would
buy nothing while leaving lane DLG2's ctor oracle carrying three permanent
divergences.

---

## 6. Census

`tools/dlgctrl_vtable_audit.py`, run on the tip, measures the vtable surface
directly rather than through the namespace census: **38 of 38** standalone tables
now reproduce retail's inherited slots (0 before), **166 of 166** stub-occupied
inherited slots filled, **0** remaining.

Minted DATA rows: **14 → 15**. SET DIFF, measured on this branch's tip at the
`promoted` run: the entrant is
`?_typeInfo@DialogController@UI@@0VBaseTypeInfo@Object@@A`; nothing left. It is a
set diff and not a count, because a count that moved by one could equally have
been a swap.

Not moved by this lane: the UI namespace census (the nine bodies were already
ported *and registered* before this wave — what changed is that seven of them are
now **exercised**, which no per-namespace census reports).

---

## 7. Files touched

| file | change |
|---|---|
| `tools/dlgctrl_vtable_audit.py` | **new** — the retail extractor, the cross-check screen, `--gen` |
| `code/Game/Dialog/dialog_controller_vtable.{h,cpp}` | **new** — `Register`/`Apply` + the generated mask table |
| `code/Game/Dialog/*.cpp` (18 files) | one `DLGCTRL_VTABLE_INHERIT` per subclass table |
| `code/Game/Dialog/ui_dialog_stub_factories.cpp` | 21 registrations + the three macro types |
| `code/Game/Tutorial/tutorial_dialog_vtables.cpp` | TutorialUI, registered with `&Nop` |
| `standalone/main.cpp` | one `Apply()` call after `AddressRegistryInit()` |
| `standalone/reg_mint_promoted.cpp` | promote `?_typeInfo@DialogController@UI@@`; rewrite the `??_7DialogController` hold with the permanent blocker |

`standalone/tool_harness.cpp`, `autostart.cpp` and `modal_driver.cpp` were **not**
touched (lane MOUSE owns them this wave); the existing `housepanel` route was
used as-is.

**Build:** debug green; release checked properly with `make clean` first
(`make standalone` from clean, 8.1 MB). Worth recording that the hazard is
**symmetric** — `make standalone-debug` immediately after `make standalone` also
reports "Nothing to be done" and leaves the *release* binary sitting at the debug
path, which would silently mis-attribute any run taken next.
