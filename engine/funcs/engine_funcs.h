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

// Video::Device::Lock(this) — enters critical section, increments lock count
// Ordinal 3214: ?Lock@Device@Video@@QEAAXXZ
typedef void (*fn_VideoDevice_Lock_t)(void* self);
extern fn_VideoDevice_Lock_t fn_VideoDevice_Lock;

// Video::Device::Unlock(this) — decrements lock count, leaves critical section
// Ordinal 4462: ?Unlock@Device@Video@@QEAAXXZ
typedef void (*fn_VideoDevice_Unlock_t)(void* self);
extern fn_VideoDevice_Unlock_t fn_VideoDevice_Unlock;

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

// String::operator<(String const&) — lexicographic comparison
// Member function: RCX=this, RDX=other, returns bool
typedef bool (*fn_String_LessThan_t)(const void* thisStr, const void* other);
extern fn_String_LessThan_t fn_String_LessThan;

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

// ExternalFactory::Find(this, Filename const&) -> External const*
// Ordinal 2337: ?Find@ExternalFactory@Object@@QEBAPEBVExternal@2@AEBVFilename@System@@@Z
typedef void* (*fn_ExternalFactory_Find_t)(void* self, const void* filename);
extern fn_ExternalFactory_Find_t fn_ExternalFactory_Find;

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

// UI::Element::SetUserData(this, data) — sets *(Base**)(this + 0x80) = data
typedef void (*fn_Element_SetUserData_t)(void* self, void* data);
extern fn_Element_SetUserData_t fn_Element_SetUserData;

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

// SpinnerElement::SetMinMax(this, min, max) — reads +0xa0 field, ONLY for SpinnerElement.
// WARNING: Do NOT call on SliderElement — use fn_NumberEditElement_SetMinMax instead.
// SpinnerElement::SetMinMax dereferences +0xa0 which is NULL on SliderElements → crash.
typedef void (*fn_SpinnerElement_SetMinMax_t)(void* self, int minVal, int maxVal);
extern fn_SpinnerElement_SetMinMax_t fn_SpinnerElement_SetMinMax;

// SpinnerElement::GetValue(this) -> int
typedef int (*fn_SpinnerElement_GetValue_t)(const void* self);
extern fn_SpinnerElement_GetValue_t fn_SpinnerElement_GetValue;

// SpinnerElement::GetMaxValue(this) -> int
typedef int (*fn_SpinnerElement_GetMaxValue_t)(const void* self);
extern fn_SpinnerElement_GetMaxValue_t fn_SpinnerElement_GetMaxValue;

// SpinnerElement::SetIncrement(this, int)
typedef void (*fn_SpinnerElement_SetIncrement_t)(void* self, int increment);
extern fn_SpinnerElement_SetIncrement_t fn_SpinnerElement_SetIncrement;

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

// --- UI::Dialog ---

// Dialog::CreateElement(dialog, retInternalBasePtr, imageExternalRef)
// Dispatch slot 0x1401528c0. Creates a UI element from an external resource ref.
typedef void (*fn_Dialog_CreateElement_t)(void* dialog, void* retIBP, void* imageExternalRef);
extern fn_Dialog_CreateElement_t fn_Dialog_CreateElement;

// --- Input key name helper (exe 0x140099c20) ---

// GetInputKeyName(keyMapContainer, retUIString, deviceType, inputIndex)
// Member function with hidden retval. Container is *g_pKeyMapSingleton.
// Returns ptr to retUIString (16-byte UI::String).
typedef void* (*fn_GetInputKeyName_t)(void* keyMapContainer, void* retUIString, int deviceType, int inputIndex);
extern fn_GetInputKeyName_t fn_GetInputKeyName;

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
extern uint32_t* g_TypeId_RawMaterialDescription;           // DAT_1401c7cb8
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

// BridgeTool-related type IDs
extern uint32_t* g_TypeId_CreateBridgeDescription; // DAT_1401c73f8 (Startup component lookup)

// RemovalTool-related type IDs
extern uint32_t* g_TypeId_RemovalDescription;      // DAT_1401c74a0 (RemovalTool::Startup component lookup)
// g_TypeId_MapComponent = g_TypeId_BuildComponent_Slot4 (DAT_1401c79e0) — already declared above
// g_TypeId_MultiObjectComponent (DAT_1401c7ac0) — already declared below (~line 1448)
extern uint32_t* g_TypeId_ModelComponent;          // DAT_1401c7a50
extern uint32_t* g_TypeId_PickingComponent;        // DAT_1401c7c10

// BridgeTool helper functions (internal to exe)
// BridgeTool_SnapEndCell (0x1400fb710): snaps end cell to same row or column as drag start
// RCX=self(BridgeTool*), EDX=endCell -> returns snapped cell index
typedef int (*fn_BridgeTool_SnapEndCell_t)(void* self, int endCell);
extern fn_BridgeTool_SnapEndCell_t fn_BridgeTool_SnapEndCell;

// BridgeTool_ValidatePath (0x1400fb2d0): validates bridge path between two cells
// RCX=self(BridgeTool*), EDX=startCell, R8D=endCell, R9=outLength(int*) -> returns bool (1=valid)
typedef int (*fn_BridgeTool_ValidatePath_t)(void* self, int startCell, int endCell, int* outLength);
extern fn_BridgeTool_ValidatePath_t fn_BridgeTool_ValidatePath;

// === DragPlacementTool/FencedTool dependencies ===

// DragPlacementTool type IDs
extern uint32_t* g_TypeId_CreatePlacementDescription; // DAT_1401c7430 (DragPlacementTool::Startup)

// FencedTool type IDs
extern uint32_t* g_TypeId_CreateFencedDescription;    // DAT_1401c4100 (FencedTool::Startup)
extern uint32_t* g_TypeId_AreaDescription;            // DAT_1401c7a18 (shared: DragPlacementTool + FencedTool)
extern uint32_t* g_TypeId_FenceDescription;           // DAT_1401c7a88 (FencedTool::Startup)
extern uint32_t* g_TypeId_SizeDescription;            // DAT_1401c7468 (FencedTool::Startup)

// FencedTool::OnKey calls DragTool::OnKey as base class
typedef uint64_t (*fn_DragTool_OnKey_t)(void* self, char key);
extern fn_DragTool_OnKey_t fn_DragTool_OnKey;

// FencedTool::Shutdown calls DragTool::Shutdown (0x1400fc640)
typedef void (*fn_DragTool_Shutdown_t)(void* self);
extern fn_DragTool_Shutdown_t fn_DragTool_Shutdown;

// ============================================================
// Game Singletons (Phase A3 — exe global pointers)
// ============================================================

// Game/World singleton — accessed by ~20 UI methods
// Sub-fields: +0xB0 ProfessionList, +0xF0 CitizenList, +0xF8 WorldGrid,
//             +0x100 ResourceTypeMap, +0x110 RadiusDisplayManager
extern void** g_pGameWorldSingleton;  // DAT_1401c3dd8

// Input::KeyMap singleton — used by HotKeyToolTipUI
extern void** g_pKeyMapSingleton;     // DAT_1401c3de8 (container/manager object, NOT the KeyMap)
extern void** g_pKeyMapObjectExref;   // exref at 0x140151738 — triple deref to get actual Input::KeyMap:
                                      //   ***(void****)g_pKeyMapObjectExref
                                      //   (slot → External ptr → wrapped ptr → KeyMap)
                                      // Use this for fn_KeyMap_GetValue/fn_KeyMap_Pressed calls

// Engine/UpdateLoop singleton — exref at 0x140151440 (double-indirect)
// Sub-fields: +0x10 = engine state (debug counters at +0x2150..+0x215C)
//             +0x58 = total time (Time ticks), +0x68 = frame time (Time ticks)
// Used by DebugUI::Slot8 for performance counter display
extern void** g_pEngineSingleton;     // exref 0x140151440

// ============================================================
// Common Exe Functions (Phase A4 — Reimplemented in clean C++)
// ============================================================
// These were previously exe-resolved function pointers.
// Now implemented as clean C++ using type ID globals + InternalBasePtr.
// No exe address resolution needed.

// Entity::FindComponent — generic implementation (in engine_funcs.cpp).
// Searches entity component array at +0x30 (ptr) / +0x38 (count).
// Calls vtable[2] (+0x10) on each component to get type ID, confirms with IsDerivedFrom.
// Stores result via InternalBasePtr::InternalBasePtr(retval, found).
// REIMPLEMENTED: decompiled from all FindComponent_X variants (identical algorithm).
void Entity_FindComponentByExeTypeId(void* entity, void* retval, uint32_t* typeIdPtr);

// Type ID globals used by FindComponent variants.
// Resolved from exe .rdata (direct DAT_ addresses, not double-indirect exrefs).
// Names reflect which component each FindComponent function actually searches for.
extern uint32_t* g_TypeId_BuildComponent_Slot4;     // DAT_1401c79e0 (fn_FindComponent_Build)
extern uint32_t* g_TypeId_BuildStateComponent;      // DAT_1401c7d98 (fn_FindComponent_BuildState)
extern uint32_t* g_TypeId_StateMachineComponent_FC; // DAT_1401c7350 (fn_FindComponent_StateMachine)
extern uint32_t* g_TypeId_PositionComponent;        // DAT_1401c7008 (fn_FindComponent_Position)
extern uint32_t* g_TypeId_DestroyComponent_FC;      // DAT_1401c75f0 (fn_FindComponent_Destroy)
extern uint32_t* g_TypeId_BuildingComponent;        // DAT_1401c4b80 (fn_FindComponent_Building)
extern uint32_t* g_TypeId_AttendanceComponent;      // DAT_1401c3f40 (fn_FindComponent_Attendance)
extern uint32_t* g_TypeId_PastureComponent;         // DAT_1401c7ba0 (fn_FindComponent_Pasture)
extern uint32_t* g_TypeId_AnimalDescription;        // DAT_1401c6fd0 (used by PastureCalcMaxCount)
extern uint32_t* g_TypeId_OrchardComponent;         // DAT_1401c46b0 (fn_FindComponent_Orchard)
extern uint32_t* g_TypeId_SMComponent;              // DAT_1401c71c8 (fn_FindComponent_SM)
extern uint32_t* g_TypeId_CitizenAIComponent_FC;    // DAT_1401c7158 (fn_FindComponent_CitizenAI)
extern uint32_t* g_TypeId_StorageComponent_FC;      // DAT_1401c7e78 (fn_FindComponent_Storage/Residence)
extern uint32_t* g_TypeId_ResidenceComponent;        // DAT_1401c7cf0
extern uint32_t* g_TypeId_CemeteryComponent;        // DAT_1401c4090 (fn_FindComponent_Cemetery)
extern uint32_t* g_TypeId_TombstoneComponent;       // DAT_1401c4a30
extern uint32_t* g_TypeId_ImmigrationComponent;     // DAT_1401c4480 (fn_FindComponent_Immigration)
extern uint32_t* g_TypeId_TrackerComponent;         // DAT_1401c4b10 (fn_FindComponent_Tracker)
extern uint32_t* g_TypeId_CharacterComponent;       // DAT_1401c7a50 (fn_FindComponent_Character)

// Typedef kept for any existing code that uses fn_FindComponent_t as a type.
typedef void (*fn_FindComponent_t)(void* entity, void* retval);

// FindComponent inline wrappers — call Entity_FindComponentByExeTypeId with correct type ID.
// Original exe functions (FUN_140XXXXXX) are no longer needed.

// fn_FindComponent_Build — DAT_1401c79e0 (BuildComponent), original FUN_140004680
inline void fn_FindComponent_Build(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_BuildComponent_Slot4);
}
// fn_FindComponent_BuildState — DAT_1401c7d98, original FUN_140003ea0
inline void fn_FindComponent_BuildState(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_BuildStateComponent);
}
// fn_FindComponent_StateMachine — DAT_1401c7350, original FUN_14000b6f0
inline void fn_FindComponent_StateMachine(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_StateMachineComponent_FC);
}
// fn_FindComponent_Position — DAT_1401c7008, original FUN_14000d6c0
inline void fn_FindComponent_Position(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_PositionComponent);
}
// fn_FindComponent_Destroy — DAT_1401c75f0, original FUN_140005780
inline void fn_FindComponent_Destroy(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_DestroyComponent_FC);
}
// fn_FindComponent_Building — DAT_1401c4b80, original FUN_140015140
inline void fn_FindComponent_Building(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_BuildingComponent);
}
// fn_FindComponent_Workplace — DAT_1401c8070 (= g_TypeId_StorageLocationComponent), original FUN_140003050
inline void fn_FindComponent_Workplace(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_StorageLocationComponent);
}
// fn_FindComponent_Attendance — DAT_1401c3f40, original FUN_1400105e0
inline void fn_FindComponent_Attendance(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_AttendanceComponent);
}
// fn_FindComponent_Pasture — DAT_1401c7ba0, original FUN_140004fe0
inline void fn_FindComponent_Pasture(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_PastureComponent);
}
// fn_FindComponent_Orchard — DAT_1401c46b0, original FUN_140005440
inline void fn_FindComponent_Orchard(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_OrchardComponent);
}
// fn_FindComponent_BuildState2 — DAT_1401c80e0 (= g_TypeId_WorkplaceComponent), original FUN_140002e10
inline void fn_FindComponent_BuildState2(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_WorkplaceComponent);
}
// fn_FindComponent_SM — DAT_1401c71c8, original FUN_14000a170
inline void fn_FindComponent_SM(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_SMComponent);
}
// fn_FindComponent_CitizenAI — DAT_1401c7158, original FUN_140009790
inline void fn_FindComponent_CitizenAI(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_CitizenAIComponent_FC);
}
// fn_FindComponent_Storage — DAT_1401c7e78, original FUN_140006270
// NOTE: same function as fn_FindComponent_Residence (same type ID search)
inline void fn_FindComponent_Storage(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_StorageComponent_FC);
}
// fn_FindComponent_Residence — same as fn_FindComponent_Storage (same type ID)
inline void fn_FindComponent_Residence(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_StorageComponent_FC);
}
// fn_FindComponent_Cemetery — DAT_1401c4090, original FUN_140010a00
inline void fn_FindComponent_Cemetery(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_CemeteryComponent);
}
// fn_FindComponent_Immigration — DAT_1401c4480, original FUN_140012020
inline void fn_FindComponent_Immigration(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_ImmigrationComponent);
}
// fn_FindComponent_Tracker — DAT_1401c4b10, original FUN_140013320
inline void fn_FindComponent_Tracker(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_TrackerComponent);
}
// Forward declarations needed by inline helpers below (defined later in this file)
extern uint32_t* g_TypeId_HappinessComponent;
extern uint32_t* g_TypeId_ProfessionComponent;
// fn_FindComponent_MeshInstance — DAT_1401c7e08 (= g_TypeId_HappinessComponent), original FUN_140002f90
inline void fn_FindComponent_MeshInstance(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_HappinessComponent);
}
// fn_FindComponent_Character — DAT_1401c7a50, original FUN_140003f60
inline void fn_FindComponent_Character(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_CharacterComponent);
}
// fn_FindProfessionComponent — DAT_1401c70e8 (= g_TypeId_ProfessionComponent), original FUN_140011050
inline void fn_FindProfessionComponent(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_ProfessionComponent);
}

// ============================================================
// Dynamic array helpers — Reimplemented in clean C++
// (previously resolved from Application-x64.exe)
// All use fn_MemAlloc / fn_MemFreeStatic from Runtime-x64.dll.
// Array header layout: [0]=ptr(void*), [1..low32]=count(int), [0xc..low32]=capacity(int)
// ============================================================

// ExeArrayResize: 8-byte elements. Zeroes new entries, copies existing.
// Capacity grows 3x when adding one beyond current capacity.
// Original: FUN_1400032e0 (6 uses)
void fn_ExeArrayResize(void* arrayBase, int newCount);

// ExeArrayResize2: 4-byte elements. Copies existing only (no zero-init for new).
// Original: FUN_1400021d0 (2 uses)
void fn_ExeArrayResize2(void* arrayBase, int newCount);

// ExeArrayResize3: 8-byte elements. Copies existing, zeroes new entries.
// Original: FUN_140001bb0 (1 use)
void fn_ExeArrayResize3(void* arrayBase, int newCount);

// ExeArrayPush: 4-byte elements. Resizes to newCount, fills new slot with *elementPtr.
// Original: FUN_140003600 (3 uses)
void fn_ExeArrayPush(void* arrayBase, int newCount, void* elementPtr);

// StorageComponent::QueryCount(storageComp) -> int
// FUN_14012cf10: iterates +0x28 item array (count at +0x30). Each slot is 0x10 bytes:
//   [0..7] = ptr to "item" struct (has +0x28=component ptr array, +0x30=comp count)
//   [8..11] = item quantity count
// For each item: scan its component array for ResourceType (type ID check at vtable+0x10,
// then IsDerivedFrom). Accumulate ResourceType.quantity(+0x48) * item.count.
inline int fn_StorageComponent_QueryCount(void* storageComp) {
    uintptr_t sc = (uintptr_t)storageComp;
    int slotCount = *(int*)(sc + 0x30);
    int total = 0;
    uintptr_t itemArray = *(uintptr_t*)(sc + 0x28);
    for (int i = 0; i < slotCount; i++) {
        uintptr_t slotBase = itemArray + (uintptr_t)i * 0x10;
        uintptr_t item = *(uintptr_t*)slotBase;
        int itemQty = *(int*)(slotBase + 0x8);
        // Scan item's component array for ResourceType
        int compCount = *(int*)(item + 0x30);
        uintptr_t compArray = *(uintptr_t*)(item + 0x28);
        void* resourceType = nullptr;
        for (int j = 0; j < compCount; j++) {
            void* comp = *(void**)(compArray + (uintptr_t)j * 8);
            // GetTypeId via vtable slot +0x10
            typedef uint32_t (*GetTypeId_t)(void*);
            uint32_t typeId = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
            if (typeId == *g_TypeId_RawMaterialDescription) {
                if (fn_IsDerivedFrom(comp, *g_TypeId_RawMaterialDescription)) {
                    resourceType = comp;
                }
                break;
            }
        }
        if (resourceType) {
            int quantity = *(int*)((uintptr_t)resourceType + 0x48);
            total += quantity * itemQty;
        }
    }
    return total;
}

// Radius calculation helper (returns radius display index)
typedef int (*fn_RadiusCalc_t)(void* component, int tileIndex);
extern fn_RadiusCalc_t fn_RadiusCalc;           // FUN_140055430

// RadiusDisplayManager::Remove(radiusMgr, index)
typedef void (*fn_RadiusDisplay_Remove_t)(void* radiusMgr, int index);
extern fn_RadiusDisplay_Remove_t fn_RadiusDisplay_Remove;  // FUN_1400b0d00

// Get active product index from ConsumeProduce component
typedef int (*fn_GetActiveProductIndex_t)(void* component);
extern fn_GetActiveProductIndex_t fn_GetActiveProductIndex; // FUN_140119470

// fn_StorageCountQuery / fn_CalcWorkerSlots — inline defined later (after fn_InternalBasePtr_CheckPtr)
// FUN_14012e5c0: Calculate worker slot count for a StorageLocationComponent/WorkplaceComponent.

// CropUI helper functions — REIMPLEMENTED in code/Game/Dialog/cropui.cpp
// typedef void (*fn_CropUIPopulate_t)(void* cropUI);
// extern fn_CropUIPopulate_t fn_CropUI_PopulateCombo;        // FUN_140078830
// typedef int (*fn_CropUIExpected_t)(void* cropUI);
// extern fn_CropUIExpected_t fn_CropUI_GetExpectedCount;     // FUN_140078a40

// CemeteryGetBurialCount — now inline in cemeteryui.cpp (CemeteryGetBurialCount_Impl)

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

// UI::ImageElement type ID at exref 0x140151500 (used by EventNotifyDialog::Update,
// ScenarioDialog, and many other dialog Update functions to check child element types)
extern uint32_t* g_TypeId_ImageElement;

// ============================================================
// Additional Exe Type IDs (Phase D additions)
// ============================================================

// ControlComponent type ID at DAT_1401c4e20 (direct)
extern uint32_t* g_TypeId_ControlComponent;

// ============================================================
// Additional Exe Functions (Phase D additions)
// ============================================================

// FindComponentByTypeId(entity, typeId) — searches entity component array
// Reimplemented from FUN_1400c3680: iterates entity+0x30 array (count at +0x38),
// calls GetTypeId() via vtable[2] (offset 0x10) on each component, returns first match.
// Original used hidden return pointer (InternalBasePtr); this inline returns void* directly.
inline void* FindComponentByTypeId(void* entity, int typeId) {
    if (!entity) return nullptr;
    uintptr_t e = (uintptr_t)entity;
    int count = *(int*)(e + 0x38);
    uintptr_t arr = *(uintptr_t*)(e + 0x30);  // array of InternalBasePtr (8 bytes each)
    for (int i = 0; i < count; i++) {
        void* compPtr = *(void**)(arr + (uintptr_t)i * 8);  // InternalBasePtr value = Internal*
        if (!compPtr) continue;
        // CheckPtr validates the pointer; for our purposes we just null-check above.
        // GetTypeId: vtable[2] at offset 0x10
        void** vtable = *(void***)compPtr;
        typedef int (*GetTypeId_t)(void*);
        int id = ((GetTypeId_t)vtable[2])(compPtr);
        if (id == typeId) {
            return compPtr;
        }
    }
    return nullptr;
}

// EntityTrackingSet_BST_Delete(root, node) — AVL/BST node deletion helper
// FUN_140025740: called by EntityTrackingSet_Remove to delete found node
typedef void (*fn_EntityTrackingSet_BST_Delete_t)(void* root, void* node);
extern fn_EntityTrackingSet_BST_Delete_t fn_EntityTrackingSet_BST_Delete;

// WorkplaceComponent_Toggle(comp, newState) — toggles workplace enabled/disabled
// FUN_14012fc00: 166-line function, called as opaque function pointer
typedef void (*fn_WorkplaceComponent_Toggle_t)(void* comp, bool newState);
extern fn_WorkplaceComponent_Toggle_t fn_WorkplaceComponent_Toggle;

// OrchardComponent_GetGrowth(comp) — returns average growth ratio as float [0.0, 1.0]
// FUN_140051640: iterates orchard slots, averages growth values
typedef float (*fn_OrchardGetGrowth_t)(void* comp);
extern fn_OrchardGetGrowth_t fn_OrchardGetGrowth;

// PastureCalcMaxCount, PastureUI_UpdateSlider — now inline in pastureui.cpp

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

// SaveLoad::~SaveLoad(this) — destructor (clears _singleton, releases refs)
// ??1SaveLoad@Save@@QEAA@XZ (0x1800c3c90, ordinal 666)
typedef void (*fn_SaveLoad_Dtor_t)(void* saveLoad);
extern fn_SaveLoad_Dtor_t fn_SaveLoad_Dtor;

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

// SaveLoadDialog helper: FUN_1400b6ce0 — populates the file list table
// NOW REIMPLEMENTED in code/Game/Dialog/saveloaddialog.cpp as SaveLoadDialog_PopulateFileList()
// Old typedef kept for reference:
// typedef void (*fn_SaveLoadDialog_PopulateFileList_t)(void* self);
// extern fn_SaveLoadDialog_PopulateFileList_t fn_SaveLoadDialog_PopulateFileList;
struct SaveLoadDialog;
void SaveLoadDialog_PopulateFileList(SaveLoadDialog* self);

// TextElement::SetText(this, UI::String*) — sets text element text (takes UI::String, 16 bytes)
// ?SetText@TextElement@UI@@QEAAXVString@2@@Z (0x180042070)
typedef void (*fn_EditElement_SetText_t)(void* self, void* str);
extern fn_EditElement_SetText_t fn_EditElement_SetText;

// EditElement::SetText(this, System::String const&) — sets edit field text (takes System::String, 8 bytes)
// ?SetText@EditElement@UI@@QEAAXAEBVString@System@@@Z (0x1800d0db0)
typedef void (*fn_EditElement_SetText_SysStr_t)(void* self, void* sysStr);
extern fn_EditElement_SetText_SysStr_t fn_EditElement_SetText_SysStr;

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

// LabelElement::GetSprite — returns Sprite via hidden return pointer
// Address: 0x1800429d0, Ordinal: 2927
typedef void (*fn_LabelElement_GetSprite_t)(void* self, void* retval);
extern fn_LabelElement_GetSprite_t fn_LabelElement_GetSprite;

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

// Audio::Scene::Update(this, Time, Time, Random&, Vector3 const&, float) -> bool
// ?Update@Scene@Audio@@QEAA_NVTime@System@@0AEAVRandom@4@AEBVVector3@Math@@M@Z
// Runtime 0x1800df6c0. Per-frame tick that advances the scene's audio channels
// (music, ambient, 3D emitters). MenuDLT::Update (exe 0x14009b6e0) calls it with
//   (scene@+0xb30, elapsedTime, Time(0), &Random@+0x168, &listenerVec3{0,0,0}, 0.0f)
// when the app is not paused. The two Time args are 8-byte System::Time values
// passed by register value; Random/Vector3 args are passed by pointer.
typedef bool (*fn_Audio_Scene_Update_t)(void* scene, int64_t time1, int64_t time2,
                                        void* rng, const void* listenerVec3, float f);
extern fn_Audio_Scene_Update_t fn_Audio_Scene_Update;

// ScenarioDialog helper: loads the selected scenario — NOW REIMPLEMENTED
// fn_ScenarioDialog_LoadSelected replaced by inline code in scenariodialog.cpp

// SaveLoad::Update(this, Time*) — advance save/load operation
// ?Update@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z
typedef void (*fn_SaveLoad_Update_t)(void* saveLoad, void* time);
extern fn_SaveLoad_Update_t fn_SaveLoad_Update;

// IO::Persistence::GetSavePath(this, retval) -> Filename (non-trivial return ABI)
// ?GetSavePath@Persistence@IO@@QEAA?AVFilename@System@@XZ
// MSVC x64 member: RCX=this, RDX=hidden_retval (Filename buffer)
typedef void* (*fn_Persistence_GetSavePath_t)(void* self, void* retval);
extern fn_Persistence_GetSavePath_t fn_Persistence_GetSavePath;

// IO::FileSystem::GetDataDirectory(this, retval) -> String (non-trivial return ABI)
// ?GetDataDirectory@FileSystem@IO@@QEBAAEBVString@System@@XZ
// MSVC x64 member: RCX=this, RDX=hidden_retval (String buffer)
typedef void* (*fn_FileSystem_GetDataDirectory_t)(void* self, void* retval);
extern fn_FileSystem_GetDataDirectory_t fn_FileSystem_GetDataDirectory;

// IO::FileSystem singleton
extern void** g_pFileSystemSingleton2;  // separate exref for GetDataDirectory calls

// LoadScenarioFromSave: FUN_1400b49f0(worldSetup, savedState, scenarioIdx, retvalPtr)
// Called from ScenarioDialog_LoadSelected when loading a saved scenario.
// Constructs GameUpdateLoop with saved state, then GameLoadDialogLoopT, then LoaderLoop.
typedef void (*fn_LoadScenarioFromSave_t)(void* worldSetup, void* savedState,
                                           void* scenarioIdx, void* retvalPtr);
extern fn_LoadScenarioFromSave_t fn_LoadScenarioFromSave;

// ModDetailsDialog helpers
// REPLACED: now inline ModDetailsDialog_OpenUpdateDialog_Impl() in moddetailsdialog.cpp
// REPLACED: now inline ModDetailsDialog_DeleteMod_Impl() in moddetailsdialog.cpp

// ModUpdateDialog vtable pointer (PTR_FUN_14015c6c0 in exe .rdata)
extern uintptr_t g_ModUpdateDialog_Vtable;

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

// Input::KeyMap::Pressed(this, slot, keyId) -> bool
// Resolved from exe dispatch table at 0x140151758
typedef bool (*fn_KeyMap_Pressed_t)(void* self, int slot, int keyId);
extern fn_KeyMap_Pressed_t fn_KeyMap_Pressed;

// fn_FollowDialog_Close — now inline in followdialog.cpp Slot7 (Escape key handler)

// ============================================================
// UpgradeUI helpers
// ============================================================

// UpgradeDescription type ID at DAT_1401c5868
extern uint32_t* g_TypeId_UpgradeDescription;

// DestroyComponent_StartUpgrade — drives state machine through upgrade transition
// NOW REIMPLEMENTED in code/Game/Dialog/destroyui.cpp as DestroyComponent_StartUpgrade_Impl()
// Old typedef kept for reference:
// typedef void (*fn_DestroyComponent_StartUpgrade_t)(void* destroyComp);
// extern fn_DestroyComponent_StartUpgrade_t fn_DestroyComponent_StartUpgrade;
void DestroyComponent_StartUpgrade_Impl(void* destroyComp);

// ============================================================
// ResidenceUI helpers
// ============================================================

// fn_ResidenceUI_GoToCitizen — now inline in residenceui.cpp ResidenceUI_GoToCitizen

// ResidenceUI array push — now inline as ResidenceUI_ArrayPush_Impl in residenceui.cpp

// ============================================================
// WorkplaceUI helpers
// ============================================================

// WorkplaceComp_SetMaxWorkers(workplaceComp, newMax) — sets max worker count and evicts excess
// FUN_14012fb00: called by WorkplaceUI_AdjustWorkerCount
typedef void (*fn_WorkplaceComp_SetMaxWorkers_t)(void* workplaceComp, int newMax);
extern fn_WorkplaceComp_SetMaxWorkers_t fn_WorkplaceComp_SetMaxWorkers;

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

// EditTool type ID at g_TypeId_EditTool
extern uint32_t* g_TypeId_EditTool;  // resolved from _typeInfo_exref for EditTool checks

// ============================================================
// EntityUI helpers
// ============================================================

// fn_EntityUI_SetEditMode — reimplemented in entityui.cpp as EntityUI_SetEditMode_Impl
// FUN_140102b60: sets player controller edit state (BST lookup + Factory::CreateObject)
void EntityUI_SetEditMode_Impl(void* playerController, uint64_t mode);

// fn_EntityUI_SelectEntity — reimplemented in entityui.cpp as EntityUI_SelectEntity_Impl
// FUN_140102e30: BST insert/update for entity tracking
void EntityUI_SelectEntity_Impl(void* playerController, void* entityPtr,
                                 uint32_t x, uint32_t y);

// Factory::CreateObject(factory_singleton, typeId) -> Base*
// Used by SetEditMode to create tool controller objects.
// Ordinal for Object::Factory::CreateObject
typedef void* (*fn_Factory_CreateObject_t)(void* factory, uint32_t typeId);
extern fn_Factory_CreateObject_t fn_Factory_CreateObject;

// ToolController type ID — DAT_1401c6eb8 (IsDerivedFrom check on Factory::CreateObject result)
extern uint32_t* g_TypeId_ToolController;

// ToolSetting type ID — TypeInfo_ToolSetting (used to IsDerivedFrom check on created setting)
extern uint32_t* g_TypeId_ToolSetting;

// AVL tree rebalance for 0x38-byte nodes (entity tracking BST)
// FUN_140025920: walks up tree fixing balance factors, does rotations
typedef void (*fn_AVL_Rebalance_0x38_t)(void* treeRoot, void* newNode);
extern fn_AVL_Rebalance_0x38_t fn_AVL_Rebalance_0x38;

// EditTool_GetSelected(editTool, retval) — gets currently selected entity
// vtable[13] (offset 0x68): virtual call on EditTool, returns InternalPtr<ComponentEntity>
// Used by EntityUI::Slot7 to check if current entity is selected

// EventLogUI_ToggleNotification — now inline in eventlogui.cpp Slot9
// EventLogUI_NavigateToEvent — now inline in eventlogui.cpp Slot9

// ============================================================
// Phase F Wave 1: MapParamsDialog / UpgradeUI / ConsumeProduceUI / HelpUI
// ============================================================

// fn_MapParamsDialog_OnOk — now inline in mapparamsdialog.cpp MapParamsDialog_OnEvent (eventData==1)
// fn_MapParamsDialog_OnNewName — now inline in mapparamsdialog.cpp MapParamsDialog_OnEvent (eventData==0)
// fn_GameWorld_GetMapName — now inline where needed (direct access to gameWorld+0x28)
// fn_UpgradeUI_Serialize — now inline as UpgradeUI_Serialize in upgradeui.cpp
// fn_UpgradeUI_OnRemove — now inline as UpgradeUI_OnRemove in upgradeui.cpp

