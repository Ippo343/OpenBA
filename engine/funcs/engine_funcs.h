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

// Engine/UpdateLoop singleton — exref at 0x140151440 (double-indirect)
// Sub-fields: +0x10 = engine state (debug counters at +0x2150..+0x215C)
//             +0x58 = total time (Time ticks), +0x68 = frame time (Time ticks)
// Used by DebugUI::Slot8 for performance counter display
extern void** g_pEngineSingleton;     // exref 0x140151440

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

// SaveLoad::SetSaveSprite(this, Sprite) — set the sprite used for save screenshots
// ?SetSaveSprite@SaveLoad@Save@@QEAAXVSprite@UI@@@Z
typedef void (*fn_SaveLoad_SetSaveSprite_t)(void* saveLoad, void* sprite);
extern fn_SaveLoad_SetSaveSprite_t fn_SaveLoad_SetSaveSprite;

// SaveLoad::SetMinOperationTime(this, &Time) — set minimum time for save/load operation display
// ?SetMinOperationTime@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z
typedef void (*fn_SaveLoad_SetMinOperationTime_t)(void* saveLoad, void* time);
extern fn_SaveLoad_SetMinOperationTime_t fn_SaveLoad_SetMinOperationTime;

// SaveLoad::SetOperation(this, Operation, int) — start a save/load/delete operation
// ?SetOperation@SaveLoad@Save@@QEAAXW4Operation@12@H@Z
typedef void (*fn_SaveLoad_SetOperation_t)(void* saveLoad, int operation, int param);
extern fn_SaveLoad_SetOperation_t fn_SaveLoad_SetOperation;

// SaveLoad::GetSaveCount(this) -> int — number of save files
// ?GetSaveCount@SaveLoad@Save@@QEBAHXZ
typedef int (*fn_SaveLoad_GetSaveCount_t)(void* saveLoad);
extern fn_SaveLoad_GetSaveCount_t fn_SaveLoad_GetSaveCount;

// SaveLoad::GetSaveInfo(this, index) -> SaveInfo* — get info for save file at index
// ?GetSaveInfo@SaveLoad@Save@@QEBAAEBUSaveInfo@12@H@Z
typedef void* (*fn_SaveLoad_GetSaveInfo_t)(void* saveLoad, int index);
extern fn_SaveLoad_GetSaveInfo_t fn_SaveLoad_GetSaveInfo;

// SaveLoad::GetSaveState(this) -> SavedState* — returns save state ptr at this+0x40
// Used by GameUpdateLoop ctor to set the save file to load.
// ?GetSaveState@SaveLoad@Save@@QEAAPEAVSavedState@2@XZ (0x1800c3b50, ordinal 2891)
typedef void* (*fn_SaveLoad_GetSaveState_t)(void* saveLoad);
extern fn_SaveLoad_GetSaveState_t fn_SaveLoad_GetSaveState;

// Sprite::IsValid(this) -> bool
// ?IsValid@Sprite@UI@@QEBA_NXZ
typedef bool (*fn_Sprite_IsValid_t)(void* sprite);
extern fn_Sprite_IsValid_t fn_Sprite_IsValid;

// UserInterface::Play(this, soundId) — play a UI sound effect
// ?Play@UserInterface@UI@@QEAAXH@Z
typedef void (*fn_UI_Play_t)(void* ui, int soundId);
extern fn_UI_Play_t fn_UI_Play;

// DateTime::AsString(this, retval) -> String — format date+time as string (non-trivial return)
// ?AsString@DateTime@System@@QEBA?AVString@2@XZ
typedef void* (*fn_DateTime_AsString_t)(void* dateTime, void* retval);
extern fn_DateTime_AsString_t fn_DateTime_AsString;

// SaveLoadDialog helper: FUN_1400b6ce0 — populates the file list table (442 lines, exe function)
typedef void (*fn_SaveLoadDialog_PopulateFileList_t)(void* self);
extern fn_SaveLoadDialog_PopulateFileList_t fn_SaveLoadDialog_PopulateFileList;

// EditElement::SetText(this, String*) — sets edit field text
// ?SetText@EditElement@UI@@QEAAXVString@2@@Z (0x180042070)
typedef void (*fn_EditElement_SetText_t)(void* self, void* str);
extern fn_EditElement_SetText_t fn_EditElement_SetText;

// Input::Device::Released(this, InputDevice, 0, keyIndex) -> bool
// ?Released@Device@Input@@QEBA_NW4InputDevice@12@HH@Z
// InputDevice: 1=keyboard, 2=mouse
typedef bool (*fn_Input_Device_Released_t)(void* device, int inputDevice, int param, int keyIndex);
extern fn_Input_Device_Released_t fn_Input_Device_Released;

// Input::Device singleton — accessed via exe exref at 0x140151b80 (double-indirect)
extern void** g_pInputDeviceSingleton;

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

// AchievementManager::GetAchievementCount(this) -> int
// ?GetAchievementCount@AchievementManager@Achievement@@QEBAHXZ (0x1800079b0, ordinal 2434)
typedef int (*fn_AchievementManager_GetAchievementCount_t)(void* self);
extern fn_AchievementManager_GetAchievementCount_t fn_AchievementManager_GetAchievementCount;

// AchievementManager::GetAchievement(this, retval, index) -> ExternalPtr<Achievement>
// Member, non-trivial return: RCX=this, RDX=retval, R8=index
// ?GetAchievement@AchievementManager@Achievement@@QEBA?AV?$ExternalPtr@VAchievement@1@@Object@@H@Z
// (0x18009e8a0, ordinal 2433)
typedef void (*fn_AchievementManager_GetAchievement_t)(void* self, void* retval, int index);
extern fn_AchievementManager_GetAchievement_t fn_AchievementManager_GetAchievement;

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

// ComboElement type ID: exref at 0x140151690 — for IsDerivedFrom checks on combo elements
extern uint32_t* g_TypeId_ComboElement;

// GetCurrentProductIndex (FUN_140119470 in exe)
// Reads +0x50 product index; if -1 does weighted random selection from +0x58 float array.
typedef uint32_t (*fn_GetCurrentProductIndex_t)(void* component);
extern fn_GetCurrentProductIndex_t fn_GetCurrentProductIndex;

// FindDialogByName (FUN_1400e1600 in exe)
// Parses dialog name string, searches UI for matching dialog instance.
// Non-trivial return: RCX=retval(InternalBasePtr*), RDX=name(String*)
typedef void* (*fn_FindDialogByName_t)(void* retval, void* name);
extern fn_FindDialogByName_t fn_FindDialogByName;

// ResolveExternalByName (FUN_14001b1d0 in exe)
// Resolves and caches an External object by filename. Updates string+0x10 with result.
typedef void (*fn_ResolveExternalByName_t)(void* nameString);
extern fn_ResolveExternalByName_t fn_ResolveExternalByName;

// Object::External::External(this) — base class constructor/reinit
// Runtime function at dispatch table 0x140151e98
typedef void (*fn_External_External_t)(void* self);
extern fn_External_External_t fn_External_External;

// Object::External::~External(this) — base class destructor
// Runtime function at dispatch table 0x140151ea0
typedef void (*fn_External_Dtor_t)(void* self);
extern fn_External_Dtor_t fn_External_Dtor;

// ============================================================
// Phase F Wave 2C: EntityUI::Slot8, ResidenceUI::Slot7, EventNotifyDialog::Slot7
// ============================================================

// Input::KeyMap::GetValue(this, slot, axis) -> short (axis value, e.g. mouse X/Y)
// ?GetValue@KeyMap@Input@@QEBAFHH@Z (0x1800a7b60)
typedef short (*fn_KeyMap_GetValue_t)(void* self, int slot, int axis);
extern fn_KeyMap_GetValue_t fn_KeyMap_GetValue;

// UI::UserInterface::GetFirstContainedDialog(this, x, y) -> InternalPtr<DialogInstance>
// Member, non-trivial return: RCX=this, RDX=retval, R8=x, R9=y
// ?GetFirstContainedDialog@UserInterface@UI@@QEAA?AV?$InternalPtr@VDialogInstance@UI@@@Object@@HH@Z
// (0x18011fb00)
typedef void* (*fn_UI_GetFirstContainedDialog_t)(void* self, void* retval, int x, int y);
extern fn_UI_GetFirstContainedDialog_t fn_UI_GetFirstContainedDialog;

// EditTool_SelectEntity (FUN_1400fd3b0 in exe)
// Selects an entity in the edit tool, opens entity dialog if needed.
// RCX=editTool*, RDX=entityInternalBasePtr*
typedef void (*fn_EditTool_SelectEntity_t)(void* editTool, void* entityPtr);
extern fn_EditTool_SelectEntity_t fn_EditTool_SelectEntity;

// FindComponent_Residence — finds ResidenceComponent on entity (DAT for housing component)
// Used by ResidenceUI::Slot7 to iterate residents.
// Searches entity component array at +0x30/+0x38.
extern fn_FindComponent_t fn_FindComponent_Residence;  // FUN_140006270 or similar

