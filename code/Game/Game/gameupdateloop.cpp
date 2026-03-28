// code/Game/Game/gameupdateloop.cpp
// Clean reimplementation of GameUpdateLoop vtable methods from Application-x64.exe.
//
// GameUpdateLoop (0x160 bytes), vtable at 0x14015e2f0.
// Extends UpdateLoop (0x28 bytes base).
//
// This is the main gameplay loop. It owns all game subsystems and orchestrates
// the simulation tick (Update), render frame (Draw), and lifecycle (Startup/Shutdown).
//
// Methods reimplemented:
//   - GameUpdateLoop::Update            (0x140096380) -- main simulation tick
//   - GameUpdateLoop::Draw              (0x140096010) -- render interpolated frame
//   - GameUpdateLoop::Startup           (0x140097C00) -- init UI dialogs, cursor, game systems
//   - GameUpdateLoop::Shutdown          (0x1400977C0) -- teardown all subsystems
//   - GameUpdateLoop::OnScenarioComplete(0x140096830) -- scenario win/lose notification
//
// HEAVILY INSTRUMENTED with printf() for tracing a hang during first Update tick
// after loading a tutorial scenario.

#include "GameUpdateLoop.h"
#include "Entity/entitylist_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>
#include <cmath>

// ============================================================
// Exe globals (resolved at init time from .rdata/.data addresses)
// ============================================================

// g_rootOnlyUpdates: flag set to true when inner sim loop iteration is not the last
// Controls whether AnimationManager skips non-root bone updates for speed.
// Referenced as _rootOnlyUpdates_exref in Ghidra decompile.
// Indirect: exe .rdata at 0x140151f30 points to the actual bool in Runtime .data.
static bool** g_pp_rootOnlyUpdates = nullptr;

// g_frameIndex: global frame counter incremented each simulation tick.
// Referenced as _frameIndex_exref in Ghidra decompile.
// Indirect: exe .rdata at 0x140151f38 points to the actual int in Runtime .data.
static int32_t** g_pp_frameIndex = nullptr;

// ============================================================
// Exe function pointers (not yet in engine_funcs.h)
// ============================================================

// Game::UpdateTimedEvents (FUN_1400a5d10) — processes timed event queue
// RCX=timedEventQueue (game+0x78), RDX=time
typedef void (*fn_Game_UpdateTimedEvents_t)(void* timedEventQueue, void* time);
static fn_Game_UpdateTimedEvents_t fn_Game_UpdateTimedEvents = nullptr;

// Graphics::AnimationManager::Update(this, Time const&, bool rootOnly)
// Runtime DLL export
typedef void (*fn_AnimationManager_Update_t)(void* self, void* time, bool rootOnly);
static fn_AnimationManager_Update_t fn_AnimationManager_Update = nullptr;

// EntityList::Update (FUN_1400c4090) — tick all entity components
// RCX=entityList, RDX=time
typedef void (*fn_EntityList_Update_t)(void* entityList, void* time);
static fn_EntityList_Update_t fn_EntityList_Update = nullptr;

// CitizenBehavior::Update / Map::Update (FUN_14008c2b0) — population tick
// RCX=citizenList, RDX=time
typedef void (*fn_CitizenList_Update_t)(void* citizenList, void* time);
static fn_CitizenList_Update_t fn_CitizenList_Update = nullptr;

// EntityList::PostUpdate (FUN_1400c43d0) — post-update cleanup pass
// RCX=entityList
typedef void (*fn_EntityList_PostUpdate_t)(void* entityList);
static fn_EntityList_PostUpdate_t fn_EntityList_PostUpdate = nullptr;

// AmbientSound::Update (FUN_1400a40e0) — ambient sound state update
// RCX=ambientSoundData
typedef void (*fn_AmbientSound_Update_t)(void* ambientSoundData);
static fn_AmbientSound_Update_t fn_AmbientSound_Update = nullptr;

// EntityUI::Update / PlayerToolUpdate (FUN_1401026a0)
// RCX=entityUI, RDX=time(int64), R8=inputFlag(bool)
// Returns int (cursor index)
typedef int64_t (*fn_EntityUI_Update_t)(void* entityUI, int64_t time, bool inputFlag);
static fn_EntityUI_Update_t fn_EntityUI_Update = nullptr;

// PlayerController::CalcTransform (FUN_1400983f0)
// RCX=playerController, RDX=outTransform -> returns Transform*
typedef void* (*fn_PlayerController_CalcTransform_t)(void* self, void* outTransform);
static fn_PlayerController_CalcTransform_t fn_PlayerController_CalcTransform = nullptr;

// Audio::Scene::Update (Runtime)
// RCX=scene, RDX=time, R8=scaledTime, R9=random, stack=listenerPos, stack=windFactor
typedef void (*fn_Audio_Scene_Update_t)(void* scene, int64_t time, int64_t scaledTime,
                                        void* random, void* listenerPos, uint32_t windFactor);
static fn_Audio_Scene_Update_t fn_Audio_Scene_Update = nullptr;

// Audio::Scene::StopAll (Runtime)
// RCX=scene
typedef void (*fn_Audio_Scene_StopAll_t)(void* scene);
static fn_Audio_Scene_StopAll_t fn_Audio_Scene_StopAll = nullptr;

// Achievement::AchievementInstance::Update(this, Time const&)
typedef void (*fn_AchievementInstance_Update_t)(void* self, void* time);
static fn_AchievementInstance_Update_t fn_AchievementInstance_Update = nullptr;

// Achievement::AchievementInstance::IsMet(this) -> bool
typedef bool (*fn_AchievementInstance_IsMet_t)(void* self);
static fn_AchievementInstance_IsMet_t fn_AchievementInstance_IsMet = nullptr;

// Achievement::AchievementInstance::HasFailed(this) -> bool
typedef bool (*fn_AchievementInstance_HasFailed_t)(void* self);
static fn_AchievementInstance_HasFailed_t fn_AchievementInstance_HasFailed = nullptr;

// Achievement::AchievementManager::Update(this, Time const&)
typedef void (*fn_AchievementManager_Update_t)(void* self, void* time);
static fn_AchievementManager_Update_t fn_AchievementManager_Update = nullptr;

// Achievement::AchievementManager::SetEventHandler(this, handler)
typedef void (*fn_AchievementManager_SetEventHandler_t)(void* self, void* handler);
static fn_AchievementManager_SetEventHandler_t fn_AchievementManager_SetEventHandler = nullptr;

// UI::UserInterface::PausesGame(this) -> bool (Runtime)
typedef bool (*fn_UI_PausesGame_t)(void* self);
static fn_UI_PausesGame_t fn_UI_PausesGame = nullptr;

// Graphics::Scene::Clean(this) (Runtime)
typedef void (*fn_Graphics_Scene_Clean_t)(void* scene);
static fn_Graphics_Scene_Clean_t fn_Graphics_Scene_Clean = nullptr;

// Graphics::Scene::RemoveGlobalObject(this, Geometry*) (Runtime)
typedef void (*fn_Graphics_Scene_RemoveGlobalObject_t)(void* scene, void* geometry);
static fn_Graphics_Scene_RemoveGlobalObject_t fn_Graphics_Scene_RemoveGlobalObject = nullptr;

// Graphics::Renderer::Draw(this, Context*, float alpha, Scene*, Camera*)
typedef void (*fn_Graphics_Renderer_Draw_t)(void* self, void* ctx, float alpha, void* scene, void* camera);
static fn_Graphics_Renderer_Draw_t fn_Graphics_Renderer_Draw = nullptr;

// Graphics::Renderer::~Renderer(this)
typedef void (*fn_Graphics_Renderer_Dtor_t)(void* self);
static fn_Graphics_Renderer_Dtor_t fn_Graphics_Renderer_Dtor = nullptr;

// Save::SaveLoad::Draw(this, Context*, Material*)
typedef void (*fn_SaveLoad_Draw_t)(void* self, void* ctx, void* material);
static fn_SaveLoad_Draw_t fn_SaveLoad_Draw = nullptr;

// Video::Context::SetTime(this, float)
typedef void (*fn_Video_Context_SetTime_t)(void* self, float time);
static fn_Video_Context_SetTime_t fn_Video_Context_SetTime = nullptr;

// Video::Context::SetCamera(this, Matrix*, Matrix*)
typedef void (*fn_Video_Context_SetCamera_t)(void* self, void* viewMatrix, void* projMatrix);
static fn_Video_Context_SetCamera_t fn_Video_Context_SetCamera = nullptr;

// Math::Transform::AsTransform3(this) -> Transform3* (returns ptr into self)
// Already in engine_funcs.h as fn_Transform_AsTransform3 but it takes (self, retval)
// For the Draw path we use the Runtime version that returns in-place
typedef void* (*fn_Transform3_AsMatrix_t)(void* self);
static fn_Transform3_AsMatrix_t fn_Transform3_AsMatrix = nullptr;

// Math::Vector3::Normalize(this)
typedef void (*fn_Vector3_Normalize_t)(void* self);
static fn_Vector3_Normalize_t fn_Vector3_Normalize = nullptr;

// Engine::MainLoop::SetNextLoop(this, UpdateLoop*)
typedef void (*fn_MainLoop_SetNextLoop_t)(void* self, void* loop);
static fn_MainLoop_SetNextLoop_t fn_MainLoop_SetNextLoop = nullptr;

// fn_ImageBuffer_CopyToTexture — available from engine_funcs.h

// FUN_140095da0 — QuickSave handler (complex, just call via pointer)
typedef void (*fn_QuickSave_t)(void* self);
static fn_QuickSave_t fn_QuickSave = nullptr;

// Startup sub-functions
// FUN_140099260 — SetupUIReferences(keyMapManager, UI)
typedef void (*fn_SetupUIRefs_t)(void* keyMapMgr, void* ui);
static fn_SetupUIRefs_t fn_SetupUIRefs = nullptr;

// FUN_1400a90a0 — ProfessionList::Startup(professionList)
typedef void (*fn_ProfessionList_Startup_t)(void* professionList);
static fn_ProfessionList_Startup_t fn_ProfessionList_Startup = nullptr;