// ConsumeProduceUI::Slot4 — combo setup with resource sprites
// 0x14005F3D0
typedef void (*fn_ConsumeProduceUI_Slot4_t)(void* self);
extern fn_ConsumeProduceUI_Slot4_t fn_ConsumeProduceUI_Slot4;

// HelpUI_Slot4 — now inline as HelpUI_Slot4 in helpui.cpp
// HelpUI_Slot9 — now inline as HelpUI_Slot9 in helpui.cpp

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

// GoalDialog_ArrayPush — now inline as GoalDialog_ArrayPush_Impl in goaldialog.cpp
// EventNotifyDialog_ArrayPush — now inline as EventNotifyDialog_ArrayPush_Impl in eventnotifydialog.cpp
// ScenarioDialog_ArrayPush — now inline as ScenarioDialog_ArrayPush_Impl in scenariodialog.cpp

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

// fn_EventNotifyDialog_ArrayRemove — now inline as EventNotifyDialog_ArrayRemove_Impl in eventnotifydialog.cpp

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
// FULLY REIMPLEMENTED: ToolbarDialog_Ctor_Impl() in code/Game/Dialog/toolbardialog.cpp
// Creates a ToolbarDialog from parent + toolbar data, populates buttons/sprites/shortcuts.
// The fn_ToolbarDialog_Ctor pointer is kept for the initial toolbar creation call from
// FUN_140097c00 (which is not yet reimplemented). Sub-dialog creation in OnEvent now
// calls ToolbarDialog_Ctor_Impl directly.
typedef void* (*fn_ToolbarDialog_Ctor_t)(void* self, void* userInterface, void* dialog,
                                         void* parentPtr, void* toolbarData);
extern fn_ToolbarDialog_Ctor_t fn_ToolbarDialog_Ctor;

// ToolbarDialog game system dispatch functions (called from OnEvent)
// Case 1: ShowGroup — toggle visibility of a game group
// Must call ORIGINAL exe function (not our reimpl) because 15+ other callers
// (EntityUI, CitizenComponent, TradeWork, etc.) call FUN_140102b60 directly.
// Using our reimpl causes state desync (pc+0x20/0x28 overwritten by original callers).
typedef void (*fn_Toolbar_ShowGroup_t)(void* gameSubsys, int64_t groupId);
extern fn_Toolbar_ShowGroup_t fn_Toolbar_ShowGroup;

// Case 2: Utility action
// REPLACED: now Toolbar_Utility_Impl() in code/Game/Dialog/toolbardialog.cpp
typedef void (*fn_Toolbar_Utility_t)(void* gameSubsys, void* retBuf, uint64_t param);
extern fn_Toolbar_Utility_t fn_Toolbar_Utility;

// Case 3: Toggle all
// FULLY REIMPLEMENTED: Toolbar_ToggleAll_Impl() in code/Game/Dialog/toolbardialog.cpp
// Kept as fallback only — fn_Toolbar_ToggleAll is no longer called by Toolbar_ToggleAll_Impl.
typedef void (*fn_Toolbar_ToggleAll_t)(void* gameSubsys);
extern fn_Toolbar_ToggleAll_t fn_Toolbar_ToggleAll;

// ToolbarDialog vtable pointer (PTR_FUN_14015fa00 in exe .rdata)
extern uintptr_t g_ToolbarDialog_Vtable;

// PauseDialog vtable pointer (PTR_FUN_14015f550 in exe .rdata)
extern uintptr_t g_PauseDialog_Vtable;

// ToolUI child dialog vtable (PTR_FUN_14015f5a0 in exe .rdata) — used by Toolbar_Utility
extern uintptr_t g_ToolUI_ChildDialog_Vtable;

// fn_Factory_CreateObject, g_TypeId_ToolController, g_TypeId_ToolSetting — declared above (entityui section)

// Toolbar item sort function (FUN_140021790) — sorts toolbar items by priority
typedef void (*fn_ToolbarSort_t)(void* array, int count);
extern fn_ToolbarSort_t fn_ToolbarSort;

// TypeInfo for Toolbar external — used by ExternalFactory::FindNext
extern uint32_t* g_TypeId_ToolbarExternal;

// Toolbar type ID at DAT_1401c6128 — used by ExternalFactory::FindNext and IsDerivedFrom
// in ToolbarDialog_Ctor to iterate all Toolbar items matching a group
extern uint32_t* g_TypeId_ToolbarItem;

// RadioElement::FindCheckBoxes(this) — scans children for checkboxes and registers them
// ?FindCheckBoxes@RadioElement@UI@@QEAAXXZ (0x1800d5750)
typedef void (*fn_RadioElement_FindCheckBoxes_t)(void* self);
extern fn_RadioElement_FindCheckBoxes_t fn_RadioElement_FindCheckBoxes;

// Sprite constructor taking spriteSheet + spriteName (same as fn_Sprite_Ctor but used in toolbar)
// Already available as fn_Sprite_Ctor

// FUN_140100270: Find open dialog by controller type matching group ID
// Searches all open dialogs for one whose controller matches a specific type
// and whose +0x30 field equals the given param. Returns InternalBasePtr.
typedef void (*fn_FindDialogByControllerType_t)(void* toolController, void* retBuf, uint64_t param);
extern fn_FindDialogByControllerType_t fn_FindDialogByControllerType;

// fn_Toolbar_FindEntity — now inline as Toolbar_FindEntity_Impl in toolbardialog.cpp
// fn_Toolbar_CenterOnEntity (FUN_140103040) — now inline as Toolbar_CenterOnEntity_Impl below

// --- Sub-dependencies for Toolbar_CenterOnEntity_Impl ---

// MultiObjectComponent type ID — DAT_1401c7ac0 (multi-tile buildings: Cemetery, Pasture, Tunnel, Bridge)
extern uint32_t* g_TypeId_MultiObjectComponent;

// FindComponent_MultiObject — inline wrapper for MultiObjectComponent lookup
inline void fn_FindComponent_MultiObject(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_MultiObjectComponent);
}

// EntityPanel_SyncTracking (FUN_1400666b0) — syncs tracking/follow flag to dialog controller
typedef void (*fn_EntityPanel_SyncTracking_t)(void* panelController);
extern fn_EntityPanel_SyncTracking_t fn_EntityPanel_SyncTracking;

// CameraSys_ComputeViewTransform (FUN_1400983f0) — computes camera look-at transform
// Reads target position from cameraSys+0x1C, yaw at +0x28, distance at +0x30.
// Writes compact Transform (7 dwords = 0x1C bytes) to output buffer and cameraSys[0..6].
typedef void* (*fn_CameraSys_ComputeViewTransform_t)(void* cameraSys, void* outTransform);
extern fn_CameraSys_ComputeViewTransform_t fn_CameraSys_ComputeViewTransform;

// Math::Transform::AsTransform3 — converts compact Transform (0x1C bytes) to Transform3 (48 bytes)
// Non-trivial return: RCX=this(Transform*), RDX=retval(Transform3*)
// Runtime export: ?AsTransform3@Transform@Math@@QEBA?AVTransform3@2@XZ
typedef void* (*fn_Transform_AsTransform3_t)(void* self, void* retval);
extern fn_Transform_AsTransform3_t fn_Transform_AsTransform3;

// Graphics::Camera::SetCameraToWorld — sets camera world transform from Transform3
// Runtime export: ?SetCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z
typedef void (*fn_Camera_SetCameraToWorld_t)(void* camera, void* transform3);
extern fn_Camera_SetCameraToWorld_t fn_Camera_SetCameraToWorld;

// Toolbar_CenterOnEntity_Impl — defined at end of file (needs all declarations above it)

// fn_ToolbarDialog_CloseSubDialog — now inline as ToolbarDialog_CloseSubDialog_Impl in toolbardialog.cpp
// fn_ToolbarDialog_ChangeSpeed — now inline as ToolbarDialog_ChangeSpeed_Impl in toolbardialog.cpp

// ToolbarDialog update button states helper (FUN_1400b8fd0)
// REPLACED: now ToolbarDialog_UpdateButtons_Impl() in code/Game/Dialog/toolbardialog.cpp

// ToolbarDialog exref type — for IsDerivedFrom checks on toolbar elements
extern uint32_t* g_TypeId_ToolbarElement;  // _typeInfo_exref in ToolbarDialog Update/OnEvent

// ToolbarDialog parent type ID (DAT_1401c5d38) — for IsDerivedFrom in case 5 (CenterOnPoint)
extern uint32_t* g_TypeId_ToolbarDialogParent;

// ComboElement type ID: exref at 0x140151690 — for IsDerivedFrom checks on combo elements
extern uint32_t* g_TypeId_ComboElement;

// fn_GetCurrentProductIndex — inline defined later (after fn_Random_RandFloat)
// FUN_140119470: if +0x50 != 0xffffffff return it; else weighted random pick from +0x58 array.

// FindDialogByName (FUN_1400e1600 in exe) — REPLACED with FindDialogByName_Impl (Phase H)
// See inline FindDialogByName_Impl() near end of this file.
// Old fn_* pointer kept for backward compat; callers should use FindDialogByName_Impl().
typedef void* (*fn_FindDialogByName_t)(void* retval, void* name);
extern fn_FindDialogByName_t fn_FindDialogByName;

// Forward declarations needed by ResolveExternalByName_Impl below
typedef void* (*fn_Filename_CtorString_t)(void* self, const void* str);
extern fn_Filename_CtorString_t fn_Filename_CtorString;
typedef void (*fn_Filename_Dtor_t)(void* self);
extern fn_Filename_Dtor_t fn_Filename_Dtor;

// ResolveExternalByName (FUN_14001b1d0 in exe) — REPLACED with inline C++
// Resolves and caches an External object by filename.
// Input: nameString points to an ExternalRef struct:
//   +0x00: String (8 bytes) — resource name
//   +0x08: int32  — cached generation counter (compared against ExternalFactory+0x08)
//   +0x10: void*  — cached External* result
// If the generation counter matches the factory's, the cached result is still valid.
// Otherwise, looks up the External by converting the String to a Filename and calling
// ExternalFactory::Find. If found and passes IsDerivedFrom(g_TypeId_BuildComponent),
// caches the result; otherwise caches nullptr.
typedef void (*fn_ResolveExternalByName_t)(void* nameString); // kept for ABI reference
inline void ResolveExternalByName_Impl(void* nameString) {
    uintptr_t s = (uintptr_t)nameString;
    void* factory = *g_pExternalFactorySingleton;
    if (!factory) return;

    // Check generation counter: if matches, cached value is still valid
    int32_t refGen     = *(int32_t*)(s + 0x08);
    int32_t factoryGen = *(int32_t*)((uintptr_t)factory + 0x08);
    if (refGen == factoryGen) return;

    // Convert String to Filename, look up in factory
    uint8_t filename[8] = {};
    fn_Filename_CtorString(filename, (const void*)s);
    void* found = fn_ExternalFactory_Find(factory, filename);
    fn_Filename_Dtor(filename);

    // Validate the found object is the expected type (Description base).
    // In standalone the exe-side type-id storage (g_TypeId_BuildComponent ->
    // DAT_1401c7318) is unregistered, so the global is NULL — dereferencing it
    // would fault. Per the factory-cascade strategy ([[feedback_factory_cascade_strategy]])
    // trust the FindOrLoad/Find result when the type-id check can't run.
    void* result = nullptr;
    if (found) {
        if (!g_TypeId_BuildComponent || fn_IsDerivedFrom(found, *g_TypeId_BuildComponent)) {
            result = found;
        }
    }

    // Cache result and update generation
    *(void**)(s + 0x10)  = result;
    *(int32_t*)(s + 0x08) = factoryGen;
}

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
// Age multiplier constant (DAT_140172198 = 5.0f) — converts game age ticks to display years
extern float* g_pAgeMultiplier;

// Notification timer duration constant (DAT_1401721cc = 30.0f) — seconds per notification
extern float* g_pNotifyTimerDuration;

// System::Time::Time(retval, float seconds) — construct Time from float seconds
// ??0Time@System@@QEAA@M@Z
typedef void* (*fn_Time_Ctor_Float_t)(void* retval, float seconds);
extern fn_Time_Ctor_Float_t fn_Time_Ctor_Float;

// System::String::SubString(this, retval, startIndex, endIndex) -> String (member, non-trivial return)
// RCX=this, RDX=retval(String*), R8=startIndex, R9=endIndex
// ?SubString@String@System@@QEBA?AV12@HH@Z
typedef void* (*fn_String_SubString_t)(void* self, void* retval, int startIndex, int endIndex);
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

// UI::String::String(retval) — default ctor, produces empty UI::String
// Non-trivial return: RCX=retval(UIString*, 16 bytes)
// Resolved from exe dispatch table at 0x140151ca8.
typedef void* (*fn_UIString_DefaultCtor_t)(void* retval);
extern fn_UIString_DefaultCtor_t fn_UIString_DefaultCtor;

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

// EventNotifyDialog_ArrayPush — now inline as EventNotifyDialog_ArrayPush_Impl in eventnotifydialog.cpp

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

// HelpUI_ArrayResize — now inline as HelpUI_ArrayResize_Impl in helpui.cpp

// ============================================================
// BuildUI support functions
// ============================================================

// ============================================================
// CitizenUI helpers
// ============================================================

// FindComponent_CitizenAI — finds CitizenAIComponent on entity
// FUN_140009790: entity(longlong), retval(InternalBasePtr*)
// CitizenDescription type ID at DAT_1401c7190 (used for IsDerivedFrom in Slot7)
extern uint32_t* g_TypeId_CitizenDescription;  // DAT_1401c7190

// HappinessComponent type ID at DAT_1401c7e08 (used for FindComponent in StatusBarUI)
extern uint32_t* g_TypeId_HappinessComponent;  // DAT_1401c7e08

// ProfessionComponent type ID at DAT_1401c70e8 (used by FindProfessionComponent)
extern uint32_t* g_TypeId_ProfessionComponent;  // DAT_1401c70e8
extern uint32_t* g_TypeId_ProfessionDescription;  // DAT_1401c5a98 (profession type in TutorialGoalCount)

// FollowDialog vtable pointer (resolved from 0x14015be00)
// Used by CitizenUI_Slot9 to manually construct FollowDialog instances.
extern void** g_FollowDialog_Vtable;

// StatusBarUI_StatusIconHandler (FUN_14007a120) — status icon click handler
// REPLACED: now static StatusBarUI_StatusIconHandler_Impl() in code/Game/Dialog/statusbarui.cpp
// Iterates citizens starting at self->statusCitizenIndex, finds next citizen whose
// HappinessComponent status bits include the clicked icon's bit, then navigates to them.
// self = StatusBarUI*, iconIndex = 0..5 (home/food/cold/job/tool/disease)
typedef void (*fn_StatusBarUI_StatusIconHandler_t)(void* self, int iconIndex);  // kept for reference

// ============================================================
// ProductionUI support
// ============================================================

// TrackerComponent rebuild: FUN_14000d9d0 — moves/rebuilds tracker item arrays
// Called when game world item count changes.
// param_1 = dst array descriptor (uint64_t[2]: ptr, count+capacity)
// param_2 = src array descriptor (uint64_t[2]: ptr, count+capacity)
// Frees old dst buffer, allocates count*16 bytes, copies 16-byte entries from src.
inline void fn_TrackerComponent_Rebuild(void* dstArray, void* srcArray) {
    if (dstArray == srcArray) return;
    uint64_t* dst = (uint64_t*)dstArray;
    uint64_t* src = (uint64_t*)srcArray;
    fn_MemFreeStatic((void*)*dst);
    dst[1] = 0;
    dst[0] = 0;
    int count = (int)(uint32_t)src[1];  // low 32 bits of src[1]
    dst[1] = (uint64_t)(uint32_t)count;
    if (count > 0) {
        // High word of dst[1]+4 = capacity (same as count on fresh alloc)
        *(int*)((uintptr_t)dst + 0xc) = count;
        void* newBuf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)count * 16, 8);
        *dst = (uint64_t)newBuf;
    }
    // Copy entries
    if ((int)(uint32_t)dst[1] > 0) {
        uint8_t* dstBuf = (uint8_t*)*dst;
        uint8_t* srcBuf = (uint8_t*)*src;
        int n = (int)(uint32_t)dst[1];
        for (int i = 0; i < n; i++) {
            if (dstBuf) {
                uint64_t* d = (uint64_t*)(dstBuf + (uintptr_t)i * 16);
                uint64_t* s = (uint64_t*)(srcBuf + (uintptr_t)i * 16);
                d[0] = s[0];
                d[1] = s[1];
            }
        }
    }
}

// ============================================================
// ProfessionUI support
// ============================================================

// Object::External::GetPath() — returns Filename by value (non-trivial return ABI)
// Member function: RCX=this, RDX=retBuf (Filename, 8 bytes). Returns retBuf in RAX.
typedef void* (*fn_External_GetPath_t)(void* self, void* retBuf);
extern fn_External_GetPath_t fn_External_GetPath;

// System::Filename::GetPathIdentifer(this, retval) -> String (member, non-trivial return)
typedef void* (*fn_Filename_GetPathIdentifier_t)(void* self, void* retval);
extern fn_Filename_GetPathIdentifier_t fn_Filename_GetPathIdentifier;

// BSTRebalance — now inline in professionui.cpp (BSTRebalance_Impl)

// fn_ProfessionUI_HandleSpinner — reimplemented inline in professionui.cpp (ProfessionUI_HandleSpinner)
// fn_ProfessionUI_HandleButton — reimplemented inline in professionui.cpp (ProfessionUI_HandleButton)

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

// UI::DialogInstance::DialogInstance(this, ui, dialog, parentInternalPtr, dataSource, extraPtr, flags)
// Full constructor for creating child dialog instances.
// MSVC signature: (UserInterface*, Dialog const*, InternalPtr<DialogInstance>, Base*, void*, int)
// Resolved from Runtime export at 0x180114870
typedef void* (*fn_DialogInstance_Ctor_t)(void* self, void* userInterface, void* externalRes,
                                          void* parentPtr, void* dataSource, void* extraPtr, int flags);
extern fn_DialogInstance_Ctor_t fn_DialogInstance_Ctor;

// MainMenuDialog BuildId child dialog vtable (PTR_FUN_14015f5a0 in exe)
extern void** g_MainMenuDialog_BuildIdVtable;

// MainMenuDialog Slot8 private handler functions — NOW REIMPLEMENTED
// All 11 Open* functions are reimplemented inline in mainmenudialog.cpp.
// They use dialog-specific ctor function pointers and vtable addresses below.

// Dialog-specific constructor function pointers (called from Open* handlers)
// These are exe-internal ctors that call DialogInstance::DialogInstance then set subclass vtable.

// NewGameDialog ctor: FUN_140071040(self, ui, extRes, nullParent, parentPtr)
// Ctor sig: (self, ui, extRes, parentIBP, stack: parentMenuIBP)
// Allocates 0xAE8 bytes. Passes MainMenuDialog as parent via stack arg.
typedef void* (*fn_NewGameDialog_Ctor_t)(void* self, void* ui, void* extRes, void* nullParent,
                                          void* parentMenuIBP);
extern fn_NewGameDialog_Ctor_t fn_NewGameDialog_Ctor;  // 0x140071040

// SaveLoadDialog ctor: FUN_1400b8270(self, ui, extRes, nullParent, mode, enginePtr, gameWorldPtr)
// mode: 0=save, 1=load. enginePtr from MainMenuDialog+0xC0, gameWorldPtr from +0xB8.
typedef void* (*fn_SaveLoadDialog_Ctor_t)(void* self, void* ui, void* extRes, void* nullParent,
                                           uint8_t mode, void* enginePtr, void* gameWorldPtr);
extern fn_SaveLoadDialog_Ctor_t fn_SaveLoadDialog_Ctor;  // 0x1400b8270

// OptionsDialog ctor: FUN_140075150(self, ui, extRes, nullParent, stack: 0,0,0)
// Standard DialogInstance ctor args, no extra params. Allocates 0x1C0 bytes.
typedef void* (*fn_OptionsDialog_Ctor_t)(void* self, void* ui, void* extRes, void* nullParent);
extern fn_OptionsDialog_Ctor_t fn_OptionsDialog_Ctor;  // 0x140075150

// ScenarioDialog ctor: FUN_1400786b0(self, ui, extRes, parentIBP, difficulty, parentMenuIBP)
// difficulty = byte at MainMenuDialog+0xB8 area. Also used for TutorialDialog.
typedef void* (*fn_ScenarioDialog_Ctor_t)(void* self, void* ui, void* extRes, void* parentIBP,
                                           void* parentMenuIBP, uint8_t difficulty);
extern fn_ScenarioDialog_Ctor_t fn_ScenarioDialog_Ctor;  // 0x1400786b0

// MapParamsDialog ctor: FUN_14006bd80(self, ui, extRes, nullParent)
// Allocates 0xF8 bytes. Standard args.
typedef void* (*fn_MapParamsDialog_Ctor_t)(void* self, void* ui, void* extRes, void* nullParent);
extern fn_MapParamsDialog_Ctor_t fn_MapParamsDialog_Ctor;  // 0x14006bd80

// ModDialog ctor: FUN_140070030(self, ui, extRes, parentIBP, parentMenuIBP)
// Allocates 0xD8 bytes. Passes parent via stack.
typedef void* (*fn_ModDialog_Ctor_t)(void* self, void* ui, void* extRes, void* parentIBP,
                                      void* parentMenuIBP);
extern fn_ModDialog_Ctor_t fn_ModDialog_Ctor;  // 0x140070030

// AchievementDialog vtable: PTR_FUN_14015b020. Uses DialogInstance_Ctor directly + vtable patch.
// Size 0xD0. Extra fields zeroed: +0xB8, +0xC0, +0xC8.
extern void** g_AchievementDialog_Vtable;  // 0x14015b020

// GoalDialog vtable: PTR_FUN_14015beb0. Uses DialogInstance_Ctor directly + vtable patch.
// Size 0xC8 (200 decimal). Extra fields zeroed: +0xB8, +0xC0.
extern void** g_GoalDialog_Vtable;  // 0x14015beb0

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

// System::Random::RandFloat(this) -> float
// ?RandFloat@Random@System@@QEAAMXZ (ordinal 3444, Runtime 0x1800c7d40)
// Returns a uniform float in [0.0, 1.0).
typedef float (*fn_Random_RandFloat_t)(void* self);
extern fn_Random_RandFloat_t fn_Random_RandFloat;

// System::Random::RandFloatTol(this) -> float in [-1.0, 1.0). Defined as an
// extern "C" link shim in code/Game/Game/worldgen_f390_shims.cpp (lazily resolves
// the Runtime export "?RandFloatTol@Random@System@@QEAAMXZ"); declared there and
// reused by callers that need the tolerance-range primitive.
extern "C" float fn_Random_RandFloatTol(void* self);

// GetCurrentProductIndex (FUN_140119470 in exe)
// Reads +0x50 product index; if not -1 (0xffffffff), returns it directly.
// If -1, performs weighted random selection from +0x58 float array (count at +0x60)
// using System::Random::RandFloat on the game singleton's Random at +0xd8.
inline uint32_t fn_GetCurrentProductIndex(void* component) {
    uintptr_t c = (uintptr_t)component;
    uint32_t idx = *(uint32_t*)(c + 0x50);
    if ((int)idx < 0) {
        if (idx != 0xffffffff) return 0;
        int count = *(int*)(c + 0x60);
        float* weights = *(float**)(c + 0x58);
        float totalWeight = 0.0f;
        for (int i = 0; i < count; i++) totalWeight += weights[i];
        void* randomPtr = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0xd8);
        float randVal = fn_Random_RandFloat(randomPtr);
        float accum = 0.0f;
        for (int i = 0; i < count; i++) {
            accum += weights[i];
            if (randVal * totalWeight < accum) return (uint32_t)i;
        }
        return 0;
    }
    return idx;
}

// FUN_14012e5c0: Calculate worker slot count for a StorageLocationComponent/WorkplaceComponent.
// param_1+0x18 = entity ptr (checks IsDerivedFrom WorkplaceJobType at DAT_1401c80a8).
// If jobType[0x34]==0: return jobType[0x24] (fixed count).
// Else: scale by building tile area (building at param_1+0x10).
// Used as fn_StorageCountQuery in workplaceui, fn_CalcWorkerSlots in professionui.
inline int fn_StorageCountQuery(void* component) {
    uintptr_t c = (uintptr_t)component;
    void* base = *(void**)(c + 0x18);
    void* jobType = fn_IsDerivedFrom(base, *g_TypeId_WorkplaceJobType) ? base : nullptr;
    if (!jobType) return 0;
    uintptr_t jt = (uintptr_t)jobType;
    if (*(uint8_t*)(jt + 0x34) == 0) {
        return *(int*)(jt + 0x24);
    }
    fn_InternalBasePtr_CheckPtr((void*)(c + 0x10));
    uint64_t buildBP = 0;
    fn_FindComponent_Build(*(void**)(c + 0x10), &buildBP);
    fn_InternalBasePtr_CheckPtr(&buildBP);
    uintptr_t build = (uintptr_t)buildBP;
    int maxWorkers = *(int*)(jt + 0x24);
    int tileArea   = *(int*)(jt + 0x30);
    int width      = (int)*(short*)(build + 0x2c);
    int height     = (int)*(short*)(build + 0x2e);
    int result = (int)(((float)(width * height) / (float)tileArea) * (float)maxWorkers + 1.0f);
    if (result > maxWorkers) result = maxWorkers;
    if (result < 1)          result = 1;
    return result;
}
// Alias used by professionui.cpp
inline int fn_CalcWorkerSlots(void* storageLocComp) {
    return fn_StorageCountQuery(storageLocComp);
}

// Name generator: FUN_140099ac0 — NOW REIMPLEMENTED in newgamedialog.cpp
// Loads "Game/Names/Cities.ndb", generates name, trims "vill" suffix.
// Old typedef kept for reference:
// typedef void* (*fn_GenerateTownName_t)(uint64_t unused, void* retStr, void* random, int maxLen);
// extern fn_GenerateTownName_t fn_GenerateTownName;

// Inner name generator: FUN_1400a4b30(retStr, nameDB, random, minSyllables, maxChars)
// Generates random name from NameDatabase with retry loop (up to 50 attempts).
typedef void* (*fn_NameDB_GenerateName_t)(void* retStr, void* nameDB, void* random,
                                           int minSyllables, int maxChars);
extern fn_NameDB_GenerateName_t fn_NameDB_GenerateName;  // 0x1400a4b30

// TypeInfo for NameDatabase IsDerivedFrom check
extern uint32_t* g_TypeId_NameDatabase;  // DAT_1401c5a60

// fn_String_SubString already declared above (line ~1490)

// System::String::operator==(this, cstr) -> bool
typedef bool (*fn_String_Equals_t)(void* self, const char* cstr);
extern fn_String_Equals_t fn_String_Equals;

// System::String::operator+=(this, retval, cstr)
// Non-trivial return ABI: RCX=this, RDX=retval(String), R8=cstr
// Modifies self in-place AND returns copy through retval.
// Resolved from exe dispatch table at 0x140151a18.
typedef void* (*fn_String_Append_t)(void* self, void* retval, const char* cstr);
extern fn_String_Append_t fn_String_Append;

// Start game handler: FUN_1400b4540
// Constructs GameUpdateLoop -> GameLoadDialogLoopT -> LoaderLoop chain and sets next loop.
// Params: worldSetup, displayName, seed, terrainSize, disasterOff, climateGen, terrainGen, mapGen, scenarioIdx, nullIBP
typedef void (*fn_StartNewGame_t)(void* worldSetup, void* nameStr, uint32_t seed,
                                   uint32_t terrainSize, char disasterOff, void* climate,
                                   void* terrain, void* mapGen, uint32_t scenarioIdx, void* retvalIBP);
extern fn_StartNewGame_t fn_StartNewGame;

// TypeInfo IDs for generator IsDerivedFrom checks
extern uint32_t* g_TypeId_MapGenerator;      // DAT_1401c59b8
extern uint32_t* g_TypeId_ClimateGenerator;  // DAT_1401c5910
extern uint32_t* g_TypeId_TerrainGenerator;  // DAT_1401c5b40

// ============================================================
// OptionsDialog support — Runtime DLL exports for video/audio config
// ============================================================

// Video::Device::ValidateConfiguration(this, &renderer, &adapter, &width, &height,
//                                       &refreshNum, &refreshDen, &antialias, &adapter2)
// ?ValidateConfiguration@Device@Video@@QEAAXAEAH0000000@Z
typedef void (*fn_VideoDevice_ValidateConfiguration_t)(void* self, int*, int*, int*, int*, int*, int*, int*, int*);
extern fn_VideoDevice_ValidateConfiguration_t fn_VideoDevice_ValidateConfiguration;

// Video::Device::GetMultisampleModeCount(this) -> int
// ?GetMultisampleModeCount@Device@Video@@QEBAHXZ
typedef int (*fn_VideoDevice_GetMultisampleModeCount_t)(void* self);
extern fn_VideoDevice_GetMultisampleModeCount_t fn_VideoDevice_GetMultisampleModeCount;

// Video::Device::IsMultisampleModeSupported(this, mode) -> bool
// ?IsMultisampleModeSupported@Device@Video@@QEBA_NH@Z
typedef bool (*fn_VideoDevice_IsMultisampleModeSupported_t)(void* self, int mode);
extern fn_VideoDevice_IsMultisampleModeSupported_t fn_VideoDevice_IsMultisampleModeSupported;

// OptionsDialog::OnOk/OnCancel/OnApply — REMOVED: reimplemented in code/Game/Dialog/optionsdialog.cpp
typedef void (*fn_OptionsDialog_SimpleFunc_t)(void* self);  // kept for PopulateInputBindList
// OptionsDialog::PopulateInputBindList — REIMPLEMENTED in code/Game/Dialog/optionsdialog.cpp
// extern fn_OptionsDialog_SimpleFunc_t fn_OptionsDialog_PopulateInputBindList;
// OptionsDialog::OnInputBindClick — REIMPLEMENTED in code/Game/Dialog/optionsdialog.cpp
// typedef void (*fn_OptionsDialog_OnInputBindClick_t)(void* self, void* element, int bindIndex, int slot);
// extern fn_OptionsDialog_OnInputBindClick_t fn_OptionsDialog_OnInputBindClick;

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

// fn_Element_GetParent — now inline in optionsdialog.cpp (vtable call at +0x48)

// OptionsDialog game settings global — DAT_1401c3de0 in exe
// int[8] array: [0]=uiScale, [1]=autosaveInterval, [2]=statusIconDist,
// [3]=statusIconEnabled, [4]=statusIconScale, [5]=edgeScroll,
// [6]=autopause, [7]=lastTab
// Byte offsets: +0x15=showCelsius, +0x16=showWeather, +0x17=autoslow
extern int32_t** g_pGameSettingsPtr;  // DAT_1401c3de0 — pointer TO pointer to game settings

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
// StorageUI_SortItems — reimplemented as clean C++ introsort in storageui.cpp
// Sorts array of 0x18-byte resource display structs alphabetically by resource name.
// Each struct: [0]=entityPtr, [8]=resourceTypePtr, [0x10]=count
// Original: FUN_1400213f0 (introsort) + FUN_1400b8370 (comparator) + FUN_140015ad0 (insertion sort)
void StorageUI_SortItems(void* items, int count, void* scratch, void* scratch2);

// MapUI helpers — REIMPLEMENTED in code/Game/Dialog/mapui.cpp
// fn_MapUI_SetupImage (FUN_14006cb00) — now MapUI_SetupImage()
// fn_MapUI_HandleClick (FUN_14006c850) — now MapUI_HandleClick()

// MapUI ground projection helper (FUN_14006c5b0) — projects camera frustum corners
// to ground plane. Used internally by MapUI_HandleClick.
typedef void (*fn_MapUI_ProjectCornersToGround_t)(void* outputCorners);
extern fn_MapUI_ProjectCornersToGround_t fn_MapUI_ProjectCornersToGround;

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

// FUN_14012e100: UIDescription_AddControllers — REIMPLEMENTED in code/Game/Dialog/toolui.cpp
// Iterates UIDescription controller entries, checks for TabElement targets,
// and calls the appropriate AddController overload for each entry.
// Old fn_* pointer removed; replaced by UIDesc_AddControllers_Impl().
void UIDesc_AddControllers_Impl(void* uiDesc, void* dialogInstPtr, void* dataSource);

