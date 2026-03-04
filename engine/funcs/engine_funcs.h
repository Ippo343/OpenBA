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
// UI infrastructure (DialogController, Element, TextElement)
// ============================================================

// UI::DialogController::~DialogController(this) — virtual base dtor
// Resets vtable to DialogController, calls Internal::~Internal
typedef void (*fn_DialogController_Dtor_t)(void* self);
extern fn_DialogController_Dtor_t fn_DialogController_Dtor;

// UI::DialogInstance::~DialogInstance(this) — virtual base dtor
// Resets vtable to DialogInstance, cleans up dialog fields, calls Internal::~Internal
typedef void (*fn_DialogInstance_Dtor_t)(void* self);
extern fn_DialogInstance_Dtor_t fn_DialogInstance_Dtor;

// System::MemoryAllocator::MemFree(ptr) — static, gets singleton internally
// Used by scalar deleting destructors for conditional free
typedef void (*fn_MemFreeStatic_t)(void* ptr);
extern fn_MemFreeStatic_t fn_MemFreeStatic;

// UI::Element::FindElement(this, retval, stringName) — member with non-trivial return
// RCX=this(Element*), RDX=retval(InternalPtr<Element>*), R8=name(String const&)
// Recursively searches element tree by name. Returns InternalPtr to found element.
typedef void* (*fn_FindElement_t)(void* thisElement, void* retval, const void* stringName);
extern fn_FindElement_t fn_FindElement;

// UI::TextElement::SetParameter(this, index, string) — set text param by index
typedef void (*fn_TextElement_SetParameter_t)(void* self, int index, const void* str);
extern fn_TextElement_SetParameter_t fn_TextElement_SetParameter;

// UI::Element::GetUserData(this) -> Base* — returns *(Base**)(this + 0x80)
typedef void* (*fn_Element_GetUserData_t)(void* self);
extern fn_Element_GetUserData_t fn_Element_GetUserData;

// Object::InternalBasePtr::InternalBasePtr(this, Internal*) — stores ptr
typedef void* (*fn_InternalBasePtr_Ctor_t)(void* self, void* internal);
extern fn_InternalBasePtr_Ctor_t fn_InternalBasePtr_Ctor;

// Input::KeyMap::GetInputForId(this, id, slot) -> int
typedef int (*fn_KeyMap_GetInputForId_t)(void* self, int id, int slot);
extern fn_KeyMap_GetInputForId_t fn_KeyMap_GetInputForId;

// Input::KeyMap::GetDeviceForId(this, id, slot) -> int
typedef int (*fn_KeyMap_GetDeviceForId_t)(void* self, int id, int slot);
extern fn_KeyMap_GetDeviceForId_t fn_KeyMap_GetDeviceForId;

// ============================================================
// UI Widget Functions (Phase A1 — Runtime DLL exports)
// ============================================================

// --- UI::Element ---

// Element::SetFlagsOnOff(this, hideFlags, showFlags)
typedef void (*fn_Element_SetFlagsOnOff_t)(void* self, uint32_t hideFlags, uint32_t showFlags);
extern fn_Element_SetFlagsOnOff_t fn_Element_SetFlagsOnOff;

// Element::SetElementId(this, id)
typedef void (*fn_Element_SetElementId_t)(void* self, int id);
extern fn_Element_SetElementId_t fn_Element_SetElementId;

// Element::SetName(this, String const&)
typedef void (*fn_Element_SetName_t)(void* self, const void* str);
extern fn_Element_SetName_t fn_Element_SetName;

// Element::SetToolTip(this, Sprite) — by value (UI::Sprite is small)
typedef void (*fn_Element_SetToolTip_t)(void* self, void* sprite);
extern fn_Element_SetToolTip_t fn_Element_SetToolTip;

// Element::SetToolTipString(this, String const&)
typedef void (*fn_Element_SetToolTipString_t)(void* self, const void* str);
extern fn_Element_SetToolTipString_t fn_Element_SetToolTipString;

// Element::Child(this, retval, index) -> InternalPtr<Element> (member, non-trivial return)
// RCX=this, RDX=retval, R8=index
typedef void* (*fn_Element_Child_t)(void* self, void* retval, int index);
extern fn_Element_Child_t fn_Element_Child;

// Element::ChildCount(this) -> int
typedef int (*fn_Element_ChildCount_t)(const void* self);
extern fn_Element_ChildCount_t fn_Element_ChildCount;

// Element::FindElementType(this, retval, typeId) -> InternalPtr<Element> (member, non-trivial return)
// RCX=this, RDX=retval, R8=typeId
typedef void* (*fn_Element_FindElementType_t)(void* self, void* retval, uint32_t typeId);
extern fn_Element_FindElementType_t fn_Element_FindElementType;

// Element::RemoveChildren(this)
typedef void (*fn_Element_RemoveChildren_t)(void* self);
extern fn_Element_RemoveChildren_t fn_Element_RemoveChildren;

// Element::AddChild(this, InternalPtr<Element>)
typedef void (*fn_Element_AddChild_t)(void* self, void* childPtr);
extern fn_Element_AddChild_t fn_Element_AddChild;

// --- UI::DialogInstance ---

// DialogInstance::FindElement(this, retval, name) -> InternalPtr<Element> (member, non-trivial return)
// RCX=this, RDX=retval, R8=name
typedef void* (*fn_DialogInstance_FindElement_t)(void* self, void* retval, const void* name);
extern fn_DialogInstance_FindElement_t fn_DialogInstance_FindElement;

// DialogInstance::GetController(this, retval, index) -> InternalPtr<DialogController> (member, non-trivial return)
// RCX=this, RDX=retval, R8=index
typedef void* (*fn_DialogInstance_GetController_t)(const void* self, void* retval, uint32_t index);
extern fn_DialogInstance_GetController_t fn_DialogInstance_GetController;

// DialogInstance::FindElementType(this, retval, typeId) -> InternalPtr<Element> (member, non-trivial return)
// RCX=this, RDX=retval, R8=typeId
typedef void* (*fn_DialogInstance_FindElementType_t)(void* self, void* retval, uint32_t typeId);
extern fn_DialogInstance_FindElementType_t fn_DialogInstance_FindElementType;

// DialogInstance::SetFlagsOnOff(this, hideFlags, showFlags)
typedef void (*fn_DialogInstance_SetFlagsOnOff_t)(void* self, uint32_t hideFlags, uint32_t showFlags);
extern fn_DialogInstance_SetFlagsOnOff_t fn_DialogInstance_SetFlagsOnOff;

// --- UI::ComboElement ---

