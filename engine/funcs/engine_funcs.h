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

// PostDraw callback array push (FUN_18004f790) — 16-byte element
extern fn_DynArrayPush_t fn_PostDrawPush;

// PostUpdate callback array push (FUN_18004f8e0) — 16-byte element
extern fn_DynArrayPush_t fn_PostUpdatePush;

// ============================================================
// Video::Context lifecycle
// ============================================================

// Video::Context::Context(this, Device*, int, int, int, RenderBuffer*)
typedef void* (*fn_VideoCtx_Ctor_t)(void* self, void* device, int param1, int param2, int param3, void* renderBuf);
extern fn_VideoCtx_Ctor_t fn_VideoCtx_Ctor;

// Video::Context::~Context(this)
typedef void (*fn_VideoCtx_Dtor_t)(void* self);
extern fn_VideoCtx_Dtor_t fn_VideoCtx_Dtor;

// Video::Context::operator_delete(ptr)
typedef void (*fn_VideoCtx_Delete_t)(void* ptr);
extern fn_VideoCtx_Delete_t fn_VideoCtx_Delete;

// ============================================================
// System::MemoryAllocator
// ============================================================

// MemoryAllocator::Allocate(singleton, size, alignment) -> void*
typedef void* (*fn_MemAlloc_t)(void* singleton, uint64_t size, int alignment);
extern fn_MemAlloc_t fn_MemAlloc;

// MemoryAllocator::Free(singleton, ptr)
typedef void (*fn_MemFree_t)(void* singleton, void* ptr);
extern fn_MemFree_t fn_MemFree;

// MemoryAllocator::_singleton
extern void* g_pMemAllocSingleton;

// ============================================================
// Video::Device globals
// ============================================================

// Video::Device::_singleton
extern void** g_pVideoDeviceSingleton;

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

// String::String() — default ctor, zeroes the data pointer
// Member function: RCX=this(String*), returns this
typedef void* (*fn_String_Ctor_Default_t)(void* thisStr);
extern fn_String_Ctor_Default_t fn_String_Ctor_Default;

// String::String(const char*) — ctor from C string, calls Set internally
// Member function: RCX=this(String*), RDX=const char* src, returns this
typedef void* (*fn_String_Ctor_CStr_t)(void* thisStr, const char* src);
extern fn_String_Ctor_CStr_t fn_String_Ctor_CStr;

// String::String(String const&) — copy ctor, increments refcount
// Member function: RCX=this(String*), RDX=const String& src, returns this
typedef void* (*fn_String_Ctor_Copy_t)(void* thisStr, const void* src);
extern fn_String_Ctor_Copy_t fn_String_Ctor_Copy;

// ============================================================
// Object::ExternalFactory
// ============================================================

// ExternalFactory::_singleton — pointer to the global ExternalFactory instance
extern void** g_pExternalFactorySingleton;

// ExternalFactory::FindOrLoad(this, Filename const&) -> External const*
typedef void* (*fn_ExternalFactory_FindOrLoad_t)(void* self, const void* filename);
extern fn_ExternalFactory_FindOrLoad_t fn_ExternalFactory_FindOrLoad;

// ExternalFactory::Clean(this) — cleanup resources for freed bits
typedef void (*fn_ExternalFactory_Clean_t)(void* self);
extern fn_ExternalFactory_Clean_t fn_ExternalFactory_Clean;

// Object::Base::IsDerivedFrom(this, uint typeId) -> bool
typedef bool (*fn_IsDerivedFrom_t)(void* self, uint32_t typeId);
extern fn_IsDerivedFrom_t fn_IsDerivedFrom;

// Type ID for External resource (used by dialog Load methods)
extern uint32_t* g_pExternalTypeId;

// System::Filename::AdjustDelimiters(this)
typedef void (*fn_Filename_AdjustDelimiters_t)(void* self);
extern fn_Filename_AdjustDelimiters_t fn_Filename_AdjustDelimiters;

// UpdateLoop::LoadResources(this) — base class resource loading
typedef void (*fn_UpdateLoop_LoadResources_t)(void* self);
extern fn_UpdateLoop_LoadResources_t fn_UpdateLoop_LoadResources;

// Internal list cleanup (used by UpdateLoop dtor)
typedef void (*fn_ListCleanup_t)(void* listBase);
extern fn_ListCleanup_t fn_ListCleanup;

// ============================================================
// Mod entry lifecycle (for Modification::Swap)
// ============================================================

