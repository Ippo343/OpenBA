# WAVE 15 PHASE 1 CLOSED — 2026-09-09

Three lanes dispatched, three merged, integration measured by the integrator on
the merge rather than reported by a lane. **No row was promoted**, which was the
plan: phase 1 is the decomposition the kickoff asked for.

| lane | branch | merge | subject |
|---|---|---|---|
| DOOR | `wave15/door` | `5049dbc` | import-table door facts, all 20 rows |
| INSTR | `wave15/instr` | `94ecaf8` | the shared reference enumerator |
| ORDER | `wave15/order` | `6181ebd` | the resolve-ordering hazard |

Findings and corrections: [WAVE15_PHASE1_FINDINGS.md](WAVE15_PHASE1_FINDINGS.md).

## Integration, measured on the merge

Built `standalone-debug` clean on the merged tree, then ran under the game lock
(held 21:40–21:44, acquired unpiped as `wave15-integrator`):

* **Cell C, default** — `[RtSlotAudit] 0 names resolved through the caching door
  before RuntimePortInit()`. Reproduces lane ORDER's cell C.
* **Cell A, `BANISHED_RT_EARLY_TOUCH=1`** — 4 early, 4 returned UNFROZEN,
  `SaveLoad` **REPAIRED** (`early=…c6f0` the DLL slot → `final=…bb50` our minted
  row), effectiveness 1 of 4, **0 live split brains**.
* **Cell B, `+BANISHED_RT_FREEZE_EARLY=1`** — same 4, all FROZEN, `SaveLoad`
  **SPLIT**, **1 live split brain**. The control goes RED on the integration tip,
  and cells A and B differ in nothing but the §6 rule.
* **Functional** — full `BANISHED_AUTOSTART=1` run to frame 14400: 23425
  entities, 19 citizens with live vitals, components drawing, 0-byte crash log.

Note for whoever repeats this: `BANISHED_AUTOQUIT` **overrides** the autostart
New Game path — the driver injects a *Quit* click at the settled menu instead of
New Game. Combining the two does not produce a bounded in-world run, it produces
a menu-only run that exits 0 and looks healthy. The `[AutoStart]` lines go to
`standalone_error.log` (stderr), not `standalone_debug.log`.

## The two corrections this wave made to its own instructions

1. **The kickoff's Application triage line was false.** It converted Wave 12's
   count of KINDS ("2 WRITE and 24 READ") into a port-state count ("24 of them
   READS IN UNPORTED BODIES"). Measured: **9**, not 24. The integrator repeated
   the error with a false citation to Wave 12 in `WAVE15_LANE_INSTR.md`, so lane
   INSTR was dispatched against a control with a wrong expected value — and it
   scored the control RED and indicted the ground truth instead of bending its
   measurement to fit. `Application@1`'s HARMFUL verdict survives on Wave 12's
   real argument, which never used the reader split.

2. **The kickoff's prescribed fix was the wrong fix.** It called
   `SaveLoadSingletonSlot()` "a natural pickup" for conversion to
   `RtResolveCached`. But `RT_DATA_SLOT` and `RT_EXPORT_ONCE` both expand to a
   function-local static *plus* `RtResolveCached`, and that helper froze a
   non-null hit unconditionally — its own banner said so. So the hazard covered
   **83 RT_DATA_SLOT sites**, not the 4 named (nor the 10 the lane found), and
   the prescribed conversion would have fixed the null half and changed nothing
   about the half that matters. The fix belonged in the shared helper.

Both retractions are recorded where the wrong statement lives, with the
reasoning kept rather than deleted.

## Phase 2 — scope

Classify readers per row and render verdicts, using
`tools/rt_singleton_refs.py` and `wave15/door_table.csv`. Lead candidates,
re-ranked by measurement rather than the kickoff's scan:

* **`AchievementManager@Achievement`** — 6 refs, **0 unported reads**, 1
  unknown-owner. Caveat that must travel with it: its dtor WRITE is unported, so
  "0 unported reads" is not "0 unported references".
* **`Persistence@IO`** — 24 refs, 4 unported reads, 0 unknown-owner. Better than
  its raw count suggests.
* **`InternalFactory@Object`** — 6 refs, 1 unported read.

**`ExternalFactory@Object` is NOT cheap tier** despite 14 unported reads: 127 of
its 151 references (84%) are UNKNOWN-OWNER, in unexported internals no port can
move. It is the row most likely to be mis-scoped as easy.

Rules phase 2 inherits as doctrine:

* **UNKNOWN-OWNER is at least as risky as UNPORTED**, never counted as ported.
  An unexported internal has no name to register, so its read cannot be moved.
* **A promotion must register the ADDRESS as well as the NAME.** 13 live source
  sites reach a Wave 15 row through `ResolveAddr(Module::Runtime, …)`, which
  consults the port registry first and does not pass through `RtResolveCached`.
  `reg_mint_promoted.cpp:669-670` already does both for `SaveLoad`; registering
  only the name would strand those 13.
* **Neither reference route is "the reliable one".** The byte scan manufactures
  hits at non-instruction offsets; `get_xrefs_to` is blind wherever Ghidra's
  disassembly does not reach (6 real references proved invisible to it).

## Open, and deliberately not closed

* **Computed/indirect reaches** — `lea rax,[rip+base] … mov rcx,[rax+0x18]`
  references a base, not a slot, and the 20 slots are packed in one region. No
  address-keyed scan can see them. **Unbounded.** The largest hole in phase 1.
* **The identity of the 204 unattributed owners** — established as unexported
  internals, but not which, nor whether any is reachable.
* **Up to 6 references** possibly missing from lane DOOR's exe `ghidra_xrefs`
  column, for the coverage reason above. Changes no door verdict.
* **The DLL-ABSENT boot is still unmeasured for every one of these rows**,
  including the promoted one. It is a GATE, not a planner, and it belongs before
  any further promotion — phase 3.
* **`MemoryAllocator@System`** — 2188 references, its own wave, entangled with
  the allocator gate. Out of scope throughout.
* **A pre-existing crash, not caused by this wave.**
  `BANISHED_RT_ORPHAN_PROBE=1` faults at end of run (ACCESS_VIOLATION in
  ntdll+0x2FAAD, WRITE of 0x648, RCX=RSI=0x640, exit 3) *after* the probe has
  completed and restored. Lane ORDER stashed its change, rebuilt the `d64fc59`
  baseline and reproduced it identically — same fault address, registers and
  exit code. Attributed to baseline.
