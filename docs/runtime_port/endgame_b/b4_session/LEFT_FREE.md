# LEFT_FREE: declarations left as free functions, and `void*`-first parameters in code/Game

Read at commit `239c7591d` (B4 close, rules `B4_RULES.md` section 6 item 4). Checked mechanically by `acceptance.sh` lines 08 and 09 (an identifier counts as listed when it appears anywhere in this file).

Totals: **A = 408 declared names** (411 declarations; `fn_FindComponent_Storage`, `Avl38SetRemove`, `Avl38SetFindRemove` are declared twice with different types) in `exe_bodies.h` / `*_funcs.h`; **B = 106 functions** with a `void*` first parameter in code/Game.

Reason vocabulary: `free in retail` / `member of <Class>, not moved` / `erased core` / `forwarder` / `harness / port-only` / `port gap helper` / `unclear`. The class was assigned from the declaration, its comment and the first parameter; `member of` rows follow the rule "first parameter is the object, retail symbol is a member" and were not individually checked against the retail vtable/symbol.

## A. Declarations left in exe_bodies.h / *_funcs.h

### Action (68)

| name | retail address | class | reason |
|---|---|---|---|
| `CheckCellTerrain` | 0x14009cb00 | member of Map, not moved | first parameter is the object (Map* map); retail symbol is a member (exe_bodies.h:27) |
| `SetGrowthProgress` | 0x140120db0 | member of GrowthComponent, not moved | first parameter is the object (GrowthComponent* self); retail symbol is a member (exe_bodies.h:28) |
| `ModelMeshGroupCount` | 0x1401252f0 | member of ModelComponent, not moved | first parameter is the object (ModelComponent* model); retail symbol is a member (exe_bodies.h:29) |
| `ModelMeshCount` | 0x140125320 | member of ModelComponent, not moved | first parameter is the object (ModelComponent* model); retail symbol is a member (exe_bodies.h:30) |
| `ModelSelectMesh` | 0x1401253d0 | member of ModelComponent, not moved | first parameter is the object (ModelComponent* model); retail symbol is a member (exe_bodies.h:31) |
| `NaturalResourceSetAnimation` | 0x140126440 | member of NaturalResourceComponent, not moved | first parameter is the object (NaturalResourceComponent* self); retail symbol is a member (exe_bodies.h:32) |
| `NaturalResourcePlaceAtCell` | 0x140126540 | member of NaturalResourceComponent, not moved | first parameter is the object (NaturalResourceComponent* self); retail symbol is a member (exe_bodies.h:33) |
| `RemoveNaturalResourceCore` | 0x1401260f0 | member of NaturalResourceComponent, not moved | first parameter is the object (NaturalResourceComponent* self); retail symbol is a member (exe_bodies.h:34) |
| `TryCompleteTask` | 0x1400f8760 | member of WorkScheduler, not moved | first parameter is the object (WorkScheduler* sched); retail symbol is a member (exe_bodies.h:39) |
| `TaskReassign` | 0x1400f8800 | member of WorkScheduler, not moved | first parameter is the object (WorkScheduler* sched); retail symbol is a member (exe_bodies.h:40) |
| `TaskCleanup` | 0x1400f8e30 | member of WorkScheduler, not moved | first parameter is the object (WorkScheduler* sched); retail symbol is a member (exe_bodies.h:41) |
| `HideCitizenTool` | 0x1401176e0 | member of CitizenComponent, not moved | first parameter is the object (CitizenComponent* citizen); retail symbol is a member (exe_bodies.h:42) |
| `ShowCitizenTool` | 0x1401173b0 | member of CitizenComponent, not moved | first parameter is the object (CitizenComponent* citizen); retail symbol is a member (exe_bodies.h:43) |
| `HuntIsReady` | 0x140122a80 | member of HuntComponent, not moved | first parameter is the object (HuntComponent* hunt); retail symbol is a member (exe_bodies.h:49) |
| `QwordArraySetSize` | 0x140003110 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:50) |
| `MarkEntityForRemoval` | 0x1400c2ea0 | member of ComponentEntity, not moved | first parameter is the object (ComponentEntity* entity); retail symbol is a member (exe_bodies.h:67) |
| `NotifyEntityChanged` | 0x1400c3470 | member of ComponentEntity, not moved | first parameter is the object (ComponentEntity* entity); retail symbol is a member (exe_bodies.h:68) |
| `FindCharacterComponent` | 0x1400012d0 | member of ComponentEntity, not moved | first parameter is the object (ComponentEntity* entity); retail symbol is a member (exe_bodies.h:75) |
| `SetFacingDirection` | 0x14010a060 | member of CharacterComponent, not moved | first parameter is the object (CharacterComponent* character); retail symbol is a member (exe_bodies.h:76) |
| `SetAnimation` | 0x14010a1f0 | member of CharacterComponent, not moved | first parameter is the object (CharacterComponent* character); retail symbol is a member (exe_bodies.h:77) |
| `CitizenBreakCheck` | 0x140116650 | member of CitizenComponent, not moved | first parameter is the object (CitizenComponent* citizen); retail symbol is a member (exe_bodies.h:78) |
| `ActMv_ExternalPtrSerialize_22970` | 0x140022970 | member of ExternalPtr<T>, not moved | per-type ExternalPtr<T>::Serialize instance, spelled free (exe_bodies.h:83) |
| `ActMv_UpdateFacing_c8d20` | 0x1400c8d20 | member of MoveTo, not moved | first parameter is the object (MoveTo* self); retail symbol is a member (exe_bodies.h:84) |
| `ActMv_PathSetup_c8c10` | 0x1400c8c10 | member of MoveTo, not moved | first parameter is the object (MoveTo* self); retail symbol is a member (exe_bodies.h:85) |
| `BreakTimeCheck` | 0x1400a9290 | free in retail | static helper / callback; first arg is a System::Time (not an owned object) (exe_bodies.h:90) |
| `PlayModelAnimation` | 0x1401250f0 | member of ModelComponent, not moved | first parameter is the object (ModelComponent* model); retail symbol is a member (exe_bodies.h:91) |
| `StorageSubtract` | 0x14012d150 | member of StorageComponent, not moved | first parameter is the object (StorageComponent* storage); retail symbol is a member (exe_bodies.h:101) |
| `StorageAdd` | 0x14012d000 | member of StorageComponent, not moved | first parameter is the object (StorageComponent* storage); retail symbol is a member (exe_bodies.h:102) |
| `AutoPickupAppend` | 0x140044810 | member of AutoPickupComponent, not moved | first parameter is the object (AutoPickupComponent* self); retail symbol is a member (exe_bodies.h:103) |
| `ResourceCategory` | 0x140088f90 | member of ComponentDescription, not moved | first parameter is the object (ComponentDescription* material); retail symbol is a member (exe_bodies.h:104) |
| `TrackerUpsert` | 0x140058fd0 | member of TrackerComponent, not moved | first parameter is the object (TrackerComponent* tracker); retail symbol is a member (exe_bodies.h:105) |
| `RegisterRawMaterial` | 0x140088ef0 | member of CitizenBehavior, not moved | first parameter is the object (CitizenBehavior* town); retail symbol is a member (exe_bodies.h:106) |
| `PositionRawMaterial` | 0x140129760 | member of Component, not moved | first parameter is the object (Component* self); retail symbol is a member (exe_bodies.h:107) |
| `ActRm_ArrayResize16_da70` | 0x14000da70 | member of Collection::Array<T>, not moved | per-type Array<T> SetSize / Serialize / Clone / Destroy instance, spelled free (exe_bodies.h:124) |
| `ActRm_VectorClone_d9d0` | 0x14000d9d0 | member of Collection::Array<T>, not moved | per-type Array<T> SetSize / Serialize / Clone / Destroy instance, spelled free (exe_bodies.h:125) |
| `HeadingToward` | 0x1400c29a0 | free in retail | static helper (scalar / vector first parameter, no this) (exe_bodies.h:140) |
| `PickConsumeRecipe` | 0x140119470 | member of ConsumeProduceComponent, not moved | first parameter is the object (ConsumeProduceComponent* self); retail symbol is a member (exe_bodies.h:144) |
| `PathPairs_Clear_25b30` | 0x140025b30 | member of PathGraph, not moved | first parameter is the object (PathGraph* pg); retail symbol is a member (exe_bodies.h:150) |
| `Pf_SetFlagRefreshUpdate_107260` | 0x140107260 | member of Component, not moved | first parameter is the object (Component* self); retail symbol is a member (exe_bodies.h:160) |
| `InteractNodeCount` | 0x140123070 | member of InteractComponent, not moved | first parameter is the object (InteractComponent* self); retail symbol is a member (exe_bodies.h:161) |
| `StorageAcceptsMaterial` | 0x14012c560 | member of StorageComponent, not moved | first parameter is the object (StorageComponent* storage); retail symbol is a member (exe_bodies.h:162) |
| `Pf_StorageDropOff_116760` | 0x140116760 | member of Component, not moved | first parameter is the object (Component* self); retail symbol is a member (exe_bodies.h:163) |
| `Pf_StorageCountDelta_1167c0` | 0x1401167c0 | member of Component, not moved | first parameter is the object (Component* self); retail symbol is a member (exe_bodies.h:164) |
| `Pf_RayTriIntersect_aa810` | 0x1400aa810 | free in retail | static helper; first arg dead in retail (void*) (exe_bodies.h:167) |
| `Pf_TileShapeLut_ae180` | 0x1400ae180 | free in retail | static helper; first arg dead in retail (void*) (exe_bodies.h:168) |
| `Pf_PastureTargetPick_53ca0` | 0x140053ca0 | member of PopulationComponent, not moved | first parameter is the object (PopulationComponent* population); retail symbol is a member (exe_bodies.h:170) |
| `Pf_RoadQuad_cc10` | 0x1400acc10 | member of WorldGeometry, not moved | first parameter is the object (WorldGeometry* geo); retail symbol is a member (exe_bodies.h:181) |
| `Pf_CellQuadRay_aa20` | 0x1400aaa20 | member of WorldGeometry, not moved | first parameter is the object (WorldGeometry* geo); retail symbol is a member (exe_bodies.h:182) |
| `Pf_CellQuadRayWrap_0f90` | 0x1400b0f90 | member of WorldGeometry, not moved | first parameter is the object (WorldGeometry* geo); retail symbol is a member (exe_bodies.h:183) |
| `InteractNodePosition` | 0x140122e20 | member of InteractComponent, not moved | first parameter is the object (InteractComponent* self); retail symbol is a member (exe_bodies.h:184) |
| `PickReachableCell` | 0x14004edb0 | member of Component, not moved | first parameter is the object (Component* target); retail symbol is a member (exe_bodies.h:185) |
| `EntityAnchorCell` | 0x140087fb0 | free in retail | static helper taking an Internal/External pointer slot first (not the object) (exe_bodies.h:186) |
| `Pf_EntityResolvePos_2e20` | 0x140122e20 | forwarder | kept spelling of InteractNodePosition (B4 FORWARD) (exe_bodies.h:187) |
| `Pf_EntityAnchorCell_7fb0` | 0x140087fb0 | forwarder | kept spelling of EntityAnchorCell (B4 FORWARD) (exe_bodies.h:188) |
| `DropRawMaterialOnGround` | 0x1400c78c0 | member of CitizenComponent, not moved | first parameter is the object (CitizenComponent* citizen); retail symbol is a member (exe_bodies.h:195) |
| `SpillAllStorage` | 0x1400c7570 | member of DropOffRawMaterial, not moved | first parameter is the object (DropOffRawMaterial* self); retail symbol is a member (exe_bodies.h:196) |
| `DropOffExec` | 0x1400c7620 | member of DropOffRawMaterial, not moved | first parameter is the object (DropOffRawMaterial* self); retail symbol is a member (exe_bodies.h:197) |
| `RemoveAnimalActions` | 0x1400bbea0 | free in retail | static helper taking an Internal/External pointer slot first (not the object) (exe_bodies.h:198) |
| `QueryNearestCharacter` | 0x140092260 | member of CharacterGrid, not moved | first arg = grid (this) (exe_bodies.h:205) |
| `HerdStartMigration` | 0x140122340 | member of HerdComponent, not moved | first parameter is the object (HerdComponent* herd); retail symbol is a member (exe_bodies.h:206) |
| `StorageCanAdd` | 0x14012cd80 | member of StorageComponent, not moved | first parameter is the object (StorageComponent* storage); retail symbol is a member (exe_bodies.h:211) |
| `FindBestAdjacentCell` | 0x14012d5d0 | member of Component, not moved | first parameter is the object (Component* target); retail symbol is a member (exe_bodies.h:212) |
| `PickBestDropTarget` | 0x14008d590 | member of CitizenBehavior, not moved | first parameter is the object (CitizenBehavior* lists); retail symbol is a member (exe_bodies.h:214) |
| `RoadTool_helper_6e30` | 0x1400a6e30 | member of PathfinderThread, not moved | first parameter is the object (PathfinderThread* pf); retail symbol is a member (exe_bodies.h:220) |
| `ZoneComponent_sub_d360` | 0x14009d360 | member of Map, not moved | first parameter is the object (Map* map); retail symbol is a member (exe_bodies.h:221) |
| `RoadTool_sub_ce00` | 0x14009ce00 | member of Map, not moved | first parameter is the object (Map* map); retail symbol is a member (exe_bodies.h:222) |
| `Pf_RegionPairCache_Build` | 0x14009e8c0 | harness / port-only | no retail address: tail of 0x14009e8c0 split out; caller Game/region_seed.cpp (exe_bodies.h:228) |
| `Pf_RegionPairCacheTick_5d10` | 0x1400a5d10 | member of RegionPairCache, not moved | first parameter is the object (RegionPairCache* cache); retail symbol is a member (exe_bodies.h:229) |

