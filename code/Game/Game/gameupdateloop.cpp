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
#include "Component/component_factory_base.h"
#include "Object/internal_factory_audit.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstdlib>

// Toolbar HUD standalone bring-up (code/Game/Dialog/toolbar_vtable_standalone.cpp):
// builds the ToolbarDialog vtable + fills the toolbar type-id globals. Called only
// from the BANISHED_LIVE_TOOLBAR-gated HUD path in Startup below.
extern "C" void ToolbarStandalone_Init();
// EventNotify ribbon standalone ctor (code/Game/Dialog/eventnotifydialog.cpp) —
// builds the ribbon dialog (DialogInstance base + our SA vtable + field init).
extern "C" void* EventNotifyDialog_Ctor_Standalone(void* self, void* ui, void* extRes, void* parentIBP);
// Event-log writer (code/Game/Game/eventlog.cpp) — appends a real entry to the
// gameWorld+0x138 event log (formatted from the events/seasons StringTables).
extern "C" void EventLog_AddSimple(int eventType, int severity);
// NOTE: EventLog_CheckResourceShortages (code/Game/Game/eventlog.cpp, port of
// FUN_14008c100) is intentionally NOT called from here anymore — see the retirement
// note at its former call site below. The scan now runs once per tick from
// CitizenBehavior_CheckThresholds via the registered fn_NotificationEvent path.

// GUI_PLAN Keystone F Phase 3 — player tool activation + per-frame tick
// (code/Game/Player/tool_activation_standalone.cpp). Gated behind BANISHED_LIVE_TOOLS
// so the default build stays byte-identical: build the tool-mode tree at EntityUI
// startup, tick the active tool each frame (OnKey/Serialize/cursor), draw its footprint.
extern "C" void EntityUI_BuildToolModeTree_SA(void* entityUI);
extern "C" int  EntityUI_ToolTick_SA(void* entityUI, int activeFlag, int64_t time);
extern "C" void EntityUI_ToolDraw_SA(void* entityUI, void* context, int activeFlag);
// Minimap terrain painter (code/Game/Game/minimap_paint.cpp) — standalone fallback for
// the exe FUN_1400a5140 (ImageCapture::DrawCheck), wired into fn_ImageCapture_DrawCheck.
extern "C" void Minimap_PaintTerrain(void* minimap, void* context, void* worldGeom, void* sunLight);
static bool LiveToolsEnabled() {
    static int g = -1;
    if (g < 0) { g = ::ComponentFactory::LiveEnabled("BANISHED_LIVE_TOOLS") ? 1 : 0; }
    return g != 0;
}

// In-app RenderDoc capture driver — ticked once per in-game rendered frame so a
// delayed capture armed by OptionsDialog::ApplyVideoSettings (after an MSAA/res
// apply) actually fires. No-op unless the exe was launched via RenderDoc's UI.
#include "renderdoc_integration.h"

// Runtime type-id lookup (body in code/Game/Object/factory_registration.cpp) —
// used as a standalone fallback for the Dialog type-id when the exe data slot
// isn't registered. ResolveTypeIdByNameLazy walks the LIVE Runtime factory tree,
// so it sees Runtime-self-registered types like "Dialog" (GetTypeIdByName only
// knows our exe-side REGISTER_TYPE entries and returns 0 for Dialog).
namespace Object { uint32_t ResolveTypeIdByNameLazy(const char* simpleName); }

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
// NB: engine_funcs.h declares a project-wide fn_Audio_Scene_Update (used by the
// menu loop) with the verified signature (..., AEBVVector3, float). This local
// copy is kept distinct (fn_GUL_*) to preserve GameUpdateLoop's existing
// uint32_t-windFactor call shape unchanged; the two resolve the same Runtime
// export (?Update@Scene@Audio@@...). Both are ABI-identical (6th arg is a 4-byte
// stack slot either way).
typedef void (*fn_GUL_AudioSceneUpdate_t)(void* scene, int64_t time, int64_t scaledTime,
                                        void* random, void* listenerPos, uint32_t windFactor);
static fn_GUL_AudioSceneUpdate_t fn_GUL_AudioSceneUpdate = nullptr;

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

// Graphics::Scene::Clean(this) — moved to engine_funcs.{h,cpp}; reuse the
// project-wide extern so MenuDLT can also call it.

// Graphics::Scene::RemoveGlobalObject(this, Geometry*) (Runtime)
typedef void (*fn_Graphics_Scene_RemoveGlobalObject_t)(void* scene, void* geometry);
static fn_Graphics_Scene_RemoveGlobalObject_t fn_Graphics_Scene_RemoveGlobalObject = nullptr;

// Graphics::Renderer::Draw(this, Context*, float alpha, Scene*, Camera*)
// (NB: a global with the same name is declared extern in engine_funcs.h. The two
// resolve to the same Runtime export when both succeed; we keep the local static
// here for backward-compat with the existing in-flight reimpl. Local static name
// renamed to avoid the extern/static clash.)
typedef void (*fn_Graphics_Renderer_Draw_local_t)(void* self, void* ctx, float alpha, void* scene, void* camera);
static fn_Graphics_Renderer_Draw_local_t fn_Graphics_Renderer_Draw_local = nullptr;

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

// Math::Transform3::AsMatrix(this) -> Matrix (non-trivial return: 64 bytes)
// MSVC x64 member function ABI: RCX=this, RDX=hidden retval ptr, returns RAX=retval ptr
typedef void* (*fn_Transform3_AsMatrix_t)(void* self, void* retval);
static fn_Transform3_AsMatrix_t fn_Transform3_AsMatrix = nullptr;

// Math::Vector3::Normalize(this)
typedef void (*fn_Vector3_Normalize_t)(void* self);
static fn_Vector3_Normalize_t fn_Vector3_Normalize = nullptr;

// Engine::MainLoop::SetNextLoop — declared in engine_funcs.h, resolved by EngineFuncsInit()

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

// CitizenComponent::AssignHousing — now shared from engine_funcs.h

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

// Native time-of-day sun/lighting driver (code/Game/Game/weather_sun.cpp).
extern "C" void Weather_SunUpdate(void* self, void* ctx);

// In-game camera controller input (code/Game/Game/camera_input.cpp). Drives the
// PlayerController camera from the KeyMap (WASD pan, Q/E rotate, wheel zoom, presets).
// Only called in standalone, where EntityUI::Update (0x1401026a0) isn't registered.
namespace Game {
    void Camera_UpdateMovement(void* entityUI, int inputFlag, int64_t timeTicks);
    void Camera_UpdateHotkeys(void* entityUI, int inputFlag, int64_t timeTicks);
}

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

// Per-render-frame token (incremented once per GameUpdateLoop::Update). Exposed so the
// tree-driven ParticleComponent tick can un-stamp each pooled emit-state at most once
// per frame (matching retail's once-per-render-frame _frameIndex advance in
// Renderer::Render), independent of how many houses share the emit or how many sim
// sub-iterations the speed multiplier runs.
extern "C" int GameUpdateLoop_CurrentFrame() { return s_frameNum; }
// Debug trace — disabled for performance, enable when debugging
#define TRACE(...) do { } while(0)

