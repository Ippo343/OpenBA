# B3R block 0 — inventory F: boot log, gate, tools, tests, harnesses (BOOTLOG_GATE_TOOLS)

Question (NEXT_SESSION §3b.6 plus what depends on the registry): deleting registry rows changes boot-log
lines, tools and tests. Which are gate invariants, which are re-cut in writing, which die?

Table: `data/F_bootlog_gate_tools.tsv` (132 rows). Script: `scripts/F_bootlog_gate_tools.py`. Both are
read-only on product code.

## 1. The answer

1. **The gate reads 5 registry lines, and 2 of those 5 checks are dead.** `gate.sh` checks `[RtResolve] 0 misses`, `[RtMint] TRAP` = 0, `GATE WARN` = 0, `dispatch … 0 mismatch`, and `mintgen … UP TO DATE`. `stepclose.sh` also reads the `NOT IMPLEMENTED` count and set. Nothing prints `[RtMint] TRAP` (it went with the vtable mint at B.3.9) or `GATE WARN` (no producer anywhere), so both checks can only read 0.
2. **The stub-set check in `stepclose.sh` compares nothing.** Its sed replaces every address with `ADDR`. The set therefore collapses to one line, and "0 new, 0 gone" is guaranteed. Today the set is 10 addresses on the new-game route (8 tutorial, 1 load-game / menus, 10–11 tool routes, 9 in-game save).
3. **A run cannot show a deleted Runtime ADDRESS row that still has a reader.** Here is what each miss prints:
   - a `ResolveAddr(Module::Runtime, …)` miss after init: nothing;
   - a plain `ResolveExport` miss: `[RtUnresolved]`, which no gate script reads;
   - a caching-door miss: `[RtResolve] MISS`.

   So an address-row deletion has to be proved statically (link plus generator-style refusal), not by the boot log.
4. **Invariants through the teardown (8).**
   - `[RtResolve]` MISS lines: 0.
   - `[RtUnresolved]` lines: 0. This needs a new gate check on a line that already exists.
   - `[ExtLoad]` new game `calls=5683 openFileFail=0`; tutorial `5773 / 12`; Load Game `4758`.
   - typetable: 428 lines, md5 `b983f144`, lazy lookups excepted.
   - `dispatch` mismatches 0 and shadowed 0.
   - mintgen: UP TO DATE and no refusal.
   - `[RtPort] Engine lifecycle: REFUSING` and the `EngineFuncsInit` warning: absent.
   - `[RtResolve] 0 misses`: present, until the commit that deletes the caching door re-cuts `gate.sh`.
5. **Re-cut at each deletion (6 lines, plus the stub set).** These numbers only fall:
   - `[RtPort] runtime registry: 1347 exports, 1817 addrs`
   - `[AddressRegistry] 759` (printed twice)
   - `[RtMint] registered 2/2`, `minted 2 (32 bytes)`
   - `dispatch: checked 35`
   - `mintgen … 2 storage`
   - the `NOT IMPLEMENTED` set, compared by address.
6. **Die with the registry.**
   - 9 log lines: `[RtResolveEarly]`, `[RtSlotAudit]`, `[TRACE] ResolveAddr`, `[Resolve-Standalone] Address registry`, `Coverage: 759 / 582`, the crash-log `Registry:` line, 58 per-family `[RtPort] … registered` print sites, TRAP, GATE WARN.
   - 9 tools: `export_name_audit`, `addr_door_audit`, `dup_runtime_export` (and its controls), `unreg_full`, `unreg_loadpath`, `cutover_closure/`, `rt_data_harvest`, and `rt_export_addrs.inc`.
   - 7 tools change: `dispatch_slot_map`, `rt_mint_gen`, `runtime_census`, `slot_name_audit`, `abi_retslot_audit`, `check_standing_notes`, `slot3_resolver_audit`.
