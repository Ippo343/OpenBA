# B9 block brief (read with `../B9_RULES.md`; the rules there win on any conflict)

You settle the rows and survivors of YOUR block (`B9_RULES.md` §3), one commit per row (or per family with
one ruling), each `behaviour:` (retail call sites quoted, the dropped test / added call named, the expected
B11 DIFF quoted) or `B9:` (B11 IDENTICAL / SAME / RENAMED-IDENTICAL per touched TU, verdicts quoted). Your
worktree is yours; the coordinator merges and gates (the game is never run by a block).

## Setup (first thing, in this order)
1. From the repo root: `git worktree add .claude/worktrees/<branch> -b <branch> main`; `git log -1` there
   must show the NAMED base commit in your prompt (else `git reset --hard <commit>`).
2. Read `docs/runtime_port/endgame_b/B9_RULES.md` §0-§5; `docs/known_port_issues.md` for every row number
   in your prompt (grep `^| <n> |`); `docs/runtime_port/endgame_b/b4_session/LEFT_FREE.md` §A for every
   survivor name in your prompt (its line carries the B8 measurement you are re-reading); `CLAUDE.md`'s Hard
   rules ("Don't fix a faithful crash with a null check"; "No doors"; never `getenv`).
3. Ghidra MCP: ALWAYS pass `program="Application-x64.exe"` (exe, 0x140...) or `program="Runtime-x64.dll"`
   (0x180...) to decompile_function / disassemble_function / get_function_callers; NEVER call switch_program
   (the instance is shared by three agents). Ghidra drops arguments on indirect calls and register-passthrough
   forwarders: the disassembly decides argument counts. The exe is stripped: Ghidra's name is never a retail
   symbol; the class is what RCX is.
4. B11 base: `docs/runtime_port/endgame_b/b9_session/runs/b11base/perf` (main's -O2 objects at your base
   commit) and `.../b11base/debug`. Your tip objects are your worktree's `standalone/build-perf/obj` /
   `standalone/build/obj` after your build.

## Per row / survivor
1. Read the row or the LEFT_FREE line, the body, the retail body at the address AND every retail caller
   (what RCX is; how the argument is built; the parameter count from the disassembly).
2. Apply the rule (`B9_RULES.md` D1-D6). A test retail lacks is dropped, or kept AT THE CALLER whose route
   hands the value (named in the comment and in the row's status) -- never left where it is with a vaguer
   comment, never `if (obj) obj->F()` written at a caller. A second port goes; its caller goes on the one
   member. A survivor is moved, deleted, re-tagged with a NEW measurement, or ruled free / harness with the
   RCX evidence -- its old tag is never copied forward unread.
3. Build in the foreground (600000 ms timeout, re-invoke if cut), `-j2`:
   `/c/msys64/ucrt64/bin/mingw32-make.exe -j2 standalone-debug > build_debug.log 2>&1; echo $?`,
   same for `standalone-perf` and `unit`. Check the EXIT CODE, not a pipe's.
   Warnings: `norm() { grep 'warning:' "$1" | sed -E 's/:[0-9]+:[0-9]+: warning:/: warning:/' | LC_ALL=C sort -u; }`;
   `norm build_debug.log > w.txt; tr -d '\r' < docs/runtime_port/endgame_b/b9_session/runs/warn_baseline_debug.txt > wb.txt; LC_ALL=C comm -23 w.txt wb.txt`
   must be empty (same for perf). Unit: `grep -E '[0-9]+ tests' build_unit.log | tail -1` must say 0 failures.
   `python docs/runtime_port/endgame_b/b4_session/castmetric.py code runtime standalone dx11 engine | tail -2`
   on your tree: 3 (RT: 2 after D4), nothing new.
   B11: `python docs/runtime_port/endgame_b/b4_session/b11cmp.py docs/runtime_port/endgame_b/b9_session/runs/b11base/perf standalone/build-perf/obj <tu-substring> --detail`
   (or `b9_session/b11pair.py BASE TIP MAPFILE <tu-substring> --detail` with a rename map `old -> Class::Member`
   when a body changed its symbol) for the definer's TU and every caller's TU; read every DIFF; quote the
   verdicts in the commit. A `behaviour:` commit's DIFF is the dropped compare / removed call, quoted.
4. Commit (one row per commit). Title `behaviour: row <n> -- <what>` / `B9: <what>`; body = the retail
   instruction range, every caller's retail call site, the B11 verdict per TU, what was RUN ("static gate only:
   debug / perf / unit builds exit 0, 0 new warnings, unit 920 / 0, castmetric 3"), what is left and why. Update
   the row's STATUS cell in `docs/known_port_issues.md` (append to the cell: `FIXED (B9 <block>, <hash>)` /
   `NOT A DIFFERENCE (B9 <block>): <instruction quoted>` / `LEFT (B9 <block>): <owing row>`) and the
   survivor's tag on its declaration line (`// B9: <ruling> -- <measurement>`); never rewrite another row or
   reflow a table. End the commit message with the two attribution lines from your prompt.
5. A retail difference you FIND outside your rows: a new `docs/known_port_issues.md` row at the end of the
   relevant §14 table in YOUR number range (`B9_RULES.md` D8), the body left as it is.

## Hand-in
`HANDIN.md` at your worktree root (committed): per row / survivor one line -- the ruling, the commit, the B11
verdict, what you could not settle and the exact measurement. The coordinator copies it to
`b9_session/handins/<block>.md`. Merge `main` into your branch before the final build (`git merge main`),
rebuild, re-check, then report your branch name, tip hash, and the HANDIN path.

## Never
- Run the game (`sa-run`, `sa-deploy`, `sa-stress`, the resave gate, any route script); touch
  `C:/GOG Games/Banished`. Add a null / IsA / range / validity test retail lacks. Move a body between files.
  Add a helper, overload, default argument, flag or instrument retail lacks. Touch the four user-decision rows
  (1162, 1202, 1203, 1204: `wg_f390_smooth`'s fill, `DecalComponent_Serialize`, `MapComponent_Draw`,
  `RawMaterialComponent_Update`). Touch a file another block owns (your prompt lists yours; a CALLER line in
  another block's file is yours to edit -- the call only). Edit LEFT_FREE.md (the coordinator regenerates it).
  `git stash`, amend, rebase, push, sub-agents, `taskkill /IM`, `switch_program`. Edit a file with backslashes
  through a heredoc (Write / Edit tool). `find /` or any unscoped search. `getenv`.
