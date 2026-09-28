# Inventory D — STORAGE_ALLOCATOR: what it takes to delete the mint

Block 0 of the registry teardown (NEXT_SESSION.md §3b question 4, plus the other storage-like rows).
Read-only. Table: `data/D_storage_allocator.tsv` (171 rows). Script: `scripts/D_storage_allocator.py`.

## 1. The answer

1. The mint is **two cells** and **three `RT_DATA_SLOT` call sites** (the "5" of §3a is 3 uses + the
   `#define` + one diagnostic string). One of the three (`SlotMemoryAllocator()`, `singleton_ctors.cpp:158`)
   is **defined and never called**.
2. `?_singleton@MemoryAllocator@System@@` has ONE writer (`rtalloc::Construct/Destruct`, reached from the
   Engine ctor/dtor through the `??0/??1MemoryAllocator` name doors) and **five direct readers**: 2 by name
   (`RT_DATA_SLOT`, one dead) and 3 by address (`main.cpp:563/684`, `game_main.cpp:482`,
   `engine_funcs.cpp:611`), plus one dead constant (`Engine.h:147`). Everything else reads a CACHED COPY,
   `g_pMemAllocSingleton` (463 code lines, 11 dirs), or never reads the cell at all (the statics find it
   through `Get()`).
3. `?_disableHandler@Debug@System@@` has ONE writer and ZERO readers of its value
   (`enumerate_video_modes.cpp:144/152` sets 1/0; retail's only reader, `Debug::ExceptionHandler`
   0x1800c5b6f, is unported). The minimum port is a `System::Debug` class holding `static bool
   _disableHandler` — no function of Debug needs porting.
4. Deleting the mint therefore costs **one ~11-file commit** (MemoryAllocator becomes a class with
   `static MemoryAllocator* _singleton`, writer and the 3 by-address readers converted together), **one
   3-file commit** (Debug), and **one deletion commit** (mint files, `RT_DATA_SLOT`, the generator's mint
   half, one unit test, 3 tools). None of the three changes behaviour.
5. The allocator DOORS are the large part and are independent of the mint: 1,019 lines / 230 files of the
   `engine_funcs` family (`fn_MemAlloc(g_pMemAllocSingleton,…)`, `fn_MemFreeStatic`, …), 287 lines /
   151 files of by-name / by-address / exe-slot doors, 554 lines / 428 files naming
   `Object::detail::InternalMem{Alloc,Free}` (which convert by changing TWO function bodies, not 554
   lines), 112 per-file wrapper definitions, and 88 + 5 operator new/delete rows that have **0 readers**.
6. Two of the doors carry DIAGNOSTICS, not behaviour, and that is the one decision the rule sheet must make:
   `BANISHED_REDZONE` rewrites `fn_MemAlloc` / `fn_MemFreeStatic` and the exe slots 0x140152b60/b68, and the
   composed `SwapAllocate` feeds `BANISHED_ALLOC_STATS`. A direct call bypasses both.

## 2. How the table was built

`python docs/runtime_port/endgame_b/b3r_session/scripts/D_storage_allocator.py` from the repo root, no
arguments, ~50 s. It loads every `.cpp/.h/.hpp/.inc/.c` under `standalone runtime engine code dx11
tests/unit`, strips comments with `b3x_session/codegrep.py`'s `strip()` (strings kept), excludes
codegrep's `EXCL` (build trees, worktrees), and matches per line. The two GENERATED tables
(`rt_data_mint_tables.inc`, `rt_export_addrs.inc`) are excluded from reader counts.

