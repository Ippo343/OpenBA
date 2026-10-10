# Runtime-x64.dll call surface

Full per-function list: `runtime_surface.csv` in this same scratchpad directory (839 rows,
columns `mechanism,namespace,class,method,mangled_or_addr,files_using,callsites`).

## 1. Mechanisms and counts

All counts from a Python walk of `code/ engine/ runtime/ standalone/` (`.cpp/.h/.c/.hpp`,
any path containing a `build` directory excluded). The script is reproduced at the end of
this section. Raw `grep -rn <pattern> code engine runtime standalone | wc -l` gives slightly
higher numbers because it also counts declarations, comments and prose mentions.

| Mechanism | distinct | call sites | resolved where | boot stage |
|---|---|---|---|---|
| `ResolveExport("?mangled")` | 608 | 894 | `GetProcAddress`, `standalone/resolve_standalone.cpp:262` | mostly step 4 (`EngineFuncsInit`), some step 3 |
| `ResolveAddr(Module::Runtime, 0x1800…)` | 244 (222 code, 22 data) | 269 | ASLR delta add, `standalone/resolve_standalone.cpp:190` | step 2 onward, lazily at first use |
| `ResolveOrdinal(n)` | 7 | 9 | `GetProcAddress(MAKEINTRESOURCE)`, `standalone/resolve_standalone.cpp:265` | lazy + `EngineFuncsInit` |
| `RegisterDispatchSlot(exeSlot, …)` | 129 addresses | 143 sites | `standalone/address_registry.cpp:1068` | step 3 |
| `fn_*` typed pointer table | 673 externs / 682 typedefs / 603 assigned | — | `engine/funcs/engine_funcs.cpp:1536` (`EngineFuncsInit`) | step 4 |
| `g_*` Runtime globals in that table | 210 assigned | — | same | step 4 |

Exact commands used:

```
# per-mechanism raw grep (upper bound, includes decls/comments)
grep -rn "ResolveExport("              code engine runtime standalone | wc -l   # 909
grep -rn "ResolveAddr(Module::Runtime" code engine runtime standalone | wc -l   # 281
grep -rn "ResolveOrdinal("             code engine runtime standalone | wc -l   # 10
grep -rn "RegisterDispatchSlot("       code engine runtime standalone | wc -l   # 148

# authoritative parse (regex on the actual call form, build dirs skipped)
python /tmp/surf.py
#   distinct exports 608, callsites 894
#   distinct runtime addrs 244, callsites 269
#   distinct ordinals 7

# dispatch slots
grep -rn "RegisterDispatchSlot(0x" standalone/*.cpp | cut -d: -f1 | sort | uniq -c
#   132 standalone/address_registry.cpp
#    11 standalone/reg_unreg_lane1.cpp
grep -rho "RegisterDispatchSlot(0x[0-9a-fA-F]*" standalone/*.cpp \
  | grep -o "0x[0-9a-fA-F]*" | tr 'A-F' 'a-f' | sort -u | wc -l      # 129

# engine_funcs table size
grep -c "^extern .*fn_[A-Za-z0-9_]*;"            engine/funcs/engine_funcs.h    # 673
grep -c "typedef.*fn_.*_t"                       engine/funcs/engine_funcs.h    # 682
grep -c "^[a-zA-Z_].*fn_[A-Za-z0-9_]* = nullptr;" engine/funcs/engine_funcs.cpp # 603
grep -c "^[a-zA-Z_].*g_[A-Za-z0-9_]* *= *nullptr;" engine/funcs/engine_funcs.cpp # 210
```

`ResolveExport` splits across directories as code 361 / engine 364 / standalone 189 /
runtime 1 (raw grep). `ResolveAddr(Module::Runtime, …)` splits as engine 193 / code 58 /
standalone 30 / runtime 0.

### Boot order

From `standalone/main.cpp` `Init()`:

| Line | Call | What it resolves |
|---|---|---|
| 1559 | `HarnessGuardUnattendedBoot()` | writes the registry keys that suppress the launcher dialog |
| 1561 | `BootState::Start()` | boot-state watchdog |
| 1570 | `ResolveInit()` | `LoadLibrary("Runtime-x64.dll")`, computes ASLR delta; VideoDX11 is `GetModuleHandle` only |
| 1582 | `HookRuntimeMessageBoxW()` | patches Runtime's IAT |
| 1597 | `AddressRegistryInit()` | `RegisterExeAddr` + all `RegisterDispatchSlot` (these call `ResolveExport`) |
| 1602 | `EngineFuncsInit()` | the 603 `fn_*` + 210 `g_*` assignments |
| 1605 | `RuntimeFuncsInit()` | placeholder, resolves nothing |
| 1622 | `Dx11SeamInstall()` | our DX11 vtables |
| 1630 | `InstallVideoInterfacePrefill()` | detours `Video::Device::InitializeInterfaces` |
| 1638 | `RedzoneAlloc_Install()` | wraps `fn_MemAlloc`/`fn_MemFreeStatic` (env-gated) |
| 1646/1650 | `InstallAllocatorLock()` / `InstallFreeLock()` | byte-patches Runtime's allocator |
| 1799 | `Game::RunGameMain()` | `Engine::Startup` and everything after |

Everything else resolves lazily on first use, typically a function-local
`static fn_X_t s_fn = (fn_X_t)ResolveAddr(Module::Runtime, 0x…)`.

Of the 143 `RegisterDispatchSlot` sites, 110 pass `ResolveExport(...)`, 2 pass
`ResolveAddr(Module::Runtime, …)`, and 15 pass a pointer to one of our own C++ functions.
143 sites over 129 distinct addresses means 14 are registered twice (the later binding wins;
`tools/dispatch_slot_map.py --check` reports these as SHADOWED).

### Two mechanisms beyond the brief

- **Exported data symbols.** Eight singletons are fetched by mangled *data* export rather
  than by address: `?_singleton@` for `Achievement::AchievementManager`, `Input::Configuration`,
  `Input::Device`, `Video::Device`, `IO::FileSystem`, `Object::InternalFactory`,
  `Engine::MainLoop`, `Save::SaveLoad`.
- **Live code detours into the DLL.** Four modules rewrite Runtime's own instruction bytes
  or import table at boot. Detailed in section 4.

There are **no** `ResolveExport("??_7…")` vtable-symbol lookups
(`grep -rho 'ResolveExport("??_7[^"]*"' … | sort -u | wc -l` returns 0). Our code obtains
Runtime vtables by letting a Runtime constructor stamp the object and then copying or
patching slots out of the live object, not by naming the vtable symbol. See
`code/Game/Dialog/thumb_vtables.cpp:66-82` and `code/Game/Component/statusicon_geometry.cpp:328`.

## 2. Runtime dependency surface by namespace and class

### By namespace (distinct exported functions called)

| Namespace | distinct fns | call sites |
|---|---|---|
| UI | 135 | 155 |
| Graphics | 73 | 106 |
| Math | 59 | 110 |
| System | 54 | 136 |
| Video | 49 | 62 |
| IO | 40 | 65 |
| Object | 36 | 55 |
| Achievement | 25 | 26 |
| Input | 21 | 21 |
| Audio | 20 | 21 |
| Save | 20 | 23 |
| Engine | 17 | 18 |
| Volume | 12 | 21 |
| Picking | 7 | 8 |
| Utility | 2 | 2 |

About 14 of the 608 names demangle badly because the mangling contains a nested `@@`
(they land in junk buckets such as `QEAAXXZ::1`). They are present verbatim in the CSV
under their real mangled name.

### By class (top 40 of 125)

| Namespace::Class | distinct fns | call sites |
|---|---|---|
| UI::Element | 21 | 23 |
| IO::Serializer | 20 | 39 |
| Video::Context | 20 | 26 |
| UI::UserInterface | 19 | 21 |
| System::String | 16 | 21 |
| Save::SaveLoad | 16 | 19 |
| Audio::Scene | 16 | 17 |
| Math::Vector3 | 15 | 50 |
| UI::DialogInstance | 13 | 14 |
| Graphics::Geometry | 12 | 20 |
| Graphics::MeshInstance | 11 | 18 |
| Math::Transform | 11 | 17 |
| Math::Vector2 | 11 | 12 |
| Input::KeyMap | 11 | 11 |
| Video::Device | 11 | 11 |
| System::Filename | 10 | 19 |
| System::Random | 9 | 28 |
| Object::ExternalFactory | 9 | 14 |
| Achievement::AchievementManager | 8 | 9 |
| Video::ImageBuffer | 8 | 9 |
| Object::InternalBasePtr | 7 | 15 |
| Graphics::Scene | 7 | 14 |
| IO::Persistence | 7 | 9 |
| Input::Device | 7 | 7 |
| UI::SpinnerElement | 7 | 7 |
| Math::Transform3 | 6 | 11 |
| UI::Sprite | 6 | 11 |
| IO::FileSystem | 6 | 10 |
| Graphics::MaterialInstance | 6 | 10 |
| Volume::Frustum | 6 | 8 |
| Graphics::Renderer | 6 | 8 |
| Picking::Scene | 6 | 7 |
| Achievement::AchievementInstance | 6 | 6 |
| IO::CompressedMemoryStream | 6 | 6 |
| UI::ComboElement | 6 | 6 |
| Volume::Box | 5 | 9 |
| UI::SpriteSheet | 5 | 9 |
| UI::String | 5 | 8 |
| Object::External | 5 | 7 |
| UI::ImageElement | 5 | 5 |