// Age multiplier constant (DAT_140172198 = 5.0f) — converts game age ticks to display years
extern float* g_pAgeMultiplier;

// Notification timer duration constant (DAT_1401721cc = 30.0f) — seconds per notification
extern float* g_pNotifyTimerDuration;

// System::Time::Time(retval, float seconds) — construct Time from float seconds
// ??0Time@System@@QEAA@M@Z
typedef void* (*fn_Time_Ctor_Float_t)(void* retval, float seconds);
extern fn_Time_Ctor_Float_t fn_Time_Ctor_Float;

// System::String::SubString(this, retval, startIndex) -> String (member, non-trivial return)
// RCX=this, RDX=retval(String*), R8=startIndex
// ?SubString@String@System@@QEBA?AV12@H@Z
typedef void* (*fn_String_SubString_t)(void* self, void* retval, int startIndex);
extern fn_String_SubString_t fn_String_SubString;

// System::String::operator=(this, other) -> String& — string assignment
// ?operator=@String@System@@QEAAAEAV12@AEBV12@@Z
typedef void* (*fn_String_Assign_t)(void* self, const void* other);
extern fn_String_Assign_t fn_String_Assign;

// UI::String::String(retval, ExternalPtr<StringTable>) — single-arg ctor from string table entry
// Non-trivial return: RCX=retval(UIString*, 16 bytes), RDX=externalPtr(8 bytes)
// Resolved from exe dispatch table at 0x140151300.
typedef void* (*fn_UIString_Ctor_Entry_t)(void* retval, uint64_t externalPtr);
extern fn_UIString_Ctor_Entry_t fn_UIString_Ctor_Entry;

// UI::Sprite default ctor — UI::Sprite::Sprite(this)
// ??0Sprite@UI@@QEAA@XZ
typedef void* (*fn_Sprite_Default_Ctor_t)(void* retval);
extern fn_Sprite_Default_Ctor_t fn_Sprite_Default_Ctor;

// UI::Dialog::GetReference(this, retval) -> ExternalPtr (member, non-trivial return)
// RCX=this(Dialog*), RDX=retval
// ?GetReference@Dialog@UI@@QEBA?AV?$ExternalPtr@VDialogReference@UI@@@Object@@XZ
typedef void* (*fn_Dialog_GetReference_t)(void* self, void* retval);
extern fn_Dialog_GetReference_t fn_Dialog_GetReference;

// Audio::Scene::PlayEffect(scene, soundEffect, soundType, time) -> uint
// ?PlayEffect@Scene@Audio@@QEAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@W4SoundType@12@VTime@System@@@Z
// (0x1800def60)
typedef uint32_t (*fn_Audio_Scene_PlayEffect_t)(void* scene, uint64_t soundEffect, int soundType, void* timeParam);
extern fn_Audio_Scene_PlayEffect_t fn_Audio_Scene_PlayEffect;

// EventNotifyDialog active event array push (FUN_140011750 — same as GoalDialog/EventNotifyDialog array push)
// Already declared as fn_EventNotifyDialog_ArrayPush.

// UI::DialogInstance::DialogInstance ctor — creates a dialog from parent+template
// FUN_140067c30 uses this to create EventPopupDialog instances
// Already available through fn_UI_AddDialog and MemAlloc.

// EventPopupDialog vtable pointer (PTR_FUN_14015bd80)
extern void** g_EventPopupDialog_Vtable;

// ============================================================
// HelpUI support functions
// ============================================================

// UI::StringTable::GetText(this, retval, textIndex) — member, non-trivial return
// RCX=this(StringTable*), RDX=retval(String*), R8D=textIndex
// ?GetText@StringTable@UI@@QEBA?AVString@System@@H@Z
typedef void (*fn_StringTable_GetText_t)(void* self, void* retval, int textIndex);
extern fn_StringTable_GetText_t fn_StringTable_GetText;

// UI::DocumentTextElement::GetLink(this, retval, linkIndex) — member, non-trivial return
// RCX=this(DocumentTextElement*), RDX=retval(String*), R8D=linkIndex
// ?GetLink@DocumentTextElement@UI@@QEBA?AVString@System@@H@Z
typedef void (*fn_DocumentTextElement_GetLink_t)(void* self, void* retval, int linkIndex);
extern fn_DocumentTextElement_GetLink_t fn_DocumentTextElement_GetLink;

// UI::DocumentTextElement::SetText(this, UI::String) — takes UI::String (16 bytes) by value
// RCX=this(DocumentTextElement*), RDX=uiString (pointer to 16-byte UI::String)
// ?SetText@DocumentTextElement@UI@@QEAAXVString@2@@Z
typedef void (*fn_DocumentTextElement_SetText_UIString_t)(void* self, const void* uiString);
extern fn_DocumentTextElement_SetText_UIString_t fn_DocumentTextElement_SetText_UIString;

// UI::DocumentTextElement::SetText(this, System::String const&)
// RCX=this(DocumentTextElement*), RDX=text(String const&)
// ?SetText@DocumentTextElement@UI@@QEAAXAEBVString@System@@@Z
typedef void (*fn_DocumentTextElement_SetText_t)(void* self, const void* text);
extern fn_DocumentTextElement_SetText_t fn_DocumentTextElement_SetText;

// UI::ContainerElement::SetOffsets(this, offsetX, offsetY)
// ?SetOffsets@ContainerElement@UI@@QEAAXHH@Z
typedef void (*fn_ContainerElement_SetOffsets_t)(void* self, int offsetX, int offsetY);
extern fn_ContainerElement_SetOffsets_t fn_ContainerElement_SetOffsets;

// UI::String::String(this, ExternalPtr<StringTable>, int textIndex) — ctor by index
// RCX=this(UIString*, 16 bytes), RDX=stringTable(ExternalPtr), R8D=textIndex
// ??0String@UI@@QEAA@V?$ExternalPtr@VStringTable@UI@@@Object@@H@Z
typedef void* (*fn_UIString_Ctor_Index_t)(void* self, uint64_t stringTable, int textIndex);
extern fn_UIString_Ctor_Index_t fn_UIString_Ctor_Index;

// System::String::operator==(this, other) -> bool — string equality
// ??8String@System@@QEBA_NAEBV01@@Z
typedef bool (*fn_String_Equal_t)(const void* self, const void* other);
extern fn_String_Equal_t fn_String_Equal;

// StringTable type ID — resolved from _typeInfo_exref at 0x140151488
// Used for IsDerivedFrom checks on ExternalList entries
extern uint32_t* g_TypeId_StringTable;

// HelpUI element type IDs — resolved from _typeInfo_exref addresses in exe
// Used by Slot4's FindElementType calls to find typed UI elements
extern uint32_t* g_TypeId_HelpFirstElem;      // exref 0x1401517e8 -> +0x30
extern uint32_t* g_TypeId_HelpComboElem;       // exref 0x140151690 -> +0x40
extern uint32_t* g_TypeId_HelpDocTextElem;     // exref 0x1401517f0 -> +0x38
extern uint32_t* g_TypeId_HelpContainerElem;   // exref 0x1401517f8 -> +0x58
// Type ID used for IsDerivedFrom on named button elements (buttonBack/buttonForward)
extern uint32_t* g_TypeId_HelpButtonElem;      // exref 0x1401514f8 (same used in Slot9 back/fwd)

// HelpUI history array resize helper (FUN_140011d50 in exe)
// Resizes dynamic array: param1=array base (ptr+count+capacity), param2=new count
typedef void (*fn_HelpUI_ArrayResize_t)(void* arrayBase, int newCount);
extern fn_HelpUI_ArrayResize_t fn_HelpUI_ArrayResize;

// ============================================================
// BuildUI support functions
// ============================================================

// FUN_140118330: Counts occupied build tiles for a StorageComponent.
// Iterates the building footprint, counts tiles occupied by other entities.
// param1 = StorageComponent ptr
typedef int (*fn_CountOccupiedBuildTiles_t)(void* storageComp);
extern fn_CountOccupiedBuildTiles_t fn_CountOccupiedBuildTiles;

// ============================================================
// CitizenUI helpers
// ============================================================

// FindComponent_CitizenAI — finds CitizenAIComponent on entity
// FUN_140009790: entity(longlong), retval(InternalBasePtr*)
extern fn_FindComponent_t fn_FindComponent_CitizenAI;  // 0x140009790

// CitizenDescription type ID at DAT_1401c7190 (used for IsDerivedFrom in Slot7)
extern uint32_t* g_TypeId_CitizenDescription;  // DAT_1401c7190

// HappinessComponent type ID at DAT_1401c7e08 (used for FindComponent in StatusBarUI)
extern uint32_t* g_TypeId_HappinessComponent;  // DAT_1401c7e08

// ProfessionComponent type ID at DAT_1401c70e8 (used by FindProfessionComponent)
extern uint32_t* g_TypeId_ProfessionComponent;  // DAT_1401c70e8