### Component (94)

| name | retail address | class | reason |
|---|---|---|---|
| `AmbientEnvironmentComponent_InitRegion` | 0x1400436a0 | member of AmbientEnvironmentComponent, not moved | first parameter is the object (AmbientEnvironmentComponent* self); retail symbol is a member (ambientenvironmentcomponent_funcs.h:16) |
| `ClearArea_Finalize8270` | 0x140118270 | member of ClearAreaComponent, not moved | first parameter is the object (ClearAreaComponent* comp); retail symbol is a member (clearareacomponent_funcs.h:9) |
| `ClearAreaCtrl_IsDone` | - | member of ClearAreaComponent, not moved | first parameter is the object (ClearAreaComponent* comp); retail symbol is a member (clearareacomponent_funcs.h:13) |
| `ClearAreaCtrl_BuildWork` | - | member of ClearArea, not moved | placement ctor: void* work is the raw storage the ClearArea is built in (clearareacomponent_funcs.h:14) |
| `DecalComponent_Serialize` | 0x14011b7d0 | member of DecalComponent, not moved | first parameter is the object (DecalComponent* self); retail symbol is a member (decalcomponent_funcs.h:9) |
| `DiseaseComp_CheckCure` | 0x1400481b0 | forwarder | 2 outside caller files (Dialog/debugui.cpp, standalone/disaster_harness.cpp (diseasecomponent_funcs.h:30) |
| `DiseaseComp_Infect` | 0x1400483a0 | member of DiseaseComponent, not moved | first parameter is the object (DiseaseComponent* self); retail symbol is a member (diseasecomponent_funcs.h:31) |
| `DiseaseComp_GrownIdentity` | 0x140047f80 | free in retail | static helper taking an Internal/External pointer slot first (not the object) (diseasecomponent_funcs.h:43) |
| `AmbientExternalPtrSerialize_Reimpl` | 0x1400019a0 | member of ExternalPtr<T>, not moved | per-type ExternalPtr<T>::Serialize instance, spelled free (exe_bodies.h:26) |
| `AmbientEmitterResize_Reimpl` | 0x140001770 | member of Collection::Array<T>, not moved | per-type Array<T> SetSize / Serialize / Clone / Destroy instance, spelled free (exe_bodies.h:30) |
| `AmbientArrayResize_Reimpl` | 0x140002540 | member of Collection::Array<T>, not moved | per-type Array<T> SetSize / Serialize / Clone / Destroy instance, spelled free (exe_bodies.h:34) |
| `AttendanceComponent_ReleasePartnerSlot` | 0x140115a50 | member of CitizenComponent, not moved | first parameter is the object (CitizenComponent* self); retail symbol is a member (exe_bodies.h:37) |
| `AttendanceComponent_ReleaseLink1650` | 0x140111650 | member of CitizenComponent, not moved | first parameter is the object (CitizenComponent* worker); retail symbol is a member (exe_bodies.h:38) |
| `AttendanceComponent_ReleaseLink59b0` | 0x1401159b0 | member of CitizenComponent, not moved | first parameter is the object (CitizenComponent* worker); retail symbol is a member (exe_bodies.h:39) |
| `AttendanceComponent_GetJobHostComponent` | 0x140002e10 | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:41) |
| `BuildComponent_FindInQueue` | 0x140017550 | member of CitizenPtrList<BuildComponent>, not moved | first parameter is the object (CitizenPtrList<BuildComponent>* list); retail symbol is a member (exe_bodies.h:44) |
| `CemeteryListErase_Reimpl` | 0x140002cc0 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:47) |
| `Character_UpdateAnimation_Reimpl` | 0x1400c4f80 | member of ActionStack, not moved | first parameter is the object (ActionStack* arrayBase); retail symbol is a member (exe_bodies.h:53) |
| `CitizenAssignCitizens` | 0x1400d43b0 | free in retail | scheduler callback; ctx (PathGraph) is dead, the body reads the GameUpdateLoop singleton (exe_bodies.h:56) |
| `CitizenEmploy_Strat_cbf0` | 0x14008cbf0 | member of CitizenBehavior, not moved | first parameter is the object (CitizenBehavior* townMgr); retail symbol is a member (exe_bodies.h:64) |
| `CitizenEmploy_Strat_ca20` | 0x14008ca20 | member of CitizenBehavior, not moved | first parameter is the object (CitizenBehavior* townMgr); retail symbol is a member (exe_bodies.h:65) |
| `CitizenEmploy_Strat_c5f0` | 0x14008c5f0 | member of CitizenBehavior, not moved | first parameter is the object (CitizenBehavior* townMgr); retail symbol is a member (exe_bodies.h:66) |
| `CitizenEmploy_Strat_c4b0` | 0x14008c4b0 | member of CitizenBehavior, not moved | first parameter is the object (CitizenBehavior* townMgr); retail symbol is a member (exe_bodies.h:67) |
| `CitizenList_AssignWorker_c3d0` | 0x14008c3d0 | member of CitizenBehavior, not moved | first parameter is the object (CitizenBehavior* townMgr); retail symbol is a member (exe_bodies.h:68) |
| `CitizenList_AddWorker_91130` | 0x140091130 | member of CitizenBehavior, not moved | first parameter is the object (CitizenBehavior* townMgr); retail symbol is a member (exe_bodies.h:69) |
| `TownRegistry_RegisterResidence` | 0x140129de0 | member of ComponentEntity, not moved | first parameter is the object (ComponentEntity* houseEntityIBP); retail symbol is a member (exe_bodies.h:70) |
| `CitizenEmploy_SeatWorkers` | 0x14007dd10 | harness / port-only | standalone harness / driver entry, no retail symbol (exe_bodies.h:71) |
| `PathGraph_ReassignWorkers` | 0x1400d62a0 | free in retail | scheduler callback; ctx (PathGraph) is dead, the body reads the GameUpdateLoop singleton (exe_bodies.h:80) |
| `Map_RadiusQuery_92990` | 0x140092990 | member of CharacterGrid, not moved | first arg = grid (this) (exe_bodies.h:84) |
| `CitizenComp_Infect` | 0x140116b90 | member of CitizenComponent, not moved | first parameter is the object (CitizenComponent* self); retail symbol is a member (exe_bodies.h:86) |
| `CitizenAI_PerformWorkCtor4_c1070` | 0x1400c1070 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:91) |
| `CitizenAI_PerformWorkCtor5_bd070` | 0x1400bd070 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:92) |
| `CitizenAI_PerformWorkCtor6_bd420` | 0x1400bd420 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:93) |
| `CitizenAI_PerformWorkCtor8_bd790` | 0x1400bd790 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:94) |
| `CitizenAI_PerformWorkCtor10_bccb0` | 0x1400bccb0 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:95) |
| `CitizenAI_PerformWorkCtor11_bc790` | 0x1400bc790 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:96) |
| `CitizenAI_Pathfinding_NavigateTo_bda50` | 0x1400bda50 | free in retail | static helper taking an Internal/External pointer slot first (not the object) (exe_bodies.h:97) |
| `CitizenAI_Pathfinding_WalkTowards_bb490` | 0x1400bb490 | free in retail | static helper taking an Internal/External pointer slot first (not the object) (exe_bodies.h:98) |
| `CitizenAI_PlayAnimationCtor4_ba6b0` | 0x1400ba6b0 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:99) |
| `CitizenAI_ActionExecutor_c4c50` | 0x1400c4c50 | member of ActionStack, not moved | first parameter is the object (ActionStack* param_1); retail symbol is a member (exe_bodies.h:103) |
| `ResidenceCheck_2ac30` | 0x14012ac30 | member of ResidenceComponent, not moved | first parameter is the object (ResidenceComponent* self); retail symbol is a member (exe_bodies.h:108) |
| `Entity_impl_ec70` | 0x14008ec70 | member of CitizenBehavior, not moved | first parameter is the object (CitizenBehavior* list); retail symbol is a member (exe_bodies.h:109) |
| `CitizenAI_helper_e370` | 0x14008e370 | member of CitizenBehavior, not moved | first parameter is the object (CitizenBehavior* list); retail symbol is a member (exe_bodies.h:110) |
| `CitizenAI_sub_78c0` | 0x1400178c0 | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:116) |
| `CitizenAI_FindBestTarget_dd30` | 0x14008dd30 | member of CitizenBehavior, not moved | first parameter is the object (CitizenBehavior* list); retail symbol is a member (exe_bodies.h:118) |
| `Storage_GetResourceCount2` | 0x14012ab00 | member of Component, not moved | first parameter is the object (Component* self); retail symbol is a member (exe_bodies.h:122) |
| `CitizenAI_Reg_helper_e360` | 0x14000e360 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:130) |
| `CitizenAI_WorkScheduler_Insert` | 0x1400f88a0 | member of WorkScheduler, not moved | first parameter is the object (WorkScheduler* sched); retail symbol is a member (exe_bodies.h:134) |
| `CitizenAI_WorkScheduler_Post` | 0x14012ec60 | member of WorkComponent, not moved | first parameter is the object (WorkComponent* workComp); retail symbol is a member (exe_bodies.h:135) |
| `CitizenAI_WorkScheduler_Rekey` | 0x1400f8400 | member of WorkScheduler, not moved | first parameter is the object (WorkScheduler* sched); retail symbol is a member (exe_bodies.h:136) |
| `CitizenComp_StorageComponent_QueryCount` | 0x14012cf10 | member of StorageComponent, not moved | first parameter is the object (StorageComponent* self); retail symbol is a member (exe_bodies.h:140) |
| `CitizenComp_Storage_GetResourceCount` | 0x14012a9d0 | member of Component, not moved | first parameter is the object (Component* self); retail symbol is a member (exe_bodies.h:141) |
| `CitizenComp_Entity_impl_ef40` | 0x14008ef40 | free in retail | static helper; param_1 unused in retail (exe_bodies.h:144) |
| `CitizenComp_SortByProfession_02a0` | 0x1400302a0 | free in retail | sort comparator / sort helper (exe_bodies.h:148) |
| `CitizenComp_DeferredInit_117080` | 0x140117080 | member of CitizenComponent, not moved | first parameter is the object (CitizenComponent* self); retail symbol is a member (exe_bodies.h:149) |
| `CropFieldComponent_ResizeEntityList` | 0x1400335b0 | member of Collection::Array<T>, not moved | per-type Array<T> SetSize / Serialize / Clone / Destroy instance, spelled free (exe_bodies.h:155) |
| `CropFieldComponent_SerializeEntityList` | 0x1400336c0 | member of Collection::Array<T>, not moved | per-type Array<T> SetSize / Serialize / Clone / Destroy instance, spelled free (exe_bodies.h:156) |
| `CropFieldComponent_Maturity` | 0x14011b410 | forwarder | -> CropFieldComponent::Maturity; a caller still uses the old spelling (exe_bodies.h:160) |
| `DecalComponent_TerrainDecalRemove` | 0x1400b0d00 | member of WorldGeometry, not moved | first parameter is the object (WorldGeometry* terrainDecalSys); retail symbol is a member (exe_bodies.h:164) |
| `FloorComponent_FindResData` | 0x140004680 | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:180) |
| `FrustumOutline_ProjectCorners` | 0x14006c5b0 | free in retail | static helper (scalar / vector first parameter, no this) (exe_bodies.h:183) |
| `MapTerrainClear_Reimpl` | 0x1400a4d30 | member of Minimap, not moved | first parameter is the object (Minimap* gridPtr); retail symbol is a member (exe_bodies.h:200) |
| `MapComponent_DetachOccupancy_4750` | 0x140124750 | member of MapComponent, not moved | first parameter is the object (MapComponent* self); retail symbol is a member (exe_bodies.h:202) |
| `NaturalResourceComponent_FindComponentByType` | 0x140005200 | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:207) |
| `Pasture_SpawnLivestock` | 0x140128670 | member of PastureComponent, not moved | first parameter is the object (PastureComponent* pasture); retail symbol is a member (exe_bodies.h:219) |
| `Pasture_FillAllPastures` | 0x140060290 | free in retail | static helper, no parameters (walks the pastures) (exe_bodies.h:220) |
| `StartTown_SpawnStartingHerd` | 0x1400a0b10 | member of MapGenerator, not moved | first parameter is the object (MapGenerator* sc); retail symbol is a member (exe_bodies.h:222) |
| `MapGrid_StampCellRefs_9f770` | 0x14009f770 | member of Map, not moved | first parameter is the object (Map* grid); retail symbol is a member (exe_bodies.h:226) |
| `StorageComponent_QueryAvailable` | 0x14012ce70 | forwarder | -> StorageComponent::QueryAvailable; 1 outside caller (Work/buildroad.cpp (exe_bodies.h:236) |
| `ToolbarDescription_StringArrayResize` | 0x140028860 | member of Collection::Array<T>, not moved | per-type Array<T> SetSize / Serialize / Clone / Destroy instance, spelled free (exe_bodies.h:241) |
| `WorkPlaceComponent_SetAccepting` | 0x14012fc00 | forwarder | -> WorkPlaceComponent::SetAccepting; a caller still uses the old spelling (exe_bodies.h:258) |
| `WorkPlaceComponent_LeaveBuilderProfession` | 0x14012f670 | forwarder | -> WorkPlaceComponent::LeaveBuilderProfession; a caller still uses the old spelling (exe_bodies.h:259) |
| `fn_StorageComponent_QueryCount` | 0x14012cf10 | forwarder | -> StorageComponent::QueryCount; callers in Component / Dialog -> StorageComponent::QueryCount // ---- storagecom (exe_bodies.h:276) |
| `ResolveExternalByName_Impl` | 0x14001b1d0 | free in retail | erased resolver over three ExternalRef record types (LEFT: needs a CONST ruling) (exe_bodies.h:302) |
| `CreateNaturalResource_helper_e2c0` | 0x14009e2c0 | member of Map, not moved | first parameter is the object (Map* map); retail symbol is a member (exe_bodies.h:305) |
| `FireComponent_Ignite_610` | 0x140049610 | member of FireComponent, not moved | first parameter is the object (FireComponent* self); retail symbol is a member (firecomponent_funcs.h:19) |
| `FireComp_StartAudioEmitter_42c60` | - | member of AmbientEmitterComponent, not moved | first parameter is the object (AmbientEmitterComponent* ambientComp); retail symbol is a member (firecomponent_funcs.h:23) |
| `FireComponent_IsBeingDemolished_11e690` | 0x14011e690 | forwarder | a caller still uses the old spelling (firecomponent_funcs.h:28) |
| `FireComponent_BeginBurning_11e7b0` | 0x14011e7b0 | member of DestroyComponent, not moved | first parameter is the object (DestroyComponent* destroyComp); retail symbol is a member (firecomponent_funcs.h:29) |
| `FireComponent_StopBurning_11ea10` | 0x14011ea10 | member of DestroyComponent, not moved | first parameter is the object (DestroyComponent* destroyComp); retail symbol is a member (firecomponent_funcs.h:31) |
| `MapComponent_Draw` | 0x140124370 | member of MapComponent, not moved | first parameter is the object (MapComponent* self); retail symbol is a member (mapcomponent_funcs.h:17) |
| `NaturalResourceComponent_OnRemove` | 0x140125c40 | member of NaturalResourceComponent, not moved | first parameter is the object (NaturalResourceComponent* self); retail symbol is a member (naturalresourcecomponent_funcs.h:9) |
| `ParticleComponent_StartEmitterFade` | 0x140053660 | forwarder | 2 outside callers // (diseasecomponent.cpp, firecomponent_ignite.cpp (particlecomponent_funcs.h:9) |
| `PickingComponent_RegisterMesh` | 0x140128f50 | forwarder | -> PickingComponent::RefreshMesh; callers in buildcomponent.cpp, buildcomponent_state8.cpp, // destroycomponent_st (pickingcomponent_funcs.h:13) |
| `PickingComponent_RegisterMeshForEntity` | - | member of ComponentEntity, not moved | first parameter is the object (ComponentEntity* entity); retail symbol is a member (pickingcomponent_funcs.h:18) |
| `RawMaterialComponent_Update` | 0x140129130 | member of RawMaterialComponent, not moved | first parameter is the object (RawMaterialComponent* self); retail symbol is a member (rawmaterialcomponent_funcs.h:15) |
| `SpawnComponent_InitialSpawn` | 0x140055cc0 | forwarder | -> SpawnComponent::InitialSpawn; 1 outside caller (Game/starttown.cpp (spawncomponent_funcs.h:12) |
| `SpawnComponent_ReportDoSpawnCensus` | - | harness / port-only | diagnostic print of the DoSpawn tallies, no retail counterpart (spawncomponent_funcs.h:15) |
| `TerraformComponent_RegionRecalc` | 0x1400ab150 | member of WorldGeometry, not moved | first parameter is the object (WorldGeometry* terrain); retail symbol is a member (terraformcomponent_funcs.h:12) |
| `TradeMerchantOrders_SetSize` | 0x14000e760 | member of Collection::Array<T>, not moved | per-type Array<T> SetSize / Serialize / Clone / Destroy instance, spelled free (tradecomponent_funcs.h:14) |
| `TradeMerchantOrders_Destroy` | 0x14000ef10 | member of Collection::Array<T>, not moved | per-type Array<T> SetSize / Serialize / Clone / Destroy instance, spelled free (tradecomponent_funcs.h:15) |
| `TradeBuyOrders_SetSize` | 0x14000e8d0 | member of Collection::Array<T>, not moved | per-type Array<T> SetSize / Serialize / Clone / Destroy instance, spelled free (tradecomponent_funcs.h:16) |
| `TradeResourceTree_NewNode` | 0x140059ad0 | member of ComponentDescription, not moved | first parameter is the object (ComponentDescription* key); retail symbol is a member (tradecomponent_funcs.h:20) |
| `TradeResourceTree_Retrace` | 0x14000e360 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (tradecomponent_funcs.h:22) |

### Dialog (25)

| name | retail address | class | reason |
|---|---|---|---|
| `DebugUI_Slot9` | 0x140060bc0 | forwarder | 1 outside caller(s (debugui_funcs.h:9) |
| `AutoSaveDialog_Ctor_Standalone` | 0x14005e230 | member of AutoSaveDialog, not moved | constructor on caller-supplied storage (first arg = raw memory / the object) (exe_bodies.h:36) |
| `EntityUI_SelectEntity_Impl` | - | member of PlayerToolManager, not moved | first parameter is the object (PlayerToolManager* playerController); retail symbol is a member (exe_bodies.h:52) |
| `EventNotifyDialog_Ctor_Standalone` | 0x140068ab0 | member of EventNotifyDialog, not moved | constructor on caller-supplied storage (first arg = raw memory / the object) (exe_bodies.h:57) |
| `ApplyKeyMapToUI_Impl` | 0x140099260 | member of GameContainer, not moved | first parameter is the object (GameContainer* keyMap); retail symbol is a member (exe_bodies.h:65) |
| `OptionsDialog_Ctor_Standalone` | 0x140075150 | forwarder | -> OptionsDialog; 1 outside caller (Dialog/mainmenudialog.cpp (exe_bodies.h:67) |
| `SaveLoadDialog_Ctor_8270` | 0x1400b8270 | member of SaveLoadDialog, not moved | constructor on caller-supplied storage (first arg = raw memory / the object) (exe_bodies.h:73) |
| `Avl38SetInsert` | 0x140014890 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:85) |
| `Toolbar_CenterOnEntity_Impl` | 0x140103040 | forwarder | -> PlayerToolManager::CenterOnEntity; 5 outside caller files -> PlayerToolManager::CenterOnEntity // ---- toolbardialo (exe_bodies.h:93) |
| `TownHall_QueryProductionStat` | 0x140090f20 | member of CitizenBehavior, not moved | first parameter is the object (CitizenBehavior* town); retail symbol is a member (exe_bodies.h:104) |
| `TownHall_InventorySortAsc` | 0x140015200 | free in retail | sort comparator / sort helper (exe_bodies.h:105) |
| `TownHall_InventorySortDesc` | 0x140015500 | free in retail | sort comparator / sort helper (exe_bodies.h:106) |
| `TownHall_RemoveWorker` | 0x140091180 | member of CitizenBehavior, not moved | first parameter is the object (CitizenBehavior* town); retail symbol is a member (exe_bodies.h:107) |
| `TradeUI_GetBuyEntries_59670` | 0x140059670 | member of TradeComponent, not moved | first parameter is the object (TradeComponent* ts); retail symbol is a member (exe_bodies.h:114) |
| `TradeDescription_ExtendMerchantStay` | 0x14005a490 | member of TradeComponent, not moved | first parameter is the object (TradeComponent* trade); retail symbol is a member (exe_bodies.h:117) |
| `TradeUI_OnTradeButton` | 0x140081420 | forwarder | -> TradeUI::OnTradeButton; 1 outside caller(s (exe_bodies.h:119) |
| `TradeUI_GetOrderName_15fd0` | 0x140015fd0 | member of TradeComponent, not moved | first parameter is the object (TradeComponent* tradeState); retail symbol is a member (exe_bodies.h:125) |
| `TradeUI_AddPurchaseOrder_59490` | 0x140059490 | member of TradeComponent, not moved | first parameter is the object (TradeComponent* tradeState); retail symbol is a member (exe_bodies.h:126) |
| `TradeUI_GetResourcePrice_a520` | 0x14005a520 | member of TradeComponent, not moved | first parameter is the object (TradeComponent* trade); retail symbol is a member (exe_bodies.h:131) |
| `NoiseUI_Slot4` | - | member of NoiseUI, not moved | first parameter is the object (NoiseUI* self); retail symbol is a member (noiseui_funcs.h:10) |
| `NoiseUI_Slot9` | - | member of NoiseUI, not moved | first parameter is the object (NoiseUI* self); retail symbol is a member (noiseui_funcs.h:14) |
| `TradeUI_UpdateBuyTotals` | - | forwarder | a caller still uses the old spelling (tradeui_funcs.h:11) |
| `TradeUI_UpdateSellTotals` | - | member of TradeUI, not moved | first parameter is the object (TradeUI* self); retail symbol is a member (tradeui_funcs.h:12) |
| `TradeUI_RefreshSellTable` | - | member of TradeUI, not moved | first parameter is the object (TradeUI* self); retail symbol is a member (tradeui_funcs.h:13) |
| `TradeUI_RefreshBuyTable` | - | member of TradeUI, not moved | first parameter is the object (TradeUI* self); retail symbol is a member (tradeui_funcs.h:14) |

### Entity (26)

| name | retail address | class | reason |
|---|---|---|---|
| `ComponentEntity_CtorFromDescription` | 0x1400c3720 | member of ComponentEntity, not moved | constructor on caller-supplied storage (first arg = raw memory / the object) (componententity_funcs.h:13) |
| `EntityList_RegisterEntity_Impl` | - | member of EntityList, not moved | first parameter is the object (EntityList* entityList); retail symbol is a member (componententity_funcs.h:21) |
| `Entity_FindComponentByExeTypeId` | - | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:56) |
| `fn_FindComponent_Build` | 0x14014f375 | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:70) |
| `fn_FindComponent_BuildState` | - | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:72) |
| `fn_FindComponent_ConsumeProduce` | - | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:74) |
| `fn_FindComponent_Position` | - | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:76) |
| `fn_FindComponent_Destroy` | - | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:78) |
| `fn_FindComponent_Building` | 0x140015140 | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:87) |
| `fn_FindComponent_Work` | 0x140087080 | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:96) |
| `fn_FindComponent_Attendance` | - | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:98) |
| `fn_FindComponent_Pasture` | - | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:100) |
| `fn_FindComponent_Orchard` | - | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:102) |
| `fn_FindComponent_WorkPlace` | - | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:107) |
| `fn_FindComponent_SM` | - | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:109) |
| `fn_FindComponent_Citizen` | - | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:111) |
| `fn_FindComponent_Storage` | - | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:113) |
| `fn_FindComponent_Storage` | - | forwarder | a caller still uses the old spelling (exe_bodies.h:114) |
| `fn_FindComponent_Cemetery` | - | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:116) |
| `fn_FindComponent_Immigration` | - | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:118) |
| `fn_FindComponent_Tracker` | - | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:120) |
| `fn_FindComponent_MeshInstance` | 0x14014fab5 | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:124) |
| `fn_FindComponent_Character` | - | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:128) |
| `fn_FindComponent_MultiObject` | - | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:131) |
| `fn_FindComponent_Fire` | - | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:134) |
| `fn_FindComponent_Unknown7ee8` | - | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:136) |