// fn_ToolbarDialog_Ctor — available from engine_funcs.h (last param is void* toolbarData)

// FUN_140068ab0 — EventNotifyDialog::Ctor(mem)
typedef void* (*fn_EventNotifyDialog_Ctor_t)(void* mem);
static fn_EventNotifyDialog_Ctor_t fn_EventNotifyDialog_Ctor = nullptr;

// FUN_14005e230 — AutoSaveDialog::Ctor(mem, ui, dialog, parentPtr)
typedef void* (*fn_AutoSaveDialog_Ctor_t)(void* mem, void* ui, void* dialog, void* parent);
static fn_AutoSaveDialog_Ctor_t fn_AutoSaveDialog_Ctor = nullptr;

// FUN_140101ee0 — EntityUI::Startup(entityUI)
typedef void (*fn_EntityUI_Startup_t)(void* entityUI);
static fn_EntityUI_Startup_t fn_EntityUI_Startup = nullptr;

// Shutdown sub-functions
// FUN_140102b60 — EntityUI::SetTool(entityUI, tool)
typedef void (*fn_EntityUI_SetTool_t)(void* entityUI, int64_t tool);
static fn_EntityUI_SetTool_t fn_EntityUI_SetTool = nullptr;

// CitizenComponent::AssignHousing(citizen, housingPtr)
typedef void (*fn_CitizenComponent_AssignHousing_t)(void* citizen, void* housingPtr);
static fn_CitizenComponent_AssignHousing_t fn_CitizenComponent_AssignHousing = nullptr;

// EntityList::Shutdown (FUN_1400c3d30)
typedef void (*fn_EntityList_Shutdown_t)(void* entityList);
static fn_EntityList_Shutdown_t fn_EntityList_Shutdown = nullptr;

// EntityList::Dtor (FUN_1400c3e60)
typedef void (*fn_EntityList_Dtor_t)(void* entityList);
static fn_EntityList_Dtor_t fn_EntityList_Dtor = nullptr;

// CitizenBehavior::Dtor (FUN_1400907b0)
typedef void (*fn_CitizenList_Dtor_t)(void* citizenList, void* p2, void* p3, void* p4);
static fn_CitizenList_Dtor_t fn_CitizenList_Dtor = nullptr;

// Game::Dtor (FUN_14009ed60)
typedef void (*fn_Game_Dtor_t)(void* game);
static fn_Game_Dtor_t fn_Game_Dtor = nullptr;

// Tutorial::Dtor (FUN_1400f8ff0)
typedef void (*fn_Tutorial_Dtor_t)(void* tutorial);
static fn_Tutorial_Dtor_t fn_Tutorial_Dtor = nullptr;

// EntityUI::Dtor (FUN_1401024d0)
typedef void (*fn_EntityUI_Dtor_t)(void* entityUI, void* p2, void* p3, void* p4);
static fn_EntityUI_Dtor_t fn_EntityUI_Dtor = nullptr;

// Weather::Dtor (FUN_140093eb0)
typedef void (*fn_Weather_Dtor_t)(void* weather, void* p2, void* p3, void* p4);
static fn_Weather_Dtor_t fn_Weather_Dtor = nullptr;

// EventNotifier string cleanup (FUN_14000a8a0)
typedef void (*fn_EventNotifier_Cleanup_t)(void* stringTable);
static fn_EventNotifier_Cleanup_t fn_EventNotifier_Cleanup = nullptr;

// MusicPlayer::Dtor (FUN_140018220)
typedef void (*fn_MusicPlayer_Dtor_t)(void* musicPlayer);
static fn_MusicPlayer_Dtor_t fn_MusicPlayer_Dtor = nullptr;

// ImageCapture::Dtor (FUN_1400a4fc0)
typedef void (*fn_ImageCapture_Dtor_t)(void* capture, void* p2, void* p3, void* p4);
static fn_ImageCapture_Dtor_t fn_ImageCapture_Dtor = nullptr;

// Runtime destructors (resolved by mangled name or ordinal)
typedef void (*fn_Graphics_Scene_Dtor_t)(void* scene);
static fn_Graphics_Scene_Dtor_t fn_Graphics_Scene_Dtor = nullptr;

typedef void (*fn_Audio_Scene_Dtor_t)(void* scene);
static fn_Audio_Scene_Dtor_t fn_Audio_Scene_Dtor = nullptr;

typedef void (*fn_Picking_Scene_Dtor_t)(void* scene);
static fn_Picking_Scene_Dtor_t fn_Picking_Scene_Dtor = nullptr;

typedef void (*fn_AnimationManager_Dtor_t)(void* mgr);
static fn_AnimationManager_Dtor_t fn_AnimationManager_Dtor = nullptr;

// Draw sub-functions
// FUN_1400a5140 — ImageCapture::DrawCheck (image capture terrain update)
typedef void (*fn_ImageCapture_DrawCheck_t)(void* capture, void* ctx, void* worldGeom, void* sunLight);
static fn_ImageCapture_DrawCheck_t fn_ImageCapture_DrawCheck = nullptr;

// FUN_140098660 — PlayerController::UpdateCamera(playerCtrl, float, float, float)
typedef void (*fn_PlayerController_UpdateCamera_t)(void* self, float p1, float p2, float p3);
static fn_PlayerController_UpdateCamera_t fn_PlayerController_UpdateCamera = nullptr;

// FUN_1400938d0 — Weather::DrawUpdate(weather, ctx)
typedef void (*fn_Weather_DrawUpdate_t)(void* weather, void* ctx);
static fn_Weather_DrawUpdate_t fn_Weather_DrawUpdate = nullptr;

// OnScenarioComplete sub-functions
// FUN_140095580 — EventNotifier::FireNotification(notifier, eventType, priority, entityPtr)
typedef void (*fn_EventNotifier_Fire_t)(void* notifier, int eventType, int priority, void* entityPtr);
static fn_EventNotifier_Fire_t fn_EventNotifier_Fire = nullptr;

// FUN_140095750 — EventNotifier::FireNotificationWithName(notifier, type, priority, entityPtr, nameStr)
typedef void (*fn_EventNotifier_FireWithName_t)(void* notifier, int eventType, int priority, void* entityPtr, void* nameStr);
static fn_EventNotifier_FireWithName_t fn_EventNotifier_FireWithName = nullptr;

// fn_Achievement_GetTitle — available from engine_funcs.h (takes self, retval)

// fn_UIString_Ctor — available from engine_funcs.h (retval, stringTable, stringName)

// fn_UIString_ToSystemString — available from engine_funcs.h (self, retval)

// FUN_1401027c0 — CalcWindFactor (for audio scene update)
typedef uint32_t (*fn_CalcWindFactor_t)(void* time);
static fn_CalcWindFactor_t fn_CalcWindFactor = nullptr;

// System::Time::Time(this, float seconds) -> Time* (constructor from seconds)
typedef void* (*fn_Time_Ctor_Seconds_t)(void* self, float seconds);
static fn_Time_Ctor_Seconds_t fn_Time_Ctor_Seconds = nullptr;

// Object::ExternalList::Find(this, retval, Filename const&, typeInfo*) (Runtime)
typedef void (*fn_ExternalList_Find_t)(void* self, void* retval, void* filename, void* typeInfo);
static fn_ExternalList_Find_t fn_ExternalList_Find = nullptr;

// System::Filename::Filename(this, const char*) (Runtime)
// NOTE: This is the CStr variant, distinct from the default ctor fn_Filename_Ctor in engine_funcs.h
typedef void* (*fn_Filename_CtorCStr_Local_t)(void* self, const char* path);
static fn_Filename_CtorCStr_Local_t fn_Filename_CtorCStr_Local = nullptr;

// fn_Filename_Dtor — available from engine_funcs.h

// fn_InternalBasePtr_Ctor — available from engine_funcs.h

// Dialog type ID pointer (exref in exe .rdata)
static uint32_t* g_pDialogTypeId = nullptr;

// ============================================================
// Float constants from exe .rdata
// ============================================================
// DAT_14017214c = 1.0f (UV scale / general purpose)
// DAT_14017219c = some audio time scale factor
// DAT_1401721dc, DAT_140172174, DAT_140172240 = camera update constants
static float* g_fConst_1_0 = nullptr;       // 0x14017214c
static float* g_fAudioTimeScale = nullptr;   // 0x14017219c
static float* g_fCamParam1 = nullptr;        // 0x1401721dc
static float* g_fCamParam2 = nullptr;        // 0x140172174
static float* g_fCamParam3 = nullptr;        // 0x140172240

// Application::Application::IsPaused (Runtime) — not yet in engine_funcs
typedef bool (*fn_App_IsPaused_t)(void* self);
static fn_App_IsPaused_t fn_App_IsPaused = nullptr;

// DAT_1401c3de0 — some debug/editor singleton
static void** g_pDebugSingleton = nullptr;

// ============================================================
// Initialization — must be called from EngineFuncsInit or similar
// ============================================================
static bool s_initialized = false;
static int s_frameNum = 0;
// Debug trace — disabled for performance, enable when debugging
#define TRACE(...) do { } while(0)

