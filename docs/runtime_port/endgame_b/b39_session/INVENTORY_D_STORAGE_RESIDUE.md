# B.3 step 9 — block 0D inventory: singleton / static storage, the residue that stays, file ownership

Agent 0D, 2026-09-27, read-only, main @ 3fc6d9f6 (clean). Answers `NEXT_SESSION.md` §3b items 5, 6, 7.
Every count names its script (§6 "How measured"); scripts and raw outputs are in
`C:/Users/USPC/AppData/Local/Temp/claude/b39/0D/` (`storage_sites.tsv`, `level2.tsv`, `exports.tsv`,
`regexe.tsv`, `efuncs.tsv`, `deadrows.tsv`). "Site" = a code-only occurrence (comments stripped) that
obtains the cell: the mangled name as a string literal, a hex literal inside `[va, va+size)`, or a use of
a named constant / `#define` whose initialiser is one of those. The generated publishers
(`rt_data_mint_tables.inc`, `rt_export_addrs.inc`, `cutover_manifest.inc`) never count.

## 0. Headline numbers

| item | count | measured by |
|---|---:|---|
| mint storage rows (`kMintStorage`) | **48** = 43 named + 5 by-address (`EXTRA_STORAGE_BY_ADDR`) | `storage.py` (the row regex of `rt_mint_gen.py --check`) |
| of them `?_singleton@` | 20 (incl. `SaveLoad@Save`) | same |
| storage rows that are `CUTOVER_REQUIRED` manifest rows | **37** (no storage row is WITHHELD) | `storage.py` joined to `cutover_manifest.inc` |
| manifest rows in all | 59 = 58 REQUIRED + 1 WITHHELD (`??_7Dialog@UI`); 37 storage + 20 vtable + 2 `_typeInfo` | same |
| first-level access sites over the 48 storage rows | **356**: runtime 279, standalone 33, engine 19, dx11 12, code 13 | `storage.py` |
| — by binding | `RT_DATA_SLOT` 166, "other" (named constant / wrapper / table) 60, `ResolveAddr(Runtime)` 49, `AtomicExport` 37, `ResolveExport` 33, `RtResolveCached` 7, `AtomicAddr` 1, `RegisterRuntime*` 3 | same |
| second-level: exe import slots holding a storage cell (`RegisterDispatchSlot(0x14015…, cell)`) | 9 slots (0x1401514f0, 0x140152368, 0x140151d18, 0x140152760, 0x140151610, 0x140151f30, 0x140151f38, 0x140151a80, 0x140151440) + 0x1401519e8 (Modification, read in engine_funcs.cpp:1592) | `level2.py` |
| second-level: `engine_funcs` storage globals | 23 globals, 566 use lines (+ `g_pMemAllocSingleton` 479, `g_ppAchievementManager` 22) | `efuncs.py` |
| rows with a `MintStorageCell` redirect today | **1** (`?_firstTypeInfo@`, rt_data_mint.cpp:634-638) | read |
| `rt_export_addrs.inc` rows | **4,926** (the file is 4,932 lines: 4 header + 2 footer) | `exports.py` |
| — LIVE (≥1 reader outside the mint) / MINT-ONLY / REG-ONLY / NONE | **1,045 / 1,023 / 671 / 2,187** | `exports.py` |
| `RegisterExeAddr` statements | **1,007** in 174 files (address_registry.cpp 54, 144 `reg_*.cpp` 873, elsewhere 77) | `regexe.py` |
| — reader-less (literal address) | **237** = 231 function + 5 type-id cell + 1 import slot (+10 table-loop expressions, not resolvable) | `regexe.py` |
| `engine_funcs.h` `extern` globals | **414**; zero-use **34** (not 18) | `efuncs.py` |
| `RT_DELDTOR` rows | **135** (grep -c 137 counts the `#define` and the comment line) | grep |

## 1. The 10 findings that change the plan

1. **The `MintStorageCell()` redirect cannot redirect a non-manifest row.** `RegisterMintedData()`
   (standalone/rt_data_mint.cpp:763-797) registers every storage row by name and address with the raw
   `s.storage`, NOT through `MintRowAt`/`MintStorageCell`. Only `RtCutoverPublishManifest()`
   (rt_data_mint.cpp:894 → `RtMintRegisterRow` :704 → `MintRowAt` :652) goes through the redirect, and it
   runs after and overwrites. So the redirect works for the 37 REQUIRED storage rows only; for
   `?_singleton@SaveLoad@Save@@` (no manifest row, NEXT_SESSION §3a) and the 5 by-address rows it would be a
   no-op. `MintStorage()` (memset + kInit seeding, :540-583) and `MintConstructedStatics()` (:510; `ConstructLoader`
   :434, `ConstructParticleRng` :495) also write `s.storage`, not the redirected cell.
2. **The mint runs AFTER every port init and overwrites.** `RtDataMintInit()` is the last call of
   `RuntimePortInit()` (standalone/resolve_standalone.cpp:393); explicit registrations are last-writer-wins
   (resolve_standalone.cpp:239-261). A block that makes `X::_singleton` a C++ static and registers it by name
   in its `*PortInit()` is silently replaced by the mint's zeroed cell. The only safe shapes: (a) convert
   EVERY reader of the row and retire the row in the same commit, or (b) route all three mint paths through
   `MintStorageCell`.
3. **Retiring a mint row needs a generator change, not a CSV column.** `rt_mint_gen.py` does not take its
   set from `wave9_data_harvest.csv` (the CSV is a cross-check only, rt_mint_gen.py:830-837); the set is the
   census ∪ the source scan ∪ **every exported `??_7`/`?_typeInfo@`/`?_singleton@`** (the all-exported-data
   union, rt_mint_gen.py:707-729) ∪ both `?_typeInfo@` closures (:741-823). A row with zero readers stays
   minted forever. The smallest mechanism is a RETIRED list the generator subtracts last (§4 C.2).
4. **`reg_mint_promoted.cpp` (3,257 lines) is dead code with a live parser.** `RtMintRegisterPromotedRows()`
   (reg_mint_promoted.cpp:228) has no caller (its header says so, :12); its SaveLoad publish (:711-714) never
   runs. The file survives only because `tools/cutover_manifest.py` parses its `RT_UNPROMOTABLE:` prose as
   the manifest's TAG_FILE (cutover_manifest.py:51). It is the storage retirement's to delete once the
   manifest has no gap row left.
5. **`?_singleton@Device@Video@@` alone is 65 first-level sites** (runtime/src/ui 27, video 12, dx11 8,
   graphics 7) plus `g_pVideoDeviceSingleton` 54 and IAT 0x140151a80 → `g_pSingletonExref_151a80` 15;
   `?_secondsPerTick@Timer@` is 56 sites (26 through `MainLoop.h:194`'s `SecondsPerTick` constant and
   per-file `SecondsPerTick()` wrappers). The class-static conversion is ~356 + ~1,100 second-level edits,
   not "a redirect per row".
6. **Every `RT_DATA_SLOT` is a lazy first-use cache** (`runtime/funcs/rt_port.h:142`: a lambda with a
   function-local `static void*`). The 166 `RT_DATA_SLOT` storage sites are B6-5 surface wherever the
   loader thread reaches them (`updateloop_loadresources.cpp:105/109/113/118`, `loops.cpp:295/303`,
   `gameupdateloop_serialize.cpp:173/175`); a class static removes that surface — a reason to convert, not
   a hazard.
7. **`_loader` and `_rng` are OBJECTS built by the mint**, not by C++: `Engine::Loader` is
   `Loader() = default` (runtime/types/engine/UpdateLoop.h:389) and is door-built by `ConstructLoader`
   (Thread ctor door + minted `??_7Loader@Engine@@`, which exists only in `EXTRA_VTABLES`
   rt_mint_gen.py:331-334); `engine_funcs.cpp:1974-1975` read two INTERIOR addresses (0x1801fe7c0,
   0x1801fe7e0) that the mint registers by hand (rt_data_mint.cpp:694-698, :784-795). `_rng` is seeded by
   `ConstructParticleRng` (:495) — making it a C++ static with a seeding ctor while the mint row remains is
   the B4b-2 double seed.
8. **0x1801fc6d0 is mislabelled.** The mint's note (rt_mint_gen.py:275) calls it "IO::Modification's
   loaded-DLL HMODULE"; retail xrefs are `Factory::UnloadCompilers` 0x1800bfad4/0x1800bfae6,
   `Factory::Shutdown` 0x1800c0604/0x1800c0616 and Engine Shutdown 0x1800db62d/0x1800db63f (Ghidra,
   Runtime-x64.dll) — it is Object::Factory's compiler-plugin HMODULE, as `factory_dtors.cpp:287` says.
9. **`ControllerLive::Dispatcher<Tag>` is NOT dead** — only its `vtbl[16]`, `Vtable()` and `Stub`
   (code/Game/Component/controller_state_table.h:149, 272-288, 291) are. `Dispatcher<Tag>::table` and the
   step functions are the body of `TableController` (controller_state_table.h:352-371) and 26 `…Dispatch::table`
   uses in 23 files. Deleting "the template" would break every controller.
10. **engine_funcs has 34 zero-use globals, not 18**: the 10 exe code doors CLOSE_NUMBERS lists; 7 zero-use
    type-id globals (the 5 listed + `g_TypeId_StorageComponent`, `g_TypeId_StateMachineComponent`,
    engine_funcs.cpp:139-140, never initialised); 6 ctor/dtor doors (`fn_Element_Ctor`,
    `fn_ElementDescription_Ctor/_Dtor` + `fn_DialogInstance_Dtor` engine_funcs.cpp:920, `fn_Goal_BaseCtor`
    :2275, `fn_Graphics_Geometry_Ctor` :2420 — each kept alive only by its EngineFuncsDump `printf`); 7
    Runtime-address doors (`fn_PostDrawPush/PostUpdatePush/ListCleanup/Map_Find/Map_Insert/ParseCommand/
    ParseValue`, engine_funcs.cpp:859-910); 4 never-initialised `fn_TownHallUI_Update*`.

Smaller ones: 85 of 160 `Register*Slot` import-slot registrations have no by-address reader (the MainLoop one,
0x140151440, among them); the `RT_DELDTOR` count is 135; 38 of the 237 reader-less `RegisterExeAddr` rows
register a function with no other use (deleting the row orphans the body — a new `-Wunused-function`
warning for a `static` one).

---

## 2. Question A — singleton / static storage

### A.1 The 48 storage rows (+ `?_singleton@SaveLoad@Save@@`, which IS one of the 48)

Columns: sites = first-level sites (count and directory split); bindings as classified by `lib.binding`
("other" = a named constant, a wrapper call or a table row); "published onward" = what a first-level site
hands on (an exe import slot registered with the cell, an `engine_funcs` global, a file static) with its use
lines; writer = who stores the value (read, file:line). Spelling = the class static it becomes (declaration
header; definition TU). All 37 manifest storage rows are REQUIRED; "doors" is the manifest's own literal count.

