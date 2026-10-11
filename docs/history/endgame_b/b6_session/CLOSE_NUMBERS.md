# B6 (the retail-difference task) — close numbers (2026-10-09)

Start `338120596` (B5 CLOSED) -> close `a8e786963 (CLOSE-D)` (Merge b6-CLOSEC, the last gated code merge) + the close
documents. 206 commits (160 `behaviour:`), 11 gated merges (GATE_TABLE.md rows 1-11), 0 problems left
standing; 222 files, +4,324 / -3,482 lines.

## 1. What the step did

- **Every "retail-difference task" cast is gone (63 -> 0)** because the difference under it is fixed: one
  `behaviour:` commit per retail BODY, each quoting the instruction range read, BEFORE / AFTER, the
  observable and what was run (B6_RULES D2 / §14g). The §13-14 rows of `known_port_issues.md` carry
  `FIXED (B6 <block>, <hash>)` (111), `NOT A DIFFERENCE` (2: rows 20, 370), `DROPPED` with the address
  (the 14d bullets), or a KEPT ruling under D4(a) / (b) naming the harness route or PORT GAP.
- **The save-format rows (14b) first** (SAVE): 13, 14, 15, 16, 17, 19, 21, 22, 61, 63, 810, 820 — every one
  byte-identical in the binary encoding, so the golden did not move for them (C2); 18 already retail.
- **Wrong callee / object / overload (14a)**: WORK (45, 51, 220, 221, 260, 643, 861, 44, 52, the one
  `CalcMaxWorkers` body — the MEMBER was the copy that differed), COMP (1 + 551, 400 / 550, 552, 430,
  392 / 510 — every StateMachine tick passes `&Time(0)`, the integer forwarder deleted —, 511, 704, 705,
  280, 850, 390 / 391, 46, 28, 27, 26), UI (452 ruled (b) + `retail_bugs.md` §2, 690 / 664, 572, 3, 7, 8,
  10, 36-40, 60, 62, 70, 74, 76, 92-95, 470, 733, 735, 741, 2, 4, 11, 311, 653, 39 / G75 on one introsort
  core).
- **Construction / destruction (14c)**, C: the `Collection::Set` ctor writes root + count only (640 / 651 /
  652); the weather particles draw from a LOCAL Random and own a real index array (90 / 91); NoiseUI's
  dtor (42); the pathfinder's lock is a member with CriticalSection's real ctor / dtor (50 part), its path
  buffer on the stack and the copy unlocked as retail (47 / 48 — row 961 records the race copied);
  real `IO::Serializer` locals in `userinterface.cpp` (371); `ArrayDtorImpl` tests `_data` (104); the
  renderer's stack records built with retail's stores (32).
- **Guards retail lacks (14d)**: the 14 cast lines (vptr / null-allocator / first-tick clamps / readable-
  memory probes) and 60-odd KEPT bullet lines dropped with the instruction range that has no test (WORK,
  COMP, UI, D); kept under D4(a) / (b) with the reason named: `SelectTool`'s empty-registry return (the
  standalone builds the registry), textSeason's string table, ToolUI's deselect-on-close (EntityUI slot 9
  teardown), StatusIconUI's shared template, EditTool's citizen-position fallback (the 0380 port + the
  cc90 PORT GAP), `CanFindHousing`'s tile test and `InteractNodeCount`'s null description
  (`BANISHED_FORCE_WORK`), Grid_AllEmpty's owner skip, StatusIcon_Toggle's routing.
- **Row 13 SETTLED** (TRADE): the stale `TradeWork` entry was the mirrored find-remove core 0x14002c690,
  fixed on main by a4ba1f7ed (row 742) — found by bisecting the shutdown hang the row's commit shows,
  proven by putting the walk back (52 / 70 removals missed); `CompleteTask::CheckEntity` / `AbstractImpl`
  read the scheduler unguarded (0x1400c5af4 / 0x1400c5aff, 0x1400c5b40).
- **Forwarders deleted** once every caller called the member: `Pf_EntityResolvePos_2e20`,
  `NotifyEntityChanged`, `WorkComponent_CalcMaxWorkers`, `Toolbar_CenterOnEntity_Impl`,
  `StateMachine_TickReimpl`, `CitizenComp_StorageComponent_QueryCount`, `CalcWorkerSlots_Impl`,
  `ShowTool_ShowCarried`, `NaturalResourceComponent_FindComponentByType`, the `RtVideo`-style doors of
  `System::CriticalSection`.

## 2. The metric