Columns: `key` (name, address, spelling or `file:line`); `kind` (the lead's seven); `storage_file` /
`storage_line` (where the cell, table or registration lives); `writers`; `readers` (count, per-directory
split, first eight `file:line`); `reader_files` (first eight files); `becomes` — **prefixed with the row
class** (`DEAD`, `DIRECT`, `STORAGE`, `DEAD-after-DIRECT` = a registration that dies when its readers are
direct); `behaviour_change`.

Row groups: (1) the 2 minted cells; (2) every `RT_DATA_SLOT` line and every mint-machinery line;
(3) 13 allocator-door SPELLINGS, one row each with a per-directory count and, for the mangled-literal rows,
a per-BINDING split (`RT_EXPORT_ONCE` / `ResolveExport` / `AtomicExport` / `RtResolveCached` / `const char*`
table / `{` table / `Exp<` / `Dx11ResolveRuntimeImport`); the per-file wrapper definitions; the two ??3
tables and six closure-batch-2b rows with name + address readers; every allocator registration with its
own readers; (4) every `RegisterExeAddr` row at or above 0x140151000 (46) plus the 25 table-driven
`.rdata` floats; the ten `engine_funcs` data externs; seven named one-offs.

## 3. Counts

Row class x storage directory (`-` = a spelling row, which spans directories):

| class | runtime | standalone | code | engine | tests | - | total |
|---|---:|---:|---:|---:|---:|---:|---:|
| DEAD | 34 | 16 | | | 2 | 2 | 54 |
| DEAD-after-DIRECT | 10 | 2 | | | | | 12 |
| DIRECT | 3 | | | | | 16 | 19 (+1 open) |
| STORAGE | | 68 | 5 | 10 | | 2 | 85 |

Allocator spellings, CODE lines (per directory in the table):

| spelling | lines | main directories |
|---|---:|---|
| `fn_MemAlloc` (member `Allocate` door 0x1800c6e60 = composed `SwapAllocate`) | 492 | code/Game 474, standalone 10, engine/funcs 8 |
| `g_pMemAllocSingleton` (cached copy of the cell) | 463 | code/Game 430, standalone 24, engine/funcs 9 |
| `fn_MemFreeStatic` (`?MemFree@` by name) | 466 | code/Game 448 |
| `fn_MemAllocStatic` / `fn_MemFree` lambda | 34 / 8 | code/Game |
| literal `?MemAlloc@…` (RT_EXPORT_ONCE 19, ResolveExport 18, tables 3, dx11 3) | 43 | runtime 21, code 14, dx11 4 |
| literal `?MemFree@…` (RT_EXPORT_ONCE 22, ResolveExport 18, AtomicExport 13, tables 5, RtResolveCached 2) | 64 | runtime/src/ui 25, runtime other 19, code 11, dx11 5 |
| exe slots 0x140152b60/b68 (`ResolveAddr(Exe)`, `RSlot`, `R<>`) | 38 | code/Game/Work 15, Player 7, Component 6 |
| Runtime 0x1800c7a50/7ad0/6e60/6c00 by address (`RtAddr<>`, `ResolveAddr(Runtime)`) | 68 | runtime/src/ui 27, video 10, io 8 |
| 0x180009c30 (2-argument free) | 5 | runtime |
| `rtcoll::MemAlloc/MemFree/DoorAlloc/DoorFree` | 69 | runtime 55, tests 12 |
| `Object::detail::InternalMem{Alloc,Free}` | 554 | code/Game 314, runtime/src/ui 65, runtime/types 68, tests 46 |
| per-file wrapper DEFINITIONS (`void* MemAlloc(…) { door }`) | 112 | runtime 90, code 13, dx11 1 |

The 289 `MemoryAllocator` code lines: 105 door resolves of `?MemAlloc@`/`?MemFree@`, 71 type uses
(parameters, members, declarations), 44 string-bearing lines (static_assert messages, printf,
`saveload_update.cpp`'s `out.Call("System::MemoryAllocator::…")` labels), 32 generated export-address rows,
30 registrations, 4 door resolves of ctor/dtor/Startup/Shutdown (`engine_lifecycle.cpp:410/465/511/953`),
2 `_singleton` reads (`RT_DATA_SLOT`), 1 generated mint row. No line calls a static MEMBER: there is no
class to call — see §5.1.

## 4. The questions, answered

### 4.1 `?_singleton@MemoryAllocator@System@@` 0x1801fc718

- **Storage**: `kStorageBytes[16]` in `rt_data_mint_tables.inc:13`, registered by name and address by
  `RtDataMintInit()` (`resolve_standalone.cpp:384`, the last step of `RuntimePortInit`).
- **Writer**: `rtalloc::Construct` / `Destruct` (`runtime/src/system/allocator.cpp:654/659`) through
  `SingletonSlot()` (`:664`, `RT_DATA_SLOT`). Reached ONLY from `RtEngine_Engine_Ctor` / dtor via
  `RT_EXPORT_ONCE("??0/??1MemoryAllocator@System@@…")` (`engine_lifecycle.cpp:410/465`) → `Ctor_/Dtor_
  MemoryAllocator` (`singleton_ctors.cpp:245/249`) → `rtalloc::Construct/Destruct`. The object is
  `Engine::_allocator` at Engine+0; the Engine is `EngineStorage engine` on `RunGameMain`'s stack
  (`game_main.cpp:382`), constructed through the `??0Engine@0@QEAA@PEAX@Z` door.
- **Readers**: `rtalloc::Get()` (every `MemAlloc`/`MemFree` static, through the cached `RT_DATA_SLOT`);
  by address `main.cpp:563` (crash-log diagnostic), `main.cpp:684` (the self-test refresh), `game_main.cpp:
  482`, `engine_funcs.cpp:611` (initialiser of `g_pMemAllocSingleton`); `RtEngine::EngineAddr::
  AllocatorSingleton` (`Engine.h:147`, 0 readers — the whole `EngineAddr` namespace has 0 readers);
  `SlotMemoryAllocator()` (0 callers). No exe import slot names it; `rt_export_addrs.inc:4908` is generated.
- **What the class is today**: `rt::system::MemoryAllocator`, a POD with `static_assert`s
  (`runtime/types/system/MemoryAllocator.h:227`), and C bodies `rtalloc::*` in `allocator.cpp` /
  `allocator_win.cpp` with `self` explicit. It is the ONLY `runtime/types/system/*.h` type still outside
  `namespace System` (Compression, Configuration, Crc, Error, Filename, String, Thread are all `System::`).
  `runtime/types/common.h:39` forward-declares a GLOBAL `struct MemoryAllocator`.
- **What is needed**: `namespace System { class MemoryAllocator { … static MemoryAllocator* _singleton;
  static void* MemAlloc(uint64_t, int); static void MemFree(void*); static MemoryAllocator* Get();
  MemoryAllocator(); ~MemoryAllocator(); bool Startup(uint64_t, uint64_t); void Shutdown(); void*
  Allocate(uint64_t, int); void Free(void*); … }; }` with the 23 `rtalloc` bodies as members (the
  layout asserts stay). `Engine::_allocator` is then constructed by the compiler or by a direct
  `new (&_allocator) System::MemoryAllocator()` — the four `RT_EXPORT_ONCE` sites in `engine_lifecycle.cpp`
  become direct calls. `Get()` returns `_singleton`.
- **Boot order**: `_singleton` is a pointer with NO initialiser, i.e. constant-initialised zero before any
  dynamic initialiser runs — exactly today's BSS cell; no static-init-order hazard. It is non-null only
  between the Engine ctor and dtor, as in retail; anything that allocates before that dies on
  `EnterCriticalSection(0x640)`, as in retail (B34B 6.3's "LockEnter on a NULL lock" diagnosis in
  `main.cpp:556` reads this cell). The platform seam (`g_plat`) must be installed before the Engine ctor
  (`RtLaneXEnsureAllocatorPlatform`, `engine_lifecycle.cpp:1007`) — unchanged. `redzone_alloc.cpp` and
  `alloc_stats.cpp` never read the cell directly: redzone reads `g_pMemAllocSingleton` (9 lines: its heap
  walks treat it as the allocator object), alloc_stats reads nothing.
  `alloc_cutover_arm.cpp` takes `self` as a parameter. The one ordering trap: the four by-address readers
  read the MINT cell, so the writer and those four move in the SAME commit (a split leaves them reading a
  cell nobody writes).

### 4.2 `?_disableHandler@Debug@System@@` 0x1801fc708

System::Debug exports (`rt_export_addrs.inc`): `??4Debug@` 0x18007c0a0, `DisableExceptions` 0x1800c5870,
`SetExceptionCallback` 0x1800c5880, `DefaultExceptionHandler` 0x1800c5890, `ExceptionHandler` 0x1800c5b60,
`Shutdown` 0x1800c5c30, `Startup` 0x1800c5c50, `MsgBox` 0x1800c5c90, `ForceDump` 0x1800c5f00,
`GetDumpPath` 0x1800c5f10; data `_exceptionCallback` 0x1801fc700, `_disableHandler` 0x1801fc708,
`_exceptionHandler` 0x1801fc710. **Registered: none. Read in code: none** (names and addresses both
searched). Ghidra (read-only): 0x1801fc708 is READ only by `ExceptionHandler` (0x1800c5b6f) and WRITTEN by
`ExceptionHandler` ×2, `DisableExceptions`, `EnumerateVideoModes` ×2; `Debug::Startup` has no in-DLL
code caller (the exe imports it; our boot never calls it — the standalone installs its own VEH).
**Minimum port**: `runtime/types/system/Debug.h` declaring `class System::Debug` with the three data statics
(the functions declared, undefined — STUB, nothing calls them) and one definition `bool
System::Debug::_disableHandler = false;`. `enumerate_video_modes.cpp:108-111` then takes
`&System::Debug::_disableHandler`, and its two `if (disableHandler)` tests become dead (a pointer to a
static is never null) — delete them in the same commit; not a behaviour change.

### 4.3 Everything that reads the mint

| site | becomes |
|---|---|
| `runtime/funcs/rt_port.h:144` `#define RT_DATA_SLOT` | deleted |
| `runtime/funcs/rt_resolve_cache.cpp:268` string naming it | reworded |
| `runtime/src/engine/singleton_ctors.cpp:158` `SlotMemoryAllocator()` | deleted (0 callers) |
| `runtime/src/system/allocator.cpp:665` `SingletonSlot()` | `&MemoryAllocator::_singleton` (or deleted) |
| `runtime/src/video/enumerate_video_modes.cpp:111` | `&System::Debug::_disableHandler` |
| `standalone/resolve_standalone.cpp:14/384` include + `RtDataMintInit()` | deleted |
| `standalone/rt_data_mint.{h,cpp}`, `rt_data_mint_tables.inc` | deleted |
| `[RtMint] registered 2 by name, 2 by address` / `[RtMint] minted 2 storage symbols (32 bytes)…` | gone: re-cut in writing; `[RtPort] runtime registry: N exports, M addrs` drops by 2 / 2 |
| `tests/unit/b39_obj_typeinfo_unique_test.cpp:81-115` `b39_close_mint_carries_no_typeinfo_and_no_vtable` | deleted with the file (it `CHECK(s.storageRows > 0)`: it FAILS the moment the table empties) — unit count 919 → 918 |
| `tools/rt_mint_gen.py` | keep `write_export_addrs` (it also generates `rt_export_addrs.inc`) and the `tools/rt_mint_retired/` refusal; delete the storage emitter |
| `tools/addr_door_audit.py:161`, `tools/runtime_census.py:1831/1906/2071/2939`, `tools/cutover_closure/build_roots.py:68` | open the deleted file: guard or delete those paths |
| `b3x_session/finishmerge.sh:11/19`, `mergegate.sh:19/29/42` | `git add -A standalone/rt_data_mint_tables.inc` fails on a missing path: edit before the deletion merges |
| `[RtMint] TRAP` greps in `gate.sh:111`, `stepclose.sh:45`, `uiroutes.sh:32`, `devlost.sh:20`, `mods_route.sh:28` | **nothing prints that line any more** (the TRAP machinery went with the minted vtables at B.3.9): a check that always reads 0 — delete it |

There is no `MintStorageCell` and no TRAP code left in the tree.

**The generator's order matters.** `rt_mint_gen.py`'s population unions in "every exported
`?_singleton@`" (line 775), so the MemoryAllocator row does NOT leave by losing its readers: it must be
RETIRED (a `tools/rt_mint_retired/*.txt` line), and the retirement is REFUSED while any comment-stripped
code line holds `?_singleton@MemoryAllocator…` or a hex literal inside 0x1801fc718..0x1801fc720 —
including the dead `Engine.h:147` constant. `_disableHandler` enters only through the source scan
(`DATA_LITERAL`, line 448) and leaves by itself once `enumerate_video_modes.cpp:111` is gone. When both
are gone the generator would emit `static RtMintStorage kMintStorage[] = { };` — a zero-length array,
ill-formed C++ — so **the last row and the mint files go in one commit.**

### 4.4 `Object::detail::InternalMemFree` and the other allocation doors

- **Definitions**: `runtime/src/object/internal_class.cpp:133/137` (product: `RT_EXPORT_ONCE` of
  `?MemAlloc@` / `?MemFree@`) and `:92/93` (`BANISHED_UNIT_TEST`: `malloc` / `free`). Declared
  `runtime/types/object/Internal.h:457-458`. Resolves `rtalloc::MemAlloc` / `rtalloc::MemFree`
  (`allocator.cpp:1278/1280`). **Direct call**: `System::MemoryAllocator::MemAlloc(bytes, align)` /
  `::MemFree(p)` in the two product bodies. The 554 call lines (428 files: 5 root `operator delete`s via
  `RootFree`, 41 in-class ones, 24 `*Data` `DeletingDtor` arms in `CompanionData.h`, ~300 factory
  `InternalMemAlloc(sizeof(X), 8)` lines in code/Game) need NO edit; renaming them is optional later.
  The unit build keeps its host arm: `make unit` does not link `allocator.cpp`, so the direct call must sit
  behind the same `#if defined(BANISHED_UNIT_TEST)` split (or the unit build gets a host `MemoryAllocator::
  MemAlloc/MemFree`).
- **operator delete / new copies registered by name+address**: `ui_operator_delete.cpp` (60 `??3X@UI@@`
  rows, body `RtUi_ClassOperatorDelete`), `class_operator_delete.cpp` (28 rows, `RtClass_OperatorDelete`),
  `closure_batch2b.cpp:191-203` (3 named `??3`, 0x180061ae0, `operator new` 0x180011640, `LinkPrevBlock`
  0x180034d60). **Readers by name 0, by address 0 for 93 of the 94** — DEAD. The one reader:
  `wave28_lane_partsys.cpp:1042` `RtAddr<…>(0x180061ae0)(pool)` → `MemFree(pool)`.
- **`engine_funcs` doors** (`engine_funcs.cpp:24-26/39/191`, bound at `:610-643/920`): `fn_MemAlloc` =
  `ResolveAddr(Runtime, 0x1800c6e60)` = the COMPOSED `SwapAllocate` (lock + `RedzoneAlloc_TeardownOp` +
  `AllocStats_RecordAlloc`); `fn_MemFreeStatic` = `?MemFree@` = `rtalloc::MemFree` (lock + raw Free, no
  stats); `fn_MemFree` = a lambda over `fn_MemFreeStatic` that ignores its singleton; `fn_MemAllocStatic` =
  `?MemAlloc@` = `rtalloc::MemAlloc` (lock + raw Allocate, no stats). Direct: `MemoryAllocator::MemAlloc` /
  `MemFree`, EXCEPT that `fn_MemAlloc` → `MemAlloc` drops the two diagnostic hooks (see §5.3).
- **`rtcoll` doors**: `runtime/src/collection/alloc_door.cpp` `DoorAlloc/DoorFree` read the exe slot
  0x140152b60/b68 FIRST (so REDZONE sees container traffic), then 0x1800c7a50/7ad0. 69 lines.
- **Per-file wrappers**: 112 definitions (list in the script's pattern), 90 in runtime/src: `MemAlloc`,
  `MemFree`, `RtMemAlloc`, `MemFreeAtomic`, `ModMemAlloc`, `SaveInfoMemAlloc`, `UiMemFree`, `MemAllocS`,
  `HA_MemAllocViaSingleton`…; seven `void MemFree(void* p) { RtAddr<MemFreeFn>(0x1800c7ad0ull)(p); }`.
- **Spellings met that are not on the brief's list**: `Exp<Fn>("…")` (`dx11/src/buffers.cpp:175`),
  `Dx11ResolveRuntimeImport` (`dx11/src/exports.cpp:46`, `dx11_backend_access.h:381`), a `{ &slot, "name",
  0x1800134c0 }` table keyed by a VideoDX11 IAT address (`dx11/src/devicedata.cpp:252-253`),
  `R<const float*>(0x14017214c)` (`citizen_assignworkplace.cpp:125`), the `RegisterExeAddr` loop over
  `g_consts[]` (`reg_rdata_float_consts.cpp`), and `ull`-suffixed hex (`0x1800c7ad0ull`) — a plain
  `\b0x1800c7ad0\b` grep MISSES it: the by-address count was 48 before the suffix fix, 68 after.

### 4.5 Other storage-like rows

- **`RegisterExeAddr` data rows: 46** (B.3.9 said 45; the 46th is `0x140152aa8 → s_slot_TimeSerialize`,
  `reg_animal_serialize.cpp:35`, an exe IMPORT-SLOT address registered through the exe-address door, with
  0 by-address readers): 3 singleton cells (0x1401c3dd8 / 3de0 / 3de8, `address_registry.cpp:1158-1160`),
  42 `.rdata` float / bit constants in 17 files, plus that slot. Every constant has 1-37 by-address readers
  in code/Game (0x140172114: 37; 0x14017214c: 26; 0x140172234: 18); **0 constants are unread**. They
  become namespace-scope `constexpr float` (or bit-cast `uint32_t`) literals next to the readers.
- **`reg_rdata_float_consts.cpp`: 25 more** by a table loop, 1-4 readers each. **Five addresses are
  registered TWICE** (0x14017210c, 0x14017219c, 0x1401721dc, 0x140172240, 0x1401722c0): both bindings
  hold the same bits (0.33, 6, 40, 512, -0), so last-binding-wins changes nothing today — but
  `gameupdateloop_load.cpp`'s three are writable `static float`, not `const`.
- **Unregistered `.data` read by address**: 0x1401c22d0 (`action_pf_geom.cpp:279`) and 0x1401c23d0
  (`action_pf_road.cpp:324`). Nothing registers them, so every call hits `NotImplementedStub_impl` (one
  `NOT IMPLEMENTED: exe function at 0x1401c22d0` line, a stub-set entry) and falls back to the file's own
  literal table. Becomes: the literal table alone — the stub set loses two entries (a boot-log re-cut).
- **0x14015d424** (`spritecaptureui.cpp`): **already the literal `".rsc"`** (line 200); B39 §10's finding
  is stale — 0 readers.
- **`engine_funcs` data externs** (bound by initialiser, `engine_funcs.cpp:669-1109`):
  `g_pGameWorldSingleton` / `g_pGameSingleton` (both 0x1401c3dd8: 832 + 26 lines), `g_pKeyMapSingleton`
  (0x1401c3de8, 46), `g_pGameSettingsPtr` / `g_pNewGameSettings` (both 0x1401c3de0, 44 + 3),
  `g_pInputConfigSingletonExref` (import slot 0x140151738 = `&Input::Configuration::_singleton`, 36),
  `g_pAgeMultiplier` (0x140172198, 6), `g_pNotifyTimerDuration` (0x1401721cc, 3), `g_fUVScale`
  (0x14017214c, 42), `g_pMemAllocSingleton` (457). Each points at storage that already exists
  (`g_storage_*` in `address_registry.cpp`, the `k_*` constants): read it directly.
  **There is no `g_Timer_*` symbol in the tree** (0 code lines).
- **`System::Configuration::CurrentBuildId`** (carry-over 2): `RtSystem_Configuration_CurrentBuildId`
  (`value_types.cpp:612`, registered `:746`), one reader `fn_Configuration_CurrentBuildId`
  (`engine_funcs.cpp:856` → `mainmenudialog.cpp:520`). Blocked on the stated ABI question
  (`Configuration.h:224`: retail `Set()`s the return slot without zeroing it) — DIRECT once settled.
- **`Math::Vector3::operator==` 0x140152a70** (carry-over 9): `citizen_assignworkplace.cpp:123`
  `RSlot(0x140152a70)` → `s_slot_Vec3Eq`, 1 call (`:304`). The body is ported (`RtMath_Vector3_Equals`,
  `rt_math_port.cpp:63`): DIRECT, `a == b`.

## 5. Blind spots

- **The exe import table was not re-read** (`dispatch_slot_map.py` opens the retail exe in the GOG
  directory, which the brief puts off-limits). A Runtime export the exe imports and our code resolves only
  through an unregistered slot would be missed; the two allocator slots 0x140152b60/b68 are registered and
  counted. Debug's exports are in no `RegisterDispatchSlot` line.
- **Registrations with "0 readers" are 0 by NAME LITERAL and by HEX LITERAL.** A name assembled at run time
  or an address carried in data (a serialized id, a table filled by a loop from another table) is
  invisible. I read the two ??3 tables and closure 2b by hand: their rows are consumed only by their own
  registration loops. `RegisterRuntimeExport` ROW counting of the `?Allocate@`/`?Free@`/member exports:
  0 readers by name and 0 by address (the members' addresses were grepped individually) — I did not read
  every `ResolveExport(variable)` call to rule out a computed name.
- **Door-binding classification** looks at the line and the two above it. One `?MemFree@` line is
  unclassified (`?`); wrapped calls over more than three lines would be misfiled (counted, not lost).
- **Per-file wrapper definitions** are found by a signature regex (`void*/void Name…Mem{Alloc,Free}…(…)`);
  a wrapper named otherwise (e.g. `Alloc8`) or a lambda wrapper is not counted. The list in the script's
  output was read by hand; the two unit-host arms and the `rtalloc` bodies themselves are in the 112.
- **Thread**: which allocator calls run on the loader thread was not traced; the conversion does not
  change which cell or lock they use, so the question does not change.
- `code/Game` counts include harness files under `standalone/` only where named; `tests/unit` is counted
  separately.

## 6. What surprised me (code vs NEXT_SESSION §3a / comments)

1. **`g_pMemAllocSingleton` is NULL for most of the boot and is set by a SELF-TEST.** `EngineFuncsInit`
   (`engine_funcs.cpp:610`) reads the cell before the Engine exists and caches NULL; the only other writer
   is `RunFactorySelfTest`'s "refresh" (`main.cpp:685-686`), reached from `RunStartupSelfTests()`
   (`game_main.cpp:489`, inside `if (ok != 0)` and `if (s_fn_MainLoop_Ctor && …)`). 463 lines pass it to
   `fn_MemAlloc`, whose `SwapAllocate` dereferences `self+0x640`. The game works because the self-test
   always runs first. Deleting the self-test would crash the first `fn_MemAlloc`.
2. **`BANISHED_ALLOC_STATS` sees allocations but no frees.** `alloc_stats.cpp:22` says "both Allocate and
   Free take" the hooks; but the composed `SwapFree` is reachable only through `?Free@` / 0x1800c6c00, which
   have **0 readers**. Every free goes through `?MemFree@` = `rtalloc::MemFree` → raw `Free`, and every
   `?MemAlloc@` allocation bypasses `SwapAllocate` too. Diagnostic only; the live-bytes figure is wrong.
3. The "5 `RT_DATA_SLOT`" are 3 uses; one of them (`SlotMemoryAllocator`) is dead.
4. `[RtMint] TRAP` is grepped by five gate scripts and printed by nothing.
5. Stale comments: `allocator.cpp:1142-1155/1370-1380` describe `standalone/allocator_lock.cpp` (deleted;
   so is `filename_runtime_hook.cpp`, listed at `:1350` as a reader); `:1351` cites
   `engine_funcs.cpp:1631` (it is `:613`); `singleton_ctors.cpp:146-156` says "the fourteen singleton slots,
   each through RT_DATA_SLOT" (13 are class statics); `Configuration.h:238-244` describes a
   `MintStorageCell()` redirect and an `RT_DATA_SLOT` at `engine_lifecycle.cpp:283` (both gone);
   `rt_data_mint_tables.inc:12` says "all 5 .text accesses" (Ghidra shows 6: 1 read, 5 writes).
6. `RtEngine::EngineAddr` (`Engine.h:146-164`, 11 constants incl. 0x1801fc718) has 0 readers — and it
   alone would make the generator REFUSE the MemoryAllocator retirement.
7. 93 of 94 operator new/delete copies registered by address have no reader at all (the 88-row
   `??3` tables were registered "so a caller would find them"; none ever did).
8. B39 §10's `spritecaptureui.cpp:200` raw read is already fixed; the "46th" exe data row is an import slot.

## 7. Proposed blocks (disjoint FILE sets; readers first, rows last)

Order: **M1 → M2 → M3** (mint), independent of **A0**, then **A1…A6** (doors) in any order after A0,
**S1/S2** (exe storage) any time, **R** (registrations) last. Each is one worktree.

| block | files | rows / lines | content | behaviour |
|---|---:|---:|---|---|
| **M1** MemoryAllocator class | 11: `runtime/types/system/MemoryAllocator.h`, `runtime/types/common.h`, `runtime/types/engine/Engine.h`, `runtime/src/system/allocator.cpp`, `allocator_win.cpp`, `runtime/src/engine/singleton_ctors.cpp`, `engine_lifecycle.cpp`, `standalone/alloc_cutover_arm.cpp`, `standalone/main.cpp`, `code/Game/Game/game_main.cpp`, `engine/funcs/engine_funcs.cpp` (line 611 only) | 2 RT_DATA_SLOT, 4 by-address, EngineAddr, 4 ctor/dtor/Startup/Shutdown doors | `System::MemoryAllocator` real class, `_singleton` a class static; retire the row (`tools/rt_mint_retired/`), regenerate | none (same zero cell, same writer, same readers' values) |
| **M2** Debug | 3: new `runtime/types/system/Debug.h`, `runtime/src/system/debug.cpp` (or an existing system file), `runtime/src/video/enumerate_video_modes.cpp` | 1 | `System::Debug::_disableHandler`; delete the two dead null tests | none |
| **M3** delete the mint | `standalone/rt_data_mint.{h,cpp}`, `rt_data_mint_tables.inc`, `resolve_standalone.cpp`, `runtime/funcs/rt_port.h`, `rt_resolve_cache.cpp`, `tools/rt_mint_gen.py`, `tools/addr_door_audit.py`, `tools/runtime_census.py`, `tools/cutover_closure/build_roots.py`, `tests/unit/b39_obj_typeinfo_unique_test.cpp` (12) | 16 mint rows | must merge with or after M2's row leaving (empty-array); coordinator edits `finishmerge.sh`/`mergegate.sh` and the TRAP greps first | boot-log re-cut: `[RtMint]` ×2 gone, `[RtPort] runtime registry` −2/−2; unit 919 → 918 |
| **A0** one allocator API | `runtime/src/object/internal_class.cpp`, `root_operator_delete.cpp`, `runtime/src/collection/alloc_door.cpp`, `standalone/redzone_alloc.cpp`, `alloc_cutover_arm.cpp`*, `alloc_stats.cpp` (*after M1) | 2 bodies + the 2 doors | `InternalMem*` and `DoorAlloc/Free` call `MemoryAllocator::MemAlloc/MemFree`; decide where the lock / REDZONE / ALLOC_STATS hooks live (§1.6) | **titled**: REDZONE / ALLOC_STATS channel change (diagnostics only) |
| **A1** runtime/src/ui | 40 | ~65 lines | `AtomicExport`, `RtAddr<>(0x1800c7ad0ull)`, `const char* kMemFree` tables → direct | none |
| **A2** runtime/src io + engine + system + save + object + application + input | ~30 | ~60 | same | none |
| **A3** runtime/src graphics + video + audio + collection + picking + pathfinding + achievement | ~35 | ~70 (incl. `wave28_lane_partsys.cpp:1042`) | same | none |
| **A4** dx11 | 6 | 9 | `Exp<>`, `Dx11ResolveRuntimeImport`, the IAT-keyed table | none (devlost.sh) |
| **A5** code/Game + engine/funcs + standalone engine_funcs family | 230 (split by code/Game subdirectory: Component 74, Game 38, Work 36, Dialog 31, Action 23, Player 13, rest 13) | 1,019 lines + the 38 exe-slot lines + 14 ResolveExport lines | `fn_MemAlloc(g_pMemAllocSingleton, n, a)` → `MemoryAllocator::MemAlloc(n, a)` etc.; `g_pMemAllocSingleton` deleted with its self-test refresh | **titled** if A0 left the stats/redzone hooks on the member path only; else none |
| **S1** exe `.rdata` constants | 17 `standalone/reg_*.cpp` + `reg_rdata_float_consts.cpp` + `gameupdateloop_load.cpp` + ~60 code/Game readers | 67 rows | literal `constexpr` at the reader; rows deleted | none (values identical; `p ? *p : x` fallbacks become unconditional) |
| **S2** exe singleton cells + LUTs + one-offs | `address_registry.cpp:1158-1160`, `engine_funcs.{h,cpp}` externs, `action_pf_geom.cpp`, `action_pf_road.cpp`, `citizen_assignworkplace.cpp`, `reg_animal_serialize.cpp` | 3 cells, 2 LUTs, Vec3 slot, 0x140152aa8 | overlaps inventory E (engine_funcs): give the externs to E | LUTs: stub-set re-cut (log only) |
| **R** registrations | `allocator.cpp` / `allocator_win.cpp` (37 rows: 23 already DEAD), `address_registry.cpp:937-940`, `ui_operator_delete.cpp`, `class_operator_delete.cpp`, `closure_batch2b.cpp` rows (93 DEAD now) | 130+ | delete; 93 of them can go first (0 readers today) | `[RtPort]` counts re-cut |

Behaviour changes to title separately: A0's diagnostic-channel choice (and A5 if it inherits it); the
`g_pMemAllocSingleton` removal takes the self-test out of the boot's critical path (§6.1) — make it its own
commit inside A5; the LUT stub-set re-cut in S2.
