# Next session plan (written 2026-09-29, B.4 IN PROGRESS — paused by the user for usage)

B.4 (the cast metric) is NOT closed. This file says where it stands and how to resume. The plan B.4
started from is this file at `b2a5434c` (`git show b2a5434c:docs/runtime_port/endgame_b/NEXT_SESSION.md`):
its §0 (owed from the teardown), §2 (carry-overs) and §4 (after B.4) still hold and are not repeated.

## 0. Scope, as the user set it on 2026-09-29

**The DANGEROUS casts go completely; only safe casts stay.** Dangerous = any spelling of a read by
offset or of a `void*` / integer receiver reinterpreted as a class (`reinterpret_cast`, C-style,
`static_cast` through `void*` / `uintptr_t`, offset helpers, `memcpy` field copies). Safe = the
`// CAST-OK: <category>` floor (`b4_session/FLOOR.md`), the three tier-I shapes, arithmetic and
same-hierarchy `static_cast`. The number is
`python docs/runtime_port/endgame_b/b4_session/castmetric.py code runtime standalone dx11 engine`.

**The differences from retail are a SEPARATE task for another session**: `docs/known_port_issues.md`
entries 13 and 14. Blocks record what they find there (W1_BRIEF) and do not schedule `behaviour:`
commits for them. A cast that can only be removed by changing behaviour (the H5 raw-storage
members, entry 14c row 31) is LEFT and listed with the row that blocks it: expect a few hundred.

## 1. Where it stands

| | |
|---|---|
| main | see `git log -1`; last gated merge `af21c719` (GATE_TABLE row 31) |
| metric | 46,226 to remove at block 0 -> **17,621** (62% removed); floor ceilings in `b4_session/FLOOR.md` (total 382) |
| gated merges | 31, every one 0 problems (`b4_session/GATE_TABLE.md`) |
| invariants, unchanged | new game `calls=5683 openFileFail=0`; tutorial `5773/12`; typetable 428 lines md5 `b983f144`; resave golden md5 `dd3893af…`; unit 918 / 0; stub set 0 |

Documents: rule sheet `B4_RULES.md` (M, H, B rules; corrections §8); `b4_session/` — `BLOCK0_BRIEF`,
`BLOCK_BRIEF`, `W1_BRIEF` (the worker briefs), six `INVENTORY_*` with `data/` and `scripts/`,
`castmetric.py`, `acceptance.sh`, `FLOOR.md`, `B4_FILEMAP.md` + `data/filemap.tsv` (30 blocks with
disjoint file sets; regenerate with `scripts/filemap.py`), `GATE_TABLE.md`, the gate scripts.

### First pass (W1), by block

| state | blocks |
|---|---|
| MERGED and gated | W0: R0-EXE, R0-RT, E0, W0-H1, W0-H2 · T · R-UI-a, R-UI-b, R-IO, R-GFX, R-VIDEO, R-ENG · COMP-a, b, c, d, e, f · DLG-a, b, c, d, e · WORK-a, b, c · GAME-a · ACT-a |
| RUNNING (resumed 2026-10-02; branch `b4-W1-<block>`, worktree `.claude/worktrees/agent-*`) | ACT-b, GAME-b, PLAYER, TUT. `known_port_issues` 14 row numbers reserved per block: ACT-b 51-59, GAME-b 60-69, PLAYER 70-79, TUT 80-89 |
| NOT STARTED | GAME-c, R-CORE, STANDALONE, the tests (`DX11`, 15 files / 240, belongs to the dx11 seam step) |

`PathfinderThread.h` was moved from GAME-b to ACT-a (ACT-a splits its raw storage).

## 2. Resuming

1. **Sync:** `git log --oneline -5`; `git worktree list`; `git branch --list 'b4-W1-*'` with
   `git rev-list --count main..<branch>`; `git status` in each agent worktree. Look for a merge left
   in progress on main (`.git/MERGE_HEAD`), a held game-dir lock (`tools/game_dir_lock.sh status`),
   a stray `reimpl-x64.exe`.
2. **The kit** lives in `b4_session/`; the coordinator RUNS it from a scratchpad copy that holds
   `runs/` (the baseline route logs and the warning baselines cut at `b2a5434c`). A new session's
   scratchpad is empty: copy `b4_session/` there and re-cut the baseline FIRST, from a
   `make clean` rebuild of main: `gate.sh baseline 3`, then `gate.sh baseline 3 warnbaseline`.
   `mergegate.sh` in `b4_session/` builds the merged tree BEFORE the merge commit exists.
