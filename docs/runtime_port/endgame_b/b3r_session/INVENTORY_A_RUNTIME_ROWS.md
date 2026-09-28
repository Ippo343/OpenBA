# Inventory A — RUNTIME_ROWS (the Runtime half of NEXT_SESSION.md §3b question 1)

Table: `data/A_runtime_rows.tsv` (2,875 lines, one per registration, table loops expanded).
Script: `scripts/A_runtime_rows.py` (repo root, no arguments; about 20 s; prints every number below).
It needs `pefile` (use `tools/ghidra_mcp/.venv/Scripts/python.exe`) because it reads the exe's import
table through `tools/dispatch_slot_map.py`. That is a read of `Application-x64.exe`. Nothing in the game
directory is written.

## 1. The answer

- There are **2,875 registrations**: 1,341 by name and 1,534 by address. Of these, 88 come from two loop tables
  (`class_operator_delete.cpp` 28, `ui_operator_delete.cpp` 60) and 2 from the mint loop. They collapse to
  **1,819 logical rows**, because a name and its mirrored address are one row: 1,053 are registered through
  both doors, 287 by name only and 479 by address only. 470 of the addresses are internal (not exports).
  Every registered name is in `rt_export_addrs.inc`.
- **Logical rows by class: DEAD 1,239 · DIRECT 578 · STORAGE 2 · STUB 0 · CALLBACK 0.** Counted by
  registration line: DEAD 2,080 · DIRECT 791 · STORAGE 4.
- Of the 1,239 DEAD rows, 1,132 have no reader site at all. The other 107 have only dead readers: unused
  constants, uncalled functions, statics that are never read, and exe slots that nothing fills.
- The DIRECT rows are 308 named and 272 internal. They have **1,365 live reader sites in 335 files**. By door:
  RtAddr<> 423, RT_EXPORT_ONCE 238, ResolveAddr(Runtime) 204, ResolveExport 138, `ResolveAddr(Module::Exe`
  on an import slot 93 (plus 39 `ResolveExport` slot fills), per-file wrapper calls 79, Atomic* 49,
  other 102.
- DIRECT target shape (logical rows):
  - 253 are internal bodies with no retail name (mostly `Collection::Array` instantiations).
  - 263 are C-door free functions for a real member (`Rt<Ns>_<Class>_<Member>(self, …)`): 102 take a
    `void*` receiver and 161 a typed one. 212 of them already have a declaration in `runtime/types` or
    `runtime/funcs`; 51 do not.
  - 29 are free functions for a static member, 32 are qualified namespace functions, 2 are for a global and
    1 is a parameter (`allocateEntry`).
- The 25 `RuntimePortInit()` calls reach every registering function, and nothing registers from outside it.

## 2. How the table was built

1. **Files.** The script walks every `.cpp/.h/.hpp/.inc/.c` under `runtime/ standalone/ engine/ dx11/ code/`,
   excluding `codegrep.EXCL`. It strips comments with `codegrep.strip()` (strings kept) and blanks string
   contents for token work.
2. **Registrations.** Every `RegisterRuntime(Export|Addr)\s*(` call is found, skipping the declarations and
   definitions. The regex allows the space, because 99 calls are spelled `RegisterRuntimeAddr  (`. Arguments
   are split at the top level.
   - A literal key is concatenated.
   - An identifier key is looked up as a file-local `kX = "…"`.
   - A `r.field` key (the loop spelling) expands to every `{0x18…, "…"}` pair in the file. The mint uses
     `{"…", 0x18…}` rows from its included `.inc`.
   - Names and addresses are paired through `rt_export_addrs.inc`.
3. **Reader sites.** The generated `.inc` files are excluded. A site is one of these:
   - any concatenated string literal equal to a registered or exported mangled name;
   - any code literal `0x18xxxxxxx` equal to a registered or exported address. Under `dx11/` a literal
     counts only through a `Module::Runtime` door, because VideoDX11 shares the 0x180000000 base;
   - any literal `0x140151218..0x140152c18` that is an exe import slot, mapped to its imported name by
     `dispatch_slot_map.slotmap`.

   Literals that are registration keys are skipped. So are slot literals that are the first argument of
   `RegisterDispatchSlot / RegisterLane3Slot / RegisterPlacePlayerSlot / RegisterExeAddr`, and file-scope
   literals that feed no binding (see §5).
