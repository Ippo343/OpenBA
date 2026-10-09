# B6 — the retail-difference task: rule sheet (written 2026-10-09, start of the step)

Part B's first step that changes what RUNS on purpose. The 63 casts tagged "retail-difference task"
(`b4_session/LEFT_FREE.md` §C, `leftfree_c.py`) are the SYMPTOMS of places where the tree does something
retail does not, catalogued in `docs/known_port_issues.md` §13-14 (14a wrong callee / object / overload,
14b save format, 14c construction / destruction / leaks, 14d guards retail lacks, 14e normalising stores).
Each row is fixed in its own `behaviour:` commit that carries the retail evidence; the fix removes the
cast. The rows of §13-14 that carry NO cast are the same task (`b6_session/INVENTORY.md` §B counts them).
Session kit: `b6_session/` (gate scripts copied from `b5_session/`; `runs/baseline` and the warning
baselines copied, since no code changed between the B5 close and this step).

Starting numbers (main `338120596`, B5 CLOSED): PROD 89 = 63 retail-difference + 23 named prerequisite +
3 ruled + 0 work; code 74, runtime 15, standalone 0, engine 0, dx11 0. Acceptance 1 line off (10 = 21,
accepted). Unit 920 / 0. Invariants: new game 5683 / 0, tutorial 5773 / 12, typetable `b983f144`, resave
5/5 == golden `Gate.resaved.md5`.

## 0. Scope (the user's, NEXT_SESSION §0 and §2)

The DANGEROUS casts go; only safe casts stay. The differences from retail are fixed HERE, row by row, as
`behaviour:` commits with the retail evidence. A raw-storage member becomes a real member only in a
commit that reproduces retail's constructor AND destructor. Device-lost stays DEFERRED (NEXT_SESSION §3:
nothing in `OnVideoError / OnVideoRestart`, `Finish`, `Resize` is touched). The 23 named-prerequisite
and 3 ruled casts are NOT this step's (§4 of NEXT_SESSION).

## 1. Decisions

D1. **The unit of work is one known_port_issues row = one `behaviour:` commit.** A row whose fix spans
    several files (452: residenceui + entity_findcomponent + toolbardialog) is still one commit. Two rows
    that are one defect seen twice (400 / 550 / 552; 35 / 43 / 80; 392 / 510; 664 / 690; 575 / 660) are
    one commit naming every row.
D2. **What a `behaviour:` message must carry** (§14g: a sentence nobody verified is not evidence):
    - the row number(s) and the file(s);
    - the retail address of the body AND the instruction range read (`0x1400e5c25-0x1400e5c34`), what
      those instructions do in one line;
    - BEFORE: what the tree did (the observable: which object / overload / value / order);
    - AFTER: what it does now, and the OBSERVABLE difference a run would show (or "none observable:
      dead code at save version 6" / "allocation size only");
    - the cast(s) removed (`file:line`), and the metric delta;
    - what was RUN: "static gate only (debug / perf / unit builds, 0 new warnings)" for a block commit;
      the coordinator's merge adds the gate. Never claim a run that did not happen.
D3. **Order.** Non-save rows first, by bucket, in parallel blocks (§3). 14b rows LAST, serially, by the
    coordinator, each alone in its own commit with its own resave run: PASS keeps the golden (the value
    round-trips through the fixed reader and writer); a DIFF is read with `reftable_diff.py` and the
    `[Roundtrip]` offset, must be the row's own field, and re-cuts the golden in the SAME commit
    (`tools/resave_gate/README.md`: delete the golden, run twice, commit the md5). A DIFF that is not the
    row's field is a finding, not a re-cut.
D4. **A guard retail lacks is DROPPED, not kept** (CLAUDE.md: a faithful crash is upstream's null; a
    guard skips the behaviour instead of restoring it) — with two exceptions, both stated in the comment
    at the site: (a) the comment gives a STANDALONE-SPECIFIC reason (a panel template shared by several
    dialogs, a component the standalone's partial factory tree legitimately does not build, the harness
    routes); (b) the guard is the tree's only protection against a PORT GAP (an unported body that would
    have built the object). Everything else: the probe / test / early return goes, the read is retail's
    unguarded read. If a gate route then crashes, the crash is the FINDING: root-cause the upstream null
    (record it as a new row if it cannot be fixed in the step) — never re-add the guard. The vptr probes
    with casts (playanimation, citizenai_growup, destroycomponent_states, statusiconcomponent,
    buildrequirementui, townhallui, pickup, loaderdialog, userinterface) are the 14d rows with casts and
    are all in scope; the no-cast KEPT bullets of 14d are in scope where the row is CONFIRMED with an
    address (a SUSPECTED bullet is read first; if retail has the guard too, the bullet is closed as
    "not a difference" with the evidence).
