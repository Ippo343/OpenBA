# Runtime port — status

Keep this to one screen. Update the closure block after every batch; never copy it into CLAUDE.md.

## Closure (2026-09-12 close, tools/cutover_closure, roots = census + every minted vtable slot)

| | |
|---|---|
| reachable functions from our call surface | 2,871 |
| unported | 128, all C runtime (our CRT covers them) |
| **engine functions to port (static lower bound)** | **0** (206 → 856 → 145 → 0 in one day) |
| minted vtables complete | 152 of 152 (the two unexported IO::Encoding tables joined 2026-09-12 evening) |
| registry doors | name and address are ONE door (`standalone/rt_export_addrs.inc`) |

Worklist: `docs/runtime_port/closure/WORKLIST.md` (empty). Every function the DLL's call graph
reaches from our call surface OR from any minted vtable slot is ported, and every port is
reachable both by mangled name and by retail address. From here the closure cannot help:
what the gate finds next is state, not code — objects handed to our bodies by a route that
built them wrong, and callbacks stored in data.

## Endgame (2026-09-13)

- **B.3.2 OPEN — session handoff (2026-09-14, main e4ac8967):** `Collection::Array<T>`,
  `Set<T>` and `LinkedList<T>` are real templates (runtime/types/collection/Collection.h)
  wrapping the ported cores; all 339 registry rows kept as shims; `make unit` 154 tests.
  Step 2 (callers): Game/Player/Tutorial/Goal MERGED (e4ac8967, gate PASS). Dialog,
  Component/Action/Work/Entity and runtime+standalone+dx11 were in flight on worktree
  branches `worktree-agent-a07a3ff8deedd51a1`, `worktree-agent-af701169855ab5b77`,
  `worktree-agent-a06f7bd4303740385` when the session stopped — **merge each only after
  build + `make unit` + sa-run + resave gate on the merge** (check `git diff --stat` against
  the batch's file set first). Rules: `endgame_b/B32_RULES.md`.
  **Finding that reshaped step 2:** code/Game never reaches runtime/src/collection; its
  Array sites are the EXE-side MSVC monomorphisations of `Array<T>::SetSize` (0x1400021d0,
  0x1400032e0, 0x140003110, …) transcribed per file, and those convert onto the same template
  with `ExplicitSpec` rows (every exe helper zeroes its new tail; `SpecFor<T>` does not).
  **B.3.2 closing-pass list:** unify `code/Game/Component/coll_bridge.h` (GameColl types) and
  `code/Game/Dialog/dialog_array_bridge.h` (DlgElem<N>), then collapse ExplicitSpec rows onto
  trait-derived `Array<T>` where equivalent; route the template's allocation through the same
  allocator door as the tree (it bypasses the `BANISHED_REDZONE` shim today); add the width-8
  `AvlOps` specialisation (Set<Filename> in modification_*.cpp); wire the deep
  `Array::operator=` (`ArrayAssignCore`); an in-place-construct `Set::Insert`; decide
  `fn_ExeArrayResize/2/3/Push` in engine_funcs.cpp (they zero to capacity and carry a bail —
  read the retail exe bodies); the `fn_UI_GetFirstDialog` LinkedList walks in Tutorial; then
  delete `fn_*Array*/Set*` pointers, shims and rows at zero callers and regenerate the .inc
  files; STATUS + B.4 counts.

- **B.3.1 CLOSED (2026-09-13 night):** `System::String`, `System::Filename` and the nine
  `Math` value types are real classes under retail's names (`runtime/types/system/*.h`,
  `runtime/types/math/Math.h`); three cores, five caller batches by directory, one closing
  pass. The C seam between callers and these types is gone: every `fn_String_*`,
  `fn_Filename_*`, `fn_Vector*`, `fn_Quaternion*`, `fn_Transform*`, `fn_Matrix*` pointer in
  `engine/funcs/engine_funcs.{h,cpp}` is deleted, `engine/types/SystemString.h` is deleted,
  and 36 zero-consumer `RtMath_*`/`RtSystem_*` shims went with their 44 registry rows.
  `standalone/filename_dtor_check.cpp` and `BANISHED_FNDTOR_CHECK` are gone with the
  `fn_Filename_Dtor` pointer they interposed (options.h is still the whole flag inventory).
  The 21 smallest Math members are now `inline` in Math.h (bodies moved verbatim).
  `make unit`: 154 tests, 0 failures. Routes, all exit 0, no crash/hang log: autostart +
  pause-menu quit, the Mods route, the tutorial route, the Load Game dialog.
  `tools/dispatch_slot_map.py --check`: 0 mismatches. `make sa-stress` reads sim ms/iter
  **+5.5% against the 2026-09-05 baseline**, of which +4.4% is already there with the
  inlining reverted — the inlining itself is inside the run-to-run spread.
  `reinterpret_cast` (ENDGAME §B.4 metric), start -> now:

  | | code | engine | runtime | standalone | dx11 |
  |---|---|---|---|---|---|
  | start (main 1d97579c) | 23,657 | 962 | 6,826 | 3,066 | 145 |
  | now | 23,784 | 906 | 6,788 | 3,055 | 148 |

  It moved barely at all, and `code/` went UP, because the conversions traded one cast
  shape for another: a `fn_String_Set(field, s)` call became
  `reinterpret_cast<System::String*>(field)->Set(s)`. Those are the B.3.1 bridge casts
  (`// B.3.1 bridge`), one per use of a POD member of a struct that is not a class yet;
  each disappears with that struct's own §B.2 conversion, not before.

  **Left, and why** — all of it belongs to a later §B.2 class conversion, not to B.3.1:
  - Bodies still in C shape over these types: `IO::FileSystem::ReadPackfile`,
    `WriteSettingsBody`, the RtSink-routed `runtime/src/save/saveload_update.cpp`, and
    `UI::WideString` (never converted; it is its own type, not System::String).
  - `RtSystem::String` / `RtSystem::Filename` PODs and `runtime/src/system/pod_bridge.h`
    stay while any struct still declares one as a MEMBER: `IO::FileSystem` (5 fields),
    `IO::PackageFile` (5), `IO::Persistence` (5), `Achievement::Achievement` (3),
    `Achievement::Goal` (1), `Engine::UpdateLoop`'s two loop subclasses (2),
    `IO::Registry` (1), `IO::Modification` (2, `Filename`), plus
    `runtime/types/object/External.h`'s `using RtSystem::Filename`.
  - Shims still reached by name or address from an unconverted caller stay: String's
    Set/Dtor/Assign/SubString/Format/Serialize/InsertAt/RemoveAt/operator+/StrCompare,
    Filename's ctor/dtor/assign/compare family, and the Vector/Quaternion/Transform
    members the graphics bodies call.
  - The global aliases (`Scalar`, `Quaternion`, `Matrix`, `Transform`, `Transform2`,
    `Transform3` in Math.h; `Vector2/3/4` in engine/types/common.h) all still have
    hundreds of users and stay. `RtMath::` is already gone.