4. **Door.** The door is the nearest known door token in the same statement. Otherwise it is the enclosing
   open call (`call:X`), a binding (`bind:kX`), a table (`table:` / `table-row:`) or `other`.
5. **Liveness (is the reader still called).** It depends on the binding:
   - A named binding (namespace-scope constant, table, static, global written in a function) is live if the
     identifier has a use that is not its declaration, not a plain write and not a `Register*()` key. A `.cpp`
     constant counts uses in its own TU only.
   - A constant in a header namespace (`EngineAddr::Startup`) counts qualified uses.
   - Otherwise the enclosing function must have a use outside its own definition, or be declared
     `virtual` / `override`.
   - A door body that is referenced only by its own registration is live only if one of its rows is live.
     This is solved as a least fixpoint, so self-supporting cycles stay dead.
   - A `ResolveExport` that fills an exe slot is live only if the slot has a live reader.
   - An exe-slot reader keeps a Runtime row alive only if the slot is filled through `ResolveExport`. The 7
     unfilled slots and the 10 slots filled directly with our body do not count (§5).
6. **Class.**
   - STORAGE: the mint rows.
   - STUB: the target body is marked NOT IMPLEMENTED or PORT GAP (none are).
   - DEAD: no live reader.
   - DIRECT: every other row. Its `direct_replacement` is the qualified C++ name, demangled from the key,
     plus `[decl: header]` when a `runtime/types` or `runtime/funcs` header declares that member in that
     class.
7. **Columns.**
   - `readers_by_name / _by_addr / _by_exe_slot` = `sites/live sites`.
   - `reader_files` = `file:line`, marked `(dead)` where dead, 8 shown and then `+N`.
   - Extra columns after `direct_replacement`: `door` (E/A), `key_spelling` (literal / const kX / table r.f),
     `reg_function` (qualified), `registered_more_than_once`, `notes`, `reader_doors` (door×count) and
     `target_params`.

## 3. Count per class, per directory (registration lines; logical rows in §1)

| registering dir (runtime/src/…) | DEAD | DIRECT | files |
|---|---:|---:|---:|
| ui | 651 | 87 | 94 |
| video | 371 | 65 | 28 |
| graphics | 236 | 59 | 30 |
| collection | 53 | 187 | 11 |
| object | 147 | 89 | 22 |
| io | 120 | 115 | 33 |
| system | 74 | 98 | 14 |
| achievement | 76 | 0 | 6 |
| input | 63 | 2 | 8 |
| audio | 59 | 2 | 10 |
| application | 57 | 3 | 3 |
| save | 47 | 7 | 8 |
| engine | 37 | 23 | 10 |
| picking | 37 | 7 | 4 |
| math | 25 | 29 | 3 |
| volume | 0 | 13 | 1 |
| collision2d / pathfinding / crt / utility | 10 / 7 / 7 / 1 | 0 / 0 / 3 / 4 | 2/1/3/1 |
| `standalone/rt_data_mint.cpp` | STORAGE 4 (2 rows) | | 1 |

- Of the 293 registering files, **166 register only DEAD rows**.
- Live reader sites of DIRECT rows, by reader area:

  | reader area | sites | files |
  |---|---:|---:|
  | runtime/src/ui | 389 | 83 |
  | runtime/src/graphics | 157 | 27 |
  | runtime/src/io | 157 | 32 |
  | runtime/src/engine | 76 | 8 |
  | code/Game/Component | 72 | 44 |
  | runtime/src/collection | 60 | 8 |
  | runtime/src/video | 57 | 15 |
  | code/Game/Game | 52 | 12 |
  | standalone | 52 | 11 |
  | engine (all `engine_funcs.cpp`) | 39 | 1 |
  | code/Game/Work | 39 | 17 |
  | runtime/src/save | 37 | 7 |
  | runtime/src/object | 34 | 15 |
  | dx11 | 29 | 6 |
  | code/Game/Player | 28 | 9 |

  The rest are 87 sites.
