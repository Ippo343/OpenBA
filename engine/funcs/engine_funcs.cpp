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

// String functions
fn_String_Set_t fn_String_Set = nullptr;
fn_String_Format_t fn_String_Format = nullptr;
fn_String_Dtor_t fn_String_Dtor = nullptr;
fn_String_Ctor_Default_t fn_String_Ctor_Default = nullptr;
fn_String_Ctor_CStr_t fn_String_Ctor_CStr = nullptr;
fn_String_Ctor_Copy_t fn_String_Ctor_Copy = nullptr;

// ExternalFactory
void** g_pExternalFactorySingleton = nullptr;
fn_ExternalFactory_FindOrLoad_t fn_ExternalFactory_FindOrLoad = nullptr;
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

// FindComponentByTypeId (entity component array search)
fn_FindComponentByTypeId_t fn_FindComponentByTypeId = nullptr;

// EntityTrackingSet_Remove (BST remove)
fn_EntityTrackingSet_Remove_t fn_EntityTrackingSet_Remove = nullptr;

// ImageElement
fn_ImageElement_SetSprite_t fn_ImageElement_SetSprite = nullptr;
fn_ImageElement_SetWidthHeight_t fn_ImageElement_SetWidthHeight = nullptr;

// LabelElement
fn_LabelElement_SetSprite_t fn_LabelElement_SetSprite = nullptr;

// TextElement
fn_TextElement_SetText_t fn_TextElement_SetText = nullptr;

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
uint32_t* g_TypeId_ResourceType = nullptr;
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

// ============================================================
// Game Singletons (Phase A3)
// ============================================================
void** g_pGameWorldSingleton = nullptr;
void** g_pKeyMapSingleton = nullptr;
void** g_pEngineSingleton = nullptr;

// ============================================================
// Common Exe Functions (Phase A4)
// ============================================================
fn_FindComponent_t fn_FindComponent_StateMachine = nullptr;
fn_FindComponent_t fn_FindComponent_MeshInstance = nullptr;
fn_FindComponent_t fn_FindComponent_Destroy = nullptr;
fn_FindComponent_t fn_FindComponent_Build = nullptr;
fn_FindComponent_t fn_FindComponent_ConsumeProduce = nullptr;
fn_FindComponent_t fn_FindComponent_Resource = nullptr;
fn_FindComponent_t fn_FindComponent_Storage = nullptr;
fn_FindComponent_t fn_FindComponent_Workplace = nullptr;
fn_FindComponent_t fn_FindComponent_Radius = nullptr;
fn_FindComponent_t fn_FindComponent_BuildState = nullptr;
fn_FindComponent_t fn_FindComponent_Attendance = nullptr;
fn_FindComponent_t fn_FindComponent_Cemetery = nullptr;

fn_ExeArrayResize_t fn_ExeArrayResize = nullptr;
fn_ExeArrayResize_t fn_ExeArrayResize2 = nullptr;
fn_ExeArrayPush_t fn_ExeArrayPush = nullptr;

fn_StorageComponent_QueryCount_t fn_StorageComponent_QueryCount = nullptr;
fn_RadiusCalc_t fn_RadiusCalc = nullptr;
fn_RadiusDisplay_Remove_t fn_RadiusDisplay_Remove = nullptr;

fn_DestroyQuery_t fn_DestroyComponent_CanReclaim = nullptr;
fn_DestroyQuery_t fn_DestroyComponent_CanRebuild = nullptr;
fn_DestroyAction_t fn_DestroyComponent_Reclaim = nullptr;
fn_DestroyAction_t fn_DestroyComponent_Rebuild = nullptr;
fn_DestroyAction_t fn_DestroyComponent_Destroy = nullptr;

fn_GetActiveProductIndex_t fn_GetActiveProductIndex = nullptr;
fn_StorageCountQuery_t fn_StorageCountQuery = nullptr;
fn_CropUIPopulate_t fn_CropUI_PopulateCombo = nullptr;
fn_CropUIExpected_t fn_CropUI_GetExpectedCount = nullptr;
fn_CemeteryBurialCount_t fn_CemeteryGetBurialCount = nullptr;

// FindComponent_Immigration (ImmigrationComponent, FUN_140012020)
fn_FindComponent_t fn_FindComponent_Immigration = nullptr;
// WorkplaceComponent_Toggle (FUN_14012fc00)
fn_WorkplaceComponent_Toggle_t fn_WorkplaceComponent_Toggle = nullptr;

// FindComponent_Orchard (OrchardComponent, FUN_140005440)
fn_FindComponent_t fn_FindComponent_Orchard = nullptr;
// OrchardComponent_GetGrowth (FUN_140051640)
fn_OrchardGetGrowth_t fn_OrchardGetGrowth = nullptr;
// FindComponent_Pasture (PastureComponent, FUN_140004fe0)
fn_FindComponent_t fn_FindComponent_Pasture = nullptr;
// PastureComponent_CalcMaxCount (FUN_140128570)
fn_PastureCalcMaxCount_t fn_PastureCalcMaxCount = nullptr;
// PastureUI_UpdateSlider (FUN_1400b4fa0)
fn_PastureUI_UpdateSlider_t fn_PastureUI_UpdateSlider = nullptr;

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
fn_SaveLoadDialog_PopulateFileList_t fn_SaveLoadDialog_PopulateFileList = nullptr;
fn_EditElement_SetText_t fn_EditElement_SetText = nullptr;
fn_Input_Device_Released_t fn_Input_Device_Released = nullptr;
void** g_pInputDeviceSingleton = nullptr;
fn_ImageElement_GetSprite_t fn_ImageElement_GetSprite = nullptr;
fn_SpriteSheet_GetSpriteIndexFromName_t fn_SpriteSheet_GetSpriteIndexFromName = nullptr;
fn_SliderElement_SetParameter_t fn_SliderElement_SetParameter = nullptr;
fn_Audio_Scene_Pause_t fn_Audio_Scene_Pause = nullptr;
fn_ScenarioDialog_LoadSelected_t fn_ScenarioDialog_LoadSelected = nullptr;
fn_ModDetailsDialog_OpenUpdateDialog_t fn_ModDetailsDialog_OpenUpdateDialog = nullptr;
fn_ModDetailsDialog_DeleteMod_t fn_ModDetailsDialog_DeleteMod = nullptr;
fn_SelectResourceUI_GetResourceCount_t fn_SelectResourceUI_GetResourceCount = nullptr;
fn_SelectResourceUI_PopulateCombo_t fn_SelectResourceUI_PopulateCombo = nullptr;
fn_GoalList_GetGoal_t fn_GoalList_GetGoal = nullptr;
fn_GoalInstance_IsMet_t fn_GoalInstance_IsMet = nullptr;
fn_GoalInstance_HasFailed_t fn_GoalInstance_HasFailed = nullptr;
void*** g_ppAchievementManager = nullptr;
fn_AchievementManager_IsAchievementAwarded_t fn_AchievementManager_IsAchievementAwarded = nullptr;
// FollowDialog helpers
fn_FindComponent_t fn_FindComponent_Character = nullptr;
fn_KeyMap_Pressed_t fn_KeyMap_Pressed = nullptr;
fn_FollowDialog_Close_t fn_FollowDialog_Close = nullptr;

// UpgradeUI helpers
uint32_t* g_TypeId_UpgradeDescription = nullptr;
fn_DestroyComponent_StartUpgrade_t fn_DestroyComponent_StartUpgrade = nullptr;

fn_ResidenceUI_GoToCitizen_t fn_ResidenceUI_GoToCitizen = nullptr;
fn_ResidenceUI_ArrayPush_t fn_ResidenceUI_ArrayPush = nullptr;
fn_EventLogUI_ToggleNotification_t fn_EventLogUI_ToggleNotification = nullptr;
fn_EventLogUI_NavigateToEvent_t fn_EventLogUI_NavigateToEvent = nullptr;

// WorkplaceUI helpers
fn_FindComponent_t fn_FindComponent_BuildState2 = nullptr;
fn_WorkplaceUI_AdjustWorkerCount_t fn_WorkplaceUI_AdjustWorkerCount = nullptr;
fn_WorkplaceUI_ToggleWorkplace_t fn_WorkplaceUI_ToggleWorkplace = nullptr;
fn_CitizenList_RequestWorker_t fn_CitizenList_RequestWorker = nullptr;
fn_CitizenList_AssignWorker_t fn_CitizenList_AssignWorker = nullptr;
fn_CitizenList_RemoveWorker_t fn_CitizenList_RemoveWorker = nullptr;
fn_FindComponent_t fn_FindComponent_Position = nullptr;
uint32_t* g_TypeId_EditTool = nullptr;

