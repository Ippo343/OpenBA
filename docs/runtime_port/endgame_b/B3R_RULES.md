# The registry teardown: the rule sheet

Written 2026-09-28 by the coordinator from the six block-0 inventories in `b3r_session/`
(`INVENTORY_A_RUNTIME_ROWS`, `_B_EXE_ROWS`, `_C_BY_ADDRESS_DATA`, `_D_STORAGE_ALLOCATOR`,
`_E_ENGINE_FUNCS`, `_F_BOOTLOG_GATE_TOOLS`; each has a table in `b3r_session/data/` and the script that
rebuilds it in `b3r_session/scripts/`). Earlier rule sheets still bind (B34B … B3X); a later rule wins.

**The step.** A real class needs no door. Every row that registers one of OUR bodies under a retail name
or a retail address, and every resolve that reads such a row, becomes a direct C++ reference or is
deleted. What is left at the end is the list of things that have no C++ body yet.

## 1. Measured (block 0)

| | |
|---|---|
| Runtime registrations | 2,875 (1,341 by name, 1,534 by address) = 1,819 logical rows: DEAD 1,239, DIRECT 578, STORAGE 2 |
| Runtime DIRECT reader sites | 1,365 in 335 files (`RtAddr<>` 423, `RT_EXPORT_ONCE` 238, `ResolveAddr(Runtime` 204, `ResolveExport` 138, exe import slots 93) |
| Exe addresses | 778 = 760 registered + 18 read but never registered: DIRECT 635, STORAGE 63, DEAD 45, CALLBACK 17, STUB 18 |
| Exe reader occurrences | 2,375 (exe functions 1,961, data 252, import slots 162) |
| Exe DIRECT bodies with no header declaration | 547 of 584 |
| `engine_funcs` | 227 globals + 52 function declarations + 12 inline bodies; DIRECT 214, STORAGE 10, DEAD 3, STUB 1; 4,890 use lines in 460 files (2,152 calls, 1,765 guards) |
| allocator doors | `engine_funcs` family 1,019 lines / 230 files; name / address / exe-slot doors 287 lines / 151 files; 112 per-file wrappers |
| the mint | 2 storage cells, 3 `RT_DATA_SLOT` uses (one dead) |
| `rt_export_addrs.inc` | 4,926 rows, read only inside `Register*` to mirror one door into the other; 16 rows have a reader of the mirrored key |
| data-driven reads | 28 sites; 0 serialized function ids, 0 computed addresses |
| lazy-cache spellings | 12, at 1,335 sites; only the `std::atomic` one is loader-thread safe |
| registry boot lines today | `[RtPort] runtime registry: 1347 exports, 1817 addrs`; `[AddressRegistry] 759`; `[RtMint] registered 2/2` |
| `NOT IMPLEMENTED` set | new game 10 addresses, tutorial 8, Load Game 1 |

NEXT_SESSION §3a corrections: `RegisterExeAddr(` is 698 code lines; `X_ResolveFuncs` has 138 calls;
`RegisterRuntimeAddr` needs a space-tolerant pattern (99 calls are spelled `RegisterRuntimeAddr  (`);
"import slots dominate code/Game" is false for exe rows (162 of 2,375 reads); `RT_DATA_SLOT` has 3 uses.

## 2. Decisions

### R1 — Readers first, rows last
A reader conversion is checked by the compiler and changes nothing while the row still exists. A row
deletion is checked by nothing at run time (INVENTORY_F: a Runtime address row that is deleted and still
read prints NOTHING). So:
- **W0** prerequisites (serial or on disjoint files): the mirror, the mint, one allocator API,
  declarations.
- **W1** reader blocks, parallel, by FILE. A block converts EVERY door spelling in its files — it does
  not own rows or classes. It also deletes the DEAD registration lines that live in its files.
- **AUDIT**, independent, read-only: every remaining resolve classified.
- **W2** row deletion and file deletion, serial.
- **CLOSE**.