- **CLOSED (2026-09-13):** the stress-town teardown crash/hang was `FishingWork::OnCancel`
  ported with three parameters where retail forwards a fourth (the packed slot word) by
  register passthrough — Slot6 then stored a `2` and a null past two 4-entry arrays on every
  cancelled fishing action. One-line fix in `code/Game/Work/fishingwork.cpp`; root cause,
  measurement and what the earlier analysis got wrong: `TEARDOWN_WILD_WRITE.md` §0. Verified:
  debug exe reproduced 1/1 before the fix with the writer named, 0/2 after (both teardown
  brackets clean, clean exit). The loader race in `ExternalFactory::Add` was not reached.

The plan for what follows the gate is `ENDGAME.md`: A remove the DLL, C remove the flags,
B fold the C bodies into real classes, in that order. **2026-09-13: A.1 and A.2 are done.**
Tag `dll-present-last` (commit `86d28a73`) is the last commit where the exe still loaded
Runtime-x64.dll; the exe never loads it now. `make sa-run` is the DLL-absent route and
`make cutover` is kept as an alias for it.
- The flip exposed one bug the old gate could never see: `Video::Device::InitializeInterfaces`
  still `LoadLibraryW`'d the video plugins, and VideoDX9-x64.dll statically imports
  Runtime-x64.dll, so the DLL was mapped behind `ResolveInit`'s back and its first heap call
  faulted. The move-aside had made that LoadLibrary fail for the wrong reason. The loop now
  loads nothing (`runtime/src/video/video_interfaces_win.cpp`); `ResolveInit` fails fast if the
  DLL is ever found mapped. Verified 2026-09-13 with the DLL on disk: sa-run + pause-menu
  quit exit 0, `BANISHED_AUTOQUIT`, and the resave gate PASS.