static void EnsureInit() {
    if (s_initialized) return;
    s_initialized = true;

    TRACE("[GameUpdateLoop] Resolving function pointers...\n");

    // [GUARD-M5] g_pEngineSingleton is resolved in engine_funcs via exref slot
    // 0x140151440, which is unregistered in standalone (→ null). GameUpdateLoop::Draw
    // reads engine+0x10 (video context), +0x68 (frame time), +0x30 (shadow mode).
    // In standalone the object carrying those fields is the MainLoop itself — the
    // working menu Draw stub sources the video context from *(MainLoop + 0x10)
    // (menudialogloopt_stubs.cpp). NOTE the EngineInstance at MainLoop+0x78 is a
    // DIFFERENT object (audio @ +0x6d0); its +0x10 is not the context. So point
    // g_pEngineSingleton at the MainLoop slot: *g_pEngineSingleton == MainLoop.
    if (!g_pEngineSingleton && g_pMainLoopSingleton && *g_pMainLoopSingleton) {
        g_pEngineSingleton = g_pMainLoopSingleton;
        fprintf(stderr, "[GUL] g_pEngineSingleton := MainLoop singleton (%p -> ctx=%p)\n",
                (void*)*g_pEngineSingleton,
                *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(*g_pEngineSingleton) + 0x10));
        fflush(stderr);
    }

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
    // AchievementInstance methods — indirect via .rdata dispatch tables in exe.
    // In standalone these dispatch slots are unregistered, so ResolveAddr returns
    // 0; guard the deref (a NULL slot crashed EnsureInit on the New Game path,
    // since EnsureInit runs from every GameUpdateLoop vtable method). A NULL fn
    // pointer is fine here — CHECK_FN only warns, and every call site is gated by
    // the m_game==null world guards (and the new-game path skips AchievementInstance
    // entirely). Registering these slots properly is M5 work.
    {
        uintptr_t slot = ResolveAddr(Module::Exe, 0x140152058);
        fn_AchievementInstance_Update = slot
            ? *reinterpret_cast<fn_AchievementInstance_Update_t*>(slot) : nullptr;
        slot = ResolveAddr(Module::Exe, 0x140152048);
        fn_AchievementInstance_IsMet = slot
            ? *reinterpret_cast<fn_AchievementInstance_IsMet_t*>(slot) : nullptr;
        slot = ResolveAddr(Module::Exe, 0x140152060);
        fn_AchievementInstance_HasFailed = slot
            ? *reinterpret_cast<fn_AchievementInstance_HasFailed_t*>(slot) : nullptr;
    }
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
    // fn_CitizenComponent_AssignHousing — resolved centrally in EngineFuncsInit()
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
    // Standalone: the exe FUN_1400a5140 isn't loaded, so ResolveAddr returns null and
    // the minimap terrain would never be painted. Fall back to our clean port
    // (code/Game/Game/minimap_paint.cpp) so the minimap renders real terrain.
    if (!fn_ImageCapture_DrawCheck) {
        fn_ImageCapture_DrawCheck =
            reinterpret_cast<fn_ImageCapture_DrawCheck_t>(&Minimap_PaintTerrain);
    }
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
    // AnimationManager::Update — indirect via .rdata dispatch at 0x140152050.
    // Same standalone-unregistered guard as the AchievementInstance slots above.
    {
        uintptr_t slot = ResolveAddr(Module::Exe, 0x140152050);
        fn_AnimationManager_Update = slot
            ? *reinterpret_cast<fn_AnimationManager_Update_t*>(slot) : nullptr;
    }
    // Runtime functions — resolved via ResolveExport (by mangled name)
    // These resolve directly to Runtime-x64.dll function addresses.
    fn_UI_PausesGame = reinterpret_cast<fn_UI_PausesGame_t>(
        ResolveExport("?PausesGame@UserInterface@UI@@QEBA_NXZ"));
    // fn_Graphics_Scene_Clean is now resolved in engine_funcs.cpp (extern).
    fn_Graphics_Scene_RemoveGlobalObject = reinterpret_cast<fn_Graphics_Scene_RemoveGlobalObject_t>(
        ResolveExport("?RemoveGlobalObject@Scene@Graphics@@QEAAXPEAVGeometry@2@@Z"));
    // [FIX-M5] The mangled name here was wrong (AEAVScene/Camera@Math); the correct
    // signature is const-ref Scene + Camera@Graphics (AEBVScene@2@ AEBVCamera@2@) —
    // the same export engine_funcs resolves as fn_Graphics_Renderer_Draw. The wrong
    // name resolved NULL → Draw's Renderer::Draw call crashed (EXEC of 0). Prefer the
    // already-resolved global; fall back to the corrected mangled name.
    fn_Graphics_Renderer_Draw_local = reinterpret_cast<fn_Graphics_Renderer_Draw_local_t>(
        fn_Graphics_Renderer_Draw);
    if (!fn_Graphics_Renderer_Draw_local) {
        fn_Graphics_Renderer_Draw_local = reinterpret_cast<fn_Graphics_Renderer_Draw_local_t>(
            ResolveExport("?Draw@Renderer@Graphics@@QEAAXAEAVContext@Video@@MAEBVScene@2@AEBVCamera@2@@Z"));
    }
    fn_Graphics_Renderer_Dtor = reinterpret_cast<fn_Graphics_Renderer_Dtor_t>(
        ResolveExport("??1Renderer@Graphics@@QEAA@XZ"));
    fn_GUL_AudioSceneUpdate = reinterpret_cast<fn_GUL_AudioSceneUpdate_t>(
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
    // fn_MainLoop_SetNextLoop — resolved by EngineFuncsInit()
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
    if (!fn_ExternalList_Find) {
        // Fallback: read from exe dispatch slot 0x140152160 (used by Startup at 0x140097c9f)
        uintptr_t dispatchAddr = ResolveAddr(Module::Exe, 0x140152160);
        if (dispatchAddr)
            fn_ExternalList_Find = *reinterpret_cast<fn_ExternalList_Find_t*>(dispatchAddr);
    }
    fn_Filename_CtorCStr_Local = reinterpret_cast<fn_Filename_CtorCStr_Local_t>(
        ResolveExport("??0Filename@System@@QEAA@PEBD@Z"));
    // fn_Filename_Dtor — resolved by EngineFuncsInit()
    // fn_InternalBasePtr_Ctor — resolved by EngineFuncsInit()

    // Dialog type ID
    g_pDialogTypeId = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4138));
    // Standalone: the exe's Dialog type-id storage at 0x1401c4138 is unregistered,
    // so ResolveAddr returns 0. Fall back to the runtime type registry — "Dialog"
    // (parent External) is self-registered by Runtime at startup, so
    // GetTypeIdByName yields the same id the exe slot would hold.
    if (!g_pDialogTypeId) {
        static uint32_t s_dialogTypeId = 0;
        if (s_dialogTypeId == 0) s_dialogTypeId = ::Object::ResolveTypeIdByNameLazy("Dialog");
        g_pDialogTypeId = &s_dialogTypeId;
    }

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
    CHECK_FN(fn_GUL_AudioSceneUpdate);
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

// In-game tool-injection harness (standalone/tool_harness.cpp), env-gated by
// BANISHED_TOOL_HARNESS. No-op unless that env var is set.
extern "C" void ToolHarness_Tick(void* entityUI, int frameNum);
extern "C" void CitizenDeathHarness_Tick(int frameNum);   // standalone/death_harness.cpp
extern "C" void PastureFillHarness_Tick(int frameNum);    // standalone/pasture_harness.cpp
// TownHallUI gate diagnostic (code/Game/Dialog/townhallui.cpp) — one-shot report
// of the fn-pointer / type-id / town-data gates behind the town-hall panel.
extern "C" void TownHallSA_Diag(int frameNum);

// fn_MemFreeStatic is already available from engine_funcs.h

// ============================================================
// GameUpdateLoop::Update (0x140096380) — Main simulation tick
// ============================================================
// This is the core game loop tick. Called once per frame by MainLoop::Frame.
// Runs the simulation N times (speed multiplier), then updates UI, audio, etc.
//
// HEAVILY INSTRUMENTED for debugging tutorial hang.

extern "C" unsigned long long __stdcall GetTickCount64(void);   // kernel32 (fps trace)

// ============================================================
// [SIMPROF] Per-phase frame profiler (env BANISHED_SIMPROF)
// ============================================================
// Diagnostic only: wall-clock cost of each sim phase, so we can tell whether a
// speed-multiplier slowdown is a linear "N sim iterations" cost or something
// superlinear. Reports every BANISHED_SIMPROF frames (default 120) and resets.
// Zero cost when off (one predictable branch per probe).
// QueryPerformanceCounter/Frequency come from <windows.h> (LARGE_INTEGER), which
// this TU already pulls in; the wrappers below keep the call sites on plain int64.

namespace {

enum SimPhase {
    kPhTimedEvents = 0, kPhAnimation, kPhEntityList, kPhWeather,
    kPhCitizenList, kPhPostSim, kPhWholeUpdate, kPhCount
};
const char* const kPhaseName[kPhCount] = {
    "TimedEvents", "Animation", "EntityList", "Weather",
    "CitizenList", "PostSim(UI/cam/audio)", "TOTAL Update"
};

int      g_simProfOn   = -1;      // -1 = unresolved, 0 = off, else report interval
long long g_qpcFreq    = 0;
double   g_phSec[kPhCount] = {0};
int      g_phCalls[kPhCount] = {0};
int      g_profFrames  = 0;
long long g_profSimIters = 0;

inline bool SimProfOn() {
    if (g_simProfOn < 0) {
        const char* e = std::getenv("BANISHED_SIMPROF");
        int n = (e && e[0] && e[0] != '0') ? std::atoi(e) : 0;
        if (e && e[0] && e[0] != '0' && n <= 1) n = 120;   // "1" => default interval
        g_simProfOn = n;
        if (n) { LARGE_INTEGER f; QueryPerformanceFrequency(&f); g_qpcFreq = f.QuadPart; }
    }
    return g_simProfOn > 0;
}
inline long long QpcNow() { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; }

struct PhaseTimer {
    int ph; long long t0; bool on;
    explicit PhaseTimer(int p) : ph(p), t0(0), on(SimProfOn()) { if (on) t0 = QpcNow(); }
    ~PhaseTimer() {
        if (!on || !g_qpcFreq) return;
        g_phSec[ph] += (double)(QpcNow() - t0) / (double)g_qpcFreq;
        g_phCalls[ph]++;
    }
};

// Emit the accumulated report and reset the window.
void SimProfReport(int speedMultiplier, long entCount) {
    if (++g_profFrames < g_simProfOn) return;
    double total = g_phSec[kPhWholeUpdate];
    double msPerFrame = total * 1000.0 / g_profFrames;
    std::fprintf(stderr,
        "[SIMPROF] speed=%d frames=%d  %.2f ms/frame (%.1f fps equivalent)  "
        "simIters/frame=%.2f entities~=%ld\n",
        speedMultiplier, g_profFrames, msPerFrame,
        msPerFrame > 0 ? 1000.0 / msPerFrame : 0.0,
        (double)g_profSimIters / (double)g_profFrames, entCount);
    for (int i = 0; i < kPhCount; i++) {
        if (!g_phCalls[i]) continue;
        double ms = g_phSec[i] * 1000.0 / g_profFrames;
        std::fprintf(stderr,
            "[SIMPROF]   %-22s %8.3f ms/frame  %6.2f%%  calls/frame=%.2f  %.4f ms/call\n",
            kPhaseName[i], ms, total > 0 ? g_phSec[i] * 100.0 / total : 0.0,
            (double)g_phCalls[i] / (double)g_profFrames,
            g_phSec[i] * 1000.0 / (double)g_phCalls[i]);
    }
    std::fflush(stderr);
    for (int i = 0; i < kPhCount; i++) { g_phSec[i] = 0; g_phCalls[i] = 0; }
    g_profFrames = 0;
    g_profSimIters = 0;
}

// [FORCE SPEED] env BANISHED_FORCE_SPEED=N — pin the EntityUI speed multiplier
// (+0x34) and clear the paused flag (+0x30) every frame, so an A/B of 1x vs 2x
// vs 3x is deterministic under autostart with no clicking. Diagnostic only.
int g_forceSpeed = -1;
inline void ApplyForceSpeed(void* entityUIv) {
    if (g_forceSpeed < 0) {
        const char* e = std::getenv("BANISHED_FORCE_SPEED");
        g_forceSpeed = (e && e[0]) ? std::atoi(e) : 0;
    }
    if (g_forceSpeed > 0 && entityUIv) {
        uint8_t* ui = (uint8_t*)entityUIv;
        *(char*)(ui + 0x30) = 0;                       // not paused
        *(int32_t*)(ui + 0x34) = g_forceSpeed;         // speed multiplier
    }
}

}  // namespace

