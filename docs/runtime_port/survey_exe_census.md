# What is left to be ported from `Application-x64.exe`

Branch `wave/upgrade-path-complete`, tip `d7023fa`, 2026-09-06. Every number below names the command that produced it. No files were modified.

## 1. Registry census

Counts from a Python scan of `standalone/`, `code/`, `engine/`, `runtime/` (excluding `build*/`) that resolves both literal-hex arguments and same-file `k...` constants, plus the four table-driven registrars (`reg_element_typeids`, `reg_food_typeid`, `reg_rdata_float_consts`, `reg_unreg_lane4`).

| Registration band | Distinct addresses |
|---|---|
| `.text` code functions (`0x140001000`-`0x1401501ff`) | **1,034** |
| PE import / dispatch slots (`0x140151000`-`0x140152fff`) | **152** (129 via `RegisterDispatchSlot`, rest direct) |
| `.rdata` vtables and constants | **229** |
| `.data` globals and type-id storage | **140** |
| **Union of all `RegisterExeAddr` + `RegisterDispatchSlot`** | **1,555** |

Call-site totals: 1,399 `RegisterExeAddr` and 148 `RegisterDispatchSlot` lines, so a handful of addresses are registered twice.

**There is no project-wide list of "real" game functions.** No such artifact exists in `decomp/`, `docs/waves/`, `tools/`, or the four `*_hash_index.json` files at the repo root, which hold only Ghidra hash metadata. Two denominators are available instead.

Ghidra, queried live through the MCP bridge, now reports **11,473 functions** in the exe, not the 10,476 in CLAUDE.md. Of those, 9,530 carry a custom name and 1,111 still carry a default `FUN_` name. That measures documentation, not porting.

The honest denominator for porting is what our tree actually asks for. A scan for literal `ResolveAddr(Module::Exe, 0x...)` sites finds **1,135 distinct addresses requested**, of which **139 are unregistered**:

| Band | Asked for | Unregistered |
|---|---|---|
| `.text` code | 651 | **14** |
| PE import slot | 196 | 69 |
| `.rdata` vtable/const | 136 | 36 |
| `.data` global | 152 | 20 |

The 14 unregistered `.text` addresses our own code asks for:
`0x140002280 0x140012a60 0x140017050 0x14001fdb0 0x14001fe60 0x1400786b0 0x1400938d0 0x140097fb0 0x14009e8c0 0x1400f2f00 0x1400fcc90 0x140101ee0 0x1401026a0 0x140128f50`

The runtime census is smaller because it records only addresses a real run resolves. `docs/waves/unreg_census_20260904_postwave.txt` has 55 rows; diffed against today's registry, **49 are still unregistered and 6 have landed since**. The older pre-wave census had 157 rows, of which 127 are still unregistered. That file is superseded.

Category (c), unaccounted for, is large but mostly uninteresting: roughly 10,400 exe functions are neither registered nor requested. Most are CRT, exception, and thunk code, or bodies our C++ reimplements inline without ever needing an address. Nothing in the repo partitions that set, and building one would need a Ghidra-side pass.

Deliberately held back, per the wave verdicts: an unregistered address is often correct. `docs/waves/code_fns_wave.md` records that of 18 audited rows only 6 were real gaps and 5 would have caused active harm if registered. Reason categories in use are `deliberate - leave` (a caller null-guards and takes a fallback), `dead` (no caller), and `needs a run to decide`.

## 2. Stubs still live

Marker counts across `code/`, `standalone/`, `engine/`, `runtime/`, excluding build directories:

| Marker | Hits |
|---|---|
| `Stub_` | 813 |
| `_stub` | 78 |
| `STUB` | 59 |
| `unported` | 55 |
| `NOT IMPLEMENTED` | 16 |
| `passthrough` | 13 |
| `XXX` | 12 |
| `TODO` | 10 |
| `NotImplementedStub` | 5 |

The `Stub_` bulk is harness by design, not gaps. The four `standalone/*_stubs.cpp` files (menu, splash, loader, game-update-loop vtables) plus `code/Game/Dialog/ui_dialog_stub_factories.cpp` account for most of it. That last file is 1,654 lines of generic 48-slot UI vtables whose deliberate purpose is to keep `IsDerivedFrom(NULL)` from firing; two entries were later given real bodies.

Real functional gaps, ranked by likely player-visible impact:

| # | Gap | Location |
|---|---|---|
| 1 | Crop field state-machine handler bodies all registered as `nullptr`; the table runs, the states do nothing | `code/Game/Component/cropfieldcomponent_states.cpp:41` |
| 2 | Three citizen AI states unported: Work, AdultTransition, ElderTransition | `code/Game/Component/citizenai_handlers2.cpp:16` |
| 3 | Fenced-area tool unported, so pastures are unreachable without test scaffolding | `code/Game/Game/starttown.cpp:2383` |
| 4 | Drag-area and placement tools unported for crop field, hunter lodge, gatherer hut | `code/Game/Game/starttown.cpp:3041` |
| 5 | Pasture dialog unported; livestock type must come from an env var | `code/Game/Component/pasturecomponent_spawn.cpp:369` |
| 6 | Achievement award toast unported, so awards record silently with no on-screen announcement | `code/Game/Game/gameupdateloop_load.cpp:1317` |
| 7 | `ClearStorage` slot 3 Serialize has no port; the vtable holds a no-op | `code/Game/Work/clearstorage.cpp:683` |
| 8 | Trade controller event `0xa` (`b240`) registered as null | `code/Game/Component/tradecomponent_states.cpp:620` |
| 9 | `AutoPickupWork` slots 1 and 2 substituted with generic shims | `code/Game/Work/autopickupwork.cpp:98` |
| 10 | Work base scalar dtor, dtor thunk and GetTypeId substituted with generic shims | `code/Game/Work/work_base_standalone.cpp:115` |
| 11 | Background music ported but gated opt-in and never proven to play | `code/Game/Game/unreg_lane5_musicplayer.cpp:107` |
| 12 | `RemovalTool::CalcPath` drafted but not landed; its closure pulls in two sim functions | `final_steps.md`, Keystone A wave 2b |
| 13 | MarketWork AVL cluster and `LongTermPlan` remain the residual Work defers | `final_steps.md`, Keystone B tail |
| 14 | `EditTool::SelectEntity` null path logs and returns 0 | `code/Game/Player/edittool.cpp:1044` and `:1138` |
| 15 | `SubArrayDeepCopy` clears count but not capacity, unlike retail | `code/Game/Component/serialize_struct60b.cpp:76` |
| 16 | `AmbientEmitterComponent` find, blocked on a run that counts emitters | `docs/waves/lanes/integrator_queue.md`, tail |

Two entries the wave docs still list as open are already closed in the tree: the quicksave/quickload `EXEC-of-0` at `0x140095da0` is ported and registered (`code/Game/Game/gameupdateloop.cpp:2204`), and `MusicPlayer::Update` is ported behind its flag.

## 3. Existing "what's left" documents

| Document | Open items | Staleness |
|---|---|---|
| `final_steps.md` | Definition-of-done checklist, six keystones, 95-address gap | **Badly stale.** Ground-truthed 2026-06-19. Claims the world does not render, does not simulate, has no HUD, and that all 68 components are inert. All four are false now. |
| `decomp/README.md` | Five passthrough wrappers blocked on FileSystem/PackageFile | **Stale.** Its status legend and "57 hooks, all verified in-game" section describe the removed proxy-DLL mode. |
| `decomp/application.md` | Per-class inventory | **Stale legend.** Nearly every component is marked `~` = "passthrough hooks active", which no longer exists. Struct and address facts remain useful. |
| `docs/waves/code_fns_wave.md` | 3 open: screenshot crash, ambient-emitter run, music run | Item 1 is closed in the tree. Items 2 and 3 are still open. |
| `docs/waves/unregistered_slots_wave.md` | "the expensive band": 48 `.text`, 12 `.rdata`, 5 `.data` | Current in method. Counts predate the six registrations that landed since. |
| `docs/waves/rdata_data_wave.md` | 17 never-triaged `.rdata`/`.data` addresses | Current. Lane diff shows 12 of its 17 are still unregistered. |
| `docs/waves/lanes/integrator_queue.md` | Two deletion candidates to verify, one stale comment, one queued run | Current. |

Lane-file diff against today's registry, per `docs/waves/lanes/*.txt`: `code_lane1` 7 of 9 still unregistered, `lane2` 23 of 23, `lane3` 24 of 24, `lane4` 43 of 50, `w2_lane1` 5 of 6, `w2_lane2` 5 of 6, `w2_lane3` 2 of 5. The `lane2` and `lane3` files are earlier-wave dispatch-slot lists whose rows were mostly resolved as deliberate leaves, not ports.

## 4. Census tooling

| Script | What it does |
|---|---|
| `tools/dispatch_slot_map.py` | Authoritative exe PE-import slot to mangled-name map; `--check` validates our registrations |
| `tools/unreg_full.py` | Annotates a `BANISHED_UNREG_CENSUS` dump, grouping every row by band with its import name |
| `tools/unreg_loadpath.py` | Same dump, filtered to rows resolved during load or post-load |
| `tools/slot3_resolver_audit.py` | Finds component slot-3 Serialize wrappers that never call their resolver, the silent desync shape |
| `tools/typeid_xcheck.py` | Joins `0x1401cXXXX` type-id literals in our code against `standalone/factory_registrations.csv` |
| `tools/slot_name_audit.py` | Reports every import slot we read together with the symbol the PE table actually puts there |
| `tools/dump_vtable.py`, `tools/find_vtable.py` | Read a vtable straight out of exe `.rdata` |
| `tools/gen_factory_registrations.py` | Generates the factory registration CSV from Ghidra |
| `tools/pe_exports.py`, `tools/symbolize.py` | Export enumeration; crash-log symbolization via `nm` |

`tools/ghidra_mcp/.venv/Scripts/python.exe tools/dispatch_slot_map.py --check` ran clean:

```
checked 72 ResolveExport registrations, 0 name mismatch(es);
14 address(es) registered more than once
```

Zero mismatches is the required state. The 14 shadowed addresses are all `IO::Serializer` methods in `standalone/address_registry.cpp`, where an early block around lines 1134-1213 is dead and a later block near 1663-1680 wins. That is dead code to clean up, not a behavioural defect.