// CitizenUI_BuildIconArray(self+offset, groupElement) — builds whole/half icon arrays
// FUN_14006abf0: iterates children of group, finds "whole"/"half" sub-elements
typedef void (*fn_CitizenUI_BuildIconArray_t)(void* iconArrayBase, void* groupElemPtr);
extern fn_CitizenUI_BuildIconArray_t fn_CitizenUI_BuildIconArray;

// CitizenUI_UpdateIconDisplay(iconArrayBase, currentVal, maxVal) — updates heart/star icons
// FUN_14006aed0: shows/hides whole/half icons based on ratio
typedef void (*fn_CitizenUI_UpdateIconDisplay_t)(void* iconArrayBase, float currentVal, float maxVal);
extern fn_CitizenUI_UpdateIconDisplay_t fn_CitizenUI_UpdateIconDisplay;

// FindProfessionComponent(entityPtr, retval) — finds ProfessionComponent on entity
// FUN_140011050: searches component array for type DAT_1401c70e8
typedef void* (*fn_FindProfessionComponent_t)(void* entity, void* retval);
extern fn_FindProfessionComponent_t fn_FindProfessionComponent;

// CitizenUI::Slot9 original — used as fallback for follow button (0x228)
// The follow dialog construction has MSVC hidden stack parameters.
typedef void (*fn_CitizenUI_Slot9_Original_t)(void* self, uint64_t p2, int p3, int p4);
extern fn_CitizenUI_Slot9_Original_t fn_CitizenUI_Slot9_Original;

// Game world+0x118 lookup helper (FUN_1400f9a60)
// Finds entity by tile index in the world grid
typedef void* (*fn_WorldGrid_FindEntity_t)(void* worldGrid, void* retval, int tileIndex);
extern fn_WorldGrid_FindEntity_t fn_WorldGrid_FindEntity;

// ============================================================
// ProductionUI support
// ============================================================

// FindComponent_Tracker: FUN_140013320 — searches entity components for TrackerComponent
// (TypeInfo DAT_1401c4b10). Same FindComponent pattern as others.
extern fn_FindComponent_t fn_FindComponent_Tracker;  // FUN_140013320

// TrackerComponent rebuild: FUN_14000d9d0 — moves/rebuilds tracker item arrays
// Called when game world item count changes.
typedef void (*fn_TrackerComponent_Rebuild_t)(void* srcArray, void* dstArray);
extern fn_TrackerComponent_Rebuild_t fn_TrackerComponent_Rebuild;

// ============================================================
// ProfessionUI support
// ============================================================

// Object::External::GetPath(this) -> Filename* (returns internal pointer)
typedef void* (*fn_External_GetPath_t)(void* self);
extern fn_External_GetPath_t fn_External_GetPath;

// System::Filename::GetPathIdentifer(this, retval) -> String (member, non-trivial return)
typedef void* (*fn_Filename_GetPathIdentifier_t)(void* self, void* retval);
extern fn_Filename_GetPathIdentifier_t fn_Filename_GetPathIdentifier;

// BST rebalance after insert: FUN_140013480
typedef void (*fn_BSTRebalance_t)(void* bstBase, void** insertedNode);
extern fn_BSTRebalance_t fn_BSTRebalance;

// Slot9 spinner handler: FUN_1400763c0 (complex citizen assignment)
typedef void (*fn_ProfessionUI_HandleSpinner_t)(void* self, int profIndex);
extern fn_ProfessionUI_HandleSpinner_t fn_ProfessionUI_HandleSpinner;

// Slot9 button handler: FUN_140076a30 (find citizen, center camera)
typedef void (*fn_ProfessionUI_HandleButton_t)(void* self, int profIndex);
extern fn_ProfessionUI_HandleButton_t fn_ProfessionUI_HandleButton;

// FUN_14012e5c0: Calculate worker slots from StorageLocationComponent
typedef int (*fn_CalcWorkerSlots_t)(void* storageLocComp);
extern fn_CalcWorkerSlots_t fn_CalcWorkerSlots;

// ============================================================
// MainMenuDialog support functions
// ============================================================

// Application::Application::Quit(this) — sets quit flag
// ?Quit@Application@1@QEAAXXZ (ordinal 3437)
typedef void (*fn_App_Quit_t)(void* self);
extern fn_App_Quit_t fn_App_Quit;

// System::Configuration::CurrentVersion() -> String (static, hidden ret ptr in RCX)
// ?CurrentVersion@Configuration@System@@SA?AVString@2@XZ
typedef void (*fn_Configuration_CurrentVersion_t)(void* retStr);
extern fn_Configuration_CurrentVersion_t fn_Configuration_CurrentVersion;

// System::Configuration::CurrentBuildId() -> String (static, hidden ret ptr in RCX)
// ?CurrentBuildId@Configuration@System@@SA?AVString@2@XZ
typedef void (*fn_Configuration_CurrentBuildId_t)(void* retStr);
extern fn_Configuration_CurrentBuildId_t fn_Configuration_CurrentBuildId;

// Object::InternalBasePtr::CheckPtr(this) — validates internal pointer
// ?CheckPtr@InternalBasePtr@Object@@QEAAXXZ
typedef void (*fn_InternalBasePtr_CheckPtr_t)(void* self);
extern fn_InternalBasePtr_CheckPtr_t fn_InternalBasePtr_CheckPtr;

// UI::DialogInstance::DialogInstance(this, userInterface, externalResource, parentPtr, p4, p5, p6)
// Full constructor for creating child dialog instances
// Resolved from Runtime export
typedef void* (*fn_DialogInstance_Ctor_t)(void* self, void* userInterface, void* externalRes,
                                          void* parentPtr, int p4, int p5, int p6);
extern fn_DialogInstance_Ctor_t fn_DialogInstance_Ctor;

// MainMenuDialog BuildId child dialog vtable (PTR_FUN_14015f5a0 in exe)
extern void** g_MainMenuDialog_BuildIdVtable;

// MainMenuDialog Slot8 private handler functions (resolved from exe)
// Each opens a sub-dialog or performs a game action.
typedef void (*fn_MainMenu_Handler_t)(void* self);
extern fn_MainMenu_Handler_t fn_MainMenu_OpenNewGame;      // 0x1400B3620
extern fn_MainMenu_Handler_t fn_MainMenu_OpenLoad;          // 0x1400B3770
extern fn_MainMenu_Handler_t fn_MainMenu_OpenSave;          // 0x1400B38B0
extern fn_MainMenu_Handler_t fn_MainMenu_OpenOptions;       // 0x1400B3A10
extern fn_MainMenu_Handler_t fn_MainMenu_ReturnToMenu;      // 0x1400B4790
extern fn_MainMenu_Handler_t fn_MainMenu_OpenAchievement;   // 0x1400B34C0
extern fn_MainMenu_Handler_t fn_MainMenu_OpenScenario;      // 0x1400B3380
extern fn_MainMenu_Handler_t fn_MainMenu_OpenGoal;          // 0x1400B3220
extern fn_MainMenu_Handler_t fn_MainMenu_OpenMap;           // 0x1400B3120
extern fn_MainMenu_Handler_t fn_MainMenu_OpenTutorial;      // 0x1400B2FE0
extern fn_MainMenu_Handler_t fn_MainMenu_OpenMod;           // 0x1400B2EA0

// ============================================================
// NewGameDialog support functions
// ============================================================

// Game settings pointer (DAT_1401c3de0 in exe .data section)
// Allocated in GameMain, 0x34 bytes. Contains:
//   +0x20: terrain generator combo index
//   +0x24: terrain size combo index
//   +0x28: climate combo index
//   +0x2c: map condition/generator combo index
//   +0x30: disaster combo index
extern void** g_pNewGameSettings;  // DAT_1401c3de0

// System::Random::Rand(this, maxVal) -> uint
// ?Rand@Random@System@@QEAAII@Z (ordinal 3442, 0x1800c8040)
typedef uint32_t (*fn_Random_Rand_t)(void* self, uint32_t maxVal);
extern fn_Random_Rand_t fn_Random_Rand;

// Name generator: FUN_140099ac0(unused, retStr, random, maxLen)
// Loads "Game/Names/Cities.ndb", generates random name
typedef void* (*fn_GenerateTownName_t)(uint64_t unused, void* retStr, void* random, int maxLen);
extern fn_GenerateTownName_t fn_GenerateTownName;

// Start game handler: FUN_1400b4540(worldSetupPtr, nameStr, seed, terrainSize, disasterOff, climate, terrain, mapGen, param9, retval)
typedef void (*fn_StartNewGame_t)(void* worldSetup, void* nameStr, uint64_t seed,
                                   uint64_t terrainSize, int disasterOff, void* climate,
                                   void* terrain, void* mapGen, int param9, void* retval);
extern fn_StartNewGame_t fn_StartNewGame;

// TypeInfo IDs for generator IsDerivedFrom checks
extern uint32_t* g_TypeId_MapGenerator;      // DAT_1401c59b8
extern uint32_t* g_TypeId_ClimateGenerator;  // DAT_1401c5910
extern uint32_t* g_TypeId_TerrainGenerator;  // DAT_1401c5b40

