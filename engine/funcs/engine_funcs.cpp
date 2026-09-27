// Resolved engine function pointers.
// Initializes function pointers and global variable addresses from Runtime-x64.dll.

#include "engine_funcs.h"
#include "options.h"
#include "resolve.h"

#include <cstdio>

// Shared .rdata-slot type-id resolver. Declared in engine_funcs.h.
// Prefers the exe .rdata DWORD; falls back to the live factory tree by NAME when that slot
// is unregistered (ResolveAddr -> 0) or registered-but-filled-with-zero. Testing the VALUE
// rather than the pointer is deliberate: a slot holding 0 is the documented failure mode a
// null-pointer check cannot see.
namespace Object { uint32_t ResolveTypeIdByNameLazy(const char* simpleName); }

// ScenarioDialog_Ctor (code/Game/Dialog/scenariodialog.cpp) — ported replacement
// for the unregistered ScenarioDialog_CopyCtor (0x1400786b0). Declared here rather
// than including ui_dialogs.h to avoid pulling the dialog header tree into this file.
void* ScenarioDialog_Ctor(void* self, void* ui, void* extRes, void* parentIBP,
                          void* parentMenuIBP, uint8_t difficulty);

uint32_t ResolveExeTypeId(uintptr_t rdataAddr, const char* name, uint32_t* cache) {
    if (cache && *cache) return *cache;
    uint32_t tid = 0;
    const uint32_t* p = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, rdataAddr));
    if (p && *p && *p != 0xFFFFFFFFu) tid = *p;
    if (!tid) tid = ::Object::ResolveTypeIdByNameLazy(name);
    if (cache) *cache = tid;
    return tid;
}

// ============================================================
// Timer globals — addresses in Runtime-x64.dll
// ============================================================
// System::Timer::_ticksPerSecond         @ 0x1801fe720
// System::Timer::_secondsPerTick         @ 0x1801fe728
// System::Timer::_systemTicksPerSecond   @ 0x1801fe730
// System::Timer::_systemInitialTick      @ 0x1801fe738

int64_t* g_Timer_systemInitialTick    = nullptr;
int64_t* g_Timer_ticksPerSecond       = nullptr;
int64_t* g_Timer_systemTicksPerSecond = nullptr;
double*  g_Timer_secondsPerTick       = nullptr;

// ============================================================
// Engine function pointers
// ============================================================

// Video::Context lifecycle — the whole group RETIRED by ENDGAME B.3.5 (b35-K2),
// zero callers tree-wide. See engine_funcs.h.

// MemoryAllocator
fn_MemAlloc_t fn_MemAlloc = nullptr;
fn_MemFree_t fn_MemFree = nullptr;
void* g_pMemAllocSingleton = nullptr;

// Video::Device singleton
void** g_pVideoDeviceSingleton = nullptr;

// Video::Device Lock/Unlock

// ExternalFactory
void** g_pExternalFactorySingleton = nullptr;
fn_IsDerivedFrom_t fn_IsDerivedFrom = nullptr;
// Null-safe trampoline installed over the Runtime IsDerivedFrom (see EngineFuncsInit).
static fn_IsDerivedFrom_t s_fn_IsDerivedFrom_real = nullptr;
static bool IsDerivedFrom_NullSafe(void* self, uint32_t typeId) {
    if (!self) return false;
    return s_fn_IsDerivedFrom_real ? s_fn_IsDerivedFrom_real(self, typeId) : false;
}
uint32_t* g_TypeId_Dialog = nullptr;

// Persistence
void** g_pPersistenceSingleton = nullptr;

fn_MemFreeStatic_t fn_MemFreeStatic = nullptr;
fn_InternalBasePtr_Ctor_t fn_InternalBasePtr_Ctor = nullptr;

// ============================================================
// UI Widget Functions (Phase A1)
// ============================================================

// CheckboxElement type ID (exref)
uint32_t* g_TypeId_CheckboxElement = nullptr;

// UI::ButtonElement type info (exe slot 0x1401514f8)
uint32_t* g_TypeId_ButtonElement = nullptr;

// UI::Element type info (exe slot 0x140151660)
uint32_t* g_TypeId_Element = nullptr;

// UI::ImageElement type ID (exref 0x140151500)
uint32_t* g_TypeId_ImageElement = nullptr;

// ControlComponent type ID (direct)
uint32_t* g_TypeId_ControlComponent = nullptr;

// FindComponentByTypeId — now inline in engine_funcs.h (reimplemented from FUN_1400c3680)

// EntityTrackingSet_BST_Delete (AVL node deleter, sub-function of _Remove)
fn_EntityTrackingSet_BST_Delete_t fn_EntityTrackingSet_BST_Delete = nullptr;

// ============================================================
// Exe Type IDs (Phase A2)
// ============================================================
uint32_t* g_TypeId_ComponentEntity = nullptr;
uint32_t* g_TypeId_ProgressElement = nullptr;
uint32_t* g_TypeId_ComponentDescription_FC = nullptr;
uint32_t* g_TypeId_ConsumeProduceComponent = nullptr;
uint32_t* g_TypeId_RadiusComponent = nullptr;
uint32_t* g_TypeId_ToolbarDescription_FC = nullptr;
uint32_t* g_TypeId_ResourceComponent = nullptr;
uint32_t* g_TypeId_RawMaterialDescription = nullptr;
uint32_t* g_TypeId_MeshInstanceComponent = nullptr;
uint32_t* g_TypeId_DestroyComponent = nullptr;
uint32_t* g_TypeId_WorkplaceComponent = nullptr;
uint32_t* g_TypeId_WorkComponent = nullptr;
uint32_t* g_TypeId_WorkDescription = nullptr;
uint32_t* g_TypeId_AttendanceDescription = nullptr;
uint32_t* g_TypeId_ResourceLimitType = nullptr;
uint32_t* g_TypeId_ToolUI = nullptr;

// BridgeTool-related type IDs
uint32_t* g_TypeId_CreateBridgeDescription = nullptr;
// RemovalTool-related type IDs
uint32_t* g_TypeId_RemovalDescription = nullptr;
// g_TypeId_MapComponent_FC (DAT_1401c79e0 = MapComponent) defined below with the FindComponent type-ids
// g_TypeId_MultiObjectComponent already defined below (~line 444)

// BridgeTool helper functions
fn_BridgeTool_SnapEndCell_t fn_BridgeTool_SnapEndCell = nullptr;
fn_BridgeTool_ValidatePath_t fn_BridgeTool_ValidatePath = nullptr;

// DragPlacementTool/FencedTool type IDs
uint32_t* g_TypeId_CreatePlacementDescription = nullptr;
uint32_t* g_TypeId_CreateFencedDescription = nullptr;
uint32_t* g_TypeId_AreaDescription = nullptr;
uint32_t* g_TypeId_FenceDescription = nullptr;
uint32_t* g_TypeId_SizeDescription = nullptr;

// ============================================================
// Game Singletons (Phase A3)
// ============================================================
void** g_pGameWorldSingleton = nullptr;
void** g_pKeyMapSingleton = nullptr;
void** g_pInputConfigSingletonExref = nullptr;

// ============================================================
// Common Exe Functions (Phase A4) — type ID globals for reimplemented FindComponent
// ============================================================
// FindComponent functions are now inline C++ (see engine_funcs.h).
// These globals are resolved directly from exe .rdata addresses.
uint32_t* g_TypeId_MapComponent_FC     = nullptr;  // DAT_1401c79e0
uint32_t* g_TypeId_StateMachineComponent_FC      = nullptr;  // DAT_1401c7d98
uint32_t* g_TypeId_ConsumeProduceComponent_FC = nullptr;  // DAT_1401c7350
uint32_t* g_TypeId_BuildComponent_FC        = nullptr;  // DAT_1401c7008
uint32_t* g_TypeId_DestroyComponent_FC      = nullptr;  // DAT_1401c75f0
uint32_t* g_TypeId_TradeComponent_FC        = nullptr;  // DAT_1401c4b80
uint32_t* g_TypeId_AttendanceComponent      = nullptr;  // DAT_1401c3f40
uint32_t* g_TypeId_PastureComponent         = nullptr;  // DAT_1401c7ba0
uint32_t* g_TypeId_AnimalDescription        = nullptr;  // DAT_1401c6fd0
uint32_t* g_TypeId_OrchardComponent         = nullptr;  // DAT_1401c46b0
uint32_t* g_TypeId_ClearAreaComponent_FC              = nullptr;  // DAT_1401c71c8
uint32_t* g_TypeId_CitizenComponent_FC      = nullptr;  // DAT_1401c7158
uint32_t* g_TypeId_StorageComponent_FC      = nullptr;  // DAT_1401c7e78
uint32_t* g_TypeId_ResidenceComponent       = nullptr;  // DAT_1401c7cf0
uint32_t* g_TypeId_CemeteryComponent        = nullptr;  // DAT_1401c4090
uint32_t* g_TypeId_TombstoneComponent       = nullptr;  // DAT_1401c4a30
uint32_t* g_TypeId_ImmigrationComponent     = nullptr;  // DAT_1401c4480
uint32_t* g_TypeId_TrackerComponent         = nullptr;  // DAT_1401c4b10
uint32_t* g_TypeId_ModelComponent_FC       = nullptr;  // DAT_1401c7a50

fn_RadiusCalc_t fn_RadiusCalc = nullptr;
fn_RadiusDisplay_Remove_t fn_RadiusDisplay_Remove = nullptr;

fn_GetActiveProductIndex_t fn_GetActiveProductIndex = nullptr;
// fn_CropUI_PopulateCombo — REIMPLEMENTED in code/Game/Dialog/cropui.cpp
// fn_CropUI_GetExpectedCount — REIMPLEMENTED in code/Game/Dialog/cropui.cpp
// CemeteryGetBurialCount — now static inline in cemeteryui.cpp

// WorkplaceComponent_Toggle (FUN_14012fc00)
fn_WorkplaceComponent_Toggle_t fn_WorkplaceComponent_Toggle = nullptr;

// OrchardComponent_GetGrowth (FUN_140051640)
fn_OrchardGetGrowth_t fn_OrchardGetGrowth = nullptr;
// PastureCalcMaxCount, PastureUI_UpdateSlider — now static inline in pastureui.cpp

// ============================================================
// Video::ImageBuffer (Runtime DLL exports)
// ============================================================

fn_SaveLoad_Dtor_t fn_SaveLoad_Dtor = nullptr;
void** g_pSaveLoadSingleton = nullptr;
fn_DateTime_AsString_t fn_DateTime_AsString = nullptr;
// fn_SaveLoadDialog_PopulateFileList — REMOVED, now reimplemented in saveloaddialog.cpp
void** g_pInputDeviceSingleton = nullptr;
void** g_pFileSystemSingleton2 = nullptr;
fn_LoadScenarioFromSave_t fn_LoadScenarioFromSave = nullptr;
void*** g_ppAchievementManager = nullptr;
// fn_FollowDialog_Close — inlined in followdialog.cpp Slot7

// UpgradeUI helpers
uint32_t* g_TypeId_UpgradeUIConfig = nullptr;
// fn_DestroyComponent_StartUpgrade — REMOVED, now reimplemented in destroyui.cpp

// fn_ResidenceUI_GoToCitizen — now inline in residenceui.cpp ResidenceUI_GoToCitizen
// fn_ResidenceUI_ArrayPush — now inline as ResidenceUI_ArrayPush_Impl in residenceui.cpp
// fn_EventLogUI_ToggleNotification — now inline in eventlogui.cpp Slot9
// fn_EventLogUI_NavigateToEvent — now inline in eventlogui.cpp Slot9

// WorkplaceUI helpers
fn_WorkplaceComp_SetMaxWorkers_t fn_WorkplaceComp_SetMaxWorkers = nullptr;
fn_CitizenList_RequestWorker_t fn_CitizenList_RequestWorker = nullptr;
fn_CitizenList_RemoveWorker_t fn_CitizenList_RemoveWorker = nullptr;
uint32_t* g_TypeId_EditTool = nullptr;

// EntityUI helpers — now reimplemented inline in entityui.cpp
// fn_EntityUI_SetEditMode — inlined
// fn_EntityUI_SelectEntity — inlined
uint32_t* g_TypeId_ToolController = nullptr;
uint32_t* g_TypeId_ToolSetting = nullptr;
fn_AVL_Rebalance_0x38_t fn_AVL_Rebalance_0x38 = nullptr;
// ============================================================
// Phase F Wave 1: MapParamsDialog / UpgradeUI / ConsumeProduceUI / HelpUI
// ============================================================
// fn_MapParamsDialog_OnOk — inlined in mapparamsdialog.cpp
// fn_MapParamsDialog_OnNewName — inlined in mapparamsdialog.cpp
// fn_GameWorld_GetMapName — inlined in mapparamsdialog.cpp
// fn_UpgradeUI_Serialize — inlined as UpgradeUI_Serialize in upgradeui.cpp
// fn_UpgradeUI_OnRemove — inlined as UpgradeUI_OnRemove in upgradeui.cpp
// fn_HelpUI_Slot4 — now inline as HelpUI_Slot4 in helpui.cpp
// fn_HelpUI_Slot9 — now inline as HelpUI_Slot9 in helpui.cpp

// fn_GoalDialog_ArrayPush — now inline as GoalDialog_ArrayPush_Impl in goaldialog.cpp
// fn_EventNotifyDialog_ArrayPush — now inline as EventNotifyDialog_ArrayPush_Impl in eventnotifydialog.cpp
// fn_ScenarioDialog_ArrayPush — now inline as ScenarioDialog_ArrayPush_Impl in scenariodialog.cpp
fn_ExternalList_GetResourceCount_t fn_ExternalList_GetResourceCount = nullptr;
uint32_t* g_TypeId_Scenario = nullptr;
// fn_EventNotifyDialog_ArrayRemove — inlined as EventNotifyDialog_ArrayRemove_Impl in eventnotifydialog.cpp
fn_ToolbarDialog_Ctor_t fn_ToolbarDialog_Ctor = nullptr;
fn_Toolbar_ShowGroup_t fn_Toolbar_ShowGroup = nullptr;
uintptr_t g_ToolUI_ChildDialog_Vtable = 0; // PTR_FUN_14015f5a0
// fn_Factory_CreateObject, g_TypeId_ToolController, g_TypeId_ToolSetting — defined above (entityui section)
uint32_t* g_TypeId_ToolbarExternal = nullptr;
uint32_t* g_TypeId_ToolbarItem = nullptr;
fn_FindDialogByControllerType_t fn_FindDialogByControllerType = nullptr;
// fn_Toolbar_FindEntity — inlined as Toolbar_FindEntity_Impl in toolbardialog.cpp
// fn_Toolbar_CenterOnEntity — inlined as Toolbar_CenterOnEntity_Impl in engine_funcs.h
uint32_t* g_TypeId_MultiObjectComponent = nullptr;
fn_EntityPanel_SyncTracking_t fn_EntityPanel_SyncTracking = nullptr;
fn_CameraSys_ComputeViewTransform_t fn_CameraSys_ComputeViewTransform = nullptr;
// fn_ToolbarDialog_CloseSubDialog — inlined as ToolbarDialog_CloseSubDialog_Impl in toolbardialog.cpp
// fn_ToolbarDialog_ChangeSpeed — inlined as ToolbarDialog_ChangeSpeed_Impl in toolbardialog.cpp
// fn_ToolbarDialog_UpdateButtons — inlined as ToolbarDialog_UpdateButtons_Impl in toolbardialog.cpp
uint32_t* g_TypeId_CheckElement = nullptr;
uint32_t* g_TypeId_ToolbarDialogParent = nullptr;
uint32_t* g_TypeId_ComboElement = nullptr;
// fn_ResolveExternalByName — REPLACED with inline ResolveExternalByName_Impl in engine_funcs.h

fn_EditTool_SelectEntity_t fn_EditTool_SelectEntity = nullptr;
float* g_pAgeMultiplier = nullptr;
float* g_pNotifyTimerDuration = nullptr;
fn_Time_Ctor_Float_t fn_Time_Ctor_Float = nullptr;
// fn_Dialog_GetReference removed — see engine_funcs.h (Wave 7 lane B)
uint32_t* g_TypeId_StringTable = nullptr;
uint32_t* g_TypeId_HelpFirstElem = nullptr;
uint32_t* g_TypeId_HelpComboElem = nullptr;
uint32_t* g_TypeId_HelpDocTextElem = nullptr;
uint32_t* g_TypeId_HelpContainerElem = nullptr;
uint32_t* g_TypeId_HelpButtonElem = nullptr;
// fn_HelpUI_ArrayResize — now inline as HelpUI_ArrayResize_Impl in helpui.cpp

// NewGameDialog support
void** g_pNewGameSettings = nullptr;
// fn_GenerateTownName — REMOVED, now reimplemented in newgamedialog.cpp
fn_NameDB_GenerateName_t fn_NameDB_GenerateName = nullptr;
uint32_t* g_TypeId_NameDatabase = nullptr;
fn_StartNewGame_t fn_StartNewGame = nullptr;
uint32_t* g_TypeId_MapGenerator = nullptr;
uint32_t* g_TypeId_ClimateGenerator = nullptr;
uint32_t* g_TypeId_TerrainGenerator = nullptr;

// OptionsDialog support — Runtime DLL exports

// OptionsDialog exe helpers — OnOk/OnCancel/OnApply reimplemented in code/Game/Dialog/optionsdialog.cpp
// PopulateInputBindList and OnInputBindClick now reimplemented in code/Game/Dialog/optionsdialog.cpp
// fn_OptionsDialog_PopulateInputBindList — REIMPLEMENTED in code/Game/Dialog/optionsdialog.cpp
// fn_OptionsDialog_OnInputBindClick — REIMPLEMENTED in code/Game/Dialog/optionsdialog.cpp

