# Wave 10D — finish UI

> ## ✅ CLOSED 2026-09-09 — **UI 183 / 186 = 98.4%** (from 96.2%), seven rows down to three
> Three lanes, all merged, plus the integrator's single wave-close regeneration
> and both promotions. Release green from a genuine `make clean`; the close cell
> exits 0 with a 0-byte crash log.
>
> **`??_7ToolTipDialog@UI@@6B@` and `?_typeInfo@ToolTipDialog@UI@@` are PROMOTED**
> — 17 minted DATA rows now live on every run. The vtable's gate was DISPATCH,
> not readiness, named with an abort condition **in advance**: `operatorNew`
> non-zero, because a ToolTipDialog cannot be constructed without that table
> stamped onto it. Measured `operatorNew=2` against `BeginToolTip entries=2`,
> `bytes=192`, `GetToolTipString/operatorNew == 1`, `SetText/operatorNew == 1`
> with `found=2 wrote=2` — then re-measured unchanged with the promotion applied.
>
> **The three that remain:**
> * `??_7Dialog@UI@@6B@` — green, live for the stamp, dead for the dispatch.
> * `??_7DialogController@UI@@6B@` — green, and its blocker is **permanent**: not
>   one of the 39 retail subclass vtables inherits slot 0, so retail itself never
>   dispatches the base destructor from a subclass. No future lane should hunt it.
> * `?BeginDragDrop@UserInterface@UI@@` — **entered this wave** from lane MOUSE's
>   injector, resolved by name rather than through an address door. The same
>   honest price the wave has paid four times now.

**Opened 2026-09-09 on `wave10/base` @ `190e91c`.** Three lanes, and the scope is
deliberately narrow: **the last seven UI rows and nothing else.** Opening the
next namespace is explicitly OUT of scope this wave.

## Housekeeping fixed before this wave: the worktree seeding bug

Every agent worktree in this repo was being created at **`origin/HEAD` →
`origin/main` → `965361d`, a commit from 24 July** — 605 commits and six weeks
behind. Not intermittent: **35 of 41 `worktree-agent-*` branches sat at exactly
that commit**, across waves 9, 10, 10B and 10C. It only became visible in 10C
because lanes were told to verify their base with `merge-base --is-ancestor`.

Root cause was simply that nothing had been pushed since July, so "the remote's
default branch" was a stale tree. Fixed by fast-forwarding (no rewrite):

```
origin/main         965361d -> 190e91c   (605 commits)
origin/wave10/base  7358732 -> 190e91c   (40 commits)
```

**No integrity damage:** all nine lane branches from waves 10B and 10C descend
correctly from their intended base — every lane caught it and re-cut its branch.
The lanes paid time, not correctness.

**The hazard it would have caused, stated so it is not forgotten:** a lane that
did NOT check would not merely write code against a July tree, it would
**measure** against one. "Grep for the port before writing it" against a
393-commit-old checkout returns nothing and reads as *not ported yet*, so the
lane re-ports what already exists; a census "before" snapshot taken there is
meaningless. That is `reference_half_registered_port_reads_as_absent` with a much
larger blast radius. Lanes still verify their base — the check costs one command
and it is the only thing that made this visible.

## The board

UI is at **177 / 184 = 96.2%**. Seven rows left, and only four of them are
missing a port at all — the other three are ported, `TRAP=0`, and held back for
reasons this project derived and re-derived across two waves.

### The seven remaining UI rows, and what each actually needs

| row | state | what it needs | lane |
|---|---|---|---|
| `??2ToolTipDialog@UI@@SAPEAX_K@Z` | unported | a port | **TIP** |
| `??_7ToolTipDialog@UI@@6B@` | unported | a port (+ a mint row?) | **TIP** |
| `?GetToolTipString@Element@UI@@` | unported | a port | **TIP** |
| `?SetText@ToolTipDialog@UI@@` | unported | a port | **TIP** |
| `??_7Dialog@UI@@6B@` | ported 9/9, `TRAP=0` | a DISPATCH | **VTFID** |
| `??_7DialogController@UI@@6B@` | ported 11/11, `TRAP=0` | a CONSUMER | **VTFID** |
| `?_typeInfo@DialogController@UI@@` | minted, READY | a CALLER | **VTFID** |

All four unported rows entered the census in Wave 10C as `BeginToolTip`'s
callees. All three held rows are downstream of the same fact about our 27
subclass vtables. So the seven rows are really **two problems and one
capability**.

