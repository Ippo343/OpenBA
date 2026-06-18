// Resolved engine function pointers.
// Initializes function pointers and global variable addresses from Runtime-x64.dll.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>

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

// Input::Device::Update @ 0x1800e1490
fn_InputDevice_Update_t fn_InputDevice_Update = nullptr;

// Audio::Device::Update — need to find address (TBD, resolve by name for now)
fn_AudioDevice_Update_t fn_AudioDevice_Update = nullptr;

// Audio::Device::FreeChannelPlatform — TBD
fn_AudioDevice_FreeChannel_t fn_AudioDevice_FreeChannel = nullptr;

// Application::Application::PlatformUpdate — TBD
fn_App_PlatformUpdate_t fn_App_PlatformUpdate = nullptr;

// Video::Context::ClearResources — TBD
fn_VideoCtx_ClearResources_t fn_VideoCtx_ClearResources = nullptr;

// Video::Context::Reset — TBD
fn_VideoCtx_Reset_t fn_VideoCtx_Reset = nullptr;

// Engine::Loader::Load
fn_Loader_Load_t fn_Loader_Load = nullptr;

// Application::Application::SleepThread
fn_App_SleepThread_t fn_App_SleepThread = nullptr;

// UI::UserInterface::Update
fn_UI_Update_t fn_UI_Update = nullptr;

// UI::UserInterface::Draw
fn_UI_Draw_t fn_UI_Draw = nullptr;

// UI::UserInterface::CloseDialog
fn_UI_CloseDialog_t fn_UI_CloseDialog = nullptr;

// Dynamic array push helper
fn_DynArrayPush_t fn_DynArrayPush = nullptr;

// PostDraw/PostUpdate callback array push helpers
fn_DynArrayPush_t fn_PostDrawPush = nullptr;
fn_DynArrayPush_t fn_PostUpdatePush = nullptr;

// Video::Context lifecycle
fn_VideoCtx_Ctor_t fn_VideoCtx_Ctor = nullptr;
fn_VideoCtx_Dtor_t fn_VideoCtx_Dtor = nullptr;
fn_VideoCtx_Delete_t fn_VideoCtx_Delete = nullptr;

// MemoryAllocator
fn_MemAlloc_t fn_MemAlloc = nullptr;
fn_MemFree_t fn_MemFree = nullptr;
void* g_pMemAllocSingleton = nullptr;

// Video::Device singleton
void** g_pVideoDeviceSingleton = nullptr;

// Video::Device Lock/Unlock
fn_VideoDevice_Lock_t fn_VideoDevice_Lock = nullptr;
fn_VideoDevice_Unlock_t fn_VideoDevice_Unlock = nullptr;

// String functions
fn_String_Set_t fn_String_Set = nullptr;
fn_String_Format_t fn_String_Format = nullptr;
fn_String_Dtor_t fn_String_Dtor = nullptr;
fn_String_LessThan_t fn_String_LessThan = nullptr;
fn_String_Ctor_Default_t fn_String_Ctor_Default = nullptr;
fn_String_Ctor_CStr_t fn_String_Ctor_CStr = nullptr;
fn_String_Ctor_Copy_t fn_String_Ctor_Copy = nullptr;

// ExternalFactory
void** g_pExternalFactorySingleton = nullptr;
fn_ExternalFactory_FindOrLoad_t fn_ExternalFactory_FindOrLoad = nullptr;
fn_ExternalFactory_Find_t fn_ExternalFactory_Find = nullptr;
fn_ExternalFactory_Clean_t fn_ExternalFactory_Clean = nullptr;
fn_IsDerivedFrom_t fn_IsDerivedFrom = nullptr;
uint32_t* g_pExternalTypeId = nullptr;
fn_Filename_AdjustDelimiters_t fn_Filename_AdjustDelimiters = nullptr;
fn_UpdateLoop_LoadResources_t fn_UpdateLoop_LoadResources = nullptr;
fn_ListCleanup_t fn_ListCleanup = nullptr;

// Mod entry lifecycle
fn_ModEntry_CopyCtor_t fn_ModEntry_CopyCtor = nullptr;
fn_ModEntry_Assign_t fn_ModEntry_Assign = nullptr;
fn_ModEntry_Dtor_t fn_ModEntry_Dtor = nullptr;

// Map::Find (AVL tree lookup)
fn_Map_Find_t fn_Map_Find = nullptr;

// Map::Insert (AVL tree insert)
fn_Map_Insert_t fn_Map_Insert = nullptr;

// Configuration parsing helpers
fn_ParseCommand_t fn_ParseCommand = nullptr;
fn_ParseValue_t fn_ParseValue = nullptr;

// Persistence
void** g_pPersistenceSingleton = nullptr;
fn_Persistence_SetBool_t fn_Persistence_SetBool = nullptr;
fn_Persistence_SetInt_t fn_Persistence_SetInt = nullptr;

// Filename helpers
fn_Filename_GetPathFilename_t fn_Filename_GetPathFilename = nullptr;

// Array<Mod> copy
fn_ModArrayCopy_t fn_ModArrayCopy = nullptr;

// UI infrastructure (existing)
fn_DialogController_Dtor_t fn_DialogController_Dtor = nullptr;
fn_DialogInstance_Dtor_t fn_DialogInstance_Dtor = nullptr;
fn_MemFreeStatic_t fn_MemFreeStatic = nullptr;
fn_FindElement_t fn_FindElement = nullptr;
fn_TextElement_SetParameter_t fn_TextElement_SetParameter = nullptr;
fn_Element_GetUserData_t fn_Element_GetUserData = nullptr;
fn_InternalBasePtr_Ctor_t fn_InternalBasePtr_Ctor = nullptr;
fn_KeyMap_GetInputForId_t fn_KeyMap_GetInputForId = nullptr;
fn_KeyMap_GetDeviceForId_t fn_KeyMap_GetDeviceForId = nullptr;

// ============================================================
// UI Widget Functions (Phase A1)
// ============================================================

// Element
fn_Element_SetFlagsOnOff_t fn_Element_SetFlagsOnOff = nullptr;
fn_Element_SetElementId_t fn_Element_SetElementId = nullptr;
fn_Element_SetName_t fn_Element_SetName = nullptr;
fn_Element_SetToolTip_t fn_Element_SetToolTip = nullptr;
fn_Element_SetToolTipString_t fn_Element_SetToolTipString = nullptr;
fn_Element_Child_t fn_Element_Child = nullptr;
fn_Element_ChildCount_t fn_Element_ChildCount = nullptr;
fn_Element_FindElementType_t fn_Element_FindElementType = nullptr;
fn_Element_RemoveChildren_t fn_Element_RemoveChildren = nullptr;
fn_Element_AddChild_t fn_Element_AddChild = nullptr;

// DialogInstance
fn_DialogInstance_FindElement_t fn_DialogInstance_FindElement = nullptr;
fn_DialogInstance_GetController_t fn_DialogInstance_GetController = nullptr;
fn_DialogInstance_FindElementType_t fn_DialogInstance_FindElementType = nullptr;
fn_DialogInstance_SetFlagsOnOff_t fn_DialogInstance_SetFlagsOnOff = nullptr;

// ComboElement
fn_ComboElement_AddItem_t fn_ComboElement_AddItem = nullptr;
fn_ComboElement_SetCurrentItem_t fn_ComboElement_SetCurrentItem = nullptr;
fn_ComboElement_GetItemCount_t fn_ComboElement_GetItemCount = nullptr;
fn_ComboElement_GetCurrentItem_t fn_ComboElement_GetCurrentItem = nullptr;
fn_ComboElement_GetItemData_t fn_ComboElement_GetItemData = nullptr;
fn_ComboElement_RemoveAllItems_t fn_ComboElement_RemoveAllItems = nullptr;

// ProgressElement
fn_ProgressElement_SetProgressTextParameter_t fn_ProgressElement_SetProgressTextParameter = nullptr;
fn_ProgressElement_SetMaximumValue_t fn_ProgressElement_SetMaximumValue = nullptr;
fn_ProgressElement_SetCurrentValue_t fn_ProgressElement_SetCurrentValue = nullptr;

// TableElement
fn_TableElement_SetSize_t fn_TableElement_SetSize = nullptr;
fn_TableElement_SetChild3_t fn_TableElement_SetChild3 = nullptr;
fn_TableElement_SetChild2_t fn_TableElement_SetChild2 = nullptr;
fn_TableElement_GetRowCount_t fn_TableElement_GetRowCount = nullptr;
fn_TableElement_GetColumnCount_t fn_TableElement_GetColumnCount = nullptr;

// NumberEditElement
fn_NumberEditElement_SetValue_t fn_NumberEditElement_SetValue = nullptr;
fn_NumberEditElement_SetMinMax_t fn_NumberEditElement_SetMinMax = nullptr;
fn_NumberEditElement_GetValue_t fn_NumberEditElement_GetValue = nullptr;

// NumberElement
fn_NumberElement_SetValueInt_t fn_NumberElement_SetValueInt = nullptr;
fn_NumberElement_SetValueFloat_t fn_NumberElement_SetValueFloat = nullptr;

// SpinnerElement
fn_SpinnerElement_SetElementId_t fn_SpinnerElement_SetElementId = nullptr;
fn_SpinnerElement_SetValue_t fn_SpinnerElement_SetValue = nullptr;
fn_SpinnerElement_SetMinMax_t fn_SpinnerElement_SetMinMax = nullptr;
fn_SpinnerElement_GetValue_t fn_SpinnerElement_GetValue = nullptr;
fn_SpinnerElement_GetMaxValue_t fn_SpinnerElement_GetMaxValue = nullptr;
fn_SpinnerElement_SetIncrement_t fn_SpinnerElement_SetIncrement = nullptr;
fn_SpinnerElement_IsActive_t fn_SpinnerElement_IsActive = nullptr;

// PageElement
fn_PageElement_GetActivePage_t fn_PageElement_GetActivePage = nullptr;
fn_PageElement_SetActivePage_t fn_PageElement_SetActivePage = nullptr;

// CheckboxElement type ID (exref)
uint32_t* g_TypeId_CheckboxElement = nullptr;

// UI element base type ID (exref)
uint32_t* g_TypeId_UIElementBase = nullptr;

// Object::Internal type ID (exref 0x140151660)
uint32_t* g_TypeId_ObjectInternal = nullptr;

// UI::ImageElement type ID (exref 0x140151500)
uint32_t* g_TypeId_ImageElement = nullptr;

// ControlComponent type ID (direct)
uint32_t* g_TypeId_ControlComponent = nullptr;

// FindComponentByTypeId — now inline in engine_funcs.h (reimplemented from FUN_1400c3680)

// EntityTrackingSet_BST_Delete (AVL node deleter, sub-function of _Remove)
fn_EntityTrackingSet_BST_Delete_t fn_EntityTrackingSet_BST_Delete = nullptr;

// ImageElement
fn_ImageElement_SetSprite_t fn_ImageElement_SetSprite = nullptr;
fn_ImageElement_SetWidthHeight_t fn_ImageElement_SetWidthHeight = nullptr;

// LabelElement
fn_LabelElement_SetSprite_t fn_LabelElement_SetSprite = nullptr;

// TextElement
fn_TextElement_SetText_t fn_TextElement_SetText = nullptr;
fn_Dialog_CreateElement_t fn_Dialog_CreateElement = nullptr;
fn_GetInputKeyName_t fn_GetInputKeyName = nullptr;

// ============================================================
// Exe Type IDs (Phase A2)
// ============================================================
uint32_t* g_TypeId_ComponentEntity = nullptr;
uint32_t* g_TypeId_ProgressElement = nullptr;
uint32_t* g_TypeId_BuildComponent = nullptr;
uint32_t* g_TypeId_ConsumeProduceComponent = nullptr;
uint32_t* g_TypeId_RadiusComponent = nullptr;
uint32_t* g_TypeId_BuildRequirementComponent = nullptr;
uint32_t* g_TypeId_ResourceComponent = nullptr;
uint32_t* g_TypeId_RawMaterialDescription = nullptr;
uint32_t* g_TypeId_DialogController = nullptr;
uint32_t* g_TypeId_PageToggleUI = nullptr;
uint32_t* g_TypeId_MeshInstanceComponent = nullptr;
uint32_t* g_TypeId_DestroyComponent = nullptr;
uint32_t* g_TypeId_WorkplaceComponent = nullptr;
uint32_t* g_TypeId_StorageLocationComponent = nullptr;
uint32_t* g_TypeId_StorageComponent = nullptr;
uint32_t* g_TypeId_StateMachineComponent = nullptr;
uint32_t* g_TypeId_WorkplaceJobType = nullptr;
uint32_t* g_TypeId_AttendanceSubComp = nullptr;
uint32_t* g_TypeId_ResourceLimitType = nullptr;
uint32_t* g_TypeId_ToolUI = nullptr;
uint32_t* g_TypeId_ProfessionDescription = nullptr;

// BridgeTool-related type IDs
uint32_t* g_TypeId_CreateBridgeDescription = nullptr;
// RemovalTool-related type IDs
uint32_t* g_TypeId_RemovalDescription = nullptr;
// g_TypeId_MapComponent = g_TypeId_BuildComponent_Slot4 (same DAT_1401c79e0)
uint32_t* g_TypeId_ModelComponent = nullptr;
// g_TypeId_MultiObjectComponent already defined below (~line 444)
uint32_t* g_TypeId_PickingComponent = nullptr;

// BridgeTool helper functions
fn_BridgeTool_SnapEndCell_t fn_BridgeTool_SnapEndCell = nullptr;
fn_BridgeTool_ValidatePath_t fn_BridgeTool_ValidatePath = nullptr;

// DragPlacementTool/FencedTool type IDs
uint32_t* g_TypeId_CreatePlacementDescription = nullptr;
uint32_t* g_TypeId_CreateFencedDescription = nullptr;
uint32_t* g_TypeId_AreaDescription = nullptr;
uint32_t* g_TypeId_FenceDescription = nullptr;
uint32_t* g_TypeId_SizeDescription = nullptr;

// DragPlacementTool/FencedTool function pointers
fn_DragTool_OnKey_t fn_DragTool_OnKey = nullptr;
fn_DragTool_Shutdown_t fn_DragTool_Shutdown = nullptr;

// ============================================================
// Game Singletons (Phase A3)
// ============================================================
void** g_pGameWorldSingleton = nullptr;
void** g_pKeyMapSingleton = nullptr;
void** g_pKeyMapObjectExref = nullptr;
void** g_pEngineSingleton = nullptr;

// ============================================================
// Common Exe Functions (Phase A4) — type ID globals for reimplemented FindComponent
// ============================================================
// FindComponent functions are now inline C++ (see engine_funcs.h).
// These globals are resolved directly from exe .rdata addresses.
uint32_t* g_TypeId_BuildComponent_Slot4     = nullptr;  // DAT_1401c79e0
uint32_t* g_TypeId_BuildStateComponent      = nullptr;  // DAT_1401c7d98
uint32_t* g_TypeId_StateMachineComponent_FC = nullptr;  // DAT_1401c7350
uint32_t* g_TypeId_PositionComponent        = nullptr;  // DAT_1401c7008
uint32_t* g_TypeId_DestroyComponent_FC      = nullptr;  // DAT_1401c75f0
uint32_t* g_TypeId_BuildingComponent        = nullptr;  // DAT_1401c4b80
uint32_t* g_TypeId_AttendanceComponent      = nullptr;  // DAT_1401c3f40
uint32_t* g_TypeId_PastureComponent         = nullptr;  // DAT_1401c7ba0
uint32_t* g_TypeId_AnimalDescription        = nullptr;  // DAT_1401c6fd0
uint32_t* g_TypeId_OrchardComponent         = nullptr;  // DAT_1401c46b0
uint32_t* g_TypeId_SMComponent              = nullptr;  // DAT_1401c71c8
uint32_t* g_TypeId_CitizenAIComponent_FC    = nullptr;  // DAT_1401c7158
uint32_t* g_TypeId_StorageComponent_FC      = nullptr;  // DAT_1401c7e78
uint32_t* g_TypeId_ResidenceComponent       = nullptr;  // DAT_1401c7cf0
uint32_t* g_TypeId_CemeteryComponent        = nullptr;  // DAT_1401c4090
uint32_t* g_TypeId_TombstoneComponent       = nullptr;  // DAT_1401c4a30
uint32_t* g_TypeId_ImmigrationComponent     = nullptr;  // DAT_1401c4480
uint32_t* g_TypeId_TrackerComponent         = nullptr;  // DAT_1401c4b10
uint32_t* g_TypeId_CharacterComponent       = nullptr;  // DAT_1401c7a50

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
fn_ImageBuffer_Ctor_t fn_ImageBuffer_Ctor = nullptr;
fn_ImageBuffer_Dtor_t fn_ImageBuffer_Dtor = nullptr;
fn_ImageBuffer_CopyToTexture_t fn_ImageBuffer_CopyToTexture = nullptr;
fn_ImageBuffer_CopyFromTexture_t fn_ImageBuffer_CopyFromTexture = nullptr;

// ============================================================
// Phase F: Dialog/UI Runtime functions
// ============================================================
fn_ImageElement_SetRotation_t fn_ImageElement_SetRotation = nullptr;
fn_Time_AsSeconds_t fn_Time_AsSeconds = nullptr;
fn_SaveLoad_Dtor_t fn_SaveLoad_Dtor = nullptr;
fn_SaveLoad_GetOperation_t fn_SaveLoad_GetOperation = nullptr;
void** g_pSaveLoadSingleton = nullptr;
fn_SaveLoad_SetSaveSprite_t fn_SaveLoad_SetSaveSprite = nullptr;
fn_SaveLoad_SetMinOperationTime_t fn_SaveLoad_SetMinOperationTime = nullptr;
fn_SaveLoad_SetOperation_t fn_SaveLoad_SetOperation = nullptr;
fn_SaveLoad_GetSaveCount_t fn_SaveLoad_GetSaveCount = nullptr;
fn_SaveLoad_GetSaveInfo_t fn_SaveLoad_GetSaveInfo = nullptr;
fn_SaveLoad_GetSaveState_t fn_SaveLoad_GetSaveState = nullptr;
fn_Sprite_IsValid_t fn_Sprite_IsValid = nullptr;
fn_UI_Play_t fn_UI_Play = nullptr;
fn_DateTime_AsString_t fn_DateTime_AsString = nullptr;
// fn_SaveLoadDialog_PopulateFileList — REMOVED, now reimplemented in saveloaddialog.cpp
fn_EditElement_SetText_t fn_EditElement_SetText = nullptr;
fn_EditElement_SetText_SysStr_t fn_EditElement_SetText_SysStr = nullptr;
fn_Input_Device_Released_t fn_Input_Device_Released = nullptr;
void** g_pInputDeviceSingleton = nullptr;
fn_ImageElement_GetSprite_t fn_ImageElement_GetSprite = nullptr;
fn_LabelElement_GetSprite_t fn_LabelElement_GetSprite = nullptr;
fn_SpriteSheet_GetSpriteIndexFromName_t fn_SpriteSheet_GetSpriteIndexFromName = nullptr;
fn_SliderElement_SetParameter_t fn_SliderElement_SetParameter = nullptr;
fn_Audio_Scene_Pause_t fn_Audio_Scene_Pause = nullptr;
fn_Audio_Scene_Update_t fn_Audio_Scene_Update = nullptr;
// fn_ScenarioDialog_LoadSelected — REMOVED, now reimplemented in scenariodialog.cpp
fn_SaveLoad_Update_t fn_SaveLoad_Update = nullptr;
fn_Persistence_GetSavePath_t fn_Persistence_GetSavePath = nullptr;
fn_FileSystem_GetDataDirectory_t fn_FileSystem_GetDataDirectory = nullptr;
void** g_pFileSystemSingleton2 = nullptr;
fn_LoadScenarioFromSave_t fn_LoadScenarioFromSave = nullptr;
uintptr_t g_ModUpdateDialog_Vtable = 0;  // PTR_FUN_14015c6c0
fn_SelectResourceUI_GetResourceCount_t fn_SelectResourceUI_GetResourceCount = nullptr;
fn_SelectResourceUI_PopulateCombo_t fn_SelectResourceUI_PopulateCombo = nullptr;
fn_GoalList_GetGoal_t fn_GoalList_GetGoal = nullptr;
fn_GoalInstance_IsMet_t fn_GoalInstance_IsMet = nullptr;
fn_GoalInstance_HasFailed_t fn_GoalInstance_HasFailed = nullptr;
void*** g_ppAchievementManager = nullptr;
fn_AchievementManager_IsAchievementAwarded_t fn_AchievementManager_IsAchievementAwarded = nullptr;
// FollowDialog helpers
fn_KeyMap_Pressed_t fn_KeyMap_Pressed = nullptr;
// fn_FollowDialog_Close — inlined in followdialog.cpp Slot7

// UpgradeUI helpers
uint32_t* g_TypeId_UpgradeDescription = nullptr;
// fn_DestroyComponent_StartUpgrade — REMOVED, now reimplemented in destroyui.cpp

// fn_ResidenceUI_GoToCitizen — now inline in residenceui.cpp ResidenceUI_GoToCitizen
// fn_ResidenceUI_ArrayPush — now inline as ResidenceUI_ArrayPush_Impl in residenceui.cpp
// fn_EventLogUI_ToggleNotification — now inline in eventlogui.cpp Slot9
// fn_EventLogUI_NavigateToEvent — now inline in eventlogui.cpp Slot9

// WorkplaceUI helpers
fn_WorkplaceComp_SetMaxWorkers_t fn_WorkplaceComp_SetMaxWorkers = nullptr;
fn_CitizenList_RequestWorker_t fn_CitizenList_RequestWorker = nullptr;
fn_CitizenList_AssignWorker_t fn_CitizenList_AssignWorker = nullptr;
fn_CitizenList_RemoveWorker_t fn_CitizenList_RemoveWorker = nullptr;
uint32_t* g_TypeId_EditTool = nullptr;

// EntityUI helpers — now reimplemented inline in entityui.cpp
// fn_EntityUI_SetEditMode — inlined
// fn_EntityUI_SelectEntity — inlined
fn_Factory_CreateObject_t fn_Factory_CreateObject = nullptr;
uint32_t* g_TypeId_ToolController = nullptr;
uint32_t* g_TypeId_ToolSetting = nullptr;
fn_AVL_Rebalance_0x38_t fn_AVL_Rebalance_0x38 = nullptr;
// ============================================================
// Phase F Wave 1: MapParamsDialog / UpgradeUI / ConsumeProduceUI / HelpUI
// ============================================================
// fn_MapParamsDialog_OnOk — inlined in mapparamsdialog.cpp
// fn_MapParamsDialog_OnNewName — inlined in mapparamsdialog.cpp
// fn_GameWorld_GetMapName — inlined in mapparamsdialog.cpp
fn_Element_SetUserData_t fn_Element_SetUserData = nullptr;
// fn_UpgradeUI_Serialize — inlined as UpgradeUI_Serialize in upgradeui.cpp
// fn_UpgradeUI_OnRemove — inlined as UpgradeUI_OnRemove in upgradeui.cpp
fn_ConsumeProduceUI_Slot4_t fn_ConsumeProduceUI_Slot4 = nullptr;
// fn_HelpUI_Slot4 — now inline as HelpUI_Slot4 in helpui.cpp
// fn_HelpUI_Slot9 — now inline as HelpUI_Slot9 in helpui.cpp

// ============================================================
// Phase F Wave 2A: GoalDialog / EventNotifyDialog / ScenarioDialog
// ============================================================
fn_GoalList_GetGoalCount_t fn_GoalList_GetGoalCount = nullptr;
fn_Dialog_GetTemplate_t fn_Dialog_GetTemplate = nullptr;
fn_Goal_GetText_t fn_Goal_GetText = nullptr;
fn_Achievement_GetTitle_t fn_Achievement_GetTitle = nullptr;
fn_Achievement_GetDescription_t fn_Achievement_GetDescription = nullptr;
fn_Achievement_GetSpriteName_t fn_Achievement_GetSpriteName = nullptr;
fn_AchievementManager_GetAchievementCount_t fn_AchievementManager_GetAchievementCount = nullptr;
fn_AchievementManager_GetAchievement_t fn_AchievementManager_GetAchievement = nullptr;
// fn_GoalDialog_ArrayPush — now inline as GoalDialog_ArrayPush_Impl in goaldialog.cpp
// fn_EventNotifyDialog_ArrayPush — now inline as EventNotifyDialog_ArrayPush_Impl in eventnotifydialog.cpp
// fn_ScenarioDialog_ArrayPush — now inline as ScenarioDialog_ArrayPush_Impl in scenariodialog.cpp
fn_ExternalList_GetResourceCount_t fn_ExternalList_GetResourceCount = nullptr;
uint32_t* g_TypeId_Scenario = nullptr;
fn_Element_SetPadding_t fn_Element_SetPadding = nullptr;
// fn_EventNotifyDialog_ArrayRemove — inlined as EventNotifyDialog_ArrayRemove_Impl in eventnotifydialog.cpp
fn_Sprite_Ctor_t fn_Sprite_Ctor = nullptr;
fn_UIString_Ctor_t fn_UIString_Ctor = nullptr;
fn_UIString_ToSystemString_t fn_UIString_ToSystemString = nullptr;
fn_UI_AddDialog_t fn_UI_AddDialog = nullptr;
fn_Element_GetPaddedSize_t fn_Element_GetPaddedSize = nullptr;
fn_RadioElement_SetActiveRadio_t fn_RadioElement_SetActiveRadio = nullptr;
fn_ToolbarDialog_Ctor_t fn_ToolbarDialog_Ctor = nullptr;
fn_Toolbar_ShowGroup_t fn_Toolbar_ShowGroup = nullptr;
fn_Toolbar_Utility_t fn_Toolbar_Utility = nullptr;
fn_Toolbar_ToggleAll_t fn_Toolbar_ToggleAll = nullptr;
uintptr_t g_ToolbarDialog_Vtable = 0;   // PTR_FUN_14015fa00
uintptr_t g_PauseDialog_Vtable = 0;     // PTR_FUN_14015f550
uintptr_t g_ToolUI_ChildDialog_Vtable = 0; // PTR_FUN_14015f5a0
// fn_Factory_CreateObject, g_TypeId_ToolController, g_TypeId_ToolSetting — defined above (entityui section)
fn_ToolbarSort_t fn_ToolbarSort = nullptr;
uint32_t* g_TypeId_ToolbarExternal = nullptr;
uint32_t* g_TypeId_ToolbarItem = nullptr;
fn_RadioElement_FindCheckBoxes_t fn_RadioElement_FindCheckBoxes = nullptr;
fn_FindDialogByControllerType_t fn_FindDialogByControllerType = nullptr;
// fn_Toolbar_FindEntity — inlined as Toolbar_FindEntity_Impl in toolbardialog.cpp
// fn_Toolbar_CenterOnEntity — inlined as Toolbar_CenterOnEntity_Impl in engine_funcs.h
uint32_t* g_TypeId_MultiObjectComponent = nullptr;
fn_EntityPanel_SyncTracking_t fn_EntityPanel_SyncTracking = nullptr;
fn_CameraSys_ComputeViewTransform_t fn_CameraSys_ComputeViewTransform = nullptr;
fn_Transform_AsTransform3_t fn_Transform_AsTransform3 = nullptr;
fn_Camera_SetCameraToWorld_t fn_Camera_SetCameraToWorld = nullptr;
// fn_ToolbarDialog_CloseSubDialog — inlined as ToolbarDialog_CloseSubDialog_Impl in toolbardialog.cpp
// fn_ToolbarDialog_ChangeSpeed — inlined as ToolbarDialog_ChangeSpeed_Impl in toolbardialog.cpp
// fn_ToolbarDialog_UpdateButtons — inlined as ToolbarDialog_UpdateButtons_Impl in toolbardialog.cpp
uint32_t* g_TypeId_ToolbarElement = nullptr;
uint32_t* g_TypeId_ToolbarDialogParent = nullptr;
uint32_t* g_TypeId_ComboElement = nullptr;
fn_FindDialogByName_t fn_FindDialogByName = nullptr;
// fn_ResolveExternalByName — REPLACED with inline ResolveExternalByName_Impl in engine_funcs.h
fn_External_External_t fn_External_External = nullptr;
fn_External_Dtor_t fn_External_Dtor = nullptr;

// Phase F Wave 2C: EntityUI::Slot8, ResidenceUI::Slot7, EventNotifyDialog::Slot7
fn_KeyMap_GetValue_t fn_KeyMap_GetValue = nullptr;
fn_UI_GetFirstContainedDialog_t fn_UI_GetFirstContainedDialog = nullptr;
fn_EditTool_SelectEntity_t fn_EditTool_SelectEntity = nullptr;
float* g_pAgeMultiplier = nullptr;
float* g_pNotifyTimerDuration = nullptr;
fn_Time_Ctor_Float_t fn_Time_Ctor_Float = nullptr;
fn_String_SubString_t fn_String_SubString = nullptr;
fn_String_Assign_t fn_String_Assign = nullptr;
fn_UIString_Ctor_Entry_t fn_UIString_Ctor_Entry = nullptr;
fn_UIString_DefaultCtor_t fn_UIString_DefaultCtor = nullptr;
fn_Sprite_Default_Ctor_t fn_Sprite_Default_Ctor = nullptr;
fn_Dialog_GetReference_t fn_Dialog_GetReference = nullptr;
fn_Audio_Scene_PlayEffect_t fn_Audio_Scene_PlayEffect = nullptr;
void** g_EventPopupDialog_Vtable = nullptr;
fn_StringTable_GetText_t fn_StringTable_GetText = nullptr;
fn_DocumentTextElement_GetLink_t fn_DocumentTextElement_GetLink = nullptr;
fn_DocumentTextElement_SetText_UIString_t fn_DocumentTextElement_SetText_UIString = nullptr;
fn_DocumentTextElement_SetText_t fn_DocumentTextElement_SetText = nullptr;
fn_ContainerElement_SetOffsets_t fn_ContainerElement_SetOffsets = nullptr;
fn_UIString_Ctor_Index_t fn_UIString_Ctor_Index = nullptr;
fn_String_Equal_t fn_String_Equal = nullptr;
uint32_t* g_TypeId_StringTable = nullptr;
uint32_t* g_TypeId_HelpFirstElem = nullptr;
uint32_t* g_TypeId_HelpComboElem = nullptr;
uint32_t* g_TypeId_HelpDocTextElem = nullptr;
uint32_t* g_TypeId_HelpContainerElem = nullptr;
uint32_t* g_TypeId_HelpButtonElem = nullptr;
// fn_HelpUI_ArrayResize — now inline as HelpUI_ArrayResize_Impl in helpui.cpp

// NewGameDialog support
void** g_pNewGameSettings = nullptr;
fn_Random_Rand_t fn_Random_Rand = nullptr;
fn_Random_RandFloat_t fn_Random_RandFloat = nullptr;
// fn_GenerateTownName — REMOVED, now reimplemented in newgamedialog.cpp
fn_NameDB_GenerateName_t fn_NameDB_GenerateName = nullptr;
uint32_t* g_TypeId_NameDatabase = nullptr;
// fn_String_SubString already defined above
fn_String_Equals_t fn_String_Equals = nullptr;
fn_String_Append_t fn_String_Append = nullptr;
fn_StartNewGame_t fn_StartNewGame = nullptr;
uint32_t* g_TypeId_MapGenerator = nullptr;
uint32_t* g_TypeId_ClimateGenerator = nullptr;
uint32_t* g_TypeId_TerrainGenerator = nullptr;

// OptionsDialog support — Runtime DLL exports
fn_VideoDevice_ValidateConfiguration_t fn_VideoDevice_ValidateConfiguration = nullptr;
fn_VideoDevice_GetMultisampleModeCount_t fn_VideoDevice_GetMultisampleModeCount = nullptr;
fn_VideoDevice_IsMultisampleModeSupported_t fn_VideoDevice_IsMultisampleModeSupported = nullptr;

// OptionsDialog exe helpers — OnOk/OnCancel/OnApply reimplemented in code/Game/Dialog/optionsdialog.cpp
// PopulateInputBindList and OnInputBindClick now reimplemented in code/Game/Dialog/optionsdialog.cpp
// fn_OptionsDialog_PopulateInputBindList — REIMPLEMENTED in code/Game/Dialog/optionsdialog.cpp
// fn_OptionsDialog_OnInputBindClick — REIMPLEMENTED in code/Game/Dialog/optionsdialog.cpp

// ShowDialog, DeletePtr, KeyMap input functions, dialog ctors
fn_UI_ShowDialog_t fn_UI_ShowDialog = nullptr;
fn_InternalBasePtr_DeletePtr_t fn_InternalBasePtr_DeletePtr = nullptr;
fn_KeyMap_SetInput_t fn_KeyMap_SetInput = nullptr;
fn_KeyMap_ClearInput2_t fn_KeyMap_ClearInput2 = nullptr;
fn_KeyMap_ClearInput3_t fn_KeyMap_ClearInput3 = nullptr;
fn_KeyMap_GetIdForDeviceInput_t fn_KeyMap_GetIdForDeviceInput = nullptr;
fn_KeyMap_RegisterInput_t fn_KeyMap_RegisterInput = nullptr;
fn_KeyMap_ClearAll_t fn_KeyMap_ClearAll = nullptr;
fn_KeyMap_Load_t fn_KeyMap_Load = nullptr;
fn_OptionInputBindDialog_Ctor_t fn_OptionInputBindDialog_Ctor = nullptr;
fn_MessageDialog_Ctor_t fn_MessageDialog_Ctor = nullptr;

void** g_pAudioConfigSingleton = nullptr;
void** g_pVideoConfigSingleton = nullptr;
fn_VideoDevice_GetInterface_t fn_VideoDevice_GetInterface = nullptr;
fn_VideoDevice_GetInterfaceCount_t fn_VideoDevice_GetInterfaceCount = nullptr;
fn_Sprite_DefaultCtor_t fn_Sprite_DefaultCtor = nullptr;
fn_TabElement_SetActiveTab_t fn_TabElement_SetActiveTab = nullptr;
// fn_Element_GetParent — now inline in optionsdialog.cpp (vtable call at +0x48)
int32_t** g_pGameSettingsPtr = nullptr;

// StorageUI support
fn_ExternalFactory_FindNext_t fn_ExternalFactory_FindNext = nullptr;
// fn_StorageUI_SortItems — reimplemented as StorageUI_SortItems() in storageui.cpp

// MapUI helpers — REIMPLEMENTED in code/Game/Dialog/mapui.cpp
// fn_MapUI_SetupImage — now MapUI_SetupImage()
// fn_MapUI_HandleClick — now MapUI_HandleClick()
fn_MapUI_ProjectCornersToGround_t fn_MapUI_ProjectCornersToGround = nullptr;

// ToolUI support
fn_DialogInstance_SetUpperLeft_t fn_DialogInstance_SetUpperLeft = nullptr;
fn_UI_BringDialogToFront_t fn_UI_BringDialogToFront = nullptr;
// fn_UIDesc_AddControllers removed — reimplemented as UIDesc_AddControllers_Impl()
uint32_t* g_TypeId_UIDescription = nullptr;
fn_TabElement_AddTab_t fn_TabElement_AddTab = nullptr;
fn_DialogInstance_AddControllerTab_t fn_DialogInstance_AddControllerTab = nullptr;
fn_DialogInstance_AddController6_t fn_DialogInstance_AddController6 = nullptr;

// DragTool support
fn_KeyMap_Released_t fn_KeyMap_Released = nullptr;
fn_DialogInstance_GetRoot_t fn_DialogInstance_GetRoot = nullptr;
fn_AudioScene_PlayEffect_t fn_AudioScene_PlayEffect = nullptr;