fn_InternalBasePtr_DeletePtr_t fn_InternalBasePtr_DeletePtr = nullptr;
fn_OptionInputBindDialog_Ctor_t fn_OptionInputBindDialog_Ctor = nullptr;
fn_MessageDialog_Ctor_t fn_MessageDialog_Ctor = nullptr;

void** g_pAudioConfigSingleton = nullptr;
void** g_pVideoConfigSingleton = nullptr;
// fn_Element_GetParent — now inline in optionsdialog.cpp (vtable call at +0x48)
int32_t** g_pGameSettingsPtr = nullptr;

// StorageUI support
// fn_StorageUI_SortItems — reimplemented as StorageUI_SortItems() in storageui.cpp

// MapUI helpers — REIMPLEMENTED in code/Game/Dialog/mapui.cpp
// fn_MapUI_SetupImage — now MapUI_SetupImage()
// fn_MapUI_HandleClick — now MapUI_HandleClick()
fn_MapUI_ProjectCornersToGround_t fn_MapUI_ProjectCornersToGround = nullptr;

// fn_UIDesc_AddControllers removed — reimplemented as UIDesc_AddControllers_Impl()
uint32_t* g_TypeId_UIDescription = nullptr;

// Player/Tool module — shared engine APIs
fn_SceneAddObject_t fn_SceneAddObject = nullptr;
fn_SceneRemoveObject_t fn_SceneRemoveObject = nullptr;
fn_MeshInstance_UpdateHierarchy_t fn_MeshInstance_UpdateHierarchy = nullptr;
// Graphics::EdgeMesh family (selection-outline tessellation)
fn_MemAllocStatic_t                fn_MemAllocStatic = nullptr;
fn_Box_Ctor_t                      fn_Box_Ctor = nullptr;
fn_Box_Extend_t                    fn_Box_Extend = nullptr;
fn_Box_Assign_t                    fn_Box_Assign = nullptr;
fn_EdgeMesh_TriangleVertex_Ctor_t  fn_EdgeMesh_TriangleVertex_Ctor = nullptr;
fn_EdgeMesh_EdgeVertex_Ctor_t      fn_EdgeMesh_EdgeVertex_Ctor = nullptr;
fn_TileRender_t fn_TileRender = nullptr;
fn_FencedPlacementParams_t fn_FencedPlacementParams = nullptr;

fn_Configuration_GetVersionString_t fn_Configuration_GetVersionString = nullptr;
// ModDetails_StringArrayPush — now static inline in moddetailsdialog.cpp

// CitizenUI helpers
uint32_t* g_TypeId_CitizenDescription = nullptr;
uint32_t* g_TypeId_StatusIconComponent = nullptr;
uint32_t* g_TypeId_CitizenAIComponent = nullptr;
// fn_StatusBarUI_StatusIconHandler — REPLACED with static StatusBarUI_StatusIconHandler_Impl() in statusbarui.cpp

// ProductionUI support
// fn_TrackerComponent_Rebuild — inline in engine_funcs.h

// ProfessionUI support
// BSTRebalance — now static inline in professionui.cpp
// fn_ProfessionUI_HandleSpinner — reimplemented in professionui.cpp
// fn_ProfessionUI_HandleButton — reimplemented in professionui.cpp

fn_Configuration_CurrentVersion_t fn_Configuration_CurrentVersion = nullptr;
fn_Configuration_CurrentBuildId_t fn_Configuration_CurrentBuildId = nullptr;
// fn_MainMenu_* handlers — REMOVED, now reimplemented in mainmenudialog.cpp
// Dialog ctor function pointers (used by reimplemented Open* handlers)
fn_NewGameDialog_Ctor_t fn_NewGameDialog_Ctor = nullptr;
fn_SaveLoadDialog_Ctor_t fn_SaveLoadDialog_Ctor = nullptr;
fn_OptionsDialog_Ctor_t fn_OptionsDialog_Ctor = nullptr;
fn_ScenarioDialog_Ctor_t fn_ScenarioDialog_Ctor = nullptr;
fn_MapParamsDialog_Ctor_t fn_MapParamsDialog_Ctor = nullptr;
fn_ModDialog_Ctor_t fn_ModDialog_Ctor = nullptr;

// ModDialog support
void** g_ppModificationSingleton = nullptr;
// fn_DialogInstance_AddController removed — see engine_funcs.h (Wave 7 lane B)
// fn_ModDialog_RefreshDisplay — REPLACED with static ModDialog_RefreshDisplay_Impl() in moddialog.cpp
// fn_ModDialog_ToggleMod — REPLACED with inline impl in moddialog.cpp
// fn_ModDialog_MoveToTop — REPLACED with inline impl in moddialog.cpp
// fn_ModDialog_ShowDetails — REPLACED with inline impl in moddialog.cpp
// fn_ModDialog_Apply — REPLACED with inline impl in moddialog.cpp
fn_ModDialog_CloseAndReload_t fn_ModDialog_CloseAndReload = nullptr;
fn_MainMenu_Reload_t fn_MainMenu_Reload = nullptr;
fn_Game_ReloadWithSave_t fn_Game_ReloadWithSave = nullptr;
fn_SavedState_Ctor_t fn_SavedState_Ctor = nullptr;

// IO::Serializer shared globals — all five removed, ENDGAME B.3.4a closing 2d.

// TownHallUI support
uint32_t* g_TypeId_TabElement = nullptr;
// fn_TownHallUI_InitGeneral — reimplemented as TownHallUI_InitGeneral() in townhallui.cpp
// fn_TownHallUI_UpdateGraph — reimplemented in townhallui.cpp
// fn_TownHallUI_UpdateGeneral — reimplemented in townhallui.cpp
// fn_TownHallUI_UpdateProfessions — reimplemented in townhallui.cpp
// fn_TownHallUI_OnProfessionSpinner — reimplemented in townhallui.cpp
void** g_pGameSingleton = nullptr;
// GroupRating helpers
fn_GroupRating_Init_t fn_GroupRating_Init = nullptr;
fn_GroupRating_Update_t fn_GroupRating_Update = nullptr;
// Production stat query
fn_QueryProductionStat_t fn_QueryProductionStat = nullptr;
// Citizen assignment (spinner)
fn_CitizenList_FindIdle_t fn_CitizenList_FindIdle = nullptr;
fn_CitizenList_FindWorking_t fn_CitizenList_FindWorking = nullptr;
fn_CitizenList_FindStudent_t fn_CitizenList_FindStudent = nullptr;
fn_CitizenList_FindAny_t fn_CitizenList_FindAny = nullptr;
// Array push helpers
fn_ProfArrayPush_t fn_ProfArrayPush = nullptr;
fn_ProdArrayPush_t fn_ProdArrayPush = nullptr;
// BST helpers
fn_BST_Rebalance_0x30_t fn_BST_Rebalance_0x30 = nullptr;
fn_BST_Rebalance_0x28_t fn_BST_Rebalance_0x28 = nullptr;
fn_BST_Insert_0x14_t fn_BST_Insert_0x14 = nullptr;
fn_BST_FreeNodes_t fn_BST_FreeNodes = nullptr;
// Inventory sort
fn_InventorySort_Asc_t fn_InventorySort_Asc = nullptr;
fn_InventorySort_Desc_t fn_InventorySort_Desc = nullptr;
// fn_GoToEntity — inlined as Toolbar_CenterOnEntity_Impl in engine_funcs.h

// SpriteCaptureUI support
uint32_t* g_TypeId_EditElement = nullptr;
uint32_t* g_TypeId_CaptureBoxElement = nullptr;

uint32_t* g_TypeInfo_ValueDescription = nullptr;
uint32_t* g_TypeInfo_TradeDescription = nullptr;
fn_DynArray_Clear_0x18_t fn_DynArray_Clear_0x18 = nullptr;
fn_IntroSort_ByName_t fn_IntroSort_ByName = nullptr;
fn_IntroSort_ByComponent_t fn_IntroSort_ByComponent = nullptr;
// fn_TradeUI_RefreshSellTable — now reimplemented inline in tradeui.cpp
// fn_TradeUI_RefreshBuyTable — now reimplemented inline in tradeui.cpp
fn_DynArray_Clear_0x20_t fn_DynArray_Clear_0x20 = nullptr;
fn_DynArray_Clear_0x18_t fn_DynArray_Clear_0x18_Buy = nullptr;
fn_Building_GetOrderName_t fn_Building_GetOrderName = nullptr;
fn_Building_GetBuyEntries_t fn_Building_GetBuyEntries = nullptr;
fn_TradeUI_OnTradeButton_t fn_TradeUI_OnTradeButton = nullptr;
uint32_t* g_TypeId_TableElement = nullptr;
uint32_t* g_TypeId_NumberElement = nullptr;
uint32_t* g_TypeId_TextElement = nullptr;
uint32_t* g_TypeId_LabelElement = nullptr;
uint32_t* g_TypeId_NumberEditElement = nullptr;
uint32_t* g_TypeId_SpinnerElement = nullptr;
uint32_t* g_TypeId_SelectDescription = nullptr;
fn_Building_GetSellableCount_t fn_Building_GetSellableCount = nullptr;
fn_StorageTree_Insert_t fn_StorageTree_Insert = nullptr;
fn_Building_AddBuyToStorage_t fn_Building_AddBuyToStorage = nullptr;
fn_Building_AddPurchaseOrder_t fn_Building_AddPurchaseOrder = nullptr;
// fn_NoiseUI_SetupImage — REIMPLEMENTED in code/Game/Dialog/noiseui.cpp
// fn_NoiseUI_GenerateNoise — REIMPLEMENTED in code/Game/Dialog/noiseui.cpp
// NoiseUI runtime dependencies
fn_PerlinNoise_Ctor_t fn_PerlinNoise_Ctor = nullptr;
fn_PerlinNoise_Generate_t fn_PerlinNoise_Generate = nullptr;
// Random default ctor: uses existing fn_Random_Default_Ctor
// --- ModDialog::Slot4 support ---
// ENDGAME B.3.9 EXA: fn_MaterialInstance_Ctor / fn_SpriteSheet_Ctor and the
// always-zero g_ModDialog_{MaterialInstance,SpriteSheet}Vtable are gone: the
// four thumbnail builders construct the real classes.
fn_ModDialog_ArrayPush_t fn_ModDialog_ArrayPush = nullptr;
uint32_t* g_TypeId_BrowserController = nullptr;
uint32_t* g_TypeId_Material = nullptr;
float* g_fUVScale = nullptr;
fn_Weather_ResetTime_t fn_Weather_ResetTime = nullptr;
uint32_t* g_TypeId_DiseaseDescription = nullptr;
uint32_t* g_TypeId_DrawableType = nullptr;
// fn_DebugUI_OnDrawComboChanged removed — now reimplemented in code/Game/Dialog/debugui.cpp
fn_DrawGeometry_Ctor_t fn_DrawGeometry_Ctor = nullptr;
fn_DrawGeometry_Init_t fn_DrawGeometry_Init = nullptr;
fn_AudioObjectGroup_Ctor_t fn_AudioObjectGroup_Ctor = nullptr;
fn_AudioScene_RegisterObject_t fn_AudioScene_RegisterObject = nullptr;
fn_Entity_FindAudioComponent_t fn_Entity_FindAudioComponent = nullptr;

// --- MenuScene 3D rendering (Phase A) ---
fn_Graphics_Renderer_Ctor_t      fn_Graphics_Renderer_Ctor      = nullptr;
fn_Graphics_Scene_Ctor_t         fn_Graphics_Scene_Ctor         = nullptr;
fn_Volume_Frustum_Ctor_t         fn_Volume_Frustum_Ctor         = nullptr;
fn_Graphics_Camera_Ctor_t        fn_Graphics_Camera_Ctor        = nullptr;


// --- DebugUI deep-reimpl helpers ---
uint32_t* g_TypeId_FireComponent = nullptr;
// g_TypeId_CitizenComponent removed — was duplicate of g_TypeId_RawMaterialDescription (DAT_1401c7cb8)
fn_Game_DestroyEntity_t fn_Game_DestroyEntity = nullptr;
fn_FireComponent_Ignite_t fn_FireComponent_Ignite = nullptr;
fn_DiseaseComponent_Infect_t fn_DiseaseComponent_Infect = nullptr;
fn_StorageComponent_GetCapacity_t fn_StorageComponent_GetCapacity = nullptr;
fn_StorageComponent_SpawnItem_t fn_StorageComponent_SpawnItem = nullptr;
fn_StorageCanAdd_t fn_StorageCanAdd = nullptr;   // 0x14012cd80
fn_StorageAddItem_t fn_StorageAddItem = nullptr;  // 0x14012d000
fn_EntityFactory_CreateEntity_t fn_EntityFactory_CreateEntity = nullptr;
fn_EntityList_RegisterEntity_t fn_EntityList_RegisterEntity = nullptr;
uint32_t* g_TypeId_InteractComponent_FC = nullptr;
uint32_t* g_TypeId_InteractDescription_FC = nullptr;
fn_ResidenceComponent_GetOccupantCount_t fn_ResidenceComponent_GetOccupantCount = nullptr;
fn_ResidenceComponent_GetSpawnPoint_t fn_ResidenceComponent_GetSpawnPoint = nullptr;
fn_CitizenComponent_Setup_t fn_CitizenComponent_Setup = nullptr;
fn_DisasterComponent_Infect_t fn_DisasterComponent_Infect = nullptr;

// --- SaveLoadDialog::Load helpers ---
// ENDGAME B.3.9 EXA: the loop doors (??0UpdateLoop/LoaderLoop/DialogLoaderLoopT/
// DialogLoopT) and the four exe loop vtable globals are gone — every builder
// constructs the real loop class (code/Game/Game/*Loop*.h).
fn_Random_Default_Ctor_t fn_Random_Default_Ctor = nullptr;
// fn_SetEngineNextLoop — REPLACED with inline SetEngineNextLoop() in engine_funcs.h
// fn_SetDialogLoaderNextLoop — REPLACED with inline SetDialogLoaderNextLoop() in engine_funcs.h
// fn_GetLoadingSoundString — REPLACED with inline GetLoadingSoundString() in engine_funcs.h
fn_AudioScene_Ctor_t fn_AudioScene_Ctor = nullptr;

void** g_pFileSystemSingleton = nullptr;

// ============================================================
// Engine globals
// ============================================================

// Engine::Loader instance (_loader) @ 0x1801fe7b8
void* g_pLoader = nullptr;

// _loader.m_flags @ 0x1801fe7c0
uint32_t* g_pLoaderFlags = nullptr;

// _loader.m_completionEvent @ 0x1801fe7e0
HANDLE* g_pLoaderCompletionEvent = nullptr;

// Application::Application::_singleton @ 0x1801fc678 (Runtime export
// ?_singleton@Application@1@0PEAV11@EA -- corrected by Wave 8 lane E; the
// previous address here was stale and unused by any resolve call).
void** g_pAppSingleton = nullptr;

void** g_pInputConfigSingleton = nullptr;
fn_BaseTypeInfo_Ctor_t fn_BaseTypeInfo_Ctor = nullptr;
void** g_pMainLoopSingleton = nullptr;
fn_SaveGameSettings_t fn_SaveGameSettings = nullptr;
fn_ReadGameSettings_t fn_ReadGameSettings = nullptr;
fn_ApplyKeyMapToUI_t fn_ApplyKeyMapToUI = nullptr;
fn_UpdateAutoSaveTimer_t fn_UpdateAutoSaveTimer = nullptr;
uint32_t* g_TypeId_AutoSaveDialogType = nullptr;
fn_RegisterDefaultInputBindings_t fn_RegisterDefaultInputBindings = nullptr;

fn_ExternalPtr_Serialize_t fn_ExternalPtr_Serialize = nullptr;
fn_ExternalPtr_Serialize2_t fn_ExternalPtr_Serialize2 = nullptr;
fn_ExternalPtr_Serialize3_t fn_ExternalPtr_Serialize3 = nullptr;
fn_ExternalRefArray_Serialize_t fn_ExternalRefArray_Serialize = nullptr;
fn_DynArray_Resize_t fn_DynArray_Resize = nullptr;
fn_StringArray_Resize_t fn_StringArray_Resize = nullptr;
fn_ExternalPtr_BuildSerialize_t fn_ExternalPtr_BuildSerialize = nullptr;
fn_WeatherResize_t fn_WeatherResize = nullptr;
fn_WeatherExternalSerialize_t fn_WeatherExternalSerialize = nullptr;

// TutorialSetupDialog helpers
fn_SerializeExternalPtr_t fn_SerializeExternalPtr = nullptr;
fn_TSD_Slot9Helper_t fn_TSD_Slot9Helper = nullptr;
fn_TSD_OpenDialog_t fn_TSD_OpenDialog = nullptr;
fn_TSD_SetDialogFlags_t fn_TSD_SetDialogFlags = nullptr;
fn_TSD_FindInstance_t fn_TSD_FindInstance = nullptr;
fn_SetFollowBuilding_t fn_SetFollowBuilding = nullptr;
fn_SetEditMode_t fn_SetEditMode = nullptr;

fn_MapGetTerrainPos_t fn_MapGetTerrainPos = nullptr;

// Phase H: Sub-dependency blockers
void** g_pFactorySingleton = nullptr;
fn_IsA_t fn_IsA = nullptr;

// ClearStorage helpers
uint32_t* g_TypeId_UnknownComp_7ee8       = nullptr;  // DAT_1401c7ee8
fn_Work_CanStartWork_t fn_Work_CanStartWork = nullptr;  // FUN_1400f7100
fn_FindStorageDestination_t fn_FindStorageDestination = nullptr;  // FUN_14008d590
fn_Map_CanPathTo_t fn_Map_CanPathTo = nullptr;  // FUN_1400a00f0
fn_Work_ValidateAssignment_t fn_Work_ValidateAssignment = nullptr;  // FUN_1400f6ef0
fn_Pathfinding_FindPath_t fn_Pathfinding_FindPath = nullptr;  // FUN_1400bf790
fn_CitizenAI_RegisterWork_t fn_CitizenAI_RegisterWork = nullptr;  // FUN_140021a30
fn_CitizenAI_LongTermPlan_t fn_CitizenAI_LongTermPlan = nullptr;  // FUN_140115e60
fn_BST_Remove_t fn_BST_Remove = nullptr;  // FUN_14001a6e0
fn_Storage_FindSlot_t fn_Storage_FindSlot = nullptr;  // FUN_14012d5d0
fn_Entity_GetPosition_t fn_Entity_GetPosition = nullptr;  // FUN_140122e20