// TypeId for UIDescription — DAT_1401c7f90
extern uint32_t* g_TypeId_UIDescription;

// UI::TabElement::AddTab(this, retval, tabDesc, elementDesc) -> InternalPtr<Element>
// Non-trivial return ABI: RCX=this, RDX=retval, R8=tabDesc, R9=elementDesc
// Resolved from exe dispatch table at 0x1401529c8
typedef void* (*fn_TabElement_AddTab_t)(void* self, void* retval, void* tabDesc, void* elementDesc);
extern fn_TabElement_AddTab_t fn_TabElement_AddTab;

// UI::DialogInstance::AddController (5-arg, tab variant)
// (this, typeId, dataSource, controllerConfig, tabPageElement)
// Resolved from exe dispatch table at 0x1401529d0
typedef void (*fn_DialogInstance_AddControllerTab_t)(void* self, uint32_t typeId,
    void* dataSource, void* controllerConfig, void* tabPageElement);
extern fn_DialogInstance_AddControllerTab_t fn_DialogInstance_AddControllerTab;

// UI::DialogInstance::AddController (6-arg, non-tab variant)
// (this, typeId, dataSource, controllerConfig, elementDesc, nameStr)
// Resolved from exe dispatch table at 0x140151a90
typedef void (*fn_DialogInstance_AddController6_t)(void* self, uint32_t typeId,
    void* dataSource, void* controllerConfig, void* elementDesc, void* nameStr);
extern fn_DialogInstance_AddController6_t fn_DialogInstance_AddController6;

// ============================================================
// DragTool support functions
// ============================================================

// Input::KeyMap::Released(this, slot, keyId) -> bool
// ?Released@KeyMap@Input@@QEBA_NHH@Z
// Dispatch table at 0x1401528f0
typedef bool (*fn_KeyMap_Released_t)(void* self, int slot, int keyId);
extern fn_KeyMap_Released_t fn_KeyMap_Released;

// UI::DialogInstance::GetRoot(this) -> InternalBasePtr<Element>* (returns ptr to root element IBP)
// ?GetRoot@DialogInstance@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@XZ
// Dispatch table at 0x1401527f8. Returns InternalPtr<Element> BY VALUE (hidden
// retbuf: RCX=this, RDX=&retbuf; returns retbuf in RAX). Caller supplies retbuf.
typedef void* (*fn_DialogInstance_GetRoot_t)(void* self, void* retbuf);
extern fn_DialogInstance_GetRoot_t fn_DialogInstance_GetRoot;

// Audio::Scene::PlayEffect(this, soundId, count, time) — plays a sound effect
// ?PlayEffect@Scene@Audio@@QEAAXVExternalPtr@Object@@HAEBVTime@System@@@Z
// Dispatch table at 0x140152830
typedef void (*fn_AudioScene_PlayEffect_t)(void* self, uint64_t soundId, int count, void* time);
extern fn_AudioScene_PlayEffect_t fn_AudioScene_PlayEffect;

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

// ModDetails_StringArrayPush — now inline in moddetailsdialog.cpp (ModDetails_StringArrayPush_Impl)

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

// UI::DialogInstance::OnUpdateLayout(this) — virtual, slot +0x30
// Iterates child elements at +0x78 (count at +0x80) and dispatches their
// OnUpdateLayout virtual (child vtable +0x38). Mangled:
//   ?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ — Runtime DLL @ 0x1801153a0
typedef void (*fn_DialogInstance_OnUpdateLayout_t)(void* self);
extern fn_DialogInstance_OnUpdateLayout_t fn_DialogInstance_OnUpdateLayout;

// UI::DialogInstance::OnUpdate(this, Time const&) — virtual, slot +0x38
// Iterates child elements at +0x78 (count at +0x80) and dispatches their
// OnUpdate virtual (child vtable +0x40) with the time pointer. Mangled:
//   ?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z — Runtime DLL @ 0x180115400
typedef void (*fn_DialogInstance_OnUpdate_t)(void* self, const void* time);
extern fn_DialogInstance_OnUpdate_t fn_DialogInstance_OnUpdate;

// ============================================================
// ModDialog helper functions (exe-local)
// ============================================================

// FUN_14006eec0: Refresh mod list display (updates text, icons, thumbnails)
// REPLACED: now static ModDialog_RefreshDisplay_Impl() in code/Game/Dialog/moddialog.cpp
typedef void (*fn_ModDialog_RefreshDisplay_t)(void* self);  // kept for reference

// FUN_14006e930: Toggle mod enabled/disabled at index
// REPLACED: now inline ModDialog_ToggleMod_Impl() in code/Game/Dialog/moddialog.cpp

// FUN_14006e660: Move mod to top of list
// REPLACED: now inline ModDialog_MoveToTop_Impl() in code/Game/Dialog/moddialog.cpp

// FUN_14006e490: Show mod details dialog
// REPLACED: now inline ModDialog_ShowDetails_Impl() in code/Game/Dialog/moddialog.cpp

// FUN_14006ea70: Apply mod changes (save, reload if needed)
// REPLACED: now inline ModDialog_Apply_Impl() in code/Game/Dialog/moddialog.cpp

// ModDetailsDialog vtable pointer (PTR_FUN_14015c670 in exe .rdata)
extern uintptr_t g_ModDetailsDialog_Vtable;

// ModDialog_CloseAndReload (FUN_14006e9b0) — closes dialog and triggers UI reload
typedef void (*fn_ModDialog_CloseAndReload_t)(void* self);
extern fn_ModDialog_CloseAndReload_t fn_ModDialog_CloseAndReload;

// ModDialog_RefreshDisplay original (FUN_14006eec0) — for A/B testing
typedef void (*fn_ModDialog_RefreshDisplay_Orig_t)(void* self);
extern fn_ModDialog_RefreshDisplay_Orig_t fn_ModDialog_RefreshDisplay_Orig;

// MainMenu_Reload (FUN_1400b4790) — reloads from main menu context
typedef void (*fn_MainMenu_Reload_t)(void* self);
extern fn_MainMenu_Reload_t fn_MainMenu_Reload;

// Game_ReloadWithSave (FUN_1400b49f0) — saves then reloads from in-game context
typedef void (*fn_Game_ReloadWithSave_t)(void* self, void* savedState, void* param, void* nullRef);
extern fn_Game_ReloadWithSave_t fn_Game_ReloadWithSave;

// Save::SavedState::SavedState() — default ctor (0x10 bytes)
typedef void* (*fn_SavedState_Ctor_t)(void* self);
extern fn_SavedState_Ctor_t fn_SavedState_Ctor;

// ReloadDialog vtable pointer (PTR_FUN_14015c480 in exe .rdata)
extern uintptr_t g_ReloadDialog_Vtable;

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

typedef void (*g_fn_Serialize_Bool_t)(void* self, void* field, const char* name);
extern g_fn_Serialize_Bool_t g_fn_Serialize_Bool;

typedef int (*g_fn_Serializer_GetVersion_t)(void* self);
extern g_fn_Serializer_GetVersion_t g_fn_Serializer_GetVersion;

// ============================================================
// TownHallUI support functions
// ============================================================

// TabElement type ID: exref at 0x140151de0 (double-indirect)
extern uint32_t* g_TypeId_TabElement;

// --- Reimplemented TownHallUI functions (clean C++ in townhallui.cpp) ---
// Forward-declared here; defined in code/Game/Dialog/townhallui.cpp.
struct TownHallUI;  // forward decl
void TownHallUI_InitGeneral(TownHallUI* self);           // was FUN_1400807f0
void TownHallUI_InitGraph(TownHallUI* self);             // was FUN_14007f0a0
void TownHallUI_InitProfessions(TownHallUI* self);       // was FUN_140080160
void TownHallUI_InitProduction(TownHallUI* self);        // was FUN_14007fc30
void TownHallUI_InitInventory(TownHallUI* self);         // was FUN_14007f660
void TownHallUI_InitAgriculture(TownHallUI* self);       // was FUN_14007ebf0
void TownHallUI_UpdateGraph(TownHallUI* self);           // was FUN_14007e5b0
void TownHallUI_UpdateGeneral(TownHallUI* self);         // was FUN_14007d030
void TownHallUI_UpdateProfessions(TownHallUI* self);     // was FUN_14007ccb0
void TownHallUI_UpdateProduction(TownHallUI* self);      // was FUN_14007c7f0
void TownHallUI_UpdateAgriculture(TownHallUI* self);     // was FUN_14007c220
void TownHallUI_UpdateInventory(TownHallUI* self);       // was FUN_14007def0
void TownHallUI_UpdateAll(TownHallUI* self);             // was FUN_14007d590
void TownHallUI_OnProfessionButton(TownHallUI* self, int index); // was FUN_14007dca0
void TownHallUI_OnProfessionSpinner(TownHallUI* self, int index); // was FUN_14007dd10

// --- Still exe-resolved TownHallUI helpers (thin wrappers in townhallui.cpp) ---
typedef void (*fn_TownHallUI_SubFunc_t)(void* self);
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_InitProfessions; // FUN_140080160
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_InitProduction;  // FUN_14007fc30
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_InitInventory;   // FUN_14007f660
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_InitAgriculture; // FUN_14007ebf0
// fn_TownHallUI_UpdateProduction — reimplemented in townhallui.cpp (FUN_14007c7f0)
// fn_TownHallUI_UpdateAgriculture — reimplemented in townhallui.cpp (FUN_14007c220)
// fn_TownHallUI_UpdateInventory — reimplemented in townhallui.cpp (FUN_14007def0)
// fn_TownHallUI_UpdateAll — reimplemented in townhallui.cpp (FUN_14007d590)
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateProduction;   // kept for reference, nullptr
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateAgriculture;  // kept for reference, nullptr
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateInventory;    // kept for reference, nullptr
extern fn_TownHallUI_SubFunc_t fn_TownHallUI_UpdateAll;          // kept for reference, nullptr

// --- GroupRating helpers (hearts/stars display) ---
// FUN_14006abf0: Init group rating element — iterates children of a group element,
// finds "whole" and "half" sub-elements in each child, stores them in two dynamic arrays.
// Layout: param_1 points to {wholeArray(ptr), wholeCount|wholeCap, halfArray(ptr), halfCount|halfCap}
typedef void (*fn_GroupRating_Init_t)(void* ratingArrayBase, void* groupBasePtr);
extern fn_GroupRating_Init_t fn_GroupRating_Init;    // FUN_14006abf0

// FUN_14006aed0: Update group rating display — shows whole/half icons based on value/max ratio.
// Shows filled whole icons, one half icon at threshold, hides the rest.
typedef void (*fn_GroupRating_Update_t)(void* ratingArrayBase, float value, float maxValue);
extern fn_GroupRating_Update_t fn_GroupRating_Update; // FUN_14006aed0

// --- GraphElement functions (resolved from Runtime DLL) ---
typedef void (*fn_GraphElement_RemoveGraphs_t)(void* self);
extern fn_GraphElement_RemoveGraphs_t fn_GraphElement_RemoveGraphs;

typedef void (*fn_GraphElement_AddGraph_t)(void* self, uint32_t color);
extern fn_GraphElement_AddGraph_t fn_GraphElement_AddGraph;

typedef void (*fn_GraphElement_AddGraphPoint_t)(void* self, int graphIndex, int value);
extern fn_GraphElement_AddGraphPoint_t fn_GraphElement_AddGraphPoint;

// --- StringTable functions (resolved from Runtime DLL) ---
typedef int (*fn_StringTable_GetCount_t)(void* self);
extern fn_StringTable_GetCount_t fn_StringTable_GetCount;

// --- Production stat query (exe) ---
// FUN_140090f20: Query production stats for a resource over a time range.
// Returns accumulated production/consumption count.
typedef int (*fn_QueryProductionStat_t)(void* gameData, int resourceIdx, int startTick, int endTick);
extern fn_QueryProductionStat_t fn_QueryProductionStat;  // FUN_140090f20

// --- Citizen assignment functions used by OnProfessionSpinner ---
// FUN_14008cbf0: Try to find idle citizen for profession assignment
typedef void* (*fn_CitizenList_FindIdle_t)(void* citizenList, void* retval, void* profSlot, uint64_t profId);
extern fn_CitizenList_FindIdle_t fn_CitizenList_FindIdle;  // FUN_14008cbf0

// FUN_14008ca20: Try to find working citizen to reassign
typedef void* (*fn_CitizenList_FindWorking_t)(void* citizenList, void* retval, void* profSlot, uint64_t profId);
extern fn_CitizenList_FindWorking_t fn_CitizenList_FindWorking;  // FUN_14008ca20

// FUN_14008c5f0: Try to find student to assign
typedef void* (*fn_CitizenList_FindStudent_t)(void* citizenList, void* retval, void* profSlot, uint64_t profId);
extern fn_CitizenList_FindStudent_t fn_CitizenList_FindStudent;  // FUN_14008c5f0

// FUN_14008c4b0: Final fallback — find any available citizen
typedef void (*fn_CitizenList_FindAny_t)(void* citizenList, void* retval, void* profSlot, uint64_t profId);
extern fn_CitizenList_FindAny_t fn_CitizenList_FindAny;  // FUN_14008c4b0

// FUN_140089470: Shuffle entity list for a profession (randomizes assignment order)
// citizenList = CitizenList (gameWorld+0xF0), profId = profession ExternalPtr value
typedef void (*fn_CitizenList_ShuffleEntities_t)(void* citizenList, uint64_t profId);
extern fn_CitizenList_ShuffleEntities_t fn_CitizenList_ShuffleEntities;  // FUN_140089470

// --- Profession array push helpers (exe) ---
// FUN_140015800: Push 0x40-byte profession entry into dynamic array at (ptr, count|cap).
typedef void (*fn_ProfArrayPush_t)(void* arrayBase, int newCount, void* entry);
extern fn_ProfArrayPush_t fn_ProfArrayPush;  // FUN_140015800

// FUN_1400159b0: Push 0x28-byte production limit entry into dynamic array.
typedef void (*fn_ProdArrayPush_t)(void* arrayBase, int newCount, void* entry);
extern fn_ProdArrayPush_t fn_ProdArrayPush;  // FUN_1400159b0

// --- BST helpers (exe) ---
// FUN_140013480: Rebalance BST after insert (professions, 0x30-byte nodes)
typedef void (*fn_BST_Rebalance_0x30_t)(void* treeRoot, void* newNode);
extern fn_BST_Rebalance_0x30_t fn_BST_Rebalance_0x30;  // FUN_140013480

// FUN_1400149a0: Rebalance BST after insert (agriculture, 0x28-byte nodes)
typedef void (*fn_BST_Rebalance_0x28_t)(void* treeRoot, void* newNode);
extern fn_BST_Rebalance_0x28_t fn_BST_Rebalance_0x28;  // FUN_1400149a0

// FUN_140014890: Insert into BST (used in UpdateAll)
typedef void (*fn_BST_Insert_0x14_t)(void* treeRoot, void* retNode, void* keyPtr);
extern fn_BST_Insert_0x14_t fn_BST_Insert_0x14;  // FUN_140014890

// FUN_140014c30: Free BST nodes recursively
typedef void (*fn_BST_FreeNodes_t)(void* treeRoot, void* subtree);
extern fn_BST_FreeNodes_t fn_BST_FreeNodes;  // FUN_140014c30

// --- Inventory sort helpers (exe) ---
// FUN_140015200: Sort inventory array ascending (by name)
typedef void (*fn_InventorySort_Asc_t)(void* items, int count, void* scratch1, void* scratch2);
extern fn_InventorySort_Asc_t fn_InventorySort_Asc;  // FUN_140015200

// FUN_140015500: Sort inventory array descending (by name)
typedef void (*fn_InventorySort_Desc_t)(void* items, int count, void* scratch1, void* scratch2);
extern fn_InventorySort_Desc_t fn_InventorySort_Desc;  // FUN_140015500

// fn_GoToEntity — now inline as Toolbar_CenterOnEntity_Impl (FUN_140103040)

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

// UI::Element::SetNeedsSizeCalc(this) — marks element for layout recalculation
// ?SetNeedsSizeCalc@Element@UI@@QEAAXXZ
typedef void (*fn_Element_SetNeedsSizeCalc_t)(void* self);
extern fn_Element_SetNeedsSizeCalc_t fn_Element_SetNeedsSizeCalc;

// TradeUI::RefreshSortedDisplay (FUN_1400846b0) — REPLACED with TradeUI_RefreshDisplay in tradeui.cpp
// fn_TradeUI_RefreshDisplay pointer kept for reference / fallback only.
typedef void (*fn_TradeUI_RefreshDisplay_t)(void* self, uint64_t p2, uint64_t p3, int64_t p4);
extern fn_TradeUI_RefreshDisplay_t fn_TradeUI_RefreshDisplay;

// ValueDescription type ID — DAT_1401c4bf0 (sprite + name on trade entity descriptions)
extern uint32_t* g_TypeInfo_ValueDescription;

// TradeDescription type ID — DAT_1401c4bb8 (trade order data on TradeComponent)
extern uint32_t* g_TypeInfo_TradeDescription;

// DynArray clear for 0x18-byte stride arrays (FUN_140016200)
// Resets count to 0. arrayStruct = pointer to {dataPtr, ..., count(+8), capacity(+C)}.
typedef void (*fn_DynArray_Clear_0x18_t)(void* arrayStruct);
extern fn_DynArray_Clear_0x18_t fn_DynArray_Clear_0x18;

// Introsort by name (FUN_1400162d0) — sorts 0x18-byte entries, name-based comparator
typedef void (*fn_IntroSort_ByName_t)(void* array, int count, void* context, int64_t param4);
extern fn_IntroSort_ByName_t fn_IntroSort_ByName;

// Introsort by component (FUN_1400165d0) — sorts 0x18-byte entries, component-based comparator
typedef void (*fn_IntroSort_ByComponent_t)(void* array, int count, void* context);
extern fn_IntroSort_ByComponent_t fn_IntroSort_ByComponent;

// fn_TradeUI_RefreshSellTable — now reimplemented inline in tradeui.cpp (FUN_140083c70)
// fn_TradeUI_RefreshBuyTable — now reimplemented inline in tradeui.cpp (FUN_140083100)

// DynArray clear for 0x20-byte stride arrays (FUN_140016050)
// Resets count to 0. arrayStruct = pointer to {dataPtr(+0), count(+8), capacity(+C)}.
typedef void (*fn_DynArray_Clear_0x20_t)(void* arrayStruct);
extern fn_DynArray_Clear_0x20_t fn_DynArray_Clear_0x20;

// DynArray clear for 0x18-byte stride arrays (FUN_140016130) — for buy entries
// Same pattern as fn_DynArray_Clear_0x18 but at different address.
extern fn_DynArray_Clear_0x18_t fn_DynArray_Clear_0x18_Buy;

// Building_GetOrderName(building, retval) -> String*
// FUN_140015fd0: returns the current order's name string
typedef void* (*fn_Building_GetOrderName_t)(void* building, void* retval);
extern fn_Building_GetOrderName_t fn_Building_GetOrderName;

// Building_GetBuyEntries(building, dynArray) — populates buy entry list
// FUN_140059670: walks trade description + ExternalFactory to build buyable item list
typedef void (*fn_Building_GetBuyEntries_t)(void* building, void* dynArray);
extern fn_Building_GetBuyEntries_t fn_Building_GetBuyEntries;

// DynArray_Clear_0x08 (FUN_140001bb0) — clear 8-byte stride array (for buy entries temp)
typedef void (*fn_DynArray_Clear_0x08_t)(void* arrayStruct, int newCount);
extern fn_DynArray_Clear_0x08_t fn_DynArray_Clear_0x08;

// TradeUI::OnTradeButton — executes trade (needs opus — ~160 lines)
typedef void (*fn_TradeUI_OnTradeButton_t)(void* self, void* p2, void* p3, int64_t p4);
extern fn_TradeUI_OnTradeButton_t fn_TradeUI_OnTradeButton;   // 0x140081420

// TradeUI type IDs used in IsDerivedFrom checks
// TableElement type ID — exref 0x140151510
extern uint32_t* g_TypeId_TableElement;

// SpinnerElement / NumberElement type ID — exref 0x1401516c8
extern uint32_t* g_TypeId_SpinnerElement;

// TextElement type ID — exref 0x140151508
extern uint32_t* g_TypeId_TextElement;

// LabelElement type ID — exref 0x140151668
extern uint32_t* g_TypeId_LabelElement;

// NumberEditElement type ID — exref 0x140151cb0
extern uint32_t* g_TypeId_NumberEditElement;

// SliderElement type ID — exref 0x140151cf0
// Used in TradeUI inner loops (tablePurchase child(2), tableStorage child(1),
// tableSell child(3), tableBuy child(3))
extern uint32_t* g_TypeId_SliderElement;

// ButtonElement type ID — exref 0x1401514f8 (same as g_TypeId_HelpButtonElem)
// Already available as g_TypeId_HelpButtonElem

// ComboElement type ID — exref 0x140151690 (same as g_TypeId_HelpComboElem)
// Already available as g_TypeId_HelpComboElem

// SelectDescription type ID — DAT_1401c48a8
// Used in Serialize ExternalFactory loop to count selectable build types
extern uint32_t* g_TypeId_SelectDescription;

// FUN_14005a520: Building_GetSellableCount(building, resource_entity) -> int
// Queries available sell count for a given resource at a building.
// Used by UpdateSellTotals and RefreshSellSpinnerValues.
typedef int (*fn_Building_GetSellableCount_t)(void* building, void* resourceEntity);
extern fn_Building_GetSellableCount_t fn_Building_GetSellableCount;  // 0x14005a520

// FUN_14000e1c0: StorageTree_Insert(tree*, retval*, entry*) — insert into storage BST
// Used by OnStorageSpinner to track storage resource assignments.
typedef void* (*fn_StorageTree_Insert_t)(void* tree, void* retval, void* entry);
extern fn_StorageTree_Insert_t fn_StorageTree_Insert;  // 0x14000e1c0

// FUN_14005baa0: Building_RemoveBuyOrder(building, resource_entity, qty) — remove buy order
// Used by OnStorageSpinner when removing an item from buy list.
typedef void (*fn_Building_RemoveBuyOrder_t)(void* building, void* resourceEntity, int qty);
extern fn_Building_RemoveBuyOrder_t fn_Building_RemoveBuyOrder;  // 0x14005baa0

// FUN_14005ace0: Building_AddBuyToStorage(building, resource_entity, qty)
// Used by OnStorageSpinner when a storage resource is assigned for buying.
typedef void (*fn_Building_AddBuyToStorage_t)(void* building, void* resourceEntity, int qty);
extern fn_Building_AddBuyToStorage_t fn_Building_AddBuyToStorage;  // 0x14005ace0

// FUN_140059490: Building_AddPurchaseOrder(building, resource_entity, enabled)
// Sets/updates a purchase order entry. Used by OnPurchaseButton.
typedef void (*fn_Building_AddPurchaseOrder_t)(void* building, void* resourceEntity, int8_t enabled);
extern fn_Building_AddPurchaseOrder_t fn_Building_AddPurchaseOrder;  // 0x140059490

// ============================================================
// NoiseUI helpers (Application-x64.exe)
// ============================================================

// NoiseUI ImageBuffer vtable (0x14015c8c0 in exe .rdata) — custom vtable
// set on ImageBuffer after construction. Resolved at runtime.
extern uintptr_t g_NoiseUI_ImageBufferVtable;

// NoiseUI helpers — REIMPLEMENTED in code/Game/Dialog/noiseui.cpp
// fn_NoiseUI_SetupImage (0x140071890) — now NoiseUI_SetupImage()
// fn_NoiseUI_GenerateNoise (0x1400711D0) — now NoiseUI_GenerateNoise()

// NoiseUI runtime dependencies (resolved from Runtime-x64.dll exports)
// Utility::PerlinNoise::PerlinNoise(float, float, int, int)
typedef void* (*fn_PerlinNoise_Ctor_t)(void* self, float persistance, float octaves,
                                        int seed1, int seed2);
extern fn_PerlinNoise_Ctor_t fn_PerlinNoise_Ctor;

// Utility::PerlinNoise::Generate(float, float) const -> float
typedef float (*fn_PerlinNoise_Generate_t)(const void* self, float x, float y);
extern fn_PerlinNoise_Generate_t fn_PerlinNoise_Generate;

// System::Random::Random() — default ctor (seeds from time), uses fn_Random_Default_Ctor

// System::Random::RandTol(int) -> int — returns random in [-range, +range]
typedef int (*fn_Random_RandTol_t)(void* self, int range);
extern fn_Random_RandTol_t fn_Random_RandTol;

// System::String::ToFloat(const char*) -> float — static, parses string to float
typedef float (*fn_String_ToFloat_t)(const char* str);
extern fn_String_ToFloat_t fn_String_ToFloat;

// ============================================================
// DebugUI support
// ============================================================

// Weather_ResetTime (0x1400944e0) — resets weather to a given time, updates all active weather
// entries (geometry fade, audio, lightning), recalculates day index and time transition.
// RCX=weatherMgrPtr(longlong*), RDX=timePtr(longlong*)
typedef void (*fn_Weather_ResetTime_t)(void* weatherMgrPtr, void* timePtr);
extern fn_Weather_ResetTime_t fn_Weather_ResetTime;

// UI::UserInterface::SetCursor(this, ExternalPtr<Cursor>) — sets active cursor
// Ordinal 3865: ?SetCursor@UserInterface@UI@@QEAAXV?$ExternalPtr@VCursor@UI@@@Object@@@Z
typedef void (*fn_UI_SetCursor_t)(void* self, uint64_t cursorPtr);
extern fn_UI_SetCursor_t fn_UI_SetCursor;

// UI::UserInterface::SetFullscreenFade(this, fadeIn, color, reverse) — controls fade overlay
// Ordinal 3887: ?SetFullscreenFade@UserInterface@UI@@QEAAX_NI0@Z
typedef void (*fn_UI_SetFullscreenFade_t)(void* self, bool fadeIn, uint32_t color, bool reverse);
extern fn_UI_SetFullscreenFade_t fn_UI_SetFullscreenFade;

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

// Game_SpawnResources (0x140091660) — REPLACED with Game_SpawnResources_Impl (Phase H)
// See inline Game_SpawnResources_Impl() near end of this file.
// Old fn_* pointer kept for backward compat; callers should use Game_SpawnResources_Impl().
typedef void (*fn_Game_SpawnResources_t)(void* citizenList, uint32_t resourceFlag, int count);
extern fn_Game_SpawnResources_t fn_Game_SpawnResources;

// Game_TriggerDisasterByType (0x140060110) — REPLACED with Game_TriggerDisasterByType_Impl (Phase H)
// See inline Game_TriggerDisasterByType_Impl() near end of this file.
// Old fn_* pointer kept for backward compat; callers should use Game_TriggerDisasterByType_Impl().
typedef void (*fn_Game_TriggerDisasterByType_t)(int type);
extern fn_Game_TriggerDisasterByType_t fn_Game_TriggerDisasterByType;

// Game_DestroyAllRawMaterials (0x140060000) — destroys all loose raw material entities
typedef void (*fn_Game_DestroyAllRawMaterials_t)(void);
extern fn_Game_DestroyAllRawMaterials_t fn_Game_DestroyAllRawMaterials;

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

// DebugUI_OnDrawComboChanged — now reimplemented in code/Game/Dialog/debugui.cpp
// Sub-functions needed by the reimplementation:

// DrawGeometry_Ctor (FUN_1400d3f10) — constructs a DrawGeometry object (0xf0 bytes)
// Calls Graphics::Geometry::Geometry() base ctor, sets vtable, inits fields.
// RCX=allocated memory (0xf0 bytes) -> returns constructed object
typedef void* (*fn_DrawGeometry_Ctor_t)(void* mem);
extern fn_DrawGeometry_Ctor_t fn_DrawGeometry_Ctor;

// DrawGeometry_Init (FUN_1400d3f80) — initializes a DrawGeometry with descriptor data
// Generates random vertex positions, index buffers, sets transform/bounds/draw flags.
// RCX=geometry, RDX=descriptor ptr (from draw descriptor array entry +0x00)
typedef void (*fn_DrawGeometry_Init_t)(void* geometry, void* descriptor);
extern fn_DrawGeometry_Init_t fn_DrawGeometry_Init;

// AudioObjectGroup_Ctor (FUN_1400c3720) — constructs an audio object group (0x48 bytes)
// Creates Internal object, iterates audio descriptors, creates sub-objects via Factory.
// RCX=allocated memory (0x48 bytes), RDX=audio descriptor ptr -> returns constructed object
typedef void* (*fn_AudioObjectGroup_Ctor_t)(void* mem, void* audioDesc);
extern fn_AudioObjectGroup_Ctor_t fn_AudioObjectGroup_Ctor;

// AudioScene_RegisterObject (FUN_1400c48a0) — registers an audio object with the audio manager
// RCX=audioMgr (world+0xE8), RDX=InternalBasePtr* to audio object, R8D=sound type
typedef void (*fn_AudioScene_RegisterObject_t)(void* audioMgr, void* audioObjPtr, int soundType);
extern fn_AudioScene_RegisterObject_t fn_AudioScene_RegisterObject;

// Entity_FindAudioComponent (FUN_140002280) — finds audio component on an entity
// Searches entity's component list for audio component type (DAT_1401c3e60).
// RCX=entity, RDX=out InternalBasePtr -> returns InternalBasePtr*
// NOTE: member function ABI — RCX=this(entity), RDX=hidden retval ptr
typedef void* (*fn_Entity_FindAudioComponent_t)(void* entity, void* retval);
extern fn_Entity_FindAudioComponent_t fn_Entity_FindAudioComponent;

// Graphics::Scene::AddGlobalObject — adds a Geometry to the global scene render list
// RCX=Graphics::Scene*, RDX=Geometry*
typedef void (*fn_Graphics_Scene_AddGlobalObject_t)(void* scene, void* geometry);
extern fn_Graphics_Scene_AddGlobalObject_t fn_Graphics_Scene_AddGlobalObject;

// Graphics::Scene::AddObject — adds a per-instance Geometry; returns id (uint).
// RCX=Graphics::Scene*, RDX=Geometry*  ->  EAX = handle id.
typedef unsigned int (*fn_Graphics_Scene_AddObject_t)(void* scene, void* geometry);
extern fn_Graphics_Scene_AddObject_t fn_Graphics_Scene_AddObject;

// Graphics::GraphicsMesh::GetMaterialList(int) — returns a pointer to a
// Collection::Array<ExternalPtr<MaterialInstance>> stored at offset matIdx*0x10
// inside GraphicsMesh+0x70. Member function returning >8B uses MS x64 ABI:
// RCX=this(GraphicsMesh*), RDX=int matIdx → returns Array* in RAX.
// Array layout: +0x00 = data ptr (8-byte ExternalPtrs), +0x08 = count.
typedef void* (*fn_Graphics_GraphicsMesh_GetMaterialList_t)(void* self, int matIdx);
extern fn_Graphics_GraphicsMesh_GetMaterialList_t fn_Graphics_GraphicsMesh_GetMaterialList;

// Graphics::MeshInstance::SetMaterialInstance(int idx, ExternalPtr<MaterialInstance>)
// Writes the 8-byte ExternalPtr value into MeshInstance+0xC8 + idx*8.
// RCX=MeshInstance*, RDX=int, R8=ExternalPtr value (8 bytes).
typedef void (*fn_Graphics_MeshInstance_SetMaterialInstance_t)(void* self, int idx, void* matExtPtr);
extern fn_Graphics_MeshInstance_SetMaterialInstance_t fn_Graphics_MeshInstance_SetMaterialInstance;

// Graphics::ParticleSystemInstance::ParticleSystemInstance() — default ctor.
// Object size 0xe0, alignment 8.
typedef void* (*fn_Graphics_ParticleSystemInstance_Ctor_t)(void* self);
extern fn_Graphics_ParticleSystemInstance_Ctor_t fn_Graphics_ParticleSystemInstance_Ctor;

// Graphics::Particle::Particle() — default ctor for the Particle base class
// (Runtime 0x180103e80, export ??0Particle@Graphics@@QEAA@XZ). Initializes the
// [0x00,0x48) base region + base vtable. Used by the standalone Tornado factory
// (code/Game/Game/tornado.cpp), which then fills the Tornado-specific float
// block at +0x48.. — mirrors the exe ctor FUN_1400d23c0's first call.
typedef void* (*fn_Graphics_Particle_Ctor_t)(void* self);
extern fn_Graphics_Particle_Ctor_t fn_Graphics_Particle_Ctor;

