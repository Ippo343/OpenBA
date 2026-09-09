# Wave 10E — lane FINAL: a verdict on the two `TRAP=0` UI vtable rows

**Worktree** `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-af7f81a00b3abb53a`
**Branch** `wave10e/lane-final`, cut from `wave10/base` @ `c22d952`.
`git merge-base --is-ancestor c22d952 HEAD` → ancestor, checked before any work.

> ## VERDICT: **outcome (2) for both rows.**
> `??_7DialogController@UI@@6B@` is **permanently unpromotable** and now has a
> proof that does not depend on reachability at all. `??_7Dialog@UI@@6B@` is
> **unpromotable today**, with a mechanical, one-grep **expiry condition** that
> names the two exports whose port would flip it. Nothing here was promoted.
>
> **UI's ceiling on this metric is 184 / 186 = 98.9%** — not the 185/186 the
> wave doc quotes; with two permanently unpromotable rows out of 186 called,
> 184 is the arithmetic. Lane DRAG's `BeginDragDrop` is the row that reaches it,
> so once DRAG merges **the UI namespace is AT its ceiling and is finished.**

---

## 0. What this lane did NOT do, and why that is the deliverable

It did not find a route, and it did not promote for tidiness. It replaced two
*absences of evidence* with proofs, and it built the one instrument that can
ever expire one of them. Three of the findings below contradict something
already written down in this tree, so each is stated with the command that
produces it rather than as an argument.

