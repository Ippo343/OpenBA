# WAVE 15 LANE ORDER — the frozen-static family and the resolve-ordering hazard

**Worktree `C:/code/GIT/wt-wave15-order`, branch `wave15/order`.** Work only there.

## Why this lane exists

`runtime/funcs/rt_resolve_cache.cpp` §5 documents the hazard: a function-local

    static T* p = ResolveExport("...");

freezes the FIRST answer — **including a non-null one**. So any site whose first
resolve runs before `RuntimePortInit()` keeps the DLL's address forever while
every other site moves to our storage after a promotion. That is a SPLIT BRAIN
on a live mutable pointer, and it would present as a bug in whatever lane
promotes the row, not here.

The kickoff mentions this once, names `SaveLoadSingletonSlot()` as "still a plain
frozen static rather than `RtResolveCached`", and calls it "harmless today and a
natural pickup".

**The integrator checked the tree and the kickoff understates it.**
`runtime/src/save/saveload_internal.h:94-108` holds **FOUR** frozen accessors,
and **three of the four are Wave 15 rows**:

    SaveLoadSingletonSlot()     ?_singleton@SaveLoad@Save@@         PROMOTED already
    FileSystemSingleton()       ?_singleton@FileSystem@IO@@         in scope, ~113 refs
    ModificationSingleton()     ?_singleton@Modification@IO@@       in scope, ~11 refs
    PersistenceSingleton()      ?_singleton@Persistence@IO@@        in scope, ~30 refs

`SaveLoad` is ALREADY PROMOTED, so for that one row the hazard is not
hypothetical — it is live in the tree today. This is a family, not a pickup.

## What to establish, in this order

1. **Enumerate the whole family, not just this file.** Find every site in
   `runtime/`, `code/`, `engine/` and `standalone/` that resolves any of the 20
   `?_singleton@` names, by ANY route: `ResolveExport`, `RtDataExport`,
   `RT_DATA_SLOT`, `RT_EXPORT_ONCE`, `ResolveAddr(Module::Runtime, ...)`, and
   any macro that expands to one of them. Classify each as frozen-static /
   re-resolving / cached-through-the-right-helper.
   * A literal grep is blind to the same name spelled through a variable or a
     named constant and never announces it — four instruments in this project
     have shipped with exactly that defect
     (`reference_literal_grep_blind_to_variables.md`,
     `reference_census_cannot_see_nonliteral_registrations.md`).
   * `ResolveAddr(Module::Runtime, addr)` is **not** "the DLL" — it consults the
     port registry FIRST (`reference_resolveaddr_consults_port_registry_first.md`).
   * `ResolveExport` FALLS THROUGH to the raw impl for an unported symbol
     (`reference_converting_a_door_is_not_satisfying_it.md`).

2. **Find out what `runtime/funcs/rt_resolve_cache.cpp` actually offers.** There
   is NO `rt_resolve_cache.h` beside it in the tree. Do not write the words
   `RtResolveCached` into code or a doc until you have grepped the symbol and
   found its real declaration and semantics — a prescription in a past wave doc
   was read as a completion and the named symbol did not exist anywhere
   (`reference_a_prescription_read_as_a_completion.md`).

3. **MEASURE the ordering; do not read `main.cpp` and conclude.** The kickoff is
   explicit that lane SINGLETON verified ordering by MEASUREMENT — two
   independent sites agreeing on our storage — and that is what made it
   trustworthy. `RtDataMintInit()` is inside `RuntimePortInit()`
   (`standalone/main.cpp` ~1731); `AddressRegistryInit()` and
   `EngineFuncsInit()` run later. Confirm by measurement for the family, at
   least for the already-promoted `SaveLoad` row where a wrong answer is a live
   defect. `BANISHED_RT_ORPHAN_PROBE=1` (`standalone/rt_orphan_probe.cpp`) and
   `BANISHED_RT_MINT_ROWS=<symbol>` reproduce Wave 14's A/B with no code change.

4. **Fix what the measurement shows is wrong**, in `runtime/` only. Prefer the
   smallest change that makes a first-resolve-before-init impossible or
   detectable. A conversion is only worth landing if you can show the difference.

## The control this lane must carry

A change here is invisible without an instrument that could go RED.

* **A predicate written into a warrant and never EVALUATED is not a check** —
  one in this project was false on the very run that approved it
  (`reference_a_predicate_nobody_evaluates.md`). Whatever assertion you add,
  RUN it, and show its value.
* **Test a gate by asking whether its quantity VARIES with the thing it gates.**
  An attendance gate that measured the bodies could never gate the data row
  because the bodies register unconditionally
  (`reference_the_gate_was_never_a_control.md`).
* **A shadow marker is a SELF-CHECK, not an oracle** — it takes the port's own
  verdict and never consults the DLL
  (`reference_shadow_marker_is_not_an_oracle.md`). If you want a predictive
  oracle use `runtime/funcs/rt_predict.h` (ours predicts, THEIRS runs for real).
* If you add a negative control, its subject must be something the compared set
  ACTUALLY compares, or a sound instrument will report NOT CAUGHT
  (`reference_control_must_pick_a_compared_subject.md`).

## If you need to run the game

You probably do, for step 3. Take `tools/game_dir_lock.sh acquire` and **never
pipe it** — a pipeline exits with the last command's status, so a TIMEOUT reads
as a GRANT and you will deploy over another lane
(`reference_a_piped_acquire_discards_the_refusal.md`). Release it when done, and
say in your report how long you held it. Poll boot with `tools/boot_state.sh`,
never a `grep -q` loop on the log — a blocked boot burns the whole timeout.

## Out of scope

Promote nothing. Do not touch `standalone/reg_mint_promoted.cpp`,
`standalone/rt_data_mint_tables.inc`, or `tools/` — INSTR and DOOR own those
columns. If your finding requires a change there, REPORT it; the integrator
lands it.

## Report

Worktree path, branch, commit hashes. The family enumeration with each site
classified, and the blindness of your enumeration method stated. What
`rt_resolve_cache.cpp` really provides. The ordering MEASUREMENT with the actual
numbers, and whether the already-promoted `SaveLoad` row is currently correct or
not. What you changed, and the control result that shows it mattered.