// EntityUI helpers
fn_EntityUI_SetEditMode_t fn_EntityUI_SetEditMode = nullptr;
fn_EntityUI_SelectEntity_t fn_EntityUI_SelectEntity = nullptr;
fn_FindComponent_t fn_FindComponent_SM = nullptr;
fn_GetDescPage_t fn_GetDescPage = nullptr;
fn_BuildComplete_t fn_BuildComplete = nullptr;

// ============================================================
// Phase F Wave 1: MapParamsDialog / UpgradeUI / ConsumeProduceUI / HelpUI
// ============================================================
fn_MapParamsDialog_OnOk_t fn_MapParamsDialog_OnOk = nullptr;
fn_MapParamsDialog_OnNewName_t fn_MapParamsDialog_OnNewName = nullptr;
fn_GameWorld_GetMapName_t fn_GameWorld_GetMapName = nullptr;
fn_Element_SetUserData_t fn_Element_SetUserData = nullptr;
fn_UpgradeUI_Serialize_t fn_UpgradeUI_Serialize = nullptr;
fn_UpgradeUI_OnRemove_t fn_UpgradeUI_OnRemove = nullptr;
fn_ConsumeProduceUI_Slot4_t fn_ConsumeProduceUI_Slot4 = nullptr;
fn_HelpUI_Slot4_t fn_HelpUI_Slot4 = nullptr;
fn_HelpUI_Slot9_t fn_HelpUI_Slot9 = nullptr;

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
fn_GoalDialog_ArrayPush_t fn_GoalDialog_ArrayPush = nullptr;
fn_GoalDialog_ArrayPush_t fn_EventNotifyDialog_ArrayPush = nullptr;
fn_ScenarioDialog_ArrayPush_t fn_ScenarioDialog_ArrayPush = nullptr;
fn_ExternalList_GetResourceCount_t fn_ExternalList_GetResourceCount = nullptr;
uint32_t* g_TypeId_Scenario = nullptr;
fn_Element_SetPadding_t fn_Element_SetPadding = nullptr;
fn_EventNotifyDialog_ArrayRemove_t fn_EventNotifyDialog_ArrayRemove = nullptr;
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
fn_Toolbar_FindEntity_t fn_Toolbar_FindEntity = nullptr;
fn_Toolbar_CenterOnEntity_t fn_Toolbar_CenterOnEntity = nullptr;
fn_ToolbarDialog_CloseSubDialog_t fn_ToolbarDialog_CloseSubDialog = nullptr;
fn_ToolbarDialog_ChangeSpeed_t fn_ToolbarDialog_ChangeSpeed = nullptr;
fn_ToolbarDialog_UpdateButtons_t fn_ToolbarDialog_UpdateButtons = nullptr;
uint32_t* g_TypeId_ToolbarElement = nullptr;
uint32_t* g_TypeId_ToolbarDialogParent = nullptr;
uint32_t* g_TypeId_ComboElement = nullptr;
fn_GetCurrentProductIndex_t fn_GetCurrentProductIndex = nullptr;
fn_FindDialogByName_t fn_FindDialogByName = nullptr;
fn_ResolveExternalByName_t fn_ResolveExternalByName = nullptr;
fn_External_External_t fn_External_External = nullptr;
fn_External_Dtor_t fn_External_Dtor = nullptr;

// Phase F Wave 2C: EntityUI::Slot8, ResidenceUI::Slot7, EventNotifyDialog::Slot7
fn_KeyMap_GetValue_t fn_KeyMap_GetValue = nullptr;
fn_UI_GetFirstContainedDialog_t fn_UI_GetFirstContainedDialog = nullptr;
fn_EditTool_SelectEntity_t fn_EditTool_SelectEntity = nullptr;
fn_FindComponent_t fn_FindComponent_Residence = nullptr;
float* g_pAgeMultiplier = nullptr;
float* g_pNotifyTimerDuration = nullptr;
fn_Time_Ctor_Float_t fn_Time_Ctor_Float = nullptr;
fn_String_SubString_t fn_String_SubString = nullptr;
fn_String_Assign_t fn_String_Assign = nullptr;
fn_UIString_Ctor_Entry_t fn_UIString_Ctor_Entry = nullptr;
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
fn_HelpUI_ArrayResize_t fn_HelpUI_ArrayResize = nullptr;

// BuildUI support
fn_CountOccupiedBuildTiles_t fn_CountOccupiedBuildTiles = nullptr;

// NewGameDialog support
void** g_pNewGameSettings = nullptr;
fn_Random_Rand_t fn_Random_Rand = nullptr;
fn_GenerateTownName_t fn_GenerateTownName = nullptr;
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
fn_OptionsDialog_SimpleFunc_t fn_OptionsDialog_PopulateInputBindList = nullptr;
fn_OptionsDialog_OnInputBindClick_t fn_OptionsDialog_OnInputBindClick = nullptr;

// ShowDialog, DeletePtr, KeyMap input functions, dialog ctors
fn_UI_ShowDialog_t fn_UI_ShowDialog = nullptr;
fn_InternalBasePtr_DeletePtr_t fn_InternalBasePtr_DeletePtr = nullptr;
fn_KeyMap_SetInput_t fn_KeyMap_SetInput = nullptr;
fn_KeyMap_ClearInput2_t fn_KeyMap_ClearInput2 = nullptr;
fn_KeyMap_ClearInput3_t fn_KeyMap_ClearInput3 = nullptr;
fn_KeyMap_GetIdForDeviceInput_t fn_KeyMap_GetIdForDeviceInput = nullptr;
fn_OptionInputBindDialog_Ctor_t fn_OptionInputBindDialog_Ctor = nullptr;
fn_MessageDialog_Ctor_t fn_MessageDialog_Ctor = nullptr;

void** g_pAudioConfigSingleton = nullptr;
void** g_pVideoConfigSingleton = nullptr;
fn_VideoDevice_GetInterface_t fn_VideoDevice_GetInterface = nullptr;
fn_VideoDevice_GetInterfaceCount_t fn_VideoDevice_GetInterfaceCount = nullptr;
fn_Sprite_DefaultCtor_t fn_Sprite_DefaultCtor = nullptr;
fn_TabElement_SetActiveTab_t fn_TabElement_SetActiveTab = nullptr;
fn_Element_GetParent_t fn_Element_GetParent = nullptr;
int32_t* g_pGameSettings = nullptr;

// StorageUI support
fn_ExternalFactory_FindNext_t fn_ExternalFactory_FindNext = nullptr;
fn_StorageUI_SortItems_t fn_StorageUI_SortItems = nullptr;

// MapUI helpers
fn_MapUI_SetupImage_t fn_MapUI_SetupImage = nullptr;
fn_MapUI_HandleClick_t fn_MapUI_HandleClick = nullptr;

// ToolUI support
fn_DialogInstance_SetUpperLeft_t fn_DialogInstance_SetUpperLeft = nullptr;
fn_UI_BringDialogToFront_t fn_UI_BringDialogToFront = nullptr;
fn_UIDesc_AddControllers_t fn_UIDesc_AddControllers = nullptr;
uint32_t* g_TypeId_UIDescription = nullptr;

// ModDetailsDialog support
fn_Element_Hide_t fn_Element_Hide = nullptr;
fn_Modification_Find_t fn_Modification_Find = nullptr;
fn_Configuration_GetVersionString_t fn_Configuration_GetVersionString = nullptr;
fn_String_AppendStr_t fn_String_AppendStr = nullptr;
fn_String_Concat_t fn_String_Concat = nullptr;
fn_Filename_Less_t fn_Filename_Less = nullptr;
fn_ModDetails_StringArrayPush_t fn_ModDetails_StringArrayPush = nullptr;

// CitizenUI helpers
fn_FindComponent_t fn_FindComponent_CitizenAI = nullptr;
uint32_t* g_TypeId_CitizenDescription = nullptr;
uint32_t* g_TypeId_HappinessComponent = nullptr;
uint32_t* g_TypeId_ProfessionComponent = nullptr;
fn_CitizenUI_BuildIconArray_t fn_CitizenUI_BuildIconArray = nullptr;
fn_CitizenUI_UpdateIconDisplay_t fn_CitizenUI_UpdateIconDisplay = nullptr;
fn_FindProfessionComponent_t fn_FindProfessionComponent = nullptr;
fn_CitizenUI_Slot9_Original_t fn_CitizenUI_Slot9_Original = nullptr;
fn_StatusBarUI_StatusIconHandler_t fn_StatusBarUI_StatusIconHandler = nullptr;
fn_WorldGrid_FindEntity_t fn_WorldGrid_FindEntity = nullptr;

// ProductionUI support
fn_FindComponent_t fn_FindComponent_Tracker = nullptr;
fn_TrackerComponent_Rebuild_t fn_TrackerComponent_Rebuild = nullptr;