### Game (76)

| name | retail address | class | reason |
|---|---|---|---|
| `NameDB_GenerateName` | 0x1400a4b30 | member of NameDatabase, not moved | first arg is the String return slot; the db is the object (exe_bodies.h:39) |
| `NameDB_PickAndGenerate` | 0x1400a4c60 | member of NameDatabase, not moved | first arg is the String return slot; the db is the object (exe_bodies.h:40) |
| `EventLog_AddEvent` | 0x140095580 | forwarder | -> EventLog::AddEvent; a caller still uses the old spelling (exe_bodies.h:50) |
| `EventLog_AddCitizenEvent` | 0x140094e00 | forwarder | -> EventLog::AddCitizenEvent; a caller still uses the old spelling (exe_bodies.h:51) |
| `EventLog_YearString_Reg` | - | member of GameUpdateLoop, not moved | first parameter is the object (GameUpdateLoop* gameWorld); retail symbol is a member (exe_bodies.h:54) |
| `EventLog_SeasonString_Reg` | - | member of GameUpdateLoop, not moved | first parameter is the object (GameUpdateLoop* gameWorld); retail symbol is a member (exe_bodies.h:55) |
| `EventLog_TileIndex_Reg` | 0x140095030 | free in retail | static helper; RCX dead in retail (exe_bodies.h:56) |
| `Game_TriggerDisasterByType_Impl` | 0x140060110 | free in retail | static helper (scalar / vector first parameter, no this) (exe_bodies.h:63) |
| `Game_SpawnResources_Impl` | - | member of CitizenBehavior, not moved | first parameter is the object (CitizenBehavior* citizenList); retail symbol is a member (exe_bodies.h:83) |
| `Game_WeatherDtor` | 0x140093eb0 | member of WeatherSimulation, not moved | first parameter is the object (WeatherSimulation* self); retail symbol is a member (exe_bodies.h:87) |
| `GameSettings_Load` | 0x140098b60 | member of GameSettings, not moved | first parameter is the object (GameSettings* settings); retail symbol is a member (exe_bodies.h:90) |
| `GameSettings_Save` | 0x140098970 | member of GameSettings, not moved | first parameter is the object (GameSettings* settings); retail symbol is a member (exe_bodies.h:91) |
| `CalcWindFactor_Reg` | 0x1401027c0 | free in retail | static helper; the time argument is dead in retail (exe_bodies.h:102) |
| `BucketTree_Serialize_21ba0` | 0x140021ba0 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:109) |
| `EntityList_Shutdown_c3d30` | 0x1400c3d30 | member of EntityList, not moved | first parameter is the object (EntityList* self); retail symbol is a member (exe_bodies.h:113) |
| `EntityUI_Dtor_1024d0` | 0x1401024d0 | member of PlayerToolManager, not moved | first parameter is the object (PlayerToolManager* self); retail symbol is a member (exe_bodies.h:114) |
| `Tutorial_Dtor_f8ff0` | 0x1400f8ff0 | member of WorkScheduler, not moved | first parameter is the object (WorkScheduler* self); retail symbol is a member (exe_bodies.h:115) |
| `Reimpl_TradeWork_impl_04d0` | 0x1400b04d0 | forwarder | 1 outside caller(s (exe_bodies.h:120) |
| `Goals_Serialize_f9240` | 0x1400f9240 | forwarder | 1 outside caller(s (exe_bodies.h:125) |
| `Reimpl_MapGetTerrainPos` | 0x14009f3e0 | member of Map, not moved | first parameter is the object (Map* game); retail symbol is a member (exe_bodies.h:131) |
| `Minimap_PaintTerrain` | 0x1400a5140 | forwarder | 1 outside caller(s (exe_bodies.h:137) |
| `ProfessionList_Startup_Impl` | 0x1400a90a0 | forwarder | 1 outside caller(s (exe_bodies.h:141) |
| `Reimpl_Transform_SerializeWrapper` | 0x1400988c0 | forwarder | 1 outside caller(s (exe_bodies.h:145) |
| `Reimpl_Array_Resize_Stride08` | 0x140001bb0 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:146) |
| `Reimpl_Array_Serialize_Stride20` | 0x14001bbf0 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:150) |
| `Reimpl_Array_Serialize_External_GraphicsMesh` | 0x14001bcc0 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:151) |
| `Reimpl_Array_Serialize_External_Particle` | 0x14001bff0 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:152) |
| `ExternalPtrSerialize_GraphicsMesh` | 0x14001b8e0 | member of ExternalPtr<T>, not moved | per-type ExternalPtr<T>::Serialize instance, spelled free (exe_bodies.h:160) |
| `ExternalPtrSerialize_Weather` | 0x1400196a0 | member of ExternalPtr<T>, not moved | per-type ExternalPtr<T>::Serialize instance, spelled free (exe_bodies.h:161) |
| `ExternalPtrSerialize_StringTable` | 0x14000bf40 | member of ExternalPtr<T>, not moved | per-type ExternalPtr<T>::Serialize instance, spelled free (exe_bodies.h:162) |
| `ExternalPtrSerialize_Toolbar` | - | member of ExternalPtr<T>, not moved | per-type ExternalPtr<T>::Serialize instance, spelled free (exe_bodies.h:163) |
| `ExternalPtrSerialize_EnvironmentDesc` | 0x1400192a0 | member of ExternalPtr<T>, not moved | per-type ExternalPtr<T>::Serialize instance, spelled free (exe_bodies.h:164) |
| `ExternalPtrSerialize_Typed_7140` | 0x140007140 | member of ExternalPtr<T>, not moved | per-type ExternalPtr<T>::Serialize instance, spelled free (exe_bodies.h:165) |
| `ExternalPtrSerialize_MapGenerator` | 0x140019ec0 | member of ExternalPtr<T>, not moved | per-type ExternalPtr<T>::Serialize instance, spelled free (exe_bodies.h:166) |
| `ExternalPtrSerialize_TerrainGenerator` | 0x14001a0c0 | member of ExternalPtr<T>, not moved | per-type ExternalPtr<T>::Serialize instance, spelled free (exe_bodies.h:167) |
| `ExternalPtrSerialize_ClimateGenerator` | 0x1400194a0 | member of ExternalPtr<T>, not moved | per-type ExternalPtr<T>::Serialize instance, spelled free (exe_bodies.h:168) |
| `Reimpl_ExternalPtrArray_Serialize` | 0x1400049c0 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:191) |
| `Reimpl_TradeWork_helper_e270` | 0x1400ae270 | forwarder | 1 outside caller(s (exe_bodies.h:197) |
| `Reimpl_Array_Resize_Stride02` | 0x14001c890 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:200) |
| `Reimpl_Array_Resize_Stride16B` | 0x14001ef10 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:201) |
| `Reimpl_Array_Resize_Stride04` | 0x1400021d0 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:202) |
| `Reimpl_Array_Resize_Byte` | 0x14000a590 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:203) |
| `TownRegistry_RegisterStorageLocation` | 0x14012d230 | member of Component, not moved | first parameter is the object (Component* storageLocationComp); retail symbol is a member (exe_bodies.h:211) |
| `TownStorage_BuildCompleteInit_7ff0` | 0x140107ff0 | member of Component, not moved | first parameter is the object (Component* componentWithEntity); retail symbol is a member (exe_bodies.h:212) |
| `StartTown_SeedStartingResources` | 0x1400a1f00 | member of MapGenerator, not moved | first parameter is the object (MapGenerator* startConfig); retail symbol is a member (exe_bodies.h:213) |
| `ResidenceComponent_impl_95a0` | 0x1400895a0 | member of CitizenBehavior, not moved | first parameter is the object (CitizenBehavior* town); retail symbol is a member (exe_bodies.h:217) |
| `TownRegistry_ShuffleProfessionList` | 0x140089470 | member of CitizenBehavior, not moved | first parameter is the object (CitizenBehavior* townMgr); retail symbol is a member (exe_bodies.h:218) |
| `FindAvailableBuilding` | 0x14008d100 | member of CitizenBehavior, not moved | first parameter is the object (CitizenBehavior* townMgr); retail symbol is a member (exe_bodies.h:219) |
| `TownRegistry_RegisterWorkplace` | - | member of CitizenBehavior, not moved | first parameter is the object (CitizenBehavior* townMgr); retail symbol is a member (exe_bodies.h:221) |
| `MusicPlayer_Update` | 0x1400a40e0 | forwarder | -> MusicPlayer::Update; 1 outside caller: gameupdateloop.cpp (exe_bodies.h:225) |
| `WeatherParticle_GeoCtor` | 0x1400d3f10 | member of void, not moved | constructor on caller-supplied storage (first arg = raw memory / the object) (exe_bodies.h:230) |
| `WeatherParticle_Config` | 0x1400d3f80 | member of WeatherParticleGeometry, not moved | first parameter is the object (WeatherParticleGeometry* geo); retail symbol is a member (exe_bodies.h:233) |
| `Weather_ResetTime_Reimpl` | 0x1400944e0 | forwarder | -> WeatherSimulation::Update; a caller still uses the old spelling (exe_bodies.h:238) |
| `fn_Random_RandFloatTol` | - | free in retail | static helper / callback; first arg is a System::Random (not an owned object) (exe_bodies.h:246) |
| `WorldSetup_LoadFromSave` | 0x1400b49f0 | member of MainMenuDialog, not moved | first parameter is the object (MainMenuDialog* worldSetup); retail symbol is a member (exe_bodies.h:250) |
| `SetEngineNextLoop` | 0x140097fb0 | member of GameUpdateLoop, not moved | first parameter is the object (GameUpdateLoop* game); retail symbol is a member (exe_bodies.h:265) |
| `SetDialogLoaderNextLoop` | 0x14009a8f0 | member of MenuDialogLoopT, not moved | first parameter is the object (MenuDialogLoopT* menuLoop); retail symbol is a member (exe_bodies.h:266) |
| `ResizeStride0x10` | 0x140017640 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:295) |
| `Reimpl_Array_Resize_Stride16A` | - | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:303) |
| `Reimpl_Array_Resize_ArrayOfArrays` | - | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:305) |
| `MenuDialogLoopT_LoadScene_PhaseA` | 0x14009A930 | member of MenuDialogLoopT, not moved | first parameter is the object (MenuDialogLoopT* self); retail symbol is a member (menudialogloopt_funcs.h:41) |
| `MenuDialogLoopT_LoadSceneA1` | - | member of MenuDialogLoopT, not moved | first parameter is the object (MenuDialogLoopT* self); retail symbol is a member (menudialogloopt_funcs.h:53) |
| `MenuDialogLoopT_LoadSceneA3` | - | member of MenuDialogLoopT, not moved | first parameter is the object (MenuDialogLoopT* self); retail symbol is a member (menudialogloopt_funcs.h:58) |
| `MenuDialogLoopT_LoadStaticMeshes_PhaseB` | 0x14009A930 | member of MenuDialogLoopT, not moved | first parameter is the object (MenuDialogLoopT* self); retail symbol is a member (menudialogloopt_funcs.h:73) |
| `MenuDialogLoopT_PhaseB_InitState` | - | member of MenuDialogLoopT_PhaseBState, not moved | first parameter is the object (MenuDialogLoopT_PhaseBState* state); retail symbol is a member (menudialogloopt_funcs.h:97) |
| `MenuDialogLoopT_PhaseB_TickChunk` | - | member of MenuDialogLoopT_PhaseBState, not moved | first parameter is the object (MenuDialogLoopT_PhaseBState* state); retail symbol is a member (menudialogloopt_funcs.h:104) |
| `MenuDialogLoopT_LoadParticles_PhaseC` | 0x14009A930 | member of MenuDialogLoopT, not moved | first parameter is the object (MenuDialogLoopT* self); retail symbol is a member (menudialogloopt_funcs.h:118) |
| `MenuDialogLoopT_TickParticles` | - | member of MenuDialogLoopT, not moved | first parameter is the object (MenuDialogLoopT* self); retail symbol is a member (menudialogloopt_funcs.h:124) |
| `MenuDialogLoopT_DrawScene_PhaseA` | 0x14009b440 | member of MenuDialogLoopT, not moved | first parameter is the object (MenuDialogLoopT* self); retail symbol is a member (menudialogloopt_funcs.h:133) |
| `MenuDialogLoopT_TickAudio` | 0x1800dd3c0 | free in retail | static helper / callback; first arg is a System::Time (not an owned object) (menudialogloopt_funcs.h:158) |
| `MenuDialogLoopT_LoadMusic` | 0x14009a930 | member of MenuDialogLoopT, not moved | first parameter is the object (MenuDialogLoopT* self); retail symbol is a member (menudialogloopt_funcs.h:176) |
| `MenuDialogLoopT_StartMusic` | 0x14009b930 | member of MenuDialogLoopT, not moved | first parameter is the object (MenuDialogLoopT* self); retail symbol is a member (menudialogloopt_funcs.h:187) |
| `MenuDialogLoopT_StopMusic` | 0x14009b280 | member of MenuDialogLoopT, not moved | first parameter is the object (MenuDialogLoopT* self); retail symbol is a member (menudialogloopt_funcs.h:193) |
| `MenuDialogLoopT_BindUISounds` | 0x140099260 | member of MenuDialogLoopT, not moved | first parameter is the object (MenuDialogLoopT* self); retail symbol is a member (menudialogloopt_funcs.h:209) |
| `MenuDialogLoopT_TickAudioScene` | 0x14009b6e0 | member of MenuDialogLoopT, not moved | first parameter is the object (MenuDialogLoopT* self); retail symbol is a member (menudialogloopt_funcs.h:222) |
| `MenuDialogLoopT_UpdateCamera_PhaseA` | 0x14009b6e0 | member of MenuDialogLoopT, not moved | first parameter is the object (MenuDialogLoopT* self); retail symbol is a member (menudialogloopt_funcs.h:238) |