// Graphics::ParticleInstance::IncrementInstanceIndex() — advances the
// Runtime's static _frameIndex counter. ParticleSystemInstance::Update
// early-returns if `emit+0x2c == _frameIndex`, so this MUST be called
// once per render frame for baked particle systems to actually simulate
// after their first update. Mirrors what Renderer::Render does in retail.
typedef void (*fn_Graphics_ParticleInstance_IncrementInstanceIndex_t)(void);
extern fn_Graphics_ParticleInstance_IncrementInstanceIndex_t fn_Graphics_ParticleInstance_IncrementInstanceIndex;

// Graphics::Scene::Clean(this) — walks scene objects and calls Geometry::Clean
// (vtable +0x40) on each that has a dirty bit set. Must be called once per
// frame after all per-instance Update calls; without it, transform/bounds
// updates don't propagate to the renderer's per-frame state, causing flicker.
typedef void (*fn_Graphics_Scene_Clean_t)(void* scene);
extern fn_Graphics_Scene_Clean_t fn_Graphics_Scene_Clean;

// ============================================================
// MenuScene 3D rendering — Runtime DLL Graphics::/Math::/Volume:: functions
// Used by code/Game/Game/menudialogloopt.cpp (Phase A scene background).
// ============================================================

// Graphics::Renderer::Renderer() — default ctor. Object size 0x50, alignment 4.
typedef void* (*fn_Graphics_Renderer_Ctor_t)(void* self);
extern fn_Graphics_Renderer_Ctor_t fn_Graphics_Renderer_Ctor;

// Graphics::Scene::Scene() — default ctor. Object size 0x78, alignment 8.
typedef void* (*fn_Graphics_Scene_Ctor_t)(void* self);
extern fn_Graphics_Scene_Ctor_t fn_Graphics_Scene_Ctor;

// Graphics::SunLight::SunLight() — default ctor. Object size 0x108, alignment 8.
typedef void* (*fn_Graphics_SunLight_Ctor_t)(void* self);
extern fn_Graphics_SunLight_Ctor_t fn_Graphics_SunLight_Ctor;

// Graphics::SunLight::Set(direction, color, ambient) — set lighting parameters.
// RCX=SunLight*, RDX=Vector3* (direction), R8=Vector4* (sun color), R9=Vector4* (ambient color).
typedef void (*fn_Graphics_SunLight_Set_t)(void* self, void* direction, void* sunColor, void* ambient);
extern fn_Graphics_SunLight_Set_t fn_Graphics_SunLight_Set;

// Graphics::MeshInstance::MeshInstance() — default ctor. Object size 0x120, alignment 8.
typedef void* (*fn_Graphics_MeshInstance_Ctor_t)(void* self);
extern fn_Graphics_MeshInstance_Ctor_t fn_Graphics_MeshInstance_Ctor;

// Graphics::MeshInstance::SetMesh(meshExternalPtr) — assign mesh resource.
// RCX=MeshInstance*, RDX=ExternalPtr<GraphicsMesh> (8-byte external ptr value).
typedef void (*fn_Graphics_MeshInstance_SetMesh_t)(void* self, void* meshPtr);
extern fn_Graphics_MeshInstance_SetMesh_t fn_Graphics_MeshInstance_SetMesh;

// Graphics::Renderer::Draw(context, alpha, scene, camera) — main render call.
// RCX=Renderer*, RDX=Video::Context*, XMM2=alpha, R9=Scene*, [stack+0x20]=Camera*.
typedef void (*fn_Graphics_Renderer_Draw_t)(void* self, void* context, float alpha, void* scene, void* camera);
extern fn_Graphics_Renderer_Draw_t fn_Graphics_Renderer_Draw;

// Math::Vector3::FromYawPitch(retVec, yaw, pitch) — constructs a direction
// Vector3 from yaw + pitch (each a `Math::Scalar const&` = float*).
// Static fn returning struct >8B → RCX=retVec (hidden return), RDX=yaw,
// R8=pitch. Note: Ghidra often drops R8 in the decompile — verified via
// disassembly of MainMenuDialog_helper_83f0 (0x1400983f0) and MenuDLT::Draw
// (0x14009b440), both of which set R8 explicitly via LEA before the call.
typedef void* (*fn_Math_Vector3_FromYawPitch_t)(void* retVec, void* yaw, void* pitch);
extern fn_Math_Vector3_FromYawPitch_t fn_Math_Vector3_FromYawPitch;

// Math::Transform::FromLookAt(retval, eye, target) — static fn that builds a
// compact Math::Transform (28B = pos vec3 + quat) looking from `eye` toward
// `target`. Used by the menu camera derivation (MainMenuDialog_helper_83f0 at
// exe 0x1400983f0). Mangled: ?FromLookAt@Transform@Math@@SA?AV12@AEBVVector3@2@0@Z
// Static fn returning struct >8B → RCX=retval, RDX=eye, R8=target.
typedef void* (*fn_Math_Transform_FromLookAt_t)(void* retval, void* eye, void* target);
extern fn_Math_Transform_FromLookAt_t fn_Math_Transform_FromLookAt;

// Video::Context::SetFog(fogStart, fogEnd, fogColorVec4) — push fog params.
// RCX=Context*, XMM1=fogStart, XMM2=fogEnd, R9=Vector4*.
typedef void (*fn_Video_Context_SetFog_t)(void* context, float fogStart, float fogEnd, void* fogColor);
extern fn_Video_Context_SetFog_t fn_Video_Context_SetFog;

// Math::Transform::Transform() — default ctor. Object size 0x10C (0x34 transform + 0xD8 camera).
typedef void* (*fn_Math_Transform_Ctor_t)(void* self);
extern fn_Math_Transform_Ctor_t fn_Math_Transform_Ctor;

// Math::Transform3::Identity() — returns pointer to a static identity transform.
typedef void* (*fn_Math_Transform3_Identity_t)();
extern fn_Math_Transform3_Identity_t fn_Math_Transform3_Identity;

// Volume::Frustum::Frustum(self, near, far, fovX, fovY) — perspective frustum.
// Arg ORDER (verified via FUN_140098800 + Frustum::Frustum @ Runtime 0x1800fa1e0):
//   RCX=self, RDX=near (Scalar*), R8=far (Scalar*), R9=fovX (Scalar*), [+0x20]=fovY (Scalar*).
// near/far are stored as raw plane distances; fovX/fovY are half-angles in degrees
// (used for sin/cos when computing side-plane normals).
typedef void* (*fn_Volume_Frustum_Ctor_t)(void* self, void* nearZ, void* farZ, void* fovX, void* fovY);
extern fn_Volume_Frustum_Ctor_t fn_Volume_Frustum_Ctor;

// Graphics::Camera::Camera(frustum, transform3, isPerspective) — non-default ctor.
// RCX=self, RDX=Frustum*, R8=Transform3*, R9b=isPerspective bool.
typedef void* (*fn_Graphics_Camera_Ctor_t)(void* self, void* frustum, void* transform3, bool isPerspective);
extern fn_Graphics_Camera_Ctor_t fn_Graphics_Camera_Ctor;

// Graphics::Camera::SetFrustum(frustum) — replace the camera's frustum.
typedef void (*fn_Graphics_Camera_SetFrustum_t)(void* self, void* frustum);
extern fn_Graphics_Camera_SetFrustum_t fn_Graphics_Camera_SetFrustum;

// Audio::Scene::MoveEmitter — moves an audio emitter from old to new position
// RCX=Audio::Scene*, EDX=emitterId, R8=oldPos(Vector3*), R9=newPos(Vector3*)
typedef void (*fn_Audio_Scene_MoveEmitter_t)(void* scene, uint32_t emitterId, void* oldPos, void* newPos);
extern fn_Audio_Scene_MoveEmitter_t fn_Audio_Scene_MoveEmitter;

// Audio::Scene::SetEmitterVolume — sets volume on an audio emitter
// RCX=Audio::Scene*, EDX=emitterId, R8=pos(Vector3*), XMM3=volume(float)
typedef void (*fn_Audio_Scene_SetEmitterVolume_t)(void* scene, uint32_t emitterId, void* pos, float volume);
extern fn_Audio_Scene_SetEmitterVolume_t fn_Audio_Scene_SetEmitterVolume;

// --- Audio::Scene 3D emitter API (ambient environment + building emitters) ---
// These back the AmbientEnvironmentComponent / AmbientEmitterComponent audio
// (biome wind/nature beds, per-building loops). All are Runtime-x64.dll exports;
// resolved by mangled name so both DLL and standalone builds share the pointer.

// Audio::Scene::FindEffect3D(ExternalPtr<SoundEffect>) -> uint handle (0 if none)
// RCX=this, RDX=soundRef (8-byte ExternalPtr by value).
typedef uint32_t (*fn_Audio_Scene_FindEffect3D_t)(void* scene, uint64_t soundRef);
extern fn_Audio_Scene_FindEffect3D_t fn_Audio_Scene_FindEffect3D;

// Audio::Scene::PlayEffect3D(ExternalPtr<SoundEffect>, SoundType, const Time&, const Vector3&) -> uint handle
// RCX=this, RDX=soundRef, R8D=soundType(3), R9=Time*, [rsp+20h]=Vector3* pos.
typedef uint32_t (*fn_Audio_Scene_PlayEffect3D_t)(void* scene, uint64_t soundRef, int soundType,
                                                  const void* timePtr, const void* posVec3);
extern fn_Audio_Scene_PlayEffect3D_t fn_Audio_Scene_PlayEffect3D;

// Audio::Scene::AddEmitter(uint id, const Vector3&, float vol)
// RCX=this, EDX=id, R8=Vector3*, XMM3=vol.
typedef void (*fn_Audio_Scene_AddEmitter_t)(void* scene, uint32_t id, const void* posVec3, float vol);
extern fn_Audio_Scene_AddEmitter_t fn_Audio_Scene_AddEmitter;

// Audio::Scene::RemoveEmitter(uint id, const Vector3&)
typedef void (*fn_Audio_Scene_RemoveEmitter_t)(void* scene, uint32_t id, const void* posVec3);
extern fn_Audio_Scene_RemoveEmitter_t fn_Audio_Scene_RemoveEmitter;

// Audio::Scene::GetEmitterCount(uint id) const -> int
typedef int (*fn_Audio_Scene_GetEmitterCount_t)(void* scene, uint32_t id);
extern fn_Audio_Scene_GetEmitterCount_t fn_Audio_Scene_GetEmitterCount;

// Audio::Scene::StopEffect3D(uint id)
typedef void (*fn_Audio_Scene_StopEffect3D_t)(void* scene, uint32_t id);
extern fn_Audio_Scene_StopEffect3D_t fn_Audio_Scene_StopEffect3D;

// Math::Vector3::operator== — compares two Vector3 for equality
// RCX=lhs(Vector3*), RDX=rhs(Vector3*) -> returns bool
typedef bool (*fn_Vector3_Equals_t)(void* lhs, void* rhs);
extern fn_Vector3_Equals_t fn_Vector3_Equals;

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
// DebugUI deep-reimpl helper functions (exe sub-functions)
// ============================================================

// FireComponent type ID — DAT_1401c41e0 (used by FindComponent_Fire)
extern uint32_t* g_TypeId_FireComponent;

// fn_FindComponent_Fire — searches entity for FireComponent (DAT_1401c41e0)
inline void fn_FindComponent_Fire(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_FireComponent);
}

// fn_FindComponent_RawMaterial — searches entity for RawMaterialDescription (DAT_1401c7cb8)
inline void fn_FindComponent_RawMaterial(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_RawMaterialDescription);
}

// Game_DestroyEntity (FUN_1400c3ec0 in exe) — destroys an entity from the entity list
// RCX=entityList, RDX=entity
typedef void (*fn_Game_DestroyEntity_t)(void* entityList, void* entity);
extern fn_Game_DestroyEntity_t fn_Game_DestroyEntity;

// FireComponent_Ignite (0x140049610 in exe) — sets a building on fire
// RCX=fireComponent
typedef void (*fn_FireComponent_Ignite_t)(void* self);
extern fn_FireComponent_Ignite_t fn_FireComponent_Ignite;

// DiseaseComponent_Infect (exe) — infects a citizen with disease
// RCX=diseaseComponent
typedef void (*fn_DiseaseComponent_Infect_t)(void* self);
extern fn_DiseaseComponent_Infect_t fn_DiseaseComponent_Infect;

// StorageComponent_GetCapacity (FUN_140128570) — returns storage capacity
// RCX=storageComponent
typedef int (*fn_StorageComponent_GetCapacity_t)(void* self);
extern fn_StorageComponent_GetCapacity_t fn_StorageComponent_GetCapacity;

// StorageComponent_SpawnItem (FUN_140128670) — spawns one item in storage
// RCX=storageComponent, RDX=retval (InternalBasePtr)
typedef void (*fn_StorageComponent_SpawnItem_t)(void* self, void* retval);
extern fn_StorageComponent_SpawnItem_t fn_StorageComponent_SpawnItem;

// EntityFactory_CreateEntity (FUN_1400c3720) — creates a new entity from an external template
// RCX=memBlock(allocated 0x48 bytes), RDX=externalTemplate
typedef void* (*fn_EntityFactory_CreateEntity_t)(void* memBlock, void* externalTemplate);
extern fn_EntityFactory_CreateEntity_t fn_EntityFactory_CreateEntity;

// EntityList_RegisterEntity (FUN_1400c48a0) — registers entity with the entity list
// RCX=entityList, RDX=entityPtrRef(InternalBasePtr*), R8D=categoryId
typedef void (*fn_EntityList_RegisterEntity_t)(void* entityList, void* entityPtrRef, int categoryId);
extern fn_EntityList_RegisterEntity_t fn_EntityList_RegisterEntity;

// ResidenceComponent type ID — DAT_1401c7900 (used by SpawnCitizens)
extern uint32_t* g_TypeId_ResidenceComponent_FC;

// fn_FindComponent_ResidenceFC — searches entity for ResidenceComponent (DAT_1401c7900)
inline void fn_FindComponent_ResidenceFC(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_ResidenceComponent_FC);
}

// ResidenceDescription type ID — DAT_1401c7938 (used by SpawnCitizens)
extern uint32_t* g_TypeId_ResidenceDescription;

// ResidenceComponent_GetOccupantCount (FUN_140123070) — returns current occupant count
// RCX=residenceComponent, EDX=mode(0=current)
typedef int (*fn_ResidenceComponent_GetOccupantCount_t)(void* self, int mode);
extern fn_ResidenceComponent_GetOccupantCount_t fn_ResidenceComponent_GetOccupantCount;

// ResidenceComponent_GetSpawnPoint (FUN_140122e20) — gets spawn point for citizen creation
// RCX=residenceComponent, RDX=outVec3, R8D=param3, R9D=idx, [rsp+0x28]=outPtr
typedef void (*fn_ResidenceComponent_GetSpawnPoint_t)(void* self, void* outVec3, int p3, int idx, float* outParam);
extern fn_ResidenceComponent_GetSpawnPoint_t fn_ResidenceComponent_GetSpawnPoint;

// CitizenComponent_Setup (FUN_140117080) — initializes a new citizen entity
// Complex signature: self, position, gender, p4, age, p6, parentPtr, descPtr
typedef void (*fn_CitizenComponent_Setup_t)(void* self, void* pos, uint32_t gender, char p4, float age, char p6, void* parentPtr, void* descPtr);
extern fn_CitizenComponent_Setup_t fn_CitizenComponent_Setup;

// Random::RandFloat (already declared as fn_Random_RandFloat)
// Random::RandFloatTol — not needed here

// DisasterComponent_Infect (FUN_140116b90) — applies disease disaster to a building
// RCX=building, RDX=diseaseDescription
typedef void (*fn_DisasterComponent_Infect_t)(void* building, void* diseaseDesc);
extern fn_DisasterComponent_Infect_t fn_DisasterComponent_Infect;

// FUN_1400893c0 — checks if building list has a condition (used by TriggerDisasterOnBuilding)
typedef bool (*fn_BuildingList_CheckCondition_t)(void* buildingList);
extern fn_BuildingList_CheckCondition_t fn_BuildingList_CheckCondition;

// ============================================================
// SaveLoadDialog::Load — game load loop construction helpers
// ============================================================

// GameUpdateLoop ctor (FUN_140098060 in exe) — NOW REIMPLEMENTED in saveloaddialog.cpp
// Constructs the gameplay UpdateLoop (0x160 bytes).
// Calls Engine::UpdateLoop::UpdateLoop then sets vtable to PTR_FUN_14015e2f0.
// Old typedef kept for reference:
// typedef void* (*fn_GameUpdateLoop_Ctor_t)(void* self, void* savedState, void* loaderLoopPtr,
//                                            uint32_t opFlag, void* nameStr,
//                                            uint32_t p6, uint32_t p7, uint8_t p8,
//                                            uint64_t p9, uint64_t p10, uint64_t p11);
// extern fn_GameUpdateLoop_Ctor_t fn_GameUpdateLoop_Ctor;

// Engine::UpdateLoop::UpdateLoop(this) — base ctor from Runtime DLL
// ??0UpdateLoop@Engine@@QEAA@XZ
typedef void* (*fn_UpdateLoop_Ctor_t)(void* self);
extern fn_UpdateLoop_Ctor_t fn_UpdateLoop_Ctor;

// GameUpdateLoop vtable: PTR_FUN_14015e2f0 — written after UpdateLoop base ctor
extern void** g_GameUpdateLoop_Vtable;  // 0x14015e2f0

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

// Engine::DialogLoopT::DialogLoopT(self, dialogFilename) — Runtime base ctor
// ??0DialogLoopT@Engine@@QEAA@AEBVString@System@@@Z (Runtime 0x1800db3f0)
// Calls UpdateLoop::UpdateLoop, patches DialogLoopT vtable, copies string, inits UI::UserInterface.
typedef void* (*fn_DialogLoopT_Ctor_t)(void* self, void* dialogFilename);
extern fn_DialogLoopT_Ctor_t fn_DialogLoopT_Ctor;

// Engine::DialogLoopT::Update(self, Time const&) — Runtime base virtual Update
// ?Update@DialogLoopT@Engine@@UEAA_NAEBVTime@System@@@Z (Runtime 0x1800db270)
// Body: calls UI::UserInterface::Update on the embedded UI at self+0x40, returns true.
typedef bool (*fn_DialogLoopT_Update_t)(void* self, void* time);
extern fn_DialogLoopT_Update_t fn_DialogLoopT_Update;

// Engine::DialogLoopT::Startup(self) — Runtime base virtual Startup
// ?Startup@DialogLoopT@Engine@@UEAAXXZ (Runtime 0x1800db2f0)
// Calls vtable[12]/CreateDialog to construct the dialog instance, stores at
// self+0x38 (m_dialogInstance), invokes the dialog's Slot4 (OnOpened/Setup),
// then pushes the InternalPtr into the UI's pending-open dialog array (+0x68).
typedef void (*fn_DialogLoopT_Startup_t)(void* self);
extern fn_DialogLoopT_Startup_t fn_DialogLoopT_Startup;

// Engine::DialogLoopT::Load(self) — Runtime base virtual Load (vtable +0x30)
// ?Load@DialogLoopT@Engine@@UEAAXXZ (Runtime 0x1800db140)
// Reads m_dialogFilename from self+0x28, calls Filename::AdjustDelimiters, then
// ExternalFactory::FindOrLoad on the path. Stores the resulting External (or null
// if IsDerivedFrom<Dialog::Description> fails) at self+0x30 (m_dialogExternal).
// Subclass overrides (e.g., MenuDLT::Load) typically tail-call this then load
// extra companion resources (audio rsc, scene rsc, particles).
typedef void (*fn_DialogLoopT_Load_t)(void* self);
extern fn_DialogLoopT_Load_t fn_DialogLoopT_Load;

// Engine::DialogLoaderLoopT::Startup(self) — Runtime DialogLoaderLoopT virtual Startup
// ?Startup@DialogLoaderLoopT@Engine@@UEAAXXZ (Runtime 0x1800db000)
// Calls vtable[+0x60]/CreateDialog to build the dialog instance, stores at
// self+0x58 (m_dialogInstance), invokes the dialog's Slot4 (Setup/OnOpened),
// pushes IBP into the UI's pending-open dialog array at self+0x88, then calls
// Loader::Load(LoaderLoop::_loader, m_nextLoop). Used by both the ShiningRock
// splash and the Loading screen.
typedef void (*fn_DialogLoaderLoopT_Startup_t)(void* self);
extern fn_DialogLoaderLoopT_Startup_t fn_DialogLoaderLoopT_Startup;

// Engine::DialogLoaderLoopT::Load(self) — Runtime base virtual Load (vtable +0x30)
// ?Load@DialogLoaderLoopT@Engine@@UEAAXXZ (Runtime 0x1800dad50)
// Reads m_dialogFilename @ +0x48, calls Filename::AdjustDelimiters, then
// ExternalFactory::FindOrLoad. Stores resulting External (or null on type
// mismatch) at +0x50 (m_dialogExternal). Same shape as DialogLoopT::Load
// but with the DialogLoaderLoopT field offsets.
typedef void (*fn_DialogLoaderLoopT_Load_t)(void* self);
extern fn_DialogLoaderLoopT_Load_t fn_DialogLoaderLoopT_Load;

// Audio::Scene::Scene() — Runtime default ctor (0x1800e0160)
// ??0Scene@Audio@@QEAA@XZ — initializes 0x38-byte Audio::Scene sub-object.
typedef void* (*fn_AudioScene_Ctor_t)(void* self);
extern fn_AudioScene_Ctor_t fn_AudioScene_Ctor;

// Exe vtable pointer for MenuDialogLoopT (subclass of Engine::DialogLoopT).
// PTR_FUN_14015ed00 — written to DialogLoopT after MenuDialogLoopT_Ctor.
extern void** g_MenuDialogLoopT_Vtable;  // 0x14015ed00

// MenuDialogLoopT_Ctor — FUN_14009bb00 — NOW REIMPLEMENTED INLINE
// Calls Engine::DialogLoopT::DialogLoopT(self, filename), then patches vtable to
// PTR_FUN_14015ed00 (MenuDialogLoopT subclass vtable), constructs System::Random at +0x168
// and Audio::Scene at +0xb30, then zeroes extra fields (+0xb68..+0xbf8).
// Allocate 0xc00 bytes before calling. Used to create the MainMenu/tutorial dialog loop.
// Old typedef kept for reference:
// typedef void* (*fn_MenuDialogLoopT_Ctor_t)(void* self, void* dialogFilename);
// extern fn_MenuDialogLoopT_Ctor_t fn_MenuDialogLoopT_Ctor;
inline void* MenuDialogLoopT_Ctor(void* self, void* dialogFilename) {
    // 1. Call base Engine::DialogLoopT ctor (initializes UpdateLoop, UI::UserInterface, etc.)
    fn_DialogLoopT_Ctor(self, dialogFilename);

    // 2. Patch vtable to MenuDialogLoopT exe subclass
    *(void**)self = (void*)g_MenuDialogLoopT_Vtable;

    // 3. Construct embedded sub-objects
    fn_Random_Default_Ctor((void*)((uintptr_t)self + 0x168));   // System::Random at +0x168
    fn_AudioScene_Ctor((void*)((uintptr_t)self + 0xb30));       // Audio::Scene at +0xb30

    // 4. Zero-init MenuDialogLoopT-specific fields (+0xb68..+0xbf8)
    uintptr_t s = (uintptr_t)self;
    for (uintptr_t off = 0xb68; off <= 0xbf8; off += 8)
        *(uint64_t*)(s + off) = 0;

    return self;
}

// SetEngineNextLoop — sets next loop on the MainLoop engine singleton
// FUN_140097fb0(enginePtr, loaderLoop): sets UI cursor, fullscreen fade, stores next loop ptr
// Called when self->m_loaderLoop1 (+0xE0) is null (use engine at +0xE8).
// REPLACED: was fn_SetEngineNextLoop (ResolveAddr(Module::Exe, 0x140097fb0))
// Now inline C++ — calls runtime UI functions (SetCursor, SetFullscreenFade).
typedef void (*fn_SetEngineNextLoop_t)(uintptr_t enginePtr, void* loaderLoop);  // kept for compat
inline void SetEngineNextLoop(uintptr_t enginePtr, void* loaderLoop) {
    void* ui = *(void**)(enginePtr + 0x98);
    // Set cursor to default (cursor ptr from KeyMap singleton+0x38)
    uint64_t cursorVal = *(uint64_t*)((uintptr_t)(*g_pKeyMapSingleton) + 0x38);
    if (fn_UI_SetCursor) fn_UI_SetCursor(ui, cursorVal);
    // Fade to black (fadeIn=true, color=0xFF000000=opaque black, reverse=false)
    if (fn_UI_SetFullscreenFade) fn_UI_SetFullscreenFade(ui, true, 0xFF000000, false);
    // Store next loop pointer at engine+0xC8
    *(void**)(enginePtr + 0xC8) = loaderLoop;
}

// SetDialogLoaderNextLoop — FUN_14009a8f0 — NOW REIMPLEMENTED INLINE
// Sets fullscreen fade on DialogLoaderLoopT's UI (+0x40), stores next loop at +0xb68.
// Old typedef kept for reference:
// typedef void (*fn_SetDialogLoaderNextLoop_t)(uintptr_t dialogLoaderLoop, void* loaderLoop);
// extern fn_SetDialogLoaderNextLoop_t fn_SetDialogLoaderNextLoop;
inline void SetDialogLoaderNextLoop(uintptr_t dialogLoaderLoop, void* loaderLoop) {
    fn_UI_SetFullscreenFade((void*)(dialogLoaderLoop + 0x40), true, 0xFF000000, false);
    *(void**)(dialogLoaderLoop + 0xb68) = loaderLoop;
}

// GetLoadingSoundString — FUN_140099130 — NOW REIMPLEMENTED INLINE
// Creates a Random, generates rand 0-11, formats "Dialog/Loading.rsc:gameLoad%i".
// Old typedef kept for reference:
// typedef void* (*fn_GetLoadingSoundString_t)(uint64_t unused, void* retStr,
//                                              uint64_t unused2, uint64_t unused3);
// extern fn_GetLoadingSoundString_t fn_GetLoadingSoundString;
inline void* GetLoadingSoundString(void* retStr) {
    uint8_t rng[2520];  // System::Random is large (~2512 bytes)
    fn_Random_Default_Ctor(rng);
    uint32_t variant = fn_Random_Rand(rng, 12);
    fn_String_Format(retStr, "Dialog/Loading.rsc:gameLoad%i", variant);
    return retStr;
}

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
// Stage 3: OptionsDialog Runtime exports
// ============================================================

// --- Audio::Configuration ---

// Audio::Configuration::Save(this)
// ?Save@Configuration@Audio@@QEAAXXZ
typedef void (*fn_AudioConfig_Save_t)(void* self);
extern fn_AudioConfig_Save_t fn_AudioConfig_Save;

// Audio::Configuration::Startup(this) -> bool
// ?Startup@Configuration@Audio@@QEAA_NXZ
typedef bool (*fn_AudioConfig_Startup_t)(void* self);
extern fn_AudioConfig_Startup_t fn_AudioConfig_Startup;

// --- Video::Configuration ---

// Video::Configuration::Save(this)
// ?Save@Configuration@Video@@QEAAXXZ
typedef void (*fn_VideoConfig_Save_t)(void* self);
extern fn_VideoConfig_Save_t fn_VideoConfig_Save;

// --- Input::Configuration ---

// Input::Configuration::Save(this)
// ?Save@Configuration@Input@@QEAAXXZ
typedef void (*fn_InputConfig_Save_t)(void* self);
extern fn_InputConfig_Save_t fn_InputConfig_Save;

// Input::Configuration::Load(this)
// ?Load@Configuration@Input@@QEAAXXZ
typedef void (*fn_InputConfig_Load_t)(void* self);
extern fn_InputConfig_Load_t fn_InputConfig_Load;

// Input::Configuration::_singleton
extern void** g_pInputConfigSingleton;

// --- IO::Persistence ---

// IO::Persistence::WriteSettings(this) const
// ?WriteSettings@Persistence@IO@@QEBAXXZ
typedef void (*fn_Persistence_WriteSettings_t)(const void* self);
extern fn_Persistence_WriteSettings_t fn_Persistence_WriteSettings;

// IO::Persistence::ReadSettings(this)
// ?ReadSettings@Persistence@IO@@QEAAXXZ
typedef void (*fn_Persistence_ReadSettings_t)(void* self);
extern fn_Persistence_ReadSettings_t fn_Persistence_ReadSettings;

// --- Input::Device ---

// Input::Device::EnableMouseClipping(this, bool)
// ?EnableMouseClipping@Device@Input@@QEAAX_N@Z
typedef void (*fn_InputDevice_EnableMouseClipping_t)(void* self, bool enable);
extern fn_InputDevice_EnableMouseClipping_t fn_InputDevice_EnableMouseClipping;

// Input::Device::ResetState(this)
// ?ResetState@Device@Input@@QEAAXXZ
typedef void (*fn_InputDevice_ResetState_t)(void* self);
extern fn_InputDevice_ResetState_t fn_InputDevice_ResetState;

// --- Video::Device ---

// Video::Device::ApplyChangedSettings(this) -> bool
// ?ApplyChangedSettings@Device@Video@@QEAA_NXZ
typedef bool (*fn_VideoDevice_ApplyChangedSettings_t)(void* self);
extern fn_VideoDevice_ApplyChangedSettings_t fn_VideoDevice_ApplyChangedSettings;

// Video::Device::Startup(this) -> bool
// ?Startup@Device@Video@@QEAA_NXZ
typedef bool (*fn_VideoDevice_Startup_t)(void* self);
extern fn_VideoDevice_Startup_t fn_VideoDevice_Startup;

// Video::Device::RecreateDynamic(this)
// ?RecreateDynamic@Device@Video@@QEAAXXZ
typedef void (*fn_VideoDevice_RecreateDynamic_t)(void* self);
extern fn_VideoDevice_RecreateDynamic_t fn_VideoDevice_RecreateDynamic;

// Video::Device::ReleaseDynamic(this)
// ?ReleaseDynamic@Device@Video@@QEAAXXZ
typedef void (*fn_VideoDevice_ReleaseDynamic_t)(void* self);
extern fn_VideoDevice_ReleaseDynamic_t fn_VideoDevice_ReleaseDynamic;

// Video::Device::Shutdown(this)
// ?Shutdown@Device@Video@@QEAAXXZ
typedef void (*fn_VideoDevice_Shutdown_t)(void* self);
extern fn_VideoDevice_Shutdown_t fn_VideoDevice_Shutdown;

// --- Engine::MainLoop ---

// Engine::MainLoop::OnVideoError(this)
// ?OnVideoError@MainLoop@Engine@@QEAAXXZ
typedef void (*fn_MainLoop_OnVideoError_t)(void* self);
extern fn_MainLoop_OnVideoError_t fn_MainLoop_OnVideoError;

// Engine::MainLoop::OnVideoRestart(this)
// ?OnVideoRestart@MainLoop@Engine@@QEAAXXZ
typedef void (*fn_MainLoop_OnVideoRestart_t)(void* self);
extern fn_MainLoop_OnVideoRestart_t fn_MainLoop_OnVideoRestart;

// Engine::MainLoop::SetNextLoop(this, UpdateLoop*)
// ?SetNextLoop@MainLoop@Engine@@QEAAXPEAVUpdateLoop@2@@Z  (Runtime 0x1800dd7a0)
typedef void (*fn_MainLoop_SetNextLoop_t)(void* self, void* loop);
extern fn_MainLoop_SetNextLoop_t fn_MainLoop_SetNextLoop;

// Object::BaseTypeInfo::BaseTypeInfo(this, name, parent, abstract, factory, inPlace)
// ??0BaseTypeInfo@Object@@QEAA@PEBD00P6APEAVBase@1@XZP6APEAV21@PEAX@Z@Z  (Runtime 0x180006320)
// Builds the linked list at Runtime DLL's _firstTypeInfo. Object::Factory::Startup
// later walks this list to populate the typeId tree. The 6-arg ctor stores
// (name, parent[colon-stripped], abstract, factoryFn, inPlaceCreateFn) into the
// 0x38-byte BaseTypeInfo slot pointed to by `self`, then prepends to _firstTypeInfo.
typedef void (*fn_BaseTypeInfo_Ctor_t)(void* self,
                                       const char* name,
                                       const char* parent,
                                       const char* abstract,
                                       void* factoryFn,
                                       void* inPlaceCreateFn);