// ============================================================
// OptionsDialog support functions (private helpers in exe)
// ============================================================

// OptionsDialog::PopulateVideoComboBoxes(self) — fills renderer, adapter, resolution, refresh, antialias, filter combos
// FUN_140074140
typedef void (*fn_OptionsDialog_PopulateVideoComboBoxes_t)(void* self);
extern fn_OptionsDialog_PopulateVideoComboBoxes_t fn_OptionsDialog_PopulateVideoComboBoxes;

// OptionsDialog::PopulateAdapterCombo(self) — refills adapter combo after renderer change
// FUN_140074040
extern fn_OptionsDialog_PopulateVideoComboBoxes_t fn_OptionsDialog_PopulateAdapterCombo;

// OptionsDialog::PopulateResolutionCombo(self) — refills resolution combo
// FUN_140073ec0
extern fn_OptionsDialog_PopulateVideoComboBoxes_t fn_OptionsDialog_PopulateResolutionCombo;

// OptionsDialog::PopulateRefreshCombo(self) — refills refresh rate combo
// FUN_140073d20
extern fn_OptionsDialog_PopulateVideoComboBoxes_t fn_OptionsDialog_PopulateRefreshCombo;

// OptionsDialog::PopulateAntialiasCombo(self) — refills antialias mode combo
// FUN_140073b10
extern fn_OptionsDialog_PopulateVideoComboBoxes_t fn_OptionsDialog_PopulateAntialiasCombo;

// OptionsDialog::PopulateFilterCombo(self) — refills texture filter combo
// FUN_140073800
extern fn_OptionsDialog_PopulateVideoComboBoxes_t fn_OptionsDialog_PopulateFilterCombo;

// OptionsDialog::PopulateInputBindList(self) — fills input binding list
// FUN_140073580
extern fn_OptionsDialog_PopulateVideoComboBoxes_t fn_OptionsDialog_PopulateInputBindList;

// OptionsDialog::ValidateVideoConfig(self) — validates and fixes config after combo changes
// FUN_140072eb0
extern fn_OptionsDialog_PopulateVideoComboBoxes_t fn_OptionsDialog_ValidateVideoConfig;

// OptionsDialog::OnOk(self) — apply all settings, save, close dialog
// FUN_140072930
extern fn_OptionsDialog_PopulateVideoComboBoxes_t fn_OptionsDialog_OnOk;

// OptionsDialog::OnCancel(self) — revert settings, close dialog
// FUN_140072840
extern fn_OptionsDialog_PopulateVideoComboBoxes_t fn_OptionsDialog_OnCancel;

// OptionsDialog::OnApply(self) — apply all settings, save, keep dialog open
// FUN_1400728e0
extern fn_OptionsDialog_PopulateVideoComboBoxes_t fn_OptionsDialog_OnApply;

// OptionsDialog::OnInputBindClick(self, element, bindIndex, slot) — handle input bind button
// FUN_1400729a0
typedef void (*fn_OptionsDialog_OnInputBindClick_t)(void* self, void* element, int bindIndex, int slot);
extern fn_OptionsDialog_OnInputBindClick_t fn_OptionsDialog_OnInputBindClick;

// Video::Device singleton — needed for video config singletons
// Already available as g_pVideoDeviceSingleton

// Audio::Configuration singleton — _singleton_exref used in slider handlers
// Points to int[4] array of volume levels at offsets 0, 4, 8, 0xC
extern void** g_pAudioConfigSingleton;  // resolved from _singleton_exref in slider code

// Video::Configuration singleton — contains current video settings
extern void** g_pVideoConfigSingleton;  // resolved from _singleton_exref in Startup

// Video::Device::GetInterface(this, index) -> GraphicsInterface*
typedef void* (*fn_VideoDevice_GetInterface_t)(void* self, int index);
extern fn_VideoDevice_GetInterface_t fn_VideoDevice_GetInterface;

// Video::Device::GetInterfaceCount(this) -> int
typedef int (*fn_VideoDevice_GetInterfaceCount_t)(void* self);
extern fn_VideoDevice_GetInterfaceCount_t fn_VideoDevice_GetInterfaceCount;

// UI::Sprite::Sprite() — default constructor, zeroes 16 bytes
typedef void* (*fn_Sprite_DefaultCtor_t)(void* self);
extern fn_Sprite_DefaultCtor_t fn_Sprite_DefaultCtor;

// UI::TabElement::SetActiveTab(this, tabIndex)
typedef void (*fn_TabElement_SetActiveTab_t)(void* self, int tabIndex);
extern fn_TabElement_SetActiveTab_t fn_TabElement_SetActiveTab;

// UI::Element::GetParent(elem) -> Element*
// FUN_140010460 — returns parent element pointer
typedef void* (*fn_Element_GetParent_t)(void* self);
extern fn_Element_GetParent_t fn_Element_GetParent;

// OptionsDialog game settings global — DAT_1401c3de0 in exe
// int[8] array: [0]=uiScale, [1]=autosaveInterval, [2]=statusIconDist,
// [3]=statusIconEnabled, [4]=statusIconScale, [5]=edgeScroll,
// [6]=autopause, [7]=lastTab
// Byte offsets: +0x15=showCelsius, +0x16=showWeather, +0x17=autoslow
extern int32_t* g_pGameSettings;  // DAT_1401c3de0

// ============================================================
// StorageUI support functions
// ============================================================

// ExternalFactory::FindNext(this, typeId, prevExternal) -> External const*
// Iterates all loaded externals matching typeId. Pass nullptr to start.
// ?FindNext@ExternalFactory@Object@@QEBAPEBVExternal@2@IPEBV32@@Z
typedef void* (*fn_ExternalFactory_FindNext_t)(void* self, uint32_t typeId, void* prev);
extern fn_ExternalFactory_FindNext_t fn_ExternalFactory_FindNext;

// StorageUI sort function (FUN_1400213f0) — sorts 0x18-byte entries by resource name.
// Quicksort/introsort with inline comparator using UI::String → System::String.
typedef void (*fn_StorageUI_SortItems_t)(void* items, int count, void* scratch, void* scratch2);
extern fn_StorageUI_SortItems_t fn_StorageUI_SortItems;

// MapUI helper: creates MaterialInstance + SpriteSheet from ImageBuffer, finds image element
// FUN_14006cb00(self, imageBuffer, spriteSheetOut, materialOut, useBlend, elementName)
typedef void (*fn_MapUI_SetupImage_t)(void* self, void* imageBuffer, void** spriteSheetOut,
                                      void** materialOut, char useBlend, const char* elementName);
extern fn_MapUI_SetupImage_t fn_MapUI_SetupImage;

// MapUI helper: translates map click coordinates to camera position
// FUN_14006c850(param1, clickX, clickY, mapWidth, mapHeight)
typedef void (*fn_MapUI_HandleClick_t)(void* param1, int clickX, int clickY, int mapWidth, int mapHeight);
extern fn_MapUI_HandleClick_t fn_MapUI_HandleClick;

// ============================================================
// ToolUI support functions
// ============================================================

// UI::DialogInstance::SetUpperLeft(this, x, y, animate)
// ?SetUpperLeft@DialogInstance@UI@@QEAAXHH_N@Z
typedef void (*fn_DialogInstance_SetUpperLeft_t)(void* self, int x, int y, bool animate);
extern fn_DialogInstance_SetUpperLeft_t fn_DialogInstance_SetUpperLeft;

// UI::UserInterface::BringDialogToFront(this, InternalPtr<DialogInstance>&)
// ?BringDialogToFront@UserInterface@UI@@QEAAXAEAV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z
typedef void (*fn_UI_BringDialogToFront_t)(void* self, void* dialogPtr);
extern fn_UI_BringDialogToFront_t fn_UI_BringDialogToFront;

// FUN_14012e100: UIDescription_AddControllers(uiDesc, dialogInstPtr, dataSource)
// Iterates UIDescription entries and calls DialogInstance::AddController for each.
typedef void (*fn_UIDesc_AddControllers_t)(void* uiDesc, void* dialogInstPtr, void* dataSource);
extern fn_UIDesc_AddControllers_t fn_UIDesc_AddControllers;

// TypeId for UIDescription — DAT_1401c7f90
extern uint32_t* g_TypeId_UIDescription;

// ============================================================
// ModDetailsDialog support functions
// ============================================================

// UI::Element::Hide(this, hide) — sets/clears hidden flag
// ?Hide@Element@UI@@QEAAX_N@Z
typedef void (*fn_Element_Hide_t)(void* self, bool hide);
extern fn_Element_Hide_t fn_Element_Hide;

// IO::Modification::Find(this, Filename const&) -> int (mod index, -1 if not found)
// ?Find@Modification@IO@@QEBAHAEBVFilename@System@@@Z
typedef int (*fn_Modification_Find_t)(void* self, const void* filename);
extern fn_Modification_Find_t fn_Modification_Find;