static void EnsureInit() {
    if (s_initialized) return;
    s_initialized = true;

    TRACE("[GameUpdateLoop] Resolving function pointers...\n");

    // Exe globals (indirect — .rdata dispatch table pointers into Runtime .data)
    g_pp_rootOnlyUpdates = reinterpret_cast<bool**>(ResolveAddr(Module::Exe, 0x140151f30));
    g_pp_frameIndex = reinterpret_cast<int32_t**>(ResolveAddr(Module::Exe, 0x140151f38));
    g_pDebugSingleton = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401c3de0));

    // Float constants
    g_fConst_1_0 = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x14017214c));
    g_fAudioTimeScale = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x14017219c));
    g_fCamParam1 = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x1401721dc));
    g_fCamParam2 = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172174));
    g_fCamParam3 = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172240));

    // Exe functions
    fn_Game_UpdateTimedEvents = reinterpret_cast<fn_Game_UpdateTimedEvents_t>(
        ResolveAddr(Module::Exe, 0x1400a5d10));
    fn_EntityList_Update = reinterpret_cast<fn_EntityList_Update_t>(
        ResolveAddr(Module::Exe, 0x1400c4090));
    fn_CitizenList_Update = reinterpret_cast<fn_CitizenList_Update_t>(
        ResolveAddr(Module::Exe, 0x14008c2b0));
    fn_EntityList_PostUpdate = reinterpret_cast<fn_EntityList_PostUpdate_t>(
        ResolveAddr(Module::Exe, 0x1400c43d0));
    fn_AmbientSound_Update = reinterpret_cast<fn_AmbientSound_Update_t>(
        ResolveAddr(Module::Exe, 0x1400a40e0));
    fn_EntityUI_Update = reinterpret_cast<fn_EntityUI_Update_t>(
        ResolveAddr(Module::Exe, 0x1401026a0));
    fn_PlayerController_CalcTransform = reinterpret_cast<fn_PlayerController_CalcTransform_t>(
        ResolveAddr(Module::Exe, 0x1400983f0));
    // AchievementInstance methods — indirect via .rdata dispatch tables in exe
    fn_AchievementInstance_Update = *reinterpret_cast<fn_AchievementInstance_Update_t*>(
        ResolveAddr(Module::Exe, 0x140152058));
    fn_AchievementInstance_IsMet = *reinterpret_cast<fn_AchievementInstance_IsMet_t*>(
        ResolveAddr(Module::Exe, 0x140152048));
    fn_AchievementInstance_HasFailed = *reinterpret_cast<fn_AchievementInstance_HasFailed_t*>(
        ResolveAddr(Module::Exe, 0x140152060));
    fn_QuickSave = reinterpret_cast<fn_QuickSave_t>(
        ResolveAddr(Module::Exe, 0x140095da0));
    fn_CalcWindFactor = reinterpret_cast<fn_CalcWindFactor_t>(
        ResolveAddr(Module::Exe, 0x1401027c0));

    // Startup sub-functions
    fn_SetupUIRefs = reinterpret_cast<fn_SetupUIRefs_t>(
        ResolveAddr(Module::Exe, 0x140099260));
    fn_ProfessionList_Startup = reinterpret_cast<fn_ProfessionList_Startup_t>(
        ResolveAddr(Module::Exe, 0x1400a90a0));
    // fn_ToolbarDialog_Ctor — resolved by EngineFuncsInit()
    fn_EventNotifyDialog_Ctor = reinterpret_cast<fn_EventNotifyDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x140068ab0));
    fn_AutoSaveDialog_Ctor = reinterpret_cast<fn_AutoSaveDialog_Ctor_t>(
        ResolveAddr(Module::Exe, 0x14005e230));
    fn_EntityUI_Startup = reinterpret_cast<fn_EntityUI_Startup_t>(
        ResolveAddr(Module::Exe, 0x140101ee0));

    // Shutdown sub-functions
    fn_EntityUI_SetTool = reinterpret_cast<fn_EntityUI_SetTool_t>(
        ResolveAddr(Module::Exe, 0x140102b60));
    fn_CitizenComponent_AssignHousing = reinterpret_cast<fn_CitizenComponent_AssignHousing_t>(
        ResolveAddr(Module::Exe, 0x140111ee0));
    fn_EntityList_Shutdown = reinterpret_cast<fn_EntityList_Shutdown_t>(
        ResolveAddr(Module::Exe, 0x1400c3d30));
    fn_EntityList_Dtor = reinterpret_cast<fn_EntityList_Dtor_t>(
        ResolveAddr(Module::Exe, 0x1400c3e60));
    fn_CitizenList_Dtor = reinterpret_cast<fn_CitizenList_Dtor_t>(
        ResolveAddr(Module::Exe, 0x1400907b0));
    fn_Game_Dtor = reinterpret_cast<fn_Game_Dtor_t>(
        ResolveAddr(Module::Exe, 0x14009ed60));
    fn_Tutorial_Dtor = reinterpret_cast<fn_Tutorial_Dtor_t>(
        ResolveAddr(Module::Exe, 0x1400f8ff0));
    fn_EntityUI_Dtor = reinterpret_cast<fn_EntityUI_Dtor_t>(
        ResolveAddr(Module::Exe, 0x1401024d0));
    fn_Weather_Dtor = reinterpret_cast<fn_Weather_Dtor_t>(
        ResolveAddr(Module::Exe, 0x140093eb0));
    fn_EventNotifier_Cleanup = reinterpret_cast<fn_EventNotifier_Cleanup_t>(
        ResolveAddr(Module::Exe, 0x14000a8a0));
    fn_MusicPlayer_Dtor = reinterpret_cast<fn_MusicPlayer_Dtor_t>(
        ResolveAddr(Module::Exe, 0x140018220));
    fn_ImageCapture_Dtor = reinterpret_cast<fn_ImageCapture_Dtor_t>(
        ResolveAddr(Module::Exe, 0x1400a4fc0));

    // Draw sub-functions
    fn_ImageCapture_DrawCheck = reinterpret_cast<fn_ImageCapture_DrawCheck_t>(
        ResolveAddr(Module::Exe, 0x1400a5140));
    fn_PlayerController_UpdateCamera = reinterpret_cast<fn_PlayerController_UpdateCamera_t>(
        ResolveAddr(Module::Exe, 0x140098660));
    fn_Weather_DrawUpdate = reinterpret_cast<fn_Weather_DrawUpdate_t>(
        ResolveAddr(Module::Exe, 0x1400938d0));

    // OnScenarioComplete sub-functions
    fn_EventNotifier_Fire = reinterpret_cast<fn_EventNotifier_Fire_t>(
        ResolveAddr(Module::Exe, 0x140095580));
    fn_EventNotifier_FireWithName = reinterpret_cast<fn_EventNotifier_FireWithName_t>(
        ResolveAddr(Module::Exe, 0x140095750));

    // Runtime function pointers (resolved by mangled name)
    // AnimationManager::Update — indirect via .rdata dispatch at 0x140152050
    fn_AnimationManager_Update = *reinterpret_cast<fn_AnimationManager_Update_t*>(
        ResolveAddr(Module::Exe, 0x140152050));
    // Runtime functions — resolved via ResolveExport (by mangled name)
    // These resolve directly to Runtime-x64.dll function addresses.
    fn_UI_PausesGame = reinterpret_cast<fn_UI_PausesGame_t>(
        ResolveExport("?PausesGame@UserInterface@UI@@QEBA_NXZ"));
    fn_Graphics_Scene_Clean = reinterpret_cast<fn_Graphics_Scene_Clean_t>(
        ResolveExport("?Clean@Scene@Graphics@@QEAAXXZ"));
    fn_Graphics_Scene_RemoveGlobalObject = reinterpret_cast<fn_Graphics_Scene_RemoveGlobalObject_t>(
        ResolveExport("?RemoveGlobalObject@Scene@Graphics@@QEAAXPEAVGeometry@2@@Z"));
    fn_Graphics_Renderer_Draw = reinterpret_cast<fn_Graphics_Renderer_Draw_t>(
        ResolveExport("?Draw@Renderer@Graphics@@QEAAXAEAVContext@Video@@MAEAVScene@2@AEAVCamera@Math@@@Z"));
    fn_Graphics_Renderer_Dtor = reinterpret_cast<fn_Graphics_Renderer_Dtor_t>(
        ResolveExport("??1Renderer@Graphics@@QEAA@XZ"));
    fn_Audio_Scene_Update = reinterpret_cast<fn_Audio_Scene_Update_t>(
        ResolveExport("?Update@Scene@Audio@@QEAA_NVTime@System@@0AEAVRandom@4@AEBVVector3@Math@@M@Z"));
    fn_Audio_Scene_StopAll = reinterpret_cast<fn_Audio_Scene_StopAll_t>(
        ResolveExport("?StopAll@Scene@Audio@@QEAAXXZ"));
    fn_Video_Context_SetTime = reinterpret_cast<fn_Video_Context_SetTime_t>(
        ResolveExport("?SetTime@Context@Video@@QEAAXM@Z"));
    fn_Video_Context_SetCamera = reinterpret_cast<fn_Video_Context_SetCamera_t>(
        ResolveExport("?SetCamera@Context@Video@@QEAAXAEBVMatrix@Math@@0@Z"));
    fn_Transform3_AsMatrix = reinterpret_cast<fn_Transform3_AsMatrix_t>(
        ResolveExport("?AsMatrix@Transform3@Math@@QEBA?AVMatrix@2@XZ"));
    fn_Vector3_Normalize = reinterpret_cast<fn_Vector3_Normalize_t>(
        ResolveExport("?Normalize@Vector3@Math@@QEBA?AV12@XZ"));
    fn_MainLoop_SetNextLoop = reinterpret_cast<fn_MainLoop_SetNextLoop_t>(
        ResolveExport("?SetNextLoop@MainLoop@Engine@@QEAAXPEAVUpdateLoop@2@@Z"));
    // fn_ImageBuffer_CopyToTexture — resolved by EngineFuncsInit()
    fn_SaveLoad_Draw = reinterpret_cast<fn_SaveLoad_Draw_t>(
        ResolveExport("?Draw@SaveLoad@Save@@QEAAXAEAVContext@Video@@V?$ExternalPtr@VMaterial@Graphics@@@Object@@@Z"));
    fn_Graphics_Scene_Dtor = reinterpret_cast<fn_Graphics_Scene_Dtor_t>(
        ResolveExport("??1Scene@Graphics@@QEAA@XZ"));
    fn_Audio_Scene_Dtor = reinterpret_cast<fn_Audio_Scene_Dtor_t>(
        ResolveExport("??1Scene@Audio@@QEAA@XZ"));
    fn_Picking_Scene_Dtor = reinterpret_cast<fn_Picking_Scene_Dtor_t>(
        ResolveExport("??1Scene@Picking@@QEAA@XZ"));
    fn_AnimationManager_Dtor = reinterpret_cast<fn_AnimationManager_Dtor_t>(
        ResolveExport("??1AnimationManager@Graphics@@QEAA@XZ"));
    fn_AchievementManager_Update = reinterpret_cast<fn_AchievementManager_Update_t>(
        ResolveExport("?Update@AchievementManager@Achievement@@QEAAXAEBVTime@System@@@Z"));
    fn_AchievementManager_SetEventHandler = reinterpret_cast<fn_AchievementManager_SetEventHandler_t>(
        ResolveExport("?SetEventHandler@AchievementManager@Achievement@@QEAAXP6AXH@Z@Z"));
    fn_Time_Ctor_Seconds = reinterpret_cast<fn_Time_Ctor_Seconds_t>(
        ResolveExport("??0Time@System@@QEAA@M@Z"));
    fn_App_IsPaused = reinterpret_cast<fn_App_IsPaused_t>(
        ResolveExport("?IsPaused@Application@1@QEAA_NXZ"));
    fn_ExternalList_Find = reinterpret_cast<fn_ExternalList_Find_t>(
        ResolveExport("?Find@ExternalList@Object@@QEBA?AV?$ExternalPtr@VExternal@Object@@@2@AEBVFilename@System@@VType@2@@Z"));
    fn_Filename_CtorCStr_Local = reinterpret_cast<fn_Filename_CtorCStr_Local_t>(
        ResolveExport("??0Filename@System@@QEAA@PEBD@Z"));
    // fn_Filename_Dtor — resolved by EngineFuncsInit()
    // fn_InternalBasePtr_Ctor — resolved by EngineFuncsInit()

    // Dialog type ID
    g_pDialogTypeId = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4138));

    // Bulk null-check all ResolveExport pointers
    #define CHECK_FN(fn) if (!(fn)) TRACE("[GameUpdateLoop] WARNING: " #fn " is NULL!\n")
    CHECK_FN(fn_Game_UpdateTimedEvents);
    CHECK_FN(fn_EntityList_Update);
    CHECK_FN(fn_CitizenList_Update);
    CHECK_FN(fn_EntityList_PostUpdate);
    CHECK_FN(fn_AmbientSound_Update);
    CHECK_FN(fn_EntityUI_Update);
    CHECK_FN(fn_PlayerController_CalcTransform);
    CHECK_FN(fn_AchievementInstance_Update);
    CHECK_FN(fn_AchievementInstance_IsMet);
    CHECK_FN(fn_AchievementInstance_HasFailed);
    CHECK_FN(fn_QuickSave);
    CHECK_FN(fn_CalcWindFactor);
    CHECK_FN(fn_AnimationManager_Update);
    CHECK_FN(fn_UI_PausesGame);
    CHECK_FN(fn_Graphics_Scene_Clean);
    CHECK_FN(fn_Audio_Scene_Update);
    CHECK_FN(fn_Audio_Scene_StopAll);
    CHECK_FN(fn_Video_Context_SetTime);
    CHECK_FN(fn_Video_Context_SetCamera);
    CHECK_FN(fn_Transform3_AsMatrix);
    CHECK_FN(fn_Vector3_Normalize);
    CHECK_FN(fn_MainLoop_SetNextLoop);
    CHECK_FN(fn_Time_Ctor_Seconds);
    CHECK_FN(fn_App_IsPaused);
    CHECK_FN(fn_ExternalList_Find);
    CHECK_FN(fn_Filename_CtorCStr_Local);
    CHECK_FN(fn_SaveLoad_Draw);
    CHECK_FN(fn_AchievementManager_Update);
    CHECK_FN(fn_EventNotifier_Fire);
    CHECK_FN(fn_EventNotifier_FireWithName);
    #undef CHECK_FN

    // Initialize EntityList clean module function pointers
    EntityList_InitFuncs();

    TRACE("[GameUpdateLoop] Function pointers resolved.\n");
}