| | start `338120596` | close |
|---|---|---|
| castmetric PROD total | 89 (63 retail-difference + 23 named prerequisite + 3 ruled) | **26** (= 0 + 23 + 3 + 0 work) |
| code / runtime / standalone / engine / dx11 | 74 / 15 / 0 / 0 / 0 | 13 / 13 / 0 / 0 / 0 |
| cast_ok / tier_i | 731 / 449 | 720 / 448 |
| acceptance (`b4_session/acceptance.sh`) | 1 line off (10 = 21) | **1 line off: 10 = 19** (the accepted template-field miscount); 01 = 26 of target 26 |
| unit tests | 920 / 0 | **920 / 0** |
| `exe_bodies.h` / `*_funcs.h` declarations | 407 | 401 |
| known_port_issues rows added | — | 900, 920-922, 940-944, 960-962, 990 |

## 3. Gate rows

Eleven merges (SAVE, WORK, COMP, CLOSEA, C, UI, UI2, TRADE, CLOSEB, D, CLOSEC): every one new game
`5683/0`, tutorial `5773/12`, typetable `b983f144`, resave 5/5, 0 new warnings, unit 920 / 0; the ui routes
on UI / D, the tool routes on WORK / COMP / UI / D; GATE_TABLE.md. ONE merge (COMP, row 3) reported 5
problems: resave PASS + a crash log on every run — the FIXTURE finding (§5), after which the same exe
passed 4/4 with crash 0.

## 4. Close checks (on `a8e786963 (CLOSE-D)`)

- `worldcmp.sh` against `oldbase` (`b2a5434c`): identical, 118 lines, seed 12345 (preliminary on the UI
  merge and again at the close: `runs/worldcmp_pre`, `runs/close_worldcmp`).
- `stepclose.sh`: §6.
- Independent audit: `b6_session/AUDIT.md` (§7).

## 5. Found on the way

- **The resave fixture held citizens retail never makes** (row 990): 11 of 23 with a null profession,
  born under the old tree's birth path; once `CheckEducation` read it unguarded as retail, the loaded game
  died at frame 60 after a PASSed roundtrip. Fixture and golden RE-CUT (`tools/resave_gate/README.md`); no
  guard, no load-time fixup. A faithful crash can point at a FIXTURE (C5).
- Row 13 was already fixed by row 742's BST core; the row's commit showed a shutdown HANG, not the crash.
- `keepboth.py` doubles a table row both sides edited; a later merge from a branch that merged main
  de-duplicates it.
- The `CalcMaxWorkers` MEMBER was the port that differed, not the guarded forwarder the row blamed.
- Retail quirks copied, listed for a possible "fixed" flavour later: row 452's over-read (deviated, the one
  STANDALONE DEVIATION of the step), row 961's pathfinder race, row 44's never-notify, row 49's stale dword.

## 6. STEPCLOSE (`runs/stepclose/summary.txt`, on `a8e786963 (CLOSE-D)`)

Mods route exit 0, crash 0; front-end Quit / Options / Achievements exit 0, crash 0; GoalDialog from a tutorial
pause menu opens (1 `OpenGoal` line, killed at its time limit as designed); placement / road / removal tool
routes exit 0 (PlacementTool CREATE -> 1, road 1/1 span placed); in-game save `SAVE OPERATION COMPLETE`, game
exit status 0, crash log 0 (the file-mode DIFF verdict vs the input is expected, as in B5); `sa-stress` exit 0,
clean quit (800 citizens x3, speed 10, 6000 frames, 139 fps, +0.5% vs baseline; recorded, not chased); true
release compile from a clean obj dir exit 0 (940 files, 6.4 MB exe, 191 warnings = the standing set), `make
clean`, clean rebuild of debug / perf / unit exit 0, then the full gate on the clean-built exe: 0 problems,
0 new warnings, unit 920 / 0, new game 5683 / 0, tutorial 5773 / 12, loadgame, -O2 new game, resave 3/3 on
the re-cut golden; `NOT IMPLEMENTED` stubs 0, stub set 0 new / 0 gone; tutorial census citizen=16 herd=16
(15:35-15:49).

## 7. Audit

`b6_session/AUDIT.md` (scope 338120596..6366923fe, the 110 `behaviour:` commits before the D merge; 50 re-read
in Ghidra): **104 OK, 4 FINDING, 2 QUESTION**, plus 3 general findings and 5 general questions. Every finding
is documentary (a wrong body address in row 820's message — 0x1400c3140, not 0x1400c3100; G39 riding in
G40's commit 419bef6d0; comments made false by row 104; the QueryCount forwarder and 11 include comments;
the §10 / §13 headers and row 452's cell; CLOSE-B's message describing retail's control flow without the
`-1 < taskIndex` test) — the code was right in every case; applied in CLOSE-D (a8e786963) where a file can be
edited, the two messages stand (no amend). The questions are ruled in B6_RULES §7 C6-C10 (452 under D7, a
copied race, an unwritten bool member, NullSafe is a guard) or owed to B7 as rows 991-994 and the 921 / 922
rulings. The D merge (86275dafd, 54 commits) is NOT audited: B7's audit starts there.