- `*_PortInit` rows and registering functions, in call order:

  | init function | rows | fns |
  |---|---:|---:|
  | Math | 62 | 5 |
  | Volume | 13 | 1 |
  | System | 163 | 15 |
  | Object | 236 | 22 |
  | Io | 233 | 32 |
  | Achievement | 76 | 6 |
  | Save | 52 | 7 |
  | Picking | 37 | 2 |
  | the four batch-8 `Rt*Register` | 6 + 4 + 7 + 6 | 1 each |
  | Video | 434 | 27 |
  | Graphics | 313 | 32 |
  | Ui | 738 | 94 |
  | Engine | 44 | 8 |
  | Audio | 58 | 10 |
  | Input | 62 | 8 |
  | Application | 71 | 5 |
  | Utility | 5 | 1 |
  | Collection | 146 | 8 |
  | CollectionDestroy | 71 | 1 |
  | ClosureBatch2b | 19 | 7 |
  | SingletonCtors | 15 | 1 |
  | DataMint | 4 | 1 |

- **Registered more than once:**
  - `?Lerp@?$InterpolatedState@VTransform@Math@@@Graphics@@…` (0x18001aff0) is registered by
    `graphics/wave9_lane_gfx.cpp:341/344` **and** `graphics/animationstate_transforms.cpp:480/484`, with
    two different bodies. The second wins by call order (`rt_graphics_port.cpp:111` then `:320`).
  - 0x18002e8d0 is registered by `object/external_path_stack.cpp:415` and `object/factory_dtors.cpp:603`,
    with the same body.
- **Keys:**
  - Names not in `rt_export_addrs.inc`: 0.
  - Addresses that are not exports: 470. They are Collection instantiations, path-stack and set helpers,
    and CRT pieces. See `notes` = `internal`.

## 4. Blind spots

- **Liveness is by identifier token, not by a call graph.**
  - A reader inside a function with a common short name (`Update`, `Draw`, `Serialize`, `Ctor`) counts as
    live whenever that token is used anywhere. DIRECT can therefore be overstated. DEAD is not overstated
    by this mechanism.
  - Functions reached only through a vtable are live if the name is declared `virtual` / `override`
    anywhere, even when that declaration belongs to a different class.
  - A function whose name is formed by `##` pasting (`J1_ENTRY(ADDR)` in `collection/array.cpp`) is invisible
    as a definition, so `target_params` is empty for those rows. It is never a reader.
- **Anything not spelled as a literal** is invisible to the method.
  - Checked by hand: every `ResolveExport` / `ResolveAddr(Module::Runtime` / `RtResolveCached` with a
    non-literal argument (24 + 62 + 19 lines). All of them are wrappers whose callers pass literals, which
    the scan finds, except `rt_resolve_cache.cpp:284`. That line is `RtResolveSlotAudit`, which re-resolves
    names that first came through a literal door.
  - No mangled name is built at run time: there is no `"?…%s"` and no string concatenation.
  - The only address arithmetic is `engine_platform_win.cpp:73/92`, which is `GetProcAddress` scaffolding
    with no registry key.
- **Tests are not scanned.** `tests/unit/rt_port_stub.cpp` stubs the registry: `ResolveExport` returns null
  and `ResolveAddr` returns 0 or `g_unitResolveAddr`. Test literals can therefore never keep a row alive, but
  `tests/unit/array_test.cpp` answers 0x1800298f0 by address. Converting runtime/src readers changes what
  those tests exercise, so they must be re-run.
- **Tools are not scanned.** `tools/rt_mint_gen.py` (the retired-row refusal), `tools/runtime_census.py` and
  `tools/dispatch_slot_map.py --check` read these files as text. Deleting rows will move their numbers.