// ============================================================
// Forward declarations
// ============================================================
extern "C" void GameUpdateLoop_OnScenarioComplete(GameUpdateLoop* self);

// fn_MemFreeStatic is already available from engine_funcs.h

// ============================================================
// GameUpdateLoop::Update (0x140096380) — Main simulation tick
// ============================================================
// This is the core game loop tick. Called once per frame by MainLoop::Frame.
// Runs the simulation N times (speed multiplier), then updates UI, audio, etc.
//
// HEAVILY INSTRUMENTED for debugging tutorial hang.

extern "C"
bool GameUpdateLoop_Update(GameUpdateLoop* self, void* time) {
    EnsureInit();

    s_frameNum++;

    TRACE("[GameUpdateLoop::Update] ENTER frame=%d (self=%p, time=%p, gameTime=%lld)\n",
          s_frameNum, (void*)self, time, (long long)self->m_gameTime);

    // Step 1: Check if UI pauses game
    TRACE("[GameUpdateLoop::Update] checking PausesGame...\n");
    bool paused = fn_UI_PausesGame(self->m_userInterface);
    TRACE("[GameUpdateLoop::Update] PausesGame = %d\n", (int)paused);

    if (paused) {
        TRACE("[GameUpdateLoop::Update] game paused by UI, skipping simulation loop\n");
        goto post_simulation;
    }

    {
        int speedMultiplier = 0;
        int iteration = 0;

        // Step 2: Read speed multiplier from EntityUI
        // EntityUI+0x30 = paused flag, EntityUI+0x34 = speed multiplier
        uint8_t* entityUI = (uint8_t*)self->m_entityUI;
        bool entityUIPaused = (*(char*)(entityUI + 0x30)) != 0;
        if (!entityUIPaused) {
            speedMultiplier = *(int32_t*)(entityUI + 0x34);
        }
        TRACE("[GameUpdateLoop::Update] entityUIPaused=%d, speedMultiplier=%d\n",
               (int)entityUIPaused, speedMultiplier);

        if (speedMultiplier > 0) {
            // Accumulate game time
            self->m_gameTime += *(int64_t*)time;
            TRACE("[GameUpdateLoop::Update] gameTime updated to %lld\n", (long long)self->m_gameTime);
        }

        // Step 3: Inner simulation loop
        if (speedMultiplier > 0) {
            do {
                TRACE("[GameUpdateLoop::Update] === sim iteration %d/%d ===\n", iteration + 1, speedMultiplier);

                // 3a. Timed event queue
                void* game = self->m_game;
                void* timedEventQueue = *(void**)((uint8_t*)game + 0x78);
                TRACE("[GameUpdateLoop::Update] calling Game::UpdateTimedEvents (queue=%p)...\n", timedEventQueue);
                fn_Game_UpdateTimedEvents(timedEventQueue, time);
                TRACE("[GameUpdateLoop::Update] Game::UpdateTimedEvents done\n");

                // 3b. AnimationManager::Update
                bool rootOnly = (iteration < speedMultiplier - 1);
                if (g_pp_rootOnlyUpdates && *g_pp_rootOnlyUpdates) {
                    **g_pp_rootOnlyUpdates = rootOnly;
                }
                TRACE("[GameUpdateLoop::Update] calling AnimationManager::Update (rootOnly=%d)...\n", (int)rootOnly);
                fn_AnimationManager_Update(self->m_animationManager, time, rootOnly);
                TRACE("[GameUpdateLoop::Update] AnimationManager::Update done\n");

                // 3c. EntityList::Update (component ticks)
                // Uses custom reimpl — original fn_EntityList_Update (0x1400c4090)
                // hangs due to a spinlock issue in the hooked environment.
                TRACE("[GameUpdateLoop::Update] calling EntityList::Update (entityList=%p)...\n", self->m_entityList);
                EntityList_Update(reinterpret_cast<EntityList*>(self->m_entityList), time);
                TRACE("[GameUpdateLoop::Update] EntityList::Update done\n");

                // 3d. Day counter increment
                {
                    int32_t* dayCounter = (int32_t*)self->m_dayCounter;
                    int64_t* accumulated = (int64_t*)(dayCounter + 2); // +0x08
                    int64_t* dayLength = (int64_t*)(dayCounter + 4);   // +0x10
                    *accumulated += *(int64_t*)time;
                    if (*accumulated >= *dayLength) {
                        *dayCounter = *dayCounter + 1;
                        *accumulated -= *dayLength;
                        TRACE("[GameUpdateLoop::Update] day counter incremented to %d\n", *dayCounter);
                    }
                }

                // 3e. Weather::ResetTime
                TRACE("[GameUpdateLoop::Update] calling Weather_ResetTime...\n");
                fn_Weather_ResetTime(self->m_weather, time);
                TRACE("[GameUpdateLoop::Update] Weather_ResetTime done\n");

                // 3f. CitizenBehavior::Update / Map::Update
                TRACE("[GameUpdateLoop::Update] calling CitizenList::Update (citizenList=%p)...\n", self->m_citizenList);
                fn_CitizenList_Update(self->m_citizenList, time);
                TRACE("[GameUpdateLoop::Update] CitizenList::Update done\n");

                // 3g. Tutorial frame/time increment
                {
                    uint8_t* tutorial = (uint8_t*)self->m_tutorial;
                    int32_t* frameCount = (int32_t*)(tutorial + 0x08);
                    int64_t* tutorialTime = (int64_t*)(tutorial + 0x130);
                    *frameCount = *frameCount + 1;
                    *tutorialTime = *tutorialTime + *(int64_t*)time;
                    TRACE("[GameUpdateLoop::Update] tutorial frame=%d, tutorialTime=%lld\n",
                           *frameCount, (long long)*tutorialTime);
                }

                // 3h. Global frame index increment
                if (g_pp_frameIndex && *g_pp_frameIndex) {
                    **g_pp_frameIndex = **g_pp_frameIndex + 1;
                }

                iteration++;
            } while (iteration < speedMultiplier);
        }

        // Step 4: Achievement check
        TRACE("[GameUpdateLoop::Update] checking achievements (scenarioInstance=%p, completed=%d)...\n",
               self->m_scenarioInstance, (int)self->m_scenarioCompleted);
        if (self->m_scenarioInstance != nullptr && self->m_scenarioCompleted == 0) {
            TRACE("[GameUpdateLoop::Update] calling InternalBasePtr::CheckPtr on scenarioInstance...\n");
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)self + 0x150));
            TRACE("[GameUpdateLoop::Update] calling AchievementInstance::Update...\n");
            fn_AchievementInstance_Update(*(void**)((uintptr_t)self + 0x150), time);
            TRACE("[GameUpdateLoop::Update] AchievementInstance::Update done\n");

            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)self + 0x150));
            bool isMet = fn_AchievementInstance_IsMet(*(void**)((uintptr_t)self + 0x150));
            TRACE("[GameUpdateLoop::Update] AchievementInstance::IsMet = %d\n", (int)isMet);

            if (!isMet) {
                fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)self + 0x150));
                bool hasFailed = fn_AchievementInstance_HasFailed(*(void**)((uintptr_t)self + 0x150));
                TRACE("[GameUpdateLoop::Update] AchievementInstance::HasFailed = %d\n", (int)hasFailed);
                if (hasFailed) {
                    TRACE("[GameUpdateLoop::Update] calling OnScenarioComplete (failed)...\n");
                    GameUpdateLoop_OnScenarioComplete(self);
                    TRACE("[GameUpdateLoop::Update] OnScenarioComplete done\n");
                }
            } else {
                TRACE("[GameUpdateLoop::Update] calling OnScenarioComplete (met)...\n");
                GameUpdateLoop_OnScenarioComplete(self);
                TRACE("[GameUpdateLoop::Update] OnScenarioComplete done\n");
            }
        }

        // Step 4b: AchievementManager::Update with scaled time
        TRACE("[GameUpdateLoop::Update] calling AchievementManager::Update...\n");
        {
            float seconds = fn_Time_AsSeconds(time);
            int64_t scaledTime[1];
            fn_Time_Ctor_Seconds(scaledTime, seconds * (float)speedMultiplier);
            void* achievementMgr = **g_ppAchievementManager;
            fn_AchievementManager_Update(achievementMgr, scaledTime);
        }
        TRACE("[GameUpdateLoop::Update] AchievementManager::Update done\n");

        // Step 5: EntityList post-update cleanup
        TRACE("[GameUpdateLoop::Update] calling EntityList::PostUpdate...\n");
        EntityList_PostUpdate(reinterpret_cast<EntityList*>(self->m_entityList));
        TRACE("[GameUpdateLoop::Update] EntityList::PostUpdate done\n");

        // Step 6: Camera transform update
        TRACE("[GameUpdateLoop::Update] updating camera transform...\n");
        {
            uint8_t transform3[44]; // Transform3 scratch space
            void* playerCtrl = self->m_playerController;
            void* result = fn_PlayerController_CalcTransform(playerCtrl, transform3);
            // AsTransform3 on the result, then SetCameraToWorld
            void* t3 = fn_Transform_AsTransform3(result, transform3);
            fn_Camera_SetCameraToWorld((void*)((uintptr_t)playerCtrl + 0x34), t3);
        }
        TRACE("[GameUpdateLoop::Update] camera transform done\n");
    }

