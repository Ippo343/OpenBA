# Wave 10C — the nine remaining UI rows, and the debts behind them

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

## The two lanes that are not porting lanes

Half this wave is spent on things already shipped, because two of Wave 10B's
findings say the instruments under them are weaker than the row counts suggest.

### TIDENT — eleven live promotions rest on a screen that may not be able to fail

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