D5. **Dead code is fixed to retail's shape anyway** (row 95's `!InstantBuildPath` arm, 400 / 552
    `SpawnSubResource` and slot 8, 810's version < 2 arm, 230's version-5 clamp, 430's audio branch): no
    gate risk, the cast goes, the message says "dead at ... today".
D6. **A SUSPECTED row is read before it is fixed.** If retail turns out to do what the tree does, the row
    is closed "NOT A DIFFERENCE" with the evidence in a doc-only commit (`B6:` prefix) and the cast, if
    any, is retyped as what it is (or stays with a corrected tag naming its real reason).
D7. **Wrong-object rows where retail reads past a type's end** (452: retail 0x1400b56a0 reads +0x50 /
    +0x58 of what 0x140006270 returns) are settled by reading the `RegisterType` that writes the finder's
    type cell. The tree then reads the SAME component retail reads, by name, through that class's header
    (if retail reads ResidenceComponent through a StorageComponent-named cell, the cell is the fact and
    the header's name was wrong). No "fix" of retail.
D8. **Construction / destruction rows (14c)** follow B4's H5 rule: one commit per class, the ctor's stores
    and the dtor's releases in retail's ORDER, the member declared at its retail offset (`static_assert`),
    the unit test that pinned the raw field rewritten in the same commit. A `Collection::Set` member's
    +0x0c pad store (rows 640 / 651 / 652) is fixed ONCE in `Collection.h` (the Set ctor writes `_root`
    and `_count` only), gated alone.
D9. **Overload rows (14b 16, 18, 19; 820; 61; 63)** change the declared type of the field or the call to
    the overload retail's import slot names; the binary encoding writes the same bytes for int / uint
    and short / ushort, so the resave is expected to PASS — the message still says which slot.
D10. **Row 13 (`TradeWork::Validate`, the stale entry)** is a debugging unit, not a conversion: one opus
    agent, allowed to run the stress route (`BANISHED_STRESS=1 BANISHED_TRADE_SELL=600
    BANISHED_TRADE_SELL_SEED=50`) under the lock as holder `trade`, on the perf exe
    (`feedback_stress_repro_on_perf_exe`), with instruments it REMOVES before the hand-in. The fix is
    whatever retail path removes the citizen from the tree; no check at the fault site.
D11. **Rows that change the generated world or the gate's loader counts** (90: a local Random instead of
    the world Random; 92: the material resolve) are expected to change `worldcmp.sh`'s lines (90 changes
    the particle draws only, which worldcmp does not print; 92 may change the loader count). The gate's
    `5683 / 0` invariant is re-cut ONLY if the message explains the delta from the row (e.g. three
    generation-cached Finds replace three bare Finds). `oldbase` moves to the B6 close commit at the
    close, by decision, recorded in CLOSE_NUMBERS.
D12. **The TUT forwarder family (rows 35 / 43 / 80)**: `WorkComponent_CalcMaxWorkers` (tutorial_base.cpp)
    is a second port of 0x14012e5c0 with two guards; the member `WorkComponent::CalcMaxWorkers` is the
    faithful one. The fix: every caller calls the member; the forwarder is deleted. One commit.

## 2. Per-row recipe (what a block does for one row)

1. Read the row IN FULL in `known_port_issues.md` and the INVENTORY row; read the tree's site and its
   comments; read retail at the quoted address (decompile + disassembly, `program="Application-x64.exe"`
   or `"Runtime-x64.dll"` passed EXPLICITLY — never `switch_program`, the Ghidra instance is shared).
2. Write the fix: the smallest change that makes the tree do what the instructions do. No new helper
   unless retail has the function. No null guard added. A typed local where retail has a stack record.
   Keep the retail address comments; replace the `LEFT` / `DEFERRED (retail-difference ...)` tag on the
   line by nothing (the cast is gone) or, if a cast legitimately remains for ANOTHER reason, by that
   reason's tag (the coordinator rules it).
3. Build `-j2` debug + perf + unit (`mingw32-make -j2 standalone-debug`, `standalone-perf`, `unit`;
   600000 ms timeout, foreground, re-invoke). 0 new warnings against `b6_session/runs/warn_baseline_*.txt`
   (the gate's `norm` rule: `grep 'warning:' log | sed -E 's/:[0-9]+:[0-9]+: warning:/: warning:/' | sort -u`,
   then `comm -23` against the baseline). Unit 0 failures. `castmetric.py` on your worktree: the row's
   casts gone, nothing new.
4. Commit: `behaviour: row N — <what>` with the D2 content. One row per commit.
5. Update the row in `known_port_issues.md`: append ` | FIXED (B6 <block>, <commit>): <one line>` to its
   status cell — APPEND to the row, never rewrite other rows (the merge keeps both sides by line).