// System::Configuration::GetVersionString(retval, version, isBeta) -> String
// Static with hidden return: RCX=retval(String*), RDX=version(uint), R8B=isBeta(bool)
// ?GetVersionString@Configuration@System@@SA?AVString@2@I_N@Z
typedef void* (*fn_Configuration_GetVersionString_t)(void* retval, uint32_t version, bool isBeta);
extern fn_Configuration_GetVersionString_t fn_Configuration_GetVersionString;

// System::String::operator+=(this, String const&) — appends string in-place
// Non-exported, resolved from exe dispatch table at 0x140151a00
typedef void (*fn_String_AppendStr_t)(void* self, const void* other);
extern fn_String_AppendStr_t fn_String_AppendStr;

// System::operator+(lhs, rhs) -> String — free function, hidden ret ptr
// Non-exported, resolved from exe dispatch table at 0x140151a08
// RCX=retval(String*), RDX=lhs(String const&), R8=rhs(String const&)
typedef void* (*fn_String_Concat_t)(void* retval, const void* lhs, const void* rhs);
extern fn_String_Concat_t fn_String_Concat;

// System::Filename::operator<(this, other) -> bool — lexicographic comparison
// Non-exported, resolved from exe dispatch table at 0x140151a20
typedef bool (*fn_Filename_Less_t)(const void* self, const void* other);
extern fn_Filename_Less_t fn_Filename_Less;

// FUN_14006e290: ModDetails string array push — appends a String copy to dynamic array
// RCX=arrayBase(ptr+count+capacity), RDX=string(String const*)
typedef void (*fn_ModDetails_StringArrayPush_t)(void* arrayBase, const void* str);
extern fn_ModDetails_StringArrayPush_t fn_ModDetails_StringArrayPush;

// ============================================================
// IO::Modification (mod management singleton)
// ============================================================

// IO::Modification singleton pointer — exe .rdata at 0x1401519e8
// Usage: Modification* mod = *(Modification**)g_ppModificationSingleton;
extern void** g_ppModificationSingleton;

// IO::Modification::GetCount(this) -> int
// Too small to hook (4 bytes at 0x18000da90). Returns m_count.
typedef int (*fn_Modification_GetCount_t)(void* self);
extern fn_Modification_GetCount_t fn_Modification_GetCount;

// IO::Modification::GetModification(this, index) -> Mod*
// Too small to hook (12 bytes at 0x18000da80). Returns &m_mods[index].
typedef void* (*fn_Modification_GetMod_t)(void* self, int index);
extern fn_Modification_GetMod_t fn_Modification_GetMod;

// IO::Modification::Swap(this, idx1, idx2)
// Hooked at 0x1800aeb20. Swaps two mod entries by index.
typedef void (*fn_Modification_Swap_t)(void* self, int idx1, int idx2);
extern fn_Modification_Swap_t fn_Modification_Swap;

// IO::Modification::Enable(this, index, enabled)
// Hooked at 0x1800aeab0. Sets enabled flag.
typedef void (*fn_Modification_Enable_t)(void* self, int index, bool enabled);
extern fn_Modification_Enable_t fn_Modification_Enable;

// IO::Modification::RefreshList(this)
// Hooked at 0x1800af5f0. Scans filesystem for .pkm packages.
typedef void (*fn_Modification_RefreshList_t)(void* self);
extern fn_Modification_RefreshList_t fn_Modification_RefreshList;

// IO::Modification::SaveList(this)
// Hooked at 0x1800b05f0. Persists mod state.
typedef void (*fn_Modification_SaveList_t)(void* self);
extern fn_Modification_SaveList_t fn_Modification_SaveList;

// ============================================================
// UI::DialogInstance::OnElementEvent
// ============================================================

// DialogInstance::OnElementEvent(this, elementPtr, msg, id, p5, p6, p7, p8)
// Base class event handler — exe dispatch at [0x140151a30]
typedef void (*fn_DialogInstance_OnElementEvent_t)(void* self, void* elementPtr,
    int msg, int id, uint32_t p5, uint32_t p6, uint32_t p7, uint64_t p8);
extern fn_DialogInstance_OnElementEvent_t fn_DialogInstance_OnElementEvent;

// UI::DialogInstance::AddController(this, typeId, p3, p4, external, nameStr)
// Adds a sub-controller to a dialog instance.
typedef void (*fn_DialogInstance_AddController_t)(void* self, uint32_t typeId,
    int p3, int p4, void* external, void* nameStr);
extern fn_DialogInstance_AddController_t fn_DialogInstance_AddController;

// ============================================================
// ModDialog helper functions (exe-local)
// ============================================================

// FUN_14006eec0: Refresh mod list display (updates text, icons, thumbnails)
typedef void (*fn_ModDialog_RefreshDisplay_t)(void* self);
extern fn_ModDialog_RefreshDisplay_t fn_ModDialog_RefreshDisplay;

// FUN_14006e930: Toggle mod enabled/disabled at index
typedef void (*fn_ModDialog_ToggleMod_t)(void* self, int index);
extern fn_ModDialog_ToggleMod_t fn_ModDialog_ToggleMod;

// FUN_14006e660: Move mod to top of list
typedef void (*fn_ModDialog_MoveToTop_t)(void* self, int index);
extern fn_ModDialog_MoveToTop_t fn_ModDialog_MoveToTop;

// FUN_14006e490: Show mod details dialog
typedef void (*fn_ModDialog_ShowDetails_t)(void* self, int index);
extern fn_ModDialog_ShowDetails_t fn_ModDialog_ShowDetails;

// FUN_14006ea70: Apply mod changes (save, reload if needed)
typedef void (*fn_ModDialog_Apply_t)(void* self);
extern fn_ModDialog_Apply_t fn_ModDialog_Apply;

// ============================================================
// IO::Serializer — save/load serialization (shared globals)
// ============================================================
// Note: Many reimpl files define local statics with names like
// fn_Serializer_Ctor. These shared globals use g_ prefix to avoid conflicts.

typedef void (*g_fn_Serializer_Ctor_t)(void* self, void* stream, int version);
extern g_fn_Serializer_Ctor_t g_fn_Serializer_Ctor;

typedef void (*g_fn_Serializer_Dtor_t)(void* self);
extern g_fn_Serializer_Dtor_t g_fn_Serializer_Dtor;

typedef void (*g_fn_Serialize_Int_t)(void* self, void* field, const char* name);
extern g_fn_Serialize_Int_t g_fn_Serialize_Int;

typedef int (*g_fn_Serializer_GetVersion_t)(void* self);
extern g_fn_Serializer_GetVersion_t g_fn_Serializer_GetVersion;

// ============================================================
// TownHallUI support functions
// ============================================================

// TabElement type ID: exref at 0x140151de0 (double-indirect)
extern uint32_t* g_TypeId_TabElement;

// TownHallUI::Serialize sub-functions (exe-local tab init)
typedef void (*fn_TownHallUI_SubFunc_t)(void* self);
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_InitGeneral;     // FUN_1400807f0
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_InitProfessions; // FUN_140080160
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_InitProduction;  // FUN_14007fc30
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_InitInventory;   // FUN_14007f660
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_InitGraph;       // FUN_14007f0a0
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_InitAgriculture; // FUN_14007ebf0
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateGraph;     // FUN_14007e5b0

// TownHallUI::Startup sub-functions (tab update dispatchers)
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateGeneral;      // FUN_14007d030
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateProfessions;  // FUN_14007ccb0
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateProduction;   // FUN_14007c7f0
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateAgriculture;  // FUN_14007c220

// TownHallUI::OnCreate sub-functions
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateInventory;    // FUN_14007def0
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateAll;          // FUN_14007d590

// TownHallUI::OnRemove sub-functions
typedef void (*fn_TownHallUI_OnProfChange_t)(void* self, int index);
extern fn_TownHallUI_OnProfChange_t fn_TownHallUI_OnProfessionButton;  // FUN_14007dca0
extern fn_TownHallUI_OnProfChange_t fn_TownHallUI_OnProfessionSpinner; // FUN_14007dd10

// Game singleton pointer: DAT_1401c3dd8 in exe .data section
// Used by TownHallUI::OnCreate (+0xf0->+0x150->+8 = game time tick)
// and OnRemove (+0xb0->+0x28 = profession count)
extern void** g_pGameSingleton;  // 0x1401c3dd8

// ============================================================
// SpriteCaptureUI support functions
// ============================================================

// EditElement type ID: exref at 0x140151b50 (double-indirect: ptr -> ptr -> typeId)
extern uint32_t* g_TypeId_EditElement;

// CaptureBoxElement type ID: DAT_1401c5638 (direct dword in .data)
extern uint32_t* g_TypeId_CaptureBoxElement;

// ============================================================
// TradeUI support functions
// ============================================================

// FindComponent_Building — searches entity components for BuildingComponent (DAT_1401c4b80)
// FUN_140015140(entity, retval) — same pattern as other FindComponent variants
extern fn_FindComponent_t fn_FindComponent_Building;  // 0x140015140

