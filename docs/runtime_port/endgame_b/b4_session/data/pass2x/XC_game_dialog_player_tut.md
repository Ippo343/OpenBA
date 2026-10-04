# Wave X inventory XC: game / dialog / player / tutorial / goal / object
811 casts in 139 files, 766 cast lines. Per-cast rows: `XC_game_dialog_player_tut.tsv`;
per-declaration rows (186): `XC_game_dialog_player_tut_families.tsv`.
Classified: SIG 326 (about 150 functions), H5 131, FIELD 89, OTHER 63, POD 60, LOCAL 46,
CONST 43, FLOOR 39, ERASED 5, SLOT 5, H8 4.
## Largest families (casts)
1. CONST:FindOrLoad 43 (one `const_cast` fix on the factory result type unblocks all).
2. H5:HighlightComponent::edgeMeshInstance/transform 18 (Player/tool_base_render.cpp; +0x28 / +0x100).
3. H5:PlayerController::camera 15 (raw `uint8_t camera[0xD8]`, 10 files).
4-5. FLOOR:crt 14 (water_contour_mesh); OTHER raw stack decal block 14; POD:RtObject::Internal 12; LOCAL T2 bridges 12.
6. FIELD:ToolRegistryNode 11 (type is file-local to Dialog/toolbardialog.cpp; Game teardown + Player read it by offset).
7. H5:WorldGeometry::_arrays 11. 8. SIG:Pf_AvlRebalanceB_13480 10. 9. SIG:ToolbarUtility_BstInsert_4890 9.
10. SIG:SetEngineNextLoop 7 (with SetDialogLoaderNextLoop 3: both take the loop as `uintptr_t`),
    SIG:TradeWork_impl_4b40 7, SIG:Pf_RoadMeshRebuild_f370 6, SIG:ScenarioDialog_Ctor 6.
Shared AVL entry points (own ONE owner): Pf_AvlRebalanceB_13480, ToolbarUtility_BstInsert_4890,
TradeWork_impl_4b40 (0x140014b40), PlayerTrade_Retrace_25920, PlayerTrade_BSTRemove_25740,
WorkBST_BSTRemove_a6e0, CitizenAI_Reg_helper_e360: 40 casts across Dialog, Game, Player, Tutorial, Goal.
`FindAvailableBuilding` / `CitizenEmploy_Strat_*` / `TownRegistry_RegisterWorkplace` take the town
manager as `int64_t`: 5 Dialog/Game files cast for them.
## Pure forwarders with ZERO callers (deletable now; verified by grep over code runtime standalone dx11 engine tests)
- `Reimpl_TradeWork_sub_c1e0` -> WorldGeometry::BuildTerrainMesh (Game/serialize_heightmap_mesh.cpp:519, decl Game/exe_bodies.h:283)
- `Reimpl_TradeWork_sub_c780` -> WorldGeometry::UploadHeightImage (Game/serialize_image_upload.cpp:218, decl exe_bodies.h:285)
- `RemovalCalc_CitizenAI_9ad0` -> WorkScheduler::PrioritizeWork (Player/keystone_a_removalcalcpath.cpp:612, decl Player/exe_bodies.h:45)
- `PlayerPath_FindComponent_5780`, `PlayerPath_FindComponent_3ea0` -> ComponentEntity::FindComponent<> (Player/tool_base_path.cpp:56,62;
  only `#include "Player/exe_bodies.h"` lines and comments mention them)
- `PlayerPath_Sub_11e710` -> DestroyComponent::InRemovalState (Player/tool_base_path.cpp:96)
- `MessageDialog_Ctor_Standalone` (a placement-new of MessageDialog; Dialog/messagedialog.cpp:104, decl Dialog/exe_bodies.h:58): no caller.
Deleting these also deletes 9 casts. `EventLog_EventArray_Destroy` is NOT zero (Game_EventNotifierCleanup calls it).
## Stale `// LEFT` claims (the code contradicts them; casts are removable now: reclassified LOCAL)
- "Profession::stringName is raw String storage (H5)": Game/Profession.h:54 declares `System::String stringName`
  (Dialog/citizenui.cpp:400, followdialog.cpp:182: the cast is String -> String).
- "MapGenerator declares _field20 / _field28 / _field30 as void*": Game/MapGenerator.h:89-91 types them
  `UI::StringTable*`, `System::String`, `System::String`; ClimateGenerator likewise. Only
  `TerrainGenerator::_field20` is still `void*` (Dialog/newgamedialog.cpp:122,275,276,284, mapparamsdialog.cpp:47,48).
- "MenuDialogLoopT::_saveLoad is void*": MenuDialogLoopT.h:72 is `Save::SaveLoad*` (Dialog/mainmenudialog.cpp:216).
- "CitizenBehavior::field_0x100 / field_0x160 are void* tree roots": CitizenBehavior.h:287,310 are
  `ProfessionBucket*` / `TemplateBucket*` (Game/citizenlist_serialize.cpp:500,612 actually wait on the
  `void* rootField` parameters of SerializeCompRefTree / SerializeLegacyKeyIntTree; town_workplace_registry.cpp:104
  needs its local ProfessionTreeNode unified with ProfessionBucket).
- "Map::regionPairCache is int64_t*": Map.h:115 is `RegionPairCache*` (Game/keystone_a_gamedtor.cpp:282).
- "PointList::Point::name is the String's raw char*": PointList.h:97 is `System::String name` (Game/starttown.cpp:2453).
- "RtIO::ModArray::data is void*": `Mod::files` is `ModArrayOf<RtSystem::Filename>` (Modification.h:119), data typed
  (Dialog/moddialog.cpp:142, moddetailsdialog.cpp:203).
- "WorkScheduler::field_0x20 is void*": WorkScheduler.h:81 is `WorkSchedulerNode20*` (Game/gameupdateloop_teardown.cpp:445,446).
## Notes for the next wave
- Callers of SLOT:Tool::IsSelected also live in Component/nomadcomponent.cpp:85. SLOT:TutorialSetup::Update /
  TutorialGoal::IsComplete have their callers only in Tutorial/ (StepExecute forwards `void*` time / list).
- `GameUpdateLoop` ctor (uint64_t scenario IBP) is called from 6 files: newgamedialog, saveloaddialog, worldsetup_reload,
  tutorial_next_scenario, gameupdateloop.cpp, GameUpdateLoop.h.
- OTHER rows that must stay faithful: Dialog/residenceui.cpp:465-479 (retail over-reads a 0x40-byte StorageComponent;
  known_port_issues 14a row 452), Game/starttown.cpp:1823 (workers pointer read as a count, known_port_issues 14),
  keystone_a_removalcalcpath.cpp Time/int out slot, the tessellation descBlock (0x60 raw stack block).
- FLOOR candidates to mark `// CAST-OK`: crt (water_contour_mesh, 14 + gameupdateloop_serialize.cpp:885), ptr_int,
  vslot (townhallui.cpp:1090, workplaceui.cpp:483, splash_loop_stubs.cpp:428), float_bits (tessellation), win32, alloc_header.
