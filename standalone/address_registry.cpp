// Address registry for standalone exe build.
// Maps all known Ghidra exe addresses to our reimplemented functions/data.
//
// This is the core mapping that makes standalone mode work. As functions get
// reimplemented in code/Game/, add their addresses here. Unregistered addresses
// return 0 (NULL) from ResolveAddr and log a NOT IMPLEMENTED warning.
//
// Three sections:
//   1. CODE FUNCTIONS — direct exe function reimplementations
//   2. IAT DISPATCH SLOTS — .rdata slots that hold Runtime DLL function pointers
//   3. VTABLES & TYPE IDS — static vtable arrays and type info globals

#ifdef STANDALONE_EXE

#include "resolve.h"
#include "address_registry.h"
#include "engine_funcs.h"
#include "Game/game.h"
#include "../runtime/src/io/serializer.h"  // RtSerializer_* reimpl forwarders (env-gated)
#include <cstdio>
#include <cstdint>

// ============================================================
// KeyMap exref (slot 0x140151738) — fake External wrapper for standalone.
//
// Original exe layout: the slot at 0x140151738 holds an Object::ExternalPtr
// wrapper that, when triple-dereffed, yields the live Input::KeyMap pointer.
// The Runtime DLL fills this slot during type-registration plumbing in the
// real game. In standalone we never go through that machinery, so the slot
// stays zero and ButtonElement::Update faults inside Input::KeyMap::Pressed.
//
// We fake it with a 2-link chain matching the disasm
//   MOV RAX,[0x140151738] ; MOV R8,[RAX] ; MOV R14,[R8]
// where R14 = KeyMap*. Three derefs total from the slot address.
//
// After Engine::Startup (which constructs Input::Configuration with its
// embedded KeyMap), RunGameMain calls SetStandaloneKeyMap(km) to fill
// g_StandaloneKeyMapPtr with the real pointer. Game::RegisterDefaultInputBindings
// then walks the same triple-deref chain the original code uses.
// ============================================================
extern "C" {
    // Level 3 — actual KeyMap*. Filled at runtime after Engine::Startup.
    void* g_StandaloneKeyMapPtr = nullptr;
    // Level 2 — points to level-3 storage (constant).
    void* g_StandaloneKeyMapMid = &g_StandaloneKeyMapPtr;
}

extern "C" void SetStandaloneKeyMap(void* keyMap) {
    g_StandaloneKeyMapPtr = keyMap;
}

// Per-class vtable registration entry points. Each lives next to its class's
// reimplementation in code/Game/<Module>/<Class>.cpp.
extern "C" {
    void RegisterStatGoalVtable();
    void RegisterTimeGoalVtable();
    void RegisterScenarioVtable();
    void RegisterToolbarVtable();
    void RegisterMusicListVtable();
    void RegisterBuildingGoalVtable();
    void RegisterPopulationGoalVtable();
    void RegisterTimeGoalInstanceVtable();
    void RegisterFinishTutorialGoalVtable();
    void RegisterFinishTutorialInstanceVtable();
    void RegisterMainMenuDialogVtable();
    void RegisterLoaderDialogVtable();
    void RegisterOptionsDialogVtable();
    void RegisterNewGameDialogVtable();
    void RegisterOptionInputBindDialogVtable();
    void RegisterMessageDialogVtable();
    void RegisterMenuSceneVtable();
    void RegisterGeometryVtable();
    void RegisterWeatherVtable();
    void RegisterClimateGeneratorVtable();
    void RegisterMapGeneratorVtable();
    void RegisterTerrainGeneratorVtable();

    // M5 5c Foundation C — entity description types (template sub-descriptions).
    void RegisterComponentDescriptionVtable();
    void RegisterBuildDescriptionVtable();
    void RegisterWorkDescriptionVtable();
    void RegisterWorkPlaceDescriptionVtable();
    void RegisterResidenceDescriptionVtable();
    void RegisterZoneDescriptionVtable();
    void RegisterMapDescriptionVtable();
    void RegisterCreatePlacedDescriptionVtable();
    void RegisterCreateDragDescriptionVtable();
    void RegisterCharacterDescriptionVtable();
    void RegisterCitizenDescriptionVtable();
    void RegisterCitizenAIDescriptionVtable();
    void RegisterStateMachineDescriptionVtable();
    void RegisterNaturalResourceDescriptionVtable();
    void RegisterAbsorbResourceDescriptionVtable();
    void RegisterUIDescriptionVtable();
    void RegisterToolbarDescriptionVtable();
    void RegisterStatusIconDescriptionVtable();
    void RegisterAmbientEmitterDescriptionVtable();
    void RegisterInteractDescriptionVtable();
    void RegisterClearAreaDescriptionVtable();
    void RegisterDestroyDescriptionVtable();
    void RegisterStorageDescriptionVtable();
    void RegisterStorageLocationDescriptionVtable();
    void RegisterStorageDisplayDescriptionVtable();
    void RegisterModelDescriptionVtable();
    void RegisterPickingDescriptionVtable();
    void RegisterHighlightDescriptionVtable();
    void RegisterDecalDescriptionVtable();
    void RegisterParticleDescriptionVtable();
    void RegisterHerdDescriptionVtable();
    void RegisterPopulationDescriptionVtable();
    void RegisterAnimalDescriptionVtable();
    // wave 2 (economy / farming / services / tools)
    void RegisterAmbientEnvironmentDescriptionVtable();
    void RegisterAttendanceDescriptionVtable();
    void RegisterAutoPickupDescriptionVtable();
    void RegisterBoatDescriptionVtable();
    void RegisterCaptureBoxDescriptionVtable();
    void RegisterCemeteryDescriptionVtable();
    void RegisterClearResourceDescriptionVtable();
    void RegisterConsumeProduceDescriptionVtable();
    void RegisterCreateBridgeDescriptionVtable();
    void RegisterCreateFencedDescriptionVtable();
    void RegisterCreatePathDescriptionVtable();
    void RegisterCreateRemovalDescriptionVtable();
    void RegisterCreateRoadDescriptionVtable();
    void RegisterCropFieldDescriptionVtable();
    void RegisterDiseaseDescriptionVtable();
    void RegisterEnvironmentDescriptionVtable();
    void RegisterFireDescriptionVtable();
    void RegisterFishingDescriptionVtable();
    void RegisterFloorDescriptionVtable();
    void RegisterForesterDescriptionVtable();
    void RegisterFruitDescriptionVtable();
    void RegisterFrustumOutlineDescriptionVtable();
    void RegisterGrowthDescriptionVtable();
    void RegisterHappinessDescriptionVtable();
    void RegisterHealerDescriptionVtable();
    void RegisterHerbalistDescriptionVtable();
    void RegisterHuntDescriptionVtable();
    void RegisterImmigrationDescriptionVtable();
    void RegisterLivestockDescriptionVtable();
    void RegisterMarketDescriptionVtable();
    void RegisterMarketLocationDescriptionVtable();
    void RegisterMineDescriptionVtable();
    void RegisterMultiObjectDescriptionVtable();
    void RegisterNomadDescriptionVtable();
    void RegisterOrchardDescriptionVtable();
    void RegisterPastureDescriptionVtable();
    void RegisterQuarryDescriptionVtable();
    void RegisterRadiusDescriptionVtable();
    void RegisterRawMaterialDescriptionVtable();
    void RegisterSelectDescriptionVtable();
    void RegisterSpawnDescriptionVtable();
    void RegisterTerraformDescriptionVtable();
    void RegisterTombstoneDescriptionVtable();
    void RegisterTornadoDescriptionVtable();
    void RegisterTrackerDescriptionVtable();
    void RegisterTradeDescriptionVtable();
    void RegisterValueDescriptionVtable();
    void RegisterWellDescriptionVtable();
    void RegisterWildAnimalDescriptionVtable();
    void RegisterAnimAudioEventVtable();
    void RegisterProfessionVtable();
    void RegisterNameDatabaseVtable();
    void RegisterDiseaseVtable();
    void RegisterProfessionListVtable();

    // Factory cascade (notes/FACTORY_CASCADE_145_BRIEF.md) — Action family.
    void RegisterPlayAnimationVtable();
    void RegisterAnimateNaturalResourceVtable();
    void RegisterCompleteRoadVtable();
    void RegisterCompleteTaskVtable();
    void RegisterConsumeRawMaterialVtable();
    void RegisterCreateNaturalResourceVtable();
    void RegisterCreateRawMaterialVtable();
    void RegisterDropOffRawMaterialVtable();
    void RegisterHuntAnimalVtable();
    void RegisterMoveToVtable();
    void RegisterPerformWorkVtable();
    void RegisterPickupRawMaterialVtable();
    void RegisterPickupStoredRawMaterialVtable();
    void RegisterRemoveAnimalVtable();
    void RegisterRemoveNaturalResourceVtable();
    void RegisterSendComponentMessageVtable();
    void RegisterShowToolVtable();

    // Factory cascade — Work family (Object::Internal -> Work, 13-slot vtables).
    void RegisterBuildRoadVtable();
    void RegisterBuildStructureVtable();
    void RegisterClearAreaVtable();
    void RegisterClearStorageVtable();
    void RegisterConsumeProduceVtable();
    void RegisterFireWorkVtable();
    void RegisterFishingWorkVtable();
    void RegisterGatherRawMaterialsVtable();
    void RegisterLocationWorkVtable();
    void RegisterMarketWorkVtable();
    void RegisterPickupVtable();
    void RegisterPlantAreaVtable();
    void RegisterPlantOrchardVtable();
    void RegisterTendAreaVtable();
    void RegisterTradeWorkVtable();
    void RegisterForesterWorkVtable();
    void RegisterHarvestAreaVtable();
    void RegisterHarvestOrchardVtable();
    void RegisterHerbalistWorkVtable();
    void RegisterHuntWorkVtable();
    void RegisterPastureWorkVtable();

    // Factory cascade — External-resource family (Object::External, 9-slot
    // vtables w/ GetFileVersion @ +0x18; MusicList pattern). Tutorial types.
    void RegisterTutorialVtable();
    void RegisterTutorialGoalButtonVtable();
    void RegisterTutorialGoalGameSpeedVtable();
    void RegisterTutorialGoalCountVtable();
    void RegisterTutorialImageVtable();
    void RegisterTutorialImageHotKeyVtable();
    void RegisterTutorialImageInputKeyVtable();
    void RegisterTutorialImageToolbarIconVtable();
    void RegisterTutorialSetupCameraVtable();
    void RegisterTutorialSetupGameSpeedVtable();
    void RegisterTutorialSetupRestrictPlayerVtable();
    void RegisterTutorialStepVtable();

    // Factory cascade — Tool family (Object::Internal -> Tool / DragTool /
    // ToolSetting; shared base in code/Game/Player/tool_base_standalone.cpp).
    void RegisterBridgeToolVtable();
    void RegisterDragPlacementToolVtable();
    void RegisterEditToolVtable();
    void RegisterFencedToolVtable();
    void RegisterPathToolVtable();
    void RegisterPlacementToolVtable();
    void RegisterRemovalToolVtable();
    void RegisterRoadToolVtable();
    void RegisterRotationSettingVtable();
}

// ============================================================
// Section 1: CODE FUNCTIONS
// ============================================================
// As you reimplement exe functions in code/Game/, add entries here:
//   RegisterExeAddr(0x140XXXXXX, (uintptr_t)&YourFunction);
//
// Example:
//   #include "Game/weather_funcs.h"
//   RegisterExeAddr(0x140025020, (uintptr_t)&Weather_Dtor);

// M3.5 — MenuScene::Serialize generic helpers
// (decompiled bodies live in code/Game/Game/serialize_arrays.cpp +
//  code/Game/Game/serialize_externals.cpp)
extern "C" {
    void Reimpl_Geometry_Init(void* self);
    void Reimpl_Transform_SerializeWrapper(void* transform, void* stream);
    void Reimpl_Array_Resize_Stride20(void* arrayHeader, int newCount);
    void Reimpl_Array_Resize_Stride08(void* arrayHeader, int newCount);
    void Reimpl_Array_Resize_Stride14(void* arrayHeader, int newCount);
    void Reimpl_Array_Serialize_Stride20(void* serializer, uint32_t count, void* arrayBase);
    void Reimpl_Array_Serialize_External_GraphicsMesh(void* serializer, int count, void* arrayBase);
    void Reimpl_Array_Serialize_External_Particle(void* serializer, int count, void* arrayBase);
    void Reimpl_ExternalPtr_Serialize_GraphicsMesh(void* refField, void* serializer);
    void Reimpl_ExternalPtr_Serialize_Weather(void* refField, void* serializer);
    void Reimpl_ExternalPtr_Serialize_StringTable(void* refField, void* serializer);
}

