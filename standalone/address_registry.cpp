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


#include "resolve.h"
#include "address_registry.h"
#include "engine_funcs.h"
#include "Game/game.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton (cell 0x1401c3dd8)
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
    void RegisterOptionsDialogVtable();
    void RegisterNewGameDialogVtable();
    void RegisterOptionInputBindDialogVtable();
    void RegisterMessageDialogVtable();

    // M5 5c Foundation C — entity description types (template sub-descriptions).
    void RegisterToolbarDescriptionStringArrayResize();   // FUN_140028860 (the table row went with ToolbarDescription's real class, B.3.8 K1)
    void RegisterParticleComponentAddrs();   // building particle emitters (chimney smoke etc)
    void RegisterWeatherParticleVtable();
    void Reimpl_WeatherArray_Resize(void* hdr, int newCount);
    void RegisterFrustumOutlineDescriptionVtable();
    void RegisterAmbientExternalPtrSerialize();
    void RegisterProfessionListVtable();

    // Factory cascade — Work family (Object::Internal -> Work, 13-slot vtables).
    void RegisterBuildRoadVtable();

    // Tool family: real classes since ENDGAME B.3.8 (code/Game/Player/*Tool.h);
    // no tool vtable is registered at its exe address any more.
    void RegisterPathToolSetSelected();   // dd710 route-builder (pathtool_setselected.cpp)
}

// ============================================================
// Section 1: CODE FUNCTIONS
// ============================================================
// As you reimplement exe functions in code/Game/, add entries here:
//   RegisterExeAddr(0x140XXXXXX, (uintptr_t)&YourFunction);
// A row is for an address something still RESOLVES (ResolveAddr / R<> / a dispatch
// slot). A converted class's virtuals and vtable need none: the compiler's table
// reaches them (ENDGAME B.3.8, B38_RULES D5).

// M3.5 — MenuScene::Serialize generic helpers
// (decompiled bodies live in code/Game/Game/serialize_arrays.cpp +
//  code/Game/Game/serialize_externals.cpp)
extern "C" {
    void Reimpl_Transform_SerializeWrapper(void* transform, void* stream);
    void Reimpl_Array_Resize_Stride20(void* arrayHeader, int newCount);
    void Reimpl_Array_Resize_Stride08(void* arrayHeader, int newCount);
    void Reimpl_Array_Resize_Stride14(void* arrayHeader, int newCount);
    void Reimpl_Array_Resize_Stride10_Spawn(void* arrayHeader, int newCount);
    void Reimpl_Array_Serialize_Stride20(void* serializer, uint32_t count, void* arrayBase);
    void Reimpl_Array_Serialize_External_GraphicsMesh(void* serializer, int count, void* arrayBase);
    void Reimpl_Array_Serialize_External_Particle(void* serializer, int count, void* arrayBase);
    void Reimpl_ExternalPtr_Serialize_GraphicsMesh(void* refField, void* serializer);
    void Reimpl_ExternalPtr_Serialize_Weather(void* refField, void* serializer);
    void Reimpl_ExternalPtr_Serialize_StringTable(void* refField, void* serializer);
    // FUN_1400049c0 — ExternalPtr<ComponentDescription>[] array serializer.
    // (decompiled body lives in code/Game/Game/serialize_externals_array.cpp)
    void Reimpl_ExternalPtrArray_Serialize(void* serializer, int count,
                                           long long arrayBase, void* deadInit);
}

// M3.5 Path B session 2 — 0x60B struct array lifecycle helpers
// (decompiled bodies live in code/Game/Game/serialize_struct60b.cpp)
extern "C" {
    void Reimpl_Struct60B_Resize(void* arrayHeader, unsigned int newCount);
    void* Reimpl_Struct60B_CopyCtor(void* dst, const void* src);
    void Reimpl_Struct60B_DtorRange(void* unused, void* first, int count);
}