The remaining 85 classes have 1-4 distinct functions each. Full list in the CSV.

### Non-exported Runtime internals (222 code addresses)

178 of the 222 are resolved in `engine/funcs/engine_funcs.cpp` alone. The remaining 44 are
spread thin: `standalone/filename_runtime_hook.cpp` 9, `code/Game/Game/weather_particle_draw.cpp` 5,
`code/Game/Game/game_main.cpp` 5, `code/Game/Game/scenario.cpp` 4,
`code/Game/Tutorial/tutorialgoaldialog.cpp` 4, then 1-2 each across ~20 files.

Grouped by the `fn_*` / `g_*` / `s_*` name our code assigns them (the nearest identifier
within 3 lines of the `ResolveAddr` call):

| Group | count | Group | count |
|---|---|---|---|
| UI Element internals | 53 | Path / pathfinding | 4 |
| Dialog internals | 23 | Video | 4 |
| String | 9 | Persistence | 4 |
| Goal / GoalList | 6 | Device | 4 |
| Save | 6 | External | 4 |
| Factory | 6 | Loader | 4 |
| Timer | 6 | File | 3 |
| Filename | 5 | Serializer | 3 |
| Achievement | 5 | MemAlloc | 3 |
| UI (other) | 5 | InternalFactory | 2 |
| Random | 5 | Input | 2 |
| Scene | 5 | Resource | 2 |

52 do not group by keyword. The identifiable clusters among them:

- Modification / mod-list helpers: `0x18000da80` `GetMod`, `0x18000da90` `GetCount`,
  `0x180010110` array copy, `0x1800101e0` entry copy-ctor, `0x180010ab0` list cleanup,
  `0x180011460` entry dtor, `0x180026ef0` entry assign.
- Math primitives: `0x18001e540` matrix ctor, `0x1800220b0` from-yaw-pitch,
  `0x18008f530` scalar abs, `0x18001a880` set-inverse-scale.
- Containers: `0x18002a090` map insert, `0x18002ace0` map find, `0x18007a0d0` dynamic-array push.
- Render queue: `0x18004f790` post-draw push, `0x18004f8e0` post-update push.
- Geometry/context: `0x18001a360` `Context::SetFog`, `0x1800a3fa0` geometry attach,
  `0x1800a41c0` geometry base dtor.
- Input: `0x1800a7190` `KeyMap::Load`, `0x1800a7b60` `KeyMap::GetValue`.
- Detour targets (also section 4): `0x180006770`, `0x1800067d0`, `0x180007d40`.

## 3. Runtime types we already mirror, and Runtime code we already reimplement

### `runtime/types/` — five Runtime structs, offset-asserted

| Header | static_asserts |
|---|---|
| `runtime/types/ui/SpriteSheet.h` | 26 |
| `runtime/types/ui/LabelElement.h` | 20 |
| `runtime/types/video/ImageBuffer.h` | 19 |
| `runtime/types/ui/ImageElement.h` | 10 |
| `runtime/types/ui/Sprite.h` | 4 |
| `runtime/types/common.h` | 0 |

### `engine/types/` — 148 headers, of which 14 describe RUNTIME structs

Identified by `grep -rln "Runtime-x64|Runtime 0x1800|Module::Runtime|0x1800" engine/types`:

| Header | static_asserts |
|---|---|
| `engine/types/ui/Panels.h` | 73 |
| `engine/types/MainLoop.h` | 23 |
| `engine/types/Modification.h` | 18 |
| `engine/types/Dialogs.h` | 12 |
| `engine/types/io_serializer.h` | 11 |
| `engine/types/Configuration.h` | 8 |
| `engine/types/DialogLoopT.h` | 8 |
| `engine/types/UpdateLoop.h` | 8 |
| `engine/types/DialogLoaderLoopT.h` | 7 |
| `engine/types/LoaderLoop.h` | 6 |
| `engine/types/ui/Elements.h` | 6 |
| `engine/types/ui/DialogInstance.h` | 5 |
| `engine/types/ui/DialogController.h` | 4 |
| `engine/types/SystemString.h` | 1 |