// BuildRoad::Execute helpers
fn_Map_CheckPassable_t fn_Map_CheckPassable = nullptr;
fn_Map_FindNearestPassable_t fn_Map_FindNearestPassable = nullptr;
fn_PerformWork_Action_Ctor_t fn_PerformWork_Action_Ctor = nullptr;
fn_PerformWork_Action_Ctor3_t fn_PerformWork_Action_Ctor3 = nullptr;
fn_FindStorageForResource_t fn_FindStorageForResource = nullptr;
fn_FindDescription_RawMaterial_t fn_FindDescription_RawMaterial = nullptr;
fn_FindComponent_NaturalResourceC_t fn_FindComponent_NaturalResourceC = nullptr;
fn_FindComponent_WorkC_t fn_FindComponent_WorkC = nullptr;
fn_TryAssignWorkplace_t fn_TryAssignWorkplace = nullptr;
fn_FindComponent_RawMaterialC_t fn_FindComponent_RawMaterialC = nullptr;
fn_GetRemainingCapacity_t fn_GetRemainingCapacity = nullptr;
fn_BuildRoad_QueryStorage_t fn_BuildRoad_QueryStorage = nullptr;

fn_BST_SerializeHelper_t fn_BST_SerializeHelper = nullptr;
fn_Entity_FindComponent_Position_t fn_Entity_FindComponent_Position = nullptr;
fn_Entity_FindComponent_Build_t fn_Entity_FindComponent_Build = nullptr;
fn_Entity_FindComponent_WorkPlace_t fn_Entity_FindComponent_WorkPlace = nullptr;
fn_Entity_FindComponent_MatDesc_t fn_Entity_FindComponent_MatDesc = nullptr;
fn_PositionComponent_Count_t fn_PositionComponent_Count = nullptr;
fn_CalcMaxBuilders_t fn_CalcMaxBuilders = nullptr;
fn_Entity_GetPositionForIndex_t fn_Entity_GetPositionForIndex = nullptr;
fn_Map_Invalidate_t fn_Map_Invalidate = nullptr;
fn_Map_CalcPathCost_t fn_Map_CalcPathCost = nullptr;
fn_Map_NeighborLookup_t fn_Map_NeighborLookup = nullptr;
fn_Map_CalcPathDist_t fn_Map_CalcPathDist = nullptr;
fn_Map_CachePathDist_t fn_Map_CachePathDist = nullptr;
fn_SetupBuildActions_t fn_SetupBuildActions = nullptr;
fn_BST_Insert_t fn_BST_Insert = nullptr;
fn_CitizenComponent_CheckWorkplaceValid_t fn_CitizenComponent_CheckWorkplaceValid = nullptr;
// fn_CitizenComponent_HasVacancy is now an inline in engine_funcs.h (was a null-in-SA
// ResolveAddr pointer that crashed when called unguarded).
fn_FindStorageLocationComp_t fn_FindStorageLocationComp = nullptr;
fn_FindWorkplaceComp_t fn_FindWorkplaceComp = nullptr;

fn_ExternalPtr_SerializeTutorial_t fn_ExternalPtr_SerializeTutorial = nullptr;
fn_BuildingTypeArray_Dtor_t fn_BuildingTypeArray_Dtor = nullptr;
fn_FindUIElementByPath_t fn_FindUIElementByPath = nullptr;
fn_ObjectInternal_Ctor_t fn_ObjectInternal_Ctor = nullptr;
fn_TutorialTreeInsert_t fn_TutorialTreeInsert = nullptr;
fn_TutorialStepExecute_t fn_TutorialStepExecute = nullptr;
uint32_t* g_TypeId_FinishTutorialGoal = nullptr;
void** g_pSingletonExref_151a80 = nullptr;

// ComponentEntity dependencies
uint32_t* g_TypeId_ComponentBase = nullptr;

// CitizenBehavior dependencies
fn_ProfResArray_Init_t fn_ProfResArray_Init = nullptr;
fn_StatsArray_Init_t fn_StatsArray_Init = nullptr;
fn_SummaryArray_Init_t fn_SummaryArray_Init = nullptr;

// === EntityList dependencies (Agent A) ===
fn_IBPTreeInsert_t fn_IBPTreeInsert = nullptr;
fn_IBPTreeRemove_t fn_IBPTreeRemove = nullptr;

// Standalone plain-BST fallback for the EntityList update/draw trees (defined in
// code/Game/Entity/entitylist.cpp). Used when the exe AVL tree is unmapped.
extern "C" void* IBPTreeInsert_CppImpl(void*, void*, void*);
extern "C" void  IBPTreeRemove_CppImpl(void*, void*);
fn_BucketTreeSerialize_t fn_BucketTreeSerialize = nullptr;

// === CitizenBehavior Path dependencies (Agent C) ===
fn_Entity_GetGridId_t fn_Entity_GetGridId = nullptr;
fn_PathBST_Insert_t fn_PathBST_Insert = nullptr;
fn_PathPairs_Clear_t fn_PathPairs_Clear = nullptr;
fn_PathGraph_ReassignWorkers_t fn_PathGraph_ReassignWorkers = nullptr;
fn_PathGraph_AssignCitizens_t fn_PathGraph_AssignCitizens = nullptr;
fn_PathBST_FreeRecursive_t fn_PathBST_FreeRecursive = nullptr;

// === CitizenBehavior remaining core dependencies (Agent D) ===
fn_NotificationEvent_t fn_NotificationEvent = nullptr;

// === CitizenBehavior FindWorking/FindIdle/FindStudent dependencies ===
fn_PathDistance_t fn_PathDistance = nullptr;
fn_CitizenComponent_AssignHousing_t fn_CitizenComponent_AssignHousing = nullptr;

// ============================================================
// Phase A4: Entity::FindComponent and Array Utility Implementations
// ============================================================
// These were previously resolved from Application-x64.exe.
// Now implemented as clean C++ using Runtime DLL primitives.
// ============================================================

// Entity_FindComponentByExeTypeId -- searches entity component array for a component
// matching the given type ID. Stores found component (or null) via InternalBasePtr.
//
// Entity layout (from decompilation of all FindComponent variants):
//   entity+0x30 = component pointer array (void**)
//   entity+0x38 = component count (int32)
//
// Each component's vtable slot +0x10 (index 2) returns its type ID (int).
// IsDerivedFrom confirms the match before storing.
//
// Decompiled from FUN_140004680 (fn_FindComponent_Build) and confirmed identical
// across all 20+ FindComponent_X variants.
void Entity_FindComponentByExeTypeId(void* entity, void* retval, uint32_t* typeIdPtr) {
    // Zero out retval (InternalBasePtr = nullptr)
    *(void**)retval = nullptr;
    if (!entity || !typeIdPtr) return;

    uint32_t targetTypeId = *typeIdPtr;

    // Component array at entity+0x30 (ptr), count at entity+0x38
    void** compArray = *(void***)((uintptr_t)entity + 0x30);
    int    compCount = *(int*)   ((uintptr_t)entity + 0x38);

    void* found = nullptr;
    for (int i = 0; i < compCount; i++) {
        void* comp = compArray[i];
        if (!comp) continue;

        // vtable slot +0x10 (index 2) returns the component's type ID
        void** vtable = *(void***)comp;
        typedef int (*fn_GetTypeId_t)(void*);
        int compTypeId = ((fn_GetTypeId_t)vtable[2])(comp);

        if ((uint32_t)compTypeId == targetTypeId) {
            // Confirm with IsDerivedFrom
            if (fn_IsDerivedFrom(comp, targetTypeId)) {
                found = comp;
            }
            break;
        }
    }

    // Store result via InternalBasePtr::InternalBasePtr(retval, found)
    new (retval) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
}

// ============================================================
// Dynamic Array Utility Implementations (Phase A4)
// ============================================================
// Array header layout (all variants):
//   +0x00: data pointer (void*)  -- points to element data
//   +0x08: count        (int32)  -- current element count
//   +0x0C: capacity     (int32)  -- allocated capacity
//
// Capacity growth: when newCount == oldCount + 1 and oldCount != 0:
//   new capacity = newCount * 3
// Otherwise capacity = newCount (exact fit or shrink).
//
// Memory: fn_MemAlloc(g_pMemAllocSingleton, size, alignment) / fn_MemFreeStatic(ptr)

// ModDialog thumbnail array push (was ResolveAddr(Module::Exe, 0x140012a60), FUN_140012a60).
// {ptr,count:i32,cap:i32} header at +0x00/+0x08/+0x0C, 0x58-byte (11 qword) elements.
// Byte-faithful: grows (3x over-allocation on a plain +1 append), copies surviving old
// elements, and fills every newly-uncovered slot [oldCount, newCount) from elementPtr —
// in the common case that is just the appended slot at index oldCount.
void fn_ModDialog_ArrayPush58_CppImpl(void* arrayHeader, int newCount, void* elementPtr) {
    uint8_t* base   = (uint8_t*)arrayHeader;
    void**   pData  = (void**)base;
    int*     pCount = (int*)(base + 0x08);
    int*     pCap   = (int*)(base + 0x0C);
    static constexpr uint64_t kStride = 0x58;
    static constexpr int kWords = kStride / 8;

    if (newCount == *pCount) return;
    void* oldData  = *pData;
    int   oldCount = *pCount;
    *pCount = newCount;

    if (*pCap < newCount) {
        *pCap = (newCount == oldCount + 1 && oldCount != 0) ? newCount * 3 : newCount;
        *pData = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)*pCap * kStride, 8);
    }

    int startIdx = (oldData == *pData) ? oldCount : 0;
    for (int i = startIdx; i < newCount; ++i) {
        uint64_t* dst = (uint64_t*)((uint8_t*)*pData + (uint64_t)i * kStride);
        const uint64_t* src = (i < oldCount)
            ? (const uint64_t*)((uint8_t*)oldData + (uint64_t)i * kStride)
            : (const uint64_t*)elementPtr;
        for (int f = 0; f < kWords; ++f) dst[f] = src[f];
    }

    if (oldData != *pData) {
        fn_MemFree(g_pMemAllocSingleton, oldData);
    }
}

// ============================================================
// BuildStructure Work class helpers
// ============================================================
fn_TreeRemoveByKey_t fn_TreeRemoveByKey = nullptr;
fn_TreeRecursiveFree_t fn_TreeRecursiveFree = nullptr;
fn_StateMachine_ReleaseTasks_t fn_StateMachine_ReleaseTasks = nullptr;

fn_Video_VertexBuffer_Ctor_t fn_Video_VertexBuffer_Ctor = nullptr;
fn_Video_IndexBuffer_Ctor_t fn_Video_IndexBuffer_Ctor = nullptr;

fn_Array_Resize_t fn_Array_Resize_Stride20 = nullptr;
fn_Array_Resize_t fn_Array_Resize_Stride08 = nullptr;
fn_Array_Resize_t fn_Array_Resize_Stride14 = nullptr;
fn_Array_Serialize_Stride20_t fn_Array_Serialize_Stride20 = nullptr;

fn_Array_Serialize_External_t fn_Array_Serialize_External_GraphicsMesh = nullptr;
fn_Array_Serialize_External_t fn_Array_Serialize_External_Particle = nullptr;
fn_ExternalPtr_Serialize_t fn_ExternalPtr_Serialize_GraphicsMesh = nullptr;
fn_ExternalPtr_Serialize_t fn_ExternalPtr_Serialize_Weather = nullptr;

// M3.5 Path B session 2 — 0x60B struct array lifecycle helpers.
fn_Struct60B_CopyCtor_t     fn_Struct60B_CopyCtor           = nullptr;
fn_Struct60B_DtorRange_t    fn_Struct60B_DtorRange          = nullptr;
fn_Struct60B_SubArrayCopy_t fn_Struct60B_SubArrayCopyStride14 = nullptr;
fn_Struct60B_SubArrayCopy_t fn_Struct60B_SubArrayCopyStride02 = nullptr;

uint32_t** g_ppTypeInfo_GraphicsMesh = nullptr;
uint32_t** g_ppTypeInfo_Particle = nullptr;
uint32_t*  g_pTypeId_Weather = nullptr;

// M3.5 Path B session 3 — typed-ExternalPtr serialize helper at 0x140007140.
uint32_t** g_ppTypeInfo_MaterialInstance = nullptr;

// M3.5 Path B session 5 — additional Geometry::Serialize resize helpers.
fn_Array_Resize_t fn_Array_Resize_Stride02       = nullptr;  // 0x14001c890
fn_Array_Resize_t fn_Array_Resize_Stride16B      = nullptr;  // 0x14001ef10

// M3.5 Path B session 8 — the promoted IO::Serializer dispatch-slot pointers
// are all gone now; see the banner on the (former) typedef block in
// engine_funcs.h. The slot rows in address_registry.cpp are untouched.

// M3.5 — Geometry vtable draw primitives (slot 0x14015f110)
fn_Frustum_Contains_t                  fn_Frustum_Contains                 = nullptr;
fn_Frustum_MulTransform3_t             fn_Frustum_MulTransform3            = nullptr;

// ============================================================
// Initialization
// ============================================================