### R2 — What a reader becomes
| the row is | the reader becomes |
|---|---|
| DIRECT, target is a member of a real class | `p->Member(…)` / `Class::Static(…)`; a `void*` receiver takes `reinterpret_cast<Class*>(p)  // B.3.x bridge` |
| DIRECT, target is a free function (the `code/Game` C bodies, the Runtime internal bodies) | a call of that function by name, through its declaring header (R5). NEVER a local `extern` prototype in the caller |
| DIRECT, target is a `Collection::Array` instantiation body | the body by name. (Turning it into the template is B.1 decision 3 and is NOT this step.) |
| STORAGE | the class static / namespace-scope object / `constexpr`, by name (R7) |
| STUB | R4 |
| CALLBACK | R8 |

- **The door decides the overload, never the pointer's name or the argument's type.** Resolve what the
  reader is BOUND to (the address or mangled name in its initialiser) and look that key up in the
  inventory tables. 19 `engine_funcs` globals and two slot readers are misnamed (INVENTORY_E §5.8,
  INVENTORY_B surprise 5).
- **A readiness guard on a DIRECT row is deleted with the conversion**: `if (fn_X) fn_X(a)` becomes
  `X(a)`; `if (!fn_X) return;` is deleted; an `else` arm of such a guard is dead code and is deleted.
  A null check on an OBJECT that retail has stays. 1,765 guard lines are of this kind.
- **Guard-only doors** (`fn_InternalBasePtr_Ctor` 393, `fn_InternalBasePtr_DeletePtr` 38: tested,
  never called): the test is deleted, the guarded statement stays.
- **The cache dies with its reader.** No replacement cache, static or atomic, is introduced: a direct
  call has none. `X_ResolveFuncs()` / `_InitFuncs()` / `Ensure*()` definitions that resolve nothing any
  more are deleted with their calls.
- **By-value ABI.** A door typedef that carries an explicit hidden return slot, or passes
  `System::String` / `UI::String` / `InternalPtr<T>` / a class of 16 bytes or more by pointer, does not
  convert by renaming: the direct call uses the MEMBER's declared signature and the compiler passes the
  slot. Build the temporary the door's caller built. `ExternalPtr<T>` by value is a bare pointer.
- **A reader whose argument list disagrees with the body's** (INVENTORY_E §5.6: `fn_Map_Invalidate`
  1 vs 4, `fn_Map_CalcPathDist` 3 vs 4; anything else the compiler finds): settle from retail's call
  site in Ghidra. It is a BEHAVIOUR commit of its own (title `behaviour:`), with the address. When it
  cannot be settled the site is LEFT and listed — never guessed, never padded with a zero.
- No behaviour change is expected from a W1 block outside the titled commits of §5.

### R3 — DEAD rows
A row with no reader by name AND by address (code lines; the inventory tables say which) is deleted
with its registration line. A register function left empty is deleted with its call in
`RuntimePortInit()` / `AddressRegistryInit()` (those two files are the coordinator's: list the call,
the closing owner removes it). A body that loses its last reference is NOT deleted in W1 unless it is
`static` and the compiler says it is unused: bodies are provenance, and an unreferenced exported body
is a question for the close.
Dead READERS (constants, statics, helper definitions with no caller) are deleted by the block that
owns the file: 91 unused `kX = "<mangled>"` constants, the `XAddr` constant namespaces (262 of 264
unused), `RegisterExeAddrs`, `ReadGlobal` / `WriteGlobal`, the six caller-less wrappers.

