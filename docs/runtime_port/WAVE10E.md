# Wave 10E — the last three UI rows, and an honest verdict on the number

**Opened 2026-09-09 on `wave10/base` @ `c22d952`.** Three lanes.

UI stands at **183 / 186 = 98.4%**. This wave intends to be the last UI wave —
but *finishing* is not the same as *reaching 100%*, and one of the three lanes
exists to say which of those is actually available.

## Integrator error, recorded: the lanes cannot see this document

All three lanes were dispatched with base `c22d952`, but this file was committed
at `fca68db`, **after** it. So none of them can read the wave doc they were told
to read. Lane DRAG hit it, said so plainly rather than inventing context, and
worked from `WAVE10D.md` + `WAVE10D_LANE_MOUSE.md` §6 instead. No harm — the
per-lane briefs are self-contained — but the ordering was mine to get right:
commit the wave doc BEFORE dispatching, or dispatch on the doc's own commit.

## The three rows

| row | state | what it needs | lane |
|---|---|---|---|
| `?BeginDragDrop@UserInterface@UI@@` | ✅ **PORTED AND VERIFIED** | — | **DRAG** |
| `??_7Dialog@UI@@6B@` | ✅ verdict: **UNPROMOTABLE TODAY**, expiry named | — | **FINAL** |
| `??_7DialogController@UI@@6B@` | ✅ verdict: **PERMANENTLY UNPROMOTABLE** | — | **FINAL** |

`BeginDragDrop` is the only genuine port left in the namespace. Lane MOUSE
already published its full 15-instruction listing and field map (`+0x60` dialog,
`+0x70` id, `+0x78` data, `+0x80`/`+0x84` grab offsets) and its tail-JMP target
`?Play@UserInterface@UI@@` **is already ported**, so it should pull in little or
nothing new.

### DRAG — ✅ done, and the first A/B was worthless

UI **183/186 → 184/186 (98.9%)**. Nothing entered the census: rows 1107 → 1107,
ENTERED none, LEFT none, exactly one ported cell moved. `?Play@` was already ours
and pulled in nothing, as predicted.

**The lane caught its own useless control before reporting.** The drag subject is
a 40×40 button aimed at centre, so `grabX == grabY == 20` — and a
**transposition** of the two `int32` stores would have been completely invisible.
The first A/B agreed and proved nothing. `BANISHED_MOUSE_DRAGSKEW` broke the
symmetry:

| cell | who ran | grabX | grabY |
|---|---|---|---|
| `skew_on` | OURS | 23 | 13 |
| `skew_off` | DLL | 23 | 13 |
| `skew_swap` | control | **13** | **23** ← caught |

The readback sits OUTSIDE the switch, so neither side reports its own verdict —
the `reference_shadow_marker_is_not_an_oracle` shape avoided by construction.

Two things carried forward:

* **MOUSE's published instruction count was wrong again** — 15, not 11 — and DRAG
  counted it with an instrument twice rather than by eye. Same shape as every
  Wave 10D count defect: the analysis right, the number summarising it wrong.
* **MOUSE's open question is answered and bounded.** No instruction in
  `BeginDragDrop` touches `+0x68`, and a scan of the WHOLE `UserInterface` range
  for stores to `+0x68` finds five instructions at four sites, **all writing
  zero**. No `UserInterface` member sets `_dragDropElement` non-null, so
  `EndDragDrop`'s destroy arm is unreachable as a **property of the class**, not
  as a gap in the injector. That closes a question rather than parking it.
* A census blind spot recorded rather than worked around: DRAG's `Play` call site
  uses the named constant `kPlay`, which `runtime_census.py` cannot see — nor can
  it see `userinterface_frame.cpp`'s. Harmless only because that row is already
  called-and-ported. Its own *registration* is spelled as a literal for exactly
  this reason.

## The question lane FINAL exists to answer

Two rows are `TRAP= 0` — every slot is our code — and both are deliberately
unpromoted, each for a reason derived and re-derived across three waves:

* `??_7DialogController@UI@@6B@` — **the blocker is already proven permanent.**
  Its only consumer is reachable solely through slot 0, and **not one of the 39
  retail subclass vtables inherits slot 0**: retail itself never dispatches the
  base destructor from a subclass. Lane VTFID also showed the gate that was
  supposed to clear it *was never a control for the row* — the counter measured
  bodies registered unconditionally, so it could not vary with the row.
* `??_7Dialog@UI@@6B@` — **live for the stamp, dead for the dispatch.** Arming it
  moves lane DLG2's ctor differential `divergences=0 → 3` (our `??0Dialog` stamps
  the minted table on every dialog the game builds), yet its seven-way attendance
  counter stays at zero, because the temp Dialog is destroyed by a *direct* call
  to our ported `??1Dialog`, never through slot 0.

So the honest possibilities are:

1. a dispatch route exists for `??_7Dialog` and nobody has found it;
2. no route exists, both rows are **permanently unpromotable**, and UI's ceiling
   on this metric is 184/186 *(this doc originally said 185/186 — the integrator's
   arithmetic error, caught by lane FINAL: it is 186 called minus TWO
   unpromotable rows, not one)* — in which case that should be recorded with proof
   and the census taught to say so, rather than the wave quietly stopping at
   98.4% and leaving two rows that look like unfinished work;
3. the rows should be promoted anyway for cutover completeness — which would
   contradict this project's own standing rule that a promotion nothing
   exercises is not a port.