// Player/Tool module — shared engine APIs
fn_SceneAddObject_t fn_SceneAddObject = nullptr;
fn_SceneRemoveObject_t fn_SceneRemoveObject = nullptr;
fn_Transform_FromYawPitchOffset_t fn_Transform_FromYawPitchOffset = nullptr;
fn_Geometry_SetTransform_t fn_Geometry_SetTransform = nullptr;
fn_MeshInstance_UpdateHierarchy_t fn_MeshInstance_UpdateHierarchy = nullptr;
fn_TileRender_t fn_TileRender = nullptr;
fn_TileValidityCheck_t fn_TileValidityCheck = nullptr;
fn_FencedPlacementParams_t fn_FencedPlacementParams = nullptr;
fn_HighlightRemove_t fn_HighlightRemove = nullptr;

// ModDetailsDialog support
fn_Element_Hide_t fn_Element_Hide = nullptr;
fn_Modification_Find_t fn_Modification_Find = nullptr;
fn_Configuration_GetVersionString_t fn_Configuration_GetVersionString = nullptr;
fn_String_AppendStr_t fn_String_AppendStr = nullptr;
fn_String_Concat_t fn_String_Concat = nullptr;
fn_Filename_Less_t fn_Filename_Less = nullptr;
// ModDetails_StringArrayPush — now static inline in moddetailsdialog.cpp

// CitizenUI helpers
uint32_t* g_TypeId_CitizenDescription = nullptr;
uint32_t* g_TypeId_HappinessComponent = nullptr;
uint32_t* g_TypeId_ProfessionComponent = nullptr;
void** g_FollowDialog_Vtable = nullptr;
// fn_StatusBarUI_StatusIconHandler — REPLACED with static StatusBarUI_StatusIconHandler_Impl() in statusbarui.cpp

// ProductionUI support
// fn_TrackerComponent_Rebuild — inline in engine_funcs.h

// ProfessionUI support
fn_External_GetPath_t fn_External_GetPath = nullptr;
fn_Filename_GetPathIdentifier_t fn_Filename_GetPathIdentifier = nullptr;
// BSTRebalance — now static inline in professionui.cpp
// fn_ProfessionUI_HandleSpinner — reimplemented in professionui.cpp
// fn_ProfessionUI_HandleButton — reimplemented in professionui.cpp

// MainMenuDialog support
fn_App_Quit_t fn_App_Quit = nullptr;
fn_Configuration_CurrentVersion_t fn_Configuration_CurrentVersion = nullptr;
fn_Configuration_CurrentBuildId_t fn_Configuration_CurrentBuildId = nullptr;
fn_InternalBasePtr_CheckPtr_t fn_InternalBasePtr_CheckPtr = nullptr;
fn_DialogInstance_Ctor_t fn_DialogInstance_Ctor = nullptr;
void** g_MainMenuDialog_BuildIdVtable = nullptr;
// fn_MainMenu_* handlers — REMOVED, now reimplemented in mainmenudialog.cpp
// Dialog ctor function pointers (used by reimplemented Open* handlers)
fn_NewGameDialog_Ctor_t fn_NewGameDialog_Ctor = nullptr;
fn_SaveLoadDialog_Ctor_t fn_SaveLoadDialog_Ctor = nullptr;
fn_OptionsDialog_Ctor_t fn_OptionsDialog_Ctor = nullptr;
fn_ScenarioDialog_Ctor_t fn_ScenarioDialog_Ctor = nullptr;
fn_MapParamsDialog_Ctor_t fn_MapParamsDialog_Ctor = nullptr;
fn_ModDialog_Ctor_t fn_ModDialog_Ctor = nullptr;
void** g_AchievementDialog_Vtable = nullptr;
void** g_GoalDialog_Vtable = nullptr;

// ModDialog support
void** g_ppModificationSingleton = nullptr;
fn_Modification_GetCount_t fn_Modification_GetCount = nullptr;
fn_Modification_GetMod_t fn_Modification_GetMod = nullptr;
fn_Modification_Swap_t fn_Modification_Swap = nullptr;
fn_Modification_Enable_t fn_Modification_Enable = nullptr;
fn_Modification_RefreshList_t fn_Modification_RefreshList = nullptr;
fn_Modification_SaveList_t fn_Modification_SaveList = nullptr;
fn_DialogInstance_OnElementEvent_t fn_DialogInstance_OnElementEvent = nullptr;
fn_DialogInstance_AddController_t fn_DialogInstance_AddController = nullptr;
fn_DialogInstance_OnUpdateLayout_t fn_DialogInstance_OnUpdateLayout = nullptr;
fn_DialogInstance_OnUpdate_t fn_DialogInstance_OnUpdate = nullptr;
// fn_ModDialog_RefreshDisplay — REPLACED with static ModDialog_RefreshDisplay_Impl() in moddialog.cpp
// fn_ModDialog_ToggleMod — REPLACED with inline impl in moddialog.cpp
// fn_ModDialog_MoveToTop — REPLACED with inline impl in moddialog.cpp
// fn_ModDialog_ShowDetails — REPLACED with inline impl in moddialog.cpp
// fn_ModDialog_Apply — REPLACED with inline impl in moddialog.cpp
uintptr_t g_ModDetailsDialog_Vtable = 0;  // PTR_FUN_14015c670
fn_ModDialog_CloseAndReload_t fn_ModDialog_CloseAndReload = nullptr;
fn_ModDialog_RefreshDisplay_Orig_t fn_ModDialog_RefreshDisplay_Orig = nullptr;
fn_MainMenu_Reload_t fn_MainMenu_Reload = nullptr;
fn_Game_ReloadWithSave_t fn_Game_ReloadWithSave = nullptr;
fn_SavedState_Ctor_t fn_SavedState_Ctor = nullptr;
uintptr_t g_ReloadDialog_Vtable = 0;  // PTR_FUN_14015c480

// IO::Serializer (shared globals with g_ prefix)
g_fn_Serializer_Ctor_t g_fn_Serializer_Ctor = nullptr;
g_fn_Serializer_Dtor_t g_fn_Serializer_Dtor = nullptr;
g_fn_Serialize_Int_t g_fn_Serialize_Int = nullptr;
g_fn_Serialize_Bool_t g_fn_Serialize_Bool = nullptr;
g_fn_Serializer_GetVersion_t g_fn_Serializer_GetVersion = nullptr;

// TownHallUI support
uint32_t* g_TypeId_TabElement = nullptr;
// fn_TownHallUI_InitGeneral — reimplemented as TownHallUI_InitGeneral() in townhallui.cpp
fn_TownHallUI_SubFunc_t fn_TownHallUI_InitProfessions = nullptr;
fn_TownHallUI_SubFunc_t fn_TownHallUI_InitProduction = nullptr;
fn_TownHallUI_SubFunc_t fn_TownHallUI_InitInventory = nullptr;
fn_TownHallUI_SubFunc_t fn_TownHallUI_InitAgriculture = nullptr;
// fn_TownHallUI_UpdateGraph — reimplemented in townhallui.cpp
// fn_TownHallUI_UpdateGeneral — reimplemented in townhallui.cpp
// fn_TownHallUI_UpdateProfessions — reimplemented in townhallui.cpp
fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateProduction = nullptr;
fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateAgriculture = nullptr;
fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateInventory = nullptr;
fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateAll = nullptr;
// fn_TownHallUI_OnProfessionSpinner — reimplemented in townhallui.cpp
void** g_pGameSingleton = nullptr;
// GroupRating helpers
fn_GroupRating_Init_t fn_GroupRating_Init = nullptr;
fn_GroupRating_Update_t fn_GroupRating_Update = nullptr;
// GraphElement functions
fn_GraphElement_RemoveGraphs_t fn_GraphElement_RemoveGraphs = nullptr;
fn_GraphElement_AddGraph_t fn_GraphElement_AddGraph = nullptr;
fn_GraphElement_AddGraphPoint_t fn_GraphElement_AddGraphPoint = nullptr;
// StringTable
fn_StringTable_GetCount_t fn_StringTable_GetCount = nullptr;
// Production stat query
fn_QueryProductionStat_t fn_QueryProductionStat = nullptr;
// Citizen assignment (spinner)
fn_CitizenList_FindIdle_t fn_CitizenList_FindIdle = nullptr;
fn_CitizenList_FindWorking_t fn_CitizenList_FindWorking = nullptr;
fn_CitizenList_FindStudent_t fn_CitizenList_FindStudent = nullptr;
fn_CitizenList_FindAny_t fn_CitizenList_FindAny = nullptr;
fn_CitizenList_ShuffleEntities_t fn_CitizenList_ShuffleEntities = nullptr;
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

// TradeUI support
fn_Element_SetNeedsSizeCalc_t fn_Element_SetNeedsSizeCalc = nullptr;
fn_TradeUI_RefreshDisplay_t fn_TradeUI_RefreshDisplay = nullptr;
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
fn_DynArray_Clear_0x08_t fn_DynArray_Clear_0x08 = nullptr;
fn_TradeUI_OnTradeButton_t fn_TradeUI_OnTradeButton = nullptr;
uint32_t* g_TypeId_TableElement = nullptr;
uint32_t* g_TypeId_SpinnerElement = nullptr;
uint32_t* g_TypeId_TextElement = nullptr;
uint32_t* g_TypeId_LabelElement = nullptr;
uint32_t* g_TypeId_NumberEditElement = nullptr;
uint32_t* g_TypeId_SliderElement = nullptr;
uint32_t* g_TypeId_SelectDescription = nullptr;
fn_Building_GetSellableCount_t fn_Building_GetSellableCount = nullptr;
fn_StorageTree_Insert_t fn_StorageTree_Insert = nullptr;
fn_Building_RemoveBuyOrder_t fn_Building_RemoveBuyOrder = nullptr;
fn_Building_AddBuyToStorage_t fn_Building_AddBuyToStorage = nullptr;
fn_Building_AddPurchaseOrder_t fn_Building_AddPurchaseOrder = nullptr;
uintptr_t g_NoiseUI_ImageBufferVtable = 0;
// fn_NoiseUI_SetupImage — REIMPLEMENTED in code/Game/Dialog/noiseui.cpp
// fn_NoiseUI_GenerateNoise — REIMPLEMENTED in code/Game/Dialog/noiseui.cpp
// NoiseUI runtime dependencies
fn_PerlinNoise_Ctor_t fn_PerlinNoise_Ctor = nullptr;
fn_PerlinNoise_Generate_t fn_PerlinNoise_Generate = nullptr;
// Random default ctor: uses existing fn_Random_Default_Ctor
fn_Random_RandTol_t fn_Random_RandTol = nullptr;
fn_String_ToFloat_t fn_String_ToFloat = nullptr;
// --- ModDialog::Slot4 support ---
fn_MaterialInstance_Ctor_t fn_MaterialInstance_Ctor = nullptr;
fn_MaterialInstance_SetMaterial_t fn_MaterialInstance_SetMaterial = nullptr;
fn_MaterialInstance_AddTexture_t fn_MaterialInstance_AddTexture = nullptr;
fn_MaterialInstance_BuildParameters_t fn_MaterialInstance_BuildParameters = nullptr;
fn_SpriteSheet_Ctor_t fn_SpriteSheet_Ctor = nullptr;
fn_SpriteSheet_AddSpriteInfo_t fn_SpriteSheet_AddSpriteInfo = nullptr;
fn_SpriteSheet_SetSpriteInfo_t fn_SpriteSheet_SetSpriteInfo = nullptr;
fn_Sprite_Ctor_ByIndex_t fn_Sprite_Ctor_ByIndex = nullptr;
fn_ImageBuffer_Width_t fn_ImageBuffer_Width = nullptr;
fn_ImageBuffer_Height_t fn_ImageBuffer_Height = nullptr;
uintptr_t g_ModDialog_MaterialInstanceVtable = 0;
uintptr_t g_ModDialog_SpriteSheetVtable = 0;
fn_ModDialog_ArrayPush_t fn_ModDialog_ArrayPush = nullptr;
uint32_t* g_TypeId_BrowserController = nullptr;
uint32_t* g_TypeId_Material = nullptr;
float* g_fUVScale = nullptr;
fn_Weather_ResetTime_t fn_Weather_ResetTime = nullptr;
fn_UI_SetCursor_t fn_UI_SetCursor = nullptr;
fn_UI_SetFullscreenFade_t fn_UI_SetFullscreenFade = nullptr;
fn_DebugUI_SetDayLighting_t fn_DebugUI_SetDayLighting = nullptr;
uint32_t* g_TypeId_DiseaseDescription = nullptr;
uint32_t* g_TypeId_DrawableType = nullptr;
fn_Game_SpawnResources_t fn_Game_SpawnResources = nullptr;
fn_Game_TriggerDisasterByType_t fn_Game_TriggerDisasterByType = nullptr;
fn_Game_DestroyAllRawMaterials_t fn_Game_DestroyAllRawMaterials = nullptr;
fn_Game_SpawnItemsInStorage_t fn_Game_SpawnItemsInStorage = nullptr;
fn_Game_SpawnCitizens_t fn_Game_SpawnCitizens = nullptr;
fn_DebugUI_TriggerDisasterOnBuilding_t fn_DebugUI_TriggerDisasterOnBuilding = nullptr;
fn_Game_StartRandomFire_t fn_Game_StartRandomFire = nullptr;
fn_Game_SpawnTornado_t fn_Game_SpawnTornado = nullptr;
fn_Game_InfectRandomCitizen_t fn_Game_InfectRandomCitizen = nullptr;
fn_DebugUI_OnDiseaseComboChanged_t fn_DebugUI_OnDiseaseComboChanged = nullptr;
// fn_DebugUI_OnDrawComboChanged removed — now reimplemented in code/Game/Dialog/debugui.cpp
fn_DrawGeometry_Ctor_t fn_DrawGeometry_Ctor = nullptr;
fn_DrawGeometry_Init_t fn_DrawGeometry_Init = nullptr;
fn_AudioObjectGroup_Ctor_t fn_AudioObjectGroup_Ctor = nullptr;
fn_AudioScene_RegisterObject_t fn_AudioScene_RegisterObject = nullptr;
fn_Entity_FindAudioComponent_t fn_Entity_FindAudioComponent = nullptr;
fn_Graphics_Scene_AddGlobalObject_t fn_Graphics_Scene_AddGlobalObject = nullptr;
fn_Graphics_Scene_AddObject_t       fn_Graphics_Scene_AddObject       = nullptr;
fn_Graphics_GraphicsMesh_GetMaterialList_t fn_Graphics_GraphicsMesh_GetMaterialList = nullptr;
fn_Graphics_MeshInstance_SetMaterialInstance_t fn_Graphics_MeshInstance_SetMaterialInstance = nullptr;
fn_Graphics_ParticleSystemInstance_Ctor_t fn_Graphics_ParticleSystemInstance_Ctor = nullptr;
fn_Graphics_ParticleInstance_IncrementInstanceIndex_t fn_Graphics_ParticleInstance_IncrementInstanceIndex = nullptr;
fn_Graphics_Particle_Ctor_t fn_Graphics_Particle_Ctor = nullptr;
fn_Graphics_Scene_Clean_t fn_Graphics_Scene_Clean = nullptr;

// --- MenuScene 3D rendering (Phase A) ---
fn_Graphics_Renderer_Ctor_t      fn_Graphics_Renderer_Ctor      = nullptr;
fn_Graphics_Scene_Ctor_t         fn_Graphics_Scene_Ctor         = nullptr;
fn_Graphics_SunLight_Ctor_t      fn_Graphics_SunLight_Ctor      = nullptr;
fn_Graphics_SunLight_Set_t       fn_Graphics_SunLight_Set       = nullptr;
fn_Graphics_MeshInstance_Ctor_t  fn_Graphics_MeshInstance_Ctor  = nullptr;
fn_Graphics_MeshInstance_SetMesh_t fn_Graphics_MeshInstance_SetMesh = nullptr;
fn_Graphics_Renderer_Draw_t      fn_Graphics_Renderer_Draw      = nullptr;
fn_Math_Vector3_FromYawPitch_t   fn_Math_Vector3_FromYawPitch   = nullptr;
fn_Math_Transform_FromLookAt_t   fn_Math_Transform_FromLookAt   = nullptr;
fn_Video_Context_SetFog_t        fn_Video_Context_SetFog        = nullptr;
fn_Math_Transform_Ctor_t         fn_Math_Transform_Ctor         = nullptr;
fn_Math_Transform3_Identity_t    fn_Math_Transform3_Identity    = nullptr;
fn_Volume_Frustum_Ctor_t         fn_Volume_Frustum_Ctor         = nullptr;
fn_Graphics_Camera_Ctor_t        fn_Graphics_Camera_Ctor        = nullptr;
fn_Graphics_Camera_SetFrustum_t  fn_Graphics_Camera_SetFrustum  = nullptr;

fn_Audio_Scene_MoveEmitter_t fn_Audio_Scene_MoveEmitter = nullptr;
fn_Audio_Scene_SetEmitterVolume_t fn_Audio_Scene_SetEmitterVolume = nullptr;
fn_Vector3_Equals_t fn_Vector3_Equals = nullptr;
fn_DebugUI_OnDaySliderChanged_t fn_DebugUI_OnDaySliderChanged = nullptr;
fn_DebugUI_WriteLightingColors_t fn_DebugUI_WriteLightingColors = nullptr;
fn_Game_GetMonthFromTime_t fn_Game_GetMonthFromTime = nullptr;

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
uint32_t* g_TypeId_ResidenceComponent_FC = nullptr;
uint32_t* g_TypeId_ResidenceDescription = nullptr;
fn_ResidenceComponent_GetOccupantCount_t fn_ResidenceComponent_GetOccupantCount = nullptr;
fn_ResidenceComponent_GetSpawnPoint_t fn_ResidenceComponent_GetSpawnPoint = nullptr;
fn_CitizenComponent_Setup_t fn_CitizenComponent_Setup = nullptr;
fn_DisasterComponent_Infect_t fn_DisasterComponent_Infect = nullptr;
fn_BuildingList_CheckCondition_t fn_BuildingList_CheckCondition = nullptr;

// --- SaveLoadDialog::Load helpers ---
// fn_GameUpdateLoop_Ctor — REMOVED, now reimplemented in saveloaddialog.cpp
fn_UpdateLoop_Ctor_t fn_UpdateLoop_Ctor = nullptr;
void** g_GameUpdateLoop_Vtable = nullptr;
fn_LoaderLoop_Ctor_t fn_LoaderLoop_Ctor = nullptr;
fn_DialogLoaderLoopT_Ctor_t fn_DialogLoaderLoopT_Ctor = nullptr;
fn_Random_Default_Ctor_t fn_Random_Default_Ctor = nullptr;
void** g_GameLoadDialogLoopT_Vtable = nullptr;
void** g_GameLoaderLoop_Vtable = nullptr;
// fn_SetEngineNextLoop — REPLACED with inline SetEngineNextLoop() in engine_funcs.h
// fn_SetDialogLoaderNextLoop — REPLACED with inline SetDialogLoaderNextLoop() in engine_funcs.h
// fn_GetLoadingSoundString — REPLACED with inline GetLoadingSoundString() in engine_funcs.h
fn_DialogLoopT_Ctor_t fn_DialogLoopT_Ctor = nullptr;
fn_DialogLoopT_Update_t fn_DialogLoopT_Update = nullptr;
fn_DialogLoopT_Startup_t fn_DialogLoopT_Startup = nullptr;
fn_DialogLoaderLoopT_Startup_t fn_DialogLoaderLoopT_Startup = nullptr;
fn_DialogLoaderLoopT_Load_t fn_DialogLoaderLoopT_Load = nullptr;
fn_DialogLoopT_Load_t fn_DialogLoopT_Load = nullptr;
fn_AudioScene_Ctor_t fn_AudioScene_Ctor = nullptr;
// fn_MenuDialogLoopT_Ctor — REMOVED, now reimplemented as inline MenuDialogLoopT_Ctor() in engine_funcs.h
void** g_MenuDialogLoopT_Vtable = nullptr;

// --- SpriteCaptureUI helpers ---
fn_Input_Device_GetValue_t fn_Input_Device_GetValue = nullptr;
fn_Input_Device_Pressed_t  fn_Input_Device_Pressed  = nullptr;
fn_UI_EndSpriteCapture_t   fn_UI_EndSpriteCapture   = nullptr;
void** g_pFileSystemSingleton = nullptr;
fn_FileSystem_GetSourceDirectory_t fn_FileSystem_GetSourceDirectory = nullptr;
fn_External_SaveText_t     fn_External_SaveText     = nullptr;
fn_Filename_CtorString_t   fn_Filename_CtorString   = nullptr;
fn_Filename_CtorCStr_t     fn_Filename_CtorCStr     = nullptr;
fn_Filename_Ctor_t         fn_Filename_Ctor         = nullptr;
fn_Filename_Dtor_t         fn_Filename_Dtor         = nullptr;
fn_String_Concat_t         fn_String_ConcatSS       = nullptr;
fn_String_Concat_t         fn_String_ConcatSS2      = nullptr;

// ============================================================
// Engine globals
// ============================================================

// Engine::Loader instance (_loader) @ 0x1801fe7b8
void* g_pLoader = nullptr;

// _loader.m_flags @ 0x1801fe7c0
uint32_t* g_pLoaderFlags = nullptr;

// _loader.m_completionEvent @ 0x1801fe7e0
HANDLE* g_pLoaderCompletionEvent = nullptr;

// Application::Application::_singleton @ 0x1801feb80
void** g_pAppSingleton = nullptr;

// ============================================================
// Stage 3: OptionsDialog Runtime exports
// ============================================================
fn_AudioConfig_Save_t fn_AudioConfig_Save = nullptr;
fn_AudioConfig_Startup_t fn_AudioConfig_Startup = nullptr;
fn_VideoConfig_Save_t fn_VideoConfig_Save = nullptr;
fn_InputConfig_Save_t fn_InputConfig_Save = nullptr;
fn_InputConfig_Load_t fn_InputConfig_Load = nullptr;
void** g_pInputConfigSingleton = nullptr;
fn_Persistence_WriteSettings_t fn_Persistence_WriteSettings = nullptr;
fn_Persistence_ReadSettings_t fn_Persistence_ReadSettings = nullptr;
fn_InputDevice_EnableMouseClipping_t fn_InputDevice_EnableMouseClipping = nullptr;
fn_InputDevice_ResetState_t fn_InputDevice_ResetState = nullptr;
fn_VideoDevice_ApplyChangedSettings_t fn_VideoDevice_ApplyChangedSettings = nullptr;
fn_VideoDevice_Startup_t fn_VideoDevice_Startup = nullptr;
fn_VideoDevice_RecreateDynamic_t fn_VideoDevice_RecreateDynamic = nullptr;
fn_VideoDevice_ReleaseDynamic_t fn_VideoDevice_ReleaseDynamic = nullptr;
fn_VideoDevice_Shutdown_t fn_VideoDevice_Shutdown = nullptr;
fn_MainLoop_OnVideoError_t fn_MainLoop_OnVideoError = nullptr;
fn_MainLoop_OnVideoRestart_t fn_MainLoop_OnVideoRestart = nullptr;
fn_MainLoop_SetNextLoop_t fn_MainLoop_SetNextLoop = nullptr;
fn_BaseTypeInfo_Ctor_t fn_BaseTypeInfo_Ctor = nullptr;
void** g_pMainLoopSingleton = nullptr;
fn_UI_EnableCursor_t fn_UI_EnableCursor = nullptr;
fn_UI_GetFirstDialog_t fn_UI_GetFirstDialog = nullptr;
fn_UI_SetFullscreenFadeSprite_t fn_UI_SetFullscreenFadeSprite = nullptr;
fn_SaveGameSettings_t fn_SaveGameSettings = nullptr;
fn_ReadGameSettings_t fn_ReadGameSettings = nullptr;
fn_ApplyKeyMapToUI_t fn_ApplyKeyMapToUI = nullptr;
fn_UpdateAutoSaveTimer_t fn_UpdateAutoSaveTimer = nullptr;
uint32_t* g_TypeId_AutoSaveDialogType = nullptr;
fn_RegisterDefaultInputBindings_t fn_RegisterDefaultInputBindings = nullptr;

// CaptureBoxElement / FrustumOutlineElement support
fn_ElementDescription_GetFileVersion_t fn_ElementDescription_GetFileVersion = nullptr;
fn_Element_Draw_t fn_Element_Draw = nullptr;
fn_SpriteContext_AddLine_t fn_SpriteContext_AddLine = nullptr;
fn_SpriteContext_SetScissor4_t fn_SpriteContext_SetScissor4 = nullptr;
fn_SpriteContext_SetScissorVP_t fn_SpriteContext_SetScissorVP = nullptr;
fn_FrustumProject_t fn_FrustumProject = nullptr;

// Description class reimpl support
g_fn_String_Serialize_t g_fn_String_Serialize = nullptr;
fn_ElementDescription_Serialize_t fn_ElementDescription_Serialize = nullptr;
fn_ElementDescription_Dtor_t fn_ElementDescription_Dtor = nullptr;
fn_ElementDescription_Ctor_t fn_ElementDescription_Ctor = nullptr;
fn_ElementDescription_CreateInstance_t fn_ElementDescription_CreateInstance = nullptr;
fn_Element_Ctor_t fn_Element_Ctor = nullptr;
fn_Element_Dtor_t fn_Element_Dtor = nullptr;
fn_Element_ComputeLayout_t fn_Element_ComputeLayout = nullptr;
fn_Element_GetAlignedPosition_t fn_Element_GetAlignedPosition = nullptr;
fn_Sprite_Sprite_t fn_Sprite_Sprite = nullptr;
fn_Sprite_DefaultCtor2_t fn_Sprite_DefaultCtor2 = nullptr;
fn_ExternalPtr_Serialize_t fn_ExternalPtr_Serialize = nullptr;
fn_ExternalPtr_Serialize2_t fn_ExternalPtr_Serialize2 = nullptr;
fn_ExternalPtr_Serialize3_t fn_ExternalPtr_Serialize3 = nullptr;
fn_ExternalRefArray_Serialize_t fn_ExternalRefArray_Serialize = nullptr;
fn_Serializer_SerializeBulk_t fn_Serializer_SerializeBulk = nullptr;
fn_ComponentBase_Serialize_t fn_ComponentBase_Serialize = nullptr;
fn_DynArray_Resize_t fn_DynArray_Resize = nullptr;
fn_MemZeroFill_t fn_MemZeroFill = nullptr;
fn_StringArray_Resize_t fn_StringArray_Resize = nullptr;
fn_ExternalPtr_BuildSerialize_t fn_ExternalPtr_BuildSerialize = nullptr;
fn_WeatherResize_t fn_WeatherResize = nullptr;
fn_WeatherExternalSerialize_t fn_WeatherExternalSerialize = nullptr;
fn_ToolbarDescription_cleanup_t fn_ToolbarDescription_cleanup = nullptr;
fn_CreateBridgeDescription_Ctor_t fn_CreateBridgeDescription_Ctor = nullptr;
fn_CreateDragDescription_Ctor_t fn_CreateDragDescription_Ctor = nullptr;
fn_CreateFencedDescription_Ctor_t fn_CreateFencedDescription_Ctor = nullptr;

// TutorialSetupDialog helpers
fn_SerializeExternalPtr_t fn_SerializeExternalPtr = nullptr;
fn_TSD_Slot9Helper_t fn_TSD_Slot9Helper = nullptr;
fn_TSD_OpenDialog_t fn_TSD_OpenDialog = nullptr;
fn_TSD_SetDialogFlags_t fn_TSD_SetDialogFlags = nullptr;
fn_TSD_FindInstance_t fn_TSD_FindInstance = nullptr;
fn_TSD_FindByString_t fn_TSD_FindByString = nullptr;
fn_SetFollowBuilding_t fn_SetFollowBuilding = nullptr;
fn_SetEditMode_t fn_SetEditMode = nullptr;

// Phase G Wave 1: SaveLoadDialog + EventNotifyDialog
fn_SaveLoad_CheckMod_t fn_SaveLoad_CheckMod = nullptr;
fn_SaveLoad_SetSaveName_t fn_SaveLoad_SetSaveName = nullptr;
uint32_t* g_TypeId_DialogInstance = nullptr;
fn_MapGetTerrainPos_t fn_MapGetTerrainPos = nullptr;

// Phase H: Sub-dependency blockers
fn_Factory_GetTypeIdFromName_t fn_Factory_GetTypeIdFromName = nullptr;
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
fn_Vector3_Ctor_t fn_Vector3_Ctor = nullptr;
fn_PerformWork_Action_Ctor_t fn_PerformWork_Action_Ctor = nullptr;
fn_PerformWork_Action_Ctor3_t fn_PerformWork_Action_Ctor3 = nullptr;
fn_FindStorageForResource_t fn_FindStorageForResource = nullptr;
fn_FindComponent_WorkplaceC_t fn_FindComponent_WorkplaceC = nullptr;
fn_FindComponent_BuildStateC_t fn_FindComponent_BuildStateC = nullptr;
fn_FindComponent_StorageLocC_t fn_FindComponent_StorageLocC = nullptr;
fn_TryAssignWorkplace_t fn_TryAssignWorkplace = nullptr;
fn_FindComponent_RawMaterialC_t fn_FindComponent_RawMaterialC = nullptr;
fn_GetRemainingCapacity_t fn_GetRemainingCapacity = nullptr;
fn_BuildRoad_QueryStorage_t fn_BuildRoad_QueryStorage = nullptr;

// BuildStructure support
fn_Work_SerializeBase_t fn_Work_SerializeBase = nullptr;
fn_BST_SerializeHelper_t fn_BST_SerializeHelper = nullptr;
fn_Entity_FindComponent_Position_t fn_Entity_FindComponent_Position = nullptr;
fn_Entity_FindComponent_Build_t fn_Entity_FindComponent_Build = nullptr;
fn_Entity_FindComponent_BuildDesc_t fn_Entity_FindComponent_BuildDesc = nullptr;
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
fn_CitizenComponent_HasVacancy_t fn_CitizenComponent_HasVacancy = nullptr;
fn_FindStorageLocationComp_t fn_FindStorageLocationComp = nullptr;
fn_FindWorkplaceComp_t fn_FindWorkplaceComp = nullptr;

// Achievement Goal support
fn_Goal_BaseDtor_t fn_Goal_BaseDtor = nullptr;
fn_Goal_BaseCtor_t fn_Goal_BaseCtor = nullptr;
fn_Goal_BaseSerialize_t fn_Goal_BaseSerialize = nullptr;
fn_GoalInstance_BaseDtor_t fn_GoalInstance_BaseDtor = nullptr;
fn_GoalInstance_Persist_t fn_GoalInstance_Persist = nullptr;
fn_InternalBasePtr_Serialize_t fn_InternalBasePtr_Serialize = nullptr;
fn_IsRunning_t fn_IsRunning = nullptr;
fn_ExternalPtr_SerializeTutorial_t fn_ExternalPtr_SerializeTutorial = nullptr;
fn_BuildingTypeArray_Dtor_t fn_BuildingTypeArray_Dtor = nullptr;
fn_BuildingTypeArray_Resize_t fn_BuildingTypeArray_Resize = nullptr;
fn_StringHash_t fn_StringHash = nullptr;
fn_FindUIElementByPath_t fn_FindUIElementByPath = nullptr;
fn_ObjectInternal_Ctor_t fn_ObjectInternal_Ctor = nullptr;
fn_TutorialTreeInsert_t fn_TutorialTreeInsert = nullptr;
fn_TutorialStepExecute_t fn_TutorialStepExecute = nullptr;
uint32_t* g_TypeId_FinishTutorialGoal = nullptr;
void** g_pSingletonExref_151a80 = nullptr;
void** g_TutorialInstanceVtable = nullptr;

// ComponentEntity dependencies
uint32_t* g_TypeId_ComponentBase = nullptr;
fn_ExeArrayResizeFill_t fn_ExeArrayResizeFill = nullptr;

// CitizenBehavior dependencies
fn_CitizenBehavior_CountResources_t fn_CitizenBehavior_CountResources = nullptr;
fn_CitizenBehavior_CheckThresholds_t fn_CitizenBehavior_CheckThresholds = nullptr;
fn_CitizenBehavior_CollectStats_t fn_CitizenBehavior_CollectStats = nullptr;
fn_CitizenBehavior_ProcessPaths_t fn_CitizenBehavior_ProcessPaths = nullptr;
fn_CitizenBehavior_BuildPathGraph_t fn_CitizenBehavior_BuildPathGraph = nullptr;
fn_CitizenBehavior_ShuffleSchools_t fn_CitizenBehavior_ShuffleSchools = nullptr;
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
g_fn_Time_Serialize_t g_fn_Time_Serialize = nullptr;
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
    fn_InternalBasePtr_Ctor(retval, found);
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

// fn_ExeArrayResize: 8-byte elements. Copies existing, zeroes new entries.
// Original: FUN_1400032e0 (6 uses)
void fn_ExeArrayResize(void* arrayBase, int newCount) {
    uint8_t* base   = (uint8_t*)arrayBase;
    void**   pData  = (void**)base;
    int*     pCount = (int*)(base + 0x08);
    int*     pCap   = (int*)(base + 0x0C);

    int oldCount = *pCount;
    int oldCap   = *pCap;

    // Sanity guard: an absurd newCount means the caller passed a corrupted count
    // (e.g. a template desc whose component-count field read as garbage). Bail
    // loudly instead of MemAlloc'ing a multi-GB block (segfault).
    if (newCount < 0 || newCount > 0x100000) {
        fprintf(stderr, "[fn_ExeArrayResize] ABSURD newCount=%d (base=%p oldCount=%d oldCap=%d) — bail\n",
                newCount, arrayBase, oldCount, oldCap);
        fflush(stderr);
        return;
    }

    if (newCount > oldCap) {
        int newCap = (newCount == oldCount + 1 && oldCount != 0) ? newCount * 3 : newCount;
        void* newData = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap * 8, 8);
        if (*pData && oldCount > 0) {
            uint64_t* src = (uint64_t*)*pData;
            uint64_t* dst = (uint64_t*)newData;
            for (int i = 0; i < oldCount; i++) dst[i] = src[i];
        }
        if (newData) {
            uint64_t* dst = (uint64_t*)newData;
            for (int i = oldCount; i < newCap; i++) dst[i] = 0;
        }
        fn_MemFreeStatic(*pData);
        *pData = newData;
        *pCap  = newCap;
    } else if (newCount < oldCount) {
        if (*pData) {
            uint64_t* dst = (uint64_t*)*pData;
            for (int i = newCount; i < oldCount; i++) dst[i] = 0;
        }
    }
    *pCount = newCount;
}

// fn_ExeArrayResize2: 4-byte elements. Copies existing only (no zero-init for new).
// Original: FUN_1400021d0 (2 uses)
void fn_ExeArrayResize2(void* arrayBase, int newCount) {
    uint8_t* base   = (uint8_t*)arrayBase;
    void**   pData  = (void**)base;
    int*     pCount = (int*)(base + 0x08);
    int*     pCap   = (int*)(base + 0x0C);

    int oldCount = *pCount;
    int oldCap   = *pCap;

    if (newCount > oldCap) {
        int newCap = (newCount == oldCount + 1 && oldCount != 0) ? newCount * 3 : newCount;
        void* newData = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap * 4, 4);
        if (*pData && oldCount > 0) {
            uint32_t* src = (uint32_t*)*pData;
            uint32_t* dst = (uint32_t*)newData;
            for (int i = 0; i < oldCount; i++) dst[i] = src[i];
        }
        fn_MemFreeStatic(*pData);
        *pData = newData;
        *pCap  = newCap;
    }
    *pCount = newCount;
}