## Lanes

| lane | scope |
|---|---|
| **TIP** | the four `ToolTipDialog` rows — the only actual porting left in UI |
| **VTFID** | the 27 subclass vtables' inherited slots — all three held rows are downstream of this |
| **MOUSE** | a harness mouse/hover injector — the only route that reaches any of it |

### VTFID — ✅ DONE: 166 slots across 38 tables, attendance 0 → 1699

> **The brief's count was wrong and the lane corrected it: 38 tables, not 27.**
> 27 was the number of FILES carrying the `StubNop` idiom; there are 39
> `DialogController` subclasses and 38 standalone vtables (`SelectResourceUI` has
> none). **166 of 418 compared slots changed** — every slot where retail inherits
> and we held a file-local no-op. Zero remain.
>
> **Two earlier designs of the CHECK were wrong, silently**, and the lane recorded
> both in the tool's docstring rather than discarding them:
> * chasing Factory functions and taking the longest `.rdata` run **merged**
>   adjacent tables and **collided** CitizenUI onto DestroyUI — and 3 of the 6
>   vtable addresses this tree had recorded *by hand* came back wrong;
> * a structural scan found 38 against a CSV closure of 38 — and the **set diff
>   was one-in one-out** (gained DebugUI, whose CSV parent omits the `UI::`
>   namespace; lost TradeUI, which inherits only slot 3). **The counts agreeing
>   proved nothing** — `reference_diff_the_set_not_the_count`, again.
>
> The design that worked locates each table by its **own slot-1 `TypeName` stub**,
> so tables name themselves and no threshold is needed. All 39 located; every
> hand-recorded address agrees.
>
> **And the rule is real but NOT uniform: no two classes share a mask.** Applying
> `destroyui.cpp`'s 3/5/6/8/10 to all 38 — exactly what this brief's hypothesis
> invited — would have been wrong for **30 of them**. Third wave running in which
> "the obvious template" was wrong.
>
> Overrides are protected **by construction**: a slot is filled only if it still
> holds that file's own stub pointer. The lane predicted `176 inherited = 166 to
> fill + 10 to keep` BEFORE running; the run reports `filled=166 keptOverride=10
> unresolved=0 unknown=0`. That arithmetic caught a silent-failure class the
> compiler nearly missed (`tutorial_dialog_vtables.cpp` calls its no-op `Nop`).
>
> A/B on one binary, md5-identical halves: `[DlgCtrl] ATTENDANCE` **0 → 1699**
> with no route switch. The control reproduces lane ROUTES' reading exactly
> (`OnOpened=9 OnClosed=8`); `OnUpdateLayout=841 OnUpdate=841` are new.
>
> **The three rows: one promoted, two closed for good.**
> * `?_typeInfo@DialogController@` — **PROMOTED**, with what varies named: the
>   `[TidStatic]` node pointer moves `DLL+0x1f9e48` → our image, id
>   byte-identical, and the negative control *in the same line pair* is that
>   unarmed `GroupElement`'s node does **not** move. The lane explicitly does NOT
>   attribute this to its own fix — it prints identically with the fix off, so the
>   row is downstream of something else. Refusing a flattering attribution it
>   could easily have claimed.
> * `??_7DialogController@UI@@6B@` — **stays off, and the blocker is now
>   PERMANENT rather than pending.** The recorded gate is satisfied (1699) and
>   satisfying it changed nothing, exactly as ROUTES predicted. The new fact: its
>   only consumer is reachable only through slot 0, and **not one of the 39 retail
>   subclass vtables inherits slot 0** — retail itself never dispatches the base
>   destructor from a subclass. **No future lane should hunt that route.**
> * `??_7Dialog@UI@@6B@` — stays off; DLG8's finding stands.
>
> Also flagged: the "Nothing to be done" hazard is **symmetric** —
> `make standalone-debug` straight after `make standalone` leaves the *release*
> binary sitting at the debug path.

The original framing:

### VTFID — the fidelity fix lane ROUTES scoped but did not take

All 27 standalone `DialogController` subclass vtables replace retail's
**inherited** base slots with a file-local `StubNop`. Retail's derived vtables
*copy* the base entry for every virtual the subclass does not override —
`destroyui.cpp:547` says exactly that about retail's own table. Making our 27
reproduce that is a pure fidelity fix, and ROUTES already proved it
behaviour-preserving for slots 5/6/10 (attendance 0 → 17 → 51). It is also the
most likely thing to give `??_7DialogController` and `??_7Dialog` a real
dispatch, which is the only thing keeping either row unpromoted.

### MOUSE — ✅ DONE, and it found the confound

> The injector lands as `standalone/mouse_inject.{h,cpp}` + `ui_hover_detour.*`,
> off by default. It moved all three counters and is the route that verified lane
> TIP's four rows from the other end.
>
> **The finding with the widest blast radius is a registry repair:**
> `GameUIScale` was **0**, and with `UIScale=0` the in-game clip region is
> `(0,0)-(-1,-1)` — **nothing can hover at all.** Every `entries=0` taken on the
> hover path before this fix therefore had TWO sufficient causes, "no injector"
> and "degenerate clip region", and discriminated between neither. That weakens
> Wave 10C lane UIF6's "an unattended run has no mouse" attribution, which this
> integrator reported as settled. UIF6's separate evidence (the same file's
> `IsInsideClipRegion` entering 1014 times) still rules out a dead registration,
> so the conclusion survives — its reasoning was under-determined.

### MOUSE — a capability, not a row

`EndDragDrop`, `EndHover` and `BeginToolTip` all read `entries=0` for one shared
reason: they are **mouse-driven**, and an unattended run has no mouse. Lane UIF6
checked there is no injector in `standalone/` and correctly **rejected a detour**
— a detour changes *who* reaches the body, not whether the game hovers. An
injector closes those three, unblocks hover/drag/tooltip work for every future
UI lane, and converts a standing excuse into a measurement. Same shape as lane
SHOW's `MainLoop+0x90` route, which is the template.

### Out of scope, named so it is not drifted into

Opening `Object` (57.1%, the lowest-completion namespace), `Video` (gap 40) or
`Graphics` (gap 31) is **the next wave's job, not this one's**. A lane that finds
itself porting outside `UI` should stop and report the scope rather than take it.

### The wave-close regeneration — an integrator decision, recorded

`??_7ToolTipDialog@UI@@6B@` (DLL `0x180157880`, 10 slots) is **not** a row in
`standalone/rt_data_mint_tables.inc`, and lane TIP raised that rather than acting
on it. The table is stale, not the rule wrong: `rt_mint_gen.py`'s source scan
matches the literal that Wave 10C's `runtime/src/ui/userinterface_hover.cpp:410`
already spells, so the row appears the moment the generator is re-run. Verified —
zero hits in the `.inc`, literal present at that line.

**Decision: the integrator regenerates ONCE at wave close, after all three lanes
land.** Regenerating mid-wave is a whole-file change to a ~2100-line generated
file that also re-runs `runtime_census.py`, so it would capture a mid-wave
snapshot of every other lane's landings and conflict with anyone who did it
second. Hand-adding the row is refused: the generator exists so nobody hand-adds.

**And the consequence that changes lane TIP's scope: minting is not porting.**
Lane MINT established this by measurement in Wave 10B — it minted `??_7Dialog`
and the UI census set-diff came back EMPTY. A `??_7` row leaves the census only
on PROMOTION. So the sequence is:

1. TIP ports slots 0, 1, 2 **now** (slot 0 as a row of the deleting-dtor FAMILY,
   the way lane DLG8 added row 38 — not a hand copy);
2. the integrator regenerates at close, and the row appears;
3. the shadow must read `TRAP= 0` *immediately*, which is only true if (1) is
   done — otherwise we regenerate into a BLOCKED row with three traps and the
   wave does not finish UI;
4. promotion needs an attendance route, which is lane MOUSE's injector, because
   `BeginToolTip` is what allocates the ToolTipDialog.

Slots 3..9 are claimed to be `DialogInstance`'s inherited copies — already ours
and PROMOTED at 10/10, so genuinely inherited slots cost nothing. TIP is required
to confirm that **by address** against `??_7DialogInstance`'s slot targets rather
than assume it, which matters because lane VTFID is concurrently testing exactly
that "derived tables copy the base entry" hypothesis for our 27 subclass vtables.

### The one dependency between lanes

`BeginToolTip` **allocates a ToolTipDialog** — so TIP's four rows and MOUSE's
injector are the same path seen from two ends. MOUSE owns the harness and builds
the route; TIP ports the bodies and may legitimately report "verified once
MOUSE's injector lands". They must not both build an injector.

## The wave-close sequence (integrator-owned)

Lane TIP froze its branch at `71cbf1d` rather than merging MOUSE and
re-measuring — correct, per
`reference_lane_edits_after_report_invalidate_the_measurement`. The combined
measurement belongs to whoever holds both branches, i.e. the integrator. Order:

1. merge TIP + VTFID + MOUSE;
2. regenerate `standalone/rt_data_mint_tables.inc` **once**;
3. confirm the new `??_7ToolTipDialog` row reads `READY / TRAP= 0`. **If it does
   not, DO NOT promote** — re-derive the slot table in `WAVE10D_LANE_TIP.md` §2
   rather than working around it;
4. run the one combined cell (below);
5. apply warrants 7a and 7b from `docs/runtime_port/WAVE10D_LANE_TIP.md`.

Warrant **7b** (`?_typeInfo@ToolTipDialog@`) does not depend on the run — its
evidence is already measured and shows the row character-for-character identical
to three that ship promoted.

Warrant **7a** (`??_7ToolTipDialog@`) does, and the discriminating counter is
named in advance: `operatorNew`. A ToolTipDialog cannot be built without that
vtable being stamped onto it, so a non-zero `operatorNew` proves DISPATCH rather
than mere readiness. Predicted on MOUSE's measured `BeginToolTip entries=2
rejected=0`:

```
[RtPort] UI::ToolTipDialog ATTENDANCE
    operatorNew=2 bytes=192   GetToolTipString=2   SetText=2   TypeName=0 Type=0