The other 134 headers in `engine/types/` are exe-side game types (components, tools,
dialogs, actions) and are not part of the Runtime port surface.

### `runtime/src/` — 809 lines, only partly live

- **`runtime/src/io/serializer.cpp` (311 lines) + `serializer.h` (44)** — a real
  reimplementation of the twelve `IO::Serializer` forwarder methods, byte-accurate to the
  originals. **Wiring: it replaces the exe's import slots, not the DLL's exports.**
  `standalone/address_registry.cpp:1663-1680` points 15 dispatch slots at `RtSerializer_*`
  (`0x140152ac8` int, `0x140152a60` float, `0x140152a58` bool, `0x140152410`/`0x140152418`
  uchar, `0x140152a50` uint, `0x140152a48` GetVersion, `0x140152488` ushort array,
  `0x140151278` int array, `0x1401523f8` short array, `0x140152490`/`0x140152400` uchar
  array, `0x140152618`/`0x140152478` SerializeBytes). Each wrapper checks
  `BANISHED_REIMPL_SERIALIZER` once (cached); **unset, which is the default, it tail-calls
  the resolved Runtime original.** So it runs *alongside* the DLL by default and only
  *replaces* it under the env var. This is the A/B pattern a Runtime port should follow.
  Note `address_registry.cpp:1130` marks an earlier registration of one of these slots as
  shadowed by the `RtSerializer_*` binding.
- **`runtime/src/ui/sprite_debug.cpp` (93), `spritesheet_debug.cpp` (165),
  `element_sprite_debug.cpp` (147)** — logging wrappers around `UI::Sprite`,
  `UI::SpriteSheet` and `UI::Element` sprite accessors, not reimplementations.
  `grep -rn "SpriteSheet_Debug_|Sprite_Debug_|Element_Sprite_Debug" code engine standalone`
  returns **zero callers**. They compile (Makefile line 30 globs `runtime/src/*/*.cpp`)
  but are dead code.
- **`runtime/funcs/runtime_funcs.cpp` (12 lines)** — a placeholder. `RuntimeFuncsInit()`
  resolves nothing and prints a debug line.

### `dx11/` — the working model for a DLL port

By contrast, VideoDX11-x64.dll is already fully replaced: 13 `.cpp` files under `dx11/src/`
plus `dx11/funcs/dx11_seam.cpp` (twelve vtables, 84 slots verified against `dx11/README.md`).
The DLL is never loaded — `standalone/resolve_standalone.cpp:216-224` uses `GetModuleHandle`
only, and `InstallVideoInterfacePrefill` stops Runtime from `LoadLibrary`ing it. Two exports
(`CreateDeviceData`, `GetDeviceName`) are ours. That is the shape a Runtime port should aim
at, except Runtime has ~4,800 exports rather than 2.

## 4. Coupling hazards for a Runtime port

Ordered by how hard each is to carry across.

**A. Live byte-patching of Runtime code.** Three modules rewrite instruction bytes in the
loaded DLL, each depending on the exact prologue at a hardcoded address:

- `standalone/allocator_lock.cpp:292` — `System::MemoryAllocator::Allocate` @ `0x1800c6e60`,
  13-byte prologue overwritten with `MOV RAX,imm64; JMP RAX; NOP`, original bytes relocated
  to a trampoline that jumps back to `Allocate+13`. `VirtualProtect` at line 336, restore at 342,
  `FlushInstructionCache` at 343. Purpose: retail's `Allocate` takes no lock while `Free` does,
  so the background Loader thread races the main thread. **Default on** (`BANISHED_ALLOC_LOCK`).
- `standalone/allocator_lock.cpp:363` — `System::MemoryAllocator::Free` @ `0x1800c6c00`,
  12-byte prologue, same technique. `VirtualProtect` at 407, restore 413, flush 414.
- `standalone/filename_runtime_hook.cpp:392` — `System::Filename::~Filename` @ `0x180006980`,
  18-byte boundary.
- `standalone/filename_runtime_hook.cpp:402` — `System::Filename::operator=` @ `0x180006770`,
  13-byte boundary.
