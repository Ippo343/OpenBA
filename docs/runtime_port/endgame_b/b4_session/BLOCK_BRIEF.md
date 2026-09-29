# Block brief — B.4, the cast metric (W0 / W1 / W2 owners)

Read, in this order: `../B4_RULES.md` (all of it), then the inventories that cover your files and
their tables in `data/` (a table is a map, not the authority: re-derive every class, slot, offset,
width and signedness from retail before you edit).

| you own | read |
|---|---|
| bodies in `code/Game` | `INVENTORY_A_EXE_BODIES.md`, `data/A_exe_bodies.tsv` |
| bodies in `runtime`, `standalone`, `dx11` | `INVENTORY_B_RUNTIME_BODIES.md`, `data/B_runtime_bodies.tsv`, `data/B_runtime_exports.tsv` |
| any offset read | `INVENTORY_C_OFFSETS.md`, `data/C_offsets.tsv` (filter on your files / classes) |
| GameUpdateLoop, CitizenComponent, settings, container | `INVENTORY_D_BIG_OBJECTS.md`, `data/D_retail_layouts.tsv` |
| a cast that must stay | `INVENTORY_E_FLOOR_AND_TESTS.md`, `data/E_floor_and_tests.tsv` |
| the numbers you hand in | `INVENTORY_F_ALL_SPELLINGS.md`, `castmetric.py` |

## First
You work in your own git worktree. `git log --oneline -1` must show the hash the coordinator gave
you. If it does not: `git fetch . 2>/dev/null; git reset --hard main`. Then
`git checkout -b <your branch>`. Record
`python docs/runtime_port/endgame_b/b4_session/castmetric.py <your files> --by-file --tsv > <scratch>/metric_base.tsv`.

## Rules
- Edit ONLY your block's files. A field you need in a header another block owns: ask in your
  hand-in, LEAVE the read, do not edit the header (H9).
- No behaviour change outside a commit whose title starts `behaviour:` and names the retail address
  that proves it. If a conversion would change behaviour, or a class / signature / width cannot be
  settled from retail, LEAVE the site and list it.
- Retail wins. Never "fix" a faithful crash with a null check. Never `getenv` (flags are
  `Opt().field`). No new flag, instrument, cache, env gate or helper. No `Register*()` inside a
  `printf`. Nothing reads a vtable by literal index. No door: nothing registers or resolves a body
  by retail name or address.
- **A cast is removed by naming the thing, never by respelling it** (M5): no C-style cast,
  `static_cast` through `void*` / `uintptr_t`, `memcpy`, union or offset helper in its place.
- Type headers: a new or retyped field is `static_assert(offsetof)`ed; `sizeof` stays asserted.
- Build in YOUR worktree, in the FOREGROUND (Bash timeout 600000 ms; re-invoke if it times out;
  never a background waiter):
  `/c/msys64/ucrt64/bin/mingw32-make.exe -j3 standalone-debug > build_debug.log 2>&1; echo $?`
  then `-j3 standalone-perf`, then `-j3 unit`. All exit 0, unit 0 failures, no new warning in a file
  you touched. Check the build's exit code, not a pipe's. Never higher than `-j3`. If the shell is
  killed for low memory, say so in the hand-in; do not restart in a loop.
- NEVER run the game, `sa-run`, `sa-deploy`, or touch `C:/GOG Games/Banished`. The coordinator gates.
- Never `find /` or any unscoped search; never `git stash`, amend, rebase, push; never `pacman`;
  no sub-agents.
- Ghidra (retail): `check_connection` first; `program=` on EVERY call (`Application-x64.exe` or
  `Runtime-x64.dll`), never `switch_program`; read-only — rename nothing, save nothing. Ghidra's C
  drops arguments on indirect calls and register-passthrough forwarders: for a signature, read the
  disassembly.
- Scratch files and helper scripts: `<scratchpad>/b4/<your block>/` only. Scripts take the worktree
  path explicitly. Before each commit: `git diff --stat` and revert anything outside your file set.
- Small titled commits `B4 <block>: …`, ending with
  `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`. Every commit builds. A file deletion is
  staged in the commit that removes its last user. Do not commit build logs.
- Before the final build: `git merge main` if main moved; keep both sides of an append-only
  conflict.

## Finishing
- `castmetric.py <your files> --by-file --tsv` again; diff against your base: the total fell, NO
  column rose in any file (M1).
- B11 (W1 blocks): compare your translation units' debug objects function by function with main's
  (`objdump -d --no-show-raw-insn`, symbol names and addresses normalised); list every function that
  differs with its reason.

## Hand-in (your final message, under 60 lines)
- branch, last commit; debug / perf / unit results with exact numbers
- the metric: base total → tip total for your files, per column
- headers: fields declared / retyped / renamed, asserts added; bodies: made members, duplicates
  deleted, unreferenced deleted, callers rewired
- what is LEFT: file:line, the reason (class not settled, header owned by another block, H5
  non-trivial member, unset register, split body)
- fields you need in headers you do not own
- `behaviour:` commits, each with the retail address that proves it
- anything in the inventories or the rule sheet that the code or retail contradicts