6. Hand in: branch, the commit list (row -> hash), per row the evidence summary, castmetric before /
   after on your tree, the static-gate numbers, anything NOT fixed and why (a RE-READ that failed, a
   dependency on another block's file).

## 3. Shape and order

- **Block 0 (done first, opus, read-only):** `b6_session/INVENTORY.md` — the 63 casts joined to their
  rows with verdicts, the no-cast rows, dependencies, counts.
- **Blocks (opus, worktrees from main, `-j2`, at most four building at once, DISJOINT files):**
  | block | bucket | rows (INVENTORY decides the final list) | files owned |
  |---|---|---|---|
  | A-WORK | 14a in Action / Work / Component | 1, 44, 45, 46, 51, 52, 220, 221, 260, 392 / 510, 400 / 550 / 552, 430, 511, 643, 704, 861, 35 / 43 / 80 | `code/Game/Action/*`, `code/Game/Work/*`, `code/Game/Component/*` named by those rows, `tutorial_base.cpp` |
  | A-UI | 14a in Dialog / Game / Player | 2, 3, 4, 7, 8, 9, 10, 11, 36-41, 452 (+ entity_findcomponent, toolbardialog), 572, 60, 62, 70, 76, 90, 92, 95, 735, 741, 664 / 690 | `code/Game/Dialog/*`, `code/Game/Game/*`, `code/Game/Player/*` named by those rows |
  | C | 14c | 42, 50, 91, 104, 370, 723, 640 / 651 / 652 (Collection.h, alone) | the class headers and .cpp named by those rows; `runtime/types/collection/Collection.h` |
  | D | 14d | the cast rows (playanimation 83, citizenai_growup 184 / 850, destroycomponent_states 210 / 391, statusiconcomponent 63, buildrequirementui 83, townhallui 1087, pickup 514, loaderdialog 86-87 / 572b, userinterface 1225 / 1250 / 371, mapgenerator 287 / 470, menudialogloopt_stubs 500, splash_loop_stubs 265) then the CONFIRMED no-cast KEPT bullets | the files named; D edits a file another block owns ONLY for the guard line, and says so (the coordinator merges D last among the blocks) |
  | TRADE | 13 | the stale-entry fix | `code/Game/Work/tradework.cpp`, `completetask.cpp`, the citizen removal path |
  Row 14e's open items are value-identical (SETTLED / CONFIRMED in the section); the one cast-free
  SUSPECTED site (row 311, `Cemetery_SpawnTombstone_6a90`) goes to A-UI.
- **14b (coordinator, serial, each alone):** rows 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 61, 63, 230,
  810, 820 — in that order, non-golden-changing overload rows first (16, 18, 19, 61, 63, 820), then the
  layout rows (13, 14, 15, 17), then the settings rows (21, 22), then the dead arms (230, 810).
- **CLOSE (coordinator):** `leftfree_c.py` regenerated (`LEFT_FREE.md` §C: the retail-difference group
  at 0 or at the residue with each residue's reason), acceptance line 01's target = the new PROD total,
  known_port_issues §13-14 statuses, `worldcmp.sh` vs `oldbase`, `stepclose.sh`, the independent audit,
  CLOSE_NUMBERS, STATUS, NEXT_SESSION rewritten.

## 4. The gate (every merge, coordinator only)

`b6_session/mergegate.sh <branch> <label> "<title>"`: builds the MERGED tree before the merge commit
exists, then `gate.sh` (debug / release / perf / unit builds, 0 new warnings, new game `5683/0`, tutorial
`5773/12`, loadgame, the -O2 new-game route, typetable == baseline, resave 5/5 == golden). `ui` and `tools`
routes on the A-UI and D merges (the guards dropped there are on dialog paths). `devlost.sh` only on a
merge that touches a video object (none planned). A crash on a route after a D merge is read with
`standalone_crash.log` first (skill `triage-run`), bisected by the block's commits, root-caused upstream.

## 5. Working rules (B5's, unchanged, plus this step's)

- Agents never run the game (D10's TRADE agent is the one exception, under the lock, holder `trade`);
  builds `-j2` in the foreground; at most four building at once; the coordinator alone gates. No
  `git stash`, no amend / rebase / push, no sub-agents, no `taskkill /IM`. A fresh worktree is reset to
  the NAMED base commit (`git log -1` before editing). Merge main before the final build.
- Commit the gate table (and anything else) BEFORE `mergegate.sh`: it refuses a dirty tree.
- A long close runs with `nohup bash … &` from a short foreground call.
- Never `git add` the session directory wholesale (`runs/`, `gate_*.out`, merge logs are untracked).
- Write files that contain backslashes with the Write / Edit tool, never a heredoc.
- A marker proposal is not a marker; no ceiling rises in this step.

## 6. Acceptance at the close

`b4_session/acceptance.sh` line 01 = the new PROD total with the retail-difference group at its residue
(target 0; a residue is listed row by row in LEFT_FREE §C with the reason it stays and a known_port_issues
row that says what is owed); lines 02-09, 11, 12 at 0; 10 = 21 (accepted). Unit 0 failures. Every §13-14
row's status cell ends in FIXED / NOT A DIFFERENCE / LEFT (with the owing row). `stepclose.sh` clean.
`worldcmp.sh` identical, or DIFFERS with every differing line attributed to a row in CLOSE_NUMBERS.

## 7. Corrections (dated, appended as blocks find them)

2026-10-09, from the coordinator's own SAVE unit (the 14b rows, done first while Block 0 ran):
- C1. **Rows that share one retail body are one commit** (D1 refinement): rows 13 / 14 / 15 / 810 are all
  `CitizenBehavior::Serialize` 0x14008f760; the message lists every row with its own BEFORE / AFTER.
- C2. **The 14b rows merge FIRST, not last**, because every one of them turned out byte-identical in the
  binary encoding (overloads of the same width, a field moved within its record, a dead arm, a clamp the
  gate's save already satisfies): no golden re-cut is expected, so one gate with resave x5 covers them;
  a DIFF would still be bisected row by row on the branch. D3's "each alone" stays the rule for any 14b
  row that CHANGES bytes.
- C3. **A worktree's copy of `runs/warn_baseline_*.txt` is CRLF** (`core.autocrlf = true` on checkout; main's
  copy is LF): a block's warning diff must `tr -d '\r'` the baseline first or every line reads as new.
  The gate runs from main and is unaffected.
- C4. Row 18 was already retail (B4 typed the four bytes `int8_t` with the signed-char overload); row 20 is
  NOT A DIFFERENCE (read: Fruit +0x30 float / +0x34 bool; LayoutDescription's enums through the uint slot).
- C5. **A faithful crash can point at a FIXTURE.** The COMP merge's resave runs died at frame 60 once
  `CheckEducation` read the profession unguarded (row 1): the 2026-09-12 `Gate.sav` held 11 citizens
  with a null profession, born under the old tree. Retail never makes one; the live paths all pass
  one; the save was the upstream null (row 990). Fixture and golden re-cut (`tools/resave_gate/README.md`);
  no guard, no load-time fixup. The rule stands: root-cause the null, and the root may be a file.

2026-10-09, at the close (from `b6_session/AUDIT.md`: 104 OK, 4 FINDING, 2 QUESTION on the 110
`behaviour:` commits audited; F1-F7, Q1-Q7):
- C6. **D7 ruled for row 452:** retail's `GoToCitizen` 0x1400b56a0 reads +0x50 / +0x58 PAST a 0x40-byte
  StorageComponent (cell 0x1401c7e78 has one writer; the Type compare is exact). The coordinator ruled
  (b): the tree reads the ResidenceComponent's family rows by name, marked `STANDALONE DEVIATION`, and
  `docs/retail_bugs.md` §2 records the over-read. Scope: this one site; every other retail quirk met in
  B6 is COPIED (rows 44, 49, 961).
- C7. **A retail RACE is copied** (rows 47 / 48, row 961): the pathfinder's unlocked result copy is retail's
  three-instruction window; the B4 lock that hid it is gone. If the AV returns on a route it is the
  FINDING to record, not a reason to re-lock. The `atomic_signal_fence` only keeps GCC from hoisting the
  two loads; it is the tree's spelling of retail's re-reads, checked on the perf object.
- C8. **An uninitialised `bool` member is retail's byte** (row 27, `workEnabled`): the ctor leaves +0x70 as
  allocated, Persist moves the raw byte; no tree reader tests it before Command stores it. Policy: a
  member retail leaves unwritten stays unwritten; a reader that needs 0 / 1 is a finding at the reader.
- C9. **`IsDerivedFromNullSafe` IS a guard.** Blocks introduced it in new narrowing code (rows 7, 8, 10,
  40, 92, two find templates): row 993 owes the member spelling. D4 applies to helpers, not only to
  `if (p)` lines.
- C10. **The audit's documentary findings** (F1 820's body address is 0x1400c3140 not 0x1400c3100; F2
  G39 rides in G40's commit 419bef6d0; F3 the "unconditional MemFree" comments made false by row 104;
  F4 the QueryCount forwarder (CLOSE-C) and 11 include comments; F6 §10 / §13 headers and row 452's
  cell; F7 CLOSE-B's message) are corrected in the close commit where a file can be edited; the two
  messages stand as written (no amend), recorded here and in CLOSE_NUMBERS §7. Rows 991 (104 sites
  with the retired "partial" reasons and no row), 992 (ProfessionList's refs on the StringTable
  ExternalPtr), 993, 994 and the rulings owed on 921 / 922 (+ TickIdle's `smV &&`) are the B6 RESIDUE,
  handed to B7.