// ComboElement::AddItem(this, Sprite const&, String const&, userData) -> int
typedef int (*fn_ComboElement_AddItem_t)(void* self, const void* sprite, const void* str, void* userData);
extern fn_ComboElement_AddItem_t fn_ComboElement_AddItem;

// ComboElement::SetCurrentItem(this, index)
typedef void (*fn_ComboElement_SetCurrentItem_t)(void* self, int index);
extern fn_ComboElement_SetCurrentItem_t fn_ComboElement_SetCurrentItem;

// ComboElement::GetItemCount(this) -> int
typedef int (*fn_ComboElement_GetItemCount_t)(const void* self);
extern fn_ComboElement_GetItemCount_t fn_ComboElement_GetItemCount;

// ComboElement::GetCurrentItem(this) -> int
typedef int (*fn_ComboElement_GetCurrentItem_t)(const void* self);
extern fn_ComboElement_GetCurrentItem_t fn_ComboElement_GetCurrentItem;

// ComboElement::GetItemData(this, index) -> void*
typedef void* (*fn_ComboElement_GetItemData_t)(const void* self, int index);
extern fn_ComboElement_GetItemData_t fn_ComboElement_GetItemData;

// ComboElement::RemoveAllItems(this)
typedef void (*fn_ComboElement_RemoveAllItems_t)(void* self);
extern fn_ComboElement_RemoveAllItems_t fn_ComboElement_RemoveAllItems;

// --- UI::ProgressElement ---

// ProgressElement::SetProgressTextParameter(this, index, String const&)
typedef void (*fn_ProgressElement_SetProgressTextParameter_t)(void* self, int index, const void* str);
extern fn_ProgressElement_SetProgressTextParameter_t fn_ProgressElement_SetProgressTextParameter;

// ProgressElement::SetMaximumValue(this, float)
typedef void (*fn_ProgressElement_SetMaximumValue_t)(void* self, float value);
extern fn_ProgressElement_SetMaximumValue_t fn_ProgressElement_SetMaximumValue;

// ProgressElement::SetCurrentValue(this, float)
typedef void (*fn_ProgressElement_SetCurrentValue_t)(void* self, float value);
extern fn_ProgressElement_SetCurrentValue_t fn_ProgressElement_SetCurrentValue;

// --- UI::TableElement ---

// TableElement::SetSize(this, rows, cols)
typedef void (*fn_TableElement_SetSize_t)(void* self, int rows, int cols);
extern fn_TableElement_SetSize_t fn_TableElement_SetSize;

// TableElement::SetChild(this, elem, col, row)
typedef void (*fn_TableElement_SetChild3_t)(void* self, void* elemPtr, int col, int row);
extern fn_TableElement_SetChild3_t fn_TableElement_SetChild3;

// TableElement::SetChild(this, elem, col) — single-index overload
typedef void (*fn_TableElement_SetChild2_t)(void* self, void* elemPtr, int col);
extern fn_TableElement_SetChild2_t fn_TableElement_SetChild2;

// TableElement::GetRowCount(this) -> int
typedef int (*fn_TableElement_GetRowCount_t)(const void* self);
extern fn_TableElement_GetRowCount_t fn_TableElement_GetRowCount;

// TableElement::GetColumnCount(this) -> int
typedef int (*fn_TableElement_GetColumnCount_t)(const void* self);
extern fn_TableElement_GetColumnCount_t fn_TableElement_GetColumnCount;

// --- UI::NumberEditElement ---

// NumberEditElement::SetValue(this, int)
typedef void (*fn_NumberEditElement_SetValue_t)(void* self, int value);
extern fn_NumberEditElement_SetValue_t fn_NumberEditElement_SetValue;

// NumberEditElement::SetMinMax(this, min, max)
typedef void (*fn_NumberEditElement_SetMinMax_t)(void* self, int minVal, int maxVal);
extern fn_NumberEditElement_SetMinMax_t fn_NumberEditElement_SetMinMax;

// NumberEditElement::GetValue(this) -> int
typedef int (*fn_NumberEditElement_GetValue_t)(const void* self);
extern fn_NumberEditElement_GetValue_t fn_NumberEditElement_GetValue;

// --- UI::NumberElement ---

// NumberElement::SetValue(this, int)
typedef void (*fn_NumberElement_SetValueInt_t)(void* self, int value);
extern fn_NumberElement_SetValueInt_t fn_NumberElement_SetValueInt;

// NumberElement::SetValue(this, float)
typedef void (*fn_NumberElement_SetValueFloat_t)(void* self, float value);
extern fn_NumberElement_SetValueFloat_t fn_NumberElement_SetValueFloat;

// --- UI::SpinnerElement ---

// SpinnerElement::SetElementId(this, int)
typedef void (*fn_SpinnerElement_SetElementId_t)(void* self, int id);
extern fn_SpinnerElement_SetElementId_t fn_SpinnerElement_SetElementId;

// SpinnerElement::SetValue(this, int)
typedef void (*fn_SpinnerElement_SetValue_t)(void* self, int value);
extern fn_SpinnerElement_SetValue_t fn_SpinnerElement_SetValue;

// SpinnerElement::SetMinMax(this, min, max)
typedef void (*fn_SpinnerElement_SetMinMax_t)(void* self, int minVal, int maxVal);
extern fn_SpinnerElement_SetMinMax_t fn_SpinnerElement_SetMinMax;

// SpinnerElement::GetValue(this) -> int
typedef int (*fn_SpinnerElement_GetValue_t)(const void* self);
extern fn_SpinnerElement_GetValue_t fn_SpinnerElement_GetValue;

// --- UI::ImageElement ---

// ImageElement::SetSprite(this, Sprite) — by value
typedef void (*fn_ImageElement_SetSprite_t)(void* self, void* sprite);
extern fn_ImageElement_SetSprite_t fn_ImageElement_SetSprite;

// ImageElement::SetWidthHeight(this, w, h)
typedef void (*fn_ImageElement_SetWidthHeight_t)(void* self, int w, int h);
extern fn_ImageElement_SetWidthHeight_t fn_ImageElement_SetWidthHeight;

// --- UI::LabelElement ---

// LabelElement::SetSprite(this, Sprite) — by value
typedef void (*fn_LabelElement_SetSprite_t)(void* self, void* sprite);
extern fn_LabelElement_SetSprite_t fn_LabelElement_SetSprite;

// --- UI::TextElement ---

// TextElement::SetText(this, String) — by value (UI::String wrapper)
typedef void (*fn_TextElement_SetText_t)(void* self, void* str);
extern fn_TextElement_SetText_t fn_TextElement_SetText;

