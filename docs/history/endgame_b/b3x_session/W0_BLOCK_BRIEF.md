# W0 block brief — "by name first" (normal-virtual cutover)

Read, in this order: `../B3X_RULES.md` (all of it; X1 and X3 are your rules), then the inventory that
covers your files: `INVENTORY_B1_SLOTREADS_RUNTIME.md` (runtime / standalone / engine / dx11; §7 is the
per-site table, §4 and §6 the helpers and the shapes `partc.py` misses) or
`INVENTORY_B2_SLOTREADS_GAME.md` (code/Game; read "Obstacles" first).

## What you do
For every row of the inventory in YOUR files whose class FLIPS and whose KIND is DELETE, DESTRUCT,
MEMBER, DIAGNOSTIC or TABLE-BUILDER: convert it per B3X_RULES X3. The inventory is a map, not the
authority: re-derive the class and the slot from the header before each edit, and sweep your files
for rows the inventory missed (same shapes). Leave EXEMPT-TABLE rows, COM calls, the dx11 seam and
every `Video::*Data` dispatch exactly as they are.

The classes are still on `DeletingDtor` when you finish: you convert READS, not declarations.
No behaviour change. If a conversion would change behaviour, or an argument list / receiver class
cannot be settled from retail, leave the site and list it.

Slot constants, slot typedefs and helper functions that lose their last user in your files are
deleted; one still used outside your files stays.

## Rules
- Edit ONLY your block's files (B3X_RULES §4). Work ONLY in your worktree.
- First: `git merge main` is not needed at start (you were cut from main); before your final build run
  `git fetch . && git merge main` if main moved, and resolve.
- Build: `/c/msys64/ucrt64/bin/mingw32-make.exe -j3 standalone-debug` and `-j3 unit` in your worktree;
  both exit 0, unit 0 failures, no new warnings in files you touched. Never higher than -j3.
- NEVER run the game, never `sa-run` / `sa-deploy`, never touch `C:/GOG Games/Banished`.
- Never `find /`; never `git stash`; never `pacman`; never `getenv`; no null guard on a faithful deref.
- Ghidra (if you need retail): call `check_connection` first; pass `program=` on every call, never
  `switch_program`; read-only — rename nothing.
- Scratch files: `<scratchpad>/b3x/<your block>/` only.
- Commit on your branch in small titled commits (`B3X W0 <block>: …`), ending with
  `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`.

## Hand-in (your final message, under 60 lines)
- branch name and last commit; build + unit result (exact numbers)
- rows converted by KIND; rows LEFT with file:line and the reason
- headers whose declarations you corrected, with the retail address that proves each
- helpers you kept because another block uses them
- any regex `partc.py` should gain for a shape you met