// fn_ExeArrayResize3: 8-byte elements. Copies existing, zeroes new entries.
// Original: FUN_140001bb0 (1 use)
void fn_ExeArrayResize3(void* arrayBase, int newCount) {
    uint8_t* base   = (uint8_t*)arrayBase;
    void**   pData  = (void**)base;
    int*     pCount = (int*)(base + 0x08);
    int*     pCap   = (int*)(base + 0x0C);

    int oldCount = *pCount;
    int oldCap   = *pCap;

    if (newCount > oldCap) {
        int newCap = (newCount == oldCount + 1 && oldCount != 0) ? newCount * 3 : newCount;
        void* newData = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap * 8, 8);
        if (*pData && oldCount > 0) {
            uint64_t* src = (uint64_t*)*pData;
            uint64_t* dst = (uint64_t*)newData;
            for (int i = 0; i < oldCount; i++) dst[i] = src[i];
        }
        if (newData) {
            uint64_t* dst = (uint64_t*)newData;
            for (int i = oldCount; i < newCap; i++) dst[i] = 0;
        }
        fn_MemFreeStatic(*pData);
        *pData = newData;
        *pCap  = newCap;
    } else if (newCount < oldCount) {
        if (*pData) {
            uint64_t* dst = (uint64_t*)*pData;
            for (int i = newCount; i < oldCount; i++) dst[i] = 0;
        }
    }
    *pCount = newCount;
}

// fn_ExeArrayPush: 4-byte elements. Resizes to newCount, fills new slot with *elementPtr.
// Original: FUN_140003600 (3 uses)
void fn_ExeArrayPush(void* arrayBase, int newCount, void* elementPtr) {
    uint8_t* base   = (uint8_t*)arrayBase;
    void**   pData  = (void**)base;
    int*     pCount = (int*)(base + 0x08);
    int*     pCap   = (int*)(base + 0x0C);

    int oldCount = *pCount;
    int oldCap   = *pCap;

    if (newCount > oldCap) {
        int newCap = (newCount == oldCount + 1 && oldCount != 0) ? newCount * 3 : newCount;
        void* newData = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap * 4, 4);
        if (*pData && oldCount > 0) {
            uint32_t* src = (uint32_t*)*pData;
            uint32_t* dst = (uint32_t*)newData;
            for (int i = 0; i < oldCount; i++) dst[i] = src[i];
        }
        fn_MemFreeStatic(*pData);
        *pData = newData;
        *pCap  = newCap;
    }

    // Fill new slot (at index newCount-1) from elementPtr
    if (*pData && newCount > 0 && elementPtr) {
        ((uint32_t*)*pData)[newCount - 1] = *(uint32_t*)elementPtr;
    }
    *pCount = newCount;
}

// ExeArrayResizeFill_CppImpl: 8-byte elements. Grows/shrinks an array; on growth,
// new entries are filled with *fillValue (existing entries are copied on realloc).
// Faithful reimpl of FUN_140003110 (assigned to fn_ExeArrayResizeFill at init).
// Pure C++ so it works in standalone (the exe is not mapped, so ResolveAddr on
// 0x140003110 returns NULL there) and in DLL mode alike — mirrors the fn_ExeArrayResize
// family. WAS THE START-TOWN SPAWN BUG: left NULL in standalone, so EntityList::
// RegisterEntity silently skipped the master-array append (entity count stayed 0).
static void ExeArrayResizeFill_CppImpl(void* arrayBase, int newCount, void* fillValue) {
    uint8_t* base   = (uint8_t*)arrayBase;
    void**   pData  = (void**)base;
    int*     pCount = (int*)(base + 0x08);
    int*     pCap   = (int*)(base + 0x0C);

    int oldCount = *pCount;
    if (newCount == oldCount) return;

    void* oldData = *pData;
    *pCount = newCount;

    if (*pCap < newCount) {
        int newCap = (newCount == oldCount + 1 && oldCount != 0) ? newCount * 3 : newCount;
        *pCap  = newCap;
        *pData = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap * 8, 8);
    }

    // start = oldCount when the buffer was reused (only new tail filled); 0 when it
    // was reallocated (re-copy existing [0,oldCount) from the old buffer, then fill).
    int start = (oldData == *pData) ? oldCount : 0;
    if (start < newCount && *pData) {
        uint64_t* dst    = (uint64_t*)*pData;
        uint64_t* oldArr = (uint64_t*)oldData;
        uint64_t  fill   = fillValue ? *(uint64_t*)fillValue : 0;
        for (int i = start; i < newCount; i++)
            dst[i] = (i < oldCount) ? oldArr[i] : fill;
    }

    if (oldData != *pData) fn_MemFreeStatic(oldData);
}

// ============================================================
// BuildStructure Work class helpers
// ============================================================
fn_TreeRemoveByKey_t fn_TreeRemoveByKey = nullptr;
fn_TreeRecursiveFree_t fn_TreeRecursiveFree = nullptr;
fn_ObjectInternal_Dtor_t fn_ObjectInternal_Dtor = nullptr;
fn_StateMachine_ReleaseTasks_t fn_StateMachine_ReleaseTasks = nullptr;

// ============================================================
// M3.5 — MenuScene::Serialize generic helpers
// ============================================================
fn_Geometry_Init_t fn_Geometry_Init = nullptr;
fn_Graphics_Geometry_Ctor_t fn_Graphics_Geometry_Ctor = nullptr;
fn_Video_VertexBuffer_Ctor_t fn_Video_VertexBuffer_Ctor = nullptr;
fn_Video_IndexBuffer_Ctor_t fn_Video_IndexBuffer_Ctor = nullptr;
fn_Transform_SerializeWrapper_t fn_Transform_SerializeWrapper = nullptr;
fn_Math_Transform_Serialize_t   fn_Math_Transform_Serialize   = nullptr;
fn_Math_Vector3_Serialize_t     fn_Math_Vector3_Serialize     = nullptr;
fn_Math_Vector4_Serialize_t     fn_Math_Vector4_Serialize     = nullptr;
fn_System_Color_Serialize_t     fn_System_Color_Serialize     = nullptr;
fn_Serializer_Serialize_Float_t fn_Serializer_Serialize_Float = nullptr;

fn_Array_Resize_t fn_Array_Resize_Stride20 = nullptr;
fn_Array_Resize_t fn_Array_Resize_Stride08 = nullptr;
fn_Array_Resize_t fn_Array_Resize_Stride14 = nullptr;
fn_Array_Serialize_Stride20_t fn_Array_Serialize_Stride20 = nullptr;

fn_Array_Serialize_External_t fn_Array_Serialize_External_GraphicsMesh = nullptr;
fn_Array_Serialize_External_t fn_Array_Serialize_External_Particle = nullptr;
fn_ExternalPtr_Serialize_t fn_ExternalPtr_Serialize_GraphicsMesh = nullptr;
fn_ExternalPtr_Serialize_t fn_ExternalPtr_Serialize_Weather = nullptr;

// M3.5 Path B session 2 — 0x60B struct array lifecycle helpers.
fn_Struct60B_Resize_t       fn_Struct60B_Resize             = nullptr;
fn_Struct60B_CopyCtor_t     fn_Struct60B_CopyCtor           = nullptr;
fn_Struct60B_DtorRange_t    fn_Struct60B_DtorRange          = nullptr;
fn_Struct60B_SubArrayCopy_t fn_Struct60B_SubArrayCopyStride14 = nullptr;
fn_Struct60B_SubArrayCopy_t fn_Struct60B_SubArrayCopyStride02 = nullptr;

// Runtime DLL primitives needed only by the standalone reimpls.
fn_Serializer_GetEncoding_t fn_Serializer_GetEncoding = nullptr;
fn_Serializer_Serialize_Uint_t fn_Serializer_Serialize_Uint = nullptr;
fn_ExternalFactory_IsReferenceModeEnabled_t fn_ExternalFactory_IsReferenceModeEnabled = nullptr;
fn_ExternalFactory_GetReferenceId_t fn_ExternalFactory_GetReferenceId = nullptr;
fn_ExternalFactory_GetPathForReferenceId_t fn_ExternalFactory_GetPathForReferenceId = nullptr;
fn_Filename_AssignOp_t fn_Filename_AssignOp = nullptr;

uint32_t** g_ppTypeInfo_GraphicsMesh = nullptr;
uint32_t** g_ppTypeInfo_Particle = nullptr;
uint32_t*  g_pTypeId_Weather = nullptr;

// M3.5 Path B session 3 — typed-ExternalPtr serialize helper at 0x140007140.
fn_ExternalPtr_Serialize_Typed_t fn_ExternalPtr_Serialize_Typed_7140 = nullptr;
uint32_t** g_ppTypeInfo_MaterialInstance = nullptr;

// M3.5 Path B session 5 — additional Geometry::Serialize resize helpers.
fn_Array_Resize_t fn_Array_Resize_Stride02       = nullptr;  // 0x14001c890
fn_Array_Resize_t fn_Array_Resize_Stride16A      = nullptr;  // 0x14001efd0
fn_Array_Resize_t fn_Array_Resize_ArrayOfArrays  = nullptr;  // 0x14001f0a0
fn_Array_Resize_t fn_Array_Resize_Stride16B      = nullptr;  // 0x14001ef10
fn_Array_Resize_t fn_Array_Resize_Stride04       = nullptr;  // 0x1400021d0

// M3.5 Path B session 4 — per-element 0x60B serialize + typed-ExternalPtr
// array serialize (Geometry::Serialize cascade points 10 + 13).
fn_Struct60B_Serialize_t                fn_Struct60B_Serialize                       = nullptr;  // 0x14001ffa0
fn_Array_Serialize_External_Typed_t     fn_Array_Serialize_External_MaterialInstance = nullptr;  // 0x140020150

// M3.5 Path B session 6 — heightmap mesh-build path helpers.
fn_TradeWork_sub_c1e0_t    fn_TradeWork_sub_c1e0    = nullptr;  // 0x1400ac1e0
fn_TradeWork_FUN_ac880_t   fn_TradeWork_FUN_ac880   = nullptr;  // 0x1400ac880
fn_TradeWork_helper_e270_t fn_TradeWork_helper_e270 = nullptr;  // 0x1400ae270
fn_Geometry_ChunkUpdate_t  fn_Geometry_ChunkUpdate  = nullptr;  // 0x1400aad50  M3.5 chunk-stream

// M3.5 Path B session 7 — image-buffer heightmap upload helper.
fn_TradeWork_sub_c780_t fn_TradeWork_sub_c780 = nullptr;  // 0x1400ac780

// M3.5 Path B session 8 — Geometry::Serialize body + extra serialize.
fn_TradeWork_impl_04d0_t fn_TradeWork_impl_04d0 = nullptr;  // 0x1400b04d0
fn_TradeWork_impl_f1a0_t fn_TradeWork_impl_f1a0 = nullptr;  // 0x1400af1a0

// M3.5 Path B session 8 — promoted IO::Serializer dispatch slots.
fn_Ser_SerInt_t       fn_Ser_Slot_SerInt_AC8       = nullptr;  // 0x140152ac8
fn_Ser_SerByte_t      fn_Ser_Slot_SerByte_410      = nullptr;  // 0x140152410
fn_Ser_SerFloat_t     fn_Ser_Slot_SerFloat_A60     = nullptr;  // 0x140152a60
fn_Ser_SerUShortArr_t fn_Ser_Slot_SerUShortArr_488 = nullptr;  // 0x140152488
fn_Ser_SerSizedRaw_t  fn_Ser_Slot_SerSizedRaw_3F8  = nullptr;  // 0x1401523f8
fn_Ser_SerSizedRaw_t  fn_Ser_Slot_SerSizedRaw_400  = nullptr;  // 0x140152400
fn_Ser_SerByteArr_t   fn_Ser_Slot_SerByteArr_618   = nullptr;  // 0x140152618