bool EngineFuncsInit() {
    // --- Timer globals (direct memory addresses) ---
    g_Timer_ticksPerSecond       = reinterpret_cast<int64_t*>(ResolveAddr(Module::Runtime, 0x1801fe720));
    g_Timer_secondsPerTick       = reinterpret_cast<double*> (ResolveAddr(Module::Runtime, 0x1801fe728));
    g_Timer_systemTicksPerSecond = reinterpret_cast<int64_t*>(ResolveAddr(Module::Runtime, 0x1801fe730));
    g_Timer_systemInitialTick    = reinterpret_cast<int64_t*>(ResolveAddr(Module::Runtime, 0x1801fe738));

    // --- Function pointers (resolve by mangled name, fall back to RVA) ---

    // Video::Context lifecycle — the five resolves that used to sit here are GONE
    // (ENDGAME B.3.5, b35-K2: zero callers tree-wide). See engine_funcs.h.

    // MemoryAllocator
    g_pMemAllocSingleton = reinterpret_cast<void*>(
        *reinterpret_cast<void**>(ResolveAddr(Module::Runtime, 0x1801fc718)));
    fn_MemAlloc = reinterpret_cast<fn_MemAlloc_t>(
        ResolveAddr(Module::Runtime, 0x1800c6e60));

    // Video::Device::_singleton
    g_pVideoDeviceSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Runtime, 0x1801fe7a8));

    // --- ExternalFactory ---
    g_pExternalFactorySingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Runtime, 0x1801fc6c8));

    fn_IsDerivedFrom = reinterpret_cast<fn_IsDerivedFrom_t>(
        ResolveExport("?IsDerivedFrom@Base@Object@@QEBA_NI@Z"));
    // Null-safe trampoline: the Runtime impl derefs obj unguarded. Retail never
    // passes null (the dialog framework closes panels before their entity
    // dies), but standalone dialogs keep ticking after the weak-ref at +0x28
    // is nulled by entity destruction — dozens of ported call sites pass the
    // ref straight in (crashed live 2026-07-20: house panel open across the
    // upgrade demolition). null → false is the semantics every caller expects.
    if (fn_IsDerivedFrom) {
        s_fn_IsDerivedFrom_real = fn_IsDerivedFrom;
        fn_IsDerivedFrom = &IsDerivedFrom_NullSafe;
    }

    // UI::Dialog type info (?_typeInfo@Dialog@UI@@) — dialog resources
    g_TypeId_Dialog = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Runtime, 0x1801faa88));

    // --- Persistence ---
    g_pPersistenceSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Runtime, 0x1801fc6b8));

    // --- UI infrastructure ---


    // System::MemoryAllocator::MemFree — static, single arg
    fn_MemFreeStatic = reinterpret_cast<fn_MemFreeStatic_t>(
        ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
    // fn_MemFree (singleton, ptr) is the two-arg shape the exe's MemFree call sites
    // were lifted with. It was declared but NEVER ASSIGNED, so it was null for the
    // life of the project: four callers guard it (and leak), and the ModDialog
    // 0x58-entry array push called it unguarded -- EXEC-of-0 on the first Mods click.
    // Retail's free is the static MemoryAllocator::MemFree(void*) (null-safe; the
    // singleton arg is dead), so the two-arg form is a thunk onto the static.
    fn_MemFree = [](void* /*singleton*/, void* ptr) {
        if (fn_MemFreeStatic) fn_MemFreeStatic(ptr);
    };

    // Object::InternalBasePtr::InternalBasePtr(Internal*)
    fn_InternalBasePtr_Ctor = reinterpret_cast<fn_InternalBasePtr_Ctor_t>(
        ResolveExport("??0InternalBasePtr@Object@@QEAA@PEAVInternal@1@@Z"));

    // ================================================================
    // Phase A1: UI Widget Functions (Runtime DLL exports)
    // ================================================================

    // ================================================================
    // Phase A2: Exe Type IDs (.rdata pointers)
    // ================================================================
    g_TypeId_ComponentEntity = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5d70));
    // ProgressElement type ID: exref at 0x1401515b8 → pointer → uint32_t typeId
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401515b8));
        if (exref && *exref) {
            g_TypeId_ProgressElement = reinterpret_cast<uint32_t*>(*exref);
        }
    }
    g_TypeId_ComponentDescription_FC = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7318));
    g_TypeId_ConsumeProduceComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7388));
    g_TypeId_RadiusComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4870));
    g_TypeId_ToolbarDescription_FC = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7f58));
    g_TypeId_ResourceComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7040));
    g_TypeId_RawMaterialDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7cb8));
    g_TypeId_MeshInstanceComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7e40));
    g_TypeId_DestroyComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7628));
    g_TypeId_WorkplaceComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c80e0));
    g_TypeId_WorkComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c8070));
    g_TypeId_WorkDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c80a8));
    g_TypeId_AttendanceDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c3f78));
    g_TypeId_ResourceLimitType = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5520));
    g_TypeId_ToolUI = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5788));

    // BridgeTool type IDs
    g_TypeId_CreateBridgeDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c73f8));
    // RemovalTool type IDs
    g_TypeId_RemovalDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c74a0));
    // g_TypeId_MapComponent_FC (DAT_1401c79e0) is resolved in the FindComponent block below
    // g_TypeId_MultiObjectComponent already resolved in Phase E below
    // BridgeTool helper functions (exe-internal)
    fn_BridgeTool_SnapEndCell = reinterpret_cast<fn_BridgeTool_SnapEndCell_t>(
        ResolveAddr(Module::Exe, 0x1400fb710));
    fn_BridgeTool_ValidatePath = reinterpret_cast<fn_BridgeTool_ValidatePath_t>(
        ResolveAddr(Module::Exe, 0x1400fb2d0));

    // DragPlacementTool/FencedTool type IDs
    g_TypeId_CreatePlacementDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7430));
    g_TypeId_CreateFencedDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4100));
    g_TypeId_AreaDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7a18));
    g_TypeId_FenceDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7a88));
    g_TypeId_SizeDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7468));

    // CheckboxElement type ID: exref at 0x1401516a8 → pointer → uint32_t typeId
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401516a8));
        if (exref && *exref) {
            g_TypeId_CheckboxElement = reinterpret_cast<uint32_t*>(*exref);
        }
    }
    // UI element base type ID: exref at 0x1401514f8 (used by ControlUI::Slot4)
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401514f8));
        if (exref && *exref) {
            g_TypeId_ButtonElement = reinterpret_cast<uint32_t*>(*exref);
        }
    }
    // Object::Internal type ID: exref at 0x140151660 (used by BuildRequirementUI::Slot4)
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151660));
        if (exref && *exref) {
            g_TypeId_Element = reinterpret_cast<uint32_t*>(*exref);
        }
    }
    // UI::ImageElement type ID: exref at 0x140151500 (used by EventNotifyDialog::Update and others)
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151500));
        if (exref && *exref) {
            g_TypeId_ImageElement = reinterpret_cast<uint32_t*>(*exref);
        }
    }
    // ControlComponent type ID: direct uint32 at DAT_1401c4e20
    g_TypeId_ControlComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4e20));

    // ================================================================
    // Phase A3: Game Singletons
    // ================================================================
    g_pGameWorldSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));
    g_pKeyMapSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3de8));
    g_pInputConfigSingletonExref = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140151738));

    // ================================================================
    // Phase A4: FindComponent type IDs (resolved from exe .rdata)
    // The old exe function pointers (fn_FindComponent_X) are eliminated.
    // All FindComponent variants are now inline C++ in engine_funcs.h.
    // The exe array helpers (fn_ExeArrayResize/2/3/Push) are gone entirely --
    // their callers are Collection::Array<T>::SetSize.
    // ================================================================
    g_TypeId_MapComponent_FC     = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c79e0));
    g_TypeId_StateMachineComponent_FC      = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7d98));
    g_TypeId_ConsumeProduceComponent_FC = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7350));
    g_TypeId_BuildComponent_FC        = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7008));
    g_TypeId_DestroyComponent_FC      = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c75f0));
    g_TypeId_TradeComponent_FC        = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4b80));
    g_TypeId_AttendanceComponent      = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c3f40));
    g_TypeId_PastureComponent         = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7ba0));
    g_TypeId_AnimalDescription        = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c6fd0));
    g_TypeId_OrchardComponent         = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c46b0));
    g_TypeId_ClearAreaComponent_FC              = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c71c8));
    g_TypeId_CitizenComponent_FC      = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7158));
    g_TypeId_StorageComponent_FC      = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7e78));
    g_TypeId_ResidenceComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7cf0));
    g_TypeId_CemeteryComponent        = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4090));
    g_TypeId_TombstoneComponent       = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4a30));
    g_TypeId_ImmigrationComponent     = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4480));
    g_TypeId_TrackerComponent         = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4b10));
    g_TypeId_ModelComponent_FC       = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7a50));

    fn_RadiusCalc = reinterpret_cast<fn_RadiusCalc_t>(
        ResolveAddr(Module::Exe, 0x140055430));
    fn_RadiusDisplay_Remove = reinterpret_cast<fn_RadiusDisplay_Remove_t>(
        ResolveAddr(Module::Exe, 0x1400b0d00));

    // DestroyComponent_CanReclaim/CanRebuild/Reclaim/Rebuild/Destroy: now static impls in destroyui.cpp

    fn_GetActiveProductIndex = reinterpret_cast<fn_GetActiveProductIndex_t>(
        ResolveAddr(Module::Exe, 0x140119470));
    // fn_CropUI_PopulateCombo — REIMPLEMENTED in code/Game/Dialog/cropui.cpp
    // fn_CropUI_GetExpectedCount — REIMPLEMENTED in code/Game/Dialog/cropui.cpp
    // CemeteryGetBurialCount — now static inline in cemeteryui.cpp

    // FindComponentByTypeId — now inline in engine_funcs.h (reimplemented from FUN_1400c3680)

    // EntityTrackingSet_BST_Delete — AVL node deleter (FUN_140025740)
    fn_EntityTrackingSet_BST_Delete = reinterpret_cast<fn_EntityTrackingSet_BST_Delete_t>(
        ResolveAddr(Module::Exe, 0x140025740));

    // EntityTrackingSet_Remove: now a static impl in citizenui.cpp

    // fn_FindComponent_Immigration — inline in engine_funcs.h (uses g_TypeId_ImmigrationComponent)
    // WorkplaceComponent_Toggle — FUN_14012fc00 (toggles workplace enabled state)
    fn_WorkplaceComponent_Toggle = reinterpret_cast<fn_WorkplaceComponent_Toggle_t>(
        ResolveAddr(Module::Exe, 0x14012fc00));

    // fn_FindComponent_Orchard — inline in engine_funcs.h (uses g_TypeId_OrchardComponent)
    // OrchardComponent_GetGrowth — FUN_140051640 (returns average growth ratio)
    fn_OrchardGetGrowth = reinterpret_cast<fn_OrchardGetGrowth_t>(
        ResolveAddr(Module::Exe, 0x140051640));
    // fn_FindComponent_Pasture — inline in engine_funcs.h (uses g_TypeId_PastureComponent)
    // PastureCalcMaxCount, PastureUI_UpdateSlider — now static inline in pastureui.cpp

    // ================================================================
    // Video::ImageBuffer (Runtime DLL exports — by mangled name, fallback to RVA)
    // ================================================================
    // ENDGAME B.3.9 EXA: `fn_ImageBuffer_Ctor` is gone — every exe builder
    // writes `::new (p) Video::ImageBuffer()`.

    fn_SaveLoad_Dtor = reinterpret_cast<fn_SaveLoad_Dtor_t>(
        ResolveExport("??1SaveLoad@Save@@QEAA@XZ"));

    // SaveLoad singleton: exe exref at 0x140151610 -> pointer to _singleton in Runtime
    {
        void*** exref = reinterpret_cast<void***>(ResolveAddr(Module::Exe, 0x140151610));
        if (exref && *exref) {
            g_pSaveLoadSingleton = *exref;
        }
        // Standalone: the 0x140151610 dispatch slot is not registered — resolve
        // Save::SaveLoad::_singleton straight from the Runtime export. The cell is
        // written by the SaveLoad ctor (GameUpdateLoop::Load step 25), so the
        // pointer-to-cell is valid from DLL load even while the cell is still null.
        if (!g_pSaveLoadSingleton) {
            g_pSaveLoadSingleton = reinterpret_cast<void**>(
                ResolveExport("?_singleton@SaveLoad@Save@@0PEAV12@EA"));
        }
    }

    fn_DateTime_AsString = reinterpret_cast<fn_DateTime_AsString_t>(
        ResolveExport("?AsString@DateTime@System@@QEBA?AVString@2@XZ"));

    // SaveLoadDialog::PopulateFileList — REMOVED, now reimplemented in saveloaddialog.cpp

    // Input::Device singleton: exe exref at 0x140151b80 -> pointer to _singleton in Runtime
    {
        void*** exref = reinterpret_cast<void***>(ResolveAddr(Module::Exe, 0x140151b80));
        if (exref && *exref) {
            g_pInputDeviceSingleton = *exref;
        } else {
            // Standalone fallback: the exe exref slot is only populated by Runtime
            // reflection in retail; standalone never wires it. Resolve the Runtime
            // DLL export `Input::Device::_singleton` directly.
            void** p = reinterpret_cast<void**>(
                ResolveExport("?_singleton@Device@Input@@0PEAV12@EA"));
            if (p) g_pInputDeviceSingleton = p;
        }
    }

    // Persistence::GetSavePath and FileSystem::GetDataDirectory resolved here
    // until ENDGAME B.3.4a closing 2d; both were 0-CALL and are gone, resolver,
    // slot fallback and all.
    // FileSystem singleton: exe dispatch at 0x140151d18 → pointer to Runtime singleton.
    // Original: MOV RAX,[0x140151d18]; MOV RCX,[RAX] (at 0x140077d5a)
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151d18));
        g_pFileSystemSingleton2 = exref ? reinterpret_cast<void**>(*exref) : nullptr;
        // 0x140151d18 is unregistered in standalone, so the line above yields NULL.
        // The Runtime exports the singleton POINTER itself as data; ResolveExport gives
        // the address of that pointer variable, which is exactly what this is.
        if (!g_pFileSystemSingleton2)
            g_pFileSystemSingleton2 = reinterpret_cast<void**>(
                ResolveExport("?_singleton@FileSystem@IO@@0PEAV12@EA"));
    }
    fn_LoadScenarioFromSave = reinterpret_cast<fn_LoadScenarioFromSave_t>(
        ResolveAddr(Module::Exe, 0x1400b49f0));

    // AchievementManager singleton (double-indirect .rdata dispatch at 0x1401514f0)
    g_ppAchievementManager = reinterpret_cast<void***>(
        ResolveAddr(Module::Exe, 0x1401514f0));

    // fn_FollowDialog_Close — inlined in followdialog.cpp Slot7 (FUN_140068f00)

    // UpgradeUIConfig type ID at DAT_1401c5868 (UI::DialogControllerConfig;
    // its +0x20 holds the upgrade target ComponentEntity template)
    g_TypeId_UpgradeUIConfig = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5868));

    // DestroyComponent_StartUpgrade — REMOVED, now reimplemented in destroyui.cpp

    // fn_ResidenceUI_GoToCitizen — now inline in residenceui.cpp ResidenceUI_GoToCitizen
    // ResidenceUI_ArrayPush now inline in residenceui.cpp
    // EventLogUI_ToggleNotification and NavigateToEvent now inline in eventlogui.cpp

    // WorkplaceUI_AdjustWorkerCount/ToggleWorkplace: now static impls in workplaceui.cpp
    // fn_FindComponent_WorkPlace — inline in engine_funcs.h (uses g_TypeId_StateMachineComponent_FC)
    fn_WorkplaceComp_SetMaxWorkers = reinterpret_cast<fn_WorkplaceComp_SetMaxWorkers_t>(
        ResolveAddr(Module::Exe, 0x14012fb00));
    fn_CitizenList_RequestWorker = reinterpret_cast<fn_CitizenList_RequestWorker_t>(
        ResolveAddr(Module::Exe, 0x14008d100));
    fn_CitizenList_RemoveWorker = reinterpret_cast<fn_CitizenList_RemoveWorker_t>(
        ResolveAddr(Module::Exe, 0x140091180));
    // fn_FindComponent_Position — inline in engine_funcs.h (uses g_TypeId_BuildComponent_FC)
    // EditTool type ID — from the exe exref used in EntityUI
    // The original uses _typeInfo_exref which is the same exref as EditTool's type
    // We resolve it from the address seen in EntityUI::Slot7
    g_TypeId_EditTool = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c6dd8));

    // EntityUI helpers — SetEditMode/SelectEntity now reimplemented in entityui.cpp
    // Resolve sub-functions needed by reimplementations:
    g_TypeId_ToolController = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c6eb8));
    g_TypeId_ToolSetting = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c6ef0));
    fn_AVL_Rebalance_0x38 = reinterpret_cast<fn_AVL_Rebalance_0x38_t>(
        ResolveAddr(Module::Exe, 0x140025920));
    // fn_GetDescPage/fn_BuildComplete: now static impls in buildui.cpp

    // HelpUI_Slot4 and HelpUI_Slot9 now inline in helpui.cpp

    // GoalDialog_ArrayPush, EventNotifyDialog_ArrayPush, ScenarioDialog_ArrayPush
    // now inline in their respective .cpp files
    fn_ExternalList_GetResourceCount = reinterpret_cast<fn_ExternalList_GetResourceCount_t>(
        ResolveExport("?GetResourceCount@ExternalList@Object@@QEBAHXZ"));
    // fn_EventNotifyDialog_ArrayRemove (0x140011670) — inlined as EventNotifyDialog_ArrayRemove_Impl in eventnotifydialog.cpp

    // Scenario type ID: direct uint32_t in .data at 0x1401c5b08
    // (Original Slot4 at 0x140078248: MOV EDX, dword ptr [0x1401c5b08])
    g_TypeId_Scenario = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c5b08));

    fn_ToolbarDialog_Ctor = reinterpret_cast<fn_ToolbarDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x1400b9910));
    // fn_Toolbar_ShowGroup — must call original (15+ direct callers share state)
    fn_Toolbar_ShowGroup = reinterpret_cast<fn_Toolbar_ShowGroup_t>(
        ResolveAddr(Module::Exe, 0x140102b60));
    // 0x14015f5a0 is the shared ToolUI child-dialog vtable. In standalone it is
    // DELIBERATELY left unregistered (registering it poisons the menu's BuildId
    // child — see reg_player_toolui.cpp), so ResolveAddr returns 0 here; only
    // overwrite when non-null so we KEEP the standalone replacement vtable that
    // RegisterPlayerToolUIBatch (AddressRegistryInit, runs earlier) installed.
    if (uintptr_t toolUIChildVt = ResolveAddr(Module::Exe, 0x14015f5a0))
        g_ToolUI_ChildDialog_Vtable = toolUIChildVt;
    // fn_Factory_CreateObject, g_TypeId_ToolController, g_TypeId_ToolSetting — resolved above (entityui section)
    // Toolbar item sort function
    // TypeInfo for Toolbar external (used by ExternalFactory::FindNext)
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x14015f980));
        if (exref && *exref) {
            g_TypeId_ToolbarExternal = reinterpret_cast<uint32_t*>(*exref);
        }
    }
    // Toolbar type ID at DAT_1401c6128 (filled by Toolbar_RegisterType at runtime)
    g_TypeId_ToolbarItem = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c6128));
    // RadioElement::FindCheckBoxes. NO FALLBACK (Wave 9 lane F deleted it, and
    // the record of why has to survive the deletion): FindCheckBoxes is an
    // OVERLOAD SET and the fallback named the wrong member of it until
    // 2026-09-07 -- 0x1800d5750 is the PRIVATE 1-arg sibling, a genuine entry
    // point that would NOT have faulted, just run radio-group tree surgery with
    // whatever the caller left in RDX. The public 0-arg one is 0x180047c00.
    // That is why "is the fallback address an entry point" was never a
    // sufficient check (docs/runtime_port/WAVE9_LANE_D.md, and the docstring of
    // tools/resolve_fallback_audit.py, which both keep the full story).
    // FUN_140100270: Find dialog by controller type
    fn_FindDialogByControllerType = reinterpret_cast<fn_FindDialogByControllerType_t>(
        ResolveAddr(Module::Exe, 0x140100270));
    // fn_Toolbar_FindEntity — inlined as Toolbar_FindEntity_Impl in toolbardialog.cpp
    // fn_Toolbar_CenterOnEntity — inlined as Toolbar_CenterOnEntity_Impl in engine_funcs.h
    // Sub-dependencies:
    g_TypeId_MultiObjectComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7ac0));
    fn_EntityPanel_SyncTracking = reinterpret_cast<fn_EntityPanel_SyncTracking_t>(
        ResolveAddr(Module::Exe, 0x1400666b0));
    fn_CameraSys_ComputeViewTransform = reinterpret_cast<fn_CameraSys_ComputeViewTransform_t>(
        ResolveAddr(Module::Exe, 0x1400983f0));
    // fn_ToolbarDialog_CloseSubDialog — inlined as ToolbarDialog_CloseSubDialog_Impl in toolbardialog.cpp
    // fn_ToolbarDialog_ChangeSpeed — inlined as ToolbarDialog_ChangeSpeed_Impl in toolbardialog.cpp
    // fn_ToolbarDialog_UpdateButtons — REPLACED with inline impl in toolbardialog.cpp
    // ToolbarElement type ID: exref at 0x1401516a8, used in ToolbarDialog::Update/OnEvent for IsDerivedFrom
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401516a8));
        if (exref && *exref) {
            g_TypeId_CheckElement = reinterpret_cast<uint32_t*>(*exref);
        }
    }
    // ToolbarDialog parent type ID: direct uint32 at DAT_1401c5d38, used in case 5 (CenterOnPoint)
    g_TypeId_ToolbarDialogParent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5d38));
    // ComboElement type ID: exref at 0x140151690 (double-indirect)
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151690));
        if (exref && *exref) {
            g_TypeId_ComboElement = reinterpret_cast<uint32_t*>(*exref);
        }
    }
    // fn_ResolveExternalByName — REPLACED with inline ResolveExternalByName_Impl in engine_funcs.h

    fn_EditTool_SelectEntity = reinterpret_cast<fn_EditTool_SelectEntity_t>(
        ResolveAddr(Module::Exe, 0x1400fd3b0));
    // fn_FindComponent_Residence — inline in engine_funcs.h (uses g_TypeId_StorageComponent_FC, same as Storage)
    g_pAgeMultiplier = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x140172198));
    g_pNotifyTimerDuration = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x1401721cc));
    fn_Time_Ctor_Float = reinterpret_cast<fn_Time_Ctor_Float_t>(
        ResolveExport("??0Time@System@@QEAA@M@Z"));
    // fn_Dialog_GetReference resolve removed 2026-09-07 (Wave 7 lane B): the
    // mangled name was not an export and the pointer had no callers. The real
    // function is ?GetReference@Dialog@UI@@QEBA?AV?$ExternalPtr@VExternal@Object@@@Object@@H@Z
    // (0x1800388d0) and it takes an int index. See engine_funcs.h.
    // HelpUI_ArrayResize now inline as HelpUI_ArrayResize_Impl in helpui.cpp

    // --- OptionsDialog support — Runtime DLL exports ---

    // --- OptionsDialog exe helpers — all REIMPLEMENTED in code/Game/Dialog/optionsdialog.cpp ---
    // fn_OptionsDialog_PopulateInputBindList — reimplemented
    // fn_OptionsDialog_OnInputBindClick — reimplemented
    // fn_Element_GetParent — now inline in optionsdialog.cpp (vtable call)
    g_pGameSettingsPtr = reinterpret_cast<int32_t**>(ResolveAddr(Module::Exe, 0x1401c3de0));
    // Audio::Configuration singleton: exe exref at 0x140151b78 -> pointer to singleton
    {
        void*** exref = reinterpret_cast<void***>(ResolveAddr(Module::Exe, 0x140151b78));
        if (exref && *exref) {
            g_pAudioConfigSingleton = *exref;
        } else {
            // Standalone fallback: point at Runtime DLL's Audio::Configuration::_singleton
            // storage (Runtime data var at 0x1801fe758 — set by Audio::Configuration ctor at
            // 0x1800ddeb0). The exe's exref slot only gets populated by reflection in retail;
            // standalone never wires it up. The slot may still be NULL here (ctor runs lazily
            // during Engine::Engine), but it's the right address — saving the slot pointer
            // means future *g_pAudioConfigSingleton reads see the live singleton.
            void** runtimeSlot = reinterpret_cast<void**>(
                ResolveAddr(Module::Runtime, 0x1801fe758));
            if (runtimeSlot) g_pAudioConfigSingleton = runtimeSlot;
        }
    }
    // Video::Configuration singleton: exe exref at 0x140151b70 -> pointer to singleton
    {
        void*** exref = reinterpret_cast<void***>(ResolveAddr(Module::Exe, 0x140151b70));
        if (exref && *exref) {
            g_pVideoConfigSingleton = *exref;
        } else {
            // Standalone fallback: point at Runtime DLL's Video::Configuration::_singleton
            // storage (Runtime data var at 0x1801fe7a0 — set by Video::Configuration ctor at
            // 0x1800f6f10, which initializes refresh num/den to 60/1 by default). The exe's
            // exref slot only gets populated by reflection in retail; standalone never wires
            // it up. The slot may still be NULL here (ctor runs lazily during Engine::Engine),
            // but it's the right address — saving the slot pointer means future
            // *g_pVideoConfigSingleton reads see the live singleton.
            //
            // Without this fallback, OptionsDialog reads savedRefreshNum/Den as 0, then a
            // resolution change calls Video::Device::ValidateConfiguration which divides
            // by den => SIGFPE.
            void** runtimeSlot = reinterpret_cast<void**>(
                ResolveAddr(Module::Runtime, 0x1801fe7a0));
            if (runtimeSlot) g_pVideoConfigSingleton = runtimeSlot;
        }
    }

    // --- StorageUI support ---
    // fn_StorageUI_SortItems — reimplemented in storageui.cpp as StorageUI_SortItems()

    // --- MapUI helpers — REIMPLEMENTED in code/Game/Dialog/mapui.cpp ---
    // fn_MapUI_SetupImage — reimplemented as MapUI_SetupImage()
    // fn_MapUI_HandleClick — reimplemented as MapUI_HandleClick()
    fn_MapUI_ProjectCornersToGround = reinterpret_cast<fn_MapUI_ProjectCornersToGround_t>(
        ResolveAddr(Module::Exe, 0x14006c5b0));

    // --- CitizenUI helpers ---
    // fn_FindComponent_Citizen — inline in engine_funcs.h (uses g_TypeId_CitizenComponent_FC)
    g_TypeId_CitizenDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7190));
    g_TypeId_StatusIconComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7e08));
    g_TypeId_CitizenAIComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c70e8));
    // fn_FindComponent_CitizenAI — inline in engine_funcs.h (uses g_TypeId_CitizenAIComponent)
    // fn_StatusBarUI_StatusIconHandler — REPLACED with static impl in statusbarui.cpp

    // StringTable type ID: exref at 0x140151488 -> pointer -> uint32_t typeId
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151488));
        if (exref && *exref) {
            g_TypeId_StringTable = reinterpret_cast<uint32_t*>(*exref);
        }
    }

    // HelpUI element type IDs from _typeInfo_exref addresses
    {
        auto resolveTypeId = [](uintptr_t exrefAddr) -> uint32_t* {
            void** exref = reinterpret_cast<void**>(exrefAddr);
            if (exref && *exref) return reinterpret_cast<uint32_t*>(*exref);
            return nullptr;
        };
        g_TypeId_HelpFirstElem    = resolveTypeId(ResolveAddr(Module::Exe, 0x1401517e8));
        g_TypeId_HelpComboElem    = resolveTypeId(ResolveAddr(Module::Exe, 0x140151690));
        g_TypeId_HelpDocTextElem  = resolveTypeId(ResolveAddr(Module::Exe, 0x1401517f0));
        g_TypeId_HelpContainerElem = resolveTypeId(ResolveAddr(Module::Exe, 0x1401517f8));
        g_TypeId_HelpButtonElem   = resolveTypeId(ResolveAddr(Module::Exe, 0x1401514f8));
    }

    // --- ProductionUI support ---
    // fn_FindComponent_Tracker — inline in engine_funcs.h (uses g_TypeId_TrackerComponent)
    // fn_TrackerComponent_Rebuild — inline in engine_funcs.h

    // --- ProfessionUI support ---
    // NO FALLBACK. The one that used to sit here said 0x18006a3e0, which is
    // mid-body of FUN_18006a390 (+0x50); GetPath is at 0x180006250. Corrected
    // by Wave 9 lane R, then deleted by lane F -- with the DLL gone every
    // fallback would fire at once into an unmapped module, so a null here is
    // the better failure (RtResolveMiss records the name; see resolve.h).
    // BSTRebalance — now static inline in professionui.cpp (BSTRebalance_Impl)
    // fn_ProfessionUI_HandleSpinner — reimplemented in professionui.cpp
    // fn_ProfessionUI_HandleButton — reimplemented in professionui.cpp

    fn_Configuration_CurrentVersion = reinterpret_cast<fn_Configuration_CurrentVersion_t>(
        ResolveExport("?CurrentVersion@Configuration@System@@SA?AVString@2@XZ"));

    fn_Configuration_CurrentBuildId = reinterpret_cast<fn_Configuration_CurrentBuildId_t>(
        ResolveExport("?CurrentBuildId@Configuration@System@@SA?AVString@2@XZ"));

    // MainMenuDialog Slot8 handler functions — REIMPLEMENTED, now resolve dialog ctor ptrs
    fn_NewGameDialog_Ctor = reinterpret_cast<fn_NewGameDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x140071040));
    fn_SaveLoadDialog_Ctor = reinterpret_cast<fn_SaveLoadDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x1400b8270));
    fn_OptionsDialog_Ctor = reinterpret_cast<fn_OptionsDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x140075150));
    // ScenarioDialog_CopyCtor (0x1400786b0) was unregistered and called UNGUARDED
    // from both MainMenuDialog_OpenScenario and MainMenuDialog_OpenTutorial; the
    // ctor also hardcoded a store of vtable_ScenarioDialog (0x14015d2d0), itself
    // never registered. Both are now ported in scenariodialog.cpp — wire directly
    // instead of resolving the dead raw address.
    fn_ScenarioDialog_Ctor = &ScenarioDialog_Ctor;
    fn_MapParamsDialog_Ctor = reinterpret_cast<fn_MapParamsDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x14006bd80));
    fn_ModDialog_Ctor = reinterpret_cast<fn_ModDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x140070030));

    // --- NewGameDialog support ---
    g_pNewGameSettings = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401c3de0));
    // fn_GenerateTownName — REIMPLEMENTED, now resolve inner helpers
    fn_NameDB_GenerateName = reinterpret_cast<fn_NameDB_GenerateName_t>(
        ResolveAddr(Module::Exe, 0x1400a4b30));
    g_TypeId_NameDatabase = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5a60));
    fn_StartNewGame = reinterpret_cast<fn_StartNewGame_t>(
        ResolveAddr(Module::Exe, 0x1400b4540));
    g_TypeId_MapGenerator = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c59b8));
    g_TypeId_ClimateGenerator = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5910));
    g_TypeId_TerrainGenerator = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5b40));

    // fn_UIDesc_AddControllers — REIMPLEMENTED as UIDesc_AddControllers_Impl()
    g_TypeId_UIDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7f90));

    // --- Player/Tool module: shared engine APIs ---
    // Graphics::Scene::AddObject — IAT dispatch slot 0x140151370
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151370));
        if (dispatch) fn_SceneAddObject = reinterpret_cast<fn_SceneAddObject_t>(*dispatch);
    }
    // Graphics::Scene::RemoveObject — IAT dispatch slot 0x140151360
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151360));
        if (dispatch) fn_SceneRemoveObject = reinterpret_cast<fn_SceneRemoveObject_t>(*dispatch);
    }
    // Math::Transform::FromYawPitchOffset — IAT dispatch slot 0x140151288;
    // the slot is empty in standalone so we also fall back to ResolveExport.
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151288));
        if (dispatch && *dispatch) {
        } else {
        }
    }
    // Graphics::Geometry::SetTransform reached 0 CALLs in ENDGAME B.3.5 (b35-close):
    // its readers are members of the real `Graphics::Geometry`, so the slot read
    // (exe dispatch 0x1401512e0) and its ResolveExport fallback are gone. The
    // registration of the ported body is unaffected.
    // Graphics::MeshInstance::UpdateHierarchy — IAT dispatch slot 0x140152888
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152888));
        if (dispatch) fn_MeshInstance_UpdateHierarchy = reinterpret_cast<fn_MeshInstance_UpdateHierarchy_t>(*dispatch);
    }

    // --- Graphics::EdgeMesh family (selection-outline tessellation) ---
    // Resolve by mangled name (Runtime exports). The exe dispatch slots that
    // reach these (0x140151338/348/368/378/380/388/390, 0x1401513b8/f0/f8,
    // 0x140152a78/b60) are empty in standalone, so ResolveExport is primary.
    fn_MemAllocStatic = reinterpret_cast<fn_MemAllocStatic_t>(
        ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z"));
    fn_Box_Ctor = reinterpret_cast<fn_Box_Ctor_t>(
        ResolveExport("??0Box@Volume@@QEAA@XZ"));
    fn_Box_Extend = reinterpret_cast<fn_Box_Extend_t>(
        ResolveExport("?Extend@Box@Volume@@QEBA?AV12@AEBVVector3@Math@@@Z"));
    fn_Box_Assign = reinterpret_cast<fn_Box_Assign_t>(
        ResolveExport("??4Box@Volume@@QEAAAEAV01@AEBV01@@Z"));
    fn_EdgeMesh_TriangleVertex_Ctor = reinterpret_cast<fn_EdgeMesh_TriangleVertex_Ctor_t>(
        ResolveExport("??0TriangleVertex@EdgeMesh@Graphics@@QEAA@AEBVVector3@Math@@@Z"));
    fn_EdgeMesh_EdgeVertex_Ctor = reinterpret_cast<fn_EdgeMesh_EdgeVertex_Ctor_t>(
        ResolveExport("??0EdgeVertex@EdgeMesh@Graphics@@QEAA@AEBUTriangleVertex@12@0H@Z"));
    // CharacterComponent_impl_08c0 (0x1401008c0) — tile debug overlay rendering (exe function)
    fn_TileRender = reinterpret_cast<fn_TileRender_t>(
        ResolveAddr(Module::Exe, 0x1401008c0));
    // CharacterComponent_impl_f5d0 (0x14009f5d0) — single-tile terrain validity check (exe function)
    // FUN_1400dbf90 — fenced placement parameter computation (exe function)
    fn_FencedPlacementParams = reinterpret_cast<fn_FencedPlacementParams_t>(
        ResolveAddr(Module::Exe, 0x1400dbf90));
    // TradeWork_impl_0d00 (0x1400b0d00) — highlight/overlay removal (exe function)
#ifdef _DEBUG
    printf("[EngineFuncs] Player/Tool APIs resolved:\n");
    printf("  SceneAddObject            = %p\n", (void*)fn_SceneAddObject);
    printf("  SceneRemoveObject         = %p\n", (void*)fn_SceneRemoveObject);
    printf("  MeshInstance_UpdateHier   = %p\n", (void*)fn_MeshInstance_UpdateHierarchy);
    printf("  TileRender                = %p\n", (void*)fn_TileRender);
    printf("  FencedPlacementParams     = %p\n", (void*)fn_FencedPlacementParams);
#endif

    fn_Configuration_GetVersionString = reinterpret_cast<fn_Configuration_GetVersionString_t>(
        ResolveExport("?GetVersionString@Configuration@System@@SA?AVString@2@I_N@Z"));

    // String::operator+= — non-exported, resolve from exe dispatch table at 0x140151a00
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151a00));
    }
    // System::operator+ — non-exported, resolve from exe dispatch table at 0x140151a08
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151a08));
    }
    // Filename::operator< — non-exported, resolve from exe dispatch table at 0x140151a20
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151a20));
    }
    // ModDetails_StringArrayPush — now static inline in moddetailsdialog.cpp

    // --- IO::Modification ---
    // Modification singleton: exe exref at 0x1401519e8 contains a pointer to
    // IO::Modification::_singleton (in Runtime DLL). Dereference once at init.
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401519e8));
        g_ppModificationSingleton = exref ? reinterpret_cast<void**>(*exref) : nullptr;
    }
    // Modification::GetCount / ::GetModification (Runtime 0x18000da90 /
    // 0x18000da80) were resolved here until ENDGAME B.3.4a closing 2d; both
    // reached 0 CALLs. The ported bodies stay registered by address in
    // runtime/src/io/modification.cpp — only these two readers died.

    // fn_DialogInstance_AddController resolve removed 2026-09-07 (Wave 7 lane B).
    // The mangled name was not an export, so this ALWAYS fell through to exe
    // dispatch slot 0x140151a60 — which is ?AddChild@Element@UI@@…, not
    // AddController. (The correctly-named six-argument overload was resolved as
    // fn_DialogInstance_AddController6 until the ENDGAME B.3.6 closing pass, which
    // retired it with zero callers; UIDesc_AddControllers_Impl calls the member.)

    fn_ModDialog_CloseAndReload = reinterpret_cast<fn_ModDialog_CloseAndReload_t>(
        ResolveAddr(Module::Exe, 0x14006e9b0));
    fn_MainMenu_Reload = reinterpret_cast<fn_MainMenu_Reload_t>(
        ResolveAddr(Module::Exe, 0x1400b4790));
    fn_Game_ReloadWithSave = reinterpret_cast<fn_Game_ReloadWithSave_t>(
        ResolveAddr(Module::Exe, 0x1400b49f0));
    // ??0SavedState@Save@@QEAA@XZ is a real export (retail 0x1800c0ee0,
    // ordinal 362), verified against the shipped export directory, so this
    // always resolves.
    //
    // The fallback that used to sit here has been DELETED rather than
    // corrected, and it is worth saying why. It read
    //     ResolveAddr(Module::Runtime, 0x18006f330)   // "might be in exe"
    // and every part of that was wrong: 0x18006f330 is not the SavedState
    // constructor, and it is not a function entry at all -- it is 0x90 bytes
    // INSIDE the body of FUN_18006f2a0 (entry 0x18006f2a0, body ends
    // 0x18006f3ca), an unrelated internal. Taking it would have jumped into
    // the middle of another function with a SavedState* in RCX. The comment
    // also said "might be in exe" while the code passed Module::Runtime.
    //
    // It was dead code -- the name path never fails -- which is exactly what
    // made it worth removing: it is the phantom-name pattern with the failure
    // pre-loaded, waiting for someone to rename or remove the name above.
    // Wave 5 lane C found it; the mid-function address is the same class of
    // defect as the 2026-08-28 sweep that found 0x1800f8f50 pointing into
    // Frustum::Contains. Deleted, not repointed, because Wave 5 registers this
    // export and a second door earns nothing.
    fn_SavedState_Ctor = reinterpret_cast<fn_SavedState_Ctor_t>(
        ResolveExport("??0SavedState@Save@@QEAA@XZ"));

    // --- IO::Serializer (shared globals) ---
    // The five ??0Serializer / ??1Serializer / Serialize(int&) / Serialize(bool&)
    // / GetVersion exports were resolved into globals here until ENDGAME B.3.4a
    // closing 2d. IO::Serializer is a real class now, so nothing read them.

    // --- TownHallUI support ---
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151de0));
        if (exref && *exref) {
            g_TypeId_TabElement = reinterpret_cast<uint32_t*>(*exref);
        }
    }
    // fn_TownHallUI_InitGeneral — reimplemented in townhallui.cpp
    // fn_TownHallUI_InitGraph — reimplemented in townhallui.cpp
    // fn_TownHallUI_UpdateGraph — reimplemented in townhallui.cpp
    // fn_TownHallUI_UpdateGeneral — reimplemented in townhallui.cpp
    // fn_TownHallUI_UpdateProfessions — reimplemented in townhallui.cpp
    // fn_TownHallUI_OnProfessionButton — reimplemented in townhallui.cpp
    // fn_TownHallUI_OnProfessionSpinner — reimplemented in townhallui.cpp
    g_pGameSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    // --- GroupRating helpers ---
    fn_GroupRating_Init = reinterpret_cast<fn_GroupRating_Init_t>(
        ResolveAddr(Module::Exe, 0x14006abf0));
    fn_GroupRating_Update = reinterpret_cast<fn_GroupRating_Update_t>(
        ResolveAddr(Module::Exe, 0x14006aed0));

    // --- Production stat query ---
    fn_QueryProductionStat = reinterpret_cast<fn_QueryProductionStat_t>(
        ResolveAddr(Module::Exe, 0x140090f20));

    // --- Citizen assignment (spinner) ---
    fn_CitizenList_FindIdle = reinterpret_cast<fn_CitizenList_FindIdle_t>(
        ResolveAddr(Module::Exe, 0x14008cbf0));
    fn_CitizenList_FindWorking = reinterpret_cast<fn_CitizenList_FindWorking_t>(
        ResolveAddr(Module::Exe, 0x14008ca20));
    fn_CitizenList_FindStudent = reinterpret_cast<fn_CitizenList_FindStudent_t>(
        ResolveAddr(Module::Exe, 0x14008c5f0));
    fn_CitizenList_FindAny = reinterpret_cast<fn_CitizenList_FindAny_t>(
        ResolveAddr(Module::Exe, 0x14008c4b0));

    // --- Array push helpers ---
    fn_ProfArrayPush = reinterpret_cast<fn_ProfArrayPush_t>(
        ResolveAddr(Module::Exe, 0x140015800));
    fn_ProdArrayPush = reinterpret_cast<fn_ProdArrayPush_t>(
        ResolveAddr(Module::Exe, 0x1400159b0));

    // --- BST helpers ---
    fn_BST_Rebalance_0x30 = reinterpret_cast<fn_BST_Rebalance_0x30_t>(
        ResolveAddr(Module::Exe, 0x140013480));
    fn_BST_Rebalance_0x28 = reinterpret_cast<fn_BST_Rebalance_0x28_t>(
        ResolveAddr(Module::Exe, 0x1400149a0));
    fn_BST_Insert_0x14 = reinterpret_cast<fn_BST_Insert_0x14_t>(
        ResolveAddr(Module::Exe, 0x140014890));
    fn_BST_FreeNodes = reinterpret_cast<fn_BST_FreeNodes_t>(
        ResolveAddr(Module::Exe, 0x140014c30));
    // --- Inventory sort ---
    fn_InventorySort_Asc = reinterpret_cast<fn_InventorySort_Asc_t>(
        ResolveAddr(Module::Exe, 0x140015200));
    fn_InventorySort_Desc = reinterpret_cast<fn_InventorySort_Desc_t>(
        ResolveAddr(Module::Exe, 0x140015500));

    // fn_GoToEntity — inlined as Toolbar_CenterOnEntity_Impl in engine_funcs.h

    // --- SpriteCaptureUI support ---
    // EditElement type ID: exref at 0x140151b50 (double-indirect)
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151b50));
        if (exref && *exref) g_TypeId_EditElement = reinterpret_cast<uint32_t*>(*exref);
    }
    // CaptureBoxElement type ID: direct dword at 0x1401c5638
    g_TypeId_CaptureBoxElement = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5638));
    // ButtonElement type ID: exref at 0x1401514f8 (shared with HelpUI)
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401514f8));
        if (exref && *exref) g_TypeId_HelpButtonElem = reinterpret_cast<uint32_t*>(*exref);
    }

    // TradeUI support
    // fn_FindComponent_Building — inline in engine_funcs.h (uses g_TypeId_TradeComponent_FC)
    g_TypeInfo_ValueDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4bf0));
    g_TypeInfo_TradeDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4bb8));
    fn_DynArray_Clear_0x18 = reinterpret_cast<fn_DynArray_Clear_0x18_t>(
        ResolveAddr(Module::Exe, 0x140016200));
    fn_IntroSort_ByName = reinterpret_cast<fn_IntroSort_ByName_t>(
        ResolveAddr(Module::Exe, 0x1400162d0));
    fn_IntroSort_ByComponent = reinterpret_cast<fn_IntroSort_ByComponent_t>(
        ResolveAddr(Module::Exe, 0x1400165d0));
    // fn_TradeUI_RefreshSellTable/RefreshBuyTable — now reimplemented in tradeui.cpp
    fn_DynArray_Clear_0x20 = reinterpret_cast<fn_DynArray_Clear_0x20_t>(
        ResolveAddr(Module::Exe, 0x140016050));
    fn_DynArray_Clear_0x18_Buy = reinterpret_cast<fn_DynArray_Clear_0x18_t>(
        ResolveAddr(Module::Exe, 0x140016130));
    fn_Building_GetOrderName = reinterpret_cast<fn_Building_GetOrderName_t>(
        ResolveAddr(Module::Exe, 0x140015fd0));
    fn_Building_GetBuyEntries = reinterpret_cast<fn_Building_GetBuyEntries_t>(
        ResolveAddr(Module::Exe, 0x140059670));
    fn_TradeUI_OnTradeButton = reinterpret_cast<fn_TradeUI_OnTradeButton_t>(
        ResolveAddr(Module::Exe, 0x140081420));
    // TableElement type ID — dispatch slot at 0x140151510 contains a POINTER to the type ID
    // Original asm: MOV RAX,[0x140151510]; MOV EDX,[RAX] (double deref)
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151510));
        if (exref && *exref) g_TypeId_TableElement = reinterpret_cast<uint32_t*>(*exref);
    }
    // SpinnerElement type ID — dispatch slot at 0x1401516c8 contains a POINTER to the type ID
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401516c8));
        if (exref && *exref) g_TypeId_NumberElement = reinterpret_cast<uint32_t*>(*exref);
    }
    // TextElement type ID — exref at 0x140151508
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151508));
        if (exref && *exref) g_TypeId_TextElement = reinterpret_cast<uint32_t*>(*exref);
    }
    // LabelElement type ID — exref at 0x140151668
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151668));
        if (exref && *exref) g_TypeId_LabelElement = reinterpret_cast<uint32_t*>(*exref);
    }
    // NumberEditElement type ID — exref at 0x140151cb0
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151cb0));
        if (exref && *exref) g_TypeId_NumberEditElement = reinterpret_cast<uint32_t*>(*exref);
    }
    // SliderElement type ID — exref at 0x140151cf0
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151cf0));
        if (exref && *exref) g_TypeId_SpinnerElement = reinterpret_cast<uint32_t*>(*exref);
    }
    // SelectDescription type ID — DAT_1401c48a8
    g_TypeId_SelectDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c48a8));
    fn_Building_GetSellableCount = reinterpret_cast<fn_Building_GetSellableCount_t>(
        ResolveAddr(Module::Exe, 0x14005a520));
    fn_StorageTree_Insert = reinterpret_cast<fn_StorageTree_Insert_t>(
        ResolveAddr(Module::Exe, 0x14000e1c0));
    fn_Building_AddBuyToStorage = reinterpret_cast<fn_Building_AddBuyToStorage_t>(
        ResolveAddr(Module::Exe, 0x14005ace0));
    fn_Building_AddPurchaseOrder = reinterpret_cast<fn_Building_AddPurchaseOrder_t>(
        ResolveAddr(Module::Exe, 0x140059490));

    // --- ModDialog::Slot4 support (Graphics::MaterialInstance, UI::SpriteSheet, etc.) ---
    // MaterialInstance::{SetMaterial, AddTexture, BuildParameters} (exe dispatch
    // 0x140151870 / 0x140151878 / 0x140151880) reached 0 CALLs in ENDGAME B.3.5
    // (b35-close): they are members of the real `Graphics::MaterialInstance`. Both
    // resolve paths this file had for each — the slot read here and the by-name
    // fallback further down — are gone with them.
    // ModDialog array push: FUN_140012a60 is unregistered in standalone; it was called
    // UNGUARDED from ModDialog_RefreshDisplay_Impl (moddialog.cpp), so adding an
    // installed mod's thumbnail entry EXEC'd 0. Byte-faithful C++ port instead.
    fn_ModDialog_ArrayPush = &fn_ModDialog_ArrayPush58_CppImpl;
    // BrowserController type ID: DAT_1401c5210
    g_TypeId_BrowserController = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5210));
    // Material type ID: dispatch at 0x140151858 (double-indirect)
    {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151858));
        if (d && *d) g_TypeId_Material = reinterpret_cast<uint32_t*>(*d);
    }
    // UV scale float: DAT_14017214c
    g_fUVScale = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x14017214c));


    // --- NoiseUI helpers ---
    // (g_NoiseUI_ImageBufferVtable, the exe's copy 0x14015c8c0 of ImageBuffer's
    // own table, went at ENDGAME B.3.9 EXA with its hand-built registration.)
    // fn_NoiseUI_SetupImage — REIMPLEMENTED in code/Game/Dialog/noiseui.cpp
    // fn_NoiseUI_GenerateNoise — REIMPLEMENTED in code/Game/Dialog/noiseui.cpp
    // NoiseUI runtime dependencies
    fn_PerlinNoise_Ctor = reinterpret_cast<fn_PerlinNoise_Ctor_t>(
        ResolveExport("??0PerlinNoise@Utility@@QEAA@MMHH@Z"));
    fn_PerlinNoise_Generate = reinterpret_cast<fn_PerlinNoise_Generate_t>(
        ResolveExport("?Generate@PerlinNoise@Utility@@QEBAMMM@Z"));
    // Random default ctor — already resolved as fn_Random_Default_Ctor above

    // --- DebugUI helpers ---
    // Weather_ResetTime (exe 0x1400944e0)
    fn_Weather_ResetTime = reinterpret_cast<fn_Weather_ResetTime_t>(
        ResolveAddr(Module::Exe, 0x1400944e0));
    // DiseaseDescription type ID (DAT_1401c5948)
    g_TypeId_DiseaseDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5948));
    // DrawableType type ID (DAT_1401c47c8)
    g_TypeId_DrawableType = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c47c8));

    // --- DebugUI Slot9 sub-functions ---
    // fn_DebugUI_OnDrawComboChanged removed — now reimplemented
    // Sub-functions for the reimplementation:
    fn_DrawGeometry_Ctor = reinterpret_cast<fn_DrawGeometry_Ctor_t>(
        ResolveAddr(Module::Exe, 0x1400d3f10));
    fn_DrawGeometry_Init = reinterpret_cast<fn_DrawGeometry_Init_t>(
        ResolveAddr(Module::Exe, 0x1400d3f80));
    fn_AudioObjectGroup_Ctor = reinterpret_cast<fn_AudioObjectGroup_Ctor_t>(
        ResolveAddr(Module::Exe, 0x1400c3720));
    fn_AudioScene_RegisterObject = reinterpret_cast<fn_AudioScene_RegisterObject_t>(
        ResolveAddr(Module::Exe, 0x1400c48a0));
    fn_Entity_FindAudioComponent = reinterpret_cast<fn_Entity_FindAudioComponent_t>(
        ResolveAddr(Module::Exe, 0x140002280));
    // ENDGAME B.3.5 (b35-K2): `fn_Graphics_Scene_AddGlobalObject`,
    // `fn_Graphics_Scene_AddObject`, `fn_Graphics_GraphicsMesh_GetMaterialList`,
    // `fn_Graphics_MeshInstance_SetMaterialInstance` and
    // `fn_Graphics_ParticleInstance_IncrementInstanceIndex` are GONE — zero callers
    // tree-wide — and the dump below lost their five lines with them. The two ctor
    // doors keep their rows.

    // --- MenuScene 3D rendering (Phase A) ---
    fn_Graphics_Renderer_Ctor = reinterpret_cast<fn_Graphics_Renderer_Ctor_t>(
        ResolveExport("??0Renderer@Graphics@@QEAA@XZ"));
    fn_Graphics_Scene_Ctor = reinterpret_cast<fn_Graphics_Scene_Ctor_t>(
        ResolveExport("??0Scene@Graphics@@QEAA@XZ"));
    fn_Volume_Frustum_Ctor = reinterpret_cast<fn_Volume_Frustum_Ctor_t>(
        ResolveExport("??0Frustum@Volume@@QEAA@AEBVScalar@Math@@000@Z"));
    fn_Graphics_Camera_Ctor = reinterpret_cast<fn_Graphics_Camera_Ctor_t>(
        ResolveExport("??0Camera@Graphics@@QEAA@AEBVFrustum@Volume@@AEBVTransform3@Math@@_N@Z"));
    // ENDGAME B.3.5 (b35-K2): `fn_Graphics_SunLight_Set`,
    // `fn_Graphics_MeshInstance_SetMesh`, `fn_Video_Context_SetFog` and
    // `fn_Graphics_Camera_SetFrustum` are GONE — zero callers tree-wide. The three
    // ctor doors in this block stay.


    // --- DebugUI deep-reimpl helpers ---
    g_TypeId_FireComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c41e0));
    // g_TypeId_CitizenComponent removed — was duplicate of g_TypeId_RawMaterialDescription (DAT_1401c7cb8)
    fn_Game_DestroyEntity = reinterpret_cast<fn_Game_DestroyEntity_t>(
        ResolveAddr(Module::Exe, 0x140091e60));
    fn_FireComponent_Ignite = reinterpret_cast<fn_FireComponent_Ignite_t>(
        ResolveAddr(Module::Exe, 0x140049610));
    fn_DiseaseComponent_Infect = reinterpret_cast<fn_DiseaseComponent_Infect_t>(
        ResolveAddr(Module::Exe, 0x1400483a0));
    fn_StorageComponent_GetCapacity = reinterpret_cast<fn_StorageComponent_GetCapacity_t>(
        ResolveAddr(Module::Exe, 0x140128570));
    fn_StorageComponent_SpawnItem = reinterpret_cast<fn_StorageComponent_SpawnItem_t>(
        ResolveAddr(Module::Exe, 0x140128670));
    fn_StorageCanAdd = reinterpret_cast<fn_StorageCanAdd_t>(
        ResolveAddr(Module::Exe, 0x14012cd80));
    fn_StorageAddItem = reinterpret_cast<fn_StorageAddItem_t>(
        ResolveAddr(Module::Exe, 0x14012d000));
    fn_EntityFactory_CreateEntity = reinterpret_cast<fn_EntityFactory_CreateEntity_t>(
        ResolveAddr(Module::Exe, 0x1400c3720));
    fn_EntityList_RegisterEntity = reinterpret_cast<fn_EntityList_RegisterEntity_t>(
        ResolveAddr(Module::Exe, 0x1400c48a0));
    g_TypeId_InteractComponent_FC = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7900));
    g_TypeId_InteractDescription_FC = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7938));
    fn_ResidenceComponent_GetOccupantCount = reinterpret_cast<fn_ResidenceComponent_GetOccupantCount_t>(
        ResolveAddr(Module::Exe, 0x140123070));
    fn_ResidenceComponent_GetSpawnPoint = reinterpret_cast<fn_ResidenceComponent_GetSpawnPoint_t>(
        ResolveAddr(Module::Exe, 0x140122e20));
    fn_CitizenComponent_Setup = reinterpret_cast<fn_CitizenComponent_Setup_t>(
        ResolveAddr(Module::Exe, 0x140117080));
    fn_DisasterComponent_Infect = reinterpret_cast<fn_DisasterComponent_Infect_t>(
        ResolveAddr(Module::Exe, 0x140116b90));

    // --- SaveLoadDialog::Load helpers ---
    // ENDGAME B.3.9 EXA: the loop doors and exe loop vtable reads are gone (the
    // builders construct the real loop classes).
    fn_Random_Default_Ctor = reinterpret_cast<fn_Random_Default_Ctor_t>(
        ResolveExport("??0Random@System@@QEAA@XZ"));
    // fn_SetEngineNextLoop — REPLACED with inline SetEngineNextLoop() in engine_funcs.h
    // fn_SetDialogLoaderNextLoop — REPLACED with inline SetDialogLoaderNextLoop() in engine_funcs.h
    // fn_GetLoadingSoundString — REPLACED with inline GetLoadingSoundString() in engine_funcs.h
    fn_AudioScene_Ctor = reinterpret_cast<fn_AudioScene_Ctor_t>(
        ResolveExport("??0Scene@Audio@@QEAA@XZ"));

    // --- Engine globals (direct memory addresses) ---
    g_pLoader              = reinterpret_cast<void*>(ResolveAddr(Module::Runtime, 0x1801fe7b8));
    g_pLoaderFlags         = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Runtime, 0x1801fe7c0));
    g_pLoaderCompletionEvent = reinterpret_cast<HANDLE*>(ResolveAddr(Module::Runtime, 0x1801fe7e0));
    // Application singleton: exe exref at 0x140152368 contains a pointer to
    // Application::_singleton (in Runtime DLL). Dereference once at init to get
    // the actual singleton variable, so *g_pAppSingleton yields Application*.
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152368));
        g_pAppSingleton = exref ? reinterpret_cast<void**>(*exref) : nullptr;
    }