- **2026-09-13 later: A.3, A.4 and C.1 are done** (commit `5084c732` on `endgame-a3`; five
  parallel worktree batches by directory plus one serial pass over the shared definitions;
  493 files, +1.9k/-83k lines). Gone: the raw door (`RtOriginal`, `RT_ORIGINAL_ONCE`,
  `ResolveExportRaw/OrdinalRaw/RuntimeRaw`, the ordinal door), the diff oracle (`rt_diff`),
  the predictive oracle (`rt_predict`; `RtSink` survives as `runtime/funcs/rt_sink.h`, the live
  sink the save/ui bodies are written against), `io/save_trace`, 46 `*_probe.cpp`, the alloc
  and mint shadows, the 11 detour/hook installers and `hde/`, the byte-pins and `*_sweep`
  make targets, `tools/attic`, and every `BANISHED_RT_*` read (`runtime/` getenv 133 -> 7).
  `allocator_lock.cpp` (a byte patch on the DLL's Allocate/Free) is reduced to
  `standalone/alloc_stats.cpp`.
  - **Five live dependencies on the raw door were found**, each a call through NULL since
    A.1 that no gate had caught: `Graphics::Geometry` copy ctor in `graphics/mesh.cpp`
    (every MeshInstance / EdgeMeshInstance copy), `MemFree` in `ui/sprite.cpp`'s
    `SpriteSheetAssignString` (every refcount-to-zero string release), `MemAlloc/MemFree`
    in `system/string.cpp` and `object/internal.cpp` (raw first, registry fallback), and
    `_beginthreadex` in `system/thread_win.cpp`. All folded to the registry door.
  - A.5 routes, DLL-absent, all exit 0 with an empty crash log: `make sa-run` +
    pause-menu quit (town built, App::Quit), `BANISHED_AUTOQUIT`, the Load Game dialog
    (`BANISHED_AUTOMENU_BUTTON=2`), the Mods route (0 rows, no .pkm installed), and the
    resave gate PASS (byte-identical to the golden, same as the baseline on `c1fdd7f6`).
  - **The first `make sa-stress` died in boot with a VCALL-of-0** on `GraphicsMesh`'s minted
    vtable: `?PlatformSerialize@GraphicsMesh@` was unregistered in the PERF build only.
    Cause (pre-existing, 2026-09-12): seven registrations were written as
    `printf("... %d", RtXxxRegister())`, and the perf build compiles `printf` to `((void)0)`
    (`standalone/log_strip.h`), arguments included. Batch C graphics/achievement/audio/video
    leaves and three math rows never registered in any perf exe. Hoisted; sweep with
    `grep -rnPzo '(?s)printf\s*\([^;]*?Rt[A-Za-z0-9_]*Register\s*\(\)[^;]*?\);'` (0).
  - **Second perf crash, also perf-only and also pre-existing:** with the leaves registered,
    `Renderer::DrawItemList` faulted in `Context::SetTransform` reading the matrix the
    geometry's vtable slot 6 (`GetInstanceTransform`, returns `Math::Matrix` BY VALUE) had
    returned. Our `ParticleInstance::GetInstanceTransform` port (wave27_lane_gfxpart.cpp) was
    declared `void`: the hidden return slot arrives in RDX and its address must go back in
    RAX; at -O0 RAX happened to still hold it, at -O2 it was garbage. Now returns the slot.
    Audit over all 175 registered by-value-returning exports: no other port declared void
    (address-only registrations not covered). Same debug-exe town ran 1200 frames clean.
  - `make sa-stress` on the perf exe then completes 6000 frames with a clean quit. Sim cost
    0.202 ms/iteration vs the 2026-09-05 DLL-present baseline's 0.192 (+5.0%, `ph_postsim`
    0.069 -> 0.097 ms/frame is most of it); a second run with a compile going on beside it
    read +10.3%, so the run-to-run noise on this box is of the same size as the delta. The
    perf gate was deferred to after cutover by the user (Wave 4); recorded, not chased here.
    A clean A/B needs the machine idle and the baseline re-cut on this build.
  - The debug exe's stress town (`sa-stress-debug`, 1200 frames) simulates clean but CRASHES
    IN TEARDOWN after the quit click: READ fault inside `rtalloc::Free` from `MemFree` in
    `ComponentEntity_Dtor` <- `InternalBasePtr::DeletePtr` <- `EntityList_RemoveEntity` <-
    `EntityList_Shutdown` <- `GameUpdateLoop_Shutdown`. The normal-size town's pause-menu
    Quit exits 0; this is the teardown defect already listed above (seen once on 09-13
    morning), now reproducible with the 800-citizen town on the debug exe.
