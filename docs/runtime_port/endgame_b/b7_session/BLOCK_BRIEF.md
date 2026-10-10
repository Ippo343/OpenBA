# B7 block brief (read with `../B7_RULES.md`; the rules there win on any conflict)

You supply ONE named prerequisite (an inline release, a real constructor, a class member, a template)
and then type the sites that waited on it, one commit per retail body, each with the B11 verdict and
the retail evidence. The cast on the line is the symptom; the fix removes it. Your worktree is yours;
the coordinator merges and gates (the game is never run by a block).

## Setup (first thing, in this order)
1. From the repo root: `git worktree add .claude/worktrees/<branch> -b <branch> main`; `git log -1`
   there must show the NAMED base commit in your prompt (else `git reset --hard <commit>`).
2. Read `docs/runtime_port/endgame_b/B7_RULES.md` §1 (D1-D11), §2 (the recipe), §5 (working rules);
   then `b7_session/INVENTORY.md` for YOUR sites / rows; then each site's full tag line and each row's
   full text in `docs/known_port_issues.md`; then `CLAUDE.md`'s Hard rules ("Don't fix a faithful crash
   with a null check"; "No doors": nothing registers or resolves a body by retail name or address).
3. Ghidra MCP: ALWAYS pass `program="Application-x64.exe"` (exe, 0x140...) or `program="Runtime-x64.dll"`
   (0x180...) to decompile_function / disassemble_function; NEVER call switch_program (the instance is
   shared by four agents). Ghidra drops arguments on indirect calls and register-passthrough forwarders:
   the disassembly decides argument counts and which import slot is called.
4. B11 base: `docs/runtime_port/endgame_b/b7_session/runs/b11base/perf` (main's -O2 objects at your base
   commit) and `.../b11base/debug`. Your tip objects are your worktree's `build/perf/obj` /
   `build/debug/obj` after your build.

## Per site
1. Read the tree's site and its tag. Read retail at the quoted address: the decompile AND the
   instructions around the quoted range. For a SUSPECTED row, settle it now (B7_RULES D9): if retail
   does what the tree does, close the row as NOT A DIFFERENCE (a `B7:` doc commit, evidence quoted).
2. The smallest change that makes the tree's -O2 object show retail's shape (D3), with the rules of your
   unit (D4-D10). No new null guard. No helper retail does not have. No door kept beside a member
   (the door is deleted with its last caller). No `memcpy` for a vector store. No union / reference
   member to dodge a cast. Keep every retail address comment and quirk note; delete the
   `LEFT: DEFERRED (named prerequisite …)` / `audit DEFERRED (…)` tag with the cast.
3. Build in the foreground (600000 ms timeout, re-invoke if cut), `-j2`:
   `/c/msys64/ucrt64/bin/mingw32-make.exe -j2 standalone-debug > build_debug.log 2>&1; echo $?`,
   same for `standalone-perf` and `unit`. Check the EXIT CODE, not a pipe's.
   Warnings: `norm() { grep 'warning:' "$1" | sed -E 's/:[0-9]+:[0-9]+: warning:/: warning:/' | LC_ALL=C sort -u; }`;
   `norm build_debug.log > w.txt; tr -d '\r' < docs/runtime_port/endgame_b/b7_session/runs/warn_baseline_debug.txt > wb.txt; LC_ALL=C comm -23 w.txt wb.txt`
   must be empty (same for perf). Unit: `grep -E '[0-9]+ tests' build_unit.log | tail -1` must say 0 failures.
   `python docs/runtime_port/endgame_b/b4_session/castmetric.py code runtime standalone dx11 engine | tail -2`
   on your tree: the site's casts gone, nothing new.
   B11: `python docs/runtime_port/endgame_b/b4_session/b11cmp.py docs/runtime_port/endgame_b/b7_session/runs/b11base/perf build/perf/obj <tu-substring> --detail`
   for every touched translation unit; read every DIFF; quote the verdicts in the commit.
4. Commit (one prerequisite, or one retail body, per commit). Title `behaviour: <site> — <what>` when
   anything that runs changes (a store, a call, an order, a destruction point); `B7: <site> — <what>`
   for a typing whose B11 is IDENTICAL / SAME. Body = B7_RULES D2. End with the two attribution lines
   from your prompt.
5. `docs/known_port_issues.md`: a row that exists gets ` | FIXED (B7 <block>, <short hash>): <one line>`
   appended INSIDE its status cell (the last cell); NOT A DIFFERENCE likewise. Never rewrite another
   row; never reflow the tables (the merge keeps both sides by line). New findings: new rows at the end
   of the relevant §14 table in your number range (RT 1000-1019, CTOR 1020-1039, LX 1040-1059,
   RES 1060-1079).

## Never
- Run the game. Add a null / IsA / validity test retail lacks. "Fix" retail (a retail quirk is copied,
  with its comment). Touch a file another block owns (your prompt lists yours). `git stash`, amend,
  rebase, push, sub-agents, `taskkill /IM`. Edit a file with backslashes through a heredoc (Write /
  Edit tool). `find /` or any unscoped search. `switch_program` in Ghidra.

## Hand-in (your final report, short)
Branch; the commit list (site / row -> hash, one line each: the retail address, the B11 verdict, the
observable); castmetric total before / after on your tree; the static-gate numbers; sites NOT fixed and
why (a measurement that failed, a dependency, a blocker you found) with the re-tag text you left on
the line; any NEW difference found (its row number); anything the coordinator must know before
merging (a file touched outside your list, a row another block also edits).