// ProfessionUI support
fn_External_GetPath_t fn_External_GetPath = nullptr;
fn_Filename_GetPathIdentifier_t fn_Filename_GetPathIdentifier = nullptr;
fn_BSTRebalance_t fn_BSTRebalance = nullptr;
fn_ProfessionUI_HandleSpinner_t fn_ProfessionUI_HandleSpinner = nullptr;
fn_ProfessionUI_HandleButton_t fn_ProfessionUI_HandleButton = nullptr;
fn_CalcWorkerSlots_t fn_CalcWorkerSlots = nullptr;

// MainMenuDialog support
fn_App_Quit_t fn_App_Quit = nullptr;
fn_Configuration_CurrentVersion_t fn_Configuration_CurrentVersion = nullptr;
fn_Configuration_CurrentBuildId_t fn_Configuration_CurrentBuildId = nullptr;
fn_InternalBasePtr_CheckPtr_t fn_InternalBasePtr_CheckPtr = nullptr;
fn_DialogInstance_Ctor_t fn_DialogInstance_Ctor = nullptr;
void** g_MainMenuDialog_BuildIdVtable = nullptr;
fn_MainMenu_Handler_t fn_MainMenu_OpenNewGame = nullptr;
fn_MainMenu_Handler_t fn_MainMenu_OpenLoad = nullptr;
fn_MainMenu_Handler_t fn_MainMenu_OpenSave = nullptr;
fn_MainMenu_Handler_t fn_MainMenu_OpenOptions = nullptr;
fn_MainMenu_Handler_t fn_MainMenu_ReturnToMenu = nullptr;
fn_MainMenu_Handler_t fn_MainMenu_OpenAchievement = nullptr;
fn_MainMenu_Handler_t fn_MainMenu_OpenScenario = nullptr;
fn_MainMenu_Handler_t fn_MainMenu_OpenGoal = nullptr;
fn_MainMenu_Handler_t fn_MainMenu_OpenMap = nullptr;
fn_MainMenu_Handler_t fn_MainMenu_OpenTutorial = nullptr;
fn_MainMenu_Handler_t fn_MainMenu_OpenMod = nullptr;

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
fn_ModDialog_RefreshDisplay_t fn_ModDialog_RefreshDisplay = nullptr;
fn_ModDialog_ToggleMod_t fn_ModDialog_ToggleMod = nullptr;
fn_ModDialog_MoveToTop_t fn_ModDialog_MoveToTop = nullptr;
fn_ModDialog_ShowDetails_t fn_ModDialog_ShowDetails = nullptr;
fn_ModDialog_Apply_t fn_ModDialog_Apply = nullptr;

// IO::Serializer (shared globals with g_ prefix)
g_fn_Serializer_Ctor_t g_fn_Serializer_Ctor = nullptr;
g_fn_Serializer_Dtor_t g_fn_Serializer_Dtor = nullptr;
g_fn_Serialize_Int_t g_fn_Serialize_Int = nullptr;
g_fn_Serializer_GetVersion_t g_fn_Serializer_GetVersion = nullptr;

// TownHallUI support
uint32_t* g_TypeId_TabElement = nullptr;
fn_TownHallUI_SubFunc_t fn_TownHallUI_InitGeneral = nullptr;
fn_TownHallUI_SubFunc_t fn_TownHallUI_InitProfessions = nullptr;
fn_TownHallUI_SubFunc_t fn_TownHallUI_InitProduction = nullptr;
fn_TownHallUI_SubFunc_t fn_TownHallUI_InitInventory = nullptr;
fn_TownHallUI_SubFunc_t fn_TownHallUI_InitGraph = nullptr;
fn_TownHallUI_SubFunc_t fn_TownHallUI_InitAgriculture = nullptr;
fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateGraph = nullptr;
fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateGeneral = nullptr;
fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateProfessions = nullptr;
fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateProduction = nullptr;
fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateAgriculture = nullptr;
fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateInventory = nullptr;
fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateAll = nullptr;
fn_TownHallUI_OnProfChange_t fn_TownHallUI_OnProfessionButton = nullptr;
fn_TownHallUI_OnProfChange_t fn_TownHallUI_OnProfessionSpinner = nullptr;
void** g_pGameSingleton = nullptr;

// SpriteCaptureUI support
uint32_t* g_TypeId_EditElement = nullptr;
uint32_t* g_TypeId_CaptureBoxElement = nullptr;

// TradeUI support
fn_FindComponent_t fn_FindComponent_Building = nullptr;
fn_Element_SetNeedsSizeCalc_t fn_Element_SetNeedsSizeCalc = nullptr;
fn_TradeUI_Serialize_t fn_TradeUI_Serialize = nullptr;
fn_TradeUI_Startup_t fn_TradeUI_Startup = nullptr;
fn_TradeUI_OnCreate_t fn_TradeUI_OnCreate = nullptr;
fn_TradeUI_OnRemove_t fn_TradeUI_OnRemove = nullptr;
fn_TradeUI_Update_t fn_TradeUI_Update = nullptr;
fn_TradeUI_RefreshDisplay_t fn_TradeUI_RefreshDisplay = nullptr;
fn_TownHallUI_SubFunc_t fn_TradeUI_RefreshSellTable = nullptr;
fn_TownHallUI_SubFunc_t fn_TradeUI_RefreshBuyTable = nullptr;
fn_TownHallUI_SubFunc_t fn_TradeUI_UpdateSellTotals = nullptr;
fn_TownHallUI_SubFunc_t fn_TradeUI_UpdateBuyTotals = nullptr;
fn_TradeUI_OnTradeButton_t fn_TradeUI_OnTradeButton = nullptr;
fn_TownHallUI_SubFunc_t fn_TradeUI_OnDismissButton = nullptr;
fn_TradeUI_OnOrderSpinner_t fn_TradeUI_OnOrderSpinner = nullptr;
fn_TradeUI_OnPurchaseButton_t fn_TradeUI_OnPurchaseButton = nullptr;
fn_TownHallUI_SubFunc_t fn_TradeUI_OnComboPurchase = nullptr;
fn_TownHallUI_SubFunc_t fn_TradeUI_OnComboOrder = nullptr;
fn_TradeUI_OnStorageSpinner_t fn_TradeUI_OnStorageSpinner = nullptr;
fn_TradeUI_OnPurchaseButton_t fn_TradeUI_OnSellSpinner = nullptr;
fn_TradeUI_OnPurchaseButton_t fn_TradeUI_OnBuySpinner = nullptr;
fn_TradeUI_OnPurchaseButton_t fn_TradeUI_OnOrderAmountSpinner = nullptr;
uint32_t* g_TypeId_TableElement = nullptr;
uint32_t* g_TypeId_SpinnerElement = nullptr;
uint32_t* g_TypeId_TextElement = nullptr;
uint32_t* g_TypeId_SelectDescription = nullptr;
fn_TradeUI_SubFunc_t fn_TradeUI_RefreshOrderQuantityDisplay = nullptr;
fn_TradeUI_SubFunc_t fn_TradeUI_RefreshSellSpinnerValues = nullptr;
fn_TradeUI_SubFunc_t fn_TradeUI_RefreshOrderDisplay = nullptr;
uintptr_t g_NoiseUI_ImageBufferVtable = 0;
fn_NoiseUI_SetupImage_t fn_NoiseUI_SetupImage = nullptr;
fn_NoiseUI_GenerateNoise_t fn_NoiseUI_GenerateNoise = nullptr;
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
fn_DebugUI_SetDayLighting_t fn_DebugUI_SetDayLighting = nullptr;
uint32_t* g_TypeId_DiseaseDescription = nullptr;
uint32_t* g_TypeId_DrawableType = nullptr;
fn_Game_SpawnResources_t fn_Game_SpawnResources = nullptr;
fn_Game_TriggerDisasterByType_t fn_Game_TriggerDisasterByType = nullptr;
fn_Game_KillAllCitizens_t fn_Game_KillAllCitizens = nullptr;
fn_Game_SpawnItemsInStorage_t fn_Game_SpawnItemsInStorage = nullptr;
fn_Game_SpawnCitizens_t fn_Game_SpawnCitizens = nullptr;
fn_DebugUI_TriggerDisasterOnBuilding_t fn_DebugUI_TriggerDisasterOnBuilding = nullptr;
fn_Game_StartRandomFire_t fn_Game_StartRandomFire = nullptr;
fn_Game_SpawnTornado_t fn_Game_SpawnTornado = nullptr;
fn_Game_InfectRandomCitizen_t fn_Game_InfectRandomCitizen = nullptr;
fn_DebugUI_OnDiseaseComboChanged_t fn_DebugUI_OnDiseaseComboChanged = nullptr;
fn_DebugUI_OnDrawComboChanged_t fn_DebugUI_OnDrawComboChanged = nullptr;
fn_DebugUI_OnDaySliderChanged_t fn_DebugUI_OnDaySliderChanged = nullptr;
fn_DebugUI_WriteLightingColors_t fn_DebugUI_WriteLightingColors = nullptr;
fn_Game_GetMonthFromTime_t fn_Game_GetMonthFromTime = nullptr;