- **Serializer gate exists and passes**: `tools/resave_gate/resave_gate.sh Gate` (README
  there). Load a save DLL-absent, serialize the restored world again before the first tick,
  compare the raw bytes against a golden. Golden cut against `Gate.sav`
  md5 `68f99e06e6cdce6c17a8fb06e867b2c8`; two pinned runs byte-identical.
- Its first run found the last DLL-absent SAVE blocker: `CompressedStream::SetWindow`'s
  write-side grow path was still delegated to the DLL (the cutover gate never saved).
  Ported; the DLL-absent autosave now completes.
- Seen once each, pre-existing, not fixed: a loader race crash in `ExternalFactory::Add`
  (AVL rotate on a null node, `[GUL::Load] redundant dispatch` just before) and a hang in
  teardown after the pause-menu Quit returned.

- **2026-09-13: C.3 — the options struct exists and `standalone/` is at zero `getenv`.**
  `standalone/options.{h,cpp}`: one `Options` struct, filled once by `OptionsInit()` at the
  top of `WinMain` (before `HarnessGuardUnattendedBoot`, before any other reader), read
  everywhere else as `Opt().field`. Every field carries a comment naming its reader — a
  Makefile target, a `tools/` script, `perf/`, or a route documented in CLAUDE.md — and the
  ones nothing outside the tree sets are marked `[no tool reader]`, which is the C.4 delete
  list ready-made. The header also declares the tier-3 fields the `code/` sites will move
  onto (world setup, the stress/perf knobs, the `BUILD_DEMO_*` founding-town flags,
  `MENU_SCENE`, `SAVE_TEST`, `MODAL_ANSWER`), so it is the whole inventory, not just
  standalone's half.
  - **Both mid-run environment WRITES are gone.** `stress_harness.cpp`'s `SetIfUnset` bundle
    (autostart, map/seed/difficulty/climate, camera pin, the demo buildings, the in-game
    quit) is now `ApplyStressDefaults` inside `OptionsInit`, and
    `construction_force_harness.cpp`'s `_putenv_s("BANISHED_BUILD_SYNC_DELIVERED")` — which
    raced `GatherRawMaterials::Execute`'s own cached read — is the derived field
    `Options::buildSyncDelivered`. `OptionsInit` still `_putenv_s`es the stress bundle for
    the ~600 not-yet-migrated `code/` readers; that half goes with them, and `env_cache.h`
    stays until then.
  - **Deleted with their flag** (no reader in Makefile / tools / perf / .vscode / CLAUDE.md /
    STATUS.md): `BANISHED_W17_ATTEND` and all of `standalone/wave17_door_attend.cpp` (the
    Wave 17 door-attendance census, a per-`ResolveExport` strcmp loop),
    `BANISHED_MOUSE_DRAGSKEW` (existed only to skew what the A.4-deleted diff oracle could
    see), `BANISHED_AUTOSAVE_TRACE`, `BANISHED_LANE4_TRACE`, `BANISHED_W2LANE3_TRACE` (wave
    traces), and a dead `EnvOn` in `rt_data_mint.cpp`. Nothing else was deleted: the
    gameplay harnesses are wired into `code/Game/Game/gameupdateloop.cpp`, `starttown.cpp`
    and a dozen components, so retiring them belongs with the `code/` pass, not this one.
  - Verified: `standalone-debug` and `standalone-perf` both build clean, and all three
    routes exit 0 with an empty `standalone_crash.log` — `sa-run` with
    `BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120` (town built, pause menu opened at
    frame 601, Quit clicked at 721), `BANISHED_AUTOQUIT`, and the Load Game dialog
    (`BANISHED_AUTOMENU_BUTTON=2 BANISHED_AUTOMENU_CLICKS=m6`).