extern fn_BaseTypeInfo_Ctor_t fn_BaseTypeInfo_Ctor;

// Engine::MainLoop::_singleton (protected)
extern void** g_pMainLoopSingleton;

// --- UI::UserInterface ---

// UI::UserInterface::EnableCursor(this, bool)
// ?EnableCursor@UserInterface@UI@@QEAAX_N@Z
typedef void (*fn_UI_EnableCursor_t)(void* self, bool enable);
extern fn_UI_EnableCursor_t fn_UI_EnableCursor;

// Object::Internal type ID — exref at 0x140151660 in exe .rdata
// Used for IsDerivedFrom checks before InternalBasePtr wrapping / GetUserData
extern uint32_t* g_TypeId_ObjectInternal;

// ============================================================
// Stage 3: OptionsDialog Apply/Restore exe helpers
// ============================================================

// UI::UserInterface::GetFirstDialog(this, retval) — member, non-trivial return
// Returns LinkedList<InternalPtr<DialogInstance>>::Iterator (8-byte ptr)
// RCX=this, RDX=retval(Iterator*). Iterator+0x10 = next node.
// ?GetFirstDialog@UserInterface@UI@@QEAA?AUIterator@...@Collection@@XZ (0x180031150, ordinal 2670)
typedef void (*fn_UI_GetFirstDialog_t)(void* self, void* retval);
extern fn_UI_GetFirstDialog_t fn_UI_GetFirstDialog;

// UI::UserInterface::SetFullscreenFadeSprite(this, Sprite*) — sets fade sprite (16 bytes)
// ?SetFullscreenFadeSprite@UserInterface@UI@@QEAAXVSprite@2@@Z (0x18011f560, ordinal 3888)
typedef void (*fn_UI_SetFullscreenFadeSprite_t)(void* self, void* sprite);
extern fn_UI_SetFullscreenFadeSprite_t fn_UI_SetFullscreenFadeSprite;

// SaveGameSettings(gameSettings) — persists all game settings to IO::Persistence
// FUN_140098970 in exe (calls Persistence::Set for 16 entries)
typedef void (*fn_SaveGameSettings_t)(int32_t* gameSettings);
extern fn_SaveGameSettings_t fn_SaveGameSettings;

// ReadGameSettings(gameSettings) — reads all game settings from IO::Persistence
// FUN_140098b60 in exe (calls Persistence::Get for 16 entries)
typedef void (*fn_ReadGameSettings_t)(int32_t* gameSettings);
extern fn_ReadGameSettings_t fn_ReadGameSettings;

// ApplyKeyMapToUI(keyMapSingleton, userInterface) — copies keymap config fields to UI
// FUN_140099260 in exe: copies +0x58..+0x78, +0xA8/+0xB0 to UI, sets UI scale
typedef void (*fn_ApplyKeyMapToUI_t)(void* keyMap, void* userInterface);
extern fn_ApplyKeyMapToUI_t fn_ApplyKeyMapToUI;

// UpdateAutoSaveTimer(dialogInstance) — resets autosave timer based on g_pGameSettings[1]
// FUN_14005e120 in exe: reads autosave interval, starts/stops timer 0
typedef void (*fn_UpdateAutoSaveTimer_t)(void* dialogInstance);
extern fn_UpdateAutoSaveTimer_t fn_UpdateAutoSaveTimer;

// Type ID DAT_1401c4d08 — used for IsDerivedFrom check in ApplyGameSettings dialog iteration
extern uint32_t* g_TypeId_AutoSaveDialogType;

// RegisterDefaultInputBindings — clears KeyMap, re-registers all default bindings, then loads saved config
// FUN_1400992f0 in exe (standalone function, no parameters)
typedef void (*fn_RegisterDefaultInputBindings_t)();
extern fn_RegisterDefaultInputBindings_t fn_RegisterDefaultInputBindings;

// UI::UserInterface::ShowDialog(this, InternalPtr<DialogInstance>) -> int
// ?ShowDialog@UserInterface@UI@@QEAAHV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z
// ABI: RCX=this, RDX=ptr to InternalPtr (non-trivial by-value), returns int result code
typedef int (*fn_UI_ShowDialog_t)(void* self, void* dialogPtr);
extern fn_UI_ShowDialog_t fn_UI_ShowDialog;

// Object::InternalBasePtr::DeletePtr(this) — destroys internal object, nulls pointer
// ?DeletePtr@InternalBasePtr@Object@@QEAAXXZ
typedef void (*fn_InternalBasePtr_DeletePtr_t)(void* self);
extern fn_InternalBasePtr_DeletePtr_t fn_InternalBasePtr_DeletePtr;

// Input::KeyMap::SetInput(this, id, slot, device, key)
// ?SetInput@KeyMap@Input@@QEAAXHHHH@Z
typedef void (*fn_KeyMap_SetInput_t)(void* self, int id, int slot, int device, int key);
extern fn_KeyMap_SetInput_t fn_KeyMap_SetInput;

// Input::KeyMap::ClearInput(this, id, slot) — 2-param overload
// ?ClearInput@KeyMap@Input@@QEAAXHH@Z
typedef void (*fn_KeyMap_ClearInput2_t)(void* self, int id, int slot);
extern fn_KeyMap_ClearInput2_t fn_KeyMap_ClearInput2;

// Input::KeyMap::ClearInput(this, id, device, key) — 3-param overload
// ?ClearInput@KeyMap@Input@@QEAAXHHH@Z
typedef void (*fn_KeyMap_ClearInput3_t)(void* self, int id, int device, int key);
extern fn_KeyMap_ClearInput3_t fn_KeyMap_ClearInput3;

// Input::KeyMap::GetIdForDeviceInput(this, device, key) -> int
// ?GetIdForDeviceInput@KeyMap@Input@@QEBAHH H@Z
typedef int (*fn_KeyMap_GetIdForDeviceInput_t)(void* self, int device, int key);
extern fn_KeyMap_GetIdForDeviceInput_t fn_KeyMap_GetIdForDeviceInput;

// Input::KeyMap::RegisterInput(this, id, deviceType, keyCode, name, flag)
// ?RegisterInput@KeyMap@Input@@QEAAXHHHPEBD_N@Z (Runtime 0x1800a7e70)
// name is const char* (PEBD), NOT a String object.
typedef void (*fn_KeyMap_RegisterInput_t)(void* self, int id, int deviceType,
                                           int keyCode, const char* name, bool flag);
extern fn_KeyMap_RegisterInput_t fn_KeyMap_RegisterInput;

// Input::KeyMap::ClearAll(this)
// ?ClearAll@KeyMap@Input@@QEAAXXZ (Runtime 0x1800a7d20)
typedef void (*fn_KeyMap_ClearAll_t)(void* self);
extern fn_KeyMap_ClearAll_t fn_KeyMap_ClearAll;

// Input::KeyMap::Load(this) — loads saved bindings from Persistence over current map
// ?Load@KeyMap@Input@@QEAAXXZ (Runtime 0x1800a7190)
typedef void (*fn_KeyMap_Load_t)(void* self);
extern fn_KeyMap_Load_t fn_KeyMap_Load;

// OptionInputBindDialog ctor (0x140071d30) — creates 0xC8-byte dialog
// Params: (mem, userInterface, externalResource, internalBasePtrNull)
typedef void* (*fn_OptionInputBindDialog_Ctor_t)(void* mem, void* userInterface, void* external, void* internalBasePtr);
extern fn_OptionInputBindDialog_Ctor_t fn_OptionInputBindDialog_Ctor;

// MessageDialog ctor (0x14006d300) — creates 0xC0-byte dialog
// Params: (mem, userInterface, externalResource, internalBasePtrNull)
typedef void* (*fn_MessageDialog_Ctor_t)(void* mem, void* userInterface, void* external, void* internalBasePtr);
extern fn_MessageDialog_Ctor_t fn_MessageDialog_Ctor;

// ============================================================
// CaptureBoxElement / FrustumOutlineElement support
// (dispatch table reads from Application-x64.exe .rdata)
// ============================================================

// UI::ElementDescription::GetFileVersion(this) -> uint
// Called via exe dispatch table at 0x1401518c0 (Runtime export)
typedef uint32_t (*fn_ElementDescription_GetFileVersion_t)(void* self);
extern fn_ElementDescription_GetFileVersion_t fn_ElementDescription_GetFileVersion;

// UI::Element::Draw(this, SpriteContext*, byte alpha)
// Called via exe dispatch table at 0x1401518f8
typedef void (*fn_Element_Draw_t)(void* self, void* spriteCtx, uint8_t alpha);
extern fn_Element_Draw_t fn_Element_Draw;

// UI::SpriteContext::AddLine(this, Sprite*, x1, y1, x2, y2, color)
// Called via exe dispatch table at 0x140151908
typedef void (*fn_SpriteContext_AddLine_t)(void* self, void* sprite, int x1, int y1, int x2, int y2, uint32_t color);
extern fn_SpriteContext_AddLine_t fn_SpriteContext_AddLine;

// UI::SpriteContext::SetScissor(this, x, y, w, h) — 4-param overload
// Called via exe dispatch table at 0x140151900
typedef void (*fn_SpriteContext_SetScissor4_t)(void* self, int x, int y, int w, int h);
extern fn_SpriteContext_SetScissor4_t fn_SpriteContext_SetScissor4;

// UI::SpriteContext::SetScissor(this, Viewport*) — restore-from-viewport overload
// Called via exe dispatch table at 0x140151910
typedef void (*fn_SpriteContext_SetScissorVP_t)(void* self, void* viewport);
extern fn_SpriteContext_SetScissorVP_t fn_SpriteContext_SetScissorVP;

// FrustumOutlineElement frustum-to-screen projection helper (exe function)
// FUN_14006c5b0(outVec3Array) — reads camera frustum, projects 4 corners to screen-space
typedef void (*fn_FrustumProject_t)(void* outVec3Array);
extern fn_FrustumProject_t fn_FrustumProject;

// ============================================================
// Description class reimpl support (shared serialization helpers)
// ============================================================

// System::String::Serialize(this, Serializer*) — serialize/deserialize a String
typedef void (*g_fn_String_Serialize_t)(void* thisStr, void* serializer);
extern g_fn_String_Serialize_t g_fn_String_Serialize;

// UI::ElementDescription::Serialize(this, Stream*) — base class serialize
typedef void (*fn_ElementDescription_Serialize_t)(void* self, void* stream);
extern fn_ElementDescription_Serialize_t fn_ElementDescription_Serialize;

// UI::ElementDescription::~ElementDescription(this) — base destructor
typedef void (*fn_ElementDescription_Dtor_t)(void* self);
extern fn_ElementDescription_Dtor_t fn_ElementDescription_Dtor;

// UI::ElementDescription::ElementDescription(this) — base constructor
typedef void (*fn_ElementDescription_Ctor_t)(void* self);
extern fn_ElementDescription_Ctor_t fn_ElementDescription_Ctor;

// UI::ElementDescription::CreateInstance(this, InternalBasePtr*) — create element instance
typedef void (*fn_ElementDescription_CreateInstance_t)(void* self, void* ptrRef);
extern fn_ElementDescription_CreateInstance_t fn_ElementDescription_CreateInstance;

// UI::Element::Element(this) — Element base constructor
typedef void (*fn_Element_Ctor_t)(void* self);
extern fn_Element_Ctor_t fn_Element_Ctor;

// UI::Element::~Element(this) — Element base destructor (dispatch 0x1401518e0)
typedef void (*fn_Element_Dtor_t)(void* self);
extern fn_Element_Dtor_t fn_Element_Dtor;

// UI::Element::ComputeLayout(this, x, y, w, h) — base layout (dispatch 0x1401518e8)
typedef void (*fn_Element_ComputeLayout_t)(void* self, int x, int y, int w, int h);
extern fn_Element_ComputeLayout_t fn_Element_ComputeLayout;

// UI::Element::GetAlignedPosition(this, x, y, w, h, 0, 0, &outX, &outY, &outW, &outH, false, false)
// (dispatch 0x1401518f0)
typedef void (*fn_Element_GetAlignedPosition_t)(void* self, int x, int y, int w, int h,
    int p6, int p7, int* outX, int* outY, int* outW, int* outH, bool p11, bool p12);
extern fn_Element_GetAlignedPosition_t fn_Element_GetAlignedPosition;

// UI::Sprite::Sprite(retval, spriteSheet, spriteName) — Sprite ctor from sheet+name (16 bytes)
// Dispatch 0x1401514c8
typedef void* (*fn_Sprite_Sprite_t)(void* retval, uint64_t spriteSheet, const void* spriteName);
extern fn_Sprite_Sprite_t fn_Sprite_Sprite;

// UI::Sprite::Sprite(self) — default ctor (dispatch 0x140152b48)
typedef void (*fn_Sprite_DefaultCtor2_t)(void* self);
extern fn_Sprite_DefaultCtor2_t fn_Sprite_DefaultCtor2;

// Serializer encoding virtual dispatch helper.
// The Serializer struct (24 bytes) has an encoding sub-object at offset +0x08.
// Calling encoding->vtable[0x98/8](encoding, &field) serializes an ExternalPtr
// through the serializer's encoding layer.
// Usage: SerializerEncodingVCall(serializer_24bytes, 0x98, &field);
inline void SerializerEncodingVCall(void* serializer, int vtableOffset, void* field) {
    void* encoding = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(serializer) + 8);
    void* vtable = *reinterpret_cast<void**>(encoding);
    typedef void (*VFunc_t)(void*, void*);
    auto fn = *reinterpret_cast<VFunc_t*>(reinterpret_cast<uintptr_t>(vtable) + vtableOffset);
    fn(encoding, field);
}

// FUN_14000bd30(ExternalPtr*, Serializer*) — ExternalPtr serialize helper (exe function)
typedef void (*fn_ExternalPtr_Serialize_t)(void* externPtrField, void* serializer);
extern fn_ExternalPtr_Serialize_t fn_ExternalPtr_Serialize;

// FUN_14000bf40(ExternalPtr*, Serializer*) — ExternalPtr serialize variant (exe function)
typedef void (*fn_ExternalPtr_Serialize2_t)(void* externPtrField, void* serializer);
extern fn_ExternalPtr_Serialize2_t fn_ExternalPtr_Serialize2;

// FUN_140007140(ExternalPtr*, Serializer*) — another ExternalPtr serialize variant (exe)
// Used by RadiusDescription, SelectDescription, ValueDescription
typedef void (*fn_ExternalPtr_Serialize3_t)(void* externPtrField, void* serializer);
extern fn_ExternalPtr_Serialize3_t fn_ExternalPtr_Serialize3;

// FUN_1400049c0(Serializer*, count, arrayBase, param4) — External reference array serialize (exe)
typedef void (*fn_ExternalRefArray_Serialize_t)(void* serializer, int count, void* arrayBase, void* param4);
extern fn_ExternalRefArray_Serialize_t fn_ExternalRefArray_Serialize;

// IO::Serializer::Serialize(this, count, buffer, name) — bulk serialize bytes (exe dispatch)
typedef void (*fn_Serializer_SerializeBulk_t)(void* self, int count, void* buffer, const char* name);
extern fn_Serializer_SerializeBulk_t fn_Serializer_SerializeBulk;

// Component_Serialize(this, Stream*) — Component base serialize (exe function 0x14012e2a0 or similar)
typedef void (*fn_ComponentBase_Serialize_t)(void* self, void* stream);
extern fn_ComponentBase_Serialize_t fn_ComponentBase_Serialize;

// FUN_14000a590(arrayPtr, newCount) — dynamic array resize (exe function)
typedef void (*fn_DynArray_Resize_t)(void* arrayPtr, int newCount);
extern fn_DynArray_Resize_t fn_DynArray_Resize;

// FUN_14003f010(ptr, val, count) — memset-like zero fill (exe function)
typedef void (*fn_MemZeroFill_t)(void* ptr, int val, int count);
extern fn_MemZeroFill_t fn_MemZeroFill;

// FUN_140028860(arrayPtr, newCount) — String array resize for ToolbarDescription (exe)
typedef void (*fn_StringArray_Resize_t)(void* arrayPtr, int newCount);
extern fn_StringArray_Resize_t fn_StringArray_Resize;

// FUN_140002340(field, serializer) — ExternalPtr<BuildingDescription> field serialize (exe)
// Checks DAT_1401c7318 (BuildingDescription type id). Used by CreateDragDescription,
// CreateFencedDescription, and EnvironmentDescription Serialize.
typedef void (*fn_ExternalPtr_BuildSerialize_t)(void* field, void* serializer);
extern fn_ExternalPtr_BuildSerialize_t fn_ExternalPtr_BuildSerialize;

// FUN_1400198a0(arrayPtr, newCount) — Weather dynamic array resize (exe)
// Used by EnvironmentDescription::Serialize on load.
typedef void (*fn_WeatherResize_t)(void* arrayPtr, int newCount);
extern fn_WeatherResize_t fn_WeatherResize;

// FUN_1400196a0(entry, serializer) — Weather entry External serialize (exe)
// Serializes a weather dynamic array entry (External reference).
typedef void (*fn_WeatherExternalSerialize_t)(void* entry, void* serializer);
extern fn_WeatherExternalSerialize_t fn_WeatherExternalSerialize;

// FUN_14012db70(self) — ToolbarDescription internal cleanup helper (exe, unhookable)
// Sets vtable, destroys String array at +0x50, destroys strings at +0x48/+0x40/+0x38/+0x28,
// resets base vtable, calls External::~External. Called from ToolbarDescription::Dtor.
typedef void (*fn_ToolbarDescription_cleanup_t)(void* self);
extern fn_ToolbarDescription_cleanup_t fn_ToolbarDescription_cleanup;

// FUN_1400328c0(self) — CreateBridgeDescription::CreateInPlace (exe, unhookable, 12 bytes)
// Called from CreateBridgeDescription::Cleanup to re-construct in place.
typedef void (*fn_CreateBridgeDescription_Ctor_t)(void* self);
extern fn_CreateBridgeDescription_Ctor_t fn_CreateBridgeDescription_Ctor;

// FUN_1400329b0(self) — CreateDragDescription::CreateInPlace (exe, unhookable, 12 bytes)
// Called from CreateDragDescription::Cleanup to re-construct in place.
typedef void (*fn_CreateDragDescription_Ctor_t)(void* self);
extern fn_CreateDragDescription_Ctor_t fn_CreateDragDescription_Ctor;

// FUN_140004920(self) — CreateFencedDescription::CreateInPlace (exe, unhookable, 12 bytes)
// Called from CreateFencedDescription::Cleanup to re-construct in place.
typedef void (*fn_CreateFencedDescription_Ctor_t)(void* self);
extern fn_CreateFencedDescription_Ctor_t fn_CreateFencedDescription_Ctor;

// Object::Base::IsDerivedFrom(this, typeId) — already declared above but redeclared for clarity
// extern fn_IsDerivedFrom_t fn_IsDerivedFrom;  // already at line 177

// ============================================================
// TutorialSetupDialog helpers (exe-internal, no Runtime export)
// ============================================================

// FUN_140028650(External** fieldPtr, Serializer*) — serialize External* as a reference
// Handles reference-mode (id-based), path-based, and bool-presence modes.
typedef void (*fn_SerializeExternalPtr_t)(void* fieldPtr, void* serializer);
extern fn_SerializeExternalPtr_t fn_SerializeExternalPtr;

// FUN_1400e29f0(self) — TSD slot9 setup helper
// Calls SetDialogFlags(false) then shows each dialog in the 1st string vector.
typedef void (*fn_TSD_Slot9Helper_t)(void* self);
extern fn_TSD_Slot9Helper_t fn_TSD_Slot9Helper;

// FUN_1400e2700(self, param2) — open or validate tutorial dialog
// param2=true -> open dialog; false -> just validate state.
typedef void (*fn_TSD_OpenDialog_t)(void* self, char param2);
extern fn_TSD_OpenDialog_t fn_TSD_OpenDialog;

// FUN_1400e2330(self, param2) — set/clear 0x1000000 hide flag on all open dialog roots
// param2=true -> show (set flag); false -> hide (clear flag).
typedef void (*fn_TSD_SetDialogFlags_t)(void* self, char param2);
extern fn_TSD_SetDialogFlags_t fn_TSD_SetDialogFlags;

// FUN_1400e24f0(self, retval) — find current TutorialSetupDialog instance in open dialog list
// Writes InternalBasePtr to *retval; sets null if not found.
typedef void (*fn_TSD_FindInstance_t)(void* self, void* retval);
extern fn_TSD_FindInstance_t fn_TSD_FindInstance;

// FUN_1400e1600(retval, string_ptr) — find open dialog instance by type string
// Parses type name from string, walks open dialog list for matching IsA.
typedef void (*fn_TSD_FindByString_t)(void* retval, void* string_ptr);
extern fn_TSD_FindByString_t fn_TSD_FindByString;

// FUN_140101a80(playerController, retval, entityAsUlong) — set player follow-building
// Selects a building entity for the player's follow/inspection mode.
typedef void (*fn_SetFollowBuilding_t)(void* playerController, void* retval, uint64_t entityPtr);
extern fn_SetFollowBuilding_t fn_SetFollowBuilding;

// FUN_140102b60(playerController, mode) — set player controller edit mode
// mode=0 clears current selection/edit.
typedef void (*fn_SetEditMode_t)(void* playerController, uint64_t mode);
extern fn_SetEditMode_t fn_SetEditMode;

// ============================================================
// Phase G Wave 1: SaveLoadDialog DoLoad/DoSave/DoDelete + EventNotifyDialog NavigateToEvent
// ============================================================

// Save::SaveLoad::CheckMod(saveLoad, saveIndex, modIndex) — returns mod status code:
//   0 = ok, 2 = out of date, 3 = version mismatch, 4 = changed, other = missing.
// _singleton at 0x140152738 (Runtime export).
typedef int (*fn_SaveLoad_CheckMod_t)(void* saveLoad, int saveIndex, int modIndex);
extern fn_SaveLoad_CheckMod_t fn_SaveLoad_CheckMod;

// Save::SaveLoad::SetSaveName(saveLoad, String* name, int mode) — sets save name.
// mode=2 sets in save-name slot. Called before SetOperation(4,...).
// _singleton at 0x140151650 (Runtime export).
typedef void (*fn_SaveLoad_SetSaveName_t)(void* saveLoad, void* nameStr, int mode);
extern fn_SaveLoad_SetSaveName_t fn_SaveLoad_SetSaveName;

// DialogInstance type ID — used for IsDerivedFrom checks on loaded External resources
// to verify they are DialogInstance (or subclass). _typeInfo_exref at 0x140151728.
extern uint32_t* g_TypeId_DialogInstance;

// FUN_14009f3e0(mapGrid, outPos3, inXY2, flag, outTri) — terrain/scene pick from 2D position.
// Casts a ray from a 2D map position and returns the 3D terrain-intersection point.
// mapGrid: pointer to the map grid struct (int* piVar2 = *(DAT_1401c3dd8 + 0xf8)).
// outPos3: float[3] — receives (x, y, z) world position.
// inXY2:   float[3] — input position as {worldX, worldY, 0.0f}.
// flag:    '\0' = normal pick mode.
// outTri:  can be nullptr (receives picked triangle; ignored).
// Exe-only function at 0x14009f3e0.
typedef float* (*fn_MapGetTerrainPos_t)(void* mapGrid, float* outPos3, float* inXY2, char flag, void* outTri);
extern fn_MapGetTerrainPos_t fn_MapGetTerrainPos;

// ============================================================
// Phase H: Sub-dependency blockers — reimplemented inline
// ============================================================

// --- Object::Factory::GetTypeIdFromName ---
// Runtime DLL export ordinal 2991: ?GetTypeIdFromName@Factory@Object@@QEBAIPEBD_N@Z
// Looks up type name CRC in Factory BST. Returns type ID (uint32) or 0 if not found.
// RCX=this(Factory*), RDX=name(const char*), R8=includeBase(bool)
typedef uint32_t (*fn_Factory_GetTypeIdFromName_t)(void* self, const char* name, bool includeBase);
extern fn_Factory_GetTypeIdFromName_t fn_Factory_GetTypeIdFromName;

// Object::Factory::_singleton — dispatch table pointer at 0x140152760 in exe
// Double-indirect: *(void**)0x140152760 -> Factory*
extern void** g_pFactorySingleton;

// Object::Base::IsA(this, typeId) -> bool
// Runtime DLL export ordinal 3089: ?IsA@Base@Object@@QEBA_NI@Z
// Compares GetTypeId() against given typeId.
typedef bool (*fn_IsA_t)(void* self, uint32_t typeId);
extern fn_IsA_t fn_IsA;

// --- Game_RemoveEntity (0x140091a20) — adds entity to pending-removal list ---
// Adds an entity pointer to the game's removal queue at gameWorld+0x120/0x128/0x12c.
// Uses standard dynamic array grow pattern (triple capacity on first realloc).
// param_1 = gameWorld ptr (from g_pGameWorldSingleton + offset), param_2 = entity ptr
// REIMPLEMENTED inline from Ghidra decompilation.
inline void Game_RemoveEntity_Impl(void* gameWorldPtr, void* entityPtr) {
    uintptr_t gw = (uintptr_t)gameWorldPtr;
    uintptr_t entity = (uintptr_t)entityPtr;

    // Check if entity is already in the removal queue
    int count = *(int*)(gw + 0x128);
    uintptr_t* arr = *(uintptr_t**)(gw + 0x120);
    for (int i = 0; i < count; i++) {
        if (arr[i] == entity) return;  // already queued
    }

    // Dynamic array push
    int newCount = count + 1;
    if (newCount != count) {
        uintptr_t* oldArr = arr;
        *(int*)(gw + 0x128) = newCount;
        int capacity = *(int*)(gw + 0x12c);
        if (capacity < newCount) {
            // Grow: triple capacity if non-empty, else use newCount
            capacity = (count != 0) ? newCount * 3 : newCount;
            *(int*)(gw + 0x12c) = capacity;
            void* newBuf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)capacity * 8, 8);
            *(void**)(gw + 0x120) = newBuf;
        }
        uintptr_t* newArr = *(uintptr_t**)(gw + 0x120);
        int copyStart = (oldArr == newArr) ? count : 0;
        for (int i = copyStart; i < newCount; i++) {
            if (i < count) {
                newArr[i] = oldArr[i];  // copy existing
            } else {
                newArr[i] = entity;     // append new
            }
        }
        if (oldArr != newArr) {
            fn_MemFreeStatic(oldArr);
        }
    }
}

// --- Storage capacity check: FUN_14012cd80(storageComp, entityPtr, quantity) -> bool ---
// Checks if storageComp can hold 'quantity' more of the resource on entityPtr.
// Returns true if total count + (resourceType.size * quantity) <= storageComp.capacity (+0x38).
// REIMPLEMENTED inline from Ghidra decompilation.
inline bool Storage_CanAddItem(void* storageComp, void* entityPtr, int quantity) {
    uintptr_t sc = (uintptr_t)storageComp;
    uintptr_t ent = (uintptr_t)entityPtr;

    // Check resource flags match: storageComp+0x3c & resourceType+0x5c
    // Find ResourceType component on entity
    int compCount = *(int*)(ent + 0x30);
    uintptr_t compArray = *(uintptr_t*)(ent + 0x28);
    void* resourceType = nullptr;
    for (int i = 0; i < compCount; i++) {
        void* comp = *(void**)(compArray + (uintptr_t)i * 8);
        typedef uint32_t (*GetTypeId_t)(void*);
        uint32_t typeId = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
        if (typeId == *g_TypeId_RawMaterialDescription) {
            if (fn_IsDerivedFrom(comp, *g_TypeId_RawMaterialDescription)) {
                resourceType = comp;
            }
            break;
        }
    }

    // Check flags compatibility
    uint32_t storageFlags = *(uint32_t*)(sc + 0x3c);
    uint32_t resourceFlags = *(uint32_t*)((uintptr_t)resourceType + 0x5c);
    if ((storageFlags & resourceFlags) == 0) return false;

    // Check capacity: currentTotal + resourceSize * quantity <= capacity
    int currentTotal = fn_StorageComponent_QueryCount(storageComp);
    int resourceSize = *(int*)((uintptr_t)resourceType + 0x48);
    int maxCapacity = *(int*)(sc + 0x38);
    return (currentTotal + resourceSize * quantity) <= maxCapacity;
}

// --- Storage add item: FUN_14012d000(storageComp, entityPtr, quantity) ---
// Adds quantity of entity's resource to storage. If entity already exists in storage
// array, increments count. Otherwise appends a new 16-byte entry {entityPtr, quantity, 0}.
// REIMPLEMENTED inline from Ghidra decompilation.
inline void Storage_AddItem(void* storageComp, void* entityPtr, int quantity) {
    uintptr_t sc = (uintptr_t)storageComp;
    uintptr_t entity = (uintptr_t)entityPtr;

    // Check if entity already in storage array — +0x28 = array ptr, +0x30 = count, +0x34 = capacity
    int count = *(int*)(sc + 0x30);
    uintptr_t* arr = *(uintptr_t**)(sc + 0x28);

    // Each entry is 16 bytes: [0..7]=entityPtr, [8..11]=quantity, [12..15]=pad
    for (int i = 0; i < count; i++) {
        uintptr_t* entry = (uintptr_t*)((uintptr_t)arr + (uintptr_t)i * 16);
        if (*entry == entity) {
            // Already exists — increment count
            if (i >= 0) {
                int* qtyPtr = (int*)((uintptr_t)arr + (uintptr_t)i * 16 + 8);
                *qtyPtr += quantity;
            }
            return;
        }
    }

    // Not found — append new entry
    int newCount = count + 1;
    if (newCount == count) return;

    uintptr_t* oldArr = arr;
    *(int*)(sc + 0x30) = newCount;
    int capacity = *(int*)(sc + 0x34);
    if (capacity < newCount) {
        capacity = (count != 0) ? newCount * 3 : newCount;
        *(int*)(sc + 0x34) = capacity;
        void* newBuf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)capacity * 16, 8);
        *(void**)(sc + 0x28) = newBuf;
    }
    uintptr_t* newArr = *(uintptr_t**)(sc + 0x28);
    int copyStart = (oldArr == newArr) ? count : 0;
    for (int i = copyStart; i < newCount; i++) {
        uintptr_t* dst = (uintptr_t*)((uintptr_t)newArr + (uintptr_t)i * 16);
        if (i < count) {
            // Copy from old
            uintptr_t* src = (uintptr_t*)((uintptr_t)oldArr + (uintptr_t)i * 16);
            dst[0] = src[0];
            dst[1] = src[1];
        } else {
            // New entry
            dst[0] = entity;
            int32_t* newQty = (int32_t*)&dst[1];
            newQty[0] = quantity;
            newQty[1] = 0;
        }
    }
    if (oldArr != newArr) {
        fn_MemFreeStatic(oldArr);
    }
}