post_simulation:
    // Step 7: Ambient sound update
    TRACE("[GameUpdateLoop::Update] calling AmbientSound::Update...\n");
    fn_AmbientSound_Update(self->m_ambientSoundData);
    TRACE("[GameUpdateLoop::Update] AmbientSound::Update done\n");

    // Step 8: World geometry vtable call (terrain update)
    TRACE("[GameUpdateLoop::Update] calling WorldGeometry::Update...\n");
    {
        void* worldGeom = self->m_worldGeometry;
        void** vtbl = *(void***)worldGeom;
        typedef void (*WorldGeomUpdate_t)(void* self, void* time);
        WorldGeomUpdate_t worldGeomUpdate = (WorldGeomUpdate_t)vtbl[1]; // vtable[1] = slot at +0x08
        worldGeomUpdate(worldGeom, time);
    }
    TRACE("[GameUpdateLoop::Update] WorldGeometry::Update done\n");

    // Step 9: UI::UserInterface::Update
    // Note: fn_UI_Update is typed as void in engine_funcs.h but actually returns int (cursor).
    // We use a local cast to capture the return value.
    TRACE("[GameUpdateLoop::Update] calling UI::UserInterface::Update...\n");
    typedef int (*fn_UI_Update_RetInt_t)(void* self, void* time);
    int uiCursor = ((fn_UI_Update_RetInt_t)fn_UI_Update)(self->m_userInterface, time);
    TRACE("[GameUpdateLoop::Update] UI::UserInterface::Update done (cursor=%d)\n", uiCursor);

    // Step 10: EntityUI / player tool update
    int cursorIndex = 1;
    {
        bool pausedNow = fn_UI_PausesGame(self->m_userInterface);
        if (!pausedNow) {
            // Check if UI has input focus: userInterface+0xd9 == 0 means no dialog has input
            bool noInputFocus = (*(uint8_t*)((uintptr_t)self->m_userInterface + 0xd9) == 0);
            TRACE("[GameUpdateLoop::Update] calling EntityUI::Update (inputFlag=%d)...\n", (int)noInputFocus);
            int64_t result = fn_EntityUI_Update(self->m_entityUI, *(int64_t*)time, noInputFocus);
            cursorIndex = (int)result;
            TRACE("[GameUpdateLoop::Update] EntityUI::Update done (cursorIndex=%d)\n", cursorIndex);
        }

        // Override cursor if UI has input or game is paused
        bool hasInput = (*(uint8_t*)((uintptr_t)self->m_userInterface + 0xd9) != 0);
        if (hasInput || fn_UI_PausesGame(self->m_userInterface)) {
            cursorIndex = uiCursor;
        }
    }

    // Step 11: Set cursor
    TRACE("[GameUpdateLoop::Update] setting cursor (index=%d)...\n", cursorIndex);
    {
        void* keyMapMgr = *g_pKeyMapSingleton;
        uint64_t cursorVal = *(uint64_t*)((uintptr_t)keyMapMgr + 0x28 + (int64_t)cursorIndex * 8);
        fn_UI_SetCursor(self->m_userInterface, cursorVal);
    }
    TRACE("[GameUpdateLoop::Update] cursor set\n");

    // Step 12: Audio::Scene::Update
    TRACE("[GameUpdateLoop::Update] calling Audio::Scene::Update...\n");
    {
        void* playerCtrl = self->m_playerController;
        // Build listener position from PlayerController camera data
        float listenerPos[3];
        // Transform3 at playerCtrl+0x98, camera offset at +0x1c,+0x20,+0x24
        // Copy transform3 for audio listener
        float tx = *(float*)((uintptr_t)playerCtrl + 0x98);
        float ty = *(float*)((uintptr_t)playerCtrl + 0x9c);
        float tz = *(float*)((uintptr_t)playerCtrl + 0xa0);
        float cx = *(float*)((uintptr_t)playerCtrl + 0x1c);
        float cy = *(float*)((uintptr_t)playerCtrl + 0x20);
        float cz = *(float*)((uintptr_t)playerCtrl + 0x24);
        listenerPos[0] = tx + 0.0f;
        listenerPos[1] = ty + 0.0f;
        listenerPos[2] = tz + 0.0f;

        // Get speed multiplier for audio time scaling
        int speedMul = 0;
        uint8_t* entityUI = (uint8_t*)self->m_entityUI;
        if (*(char*)(entityUI + 0x30) == 0) {
            speedMul = *(int32_t*)(entityUI + 0x34);
        }

        if (fn_Audio_Scene_Update && fn_Time_Ctor_Seconds && fn_CalcWindFactor) {
            float seconds = fn_Time_AsSeconds(time);
            int64_t scaledTime[1];
            fn_Time_Ctor_Seconds(scaledTime, seconds * (float)speedMul * (*g_fAudioTimeScale));
            uint32_t windFactor = fn_CalcWindFactor(scaledTime);
            fn_Audio_Scene_Update(self->m_audioScene, *(int64_t*)time, scaledTime[0],
                                  self->m_random, listenerPos, windFactor);
        } else {
            TRACE("[GameUpdateLoop::Update] SKIP Audio::Scene::Update (NULL fptr)\n");
        }
    }
    TRACE("[GameUpdateLoop::Update] Audio::Scene::Update done\n");

    // Step 13: Quicksave/quickload key checks
    {
        void* keyMap = nullptr;
        if (g_pKeyMapObjectExref && *g_pKeyMapObjectExref) {
            void* step2 = *(void**)*g_pKeyMapObjectExref;
            if (step2) {
                keyMap = *(void**)step2;
            }
        }

        if (keyMap && fn_KeyMap_Pressed) {
            self->m_quickSavePressed = fn_KeyMap_Pressed(keyMap, 0, 0x21) ? 1 : 0;
            self->m_quickLoadPressed = fn_KeyMap_Pressed(keyMap, 0, 0x22) ? 1 : 0;
            TRACE("[GameUpdateLoop::Update] quicksave/quickload key check done (save=%d, load=%d)\n",
                  self->m_quickSavePressed, self->m_quickLoadPressed);
        } else {
            TRACE("[GameUpdateLoop::Update] SKIP quicksave/quickload (keyMap=%p, fn=%p)\n",
                  keyMap, (void*)fn_KeyMap_Pressed);
        }
    }

    // Step 14: SaveLoad::Update if active
    if (self->m_saveLoad != nullptr) {
        TRACE("[GameUpdateLoop::Update] calling SaveLoad::Update...\n");
        fn_SaveLoad_Update(self->m_saveLoad, time);
        TRACE("[GameUpdateLoop::Update] SaveLoad::Update done\n");
    }

    // Step 14b: Calculate sun direction (normalized)
    // Original at 0x140096717-0x14009676f computes direction from PlayerController
    // offsets (+0x98..+0xac), normalizes via Math::Vector3::Normalize, but the
    // normalized result goes to a stack temporary and is never stored to SunLight
    // or passed to Scene::Clean. Verified in disassembly: no MOV between the
    // Normalize CALL and the Scene::Clean CALL. This appears to be dead code in
    // the original binary — possibly a removed/vestigial lighting update.
    {
        void* playerCtrl = self->m_playerController;
        uintptr_t pc = (uintptr_t)playerCtrl;
        float dirX = *(float*)(pc + 0xa4) + *(float*)(pc + 0x98) + 0.0f;
        float dirY = *(float*)(pc + 0xa8) + *(float*)(pc + 0x9c) + 0.0f;
        float dirZ = (*(float*)(pc + 0xac) + *(float*)(pc + 0xa0)) - (*g_fConst_1_0);
        float dir[3] = { dirX, dirY, dirZ };
        float len = sqrtf(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
        if (len > 0.0f) { dir[0] /= len; dir[1] /= len; dir[2] /= len; }
        // Result intentionally unused — matches original binary behavior.
        (void)dir;
    }

    // Step 15: Graphics::Scene::Clean
    TRACE("[GameUpdateLoop::Update] calling Graphics::Scene::Clean...\n");
    if (fn_Graphics_Scene_Clean) {
        fn_Graphics_Scene_Clean(self->m_graphicsScene);
    } else {
        TRACE("[GameUpdateLoop::Update] SKIP Graphics::Scene::Clean (NULL)\n");
    }
    TRACE("[GameUpdateLoop::Update] Graphics::Scene::Clean done\n");

    // Step 16: Image capture check
    {
        uint8_t* imageCapture = (uint8_t*)self->m_imageCapture;
        if (*(int64_t*)(imageCapture + 8) != 0 && *imageCapture != 0) {
            TRACE("[GameUpdateLoop::Update] processing image capture...\n");
            fn_VideoDevice_Lock(*g_pVideoDeviceSingleton);
            fn_ImageBuffer_CopyToTexture(
                *(void**)(imageCapture + 0x10),
                *(void**)(imageCapture + 0x28),
                *(int32_t*)(imageCapture + 4),
                *(int32_t*)(imageCapture + 4));
            fn_VideoDevice_Unlock(*g_pVideoDeviceSingleton);
            *imageCapture = 0;
            TRACE("[GameUpdateLoop::Update] image capture done\n");
        }
    }

    // Step 17: Next loop transition check
    if (self->m_nextLoop != nullptr) {
        // Check if fullscreen fade is done (UI+0xf0 == 0.0f)
        float fadeProgress = *(float*)((uintptr_t)self->m_userInterface + 0xf0);
        if (fadeProgress == 0.0f) {
            TRACE("[GameUpdateLoop::Update] transitioning to next loop %p\n", self->m_nextLoop);
            if (fn_MainLoop_SetNextLoop) fn_MainLoop_SetNextLoop(*g_pMainLoopSingleton, self->m_nextLoop);
        }
    }

    TRACE("[GameUpdateLoop::Update] EXIT\n");
    return true;
}