- **2026-09-13: C.2 and C.4 are closed — the whole tree is at zero `getenv` and there are
  no live-feature gates left.** Four directory batches plus a closing pass.
  - **Gates:** the 38 remaining `kDefaultOnFlags` entries, `LiveEnabled`, `EnvOn` and the
    `BANISHED_LIVE_COMPONENTS` master read are deleted; the `EnvVar` parameter is gone from
    every `COMPONENT_LIVE_FACTORY*` / `CONTROLLER_LIVE_FACTORY*` macro and its 14
    invocations. The twelve flags the batches left undecided were proven in one forced-on
    run (all twelve set: `sa-run` + pause-menu quit exit 0, empty crash log, resave gate
    PASS twice, byte-identical to the golden) and graduated: `LIVE_AUTOSAVE`,
    `LIVE_BUILD_ONREMOVE`, `LIVE_CEMETERY`, `LIVE_CITIZEN_FAMILY`, `LIVE_CROPDISEASE`,
    `LIVE_FOUNDING_SPAWN`, `LIVE_HAPPINESS`, `LIVE_HEALER`, `LIVE_MARKETLOC`,
    `LIVE_TOMBSTONE`, plus `CLEAR_FOOTPRINT` and `LIVE_ANIMALS`, whose readers the batches
    had already removed. Graduating `LIVE_FOUNDING_SPAWN` deleted its off arm: the
    hand-rolled `SpawnOneCitizen` family loop in `starttown.cpp` and its four helpers
    (~230 lines) are gone, so the retail `sub_1040` port is now the only founding spawner.
    Three standalone opt-ins that were OFF on every default run went with their flags
    (`LIVE_CITIZENAI_TIDS` / `_HAPPYID` / `_AIID`: the `c42c0` and `c70e8` type-id fills).
  - **`getenv` is zero** in `code/`, `runtime/`, `dx11/`, `engine/` and `standalone/` — the
    only call sites left are in `standalone/options.cpp`. 69 reads moved onto `Opt()`
    (code 66, dx11 1, engine 2), with ~40 new typed/raw fields in `options.h`.
    `standalone/env_cache.{h,cpp}` and the `-include standalone/env_cache.h` in the
    Makefile are deleted, and `ApplyStressDefaults` no longer writes the environment: it
    sets the struct fields, so `OptionsInit` is now read-only with respect to the CRT
    environment block.
  - **Shims deleted with their call sites:** `MapQuery_FloorTraceOn` (+10 guarded traces),
    the empty `StartTown_DriveProduction` (and `production_harness.cpp`),
    `WorkPlaceComponent_ControllerLive`, `ObjAudit::Enabled` and the diagnostic half of
    `internal_factory_audit` (`Scan`, `ReportLeaks`, `NoteUnregisteredFree`;
    `IsRegistered`/`Count`/`Factory` stay — they are load-bearing on the free path), and
    the six empty probe stubs `WorkSchedProbe_Report`, `DumpSlot3Inventory`,
    `ModProbe_DumpModList`, `ModProbe_StartWatcher`, `TradeUI_PriceProbeReport`,
    `TradeUI_SortSelfTest`. `Options::buildSyncDelivered` lost its last reader and went too.
  - Verified on both exes: `standalone-debug` and `standalone-perf` build with exit 0 and no
    new warnings, and every route exits 0 with an empty `standalone_crash.log` — `sa-run` +
    pause-menu quit, `BANISHED_AUTOQUIT`, the Load Game dialog
    (`BANISHED_AUTOMENU_BUTTON=2 BANISHED_AUTOMENU_CLICKS=m6`), and `make sa-stress` on the
    perf exe (6000 frames, clean self-quit).

