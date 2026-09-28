# B.3.9 CLOSE NUMBERS — the step's measurements, for the STATUS block

Measured by the CLOSE owner on branch `b39-close` (head after the code commits: `5cbac19e`, on main `851adb15`).
"Start of step" is `ab1e0846` (the B.3.8 close = the step-9 gate baseline, GATE_TABLE row 1), measured the same way
on a copy of that commit's `code engine standalone runtime dx11 tests tools` (`git archive ab1e0846 … | tar -x` into
the scratch directory). The scripts are `C:/Users/USPC/AppData/Local/Temp/claude/b39-close/numbers39.py` (one tree →
JSON) and, committed beside this file, `codegrep.py` (comment-stripping grep), `acceptance.sh` (B39_RULES §6) and
`partc.py` (the normal-virtual cutover's size, for NEXT_SESSION.md). "Code lines" strip `//` and `/* */` comments and
keep string literals; `build*/` and `obj/` are excluded everywhere.

---

## 1. The scaffolding the step existed to delete

| what | start of step | now | command / source |
|---|---:|---:|---|
| minted vtables | **152** | **0** | `rt_mint_gen.py --check` / `grep -cE 'kVTable[0-9]+, kVTraps[0-9]+' standalone/rt_data_mint_tables.inc` |
| minted `BaseTypeInfo` records | **104** | **0** | same; `grep -c '"?_typeInfo@' …tables.inc` |
| minted storage rows | **48** | **2** | same (`?_singleton@MemoryAllocator@System@@`, `?_disableHandler@Debug@System@@`) |
| retired rows listed in `tools/rt_mint_retired/*.txt` | 0 | **302** (15 fragments) | the generator's `retired N row(s)` line |
| `RT_DELDTOR` rows (`deleting_dtor.cpp`) | **135** | **0** (file deleted) | `grep -c 'RT_DELDTOR(0x' runtime/src/object/deleting_dtor.cpp` |
| factory interpreters (FACTREST, FACTUI1, FACTUI2) | 3 | **0** | `ls runtime/src/object/rt_factories_* runtime/src/ui/factory_ui1*` |
| hand-built vtables | **5** (`Dispatcher<Tag>::vtbl`, `g_NoiseUI_ImageBufferVtable_Standalone`, `g_EnumStub_Vtable`, the two `thumb_vtables.cpp` mirrors) | **0** | `numbers39.py` hand-built scan (named `void* *Vtable*/*vtbl*/*_vt*[N]` in code/Game + standalone) + `thumb_vtables.cpp` deleted |
| `RegisterExeAddr` of an exe vtable address (0x140153000–0x14016ffff) | **6** | **0** | `numbers39.py` (code lines, classified by the first argument) |
| type-id cells read (0x1401c…, minus the 5 non-type-id cells) | 148 cells / 441 sites (INVENTORY_C) | **0** code lines | `acceptance.sh` A6 |
| `g_TypeId_*` / `g_tid_*` globals | 76 globals (821 reads) + 97 `g_tid_` | **0** | `grep -rn 'g_tid_\|g_TypeId_' code engine standalone` |
| cutover manifest rows / tag file / gate step | 7 rows (46 tags) | **deleted** | commit e8714fed |
| the D7 storage redirect (`rt_mint_redirect.h`, `RtMintRedirectStorage`) | added by COORD-0 | **deleted** (0 calls) | commit e51b15ee |

## 2. Doors and the registry

| what | start of step | now | command / source |
|---|---:|---:|---|
| ctor doors registered by name (`Register*("??0…`) | 206 | **99** | `numbers39.py` doors.ctor_by_name (code lines) |
| dtor doors registered by name (`Register*("??1…`) | 181 | **37** | doors.dtor_by_name |
| `rt_export_addrs.inc` rows | 4,926 | 4,926 | the generated table (every named DLL export; unchanged by design) |
| — rows with a reader (name literal or its address as a literal, anywhere in code) | 2,794 | **1,480** | numbers39.py rows_with_reader |
| `RegisterExeAddr` statements — function (< 0x140151000) | 846 | **642** | numbers39.py |
| — import-slot range (0x140151000–0x140152fff, literal) | 2 | 1 | (most slot rows go through `RegisterDispatchSlot(…)` wrappers, not counted here) |
| — exe vtable range | 6 | **0** | |
| — data / type-id cells (≥ 0x140170000) | 142 | **45** | |
| — through a variable / expression | 11 | 6 | |
| — **total** | **1,007** | **694** | |
| `standalone/address_registry.cpp` lines | 1,617 | **1,243** | `wc -l` |
| `standalone/reg_*.cpp` files | 145 | **127** | `ls standalone/reg_*.cpp` |
| `engine/funcs/engine_funcs.h` lines | 4,292 | **3,036** | `wc -l` |
| `engine/funcs/engine_funcs.cpp` lines | 2,576 | **1,652** | `wc -l` |
| `engine_funcs.h` `extern` declarations (globals) | 414 | **227** | code lines starting `extern` (not `extern "C"`) |

## 3. Resolver sites (code lines containing the token, every tree)

| token | start of step | now |
|---|---:|---:|
| `ResolveExport(` | 443 | **205** |
| `RT_EXPORT_ONCE(` | 391 | **242** |
| `RT_DATA_SLOT(` | 422 | **4** (the `#define`, the two `MemoryAllocator` slots, the one `_disableHandler` slot — the two STAY rows) |
| `ResolveAddr(Module::Exe` | 2,690 | **2,185** |
| `ResolveAddr(Module::Runtime` | 386 | **275** |

## 4. Reflection

| | start of step | now |
|---|---:|---:|
| `DEFINE_TYPE(` (Runtime classes, code lines) | 5 | **175** |
| `DEFINE_EXE_TYPE(` | 296 | 297 |
| `DECLARE_EXE_TYPE(` | 297 | 310 (the 13 abstract bases got `DECLARE_TYPE`-shaped records, RF) |
| `REGISTER_TYPE` / `REGISTER_ABSTRACT_FACTORY` / `InvokeAllRegistrations` | 3 / 13 / 1 | **0 / 0 / 0** |
| `DEFINE_EXE_VTABLE(` | 309 | **6** (4 with a `Vtable()` reader + the 2 test probes) |
| reflection chain (`Post-Startup _firstTypeInfo`) | 416 (1 duplicate `Internal`) | **414** records, 414 distinct ids (GATE_TABLE, RF / TIR / TIUI merges) |
| `GetTypeIdByName(` call lines (definition excluded) | 52 | **10** |
| — with a string LITERAL argument | 17 | **0** |
| `ResolveTypeIdByNameLazy(` call lines | 38 | **4** |
| — with a string LITERAL argument | 31 | **0** |

The by-name helpers that stay are called only with names known at RUN time: the three generic serialize fallbacks
(`serialize_arrays.cpp:398`, `serialize_externals.cpp:85`, `serialize_externals_typed.cpp:73` — `ResolveTypeIdByNameLazy
(simple)`), the tool / panel harness tables (`toolbardialog.cpp` `ToolbarUtility_HarnessDriveController`,
`disaster_harness.cpp`, `food_panel_harness.cpp`, `food_placement_harness.cpp`, `food_workers_harness.cpp`,
`panel_hold_harness.cpp` ×2, `tool_harness.cpp`) and the helpers' own internals.

## 5. Unit tests

| | start of step | now |
|---|---:|---:|
| runner line | `736 tests, 0 failures` | **`914 tests, 0 failures`** |
| `tests/unit/*.cpp` files | 69 | 79 |

(919 on main `851adb15`; this close deleted `mint_redirect_test.cpp` (5 tests) with the redirect and replaced one
mint-parsing test in `b39_obj_typeinfo_unique_test.cpp`.)

## 6. Bridge casts and the B.4 metric

`// B.3.x bridge` LINE counts (raw text, every tree):

| marker | start of step | now |
|---|---:|---:|
| `B.3.1` | 180 | 178 |
| `B.3.2` | 200 | 200 |
| `B.3.3` | 1,018 | 1,006 |
| `B.3.4` | 1,298 | 1,283 |
| `B.3.5` | 899 | 755 |
| `B.3.6` | 2,046 | 1,714 |
| `B.3.7` | 400 | 330 |
| `B.3.8` | 586 | 562 |
| `B.3.9` | 0 | 8 |
| **total** | **6,627** | **6,036** |

`reinterpret_cast` — LINE counts per directory (`grep -c` semantics over `*.cpp *.h *.inc`):

| directory | start of step (`ab1e0846`) | now | delta |
|---|---:|---:|---:|
| `code` | 21,414 | **20,863** | −551 |
| `runtime` | 6,899 | **5,773** | −1,126 |
| `standalone` | 2,554 | **2,213** | −341 |
| `engine` | 479 | **259** | −220 |
| `dx11` | 173 | **173** | 0 |
| `tests` | 538 | **589** | +51 |
| **TOTAL** | **32,057** | **29,870** | **−2,187** |

The start value equals the B.3.8 close's (32,057, `b38_session/CLOSE_NUMBERS.md` §1), so the two steps chain.

## 7. The acceptance (B39_RULES §6) — `bash docs/runtime_port/endgame_b/b39_session/acceptance.sh`

Output on `5cbac19e`, verbatim:

```
== B39 acceptance @ 5cbac19e 2026-09-27 22:27
A1   minted vtable rows (kMintVtables):        0   target 0
A2   minted ?_typeInfo@ rows:                   0   target 0
A3   is UP TO DATE (0 vtables, 0 BaseTypeInfo, 2 storage)   target 0 vtables, 0 BaseTypeInfo, 2 storage
     storage rows: ?_disableHandler@Debug@System@@0_NA ?_singleton@MemoryAllocator@System@@0PEAV12@EA
A4   RT_DELDTOR(0x in runtime/                                      RAW 5     CODE 0     target 0
A5   RegisterExeAddr( 0x140153000-0x14016ffff (code standalone engine) RAW 80    CODE 0     target 0
A5b  any Register*( of a literal in that range (all trees)          RAW -     CODE 0     target 0
A6   0x1401c.... type-id cell (minus 3dd8/3de0/3de8/22d0/23d0)      RAW (broken) CODE 0     target 0
     the five non-type-id cells, code lines: 31
A7   g_tid_ / g_TypeId_ (code engine standalone)                    RAW 0     CODE 0     target 0
A8   factory interpreter files: []   target nothing
A9   "??_7 literal (code runtime standalone engine dx11)            RAW 6     CODE 0     target 0
A10  DoorBody (runtime code)                                        RAW 11    CODE 0     target 0
A11  "?_singleton@ minus MemoryAllocator                            RAW 2     CODE 0     target 0
A12  InvokeAllRegistrations|REGISTER_ABSTRACT_FACTORY|FactoryRegistration RAW 7     CODE 0     target 0
```

Every CODE count meets its target. The RAW counts are comments (history notes naming retired rows, doors and
macros); §6's lines A4, A5, A10, A11 and A12 were written as raw greps and are **redefined as code-only scans**
(stated in B39_RULES §6 and §8) — no code line matches any of them. A6's raw form never worked (§8). The gate part of
§6 (builds, routes, resave ×5, typetable, `[RtResolve] 0 misses`, `calls=5683` / `5773 openFileFail=12`,
`stepclose.sh`) is the coordinator's, after the merge.

## 8. Static gate of every close commit

| commit | what | debug | perf | unit | dispatch | mint `--check` | new warnings |
|---|---|---|---|---|---|---|---|
| e8714fed | manifest + tag file deleted | 0 | 0 | 919 / 0 | 0 mismatches | 0/0/10 | only the 5 known mint warnings |
| 5c31806a | mint vtable / type-info paths deleted; `_firstTypeInfo` retired | 0 | 0 | 919 / 0 | 0 | 0/0/9 | **0** (the 5 are gone) |
| e51b15ee | last storage rows; redirect deleted | 0 | 0 | 914 / 0 | 0 | 0/0/2 | 0 |
| 3cad3c22 | residue sweep (303 `DEFINE_EXE_VTABLE`, ToolUI global, typedefs, literal lookup) | 0 | 0 | 914 / 0 | 0 | 0/0/2 | 0 |
| 5cbac19e | historical tools / READMEs / session kit | — (no code) | — | — | — | — | — |

Warnings: the build logs' distinct `(file, message)` set against the coordinator's clean-rebuild baseline
(128 debug / 291 perf); the close's first build was a full build of a fresh worktree (1,093 files), later builds
incremental. No game was run by the close owner.
