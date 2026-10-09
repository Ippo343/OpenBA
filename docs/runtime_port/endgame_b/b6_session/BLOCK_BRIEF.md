# B6 block brief (read with `../B6_RULES.md`; the rules there win on any conflict)

You fix a list of `docs/known_port_issues.md` rows — places where the tree does something retail does
not — one `behaviour:` commit per row, each carrying the retail evidence. The cast on the line is the
symptom; the fix removes it. Your worktree is yours; the coordinator merges and gates (the game is never
run by a block).

## Setup (first thing, in this order)
1. From the repo root: `git worktree add .claude/worktrees/<branch> -b <branch> main`; `git log -1`
   there must show the NAMED base commit in your prompt (else `git reset --hard <commit>`).
2. Read `docs/runtime_port/endgame_b/B6_RULES.md` §1 (D1-D12), §2 (the recipe), §5 (working rules); then
   `b6_session/INVENTORY.md` §A / §B for YOUR rows (the retail address, the tree's defect, the verdict,
   the files); then each row's full text in `known_port_issues.md`; then `CLAUDE.md`'s Hard rules
   ("Don't fix a faithful crash with a null check").
3. Ghidra MCP: ALWAYS pass `program="Application-x64.exe"` (exe, 0x140...) or `program="Runtime-x64.dll"`
   (0x180...) to decompile_function / disassemble_function; NEVER call switch_program (the instance is
   shared by four agents). Ghidra drops arguments on indirect calls and register-passthrough forwarders:
   the disassembly decides argument counts and which import slot is called.

## Per row
1. Read the tree's site and its comments. Read retail at the row's address: the decompile AND the
   instructions around the quoted range. If the row is SUSPECTED, settle it now (D6): if retail does what
   the tree does, close the row as NOT A DIFFERENCE (a `B6:` doc commit, evidence quoted) and move on.
2. The smallest change that makes the tree do what the instructions do:
   - wrong callee / finder / overload -> the one retail calls, by name (`FindComponent<X>()`, the typed
     member, the import slot's overload);
   - wrong object (D7) -> the component retail reads, through its class header, after the type cell is
     settled by its `RegisterType`;
   - a stack record retail has (a {root, count} pair, a default-constructed EffectSlot, a `System::Time`
     local) -> a typed local of that class;
   - a guard retail lacks (D4) -> gone, unless the site's comment states a STANDALONE-SPECIFIC reason or
     the guard covers a PORT GAP (then it stays and your hand-in says so);
   - a store retail does not do -> gone; a store retail does -> added, at its instruction;
   - construction / destruction (D8) -> the member typed at its offset, ctor stores and dtor releases in
     retail's order, the pinning unit test rewritten in the same commit.
   No new null guard. No helper retail does not have. No change outside the row. Keep every retail address
   comment and quirk note; delete the `LEFT` / `DEFERRED (retail-difference ...)` tag with the cast.
3. Build in the foreground (600000 ms timeout, re-invoke if cut), `-j2`:
   `/c/msys64/ucrt64/bin/mingw32-make.exe -j2 standalone-debug > build_debug.log 2>&1; echo $?`,
   same for `standalone-perf` and `unit`. Check the EXIT CODE, not a pipe's.
   Warnings: `norm() { grep 'warning:' "$1" | sed -E 's/:[0-9]+:[0-9]+: warning:/: warning:/' | LC_ALL=C sort -u; }`;
   `norm build_debug.log > w.txt; LC_ALL=C comm -23 w.txt docs/runtime_port/endgame_b/b6_session/runs/warn_baseline_debug.txt`
   must be empty (same for perf). Unit: `grep -E '[0-9]+ tests' build_unit.log | tail -1` must say 0 failures.
   `python docs/runtime_port/endgame_b/b4_session/castmetric.py code runtime standalone dx11 engine | tail -2`
   on your tree: the row's casts gone, nothing new.
4. Commit (one row per commit; D1 names the rows that are one commit). Title `behaviour: row N — <what>`.
   Body = B6_RULES D2: row, files, retail address + instruction range + what they do, BEFORE, AFTER and
   the observable, the casts removed with the metric delta, "static gate only: debug / perf / unit
   builds exit 0, 0 new warnings, unit N / 0". End with the two attribution lines from your prompt.
5. `known_port_issues.md`: append ` | FIXED (B6 <block>, <short hash>): <one line>` INSIDE the row's
   status cell (the last cell of the table row; for a 14d bullet, append the sentence to the bullet).
   Never rewrite another row; never reflow the tables (the merge keeps both sides by line).

## Never
- Run the game. Add a null / IsA / validity test retail lacks. "Fix" retail (a retail quirk is copied,
   with its comment). Touch a file another block owns (your prompt lists yours; a guard line in another
   block's file is block D's only exception, named in the hand-in). `git stash`, amend, rebase, push,
   sub-agents, `taskkill /IM`. Edit a file with backslashes through a heredoc (Write / Edit tool).
   `find /` or any unscoped search.

## Hand-in (your final report, short)
Branch; the commit list (row -> hash, one line each: the retail address and the observable); castmetric
total before / after on your tree; the static-gate numbers; rows NOT fixed and why (RE-READ failed,
dependency, a blocker you found) with what you learned appended to the row as a dated paragraph; any
NEW difference found on the way (append it as a new row at the end of the relevant §14 table, numbered
from 900 upward in your block's range: A-WORK 900-919, A-UI 920-939, C 940-959, D 960-979, TRADE 980-989).