- **2026-09-13: frozen-exe detector.** The boot-state watchdog now carries the main loop's
  frame counter; a stalled counter publishes `STATE=HUNG` (exit 5 from `tools/boot_state.sh`),
  writes `standalone_hang.log` with all-thread stacks, and on unattended runs terminates the
  process with exit code 4 — so a hang costs ~30 s instead of the caller's whole timeout.

## Cutover gate

`make sa-run` (`make cutover` is the same thing under its legacy name): deploys the debug
exe and boots it — the DLL is never loaded, so there is nothing left to move aside. Run it
under `tools/game_dir_lock.sh`. Symbolize frames with
`addr2line -f -C -e standalone/build/reimpl-x64.exe 0x14xxxxxxx`. The history below (walls,
"DLL-absent" language) predates A.1 and describes the old DLL-present/DLL-absent procedure,
recoverable at the `dll-present-last` tag.

Walls passed 2026-09-12: the Video shader family → the UI Description/Element family (53
classes) → `Font::Serialize`'s two by-address callees → (evening walk, one session, serial)
the two unminted `IO::Encoding` tables (Visitor 0x1801559f8, Text 0x1801559d0; both minted
by address, ten slot bodies in `io/encoding_visitor_text.cpp`) → the two unminted
`GraphicsMesh::Serialize` statics (0x1801fc69c/0x1801fc6a0, `EXTRA_STORAGE_BY_ADDR`) → a HANG,
not a crash: `AnimationGroup::Serialize` emitted the +0x30 float BEFORE the tag array (retail:
after), so the tag count came out of the float's bytes and `CompressedStream::Read` spun on a
zero-length chunk past EOF (found with `gdb -p`, not the crash log) → **the gate then booted to
the in-game world** (24 citizens, 21k entities at frame 7200) → the quit path: eleven scalar
destructors the `RT_DELDTOR` table still resolved to NULL (`object/deleting_dtor_leaves.cpp`:
SoundEffect, MaterialInstance, AnimationGroup, ToolTipDialog, ReaderWriter, Light, SunLight,
AchievementInstance, Base, Stream, ParticleInstance) → one `RT_ORIGINAL_ONCE` on a live path
(`video/buffers.cpp` MemFree) → **`make cutover` with `BANISHED_PAUSEMENU=600
BANISHED_PAUSEMENU_QUIT=120` exits 0 with an empty crash log and the DLL absent.**

**The gate's "done" condition is met: boot to the world, quit cleanly, DLL absent.** Then the
render walk (same evening): the user saw a black window. RenderDoc on a DLL-absent frame 150
showed 6 draws (DLL-present: 453); `Renderer::AddDrawItem` dropped every item on its
"VideoProgram+0x20 null" arm because every `MeshLayout\*.rsc` load failed External::Load's
version check (`[ExtLoad] badVer=65`, 0 DLL-present): `VertexLayout::GetFileVersion` returned
1, retail returns 3. Fixed; the DLL-absent frame is now 460 actions like DLL-present, and the
menu, title and terrain draw. **The three render defects from the side-by-side captures are fixed (2026-09-12 late):**
- no building or tree meshes: `MeshInstance::Update` reads two UNEXPORTED `.data` statics by
  address (guard 0x1801ff30c, 48-entry identity Transform table 0x1801ff310) and declined the
  whole update when they resolved NULL. Both minted (`EXTRA_STORAGE_BY_ADDR`). The tell was one
  `[SLOTS29] ... declining` line in the DLL-absent log that the DLL-present log did not have.