### Player (51)

| name | retail address | class | reason |
|---|---|---|---|
| `EditTool_CloseInfoDialog` | 0x1400fcc90 | member of EditTool, not moved | first parameter is the object (EditTool* self); retail symbol is a member (edittool_funcs.h:14) |
| `EditTool_IsEditTool` | - | harness / port-only | the harnesses' EditTool probe (comment: no retail address, standalone drivers) (edittool_funcs.h:19) |
| `EditTool_HarnessAcquire` | - | harness / port-only | standalone harness / driver entry, no retail symbol (edittool_funcs.h:20) |
| `EditTool_HarnessSelectEntity` | - | harness / port-only | standalone harness / driver entry, no retail symbol (edittool_funcs.h:21) |
| `EditTool_HarnessSelectCitizen` | - | harness / port-only | standalone harness / driver entry, no retail symbol (edittool_funcs.h:22) |
| `EditTool_HarnessSelectNearby` | - | harness / port-only | standalone harness / driver entry, no retail symbol (edittool_funcs.h:23) |
| `PlacementTool_Factory_SA` | - | harness / port-only | standalone harness / driver entry, no retail symbol (edittool_funcs.h:28) |
| `RemovalTool_Factory_SA` | - | harness / port-only | standalone harness / driver entry, no retail symbol (edittool_funcs.h:30) |
| `BridgeTool_SnapEndCell_Impl` | 0x1400fb710 | member of BridgeTool, not moved | first parameter is the object (BridgeTool* self); retail symbol is a member (exe_bodies.h:40) |
| `BridgeTool_ValidatePath_Impl` | 0x1400fb2d0 | member of BridgeTool, not moved | first parameter is the object (BridgeTool* self); retail symbol is a member (exe_bodies.h:41) |
| `RemovalCalc_StateMachine_dfa0` | 0x14011dfa0 | forwarder | -> DestroyComponent::RunStateMachine; a caller still uses the old spelling (exe_bodies.h:45) |
| `RemovalCalc_helper_c700` | 0x14009c700 | member of Map, not moved | first parameter is the object (Map* param_1); retail symbol is a member (exe_bodies.h:46) |
| `UpdateCreateBar` | 0x1400ffc60 | member of PlayerToolManager, not moved | first parameter is the object (PlayerToolManager* owner); retail symbol is a member (exe_bodies.h:49) |
| `UpdateToolUI_ebb0` | 0x1400febb0 | free in retail | static helper; first arg dead in retail (const void* /*never read*/) (exe_bodies.h:50) |
| `DragTool_FindDescription_5160` | 0x140005160 | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:53) |
| `DragTool_CloseToolDialog_ff980` | 0x1400ff980 | member of PlayerToolManager, not moved | first parameter is the object (PlayerToolManager* owner); retail symbol is a member (exe_bodies.h:54) |
| `DragTool_PlayTickSound_ffbf0` | 0x1400ffbf0 | member of PlayerToolManager, not moved | first parameter is the object (PlayerToolManager* owner); retail symbol is a member (exe_bodies.h:55) |
| `DragTool_PlayPlaceSound_1000f0` | 0x1401000f0 | free in retail | static helper; first arg dead in retail (const void* /*unused*/) (exe_bodies.h:56) |
| `DragTool_PlayErrorSound_100140` | 0x140100140 | free in retail | static helper; first arg dead in retail (const void* /*unused*/) (exe_bodies.h:57) |
| `DragPlacementTool_ClearPathEntities_1018d0` | 0x1401018d0 | free in retail | static helper; first arg dead in retail (const void* /*unused*/) (exe_bodies.h:58) |
| `PlayerEdit_EntityAtCell_a0280` | 0x1400a0280 | member of Map, not moved | first parameter is the object (Map* map); retail symbol is a member (exe_bodies.h:61) |
| `PlayerEdit_AvgTerrainHeight_101730` | 0x140101730 | free in retail | static helper; first arg dead in retail (void*) (exe_bodies.h:62) |
| `PlayerEdit_FencedAnchorCalc_dbf90` | 0x1400dbf90 | member of FencedTool, not moved | first parameter is the object (FencedTool* self); retail symbol is a member (exe_bodies.h:63) |
| `DecalVertexSetInsert` | 0x14001f6b0 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:92) |
| `DecalVertexSetFind` | 0x14001fc00 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:93) |
| `AddDecal` | 0x1400b0d80 | member of WorldGeometry, not moved | first parameter is the object (WorldGeometry* geom); retail symbol is a member (exe_bodies.h:96) |
| `Player_HighlightAdd_55430` | 0x140055430 | member of RadiusDescription, not moved | first parameter is the object (RadiusDescription* param_1); retail symbol is a member (exe_bodies.h:101) |
| `TradeWork_impl_ace40` | 0x1400ace40 | member of WorldGeometry, not moved | first parameter is the object (WorldGeometry* geometry); retail symbol is a member (exe_bodies.h:104) |
| `PlayerPath_CharComp_f5d0` | 0x14009f5d0 | member of Map, not moved | first parameter is the object (Map* param_1); retail symbol is a member (exe_bodies.h:107) |
| `PlayerRay_TerrainMarch_aa530` | 0x1400aa530 | member of WorldGeometry, not moved | first parameter is the object (WorldGeometry* param_1); retail symbol is a member (exe_bodies.h:114) |
| `PlayerRay_GetMouseRay_1005a0` | 0x1401005a0 | free in retail | static helper; first arg dead in retail (const void* /*never read*/) (exe_bodies.h:115) |
| `PlayerRay_ScreenToCell_101270` | 0x140101270 | free in retail | static helper; first arg dead in retail (const void* /*never read*/) (exe_bodies.h:116) |
| `HighlightSelect_Impl` | 0x1400fc9e0 | member of EditTool, not moved | first parameter is the object (EditTool* self); retail symbol is a member (exe_bodies.h:121) |
| `HighlightClear_Impl` | 0x1400fcb40 | member of EditTool, not moved | first parameter is the object (EditTool* self); retail symbol is a member (exe_bodies.h:122) |
| `HighlightRepaint_Impl` | 0x14004c4a0 | member of HighlightComponent, not moved | first parameter is the object (HighlightComponent* self); retail symbol is a member (exe_bodies.h:123) |
| `PlayerScreenRect_BuildCorners_ec70` | 0x1400fec70 | free in retail | static helper; first arg dead in retail (uint64_t /*never read*/) (exe_bodies.h:126) |
| `PlayerScreenRect_WorldToScreen_098210` | 0x140098210 | member of PlayerController, not moved | first parameter is the object (PlayerController* param_1); retail symbol is a member (exe_bodies.h:127) |
| `PlayerScreenRect_AccumulateRect_fe960` | 0x1400fe960 | free in retail | static helper; first arg dead in retail (uint64_t /*never read*/) (exe_bodies.h:128) |
| `PlayerScreenRect_EntityRect_0380` | 0x140100380 | free in retail | static helper; first arg dead in retail (const void* /*never read*/) (exe_bodies.h:129) |
| `ToolTile_FootprintValidity_d580` | 0x14009d580 | member of Map, not moved | first parameter is the object (Map* param_1); retail symbol is a member (exe_bodies.h:132) |
| `PlayerTileRender_DrawOverlay_08c0` | 0x1401008c0 | free in retail | static helper; first arg dead in retail (const void* /*never read*/) (exe_bodies.h:136) |
| `PlayerTileRender_BuildRecords_1380` | 0x140101380 | free in retail | static helper; first arg dead in retail (const void* /*never read*/) (exe_bodies.h:137) |
| `ToolUI_OpenToolDialog_ff9e0` | 0x1400ff9e0 | member of PlayerToolManager, not moved | first parameter is the object (PlayerToolManager* owner); retail symbol is a member (exe_bodies.h:140) |
| `Avl38SetRemove` | 0x140025740 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:146) |
| `Avl38SetRemove` | 0x140025740 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:147) |
| `Avl38SetFindRemove` | 0x14002c690 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:148) |
| `Avl38SetFindRemove` | 0x14002c690 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:151) |
| `RoadAStar` | 0x14009ce00 | member of Map, not moved | first parameter is the object (Map* map); retail symbol is a member (roadtool_funcs.h:15) |
| `RoadTool_FindMapDescription_6990` | 0x140026990 | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (roadtool_funcs.h:17) |
| `RoadTool_DecalStore_acd90` | 0x1400acd90 | member of WorldGeometry, not moved | first parameter is the object (WorldGeometry* mgr); retail symbol is a member (roadtool_funcs.h:19) |
| `RoadTool_PlaceRoadSegment_9c960` | - | member of Map, not moved | first parameter is the object (Map* mapGrid); retail symbol is a member (roadtool_funcs.h:20) |