// Weather simulation per-tick driver — Weather::ResetTime (0x1400944e0).
// (decompiled body lives in code/Game/Game/weather_sim.cpp)
extern "C" {
    void Weather_ResetTime_Reimpl(void* weatherSelf, void* time);
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
    void* Reimpl_TradeWork_helper_e270(void* self, void* retbuf, int col, int row);
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
class ComponentEntity;
ComponentEntity* ComponentEntity_CtorFromDescription(ComponentEntity* self, void* descriptionPtr);
void EntityList_RegisterEntity_Impl(void* entityList, void* entityIBP, int category);

// M-B (cont.) — removal-side grid occupancy un-mark (impl_4750 standalone body,
// code/Game/Component/mapcomponent.cpp).
extern "C" void MapComponent_DetachOccupancy_4750(void* self);

// In-game HUD — ToolbarDialog ctor reimpl (FUN_1400b9910), body in
// code/Game/Dialog/toolbardialog.cpp. C++ linkage (not extern "C").
void* ToolbarDialog_Ctor_Impl(void* self, void* userInterface, void* dialog,
                              void* parentPtr, void* toolbarData);

// Component registration wave (2026-06-18) — each batch registrar lives in its
// own standalone/reg_component_<batch>.cpp and registers that batch's reimplemented
// exe helper addresses. See notes/COMPONENT_REG_WAVE_BRIEF.md.
extern void RegisterComponentBatch_people();
extern void RegisterImmigrationWave();
extern void RegisterComponentBatch_economy();
extern void RegisterComponentBatch_nature();
extern void RegisterComponentBatch_visuals();
// Phase-1 self-contained-sim live bring-ups (2026-06-20) — each registers only the
// type-id .rdata global its component's live Update/Startup needs (all other helpers
// already registered by earlier waves). See final_steps.md §3 Phase 1.
extern void RegisterComponentBatch_fruit();
// Animal spawn wave (2026-07-28, deer herds + pasture livestock). Shared type-id
// globals in reg_component_animal.cpp (gated on any BANISHED_LIVE_ANIMALS/HERD/
// PASTURE/ANIMAL_UPDATE flag); per-lane bring-ups in reg_herd_live.cpp,
// reg_pasture_live.cpp, reg_animal_update.cpp.
extern void RegisterComponentBatch_animal();
extern void RegisterHerdLive();
extern void RegisterPastureLive();
extern void RegisterAnimalUpdateLive();
extern void RegisterAnimalSerialize();
// Component GAP wave (2026-06-19, branch wave/component-gap) — residual-73 closure-safe
// helpers + 2 caller-bug literal fixes. reg files: reg_component_gap_{A,B,C,D}.cpp.
// A: citizencomponent/attendance (4). B: citizenAI setup/fishing/find/serialize +
// residence caller-bug 35ec0->07960 (6+1). C: decal/statusicon (2). D: herd/trade/build +
// terraform caller-bug a4b50->ab150 (3+1). nomad b60 deferred (Factory+vtable-Startup).
extern void RegisterComponentGap_A();
extern void RegisterComponentGap_B();
extern void RegisterComponentGap_C();
extern void RegisterComponentGap_D();
// Phase-4 economy: CitizenComponent::AssignWorkplace (0x1401163d0) + roster helpers
// a550/a710/129ff0. Dormant until a workplace is findable (townMgr+0x20 populated);
// makes the organic sub_0e70 -> AssignWorkplace path actually set citizen+0x238.
extern void RegisterCitizenAssignWorkplace();
// step 2.5 defer close — UpdateHealth 1127d0 + disease subsystem (RecoverOrDie/Spread/
// Catch/Infect/AnyCitizenSick/radius-query, 8 fns; see reg_citizen_updatehealth.cpp)
extern void RegisterCitizenUpdateHealth();
// Phase-4 economy: FindAvailableBuilding (0x14008d100) — reads the townMgr+0x100
// profession BST, returns an available workplace for a profession key (the producer
// employment path: WorkPlaceComponent/WorkComponent -> townMgr+0x100 -> here).
extern void RegisterTownWorkplaceReader();

// Construction 2b/2c — CitizenAI work-scheduler AVL insert (ec60 + 88a0), the post
// chokepoint for gather haul + build labor. Body: code/Game/Component/citizenai_work_scheduler.cpp.
extern "C" void RegisterCitizenAIWorkScheduler();
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
extern void RegisterMarketWorkBstBatch();
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
extern void RegisterAutoSaveTypeIds();       // LANE1 13/14: AutoSaveDialog type-id slot 0x1401c4d08
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
extern "C" float CharacterComponent_Sinf_131c90(float); // Phase 3 Keystone D: move-tick heading sinf (0x140131c90)
extern "C" float CharacterComponent_Cosf_131090(float); // Phase 3 Keystone D: move-tick heading cosf (0x140131090)
extern void RegisterCharManager();           // Phase 3 Keystone D: char spatial-hash manager (sub_83f0/8660/8c40/8d00/28c0/8560)
extern void RegisterFoodTypeIds();           // food-production .rdata const + CropField maturity door (type-id storage gone, ENDGAME B.3.9 TR)
extern void RegisterRdataFloatConsts();     // 25 .rdata float constants resolved by code/ but registered nowhere; a null one is SILENT (cost the crop harvest)
extern void RegisterOrchardGrowth();         // OrchardComponent::GetGrowth 51640 (orchard panel yield bar)
extern void RegisterWorkActBatch();          // Phase 2 Keystone B3: 5 Work action-queue builders
extern void RegisterTradeCtrlBatch();        // TradeComponent state machine, layer 1 (BANISHED_LIVE_TRADE_CTRL)
extern void RegisterBoatCtrlAddrs();         // BoatComponent state machine, layer 4 (BANISHED_LIVE_BOAT_CTRL)
extern void RegisterWorkTailTradeBatch();    // Phase 2 Keystone B tail: TradeWork AVL serialize (c8a0/c710/c830)
extern void RegisterTradeUILeavesBatch();     // Wave A: TradeUI DynArray clear leaves (16050/16130/16200)
extern void RegisterTradeUICheckoutBatch();   // Wave D: TradeUI checkout (81420) + leaves (5baa0/5a490)
extern void RegisterWorkTailFireBatch();     // Phase 2 Keystone B tail: FireWork spiral search (881d0/88730)
extern void RegisterFireBatch();             // Fire disaster lane: ignite/spread/extinguish/slot15 + the 2 type-id globals
extern void RegisterCropDiseaseBatch();      // Crop infestation / livestock disease (DiseaseComponent 48xxx)
extern void RegisterKeystoneAZoneBatch();    // Phase 2 Keystone A: zone+road enqueue (a6f00/9d360/9ce00/a6e30)
extern void RegisterKeystoneARestreeBatch(); // Phase 2 Keystone A: residence RB-tree (17230/15ec0/172f0/17460/14b40)
extern void RegisterTownWorkplaceRegistry();  // Phase 3 economy: profession-BST workplace insert (0x1400895a0)
extern "C" void RegisterBuildActions();       // Construction 2c: SetupBuildActions (0x1400c0690) build-labor action chain
extern "C" void RegisterTownStorageSeed();    // Economy M-D world half: d230/7ff0/91660/a1f00 (storage registry + start-resource seeder)
extern "C" void RegisterCitizenNeedsUpdate(); // Economy M-D citizen half: NeedsUpdate 112260 + helper_21e0 (+ rdata floats)
extern void RegisterGameSettingsPersist();    // GameSettings <-> IO::Persistence (0x140098970 Save / 0x140098b60 Load)
extern void RegisterNameDb();                 // NameDatabase::GenerateName (0x1400a4b30) — town/citizen name generator
extern "C" void ApplyKeyMapToUI_Impl(void* keyMap, void* userInterface);  // 0x140099260 (optionsdialog.cpp)
extern "C" float CalcWindFactor_Reg(void* time);                          // 0x1401027c0 (gameupdateloop.cpp)
extern "C" void* Toolbar_Utility_Reg(void*, void*, uint64_t);             // 0x140101a80 (toolbardialog.cpp)
extern "C" void StartNewGame_Reg(void*, void*, void*, void*, char, void*, void*, void*,
                                 unsigned, void*);                        // 0x1400b4540 (newgamedialog.cpp)
namespace Game { void RegisterDefaultInputBindings(); }                   // 0x1400992f0 (inputbindings.cpp)
extern void RegisterCitizen25();              // openfuncs step 2.5 — needs-msg helpers (fde0/17a0/1540) + lifecycle quartet (2040/68a0/302a0/117080)
extern void RegisterCitizenLifeEventHandler(); // step 2.5 defer close — HandleLifeEvent 116dd0 death chain (cemetery/tombstone/mourning, 7 fns)
extern "C" void RegisterCitizen25Tick();      // openfuncs step 2.5(b) — behavior-tick group + EventLog_AddEvent
extern "C" void RegisterEventLogCitizen();    // citizen life-event chain: PostLifeEvent (ex-"AssignState") + AddCitizenEvent + TileIndex
extern void RegisterModsBatch();              // Lane 4: local mod list (ModDialog/ModDetailsDialog vtables + ctor + Modification singleton exref)
extern void RegisterDestroyWorkersBatch();    // Upgrade path: WorkPlaceComponent builder re-seat / worker release (reg_destroy_workers.cpp; 0x14012f520 + 0x14012f670 + 0x14012fb00). REGISTERED UNCONDITIONALLY — there is no env gate in the batch; the former "opt-in BANISHED_LIVE_DESTROY_WORKERS" note here was stale and caused a wrong risk assessment (2026-07-30).
extern "C" void RegisterCitizenBehaviorTick(); // Town-manager tick: CitizenBehavior_Update 0x14008c2b0 (stock recompute for status/town-hall dialogs) + 0x1401721fc=75.0f
extern "C" void RegisterCitizenAssignCitizens(); // Town employment allocator: CitizenComponent_impl_43b0 0x1400d43b0 (assigns idle laborers to needy workplaces nearest-first)
extern void RegisterTownHallBatch();          // TownHallUI promotion: GroupRating/array-push/AVL28/prod-stat/sort/RemoveWorker + seat strategies
extern void RegisterKeystoneAMapBatch();     // Phase 2 Keystone A: map overlay/init (1241b0/9dc20/124750/9dee0/e2c0)
extern void RegisterKeystoneAGameDtorBatch();// Phase 2 Keystone A: Game::Dtor + worker-thread teardown (9ed60 + 8 subs)
extern void RegisterRegionGraphBuildBatch(); // Keystone A: region graph ctor + coarse array (a8670/1e060/1e420/f9e0)
extern void RegisterRegionPairCacheBatch();  // Keystone A: region-pair ring-buffer init (1cca0)
extern void RegisterPfRegionBatch();       // Wave 2: region-graph build/merge/flood-fill (13)
extern void RegisterPfRoadBatch();         // Wave 2: road-mesh + geometry wrappers (10)
extern void RegisterPlayerBridgeBatch();        // BridgeTool SnapEndCell + ValidatePath (2)
extern void RegisterPfAstarBatch();        // Wave 2: A* core + thread dispatcher/enqueuers (15)
extern void RegisterPfRootsRoadBatch();    // Wave 3: road-edit roots (9c480/9f2f0/9e0e0/9c640)
extern void RegisterPfRootsRouteBatch();   // Wave 3: routing/storage roots (9f810/12cd80/12d5d0/8d590)
extern void RegisterPfRootsDropBatch();    // Wave 3: dropoff + RemoveAnimal ctor (c78c0/c7570/c7620/bbea0)
extern void RegisterPfRootsMiscBatch();    // Wave 3: bd40 spiral + 92260 spatial + 122340 migration
extern void RegisterEntityBatch();         // Entity gap: 6 EntityList reimpl bodies (entitylist.cpp)
extern void RegisterGoalBatch();           // Goal gap: ExternalPtr resolver + StatValueDispatch
extern void RegisterDialogBatch();         // Dialog gap: FindComponent variant + autosave handler
extern void RegisterCitizenListSerialize(); // save/load wave: CitizenBehavior serialize (8f760) + post-pass (8f580)
extern void RegisterGoalsSerialize();       // save/load wave: goal/tutorial-mgr serialize (f9240)
extern void RegisterGameGridSerialize();    // save/load wave: game map-grid serialize (9f100)
extern void RegisterGulSerialize();         // save/load wave: SaveRestore keystone (959c0/95a90) + small subs
extern "C" void RegisterGulTeardown();      // GameUpdateLoop::Shutdown chain (c3d30/c3e60/907b0/1024d0/f8ff0)
extern void RegisterSaveLoadDialog();       // save/load wave: SaveLoadDialog ctor (b8270) + vtable (14015f900)
extern void RegisterTutorialBaseBatch();   // Tutorial gap: ExternalPtr-array serialize (5) + Toolbar resolve + Check data helpers (5)
extern "C" void RegisterFindUIElementByPath();  // 0x1400e1870 — the tutorial highlight/arrow path resolver
extern void RegisterTutorialWaveBatch();   // Lane 2 Tutorial wave: 12 helper fns + 6 dialog vtables + 2 ctors
// Unregistered dispatch-slot wave (docs/waves/unregistered_slots_wave.md, 2026-09-04)
extern void RegisterUnregLane2();          // Lane 2: UI/dialog gaps — GetController + 4 SpriteSheet/Sprite ctors
extern void RegisterUnregLane3();
extern "C" void RegisterUnregLane1Batch();     // Lane 1: save/load + object core
extern void RegisterUnregLane4RdataConsts(); // Lane 4: 3 .rdata float consts          // Lane 3: graphics/math gaps — Vector3/Rectangle ctors, RandFloatTol, IsRunning, Material typeid, String/Filename ops
extern void RegisterGameLeafBatch();       // Game gap: closure-safe leaf dtors/teardown + terrain reimpls
extern void RegisterPlayerDragBatch();     // Player wave (drag): FindDescription/sounds/CloseDialog/ClearPathEntities (6)
extern void RegisterPlayerPlaceBatch();    // Player wave (place): FlipVariant (1) + UpdateHierarchy dispatch slot
extern void RegisterPlayerEditBatch();     // Player wave (edit): EntityAtCell/AvgTerrainHeight/FencedAnchorCalc (3)
extern void RegisterPlayerPathBatch();     // Player wave (path): RemovalTool checks (3) + FindComponent/state/f5d0 deps (4)
extern void RegisterPlayerTileBatch();     // Player wave r2 (tile): 003870 SetTransformFromYaw, d580 TileValidity, fd650 ValidateCreate
extern void RegisterPlayerRayBatch();      // Player wave r2 (ray): aa530 TerrainMarch, 1005a0 GetMouseRay, 101270 ScreenToCell, fda00 PlacementPos
extern void RegisterPlayerTradeBatch();    // Player wave r2c (trade): EditTool 66800 + EntityUI 666b0 + AVL tree family (102e30/2c690/25740/25920/15000/14ed0)
extern void RegisterPlayerToolUIBatch();   // Player wave r2c (toolui): OpenToolDialog ff9e0 REGISTERED; exe vtable 0x14015f5a0 deliberately NOT (menu-poison — see reg_player_toolui.cpp)
extern void RegisterToolbarUtilityBatch(); // GUI Phase 4: utility-dialog helpers (140100270 FindDialogByCtrlType + 140014890 BST insert)
extern void RegisterEditToolSelectBatch(); // EditTool select-entity->open-info-dialog (fd3b0 Entity_impl_d3b0)
extern void RegisterPlayerScreenRectBatch();// Player wave r2d (screenrect): 100380 entity screen-rect + ec70/098210/fe960 (Math via ResolveExport)
extern void RegisterComponentGap_C();      // Component GAP batch C visuals leaves: b0d00 TerrainDecalRemove + aa440 StatusIcon bounds
extern "C" void RegisterTornadoLane();     // Tornado wave: vtable 14015aa80 + slots 3/4/8 + path/damage leaves
extern void RegisterPlayerHighlightBatch(); // Player wave 5 (highlight): 55430 HighlightAdd -> b0d80 -> ace40 tessellation + BST family (TIER-2 geometry, worker-thread-free)
extern void RegisterPlayerCreateBarBatch(); // GUI Phase 3B (createbar): ffc60 CreateBar populate+position (e600/e420/e700/e360/ebb0 same-TU; ec70/fe960 via ResolveAddr)
extern void RegisterPlayerTileRenderBatch();// GUI Phase 3C (tilerender): b10e0 + 1008c0 + 101380 green/red footprint validity overlay (f5d0/0f90 already registered)
extern void RegisterPlayerRenderBatch();    // Selection outline (b930 EdgeMesh tessellation -> c4a0/c9e0/cb40); gated on BANISHED_SELECTION_OUTLINE, which is DEFAULT-ON (the default-on table, component_factory_base.cpp) -- this said "default-OFF" until 2026-09-07 (Wave 6)
extern void RegisterKeystoneARemovalBatch(); // Keystone A: RemovalTool::CalcPath (1035e0) + closure (d960/f8d50/11e4d0/11dfa0/9c700/f9ad0). Player clearance tool; lead-verified vs disasm.
extern void RegisterDestroySmBatch();        // Upgrade path: DestroyComponent state handlers (reg_destroy_sm.cpp; runtime-gated BANISHED_LIVE_DESTROY, default-ON).
// .rdata/.data census wave 2026-09-04.
extern "C" void RegisterW2Screenshot();    // GameUpdateLoop::TakeScreenshot 0x140095da0

// Code-fn wave 2026-09-04 (unported exe .text functions), six lanes.
extern void RegisterCodeLane2();           // MapParamsDialog ctor 0x14006bd80 (the Map button) + hygiene
extern "C" void RegisterCodeLane4();       // TakeScreenshot 0x140095da0 (live quicksave crash) + free ports
extern "C" void RegisterCodeLane5Batch();  // MusicPlayer::Update 0x1400a40e0 (background music) + hygiene

static void RegisterCodeFunctions() {
    // Each entry maps a Ghidra address to our reimplemented function.
    // 349 unique code addresses to fill (0x140001xxx - 0x14014Fxxx range).
    // These are exe-internal functions called by code/Game/ via function pointers.

    // Component registration wave — 4 batches of exe helper reimpls (Component module).
    RegisterUnregLane2();
    RegisterUnregLane3();

    // .rdata/.data wave — see docs/waves/rdata_data_wave.md.
    RegisterW2Screenshot();

    // Code-fn wave 2026-09-04 — see docs/waves/code_fns_wave.md.
    RegisterCodeLane2();
    RegisterCodeLane4();
    RegisterCodeLane5Batch();
    RegisterUnregLane1Batch();
    RegisterUnregLane4RdataConsts();
    RegisterComponentBatch_people();
    RegisterImmigrationWave();
    RegisterComponentBatch_economy();
    RegisterComponentBatch_nature();
    RegisterComponentBatch_visuals();
    // Phase-1 self-contained-sim live bring-ups (fruit/spawn/tombstone). The spawn
    // batch self-gates on BANISHED_LIVE_SPAWN (its type-id globals are also read by
    // other null-guarded consumers, so env-off must publish nothing).
    RegisterComponentBatch_fruit();
    // Animal spawn wave — shared type-ids + per-lane bring-ups (all self-gated).
    RegisterComponentBatch_animal();
    RegisterHerdLive();
    RegisterPastureLive();
    RegisterAnimalUpdateLive();
    RegisterAnimalSerialize();
    // Component GAP wave — residual-73 closure-safe helpers + caller-bug fixes.
    RegisterComponentGap_A();
    RegisterComponentGap_B();
    RegisterComponentGap_C();
    RegisterCitizenAssignWorkplace();
    RegisterCitizenUpdateHealth();   // step 2.5 defer close — UpdateHealth 1127d0 disease subsystem
    RegisterTownWorkplaceReader();
    RegisterComponentGap_D();

    RegisterCitizenAIWorkScheduler();

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
    RegisterMarketWorkBstBatch(); // 12-fn MarketWork BST/erase/serialize + 2 qsorts + 1 array grow
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
    RegisterAutoSaveTypeIds();       // LANE1 13/14 - AutoSaveDialog type-id slot (0x1401c4d08)
    RegisterCitizenAIHandlers();     // Phase 3 Keystone D Brick 3b — CitizenAI state handlers (Wander+)
    RegisterCitizenAIFindTarget();   // Phase 3 Keystone D Brick 3b — CitizenAI find-best-target (dd30+78c0)
    RegisterCitizenAIActionCtors();  // Phase 3 Keystone D Brick 3b — CitizenAI action-ctor family (9 ctors)
    RegisterCitizenAIExecutor();     // Phase 3 Keystone D Brick 4b — action executor + move-tick
    RegisterCharManager();           // Phase 3 Keystone D — char spatial-hash manager (move-tick tail char-index)
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
    RegisterTradeCtrlBatch();        // TradeComponent SM handlers — gated BANISHED_LIVE_TRADE_CTRL
    RegisterBoatCtrlAddrs();         // BoatComponent SM handlers — gated BANISHED_LIVE_BOAT_CTRL
    RegisterWorkTailTradeBatch();    // Phase 2 Keystone B tail — TradeWork AVL serialize (c8a0/c710/c830)
    RegisterTradeUILeavesBatch();    // Wave A — TradeUI DynArray clear leaves (3 of the 10 gate deps)
    RegisterTradeUICheckoutBatch();  // Wave D — TradeUI checkout; opt-in via BANISHED_LIVE_TRADE_CHECKOUT
    RegisterWorkTailFireBatch();     // Phase 2 Keystone B tail — FireWork spiral search (881d0/88730)
    RegisterFireBatch();             // Fire disaster lane (standalone/reg_fire.cpp)
    RegisterCropDiseaseBatch();      // Crop infestation — opt-in via BANISHED_LIVE_CROPDISEASE
    RegisterKeystoneAZoneBatch();    // Phase 2 Keystone A — zone+road enqueue cluster
    RegisterKeystoneARestreeBatch(); // Phase 2 Keystone A — residence RB-tree cluster
    RegisterTownWorkplaceRegistry(); // Phase 3 economy — profession-BST workplace insert
    RegisterBuildActions();          // Construction 2c — SetupBuildActions build-labor action chain (0x1400c0690)
    RegisterCitizenLifeEventHandler(); // step 2.5 defer close — HandleLifeEvent 116dd0 death chain
    RegisterTownStorageSeed();       // Economy M-D — storage-location registry + start-resource seeder
    RegisterCitizenNeedsUpdate();    // Economy M-D — CitizenComponent message dispatch (eat/drink delivery)
    RegisterGameSettingsPersist();   // GameSettings <-> Persistence — remembers the New Game picks
    RegisterNameDb();                // NameDatabase::GenerateName (0x1400a4b30) — town-name buttons
    // --- LANE1 sweep rows 34 + 35 (2026-08-30 audit) ------------------------
    // 0x140099260 ApplyKeyMapToUI: five UI sound-effect pointers + fullscreen fade
    // sprite + UI-scale ratio (ui+0xDC). GameUpdateLoop::Startup reaches it through
    // fn_SetupUIRefs on EVERY world boot and it was silently no-opping, so in-game UI
    // sounds/fade/scale were never applied. OptionsDialog Cancel used it too.
    RegisterExeAddr(0x140099260, reinterpret_cast<uintptr_t>(&ApplyKeyMapToUI_Impl));
    // 0x1400992f0 RegisterDefaultInputBindings: already ported and already run every
    // boot from RunGameMain, so the 50 bindings are fine — this only revives
    // OptionsDialog::OnCancel's reset-to-defaults, which went through the null pointer.
    RegisterExeAddr(0x1400992f0, reinterpret_cast<uintptr_t>(&Game::RegisterDefaultInputBindings));
    // 0x1401027c0 CalcWindFactor: |cameraPos - focus|, returns FLOAT in XMM0. It was
    // the only null leg of the 3-way guard around Audio::Scene::Update, so the entire
    // per-frame audio scene update was being skipped every frame.
    RegisterExeAddr(0x1401027c0, reinterpret_cast<uintptr_t>(&CalcWindFactor_Reg));
    // 0x140101a80 toolbar utility panel toggler. tutorialsetupdialog.cpp:369 calls it
    // UNGUARDED via the misnamed fn_SetFollowBuilding, so a tutorial step targeting a
    // building was a latent EXEC-of-0 (LANE1 sweep row 53, the LIVE-UNGUARDED half).
    RegisterExeAddr(0x140101a80, reinterpret_cast<uintptr_t>(&Toolbar_Utility_Reg));
    // 0x1400b4540 StartNewGame: the GameUpdateLoop -> GameLoadDialogLoopT -> LoaderLoop
    // chain. NewGameDialog had it inlined (the autostart-proven path); ScenarioDialog
    // called it BY ADDRESS and unguarded, so picking a scenario and clicking Load was an
    // EXEC-of-0 (LANE1 sweep row 42). One shared body now serves both.
    RegisterExeAddr(0x1400b4540, reinterpret_cast<uintptr_t>(&StartNewGame_Reg));
    RegisterCitizen25();             // openfuncs step 2.5 — needs-msg helpers (fde0/17a0/1540) + lifecycle quartet (2040/68a0/302a0/117080)
    RegisterCitizen25Tick();         // openfuncs step 2.5(b) — behavior-tick group (25a10/25b30/d6c60/d62a0) + EventLog_AddEvent (95580)
    RegisterEventLogCitizen();       // citizen life-event chain — PostLifeEvent 113af0 (ex-"AssignState") + AddCitizenEvent 94e00 + TileIndex 95030
    RegisterModsBatch();             // Lane 4 — local mod list (see standalone/reg_mods.cpp)
    RegisterDestroyWorkersBatch();   // Upgrade path — WorkPlaceComponent 12f520/12f670 worker release
    RegisterCitizenBehaviorTick();   // Town-manager tick — CitizenBehavior_Update (townMgr+0x170 stock recompute + 75s stats recorder)
    RegisterCitizenAssignCitizens(); // Town employment allocator — CitizenComponent_impl_43b0 (idle laborers -> needy workplaces, nearest-first)
    RegisterTownHallBatch();         // TownHallUI promotion — GroupRating/array-push/AVL28/prod-stat/sort/RemoveWorker + seat strategies
    RegisterKeystoneAMapBatch();     // Phase 2 Keystone A — map overlay/init cluster
    RegisterKeystoneAGameDtorBatch();// Phase 2 Keystone A — Game::Dtor + worker-thread teardown
    RegisterRegionGraphBuildBatch(); // Keystone A — region graph ctor + coarse-cell array (8670/e060/e420/f9e0)
    RegisterRegionPairCacheBatch();  // Keystone A — region-pair ring-buffer init (cca0)
    RegisterPfRegionBatch();       // pathfinding closure Wave 2 — region-graph build/merge/flood-fill
    RegisterPfRoadBatch();         // pathfinding closure Wave 2 — road-mesh + geometry wrappers
    RegisterPlayerBridgeBatch();        // BridgeTool SnapEndCell (fb710) + ValidatePath (fb2d0)
    RegisterPfAstarBatch();        // pathfinding closure Wave 2 — A* core + thread dispatcher
    RegisterPfRootsRoadBatch();    // Wave 3 roots — road-edit (9c480/9f2f0/9e0e0/9c640)
    RegisterPfRootsRouteBatch();   // Wave 3 roots — routing/storage (9f810/12cd80/12d5d0/8d590)
    RegisterPfRootsDropBatch();    // Wave 3 roots — dropoff + RemoveAnimal ctor
    RegisterPfRootsMiscBatch();    // Wave 3 roots — bd40 / 92260 / 122340

    // Entity / Goal / Dialog gap-closure waves (6 + 2 + 2 addrs).
    RegisterEntityBatch();         // EntityList PreUpdate/PostUpdate/RemoveEntity/Serialize/etc.
    RegisterGoalBatch();           // StatGoal ExternalPtr resolver (265b0) + StatValueDispatch (da4e0)
    RegisterDialogBatch();         // WorkplaceUI FindComponent (d6c0) + AutoSaveDialog handler (5dec0)
    RegisterCitizenListSerialize(); // save/load wave (code/Game/Game/citizenlist_serialize.cpp)
    RegisterGoalsSerialize();       // save/load wave (code/Game/Game/goals_serialize.cpp)
    RegisterGameGridSerialize();    // save/load wave (code/Game/Game/gamegrid_serialize.cpp)
    RegisterGulSerialize();         // save/load wave (code/Game/Game/gameupdateloop_serialize.cpp)
    RegisterGulTeardown();          // teardown chain (code/Game/Game/gameupdateloop_teardown.cpp)
    RegisterSaveLoadDialog();       // save/load wave (code/Game/Dialog/saveloaddialog.cpp)
    RegisterTutorialBaseBatch();   // Tutorial gap -> 0: serialize family (26fb0/27ad0/27de0/28cf0/28f40/29190) + Check helpers (92010/910a0/13920/12e5c0/12fbb0)
    RegisterFindUIElementByPath();  // e1870: without it every highlight + "unhide only this" step is a no-op
    RegisterTutorialWaveBatch();   // Tutorial wave: 262b0/28650/1001f0/e2250/e2330/e24f0/e2700/e29f0/1af90/1afe0/e1260/e1db0 + vtables 160f30/160870/1607a0/160d20/160c30/1609e0 + ctors e01d0/e2cc0
    RegisterFoodTypeIds();         // food-production component/description type ids — without these the fishing rate mask reads 0 on perfect water
    RegisterRdataFloatConsts();    // unregistered .rdata floats: hard guards go inert, soft `p ? *p : 0.0f` fallbacks silently use the WRONG value
    RegisterOrchardGrowth();       // OrchardComponent::GetGrowth 51640 — fn_OrchardGetGrowth was null (EXEC-of-0 in OrchardUI::Slot7)

    // Player registration wave (round 1) — closure-safe tool sub-helpers.
    RegisterPlayerDragBatch();     // 5160 FindDescription, ff980 CloseDialog, ffbf0/1000f0/100140 sounds, 1018d0 ClearPathEntities
    RegisterPlayerPlaceBatch();    // fd960 FlipVariant (+ 0x140152888 UpdateHierarchy dispatch slot)
    RegisterPlayerEditBatch();     // a0280 EntityAtCell, 101730 AvgTerrainHeight (leaf), dbf90 FencedAnchorCalc
    RegisterPlayerPathBatch();     // 104670/104770/1048a0 RemovalTool checks + 5780/3ea0/11e710/9f5d0 deps (bottom-up)
    RegisterKeystoneARemovalBatch();// RemovalTool::CalcPath 1035e0 + closure (d960/f8d50/11e4d0/11dfa0/9c700/f9ad0).
    //   Lead-verified offset-by-offset vs disasm (dfa0 had 2 crash bugs + 2 fidelity bugs, all fixed).
    //   Player clearance tool — not autostart-reached, so registering only affects the removal path.
    RegisterDestroySmBatch();      // Upgrade path: DestroyComponent state handlers (11d7b0/11cfd0/
    //   11cec0/11cc70/11c120) + guards (11da80/11e600/11e5b0).
    //   Runtime behavior gated by BANISHED_LIVE_DESTROY (default-ON since 2026-07-25;
    //   the env var is a kill-switch); see reg_destroy_sm.cpp.
    RegisterPlayerTileBatch();     // r2 tile: 003870 + d580 + fd650 (closure complete via prior waves)
    RegisterPlayerRayBatch();      // r2 ray: aa530 + 1005a0 + 101270 + fda00 (bottom-up; 1005a0 resolves 4 exports locally)
    RegisterPlayerTradeBatch();    // r2c trade: 66800 EditTool + 666b0 EntityUI + AVL family (retrace disasm-verified, typo 2e30/c690->102e30/2c690 caught)
    RegisterPlayerScreenRectBatch();// r2d: 100380 EditTool entity screen-rect + ec70/098210/fe960
    //   (value-return Math ops resolved via ResolveExport; 098210 2-deref display + projMat@+0xc8
    //   disasm-verified; EditTool-private so no startup/shared-vtable risk).
    RegisterPlayerHighlightBatch();// wave 5: 55430 HighlightAdd closure (b0d80 slot-mgr + ace40
    //   radius tessellation + BST family 1f6b0/1fc00/4b40/1f850/14b00). Worker-thread-free;
    //   shares AVL rotations 15000/14ed0 (reg_player_trade) + fa80/1f8d0/ee40/acc10/ae180 (pf waves).
    RegisterPlayerToolUIBatch();   // r2c toolui — REGISTERS OpenToolDialog ff9e0 (the reimpl
    //   writes the static standalone child-dialog vtable directly, so it is self-sufficient)
    //   and fills the g_ToolUI_ChildDialog_Vtable engine_funcs global by direct assignment.
    //   The exe vtable 0x14015f5a0 stays deliberately UNregistered forever: registering it
    //   makes the menu's BuildId child dispatch a real OnOpened on a half-built dialog
    //   (A/B-bisected startup crash). See reg_player_toolui.cpp.
    RegisterPlayerCreateBarBatch();// GUI Phase 3B: ffc60 CreateBar populate+position — fills the
    //   placement dialog's textSize/textStatus/imageIcon + SetUpperLeft over the footprint.
    //   Closure fully satisfied (sub-helpers same-TU; ec70/fe960 via reg_player_screenrect).
    RegisterPlayerTileRenderBatch();// GUI Phase 3C: b10e0 + 1008c0 + 101380 — the green/red
    //   footprint validity overlay tiles (closes reg_player_place.cpp's 101380 DEFER).
    //   f5d0 (reg_player_path) + 0f90 (reg_pf_road) already registered; render exports resolved in-TU.
    RegisterPlayerRenderBatch();   // Selection outline: b930 EdgeMesh tessellation chain ->
    //   c9e0 select-finalize (edittool_select's s_fn_c9e0) + c4a0 render + cb40 clear.
    //   GATED default-OFF (BANISHED_SELECTION_OUTLINE): registers nothing unless enabled,
    //   so the shipping select path stays byte-identical. See reg_player_render.cpp.
    RegisterToolbarUtilityBatch(); // GUI Phase 4: utility-dialog (toolbar case 2) helpers —
    //   140100270 FindDialogByControllerType + 140014890 BST_Insert_0x14. Retrace 14b40 already
    //   registered (reg_keystone_a_restree); child-dialog vtable via RegisterPlayerToolUIBatch.
    RegisterEditToolSelectBatch(); // EditTool click-to-select: fd3b0 Entity_impl_d3b0 opens the
    //   clicked entity's info DialogInstance (routes fn_EditTool_SelectEntity from edittool.cpp
    //   OnKey). Same DialogInstance ctor + g_ToolUI_ChildDialog_Vtable + AddDialog wiring as the
    //   Phase-4 utility path; teardown/refresh helpers null-guarded: fc9e0 gated on
    //   BANISHED_SELECTION_OUTLINE (reg_player_render), fcc90 deliberately never
    //   registered (per-frame EditTool deactivate slot — see reg_player_render.cpp).

    // Tornado lane (BANISHED_LIVE_TORNADO): component vtable 0x14015aa80 plus the
    // three overridden slots (3 Serialize / 4 Startup / 8 per-frame Update) and the
    // movement + damage leaves. Also carries the first port of the Vector3 array
    // grow 0x14000cf40, ParticleComponent 0x140053720 and AudioComponent 0x140042960.
    RegisterTornadoLane();

    RegisterComponentGap_C();      // Component GAP batch C visuals leaves — 0x1400b0d00
    //   TerrainDecalSystem release-one-slot (DecalComponent::OnCreate detach; without it every
    //   decal release silently no-ops and clear-job icons persist forever) + 0x1400aa440
    //   StatusIcon geometry-bounds. The batch file existed but this integrator call was missed.

    // Game registration wave — closure-safe leaf helpers (dtors/teardown + terrain).
    RegisterGameLeafBatch();       // String-array dtors 1b030/1b0a0/a8a0, MusicPlayer 18220,
    //   ImageCapture 1024fc0-dtor a4fc0, Weather 93eb0, MenuScene array 1b860, terrain 206b0/20870.
    //   All shutdown/teardown-path (dormant on autostart) -> additive, no world-build regression.

    // Lane 3 wave — built-in DebugUI / Game debug-cheat cluster. See
    // notes/wave_remaining/LANE3_DEBUGMENU_REPORT.md.

    // Game/KeyMap container ctor — FUN_140001050 (0xe0 bytes, sets g_pKeyMapSingleton)

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

    // M-B (cont.) — CreateNaturalResource_impl_4750 = the removal-side occupancy
    // UN-mark (dee0 clear), tail-called from the ported MapComponent_OnCreate
    // (vtable+0x38, dispatched by entity mark-removal c2ea0). Registering it makes
    // mapcomponent.cpp's s_fn_MapInit resolve; gated on comp+0x20 bit 0x100000.
    RegisterExeAddr(0x140124750, reinterpret_cast<uintptr_t>(&MapComponent_DetachOccupancy_4750));

    // Weather simulation tick — route Weather::ResetTime (called every sim tick
    // from GameUpdateLoop::Update, gameupdateloop.cpp step 3e) to our reimpl so
    // the temperature/snow-coverage sub-sim runs. EngineFuncsInit (after this)
    // then resolves fn_Weather_ResetTime to Weather_ResetTime_Reimpl.
    RegisterExeAddr(0x1400944e0, reinterpret_cast<uintptr_t>(&Weather_ResetTime_Reimpl));

    // Weather-descriptor array resize (stride 0x10) — EnvironmentDescription::
    // Serialize needs this to populate envRes+0x28 on load (else the rain/snow
    // descriptor array stays empty and no weather effect can spawn).
    RegisterExeAddr(0x1400198a0, reinterpret_cast<uintptr_t>(&Reimpl_WeatherArray_Resize));

    // In-game HUD bring-up — route the exe ToolbarDialog ctor (FUN_1400b9910) to
    // our reimpl so GameUpdateLoop::Startup can build the toolbar from the
    // preloaded Dialog/Toolbar.rsc. EngineFuncsInit (runs after this) then
    // resolves fn_ToolbarDialog_Ctor to ToolbarDialog_Ctor_Impl.
    RegisterExeAddr(0x1400b9910, reinterpret_cast<uintptr_t>(&ToolbarDialog_Ctor_Impl));

    // ============================================================
    // M3.5 — MenuScene::Serialize generic helpers
    // ============================================================
    RegisterExeAddr(0x1400988c0, reinterpret_cast<uintptr_t>(&Reimpl_Transform_SerializeWrapper));
    RegisterExeAddr(0x14001baf0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride20));
    RegisterExeAddr(0x140001bb0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride08));
    RegisterExeAddr(0x14001bf10, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride14));
    // SpawnDescription's 16-byte spawn-entry array. Ungated: without it every
    // SpawnDescription deserializes to count=0 (Serialize's count is a local that
    // only the resize call commits), which empties the tree spawn tables that are
    // the sole source of the Gatherer's Hut's four accepted resources.
    RegisterExeAddr(0x14000c520, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride10_Spawn));
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
    RegisterExeAddr(0x14001ef10, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride16B));
    RegisterExeAddr(0x1400021d0, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Stride04));
    // NameDatabase::Serialize array resizers (code/Game/Game/namedatabase.cpp)
    RegisterExeAddr(0x14000a590, reinterpret_cast<uintptr_t>(&Reimpl_Array_Resize_Byte));

    // ============================================================
    // M3.5 Path B session 4 — per-element 0x60B serialize + typed-ExternalPtr
    // array serialize (Geometry::Serialize cascade points 10 + 13).
    //   FUN_14001ffa0 — per-element 0x60B serialize (calls FUN_140007140 inside)
    //   FUN_140020150 — typed-ExternalPtr array serialize (Graphics::MaterialInstance,
    //                   typeInfo slot 0x140151350)
    // ============================================================

    // ============================================================
    // M3.5 Path B session 6 — heightmap mesh-build path helpers
    //   helper_e270  — vertex normal compute via heightmap deltas (read back by
    //                  action_pf_road.cpp). The sub_c1e0 / FUN_ac880 / sub_c780 and
    //                  chunk-update 0x1400aad50 / 0x1400aed40 rows went at the B.3.8
    //                  close with the dead engine_funcs pointers that read them.
    // ============================================================
    RegisterExeAddr(0x1400ae270, reinterpret_cast<uintptr_t>(&Reimpl_TradeWork_helper_e270));

    // ============================================================
    // M3.5 Path B session 8 — Geometry::Serialize body + version-gated extra
    //   impl_04d0    — main 15-step Geometry::Serialize body (master schema, v6)
    //   impl_f1a0    — version-gated extra serialize (parallel schema, v4)
    //
    // Both walk the helper graph established in sessions 2-7 and call into
    // the IO::Serializer dispatch slots (lazy-resolved within the reimpl).
    // ============================================================
    RegisterExeAddr(0x1400b04d0, reinterpret_cast<uintptr_t>(&Reimpl_TradeWork_impl_04d0));
}