### R4 — STUB: the null stays, the resolver goes
A reader of a key that resolves to NOTHING today (the 18 unregistered exe addresses; the 7 unfilled
import slots `Box` ctor, `DeletePtr`, `ExternalList::Find`, `Factory::CreateObject`, `Particle::_rng`,
`Vector3::Lerp`, `Vector3::operator==`; `fn_Entity_FindAudioComponent` 0x140002280) keeps the behaviour
it has today: the code takes the branch it takes on null. It is written without a resolver:
```cpp
// PORT GAP 0x14009e8c0 <name>: not ported; retail calls it here.
```
followed by the null arm's statements. When the null arm is a fallback that computes the same thing
locally (the two LUTs 0x1401c22d0 / 0x1401c23d0), the fallback becomes the code and the marker says so.
- **Never wire a body into a STUB reader in a W1 block.** A reader that starts calling a real body is
  a behaviour change: a titled commit of §5, after reading the consumer (`address-registry.md`: some
  slots are null on purpose).
- `gatherrawmaterials.cpp` reads 0x1401527e8 as MemAlloc and 0x1401527f0 as MemFree; the slots are
  `Particle::_rng` and `Vector3::Lerp`. The readers are deleted; the fallback they fall into is the code.
- 0x140089470 is registered only under `BANISHED_LIVE_SEAT_SHUFFLE`: the option is tested at the call
  site (`Opt().field`), the row goes.
- Each `NOT IMPLEMENTED` address that leaves the boot log this way is recorded (R10).