#ifdef _DEBUG
    printf("[EngineFuncs] Timer globals:\n");
    printf("  _ticksPerSecond       @ %p = %lld\n", (void*)g_Timer_ticksPerSecond,
           g_Timer_ticksPerSecond ? *g_Timer_ticksPerSecond : -1);
    printf("  _secondsPerTick       @ %p = %f\n", (void*)g_Timer_secondsPerTick,
           g_Timer_secondsPerTick ? *g_Timer_secondsPerTick : -1.0);
    printf("  _systemTicksPerSecond @ %p = %lld\n", (void*)g_Timer_systemTicksPerSecond,
           g_Timer_systemTicksPerSecond ? *g_Timer_systemTicksPerSecond : -1);
    printf("  _systemInitialTick    @ %p = %lld\n", (void*)g_Timer_systemInitialTick,
           g_Timer_systemInitialTick ? *g_Timer_systemInitialTick : -1);
    printf("[EngineFuncs] Functions:\n");
    printf("  MemAlloc                  = %p\n", (void*)fn_MemAlloc);
    printf("  MemAllocSingleton         = %p\n", g_pMemAllocSingleton);
    printf("  VideoDevice::_singleton   = %p -> %p\n", (void*)g_pVideoDeviceSingleton,
           g_pVideoDeviceSingleton ? *g_pVideoDeviceSingleton : nullptr);
    printf("  ExtFactory::_singleton    = %p -> %p\n", (void*)g_pExternalFactorySingleton,
           g_pExternalFactorySingleton ? *g_pExternalFactorySingleton : nullptr);
    printf("  IsDerivedFrom             = %p\n", (void*)fn_IsDerivedFrom);
    printf("  Dialog TypeId             = %p -> 0x%x\n", (void*)g_TypeId_Dialog,
           g_TypeId_Dialog ? *g_TypeId_Dialog : 0);
    printf("  Persistence::_singleton   = %p -> %p\n", (void*)g_pPersistenceSingleton,
           g_pPersistenceSingleton ? *g_pPersistenceSingleton : nullptr);
    printf("[EngineFuncs] UI infrastructure:\n");
    printf("  MemFreeStatic             = %p\n", (void*)fn_MemFreeStatic);
    printf("  InternalBasePtr::Ctor     = %p\n", (void*)fn_InternalBasePtr_Ctor);
    printf("[EngineFuncs] Phase A1 — UI Widget Functions:\n");
    printf("[EngineFuncs] Phase A2 — Type IDs:\n");
    printf("  ComponentDescription      = %p -> 0x%x\n", (void*)g_TypeId_ComponentDescription_FC,
           g_TypeId_ComponentDescription_FC ? *g_TypeId_ComponentDescription_FC : 0);
    printf("  DestroyComponent          = %p -> 0x%x\n", (void*)g_TypeId_DestroyComponent,
           g_TypeId_DestroyComponent ? *g_TypeId_DestroyComponent : 0);
    printf("  MeshInstanceComponent     = %p -> 0x%x\n", (void*)g_TypeId_MeshInstanceComponent,
           g_TypeId_MeshInstanceComponent ? *g_TypeId_MeshInstanceComponent : 0);
    printf("[EngineFuncs] Phase A3 — Singletons:\n");
    printf("  GameWorld                 = %p -> %p\n", (void*)g_pGameWorldSingleton,
           g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr);
    printf("  KeyMap (container)        = %p -> %p\n", (void*)g_pKeyMapSingleton,
           g_pKeyMapSingleton ? *g_pKeyMapSingleton : nullptr);
    printf("  KeyMap (object exref)     = %p -> %p\n", (void*)g_pInputConfigSingletonExref,
           g_pInputConfigSingletonExref ? *g_pInputConfigSingletonExref : nullptr);
    printf("[EngineFuncs] Phase A4 -- FindComponent type IDs (C++ reimpl):\n");
    printf("  MapComponent              = %p -> 0x%x\n", (void*)g_TypeId_MapComponent_FC,
           g_TypeId_MapComponent_FC ? *g_TypeId_MapComponent_FC : 0);
    printf("  CemeteryComponent         = %p -> 0x%x\n", (void*)g_TypeId_CemeteryComponent,
           g_TypeId_CemeteryComponent ? *g_TypeId_CemeteryComponent : 0);
    // DestroyComponent_Reclaim/Rebuild/Destroy: now implemented as statics in destroyui.cpp
    printf("[EngineFuncs] Globals:\n");
    printf("  _loader                   = %p\n", g_pLoader);
    printf("  _loader.m_flags           = %p -> 0x%x\n", (void*)g_pLoaderFlags,
           g_pLoaderFlags ? *g_pLoaderFlags : 0);
    printf("  _loader.m_completionEvent = %p -> %p\n", (void*)g_pLoaderCompletionEvent,
           g_pLoaderCompletionEvent ? *g_pLoaderCompletionEvent : nullptr);
    printf("  App::_singleton           = %p -> %p\n", (void*)g_pAppSingleton,
           g_pAppSingleton ? *g_pAppSingleton : nullptr);
