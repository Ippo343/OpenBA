# W2 brief — rows and files go (read AFTER `BLOCK_BRIEF.md`, `../B3R_RULES.md`, `W1_AUDIT.md`)

Wave 1 converted the readers. Wave 2 deletes what they read. Nothing at run time checks a row
deletion (a deleted Runtime address row that is still read prints NOTHING), so every deletion here is
proved BEFORE it is made, by a sweep of the whole tree, and the compiler and the linker are made to
check as much of it as they can.

## Order (serial merges; each gated alone)
| block | content | after |
|---|---|---|
| **W2-EZ** | `engine/funcs/engine_funcs.{h,cpp}`, `EngineFuncsInit()` and its call, the remaining `engine/types` aliases; the bodies that live in `engine_funcs.cpp` MOVE to their module (`fn_ModDialog_ArrayPush58_CppImpl` -> `code/Game/Dialog/`, `Entity_FindComponentByExeTypeId` -> `code/Game/Entity/`), the dead IBPTree functions go | STORAGE |
| **W2-RT** | every `RegisterRuntimeExport` / `RegisterRuntimeAddr` line in `runtime/` and `dx11/`, the register functions, the `*PortInit()` functions that only register, `RuntimePortInit()`'s calls to them, `standalone/reg_rt_mirror16.cpp` | STORAGE (parallel with W2-EZ: disjoint files) |
| **W2-EXE** | `standalone/reg_*.cpp`, `standalone/address_registry.{cpp,h}`, the `code/Game` Register functions, the dispatch slot cells, `tools/dispatch_slot_map.py` and its gate line | W2-EZ, W2-RT |
| **W2-RES** | `standalone/resolve.h`, `resolve_standalone.cpp`, `runtime/funcs/rt_resolve_cache.cpp` (create `b3r_session/RESOLVE_CACHE_GONE` in the same commit), `RegisterRuntime*` / `RtPortLookup*` in `runtime/funcs/rt_port.h`, the registry instruments (`dlgctrl_route.cpp`, `scene_watch.cpp`'s probe, `[UnregCensus]`, `TraceExeBegin/End`), the per-file `enum class Module` copies, `runtime/src/collection/alloc_door.cpp`, `tests/unit/rt_port_stub.cpp`, the old REDZONE / ALLOC_STATS seats | W2-EXE |

## Rules (in addition to BLOCK_BRIEF.md's)
1. **Before deleting a row, prove it has no reader.** For its key (mangled name, runtime address, exe
   address, import slot address) run `codegrep.py` over the WHOLE tree for the key as a literal, in
   every spelling: `0x14009c480`, `0x14009C480`, `0x9c480` next to a base, the mangled name as one
   string and as split literals. A hit that is not the registration line itself is a READER: convert
   it per R2 (your block may edit that file for that one conversion and lists it), or, if it is a STUB
   reader, write the PORT GAP null arm of R4. Never delete a row that has a reader.
2. **READ THE WHOLE BODY of every register function, `*PortInit`, `*_InitFuncs` and static
   initialiser before deleting it or its call.** They carry side effects that are not rows: an
   `atexit` hook, a type-id cache, a singleton construction, a table fill, a `printf` a gate parses,
   the order of two constructions. (W1 lost the `[ExtLoad]` exit line this way: commit `85a40178`.)
   A side effect MOVES to the init function of the module it belongs to, in the same commit, and the
   hand-in lists it: function, what it did, where it went.
3. **A body that loses its registration and has no other reference**: if it is `static`, or the
   compiler now reports it unused, delete it. Otherwise it STAYS (bodies are provenance) and is
   listed for the close with its retail address. Do not leave a new `-Wunused-function` warning:
   after your last edit do a FULL recompile (`make clean` first) and compare the warning count
   with main's.
4. **The STUB rows stay until their reader is a PORT GAP arm, then go.** What remains registered at
   the end of W2 must be nothing: a key with no C++ body is a comment at its call site, not a row.
   `0x140002280` `Entity::FindAudioComponent` is the one address in every route's `NOT IMPLEMENTED`
   set: its printer goes with the registry (W2-RES), and the gate's stub-set check then expects an
   EMPTY set (the coordinator re-cuts the baseline; R10 disposition (b)).
5. **Boot lines.** `[RtPort] … exports registered`, `[RtPort] runtime registry: E exports, A addrs`
   and `[AddressRegistry] N` fall to zero and then leave with their printers. Do not keep a printer
   that prints 0. Lines the GATE parses and that are NOT registry lines must survive unchanged:
   `[ExtLoad] calls=`, `[ExtAdd]`, `[ExtSaveText]`, `[RtUnresolved]`, `[AutoStart]`, `[BootState]`,
   the typetable dump. `INVENTORY_F_BOOTLOG_GATE_TOOLS.md` lists every parsed line and its printer.
6. **Init ORDER is behaviour.** When `RuntimePortInit()` / `AddressRegistryInit()` lose their
   registering calls, the calls that remain (constructions, installs) keep their relative order.
   When a whole init function is left with nothing, its call and its declaration go.
7. **Tools and docs.** A tool that parses a deleted file or a registry line is fixed or deleted in
   the block that deletes what it parses, never left failing: `tools/dispatch_slot_map.py`,
   `tools/runtime_census.py`, `tools/addr_door_audit.py`, `tools/cutover_closure/`. `CLAUDE.md`
   and `.claude/rules/address-registry.md` are the coordinator's.
8. **The Makefile** is the coordinator's: list the source files you delete; the coordinator removes
   explicit entries (wildcard entries need nothing).
9. No behaviour change. W2 has no `behaviour:` commit. Anything that would change behaviour is LEFT
   and listed.

## Known before you start (from W1 hand-ins; the audit adds to this in `W1_AUDIT.md`)
- Eleven init functions are already empty: `RtAchievementPortInit`, `RtApplicationPortInit`,
  `RtSingletonCtorsPortInit`, `RtEnginePortInit`, `RtInputPortInit`, `RtPickingPortInit`,
  `RtCollectionDestroyPortInit`, `RtCollision2DCollisionMeshRegister`, `RtCollision2DSurfaceRegister`,
  `RtPathfindingPointListRegister`, `RtPickingPickingMeshRegister`; and the callers of
  `RtCollectionGraphicsAnimRegister` / `RtCollectionGraphicsMeshRegister`. "Empty" was judged by the
  block that emptied it: re-read each (rule 2).
- dx11: 20 dead rows in `companion_data.cpp` / `device_enum_types.cpp`. The 84-slot `*Data` seam is
  NOT touched.
- exe: the empty `RegisterPathToolSetSelected()` call in `address_registry.cpp`, the empty
  `RegisterGameTerrainReimpls`; `standalone/reg_keystone_a_zone.cpp` has a local `void` prototype
  of `RoadTool_sub_ce00` (the body returns `int`); `WorkAcc_ZoneRefresh_9c480`
  (`work_base_accessor.cpp`) is a second body for 0x14009c480 with no caller.
- `SavedState`'s ctor row is resolved by `EngineFuncsInit`: it goes after W2-EZ.
- `MainLoopAddr::CreateVideoContext` is a dead constant; `TabElement_UpdateContainers_Body` and
  `RtGraphics_InterpolatedStateTransform_Lerp` (`wave9_lane_gfx.cpp`) are bodies with no reference.
- Inventory A's "member exists" column is unreliable (declared, undefined members exist).

## From the audit (`W1_AUDIT.md`; read §1.3 and §1.4 in full)
- **Hazards H1-H8 (§1.4): 14 surviving register / port-init functions do more than register.**
  `RtSystemAllocatorRegister()` installs the allocator platform seam, calls the cutover armer and
  prints `[RtAlloc] ARMED` (read by `tools/stress_run.sh`); `RtObjectPortInit()` installs the
  `[ExtLoad]` counters; `RtMathPortInit()` calls `RtLibm::Init()`; `RtGraphicsPortInit()` calls
  `RtGraphicsWave29SlotsSummary()`; `RegisterTownWorkplaceRegistry()` tests the seat-shuffle option;
  the slot wrappers OWN the cell storage. Each such function keeps its non-registering statements
  (renamed for what it now does if "Register" is no longer true), in the same place in the init order.
- **Dead constants (R3) owed by W1**, deleted by the W2 block named: M1 113 unused `XAddr` constants
  in 23 `code/Game/Work/*.h` headers and M2 12 in 10 `code/Game/Player/*.h` headers (W2-EXE;
  `BuildRoadAddr::CtorParams` is a live row key until its row goes); M3 138 in
  `engine/types/ui/Panels.h` and `Dialogs.h` (W2-EZ); M4 the caller-less
  `AmbientEnvironmentComponent_InitFuncs()` and its declaration (W2-EXE).
- `reg_rt_mirror16.cpp` registers nothing (its seven fills return null): W2-RT deletes it.
- 0x140089470 is registered only under `Opt().liveSeatShuffle` (R4): the option is tested at the
  call site, the row goes (W2-EXE).

## Hand-in (in addition to BLOCK_BRIEF.md's)
- registration lines deleted (by spelling), register / init functions deleted, files deleted
- every side effect moved (rule 2)
- readers found by rule 1 and what each became
- bodies left with no reference (address, name, file)
- the boot lines that left, the tools fixed or deleted
- full-recompile warning count, yours and main's
