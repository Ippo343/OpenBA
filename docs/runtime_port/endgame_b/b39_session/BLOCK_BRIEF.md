# B.3.9 block brief (every step-9 retirement block runs under this)

You RETIRE the migration scaffolding of a named set of classes — Runtime classes (blocks OBJ, SYS, IO, VID,
GFX, UI-A, UI-B, UI-D, ENG) or the exe-side construction / reflection scaffolding (EXE, RF) — on your own
worktree branch. The coordinator merges and gates. Your prompt names your block, your WAVE (W1 / W2 / W3), the
rows of that wave and your branch; your FILE SET is your block's section of `B39_FILEMAP.md` §1. This brief is
everything else. Where it and an older brief differ, this one wins.

## Read first, in this order
1. `CLAUDE.md` (hard rules: no null-check "fixes" of faithful crashes, never `getenv`, no new `BANISHED_*`
   flag, never an unscoped filesystem search — exclude `standalone/build*/`, `tests/unit/build/`,
   `.claude/worktrees/` — no `pacman`).
2. `docs/runtime_port/endgame_b/B39_RULES.md` — the WHOLE sheet. §1 is your row list (owner = your block,
   wave = your launch); D1-D10 and B9-1..16 bind you. B38 D1-D10 / B8-1..14, B37 §8/10/14, B36 B6-1..11,
   B35 B5-11/12/17 still hold.
3. `docs/runtime_port/endgame_b/B39_FILEMAP.md` — your files (§1), the shared files (§2: coordinator-owned,
   DELETE-ONLY), the hand-in format (§2.3), the foreign sites (§3).
4. The inventory rows of your classes: `b39_session/INVENTORY_A_MINT_VTABLES.md` (stampers, factory steps,
   creators, per row), `INVENTORY_B_DOORS_DELDTOR.md` (T1 doors and their callers by binding, T2, T4 your
   `RT_DELDTOR` rows, T7 pure slot 0s), `INVENTORY_C_TYPEID_REFLECTION.md` (Table 5: your `?_typeInfo@`
   readers), `INVENTORY_D_STORAGE_RESIDUE.md` (A.1: your storage cells and their sites).
5. THE PILOT — the worked examples you imitate (`B39_RULES.md` §7):
   - `git show 620e197a` — `Picking::PickingMesh`: abstract → concrete (public ctor with retail's stores,
     vector `DeletingDtor`, protected non-virtual dtor), the factory pair as the class's own statics registered
     by name + VA, the FACTREST pair / `RT_DELDTOR` row / `??1` door / slot doors deleted, the fragment line, the
     unit tests `b39_pickingmesh_*` in `tests/unit/pathfinding_picking_abi_test.cpp`.
   - `git show e1e8432a` — `Graphics::SunLight`: an exe-built class whose DOORS die (`::new (p) X()` at the one
     exe builder, the engine_funcs global deleted under DELETE-ONLY, `DoorBody` gone), its mint row retired.
   - `git show 864f95d5` — a behaviour commit: one body for a doubly-registered door.
   - `git show e2626f00` — retiring rows with no reader; `git show a084999c` — the generator mechanism and the
     position-free `RT_DELDTOR` table (`Row<0xADDR>()`).

## The recipe (per class, leaves before bases — B9-11)
1. **Read the class's row in INVENTORY_A** (stampers, factory steps, creators) and its T1/T2 doors. Every
   stamper and every factory step of your class must be gone by step 6; a stamper outside your file set that is
   not a §3 foreign site means the class is not yours to retire in this wave — report it.
2. **Header** (`runtime/types/<ns>/X.h`): public (or protected, abstract) REAL ctor = retail's body minus its
   vptr stores, base's real ctor in the init list (D1.1); `DeletingDtor(unsigned) override` concrete with the
   row's VECTOR arms (D1.2; exe-built classes keep their scalar arms); protected non-virtual `~X()` with the
   `??1` door's body (D1.3). Keep every `static_assert`. A class destroyed through slot 0 declares its OWN
   `DeletingDtor` (B9-4).
3. **Bodies move** into the ctor / dtor / members. Every Runtime-internal builder of the class (a
   `Description::CreateInstance`, `Construct`, `Reconstruct`, an inline stamp) becomes `::new (p) X(…)` (B9-5).
4. **Factory pair** (D2): delete the interpreter row (your `rt_factories_<ns>` pair, or FACTUI1/FACTUI2 rows
   under DELETE-ONLY), write `X::CreateNewObject` / `X::CreateInPlaceObject` in the class's TU and register
   them under retail's mangled names AND VAs (from `standalone/rt_export_addrs.inc`) in your namespace's
   register function. Shared `?CreateInPlaceObject@Internal@Object@@` classes register only `CreateNewObject`.
5. **Doors**: delete the `??0` / `??1` / slot doors whose only callers were the row, the interpreter or a
   derived door (T1 verdicts "dies with the mint", "no caller"). A door reached by address or an import table
   stays as `return ::new (self) X();` (B9-6). Engine_funcs globals that become zero-use: delete them (DELETE-ONLY).
6. **Retire**: in ONE commit (B9-2) — the `RT_DELDTOR` row + wrapper + registration (DELETE-ONLY in
   `deleting_dtor.cpp`), the fragment line in `tools/rt_mint_retired/<your-block>.txt`
   (`??_7X@NS@@6B@  <BLOCK>  <proof: what died>`), then run the generator. If it REFUSES, it lists the reader:
   convert it (or it is foreign — stop and report). Never register a minted name yourself (B9-3).