- `standalone/filename_runtime_hook.cpp:424` — `Filename(const String&)` ctor @ `0x1800067d0`,
  19-byte boundary. `VirtualProtect` at 346, restore 353, flush 354; a second patch site at 459-462.
  Diagnostic only, env-gated.
- `standalone/video_interface.cpp:140` `InstallVideoInterfacePrefill` — detours
  `Video::Device::InitializeInterfaces`, and `FillDx11Slot` (line 111) writes an `HMODULE`
  and two function pointers **directly into the Runtime-owned Device object** at
  `device + kSlot0 + kInterfaceDX11 * kSlotStride`, using hardcoded field offsets
  `kOffModule`, `kOffGetName`, `kOffCreate`.

**B. Import-table rewriting.** `standalone/main.cpp:706-760` walks Runtime's PE import
descriptors and swaps the `user32!MessageBoxW` IAT entry to `HookedMessageBoxW`
(`VirtualProtect` at 740, restore at 746; a second site at 814-819). Purpose: MSVC's
`_purecall` R6025 modal inside Runtime's statically-linked CRT otherwise blocks the process
with no crash log. `_set_purecall_handler` cannot reach it because it is a different CRT.

**C. Hardcoded Runtime internal layout read from our side.**

- `code/Game/Game/game_main.cpp:486` and `standalone/main.cpp:523, 545` — walk
  `Object::Factory::_firstTypeInfo` (`0x1801fc6e0`) through a hand-declared 8-field
  `BaseTypeInfo` struct `{typeId, pad, name, parent, abstract, factoryFn, ip, next}`.
- `code/Game/Game/game_main.cpp:467` — reads `System::Error::_errorString` (`0x1801fc720`)
  as a raw 8 KB `const char*`.
- `code/Game/Game/game_main.cpp:365` — reads the Modification DLL `HMODULE` slot
  (`0x1801fc6d0`) and calls `FreeLibrary` on it.
- `code/Game/Game/game_main.cpp:575` and `engine/funcs/engine_funcs.cpp:1630` — deref the
  `MemoryAllocator` singleton pointer (`0x1801fc718`) directly; the comment at 1630 notes a
  refresh is required because the factory cached NULL.
- `code/Game/Component/particlecomponent.cpp:995` — reads/writes
  `Graphics::ParticleInstance::_frameIndex`, a Runtime static at `0x1801fe77c`.
- `code/Game/Game/filesystem_setup.cpp:113, 190` and
  `code/Game/Dialog/mod_dialog_vtables.cpp:501, 549, 632, 665, 708` — deref the FileSystem
  and Modification singletons by address rather than through the exported symbol.
- `code/Game/Object/internal_factory_audit.cpp:16` — `constexpr uintptr_t
  kInternalFactorySingleton = 0x1801fc6d8`.

**D. Hardcoded Runtime instruction addresses in the crash handler.**
`standalone/main.cpp:518` compares `RIP` against `runtimeBase + 0xC0390 … +0xC03A0` to
recognize the `IsDerivedFrom` null-`this` fault, and `standalone/main.cpp:533` against
`runtimeBase + 0xBC73E` for the `External::Load` tree-walk fault, then reads
`[RBP+0x48]` for the missed type id. Both break the moment Runtime is recompiled.

**E. Vtable slots copied out of live Runtime objects.** Because no `??_7` vtable symbol is
ever resolved, our subclass vtables are built by letting the Runtime constructor stamp the
base vtable and then copying slots: `code/Game/Dialog/thumb_vtables.cpp:66-82`,
`code/Game/Component/statusicon_geometry.cpp:328` (copies the real Runtime base-Geometry
vtable), `code/Game/Dialog/saveloaddialog.cpp:854`,
`code/Game/Component/frustumoutlinedescription.cpp:89`. A port must expose these vtables
as real symbols or reproduce the stamping order exactly.

**F. Documentation drift worth correcting.** `CLAUDE.md` still describes
`SkipPlatformConfigure()` in `standalone/main.cpp` patching the `DialogBoxParamW` call site
to `MOV EAX,1; NOP`. `standalone/main.cpp:1718` says the launcher dialog is no longer
patched, and no such function exists in the tree. The dialog is now suppressed through the
registry by `HarnessGuardUnattendedBoot()` (`standalone/main.cpp:184`, called at 1559).

## 5. Runtime globals referenced (`0x1801…` / `0x1802…`)

