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
fn_SaveLoad_GetOperation_t fn_SaveLoad_GetOperation = nullptr;
void** g_pSaveLoadSingleton = nullptr;
fn_EditElement_SetText_t fn_EditElement_SetText = nullptr;
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

    fn_EditElement_SetText = reinterpret_cast<fn_EditElement_SetText_t>(
        ResolveExport("?SetText@EditElement@UI@@QEAAXVString@2@@Z"));
    if (!fn_EditElement_SetText)
        fn_EditElement_SetText = reinterpret_cast<fn_EditElement_SetText_t>(
            ResolveAddr(Module::Runtime, 0x180042070));

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

    // Only Input::Device::Update and Timer globals are critical for first reimpl
    if (!fn_InputDevice_Update) {
#ifdef _DEBUG
        printf("[EngineFuncs] WARNING: Failed to resolve Input::Device::Update\n");
#endif
        return false;
    }

    return true;
}