// M3.5 — Geometry vtable draw primitives (slot 0x14015f110)
fn_Renderer_GetCullCamera_t            fn_Renderer_GetCullCamera           = nullptr;
fn_Renderer_AddDrawItem_t              fn_Renderer_AddDrawItem             = nullptr;
fn_Renderer_GetShader_t                fn_Renderer_GetShader               = nullptr;
fn_Camera_GetFrustum_t                 fn_Camera_GetFrustum                = nullptr;
fn_Frustum_Contains_t                  fn_Frustum_Contains                 = nullptr;
fn_Frustum_MulTransform3_t             fn_Frustum_MulTransform3            = nullptr;
fn_Geometry_GetTransform_t             fn_Geometry_GetTransform            = nullptr;
fn_Transform_Inverse_t                 fn_Transform_Inverse                = nullptr;
fn_Transform_AsMatrix_t                fn_Transform_AsMatrix               = nullptr;
fn_Transform3_Copy_Ctor_t              fn_Transform3_Copy_Ctor             = nullptr;
fn_Vector3_FromFloats_Ctor_t           fn_Vector3_FromFloats_Ctor          = nullptr;
fn_InterpolatedState_Transform_Lerp_t  fn_InterpolatedState_Transform_Lerp = nullptr;
fn_Context_SetTransform_t              fn_Context_SetTransform             = nullptr;
fn_Context_SetColor_t                  fn_Context_SetColor                 = nullptr;
fn_Context_SetVertexBuffer_t           fn_Context_SetVertexBuffer          = nullptr;
fn_Context_SetImageBuffer_t            fn_Context_SetImageBuffer           = nullptr;
fn_Context_BeginIndexedVertices_t      fn_Context_BeginIndexedVertices     = nullptr;
fn_Context_EndIndexedVertices_t        fn_Context_EndIndexedVertices       = nullptr;
fn_Context_DrawIndexed_t               fn_Context_DrawIndexed              = nullptr;
fn_Geometry_UpdateAttached_t           fn_Geometry_UpdateAttached          = nullptr;
fn_Geometry_GetSubTransform_t          fn_Geometry_GetSubTransform         = nullptr;
fn_Geometry_GetInstanceTransform_t     fn_Geometry_GetInstanceTransform    = nullptr;
fn_Geometry_SetSharedInstanceData_t    fn_Geometry_SetSharedInstanceData   = nullptr;
fn_Geometry_Clean_t                    fn_Geometry_Clean                   = nullptr;

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

    // Input::Device::Update(Time const&)
    fn_InputDevice_Update = reinterpret_cast<fn_InputDevice_Update_t>(
        ResolveExport("?Update@Device@Input@@QEAAXAEBVTime@System@@@Z"));
    if (!fn_InputDevice_Update) {
        // Fallback: known RVA
        fn_InputDevice_Update = reinterpret_cast<fn_InputDevice_Update_t>(
            ResolveAddr(Module::Runtime, 0x1800e1490));
    }

    // Audio::Device::Update — resolve by name
    fn_AudioDevice_Update = reinterpret_cast<fn_AudioDevice_Update_t>(
        ResolveExport("?Update@Device@Audio@@QEAAXAEBVTime@System@@@Z"));

    // Audio::Device::FreeChannelPlatform — resolve by name
    fn_AudioDevice_FreeChannel = reinterpret_cast<fn_AudioDevice_FreeChannel_t>(
        ResolveExport("?FreeChannelPlatform@Device@Audio@@AEAAXH@Z"));

    // Application::Application::PlatformUpdate — private, returns bool
    fn_App_PlatformUpdate = reinterpret_cast<fn_App_PlatformUpdate_t>(
        ResolveExport("?PlatformUpdate@Application@1@AEAA_NXZ"));

    // Video::Context::ClearResources — resolve by name
    fn_VideoCtx_ClearResources = reinterpret_cast<fn_VideoCtx_ClearResources_t>(
        ResolveExport("?ClearResources@Context@Video@@QEAAXXZ"));

    // Video::Context::Reset — resolve by name
    fn_VideoCtx_Reset = reinterpret_cast<fn_VideoCtx_Reset_t>(
        ResolveExport("?Reset@Context@Video@@QEAAXXZ"));

    // Engine::Loader::Load(UpdateLoop*) — resolve by name
    fn_Loader_Load = reinterpret_cast<fn_Loader_Load_t>(
        ResolveExport("?Load@Loader@Engine@@QEAAXPEAVUpdateLoop@2@@Z"));

    // Application::Application::SleepThread(int ms) — resolve by name
    fn_App_SleepThread = reinterpret_cast<fn_App_SleepThread_t>(
        ResolveExport("?SleepThread@Application@1@QEAAXH@Z"));

    // UI::UserInterface::Update(Time const&) — resolve by name
    fn_UI_Update = reinterpret_cast<fn_UI_Update_t>(
        ResolveExport("?Update@UserInterface@UI@@QEAAXAEBVTime@System@@@Z"));
    if (!fn_UI_Update) {
        fn_UI_Update = reinterpret_cast<fn_UI_Update_t>(
            ResolveAddr(Module::Runtime, 0x1801213C0));
    }

    // UI::UserInterface::Draw(Context&) — resolve by name
    fn_UI_Draw = reinterpret_cast<fn_UI_Draw_t>(
        ResolveExport("?Draw@UserInterface@UI@@QEAAXAEAVContext@Video@@@Z"));
    if (!fn_UI_Draw) {
        fn_UI_Draw = reinterpret_cast<fn_UI_Draw_t>(
            ResolveAddr(Module::Runtime, 0x180121890));
    }

    // UI::UserInterface::CloseDialog(InternalPtr<DialogInstance>&) — resolve by name
    fn_UI_CloseDialog = reinterpret_cast<fn_UI_CloseDialog_t>(
        ResolveExport("?CloseDialog@UserInterface@UI@@QEAAXAEAV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z"));
    if (!fn_UI_CloseDialog) {
        fn_UI_CloseDialog = reinterpret_cast<fn_UI_CloseDialog_t>(
            ResolveAddr(Module::Runtime, 0x180120F20));
    }

    // Dynamic array push (generic helper at 0x18007a0d0)
    fn_DynArrayPush = reinterpret_cast<fn_DynArrayPush_t>(
        ResolveAddr(Module::Runtime, 0x18007a0d0));

    // PostDraw/PostUpdate callback array push (16-byte element template instantiations)
    fn_PostDrawPush = reinterpret_cast<fn_DynArrayPush_t>(
        ResolveAddr(Module::Runtime, 0x18004f790));
    fn_PostUpdatePush = reinterpret_cast<fn_DynArrayPush_t>(
        ResolveAddr(Module::Runtime, 0x18004f8e0));

    // Video::Context lifecycle
    fn_VideoCtx_Ctor = reinterpret_cast<fn_VideoCtx_Ctor_t>(
        ResolveAddr(Module::Runtime, 0x1800f1820));
    fn_VideoCtx_Dtor = reinterpret_cast<fn_VideoCtx_Dtor_t>(
        ResolveExport("??1Context@Video@@QEAA@XZ"));
    if (!fn_VideoCtx_Dtor) {
        fn_VideoCtx_Dtor = reinterpret_cast<fn_VideoCtx_Dtor_t>(
            ResolveAddr(Module::Runtime, 0x1800f16e0));
    }
    fn_VideoCtx_Delete = reinterpret_cast<fn_VideoCtx_Delete_t>(
        ResolveAddr(Module::Runtime, 0x180018500));

    // MemoryAllocator
    g_pMemAllocSingleton = reinterpret_cast<void*>(
        *reinterpret_cast<void**>(ResolveAddr(Module::Runtime, 0x1801fc718)));
    fn_MemAlloc = reinterpret_cast<fn_MemAlloc_t>(
        ResolveAddr(Module::Runtime, 0x1800c6e60));

    // Video::Device::_singleton
    g_pVideoDeviceSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Runtime, 0x1801fe7a8));

    // Video::Device Lock/Unlock (ordinals 3214 / 4462)
    fn_VideoDevice_Lock = reinterpret_cast<fn_VideoDevice_Lock_t>(
        ResolveOrdinal(3214));
    fn_VideoDevice_Unlock = reinterpret_cast<fn_VideoDevice_Unlock_t>(
        ResolveOrdinal(4462));

    // --- String functions ---
    fn_String_Set = reinterpret_cast<fn_String_Set_t>(
        ResolveExport("?Set@String@System@@QEAAXPEBD@Z"));
    if (!fn_String_Set) {
        fn_String_Set = reinterpret_cast<fn_String_Set_t>(
            ResolveAddr(Module::Runtime, 0x1800c95d0));
    }

    fn_String_Format = reinterpret_cast<fn_String_Format_t>(
        ResolveExport("?Format@String@System@@SA?AV12@PEBDZZ"));
    if (!fn_String_Format) {
        fn_String_Format = reinterpret_cast<fn_String_Format_t>(
            ResolveAddr(Module::Runtime, 0x1800c9730));
    }

    fn_String_Dtor = reinterpret_cast<fn_String_Dtor_t>(
        ResolveExport("??1String@System@@QEAA@XZ"));
    if (!fn_String_Dtor) {
        fn_String_Dtor = reinterpret_cast<fn_String_Dtor_t>(
            ResolveAddr(Module::Runtime, 0x180006c80));
    }

    // String::operator< — lexicographic comparison
    fn_String_LessThan = reinterpret_cast<fn_String_LessThan_t>(
        ResolveExport("??MString@System@@QEBA_NAEBV01@@Z"));
    if (!fn_String_LessThan) {
        fn_String_LessThan = reinterpret_cast<fn_String_LessThan_t>(
            ResolveAddr(Module::Runtime, 0x180029360));
    }

    // String::String() — default ctor
    fn_String_Ctor_Default = reinterpret_cast<fn_String_Ctor_Default_t>(
        ResolveExport("??0String@System@@QEAA@XZ"));
    if (!fn_String_Ctor_Default) {
        fn_String_Ctor_Default = reinterpret_cast<fn_String_Ctor_Default_t>(
            ResolveAddr(Module::Runtime, 0x180006760));
    }

    // String::String(const char*) — ctor from C string
    fn_String_Ctor_CStr = reinterpret_cast<fn_String_Ctor_CStr_t>(
        ResolveExport("??0String@System@@QEAA@PEBD@Z"));
    if (!fn_String_Ctor_CStr) {
        fn_String_Ctor_CStr = reinterpret_cast<fn_String_Ctor_CStr_t>(
            ResolveAddr(Module::Runtime, 0x18000b630));
    }

    // String::String(String const&) — copy ctor
    fn_String_Ctor_Copy = reinterpret_cast<fn_String_Ctor_Copy_t>(
        ResolveExport("??0String@System@@QEAA@AEBV01@@Z"));
    if (!fn_String_Ctor_Copy) {
        fn_String_Ctor_Copy = reinterpret_cast<fn_String_Ctor_Copy_t>(
            ResolveAddr(Module::Runtime, 0x180006cb0));
    }

    // --- ExternalFactory ---
    g_pExternalFactorySingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Runtime, 0x1801fc6c8));

    fn_ExternalFactory_FindOrLoad = reinterpret_cast<fn_ExternalFactory_FindOrLoad_t>(
        ResolveExport("?FindOrLoad@ExternalFactory@Object@@QEAAPEBVExternal@2@AEBVFilename@System@@@Z"));
    if (!fn_ExternalFactory_FindOrLoad) {
        fn_ExternalFactory_FindOrLoad = reinterpret_cast<fn_ExternalFactory_FindOrLoad_t>(
            ResolveAddr(Module::Runtime, 0x1800bf320));
    }

    fn_ExternalFactory_Find = reinterpret_cast<fn_ExternalFactory_Find_t>(
        ResolveExport("?Find@ExternalFactory@Object@@QEBAPEBVExternal@2@AEBVFilename@System@@@Z"));
    if (!fn_ExternalFactory_Find) {
        fn_ExternalFactory_Find = reinterpret_cast<fn_ExternalFactory_Find_t>(
            ResolveAddr(Module::Runtime, 0x1800bdeb0));
    }

    fn_ExternalFactory_Clean = reinterpret_cast<fn_ExternalFactory_Clean_t>(
        ResolveExport("?Clean@ExternalFactory@Object@@QEAAXXZ"));
    if (!fn_ExternalFactory_Clean) {
        fn_ExternalFactory_Clean = reinterpret_cast<fn_ExternalFactory_Clean_t>(
            ResolveAddr(Module::Runtime, 0x1800be310));
    }

    fn_IsDerivedFrom = reinterpret_cast<fn_IsDerivedFrom_t>(
        ResolveExport("?IsDerivedFrom@Base@Object@@QEBA_NI@Z"));
    if (!fn_IsDerivedFrom) {
        fn_IsDerivedFrom = reinterpret_cast<fn_IsDerivedFrom_t>(
            ResolveAddr(Module::Runtime, 0x1800c0390));
    }

    // Type ID for External resource (dialog resources)
    g_pExternalTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Runtime, 0x1801faa88));

    fn_Filename_AdjustDelimiters = reinterpret_cast<fn_Filename_AdjustDelimiters_t>(
        ResolveExport("?AdjustDelimiters@Filename@System@@QEAAXXZ"));
    if (!fn_Filename_AdjustDelimiters) {
        fn_Filename_AdjustDelimiters = reinterpret_cast<fn_Filename_AdjustDelimiters_t>(
            ResolveAddr(Module::Runtime, 0x1800c6970));
    }

    fn_UpdateLoop_LoadResources = reinterpret_cast<fn_UpdateLoop_LoadResources_t>(
        ResolveExport("?LoadResources@UpdateLoop@Engine@@QEAAXXZ"));
    if (!fn_UpdateLoop_LoadResources) {
        fn_UpdateLoop_LoadResources = reinterpret_cast<fn_UpdateLoop_LoadResources_t>(
            ResolveAddr(Module::Runtime, 0x1800DD7D0));
    }

    fn_ListCleanup = reinterpret_cast<fn_ListCleanup_t>(
        ResolveAddr(Module::Runtime, 0x180010ab0));

    // --- Mod entry lifecycle (for Modification::Swap) ---
    fn_ModEntry_CopyCtor = reinterpret_cast<fn_ModEntry_CopyCtor_t>(
        ResolveAddr(Module::Runtime, 0x1800101e0));
    fn_ModEntry_Assign = reinterpret_cast<fn_ModEntry_Assign_t>(
        ResolveAddr(Module::Runtime, 0x180026ef0));
    fn_ModEntry_Dtor = reinterpret_cast<fn_ModEntry_Dtor_t>(
        ResolveAddr(Module::Runtime, 0x180011460));

    // --- Map::Find / Map::Insert (AVL tree for Configuration) ---
    fn_Map_Find = reinterpret_cast<fn_Map_Find_t>(
        ResolveAddr(Module::Runtime, 0x18002ace0));
    fn_Map_Insert = reinterpret_cast<fn_Map_Insert_t>(
        ResolveAddr(Module::Runtime, 0x18002a090));

    // --- Configuration parsing helpers (private static methods) ---
    fn_ParseCommand = reinterpret_cast<fn_ParseCommand_t>(
        ResolveAddr(Module::Runtime, 0x1800c4a00));
    fn_ParseValue = reinterpret_cast<fn_ParseValue_t>(
        ResolveAddr(Module::Runtime, 0x1800c4930));

    // --- Persistence ---
    g_pPersistenceSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Runtime, 0x1801fc6b8));

    fn_Persistence_SetBool = reinterpret_cast<fn_Persistence_SetBool_t>(
        ResolveExport("?Set@Persistence@IO@@QEAAXPEBD0_N@Z"));
    if (!fn_Persistence_SetBool) {
        fn_Persistence_SetBool = reinterpret_cast<fn_Persistence_SetBool_t>(
            ResolveAddr(Module::Runtime, 0x1800b8380));
    }

    fn_Persistence_SetInt = reinterpret_cast<fn_Persistence_SetInt_t>(
        ResolveExport("?Set@Persistence@IO@@QEAAXPEBD0H@Z"));
    if (!fn_Persistence_SetInt) {
        fn_Persistence_SetInt = reinterpret_cast<fn_Persistence_SetInt_t>(
            ResolveAddr(Module::Runtime, 0x1800b80c0));
    }

    // --- Filename::GetPathFilename ---
    fn_Filename_GetPathFilename = reinterpret_cast<fn_Filename_GetPathFilename_t>(
        ResolveExport("?GetPathFilename@Filename@System@@QEBA?AV12@_N@Z"));
    if (!fn_Filename_GetPathFilename) {
        fn_Filename_GetPathFilename = reinterpret_cast<fn_Filename_GetPathFilename_t>(
            ResolveAddr(Module::Runtime, 0x1800c6740));
    }

    // --- Array<Mod>::operator= (deep copy with CopyCtor per element) ---
    fn_ModArrayCopy = reinterpret_cast<fn_ModArrayCopy_t>(
        ResolveAddr(Module::Runtime, 0x180010110));

    // --- UI infrastructure ---

    // UI::DialogController::~DialogController — virtual base dtor
    fn_DialogController_Dtor = reinterpret_cast<fn_DialogController_Dtor_t>(
        ResolveExport("??1DialogController@UI@@UEAA@XZ"));
    if (!fn_DialogController_Dtor) {
        fn_DialogController_Dtor = reinterpret_cast<fn_DialogController_Dtor_t>(
            ResolveAddr(Module::Runtime, 0x1800cd2f0));
    }

    // UI::DialogInstance::~DialogInstance — virtual base dtor
    fn_DialogInstance_Dtor = reinterpret_cast<fn_DialogInstance_Dtor_t>(
        ResolveExport("??1DialogInstance@UI@@UEAA@XZ"));
    if (!fn_DialogInstance_Dtor) {
        fn_DialogInstance_Dtor = reinterpret_cast<fn_DialogInstance_Dtor_t>(
            ResolveAddr(Module::Runtime, 0x180114d40));
    }

    // System::MemoryAllocator::MemFree — static, single arg
    fn_MemFreeStatic = reinterpret_cast<fn_MemFreeStatic_t>(
        ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
    if (!fn_MemFreeStatic) {
        fn_MemFreeStatic = reinterpret_cast<fn_MemFreeStatic_t>(
            ResolveAddr(Module::Runtime, 0x1800c7ad0));
    }

    // UI::Element::FindElement — non-trivial return ABI
    fn_FindElement = reinterpret_cast<fn_FindElement_t>(
        ResolveExport("?FindElement@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z"));
    if (!fn_FindElement) {
        fn_FindElement = reinterpret_cast<fn_FindElement_t>(
            ResolveAddr(Module::Runtime, 0x180115650));
    }

    // UI::TextElement::SetParameter
    fn_TextElement_SetParameter = reinterpret_cast<fn_TextElement_SetParameter_t>(
        ResolveExport("?SetParameter@TextElement@UI@@QEAAXHAEBVString@System@@@Z"));
    if (!fn_TextElement_SetParameter) {
        fn_TextElement_SetParameter = reinterpret_cast<fn_TextElement_SetParameter_t>(
            ResolveAddr(Module::Runtime, 0x18011e9b0));
    }

    // UI::Element::GetUserData
    fn_Element_GetUserData = reinterpret_cast<fn_Element_GetUserData_t>(
        ResolveExport("?GetUserData@Element@UI@@QEAAPEAVBase@Object@@XZ"));
    if (!fn_Element_GetUserData) {
        fn_Element_GetUserData = reinterpret_cast<fn_Element_GetUserData_t>(
            ResolveAddr(Module::Runtime, 0x180115550));
    }

    // Object::InternalBasePtr::InternalBasePtr(Internal*)
    fn_InternalBasePtr_Ctor = reinterpret_cast<fn_InternalBasePtr_Ctor_t>(
        ResolveExport("??0InternalBasePtr@Object@@QEAA@PEAVInternal@1@@Z"));
    if (!fn_InternalBasePtr_Ctor) {
        fn_InternalBasePtr_Ctor = reinterpret_cast<fn_InternalBasePtr_Ctor_t>(
            ResolveAddr(Module::Runtime, 0x1800062a0));
    }

    // Input::KeyMap::GetInputForId
    fn_KeyMap_GetInputForId = reinterpret_cast<fn_KeyMap_GetInputForId_t>(
        ResolveExport("?GetInputForId@KeyMap@Input@@QEBAHHH@Z"));
    if (!fn_KeyMap_GetInputForId) {
        fn_KeyMap_GetInputForId = reinterpret_cast<fn_KeyMap_GetInputForId_t>(
            ResolveAddr(Module::Runtime, 0x1800a7930));
    }

    // Input::KeyMap::GetDeviceForId
    fn_KeyMap_GetDeviceForId = reinterpret_cast<fn_KeyMap_GetDeviceForId_t>(
        ResolveExport("?GetDeviceForId@KeyMap@Input@@QEBAHHH@Z"));
    if (!fn_KeyMap_GetDeviceForId) {
        fn_KeyMap_GetDeviceForId = reinterpret_cast<fn_KeyMap_GetDeviceForId_t>(
            ResolveAddr(Module::Runtime, 0x1800a7970));
    }

    // ================================================================
    // Phase A1: UI Widget Functions (Runtime DLL exports)
    // ================================================================

    // --- Element ---
    fn_Element_SetFlagsOnOff = reinterpret_cast<fn_Element_SetFlagsOnOff_t>(
        ResolveExport("?SetFlagsOnOff@Element@UI@@QEAAXII@Z"));
    if (!fn_Element_SetFlagsOnOff)
        fn_Element_SetFlagsOnOff = reinterpret_cast<fn_Element_SetFlagsOnOff_t>(
            ResolveAddr(Module::Runtime, 0x180116160));

    fn_Element_SetElementId = reinterpret_cast<fn_Element_SetElementId_t>(
        ResolveExport("?SetElementId@Element@UI@@QEAAXH@Z"));
    if (!fn_Element_SetElementId)
        fn_Element_SetElementId = reinterpret_cast<fn_Element_SetElementId_t>(
            ResolveAddr(Module::Runtime, 0x180115820));

    fn_Element_SetName = reinterpret_cast<fn_Element_SetName_t>(
        ResolveExport("?SetName@Element@UI@@QEAAXAEBVString@System@@@Z"));
    if (!fn_Element_SetName)
        fn_Element_SetName = reinterpret_cast<fn_Element_SetName_t>(
            ResolveAddr(Module::Runtime, 0x1801154f0));

    fn_Element_SetToolTip = reinterpret_cast<fn_Element_SetToolTip_t>(
        ResolveExport("?SetToolTip@Element@UI@@QEAAXVString@2@@Z"));
    if (!fn_Element_SetToolTip)
        fn_Element_SetToolTip = reinterpret_cast<fn_Element_SetToolTip_t>(
            ResolveAddr(Module::Runtime, 0x180035740));

    fn_Element_SetToolTipString = reinterpret_cast<fn_Element_SetToolTipString_t>(
        ResolveExport("?SetToolTipString@Element@UI@@QEAAXAEBVString@System@@@Z"));
    if (!fn_Element_SetToolTipString)
        fn_Element_SetToolTipString = reinterpret_cast<fn_Element_SetToolTipString_t>(
            ResolveAddr(Module::Runtime, 0x1800356e0));

    fn_Element_Child = reinterpret_cast<fn_Element_Child_t>(
        ResolveExport("?Child@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@H@Z"));
    if (!fn_Element_Child)
        fn_Element_Child = reinterpret_cast<fn_Element_Child_t>(
            ResolveAddr(Module::Runtime, 0x180035540));

    fn_Element_ChildCount = reinterpret_cast<fn_Element_ChildCount_t>(
        ResolveExport("?ChildCount@Element@UI@@QEBAHXZ"));
    if (!fn_Element_ChildCount)
        fn_Element_ChildCount = reinterpret_cast<fn_Element_ChildCount_t>(
            ResolveAddr(Module::Runtime, 0x180035530));

    fn_Element_FindElementType = reinterpret_cast<fn_Element_FindElementType_t>(
        ResolveExport("?FindElementType@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z"));
    if (!fn_Element_FindElementType)
        fn_Element_FindElementType = reinterpret_cast<fn_Element_FindElementType_t>(
            ResolveAddr(Module::Runtime, 0x180115760));

    fn_Element_RemoveChildren = reinterpret_cast<fn_Element_RemoveChildren_t>(
        ResolveExport("?RemoveChildren@Element@UI@@QEAAXXZ"));
    if (!fn_Element_RemoveChildren)
        fn_Element_RemoveChildren = reinterpret_cast<fn_Element_RemoveChildren_t>(
            ResolveAddr(Module::Runtime, 0x180115570));

    fn_Element_AddChild = reinterpret_cast<fn_Element_AddChild_t>(
        ResolveExport("?AddChild@Element@UI@@QEAAXV?$InternalPtr@VElement@UI@@@Object@@@Z"));
    if (!fn_Element_AddChild)
        fn_Element_AddChild = reinterpret_cast<fn_Element_AddChild_t>(
            ResolveAddr(Module::Runtime, 0x180115df0));

    // --- DialogInstance ---
    fn_DialogInstance_FindElement = reinterpret_cast<fn_DialogInstance_FindElement_t>(
        ResolveExport("?FindElement@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z"));
    if (!fn_DialogInstance_FindElement)
        fn_DialogInstance_FindElement = reinterpret_cast<fn_DialogInstance_FindElement_t>(
            ResolveAddr(Module::Runtime, 0x180114c70));

    fn_DialogInstance_GetController = reinterpret_cast<fn_DialogInstance_GetController_t>(
        ResolveExport("?GetController@DialogInstance@UI@@QEBA?AV?$InternalPtr@VDialogController@UI@@@Object@@I@Z"));
    if (!fn_DialogInstance_GetController)
        fn_DialogInstance_GetController = reinterpret_cast<fn_DialogInstance_GetController_t>(
            ResolveAddr(Module::Runtime, 0x180113320));

    fn_DialogInstance_FindElementType = reinterpret_cast<fn_DialogInstance_FindElementType_t>(
        ResolveExport("?FindElementType@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z"));
    if (!fn_DialogInstance_FindElementType)
        fn_DialogInstance_FindElementType = reinterpret_cast<fn_DialogInstance_FindElementType_t>(
            ResolveAddr(Module::Runtime, 0x1801140c0));

    fn_DialogInstance_SetFlagsOnOff = reinterpret_cast<fn_DialogInstance_SetFlagsOnOff_t>(
        ResolveExport("?SetFlagsOnOff@DialogInstance@UI@@QEAAXII@Z"));
    if (!fn_DialogInstance_SetFlagsOnOff)
        fn_DialogInstance_SetFlagsOnOff = reinterpret_cast<fn_DialogInstance_SetFlagsOnOff_t>(
            ResolveAddr(Module::Runtime, 0x180114690));

    // --- ComboElement ---
    fn_ComboElement_AddItem = reinterpret_cast<fn_ComboElement_AddItem_t>(
        ResolveExport("?AddItem@ComboElement@UI@@QEAAHAEBVSprite@2@AEBVString@System@@PEAX@Z"));
    if (!fn_ComboElement_AddItem)
        fn_ComboElement_AddItem = reinterpret_cast<fn_ComboElement_AddItem_t>(
            ResolveAddr(Module::Runtime, 0x1800750b0));

    fn_ComboElement_SetCurrentItem = reinterpret_cast<fn_ComboElement_SetCurrentItem_t>(
        ResolveExport("?SetCurrentItem@ComboElement@UI@@QEAAXH@Z"));
    if (!fn_ComboElement_SetCurrentItem)
        fn_ComboElement_SetCurrentItem = reinterpret_cast<fn_ComboElement_SetCurrentItem_t>(
            ResolveAddr(Module::Runtime, 0x180112540));

    fn_ComboElement_GetItemCount = reinterpret_cast<fn_ComboElement_GetItemCount_t>(
        ResolveExport("?GetItemCount@ComboElement@UI@@QEBAHXZ"));
    if (!fn_ComboElement_GetItemCount)
        fn_ComboElement_GetItemCount = reinterpret_cast<fn_ComboElement_GetItemCount_t>(
            ResolveAddr(Module::Runtime, 0x180075180));

    fn_ComboElement_GetCurrentItem = reinterpret_cast<fn_ComboElement_GetCurrentItem_t>(
        ResolveExport("?GetCurrentItem@ComboElement@UI@@QEBAHXZ"));
    if (!fn_ComboElement_GetCurrentItem)
        fn_ComboElement_GetCurrentItem = reinterpret_cast<fn_ComboElement_GetCurrentItem_t>(
            ResolveAddr(Module::Runtime, 0x180075160));

    fn_ComboElement_GetItemData = reinterpret_cast<fn_ComboElement_GetItemData_t>(
        ResolveExport("?GetItemData@ComboElement@UI@@QEBAPEAXH@Z"));
    if (!fn_ComboElement_GetItemData)
        fn_ComboElement_GetItemData = reinterpret_cast<fn_ComboElement_GetItemData_t>(
            ResolveAddr(Module::Runtime, 0x180075170));

    fn_ComboElement_RemoveAllItems = reinterpret_cast<fn_ComboElement_RemoveAllItems_t>(
        ResolveExport("?RemoveAllItems@ComboElement@UI@@QEAAXXZ"));
    if (!fn_ComboElement_RemoveAllItems)
        fn_ComboElement_RemoveAllItems = reinterpret_cast<fn_ComboElement_RemoveAllItems_t>(
            ResolveAddr(Module::Runtime, 0x180075120));

    // --- ProgressElement ---
    fn_ProgressElement_SetProgressTextParameter = reinterpret_cast<fn_ProgressElement_SetProgressTextParameter_t>(
        ResolveExport("?SetProgressTextParameter@ProgressElement@UI@@QEAAXHAEBVString@System@@@Z"));
    if (!fn_ProgressElement_SetProgressTextParameter)
        fn_ProgressElement_SetProgressTextParameter = reinterpret_cast<fn_ProgressElement_SetProgressTextParameter_t>(
            ResolveAddr(Module::Runtime, 0x180119b30));

    fn_ProgressElement_SetMaximumValue = reinterpret_cast<fn_ProgressElement_SetMaximumValue_t>(
        ResolveExport("?SetMaximumValue@ProgressElement@UI@@QEAAXM@Z"));
    if (!fn_ProgressElement_SetMaximumValue)
        fn_ProgressElement_SetMaximumValue = reinterpret_cast<fn_ProgressElement_SetMaximumValue_t>(
            ResolveAddr(Module::Runtime, 0x180077ec0));

    fn_ProgressElement_SetCurrentValue = reinterpret_cast<fn_ProgressElement_SetCurrentValue_t>(
        ResolveExport("?SetCurrentValue@ProgressElement@UI@@QEAAXM@Z"));
    if (!fn_ProgressElement_SetCurrentValue)
        fn_ProgressElement_SetCurrentValue = reinterpret_cast<fn_ProgressElement_SetCurrentValue_t>(
            ResolveAddr(Module::Runtime, 0x180077ed0));

    // --- TableElement ---
    fn_TableElement_SetSize = reinterpret_cast<fn_TableElement_SetSize_t>(
        ResolveExport("?SetSize@TableElement@UI@@QEAAXHH@Z"));
    if (!fn_TableElement_SetSize)
        fn_TableElement_SetSize = reinterpret_cast<fn_TableElement_SetSize_t>(
            ResolveAddr(Module::Runtime, 0x18011db90));

    fn_TableElement_SetChild3 = reinterpret_cast<fn_TableElement_SetChild3_t>(
        ResolveExport("?SetChild@TableElement@UI@@QEAAXV?$InternalPtr@VElement@UI@@@Object@@HH@Z"));
    if (!fn_TableElement_SetChild3)
        fn_TableElement_SetChild3 = reinterpret_cast<fn_TableElement_SetChild3_t>(
            ResolveAddr(Module::Runtime, 0x18011db50));

    fn_TableElement_SetChild2 = reinterpret_cast<fn_TableElement_SetChild2_t>(
        ResolveExport("?SetChild@TableElement@UI@@QEAAXV?$InternalPtr@VElement@UI@@@Object@@H@Z"));
    if (!fn_TableElement_SetChild2)
        fn_TableElement_SetChild2 = reinterpret_cast<fn_TableElement_SetChild2_t>(
            ResolveAddr(Module::Runtime, 0x18011daa0));

    fn_TableElement_GetRowCount = reinterpret_cast<fn_TableElement_GetRowCount_t>(
        ResolveExport("?GetRowCount@TableElement@UI@@QEBAHXZ"));
    if (!fn_TableElement_GetRowCount)
        fn_TableElement_GetRowCount = reinterpret_cast<fn_TableElement_GetRowCount_t>(
            ResolveAddr(Module::Runtime, 0x180079450));

    fn_TableElement_GetColumnCount = reinterpret_cast<fn_TableElement_GetColumnCount_t>(
        ResolveExport("?GetColumnCount@TableElement@UI@@QEBAHXZ"));
    if (!fn_TableElement_GetColumnCount)
        fn_TableElement_GetColumnCount = reinterpret_cast<fn_TableElement_GetColumnCount_t>(
            ResolveAddr(Module::Runtime, 0x180079460));

    // --- NumberEditElement ---
    fn_NumberEditElement_SetValue = reinterpret_cast<fn_NumberEditElement_SetValue_t>(
        ResolveExport("?SetValue@NumberEditElement@UI@@QEAAXH@Z"));
    if (!fn_NumberEditElement_SetValue)
        fn_NumberEditElement_SetValue = reinterpret_cast<fn_NumberEditElement_SetValue_t>(
            ResolveAddr(Module::Runtime, 0x1800d4bb0));

    fn_NumberEditElement_SetMinMax = reinterpret_cast<fn_NumberEditElement_SetMinMax_t>(
        ResolveExport("?SetMinMax@NumberEditElement@UI@@QEAAXHH@Z"));
    if (!fn_NumberEditElement_SetMinMax)
        fn_NumberEditElement_SetMinMax = reinterpret_cast<fn_NumberEditElement_SetMinMax_t>(
            ResolveAddr(Module::Runtime, 0x1800d4b70));

    fn_NumberEditElement_GetValue = reinterpret_cast<fn_NumberEditElement_GetValue_t>(
        ResolveExport("?GetValue@NumberEditElement@UI@@QEBAHXZ"));
    if (!fn_NumberEditElement_GetValue)
        fn_NumberEditElement_GetValue = reinterpret_cast<fn_NumberEditElement_GetValue_t>(
            ResolveAddr(Module::Runtime, 0x1800d4b50));

    // --- NumberElement ---
    fn_NumberElement_SetValueInt = reinterpret_cast<fn_NumberElement_SetValueInt_t>(
        ResolveExport("?SetValue@NumberElement@UI@@QEAAXH@Z"));
    if (!fn_NumberElement_SetValueInt)
        fn_NumberElement_SetValueInt = reinterpret_cast<fn_NumberElement_SetValueInt_t>(
            ResolveAddr(Module::Runtime, 0x180118d50));

    fn_NumberElement_SetValueFloat = reinterpret_cast<fn_NumberElement_SetValueFloat_t>(
        ResolveExport("?SetValue@NumberElement@UI@@QEAAXM@Z"));
    if (!fn_NumberElement_SetValueFloat)
        fn_NumberElement_SetValueFloat = reinterpret_cast<fn_NumberElement_SetValueFloat_t>(
            ResolveAddr(Module::Runtime, 0x180118db0));

    // --- SpinnerElement ---
    fn_SpinnerElement_SetElementId = reinterpret_cast<fn_SpinnerElement_SetElementId_t>(
        ResolveExport("?SetElementId@SpinnerElement@UI@@QEAAXH@Z"));
    if (!fn_SpinnerElement_SetElementId)
        fn_SpinnerElement_SetElementId = reinterpret_cast<fn_SpinnerElement_SetElementId_t>(
            ResolveAddr(Module::Runtime, 0x1800d7e10));

    fn_SpinnerElement_SetValue = reinterpret_cast<fn_SpinnerElement_SetValue_t>(
        ResolveExport("?SetValue@SpinnerElement@UI@@QEAAXH@Z"));
    if (!fn_SpinnerElement_SetValue)
        fn_SpinnerElement_SetValue = reinterpret_cast<fn_SpinnerElement_SetValue_t>(
            ResolveAddr(Module::Runtime, 0x1800d7dc0));

    fn_SpinnerElement_SetMinMax = reinterpret_cast<fn_SpinnerElement_SetMinMax_t>(
        ResolveExport("?SetMinMax@SpinnerElement@UI@@QEAAXHH@Z"));
    if (!fn_SpinnerElement_SetMinMax)
        fn_SpinnerElement_SetMinMax = reinterpret_cast<fn_SpinnerElement_SetMinMax_t>(
            ResolveAddr(Module::Runtime, 0x1800d7d40));

    fn_SpinnerElement_GetValue = reinterpret_cast<fn_SpinnerElement_GetValue_t>(
        ResolveExport("?GetValue@SpinnerElement@UI@@QEBAHXZ"));
    if (!fn_SpinnerElement_GetValue)
        fn_SpinnerElement_GetValue = reinterpret_cast<fn_SpinnerElement_GetValue_t>(
            ResolveAddr(Module::Runtime, 0x1800d7cf0));

    fn_SpinnerElement_GetMaxValue = reinterpret_cast<fn_SpinnerElement_GetMaxValue_t>(
        ResolveExport("?GetMaxValue@SpinnerElement@UI@@QEBAHXZ"));

    fn_SpinnerElement_SetIncrement = reinterpret_cast<fn_SpinnerElement_SetIncrement_t>(
        ResolveExport("?SetIncrement@SpinnerElement@UI@@QEAAXH@Z"));

    fn_SpinnerElement_IsActive = reinterpret_cast<fn_SpinnerElement_IsActive_t>(
        ResolveExport("?IsActive@SpinnerElement@UI@@QEBA_NXZ"));

    // --- PageElement ---
    fn_PageElement_GetActivePage = reinterpret_cast<fn_PageElement_GetActivePage_t>(
        ResolveExport("?GetActivePage@PageElement@UI@@QEBAHXZ"));

    fn_PageElement_SetActivePage = reinterpret_cast<fn_PageElement_SetActivePage_t>(
        ResolveExport("?SetActivePage@PageElement@UI@@QEAAXH@Z"));

    // --- ImageElement ---
    fn_ImageElement_SetSprite = reinterpret_cast<fn_ImageElement_SetSprite_t>(
        ResolveExport("?SetSprite@ImageElement@UI@@QEAAXVSprite@2@@Z"));
    if (!fn_ImageElement_SetSprite)
        fn_ImageElement_SetSprite = reinterpret_cast<fn_ImageElement_SetSprite_t>(
            ResolveAddr(Module::Runtime, 0x180118600));

    fn_ImageElement_SetWidthHeight = reinterpret_cast<fn_ImageElement_SetWidthHeight_t>(
        ResolveExport("?SetWidthHeight@ImageElement@UI@@QEAAXHH@Z"));
    if (!fn_ImageElement_SetWidthHeight)
        fn_ImageElement_SetWidthHeight = reinterpret_cast<fn_ImageElement_SetWidthHeight_t>(
            ResolveAddr(Module::Runtime, 0x180118260));

    // --- LabelElement ---
    fn_LabelElement_SetSprite = reinterpret_cast<fn_LabelElement_SetSprite_t>(
        ResolveExport("?SetSprite@LabelElement@UI@@QEAAXVSprite@2@@Z"));
    if (!fn_LabelElement_SetSprite)
        fn_LabelElement_SetSprite = reinterpret_cast<fn_LabelElement_SetSprite_t>(
            ResolveAddr(Module::Runtime, 0x180042980));

    // --- TextElement ---
    fn_TextElement_SetText = reinterpret_cast<fn_TextElement_SetText_t>(
        ResolveExport("?SetText@TextElement@UI@@QEAAXVString@2@@Z"));
    if (!fn_TextElement_SetText)
        fn_TextElement_SetText = reinterpret_cast<fn_TextElement_SetText_t>(
            ResolveAddr(Module::Runtime, 0x180042070));

    // --- UI::Dialog ---
    {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401528c0));
        if (d && *d) fn_Dialog_CreateElement = reinterpret_cast<fn_Dialog_CreateElement_t>(*d);
    }

    // --- GetInputKeyName (exe 0x140099c20) ---
    fn_GetInputKeyName = reinterpret_cast<fn_GetInputKeyName_t>(
        ResolveAddr(Module::Exe, 0x140099c20));

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
    g_TypeId_BuildComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7318));
    g_TypeId_ConsumeProduceComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7388));
    g_TypeId_RadiusComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4870));
    g_TypeId_BuildRequirementComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7f58));
    g_TypeId_ResourceComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7040));
    g_TypeId_RawMaterialDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7cb8));
    g_TypeId_DialogController = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c56e0));
    g_TypeId_PageToggleUI = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4f00));
    g_TypeId_MeshInstanceComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7e40));
    g_TypeId_DestroyComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7628));
    g_TypeId_WorkplaceComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c80e0));
    g_TypeId_StorageLocationComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c8070));
    g_TypeId_WorkplaceJobType = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c80a8));
    g_TypeId_AttendanceSubComp = reinterpret_cast<uint32_t*>(
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
    // g_TypeId_MapComponent = g_TypeId_BuildComponent_Slot4 (same addr, already resolved above)
    g_TypeId_ModelComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7a50));
    // g_TypeId_MultiObjectComponent already resolved in Phase E below
    g_TypeId_PickingComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7c10));
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

    // DragPlacementTool/FencedTool function pointers
    fn_DragTool_OnKey = reinterpret_cast<fn_DragTool_OnKey_t>(
        ResolveAddr(Module::Exe, 0x1400fc6b0));  // DragTool::OnKey
    fn_DragTool_Shutdown = reinterpret_cast<fn_DragTool_Shutdown_t>(
        ResolveAddr(Module::Exe, 0x1400fc640));  // DragTool::Shutdown (Slot9)

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
            g_TypeId_UIElementBase = reinterpret_cast<uint32_t*>(*exref);
        }
    }
    // Object::Internal type ID: exref at 0x140151660 (used by BuildRequirementUI::Slot4)
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151660));
        if (exref && *exref) {
            g_TypeId_ObjectInternal = reinterpret_cast<uint32_t*>(*exref);
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
    g_pKeyMapObjectExref = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140151738));
    // Engine singleton: exe exref at 0x140151440 → pointer to Runtime var. Dereference once.
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151440));
        g_pEngineSingleton = exref ? reinterpret_cast<void**>(*exref) : nullptr;
    }

    // ================================================================
    // Phase A4: FindComponent type IDs (resolved from exe .rdata)
    // The old exe function pointers (fn_FindComponent_X) are eliminated.
    // All FindComponent variants are now inline C++ in engine_funcs.h.
    // fn_ExeArrayResize/2/3/Push are now pure C++ -- no exe resolution needed.
    // ================================================================
    g_TypeId_BuildComponent_Slot4     = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c79e0));
    g_TypeId_BuildStateComponent      = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7d98));
    g_TypeId_StateMachineComponent_FC = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7350));
    g_TypeId_PositionComponent        = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7008));
    g_TypeId_DestroyComponent_FC      = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c75f0));
    g_TypeId_BuildingComponent        = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4b80));
    g_TypeId_AttendanceComponent      = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c3f40));
    g_TypeId_PastureComponent         = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7ba0));
    g_TypeId_AnimalDescription        = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c6fd0));
    g_TypeId_OrchardComponent         = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c46b0));
    g_TypeId_SMComponent              = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c71c8));
    g_TypeId_CitizenAIComponent_FC    = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7158));
    g_TypeId_StorageComponent_FC      = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7e78));
    g_TypeId_ResidenceComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7cf0));
    g_TypeId_CemeteryComponent        = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4090));
    g_TypeId_TombstoneComponent       = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4a30));
    g_TypeId_ImmigrationComponent     = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4480));
    g_TypeId_TrackerComponent         = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4b10));
    g_TypeId_CharacterComponent       = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7a50));

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
    fn_ImageBuffer_Ctor = reinterpret_cast<fn_ImageBuffer_Ctor_t>(
        ResolveExport("??0ImageBuffer@Video@@QEAA@XZ"));
    if (!fn_ImageBuffer_Ctor)
        fn_ImageBuffer_Ctor = reinterpret_cast<fn_ImageBuffer_Ctor_t>(
            ResolveAddr(Module::Runtime, 0x1800f2580));

    fn_ImageBuffer_Dtor = reinterpret_cast<fn_ImageBuffer_Dtor_t>(
        ResolveExport("??1ImageBuffer@Video@@UEAA@XZ"));
    if (!fn_ImageBuffer_Dtor)
        fn_ImageBuffer_Dtor = reinterpret_cast<fn_ImageBuffer_Dtor_t>(
            ResolveAddr(Module::Runtime, 0x1800f2610));

    fn_ImageBuffer_CopyToTexture = reinterpret_cast<fn_ImageBuffer_CopyToTexture_t>(
        ResolveExport("?CopyToTexture@ImageBuffer@Video@@QEAA_NPEAXHH@Z"));
    if (!fn_ImageBuffer_CopyToTexture)
        fn_ImageBuffer_CopyToTexture = reinterpret_cast<fn_ImageBuffer_CopyToTexture_t>(
            ResolveAddr(Module::Runtime, 0x1800f3380));

    fn_ImageBuffer_CopyFromTexture = reinterpret_cast<fn_ImageBuffer_CopyFromTexture_t>(
        ResolveExport("?CopyFromTexture@ImageBuffer@Video@@QEAA_NPEAXHH@Z"));
    if (!fn_ImageBuffer_CopyFromTexture)
        fn_ImageBuffer_CopyFromTexture = reinterpret_cast<fn_ImageBuffer_CopyFromTexture_t>(
            ResolveAddr(Module::Runtime, 0x1800f33a0));

    // --- Phase F: Dialog/UI Runtime functions ---
    fn_ImageElement_SetRotation = reinterpret_cast<fn_ImageElement_SetRotation_t>(
        ResolveExport("?SetRotation@ImageElement@UI@@QEAAXM@Z"));
    if (!fn_ImageElement_SetRotation)
        fn_ImageElement_SetRotation = reinterpret_cast<fn_ImageElement_SetRotation_t>(
            ResolveAddr(Module::Runtime, 0x1801182c0));

    fn_Time_AsSeconds = reinterpret_cast<fn_Time_AsSeconds_t>(
        ResolveExport("?AsSeconds@Time@System@@QEBAMXZ"));
    if (!fn_Time_AsSeconds)
        fn_Time_AsSeconds = reinterpret_cast<fn_Time_AsSeconds_t>(
            ResolveAddr(Module::Runtime, 0x180007900));

    fn_SaveLoad_Dtor = reinterpret_cast<fn_SaveLoad_Dtor_t>(
        ResolveExport("??1SaveLoad@Save@@QEAA@XZ"));
    if (!fn_SaveLoad_Dtor)
        fn_SaveLoad_Dtor = reinterpret_cast<fn_SaveLoad_Dtor_t>(
            ResolveAddr(Module::Runtime, 0x1800c3c90));

    fn_SaveLoad_GetOperation = reinterpret_cast<fn_SaveLoad_GetOperation_t>(
        ResolveExport("?GetOperation@SaveLoad@Save@@QEBA?AW4Operation@12@XZ"));
    if (!fn_SaveLoad_GetOperation)
        fn_SaveLoad_GetOperation = reinterpret_cast<fn_SaveLoad_GetOperation_t>(
            ResolveAddr(Module::Runtime, 0x1800c3c80));

    // SaveLoad singleton: exe exref at 0x140151610 -> pointer to _singleton in Runtime
    {
        void*** exref = reinterpret_cast<void***>(ResolveAddr(Module::Exe, 0x140151610));
        if (exref && *exref) {
            g_pSaveLoadSingleton = *exref;
        }
    }

    fn_SaveLoad_SetSaveSprite = reinterpret_cast<fn_SaveLoad_SetSaveSprite_t>(
        ResolveExport("?SetSaveSprite@SaveLoad@Save@@QEAAXVSprite@UI@@@Z"));

    fn_SaveLoad_SetMinOperationTime = reinterpret_cast<fn_SaveLoad_SetMinOperationTime_t>(
        ResolveExport("?SetMinOperationTime@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z"));

    fn_SaveLoad_SetOperation = reinterpret_cast<fn_SaveLoad_SetOperation_t>(
        ResolveExport("?SetOperation@SaveLoad@Save@@QEAAXW4Operation@12@H@Z"));
    if (!fn_SaveLoad_SetOperation) {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151658));
        if (d && *d) fn_SaveLoad_SetOperation = reinterpret_cast<fn_SaveLoad_SetOperation_t>(*d);
    }

    fn_SaveLoad_GetSaveCount = reinterpret_cast<fn_SaveLoad_GetSaveCount_t>(
        ResolveExport("?GetSaveCount@SaveLoad@Save@@QEBAHXZ"));

    fn_SaveLoad_GetSaveInfo = reinterpret_cast<fn_SaveLoad_GetSaveInfo_t>(
        ResolveExport("?GetSaveInfo@SaveLoad@Save@@QEBAAEBUSaveInfo@12@H@Z"));
    fn_SaveLoad_GetSaveState = reinterpret_cast<fn_SaveLoad_GetSaveState_t>(
        ResolveExport("?GetSaveState@SaveLoad@Save@@QEAAPEAVSavedState@2@XZ"));
    if (!fn_SaveLoad_GetSaveState) {
        fn_SaveLoad_GetSaveState = reinterpret_cast<fn_SaveLoad_GetSaveState_t>(
            ResolveAddr(Module::Runtime, 0x1800c3b50));
    }

    fn_Sprite_IsValid = reinterpret_cast<fn_Sprite_IsValid_t>(
        ResolveExport("?IsValid@Sprite@UI@@QEBA_NXZ"));

    fn_UI_Play = reinterpret_cast<fn_UI_Play_t>(
        ResolveExport("?Play@UserInterface@UI@@QEAAXH@Z"));

    fn_DateTime_AsString = reinterpret_cast<fn_DateTime_AsString_t>(
        ResolveExport("?AsString@DateTime@System@@QEBA?AVString@2@XZ"));

    // SaveLoadDialog::PopulateFileList — REMOVED, now reimplemented in saveloaddialog.cpp

    fn_EditElement_SetText = reinterpret_cast<fn_EditElement_SetText_t>(
        ResolveExport("?SetText@EditElement@UI@@QEAAXVString@2@@Z"));
    if (!fn_EditElement_SetText)
        fn_EditElement_SetText = reinterpret_cast<fn_EditElement_SetText_t>(
            ResolveAddr(Module::Runtime, 0x180042070));

    fn_EditElement_SetText_SysStr = reinterpret_cast<fn_EditElement_SetText_SysStr_t>(
        ResolveExport("?SetText@EditElement@UI@@QEAAXAEBVString@System@@@Z"));
    if (!fn_EditElement_SetText_SysStr)
        fn_EditElement_SetText_SysStr = reinterpret_cast<fn_EditElement_SetText_SysStr_t>(
            ResolveAddr(Module::Runtime, 0x1800d0db0));

    fn_Input_Device_Released = reinterpret_cast<fn_Input_Device_Released_t>(
        ResolveExport("?Released@Device@Input@@QEBA_NW4InputDevice@12@HH@Z"));

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

    fn_ImageElement_GetSprite = reinterpret_cast<fn_ImageElement_GetSprite_t>(
        ResolveExport("?GetSprite@ImageElement@UI@@QEBA?AVSprite@2@XZ"));
    if (!fn_ImageElement_GetSprite)
        fn_ImageElement_GetSprite = reinterpret_cast<fn_ImageElement_GetSprite_t>(
            ResolveAddr(Module::Runtime, 0x1801182a0));

    fn_LabelElement_GetSprite = reinterpret_cast<fn_LabelElement_GetSprite_t>(
        ResolveExport("?GetSprite@LabelElement@UI@@QEBA?AVSprite@2@XZ"));
    if (!fn_LabelElement_GetSprite)
        fn_LabelElement_GetSprite = reinterpret_cast<fn_LabelElement_GetSprite_t>(
            ResolveAddr(Module::Runtime, 0x1800429d0));

    fn_SpriteSheet_GetSpriteIndexFromName = reinterpret_cast<fn_SpriteSheet_GetSpriteIndexFromName_t>(
        ResolveExport("?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z"));
    if (!fn_SpriteSheet_GetSpriteIndexFromName)
        fn_SpriteSheet_GetSpriteIndexFromName = reinterpret_cast<fn_SpriteSheet_GetSpriteIndexFromName_t>(
            ResolveAddr(Module::Runtime, 0x1800031f0));

    // SliderElement::SetParameter
    fn_SliderElement_SetParameter = reinterpret_cast<fn_SliderElement_SetParameter_t>(
        ResolveExport("?SetParameter@SliderElement@UI@@QEAAXHAEBVString@System@@@Z"));

    // Audio::Scene::Pause — resolved from exe dispatch table (indirect pointer)
    fn_Audio_Scene_Pause = reinterpret_cast<fn_Audio_Scene_Pause_t>(
        ResolveExport("?Pause@Scene@Audio@@QEAAXH_N@Z"));
    if (!fn_Audio_Scene_Pause) {
        uintptr_t dispatchAddr = ResolveAddr(Module::Exe, 0x140152030);
        if (dispatchAddr)
            fn_Audio_Scene_Pause = *reinterpret_cast<fn_Audio_Scene_Pause_t*>(dispatchAddr);
    }

    // Audio::Scene::Update — Runtime 0x1800df6c0. Per-frame channel advance
    // (music/ambient/3D). Resolve by mangled export, fall back to direct Runtime
    // address (the exe dispatch slot 0x140152078 holds the same export).
    fn_Audio_Scene_Update = reinterpret_cast<fn_Audio_Scene_Update_t>(
        ResolveExport("?Update@Scene@Audio@@QEAA_NVTime@System@@0AEAVRandom@4@AEBVVector3@Math@@M@Z"));
    if (!fn_Audio_Scene_Update) {
        fn_Audio_Scene_Update = reinterpret_cast<fn_Audio_Scene_Update_t>(
            ResolveAddr(Module::Runtime, 0x1800df6c0));
    }

    // ScenarioDialog helper — fn_ScenarioDialog_LoadSelected REMOVED, now reimplemented
    // New helpers for reimplemented ScenarioDialog_LoadSelected:
    fn_SaveLoad_Update = reinterpret_cast<fn_SaveLoad_Update_t>(
        ResolveExport("?Update@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z"));
    if (!fn_SaveLoad_Update) {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151d20));
        if (d && *d) fn_SaveLoad_Update = reinterpret_cast<fn_SaveLoad_Update_t>(*d);
    }
    fn_Persistence_GetSavePath = reinterpret_cast<fn_Persistence_GetSavePath_t>(
        ResolveExport("?GetSavePath@Persistence@IO@@QEAA?AVFilename@System@@XZ"));
    if (!fn_Persistence_GetSavePath) {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151d38));
        if (d && *d) fn_Persistence_GetSavePath = reinterpret_cast<fn_Persistence_GetSavePath_t>(*d);
    }
    fn_FileSystem_GetDataDirectory = reinterpret_cast<fn_FileSystem_GetDataDirectory_t>(
        ResolveExport("?GetDataDirectory@FileSystem@IO@@QEBAAEBVString@System@@XZ"));
    if (!fn_FileSystem_GetDataDirectory) {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151d30));
        if (d && *d) fn_FileSystem_GetDataDirectory = reinterpret_cast<fn_FileSystem_GetDataDirectory_t>(*d);
    }
    // FileSystem singleton: exe dispatch at 0x140151d18 → pointer to Runtime singleton.
    // Original: MOV RAX,[0x140151d18]; MOV RCX,[RAX] (at 0x140077d5a)
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151d18));
        g_pFileSystemSingleton2 = exref ? reinterpret_cast<void**>(*exref) : nullptr;
    }
    fn_LoadScenarioFromSave = reinterpret_cast<fn_LoadScenarioFromSave_t>(
        ResolveAddr(Module::Exe, 0x1400b49f0));

    // ModDetailsDialog/ModUpdateDialog vtable — exe .rdata pointers
    g_ModUpdateDialog_Vtable = ResolveAddr(Module::Exe, 0x14015c6c0);

    // SelectResourceUI helpers — exe functions (shared by Orchard/Pasture)
    fn_SelectResourceUI_GetResourceCount = reinterpret_cast<fn_SelectResourceUI_GetResourceCount_t>(
        ResolveAddr(Module::Exe, 0x140078a40));
    fn_SelectResourceUI_PopulateCombo = reinterpret_cast<fn_SelectResourceUI_PopulateCombo_t>(
        ResolveAddr(Module::Exe, 0x140078830));

    // Achievement::GoalList / GoalInstance (Runtime DLL exports)
    fn_GoalList_GetGoal = reinterpret_cast<fn_GoalList_GetGoal_t>(
        ResolveExport("?GetGoal@GoalList@Achievement@@QEBA?AV?$InternalPtr@VGoalInstance@Achievement@@@Object@@H@Z"));
    if (!fn_GoalList_GetGoal)
        fn_GoalList_GetGoal = reinterpret_cast<fn_GoalList_GetGoal_t>(
            ResolveAddr(Module::Runtime, 0x180001710));

    fn_GoalInstance_IsMet = reinterpret_cast<fn_GoalInstance_IsMet_t>(
        ResolveExport("?IsMet@GoalInstance@Achievement@@QEBA_NXZ"));
    if (!fn_GoalInstance_IsMet)
        fn_GoalInstance_IsMet = reinterpret_cast<fn_GoalInstance_IsMet_t>(
            ResolveAddr(Module::Runtime, 0x18009eed0));

    fn_GoalInstance_HasFailed = reinterpret_cast<fn_GoalInstance_HasFailed_t>(
        ResolveExport("?HasFailed@GoalInstance@Achievement@@QEBA_NXZ"));
    if (!fn_GoalInstance_HasFailed)
        fn_GoalInstance_HasFailed = reinterpret_cast<fn_GoalInstance_HasFailed_t>(
            ResolveAddr(Module::Runtime, 0x18009eec0));

    // AchievementManager singleton (double-indirect .rdata dispatch at 0x1401514f0)
    g_ppAchievementManager = reinterpret_cast<void***>(
        ResolveAddr(Module::Exe, 0x1401514f0));

    fn_AchievementManager_IsAchievementAwarded =
        reinterpret_cast<fn_AchievementManager_IsAchievementAwarded_t>(
            ResolveExport("?IsAchievementAwarded@AchievementManager@Achievement@@QEBA_NH@Z"));
    if (!fn_AchievementManager_IsAchievementAwarded)
        fn_AchievementManager_IsAchievementAwarded =
            reinterpret_cast<fn_AchievementManager_IsAchievementAwarded_t>(
                ResolveAddr(Module::Runtime, 0x1800079c0));

    // fn_FindComponent_Character — inline in engine_funcs.h (uses g_TypeId_CharacterComponent)
    // Input::KeyMap::Pressed — from exe dispatch table at 0x140151758
    fn_KeyMap_Pressed = reinterpret_cast<fn_KeyMap_Pressed_t>(
        ResolveExport("?Pressed@KeyMap@Input@@QEBA_NHH@Z"));
    if (!fn_KeyMap_Pressed) {
        uintptr_t dispatchAddr = ResolveAddr(Module::Exe, 0x140151758);
        if (dispatchAddr)
            fn_KeyMap_Pressed = *reinterpret_cast<fn_KeyMap_Pressed_t*>(dispatchAddr);
    }

    // fn_FollowDialog_Close — inlined in followdialog.cpp Slot7 (FUN_140068f00)

    // UpgradeDescription type ID at DAT_1401c5868
    g_TypeId_UpgradeDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5868));

    // DestroyComponent_StartUpgrade — REMOVED, now reimplemented in destroyui.cpp

    // fn_ResidenceUI_GoToCitizen — now inline in residenceui.cpp ResidenceUI_GoToCitizen
    // ResidenceUI_ArrayPush now inline in residenceui.cpp
    // EventLogUI_ToggleNotification and NavigateToEvent now inline in eventlogui.cpp

    // WorkplaceUI_AdjustWorkerCount/ToggleWorkplace: now static impls in workplaceui.cpp
    // fn_FindComponent_BuildState2 — inline in engine_funcs.h (uses g_TypeId_BuildStateComponent)
    fn_WorkplaceComp_SetMaxWorkers = reinterpret_cast<fn_WorkplaceComp_SetMaxWorkers_t>(
        ResolveAddr(Module::Exe, 0x14012fb00));
    fn_CitizenList_RequestWorker = reinterpret_cast<fn_CitizenList_RequestWorker_t>(
        ResolveAddr(Module::Exe, 0x14008d100));
    fn_CitizenList_AssignWorker = reinterpret_cast<fn_CitizenList_AssignWorker_t>(
        ResolveAddr(Module::Exe, 0x14008c3d0));
    fn_CitizenList_RemoveWorker = reinterpret_cast<fn_CitizenList_RemoveWorker_t>(
        ResolveAddr(Module::Exe, 0x140091180));
    // fn_FindComponent_Position — inline in engine_funcs.h (uses g_TypeId_PositionComponent)
    // EditTool type ID — from the exe exref used in EntityUI
    // The original uses _typeInfo_exref which is the same exref as EditTool's type
    // We resolve it from the address seen in EntityUI::Slot7
    g_TypeId_EditTool = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c6dd8));

    // EntityUI helpers — SetEditMode/SelectEntity now reimplemented in entityui.cpp
    // Resolve sub-functions needed by reimplementations:
    fn_Factory_CreateObject = reinterpret_cast<fn_Factory_CreateObject_t>(
        ResolveExport("?CreateObject@Factory@Object@@QEBAPEAVBase@2@I@Z"));
    if (!fn_Factory_CreateObject) {
        fn_Factory_CreateObject = reinterpret_cast<fn_Factory_CreateObject_t>(
            ResolveAddr(Module::Runtime, 0x1800c0ae0));
    }
    g_TypeId_ToolController = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c6eb8));
    g_TypeId_ToolSetting = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c6ef0));
    fn_AVL_Rebalance_0x38 = reinterpret_cast<fn_AVL_Rebalance_0x38_t>(
        ResolveAddr(Module::Exe, 0x140025920));
    // fn_GetDescPage/fn_BuildComplete: now static impls in buildui.cpp

    // Phase F Wave 1: MapParamsDialog / UpgradeUI / ConsumeProduceUI / HelpUI
    // fn_MapParamsDialog_OnOk (0x14006b690) — inlined in mapparamsdialog.cpp
    // fn_MapParamsDialog_OnNewName (0x14006b700) — inlined in mapparamsdialog.cpp
    // fn_GameWorld_GetMapName (0x140011f90) — inlined in mapparamsdialog.cpp
    fn_Element_SetUserData = reinterpret_cast<fn_Element_SetUserData_t>(
        ResolveAddr(Module::Runtime, 0x180115560));
    // fn_UpgradeUI_Serialize (0x140086d90) — inlined in upgradeui.cpp
    // fn_UpgradeUI_OnRemove (0x140086c90) — inlined in upgradeui.cpp
    fn_ConsumeProduceUI_Slot4 = reinterpret_cast<fn_ConsumeProduceUI_Slot4_t>(
        ResolveAddr(Module::Exe, 0x14005F3D0));
    // HelpUI_Slot4 and HelpUI_Slot9 now inline in helpui.cpp

    // Phase F Wave 2A: GoalDialog / EventNotifyDialog / ScenarioDialog
    fn_GoalList_GetGoalCount = reinterpret_cast<fn_GoalList_GetGoalCount_t>(
        ResolveExport("?GetGoalCount@GoalList@Achievement@@QEBAHXZ"));
    fn_Dialog_GetTemplate = reinterpret_cast<fn_Dialog_GetTemplate_t>(
        ResolveExport("?GetTemplate@Dialog@UI@@QEBA?AV?$ExternalPtr@VElementDescription@UI@@@Object@@H@Z"));
    fn_Goal_GetText = reinterpret_cast<fn_Goal_GetText_t>(
        ResolveExport("?GetText@Goal@Achievement@@QEBA?AVString@System@@XZ"));
    fn_Achievement_GetTitle = reinterpret_cast<fn_Achievement_GetTitle_t>(
        ResolveExport("?GetTitle@Achievement@1@QEBA?AVString@System@@XZ"));
    if (!fn_Achievement_GetTitle) {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151598));
        if (d && *d) fn_Achievement_GetTitle = reinterpret_cast<fn_Achievement_GetTitle_t>(*d);
    }
    fn_Achievement_GetDescription = reinterpret_cast<fn_Achievement_GetDescription_t>(
        ResolveExport("?GetDescription@Achievement@1@QEBA?AVString@System@@XZ"));
    if (!fn_Achievement_GetDescription) {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151590));
        if (d && *d) fn_Achievement_GetDescription = reinterpret_cast<fn_Achievement_GetDescription_t>(*d);
    }
    fn_Achievement_GetSpriteName = reinterpret_cast<fn_Achievement_GetSpriteName_t>(
        ResolveExport("?GetSpriteName@Achievement@1@QEBA?AVString@System@@XZ"));
    if (!fn_Achievement_GetSpriteName) {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401515a0));
        if (d && *d) fn_Achievement_GetSpriteName = reinterpret_cast<fn_Achievement_GetSpriteName_t>(*d);
    }
    fn_AchievementManager_GetAchievementCount = reinterpret_cast<fn_AchievementManager_GetAchievementCount_t>(
        ResolveExport("?GetAchievementCount@AchievementManager@Achievement@@QEBAHXZ"));
    if (!fn_AchievementManager_GetAchievementCount)
        fn_AchievementManager_GetAchievementCount = reinterpret_cast<fn_AchievementManager_GetAchievementCount_t>(
            ResolveAddr(Module::Runtime, 0x1800079b0));
    fn_AchievementManager_GetAchievement = reinterpret_cast<fn_AchievementManager_GetAchievement_t>(
        ResolveExport("?GetAchievement@AchievementManager@Achievement@@QEBA?AV?$ExternalPtr@VAchievement@1@@Object@@H@Z"));
    if (!fn_AchievementManager_GetAchievement)
        fn_AchievementManager_GetAchievement = reinterpret_cast<fn_AchievementManager_GetAchievement_t>(
            ResolveAddr(Module::Runtime, 0x18009e8a0));
    // GoalDialog_ArrayPush, EventNotifyDialog_ArrayPush, ScenarioDialog_ArrayPush
    // now inline in their respective .cpp files
    fn_ExternalList_GetResourceCount = reinterpret_cast<fn_ExternalList_GetResourceCount_t>(
        ResolveExport("?GetResourceCount@ExternalList@Object@@QEBAHXZ"));
    fn_Element_SetPadding = reinterpret_cast<fn_Element_SetPadding_t>(
        ResolveExport("?SetPadding@Element@UI@@QEAAXHHHH@Z"));
    // fn_EventNotifyDialog_ArrayRemove (0x140011670) — inlined as EventNotifyDialog_ArrayRemove_Impl in eventnotifydialog.cpp

    // Scenario type ID: direct uint32_t in .data at 0x1401c5b08
    // (Original Slot4 at 0x140078248: MOV EDX, dword ptr [0x1401c5b08])
    g_TypeId_Scenario = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c5b08));

    fn_Sprite_Ctor = reinterpret_cast<fn_Sprite_Ctor_t>(
        ResolveExport("??0Sprite@UI@@QEAA@V?$ExternalPtr@VSpriteSheet@UI@@@Object@@AEBVString@System@@@Z"));
    if (!fn_Sprite_Ctor) {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401514c8));
        if (d && *d) fn_Sprite_Ctor = reinterpret_cast<fn_Sprite_Ctor_t>(*d);
    }
    fn_UIString_Ctor = reinterpret_cast<fn_UIString_Ctor_t>(
        ResolveExport("??0String@UI@@QEAA@V?$ExternalPtr@VStringTable@UI@@@Object@@AEBV0System@@@Z"));
    if (!fn_UIString_Ctor) {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151300));
        if (d && *d) fn_UIString_Ctor = reinterpret_cast<fn_UIString_Ctor_t>(*d);
    }
    fn_UIString_ToSystemString = reinterpret_cast<fn_UIString_ToSystemString_t>(
        ResolveExport("??BString@UI@@QEBA?AV0System@@XZ"));

    // Phase F Wave 2B: ToolbarDialog
    fn_UI_AddDialog = reinterpret_cast<fn_UI_AddDialog_t>(
        ResolveExport("?AddDialog@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z"));
    if (!fn_UI_AddDialog) {
        fn_UI_AddDialog = reinterpret_cast<fn_UI_AddDialog_t>(
            ResolveAddr(Module::Runtime, 0x180120ee0));
    }
    fn_Element_GetPaddedSize = reinterpret_cast<fn_Element_GetPaddedSize_t>(
        ResolveExport("?GetPaddedSize@Element@UI@@QEAAXAEAH0@Z"));
    if (!fn_Element_GetPaddedSize) {
        fn_Element_GetPaddedSize = reinterpret_cast<fn_Element_GetPaddedSize_t>(
            ResolveAddr(Module::Runtime, 0x180115f10));
    }
    fn_RadioElement_SetActiveRadio = reinterpret_cast<fn_RadioElement_SetActiveRadio_t>(
        ResolveExport("?SetActiveRadio@RadioElement@UI@@QEAAXH@Z"));
    if (!fn_RadioElement_SetActiveRadio) {
        fn_RadioElement_SetActiveRadio = reinterpret_cast<fn_RadioElement_SetActiveRadio_t>(
            ResolveAddr(Module::Runtime, 0x1800d56f0));
    }
    fn_ToolbarDialog_Ctor = reinterpret_cast<fn_ToolbarDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x1400b9910));
    // fn_Toolbar_ShowGroup — must call original (15+ direct callers share state)
    fn_Toolbar_ShowGroup = reinterpret_cast<fn_Toolbar_ShowGroup_t>(
        ResolveAddr(Module::Exe, 0x140102b60));
    fn_Toolbar_Utility = reinterpret_cast<fn_Toolbar_Utility_t>(
        ResolveAddr(Module::Exe, 0x140101a80));
    fn_Toolbar_ToggleAll = reinterpret_cast<fn_Toolbar_ToggleAll_t>(
        ResolveAddr(Module::Exe, 0x140101ce0));
    // Vtable pointers for toolbar-related dialogs
    g_ToolbarDialog_Vtable = ResolveAddr(Module::Exe, 0x14015fa00);
    g_PauseDialog_Vtable = ResolveAddr(Module::Exe, 0x14015f550);
    g_ToolUI_ChildDialog_Vtable = ResolveAddr(Module::Exe, 0x14015f5a0);
    // fn_Factory_CreateObject, g_TypeId_ToolController, g_TypeId_ToolSetting — resolved above (entityui section)
    // Toolbar item sort function
    fn_ToolbarSort = reinterpret_cast<fn_ToolbarSort_t>(
        ResolveAddr(Module::Exe, 0x140021790));
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
    // RadioElement::FindCheckBoxes
    fn_RadioElement_FindCheckBoxes = reinterpret_cast<fn_RadioElement_FindCheckBoxes_t>(
        ResolveExport("?FindCheckBoxes@RadioElement@UI@@QEAAXXZ"));
    if (!fn_RadioElement_FindCheckBoxes) {
        fn_RadioElement_FindCheckBoxes = reinterpret_cast<fn_RadioElement_FindCheckBoxes_t>(
            ResolveAddr(Module::Runtime, 0x1800d5750));
    }
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
    fn_Transform_AsTransform3 = reinterpret_cast<fn_Transform_AsTransform3_t>(
        ResolveAddr(Module::Runtime, 0x18001fcd0));
    fn_Camera_SetCameraToWorld = reinterpret_cast<fn_Camera_SetCameraToWorld_t>(
        ResolveAddr(Module::Runtime, 0x1800feb00));
    // fn_ToolbarDialog_CloseSubDialog — inlined as ToolbarDialog_CloseSubDialog_Impl in toolbardialog.cpp
    // fn_ToolbarDialog_ChangeSpeed — inlined as ToolbarDialog_ChangeSpeed_Impl in toolbardialog.cpp
    // fn_ToolbarDialog_UpdateButtons — REPLACED with inline impl in toolbardialog.cpp
    // ToolbarElement type ID: exref at 0x1401516a8, used in ToolbarDialog::Update/OnEvent for IsDerivedFrom
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401516a8));
        if (exref && *exref) {
            g_TypeId_ToolbarElement = reinterpret_cast<uint32_t*>(*exref);
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
    // ConsumeProduceUI helper: get current product index — inline in engine_funcs.h
    // TutorialGoalDialog helpers
    fn_FindDialogByName = reinterpret_cast<fn_FindDialogByName_t>(
        ResolveAddr(Module::Exe, 0x1400e1600));
    // fn_ResolveExternalByName — REPLACED with inline ResolveExternalByName_Impl in engine_funcs.h
    // Object::External::External — runtime dispatch at 0x140151e98
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151e98));
        if (dispatch && *dispatch) {
            fn_External_External = reinterpret_cast<fn_External_External_t>(*dispatch);
        }
    }
    // Object::External::~External — runtime dispatch at 0x140151ea0
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151ea0));
        if (dispatch && *dispatch) {
            fn_External_Dtor = reinterpret_cast<fn_External_Dtor_t>(*dispatch);
        }
    }

    // Phase F Wave 2C: EntityUI::Slot8, ResidenceUI::Slot7, EventNotifyDialog::Slot7
    fn_KeyMap_GetValue = reinterpret_cast<fn_KeyMap_GetValue_t>(
        ResolveExport("?GetValue@KeyMap@Input@@QEBAFHH@Z"));
    if (!fn_KeyMap_GetValue) {
        fn_KeyMap_GetValue = reinterpret_cast<fn_KeyMap_GetValue_t>(
            ResolveAddr(Module::Runtime, 0x1800a7b60));
    }
    fn_UI_GetFirstContainedDialog = reinterpret_cast<fn_UI_GetFirstContainedDialog_t>(
        ResolveExport("?GetFirstContainedDialog@UserInterface@UI@@QEAA?AV?$InternalPtr@VDialogInstance@UI@@@Object@@HH@Z"));
    if (!fn_UI_GetFirstContainedDialog) {
        fn_UI_GetFirstContainedDialog = reinterpret_cast<fn_UI_GetFirstContainedDialog_t>(
            ResolveAddr(Module::Runtime, 0x18011fb00));
    }
    fn_EditTool_SelectEntity = reinterpret_cast<fn_EditTool_SelectEntity_t>(
        ResolveAddr(Module::Exe, 0x1400fd3b0));
    // fn_FindComponent_Residence — inline in engine_funcs.h (uses g_TypeId_StorageComponent_FC, same as Storage)
    g_pAgeMultiplier = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x140172198));
    g_pNotifyTimerDuration = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x1401721cc));
    fn_Time_Ctor_Float = reinterpret_cast<fn_Time_Ctor_Float_t>(
        ResolveExport("??0Time@System@@QEAA@M@Z"));
    fn_String_SubString = reinterpret_cast<fn_String_SubString_t>(
        ResolveExport("?SubString@String@System@@QEBA?AV12@HH@Z"));
    fn_String_Assign = reinterpret_cast<fn_String_Assign_t>(
        ResolveExport("??4String@System@@QEAAAEAV01@AEBV01@@Z"));
    // UI::String::String(ExternalPtr<StringTable>) — single-arg ctor, dispatch at 0x140151300
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151300));
        if (dispatch && *dispatch) {
            fn_UIString_Ctor_Entry = reinterpret_cast<fn_UIString_Ctor_Entry_t>(*dispatch);
        }
    }
    // UI::String::String() — default ctor, dispatch at 0x140151ca8
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151ca8));
        if (dispatch && *dispatch) {
            fn_UIString_DefaultCtor = reinterpret_cast<fn_UIString_DefaultCtor_t>(*dispatch);
        }
    }
    fn_Sprite_Default_Ctor = reinterpret_cast<fn_Sprite_Default_Ctor_t>(
        ResolveExport("??0Sprite@UI@@QEAA@XZ"));
    fn_Dialog_GetReference = reinterpret_cast<fn_Dialog_GetReference_t>(
        ResolveExport("?GetReference@Dialog@UI@@QEBA?AV?$ExternalPtr@VDialogReference@UI@@@Object@@XZ"));
    fn_Audio_Scene_PlayEffect = reinterpret_cast<fn_Audio_Scene_PlayEffect_t>(
        ResolveExport("?PlayEffect@Scene@Audio@@QEAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@W4SoundType@12@VTime@System@@@Z"));
    if (!fn_Audio_Scene_PlayEffect) {
        fn_Audio_Scene_PlayEffect = reinterpret_cast<fn_Audio_Scene_PlayEffect_t>(
            ResolveAddr(Module::Runtime, 0x1800def60));
    }
    g_EventPopupDialog_Vtable = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x14015bd80));

    // --- HelpUI support functions ---
    fn_StringTable_GetText = reinterpret_cast<fn_StringTable_GetText_t>(
        ResolveExport("?GetText@StringTable@UI@@QEBA?AVString@System@@H@Z"));
    if (!fn_StringTable_GetText)
        fn_StringTable_GetText = reinterpret_cast<fn_StringTable_GetText_t>(
            ResolveAddr(Module::Runtime, 0x180002550));
    fn_DocumentTextElement_GetLink = reinterpret_cast<fn_DocumentTextElement_GetLink_t>(
        ResolveExport("?GetLink@DocumentTextElement@UI@@QEBA?AVString@System@@H@Z"));
    if (!fn_DocumentTextElement_GetLink)
        fn_DocumentTextElement_GetLink = reinterpret_cast<fn_DocumentTextElement_GetLink_t>(
            ResolveAddr(Module::Runtime, 0x18003c0c0));
    fn_DocumentTextElement_SetText_UIString = reinterpret_cast<fn_DocumentTextElement_SetText_UIString_t>(
        ResolveExport("?SetText@DocumentTextElement@UI@@QEAAXVString@2@@Z"));
    if (!fn_DocumentTextElement_SetText_UIString)
        fn_DocumentTextElement_SetText_UIString = reinterpret_cast<fn_DocumentTextElement_SetText_UIString_t>(
            ResolveAddr(Module::Runtime, 0x18003bfd0));
    fn_DocumentTextElement_SetText = reinterpret_cast<fn_DocumentTextElement_SetText_t>(
        ResolveExport("?SetText@DocumentTextElement@UI@@QEAAXAEBVString@System@@@Z"));
    if (!fn_DocumentTextElement_SetText)
        fn_DocumentTextElement_SetText = reinterpret_cast<fn_DocumentTextElement_SetText_t>(
            ResolveAddr(Module::Runtime, 0x18003c010));
    fn_ContainerElement_SetOffsets = reinterpret_cast<fn_ContainerElement_SetOffsets_t>(
        ResolveExport("?SetOffsets@ContainerElement@UI@@QEAAXHH@Z"));
    if (!fn_ContainerElement_SetOffsets)
        fn_ContainerElement_SetOffsets = reinterpret_cast<fn_ContainerElement_SetOffsets_t>(
            ResolveAddr(Module::Runtime, 0x1800cc1e0));
    fn_UIString_Ctor_Index = reinterpret_cast<fn_UIString_Ctor_Index_t>(
        ResolveExport("??0String@UI@@QEAA@V?$ExternalPtr@VStringTable@UI@@@Object@@H@Z"));
    if (!fn_UIString_Ctor_Index)
        fn_UIString_Ctor_Index = reinterpret_cast<fn_UIString_Ctor_Index_t>(
            ResolveAddr(Module::Runtime, 0x18011da20));
    fn_String_Equal = reinterpret_cast<fn_String_Equal_t>(
        ResolveExport("??8String@System@@QEBA_NAEBV01@@Z"));
    // HelpUI_ArrayResize now inline as HelpUI_ArrayResize_Impl in helpui.cpp

    // --- OptionsDialog support — Runtime DLL exports ---
    fn_VideoDevice_ValidateConfiguration = reinterpret_cast<fn_VideoDevice_ValidateConfiguration_t>(
        ResolveExport("?ValidateConfiguration@Device@Video@@QEAAXAEAH0000000@Z"));
    fn_VideoDevice_GetMultisampleModeCount = reinterpret_cast<fn_VideoDevice_GetMultisampleModeCount_t>(
        ResolveExport("?GetMultisampleModeCount@Device@Video@@QEBAHXZ"));
    fn_VideoDevice_IsMultisampleModeSupported = reinterpret_cast<fn_VideoDevice_IsMultisampleModeSupported_t>(
        ResolveExport("?IsMultisampleModeSupported@Device@Video@@QEBA_NH@Z"));

    // --- OptionsDialog exe helpers — all REIMPLEMENTED in code/Game/Dialog/optionsdialog.cpp ---
    // fn_OptionsDialog_PopulateInputBindList — reimplemented
    // fn_OptionsDialog_OnInputBindClick — reimplemented
    fn_VideoDevice_GetInterface = reinterpret_cast<fn_VideoDevice_GetInterface_t>(
        ResolveExport("?GetInterface@Device@Video@@QEBAAEBUGraphicsInterface@12@H@Z"));
    fn_VideoDevice_GetInterfaceCount = reinterpret_cast<fn_VideoDevice_GetInterfaceCount_t>(
        ResolveExport("?GetInterfaceCount@Device@Video@@QEBAHXZ"));
    fn_Sprite_DefaultCtor = reinterpret_cast<fn_Sprite_DefaultCtor_t>(
        ResolveExport("??0Sprite@UI@@QEAA@XZ"));
    fn_StringTable_GetText = reinterpret_cast<fn_StringTable_GetText_t>(
        ResolveExport("?GetText@StringTable@UI@@QEBA?AVString@System@@H@Z"));
    fn_TabElement_SetActiveTab = reinterpret_cast<fn_TabElement_SetActiveTab_t>(
        ResolveExport("?SetActiveTab@TabElement@UI@@QEAAXH@Z"));
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
    fn_ExternalFactory_FindNext = reinterpret_cast<fn_ExternalFactory_FindNext_t>(
        ResolveExport("?FindNext@ExternalFactory@Object@@QEBAPEBVExternal@2@IPEBV32@@Z"));
    if (!fn_ExternalFactory_FindNext) {
        fn_ExternalFactory_FindNext = reinterpret_cast<fn_ExternalFactory_FindNext_t>(
            ResolveAddr(Module::Runtime, 0x1800be9d0));
    }
    // fn_StorageUI_SortItems — reimplemented in storageui.cpp as StorageUI_SortItems()

    // --- MapUI helpers — REIMPLEMENTED in code/Game/Dialog/mapui.cpp ---
    // fn_MapUI_SetupImage — reimplemented as MapUI_SetupImage()
    // fn_MapUI_HandleClick — reimplemented as MapUI_HandleClick()
    fn_MapUI_ProjectCornersToGround = reinterpret_cast<fn_MapUI_ProjectCornersToGround_t>(
        ResolveAddr(Module::Exe, 0x14006c5b0));

    // --- CitizenUI helpers ---
    // fn_FindComponent_CitizenAI — inline in engine_funcs.h (uses g_TypeId_CitizenAIComponent_FC)
    g_TypeId_CitizenDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7190));
    g_TypeId_HappinessComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7e08));
    g_TypeId_ProfessionComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c70e8));
    // fn_FindProfessionComponent — inline in engine_funcs.h (uses g_TypeId_ProfessionComponent)
    g_TypeId_ProfessionDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5a98));
    g_FollowDialog_Vtable = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x14015be00));
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
    fn_External_GetPath = reinterpret_cast<fn_External_GetPath_t>(
        ResolveExport("?GetPath@External@Object@@QEBA?AVFilename@System@@XZ"));
    if (!fn_External_GetPath) {
        fn_External_GetPath = reinterpret_cast<fn_External_GetPath_t>(
            ResolveAddr(Module::Runtime, 0x18006a3e0));
    }
    fn_Filename_GetPathIdentifier = reinterpret_cast<fn_Filename_GetPathIdentifier_t>(
        ResolveExport("?GetPathIdentifer@Filename@System@@QEBA?AVString@2@XZ"));
    if (!fn_Filename_GetPathIdentifier) {
        fn_Filename_GetPathIdentifier = reinterpret_cast<fn_Filename_GetPathIdentifier_t>(
            ResolveAddr(Module::Runtime, 0x1800c6610));
    }
    // BSTRebalance — now static inline in professionui.cpp (BSTRebalance_Impl)
    // fn_ProfessionUI_HandleSpinner — reimplemented in professionui.cpp
    // fn_ProfessionUI_HandleButton — reimplemented in professionui.cpp

    // --- MainMenuDialog support ---
    fn_App_Quit = reinterpret_cast<fn_App_Quit_t>(
        ResolveExport("?Quit@Application@1@QEAAXXZ"));
    if (!fn_App_Quit) {
        fn_App_Quit = reinterpret_cast<fn_App_Quit_t>(
            ResolveAddr(Module::Runtime, 0x18009f4d0));
    }

    fn_Configuration_CurrentVersion = reinterpret_cast<fn_Configuration_CurrentVersion_t>(
        ResolveExport("?CurrentVersion@Configuration@System@@SA?AVString@2@XZ"));
    if (!fn_Configuration_CurrentVersion) {
        fn_Configuration_CurrentVersion = reinterpret_cast<fn_Configuration_CurrentVersion_t>(
            ResolveAddr(Module::Runtime, 0x1800c4f40));
    }

    fn_Configuration_CurrentBuildId = reinterpret_cast<fn_Configuration_CurrentBuildId_t>(
        ResolveExport("?CurrentBuildId@Configuration@System@@SA?AVString@2@XZ"));
    if (!fn_Configuration_CurrentBuildId) {
        fn_Configuration_CurrentBuildId = reinterpret_cast<fn_Configuration_CurrentBuildId_t>(
            ResolveAddr(Module::Runtime, 0x1800c4c60));
    }

    fn_InternalBasePtr_CheckPtr = reinterpret_cast<fn_InternalBasePtr_CheckPtr_t>(
        ResolveExport("?CheckPtr@InternalBasePtr@Object@@QEAAXXZ"));
    if (!fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr = reinterpret_cast<fn_InternalBasePtr_CheckPtr_t>(
            ResolveAddr(Module::Runtime, 0x1800c0290));
    }

    fn_DialogInstance_Ctor = reinterpret_cast<fn_DialogInstance_Ctor_t>(
        ResolveAddr(Module::Runtime, 0x180114870));

    g_MainMenuDialog_BuildIdVtable = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x14015f5a0));

    // MainMenuDialog Slot8 handler functions — REIMPLEMENTED, now resolve dialog ctor ptrs
    fn_NewGameDialog_Ctor = reinterpret_cast<fn_NewGameDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x140071040));
    fn_SaveLoadDialog_Ctor = reinterpret_cast<fn_SaveLoadDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x1400b8270));
    fn_OptionsDialog_Ctor = reinterpret_cast<fn_OptionsDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x140075150));
    fn_ScenarioDialog_Ctor = reinterpret_cast<fn_ScenarioDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x1400786b0));
    fn_MapParamsDialog_Ctor = reinterpret_cast<fn_MapParamsDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x14006bd80));
    fn_ModDialog_Ctor = reinterpret_cast<fn_ModDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x140070030));
    g_AchievementDialog_Vtable = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x14015b020));
    g_GoalDialog_Vtable = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x14015beb0));

    // --- NewGameDialog support ---
    g_pNewGameSettings = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401c3de0));
    fn_Random_Rand = reinterpret_cast<fn_Random_Rand_t>(
        ResolveExport("?Rand@Random@System@@QEAAII@Z"));
    if (!fn_Random_Rand) {
        fn_Random_Rand = reinterpret_cast<fn_Random_Rand_t>(
            ResolveAddr(Module::Runtime, 0x1800c8040));
    }
    fn_Random_RandFloat = reinterpret_cast<fn_Random_RandFloat_t>(
        ResolveExport("?RandFloat@Random@System@@QEAAMXZ"));
    if (!fn_Random_RandFloat) {
        fn_Random_RandFloat = reinterpret_cast<fn_Random_RandFloat_t>(
            ResolveAddr(Module::Runtime, 0x1800c7d40));
    }
    // fn_GenerateTownName — REIMPLEMENTED, now resolve inner helpers
    fn_NameDB_GenerateName = reinterpret_cast<fn_NameDB_GenerateName_t>(
        ResolveAddr(Module::Exe, 0x1400a4b30));
    g_TypeId_NameDatabase = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5a60));
    // fn_String_SubString already resolved above
    fn_String_Equals = reinterpret_cast<fn_String_Equals_t>(
        ResolveExport("??8String@System@@QEBA_NPEBD@Z"));
    // operator+= with non-trivial return — resolve from exe dispatch table 0x140151a18
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151a18));
        if (dispatch) fn_String_Append = reinterpret_cast<fn_String_Append_t>(*dispatch);
    }
    fn_StartNewGame = reinterpret_cast<fn_StartNewGame_t>(
        ResolveAddr(Module::Exe, 0x1400b4540));
    g_TypeId_MapGenerator = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c59b8));
    g_TypeId_ClimateGenerator = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5910));
    g_TypeId_TerrainGenerator = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5b40));

    // --- ToolUI support ---
    fn_DialogInstance_SetUpperLeft = reinterpret_cast<fn_DialogInstance_SetUpperLeft_t>(
        ResolveExport("?SetUpperLeft@DialogInstance@UI@@QEAAXHH_N@Z"));
    if (!fn_DialogInstance_SetUpperLeft) {
        fn_DialogInstance_SetUpperLeft = reinterpret_cast<fn_DialogInstance_SetUpperLeft_t>(
            ResolveAddr(Module::Runtime, 0x180114a60));
    }
    fn_UI_BringDialogToFront = reinterpret_cast<fn_UI_BringDialogToFront_t>(
        ResolveExport("?BringDialogToFront@UserInterface@UI@@QEAAXAEAV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z"));
    if (!fn_UI_BringDialogToFront) {
        fn_UI_BringDialogToFront = reinterpret_cast<fn_UI_BringDialogToFront_t>(
            ResolveAddr(Module::Runtime, 0x180120440));
    }
    // fn_UIDesc_AddControllers — REIMPLEMENTED as UIDesc_AddControllers_Impl()
    g_TypeId_UIDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7f90));
    // TabElement::AddTab — dispatch table at 0x1401529c8
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401529c8));
        if (dispatch) fn_TabElement_AddTab = reinterpret_cast<fn_TabElement_AddTab_t>(*dispatch);
    }
    // DialogInstance::AddController (5-arg, tab) — dispatch table at 0x1401529d0
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401529d0));
        if (dispatch) fn_DialogInstance_AddControllerTab =
            reinterpret_cast<fn_DialogInstance_AddControllerTab_t>(*dispatch);
    }
    // DialogInstance::AddController (6-arg, non-tab) — dispatch table at 0x140151a90
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151a90));
        if (dispatch) fn_DialogInstance_AddController6 =
            reinterpret_cast<fn_DialogInstance_AddController6_t>(*dispatch);
    }

    // --- DragTool support ---
    // KeyMap::Released — dispatch table at 0x1401528f0
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401528f0));
        if (dispatch) fn_KeyMap_Released = reinterpret_cast<fn_KeyMap_Released_t>(*dispatch);
    }
    // DialogInstance::GetRoot — dispatch table at 0x1401527f8
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401527f8));
        if (dispatch) fn_DialogInstance_GetRoot = reinterpret_cast<fn_DialogInstance_GetRoot_t>(*dispatch);
    }
    // Audio::Scene::PlayEffect — dispatch table at 0x140152830
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152830));
        if (dispatch) fn_AudioScene_PlayEffect = reinterpret_cast<fn_AudioScene_PlayEffect_t>(*dispatch);
    }

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
            fn_Transform_FromYawPitchOffset = reinterpret_cast<fn_Transform_FromYawPitchOffset_t>(*dispatch);
        } else {
            fn_Transform_FromYawPitchOffset = reinterpret_cast<fn_Transform_FromYawPitchOffset_t>(
                ResolveExport("?FromYawPitchOffset@Transform@Math@@SA?AV12@AEBVScalar@2@0AEBVVector3@2@@Z"));
        }
    }
    // Graphics::Geometry::SetTransform — IAT dispatch slot 0x1401512e0; same fallback.
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401512e0));
        if (dispatch && *dispatch) {
            fn_Geometry_SetTransform = reinterpret_cast<fn_Geometry_SetTransform_t>(*dispatch);
        } else {
            fn_Geometry_SetTransform = reinterpret_cast<fn_Geometry_SetTransform_t>(
                ResolveExport("?SetTransform@Geometry@Graphics@@QEAAXAEBVTransform@Math@@@Z"));
        }
    }
    // Graphics::MeshInstance::UpdateHierarchy — IAT dispatch slot 0x140152888
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152888));
        if (dispatch) fn_MeshInstance_UpdateHierarchy = reinterpret_cast<fn_MeshInstance_UpdateHierarchy_t>(*dispatch);
    }
    // CharacterComponent_impl_08c0 (0x1401008c0) — tile debug overlay rendering (exe function)
    fn_TileRender = reinterpret_cast<fn_TileRender_t>(
        ResolveAddr(Module::Exe, 0x1401008c0));
    // CharacterComponent_impl_f5d0 (0x14009f5d0) — single-tile terrain validity check (exe function)
    fn_TileValidityCheck = reinterpret_cast<fn_TileValidityCheck_t>(
        ResolveAddr(Module::Exe, 0x14009f5d0));
    // FUN_1400dbf90 — fenced placement parameter computation (exe function)
    fn_FencedPlacementParams = reinterpret_cast<fn_FencedPlacementParams_t>(
        ResolveAddr(Module::Exe, 0x1400dbf90));
    // TradeWork_impl_0d00 (0x1400b0d00) — highlight/overlay removal (exe function)
    fn_HighlightRemove = reinterpret_cast<fn_HighlightRemove_t>(
        ResolveAddr(Module::Exe, 0x1400b0d00));