#endif

    // FileSystem singleton: exe exref at 0x140151d18 -> pointer to _singleton in Runtime
    {
        void*** exref = reinterpret_cast<void***>(ResolveAddr(Module::Exe, 0x140151d18));
        if (exref && *exref)
            g_pFileSystemSingleton = *exref;
    }

    // --- Stage 3: OptionsDialog Runtime exports ---

    // Video::Configuration

    // Input::Configuration::_singleton
    {
        void** p = reinterpret_cast<void**>(
            ResolveExport("?_singleton@Configuration@Input@@0PEAV12@EA"));
        if (p)
            g_pInputConfigSingleton = p;
    }

    // IO::Persistence WriteSettings / ReadSettings resolved here until
    // ENDGAME B.3.4a closing 2d; both were 0-CALL.

    // Video::Device

    // Object::BaseTypeInfo
    fn_BaseTypeInfo_Ctor = reinterpret_cast<fn_BaseTypeInfo_Ctor_t>(
        ResolveExport("??0BaseTypeInfo@Object@@QEAA@PEBD00P6APEAVBase@1@XZP6APEAV21@PEAX@Z@Z"));

    // Engine::MainLoop::_singleton (protected, @1)
    {
        void** p = reinterpret_cast<void**>(
            ResolveExport("?_singleton@MainLoop@Engine@@1PEAV12@EA"));
        if (p)
            g_pMainLoopSingleton = p;
    }

    // Stage 3: OptionsDialog exe helpers
    fn_SaveGameSettings = reinterpret_cast<fn_SaveGameSettings_t>(
        ResolveAddr(Module::Exe, 0x140098970));
    fn_ReadGameSettings = reinterpret_cast<fn_ReadGameSettings_t>(
        ResolveAddr(Module::Exe, 0x140098b60));
    fn_ApplyKeyMapToUI = reinterpret_cast<fn_ApplyKeyMapToUI_t>(
        ResolveAddr(Module::Exe, 0x140099260));
    fn_UpdateAutoSaveTimer = reinterpret_cast<fn_UpdateAutoSaveTimer_t>(
        ResolveAddr(Module::Exe, 0x14005e120));
    g_TypeId_AutoSaveDialogType = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4d08));
    fn_RegisterDefaultInputBindings = reinterpret_cast<fn_RegisterDefaultInputBindings_t>(
        ResolveAddr(Module::Exe, 0x1400992f0));

    fn_InternalBasePtr_DeletePtr = reinterpret_cast<fn_InternalBasePtr_DeletePtr_t>(
        ResolveExport("?DeletePtr@InternalBasePtr@Object@@QEAAXXZ"));

    // Dialog constructors — exe functions
    fn_OptionInputBindDialog_Ctor = reinterpret_cast<fn_OptionInputBindDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x140071d30));
    fn_MessageDialog_Ctor = reinterpret_cast<fn_MessageDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x14006d300));

    // fn_Serializer_SerializeBulk read slot 0x140152400 here until ENDGAME
    // B.3.4a closing 2d; it was 0-CALL. The slot row itself stays registered.

    // Direct exe function calls used by Description serializers
    fn_ExternalPtr_Serialize = reinterpret_cast<fn_ExternalPtr_Serialize_t>(
        ResolveAddr(Module::Exe, 0x14000bd30));
    fn_ExternalPtr_Serialize2 = reinterpret_cast<fn_ExternalPtr_Serialize2_t>(
        ResolveAddr(Module::Exe, 0x14000bf40));
    fn_ExternalPtr_Serialize3 = reinterpret_cast<fn_ExternalPtr_Serialize3_t>(
        ResolveAddr(Module::Exe, 0x140007140));
    fn_ExternalRefArray_Serialize = reinterpret_cast<fn_ExternalRefArray_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400049c0));
    fn_DynArray_Resize = reinterpret_cast<fn_DynArray_Resize_t>(
        ResolveAddr(Module::Exe, 0x14000a590));
    fn_StringArray_Resize = reinterpret_cast<fn_StringArray_Resize_t>(
        ResolveAddr(Module::Exe, 0x140028860));
    fn_ExternalPtr_BuildSerialize = reinterpret_cast<fn_ExternalPtr_BuildSerialize_t>(
        ResolveAddr(Module::Exe, 0x140002340));
    fn_WeatherResize = reinterpret_cast<fn_WeatherResize_t>(
        ResolveAddr(Module::Exe, 0x1400198a0));
    fn_WeatherExternalSerialize = reinterpret_cast<fn_WeatherExternalSerialize_t>(
        ResolveAddr(Module::Exe, 0x1400196a0));

    // TutorialSetupDialog helpers (exe-internal)
    fn_SerializeExternalPtr = reinterpret_cast<fn_SerializeExternalPtr_t>(
        ResolveAddr(Module::Exe, 0x140028650));
    fn_TSD_Slot9Helper = reinterpret_cast<fn_TSD_Slot9Helper_t>(
        ResolveAddr(Module::Exe, 0x1400e29f0));
    fn_TSD_OpenDialog = reinterpret_cast<fn_TSD_OpenDialog_t>(
        ResolveAddr(Module::Exe, 0x1400e2700));
    fn_TSD_SetDialogFlags = reinterpret_cast<fn_TSD_SetDialogFlags_t>(
        ResolveAddr(Module::Exe, 0x1400e2330));
    fn_TSD_FindInstance = reinterpret_cast<fn_TSD_FindInstance_t>(
        ResolveAddr(Module::Exe, 0x1400e24f0));
    fn_SetFollowBuilding = reinterpret_cast<fn_SetFollowBuilding_t>(
        ResolveAddr(Module::Exe, 0x140101a80));
    fn_SetEditMode = reinterpret_cast<fn_SetEditMode_t>(
        ResolveAddr(Module::Exe, 0x140102b60));

    // FUN_14009f3e0 — terrain/scene pick from 2D map position (exe-only)
    fn_MapGetTerrainPos = reinterpret_cast<fn_MapGetTerrainPos_t>(
        ResolveAddr(Module::Exe, 0x14009f3e0));

    // --- Phase H: Sub-dependency blockers ---
    // Factory singleton — dispatch table at 0x140152760 (double-indirect)
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152760));
        if (dispatch) {
            g_pFactorySingleton = dispatch;
        }
    }
    // Object::Base::IsA(uint) — Runtime DLL export
    fn_IsA = reinterpret_cast<fn_IsA_t>(
        ResolveExport("?IsA@Base@Object@@QEBA_NI@Z"));