- first glyph only, and the wrong glyph: `Font::ComputeLineLayout` stored the codepoint in the
  layout entry where retail stores the GLYPH index (0x180116d99, EDX from the table load at
  0x180116d1a). AddFont uses it as the sheet sprite index; the sheet has 96 sprites.
- the grey rectangle went away with the glyph fix (a label's measured width, presumably).
Menu (town, market, text) and the in-game world (trees, rocks, shadows, toolbar) now look
right DLL-absent by screenshot (`tools/capture_window.ps1`). **The DLL-present build never
enters the ported text path** (a trace in our `AddFont` fired 0 times with the DLL present),
so a text-path port can only be seen DLL-absent; assume the same of any path a DLL-internal
caller reaches.
Method that worked twice tonight and should be the default for render walks: capture under
RenderDoc DLL-absent, look at the frame overview and one draw's state, then put a
count-limited `fprintf` on the silent-drop arms of the body the numbers point at, one gate
run, remove the trace. Also audited: the 19 `GetFileVersion` ports written as a constant all
match the DLL bytes now; ports that return through a variable were not covered.
Raw-door triage (2026-09-12 late): `BANISHED_RT_STRICT=1` on the gate prints `[RtRawResidue]`,
the raw-door requests the run actually made. 42 DLL-absent (each once). All but one are oracles,
probes, byte-pins, hook installers or `if (!g_portOn)` fall-throughs that degrade to our body or
print "not checked"; the one live dependency was `userinterface_hover.cpp`'s string release in
BeginToolTip (MemFree through the raw door), now the registry door. The other 600-odd sites were
not reached by this route; re-run the strict gate on any new route and read the table.
Load Game route, DLL-absent (2026-09-12, `BANISHED_AUTOMENU_BUTTON=2`): two defects, both fixed.
- EXEC-of-0 in `SaveLoad::UpdateList` state 1: the save-list sort calls the DLL's own CRT
  `qsort` by ADDRESS (0x18007f050) and every lane before had filed it "CRT, out of scope";
  the closure report still counts it among the CRT rows "our CRT already provides", but no
  body sat behind the retail address. Ported with `shortsort` (0x18007f340) in
  `runtime/src/crt/msvc_qsort.cpp` (the MSVC algorithm, not `::qsort`, because the
  comparator is not a strict weak order and the algorithm decides the tie order).
- Load/Cancel/Delete greyed: `UI::Element::Enable`'s port had the argument sense inverted,
  under a banner claiming the inversion was retail's. The DLL-present build never runs that
  body outside the diff oracle (dialogs reach slot 8 through the DLL's vtables), so the
  screenshot pair present/absent was the first thing to show it. Same lesson as the text path
  above: a body a DLL-internal caller reaches can only be seen DLL-absent.
The dialog now matches the DLL-present capture. Clicking Load in that dialog (the actual
save load, DLL-absent) has not been driven yet; there is no unattended click into
SaveLoadDialog.
Pressing Load (attended, DLL-absent, same evening): the save deserialized up to
`InternalBasePtr::Serialize`, whose PersistObject door was `RtOriginal` (null with no DLL);
now the registry door. After that the save loads and plays.
Picking parity, DLL-absent vs DLL-present (2026-09-12, reported as "click-to-highlight a
touch off"): a seeded, camera-pinned new game (`BANISHED_SEED=12345 BANISHED_CAMERA_PIN=town
BANISHED_SELECT_DIAG=1`) with the same 54 synthetic clicks (`SetCursorPos` + `mouse_event` at
fixed client coordinates) in both builds gave IDENTICAL terrain hit positions, cells, picking
mesh ids and selected entities on every click, and the same house highlighted. Also checked
against retail bytes: `Frustum::GetScreenDirection`, `Cursor::Serialize`, the cursor block of
`UserInterface::Draw`, `PickingComponent::OnAdded`'s AddMesh transform; window/client/backbuffer
sizes are equal in both logs. Whatever the offset is, it is not DLL-absent-specific; the
cursor is the OS cursor in both builds (the sprite-cursor arm of `UserInterface::Draw` is not
taken at the menu). Next step if it persists: the same click grid against retail's own
Application-x64.exe on one shared save.

## DLL-present build

`make sa-run` boots to the in-game world and pause-menu Quit works (re-verified 2026-09-12
at close, 975 UI exports registered, 135 deleting-dtor rows: `BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 BANISHED_PAUSEMENU=600
BANISHED_PAUSEMENU_QUIT=120`, exit 0, empty crash log). Ten divergent sites, all the
pre-existing negative controls and vtable-identity artefacts (the `AnimationEvent()` ctor
oracle still trips the released-pointer detector; make it NO-ORACLE when someone is there).
The two that appeared when the doors were unified were harness artefacts, resolved 2026-09-12
late: `SetupShaders` compared four padding bytes of an uninitialised snapshot; `Flush`
compared the live dirty mask AFTER the real draw had consumed it. Both now 0.
Fixed the same evening, DLL-present: hovering any dialog moved it (the pause menu followed the
cursor) — `DialogInstance::Update` wrote the tooltip-settle mouse position into `_upperLeftX/Y`
(+0x88) where retail uses +0x90/+0x94; found with a hardware write watchpoint from the
pause-menu harness plus a scripted mouse (`tools/capture_window.ps1` to look).
Drag-tool flashing (2026-09-12 late, both builds): `DialogInstance::Update`'s port read
Element flags bit2 (`NoMouseCapture`, what `Dialog/CreateBar.rsc` sets) as "always hit"; retail
(0x180113b9a) skips the capture block on it. The cursor-following cost panel captured the mouse
every frame, UserInterface+0xd9 went to 1, the tool tick passed OnKey key=0, OnKey closed the
panel, the next frame reopened it. Fixed from the bytes; the autostart + pause-menu-quit route
still exits 0. Not yet re-driven with an attended drag.
The unattended Mods route (`BANISHED_AUTOMENU_BUTTON=7 BANISHED_AUTOMENU_CLICKS=1001,5001,d0,
4001,101,m6 BANISHED_MODAL_ANSWER=1`) runs to Quit, exit 0. It needs at least two `.pkm` in
`WinData/` (both live in `WinData/_mods_off/` because the achievements harness needs them
out); with none, the driver now skips per-row tokens with a `[AutoStart] WARN`. ToggleMod
was also wrong (read the model and inverted; retail pushes the checkbox's own state).

## Held back, with reasons

- `System::MemoryAllocator` (7 bodies) armed only under `BANISHED_RT_ALLOC`, which
  `BANISHED_RT_NO_DLL` now implies. Needs a DLL-present A/B before it goes default-on there.
- Perf gate breached +7.7% in Wave 4; the user deferred it to after cutover.
- `VertexLayout::Add` / `GetStreamCount` / `GetElement*` are non-virtual and unported.
- Ports reached only through the DLL's own vtables (every `UI::*Description` / `*Element`
  body, the ShaderProgram family, Audio::Stream::Run) show no oracle attendance DLL-present;
  only the gate exercises them. The DLL-present run proves link/register/boot, nothing more.
- `deleting_dtor.cpp` row indices are table positions (135 rows now). Every parallel batch
  collided on them; append after the last row and re-count before registering.
- `tools/rt_deleting_dtor_verify.py` flags the two `Object::*Compiler` rows BAD (their dtors
  restamp two vtables; the reader knows one) and eight rows UNRECOGNISED (bare tail-jump
  dtors). None is a port defect; each is explained at its row.
- Two mesh-code lines reached the DLL through `RT_ORIGINAL_ONCE`, the raw door that skips
  the registry, and silently bypassed two ported bodies until 2026-09-12. A bypassed port
  never runs, so the oracle cannot see it. Grep for that macro before trusting attendance.