// --- FindDialogByName (FUN_1400e1600) — REIMPLEMENTED inline ---
// Parses dialog name string (splits on ':'), gets type IDs via Factory::GetTypeIdFromName,
// walks UI::UserInterface dialog linked list with IsA checks, optionally matches controller type.
// Non-trivial return: RCX=retval(InternalBasePtr*), RDX=name(String*)
// NOTE: replaces both fn_FindDialogByName and fn_TSD_FindByString.
inline void* FindDialogByName_Impl(void* retval, void* nameStr) {
    uintptr_t ret = (uintptr_t)retval;
    uintptr_t namePtr = *(uintptr_t*)nameStr;

    // Empty string or null -> return null
    if (namePtr == 0 || *(int*)(namePtr - 8) == 0) {
        *(uintptr_t*)ret = 0;
        return retval;
    }

    // Split string on ':'
    // Build a temporary array of substrings
    const char* str = (const char*)namePtr;
    int strLen = *(int*)(namePtr - 8);

    // Count parts (max 2 — "DialogType" or "DialogType:ControllerType")
    int colonPos = -1;
    for (int i = 0; i < strLen; i++) {
        if (str[i] == ':') { colonPos = i; break; }
    }

    // Extract part1 (and optionally part2)
    char part1[256] = {};
    char part2[256] = {};
    int partCount = 1;

    if (colonPos >= 0) {
        if (colonPos < 255) {
            memcpy(part1, str, colonPos);
            part1[colonPos] = 0;
        }
        int part2Len = strLen - colonPos - 1;
        if (part2Len > 0 && part2Len < 255) {
            memcpy(part2, str + colonPos + 1, part2Len);
            part2[part2Len] = 0;
        }
        partCount = 2;
    } else {
        int copyLen = (strLen < 255) ? strLen : 255;
        memcpy(part1, str, copyLen);
        part1[copyLen] = 0;
    }

    // Get type IDs from Factory (double-deref: dispatch slot -> pointer -> Factory*)
    if (!g_pFactorySingleton || !*g_pFactorySingleton || !fn_Factory_GetTypeIdFromName) {
        *(uintptr_t*)ret = 0;
        return retval;
    }
    void* factoryObj = *(void**)*g_pFactorySingleton;
    if (!factoryObj) {
        *(uintptr_t*)ret = 0;
        return retval;
    }

    uint32_t dialogTypeId = fn_Factory_GetTypeIdFromName(factoryObj, part1, true);
    if (dialogTypeId == 0) {
        *(uintptr_t*)ret = 0;
        return retval;
    }

    uint32_t controllerTypeId = 0;
    if (partCount == 2 && part2[0] != 0) {
        controllerTypeId = fn_Factory_GetTypeIdFromName(factoryObj, part2, true);
    }

    // Get UI singleton
    void* gameWorld = *g_pGameWorldSingleton;
    if (!gameWorld) {
        *(uintptr_t*)ret = 0;
        return retval;
    }
    void* uiSingleton = *(void**)((uintptr_t)gameWorld + 0x98);
    if (!uiSingleton || !fn_UI_GetFirstDialog || !fn_IsA) {
        *(uintptr_t*)ret = 0;
        return retval;
    }

    // Walk dialog linked list
    // GetFirstDialog writes first node pointer into retval buffer.
    // Node layout: [0]=InternalBasePtr (DialogInstance*), [0x10]=nextNode*
    // Original: LEA RDX,[RBP+0x48]; MOV RCX,RSI; CALL [0x140151ca0]
    uintptr_t nodeBuf[4] = {};
    fn_UI_GetFirstDialog(uiSingleton, nodeBuf);
    uintptr_t node = (uintptr_t)nodeBuf[0];

    while (node != 0) {
        // CheckPtr on the iterator node (InternalBasePtr) before dereferencing.
        // Original: MOV RCX,RSI; CALL [0x140152ab0] (fn_InternalBasePtr_CheckPtr)
        if (fn_InternalBasePtr_CheckPtr)
            fn_InternalBasePtr_CheckPtr((void*)node);

        // Deref node to get DialogInstance*
        void* dialog = *(void**)node;
        if (dialog) {
            // Check IsA against dialogTypeId
            if (fn_IsA(dialog, dialogTypeId)) {
                if (controllerTypeId == 0 || partCount != 2) {
                    // Match found, no controller check needed
                    *(uintptr_t*)ret = (uintptr_t)dialog;
                    return retval;
                }
                // Need to check controller type too.
                // Original reloads node and calls CheckPtr again before GetController:
                //   MOV RSI,[RBP+0x48]; MOV RCX,RSI; CALL CheckPtr
                //   MOV R8D,EBX(=controllerTypeId); LEA RDX,[retbuf]; MOV RCX,[RSI]; CALL GetController
                // GetController takes controllerTypeId as R8 — it does the type matching
                // internally and returns a matching controller or null.
                if (fn_InternalBasePtr_CheckPtr)
                    fn_InternalBasePtr_CheckPtr((void*)node);
                if (fn_DialogInstance_GetController) {
                    uintptr_t controllerBuf[2] = {};
                    fn_DialogInstance_GetController(*(void**)node, controllerBuf, controllerTypeId);
                    if (controllerBuf[0] != 0) {
                        // Controller matched — return this dialog
                        *(uintptr_t*)ret = (uintptr_t)*(void**)node;
                        return retval;
                    }
                    // Controller didn't match — continue searching
                }
            }
        }
        // Next node: node + 0x10 -> next pointer
        // Original: MOV RAX,[RBP+0x48]; MOV RSI,[RAX+0x10]; MOV [RBP+0x48],RSI
        node = *(uintptr_t*)(node + 0x10);
    }

    // Not found
    *(uintptr_t*)ret = 0;
    return retval;
}

// --- Game_TriggerDisasterByType (0x140060110) — REIMPLEMENTED inline ---
// Iterates ExternalFactory for entities with BuildComponent (DAT_1401c7318),
// finds SelectDescription sub-component (DAT_1401c48a8), checks type match,
// removes matching entity via Game_RemoveEntity.
inline void Game_TriggerDisasterByType_Impl(int disasterType) {
    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton) return;
    if (!fn_ExternalFactory_FindNext || !fn_IsDerivedFrom) return;

    void* factory = *g_pExternalFactorySingleton;
    void* gameWorld = *g_pGameWorldSingleton;
    if (!gameWorld) return;
    void* entityList = *(void**)((uintptr_t)gameWorld + 0xf0);

    // Iterate all BuildComponent-type externals
    void* ext = fn_ExternalFactory_FindNext(factory, *g_TypeId_BuildComponent, nullptr);
    while (ext != nullptr) {
        uintptr_t e = (uintptr_t)ext;
        if (fn_IsDerivedFrom(ext, *g_TypeId_BuildComponent)) {
            // Find SelectDescription sub-component
            int compCount = *(int*)(e + 0x30);
            uintptr_t compArray = *(uintptr_t*)(e + 0x28);

            // First pass: check if entity HAS a SelectDescription
            bool hasSelect = false;
            for (int i = 0; i < compCount; i++) {
                void* comp = *(void**)(compArray + (uintptr_t)i * 8);
                typedef uint32_t (*GetTypeId_t)(void*);
                uint32_t typeId = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
                if (typeId == *g_TypeId_SelectDescription) {
                    if (fn_IsDerivedFrom(comp, *g_TypeId_SelectDescription) && comp != nullptr) {
                        hasSelect = true;
                    }
                    break;
                }
            }

            if (hasSelect) {
                // Second pass: find the SelectDescription and check type
                void* selectDesc = nullptr;
                for (int i = 0; i < compCount; i++) {
                    void* comp = *(void**)(compArray + (uintptr_t)i * 8);
                    typedef uint32_t (*GetTypeId_t)(void*);
                    uint32_t typeId = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
                    if (typeId == *g_TypeId_SelectDescription) {
                        if (fn_IsDerivedFrom(comp, *g_TypeId_SelectDescription)) {
                            selectDesc = comp;
                        }
                        break;
                    }
                }
                // Check if SelectDescription.type (+0x40) matches requested disaster type
                if (selectDesc && *(int*)((uintptr_t)selectDesc + 0x40) == disasterType) {
                    Game_RemoveEntity_Impl(entityList, ext);
                }
            }
        }
        ext = fn_ExternalFactory_FindNext(factory, *g_TypeId_BuildComponent, ext);
    }
}

// --- Game_SpawnResources (0x140091660) — REIMPLEMENTED inline ---
// Spawns 'count' resources into citizen storage buildings.
// Phase 1: builds array of entity pointers whose NaturalResourceComponent flags match resourceFlag.
// Phase 2: round-robin distributes items across citizens' StorageComponents.
// param_1 = Game object (longlong*), param_2 = resourceFlag bitmask, param_3 = total count to spawn.
//
// Structure offsets on param_1 (Game object):
//   +0x110 (param_1[0x22]) = entity array ptr (raw pointers)
//   +0x118 (param_1[0x23]) = entity count
//   +0x000 (param_1[0])    = citizen InternalBasePtr array
//   +0x008 (param_1[1])    = citizen count
//
// Entities (phase 1) are raw pointers with components at +0x28 (array) / +0x30 (count).
// Citizens (phase 2) are InternalBasePtr entries with components at +0x30 (IBP array) / +0x38 (count).
//
// Calls original exe functions StorageComponent_impl_cd80 (0x14012cd80) for CanAdd
// and FUN_14012d000 (0x14012d000) for AddItem, via resolved function pointers.

// Resolved exe function pointers for Storage CanAdd/AddItem
typedef uint32_t (*fn_StorageCanAdd_t)(void* storageComp, void* entityPtr, int quantity);
extern fn_StorageCanAdd_t fn_StorageCanAdd;  // 0x14012cd80

typedef void (*fn_StorageAddItem_t)(void* storageComp, void* entityPtr, int quantity);
extern fn_StorageAddItem_t fn_StorageAddItem;  // 0x14012d000

inline void Game_SpawnResources_Impl(void* citizenListPtr, uint32_t resourceFlag, int count) {
    int64_t* param_1 = (int64_t*)citizenListPtr;

    // Dynamic array for matching entities
    // Ghidra uses local_48 = array ptr, local_40 = packed CONCAT44(capacity, count)
    void* matchArr = nullptr;
    int matchCount = 0;
    int matchCapacity = 0;

    // Phase 1: Build array of entity pointers whose NaturalResourceComponent flags match resourceFlag
    // Entity array is raw pointers: param_1[0x22] = array ptr, param_1[0x23] = count
    if ((int)param_1[0x23] > 0) {
        int64_t entityOffset = 0;
        int entityIdx = 0;
        do {
            int64_t entity = *(int64_t*)(entityOffset + param_1[0x22]);

            // Find NaturalResourceComponent on this entity (raw component array at +0x28/+0x30)
            void* natResComp = nullptr;
            int compIdx = 0;
            int64_t compOffset = 0;
            if (*(int*)(entity + 0x30) > 0) {
                do {
                    void* comp = *(void**)(*(int64_t*)(entity + 0x28) + compOffset * 8);
                    // vtable call: GetTypeId at vtable+0x10
                    typedef uint32_t (*GetTypeId_t)(void*);
                    uint32_t typeId = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
                    if (typeId == *g_TypeId_RawMaterialDescription) {
                        void* foundComp = *(void**)(*(int64_t*)(entity + 0x28) + compOffset * 8);
                        bool isDerived = fn_IsDerivedFrom(foundComp, *g_TypeId_RawMaterialDescription);
                        natResComp = nullptr;
                        if (isDerived) {
                            natResComp = foundComp;
                        }
                        goto phase1_found;
                    }
                    compIdx++;
                    compOffset++;
                } while (compIdx < *(int*)(entity + 0x30));
            }
            natResComp = nullptr;

        phase1_found:
            {
                // Check resource flags match (natResComp+0x5c & resourceFlag)
                // Original code does NOT null-check — relies on entities always having the component
                void* oldArr = matchArr;
                int oldCount = matchCount;
                if ((*(uint32_t*)((uintptr_t)natResComp + 0x5c) & resourceFlag) != 0) {
                    int newCount = matchCount + 1;
                    if (newCount != matchCount) {  // overflow guard (matches original JZ)
                        matchCount = newCount;
                        if (matchCapacity < newCount) {
                            // Growth: if first alloc, capacity = newCount; else capacity = newCount * 3
                            matchCapacity = newCount;
                            if (oldArr != nullptr) {
                                matchCapacity = newCount * 3;
                            }
                            matchArr = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)matchCapacity * 8, 8);
                        }

                        // Copy old entries + append new entity
                        // If matchArr == oldArr (no realloc needed), start from old count (just append)
                        // If matchArr != oldArr (reallocated), start from 0 (copy all + append)
                        uint32_t copyStart = 0;
                        if (oldArr == matchArr) {
                            copyStart = (uint32_t)oldCount;
                        }
                        int copyIdx = (int)copyStart;
                        if (copyIdx < newCount) {
                            int64_t* dstSlot = (int64_t*)((int64_t)matchArr + (int64_t)copyIdx * 8);
                            int64_t* srcSlot = (int64_t*)((int64_t)oldArr + (int64_t)copyIdx * 8);
                            do {
                                if (dstSlot != nullptr) {
                                    int64_t* source = &entity;
                                    if ((int)copyStart < oldCount) {
                                        source = srcSlot;
                                    }
                                    *dstSlot = *source;
                                }
                                copyStart++;
                                srcSlot++;
                                dstSlot++;
                            } while ((int)copyStart < newCount);
                        }

                        // Free old array if it was reallocated
                        if (oldArr != matchArr) {
                            fn_MemFreeStatic(oldArr);
                        }
                    }
                }
            }

            entityIdx++;
            entityOffset += 8;
        } while (entityIdx < (int)param_1[0x23]);

        // Phase 2: Round-robin distribute items across citizens' StorageComponents
        // Citizens are InternalBasePtr entries: param_1[0] = array ptr, param_1[1] = count
        if (matchCount > 0 && count > 0) {
            int spawned = 0;

            while (true) {
                bool anyAdded = false;
                int citizenIdx = 0;
                if ((int)param_1[1] < 1) break;

                int64_t citizenOffset = 0;
                do {
                    // Each citizen entry is an InternalBasePtr — load then CheckPtr
                    int64_t citizenIBP = *(int64_t*)(citizenOffset + *param_1);
                    fn_InternalBasePtr_CheckPtr(&citizenIBP);
                    int64_t citizen = citizenIBP;

                    // Find StorageComponent on citizen
                    // Citizen component array is IBP entries at citizen+0x30, count at citizen+0x38
                    void* storageComp = nullptr;
                    int cCompIdx = 0;
                    int64_t cCompOffset = 0;
                    if (*(int*)(citizen + 0x38) > 0) {
                        do {
                            // Each component entry is an InternalBasePtr — CheckPtr before reading
                            void* compIBP = (void*)(*(int64_t*)(citizen + 0x30) + cCompOffset * 8);
                            fn_InternalBasePtr_CheckPtr(compIBP);

                            typedef uint32_t (*GetTypeId_t)(void*);
                            uint32_t typeId = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)(*(int64_t*)compIBP) + 0x10)))(*(void**)compIBP);
                            if (typeId == *g_TypeId_StorageComponent_FC) {
                                // Re-CheckPtr the IBP slot and read again
                                void* compIBP2 = (void*)(*(int64_t*)(citizen + 0x30) + cCompOffset * 8);
                                fn_InternalBasePtr_CheckPtr(compIBP2);
                                void* compVal = *(void**)compIBP2;

                                bool isDerived = fn_IsDerivedFrom(compVal, *g_TypeId_StorageComponent_FC);
                                storageComp = nullptr;
                                if (isDerived) {
                                    storageComp = compVal;
                                }
                                goto phase2_found;
                            }
                            cCompIdx++;
                            cCompOffset++;
                        } while (cCompIdx < *(int*)(citizen + 0x38));
                    }
                    storageComp = nullptr;

                phase2_found:
                    {
                        // Wrap found storage component in an InternalBasePtr via fn_InternalBasePtr_Ctor
                        int64_t storageIBP = 0;
                        fn_InternalBasePtr_Ctor(&storageIBP, storageComp);

                        // Try to add one item via round-robin
                        if (spawned < count) {
                            int64_t* matchSlot = (int64_t*)((int64_t)matchArr + (int64_t)(spawned % matchCount) * 8);

                            // CheckPtr the storage IBP before use
                            fn_InternalBasePtr_CheckPtr(&storageIBP);

                            // Call original exe CanAdd function (0x14012cd80)
                            uint32_t canAdd = fn_StorageCanAdd((void*)storageIBP, (void*)*matchSlot, 1);
                            if ((char)canAdd != '\0') {
                                // CheckPtr again before AddItem
                                fn_InternalBasePtr_CheckPtr(&storageIBP);

                                // Call original exe AddItem function (0x14012d000)
                                fn_StorageAddItem((void*)storageIBP, (void*)*matchSlot, 1);
                                spawned++;
                                anyAdded = true;
                            }
                        }
                    }

                    citizenIdx++;
                    citizenOffset += 8;
                } while (citizenIdx < (int)param_1[1]);

                if (!anyAdded || spawned >= count) break;
            }
        }
    }

    // Free match array
    fn_MemFreeStatic(matchArr);
}

// ============================================================
// Toolbar_CenterOnEntity_Impl — centers camera on entity and selects it in EditTool
// ============================================================
// Reimplemented from FUN_140103040 (fn_Toolbar_CenterOnEntity / fn_GoToEntity).
//
// toolController = PlayerController (gameWorld+0x120)
// parentDialogPtr = InternalBasePtr* to parent dialog (may be null-ptr)
// entityPtr = InternalBasePtr* to target entity
//
// Logic:
// 1. If parent dialog has a PageToggleUI controller, enable entity tracking
// 2. Get entity world position:
//    - Entities with ModelComponent but no MultiObjectComponent: use model's
//      Math::Transform position (offset +0x158 = +0x148 transform + 0x10 position)
//    - Multi-object buildings: calculate center from BuildComponent grid footprint
// 3. Set camera target and snap camera to new position
// 4. Get/create EditTool and select entity in it
//
// NOTE: g_TypeId_CharacterComponent (DAT_1401c7a50) is actually ModelComponent's TypeInfo,
// not CharacterComponent. fn_FindComponent_Character finds ModelComponent.

inline void Toolbar_CenterOnEntity_Impl(void* toolController, void* parentDialogPtr, void* entityPtr) {
    uintptr_t tc = (uintptr_t)toolController;
    uintptr_t game = (uintptr_t)*g_pGameWorldSingleton;

    // === Section 1: Enable tracking on EntityPanel controller ===
    if (*(uintptr_t*)parentDialogPtr != 0) {
        fn_InternalBasePtr_CheckPtr(parentDialogPtr);

        // Get controller from dialog instance
        uint8_t controllerBuf[16] = {};
        fn_DialogInstance_GetController(*(void**)parentDialogPtr, controllerBuf, 0);

        uintptr_t controllerRaw = *(uintptr_t*)controllerBuf;
        void* panelController = nullptr;
        // g_TypeId_PageToggleUI is an exe .rdata type-id that is NULL in standalone
        // until lazily resolved; dereferencing *g_TypeId_PageToggleUI would fault.
        // In DLL mode it is always live, so this guard is a no-op there.
        if (controllerRaw != 0 && g_TypeId_PageToggleUI) {
            fn_InternalBasePtr_CheckPtr(controllerBuf);
            if (fn_IsDerivedFrom((void*)controllerRaw, *g_TypeId_PageToggleUI)) {
                panelController = (void*)controllerRaw;
            }
        }

        uint64_t ctrlRef = 0;
        fn_InternalBasePtr_Ctor(&ctrlRef, panelController);

        if (ctrlRef != 0) {
            fn_InternalBasePtr_CheckPtr(&ctrlRef);
            uintptr_t ctrl = (uintptr_t)ctrlRef;
            fn_InternalBasePtr_CheckPtr((void*)(ctrl + 0x38));
            uintptr_t trackedEntity = *(uintptr_t*)(ctrl + 0x38);

            if (*(uint8_t*)(trackedEntity + 0xd8) == 0) {
                // Enable tracking flag
                fn_InternalBasePtr_CheckPtr(&ctrlRef);
                ctrl = (uintptr_t)ctrlRef;
                fn_InternalBasePtr_CheckPtr((void*)(ctrl + 0x38));
                *(uint8_t*)(*(uintptr_t*)(ctrl + 0x38) + 0xd8) = 1;

                if (fn_EntityPanel_SyncTracking) {
                    fn_EntityPanel_SyncTracking((void*)ctrl);
                }
            }
        }
    }

    // === Section 2: Get entity world position ===
    float targetPos[3] = {0.0f, 0.0f, 0.0f};

    fn_InternalBasePtr_CheckPtr(entityPtr);
    uintptr_t entityRaw = *(uintptr_t*)entityPtr;

    // Try ModelComponent (DAT_1401c7a50 — misnamed as g_TypeId_CharacterComponent)
    uint64_t modelRef = 0;
    fn_FindComponent_Character((void*)entityRaw, &modelRef);

    bool gotPosition = false;
    if (modelRef != 0) {
        // Has ModelComponent — check for MultiObjectComponent
        uint64_t multiRef = 0;
        fn_FindComponent_MultiObject((void*)entityRaw, &multiRef);

        if (multiRef == 0) {
            // Single object (citizen/animal/tree): use model's transform position
            uint64_t modelRef2 = 0;
            fn_FindComponent_Character((void*)entityRaw, &modelRef2);
            fn_InternalBasePtr_CheckPtr(&modelRef2);
            uintptr_t model = (uintptr_t)modelRef2;

            // Math::Transform at ModelComponent+0x148 is {quat(16 bytes), pos(12 bytes)}
            // Position starts at +0x10 within the 0x1C-byte Transform = ModelComponent+0x158
            targetPos[0] = *(float*)(model + 0x158);
            targetPos[1] = *(float*)(model + 0x15C);
            targetPos[2] = *(float*)(model + 0x160);
            gotPosition = true;
        }
    }

    if (!gotPosition) {
        // Building/multi-object: calculate center from BuildComponent grid footprint
        fn_InternalBasePtr_CheckPtr(entityPtr);
        uint64_t buildRef = 0;
        fn_FindComponent_Build(*(void**)entityPtr, &buildRef);
        fn_InternalBasePtr_CheckPtr(&buildRef);
        uintptr_t build = (uintptr_t)buildRef;

        int* mapGrid = *(int**)(game + 0xF8);
        int gridSize = *mapGrid;
        float halfGrid = (float)(gridSize >> 1);

        int16_t posX = *(int16_t*)(build + 0x28);
        int16_t posY = *(int16_t*)(build + 0x2a);
        int16_t extX = *(int16_t*)(build + 0x2c);
        int16_t extY = *(int16_t*)(build + 0x2e);

        // Far corner of footprint
        int idx1 = (int)extX + ((int)extY + (int)posY) * gridSize + (int)posX;
        float inXY1[3] = {
            (float)(idx1 % gridSize) - halfGrid,
            (float)(idx1 / gridSize) - halfGrid,
            0.0f
        };
        float pos1[3];
        fn_MapGetTerrainPos(mapGrid, pos1, inXY1, '\0', nullptr);

        // Origin corner of footprint
        int idx2 = gridSize * (int)posY + (int)posX;
        float inXY2[3] = {
            (float)(idx2 % gridSize) - halfGrid,
            (float)(idx2 / gridSize) - halfGrid,
            0.0f
        };
        float pos2[3];
        fn_MapGetTerrainPos(mapGrid, pos2, inXY2, '\0', nullptr);

        // Midpoint of origin and far corner = footprint center
        targetPos[0] = (pos1[0] + pos2[0]) * 0.5f;
        targetPos[1] = (pos1[1] + pos2[1]) * 0.5f;
        targetPos[2] = (pos1[2] + pos2[2]) * 0.5f;
    }

    // === Section 3: Snap camera to entity position ===
    uintptr_t cameraSys = *(uintptr_t*)(game + 0xE0);

    // Set camera target position
    *(float*)(cameraSys + 0x1C) = targetPos[0];
    *(float*)(cameraSys + 0x20) = targetPos[1];
    *(float*)(cameraSys + 0x24) = targetPos[2];

    // Zero-duration time for instant snap (matches original)
    if (fn_Time_Ctor_Float) {
        uint64_t timeVal;
        fn_Time_Ctor_Float(&timeVal, 0.0f);
    }

    // Compute camera view transform and apply
    if (fn_CameraSys_ComputeViewTransform) {
        uint32_t transformBuf[7] = {}; // 0x1C bytes = compact Math::Transform
        void* result = fn_CameraSys_ComputeViewTransform((void*)cameraSys, transformBuf);

        if (fn_Transform_AsTransform3 && fn_Camera_SetCameraToWorld) {
            uint32_t transform3[12] = {}; // 48 bytes = Math::Transform3
            fn_Transform_AsTransform3(result, transform3);
            // Camera object embedded at cameraSys + 0x34 (offset 0xD * sizeof(uint32_t))
            fn_Camera_SetCameraToWorld((void*)(cameraSys + 0x34), transform3);
        }
    }

    // === Section 4: Select entity in EditTool ===
    // g_TypeId_EditTool is an exe .rdata type-id that is NULL in standalone until
    // lazily resolved (e.g. CitizenUI::EnsureGlobals). Dereferencing *g_TypeId_EditTool
    // with a null pointer reads address 0 and hard-faults. The camera snap (Section 3)
    // has already run, so if we can't classify the EditTool we simply skip the re-select
    // — the camera still moves to the entity. In DLL mode the id is always live.
    void* currentTool = *(void**)(tc + 0x28);
    void* editTool = nullptr;

    if (currentTool && g_TypeId_EditTool) {
        fn_InternalBasePtr_CheckPtr(&currentTool);
        if (fn_IsDerivedFrom(currentTool, *g_TypeId_EditTool)) {
            editTool = currentTool;
        }
    }

    uint64_t editToolRef = 0;
    fn_InternalBasePtr_Ctor(&editToolRef, editTool);

    if (editToolRef == 0 && g_TypeId_EditTool) {
        // Switch to EditTool mode
        EntityUI_SetEditMode_Impl(toolController, 0);

        // Re-read current tool after mode switch
        void* newTool = *(void**)(tc + 0x28);
        editTool = nullptr;
        if (newTool && g_TypeId_EditTool) {
            fn_InternalBasePtr_CheckPtr(&newTool);
            if (fn_IsDerivedFrom(newTool, *g_TypeId_EditTool)) {
                editTool = newTool;
            }
        }
        editToolRef = 0;
        fn_InternalBasePtr_Ctor(&editToolRef, editTool);
    }

    // Select the entity in the EditTool
    if (editToolRef != 0) {
        void* entityRefCopy = *(void**)entityPtr;
        fn_InternalBasePtr_CheckPtr(&editToolRef);
        fn_EditTool_SelectEntity((void*)(uintptr_t)editToolRef, &entityRefCopy);
    }
}

// ============================================================
// Work class helpers (shared by BuildStructure, ClearArea, ClearStorage)
// ============================================================

// Tree node removal by key (CitizenAI_impl_5520 @ 0x140025520)
// Searches binary tree at *param_1 for node matching param_2's key, removes it.
// Used by OnComplete/OnCancel to remove citizen from assignment tree.
typedef void (*fn_TreeRemoveByKey_t)(void* treeRootPtr, void* keyIBP);
extern fn_TreeRemoveByKey_t fn_TreeRemoveByKey;
// Alias used by ClearStorage
static inline auto& fn_BST_RemoveByKey = fn_TreeRemoveByKey;

// Recursive tree node free (FUN_140014c30 @ 0x140014c30)
// Recursively frees a binary tree (children at +0x10 and +0x18).
typedef void (*fn_TreeRecursiveFree_t)(void* treeRootPtr, void* node);
extern fn_TreeRecursiveFree_t fn_TreeRecursiveFree;

// Object::Internal::~Internal — via dispatch slot 0x140152770
typedef void (*fn_ObjectInternal_Dtor_t)(void* self);
extern fn_ObjectInternal_Dtor_t fn_ObjectInternal_Dtor;

// Citizen state machine release (FUN_1400c5400 @ 0x1400c5400)
// Walks task list, calls vtable+0x40 on each, clears slots.
// Used by Cleanup to release assigned citizens.
typedef void (*fn_StateMachine_ReleaseTasks_t)(void* taskListPtr);
extern fn_StateMachine_ReleaseTasks_t fn_StateMachine_ReleaseTasks;
// Alias used by ClearStorage
static inline auto& fn_SM_CancelWork = fn_StateMachine_ReleaseTasks;

// ============================================================
// BuildRoad::Execute helper functions (exe-only, 0x1400e4190)
// ============================================================

// FUN_1400a00f0 — Map_CheckPassable: checks if path between two cells is passable
// RCX=mapGrid, EDX=passFlags(0xe1f), R8=fromCell, R9D=toCell
// Returns: non-zero if passable
typedef uint64_t (*fn_Map_CheckPassable_t)(int* mapGrid, uint32_t flags, uint32_t fromCell, int toCell);
extern fn_Map_CheckPassable_t fn_Map_CheckPassable;

// FUN_14009ccf0 — Map_FindNearestPassable: find nearest passable cell adjacent to target
// RCX=mapGrid, EDX=srcCell, R8D=destCell, R9D=passFlags(0xe07)
// Returns: cell index (-1 if not found)
typedef int (*fn_Map_FindNearestPassable_t)(int* mapGrid, int srcCell, int destCell, uint32_t flags);
extern fn_Map_FindNearestPassable_t fn_Map_FindNearestPassable;

// Math::Vector3::Vector3(this, x, y, z) — dispatch slot at 0x140152a30
// RCX=outVec3, XMM1=x, XMM2=y, XMM3=z
typedef float* (*fn_Vector3_Ctor_t)(float* self, float x, float y, float z);
extern fn_Vector3_Ctor_t fn_Vector3_Ctor;

// FUN_1400bffd0 — PerformWork_Action_Ctor: creates a PerformWork action (11 params)
// Complex multi-param constructor. Params match Ghidra decompilation signature.
typedef void (*fn_PerformWork_Action_Ctor_t)(
    void* entityIBP, int priority, void* position, uint64_t targetCell,
    uint32_t roadType, uint8_t flags, int param7, int64_t resource,
    int resourceCount, float* velocity, void* actionRef);
extern fn_PerformWork_Action_Ctor_t fn_PerformWork_Action_Ctor;

// FUN_1400c1eb0 — PerformWork_Action_Ctor3: creates a PerformWork action (6 params)
// Simpler constructor for building at an existing structure.
typedef void (*fn_PerformWork_Action_Ctor3_t)(
    void* entityIBP, int priority, void* targetIBP, float* position,
    int flag, uint8_t param6);
extern fn_PerformWork_Action_Ctor3_t fn_PerformWork_Action_Ctor3;

// FUN_14008d590 — FindStorageForResource: searches for storage entity with resource
typedef void (*fn_FindStorageForResource_t)(
    void** citizenList, void* outEntity, int cellIndex, int64_t resourceType,
    int param5, char param6, float* velocity, void* outStorageIBP);
extern fn_FindStorageForResource_t fn_FindStorageForResource;

// FUN_1400223c0 — FindComponent type DAT_1401c7c80 (WorkplaceComponent variant)
// RCX=entity, RDX=outIBP
typedef void (*fn_FindComponent_WorkplaceC_t)(int64_t entity, void* outIBP);
extern fn_FindComponent_WorkplaceC_t fn_FindComponent_WorkplaceC;

// FUN_1400052c0 — FindComponent type DAT_1401c7b30 (BuildStateComponent variant)
// RCX=entity, RDX=outIBP
typedef void (*fn_FindComponent_BuildStateC_t)(int64_t entity, void* outIBP);
extern fn_FindComponent_BuildStateC_t fn_FindComponent_BuildStateC;

// FUN_140003050 — FindComponent_StorageLocation (already have fn_FindComponent_Workplace)
// RCX=entity, RDX=outIBP
typedef void (*fn_FindComponent_StorageLocC_t)(int64_t entity, void* outIBP);
extern fn_FindComponent_StorageLocC_t fn_FindComponent_StorageLocC;

// FUN_1400f8620 — TryAssignWorkplace
// RCX=assignMgr, RDX=citizenRef, R8D=workerCount, R9B=flag
typedef uint64_t (*fn_TryAssignWorkplace_t)(uintptr_t mgr, void* citizenRef, int count, char flag);
extern fn_TryAssignWorkplace_t fn_TryAssignWorkplace;

// FUN_140006330 — FindComponent_RawMaterial on entity (by resource type)
// RCX=resourceType, RDX=outIBP
typedef void (*fn_FindComponent_RawMaterialC_t)(int64_t resourceType, void* outIBP);
extern fn_FindComponent_RawMaterialC_t fn_FindComponent_RawMaterialC;

// FUN_140116540 — GetRemainingCapacity: returns capacity - current count
// RCX=citizenEntity
typedef int (*fn_GetRemainingCapacity_t)(uintptr_t citizenEntity);
extern fn_GetRemainingCapacity_t fn_GetRemainingCapacity;

// FUN_140115e60 — CitizenAI_State_LongTermPlan: replan citizen's state
// RCX=citizenEntity
typedef void (*fn_CitizenAI_LongTermPlan_t)(void* citizenEntity);
extern fn_CitizenAI_LongTermPlan_t fn_CitizenAI_LongTermPlan;

// FUN_14012ce70 — BuildRoad_QueryStorage: check storage for resource availability
// RCX=storageComp, RDX=resourceType  -> returns available count
typedef int (*fn_BuildRoad_QueryStorage_t)(uint64_t storageComp, int64_t resourceType);
extern fn_BuildRoad_QueryStorage_t fn_BuildRoad_QueryStorage;

// ============================================================
// ClearStorage helpers (code/Game/Work/clearstorage.cpp)
// ============================================================