// UI::Element::SetNeedsSizeCalc(this) — marks element for layout recalculation
// ?SetNeedsSizeCalc@Element@UI@@QEAAXXZ
typedef void (*fn_Element_SetNeedsSizeCalc_t)(void* self);
extern fn_Element_SetNeedsSizeCalc_t fn_Element_SetNeedsSizeCalc;

// TradeUI::Serialize (0x140084dd0) — complex setup: finds 15 elements, builds 4 table arrays,
// enumerates ExternalFactory for resource types, populates trade tables with spinners
typedef void (*fn_TradeUI_Serialize_t)(void* self);
extern fn_TradeUI_Serialize_t fn_TradeUI_Serialize;

// TradeUI::Startup (0x140082150) — complex: calculates sell/buy totals from order arrays,
// checks storage capacity, shows/hides warning text, updates trade buttons
typedef void (*fn_TradeUI_Startup_t)(void* self);
extern fn_TradeUI_Startup_t fn_TradeUI_Startup;

// TradeUI::OnCreate (0x1400827b0) — checks if order count changed, refreshes tables
typedef void (*fn_TradeUI_OnCreate_t)(void* self);
extern fn_TradeUI_OnCreate_t fn_TradeUI_OnCreate;

// TradeUI::OnRemove (0x140081f20) — event dispatcher: handles button clicks, spinner changes,
// combo selections based on event type and element ID ranges
typedef void (*fn_TradeUI_OnRemove_t)(void* self, void* param2, int eventType, int eventId);
extern fn_TradeUI_OnRemove_t fn_TradeUI_OnRemove;

// TradeUI::Update (0x1400868c0) — serializes/restores active tab and sort combo state
typedef void (*fn_TradeUI_Update_t)(void* self, void* stream, void* p3, int64_t p4);
extern fn_TradeUI_Update_t fn_TradeUI_Update;

// TradeUI::RefreshSortedDisplay (0x1400846b0) — rebuilds sorted trade table display
// Called after tab/sort change in Update, OnCreate, and at end of Serialize
typedef void (*fn_TradeUI_RefreshDisplay_t)(void* self, uint64_t p2, uint64_t p3, int64_t p4);
extern fn_TradeUI_RefreshDisplay_t fn_TradeUI_RefreshDisplay;

// TradeUI sub-functions: refresh tables and update totals
extern fn_TownHallUI_SubFunc_t fn_TradeUI_RefreshSellTable;   // FUN_140083c70 (381 lines)
extern fn_TownHallUI_SubFunc_t fn_TradeUI_RefreshBuyTable;    // FUN_140083100 (488 lines)
extern fn_TownHallUI_SubFunc_t fn_TradeUI_UpdateSellTotals;   // FUN_140082920 (49 lines)
extern fn_TownHallUI_SubFunc_t fn_TradeUI_UpdateBuyTotals;    // FUN_140082850 (47 lines)

// TradeUI::OnRemove sub-functions (called by the event dispatcher)
// FUN_140081420: OnTradeButton(self, p2, p3, p4) — handles buttonTrade click (id 0x4e20)
typedef void (*fn_TradeUI_OnTradeButton_t)(void* self, void* p2, void* p3, int64_t p4);
extern fn_TradeUI_OnTradeButton_t fn_TradeUI_OnTradeButton;    // 0x140081420

// FUN_140081950: OnDismissButton(self) — handles buttonDismiss click (id 0x4e21)
extern fn_TownHallUI_SubFunc_t fn_TradeUI_OnDismissButton;    // 0x140081950

// FUN_140081d00: OnOrderSpinner(self, idx, idx_minus1) — order quantity spinner (id 0x1388+)
typedef void (*fn_TradeUI_OnOrderSpinner_t)(void* self, int idx, int idx_minus1);
extern fn_TradeUI_OnOrderSpinner_t fn_TradeUI_OnOrderSpinner;  // 0x140081d00

// FUN_140081dd0: OnPurchaseButton(self, idx) — purchase button (id 0x1f40+)
typedef void (*fn_TradeUI_OnPurchaseButton_t)(void* self, int idx);
extern fn_TradeUI_OnPurchaseButton_t fn_TradeUI_OnPurchaseButton; // 0x140081dd0

// FUN_140081bc0: OnComboPurchase(self) — comboPurchase change (id 0x4e22)
extern fn_TownHallUI_SubFunc_t fn_TradeUI_OnComboPurchase;    // 0x140081bc0

// FUN_140081e80: OnComboOrder(self) — comboOrder change (id 0x4e23)
extern fn_TownHallUI_SubFunc_t fn_TradeUI_OnComboOrder;       // 0x140081e80

// FUN_140081ab0: OnStorageSpinner(self, idx, reserved) — storage spinner (id 0x7d0+)
typedef void (*fn_TradeUI_OnStorageSpinner_t)(void* self, int idx, void* reserved);
extern fn_TradeUI_OnStorageSpinner_t fn_TradeUI_OnStorageSpinner; // 0x140081ab0

// FUN_140081a50: OnSellSpinner(self, idx) — sell spinner (id 0xbb8+)
extern fn_TradeUI_OnPurchaseButton_t fn_TradeUI_OnSellSpinner;    // 0x140081a50

// FUN_1400819f0: OnBuySpinner(self, idx) — buy spinner (id 0xfa0+)
extern fn_TradeUI_OnPurchaseButton_t fn_TradeUI_OnBuySpinner;     // 0x1400819f0

// FUN_140081c50: OnOrderAmountSpinner(self, idx) — order amount spinner (id 0x1b58+)
extern fn_TradeUI_OnPurchaseButton_t fn_TradeUI_OnOrderAmountSpinner; // 0x140081c50

// TradeUI::Serialize sub-function type IDs used in IsDerivedFrom checks
// These are the element type IDs used when finding typed elements in Serialize
// _typeInfo_exref used in Serialize for IsDerivedFrom (ComboElement, TableElement, SpinnerElement, etc.)
// TableElement type ID — exref 0x140151510
extern uint32_t* g_TypeId_TableElement;

// SpinnerElement / NumberElement type ID — exref 0x1401516c8
extern uint32_t* g_TypeId_SpinnerElement;

// TextElement type ID — exref 0x140151508
extern uint32_t* g_TypeId_TextElement;

// ButtonElement type ID — exref 0x1401514f8 (same as g_TypeId_HelpButtonElem)
// Already available as g_TypeId_HelpButtonElem

// ComboElement type ID — exref 0x140151690 (same as g_TypeId_HelpComboElem)
// Already available as g_TypeId_HelpComboElem

// SelectDescription type ID — DAT_1401c48a8
// Used in Serialize ExternalFactory loop to count selectable build types
extern uint32_t* g_TypeId_SelectDescription;

// TradeUI::Startup end-sub-functions (called after main calculation)
// FUN_140082fb0: RefreshOrderQuantityDisplay — updates order quantity NumberElements via StateMachine
typedef void (*fn_TradeUI_SubFunc_t)(void* self);
extern fn_TradeUI_SubFunc_t fn_TradeUI_RefreshOrderQuantityDisplay;  // 0x140082fb0
// FUN_140082d80: RefreshSellSpinnerValues — updates sell-order spinners (SetMinMax, SetIncrement)
extern fn_TradeUI_SubFunc_t fn_TradeUI_RefreshSellSpinnerValues;     // 0x140082d80
// FUN_140082b20: RefreshOrderDisplay — updates order table rows (spinner values, labels, sprites)
extern fn_TradeUI_SubFunc_t fn_TradeUI_RefreshOrderDisplay;          // 0x140082b20

// ============================================================
// NoiseUI helpers (Application-x64.exe)
// ============================================================

// NoiseUI ImageBuffer vtable (0x14015c8c0 in exe .rdata) — custom vtable
// set on ImageBuffer after construction. Resolved at runtime.
extern uintptr_t g_NoiseUI_ImageBufferVtable;

// NoiseUI setup helper (0x140071890) — creates MaterialInstance + SpriteSheet
// from ImageBuffer, finds "noiseImage" element, sets sprite.
// Uses "UIMaterialNoBlend.rsc" hardcoded.
// RCX=self(NoiseUI*), RDX=imageBuffer, R8=spriteSheetOut(longlong**), R9=materialOut(void**)
typedef void (*fn_NoiseUI_SetupImage_t)(void* self, void* imageBuffer,
                                        void** spriteSheetOut, void** materialOut);
extern fn_NoiseUI_SetupImage_t fn_NoiseUI_SetupImage;

// NoiseUI noise generation (0x1400711D0) — reads slider values from UI elements,
// generates Perlin noise, renders 256x256 texture to ImageBuffer at self+0x30.
// RCX=self(NoiseUI*)
typedef void (*fn_NoiseUI_GenerateNoise_t)(void* self);
extern fn_NoiseUI_GenerateNoise_t fn_NoiseUI_GenerateNoise;

// ============================================================
// DebugUI support
// ============================================================