// M3.5 Path B session 2 — 0x60B struct array lifecycle helpers
// (decompiled bodies live in code/Game/Game/serialize_struct60b.cpp)
extern "C" {
    void Reimpl_Struct60B_Resize(void* arrayHeader, unsigned int newCount);
    void* Reimpl_Struct60B_CopyCtor(void* dst, const void* src);
    void Reimpl_Struct60B_DtorRange(void* unused, void* first, int count);
}

// M3.5 Path B session 3 — typed-ExternalPtr serialize helper at 0x140007140.
// (decompiled body lives in code/Game/Game/serialize_externals_typed.cpp)
extern "C" {
    void Reimpl_ExternalPtr_Serialize_Typed_7140(void* refField, void* serializer);
    // Scenario's typed ExternalPtr serializers (menu-loader factory cascade).
    void Reimpl_ExternalPtr_Serialize_MapGenerator(void* refField, void* serializer);
    void Reimpl_ExternalPtr_Serialize_TerrainGenerator(void* refField, void* serializer);
    void Reimpl_ExternalPtr_Serialize_ClimateGenerator(void* refField, void* serializer);
}

// M3.5 Path B session 5 — additional Geometry::Serialize resize helpers
// (decompiled bodies live in code/Game/Game/serialize_resize_helpers.cpp)
extern "C" {
    void Reimpl_Array_Resize_Stride02(void* arrayHeader, int newCount);
    void Reimpl_Array_Resize_Stride16A(void* arrayHeader, int newCount);
    void Reimpl_Array_Resize_ArrayOfArrays(void* arrayHeader, int newCount);
    void Reimpl_Array_Resize_Stride16B(void* arrayHeader, int newCount);
    void Reimpl_Array_Resize_Stride04(void* arrayHeader, int newCount);
    void Reimpl_Array_Resize_Stride0C(void* arrayHeader, int newCount);
    void Reimpl_Array_Resize_Byte(void* arrayHeader, int newCount);
}

// M3.5 Path B session 4 — per-element 0x60B + typed-ExternalPtr-array serialize
// (decompiled bodies live in code/Game/Game/serialize_struct60b_serialize.cpp)
extern "C" {
    void Reimpl_Struct60B_Serialize(void* serializer, int count, void* arrayBase);
    void Reimpl_Array_Serialize_External_MaterialInstance(
        void* serializer, int count, void* arrayBase);
}

// M3.5 Path B session 6 — heightmap mesh-build path helpers
// (decompiled bodies live in code/Game/Game/serialize_heightmap_mesh.cpp)
extern "C" {
    void  Reimpl_TradeWork_sub_c1e0(void* self);
    void  Reimpl_FUN_1400ac880(void* self, int colStart, int rowStart,
                               int cellSize_x, int cellSize_y, int16_t* vertOut);
    void* Reimpl_TradeWork_helper_e270(void* self, void* retbuf, int col, int row);
    void  Reimpl_GeometryChunkUpdate(void* self);
}

// M3.5 Path B session 7 — image-buffer heightmap upload helper
// (decompiled body lives in code/Game/Game/serialize_image_upload.cpp)
extern "C" {
    void Reimpl_TradeWork_sub_c780(void* self);
}

// M3.5 Path B session 8 — Geometry::Serialize body + version-gated extra
// (decompiled bodies live in code/Game/Game/geometry_serialize.cpp)
extern "C" {
    void Reimpl_TradeWork_impl_04d0(void* self, void* stream);
    void Reimpl_TradeWork_impl_f1a0(void* self, void* stream);
}

// M5 — GameUpdateLoop::Load (world build) registers its own exe addrs + float consts.
extern "C" void RegisterGameUpdateLoopLoadAddrs();

// M5 5c — Map terrain-position query (fn_MapGetTerrainPos = FUN_14009f3e0, ~100 call sites).
extern "C" void RegisterMapQueryAddrs();

// M5 5c Foundation B — ECS spawn primitives (bodies in
// code/Game/Entity/componententity.cpp). C++ linkage: declared with the exact
// signatures so the mangled names match the definitions.
struct ComponentEntity;
ComponentEntity* ComponentEntity_CtorFromDescription(ComponentEntity* self, void* descriptionPtr);
void EntityList_RegisterEntity_Impl(void* entityList, void* entityIBP, int category);

// In-game HUD — ToolbarDialog ctor reimpl (FUN_1400b9910), body in
// code/Game/Dialog/toolbardialog.cpp. C++ linkage (not extern "C").
void* ToolbarDialog_Ctor_Impl(void* self, void* userInterface, void* dialog,
                              void* parentPtr, void* toolbarData);

// Component registration wave (2026-06-18) — each batch registrar lives in its
// own standalone/reg_component_<batch>.cpp and registers that batch's reimplemented
// exe helper addresses. See notes/COMPONENT_REG_WAVE_BRIEF.md.
extern void RegisterComponentBatch_people();
extern void RegisterComponentBatch_economy();
extern void RegisterComponentBatch_nature();
extern void RegisterComponentBatch_visuals();
// Phase-1 self-contained-sim live bring-ups (2026-06-20) — each registers only the
// type-id .rdata global its component's live Update/Startup needs (all other helpers
// already registered by earlier waves). See final_steps.md §3 Phase 1.
extern void RegisterComponentBatch_fruit();
extern void RegisterComponentBatch_spawn();
extern void RegisterComponentBatch_tombstone();
// Component GAP wave (2026-06-19, branch wave/component-gap) — residual-73 closure-safe
// helpers + 2 caller-bug literal fixes. reg files: reg_component_gap_{A,B,C,D}.cpp.
// A: citizencomponent/attendance (4). B: citizenAI setup/fishing/find/serialize +
// residence caller-bug 35ec0->07960 (6+1). C: decal/statusicon (2). D: herd/trade/build +
// terraform caller-bug a4b50->ab150 (3+1). nomad b60 deferred (Factory+vtable-Startup).
extern void RegisterComponentGap_A();
extern void RegisterComponentGap_B();
extern void RegisterComponentGap_C();
extern void RegisterComponentGap_D();

// Work registration wave (2026-06-18) — shared base-helper sub-batch.
// reg file: standalone/reg_work_base.cpp ; bodies: code/Game/Work/work_base_reimpl.cpp.
extern void RegisterWorkBaseBatch();
// Work simple-BST (AVL) family — reg_work_bst.cpp ; bodies: code/Game/Work/work_base_bst.cpp.
extern void RegisterWorkBstBatch();
// Work cell-BST (AVL) family — reg_work_cellbst.cpp ; bodies: code/Game/Work/work_base_cellbst.cpp.
extern void RegisterWorkCellBstBatch();
// Work grid / assignment helpers — reg_work_grid.cpp ; bodies: code/Game/Work/work_base_grid.cpp.
extern void RegisterWorkGridBatch();
// Work shared-helper tail — FindComponent accessors — reg_work_accessor.cpp ; bodies: work_base_accessor.cpp.
extern void RegisterWorkAccessorBatch();
// Work shared-helper tail — sqrtf math helper — reg_work_impl2.cpp ; bodies: work_base_impl2.cpp.
extern void RegisterWorkImpl2Batch();
// Work shared-helper cluster — walkable-position search + build step — reg_work_walkpos.cpp ;
// bodies: work_base_walkpos.cpp (122be0/122b00/f6ef0; needs Transform::operator* slot 0x140151470).
extern void RegisterWorkWalkposBatch();
// Work-gap LEAF wave — 17 closure-safe per-class job helpers — reg_work_leaf.cpp ;
// bodies: work_leaf_{clear,market,gather,hunt}.cpp (4-batch opus fan-out, lead-verified).
extern void RegisterWorkLeafBatch();
// Wave 3 (Action) — StateMachine action exe-helpers, closure-safe subsets.
// reg files: reg_action_cnr.cpp / reg_action_hunt.cpp ; bodies: code/Game/Action/action_base_*.cpp.
extern void RegisterActionCnrBatch();    // CreateNaturalResource cluster (10, file closure-clean)
extern void RegisterActionHuntBatch();   // Hunt/RemoveAnimal/CompleteRoad (6 closure-safe)
extern void RegisterActionMoveBatch();   // MoveTo facing/find (4 closure-safe: 12d0/a060/c8d20/a6d50)
extern void RegisterActionCompleteBatch(); // CompleteTask BST (8760/8800/8e30) + ShowTool (1173b0/1176e0)
extern void RegisterActionRawMatBatch();   // raw-material batch E leaves (d150/d000/da70/44810/88f90/223c0)
// Pathfinding/region/road closure (drive Action gap -> 0), bottom-up tiers:
extern void RegisterPfAvlBatch();          // Tier 0: AVL/BST node-pool + rotations (10)
extern void RegisterPfPoolsBatch();        // Tier 0: A* node-pools + array resizes (13)
extern void RegisterPfGeomBatch();         // Tier 0: region/geometry/heuristic leaves (9)
extern void RegisterPfCtorsBatch();        // Wave 2: Action ctors + entity/storage leaves (12)
extern void RegisterActionChildCtorsBatch(); // Phase 2 Keystone B: 5 missing action child-ctors
extern void RegisterWorkActionCtorBatch();   // Phase 2 Keystone B2: 4 Work action-ctors
extern void RegisterActionChildCtors2Batch();// Phase 2 Keystone B3: leaf copy-ctors + consume-list
extern void RegisterCitizenAICandidateList();// Phase 3 Keystone D Brick 3: CitizenAI candidate-selection
extern void RegisterCitizenAIHandlers();     // Phase 3 Keystone D Brick 3b: CitizenAI state handlers
extern void RegisterCitizenAIFindTarget();   // Phase 3 Keystone D Brick 3b: CitizenAI find-best-target (dd30)
extern void RegisterCitizenAIActionCtors();  // Phase 3 Keystone D Brick 3b: CitizenAI action-ctor family
extern void RegisterCitizenAIExecutor();     // Phase 3 Keystone D Brick 4b: action executor + move-tick
extern void RegisterCitizenAIHandlers2();    // Phase 3 Keystone D Brick 3b: handler wave 2 (Education/SeekHappiness/FindWork)
extern void RegisterCitizenAIFindHelpers();  // Phase 3 Keystone D Brick 3b: deep find-helpers (2ac30/db20/70b0/ec70/e370)
extern void RegisterCitizenAIHandlers3();    // Phase 3 Keystone D Brick 3b: handler wave 3 (Work/Adult/Elder)
extern void RegisterCitizenAIGather();       // Phase 3 Keystone D Brick 3b-W4: Gather/Gather2 (+12ab00)
extern void RegisterCitizenAIGrowUp();       // Phase 3 Keystone D Brick 3b-W4: GrowUp + LongTermPlan (+f230)
extern void RegisterCitizenAILeisure();      // Phase 3 Keystone D Brick 3b-W4: Leisure (+99d0/fd50/a720)
extern void RegisterCitizenAIHousingIdle();  // Phase 3 Keystone D Brick 3b-W4: FindHousing/Idle + AVL subtree
extern void RegisterWorkActBatch();          // Phase 2 Keystone B3: 5 Work action-queue builders
extern void RegisterWorkTailTradeBatch();    // Phase 2 Keystone B tail: TradeWork AVL serialize (c8a0/c710/c830)
extern void RegisterWorkTailFireBatch();     // Phase 2 Keystone B tail: FireWork spiral search (881d0/88730)
extern void RegisterWorkTailSerializeBatch();// Phase 2 Keystone B tail: PastureWork Slot4 (f2370) registration
extern void RegisterKeystoneAZoneBatch();    // Phase 2 Keystone A: zone+road enqueue (a6f00/9d360/9ce00/a6e30)
extern void RegisterKeystoneARestreeBatch(); // Phase 2 Keystone A: residence RB-tree (17230/15ec0/172f0/17460/14b40)
extern void RegisterKeystoneAMapBatch();     // Phase 2 Keystone A: map overlay/init (1241b0/9dc20/124750/9dee0/e2c0)
extern void RegisterKeystoneAGameDtorBatch();// Phase 2 Keystone A: Game::Dtor + worker-thread teardown (9ed60 + 8 subs)
extern void RegisterPfRegionBatch();       // Wave 2: region-graph build/merge/flood-fill (13)
extern void RegisterPfRoadBatch();         // Wave 2: road-mesh + geometry wrappers (10)
extern void RegisterPfAstarBatch();        // Wave 2: A* core + thread dispatcher/enqueuers (15)
extern void RegisterPfRootsRoadBatch();    // Wave 3: road-edit roots (9c480/9f2f0/9e0e0/9c640)
extern void RegisterPfRootsRouteBatch();   // Wave 3: routing/storage roots (9f810/12cd80/12d5d0/8d590)
extern void RegisterPfRootsDropBatch();    // Wave 3: dropoff + RemoveAnimal ctor (c78c0/c7570/c7620/bbea0)
extern void RegisterPfRootsMiscBatch();    // Wave 3: bd40 spiral + 92260 spatial + 122340 migration
extern void RegisterEntityBatch();         // Entity gap: 6 EntityList reimpl bodies (entitylist.cpp)
extern void RegisterGoalBatch();           // Goal gap: ExternalPtr resolver + StatValueDispatch
extern void RegisterDialogBatch();         // Dialog gap: FindComponent variant + autosave handler
extern void RegisterTutorialBaseBatch();   // Tutorial gap: ExternalPtr-array serialize (5) + Toolbar resolve + Check data helpers (5)
extern void RegisterGameLeafBatch();       // Game gap: closure-safe leaf dtors/teardown + terrain reimpls
extern void RegisterPlayerDragBatch();     // Player wave (drag): FindDescription/sounds/CloseDialog/ClearPathEntities (6)
extern void RegisterPlayerPlaceBatch();    // Player wave (place): FlipVariant (1) + UpdateHierarchy dispatch slot
extern void RegisterPlayerEditBatch();     // Player wave (edit): EntityAtCell/AvgTerrainHeight/FencedAnchorCalc (3)
extern void RegisterPlayerPathBatch();     // Player wave (path): RemovalTool checks (3) + FindComponent/state/f5d0 deps (4)
extern void RegisterPlayerTileBatch();     // Player wave r2 (tile): 003870 SetTransformFromYaw, d580 TileValidity, fd650 ValidateCreate
extern void RegisterPlayerRayBatch();      // Player wave r2 (ray): aa530 TerrainMarch, 1005a0 GetMouseRay, 101270 ScreenToCell, fda00 PlacementPos
extern void RegisterPlayerTradeBatch();    // Player wave r2c (trade): EditTool 66800 + EntityUI 666b0 + AVL tree family (102e30/2c690/25740/25920/15000/14ed0)
extern void RegisterPlayerToolUIBatch();   // Player wave r2c (toolui): OpenToolDialog ff9e0 + child-dialog vtable 0x14015f5a0
extern void RegisterPlayerScreenRectBatch();// Player wave r2d (screenrect): 100380 entity screen-rect + ec70/098210/fe960 (Math via ResolveExport)
extern void RegisterPlayerHighlightBatch(); // Player wave 5 (highlight): 55430 HighlightAdd -> b0d80 -> ace40 tessellation + BST family (TIER-2 geometry, worker-thread-free)