// ============================================================
// GameUpdateLoop::Draw (0x140096010) — Render one interpolated frame
// ============================================================

extern "C"
void GameUpdateLoop_Draw(GameUpdateLoop* self, float alpha) {
    EnsureInit();

    TRACE("[GameUpdateLoop::Draw] ENTER (alpha=%.3f)\n", alpha);

    // Step 1: Check pause — if paused, force alpha to 1.0 (no interpolation)
    bool paused = fn_UI_PausesGame(self->m_userInterface);
    bool entityUIPaused = (*(char*)((uintptr_t)self->m_entityUI + 0x30) != 0);
    if (paused || entityUIPaused) {
        alpha = *g_fConst_1_0; // 1.0f
    }

    // Step 2: Set time on video context
    TRACE("[GameUpdateLoop::Draw] setting video context time...\n");
    {
        void* engine = *g_pEngineSingleton;
        void* videoCtx = *(void**)((uintptr_t)engine + 0x10);
        int64_t engineFrameTime = *(int64_t*)((uintptr_t)engine + 0x68);

        float gameSeconds = fn_Time_AsSeconds((void*)&self->m_gameTime);
        float frameSeconds = fn_Time_AsSeconds((void*)&engineFrameTime);
        fn_Video_Context_SetTime(videoCtx, gameSeconds + frameSeconds * alpha);
    }
    TRACE("[GameUpdateLoop::Draw] video context time set\n");

    // Step 3: Image capture terrain update check
    TRACE("[GameUpdateLoop::Draw] checking image capture terrain...\n");
    {
        uint8_t* imageCapture = (uint8_t*)self->m_imageCapture;
        void* engine = *g_pEngineSingleton;
        void* videoCtx = *(void**)((uintptr_t)engine + 0x10);
        if (*(int64_t*)(imageCapture + 8) == 0) {
            // Remove and re-add global objects for terrain update
            void* gameWorld = *g_pGameWorldSingleton;
            void* graphicsScene = *(void**)((uintptr_t)gameWorld + 0x60);

            fn_Graphics_Scene_RemoveGlobalObject(graphicsScene, self->m_sunLight);
            fn_Graphics_Scene_RemoveGlobalObject(graphicsScene, self->m_worldGeometry);

            fn_ImageCapture_DrawCheck(imageCapture, videoCtx, self->m_worldGeometry, self->m_sunLight);

            fn_Graphics_Scene_AddGlobalObject(graphicsScene, self->m_sunLight);
            fn_Graphics_Scene_AddGlobalObject(graphicsScene, self->m_worldGeometry);
        }
    }
    TRACE("[GameUpdateLoop::Draw] image capture terrain done\n");

    // Step 4: Camera update
    TRACE("[GameUpdateLoop::Draw] updating player camera...\n");
    fn_PlayerController_UpdateCamera(self->m_playerController,
                                     *g_fCamParam1, *g_fCamParam2, *g_fCamParam3);
    TRACE("[GameUpdateLoop::Draw] player camera done\n");

    // Step 5: Weather draw update
    TRACE("[GameUpdateLoop::Draw] calling Weather::DrawUpdate...\n");
    {
        void* engine = *g_pEngineSingleton;
        void* videoCtx = *(void**)((uintptr_t)engine + 0x10);
        fn_Weather_DrawUpdate(self->m_weather, videoCtx);
    }
    TRACE("[GameUpdateLoop::Draw] Weather::DrawUpdate done\n");

    // Step 6: Copy renderer shadow mode from engine
    {
        void* engine = *g_pEngineSingleton;
        *(uint32_t*)((uintptr_t)self->m_renderer + 0x3c) =
            *(uint32_t*)((uintptr_t)engine + 0x30);
    }

    // Step 7: Graphics::Renderer::Draw
    TRACE("[GameUpdateLoop::Draw] calling Renderer::Draw...\n");
    {
        void* engine = *g_pEngineSingleton;
        void* videoCtx = *(void**)((uintptr_t)engine + 0x10);
        fn_Graphics_Renderer_Draw(self->m_renderer, videoCtx, alpha,
                                  self->m_graphicsScene,
                                  (void*)((uintptr_t)self->m_playerController + 0x34));
    }
    TRACE("[GameUpdateLoop::Draw] Renderer::Draw done\n");

    // Step 8: SaveLoad::Draw if active
    if (self->m_saveLoad != nullptr) {
        TRACE("[GameUpdateLoop::Draw] calling SaveLoad::Draw...\n");
        void* engine = *g_pEngineSingleton;
        void* videoCtx = *(void**)((uintptr_t)engine + 0x10);
        fn_SaveLoad_Draw(self->m_saveLoad, videoCtx, self->m_uiMaterial);
        TRACE("[GameUpdateLoop::Draw] SaveLoad::Draw done\n");
    }

    // Step 9: Quicksave screenshot
    if (self->m_quickSavePressed != 0) {
        TRACE("[GameUpdateLoop::Draw] processing quicksave screenshot...\n");
        fn_QuickSave((void*)self);
        TRACE("[GameUpdateLoop::Draw] quicksave screenshot done\n");
    }

    // Step 10: Set camera matrices on video context for UI
    TRACE("[GameUpdateLoop::Draw] setting camera matrices...\n");
    {
        void* engine = *g_pEngineSingleton;
        void* videoCtx = *(void**)((uintptr_t)engine + 0x10);
        uintptr_t pc = (uintptr_t)self->m_playerController;

        // Build Transform3 from PlayerController data (offset +0x98 to +0xC4)
        // Then get view and projection matrices
        // The decompile copies Transform3 at +0x98 and projection at +0xC8
        uint8_t transform3[28]; // 7 floats
        for (int i = 0; i < 7; i++) {
            ((uint32_t*)transform3)[i] = *(uint32_t*)(pc + 0x98 + i * 4);
        }
        void* viewMatrix = fn_Transform3_AsMatrix(transform3);

        // Projection matrix at +0xC8 (16 floats = 64 bytes)
        uint8_t projMatrix[64];
        for (int i = 0; i < 16; i++) {
            ((uint32_t*)projMatrix)[i] = *(uint32_t*)(pc + 0xC8 + i * 4);
        }

        fn_Video_Context_SetCamera(videoCtx, viewMatrix, projMatrix);
    }
    TRACE("[GameUpdateLoop::Draw] camera matrices set\n");

    // Step 11: EntityUI toolbar draw (via vtable call on EntityUI's current tool)
    TRACE("[GameUpdateLoop::Draw] calling EntityUI toolbar draw...\n");
    {
        uintptr_t entityUI = (uintptr_t)self->m_entityUI;
        void* engine = *g_pEngineSingleton;
        void* videoCtx = *(void**)((uintptr_t)engine + 0x10);

        // Check if UI has input focus or pauses game -> pass as flag
        bool hasInput = (*(uint8_t*)((uintptr_t)self->m_userInterface + 0xd9) != 0);
        bool pauseCheck = fn_UI_PausesGame(self->m_userInterface);
        uint64_t drawFlag = (hasInput || pauseCheck) ? 1 : 0;

        // EntityUI+0x28 is InternalBasePtr to current tool
        fn_InternalBasePtr_CheckPtr((void*)(entityUI + 0x28));
        void* tool = *(void**)(entityUI + 0x28);
        if (tool) {
            // vtable[8] (+0x40) = Draw(tool, ctx, flag)
            void** toolVtbl = *(void***)tool;
            typedef void (*ToolDraw_t)(void* self, void* ctx, uint64_t flag);
            ToolDraw_t toolDraw = (ToolDraw_t)toolVtbl[8];
            toolDraw(tool, videoCtx, drawFlag);
        }
    }
    TRACE("[GameUpdateLoop::Draw] EntityUI toolbar draw done\n");

    // Step 12: UI::UserInterface::Draw
    TRACE("[GameUpdateLoop::Draw] calling UI::UserInterface::Draw...\n");
    {
        void* engine = *g_pEngineSingleton;
        void* videoCtx = *(void**)((uintptr_t)engine + 0x10);
        fn_UI_Draw(self->m_userInterface, videoCtx);
    }
    TRACE("[GameUpdateLoop::Draw] UI::UserInterface::Draw done\n");

    // Step 13: Quickload screenshot (after UI draw)
    if (self->m_quickLoadPressed != 0) {
        TRACE("[GameUpdateLoop::Draw] processing quickload screenshot...\n");
        fn_QuickSave((void*)self);
        TRACE("[GameUpdateLoop::Draw] quickload screenshot done\n");
    }

    TRACE("[GameUpdateLoop::Draw] EXIT\n");
}