#ifdef _DEBUG
    printf("[EngineFuncs] Player/Tool APIs resolved:\n");
    printf("  SceneAddObject            = %p\n", (void*)fn_SceneAddObject);
    printf("  SceneRemoveObject         = %p\n", (void*)fn_SceneRemoveObject);
    printf("  Transform_FromYawPitch    = %p\n", (void*)fn_Transform_FromYawPitchOffset);
    printf("  Geometry_SetTransform     = %p\n", (void*)fn_Geometry_SetTransform);
    printf("  MeshInstance_UpdateHier   = %p\n", (void*)fn_MeshInstance_UpdateHierarchy);
    printf("  TileRender                = %p\n", (void*)fn_TileRender);
    printf("  TileValidityCheck         = %p\n", (void*)fn_TileValidityCheck);
    printf("  FencedPlacementParams     = %p\n", (void*)fn_FencedPlacementParams);
    printf("  HighlightRemove           = %p\n", (void*)fn_HighlightRemove);
#endif

    // --- ModDetailsDialog support ---
    fn_Element_Hide = reinterpret_cast<fn_Element_Hide_t>(
        ResolveExport("?Hide@Element@UI@@QEAAX_N@Z"));
    fn_Modification_Find = reinterpret_cast<fn_Modification_Find_t>(
        ResolveExport("?Find@Modification@IO@@QEBAHAEBVFilename@System@@@Z"));
    fn_Configuration_GetVersionString = reinterpret_cast<fn_Configuration_GetVersionString_t>(
        ResolveExport("?GetVersionString@Configuration@System@@SA?AVString@2@I_N@Z"));

    // String::operator+= — non-exported, resolve from exe dispatch table at 0x140151a00
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151a00));
        if (dispatch) fn_String_AppendStr = reinterpret_cast<fn_String_AppendStr_t>(*dispatch);
    }
    // System::operator+ — non-exported, resolve from exe dispatch table at 0x140151a08
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151a08));
        if (dispatch) fn_String_Concat = reinterpret_cast<fn_String_Concat_t>(*dispatch);
    }
    // Filename::operator< — non-exported, resolve from exe dispatch table at 0x140151a20
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151a20));
        if (dispatch) fn_Filename_Less = reinterpret_cast<fn_Filename_Less_t>(*dispatch);
    }
    // ModDetails_StringArrayPush — now static inline in moddetailsdialog.cpp

    // --- IO::Modification ---
    // Modification singleton: exe exref at 0x1401519e8 contains a pointer to
    // IO::Modification::_singleton (in Runtime DLL). Dereference once at init.
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401519e8));
        g_ppModificationSingleton = exref ? reinterpret_cast<void**>(*exref) : nullptr;
    }
    fn_Modification_GetCount = reinterpret_cast<fn_Modification_GetCount_t>(
        ResolveAddr(Module::Runtime, 0x18000da90));
    fn_Modification_GetMod = reinterpret_cast<fn_Modification_GetMod_t>(
        ResolveAddr(Module::Runtime, 0x18000da80));
    fn_Modification_Swap = reinterpret_cast<fn_Modification_Swap_t>(
        ResolveAddr(Module::Runtime, 0x1800aeb20));
    fn_Modification_Enable = reinterpret_cast<fn_Modification_Enable_t>(
        ResolveAddr(Module::Runtime, 0x1800aeab0));
    fn_Modification_RefreshList = reinterpret_cast<fn_Modification_RefreshList_t>(
        ResolveAddr(Module::Runtime, 0x1800af5f0));
    fn_Modification_SaveList = reinterpret_cast<fn_Modification_SaveList_t>(
        ResolveAddr(Module::Runtime, 0x1800b05f0));

    // --- DialogInstance::OnElementEvent (exe dispatch table) ---
    fn_DialogInstance_OnElementEvent = reinterpret_cast<fn_DialogInstance_OnElementEvent_t>(
        ResolveExport("?OnElementEvent@DialogInstance@UI@@UEAAXAEBVElementEvent@2@@Z"));
    if (!fn_DialogInstance_OnElementEvent) {
        // Fallback: resolve from exe dispatch table pointer
        void** dispatchPtr = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151a30));
        if (dispatchPtr) fn_DialogInstance_OnElementEvent =
            reinterpret_cast<fn_DialogInstance_OnElementEvent_t>(*dispatchPtr);
    }

    fn_DialogInstance_AddController = reinterpret_cast<fn_DialogInstance_AddController_t>(
        ResolveExport("?AddController@DialogInstance@UI@@QEAAXIHHPEBVExternal@Object@@AEBVString@System@@@Z"));
    if (!fn_DialogInstance_AddController) {
        void** dispatchPtr = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151a60));
        if (dispatchPtr) fn_DialogInstance_AddController =
            reinterpret_cast<fn_DialogInstance_AddController_t>(*dispatchPtr);
    }

    // --- DialogInstance::OnUpdateLayout / OnUpdate (dispatch slots 0x1401517b0/0x1401517b8) ---
    // These back the 6-byte JMP-thunks at exe 0x14003a98a (slot +0x30) and
    // 0x14003a990 (slot +0x38). In standalone we wire them directly so the
    // MainMenuDialog vtable (and any other dialog) can dispatch child layout
    // / per-frame updates through the Runtime DLL.
    fn_DialogInstance_OnUpdateLayout = reinterpret_cast<fn_DialogInstance_OnUpdateLayout_t>(
        ResolveExport("?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ"));
    if (!fn_DialogInstance_OnUpdateLayout) {
        fn_DialogInstance_OnUpdateLayout = reinterpret_cast<fn_DialogInstance_OnUpdateLayout_t>(
            ResolveAddr(Module::Runtime, 0x1801153a0));
    }

    fn_DialogInstance_OnUpdate = reinterpret_cast<fn_DialogInstance_OnUpdate_t>(
        ResolveExport("?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z"));
    if (!fn_DialogInstance_OnUpdate) {
        fn_DialogInstance_OnUpdate = reinterpret_cast<fn_DialogInstance_OnUpdate_t>(
            ResolveAddr(Module::Runtime, 0x180115400));
    }

    // --- ModDialog helper functions ---
    // fn_ModDialog_RefreshDisplay — REPLACED with static impl in moddialog.cpp
    // fn_ModDialog_ToggleMod — REPLACED with inline impl in moddialog.cpp
    // fn_ModDialog_MoveToTop — REPLACED with inline impl in moddialog.cpp
    // fn_ModDialog_ShowDetails — REPLACED with inline impl in moddialog.cpp
    // fn_ModDialog_Apply — REPLACED with inline impl in moddialog.cpp
    g_ModDetailsDialog_Vtable = ResolveAddr(Module::Exe, 0x14015c670);
    fn_ModDialog_CloseAndReload = reinterpret_cast<fn_ModDialog_CloseAndReload_t>(
        ResolveAddr(Module::Exe, 0x14006e9b0));
    fn_ModDialog_RefreshDisplay_Orig = reinterpret_cast<fn_ModDialog_RefreshDisplay_Orig_t>(
        ResolveAddr(Module::Exe, 0x14006eec0));
    fn_MainMenu_Reload = reinterpret_cast<fn_MainMenu_Reload_t>(
        ResolveAddr(Module::Exe, 0x1400b4790));
    fn_Game_ReloadWithSave = reinterpret_cast<fn_Game_ReloadWithSave_t>(
        ResolveAddr(Module::Exe, 0x1400b49f0));
    fn_SavedState_Ctor = reinterpret_cast<fn_SavedState_Ctor_t>(
        ResolveExport("??0SavedState@Save@@QEAA@XZ"));
    if (!fn_SavedState_Ctor) {
        // Fallback: SavedState ctor might be in exe
        fn_SavedState_Ctor = reinterpret_cast<fn_SavedState_Ctor_t>(
            ResolveAddr(Module::Runtime, 0x18006f330));
    }
    g_ReloadDialog_Vtable = ResolveAddr(Module::Exe, 0x14015c480);

    // --- IO::Serializer (shared globals) ---
    g_fn_Serializer_Ctor = reinterpret_cast<g_fn_Serializer_Ctor_t>(
        ResolveExport("??0Serializer@IO@@QEAA@AEAVStream@1@H@Z"));
    g_fn_Serializer_Dtor = reinterpret_cast<g_fn_Serializer_Dtor_t>(
        ResolveExport("??1Serializer@IO@@QEAA@XZ"));
    g_fn_Serialize_Int = reinterpret_cast<g_fn_Serialize_Int_t>(
        ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAHPEBD@Z"));
    g_fn_Serialize_Bool = reinterpret_cast<g_fn_Serialize_Bool_t>(
        ResolveExport("?Serialize@Serializer@IO@@QEAAXAEA_NPEBD@Z"));
    g_fn_Serializer_GetVersion = reinterpret_cast<g_fn_Serializer_GetVersion_t>(
        ResolveExport("?GetVersion@Serializer@IO@@QEBAHXZ"));

    // --- TownHallUI support ---
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151de0));
        if (exref && *exref) {
            g_TypeId_TabElement = reinterpret_cast<uint32_t*>(*exref);
        }
    }
    // fn_TownHallUI_InitGeneral — reimplemented in townhallui.cpp
    fn_TownHallUI_InitProfessions = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x140080160));
    fn_TownHallUI_InitProduction = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x14007fc30));
    fn_TownHallUI_InitInventory = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x14007f660));
    // fn_TownHallUI_InitGraph — reimplemented in townhallui.cpp
    fn_TownHallUI_InitAgriculture = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x14007ebf0));
    // fn_TownHallUI_UpdateGraph — reimplemented in townhallui.cpp
    // fn_TownHallUI_UpdateGeneral — reimplemented in townhallui.cpp
    // fn_TownHallUI_UpdateProfessions — reimplemented in townhallui.cpp
    // fn_TownHallUI_UpdateProduction — reimplemented in townhallui.cpp
    // fn_TownHallUI_UpdateAgriculture — reimplemented in townhallui.cpp
    // fn_TownHallUI_UpdateInventory — reimplemented in townhallui.cpp
    // fn_TownHallUI_UpdateAll — reimplemented in townhallui.cpp
    // fn_TownHallUI_OnProfessionButton — reimplemented in townhallui.cpp
    // fn_TownHallUI_OnProfessionSpinner — reimplemented in townhallui.cpp
    g_pGameSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    // --- GroupRating helpers ---
    fn_GroupRating_Init = reinterpret_cast<fn_GroupRating_Init_t>(
        ResolveAddr(Module::Exe, 0x14006abf0));
    fn_GroupRating_Update = reinterpret_cast<fn_GroupRating_Update_t>(
        ResolveAddr(Module::Exe, 0x14006aed0));

    // --- GraphElement functions (Runtime DLL exports) ---
    fn_GraphElement_RemoveGraphs = reinterpret_cast<fn_GraphElement_RemoveGraphs_t>(
        ResolveExport("?RemoveGraphs@GraphElement@UI@@QEAAXXZ"));
    fn_GraphElement_AddGraph = reinterpret_cast<fn_GraphElement_AddGraph_t>(
        ResolveExport("?AddGraph@GraphElement@UI@@QEAAXI@Z"));
    fn_GraphElement_AddGraphPoint = reinterpret_cast<fn_GraphElement_AddGraphPoint_t>(
        ResolveExport("?AddGraphPoint@GraphElement@UI@@QEAAXHH@Z"));

    // --- StringTable::GetCount (Runtime DLL) ---
    fn_StringTable_GetCount = reinterpret_cast<fn_StringTable_GetCount_t>(
        ResolveExport("?GetCount@StringTable@UI@@QEBAHXZ"));

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
    fn_CitizenList_ShuffleEntities = reinterpret_cast<fn_CitizenList_ShuffleEntities_t>(
        ResolveAddr(Module::Exe, 0x140089470));

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
    // fn_FindComponent_Building — inline in engine_funcs.h (uses g_TypeId_BuildingComponent)
    fn_Element_SetNeedsSizeCalc = reinterpret_cast<fn_Element_SetNeedsSizeCalc_t>(
        ResolveExport("?SetNeedsSizeCalc@Element@UI@@QEAAXXZ"));
    fn_TradeUI_RefreshDisplay = reinterpret_cast<fn_TradeUI_RefreshDisplay_t>(
        ResolveAddr(Module::Exe, 0x1400846b0));
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
    fn_DynArray_Clear_0x08 = reinterpret_cast<fn_DynArray_Clear_0x08_t>(
        ResolveAddr(Module::Exe, 0x140001bb0));
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
        if (exref && *exref) g_TypeId_SpinnerElement = reinterpret_cast<uint32_t*>(*exref);
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
        if (exref && *exref) g_TypeId_SliderElement = reinterpret_cast<uint32_t*>(*exref);
    }
    // SelectDescription type ID — DAT_1401c48a8
    g_TypeId_SelectDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c48a8));
    fn_Building_GetSellableCount = reinterpret_cast<fn_Building_GetSellableCount_t>(
        ResolveAddr(Module::Exe, 0x14005a520));
    fn_StorageTree_Insert = reinterpret_cast<fn_StorageTree_Insert_t>(
        ResolveAddr(Module::Exe, 0x14000e1c0));
    fn_Building_RemoveBuyOrder = reinterpret_cast<fn_Building_RemoveBuyOrder_t>(
        ResolveAddr(Module::Exe, 0x14005baa0));
    fn_Building_AddBuyToStorage = reinterpret_cast<fn_Building_AddBuyToStorage_t>(
        ResolveAddr(Module::Exe, 0x14005ace0));
    fn_Building_AddPurchaseOrder = reinterpret_cast<fn_Building_AddPurchaseOrder_t>(
        ResolveAddr(Module::Exe, 0x140059490));

    // --- ModDialog::Slot4 support (Graphics::MaterialInstance, UI::SpriteSheet, etc.) ---
    // MaterialInstance ctor: dispatch at 0x140151838
    {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151838));
        if (d && *d) fn_MaterialInstance_Ctor = reinterpret_cast<fn_MaterialInstance_Ctor_t>(*d);
    }
    // MaterialInstance::SetMaterial: dispatch at 0x140151870
    {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151870));
        if (d && *d) fn_MaterialInstance_SetMaterial = reinterpret_cast<fn_MaterialInstance_SetMaterial_t>(*d);
    }
    // MaterialInstance::AddTexture: dispatch at 0x140151878
    {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151878));
        if (d && *d) fn_MaterialInstance_AddTexture = reinterpret_cast<fn_MaterialInstance_AddTexture_t>(*d);
    }
    // MaterialInstance::BuildParameters: dispatch at 0x140151880
    {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151880));
        if (d && *d) fn_MaterialInstance_BuildParameters = reinterpret_cast<fn_MaterialInstance_BuildParameters_t>(*d);
    }
    // SpriteSheet ctor: dispatch at 0x140151848
    {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151848));
        if (d && *d) fn_SpriteSheet_Ctor = reinterpret_cast<fn_SpriteSheet_Ctor_t>(*d);
    }
    // SpriteSheet::AddSpriteInfo: dispatch at 0x140151978
    {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151978));
        if (d && *d) fn_SpriteSheet_AddSpriteInfo = reinterpret_cast<fn_SpriteSheet_AddSpriteInfo_t>(*d);
    }
    // SpriteSheet::SetSpriteInfo: dispatch at 0x140151970
    {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151970));
        if (d && *d) fn_SpriteSheet_SetSpriteInfo = reinterpret_cast<fn_SpriteSheet_SetSpriteInfo_t>(*d);
    }
    // Sprite ctor by index: dispatch at 0x140151898
    {
        void** d = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151898));
        if (d && *d) fn_Sprite_Ctor_ByIndex = reinterpret_cast<fn_Sprite_Ctor_ByIndex_t>(*d);
    }
    // ImageBuffer::Width / Height — resolved by export name from Runtime
    fn_ImageBuffer_Width = reinterpret_cast<fn_ImageBuffer_Width_t>(
        ResolveExport("?Width@ImageBuffer@Video@@QEBAHXZ"));
    fn_ImageBuffer_Height = reinterpret_cast<fn_ImageBuffer_Height_t>(
        ResolveExport("?Height@ImageBuffer@Video@@QEBAHXZ"));
    // Custom vtables for mod thumbnail objects
    g_ModDialog_MaterialInstanceVtable = ResolveAddr(Module::Exe, 0x14015c400);
    g_ModDialog_SpriteSheetVtable      = ResolveAddr(Module::Exe, 0x14015c3b0);
    // ModDialog array push: FUN_140012a60
    fn_ModDialog_ArrayPush = reinterpret_cast<fn_ModDialog_ArrayPush_t>(
        ResolveAddr(Module::Exe, 0x140012a60));
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
    g_NoiseUI_ImageBufferVtable = ResolveAddr(Module::Exe, 0x14015c8c0);
    // fn_NoiseUI_SetupImage — REIMPLEMENTED in code/Game/Dialog/noiseui.cpp
    // fn_NoiseUI_GenerateNoise — REIMPLEMENTED in code/Game/Dialog/noiseui.cpp
    // NoiseUI runtime dependencies
    fn_PerlinNoise_Ctor = reinterpret_cast<fn_PerlinNoise_Ctor_t>(
        ResolveExport("??0PerlinNoise@Utility@@QEAA@MMHH@Z"));
    fn_PerlinNoise_Generate = reinterpret_cast<fn_PerlinNoise_Generate_t>(
        ResolveExport("?Generate@PerlinNoise@Utility@@QEBAMMM@Z"));
    // Random default ctor — already resolved as fn_Random_Default_Ctor above
    fn_Random_RandTol = reinterpret_cast<fn_Random_RandTol_t>(
        ResolveExport("?RandTol@Random@System@@QEAAHH@Z"));
    fn_String_ToFloat = reinterpret_cast<fn_String_ToFloat_t>(
        ResolveExport("?ToFloat@String@System@@SAMPEBD@Z"));

    // --- DebugUI helpers ---
    // Weather_ResetTime (exe 0x1400944e0)
    fn_Weather_ResetTime = reinterpret_cast<fn_Weather_ResetTime_t>(
        ResolveAddr(Module::Exe, 0x1400944e0));
    // UI::UserInterface::SetCursor — ordinal 3865
    fn_UI_SetCursor = reinterpret_cast<fn_UI_SetCursor_t>(
        ResolveExport("?SetCursor@UserInterface@UI@@QEAAXV?$ExternalPtr@VCursor@UI@@@Object@@@Z"));
    // UI::UserInterface::SetFullscreenFade — ordinal 3887
    fn_UI_SetFullscreenFade = reinterpret_cast<fn_UI_SetFullscreenFade_t>(
        ResolveExport("?SetFullscreenFade@UserInterface@UI@@QEAAX_NI0@Z"));
    fn_DebugUI_SetDayLighting = reinterpret_cast<fn_DebugUI_SetDayLighting_t>(
        ResolveAddr(Module::Exe, 0x140061780));
    // DiseaseDescription type ID (DAT_1401c5948)
    g_TypeId_DiseaseDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5948));
    // DrawableType type ID (DAT_1401c47c8)
    g_TypeId_DrawableType = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c47c8));

    // --- DebugUI Slot9 sub-functions ---
    fn_Game_SpawnResources = reinterpret_cast<fn_Game_SpawnResources_t>(
        ResolveAddr(Module::Exe, 0x140091660));
    fn_Game_TriggerDisasterByType = reinterpret_cast<fn_Game_TriggerDisasterByType_t>(
        ResolveAddr(Module::Exe, 0x140060110));
    fn_Game_DestroyAllRawMaterials = reinterpret_cast<fn_Game_DestroyAllRawMaterials_t>(
        ResolveAddr(Module::Exe, 0x140060000));
    fn_Game_SpawnItemsInStorage = reinterpret_cast<fn_Game_SpawnItemsInStorage_t>(
        ResolveAddr(Module::Exe, 0x140060290));
    fn_Game_SpawnCitizens = reinterpret_cast<fn_Game_SpawnCitizens_t>(
        ResolveAddr(Module::Exe, 0x140060380));
    fn_DebugUI_TriggerDisasterOnBuilding = reinterpret_cast<fn_DebugUI_TriggerDisasterOnBuilding_t>(
        ResolveAddr(Module::Exe, 0x14005FF30));
    fn_Game_StartRandomFire = reinterpret_cast<fn_Game_StartRandomFire_t>(
        ResolveAddr(Module::Exe, 0x14005FEB0));
    fn_Game_SpawnTornado = reinterpret_cast<fn_Game_SpawnTornado_t>(
        ResolveAddr(Module::Exe, 0x14005FD20));
    fn_Game_InfectRandomCitizen = reinterpret_cast<fn_Game_InfectRandomCitizen_t>(
        ResolveAddr(Module::Exe, 0x14005FE10));
    fn_DebugUI_OnDiseaseComboChanged = reinterpret_cast<fn_DebugUI_OnDiseaseComboChanged_t>(
        ResolveAddr(Module::Exe, 0x14005FC60));
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
    fn_Graphics_Scene_AddGlobalObject = reinterpret_cast<fn_Graphics_Scene_AddGlobalObject_t>(
        ResolveExport("?AddGlobalObject@Scene@Graphics@@QEAAXPEAVGeometry@2@@Z"));
    fn_Graphics_Scene_AddObject = reinterpret_cast<fn_Graphics_Scene_AddObject_t>(
        ResolveExport("?AddObject@Scene@Graphics@@QEAAIPEAVGeometry@2@@Z"));
    fn_Graphics_GraphicsMesh_GetMaterialList = reinterpret_cast<fn_Graphics_GraphicsMesh_GetMaterialList_t>(
        ResolveExport("?GetMaterialList@GraphicsMesh@Graphics@@QEBAAEBV?$Array@V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@@Collection@@H@Z"));
    fn_Graphics_MeshInstance_SetMaterialInstance = reinterpret_cast<fn_Graphics_MeshInstance_SetMaterialInstance_t>(
        ResolveExport("?SetMaterialInstance@MeshInstance@Graphics@@QEAAXHV?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@@Z"));
    fn_Graphics_ParticleSystemInstance_Ctor = reinterpret_cast<fn_Graphics_ParticleSystemInstance_Ctor_t>(
        ResolveExport("??0ParticleSystemInstance@Graphics@@QEAA@XZ"));
    fn_Graphics_ParticleInstance_IncrementInstanceIndex = reinterpret_cast<fn_Graphics_ParticleInstance_IncrementInstanceIndex_t>(
        ResolveExport("?IncrementInstanceIndex@ParticleInstance@Graphics@@SAXXZ"));
    fn_Graphics_Particle_Ctor = reinterpret_cast<fn_Graphics_Particle_Ctor_t>(
        ResolveExport("??0Particle@Graphics@@QEAA@XZ"));
    if (!fn_Graphics_Particle_Ctor)  // fallback: resolve by Runtime address (0x180103e80)
        fn_Graphics_Particle_Ctor = reinterpret_cast<fn_Graphics_Particle_Ctor_t>(
            ResolveAddr(Module::Runtime, 0x180103e80));
    fn_Graphics_Scene_Clean = reinterpret_cast<fn_Graphics_Scene_Clean_t>(
        ResolveExport("?Clean@Scene@Graphics@@QEAAXXZ"));
    printf("[EngineFuncs] MenuDLT static-mesh + particle helpers:\n"
           "  Scene::AddObject              = %p\n"
           "  GraphicsMesh::GetMaterialList = %p\n"
           "  MeshInstance::SetMatInstance  = %p\n"
           "  ParticleSystemInstance::Ctor  = %p\n"
           "  ParticleInstance::IncrementInstanceIndex = %p\n"
           "  Particle::Ctor                = %p\n",
           (void*)fn_Graphics_Scene_AddObject,
           (void*)fn_Graphics_GraphicsMesh_GetMaterialList,
           (void*)fn_Graphics_MeshInstance_SetMaterialInstance,
           (void*)fn_Graphics_ParticleSystemInstance_Ctor,
           (void*)fn_Graphics_ParticleInstance_IncrementInstanceIndex,
           (void*)fn_Graphics_Particle_Ctor);
    fflush(stdout);

    // --- MenuScene 3D rendering (Phase A) ---
    fn_Graphics_Renderer_Ctor = reinterpret_cast<fn_Graphics_Renderer_Ctor_t>(
        ResolveExport("??0Renderer@Graphics@@QEAA@XZ"));
    fn_Graphics_Scene_Ctor = reinterpret_cast<fn_Graphics_Scene_Ctor_t>(
        ResolveExport("??0Scene@Graphics@@QEAA@XZ"));
    fn_Graphics_SunLight_Ctor = reinterpret_cast<fn_Graphics_SunLight_Ctor_t>(
        ResolveExport("??0SunLight@Graphics@@QEAA@XZ"));
    fn_Graphics_SunLight_Set = reinterpret_cast<fn_Graphics_SunLight_Set_t>(
        ResolveExport("?Set@SunLight@Graphics@@QEAAXAEBVVector3@Math@@AEBVVector4@4@1@Z"));
    fn_Graphics_MeshInstance_Ctor = reinterpret_cast<fn_Graphics_MeshInstance_Ctor_t>(
        ResolveExport("??0MeshInstance@Graphics@@QEAA@XZ"));
    fn_Graphics_MeshInstance_SetMesh = reinterpret_cast<fn_Graphics_MeshInstance_SetMesh_t>(
        ResolveExport("?SetMesh@MeshInstance@Graphics@@QEAAXV?$ExternalPtr@VGraphicsMesh@Graphics@@@Object@@@Z"));
    fn_Graphics_Renderer_Draw = reinterpret_cast<fn_Graphics_Renderer_Draw_t>(
        ResolveExport("?Draw@Renderer@Graphics@@QEAAXAEAVContext@Video@@MAEBVScene@2@AEBVCamera@2@@Z"));
    fn_Math_Vector3_FromYawPitch = reinterpret_cast<fn_Math_Vector3_FromYawPitch_t>(
        ResolveExport("?FromYawPitch@Vector3@Math@@SA?AV12@AEBVScalar@2@0@Z"));
    fn_Math_Transform_FromLookAt = reinterpret_cast<fn_Math_Transform_FromLookAt_t>(
        ResolveExport("?FromLookAt@Transform@Math@@SA?AV12@AEBVVector3@2@0@Z"));
    fn_Video_Context_SetFog = reinterpret_cast<fn_Video_Context_SetFog_t>(
        ResolveExport("?SetFog@Context@Video@@QEAAXMMAEBVVector4@Math@@@Z"));
    fn_Math_Transform_Ctor = reinterpret_cast<fn_Math_Transform_Ctor_t>(
        ResolveExport("??0Transform@Math@@QEAA@XZ"));
    fn_Math_Transform3_Identity = reinterpret_cast<fn_Math_Transform3_Identity_t>(
        ResolveExport("?Identity@Transform3@Math@@SA?AV12@XZ"));
    fn_Volume_Frustum_Ctor = reinterpret_cast<fn_Volume_Frustum_Ctor_t>(
        ResolveExport("??0Frustum@Volume@@QEAA@AEBVScalar@Math@@000@Z"));
    fn_Graphics_Camera_Ctor = reinterpret_cast<fn_Graphics_Camera_Ctor_t>(
        ResolveExport("??0Camera@Graphics@@QEAA@AEBVFrustum@Volume@@AEBVTransform3@Math@@_N@Z"));
    fn_Graphics_Camera_SetFrustum = reinterpret_cast<fn_Graphics_Camera_SetFrustum_t>(
        ResolveExport("?SetFrustum@Camera@Graphics@@QEAAXAEBVFrustum@Volume@@@Z"));

    fn_Audio_Scene_MoveEmitter = reinterpret_cast<fn_Audio_Scene_MoveEmitter_t>(
        ResolveExport("?MoveEmitter@Scene@Audio@@QEAAXIAEBVVector3@Math@@0@Z"));
    fn_Audio_Scene_SetEmitterVolume = reinterpret_cast<fn_Audio_Scene_SetEmitterVolume_t>(
        ResolveExport("?SetEmitterVolume@Scene@Audio@@QEAAXIAEBVVector3@Math@@M@Z"));
    fn_Vector3_Equals = reinterpret_cast<fn_Vector3_Equals_t>(
        ResolveExport("?operator==@Vector3@Math@@QEBA_NAEBV12@@Z"));
    fn_DebugUI_OnDaySliderChanged = reinterpret_cast<fn_DebugUI_OnDaySliderChanged_t>(
        ResolveAddr(Module::Exe, 0x1400607E0));
    fn_DebugUI_WriteLightingColors = reinterpret_cast<fn_DebugUI_WriteLightingColors_t>(
        ResolveAddr(Module::Exe, 0x140061530));
    fn_Game_GetMonthFromTime = reinterpret_cast<fn_Game_GetMonthFromTime_t>(
        ResolveAddr(Module::Exe, 0x1400932A0));

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
    g_TypeId_ResidenceComponent_FC = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7900));
    g_TypeId_ResidenceDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7938));
    fn_ResidenceComponent_GetOccupantCount = reinterpret_cast<fn_ResidenceComponent_GetOccupantCount_t>(
        ResolveAddr(Module::Exe, 0x140123070));
    fn_ResidenceComponent_GetSpawnPoint = reinterpret_cast<fn_ResidenceComponent_GetSpawnPoint_t>(
        ResolveAddr(Module::Exe, 0x140122e20));
    fn_CitizenComponent_Setup = reinterpret_cast<fn_CitizenComponent_Setup_t>(
        ResolveAddr(Module::Exe, 0x140117080));
    fn_DisasterComponent_Infect = reinterpret_cast<fn_DisasterComponent_Infect_t>(
        ResolveAddr(Module::Exe, 0x140116b90));
    fn_BuildingList_CheckCondition = reinterpret_cast<fn_BuildingList_CheckCondition_t>(
        ResolveAddr(Module::Exe, 0x1400893c0));

    // --- SaveLoadDialog::Load helpers ---
    // fn_GameUpdateLoop_Ctor — REIMPLEMENTED, now resolve inner helpers
    fn_UpdateLoop_Ctor = reinterpret_cast<fn_UpdateLoop_Ctor_t>(
        ResolveExport("??0UpdateLoop@Engine@@QEAA@XZ"));
    if (!fn_UpdateLoop_Ctor) {
        fn_UpdateLoop_Ctor = reinterpret_cast<fn_UpdateLoop_Ctor_t>(
            ResolveAddr(Module::Runtime, 0x1800dcc00));
    }
    g_GameUpdateLoop_Vtable = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x14015e2f0));
    fn_LoaderLoop_Ctor = reinterpret_cast<fn_LoaderLoop_Ctor_t>(
        ResolveExport("??0LoaderLoop@Engine@@QEAA@PEAVUpdateLoop@1@AEBVTime@System@@@Z"));
    if (!fn_LoaderLoop_Ctor) {
        fn_LoaderLoop_Ctor = reinterpret_cast<fn_LoaderLoop_Ctor_t>(
            ResolveAddr(Module::Runtime, 0x1800dc1e0));
    }
    fn_DialogLoaderLoopT_Ctor = reinterpret_cast<fn_DialogLoaderLoopT_Ctor_t>(
        ResolveExport("??0DialogLoaderLoopT@Engine@@QEAA@AEBVString@System@@PEAVUpdateLoop@1@AEBVTime@3@@Z"));
    if (!fn_DialogLoaderLoopT_Ctor) {
        fn_DialogLoaderLoopT_Ctor = reinterpret_cast<fn_DialogLoaderLoopT_Ctor_t>(
            ResolveAddr(Module::Runtime, 0x1800db0d0));
    }
    fn_Random_Default_Ctor = reinterpret_cast<fn_Random_Default_Ctor_t>(
        ResolveExport("??0Random@System@@QEAA@XZ"));
    if (!fn_Random_Default_Ctor) {
        fn_Random_Default_Ctor = reinterpret_cast<fn_Random_Default_Ctor_t>(
            ResolveAddr(Module::Runtime, 0x1800c7f20));
    }
    // Exe vtable pointers for subclasses of DialogLoaderLoopT and LoaderLoop
    g_GameLoadDialogLoopT_Vtable = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140158800));
    g_GameLoaderLoop_Vtable      = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401588d0));
    // fn_SetEngineNextLoop — REPLACED with inline SetEngineNextLoop() in engine_funcs.h
    // fn_SetDialogLoaderNextLoop — REPLACED with inline SetDialogLoaderNextLoop() in engine_funcs.h
    // fn_GetLoadingSoundString — REPLACED with inline GetLoadingSoundString() in engine_funcs.h
    // fn_MenuDialogLoopT_Ctor — REIMPLEMENTED as inline MenuDialogLoopT_Ctor() in engine_funcs.h
    // Now resolve its inner dependencies:
    fn_DialogLoopT_Ctor = reinterpret_cast<fn_DialogLoopT_Ctor_t>(
        ResolveExport("??0DialogLoopT@Engine@@QEAA@AEBVString@System@@@Z"));
    if (!fn_DialogLoopT_Ctor) {
        fn_DialogLoopT_Ctor = reinterpret_cast<fn_DialogLoopT_Ctor_t>(
            ResolveAddr(Module::Runtime, 0x1800db3f0));
    }
    fn_DialogLoopT_Update = reinterpret_cast<fn_DialogLoopT_Update_t>(
        ResolveExport("?Update@DialogLoopT@Engine@@UEAA_NAEBVTime@System@@@Z"));
    if (!fn_DialogLoopT_Update) {
        fn_DialogLoopT_Update = reinterpret_cast<fn_DialogLoopT_Update_t>(
            ResolveAddr(Module::Runtime, 0x1800db270));
    }
    fn_DialogLoopT_Startup = reinterpret_cast<fn_DialogLoopT_Startup_t>(
        ResolveExport("?Startup@DialogLoopT@Engine@@UEAAXXZ"));
    if (!fn_DialogLoopT_Startup) {
        fn_DialogLoopT_Startup = reinterpret_cast<fn_DialogLoopT_Startup_t>(
            ResolveAddr(Module::Runtime, 0x1800db2f0));
    }
    fn_DialogLoopT_Load = reinterpret_cast<fn_DialogLoopT_Load_t>(
        ResolveExport("?Load@DialogLoopT@Engine@@UEAAXXZ"));
    if (!fn_DialogLoopT_Load) {
        fn_DialogLoopT_Load = reinterpret_cast<fn_DialogLoopT_Load_t>(
            ResolveAddr(Module::Runtime, 0x1800db140));
    }
    fn_DialogLoaderLoopT_Startup = reinterpret_cast<fn_DialogLoaderLoopT_Startup_t>(
        ResolveExport("?Startup@DialogLoaderLoopT@Engine@@UEAAXXZ"));
    if (!fn_DialogLoaderLoopT_Startup) {
        fn_DialogLoaderLoopT_Startup = reinterpret_cast<fn_DialogLoaderLoopT_Startup_t>(
            ResolveAddr(Module::Runtime, 0x1800db000));
    }
    fn_DialogLoaderLoopT_Load = reinterpret_cast<fn_DialogLoaderLoopT_Load_t>(
        ResolveExport("?Load@DialogLoaderLoopT@Engine@@UEAAXXZ"));
    if (!fn_DialogLoaderLoopT_Load) {
        fn_DialogLoaderLoopT_Load = reinterpret_cast<fn_DialogLoaderLoopT_Load_t>(
            ResolveAddr(Module::Runtime, 0x1800dad50));
    }
    fn_AudioScene_Ctor = reinterpret_cast<fn_AudioScene_Ctor_t>(
        ResolveExport("??0Scene@Audio@@QEAA@XZ"));
    if (!fn_AudioScene_Ctor) {
        fn_AudioScene_Ctor = reinterpret_cast<fn_AudioScene_Ctor_t>(
            ResolveAddr(Module::Runtime, 0x1800e0160));
    }
    g_MenuDialogLoopT_Vtable = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x14015ed00));

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
    printf("  Input::Device::Update     = %p\n", (void*)fn_InputDevice_Update);
    printf("  Audio::Device::Update     = %p\n", (void*)fn_AudioDevice_Update);
    printf("  Audio::FreeChannel        = %p\n", (void*)fn_AudioDevice_FreeChannel);
    printf("  App::PlatformUpdate       = %p\n", (void*)fn_App_PlatformUpdate);
    printf("  VideoCtx::ClearResources  = %p\n", (void*)fn_VideoCtx_ClearResources);
    printf("  VideoCtx::Reset           = %p\n", (void*)fn_VideoCtx_Reset);
    printf("  Loader::Load              = %p\n", (void*)fn_Loader_Load);
    printf("  App::SleepThread          = %p\n", (void*)fn_App_SleepThread);
    printf("  UI::Update                = %p\n", (void*)fn_UI_Update);
    printf("  UI::Draw                  = %p\n", (void*)fn_UI_Draw);
    printf("  UI::CloseDialog           = %p\n", (void*)fn_UI_CloseDialog);
    printf("  DynArrayPush              = %p\n", (void*)fn_DynArrayPush);
    printf("  String::Set               = %p\n", (void*)fn_String_Set);
    printf("  String::Format            = %p\n", (void*)fn_String_Format);
    printf("  String::~String           = %p\n", (void*)fn_String_Dtor);
    printf("  String::String()          = %p\n", (void*)fn_String_Ctor_Default);
    printf("  String::String(cstr)      = %p\n", (void*)fn_String_Ctor_CStr);
    printf("  String::String(copy)      = %p\n", (void*)fn_String_Ctor_Copy);
    printf("  PostDrawPush              = %p\n", (void*)fn_PostDrawPush);
    printf("  PostUpdatePush            = %p\n", (void*)fn_PostUpdatePush);
    printf("  VideoCtx::Ctor            = %p\n", (void*)fn_VideoCtx_Ctor);
    printf("  VideoCtx::Dtor            = %p\n", (void*)fn_VideoCtx_Dtor);
    printf("  VideoCtx::Delete          = %p\n", (void*)fn_VideoCtx_Delete);
    printf("  MemAlloc                  = %p\n", (void*)fn_MemAlloc);
    printf("  MemAllocSingleton         = %p\n", g_pMemAllocSingleton);
    printf("  VideoDevice::_singleton   = %p -> %p\n", (void*)g_pVideoDeviceSingleton,
           g_pVideoDeviceSingleton ? *g_pVideoDeviceSingleton : nullptr);
    printf("  ExtFactory::_singleton    = %p -> %p\n", (void*)g_pExternalFactorySingleton,
           g_pExternalFactorySingleton ? *g_pExternalFactorySingleton : nullptr);
    printf("  ExtFactory::FindOrLoad    = %p\n", (void*)fn_ExternalFactory_FindOrLoad);
    printf("  ExtFactory::Clean         = %p\n", (void*)fn_ExternalFactory_Clean);
    printf("  IsDerivedFrom             = %p\n", (void*)fn_IsDerivedFrom);
    printf("  ExternalTypeId            = %p -> 0x%x\n", (void*)g_pExternalTypeId,
           g_pExternalTypeId ? *g_pExternalTypeId : 0);
    printf("  Filename::AdjustDelimiters= %p\n", (void*)fn_Filename_AdjustDelimiters);
    printf("  UpdateLoop::LoadResources = %p\n", (void*)fn_UpdateLoop_LoadResources);
    printf("  ListCleanup               = %p\n", (void*)fn_ListCleanup);
    printf("  ModEntry::CopyCtor        = %p\n", (void*)fn_ModEntry_CopyCtor);
    printf("  ModEntry::Assign          = %p\n", (void*)fn_ModEntry_Assign);
    printf("  ModEntry::Dtor            = %p\n", (void*)fn_ModEntry_Dtor);
    printf("  Map::Find                 = %p\n", (void*)fn_Map_Find);
    printf("  Map::Insert               = %p\n", (void*)fn_Map_Insert);
    printf("  ParseCommand              = %p\n", (void*)fn_ParseCommand);
    printf("  ParseValue                = %p\n", (void*)fn_ParseValue);
    printf("  Persistence::_singleton   = %p -> %p\n", (void*)g_pPersistenceSingleton,
           g_pPersistenceSingleton ? *g_pPersistenceSingleton : nullptr);
    printf("  Persistence::SetBool      = %p\n", (void*)fn_Persistence_SetBool);
    printf("  Persistence::SetInt       = %p\n", (void*)fn_Persistence_SetInt);
    printf("  Filename::GetPathFilename = %p\n", (void*)fn_Filename_GetPathFilename);
    printf("  ModArrayCopy              = %p\n", (void*)fn_ModArrayCopy);
    printf("[EngineFuncs] UI infrastructure:\n");
    printf("  DialogController::~Dtor   = %p\n", (void*)fn_DialogController_Dtor);
    printf("  MemFreeStatic             = %p\n", (void*)fn_MemFreeStatic);
    printf("  FindElement               = %p\n", (void*)fn_FindElement);
    printf("  TextElement::SetParameter = %p\n", (void*)fn_TextElement_SetParameter);
    printf("  Element::GetUserData      = %p\n", (void*)fn_Element_GetUserData);
    printf("  InternalBasePtr::Ctor     = %p\n", (void*)fn_InternalBasePtr_Ctor);
    printf("  KeyMap::GetInputForId     = %p\n", (void*)fn_KeyMap_GetInputForId);
    printf("  KeyMap::GetDeviceForId    = %p\n", (void*)fn_KeyMap_GetDeviceForId);
    printf("[EngineFuncs] Phase A1 — UI Widget Functions:\n");
    printf("  Elem::SetFlagsOnOff       = %p\n", (void*)fn_Element_SetFlagsOnOff);
    printf("  Elem::SetElementId        = %p\n", (void*)fn_Element_SetElementId);
    printf("  Elem::SetName             = %p\n", (void*)fn_Element_SetName);
    printf("  Elem::SetToolTip          = %p\n", (void*)fn_Element_SetToolTip);
    printf("  Elem::Child               = %p\n", (void*)fn_Element_Child);
    printf("  Elem::ChildCount          = %p\n", (void*)fn_Element_ChildCount);
    printf("  Elem::FindElementType     = %p\n", (void*)fn_Element_FindElementType);
    printf("  DI::FindElement           = %p\n", (void*)fn_DialogInstance_FindElement);
    printf("  DI::GetController         = %p\n", (void*)fn_DialogInstance_GetController);
    printf("  DI::FindElementType       = %p\n", (void*)fn_DialogInstance_FindElementType);
    printf("  DI::SetFlagsOnOff         = %p\n", (void*)fn_DialogInstance_SetFlagsOnOff);
    printf("  Combo::AddItem            = %p\n", (void*)fn_ComboElement_AddItem);
    printf("  Combo::SetCurrentItem     = %p\n", (void*)fn_ComboElement_SetCurrentItem);
    printf("  Combo::GetItemCount       = %p\n", (void*)fn_ComboElement_GetItemCount);
    printf("  Combo::GetCurrentItem     = %p\n", (void*)fn_ComboElement_GetCurrentItem);
    printf("  Combo::RemoveAllItems     = %p\n", (void*)fn_ComboElement_RemoveAllItems);
    printf("  Progress::SetTextParam    = %p\n", (void*)fn_ProgressElement_SetProgressTextParameter);
    printf("  Progress::SetMaxValue     = %p\n", (void*)fn_ProgressElement_SetMaximumValue);
    printf("  Progress::SetCurValue     = %p\n", (void*)fn_ProgressElement_SetCurrentValue);
    printf("  Table::SetSize            = %p\n", (void*)fn_TableElement_SetSize);
    printf("  Table::SetChild(3)        = %p\n", (void*)fn_TableElement_SetChild3);
    printf("  NumEdit::SetValue         = %p\n", (void*)fn_NumberEditElement_SetValue);
    printf("  NumElem::SetValue(int)    = %p\n", (void*)fn_NumberElement_SetValueInt);
    printf("  Spinner::SetElementId     = %p\n", (void*)fn_SpinnerElement_SetElementId);
    printf("  Image::SetSprite          = %p\n", (void*)fn_ImageElement_SetSprite);
    printf("  Label::SetSprite          = %p\n", (void*)fn_LabelElement_SetSprite);
    printf("  Text::SetText             = %p\n", (void*)fn_TextElement_SetText);
    printf("[EngineFuncs] Phase A2 — Type IDs:\n");
    printf("  BuildComponent            = %p -> 0x%x\n", (void*)g_TypeId_BuildComponent,
           g_TypeId_BuildComponent ? *g_TypeId_BuildComponent : 0);
    printf("  DestroyComponent          = %p -> 0x%x\n", (void*)g_TypeId_DestroyComponent,
           g_TypeId_DestroyComponent ? *g_TypeId_DestroyComponent : 0);
    printf("  MeshInstanceComponent     = %p -> 0x%x\n", (void*)g_TypeId_MeshInstanceComponent,
           g_TypeId_MeshInstanceComponent ? *g_TypeId_MeshInstanceComponent : 0);
    printf("[EngineFuncs] Phase A3 — Singletons:\n");
    printf("  GameWorld                 = %p -> %p\n", (void*)g_pGameWorldSingleton,
           g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr);
    printf("  KeyMap (container)        = %p -> %p\n", (void*)g_pKeyMapSingleton,
           g_pKeyMapSingleton ? *g_pKeyMapSingleton : nullptr);
    printf("  KeyMap (object exref)     = %p -> %p\n", (void*)g_pKeyMapObjectExref,
           g_pKeyMapObjectExref ? *g_pKeyMapObjectExref : nullptr);
    printf("  EngineSingleton           = %p -> %p\n", (void*)g_pEngineSingleton,
           g_pEngineSingleton ? *g_pEngineSingleton : nullptr);
    printf("[EngineFuncs] Phase A4 -- FindComponent type IDs (C++ reimpl):\n");
    printf("  BuildComponent_Slot4      = %p -> 0x%x\n", (void*)g_TypeId_BuildComponent_Slot4,
           g_TypeId_BuildComponent_Slot4 ? *g_TypeId_BuildComponent_Slot4 : 0);
    printf("  CemeteryComponent         = %p -> 0x%x\n", (void*)g_TypeId_CemeteryComponent,
           g_TypeId_CemeteryComponent ? *g_TypeId_CemeteryComponent : 0);
    // DestroyComponent_Reclaim/Rebuild/Destroy: now implemented as statics in destroyui.cpp
    printf("[EngineFuncs] Video::ImageBuffer:\n");
    printf("  ImageBuffer::Ctor         = %p\n", (void*)fn_ImageBuffer_Ctor);
    printf("  ImageBuffer::Dtor         = %p\n", (void*)fn_ImageBuffer_Dtor);
    printf("  ImageBuffer::CopyToTex    = %p\n", (void*)fn_ImageBuffer_CopyToTexture);
    printf("  ImageBuffer::CopyFromTex  = %p\n", (void*)fn_ImageBuffer_CopyFromTexture);
    printf("[EngineFuncs] Globals:\n");
    printf("  _loader                   = %p\n", g_pLoader);
    printf("  _loader.m_flags           = %p -> 0x%x\n", (void*)g_pLoaderFlags,
           g_pLoaderFlags ? *g_pLoaderFlags : 0);
    printf("  _loader.m_completionEvent = %p -> %p\n", (void*)g_pLoaderCompletionEvent,
           g_pLoaderCompletionEvent ? *g_pLoaderCompletionEvent : nullptr);
    printf("  App::_singleton           = %p -> %p\n", (void*)g_pAppSingleton,
           g_pAppSingleton ? *g_pAppSingleton : nullptr);