// ============================================================
// Exe Type IDs (Phase A2 — .rdata pointers in Application-x64.exe)
// ============================================================

extern uint32_t* g_TypeId_ComponentEntity;       // DAT_1401c5d70
extern uint32_t* g_TypeId_ProgressElement;       // exref at 0x1401515b8 (double-indirect)
extern uint32_t* g_TypeId_BuildComponent;         // DAT_1401c7318
extern uint32_t* g_TypeId_ConsumeProduceComponent;// DAT_1401c7388
extern uint32_t* g_TypeId_RadiusComponent;        // DAT_1401c4870
extern uint32_t* g_TypeId_BuildRequirementComponent;// DAT_1401c7f58
extern uint32_t* g_TypeId_ResourceComponent;      // DAT_1401c7040
extern uint32_t* g_TypeId_ResourceType;           // DAT_1401c7cb8
extern uint32_t* g_TypeId_DialogController;       // DAT_1401c56e0
extern uint32_t* g_TypeId_PageToggleUI;           // DAT_1401c4f00
extern uint32_t* g_TypeId_MeshInstanceComponent;  // DAT_1401c7e40
extern uint32_t* g_TypeId_DestroyComponent;       // DAT_1401c7628
extern uint32_t* g_TypeId_WorkplaceComponent;     // DAT_1401c80e0
extern uint32_t* g_TypeId_StorageLocationComponent;// DAT_1401c8070
extern uint32_t* g_TypeId_StorageComponent;       // (TBD)
extern uint32_t* g_TypeId_StateMachineComponent;  // (TBD)
extern uint32_t* g_TypeId_WorkplaceJobType;       // DAT_1401c80a8
extern uint32_t* g_TypeId_AttendanceSubComp;      // DAT_1401c3f78
extern uint32_t* g_TypeId_ResourceLimitType;      // DAT_1401c5520
extern uint32_t* g_TypeId_ToolUI;                 // DAT_1401c5788

// ============================================================
// Game Singletons (Phase A3 — exe global pointers)
// ============================================================

// Game/World singleton — accessed by ~20 UI methods
// Sub-fields: +0xB0 ProfessionList, +0xF0 CitizenList, +0xF8 WorldGrid,
//             +0x100 ResourceTypeMap, +0x110 RadiusDisplayManager
extern void** g_pGameWorldSingleton;  // DAT_1401c3dd8

// Input::KeyMap singleton — used by HotKeyToolTipUI
extern void** g_pKeyMapSingleton;     // DAT_1401c3de8

// ============================================================
// Common Exe Functions (Phase A4 — by RVA from Application-x64.exe)
// ============================================================

// Entity::FindComponent variants (template instantiations)
// All have signature: void FindComponent(entity*, retval*)
// RCX=entity, RDX=retval(InternalPtr*)
typedef void (*fn_FindComponent_t)(void* entity, void* retval);

extern fn_FindComponent_t fn_FindComponent_StateMachine;   // 0x14000b6f0
extern fn_FindComponent_t fn_FindComponent_MeshInstance;   // 0x140002f90
extern fn_FindComponent_t fn_FindComponent_Destroy;        // FUN_140005780
extern fn_FindComponent_t fn_FindComponent_Build;          // FUN_140004680
extern fn_FindComponent_t fn_FindComponent_ConsumeProduce; // TBD
extern fn_FindComponent_t fn_FindComponent_Resource;       // TBD
extern fn_FindComponent_t fn_FindComponent_Storage;        // FUN_140006270
extern fn_FindComponent_t fn_FindComponent_Workplace;      // FUN_140003050
extern fn_FindComponent_t fn_FindComponent_Radius;         // TBD
extern fn_FindComponent_t fn_FindComponent_BuildState;     // FUN_140003ea0 (type DAT_1401c7d98)
extern fn_FindComponent_t fn_FindComponent_Attendance;     // FUN_1400105e0
extern fn_FindComponent_t fn_FindComponent_Cemetery;       // FUN_140010a00

// Array resize/push helpers (exe-internal template instantiations)
typedef void (*fn_ExeArrayResize_t)(void* arrayBase, int newCount);
extern fn_ExeArrayResize_t fn_ExeArrayResize;   // FUN_1400032e0
extern fn_ExeArrayResize_t fn_ExeArrayResize2;  // FUN_1400021d0

typedef void (*fn_ExeArrayPush_t)(void* arrayBase, int newCount, void* element);
extern fn_ExeArrayPush_t fn_ExeArrayPush;       // FUN_140003600

// StorageComponent::QueryCount(storageComp) -> int
typedef int (*fn_StorageComponent_QueryCount_t)(void* storageComp);
extern fn_StorageComponent_QueryCount_t fn_StorageComponent_QueryCount;  // 0x14012cf10

// Radius calculation helper (returns radius display index)
typedef int (*fn_RadiusCalc_t)(void* component, int tileIndex);
extern fn_RadiusCalc_t fn_RadiusCalc;           // FUN_140055430

// RadiusDisplayManager::Remove(radiusMgr, index)
typedef void (*fn_RadiusDisplay_Remove_t)(void* radiusMgr, int index);
extern fn_RadiusDisplay_Remove_t fn_RadiusDisplay_Remove;  // FUN_1400b0d00

// DestroyComponent query functions
typedef bool (*fn_DestroyQuery_t)(void* component);
extern fn_DestroyQuery_t fn_DestroyComponent_CanReclaim;   // FUN_14011e690
extern fn_DestroyQuery_t fn_DestroyComponent_CanRebuild;   // FUN_14011e600

// DestroyComponent action functions
typedef void (*fn_DestroyAction_t)(void* component);
extern fn_DestroyAction_t fn_DestroyComponent_Reclaim;     // FUN_14011e400
extern fn_DestroyAction_t fn_DestroyComponent_Rebuild;     // FUN_14011e470
extern fn_DestroyAction_t fn_DestroyComponent_Destroy;     // FUN_14011e4d0

// Get active product index from ConsumeProduce component
typedef int (*fn_GetActiveProductIndex_t)(void* component);
extern fn_GetActiveProductIndex_t fn_GetActiveProductIndex; // FUN_140119470

// Storage count query variant
typedef int (*fn_StorageCountQuery_t)(void* component);
extern fn_StorageCountQuery_t fn_StorageCountQuery;         // FUN_14012e5c0

