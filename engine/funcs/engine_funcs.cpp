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

// UI infrastructure
fn_DialogController_Dtor_t fn_DialogController_Dtor = nullptr;
fn_MemFreeStatic_t fn_MemFreeStatic = nullptr;
fn_FindElement_t fn_FindElement = nullptr;
fn_TextElement_SetParameter_t fn_TextElement_SetParameter = nullptr;
fn_Element_GetUserData_t fn_Element_GetUserData = nullptr;
fn_InternalBasePtr_Ctor_t fn_InternalBasePtr_Ctor = nullptr;
fn_KeyMap_GetInputForId_t fn_KeyMap_GetInputForId = nullptr;
fn_KeyMap_GetDeviceForId_t fn_KeyMap_GetDeviceForId = nullptr;

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