#ifdef _DEBUG
    if (g_pFactorySingleton && fn_IsA) {
        printf("[EngineFuncs] Phase H: IsA + Factory singleton resolved\n");
    }
    printf("[EngineFuncs] Phase H: FindDialogByName_Impl, Game_TriggerDisasterByType_Impl, "
           "Game_SpawnResources_Impl now available as inline replacements\n");
#endif

    // --- Phase I: Work class helpers ---
    fn_TreeRemoveByKey = reinterpret_cast<fn_TreeRemoveByKey_t>(
        ResolveAddr(Module::Exe, 0x140025520));
    fn_TreeRecursiveFree = reinterpret_cast<fn_TreeRecursiveFree_t>(
        ResolveAddr(Module::Exe, 0x140014c30));
    fn_StateMachine_ReleaseTasks = reinterpret_cast<fn_StateMachine_ReleaseTasks_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    fn_Map_CheckPassable = reinterpret_cast<fn_Map_CheckPassable_t>(
        ResolveAddr(Module::Exe, 0x1400a00f0));
    fn_Map_FindNearestPassable = reinterpret_cast<fn_Map_FindNearestPassable_t>(
        ResolveAddr(Module::Exe, 0x14009ccf0));
    fn_PerformWork_Action_Ctor = reinterpret_cast<fn_PerformWork_Action_Ctor_t>(
        ResolveAddr(Module::Exe, 0x1400bffd0));
    fn_PerformWork_Action_Ctor3 = reinterpret_cast<fn_PerformWork_Action_Ctor3_t>(
        ResolveAddr(Module::Exe, 0x1400c1eb0));
    fn_FindStorageForResource = reinterpret_cast<fn_FindStorageForResource_t>(
        ResolveAddr(Module::Exe, 0x14008d590));
    fn_FindComponent_RawMaterialC = reinterpret_cast<fn_FindComponent_RawMaterialC_t>(
        ResolveAddr(Module::Exe, 0x1400223c0));
    fn_FindComponent_NaturalResourceC = reinterpret_cast<fn_FindComponent_NaturalResourceC_t>(
        ResolveAddr(Module::Exe, 0x1400052c0));
    fn_FindComponent_WorkC = reinterpret_cast<fn_FindComponent_WorkC_t>(
        ResolveAddr(Module::Exe, 0x140003050));
    fn_TryAssignWorkplace = reinterpret_cast<fn_TryAssignWorkplace_t>(
        ResolveAddr(Module::Exe, 0x1400f8620));
    fn_FindDescription_RawMaterial = reinterpret_cast<fn_FindDescription_RawMaterial_t>(
        ResolveAddr(Module::Exe, 0x140006330));
    fn_GetRemainingCapacity = reinterpret_cast<fn_GetRemainingCapacity_t>(
        ResolveAddr(Module::Exe, 0x140116540));
    fn_CitizenAI_LongTermPlan = reinterpret_cast<fn_CitizenAI_LongTermPlan_t>(
        ResolveAddr(Module::Exe, 0x140115e60));
    fn_BuildRoad_QueryStorage = reinterpret_cast<fn_BuildRoad_QueryStorage_t>(
        ResolveAddr(Module::Exe, 0x14012ce70));
    // ClearStorage-specific helpers
    g_TypeId_UnknownComp_7ee8 = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7ee8));
    fn_Work_CanStartWork = reinterpret_cast<fn_Work_CanStartWork_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));
    fn_FindStorageDestination = reinterpret_cast<fn_FindStorageDestination_t>(
        ResolveAddr(Module::Exe, 0x14008d590));
    fn_Map_CanPathTo = reinterpret_cast<fn_Map_CanPathTo_t>(
        ResolveAddr(Module::Exe, 0x1400a00f0));
    fn_Work_ValidateAssignment = reinterpret_cast<fn_Work_ValidateAssignment_t>(
        ResolveAddr(Module::Exe, 0x1400f6ef0));
    fn_Pathfinding_FindPath = reinterpret_cast<fn_Pathfinding_FindPath_t>(
        ResolveAddr(Module::Exe, 0x1400bf790));
    fn_CitizenAI_RegisterWork = reinterpret_cast<fn_CitizenAI_RegisterWork_t>(
        ResolveAddr(Module::Exe, 0x140021a30));
    fn_BST_Remove = reinterpret_cast<fn_BST_Remove_t>(
        ResolveAddr(Module::Exe, 0x14001a6e0));
    fn_Storage_FindSlot = reinterpret_cast<fn_Storage_FindSlot_t>(
        ResolveAddr(Module::Exe, 0x14012d5d0));
    fn_Entity_GetPosition = reinterpret_cast<fn_Entity_GetPosition_t>(
        ResolveAddr(Module::Exe, 0x140122e20));
#ifdef _DEBUG
    printf("[EngineFuncs] Phase I: Work class helpers resolved\n");
#endif
    // --- BuildStructure support ---
    fn_BST_SerializeHelper = reinterpret_cast<fn_BST_SerializeHelper_t>(
        ResolveAddr(Module::Exe, 0x1400296b0));
    fn_Entity_FindComponent_Position = reinterpret_cast<fn_Entity_FindComponent_Position_t>(
        ResolveAddr(Module::Exe, 0x140002ed0));
    fn_Entity_FindComponent_Build = reinterpret_cast<fn_Entity_FindComponent_Build_t>(
        ResolveAddr(Module::Exe, 0x14000d6c0));
    fn_Entity_FindComponent_WorkPlace = reinterpret_cast<fn_Entity_FindComponent_WorkPlace_t>(
        ResolveAddr(Module::Exe, 0x140002e10));
    fn_Entity_FindComponent_MatDesc = reinterpret_cast<fn_Entity_FindComponent_MatDesc_t>(
        ResolveAddr(Module::Exe, 0x140003050));
    fn_PositionComponent_Count = reinterpret_cast<fn_PositionComponent_Count_t>(
        ResolveAddr(Module::Exe, 0x140123070));
    fn_CalcMaxBuilders = reinterpret_cast<fn_CalcMaxBuilders_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));
    fn_Entity_GetPositionForIndex = reinterpret_cast<fn_Entity_GetPositionForIndex_t>(
        ResolveAddr(Module::Exe, 0x140122e20));
    fn_Map_Invalidate = reinterpret_cast<fn_Map_Invalidate_t>(
        ResolveAddr(Module::Exe, 0x14009c480));
    fn_Map_CalcPathCost = reinterpret_cast<fn_Map_CalcPathCost_t>(
        ResolveAddr(Module::Exe, 0x14009f810));
    fn_Map_NeighborLookup = reinterpret_cast<fn_Map_NeighborLookup_t>(
        ResolveAddr(Module::Exe, 0x14009d0a0));
    fn_Map_CalcPathDist = reinterpret_cast<fn_Map_CalcPathDist_t>(
        ResolveAddr(Module::Exe, 0x1400a8ba0));
    fn_Map_CachePathDist = reinterpret_cast<fn_Map_CachePathDist_t>(
        ResolveAddr(Module::Exe, 0x1400a5b30));
    fn_SetupBuildActions = reinterpret_cast<fn_SetupBuildActions_t>(
        ResolveAddr(Module::Exe, 0x1400c0690));
    fn_BST_Insert = reinterpret_cast<fn_BST_Insert_t>(
        ResolveAddr(Module::Exe, 0x140021a30));
    fn_CitizenComponent_CheckWorkplaceValid = reinterpret_cast<fn_CitizenComponent_CheckWorkplaceValid_t>(
        ResolveAddr(Module::Exe, 0x1401158d0));
    fn_FindStorageLocationComp = reinterpret_cast<fn_FindStorageLocationComp_t>(
        ResolveAddr(Module::Exe, 0x140003050));
    fn_FindWorkplaceComp = reinterpret_cast<fn_FindWorkplaceComp_t>(
        ResolveAddr(Module::Exe, 0x140002e10));