// CropUI helper functions
typedef void (*fn_CropUIPopulate_t)(void* cropUI);
extern fn_CropUIPopulate_t fn_CropUI_PopulateCombo;        // FUN_140078830
typedef int (*fn_CropUIExpected_t)(void* cropUI);
extern fn_CropUIExpected_t fn_CropUI_GetExpectedCount;     // FUN_140078a40

// Cemetery burial count helper (iterates slots, counts occupied)
typedef int (*fn_CemeteryBurialCount_t)(void* cemeteryComp);
extern fn_CemeteryBurialCount_t fn_CemeteryGetBurialCount; // FUN_140046dd0

// SpinnerElement::IsActive(this) -> bool
typedef bool (*fn_SpinnerElement_IsActive_t)(const void* self);
extern fn_SpinnerElement_IsActive_t fn_SpinnerElement_IsActive;

// --- UI::PageElement ---

// PageElement::GetActivePage(this) -> int
typedef int (*fn_PageElement_GetActivePage_t)(const void* self);
extern fn_PageElement_GetActivePage_t fn_PageElement_GetActivePage;

// PageElement::SetActivePage(this, page)
typedef void (*fn_PageElement_SetActivePage_t)(void* self, int page);
extern fn_PageElement_SetActivePage_t fn_PageElement_SetActivePage;

// --- UI::Element type IDs (exref, double-indirect) ---

// CheckboxElement type ID at exref 0x1401516a8
extern uint32_t* g_TypeId_CheckboxElement;

// UI element base type ID at exref 0x1401514f8 (used by ControlUI)
extern uint32_t* g_TypeId_UIElementBase;

// ============================================================
// Additional Exe Type IDs (Phase D additions)
// ============================================================

// ControlComponent type ID at DAT_1401c4e20 (direct)
extern uint32_t* g_TypeId_ControlComponent;

// ============================================================
// Additional Exe Functions (Phase D additions)
// ============================================================

// FindComponentByTypeId(entity, retval, typeId) — searches entity component array
// FUN_1400c3680: iterates entity+0x30 array (count at +0x38), matches vtable[2] to typeId
typedef void* (*fn_FindComponentByTypeId_t)(void* entity, void* retval, int typeId);
extern fn_FindComponentByTypeId_t fn_FindComponentByTypeId;

// EntityTrackingSet_Remove(bstRoot, key) — removes entity from BST tracking set
// FUN_14002c690: BST remove by InternalBasePtr key
typedef void (*fn_EntityTrackingSet_Remove_t)(void* bstRoot, void* key);
extern fn_EntityTrackingSet_Remove_t fn_EntityTrackingSet_Remove;

// FindComponent_Immigration — finds ImmigrationComponent on entity (DAT_1401c4480)
extern fn_FindComponent_t fn_FindComponent_Immigration;  // FUN_140012020

// WorkplaceComponent_Toggle(comp, newState) — toggles workplace enabled/disabled
// FUN_14012fc00: 166-line function, called as opaque function pointer
typedef void (*fn_WorkplaceComponent_Toggle_t)(void* comp, bool newState);
extern fn_WorkplaceComponent_Toggle_t fn_WorkplaceComponent_Toggle;

// FindComponent_Orchard — finds OrchardComponent on entity (DAT_1401c46b0)
// Searches entity component array at +0x30/+0x38 for OrchardComponent type ID.
extern fn_FindComponent_t fn_FindComponent_Orchard;  // FUN_140005440

// OrchardComponent_GetGrowth(comp) — returns average growth ratio as float [0.0, 1.0]
// FUN_140051640: iterates orchard slots, averages growth values
typedef float (*fn_OrchardGetGrowth_t)(void* comp);
extern fn_OrchardGetGrowth_t fn_OrchardGetGrowth;

// FindComponent_Pasture — finds PastureComponent on entity (DAT_1401c7ba0)
// Searches entity component array at +0x30/+0x38 for PastureComponent type ID.
extern fn_FindComponent_t fn_FindComponent_Pasture;  // FUN_140004fe0

// PastureComponent_CalcMaxCount(comp) — calculates max animal count from pasture dimensions
// FUN_140128570: reads build size, divides area by per-animal tile requirement
typedef uint32_t (*fn_PastureCalcMaxCount_t)(void* comp);
extern fn_PastureCalcMaxCount_t fn_PastureCalcMaxCount;

// PastureUI_UpdateSlider(self) — updates slider min/max/value from component state
// FUN_1400b4fa0: finds PastureComponent, sets slider element fields
typedef void (*fn_PastureUI_UpdateSlider_t)(void* self);
extern fn_PastureUI_UpdateSlider_t fn_PastureUI_UpdateSlider;

// ============================================================
// Phase F: Dialog/UI Runtime functions
// ============================================================

// ImageElement::SetRotation(this, angle) — sets rotation in degrees
// ?SetRotation@ImageElement@UI@@QEAAXM@Z (0x1801182c0, ordinal 3952)
typedef void (*fn_ImageElement_SetRotation_t)(void* self, float angle);
extern fn_ImageElement_SetRotation_t fn_ImageElement_SetRotation;

// Time::AsSeconds(this) -> float — converts tick count to seconds
// ?AsSeconds@Time@System@@QEBAMXZ (0x180007900, ordinal 1620)
typedef float (*fn_Time_AsSeconds_t)(void* time);
extern fn_Time_AsSeconds_t fn_Time_AsSeconds;

// SaveLoad::GetOperation(this) -> int (enum Operation)
// ?GetOperation@SaveLoad@Save@@QEBA?AW4Operation@12@XZ (0x1800c3c80, ordinal 2826)
typedef int (*fn_SaveLoad_GetOperation_t)(void* saveLoad);
extern fn_SaveLoad_GetOperation_t fn_SaveLoad_GetOperation;

// SaveLoad singleton — accessed via exe exref at 0x140151610 (double-indirect)
extern void** g_pSaveLoadSingleton;

// EditElement::SetText(this, String*) — sets edit field text
// ?SetText@EditElement@UI@@QEAAXVString@2@@Z (0x180042070)
typedef void (*fn_EditElement_SetText_t)(void* self, void* str);
extern fn_EditElement_SetText_t fn_EditElement_SetText;

// ImageElement::GetSprite(this, retval) — member, non-trivial return ABI
// RCX=this, RDX=retval(Sprite*). Copies 16 bytes from +0xC0/+0xC8.
// ?GetSprite@ImageElement@UI@@QEBA?AVSprite@2@XZ (0x1801182a0, ordinal 2926)
typedef void (*fn_ImageElement_GetSprite_t)(void* self, void* retval);
extern fn_ImageElement_GetSprite_t fn_ImageElement_GetSprite;