// FUN_140017980: FindComponent by type ID 0x1401c7ee8 (unknown component type)
extern uint32_t* g_TypeId_UnknownComp_7ee8;  // DAT_1401c7ee8

inline void fn_FindComponent_Unknown7ee8(void* entity, void* retval) {
    Entity_FindComponentByExeTypeId(entity, retval, g_TypeId_UnknownComp_7ee8);
}

// FUN_1400f7100: Work::CanStartWork(self, citizenAI, taskIBP, outCompleteTask)
typedef bool (*fn_Work_CanStartWork_t)(void* self, void* citizenAI, void* taskIBP, void* outCompleteTask);
extern fn_Work_CanStartWork_t fn_Work_CanStartWork;

// Alias: fn_FindStorageDestination is the same function as fn_FindStorageForResource (0x14008d590)
// ClearStorage uses this name; both are valid.
typedef void (*fn_FindStorageDestination_t)(void* worldArray, void* outEntityIBP,
    uint32_t resourceTypeId, void* sourceEntity, int count, char allowFull,
    float* outPos, void* entityIBP);
extern fn_FindStorageDestination_t fn_FindStorageDestination;

// Alias: fn_Map_CanPathTo is the same function as fn_Map_CheckPassable (0x1400a00f0)
// ClearStorage uses this name with a different signature cast.
typedef bool (*fn_Map_CanPathTo_t)(void* mapGrid, int pathType, uint32_t destCell, uint32_t srcTypeId);
extern fn_Map_CanPathTo_t fn_Map_CanPathTo;

// FUN_1400f6ef0: BuildStructure_impl_6ef0 — validates work assignment against citizen/entity.
typedef bool (*fn_Work_ValidateAssignment_t)(void* work, void* citizenIBP, uint32_t destCell,
    uint32_t srcCell, void* time);
extern fn_Work_ValidateAssignment_t fn_Work_ValidateAssignment;

// FUN_1400bf790: Pathfinding_FindPath — sets up pathfinding for citizen work route.
typedef void (*fn_Pathfinding_FindPath_t)(void* outResult, void* citizenPtr, int pathType,
    void* destEntityIBP, float* destPos, void* srcEntityIBP, float* srcPos,
    long sourceEntity, int itemCount, int flags, int param11, char param12,
    int param13, int param14);
extern fn_Pathfinding_FindPath_t fn_Pathfinding_FindPath;

// FUN_140021a30: CitizenAI_impl_1a30 — registers work assignment with citizen AI.
typedef void (*fn_CitizenAI_RegisterWork_t)(void* citizenAISub, void* entityIBP, void* citizenIBP);
extern fn_CitizenAI_RegisterWork_t fn_CitizenAI_RegisterWork;

// FUN_14001a6e0: BST_Remove — removes a node from an intrusive BST (red-black tree).
typedef void (*fn_BST_Remove_t)(void* bstRoot, void* node);
extern fn_BST_Remove_t fn_BST_Remove;

// FUN_14012d5d0: StorageComponent_FindSlot — finds storage slot for a grid cell.
typedef int (*fn_Storage_FindSlot_t)(void* storageComp, uint32_t cellIndex, float* outPos, uint32_t typeId);
extern fn_Storage_FindSlot_t fn_Storage_FindSlot;

// FUN_140122e20: Entity_GetPosition — gets entity world position as Vector3.
typedef void (*fn_Entity_GetPosition_t)(void* entity, void* outVec3, int param3, int param4, float* param5);
extern fn_Entity_GetPosition_t fn_Entity_GetPosition;

// ============================================================
// BuildStructure support (code/Game/Work/buildstructure.cpp)
// ============================================================

// Work::Serialize base (0x1400f7400) — serializes Work base fields
typedef void (*fn_Work_SerializeBase_t)(void* self, void* stream);
extern fn_Work_SerializeBase_t fn_Work_SerializeBase;

// BST serialize helper (0x1400296b0) — serializes InternalBasePtr BST
typedef void (*fn_BST_SerializeHelper_t)(void* bstRoot, void* serializer);
extern fn_BST_SerializeHelper_t fn_BST_SerializeHelper;

// Entity::FindComponent by type ID — various component lookups on entities.

// FindComponent_Position (0x140002ed0) — type DWORD_1401c7900
typedef void* (*fn_Entity_FindComponent_Position_t)(uintptr_t entity, void* retIBP);
extern fn_Entity_FindComponent_Position_t fn_Entity_FindComponent_Position;

// FindComponent_Build (0x14000d6c0) — type DWORD_1401c7008 (BuildComponent)
typedef void* (*fn_Entity_FindComponent_Build_t)(uintptr_t entity, void* retIBP);
extern fn_Entity_FindComponent_Build_t fn_Entity_FindComponent_Build;

// FindComponent_BuildDesc (0x140002e10) — type DWORD_1401c80e0 (BuildDescription)
typedef void* (*fn_Entity_FindComponent_BuildDesc_t)(uintptr_t entity, void* retIBP);
extern fn_Entity_FindComponent_BuildDesc_t fn_Entity_FindComponent_BuildDesc;

// FindComponent_MatDesc (0x140003050) — type DWORD_1401c8070 (MaterialDescription)
// Note: same address as fn_FindComponent_StorageLocC, different type ID baked in
typedef void* (*fn_Entity_FindComponent_MatDesc_t)(uintptr_t entity, void* retIBP);
extern fn_Entity_FindComponent_MatDesc_t fn_Entity_FindComponent_MatDesc;

// PositionComponent_Count (0x140123070) — recursive position count
typedef int (*fn_PositionComponent_Count_t)(uintptr_t posComp, int mode);
extern fn_PositionComponent_Count_t fn_PositionComponent_Count;

// CalcMaxBuilders (0x14012e5c0) — calculates max builder count from description
typedef int (*fn_CalcMaxBuilders_t)(uintptr_t matDesc);
extern fn_CalcMaxBuilders_t fn_CalcMaxBuilders;

// Entity_GetPositionForIndex (0x140122e20) — gets world pos for build point index
// Note: same address as fn_Entity_GetPosition, different typedef
typedef void* (*fn_Entity_GetPositionForIndex_t)(
    uintptr_t posComp, float* outPos, int mode, int index, float* heading);
extern fn_Entity_GetPositionForIndex_t fn_Entity_GetPositionForIndex;

// Map_Invalidate (0x14009c480) — recalculates map connectivity/zones
typedef void (*fn_Map_Invalidate_t)(int* mapGrid);
extern fn_Map_Invalidate_t fn_Map_Invalidate;

// Map_CalcPathCost (0x14009f810) — calculates path cost between grid cells
typedef int (*fn_Map_CalcPathCost_t)(int* mapGrid, uint32_t cellA, int cellB, uint8_t cacheResults);
extern fn_Map_CalcPathCost_t fn_Map_CalcPathCost;

// Map_NeighborLookup (0x14009d0a0) — finds path region from neighbor cells
typedef int (*fn_Map_NeighborLookup_t)(int* mapGrid, int cell);
extern fn_Map_NeighborLookup_t fn_Map_NeighborLookup;

// Map_CalcPathDist (0x1400a8ba0) — A* / grid walk path distance
typedef int (*fn_Map_CalcPathDist_t)(int* navData, int regionA, int regionB);
extern fn_Map_CalcPathDist_t fn_Map_CalcPathDist;

// Map_CachePathDist (0x1400a5b30) — cache computed path distance
typedef void (*fn_Map_CachePathDist_t)(void* pathCache, int regionA, int regionB, int dist);
extern fn_Map_CachePathDist_t fn_Map_CachePathDist;

// SetupBuildActions (0x1400c0690) — creates the action queue for build work
typedef void (*fn_SetupBuildActions_t)(
    void* citizenEntityIBP, int priority, void* buildingEntityIBPPtr,
    float* position, int posIndex, uint8_t altAnim);
extern fn_SetupBuildActions_t fn_SetupBuildActions;

// BST_Insert (0x140021a30) — insert entity into BST (tracks claimed positions)
// Note: same address as fn_CitizenAI_RegisterWork, different typedef
typedef void* (*fn_BST_Insert_t)(void* bstRoot, void* entityIBP, void* citizenIBP);
extern fn_BST_Insert_t fn_BST_Insert;

// CitizenComponent_CheckWorkplaceValid (0x1401158d0) — checks if workplace is reachable
// RCX=CitizenComponent*, RDX=&workplaceEntityIBP. Returns bool (char).
typedef char (*fn_CitizenComponent_CheckWorkplaceValid_t)(void* citizen, void* workplaceIBP);
extern fn_CitizenComponent_CheckWorkplaceValid_t fn_CitizenComponent_CheckWorkplaceValid;

// CitizenComponent_HasVacancy (0x140017050) — checks if WorkplaceComponent has available slots
// RCX=WorkplaceComponent*. Returns bool.
typedef bool (*fn_CitizenComponent_HasVacancy_t)(uintptr_t workplaceComp);
extern fn_CitizenComponent_HasVacancy_t fn_CitizenComponent_HasVacancy;

// FUN_140003050 — FindStorageLocationComponent on entity (member fn, retval in RDX)
// RCX=entity, RDX=retval IBP. Returns retval ptr.
typedef void* (*fn_FindStorageLocationComp_t)(int64_t entity, void* outIBP);
extern fn_FindStorageLocationComp_t fn_FindStorageLocationComp;

// FUN_140002e10 — FindWorkplaceComponent on entity (member fn, retval in RDX)
// RCX=entity, RDX=retval IBP. Returns retval ptr.
typedef void* (*fn_FindWorkplaceComp_t)(int64_t entity, void* outIBP);
extern fn_FindWorkplaceComp_t fn_FindWorkplaceComp;

// ============================================================
// Achievement Goal support (code/Game/Goal/)
// ============================================================

// Achievement::Goal::~Goal (Runtime 0x18009edb0, ordinal 600)
// ?~Goal@Achievement@@UEAA@XZ — destroys Goal base (string dtor + External dtor)
typedef void (*fn_Goal_BaseDtor_t)(void* self);
extern fn_Goal_BaseDtor_t fn_Goal_BaseDtor;

// Achievement::Goal::Goal() (Runtime 0x18009ed50, ordinal 207)
// ?Goal@Achievement@@QEAA@XZ — default ctor, zeroes base fields
typedef void (*fn_Goal_BaseCtor_t)(void* self);
extern fn_Goal_BaseCtor_t fn_Goal_BaseCtor;

// Achievement::Goal::Serialize — called via dispatch slot 0x140152840
// Signature: void Goal::Serialize(this, Stream*)
typedef void (*fn_Goal_BaseSerialize_t)(void* self, void* stream);
extern fn_Goal_BaseSerialize_t fn_Goal_BaseSerialize;

// Achievement::GoalInstance::~GoalInstance (Runtime 0x18009ef40, ordinal 601)
// ??1GoalInstance@Achievement@@UEAA@XZ
typedef void (*fn_GoalInstance_BaseDtor_t)(void* self);
extern fn_GoalInstance_BaseDtor_t fn_GoalInstance_BaseDtor;

// Achievement::GoalInstance::Persist (Runtime 0x18009ef70)
// Signature: void GoalInstance::Persist(this, Stream*)
typedef void (*fn_GoalInstance_Persist_t)(void* self, void* stream);
extern fn_GoalInstance_Persist_t fn_GoalInstance_Persist;

// Object::InternalBasePtr::Serialize (Runtime DLL export)
// ?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z
typedef void (*fn_InternalBasePtr_Serialize_t)(void* self, void* serializer);
extern fn_InternalBasePtr_Serialize_t fn_InternalBasePtr_Serialize;

// UpdateLoop/Game IsRunning — dispatch slot 0x140152850
// Used by PopulationGoal::Slot8 to check if game is active
typedef bool (*fn_IsRunning_t)(void* self);
extern fn_IsRunning_t fn_IsRunning;

// System::MemoryAllocator::MemFree — static wrapper (0x18001d170 in Runtime)
// Already have fn_MemFreeStatic, but need the one used by Goal dtors
// (they call System::MemoryAllocator::MemFree which is fn_MemFreeStatic)

// FinishTutorialGoal helper: External serialize for tutorial ref (+0x30)
// FUN_1400262b0 — serializes External* field with type-check against Tutorial
typedef void (*fn_ExternalPtr_SerializeTutorial_t)(void* fieldPtr, void* serializer);
extern fn_ExternalPtr_SerializeTutorial_t fn_ExternalPtr_SerializeTutorial;

// BuildingGoal helpers
// FUN_14001b030 — destroy building type array (dtor strings + free)
typedef void (*fn_BuildingTypeArray_Dtor_t)(void* arrayField);
extern fn_BuildingTypeArray_Dtor_t fn_BuildingTypeArray_Dtor;

// FUN_14001b340 — resize building type array
typedef void (*fn_BuildingTypeArray_Resize_t)(void* arrayField, uint32_t count);
extern fn_BuildingTypeArray_Resize_t fn_BuildingTypeArray_Resize;

// FUN_14001b1d0 — compute string hash for building type entry
typedef void (*fn_StringHash_t)(void* str);
extern fn_StringHash_t fn_StringHash;

// System::String::Serialize — already declared as g_fn_String_Serialize

// System::Filename::Filename() — default ctor (dispatch slot 0x140151d40)
typedef void* (*fn_Filename_Ctor_t)(void* self);
extern fn_Filename_Ctor_t fn_Filename_Ctor;

// System::Filename::~Filename() — already declared as fn_Filename_Dtor
// IO::Serializer::Serialize(bool) — already declared as g_fn_Serialize_Bool

// Tutorial Goal helpers (exe-internal)
// ============================================================

// Object::Internal::Internal(this) — base constructor for Internal objects
// Resolved from dispatch slot 0x140152768. Returns this (void*).
typedef void* (*fn_ObjectInternal_Ctor_t)(void* self);
extern fn_ObjectInternal_Ctor_t fn_ObjectInternal_Ctor;

// FinishTutorialGoal_helper_01f0 (0x1401001f0) — tree insert for tutorial dialog setup
// Inserts tutorial entry into the tree at gameWorld+0x120 (tutorial map).
// Params: (treeRoot, tutorialExternalId, param3, screenHeight-0x1d0)
typedef void (*fn_TutorialTreeInsert_t)(void* treeRoot, uint64_t tutorialId, void* param3, int param4);
extern fn_TutorialTreeInsert_t fn_TutorialTreeInsert;

// FinishTutorialInstance_helper_1db0 (0x1400e1db0) — main tutorial step execution
// Called from FinishTutorialInstance::Slot5 (Execute/Update).
// Params: (tutorialInstance*, param2, param3, param4)
typedef void (*fn_TutorialStepExecute_t)(void* tutInst, void* param2, void* param3, uint64_t param4);
extern fn_TutorialStepExecute_t fn_TutorialStepExecute;

// TypeId for FinishTutorialGoal — stored at 0x1401c6240 in exe .rdata
extern uint32_t* g_TypeId_FinishTutorialGoal;

// Singleton External reference at 0x140151a80 — points to game config/time singleton
// Deref pattern: *(void**)(*(void**)slot + 4) gives screen height or similar int
extern void** g_pSingletonExref_151a80;

// TutorialInstance vtable pointer — 0x140160bf8 in exe .rdata
extern void** g_TutorialInstanceVtable;

// FUN_1400e1870(outIBP, elementPath, outDialogPtr) — find UI element by dotted path string.
// Parses "DialogType.ElementName" (or "DialogType.Controller.ElementName"),
// searches open dialogs for matching type, then finds child element by name.
// outIBP receives the found element's InternalBasePtr (0 if not found).
// outDialogPtr (optional, can be NULL) receives the dialog instance pointer.
// RCX=outIBP, RDX=elementPath(String*), R8=outDialogPtr(or NULL)
typedef void* (*fn_FindUIElementByPath_t)(void* outIBP, void* elementPath, void* outDialogPtr);
extern fn_FindUIElementByPath_t fn_FindUIElementByPath;

// ============================================================
// ComponentEntity dependencies
// ============================================================

// Component base class type ID (used with fn_IsDerivedFrom to verify components)
// Stored at exe .rdata 0x1401c72a8, filled at runtime by Component_Register.
extern uint32_t* g_TypeId_ComponentBase;

// ExeArrayResizeFill: 8-byte elements. Resizes array, copies existing entries,
// fills new entries with *fillValue (unlike fn_ExeArrayResize which zeroes them).
// Original: FUN_140003110 (used by ComponentEntity RefreshEntityFlags)
// Params: (arrayBase, newCount, fillValue) where arrayBase = {ptr, count, capacity}
typedef void (*fn_ExeArrayResizeFill_t)(void* arrayBase, int newCount, void* fillValue);
extern fn_ExeArrayResizeFill_t fn_ExeArrayResizeFill;

// ============================================================
// CitizenBehavior dependencies
// ============================================================

// CitizenBehavior::CountResources (0x140089ed0) — count resources per profession.
typedef void (*fn_CitizenBehavior_CountResources_t)(void* self);
extern fn_CitizenBehavior_CountResources_t fn_CitizenBehavior_CountResources;

// CitizenBehavior::CheckThresholds (0x14008c100) — check resource shortage thresholds.
typedef void (*fn_CitizenBehavior_CheckThresholds_t)(void* self, void* deltaTime);
extern fn_CitizenBehavior_CheckThresholds_t fn_CitizenBehavior_CheckThresholds;

// CitizenBehavior::CollectStats (0x14008a730) — collect population statistics.
typedef void (*fn_CitizenBehavior_CollectStats_t)(void* self);
extern fn_CitizenBehavior_CollectStats_t fn_CitizenBehavior_CollectStats;

// CitizenBehavior::ProcessPaths (0x1400d6f00) — process pending path calculations.
typedef uint64_t (*fn_CitizenBehavior_ProcessPaths_t)(void* pathGraph);
extern fn_CitizenBehavior_ProcessPaths_t fn_CitizenBehavior_ProcessPaths;

// CitizenBehavior::BuildPathGraph (0x1400d70c0) — build/rebuild path distance graph.
typedef void (*fn_CitizenBehavior_BuildPathGraph_t)(void* pathGraph);
extern fn_CitizenBehavior_BuildPathGraph_t fn_CitizenBehavior_BuildPathGraph;

// CitizenBehavior::ShuffleSchools (0x1400896a0) — Fisher-Yates shuffle on schools.
typedef void (*fn_CitizenBehavior_ShuffleSchools_t)(void* self);
extern fn_CitizenBehavior_ShuffleSchools_t fn_CitizenBehavior_ShuffleSchools;

// ProfessionResourceArray::Init (0x1400177c0) — allocate profession resource array.
typedef void (*fn_ProfResArray_Init_t)(void* arrayPtr, int count);
extern fn_ProfResArray_Init_t fn_ProfResArray_Init;

// StatsArray::Init (0x140017640) — allocate stats tracking array.
typedef void (*fn_StatsArray_Init_t)(void* base, int count);
extern fn_StatsArray_Init_t fn_StatsArray_Init;

// SummaryArray::Init (0x14000f090) — init summary/notification array.
typedef void (*fn_SummaryArray_Init_t)(void* base, int count);
extern fn_SummaryArray_Init_t fn_SummaryArray_Init;

// ============================================================
// Player/Tool module — shared engine APIs for tool reimpl
// ============================================================

// Graphics::Scene::AddObject — adds a Geometry to the scene, returns object ID
// IAT dispatch slot 0x140151370. RCX=Scene*, RDX=Geometry* -> returns uint32_t
typedef uint32_t (*fn_SceneAddObject_t)(void* scene, void* geometry);
extern fn_SceneAddObject_t fn_SceneAddObject;

// Graphics::Scene::RemoveObject — removes object from scene by ID
// IAT dispatch slot 0x140151360. RCX=Scene*, EDX=objectId
typedef void (*fn_SceneRemoveObject_t)(void* scene, uint32_t objectId);
extern fn_SceneRemoveObject_t fn_SceneRemoveObject;

// Math::Transform::FromYawPitchOffset — build transform from yaw + pitch + offset
// IAT dispatch slot 0x140151288. Static method, hidden retval ptr in RCX.
// RCX=outTransform, RDX=&yaw(Scalar), R8=&pitch(Scalar), R9=&offset(Vector3).
// Mangled: ?FromYawPitchOffset@Transform@Math@@SA?AV12@AEBVScalar@2@0AEBVVector3@2@@Z
// (the `0` after the first AEBVScalar means "same as previous arg" — second Scalar&)
typedef void (*fn_Transform_FromYawPitchOffset_t)(void* outTransform, const void* yaw, const void* pitch, const void* offset);
extern fn_Transform_FromYawPitchOffset_t fn_Transform_FromYawPitchOffset;

// Graphics::Geometry::SetTransform — apply transform to geometry/mesh
// IAT dispatch slot 0x1401512e0. RCX=Geometry*, RDX=Transform*
typedef void (*fn_Geometry_SetTransform_t)(void* geometry, void* transform);
extern fn_Geometry_SetTransform_t fn_Geometry_SetTransform;

// Graphics::MeshInstance::UpdateHierarchy — update mesh bone/transform hierarchy
// IAT dispatch slot 0x140152888. RCX=MeshInstance*
typedef void (*fn_MeshInstance_UpdateHierarchy_t)(void* mesh);
extern fn_MeshInstance_UpdateHierarchy_t fn_MeshInstance_UpdateHierarchy;

// CharacterComponent_impl_08c0 (0x1401008c0) — tile debug overlay rendering
// Submits colored tile arrays to the scene. Used by all Draw functions.
// RCX=self, RDX=Context*, R8=tileArray1, R9=tileArray2
typedef void (*fn_TileRender_t)(void* self, void* context, void* tileArray1, void* tileArray2);
extern fn_TileRender_t fn_TileRender;

// CharacterComponent_impl_f5d0 (0x14009f5d0) — single-tile terrain validity check
// Returns true if tile at (x,y) is valid for placement given terrain masks + height constraints.
typedef bool (*fn_TileValidityCheck_t)(int* mapGrid, int x, int y, uint32_t terrainMask,
    float heightRef, float heightTol, float param7, char param8, char param9, int64_t param10);
extern fn_TileValidityCheck_t fn_TileValidityCheck;

// FUN_1400dbf90 — fenced placement region parameter computation
// Computes fence layout params (bounds, rotation, etc.) from FencedTool state.
// Returns bool (true=valid placement).
typedef bool (*fn_FencedPlacementParams_t)(void* self, uint32_t* outParam2, uint32_t* outParam3,
    uint32_t* outParam4, uint32_t* outParam5, float* outParam6, uint32_t* outParam7);
extern fn_FencedPlacementParams_t fn_FencedPlacementParams;

// TradeWork_impl_0d00 (0x1400b0d00) — remove radius/highlight overlay
// RCX=*(g_pGameWorldSingleton+0x110), EDX=overlayId
typedef void (*fn_HighlightRemove_t)(void* highlightMgr, int overlayId);
extern fn_HighlightRemove_t fn_HighlightRemove;

// ============================================================
// EntityList dependencies (Agent A)
// ============================================================

// IBP BST tree insert (0x140021a30)
typedef void* (*fn_IBPTreeInsert_t)(void* treeRootPtr, void* outBuf, void* ibpPtr);
extern fn_IBPTreeInsert_t fn_IBPTreeInsert;

// IBP BST tree remove (0x14001a6e0)
typedef void (*fn_IBPTreeRemove_t)(void* treeRootPtr, void* node);
extern fn_IBPTreeRemove_t fn_IBPTreeRemove;

// System::Time::Serialize(this, Serializer&)
typedef void (*g_fn_Time_Serialize_t)(void* self, void* serializer);
extern g_fn_Time_Serialize_t g_fn_Time_Serialize;

// EntityList bucket+tree serialization helper (0x140021ba0)
typedef void (*fn_BucketTreeSerialize_t)(void* serializer, void* unused, void* bucketsPtr);
extern fn_BucketTreeSerialize_t fn_BucketTreeSerialize;

// ============================================================
// CitizenBehavior Path dependencies (Agent C)
// ============================================================

// Entity_impl_7fb0 (0x140087fb0) — get grid cell ID for an entity
typedef int (*fn_Entity_GetGridId_t)(void* entityIBP, float* outPos);
extern fn_Entity_GetGridId_t fn_Entity_GetGridId;

// BST insert for path distance cache (0x140025a10)
typedef void* (*fn_PathBST_Insert_t)(void* bstRootPtr, void* outNodePtr, void* keyPtr);
extern fn_PathBST_Insert_t fn_PathBST_Insert;

// Clear/reset pairs dynamic array (0x140025b30)
typedef void (*fn_PathPairs_Clear_t)(void* pairsArray);
extern fn_PathPairs_Clear_t fn_PathPairs_Clear;

// Post-path worker reassignment (0x1400d62a0)
typedef void (*fn_PathGraph_ReassignWorkers_t)(void* pathGraphBstContext);
extern fn_PathGraph_ReassignWorkers_t fn_PathGraph_ReassignWorkers;

// Post-path citizen assignment (0x1400d43b0)
typedef void (*fn_PathGraph_AssignCitizens_t)(void* pathGraph);
extern fn_PathGraph_AssignCitizens_t fn_PathGraph_AssignCitizens;

// Recursive BST free (0x14000e320)
typedef void (*fn_PathBST_FreeRecursive_t)(void* bstRootPtr, void* subtreeNode);
extern fn_PathBST_FreeRecursive_t fn_PathBST_FreeRecursive;

// ============================================================
// CitizenBehavior remaining core dependencies (Agent D)
// ============================================================

// Notification event helper (0x140095580)
typedef void (*fn_NotificationEvent_t)(void* notifMgr, int notifType, int severity, void* entityIBP);
extern fn_NotificationEvent_t fn_NotificationEvent;

// ============================================================
// CitizenBehavior FindWorking/FindIdle/FindStudent dependencies
// ============================================================

// PathDistance (FUN_140008830) — compute path distance between two map grid cells.
// Uses BuildStructure path first; falls back to Euclidean + grid size if no path found.
typedef int (*fn_PathDistance_t)(int* mapGrid, uint32_t gridIdA, uint32_t gridIdB);
extern fn_PathDistance_t fn_PathDistance;

// CitizenComponent::AssignHousing (FUN_140111ee0) — assign/clear housing for a citizen.
typedef void (*fn_CitizenComponent_AssignHousing_t)(void* citizen, void* housingPtr);
extern fn_CitizenComponent_AssignHousing_t fn_CitizenComponent_AssignHousing;

// g_TypeId_ResidenceComponent — already declared above (DAT_1401c7cf0)

// ============================================================
// M3.5 — MenuScene::Serialize generic helpers (Geometry/Transform serialize,
// dynamic-array resize/serialize, ExternalPtr serialize)
// ============================================================

// Geometry init helper — FUN_1400b00e0. Initializes a Graphics::Geometry-derived
// scene-mesh container at +0x230 bytes. Calls Graphics::Geometry base ctor, sets
// vtable, zeros internal fields, constructs embedded VertexBuffer/IndexBuffer pairs.
typedef void* (*fn_Geometry_Init_t)(void* self);
extern fn_Geometry_Init_t fn_Geometry_Init;

// Runtime DLL ctors used by Reimpl_Geometry_Init when the cleaned body runs
// (standalone mode, where fn_Geometry_Init resolves back to the wrapper).
//   ??0Geometry@Graphics@@QEAA@XZ      — Runtime 0x1800a4270
//   ??0VertexBuffer@Video@@QEAA@XZ     — Runtime 0x1800f55f0
//   ??0IndexBuffer@Video@@QEAA@XZ      — Runtime 0x1800f3490
typedef void (*fn_Graphics_Geometry_Ctor_t)(void* self);
extern fn_Graphics_Geometry_Ctor_t fn_Graphics_Geometry_Ctor;
typedef void (*fn_Video_VertexBuffer_Ctor_t)(void* self);
extern fn_Video_VertexBuffer_Ctor_t fn_Video_VertexBuffer_Ctor;
typedef void (*fn_Video_IndexBuffer_Ctor_t)(void* self);
extern fn_Video_IndexBuffer_Ctor_t fn_Video_IndexBuffer_Ctor;

// Transform serialize wrapper — FUN_1400988c0. Wraps a Stream in a Serializer
// then serializes Math::Transform + Vector3 (at +0x1c) + 3 floats (+0x28..+0x30).
typedef void (*fn_Transform_SerializeWrapper_t)(void* transform, void* stream);
extern fn_Transform_SerializeWrapper_t fn_Transform_SerializeWrapper;

// Math::Transform::Serialize — Runtime export (28-byte Transform pos/rot/scale).
typedef void (*fn_Math_Transform_Serialize_t)(void* transform, void* serializer);
extern fn_Math_Transform_Serialize_t fn_Math_Transform_Serialize;

// Math::Vector3::Serialize — Runtime export (12-byte Vector3).
typedef void (*fn_Math_Vector3_Serialize_t)(void* vector3, void* serializer);
extern fn_Math_Vector3_Serialize_t fn_Math_Vector3_Serialize;

// Math::Vector4::Serialize — Runtime export (16-byte Vector4).
// Slot in exe is 0x140151ed0; mangled name
// `?Serialize@Vector4@Math@@QEAAXAEAVSerializer@IO@@@Z`.
typedef void (*fn_Math_Vector4_Serialize_t)(void* vector4, void* serializer);
extern fn_Math_Vector4_Serialize_t fn_Math_Vector4_Serialize;

// System::Color::Serialize — Runtime export (4-byte ARGB color).
// Slot in exe is 0x140151ee0; mangled name
// `?Serialize@Color@System@@QEAAXAEAVSerializer@IO@@@Z`.
typedef void (*fn_System_Color_Serialize_t)(void* color, void* serializer);
extern fn_System_Color_Serialize_t fn_System_Color_Serialize;

// IO::Serializer::Serialize(float*, char* name) — single-float serialize. There
// are several Serialize overloads on Serializer (int/uint/bool/float/etc.); this
// is the (float*) variant, mangled as ?Serialize@Serializer@IO@@QEAAXAEAMPEBD@Z.
typedef void (*fn_Serializer_Serialize_Float_t)(void* serializer, float* value, const char* name);
extern fn_Serializer_Serialize_Float_t fn_Serializer_Serialize_Float;

// Dynamic-array resize helpers — three flavors that share the same capacity-
// growth structure but differ by element stride. The argument is a pointer to a
// 16-byte array header: { void* data; int32 count; int32 capacity; }.
//
//   stride 0x20 — FUN_14001baf0  (alignment 4 on alloc, copies 8 dwords/elem)
//   stride 0x08 — FUN_140001bb0  (alignment 8 on alloc, copies 1 qword/elem)
//   stride 0x14 — FUN_14001bf10  (alignment 4 on alloc, copies 5 dwords/elem)
typedef void (*fn_Array_Resize_t)(void* arrayHeader, int newCount);
extern fn_Array_Resize_t fn_Array_Resize_Stride20;  // 0x14001baf0
extern fn_Array_Resize_t fn_Array_Resize_Stride08;  // 0x140001bb0
extern fn_Array_Resize_t fn_Array_Resize_Stride14;  // 0x14001bf10

// Array element serialize helper for the +0x30 array on MenuScene (stride 0x20).
// Each element packs 7 floats + 2 shorts; serializes them in order.
// FUN_14001bbf0.
typedef void (*fn_Array_Serialize_Stride20_t)(void* serializer, uint32_t count, void* arrayBase);
extern fn_Array_Serialize_Stride20_t fn_Array_Serialize_Stride20;

// ExternalPtr-array serialize helpers (CASCADE — FindOrLoad + IsDerivedFrom on
// each element). Both ~250 instructions, byte-identical bodies, only the
// BaseTypeInfo* slot differs:
//   stride 0x08 array of GraphicsMesh exrefs — FUN_14001bcc0  (typeInfo @ slot 0x140151f20)
//   stride 0x08 array of Particle    exrefs — FUN_14001bff0  (typeInfo @ slot 0x140151420)
// param_4 in the Ghidra decompile is decompiler noise — R9 is never set by the
// caller (MenuScene::Slot5 / Geometry::Serialize), so the variable is dead.
typedef void (*fn_Array_Serialize_External_t)(void* serializer, int count, void* arrayBase);
extern fn_Array_Serialize_External_t fn_Array_Serialize_External_GraphicsMesh;  // 0x14001bcc0
extern fn_Array_Serialize_External_t fn_Array_Serialize_External_Particle;       // 0x14001bff0