- **Read by hand to cover the gaps:**
  - every dead reader site (158);
  - the 26 file-scope macro arguments;
  - all 34 header-namespace address constants;
  - `dx11/src/devicedata.cpp` `GetImports` (22 names in a local table walked by a loop);
  - `standalone/dlgctrl_route.cpp` `kSpecs` (3 addresses walked by a loop);
  - the 52 exe slot fills;
  - 12 random DEAD rows re-grepped raw across `runtime standalone engine dx11 code tests tools`. Every hit
    was a comment, a provenance table, a test or an unused constant.
- **Door spellings met that are not on the brief's list:**
  - `slot(…)`, `Slot(…)`, `SlotFn`, `ReadDispatchSlot`, the `resolveIAT` lambda (exe slots in code/Game);
  - `RuntimeAddr(…)`, `Door(…)`, `ByAddr1 / ByAddr2 / ByAddrSerN`, `MemberDtor`, `PtrSerialize`,
    `SerializeParameters`, `AtomicResolve`, `AtomicByName`, `ArraySetSizeFill` (per-file by-address or
    by-name wrappers in runtime/src);
  - `Dx11ResolveRuntimeImport(name, iat)` and the dx11 `GetImports` table;
  - `RegisterLane3Slot` and `RegisterPlacePlayerSlot` (slot fills that wrap a `ResolveExport`);
  - `RT_DATA_SLOT`, `RtPortLookupAddr` (`scene_watch.cpp:451`);
  - namespace address tables in headers (`EngineAddr`, `MainLoopAddr`, `ConfigurationAddr`,
    `ModificationAddr`).

## 5. What surprised me

1. **The §3a counts are right only with `\s*\(`.** 99 calls are spelled `RegisterRuntimeAddr  (`
   (aligned), so `codegrep 'RegisterRuntimeAddr\('` reports 1,354 code lines against 1,444 calls. Each
   loop table is one line, but it registers 28 or 60 rows through both doors.
2. **Two thirds of the registry has no reader.** 1,239 of 1,819 logical rows are DEAD, and 166 registering
   files are entirely DEAD: all of achievement, collision2d and pathfinding, most of ui (651 of 738 lines)
   and most of video (371 of 436). §3a's "1,480 with a reader at B.3.9" is well above today's 580.
3. **Dead readers that look alive:**
   - 91 file-local `const char* const kX = "<mangled>"` / `k…Addr` constants have no use. They sit in
     ui/dialog.cpp, element.cpp, userinterface*.cpp, video/device.cpp, graphics/renderer.cpp,
     io/filesystem*.cpp and object/type_system.cpp.
   - 24 more constants are used only as the key of their own `RegisterRuntimeExport(kX, …)`, in
     widgets_input.cpp, spritecontext_add.cpp, element.cpp and userinterface_close.cpp.
   - All 34 row-naming constants in the header address namespaces (`runtime/types/engine/Engine.h`
     `EngineAddr`, `MainLoop.h` `MainLoopAddr`, `engine/types/Configuration.h`, `Modification.h`) have 0
     uses. Only the two device-slot offsets are read.
   - `code/Game/Component/citizenai_executor.cpp:345-349` resolves 3 slots into statics that are never read.
4. **26 mangled literals are not readers.** `RT_SER_SCALAR` / `RT_SER_ARRAY` (`io/serializer.cpp`) and
   `RT_PKG2_SCALAR_GETTER` (`io/package_file.cpp`) take the mangled name as a macro argument and discard it.
5. **7 exe import slots are read by code/Game but filled by nothing,** so they resolve to 0 today (§6,
   behaviour changes):

   | slot | import | reader |
   |---|---|---|
   | 0x1401513f8 | `??0Box@Volume` | dragtool.cpp:135 |
   | 0x140151ab8 | `DeletePtr@InternalBasePtr` | nomadcomponent.cpp:109 and 4 `work_act_*.cpp` |
   | 0x140152160 | `Find@ExternalList` | gameupdateloop.cpp:552 |
   | 0x140152778 | `CreateObject@Factory` | nomadcomponent.cpp:103 |
   | 0x1401527e8 | `_rng@Particle` | gatherrawmaterials.cpp:219 |
   | 0x1401527f0 | `Lerp@Vector3` | gatherrawmaterials.cpp:225 |
   | 0x140152a70 | `??8Vector3` | citizen_assignworkplace.cpp:123 |