| # | claim on record | status after this lane |
|---|---|---|
| 1 | "not one of the 39 retail subclass vtables inherits slot 0" (VTFID) | **CONFIRMED**, re-derived independently over the full 39 (VTFID's own table covers 38) |
| 2 | "our ported constructors stamp `??_7DialogController` into real objects" (`dialog_controller_base.cpp:117`) | **FALSE.** No constructor in the tree stamps it. Corrected in place. |
| 3 | "our `??0Dialog` stamps the minted table on **every dialog the game builds**" (DLG8) | **OVERSTATED.** Measured live: 0 on a bare `AUTOSTART` run, 16 on the housepanel route, 2 on the tutorial route. |
| 4 | "UI's ceiling is 185/186" (`WAVE10E.md`) | **OFF BY ONE.** 186 called − 2 unpromotable = **184/186**. |

---

## 1. One fact that settles the exe half of BOTH rows

```
tools/dispatch_slot_map.py            # the exe's whole PE import table
```

Application-x64.exe imports **899** symbols — 833 from Runtime-x64.dll, 66 from
KERNEL32 — and **exactly zero of them are `??_7*`**. The exe never names a
Runtime vtable object.

That is not a statement about `UI::Dialog`; it is a statement about the import
directory, and it holds for every vtable row in the DLL. **No exe-side code —
retail's or ours — can ever be a consumer of either row.** Every consumer must
live in `runtime/`, and there the grep is complete and small.

The four `UI::Dialog` symbols the exe *does* import are all non-virtual and
none of them can dispatch:

```
0x140151588  ?GetTemplate@Dialog@UI@@QEBA...      QEBA — non-virtual
0x140151728  ?_typeInfo@Dialog@UI@@0V...A         DATA
0x1401517a0  ?GetReference@Dialog@UI@@QEBA...     QEBA — non-virtual
0x1401528c0  ?CreateElement@Dialog@UI@@QEBA...    QEBA — non-virtual
```

---

## 2. `??_7DialogController@UI@@6B@` — PERMANENTLY UNPROMOTABLE

Four independent proofs. Any one of them is sufficient; they are listed in
increasing order of how little they assume.

### P1 — no exe consumer is possible

§1. The exe imports no vtable symbol at all.

### P2 — exactly one consumer in the tree, and it is reachable only through slot 0

```
grep -rn '??_7DialogController@UI@@6B@' --include=*.cpp --include=*.h .
```

Two reads of the symbol survive the comment lines:

* `runtime/src/ui/dialog.cpp:1227` — inside `RtUiDialogController_Dtor`
  (`??1DialogController@UI@@UEAA@XZ`, retail `0x1800cd2f0`). **The row's only
  consumer.**
* `runtime/src/ui/dialog.cpp:2703` — `ResolveExportRaw`, deliberately raw
  because it is a **decoder oracle** that must see the shipped table. Raw
  bypasses the port registry, so it is not a consumer of the row.

The consumer is reached by: base-vtable slot 0, or `ResolveAddr(Module::Runtime,
0x1800cd2f0)`. The one other place the tree resolves the dtor —
`engine/funcs/engine_funcs.cpp:1752`, `fn_DialogController_Dtor` — is **never
called**: its only other appearance in the whole tree is `engine_funcs.cpp:3811`,
which *prints* the pointer. A resolved-but-never-called function pointer is not
a door.

### P3 — no subclass inherits slot 0, over the FULL universe of 39

Lane VTFID established this over the 38 classes it could compare against a
standalone table. The 39th, `SelectResourceUI`, has no standalone table and is
therefore absent from that table — so the claim was re-derived directly against
the exe, asking the binary rather than the comparison:

```
tools/dlgctrl_slot0_check.py          # new, checked in, over VTFID's own extractor
universe: 39  located: 39
slot 0 INHERITED (a named import thunk): 0
slot 0 OVERRIDDEN (own exe body):       39
slot 1 inherited by: 0 of 39
slot 2 inherited by: 0 of 39
unresolved: []
```

`SelectResourceUI` included: `vt=0x14015d330 slot0=0x140013ca0`, its own exe
body. Retail never dispatches the base destructor — or `TypeName`, or `Type` —
from a subclass.

The script's first draft printed those last two lines as `0 of 128`: it iterated
the 39-name universe but printed `len(found)`, the extractor's whole located map.
The count was right and the **denominator** was a different set — the exact shape
of defect Wave 10D shipped four times. Caught and fixed before it was published;
recorded because a proof is only as good as the number beside it.

### P4 — even if it ran, it could not be observed

This is the proof that does not depend on any route existing, and it is new.
The consumer's entire body is:

```cpp
void RtUiDialogController_Dtor(DialogController* self) {
    static void* const vtbl = ResolveExport("??_7DialogController@UI@@6B@");
    static void* const internalDtor = ResolveExport("??1Internal@Object@@UEAA@XZ");
    *reinterpret_cast<void**>(self) = vtbl;          // <- the row
    reinterpret_cast<void (*)(void*)>(internalDtor)(self);
}
```

and the **first statement** of `??1Internal@Object@@` is a store to the same
qword — in our port (`runtime/src/object/internal.cpp:554`,
`self->vptr = VtblInternal();`) and in retail, whose body that port transcribes
(`internal.cpp:538`). Nothing between the two stores reads `self->vptr`.

So the row's value is **written and overwritten without ever being read**. A
promotion could not change behaviour on any route, present or future. That
makes "find a route" the wrong question for this row: there is no route to find,
*and* finding one would not help.

### Verdict and expiry

**PERMANENTLY UNPROMOTABLE. No future lane should hunt a route.**
Nothing observable expires this — P4 is unconditional. The only thing that could
ever *want* the row registered is cutover residue hygiene (§5), which is a
different metric and an integrator decision, not a port verification.

---

## 3. `??_7Dialog@UI@@6B@` — no dispatch exists today, and the expiry is named

### 3.1 The complete consumer set: four STAMPS, zero READS

```
grep -rn 'ResolveExport("??_7Dialog@UI@@6B@")\|RT_DATA_SLOT(void\*, "??_7Dialog@UI@@6B@")' \
     --include=*.cpp --include=*.h .
```

| site | function | what it does with the value |
|---|---|---|
| `runtime/src/ui/dialog.cpp:2086` | `DialogTempCtor`, the `UI::Dialog` stack temporary inside `?CreateInstance@ElementDescription@UI@@` | stores it at `+0x00` |
| `runtime/src/ui/dialog_ctor_dtor.cpp:351` | our ported `??0Dialog@UI@@QEAA@XZ` | stores it at `+0x00` |
| `runtime/src/ui/dialog_ctor_dtor.cpp:389` | our ported `??1Dialog@UI@@UEAA@XZ`, the dtor's re-stamp | stores it at `+0x00` |
| `runtime/src/ui/dialog_vtable.cpp:313` | our ported `?Construct@Dialog@UI@@` (slot 8) | stores it at `+0x00` |

(The fifth hit, `runtime/src/ui/dialog_vptr_trip.cpp`, is this lane's instrument
reading the shipped table RAW to forward to. It does not move a census number —
the row was already `called` — but it does now appear in the census's consumer
list, and that is said here rather than left to be discovered.)

**Not one site reads the row back.** Dispatch can therefore only happen if a
stamped object escapes to something that virtual-calls it.

### 3.2 Where the stamped objects go

Both live stamps produce a **stack temporary**:

* `?CreateInstance@ElementDescription@UI@@` builds `tempDialog[0x88]` on its own
  stack, passes it as the **third argument** (R8) to `_childDesc`'s vtable slot 8,
  and destroys it with a **direct call** to `??1Dialog` (`dialog.cpp:2225`).
* the 6-arg `?AddController@DialogInstance@UI@@` does the same with
  `tmpDialog[0x90]` (`dialog_create.cpp`, `DialogCtor` / slot-8 call /
  `DialogDtor`).

Neither is ever `delete`d, entered into the object factory, or handed to the
resource system. The only foreign code that can see one is a concrete
`ElementDescription` subclass's `CreateElement`, and everything such a body can
usefully do to a `Dialog*` — `GetTemplate`, `GetReference`, `CreateElement` — is
**non-virtual** (§1's `QEBA` list).

### 3.3 The empirical screen — all NINE slots, with a positive control

Lane DLG8's seven-way `[DlgVtbl] ATTENDANCE` counter is a real instrument, but it
has two structural blind spots and no positive control: slot 0 lives in
`runtime/src/object/deleting_dtor.cpp` (row 38) and slot 4 is the shared
`?Compile@External@Object@@`, so a dispatch through **either** increments
nothing and reads exactly like no dispatch; and nothing had ever shown the
counter *can* move.

**`runtime/src/ui/dialog_vptr_trip.{h,cpp}`** (new, off by default) closes both.
Under `BANISHED_DLG_VPTR_TRIP` the four stamp sites store a nine-entry
**tripwire table** whose every entry records the slot index and then TAIL-JUMPS
to the real body, so an armed run behaves exactly like an unarmed one and any
dispatch — from our code or the DLL's, through any of the nine slots — is
counted and named. `=2` additionally fires **one** dispatch of slot 3
(`GetFileVersion`, `MOV EAX,6; RET`, reads and writes nothing) on the first
stamped object: the positive control.

It is independent of the mint — it needs no `BANISHED_RT_MINT_ROWS` — so it is a
genuinely separate screen rather than DLG8's screen read twice.

All runs: debug build, DLL present, under
`tools/game_dir_lock.sh run lane-FINAL 1800 --snapshot ./lane_final_logs/…`,
logs read from the snapshot. Deployed md5 `9f524716f86487249b94aa7419b6d7e1`
printed on **both sides** of every run.

| run | route | `stamped` | `control` | **`foreignDispatch`** | control verdict | crash log |
|---|---|---|---|---|---|---|
| `r2` | `AUTOSTART` + `LIVE_TOOLS` + `TOOL_HARNESS=housepanel` + pause-menu quit, `SEED=12345` | **16** | 1 | **0** | **CAUGHT** | 0 bytes |
| `r5` | `AUTOTUTORIAL=1` + pause-menu quit | **2** | 1 | **0** | **CAUGHT** | 0 bytes |
| `r1` | bare `AUTOSTART` + pause-menu quit | **0** | 0 | 0 | **NOT CAUGHT** | 0 bytes |
| `r3`/`r4` | Mods menu (`AUTOMENU_BUTTON=7 …`) | — | — | — | — | **2101 bytes** |

Read the table by the `stamped` column first.

* **`r1` is UNINFORMATIVE and is recorded as such**, not as a third zero. The
  tripwire never reached a live object on that route, so its `foreignDispatch=0`
  says nothing about the row — and the instrument said so itself, printing
  `NOT CAUGHT (this run's zero proves nothing)` without being asked. That is the
  `calls=0` failure this project has already paid for
  (`reference_calls_zero_can_mean_always_took_the_uncovered_branch`), refused at
  the instrument rather than in the prose.
* **`r3` gives no reading, and that was A/B'd rather than argued.** The Mods
  route dies at `reimpl-x64.exe+0x144575`, `READ of 0xd0`. The identical route
  with `BANISHED_DLG_VPTR_TRIP` **unset** (`r4`) dies at the same RVA with the
  same registers (`RAX=0xd0 RDX=0x80 RSI=0x40`), so the crash is pre-existing —
  the same one commit `62c7e63` records — and not the probe's.
* **`r2` and `r5` are the answer**: 18 live `UI::Dialog` objects carried the
  tripwire across two independent routes, the control fired and was caught in
  both, and **not one of the nine slots was dispatched by anything.** Run `r6`
  (§4(b), the `r6` reproduction) is a third informative negative from a fourth configuration:
  `stamped=13 foreignDispatch=0`.

The exact log lines behind this table are checked in at
`docs/runtime_port/wave10e_lane_final_evidence/dlgtrip_readings.txt` — the
counters, both crash headers, and the md5 — so the table can be audited without
the (gitignored) 200 KB snapshots.

DLG8's own counter agrees, by a different mechanism, in all three runs:
`[DlgVtbl] ATTENDANCE: none of the seven bodies was entered`.

### 3.4 What DOES dispatch these slots — and why our port cannot reach it

`?_typeInfo@Dialog@UI@@`'s node carries two factory entries:

```
0x180038530  ?CreateNewObject@Dialog@UI@@SAPEAVBase@Object@@XZ
0x180038610  ?CreateInPlaceObject@Dialog@UI@@SAPEAVBase@Object@@PEAX@Z
```

These are how the `Object::External` resource system creates the real,
long-lived `UI::Dialog` objects it then dispatches `Serialize`, `Compile`,
`Destruct` and `Construct` on. **Both are unported** — they are the trap stubs
`RtMintTiTrapStub_24_0` / `_24_1` at
`standalone/rt_data_mint_tables.inc:1816-1817`. Because they are unported,
retail's factory stamps the DLL's table with an in-DLL `LEA` that no
registration can redirect
(`reference_registering_an_address_converts_a_selfcheck_into_an_oracle` names
the same limit for `rel32`).

So the split is exact: **retail creates the Dialogs that get dispatched; our
port creates only stack temporaries that do not.**

### Verdict and expiry

**UNPROMOTABLE TODAY. Do not hunt a route — watch this observable instead:**

> The row becomes promotable **and required** the moment
> `?CreateNewObject@Dialog@UI@@SAPEAVBase@Object@@XZ` (`0x180038530`) or
> `?CreateInPlaceObject@Dialog@UI@@SAPEAVBase@Object@@PEAX@Z` (`0x180038610`)
> is ported and registered. From that point every resource-loaded `UI::Dialog`
> carries whatever `ResolveExport("??_7Dialog@UI@@6B@")` returns, and the
> `Object::External` framework dispatches through it.
>
> * **The check is one grep** for either mangled name outside
>   `rt_data_mint_tables.inc`.
> * **The measurement already exists**: build, then run any route with
>   `BANISHED_DLG_VPTR_TRIP=2`. The row is live the moment
>   `[DlgTrip] … foreignDispatch=` is greater than zero. Read `stamped` first —
>   a run with `stamped=0` has not tested anything.
> * A second, weaker trigger: any new `ElementDescription` subclass whose slot-8
>   `CreateElement` virtual-dispatches on its `Dialog*` argument. The same
>   `foreignDispatch` counter catches it.

---

## 4. Corrections to published numbers

**(a) The ceiling is 184/186, not 185/186.** `WAVE10E.md` states 185/186 while
also stating that *both* rows are candidates for permanent unpromotability. With
`called = 186` and two rows permanently unpromotable, the maximum `ported` is
184. Measured on this lane's tip (which does not carry lane DRAG):

```
tools/runtime_census.py
UI   1625   186   183   5   98.4%
Called but not ported (3):
  ??_7Dialog@UI@@6B@ · ??_7DialogController@UI@@6B@ · ?BeginDragDrop@UserInterface@UI@@…
```

With DRAG merged that becomes `184/186 = 98.9%`, which **is** the ceiling. The
UI namespace is then finished, and 98.9% is its terminal value.

**(b) "our `??0Dialog` stamps the minted table on every dialog the game builds"
is overstated.** Measured `stamped` counts: bare `AUTOSTART` **0**, housepanel
**16**, tutorial **2**. The bare-autostart zero is the interesting one — our
`?CreateInstance@ElementDescription@` runs only when *exe* code calls it through
the import slot; the DLL's own internal calls are `rel32` and unredirectable.

This does **not** overturn DLG8's `divergences=0 → 3` effectiveness control,
and this lane reproduced that number from a different cause. Run `r6`
(`AUTOSTART` + `RT_DIFF=1` + `DLG_VPTR_TRIP=1`, no mint row armed):

```
[DlgTrip] mode=1 stamped=13 control=0 totalDispatch=0 foreignDispatch=0
[RtPort] [DLG2 diff] rounds=6 divergences=3 …
[RtDiff] DIVERGENCE UI::Dialog::Dialog[diff] call #1 ours=byte +0x00 ours=0x40 theirs=0xa0
```

Three divergences, all at `+0x00`, produced by the **tripwire** rather than by
the mint. So DLG8's control is a control on *"`+0x00` changed"*, which is
weaker than *"the row reached a live object"* — the distinction matters because
`stamped` in that run is **13 with `RT_DIFF` on and 0 with it off**, and this
lane has not separated how many of the 13 are the ctor oracle's own private
subjects. Stated, not reconciled.

**(c) A false comment removed.** `runtime/src/ui/dialog_controller_base.cpp:117`
claimed "our ported constructors stamp that table into real objects". No
constructor in the tree stamps `??_7DialogController@UI@@6B@` — §2/P2's grep is
complete. That sentence is a stale prescription read as a fact, and it is the
reason the row kept being sent back for a route. Corrected in place, with a
pointer to P4.

---

## 5. Census proposal — **NOT applied.** Numbers, before and after

`tools/runtime_census.py` is the instrument every wave's numbers come from, so
this lane changed **nothing** in it. What follows is the proposal, with the exact
output it would produce, for the integrator to accept or reject.

### The problem

The two rows are listed under **"Called but not ported"** beside
`?BeginDragDrop@`, which is a genuinely unwritten function. They are not the
same thing: both are ported in full (9/9 and 11/11, `TRAP=0`), and neither can
ever be promoted. A reader — human or a future lane's planner — cannot tell the
three rows apart, and two waves of route-hunting is what that costs.

### The proposal

1. A new source of truth, `UNPROMOTABLE`, listed **explicitly in
   `standalone/reg_mint_promoted.cpp`** beside the promotion list — one entry
   per row, each carrying the doc reference. Not a heuristic in the script: a
   row is unpromotable because a lane proved it, and that is a written decision.
2. `runtime_census.py` reads that list and **tags the rows in the
   "Called but not ported" listing**. It changes no count.
3. One derived line per namespace that has any: the ceiling.

### Before (this lane's tip, verbatim)

```
namespace         total   called   ported   detour  called covered
UI                 1625      186      183        5   98.4%
...
Called but not ported (3):
  export   UI           ??_7Dialog@UI@@6B@
           runtime/src/ui/dialog.cpp, runtime/src/ui/dialog_ctor_dtor.cpp, …
  export   UI           ??_7DialogController@UI@@6B@
           runtime/src/ui/dialog.cpp
  export   UI           ?BeginDragDrop@UserInterface@UI@@QEAAX…
           standalone/mouse_inject.cpp
```

### After (proposed; with lane DRAG merged, so `ported` reads 184)

```
namespace         total   called   ported   detour  called covered   ceiling
UI                 1625      186      184        5   98.9%          98.9%  AT CEILING
...
Called but not ported (2):
  export   UI  ??_7Dialog@UI@@6B@            [PORTED 9/9 · UNPROMOTABLE · WAVE10E_LANE_FINAL.md §3]
  export   UI  ??_7DialogController@UI@@6B@  [PORTED 11/11 · UNPROMOTABLE · WAVE10E_LANE_FINAL.md §2]
  2 of 2 are ported in full and cannot be promoted, so this namespace is AT its
  ceiling of 184/186 = 98.9%. An UNPROMOTABLE tag is a written verdict from a
  named lane doc, never a heuristic; a row loses it the moment its doc's stated
  expiry condition is met.
```

**Every column above keeps its present arithmetic** — `total`, `called`,
`ported`, `detour` and `called covered` are untouched, so every cross-wave
comparison stays valid. The only additions are a derived `ceiling` column, the
per-row tag, and the trailing sentence. `TOTAL` gains the same two-row
subtraction in its own ceiling cell (**`888 / 1107` today; ceiling `1105`
denominator-equivalent — the integrator should decide whether a TOTAL ceiling is
worth printing at all, since the other namespaces have not been audited for
unpromotable rows and a partially-audited ceiling is worse than none**).

---

## 6. Why outcome (3) was rejected

Promoting for cutover completeness was considered and is refused, but the
argument against it is narrower than "the standing rule says so", so it is
spelled out:

* For `??_7DialogController@UI@@6B@` the promotion is not merely unexercised, it
  is **unobservable** (§2/P4). At cutover with the row unpromoted,
  `ResolveExport` returns null, the consumer stores null, and `??1Internal`
  overwrites it on the next instruction. Even the failure mode is inert.
* For `??_7Dialog@UI@@6B@` the same holds for the four stamps: a null at `+0x00`
  on a stack temporary that nothing dispatches is not read either.
* What *would* change is the `BANISHED_RT_STRICT` residue list. That is a real
  and legitimate cutover concern — but it is a **different metric** from the port
  census, and satisfying it by promoting rows into the port column would make the
  port column mean two things at once. If the integrator wants the residue clean,
  the right shape is a promotion list scoped to cutover, declared as such, not a
  quiet reclassification of these two rows as ports.

---

## 7. Flagged, NOT chased

* **`[Element] ATTENDANCE: neither Update nor Persist was entered in this run`**
  on the housepanel route (r2), where lane ROUTES published `[Element]
  ATTENDANCE Update=541` on a route of the same name. That row belongs to lane
  VERIFY this wave. No explanation is offered here: an explanation for someone
  else's anomaly needs a higher bar, and a comfortable one would retire the flag
  *and* stop the search.
* `?_typeInfo@Dialog@UI@@` is a **promoted** DATA row whose minted node carries
  two *trap* factory pointers (§3.4). That is the mint's designed behaviour for
  an unported factory and is not asserted here to be a problem — it is recorded
  because it is the same pair of addresses the expiry condition turns on.

---

## 8. Files

| file | change |
|---|---|
| `runtime/src/ui/dialog_vptr_trip.h` | **new** — what the instrument measures, and the two blind spots in DLG8's counter that it closes |
| `runtime/src/ui/dialog_vptr_trip.cpp` | **new** — nine forwarding tripwire thunks, the recorder, the positive control, the exit report |
| `runtime/src/ui/dialog.cpp` | `DialogTempCtor`'s stamp goes through `TableOrReal`; one `AfterStamp` |
| `runtime/src/ui/dialog_ctor_dtor.cpp` | the ported `??0Dialog` / `??1Dialog` stamps go through `TableOrReal`; one `AfterStamp` |
| `runtime/src/ui/dialog_vtable.cpp` | the ported `?Construct@Dialog@` stamp goes through `TableOrReal` |
| `runtime/src/ui/dialog_controller_base.cpp` | the false "our ported constructors stamp that table" paragraph corrected |
| `tools/dlgctrl_slot0_check.py` | **new** — P3's reproduction: slot 0/1/2 inheritance over the full 39, straight off the exe |
| `lane_final_run.sh` | the lock-friendly deploy+run driver these measurements used |
| `docs/runtime_port/wave10e_lane_final_evidence/dlgtrip_readings.txt` | **new** — the counter lines and crash headers from all six runs, verbatim |

**Build:** debug green (30 MB). Release checked properly — `make clean` first,
then `make standalone` from clean: **8,167,184 bytes**, the release size, not the
debug one. The clean release build also caught a real defect in the new file:
`std::printf` is invisible to `standalone/log_strip.h`'s macro and fails to
compile under `BANISHED_STRIP_LOGS`; the call sites are unqualified now.
