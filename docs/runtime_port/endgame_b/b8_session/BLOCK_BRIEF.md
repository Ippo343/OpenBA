# B8 block brief (read with `../B8_RULES.md`; the rules there win on any conflict)

You move the free functions of YOUR classes into those classes (`B8_RULES.md` D1-D7), one commit per
class, each with the B11 verdict per touched translation unit. No behaviour changes in your block
(RES excepted: its own brief is in its prompt). Your worktree is yours; the coordinator merges and
gates (the game is never run by a block).

## Setup (first thing, in this order)
1. From the repo root: `git worktree add .claude/worktrees/<branch> -b <branch> main`; `git log -1`
   there must show the NAMED base commit in your prompt (else `git reset --hard <commit>`).
2. Read `docs/runtime_port/endgame_b/B8_RULES.md` §1 (D1-D11), §2 (the recipe), §5 (working rules);
   then `b8_session/INVENTORY.md`'s section for YOUR block (every row: name, where declared, retail
   address, Ghidra's name, first parameter, target class, kind, callers); then `CLAUDE.md`'s Hard
   rules ("Don't fix a faithful crash with a null check"; "No doors").
3. Ghidra MCP: ALWAYS pass `program="Application-x64.exe"` (exe, 0x140...) or `program="Runtime-x64.dll"`
   (0x180...) to decompile_function / disassemble_function; NEVER call switch_program (the instance is
   shared by four agents). Ghidra drops arguments on indirect calls and register-passthrough forwarders:
   the disassembly decides argument counts. The exe is stripped: Ghidra's name is never a retail symbol;
   the class is what RCX is.
4. B11 base: `docs/runtime_port/endgame_b/b8_session/runs/b11base/perf` (main's -O2 objects at your base
   commit) and `.../b11base/debug`. Your tip objects are your worktree's `build/perf/obj` /
   `build/debug/obj` after your build.

## Per class
1. Read every row of the class (INVENTORY.md), the declaration's comment, the body, the retail body at
   the address (what RCX is; the parameter count from the disassembly). A row whose kind is
   `forwarder` already has its member: put every caller on the member and delete the forwarder. A
   row whose kind is `erased` becomes the class template's member (D5 / D6). A `free` / `harness` row
   (MISC) is confirmed or moved (D8).
2. Declare the member in the class's own header (retail address as the trailing comment), rewrite the
   definition in place (`self->x` -> `x`, the first parameter gone, D2), put every caller on the
   member by name (D3), delete the declaration line(s) only (D4).
3. Build in the foreground (600000 ms timeout, re-invoke if cut), `-j2`:
   `/c/msys64/ucrt64/bin/mingw32-make.exe -j2 standalone-debug > build_debug.log 2>&1; echo $?`,
   same for `standalone-perf` and `unit`. Check the EXIT CODE, not a pipe's.
   Warnings: `norm() { grep 'warning:' "$1" | sed -E 's/:[0-9]+:[0-9]+: warning:/: warning:/' | LC_ALL=C sort -u; }`;
   `norm build_debug.log > w.txt; tr -d '\r' < docs/runtime_port/endgame_b/b8_session/runs/warn_baseline_debug.txt > wb.txt; LC_ALL=C comm -23 w.txt wb.txt`
   must be empty (same for perf). Unit: `grep -E '[0-9]+ tests' build_unit.log | tail -1` must say 0 failures.
   `python docs/runtime_port/endgame_b/b4_session/castmetric.py code runtime standalone dx11 engine | tail -2`
   on your tree: 3, nothing new.
   B11: `python docs/runtime_port/endgame_b/b4_session/b11cmp.py docs/runtime_port/endgame_b/b8_session/runs/b11base/perf build/perf/obj <tu-substring> --detail`
   for the definer's TU and every caller's TU; read every DIFF; quote the verdicts in the commit.
   Expected: RENAMED-SAME (the moved body), IDENTICAL / SAME (callers). A DIFF = the row stays free
   (D1 / D7), re-tagged on its line.
4. Commit (one class per commit; a big class may be split by defining file). Title
   `B8: <Class> -- N members moved (<names>)`; body = the rows (name -> member, address), the B11
   verdict per TU, what was RUN ("static gate only: debug / perf / unit builds exit 0, 0 new warnings,
   unit 920 / 0, castmetric 3"), rows left free and why. End with the two attribution lines from your
   prompt.
5. A retail difference you FIND (a guard retail lacks, a wrong constant, a dropped call): a new
   `docs/known_port_issues.md` row at the end of the relevant §14 table in your number range
   (D10), the body moved AS IS. Never rewrite another row; never reflow the tables.

## Never
- Run the game. Change what runs (a store, a call, an order, a guard) — that is `behaviour:` and not
  yours. Add a null / IsA / validity test retail lacks. Move a body between files. Add a helper,
  overload, default argument or `static` retail lacks. Touch a class another block owns (your prompt
  lists yours; a CALLER line in another block's file is yours to edit — the call only). Edit
  LEFT_FREE.md (the coordinator regenerates it). `git stash`, amend, rebase, push, sub-agents,
  `taskkill /IM`. Edit a file with backslashes through a heredoc (Write / Edit tool). `find /` or any
  unscoped search. `switch_program` in Ghidra.

## Hand-in (your final report, short)
Branch; the commit list (class -> hash, one line each: rows moved, the B11 verdicts, anything
ONLY-BASE / ONLY-TIP paired by hand); the static-gate numbers; rows NOT moved and why (the verdict
that failed, the function-pointer site, a dependency on another block) with the re-tag text you
left on the line; any NEW difference found (its row number); anything the coordinator must know
before merging (a caller line edited in another block's file, a header both blocks touch).