// Single ExternalPtr serialize helpers (CASCADE — FindOrLoad + IsDerivedFrom).
// Identical bodies; only the typeInfo source differs:
//   GraphicsMesh exref (+0x70) — FUN_14001b8e0  (typeInfo @ slot 0x140151f20)
//   Weather      exref (+0x80) — FUN_1400196a0  (typeId  @ direct dword 0x1401c6198)
typedef void (*fn_ExternalPtr_Serialize_t)(void* refField, void* serializer);
extern fn_ExternalPtr_Serialize_t fn_ExternalPtr_Serialize_GraphicsMesh;  // 0x14001b8e0
extern fn_ExternalPtr_Serialize_t fn_ExternalPtr_Serialize_Weather;        // 0x1400196a0

// Generic shared body, parameterized by typeId. Used internally by both single
// and array variants in standalone reimpl.
//   typeIdPtr: when non-null, *typeIdPtr is the type id to validate against.
//              when null, a printf warning is emitted and validation is skipped.
typedef void (*fn_ExternalPtr_Serialize_Generic_t)(void* refField, void* serializer, const uint32_t* typeIdPtr);

// ============================================================
// M3.5 Path B session 2 — 0x60-byte struct array lifecycle helpers
// ============================================================
// Used by Geometry::Serialize (FUN_1400b04d0) and the version-gated extra
// FUN_1400af1a0 to manage the dynamic array at +0x200 (count +0x208) of
// 0x60-byte structs. Array growth needs a per-element copy-ctor (it owns
// embedded sub-arrays at +0x08 and +0x18); array shrink needs a sized
// destructor that releases those embedded sub-arrays' MemAlloc'd buffers.
//
// 0x60-byte element layout (from the copy-ctor decompile, FUN_14001f5e0):
//   +0x00 : qword  (8B)              — copied raw
//   +0x08 : array<stride=0x14>       — { void* data; int32 count; int32 cap }, deep-copy via fn_Struct60B_HelperFdb0
//   +0x18 : array<stride=0x02>       — { void* data; int32 count; int32 cap }, deep-copy via fn_Struct60B_HelperFe60
//   +0x28 : dword                    — copied raw
//   +0x2c : dword                    — copied raw
//   +0x30 : dword                    — copied raw
//   +0x34 : dword                    — copied raw
//   +0x38 : dword                    — copied raw
//   +0x3c : byte                     — copied raw
//   +0x40 : dword                    — copied raw
//   +0x44 : byte                     — copied raw
//   +0x48 : dword (float?)           — copied raw
//   +0x4c : dword (float?)           — copied raw
//   +0x50 : dword (float?)           — copied raw
//   +0x54 : dword (float?)           — copied raw
//   +0x58 : dword (float?)           — copied raw
//   +0x5c : byte                     — copied raw
//   +0x5d : byte                     — copied raw
//
// On dtor: only the two embedded sub-arrays at +0x08 and +0x18 own heap memory
// (MemAlloc'd via fn_MemAlloc); the rest of the struct is plain bytes.

// Resize helper for the 0x60-byte struct array at +0x200 — FUN_14001f250.
// arrayHeader layout: { void* data; int32 count; int32 cap }
// Calls Struct60B_CopyCtor on grow (per-element), Struct60B_DtorRange on shrink.
typedef void (*fn_Struct60B_Resize_t)(void* arrayHeader, unsigned int newCount);
extern fn_Struct60B_Resize_t fn_Struct60B_Resize;  // 0x14001f250

// Copy-ctor for one 0x60B element — FUN_14001f5e0.
// Returns dst (param_1). Deep-copies the two embedded sub-arrays via
// the helpers below; raw-copies all other fields.
typedef void* (*fn_Struct60B_CopyCtor_t)(void* dst, const void* src);
extern fn_Struct60B_CopyCtor_t fn_Struct60B_CopyCtor;  // 0x14001f5e0

// Per-element destructor loop — FUN_14001fb80.
// Walks `count` elements starting at `first`, releasing each element's two
// embedded sub-array buffers (the MemAlloc'd `data` slots at +0x08 and +0x18)
// and zeroing the headers. Does NOT free the array buffer itself — the caller
// (Resize) does that after the loop returns.
//   param_1 (RCX): unused/garbage; original passes the loop iVar4 leftover.
//   param_2 (RDX): pointer to first element to dtor.
//   param_3 (R8) : count of elements to dtor.
typedef void (*fn_Struct60B_DtorRange_t)(void* unused, void* first, int count);
extern fn_Struct60B_DtorRange_t fn_Struct60B_DtorRange;  // 0x14001fb80

// Sub-helpers used by the copy-ctor for the two embedded sub-arrays.
// Both perform "free old buffer, alloc new sized buffer, deep-copy elements".
//
//   Stride 0x14 (5 dwords / 20 bytes), alignment 2 — FUN_14001fdb0
//   Stride 0x02 (uint16),               alignment 2 — FUN_14001fe60
//
// Signature: returns dst, takes (dst, src) — both 16-byte array headers
// matching the same layout as Struct60B_Resize's arrayHeader.
typedef void* (*fn_Struct60B_SubArrayCopy_t)(void* dst, const void* src);
extern fn_Struct60B_SubArrayCopy_t fn_Struct60B_SubArrayCopyStride14;  // 0x14001fdb0
extern fn_Struct60B_SubArrayCopy_t fn_Struct60B_SubArrayCopyStride02;  // 0x14001fe60

// ============================================================
// M3.5 Path B session 5 — additional Geometry::Serialize resize helpers
// ============================================================
// All five share the standard array-resize shape (header = { void* data;
// int32 count; int32 cap }). They differ in element stride and MemAlloc
// alignment. The array-of-arrays variant additionally calls a deep-clone
// helper per outer element to copy the inner Vector2 array.
//
//   stride 0x02, align 2 — TradeWork_impl_sub_c890         @ 0x14001c890
//                          (uint16 array, used for index-buffer + sub-arrays)
//   stride 0x10, align 4 — TradeWork_impl_b260_sub_efd0    @ 0x14001efd0
//                          (int4 / Vec4 stride for arrays at +0x1C0/+0x1D0)
//   stride 0x10, align 8 — TradeWork_impl_sub_f0a0         @ 0x14001f0a0
//                          (array-of-arrays — deep-clones inner via helper_ff00)
//   stride 0x10, align 2 — FUN_14001ef10                   @ 0x14001ef10
//                          (mesh-vertex 16-byte struct copies 2 qwords/elem)
//   stride 0x04, align 4 — FUN_1400021d0                   @ 0x1400021d0
//                          (int array — used for +0x220 in Geometry::Serialize)
extern fn_Array_Resize_t fn_Array_Resize_Stride02;       // 0x14001c890  M3.5 Path B session 5
extern fn_Array_Resize_t fn_Array_Resize_Stride16A;      // 0x14001efd0  M3.5 Path B session 5
extern fn_Array_Resize_t fn_Array_Resize_ArrayOfArrays;  // 0x14001f0a0  M3.5 Path B session 5
extern fn_Array_Resize_t fn_Array_Resize_Stride16B;      // 0x14001ef10  M3.5 Path B session 5
extern fn_Array_Resize_t fn_Array_Resize_Stride04;       // 0x1400021d0  M3.5 Path B session 5

// ============================================================
// Runtime DLL primitives used by the MenuScene serialize helpers
// (resolved by mangled name; many already exist, listed here for reference)
// ============================================================

// IO::Serializer::GetEncoding — dispatch slot 0x1401529f8.
// Returns Encoding* (used to query IsLoading/IsSaving via slot vtable+0x10).
typedef void* (*fn_Serializer_GetEncoding_t)(void* self);
extern fn_Serializer_GetEncoding_t fn_Serializer_GetEncoding;

// IO::Serializer::Serialize(uint&, const char* name) — dispatch slot 0x140152a50.
// (The bool variant at 0x140152a58 is already declared as g_fn_Serialize_Bool.)
typedef void (*fn_Serializer_Serialize_Uint_t)(void* self, uint32_t* field, const char* name);
extern fn_Serializer_Serialize_Uint_t fn_Serializer_Serialize_Uint;

// Object::ExternalFactory::IsReferenceModeEnabled — dispatch slot 0x140151218.
typedef bool (*fn_ExternalFactory_IsReferenceModeEnabled_t)(void* self);
extern fn_ExternalFactory_IsReferenceModeEnabled_t fn_ExternalFactory_IsReferenceModeEnabled;

// Object::ExternalFactory::GetReferenceId — dispatch slot 0x140151220.
typedef uint32_t (*fn_ExternalFactory_GetReferenceId_t)(void* self, void* external);
extern fn_ExternalFactory_GetReferenceId_t fn_ExternalFactory_GetReferenceId;

// Object::ExternalFactory::GetPathForReferenceId — dispatch slot 0x140151228.
// Member function returning Filename: hidden retval ABI -> RCX=this, RDX=retbuf, R8=refId.
typedef void* (*fn_ExternalFactory_GetPathForReferenceId_t)(void* self, void* retFilename, uint32_t refId);
extern fn_ExternalFactory_GetPathForReferenceId_t fn_ExternalFactory_GetPathForReferenceId;

// System::Filename::operator= — dispatch slot 0x1401529e0. Returns dst (Filename&).
typedef void* (*fn_Filename_AssignOp_t)(void* dst, const void* src);
extern fn_Filename_AssignOp_t fn_Filename_AssignOp;

// ============================================================
// MenuScene serialize helper — typeInfo storage (standalone-only)
// ============================================================
// In the original exe these are .rdata BaseTypeInfo*-pointer slots filled by
// the Runtime DLL's type-registration plumbing during startup. Standalone
// either resolves them from Runtime DLL exports or leaves them null and the
// helpers gracefully no-op (cascade fix is a later step).
//
// The exe disasm reads typeId via a single deref:
//     MOV RCX,[0x140151f20]  ; pointer to BaseTypeInfo
//     MOV EDX,[RCX]          ; *(uint*)BaseTypeInfo == typeId
extern uint32_t** g_ppTypeInfo_GraphicsMesh;   // .rdata slot 0x140151f20
extern uint32_t** g_ppTypeInfo_Particle;       // .rdata slot 0x140151420
extern uint32_t*  g_pTypeId_Weather;           // direct dword 0x1401c6198

// M3.5 Path B session 3 — additional typed-ExternalPtr serialize helper.
// FUN_140007140 is byte-identical to FUN_14001b8e0 (helper #9, GraphicsMesh)
// except for the typeInfo source slot. Reads BaseTypeInfo* from .rdata slot
// 0x140151350. Confirmed via shared xrefs: CharacterComponent_impl_08c0
// FindOrLoads "Terrain/Selection.rsc" then calls
// Graphics::MaterialInstance::Setup on the result, validating against the
// same slot — so 0x140151350 is the BaseTypeInfo* pointer slot for
// Graphics::MaterialInstance.
typedef void (*fn_ExternalPtr_Serialize_Typed_t)(void* refField, void* serializer);
extern fn_ExternalPtr_Serialize_Typed_t fn_ExternalPtr_Serialize_Typed_7140;  // 0x140007140
extern uint32_t** g_ppTypeInfo_MaterialInstance;  // .rdata slot 0x140151350

// ============================================================
// M3.5 Path B session 4 — per-element 0x60B serialize + typed-ExternalPtr
// array serialize (Geometry::Serialize step 10 + step 13 cascades)
// ============================================================
//
// TradeWork_impl_ffa0 (0x14001ffa0) — ~70 instructions. Per-element serializer
// for the 0x60B struct array at MenuScene::geometry +0x200 (and the version-
// gated +0x1F0 array). For each 0x60-byte element it:
//   1. CALL FUN_140007140(elem+0x00, serializer)  — typed ExternalPtr serialize
//      (Graphics::MaterialInstance via slot 0x140151350) — CASCADE.
//   2. Serialize 5 ints (+0x28 .. +0x38)
//   3. Serialize 1 byte (+0x3c)
//   4. Serialize 1 int (+0x40)
//   5. Serialize 1 bool (+0x44)
//   6. Serialize 5 floats (+0x48 .. +0x58)
//   7. Serialize 1 bool (+0x5c)
//   8. version >= 2: Serialize 1 bool (+0x5d)
//   9. version >= 1: TradeWork_impl_sub_f9c0 — uint16-stride sub-array serialize
//      (count + raw bytes) reading from the elem+0x08 array header.
typedef void (*fn_Struct60B_Serialize_t)(void* serializer, int count, void* arrayBase);
extern fn_Struct60B_Serialize_t fn_Struct60B_Serialize;  // 0x14001ffa0  M3.5 Path B session 4

// TradeWork_impl_sub_0150 (0x140020150) — ~150 instructions, byte-identical
// shape to FUN_14001bcc0/FUN_14001bff0 (Array_Serialize_External). Loops
// `count` times, per-element FindOrLoad + IsDerivedFrom against the
// BaseTypeInfo* at .rdata slot 0x140151350 (Graphics::MaterialInstance — same
// slot as FUN_140007140; verified via disassembly:
//   140020340: MOV RAX, qword ptr [0x140151350]
//   140020347: MOV EDX, dword ptr [RAX]
//   14002034c: CALL [0x140152a98] ; IsDerivedFrom).
// Used by Geometry::Serialize step 13 for the +0x210 array (count +0x218).
typedef void (*fn_Array_Serialize_External_Typed_t)(void* serializer, int count, void* arrayBase);
extern fn_Array_Serialize_External_Typed_t fn_Array_Serialize_External_MaterialInstance;  // 0x140020150  M3.5 Path B session 4

// ============================================================
// M3.5 Path B session 6 — heightmap mesh-build path
// ============================================================
//
// Three exe-internal helpers from `TradeWork_impl_04d0` step 5 (mesh-build
// pipeline). All three live in the 0x1400acxxx / 0x1400aexxx range — NOT
// the 0x1400Cxxxx area the original recon claimed.
//
// Verified addresses (Ghidra search_functions_enhanced):
//   sub_c1e0     @ 0x1400ac1e0 — top-level mesh builder (~150 instr, 5 xrefs)
//   FUN_ac880    @ 0x1400ac880 — per-cell vertex compute (~70 instr)
//   helper_e270  @ 0x1400ae270 — vertex normal compute (~40 instr, 3 xrefs)
//
// Math constants used by the per-cell compute (also verified in Ghidra):
//   g_fUVScale       @ 0x14017214c = 1.0f
//   DAT_140172214    @ 0x140172214 = 127.5f
//   DAT_140172264    @ 0x140172264 = 16384.0f
//   DAT_140172270    @ 0x140172270 = -1/256.0
//   DAT_140172174    @ 0x140172174 = 2.0f
// (Embedded inline as constexpr in code/Game/Game/serialize_heightmap_mesh.cpp.)
//
// Runtime DLL primitives invoked (resolved via dispatch slots and/or mangled
// export fallback in the reimpl body itself — no global wiring needed):
//   ?Create@VertexBuffer@Video@@QEAA_NH_NPEAX@Z   (slot 0x140152668)
//   ?Set@VertexBuffer@Video@@QEAA_NHHPEAX@Z       (slot 0x140152690)
//   ?Create@IndexBuffer@Video@@QEAA_NH_NPEAX@Z    (slot 0x140152660)
//   ?Set@IndexBuffer@Video@@QEAA_NHHPEAX@Z        (slot 0x140152698)
//   ?Normalize@Vector3@Math@@QEBA?AV12@XZ         (slot 0x1401512c0)
typedef void  (*fn_TradeWork_sub_c1e0_t)   (void* self);
typedef void  (*fn_TradeWork_FUN_ac880_t)  (void* self, int colStart, int rowStart,
                                             int cellSize_x, int cellSize_y,
                                             int16_t* vertOut);
typedef void* (*fn_TradeWork_helper_e270_t)(void* self, void* retbuf, int col, int row);
typedef void  (*fn_Geometry_ChunkUpdate_t) (void* self);
extern fn_TradeWork_sub_c1e0_t    fn_TradeWork_sub_c1e0;     // 0x1400ac1e0  M3.5 Path B session 6
extern fn_TradeWork_FUN_ac880_t   fn_TradeWork_FUN_ac880;    // 0x1400ac880  M3.5 Path B session 6
extern fn_TradeWork_helper_e270_t fn_TradeWork_helper_e270;  // 0x1400ae270  M3.5 Path B session 6
extern fn_Geometry_ChunkUpdate_t  fn_Geometry_ChunkUpdate;   // 0x1400aad50  M3.5 — chunk-streaming

// ============================================================
// M3.5 Path B session 7 — image-buffer heightmap upload helper
// ============================================================
//
// `TradeWork_impl_sub_c780` @ 0x1400ac780 (~30 instructions). Called from
// Geometry::Serialize step 14 (final IsLoading-only block) to upload the
// heightmap raw bytes at self+0x140 into a Video::ImageBuffer at self+0x148
// using the custom NoiseUI ImageBuffer vtable (g_NoiseUI_ImageBufferVtable @
// 0x14015c8c0 — exe-resident, also already used by NoiseUI / ModDialog).
//
// Body shape (verified via Ghidra disassembly @ 0x1400ac780):
//   1. img = MemAlloc(0x68, 8)
//   2. ImageBuffer::ImageBuffer(img)              // Runtime DLL ctor
//   3. *(void**)img = &g_NoiseUI_ImageBufferVtable
//   4. self+0x148 = img
//   5. img->vtbl[0x48/8](img, 0, 2, 5, 0, w, w, 2, 1, 1, 1, 0)
//        // Create(type=0, usage=2, format=5, mipLevels=0, w, h, FilterMode=2,
//        //        AddressMode=1, AddressMode=1, AddressMode=1, mipData=null)
//   6. Device::Lock(*Device::_singleton)
//   7. ImageBuffer::CopyToTexture(img, self+0x140, w, w)
//   8. Device::Unlock(*Device::_singleton)
//
// All Runtime primitives needed are ALREADY wired in engine_funcs.h:
//   fn_ImageBuffer_Ctor          (??0ImageBuffer@Video@@QEAA@XZ)
//   fn_ImageBuffer_CopyToTexture (?CopyToTexture@ImageBuffer@Video@@QEAA_NPEAXHH@Z)
//   fn_VideoDevice_Lock          (?Lock@Device@Video@@QEAAXXZ, ordinal 3214)
//   fn_VideoDevice_Unlock        (?Unlock@Device@Video@@QEAAXXZ, ordinal 4462)
//   g_pVideoDeviceSingleton      (Runtime 0x1801fe7a8)
//   g_NoiseUI_ImageBufferVtable  (exe 0x14015c8c0, set in Phase Q)
//
// The vtable +0x48 slot ("Create") is reached via the exe-resident vtable —
// see code/Game/Dialog/noiseui.cpp lines 290-299 for the established pattern
// of dispatching through `*(void**)imageBuffer` slot index 9. The thin exe
// thunk at 0x14003aa20 forwards to Runtime via .rdata slot 0x140151a88; the
// underlying Runtime export is:
//   ?Create@ImageBuffer@Video@@UEAA_NW4ImageType@12@W4Usage@12@W4Format@12@
//      IHHW4FilterMode@12@W4AddressMode@12@44PEAV?$Array@V?$Array@E@
//      Collection@@@Collection@@@Z
//
// In standalone mode the vtable storage at 0x14015c8c0 needs to live in our
// .data and contain function pointers we provide (or thin pass-throughs that
// resolve the Runtime exports at call-time). Since NoiseUI/ModDialog already
// dereference the vtable directly via slot index 9, the standalone-side
// vtable scaffolding is shared — see standalone/address_registry.cpp
// `RegisterNoiseUIImageBufferVtable` (added in this session).
typedef void (*fn_TradeWork_sub_c780_t)(void* self);
extern fn_TradeWork_sub_c780_t fn_TradeWork_sub_c780;  // 0x1400ac780  M3.5 Path B session 7

// ============================================================
// M3.5 Path B session 8 — final assembly: Geometry::Serialize body + extra
// ============================================================
//
// Two top-level serializers driven from MenuScene::Serialize @ 0x1400a3630
// (callsites verified via Ghidra disassembly):
//
//   0x1400a36d5: CALL 0x1400b04d0   ; TradeWork_impl_04d0  — Geometry::Serialize body
//   0x1400a36f0: CALL 0x1400af1a0   ; TradeWork_impl_f1a0  — version-gated extra
//
// `TradeWork_impl_04d0` (~700 bytes) walks 15 steps that own the heightmap,
// vertex/index buffers, image-buffer upload, two parallel int4 arrays, an
// array-of-arrays, the 0x60B struct array, and four typed-ExternalPtr
// cascades (MaterialInstance via FUN_140007140 and FUN_140020150).
// `TradeWork_impl_f1a0` (~196 bytes) is the version-gated extra schema-stream
// (constructs a separate IO::Serializer with version 4) reading the OLD
// schema's +0x1F0 + +0x200 arrays before the new-schema +0x210.
//
// Both call into the M3.5 session 2-7 helper graph plus IO::Serializer's
// dispatch slots (0x140152ad8 ctor, 0x140152ad0 dtor, 0x140152ac8 int,
// 0x140152410 byte, 0x140152a58 bool, 0x140152a60 float, 0x140152a48 GetVer,
// 0x140152488 ushort-array, 0x1401523f8 / 0x140152400 sized-raw,
// 0x140152618 SerializeBytes, 0x1401526b8 Vector2::Serialize). The cleaned-C
// body resolves these slots lazily and per-step soft-fails on cascade misses.
typedef void (*fn_TradeWork_impl_04d0_t)(void* self, void* stream);
extern fn_TradeWork_impl_04d0_t fn_TradeWork_impl_04d0;  // 0x1400b04d0  M3.5 Path B session 8
typedef void (*fn_TradeWork_impl_f1a0_t)(void* self, void* stream);
extern fn_TradeWork_impl_f1a0_t fn_TradeWork_impl_f1a0;  // 0x1400af1a0  M3.5 Path B session 8

// First-class typedefs for the IO::Serializer dispatch slots that session 8
// exercises heavily. Session 4 had these as locally-cached lazy-resolve
// statics; session 8 promotes them to externs so geometry_serialize.cpp +
// future serialize sites can share. Pointers themselves are resolved in
// EngineFuncsInit() — fall back to ResolveExport for the standalone-mode
// case where the .rdata slot is unfilled until Runtime DLL loads.
//
// All slots verified by Ghidra MCP: `disassemble_function 0x1400b04d0 CALL`.
// M3.5 Path B session 8
typedef void (*fn_Ser_SerInt_t      )(void* serializer, int32_t* field, const char* name);
typedef void (*fn_Ser_SerByte_t     )(void* serializer, uint8_t* field, const char* name);
typedef void (*fn_Ser_SerFloat_t    )(void* serializer, float*   field, const char* name);
typedef void (*fn_Ser_SerUShortArr_t)(void* serializer, int32_t count, uint16_t* data, const char* name);
typedef void (*fn_Ser_SerSizedRaw_t )(void* serializer, int32_t bytes, void* ptr, const char* name);
typedef void (*fn_Ser_SerByteArr_t  )(void* serializer, int32_t count, void* ptr, const char* name);
extern fn_Ser_SerInt_t       fn_Ser_Slot_SerInt_AC8;       // 0x140152ac8  M3.5 Path B session 8
extern fn_Ser_SerByte_t      fn_Ser_Slot_SerByte_410;      // 0x140152410  M3.5 Path B session 8
extern fn_Ser_SerFloat_t     fn_Ser_Slot_SerFloat_A60;     // 0x140152a60  M3.5 Path B session 8
extern fn_Ser_SerUShortArr_t fn_Ser_Slot_SerUShortArr_488; // 0x140152488  M3.5 Path B session 8
extern fn_Ser_SerSizedRaw_t  fn_Ser_Slot_SerSizedRaw_3F8;  // 0x1401523f8  M3.5 Path B session 8
extern fn_Ser_SerSizedRaw_t  fn_Ser_Slot_SerSizedRaw_400;  // 0x140152400  M3.5 Path B session 8
extern fn_Ser_SerByteArr_t   fn_Ser_Slot_SerByteArr_618;   // 0x140152618  M3.5 Path B session 8

// ============================================================
// M3.5 — Geometry vtable draw primitives (slot 0x14015f110)
// ============================================================
//
// Real implementations of the 9 vtable slots at exe 0x14015f110, decompiled
// from the original derived "TerrainGenerator" Geometry class. The vtable
// layout (verified via Ghidra read of 0x14015f110):
//   +0x00  dtor                        (exe 0x14001ECC0)
//   +0x08  chunk-streaming/heightmap   (exe 0x1400AED40 → JMP 0x1400AAD50)
//   +0x10  per-item Draw               (exe 0x1400AEA10)
//   +0x18  collect Draw                (exe 0x1400AE490)
//   +0x20  UpdateAttached              (Runtime IAT thunk → 0x140151F28)
//   +0x28  GetSubTransform             (Runtime IAT thunk → 0x140152188)
//   +0x30  GetInstanceTransform        (Runtime IAT thunk → 0x140152280)
//   +0x38  SetSharedInstanceData       (Runtime IAT thunk → 0x140152278)
//   +0x40  Clean                       (Runtime IAT thunk → 0x140152190)
//
// Runtime export pointers for the per-item / collect Draw bodies. All are
// resolved via ResolveExport in EngineFuncsInit (fall back to dispatch slot
// reads when ResolveExport fails). Both DLL and standalone modes use these.

// Renderer / Camera / Frustum primitives (used by collect Draw)
typedef void* (*fn_Renderer_GetCullCamera_t)(void* renderer);
extern fn_Renderer_GetCullCamera_t fn_Renderer_GetCullCamera;
// ?GetCullCamera@Renderer@Graphics@@QEAAAEBVCamera@2@XZ

typedef void  (*fn_Renderer_AddDrawItem_t)(void* renderer, void* geometry,
                                           void* material, uint16_t key1,
                                           uint16_t key2, float depth,
                                           float uvScale, int flag,
                                           void* extraPtr, uint32_t extraInt);
extern fn_Renderer_AddDrawItem_t fn_Renderer_AddDrawItem;
// ?AddDrawItem@Renderer@Graphics@@QEAAXPEBVGeometry@2@PEBVMaterialInstance@2@GGMMHPEBXI@Z

typedef int   (*fn_Renderer_GetShader_t)(void* renderer);
extern fn_Renderer_GetShader_t fn_Renderer_GetShader;
// ?GetShader@Renderer@Graphics@@QEBAHXZ

// Member fns with hidden return: RCX=this, RDX=retval, R8+=args
typedef void* (*fn_Camera_GetFrustum_t)(void* camera, void* retval);
extern fn_Camera_GetFrustum_t fn_Camera_GetFrustum;
// ?GetFrustum@Camera@Graphics@@QEBA?AVFrustum@Volume@@XZ

typedef uint32_t (*fn_Frustum_Contains_t)(void* frustum, void* box, uint32_t mask);
extern fn_Frustum_Contains_t fn_Frustum_Contains;
// ?Contains@Frustum@Volume@@QEBAIAEBVBox@2@I@Z

typedef void* (*fn_Frustum_MulTransform3_t)(void* frustum, void* retval, void* xform3);
extern fn_Frustum_MulTransform3_t fn_Frustum_MulTransform3;
// ??DFrustum@Volume@@QEBA?AV01@AEBVTransform3@Math@@@Z (operator*)

// Math primitives (member fn hidden retval where applicable)
typedef void* (*fn_Geometry_GetTransform_t)(void* geometry, void* retval);
extern fn_Geometry_GetTransform_t fn_Geometry_GetTransform;
// ?GetTransform@Geometry@Graphics@@QEBA?AVTransform@Math@@XZ

typedef void* (*fn_Transform_Inverse_t)(void* xform, void* retval);
extern fn_Transform_Inverse_t fn_Transform_Inverse;
// ?Inverse@Transform@Math@@QEBA?AV12@XZ

typedef void* (*fn_Transform_AsMatrix_t)(void* xform, void* retval);
extern fn_Transform_AsMatrix_t fn_Transform_AsMatrix;
// ?AsMatrix@Transform@Math@@QEBA?AVMatrix@2@XZ

typedef void* (*fn_Transform3_Copy_Ctor_t)(void* self, const void* other);
extern fn_Transform3_Copy_Ctor_t fn_Transform3_Copy_Ctor;
// ??0Transform3@Math@@QEAA@AEBV01@@Z

typedef void* (*fn_Vector3_FromFloats_Ctor_t)(void* self, float x, float y, float z);
extern fn_Vector3_FromFloats_Ctor_t fn_Vector3_FromFloats_Ctor;
// ??0Vector3@Math@@QEAA@MMM@Z

typedef void* (*fn_InterpolatedState_Transform_Lerp_t)(void* self, void* retval, void* scalar);
extern fn_InterpolatedState_Transform_Lerp_t fn_InterpolatedState_Transform_Lerp;
// ?Lerp@?$InterpolatedState@VTransform@Math@@@Graphics@@QEBA?AVTransform@Math@@AEBVScalar@4@@Z

// Video::Context primitives (used by per-item Draw)
typedef void  (*fn_Context_SetTransform_t)(void* context, int slot, void* matrix);
extern fn_Context_SetTransform_t fn_Context_SetTransform;
// ?SetTransform@Context@Video@@QEAAXHAEBVMatrix@Math@@@Z

typedef void  (*fn_Context_SetColor_t)(void* context, void* vector4);
extern fn_Context_SetColor_t fn_Context_SetColor;
// ?SetColor@Context@Video@@QEAAXAEBVVector4@Math@@@Z

typedef void  (*fn_Context_SetVertexBuffer_t)(void* context, void* vb,
                                              int p0, int p1, int stride);
extern fn_Context_SetVertexBuffer_t fn_Context_SetVertexBuffer;
// ?SetVertexBuffer@Context@Video@@QEAAXPEBVVertexBuffer@2@HHH@Z

typedef void  (*fn_Context_SetImageBuffer_t)(void* context, void* img, int slot);
extern fn_Context_SetImageBuffer_t fn_Context_SetImageBuffer;
// ?SetImageBuffer@Context@Video@@QEAAXPEBVImageBuffer@2@H@Z

typedef bool  (*fn_Context_BeginIndexedVertices_t)(void* context, int p0, int p1, int p2);
extern fn_Context_BeginIndexedVertices_t fn_Context_BeginIndexedVertices;
// ?BeginIndexedVertices@Context@Video@@QEAA_NHHH@Z

typedef void  (*fn_Context_EndIndexedVertices_t)(void* context, int primType);
extern fn_Context_EndIndexedVertices_t fn_Context_EndIndexedVertices;
// ?EndIndexedVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z

typedef void  (*fn_Context_DrawIndexed_t)(void* context, int primType,
                                          int a, int b, int c, int d, int e);
extern fn_Context_DrawIndexed_t fn_Context_DrawIndexed;
// ?DrawIndexed@Context@Video@@QEAAXW4PrimitiveType@12@HHHHH@Z

// Inherited base methods of Graphics::Geometry (used by +0x20..+0x40 thunks).
// These are dispatched via .rdata IAT slots in the original; in standalone we
// resolve them once and call directly. Signatures inferred from Runtime export
// mangled names — none are heavily used by the menu-scene render path so the
// arg layouts are best-effort.
typedef void  (*fn_Geometry_UpdateAttached_t)(void* self);
extern fn_Geometry_UpdateAttached_t fn_Geometry_UpdateAttached;
// ?UpdateAttached@Geometry@Graphics@@UEAAXXZ

typedef void* (*fn_Geometry_GetSubTransform_t)(void* self, void* retval, int idx);
extern fn_Geometry_GetSubTransform_t fn_Geometry_GetSubTransform;
// ?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z

typedef void* (*fn_Geometry_GetInstanceTransform_t)(void* self, void* retval,
                                                    int idx, float t);
extern fn_Geometry_GetInstanceTransform_t fn_Geometry_GetInstanceTransform;
// ?GetInstanceTransform@Geometry@Graphics@@UEBA?AVMatrix@Math@@HM@Z

typedef int   (*fn_Geometry_SetSharedInstanceData_t)(void* self, void* context,
                                                     int idx, float t);
extern fn_Geometry_SetSharedInstanceData_t fn_Geometry_SetSharedInstanceData;
// ?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z

typedef bool  (*fn_Geometry_Clean_t)(void* self);
extern fn_Geometry_Clean_t fn_Geometry_Clean;
// ?Clean@Geometry@Graphics@@UEAA_NXZ

// ============================================================
// Initialization
// ============================================================

// Resolve all function pointers. Call after ResolveInit().
// Returns true if all critical pointers were resolved.
bool EngineFuncsInit();