extern "C"
bool GameUpdateLoop_Update(GameUpdateLoop* self, void* time) {
    // Stage C entry guard (TEMPORARY): GameUpdateLoop::Load (0x140096b00, the
    // in-game world build) is not yet implemented, so on the new-game path
    // m_game (+0xF8) is still null when the loader chain transitions in. Every
    // step below dereferences the unbuilt world, AND EnsureInit() below resolves
    // dispatch slots (e.g. 0x140152058 = AchievementInstance::Update) that are
    // unregistered in standalone and null-deref. So this guard MUST run before
    // EnsureInit. Hold with a no-op tick until Load runs — remove once Load
    // constructs the world (project_newgame_path_roadmap Stage C). The STAGE C
    // marker has already fired by this point.
    if (self->m_game == nullptr) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            fprintf(stderr, "[GameUpdateLoop::Update] world not built "
                    "(m_game==null) — Stage C cliff, holding (no-op tick)\n");
            fflush(stderr);
        }
        return true;
    }

    EnsureInit();

    s_frameNum++;

    // [SIMPROF] whole-Update stamp + deterministic speed override (both diagnostic-only)
    const bool simProf = SimProfOn();
    const long long frameT0 = simProf ? QpcNow() : 0;
    ApplyForceSpeed(self->m_entityUI);

    // [OBJAUDIT] env BANISHED_OBJAUDIT: periodic InternalFactory handle-table integrity
    // scan (table[i]+0x08 == i for every i) plus the running tally of objects freed while
    // still registered. A growing `count` with a flat live-object count means stale entries
    // are accumulating; `bad>0` means the table is already corrupt. Interval is
    // BANISHED_OBJAUDIT_FRAMES (default 300).
    if (ObjAudit::Enabled()) {
        static int interval = 0;
        if (interval == 0) {
            const char* v = std::getenv("BANISHED_OBJAUDIT_FRAMES");
            interval = (v && std::atoi(v) > 0) ? std::atoi(v) : 300;
        }
        if (s_frameNum % interval == 0) {
            char tag[32];
            std::snprintf(tag, sizeof(tag), "frame%d", s_frameNum);
            ObjAudit::Scan(tag);
            ObjAudit::ReportLeaks(tag);
        }
    }

    // [EVENTS] RETIRED: the BANISHED_LIVE_EVENTS resource-shortage scanner used to
    // run here (a second port of FUN_14008c100 via EventLog_CheckResourceShortages).
    // It is superseded by the registered tick path: CitizenBehavior_CheckThresholds
    // (citizenbehavior.cpp) scans the SAME townMgr+0x170/+0x180 shortage tables every
    // tick and, now that 0x140095580 (EventLog_AddEvent) is registered as
    // fn_NotificationEvent, fires the same 0x11-0x18 sev-3 events. Keeping this block
    // would double-decrement those shared cooldown timers and double-log each
    // shortage, so it is removed — following the "retire stale duplicate" precedent.
    // EventLog_CheckResourceShortages stays defined (faithful port) but uncalled.
    // The scenario/achievement popup (0x1a-0x1d) source op_6830 (0x140096830) and the
    // ribbon / EventPopupDialog paths are unaffected and stay live for any real event.

    // [FPS TRACE] env BANISHED_FPS_TRACE: wall-clock fps over 120-frame windows (entry-to-
    // entry period). Also reports the entity count so we can correlate fps with world size.
    {
        static int s_fpsOn = -1;
        if (s_fpsOn < 0) { const char* e = std::getenv("BANISHED_FPS_TRACE"); s_fpsOn = (e && e[0] && e[0] != '0') ? 1 : 0; }
        if (s_fpsOn == 1) {
            static unsigned long long s_lastMs = 0;
            static int s_winFrames = 0;
            if (s_lastMs == 0) s_lastMs = GetTickCount64();
            if (++s_winFrames >= 120) {
                unsigned long long now = GetTickCount64();
                double secs = (double)(now - s_lastMs) / 1000.0;
                double fps = secs > 0 ? s_winFrames / secs : 0.0;
                long entCount = -1;
                void* el = self->m_entityList;
                if (el) entCount = *reinterpret_cast<int*>((uint8_t*)el + 0x30);  // EntityList count (approx)
                fprintf(stderr, "[FPS] %.1f fps (%d frames / %.2fs) entityListCount~=%ld\n",
                        fps, s_winFrames, secs, entCount);
                fflush(stderr);
                s_lastMs = now; s_winFrames = 0;
            }
        }
    }

    // Drive a Player tool (env-gated, one-shot) so otherwise-dormant tool code
    // paths fire for debugger validation. No-op unless BANISHED_TOOL_HARNESS set.
    ToolHarness_Tick(self->m_entityUI, s_frameNum);

    // Kill N citizens on a chosen frame so the removal teardown (HandleLifeEvent ->
    // MarkForRemoval -> the +0x38 broadcast -> CitizenComponent::OnRemove) can be
    // exercised without waiting decades of game time for a natural death. No-op
    // unless BANISHED_KILL_CITIZENS is set.
    CitizenDeathHarness_Tick(s_frameNum);

    // One-shot pasture stocking for the animal-wave test rig. No-op unless
    // BANISHED_PASTURE_FILL is set (standalone/pasture_harness.cpp).
    PastureFillHarness_Tick(s_frameNum);

    // TownHallUI gate diagnostic (one-shot at frame 120): reports missing fn
    // pointers / type-ids / town data that would silently blank the town-hall
    // statistics panel (code/Game/Dialog/townhallui.cpp).
    TownHallSA_Diag(s_frameNum);

    // Apply the deferred founding-house "occupied" visuals now that the scene is live
    // (the Scene mesh-visual can't run during the build-time founding pass — it
    // deadlocks; see g_wpcRosterOnly). One-shot, drains its queue on an early frame.
    extern void StartTown_ApplyPendingOccupiedVisuals(int frameNum);  // C linkage (file is extern "C")
    StartTown_ApplyPendingOccupiedVisuals(s_frameNum);

    // (StatusIcon "no workers" warnings are now raised by the genuine event
    // WorkPlaceComponent_SetAccepting at workplace activation and cleared by
    // WorkplaceComponent_Attach — the per-frame occupancy-poll drain was removed.
    // See code/Game/Component/statusiconcomponent.cpp.)

    // (StorageDisplayComponent pile rendering is now driven natively by EntityList::Update's
    // 5-bucket +0x48 tick — the explicit per-frame drain was removed once the spurious
    // bit-0x400 cull on start-town buildings was fixed. See storagedisplaycomponent.cpp.)

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
                { PhaseTimer pt(kPhTimedEvents);
                if (fn_Game_UpdateTimedEvents) fn_Game_UpdateTimedEvents(timedEventQueue, time); // [GUARD-M5]
                }
                TRACE("[GameUpdateLoop::Update] Game::UpdateTimedEvents done\n");

                // 3b. AnimationManager::Update
                bool rootOnly = (iteration < speedMultiplier - 1);
                if (g_pp_rootOnlyUpdates && *g_pp_rootOnlyUpdates) {
                    **g_pp_rootOnlyUpdates = rootOnly;
                }
                TRACE("[GameUpdateLoop::Update] calling AnimationManager::Update (rootOnly=%d)...\n", (int)rootOnly);
                { PhaseTimer pt(kPhAnimation);
                if (fn_AnimationManager_Update) fn_AnimationManager_Update(self->m_animationManager, time, rootOnly); // [GUARD-M5] slot 0x140152050 unregistered
                }
                TRACE("[GameUpdateLoop::Update] AnimationManager::Update done\n");

                // 3c. EntityList::Update (component ticks)
                // Uses custom reimpl — original fn_EntityList_Update (0x1400c4090)
                // hangs due to a spinlock issue in the hooked environment.
                TRACE("[GameUpdateLoop::Update] calling EntityList::Update (entityList=%p)...\n", self->m_entityList);
                { PhaseTimer pt(kPhEntityList);
                EntityList_Update(reinterpret_cast<EntityList*>(self->m_entityList), time);
                }
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
                { PhaseTimer pt(kPhWeather);
                if (fn_Weather_ResetTime) fn_Weather_ResetTime(self->m_weather, time); // [GUARD-M5]
                }
                TRACE("[GameUpdateLoop::Update] Weather_ResetTime done\n");

                // 3f. CitizenBehavior::Update / Map::Update
                TRACE("[GameUpdateLoop::Update] calling CitizenList::Update (citizenList=%p)...\n", self->m_citizenList);
                { PhaseTimer pt(kPhCitizenList);
                if (fn_CitizenList_Update) fn_CitizenList_Update(self->m_citizenList, time); // [GUARD-M5]
                }
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
                if (simProf) g_profSimIters++;   // [SIMPROF]
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
            // g_ppAchievementManager (exe slot 0x1401514f0) is unregistered in
            // standalone (M5 first slice) — guard the triple-deref. Re-enable the
            // AchievementManager tick once that singleton/slot is wired.
            if (g_ppAchievementManager && *g_ppAchievementManager && **g_ppAchievementManager) {
                void* achievementMgr = **g_ppAchievementManager;
                fn_AchievementManager_Update(achievementMgr, scaledTime);
            }
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
    // [SIMPROF] everything from here to EXIT is the once-per-render-frame work
    // (ambient/terrain/UI/tools/camera/audio) — NOT multiplied by game speed.
    { PhaseTimer ptPost(kPhPostSim);

    // Step 7: Ambient sound update — fn_AmbientSound_Update (exe FUN_1400a40e0)
    // not yet wired in standalone. [GUARD-M5]
    TRACE("[GameUpdateLoop::Update] calling AmbientSound::Update...\n");
    if (fn_AmbientSound_Update) fn_AmbientSound_Update(self->m_ambientSoundData);
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
            // Standalone: EntityUI::Update (the camera/tool controller) is not
            // registered, so fn_EntityUI_Update is null and the camera never moved.
            // Drive the camera directly from the KeyMap here — hotkeys (pause/presets)
            // first, then per-frame movement, matching the original helper order.
            Game::Camera_UpdateHotkeys(self->m_entityUI, noInputFocus ? 1 : 0, *(int64_t*)time);
            Game::Camera_UpdateMovement(self->m_entityUI, noInputFocus ? 1 : 0, *(int64_t*)time);
            // Phase 3: tick the active player tool (the tool half of EntityUI::Update
            // that camera_input.cpp omitted) — OnKey (dialog + left-click confirm) +
            // Serialize (ghost preview) + cursor. -1 = no active tool -> keep default.
            if (LiveToolsEnabled()) {
                int toolCursor = EntityUI_ToolTick_SA(self->m_entityUI, noInputFocus ? 1 : 0, *(int64_t*)time);
                if (toolCursor >= 0) cursorIndex = toolCursor;
            }
            TRACE("[GameUpdateLoop::Update] calling EntityUI::Update (inputFlag=%d)...\n", (int)noInputFocus);
            int64_t result = fn_EntityUI_Update ? fn_EntityUI_Update(self->m_entityUI, *(int64_t*)time, noInputFocus) : 1; // [GUARD-M5]
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
        void* keyMapMgr = g_pKeyMapSingleton ? *g_pKeyMapSingleton : nullptr;  // [GUARD-M5]
        if (keyMapMgr) {
            uint64_t cursorVal = *(uint64_t*)((uintptr_t)keyMapMgr + 0x28 + (int64_t)cursorIndex * 8);
            fn_UI_SetCursor(self->m_userInterface, cursorVal);
        }
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

        if (fn_GUL_AudioSceneUpdate && fn_Time_Ctor_Seconds && fn_CalcWindFactor) {
            float seconds = fn_Time_AsSeconds(time);
            int64_t scaledTime[1];
            fn_Time_Ctor_Seconds(scaledTime, seconds * (float)speedMul * (*g_fAudioTimeScale));
            uint32_t windFactor = fn_CalcWindFactor(scaledTime);
            fn_GUL_AudioSceneUpdate(self->m_audioScene, *(int64_t*)time, scaledTime[0],
                                  self->m_random, listenerPos, windFactor);
        } else {
            TRACE("[GameUpdateLoop::Update] SKIP Audio::Scene::Update (NULL fptr)\n");
        }
    }
    TRACE("[GameUpdateLoop::Update] Audio::Scene::Update done\n");

    // Step 13: Quicksave/quickload key checks
    // Original asm at 0x1400966c1: triple deref from slot, dispatch call at 0x140151758
    // Match proven working pattern from dragtool/pathtool — no null guards (original has none).
    {
        void* keyMap = ***(void****)g_pKeyMapObjectExref;

        if (fn_KeyMap_Pressed(keyMap, 0, 0x21)) {
            self->m_quickSavePressed = 1;
        }
        if (fn_KeyMap_Pressed(keyMap, 0, 0x22)) {
            self->m_quickLoadPressed = 1;
        }
        TRACE("[GameUpdateLoop::Update] quicksave/quickload checked (save=%d, load=%d)\n",
              self->m_quickSavePressed, self->m_quickLoadPressed);
    }

    // Step 14: SaveLoad::Update if active
    if (self->m_saveLoad != nullptr && fn_SaveLoad_Update) {  // [GUARD-M5]
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

    }  // [SIMPROF] end kPhPostSim

    // [SIMPROF] close the whole-frame timer before reporting so TOTAL includes
    // this frame, then emit/reset on the reporting interval.
    if (simProf && g_qpcFreq) {
        g_phSec[kPhWholeUpdate] += (double)(QpcNow() - frameT0) / (double)g_qpcFreq;
        g_phCalls[kPhWholeUpdate]++;
        long entCount = -1;
        if (self->m_entityList)
            entCount = *reinterpret_cast<int*>((uint8_t*)self->m_entityList + 0x30);
        int spd = 0;
        if (self->m_entityUI && *(char*)((uint8_t*)self->m_entityUI + 0x30) == 0)
            spd = *(int32_t*)((uint8_t*)self->m_entityUI + 0x34);
        SimProfReport(spd, entCount);
    }

    TRACE("[GameUpdateLoop::Update] EXIT\n");
    return true;
}