static void RegisterCodeFunctions() {
    // Each entry maps a Ghidra address to our reimplemented function.
    // 349 unique code addresses to fill (0x140001xxx - 0x14014Fxxx range).
    // These are exe-internal functions called by code/Game/ via function pointers.

    // Component registration wave — 4 batches of exe helper reimpls (Component module).
    RegisterComponentBatch_people();
    RegisterComponentBatch_economy();
    RegisterComponentBatch_nature();
    RegisterComponentBatch_visuals();
    // Phase-1 self-contained-sim live bring-ups (fruit/spawn/tombstone). The spawn
    // batch self-gates on BANISHED_LIVE_SPAWN (its type-id globals are also read by
    // other null-guarded consumers, so env-off must publish nothing).
    RegisterComponentBatch_fruit();
    RegisterComponentBatch_spawn();
    RegisterComponentBatch_tombstone();
    // Component GAP wave — residual-73 closure-safe helpers + caller-bug fixes.
    RegisterComponentGap_A();
    RegisterComponentGap_B();
    RegisterComponentGap_C();
    RegisterComponentGap_D();

    // Work registration wave — shared base helpers (FindComponent family + tree free).
    RegisterWorkBaseBatch();
    // Work simple-BST (AVL) family — insert/find/remove/serialize.
    RegisterWorkBstBatch();
    // Work cell-BST (AVL) family — ClearArea insert/remove/serialize.
    RegisterWorkCellBstBatch();
    // Work grid / assignment helpers — FindCell / TryAssignWorkplace / AutoPickup.
    RegisterWorkGridBatch();
    // Work shared-helper tail — FindComponent accessors (5380/5440/22520/6330) + sqrtf (132010).
    RegisterWorkAccessorBatch();
    RegisterWorkImpl2Batch();
    RegisterWorkWalkposBatch();   // walkable-position search cluster (122be0/122b00/f6ef0)
    RegisterWorkLeafBatch();      // 17 closure-safe per-class job leaves (clear/market/gather/hunt)
    // Wave 3 (Action) — closure-safe subsets: CreateNaturalResource cluster (10) +
    // Hunt/RemoveAnimal/CompleteRoad (6). Move/CompleteTask subsets land in a follow-up.
    RegisterActionCnrBatch();
    RegisterActionHuntBatch();
    RegisterActionMoveBatch();   // MoveTo facing/find closure-safe 4 (+ 3 Vec dispatch slots above)
    RegisterActionCompleteBatch(); // CompleteTask BST 8760/8800/8e30 + ShowTool 1173b0/1176e0 (closure-safe 5)
    RegisterActionRawMatBatch();   // raw-material batch E: 6 closure-safe leaves (storage/array/find)
    RegisterPfAvlBatch();          // pathfinding closure Tier 0 — AVL/BST pool+rotations
    RegisterPfPoolsBatch();        // pathfinding closure Tier 0 — A* node-pools + array resizes
    RegisterPfGeomBatch();         // pathfinding closure Tier 0 — region/geometry/heuristic leaves
    RegisterPfCtorsBatch();        // pathfinding closure Wave 2 — Action ctors + entity/storage
    RegisterActionChildCtorsBatch(); // Phase 2 Keystone B — 5 missing action child-ctors
    RegisterWorkActionCtorBatch();   // Phase 2 Keystone B2 — 4 Work action-ctors
    RegisterActionChildCtors2Batch();// Phase 2 Keystone B3 — leaf copy-ctors + consume-list
    RegisterCitizenAICandidateList();// Phase 3 Keystone D Brick 3 — CitizenAI candidate-selection
    RegisterCitizenAIHandlers();     // Phase 3 Keystone D Brick 3b — CitizenAI state handlers (Wander+)
    RegisterCitizenAIFindTarget();   // Phase 3 Keystone D Brick 3b — CitizenAI find-best-target (dd30+78c0)
    RegisterCitizenAIActionCtors();  // Phase 3 Keystone D Brick 3b — CitizenAI action-ctor family (9 ctors)
    RegisterCitizenAIExecutor();     // Phase 3 Keystone D Brick 4b — action executor + move-tick
    RegisterCitizenAIHandlers2();    // Phase 3 Keystone D Brick 3b — handler wave 2 (Education/SeekHappiness/FindWork)
    RegisterCitizenAIFindHelpers();  // Phase 3 Keystone D Brick 3b — deep find-helpers (2ac30/db20/70b0/ec70/e370)
    RegisterCitizenAIHandlers3();     // Phase 3 Keystone D Brick 3b — handler wave 3 (Work/Adult/Elder)
    // Brick 3b-W4 (remaining 7 handlers) — UN-GATED 2026-06-20 (task #33 landed).
    // These handlers (Leisure/Gather/Gather2 are in the fresh-citizen candidate set) drive the
    // map flood-fill pathfinder (Pf_ZoneRefresh_9c480 -> Pf_FloodFillRegion_9c170) and a MoveTo
    // action-build for the FIRST time at runtime. That earlier surfaced TWO standalone bugs, now
    // FIXED (NOT the originally-suspected A* open-list AVL — that diagnosis was an addr2line
    // image-base error): (1) Pf_FloodFillRegion_9c170 never called its own EnsureFns(), so the
    // node-pool alloc s_fn_FloodAlloc(0x14001a9b0) stayed null and the BFS enqueued a null queue
    // node -> head-deref fault; (2) Entity_FindComponent_Family read the unfilled CharacterComponent
    // type-id global (DAT_1401c7078==0xFFFFFFFF) -> null entity IBP -> MoveTo ctor null-deref. Both
    // fixed in code/Game/Action/action_pf_astar.cpp + action_base_move.cpp. A/B GREEN: citizens now
    // select + ENTER Leisure and push a MoveTo crash-free; default (citizen-live OFF) byte-unchanged.
    RegisterCitizenAIGather();        // Gather/Gather2 (+12ab00)
    RegisterCitizenAIGrowUp();        // GrowUp + LongTermPlan (+f230)
    RegisterCitizenAILeisure();       // Leisure (+99d0/fd50/a720)
    RegisterCitizenAIHousingIdle();   // FindHousing/Idle + AVL subtree
    RegisterWorkActBatch();          // Phase 2 Keystone B3 — 5 Work action-queue builders
    RegisterWorkTailTradeBatch();    // Phase 2 Keystone B tail — TradeWork AVL serialize (c8a0/c710/c830)
    RegisterWorkTailFireBatch();     // Phase 2 Keystone B tail — FireWork spiral search (881d0/88730)
    RegisterWorkTailSerializeBatch();// Phase 2 Keystone B tail — PastureWork Slot4 (f2370) registration
    RegisterKeystoneAZoneBatch();    // Phase 2 Keystone A — zone+road enqueue cluster
    RegisterKeystoneARestreeBatch(); // Phase 2 Keystone A — residence RB-tree cluster
    RegisterKeystoneAMapBatch();     // Phase 2 Keystone A — map overlay/init cluster
    RegisterKeystoneAGameDtorBatch();// Phase 2 Keystone A — Game::Dtor + worker-thread teardown
    RegisterPfRegionBatch();       // pathfinding closure Wave 2 — region-graph build/merge/flood-fill
    RegisterPfRoadBatch();         // pathfinding closure Wave 2 — road-mesh + geometry wrappers
    RegisterPfAstarBatch();        // pathfinding closure Wave 2 — A* core + thread dispatcher
    RegisterPfRootsRoadBatch();    // Wave 3 roots — road-edit (9c480/9f2f0/9e0e0/9c640)
    RegisterPfRootsRouteBatch();   // Wave 3 roots — routing/storage (9f810/12cd80/12d5d0/8d590)
    RegisterPfRootsDropBatch();    // Wave 3 roots — dropoff + RemoveAnimal ctor
    RegisterPfRootsMiscBatch();    // Wave 3 roots — bd40 / 92260 / 122340

    // Entity / Goal / Dialog gap-closure waves (6 + 2 + 2 addrs).
    RegisterEntityBatch();         // EntityList PreUpdate/PostUpdate/RemoveEntity/Serialize/etc.
    RegisterGoalBatch();           // StatGoal ExternalPtr resolver (265b0) + StatValueDispatch (da4e0)
    RegisterDialogBatch();         // WorkplaceUI FindComponent (d6c0) + AutoSaveDialog handler (5dec0)
    RegisterTutorialBaseBatch();   // Tutorial gap -> 0: serialize family (26fb0/27ad0/27de0/28cf0/28f40/29190) + Check helpers (92010/910a0/13920/12e5c0/12fbb0)

    // Player registration wave (round 1) — closure-safe tool sub-helpers.
    RegisterPlayerDragBatch();     // 5160 FindDescription, ff980 CloseDialog, ffbf0/1000f0/100140 sounds, 1018d0 ClearPathEntities
    RegisterPlayerPlaceBatch();    // fd960 FlipVariant (+ 0x140152888 UpdateHierarchy dispatch slot)
    RegisterPlayerEditBatch();     // a0280 EntityAtCell, 101730 AvgTerrainHeight (leaf), dbf90 FencedAnchorCalc
    RegisterPlayerPathBatch();     // 104670/104770/1048a0 RemovalTool checks + 5780/3ea0/11e710/9f5d0 deps (bottom-up)
    RegisterPlayerTileBatch();     // r2 tile: 003870 + d580 + fd650 (closure complete via prior waves)
    RegisterPlayerRayBatch();      // r2 ray: aa530 + 1005a0 + 101270 + fda00 (bottom-up; 1005a0 resolves 4 exports locally)
    RegisterPlayerTradeBatch();    // r2c trade: 66800 EditTool + 666b0 EntityUI + AVL family (retrace disasm-verified, typo 2e30/c690->102e30/2c690 caught)
    RegisterPlayerScreenRectBatch();// r2d: 100380 EditTool entity screen-rect + ec70/098210/fe960
    //   (value-return Math ops resolved via ResolveExport; 098210 2-deref display + projMat@+0xc8
    //   disasm-verified; EditTool-private so no startup/shared-vtable risk).
    RegisterPlayerHighlightBatch();// wave 5: 55430 HighlightAdd closure (b0d80 slot-mgr + ace40
    //   radius tessellation + BST family 1f6b0/1fc00/4b40/1f850/14b00). Worker-thread-free;
    //   shares AVL rotations 15000/14ed0 (reg_player_trade) + fa80/1f8d0/ee40/acc10/ae180 (pf waves).
    RegisterPlayerToolUIBatch();   // r2c toolui — NO-OP (DEFERRED): ff9e0 + shared vtable
    //   0x14015f5a0 can't be registered without crashing the menu at startup (the menu's
    //   BuildId child in mainmenudialog.cpp assumes that shared vtable resolves null).
    //   See reg_player_toolui.cpp header. Kept wired as a no-op for when the fix lands.

    // Game registration wave — closure-safe leaf helpers (dtors/teardown + terrain).
    RegisterGameLeafBatch();       // String-array dtors 1b030/1b0a0/a8a0, MusicPlayer 18220,
    //   ImageCapture 1024fc0-dtor a4fc0, Weather 93eb0, MenuScene array 1b860, terrain 206b0/20870.
    //   All shutdown/teardown-path (dormant on autostart) -> additive, no world-build regression.

    // Game/KeyMap container ctor — FUN_140001050 (0xe0 bytes, sets g_pKeyMapSingleton)
    RegisterExeAddr(0x140001050, reinterpret_cast<uintptr_t>(&Game_Ctor));

    // M5 — PlayerController CalcTransform/UpdateCamera + the in-game loop's .rdata
    // float constants (1.0 / audio-time-scale / camera params). Without these,
    // gameupdateloop.cpp's EnsureInit resolves them to NULL and Update/Draw fault.
    RegisterGameUpdateLoopLoadAddrs();

    // M5 5c — terrain height/world-pos query (camera, tile classifier, ~100 component sites).
    RegisterMapQueryAddrs();

    // M5 5c Foundation B — ECS spawn primitives. Registering these makes
    // fn_EntityFactory_CreateEntity (0x1400c3720) and fn_EntityList_RegisterEntity
    // (0x1400c48a0) resolve to our reimpls (EngineFuncsInit runs after this).
    RegisterExeAddr(0x1400c3720, reinterpret_cast<uintptr_t>(&ComponentEntity_CtorFromDescription));
    RegisterExeAddr(0x1400c48a0, reinterpret_cast<uintptr_t>(&EntityList_RegisterEntity_Impl));

    // In-game HUD bring-up — route the exe ToolbarDialog ctor (FUN_1400b9910) to
    // our reimpl so GameUpdateLoop::Startup can build the toolbar from the
    // preloaded Dialog/Toolbar.rsc. EngineFuncsInit (runs after this) then
    // resolves fn_ToolbarDialog_Ctor to ToolbarDialog_Ctor_Impl.
    RegisterExeAddr(0x1400b9910, reinterpret_cast<uintptr_t>(&ToolbarDialog_Ctor_Impl));

    // ============================================================
    // M3.5 — MenuScene::Serialize generic helpers
    // ============================================================
    RegisterExeAddr(0x1400b00e0, reinterpret_cast<uintptr_t>(&Reimpl_Geometry_Init));
    RegisterExeAddr(0x1400988c0, reinterpret_cast<uintptr_t>(&Reimpl_Transform_SerializeWrapper));
    RegisterExeAddr(0x14001baf0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride20));
    RegisterExeAddr(0x140001bb0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride08));
    RegisterExeAddr(0x14001bf10, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride14));
    RegisterExeAddr(0x14001bbf0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Serialize_Stride20));
    RegisterExeAddr(0x14001bcc0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Serialize_External_GraphicsMesh));
    RegisterExeAddr(0x14001bff0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Serialize_External_Particle));
    RegisterExeAddr(0x14001b8e0, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_GraphicsMesh));
    RegisterExeAddr(0x1400196a0, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_Weather));
    // Object::ExternalPtr<StringTable>::Serialize (fn_ExternalPtr_Serialize2).
    // EVERY generator's (+0x20) ExternalPtr field serializes through this; if
    // unregistered it is NULL so the generators skip the field and the element
    // stream cursor mis-aligns. Body in code/Game/Game/serialize_externals.cpp.
    RegisterExeAddr(0x14000bf40, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_StringTable));

    // ============================================================
    // M3.5 Path B session 2 — 0x60B struct array lifecycle helpers
    // ============================================================
    RegisterExeAddr(0x14001f250, reinterpret_cast<uintptr_t>(&Reimpl_Struct60B_Resize));
    RegisterExeAddr(0x14001f5e0, reinterpret_cast<uintptr_t>(&Reimpl_Struct60B_CopyCtor));
    RegisterExeAddr(0x14001fb80, reinterpret_cast<uintptr_t>(&Reimpl_Struct60B_DtorRange));

    // ============================================================
    // M3.5 Path B session 3 — typed-ExternalPtr serialize helper (0x140007140).
    // Body byte-identical to FUN_14001b8e0 (helper #9, GraphicsMesh) except
    // the typeInfo source slot — reads BaseTypeInfo* from .rdata slot
    // 0x140151350 (Graphics::MaterialInstance).
    // ============================================================
    RegisterExeAddr(0x140007140, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_Typed_7140));

    // Scenario's three typed ExternalPtr serializers (menu-loader factory cascade).
    // Same body as 7140; without these Scenario_Serialize skips its +0x70/+0x78/
    // +0x80 ExtPtr fields and the loader stream misaligns -> crash.
    RegisterExeAddr(0x140019ec0, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_MapGenerator));
    RegisterExeAddr(0x14001a0c0, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_TerrainGenerator));
    RegisterExeAddr(0x1400194a0, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_ClimateGenerator));

    // ============================================================
    // M3.5 Path B session 5 — additional Geometry::Serialize resize helpers
    // (uint16, int4, array-of-arrays, mesh-vertex 16B, int)
    // ============================================================
    RegisterExeAddr(0x14001c890, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride02));
    RegisterExeAddr(0x14001efd0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride16A));
    RegisterExeAddr(0x14001f0a0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_ArrayOfArrays));
    RegisterExeAddr(0x14001ef10, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride16B));
    RegisterExeAddr(0x1400021d0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride04));
    // NameDatabase::Serialize array resizers (code/Game/Game/namedatabase.cpp)
    RegisterExeAddr(0x14001c7c0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride0C));
    RegisterExeAddr(0x14000a590, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Byte));

    // ============================================================
    // M3.5 Path B session 4 — per-element 0x60B serialize + typed-ExternalPtr
    // array serialize (Geometry::Serialize cascade points 10 + 13).
    //   FUN_14001ffa0 — per-element 0x60B serialize (calls FUN_140007140 inside)
    //   FUN_140020150 — typed-ExternalPtr array serialize (Graphics::MaterialInstance,
    //                   typeInfo slot 0x140151350)
    // ============================================================
    RegisterExeAddr(0x14001ffa0, reinterpret_cast<uintptr_t>(&Reimpl_Struct60B_Serialize));
    RegisterExeAddr(0x140020150, reinterpret_cast<uintptr_t>(&Reimpl_Array_Serialize_External_MaterialInstance));

    // ============================================================
    // M3.5 Path B session 6 — heightmap mesh-build path helpers
    //   sub_c1e0     — top-level mesh builder (allocates Vertex/IndexBuffers)
    //   FUN_ac880    — per-cell vertex compute
    //   helper_e270  — vertex normal compute via heightmap deltas
    // ============================================================
    RegisterExeAddr(0x1400ac1e0, reinterpret_cast<uintptr_t>(&Reimpl_TradeWork_sub_c1e0));
    RegisterExeAddr(0x1400ac880, reinterpret_cast<uintptr_t>(&Reimpl_FUN_1400ac880));
    RegisterExeAddr(0x1400ae270, reinterpret_cast<uintptr_t>(&Reimpl_TradeWork_helper_e270));

    // ============================================================
    // M3.5 — Geometry chunk-streaming (per-frame dirty-chunk upload)
    //   0x1400aad50 — body (called via JMP thunk from vtable +0x08)
    //   0x1400aed40 — JMP thunk (the actual vtable target)
    // The vtable slot in g_Geometry_Vtable_Standalone already routes to
    // GeomDraw_ChunkUpdate -> Reimpl_GeometryChunkUpdate, so registering
    // these addresses is for any out-of-band callers using ResolveAddr.
    // ============================================================
    RegisterExeAddr(0x1400aad50, reinterpret_cast<uintptr_t>(&Reimpl_GeometryChunkUpdate));
    RegisterExeAddr(0x1400aed40, reinterpret_cast<uintptr_t>(&Reimpl_GeometryChunkUpdate));

    // ============================================================
    // M3.5 Path B session 7 — image-buffer heightmap upload helper
    //   sub_c780     — alloc ImageBuffer, set NoiseUI vtable, Create + upload
    //                  (heightmap raw bytes -> Video texture)
    // ============================================================
    RegisterExeAddr(0x1400ac780, reinterpret_cast<uintptr_t>(&Reimpl_TradeWork_sub_c780));

    // ============================================================
    // M3.5 Path B session 8 — Geometry::Serialize body + version-gated extra
    //   impl_04d0    — main 15-step Geometry::Serialize body (master schema, v6)
    //   impl_f1a0    — version-gated extra serialize (parallel schema, v4)
    //
    // Both walk the helper graph established in sessions 2-7 and call into
    // the IO::Serializer dispatch slots (lazy-resolved within the reimpl).
    // ============================================================
    RegisterExeAddr(0x1400b04d0, reinterpret_cast<uintptr_t>(&Reimpl_TradeWork_impl_04d0));
    RegisterExeAddr(0x1400af1a0, reinterpret_cast<uintptr_t>(&Reimpl_TradeWork_impl_f1a0));
}