### Tutorial (10)

| name | retail address | class | reason |
|---|---|---|---|
| `FindUIElementByPath_Impl` | 0x1400e1870 | member of Object::InternalPtr<UI::Element>, not moved | first parameter is the object (Object::InternalPtr<UI::Element>* out); retail symbol is a member (exe_bodies.h:29) |
| `TutBase_SerializeStepArray` | 0x140026fb0 | member of Collection::Array<T>, not moved | per-type Array<T> SetSize / Serialize / Clone / Destroy instance, spelled free (exe_bodies.h:37) |
| `TutBase_SerializeImageArray` | 0x140028cf0 | member of Collection::Array<T>, not moved | per-type Array<T> SetSize / Serialize / Clone / Destroy instance, spelled free (exe_bodies.h:38) |
| `TutBase_SerializeSetupArray` | 0x140028f40 | member of Collection::Array<T>, not moved | per-type Array<T> SetSize / Serialize / Clone / Destroy instance, spelled free (exe_bodies.h:39) |
| `TutBase_SerializeGoalArray` | 0x140029190 | member of Collection::Array<T>, not moved | per-type Array<T> SetSize / Serialize / Clone / Destroy instance, spelled free (exe_bodies.h:40) |
| `TutBase_SerializeImageRef` | 0x140027ad0 | member of ExternalPtr<T>, not moved | per-type ExternalPtr<T>::Serialize instance, spelled free (exe_bodies.h:41) |
| `WorkComponent_CalcMaxWorkers` | 0x14012e5c0 | member of WorkComponent, not moved | first parameter is the object (WorkComponent* workComp); retail symbol is a member (exe_bodies.h:43) |
| `TutWave_SerializeTutorialRef` | 0x1400262b0 | member of ExternalPtr<T>, not moved | per-type ExternalPtr<T>::Serialize instance, spelled free (exe_bodies.h:48) |
| `TutWave_SerializeDialogRef` | 0x140028650 | member of ExternalPtr<T>, not moved | per-type ExternalPtr<T>::Serialize instance, spelled free (exe_bodies.h:50) |
| `TutWave_TutorialTreeInsert` | 0x1401001f0 | member of PlayerToolManager, not moved | first parameter is the object (PlayerToolManager* treeOwner); retail symbol is a member (exe_bodies.h:54) |

### Work (61)