7. **Tests.** 3 unit tests go through the registry, via `g_unitResolveAddr` in `tests/unit/rt_port_stub.cpp`: the 2 in `alloc_door_test.cpp` and 1 in `array_test.cpp`. They need another seam. The 5 `InitFields`/`ReleaseFields` files (carry-over 17) are LINK seams, not registry seams. They survive unless the product translation unit becomes linkable into `unit.exe`.
8. **Harnesses.**
   - 15 `standalone/` files hold 35 door lines. Another 18 files use `engine_funcs` externs only (218 uses in 33 files).
   - Only 6 of those files run on a kit route: `main`, `menudialogloopt_stubs`, `splash_loop_stubs`, `pausemenu_harness`, `tool_harness`, `stress_harness`. The rest are compile-checked only.
   - `redzone_alloc.cpp` WRITES the allocator dispatch-slot cells. It depends on registry STORAGE.

## 2. How the table was built

- **The script.** `python docs/runtime_port/endgame_b/b3r_session/scripts/F_bootlog_gate_tools.py`, run from the repo root with no arguments. It takes about 25 s and is deterministic (two runs gave the same md5). It imports `strip()` from `b3x_session/codegrep.py`, which strips comments and keeps strings. It also excludes `standalone/build*/`, `tests/unit/build/` and `.claude/worktrees/`.
- **Columns.**
  - `item`: the line, tool, test or harness.
  - `kind`: `log_line`, `tool`, `test` or `harness`.
  - `source_file` / `source_line`: the code line that PRINTS the line, found by its format string in stripped code. For a tool, the file count and line count. For a test, its door lines and `UNIT_TEST` count. For a harness, its binding counts.
  - `depends_on`: `exe_addr`, `rt_name`, `rt_addr`, `mint` or `engine_funcs`.
  - `read_by`: grep of the b3x kit (`*.sh`, `*.py`), `tools/`, `.claude/skills`, `.claude/rules`, `Makefile` and `CLAUDE.md` for the line's token or the tool's file name.
  - `verdict`: `invariant`, `recut`, `delete`, `keep` or `keep_changed`.
  - `note`: the value printed today and the reasoning.
