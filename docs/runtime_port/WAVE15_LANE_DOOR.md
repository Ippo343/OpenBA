# WAVE 15 LANE DOOR — what door does each of the 20 rows have?

**Worktree `C:/code/GIT/wt-wave15-door`, branch `wave15/door`.** Work only there.

## Why this lane exists

**We intercept only at DOORS.** An in-DLL `rel32` call or a RIP-relative `LEA`
stamp is not a door (`reference_a_ported_row_can_be_permanently_unpromotable.md`).
For a data row the door is an entry in a consumer binary's IMPORT TABLE, which
we can redirect; a RIP-relative load inside the DLL can never be redirected.

So "does this row have a door, and in which binary" is the fact that decides
whether a promotion can even be dispatched, and it is completely independent of
counting the DLL-internal references (lane INSTR's job). Both halves are needed
before any row gets a verdict.

## The claims to VERIFY, not inherit

`WAVE15_KICKOFF_SINGLETONS.md` asserts a `exe door yes/no` column for all 20
rows and states "the exe imports **15 of the 20**". **Nobody has verified that
number** — it was produced by the Wave 14 integrator alongside the size scan
that the same document warns is a crude upper bound. Treat the column as a
hypothesis to test row by row. If it is right, say it is confirmed and by what
route. If any cell is wrong, that is this lane's highest-value finding.

## Deliverable

`docs/runtime_port/wave15/door_table.md` + a CSV, one row per singleton symbol:

| field | meaning |
|---|---|
| `symbol`, `slot_va` | from `standalone/rt_data_mint_tables.inc:2157-2176` |
| `exe_import` | imported by name by `Application-x64.exe`? if so, the IAT slot VA |
| `dx11_import`, `dx9_import` | same for `VideoDX11-x64.dll`, `VideoDX9-x64.dll` |
| `exe_text_sites` | how many `.text` sites in the exe read that IAT slot |
| `registered` | does our tree already `RegisterDispatchSlot` that slot? where? |
| `accessor_imported` | is the row's `?Get@...` accessor imported by anything? |
| `door_verdict` | DOOR / NO-DOOR / NO-DOOR-BUT-ACCESSOR-DOOR |

For the ~4-5 NO-DOOR rows (`AnimationManager@Graphics`, `Configuration@System`,
`Engine@1`, `Device@Audio` per the kickoff — verify the membership of that set)
answer the kickoff's actual question: **with no import slot, is there anything to
promote INTO at all?** A row nothing outside the DLL can reach may have no
promotion semantics whatsoever, which would be a clean NO-OP verdict rather than
unfinished work. Establish that from the binaries; do not argue it.

## Tools that already do most of this

* `tools/dispatch_slot_map.py <addr>` — the exe's import table is a NORMAL PE
  import table; this reads the exact mangled name for any slot. `--check`
  validates our registrations and **must stay at zero mismatches** — run it
  before and after; you should move it zero.
* `tools/pe_exports.py`, and `pefile` in `tools/ghidra_mcp/.venv/Scripts/python.exe`.
* CLAUDE.md records the ground truth to reconcile against: **833 imports from
  Runtime-x64.dll** in slots `0x140151218`-`0x140152c18`, 66 from KERNEL32 in
  `0x140151000`-`0x140151208`.

## Traps

* **Close the import-by-ORDINAL route explicitly.** No binary imports anything
  from Runtime by ordinal at all — that CLOSES the route rather than failing to
  find an instance, and it must be stated that way (kickoff §method 3).
* **`GetProcAddress` by name**: check whether each name STRING exists anywhere
  outside the export directory in each binary. `GetProcAddress(h,
  MAKEINTRESOURCE(n))` has NO string to find — name that route as
  un-enumerable rather than pretending it is closed (kickoff §method 4).
* **Check pointer-valued references to the ACCESSOR too**, not only to the slot.
  Wave 14's lane had not done this until challenged, and it was the right
  challenge (kickoff §method 5).
* **A registered slot is not automatically a safe slot, and an unregistered one
  is not automatically safe to register.** Some are left null on purpose so a
  dispatch-or-fallback path takes the fallback, and a few are read under the
  WRONG name and work only while null
  (`docs/waves/unregistered_slots_wave.md`). You are only DOCUMENTING here —
  register nothing.
* **A source grep proving a callee is ABSENT cannot tell an unexported internal
  from an export nobody ported yet.** Run the EXPORT TABLE
  (`reference_grep_cannot_tell_internal_from_unported_export.md`).
* Two checks that share a hidden premise are ONE check
  (`reference_two_checks_one_hidden_premise.md`). If you verify the exe column
  twice, make sure the second route could actually have disagreed.

## Out of scope

Render no promotion verdicts beyond the NO-DOOR / nothing-to-promote-into
argument. Change no registrations. Do not count DLL-internal references — that
is lane INSTR and duplicating it wastes a lane and produces a second number
nobody can reconcile.

## Report

Worktree path, branch, commit hashes. The completed table. **Explicitly: which
kickoff `exe door` cells you CONFIRMED, which you CORRECTED, and the verified
count against its claimed 15 of 20.** `dispatch_slot_map.py --check` mismatch
count before and after. Every route you could not enumerate, named.