// ============================================================
// Section 2: IAT DISPATCH SLOTS
// ============================================================
// The original exe has .rdata slots at 0x140151xxx-0x140152xxx that hold
// pointers to Runtime DLL functions (filled by the exe's custom loader).
// In standalone mode, we allocate our own static slots and fill them with
// the Runtime function pointers resolved via ResolveExport.
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
    // (0x140151f30 Graphics::MeshInstance::_rootOnlyUpdates: row deleted at ENDGAME
    // B.3.9 SS2 — its readers, GameUpdateLoop::Update, the character move-tick and
    // PerfX, read the class static.)
    // Object::Type::Serialize — slot 0x1401516b0. Used by UIDescription (entity
    // element +0x00), ConsumeProduce, SendComponentMessage, ConsumeProduceDescription.
    // UNREGISTERED previously → those reimpls' `if (s_fn_Type_Serialize)` guard
    // silently SKIPPED the per-element Type read → stream drift → bogus String
    // length → CompressedStream::Read zero-progress spin on Dialog\StandardDialog.rsc
    // (menu UIDescriptions have count==0 so never hit it). The mangled name is the
    // exe's own import string at the slot's RVA (0x1401b8d2e).
    RegisterDispatchSlot(0x1401516b0,
        (uintptr_t)ResolveExport("?Serialize@Type@Object@@QEAAXAEAVSerializer@IO@@@Z"));
    // (0x1401518b8 ??1ElementDescription / 0x1401518b0 ??0ElementDescription /
    // 0x1401518d8 ??0Element: rows deleted at ENDGAME B.3.9 EXA — their only
    // readers were engine_funcs door globals with no callers.)
    RegisterDispatchSlot(0x1401518f0,
        // PE import table: this slot is the INSTANCE overload (6 ints + 4 int& + 2
        // bools), which is what retail FrustumOutlineElement_Slot5 (0x14006c1c0)
        // calls and what the typedef at engine_funcs.h:3870 declares. We had bound
        // the 17-param STATIC overload, so `this` would land where Alignment goes
        // and the callee would read six argument slots the caller never wrote.
        // Latent, not live: zero callers today.
        (uintptr_t)ResolveExport("?GetAlignedPosition@Element@UI@@QEAAXHHHHHHAEAH000_N1@Z"));
    // System::Random RandTol / RandFloat — .rdata dispatch slots read by many Work/
    // Action reimpls (PastureWork Slot4, forester/herbalist, spawn/tombstone all
    // carry per-file fallbacks or MISSING traces for these). Previously UNREGISTERED:
    // PastureWork_ResolveFuncs FAILED on Random_RandTol, so the organic herder's
    // grazing-position roll had a null fn ptr. Mangled names match engine_funcs.cpp.

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
    // Serialize(int count, bool* array, name) — the bool ARRAY overload, distinct
    // from the single-bool slot 0x140152a58 above (AEA_N vs HPEA_N). Seven retail
    // functions call it: ParticleComponent_Shutdown (0x140053270),
    // BuildStructure/HarvestArea/HarvestOrchard/PlantArea/PlantOrchard_Shutdown and
    // FUN_1400952c0 — all save-path serializers. It was registered NOWHERE, and
    // particlecomponent.cpp compounded that by binding the single-bool slot's
    // ADDRESS as a raw callee, so every save jumped into .rdata and died with
    // "EXEC of <data address>". A world with a particle emitter — i.e. any world —
    // could not be saved at all.
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

    // Phase 3 Keystone D Brick 4b-exec — MoveTo / PlayAnimation ACTION-METHOD dispatch slots.
    // In the original exe these .rdata IAT slots are pre-filled by the loader; moveto.cpp /
    // playanimation.cpp rely on that. In standalone they must be registered or their
    // ResolveFuncs leave s_fn_* null and the action vtable methods (precheck/execute, run by
    // the move-tick executor c4c50) call through null. Names = the exe's own import strings
    // at each slot RVA.
    // 0x140152ab0 (Object::InternalBasePtr::CheckPtr, IEBA) WAS registered here and is GONE
    // as of ENDGAME B.3.3 step 6D: CheckPtr is a bare `RET 0` in every shipped build, all
    // 4,570 call sites are deleted, and the sweep found no reader of this slot left — by
    // address (`ResolveAddr(Module::Exe, 0x140152ab0)`) or by name
    // (`ResolveExport("?CheckPtr@InternalBasePtr@Object@@IEBAXXZ")`). The RUNTIME-side port
    // and its `RegisterRuntimeExport` stay (runtime/src/object/internal.cpp): the mangled
    // name is still the ABI door for the real class's empty member. Do not re-add this row
    // without a caller.
    // MarketWork slot 4 stage 5 builds each FindPath argument with ctor-then-Copy. Both
    // slots were unregistered, so they resolved to 0 and calling them would have been an
    // EXEC-of-0 on exactly the path we are trying to exercise; marketwork.cpp carries a
    // fallback through fn_InternalBasePtr_Ctor for that case, which these registrations
    // retire.
    // ⚠ Names read from `objdump -p Runtime-x64.dll`, NOT guessed. Copy is IEAA
    // (protected), not QEAA — the obvious guess is wrong and would resolve to nullptr.
    RegisterDispatchSlot(0x140152ac0,   // Object::InternalBasePtr::InternalBasePtr(this) [default ctor]
        (uintptr_t)ResolveExport("??0InternalBasePtr@Object@@QEAA@XZ"));
    RegisterDispatchSlot(0x140152ab8,   // Object::InternalBasePtr::Copy(this, const InternalBasePtr&) [protected, IEAA]
        (uintptr_t)ResolveExport("?Copy@InternalBasePtr@Object@@IEAAXAEBV12@@Z"));
    RegisterDispatchSlot(0x140152a98,   // Object::Base::IsDerivedFrom(this, typeId) const -> bool
        (uintptr_t)ResolveExport("?IsDerivedFrom@Base@Object@@QEBA_NI@Z"));
    RegisterDispatchSlot(0x140151268,   // Math::Vector3::Vector3(this, Scalar&, Scalar&, Scalar&)
        (uintptr_t)ResolveExport("??0Vector3@Math@@QEAA@AEBVScalar@1@00@Z"));
    RegisterDispatchSlot(0x140151298,   // Math::Vector3::Dot(this, other) const -> Scalar
        (uintptr_t)ResolveExport("?Dot@Vector3@Math@@QEBA?AVScalar@2@AEBV12@@Z"));
    // Boat controller (boatcomponent_states.cpp): the two Vector3 value-return forms the
    // boat's steering core 0x1400463d0 and its state-1/3 handlers call. Both take a
    // hidden retbuf in RDX (Scalar is not returned in a register here — 0x140046482 and
    // 0x140044dd1 both LEA the destination first).
    RegisterDispatchSlot(0x140151290,   // Math::Vector3::Length(this) const -> Scalar
        (uintptr_t)ResolveExport("?Length@Vector3@Math@@QEBA?AVScalar@2@XZ"));
    RegisterDispatchSlot(0x1401512b8,   // Math::Vector3::Normalize(this, Scalar& outLen) -> Vector3
        (uintptr_t)ResolveExport("?Normalize@Vector3@Math@@QEBA?AV12@AEAVScalar@2@@Z"));
    RegisterDispatchSlot(0x1401512a0,   // Math::Vector3::FromYawPitch(Scalar&,Scalar&) -> Vector3
        // Static value-return ABI: RCX=hidden retbuf, RDX=yaw, R8=pitch. Read by
        // PerformWork::AbstractImpl (performwork.cpp) to convert the chop yaw into a
        // facing direction — unregistered → s_fn_FromYawPitch null → null-call the first
        // time a forester organically arrives at a tree and runs its chop precheck.
        (uintptr_t)ResolveExport("?FromYawPitch@Vector3@Math@@SA?AV12@AEBVScalar@2@0@Z"));

    // Bulk FLOAT array: Serialize(int count, float* data, name) — HPEAM form.
    // Required by ClimateGenerator::Serialize for its 48-float (+0x44) block.
    // System::Filename ctor/dtor — used by Terrain/Map generator Serialize for
    // the stack-temp Filename wrappers around filename String reads.
    RegisterDispatchSlot(0x1401526b8,
        (uintptr_t)ResolveExport("?Serialize@Vector2@Math@@QEAAXAEAVSerializer@IO@@@Z"));
    // System::String::operator= and ~String. TradeComponent::Setup writes every
    // merchant name through these two slots (tradecomponent.cpp:299-302); with the
    // assign slot null the copy into the name array never happened AT ALL, so the
    // whole array stayed zeroed and every trader rendered blank — independent of,
    // and behind, the missing 0x1400a4c60 name generator. A dispatch slot with no
    // reader is unregistered; neighbouring slots being fine proves nothing.
    RegisterDispatchSlot(0x1401514c0,
        (uintptr_t)ResolveExport("??4String@System@@QEAAAEAV01@AEBV01@@Z"));
    RegisterDispatchSlot(0x140152af0,
        (uintptr_t)ResolveExport("??1String@System@@QEAA@XZ"));
    // Math::Vector3::Serialize — required by Weather::Serialize (velocityDir at
    // weather+0x2C). Without it, Weather_Serialize null-derefs s_fn_Vector3_Serialize
    // when MenuScene's +0x80 Weather extPtr loads (intermittent: only on scenes
    // that reference an Environment/*.rsc).
    // MenuScene Slot5 atmospheric block — Vector4 + Color serialize.
    // Slots used at exe 0x1400a3796 (Vector4) and 0x1400a37aa..1400a37c6 (Color x3).
    // Without these, the post-Geometry tail of MenuScene::Serialize hits a
    // NOT_IMPLEMENTED dispatch slot and the menu-scene fog/sky/sun colors
    // stay at MenuScene_Init defaults (or zero for sun yaw/pitch).

    // ----------------------------------------------------------------
    // Terrain-decal / radius-highlight tessellation (BANISHED_LIVE_DECAL, Player wave 5).
    // Math::Transform2 / Vector2 ops read by TradeWork_impl_ace40
    // (tool_base_highlight_tess.cpp) to compose the per-tile UV->world transform and
    // project the 4 quad corners. Mangled names = the exe's own import strings
    // (0x1401bf138.. / 0x1401bbdf0). FromYawOffset is STATIC (value-return: RCX=retbuf);
    // the two operator* are member value-return (RCX=this, RDX=retbuf, R8=arg).
    // ----------------------------------------------------------------
    RegisterDispatchSlot(0x140151e58,   // Math::Vector2::Vector2() default ctor
        (uintptr_t)ResolveExport("??0Vector2@Math@@QEAA@XZ"));
    RegisterDispatchSlot(0x140152648,   // Math::Transform2::Transform2(Scalar&, Vector2&)
        (uintptr_t)ResolveExport("??0Transform2@Math@@QEAA@AEBVScalar@1@AEBVVector2@1@@Z"));
    RegisterDispatchSlot(0x140152650,   // Math::Transform2::FromYawOffset(Scalar&, Vector2&) [static]
        (uintptr_t)ResolveExport("?FromYawOffset@Transform2@Math@@SA?AV12@AEBVScalar@2@AEBVVector2@2@@Z"));
    RegisterDispatchSlot(0x140152640,   // Math::Transform2::operator*(Transform2&) const -> Transform2
        (uintptr_t)ResolveExport("??DTransform2@Math@@QEBA?AV01@AEBV01@@Z"));
    RegisterDispatchSlot(0x140152658,   // Math::Transform2::operator*(Vector2&) const -> Vector2
        (uintptr_t)ResolveExport("??DTransform2@Math@@QEBA?AVVector2@1@AEBV21@@Z"));

    // System::MemoryAllocator static MemAlloc/MemFree dispatch slots. Read via the
    // slot-deref pattern (*(fn**)ResolveAddr(0x140152b60/b68)) by the TradeWork tile
    // tessellation (tool_base_highlight*.cpp) AND every Work action helper
    // (work_act_*.cpp). Previously UNREGISTERED → ResolveAddr returned null and those
    // reimpls' MemAlloc/MemFree fn-ptrs stayed null; the terrain-decal tessellator's
    // index-buffer MemAlloc then returned null → AppendIndex wrote through a null base
    // (ACCESS_VIOLATION). NOTE the real export is SAPEAX_KH (uint64 size, int align) —
    // the ...SAPEAXII fallback string used by those files does NOT exist.
    RegisterDispatchSlot(0x140152b60,   // MemAlloc(size, align) static -> void*
        (uintptr_t)ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z"));
    RegisterDispatchSlot(0x140152b68,   // MemFree(void*) static
        (uintptr_t)ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));

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
    // Material swap — read by ModelComp_SetMaterialSet_125020 (the DestroyComponent
    // state-6 rubble step, destroycomponent_state6.cpp), which re-points a collapsing
    // building's MeshInstance onto the mesh's BURNT material set. Unregistered, the
    // rubble renders with its intact materials and the fire looks like a mesh swap only.
    // Math::Transform::FromYawPitchOffset — used by PlacementTool::Serialize to build
    // the ghost-preview transform from cursor yaw + world offset. The exe reaches it via
    // this .rdata slot, empty in standalone; without it the preview mesh gets an all-zero
    // (degenerate) transform => collapsed/invisible ghost. Static fn, returns Transform
    // by value (hidden retbuf). Sibling of the Transform ctor slot just above.
    RegisterDispatchSlot(0x140151288,                       // Transform::FromYawPitchOffset
        (uintptr_t)ResolveExport("?FromYawPitchOffset@Transform@Math@@SA?AV12@AEBVScalar@2@0AEBVVector3@2@@Z"));
    // Graphics::Geometry::GetTransform — read back the mesh transform (bbox for the
    // CreateBar cost dialog). Member fn, returns Transform by value (hidden retbuf).
    RegisterDispatchSlot(0x140151460,                       // Geometry::GetTransform
        (uintptr_t)ResolveExport("?GetTransform@Geometry@Graphics@@QEBA?AVTransform@Math@@XZ"));
    RegisterDispatchSlot(0x140151370,                       // Scene::AddObject
        (uintptr_t)ResolveExport("?AddObject@Scene@Graphics@@QEAAIPEAVGeometry@2@@Z"));
    RegisterDispatchSlot(0x140151360,                       // Scene::RemoveObject
        (uintptr_t)ResolveExport("?RemoveObject@Scene@Graphics@@QEAAXI@Z"));
    RegisterDispatchSlot(0x140152b00,                       // System::Time::Time(float)
        (uintptr_t)ResolveExport("??0Time@System@@QEAA@M@Z"));
    // System::Time::AsSeconds — CitizenComponent::Update reads this to get dt_sec for
    // hunger/thirst/age accumulation. Unregistered => s_fn_Time_AsSeconds NULL =>
    // dt_sec 0.0f => needs never climb (Phase-3 accumulation bug). Ghidra: first
    // indirect CALL in Update @0x140112eb4 = CALL [0x140151318] -> Time::AsSeconds.

    // (0x140151a80 Video::Device::_singleton: row deleted at ENDGAME B.3.9 SS2 — every
    // screen-dimension reader reads the class static.)

    // UI::Dialog::CreateElement — builds an Element from an ElementDescription
    // ExternalPtr. Every TutorialImage* slot 9 opens with it, and those slots only
    // became reachable once the Tutorial vtables were corrected to their real slot
    // counts; with the slot unregistered fn_Dialog_CreateElement was null, the
    // element came back 0, and Element::FindElementType(0, ...) faulted in Runtime.
    // (code/Game/Work/gatherrawmaterials.cpp calls this same slot "iatSerIntBulk" —
    // a misnomer; the retail call site at 0x1400E095A is CreateElement.)

    // (0x140152768 ??0Internal: row deleted at ENDGAME B.3.9 OBJ — its one
    // reader, engine_funcs' fn_ObjectInternal_Ctor, served only the Work_Ctor25
    // door, whose builders now construct the real Work leaves.)
    // (0x140152770 ??1Internal: row deleted at ENDGAME B.3.9 EXA — its readers
    // were the never-called fn_ObjectInternal_Dtor and five Action-local copies,
    // used only as readiness conjuncts.)
    RegisterDispatchSlot(0x140152a30,
        (uintptr_t)ResolveExport("??0Vector3@Math@@QEAA@MMM@Z"));

    // Phase 3 Keystone D — CharacterComponent move-tick TAIL render slots.
    // The tail (CharacterComponent_MoveTick_1094c0, citizenai_executor.cpp) writes the
    // citizen's per-frame world transform into ModelComponent+0x148, pushes it to
    // Graphics::Geometry::SetTransform (already registered at 0x1401512e0) and
    // Picking::Scene::SetTransform, and derives heading from the mesh root-animation
    // offset. It only runs when the char-index (CharacterComponent+0x40) >= 0, i.e. once
    // CharacterComponent::OnAdd + the char spatial-hash manager are live. Names = the
    // exe's own import strings at each slot RVA. All five are value-return / member ABIs:
    //   RootAnimationOffset / Vector3::Dot / Scalar::Max return by hidden retbuf.
    RegisterDispatchSlot(0x140151280,   // Math::Transform::operator=(const Transform&) -> Transform&
        (uintptr_t)ResolveExport("??4Transform@Math@@QEAAAEAV01@AEBV01@@Z"));
    RegisterDispatchSlot(0x140151418,   // Math::Transform::Transform(const Transform&) [copy ctor]
        (uintptr_t)ResolveExport("??0Transform@Math@@QEAA@AEBV01@@Z"));
    // Picking::PickingMesh::Vertices(int lod) const -> const Array<Vector3>&
    // Read by TerraformComponent's build-complete mesh pass (exe 0x140056b50) to walk the
    // TunnelFloor/TunnelCeiling picking meshes that carve the tunnel-mouth heightmap. This
    // was the LAST unregistered slot in that chain: with it null the pass bailed, so a
    // finished tunnel drew its DecalComponent overlay but got no terrain deformation.
    // Measured 2026-08-22 — the run logged "MeshPass(ceiling) SKIPPED — Transform/PickingMesh
    // runtime slots unresolved" while FootprintPass in the SAME message converted 21 cells,
    // which is what isolated this one slot rather than the whole component.
    // Math::Quaternion::Rotate(const Vector3&) const -> Vector3 (member value-return:
    // RCX=this, RDX=retbuf, R8=vec). Read by the attach-point resolver 0x140123880 to
    // turn a description-local yaw into a world heading, and by action_pf_road.cpp,
    // which has been reading this slot since the road wave and getting NULL — it
    // null-guards, so the miss was silent there. Name is the exe's import string at
    // the slot RVA and matches Runtime export [3589].
    // Picking::Scene::Intersect(start, dir, maxDist, &hitPos, &hitNormal) -> hit id or -1.
    // The ray-cast half of every mesh-precise pick. Read by:
    //   * CharacterComponent_impl_f3e0 (0x14009f534) — the deck/floor height refine that
    //     stands a citizen ON a dock or bridge instead of on the terrain under it
    //     (mapquery.cpp; that file falls back to the export by name if this is null).
    //   * PathTool (0x1400dd710 area) and EditTool — entity picking. EditTool already
    //     resolves it by mangled name; PathTool only ever read this slot, so its mesh
    //     pick has been dead, which is the "buildings are click-the-middle picky" report.
    // Picking::Scene::GetUserData(uint hitId) -> void* (the owning entity). Registered
    // alongside Intersect deliberately: a hit id with no way to resolve its owner is a
    // pick that cannot select anything, so Intersect ALONE would leave PathTool exactly
    // as broken (it null-guards this pointer and silently skips the mesh pick).
    // Picking::Scene::SetMeshUserData(uint id, void* userData, int, int, callback*) —
    // re-points an ALREADY-registered pick mesh at a different owner. Exactly two exe
    // callers: BridgeTool::CalcPath (0x1400fab75, re-owns each deck segment to the master
    // bridge entity) and FencedTool_helper_90d0 (0x140129114). Slot RVA read out of the
    // .rdata name table: 0x1401529a0 holds 0x1c0608 -> "?SetMeshUserData@Scene@Picking@@…"
    // (ordinal prefix 0x0f4c = 3916, which is the export's ordinal).
    RegisterDispatchSlot(0x140151e78,   // Math::Scalar::Max(const Scalar&) const -> Scalar
        (uintptr_t)ResolveExport("?Max@Scalar@Math@@QEBA?AV12@AEBV12@@Z"));

    // Math::Vector4::Vector4(const Scalar&) — broadcasts one float into all four lanes
    // (Runtime 0x18001d750 is literally four copies of *rdx). This is the slot
    // GrowthComponent::SetGrowthProgress (0x140120db0) and its per-frame twin use to
    // build the MeshInstance VISUAL SCALE for every non-crop natural resource. With the
    // slot empty our port's `if (s_fn_Vec4)` guard turned the whole write into a silent
    // no-op, so every runtime-spawned tree kept the MeshInstance ctor default of
    // (1,1,1,1) and rendered full-grown from the instant it was planted.
    RegisterDispatchSlot(0x140152958,
        (uintptr_t)ResolveExport("??0Vector4@Math@@QEAA@AEBVScalar@1@@Z"));
    // sinf / cosf used by the heading quaternion (clean CRT-wrapper reimpls, sqrtf precedent).
    RegisterExeAddr(0x140131c90, reinterpret_cast<uintptr_t>(&CharacterComponent_Sinf_131c90));
    RegisterExeAddr(0x140131090, reinterpret_cast<uintptr_t>(&CharacterComponent_Cosf_131090));

    // Graphics::AnimationManager::Update(Time const&, bool rootOnly) — the per-frame
    // animation tick. GameUpdateLoop::Update (exe FUN_140096380) calls this through
    // slot 0x140152050 every sim-iteration with (mgr@GameWorld+0x68, time, rootOnly).
    // UNREGISTERED until now ([GUARD-M5]) → the tick was silently skipped → skinned
    // citizen bone palettes stayed frozen at bind-pose (RenderDoc-verified static).
    // Runtime ?Update@AnimationManager@Graphics@@QEAAXAEBVTime@System@@_N@Z @ 0x1800fc240.
    // It iterates the manager's animation-group array (this+0x00, count this+0x08) and
    // advances each AnimationState; no-ops when no MeshInstance has registered a group.

    // (0x140152368 Application::_singleton and 0x140152760 Object::Factory::_singleton:
    // rows deleted at ENDGAME B.3.9 SS2 — their readers read the class statics.)

    // (0x140151728, the UI::Dialog type-info import slot: row deleted at ENDGAME
    // B.3.9 TR -- its two readers, mainmenudialog.cpp / mod_dialog_vtables.cpp
    // LoadDialogResource, read UI::Dialog::TypeId().)

    // (0x140151e98 ??0External / 0x140151ea0 ??1External: rows deleted at
    // ENDGAME B.3.9 EXA — their only readers, engine_funcs' fn_External_External
    // / fn_External_Dtor, had no callers.)

    // Object::Base::IsA — dispatch slot 0x140151410, read as `CALL qword ptr [...]`.
    // The slot was bound to exe 0x140002d40, which is NOT IsA (it is
    // DynArray8_EraseByValue) and has no reimplementation, so every consumer's
    // ResolveAddr returned 0 and their `s_fn_IsA &&` guards silently failed.
    //
    // That single null cost this project twice: it demoted every planted ORCHARD cell
    // (fixed there with a local fallback) and then, identically, every planted CROP
    // FIELD cell — 64 correctly-bound slots torn down on the next validate pass, the
    // field pinned forever. It is also what the food dialog harness saw as orchard
    // CanCommand hitting "NOT IMPLEMENTED: exe function at 0x140151410".
    //
    // Registering the slot here fixes it once for every present and future consumer,
    // instead of relying on each call site remembering a fallback. The two existing
    // fallbacks stay as belt-and-braces.
    RegisterDispatchSlot(0x140151410,
        (uintptr_t)ResolveExport("?IsA@Base@Object@@QEBA_NI@Z"));

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
    RegisterDispatchSlot(0x140152a48, reinterpret_cast<uintptr_t>(&RtSerializer_GetVersion));       // GetVersion()
    RegisterDispatchSlot(0x140152488, reinterpret_cast<uintptr_t>(&RtSerializer_Serialize_UShortArr)); // Serialize(cnt,ushort*)
    RegisterDispatchSlot(0x1401523f8, reinterpret_cast<uintptr_t>(&RtSerializer_Serialize_ShortArr));  // Serialize(cnt,short*)
    // PE import table: 0x140152400 is Serialize(int,uchar*,name) -> encoding vtbl
    // +0x60 (an N-iteration one-byte loop), NOT SerializeBytes -> +0xC8 (one
    // N-byte call). Wire-identical under the binary encoding, so this was never a
    // desync, but it dispatched the wrong Encoding virtual.
    RegisterDispatchSlot(0x140152400, reinterpret_cast<uintptr_t>(&RtSerializer_Serialize_UCharArr));  // Serialize(cnt,uchar*) -- encoding vtbl +0x60
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
static void* g_storage_GameSettingsPtr = nullptr;       // 0x1401c3de0