| name | retail address | class | reason |
|---|---|---|---|
| `BuildRoad_CtorParams_Impl` | 0x1400e4b50 | member of BuildRoad, not moved | constructor on caller-supplied storage (first arg = raw memory / the object) (buildroad_funcs.h:14) |
| `BuildStructure_SetupBuildActions` | 0x1400c0690 | free in retail | static helper taking an Internal/External pointer slot first (not the object) (exe_bodies.h:23) |
| `BuildStructure_CreateInPlaceFull` | 0x1400e59c0 | member of BuildStructure, not moved | ctor on caller-supplied storage (void* mem) (exe_bodies.h:29) |
| `BuildStructure_ConstructForSite` | - | harness / port-only | standalone harness / driver entry, no retail symbol (exe_bodies.h:30) |
| `BuildStructure_ForceDrive` | - | harness / port-only | standalone harness / driver entry, no retail symbol (exe_bodies.h:31) |
| `BuildStructure_DepsReady` | - | harness / port-only | standalone harness / driver entry, no retail symbol (exe_bodies.h:32) |
| `ForesterWork_ExecuteReady` | - | harness / port-only | standalone harness / driver entry, no retail symbol (exe_bodies.h:35) |
| `ForesterWork_ForceDrive` | - | harness / port-only | standalone harness / driver entry, no retail symbol (exe_bodies.h:36) |
| `GatherRawMaterials_ConstructForSite` | - | harness / port-only | standalone harness / driver entry, no retail symbol (exe_bodies.h:39) |
| `GatherRawMaterials_ForceDrive` | - | harness / port-only | standalone harness / driver entry, no retail symbol (exe_bodies.h:40) |
| `GatherRawMaterials_DepsReady` | - | harness / port-only | standalone harness / driver entry, no retail symbol (exe_bodies.h:41) |
| `GatherRawMaterials_Ctor_Impl` | 0x1400ec710 | member of Work, not moved | constructor on caller-supplied storage (first arg = raw memory / the object) (exe_bodies.h:45) |
| `HarvestOrchard_CtorFromEntity` | - | harness / port-only | port-only constructor, no retail symbol; reached by a local extern from a *states.cpp (exe_bodies.h:48) |
| `PastureWork_CreateForEntity` | - | harness / port-only | port-only constructor, no retail symbol; reached by a local extern from a *states.cpp (exe_bodies.h:51) |
| `PlantOrchard_CtorFromEntity` | - | harness / port-only | port-only constructor, no retail symbol; reached by a local extern from a *states.cpp (exe_bodies.h:55) |
| `TendArea_CtorFromEntity` | - | harness / port-only | port-only constructor, no retail symbol; reached by a local extern from a *states.cpp (exe_bodies.h:58) |
| `WorkAct_HarvestAreaCtor_c1530` | 0x1400c1530 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:62) |
| `WorkAct_ConsumeProduceCtor_bef70` | 0x1400bef70 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:65) |
| `WorkAct_HuntWorkCtor_bbbc0` | 0x1400bbbc0 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:69) |
| `WorkAct_ShowToolCtor3_bdd20` | 0x1400bdd20 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:71) |
| `WorkAct_GatherRawMatCtor_bf790` | 0x1400bf790 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:76) |
| `WorkAct_PerformWorkCopyCtor_ba9f0` | 0x1400ba9f0 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:79) |
| `WorkAct_PerformWorkCtor_bffd0` | 0x1400bffd0 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:80) |
| `WorkAct_GatherWorkCtor_bb7b0` | 0x1400bb7b0 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:83) |
| `WorkAct_FishWorkCtor_beae0` | 0x1400beae0 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:84) |
| `WorkAcc_FindComponent_6330` | 0x140006330 | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:88) |
| `WorkAct_PerformWorkCtor_c1eb0` | 0x1400c1eb0 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:92) |
| `WorkAct_PickupRawMaterialCopyCtor_c9b70` | 0x1400c9b70 | member of PickupRawMaterial, not moved | constructor on caller-supplied storage (first arg = raw memory / the object) (exe_bodies.h:94) |
| `WorkAct_PerformWorkCtor9_c0ad0` | 0x1400c0ad0 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:95) |
| `WorkAct_PlayAnimationCtor_c2490` | 0x1400c2490 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:96) |
| `WorkAct_GrazingWorkCtor_bb0a0` | 0x1400bb0a0 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:97) |
| `WorkAct_PlantWorkCtor_be6e0` | 0x1400be6e0 | free in retail | action factory: builds the Action and pushes it (first arg is the citizen ref, not the object) (exe_bodies.h:98) |
| `WorkBST_RegisterWork_1a30` | 0x140021a30 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:101) |
| `WorkBST_BSTRemove_a6e0` | 0x14001a6e0 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:102) |
| `WorkBST_BSTFindRemove_25520` | 0x140025520 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:103) |
| `WorkBST_BST_Serialize_296b0` | 0x1400296b0 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:105) |
| `WorkCellBST_Insert_9a40` | 0x140029a40 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:140) |
| `WorkCellBST_Remove_ca60` | 0x14001ca60 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:141) |
| `WorkCellBST_Serialize_9de0` | 0x140029de0 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:143) |
| `WorkGrid_FindCell_9ccf0` | 0x14009ccf0 | member of Map, not moved | first parameter is the object (Map* map); retail symbol is a member (exe_bodies.h:181) |
| `WorkGrid_TryAssignWorkplace_f8620` | 0x1400f8620 | member of WorkScheduler, not moved | first parameter is the object (WorkScheduler* mgr); retail symbol is a member (exe_bodies.h:183) |
| `WorkImpl_Sqrtf_132010` | 0x140132010 | free in retail | static helper; first arg dead in retail (float param_1) (exe_bodies.h:186) |
| `WorkBase_TreeNodeFree_14c30` | 0x140014c30 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:189) |
| `WorkBase_TreeNodeFree_e320` | 0x14000e320 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:190) |
| `WorkBase_TaskListCleanup_c5400` | 0x1400c5400 | member of Collection::Array<T>, not moved | per-type Array<T> SetSize / Serialize / Clone / Destroy instance, spelled free (exe_bodies.h:192) |
| `WorkBase_CalcDistance_8830` | 0x140008830 | member of Map, not moved | first parameter is the object (Map* map); retail symbol is a member (exe_bodies.h:193) |
| `WorkBase_ValidateCell_a00f0` | 0x1400a00f0 | member of Map, not moved | first parameter is the object (Map* map); retail symbol is a member (exe_bodies.h:194) |
| `WorkImpl_WalkablePos_122be0` | 0x140122be0 | member of InteractComponent, not moved | first parameter is the object (InteractComponent* interact); retail symbol is a member (exe_bodies.h:199) |
| `WorkImpl_WalkablePosRecursive_122b00` | 0x140122b00 | member of InteractComponent, not moved | first parameter is the object (InteractComponent* interact); retail symbol is a member (exe_bodies.h:200) |
| `WorkLeaf_PlantArea_b390` | 0x14011b390 | member of CropFieldComponent, not moved | first parameter is the object (CropFieldComponent* crop); retail symbol is a member (exe_bodies.h:204) |
| `WorkLeaf_BstRemoveByIBP_298a0` | 0x1400298a0 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:206) |
| `WorkLeaf_BstInsertByIBP_29920` | 0x140029920 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:207) |
| `WorkLeaf_BstSerializeIBPpair_29c10` | 0x140029c10 | erased core | one shared type-erased body (AVL / array / list / FindComponent core) under typed wrappers (exe_bodies.h:208) |
| `WorkLeaf_SortTilesByDistance_9d7c0` | 0x14009d7c0 | free in retail | sort comparator / sort helper (exe_bodies.h:211) |
| `WorkLeaf_GetTilesInArea_9fc00` | 0x14009fc00 | member of Map, not moved | first parameter is the object (Map* map); retail symbol is a member (exe_bodies.h:212) |
| `WorkLeaf_FreeStorageCapacity_116540` | 0x140116540 | member of Component, not moved | first parameter is the object (Component* owner); retail symbol is a member (exe_bodies.h:216) |
| `WorkLeaf_FindGrowthDescription_c3c0` | 0x14000c3c0 | free in retail | per-type FindComponent / FindDescription instance (one retail function per type, no this) (exe_bodies.h:218) |
| `WorkLeaf_FootprintOwnerCheck_120c30` | 0x140120c30 | member of GrowthDescription, not moved | first parameter is the object (GrowthDescription* growth); retail symbol is a member (exe_bodies.h:219) |
| `WorkLeaf_ConsumeProduce_93a0` | 0x1401193a0 | member of ConsumeProduceComponent, not moved | first parameter is the object (ConsumeProduceComponent* comp); retail symbol is a member (exe_bodies.h:225) |
| `WorkLeaf_Pasture_8350` | 0x140128350 | member of PastureComponent, not moved | first parameter is the object (PastureComponent* pasture); retail symbol is a member (exe_bodies.h:226) |
| `WorkLeaf_ConsumeProduce_b310` | 0x14012b310 | member of ResourceProduceComponent, not moved | first parameter is the object (ResourceProduceComponent* producer); retail symbol is a member (exe_bodies.h:227) |

### Count per reason (section A, by declaration)

| reason | count |
|---|---|
| member of <Class>, not moved | 234 |
| free in retail | 89 |
| erased core | 37 |
| forwarder | 29 |
| harness / port-only | 22 |
| port gap helper | 0 |
| **total** | **411** |

## B. code/Game functions with a `void*` first parameter

One row per name (the definition when there is one; `+N` = further declarations / extern copies of the same name). "In A" = declared in an exe_bodies.h / *_funcs.h header (section A).

### B1. Declared in section A (allowed by the rule)