// ============================================================
// Section 2: IAT DISPATCH SLOTS
// ============================================================
// The original exe has .rdata slots at 0x140151xxx-0x140152xxx that hold
// pointers to Runtime DLL functions (filled by the exe's custom loader).
// In standalone mode, we allocate our own static slots and fill them with
// the Runtime DLL function pointers resolved via ResolveExport/ResolveOrdinal.
//
// Each slot is a uintptr_t that code/Game/ reads via:
//   slot = ResolveAddr(Module::Exe, 0x14015XXXX);
//   fn = *reinterpret_cast<fn_t*>(slot);
//
// So we need to provide a real memory address containing the function pointer.

// Storage for IAT dispatch slot values.
// Each slot is a function pointer that code/Game/ dereferences.
#include <unordered_map>

static std::unordered_map<uintptr_t, uintptr_t>& GetSlotStorage() {
    static std::unordered_map<uintptr_t, uintptr_t> slots;
    return slots;
}

// Register a dispatch slot: the registry maps ghidra_addr -> &slot_storage[ghidra_addr],
// and slot_storage[ghidra_addr] holds the actual function pointer.
static void RegisterDispatchSlot(uintptr_t ghidra_addr, uintptr_t fn_ptr) {
    auto& slots = GetSlotStorage();
    slots[ghidra_addr] = fn_ptr;
    // Register the ADDRESS of our slot storage (not the fn ptr itself),
    // because code reads: fn = *(fn_t*)ResolveAddr(Module::Exe, addr)
    RegisterExeAddr(ghidra_addr, reinterpret_cast<uintptr_t>(&slots[ghidra_addr]));
}

