# Wave X inventory — XE_standalone (standalone/)

97 casts on 92 lines in 18 files, every cast classified (`XE_standalone.tsv`).
By class: SIG 33, LOCAL 29, FIELD 11, OTHER 9, FLOOR 8, H5 6, SLOT 1.
`XE_standalone_families.tsv`: 11 SIG / SLOT families, each with all callers across the tree.

## Families by casts
1. SIG:CitizenEmploy_SeatWorkers(int64_t town, uint64_t profession, int) 6 — defined code/Game/Component/citizen_employ_cascade.cpp:411; callers starttown.cpp, three harnesses (local externs). Typed: `TownManager*`, `const ProfessionDesc*`.
2. SIG:PerfX::PickCullSkip / ModelCulled(void* model) 6 — perf_experimental.cpp; callers also code/Game/Component/citizenai_executor.cpp, statusiconcomponent.cpp.
3. SIG:EntityUI_SetEditMode_Impl(uint64_t mode) 5 — entityui.cpp:172, exe_bodies.h:47, edittool.cpp (local extern), food_panel_harness, tool_harness (4 sites).
4. SIG:ModDialog_LastOpened / ModDetailsDialog_LastOpened (void* return) 4 — mod_dialog_vtables.cpp:327-328; local externs in autostart.cpp and pausemenu_harness.cpp. Same dialog-as-void* theme as `g_StandaloneNewGameDialog` (FIELD, 2) and `EntityUI_OpenPauseMenu` (SIG, 1).
5. SIG:CompProf_BeginKind(void* comp) 3, SIG:RemovalTool/PlacementTool_Factory_SA (void* return, pure forwarders to `CreateNewObject`) 3, SIG:CitizenList_AddWorker_91130 2, SIG:EditTool_CloseInfoDialog(uintptr_t) 2, SIG:ToolbarUtility_HarnessOpen(uint64_t) 1.
6. SLOT:Component::Update(void* time) 1 (perf_experimental.cpp:417) — Component.h:61, 64 files declare/override a `void*` Update.
Not in the families table: LOCAL 29 (`scene_watch.cpp` 17 and `perf_experimental.cpp` 12 read Camera / MeshInstance / DetailLevel / AnimationState by byte offset; the typed classes exist in runtime/types/graphics, so these two files can be retyped by their own block), FIELD 11 (`MainLoopCallback::_func/_context` 4, `g_BridgeLastMaster/Segments` 2, `g_StandaloneNewGameDialog` 2, `MemoryAllocator::freeLists` 2, `BaseTypeInfo::factoryFn` 1), OTHER 9 (InternalPtr<> raw locals handed to `BeginDragDrop` / combo slots 6 — known_port_issues; `CalcCost(int*)` 8-byte local 2; bool-as-byte 1), FLOOR 8 (dx11_seam 5, vslot 3 — carry `// CAST-OK`), H5 6 (`Element::_name/_toolTipString` 5, `CitizenComponent::name` 1).

## Cross-region dependencies (callers outside standalone/)
- Typing items 1-5 changes code/Game files: citizen_employ_cascade.cpp, starttown.cpp, entityui.cpp, exe_bodies.h, edittool.cpp, edittool_funcs.h, toolbardialog.cpp, mod_dialog_vtables.cpp, placementtool.cpp, removaltool.cpp, citizenai_executor.cpp, statusiconcomponent.cpp, entitylist.cpp. The standalone casts clear when those blocks retype the signatures; the standalone files only follow.
- `AnimBatchDecide(const void* time)` waits on the Component::Update slot (XA's family).

## LEFT comments the code contradicts
- None found. All 84 LEFT lines name a real blocker (a void* / integer signature in code/Game, an engine void* field, or the dx11 seam).
- Note: the two `CalcCost(int*)` rows say "resizing is not B.4's"; `CalcCost` is declared in BridgeTool.h:52, RoadTool.h:48, FencedTool.h:64, RemovalTool.h:53, DragPlacementTool.h:48 and a harness local (tool_harness.cpp:206), so any retyping of that out-param touches those headers (not verified which parameter is the `int*`).

## Deletable now (pure forwarder, zero callers)
- None: every forwarder in this region has live callers.