// ============================================================
// GameUpdateLoop::Startup (0x140097C00) — Initialize UI and game systems
// ============================================================

extern "C"
void GameUpdateLoop_Startup(GameUpdateLoop* self) {
    EnsureInit();

    TRACE("[GameUpdateLoop::Startup] ENTER (self=%p)\n", (void*)self);

    // Set audio scene on UI
    TRACE("[GameUpdateLoop::Startup] setting audio scene on UI...\n");
    *(void**)((uintptr_t)self->m_userInterface + 0x90) = self->m_audioScene;

    // Setup UI references (cursors, lists, etc.)
    TRACE("[GameUpdateLoop::Startup] calling SetupUIRefs...\n");
    fn_SetupUIRefs(*g_pKeyMapSingleton, self->m_userInterface);
    TRACE("[GameUpdateLoop::Startup] SetupUIRefs done\n");

    // Set default cursor
    TRACE("[GameUpdateLoop::Startup] setting default cursor...\n");
    {
        void* keyMapMgr = *g_pKeyMapSingleton;
        uint64_t cursorVal = *(uint64_t*)((uintptr_t)keyMapMgr + 0x30);
        fn_UI_SetCursor(self->m_userInterface, cursorVal);
    }

    // Enable cursor
    TRACE("[GameUpdateLoop::Startup] enabling cursor...\n");
    fn_UI_EnableCursor(self->m_userInterface, true);

    // ProfessionList startup
    TRACE("[GameUpdateLoop::Startup] calling ProfessionList::Startup...\n");
    fn_ProfessionList_Startup(self->m_professionList);
    TRACE("[GameUpdateLoop::Startup] ProfessionList::Startup done\n");

    // Create and add Toolbar dialog
    TRACE("[GameUpdateLoop::Startup] creating Toolbar dialog...\n");
    {
        uint8_t filename[8];
        fn_Filename_CtorCStr_Local(filename, "Dialog/Toolbar.rsc");

        // Find the Dialog resource in the ExternalList
        void* foundBase = nullptr;
        uint32_t typeInfo = *g_pDialogTypeId;
        int64_t findResult[2]; // Filename + typeInfo pair
        findResult[0] = 0;
        fn_ExternalList_Find(self->m_resourceList, &foundBase, filename, &typeInfo);

        void* dialogRes = nullptr;
        if (foundBase != nullptr) {
            if (fn_IsDerivedFrom(foundBase, *g_pDialogTypeId)) {
                dialogRes = foundBase;
            }
        }
        fn_Filename_Dtor(filename);

        // Allocate and construct ToolbarDialog (0x108 bytes)
        void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0x108, 8);
        void* toolbar = nullptr;
        if (mem != nullptr) {
            int64_t nullParent[1] = {0};
            fn_InternalBasePtr_Ctor(nullParent, nullptr);
            toolbar = fn_ToolbarDialog_Ctor(mem, self->m_userInterface, dialogRes, nullParent, nullptr);
        }

        // Add to UI
        int64_t dialogPtr[1];
        fn_InternalBasePtr_Ctor(dialogPtr, toolbar);
        fn_UI_AddDialog(self->m_userInterface, dialogPtr);
    }
    TRACE("[GameUpdateLoop::Startup] Toolbar dialog added\n");

    // Create and add EventNotify dialog
    TRACE("[GameUpdateLoop::Startup] creating EventNotify dialog...\n");
    {
        uint8_t filename[8];
        fn_Filename_CtorCStr_Local(filename, "Dialog/EventNotify.rsc");
        void* foundBase = nullptr;
        uint32_t typeInfo = *g_pDialogTypeId;
        fn_ExternalList_Find(self->m_resourceList, &foundBase, filename, &typeInfo);
        fn_Filename_Dtor(filename);

        // Allocate EventNotify (0xF0 bytes)
        void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0xF0, 8);
        void* eventNotify = nullptr;
        if (mem != nullptr) {
            eventNotify = fn_EventNotifyDialog_Ctor(mem);
        }

        int64_t dialogPtr[1];
        fn_InternalBasePtr_Ctor(dialogPtr, eventNotify);
        fn_UI_AddDialog(self->m_userInterface, dialogPtr);
    }
    TRACE("[GameUpdateLoop::Startup] EventNotify dialog added\n");

    // Create and add AutoSave dialog
    TRACE("[GameUpdateLoop::Startup] creating AutoSave dialog...\n");
    {
        uint8_t filename[8];
        fn_Filename_CtorCStr_Local(filename, "Dialog/AutoSave.rsc");
        void* foundBase = nullptr;
        uint32_t typeInfo = *g_pDialogTypeId;
        fn_ExternalList_Find(self->m_resourceList, &foundBase, filename, &typeInfo);

        void* dialogRes = nullptr;
        if (foundBase != nullptr) {
            if (fn_IsDerivedFrom(foundBase, *g_pDialogTypeId)) {
                dialogRes = foundBase;
            }
        }
        fn_Filename_Dtor(filename);

        // Allocate AutoSave dialog (0xD0 bytes)
        void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0xD0, 8);
        void* autoSave = nullptr;
        if (mem != nullptr) {
            int64_t nullParent[1] = {0};
            fn_InternalBasePtr_Ctor(nullParent, nullptr);
            autoSave = fn_AutoSaveDialog_Ctor(mem, self->m_userInterface, dialogRes, nullParent);
        }

        int64_t dialogPtr[1];
        fn_InternalBasePtr_Ctor(dialogPtr, autoSave);
        fn_UI_AddDialog(self->m_userInterface, dialogPtr);
    }
    TRACE("[GameUpdateLoop::Startup] AutoSave dialog added\n");

    // Set fullscreen fade
    TRACE("[GameUpdateLoop::Startup] setting fullscreen fade...\n");
    fn_UI_SetFullscreenFade(self->m_userInterface, true, 0xFF000000, true);
    fn_UI_SetFullscreenFade(self->m_userInterface, false, 0xFF000000, false);
    TRACE("[GameUpdateLoop::Startup] fullscreen fade set\n");

    // EntityUI startup
    TRACE("[GameUpdateLoop::Startup] calling EntityUI::Startup...\n");
    fn_EntityUI_Startup(self->m_entityUI);
    TRACE("[GameUpdateLoop::Startup] EntityUI::Startup done\n");

    TRACE("[GameUpdateLoop::Startup] EXIT\n");
}


// ============================================================
// GameUpdateLoop::Shutdown (0x1400977C0) — Teardown all subsystems
// ============================================================

