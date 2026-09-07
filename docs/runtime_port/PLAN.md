# Runtime-x64.dll Port — Review, Facts, and Plan

Ground-truthed 2026-09-06 on branch `wave/upgrade-path-complete` (tip `d7023fa`, 382 commits
ahead of `main`). Every number below came from a command or a live Ghidra query run that day;
the source is named next to each. This document is the brief for agents working the runtime
port. Read §4 (strategy) and §6 (per-function recipe) before touching code.

---

## 1. Exe final review — where Application-x64.exe stands

### 1.1 Registry census (`standalone/`, `code/`, `engine/`, `runtime/`, build dirs excluded)

| Band | Distinct addresses registered |
|---|---|
| `.text` code functions (`0x140001000`–`0x1401501ff`) | 1,034 |
| PE import / dispatch slots (`0x140151000`–`0x140152fff`) | 152 |
| `.rdata` vtables + constants | 229 |
| `.data` globals + type-id storage | 140 |
| **Union** | **1,555** |

`tools/dispatch_slot_map.py --check`: 0 name mismatches, 14 shadowed addresses (all
`IO::Serializer` methods registered twice in `standalone/address_registry.cpp`; the early block
around lines 1134–1213 is dead, the later block near 1663–1680 wins). Dead code, not a defect.

