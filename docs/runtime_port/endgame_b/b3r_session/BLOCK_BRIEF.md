# Block brief — the registry teardown (W0 / W1 / W2 owners)

Read, in this order: `../B3R_RULES.md` (all of it), then the inventory that covers your files and its
table in `data/` (the table is a map, not the authority: re-derive every binding from the code before
you edit, and sweep your files for readers the inventory missed).

| your files are in | read |
|---|---|
| `code/Game`, `standalone` | `INVENTORY_B_EXE_ROWS.md`, `INVENTORY_E_ENGINE_FUNCS.md`, then A for import slots |
| `runtime`, `dx11` | `INVENTORY_A_RUNTIME_ROWS.md`, `INVENTORY_C_BY_ADDRESS_DATA.md` |
| the allocator, the mint | `INVENTORY_D_STORAGE_ALLOCATOR.md` |
| anything that prints or is parsed | `INVENTORY_F_BOOTLOG_GATE_TOOLS.md` |

## First
`git log --oneline -1` must show main's tip (the coordinator tells you the hash). If it does not:
`git reset --hard main`. Then `git checkout -b <your branch>`.

## Rules
- Edit ONLY your block's files (B3R_RULES §4). Work ONLY in your worktree. The one exception is
  appending a missing declaration to a `code/Game/<Module>/exe_bodies.h` or a `runtime/funcs` header
  (R5); list every such line in your hand-in.
- Rows other blocks may still read STAY. In W1 you delete a registration line only when the inventory
  table says DEAD and your own code-line sweep of the WHOLE tree (not only your files) agrees.
- No behaviour change outside a commit titled `behaviour:`. If a conversion would change behaviour,
  or an argument list / receiver cannot be settled from retail, LEAVE the site and list it.
- Retail wins. Never "fix" a faithful crash with a null check. Never `getenv` (flags are
  `Opt().field`). No new flag, instrument, cache or env gate. No `Register*()` inside a `printf`.
  Nothing reads a vtable by literal index. Generated files are regenerated, never hand-edited.
- Build in YOUR worktree, in the FOREGROUND (Bash timeout 600000 ms; re-invoke if it times out; never
  a background waiter): `/c/msys64/ucrt64/bin/mingw32-make.exe -j3 standalone-debug > build_debug.log 2>&1; echo $?`
  then `-j3 standalone-perf`, then `-j3 unit`. All exit 0, unit 0 failures, no new warning in a file
  you touched. Check the build's exit code, not a pipe's. Never higher than -j3.
- NEVER run the game, `sa-run`, `sa-deploy`, or touch `C:/GOG Games/Banished`. The coordinator gates.
- Never `find /`; never `git stash`, amend, rebase, push; never `pacman`; no sub-agents.
- Ghidra (when you need retail): `check_connection` first; `program=` on every call, never
  `switch_program`; read-only — rename nothing.
- Scratch files and helper scripts: `<scratchpad>/b3r/<your block>/` only. Scripts take the worktree
  path explicitly. Before each commit: `git diff --stat` and revert anything outside your file set.
- Small titled commits `B3R <block>: …`, ending with
  `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`. Do not commit build logs.
- Before the final build: `git merge main` if main moved; keep both sides of an append-only conflict.

## Finishing
Finish with a line-by-line re-sweep of your files with `python docs/runtime_port/endgame_b/b3r_session/codegrep.py`
for every door spelling: `ResolveAddr\s*\(`, `ResolveExport\s*\(`, `RT_EXPORT_ONCE`, `RtAddr\s*<`,
`RtResolveCached`, `AtomicExport|AtomicAddr`, `RT_DATA_SLOT`, `RtPortLookup`, `\bfn_\w+`, `\bg_p\w+`,
the file-local helpers (`RSlot`, `ResolveSlot\w*`, `ResolveFn`, `loadExref`, `resolveIAT`, `SlotFn`,
`ByAddr`), `_ResolveFuncs|_InitFuncs|Ensure\w+`, `Register(Exe|Runtime)\w*\s*\(`. Every hit that
remains is in your hand-in with its reason.

## Hand-in (your final message, under 60 lines)
- branch, last commit; debug / perf / unit results with exact numbers
- readers converted, by door spelling; guards deleted; DEAD lines deleted; helper / resolver
  definitions deleted
- what is LEFT: file:line, the key, the reason (STUB with its PORT GAP marker, a signature that
  could not be settled, a row another block reads)
- declarations you added outside your file set
- `behaviour:` commits, each with the retail address that proves it
- `NOT IMPLEMENTED` addresses whose reader you removed, with disposition (a) / (b) / (c) of R10
- register functions that are now empty (the coordinator removes their calls)
- anything in the inventories or the rule sheet that the code contradicts