| name | file:line | signature | what the void* is | in A |
|---|---|---|---|---|
| `AutoSaveDialog_Ctor_Standalone` | Dialog/autosavedialog.cpp:257 +1 | `extern "C" AutoSaveDialog* AutoSaveDialog_Ctor_Standalone(void* self, UI::UserInterface* ui, const UI::Dialog*` | raw storage the AutoSaveDialog is constructed in | yes |
| `BuildStructure_CreateInPlaceFull` | Work/buildstructure.cpp:786 +1 | `extern "C" BuildStructure* BuildStructure_CreateInPlaceFull(void* mem, ComponentEntity** entityIBP, bool altAn` | raw storage the BuildStructure is placed in | yes |
| `CalcWindFactor_Reg` | Game/gameupdateloop.cpp:2178 +1 | `extern "C" float CalcWindFactor_Reg(void* time)` | dead time argument | yes |
| `CemeteryListErase_Reimpl` | Component/cemeterycomponent.cpp:146 +2 | `extern "C" void CemeteryListErase_Reimpl(void* listBase, void* ptrValue)` | first word = list header {data ptr, count}; erased over every 8-byte-pointer list | yes |
| `CitizenAssignCitizens` | Component/citizen_assign_citizens.cpp:297 +1 | `extern "C" void CitizenAssignCitizens(void* ctxV)` | ctx = PathGraph, dead in the body | yes |
| `CitizenComp_Entity_impl_ef40` | Component/citizencomponent.cpp:1817 +1 | `ComponentDescription** CitizenComp_Entity_impl_ef40(const void* param_1, ComponentDescription** param_2, Compo` | param_1 unused in retail | yes |
| `ClearAreaCtrl_BuildWork` | Component/clearareacomponent_states.cpp:543 +1 | `extern "C" ClearArea* ClearAreaCtrl_BuildWork(void* work, ComponentEntity* ownerEnt, ComponentDescription* des` | raw storage the ClearArea is placed in | yes |
| `DragPlacementTool_ClearPathEntities_1018d0` | Player/tool_base_drag.cpp:149 +1 | `void DragPlacementTool_ClearPathEntities_1018d0(const void* /*unused*/, int minCol, int minRow, int maxCol, in` | unused first arg (retail static helper) | yes |
| `DragTool_PlayErrorSound_100140` | Player/tool_base_drag.cpp:130 +1 | `void DragTool_PlayErrorSound_100140(const void* /*unused*/)` | unused first arg | yes |
| `DragTool_PlayPlaceSound_1000f0` | Player/tool_base_drag.cpp:118 +1 | `void DragTool_PlayPlaceSound_1000f0(const void* /*unused*/)` | unused first arg | yes |
| `EditTool_IsEditTool` | Player/edittool.cpp:685 +2 | `extern "C" bool EditTool_IsEditTool(void* tool)` | any Object::Base* under a type-id test | yes |
| `EventLog_TileIndex_Reg` | Game/eventlog.cpp:198 +1 | `extern "C" int EventLog_TileIndex_Reg(void* /*unused*/, const ::Object::InternalBasePtr* entityIBP)` | RCX dead in retail | yes |
| `EventNotifyDialog_Ctor_Standalone` | Dialog/eventnotifydialog.cpp:627 +1 | `extern "C" EventNotifyDialog* EventNotifyDialog_Ctor_Standalone(void* self, UI::UserInterface* ui, const UI::D` | raw storage the EventNotifyDialog is constructed in | yes |
| `GatherRawMaterials_Ctor_Impl` | Work/gatherrawmaterials_ctor.cpp:128 +1 | `extern "C" Work* GatherRawMaterials_Ctor_Impl(void* mem, ComponentEntity* const* buildEntityIBP)` | raw storage the work is constructed in | yes |
| `HarvestOrchard_CtorFromEntity` | Work/harvestorchard_ctor.cpp:78 +1 | `extern "C" Work* HarvestOrchard_CtorFromEntity(void* mem, ComponentEntity* const* entityIBP)` | raw storage the work is constructed in | yes |
| `OptionsDialog_Ctor_Standalone` | Dialog/optionsdialog.cpp:1591 +1 | `OptionsDialog* OptionsDialog_Ctor_Standalone(void* self, UI::UserInterface* ui, const UI::Dialog* extRes, cons` | raw storage the OptionsDialog is constructed in | yes |
| `PathGraph_ReassignWorkers` | Component/citizen_reassign_workers.cpp:221 +1 | `extern "C" void PathGraph_ReassignWorkers(void* ctxV)` | ctx = PathGraph, dead in the body | yes |
| `Pf_EntityResolvePos_2e20` | Action/action_pf_road.cpp:955 +1 | `void* Pf_EntityResolvePos_2e20(void* self, float* retVec3, int p3, int p4, float* p5)` | self = the InteractComponent (forwarded to InteractNodePosition) | yes |
| `Pf_RayTriIntersect_aa810` | Action/action_pf_geom.cpp:132 +1 | `int Pf_RayTriIntersect_aa810(void* /*self*/, const float* P, const float* D, const float* maxT, const float* V` | unused self | yes |
| `Pf_TileShapeLut_ae180` | Action/action_pf_geom.cpp:184 +1 | `int Pf_TileShapeLut_ae180(void* /*self*/, char p2, char p3, char p4, char p5, char p6, char p7, char p8, char ` | unused self | yes |
| `PlantOrchard_CtorFromEntity` | Work/plantorchard_ctor.cpp:89 +1 | `extern "C" Work* PlantOrchard_CtorFromEntity(void* mem, ComponentEntity* const* entityIBP, ComponentDescriptio` | raw storage the work is constructed in | yes |
| `PlayerEdit_AvgTerrainHeight_101730` | Player/tool_base_edit.cpp:66 +1 | `float PlayerEdit_AvgTerrainHeight_101730(void* /*param_1*/, int p2, int p3, int p4, int p5)` | unused first arg | yes |
| `PlayerRay_GetMouseRay_1005a0` | Player/tool_base_ray.cpp:197 +1 | `extern "C" uint8_t PlayerRay_GetMouseRay_1005a0(const void* /*param_1*/, float* outOrigin, float* outDir)` | never read | yes |
| `PlayerRay_ScreenToCell_101270` | Player/tool_base_ray.cpp:266 +1 | `extern "C" int PlayerRay_ScreenToCell_101270(const void* param_1)` | never read | yes |
| `PlayerScreenRect_EntityRect_0380` | Player/tool_base_screenrect.cpp:292 +1 | `extern "C" uint8_t PlayerScreenRect_EntityRect_0380(const void* /*param_1*/, ComponentEntity* const* param_2, ` | never read | yes |
| `PlayerTileRender_BuildRecords_1380` | Player/exe_bodies.h:137 +1 | `extern "C" void PlayerTileRender_BuildRecords_1380(const void* /*never read*/, Video::Context* ctx, int param_` | never read | yes |
| `PlayerTileRender_DrawOverlay_08c0` | Player/tool_base_tilerender.cpp:175 +1 | `extern "C" void PlayerTileRender_DrawOverlay_08c0(const void* /*param_1*/, Video::Context* ctx, const Collecti` | never read | yes |
| `Reimpl_Array_Resize_Byte` | Game/serialize_resize_helpers.cpp:198 +2 | `extern "C" void Reimpl_Array_Resize_Byte(void* arrayHeader, int newCount)` | array header (erased core) | yes |
| `Reimpl_Array_Resize_Stride02` | Game/serialize_resize_helpers.cpp:148 +2 | `extern "C" void Reimpl_Array_Resize_Stride02(void* arrayHeader, int newCount)` | array header (erased core) | yes |
| `Reimpl_Array_Resize_Stride04` | Game/serialize_resize_helpers.cpp:178 +5 | `extern "C" void Reimpl_Array_Resize_Stride04(void* arrayHeader, int newCount)` | array header (erased core) | yes |
| `Reimpl_Array_Resize_Stride08` | Game/serialize_arrays.cpp:280 +1 | `extern "C" void Reimpl_Array_Resize_Stride08(void* arrayHeader, int newCount)` | array header (erased core) | yes |
| `Reimpl_Array_Resize_Stride16A` | Game/serialize_resize_helpers.cpp:157 +1 | `extern "C" void Reimpl_Array_Resize_Stride16A(void* arrayHeader, int newCount)` | array header (erased core) | yes |
| `Reimpl_Array_Resize_Stride16B` | Game/serialize_resize_helpers.cpp:168 +1 | `extern "C" void Reimpl_Array_Resize_Stride16B(void* arrayHeader, int newCount)` | array header (erased core) | yes |
| `ResizeStride0x10` | Game/citizenlist_serialize.cpp:230 +1 | `void ResizeStride0x10(void* hdr, int n)` | array header (erased core) | yes |
| `ResolveExternalByName_Impl` | Component/componentdescription.cpp:212 +1 | `void ResolveExternalByName_Impl(void* nameString)` | an ExternalRef record (three record types share it) | yes |
| `SaveLoadDialog_Ctor_8270` | Dialog/exe_bodies.h:73 +1 | `extern "C" SaveLoadDialog* SaveLoadDialog_Ctor_8270(void* self, UI::UserInterface* ui, const UI::Dialog* extRe` | raw storage the SaveLoadDialog is constructed in | yes |
| `TendArea_CtorFromEntity` | Work/tendarea_ctor.cpp:43 +1 | `extern "C" Work* TendArea_CtorFromEntity(void* mem, ComponentEntity* const* entityIBP, int kind)` | raw storage the work is constructed in | yes |
| `Toolbar_CenterOnEntity_Impl` | Dialog/toolbardialog.cpp:1672 +1 | `void Toolbar_CenterOnEntity_Impl(void* toolController, void* parentDialogPtr, void* entityPtr)` | toolController = PlayerToolManager (also parentDialogPtr / entityPtr are void*) | yes |
| `UpdateToolUI_ebb0` | Player/tool_base_createbar.cpp:249 +1 | `extern "C" uint32_t UpdateToolUI_ebb0(const void* /*dummy*/, const ::Box* box, const Math::Transform* transfor` | dummy / never read | yes |
| `WeatherParticle_GeoCtor` | Game/weather_particles.cpp:137 +1 | `extern "C" void* WeatherParticle_GeoCtor(void* mem)` | raw storage the WeatherParticleGeometry is placed in | yes |
| `WorkAct_PickupRawMaterialCopyCtor_c9b70` | Work/work_base_actionctor.cpp:319 +1 | `PickupRawMaterial* WorkAct_PickupRawMaterialCopyCtor_c9b70(void* self, CitizenComponent* const* p2, ComponentE` | raw storage the PickupRawMaterial is placed in | yes |
| `WorkBase_TreeNodeFree_14c30` | Work/work_base_reimpl.cpp:118 +1 | `void WorkBase_TreeNodeFree_14c30(void* root, void* node)` | root = tree head (erased core) | yes |
| `WorkBase_TreeNodeFree_e320` | Work/work_base_reimpl.cpp:128 +1 | `void WorkBase_TreeNodeFree_e320(void* root, void* node)` | root = tree head (erased core) | yes |

### B2. NOT declared in an exe_bodies.h / *_funcs.h header (violations of B4_RULES 6.4)

"True type" = the class the `void*` should be; `none: ...` rows are not an object at all (address probe, raw storage) and need a ruling instead of a retype.