// ============================================================
// GameUpdateLoop::Draw (0x140096010) — Render one interpolated frame
// ============================================================

extern "C"
void GameUpdateLoop_Draw(GameUpdateLoop* self, float alpha) {
    // Stage C entry guard (TEMPORARY): see GameUpdateLoop_Update — must run
    // before EnsureInit (which null-derefs an unregistered dispatch slot in
    // standalone). m_game is null until GameUpdateLoop::Load builds the world;
    // the draw path dereferences the unbuilt world (UI, renderer, scenes).
    if (self->m_game == nullptr) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            fprintf(stderr, "[GameUpdateLoop::Draw] world not built "
                    "(m_game==null) — Stage C cliff, skipping draw\n");
            fflush(stderr);
        }
        return;
    }

    EnsureInit();

    // Tick the RenderDoc capture state machine once per rendered frame. The
    // engine's Present happens after this Draw returns (MainLoop::Draw EndFrame),
    // so a StartFrameCapture here brackets a full frame of GPU work.
    RenderDocIntegration::OnFrame();
    // One-shot in-game capture: BANISHED_CAPTURE_INGAME=N arms a RenderDoc
    // frame capture N in-game frames after the first drawn frame (default 120,
    // ~enough for the camera to settle on the start town). Requires the exe to
    // be launched under RenderDoc (renderdoc.dll injected). Used to inspect the
    // terrain draw's PS bindings + shadow cbuffer with shadows ON.
    {
        static int s_capArm = -2;      // -2=unread, -1=disabled, >=0 = countdown to arm
        if (s_capArm == -2) {
            const char* e = std::getenv("BANISHED_CAPTURE_INGAME");
            s_capArm = (e && e[0]) ? (atoi(e) > 0 ? atoi(e) : 120) : -1;
        }
        if (s_capArm >= 0) {
            if (s_capArm == 0) {
                RenderDocIntegration::RequestCaptureInFrames(0);
            }
            --s_capArm;
        }
    }

    TRACE("[GameUpdateLoop::Draw] ENTER (alpha=%.3f)\n", alpha);

    // TEMP diagnostic: confirm the in-game render present runs per-frame + how many
    // animation groups the AnimationManager holds (this+0x08) — i.e. whether any
    // MeshInstance registered an animation (0 ⇒ AnimationManager::Update no-ops).
    if (std::getenv("BANISHED_DRAW_TRACE")) {
        static long s_drawN = 0;
        if ((s_drawN % 120) == 0) {
            int animGroups = self->m_animationManager
                ? *reinterpret_cast<int*>((uint8_t*)self->m_animationManager + 0x08) : -1;
            fprintf(stderr, "[DrawTick] frame=%ld alpha=%.3f paused=%d animGroups=%d\n",
                    s_drawN, alpha, (int)fn_UI_PausesGame(self->m_userInterface), animGroups);
            fflush(stderr);
        }
        ++s_drawN;
    }
    // [DRAW FPS] env BANISHED_FPS_TRACE: the RENDER framerate (Draw calls/sec). The sim Update
    // runs fixed-timestep 60Hz (multiple steps per rendered frame to catch up), so the Update
    // counter always reads ~60; THIS measures the visible frame rate. Compare to the [FPS]
    // (Update) line: if Draw << Update, rendering is the bottleneck.
    if (std::getenv("BANISHED_FPS_TRACE")) {
        static unsigned long long s_dLastMs = 0;
        static int s_dFrames = 0;
        if (s_dLastMs == 0) s_dLastMs = GetTickCount64();
        if (++s_dFrames >= 120) {
            unsigned long long now = GetTickCount64();
            double secs = (double)(now - s_dLastMs) / 1000.0;
            fprintf(stderr, "[DRAW-FPS] %.1f fps (%d draws / %.2fs)\n",
                    secs > 0 ? s_dFrames / secs : 0.0, s_dFrames, secs);
            fflush(stderr);
            s_dLastMs = now; s_dFrames = 0;
        }
    }

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
        // fn_ImageCapture_DrawCheck (exe FUN_1400a5140) is our clean port
        // Minimap_PaintTerrain in standalone (see Startup fallback above). It fires
        // once — when the minimap's terrain GPU buffer (imageCapture+0x08) is still
        // null — to render the terrain top-down into the minimap pixel arrays.
        if (fn_ImageCapture_DrawCheck && *(int64_t*)(imageCapture + 8) == 0) {
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
        if (fn_Weather_DrawUpdate) fn_Weather_DrawUpdate(self->m_weather, videoCtx); // [GUARD-M5]
        // Native time-of-day sun/lighting. The original Weather::DrawUpdate
        // (above) no-ops in standalone (not reimplemented + Environment.rsc not
        // loaded), so drive the day cycle directly from the real base-game
        // keyframes. Animates sun elevation + light/ambient/fog colors so
        // shadows track the in-game day (see weather_sun.cpp).
        Weather_SunUpdate(self, videoCtx);

        // The original Weather::DrawUpdate (exe 0x1400938d0) ALSO pushes four
        // terrain weather params from the Weather struct into the Context
        // cbuffer (mirrored at ctx+0x288 and ctx+0x3b8, dirty bit 0x10800):
        //   weather+0x1c = SNOW COVERAGE   (FUN_140093420: saturate((snowAccum -
        //                  thresh) * scale))
        //   weather+0x20 = seasonal fade A
        //   weather+0x24 = seasonal fade B
        //   weather+0x28 = max active weather (rain/snow) intensity
        // In standalone DrawUpdate no-ops AND the weather sim (Weather::ResetTime
        // 0x1400944e0 -> FUN_140093420) never runs, so these were never written
        // and the terrain shader read stale/garbage from those cbuffer slots —
        // showing snow on ~half the ground. The Weather ctor (FUN_140093f60)
        // zero-inits all four, which is exactly the spring/clear/no-snow state,
        // so push them faithfully (they animate for free once the snow sim is
        // wired). Set BANISHED_FORCE_SNOW=<float> to override the coverage for
        // testing.
        {
            uint8_t* weather = reinterpret_cast<uint8_t*>(self->m_weather);
            if (weather && videoCtx) {
                uint8_t* ctx = reinterpret_cast<uint8_t*>(videoCtx);
                uint32_t s0 = *reinterpret_cast<uint32_t*>(weather + 0x1c);  // snow coverage
                uint32_t s1 = *reinterpret_cast<uint32_t*>(weather + 0x20);
                uint32_t s2 = *reinterpret_cast<uint32_t*>(weather + 0x24);
                uint32_t s3 = *reinterpret_cast<uint32_t*>(weather + 0x28);
                static int s_forceSnow = -1;
                static float s_snowVal = 0.0f;
                if (s_forceSnow < 0) {
                    const char* e = std::getenv("BANISHED_FORCE_SNOW");
                    s_forceSnow = (e && e[0]) ? 1 : 0;
                    if (s_forceSnow) s_snowVal = (float)atof(e);
                }
                if (s_forceSnow) s0 = *reinterpret_cast<uint32_t*>(&s_snowVal);
                // Optional verification trace: BANISHED_SUN_TRACE=1 dumps the four
                // weather/terrain params (as floats) we push to the Context cbuffer
                // every ~120 frames. s0 = snow coverage -> _pc_.userPosition.x.
                {
                    static int s_snowTrace = -1;
                    if (s_snowTrace < 0) {
                        const char* e = std::getenv("BANISHED_SUN_TRACE");
                        s_snowTrace = (e && e[0] && e[0] != '0') ? 1 : 0;
                    }
                    if (s_snowTrace) {
                        static int s_k = 0;
                        if ((s_k++ % 120) == 0) {
                            fprintf(stderr, "[snow] coverage=%.3f fadeA=%.3f fadeB=%.3f "
                                    "wxIntensity=%.3f (forceSnow=%d)\n",
                                    *reinterpret_cast<float*>(&s0),
                                    *reinterpret_cast<float*>(&s1),
                                    *reinterpret_cast<float*>(&s2),
                                    *reinterpret_cast<float*>(&s3), s_forceSnow);
                            fflush(stderr);
                        }
                    }
                }
                *reinterpret_cast<uint32_t*>(ctx + 0x3b8) = s0;
                *reinterpret_cast<uint32_t*>(ctx + 0x3bc) = s1;
                *reinterpret_cast<uint32_t*>(ctx + 0x3c0) = s2;
                *reinterpret_cast<uint32_t*>(ctx + 0x3c4) = s3;
                *reinterpret_cast<uint32_t*>(ctx + 0x288) = s0;
                *reinterpret_cast<uint32_t*>(ctx + 0x28c) = s1;
                *reinterpret_cast<uint32_t*>(ctx + 0x290) = s2;
                *reinterpret_cast<uint32_t*>(ctx + 0x294) = s3;
                *reinterpret_cast<uint32_t*>(ctx + 0x10) |= 0x10800;
            }
        }
    }
    TRACE("[GameUpdateLoop::Draw] Weather::DrawUpdate done\n");

    // Step 6: Copy renderer shadow mode from engine.
    // renderer+0x3c is the shadow mode: Renderer::AddScene (Runtime 0x180105700) stores it
    // as the main draw group's +0xe8, and Renderer::Draw (0x180104bb0) reads it back into
    // renderer+0x3c before dispatching each global object's vtable+0x48 (DrawShadowMap) /
    // vtable+0x50 (Draw). SunLight::DrawShadowMap (0x180106bc0) is gated by
    // `if (renderer+0x3c != 0)`; SunLight::Draw (0x1801067b0, main pass) is NOT — it always
    // runs Context::SetLight + Renderer::DrawItemList to light and render the terrain.
    {
        void* engine = *g_pEngineSingleton;
        uint32_t shadowMode = *(uint32_t*)((uintptr_t)engine + 0x30);
        // The shadow-mode source field. The ORIGINAL GameUpdateLoop::Draw (exe 0x140096010)
        // sets renderer+0x3c from *(*g_pVideoConfigSingleton + 0x30) — the shadow-QUALITY
        // setting (index, 0=off, >0=on), NOT engine+0x30 (in standalone g_pEngineSingleton ==
        // MainLoop, so engine+0x30 is garbage). Shadows are now DEFAULT-ON: mirror the
        // original and read VideoConfig+0x30 so SunLight::DrawShadowMap engages (verified
        // crash-free; terrain + buildings cast/receive). The day cycle is driven by
        // Weather_SunUpdate above. BANISHED_DISABLE_SHADOWS=1 forces it off for A/B.
        static int s_disableShadows = -1;
        if (s_disableShadows < 0) {
            const char* e = std::getenv("BANISHED_DISABLE_SHADOWS");
            s_disableShadows = (e && e[0] && e[0] != '0') ? 1 : 0;
        }
        if (!s_disableShadows && g_pVideoConfigSingleton && *g_pVideoConfigSingleton) {
            shadowMode = *(uint32_t*)((uintptr_t)*g_pVideoConfigSingleton + 0x30);
        } else {
            shadowMode = 0;
        }
        *(uint32_t*)((uintptr_t)self->m_renderer + 0x3c) = shadowMode;
    }

    // Step 7: Graphics::Renderer::Draw
    TRACE("[GameUpdateLoop::Draw] calling Renderer::Draw...\n");
    {
        void* engine = *g_pEngineSingleton;
        void* videoCtx = *(void**)((uintptr_t)engine + 0x10);
        fn_Graphics_Renderer_Draw_local(self->m_renderer, videoCtx, alpha,
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

        // Original copies Transform3 (12 floats at +0x98..+0xC4 = 48 bytes) and projection
        // matrix (16 floats at +0xC8) from PlayerController onto the stack, then calls
        // AsMatrix → SetCamera. Math::Transform3 is a 48-byte struct (3x4). Copying only
        // 7 floats (the earlier bug) left AsMatrix reading 20 bytes of stack garbage —
        // including the camera translation — producing a corrupt view matrix that threw
        // the 3D placement overlay behind the camera (RenderDoc VS-out w<0). Terrain was
        // unaffected (drawn earlier inside Renderer::Draw with its own camera).
        // AsMatrix returns Matrix by value (64 bytes) — requires hidden return pointer ABI.
        uint8_t transform3[48]; // 12 floats = Transform3 (48 bytes)
        for (int i = 0; i < 12; i++) {
            ((uint32_t*)transform3)[i] = *(uint32_t*)(pc + 0x98 + i * 4);
        }

        // AsMatrix: RCX=this(Transform3*), RDX=hidden retval(Matrix*), returns RAX=retval
        uint8_t viewMatrix[64]; // 4x4 float Matrix output buffer
        fn_Transform3_AsMatrix(transform3, viewMatrix);

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
    // [HUD-DIAG] Unconditional one-shot: confirm Startup is invoked at all, and
    // whether the world is already built (m_game set) when it runs. If Startup
    // fires before Load (m_game==null), it early-returns forever and the HUD
    // would never be created — an ordering bug we must catch here.
    {
        static bool s_called = false;
        if (!s_called) {
            s_called = true;
            fprintf(stderr, "[HUD-DIAG] Startup CALLED self=%p m_game=%p "
                    "m_userInterface=%p m_resourceList(+0x08)=%p\n",
                    (void*)self,
                    self ? (void*)self->m_game : nullptr,
                    self ? (void*)self->m_userInterface : nullptr,
                    self ? (void*)self->m_resourceList : nullptr);
            fflush(stderr);
        }
    }

    // Stage C entry guard (TEMPORARY): see GameUpdateLoop_Update — must run
    // before EnsureInit (which null-derefs an unregistered dispatch slot in
    // standalone). On the new-game path m_game (+0xF8) is null until
    // GameUpdateLoop::Load builds the world; Startup dereferences m_userInterface
    // (+0x98, also null) below. Skip cleanly until Load is implemented.
    if (self->m_game == nullptr) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            fprintf(stderr, "[GameUpdateLoop::Startup] world not built "
                    "(m_game==null) — Stage C cliff, skipping startup\n");
            fflush(stderr);
        }
        return;
    }

    EnsureInit();

    TRACE("[GameUpdateLoop::Startup] ENTER (self=%p)\n", (void*)self);

    // Set audio scene on UI
    TRACE("[GameUpdateLoop::Startup] setting audio scene on UI...\n");
    *(void**)((uintptr_t)self->m_userInterface + 0x90) = self->m_audioScene;

    // Setup UI references (cursors, lists, etc.)
    // fn_SetupUIRefs (exe FUN_140099260) is not reimplemented/registered in
    // standalone yet (M5 first slice) — guard until it lands. [GUARD-M5]
    TRACE("[GameUpdateLoop::Startup] calling SetupUIRefs...\n");
    if (fn_SetupUIRefs) fn_SetupUIRefs(*g_pKeyMapSingleton, self->m_userInterface);
    TRACE("[GameUpdateLoop::Startup] SetupUIRefs done\n");

    // Set default cursor
    TRACE("[GameUpdateLoop::Startup] setting default cursor...\n");
    {
        void* keyMapMgr = g_pKeyMapSingleton ? *g_pKeyMapSingleton : nullptr;  // [GUARD-M5]
        if (keyMapMgr) {
            uint64_t cursorVal = *(uint64_t*)((uintptr_t)keyMapMgr + 0x30);
            fn_UI_SetCursor(self->m_userInterface, cursorVal);
        }
    }

    // Enable cursor
    TRACE("[GameUpdateLoop::Startup] enabling cursor...\n");
    fn_UI_EnableCursor(self->m_userInterface, true);

    // ProfessionList startup — builds the sorted profession ARRAY at profList+0x20/+0x28
    // (FUN_1400a90a0) for the profession UI panel. m_professionList (gw+0xb0) is normally
    // DEFERRED (null) in the standalone slice; it goes live under BANISHED_LIVE_PROFESSIONLIST.
    // MUST guard the function pointer too: fn_ProfessionList_Startup (exe 0x1400a90a0) is
    // unregistered in standalone (resolves NULL), so once m_professionList is non-null an
    // unguarded call faulted EXEC-of-0 in GameUpdateLoop_Startup. The array is only read by
    // the profession UI (count at +0x28); leaving it empty is safe — the per-tick housing/
    // workplace finders key on the default profession (+0x30) / profRef2 (+0x38) from Serialize.
    TRACE("[GameUpdateLoop::Startup] calling ProfessionList::Startup...\n");
    if (self->m_professionList && fn_ProfessionList_Startup)
        fn_ProfessionList_Startup(self->m_professionList);
    TRACE("[GameUpdateLoop::Startup] ProfessionList::Startup done\n");

    // Game HUD dialogs (Toolbar/EventNotify/AutoSave) are DEFERRED in the M5 first
    // slice: they require the loop's resource list (m_resourceList, +0x08 — the
    // standalone loader does not populate it yet, so ExternalList::Find faults) AND
    // their exe dialog ctors (not reimplemented). Disable the whole section until
    // both are wired. [GUARD-M5]
    // [HUD-DIAG] One-shot probe: did the preload populate the loop's resource
    // list (+0x08)? And does ExternalList::Find resolve the toolbar Dialog
    // resource out of it? This is the gate for enabling HUD creation below.
    {
        static bool s_diag = false;
        if (!s_diag) {
            s_diag = true;
            void* rl = self->m_resourceList;
            fprintf(stderr, "[HUD-DIAG] probe: m_resourceList=%p Find=%p FilenameCtor=%p "
                    "Dtor=%p DialogTypeId=%p(val=%u) ToolbarCtor=%p UI_AddDialog=%p IsDerivedFrom=%p\n",
                    rl, (void*)fn_ExternalList_Find, (void*)fn_Filename_CtorCStr_Local,
                    (void*)fn_Filename_Dtor, (void*)g_pDialogTypeId,
                    g_pDialogTypeId ? *g_pDialogTypeId : 0u,
                    (void*)fn_ToolbarDialog_Ctor, (void*)fn_UI_AddDialog, (void*)fn_IsDerivedFrom);
            fflush(stderr);
            if (rl && fn_ExternalList_Find && fn_Filename_CtorCStr_Local) {
                uint8_t fnbuf[8];
                fn_Filename_CtorCStr_Local(fnbuf, "Dialog/Toolbar.rsc");
                void* foundBase = nullptr;
                uint32_t typeInfo = g_pDialogTypeId ? *g_pDialogTypeId : 0u;
                fn_ExternalList_Find(rl, &foundBase, fnbuf, &typeInfo);
                fprintf(stderr, "[HUD-DIAG] ExternalList::Find(\"Dialog/Toolbar.rsc\") -> foundBase=%p\n",
                        foundBase);
                fflush(stderr);
                if (fn_Filename_Dtor) fn_Filename_Dtor(fnbuf);
            } else {
                fprintf(stderr, "[HUD-DIAG] Find probe skipped (resourceList or fns null)\n");
                fflush(stderr);
            }
        }
    }

    const bool kCreateGameDialogs = false;
    // HUD bring-up — the in-game Toolbar (Keystone F). Gated behind the env var
    // BANISHED_LIVE_TOOLBAR (default OFF → the default build stays byte-identical).
    // When set, ToolbarStandalone_Init() (toolbar_vtable_standalone.cpp) builds the
    // standalone ToolbarDialog vtable (exe 0x14015fa00 has no standalone counterpart)
    // and fills the toolbar type-id globals (g_TypeId_ToolbarItem 0x1401c6128 etc.),
    // and the resource is resolved via ExternalFactory::FindOrLoad (m_resourceList is
    // empty in standalone — the preload that populates it is off by default).
    // The Toolbar ctor (0x1400b9910 -> ToolbarDialog_Ctor_Impl) is already registered.
    //
    // NOTE: the toolbar dialog constructs + ticks crash-free with BANISHED_LIVE_TOOLBAR
    // alone, but its BUTTONS come from the "Toolbar" item externals, which are loaded by
    // the resource preload. So a fully POPULATED, visibly-rendered toolbar (10 buttons)
    // needs BANISHED_LIVE_TOOLBAR=1 *and* BANISHED_GAME_PRELOAD=1 — verified rendering
    // the real bottom-right toolbar bar on screen, crash-free. With LIVE_TOOLBAR alone
    // the dialog is present but empty (itemCount=0).
    static int s_toolbarGate = -1;
    if (s_toolbarGate < 0) {
        s_toolbarGate = ::ComponentFactory::LiveEnabled("BANISHED_LIVE_TOOLBAR") ? 1 : 0;
    }
    const bool kCreateToolbar = (s_toolbarGate != 0);

    // Captured toolbar dialog — the EventNotify ribbon parents to it (positions the
    // ribbon just below the toolbar; see EventNotifyDialog_Update). Null if the
    // toolbar was not created (LIVE_TOOLBAR off), in which case the ribbon Update
    // early-returns on the null parent, so we skip creating the ribbon too.
    void* g_ribbonParentToolbar = nullptr;

    // Create and add Toolbar dialog
    TRACE("[GameUpdateLoop::Startup] creating Toolbar dialog...\n");
    if (kCreateToolbar) {
        // Synthesize the standalone ToolbarDialog vtable + fill the toolbar type-id
        // globals before any toolbar object is constructed. Idempotent.
        ToolbarStandalone_Init();
        uint8_t filename[8];
        fn_Filename_CtorCStr_Local(filename, "Dialog/Toolbar.rsc");

        // Find the Dialog resource. First try the loop's resource list (populated
        // when preload is on); if that is empty or misses, fall back to
        // ExternalFactory::FindOrLoad (the path OpenToolDialog/ToggleAll use — loads
        // the .rsc directly, robust in standalone where m_resourceList is empty).
        void* foundBase = nullptr;
        uint32_t typeInfo = g_pDialogTypeId ? *g_pDialogTypeId : 0; // [GUARD-M5] slot 0x1401c4138 unregistered
        if (self->m_resourceList && fn_ExternalList_Find)
            fn_ExternalList_Find(self->m_resourceList, &foundBase, filename, &typeInfo);
        if (foundBase == nullptr && fn_ExternalFactory_FindOrLoad &&
            g_pExternalFactorySingleton && *g_pExternalFactorySingleton) {
            foundBase = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filename);
        }

        void* dialogRes = nullptr;
        if (foundBase != nullptr && g_pDialogTypeId) {
            if (fn_IsDerivedFrom(foundBase, *g_pDialogTypeId)) {
                dialogRes = foundBase;
            }
        }
        fn_Filename_Dtor(filename);

        fprintf(stderr, "[HUD] Toolbar: foundBase=%p dialogRes=%p Ctor=%p UI=%p\n",
                foundBase, dialogRes, (void*)fn_ToolbarDialog_Ctor, (void*)self->m_userInterface);
        fflush(stderr);

        // Allocate and construct ToolbarDialog (0x108 bytes). [GUARD-M5]
        void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0x108, 8);
        void* toolbar = nullptr;
        // Require a valid dialog resource — ToolbarDialog_Ctor_Impl reads the
        // toolbar item list out of it and faults on null. If type-id resolution
        // failed (dialogRes null), skip construction rather than crash.
        if (mem != nullptr && fn_ToolbarDialog_Ctor && dialogRes != nullptr) {
            int64_t nullParent[1] = {0};
            fn_InternalBasePtr_Ctor(nullParent, nullptr);
            toolbar = fn_ToolbarDialog_Ctor(mem, self->m_userInterface, dialogRes, nullParent, nullptr);
        }

        // Add to UI (only if the dialog was actually constructed).
        if (toolbar) {
            g_ribbonParentToolbar = toolbar;  // parent for the EventNotify ribbon
            int64_t dialogPtr[1];
            fn_InternalBasePtr_Ctor(dialogPtr, toolbar);
            fn_UI_AddDialog(self->m_userInterface, dialogPtr);
            fprintf(stderr, "[HUD] Toolbar CONSTRUCTED + added to UI: toolbar=%p itemCount(+0xD8)=%d "
                    "buttonCount(+0xC8)=%d speedCount(+0x100)=%d\n", toolbar,
                    *(int32_t*)((uintptr_t)toolbar + 0xD8), *(int32_t*)((uintptr_t)toolbar + 0xC8),
                    *(int32_t*)((uintptr_t)toolbar + 0x100));
        } else {
            fprintf(stderr, "[HUD] Toolbar NOT constructed (mem=%p Ctor=%p) — freeing\n",
                    mem, (void*)fn_ToolbarDialog_Ctor);
            if (mem) fn_MemFreeStatic(mem);
        }
        fflush(stderr);
    }
    TRACE("[GameUpdateLoop::Startup] Toolbar dialog added\n");

    // ----------------------------------------------------------------
    // Debug/cheat panel auto-open (BANISHED_DEBUG_PANEL). One-shot: loads
    // Dialog/Debug.rsc — whose Dialog resource declares DebugUI as its
    // controller — and adds it to the in-game UI. fn_DialogInstance_Ctor
    // (Runtime 0x180114870) instantiates the DebugUI controller via our promoted
    // standalone factory (see code/Game/Dialog/debugui.cpp) and the framework
    // dispatches its Slot4, binding the panel's combos/buttons/sliders/counters
    // to live game data. Off by default so the normal build is byte-identical.
    // The panel is otherwise opened in-game by the DebugMenu.pkm mod's trigger;
    // this env just pops it automatically for testing. Grep the log for
    // "[DebugUI]" to confirm the controller wired.
    {
        static bool s_debugPanelOpened = false;
        const char* dbgEnv = getenv("BANISHED_DEBUG_PANEL");
        const bool wantPanel = dbgEnv && dbgEnv[0] && !(dbgEnv[0] == '0' && dbgEnv[1] == '\0');
        if (!s_debugPanelOpened && wantPanel) {
            s_debugPanelOpened = true;
            if (fn_Filename_CtorCStr_Local && fn_ExternalFactory_FindOrLoad &&
                g_pExternalFactorySingleton && *g_pExternalFactorySingleton &&
                fn_MemAlloc && g_pMemAllocSingleton && fn_DialogInstance_Ctor &&
                fn_InternalBasePtr_Ctor && fn_UI_AddDialog && self->m_userInterface) {
                // The package stores the leaf name ("Debug.rsc"); the folder the
                // resource system prepends is uncertain (Toolbar/MainMenu live under
                // "Dialog/"). Probe candidate paths and use the first that resolves.
                static const char* kCandidates[] = {
                    "Dialog/Debug.rsc", "Debug.rsc", "Menu/Debug.rsc",
                    "Dialog/DebugMenu.rsc", "DebugMenu.rsc", "Dialog/Cheat.rsc",
                };
                void* found = nullptr;
                for (const char* cand : kCandidates) {
                    uint8_t fnbuf[8];
                    fn_Filename_CtorCStr_Local(fnbuf, cand);
                    void* r = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, fnbuf);
                    if (fn_Filename_Dtor) fn_Filename_Dtor(fnbuf);
                    fprintf(stderr, "[DebugUI] auto-open probe: '%s' -> %p\n", cand, r);
                    if (r) { found = r; break; }
                }
                // Accept the loaded resource if it is a Dialog (or External) — the
                // guard mirrors the toolbar/eventnotify open paths. Permissive when
                // the type-id slot is unregistered in standalone.
                void* dialogRes = nullptr;
                if (found &&
                    (!g_pDialogTypeId || fn_IsDerivedFrom(found, *g_pDialogTypeId) ||
                     (g_pExternalTypeId && fn_IsDerivedFrom(found, *g_pExternalTypeId))))
                    dialogRes = found;
                fprintf(stderr, "[DebugUI] auto-open: Debug.rsc found=%p accepted=%p UI=%p\n",
                        found, dialogRes, (void*)self->m_userInterface);
                if (dialogRes) {
                    void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0xB8, 8);  // DialogInstance base
                    if (mem) {
                        int64_t nullParent[1] = {0};
                        fn_InternalBasePtr_Ctor(nullParent, nullptr);
                        // (self, ui, dialogRes, parent, dataSource, extraCtrlData, flags)
                        void* dlg = fn_DialogInstance_Ctor(mem, self->m_userInterface,
                                                           dialogRes, nullParent, nullptr, nullptr, 0);
                        if (dlg) {
                            int64_t dialogPtr[1];
                            fn_InternalBasePtr_Ctor(dialogPtr, dlg);
                            fn_UI_AddDialog(self->m_userInterface, dialogPtr);
                            fprintf(stderr, "[DebugUI] auto-open: DialogInstance added dlg=%p "
                                    "(Slot4 fires on first layout/update)\n", dlg);
                        } else if (fn_MemFreeStatic) {
                            fn_MemFreeStatic(mem);
                            fprintf(stderr, "[DebugUI] auto-open: DialogInstance_Ctor returned null\n");
                        }
                    }
                } else {
                    fprintf(stderr, "[DebugUI] auto-open: Debug.rsc not found / wrong type — is "
                            "DebugMenu.pkm present in WinData?\n");
                }
                fflush(stderr);
            } else {
                fprintf(stderr, "[DebugUI] auto-open SKIPPED: a required engine fn/singleton is null\n");
                fflush(stderr);
            }
        }
    }

    // Create and add EventNotify ribbon (BANISHED_LIVE_EVENTS). Shows recent game
    // events as a strip below the toolbar. Uses the FindOrLoad fallback (empty
    // m_resourceList in standalone) like the toolbar, and parents to the toolbar
    // dialog so Update can position the ribbon. Skipped if the toolbar is absent
    // (the ribbon Update early-returns on a null parent anyway).
    TRACE("[GameUpdateLoop::Startup] creating EventNotify dialog...\n");
    static int s_eventsGate = -1;
    if (s_eventsGate < 0) {
        s_eventsGate = ::ComponentFactory::LiveEnabled("BANISHED_LIVE_EVENTS") ? 1 : 0;
    }
    if (s_eventsGate && g_ribbonParentToolbar != nullptr) {
        uint8_t filename[8];
        fn_Filename_CtorCStr_Local(filename, "Dialog/EventNotify.rsc");
        void* foundBase = nullptr;
        uint32_t typeInfo = g_pDialogTypeId ? *g_pDialogTypeId : 0;
        if (self->m_resourceList && fn_ExternalList_Find)
            fn_ExternalList_Find(self->m_resourceList, &foundBase, filename, &typeInfo);
        if (foundBase == nullptr && fn_ExternalFactory_FindOrLoad &&
            g_pExternalFactorySingleton && *g_pExternalFactorySingleton) {
            foundBase = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filename);
        }
        void* dialogRes = nullptr;
        if (foundBase != nullptr && g_pDialogTypeId && fn_IsDerivedFrom(foundBase, *g_pDialogTypeId))
            dialogRes = foundBase;
        fn_Filename_Dtor(filename);

        fprintf(stderr, "[HUD] EventNotify: foundBase=%p dialogRes=%p parentToolbar=%p\n",
                foundBase, dialogRes, g_ribbonParentToolbar);
        fflush(stderr);

        void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0xF0, 8);
        void* eventNotify = nullptr;
        if (mem != nullptr && dialogRes != nullptr) {
            int64_t parentIBP[1];
            fn_InternalBasePtr_Ctor(parentIBP, g_ribbonParentToolbar);
            eventNotify = EventNotifyDialog_Ctor_Standalone(mem, self->m_userInterface, dialogRes, parentIBP);
        }

        if (eventNotify) {
            int64_t dialogPtr[1];
            fn_InternalBasePtr_Ctor(dialogPtr, eventNotify);
            fn_UI_AddDialog(self->m_userInterface, dialogPtr);
            fprintf(stderr, "[HUD] EventNotify ribbon CONSTRUCTED + added to UI: %p\n", eventNotify);
            fflush(stderr);
        } else if (mem) {
            fn_MemFreeStatic(mem);
        }
    }
    TRACE("[GameUpdateLoop::Startup] EventNotify dialog added\n");

    // Create and add AutoSave dialog
    TRACE("[GameUpdateLoop::Startup] creating AutoSave dialog...\n");
    if (kCreateGameDialogs) {
        uint8_t filename[8];
        fn_Filename_CtorCStr_Local(filename, "Dialog/AutoSave.rsc");
        void* foundBase = nullptr;
        uint32_t typeInfo = g_pDialogTypeId ? *g_pDialogTypeId : 0; // [GUARD-M5] slot 0x1401c4138 unregistered
        fn_ExternalList_Find(self->m_resourceList, &foundBase, filename, &typeInfo);

        void* dialogRes = nullptr;
        if (foundBase != nullptr && g_pDialogTypeId) {
            if (fn_IsDerivedFrom(foundBase, *g_pDialogTypeId)) {
                dialogRes = foundBase;
            }
        }
        fn_Filename_Dtor(filename);

        // Allocate AutoSave dialog (0xD0 bytes). [GUARD-M5]
        void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0xD0, 8);
        void* autoSave = nullptr;
        if (mem != nullptr && fn_AutoSaveDialog_Ctor) {
            int64_t nullParent[1] = {0};
            fn_InternalBasePtr_Ctor(nullParent, nullptr);
            autoSave = fn_AutoSaveDialog_Ctor(mem, self->m_userInterface, dialogRes, nullParent);
        }

        if (autoSave) {
            int64_t dialogPtr[1];
            fn_InternalBasePtr_Ctor(dialogPtr, autoSave);
            fn_UI_AddDialog(self->m_userInterface, dialogPtr);
        } else if (mem) {
            fn_MemFreeStatic(mem);
        }
    }
    TRACE("[GameUpdateLoop::Startup] AutoSave dialog added\n");

    // Set fullscreen fade
    TRACE("[GameUpdateLoop::Startup] setting fullscreen fade...\n");
    fn_UI_SetFullscreenFade(self->m_userInterface, true, 0xFF000000, true);
    fn_UI_SetFullscreenFade(self->m_userInterface, false, 0xFF000000, false);
    TRACE("[GameUpdateLoop::Startup] fullscreen fade set\n");

    // EntityUI startup — fn_EntityUI_Startup (exe FUN_140101ee0) not yet wired in
    // standalone (M5 first slice). [GUARD-M5]
    TRACE("[GameUpdateLoop::Startup] calling EntityUI::Startup...\n");
    if (fn_EntityUI_Startup) fn_EntityUI_Startup(self->m_entityUI);
    // Standalone: fn_EntityUI_Startup (0x140101ee0) is NULL, so build the tool-mode
    // tree ourselves (Phase 3) so a build-button click resolves the PlacementTool.
    if (LiveToolsEnabled()) EntityUI_BuildToolModeTree_SA(self->m_entityUI);
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

    // Stage C entry guard (TEMPORARY): on the new-game path the world was never
    // built (GameUpdateLoop::Load is a stub), so m_game (+0xF8) is null and the
    // teardown below dereferences unbuilt subsystems (m_entityUI, m_userInterface,
    // m_audioScene, m_citizenList). Nothing was constructed, so there is nothing
    // to tear down — return. (Destroy still runs afterward to free the loop.)
    if (self->m_game == nullptr) {
        fprintf(stderr, "[GameUpdateLoop::Shutdown] world not built (m_game==null) "
                "— Stage C cliff, nothing to tear down\n");
        fflush(stderr);
        return;
    }

    // Clear achievement event handler
    TRACE("[GameUpdateLoop::Shutdown] clearing achievement event handler...\n");
    if (g_ppAchievementManager && *g_ppAchievementManager && **g_ppAchievementManager) {
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


// ============================================================
// GameUpdateLoop::Destroy (0x140019DD0) — Scalar deleting destructor
// ============================================================
// Ghidra decompile:
//   1. Set vtable to GameUpdateLoop vtable
//   2. Clear g_pGameWorldSingleton to NULL
//   3. Destroy System::String at +0x28 (m_saveName)
//   4. Call UpdateLoop::~UpdateLoop (base dtor in Runtime)
//   5. If flags & 1, free memory

// UpdateLoop::~UpdateLoop (Runtime 0x1800DDDB0)
typedef void (*fn_UpdateLoop_Dtor_t)(void* self);
static fn_UpdateLoop_Dtor_t fn_UpdateLoop_Dtor = nullptr;

extern "C"
void* GameUpdateLoop_Destroy(GameUpdateLoop* self, uint32_t flags) {
    EnsureInit();

    TRACE("[GameUpdateLoop::Destroy] ENTER (self=%p, flags=0x%x)\n", (void*)self, flags);

    // Resolve UpdateLoop dtor on first call
    if (!fn_UpdateLoop_Dtor) {
        fn_UpdateLoop_Dtor = reinterpret_cast<fn_UpdateLoop_Dtor_t>(
            ResolveExport("??1UpdateLoop@Engine@@UEAA@XZ"));
        if (!fn_UpdateLoop_Dtor) {
            fn_UpdateLoop_Dtor = reinterpret_cast<fn_UpdateLoop_Dtor_t>(
                ResolveAddr(Module::Runtime, 0x1800DDDB0));
        }
    }

    // Step 1: Reset vtable to GameUpdateLoop vtable
    uintptr_t vtableAddr = ResolveAddr(Module::Exe, 0x14015e2f0);
    self->vtable = reinterpret_cast<UpdateLoopVtable*>(vtableAddr);

    // Step 2: Clear game world singleton
    *g_pGameWorldSingleton = nullptr;

    // Step 3: Destroy System::String at +0x28 (m_saveName)
    fn_String_Dtor(self->m_saveName);

    // Step 4: Call UpdateLoop::~UpdateLoop (base class destructor)
    if (fn_UpdateLoop_Dtor) {
        fn_UpdateLoop_Dtor(self);
    }

    // Step 5: Conditional free
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }

    TRACE("[GameUpdateLoop::Destroy] EXIT\n");
    return self;
}