3. **A block that was running** (none at this pause; kept for the next one)**:** its branch and worktree survive. If it has a hand-in (a final
   message), merge it: `mergegate.sh <branch> <label> "<title>" ui tools`, plus `sa-stress` on
   gameplay blocks and `devlost.sh` on Video / Graphics / Model changes. If it has none, resume the
   agent by message, or start a fresh agent on the same branch with the block's brief
   (`W1_BRIEF.md` + the block's row of the file map) and tell it to read `git log main..HEAD` first.
4. **The blocks not started:** one agent each, prompt = BLOCK_BRIEF + B4_RULES + W1_BRIEF + the
   block's tsv rows + the scope of §0 + what finished blocks asked of that block's headers
   (GATE_TABLE, the hand-in lists). ACT-b after ACT-a (it needs the action records typed); GAME-b
   owns `ProfessionList.h`, `NameDatabase`, `TerrainGenerator`, the `TradeWork_impl_*` bodies in
   `geometry_serialize.cpp` whose receiver is not a TradeWork; STANDALONE moves the four game-logic
   files (`menudialogloopt_stubs`, `splash_loop_stubs`, `loader_loop_stubs`,
   `chunked_scene_loader`) to `code/Game/Game/` and converts or retires the harnesses.
5. **Limits:** at most FOUR agents, building at `-j2`; the gate at `-j4`; one game-running command
   at a time. Five agents at `-j3` locked the machine up on 2026-09-29.
6. **Never edit the working tree of main while a merge script runs.** Prepare docs in the
   scratchpad and commit them between merges.

## 3. After the first pass

1. **Second pass.** The first pass leaves a quarter to a half of each block's casts: reads waiting
   on a typing another block owned, which has since landed (every `GameUpdateLoop` member,
   `MapCell`, `ModelComponent::transform`, `StateMachine`, `StorageComponent::Item` …). The
   Component module shows the size: 3,270 left after all six blocks. Re-cut the file map on the
   metric, one agent per module, same rules. Rename `GameUpdateLoop::m_tutorial` (it holds a
   `WorkScheduler`) and `m_imageCapture` (the `Minimap`) here.
2. **Closing wave.** Rewire the callers of every `// B4 FORWARD` forwarder and delete the
   forwarders and their declarations (`acceptance.sh` lines 08 / 09: 619 C-body declarations, 854
   `void*`-first functions at `1d5b30a6`); delete the offset helpers left in headers
   (`widget_common.h`, `coll_bridge.h`, `description_common.h`) and the `Rt<Ns>::` POD mirrors (60).
3. **An independent read-only AUDIT** before the C-body headers are deleted (the teardown's audit
   found the one real defect of that step).
4. **Close:** `acceptance.sh` (12 lines), `stepclose.sh`, `sa-stress` on a QUIET machine (the p95
   figures recorded during W1 were taken with agents building and are not comparable),
   `CLOSE_NUMBERS.md`, the STATUS block, this file replaced by the plan for the dx11 seam.

## 4. What bit during W1 (bind the next session)

- **Parallel blocks that type shared fields collide at merge.** Both build alone; merged, the
  second one's callers name the first one's old fields. `mergegate.sh` stops before committing; fix
  inside the merge, or send the block back to merge main on its own branch. Merge shared headers
  EARLY (COMP-e's `-shared` branch) rather than at the end of a block.
- **Append-only conflicts** (`CitizenBehavior.h`, the ABI tests' `CHECK_EQ` runs): keep both sides;
  resolve test lines PER CLASS by owner. Match conflict separators as whole lines (`^=======$`): a
  substring search matched a comment banner once.
- **The casts fall only where the source is typed.** Block T could rewire 4 of ~500 find callers;
  the class blocks removed 75-93% once their headers and the entity pointer were typed.
- **Inventory tables mis-tag classes and body names mislead**: a block re-derives class, slot,
  offset and width from retail.
- **A route nobody ran finds a crash nobody caused**: `known_port_issues` 13 reproduced on the
  step's base. Compare against the base before blaming a merge.
- **Two merge commits on main do not build** (`f41a352a`, `18662af7`; each fixed by the next
  commit): `git bisect skip`. Several block branches hold one such commit inside them too.
- **The warning check compares text.** Naming a type changes a standing warning's wording
  (`uintptr_t [0]` -> `ConsumeProduceRecipe* [0]`) and the gate counts it as new; the routes and
  the stress run are then skipped. Read `warn_new_*.txt`; update the baseline file and keep it
  SORTED (`LC_ALL=C sort -u`). Log-only locals need `[[maybe_unused]]`: the perf build strips the
  log lines that read them.
- **`known_port_issues` 14 conflicts on every merge** (row numbers are global and two blocks take
  the same next number): keep both sides, renumber the incoming rows. Rows run to 50.
- **Asked of the blocks not started** (from the last hand-ins): `Work.h` slot parameters and the
  Persist stream are still `void*` (second pass); the Scenario Strings in GAME-b's header;
  `UserInterface::_dialogs`; `WorkSchedulerTree::root`; `PlayerController::camera`; ACT-b's ctor
  C declarations; `~NoiseUI` leaks now that its OnCreated runs (14c row 42).
- **A lockup corrupts files**: one header in an agent worktree was written as NUL bytes and one
  object file was unreadable. After a lockup: `git status` everywhere, `git fsck`, delete object
  directories when a link fails with "file format not recognized".
