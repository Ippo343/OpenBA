# WAVE 22 INTEGRATOR LOG

Base `wave22/base` at `37706651`. Five lanes: ALLOC, TIMER, ERRCFG, SYSBODY, GEOM.

## Merge 1 — four lanes (GEOM, TIMER, SYSBODY, ERRCFG), ALLOC still running

Merged in that order. **No conflicts.** `standalone-debug` builds clean, zero warnings.

### MEASURED ON THE MERGED TIP — not summed from lane reports

```
TOTAL   4632   1314 called   1137 ported   9 detour   86.5%
System   262    103 called     86 ported   2 detour   83.5%
internal         183 called    141 ported            77.0%
=> 42 of 177 gap rows (24%) are a function to reimplement.
```

Against the kickoff's re-derived base (`1313 / 1129 / 86.0%`, System `103 / 78 / 75.7%`).

**NO LANE REPORTED THIS NUMBER, AND NEITHER LANE FIGURE WAS RIGHT.** ERRCFG measured
System `81` on its own tip; SYSBODY measured `83` on its own tip. The merged tip is `86`
— base 78 + ERRCFG's 3 + SYSBODY's 5, because the two lanes' rows are disjoint. Each lane
was honest about the tree it measured; **neither tree existed after the merge.** This is
WAVE21_CLOSED §1.1 obeyed rather than re-learned: the wave figure comes from the merged
tip or it does not get published.

`called` 1313 -> 1314 and `internal` 182 -> 183 is **ERRCFG's own self-flagged growth** —
`??1Configuration@` reaches retail's CRT `exit` at `0x18007f8a8` by address, a new
address-keyed door. Porting can grow the cutover surface; the lane declared it before the
integrator measured it.

### CHECKERS — compared as a DELTA against base, not read as absolutes

| checker | base `37706651` | merged | verdict |
|---|---|---|---|
| `dispatch_slot_map.py --check` | — | 110 registrations, **0 mismatches**, 0 double-registered | green |
| `unpromotable_slot_check.py` | 21 tags / 42 expiries, 0 STALE | 20 tags / 41 expiries, 0 STALE, 0 UNDECLARED, 0 MISSING | green, **-1 tag = GEOM's discharged tag**, as predicted |
| `port_bypass_audit.py` | BYPASS=28 UNRESOLVED=3 | BYPASS=28 UNRESOLVED=3 | green |

`port_bypass_audit` was diffed **by set, not by count** (`reference_diff_the_set_not_the_count.md`).
The sets are identical but for `OK-ORACLE` 582 -> 592: SYSBODY's ten new oracle doors.
**No new bypass entered the tree**, which independently confirms ERRCFG's claim that it
converted the `?Display@` `RtOriginal<>` door in the same commit that created the hazard.

The `UNMATCHED ??_7ReaderWriter@IO@@6B@` line **pre-dates this wave** — present at base,
unchanged. Not a merge artefact; it is Wave 21's inherited-numerator item still open.

## PENDING

- Lane ALLOC has not reported. It holds the game directory and is re-running a perf series
  after a contamination (below). Its branch also edits `tools/runtime_census.py`, so
  **the figures above were taken with the BASE census tool** and must be re-taken after
  ALLOC lands — with a note saying which tool version produced which figure
  (`reference_same_tool_both_sides_is_not_enough.md`).

## INCIDENT — three runaway `find /` scans

Three Git-for-Windows `find.exe` whole-filesystem scans (PIDs 17212, 21608, 26308), two
searching for project **memory files** and one for `python.exe`, had each burned ~4,500
CPU-seconds and were each still consuming a full core. Lane ALLOC detected them as a
confounder in its own A/B — `off_1`'s first window read 0.19 ms/iter and its second 0.46
on **one binary** — announced rather than killing them unilaterally, and continued with a
paired design.

The integrator verified the PIDs and command lines, killed all three, and confirmed
termination across `Get-Process`, `Get-CimInstance` **and** `tasklist` (the first CIM query
returned a stale snapshot still listing them — one tool's answer was not enough).

**Two lessons, both bigger than the measurement they delayed:**
1. The confounder was **more than twice the size of the effect** and **non-stationary**, so
   the paired alternating design's correction assumption was violated mid-run. Contaminated
   absolutes were discarded, not salvaged.
2. **Three lanes independently reached for `find /` to locate a memory file.** That is a
   documentation gap, not three lane errors — the paths are fixed and known. Raised for the
   user's decision at close; deliberately NOT encoded in CLAUDE.md mid-wave.