```

**Read these as RATIOS against `operatorNew`, not as absolute counts** — every
absolute scales with the injector script, so an absolute prediction turns a
script change into a false divergence:

```
GetToolTipString / operatorNew == 1   on the EMPTY-string arm
                               == 2   on the non-empty arm
SetText          / operatorNew == 1
bytes            / operatorNew == 192 (0xc0)
```

`bytes/operatorNew == 192` discriminates: it proves the caller is `BeginToolTip`
and not something else. The empty-arm ratio of 1 cross-checks MOUSE's
`nonEmptyString=0` from the other end. `TypeName`/`Type` staying 0 is expected —
they exist for the vtable's `TRAP=0`, not for this route.

**Lane TIP's committed warrant originally predicted `GetToolTipString` = TWO per
tooltip**, which holds only on the non-empty arm. MOUSE's cell takes the EMPTY
arm, so a correct run would have read 1 against a doc promising 2 and **a correct
port would have been reported as broken.** Found by MOUSE's review, raised by
TIP, and fixed by the integrator at merge rather than by TIP — so a reported
lane's branch does not move. The ratios-not-counts framing is MOUSE's, and it is
the more durable fix: it removes a whole class of script-dependent false
divergence rather than correcting one number.

**And the abort condition is stated in advance rather than rationalised after:**
if `operatorNew` is zero while MOUSE's `BeginToolTip` reads `entries=2`, that is
a genuine finding — the DLL's `BeginToolTip` ran and ours did not — and warrant
7a must NOT be applied.

### A confound MOUSE found that weakens earlier evidence, including Wave 10C's

MOUSE repaired `HKCU\…\Banished` `GameUIScale` from **0 to 100**. With
`UIScale=0` the in-game clip region is `(0,0)-(-1,-1)` and **nothing can hover at
all**.

That means every `entries=0` reading taken on the hover path before this fix had
**two sufficient causes** — "no injector" and "degenerate clip region" — and did
not discriminate between them. Lane TIP says so about its own three runs rather
than letting its verdict stand on the stronger reading. It also retroactively
weakens Wave 10C lane UIF6's attribution that `EndDragDrop`/`EndHover`/
`BeginToolTip` read `entries=0` *because an unattended run has no mouse*: that
conclusion was under-determined, though UIF6's separate evidence (the same file's
`IsInsideClipRegion` entering 1014 times behind the same registration path) still
rules out a dead registration.

## Standing rules

As `WAVE10C.md`, plus:

* **Verify your base.** `git merge-base --is-ancestor <base> HEAD` before any
  work. The seeding bug is fixed at the source but the check stays.
* A green readiness screen is not a route; attendance counters are the separate
  instrument.
* **Test a gate before trusting it**: does the quantity it measures actually VARY
  with the thing being gated? Wave 10C's DialogController gate did not, and
  finding the route was what exposed it.
* The release check must be a real one — `make standalone` after a debug build
  says "Nothing to be done" and proves nothing.
* The modal self-test writes to `standalone_error.log`, **not** the debug log.