| # | row (mangled / address) | retail VA | size | class | holds | manifest | writer / initialiser | first-level sites (dirs) | bindings | published onward | redirect today | class-static spelling | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| A1 | `?_crcTable@CRC@System@@0PAIA` | 0x1801f9080 | 1024 | System::CRC | uint32[256] | — | none (crc.cpp builds its own table) | 0 (—) | — | — | no | — | no reader: drop from the mint (exclusion list); crc.cpp's table is the class's |
| A2 | `?_lkeyMap@Device@Input@@0PAEA` | 0x1801f9480 | 112 | Input::Device | uint8[0x70] | — | none | 0 (—) | — | — | no | — | no reader: drop via exclusion list |
| A3 | `?_keyMap@Device@Input@@0PAEA` | 0x1801f94f0 | 256 | Input::Device | uint8[256] VK→DIK | — | none in tree | 0 (—) | — | — | no | `static const uint8_t _keyMap[256];` if Input ports it | no reader: drop via exclusion list (or keep until Input reads it) |
| A4 | `?_singleton@AchievementManager@Achievement@@0PEAV12@EA` | 0x1801fc670 | 8 | Achievement::AchievementManager | ptr to singleton | REQUIRED (Achievement, doors=6) | RtAchievement_AchievementManager_Ctor engine_lifecycle.cpp:373 (=self); achievement_state.cpp:139 (=nullptr) | 5 (code/Game/Game=1; runtime/src/achievement=2; runtime/src/engine=1; standalone=1) | RT_DATA_SLOT=3; ResolveExport=2 | s_ppAchMgr (3 use lines); IAT 0x1401514f0 (1 use lines) | no | `static AchievementManager* _singleton;` runtime/types/achievement/AchievementManager.h; def runtime/src/achievement/achievement_manager.cpp | class static now (5 sites + IAT 0x1401514f0 → engine_funcs g_ppAchievementManager 22 uses) |
| A5 | `?_singleton@Application@1@0PEAV11@EA` | 0x1801fc678 | 8 | Engine::Application | ptr to singleton | REQUIRED (Application, doors=12) | Ctor_Application singleton_ctors.cpp:580 (Publish) | 12 (dx11=1; runtime/src/application=2; runtime/src/engine=2; runtime/src/input=4; runtime/src/ui=1; runtime/src/video=1; standalone=1) | AtomicExport=2; RT_DATA_SLOT=8; ResolveAddr(Runtime)=1; other=1 | IAT 0x140152368 (1 use lines) | no | `static Application* _singleton;` runtime/types/application/Application.h; def runtime/src/application/application_accessors.cpp | class static now (12 sites incl. dx11 devicedata.cpp:282 import row + IAT 0x140152368 → g_pAppSingleton 16) |
| A6 | `0x1801fc69c` | 0x1801fc69c | 4 | Graphics::GraphicsMesh (unexported) | int — TriangleList serialize counter | — | graphicsmesh_vtable.cpp SerializeCounter() (writes through the slot) | 1 (runtime/src/graphics=1) | ResolveAddr(Runtime)=1 | — | no | `static int _serializeCounter;` runtime/types/graphics/GraphicsMesh.h (file-static in graphicsmesh_vtable.cpp is enough — unexported) | class/file static now (1 site); retires the EXTRA_STORAGE_BY_ADDR entry |
| A7 | `0x1801fc6a0` | 0x1801fc6a0 | 8 | Graphics::GraphicsMesh (unexported) | GraphicsMesh* — 'current mesh' during Serialize | — | graphicsmesh_vtable.cpp SerializeCurrentMesh() | 1 (runtime/src/graphics=1) | ResolveAddr(Runtime)=1 | — | no | file static in graphicsmesh_vtable.cpp | file static now (1 site); retires EXTRA_STORAGE_BY_ADDR entry |
| A8 | `?_singleton@FileSystem@IO@@0PEAV12@EA` | 0x1801fc6a8 | 8 | IO::FileSystem | ptr to singleton | REQUIRED (IO, doors=28) | Ctor_FileSystem singleton_ctors.cpp:392 | 27 (code/Game/Game=2; engine=1; runtime/src/achievement=1; runtime/src/audio=1; runtime/src/engine=2; runtime/src/io=13; runtime/src/object=4; runtime/src/save=1; runtime/src/video=1; standalone=1) | RT_DATA_SLOT=22; ResolveExport=3; RtResolveCached=2 | s_ppFileSystem (1 use lines); s_pFileSystemSingleton (4 use lines); g_pFileSystemSingleton2 (7 use lines); IAT 0x140151d18 (2 use lines) | no | `static FileSystem* _singleton;` runtime/types/io/FileSystem.h; def runtime/src/io/ (filesystem ctor TU) | class static now (27 sites, 22 RT_DATA_SLOT — the most in IO; + IAT 0x140151d18 → g_pFileSystemSingleton 2, + g_pFileSystemSingleton2 8). Loader-thread reader: updateloop_loadresources.cpp:113 |
| A9 | `?_singleton@Modification@IO@@2PEAV12@EA` | 0x1801fc6b0 | 8 | IO::Modification | ptr to singleton (public static, `2`) | REQUIRED (IO, doors=6) | Ctor_Modification singleton_ctors.cpp:270; RtIo_Modification_Dtor iobase_vtables.cpp:540 | 7 (engine=1; runtime/src/engine=3; runtime/src/io=1; runtime/src/save=1; standalone=1) | RT_DATA_SLOT=5; ResolveAddr(Runtime)=1; other=1 | g_slot_ModificationSingleton (2 use lines) | no | `static Modification* _singleton;` (PUBLIC) runtime/types/io/Modification.h; def runtime/src/io/ | class static now (7 sites + IAT 0x1401519e8 → g_ppModificationSingleton 16; engine/types/Modification.h:112 constant; reg_mods.cpp:91 g_slot_). Loader-thread: loops.cpp:303, updateloop_loadresources.cpp:109 |
| A10 | `?_singleton@Persistence@IO@@0PEAV12@EA` | 0x1801fc6b8 | 8 | IO::Persistence | ptr to singleton | REQUIRED (IO, doors=13) | Ctor_Persistence singleton_ctors.cpp:461 | 14 (engine=1; runtime/src/achievement=1; runtime/src/audio=2; runtime/src/engine=2; runtime/src/input=4; runtime/src/io=2; runtime/src/save=1; runtime/src/video=1) | RT_DATA_SLOT=11; ResolveAddr(Runtime)=1; ResolveExport=2 | g_pPersistenceSingleton (8 use lines) | no | `static Persistence* _singleton;` runtime/types/io/Persistence.h; def runtime/src/io/ | class static now (14 sites + g_pPersistenceSingleton 25 use lines) |
| A11 | `?_singleton@ExternalFactory@Object@@0PEAV12@EA` | 0x1801fc6c8 | 8 | Object::ExternalFactory | ptr to singleton | REQUIRED (Object, doors=8) | Ctor_ExternalFactory singleton_ctors.cpp:323 | 9 (engine=1; runtime/src/engine=5; runtime/src/io=1; runtime/src/object=2) | AtomicExport=1; RT_DATA_SLOT=6; ResolveAddr(Runtime)=1; RtResolveCached=1 | g_pExternalFactorySingleton (8 use lines) | no | `static ExternalFactory* _singleton;` runtime/types/object/External.h; def runtime/src/object/external_class_doors.cpp (or external.cpp) | class static now (9 sites + g_pExternalFactorySingleton 183 use lines — the largest engine_funcs storage fan-out). LOADER THREAD: updateloop_loadresources.cpp:105, loops.cpp:295, every world-build ExternalList::Find |
| A12 | `0x1801fc6d0` | 0x1801fc6d0 | 8 | Object::Factory (unexported) — see note | HMODULE of the compiler plugin | — | retail writers not decoded; readers/clearers Factory::UnloadCompilers 0x1800bfad4/ae6, Factory::Shutdown 0x1800c0604/16, Engine Shutdown 0x1800db62d/3f (Ghidra xrefs) | 3 (code/Game/Game=1; runtime/src/engine=1; runtime/src/object=1) | ResolveAddr(Runtime)=3 | — | no | `static HMODULE _compilerModule;` private in Object::Factory (runtime/types/object/TypeInfo.h), def runtime/src/object/factory_dtors.cpp | class static now (3 sites: game_main.cpp:359, engine_lifecycle.cpp:851, factory_dtors.cpp:528); the mint's note mislabels it IO::Modification |
| A13 | `?_singleton@InternalFactory@Object@@0PEAV12@EA` | 0x1801fc6d8 | 8 | Object::InternalFactory | ptr to singleton | REQUIRED (Object, doors=4) | Ctor_InternalFactory singleton_ctors.cpp:360 | 5 (code/Game/Game=1; code/Game/Object=2; runtime/src/engine=1; runtime/src/object=1) | RT_DATA_SLOT=2; ResolveAddr(Runtime)=1; ResolveExport=1; other=1 | s_ppIntFactory (2 use lines) | no | `static InternalFactory* _singleton;` runtime/types/object/Internal.h; def runtime/src/object/internal.cpp | class static now (5 sites; gameupdateloop_serialize.cpp:173 is a LOADER-THREAD save-load reader, internal_factory_audit.cpp:16 constant) |
| A14 | `?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA` | 0x1801fc6e0 | 8 | Object::BaseTypeInfo | head of the reflection chain | REQUIRED (Object, doors=1) | CRT-init DEFINE_TYPE statics + MintStorage splice rt_data_mint.cpp:571 | 6 (code/Game/Game=1; standalone=5) | ResolveAddr(Runtime)=3; other=3 | — | YES (only this one) | ALREADY `static BaseTypeInfo* _firstTypeInfo;` runtime/types/object/TypeInfo.h:461 | REDIRECTED ALREADY (rt_data_mint.cpp:635); row stays until the 3 by-address readers (game_main.cpp:484, main.cpp:670, main.cpp:743) read the static; then the redirect, the splice and the row go together with the _typeInfo rows (D10 last) |
| A15 | `?_singleton@Factory@Object@@0PEAV12@EA` | 0x1801fc6e8 | 8 | Object::Factory | ptr to singleton | REQUIRED (Object, doors=11) | Ctor_Factory singleton_ctors.cpp:291 | 11 (runtime/src/achievement=1; runtime/src/engine=2; runtime/src/io=1; runtime/src/object=5; runtime/src/ui=1; standalone=1) | RT_DATA_SLOT=6; ResolveAddr(Runtime)=1; ResolveExport=1; RtResolveCached=3 | IAT 0x140152760 (2 use lines) | no | `static Factory* _singleton;` runtime/types/object/TypeInfo.h; def runtime/src/object/type_reflection.cpp | class static now (11 sites + IAT 0x140152760 → g_pFactorySingleton 38 + nomadcomponent.cpp:101) |
| A16 | `?_singleton@SaveLoad@Save@@0PEAV12@EA` | 0x1801fc6f0 | 8 | Save::SaveLoad | ptr to singleton | — | saveload.cpp:207 (=self), saveload.cpp:432 (=nullptr), saveload_internal.h:157 | 9 (engine=1; runtime/src/engine=1; runtime/src/save=1; standalone=6) | RT_DATA_SLOT=2; RegisterRuntimeAddr=1; RegisterRuntimeExport=1; ResolveExport=3; other=2 | g_pSaveLoadSingleton (10 use lines); IAT 0x140151610 (1 use lines); s_pSingleton (5 use lines) | no | `static SaveLoad* _singleton;` runtime/types/save/SaveLoad.h; def runtime/src/save/saveload.cpp | class static now (9 sites + IAT 0x140151610 + g_pSaveLoadSingleton 27). NO MANIFEST ROW → MintStorageCell alone cannot redirect it (RegisterMintedData publishes s.storage); reg_mint_promoted.cpp:711-714 is dead code (RtMintRegisterPromotedRows has no caller) |
| A17 | `?_singleton@Configuration@System@@0PEAV12@EA` | 0x1801fc6f8 | 8 | System::Configuration | ptr to singleton | REQUIRED (System, doors=5) | RtSystem_Configuration_Ctor engine_lifecycle.cpp:342 | 4 (runtime/src/engine=2; runtime/src/io=2) | RT_DATA_SLOT=4 | — | no | `static Configuration* _singleton;` runtime/types/system/Configuration.h; def runtime/src/system/ | class static now (4 sites; updateloop_loadresources.cpp:118 is loader-thread) |
| A18 | `?_exceptionCallback@Debug@System@@0P6A?AW4ErrorResponse@12@PEAXPEADH@ZEA` | 0x1801fc700 | 8 | System::Debug (unported) | fn ptr | — | none in tree | 0 (—) | — | — | no | — | stays (0 readers; System::Debug unported) — or drop from the mint via the exclusion list (nothing reads it) |
| A19 | `?_disableHandler@Debug@System@@0_NA` | 0x1801fc708 | 1 | System::Debug (unported) | bool | REQUIRED (System, doors=2) | enumerate_video_modes.cpp (the one RT_DATA_SLOT site raises/lowers it) | 1 (runtime/src/video=1) | RT_DATA_SLOT=1 | — | no | — (B34B §4.1: NO, nothing to convert it to) | stays (System::Debug unported); or a file static in enumerate_video_modes.cpp if the block owner agrees — 1 site |
| A20 | `?_hideDisplay@Error@System@@0_NA` | 0x1801fc709 | 1 | System::Error | bool | REQUIRED (System, doors=3) | ErrorHideDisplaySlot engine_lifecycle.cpp:691 (=true); error.cpp | 2 (runtime/src/engine=1; runtime/src/system=1) | RT_DATA_SLOT=2 | — | no | `static bool _hideDisplay;` runtime/types/system/Error.h; def runtime/src/system/error.cpp | class static now (2 sites) — the three Error statics together |
| A21 | `?_stopApplication@Error@System@@0_NA` | 0x1801fc70a | 1 | System::Error | bool | REQUIRED (System, doors=2) | error.cpp SetText | 1 (runtime/src/system=1) | RT_DATA_SLOT=1 | — | no | `static bool _stopApplication;` Error.h / error.cpp | class static now (1 site) |
| A22 | `?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA` | 0x1801fc70b | 1 | Graphics::MeshInstance | bool | REQUIRED (Graphics, doors=4) | no writer in the tree (retail setter unported); zero-initialised | 4 (code/Game/Component=1; runtime/src/graphics=1; standalone=2) | ResolveAddr(Runtime)=1; ResolveExport=3 | s_pRootOnlyUpdates (4 use lines); IAT 0x140151f30 (3 use lines); s_rootOnly (8 use lines) | no | `static bool _rootOnlyUpdates;` runtime/types/graphics/Mesh.h; def runtime/src/graphics/ | class static now (4 sites + IAT 0x140151f30 → gameupdateloop.cpp:440 g_pp_rootOnlyUpdates, citizenai_executor.cpp:343, perf_experimental.cpp:105) |
| A23 | `?_exceptionHandler@Debug@System@@0PEAXEA` | 0x1801fc710 | 8 | System::Debug (unported) | void* | — | none | 0 (—) | — | — | no | — | stays / exclusion list (0 readers) |
| A24 | `?_singleton@MemoryAllocator@System@@0PEAV12@EA` | 0x1801fc718 | 8 | System::MemoryAllocator | ptr to singleton | REQUIRED (System, doors=4) | allocator.cpp:654 (=self) / :659 (=nullptr); Ctor_MemoryAllocator singleton_ctors.cpp:239 | 7 (code/Game/Game=1; engine=1; runtime/src/engine=1; runtime/src/system=1; runtime/types/engine=1; standalone=2) | RT_DATA_SLOT=2; ResolveAddr(Runtime)=4; other=1 | g_pMemAllocSingleton (12 use lines) | no | `static MemoryAllocator* _singleton;` runtime/types/system/MemoryAllocator.h; def runtime/src/system/allocator.cpp | needs carry-over 1 (MemoryAllocator, deferred) first: 7 sites but g_pMemAllocSingleton has 479 use lines (the MemAlloc door fan-out) |
| A25 | `?_errorString@Error@System@@0PADA` | 0x1801fc720 | 8192 | System::Error | char[0x2000] BUFFER (not a pointer) | REQUIRED (System, doors=2) | error.cpp SetText | 2 (code/Game/Game=1; runtime/src/system=1) | RT_DATA_SLOT=1; ResolveAddr(Runtime)=1 | — | no | `static char _errorString[0x2000];` Error.h / error.cpp — redirect must return the ARRAY address | class static now (2 sites: error.cpp, game_main.cpp:465) |
| A26 | `?_ticksPerSecond@Timer@System@@0_JA` | 0x1801fe720 | 8 | System::Timer | int64 (EXTRA_INIT 1000000) | REQUIRED (System, doors=2) | PublishTimerStatics timer.cpp:188 (copy of timer.cpp's own global) + mint kInit seed | 3 (engine=1; runtime/src/engine=1; runtime/src/system=1) | RT_DATA_SLOT=1; ResolveAddr(Runtime)=2 | g_Timer_ticksPerSecond (10 use lines) | no | `static int64_t _ticksPerSecond;` runtime/types/system/Timer.h; def runtime/src/system/timer.cpp (initialiser 1000000 there, EXTRA_INIT retires) | class static now — FIVE TOGETHER (B34B §4.1); 3 sites + g_Timer_ticksPerSecond 17 use lines |
| A27 | `?_secondsPerTick@Timer@System@@0NA` | 0x1801fe728 | 8 | System::Timer | double (EXTRA_INIT 1e-6) | REQUIRED (System, doors=20) | PublishTimerStatics timer.cpp:190 + mint kInit seed | 56 (engine=1; runtime/src/audio=7; runtime/src/engine=3; runtime/src/graphics=22; runtime/src/input=1; runtime/src/save=3; runtime/src/system=1; runtime/src/ui=17; runtime/types/engine=1) | AtomicExport=11; RT_DATA_SLOT=16; ResolveAddr(Runtime)=3; other=26 | g_Timer_secondsPerTick (11 use lines) | no | `static double _secondsPerTick;` Timer.h / timer.cpp | class static now — five together; 56 sites (26 through MainLoop.h:194's `SecondsPerTick` constant and per-file SecondsPerTick() wrappers) + g_Timer_secondsPerTick 11. Divided by in 3 gameplay components: never let it read 0 |
| A28 | `?_systemTicksPerSecond@Timer@System@@0_JA` | 0x1801fe730 | 8 | System::Timer | int64 (QPF) | REQUIRED (System, doors=2) | PublishTimerStatics timer.cpp:192 | 3 (engine=1; runtime/src/engine=1; runtime/src/system=1) | RT_DATA_SLOT=1; ResolveAddr(Runtime)=2 | g_Timer_systemTicksPerSecond (2 use lines) | no | `static int64_t _systemTicksPerSecond;` | class static now — five together (3 sites + g_Timer_systemTicksPerSecond 2) |
| A29 | `?_systemInitialTick@Timer@System@@0_JA` | 0x1801fe738 | 8 | System::Timer | int64 (QPC at startup) | REQUIRED (System, doors=2) | PublishTimerStatics timer.cpp:194 | 3 (engine=1; runtime/src/engine=1; runtime/src/system=1) | RT_DATA_SLOT=1; ResolveAddr(Runtime)=2 | g_Timer_systemInitialTick (2 use lines) | no | `static int64_t _systemInitialTick;` | class static now — five together (3 sites + g_Timer_systemInitialTick 2) |
| A30 | `?_systemSecondsPerTick@Timer@System@@0NA` | 0x1801fe740 | 8 | System::Timer | double | REQUIRED (System, doors=2) | PublishTimerStatics timer.cpp:196 | 1 (runtime/src/system=1) | RT_DATA_SLOT=1 | — | no | `static double _systemSecondsPerTick;` | class static now — five together (1 site) |
| A31 | `?_singleton@Engine@1@0PEAV11@EA` | 0x1801fe748 | 8 | Engine::Engine | ptr to singleton | REQUIRED (Engine, doors=4) | RtEngine_Engine_Ctor engine_lifecycle.cpp:439 / :469 | 4 (runtime/src/engine=3; runtime/types/engine=1) | AtomicExport=1; RT_DATA_SLOT=2; other=1 | — | no | `static Engine* _singleton;` runtime/types/engine/Engine.h; def runtime/src/engine/engine_lifecycle.cpp | class static now (4 sites) |
| A32 | `?_singleton@MainLoop@Engine@@1PEAV12@EA` | 0x1801fe750 | 8 | Engine::MainLoop | ptr to singleton (protected) | REQUIRED (Engine, doors=8) | engine.cpp:467 (=self) | 10 (engine=1; runtime/src/engine=3; runtime/src/save=1; runtime/src/ui=1; runtime/types/engine=1; standalone=3) | RT_DATA_SLOT=3; ResolveAddr(Runtime)=1; ResolveExport=2; other=4 | IAT 0x140151440 (0 use lines) | no | `static MainLoop* _singleton;` (protected + accessor) runtime/types/engine/MainLoop.h; def runtime/src/engine/engine.cpp | class static now (10 sites + g_pMainLoopSingleton 38; IAT 0x140151440 registered with 0 readers; modal_driver.cpp:117 constant) |
| A33 | `?_singleton@Configuration@Audio@@0PEAV12@EA` | 0x1801fe758 | 8 | Audio::Configuration | ptr to singleton | REQUIRED (Audio, doors=5) | Ctor_AudioConfiguration singleton_ctors.cpp:677; engine_platform.cpp:275 / audio.cpp:1241 (=nullptr) | 5 (engine=1; runtime/src/audio=2; runtime/src/engine=2) | RT_DATA_SLOT=4; ResolveAddr(Runtime)=1 | — | no | `static Configuration* _singleton;` runtime/types/audio/Audio.h; def runtime/src/audio/audio.cpp | class static now (5 sites + g_pAudioConfigSingleton 12) |
| A34 | `?_singleton@Device@Audio@@0PEAV12@EA` | 0x1801fe760 | 8 | Audio::Device | ptr to singleton | REQUIRED (Audio, doors=6) | Ctor_AudioDevice singleton_ctors.cpp:774 | 5 (runtime/src/audio=4; runtime/src/engine=1) | RT_DATA_SLOT=5 | — | no | `static Device* _singleton;` runtime/types/audio/Device.h; def runtime/src/audio/ | class static now (5 sites) |
| A35 | `?_singleton@Configuration@Input@@0PEAV12@EA` | 0x1801fe768 | 8 | Input::Configuration | ptr to singleton | REQUIRED (Input, doors=11) | Ctor_InputConfiguration singleton_ctors.cpp:655; input.cpp:571, engine_platform.cpp:231 (=nullptr) | 10 (engine=1; runtime/src/engine=2; runtime/src/input=3; runtime/src/ui=4) | AtomicExport=4; RT_DATA_SLOT=5; ResolveExport=1 | — | no | `static Configuration* _singleton;` runtime/types/input/Input.h; def runtime/src/input/input.cpp | class static now (10 sites + g_pInputConfigSingleton 11) |
| A36 | `?_singleton@Device@Input@@0PEAV12@EA` | 0x1801fe770 | 8 | Input::Device | ptr to singleton | REQUIRED (Input, doors=11) | Ctor_InputDevice singleton_ctors.cpp:632 | 10 (engine=1; runtime/src/engine=1; runtime/src/input=2; runtime/src/ui=5; standalone=1) | AtomicExport=4; RT_DATA_SLOT=4; ResolveExport=2 | — | no | `static Device* _singleton;` runtime/types/input/Device.h; def runtime/src/input/ | class static now (10 sites + g_pInputDeviceSingleton 11; mouse_inject.cpp:350) |
| A37 | `?_frameIndex@ParticleInstance@Graphics@@1HA` | 0x1801fe77c | 4 | Graphics::ParticleInstance | int (protected, `1`) | REQUIRED (Graphics, doors=4) | RtFrameIndexSlot animation.cpp:217 (increments) | 5 (code/Game/Component=1; runtime/src/graphics=3; standalone=1) | ResolveAddr(Runtime)=1; ResolveExport=3; other=1 | s_pFrameIndex (3 use lines); IAT 0x140151f38 (1 use lines) | no | `static int _frameIndex;` (protected) runtime/types/graphics/ParticleInstance.h; def runtime/src/graphics/animation.cpp | class static now (5 sites + IAT 0x140151f38 → gameupdateloop.cpp:441 g_pp_frameIndex; particlecomponent.cpp:575) |
| A38 | `?_firstSurface@ImageBuffer@Video@@0PEAV12@EA` | 0x1801fe780 | 8 | Video::ImageBuffer | head of live-surface list | REQUIRED (Video, doors=3) | PlatformConstruct/Destruct dynamic_resources.cpp:160 / buffers.cpp:112 | 2 (runtime/src/video=2) | RT_DATA_SLOT=1; ResolveExport=1 | — | no | `static ImageBuffer* _firstSurface;` runtime/types/video/ImageBuffer.h; def runtime/src/video/dynamic_resources.cpp | class static now (2 sites) — the four list heads together (device-lost walks them) |
| A39 | `?_firstBuffer@IndexBuffer@Video@@0PEAV12@EA` | 0x1801fe788 | 8 | Video::IndexBuffer | list head | REQUIRED (Video, doors=3) | dynamic_resources.cpp:165 / buffers.cpp:117 | 2 (runtime/src/video=2) | RT_DATA_SLOT=1; ResolveExport=1 | — | no | `static IndexBuffer* _firstBuffer;` runtime/types/video/Buffers.h | class static now (2 sites) |
| A40 | `?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA` | 0x1801fe790 | 8 | Video::RenderBuffer | list head | REQUIRED (Video, doors=3) | dynamic_resources.cpp / buffers.cpp | 2 (runtime/src/video=2) | RT_DATA_SLOT=1; ResolveExport=1 | — | no | `static RenderBuffer* _firstRenderBuffer;` runtime/types/video/Device.h | class static now (2 sites) |
| A41 | `?_firstBuffer@VertexBuffer@Video@@0PEAV12@EA` | 0x1801fe798 | 8 | Video::VertexBuffer | list head | REQUIRED (Video, doors=3) | dynamic_resources.cpp:170 / buffers.cpp:122 | 2 (runtime/src/video=2) | RT_DATA_SLOT=1; ResolveExport=1 | — | no | `static VertexBuffer* _firstBuffer;` runtime/types/video/Buffers.h | class static now (2 sites) |
| A42 | `?_singleton@Configuration@Video@@0PEAV12@EA` | 0x1801fe7a0 | 8 | Video::Configuration | ptr to singleton | REQUIRED (Video, doors=16) | Ctor_VideoConfiguration singleton_ctors.cpp:555; engine_platform.cpp:248 (=nullptr) | 17 (dx11=3; engine=1; runtime/src/engine=2; runtime/src/graphics=3; runtime/src/save=1; runtime/src/video=6; standalone=1) | RT_DATA_SLOT=12; ResolveAddr(Runtime)=2; ResolveExport=1; other=2 | — | no | `static Configuration* _singleton;` runtime/types/video/Device.h; def runtime/src/video/device.cpp | class static now (17 sites: dx11 ×3 across the DLL-replacement seam — contextdata.cpp:245, devicedata.cpp:283, imagebufferdata.cpp:217; + g_pVideoConfigSingleton 18) |
| A43 | `?_singleton@Device@Video@@0PEAV12@EA` | 0x1801fe7a8 | 8 | Video::Device | ptr to singleton | REQUIRED (Video, doors=37) | Ctor_VideoDevice singleton_ctors.cpp:510 (CMOVE: only for type 0) | 65 (dx11=8; engine=1; runtime/src/engine=3; runtime/src/graphics=7; runtime/src/save=3; runtime/src/ui=27; runtime/src/video=12; runtime/types/engine=1; standalone=3) | AtomicAddr=1; AtomicExport=13; RT_DATA_SLOT=26; ResolveAddr(Runtime)=8; ResolveExport=2; RtResolveCached=1; other=14 | g_pVideoDeviceSingleton (7 use lines); IAT 0x140151a80 (1 use lines) | no | `static Device* _singleton;` runtime/types/video/Device.h; def runtime/src/video/device.cpp | class static now — the biggest: 65 sites (runtime/ui 27, video 12, dx11 8) + IAT 0x140151a80 → g_pSingletonExref_151a80 15 + g_pVideoDeviceSingleton 54. dx11 reads it by NAME (imagebufferdata.cpp:216, devicedata.cpp:284) and by ADDRESS (buffers.cpp:171, renderstatedata.cpp:329, dx11_backend_access.h:90/98) |
| A44 | `?_singleton@AnimationManager@Graphics@@0PEAV12@EA` | 0x1801fe7b0 | 8 | Graphics::AnimationManager | ptr to singleton | REQUIRED (Graphics, doors=6) | animation.cpp:340 (=self) / :364 (=nullptr) | 7 (runtime/src/graphics=7) | RT_DATA_SLOT=5; ResolveAddr(Runtime)=2 | — | no | `static AnimationManager* _singleton;` runtime/types/graphics/AnimationManager.h (the header says why it is absent today, :237); def runtime/src/graphics/animation.cpp | class static now (7 sites, all runtime/src/graphics) |
| A45 | `?_loader@LoaderLoop@Engine@@1VLoader@2@A` | 0x1801fe7b8 | 56 | Engine::LoaderLoop | Engine::Loader OBJECT (0x38) — Thread + vptr + _loop | REQUIRED (Engine, doors=2) | the MINT: ConstructLoader rt_data_mint.cpp:434 (Thread ctor door + minted ??_7Loader@Engine) + interior doors +0x08/+0x28 (rt_data_mint.cpp:694-698) | 7 (engine=3; runtime/src/engine=1; standalone=3) | AtomicExport=1; RegisterRuntimeAddr=1; ResolveAddr(Runtime)=3; other=2 | g_pLoader (10 use lines); g_pLoaderFlags (2 use lines); g_pLoaderCompletionEvent (3 use lines) | no | `static Loader _loader;` (protected) runtime/types/engine/UpdateLoop.h LoaderLoop; def runtime/src/engine/loops.cpp | needs a REAL `Engine::Loader()` ctor (`: Thread(0x40000), _loop(nullptr)`) and a real `System::Thread(unsigned)` first — today `Loader() = default` (UpdateLoop.h:389) and the object is door-built; then 7 sites + engine_funcs g_pLoader 9, g_pLoaderFlags 2, g_pLoaderCompletionEvent 4 (interior addresses 0x1801fe7c0 / 0x1801fe7e0). LOADER THREAD object; KNOWN_CTORS + EXTRA_VTABLES (??_7Loader@Engine) retire with it |
| A46 | `?_rng@Particle@Graphics@@1VRandom@System@@A` | 0x1801fe7f0 | 2500 | Graphics::Particle | System::Random OBJECT (2500 B, MT state) | REQUIRED (Graphics, doors=4) | the MINT: ConstructParticleRng rt_data_mint.cpp:495 (seeds from time(), transcribes initialiser 0x180149650) | 4 (code/Game/Game=1; runtime/src/graphics=2; standalone=1) | ResolveExport=3; other=1 | — | no | `static System::Random _rng;` (protected) runtime/types/graphics/Particle.h; def runtime/src/graphics/ (TU with retail's seeding ctor) | class static now ONLY with the double-seed rule: `System::Random` must have ONE seeding ctor run exactly once (CRT init), and ConstructParticleRng + the KNOWN_CTORS row + the mint row are deleted in the SAME commit (4 sites: tornado.cpp:344 f.rng, two graphics ParticleRng()) |
| A47 | `0x1801ff30c` | 0x1801ff30c | 4 | Graphics::MeshInstance (unexported) | static-init guard (bit 0) | — | wave29_lane_slots.cpp UpdateInitGuard() | 1 (runtime/src/graphics=1) | ResolveAddr(Runtime)=1 | — | no | function-local or file static in the MeshInstance::Update TU — a C++ function-local static IS retail's shape here (guard+table) | file static now (1 site) — with 0x1801ff310, together |
| A48 | `0x1801ff310` | 0x1801ff310 | 1344 | Graphics::MeshInstance (unexported) | 48 × Math::Transform identity table (0x540) | — | wave29_lane_slots.cpp IdentityTable() lazily fills | 1 (runtime/src/graphics=1) | ResolveAddr(Runtime)=1 | — | no | file static `static Math::Transform s_identity[48];` filled at static init (B6-5), not first use | file static now (1 site) — with the guard; MeshInstance::Update runs from Scene update (main thread) — verify no loader-thread reach before keeping the lazy fill |

### A.2 The 22 non-storage manifest rows (vtables, `_typeInfo`) — for completeness

| # | manifest row (non-storage) | VA | kind | manifest | sites (dirs) | bindings | verdict |
|---|---|---|---|---|---|---|---|
| M1 | `??_7Internal@Object@@6B@` | 0x180154720 | vtable | REQUIRED (Object, doors=3) | 4 (runtime/src/object=4) | ResolveExport=3; other=1 | retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M2 | `??_7Base@Object@@6B@` | 0x180154788 | vtable | REQUIRED (Object, doors=63) | 97 (runtime/src/achievement=4; runtime/src/audio=5; runtime/src/collision2d=6; runtime/src/graphics=14; runtime/src/io=1; runtime/src/object=10; runtime/src/pathfinding=3; runtime/src/picking=3; runtime/src/save=1; runtime/src/ui=44; runtime/src/video=4; runtime/types/ui=2) | AtomicExport=7; RT_DATA_SLOT=30; RT_EXPORT_ONCE=2; ResolveExport=9; other=49 | retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M3 | `??_7External@Object@@6B@` | 0x1801547a0 | vtable | REQUIRED (Object, doors=60) | 92 (runtime/src/achievement=4; runtime/src/audio=5; runtime/src/collision2d=6; runtime/src/graphics=14; runtime/src/io=1; runtime/src/object=5; runtime/src/pathfinding=3; runtime/src/picking=3; runtime/src/save=1; runtime/src/ui=44; runtime/src/video=4; runtime/types/ui=2) | AtomicExport=7; RT_DATA_SLOT=29; RT_EXPORT_ONCE=2; ResolveExport=6; other=48 | retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M4 | `??_7Stream@IO@@6B@` | 0x1801549a0 | vtable | REQUIRED (IO, doors=10) | 13 (runtime/src/io=6; runtime/src/object=4; runtime/src/save=2; runtime/src/ui=1) | AtomicExport=1; RT_DATA_SLOT=7; RT_VTABLE_ONCE=2; ResolveAddr(Runtime)=1; ResolveExport=1; other=1 | retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M5 | `??_7CompiledExternal@Object@@6B@` | 0x180154a30 | vtable | REQUIRED (Object, doors=8) | 11 (runtime/src/audio=4; runtime/src/graphics=3; runtime/src/object=2; runtime/src/video=2) | RT_DATA_SLOT=5; other=6 | retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M6 | `??_7ReaderWriter@IO@@6B@` | 0x1801553b0 | vtable | REQUIRED (IO, doors=6) | 5 (runtime/src/io=4; runtime/src/object=1) | RT_DATA_SLOT=1; ResolveExport=3; other=1 | retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M7 | `??_7Dialog@UI@@6B@` | 0x1801562a0 | vtable | WITHHELD (UI, doors=5) | 3 (runtime/src/ui=3) | RT_DATA_SLOT=1; ResolveExport=2 | WITHHELD (permanent harm) — never published; retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M8 | `??_7DialogController@UI@@6B@` | 0x1801565f0 | vtable | REQUIRED (UI, doors=2) | 1 (runtime/src/ui=1) | ResolveExport=1 | retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M9 | `??_7RenderBufferData@Video@@6B@` | 0x180158a28 | vtable | REQUIRED (Video, doors=2) | 1 (runtime/src/video=1) | ResolveExport=1 | retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M10 | `??_7VideoProgramData@Video@@6B@` | 0x180158a48 | vtable | REQUIRED (Video, doors=2) | 1 (runtime/src/video=1) | ResolveExport=1 | retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M11 | `??_7RenderStateData@Video@@6B@` | 0x180158a60 | vtable | REQUIRED (Video, doors=3) | 1 (runtime/src/video=1) | ResolveExport=1 | retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M12 | `??_7PixelProgramData@Video@@6B@` | 0x180158a78 | vtable | REQUIRED (Video, doors=2) | 1 (runtime/src/video=1) | ResolveExport=1 | retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M13 | `??_7VertexProgramData@Video@@6B@` | 0x180158a90 | vtable | REQUIRED (Video, doors=2) | 1 (runtime/src/video=1) | ResolveExport=1 | retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M14 | `??_7VertexLayoutData@Video@@6B@` | 0x180158aa8 | vtable | REQUIRED (Video, doors=2) | 1 (runtime/src/video=1) | ResolveExport=1 | retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M15 | `??_7VertexBufferData@Video@@6B@` | 0x180158ac0 | vtable | REQUIRED (Video, doors=2) | 1 (runtime/src/video=1) | ResolveExport=1 | retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M16 | `??_7IndexBufferData@Video@@6B@` | 0x180158af8 | vtable | REQUIRED (Video, doors=2) | 1 (runtime/src/video=1) | ResolveExport=1 | retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M17 | `??_7ConstantBufferData@Video@@6B@` | 0x180158b30 | vtable | REQUIRED (Video, doors=2) | 1 (runtime/src/video=1) | ResolveExport=1 | retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M18 | `??_7ImageBufferData@Video@@6B@` | 0x180158b50 | vtable | REQUIRED (Video, doors=2) | 1 (runtime/src/video=1) | ResolveExport=1 | retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M19 | `??_7ContextData@Video@@6B@` | 0x180158b90 | vtable | REQUIRED (Video, doors=2) | 1 (runtime/src/video=1) | ResolveExport=1 | retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M20 | `??_7DeviceData@Video@@6B@` | 0x180158c30 | vtable | REQUIRED (Video, doors=2) | 1 (runtime/src/video=1) | ResolveExport=1 | retires with its mint row (D10: after every stamper/reader converts) — 0A/0B's inventory; the manifest row goes when the census stops seeing the name |
| M21 | `?_typeInfo@CompiledExternal@Object@@0VBaseTypeInfo@2@A` | 0x1801f9b00 | typeinfo | REQUIRED (Object, doors=3) | 2 (runtime/src/object=2) | RT_DATA_SLOT=2 | retires with the _typeInfo rows (D10), after the reflection fold |
| M22 | `?_typeInfo@Internal@Object@@0VBaseTypeInfo@2@A` | 0x1801f9c18 | typeinfo | REQUIRED (Object, doors=3) | 2 (runtime/src/object=2) | RT_DATA_SLOT=2 | retires with the _typeInfo rows (D10), after the reflection fold |

### A.3 Every first-level site, per row (file:line binding [how])

- `?_crcTable@CRC@System@@0PAIA`: NONE
- `?_lkeyMap@Device@Input@@0PAEA`: NONE
- `?_keyMap@Device@Input@@0PAEA`: NONE
- `?_singleton@AchievementManager@Achievement@@0PEAV12@EA`: code/Game/Game/gameupdateloop_serialize.cpp:175 ResolveExport, runtime/src/achievement/achievement_manager.cpp:100 RT_DATA_SLOT, runtime/src/achievement/achievement_state.cpp:106 RT_DATA_SLOT, runtime/src/engine/engine_lifecycle.cpp:299 RT_DATA_SLOT, standalone/reg_unreg_lane1.cpp:205 ResolveExport
- `?_singleton@Application@1@0PEAV11@EA`: dx11/src/devicedata.cpp:282 other, runtime/src/application/application_accessors.cpp:50 RT_DATA_SLOT, runtime/src/application/platform_startup_win.cpp:117 RT_DATA_SLOT, runtime/src/engine/loops.cpp:1151 AtomicExport, runtime/src/engine/singleton_ctors.cpp:161 RT_DATA_SLOT, runtime/src/input/input_device_update.cpp:173 RT_DATA_SLOT, runtime/src/input/input_platform_shutdown_win.cpp:167 RT_DATA_SLOT, runtime/src/input/input_platform_startup_win.cpp:111 RT_DATA_SLOT, runtime/src/input/input_win.cpp:129 RT_DATA_SLOT, runtime/src/ui/userinterface.cpp:299 AtomicExport, runtime/src/video/device_startup.cpp:157 RT_DATA_SLOT, standalone/address_registry.cpp:1285 ResolveAddr(Runtime) [addr]
- `0x1801fc69c`: runtime/src/graphics/graphicsmesh_vtable.cpp:217 ResolveAddr(Runtime) [addr]
- `0x1801fc6a0`: runtime/src/graphics/graphicsmesh_vtable.cpp:222 ResolveAddr(Runtime) [addr]
- `?_singleton@FileSystem@IO@@0PEAV12@EA`: code/Game/Game/gameupdateloop.cpp:1193 RT_DATA_SLOT, code/Game/Game/w2_takescreenshot.cpp:162 ResolveExport, engine/funcs/engine_funcs.cpp:1184 ResolveExport, runtime/src/achievement/achievement_manager.cpp:113 RT_DATA_SLOT, runtime/src/audio/vtbatch7b_stream_run_win.cpp:266 RT_DATA_SLOT, runtime/src/engine/singleton_ctors.cpp:157 RT_DATA_SLOT, runtime/src/engine/updateloop_loadresources.cpp:113 RT_DATA_SLOT, runtime/src/io/closure_batch2b_win.cpp:103 RT_DATA_SLOT, runtime/src/io/compressed_stream_write.cpp:845 RT_DATA_SLOT, runtime/src/io/compressedcopyfilestream.cpp:142 RT_DATA_SLOT, runtime/src/io/file_stream.cpp:216 RT_DATA_SLOT, runtime/src/io/filesystem_packages.cpp:366 RT_DATA_SLOT, runtime/src/io/modification_refresh.cpp:924 RT_DATA_SLOT, runtime/src/io/modification_setupfilesystem.cpp:433 RtResolveCached, runtime/src/io/package_file_read_win.cpp:280 RT_DATA_SLOT, runtime/src/io/persistence_saveinfo_job.cpp:182 RT_DATA_SLOT, runtime/src/io/persistence_savelist_job.cpp:168 RtResolveCached, runtime/src/io/persistence_startup.cpp:99 RT_DATA_SLOT, runtime/src/io/persistence_worker_win.cpp:161 RT_DATA_SLOT, runtime/src/io/text_file_stream_win.cpp:128 RT_DATA_SLOT, runtime/src/object/external_compiled.cpp:94 RT_DATA_SLOT, runtime/src/object/external_load.cpp:202 RT_DATA_SLOT, runtime/src/object/external_path_win.cpp:139 RT_DATA_SLOT, runtime/src/object/external_savetext_win.cpp:194 RT_DATA_SLOT, runtime/src/save/saveload_internal.h:117 RT_DATA_SLOT, runtime/src/video/save_image_to_file.cpp:136 RT_DATA_SLOT, standalone/reg_unreg_lane1.cpp:182 ResolveExport
- `?_singleton@Modification@IO@@2PEAV12@EA`: engine/types/Modification.h:112 other [addr const-def Singleton], runtime/src/engine/loops.cpp:303 RT_DATA_SLOT, runtime/src/engine/singleton_ctors.cpp:153 RT_DATA_SLOT, runtime/src/engine/updateloop_loadresources.cpp:109 RT_DATA_SLOT, runtime/src/io/iobase_vtables.cpp:540 RT_DATA_SLOT, runtime/src/save/saveload_internal.h:121 RT_DATA_SLOT, standalone/reg_mods.cpp:91 ResolveAddr(Runtime) [addr]
- `?_singleton@Persistence@IO@@0PEAV12@EA`: engine/funcs/engine_funcs.cpp:915 ResolveAddr(Runtime) [addr], runtime/src/achievement/achievement_state.cpp:117 RT_DATA_SLOT, runtime/src/audio/audio.cpp:567 ResolveExport, runtime/src/audio/audio.cpp:594 ResolveExport, runtime/src/engine/engine_platform.cpp:151 RT_DATA_SLOT, runtime/src/engine/singleton_ctors.cpp:158 RT_DATA_SLOT, runtime/src/input/input.cpp:458 RT_DATA_SLOT, runtime/src/input/input_config_startup.cpp:129 RT_DATA_SLOT, runtime/src/input/keymap_load.cpp:242 RT_DATA_SLOT, runtime/src/input/keymap_save.cpp:237 RT_DATA_SLOT, runtime/src/io/modification.cpp:918 RT_DATA_SLOT, runtime/src/io/modification_refresh.cpp:925 RT_DATA_SLOT, runtime/src/save/saveload_internal.h:125 RT_DATA_SLOT, runtime/src/video/device.cpp:160 RT_DATA_SLOT
- `?_singleton@ExternalFactory@Object@@0PEAV12@EA`: engine/funcs/engine_funcs.cpp:879 ResolveAddr(Runtime) [addr], runtime/src/engine/loops.cpp:295 RT_DATA_SLOT, runtime/src/engine/loops.cpp:1456 AtomicExport, runtime/src/engine/mainloop_video.cpp:351 RT_DATA_SLOT, runtime/src/engine/singleton_ctors.cpp:155 RT_DATA_SLOT, runtime/src/engine/updateloop_loadresources.cpp:105 RT_DATA_SLOT, runtime/src/io/modification_setupfilesystem.cpp:435 RtResolveCached, runtime/src/object/external_class_doors.cpp:183 RT_DATA_SLOT, runtime/src/object/external_ptr_serialize.cpp:235 RT_DATA_SLOT
- `0x1801fc6d0`: code/Game/Game/game_main.cpp:359 ResolveAddr(Runtime) [addr], runtime/src/engine/engine_lifecycle.cpp:851 ResolveAddr(Runtime) [addr], runtime/src/object/factory_dtors.cpp:528 ResolveAddr(Runtime) [addr]
- `?_singleton@InternalFactory@Object@@0PEAV12@EA`: code/Game/Game/gameupdateloop_serialize.cpp:173 ResolveExport, code/Game/Object/internal_factory_audit.cpp:16 other [addr const-def kInternalFactorySingleton], code/Game/Object/internal_factory_audit.cpp:32 ResolveAddr(Runtime) [via kInternalFactorySingleton], runtime/src/engine/singleton_ctors.cpp:156 RT_DATA_SLOT, runtime/src/object/internal_class.cpp:588 RT_DATA_SLOT
- `?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA`: code/Game/Game/game_main.cpp:484 ResolveAddr(Runtime) [addr], standalone/main.cpp:670 ResolveAddr(Runtime) [addr], standalone/main.cpp:743 ResolveAddr(Runtime) [addr], standalone/rt_data_mint.cpp:514 other, standalone/rt_data_mint.cpp:571 other, standalone/rt_data_mint.cpp:635 other
- `?_singleton@Factory@Object@@0PEAV12@EA`: runtime/src/achievement/goal_list.cpp:201 RT_DATA_SLOT, runtime/src/engine/mainloop_video.cpp:354 RT_DATA_SLOT, runtime/src/engine/singleton_ctors.cpp:154 RT_DATA_SLOT, runtime/src/io/modification_setupfilesystem.cpp:441 RtResolveCached, runtime/src/object/external.cpp:487 RtResolveCached, runtime/src/object/external_load.cpp:342 RT_DATA_SLOT, runtime/src/object/external_savetext_win.cpp:261 RT_DATA_SLOT, runtime/src/object/internal_class.cpp:761 RtResolveCached, runtime/src/object/type_reflection.cpp:301 RT_DATA_SLOT, runtime/src/ui/dialog_create.cpp:344 ResolveExport, standalone/address_registry.cpp:1293 ResolveAddr(Runtime) [addr]
- `?_singleton@SaveLoad@Save@@0PEAV12@EA`: engine/funcs/engine_funcs.cpp:1147 ResolveExport, runtime/src/engine/mainloop_video.cpp:357 RT_DATA_SLOT, runtime/src/save/saveload_internal.h:114 RT_DATA_SLOT, standalone/reg_mint_promoted.cpp:711 other, standalone/reg_mint_promoted.cpp:712 other [addr], standalone/reg_mint_promoted.cpp:713 RegisterRuntimeExport, standalone/reg_mint_promoted.cpp:714 RegisterRuntimeAddr [addr], standalone/reg_unreg_lane1.cpp:173 ResolveExport, standalone/saveload_harness.cpp:100 ResolveExport
- `?_singleton@Configuration@System@@0PEAV12@EA`: runtime/src/engine/engine_lifecycle.cpp:295 RT_DATA_SLOT, runtime/src/engine/updateloop_loadresources.cpp:118 RT_DATA_SLOT, runtime/src/io/filesystem_setupdirs_win.cpp:195 RT_DATA_SLOT, runtime/src/io/filesystem_startup.cpp:84 RT_DATA_SLOT
- `?_exceptionCallback@Debug@System@@0P6A?AW4ErrorResponse@12@PEAXPEADH@ZEA`: NONE
- `?_disableHandler@Debug@System@@0_NA`: runtime/src/video/enumerate_video_modes.cpp:111 RT_DATA_SLOT
- `?_hideDisplay@Error@System@@0_NA`: runtime/src/engine/engine_lifecycle.cpp:302 RT_DATA_SLOT, runtime/src/system/error.cpp:201 RT_DATA_SLOT
- `?_stopApplication@Error@System@@0_NA`: runtime/src/system/error.cpp:195 RT_DATA_SLOT
- `?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA`: code/Game/Component/citizenai_executor.cpp:347 ResolveExport, runtime/src/graphics/wave29_lane_slots.cpp:151 ResolveAddr(Runtime) [addr], standalone/address_registry.cpp:827 ResolveExport, standalone/perf_experimental.cpp:109 ResolveExport
- `?_exceptionHandler@Debug@System@@0PEAXEA`: NONE
- `?_singleton@MemoryAllocator@System@@0PEAV12@EA`: code/Game/Game/game_main.cpp:558 ResolveAddr(Runtime) [addr], engine/funcs/engine_funcs.cpp:869 ResolveAddr(Runtime) [addr], runtime/src/engine/singleton_ctors.cpp:152 RT_DATA_SLOT, runtime/src/system/allocator.cpp:666 RT_DATA_SLOT, runtime/types/engine/Engine.h:148 other [addr const-def AllocatorSingleton], standalone/main.cpp:575 ResolveAddr(Runtime) [addr], standalone/main.cpp:735 ResolveAddr(Runtime) [addr]
- `?_errorString@Error@System@@0PADA`: code/Game/Game/game_main.cpp:465 ResolveAddr(Runtime) [addr], runtime/src/system/error.cpp:192 RT_DATA_SLOT
- `?_ticksPerSecond@Timer@System@@0_JA`: engine/funcs/engine_funcs.cpp:851 ResolveAddr(Runtime) [addr], runtime/src/engine/mainloop_frame.cpp:289 ResolveAddr(Runtime) [addr], runtime/src/system/timer.cpp:188 RT_DATA_SLOT
- `?_secondsPerTick@Timer@System@@0NA`: engine/funcs/engine_funcs.cpp:852 ResolveAddr(Runtime) [addr], runtime/src/audio/audio_play_platform.cpp:266 RT_DATA_SLOT [via SecondsPerTick], runtime/src/audio/audio_play_platform.cpp:267 RT_DATA_SLOT, runtime/src/audio/audio_play_platform.cpp:316 other [via SecondsPerTick], runtime/src/audio/audio_scene_update.cpp:309 RT_DATA_SLOT [via SecondsPerTick], runtime/src/audio/audio_scene_update.cpp:313 RT_DATA_SLOT, runtime/src/audio/audio_scene_update.cpp:466 other [via SecondsPerTick], runtime/src/audio/vtbatch7_soundbuffer_soundeffect.cpp:417 RT_DATA_SLOT, runtime/src/engine/engine.cpp:250 other [via SecondsPerTick], runtime/src/engine/engine.cpp:268 other [via SecondsPerTick], runtime/src/engine/mainloop_frame.cpp:370 ResolveAddr(Runtime) [addr], runtime/src/graphics/animationstate.cpp:149 RT_DATA_SLOT [via SecondsPerTick], runtime/src/graphics/animationstate.cpp:150 RT_DATA_SLOT, runtime/src/graphics/animationstate.cpp:676 other [via SecondsPerTick], runtime/src/graphics/animationstate.cpp:719 other [via SecondsPerTick], runtime/src/graphics/animationstate_transforms.cpp:214 RT_DATA_SLOT [via SecondsPerTick], runtime/src/graphics/animationstate_transforms.cpp:215 RT_DATA_SLOT, runtime/src/graphics/animationstate_transforms.cpp:221 other [via SecondsPerTick], runtime/src/graphics/animationstate_transforms.cpp:928 other [via SecondsPerTick], runtime/src/graphics/animationstate_transforms.cpp:1503 other [via SecondsPerTick], runtime/src/graphics/animationstate_transforms.cpp:1693 other [via SecondsPerTick], runtime/src/graphics/vtbatch7_animgroup_particlesystem.cpp:614 RT_DATA_SLOT [via SecondsPerTick], runtime/src/graphics/vtbatch7_animgroup_particlesystem.cpp:615 RT_DATA_SLOT, runtime/src/graphics/vtbatch7_animgroup_particlesystem.cpp:645 other [via SecondsPerTick], runtime/src/graphics/wave27_lane_gfxbig.cpp:640 other [via SecondsPerTick], runtime/src/graphics/wave27_lane_gfxbig.cpp:642 ResolveAddr(Runtime) [addr], runtime/src/graphics/wave27_lane_gfxbig.cpp:957 other [via SecondsPerTick], runtime/src/graphics/wave27_lane_gfxbig.cpp:1027 other [via SecondsPerTick], runtime/src/graphics/wave27_lane_gfxbig.cpp:1097 other [via SecondsPerTick], runtime/src/graphics/wave27_lane_gfxbig.cpp:1108 other [via SecondsPerTick], runtime/src/graphics/wave29_lane_gfxbody.cpp:254 RT_DATA_SLOT [via SecondsPerTick], runtime/src/graphics/wave29_lane_gfxbody.cpp:255 RT_DATA_SLOT, runtime/src/graphics/wave29_lane_gfxbody.cpp:782 other [via SecondsPerTick], runtime/src/input/input_device_update.cpp:179 RT_DATA_SLOT, runtime/src/save/saveload.cpp:161 other [via SecondsPerTick], runtime/src/save/saveload_internal.h:129 other [via SecondsPerTick], runtime/src/save/saveload_internal.h:137 other [via SecondsPerTick], runtime/src/system/timer.cpp:190 RT_DATA_SLOT, runtime/src/ui/animatedimageelement.cpp:60 AtomicExport [via SecondsPerTick], runtime/src/ui/animatedimageelement.cpp:62 AtomicExport, runtime/src/ui/animatedimageelement.cpp:130 other [via SecondsPerTick], runtime/src/ui/dialog.cpp:1873 RT_DATA_SLOT, runtime/src/ui/documenttextelement.cpp:126 AtomicExport [via SecondsPerTick], runtime/src/ui/documenttextelement.cpp:128 AtomicExport, runtime/src/ui/documenttextelement.cpp:193 other [via SecondsPerTick], runtime/src/ui/editelement.cpp:95 AtomicExport [via SecondsPerTick], runtime/src/ui/editelement.cpp:97 AtomicExport, runtime/src/ui/editelement.cpp:377 other [via SecondsPerTick], runtime/src/ui/userinterface.cpp:291 AtomicExport, runtime/src/ui/userinterface_close.cpp:174 AtomicExport, runtime/src/ui/userinterface_frame.cpp:177 AtomicExport, runtime/src/ui/userinterface_hover.cpp:330 AtomicExport, runtime/src/ui/userinterface_showdialog.cpp:215 other, runtime/src/ui/userinterface_showdialog.cpp:381 other [via SecondsPerTick], runtime/src/ui/userinterface_standalone.cpp:148 AtomicExport, runtime/types/engine/MainLoop.h:194 other [addr const-def SecondsPerTick]
- `?_systemTicksPerSecond@Timer@System@@0_JA`: engine/funcs/engine_funcs.cpp:853 ResolveAddr(Runtime) [addr], runtime/src/engine/mainloop_frame.cpp:291 ResolveAddr(Runtime) [addr], runtime/src/system/timer.cpp:192 RT_DATA_SLOT
- `?_systemInitialTick@Timer@System@@0_JA`: engine/funcs/engine_funcs.cpp:854 ResolveAddr(Runtime) [addr], runtime/src/engine/mainloop_frame.cpp:287 ResolveAddr(Runtime) [addr], runtime/src/system/timer.cpp:194 RT_DATA_SLOT
- `?_systemSecondsPerTick@Timer@System@@0NA`: runtime/src/system/timer.cpp:196 RT_DATA_SLOT
- `?_singleton@Engine@1@0PEAV11@EA`: runtime/src/engine/engine_lifecycle.cpp:292 RT_DATA_SLOT, runtime/src/engine/engine_platform.cpp:154 RT_DATA_SLOT, runtime/src/engine/loops.cpp:1454 AtomicExport, runtime/types/engine/Engine.h:147 other [addr const-def Singleton]
- `?_singleton@MainLoop@Engine@@1PEAV12@EA`: engine/funcs/engine_funcs.cpp:2080 ResolveExport, runtime/src/engine/engine.cpp:232 RT_DATA_SLOT, runtime/src/engine/loops.cpp:320 RT_DATA_SLOT, runtime/src/engine/mainloop_video.cpp:360 RT_DATA_SLOT, runtime/src/save/saveload_update.cpp:1144 other, runtime/src/ui/userinterface_showdialog.cpp:214 other, runtime/types/engine/MainLoop.h:136 other [addr const-def Singleton], standalone/modal_driver.cpp:117 other [addr const-def kMainLoop_Singleton], standalone/modal_driver.cpp:163 ResolveAddr(Runtime) [via kMainLoop_Singleton], standalone/reg_unreg_lane1.cpp:189 ResolveExport
- `?_singleton@Configuration@Audio@@0PEAV12@EA`: engine/funcs/engine_funcs.cpp:1361 ResolveAddr(Runtime) [addr], runtime/src/audio/audio.cpp:1241 RT_DATA_SLOT, runtime/src/audio/audio_scene_update.cpp:305 RT_DATA_SLOT, runtime/src/engine/engine_platform.cpp:145 RT_DATA_SLOT, runtime/src/engine/singleton_ctors.cpp:165 RT_DATA_SLOT
- `?_singleton@Device@Audio@@0PEAV12@EA`: runtime/src/audio/audio.cpp:802 RT_DATA_SLOT, runtime/src/audio/audio_scene_update.cpp:300 RT_DATA_SLOT, runtime/src/audio/batchc_audio_leaves.cpp:63 RT_DATA_SLOT, runtime/src/audio/vtbatch7b_stream_run_win.cpp:262 RT_DATA_SLOT, runtime/src/engine/singleton_ctors.cpp:164 RT_DATA_SLOT
- `?_singleton@Configuration@Input@@0PEAV12@EA`: engine/funcs/engine_funcs.cpp:2063 ResolveExport, runtime/src/engine/engine_platform.cpp:148 RT_DATA_SLOT, runtime/src/engine/singleton_ctors.cpp:163 RT_DATA_SLOT, runtime/src/input/input.cpp:571 RT_DATA_SLOT, runtime/src/input/input_device_update.cpp:176 RT_DATA_SLOT, runtime/src/input/input_platform_startup_win.cpp:108 RT_DATA_SLOT, runtime/src/ui/editelement.cpp:93 AtomicExport, runtime/src/ui/userinterface.cpp:303 AtomicExport, runtime/src/ui/userinterface_draw.cpp:179 AtomicExport, runtime/src/ui/userinterface_frame.cpp:189 AtomicExport
- `?_singleton@Device@Input@@0PEAV12@EA`: engine/funcs/engine_funcs.cpp:1166 ResolveExport, runtime/src/engine/singleton_ctors.cpp:162 RT_DATA_SLOT, runtime/src/input/input.cpp:101 RT_DATA_SLOT, runtime/src/input/input_platform_startup_win.cpp:166 RT_DATA_SLOT, runtime/src/ui/combodialog.cpp:213 RT_DATA_SLOT, runtime/src/ui/editelement.cpp:89 AtomicExport, runtime/src/ui/element_bodies_b5.cpp:203 AtomicExport, runtime/src/ui/userinterface_draw.cpp:175 AtomicExport, runtime/src/ui/userinterface_frame.cpp:185 AtomicExport, standalone/mouse_inject.cpp:350 ResolveExport
- `?_frameIndex@ParticleInstance@Graphics@@1HA`: code/Game/Component/particlecomponent.cpp:575 ResolveAddr(Runtime) [addr], runtime/src/graphics/animation.cpp:173 other [const-def kFrameIndexData], runtime/src/graphics/animation.cpp:217 ResolveExport [via kFrameIndexData], runtime/src/graphics/wave28_lane_partsys.cpp:344 ResolveExport, standalone/reg_unreg_lane3.cpp:215 ResolveExport
- `?_firstSurface@ImageBuffer@Video@@0PEAV12@EA`: runtime/src/video/buffers.cpp:112 ResolveExport, runtime/src/video/dynamic_resources.cpp:160 RT_DATA_SLOT
- `?_firstBuffer@IndexBuffer@Video@@0PEAV12@EA`: runtime/src/video/buffers.cpp:117 ResolveExport, runtime/src/video/dynamic_resources.cpp:165 RT_DATA_SLOT
- `?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA`: runtime/src/video/buffers.cpp:131 ResolveExport, runtime/src/video/dynamic_resources.cpp:175 RT_DATA_SLOT
- `?_firstBuffer@VertexBuffer@Video@@0PEAV12@EA`: runtime/src/video/buffers.cpp:122 ResolveExport, runtime/src/video/dynamic_resources.cpp:170 RT_DATA_SLOT
- `?_singleton@Configuration@Video@@0PEAV12@EA`: dx11/src/contextdata.cpp:245 other, dx11/src/devicedata.cpp:283 other, dx11/src/imagebufferdata.cpp:217 ResolveExport, engine/funcs/engine_funcs.cpp:1383 ResolveAddr(Runtime) [addr], runtime/src/engine/engine_platform.cpp:142 RT_DATA_SLOT, runtime/src/engine/singleton_ctors.cpp:160 RT_DATA_SLOT, runtime/src/graphics/imagebuffer_derived.cpp:803 RT_DATA_SLOT, runtime/src/graphics/lights_vtable.cpp:297 RT_DATA_SLOT, runtime/src/graphics/sun_camera.cpp:125 RT_DATA_SLOT, runtime/src/save/saveload_draw.cpp:353 RT_DATA_SLOT, runtime/src/video/buffers.cpp:93 RT_DATA_SLOT, runtime/src/video/device.cpp:155 RT_DATA_SLOT, runtime/src/video/device_adjustdisplay_win.cpp:97 RT_DATA_SLOT, runtime/src/video/device_startup.cpp:154 RT_DATA_SLOT, runtime/src/video/device_startup_win.cpp:46 RT_DATA_SLOT, runtime/src/video/video_resources.cpp:140 RT_DATA_SLOT, standalone/main.cpp:833 ResolveAddr(Runtime) [addr]
- `?_singleton@Device@Video@@0PEAV12@EA`: dx11/src/buffers.cpp:171 ResolveAddr(Runtime) [addr], dx11/src/buffers.cpp:209 other [via VideoDeviceSingleton], dx11/src/buffers.cpp:216 other [via VideoDeviceSingleton], dx11/src/devicedata.cpp:284 other, dx11/src/dx11_backend_access.h:90 other [addr const-def kRt_VideoDeviceSingleton], dx11/src/dx11_backend_access.h:98 ResolveAddr(Runtime) [via kRt_VideoDeviceSingleton], dx11/src/imagebufferdata.cpp:216 ResolveExport, dx11/src/renderstatedata.cpp:329 ResolveAddr(Runtime) [addr], engine/funcs/engine_funcs.cpp:875 ResolveAddr(Runtime) [addr], runtime/src/engine/engine.cpp:241 RT_DATA_SLOT, runtime/src/engine/engine_platform.cpp:139 RT_DATA_SLOT, runtime/src/engine/singleton_ctors.cpp:159 RT_DATA_SLOT, runtime/src/graphics/batchc_gfx_leaves.cpp:728 RT_DATA_SLOT, runtime/src/graphics/graphicsmesh_vtable.cpp:229 RT_DATA_SLOT, runtime/src/graphics/lights_vtable.cpp:293 RT_DATA_SLOT, runtime/src/graphics/meshinstance_draw.cpp:171 RT_DATA_SLOT, runtime/src/graphics/renderer.cpp:257 ResolveAddr(Runtime) [addr], runtime/src/graphics/sun_camera.cpp:124 RT_DATA_SLOT, runtime/src/graphics/sunlight.cpp:187 RT_DATA_SLOT, runtime/src/save/saveload_draw.cpp:348 RT_DATA_SLOT, runtime/src/save/saveload_saveinfo.cpp:105 AtomicAddr [addr], runtime/src/save/saveload_update.cpp:113 RT_DATA_SLOT, runtime/src/ui/combodialog_ctor.cpp:343 RT_DATA_SLOT, runtime/src/ui/containerelement.cpp:104 AtomicExport, runtime/src/ui/dialog.cpp:357 RT_DATA_SLOT [via VideoDeviceSingleton], runtime/src/ui/dialog.cpp:358 RT_DATA_SLOT, runtime/src/ui/dialog.cpp:1821 other [via VideoDeviceSingleton], runtime/src/ui/dialog.cpp:1966 other [via VideoDeviceSingleton], runtime/src/ui/element.cpp:290 AtomicExport [via VideoDeviceSingleton], runtime/src/ui/element.cpp:292 AtomicExport, runtime/src/ui/element.cpp:640 other [via VideoDeviceSingleton], runtime/src/ui/element_bodies_b5.cpp:196 AtomicExport [via VideoDeviceSingleton], runtime/src/ui/element_bodies_b5.cpp:198 AtomicExport, runtime/src/ui/element_bodies_b5.cpp:364 other [via VideoDeviceSingleton], runtime/src/ui/element_common.h:196 AtomicExport [via VideoDeviceSingleton], runtime/src/ui/element_common.h:198 AtomicExport, runtime/src/ui/element_common.h:274 other [via VideoDeviceSingleton], runtime/src/ui/spritecontext_flush.cpp:318 ResolveAddr(Runtime) [addr], runtime/src/ui/userinterface.cpp:293 AtomicExport [via VideoDeviceSingleton], runtime/src/ui/userinterface.cpp:295 AtomicExport, runtime/src/ui/userinterface.cpp:769 other [via VideoDeviceSingleton], runtime/src/ui/userinterface_draw.cpp:169 AtomicExport [via VideoDeviceSingleton], runtime/src/ui/userinterface_draw.cpp:171 AtomicExport, runtime/src/ui/userinterface_draw.cpp:260 other [via VideoDeviceSingleton], runtime/src/ui/userinterface_draw.cpp:658 other [via VideoDeviceSingleton], runtime/src/ui/userinterface_frame.cpp:179 AtomicExport [via VideoDeviceSingleton], runtime/src/ui/userinterface_frame.cpp:181 AtomicExport, runtime/src/ui/userinterface_frame.cpp:391 other [via VideoDeviceSingleton], runtime/src/ui/widget_common.h:298 RtResolveCached, runtime/src/video/buffers.cpp:88 RT_DATA_SLOT, runtime/src/video/context_ctor.cpp:195 RT_DATA_SLOT, runtime/src/video/device.cpp:150 RT_DATA_SLOT, runtime/src/video/device_startup_win.cpp:42 RT_DATA_SLOT, runtime/src/video/dynamic_resources.cpp:150 RT_DATA_SLOT, runtime/src/video/pixelprogram.cpp:91 RT_DATA_SLOT, runtime/src/video/renderstate.cpp:135 RT_DATA_SLOT, runtime/src/video/shaderprogram.cpp:163 RT_DATA_SLOT, runtime/src/video/vertexlayout.cpp:105 RT_DATA_SLOT, runtime/src/video/vertexprogram.cpp:133 RT_DATA_SLOT, runtime/src/video/video_resources.cpp:143 RT_DATA_SLOT, runtime/src/video/videoprogram.cpp:82 RT_DATA_SLOT, runtime/types/engine/MainLoop.h:189 other [addr const-def VideoDeviceSingleton], standalone/address_registry.cpp:1152 ResolveExport, standalone/main.cpp:831 ResolveAddr(Runtime) [addr], standalone/main.cpp:1158 ResolveAddr(Runtime) [addr]
- `?_singleton@AnimationManager@Graphics@@0PEAV12@EA`: runtime/src/graphics/animation.cpp:207 ResolveAddr(Runtime) [addr], runtime/src/graphics/animationstate.cpp:142 RT_DATA_SLOT, runtime/src/graphics/animationstate_transforms.cpp:207 RT_DATA_SLOT, runtime/src/graphics/mesh.cpp:353 RT_DATA_SLOT, runtime/src/graphics/meshinstance_draw.cpp:175 RT_DATA_SLOT, runtime/src/graphics/wave27_lane_gfxbig.cpp:651 ResolveAddr(Runtime) [addr], runtime/src/graphics/wave28_lane_gfxbody.cpp:661 RT_DATA_SLOT
- `?_loader@LoaderLoop@Engine@@1VLoader@2@A`: engine/funcs/engine_funcs.cpp:1973 ResolveAddr(Runtime) [addr], engine/funcs/engine_funcs.cpp:1974 ResolveAddr(Runtime) [addr+0x8], engine/funcs/engine_funcs.cpp:1975 ResolveAddr(Runtime) [addr+0x28], runtime/src/engine/loops.cpp:1118 AtomicExport, standalone/rt_data_mint.cpp:516 other, standalone/rt_data_mint.cpp:694 RegisterRuntimeAddr, standalone/rt_data_mint.cpp:789 other
- `?_rng@Particle@Graphics@@1VRandom@System@@A`: code/Game/Game/tornado.cpp:344 ResolveExport, runtime/src/graphics/wave27_lane_gfxpart.cpp:256 ResolveExport, runtime/src/graphics/wave28_lane_partsys.cpp:255 ResolveExport, standalone/rt_data_mint.cpp:518 other
- `0x1801ff30c`: runtime/src/graphics/wave29_lane_slots.cpp:135 ResolveAddr(Runtime) [addr]
- `0x1801ff310`: runtime/src/graphics/wave29_lane_slots.cpp:138 ResolveAddr(Runtime) [addr]

### A.4 Special cases

| case | rows | rule |
|---|---|---|
| **Timer's five together** | `_ticksPerSecond` 0x1801fe720, `_secondsPerTick` 0x1801fe728, `_systemTicksPerSecond` 0x1801fe730, `_systemInitialTick` 0x1801fe738, `_systemSecondsPerTick` 0x1801fe740 | timer.cpp already owns the values in its own globals and COPIES them into the cells (`PublishTimerStatics` timer.cpp:187-197); two rows are also seeded by the mint's `EXTRA_INIT` (rt_mint_gen.py:324-329). One commit: the five become `System::Timer` statics initialised in timer.cpp (1000000 / 1e-6 as initialisers), `PublishTimerStatics`, `EXTRA_INIT` and the five rows go; the 56+3+3+3+1 sites and `g_Timer_*` (17+11+2+2 use lines, engine_funcs.cpp:851-854) convert. `_secondsPerTick` is DIVIDED BY (hunt/livestock/spawn components): a zero there is inf/NaN with no crash — the gate must include a run that places a hunter. |
| **`Particle::_rng` double seed** | 0x1801fe7f0 (2500 B) | B4b-2 verbatim. The mint seeds it once (`ConstructParticleRng`, rt_data_mint.cpp:495-503). The C++ static must be the ONLY seeder: delete `ConstructParticleRng`, its `KNOWN_CTORS` entry (rt_mint_gen.py:419-421) and the row in the SAME commit that defines `static System::Random Particle::_rng`; confirm `System::Random()` (Value.h:195) reproduces initialiser 0x180149650 (time-seeded). Readers: tornado.cpp:344 (`f.rng`), two `ParticleRng()` in runtime/src/graphics. |
| **`_loader` object** | 0x1801fe7b8 (0x38) | needs a real `Engine::Loader()` (`: System::Thread(0x40000)`, `_loop(nullptr)`) and a real `System::Thread(unsigned)` first; today door-built (rt_data_mint.cpp:434-469). Interior readers 0x1801fe7c0 (+0x08 runFlags) and 0x1801fe7e0 (+0x28 hFinished) in engine_funcs.cpp:1974-1975 become member reads. `??_7Loader@Engine@@` (EXTRA_VTABLES) and its `KNOWN_CTORS` entry go with it. The object is the LOADER THREAD itself. Construction moves from mint time (inside `RuntimePortInit`) to CRT init — a Thread ctor that creates OS events at static init: measure, it is a boot-order change. |
| **`_firstTypeInfo`** | 0x1801fc6e0 | the one redirected row. Stays until the reflection fold (D10: `_typeInfo` rows then `Object::Base` last); the 3 by-address readers (game_main.cpp:484, standalone/main.cpp:670, :743) become `Object::BaseTypeInfo::_firstTypeInfo`. |
| **`MemoryAllocator::_singleton`** | 0x1801fc718 | 7 sites, but `g_pMemAllocSingleton` has 479 use lines (MemAlloc door fan-out) — carry-over 1, deferred on purpose. Stays. |
| **Loader-thread readers (B6-5)** | ExternalFactory (updateloop_loadresources.cpp:105, loops.cpp:295, the 183 `g_pExternalFactorySingleton` lines, many on world build), FileSystem (:113), Modification (:109, loops.cpp:303), System::Configuration (:118), InternalFactory + AchievementManager (gameupdateloop_serialize.cpp:173/175, save load), `_loader` | a class static is initialised before `main` — no lazy init, no race; the conversion REMOVES the per-site lazy `RT_DATA_SLOT`/`if (!s_x) s_x = …` caches (every one of the 166 `RT_DATA_SLOT` sites is one). Loader-thread reach is inferred from the file, not traced (§7). |
| **dx11 across the "module boundary"** | Video::Device (dx11 8 sites), Video::Configuration (3), Application (1) | dx11 is compiled INTO reimpl-x64.exe (Makefile:40-42); the boundary is nominal. `devicedata.cpp:282-284`'s import table (`g_im.appSingleton` / `configSingleton` / `deviceSingleton`, carry-over 6) and `imagebufferdata.cpp:216-217`, `contextdata.cpp:245` read by NAME; `buffers.cpp:171`, `renderstatedata.cpp:329`, `dx11_backend_access.h:90/98` by ADDRESS. They become `::Video::Device::_singleton` etc. directly (dx11 already names `::Video::Device`, buffers.cpp:216). No retail IAT is read (dx11_backend_access.cpp:9-21). |
| **exe import slots used as data exrefs** | 0x1401514f0 AchievementManager, 0x140152368 Application, 0x140151d18 FileSystem, 0x140152760 Factory, 0x140151610 SaveLoad, 0x140151f30 `_rootOnlyUpdates`, 0x140151f38 `_frameIndex`, 0x140151a80 Video::Device, 0x1401519e8 Modification, 0x140151440 MainLoop (no reader) | registered in `reg_unreg_lane1.cpp:173/182/189/205`, `reg_unreg_lane3.cpp:215`, `address_registry.cpp:827/1152/1285/1293`; read by `ResolveAddr(Module::Exe, …)` in engine_funcs.cpp:1137/1177/1191/1592/1980/2051/2160/2318, gameupdateloop.cpp:440-441, nomadcomponent.cpp:101, citizenai_executor.cpp:343, perf_experimental.cpp:105. Each import-slot row goes when its readers read the class static; `dispatch_slot_map.py --check` must stay 0 (a deleted registration is fine, a mismatched one is not). |
| **`g_pInputConfigSingletonExref` is misnamed** | engine_funcs.cpp:1062, IAT 0x140151738, 45 use lines | it is the KeyMap `ExternalPtr` exref (address_registry.cpp:23-32, :1347-1355), not Input::Configuration's singleton — not a storage row; exe-side. |
| **no reader at all** | `_crcTable`, `_keyMap`, `_lkeyMap`, `_exceptionCallback`, `_exceptionHandler` | MINT-ONLY: minted because the all-exported-data union / census scan once saw them; drop via the RETIRED list (§4 C.2). `_disableHandler` has ONE reader (enumerate_video_modes.cpp) and no class to own it (System::Debug unported) — stays (B34B §4.1). |

### A.5 Verdict summary

| verdict | rows |
|---|---|
| **class static now** (all readers converted in the owning block; no redirect needed if the row retires in the same commit) | 39: every `?_singleton@` except MemoryAllocator (19), Timer ×5 (one commit), Error ×3, the 4 Video list heads, `_frameIndex`, `_rootOnlyUpdates`, the 4 unexported GraphicsMesh/MeshInstance cells, 0x1801fc6d0 (Factory HMODULE), `_rng` (with the double-seed rule) |
| **needs X first** | `_loader` (real Loader/Thread ctors), `?_singleton@MemoryAllocator@` (carry-over 1), `_firstTypeInfo` (the reflection fold, D10 last) |
| **stays** | `_disableHandler` (System::Debug unported, 1 reader) |
| **drop, no reader** | `_crcTable`, `_keyMap`, `_lkeyMap`, `_exceptionCallback`, `_exceptionHandler` (5) |
| **manifest vtable/_typeInfo rows (22)** | retire with their mint rows (0A/0B); `??_7Dialog@UI` stays WITHHELD until then |

---

## 3. Question B — what stays after the step

### B.1 `standalone/rt_export_addrs.inc` (4,926 rows)

It is generated from the DLL's export directory alone (`write_export_addrs`, rt_mint_gen.py:607-630) — the
tree never changes it — and its ONE consumer is the name↔address mirror in `resolve_standalone.cpp:211-261`.
Retiring mint rows does not shrink it. What matters is which rows still have a reader:

| state | rows | by mangled kind |
|---|---:|---|
| LIVE (≥1 non-mint reader, by name or address) | **1,045** | member fn 349, dtor 173, ctor 158, vtable 143, `_typeInfo` 103, storage 36, static member fn 31, virtual member fn 26, operator new/delete 10, other operator 10, operator= 6 |
| MINT-ONLY (only the mint's slot / factory tables name it) | **1,023** | virtual member fn 804, factory static 202, member fn 7, storage 5, vtable 4, `_typeInfo` 1 — these die with the mint rows |
| REG-ONLY (registered, nobody asks) | **671** | member fn 447, operator new/delete 96, ctor 67, static member fn 44, dtor 9, other 8 |
| NONE (not mentioned at all) | **2,187** | member fn 968, static member fn 373, ctor 292, operator= 228, new/delete 203, other operator 104, dtor 17, free 2 |

LIVE rows by where their readers are: runtime only 650; runtime+standalone 117; standalone only 96; engine
only 37; code/Game only 24; dx11 only 20; mixed rest 101. **395 LIVE rows have a reader outside `runtime/`**
(member fn 110, ctor 88, `_typeInfo` 49, vtable 41, storage 36, dtor 29, virtual 16, static 13, other 13) —
that is the registry-teardown population after B.3; the 650 runtime-only rows are the Runtime's own by-name
doors (per namespace). Rows whose mirror is actually exercised (read on one door, registered only on the
other): ≤ 63 (heuristic; a registration through a named constant, e.g. widgets_input.cpp:1093-1156's
`kSpinner*`, reads as a READ of the name, so this over-counts). The exact "categories of the live ones" the
brief asks (ctor/dtor doors, one-line qualified delegations …) are by mangled kind above; "one-line
qualified delegation" is not decidable from the row — not measured.

### B.2 `RegisterExeAddr` (1,007 statements) and the import-slot registrations

| category (by address) | rows | with a by-address reader | reader-less |
|---|---:|---:|---:|
| exe function bodies (< 0x140151000) | 847 | 616 | **231** |
| import slots (0x140151000-0x140152fff) via RegisterExeAddr | 2 | 1 | 1 |
| exe vtables (.rdata 0x140153000-0x14016ffff) | 6 | 6 | 0 |
| type-id cells (0x1401c0000-0x1401dffff) | 100 | 95 | 5 |
| other `.data` (≥ 0x140170000, e.g. `g_fUVScale` 0x14017214c) | 42 | 42 | 0 |
| through an expression (table loops) | 10 | not resolvable | — |
| **total** | **1,007** | | **237** |
| `Register*Slot(0x14015…)` import-slot rows (RegisterDispatchSlot 128, Lane3 22, Lane2 6, Goal 3, PlacePlayer 1) | 160 | 75 | **85** |

After step 9: the 6 vtable rows go (acceptance), the 100 type-id rows go with `g_tid_*` (D10 c), the 5
reader-less type-id rows at once. What STAYS for the post-B.3 registry teardown: **~616 function rows with a
by-address reader** (exe-phase bodies reached by `ResolveAddr(Module::Exe, …)` from exe-phase code), **42
other `.data` rows**, the live import-slot rows (75) — ~730 rows, plus the 10 table-loop registrations.

### B.3 `engine/funcs/engine_funcs.{h,cpp}` (414 `extern` globals)

| category | globals | code use lines | zero-use |
|---|---:|---:|---:|
| exe code doors (`ResolveAddr(Exe, <0x140151000)`; incl. a few `.data` like `g_fUVScale`) | 171 | 700 | 10 |
| type-id globals (`g_TypeId_*`) | 111 | 2,492 | 7 |
| ctor/dtor doors | 54 | 818 (`fn_InternalBasePtr_Ctor` 403, `fn_Time_Ctor_Float` 201) | 6 |
| storage globals | 23 | 566 (`g_pExternalFactorySingleton` 183, `g_pVideoDeviceSingleton` 54, `g_pFactorySingleton` 38, `g_pMainLoopSingleton` 38 …) | 0 |
| Runtime doors, other | 16 | 1,191 (`fn_IsDerivedFrom` 1,085) | 7 |
| exe vtable globals | 9 | 36 | 0 |
| ExternalPtr exe doors | 8 | 150 (`fn_ExternalPtr_BuildSerialize` 86) | 0 |
| MemoryAllocator doors | 5 | 1,657 (`fn_MemFreeStatic` 598, `fn_MemAlloc` 534, `g_pMemAllocSingleton` 479) | 0 |
| by-value `String` return doors | 4 | 8 (`fn_Configuration_{GetVersionString,CurrentBuildId,CurrentVersion}`, `fn_DateTime_AsString`) | 0 |
| exe import-slot globals (`g_ppAchievementManager`, `g_ppTypeInfo_{MaterialInstance,GraphicsMesh,Particle}`) | 4 | 46 | 0 |
| uninitialised / points at our own fn | 9 | 5 | 4 |
| **total** | **414** | | **34** |

The 34 zero-use globals (each also has a typedef, a definition, an initialiser and usually an
EngineFuncsDump `printf` line to delete): `fn_GetInputKeyName fn_CitizenList_AssignWorker
fn_Building_RemoveBuyOrder fn_Game_SpawnResources fn_Game_GetMonthFromTime fn_BuildingList_CheckCondition
fn_CitizenBehavior_{CountResources,CheckThresholds,CollectStats} fn_Struct60B_Resize` ·
`g_TypeId_{DialogController,PageToggleUI,StorageComponent,StateMachineComponent,ModelComponent,
PickingComponent,ProfessionDescription}` · `fn_DialogInstance_Dtor fn_ElementDescription_{Dtor,Ctor}
fn_Element_Ctor fn_Goal_BaseCtor fn_Graphics_Geometry_Ctor` · `fn_PostDrawPush fn_PostUpdatePush
fn_ListCleanup fn_Map_Find fn_Map_Insert fn_ParseCommand fn_ParseValue` ·
`fn_TownHallUI_{UpdateProduction,UpdateAgriculture,UpdateInventory,UpdateAll}`. All safe to delete now
(no reader by binding; the dump `printf((void*)X)` is not a caller — B38 §11.5).

What STAYS after step 9: the MemoryAllocator doors (carry-over 1), the ExternalPtr doors (carry-over 3), the
161 live exe code doors, `fn_IsDerivedFrom`/`fn_IsA`/`fn_InternalBasePtr_*`, the ctor doors of by-value
Runtime classes without real ctors (`fn_Time_Ctor_Float`, `fn_PerlinNoise_Ctor` …), the 4 String-return doors
(carry-over 2 keeps `CurrentBuildId`). What GOES in step 9: the 9 exe vtable globals (loop builders +
thumbnails), the 111 `g_TypeId_*` (→ `X::TypeId()`), the 23 storage globals (→ class statics), the 34
zero-use.

### B.4 The 237 reader-less rows and the dead template

- **237 reader-less `RegisterExeAddr` rows: zero readers confirmed** by a whole-tree literal + named-constant
  sweep (`regexe.py`, the b38-close `rows.py` method). The exe registry is exact-match
  (`resolve_standalone.cpp:408-431`) and is otherwise read only for its SIZE (`GetRegistryStats`,
  address_registry.cpp:1614, main.cpp:564/1568, resolve_standalone.cpp:498), so deleting them changes nothing
  but the `[Resolve-Standalone] Address registry: N entries` boot line. **Safe to delete**, but they are not
  step-9 acceptance (none names a converted class): do it as ONE mechanical exe-side commit. Caveat: 38 of
  them register a body with no other use (`deadrows.py`, heuristic: ≤3 mentions of the name) — e.g. the 8
  `gameupdateloop_serialize.cpp:1030-1037` serialize helpers, 7 `reg_citizenai_housing_idle.cpp:51-57`
  helpers; deleting the row orphans the body (a `static` one warns — the new-warning gate) — delete the body
  too, or leave those 38 for the registry teardown. Largest files: reg_immigration.cpp 29, reg_destroy_sm.cpp
  17, reg_boat_ctrl.cpp 14, reg_trade_ctrl.cpp 13, reg_component_cropdisease.cpp 12 (full list in
  `regexe.tsv` / `deadrows.tsv`).
- **85 reader-less import-slot registrations**: same argument; `dispatch_slot_map.py --check` counts
  mismatches, not coverage — deleting a registration cannot create a mismatch.
- **`ControllerLive::Dispatcher<Tag>`**: only `vtbl[16]` (controller_state_table.h:149, 291), `Vtable()`
  (:274-288) and `Stub` (:272) have no reader; `Dispatcher<Tag>::table` and the step functions are live
  (26 `…Dispatch::table` uses; `TableController` :352-371). Delete the three members only.

---

## 4. Question C — file ownership for the retirement blocks

### C.1 Namespace blocks — owned files and their stake in the shared files

| block | owns (exclusive) | mint vt / ti / storage rows | `RT_DELDTOR` rows | manifest rows | tests |
|---|---|---|---|---|---|
| Object | runtime/src/object (41), runtime/types/object (4) | 7 / 8 / 4 (+0x1801fc6d0) | 7 | 10 (4 vt, 2 ti, 4 storage) | object_{external,internal,ptr,typeinfo,vtable_abi}_test.cpp |
| System | runtime/src/system (21), types/system (11), runtime/src/debug | 1 / 0 / 14 | 0 | 11 storage (Timer 5, Error 3, Config, MemAlloc, `_disableHandler`) | system_abi_test.cpp, string/filename/alloc_door tests |
| IO | runtime/src/io (49), types/io (8) | 15 / 1 / 3 | 8 | 5 (2 vt, 3 storage) | io_{streams,vtable}_abi_test.cpp |
| Collection | runtime/src/collection (20), types/collection | 0 / 0 / 0 | 0 | 0 | array/list/set tests |
| Video + Graphics | runtime/src/{video,graphics} (37+37), types (10+17) | 39 / 18 / 10 (+4 by-address) | 39 | 22 (12 `*Data` vt, 10 storage) | video_*, graphics_*, rb_{video,graphics} tests |
| UI | runtime/src/ui (109), types/ui (73) | 68 / 68 / 0 | 67 | 2 (Dialog WITHHELD, DialogController) | ui_* tests (13 files), rb_ui |
| Engine/Save/Achievement/Audio/Input/Pathfinding/Picking (+Application, Collision2D) | runtime/src/{engine 16, save 11, achievement 13, audio 17, input 10, pathfinding 4, picking 9, application 9, collision2d 5} | 17 / 9 / 12 | 14 | 9 storage | engine_loop, save, achievement_goal, audio, input, pathfinding_picking, application, rb_{engine,achievement} tests |
| exe-side | code/Game (loop builders, `g_tid_*`, ImageBuffer/SpriteSheet/MaterialInstance copies), engine/funcs readers, standalone/address_registry.cpp + reg_*.cpp, standalone/main.cpp readers | — | — | — | exe_class_abi, act/wrk/… |

(Mint namespace counts by the mangled outermost scope, `storage.py`/inline script; `(by-address)` rows: 5
vtables, 5 storage.)

### C.2 Shared files and the smallest hand-in mechanism

| shared file | who touches it | proposal |
|---|---|---|
| `standalone/rt_data_mint_tables.inc` | every block | NEVER edited; regenerated by the coordinator at merge. Blocks retire rows through **`tools/rt_mint_retired.txt`** (new): one line per retired row `<mangled or 0xVA>  <block>  <commit>`, sorted, grouped under `# <namespace>` headers so parallel hunks do not collide. `rt_mint_gen.py` subtracts the list AFTER every union and closure (after :823, before :825) and REFUSES if a retired symbol is still (a) a source literal (`scan_source_for_data_symbols`, :536) or (b) a census consumer, or if a retired `??_7X` still drags a non-retired `?_typeInfo@X` through the closure mismatch — a retirement with a live reader is a generator error, not a boot fault. A per-row CSV column cannot work: the CSV is not the population (finding 3). |
| `tools/rt_mint_gen.py` | coordinator only | the one-time change above; `KNOWN_CTORS` / `EXTRA_INIT` / `EXTRA_STORAGE_BY_ADDR` / `EXTRA_VTABLES` entries are deleted by the coordinator in the hunk that retires their row (Timer: EXTRA_INIT ×2; `_loader`: KNOWN_CTORS + EXTRA_VTABLES; `_rng`: KNOWN_CTORS; 4 by-address rows) |
| `standalone/rt_data_mint.cpp` | coordinator | `MintStorageCell`, `ConstructLoader`, `ConstructParticleRng`, the `_loader` interior doors, the `_firstTypeInfo` splice — each deleted with its row. If any block needs a transitional redirect, first route `RegisterMintedData`, `MintStorage` and `MintConstructedStatics` through `MintStorageCell` (finding 1) — one coordinator commit before the storage blocks start |
| `standalone/cutover_manifest.inc` + `tools/cutover_manifest.py` + `standalone/reg_mint_promoted.cpp` (TAG_FILE) | coordinator | regenerated (`--inc`) at merge; a REQUIRED row disappears when the census no longer sees a literal door. The gate's "REQUIRED row not minted" check is the interlock that catches a retired row with a surviving door. When the manifest is empty, `reg_mint_promoted.cpp` (dead code, finding 4) and the manifest machinery go in one coordinator commit |
| `runtime/src/object/deleting_dtor.cpp` (135 rows) | Object block owns the file; others hand in | blocks list the rows (by deleting-dtor VA) their compiler `DeletingDtor` supersedes in their report; the Object-block owner (or the coordinator) applies them in one hunk per namespace. The table is address-sorted, so namespaces interleave — do NOT let blocks edit it in parallel |
| `standalone/rt_export_addrs.inc` | nobody | unchanged by step 9 (DLL-derived); goes with the registry teardown |
| `engine/funcs/engine_funcs.{h,cpp}` | coordinator | blocks hand in a list "global X: 0 uses after my commits" (by binding, dump `printf` excluded); coordinator deletes typedef + extern + definition + initialiser + dump line. Storage globals (23) and `g_TypeId_*` (111) are the bulk |
| `standalone/address_registry.cpp`, `standalone/reg_*.cpp` | exe-side block | namespace blocks never touch them; the import-slot rows of a converted singleton (reg_unreg_lane1/3, address_registry.cpp:827/1152/1285/1293) are handed to the exe-side block as a list |
| `tests/unit/sources.mk`, Makefile | coordinator | new test files listed in the block's report |
| `runtime/funcs/rt_port.h` (`RT_DATA_SLOT`) | coordinator, last | the macro goes when its last use does |

---

## 5. Retirement consequences

**Delete at once (no conversion needed, no reader by binding):** the 34 zero-use engine_funcs globals;
`Dispatcher<Tag>::{vtbl, Vtable, Stub}`; the 5 reader-less type-id `RegisterExeAddr` rows; the 5 no-reader
storage rows (`_crcTable`, `_keyMap`, `_lkeyMap`, `_exceptionCallback`, `_exceptionHandler`) once the RETIRED
list exists; the 199 reader-less function rows whose body is used elsewhere and the 85 reader-less import-slot
registrations (one exe-side commit; boot's registry-size line changes).

**Needs a conversion first:** 39 storage rows → class statics, each with ALL its first-level sites and its
second-level globals/IAT slots in the owning namespace block (356 + ~1,100 edits; Video::Device, Timer,
FileSystem, ExternalFactory the big ones); `_loader` (real Loader/Thread ctors); `_rng` (single seeder);
`_firstTypeInfo` (reflection fold); the 38 reader-less rows whose body is otherwise unused (delete body + row).

**Must stay (and why):** `?_singleton@MemoryAllocator@` + the 5 MemoryAllocator doors (carry-over 1);
`_disableHandler` (System::Debug unported); the ExternalPtr doors (carry-over 3); `rt_export_addrs.inc` whole
(DLL-derived; teardown after B.3, 395 rows with a non-runtime reader); ~616 live exe function rows + 42 `.data`
rows + 75 live import-slot rows; `CurrentBuildId` (carry-over 2).

**Order inside the storage block(s):** (1) coordinator: RETIRED list in the generator (+ optionally route all
mint paths through `MintStorageCell`); (2) per namespace, per class: declare/define the static, convert every
site in the table, hand in the engine_funcs/IAT lists, add the row to RETIRED — one commit per class, Timer's
five in one, Error's three in one, the four Video list heads in one; (3) coordinator regenerates mint +
manifest, gates. Every storage commit changes the `[RtMint] … storage symbols` boot line.

---

## 6. How measured

All scripts in `C:/Users/USPC/AppData/Local/Temp/claude/b39/0D/`, run on main @ 3fc6d9f6, scanning
`code engine standalone runtime dx11 tests` (`*.cpp *.h *.hpp *.inc *.c`), `build*/` and `obj/` excluded,
`.claude/` never walked. `lib.py`: loader, comment stripper (strings kept / blanked), statement slicer,
binding classifier.
- `storage.py` → `storage_sites.tsv`, `storage_summary.tsv`: rows from `kMintStorage` and `CUTOVER_ROW`;
  literal + in-range hex + named-constant sites.
- `level2.py` → `level2.tsv`: import slots (`Register*Slot(0x14015…, cell)` → readers of that hex), engine_funcs
  globals and file statics assigned at a site → their uses.
- `writers.py` → `writers.tsv`: enclosing function + a store heuristic (W?); the writer column of A.1 was
  then read by hand (`Publish(` sites in singleton_ctors.cpp:270-774, engine_lifecycle.cpp:342/373/439/469,
  saveload.cpp:207/432, animation.cpp:340/364, allocator.cpp:654/659, engine.cpp:467, timer.cpp:187-197).
- `exports.py` → `exports.tsv`, `exports_summary.txt`: per export row, READ / REG / MINT / TEST occurrences
  by name literal and by address; registration tables recognised by file + `(name, 0x…ull)` shape.
- `regexe.py` → `regexe.tsv`, `regexe_summary.txt`: every `RegisterExeAddr` (1,007, matches CLOSE_NUMBERS §4)
  and `Register*Slot`, readers = other code occurrences of the hex or of a constant holding it.
- `deadrows.py` → `deadrows.tsv`: registered symbol of each reader-less row and its tree-wide mention count.
- `efuncs.py` → `efuncs.tsv`, `efuncs_summary.txt`: `extern` globals of engine_funcs.h, initialiser binding,
  uses (extern / definition / initialiser / dump printf excluded).
- `gen_md.py`: the §2 tables. Ghidra: one `get_xrefs_to 0x1801fc6d0, program=Runtime-x64.dll` (read only).

## 7. Not measured

- Which of the 356 storage sites run on the loader thread: inferred from the file (`updateloop_loadresources`,
  `loops.cpp` Loader::Run, `gameupdateloop_serialize`), not traced.
- The writer column is read for the singletons and Timer/Error; the retail writers of the five by-address
  cells and of `_rootOnlyUpdates` / `_frameIndex` were not decoded.
- Whether `System::Random()` reproduces retail initialiser 0x180149650 byte for byte (the `_rng` rule
  depends on it).
- Whether a `System::Thread` built at CRT init (a class-static `_loader`) is safe before `main` — needs a run.
- Second-level uses of per-file accessor wrappers in runtime/ (`SlotVideoDevice()`, `SecondsPerTick()` …)
  are not counted: converting the wrapper body converts them.
- `rt_export_addrs.inc`: "one-line qualified delegation" vs other member doors is not decidable from the row;
  the mirror-needed count (≤63) over-counts registrations made through named constants.
- `RegisterExeAddr` rows registered from table loops (10 expressions): their addresses are not resolved.
- Reader-less row bodies: the "used elsewhere" split is a mention count (≤3), not a call graph.
- The retirement blocks' conversion edit count (~1,100 second-level lines) counts use LINES of the
  engine_funcs globals, not distinct call sites.
