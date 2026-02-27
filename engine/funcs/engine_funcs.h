#pragma once
// Resolved engine function pointers.
// These are runtime-resolved pointers to functions in Runtime-x64.dll
// (and occasionally Application-x64.exe) that reimplemented code calls.
//
// Resolved by EngineFuncsInit() after ResolveInit() succeeds.
// Add new entries as each reimplemented function needs to call more engine code.

#include <windows.h>
#include <cstdint>

// ============================================================
// Timer globals (in Runtime-x64.dll .data section)
// Used by MainLoop for QPC-to-tick conversion.
// ============================================================

// System::Timer::_systemInitialTick — QPC value at engine init
extern int64_t* g_Timer_systemInitialTick;

// System::Timer::_ticksPerSecond — engine ticks per second (numerator)
extern int64_t* g_Timer_ticksPerSecond;

// System::Timer::_systemTicksPerSecond — QPC ticks per second (denominator)
extern int64_t* g_Timer_systemTicksPerSecond;

// System::Timer::_secondsPerTick — 1.0 / ticksPerSecond (double)
extern double* g_Timer_secondsPerTick;

// ============================================================
// Engine function pointers
// ============================================================

// Input::Device::Update(this, Time const&)
typedef void (*fn_InputDevice_Update_t)(void* self, void* time);
extern fn_InputDevice_Update_t fn_InputDevice_Update;

// Audio::Device::Update(this, Time const&)
typedef void (*fn_AudioDevice_Update_t)(void* self, void* time);
extern fn_AudioDevice_Update_t fn_AudioDevice_Update;

// Audio::Device::FreeChannelPlatform(this, int channel)
typedef void (*fn_AudioDevice_FreeChannel_t)(void* self, int channel);
extern fn_AudioDevice_FreeChannel_t fn_AudioDevice_FreeChannel;

// Application::Application::PlatformUpdate(this) -> bool
typedef bool (*fn_App_PlatformUpdate_t)(void* self);
extern fn_App_PlatformUpdate_t fn_App_PlatformUpdate;

// Video::Context::ClearResources(this)
typedef void (*fn_VideoCtx_ClearResources_t)(void* self);
extern fn_VideoCtx_ClearResources_t fn_VideoCtx_ClearResources;

// Video::Context::Reset(this)
typedef void (*fn_VideoCtx_Reset_t)(void* self);
extern fn_VideoCtx_Reset_t fn_VideoCtx_Reset;

// Engine::Loader::Load(this, UpdateLoop*)
typedef void (*fn_Loader_Load_t)(void* loader, void* updateLoop);
extern fn_Loader_Load_t fn_Loader_Load;

// Application::Application::SleepThread(this, int ms)
typedef void (*fn_App_SleepThread_t)(void* self, int ms);
extern fn_App_SleepThread_t fn_App_SleepThread;

// UI::UserInterface::Update(this, Time const&)
typedef void (*fn_UI_Update_t)(void* self, void* time);
extern fn_UI_Update_t fn_UI_Update;

// UI::UserInterface::Draw(this, Context&)
typedef void (*fn_UI_Draw_t)(void* self, void* context);
extern fn_UI_Draw_t fn_UI_Draw;

// UI::UserInterface::CloseDialog(this, InternalPtr<DialogInstance>&)
typedef void (*fn_UI_CloseDialog_t)(void* self, void* dialogPtr);
extern fn_UI_CloseDialog_t fn_UI_CloseDialog;

// Generic dynamic array push (FUN_18007a0d0) — used for dialog pending lists
typedef void (*fn_DynArrayPush_t)(void* arrayBase, int newCount, void* elementPtr);
extern fn_DynArrayPush_t fn_DynArrayPush;

// ============================================================
// System::String function pointers
// ============================================================

// String::Set(const char*) — releases old data, copies new
// Member function: RCX=this(String*), RDX=const char* src
typedef void (*fn_String_Set_t)(void* thisStr, const char* src);
extern fn_String_Set_t fn_String_Set;

// String::Format(const char*, ...) — static, creates formatted string
// MSVC ABI: hidden return pointer in RCX (String*), RDX=fmt, R8/R9/stack=varargs
// Returns: pointer to the return String (same as RCX)
typedef void* (*fn_String_Format_t)(void* retval, const char* fmt, ...);
extern fn_String_Format_t fn_String_Format;

// String::~String() — decrements refcount, frees if reaches 0
// Member function: RCX=this(String*)
typedef void (*fn_String_Dtor_t)(void* thisStr);
extern fn_String_Dtor_t fn_String_Dtor;

// ============================================================
// Engine globals (resolved pointers into Runtime-x64.dll data)
// ============================================================

// Engine::Loader global instance (_loader) at 0x1801fe7b8
extern void* g_pLoader;

// _loader.m_flags at 0x1801fe7c0 (bit 0 = thread running)
extern uint32_t* g_pLoaderFlags;

// _loader.m_completionEvent at 0x1801fe7e0 (HANDLE, manual-reset event)
extern HANDLE* g_pLoaderCompletionEvent;

// Application::Application::_singleton at 0x1801feb80
extern void** g_pAppSingleton;

// ============================================================
// Initialization
// ============================================================

// Resolve all function pointers. Call after ResolveInit().
// Returns true if all critical pointers were resolved.
bool EngineFuncsInit();