// Mod copy constructor (0x1800101e0) — copy-constructs a 0x80-byte Mod entry
// RCX=dest, RDX=src, returns dest
typedef void* (*fn_ModEntry_CopyCtor_t)(void* dest, const void* src);
extern fn_ModEntry_CopyCtor_t fn_ModEntry_CopyCtor;

// Mod assignment operator (0x180026ef0) — assigns one Mod entry to another
// RCX=dest, RDX=src, returns dest
typedef void* (*fn_ModEntry_Assign_t)(void* dest, const void* src);
extern fn_ModEntry_Assign_t fn_ModEntry_Assign;

// Mod destructor (0x180011460) — destroys a Mod entry (decrements string refcounts, frees arrays)
// RCX=self
typedef void (*fn_ModEntry_Dtor_t)(void* self);
extern fn_ModEntry_Dtor_t fn_ModEntry_Dtor;

// ============================================================
// Collection::Map (AVL tree) — for Configuration command-line accessors
// ============================================================

// Map::Find(this, key) -> node* (returns nullptr if not found)
// AVL tree lookup: compares String keys, returns pointer to node
// Node layout: +0x00 key(String), +0x08 value(String), +0x10 parent, +0x18 left, +0x20 right
// RCX=this(Map*), RDX=key(String const&), returns node pointer or nullptr
typedef void* (*fn_Map_Find_t)(const void* self, const void* key);
extern fn_Map_Find_t fn_Map_Find;

// Map::Insert(map, outNode, key, value) — BST insert/update
// RCX=map*, RDX=outNode*, R8=key(String*), R9=value(String*)
// Returns pointer to outNode
typedef void* (*fn_Map_Insert_t)(void* map, void* outNode, const void* key, const void* value);
extern fn_Map_Insert_t fn_Map_Insert;

// ============================================================
// Configuration helpers (for SetCommandLine reimpl)
// ============================================================

// Configuration::ParseCommand(retval, pos, cmdLine) — static, non-trivial return
// Extracts "/command" token from command line string.
// RCX=retval(String*), RDX=pos(int*), R8=cmdLine(const String*)
typedef void* (*fn_ParseCommand_t)(void* retval, int* pos, const void* cmdLine);
extern fn_ParseCommand_t fn_ParseCommand;

// Configuration::ParseValue(retval, pos, cmdLine) — static, non-trivial return
// Extracts value token after command.
// RCX=retval(String*), RDX=pos(int*), R8=cmdLine(const String*)
typedef void* (*fn_ParseValue_t)(void* retval, int* pos, const void* cmdLine);
extern fn_ParseValue_t fn_ParseValue;

// ============================================================
// IO::Persistence — key/value settings store
// ============================================================

// Persistence::_singleton pointer
extern void** g_pPersistenceSingleton;

// Persistence::Set(this, prefix, suffix, bool) — store bool setting
// Key = prefix + suffix (e.g., "ModEnabled_" + "ModName")
// RCX=this, RDX=prefix(const char*), R8=suffix(const char*), R9B=value(bool)
typedef void (*fn_Persistence_SetBool_t)(void* self, const char* prefix, const char* suffix, bool value);
extern fn_Persistence_SetBool_t fn_Persistence_SetBool;

// Persistence::Set(this, prefix, suffix, int) — store int setting
// RCX=this, RDX=prefix(const char*), R8=suffix(const char*), R9D=value(int)
typedef void (*fn_Persistence_SetInt_t)(void* self, const char* prefix, const char* suffix, int value);
extern fn_Persistence_SetInt_t fn_Persistence_SetInt;

// ============================================================
// System::Filename helpers
// ============================================================

// Filename::GetPathFilename(self, retval, stripExt) — const member, non-trivial return
// Extracts filename portion (no path, optionally no extension).
// RCX=self(const Filename*), RDX=retval(Filename*), R8B=stripExtension
typedef void* (*fn_Filename_GetPathFilename_t)(const void* self, void* retval, bool stripExt);
extern fn_Filename_GetPathFilename_t fn_Filename_GetPathFilename;

// ============================================================
// Dynamic array operations (for Mod array management)
// ============================================================

// Array<Mod>::operator= — deep copies source array to dest (0x80-byte elements)
// Uses fn_ModEntry_CopyCtor per element, handles reallocation.
// RCX=dest(ArrayHeader*), RDX=src(const ArrayHeader*)
typedef void* (*fn_ModArrayCopy_t)(void* dest, const void* src);
extern fn_ModArrayCopy_t fn_ModArrayCopy;

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
