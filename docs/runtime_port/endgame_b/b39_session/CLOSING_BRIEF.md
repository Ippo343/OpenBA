# B.3.9 closing brief (CLOSE: the last owner of step 9)

You run after EVERY block and batch of `B39_RULES.md` §4 is merged and gated. You own no file exclusively
and may edit ANY file; you do not run in parallel with anyone. You finish what the blocks handed in, retire
the rows that needed readers from several blocks, delete the retirement machinery itself, run the acceptance
of `B39_RULES.md` §6, and write the step close. Plan two sessions; commit what is verified between them.

## Read first
1. `CLAUDE.md`; `B39_RULES.md` (whole: D3, D4, D5, D7 and §6 are yours); `B39_FILEMAP.md` §2 (the shared files
   you now empty) and §2.3 (the hand-in lines).
2. Every block report's hand-in blocks (the coordinator collects them in `b39_session/HANDINS.md`): `STORAGE-LAST`,
   `TYPEID-DEAD`, `ENGINE-FUNCS`, `EXE-ROW`, `GEN-ENTRY`, `MINT-CPP`.
3. `b39_session/INVENTORY_D_STORAGE_RESIDUE.md` A.1/A.4 (storage), `INVENTORY_C` Tables 1/3/4 (type ids),
   `INVENTORY_B` "Must stay" (what survives the step).

## The work, in this order (each a commit or a few; static gate before each)
1. **Re-measure first**: `settled.py` (open rows per owner — every open row that is not CLOSE's is a block
   that did not finish: stop and tell the coordinator), `rt_mint_gen.py --check` counts, `grep -c
   'RT_DELDTOR(0x' runtime/src/object/deleting_dtor.cpp`.
2. **Storage rows whose readers spanned blocks** (D7): per row (Timer's five together, Error's three, the four
   Video list heads), convert the remaining readers (`STORAGE-LAST` lists), including the second-level
   `engine_funcs` globals and the exe import-slot exrefs (`ResolveAddr(Module::Exe, 0x14015…)`), delete the
   `RtMintRedirectStorage` call, add the fragment line `tools/rt_mint_retired/close.txt`, regenerate. Keep
   `?_singleton@MemoryAllocator@System@@` and `?_disableHandler@Debug@System@@` (they STAY — D10).
3. **Type-id close** (D5): for every `TYPEID-DEAD` cell (re-grep the whole tree for the address, the `g_tid_`
   and `g_TypeId_` names — a read by name counts): its `RegisterExeAddr` / registrar row
   (`reg_*typeid*.cpp`, `reg_unreg_lane4.cpp`, `reg_food_typeid.cpp`, …), its `g_tid_*` storage, its `Fill*`
   line in `game_main.cpp` (RF's file — you own it now), its `g_TypeId_*` global (typedef + extern +
   definition + initialiser + dump line). Registrar files that empty out are deleted. The five non-type-id cells
   (0x1401c3dd8/3de0/3de8 singletons, 0x1401c22d0/23d0 tables) STAY.
4. **The `RT_DELDTOR` machinery** (D4): with the table empty, delete `kSpecs`, the wrappers,
   `DeletingDtorCore`, `EhVectorDtor`, `RtObjectDeletingDtorPortInit` and its call, `deleting_dtor_leaves.cpp`
   if empty — KEEP `deleting_dtor_internal.h`'s `rtcoll::MemFree` / `MemAlloc` (three other TUs include it).
5. **The mint** (COORD's files, yours now): with no vtable and no type-info row left, delete the vtable and
   `BaseTypeInfo` paths of `rt_data_mint.cpp` (`MintVtables`, `MintTypeInfos`, the trap stubs, the roster
   lines) and of `rt_mint_gen.py` (keep the storage path and the RETIRED mechanism while the two STAY rows are
   minted); regenerate; `[RtMint]` then reports 0 vtables, 0 BaseTypeInfo, 2 storage symbols.
6. **The manifest**: when `cutover_manifest.inc` has no row left (or only the two STAY rows — decide with the
   coordinator), delete `reg_mint_promoted.cpp` (TAG_FILE), the manifest machinery and its gate from the gate
   kit, in one commit. `RT_DATA_SLOT` (`runtime/funcs/rt_port.h`) goes when its last use does.
7. **Acceptance**: run every command of `B39_RULES.md` §6 and paste the output into STATUS. A non-zero line
   is a finding: fix it or name the block that must.
8. **The step close** (coordinator runs the game; you prepare): `b38_session/stepclose.sh`'s route set, the
   true release compile then `make clean`, the clean-rebuild gate, the `NOT IMPLEMENTED` stub-set diff, the
   tutorial `[TownCensus]` line. Write `docs/runtime_port/STATUS.md`'s B.3.9 block (counts before → after:
   mint 152/104/48 → 0/0/2, `RT_DELDTOR` 135 → 0, exe vtable rows 6 → 0, type-id cells 148 → 5 non-type-id,
   storage rows converted), `B39_RULES.md` §8 "as run" corrections, and a new `NEXT_SESSION.md` for the
   normal-virtual cutover (D10 OUT; INVENTORY_B Part C is its size).

## Process rules
- Your own worktree + branch (`git checkout -b b39-close`, `git merge main`). Commit per item via
  `git commit -F <file>`, title `ENDGAME B.3.9 close: …`, ending with the `Co-Authored-By:` line your prompt gives.
- FORBIDDEN: `git stash`, amend, rebase, push; sub-agents that edit; running the game (the coordinator gates);
  hand-editing a generated `.inc`; `getenv`; new flags or instruments; null-check "fixes"; a `Register*()` in a
  `printf`.
- STATIC GATE before every commit, foreground, 600000 ms, exit code from the command:
  `/c/msys64/ucrt64/bin/mingw32-make.exe -j10 standalone-debug`, `standalone-perf`, `unit` (0 failures);
  `dispatch_slot_map.py --check` 0 mismatches; `cutover_manifest.py --gate` (until you delete it);
  `rt_mint_gen.py --check` UP TO DATE; `git checkout -- standalone/rt_export_addrs.inc` if only line endings.
- A behaviour change you find (a storage reader that read 0 until now, a type id that was 0) is its own titled
  commit `… (behaviour: …)`, listed for the coordinator's resave ×5.

## Report
Branch + commits (behaviour ones marked); the acceptance output verbatim; rows that STAY and why; boot-line
changes (`BOOTLINE`); anything the sheet got wrong.
