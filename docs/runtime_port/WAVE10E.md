# Wave 10E — the last three UI rows, and an honest verdict on the number

**Opened 2026-09-09 on `wave10/base` @ `c22d952`.** Three lanes.

UI stands at **183 / 186 = 98.4%**. This wave intends to be the last UI wave —
but *finishing* is not the same as *reaching 100%*, and one of the three lanes
exists to say which of those is actually available.

## The three rows

| row | state | what it needs | lane |
|---|---|---|---|
| `?BeginDragDrop@UserInterface@UI@@` | unported | a port | **DRAG** |
| `??_7Dialog@UI@@6B@` | ported 9/9, `TRAP=0` | a DISPATCH, or a verdict | **FINAL** |
| `??_7DialogController@UI@@6B@` | ported 11/11, `TRAP=0` | a verdict — its blocker is already proven permanent | **FINAL** |

`BeginDragDrop` is the only genuine port left in the namespace. Lane MOUSE
already published its full 15-instruction listing and field map (`+0x60` dialog,
`+0x70` id, `+0x78` data, `+0x80`/`+0x84` grab offsets) and its tail-JMP target
`?Play@UserInterface@UI@@` **is already ported**, so it should pull in little or
nothing new.

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
   on this metric is 185/186 — in which case that should be recorded with proof
   and the census taught to say so, rather than the wave quietly stopping at
   98.4% and leaving two rows that look like unfinished work;
3. the rows should be promoted anyway for cutover completeness — which would
   contradict this project's own standing rule that a promotion nothing
   exercises is not a port.

**Lane FINAL may return any of the three, including (2).** A proven ceiling is a
better deliverable than a forced 100%, and this project has repeatedly caught
itself when a number was chased instead of derived.

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