25 distinct addresses, from
`grep -rho '0x1801f[0-9a-fA-F]{4}|0x1801e[0-9a-fA-F]{4}|0x1802[0-9a-fA-F]{5}' code engine runtime standalone | sort -u`:

| Address | Name we gave it | Where |
|---|---|---|
| 0x1801faa88 | `UI::Dialog` type id | `engine/funcs/engine_funcs.cpp:1745` |
| 0x1801fc678 | `Application::Application::_singleton` (ref used by MainMenuDialog::OnCommand) | `standalone/address_registry.cpp:1579` |
| 0x1801fc6a8 | `IO::FileSystem::_singleton` | `code/Game/Game/filesystem_setup.cpp:113, 190` |
| 0x1801fc6b0 | `IO::Modification::_singleton` | `standalone/reg_mods.cpp`, `code/Game/Dialog/mod_dialog_vtables.cpp:473` |
| 0x1801fc6b8 | `IO::Persistence::_singleton` | `engine/funcs/engine_funcs.cpp:1786` |
| 0x1801fc6c8 | `Object::ExternalFactory::_singleton` | `engine/funcs/engine_funcs.cpp:1703` |
| 0x1801fc6d0 | Modification DLL `HMODULE` slot | `code/Game/Game/game_main.cpp:365` |
| 0x1801fc6d8 | `Object::InternalFactory::_singleton` | `code/Game/Object/internal_factory_audit.cpp:16` |
| 0x1801fc6e0 | `Object::Factory::_firstTypeInfo` (type-info list head) | `code/Game/Game/game_main.cpp:486`, `standalone/main.cpp:523, 545` |
| 0x1801fc6e8 | Factory singleton used by `LoadDialogResource` | `standalone/address_registry.cpp:1587` |
| 0x1801fc718 | `System::MemoryAllocator` singleton | `code/Game/Game/game_main.cpp:575`, `engine/funcs/engine_funcs.cpp:1630` |
| 0x1801fc720 | `System::Error::_errorString` (8 KB buffer) | `code/Game/Game/game_main.cpp:467` |
| 0x1801fe720 | `System::Timer::_ticksPerSecond` | `engine/funcs/engine_funcs.cpp` |
| 0x1801fe728 | `System::Timer::_secondsPerTick` | `engine/funcs/engine_funcs.cpp` |
| 0x1801fe730 | `System::Timer::_systemTicksPerSecond` | `engine/funcs/engine_funcs.cpp` |
| 0x1801fe738 | `System::Timer::_systemInitialTick` | `engine/funcs/engine_funcs.cpp` |
| 0x1801fe750 | `Engine::MainLoop::_singleton` | `engine/types/MainLoop.h:19` (`constexpr Singleton`) |
| 0x1801fe758 | `Audio::Configuration` singleton | `engine/funcs/engine_funcs.cpp:3023` |
| 0x1801fe77c | `Graphics::ParticleInstance::_frameIndex` | `code/Game/Component/particlecomponent.cpp:995` |
| 0x1801fe7a0 | `Video::Configuration` singleton | `engine/funcs/engine_funcs.cpp:3045` |
| 0x1801fe7a8 | `Video::Device::_singleton` | `engine/funcs/engine_funcs.cpp:1636` |
| 0x1801fe7b8 | `g_pLoader` | `engine/funcs/engine_funcs.cpp` |
| 0x1801fe7c0 | `g_pLoaderFlags` | `engine/funcs/engine_funcs.cpp` |
| 0x1801fe7e0 | `g_pLoaderCompletionEvent` | `engine/funcs/engine_funcs.cpp` |
| 0x1801feb80 | `Application::Application::_singleton` | `engine/funcs/engine_funcs.cpp:938`, `engine/funcs/engine_funcs.h:324` |

Eight of these are also reachable as **exported data symbols** and are usually fetched that
way instead: `?_singleton@MainLoop@Engine@@1PEAV12@EA`, `?_singleton@Device@Video@@0PEAV12@EA`,
`?_singleton@FileSystem@IO@@0PEAV12@EA`, `?_singleton@InternalFactory@Object@@0PEAV12@EA`,
`?_singleton@SaveLoad@Save@@0PEAV12@EA`,
`?_singleton@AchievementManager@Achievement@@0PEAV12@EA`,
`?_singleton@Device@Input@@0PEAV12@EA`, `?_singleton@Configuration@Input@@0PEAV12@EA`.
The remaining seventeen are address-only: a Runtime port must either export them or provide
an accessor, because nothing in the binary names them.