// --- SaveLoadDialog::Load helpers ---
fn_GameUpdateLoop_Ctor_t fn_GameUpdateLoop_Ctor = nullptr;
fn_LoaderLoop_Ctor_t fn_LoaderLoop_Ctor = nullptr;
fn_DialogLoaderLoopT_Ctor_t fn_DialogLoaderLoopT_Ctor = nullptr;
fn_Random_Default_Ctor_t fn_Random_Default_Ctor = nullptr;
void** g_GameLoadDialogLoopT_Vtable = nullptr;
void** g_GameLoaderLoop_Vtable = nullptr;
fn_SetEngineNextLoop_t fn_SetEngineNextLoop = nullptr;
fn_SetDialogLoaderNextLoop_t fn_SetDialogLoaderNextLoop = nullptr;
fn_GetLoadingSoundString_t fn_GetLoadingSoundString = nullptr;
fn_MenuDialogLoopT_Ctor_t fn_MenuDialogLoopT_Ctor = nullptr;
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
    g_TypeId_ResourceType = reinterpret_cast<uint32_t*>(
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
    g_pEngineSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140151440));

    // ================================================================
    // Phase A4: Common Exe Functions (by RVA)
    // ================================================================
    fn_FindComponent_StateMachine = reinterpret_cast<fn_FindComponent_t>(
        ResolveAddr(Module::Exe, 0x14000b6f0));
    fn_FindComponent_Destroy = reinterpret_cast<fn_FindComponent_t>(
        ResolveAddr(Module::Exe, 0x140005780));
    fn_FindComponent_Build = reinterpret_cast<fn_FindComponent_t>(
        ResolveAddr(Module::Exe, 0x140004680));
    fn_FindComponent_MeshInstance = reinterpret_cast<fn_FindComponent_t>(
        ResolveAddr(Module::Exe, 0x140002f90));
    fn_FindComponent_Storage = reinterpret_cast<fn_FindComponent_t>(
        ResolveAddr(Module::Exe, 0x140006270));
    fn_FindComponent_BuildState = reinterpret_cast<fn_FindComponent_t>(
        ResolveAddr(Module::Exe, 0x140003ea0));
    fn_FindComponent_Workplace = reinterpret_cast<fn_FindComponent_t>(
        ResolveAddr(Module::Exe, 0x140003050));
    fn_FindComponent_Attendance = reinterpret_cast<fn_FindComponent_t>(
        ResolveAddr(Module::Exe, 0x1400105e0));
    fn_FindComponent_Cemetery = reinterpret_cast<fn_FindComponent_t>(
        ResolveAddr(Module::Exe, 0x140010a00));

    fn_ExeArrayResize = reinterpret_cast<fn_ExeArrayResize_t>(
        ResolveAddr(Module::Exe, 0x1400032e0));
    fn_ExeArrayResize2 = reinterpret_cast<fn_ExeArrayResize_t>(
        ResolveAddr(Module::Exe, 0x1400021d0));
    fn_ExeArrayPush = reinterpret_cast<fn_ExeArrayPush_t>(
        ResolveAddr(Module::Exe, 0x140003600));

    fn_StorageComponent_QueryCount = reinterpret_cast<fn_StorageComponent_QueryCount_t>(
        ResolveAddr(Module::Exe, 0x14012cf10));
    fn_RadiusCalc = reinterpret_cast<fn_RadiusCalc_t>(
        ResolveAddr(Module::Exe, 0x140055430));
    fn_RadiusDisplay_Remove = reinterpret_cast<fn_RadiusDisplay_Remove_t>(
        ResolveAddr(Module::Exe, 0x1400b0d00));

    fn_DestroyComponent_CanReclaim = reinterpret_cast<fn_DestroyQuery_t>(
        ResolveAddr(Module::Exe, 0x14011e690));
    fn_DestroyComponent_CanRebuild = reinterpret_cast<fn_DestroyQuery_t>(
        ResolveAddr(Module::Exe, 0x14011e600));
    fn_DestroyComponent_Reclaim = reinterpret_cast<fn_DestroyAction_t>(
        ResolveAddr(Module::Exe, 0x14011e400));
    fn_DestroyComponent_Rebuild = reinterpret_cast<fn_DestroyAction_t>(
        ResolveAddr(Module::Exe, 0x14011e470));
    fn_DestroyComponent_Destroy = reinterpret_cast<fn_DestroyAction_t>(
        ResolveAddr(Module::Exe, 0x14011e4d0));

    fn_GetActiveProductIndex = reinterpret_cast<fn_GetActiveProductIndex_t>(
        ResolveAddr(Module::Exe, 0x140119470));
    fn_StorageCountQuery = reinterpret_cast<fn_StorageCountQuery_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));
    fn_CropUI_PopulateCombo = reinterpret_cast<fn_CropUIPopulate_t>(
        ResolveAddr(Module::Exe, 0x140078830));
    fn_CropUI_GetExpectedCount = reinterpret_cast<fn_CropUIExpected_t>(
        ResolveAddr(Module::Exe, 0x140078a40));
    fn_CemeteryGetBurialCount = reinterpret_cast<fn_CemeteryBurialCount_t>(
        ResolveAddr(Module::Exe, 0x140046dd0));

    // FindComponentByTypeId — entity component array search (FUN_1400c3680)
    fn_FindComponentByTypeId = reinterpret_cast<fn_FindComponentByTypeId_t>(
        ResolveAddr(Module::Exe, 0x1400c3680));

    // EntityTrackingSet_Remove — BST remove (FUN_14002c690)
    fn_EntityTrackingSet_Remove = reinterpret_cast<fn_EntityTrackingSet_Remove_t>(
        ResolveAddr(Module::Exe, 0x14002c690));

    // FindComponent_Immigration — FUN_140012020 (searches for DAT_1401c4480)
    fn_FindComponent_Immigration = reinterpret_cast<fn_FindComponent_t>(
        ResolveAddr(Module::Exe, 0x140012020));

    // WorkplaceComponent_Toggle — FUN_14012fc00 (toggles workplace enabled state)
    fn_WorkplaceComponent_Toggle = reinterpret_cast<fn_WorkplaceComponent_Toggle_t>(
        ResolveAddr(Module::Exe, 0x14012fc00));

    // FindComponent_Orchard — FUN_140005440 (searches entity +0x30/+0x38 for DAT_1401c46b0)
    fn_FindComponent_Orchard = reinterpret_cast<fn_FindComponent_t>(
        ResolveAddr(Module::Exe, 0x140005440));
    // OrchardComponent_GetGrowth — FUN_140051640 (returns average growth ratio)
    fn_OrchardGetGrowth = reinterpret_cast<fn_OrchardGetGrowth_t>(
        ResolveAddr(Module::Exe, 0x140051640));
    // FindComponent_Pasture — FUN_140004fe0 (searches for DAT_1401c7ba0)
    fn_FindComponent_Pasture = reinterpret_cast<fn_FindComponent_t>(
        ResolveAddr(Module::Exe, 0x140004fe0));
    // PastureComponent_CalcMaxCount — FUN_140128570
    fn_PastureCalcMaxCount = reinterpret_cast<fn_PastureCalcMaxCount_t>(
        ResolveAddr(Module::Exe, 0x140128570));
    // PastureUI_UpdateSlider — FUN_1400b4fa0
    fn_PastureUI_UpdateSlider = reinterpret_cast<fn_PastureUI_UpdateSlider_t>(
        ResolveAddr(Module::Exe, 0x1400b4fa0));

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

    // SaveLoadDialog::PopulateFileList — exe function at 0x1400b6ce0
    fn_SaveLoadDialog_PopulateFileList = reinterpret_cast<fn_SaveLoadDialog_PopulateFileList_t>(
        ResolveAddr(Module::Exe, 0x1400b6ce0));

    fn_EditElement_SetText = reinterpret_cast<fn_EditElement_SetText_t>(
        ResolveExport("?SetText@EditElement@UI@@QEAAXVString@2@@Z"));
    if (!fn_EditElement_SetText)
        fn_EditElement_SetText = reinterpret_cast<fn_EditElement_SetText_t>(
            ResolveAddr(Module::Runtime, 0x180042070));

    fn_Input_Device_Released = reinterpret_cast<fn_Input_Device_Released_t>(
        ResolveExport("?Released@Device@Input@@QEBA_NW4InputDevice@12@HH@Z"));

    // Input::Device singleton: exe exref at 0x140151b80 -> pointer to _singleton in Runtime
    {
        void*** exref = reinterpret_cast<void***>(ResolveAddr(Module::Exe, 0x140151b80));
        if (exref && *exref) {
            g_pInputDeviceSingleton = *exref;
        }
    }

    fn_ImageElement_GetSprite = reinterpret_cast<fn_ImageElement_GetSprite_t>(
        ResolveExport("?GetSprite@ImageElement@UI@@QEBA?AVSprite@2@XZ"));
    if (!fn_ImageElement_GetSprite)
        fn_ImageElement_GetSprite = reinterpret_cast<fn_ImageElement_GetSprite_t>(
            ResolveAddr(Module::Runtime, 0x1801182a0));

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

    // ScenarioDialog helper — exe function at 0x140077cb0
    fn_ScenarioDialog_LoadSelected = reinterpret_cast<fn_ScenarioDialog_LoadSelected_t>(
        ResolveAddr(Module::Exe, 0x140077cb0));

    // ModDetailsDialog helpers — exe functions
    fn_ModDetailsDialog_OpenUpdateDialog = reinterpret_cast<fn_ModDetailsDialog_OpenUpdateDialog_t>(
        ResolveAddr(Module::Exe, 0x14006d400));
    fn_ModDetailsDialog_DeleteMod = reinterpret_cast<fn_ModDetailsDialog_DeleteMod_t>(
        ResolveAddr(Module::Exe, 0x14006d550));

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

    // FindComponent_Character — FUN_140003f60
    fn_FindComponent_Character = reinterpret_cast<fn_FindComponent_t>(
        ResolveAddr(Module::Exe, 0x140003f60));

    // Input::KeyMap::Pressed — from exe dispatch table at 0x140151758
    fn_KeyMap_Pressed = reinterpret_cast<fn_KeyMap_Pressed_t>(
        ResolveExport("?Pressed@KeyMap@Input@@QEBA_NHH@Z"));
    if (!fn_KeyMap_Pressed) {
        uintptr_t dispatchAddr = ResolveAddr(Module::Exe, 0x140151758);
        if (dispatchAddr)
            fn_KeyMap_Pressed = *reinterpret_cast<fn_KeyMap_Pressed_t*>(dispatchAddr);
    }

    // FollowDialog close helper — FUN_140068f00
    fn_FollowDialog_Close = reinterpret_cast<fn_FollowDialog_Close_t>(
        ResolveAddr(Module::Exe, 0x140068f00));

    // UpgradeDescription type ID at DAT_1401c5868
    g_TypeId_UpgradeDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5868));

    // DestroyComponent_StartUpgrade — FUN_14011dfa0
    fn_DestroyComponent_StartUpgrade = reinterpret_cast<fn_DestroyComponent_StartUpgrade_t>(
        ResolveAddr(Module::Exe, 0x14011dfa0));

    // ResidenceUI helper (exe function)
    fn_ResidenceUI_GoToCitizen = reinterpret_cast<fn_ResidenceUI_GoToCitizen_t>(
        ResolveAddr(Module::Exe, 0x1400b56a0));
    // ResidenceUI array push (0x30-byte elements) — FUN_140021020
    fn_ResidenceUI_ArrayPush = reinterpret_cast<fn_ResidenceUI_ArrayPush_t>(
        ResolveAddr(Module::Exe, 0x140021020));

    // EventLogUI helpers (exe functions)
    fn_EventLogUI_ToggleNotification = reinterpret_cast<fn_EventLogUI_ToggleNotification_t>(
        ResolveAddr(Module::Exe, 0x140067320));
    fn_EventLogUI_NavigateToEvent = reinterpret_cast<fn_EventLogUI_NavigateToEvent_t>(
        ResolveAddr(Module::Exe, 0x140067370));

    // WorkplaceUI helpers
    fn_FindComponent_BuildState2 = reinterpret_cast<fn_FindComponent_t>(
        ResolveAddr(Module::Exe, 0x140002e10));
    fn_WorkplaceUI_AdjustWorkerCount = reinterpret_cast<fn_WorkplaceUI_AdjustWorkerCount_t>(
        ResolveAddr(Module::Exe, 0x140087610));
    fn_WorkplaceUI_ToggleWorkplace = reinterpret_cast<fn_WorkplaceUI_ToggleWorkplace_t>(
        ResolveAddr(Module::Exe, 0x1400876d0));
    fn_CitizenList_RequestWorker = reinterpret_cast<fn_CitizenList_RequestWorker_t>(
        ResolveAddr(Module::Exe, 0x14008d100));
    fn_CitizenList_AssignWorker = reinterpret_cast<fn_CitizenList_AssignWorker_t>(
        ResolveAddr(Module::Exe, 0x14008c3d0));
    fn_CitizenList_RemoveWorker = reinterpret_cast<fn_CitizenList_RemoveWorker_t>(
        ResolveAddr(Module::Exe, 0x140091180));
    fn_FindComponent_Position = reinterpret_cast<fn_FindComponent_t>(
        ResolveAddr(Module::Exe, 0x14000d6c0));

    // EditTool type ID — from the exe exref used in EntityUI
    // The original uses _typeInfo_exref which is the same exref as EditTool's type
    // We resolve it from the address seen in EntityUI::Slot7
    g_TypeId_EditTool = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4158));

    // EntityUI helpers
    fn_EntityUI_SetEditMode = reinterpret_cast<fn_EntityUI_SetEditMode_t>(
        ResolveAddr(Module::Exe, 0x140102b60));
    fn_EntityUI_SelectEntity = reinterpret_cast<fn_EntityUI_SelectEntity_t>(
        ResolveAddr(Module::Exe, 0x140102e30));
    fn_FindComponent_SM = reinterpret_cast<fn_FindComponent_t>(
        ResolveAddr(Module::Exe, 0x14000a170));
    fn_GetDescPage = reinterpret_cast<fn_GetDescPage_t>(
        ResolveAddr(Module::Exe, 0x140011320));
    fn_BuildComplete = reinterpret_cast<fn_BuildComplete_t>(
        ResolveAddr(Module::Exe, 0x140109390));

    // Phase F Wave 1: MapParamsDialog / UpgradeUI / ConsumeProduceUI / HelpUI
    fn_MapParamsDialog_OnOk = reinterpret_cast<fn_MapParamsDialog_OnOk_t>(
        ResolveAddr(Module::Exe, 0x14006b690));
    fn_MapParamsDialog_OnNewName = reinterpret_cast<fn_MapParamsDialog_OnNewName_t>(
        ResolveAddr(Module::Exe, 0x14006b700));
    fn_GameWorld_GetMapName = reinterpret_cast<fn_GameWorld_GetMapName_t>(
        ResolveAddr(Module::Exe, 0x140011f90));
    fn_Element_SetUserData = reinterpret_cast<fn_Element_SetUserData_t>(
        ResolveAddr(Module::Runtime, 0x180115560));
    fn_UpgradeUI_Serialize = reinterpret_cast<fn_UpgradeUI_Serialize_t>(
        ResolveAddr(Module::Exe, 0x140086d90));
    fn_UpgradeUI_OnRemove = reinterpret_cast<fn_UpgradeUI_OnRemove_t>(
        ResolveAddr(Module::Exe, 0x140086c90));
    fn_ConsumeProduceUI_Slot4 = reinterpret_cast<fn_ConsumeProduceUI_Slot4_t>(
        ResolveAddr(Module::Exe, 0x14005F3D0));
    fn_HelpUI_Slot4 = reinterpret_cast<fn_HelpUI_Slot4_t>(
        ResolveAddr(Module::Exe, 0x14006A500));
    fn_HelpUI_Slot9 = reinterpret_cast<fn_HelpUI_Slot9_t>(
        ResolveAddr(Module::Exe, 0x14006A290));

    // Phase F Wave 2A: GoalDialog / EventNotifyDialog / ScenarioDialog
    fn_GoalList_GetGoalCount = reinterpret_cast<fn_GoalList_GetGoalCount_t>(
        ResolveExport("?GetGoalCount@GoalList@Achievement@@QEBAHXZ"));
    fn_Dialog_GetTemplate = reinterpret_cast<fn_Dialog_GetTemplate_t>(
        ResolveExport("?GetTemplate@Dialog@UI@@QEBA?AV?$ExternalPtr@VElementDescription@UI@@@Object@@H@Z"));
    fn_Goal_GetText = reinterpret_cast<fn_Goal_GetText_t>(
        ResolveExport("?GetText@Goal@Achievement@@QEBA?AVString@System@@XZ"));
    fn_Achievement_GetTitle = reinterpret_cast<fn_Achievement_GetTitle_t>(
        ResolveExport("?GetTitle@Achievement@1@QEBA?AVString@System@@XZ"));
    fn_Achievement_GetDescription = reinterpret_cast<fn_Achievement_GetDescription_t>(
        ResolveExport("?GetDescription@Achievement@1@QEBA?AVString@System@@XZ"));
    fn_Achievement_GetSpriteName = reinterpret_cast<fn_Achievement_GetSpriteName_t>(
        ResolveExport("?GetSpriteName@Achievement@1@QEBA?AVString@System@@XZ"));
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
    fn_GoalDialog_ArrayPush = reinterpret_cast<fn_GoalDialog_ArrayPush_t>(
        ResolveAddr(Module::Exe, 0x140011750));
    fn_EventNotifyDialog_ArrayPush = fn_GoalDialog_ArrayPush;
    fn_ScenarioDialog_ArrayPush = reinterpret_cast<fn_ScenarioDialog_ArrayPush_t>(
        ResolveAddr(Module::Exe, 0x140013b60));
    fn_ExternalList_GetResourceCount = reinterpret_cast<fn_ExternalList_GetResourceCount_t>(
        ResolveExport("?GetResourceCount@ExternalList@Object@@QEBAHXZ"));
    fn_Element_SetPadding = reinterpret_cast<fn_Element_SetPadding_t>(
        ResolveExport("?SetPadding@Element@UI@@QEAAXHHHH@Z"));
    fn_EventNotifyDialog_ArrayRemove = reinterpret_cast<fn_EventNotifyDialog_ArrayRemove_t>(
        ResolveAddr(Module::Exe, 0x140011670));

    // Scenario type ID: exref at 0x140151510 -> pointer -> uint32_t typeId
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151510));
        if (exref && *exref) {
            g_TypeId_Scenario = reinterpret_cast<uint32_t*>(*exref);
        }
    }

    fn_Sprite_Ctor = reinterpret_cast<fn_Sprite_Ctor_t>(
        ResolveExport("??0Sprite@UI@@QEAA@V?$ExternalPtr@VSpriteSheet@UI@@@Object@@AEBVString@System@@@Z"));
    fn_UIString_Ctor = reinterpret_cast<fn_UIString_Ctor_t>(
        ResolveExport("??0String@UI@@QEAA@V?$ExternalPtr@VStringTable@UI@@@Object@@AEBV0System@@@Z"));
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
    fn_Toolbar_ShowGroup = reinterpret_cast<fn_Toolbar_ShowGroup_t>(
        ResolveAddr(Module::Exe, 0x140102b60));
    fn_Toolbar_Utility = reinterpret_cast<fn_Toolbar_Utility_t>(
        ResolveAddr(Module::Exe, 0x140101a80));
    fn_Toolbar_ToggleAll = reinterpret_cast<fn_Toolbar_ToggleAll_t>(
        ResolveAddr(Module::Exe, 0x140101ce0));
    fn_Toolbar_FindEntity = reinterpret_cast<fn_Toolbar_FindEntity_t>(
        ResolveAddr(Module::Exe, 0x140088d40));
    fn_Toolbar_CenterOnEntity = reinterpret_cast<fn_Toolbar_CenterOnEntity_t>(
        ResolveAddr(Module::Exe, 0x140103040));
    fn_ToolbarDialog_CloseSubDialog = reinterpret_cast<fn_ToolbarDialog_CloseSubDialog_t>(
        ResolveAddr(Module::Exe, 0x1400b9730));
    fn_ToolbarDialog_ChangeSpeed = reinterpret_cast<fn_ToolbarDialog_ChangeSpeed_t>(
        ResolveAddr(Module::Exe, 0x1400b91b0));
    fn_ToolbarDialog_UpdateButtons = reinterpret_cast<fn_ToolbarDialog_UpdateButtons_t>(
        ResolveAddr(Module::Exe, 0x1400b8fd0));
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
    // ConsumeProduceUI helper: get current product index
    fn_GetCurrentProductIndex = reinterpret_cast<fn_GetCurrentProductIndex_t>(
        ResolveAddr(Module::Exe, 0x140119470));
    // TutorialGoalDialog helpers
    fn_FindDialogByName = reinterpret_cast<fn_FindDialogByName_t>(
        ResolveAddr(Module::Exe, 0x1400e1600));
    fn_ResolveExternalByName = reinterpret_cast<fn_ResolveExternalByName_t>(
        ResolveAddr(Module::Exe, 0x14001b1d0));
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
    fn_FindComponent_Residence = fn_FindComponent_Storage;  // ResidenceUI uses StorageComponent
    g_pAgeMultiplier = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x140172198));
    g_pNotifyTimerDuration = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x1401721cc));
    fn_Time_Ctor_Float = reinterpret_cast<fn_Time_Ctor_Float_t>(
        ResolveExport("??0Time@System@@QEAA@M@Z"));
    fn_String_SubString = reinterpret_cast<fn_String_SubString_t>(
        ResolveExport("?SubString@String@System@@QEBA?AV12@H@Z"));
    fn_String_Assign = reinterpret_cast<fn_String_Assign_t>(
        ResolveExport("?operator=@String@System@@QEAAAEAV12@AEBV12@@Z"));
    // UI::String::String(ExternalPtr<StringTable>) — single-arg ctor, dispatch at 0x140151300
    {
        void** dispatch = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151300));
        if (dispatch && *dispatch) {
            fn_UIString_Ctor_Entry = reinterpret_cast<fn_UIString_Ctor_Entry_t>(*dispatch);
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
    fn_HelpUI_ArrayResize = reinterpret_cast<fn_HelpUI_ArrayResize_t>(
        ResolveAddr(Module::Exe, 0x140011d50));

    // --- BuildUI support ---
    fn_CountOccupiedBuildTiles = reinterpret_cast<fn_CountOccupiedBuildTiles_t>(
        ResolveAddr(Module::Exe, 0x140118330));

    // --- OptionsDialog support — Runtime DLL exports ---
    fn_VideoDevice_ValidateConfiguration = reinterpret_cast<fn_VideoDevice_ValidateConfiguration_t>(
        ResolveExport("?ValidateConfiguration@Device@Video@@QEAAXAEAH0000000@Z"));
    fn_VideoDevice_GetMultisampleModeCount = reinterpret_cast<fn_VideoDevice_GetMultisampleModeCount_t>(
        ResolveExport("?GetMultisampleModeCount@Device@Video@@QEBAHXZ"));
    fn_VideoDevice_IsMultisampleModeSupported = reinterpret_cast<fn_VideoDevice_IsMultisampleModeSupported_t>(
        ResolveExport("?IsMultisampleModeSupported@Device@Video@@QEBA_NH@Z"));

    // --- OptionsDialog exe helpers (OnOk/OnCancel/OnApply reimplemented) ---
    fn_OptionsDialog_PopulateInputBindList = reinterpret_cast<fn_OptionsDialog_SimpleFunc_t>(
        ResolveAddr(Module::Exe, 0x140073580));
    fn_OptionsDialog_OnInputBindClick = reinterpret_cast<fn_OptionsDialog_OnInputBindClick_t>(
        ResolveAddr(Module::Exe, 0x1400729a0));
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
    fn_Element_GetParent = reinterpret_cast<fn_Element_GetParent_t>(
        ResolveAddr(Module::Exe, 0x140010460));
    g_pGameSettings = reinterpret_cast<int32_t*>(ResolveAddr(Module::Exe, 0x1401c3de0));
    // Audio::Configuration singleton: exe exref at 0x140151b78 -> pointer to singleton
    {
        void*** exref = reinterpret_cast<void***>(ResolveAddr(Module::Exe, 0x140151b78));
        if (exref && *exref) {
            g_pAudioConfigSingleton = *exref;
        }
    }
    // Video::Configuration singleton: exe exref at 0x140151b70 -> pointer to singleton
    {
        void*** exref = reinterpret_cast<void***>(ResolveAddr(Module::Exe, 0x140151b70));
        if (exref && *exref) {
            g_pVideoConfigSingleton = *exref;
        }
    }

    // --- StorageUI support ---
    fn_ExternalFactory_FindNext = reinterpret_cast<fn_ExternalFactory_FindNext_t>(
        ResolveExport("?FindNext@ExternalFactory@Object@@QEBAPEBVExternal@2@IPEBV32@@Z"));
    if (!fn_ExternalFactory_FindNext) {
        fn_ExternalFactory_FindNext = reinterpret_cast<fn_ExternalFactory_FindNext_t>(
            ResolveAddr(Module::Runtime, 0x1800be9d0));
    }
    fn_StorageUI_SortItems = reinterpret_cast<fn_StorageUI_SortItems_t>(
        ResolveAddr(Module::Exe, 0x1400213f0));

    // --- MapUI helpers ---
    fn_MapUI_SetupImage = reinterpret_cast<fn_MapUI_SetupImage_t>(
        ResolveAddr(Module::Exe, 0x14006cb00));
    fn_MapUI_HandleClick = reinterpret_cast<fn_MapUI_HandleClick_t>(
        ResolveAddr(Module::Exe, 0x14006c850));

    // --- CitizenUI helpers ---
    fn_FindComponent_CitizenAI = reinterpret_cast<fn_FindComponent_t>(
        ResolveAddr(Module::Exe, 0x140009790));
    g_TypeId_CitizenDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7190));
    g_TypeId_HappinessComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7e08));
    g_TypeId_ProfessionComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c70e8));
    fn_CitizenUI_BuildIconArray = reinterpret_cast<fn_CitizenUI_BuildIconArray_t>(
        ResolveAddr(Module::Exe, 0x14006abf0));
    fn_CitizenUI_UpdateIconDisplay = reinterpret_cast<fn_CitizenUI_UpdateIconDisplay_t>(
        ResolveAddr(Module::Exe, 0x14006aed0));
    fn_FindProfessionComponent = reinterpret_cast<fn_FindProfessionComponent_t>(
        ResolveAddr(Module::Exe, 0x140011050));
    fn_WorldGrid_FindEntity = reinterpret_cast<fn_WorldGrid_FindEntity_t>(
        ResolveAddr(Module::Exe, 0x1400f9a60));
    fn_CitizenUI_Slot9_Original = reinterpret_cast<fn_CitizenUI_Slot9_Original_t>(
        ResolveAddr(Module::Exe, 0x1400650F0));
    fn_StatusBarUI_StatusIconHandler = reinterpret_cast<fn_StatusBarUI_StatusIconHandler_t>(
        ResolveAddr(Module::Exe, 0x14007a120));

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
    fn_FindComponent_Tracker = reinterpret_cast<fn_FindComponent_t>(
        ResolveAddr(Module::Exe, 0x140013320));
    fn_TrackerComponent_Rebuild = reinterpret_cast<fn_TrackerComponent_Rebuild_t>(
        ResolveAddr(Module::Exe, 0x14000d9d0));

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
            ResolveAddr(Module::Runtime, 0x180069150));
    }
    fn_BSTRebalance = reinterpret_cast<fn_BSTRebalance_t>(
        ResolveAddr(Module::Exe, 0x140013480));
    fn_ProfessionUI_HandleSpinner = reinterpret_cast<fn_ProfessionUI_HandleSpinner_t>(
        ResolveAddr(Module::Exe, 0x1400763c0));
    fn_ProfessionUI_HandleButton = reinterpret_cast<fn_ProfessionUI_HandleButton_t>(
        ResolveAddr(Module::Exe, 0x140076a30));
    fn_CalcWorkerSlots = reinterpret_cast<fn_CalcWorkerSlots_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));

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

    // MainMenuDialog Slot8 handler functions (private methods in exe)
    fn_MainMenu_OpenNewGame    = reinterpret_cast<fn_MainMenu_Handler_t>(ResolveAddr(Module::Exe, 0x1400B3620));
    fn_MainMenu_OpenLoad       = reinterpret_cast<fn_MainMenu_Handler_t>(ResolveAddr(Module::Exe, 0x1400B3770));
    fn_MainMenu_OpenSave       = reinterpret_cast<fn_MainMenu_Handler_t>(ResolveAddr(Module::Exe, 0x1400B38B0));
    fn_MainMenu_OpenOptions    = reinterpret_cast<fn_MainMenu_Handler_t>(ResolveAddr(Module::Exe, 0x1400B3A10));
    fn_MainMenu_ReturnToMenu   = reinterpret_cast<fn_MainMenu_Handler_t>(ResolveAddr(Module::Exe, 0x1400B4790));
    fn_MainMenu_OpenAchievement = reinterpret_cast<fn_MainMenu_Handler_t>(ResolveAddr(Module::Exe, 0x1400B34C0));
    fn_MainMenu_OpenScenario   = reinterpret_cast<fn_MainMenu_Handler_t>(ResolveAddr(Module::Exe, 0x1400B3380));
    fn_MainMenu_OpenGoal       = reinterpret_cast<fn_MainMenu_Handler_t>(ResolveAddr(Module::Exe, 0x1400B3220));
    fn_MainMenu_OpenMap        = reinterpret_cast<fn_MainMenu_Handler_t>(ResolveAddr(Module::Exe, 0x1400B3120));
    fn_MainMenu_OpenTutorial   = reinterpret_cast<fn_MainMenu_Handler_t>(ResolveAddr(Module::Exe, 0x1400B2FE0));
    fn_MainMenu_OpenMod        = reinterpret_cast<fn_MainMenu_Handler_t>(ResolveAddr(Module::Exe, 0x1400B2EA0));

    // --- NewGameDialog support ---
    g_pNewGameSettings = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401c3de0));
    fn_Random_Rand = reinterpret_cast<fn_Random_Rand_t>(
        ResolveExport("?Rand@Random@System@@QEAAII@Z"));
    if (!fn_Random_Rand) {
        fn_Random_Rand = reinterpret_cast<fn_Random_Rand_t>(
            ResolveAddr(Module::Runtime, 0x1800c8040));
    }
    fn_GenerateTownName = reinterpret_cast<fn_GenerateTownName_t>(
        ResolveAddr(Module::Exe, 0x140099ac0));
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
    fn_UIDesc_AddControllers = reinterpret_cast<fn_UIDesc_AddControllers_t>(
        ResolveAddr(Module::Exe, 0x14012e100));
    g_TypeId_UIDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7f90));

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
    fn_ModDetails_StringArrayPush = reinterpret_cast<fn_ModDetails_StringArrayPush_t>(
        ResolveAddr(Module::Exe, 0x14006e290));

    // --- IO::Modification ---
    g_ppModificationSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401519e8));
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

    // --- ModDialog helper functions (exe-local) ---
    fn_ModDialog_RefreshDisplay = reinterpret_cast<fn_ModDialog_RefreshDisplay_t>(
        ResolveAddr(Module::Exe, 0x14006eec0));
    fn_ModDialog_ToggleMod = reinterpret_cast<fn_ModDialog_ToggleMod_t>(
        ResolveAddr(Module::Exe, 0x14006e930));
    fn_ModDialog_MoveToTop = reinterpret_cast<fn_ModDialog_MoveToTop_t>(
        ResolveAddr(Module::Exe, 0x14006e660));
    fn_ModDialog_ShowDetails = reinterpret_cast<fn_ModDialog_ShowDetails_t>(
        ResolveAddr(Module::Exe, 0x14006e490));
    fn_ModDialog_Apply = reinterpret_cast<fn_ModDialog_Apply_t>(
        ResolveAddr(Module::Exe, 0x14006ea70));

    // --- IO::Serializer (shared globals) ---
    g_fn_Serializer_Ctor = reinterpret_cast<g_fn_Serializer_Ctor_t>(
        ResolveExport("?Serializer@IO@@QEAA@PEAVStream@1@H@Z"));
    g_fn_Serializer_Dtor = reinterpret_cast<g_fn_Serializer_Dtor_t>(
        ResolveExport("??1Serializer@IO@@QEAA@XZ"));
    g_fn_Serialize_Int = reinterpret_cast<g_fn_Serialize_Int_t>(
        ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAint@PEBD@Z"));
    g_fn_Serializer_GetVersion = reinterpret_cast<g_fn_Serializer_GetVersion_t>(
        ResolveExport("?GetVersion@Serializer@IO@@QEAAHXZ"));

    // --- TownHallUI support ---
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151de0));
        if (exref && *exref) {
            g_TypeId_TabElement = reinterpret_cast<uint32_t*>(*exref);
        }
    }
    fn_TownHallUI_InitGeneral = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x1400807f0));
    fn_TownHallUI_InitProfessions = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x140080160));
    fn_TownHallUI_InitProduction = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x14007fc30));
    fn_TownHallUI_InitInventory = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x14007f660));
    fn_TownHallUI_InitGraph = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x14007f0a0));
    fn_TownHallUI_InitAgriculture = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x14007ebf0));
    fn_TownHallUI_UpdateGraph = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x14007e5b0));
    fn_TownHallUI_UpdateGeneral = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x14007d030));
    fn_TownHallUI_UpdateProfessions = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x14007ccb0));
    fn_TownHallUI_UpdateProduction = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x14007c7f0));
    fn_TownHallUI_UpdateAgriculture = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x14007c220));
    fn_TownHallUI_UpdateInventory = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x14007def0));
    fn_TownHallUI_UpdateAll = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x14007d590));
    fn_TownHallUI_OnProfessionButton = reinterpret_cast<fn_TownHallUI_OnProfChange_t>(
        ResolveAddr(Module::Exe, 0x14007dca0));
    fn_TownHallUI_OnProfessionSpinner = reinterpret_cast<fn_TownHallUI_OnProfChange_t>(
        ResolveAddr(Module::Exe, 0x14007dd10));
    g_pGameSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

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
    fn_FindComponent_Building = reinterpret_cast<fn_FindComponent_t>(
        ResolveAddr(Module::Exe, 0x140015140));
    fn_Element_SetNeedsSizeCalc = reinterpret_cast<fn_Element_SetNeedsSizeCalc_t>(
        ResolveExport("?SetNeedsSizeCalc@Element@UI@@QEAAXXZ"));
    fn_TradeUI_Serialize = reinterpret_cast<fn_TradeUI_Serialize_t>(
        ResolveAddr(Module::Exe, 0x140084dd0));
    fn_TradeUI_Startup = reinterpret_cast<fn_TradeUI_Startup_t>(
        ResolveAddr(Module::Exe, 0x140082150));
    fn_TradeUI_OnCreate = reinterpret_cast<fn_TradeUI_OnCreate_t>(
        ResolveAddr(Module::Exe, 0x1400827b0));
    fn_TradeUI_OnRemove = reinterpret_cast<fn_TradeUI_OnRemove_t>(
        ResolveAddr(Module::Exe, 0x140081f20));
    fn_TradeUI_Update = reinterpret_cast<fn_TradeUI_Update_t>(
        ResolveAddr(Module::Exe, 0x1400868c0));
    fn_TradeUI_RefreshDisplay = reinterpret_cast<fn_TradeUI_RefreshDisplay_t>(
        ResolveAddr(Module::Exe, 0x1400846b0));
    fn_TradeUI_RefreshSellTable = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x140083c70));
    fn_TradeUI_OnTradeButton = reinterpret_cast<fn_TradeUI_OnTradeButton_t>(
        ResolveAddr(Module::Exe, 0x140081420));
    fn_TradeUI_OnDismissButton = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x140081950));
    fn_TradeUI_OnOrderSpinner = reinterpret_cast<fn_TradeUI_OnOrderSpinner_t>(
        ResolveAddr(Module::Exe, 0x140081d00));
    fn_TradeUI_OnPurchaseButton = reinterpret_cast<fn_TradeUI_OnPurchaseButton_t>(
        ResolveAddr(Module::Exe, 0x140081dd0));
    fn_TradeUI_OnComboPurchase = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x140081bc0));
    fn_TradeUI_OnComboOrder = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x140081e80));
    fn_TradeUI_OnStorageSpinner = reinterpret_cast<fn_TradeUI_OnStorageSpinner_t>(
        ResolveAddr(Module::Exe, 0x140081ab0));
    fn_TradeUI_OnSellSpinner = reinterpret_cast<fn_TradeUI_OnPurchaseButton_t>(
        ResolveAddr(Module::Exe, 0x140081a50));
    fn_TradeUI_OnBuySpinner = reinterpret_cast<fn_TradeUI_OnPurchaseButton_t>(
        ResolveAddr(Module::Exe, 0x1400819f0));
    fn_TradeUI_OnOrderAmountSpinner = reinterpret_cast<fn_TradeUI_OnPurchaseButton_t>(
        ResolveAddr(Module::Exe, 0x140081c50));
    g_TypeId_TableElement = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x140151510));
    g_TypeId_SpinnerElement = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401516c8));
    // TextElement type ID — exref at 0x140151508
    {
        void** exref = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151508));
        if (exref && *exref) g_TypeId_TextElement = reinterpret_cast<uint32_t*>(*exref);
    }
    // SelectDescription type ID — DAT_1401c48a8
    g_TypeId_SelectDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c48a8));
    fn_TradeUI_RefreshOrderQuantityDisplay = reinterpret_cast<fn_TradeUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x140082fb0));
    fn_TradeUI_RefreshSellSpinnerValues = reinterpret_cast<fn_TradeUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x140082d80));
    fn_TradeUI_RefreshOrderDisplay = reinterpret_cast<fn_TradeUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x140082b20));
    fn_TradeUI_RefreshBuyTable = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x140083100));
    fn_TradeUI_UpdateSellTotals = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x140082920));
    fn_TradeUI_UpdateBuyTotals = reinterpret_cast<fn_TownHallUI_SubFunc_t>(
        ResolveAddr(Module::Exe, 0x140082850));

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
    fn_NoiseUI_SetupImage = reinterpret_cast<fn_NoiseUI_SetupImage_t>(
        ResolveAddr(Module::Exe, 0x140071890));
    fn_NoiseUI_GenerateNoise = reinterpret_cast<fn_NoiseUI_GenerateNoise_t>(
        ResolveAddr(Module::Exe, 0x1400711D0));

    // --- DebugUI helpers ---
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
    fn_Game_KillAllCitizens = reinterpret_cast<fn_Game_KillAllCitizens_t>(
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
    fn_DebugUI_OnDrawComboChanged = reinterpret_cast<fn_DebugUI_OnDrawComboChanged_t>(
        ResolveAddr(Module::Exe, 0x1400608A0));
    fn_DebugUI_OnDaySliderChanged = reinterpret_cast<fn_DebugUI_OnDaySliderChanged_t>(
        ResolveAddr(Module::Exe, 0x1400607E0));
    fn_DebugUI_WriteLightingColors = reinterpret_cast<fn_DebugUI_WriteLightingColors_t>(
        ResolveAddr(Module::Exe, 0x140061530));
    fn_Game_GetMonthFromTime = reinterpret_cast<fn_Game_GetMonthFromTime_t>(
        ResolveAddr(Module::Exe, 0x1400932A0));

    // --- SaveLoadDialog::Load helpers ---
    fn_GameUpdateLoop_Ctor = reinterpret_cast<fn_GameUpdateLoop_Ctor_t>(
        ResolveAddr(Module::Exe, 0x140098060));
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
    fn_SetEngineNextLoop = reinterpret_cast<fn_SetEngineNextLoop_t>(
        ResolveAddr(Module::Exe, 0x140097fb0));
    fn_SetDialogLoaderNextLoop = reinterpret_cast<fn_SetDialogLoaderNextLoop_t>(
        ResolveAddr(Module::Exe, 0x14009a8f0));
    fn_GetLoadingSoundString = reinterpret_cast<fn_GetLoadingSoundString_t>(
        ResolveAddr(Module::Exe, 0x140099130));
    fn_MenuDialogLoopT_Ctor = reinterpret_cast<fn_MenuDialogLoopT_Ctor_t>(
        ResolveAddr(Module::Exe, 0x14009bb00));
    g_MenuDialogLoopT_Vtable = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x14015ed00));

    // --- Engine globals (direct memory addresses) ---
    g_pLoader              = reinterpret_cast<void*>(ResolveAddr(Module::Runtime, 0x1801fe7b8));
    g_pLoaderFlags         = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Runtime, 0x1801fe7c0));
    g_pLoaderCompletionEvent = reinterpret_cast<HANDLE*>(ResolveAddr(Module::Runtime, 0x1801fe7e0));
    g_pAppSingleton        = reinterpret_cast<void**>(ResolveAddr(Module::Runtime, 0x1801feb80));

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
    printf("  KeyMap                    = %p -> %p\n", (void*)g_pKeyMapSingleton,
           g_pKeyMapSingleton ? *g_pKeyMapSingleton : nullptr);
    printf("  EngineSingleton           = %p -> %p\n", (void*)g_pEngineSingleton,
           g_pEngineSingleton ? *g_pEngineSingleton : nullptr);
    printf("[EngineFuncs] Phase A4 — Exe Functions:\n");
    printf("  FindComp_StateMachine     = %p\n", (void*)fn_FindComponent_StateMachine);
    printf("  FindComp_Destroy          = %p\n", (void*)fn_FindComponent_Destroy);
    printf("  FindComp_Build            = %p\n", (void*)fn_FindComponent_Build);
    printf("  StorageQueryCount         = %p\n", (void*)fn_StorageComponent_QueryCount);
    printf("  DestroyComp_Reclaim       = %p\n", (void*)fn_DestroyComponent_Reclaim);
    printf("  DestroyComp_Rebuild       = %p\n", (void*)fn_DestroyComponent_Rebuild);
    printf("  DestroyComp_Destroy       = %p\n", (void*)fn_DestroyComponent_Destroy);
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
        ResolveExport("?GetIdForDeviceInput@KeyMap@Input@@QEBAHH@Z"));

    // Dialog constructors — exe functions
    fn_OptionInputBindDialog_Ctor = reinterpret_cast<fn_OptionInputBindDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x140071d30));
    fn_MessageDialog_Ctor = reinterpret_cast<fn_MessageDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x14006d300));

    // Only Input::Device::Update and Timer globals are critical for first reimpl
    if (!fn_InputDevice_Update) {
#ifdef _DEBUG
        printf("[EngineFuncs] WARNING: Failed to resolve Input::Device::Update\n");
#endif
        return false;
    }

    return true;
}