// DebugUI_SetDayLighting (FUN_140061780) — reads lighting data from weather table
// for given month index, sets slider values on sun/amb/fog color elements.
// RCX=self(DebugUI*), EDX=monthIndex(int)
typedef void (*fn_DebugUI_SetDayLighting_t)(void* self, int monthIndex);
extern fn_DebugUI_SetDayLighting_t fn_DebugUI_SetDayLighting;

// TypeId for DiseaseDescription — DAT_1401c5948
extern uint32_t* g_TypeId_DiseaseDescription;

// TypeId for DrawableType (used in drawCombo population) — DAT_1401c47c8
extern uint32_t* g_TypeId_DrawableType;

// ============================================================
// DebugUI Slot9 sub-functions (Application-x64.exe)
// ============================================================

// Game_SpawnResources (0x140091660) — spawns resources into citizen storage
// RCX=citizenList*, EDX=resourceFlag(uint), R8D=count(int)
typedef void (*fn_Game_SpawnResources_t)(void* citizenList, uint32_t resourceFlag, int count);
extern fn_Game_SpawnResources_t fn_Game_SpawnResources;

// Game_TriggerDisasterByType (0x140060110) — triggers disaster by type index
// EDX=type(int): 0/1/2
typedef void (*fn_Game_TriggerDisasterByType_t)(int type);
extern fn_Game_TriggerDisasterByType_t fn_Game_TriggerDisasterByType;

// Game_KillAllCitizens (0x140060000) — kills all citizens
typedef void (*fn_Game_KillAllCitizens_t)(void);
extern fn_Game_KillAllCitizens_t fn_Game_KillAllCitizens;

// Game_SpawnItemsInStorage (0x140060290) — spawns items in storage buildings
typedef void (*fn_Game_SpawnItemsInStorage_t)(void);
extern fn_Game_SpawnItemsInStorage_t fn_Game_SpawnItemsInStorage;

// Game_SpawnCitizens (0x140060380) — spawns new citizens at random buildings
typedef void (*fn_Game_SpawnCitizens_t)(void);
extern fn_Game_SpawnCitizens_t fn_Game_SpawnCitizens;

// DebugUI_TriggerDisasterOnBuilding (0x14005FF30) — triggers disaster on random building
// RCX=self(DebugUI*)
typedef void (*fn_DebugUI_TriggerDisasterOnBuilding_t)(void* self);
extern fn_DebugUI_TriggerDisasterOnBuilding_t fn_DebugUI_TriggerDisasterOnBuilding;

// Game_StartRandomFire (0x14005FEB0) — ignites a random building
typedef void (*fn_Game_StartRandomFire_t)(void);
extern fn_Game_StartRandomFire_t fn_Game_StartRandomFire;

// Game_SpawnTornado (0x14005FD20) — spawns a tornado entity
typedef void (*fn_Game_SpawnTornado_t)(void);
extern fn_Game_SpawnTornado_t fn_Game_SpawnTornado;

// Game_InfectRandomCitizen (0x14005FE10) — infects a random citizen with disease
typedef void (*fn_Game_InfectRandomCitizen_t)(void);
extern fn_Game_InfectRandomCitizen_t fn_Game_InfectRandomCitizen;

// DebugUI_OnDiseaseComboChanged (0x14005FC60) — handles disease combo selection
// RCX=self(DebugUI*)
typedef void (*fn_DebugUI_OnDiseaseComboChanged_t)(void* self);
extern fn_DebugUI_OnDiseaseComboChanged_t fn_DebugUI_OnDiseaseComboChanged;

// DebugUI_OnDrawComboChanged (0x1400608A0) — handles draw combo selection (scene/mesh)
// RCX=self(DebugUI*)
typedef void (*fn_DebugUI_OnDrawComboChanged_t)(void* self);
extern fn_DebugUI_OnDrawComboChanged_t fn_DebugUI_OnDrawComboChanged;

// DebugUI_OnDaySliderChanged (0x1400607E0) — handles day slider change, updates time/weather
// RCX=self(DebugUI*)
typedef void (*fn_DebugUI_OnDaySliderChanged_t)(void* self);
extern fn_DebugUI_OnDaySliderChanged_t fn_DebugUI_OnDaySliderChanged;

// DebugUI_WriteLightingColors (0x140061530) — writes color slider values to weather table
// RCX=self(DebugUI*), EDX=monthIndex(int)
typedef void (*fn_DebugUI_WriteLightingColors_t)(void* self, int monthIndex);
extern fn_DebugUI_WriteLightingColors_t fn_DebugUI_WriteLightingColors;

// Game_GetMonthFromTime (0x1400932A0) — converts game time to month index (0-11)
// RCX=weatherMgr(void*)
typedef int (*fn_Game_GetMonthFromTime_t)(void* weatherMgr);
extern fn_Game_GetMonthFromTime_t fn_Game_GetMonthFromTime;

// ============================================================
// SaveLoadDialog::Load — game load loop construction helpers
// ============================================================

// GameUpdateLoop ctor (FUN_140098060 in exe) — constructs the gameplay UpdateLoop (0x160 bytes).
// 11 params: self, savedState, loaderLoopPtr, opFlag, nameStr, p6, p7, p8, p9, p10, p11
// This is an exe-internal function that calls Engine::UpdateLoop::UpdateLoop then sets its vtable
// to PTR_FUN_14015e2f0 and initializes all fields.
typedef void* (*fn_GameUpdateLoop_Ctor_t)(void* self, void* savedState, void* loaderLoopPtr,
                                           uint32_t opFlag, void* nameStr,
                                           uint32_t p6, uint32_t p7, uint8_t p8,
                                           uint64_t p9, uint64_t p10, uint64_t p11);
extern fn_GameUpdateLoop_Ctor_t fn_GameUpdateLoop_Ctor;

// Engine::LoaderLoop ctor (Runtime 0x1800DC1E0)
// RCX=self(LoaderLoop*), RDX=nextLoop(UpdateLoop*), R8=delayTime(Time const&)
// ??0LoaderLoop@Engine@@QEAA@PEAVUpdateLoop@1@AEBVTime@System@@@Z
typedef void* (*fn_LoaderLoop_Ctor_t)(void* self, void* nextLoop, void* delayTime);
extern fn_LoaderLoop_Ctor_t fn_LoaderLoop_Ctor;

// Engine::DialogLoaderLoopT ctor (Runtime 0x1800DB0D0)
// RCX=self, RDX=dialogFilename(String const&), R8=nextLoop(UpdateLoop*), R9=delayTime(Time const&)
// ??0DialogLoaderLoopT@Engine@@QEAA@AEBVString@System@@PEAVUpdateLoop@1@AEBVTime@3@@Z
typedef void* (*fn_DialogLoaderLoopT_Ctor_t)(void* self, void* dialogFilename,
                                              void* nextLoop, void* delayTime);
extern fn_DialogLoaderLoopT_Ctor_t fn_DialogLoaderLoopT_Ctor;

// System::Random::Random() — default ctor (Runtime 0x1800c7f20), seeds from time(nullptr)
// ??0Random@System@@QEAA@XZ
typedef void* (*fn_Random_Default_Ctor_t)(void* self);
extern fn_Random_Default_Ctor_t fn_Random_Default_Ctor;

// Exe vtable pointer for GameLoadDialogLoopT (subclass of DialogLoaderLoopT).
// PTR_FUN_140158800 — written to DialogLoaderLoopT after base ctor to override vtable.
extern void** g_GameLoadDialogLoopT_Vtable;  // 0x140158800

// Exe vtable pointer for GameLoaderLoop (subclass of LoaderLoop).
// PTR_FUN_1401588d0 — written to outer LoaderLoop after base ctor to override vtable.
extern void** g_GameLoaderLoop_Vtable;  // 0x1401588d0

// SetEngineNextLoop — sets next loop on the MainLoop engine singleton
// FUN_140097fb0(enginePtr, loaderLoop): sets UI cursor, fullscreen fade, stores next loop ptr
// Called when self->m_loaderLoop1 (+0xE0) is null (use engine at +0xE8).
typedef void (*fn_SetEngineNextLoop_t)(uintptr_t enginePtr, void* loaderLoop);
extern fn_SetEngineNextLoop_t fn_SetEngineNextLoop;

// SetDialogLoaderNextLoop — sets next loop on a DialogLoaderLoopT
// FUN_14009a8f0(dialogLoaderLoop, loaderLoop): sets fullscreen fade, stores next loop ptr
// Called when self->m_loaderLoop1 (+0xE0) is non-null.
typedef void (*fn_SetDialogLoaderNextLoop_t)(uintptr_t dialogLoaderLoop, void* loaderLoop);
extern fn_SetDialogLoaderNextLoop_t fn_SetDialogLoaderNextLoop;

// GetLoadingSoundString — formats "Dialog/Loading.rsc:gameLoad%i" for a random variant (0-11)
// FUN_140099130(unused, retStr, unused, unused) — ignores all params except retStr
// RDX=retStr(char* buffer of 8 bytes, used as System::String)
typedef void* (*fn_GetLoadingSoundString_t)(uint64_t unused, void* retStr,
                                             uint64_t unused2, uint64_t unused3);