**Lane FINAL may return any of the three, including (2).** A proven ceiling is a
better deliverable than a forced 100%, and this project has repeatedly caught
itself when a number was chased instead of derived.

### ✅ VERDICT: outcome (2), both rows. Nothing promoted. **UI is AT its ceiling.**

**`??_7DialogController@UI@@6B@` — PERMANENTLY UNPROMOTABLE.** Its single
consumer stamps the pointer and then immediately calls `??1Internal@Object@@`,
**whose first statement overwrites the same qword.** The value is written and
overwritten *unread*, so a promotion could not change behaviour on any route that
could ever exist. **Expiry: none. No future lane should hunt a route.**

**`??_7Dialog@UI@@6B@` — unpromotable today, expiry mechanical.** Complete
consumer set: 4 stamps / 0 reads, all onto stack temporaries destroyed by a direct
`??1Dialog` call. The Dialogs the framework actually dispatches on are built by
`?CreateNewObject@Dialog@UI@@` (`0x180038530`) and `?CreateInPlaceObject@Dialog@UI@@`
(`0x180038610`), **both unported**, so retail's factory stamps its own table
through an unredirectable in-DLL `LEA`. **Expires the moment either export is
ported and registered** — one grep to check, `[DlgTrip] foreignDispatch > 0` to
confirm.

**One fact settles the exe half of both rows:** `Application-x64.exe` imports 899
symbols and **zero `??_7*`**. No exe code can ever consume a Runtime vtable row.

**Three published numbers corrected, one of them mine:**

1. **This document's ceiling of 185/186 was wrong — it is 184/186 = 98.9%**, being
   186 called minus **two** unpromotable rows. With lane DRAG merged, **the UI
   namespace is at its ceiling and is finished.**
2. `dialog_controller_base.cpp:117`'s *"our ported constructors stamp that table
   into real objects"* is **FALSE** — no constructor stamps it. **That sentence
   kept the route hunt alive for two waves.** Corrected in place.
3. DLG8's *"every dialog the game builds"* is overstated: the live stamp count is
   **0** on a bare `AUTOSTART` run, and its `divergences=0→3` reproduces here from
   a *different* cause — which shows it was a control on "`+0x00` changed", not on
   the row.

**The instrument DLG8 never had.** `runtime/src/ui/dialog_vptr_trip.{h,cpp}`
(off by default) stamps a 9-entry forwarding tripwire in place of the row's value,
closing DLG8's two structural blind spots (slot 0 lives in `deleting_dtor.cpp`;
slot 4 is the shared `External::Compile`) and adding a **positive control**:
housepanel `stamped=16`, tutorial `stamped=2`, control CAUGHT in both,
`foreignDispatch=0` in both. The bare-autostart run reads `stamped=0` and **the
instrument itself prints "this run's zero proves nothing"** — recorded as
uninformative rather than banked as a third zero.

Slot-0 inheritance re-derived over the full **39** (VTFID's table covers 38;
`SelectResourceUI` has no standalone table): **0 inherit slot 0, 1 or 2.**
Reproduction is checked in as `tools/dlgctrl_slot0_check.py` — whose first draft
printed a wrong denominator, caught and fixed before publishing, and recorded.

**The census was left alone.** The representation proposal (an explicit
`UNPROMOTABLE` list, a per-row tag in the "called but not ported" listing, and a
derived `ceiling` column) is written with before/after output and left to the
integrator, with every existing column keeping its present arithmetic so
cross-wave comparisons stay valid.

## Lane VERIFY — finishing means verified, not merely ported

Several UI bodies are registered, default-on and **ported, not verified**. Lane
MOUSE's injector did not exist when most of those verdicts were written, so some
are now closable and nobody has been back:

* `?Update@Element@UI@@` — 541 entries, never differentially compared;
* `?Persist@Element@UI@@` — a real zero;
* `?OnOpened@DialogInstance@UI@@`'s settings half — five entries, all `noBuffer`;
* the `ToolTipDialog` family's **non-empty arm** — Wave 10D covered only the
  empty arm (`#time` has an empty `+0x58`), so `GetToolTipString`'s second call
  and `SetText`'s other path are untouched. The search key is `+0x58`, set at
  runtime by `?SetToolTipString@Element@UI@@` (`0x1800356e0`) — **NOT** the
  `.rsc` `tipTbl`/`tipIdx` pair at `+0x48`, which selects the arm we already
  have.

## Standing rules

As `WAVE10D.md`, plus what that wave cost:

* **Verify your base** (`git merge-base --is-ancestor c22d952 HEAD`).
* **Do not publish a number you did not compute.** Wave 10D shipped two lane docs
  whose instruction counts were wrong in 4/4 and 2/2 checked cells, while every
  byte size beside them was right. Half a cell computed and half guessed reads as
  authoritative.
* **An explanation offered for someone else's anomaly needs a HIGHER bar than one
  for your own.** A comfortable reconciliation retires the flag *and stops the
  search* — in 10D it hid four further errors for a full round
  (`reference_a_reconciliation_retires_the_flag`).
* A green readiness screen is not a route; **test a gate before trusting it** —
  does the quantity it measures actually VARY with the thing being gated?
* Read counters as **ratios against an anchor**, not absolutes: every absolute
  scales with the harness script, so an absolute prediction turns a script change
  into a false divergence.
* The release check must be real (`make clean` first) — and the hazard is
  **symmetric**: `standalone-debug` straight after `standalone` leaves the
  release binary at the debug path.
* The modal self-test writes to `standalone_error.log`, not the debug log.