#endif

    // --- SpriteCaptureUI support ---
    fn_Input_Device_GetValue = reinterpret_cast<fn_Input_Device_GetValue_t>(
        ResolveExport("?GetValue@Device@Input@@QEBAFW4InputDevice@12@HH@Z"));
    if (!fn_Input_Device_GetValue)
        fn_Input_Device_GetValue = reinterpret_cast<fn_Input_Device_GetValue_t>(
            ResolveAddr(Module::Runtime, 0x1800e1660));

    fn_Input_Device_Pressed = reinterpret_cast<fn_Input_Device_Pressed_t>(
        ResolveExport("?Pressed@Device@Input@@QEBA_NW4InputDevice@12@HH@Z"));
    if (!fn_Input_Device_Pressed)
        fn_Input_Device_Pressed = reinterpret_cast<fn_Input_Device_Pressed_t>(
            ResolveAddr(Module::Runtime, 0x1800e16c0));

    fn_UI_EndSpriteCapture = reinterpret_cast<fn_UI_EndSpriteCapture_t>(
        ResolveExport("?EndSpriteCapture@UserInterface@UI@@QEAAXXZ"));
    if (!fn_UI_EndSpriteCapture)
        fn_UI_EndSpriteCapture = reinterpret_cast<fn_UI_EndSpriteCapture_t>(
            ResolveAddr(Module::Runtime, 0x18011f4a0));

    // FileSystem singleton: exe exref at 0x140151d18 -> pointer to _singleton in Runtime
    {
        void*** exref = reinterpret_cast<void***>(ResolveAddr(Module::Exe, 0x140151d18));
        if (exref && *exref)
            g_pFileSystemSingleton = *exref;
    }

    fn_FileSystem_GetSourceDirectory = reinterpret_cast<fn_FileSystem_GetSourceDirectory_t>(
        ResolveExport("?GetSourceDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ"));
    if (!fn_FileSystem_GetSourceDirectory)
        fn_FileSystem_GetSourceDirectory = reinterpret_cast<fn_FileSystem_GetSourceDirectory_t>(
            ResolveAddr(Module::Runtime, 0x180007b90));

    fn_External_SaveText = reinterpret_cast<fn_External_SaveText_t>(
        ResolveExport("?SaveText@External@Object@@QEAA_NAEBVFilename@System@@0@Z"));
    if (!fn_External_SaveText)
        fn_External_SaveText = reinterpret_cast<fn_External_SaveText_t>(
            ResolveAddr(Module::Runtime, 0x1800bb9d0));

    fn_Filename_CtorString = reinterpret_cast<fn_Filename_CtorString_t>(
        ResolveExport("??0Filename@System@@QEAA@AEBVString@1@@Z"));
    if (!fn_Filename_CtorString)
        fn_Filename_CtorString = reinterpret_cast<fn_Filename_CtorString_t>(
            ResolveAddr(Module::Runtime, 0x18000c780));

    // Default ctor (no-arg). Mangled name verified via Ghidra Runtime DLL
    // 0x180006220 — body is 8-byte zero-init.
    fn_Filename_Ctor = reinterpret_cast<fn_Filename_Ctor_t>(
        ResolveExport("??0Filename@System@@QEAA@XZ"));
    if (!fn_Filename_Ctor)
        fn_Filename_Ctor = reinterpret_cast<fn_Filename_Ctor_t>(
            ResolveAddr(Module::Runtime, 0x180006220));

    fn_Filename_CtorCStr = reinterpret_cast<fn_Filename_CtorCStr_t>(
        ResolveExport("??0Filename@System@@QEAA@PEBD@Z"));
    if (!fn_Filename_CtorCStr)
        fn_Filename_CtorCStr = reinterpret_cast<fn_Filename_CtorCStr_t>(
            ResolveAddr(Module::Runtime, 0x180026cf0));

    fn_Filename_Dtor = reinterpret_cast<fn_Filename_Dtor_t>(
        ResolveExport("??1Filename@System@@QEAA@XZ"));
    if (!fn_Filename_Dtor)
        fn_Filename_Dtor = reinterpret_cast<fn_Filename_Dtor_t>(
            ResolveAddr(Module::Runtime, 0x180006980));

    // String concat operator+ via dispatch table (non-trivial return ABI, free functions)
    // Dispatch at 0x140151a10 (ordinal 0x100) and 0x140151a08 (ordinal 0xff)
    {
        void** d1 = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151a10));
        if (d1 && *d1)
            fn_String_ConcatSS = reinterpret_cast<fn_String_Concat_t>(*d1);
        void** d2 = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151a08));
        if (d2 && *d2)
            fn_String_ConcatSS2 = reinterpret_cast<fn_String_Concat_t>(*d2);
    }

    // --- Stage 3: OptionsDialog Runtime exports ---

    // Audio::Configuration
    fn_AudioConfig_Save = reinterpret_cast<fn_AudioConfig_Save_t>(
        ResolveExport("?Save@Configuration@Audio@@QEAAXXZ"));
    fn_AudioConfig_Startup = reinterpret_cast<fn_AudioConfig_Startup_t>(
        ResolveExport("?Startup@Configuration@Audio@@QEAA_NXZ"));

    // Video::Configuration
    fn_VideoConfig_Save = reinterpret_cast<fn_VideoConfig_Save_t>(
        ResolveExport("?Save@Configuration@Video@@QEAAXXZ"));

    // Input::Configuration
    fn_InputConfig_Save = reinterpret_cast<fn_InputConfig_Save_t>(
        ResolveExport("?Save@Configuration@Input@@QEAAXXZ"));
    fn_InputConfig_Load = reinterpret_cast<fn_InputConfig_Load_t>(
        ResolveExport("?Load@Configuration@Input@@QEAAXXZ"));

    // Input::Configuration::_singleton
    {
        void** p = reinterpret_cast<void**>(
            ResolveExport("?_singleton@Configuration@Input@@0PEAV12@EA"));
        if (p)
            g_pInputConfigSingleton = p;
    }

    // IO::Persistence
    fn_Persistence_WriteSettings = reinterpret_cast<fn_Persistence_WriteSettings_t>(
        ResolveExport("?WriteSettings@Persistence@IO@@QEBAXXZ"));
    fn_Persistence_ReadSettings = reinterpret_cast<fn_Persistence_ReadSettings_t>(
        ResolveExport("?ReadSettings@Persistence@IO@@QEAAXXZ"));

    // Input::Device
    fn_InputDevice_EnableMouseClipping = reinterpret_cast<fn_InputDevice_EnableMouseClipping_t>(
        ResolveExport("?EnableMouseClipping@Device@Input@@QEAAX_N@Z"));
    fn_InputDevice_ResetState = reinterpret_cast<fn_InputDevice_ResetState_t>(
        ResolveExport("?ResetState@Device@Input@@QEAAXXZ"));

    // Video::Device
    fn_VideoDevice_ApplyChangedSettings = reinterpret_cast<fn_VideoDevice_ApplyChangedSettings_t>(
        ResolveExport("?ApplyChangedSettings@Device@Video@@QEAA_NXZ"));
    fn_VideoDevice_Startup = reinterpret_cast<fn_VideoDevice_Startup_t>(
        ResolveExport("?Startup@Device@Video@@QEAA_NXZ"));
    fn_VideoDevice_RecreateDynamic = reinterpret_cast<fn_VideoDevice_RecreateDynamic_t>(
        ResolveExport("?RecreateDynamic@Device@Video@@QEAAXXZ"));
    fn_VideoDevice_ReleaseDynamic = reinterpret_cast<fn_VideoDevice_ReleaseDynamic_t>(
        ResolveExport("?ReleaseDynamic@Device@Video@@QEAAXXZ"));
    fn_VideoDevice_Shutdown = reinterpret_cast<fn_VideoDevice_Shutdown_t>(
        ResolveExport("?Shutdown@Device@Video@@QEAAXXZ"));

    // Engine::MainLoop
    fn_MainLoop_OnVideoError = reinterpret_cast<fn_MainLoop_OnVideoError_t>(
        ResolveExport("?OnVideoError@MainLoop@Engine@@QEAAXXZ"));
    fn_MainLoop_OnVideoRestart = reinterpret_cast<fn_MainLoop_OnVideoRestart_t>(
        ResolveExport("?OnVideoRestart@MainLoop@Engine@@QEAAXXZ"));
    fn_MainLoop_SetNextLoop = reinterpret_cast<fn_MainLoop_SetNextLoop_t>(
        ResolveExport("?SetNextLoop@MainLoop@Engine@@QEAAXPEAVUpdateLoop@2@@Z"));

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

    // UI::UserInterface
    fn_UI_EnableCursor = reinterpret_cast<fn_UI_EnableCursor_t>(
        ResolveExport("?EnableCursor@UserInterface@UI@@QEAAX_N@Z"));
    fn_UI_GetFirstDialog = reinterpret_cast<fn_UI_GetFirstDialog_t>(
        ResolveExport("?GetFirstDialog@UserInterface@UI@@QEAA?AUIterator@?$LinkedList@V?$InternalPtr@VDialogInstance@UI@@@Object@@UDefaultAllocator@Collection@@@Collection@@XZ"));
    fn_UI_SetFullscreenFadeSprite = reinterpret_cast<fn_UI_SetFullscreenFadeSprite_t>(
        ResolveExport("?SetFullscreenFadeSprite@UserInterface@UI@@QEAAXVSprite@2@@Z"));

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

    // ShowDialog, DeletePtr — Runtime DLL exports
    fn_UI_ShowDialog = reinterpret_cast<fn_UI_ShowDialog_t>(
        ResolveExport("?ShowDialog@UserInterface@UI@@QEAAHV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z"));
    fn_InternalBasePtr_DeletePtr = reinterpret_cast<fn_InternalBasePtr_DeletePtr_t>(
        ResolveExport("?DeletePtr@InternalBasePtr@Object@@QEAAXXZ"));

    // KeyMap input management — Runtime DLL exports
    fn_KeyMap_SetInput = reinterpret_cast<fn_KeyMap_SetInput_t>(
        ResolveExport("?SetInput@KeyMap@Input@@QEAAXHHHH@Z"));
    fn_KeyMap_ClearInput2 = reinterpret_cast<fn_KeyMap_ClearInput2_t>(
        ResolveExport("?ClearInput@KeyMap@Input@@QEAAXHH@Z"));
    fn_KeyMap_ClearInput3 = reinterpret_cast<fn_KeyMap_ClearInput3_t>(
        ResolveExport("?ClearInput@KeyMap@Input@@QEAAXHHH@Z"));
    fn_KeyMap_GetIdForDeviceInput = reinterpret_cast<fn_KeyMap_GetIdForDeviceInput_t>(
        ResolveExport("?GetIdForDeviceInput@KeyMap@Input@@QEBAHHH@Z"));

    // KeyMap default-binding setup — used by Game_RegisterDefaultInputBindings
    // (clean reimpl of FUN_1400992f0). Mangled names verified against
    // Runtime-x64.dll exports in Ghidra.
    fn_KeyMap_RegisterInput = reinterpret_cast<fn_KeyMap_RegisterInput_t>(
        ResolveExport("?RegisterInput@KeyMap@Input@@QEAAXHHHPEBD_N@Z"));
    if (!fn_KeyMap_RegisterInput) {
        fn_KeyMap_RegisterInput = reinterpret_cast<fn_KeyMap_RegisterInput_t>(
            ResolveAddr(Module::Runtime, 0x1800a7e70));
    }
    fn_KeyMap_ClearAll = reinterpret_cast<fn_KeyMap_ClearAll_t>(
        ResolveExport("?ClearAll@KeyMap@Input@@QEAAXXZ"));
    if (!fn_KeyMap_ClearAll) {
        fn_KeyMap_ClearAll = reinterpret_cast<fn_KeyMap_ClearAll_t>(
            ResolveAddr(Module::Runtime, 0x1800a7d20));
    }
    fn_KeyMap_Load = reinterpret_cast<fn_KeyMap_Load_t>(
        ResolveExport("?Load@KeyMap@Input@@QEAAXXZ"));
    if (!fn_KeyMap_Load) {
        fn_KeyMap_Load = reinterpret_cast<fn_KeyMap_Load_t>(
            ResolveAddr(Module::Runtime, 0x1800a7190));
    }

    // Dialog constructors — exe functions
    fn_OptionInputBindDialog_Ctor = reinterpret_cast<fn_OptionInputBindDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x140071d30));
    fn_MessageDialog_Ctor = reinterpret_cast<fn_MessageDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x14006d300));

    // CaptureBoxElement / FrustumOutlineElement — read from exe dispatch table
    // These slots are already filled by the runtime at startup.
    fn_ElementDescription_GetFileVersion = *reinterpret_cast<fn_ElementDescription_GetFileVersion_t*>(
        ResolveAddr(Module::Exe, 0x1401518c0));
    fn_Element_Draw = *reinterpret_cast<fn_Element_Draw_t*>(
        ResolveAddr(Module::Exe, 0x1401518f8));
    fn_SpriteContext_AddLine = *reinterpret_cast<fn_SpriteContext_AddLine_t*>(
        ResolveAddr(Module::Exe, 0x140151908));
    fn_SpriteContext_SetScissor4 = *reinterpret_cast<fn_SpriteContext_SetScissor4_t*>(
        ResolveAddr(Module::Exe, 0x140151900));
    fn_SpriteContext_SetScissorVP = *reinterpret_cast<fn_SpriteContext_SetScissorVP_t*>(
        ResolveAddr(Module::Exe, 0x140151910));
    fn_FrustumProject = reinterpret_cast<fn_FrustumProject_t>(
        ResolveAddr(Module::Exe, 0x14006c5b0));

    // --- Description class reimpl support ---
    // Dispatch table reads for Runtime DLL functions
    g_fn_String_Serialize = *reinterpret_cast<g_fn_String_Serialize_t*>(
        ResolveAddr(Module::Exe, 0x140151238));
    fn_ElementDescription_Serialize = *reinterpret_cast<fn_ElementDescription_Serialize_t*>(
        ResolveAddr(Module::Exe, 0x1401518c8));
    fn_ElementDescription_Dtor = *reinterpret_cast<fn_ElementDescription_Dtor_t*>(
        ResolveAddr(Module::Exe, 0x1401518b8));
    fn_ElementDescription_Ctor = *reinterpret_cast<fn_ElementDescription_Ctor_t*>(
        ResolveAddr(Module::Exe, 0x1401518b0));
    fn_ElementDescription_CreateInstance = *reinterpret_cast<fn_ElementDescription_CreateInstance_t*>(
        ResolveAddr(Module::Exe, 0x1401518d0));
    fn_Element_Ctor = *reinterpret_cast<fn_Element_Ctor_t*>(
        ResolveAddr(Module::Exe, 0x1401518d8));
    fn_Element_Dtor = *reinterpret_cast<fn_Element_Dtor_t*>(
        ResolveAddr(Module::Exe, 0x1401518e0));
    fn_Element_ComputeLayout = *reinterpret_cast<fn_Element_ComputeLayout_t*>(
        ResolveAddr(Module::Exe, 0x1401518e8));
    fn_Element_GetAlignedPosition = *reinterpret_cast<fn_Element_GetAlignedPosition_t*>(
        ResolveAddr(Module::Exe, 0x1401518f0));
    fn_Serializer_SerializeBulk = *reinterpret_cast<fn_Serializer_SerializeBulk_t*>(
        ResolveAddr(Module::Exe, 0x140152400));

    // Direct exe function calls used by Description serializers
    fn_ExternalPtr_Serialize = reinterpret_cast<fn_ExternalPtr_Serialize_t>(
        ResolveAddr(Module::Exe, 0x14000bd30));
    fn_ExternalPtr_Serialize2 = reinterpret_cast<fn_ExternalPtr_Serialize2_t>(
        ResolveAddr(Module::Exe, 0x14000bf40));
    fn_ExternalPtr_Serialize3 = reinterpret_cast<fn_ExternalPtr_Serialize3_t>(
        ResolveAddr(Module::Exe, 0x140007140));
    fn_ExternalRefArray_Serialize = reinterpret_cast<fn_ExternalRefArray_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400049c0));
    fn_ComponentBase_Serialize = reinterpret_cast<fn_ComponentBase_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    fn_DynArray_Resize = reinterpret_cast<fn_DynArray_Resize_t>(
        ResolveAddr(Module::Exe, 0x14000a590));
    fn_MemZeroFill = reinterpret_cast<fn_MemZeroFill_t>(
        ResolveAddr(Module::Exe, 0x14003f010));
    fn_StringArray_Resize = reinterpret_cast<fn_StringArray_Resize_t>(
        ResolveAddr(Module::Exe, 0x140028860));
    fn_ExternalPtr_BuildSerialize = reinterpret_cast<fn_ExternalPtr_BuildSerialize_t>(
        ResolveAddr(Module::Exe, 0x140002340));
    fn_WeatherResize = reinterpret_cast<fn_WeatherResize_t>(
        ResolveAddr(Module::Exe, 0x1400198a0));
    fn_WeatherExternalSerialize = reinterpret_cast<fn_WeatherExternalSerialize_t>(
        ResolveAddr(Module::Exe, 0x1400196a0));
    fn_ToolbarDescription_cleanup = reinterpret_cast<fn_ToolbarDescription_cleanup_t>(
        ResolveAddr(Module::Exe, 0x14012db70));
    fn_CreateBridgeDescription_Ctor = reinterpret_cast<fn_CreateBridgeDescription_Ctor_t>(
        ResolveAddr(Module::Exe, 0x1400328c0));
    fn_CreateDragDescription_Ctor = reinterpret_cast<fn_CreateDragDescription_Ctor_t>(
        ResolveAddr(Module::Exe, 0x1400329b0));
    fn_CreateFencedDescription_Ctor = reinterpret_cast<fn_CreateFencedDescription_Ctor_t>(
        ResolveAddr(Module::Exe, 0x140004920));

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
    fn_TSD_FindByString = reinterpret_cast<fn_TSD_FindByString_t>(
        ResolveAddr(Module::Exe, 0x1400e1600));
    fn_SetFollowBuilding = reinterpret_cast<fn_SetFollowBuilding_t>(
        ResolveAddr(Module::Exe, 0x140101a80));
    fn_SetEditMode = reinterpret_cast<fn_SetEditMode_t>(
        ResolveAddr(Module::Exe, 0x140102b60));

    // Phase G Wave 1: SaveLoadDialog DoLoad/DoSave/DoDelete + EventNotifyDialog NavigateToEvent
    // Save::SaveLoad::CheckMod — Runtime DLL export (via dispatch table at 0x140152738)
    fn_SaveLoad_CheckMod = reinterpret_cast<fn_SaveLoad_CheckMod_t>(
        ResolveExport("?CheckMod@SaveLoad@Save@@QEAAHHI@Z"));
    if (!fn_SaveLoad_CheckMod) {
        // Fallback: resolve via exe dispatch table
        fn_SaveLoad_CheckMod = *reinterpret_cast<fn_SaveLoad_CheckMod_t*>(
            ResolveAddr(Module::Exe, 0x140152738));
    }
    // Save::SaveLoad::SetSaveName — Runtime DLL export (via dispatch table at 0x140151650)
    fn_SaveLoad_SetSaveName = reinterpret_cast<fn_SaveLoad_SetSaveName_t>(
        ResolveExport("?SetSaveName@SaveLoad@Save@@QEAAXV?$String@D@System@@H@Z"));
    if (!fn_SaveLoad_SetSaveName) {
        fn_SaveLoad_SetSaveName = *reinterpret_cast<fn_SaveLoad_SetSaveName_t*>(
            ResolveAddr(Module::Exe, 0x140151650));
    }
    // DialogInstance type ID — _typeInfo_exref at 0x140151728
    g_TypeId_DialogInstance = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x140151728));
    // FUN_14009f3e0 — terrain/scene pick from 2D map position (exe-only)
    fn_MapGetTerrainPos = reinterpret_cast<fn_MapGetTerrainPos_t>(
        ResolveAddr(Module::Exe, 0x14009f3e0));

    // Sprite ctors — dispatch table
    fn_Sprite_Sprite = *reinterpret_cast<fn_Sprite_Sprite_t*>(
        ResolveAddr(Module::Exe, 0x1401514c8));
    fn_Sprite_DefaultCtor2 = *reinterpret_cast<fn_Sprite_DefaultCtor2_t*>(
        ResolveAddr(Module::Exe, 0x140152b48));

    // --- Phase H: Sub-dependency blockers ---
    // Factory::GetTypeIdFromName — Runtime DLL export
    fn_Factory_GetTypeIdFromName = reinterpret_cast<fn_Factory_GetTypeIdFromName_t>(
        ResolveExport("?GetTypeIdFromName@Factory@Object@@QEBAIPEBD_N@Z"));
    if (!fn_Factory_GetTypeIdFromName) {
        fn_Factory_GetTypeIdFromName = reinterpret_cast<fn_Factory_GetTypeIdFromName_t>(
            ResolveOrdinal(2991));
    }
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
    if (!fn_IsA) {
        fn_IsA = reinterpret_cast<fn_IsA_t>(
            ResolveOrdinal(3089));
    }