#ifdef _DEBUG
    printf("[EngineFuncs] Phase I+J: All work class + BuildStructure helpers resolved\n");
#endif


    // InternalBasePtr::Serialize was resolved here until ENDGAME B.3.4a
    // closing 2d; it was 0-CALL.

    // FinishTutorialGoal helper: External serialize for tutorial ref
    // BuildingGoal helper
    fn_BuildingTypeArray_Dtor = reinterpret_cast<fn_BuildingTypeArray_Dtor_t>(
        ResolveAddr(Module::Exe, 0x14001b030));
    fn_ExternalPtr_SerializeTutorial = reinterpret_cast<fn_ExternalPtr_SerializeTutorial_t>(
        ResolveAddr(Module::Exe, 0x1400262b0));

#ifdef _DEBUG
    printf("[EngineFuncs] Achievement Goal support resolved\n");
#endif

    // --- Phase K: Tutorial Goal helpers ---
    fn_FindUIElementByPath = reinterpret_cast<fn_FindUIElementByPath_t>(
        ResolveAddr(Module::Exe, 0x1400e1870));

    // Object::Internal::Internal — dispatch slot 0x140152768
    if (!fn_ObjectInternal_Ctor) {
        void** slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152768));
        if (slot && *slot)
            fn_ObjectInternal_Ctor = reinterpret_cast<fn_ObjectInternal_Ctor_t>(*slot);
    }

    // FinishTutorialGoal_helper_01f0 — tutorial tree insert (exe-internal)
    fn_TutorialTreeInsert = reinterpret_cast<fn_TutorialTreeInsert_t>(
        ResolveAddr(Module::Exe, 0x1401001f0));

    // FinishTutorialInstance_helper_1db0 — tutorial step execute (exe-internal)
    fn_TutorialStepExecute = reinterpret_cast<fn_TutorialStepExecute_t>(
        ResolveAddr(Module::Exe, 0x1400e1db0));

    // TypeId for FinishTutorialGoal — stored at exe 0x1401c6240
    g_TypeId_FinishTutorialGoal = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c6240));

    // Singleton exref at 0x140151a80
    g_pSingletonExref_151a80 = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140151a80));

#ifdef _DEBUG
    printf("[EngineFuncs] Phase K: Tutorial goal helpers resolved\n");
    printf("  FindUIElementByPath   = %p\n", (void*)fn_FindUIElementByPath);
    printf("  ObjectInternal::Ctor  = %p\n", (void*)fn_ObjectInternal_Ctor);
    printf("  TutorialTreeInsert    = %p\n", (void*)fn_TutorialTreeInsert);
    printf("  TutorialStepExecute   = %p\n", (void*)fn_TutorialStepExecute);
    printf("  TypeId_FTGoal         = %p\n", (void*)g_TypeId_FinishTutorialGoal);
    printf("  SingletonExref_151a80 = %p\n", (void*)g_pSingletonExref_151a80);
#endif

    // ================================================================
    // Phase L: ComponentEntity dependencies
    // ================================================================
    g_TypeId_ComponentBase = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c72a8));
#ifdef _DEBUG
    printf("[EngineFuncs] Phase L: ComponentEntity deps resolved\n");
    printf("  TypeId_ComponentBase  = %p\n", (void*)g_TypeId_ComponentBase);
#endif

    // ================================================================
    // Phase L2: CitizenBehavior dependencies
    // ================================================================
    fn_ProfResArray_Init = reinterpret_cast<fn_ProfResArray_Init_t>(
        ResolveAddr(Module::Exe, 0x1400177c0));
    fn_StatsArray_Init = reinterpret_cast<fn_StatsArray_Init_t>(
        ResolveAddr(Module::Exe, 0x140017640));
    fn_SummaryArray_Init = reinterpret_cast<fn_SummaryArray_Init_t>(
        ResolveAddr(Module::Exe, 0x14000f090));
#ifdef _DEBUG
    printf("[EngineFuncs] Phase L2: CitizenBehavior deps resolved\n");
#endif

    // === EntityList dependencies (Agent A) ===
    fn_IBPTreeInsert = reinterpret_cast<fn_IBPTreeInsert_t>(
        ResolveAddr(Module::Exe, 0x140021a30));
    fn_IBPTreeRemove = reinterpret_cast<fn_IBPTreeRemove_t>(
        ResolveAddr(Module::Exe, 0x14001a6e0));
    // Standalone: the exe AVL tree (0x140021a30 / 0x14001a6e0) is unmapped, so the
    // above resolve to NULL — EntityList::PreUpdate would crash the first Update
    // tick once a spawned entity has an update-capable component. Fall back to our
    // plain-BST reimplementation (entitylist.cpp), which is functionally
    // equivalent for the in-order walks the engine performs.
    if (!fn_IBPTreeInsert)
        fn_IBPTreeInsert = reinterpret_cast<fn_IBPTreeInsert_t>(&IBPTreeInsert_CppImpl);
    if (!fn_IBPTreeRemove)
        fn_IBPTreeRemove = reinterpret_cast<fn_IBPTreeRemove_t>(&IBPTreeRemove_CppImpl);
    fn_BucketTreeSerialize = reinterpret_cast<fn_BucketTreeSerialize_t>(
        ResolveAddr(Module::Exe, 0x140021ba0));
#ifdef _DEBUG
    printf("[EngineFuncs] EntityList deps resolved:\n");
    printf("  IBPTreeInsert       = %p\n", (void*)fn_IBPTreeInsert);
    printf("  IBPTreeRemove       = %p\n", (void*)fn_IBPTreeRemove);
    printf("  BucketTreeSerialize = %p\n", (void*)fn_BucketTreeSerialize);
#endif

    // === CitizenBehavior Path dependencies (Agent C) ===
    fn_Entity_GetGridId = reinterpret_cast<fn_Entity_GetGridId_t>(
        ResolveAddr(Module::Exe, 0x140087fb0));
    fn_PathBST_Insert = reinterpret_cast<fn_PathBST_Insert_t>(
        ResolveAddr(Module::Exe, 0x140025a10));
    fn_PathPairs_Clear = reinterpret_cast<fn_PathPairs_Clear_t>(
        ResolveAddr(Module::Exe, 0x140025b30));
    fn_PathGraph_ReassignWorkers = reinterpret_cast<fn_PathGraph_ReassignWorkers_t>(
        ResolveAddr(Module::Exe, 0x1400d62a0));
    fn_PathGraph_AssignCitizens = reinterpret_cast<fn_PathGraph_AssignCitizens_t>(
        ResolveAddr(Module::Exe, 0x1400d43b0));
    fn_PathBST_FreeRecursive = reinterpret_cast<fn_PathBST_FreeRecursive_t>(
        ResolveAddr(Module::Exe, 0x14000e320));
#ifdef _DEBUG
    printf("[EngineFuncs] Path sub-functions resolved\n");
#endif

    // === CitizenBehavior remaining core dependencies (Agent D) ===
    fn_NotificationEvent = reinterpret_cast<fn_NotificationEvent_t>(
        ResolveAddr(Module::Exe, 0x140095580));
#ifdef _DEBUG
    printf("  NotificationEvent = %p\n", (void*)fn_NotificationEvent);
#endif

    // === CitizenBehavior FindWorking/FindIdle/FindStudent dependencies ===
    fn_PathDistance = reinterpret_cast<fn_PathDistance_t>(
        ResolveAddr(Module::Exe, 0x140008830));
    fn_CitizenComponent_AssignHousing = reinterpret_cast<fn_CitizenComponent_AssignHousing_t>(
        ResolveAddr(Module::Exe, 0x140111ee0));
#ifdef _DEBUG
    printf("[EngineFuncs] CitizenBehavior Find deps resolved\n");
    printf("  PathDistance       = %p\n", (void*)fn_PathDistance);
    printf("  AssignHousing      = %p\n", (void*)fn_CitizenComponent_AssignHousing);
#endif

    // Runtime ctors used by the standalone Reimpl_Geometry_Init body.
    fn_Video_VertexBuffer_Ctor = reinterpret_cast<fn_Video_VertexBuffer_Ctor_t>(
        ResolveExport("??0VertexBuffer@Video@@QEAA@XZ"));
    fn_Video_IndexBuffer_Ctor = reinterpret_cast<fn_Video_IndexBuffer_Ctor_t>(
        ResolveExport("??0IndexBuffer@Video@@QEAA@XZ"));
    // Serializer::Serialize(float&) was resolved here until ENDGAME B.3.4a
    // closing 2d; it was 0-CALL.

    fn_Array_Resize_Stride20 = reinterpret_cast<fn_Array_Resize_t>(
        ResolveAddr(Module::Exe, 0x14001baf0));
    fn_Array_Resize_Stride08 = reinterpret_cast<fn_Array_Resize_t>(
        ResolveAddr(Module::Exe, 0x140001bb0));
    fn_Array_Resize_Stride14 = reinterpret_cast<fn_Array_Resize_t>(
        ResolveAddr(Module::Exe, 0x14001bf10));
    fn_Array_Serialize_Stride20 = reinterpret_cast<fn_Array_Serialize_Stride20_t>(
        ResolveAddr(Module::Exe, 0x14001bbf0));

    fn_Array_Serialize_External_GraphicsMesh =
        reinterpret_cast<fn_Array_Serialize_External_t>(
            ResolveAddr(Module::Exe, 0x14001bcc0));
    fn_Array_Serialize_External_Particle =
        reinterpret_cast<fn_Array_Serialize_External_t>(
            ResolveAddr(Module::Exe, 0x14001bff0));
    fn_ExternalPtr_Serialize_GraphicsMesh =
        reinterpret_cast<fn_ExternalPtr_Serialize_t>(
            ResolveAddr(Module::Exe, 0x14001b8e0));
    fn_ExternalPtr_Serialize_Weather =
        reinterpret_cast<fn_ExternalPtr_Serialize_t>(
            ResolveAddr(Module::Exe, 0x1400196a0));

    // ============================================================
    // M3.5 Path B session 2 — 0x60B struct array lifecycle helpers
    // ============================================================
    fn_Struct60B_CopyCtor = reinterpret_cast<fn_Struct60B_CopyCtor_t>(
        ResolveAddr(Module::Exe, 0x14001f5e0));
    fn_Struct60B_DtorRange = reinterpret_cast<fn_Struct60B_DtorRange_t>(
        ResolveAddr(Module::Exe, 0x14001fb80));
    fn_Struct60B_SubArrayCopyStride14 = reinterpret_cast<fn_Struct60B_SubArrayCopy_t>(
        ResolveAddr(Module::Exe, 0x14001fdb0));
    fn_Struct60B_SubArrayCopyStride02 = reinterpret_cast<fn_Struct60B_SubArrayCopy_t>(
        ResolveAddr(Module::Exe, 0x14001fe60));
#ifdef _DEBUG
    printf("[EngineFuncs] M3.5 Path B session 2 (0x60B struct lifecycle) resolved\n");
    printf("  Struct60B_CopyCtor      = %p\n", (void*)fn_Struct60B_CopyCtor);
    printf("  Struct60B_DtorRange     = %p\n", (void*)fn_Struct60B_DtorRange);
    printf("  Struct60B_SubCopyS14    = %p\n", (void*)fn_Struct60B_SubArrayCopyStride14);
    printf("  Struct60B_SubCopyS02    = %p\n", (void*)fn_Struct60B_SubArrayCopyStride02);
#endif

    // ============================================================
    // M3.5 Path B session 5 — additional Geometry::Serialize resize helpers
    // ============================================================
    fn_Array_Resize_Stride02 = reinterpret_cast<fn_Array_Resize_t>(
        ResolveAddr(Module::Exe, 0x14001c890));
    fn_Array_Resize_Stride16B = reinterpret_cast<fn_Array_Resize_t>(
        ResolveAddr(Module::Exe, 0x14001ef10));
#ifdef _DEBUG
    printf("[EngineFuncs] M3.5 Path B session 5 (geometry resize helpers) resolved\n");
    printf("  Array_Resize_Stride02       = %p\n", (void*)fn_Array_Resize_Stride02);
    printf("  Array_Resize_Stride16B      = %p\n", (void*)fn_Array_Resize_Stride16B);
#endif

    // Runtime DLL primitives — this block resolved a handful of Serializer and
    // ExternalFactory doors via dispatch slot, falling back to the mangled
    // export. It is EMPTY now and the shared `void** slot` scratch local went
    // with its last reader:
    //   0x1401529f8 / 0x140152a50 — Serializer::GetEncoding and
    //     Serializer::Serialize(uint&), both 0-CALL, ENDGAME B.3.4a closing 2d.
    //   0x140151218 / 0x140151220 / 0x140151228 —
    //     ExternalFactory::IsReferenceModeEnabled / GetReferenceId /
    //     GetPathForReferenceId, ENDGAME B.3.3 step 7: their last callers are
    //     member calls on ::Object::ExternalFactory.
    // All five dispatch slots stay registered in address_registry.cpp.

    // BaseTypeInfo* slots / typeId storage. These get filled by Runtime DLL
    // type-registration plumbing in the original; in standalone they may be
    // null until the underlying type registers, in which case the helpers
    // below detect null and emit a one-shot warning instead of crashing.
    g_ppTypeInfo_GraphicsMesh = reinterpret_cast<uint32_t**>(
        ResolveAddr(Module::Exe, 0x140151f20));
    g_ppTypeInfo_Particle = reinterpret_cast<uint32_t**>(
        ResolveAddr(Module::Exe, 0x140151420));
    g_pTypeId_Weather = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c6198));

    // M3.5 Path B session 3 — FUN_140007140 + MaterialInstance typeInfo slot.
    g_ppTypeInfo_MaterialInstance = reinterpret_cast<uint32_t**>(
        ResolveAddr(Module::Exe, 0x140151350));

#ifdef _DEBUG
    printf("[EngineFuncs] M3.5 MenuScene::Serialize helpers resolved\n");
    printf("  Video_VertexBuffer_Ctor = %p\n", (void*)fn_Video_VertexBuffer_Ctor);
    printf("  Video_IndexBuffer_Ctor  = %p\n", (void*)fn_Video_IndexBuffer_Ctor);
    printf("  Array_Resize_S20/S08/S14= %p / %p / %p\n",
        (void*)fn_Array_Resize_Stride20,
        (void*)fn_Array_Resize_Stride08,
        (void*)fn_Array_Resize_Stride14);
    printf("  Array_Serialize_S20     = %p\n", (void*)fn_Array_Serialize_Stride20);
    printf("  ExtArr GraphicsMesh/Part= %p / %p\n",
        (void*)fn_Array_Serialize_External_GraphicsMesh,
        (void*)fn_Array_Serialize_External_Particle);
    printf("  ExtPtr Mesh/Weather     = %p / %p\n",
        (void*)fn_ExternalPtr_Serialize_GraphicsMesh,
        (void*)fn_ExternalPtr_Serialize_Weather);
    printf("  TypeInfo Mesh/Particle  = %p / %p\n",
        (void*)g_ppTypeInfo_GraphicsMesh, (void*)g_ppTypeInfo_Particle);
    printf("  TypeId Weather          = %p\n", (void*)g_pTypeId_Weather);
    // M3.5 Path B session 3
    printf("  TypeInfo MaterialInst   = %p\n",
        (void*)g_ppTypeInfo_MaterialInstance);
#endif

    // ============================================================
    // M3.5 Path B session 4 — per-element 0x60B + typed-ExternalPtr-array.
    // ============================================================
#ifdef _DEBUG
    printf("[EngineFuncs] M3.5 Path B session 4 (0x60B serialize) resolved\n");
#endif

    // ============================================================
    // M3.5 Path B session 8 — Geometry::Serialize body + extra.
    // ============================================================

    // M3.5 Path B session 8 — the promoted IO::Serializer dispatch-slot block
    // that used to stand here is gone, and the shared `void** sp` scratch local
    // with it. 0x140152ac8 / 0x140152a60 retired in ENDGAME B.3.4 step 0 (they
    // aliased g_fn_Serialize_Int / fn_Serializer_Serialize_Float);
    // 0x140152488 / 0x140152618 retired in closing 2a; and 0x140152410 /
    // 0x1401523f8 / 0x140152400 retire here in closing 2d with the guards that
    // were their last readers. Every one of those SLOTS stays bound in
    // standalone/address_registry.cpp — they are PE imports read by unported
    // exe code, not our call sites.

    // ============================================================
    // M3.5 — Geometry vtable draw primitives (slot 0x14015f110)
    // ============================================================
    // The Renderer / Camera / Frustum / Math / Context primitives backing the
    // 9-slot derived Geometry vtable. All resolved via ResolveExport from the
    // Runtime DLL exports. These provide the actual draw path, used by
    // code/Game/Game/geometry_draw.cpp.
    fn_Frustum_Contains = reinterpret_cast<fn_Frustum_Contains_t>(
        ResolveExport("?Contains@Frustum@Volume@@QEBAIAEBVBox@2@I@Z"));
    fn_Frustum_MulTransform3 = reinterpret_cast<fn_Frustum_MulTransform3_t>(
        ResolveExport("??DFrustum@Volume@@QEBA?AV01@AEBVTransform3@Math@@@Z"));
#ifdef _DEBUG
    printf("[EngineFuncs] M3.5 Geometry vtable draw primitives resolved\n");
    printf("  Frustum_Contains         = %p\n", (void*)fn_Frustum_Contains);
    printf("  Frustum_MulTransform3    = %p\n", (void*)fn_Frustum_MulTransform3);
#endif

    return true;
}
