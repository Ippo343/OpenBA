# B.3.9 caller-batch brief (C1-C5: the type-id cells become `X::TypeId()`)

You convert every TYPE-ID CELL READ in your batch's files to the class's static `X::TypeId()`, on your own
worktree branch. The coordinator merges and gates. Your prompt names the batch and the branch; your FILE SET
is your batch's section of `B39_FILEMAP.md` §1 (C1 `code/Game/Component` a-h, C2 Component i-z, C3 `Work`,
C4 `Action` + `Goal` + `Entity`, C5 `Dialog` + `Player` + `Tutorial` + `Game` — minus the EXE / RF files).
You retire NO mint row and touch NO Runtime file.

## Read first
1. `CLAUDE.md` (no null-check "fixes", never `getenv`, no unscoped searches — exclude `standalone/build*/`,
   `tests/unit/build/`, `.claude/worktrees/` — no `pacman`).
2. `B39_RULES.md` D5 (the rule), D6 (the abstract bases wait for RF), §3 B9-8, B9-14, B9-15.
3. `b39_session/INVENTORY_C_TYPEID_REFLECTION.md`: **Table 1** (cell → the CLASS at the cell — the only
   authority), Table 2 (your sites, by file), Table 3 (the `g_TypeId_*` globals: cell, class, readers — 23 names
   LIE), Table 4 (`g_tid_*`), the Notes on Table 1 (unregistered cells, `+8` reads).

## The recipe
1. For each reader in your files — `ResolveAddr(Module::Exe, 0x1401c…)`, a per-file wrapper (`R<>`, `RExe`,
   `Tid`, `DerefTid`, `ResolveTid`, `ResolveTypeId`, `ResolveExeTypeId`, `PopulationTypeId`, `rd`,
   `MW_TypeId`, `SerializeValidatedIBP`), a `g_tid_X`, a `*g_TypeId_X` — find the CELL (the address, or the
   global's initialiser in `engine/funcs/engine_funcs.cpp`), look up Table 1's CLASS, write `Class::TypeId()`.
   **BY CELL, never by the variable's name** (`*g_TypeId_AreaDescription` is `MapDescription::TypeId()`).
   Include the class header; qualify.
2. A per-file wrapper / cache that becomes unused goes with its last use (no dead helper left behind; a
   `static` one would warn).
3. **Behaviour sites, each its OWN commit titled `ENDGAME B.3.9 <batch>: … (behaviour: …)`**: a read of one
   of the 13 UNREGISTERED cells (reads 0 today; e.g. `CitizenAIComponent` 0x1401c70e8, `HappinessComponent`
   0x1401c42c0) and the 8 `+8` NAME reads (0x1401c5f38/5f70/5fe0/6088/60c0, 0x1401c72e8). First read the retail
   caller in Ghidra (`get_xrefs_to <cell>`, `disassemble_function`, `program="Application-x64.exe"`), then
   transcribe what retail does — a `+8` read is `X::TypeName()` / the record's name, not an id. If retail
   proves the tree's current behaviour, leave it and say so.
4. The 7 ABSTRACT-base cells (`Action` 0x1401c5da8, `Work` 0x1401c6cf8, `Tool` 0x1401c6eb8, `ToolSetting`
   0x1401c6ef0, `Component` 0x1401c72a8, `Description` 0x1401c72e0, `ControllerComponent` 0x1401c73c0): only if
   RF's `DECLARE_TYPE` on the abstract bases is on main (your prompt says); otherwise leave them and list them.
5. Do NOT delete registrations, `g_tid_*` storage, `Fill*` lines or `g_TypeId_*` globals — they are CLOSE's.
   Hand in `TYPEID-DEAD <cell> <class> last reader removed in <commit>` for every cell whose last reader in the
   WHOLE tree you removed (grep the whole tree for the address, the `g_tid_` and the `g_TypeId_` names first).
6. Runtime-class ids read through an exe IAT `?_typeInfo@` slot or `ResolveTypeIdByNameLazy("XElement")` are the
   Runtime fold's (D6 step 2): convert them to `UI::XElement::TypeId()` only if your prompt says the accessor
   is ported; else list them.

## Process rules
- `git checkout -b <branch>`, `git merge main`, confirm the launch commit. Only your files. Commit per
  directory slice (≤ ~40 sites per commit) via `git commit -F <file>`, title `ENDGAME B.3.9 <batch>: …`,
  ending with the `Co-Authored-By:` line your prompt gives.
- FORBIDDEN: `git stash`, amend, rebase, push; sub-agents that edit; editing a file outside your set; running
  the game; hand-editing a generated `.inc`; `getenv`; null-check "fixes"; a `Register*()` in a `printf`.
- STATIC GATE before every commit (foreground, 600000 ms, exit code read from the command):
  `/c/msys64/ucrt64/bin/mingw32-make.exe -j10 standalone-debug`, `standalone-perf`, `unit` (0 failures);
  `tools/ghidra_mcp/.venv/Scripts/python.exe tools/dispatch_slot_map.py --check` (0 mismatches);
  `… tools/cutover_manifest.py --gate`; `… tools/rt_mint_gen.py --check` (then
  `git checkout -- standalone/rt_export_addrs.inc` if only line endings changed); no new warnings in touched
  files; `git status --short` only your files. Scripts / logs in
  `C:/Users/USPC/AppData/Local/Temp/claude/<your-branch>/`.

## Report
Branch + commits (mark behaviour ones); sites converted per file; the cells whose readers you did NOT convert
and why; every Table 1 / Table 3 fact you found wrong (with the retail evidence); static gate verbatim; the
hand-in blocks `TYPEID-DEAD`, `BEHAVIOUR`, `BOOTLINE` (usually none).