#ifdef _DEBUG
    if (fn_Factory_GetTypeIdFromName && g_pFactorySingleton && fn_IsA) {
        printf("[EngineFuncs] Phase H: Factory::GetTypeIdFromName + IsA + Factory singleton resolved\n");
    }
    if (fn_Factory_GetTypeIdFromName) {
        printf("[EngineFuncs] Phase H: FindDialogByName_Impl, Game_TriggerDisasterByType_Impl, "
               "Game_SpawnResources_Impl now available as inline replacements\n");
    }
#endif

    // --- Phase I: Work class helpers ---
    fn_TreeRemoveByKey = reinterpret_cast<fn_TreeRemoveByKey_t>(
        ResolveAddr(Module::Exe, 0x140025520));
    fn_TreeRecursiveFree = reinterpret_cast<fn_TreeRecursiveFree_t>(
        ResolveAddr(Module::Exe, 0x140014c30));
    fn_ObjectInternal_Dtor = reinterpret_cast<fn_ObjectInternal_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152770)));
    fn_StateMachine_ReleaseTasks = reinterpret_cast<fn_StateMachine_ReleaseTasks_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    fn_Map_CheckPassable = reinterpret_cast<fn_Map_CheckPassable_t>(
        ResolveAddr(Module::Exe, 0x1400a00f0));
    fn_Map_FindNearestPassable = reinterpret_cast<fn_Map_FindNearestPassable_t>(
        ResolveAddr(Module::Exe, 0x14009ccf0));
    fn_Vector3_Ctor = *reinterpret_cast<fn_Vector3_Ctor_t*>(
        ResolveAddr(Module::Exe, 0x140152a30));
    fn_PerformWork_Action_Ctor = reinterpret_cast<fn_PerformWork_Action_Ctor_t>(
        ResolveAddr(Module::Exe, 0x1400bffd0));
    fn_PerformWork_Action_Ctor3 = reinterpret_cast<fn_PerformWork_Action_Ctor3_t>(
        ResolveAddr(Module::Exe, 0x1400c1eb0));
    fn_FindStorageForResource = reinterpret_cast<fn_FindStorageForResource_t>(
        ResolveAddr(Module::Exe, 0x14008d590));
    fn_FindComponent_WorkplaceC = reinterpret_cast<fn_FindComponent_WorkplaceC_t>(
        ResolveAddr(Module::Exe, 0x1400223c0));
    fn_FindComponent_BuildStateC = reinterpret_cast<fn_FindComponent_BuildStateC_t>(
        ResolveAddr(Module::Exe, 0x1400052c0));
    fn_FindComponent_StorageLocC = reinterpret_cast<fn_FindComponent_StorageLocC_t>(
        ResolveAddr(Module::Exe, 0x140003050));
    fn_TryAssignWorkplace = reinterpret_cast<fn_TryAssignWorkplace_t>(
        ResolveAddr(Module::Exe, 0x1400f8620));
    fn_FindComponent_RawMaterialC = reinterpret_cast<fn_FindComponent_RawMaterialC_t>(
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
    fn_Work_SerializeBase = reinterpret_cast<fn_Work_SerializeBase_t>(
        ResolveAddr(Module::Exe, 0x1400f7400));
    fn_BST_SerializeHelper = reinterpret_cast<fn_BST_SerializeHelper_t>(
        ResolveAddr(Module::Exe, 0x1400296b0));
    fn_Entity_FindComponent_Position = reinterpret_cast<fn_Entity_FindComponent_Position_t>(
        ResolveAddr(Module::Exe, 0x140002ed0));
    fn_Entity_FindComponent_Build = reinterpret_cast<fn_Entity_FindComponent_Build_t>(
        ResolveAddr(Module::Exe, 0x14000d6c0));
    fn_Entity_FindComponent_BuildDesc = reinterpret_cast<fn_Entity_FindComponent_BuildDesc_t>(
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
    fn_CitizenComponent_HasVacancy = reinterpret_cast<fn_CitizenComponent_HasVacancy_t>(
        ResolveAddr(Module::Exe, 0x140017050));
    fn_FindStorageLocationComp = reinterpret_cast<fn_FindStorageLocationComp_t>(
        ResolveAddr(Module::Exe, 0x140003050));
    fn_FindWorkplaceComp = reinterpret_cast<fn_FindWorkplaceComp_t>(
        ResolveAddr(Module::Exe, 0x140002e10));
#ifdef _DEBUG
    printf("[EngineFuncs] Phase I+J: All work class + BuildStructure helpers resolved\n");
#endif

    // --- Achievement Goal support ---
    fn_Goal_BaseDtor = reinterpret_cast<fn_Goal_BaseDtor_t>(
        ResolveExport("??1Goal@Achievement@@UEAA@XZ"));
    if (!fn_Goal_BaseDtor)
        fn_Goal_BaseDtor = reinterpret_cast<fn_Goal_BaseDtor_t>(
            ResolveAddr(Module::Runtime, 0x18009edb0));

    fn_Goal_BaseCtor = reinterpret_cast<fn_Goal_BaseCtor_t>(
        ResolveExport("??0Goal@Achievement@@QEAA@XZ"));
    if (!fn_Goal_BaseCtor)
        fn_Goal_BaseCtor = reinterpret_cast<fn_Goal_BaseCtor_t>(
            ResolveAddr(Module::Runtime, 0x18009ed50));

    // Goal::Serialize via dispatch slot 0x140152840
    {
        void** slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152840));
        if (slot && *slot)
            fn_Goal_BaseSerialize = reinterpret_cast<fn_Goal_BaseSerialize_t>(*slot);
    }

    fn_GoalInstance_BaseDtor = reinterpret_cast<fn_GoalInstance_BaseDtor_t>(
        ResolveExport("??1GoalInstance@Achievement@@UEAA@XZ"));
    if (!fn_GoalInstance_BaseDtor)
        fn_GoalInstance_BaseDtor = reinterpret_cast<fn_GoalInstance_BaseDtor_t>(
            ResolveAddr(Module::Runtime, 0x18009ef40));

    fn_GoalInstance_Persist = reinterpret_cast<fn_GoalInstance_Persist_t>(
        ResolveExport("?Persist@GoalInstance@Achievement@@QEAAXAEAVStream@IO@@@Z"));
    if (!fn_GoalInstance_Persist)
        fn_GoalInstance_Persist = reinterpret_cast<fn_GoalInstance_Persist_t>(
            ResolveAddr(Module::Runtime, 0x18009ef70));

    fn_InternalBasePtr_Serialize = reinterpret_cast<fn_InternalBasePtr_Serialize_t>(
        ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z"));

    // IsRunning via dispatch slot 0x140152850
    {
        void** slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152850));
        if (slot && *slot)
            fn_IsRunning = reinterpret_cast<fn_IsRunning_t>(*slot);
    }

    // FinishTutorialGoal helper: External serialize for tutorial ref
    fn_ExternalPtr_SerializeTutorial = reinterpret_cast<fn_ExternalPtr_SerializeTutorial_t>(
        ResolveAddr(Module::Exe, 0x1400262b0));

    // BuildingGoal helpers
    fn_BuildingTypeArray_Dtor = reinterpret_cast<fn_BuildingTypeArray_Dtor_t>(
        ResolveAddr(Module::Exe, 0x14001b030));
    fn_BuildingTypeArray_Resize = reinterpret_cast<fn_BuildingTypeArray_Resize_t>(
        ResolveAddr(Module::Exe, 0x14001b340));
    fn_StringHash = reinterpret_cast<fn_StringHash_t>(
        ResolveAddr(Module::Exe, 0x14001b1d0));

    // Filename ctor/dtor — already resolved from dispatch slots earlier in init
    // fn_Filename_Ctor at 0x140151d40, fn_Filename_Dtor at 0x1401529e8
    // Resolve here as fallback if not yet done
    if (!fn_Filename_Ctor) {
        void** slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151d40));
        if (slot && *slot)
            fn_Filename_Ctor = reinterpret_cast<fn_Filename_Ctor_t>(*slot);
    }
    if (!fn_Filename_Dtor) {
        void** slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401529e8));
        if (slot && *slot)
            fn_Filename_Dtor = reinterpret_cast<fn_Filename_Dtor_t>(*slot);
    }