// (The hand-built `Video::ImageBuffer` table registered at 0x14015c8c0 — slots
// 1-8 returning null — went at ENDGAME B.3.9 EXA: the exe table is the class's
// own, and every builder constructs the real class.)

static void RegisterVtablesAndTypeIds() {
    // --- Game singleton pointer storage (initialized to null, written at boot) ---
    RegisterExeAddr(0x1401c3de8, reinterpret_cast<uintptr_t>(&g_storage_KeyMapSingleton));
    RegisterExeAddr(0x1401c3dd8, reinterpret_cast<uintptr_t>(&GameUpdateLoop::_singleton));   // B3R STORAGE
    RegisterExeAddr(0x1401c3de0, reinterpret_cast<uintptr_t>(&g_storage_GameSettingsPtr));

    // ENDGAME B.3.9 EXA: the four loop vtable rows (MenuDialogLoopT 0x14015ed00,
    // GameLoadDialogLoopT 0x140158800, GameLoaderLoop 0x1401588d0, GameUpdateLoop
    // 0x14015e2f0) are gone: every builder constructs the real class.

    // Per-class vtables defined alongside their reimplementation in
    // code/Game/<Module>/<Class>.cpp — declared at file scope above.
    RegisterOptionsDialogVtable();
    RegisterNewGameDialogVtable();
    RegisterOptionInputBindDialogVtable();
    RegisterMessageDialogVtable();

    // Description-side helper doors (the description classes are real classes;
    // their tables are the compiler's and no longer registered).
    RegisterToolbarDescriptionStringArrayResize();
    RegisterParticleComponentAddrs();   // ParticleComponent helper closure (0x1400524xx-0x1400536xx)
    RegisterWeatherParticleVtable();   // weather-particle Geometry doors 0x1400d3f10 / 0x1400d3f80 (the table row is gone)
    RegisterFrustumOutlineDescriptionVtable();

    // Ambient audio: SoundEffect ExternalPtr serialize (0x1400019a0) + biome-table
    // array resize (0x140002540) so the ambient descriptions actually load their
    // day/night sound refs (else they deserialize as null → silent ambient).
    RegisterAmbientExternalPtrSerialize();

    // ProfessionList::Startup door 0x1400a90a0 (GameUpdateLoop::Startup resolves it;
    // the table row is gone). See professionlist.cpp.
    RegisterProfessionListVtable();

    // Factory cascade — Work family (Object::Internal -> Work, 13-slot vtables;
    // shared base helpers in code/Game/Work/work_base_standalone.cpp).
    RegisterBuildRoadVtable();

    // Tool family (real classes since ENDGAME B.3.8): only the route-builder door.
    RegisterPathToolSetSelected();

    // Foundation C wave-2 array/ExternalPtr helper addrs (NULL in standalone until
    // registered). The single-ExternalPtr variants are stream-identical to the
    // StringTable serialize; the array resizers map to the proven stride helpers.
    RegisterExeAddr(0x14000b7b0, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_StringTable)); // ref-mode ExtPtr (Floor/Terraform)
    // 0x140007140 (typed ExtPtr, Nomad) is registered once in RegisterCodeFunctions
    // above — the second registration here pointed at the SAME symbol and only made
    // the owner ambiguous. Duplicate-registration sweep 2026-08-30.
    RegisterExeAddr(0x14000bd30, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_StringTable)); // ExtPtr variant

    // Foundation C helper — single ExternalPtr field serialize (FUN_140002340 =
    // fn_ExternalPtr_BuildSerialize). Resolves via ResolveAddr(Module::Exe) so it
    // is NULL in standalone unless registered; many description ExternalPtr fields
    // (Herd/Population/NaturalResource/Citizen) depend on it. Stream-identical to
    // the generic ExternalPtr serialize → reuse the StringTable reimpl body.
    RegisterExeAddr(0x140002340, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_StringTable));

    // Foundation C helper — ExternalPtr<ComponentDescription>[] ARRAY serialize
    // (FUN_1400049c0). Per-element body is byte-identical to the 0x140002340
    // single-element sibling above (same DWORD_1401c7318 = ComponentDescription
    // validation). Was NULL in standalone, so Create{Removal,Bridge,Fenced}
    // Description + animaldescription Serialize bailed and never deserialized
    // their ExternalPtr arrays. 4-arg signature (caller passes nullptr deadInit).
    RegisterExeAddr(0x1400049c0, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtrArray_Serialize));

    // Foundation C helper — single ExternalRef<Profession> field serialize
    // (NaturalResource +0x78, Citizen +0x20/+0x28/+0x30). Stream-identical to the
    // generic ExternalPtr serialize; reuse the StringTable reimpl body.
    RegisterExeAddr(0x140018020, reinterpret_cast<uintptr_t>(&Reimpl_ExternalPtr_Serialize_StringTable));
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