extern "C"
void GameUpdateLoop_Shutdown(GameUpdateLoop* self) {
    EnsureInit();

    TRACE("[GameUpdateLoop::Shutdown] ENTER (self=%p)\n", (void*)self);

    // Clear achievement event handler
    TRACE("[GameUpdateLoop::Shutdown] clearing achievement event handler...\n");
    {
        void* achievementMgr = **g_ppAchievementManager;
        fn_AchievementManager_SetEventHandler(achievementMgr, nullptr);
    }

    // Clear game flags (remove 0x100 bit)
    TRACE("[GameUpdateLoop::Shutdown] clearing game flags...\n");
    {
        void* gameWorld = *g_pGameWorldSingleton;
        uint32_t* flags = (uint32_t*)((uintptr_t)gameWorld + 0xD0);
        *flags = *flags & 0xFFFFFEFF;
    }

    // EntityUI::SetTool(nullptr)
    TRACE("[GameUpdateLoop::Shutdown] calling EntityUI::SetTool(0)...\n");
    fn_EntityUI_SetTool(self->m_entityUI, 0);

    // Disable cursor
    TRACE("[GameUpdateLoop::Shutdown] disabling cursor...\n");
    fn_UI_EnableCursor(self->m_userInterface, false);

    // Stop all audio
    TRACE("[GameUpdateLoop::Shutdown] stopping all audio...\n");
    fn_Audio_Scene_StopAll(self->m_audioScene);

    // Citizen housing reassignment loop
    TRACE("[GameUpdateLoop::Shutdown] reassigning citizen housing...\n");
    {
        uint8_t* citizenList = (uint8_t*)self->m_citizenList;
        int32_t citizenCount = *(int32_t*)(citizenList + 0xa8);
        void** citizenArray = *(void***)(citizenList + 0xa0);

        for (int i = 0; i < citizenCount; i++) {
            void* citizen = citizenArray[i];
            fn_InternalBasePtr_CheckPtr(&citizen);

            uint8_t* cit = (uint8_t*)citizen;
            // Check if citizen needs housing reassignment:
            // age (float at +0x3c) >= max age (from profession at +0x288 + 0x44)
            // and housing ptr at +0x208 == 0
            float age = *(float*)(cit + 0x3c);
            float maxAge = *(float*)(*(uintptr_t*)(cit + 0x288) + 0x44);
            void* housing = *(void**)(cit + 0x208);

            if (age >= maxAge && housing == nullptr) {
                // Get profession list town hall ptr
                void* profList = self->m_professionList;
                int64_t townHall = *(int64_t*)((uintptr_t)profList + 0x30);

                // Check if citizen's current building != townHall or housing != null
                int64_t citBuilding = *(int64_t*)(cit + 0x1f0);
                int64_t citHousingRef = *(int64_t*)(cit + 0x248);

                int64_t nullRef[1] = {0};
                fn_InternalBasePtr_Ctor(nullRef, nullptr);

                if (citBuilding != townHall || citHousingRef != nullRef[0]) {
                    fn_CitizenComponent_AssignHousing(citizen, nullRef);
                    *(int64_t*)(cit + 0x1f0) = townHall;
                }
            }
        }
    }
    TRACE("[GameUpdateLoop::Shutdown] citizen housing done\n");

    // EntityList shutdown
    TRACE("[GameUpdateLoop::Shutdown] calling EntityList::Shutdown...\n");
    fn_EntityList_Shutdown(self->m_entityList);
    TRACE("[GameUpdateLoop::Shutdown] EntityList::Shutdown done\n");

    // Remove global scene objects
    TRACE("[GameUpdateLoop::Shutdown] removing global scene objects...\n");
    {
        void* gameWorld = *g_pGameWorldSingleton;
        void* graphicsScene = *(void**)((uintptr_t)gameWorld + 0x60);
        fn_Graphics_Scene_RemoveGlobalObject(graphicsScene, self->m_worldGeometry);
        fn_Graphics_Scene_RemoveGlobalObject(graphicsScene, self->m_sunLight);
        fn_Graphics_Scene_RemoveGlobalObject(graphicsScene, self->m_skyMesh);
    }

    // Delete scenario instance
    if (self->m_scenarioInstance != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown] deleting scenario instance...\n");
        fn_InternalBasePtr_DeletePtr((void*)((uintptr_t)self + 0x150));
    }

    // Free/destroy all subsystems
    TRACE("[GameUpdateLoop::Shutdown] destroying subsystems...\n");

    // Random
    if (self->m_random != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   freeing Random...\n");
        fn_MemFreeStatic(self->m_random);
    }

    // PlayerController
    if (self->m_playerController != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   freeing PlayerController...\n");
        fn_MemFreeStatic(self->m_playerController);
    }

    // Renderer
    if (self->m_renderer != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying Renderer...\n");
        fn_Graphics_Renderer_Dtor(self->m_renderer);
        fn_MemFreeStatic(self->m_renderer);
    }

    // EntityList
    if (self->m_entityList != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying EntityList...\n");
        fn_EntityList_Dtor(self->m_entityList);
        fn_MemFreeStatic(self->m_entityList);
    }

    // UserInterface
    if (self->m_userInterface != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying UserInterface...\n");
        // Call Runtime destructor
        typedef void (*fn_UI_Dtor_t)(void* self);
        fn_UI_Dtor_t uiDtor = reinterpret_cast<fn_UI_Dtor_t>(
            ResolveExport("??1UserInterface@UI@@QEAA@XZ"));
        if (uiDtor) uiDtor(self->m_userInterface);
        fn_MemFreeStatic(self->m_userInterface);
    }

    // CitizenBehavior
    if (self->m_citizenList != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying CitizenBehavior...\n");
        fn_CitizenList_Dtor(self->m_citizenList, nullptr, nullptr, nullptr);
        fn_MemFreeStatic(self->m_citizenList);
    }

    // Game
    if (self->m_game != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying Game...\n");
        fn_Game_Dtor(self->m_game);
        fn_MemFreeStatic(self->m_game);
    }

    // Tutorial
    if (self->m_tutorial != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying Tutorial...\n");
        fn_Tutorial_Dtor(self->m_tutorial);
        fn_MemFreeStatic(self->m_tutorial);
    }

    // EntityUI
    if (self->m_entityUI != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying EntityUI...\n");
        fn_EntityUI_Dtor(self->m_entityUI, nullptr, nullptr, nullptr);
        fn_MemFreeStatic(self->m_entityUI);
    }

    // DayCounter
    if (self->m_dayCounter != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   freeing DayCounter...\n");
        fn_MemFreeStatic(self->m_dayCounter);
    }

    // Weather
    if (self->m_weather != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying Weather...\n");
        fn_Weather_Dtor(self->m_weather, nullptr, nullptr, nullptr);
        fn_MemFreeStatic(self->m_weather);
    }

    // EventNotifier
    if (self->m_eventNotifier != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying EventNotifier...\n");
        fn_EventNotifier_Cleanup((void*)((uintptr_t)self->m_eventNotifier + 0x18));
        fn_MemFreeStatic(self->m_eventNotifier);
    }

    // Graphics::Scene
    if (self->m_graphicsScene != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying Graphics::Scene...\n");
        fn_Graphics_Scene_Dtor(self->m_graphicsScene);
        fn_MemFreeStatic(self->m_graphicsScene);
    }

    // Audio::Scene
    if (self->m_audioScene != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying Audio::Scene...\n");
        fn_Audio_Scene_Dtor(self->m_audioScene);
        fn_MemFreeStatic(self->m_audioScene);
    }

    // Picking::Scene
    if (self->m_pickingScene != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying Picking::Scene...\n");
        fn_Picking_Scene_Dtor(self->m_pickingScene);
        fn_MemFreeStatic(self->m_pickingScene);
    }

    // WorldGeometry, SunLight, SkyMesh — via scalar deleting dtor (vtable[0] with flags=1)
    if (self->m_worldGeometry != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying WorldGeometry...\n");
        typedef void (*ScalarDtor_t)(void* self, uint32_t flags);
        ScalarDtor_t dtor = *(ScalarDtor_t*)(*(uintptr_t*)self->m_worldGeometry);
        dtor(self->m_worldGeometry, 1);
    }
    if (self->m_sunLight != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying SunLight...\n");
        typedef void (*ScalarDtor_t)(void* self, uint32_t flags);
        ScalarDtor_t dtor = *(ScalarDtor_t*)(*(uintptr_t*)self->m_sunLight);
        dtor(self->m_sunLight, 1);
    }
    if (self->m_skyMesh != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying SkyMesh...\n");
        typedef void (*ScalarDtor_t)(void* self, uint32_t flags);
        ScalarDtor_t dtor = *(ScalarDtor_t*)(*(uintptr_t*)self->m_skyMesh);
        dtor(self->m_skyMesh, 1);
    }

    // AnimationManager
    if (self->m_animationManager != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying AnimationManager...\n");
        fn_AnimationManager_Dtor(self->m_animationManager);
        fn_MemFreeStatic(self->m_animationManager);
    }

    // MusicPlayer
    if (self->m_musicPlayer != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying MusicPlayer...\n");
        fn_MusicPlayer_Dtor(self->m_musicPlayer);
        fn_MemFreeStatic(self->m_musicPlayer);
    }

    // SaveLoad
    if (self->m_saveLoad != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying SaveLoad...\n");
        fn_SaveLoad_Dtor(self->m_saveLoad);
        fn_MemFreeStatic(self->m_saveLoad);
    }

    // ImageCapture
    if (self->m_imageCapture != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   destroying ImageCapture...\n");
        fn_ImageCapture_Dtor(self->m_imageCapture, nullptr, nullptr, nullptr);
        fn_MemFreeStatic(self->m_imageCapture);
    }

    // AmbientSoundData
    if (self->m_ambientSoundData != nullptr) {
        TRACE("[GameUpdateLoop::Shutdown]   freeing AmbientSoundData...\n");
        fn_MemFreeStatic(self->m_ambientSoundData);
    }

    TRACE("[GameUpdateLoop::Shutdown] EXIT\n");
}


// ============================================================
// GameUpdateLoop::OnScenarioComplete (0x140096830)
// ============================================================
// Called when the scenario achievement is met or has failed.
// Sets m_scenarioCompleted flag and fires an event notification.

extern "C"
void GameUpdateLoop_OnScenarioComplete(GameUpdateLoop* self) {
    EnsureInit();

    TRACE("[GameUpdateLoop::OnScenarioComplete] ENTER (self=%p)\n", (void*)self);

    // Set completed flag
    self->m_scenarioCompleted = 1;

    // Check if the achievement has a parent Achievement object
    fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)self + 0x150));
    void* achievementInstance = *(void**)((uintptr_t)self + 0x150);
    void* achievement = *(void**)((uintptr_t)achievementInstance + 0x20);

    if (achievement == nullptr) {
        // No parent achievement — simple notification
        // Event type: 0x1d (win) or 0x1c (fail) depending on IsMet
        TRACE("[GameUpdateLoop::OnScenarioComplete] no parent achievement, firing simple notification...\n");
        int64_t nullEntity[1] = {0};
        fn_InternalBasePtr_Ctor(nullEntity, nullptr);

        fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)self + 0x150));
        bool isMet = fn_AchievementInstance_IsMet(*(void**)((uintptr_t)self + 0x150));

        // 0x1d = scenario won (29), 0x1c = scenario failed (28)
        int eventType = 0x1d - (int)isMet;
        fn_EventNotifier_Fire(self->m_eventNotifier, eventType, 4, nullEntity);
    } else {
        // Has parent achievement — notification with achievement title
        TRACE("[GameUpdateLoop::OnScenarioComplete] has parent achievement, firing named notification...\n");

        // Get achievement title (non-trivial return: self, retval)
        uint8_t titleStr[8] = {0}; // System::String retval buffer
        fn_Achievement_GetTitle(achievement, titleStr);

        // Build UI::String from achievement name
        // fn_UIString_Ctor(retval, stringTable, stringName)
        uint8_t uiStr[24]; // UI::String scratch (non-trivial return in first param)
        fn_UIString_Ctor(uiStr, *(uint64_t*)((uintptr_t)achievement + 0x20), titleStr);

        // Convert to System::String (member, non-trivial return: self, retval)
        uint8_t sysStr[8] = {0}; // System::String
        fn_UIString_ToSystemString(uiStr, sysStr);

        int64_t nullEntity[1] = {0};
        fn_InternalBasePtr_Ctor(nullEntity, nullptr);

        fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)self + 0x150));
        bool isMet = fn_AchievementInstance_IsMet(*(void**)((uintptr_t)self + 0x150));

        // 0x1b = scenario won with name (27), 0x1a = scenario failed with name (26)
        int eventType = 0x1b - (int)isMet;
        fn_EventNotifier_FireWithName(self->m_eventNotifier, eventType, 4, nullEntity, sysStr);

        fn_String_Dtor(sysStr);
    }

    TRACE("[GameUpdateLoop::OnScenarioComplete] EXIT\n");
}