// SpriteSheet::GetSpriteIndexFromName(this, name) -> int
// ?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z (0x1800031f0, ordinal 2930)
typedef int (*fn_SpriteSheet_GetSpriteIndexFromName_t)(void* sheet, void* name);
extern fn_SpriteSheet_GetSpriteIndexFromName_t fn_SpriteSheet_GetSpriteIndexFromName;

// SliderElement::SetParameter(this, index, String const&)
// ?SetParameter@SliderElement@UI@@QEAAXHAEBVString@System@@@Z
typedef void (*fn_SliderElement_SetParameter_t)(void* self, int index, const void* str);
extern fn_SliderElement_SetParameter_t fn_SliderElement_SetParameter;

// Audio::Scene::Pause(this, channel, pause) — unpauses audio scene channels
// Resolved from exe dispatch table at 0x140152030
typedef void (*fn_Audio_Scene_Pause_t)(void* scene, int channel, bool pause);
extern fn_Audio_Scene_Pause_t fn_Audio_Scene_Pause;

// ScenarioDialog helper: loads the selected scenario (FUN_140077cb0)
typedef void (*fn_ScenarioDialog_LoadSelected_t)(void* self);
extern fn_ScenarioDialog_LoadSelected_t fn_ScenarioDialog_LoadSelected;

// ModDetailsDialog helpers
typedef void (*fn_ModDetailsDialog_OpenUpdateDialog_t)(void* self);
extern fn_ModDetailsDialog_OpenUpdateDialog_t fn_ModDetailsDialog_OpenUpdateDialog;

typedef void (*fn_ModDetailsDialog_DeleteMod_t)(void* self);
extern fn_ModDetailsDialog_DeleteMod_t fn_ModDetailsDialog_DeleteMod;

// SelectResourceUI::GetResourceCount(self) -> int
// FUN_140078a40 — counts expected resources for combo (shared by Orchard/Pasture)
typedef int (*fn_SelectResourceUI_GetResourceCount_t)(void* self);
extern fn_SelectResourceUI_GetResourceCount_t fn_SelectResourceUI_GetResourceCount;

// SelectResourceUI::PopulateCombo(self)
// FUN_140078830 — repopulates combo element with current resources
typedef void (*fn_SelectResourceUI_PopulateCombo_t)(void* self);
extern fn_SelectResourceUI_PopulateCombo_t fn_SelectResourceUI_PopulateCombo;

// ============================================================
// Video::ImageBuffer (Runtime DLL exports, used by MapUI/NoiseUI/SpriteCaptureUI)
// ============================================================

// ImageBuffer::ImageBuffer() — default ctor
// ??0ImageBuffer@Video@@QEAA@XZ (0x1800f2580, ordinal 227)
typedef void* (*fn_ImageBuffer_Ctor_t)(void* self);
extern fn_ImageBuffer_Ctor_t fn_ImageBuffer_Ctor;

// ImageBuffer::~ImageBuffer() — virtual dtor
// ??1ImageBuffer@Video@@UEAA@XZ (0x1800f2610, ordinal 609)
typedef void (*fn_ImageBuffer_Dtor_t)(void* self);
extern fn_ImageBuffer_Dtor_t fn_ImageBuffer_Dtor;

// ImageBuffer::CopyToTexture(this, data, width, height) -> bool
// ?CopyToTexture@ImageBuffer@Video@@QEAA_NPEAXHH@Z (0x1800f3380, ordinal 1869)
typedef bool (*fn_ImageBuffer_CopyToTexture_t)(void* self, void* data, int width, int height);
extern fn_ImageBuffer_CopyToTexture_t fn_ImageBuffer_CopyToTexture;

// ImageBuffer::CopyFromTexture(this, data, width, height) -> bool
// ?CopyFromTexture@ImageBuffer@Video@@QEAA_NPEAXHH@Z (0x1800f33a0, ordinal 1866)
typedef bool (*fn_ImageBuffer_CopyFromTexture_t)(void* self, void* data, int width, int height);
extern fn_ImageBuffer_CopyFromTexture_t fn_ImageBuffer_CopyFromTexture;

// ============================================================
// Achievement::GoalList / GoalInstance (Runtime DLL exports)
// ============================================================

// GoalList::GetGoal(this, retval, index) — member, non-trivial return ABI
// RCX=GoalList, RDX=hidden_retval (InternalPtr<GoalInstance>), R8=index
// ?GetGoal@GoalList@Achievement@@QEBA?AV?$InternalPtr@VGoalInstance@Achievement@@@Object@@H@Z
// (0x180001710, ordinal 2683)
typedef void (*fn_GoalList_GetGoal_t)(void* self, void* retval, int index);
extern fn_GoalList_GetGoal_t fn_GoalList_GetGoal;

// GoalInstance::IsMet(this) -> bool — checks if goal is met (bit 0 of +0x10)
// ?IsMet@GoalInstance@Achievement@@QEBA_NXZ (0x18009eed0, ordinal 3141)
typedef bool (*fn_GoalInstance_IsMet_t)(void* self);
extern fn_GoalInstance_IsMet_t fn_GoalInstance_IsMet;

// GoalInstance::HasFailed(this) -> bool — checks if goal has failed (bit 1 of +0x10)
// ?HasFailed@GoalInstance@Achievement@@QEBA_NXZ (0x18009eec0, ordinal 3046)
typedef bool (*fn_GoalInstance_HasFailed_t)(void* self);
extern fn_GoalInstance_HasFailed_t fn_GoalInstance_HasFailed;

// AchievementManager singleton — double-indirect via .rdata dispatch at 0x1401514f0
// Usage: void* manager = **g_ppAchievementManager;
extern void*** g_ppAchievementManager;

// AchievementManager::IsAchievementAwarded(this, index) -> bool
// ?IsAchievementAwarded@AchievementManager@Achievement@@QEBA_NH@Z (0x1800079c0, ordinal 3091)
typedef bool (*fn_AchievementManager_IsAchievementAwarded_t)(void* self, int index);
extern fn_AchievementManager_IsAchievementAwarded_t fn_AchievementManager_IsAchievementAwarded;

// ============================================================
// FollowDialog helpers
// ============================================================

// FindComponent_Character — FUN_140003f60 (finds Character/CitizenAI component)
// Same signature as other FindComponent variants: entity*, retval*
extern fn_FindComponent_t fn_FindComponent_Character;  // FUN_140003f60

