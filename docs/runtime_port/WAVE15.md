# WAVE 15 — the 20 `_singleton` data rows

**Dispatched 2026-09-09.** Scope, partition and integration rules.
Read [WAVE15_KICKOFF_SINGLETONS.md](WAVE15_KICKOFF_SINGLETONS.md) first — it is the
scoping document and this file does not repeat it.

## What the wave is, and what it is NOT

The kickoff's central recommendation is adopted: **this wave does not promote 17
rows.** Phase 1 is a DECOMPOSITION — it builds one shared instrument, establishes
the door facts, and closes an ordering hazard. Verdicts and promotions are
phase 2 and 3, scoped from phase 1's output, and every promotion is gated on the
DLL-absent boot (kickoff, closing section).

A row is a LIVE MUTABLE POINTER. A wrong promotion strands real readers. That is
why the wave buys an instrument before it buys verdicts.

## Facts re-verified on the tip (56f5787) before dispatch

The integrator checked these rather than inheriting them, because a wave doc's
claims go stale (`reference_holdback_blockers_are_stale_check_them_first.md`):

* 20 `?_singleton@` rows are minted in `standalone/rt_data_mint_tables.inc`
  lines 2157-2176. Confirmed, with their slot VAs.
* `SaveLoad@Save` is promoted (`reg_mint_promoted.cpp:667`), with the full
  enumeration written above it. `Application@1` carries `RT_UNPROMOTABLE ... |
  HARMFUL` at line 704. Both confirmed.
* **The frozen-static hazard is a FAMILY, not one site.** The kickoff names
  `SaveLoadSingletonSlot()` as "a natural pickup". In fact
  `runtime/src/save/saveload_internal.h:94-108` holds **four** frozen
  `static ... = RtDataExport<...>(...)` accessors, and **three of the four are
  Wave 15 rows in scope**: `SaveLoad`, `FileSystem@IO`, `Modification@IO`,
  `Persistence@IO`. Any of these whose first resolve runs before
  `RuntimePortInit()` keeps the DLL address forever while everyone else moves.
  This is lane ORDER's subject and it is larger than the kickoff implies.
* `runtime/funcs/rt_resolve_cache.cpp` exists; there is **no** `.h` beside it.
  A lane told to "use `RtResolveCached`" must find out what is actually
  exported before writing the word (`reference_a_prescription_read_as_a_completion.md`).
* `tools/rt_mint_writer_audit.py` already decodes ten rip-relative **STORE**
  forms, attributes each to the owning export, and carries a nine-symbol
  negative control read out of Ghidra. Lane INSTR extends this; it does not
  start from nothing.

## Partition — phase 1, three independent lanes

| lane | branch | worktree | subject |
|---|---|---|---|
| INSTR | `wave15/instr` | `C:/code/GIT/wt-wave15-instr` | the shared reference enumerator (reads + writes + pointer-valued), all 20 rows |
| DOOR | `wave15/door` | `C:/code/GIT/wt-wave15-door` | import-table door facts for all 20 rows; what promotion means for a no-door row |
| ORDER | `wave15/order` | `C:/code/GIT/wt-wave15-order` | the frozen-static family and the resolve-ordering hazard |

The three touch disjoint files by construction: INSTR writes `tools/`, DOOR
writes `docs/` + `tools/`, ORDER writes `runtime/`. Any lane that needs to edit
outside its column reports the need instead of doing it.

## Rules for every lane

1. **Your worktree is private. Never `git checkout`/`switch` a branch.** Work
   only in the path in the table above, commit only there
   (`reference_parallel_lanes_need_private_worktrees.md`).
2. **Report your worktree PATH and branch name in your final report**, plus the
   commit hashes you made. The integrator builds from those.
3. **Stop reporting, then stop editing.** A lane that keeps improving after it
   reports invalidates the integrator's measurement
   (`reference_lane_edits_after_report_invalidate_the_measurement.md`).
4. **Never use a bash heredoc** for file content or commit messages. Write the
   file, then `git commit -F` (`reference_heredoc_eats_backslashes.md`).
5. **Do not run the game** without taking `tools/game_dir_lock.sh` — and never
   pipe the acquire into anything, because a pipeline's exit status is the last
   command's and a TIMEOUT then reads as a GRANT
   (`reference_a_piped_acquire_discards_the_refusal.md`). Phase 1 lanes should
   not need the game at all; say so if you think you do.
6. **Every instrument needs a control that can go RED**, and you must state the
   MECHANISM of each miss, not a score. A control scored on a union of screens
   hides a blind screen (`reference_union_control_hides_a_blind_screen.md`).
7. **A grep for a literal is blind to the same fact spelled through a
   variable**, and it never announces the blindness. Four separate instruments
   in this project have shipped with that defect
   (`reference_literal_grep_blind_to_variables.md`).
8. Report what you did NOT establish as explicitly as what you did. "Name the
   route you cannot enumerate" (kickoff §method 4).

## Phase 2 and 3 (not dispatched)

Phase 2 fans classification/verdict lanes over the cheap tier using INSTR's
enumerator and DOOR's table. Phase 3 is the DLL-absent boot as a GATE — not a
planner (`reference_the_boot_is_a_gate_not_a_planner.md`) — before any further
promotion. `MemoryAllocator@System` is explicitly out of scope for the whole
wave.