7. **Type info** (D6): W1 port the retail `static TypeId()` / `GetTypeInfo()` over the minted record (a
   namespace-scope atomic slot filled at port init, B9-10). W3 `DEFINE_TYPE(X, "<exact parent literal>", ext)`
   and retire the `?_typeInfo@X` row (all its readers must be `X::TypeId()` by then).
8. **Storage** (D7, from W1 on, after COORD-0's redirect is on main): the class static in the class, the
   `RtMintRedirectStorage("<symbol>", &X::_s)` call in your `*PortInit`, your files' sites converted. Retire the
   row only when its LAST reader is gone; otherwise hand in `STORAGE-LAST`. `_loader` / `_rng` are never
   redirected (one commit each, D7).
9. **Unit test** (B9-16): `tests/unit/b39_<block>_<topic>_test.cpp` or your existing ABI test: not abstract,
   `!std::is_destructible` (protected dtor), `!std::has_virtual_destructor`, slot order via PMF (adjustment 0),
   the ctor's retail stores, both `DeletingDtor` arms with a recording dtor marker. Update existing tests that
   asserted abstractness or called a deleted door.

EXE and RF blocks follow `B39_RULES.md` D8 / D6 instead of steps 2-8: builders become the real class's ctor,
copy sites and hand-built tables go, `RegisterExeAddr(kVtable)` rows go after their last reader, and every
foreign type-info / storage reader in your files switches to the class accessor (W2).

## Behaviour changes (B9-14)
A converted class whose objects now run a body the minted table did not (a trap / stub / PORT GAP slot that is
now retail's, a base dtor the minted slot skipped, a vptr compare that now sees the compiler table, a factory
that stops returning a stub) is its OWN commit titled `ENDGAME B.3.9 <block>: … (behaviour: …)`. List each
under `BEHAVIOUR` in your report. If a now-running teardown frees something another owner holds, FIND THE
OWNER — never add a guard.

## Process rules
- Work ONLY in your worktree, on your branch. First: `git checkout -b <branch>`, then `git merge main`
  (fast-forward) and confirm `git log -1` shows the coordinator's launch commit named in your prompt.
- Touch ONLY your file set + the DELETE-ONLY rows of your classes (`B39_FILEMAP.md` §2.2) + your §3 foreign
  sites + new files in your directories. Never reformat; never drop existing code when adding to a file.
- FORBIDDEN: `git stash`, `commit --amend`, rebase, push, force anything; sub-agents that edit files;
  `pacman`; unscoped filesystem searches; `getenv`; new `BANISHED_*` flags or instruments; hand-editing any
  generated `.inc` (`rt_data_mint_tables.inc`, `rt_export_addrs.inc`, `cutover_manifest.inc` are regenerated);
  a `Register*()` call inside a `printf` argument; null-check "fixes"; running the game or touching
  `C:/GOG Games/Banished` (the coordinator alone takes the game directory).
- Commit small (one class or one family per commit) via `git commit -F <file>` (no heredoc through the shell;
  avoid the section sign and arrows in the message), title `ENDGAME B.3.9 <block>: <what>`, ending with the
  `Co-Authored-By:` line your prompt gives.
- Helper scripts and logs go in `C:/Users/USPC/AppData/Local/Temp/claude/<your-branch>/`.
- **STATIC GATE before every code commit**, in the FOREGROUND with a 600000 ms timeout, exit code read from
  the command itself (`make … > log 2>&1; echo $?`):
  `/c/msys64/ucrt64/bin/mingw32-make.exe -j10 standalone-debug`, `standalone-perf`, `unit` (all exit 0,
  unit 0 failures); `tools/ghidra_mcp/.venv/Scripts/python.exe tools/dispatch_slot_map.py --check`
  (0 mismatches); `… tools/cutover_manifest.py --gate` (OK; a cosmetic door-count WARN is fine — refresh with
  `--inc` and commit it with your change); `… tools/rt_mint_gen.py --check` (UP TO DATE; regenerate with
  `rt_mint_gen.py` when you retired a row); then `git checkout -- standalone/rt_export_addrs.inc` if the
  generator only re-ended its lines; `git status --short` shows nothing outside your file set. No NEW warnings
  in files you touched (compare the perf build's `warning:` set against your branch point).
- Before your final build: `git merge main` again, REGENERATE the mint and the manifest instead of merging
  them by hand, rebuild, re-run the static gate.
- Ghidra (if needed): pass `program="Application-x64.exe"` / `"Runtime-x64.dll"` on every call; never
  `switch_program`; no renames, no saves. Ghidra's C drops arguments on indirect calls and forwarders: read the
  disassembly.
- If your context gets deep: commit what is verified and report exactly which rows are left. A shorter honest
  block beats a risky one.

## Report (short, ends with the §2.3 hand-in blocks)
Branch + commit list (mark behaviour commits); rows retired (vt / `RT_DELDTOR` / ti / storage) and the new
generator counts; doors deleted vs kept (with the reason a door stays); factory rows converted; tests added;
anything in B39_RULES / FILEMAP / the inventories you found WRONG (with evidence); foreign sites edited; static
gate results verbatim; what you did NOT do and why; the hand-in blocks (`RETIRED`, `GEN-ENTRY`, `MINT-CPP`,
`ENGINE-FUNCS`, `EXE-ROW`, `STORAGE-LAST`, `FOREIGN`, `BEHAVIOUR`, `BOOTLINE`).