### R5 — Declarations
Every DIRECT body is declared in exactly one header, written from its DEFINITION (linkage included:
321 of the registering files' prototypes are `extern "C"`), and the defining `.cpp` includes that
header so the compiler checks the pair.
- `code/Game` bodies: `code/Game/<Module>/exe_bodies.h`, grouped by defining file, the retail address
  as a comment on each line. (These are the C bodies. Making them members is B.4's work; the header
  is where that work will find them.)
- Runtime bodies with no declaration (51 C-door functions, the internal bodies): the existing
  `runtime/funcs/` or `runtime/types/<ns>/` header of their namespace.
- `engine_funcs.h`'s 52 function declarations, 12 inline bodies, the `extern "C"` line and the forward
  declarations MOVE to the header of the module that defines them; `engine_funcs.h` includes the new
  headers until EZ deletes it.
- Two globals bound to one address are one declaration. Two BODIES for one retail function
  (0x14012e5c0; `InterpolatedState<Transform>::Lerp` 0x18001aff0): the body that is live today stays
  live — the one registered LAST. The other is listed for the close.

### R6 — The mirror, the mint, the generator
- **The mirror goes first and whole** (INVENTORY_C §6 C8): the 16 keys whose only registration is the
  mirrored one are registered explicitly in `standalone/reg_rt_mirror16.cpp`; then the mirror half of
  `resolve_standalone.cpp`, `standalone/rt_export_addrs.inc` and the generator's `write_export_addrs`
  are deleted. `[RtPort] runtime registry` falls by 28 exports / 273 addresses: a re-cut (R10).
- `rt_mint_gen.py --check` must not write any file.
- **The mint** goes in INVENTORY_D's three commits: M1 `System::MemoryAllocator::_singleton` a class
  static (the writer and the four by-address readers in the same commit; the dead `EngineAddr`
  constants deleted so the generator does not refuse), M2 `System::Debug::_disableHandler`, M3 the mint
  files, `RT_DATA_SLOT`, the generator's storage half, the test that asserts `storageRows > 0`.
  The retirement mechanism (`tools/rt_mint_retired/`) is used for M1 and M2 and deleted by M3.
- After M3 the generator has nothing to generate: `tools/rt_mint_gen.py` is deleted with its gate line.

### R7 — Storage
- `.rdata` constants (67 rows): a `constexpr` in the P0 header of the module that reads it, VALUE
  copied from the registering file (which was verified against the image). Five have two cells with
  the same value: one constant. `p ? *p : fallback` becomes the constant.
- Singleton cells and exrefs (`g_pGameWorldSingleton`, `g_pMainLoopSingleton`, the settings cells, the
  `Input::Configuration` / KeyMap triple-deref chain): the class static or the named object the cell
  points at. The chain of `address_registry.cpp` (`g_StandaloneKeyMapMid`) stays a named object until
  its readers read `KeyMap` directly.
- `g_pMemAllocSingleton` is a snapshot that is NULL until `RunFactorySelfTest` refreshes it
  (INVENTORY_D §6.1). Readers read `System::MemoryAllocator::_singleton` LIVE. Removing the refresh is
  a titled commit (§5): the value every reader sees on the gate routes is the same, the boot no longer
  depends on a self-test.

### R8 — CALLBACK
- The citizen-AI state switch (14 handlers, all registered) and the tutorial `legs[3]` table: direct
  function references in the table or switch.
- Dispatch slots (51 cells, 93 rows): a slot cell dies when its last reader is converted. No reader is
  converted to read a cell by another name. `tools/dispatch_slot_map.py --check` stays at 0 mismatches
  until the last slot row goes, then leaves the gate with it.
- dx11's two import structs (`Imports`, `RuntimeDeps`): direct references; `Dx11ResolveRuntimeImport`
  is deleted. The 84-slot `*Data` seam is NOT touched (B5-8, carry-over 6).
- The four `DEFINE_EXE_VTABLE` lines keep no row and are not part of this step.

### R9 — The allocator
One API: `System::MemoryAllocator::MemAlloc / MemFree / Allocate / Free` as members of the real class
(M1). `Object::detail::InternalMemAlloc / InternalMemFree`, `DoorAlloc / DoorFree`, the root and
in-class `operator delete`s, the 112 per-file wrappers and the `engine_funcs` family
(`fn_MemAlloc(g_pMemAllocSingleton, n, a)` …) call it directly.
- **`BANISHED_REDZONE` and `BANISHED_ALLOC_STATS` keep working.** They are existing instruments that
  today swap door pointers and slot cells. They are re-seated on ONE named hook pair owned by the
  allocator (`System::MemoryAllocator`'s translation unit; null unless armed at boot from `Options`),
  consulted at the entry of `MemAlloc` and `MemFree`. No new flag. This is the one place where our
  body has a branch retail's does not; the comment says so (STANDALONE DEVIATION, instrument seat).
- Consequence, titled: `BANISHED_ALLOC_STATS` starts seeing frees (it saw none: INVENTORY_D §6.2).
- The redzone arming order (before the first allocation it must cover) is kept.
- `fn_MemAlloc` is bound to `?Allocate@` → the anonymous `SwapAllocate`; after A0 the name is
  `MemoryAllocator::Allocate`.
- 93 of the 94 `operator new` / `operator delete` COMDAT rows have no reader: DEAD (R3).

### R10 — The boot log and the gate
**Invariant on every merge:** builds exit 0, 0 new warnings; unit 0 failures and a count that only
changes in writing; new game `calls=5683 openFileFail=0`, tutorial `5773/12`; typetable 428 lines md5
`b983f144`; resave N/N == golden `dd3893af`; `[RtUnresolved]` 0; no NEW address in a route's
`NOT IMPLEMENTED` set; the -O2 new-game route clean.
**Re-cut, only allowed to FALL, written down in `b3r_session/GATE_TABLE.md` on the merge that moves
it:** `[RtPort] runtime registry: E exports, A addrs`; `[AddressRegistry] N`; `[RtMint]` lines (gone at
M3); the `NOT IMPLEMENTED` set per route, by address, each address that leaves with its disposition:
(a) its reader was dead, (b) its reader became the PORT GAP null arm of R4, (c) it got a body (titled).
**Leaves the gate with its printer:** `[RtResolve] 0 misses` (the commit that deletes
`runtime/funcs/rt_resolve_cache.cpp` creates `b3r_session/RESOLVE_CACHE_GONE`), `mintgen`, `dispatch`.
The kit is `b3r_session/` (`gate.sh` compares the stub set by address and counts `[RtUnresolved]`;
the `[RtMint] TRAP` / `GATE WARN` checks, which nothing could trip, are gone).

### R11 — Tests and harnesses
- Three unit tests answer through `g_unitResolveAddr` (`alloc_door_test` x2, `array_test` x1): they
  are re-seated by the block that owns the product file, on the named hook (R9) or a direct call.
  `tests/unit/rt_port_stub.cpp` goes when the last of them is re-seated.
- The `InitFields` / `ReleaseFields` seams (carry-over 17) are link seams: untouched.
- Env-gated harness and diagnostic code converts like any other reader. 29 harness rows run on no gate
  route: they are compile-checked only, and the hand-in says so per file.
- Instruments that inspect the REGISTRY ITSELF (`dlgctrl_route.cpp`, `scene_watch.cpp`'s
  `RtPortLookupAddr` probe, `[UnregCensus]`, `TraceExeBegin/End`) go when the registry goes (W2).

## 3. Standing rules (unchanged)
Retail wins. Never "fix" a faithful crash with a null check. Never `getenv`. No new flag or instrument.
No `Register*()` inside a `printf`. Generated files are regenerated, never hand-edited. Nothing reads a
vtable by literal index. Deletion is `delete p`. Find readers by BINDING (`BLOCK0_BRIEF.md` lists the
spellings; INVENTORY_C adds `if (!x) x = …`, `Ensure*`, guards that set the flag before filling, and a
static initialiser that registers). Count CODE lines (`b3r_session/codegrep.py`).
`IsDerivedFrom` on a possibly-null receiver: `fn_IsDerivedFrom` is a null-safe trampoline today (a
documented standalone deviation). Its readers call `Object::IsDerivedFromNullSafe(p, id)` (one inline
helper, declared by P0 beside `Object::Base`), so no site changes behaviour. Removing the deviation is
not this step.

## 4. Blocks and file sets

Blocks own FILES. A file named in a W0 block is RESERVED: no other block edits it before that block
has merged. The coordinator's files, always: `standalone/resolve.h`, `resolve_standalone.cpp` (outside
W0-SM), `address_registry.cpp` / `.h`, `standalone/main.cpp` (outside W0-SM), the `Makefile`,
`docs/`, the kit.

### W0 (prerequisites)
| block | files | content |
|---|---|---|
| **W0-SM** mirror + mint (one owner, serial commits) | `standalone/resolve_standalone.cpp`, `rt_export_addrs.inc`, new `reg_rt_mirror16.cpp`, `rt_data_mint.*`, `rt_data_mint_tables.inc`, `tools/rt_mint_gen.py`, `tools/rt_mint_retired/`, `tools/addr_door_audit.py`, `tools/runtime_census.py`, `tools/cutover_closure/build_roots.py`, `runtime/funcs/rt_port.h`, `rt_resolve_cache.cpp`, `runtime/types/system/MemoryAllocator.h`, new `Debug.h`, `runtime/types/common.h`, `runtime/types/engine/Engine.h`, `runtime/src/system/allocator.cpp`, `allocator_win.cpp`, new `debug.cpp`, `runtime/src/engine/singleton_ctors.cpp`, `engine_lifecycle.cpp`, `runtime/src/video/enumerate_video_modes.cpp`, `standalone/alloc_cutover_arm.cpp`, `standalone/main.cpp`, `code/Game/Game/game_main.cpp`, `engine/funcs/engine_funcs.cpp` (the singleton line only), `tests/unit/b39_obj_typeinfo_unique_test.cpp` | R6: MIRROR, M1, M2, M3 |
| **W0-A0** one allocator API (after W0-SM merges) | `runtime/src/object/internal_class.cpp`, `root_operator_delete.cpp`, `class_operator_delete.cpp`, `runtime/src/ui/ui_operator_delete.cpp`, `runtime/src/collection/alloc_door.cpp`, `runtime/src/system/allocator.cpp`, `allocator_win.cpp`, `closure_batch2b.cpp`, `standalone/redzone_alloc.*`, `alloc_cutover_arm.cpp`, `alloc_stats.cpp`, `tests/unit/alloc_door_test.cpp`, `tests/unit/rt_port_stub.cpp` | R9 |
| **W0-P0a** declarations | NEW `code/Game/{Action,Work}/exe_bodies.h` + one `#include` line in each defining `.cpp` there | R5 |
| **W0-P0b** declarations | NEW `code/Game/Component/exe_bodies.h` + the include lines | R5 |
| **W0-P0c** declarations | NEW `code/Game/{Game,Player,Dialog,Tutorial,Entity,Goal}/exe_bodies.h` + the include lines; `engine/funcs/engine_funcs.h` (move declarations and inline bodies out; it includes the new headers); the 11 `engine/types` headers with no includer (deleted); the Runtime declarations missing from `runtime/funcs/` and `runtime/types/`; `Object::IsDerivedFromNullSafe`; the `.rdata` `constexpr`s (R7) | R5, R7 |

### W1 (reader blocks; each also deletes the DEAD registration lines in its files)
| block | files |
|---|---|
| **G-CompA** | `code/Game/Component/citizen*` |
| **G-CompB** | `code/Game/Component/[a-f]*` except `citizen*` |
| **G-CompC** | `code/Game/Component/[g-z]*` |
| **G-Work** | `code/Game/Work/` |
| **G-Action** | `code/Game/Action/` |
| **G-Dialog** | `code/Game/Dialog/` |
| **G-Game** | `code/Game/Game/`, `Entity/`, `Goal/`, and any other `code/Game` directory not named here |
| **G-Player** | `code/Game/Player/`, `Tutorial/` |
| **S-Harness** | `standalone/` except the coordinator's files, the `reg_*.cpp` files and W0's files |
| **RT-UI** | `runtime/src/ui/` |
| **RT-VG** | `runtime/src/video/`, `graphics/` |
| **RT-IO** | `runtime/src/io/`, `save/` |
| **RT-Core** | the other `runtime/src/` directories, `runtime/types/`, `runtime/funcs/` |
| **DX** | `dx11/` (imports only; `devlost.sh` on its merge) |

A block that needs a declaration P0 did not write adds it to the P0 header of the DEFINING module and
says so in its hand-in (the one permitted edit outside its file set; merge conflicts there are
append-only).

### AUDIT, W2, CLOSE
| block | content |
|---|---|
| **AUDIT** (read-only, independent of every W1 owner) | re-run the six inventory scripts on the merged tree; every remaining resolve / registration classified STUB, CALLBACK, LEFT-with-reason or MISSED; a MISSED list per block |
| **W2-RT** | the Runtime registration lines and register functions, `RuntimePortInit()`, `reg_rt_mirror16.cpp` |
| **W2-EXE** | `standalone/reg_*.cpp`, `address_registry.cpp`, the `code/Game` Register functions, the dispatch slots, `tools/dispatch_slot_map.py` |
| **W2-EZ** | `engine/funcs/engine_funcs.{h,cpp}`, `EngineFuncsInit()`, the remaining `engine/types` aliases |
| **W2-RES** | `standalone/resolve.h`, `resolve_standalone.cpp`, `rt_resolve_cache.cpp`, the registry instruments (R11), the 150 per-file `enum class Module` copies |
| **CLOSE** | acceptance (`b3r_session/acceptance.sh`, INVENTORY_F §6e R1-R17), numbers, step close, the next plan |

## 5. Behaviour commits (each titled `behaviour:`, gated with resave x5)
1. R9: `BANISHED_ALLOC_STATS` sees frees; the instruments' seat moves to the named hook.
2. R7: `g_pMemAllocSingleton` read live; the self-test refresh removed.
3. R2: `fn_Map_Invalidate` and `fn_Map_CalcPathDist` argument lists, from retail.
4. R4: any STUB that gets a body (`Entity::FindAudioComponent` 0x140002280, the 7 unfilled slots, the
   two PORT GAP vtable slots whose bodies are already ported — CitizenAI [6], Picking [11]).
5. R5: the losing body of a doubly-registered address, if it is deleted.
None of 3-5 is required by the acceptance; each is done only with retail evidence, else listed as left.

## 6. Acceptance (NEXT_SESSION §3f; `b3r_session/acceptance.sh`)
Every row classified; 0 DEAD rows; 0 readers of a DIRECT row; the mint deleted; `engine_funcs` deleted
or down to the STUB list; what remains of the registry is listed BY NAME with its reason; the gate
passes; `stepclose.sh`.

## 7. Run log
The table is `b3r_session/GATE_TABLE.md` (one row per gated merge).
- Block 0: six read-only inventories (opus), the rule sheet, the kit.
- The seven items owed from the cutover: one branch, merged first.
- W0: P0b, P0a, SM, P0c, A0 — five gated merges, 0 problems; two re-cuts of the `[RtPort]` line
  (1347 / 1817 -> 1338 / 1537 -> 1247 / 1444), unit 919 -> 918.

## 8. Corrections
(dated, as the blocks find them)

1. **2026-09-28, W0-A0 — R9 was wrong about `fn_MemAlloc`.** R9 says "after A0 the name is
   `MemoryAllocator::Allocate`". The global was bound to `?Allocate@`, but the body registered under
   that name was `SwapAllocate`, which takes the allocator LOCK; the member `Allocate` is retail's
   raw, unlocked body. `fn_MemAlloc(g_pMemAllocSingleton, n, a)` becomes
   `System::MemoryAllocator::MemAlloc(n, a)`; `fn_MemFree` / `fn_MemFreeStatic` become
   `MemoryAllocator::MemFree(p)`. (`W1_BRIEF.md` carries the corrected rule.)
2. **2026-09-28, W0-SM — the mirror kept 7 keys alive, not 16** (INVENTORY_C's regex missed
   registrations written as split string literals; eight of its keys had no bound reader; it missed
   0x1800fdb20 `AnimationState::Update`). `[RtPort]` fell by 9 / 280, not 28 / 273. The file keeps
   the name `reg_rt_mirror16.cpp`.
3. **2026-09-28, W0-SM — `Debug::_disableHandler`'s row left with the mint (M3), not in M2**: it was
   the mint's last row, and retiring it alone made the generator emit an empty array. The mint
   registered AFTER the `[RtPort]` count was printed, so M3 did not move that line.
4. **2026-09-28 — `code/Game/Component` is FOUR reader blocks** (433 files with headers): G-CompA
   `citizen*`, G-CompB `a`-`f` except `citizen*`, G-CompC `g`-`o`, G-CompD `p`-`z`, all
   case-insensitive. §4's three-way split is superseded.
5. **2026-09-28 — a STORAGE block sits between W1 and the AUDIT.** R7's class statics do not exist
   yet and the cells are file-statics of `address_registry.cpp`. W1 blocks LEAVE every read of an
   `engine_funcs` STORAGE global (`g_pGameWorldSingleton`, `g_pGameSettingsPtr`, the singleton cells
   and exrefs) and its pointer test, and count them in the hand-in. A file-local resolve of a cell
   that such a global is bound to (0x1401c3dd8, 0x1401c3de0 …) is rewritten to read the global
   (same cell, same number of dereferences), so the STORAGE block converts one name per cell.
   The `.rdata` constants are not part of this: they convert in W1.
6. **2026-09-28, W0-P0c — counts.** The shared StringTable serialize body serves 5 exe addresses
   (INVENTORY_E said 3). INVENTORY_A's "212 already declared" counted the class MEMBER; the C-door
   functions had 4 declarations, so P0c added 280. 30 bodies have internal linkage
   (`data/P0c_declared.tsv`): the W1 block that owns the DEFINING file gives them external linkage.
   The `.rdata` "floats" include one double (0x140172168) and one 64-bit mask (0x1401722b0).