extern fn_GetLoadingSoundString_t fn_GetLoadingSoundString;

// ============================================================
// ModDialog::Slot4 support (Application-x64.exe + Runtime-x64.dll)
// ============================================================

// Graphics::MaterialInstance::MaterialInstance(this) — ctor (0x60 bytes)
// Dispatch table at 0x140151838 in exe
typedef void* (*fn_MaterialInstance_Ctor_t)(void* self);
extern fn_MaterialInstance_Ctor_t fn_MaterialInstance_Ctor;

// Graphics::MaterialInstance::SetMaterial(this, material)
// Dispatch table at 0x140151870
typedef void (*fn_MaterialInstance_SetMaterial_t)(void* self, void* material);
extern fn_MaterialInstance_SetMaterial_t fn_MaterialInstance_SetMaterial;

// Graphics::MaterialInstance::AddTexture(this, name, imageBuffer)
// Dispatch table at 0x140151878
typedef void (*fn_MaterialInstance_AddTexture_t)(void* self, void* nameStr, void* imageBuffer);
extern fn_MaterialInstance_AddTexture_t fn_MaterialInstance_AddTexture;

// Graphics::MaterialInstance::BuildParameters(this)
// Dispatch table at 0x140151880
typedef void (*fn_MaterialInstance_BuildParameters_t)(void* self);
extern fn_MaterialInstance_BuildParameters_t fn_MaterialInstance_BuildParameters;

// UI::SpriteSheet::SpriteSheet(this) — ctor (0x58 bytes)
// Dispatch table at 0x140151848
typedef void* (*fn_SpriteSheet_Ctor_t)(void* self);
extern fn_SpriteSheet_Ctor_t fn_SpriteSheet_Ctor;

// UI::SpriteSheet::AddSpriteInfo(this, name, groupName, x, y, w, h)
// Dispatch table at 0x140151978
typedef void (*fn_SpriteSheet_AddSpriteInfo_t)(void* self, void* nameStr, void* groupStr,
                                                int x, int y, int w, int h);
extern fn_SpriteSheet_AddSpriteInfo_t fn_SpriteSheet_AddSpriteInfo;

// UI::SpriteSheet::SetSpriteInfo(this, index, w, h, u0, v0, u1, v1)
// Dispatch table at 0x140151970
typedef void (*fn_SpriteSheet_SetSpriteInfo_t)(void* self, int index,
                                                int w, int h,
                                                float u0, float v0, float u1, float v1);
extern fn_SpriteSheet_SetSpriteInfo_t fn_SpriteSheet_SetSpriteInfo;

// UI::Sprite::Sprite(retval, SpriteSheet*, int index) — by-index ctor
// Dispatch table at 0x140151898
// Returns retval (16-byte Sprite struct)
typedef void* (*fn_Sprite_Ctor_ByIndex_t)(void* retval, void* spriteSheet, int index);
extern fn_Sprite_Ctor_ByIndex_t fn_Sprite_Ctor_ByIndex;

// Video::ImageBuffer::Width(this) -> int — returns pixel width
// Dispatch table at 0x140151a00 (ordinal 0xD0)
typedef int (*fn_ImageBuffer_Width_t)(void* self);
extern fn_ImageBuffer_Width_t fn_ImageBuffer_Width;

// Video::ImageBuffer::Height(this) -> int — returns pixel height
// Dispatch table at 0x140151a08 (ordinal 0xCF)
typedef int (*fn_ImageBuffer_Height_t)(void* self);
extern fn_ImageBuffer_Height_t fn_ImageBuffer_Height;

// Custom vtables set on per-mod thumbnail objects (exe .rdata)
// ModDialog MaterialInstance vtable (0x14015c400)
extern uintptr_t g_ModDialog_MaterialInstanceVtable;  // 0x14015c400
// ModDialog SpriteSheet vtable (0x14015c3b0)
extern uintptr_t g_ModDialog_SpriteSheetVtable;       // 0x14015c3b0

// ModDialog thumbnail array push (FUN_140012a60) — 0x58-byte stride
// Resizes and copies 0x58-byte entries into the mod cache array.
// RCX=arrayHeader(ptr+count+capacity), RDX=newCount, R8=entry(0x58 bytes)
typedef void (*fn_ModDialog_ArrayPush_t)(void* arrayHeader, int newCount, void* entry);
extern fn_ModDialog_ArrayPush_t fn_ModDialog_ArrayPush;  // 0x140012a60

// Browser controller type ID — DAT_1401c5210
// Used by AddController to attach the mod browser controller.
extern uint32_t* g_TypeId_BrowserController;  // 0x1401c5210

// Material type ID for IsDerivedFrom check in ModDialog (dispatch 0x140151858)
extern uint32_t* g_TypeId_Material;

// DAT_14017214c — UV scale constant (= 1.0f used in SpriteSheet::SetSpriteInfo)
extern float* g_fUVScale;  // DAT_14017214c

// ============================================================
// SpriteCaptureUI support functions
// ============================================================

// Input::Device::GetValue(this, InputDevice, slot, axis) -> short
// ?GetValue@Device@Input@@QEBAFW4InputDevice@12@HH@Z (0x1800e1660, ordinal 3004)
// InputDevice: 1=keyboard, 2=mouse. axis 0=X, 1=Y, 4=left button.
typedef short (*fn_Input_Device_GetValue_t)(void* self, int device, int slot, int axis);
extern fn_Input_Device_GetValue_t fn_Input_Device_GetValue;

// Input::Device::Pressed(this, InputDevice, slot, key) -> bool
// ?Pressed@Device@Input@@QEBA_NW4InputDevice@12@HH@Z (0x1800e16c0, ordinal 3431)
typedef bool (*fn_Input_Device_Pressed_t)(void* self, int device, int slot, int key);
extern fn_Input_Device_Pressed_t fn_Input_Device_Pressed;

// UI::UserInterface::EndSpriteCapture(this) — clears capture state
// ?EndSpriteCapture@UserInterface@UI@@QEAAXXZ (0x18011f4a0, ordinal 2316)
typedef void (*fn_UI_EndSpriteCapture_t)(void* self);
extern fn_UI_EndSpriteCapture_t fn_UI_EndSpriteCapture;

// IO::FileSystem singleton — exref at 0x140151d18 (ordinal 0x161 _singleton)
extern void** g_pFileSystemSingleton;

// IO::FileSystem::GetSourceDirectory(this, retval) — non-trivial return (Filename by value)
// RCX=this(FileSystem*), RDX=retval(Filename*, 8 bytes). Returns retval ptr.
// ?GetSourceDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ (0x180007b90, ordinal 2922)
typedef void* (*fn_FileSystem_GetSourceDirectory_t)(void* self, void* retval);
extern fn_FileSystem_GetSourceDirectory_t fn_FileSystem_GetSourceDirectory;

// Object::External::SaveText(this, path, subdir) -> bool
// ?SaveText@External@Object@@QEAA_NAEBVFilename@System@@0@Z (0x1800bb9d0, ordinal 364)
typedef bool (*fn_External_SaveText_t)(void* self, void* pathFilename, void* subdirFilename);
extern fn_External_SaveText_t fn_External_SaveText;

// System::Filename::Filename(this, String const&) — ctor from System::String
// ??0Filename@System@@QEAA@AEBVString@1@@Z (0x18000c780, ordinal 192)
typedef void* (*fn_Filename_CtorString_t)(void* self, const void* str);
extern fn_Filename_CtorString_t fn_Filename_CtorString;

// System::Filename::Filename(this, const char*) — ctor from C string
// ??0Filename@System@@QEAA@PEBD@Z (0x180026cf0, ordinal 193)
typedef void* (*fn_Filename_CtorCStr_t)(void* self, const char* str);
extern fn_Filename_CtorCStr_t fn_Filename_CtorCStr;

// System::Filename::~Filename(this) — destructor (decrements refcount)
// ??1Filename@System@@QEAA@XZ (0x180006980, ordinal 595)
typedef void (*fn_Filename_Dtor_t)(void* self);
extern fn_Filename_Dtor_t fn_Filename_Dtor;

// System::operator+(retval, String const&, String const&) — non-trivial return
// RCX=retval(String*, 8 bytes), RDX=lhs, R8=rhs. Returns retval ptr.
// Dispatch table exref at 0x140151a10 (ordinal 0x100)
typedef void* (*fn_String_Concat_t)(void* retval, const void* lhs, const void* rhs);
extern fn_String_Concat_t fn_String_ConcatSS;  // String+String variant (dispatch 0x140151a10)
extern fn_String_Concat_t fn_String_ConcatSS2; // String+String variant (dispatch 0x140151a08)

// ============================================================
// Initialization
// ============================================================

// Resolve all function pointers. Call after ResolveInit().
// Returns true if all critical pointers were resolved.
bool EngineFuncsInit();