static void RegisterDispatchSlots() {
    // Each entry maps an exe .rdata dispatch slot to a Runtime DLL function.
    // EngineFuncsInit and Description reimpls read these via:
    //   fn = *(fn_t*)ResolveAddr(Module::Exe, 0x14015XXXX);
    RegisterDispatchSlot(0x1401518c0,
        (uintptr_t)ResolveExport("?GetFileVersion@ElementDescription@UI@@UEBAHXZ"));
    RegisterDispatchSlot(0x1401518f8,
        (uintptr_t)ResolveExport("?Draw@Element@UI@@UEBAXAEAVSpriteContext@2@E@Z"));
    RegisterDispatchSlot(0x140151908,
        (uintptr_t)ResolveExport("?AddLine@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHI@Z"));
    RegisterDispatchSlot(0x140151900,
        (uintptr_t)ResolveExport("?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@HHHH@Z"));
    RegisterDispatchSlot(0x140151910,
        (uintptr_t)ResolveExport("?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@AEBV34@@Z"));
    RegisterDispatchSlot(0x140151238,
        (uintptr_t)ResolveExport("?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z"));
    // Object::Type::Serialize — slot 0x1401516b0. Used by UIDescription (entity
    // element +0x00), ConsumeProduce, SendComponentMessage, ConsumeProduceDescription.
    // UNREGISTERED previously → those reimpls' `if (s_fn_Type_Serialize)` guard
    // silently SKIPPED the per-element Type read → stream drift → bogus String
    // length → CompressedStream::Read zero-progress spin on Dialog\StandardDialog.rsc
    // (menu UIDescriptions have count==0 so never hit it). The mangled name is the
    // exe's own import string at the slot's RVA (0x1401b8d2e).
    RegisterDispatchSlot(0x1401516b0,
        (uintptr_t)ResolveExport("?Serialize@Type@Object@@QEAAXAEAVSerializer@IO@@@Z"));
    RegisterDispatchSlot(0x1401518c8,
        (uintptr_t)ResolveExport("?Serialize@ElementDescription@UI@@UEAA_NAEAVStream@IO@@@Z"));
    RegisterDispatchSlot(0x1401518b8,
        (uintptr_t)ResolveExport("??1ElementDescription@UI@@UEAA@XZ"));
    RegisterDispatchSlot(0x1401518b0,
        (uintptr_t)ResolveExport("??0ElementDescription@UI@@QEAA@XZ"));
    RegisterDispatchSlot(0x1401518d0,
        (uintptr_t)ResolveExport("?CreateInstance@ElementDescription@UI@@QEBAXV?$InternalPtr@VElement@UI@@@Object@@@Z"));
    RegisterDispatchSlot(0x1401518d8,
        (uintptr_t)ResolveExport("??0Element@UI@@QEAA@XZ"));
    RegisterDispatchSlot(0x1401518e0,
        (uintptr_t)ResolveExport("??1Element@UI@@UEAA@XZ"));
    RegisterDispatchSlot(0x1401518e8,
        (uintptr_t)ResolveExport("?ComputeLayout@Element@UI@@UEAAXHHHH@Z"));
    RegisterDispatchSlot(0x1401518f0,
        (uintptr_t)ResolveExport("?GetAlignedPosition@Element@UI@@SAXW4Alignment@ElementDescription@2@HHHHHHHHHHAEAH111_N2@Z"));
    RegisterDispatchSlot(0x140152400,
        (uintptr_t)ResolveExport("?SerializeBytes@Serializer@IO@@QEAAXHPEAXPEBD@Z"));

    // ----------------------------------------------------------------
    // M3.5 cleanup — IO::Serializer dispatch slots (Geometry::Serialize)
    // ----------------------------------------------------------------
    // Slots verified against fn_TradeWork_impl_04d0 (0x1400b04d0) call sites:
    //   ctor  takes (this, Stream&, version)        — single-arg, RDX/R8 set by caller
    //   dtor  takes (this)                          — single-arg
    //   Int/Bool/Byte/Float take (this, T*, name)
    //   UShort/Short/Byte arrays take (this, count, T*, name)
    //   GetVersion takes (this) -> int
    // Without these, terrain heightmap soft-fails and the menu renders empty.
    RegisterDispatchSlot(0x140152ad8,
        (uintptr_t)ResolveExport("??0Serializer@IO@@QEAA@AEAVStream@1@H@Z"));
    RegisterDispatchSlot(0x140152ad0,
        (uintptr_t)ResolveExport("??1Serializer@IO@@QEAA@XZ"));
    RegisterDispatchSlot(0x140152ac8,
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAHPEBD@Z"));
    RegisterDispatchSlot(0x140152410,
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAEPEBD@Z"));
    RegisterDispatchSlot(0x140152a58,
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEA_NPEBD@Z"));
    RegisterDispatchSlot(0x140152a60,
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAMPEBD@Z"));
    RegisterDispatchSlot(0x140152a48,
        (uintptr_t)ResolveExport("?GetVersion@Serializer@IO@@QEBAHXZ"));
    RegisterDispatchSlot(0x140152488,
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXHPEAGPEBD@Z"));
    RegisterDispatchSlot(0x1401523f8,
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXHPEAFPEBD@Z"));
    RegisterDispatchSlot(0x140152618,
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXHPEAEPEBD@Z"));
    // NameDatabase::Serialize (0x1400a45e0) dispatch slots — distinct .rdata
    // slots that resolve to the same IO::Serializer overloads as above.
    RegisterDispatchSlot(0x140152478,   // SerializeBytes(count, void*, name) — array1 field0
        (uintptr_t)ResolveExport("?SerializeBytes@Serializer@IO@@QEAAXHPEAXPEBD@Z"));
    RegisterDispatchSlot(0x140152418,   // Serialize(uchar&) — array1 field1
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAEPEBD@Z"));
    RegisterDispatchSlot(0x140152a50,   // Serialize(uint&) — array1 field2
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAIPEBD@Z"));
    // Wave 3 (Action) — Math::Vector dispatch slots used by the MoveTo facing helpers
    // (action_base_move.cpp SetFacingDirection 0x14010a060 / UpdateFacing 0x1400c8d20).
    RegisterDispatchSlot(0x1401512c0,   // Math::Vector3::Normalize(this) -> Vector3
        (uintptr_t)ResolveExport("?Normalize@Vector3@Math@@QEBA?AV12@XZ"));
    RegisterDispatchSlot(0x1401512d8,   // Math::Vector3::Heading(this) -> float
        (uintptr_t)ResolveExport("?Heading@Vector3@Math@@QEBAMXZ"));
    RegisterDispatchSlot(0x1401526b0,   // Math::Vector2::Normalize(this) const -> Vector2 (by value)
        // Value-return member ABI: RCX=this, RDX=hidden retbuf. The exe only exports the
        // CONST value-return form; the prior "QEAAAEAV12" (non-const ref-return) name does
        // NOT exist -> ResolveExport returned null -> slot null -> null-call crash in MoveTo.
        (uintptr_t)ResolveExport("?Normalize@Vector2@Math@@QEBA?AV12@XZ"));
    // Math::Transform::operator*(Vector3 const&) const -> Vector3 (member value-return ABI:
    // RCX=this, RDX=retVec3, R8=inVec3). Used by the Work walkable-position search
    // (work_base_walkpos.cpp 0x140122be0) and the PF-ROAD cell-quad helpers. Mangled name
    // is the exe's import string at the slot RVA 0x1401b72xx (see tool_base_screenrect.cpp).
    RegisterDispatchSlot(0x140151470,
        (uintptr_t)ResolveExport("??DTransform@Math@@QEBA?AVVector3@1@AEBV21@@Z"));
    RegisterDispatchSlot(0x140152490,   // Serialize(count, uchar*, name) — array3 bulk
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXHPEAEPEBD@Z"));

    // Phase 3 Keystone D Brick 4b-exec — MoveTo / PlayAnimation ACTION-METHOD dispatch slots.
    // moveto.cpp / playanimation.cpp were written for DLL mode where these exe .rdata IAT slots
    // are pre-filled by the loader. In standalone they must be registered or MoveTo_ResolveFuncs
    // leaves s_fn_* null and the action vtable methods (precheck/execute, run by the move-tick
    // executor c4c50) call through null. Names = the exe's own import strings at each slot RVA.
    RegisterDispatchSlot(0x140152ab0,   // Object::InternalBasePtr::CheckPtr(this) const [protected, IEBA]
        (uintptr_t)ResolveExport("?CheckPtr@InternalBasePtr@Object@@IEBAXXZ"));
    RegisterDispatchSlot(0x140152aa0,   // Object::InternalBasePtr::Serialize(this, Serializer&)
        (uintptr_t)ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z"));
    RegisterDispatchSlot(0x140152458,   // Math::Vector2::Dot(this, other) const -> Scalar
        (uintptr_t)ResolveExport("?Dot@Vector2@Math@@QEBA?AVScalar@2@AEBV12@@Z"));
    RegisterDispatchSlot(0x140152a98,   // Object::Base::IsDerivedFrom(this, typeId) const -> bool
        (uintptr_t)ResolveExport("?IsDerivedFrom@Base@Object@@QEBA_NI@Z"));
    RegisterDispatchSlot(0x140151268,   // Math::Vector3::Vector3(this, Scalar&, Scalar&, Scalar&)
        (uintptr_t)ResolveExport("??0Vector3@Math@@QEAA@AEBVScalar@1@00@Z"));
    RegisterDispatchSlot(0x140151298,   // Math::Vector3::Dot(this, other) const -> Scalar
        (uintptr_t)ResolveExport("?Dot@Vector3@Math@@QEBA?AVScalar@2@AEBV12@@Z"));

    // Achievement::Goal::Serialize — base serialize for StatGoal/TimeGoal/etc.
    // (virtual, Runtime export `?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z`,
    //  Runtime addr 0x18009ee20). StatGoal_Slot5/TimeGoal read this slot via
    //  ResolveAddr(Module::Exe, 0x140152840) and call it FIRST to consume the base
    //  Goal fields (an External ref at +0x20 + a String at +0x28) from the stream.
    //  Without it, s_fn_GoalSerialize stays NULL and the base call is SKIPPED, so
    //  every subsequent field read is shifted early. For goals whose statA is a
    //  non-empty resource ref (e.g. Achievement/Uneducated_noschool.crs -> stat
    //  "Template\SchoolHouse.rsc"), the misaligned String::Serialize reads the
    //  length word from the wrong offset (high bit clear -> WIDE path) and over-reads
    //  past the 127-byte resource, hanging IO::CompressedStream::Read in a
    //  zero-progress loop — the standalone menu "100% CPU" loader-thread spin.
    {
        uintptr_t goalSerialize =
            (uintptr_t)ResolveExport("?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z");
        if (!goalSerialize)
            goalSerialize = ResolveAddr(Module::Runtime, 0x18009ee20);
        RegisterDispatchSlot(0x140152840, goalSerialize);
    }

    // Bulk int array: Serialize(int count, int* data, name).
    // Used in many places (TradeWork, MoveTo, AutoPickup, etc.) and required
    // for Geometry::Serialize step 11 (+0x220 vertex-int array, count=901
    // observed; without this the cursor was advancing wrong → step 13 read
    // garbage count).
    RegisterDispatchSlot(0x140151278,
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXHPEAHPEBD@Z"));
    // Bulk FLOAT array: Serialize(int count, float* data, name) — HPEAM form.
    // Required by ClimateGenerator::Serialize for its 48-float (+0x44) block.
    RegisterDispatchSlot(0x140151ea8,
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXHPEAMPEBD@Z"));
    // System::Filename ctor/dtor — used by Terrain/Map generator Serialize for
    // the stack-temp Filename wrappers around filename String reads.
    RegisterDispatchSlot(0x140151d40,
        (uintptr_t)ResolveExport("??0Filename@System@@QEAA@XZ"));
    RegisterDispatchSlot(0x1401529e8,
        (uintptr_t)ResolveExport("??1Filename@System@@QEAA@XZ"));
    RegisterDispatchSlot(0x1401526b8,
        (uintptr_t)ResolveExport("?Serialize@Vector2@Math@@QEAAXAEAVSerializer@IO@@@Z"));
    // Math::Vector3::Serialize — required by Weather::Serialize (velocityDir at
    // weather+0x2C). Without it, Weather_Serialize null-derefs s_fn_Vector3_Serialize
    // when MenuScene's +0x80 Weather extPtr loads (intermittent: only on scenes
    // that reference an Environment/*.rsc).
    RegisterDispatchSlot(0x140152a68,
        (uintptr_t)ResolveExport("?Serialize@Vector3@Math@@QEAAXAEAVSerializer@IO@@@Z"));
    // MenuScene Slot5 atmospheric block — Vector4 + Color serialize.
    // Slots used at exe 0x1400a3796 (Vector4) and 0x1400a37aa..1400a37c6 (Color x3).
    // Without these, the post-Geometry tail of MenuScene::Serialize hits a
    // NOT_IMPLEMENTED dispatch slot and the menu-scene fog/sky/sun colors
    // stay at MenuScene_Init defaults (or zero for sun yaw/pitch).
    RegisterDispatchSlot(0x140151ed0,
        (uintptr_t)ResolveExport("?Serialize@Vector4@Math@@QEAAXAEAVSerializer@IO@@@Z"));
    RegisterDispatchSlot(0x140151ee0,
        (uintptr_t)ResolveExport("?Serialize@Color@System@@QEAAXAEAVSerializer@IO@@@Z"));
    RegisterDispatchSlot(0x140152668,
        (uintptr_t)ResolveExport("?Create@VertexBuffer@Video@@QEAA_NH_NPEAX@Z"));
    RegisterDispatchSlot(0x140152690,
        (uintptr_t)ResolveExport("?Set@VertexBuffer@Video@@QEAA_NHHPEAX@Z"));
    RegisterDispatchSlot(0x140152660,
        (uintptr_t)ResolveExport("?Create@IndexBuffer@Video@@QEAA_NH_NPEAX@Z"));
    RegisterDispatchSlot(0x140152698,
        (uintptr_t)ResolveExport("?Set@IndexBuffer@Video@@QEAA_NHHPEAX@Z"));

    // ----------------------------------------------------------------
    // Foundation F — Graphics render-path dispatch slots (ModelComponent).
    // ----------------------------------------------------------------
    // The real (non-inert) ModelComponent constructs a Graphics::MeshInstance
    // sub-object at +0x28 and a Math::Transform at +0x148, then its lifecycle
    // methods (Startup=+0x20 sets the mesh from the description, OnAdded=+0x30
    // registers the geometry in Graphics::Scene) drive rendering. All of these
    // are Runtime-x64.dll exports; the exe reaches them through these .rdata
    // dispatch slots, which the loader fills by name at startup — in standalone
    // we fill them here. Names recovered from the exe's slot->name RVA table.
    RegisterDispatchSlot(0x140151490,                       // MeshInstance ctor
        (uintptr_t)ResolveExport("??0MeshInstance@Graphics@@QEAA@XZ"));
    RegisterDispatchSlot(0x1401514a8,                       // MeshInstance dtor
        (uintptr_t)ResolveExport("??1MeshInstance@Graphics@@UEAA@XZ"));
    RegisterDispatchSlot(0x140151498,                       // MeshInstance::SetMesh
        (uintptr_t)ResolveExport("?SetMesh@MeshInstance@Graphics@@QEAAXV?$ExternalPtr@VGraphicsMesh@Graphics@@@Object@@@Z"));
    RegisterDispatchSlot(0x1401514b0,                       // MeshInstance::IsAnimating
        (uintptr_t)ResolveExport("?IsAnimating@MeshInstance@Graphics@@QEBA_NXZ"));
    RegisterDispatchSlot(0x140152990,                       // MeshInstance::Serialize
        (uintptr_t)ResolveExport("?Serialize@MeshInstance@Graphics@@QEAAXAEAVSerializer@IO@@@Z"));
    RegisterDispatchSlot(0x140151328,                       // Math::Transform ctor
        (uintptr_t)ResolveExport("??0Transform@Math@@QEAA@XZ"));
    RegisterDispatchSlot(0x1401522e0,                       // Math::Transform::Serialize
        (uintptr_t)ResolveExport("?Serialize@Transform@Math@@QEAAXAEAVSerializer@IO@@@Z"));
    RegisterDispatchSlot(0x1401512e0,                       // Geometry::SetTransform
        (uintptr_t)ResolveExport("?SetTransform@Geometry@Graphics@@QEAAXAEBVTransform@Math@@@Z"));
    RegisterDispatchSlot(0x140151370,                       // Scene::AddObject
        (uintptr_t)ResolveExport("?AddObject@Scene@Graphics@@QEAAIPEAVGeometry@2@@Z"));
    RegisterDispatchSlot(0x140151360,                       // Scene::RemoveObject
        (uintptr_t)ResolveExport("?RemoveObject@Scene@Graphics@@QEAAXI@Z"));
    RegisterDispatchSlot(0x140152468,                       // Serializer::Serialize(short*)
        (uintptr_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAFPEBD@Z"));
    RegisterDispatchSlot(0x1401512e8,                       // Random::Rand(range)
        (uintptr_t)ResolveExport("?Rand@Random@System@@QEAAII@Z"));
    RegisterDispatchSlot(0x140152b00,                       // System::Time::Time(float)
        (uintptr_t)ResolveExport("??0Time@System@@QEAA@M@Z"));

    RegisterDispatchSlot(0x140152738,
        (uintptr_t)ResolveExport("?CheckMod@SaveLoad@Save@@QEBAHHH@Z"));
    RegisterDispatchSlot(0x140151650,
        (uintptr_t)ResolveExport("?SetSaveName@SaveLoad@Save@@QEAAXAEBVString@System@@H@Z"));
    RegisterDispatchSlot(0x1401514c8,
        (uintptr_t)ResolveExport("??0Sprite@UI@@QEAA@V?$ExternalPtr@VSpriteSheet@UI@@@Object@@AEBVString@System@@@Z"));
    RegisterDispatchSlot(0x140152b48,
        (uintptr_t)ResolveExport("??0Sprite@UI@@QEAA@XZ"));
    // Object::Internal::Internal — the CTOR slot. MUST be registered alongside the
    // ~Internal dtor slot below: without it, fn_ObjectInternal_Ctor stays NULL, so every
    // dynamically-built object (citizen action stack: MoveTo/PlayAnimation/
    // SendComponentMessage) skips Object::Internal registration and its registry index
    // (+0x08) is left as uninitialized heap garbage. ~Internal (the dtor, which IS
    // resolved) then does a swap-remove using that garbage index -> OOB write into the
    // InternalFactory registry array -> crash when c5400 tears the action stack down.
    // (World-build entities are unaffected: they're built by the real exe factories that
    //  call the real Object::Internal::Internal directly at load.)
    RegisterDispatchSlot(0x140152768,
        (uintptr_t)ResolveExport("??0Internal@Object@@QEAA@XZ"));
    RegisterDispatchSlot(0x140152770,
        (uintptr_t)ResolveExport("??1Internal@Object@@UEAA@XZ"));
    RegisterDispatchSlot(0x140152a30,
        (uintptr_t)ResolveExport("??0Vector3@Math@@QEAA@MMM@Z"));

    // Application::Application::_singleton — Runtime data variable holding the
    // Application*. The exe normally has a .rdata exref at 0x140152368 that
    // points to this Runtime variable. engine_funcs.cpp reads:
    //   exref       = ResolveAddr(Exe, 0x140152368)         -> our slot storage
    //   *exref      = ResolveAddr(Runtime, 0x1801fc678)     -> &Application::_singleton
    //   g_pAppSingleton = *exref                             -> &Application::_singleton
    //   *g_pAppSingleton                                    -> Application*
    // Without this registration MainMenuDialog::OnCommand can't reach App::Quit.
    RegisterDispatchSlot(0x140152368,
        ResolveAddr(Module::Runtime, 0x1801fc678));

    // Object::Factory::_singleton — same pattern.
    // engine_funcs.cpp Phase H reads slot 0x140152760, expecting **slot to
    // be the live Factory*. Without this, fn_Factory_GetTypeIdFromName +
    // singleton are unusable, blocking the type-ID fallback in
    // LoadDialogResource (and many other Factory consumers).
    RegisterDispatchSlot(0x140152760,
        ResolveAddr(Module::Runtime, 0x1801fc6e8));

    // UI::Dialog::_typeInfo external — exe slot 0x140151728 in the original
    // is patched by the loader with the **type ID itself** (4-byte uint),
    // NOT a pointer to BaseTypeInfo. Original code reads it as
    // `*(uint32_t*)0x140151728` (single deref), so the slot must contain
    // the typeId directly.
    //
    // Note: Ghidra labels this _typeInfo@DialogInstance based on a heuristic
    // but the actual type checked here is UI::Dialog (Runtime 0x1801faa88) —
    // FUN_140064720 loads Dialog/Follow.rsc and runs IsDerivedFrom against
    // this slot, and the .rsc loads as a UI::Dialog External.
    //
    // Type ID isn't known until Factory::Startup runs; LoadDialogResource has
    // a fallback path that uses fn_Factory_GetTypeIdFromName instead, so we
    // leave this slot zero-filled until something else needs it.
    RegisterDispatchSlot(0x140151728, 0);

    // Object::External::External — dispatch slot for the External base ctor.
    // MenuScene_Init reads:
    //   slot = ResolveAddr(Module::Exe, 0x140151e98);
    //   fn_External_Ctor = *(fn_t*)slot;
    // So the slot must hold a pointer to the Runtime DLL's External::External.
    // Mangled: ??0External@Object@@QEAA@XZ (default ctor, public non-virtual).
    RegisterDispatchSlot(0x140151e98,
        (uintptr_t)ResolveExport("??0External@Object@@QEAA@XZ"));

    // Object::External::~External — paired dtor for the same dispatch pattern.
    // MenuScene_Dtor reads slot 0x140151ea0 the same way.
    RegisterDispatchSlot(0x140151ea0,
        (uintptr_t)ResolveExport("??1External@Object@@UEAA@XZ"));

    // Input::KeyMap exref. Slot stores a level-1 pointer to a "level-2 cell"
    // whose contents are the real KeyMap*. See the comment near
    // g_StandaloneKeyMapPtr above for the reasoning.
    //
    // Order: ResolveAddr(0x140151738) -> &slot_storage (uintptr_t)
    //        *(void**)slot_storage = (void*)&g_StandaloneKeyMapMid  -- level 1
    //        *(void**)&g_StandaloneKeyMapMid = g_StandaloneKeyMapMid
    //                                        = &g_StandaloneKeyMapPtr -- level 2
    //        *(void**)&g_StandaloneKeyMapPtr = g_StandaloneKeyMapPtr
    //                                        = KeyMap* -- level 3
    // Triple deref from the slot address yields the KeyMap, matching the
    // original exe's disasm.
    RegisterDispatchSlot(0x140151738,
        reinterpret_cast<uintptr_t>(&g_StandaloneKeyMapMid));

    // ----------------------------------------------------------------
    // IO::Serializer reimpl wrappers (runtime/src/io/serializer.cpp)
    // ----------------------------------------------------------------
    // These OVERRIDE the Runtime-fn registrations above for the same exe
    // dispatch slots, routing the standalone deserialize through OUR clean,
    // debuggable C++ forwarders. Each wrapper checks BANISHED_REIMPL_SERIALIZER
    // ONCE: UNSET (default) => it tail-calls the resolved Runtime original, so
    // the default build is byte-identical to before. SET to "1" => the clean
    // reimpl runs and can be stepped with source while watching the stream
    // cursor (MemoryStream + 0x24) advance.
    //
    // Registered LAST so the slot map's value for each address is our wrapper.
    RegisterDispatchSlot(0x140152ac8, reinterpret_cast<uintptr_t>(&RtSerializer_Serialize_Int));    // Serialize(int&)
    RegisterDispatchSlot(0x140152a60, reinterpret_cast<uintptr_t>(&RtSerializer_Serialize_Float));  // Serialize(float&)
    RegisterDispatchSlot(0x140152a58, reinterpret_cast<uintptr_t>(&RtSerializer_Serialize_Bool));   // Serialize(bool&)
    RegisterDispatchSlot(0x140152410, reinterpret_cast<uintptr_t>(&RtSerializer_Serialize_UChar));  // Serialize(uchar&)
    RegisterDispatchSlot(0x140152418, reinterpret_cast<uintptr_t>(&RtSerializer_Serialize_UChar));  // Serialize(uchar&) (NameDB alias)
    RegisterDispatchSlot(0x140152a50, reinterpret_cast<uintptr_t>(&RtSerializer_Serialize_UInt));   // Serialize(uint&)
    RegisterDispatchSlot(0x140152a48, reinterpret_cast<uintptr_t>(&RtSerializer_GetVersion));       // GetVersion()
    RegisterDispatchSlot(0x140152488, reinterpret_cast<uintptr_t>(&RtSerializer_Serialize_UShortArr)); // Serialize(cnt,ushort*)
    RegisterDispatchSlot(0x140151278, reinterpret_cast<uintptr_t>(&RtSerializer_Serialize_IntArr));    // Serialize(cnt,int*)
    RegisterDispatchSlot(0x1401523f8, reinterpret_cast<uintptr_t>(&RtSerializer_Serialize_ShortArr));  // Serialize(cnt,short*)
    RegisterDispatchSlot(0x140152490, reinterpret_cast<uintptr_t>(&RtSerializer_Serialize_UCharArr));  // Serialize(cnt,uchar*) (NameDB bulk)
    RegisterDispatchSlot(0x140152400, reinterpret_cast<uintptr_t>(&RtSerializer_SerializeBytes));      // SerializeBytes(size,void*)
    RegisterDispatchSlot(0x140152618, reinterpret_cast<uintptr_t>(&RtSerializer_SerializeBytes));      // SerializeBytes (alt slot)
    RegisterDispatchSlot(0x140152478, reinterpret_cast<uintptr_t>(&RtSerializer_SerializeBytes));      // SerializeBytes (NameDB alias)
}

// ============================================================
// Section 3: VTABLES & TYPE IDS
// ============================================================
// Static vtable arrays for game classes. In standalone mode, WE define
// the vtable contents (pointing to our reimplemented virtual functions).
// Type IDs are simple uint32_t globals used by FindComponent.

// Singleton-pointer storage in .data/.bss. The original exe has 8-byte slots
// for global object pointers (e.g. KeyMap, GameWorld, GameSettings) that get
// written during Game_Ctor / engine boot. We reserve our own static storage
// here and register its address; engine_funcs.cpp dereferences these slots.
static void* g_storage_KeyMapSingleton = nullptr;       // 0x1401c3de8
static void* g_storage_GameWorldSingleton = nullptr;    // 0x1401c3dd8
static void* g_storage_GameSettingsPtr = nullptr;       // 0x1401c3de0

// Defined in standalone/menudialogloopt_stubs.cpp.
extern void RegisterMenuDialogLoopVtable();

// Defined in standalone/splash_loop_stubs.cpp.
extern void RegisterSplashDialogLoopVtable();

// Defined in standalone/loader_loop_stubs.cpp.
extern void RegisterLoaderLoopVtables();

// Defined in standalone/gameupdateloop_stubs.cpp — the in-game simulation loop
// vtable (0x14015e2f0). Built by NewGameDialog::StartGame on "OK".
extern void RegisterGameUpdateLoopVtable();

// ============================================================
// M3.5 Path B session 7 — NoiseUI ImageBuffer vtable scaffolding
// ============================================================
// The original exe stores a 10-slot vtable at 0x14015c8c0 that gets installed
// onto Video::ImageBuffer instances created by NoiseUI / ModDialog / and
// (per session 7) the Geometry::Serialize heightmap upload helper. Each slot
// in the original is a thin JMP qword [0x14015XXXX] thunk that the loader
// fills with Runtime DLL function pointers.
//
// In standalone mode we own the storage: WE allocate a 10-slot array, fill it
// with Runtime exports resolved at boot, and register its address. Code that
// reads `g_NoiseUI_ImageBufferVtable` (which equals `ResolveAddr(Module::Exe,
// 0x14015c8c0)`) gets our backing storage and dispatches normally.
//
// Slot layout (verified via Ghidra disassembly @ 0x14015c8c0):
//   [0] +0x00  Dtor             — original 0x1400129D0 (ModDialog_Slot10)
//   [1] +0x08  TypeName-ish     — JMP [0x140151b20]
//   [2] +0x10  ?                — JMP [0x140151b28]
//   [3] +0x18  ?                — JMP [0x140151ae0]
//   [4] +0x20  ?                — JMP [0x140151240]
//   [5] +0x28  ?                — JMP [0x140151ae8]
//   [6] +0x30  ?                — JMP [0x140151b00]
//   [7] +0x38  ?                — JMP [0x140151b08]
//   [8] +0x40  ?                — JMP [0x140151b10]
//   [9] +0x48  Create           — JMP [0x140151a88]  (?Create@ImageBuffer@Video@@)
//
// Only slots [0] (dtor) and [9] (Create) are dispatched by code we currently
// have running in standalone. Other slots are left null — if the engine ever
// dispatches one and finds it null, the stub will need to be filled with the
// matching Runtime export. Cascade-risk surface documented in the session 7
// report. The Runtime exports for the unknown slots can be discovered via
// Ghidra's `0x14015XXXX` slot pre-loaded values when the Runtime DLL fills
// them at startup; for now the safe-no-op approach mirrors the
// "MenuScene_VtblStub_ReturnNull" pattern used elsewhere.
static void* g_NoiseUI_ImageBufferVtable_Standalone[10] = {0};

// Forward-decl ImageBuffer dtor type — Runtime exports
//   ??1ImageBuffer@Video@@UEAA@XZ
typedef void  (*fn_ImgBuf_Dtor_StandaloneVt_t)(void* self);
typedef bool  (*fn_ImgBuf_Create_StandaloneVt_t)(
    void* self, int imageType, int usage, int format,
    unsigned int mipLevels, int width, int height,
    int filterMode, int addrU, int addrV, int addrW,
    void* mipData);

// Safe no-op stub for slots [1]..[8] until they're individually resolved.
static void* NoiseUIImgBuf_VtblStub_ReturnNull(void* /*self*/, ...) {
    return nullptr;
}

extern "C" void RegisterNoiseUIImageBufferVtable() {
    // Resolve slot [0] = ImageBuffer destructor.
    fn_ImgBuf_Dtor_StandaloneVt_t fnDtor =
        reinterpret_cast<fn_ImgBuf_Dtor_StandaloneVt_t>(
            ResolveExport("??1ImageBuffer@Video@@UEAA@XZ"));
    if (!fnDtor) {
        fnDtor = reinterpret_cast<fn_ImgBuf_Dtor_StandaloneVt_t>(
            ResolveAddr(Module::Runtime, 0x1800f2610));
    }

    // Resolve slot [9] = ImageBuffer::Create — vtable +0x48 in the layout.
    // Mangled name verified via Ghidra:
    //   ?Create@ImageBuffer@Video@@UEAA_NW4ImageType@12@W4Usage@12@
    //     W4Format@12@IHHW4FilterMode@12@W4AddressMode@12@44PEAV?$Array@
    //     V?$Array@E@Collection@@@Collection@@@Z
    fn_ImgBuf_Create_StandaloneVt_t fnCreate =
        reinterpret_cast<fn_ImgBuf_Create_StandaloneVt_t>(
            ResolveExport("?Create@ImageBuffer@Video@@UEAA_NW4ImageType@12@W4Usage@12@W4Format@12@IHHW4FilterMode@12@W4AddressMode@12@44PEAV?$Array@V?$Array@E@Collection@@@Collection@@@Z"));

    g_NoiseUI_ImageBufferVtable_Standalone[0] = reinterpret_cast<void*>(fnDtor);
    g_NoiseUI_ImageBufferVtable_Standalone[1] = reinterpret_cast<void*>(&NoiseUIImgBuf_VtblStub_ReturnNull);
    g_NoiseUI_ImageBufferVtable_Standalone[2] = reinterpret_cast<void*>(&NoiseUIImgBuf_VtblStub_ReturnNull);
    g_NoiseUI_ImageBufferVtable_Standalone[3] = reinterpret_cast<void*>(&NoiseUIImgBuf_VtblStub_ReturnNull);
    g_NoiseUI_ImageBufferVtable_Standalone[4] = reinterpret_cast<void*>(&NoiseUIImgBuf_VtblStub_ReturnNull);
    g_NoiseUI_ImageBufferVtable_Standalone[5] = reinterpret_cast<void*>(&NoiseUIImgBuf_VtblStub_ReturnNull);
    g_NoiseUI_ImageBufferVtable_Standalone[6] = reinterpret_cast<void*>(&NoiseUIImgBuf_VtblStub_ReturnNull);
    g_NoiseUI_ImageBufferVtable_Standalone[7] = reinterpret_cast<void*>(&NoiseUIImgBuf_VtblStub_ReturnNull);
    g_NoiseUI_ImageBufferVtable_Standalone[8] = reinterpret_cast<void*>(&NoiseUIImgBuf_VtblStub_ReturnNull);
    g_NoiseUI_ImageBufferVtable_Standalone[9] = reinterpret_cast<void*>(fnCreate);

    RegisterExeAddr(0x14015c8c0,
                    reinterpret_cast<uintptr_t>(g_NoiseUI_ImageBufferVtable_Standalone));
}

static void RegisterVtablesAndTypeIds() {
    // --- Game singleton pointer storage (initialized to null, written at boot) ---
    RegisterExeAddr(0x1401c3de8, reinterpret_cast<uintptr_t>(&g_storage_KeyMapSingleton));
    RegisterExeAddr(0x1401c3dd8, reinterpret_cast<uintptr_t>(&g_storage_GameWorldSingleton));
    RegisterExeAddr(0x1401c3de0, reinterpret_cast<uintptr_t>(&g_storage_GameSettingsPtr));

    // --- MenuDialogLoopT vtable (0x14015ed00) — stubs for now ---
    RegisterMenuDialogLoopVtable();

    // --- Splash DialogLoaderLoopT vtable (0x140158800) — stubs for now ---
    // Used by both Dialog\Loading.rsc and Dialog\ShiningRock.rsc instances.
    RegisterSplashDialogLoopVtable();

    // --- LoaderLoop vtables (0x140158870 inner + 0x1401588d0 outer) — stubs ---
    RegisterLoaderLoopVtables();

    // --- GameUpdateLoop vtable (0x14015e2f0) — in-game simulation loop ---
    // NewGameDialog::StartGame writes this onto the freshly-allocated
    // GameUpdateLoop; without it the loop gets a NULL vptr. Slots 0-4 are the
    // clean reimpls (Destroy/Startup/Shutdown/Update/Draw); the rest are stubs
    // (GetResourceFilename returns "Game/Game"; Load logs the Stage-C cliff).
    RegisterGameUpdateLoopVtable();

    // Per-class vtables defined alongside their reimplementation in
    // code/Game/<Module>/<Class>.cpp — declared at file scope above.
    RegisterStatGoalVtable();
    RegisterTimeGoalVtable();
    RegisterScenarioVtable();
    RegisterToolbarVtable();
    RegisterMusicListVtable();
    RegisterBuildingGoalVtable();
    RegisterPopulationGoalVtable();
    RegisterTimeGoalInstanceVtable();
    RegisterFinishTutorialGoalVtable();
    RegisterFinishTutorialInstanceVtable();
    RegisterMainMenuDialogVtable();
    RegisterLoaderDialogVtable();
    RegisterOptionsDialogVtable();
    RegisterNewGameDialogVtable();
    RegisterOptionInputBindDialogVtable();
    RegisterMessageDialogVtable();
    RegisterMenuSceneVtable();

    // Weather vtable + factory at exe 0x140160290 — needed for MenuScene
    // particle deserialization (fn_ExternalPtr_Serialize_Weather).
    RegisterWeatherVtable();

    // World-generator types (Climate/Map/Terrain) — exe-defined Object::External
    // subclasses loaded as elements of the NewGameDialog combo list .rsc files.
    // Each registers its verified 9-slot vtable so ExternalFactory::FindOrLoad's
    // per-element load resolves a real factory instead of NullFactory crashing in
    // Runtime IsDerivedFrom. REGISTER_TYPE entries live next to the reimpls in
    // code/Game/Game/{climate,map,terrain}generator.cpp.
    RegisterClimateGeneratorVtable();
    RegisterMapGeneratorVtable();
    RegisterTerrainGeneratorVtable();

    // M5 5c Foundation C — entity description factories. Each registers its
    // verified vtable so ExternalFactory::FindOrLoad's per-sub-description load
    // resolves a real factory + Serialize instead of NullFactory. The matching
    // REGISTER_TYPE entries live next to the reimpls in code/Game/Component/
    // <type>.cpp; their NullFactory stubs were removed from factory_registrations_bulk.
    RegisterComponentDescriptionVtable();
    RegisterBuildDescriptionVtable();
    RegisterWorkDescriptionVtable();
    RegisterWorkPlaceDescriptionVtable();
    RegisterResidenceDescriptionVtable();
    RegisterZoneDescriptionVtable();
    RegisterMapDescriptionVtable();
    RegisterCreatePlacedDescriptionVtable();
    RegisterCreateDragDescriptionVtable();
    RegisterCharacterDescriptionVtable();
    RegisterCitizenDescriptionVtable();
    RegisterCitizenAIDescriptionVtable();
    RegisterStateMachineDescriptionVtable();
    RegisterNaturalResourceDescriptionVtable();
    RegisterAbsorbResourceDescriptionVtable();
    RegisterUIDescriptionVtable();
    RegisterToolbarDescriptionVtable();
    RegisterStatusIconDescriptionVtable();
    RegisterAmbientEmitterDescriptionVtable();
    RegisterInteractDescriptionVtable();
    RegisterClearAreaDescriptionVtable();
    RegisterDestroyDescriptionVtable();
    RegisterStorageDescriptionVtable();
    RegisterStorageLocationDescriptionVtable();
    RegisterStorageDisplayDescriptionVtable();
    RegisterModelDescriptionVtable();
    RegisterPickingDescriptionVtable();
    RegisterHighlightDescriptionVtable();
    RegisterDecalDescriptionVtable();
    RegisterParticleDescriptionVtable();
    RegisterHerdDescriptionVtable();
    RegisterPopulationDescriptionVtable();
    RegisterAnimalDescriptionVtable();
    // wave 2 (economy / farming / services / tools)
    RegisterAmbientEnvironmentDescriptionVtable();
    RegisterAttendanceDescriptionVtable();
    RegisterAutoPickupDescriptionVtable();
    RegisterBoatDescriptionVtable();
    RegisterCaptureBoxDescriptionVtable();
    RegisterCemeteryDescriptionVtable();
    RegisterClearResourceDescriptionVtable();
    RegisterConsumeProduceDescriptionVtable();
    RegisterCreateBridgeDescriptionVtable();
    RegisterCreateFencedDescriptionVtable();
    RegisterCreatePathDescriptionVtable();
    RegisterCreateRemovalDescriptionVtable();
    RegisterCreateRoadDescriptionVtable();
    RegisterCropFieldDescriptionVtable();
    RegisterDiseaseDescriptionVtable();
    RegisterEnvironmentDescriptionVtable();
    RegisterFireDescriptionVtable();
    RegisterFishingDescriptionVtable();
    RegisterFloorDescriptionVtable();
    RegisterForesterDescriptionVtable();
    RegisterFruitDescriptionVtable();
    RegisterFrustumOutlineDescriptionVtable();
    RegisterGrowthDescriptionVtable();
    RegisterHappinessDescriptionVtable();
    RegisterHealerDescriptionVtable();
    RegisterHerbalistDescriptionVtable();
    RegisterHuntDescriptionVtable();
    RegisterImmigrationDescriptionVtable();
    RegisterLivestockDescriptionVtable();
    RegisterMarketDescriptionVtable();
    RegisterMarketLocationDescriptionVtable();
    RegisterMineDescriptionVtable();
    RegisterMultiObjectDescriptionVtable();
    RegisterNomadDescriptionVtable();
    RegisterOrchardDescriptionVtable();
    RegisterPastureDescriptionVtable();
    RegisterQuarryDescriptionVtable();
    RegisterRadiusDescriptionVtable();
    RegisterRawMaterialDescriptionVtable();
    RegisterSelectDescriptionVtable();
    RegisterSpawnDescriptionVtable();
    RegisterTerraformDescriptionVtable();
    RegisterTombstoneDescriptionVtable();
    RegisterTornadoDescriptionVtable();
    RegisterTrackerDescriptionVtable();
    RegisterTradeDescriptionVtable();
    RegisterValueDescriptionVtable();
    RegisterWellDescriptionVtable();
    RegisterWildAnimalDescriptionVtable();

    // M5 5c citizens — AnimAudioEvent (Graphics::AnimationEvent subclass) is part of
    // the Character animation closure; without a real factory the citizen template
    // load hit "[NullFactory] Hit for type: AnimAudioEvent". See animaudioevent.cpp.
    RegisterAnimAudioEventVtable();

    // M5 5c citizens — Profession (Object::External) referenced by CitizenDescription.
    RegisterProfessionVtable();

    // Menu-load — NameDatabase (.ndb, Object::External) loaded by MenuScene_Setup
    // during DialogLoaderLoopT::Load; was a NullFactory crash. See namedatabase.cpp.
    RegisterNameDatabaseVtable();

    // In-game worldgen — Disease (Object::External) loaded by the climate/environment
    // resource list during BuildTerrain; was a NullFactory stub → IsDerivedFrom(NULL)
    // crash @Runtime+0xC039A (masked as a "BuildTerrain hang"). See disease.cpp.
    RegisterDiseaseVtable();

    // In-game worldgen — ProfessionList (Object::External, Profession.rsc) loaded
    // during the world build; same IsDerivedFrom(NULL) NullFactory crash. See
    // professionlist.cpp.
    RegisterProfessionListVtable();

    // Factory cascade (notes/FACTORY_CASCADE_145_BRIEF.md) — Action family.
    // Object::Internal-derived, 10-slot vtables; shared base helpers in
    // code/Game/Action/action_base_standalone.cpp.
    RegisterPlayAnimationVtable();
    RegisterAnimateNaturalResourceVtable();
    RegisterCompleteRoadVtable();
    RegisterCompleteTaskVtable();
    RegisterConsumeRawMaterialVtable();
    RegisterCreateNaturalResourceVtable();
    RegisterCreateRawMaterialVtable();
    RegisterDropOffRawMaterialVtable();
    RegisterHuntAnimalVtable();
    RegisterMoveToVtable();
    RegisterPerformWorkVtable();
    RegisterPickupRawMaterialVtable();
    RegisterPickupStoredRawMaterialVtable();
    RegisterRemoveAnimalVtable();
    RegisterRemoveNaturalResourceVtable();
    RegisterSendComponentMessageVtable();
    RegisterShowToolVtable();

    // Factory cascade — Work family (Object::Internal -> Work, 13-slot vtables;
    // shared base helpers in code/Game/Work/work_base_standalone.cpp).
    RegisterBuildRoadVtable();
    RegisterBuildStructureVtable();
    RegisterClearAreaVtable();
    RegisterClearStorageVtable();
    RegisterConsumeProduceVtable();
    RegisterFireWorkVtable();
    RegisterFishingWorkVtable();
    RegisterGatherRawMaterialsVtable();
    RegisterLocationWorkVtable();
    RegisterMarketWorkVtable();
    RegisterPickupVtable();
    RegisterPlantAreaVtable();
    RegisterPlantOrchardVtable();
    RegisterTendAreaVtable();
    RegisterTradeWorkVtable();
    RegisterForesterWorkVtable();
    RegisterHarvestAreaVtable();
    RegisterHarvestOrchardVtable();
    RegisterHerbalistWorkVtable();
    RegisterHuntWorkVtable();
    RegisterPastureWorkVtable();

    // Factory cascade — External-resource family (MusicList pattern). Tutorial types.
    RegisterTutorialVtable();
    RegisterTutorialGoalButtonVtable();
    RegisterTutorialGoalGameSpeedVtable();
    RegisterTutorialGoalCountVtable();
    RegisterTutorialImageVtable();
    RegisterTutorialImageHotKeyVtable();
    RegisterTutorialImageInputKeyVtable();
    RegisterTutorialImageToolbarIconVtable();
    RegisterTutorialSetupCameraVtable();
    RegisterTutorialSetupGameSpeedVtable();
    RegisterTutorialSetupRestrictPlayerVtable();
    RegisterTutorialStepVtable();

    // Factory cascade — Tool family (shared base tool_base_standalone.cpp).
    RegisterBridgeToolVtable();
    RegisterDragPlacementToolVtable();
    RegisterEditToolVtable();
    RegisterFencedToolVtable();
    RegisterPathToolVtable();
    RegisterPlacementToolVtable();
    RegisterRemovalToolVtable();
    RegisterRoadToolVtable();
    RegisterRotationSettingVtable();

    // Foundation C wave-2 array/ExternalPtr helper addrs (NULL in standalone until
    // registered). The single-ExternalPtr variants are stream-identical to the
    // StringTable serialize; the array resizers map to the proven stride helpers.
    RegisterExeAddr(0x14000b7b0, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_StringTable)); // ref-mode ExtPtr (Floor/Terraform)
    RegisterExeAddr(0x140007140, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_Typed_7140));   // typed ExtPtr (Nomad)
    RegisterExeAddr(0x14000bd30, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_StringTable)); // ExtPtr variant

    // Foundation C helper — single ExternalPtr field serialize (FUN_140002340 =
    // fn_ExternalPtr_BuildSerialize). Resolves via ResolveAddr(Module::Exe) so it
    // is NULL in standalone unless registered; many description ExternalPtr fields
    // (Herd/Population/NaturalResource/Citizen) depend on it. Stream-identical to
    // the generic ExternalPtr serialize → reuse the StringTable reimpl body.
    RegisterExeAddr(0x140002340, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_StringTable));

    // Foundation C helper — single ExternalRef<Profession> field serialize
    // (NaturalResource +0x78, Citizen +0x20/+0x28/+0x30). Stream-identical to the
    // generic ExternalPtr serialize; reuse the StringTable reimpl body.
    RegisterExeAddr(0x140018020, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_StringTable));

    // Geometry / "TerrainGenerator" derived vtable at exe 0x14015f110.
    // Without this, Reimpl_Geometry_Init's `*self = vtbl` no-ops because
    // 0x14015f110 isn't registered; the Geometry retains Runtime's base
    // Graphics::Geometry vtable whose slot +0x18 (Draw) is _purecall.
    // Renderer::Draw walks Scene+0x60 and dispatches +0x18, popping the
    // MSVC R6025 MessageBox.
    RegisterGeometryVtable();

    // M3.5 Path B session 7 — NoiseUI ImageBuffer vtable scaffolding
    // (used by NoiseUI / ModDialog / Geometry::Serialize image upload helper).
    RegisterNoiseUIImageBufferVtable();

    // TODO: Define static vtable arrays for each game class.
    // Each vtable is an array of function pointers matching the original layout.
    //
    // Pattern:
    //   static void* vtable_Weather[] = {
    //       (void*)&Weather_Dtor,     // +0x00
    //       (void*)&Weather_Slot1,    // +0x08
    //       ...
    //   };
    //   RegisterExeAddr(0x140160290, (uintptr_t)vtable_Weather);
    //
    // Type IDs are simpler — just static uint32_t storage:
    //   static uint32_t typeId_WeatherComponent = 0; // filled by RegisterType
    //   RegisterExeAddr(0x1401c7XXX, (uintptr_t)&typeId_WeatherComponent);
}

// ============================================================
// Init — call all registration sections
// ============================================================
void AddressRegistryInit() {
    RegisterCodeFunctions();
    RegisterDispatchSlots();
    RegisterVtablesAndTypeIds();

#ifdef _DEBUG
    AddressRegistryPrintStats();
#endif
}

void AddressRegistryPrintStats() {
    RegistryStats stats = GetRegistryStats();
    printf("[AddressRegistry] %zu addresses registered\n", stats.registered);
}

#endif // STANDALONE_EXE
