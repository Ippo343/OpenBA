# WAVE 15 LANE INSTR — the shared reference enumerator

**Worktree `C:/code/GIT/wt-wave15-instr`, branch `wave15/instr`.** Work only there.

## Why this lane exists

The kickoff establishes that the wave's real unit of work is **classifying each
row's readers**, and that the per-row reference counts in its table are a
CALIBRATED UPPER-BOUND SCAN, not a measurement — it says so twice and asks that
the numbers never be quoted as measurements. Wave 14's lane SINGLETON produced
the only two exact counts by hand (SaveLoad 5, Application 26).

If three classification lanes each hand-roll their own enumeration, we get three
instruments with three different blind spots and no way to compare rows. This
lane builds the ONE instrument, calibrated on the two known ground truths, so
phase 2 is classification instead of re-derivation.

## Deliverable

`tools/rt_singleton_refs.py` — for each of the 20 `?_singleton@` slot VAs in
`standalone/rt_data_mint_tables.inc` (lines 2157-2176), enumerate every
reference to that slot in `Runtime-x64.dll`'s `.text`, and emit a CSV with one
row per reference:

    symbol, slot_va, ref_va, kind, owning_export, owner_port_state

* `kind` ∈ READ / WRITE / RMW / LEA (pointer-valued) / UNKNOWN-FORM
* `owning_export` — nearest preceding export, the attribution
  `tools/rt_mint_writer_audit.py` already does
* `owner_port_state` — PORTED+REGISTERED / PORTED-UNREGISTERED / UNPORTED,
  derived from the tree, see the trap list below

Plus a short `docs/runtime_port/wave15/instr_report.md` and the CSV committed
beside the script (`reference_same_tool_both_sides_is_not_enough.md`: archive
the script beside its numbers).

## Start from what exists — do not start from nothing

`tools/rt_mint_writer_audit.py` already decodes ten rip-relative STORE forms
with `mod=00 rm=101`, attributes each to the nearest preceding export via
bisect, and carries a NINE-SYMBOL negative control read independently out of
Ghidra's xref database, exiting non-zero if it cannot reproduce Ghidra's answer.
Read it fully before writing a line. Your job is to add the READ, RMW and
pointer-valued (LEA) forms and the per-row output — a large fraction of the work
is already sitting in that file, including the part that makes it trustworthy.

## Controls — the part that decides whether this lane succeeded

An enumerator whose count nobody can falsify is worth nothing here. Three
controls, and **score and report each SEPARATELY** — a control scored on the
union of two screens reads green while one screen is blind
(`reference_union_control_hides_a_blind_screen.md`):

1. **SaveLoad ground truth — must be EXACTLY the five, at the five VAs.** They
   are written out in `standalone/reg_mint_promoted.cpp` (in the block above
   line 667) with address, R/W and owning export. Reproduce all five including
   `??1SaveLoad`'s `MOV qword ptr [rip+d],0`, where **RIP is measured PAST the
   imm32** — the hand scan first reported four because it assumed no trailing
   immediate. If your decoder gets 5 without handling that, find out why before
   believing it.
2. **Application ground truth — 26, of which 24 are READS IN UNPORTED BODIES.**
   From `WAVE12_APPLICATION.md`. Both the count AND the classification split
   must reproduce; the split is the thing phase 2 actually consumes.
3. **A second, decoder-independent route that CAN disagree.** Ghidra
   `get_xrefs_to` on each slot VA via the MCP tools. The kickoff is explicit
   that a second route only helps if it can actually disagree — and theirs did,
   4-vs-5. Report every per-row disagreement and its CAUSE. Do not silently
   reconcile one (`reference_a_reconciliation_retires_the_flag.md`).

State the MECHANISM of any miss, not a pass rate. If a form is undecodable,
emit `UNKNOWN-FORM` rather than dropping the reference — a dropped reference is
a false SAFE, which is the dangerous direction.

## Traps specific to the `owner_port_state` column

This column is where this project has repeatedly gone wrong, so it gets its own
list. Every one of these is a real logged incident:

* **A body PORTED but never REGISTERED reads exactly like an ABSENT one.** Two
  instances, one of them a whole reimplemented dialog. Grep for BOTH the address
  and the mangled name, and by SYMBOL not by class string
  (`reference_half_registered_port_reads_as_absent.md`).
* **An address-based "is it registered?" test reads every FACTORY-registered
  type as DEAD.** The working sweep is Factory-with-no-REGISTER_TYPE
  (`reference_address_registration_check_reads_factory_types_as_dead.md`).
* **A registration whose name is a named constant or a macro is invisible to a
  literal grep.** `runtime_census.py` has had this bug in at least three
  distinct forms (`reference_census_cannot_see_nonliteral_registrations.md`,
  `reference_census_cannot_see_the_macro_doors.md`).
* **You need a comment stripper.** A checker without one reports PROSE as code,
  and the prose most likely to name an address is a comment saying that code was
  deleted — so its false positives cluster on dead things and it MANUFACTURES
  work (`reference_uncommented_checker_manufactures_work.md`). The control for
  the stripper is a count it must NOT move.
* `AEAA` in a mangled name means **PRIVATE, not unexported.** This DLL exports
  its private members (`reference_aeaa_is_private_not_unexported.md`).
* Ghidra MCP's `get_function_metrics` silently ignores `address=` and falls back
  to a NAME lookup (`reference_ghidra_mcp_address_param_silently_ignored.md`).

## Out of scope

Do not render verdicts. Do not promote anything. Do not touch
`reg_mint_promoted.cpp` or any `runtime/` source — those belong to other lanes
and to phase 2. `MemoryAllocator@System` (~10935 scan hits) is out of scope for
the wave; if including it makes your script slow or noisy, emit its rows to a
separate file and say so.

## Report

Worktree path, branch, commit hashes. The three control results scored
separately with the mechanism of each miss. The per-row reference totals as a
table, flagged as the FIRST MEASUREMENT of these rows (the kickoff's numbers are
not measurements). Every route you could not enumerate, named.