- **Values printed today.** A run is not allowed in block 0, so these are embedded from the B3X close runs: `gate.sh closeclean` and `stepclose.sh` on `0419d0ce` (session `e971e316`'s scratchpad `kit/runs/`, not committed). They were read from those logs, and also from `runs/baseline` (`deaf604c`): every registry number was identical at both ends of B3X.
- **Harness binding regexes** (whole-file, multi-line):
  - `exe_slot`: `ResolveAddr(Module::Exe, 0x14015[12]xxx)`
  - `exe_addr`: other `Module::Exe`
  - `rt_addr`: `Module::Runtime` and `RtPortLookupAddr`
  - `rt_name`: `ResolveExport`, `RT_EXPORT_ONCE`, `RtResolveCached`, `RT_DATA_SLOT` and `RtPortLookupExport`
  - `helper`: `RSlot`, `ResolveSlotPtr`, `ResolveFn<`, `ResolveSlot(`, `RtAddr<`, and local `slot(` / `SlotFn(` / `R<>(0x14…`
  - `register`: `Register{ExeAddr,RuntimeExport,RuntimeAddr,DispatchSlot}(`
  - `engine_funcs`: any of the 228 `extern` names in `engine/funcs/engine_funcs.h`
  - Flags come from `Opt().field` in the file. The route mapping comes from the env lines of the kit and `tools/resave_gate`.

## 3. Counts

Rows per kind and verdict: log_line 42 (invariant 9, recut 20 [including 13 stub rows], delete 9, keep_changed 4); tool 25 (keep 9, keep_changed 7, delete 9); test 27 (keep 10, keep_changed 17); harness 38 (keep 5, keep_changed 33).

| directory | log_line | tool | test | harness |
|---|---:|---:|---:|---:|
| `runtime/` | 8 (rt_resolve_cache 5, the RtPort family, REFUSING, ExtLoad) | — | 16 (unit-linked sources) | — |
| `standalone/` | 16 | 1 (`rt_export_addrs.inc`) | — | 36 |
| `code/` | 14 (13 stub readers + typetable) | — | — | 2 |
| `tools/` + kit | 2 (the `dispatch` / `mintgen` gate lines) | 24 | — | — |
| `tests/unit/` | — | — | 11 | — |
| no producer | 2 (TRAP, GATE WARN) | — | — | — |

The row classes this inventory touches (for inventory A's table):

| row | class | reason |
|---|---|---|
| mint 2/2 | STORAGE | carry-over 1 plus `_disableHandler`; `main.cpp:564`'s crash-log reader is one more by-address reader of `0x1801fc718` |
| dispatch-slot cells `0x140152b60` / `0x140152b68` | STORAGE | a hook cell `redzone_alloc.cpp` writes and `alloc_door.cpp` re-reads |
| `0x140089470` | STUB, deliberately null | flag-gated |
| `0x1401c22d0` / `0x1401c23d0` | DIRECT | the reader already falls back to a local C++ table |
| the other 8 newgame stub addresses | STUB | resolved in an init batch, so they may never be called |

## 4. Blind spots

- **The boot log cannot see a dead ADDRESS door.** A `ResolveAddr(Module::Runtime, x)` after `RtResolveMarkPortInitDone()` that finds no row returns 0 silently (`resolve_standalone.cpp:429-439`). The early-note recorder only records before init. The Exe door prints `NOT IMPLEMENTED` once per address, at RESOLVE time. Many readers resolve a whole batch in one init function (`gameupdateloop.cpp:450-518`, `edittool.cpp:150-156`, `action_base_cnr.cpp:128-134`), so the stub set lists addresses that were RESOLVED, not addresses that were called.
- **Only the routes the kit runs are seen.** The 29 harness rows marked "no kit route" run under no gate. Their door lines are verified by the compiler only.
- **The kit reads code lines, never the log.** As a result, the table's `read_by` for a log line comes from the kit only. A person reading `standalone_crash.log` (triage-run skill) is not counted.
- **Current values come from the B3X close, not from HEAD.** HEAD is the close plus documentation commits, and the working tree showed only `rt_export_addrs.inc` modified, which the kit reverts, so no drift is expected. Still, a block-1 baseline run must re-read every number.
- **Three `RegisterExeAddr` rows exist before `ResolveInit()`** (`[Resolve-Standalone] Address registry: 3 entries`). They are registered by something that runs before step 2 of `Init()`. I did not find which code registers them: no file-scope `static … = Register…(` initialiser matched.
- **Harness regexes see the ten spellings of BLOCK0_BRIEF only.** A per-file wrapper with another name would read 0. I read the 15 files that have doors, plus `hw_watch`, `stall_debug`, `entity_comp_validator`, `sampler` and `saveload_harness` (0 doors), by grep context, not line by line. `engine_funcs` counts are identifier matches, so a local variable with the same name as an extern would count.
- **Env-gated diagnostics inside `code/Game`** (for example the `gameupdateloop.cpp` FORCE_/TRACE blocks and `SetUnregPhase`) are counted by the `code/` inventories, not here. Only the two `code/Game/Game/*_harness.cpp` files are in this table.
- **Tool `read_by` is a filename grep.** A tool invoked by a path held in a variable would be missed. The kit and the Makefile were also read by hand.

## 5. What surprised me

1. **`stepclose.sh`'s stub SET check is vacuous.** `sed -E 's/0x[0-9A-Fa-f]{8,}/ADDR/g'` erases the one field that distinguishes the lines. `CLOSE_NUMBERS.md` §5's "`NOT IMPLEMENTED` stub set: 0 new, 0 gone" was therefore guaranteed rather than measured. The real sets are in the table (`stub:` rows).
2. **The gate checks `[RtMint] TRAP` and `GATE WARN`, and neither string has a producer.** The checks appear in `gate.sh`, `uiroutes.sh`, `stepclose.sh`, `mods_route.sh`, `devlost.sh` and `devlost_at.sh`. `GATE_TABLE.md` reports "0 TRAP, 0 GATE WARN" on every row as if it were evidence.
3. **`tools/rt_mint_gen.py` rewrites `standalone/rt_export_addrs.inc` even under `--check`.** `write_export_addrs(pe)` at `tools/rt_mint_gen.py:702` runs before the `--check` branch. That is the permanent `M standalone/rt_export_addrs.inc` in `git status`, and why every kit script ends with `git checkout -- standalone/rt_export_addrs.inc`. Once the `.inc` is deleted, the next gate `--check` would RE-CREATE it untracked, and `mergegate.sh`'s `TREE NOT CLEAN` guard would then refuse the next merge.
4. **`rt_export_addrs.inc` is not a list of rows with readers.** It is the whole DLL export table, 4,926 rows (§3a says "1,480 with a reader"). `resolve_standalone.cpp` reads it for one purpose: to MIRROR each `Register*` call into the opposite door. It cannot shrink row by row. It dies whole when no reader uses the door opposite its registration. For the same reason, `[RtPort] runtime registry: 1347 exports, 1817 addrs` counts map entries including mirrors: 1,817 addresses against 1,452 `RegisterRuntimeAddr` code lines.
5. **`[RtResolve] 0 misses`, the line NEXT_SESSION §3b.6 names first, covers only the caching door** (`RtResolveCached` / `RT_EXPORT_ONCE` / `RT_DATA_SLOT`, `rt_resolve_cache.cpp:316-358`). `gate.sh` flags a route WITHOUT it, so the commit that deletes that door must edit `gate.sh`, or every later gate fails. `[RtUnresolved]`, the line that covers plain `ResolveExport`, is checked by no script.
6. **The registry is used as a feature flag once.** `standalone/reg_town_workplace_registry.cpp:39` registers `0x140089470` only under `BANISHED_LIVE_SEAT_SHUFFLE`, and `town_workplace_registry.cpp:57`'s null means "deterministic seat pick", the chosen default. The address therefore sits in the new-game stub set on purpose. The `Makefile:293` comment says every `BANISHED_LIVE_*` flag graduated or was deleted; this one did neither.
7. **`redzone_alloc.cpp` (BANISHED_REDZONE) swaps the allocator by writing into the exe dispatch-slot cells `0x140152b60` / `0x140152b68`.** `rtcoll::DoorAlloc` / `DoorFree` re-read the cell on every call precisely for that, and `alloc_door_test.cpp` pins it. There are 30 more reader lines in `code/Game` (20 files), and most of them copy the pointer into a static once (`s_x = *slot`). Whether the shim sees their allocations therefore depends on whether it was installed before their first call. Converting these doors to direct calls silently breaks the redzone instrument unless the cell survives as a named hook.
8. **Two stub-set entries are dead resolves.** The readers of `0x1401c22d0` / `0x1401c23d0` (`action_pf_geom.cpp:279`, `action_pf_road.cpp:324`) fall back to a local C++ table. Deleting the resolve changes nothing except removing two stub lines.
9. **`tests/unit/rt_port_stub.cpp` declares `Module { Exe, Runtime, VideoDX11, COUNT = 3 }`, but `standalone/resolve.h` has `COUNT = 2`.** It is harmless because enumerators do not mangle. Its comment claims "byte-identical".
10. **Two `[RtPort]` families share the tag.** The registration counts die with the registry, but the ATTENDANCE lines do not, and `stepclose.sh` reads `Audio::Stream ATTENDANCE`. A sweep keyed on the tag would break the audio check.
11. **`dispatch_slot_map.py --check` name-checks only `RegisterDispatchSlot(addr, ResolveExport("…"))` pairs**: 35 of the 44 `RegisterDispatchSlot(0x…` code lines. The other 9 pass a function pointer and are only checked for shadowing. The READERS of the slots, which are among the 2,185 `ResolveAddr(Module::Exe` lines, are not checked at all. Its green says nothing about whether a reader wants the symbol the slot holds; `slot_name_audit.py` is the report for that. It also opens the RETAIL exe in the game directory.
12. A stale worktree `.claude/worktrees/agent-a7321f2dba083cc3a` exists, but NEXT_SESSION §1 wants main only.

## 6. Proposal

### 6a. Invariants through the whole teardown (every merge)

new game `[ExtLoad] calls=5683 openFileFail=0`, tutorial `calls=5773 openFileFail=12`, Load Game 4758-4787;
typetable 428 lines md5 `b983f144` (minus lazy by-name lines, as today); resave x5 == golden; unit N / 0
failures; route exit 0, crash log 0; `[RtResolve] MISS` = 0; **`[RtUnresolved]` = 0 (new check on an
existing line)**; `dispatch … 0 name mismatch(es); 0 … more than once`; `mintgen` UP TO DATE with no
`RETIRED ROW STILL READ`; `REFUSING to register` and `Some engine functions not resolved` absent;
`[RtResolve] 0 misses` present **until** the commit that deletes `rt_resolve_cache.cpp` (that commit edits
`gate.sh`).

### 6b. Re-cut lines and the rule

These lines are re-cut: `[RtPort] runtime registry: E exports, A addrs`; `[AddressRegistry] N`; `[RtMint] registered n/a`, `minted m (b bytes)`; `dispatch: checked N`; `mintgen … N storage`; the `NOT IMPLEMENTED` set per route (newgame, tutorial, loadgame, menus, tools, savefile).

**Rule, written into the merge's GATE_TABLE row:**
- Record old → new for each changed number.
- For the stub set, record each address that LEFT together with its disposition:
  - (a) the reader now calls a named C++ body;
  - (b) the reader was deleted as dead;
  - (c) the null was made explicit (STUB kept, and it moves to the written STUB list).
- A number that RISES is a regression. So is an address that ENTERS the set. A line that vanishes must be named in the merge title, the way the typetable lazy lines are.
- `stepclose.sh` must keep the addresses in its set comparison (its sed changes to `s/[0-9A-Fa-f]{16}/PTR/g` only).

### 6c. Kit and tool changes, before block 1 (each its own commit; no product change)

1. In `b3r_session/gate.sh`:
   - drop the TRAP and GATE WARN counts;
   - add `[RtUnresolved]` count == 0;
   - add the per-route `NOT IMPLEMENTED` set diff against `runs/baseline`, with addresses kept;
   - print `[RtPort] runtime registry` and `[AddressRegistry]` into the summary so the re-cut numbers land in the record.
   
   Drop the TRAP count from `uiroutes.sh`, `stepclose.sh`, `mods_route.sh` and `devlost*.sh` too, and fix `stepclose.sh`'s sed.
2. `tools/rt_mint_gen.py`: `--check` must not write `rt_export_addrs.inc`. Move `write_export_addrs` behind `--export-addrs-only` or after the check. Without this, deleting the `.inc` breaks `mergegate.sh`.

### 6d. Blocks (disjoint file sets) for the rows of THIS inventory

| block | files | rows | first? / behaviour |
|---|---|---:|---|
| F-K (kit) | the 10 `b3r_session/*.sh` copies + `acceptance.sh` | 12 log-line rows | FIRST (6c.1) |
| F-G (generator) | `tools/rt_mint_gen.py` | 2 | FIRST (6c.2) |
| F-ALLOC | `runtime/src/collection/alloc_door.cpp`, `standalone/redzone_alloc.cpp`, `tests/unit/alloc_door_test.cpp`, and the new named hook cell (header in `runtime/types/collection/`) | 3 | before any block converts a `0x140152b6x` reader; titled commit, behaviour-neutral, check it with a `BANISHED_REDZONE` run |
| F-SEAT | `standalone/reg_town_workplace_registry.cpp`, `code/Game/Game/town_workplace_registry.cpp` | 2 | separate TITLED commit: the `Opt().liveSeatShuffle` test moves to the call site; A/B default unchanged |
| F-HARN-ROUTED | `standalone/{main,menudialogloopt_stubs,splash_loop_stubs,pausemenu_harness,tool_harness,stress_harness}.cpp` | 6 | gated by gate.sh + toolroutes + sa-stress; `main.cpp`'s crash-log cell reader goes WITH carry-over 1's storage block |
| F-HARN-UNROUTED | the other 9 door files (`death`, `food_panel`, `food_workers`, `market_overstock`, `orchard_workers`, `pasture`, `trade_buy`, `dlgctrl_route`, `modal_driver`) + `scene_watch.cpp` (carry-over 4; edits `check_standing_notes.py` NOTES in the same commit) | 10 | compile-only; one smoke run each is advised, since no gate route reaches them |
| F-ARRAY-TEST | `tests/unit/array_test.cpp` | 1 | must ride IN the block that owns `runtime/src/collection/array.cpp` (a test-local `ArraySpec` replaces `PkgResolver`) |
| F-CLOSE | `standalone/resolve_standalone.cpp`, `runtime/funcs/rt_resolve_cache.cpp`, `standalone/rt_data_mint.cpp`, `standalone/address_registry.cpp`, `tests/unit/rt_port_stub.cpp`, `tests/unit/sources.mk`, `b3r_session/gate.sh` (the `[RtResolve] 0 misses` re-cut), the dying tools (9) | 25 | LAST: the closing owner; deletes the log lines of §1.6 |

The 18 engine_funcs-only harness files belong to whichever block dissolves `engine/funcs`. Converting an extern to a declaration does not change their text if the name is kept.

### 6e. `acceptance.sh` for the step (countable lines, CODE lines via `codegrep.py`; PROD = code runtime standalone engine dx11)

| id | line | target |
|---|---|---|
| R1 | `RegisterRuntimeExport(` + `RegisterRuntimeAddr(` in PROD | 0 (the storage rows: 0 once carry-over 1 lands) |
| R2 | `RegisterExeAddr(` + `RegisterDispatchSlot(` in PROD | = the CALLBACK rows listed by name in the rule sheet |
| R3 | `ResolveExport(` / `RT_EXPORT_ONCE(` / `RtResolveCached(` / `RT_DATA_SLOT(` in PROD + tests | 0 |
| R4 | `ResolveAddr(Module::Runtime` + `RtAddr<` + `RtPortLookup` in PROD | 0 |
| R5 | `ResolveAddr(Module::Exe` in PROD | = STUB list + CALLBACK list, listed by name |
| R6 | files exist: `standalone/rt_export_addrs.inc`, `rt_data_mint*.{cpp,h,inc}`, `resolve_standalone.cpp`, `address_registry.cpp`, `reg_*.cpp`, `runtime/funcs/rt_resolve_cache.cpp` | 0 (or each listed with its reason) |
| R7 | `extern` in `engine/funcs/engine_funcs.h` | = the STUB / not-yet-ported list |
| R8 | `g_unitResolveAddr` in tests | 0; `tests/unit/rt_port_stub.cpp` deleted |
| R9 | `ResolveAddr(Module::Exe, 0x140152b6[08])` in PROD | 0 (the redzone hook is a named cell) |
| R10 | `RegisterExeAddr(0x140089470` | 0 (the flag is tested at the call site) |
| R11 | kit greps `RtMint\] TRAP`, `GATE WARN` | 0 |
| R12 | `rt_mint_gen.py --check` (or its successor retired-reader check) | exit 0, 0 RETIRED ROW STILL READ; does not dirty the tree (`git status --porcelain` empty after it) |
| R13 | `dispatch_slot_map.py --check` | 0 mismatches (vacuous at 0 rows: then removed from gate.sh) |
| R14 | stepclose newgame log: `[RtUnresolved]` / `[RtResolve] MISS` / `[RtMint]` / `[RtPort] runtime registry` / `[AddressRegistry]` lines | 0 / 0 / 0 / 0 / 0 |
| R15 | stepclose `NOT IMPLEMENTED` set per route, by address | ⊆ baseline; every address that left is listed with disposition (a)/(b)/(c) |
| R16 | invariants of 6a | as stated |
| R17 | `unit` | N tests, 0 failures (N re-cut when the 3 injected tests are re-seated; not lower) |

It covers §3f: every row classified (R1-R7 against the rule sheet), 0 DEAD left (R1, R3, R4), 0 readers
of a DIRECT row (R3-R5), the mint deleted or listed (R1, R6, R12), `engine_funcs` = STUB list (R7), the
gate (R16), `stepclose.sh` (R14, R15).