// Input::KeyMap::Pressed(this, slot, keyId) -> bool
// Resolved from exe dispatch table at 0x140151758
typedef bool (*fn_KeyMap_Pressed_t)(void* self, int slot, int keyId);
extern fn_KeyMap_Pressed_t fn_KeyMap_Pressed;

// FollowDialog close helper — clears follow target, closes dialog
// FUN_140068f00: (self)
typedef void (*fn_FollowDialog_Close_t)(void* self);
extern fn_FollowDialog_Close_t fn_FollowDialog_Close;

// ============================================================
// UpgradeUI helpers
// ============================================================

// UpgradeDescription type ID at DAT_1401c5868
extern uint32_t* g_TypeId_UpgradeDescription;

// DestroyComponent_StartUpgrade — drives state machine through upgrade transition
// FUN_14011dfa0: complex 120+ line function, resolved as opaque function pointer
typedef void (*fn_DestroyComponent_StartUpgrade_t)(void* destroyComp);
extern fn_DestroyComponent_StartUpgrade_t fn_DestroyComponent_StartUpgrade;

// ============================================================
// ResidenceUI helpers
// ============================================================

// ResidenceUI_GoToCitizen(self, citizenIndex) — navigates camera to resident
// FUN_1400b56a0: reads resident data from dynamic array, focuses camera
typedef void (*fn_ResidenceUI_GoToCitizen_t)(void* self, int citizenIndex);
extern fn_ResidenceUI_GoToCitizen_t fn_ResidenceUI_GoToCitizen;

// ResidenceUI array push (FUN_140021020) — resizes and copies 0x30-byte entries
// Used by ResidenceUI::Slot4 to build the resident row element array.
// Signature: (arrayBase*, newCount, elementPtr*)
typedef void (*fn_ResidenceUI_ArrayPush_t)(void* arrayBase, int newCount, void* element);
extern fn_ResidenceUI_ArrayPush_t fn_ResidenceUI_ArrayPush;

// ============================================================
// WorkplaceUI helpers
// ============================================================

// FindComponent for BuildState (type DAT_1401c8118)
// FUN_140002e10: entity, retval*
extern fn_FindComponent_t fn_FindComponent_BuildState2;  // FUN_140002e10

// WorkplaceUI_AdjustWorkerCount(self, delta) — adjusts assigned worker count by +/-delta
// FUN_140087610: called for jobCountUp (+1) and jobCountDown (-1) buttons
typedef void (*fn_WorkplaceUI_AdjustWorkerCount_t)(void* self, int delta);
extern fn_WorkplaceUI_AdjustWorkerCount_t fn_WorkplaceUI_AdjustWorkerCount;

// WorkplaceUI_ToggleWorkplace(self) — toggles workplace enabled/disabled state
// FUN_1400876d0: called when workPlaceButton is clicked (ID 0x1454)
typedef void (*fn_WorkplaceUI_ToggleWorkplace_t)(void* self);
extern fn_WorkplaceUI_ToggleWorkplace_t fn_WorkplaceUI_ToggleWorkplace;

// CitizenList::RequestWorker(citizenList, professionId) — adds worker request
// FUN_14008d100: called for workerCountUp (ID 0x1450)
typedef void (*fn_CitizenList_RequestWorker_t)(void* citizenList, void* retval, uint64_t professionId);
extern fn_CitizenList_RequestWorker_t fn_CitizenList_RequestWorker;

// CitizenList::AssignWorker(citizenList, retval, workerSlot, professionId) — assign worker
// FUN_14008c3d0: called after requesting worker
typedef void (*fn_CitizenList_AssignWorker_t)(void* citizenList, void* retval, void* workerSlot, uint64_t professionId);
extern fn_CitizenList_AssignWorker_t fn_CitizenList_AssignWorker;

// CitizenList::RemoveWorker(citizenList, professionId) — removes worker assignment
// FUN_140091180: called for workerCountDown (ID 0x1451)
typedef void (*fn_CitizenList_RemoveWorker_t)(void* citizenList, uint64_t professionId);
extern fn_CitizenList_RemoveWorker_t fn_CitizenList_RemoveWorker;

// FindComponent_Position — finds PositionComponent on entity
// Searches entity component array for PositionComponent type ID.
extern fn_FindComponent_t fn_FindComponent_Position;  // FUN_14000d6c0

// EditTool type ID at g_TypeId_EditTool
extern uint32_t* g_TypeId_EditTool;  // resolved from _typeInfo_exref for EditTool checks

// ============================================================
// EntityUI helpers
// ============================================================

// EntityUI_SetEditMode(playerController, mode) — sets edit tool selection mode
// FUN_140102b60: sets player controller edit state
typedef void (*fn_EntityUI_SetEditMode_t)(void* playerController, int mode);
extern fn_EntityUI_SetEditMode_t fn_EntityUI_SetEditMode;

// EntityUI_SelectEntity(playerController, entityPtr, x, y) — select entity for editing
// FUN_140102e30: passes entity to edit tool's select
typedef void (*fn_EntityUI_SelectEntity_t)(void* playerController, void* entityPtr, int x, int y);
extern fn_EntityUI_SelectEntity_t fn_EntityUI_SelectEntity;

// EditTool_GetSelected(editTool, retval) — gets currently selected entity
// vtable[13] (offset 0x68): virtual call on EditTool, returns InternalPtr<ComponentEntity>
// Used by EntityUI::Slot7 to check if current entity is selected

// FindComponent_SM — finds StateMachine/animation component on entity
// FUN_14000a170: entity, retval*
extern fn_FindComponent_t fn_FindComponent_SM;  // FUN_14000a170

// GetDescPage(entityDesc, retval) — gets default page from entity description
// FUN_140011320: reads description fields, returns page-related struct ptr
typedef void (*fn_GetDescPage_t)(void* entityDesc, void* retval);
extern fn_GetDescPage_t fn_GetDescPage;

// BuildComplete(posComp) -> bool — checks if build is complete
// FUN_140109390: reads build progress from position component
typedef bool (*fn_BuildComplete_t)(void* posComp);
extern fn_BuildComplete_t fn_BuildComplete;

// ============================================================
// EventLogUI helpers
// ============================================================

// EventLogUI_ToggleNotification(self, buttonIndex) — toggles event notification flag
// FUN_140067320: reads flag from button element, copies to event log global
typedef void (*fn_EventLogUI_ToggleNotification_t)(void* self, int buttonIndex);
extern fn_EventLogUI_ToggleNotification_t fn_EventLogUI_ToggleNotification;