#ifdef _DEBUG
    printf("[EngineFuncs] Achievement Goal support resolved\n");
    printf("  Goal::~Goal         = %p\n", (void*)fn_Goal_BaseDtor);
    printf("  Goal::Goal()        = %p\n", (void*)fn_Goal_BaseCtor);
    printf("  Goal::Serialize     = %p\n", (void*)fn_Goal_BaseSerialize);
    printf("  GoalInst::~GoalInst = %p\n", (void*)fn_GoalInstance_BaseDtor);
    printf("  GoalInst::Persist   = %p\n", (void*)fn_GoalInstance_Persist);
    printf("  IBP::Serialize      = %p\n", (void*)fn_InternalBasePtr_Serialize);
    printf("  IsRunning           = %p\n", (void*)fn_IsRunning);
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

    // TutorialInstance vtable at 0x140160bf8
    g_TutorialInstanceVtable = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140160bf8));
#ifdef _DEBUG
    printf("[EngineFuncs] Phase K: Tutorial goal helpers resolved\n");
    printf("  FindUIElementByPath   = %p\n", (void*)fn_FindUIElementByPath);
    printf("  ObjectInternal::Ctor  = %p\n", (void*)fn_ObjectInternal_Ctor);
    printf("  TutorialTreeInsert    = %p\n", (void*)fn_TutorialTreeInsert);
    printf("  TutorialStepExecute   = %p\n", (void*)fn_TutorialStepExecute);
    printf("  TypeId_FTGoal         = %p\n", (void*)g_TypeId_FinishTutorialGoal);
    printf("  SingletonExref_151a80 = %p\n", (void*)g_pSingletonExref_151a80);
    printf("  TutInstVtable         = %p\n", (void*)g_TutorialInstanceVtable);
#endif

    // ================================================================
    // Phase L: ComponentEntity dependencies
    // ================================================================
    g_TypeId_ComponentBase = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c72a8));
    // Pure C++ reimpl (FUN_140003110) — NOT exe-resolved: in standalone the exe is
    // not mapped, so ResolveAddr(0x140003110) would return NULL and EntityList::
    // RegisterEntity would silently skip its array appends.
    fn_ExeArrayResizeFill = &ExeArrayResizeFill_CppImpl;
#ifdef _DEBUG
    printf("[EngineFuncs] Phase L: ComponentEntity deps resolved\n");
    printf("  TypeId_ComponentBase  = %p\n", (void*)g_TypeId_ComponentBase);
    printf("  ExeArrayResizeFill    = %p\n", (void*)fn_ExeArrayResizeFill);
#endif

    // ================================================================
    // Phase L2: CitizenBehavior dependencies
    // ================================================================
    fn_CitizenBehavior_CountResources = reinterpret_cast<fn_CitizenBehavior_CountResources_t>(
        ResolveAddr(Module::Exe, 0x140089ed0));
    fn_CitizenBehavior_CheckThresholds = reinterpret_cast<fn_CitizenBehavior_CheckThresholds_t>(
        ResolveAddr(Module::Exe, 0x14008c100));
    fn_CitizenBehavior_CollectStats = reinterpret_cast<fn_CitizenBehavior_CollectStats_t>(
        ResolveAddr(Module::Exe, 0x14008a730));
    fn_CitizenBehavior_ProcessPaths = reinterpret_cast<fn_CitizenBehavior_ProcessPaths_t>(
        ResolveAddr(Module::Exe, 0x1400d6f00));
    fn_CitizenBehavior_BuildPathGraph = reinterpret_cast<fn_CitizenBehavior_BuildPathGraph_t>(
        ResolveAddr(Module::Exe, 0x1400d70c0));
    fn_CitizenBehavior_ShuffleSchools = reinterpret_cast<fn_CitizenBehavior_ShuffleSchools_t>(
        ResolveAddr(Module::Exe, 0x1400896a0));
    fn_ProfResArray_Init = reinterpret_cast<fn_ProfResArray_Init_t>(
        ResolveAddr(Module::Exe, 0x1400177c0));
    fn_StatsArray_Init = reinterpret_cast<fn_StatsArray_Init_t>(
        ResolveAddr(Module::Exe, 0x140017640));
    fn_SummaryArray_Init = reinterpret_cast<fn_SummaryArray_Init_t>(
        ResolveAddr(Module::Exe, 0x14000f090));
#ifdef _DEBUG
    printf("[EngineFuncs] Phase L2: CitizenBehavior deps resolved\n");
    printf("  CountResources = %p\n", (void*)fn_CitizenBehavior_CountResources);
    printf("  CollectStats   = %p\n", (void*)fn_CitizenBehavior_CollectStats);
    printf("  ProcessPaths   = %p\n", (void*)fn_CitizenBehavior_ProcessPaths);
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
    g_fn_Time_Serialize = reinterpret_cast<g_fn_Time_Serialize_t>(
        ResolveExport("?Serialize@Time@System@@QEAAXAEAVSerializer@IO@@@Z"));
    fn_BucketTreeSerialize = reinterpret_cast<fn_BucketTreeSerialize_t>(
        ResolveAddr(Module::Exe, 0x140021ba0));
#ifdef _DEBUG
    printf("[EngineFuncs] EntityList deps resolved:\n");
    printf("  IBPTreeInsert       = %p\n", (void*)fn_IBPTreeInsert);
    printf("  IBPTreeRemove       = %p\n", (void*)fn_IBPTreeRemove);
    printf("  Time::Serialize     = %p\n", (void*)g_fn_Time_Serialize);
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

    // ============================================================
    // M3.5 — MenuScene::Serialize generic helpers
    // ============================================================
    fn_Geometry_Init = reinterpret_cast<fn_Geometry_Init_t>(
        ResolveAddr(Module::Exe, 0x1400b00e0));
    // Runtime ctors used by the standalone Reimpl_Geometry_Init body.
    fn_Graphics_Geometry_Ctor = reinterpret_cast<fn_Graphics_Geometry_Ctor_t>(
        ResolveExport("??0Geometry@Graphics@@QEAA@XZ"));
    fn_Video_VertexBuffer_Ctor = reinterpret_cast<fn_Video_VertexBuffer_Ctor_t>(
        ResolveExport("??0VertexBuffer@Video@@QEAA@XZ"));
    fn_Video_IndexBuffer_Ctor = reinterpret_cast<fn_Video_IndexBuffer_Ctor_t>(
        ResolveExport("??0IndexBuffer@Video@@QEAA@XZ"));
    fn_Transform_SerializeWrapper = reinterpret_cast<fn_Transform_SerializeWrapper_t>(
        ResolveAddr(Module::Exe, 0x1400988c0));
    fn_Math_Transform_Serialize = reinterpret_cast<fn_Math_Transform_Serialize_t>(
        ResolveExport("?Serialize@Transform@Math@@QEAAXAEAVSerializer@IO@@@Z"));
    fn_Math_Vector3_Serialize = reinterpret_cast<fn_Math_Vector3_Serialize_t>(
        ResolveExport("?Serialize@Vector3@Math@@QEAAXAEAVSerializer@IO@@@Z"));
    fn_Math_Vector4_Serialize = reinterpret_cast<fn_Math_Vector4_Serialize_t>(
        ResolveExport("?Serialize@Vector4@Math@@QEAAXAEAVSerializer@IO@@@Z"));
    fn_System_Color_Serialize = reinterpret_cast<fn_System_Color_Serialize_t>(
        ResolveExport("?Serialize@Color@System@@QEAAXAEAVSerializer@IO@@@Z"));
    fn_Serializer_Serialize_Float = reinterpret_cast<fn_Serializer_Serialize_Float_t>(
        ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAMPEBD@Z"));

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
    fn_Struct60B_Resize = reinterpret_cast<fn_Struct60B_Resize_t>(
        ResolveAddr(Module::Exe, 0x14001f250));
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
    printf("  Struct60B_Resize        = %p\n", (void*)fn_Struct60B_Resize);
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
    fn_Array_Resize_Stride16A = reinterpret_cast<fn_Array_Resize_t>(
        ResolveAddr(Module::Exe, 0x14001efd0));
    fn_Array_Resize_ArrayOfArrays = reinterpret_cast<fn_Array_Resize_t>(
        ResolveAddr(Module::Exe, 0x14001f0a0));
    fn_Array_Resize_Stride16B = reinterpret_cast<fn_Array_Resize_t>(
        ResolveAddr(Module::Exe, 0x14001ef10));
    fn_Array_Resize_Stride04 = reinterpret_cast<fn_Array_Resize_t>(
        ResolveAddr(Module::Exe, 0x1400021d0));
#ifdef _DEBUG
    printf("[EngineFuncs] M3.5 Path B session 5 (geometry resize helpers) resolved\n");
    printf("  Array_Resize_Stride02       = %p\n", (void*)fn_Array_Resize_Stride02);
    printf("  Array_Resize_Stride16A      = %p\n", (void*)fn_Array_Resize_Stride16A);
    printf("  Array_Resize_ArrayOfArrays  = %p\n", (void*)fn_Array_Resize_ArrayOfArrays);
    printf("  Array_Resize_Stride16B      = %p\n", (void*)fn_Array_Resize_Stride16B);
    printf("  Array_Resize_Stride04       = %p\n", (void*)fn_Array_Resize_Stride04);
#endif

    // Runtime DLL primitives — resolve via dispatch slots, fall back to mangled.
    {
        void** slot;
        slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401529f8));
        if (slot && *slot)
            fn_Serializer_GetEncoding = reinterpret_cast<fn_Serializer_GetEncoding_t>(*slot);
        else
            fn_Serializer_GetEncoding = reinterpret_cast<fn_Serializer_GetEncoding_t>(
                ResolveExport("?GetEncoding@Serializer@IO@@QEAAAEBVEncoding@2@XZ"));

        slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152a50));
        if (slot && *slot)
            fn_Serializer_Serialize_Uint = reinterpret_cast<fn_Serializer_Serialize_Uint_t>(*slot);
        else
            fn_Serializer_Serialize_Uint = reinterpret_cast<fn_Serializer_Serialize_Uint_t>(
                ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAIPEBD@Z"));

        slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151218));
        if (slot && *slot)
            fn_ExternalFactory_IsReferenceModeEnabled =
                reinterpret_cast<fn_ExternalFactory_IsReferenceModeEnabled_t>(*slot);
        else
            fn_ExternalFactory_IsReferenceModeEnabled =
                reinterpret_cast<fn_ExternalFactory_IsReferenceModeEnabled_t>(
                    ResolveExport("?IsReferenceModeEnabled@ExternalFactory@Object@@QEBA_NXZ"));

        slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151220));
        if (slot && *slot)
            fn_ExternalFactory_GetReferenceId =
                reinterpret_cast<fn_ExternalFactory_GetReferenceId_t>(*slot);
        else
            fn_ExternalFactory_GetReferenceId =
                reinterpret_cast<fn_ExternalFactory_GetReferenceId_t>(
                    ResolveExport("?GetReferenceId@ExternalFactory@Object@@QEBAIPEBVExternal@2@@Z"));

        slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151228));
        if (slot && *slot)
            fn_ExternalFactory_GetPathForReferenceId =
                reinterpret_cast<fn_ExternalFactory_GetPathForReferenceId_t>(*slot);
        else
            fn_ExternalFactory_GetPathForReferenceId =
                reinterpret_cast<fn_ExternalFactory_GetPathForReferenceId_t>(
                    ResolveExport("?GetPathForReferenceId@ExternalFactory@Object@@QEBA?AVFilename@System@@I@Z"));

        slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401529e0));
        if (slot && *slot)
            fn_Filename_AssignOp = reinterpret_cast<fn_Filename_AssignOp_t>(*slot);
        else
            fn_Filename_AssignOp = reinterpret_cast<fn_Filename_AssignOp_t>(
                ResolveExport("??4Filename@System@@QEAAAEAV01@AEBV01@@Z"));
    }

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
    fn_ExternalPtr_Serialize_Typed_7140 =
        reinterpret_cast<fn_ExternalPtr_Serialize_Typed_t>(
            ResolveAddr(Module::Exe, 0x140007140));
    g_ppTypeInfo_MaterialInstance = reinterpret_cast<uint32_t**>(
        ResolveAddr(Module::Exe, 0x140151350));

#ifdef _DEBUG
    printf("[EngineFuncs] M3.5 MenuScene::Serialize helpers resolved\n");
    printf("  Geometry_Init           = %p\n", (void*)fn_Geometry_Init);
    printf("  Graphics_Geometry_Ctor  = %p\n", (void*)fn_Graphics_Geometry_Ctor);
    printf("  Video_VertexBuffer_Ctor = %p\n", (void*)fn_Video_VertexBuffer_Ctor);
    printf("  Video_IndexBuffer_Ctor  = %p\n", (void*)fn_Video_IndexBuffer_Ctor);
    printf("  Transform_SerializeWrap = %p\n", (void*)fn_Transform_SerializeWrapper);
    printf("  Math_Transform_Serial   = %p\n", (void*)fn_Math_Transform_Serialize);
    printf("  Math_Vector3_Serial     = %p\n", (void*)fn_Math_Vector3_Serialize);
    printf("  Math_Vector4_Serial     = %p\n", (void*)fn_Math_Vector4_Serialize);
    printf("  System_Color_Serial     = %p\n", (void*)fn_System_Color_Serialize);
    printf("  Serializer_Float        = %p\n", (void*)fn_Serializer_Serialize_Float);
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
    printf("  Serializer_GetEncoding  = %p\n", (void*)fn_Serializer_GetEncoding);
    printf("  Serializer_Serialize_U  = %p\n", (void*)fn_Serializer_Serialize_Uint);
    printf("  EFactory_IsRefModeEn    = %p\n", (void*)fn_ExternalFactory_IsReferenceModeEnabled);
    printf("  EFactory_GetReferenceId = %p\n", (void*)fn_ExternalFactory_GetReferenceId);
    printf("  EFactory_GetPathForRef  = %p\n", (void*)fn_ExternalFactory_GetPathForReferenceId);
    printf("  Filename_AssignOp       = %p\n", (void*)fn_Filename_AssignOp);
    printf("  TypeInfo Mesh/Particle  = %p / %p\n",
        (void*)g_ppTypeInfo_GraphicsMesh, (void*)g_ppTypeInfo_Particle);
    printf("  TypeId Weather          = %p\n", (void*)g_pTypeId_Weather);
    // M3.5 Path B session 3
    printf("  ExtPtr_Typed_7140       = %p\n",
        (void*)fn_ExternalPtr_Serialize_Typed_7140);
    printf("  TypeInfo MaterialInst   = %p\n",
        (void*)g_ppTypeInfo_MaterialInstance);
#endif

    // ============================================================
    // M3.5 Path B session 4 — per-element 0x60B + typed-ExternalPtr-array.
    // ============================================================
    // FUN_14001ffa0 — 0x60B per-element serialize (calls FUN_140007140 inside,
    // followed by 14 primitive serializes + version-gated extras + sub_f9c0).
    fn_Struct60B_Serialize = reinterpret_cast<fn_Struct60B_Serialize_t>(
        ResolveAddr(Module::Exe, 0x14001ffa0));
    // FUN_140020150 — typed-ExternalPtr array serialize. typeInfo source is
    // .rdata slot 0x140151350 (same as FUN_140007140, Graphics::MaterialInstance).
    // Verified via disassembly: 140020340: MOV RAX,[0x140151350].
    fn_Array_Serialize_External_MaterialInstance =
        reinterpret_cast<fn_Array_Serialize_External_Typed_t>(
            ResolveAddr(Module::Exe, 0x140020150));
#ifdef _DEBUG
    printf("[EngineFuncs] M3.5 Path B session 4 (0x60B + typed-array serialize) resolved\n");
    printf("  Struct60B_Serialize             = %p\n",
        (void*)fn_Struct60B_Serialize);
    printf("  Array_Serialize_Ext_MatInstance = %p\n",
        (void*)fn_Array_Serialize_External_MaterialInstance);
#endif

    // ============================================================
    // M3.5 Path B session 6 — heightmap mesh-build path helpers.
    // ============================================================
    // sub_c1e0 (0x1400ac1e0) — top-level mesh builder. Called from
    // Geometry::Serialize step 5 to allocate VertexBuffer/IndexBuffer at
    // self+0x100/+0x120 and fill them from the deserialized heightmap.
    fn_TradeWork_sub_c1e0 = reinterpret_cast<fn_TradeWork_sub_c1e0_t>(
        ResolveAddr(Module::Exe, 0x1400ac1e0));
    // FUN_1400ac880 — per-cell vertex-block compute (writes 16-byte verts).
    fn_TradeWork_FUN_ac880 = reinterpret_cast<fn_TradeWork_FUN_ac880_t>(
        ResolveAddr(Module::Exe, 0x1400ac880));
    // helper_e270 (0x1400ae270) — vertex normal compute via heightmap deltas.
    fn_TradeWork_helper_e270 = reinterpret_cast<fn_TradeWork_helper_e270_t>(
        ResolveAddr(Module::Exe, 0x1400ae270));
    // Chunk-streaming (0x1400aad50, dispatched via JMP thunk 0x1400aed40 from
    // Geometry vtable +0x08). Per-frame walk of dirty-chunk tree at self+0xE8
    // that re-uploads regenerated vertex data via Video::VertexBuffer::Set.
    fn_Geometry_ChunkUpdate = reinterpret_cast<fn_Geometry_ChunkUpdate_t>(
        ResolveAddr(Module::Exe, 0x1400aad50));
#ifdef _DEBUG
    printf("[EngineFuncs] M3.5 Path B session 6 (heightmap mesh build) resolved\n");
    printf("  TradeWork_sub_c1e0      = %p\n", (void*)fn_TradeWork_sub_c1e0);
    printf("  TradeWork_FUN_ac880     = %p\n", (void*)fn_TradeWork_FUN_ac880);
    printf("  TradeWork_helper_e270   = %p\n", (void*)fn_TradeWork_helper_e270);
    printf("  Geometry_ChunkUpdate    = %p\n", (void*)fn_Geometry_ChunkUpdate);
#endif

    // ============================================================
    // M3.5 Path B session 7 — image-buffer heightmap upload helper.
    // ============================================================
    // sub_c780 (0x1400ac780) — final IsLoading-only step in
    // Geometry::Serialize that allocates a Video::ImageBuffer at self+0x148,
    // sets the custom NoiseUI vtable, and uploads the heightmap raw bytes
    // (self+0x140) into a texture via Device::Lock + CopyToTexture +
    // Device::Unlock. All Runtime primitives + the exe vtable address are
    // already wired (fn_ImageBuffer_*, fn_VideoDevice_Lock/Unlock,
    // g_pVideoDeviceSingleton, g_NoiseUI_ImageBufferVtable).
    fn_TradeWork_sub_c780 = reinterpret_cast<fn_TradeWork_sub_c780_t>(
        ResolveAddr(Module::Exe, 0x1400ac780));
#ifdef _DEBUG
    printf("[EngineFuncs] M3.5 Path B session 7 (image-buffer upload) resolved\n");
    printf("  TradeWork_sub_c780      = %p\n", (void*)fn_TradeWork_sub_c780);
#endif

    // ============================================================
    // M3.5 Path B session 8 — Geometry::Serialize body + extra.
    // ============================================================
    // Top-level pair driven from MenuScene::Serialize. Both bodies are also
    // reimplemented in code/Game/Game/geometry_serialize.cpp; the address
    // registry maps these exe addresses to the reimpl entry points so
    // standalone mode runs the cleaned C body when the function pointer here
    // resolves to NULL.
    fn_TradeWork_impl_04d0 = reinterpret_cast<fn_TradeWork_impl_04d0_t>(
        ResolveAddr(Module::Exe, 0x1400b04d0));
    fn_TradeWork_impl_f1a0 = reinterpret_cast<fn_TradeWork_impl_f1a0_t>(
        ResolveAddr(Module::Exe, 0x1400af1a0));

    // M3.5 Path B session 8 — promoted IO::Serializer dispatch slots.
    {
        void** sp;
        sp = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152ac8));
        if (sp && *sp) fn_Ser_Slot_SerInt_AC8 = reinterpret_cast<fn_Ser_SerInt_t>(*sp);
        sp = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152410));
        if (sp && *sp) fn_Ser_Slot_SerByte_410 = reinterpret_cast<fn_Ser_SerByte_t>(*sp);
        sp = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152a60));
        if (sp && *sp) fn_Ser_Slot_SerFloat_A60 = reinterpret_cast<fn_Ser_SerFloat_t>(*sp);
        sp = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152488));
        if (sp && *sp) fn_Ser_Slot_SerUShortArr_488 = reinterpret_cast<fn_Ser_SerUShortArr_t>(*sp);
        sp = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401523f8));
        if (sp && *sp) fn_Ser_Slot_SerSizedRaw_3F8 = reinterpret_cast<fn_Ser_SerSizedRaw_t>(*sp);
        sp = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152400));
        if (sp && *sp) fn_Ser_Slot_SerSizedRaw_400 = reinterpret_cast<fn_Ser_SerSizedRaw_t>(*sp);
        sp = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152618));
        if (sp && *sp) fn_Ser_Slot_SerByteArr_618 = reinterpret_cast<fn_Ser_SerByteArr_t>(*sp);
    }
#ifdef _DEBUG
    printf("[EngineFuncs] M3.5 Path B session 8 (Geometry::Serialize) resolved\n");
    printf("  TradeWork_impl_04d0     = %p\n", (void*)fn_TradeWork_impl_04d0);
    printf("  TradeWork_impl_f1a0     = %p\n", (void*)fn_TradeWork_impl_f1a0);
    printf("  Ser slot SerInt_AC8     = %p\n", (void*)fn_Ser_Slot_SerInt_AC8);
    printf("  Ser slot SerByte_410    = %p\n", (void*)fn_Ser_Slot_SerByte_410);
    printf("  Ser slot SerFloat_A60   = %p\n", (void*)fn_Ser_Slot_SerFloat_A60);
    printf("  Ser slot SerUShortArr   = %p\n", (void*)fn_Ser_Slot_SerUShortArr_488);
    printf("  Ser slot SerSizedRaw_3F8= %p\n", (void*)fn_Ser_Slot_SerSizedRaw_3F8);
    printf("  Ser slot SerSizedRaw_400= %p\n", (void*)fn_Ser_Slot_SerSizedRaw_400);
    printf("  Ser slot SerByteArr_618 = %p\n", (void*)fn_Ser_Slot_SerByteArr_618);
#endif

    // ============================================================
    // M3.5 — Geometry vtable draw primitives (slot 0x14015f110)
    // ============================================================
    // The Renderer / Camera / Frustum / Math / Context primitives backing the
    // 9-slot derived Geometry vtable. All resolved via ResolveExport from the
    // Runtime DLL exports. In standalone mode these provide the actual draw
    // path; in DLL mode they're consulted only by code/Game/Game/geometry_draw.cpp
    // when the original exe vtable's slot dispatch lands in our cleaned body.
    fn_Renderer_GetCullCamera = reinterpret_cast<fn_Renderer_GetCullCamera_t>(
        ResolveExport("?GetCullCamera@Renderer@Graphics@@QEAAAEBVCamera@2@XZ"));
    fn_Renderer_AddDrawItem = reinterpret_cast<fn_Renderer_AddDrawItem_t>(
        ResolveExport("?AddDrawItem@Renderer@Graphics@@QEAAXPEBVGeometry@2@PEBVMaterialInstance@2@GGMMHPEBXI@Z"));
    fn_Renderer_GetShader = reinterpret_cast<fn_Renderer_GetShader_t>(
        ResolveExport("?GetShader@Renderer@Graphics@@QEBAHXZ"));
    fn_Camera_GetFrustum = reinterpret_cast<fn_Camera_GetFrustum_t>(
        ResolveExport("?GetFrustum@Camera@Graphics@@QEBA?AVFrustum@Volume@@XZ"));
    fn_Frustum_Contains = reinterpret_cast<fn_Frustum_Contains_t>(
        ResolveExport("?Contains@Frustum@Volume@@QEBAIAEBVBox@2@I@Z"));
    fn_Frustum_MulTransform3 = reinterpret_cast<fn_Frustum_MulTransform3_t>(
        ResolveExport("??DFrustum@Volume@@QEBA?AV01@AEBVTransform3@Math@@@Z"));
    fn_Geometry_GetTransform = reinterpret_cast<fn_Geometry_GetTransform_t>(
        ResolveExport("?GetTransform@Geometry@Graphics@@QEBA?AVTransform@Math@@XZ"));
    fn_Transform_Inverse = reinterpret_cast<fn_Transform_Inverse_t>(
        ResolveExport("?Inverse@Transform@Math@@QEBA?AV12@XZ"));
    fn_Transform_AsMatrix = reinterpret_cast<fn_Transform_AsMatrix_t>(
        ResolveExport("?AsMatrix@Transform@Math@@QEBA?AVMatrix@2@XZ"));
    fn_Transform3_Copy_Ctor = reinterpret_cast<fn_Transform3_Copy_Ctor_t>(
        ResolveExport("??0Transform3@Math@@QEAA@AEBV01@@Z"));
    fn_Vector3_FromFloats_Ctor = reinterpret_cast<fn_Vector3_FromFloats_Ctor_t>(
        ResolveExport("??0Vector3@Math@@QEAA@MMM@Z"));
    fn_InterpolatedState_Transform_Lerp = reinterpret_cast<fn_InterpolatedState_Transform_Lerp_t>(
        ResolveExport("?Lerp@?$InterpolatedState@VTransform@Math@@@Graphics@@QEBA?AVTransform@Math@@AEBVScalar@4@@Z"));
    fn_Context_SetTransform = reinterpret_cast<fn_Context_SetTransform_t>(
        ResolveExport("?SetTransform@Context@Video@@QEAAXHAEBVMatrix@Math@@@Z"));
    fn_Context_SetColor = reinterpret_cast<fn_Context_SetColor_t>(
        ResolveExport("?SetColor@Context@Video@@QEAAXAEBVVector4@Math@@@Z"));
    fn_Context_SetVertexBuffer = reinterpret_cast<fn_Context_SetVertexBuffer_t>(
        ResolveExport("?SetVertexBuffer@Context@Video@@QEAAXPEBVVertexBuffer@2@HHH@Z"));
    fn_Context_SetImageBuffer = reinterpret_cast<fn_Context_SetImageBuffer_t>(
        ResolveExport("?SetImageBuffer@Context@Video@@QEAAXPEBVImageBuffer@2@H@Z"));
    fn_Context_BeginIndexedVertices = reinterpret_cast<fn_Context_BeginIndexedVertices_t>(
        ResolveExport("?BeginIndexedVertices@Context@Video@@QEAA_NHHH@Z"));
    fn_Context_EndIndexedVertices = reinterpret_cast<fn_Context_EndIndexedVertices_t>(
        ResolveExport("?EndIndexedVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z"));
    fn_Context_DrawIndexed = reinterpret_cast<fn_Context_DrawIndexed_t>(
        ResolveExport("?DrawIndexed@Context@Video@@QEAAXW4PrimitiveType@12@HHHHH@Z"));
    fn_Geometry_UpdateAttached = reinterpret_cast<fn_Geometry_UpdateAttached_t>(
        ResolveExport("?UpdateAttached@Geometry@Graphics@@UEAAXXZ"));
    fn_Geometry_GetSubTransform = reinterpret_cast<fn_Geometry_GetSubTransform_t>(
        ResolveExport("?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z"));
    fn_Geometry_GetInstanceTransform = reinterpret_cast<fn_Geometry_GetInstanceTransform_t>(
        ResolveExport("?GetInstanceTransform@Geometry@Graphics@@UEBA?AVMatrix@Math@@HM@Z"));
    fn_Geometry_SetSharedInstanceData = reinterpret_cast<fn_Geometry_SetSharedInstanceData_t>(
        ResolveExport("?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z"));
    fn_Geometry_Clean = reinterpret_cast<fn_Geometry_Clean_t>(
        ResolveExport("?Clean@Geometry@Graphics@@UEAA_NXZ"));
#ifdef _DEBUG
    printf("[EngineFuncs] M3.5 Geometry vtable draw primitives resolved\n");
    printf("  Renderer_GetCullCamera   = %p\n", (void*)fn_Renderer_GetCullCamera);
    printf("  Renderer_AddDrawItem     = %p\n", (void*)fn_Renderer_AddDrawItem);
    printf("  Renderer_GetShader       = %p\n", (void*)fn_Renderer_GetShader);
    printf("  Camera_GetFrustum        = %p\n", (void*)fn_Camera_GetFrustum);
    printf("  Frustum_Contains         = %p\n", (void*)fn_Frustum_Contains);
    printf("  Frustum_MulTransform3    = %p\n", (void*)fn_Frustum_MulTransform3);
    printf("  Geometry_GetTransform    = %p\n", (void*)fn_Geometry_GetTransform);
    printf("  Transform_Inverse        = %p\n", (void*)fn_Transform_Inverse);
    printf("  Transform_AsMatrix       = %p\n", (void*)fn_Transform_AsMatrix);
    printf("  Transform3_Copy_Ctor     = %p\n", (void*)fn_Transform3_Copy_Ctor);
    printf("  Vector3_FromFloats_Ctor  = %p\n", (void*)fn_Vector3_FromFloats_Ctor);
    printf("  InterpState_Trans_Lerp   = %p\n", (void*)fn_InterpolatedState_Transform_Lerp);
    printf("  Context_SetTransform     = %p\n", (void*)fn_Context_SetTransform);
    printf("  Context_SetColor         = %p\n", (void*)fn_Context_SetColor);
    printf("  Context_SetVertexBuffer  = %p\n", (void*)fn_Context_SetVertexBuffer);
    printf("  Context_SetImageBuffer   = %p\n", (void*)fn_Context_SetImageBuffer);
    printf("  Context_BeginIndexedVtx  = %p\n", (void*)fn_Context_BeginIndexedVertices);
    printf("  Context_EndIndexedVtx    = %p\n", (void*)fn_Context_EndIndexedVertices);
    printf("  Context_DrawIndexed      = %p\n", (void*)fn_Context_DrawIndexed);
    printf("  Geometry_UpdateAttached  = %p\n", (void*)fn_Geometry_UpdateAttached);
    printf("  Geometry_GetSubTransform = %p\n", (void*)fn_Geometry_GetSubTransform);
    printf("  Geometry_GetInstanceTr   = %p\n", (void*)fn_Geometry_GetInstanceTransform);
    printf("  Geometry_SetSharedInst   = %p\n", (void*)fn_Geometry_SetSharedInstanceData);
    printf("  Geometry_Clean           = %p\n", (void*)fn_Geometry_Clean);
#endif

    // Only Input::Device::Update and Timer globals are critical for first reimpl
    if (!fn_InputDevice_Update) {
#ifdef _DEBUG
        printf("[EngineFuncs] WARNING: Failed to resolve Input::Device::Update\n");
#endif
        return false;
    }

    return true;
}
