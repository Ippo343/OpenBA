# Wave 9 lane SERIAL — predictions, recorded BEFORE measuring

Written 2026-09-08 against branch `wave9/lane-serial` (worktree
`C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\lane-serial-089e6eb4`,
cut from `071864f`), after the port compiled clean and **before** any instrument
was run on it.

The point of writing these down first is the one the parent lane asked for: an
instrument that moves when flat was predicted is a signal to stop and find out
why, and that is only true if the prediction was not written after the fact.

## P1 — the mint wall

`tools/serial_rowcheck.py .` will report **75 rows examined, 75 CLEARED, 0 WALL**,
up from the 73 WALL it reported before this lane's registration file existed.

The two rows already cleared before this lane started are lane ENC-2's
`?Serialize@BinaryWriter@IO@@UEAAXAEAIPEBD@Z` (`0x1800a8fb0`) and
`...AEAHPEBD@Z` (`0x1800a8fd0`), and this lane deliberately does **not**
re-register them.

## P2 — the door set-diff: EMPTY IN BOTH DIRECTIONS

**OPENED: {} — CLOSED: {}**

Not "approximately zero" and not a count: the claim is that the sets are empty.

* **OPENED = {}** because not one of the 73 bodies resolves anything. Every body
  is a field read or a dispatch through a vtable pointer the caller already
  supplied — the stream's (scalar and `SerializeBytes`), or the codec's own (the
  array forms). `grep -n 'Resolve\|RT_EXPORT_ONCE\|RT_DATA_SLOT'
  runtime/src/io/serialize_family.cpp` must print **nothing**. That is the check;
  it is falsifiable in one command.
* **CLOSED = {}** because a door closes only when something our source used to
  ask the DLL for stops being asked. Nothing in our tree called any of these 73
  before today, so they were never in the residue set, and
  `reference_converting_a_door_is_not_satisfying_it.md` applies in reverse: a
  port cannot remove a name from a set it was never in.

## P3 — the census and the unsatisfied-export scan will be FLAT

`tools/runtime_census.py`'s `called` column will be **unchanged**, for the same
reason as P2: `called` is derived from resolve sites in OUR source, and this
family has none.

`tools/unsatisfied_export_scan.py` will be **unchanged**, because it lists names
our source resolves through a door.

⚠️ `ported` is the one number this lane is NOT confident about, and saying so
here is the point of the file. If the census counts a `RegisterRuntimeExport`
only when the same name also appears in `called`, `ported` is flat too. If it
counts registrations outright, `ported` rises by up to 73 and the coverage
percentage moves without any door closing — which would be a **misleading**
improvement, not a real one. **Prediction: `called` flat. `ported` either flat or
+73, and whichever it is, the honest headline is neither.**

## P4 — the deliverable, stated as a mint fact

The number that means something here is not a census percentage:

> **73 of the 75 `RT_MINT_NAMED` slots across `??_7BinaryReader@IO@@6B@`,
> `??_7BinaryWriter@IO@@6B@` and `??_7Visitor@IO@@6B@` now have ported bodies
> registered under both doors and therefore survive a DLL-less boot. The other
> two are lane ENC-2's and were already surviving. The three tables go from
> 2/75 to 75/75.**

Slot 0 of each table (the scalar-deleting destructor) is `RT_MINT_UNNAMED` and is
**not** in the 75 and **not** this lane's: BinaryWriter's is lane ENC-2's
`0x180023b50`; BinaryReader's and Visitor's remain open and are named as
follow-on work.

## P5 — the probe

`[SerialFam] family probe:` will report `cases=` **around 100** (11 types × {scalar,
array×3 counts} × 2 directions = 88, plus 2 `SerializeBytes`, 2 `Skip`, 3
`GetVersion`, 24 Visitor = **119**), `types=11/11`, `neg=3/3`, and **0 divergences**.

`silent=` will be **non-zero and large** (24 Visitor cases + 22 zero/negative-count
array cases + writer `Skip` = 47), and that is a REPORTED number rather than an
absence, because a case that legitimately emits nothing must not be able to
masquerade as coverage.

**If `neg` comes back less than `3/3` the probe reports FAIL even with zero
divergences**, because at that point the zero is uninformative — the failure mode
in `reference_probe_needs_an_effectiveness_control.md`.

## P6 — what this lane expects NOT to be able to verify live

Every one of the 73 is verified by the probe against the shipped bodies. What the
probe does **not** show is any of them being reached by the real engine on a real
boot: these are vtable slots, and whether the game's own save/load path drives
them is a different question from whether they are correct. So the probe's
`cases=` is a comparison count, not a reachability count, and this lane will
report the two separately rather than letting a large `cases=` read as coverage
(WAVE9.md lane M: `calls=12,896` of which ten did work).