// EventLogUI_NavigateToEvent(self, rowIndex) — navigates camera to event location
// FUN_140067370: computes map coords from event, sets camera target
typedef void (*fn_EventLogUI_NavigateToEvent_t)(void* self, int rowIndex);
extern fn_EventLogUI_NavigateToEvent_t fn_EventLogUI_NavigateToEvent;

// ============================================================
// Phase F Wave 1: MapParamsDialog / UpgradeUI / ConsumeProduceUI / HelpUI
// ============================================================

// MapParamsDialog::OnOk — copies editName text to game world, closes dialog
// FUN_14006b690
typedef void (*fn_MapParamsDialog_OnOk_t)(void* self);
extern fn_MapParamsDialog_OnOk_t fn_MapParamsDialog_OnOk;

// MapParamsDialog::OnNewName — generates random name, sets editName text
// FUN_14006b700
typedef void (*fn_MapParamsDialog_OnNewName_t)(void* self);
extern fn_MapParamsDialog_OnNewName_t fn_MapParamsDialog_OnNewName;

// GameWorld_GetMapName — copies gameWorld+0x28 into String
// FUN_140011f90: RCX=gameWorld, RDX=retval(String*), returns retval
typedef void* (*fn_GameWorld_GetMapName_t)(void* gameWorld, void* retval);
extern fn_GameWorld_GetMapName_t fn_GameWorld_GetMapName;

// Element::SetUserData(this, Base*) — stores user data pointer
typedef void (*fn_Element_SetUserData_t)(void* self, void* data);
extern fn_Element_SetUserData_t fn_Element_SetUserData;

// UpgradeUI::Serialize — complex setup (opaque passthrough via exe address)
// 0x140086d90
typedef void (*fn_UpgradeUI_Serialize_t)(void* self);
extern fn_UpgradeUI_Serialize_t fn_UpgradeUI_Serialize;

// UpgradeUI::OnRemove — button click starts upgrade
// 0x140086c90
typedef void (*fn_UpgradeUI_OnRemove_t)(void* self, void* p2, int eventType, int eventId);
extern fn_UpgradeUI_OnRemove_t fn_UpgradeUI_OnRemove;

// ConsumeProduceUI::Slot4 — combo setup with resource sprites
// 0x14005F3D0
typedef void (*fn_ConsumeProduceUI_Slot4_t)(void* self);
extern fn_ConsumeProduceUI_Slot4_t fn_ConsumeProduceUI_Slot4;

// HelpUI::Slot4 — page navigation setup
// 0x14006A500
typedef void (*fn_HelpUI_Slot4_t)(void* self);
extern fn_HelpUI_Slot4_t fn_HelpUI_Slot4;

// HelpUI::Slot9 — button/combo event handler
// 0x14006A290
typedef void (*fn_HelpUI_Slot9_t)(void* self, uint64_t param2, int param3, int param4);
extern fn_HelpUI_Slot9_t fn_HelpUI_Slot9;

// ============================================================
// Phase F Wave 2A: GoalDialog / EventNotifyDialog / ScenarioDialog
// ============================================================

// GoalList::GetGoalCount(this) -> int — returns count at this+8
// ?GetGoalCount@GoalList@Achievement@@QEBAHXZ (0x180001700, ordinal 2684)
typedef int (*fn_GoalList_GetGoalCount_t)(void* self);
extern fn_GoalList_GetGoalCount_t fn_GoalList_GetGoalCount;

// UI::Dialog::GetTemplate(this, retval, index) -> ExternalPtr<ElementDescription>
// Member, non-trivial return: RCX=this, RDX=retval, R8=index
// ?GetTemplate@Dialog@UI@@QEBA?AV?$ExternalPtr@VElementDescription@UI@@@Object@@H@Z
// (0x1800388b0, ordinal 2956)
typedef void (*fn_Dialog_GetTemplate_t)(void* self, void* retval, int index);
extern fn_Dialog_GetTemplate_t fn_Dialog_GetTemplate;

// Goal::GetText(this, retval) -> String — non-trivial return
// RCX=this, RDX=retval(String*). Copies string at this+0x28 with refcount.
// ?GetText@Goal@Achievement@@QEBA?AVString@System@@XZ (0x180001ba0, ordinal 2958)
typedef void (*fn_Goal_GetText_t)(void* self, void* retval);
extern fn_Goal_GetText_t fn_Goal_GetText;

// Achievement::GetTitle(this, retval) -> String — non-trivial return
// RCX=this, RDX=retval(String*). Copies string at this+0x28 with refcount.
// ?GetTitle@Achievement@1@QEBA?AVString@System@@XZ (0x1800013e0, ordinal 2973)
typedef void (*fn_Achievement_GetTitle_t)(void* self, void* retval);
extern fn_Achievement_GetTitle_t fn_Achievement_GetTitle;

// Achievement::GetDescription(this, retval) -> String — non-trivial return
// RCX=this, RDX=retval(String*). Returns description string.
// ?GetDescription@Achievement@1@QEBA?AVString@System@@XZ (0x180001420, ordinal)
typedef void (*fn_Achievement_GetDescription_t)(void* self, void* retval);
extern fn_Achievement_GetDescription_t fn_Achievement_GetDescription;

// Achievement::GetSpriteName(this, retval) -> String — non-trivial return
// ?GetSpriteName@Achievement@1@QEBA?AVString@System@@XZ (0x1800013a0)
typedef void (*fn_Achievement_GetSpriteName_t)(void* self, void* retval);
extern fn_Achievement_GetSpriteName_t fn_Achievement_GetSpriteName;

// GoalDialog dynamic array push (FUN_140011750 in retail exe)
// Pushes a 0x18-byte entry (3 pointers: imageComplete, imageFail, textGoal) to goal array.
// Signature: (arrayBase*, newCount, elementPtr*)
typedef void (*fn_GoalDialog_ArrayPush_t)(void* arrayBase, int newCount, void* element);
extern fn_GoalDialog_ArrayPush_t fn_GoalDialog_ArrayPush;

// EventNotifyDialog dynamic array push (FUN_140011750 in retail exe — same function)
// Pushes a 0x18-byte entry (3 pointers: group, image, button) to notification array.
extern fn_GoalDialog_ArrayPush_t fn_EventNotifyDialog_ArrayPush;

// ScenarioDialog dynamic array push (FUN_140013b60 in retail exe)
// Pushes a 0x38-byte entry to scenario array.
typedef void (*fn_ScenarioDialog_ArrayPush_t)(void* arrayBase, int newCount, void* element);
extern fn_ScenarioDialog_ArrayPush_t fn_ScenarioDialog_ArrayPush;

