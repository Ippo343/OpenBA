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
| `ResolveExport("?mangled@…")` | 909 | Exported Runtime function by MSVC name |
| `ResolveAddr(Module::Runtime, 0x1800xxxxx)` | 281 | Non-exported Runtime internal by Ghidra address |
| `ResolveOrdinal(n)` | 10 | Export by ordinal |
| `RegisterDispatchSlot(exeSlot, ResolveExport(…))` | 148 | The exe's own PE import table; retail-shaped code reads `*(fn**)slot` |
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
the `IsDerivedFrom` null-this fault) and `:533` (`+0xBC73E`, `External::Load`). Retire with Wave 3.

**D. Vtables copied out of live Runtime objects.** No `??_7` vtable symbol is ever resolved; our
subclasses let the Runtime ctor stamp the base vtable and then copy or patch slots:
`code/Game/Dialog/thumb_vtables.cpp:66-82`, `code/Game/Component/statusicon_geometry.cpp:328`,
`code/Game/Dialog/saveloaddialog.cpp:854`, `code/Game/Component/frustumoutlinedescription.cpp:89`.
A ported class must expose its vtable as a real symbol or reproduce the stamping order.

**E. Dead code.** `runtime/src/ui/spritesheet_debug.cpp` had zero callers and was removed
(2026-09-06). `sprite_debug.cpp` and `element_sprite_debug.cpp` are LIVE: `buildui.cpp`,
`buildrequirementui.cpp` and `toolbardialog.cpp` reach them through bare `extern` declarations,
which a filename grep misses. `runtime/funcs/runtime_funcs.cpp` still resolves nothing.

### 2.3 Runtime globals we reference (25 addresses; 17 have no export and need an accessor)

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
| **1 Math + Volume** | 292 fns, 30 KB | S | Pure, zero deps, diff-testable to the bit; proves the seam + harness |
| **2 System + Collection** | String, MemoryAllocator, Filename, WideString, Time/Timer, Random, CRC, Color, CriticalSection, Thread, Compression, Configuration; Collection templates as headers | M | 4,100+ inbound edges; allocator is global state (already detoured for locking) and must be swapped atomically with its `.data` singleton |
| **3 Object** | Base, Type, BaseTypeInfo, Internal, External, InternalPtr, Factory, InternalFactory, ExternalFactory, ExternalList | M | The reflection + handle table. Once ours, object construction flows through us and vtable ownership stops being a problem |
| **4 IO** | Stream family, Serializer (started), BinaryReader/Writer, Encoding, Persistence/Registry, FileSystem, PackageFile, Modification | L | PackageFile is the densest class in the DLL (25 KB) |
| **5 Algorithm packages** (parallel lanes) | Collision2D, Picking, Pathfinding, Achievement, Save | M each | Self-contained, each ~13–18 KB, only System/Object below them |
| **6 Video + Graphics** | Video abstraction (Device/Context/buffers; the VideoDX11 plugin interface stays), Scene/Renderer/Mesh/MeshInstance/Animation/Particle/Lights | L | Graphics→Video 73 edges, port Video first |
| **7 UI** | 73 classes, 1,625 fns, 193 KB | XL | Element by element; DialogInstance and UserInterface last |
| **8 Top** | Audio, Input, Application, Engine (MainLoop/UpdateLoop/Loader family, some already reimplemented in the hook era: `engine/types/MainLoop.h` etc.), Save | M | Integrators; mostly wiring |
| **9 Cutover** | Remove `LoadLibrary` of Runtime; retire the §2.2 patches; VideoDX11 is already ours (`dx11/`) | S | |

Waves 1–2 are the proving ground; do not parallelize past Wave 2 until the seam, the detour and
the diff harness have each caught at least one real divergence.

### 4.3 Risks, ranked

1. **Collection template layouts** (inlined, no ABI). Recover from the profile build's asserts and
   from consumer code; `Array<T>` is the `{data,count,cap}` 16-byte triple already known from the
   exe port, `Map` is the AVL tree behind `Map_Insert`/`Map_Find`.
2. **The allocator.** Bucketed free lists, refcount at `str-4`, user pointer at `str-8`
   (`filename_runtime_hook.cpp` header documents it). Everything allocates through it, so a
   half-ported allocator corrupts everything. Port it with the diff harness in "shadow" mode
   (ours computes, theirs executes) before switching.
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
   is **call-graph propagation** (an anchored function whose caller is mapped, whose retail call
   site still targets a `FUN_`). Until that lands, treat the pool as unnamed and budget the
   naming per wave — do not plan a wave on the assumption it is solved.
5. **CRT/SEH inside the DLL.** The DLL has its own CRT and `__C_specific_handler`; a port that
   throws or unwinds across the DLL boundary is undefined. Keep exceptions out of ported code.

---

## 5. Definition of done for the runtime phase

- [x] Wave 0 tooling merged; `tools/runtime_census.py` reports registered / called / total per namespace.
      Baseline 2026-09-06: **869 functions called, 0 ported** (UI 137, Graphics 78, Math 60,
      Video 60, System 57, IO 39, Object 38, Achievement 37, Input 21, Audio 20, Save 20,
      Engine 19, Volume 12, Picking 8, Application 5, Utility 2, plus 245 non-namespaced
      internals, 7 ordinals and 4 names the census cannot attribute).
- [ ] Every ported function passes the diff harness against the DLL on `sa-run` and `sa-stress`.
- [ ] `sa-stress` `sim ms / iteration` within 5% of the pre-port baseline at each wave boundary.
- [ ] Tutorials 1–4, a new game to 36/36 achievements' current 25 count, and `Save9.sav` load all green at each wave boundary.
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

Dispatch tiers: Wave 1–2 leaf functions are Sonnet work with a Fable/Opus verifier; anything
touching the allocator, the factory, or a vtable is Opus with a Fable review pass.