There is **no project-wide list of "real" game functions**; Ghidra now reports 11,473 exe
functions (CLAUDE.md says 10,476), 9,530 custom-named. The honest porting denominator is what our
tree asks for: **1,135 distinct `ResolveAddr(Module::Exe, …)` addresses, 139 unregistered**
(14 `.text`, 69 import slots, 36 `.rdata`, 20 `.data`). Most unregistered slots are deliberate
(see `docs/waves/unregistered_slots_wave.md`: a null slot often selects retail's fallback path).
The runtime census `docs/waves/unreg_census_20260904_postwave.txt` has 55 rows, 49 still open.

The 14 unregistered `.text` addresses our own code requests:
`0x140002280 0x140012a60 0x140017050 0x14001fdb0 0x14001fe60 0x1400786b0 0x1400938d0
0x140097fb0 0x14009e8c0 0x1400f2f00 0x1400fcc90 0x140101ee0 0x1401026a0 0x140128f50`

Triaged 2026-09-06 (lane D, `docs/waves/exe_closeout/lane_d.md`): `0x14001fdb0`, `0x14001fe60` and
`0x140128f50` PORTED (the last, `PickingComponent::OnAdded`, was a real gap: three consumers
null-guarded and skipped the pick-mesh rebuild, and `buildcomponent_state8.cpp` passed the
InternalBasePtr's ADDRESS instead of the object at four sites). `0x140012a60 0x140017050
0x140097fb0 0x1400786b0` are comment-only census artifacts. `0x1400938d0 0x14009e8c0 0x140101ee0
0x1401026a0 0x1400fcc90` are deliberate active-harm leaves. **`0x1400f2f00` must stay
unregistered**: `pickup.cpp:828` uses it as dispatch-or-fallback and registering it makes the
dispatcher call itself. Live census after the wave: 55 rows.

### 1.2 Real functional gaps still in the tree (ranked by player-visible impact)

| # | Gap | Where |
|---|---|---|
| 1 | ~~Crop-field state-machine handler bodies registered as `nullptr`~~ **CLOSED (lane A) — stale banner; all 11 handlers have bodies (`0x14011a920`…`0x140119d80`), the remaining nulls are the trans30 slot retail leaves empty. Field ran 9→10→11 end to end through the real tool path** | `docs/waves/exe_closeout/lane_a.md` |
| 2 | ~~Citizen AI states Work / AdultTransition / ElderTransition unported~~ **CLOSED — the cited line was a stale comment; all three are in `citizenai_handlers3.cpp` and registered. All three observed entering in-game. `CitizenComponent+0x68` is the Disease\*, NOT a housing link: state 5 is the sick-citizen state and is offered (and entered) as soon as a citizen is infected** | `docs/waves/exe_closeout/lane_c.md` |
| 3 | ~~Fenced-area tool unported~~ **CLOSED (lane B) — `FencedTool` fully ported (vtable `0x1401605d0`, CalcPath `0x1400dae00`) and in the tool-mode tree; pasture placed through it in-game** | `docs/waves/exe_closeout/lane_b.md` |
| 4 | ~~Drag-area / placement tools for crop field, hunter, gatherer~~ **CLOSED (lane A) — both tools ported and default-on; the real gap was the harness (`FirePlacementTool` hard-coded WoodHouse), fixed** | `docs/waves/exe_closeout/lane_a.md` |
| 5 | ~~Pasture dialog unported~~ **CLOSED (lane B) — `PastureUI` vtable `0x14015f610` incl. Slot11 SetLivestockType `0x1400b4c40`; livestock chosen through the real combo event in-game. Harness bug fixed: the pasture registry only holds FINISHED pastures** | `docs/waves/exe_closeout/lane_b.md` |
| 6 | ~~Achievement award toast unported~~ **PORTED — `0x140096960` → `code/Game/Game/eventlog_achievement.cpp`, installed at Load step 30, toast verified in-game ("[award message elided]")** | `docs/waves/exe_closeout/lane_c.md` |
| 7 | ~~`ClearStorage` slot-3 Serialize is a no-op~~ **PORTED (lane D)** — retail `0x1400e7350` = Work::Serialize + Serializer(stream,0) + BST at +0x58; the no-op read ZERO bytes and desynced the next record | `docs/waves/exe_closeout/lane_d.md` |
| 8 | ~~Trade controller event `0xa` registered null~~ **PORTED (lane D)** — `0x14005b240`: inlined StateMachine::Tick → RequestStateTransition(sm,2) + Tick | `docs/waves/exe_closeout/lane_d.md` |
| 9 | ~~`AutoPickupWork` slots 1/2 are generic shims~~ **CLOSED (lane D)** — not dtor thunks; they return the REGISTER_TYPE id/name pair, identical via GetTypeIdByName | `docs/waves/exe_closeout/lane_d.md` |
| 10 | ~~Work base dtor / dtor-thunk / GetTypeId shims~~ **PORTED (lane D)** — real deviation: the scalar dtor omitted retail `0x14002ca60`'s Work-base vtable re-stamp | `docs/waves/exe_closeout/lane_d.md` |
| 11 | ~~Background music ported, opt-in, never proven to play~~ **PORTED, DEFAULT-ON (lane D)** — `MusicTracks.rsc -> 15 tracks`, `PlayEffect -> handle=1536` with no env var set | `docs/waves/exe_closeout/lane_d.md` |
| 12 | ~~`RemovalTool::CalcPath` drafted, not landed~~ **CLOSED (lane D)** — already landed in `reg_keystone_a_removal.cpp:41`; absent from the census | `docs/waves/exe_closeout/lane_d.md` |
| 13 | ~~MarketWork AVL cluster + `LongTermPlan` residual defers~~ **CLOSED (lane D)** — all registered; `0x1400eff80` is Execute (not Serialize), in the vtable | `docs/waves/exe_closeout/lane_d.md` |
| 14 | ~~`EditTool::SelectEntity` null path returns 0~~ **CLOSED (lane D)** — `0x1400fd3b0` IS registered; the guards are dead code in harness-only functions | `docs/waves/exe_closeout/lane_d.md` |
| 15 | ~~`SubArrayDeepCopy` clears count but not capacity~~ **PORTED (lane D)** — retail zeroes one qword over 0x08..0x0f; also registered `0x14001fdb0`/`0x14001fe60` | `docs/waves/exe_closeout/lane_d.md` |
| 16 | ~~`AmbientEmitterComponent` find, needs a counting run~~ **CLOSED, measured (lane D)** — 1000 refreshes: count1=435, count2plus=565, max=2; the "≥2 emitters" criterion fails; `a710` reads entry[1] unguarded | `docs/waves/exe_closeout/lane_d.md` |

Closeout 2026-09-06: items 1–5, the player-visible ones, were ALL stale source comments over
code that was already ported (lanes A/B/C proved each end to end in-game); item 6 was a real gap
and is ported. Lane C's one remaining open is now CLOSED TOO (2026-09-06): the premise was false.
`CitizenComponent+0x68` is the **Disease\*** (Infect `0x140116b90` stores it there), not a housing
link, so candidate state 5 is offered only to SICK citizens and a healthy town correctly never
offers it. With `BANISHED_FORCE_SICK=25` the census reports `Work (5) offered=55 top=55 p6=55` and
the handler `0x14010d790` enters; the control run has neither. Nothing else is open. Everything else is a corner. The `Stub_` marker count (813) is harness by design, mostly the
four `standalone/*_stubs.cpp` vtable files and `ui_dialog_stub_factories.cpp`.

### 1.3 Documents that are now wrong

| Document | Problem |
|---|---|
| `final_steps.md` | Ground-truthed 2026-06-19. Says the world does not render, does not simulate, has no HUD, all 68 components inert. All false. Keep only as history. |
| `decomp/README.md` | Legend and "57 hooks verified in-game" describe the removed proxy-DLL mode. |
| `decomp/application.md` | Legend `~` = "passthrough hooks active" no longer exists; struct/address facts still good. |
| `CLAUDE.md` | Exe function count (10,476 vs 11,473 in Ghidra); "Ghidra 6 programs" is now 7. |

### 1.4 Housekeeping before the runtime phase

All done 2026-09-06 (`4eeed89`, `0f8257f`, `9d68a38`): `main` fast-forwarded; the 14 dead
`IO::Serializer` registrations and `spritesheet_debug.{cpp,h}` removed (`--check` now 0/0);
`decomp/README.md` rewritten for standalone mode; `final_steps.md` marked historical;
CLAUDE.md launcher-dialog paragraph, function count and Ghidra program list corrected.

---

## 2. How our code calls the Runtime DLL today

Resolution happens in `standalone/resolve_standalone.cpp` (`ResolveInit` → `LoadLibrary`, ASLR
delta), then `EngineFuncsInit()` + `RuntimeFuncsInit()` from `standalone/main.cpp:1605` fill
typed pointer tables.

| Mechanism | Sites | What it is |
|---|---|---|
| `ResolveExport("?mangled@…")` | 990 | Exported Runtime function by MSVC name (651 distinct names) |
| `ResolveAddr(Module::Runtime, 0x1800xxxxx)` | 401 | Non-exported Runtime internal by Ghidra address (301 distinct) |
| `ResolveOrdinal(n)` | 14 | Export by ordinal (7 distinct) |
| `RegisterDispatchSlot(exeSlot, ResolveExport(…))` | 139 | The exe's own PE import table; retail-shaped code reads `*(fn**)slot` |
| `Dx11ResolveRuntimeImport(mangled, iat)` | `dx11/`'s own door | **Was missing from this table and from `runtime_census.py` until 2026-09-07.** `dx11/` imports from Runtime exactly as the shipped plugin did; 60 distinct names, all verified real exports. Video's called count goes 69 → 112 on this correction alone (WAVE9.md) |
| `engine/funcs/engine_funcs.{h,cpp}` | ~1,029 resolves | Typed `fn_*` pointer table, the main consumer of the two above |
| Runtime globals by address (`0x1801f…`, `0x1802…`) | see §2.2 | Singletons: MainLoop, Persistence, Video::Device, Timer, allocator |
| Virtual calls through raw vtable pointers | everywhere | Objects built by retail ctors carry DLL vtables |
| Exported data symbols (`?_singleton@…`) | 8 | MainLoop, Video::Device, FileSystem, InternalFactory, SaveLoad, AchievementManager, Input::Device, Input::Configuration |
| Direct code / IAT patches into the loaded DLL | 4 files | `standalone/allocator_lock.cpp` (13-byte detours on `MemoryAllocator::Allocate`/`Free`, default-on), `standalone/filename_runtime_hook.cpp` (diagnostic detours), `standalone/video_interface.cpp` (detours `Video::Device::InitializeInterfaces`, writes into the Device object), `standalone/main.cpp:706-760` (swaps `MessageBoxW` in Runtime's IAT) |

Note: CLAUDE.md still describes a `SkipPlatformConfigure()` patch on the `DialogBoxParamW` call
site. That function no longer exists; the launcher dialog is suppressed through the registry by
`HarnessGuardUnattendedBoot()` (`standalone/main.cpp:184`).

**VideoDX11-x64.dll is already fully replaced** (`dx11/src/`, 13 files, plus `dx11/funcs/dx11_seam.cpp`
with twelve vtables / 84 slots). The DLL is never loaded; `InstallVideoInterfacePrefill` stops
Runtime from `LoadLibrary`ing it and our two exports (`CreateDeviceData`, `GetDeviceName`) are
what Runtime sees. That is the finished shape a Runtime port aims at, with ~4,800 exports
instead of two.

The distinct dependency surface (`call_surface_20260906.csv`, 839 rows): **588 exports,
244 internal addresses, 7 ordinals across 126 classes.** Heaviest by call site: `Math::Vector3`
(50), `IO::Serializer` (39), `System::MemoryAllocator` (29), `System::Random` (28),
`Video::Context` (26), `UI::Element` (23), `System::String` (21), `UI::UserInterface` (21),
`Graphics::Geometry` (20), `System::Filename` (19), `Save::SaveLoad` (19).
By namespace: UI 135 functions, Graphics 73, Math 59, System 54, Video 49, IO 40, Object 36,
Achievement 25, Input 21, Save 20, Audio 20, Engine 17, plus 253 unnamed internal addresses.

### 2.1 The one existing runtime reimpl is the precedent

`runtime/src/io/serializer.cpp` reimplements the `IO::Serializer` forwarders. Each wrapper checks
`BANISHED_REIMPL_SERIALIZER` once; unset it tail-calls the resolved original, set it runs the
port. It is registered into the exe dispatch slots. **This only redirects our callers.** The
DLL's own internal calls to `Serializer::Serialize` still go to the DLL. That is fine for a
forwarder; it is not fine for anything with state or for A/B-proving a port.

### 2.2 Coupling hazards for a port (each breaks the moment the function it names is ours)

**A. Byte patches on Runtime prologues** at hardcoded addresses. When the target is ported the
patch must be retired, not carried.

| Target | Address | File |
|---|---|---|
| `MemoryAllocator::Allocate` (13-byte prologue, default-on lock) | `0x1800c6e60` | `standalone/allocator_lock.cpp:292` |
| `MemoryAllocator::Free` (12 bytes) | `0x1800c6c00` | `standalone/allocator_lock.cpp:363` |
| `Filename::~Filename` / `operator=` / ctor (diagnostic) | `0x180006980`, `0x180006770`, `0x1800067d0` | `standalone/filename_runtime_hook.cpp:392-424` |
| `Video::Device::InitializeInterfaces` + direct writes into the Device object at hardcoded slot offsets | — | `standalone/video_interface.cpp:111-140` |
| `user32!MessageBoxW` in Runtime's IAT (routes the CRT `_purecall` R6025 modal to a crash log) | — | `standalone/main.cpp:706-760`, `814-819` |

**B. Our code reads Runtime-internal layout directly.** `Object::Factory::_firstTypeInfo`
walked through a hand-declared 8-field `BaseTypeInfo` (`code/Game/Game/game_main.cpp:486`,
`standalone/main.cpp:523,545`); `System::Error::_errorString` as a raw 8 KB buffer
(`game_main.cpp:467`); the Modification `HMODULE` slot freed with `FreeLibrary`
(`game_main.cpp:365`); the allocator singleton deref (`game_main.cpp:575`,
`engine/funcs/engine_funcs.cpp:1630`); `Graphics::ParticleInstance::_frameIndex`
(`code/Game/Component/particlecomponent.cpp:995`); FileSystem/Modification singletons by
address (`code/Game/Game/filesystem_setup.cpp:113,190`, `code/Game/Dialog/mod_dialog_vtables.cpp`);
`kInternalFactorySingleton` (`code/Game/Object/internal_factory_audit.cpp:16`).

**C. Crash handler matches Runtime instruction offsets.** `standalone/main.cpp:518` (`+0xC0390…A0`,
the `IsDerivedFrom` null-this fault) and `:533` (`+0xBC73E`, `External::Load`).
~~Retire with Wave 3.~~ **WRONG, corrected by Wave 3 lane A: KEEP, narrowed.** Porting
`IsDerivedFrom` moves OUR callers into the exe, but no detour is installed, so the DLL's own
internal callers still reach `Runtime+0xC0390` and the handler still has to recognise the fault
for them. The match becomes retirable only when the function is detoured, or at cutover. A scope
note to that effect is now a comment at `standalone/main.cpp:521`.

**D. Vtables copied out of live Runtime objects.** The claim that began this entry — "No `??_7`
vtable symbol is ever resolved" — was read as "none CAN be". **That is false: the DLL exports 147
`??_7` vtable symbols** (UI 68, Graphics 20, Video 19, IO 15, Object 7, Engine 5, Achievement 3,
Audio 3, Collision2D 2, Pathfinding 2, Picking 1, System 1), verified with pefile against the
shipped DLL. Wave 2 used one for real: `System::Thread`'s ctor/dtor stamp
`??_7Thread@System@@6B@` (`0x180155f20`) resolved by name, instead of cloning slots. Two of the
classes listed below — `Graphics::MaterialInstance` and `UI::SpriteSheet` — have exported vtables
too, so their mirroring workaround was never necessary. **Caveat, do not over-read this:** an
exported vtable makes the ADDRESS available; whether stamping it is SAFE is a per-class argument.
Thread's was easy because it is an always-subclassed base whose slot 0 is `_purecall`. Re-make
that argument per class; do not assume it transfers. Our subclasses currently let the Runtime ctor
stamp the base vtable and then copy or patch slots:
`code/Game/Dialog/thumb_vtables.cpp:66-82`, `code/Game/Component/statusicon_geometry.cpp:328`,
`code/Game/Dialog/saveloaddialog.cpp:854`, `code/Game/Component/frustumoutlinedescription.cpp:89`.
A ported class must expose its vtable as a real symbol or reproduce the stamping order.

**E. Dead code.** `runtime/src/ui/spritesheet_debug.cpp` had zero callers and was removed
(2026-09-06). `sprite_debug.cpp` and `element_sprite_debug.cpp` are LIVE: `buildui.cpp`,
`buildrequirementui.cpp` and `toolbardialog.cpp` reach them through bare `extern` declarations,
which a filename grep misses. `runtime/funcs/runtime_funcs.cpp` still resolves nothing.

### 2.3 Runtime globals we reference (25 addresses; the "17 have no export" count is WRONG)

**Corrected by Wave 3 lane A.** Three rows of this table were wrong, and the error was in the
direction that overstates the cutover cost — each of these IS an exported data symbol that can be
resolved by name instead of by address:

| Address | This table used to say | Actually |
|---|---|---|
| 0x1801fc6e0 | `Object::Factory::_firstTypeInfo` | `?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA` — owned by **BaseTypeInfo**, not Factory |
| 0x1801fc6e8 | "Factory singleton (LoadDialogResource)" | `?_singleton@Factory@Object@@0PEAV12@EA` — note the `0PEAV12@EA` mangling; the obvious `1PEAV12@A` is a phantom |
| — | (absent) | `?_crcTable@CRC@System@@0PAIA` is exported too |

So **the "17 have no export" figure has never been measured and must be re-measured** before any
wave plans accessors around it. The table below still carries the old names; treat the three rows
above as the correction rather than editing the grid in place, so the old claim stays recognisable.

| Address | Name | Address | Name |
|---|---|---|---|
| 0x1801faa88 | `UI::Dialog` type id | 0x1801fc720 | `System::Error::_errorString` |
| 0x1801fc678 | `Application::_singleton` (alt ref) | 0x1801fe720 | `Timer::_ticksPerSecond` |
| 0x1801fc6a8 | `IO::FileSystem::_singleton` | 0x1801fe728 | `Timer::_secondsPerTick` |
| 0x1801fc6b0 | `IO::Modification::_singleton` | 0x1801fe730 | `Timer::_systemTicksPerSecond` |
| 0x1801fc6b8 | `IO::Persistence::_singleton` | 0x1801fe738 | `Timer::_systemInitialTick` |
| 0x1801fc6c8 | `Object::ExternalFactory::_singleton` | 0x1801fe750 | `Engine::MainLoop::_singleton` |
| 0x1801fc6d0 | Modification DLL `HMODULE` | 0x1801fe758 | `Audio::Configuration` singleton |
| 0x1801fc6d8 | `Object::InternalFactory::_singleton` | 0x1801fe77c | `ParticleInstance::_frameIndex` |
| 0x1801fc6e0 | `Object::Factory::_firstTypeInfo` | 0x1801fe7a0 | `Video::Configuration` singleton |
| 0x1801fc6e8 | Factory singleton (LoadDialogResource) | 0x1801fe7a8 | `Video::Device::_singleton` |
| 0x1801fc718 | `System::MemoryAllocator` singleton | 0x1801fe7b8/c0/e0 | `g_pLoader`, `g_pLoaderFlags`, `g_pLoaderCompletionEvent` |
| 0x1801feb80 | `Application::Application::_singleton` | | |

Full detail: `survey_call_surface.md` and `call_surface_20260906.csv` in this directory; the
Ghidra census is `survey_ghidra_census.md`, the exe census `survey_exe_census.md`.

---

## 3. What Runtime-x64.dll is (live Ghidra, 2026-09-06)

### 3.1 Census

15,109 functions iterated (15,290 with thunks), 1,209,608 bytes of bodies in a 1,349,120-byte
`.text`. 4,633 export entry points. **2,467 `FUN_`-named internals holding 407,017 bytes, a third
of all code**, plus 7,761 global-named functions (170,729 bytes) that are almost entirely CRT
auto-matches and the MSVC demangler (`UnDecorator`/`DName`, 70 functions).

| Namespace | Classes | Functions | Body bytes |
|---|---|---|---|
| UI | 73 | 1,625 | 193,399 |
| Graphics | 40 | 599 | 93,712 |
| Video | 35 | 573 | 54,751 |
| IO | 21 | 407 | 73,426 |
| System | 18 | 262 | 28,289 |
| Math | 10 | 228 | 21,503 |
| Object | 13 | 191 | 23,318 |
| Audio | 6 | 137 | 15,651 |
| Achievement | 6 | 118 | 8,078 |
| Engine | 7 | 108 | 14,817 |
| Collision2D | 3 | 76 | 18,051 |
| Volume | 5 | 64 | 8,589 |
| Input | 3 | 63 | 9,129 |
| Picking | 2 | 49 | 14,039 |
| Pathfinding | 3 | 45 | 13,086 |
| Save | 2 | 37 | 12,723 |
| Application | 1 | 26 | 2,450 |
| Utility | 2 | 16 | 5,029 |
| Debug | 4 | 8 | 28 |

Identified engine namespaces total about 610 KB; UI is 32% of that. Every namespaced function is
exported (4,740 namespaced vs 4,633 exported), so **the non-exported engine code IS the `FUN_`
pool**, and that is where the port risk lives. Project-authored names in the Runtime database
number about a dozen (`Map_Insert`, `Map_Find`, `DynArrayPush`, `ModArrayCopy`, …). The DLL is
essentially un-annotated.

### 3.2 Layering (from a full cross-namespace call-edge matrix; nearly acyclic)

| Tier | Namespaces | Depends on |
|---|---|---|
| 0 | Math, Volume, Debug, Utility | nothing engine-side (Volume→Math 1 edge) |
| 1 | System | KERNEL32 + CRT only |
| 2 | IO, Object | System (IO 238, Object 103); Object→IO 35 |
| 3 | Collision2D, Picking, Pathfinding | System (~11 each), trace of Object; Picking adds Math/Volume |
| 4 | Video, Graphics, Audio, Achievement, Input, UI | System + Object; Graphics→Video 73; UI→System 401, →KERNEL32 472 |
| 5 | Save, Engine | everything; Engine is the only true integrator |
| — | Application | 26-function USER32 shim called by Input and Engine; treat as a Tier-1 leaf |

### 3.3 Foundation types by inbound call edges

| Class | Fns | Bytes | Inbound edges |
|---|---|---|---|
| `System::String` | 48 | 6,333 | 2,388 |
| `System::MemoryAllocator` | 30 | 4,169 | 1,778 |
| `System::Filename` | 18 | 2,737 | 986 |
| `Object::External` | 23 | 4,397 | 624 |
| `Object::Base` | 10 | 387 | 363 |
| `Object::ExternalFactory` | 37 | 10,952 | 144 |
| `System::WideString` | 29 | 2,946 | 108 |
| `IO::FileSystem` | 54 | 14,562 | 107 |
| `IO::Serializer` | 30 | 414 | 89 |

**`Collection::Array` / `Set` / `LinkedList` / `Map` / `DefaultAllocator` are header-only
templates inlined into every consumer.** Zero exported symbols define them; they appear only inside
other mangled names. Their layouts must be recovered from consumer field access and getting them
wrong silently corrupts every class that embeds one. The out-of-line remnants are the three
hand-named helpers `Map_Insert`, `Map_Find`, `DynArrayPush`.

### 3.4 The profile build — the biggest lever we have

`ToolKit/bin/x64/Runtime-x64-profile.dll` is **now imported into the Ghidra project** (16,002
functions, current program as of 2026-09-06). Compared to retail: 5,182 exports vs 4,926, `.text`
1,669,714 vs 1,349,120 bytes, and a CodeView record naming
`C:\Projects\Buildy\bin\x64.Profile\Runtime-x64-profile.pdb`. Its strings recover the **original
source tree: 207 files, 116 `.cpp`** (`original_source_tree.txt` in this directory, produced by
`strings -n 8` over the profile DLL filtered for `code\Runtime\…` paths and `Dir\File.cpp` names). Directory counts: UI 31 cpp / 38 h, Graphics 18/10, IO 17/4, Video 11/9,
Object 7/6, System 7/2, Audio 5/5, Engine 5, Input 3/2, Utility 3/2, Achievement 2/3,
Pathfinding 2/1, Save 2, Collection 0/2, Collision2D 0/2, Picking 1/1, Volume 1/1,
Application 1/1. Files named `*_compile.cpp` are the offline asset compiler paths and are
likely absent from retail. `runtime/src/` mirrors this tree, exactly as `code/Game/` mirrors the
exe's.

The exe port proved the method (`ToolKit/PROFILE_ANALYSIS.md`): string xrefs in the profile
binary name the function, then `normalized_opcodes_sha256` hash matching maps it to the retail
address. The four `*_hash_index.json` files at the repo root cover the exe only; the same
ghidra-mcp tools (`build_function_hash_index`, `bulk_fuzzy_match`) need to be run for the two
Runtime programs.

---

## 4. Strategy

### 4.1 Decisions (confirmed by the user 2026-09-06)

1. **Endgame is static linking into `reimpl-x64.exe`, not a drop-in `Runtime-x64.dll`.** The exe
   is ours, so nothing needs the DLL's export table. Building a replacement DLL would force MSVC
   name mangling on a GCC build for no consumer. Cutover = stop `LoadLibrary("Runtime-x64.dll")`.
   **The stated reason is portability: the finished exe should run on non-Windows systems.**
   That adds a rule for every wave: Win32 stays behind a platform seam. The original tree already
   draws the line for us with its `_win` files (`Engine_win.cpp`, `AudioDevice_win.cpp`,
   `SoundBuffer_win.cpp`, `Registry_win.cpp`, `WideString_win.cpp`, `ImageBuffer_win.cpp`,
   `IndexBuffer_win.cpp`, `VertexBuffer_win.cpp`, `ShaderProgram_win.cpp`, `VideoDevice_win.h`).
   Port those as `runtime/src/<Dir>/<File>_win.cpp` with a header that a `_posix`/`_sdl` twin can
   implement later; never let `<windows.h>` leak into a non-`_win` file. Known Windows-only
   surfaces to plan around: `IO::Persistence` is registry-backed (`HKCU\Software\Shining Rock
   Software LLC\Banished`), `Input::Device` is DirectInput, `Application` is a USER32 window shim,
   the renderer is DX11 (`dx11/`; a second backend is a separate project after cutover), the
   `System::Thread`/`CriticalSection`/`Timer` family wraps KERNEL32 and thread-pool timers.
   Byte-faithful MSVC-ABI behaviour (hidden return slots, struct layouts) matters only while the
   DLL is still loaded; after cutover the layouts are ours to keep or drop.
2. **Progressive, same seam as the exe port.** `ResolveExport`/`ResolveAddr(Module::Runtime, …)`
   consult a **runtime registry first** (`RegisterRuntimeExport(name, ours)`,
   `RegisterRuntimeAddr(addr, ours)`). Our ~1,180 call sites flip the moment a port registers.
3. **Detour the DLL for its internal callers.** Unlike the exe, the DLL's internal call graph
   never goes through exports, so registering a port redirects only us. To A/B a port against
   *all* callers, inline-patch the DLL function to `JMP ours` (the `allocator_lock.cpp` pattern;
   the full hde64-based installer with trampolines lives in git at `fcd3fb6^:patches/src/hooks.cpp`
   and should be revived as `standalone/runtime_detour.{h,cpp}`). **Done in Wave 0** — note that
   `standalone/runtime_detour.h` already existed, declaring the simpler byte-copy
   `RuntimeDetourInstall` implemented in `filename_runtime_hook.cpp` and used by three files.
   Both APIs now live in that header; use the HDE64 `InstallRuntimeDetour*` unless you have read
   the prologue and want the strict expected-bytes guard. Env kill-switch per port,
   default-on once proven, exactly like `kDefaultOnFlags`.
4. **Differential testing is the oracle.** The DLL stays loaded until cutover, so every pure port
   can run both implementations and compare (`BANISHED_RT_DIFF=1`: call ours, call theirs, compare
   outputs/bytes, log the first divergence with a stack). The exe port never had an in-process
   oracle; this one does. Use it for Math, System, Collection, IO codecs, Collision, Pathfinding.
5. **Binary-compatible layouts, DLL globals by address until the owner is ported.** Every struct
   in `runtime/types/` gets `static_assert` offsets. Stateful subsystems (allocator, factory/handle
   table, filesystem, UI manager) switch **atomically** as a class: ctor + vtable + every method
   together. A class ported half-way with two vtables in play is the swap-remove UAF family again.
6. **MSVC-ABI faithfulness rules carry over unchanged**: hidden return slots for String-returning
   members, 5th-arg-onward pointer widths, comma-assignment loop conditions, `_InitFuncs` with
   zero callers. Recall the memory notes before porting anything that returns a String.

### 4.2 Wave order (Tier order from §3.2, sized from §3.1)

| Wave | Scope | Size | Why here |
|---|---|---|---|
| **0 Tooling** — **LANDED 2026-09-06, see [WAVE0.md](WAVE0.md)** | Runtime registry seam; detour installer; diff harness; profile↔retail hash index for the two Runtime programs; `runtime/src/<dir>/` skeleton; `tools/runtime_census.py`. All four mechanisms proved on a live process by `standalone/rt_selftest.cpp`, and a registered **null port** carried 16,027,547 `System::Random::Rand` calls through a full autostart game with a clean exit. **The `FUN_` pool is MEASURED AND UNSOLVED** — the map yields 9 rows naming a retail `FUN_` (none by hash) and the string-anchor pass yields 66 real anchors with 0 confirmed bridges to retail. See §4.3 risk 4, rewritten. | 1–2 sessions | Everything after depends on it |
| **1 Math + Volume** — **LANDED 2026-09-06, see [WAVE1.md](WAVE1.md)** | 70 of the 71 functions our tree calls (Math 58/59, Volume 12/12); the one deferral is `Graphics::InterpolatedState<Transform>::Lerp`, whose layout nothing in this wave pins. **6,556,631 diffed calls, 0 divergences** against the DLL under `BANISHED_RT_LIBM=dll`; 90 residual 1-ULP divergences at 2 sin/cos sites under our own libm. Found 3 real defects — a **pre-existing wild write** (`fn_Math_Transform3_Identity` declared with no hidden return slot, scribbling 0x30 bytes through a stale RCX on every menu scene load), a reassociated float sum in `Transform::FromDirection`, and 4 exports resolved under manglings that do not exist in the DLL. | S | Pure, zero deps, diff-testable to the bit; proves the seam + harness |
| **2 System + Collection** — **LANDED 2026-09-06, see [WAVE2.md](WAVE2.md)** | 53 of the 60 System functions our tree calls (String 18, Filename 11, Random/Time/DateTime/Color/Configuration 17, CriticalSection/Thread 8), plus the Collection layouts as headers ([WAVE2_COLLECTION.md](WAVE2_COLLECTION.md)) and MemoryAllocator reversed, ported and left **OPT-IN with a NO-GO verdict** ([WAVE2_ALLOCATOR.md](WAVE2_ALLOCATOR.md)). **22,733,915 diffed calls, 0 new divergences** — including 16,035,064 `Random::Rand` calls bit-exact. WideString/CRC/Compression/Timer are not called by our tree and were not ported. | M | 4,100+ inbound edges; the allocator is global state and is the one piece NOT taken live |
| **3 Object** — **LANDED 2026-09-06, see [WAVE3.md](WAVE3.md)** | 22 of the 41 Object functions our tree calls (type system 6, handle table 5, external table 11), across three lanes plus a perf lane. **38,613,179 diffed calls, 0 new divergences**; the handle-table shadow oracle ran clean at 93,387 calls each for `Internal::Internal`/`~Internal`. Four `ExternalFactory` mutators are NOT ported (no oracle possible / Wave 4 dependency) and five handle-table mutators are opt-in with a **NO-GO**. Found that the Wave 2 perf breach was ONE defect (`RtOriginal` is `GetProcAddress`, called inside `System::Time`'s body), that WAVE2.md's perf attribution was backwards, and that `CheckPtr` is a bare `RET` in the profile build too. **The vtable-ownership worry in this row did not materialise**: `??_7Internal`/`??_7Base` are exported, so the port stamps the DLL's own vtable — a per-class argument, not a general one. | M | The reflection + handle table |
| **4 IO** — **LANDED 2026-09-06, see [WAVE4.md](WAVE4.md)** | 39 of the 41 IO functions our tree calls (Serializer 22, CompressedMemoryStream 6, Persistence 5, FileSystem/Modification 6); **21,672,859 diffed calls, 0 IO divergences**. Also FIXED the `BANISHED_AUTOLOAD` harness this plan had blocked on since Wave 2, and disproved three recorded facts about `IO::Serializer` (the ctor's `version` is NOT ignored, the factory-slot arity IS 3, and `Serializer+0x08` is a `ReaderWriter*`). The 5% perf gate is BREACHED at +7.7%/+7.5% and DEFERRED BY THE USER to after the final stage. Scoped to the 39 IO symbols our tree actually calls, which is far less than this row implied. "Stream family" is 6 exports on ONE class, `IO::CompressedMemoryStream`, all thin shims over `IO::CompressedStream`; the codec, `FileStream`, `BinaryFileStream`, `TextFileStream`, `VisitorStream`, `CompressedFileStream` and `CompressedCopyFileStream` are untouched. **`PackageFile` and `BinaryReader/Writer` are NOT in this wave at all** — our tree never calls them, so the "densest class in the DLL" note describes work no wave has yet needed. Lanes: A Serializer (19), B CompressedMemoryStream (6), C Persistence (7), D FileSystem+Modification (7), H the `BANISHED_AUTOLOAD` harness | M | Encoding vtable offsets differ between retail and the profile build — see the warning in `runtime/src/io/rt_io_port.cpp` |
| **5 Algorithm packages** — **LANDED 2026-09-06, see [WAVE5.md](WAVE5.md)** | 58 of the 74 functions our tree calls: Achievement 34 (Goal 13 + Manager family 21), Save 15, Picking 9/9. **Collision2D and Pathfinding are TRUE ZEROES** — our tree calls neither, and pathfinding is reimplemented natively in the exe (`code/Game/Action/action_pf_*.cpp`), so this row's five namespaces are really three. 14 runs, the largest 160,416,395 diffed calls, **0 divergences in every Wave 5 site**. Found 2 live defects (4 phantom `Goal` manglings, one of which skipped the InternalFactory swap-remove before a `MemFree`; a `SavedState` fallback pointing 0x90 bytes INTO another function), a pre-existing mods-menu crash, and **an uninitialised read in the shipped DLL** (`RemoveMesh` leaves `Entry::_userValue2` uninitialised). Also fixed three census blind spots. | M each | Self-contained, each ~13–18 KB, only System/Object below them |
| **6 Video + Graphics** — **LANDED 2026-09-07, see [WAVE6.md](WAVE6.md)** | 113 of the 148 functions our tree calls (Video 53/69, Graphics 59/87), across 7 lanes. **Two FINAL runs, 25,858,610 and 49,643,611 diffed calls, 0 Wave 6 divergences** — every remaining divergence is the pre-existing Wave 1 1-ULP family. Found 5 port defects and 2 oracle-design defects, plus a LIVE functional gap that was not a port: `Geometry::Attach`/`Detach` were spelled `AEAV12@` where the DLL only exports `PEAV12@`, `ResolveExport` has no address fallback, and both call sites are `if (ptr)`-guarded — so every attach and detach in `action_base_complete.cpp` had been silently dead since it was written. **This row's premise about the detour was WRONG in our favour**: `Video::Context` has no vtable and `??_7Geometry` is exported, so no detour was needed and none was installed. `Renderer::Draw` is held back and wants its own lane. | L | Graphics→Video 73 edges, port Video first |
| **7 UI** — **LANDED 2026-09-07, see [WAVE7.md](WAVE7.md)** | 101 of the 147 functions our tree calls, across 6 lanes (Element 21/23, Dialog family 14/28, UserInterface 8/19, input widgets 18/27, layout widgets 20/24, sprite/text 20/23). **Two closing runs, 26,280,535 and 50,848,191 diffed calls, 0 Wave 7 divergences** — every residual is the Wave 1 1-ULP family. The integration run DID find 2 new sites (`SpriteSheet` ctor and `AddSpriteInfo`, both 100% divergent); both were the ORACLE comparing bytes neither side writes, root-caused from disassembly and closed with no ported body changed. **This row's premise about vtables was wrong in our favour**: all 68 UI vtables are exported, so a ported ctor stamps the DLL's own — the `??_7Geometry` lever from Wave 6, general across the namespace. `UserInterface` turned out to have NO vtable at all. Six pre-existing defects fixed at source, five latent-but-live. **The detour was again not needed and the count is still 0 across all 4,632 functions.** | XL | Element by element; DialogInstance and UserInterface last |
| **8 Top** — **LANDED 2026-09-07, see [WAVE8.md](WAVE8.md)** | 49 of the 81 functions our tree calls across six namespaces (Engine 15/25, Audio 13/22, Input 15/21, Application 5/6, Save remainder 2/5, Utility 1/2), in 5 lanes; **32 held back with written reasons**, which is the highest holdback ratio of any wave and the correct outcome for a wave sitting on the allocator and the OS. **Two runs, 28,191,797 and 50,870,488 diffed calls, 0 Wave 8 divergences**; a third run under `BANISHED_RT_LIBM=dll` drives the WHOLE TREE to 0, which finally proves the Wave 1 1-ULP residual is libm rounding and not port logic. **THIS ROW'S PREMISE ABOUT THE DETOUR WAS RIGHT WHERE WAVES 6 AND 7 WERE WRONG, AND THE ANSWER IS STILL NOT AN INSTALLED DETOUR**: `MainLoop::OnVideoError`/`OnVideoRestart` each have exactly ONE internal caller (the static thunks the `MainLoop` ctor installs into `Video::Device::_singleton` +0x50/+0x58, reached only on device lost/restore), so a port that registered the export and stopped would be dead code reporting `calls=0 divergences=0`. Both held back; the count is still 0, but now because the case was FOUND AND DECLINED. The allocator NO-GO turned out to cover FOUR functions, not the ctor/dtor pair this plan predicted — `Engine::Startup` writes three allocator fields directly and `Shutdown` calls the allocator's shutdown. **Phantom export names went 3 → 0 tree-wide for the first time in the project**; all three were Wave 8 names and two had been diagnosed by Wave 5 and never fixed at the source. Six defects fixed at source, including a `tools/rt_run.sh` `tail -80` that MANUFACTURED `calls=0` for 1,100 of 1,200 oracle lines. | M | Integrators; mostly wiring |
| **9 Cutover** — **SCOPED 2026-09-07, see [WAVE9.md](WAVE9.md); the "S" below was wrong** | Remove `LoadLibrary` of Runtime; retire the §2.2 patches; VideoDX11 is already ours (`dx11/`). **The honest gap is 319 items, not a `LoadLibrary` call**: 145 functions, 57 unnamed DLL internals by address, 51 ctors/dtors, and **59 exported DATA symbols (28 `??_7` vtables, 17 `?_singleton@`, 14 `?_typeInfo@`) that have no body to reimplement and must be MINTED**. Three things every earlier wave was allowed to defer all come due here at once: the allocator NO-GO (a heap cannot be half ours), the `FUN_` pool (§4.3 risk 4, closed to automation), and every wave-3-to-8 holdback. **And the whole verification apparatus dies at cutover** — `BANISHED_RT_DIFF`, `RtOriginal`, `BANISHED_RT_PORT=0` and the Wave 6/7 "stamp the DLL's own exported `??_7`" lever all need the DLL to be there, so nothing may be ported in this wave that was not diff-verified BEFORE the DLL goes. That inverts §6 step 6 for this wave and forces the lane order. The one piece of good news is that **the detour question becomes moot**: with no DLL running its own code, "the DLL's own internal callers" stops being a category and the count stays 0 forever. | **XL** (not S) | Nothing left to port against |

~~Waves 1–2 are the proving ground; do not parallelize past Wave 2 until the seam, the detour and
the diff harness have each caught at least one real divergence.~~
**Superseded, and worth stating plainly rather than deleting: waves 3 and 4 BOTH parallelized
(3 lanes and 5 lanes) without that condition ever being met.** Two of the three did prove
themselves — the seam carries every port, and the diff harness has caught real defects in every
wave since (Wave 1's three, Wave 4's two oracle defects). **The detour has caught nothing,
because no detour has ever been installed on a port**: the census `detour` column is 0 across all
4,632 functions, and the only `InstallRuntimeDetour*` call in the tree is Wave 0's self-test.
That is not a failure — every wave so far has been redirectable through the registry alone, since
our own call sites are what matter — but it does mean **the "DLL's own internal callers" problem
(§4.1 decision 3) remains completely unexercised**. The first wave that ports something the DLL
calls internally will be using that mechanism for the first time, in anger, with no prior
evidence it works beyond one self-test.

**Wave 6 was expected to be that wave, and it was not.** It ported `Video::Context`, which the
DLL's own `Graphics::Renderer`, `UI::Begin` and `MainLoop::Draw` call internally — the exact
shape this note describes. It still needed no detour, for two reasons worth carrying forward:
`Video::Context` has NO vtable, so our port and the DLL's code operate on one live object with no
dispatch ambiguity as long as the layout is faithful; and where a vtable was involved
(`Graphics::Geometry`), `??_7Geometry@Graphics@@6B@` is EXPORTED, so the ported ctor stamps the
DLL's own vtable byte for byte instead of minting one. The detour count is still 0 across all
4,632 functions. The mechanism remains unproven in anger, and the first wave that ports a
*stateful* class the DLL drives internally will still be its first real user.

### 4.3 Risks, ranked

1. **Collection template layouts** (inlined, no ABI). **MEASURED IN WAVE 2**, see
   [WAVE2_COLLECTION.md](WAVE2_COLLECTION.md) and `runtime/types/collection/Collection.h`.
   This entry used to say "`Map` is the AVL tree behind `Map_Insert`/`Map_Find`".
   **There is no `Collection::Map`.** `Map_Insert`/`Map_Find` are names WE invented in
   Ghidra. The mangled template ids across the 4,926 exports are `InternalPtr` 103,
   `ExternalPtr` 94, `Array` 44, `InterpolatedState` 57, `HashGrid` 4, `Set` 2,
   `LinkedList` 1, `Map` **0**; `PackageFile::BuildPackageFile`'s signature spells the
   real shape, `Set<Filename, DefaultAllocator, Filename, AVLCompare>` — a four-parameter
   `Set<T, Alloc, Key, Compare>`, which is exactly why a "map" here is `Set<Pair<K,V>>`
   keyed on the pair's first. `Array<T>` IS the 16-byte `{_data, _size:i32, _capacity:i32}`
   triple. Recovered with HIGH confidence: `Array` 0x10, `Set` 0x10, `Set::Node`
   `align8(sizeof T)+0x20`, `LinkedList` 0x18, `LinkedList::Node` `align8(sizeof T)+0x10`.
   `Set::Iterator` is NOT pinned and is labelled a guess in the header.
   **Trap for every later wave:** node pointer offsets are a function of `sizeof(T)`, so
   MSVC emits a separate insert/find/rebalance per instantiation — one ported `Set` body
   cannot serve two element widths (`reference_one_bst_find_two_node_layouts.md`).
2. **The allocator.** **REVERSED IN WAVE 2**, see [WAVE2_ALLOCATOR.md](WAVE2_ALLOCATOR.md).
   This entry was wrong twice over. It is NOT bucketed free lists: it is a **single-arena
   boundary-tag allocator with 192 segregated first-fit bins**, `sizeof == 0x670`, and it is
   **embedded at offset 0 of `Engine::Engine`** (`Engine::GetMemoryAllocator` is a bare
   `return this`). And the `str-4` / `str-8` pair is **`System::String`'s header, not the
   allocator's** — `String::SetLength` (`0x1800c8220`) allocates `len+9` and sets
   `data = user+8`, so `data-8` is the LENGTH, `data-4` is the refcount, and the allocator's
   own boundary tag is a further 8 bytes back at `data-0x10`. The allocator header carries no
   refcount. (`filename_runtime_hook.cpp`'s own comments were right; this paraphrase of them
   was not.) The whole private API is exported — `GetBinForSize`, `GetFreeTagForSize`,
   `LinkFreeTag`/`UnlinkFreeTag`, `AddNewMemoryRange`, `CommitToPage`/`ReleaseToPage`,
   `PlatformAllocate`/`PlatformFree`, and `_singleton` as a data symbol — so the struct names
   `MemoryRange`/`MemoryTag`/`FreeTag` and the Win32 seam are given, not guessed.
   The risk is also MILDER than stated: the port manipulates the LIVE singleton's own tags,
   bins and ranges, so "we allocate from our heap while the DLL frees into its own" cannot
   happen — one heap, one metadata set. **Status: ported, opt-in, verdict NO-GO** — the
   shadow proof has never executed, and an unmeasured port is not a port.
3. **Vtable ownership.** Until Wave 3 lands, objects built by retail ctors carry DLL vtables and
   virtual calls land in the DLL regardless of what we registered. Port classes whole.
4. **The `FUN_` pool.** 407 KB of un-named internals. **This entry was wrong and is now
   measured** (Wave 0, `docs/runtime_port/WAVE0.md`): the profile build + hash matching is NOT a
   way through it. Hash/fuzzy matching yields **9** rows that name a retail `FUN_` (none
   byte-provable), because Ghidra names from the export table and the profile build's internals
   are `FUN_` too — unnamed matched to unnamed. The string-anchor pass yields **66** real
   attributions of which **0** bridge to retail, and the two levers are anti-correlated: the
   substantive functions worth anchoring are the ones whose codegen differs most between builds,
   so they are the least likely to match. No PDB ships. The untried lever with a real mechanism
   Call-graph propagation was then tried too, and it is the cleanest of the three — 46 pairs, all
   at arity 1/1, 8/8 verified by decompiling both sides, 0% false positives — but it transfers
   **zero names**, because both sides of every pair are `FUN_`; what it establishes is cross-build
   address equivalence. No PDB ships (checked). **Tally across all three levers: ~50 of 2,467
   touched (2%), 9 names proposed, none verified.** The pool will not close by a bulk automated
   pass; it closes by ordinary decompile-and-read work on the functions a wave actually needs,
   which is bounded per wave. Budget that cost; do not plan a wave as though the naming were
   solved.
5. **CRT/SEH inside the DLL.** The DLL has its own CRT and `__C_specific_handler`; a port that
   throws or unwinds across the DLL boundary is undefined. Keep exceptions out of ported code.

---

## 5. Definition of done for the runtime phase

- [x] Wave 0 tooling merged; `tools/runtime_census.py` reports registered / called / total per namespace.
      Baseline 2026-09-06: **869 functions called, 0 ported** (UI 137, Graphics 78, Math 60,
      Video 60, System 57, IO 39, Object 38, Achievement 37, Input 21, Audio 20, Save 20,
      Engine 19, Volume 12, Picking 8, Application 5, Utility 2, plus 245 non-namespaced
      internals, 7 ordinals and 4 names the census cannot attribute).
- [x] **Wave 1 (Math + Volume) merged.** Census after the wave: **868 called, 70 ported (8.1%)** —
      Math 58/59 (98.3%), Volume 12/12 (100%), everything else still 0.
- [x] **Wave 2 (System + Collection) merged.** Census after the wave: **873 called, 139 ported
      (15.9%)** — System 53/60 (88.3%), Math 58/59, Volume 12/12, plus 16 non-namespaced internal
      addresses. The 7 System functions still unported are `MemoryAllocator::MemAlloc`/`MemFree`
      (opt-in, NO-GO), `Thread`'s vtable and `ThreadFunc` (used from the DLL as-is, deliberately),
      and `MemoryAllocator::_singleton`. 27 further registrations exist in `allocator.cpp` /
      `allocator_win.cpp` and are **excluded from the count** because their Register function
      returns 0 unless armed — `tools/runtime_census.py` reports them separately.
- [x] **Wave 4 (IO) merged.** Census after the wave: **879 called, 217 ported (24.7%)** —
      IO 36/41 (87.8%), Object 27/41, System 53/60, Math 58/59, Volume 12/12. 39 of the 41 IO
      functions our tree calls are ported; the two that are not are `Persistence::ReadSettings`
      and `WriteSettings` (they mutate the real HKCU registry and `WriteSettings` deletes keys,
      so no safe oracle exists), plus `FileSystem::_singleton`, a DATA export that cannot be
      registered before cutover. Diff oracle: **83 sites, 21,672,859 calls, 0 IO divergences**.
      Wave 4 also FIXED the `BANISHED_AUTOLOAD` harness this row used to be blocked on, and
      corrected three recorded facts about `IO::Serializer` that were wrong. See
      [WAVE4.md](WAVE4.md).
- [x] **Wave 3 (Object) merged.** Census after the wave: **877 called, 170 ported (19.4%)** —
      Object 21/41 (51.2%), System 53/60, Math 58/59, Volume 12/12.
      **THOSE TWO NUMBERS WERE AN UNDERCOUNT, corrected during Wave 4 to
      Object 27/41 (65.9%).** `type_system.cpp` registered its six ports through
      `kName*` constants, and `tools/runtime_census.py` matches
      `RegisterRuntimeExport("<literal>"` and cannot see through a named
      constant. The ports were live in the process the whole time and invisible
      to the tool this project treats as its authority over any agent's
      "all N ported" claim. Found only because Wave 4 lane D repeated the
      pattern. The census now WARNS on a non-literal registration name, so this
      cannot recur silently; the registrations are spelled as literals. The Object functions still
      unported are the 4 `ExternalFactory`/`External` mutators (deliberate, no oracle), the 5
      handle-table mutators (opt-in, NO-GO), `CheckPtr` (deliberate, see WAVE3.md finding 3) and
      the exported DATA symbols, which cannot be registered before cutover.
- [x] **Wave 5 (Achievement + Save + Picking) merged.** Census after the wave: **727 called, 248
      ported (34.1%)** — Achievement 34/50, Save 15/20, Picking 9/9 (100%), IO 36/52, Object 28/43,
      System 54/68, Math 58/61, Volume 12/12. **Those denominators moved because the census itself
      was fixed, not because the tree changed**: it was bucketing every address-called function
      under a pseudo-namespace `internal` (262 rows) and counting a function reached by BOTH a name
      and an address TWICE. Resolving addresses through the shipped export directory dropped
      `internal` to 35 and TOTAL called from 898 to 727. The orphan list — the one whose whole job
      is catching `reference_uncalled_port_is_not_verified.md` — went from 11 exports + 72 addresses
      to 7 + 0, because every deliberate both-doors registration had been reporting caller-less
      forever. See [WAVE5.md](WAVE5.md).
      Four ports are registered, reachable and **NOT verified**, each with a written reason:
      `Scene::SetMeshUserData` (both call sites guard on an already-registered mesh),
      `SavedState::SavedState` (its only path crashes upstream in a defect proven pre-existing by a
      `BANISHED_RT_PORT=0` A/B), `~Achievement` (reachable only via a Scenario vtable slot the DLL
      invokes on resource release, and scenarios live for the process), and `SaveLoad::Update` cases
      3 and 5. Seven more are held back with reasons, four of them because constructing an
      `IO::Serializer` emits a 12-byte stream header and admits no oracle.
- [x] **Wave 6 (Video + Graphics) merged.** Census after the wave: **756 called, 360 ported
      (47.6%)** — Video 53/69 (76.8%), Graphics 59/87 (67.8%), Picking 9/9, System 54/68,
      Math 58/63, Volume 12/13, IO 36/52, Achievement 34/50, Object 28/44, Save 15/20.
      Two final runs, **0 Wave 6 divergences** in 25.8M and 49.6M diffed calls, both with a
      zero-byte crash log. 86 of the 113 ports executed in the autostart run; 2 of the 7 EdgeMesh
      ports executed under `BANISHED_TOOL_HARNESS=select`, and the remaining five were closed by
      `BANISHED_TOOL_HARNESS=pasturefull` — a pasture has no per-cell mesh variant, so it takes
      `tool_base_render.cpp`'s BUILD branch instead of its PREBUILT one (209 sites, 26,237,724
      calls, 0 new divergences). **All 113 ports have now executed** except the
      `EdgeMeshInstance` COPY ctor, which this wave's own `engine_funcs` fix left with no caller
      at all. Wave 6 also added `BANISHED_SCREENSHOT_AT=<frame>`, the first thing in the tree that
      can answer "is it actually on the screen" — the diff oracle and both censuses stop at the
      GPU boundary.
      Held back with written reasons: `Renderer::Draw` + `AddDrawItem` (they pin `Video::Context`
      and `Geometry` layouts two lanes were porting concurrently — PLAN §4.3 risk 3, verbatim),
      `Video::Device` `Startup`/`Shutdown`/`Recreate`/`ReleaseDynamic` and `RenderBuffer`
      ctor/dtor (real D3D11 device + a Win32 critical section, no oracle in either direction),
      `MeshInstance::AddDrawItems` (`standalone/scene_watch.cpp` resolves its ADDRESS as an
      identity token to recognise a MeshInstance — registering it would silently blind
      `BANISHED_LOD_CENSUS`, the `0x1400f2f00` shape from §1.1), `AnimationManager::Update` /
      `Serialize`, `FrameCopyImageBuffer`, `ImageBuffer::SaveImageToFile`, and the exported DATA
      symbols. See [WAVE6.md](WAVE6.md).
- [x] **Wave 7 (UI) merged.** Census after the wave: **766 called, 458 ported (59.8%)** —
      UI 98/149 (65.8%), Math 58/63, Volume 12/13, Picking 9/9, Video 53/69, Graphics 59/87,
      System 54/71, IO 36/52, Achievement 34/50, Object 28/44, Save 15/20.
      Two closing runs, **0 Wave 7 divergences** in 26,280,535 and 50,848,191 diffed calls, both
      with a zero-byte crash log; every residual is the Wave 1 1-ULP family.
      **101 functions registered** across 6 lanes (the process prints `[RtPort] UI: 101`; the
      census's 98 is the *called*-surface subset — both correct, different denominators).
      **Coverage was total on the save path: zero `calls=0` sites.** The one exception across both
      paths is `UI::EditElement::SetText`, registered and never exercised — its only live caller
      is `SpriteCaptureUI::Serialize`, a dev tool neither harness reaches.
      The integration run FOUND 2 new 100%-divergent sites (`SpriteSheet` ctor, `AddSpriteInfo`)
      and both were the ORACLE comparing bytes neither side writes — closed by narrowing the
      compare extent to the writer's final store, read out of the shipped binary and verified
      twice. No ported body changed.
      **Six pre-existing defects fixed at source**, five latent-but-live: `fn_Element_SetPadding`'s
      argument order (retail is left/RIGHT/TOP/bottom); `EditElement::SetText`'s fallback
      resolving to `UI::TextElement::SetText`, a different class; `fn_DialogInstance_AddController`
      silently bound to `Element::AddChild` by an always-firing fallback; a `Dialog::GetReference`
      mangling naming a class (`DialogReference`) that exists nowhere in the 4,926 exports; a
      `GetController` type-id filter of `0` in `Toolbar_CenterOnEntity_Impl` that had **survived
      its own fix note** (see `reference_getcontroller_third_arg_is_a_type_filter`, now on its
      third occurrence); and `runtime/types/ui/String.h` compiling only by accident of include
      order. Also corrected `Collection.h`'s `LinkedList` size comment (said 0x10 where the same
      block says 0x18) — in the file §4.3 risk 1 ranks the highest layout risk in the port.
      **Two premises in the wave brief were WRONG and the lanes disproved them**: a phantom export
      name is NOT automatically a dead call site (the call sites carry their own
      `ResolveAddr(Module::Runtime, …)` fallback, so three of the six were benign), and a
      written-extent constant is a property of a WRITER, not of a type — the same field is
      correctly excluded from one oracle and included in another.
      **New project-wide hazard, and the most transferable thing the wave produced:**
      `ResolveAddr(Module::Runtime, addr)` consults the port registry BEFORE the module handle
      (`standalone/resolve_standalone.cpp:325`), so code that reads shipped bytes at an address it
      also registers decodes **its own prologue**. Only `RtOriginal`/`ResolveExportRaw` reaches the
      DLL. The first wave to install a detour or build a trampoline hits this by construction.
      Held back with written reasons (48): `Element` dtor (no live caller in the tree AND no
      oracle) and `RemoveChildren` (a replay frees every child twice); 10 in the Dialog family
      (allocation/ownership split, the handle-table swap-remove, and `ElementDescription::Serialize`,
      whose name would re-point exe slot `0x1401518c8` onto the `.rsc` parse path); 11 of
      `UserInterface`, including `ShowDialog` (a nested `MainLoop::Frame` loop); 9 input widgets;
      4 layout widgets; 3 `SpriteContext` (they submit into the live GPU vertex buffer). Five more
      lane-E mutators are registered with an explicit NO ORACLE, because the restore-based oracle
      that works for the other nine is a use-after-free where ours has already run a scalar
      deleting dtor or released a refcount. See [WAVE7.md](WAVE7.md).
- [ ] Every ported function passes the diff harness against the DLL on `sa-run` and `sa-stress`.
      Wave 1 on `sa-run`: **6,556,631 calls / 0 divergences** with `BANISHED_RT_LIBM=dll`;
      90 divergences in 2 sites with our own libm, all 1 ULP and all sin/cos-fed
      (`Vector3::FromYawPitch`, `Transform::FromYawPitchOffset`). Not yet run under `sa-stress`.
      Wave 3 on `sa-run`: **38,613,179 calls / 0 new divergences** across 59 sites, all 12 Object
      sites with non-zero `calls=`. Residual is still those same 2 Wave 1 sites (138 divergences,
      1 ULP). **Four Wave 3 functions could not be exercised at all** — see the `Save9.sav` row.
- [ ] `sa-stress` `sim ms / iteration` within 5% of the pre-port baseline at each wave boundary.
      **DEFERRED BY THE USER 2026-09-06, at the Wave 4 boundary, to after the final stage:**
      *"dont really care if its slower we can fix that after the final stage."* The gate is
      therefore NOT a blocker for waves 4-8, and a wave must not be held open on it. It is not
      cancelled: it moves to the cutover stage (§4.2 wave 9), where the DLL is gone and the
      numbers mean something different anyway.
      **Wave 4 measured and BREACHED it: +7.7% and +7.5% over two runs on a verified-quiet machine** (three earlier runs
      at +8.4/+7.0/+9.8 were discarded as contaminated — another session was building and running
      the game on the same box; the clean number landed in the same range, so contention was NOT
      the explanation). `perf/baselines/current.csv` was NOT touched.
      **The `BANISHED_RT_PORT=0` A/B was NOT run** — it was the next step when the deferral came
      in. Whoever picks perf up should run it FIRST: it is free, it takes three runs, and it is
      the only thing that says whether the ~3-4 points Wave 4 added are the IO ports or drift.
      Wave 3 closed at +3.3-4.4%, so that is the delta to explain.
      **CLOSED at the Wave 3 boundary: +4.4% / +3.3% / +3.3% PASS over three runs.**
      The Wave 2 breach was ONE defect, not the seam: `RtOriginal` is `GetProcAddress` and
      `System::Time`'s ctor and `AsSeconds` called it inside their bodies, on the value type every
      `Update(const Time&)` passes. Caching it took the Wave 2 boundary from +7.8% to +2.2%.
      `perf/baselines/current.csv` was NOT touched.
      Two corrections to what this row used to say. (a) The Wave 2 split (~2.5 Wave 2 /
      ~4.5 predating) was **backwards** — measured over 3-5 runs per config it is drift +1.3,
      Wave 1 +1.0, Wave 2 +4.8. Its control was a single run. (b) `tools/stress_report.py`'s
      threshold being looser than this gate is now FIXED: it prints
      `PLAN 5% gate (sim ms/iter): +N%  PASS/FAIL` explicitly.
      **Standing rule from this wave: 3+ runs per config, and take the free `BANISHED_RT_PORT=0`
      A/B at every boundary.** One run cannot resolve a 5-point effect.
- [ ] Tutorials 1–4, a new game to 36/36 achievements' current 25 count, and `Save9.sav` load all green at each wave boundary.
      **`Save9.sav` NOW LOADS (Wave 4 lane H, `ef0debc`).** The blocker was never the save and
      never a port: `UI::UserInterface::ShowDialog` is a nested `MainLoop::Frame` loop and the
      `modsMissing` prompt had no unattended answer. Two more defects sat behind it — the
      long-standing "+0x7FC high dword wild write" (one `fn_String_Dtor` on an `InternalBasePtr`,
      decrementing an allocator boundary tag) and a missing `GameContainer_EnsureLists` on the
      Load path. All four Wave 3 functions that lived only on this path are now exercised:
      `GetPathForReferenceId` 89,733 calls, one call each for `GetReferenceId`,
      `ClearPersistState` and `ResetPersistState`, all 0 divergences — which retires
      "registered but never executed" without being a volume argument.
      **Tutorials 1-4 and the achievement count were NOT re-run at the Wave 4 boundary**, so this
      box stays unticked. Only the `Save9` half was verified.
      **The `Save9.sav` harness is FIXED (Wave 4 lane H, see [WAVE4_LANE_H.md](WAVE4_LANE_H.md)).**
      `Save9` and `Auto` both load, build the world and reach gameplay unattended, and all four
      Wave 3 functions that lived only on that path are now exercised with 0 divergences —
      `ExternalFactory::GetPathForReferenceId` 89,733 calls, `GetReferenceId` 1,
      `InternalFactory::ClearPersistState` 1 and `ResetPersistState` 1 (the last two in shadow
      mode). Three defects, none of them a port and none of them save-specific: (a)
      `UI::UserInterface::ShowDialog` is a NESTED `MainLoop::Frame` LOOP, and the `modsMissing`
      prompt every save on this machine triggers ("Debug Menu", not installed) had no unattended
      answer — that was the whole stall; (b) the "load-path wild write with +0x7FC in the high
      dword" was `fn_String_Dtor` called on an `InternalBasePtr` in `SaveLoadDialog::DoLoad`,
      decrementing an allocator boundary tag's high dword — one line, not a wild write; (c) the
      Load path never called `GameContainer_EnsureLists`, which is why a loaded game's
      `AchievementManager` had no instance list. This row used to say the harness "loops in
      `MenuDLT::Draw` forever with no world created" and that the four functions were
      registered-but-unexercised; both are now false. Tutorials 1–4 and the 36/36 run have NOT
      been re-measured at this boundary.
- [x] **Wave 8 (Top) merged.** Census after the wave: **772 called, 507 ported (65.7%)** —
      Engine 15/29, Audio 13/21, Input 15/22, Application 5/6, Save 15/18, Utility 1/2.
      49 ported, **32 held back with written reasons**. Two runs, **0 Wave 8 divergences** in
      28,191,797 and 50,870,488 diffed calls, both with a zero-byte crash log; a
      `BANISHED_RT_LIBM=dll` run takes the whole tree to 0 divergences.
      **The detour question this document has carried open since Wave 5 is answered**: Wave 8
      found the first genuine detour requirement and declined it with a reason (see the wave
      row above). The count is still 0 across all 4,632 functions.
      **`tools/export_name_audit.py` is at 0 phantoms for the first time** (651 names).
      **NOT verified, and stated plainly: ~22 of the 49 ports produced NO diff site.** The whole
      of `Input::Device` and most of `Input::KeyMap` need an ATTENDED run with real keyboard and
      mouse — the unattended harness injects dialog clicks by calling handlers directly and never
      passes through the input layer, so `calls=0` there means "the harness did not ask", not
      "verified" (`reference_uncalled_port_is_not_verified.md`).
- [ ] **Wave 9 (Cutover) SCOPED 2026-09-07, not started. See [WAVE9.md](WAVE9.md).**
      **The census numbers this checklist has published since Wave 1 were wrong in BOTH
      directions**, and the tool that found it is new: `BANISHED_RT_STRICT=1`
      (`standalone/resolve_standalone.cpp`, `f95c1af`) records every resolve that misses the
      port registry and falls through to the shipped DLL. One autostart run reported **260
      distinct requests still arriving at Runtime-x64.dll**
      (`docs/runtime_port/wave9_residue_autostart.txt`), and comparing that against
      `runtime_census.py` found three tool defects: `dx11/`'s entire Runtime import surface
      was invisible (**+51 called**), registrations inside an off-by-default `if (<flag>)`
      block were counted as live ports (**-6 ported**, all of WAVE3.md's NO-GO handle-table
      tier — Object 28 → 22, which is what WAVE3.md said all along), and 18 registrations
      passing a named mangled-name constant were warned about instead of counted (UI 98 →
      101).
      **Honest census: 823 called, 504 ported (61.2%)** — not the 772 / 507 / 65.7% recorded
      at the Wave 8 boundary. 319 unported: 145 functions, 57 unnamed internals, 51
      ctors/dtors, 59 exported DATA symbols, 4 ordinals, 3 `.data` globals.
      **An empty `[RtResidue]` list across the full harness matrix is the real cutover gate**,
      and it is the first thing in this tree that asks the PROCESS rather than the source.
- [x] **Three Wave 9 prerequisites CLOSED 2026-09-07** (details in [WAVE9.md](WAVE9.md)):
      (a) **All 8 census orphans have verdicts.** Three are exercised by a direct C++ call from
      a sibling port; three are the char/schar/short members of the 18-member `IO::Serializer`
      forwarder family, verified structurally at registration and deliberately dead;
      `Vector2::Normalize(Scalar&)` is verified by a new probe (10/10 cases, `0xCD`-poisoned
      buffers) and `EdgeMeshInstance`'s copy ctor by instruction-for-instruction comparison with
      retail `0x1800174f0`, all three of its bindings confirmed against the export directory.
      (b) **`Input::Device` no longer needs an attended run, and WAVE8.md's premise was half
      wrong.** Its three readers never touch the hardware — they are const reads of latched
      state with full replay oracles, so what they needed was a CALLER. `RtOrphanProbeInGame`
      sweeps every valid triple: **570 comparisons, 0 mismatches**. What remains of the item is
      three `KeyMap` const readers needing a populated binding tree.
      (c) **The 1-ULP libm residual is decided and now measured.** The decision was already
      written in `runtime/funcs/rt_libm.h` (our libm ships; the DLL route is diagnostic-only).
      The unmeasured risk was world-gen compounding: three `sa-stress` runs show **the control
      moves further than the change** (102 entities between two same-libm runs vs 69 between
      libms) and every deterministic column is identical. A bound below the harness noise floor,
      not a proof of bit-identity.
      **A wrong finding was published and withdrawn in the same session**: `BANISHED_AUTOQUIT=1`
      quits at the MAIN MENU, so two residue runs that never reached gameplay looked
      path-independent. Through the pause menu the residue is **309, not 260** — a strict
      superset, 49 new and none lost. Lane 0's matrix is required after all.
- [x] **The Wave 8 attended-run item is CLOSED IN FULL, 2026-09-07** — `Input::Device`'s three
      readers AND `Input::KeyMap`'s three, **1,909 comparisons, 0 mismatches**, no human at a
      keyboard. Domains swept exhaustively rather than sampled (`id` is a `uint8_t`, so 0..255
      IS the domain). See [[reference-const-reader-needs-a-caller-not-a-human]] in memory and
      the CLOSED sections of [WAVE9.md](WAVE9.md).
- [x] **The 32 wave-3-to-8 holdbacks are TRIAGED with a per-function cutover disposition**
      (WAVE9.md). The reframe that makes them tractable: at cutover a holdback cannot stay on
      the DLL, but it does not automatically need a faithful port either — it needs to **not be
      a missing symbol**, so each resolves to *port*, *probe*, or *stub with a written reason*.
      Groups: **P** 1 function (a probe on machinery that now exists), **B** 10 (blocked on a
      named dependency — `Audio::SoundBuffer`/`SoundEffect`, retail's `IO::Encoding` offsets,
      the allocator), **N** 7 (allocator-bound, or oracle-hard: `Input::Device::Update` needs a
      BESPOKE oracle because call-twice drains the DirectInput queue it is reading), **D/X** 14
      (stub-with-a-reason, or already unblocked). **The table's own first draft got the
      `ExternalFactory` group WRONG, and that is the argument for writing it per function**: it
      asserted all four were held back on a Wave 4 dependency that has since landed. Checking
      each shows a 1/1/2 split — only `SerializeReferenceTable` is unblocked (lane A's cheapest
      row); `SaveText` needs `IO::TextFileStream`, which WAVE4.md says in as many words it did
      NOT port; and `FindOrLoad`/`Clean` were never blocked on Wave 4 at all — they have no
      possible oracle, because both rewrite the one shared AVL tree. A group summary that was
      accurate about the group was wrong about three of its four members.
- [ ] `reimpl-x64.exe` boots to gameplay with `Runtime-x64.dll` absent from the game directory.

---

## 6. Per-function recipe for agents

1. **Locate in the profile build first.** `switch_program("Runtime-x64-profile.dll")`, find the
   function by export name or by a string anchor (assert text, source path); decompile there.
   Names in the profile build's strings win over any name we invented.
2. **Map to retail.** Hash-match to `Runtime-x64.dll` (`bulk_fuzzy_match` / the Runtime hash
   index once Wave 0 builds it). Record both addresses in the source comment. Rename the retail
   function in Ghidra with the recovered name (`Namespace_Class_Method`).
3. **Types.** Add or extend the struct in `runtime/types/<Dir>/<Class>.h` with `static_assert` on
   every field offset. Container members use the `Collection` headers, never hand-rolled triples.
4. **Port** into `runtime/src/<Dir>/<file>.cpp`, mirroring the recovered source tree. Byte-faithful
   first, readable second; no null guards that retail lacks (see CLAUDE.md).
5. **Register** with `RegisterRuntimeExport` / `RegisterRuntimeAddr`, install the detour, and add
   the diff-harness wrapper. Env kill-switch `BANISHED_RT_<CLASS>=0`.
6. **Test**: `sa-run` with `BANISHED_RT_DIFF=1`; zero divergences; then `sa-stress` and the save
   load. Read `standalone_crash.log` first if it dies.
7. **Record**: the export name, both addresses, the diff result and the run in the commit message.
8. **Correct the comments the port just disproved.** A port is not finished until the stale claims
   it invalidated are fixed AT THEIR SOURCE — in `engine/funcs/`, `engine/types/`, `code/Game/`,
   `standalone/` and in this document. This is not tidying. Every wave of this project so far has
   lost time to a comment that outlived the thing it described: five "player-visible gaps" that
   were stale banners over ported code (§1.2), a stub whose "why this is stubbed" note named a
   blocker that had already been removed, a `fn_*` name that had not matched its target for
   months. A wrong comment is worse than no comment, because it is believed.
   The rule, concretely:
     * Fix the comment where it is WRITTEN, not only in the wave doc. A wave doc nobody opens
       does not correct `engine_funcs.h`.
     * When a `fn_*` typedef or its comment misnames or mistypes its target, fix BOTH — the
       signature is load-bearing (a missing hidden return slot is a wild write) and the name is
       what the next reader greps for.
     * A comment that was right about the mechanism but wrong about the OWNER still has to
       change: `str-4`/`str-8` is `System::String`'s header, not the allocator's, and PLAN.md
       §4.3 risk 2 repeated that error for months because it paraphrased a correct comment.
     * State what the thing IS now, and keep one line saying what it was believed to be, so the
       next reader recognises the old claim if they meet it elsewhere.
   Verify first, then correct: a comment "corrected" from an unverified port just moves the
   error.

Dispatch tiers: Wave 1–2 leaf functions are Sonnet work with a Fable/Opus verifier; anything
touching the allocator, the factory, or a vtable is Opus with a Fable review pass.