// ExternalList::GetResourceCount(this) -> int
// ?GetResourceCount@ExternalList@Object@@QEBAHXZ (0x180007650)
typedef int (*fn_ExternalList_GetResourceCount_t)(void* self);
extern fn_ExternalList_GetResourceCount_t fn_ExternalList_GetResourceCount;

// Scenario type ID pointer (for IsDerivedFrom check in ScenarioDialog)
extern uint32_t* g_TypeId_Scenario;  // from exref in ScenarioDialog::Slot4

// UI::Element::SetPadding(this, left, top, right, bottom)
// ?SetPadding@Element@UI@@QEAAXHHHH@Z
typedef void (*fn_Element_SetPadding_t)(void* self, int left, int top, int right, int bottom);
extern fn_Element_SetPadding_t fn_Element_SetPadding;

// UI::Sprite::Sprite(retval, ExternalPtr<SpriteSheet>, String const&) — non-trivial return
// RCX=retval(Sprite*), RDX=spriteSheet, R8=spriteName(String const&)
// ??0Sprite@UI@@QEAA@V?$ExternalPtr@VSpriteSheet@UI@@@Object@@AEBVString@System@@@Z
typedef void* (*fn_Sprite_Ctor_t)(void* retval, uint64_t spriteSheet, const void* spriteName);
extern fn_Sprite_Ctor_t fn_Sprite_Ctor;

// UI::String::String(retval, ExternalPtr<StringTable>, String const&) — non-trivial return
// RCX=retval(UIString*), RDX=stringTable, R8=stringName(String const&)
// ??0String@UI@@QEAA@V?$ExternalPtr@VStringTable@UI@@@Object@@AEBV0System@@@Z
typedef void* (*fn_UIString_Ctor_t)(void* retval, uint64_t stringTable, const void* stringName);
extern fn_UIString_Ctor_t fn_UIString_Ctor;

// UI::String::operator System::String() — converts UI::String to System::String
// Member, non-trivial return: RCX=this(UIString*), RDX=retval(System::String*)
// ??BString@UI@@QEBA?AV0System@@XZ (ordinal 1307)
typedef void (*fn_UIString_ToSystemString_t)(void* self, void* retval);
extern fn_UIString_ToSystemString_t fn_UIString_ToSystemString;

// EventNotifyDialog array remove (FUN_140011670 in retail exe)
// Removes entry at index from dynamic array (shifts remaining, decrements count).
typedef void (*fn_EventNotifyDialog_ArrayRemove_t)(void* arrayBase, int index);
extern fn_EventNotifyDialog_ArrayRemove_t fn_EventNotifyDialog_ArrayRemove;

// UI::UserInterface::AddDialog(this, InternalPtr<DialogInstance>)
// ?AddDialog@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z
typedef void (*fn_UI_AddDialog_t)(void* self, void* dialogPtr);
extern fn_UI_AddDialog_t fn_UI_AddDialog;

// UI::Element::GetPaddedSize(this, int& width, int& height) — member function
// ?GetPaddedSize@Element@UI@@QEAAXAEAH0@Z
typedef void (*fn_Element_GetPaddedSize_t)(void* self, int* outWidth, int* outHeight);
extern fn_Element_GetPaddedSize_t fn_Element_GetPaddedSize;

// UI::RadioElement::SetActiveRadio(this, int index)
// ?SetActiveRadio@RadioElement@UI@@QEAAXH@Z
typedef void (*fn_RadioElement_SetActiveRadio_t)(void* self, int index);
extern fn_RadioElement_SetActiveRadio_t fn_RadioElement_SetActiveRadio;

// ToolbarDialog constructor (FUN_1400b9910 in retail exe)
// Creates a child ToolbarDialog from parent + toolbar data
typedef void* (*fn_ToolbarDialog_Ctor_t)(void* self, void* userInterface, void* dialog,
                                         void* parentPtr, void* toolbarData);
extern fn_ToolbarDialog_Ctor_t fn_ToolbarDialog_Ctor;

// ToolbarDialog game system dispatch functions (called from OnEvent)
// Case 1: ShowGroup — toggle visibility of a game group
typedef void (*fn_Toolbar_ShowGroup_t)(void* gameSubsys, int64_t groupId);
extern fn_Toolbar_ShowGroup_t fn_Toolbar_ShowGroup;

// Case 2: Utility action
typedef void (*fn_Toolbar_Utility_t)(void* gameSubsys, void* retBuf, uint64_t param);
extern fn_Toolbar_Utility_t fn_Toolbar_Utility;

// Case 3: Toggle all
typedef void (*fn_Toolbar_ToggleAll_t)(void* gameSubsys);
extern fn_Toolbar_ToggleAll_t fn_Toolbar_ToggleAll;

// Case 4: Focus entity
typedef void* (*fn_Toolbar_FindEntity_t)(void* entityList, void* retBuf, uint64_t typeId, int index);
extern fn_Toolbar_FindEntity_t fn_Toolbar_FindEntity;

// Case 4: Center on entity
typedef void (*fn_Toolbar_CenterOnEntity_t)(void* gameSubsys, void* parentPtr, void* entityPtr);
extern fn_Toolbar_CenterOnEntity_t fn_Toolbar_CenterOnEntity;

// ToolbarDialog close sub-dialog helper (FUN_1400b9730)
typedef void (*fn_ToolbarDialog_CloseSubDialog_t)(void* self);
extern fn_ToolbarDialog_CloseSubDialog_t fn_ToolbarDialog_CloseSubDialog;

// ToolbarDialog change speed helper (FUN_1400b91b0)
typedef void (*fn_ToolbarDialog_ChangeSpeed_t)(void* self, int direction);
extern fn_ToolbarDialog_ChangeSpeed_t fn_ToolbarDialog_ChangeSpeed;

// ToolbarDialog update button states helper (FUN_1400b8fd0)
typedef void (*fn_ToolbarDialog_UpdateButtons_t)(void* self);
extern fn_ToolbarDialog_UpdateButtons_t fn_ToolbarDialog_UpdateButtons;

// ToolbarDialog exref type — for IsDerivedFrom checks on toolbar elements
extern uint32_t* g_TypeId_ToolbarElement;  // _typeInfo_exref in ToolbarDialog Update/OnEvent

// ToolbarDialog parent type ID (DAT_1401c5d38) — for IsDerivedFrom in case 5 (CenterOnPoint)
extern uint32_t* g_TypeId_ToolbarDialogParent;

// ============================================================
// Initialization
// ============================================================

// Resolve all function pointers. Call after ResolveInit().
// Returns true if all critical pointers were resolved.
bool EngineFuncsInit();