6. **10 exe slots are filled directly with our bodies,** bypassing the registry. These are seven
   `RtSerializer_Serialize_*` slots, `RtSerializer_GetVersion`, `Time::Serialize` (no reader) and the
   `Input::Configuration::_singleton` slot. Their 10 reader sites in
   `serialize_struct60b_serialize.cpp` and `engine_funcs.cpp:674` keep no Runtime row alive, but they still
   need converting.
7. **3 slot fills in `address_registry.cpp` fill slots nothing reads** (0x1401518f0, 0x1401526b0,
   0x140151e78).
8. **VideoDX11 and Runtime share 0x180000000.** `dx11/src/devicedata.cpp` holds VideoDX11 IAT addresses
   (0x1800133xx) next to the Runtime names, so a numeric scan of dx11 reads VideoDX11 addresses as Runtime
   ones.
9. **The table-driven doors contradict "CALLBACK".** The only addresses held in data are code-owned tables
   walked by a loop: `dlgctrl_route.cpp` `kSpecs` (behind `Opt().dlgCtrlRoute`) and `dx11 GetImports`. Both
   can name their targets, so CALLBACK is 0 for Runtime rows. No serialized id or state-machine row resolves
   through the Runtime doors. STUB is 0 as well: no registered Runtime target is a NOT IMPLEMENTED body.
   The stub set is exe-side (inventory B).

## 6. Proposed blocks (disjoint file sets; each block owns its files' registrations AND readers)

| block | owns | files | DEAD lines | DIRECT lines | live reader sites | rows read here, registered elsewhere |
|---|---|---:|---:|---:|---:|---:|
| A1 ui | runtime/src/ui | 98 | 651 | 87 | 389 | 86 |
| A2 video+graphics | runtime/src/video, graphics | 58 | 607 | 124 | 214 | 103 |
| A3 io+save | runtime/src/io, save | 47 | 167 | 122 | 194 | 52 |
| A4 runtime core | the other runtime/src dirs + runtime/types + runtime/funcs | 105 | 655 | 458 | 238 | 34 |
| A5 exe-side readers | code/, engine/, standalone/, dx11/ | 115 | 0 | 0 (+4 STORAGE) | 330 | 130 |

**Order**
1. **Phase 1, in parallel (rows stay; a READER conversion is checkable by the compiler).**
   - Each block converts its live readers to the `direct_replacement`.
   - Each block also deletes its own DEAD registration lines, its dead reader constants and statics, and
     the `k…` constants that only key a registration.
   - A DEAD row needs no one else. Blocks A1 to A4 can delete their DEAD lines on day one: 2,080 lines, 166
     whole register functions.
2. **Phase 2.** Once every block has converted, the DIRECT rows are DEAD. The block that owns each
   registering file deletes those lines, and the generator (`tools/rt_mint_gen.py`) plus a link prove it.
   Cross-block rows (the last column) are why this cannot run in phase 1.
3. **Coordination.**
   - A5's `engine/funcs/engine_funcs.cpp` (39 sites) overlaps inventory E. Give that file to one block
     only.
   - A5's `standalone/address_registry.cpp` slot fills overlap inventory B (exe rows).
   - The mint (4 lines, 2 rows) is inventory D's.

**Separate titled commits (behaviour changes, gated with resave x5)**
- The 7 unfilled exe slots in §5.5. A direct call would replace a null or a fallback path with the real
  body. Read each consumer first, per `.claude/rules/address-registry.md`: some slots are null on purpose.
- The `InterpolatedState<Transform>::Lerp` double registration. Deleting the losing copy in
  `wave9_lane_gfx.cpp` is not a behaviour change. Deleting the winner is.
- `standalone/dlgctrl_route.cpp` (diagnostic route behind a flag) and `scene_watch.cpp`'s `RtPortLookupAddr`
  probe. These inspect the registry itself, so they go when the registry goes, not before.
- The boot-log lines `[RtPort] … exports, … addrs` and `[RtMint]` change with every deleted row. That is
  question 6's decision.
