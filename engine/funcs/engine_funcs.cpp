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