| name | file:line | signature | what the void* is | true type |
|---|---|---|---|---|
| `AsArray` | Component/coll_bridge.h:237 +1 | `inline Collection::Array<T>& AsArray(void* header)` | Collection::Array<T> header (typed view over a raw header) | Collection::Array<T>* |
| `Avl38FreeSubtree` | Player/exe_bodies.h:271 | `void Avl38FreeSubtree(const void* context, Node* node)` | unused RCX (the owning tree); body frees `node` | AvlTreeHead<Node>* |
| `ByteHeader` | Game/serialize_resize_helpers.cpp:57 | `RawArray<uint8_t>& ByteHeader(void* arrayHeader)` | array header of a byte array | RawArray<uint8_t>* |
| `CalcWindFactor_Impl` | Game/gameupdateloop.cpp:2164 | `static float CalcWindFactor_Impl(void* /*time — dead in retail*/)` | dead time argument (comment: dead in retail) | const System::Time* |
| `CitUI_OkPtr` | Dialog/citizenui.cpp:78 | `static inline bool CitUI_OkPtr(const void* p)` | raw address under probe (crash-guard) | none: address probe (needs a ruling, not a class) |
| `CreateInPlaceObject` | Object/exe_type.h:84 | `static ::Object::Base* CreateInPlaceObject(void* storage)` | raw storage of the placement-new in the Object type macro (exe_type.h) | none: placement storage of the macro's class |
| `Debug_Sprite_DefaultCtor` | Dialog/buildrequirementui.cpp:53 +2 | `extern void* Debug_Sprite_DefaultCtor(void* retval)` | retval = storage of the Sprite default-constructed (extern, three Dialog files) | UI::Sprite* (unverified: named by the extern only) |
| `DestroyBatchRange` | Game/serialize_struct60b.cpp:106 | `void DestroyBatchRange(void* ignoredThis, uint8_t* base, int count)` | ignored `this` of the Array<GeometryBatch> destroy slot | Collection::Array<GeometryBatch>* |
| `DestroyEventEntry` | Game/eventlog.cpp:107 | `void DestroyEventEntry(void* ignoredThis, uint8_t* base, int count)` | ignored `this` of the Array<EventLogEntry> destroy slot | Collection::Array<EventLogEntry>* |
| `DestroyIntAssocOuter` | Entity/citizenbehavior.cpp:1235 | `void DestroyIntAssocOuter(void* ignoredThis, uint8_t* base, int count)` | ignored `this` of the Array<Array<int32_t>> destroy slot | Collection::Array<Collection::Array<int32_t>>* |
| `DestroyMGRef` | Game/mapgenerator.cpp:268 | `void DestroyMGRef(void* ignoredThis, uint8_t* base, int count)` | ignored `this` of the Array<ExternalRef> destroy slot | Collection::Array<ExternalRef>* |
| `DestroyMGStructure` | Game/mapgenerator.cpp:312 | `void DestroyMGStructure(void* ignoredThis, uint8_t* base, int count)` | ignored `this` of the Array<InitialStructure> destroy slot | Collection::Array<InitialStructure>* |
| `DestroyProfRow` | Dialog/townhall_helpers.cpp:147 | `void DestroyProfRow(void* ignoredThis, uint8_t* base, int count)` | ignored `this` of the Array<ProfRow> destroy slot | Collection::Array<ProfRow>* |
| `EntityCompValidate` | Entity/componententity.cpp:49 | `extern "C" int EntityCompValidate(void*, void*, void*, int)` | three void* + int, extern decl of standalone/entity_comp_validator.cpp (unread) | unclear: harness validator, signature declared in componententity.cpp only |
| `Entity_ReassignCost_6c60` | Component/citizen_reassign_workers.cpp:161 | `extern "C" uint64_t Entity_ReassignCost_6c60(const void* /*param_1*/, ResidenceComponent* const* param_2, Resi` | param_1 commented out (unused) | unused first arg of a retail free function; none |
| `Executable` | Game/force_work.cpp:83 +1 | `bool Executable(const void* p)` | raw address under probe (force_work harness) | none: address probe (needs a ruling, not a class) |
| `FD_OkPtr` | Dialog/followdialog.cpp:49 | `static inline bool FD_OkPtr(const void* ptr)` | raw address under probe | none: address probe (needs a ruling, not a class) |
| `FoodPanel_Tick` | Game/gameupdateloop.cpp:370 | `extern "C" void FoodPanel_Tick(void* entityUI, int frameNum)` | entityUI = the PlayerToolManager (EntityUI) the harness ticks | PlayerToolManager* |
| `ForwardRef` | Game/serialize_externals.cpp:154 | `void ForwardRef(void* refField, void* serializer, uint32_t typeId, const char* typeName)` | refField = an Object::External* slot; serializer is the 2nd void* | Object::External** |
| `GameUpdateLoop_IsWorldBuilt` | Game/gameupdateloop_load.cpp:706 +1 | `extern "C" bool GameUpdateLoop_IsWorldBuilt(void* loop)` | the GameUpdateLoop under test | GameUpdateLoop* |
| `GenericArrayResize` | Game/serialize_resize_helpers.cpp:61 | `void GenericArrayResize(void* arrayHeader, int newCount, size_t stride, int alignment)` | array header, element size passed as stride (the erased core) | RawArray<uint8_t>* (erased) |
| `HasStates` | Component/controller_state_table.h:244 | `static char HasStates(void* /*node*/)` | node ignored (controller table slot) | ControllerComponent::Controller* |
| `Helper_sub_7c90` | Component/citizenai_housing_idle.cpp:310 +1 | `void Helper_sub_7c90(void* /*param_1: the citizen, unread*/, CitizenComponent* const* param_2, ComponentEntity` | "the citizen, unread" (comment) | CitizenComponent* |
| `Helper_sub_8000` | Component/citizenai_housing_idle.cpp:178 +1 | `void Helper_sub_8000(void* /*param_1: the citizen, unread*/, ComponentEntity* const* param_2, WorkSchedulerIdN` | "the citizen, unread" (comment) | CitizenComponent* |
| `Hotkey_BuildKeyName` | Dialog/hotkeytooltipui.cpp:73 +3 | `void Hotkey_BuildKeyName(void* retval, int device, int keyIndex)` | retval = String return slot | System::String* |
| `IdxForFallback` | Dialog/resourcelimitui.cpp:112 | `int IdxForFallback(void* self)` | identity key of a ResourceLimitUI row (compared by address) | ResourceLimitUI* |
| `InitPerformWork` | Component/citizenai_actionctors.cpp:132 | `static PerformWork* InitPerformWork(void* mem, CitizenComponent* citizen, float animIdx, uint32_t type, System` | raw storage the PerformWork is built in | none: placement storage of PerformWork |
| `IsDerivedFromOrTrust` | Object/factory_registration.cpp:194 +1 | `bool IsDerivedFromOrTrust(const void* obj, uint32_t typeId)` | any object, type-id tested | const Object::Base* |
| `MarketBst_ae50` | Work/marketwork_bst_a9f0.cpp:94 +1 | `extern "C" MarketWork::ClaimNode* MarketBst_ae50(void* unused, MarketWork::Claim* src, MarketWork::ClaimNode* ` | unused RCX (the claim tree) | MarketWork* |
| `MarketBst_aee0` | Work/marketwork_bst_aaa0.cpp:133 +1 | `extern "C" void MarketBst_aee0(void* /*unusedRcx*/, MarketWork::ClaimNode* node)` | unused RCX (the claim tree) | MarketWork* |
| `MemFree` | Action/action_pf_region.cpp:44 +3 | `static inline void MemFree(void* p)` | raw heap block | none: raw allocation (System::MemoryAllocator takes void*) |
| `OrchardForceStart_Tick` | Game/gameupdateloop.cpp:390 | `extern "C" void OrchardForceStart_Tick(void* entityUI, int frameNum)` | entityUI = PlayerToolManager | PlayerToolManager* |
| `PanelHoldHarness_Tick` | Game/gameupdateloop.cpp:381 | `extern "C" void PanelHoldHarness_Tick(void* entityUI, int frameNum)` | entityUI = PlayerToolManager | PlayerToolManager* |
| `PanelStaffingDriver_Tick` | Game/gameupdateloop.cpp:380 | `extern "C" void PanelStaffingDriver_Tick(void* entityUI, int frameNum)` | entityUI = PlayerToolManager | PlayerToolManager* |
| `PauseMenuHarness_Tick` | Game/gameupdateloop.cpp:369 | `extern "C" void PauseMenuHarness_Tick(void* entityUI, int frameNum)` | entityUI = PlayerToolManager | PlayerToolManager* |
| `PickXfCtorMember` | Component/citizenai_executor.cpp:269 | `static Math::Transform* PickXfCtorMember(void* xformOut, const Math::Transform* transformIn)` | xformOut = Transform return storage | Math::Transform* |
| `Plausible` | Component/destroycomponent_workerrelease.cpp:97 | `static bool Plausible(const void* p)` | raw address under probe | none: address probe (needs a ruling, not a class) |
| `PtrCorrupt` | Entity/entitylist.cpp:196 | `static inline bool PtrCorrupt(const void* pv)` | raw address under probe | none: address probe (needs a ruling, not a class) |
| `RUI_OkPtr` | Dialog/residenceui.cpp:62 | `static inline bool RUI_OkPtr(const void* ptr)` | raw address under probe | none: address probe (needs a ruling, not a class) |
| `Readable` | Game/force_work.cpp:56 +1 | `bool Readable(const void* p, size_t n)` | raw address + length under probe | none: address probe (needs a ruling, not a class) |
| `Reimpl_Array_Resize_Stride0C` | Game/serialize_resize_helpers.cpp:188 +1 | `extern "C" void Reimpl_Array_Resize_Stride0C(void* arrayHeader, int newCount)` | array header (erased core), not in a header: defined in serialize_resize_helpers.cpp, extern in namedatabase.cpp | RawArray<T>* (erased core) |
| `RequestTransition` | Component/controller_state_table.h:233 | `static int RequestTransition(void* /*node*/, int curIdx, int* pDesired)` | node ignored (controller table slot) | ControllerComponent::Controller* |
| `RosterCompare_9e20` | Component/residencecomponent.cpp:271 | `int RosterCompare_9e20(const void* pa, const void* pb)` | qsort element pointers: each points at a CitizenComponent* slot | CitizenComponent* const* |
| `SI_Plausible` | Component/statusiconcomponent.cpp:359 | `static inline bool SI_Plausible(const void* p)` | raw address under probe | none: address probe (needs a ruling, not a class) |
| `SafeGetTypeId` | Game/force_work.cpp:95 +1 | `uint32_t SafeGetTypeId(void* obj)` | any object whose type id is read under SEH | Object::Base* |
| `SaneHeapPtr` | Dialog/buildrequirementui.cpp:60 | `static inline bool SaneHeapPtr(const void* p)` | raw address under probe | none: address probe (needs a ruling, not a class) |
| `SaveLoadHarness_Tick` | Game/gameupdateloop.cpp:376 | `extern "C" void SaveLoadHarness_Tick(void* gul, int frameNum)` | gul = GameUpdateLoop | GameUpdateLoop* |
| `ScenarioDialog_Ctor` | Dialog/scenariodialog.cpp:475 +1 | `ScenarioDialog* ScenarioDialog_Ctor(void* self, UI::UserInterface* ui, const UI::Dialog* extRes, const Object:` | raw storage the ScenarioDialog is constructed in | none: placement storage of ScenarioDialog (same shape as the *_Ctor_Standalone rows) |
| `SerializeLegacyKeyIntTree` | Game/citizenlist_serialize.cpp:429 | `void SerializeLegacyKeyIntTree(void* rootField, int32_t* countField, IO::Serializer* serializer)` | rootField = the AVL root slot of a legacy key->int tree | AvlTreeHead<Node>* / node-pointer slot (erased core) |
| `Step` | Component/controller_state_table.h:203 | `static int Step(void* /*node*/, int input)` | node ignored (controller table slot 0) | ControllerComponent::Controller* |
| `ToolHarness_Tick` | Game/gameupdateloop.cpp:368 | `extern "C" void ToolHarness_Tick(void* entityUI, int frameNum)` | entityUI = PlayerToolManager | PlayerToolManager* |
| `ToolUI_FindTracked` | Dialog/toolui.cpp:348 | `static PinnedToolNode* ToolUI_FindTracked(const void* key)` | key of the pinned-tool set: a description or an entity address (known_port_issues row 661) | const ComponentDescription* / ComponentEntity* (two callers hand different types) |
| `ToolbarDialog_Ctor_Impl` | Dialog/toolbardialog.cpp:1144 +1 | `ToolbarDialog* ToolbarDialog_Ctor_Impl(void* self, UI::UserInterface* userInterface, const UI::Dialog* dialog,` | raw storage the ToolbarDialog is constructed in | none: placement storage of ToolbarDialog (same shape as the *_Ctor_Standalone rows) |
| `TradeSellHarness_Tick` | Game/gameupdateloop.cpp:385 | `extern "C" void TradeSellHarness_Tick(void* entityUI, int frameNum)` | entityUI = PlayerToolManager | PlayerToolManager* |
| `TreeNodeFree_2ce60` | Game/gameupdateloop_teardown.cpp:148 | `extern "C" void TreeNodeFree_2ce60(void* hdrV, WorkSchedulerBucketNode* node)` | hdrV = the tree head | AvlTreeHead<WorkSchedulerBucketNode>* |
| `TreeNodeFree_2df90` | Game/gameupdateloop_teardown.cpp:160 | `extern "C" void TreeNodeFree_2df90(void* hdrV, ToolRegistryNode* node)` | hdrV = the tree head | AvlTreeHead<ToolRegistryNode>* |
| `Update` | Player/BridgeTool.h:49 +8 | `void Update(void*, char) override` | Tool::Update(void* time, char active) and its overrides in Player/*Tool.h: the frame time | const System::Time* |
| `ValidNode` | Component/citizenai_housing_idle.cpp:129 | `static inline bool ValidNode(const void* p)` | raw address under probe | none: address probe (needs a ruling, not a class) |
| `ValidateTask` | Work/Work.h:146 | `bool ValidateTask(void* unusedRdx, CitizenComponent** citizen, CompleteTask** out)` | unusedRdx (spilled, never read, per the comment) | none: dead argument of a Work member; `void*` is a placeholder |
| `WAP_BuildInlinePerformWork` | Work/work_act_perform.cpp:100 | `static PerformWork* WAP_BuildInlinePerformWork(void* o, CitizenComponent* entityInner, float heading, int acti` | raw storage the PerformWork is built in | none: placement storage of PerformWork |
| `WeldHash` | Game/water_contour_mesh.cpp:190 | `uint32_t WeldHash(const void* key, int stride)` | key bytes of a vertex record, stride passed | none: raw byte record (hash input) |
| `WorkLeaf_TileDistCompare` | Work/work_leaf_clear.cpp:314 | `int WorkLeaf_TileDistCompare(const void* a, const void* b)` | qsort element pointers: GridTile | const GridTile* |
| `WorkPlaceComponent_WasFactoryBuilt` | Component/workplacecomponent.cpp:691 | `extern "C" bool WorkPlaceComponent_WasFactoryBuilt(const void* p)` | a WorkPlaceComponent address looked up in the census table | const WorkPlaceComponent* |

B1 = 43 names, B2 = 63 names (violations), B total = 106.

### B2 by true type

| true type | count |
|---|---|
| none: address probe | 10 |
| PlayerToolManager* | 7 |
| ControllerComponent::Controller* | 3 |
| RawArray<uint8_t>* | 2 |
| const System::Time* | 2 |
| GameUpdateLoop* | 2 |
| CitizenComponent* | 2 |
| none: placement storage of PerformWork | 2 |
| MarketWork* | 2 |
| Collection::Array<T>* | 1 |
| AvlTreeHead<Node>* | 1 |
| none: placement storage of the macro's class | 1 |
| UI::Sprite* | 1 |
| Collection::Array<GeometryBatch>* | 1 |
| Collection::Array<EventLogEntry>* | 1 |
| Collection::Array<Collection::Array<int32_t>>* | 1 |
| Collection::Array<ExternalRef>* | 1 |
| Collection::Array<InitialStructure>* | 1 |
| Collection::Array<ProfRow>* | 1 |
| unclear: harness validator, signature declared in componententity.cpp only | 1 |
| unused first arg of a retail free function; none | 1 |
| Object::External** | 1 |
| System::String* | 1 |
| ResourceLimitUI* | 1 |
| const Object::Base* | 1 |
| none: raw allocation | 1 |
| Math::Transform* | 1 |
| RawArray<T>* | 1 |
| CitizenComponent* const* | 1 |
| Object::Base* | 1 |
| none: placement storage of ScenarioDialog | 1 |
| AvlTreeHead<Node>* / node-pointer slot | 1 |
| const ComponentDescription* / ComponentEntity* | 1 |
| none: placement storage of ToolbarDialog | 1 |
| AvlTreeHead<WorkSchedulerBucketNode>* | 1 |
| AvlTreeHead<ToolRegistryNode>* | 1 |
| none: dead argument of a Work member; `void*` is a placeholder | 1 |
| none: raw byte record | 1 |
| const GridTile* | 1 |
| const WorkPlaceComponent* | 1 |

## C. Casts left, by reason

(generated by the coordinator from castmetric.py --detail)
