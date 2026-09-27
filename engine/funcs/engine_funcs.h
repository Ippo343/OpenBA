#pragma once
// Resolved engine function pointers.
// These are runtime-resolved pointers to functions in Runtime-x64.dll
// (and occasionally Application-x64.exe) that reimplemented code calls.
//
// Resolved by EngineFuncsInit() after ResolveInit() succeeds.
// Add new entries as each reimplemented function needs to call more engine code.

#include <windows.h>
#include "options.h"
#include <cstdint>
#include <new>
#include "math/Math.h"
#include "system/String.h"
#include "system/Filename.h"
#include "system/Value.h"      // ::System::Random — the inline bodies below were
                               // the last callers of the fn_Random_* door pointers.
#include "ui/UserInterface.h"   // UI::UserInterface::_dialogs (LinkedList walk below)
// ENDGAME B.3.6 closing pass: the inline bodies below were the last callers of
// the `fn_UI_SetCursor` / `fn_UI_SetFullscreenFade` / `fn_DialogInstance_GetController`
// door pointers; they call the real classes now. Spelled by path: `ui/DialogInstance.h`
// is ALSO the name of the engine-side alias header.
#include "../../runtime/types/ui/DialogInstance.h"   // UI::DialogInstance::GetController
// ENDGAME B.3.3 step 7: the inline bodies below were the last callers of the
// `fn_ExternalFactory_*` / `fn_Factory_*` / `fn_InternalBasePtr_Ctor` pointers.
// They call the real classes now. GLOBAL SCOPE, and never inside an #if —
// B33_RULES' namespace hazard.
#include "object/External.h"    // ::Object::ExternalFactory
#include "object/Internal.h"    // ::Object::InternalBasePtr
#include "object/TypeInfo.h"    // ::Object::Factory
// ENDGAME B.3.5 closing pass: the camera-snap inline body below was one of the
// four callers of `fn_Camera_SetCameraToWorld`. It calls the real class now.
// GLOBAL SCOPE, never inside an #if. Safe AFTER the `<windows.h>` at the top of
// this header only because Volume.h (which Camera.h pulls for `Frustum`) became
// order-free in the same pass -- before that, `<wingdi.h>`'s `Rectangle()`
// function hid Volume.h's class of that name and this include could not be made.
#include "graphics/Camera.h"   // ::Graphics::Camera

// ============================================================
// Engine function pointers
// ============================================================

// ENDGAME B.3.5 caller batch (b35-K2), THE GLOBAL DOOR SWEEP: the whole
// Video::Context LIFECYCLE GROUP is GONE from this header — `fn_VideoCtx_Ctor`,
// `fn_VideoCtx_Dtor`, `fn_VideoCtx_Delete`, `fn_VideoCtx_ClearResources` and
// `fn_VideoCtx_Reset`, with their typedefs, externs, definitions, initialisers
// and their four EngineFuncsDump lines. The note further down this header (which
// also goes) had already recorded that all five were resolved-and-never-called
// once batch b35-I3 moved main.cpp's FSE-refocus recovery onto
// `Video::Context::Reset()`. A tree-wide caller count by binding confirms zero:
// the only remaining mentions are COMMENTS in runtime/src/video/context.cpp and
// context_lifecycle.cpp, and B.3.2's rules say a comment is not a caller. The
// registered doors in runtime/src/video/ are untouched.

// ============================================================
// Video::Context lifecycle — RETIRED (ENDGAME B.3.5, batch b35-K2)
// ============================================================
//
// All five rows are gone; see the note at the top of this header. The facts the
// old banner recorded, kept because they are still true of the TREE: the Context
// is created and destroyed entirely inside Runtime's Engine::MainLoop
// (CreateVideoContext 0x1800dd500 / ReleaseVideoContext / ~MainLoop), so nothing
// on the engine side ever drove these pointers except `EngineFuncsDump()`. The
// resolve MECHANISM differed per row and that is why a name-anchored sweep
// undercounts this group: the ctor and `operator delete` came from
// `ResolveAddr(Module::Runtime, 0x1800f1820 / 0x180018500)`, not from a mangled
// name, and both of those addresses resolve to `??0Context@Video@@` and
// `??3Context@Video@@` in `standalone/rt_export_addrs.inc`.

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

// Video::Device::_singleton -- read as the class static (ENDGAME B.3.9 SS2).

// Video::Device::Lock / ::Unlock (ordinals 3214 / 4462) are GONE in ENDGAME
// B.3.5 (b35-close): both are members of the real `Video::Device`. Lock outlived
// Unlock by one commit because minimap_upload.cpp's readiness printf was still
// reading its address; that `%p` went with it.

// ============================================================
// System::String function pointers
// ============================================================

// ============================================================
// Object::ExternalFactory
// ============================================================

// ExternalFactory::_singleton -- read as the class static (ENDGAME B.3.9 SS2).

// ENDGAME B.3.3 step 7: the `fn_ExternalFactory_*` and `fn_External_GetPath` /
// `fn_External_SaveText` pointers are DELETED — every caller is a member call on
// `::Object::ExternalFactory` / `::Object::External` (runtime/types/object/External.h).
// The notes below stay because they describe those members' behaviour, which the
// callers still have to reason about.

// ExternalFactory::FindOrLoad(this, Filename const&) -> External const*
// Retail 0x1800bf320, reversed 2026-09-06 (runtime port Wave 3 lane C).
// Ported: runtime/src/object/external_compiled.cpp.
//
// CACHING SEMANTICS, because several exe-side bugs have traced back to a wrong
// belief about them:
//   * A HIT does NOT reload and does NOT re-parse. Find() returns the cached
//     External and FindOrLoad returns it unchanged. The only extra work is a
//     second call to ExternalFactory::Add, and only when EnableLoad
//     (factory+0x41) is set.
//   * Add (0x1800be570) ORs `1 << factory->_cleanBitIndex` into the entry's
//     usage mask (entry+0x10) and, ONLY IF that bit was not already set, calls
//     the External's vtable slot +0x28 (Serialize) with an IO::VisitorStream —
//     a reference-VISIT pass, not a re-read of the file. That is the "Serialize
//     re-runs" effect; it is once per resource per clean generation, not once
//     per FindOrLoad. See code/Game/Component/builddescription.cpp.
//   * A MISS expands the path, builds `dataDir + dir + name + ext`, calls
//     External::Load (0x1800bc620), then overwrites the new object's _path with
//     the expanded RELATIVE path and Add()s it. So the tree is keyed on the
//     relative path even though the file was opened by an absolute one.
//   * A path with no `:identifier` suffix is looked up as `path + ":resource"`.
//   * Lookup is case-INSENSITIVE (the tree compares with _stricmp).

// ExternalFactory::Find(this, Filename const&) -> External const*
// Ordinal 2337: ?Find@ExternalFactory@Object@@QEBAPEBVExternal@2@AEBVFilename@System@@@Z

// ExternalFactory::Clean(this) — evict resources unused in this generation.
// Retail 0x1800be310, reversed 2026-09-06 (runtime port Wave 3 lane C). The old
// one-liner here said "cleanup resources for freed bits", which named the
// mechanism without saying what it does. Precisely:
//   in-order walk the resource tree; for each entry, if factory->_cleanBitIndex
//   (+0x00, SIGNED) is >= 0 clear bit `1 << (idx & 31)` from the entry's usage
//   mask (+0x10) and collect the entry when the mask reaches zero; if the index
//   is NEGATIVE, collect EVERY entry unconditionally. Then, for each collected
//   key, call the External's vtable slot 0 with deleteFlag=1 (the scalar
//   deleting destructor — the object is FREED, every cached External* to it is
//   now dangling) and erase the tree node. Finally bump the generation counter
//   at factory+0x08, which is what invalidates the exe's ExternalRef caches
//   (see ResolveExternalByName_Impl below).
// Ported: runtime/src/object/external_set.cpp.

// Object::Base::IsDerivedFrom(this, uint typeId) -> bool
typedef bool (*fn_IsDerivedFrom_t)(void* self, uint32_t typeId);
extern fn_IsDerivedFrom_t fn_IsDerivedFrom;


// ============================================================
// Mod entry lifecycle (for Modification::Swap)
// ============================================================

// ============================================================
// IO::Persistence — key/value settings store
// ============================================================

// Persistence::_singleton -- read as the class static (ENDGAME B.3.9 SS2).

// ============================================================
// System::Filename helpers
// ============================================================

// ============================================================
// Dynamic array operations (for Mod array management)
// ============================================================

// ============================================================
// Engine globals (resolved pointers into Runtime-x64.dll data)
// ============================================================

// Application::Application::_singleton -- read as the class static (ENDGAME B.3.9 SS2).

// ============================================================
// UI infrastructure (DialogController, Element, TextElement)
// ============================================================


// System::MemoryAllocator::MemFree(ptr) — static, gets singleton internally
// Used by scalar deleting destructors for conditional free
typedef void (*fn_MemFreeStatic_t)(void* ptr);
extern fn_MemFreeStatic_t fn_MemFreeStatic;

// Object::InternalBasePtr::InternalBasePtr(this, Internal*) — stores ptr
typedef void* (*fn_InternalBasePtr_Ctor_t)(void* self, void* internal);
extern fn_InternalBasePtr_Ctor_t fn_InternalBasePtr_Ctor;

// ============================================================
// UI Widget Functions (Phase A1 — Runtime DLL exports)
// ENDGAME B.3.6 closing pass: every UI::* door pointer that stood here reached
// zero references and was deleted; callers use the real UI classes
// (runtime/types/ui/*.h). The ctor/dtor doors the exe subclasses still
// constructed through (fn_DialogInstance_Ctor/_Dtor, fn_Element_Ctor,
// fn_ElementDescription_Ctor/_Dtor, fn_SpriteSheet_Ctor) are gone too: the
// exe classes use the real constructors (ENDGAME B.3.8 / B.3.9).
// ============================================================

// ============================================================
// Exe Type IDs (Phase A2 — .rdata pointers in Application-x64.exe)
// ============================================================
// ENDGAME B.3.9 TR: the type-id cell pointers that stood here are
// gone; readers use X::TypeId() of the class at the cell (INVENTORY_C Table 1).

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


// ============================================================
// Game Singletons (Phase A3 — exe global pointers)
// ============================================================

// Game/World singleton — accessed by ~20 UI methods
// Sub-fields: +0xB0 ProfessionList, +0xF0 CitizenList, +0xF8 WorldGrid,
//             +0x100 ResourceTypeMap, +0x110 RadiusDisplayManager
extern void** g_pGameWorldSingleton;  // DAT_1401c3dd8

// Input::KeyMap singleton — used by HotKeyToolTipUI
extern void** g_pKeyMapSingleton;     // DAT_1401c3de8 (container/manager object, NOT the KeyMap)
// Exe import slot 0x140151738 = ?_singleton@Configuration@Input@@0PEAV12@EA
// (B37_RULES 6.3 defect 3: this global was named g_pInputConfigSingletonExref until the
// B.3.7 close). Triple deref yields the Input::KeyMap*:
//   slot -> &Configuration::_singleton -> Configuration* -> +0x00 _keyMap.
// Standalone registers the slot with a two-link chain ending in the KeyMap
// (standalone/address_registry.cpp, g_StandaloneKeyMapMid).
extern void** g_pInputConfigSingletonExref;


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


// Typedef kept for any existing code that uses fn_FindComponent_t as a type.
typedef void (*fn_FindComponent_t)(void* entity, void* retval);

// FindComponent inline wrappers — call Entity_FindComponentByExeTypeId with correct type ID.
// Original exe functions (FUN_140XXXXXX) are no longer needed.

// fn_FindComponent_Build — DAT_1401c79e0, original FUN_140004680. MISNOMER: the type id at
// DAT_1401c79e0 is MapComponent, not BuildComponent (registrar MapComponent_RegisterType
// 0x14014f375). Bodies: code/Game/Entity/entity_findcomponent.cpp.
void fn_FindComponent_Build(void* entity, void* retval);
// fn_FindComponent_BuildState — DAT_1401c7d98, original FUN_140003ea0
void fn_FindComponent_BuildState(void* entity, void* retval);
// fn_FindComponent_ConsumeProduce — DAT_1401c7350, original FUN_14000b6f0
void fn_FindComponent_ConsumeProduce(void* entity, void* retval);
// fn_FindComponent_Position — DAT_1401c7008, original FUN_14000d6c0
void fn_FindComponent_Position(void* entity, void* retval);
// fn_FindComponent_Destroy — DAT_1401c75f0, original FUN_140005780
void fn_FindComponent_Destroy(void* entity, void* retval);
// fn_FindComponent_Building — DAT_1401c4b80, original FUN_140015140.
// ⚠ MISNAMED, kept only because the name is load-bearing across the tree: 0x140015140
// matches DWORD_1401c4b80 = TradeComponent (verified at instruction level 2026-08-21),
// so this is a TRADING-POST finder, not a "building" finder. Anything reading its result
// as "is there a building here?" is wrong — see the animal habitat test in
// animalcomponent.cpp, whose comment said exactly that until it was corrected.
// Consequence for gating: this finder returns null for EVERY entity while
// BANISHED_LIVE_TRADE_TIDS is off, and non-null only for a trading post when it is on.
void fn_FindComponent_Building(void* entity, void* retval);
// fn_FindComponent_Work — DAT_1401c8070 = "WorkComponent" (factory_registrations.csv:306),
// original FUN_140003050. Was named fn_FindComponent_Workplace, which is a DIFFERENT type:
// WorkPlaceComponent is DAT_1401c80e0 / fn_FindComponent_WorkPlace below. Retail uses both,
// side by side, in WorkPlaceUI::OnCreate (0x140087080) and in the builder-profession pair
// (0x14012f520 / 0x14012f670). Confusing them costs crashes — a WorkComponent read as a
// WorkPlaceComponent puts the small work id (+0x28) where a roster heap pointer (+0x58) is
// expected. Fields used off a WorkComponent: +0x18 WorkDescription, +0x28 scheduler work id
// (-1 = none), +0x30 cached max workers (WorkComponent_CalcMaxWorkers).
void fn_FindComponent_Work(void* entity, void* retval);
// fn_FindComponent_Attendance — DAT_1401c3f40, original FUN_1400105e0
void fn_FindComponent_Attendance(void* entity, void* retval);
// fn_FindComponent_Pasture — DAT_1401c7ba0, original FUN_140004fe0
void fn_FindComponent_Pasture(void* entity, void* retval);
// fn_FindComponent_Orchard — DAT_1401c46b0, original FUN_140005440
void fn_FindComponent_Orchard(void* entity, void* retval);
// fn_FindComponent_WorkPlace — DAT_1401c80e0 = "WorkPlaceComponent"
// (factory_registrations.csv:308), original FUN_140002e10. Was named
// fn_FindComponent_BuildState2. Fields: +0x48 active profession key, +0x58 roster base,
// +0x60 roster count, +0x18 WorkPlaceDescription (whose +0x20 is the natural profession).
void fn_FindComponent_WorkPlace(void* entity, void* retval);
// fn_FindComponent_SM — DAT_1401c71c8, original FUN_14000a170
void fn_FindComponent_SM(void* entity, void* retval);
// fn_FindComponent_Citizen — DAT_1401c7158, original FUN_140009790
void fn_FindComponent_Citizen(void* entity, void* retval);
// fn_FindComponent_Storage — DAT_1401c7e78, original FUN_140006270
// NOTE: same function as fn_FindComponent_Residence (same type ID search)
void fn_FindComponent_Storage(void* entity, void* retval);
// fn_FindComponent_Residence — same as fn_FindComponent_Storage (same type ID)
void fn_FindComponent_Residence(void* entity, void* retval);
// fn_FindComponent_Cemetery — DAT_1401c4090, original FUN_140010a00
void fn_FindComponent_Cemetery(void* entity, void* retval);
// fn_FindComponent_Immigration — DAT_1401c4480, original FUN_140012020
void fn_FindComponent_Immigration(void* entity, void* retval);
// fn_FindComponent_Tracker — DAT_1401c4b10, original FUN_140013320
void fn_FindComponent_Tracker(void* entity, void* retval);
// fn_FindComponent_MeshInstance — DAT_1401c7e08,
// StatusIconComponent's BaseTypeInfo (writer StatusIconComponent_RegisterType
// 0x14014fab5; real HappinessComponent id is 0x1401c42c0). Original FUN_140002f90.
void fn_FindComponent_MeshInstance(void* entity, void* retval);
// fn_FindComponent_Character — DAT_1401c7a50 = ModelComponent (writer ModelComponent_RegisterType);
// finds ModelComponent. Function name kept for callers; the "Character" label is historical.
// Original FUN_140003f60.
void fn_FindComponent_Character(void* entity, void* retval);
// fn_FindComponent_CitizenAI — FUN_140011050, Entity::FindComponent<CitizenAIComponent>
// (compares against [0x1401c70e8] at 0x140011093, IsDerivedFrom at 0x1400110ec).
// ENDGAME B.3.9 CB: defined in code/Game/Dialog/citizenui.cpp over
// CitizenAIComponent::TypeId(); the cell is unregistered, so the old inline over
// the cell pointer only worked when a dialog re-pointed that global.
void fn_FindComponent_CitizenAI(void* entity, void* retval);

// ============================================================
// Dynamic array helpers — Reimplemented in clean C++
// (previously resolved from Application-x64.exe)
// All use fn_MemAlloc / fn_MemFreeStatic from Runtime-x64.dll.
// Array header layout: [0]=ptr(void*), [1..low32]=count(int), [0xc..low32]=capacity(int)
// ============================================================

// The four exe array helpers that used to live here (fn_ExeArrayResize/2/3/Push,
// the MSVC monomorphisations of Collection::Array<T>::SetSize at 0x1400032e0 /
// 0x1400021d0 / 0x140001bb0 / 0x140003600) are gone with the B.3.2 closing pass:
// every caller is now Collection::Array<...>::SetSize. Read rtcoll::SetSizeCore
// for the algorithm.

// StorageComponent::QueryCount(storageComp) -> int
// FUN_14012cf10: iterates +0x28 item array (count at +0x30). Each slot is 0x10 bytes:
//   [0..7] = ptr to "item" struct (has +0x28=component ptr array, +0x30=comp count)
//   [8..11] = item quantity count
// For each item: scan its component array for ResourceType (type ID check at vtable+0x10,
// then IsDerivedFrom). Accumulate ResourceType.quantity(+0x48) * item.count.
int fn_StorageComponent_QueryCount(void* storageComp);

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

// --- UI::Element type IDs (exref, double-indirect) ---


// ============================================================
// Additional Exe Type IDs (Phase D additions)
// ============================================================


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

// Time::AsSeconds — REMOVED, ENDGAME B.3.4b System closing (0 CALLs).
// Callers use System::Time::AsSeconds(); the Runtime export
// ?AsSeconds@Time@System@@QEBAMXZ (0x180007900, ordinal 1620) stays registered.

// SaveLoad::~SaveLoad(this) — destructor (clears _singleton, releases refs)
// ??1SaveLoad@Save@@QEAA@XZ (0x1800c3c90, ordinal 666)
typedef void (*fn_SaveLoad_Dtor_t)(void* saveLoad);
extern fn_SaveLoad_Dtor_t fn_SaveLoad_Dtor;

// DateTime::AsString(this, retval) -> String — format date+time as string (non-trivial return)
// ?AsString@DateTime@System@@QEBA?AVString@2@XZ
typedef void* (*fn_DateTime_AsString_t)(void* dateTime, void* retval);
extern fn_DateTime_AsString_t fn_DateTime_AsString;

// SaveLoadDialog helper: FUN_1400b6ce0 — populates the file list table
// NOW REIMPLEMENTED in code/Game/Dialog/saveloaddialog.cpp as SaveLoadDialog_PopulateFileList()
// Old typedef kept for reference:
// typedef void (*fn_SaveLoadDialog_PopulateFileList_t)(void* self);
// extern fn_SaveLoadDialog_PopulateFileList_t fn_SaveLoadDialog_PopulateFileList;
class SaveLoadDialog;
void SaveLoadDialog_PopulateFileList(SaveLoadDialog* self);

// ScenarioDialog helper: loads the selected scenario — NOW REIMPLEMENTED
// fn_ScenarioDialog_LoadSelected replaced by inline code in scenariodialog.cpp

// IO::Persistence::GetSavePath and IO::FileSystem::GetDataDirectory — REMOVED,
// ENDGAME B.3.4a closing 2d. Both doors ran out of callers once the caller
// batches converted; pointer, typedef, definition, resolver and exe-slot
// fallback all went together. Their exe dispatch slots (0x140151d38 /
// 0x140151d30) were never registered and the rows stay as they were.

// LoadScenarioFromSave: FUN_1400b49f0(worldSetup, savedState, scenarioIdx, retvalPtr)
// Called from ScenarioDialog_LoadSelected when loading a saved scenario.
// Constructs GameUpdateLoop with saved state, then GameLoadDialogLoopT, then LoaderLoop.
typedef void (*fn_LoadScenarioFromSave_t)(void* worldSetup, void* savedState,
                                           void* scenarioIdx, void* retvalPtr);
extern fn_LoadScenarioFromSave_t fn_LoadScenarioFromSave;

// ModDetailsDialog helpers
// REPLACED: now inline ModDetailsDialog_OpenUpdateDialog_Impl() in moddetailsdialog.cpp
// REPLACED: now inline ModDetailsDialog_DeleteMod_Impl() in moddetailsdialog.cpp

// SelectResourceUI::GetResourceCount(self) -> int
// FUN_140078a40 — counts expected resources for combo (shared by Orchard/Pasture)
typedef int (*fn_SelectResourceUI_GetResourceCount_t)(void* self);

// ============================================================
// Video::ImageBuffer (Runtime DLL exports, used by MapUI/NoiseUI/SpriteCaptureUI)
// ============================================================

// (fn_ImageBuffer_Ctor went at ENDGAME B.3.9 EXA: `::new (p) Video::ImageBuffer()`.)

// ENDGAME B.3.5 (b35-K2): `fn_ImageBuffer_Dtor`
// (??1ImageBuffer@Video@@UEAA@XZ, 0x1800f2610) is GONE — zero callers tree-wide.
// The registered door in runtime/src/video/ is untouched.

// ImageBuffer::CopyToTexture (0x1800f3380, ordinal 1869) and ::CopyFromTexture
// (0x1800f33a0, ordinal 1866) are GONE in ENDGAME B.3.5 (b35-close): both are
// members of the real `Video::ImageBuffer`. Their last readers were two diagnostic
// `%p`s (minimap_upload.cpp and w2_takescreenshot.cpp), not calls.

// ============================================================
// FollowDialog helpers
// ============================================================

// fn_FollowDialog_Close — now inline in followdialog.cpp Slot7 (Escape key handler)

// ============================================================
// Shared SA dialog guard
// ============================================================
// Is a DialogController's data-source entity (+0x28) still safe to read? The
// weak ref is nulled when the entity is destroyed, and while the
// marked-for-removal flag (entity+0x40 bit 0x400) is set the entity survives
// until the EntityList drain with its components already torn down. Reading a
// roster/storage in either state crashed live 2026-07-20 (upgrade path
// demolishing a house with its panel open). Wrappers bail — the panel freezes
// until the dialog closes.
inline bool SA_DialogEntityAlive(void* self) {
    void* e = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x28);
    if (!e) return false;
    if ((*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(e) + 0x40) >> 10) & 1)
        return false;
    return true;
}

// ============================================================
// UpgradeUI helpers
// ============================================================


// 0x14011dfa0 ("StartUpgrade" — misnomer; the generic BuildState state-machine
// driver): ported disasm-faithfully as RemovalCalc_StateMachine_dfa0 in
// code/Game/Player/keystone_a_removalcalcpath.cpp and registered for that
// address. Callers (UpgradeUI_OnRemove) reach it via
// ResolveAddr(Module::Exe, 0x14011dfa0). An earlier divergent port in
// destroyui.cpp was removed 2026-07-20.

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

// CitizenList::RemoveWorker(citizenList, professionId) — removes worker assignment
// FUN_140091180: called for workerCountDown (ID 0x1451)
typedef void (*fn_CitizenList_RemoveWorker_t)(void* citizenList, uint64_t professionId);
extern fn_CitizenList_RemoveWorker_t fn_CitizenList_RemoveWorker;


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

// HelpUI_Slot4 — now inline as HelpUI_Slot4 in helpui.cpp
// HelpUI_Slot9 — now inline as HelpUI_Slot9 in helpui.cpp

// ============================================================
// Phase F Wave 2A: GoalDialog / EventNotifyDialog / ScenarioDialog
// ============================================================

// GoalDialog_ArrayPush — now inline as GoalDialog_ArrayPush_Impl in goaldialog.cpp
// EventNotifyDialog_ArrayPush — now inline as EventNotifyDialog_ArrayPush_Impl in eventnotifydialog.cpp
// ScenarioDialog_ArrayPush — now inline as ScenarioDialog_ArrayPush_Impl in scenariodialog.cpp

// ExternalList::GetResourceCount(this) -> int
// ?GetResourceCount@ExternalList@Object@@QEBAHXZ (0x180007650)
typedef int (*fn_ExternalList_GetResourceCount_t)(void* self);
extern fn_ExternalList_GetResourceCount_t fn_ExternalList_GetResourceCount;


// fn_EventNotifyDialog_ArrayRemove — now inline as EventNotifyDialog_ArrayRemove_Impl in eventnotifydialog.cpp

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

// ToolUI child dialog vtable (PTR_FUN_14015f5a0 in exe .rdata) — used by Toolbar_Utility
extern uintptr_t g_ToolUI_ChildDialog_Vtable;

// Toolbar item sort function (FUN_140021790) — sorts toolbar items by priority
typedef void (*fn_ToolbarSort_t)(void* array, int count);


// FUN_140100270: Find open dialog by controller type matching group ID
// Searches all open dialogs for one whose controller matches a specific type
// and whose +0x30 field equals the given param. Returns InternalBasePtr.
typedef void (*fn_FindDialogByControllerType_t)(void* toolController, void* retBuf, uint64_t param);
extern fn_FindDialogByControllerType_t fn_FindDialogByControllerType;

// fn_Toolbar_FindEntity — now inline as Toolbar_FindEntity_Impl in toolbardialog.cpp
// fn_Toolbar_CenterOnEntity (FUN_140103040) — now inline as Toolbar_CenterOnEntity_Impl below

// --- Sub-dependencies for Toolbar_CenterOnEntity_Impl ---


// FindComponent_MultiObject — inline wrapper for MultiObjectComponent lookup
void fn_FindComponent_MultiObject(void* entity, void* retval);

// EntityPanel_SyncTracking (FUN_1400666b0) — syncs tracking/follow flag to dialog controller
typedef void (*fn_EntityPanel_SyncTracking_t)(void* panelController);
extern fn_EntityPanel_SyncTracking_t fn_EntityPanel_SyncTracking;

// CameraSys_ComputeViewTransform (FUN_1400983f0) — computes camera look-at transform
// Reads target position from cameraSys+0x1C, yaw at +0x28, distance at +0x30.
// Writes compact Transform (7 dwords = 0x1C bytes) to output buffer and cameraSys[0..6].
typedef void* (*fn_CameraSys_ComputeViewTransform_t)(void* cameraSys, void* outTransform);
extern fn_CameraSys_ComputeViewTransform_t fn_CameraSys_ComputeViewTransform;

// Graphics::Camera::SetCameraToWorld (?SetCameraToWorld@Camera@Graphics@@QEAAX
// AEBVTransform3@Math@@@Z, Runtime 0x1800feb00) is GONE in ENDGAME B.3.5
// (b35-close): it is a member of the real `Graphics::Camera`. This row was the LAST
// Video/Graphics door in this file bound BY ADDRESS rather than by mangled name,
// which is why three name-anchored caller sweeps walked past its four callers.

// Toolbar_CenterOnEntity_Impl — defined at end of file (needs all declarations above it)

// fn_ToolbarDialog_CloseSubDialog — now inline as ToolbarDialog_CloseSubDialog_Impl in toolbardialog.cpp
// fn_ToolbarDialog_ChangeSpeed — now inline as ToolbarDialog_ChangeSpeed_Impl in toolbardialog.cpp

// ToolbarDialog update button states helper (FUN_1400b8fd0)
// REPLACED: now ToolbarDialog_UpdateButtons_Impl() in code/Game/Dialog/toolbardialog.cpp


// fn_GetCurrentProductIndex — inline defined later (after fn_Random_RandFloat)
// FUN_140119470: if +0x50 != 0xffffffff return it; else weighted random pick from +0x58 array.

// ResolveExternalByName (FUN_14001b1d0 in exe) — REPLACED with inline C++
// Resolves and caches an External object by filename.
// Input: nameString points to an ExternalRef struct:
//   +0x00: String (8 bytes) — resource name
//   +0x08: int32  — cached generation counter (compared against ExternalFactory+0x08)
//   +0x10: void*  — cached External* result
// If the generation counter matches the factory's, the cached result is still valid.
// Otherwise, looks up the External by converting the String to a Filename and calling
// ExternalFactory::Find. If found and passes IsDerivedFrom(ComponentDescription::TypeId()),
// caches the result; otherwise caches nullptr. Body: code/Game/Component/componentdescription.cpp.
typedef void (*fn_ResolveExternalByName_t)(void* nameString); // kept for ABI reference
void ResolveExternalByName_Impl(void* nameString);

// (fn_External_External / fn_External_Dtor — the ??0/??1External doors read
// through exe slots 0x140151e98 / 0x140151ea0 — went at ENDGAME B.3.9 EXA:
// zero readers.)

// ============================================================
// Phase F Wave 2C: EntityUI::Slot8, ResidenceUI::Slot7, EventNotifyDialog::Slot7
// ============================================================

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

// fn_Dialog_GetReference — REMOVED 2026-09-07 (Wave 7 lane B,
// docs/runtime_port/WAVE7_LANE_B.md §5). It was declared as
//   UI::Dialog::GetReference(this, retval) -> ExternalPtr<UI::DialogReference>
//   ?GetReference@Dialog@UI@@QEBA?AV?$ExternalPtr@VDialogReference@UI@@@Object@@XZ
// and BOTH halves of that were wrong. The DLL exports no symbol containing
// "DialogReference" at all — the class name was invented — and the real
// function takes an INT INDEX the typedef omitted:
//   ?GetReference@Dialog@UI@@QEBA?AV?$ExternalPtr@VExternal@Object@@@Object@@H@Z
//   Runtime 0x1800388d0, ordinal 2861: `*out = ((void**)this[0x78])[index]`,
//   the twin of ?GetTemplate@ (0x1800388b0) against Dialog+0x78 instead of +0x68.
// So this pointer has been NULL since it was written (ResolveExport has no
// address fallback), with no fallback branch to hide it. Deleted rather than
// repaired because it had ZERO callers tree-wide; whoever needs it should
// declare the real signature, index and all.

// EventNotifyDialog_ArrayPush — now inline as EventNotifyDialog_ArrayPush_Impl in eventnotifydialog.cpp

// EventPopupDialog vtable pointer (PTR_FUN_14015bd80)

// ============================================================
// HelpUI support functions
// ============================================================


// HelpUI_ArrayResize — now inline as HelpUI_ArrayResize_Impl in helpui.cpp

// ============================================================
// BuildUI support functions
// ============================================================

// ============================================================
// CitizenUI helpers
// ============================================================


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

// BSTRebalance — now inline in professionui.cpp (BSTRebalance_Impl)

// fn_ProfessionUI_HandleSpinner — reimplemented inline in professionui.cpp (ProfessionUI_HandleSpinner)
// fn_ProfessionUI_HandleButton — reimplemented inline in professionui.cpp (ProfessionUI_HandleButton)

// ============================================================
// MainMenuDialog support functions
// ============================================================

// System::Configuration::CurrentVersion() -> String (static, hidden ret ptr in RCX)
// ?CurrentVersion@Configuration@System@@SA?AVString@2@XZ
typedef void (*fn_Configuration_CurrentVersion_t)(void* retStr);
extern fn_Configuration_CurrentVersion_t fn_Configuration_CurrentVersion;

// System::Configuration::CurrentBuildId() -> String (static, hidden ret ptr in RCX)
// ?CurrentBuildId@Configuration@System@@SA?AVString@2@XZ
typedef void (*fn_Configuration_CurrentBuildId_t)(void* retStr);
extern fn_Configuration_CurrentBuildId_t fn_Configuration_CurrentBuildId;

// Object::InternalBasePtr::CheckPtr — GONE, ENDGAME B.3.3 step 6D. Nothing calls it
// any more and nothing may: `?CheckPtr@InternalBasePtr@Object@@IEBAXXZ` is a bare
// `RET 0` (`C2 00 00`) in the shipped Runtime-x64.dll at 0x1800c0290 AND in the
// ToolKit profile build at 0x18008c5d0 — read from the disassembly in both, not from
// decompiled C. It was compiled out at SOURCE level in every shipped configuration,
// so no build of this engine has ever validated an InternalBasePtr, and the 4,570
// exe-side call sites were doing nothing at a cost of ~60k indirect calls a frame.
// The pointer, its typedef, `g_checkPtrIsNop`, `InternalBasePtr_CheckPtr_Inline` and
// the `BANISHED_CHECKPTR_STATIC_NOP` perf-build variant all went with them.
// `Object::InternalBasePtr::CheckPtr()` still EXISTS as an empty member of the real
// class (runtime/types/object/Internal.h) and is still registered as a port, because
// the SYMBOL has to exist at cutover — see runtime/src/object/internal.cpp.
// NEVER ADD VALIDATION TO IT, and never reintroduce a caller.

// UI::DialogInstance::DialogInstance(this, ui, dialog, parentInternalPtr, dataSource, extraPtr, flags)
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

// System::Random::Rand(uint) and ::RandFloat() — REMOVED, ENDGAME B.3.4b System
// closing (0 CALLs). Callers use the members on System::Random (system/Value.h);
// the Runtime exports ?Rand@Random@System@@QEAAII@Z (ordinal 3442, 0x1800c8040)
// and ?RandFloat@Random@System@@QEAAMXZ (ordinal 3444, 0x1800c7d40) stay bound.

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
        // B.3.4b bridge: ?RandFloat@Random@System@@QEAAMXZ (0x1800c7d40).
        float randVal = reinterpret_cast<System::Random*>(randomPtr)->RandFloat();
        float accum = 0.0f;
        for (int i = 0; i < count; i++) {
            accum += weights[i];
            if (randVal * totalWeight < accum) return (uint32_t)i;
        }
        return 0;
    }
    return idx;
}

// FUN_14012e5c0: Calculate the STARTING job count of a WorkComponent (DAT_1401c8070) —
// i.e. the initial value of WorkComponent+0x30, the number of worker SLOTS the site opens
// with. It is NOT the maximum the player can reach: the jobCount spinner raises +0x30 up
// to WorkDescription+0x2c (_maxWorkerCount) via SetMaxWorkers (0x14012fb00).
// param_1+0x18 = the WorkDescription (checks IsDerivedFrom at DAT_1401c80a8).
// WorkDescription layout: +0x20 _allowCountChange, +0x24 _defaultWorkers,
//   +0x28 _minWorkerCount, +0x2c _maxWorkerCount, +0x30 _maxArea, +0x34 _scaleWorkersWithSize.
// If _scaleWorkersWithSize (+0x34) == 0: return _defaultWorkers (+0x24) flat.
// Else: trunc(w*h / _maxArea * _defaultWorkers + 1) clamped to [1, _defaultWorkers].
// Used as fn_StorageCountQuery in workplaceui, fn_CalcWorkerSlots in professionui.
int fn_StorageCountQuery(void* component);
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


// Start game handler: FUN_1400b4540
// Constructs GameUpdateLoop -> GameLoadDialogLoopT -> LoaderLoop chain and sets next loop.
// Params: worldSetup, displayName, seed, terrainSize, disasterOff, mapGen, terrainGen, climateGen, scenarioIdx, nullIBP
// NOTE the generator order: args 6/7/8 land in GameUpdateLoop +0x40/+0x48/+0x50,
// which retail Load reads as mapGen / terrainGen / climateGen respectively.
typedef void (*fn_StartNewGame_t)(void* worldSetup, void* nameStr, uint32_t seed,
                                   uint32_t terrainSize, char disasterOff, void* mapGen,
                                   void* terrain, void* climate, uint32_t scenarioIdx, void* retvalIBP);
extern fn_StartNewGame_t fn_StartNewGame;


// ============================================================
// OptionsDialog support — Runtime DLL exports for video/audio config
// ============================================================

// OptionsDialog::OnOk/OnCancel/OnApply — REMOVED: reimplemented in code/Game/Dialog/optionsdialog.cpp
typedef void (*fn_OptionsDialog_SimpleFunc_t)(void* self);  // kept for PopulateInputBindList
// OptionsDialog::PopulateInputBindList — REIMPLEMENTED in code/Game/Dialog/optionsdialog.cpp
// extern fn_OptionsDialog_SimpleFunc_t fn_OptionsDialog_PopulateInputBindList;
// OptionsDialog::OnInputBindClick — REIMPLEMENTED in code/Game/Dialog/optionsdialog.cpp
// typedef void (*fn_OptionsDialog_OnInputBindClick_t)(void* self, void* element, int bindIndex, int slot);
// extern fn_OptionsDialog_OnInputBindClick_t fn_OptionsDialog_OnInputBindClick;

// Video::Device, Audio::Configuration (int[4] volume levels at 0, 4, 8, 0xC) and
// Video::Configuration singletons: read as the class statics (ENDGAME B.3.9 SS2).

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

// FUN_14012e100: UIDescription_AddControllers — REIMPLEMENTED in code/Game/Dialog/toolui.cpp
// Iterates UIDescription controller entries, checks for TabElement targets,
// and calls the appropriate AddController overload for each entry.
// Old fn_* pointer removed; replaced by UIDesc_AddControllers_Impl().
void UIDesc_AddControllers_Impl(void* uiDesc, void* dialogInstPtr, void* dataSource);


// ============================================================
// DragTool support functions
// ============================================================

// REMOVED at Wave 8 integration: fn_AudioScene_PlayEffect.
// It was a vestigial resolve under a WRONG NAME. Slot 0x140152830 is NOT any
// Audio::Scene::PlayEffect — tools/dispatch_slot_map.py reads it straight out of
// the exe's PE import table as `??0Goal@Achievement@@QEAA@XZ`
// (Achievement::Goal::Goal). The unregistered-slots wave (standalone/
// reg_unreg_lane3.cpp:265-273, reg_unreg_lane4.cpp:53) had already identified
// the mislabel and deliberately left the slot unregistered.
//
// It had ZERO call sites, so it never misbehaved — but it was armed: the moment
// anyone registered that slot, this pointer would have become Goal's ctor called
// through a guessed 4-argument signature. Same family as the ten vestigial fn_*
// resolves deleted in the code-fn wave (reference_exe_has_a_real_pe_import_table.md).
// The REAL Audio::Scene::PlayEffect exports are ported by Wave 8 lane C
// (runtime/src/audio/audio.cpp) and registered by name AND address.

// ============================================================
// ModDetailsDialog support functions
// ============================================================

// System::Configuration::GetVersionString(retval, version, isBeta) -> String
// Static with hidden return: RCX=retval(String*), RDX=version(uint), R8B=isBeta(bool)
// ?GetVersionString@Configuration@System@@SA?AVString@2@I_N@Z
typedef void* (*fn_Configuration_GetVersionString_t)(void* retval, uint32_t version, bool isBeta);
extern fn_Configuration_GetVersionString_t fn_Configuration_GetVersionString;

// ModDetails_StringArrayPush — now inline in moddetailsdialog.cpp (ModDetails_StringArrayPush_Impl)

// ============================================================
// IO::Modification (mod management singleton)
// ============================================================

// IO::Modification::GetCount / ::GetModification — REMOVED, ENDGAME B.3.4a
// closing 2d. Both were 0-CALL doors onto Runtime 0x18000da90 / 0x18000da80;
// the ported bodies stay registered by address in runtime/src/io/modification.cpp
// and callers reach them as real members. Only the readers died here.

// ============================================================
// UI::DialogInstance::OnElementEvent
// ============================================================

// fn_DialogInstance_AddController — REMOVED 2026-09-07 (Wave 7 lane B,
// docs/runtime_port/WAVE7_LANE_B.md §5). It was declared as
//   AddController(this, typeId, int p3, int p4, void* external, void* nameStr)
//   ?AddController@DialogInstance@UI@@QEAAXIHHPEBVExternal@Object@@AEBVString@System@@@Z
// which is not an export. It was a mis-mangling of the six-argument overload
// at Runtime 0x180113470 / ordinal 1541, whose real signature is
//   (uint, Object::Base*, ExternalPtr<DialogControllerConfig>,
//    ExternalPtr<ElementDescription>, System::String const&)
// — the two `int`s were pointer arguments (the exact
// reference_hidden_retval_and_int_pointer_args.md failure). That overload is
// ALREADY resolved correctly as fn_DialogInstance_AddController6 (itself retired
// with zero callers by the ENDGAME B.3.6 closing pass), so this was a duplicate
// as well as a wrong name.
//
// Worse than dead: its ResolveExport returned null, so its fallback fired and
// read exe dispatch slot 0x140151a60, which tools/dispatch_slot_map.py says is
// ?AddChild@Element@UI@@… — the pointer was silently bound to Element::AddChild.
// standalone/reg_unreg_lane2.cpp's note claiming that branch "never fires
// because the ResolveExport already succeeds" was wrong for the same reason.
// Deleted rather than repaired because it had ZERO callers tree-wide.

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

// ModDialog_CloseAndReload (FUN_14006e9b0) — closes dialog and triggers UI reload
typedef void (*fn_ModDialog_CloseAndReload_t)(void* self);
extern fn_ModDialog_CloseAndReload_t fn_ModDialog_CloseAndReload;

// MainMenu_Reload (FUN_1400b4790) — reloads from main menu context
typedef void (*fn_MainMenu_Reload_t)(void* self);
extern fn_MainMenu_Reload_t fn_MainMenu_Reload;

// Game_ReloadWithSave (FUN_1400b49f0) — saves then reloads from in-game context
typedef void (*fn_Game_ReloadWithSave_t)(void* self, void* savedState, void* param, void* nullRef);
extern fn_Game_ReloadWithSave_t fn_Game_ReloadWithSave;

// Save::SavedState::SavedState() — default ctor (0x10 bytes)
typedef void* (*fn_SavedState_Ctor_t)(void* self);
extern fn_SavedState_Ctor_t fn_SavedState_Ctor;

// ============================================================
// IO::Serializer — save/load serialization (shared globals): ALL GONE
// ============================================================
// ENDGAME B.3.4a closing 2d. The five shared doors g_fn_Serializer_Ctor /
// _Dtor / g_fn_Serialize_Int / _Bool / g_fn_Serializer_GetVersion are removed:
// IO::Serializer is a real C++ class now (runtime/src/io/serializer_class.cpp),
// every call site constructs, destroys and Serialize()s it as a member, and the
// readiness guards that were the pointers' last readers died in 2b/2c/2d.
// The exe dispatch slots they aliased (0x140152ac8, 0x140152a58, ...) STAY
// bound in standalone/address_registry.cpp: those are PE import slots that
// unported, retail-shaped exe code still reads.

// ============================================================
// TownHallUI support functions
// ============================================================


// --- Reimplemented TownHallUI functions (clean C++ in townhallui.cpp) ---
// Forward-declared here; defined in code/Game/Dialog/townhallui.cpp.
class TownHallUI;  // forward decl
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


// ============================================================
// TradeUI support functions
// ============================================================

// TradeUI::RefreshSortedDisplay (FUN_1400846b0) — REPLACED with TradeUI_RefreshDisplay in tradeui.cpp
// fn_TradeUI_RefreshDisplay pointer kept for reference / fallback only.
typedef void (*fn_TradeUI_RefreshDisplay_t)(void* self, uint64_t p2, uint64_t p3, int64_t p4);


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

// TradeUI::OnTradeButton — executes trade (needs opus — ~160 lines)
typedef void (*fn_TradeUI_OnTradeButton_t)(void* self, void* p2, void* p3, int64_t p4);
extern fn_TradeUI_OnTradeButton_t fn_TradeUI_OnTradeButton;   // 0x140081420




// FUN_14005a520: (tradeState, resourceEntityTemplate) -> int
//
// ⚠ THE NAME IS WRONG — kept only because ~4 call sites use it. This is NOT a
// "sellable count". It is the per-resource PRICE/VALUE lookup for the trade panel.
// Verified from the 0x14005a520 decompile (2026-07-30):
//   * When tradeState+0x2c < 0 (no merchant docked) it finds the resource's
//     ValueDescription (DWORD_1401c4bf0) and returns desc+0x40 — the
//     base value.
//   * Otherwise it finds the resource's RawMaterial-ish component (DWORD_1401c7cb8)
//     and tests that component's category flags at +0x5c against the docked
//     merchant's masks, then returns desc+0x40 (buy) or desc+0x40 * DAT_140172124
//     (sell, a discount multiplier), or 0 if the merchant deals in neither.
//
// ⚠ MERCHANT-RECORD DECODE (needed to port this, and easy to get wrong):
// The merchant masks live in the TradeDescription+0x48 merchant array, element
// stride 0x48, indexed by tradeState+0x2c:
//     merchant[n] + 0x14  =  BUY  resource-category bitmask
//     merchant[n] + 0x18  =  SELL resource-category bitmask
// Both are ANDed against the resource component's flags at +0x5c. They are
// **bitmasks, not enums** — a live BANISHED_TRADE_PROBE dump read them as
// {32, 221728, 7232} etc., which look like nonsense enum values but are perfectly
// ordinary category masks. Anything that treats them as enum ordinals will
// silently price every resource wrong rather than fail loudly.
//
// Closure needed before this can be registered: sub-callee 0x14000ea80
// (TradeUI_sub_sub_a520_sub_ea80) plus the two type-id globals above.
// Used by UpdateSellTotals, RefreshSellSpinnerValues, RefreshSellTable, and the
// merchant auto-buy routine TradeDescription_helper_sub_9ad0 (0x140059ad0).
typedef int (*fn_Building_GetSellableCount_t)(void* building, void* resourceEntity);
extern fn_Building_GetSellableCount_t fn_Building_GetSellableCount;  // 0x14005a520

// FUN_14000e1c0: StorageTree_Insert(tree*, retval*, entry*) — insert into storage BST
// Used by OnStorageSpinner to track storage resource assignments.
typedef void* (*fn_StorageTree_Insert_t)(void* tree, void* retval, void* entry);
extern fn_StorageTree_Insert_t fn_StorageTree_Insert;  // 0x14000e1c0

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

// (g_NoiseUI_ImageBufferVtable went at ENDGAME B.3.9 EXA: 0x14015c8c0 is the
// exe's copy of Video::ImageBuffer's own table; the builders construct the class.)

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

// System::Random::RandTol(int) — REMOVED, ENDGAME B.3.4b System closing
// (0 CALLs). Callers use System::Random::RandTol(); the Runtime export
// ?RandTol@Random@System@@QEAAHH@Z stays bound.

// ============================================================
// DebugUI support
// ============================================================

// Weather_ResetTime (0x1400944e0) — resets weather to a given time, updates all active weather
// entries (geometry fade, audio, lightning), recalculates day index and time transition.
// RCX=weatherMgrPtr(longlong*), RDX=timePtr(longlong*)
typedef void (*fn_Weather_ResetTime_t)(void* weatherMgrPtr, void* timePtr);
extern fn_Weather_ResetTime_t fn_Weather_ResetTime;


// ============================================================
// DebugUI Slot9 sub-functions (Application-x64.exe)
// ============================================================

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

// ENDGAME B.3.5 (b35-K2), THE GLOBAL DOOR SWEEP. Four of the five rows that used
// to sit here are GONE — zero callers tree-wide after batches b35-I1/I2/K1
// converted `code/**`:
//   fn_Graphics_Scene_AddGlobalObject            (?AddGlobalObject@Scene@Graphics@@)
//   fn_Graphics_Scene_AddObject                  (?AddObject@Scene@Graphics@@)
//   fn_Graphics_GraphicsMesh_GetMaterialList     (?GetMaterialList@GraphicsMesh@Graphics@@)
//   fn_Graphics_MeshInstance_SetMaterialInstance (?SetMaterialInstance@MeshInstance@Graphics@@)
// The first two were the two HEAVIEST doors in B35_RULES section 7's table (12
// and 7 call sites); both are at zero now. Their registered doors are untouched.


// ENDGAME B.3.5 (b35-K2): `fn_Graphics_ParticleInstance_IncrementInstanceIndex`
// (?IncrementInstanceIndex@ParticleInstance@Graphics@@SAXXZ) is GONE — zero
// callers. The FACT it documented is load-bearing and stays on record here:
// `ParticleSystemInstance::Update` early-returns when `emit+0x2c == _frameIndex`,
// so this static MUST be advanced once per render frame or baked particle systems
// stop simulating after their first update. Whoever calls it now calls the member.

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

// ENDGAME B.3.5 (b35-K2): `fn_Graphics_SunLight_Set` (?Set@SunLight@Graphics@@)
// is GONE — zero callers. ENDGAME B.3.9 pilot: `fn_Graphics_SunLight_Ctor`
// (the ??0SunLight door) is GONE too — its one caller,
// MenuDialogLoopT_LoadSceneA1, builds `::new (p) Graphics::SunLight()`.

// (fn_Graphics_MeshInstance_Ctor went at ENDGAME B.3.9 EXA: MenuDialogLoopT's
// scene build writes `::new (p) Graphics::MeshInstance()`.)

// ENDGAME B.3.5 (b35-K2): `fn_Graphics_MeshInstance_SetMesh`
// (?SetMesh@MeshInstance@Graphics@@) is GONE — zero callers. It had FOUR in
// B35_RULES section 7's table.

// ENDGAME B.3.5 (b35-K2): `fn_Video_Context_SetFog` (?SetFog@Context@Video@@) is
// GONE — zero callers.

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

// ENDGAME B.3.5 (b35-K2): `fn_Graphics_Camera_SetFrustum`
// (?SetFrustum@Camera@Graphics@@) is GONE — zero callers. `fn_Graphics_Camera_Ctor`
// above it STAYS: a ctor door with ten live uses (B4b-2 / section 11).

// --- Audio::Scene 3D emitter API (ambient environment + building emitters) ---
// These back the AmbientEnvironmentComponent / AmbientEmitterComponent audio
// (biome wind/nature beds, per-building loops). All are Runtime-x64.dll exports;
// resolved by mangled name so both DLL and standalone builds share the pointer.

// ============================================================
// DebugUI deep-reimpl helper functions (exe sub-functions)
// ============================================================


// fn_FindComponent_Fire — searches entity for FireComponent (DAT_1401c41e0)
void fn_FindComponent_Fire(void* entity, void* retval);

// fn_FindComponent_RawMaterial — searches entity for RawMaterialDescription (DAT_1401c7cb8)

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


// fn_FindComponent_ResidenceFC — searches entity for InteractComponent (DAT_1401c7900);
// function name kept for callers.


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

// ============================================================
// SaveLoadDialog::Load — game load loop construction helpers
// ============================================================

// ENDGAME B.3.9 EXA: the GameUpdateLoop / GameLoadDialogLoopT / GameLoaderLoop /
// MenuDialogLoopT builders construct the real classes (code/Game/Game/*Loop*.h);
// the four loop ctor doors, the four exe loop vtable globals and the inline
// MenuDialogLoopT_Ctor are gone.

// System::Random::Random() — default ctor (Runtime 0x1800c7f20), seeds from time(nullptr)
// ??0Random@System@@QEAA@XZ
typedef void* (*fn_Random_Default_Ctor_t)(void* self);
extern fn_Random_Default_Ctor_t fn_Random_Default_Ctor;

// Audio::Scene::Scene() — Runtime default ctor (0x1800e0160)
// ??0Scene@Audio@@QEAA@XZ — initializes 0x38-byte Audio::Scene sub-object.
typedef void* (*fn_AudioScene_Ctor_t)(void* self);
extern fn_AudioScene_Ctor_t fn_AudioScene_Ctor;

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
    reinterpret_cast<UI::UserInterface*>(ui)->SetCursor(  // B.3.6 bridge
        ::Object::ExternalPtr<UI::Cursor>(reinterpret_cast<UI::Cursor*>(cursorVal)));
    // Fade to black (fadeIn=true, color=0xFF000000=opaque black, reverse=false)
    reinterpret_cast<UI::UserInterface*>(ui)->SetFullscreenFade(true, 0xFF000000, false);  // B.3.6 bridge
    // Store next loop pointer at engine+0xC8
    *(void**)(enginePtr + 0xC8) = loaderLoop;
}

// SetDialogLoaderNextLoop — FUN_14009a8f0 — NOW REIMPLEMENTED INLINE
// Sets fullscreen fade on DialogLoaderLoopT's UI (+0x40), stores next loop at +0xb68.
// Old typedef kept for reference:
// typedef void (*fn_SetDialogLoaderNextLoop_t)(uintptr_t dialogLoaderLoop, void* loaderLoop);
// extern fn_SetDialogLoaderNextLoop_t fn_SetDialogLoaderNextLoop;
inline void SetDialogLoaderNextLoop(uintptr_t dialogLoaderLoop, void* loaderLoop) {
    reinterpret_cast<UI::UserInterface*>(dialogLoaderLoop + 0x40)->SetFullscreenFade(  // B.3.6 bridge
        true, 0xFF000000, false);
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
    // B.3.4b bridge: the uint overload ?Rand@Random@System@@QEAAII@Z (exe slot
    // 0x1401512e8, Runtime 0x1800c8040) — `12u` picks it over Rand() and
    // Rand(int,int). The ctor door above STAYS (B.3.4b keeps ctor/dtor doors).
    uint32_t variant = reinterpret_cast<System::Random*>(rng)->Rand(12u);
    // BANISHED_MENU_SCENE pins every random front-end pick, this one included:
    // the loading image it selects stays resident and is numbered into the
    // save's reference table, so an unpinned pick shifts every reference id
    // after "Dialog\Loading.rsc:gameLoadN" (tools/resave_gate/).
    if (Opt().menuScene >= 0)
        variant = static_cast<unsigned>(Opt().menuScene) % 12u;
    // ?Format@String@System@@SA?AV12@PEBDZZ  retail 0x1800c9730
    *reinterpret_cast<System::String*>(retStr) =
        System::String::Format("Dialog/Loading.rsc:gameLoad%i", variant);
    return retStr;
}

// ============================================================
// ModDialog::Slot4 support (Application-x64.exe + Runtime-x64.dll)
// ============================================================

// ENDGAME B.3.9 EXA: the MaterialInstance / SpriteSheet ctor doors and the
// exe thumbnail table globals are gone — `::new (p) Graphics::MaterialInstance()`
// / `UI::SpriteSheet()` (the exe tables 0x14015c400 / 0x14015c3b0 are the
// classes' own, read in Ghidra: slot 0 the exe scalar deleting dtor, 1-8 thunks
// onto the class's own members).

// ModDialog thumbnail array push (FUN_140012a60) — 0x58-byte stride
// Resizes and copies 0x58-byte entries into the mod cache array.
// RCX=arrayHeader(ptr+count+capacity), RDX=newCount, R8=entry(0x58 bytes)
typedef void (*fn_ModDialog_ArrayPush_t)(void* arrayHeader, int newCount, void* entry);
extern fn_ModDialog_ArrayPush_t fn_ModDialog_ArrayPush;  // 0x140012a60


// DAT_14017214c — UV scale constant (= 1.0f used in SpriteSheet::SetSpriteInfo)
extern float* g_fUVScale;  // DAT_14017214c

// ============================================================
// SpriteCaptureUI support functions
// ============================================================

// Object::External::SaveText(this, path, subdir) -> bool
// ?SaveText@External@Object@@QEAA_NAEBVFilename@System@@0@Z (0x1800bb9d0, ordinal 364)

// ============================================================
// Stage 3: OptionsDialog Runtime exports
// ============================================================

// --- Audio::Configuration ---

// --- Video::Configuration --- EMPTY since ENDGAME B.3.5 (b35-close):
// `Configuration::Save` is a member of the real `Video::Configuration`.

// --- Input::Configuration ---

// --- IO::Persistence ---
// WriteSettings / ReadSettings — REMOVED, ENDGAME B.3.4a closing 2d (0 CALLs).

// --- Input::Device ---

// --- Video::Device --- EMPTY since ENDGAME B.3.5 (b35-close): every door this
// section held (`Startup`, `Shutdown`, `ApplyChangedSettings`, `RecreateDynamic`,
// `ReleaseDynamic`, `ValidateConfiguration`, `GetMultisampleModeCount`,
// `IsMultisampleModeSupported`, `GetInterface`, `GetInterfaceCount`, `Unlock`) is a
// member of the real `Video::Device`; its singleton is the class static.

// --- Engine::MainLoop ---


// ============================================================
// Stage 3: OptionsDialog Apply/Restore exe helpers
// ============================================================

// UI::UserInterface::GetFirstDialog (0x180031150, ordinal 2670) no longer has a
// pointer here: its last consumer, FindDialogByName_Impl below, walks
// `ui->_dialogs` directly, which is that function's whole body
// (`*retval = this->_dialogs._head`).

// SaveGameSettings(gameSettings) — persists all game settings to IO::Persistence
// FUN_140098970 in exe (calls Persistence::Set for 16 entries)
typedef void (*fn_SaveGameSettings_t)(int32_t* gameSettings);
extern fn_SaveGameSettings_t fn_SaveGameSettings;

// ReadGameSettings(gameSettings) — reads all game settings from IO::Persistence
// FUN_140098b60 in exe (calls Persistence::Get for 16 entries)
typedef void (*fn_ReadGameSettings_t)(int32_t* gameSettings);
extern fn_ReadGameSettings_t fn_ReadGameSettings;

// ApplyKeyMapToUI(keyMapSingleton, userInterface) — copies keymap config fields to UI
// FUN_140099260 in exe: copies the KEYMAP's +0x58..+0x78 and +0xA8/+0xB0 into the UI,
// sets UI scale. The offsets named here are SOURCE offsets in the KeyMap; the
// destinations are UserInterface +0xA8..+0xC8 (sound slots 2..6 of _sounds[7] at +0x98)
// and the fade sprite at UI +0xe0 (Wave 7 lane C, which reversed that layout).
// Read as a claim about UI offsets until then, which it is not.
typedef void (*fn_ApplyKeyMapToUI_t)(void* keyMap, void* userInterface);
extern fn_ApplyKeyMapToUI_t fn_ApplyKeyMapToUI;

// UpdateAutoSaveTimer(dialogInstance) — resets autosave timer based on g_pGameSettings[1]
// FUN_14005e120 in exe: reads autosave interval, starts/stops timer 0
typedef void (*fn_UpdateAutoSaveTimer_t)(void* dialogInstance);
extern fn_UpdateAutoSaveTimer_t fn_UpdateAutoSaveTimer;


// RegisterDefaultInputBindings — clears KeyMap, re-registers all default bindings, then loads saved config
// FUN_1400992f0 in exe (standalone function, no parameters)
typedef void (*fn_RegisterDefaultInputBindings_t)();
extern fn_RegisterDefaultInputBindings_t fn_RegisterDefaultInputBindings;

// Object::InternalBasePtr::DeletePtr(this) — destroys internal object, nulls pointer
// ?DeletePtr@InternalBasePtr@Object@@QEAAXXZ
typedef void (*fn_InternalBasePtr_DeletePtr_t)(void* self);
extern fn_InternalBasePtr_DeletePtr_t fn_InternalBasePtr_DeletePtr;

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

// ============================================================
// Description class reimpl support (shared serialization helpers)
// ============================================================

// (fn_ElementDescription_Dtor / _Ctor and fn_Element_Ctor — doors read through
// exe slots 0x1401518b8 / 0x1401518b0 / 0x1401518d8 — went at ENDGAME B.3.9
// EXA: zero readers.)

// SerializerEncodingVCall — REMOVED, ENDGAME B.3.4a closing 2d. It hand-rolled
// a virtual dispatch through the 24-byte Serializer's encoding sub-object at
// +0x08; it had no caller left once IO::Serializer became a real class.

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

// IO::Serializer::Serialize(this, count, buffer, name) — bulk serialize bytes.
// REMOVED, ENDGAME B.3.4a closing 2d (0 CALLs). Its slot 0x140152400 stays
// registered in standalone/address_registry.cpp.

// FUN_14000a590(arrayPtr, newCount) — dynamic array resize (exe function)
typedef void (*fn_DynArray_Resize_t)(void* arrayPtr, int newCount);
extern fn_DynArray_Resize_t fn_DynArray_Resize;

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

// FUN_1400328c0(self) — CreateBridgeDescription::CreateInPlace (exe, unhookable, 12 bytes)
// Called from CreateBridgeDescription::Cleanup to re-construct in place.
typedef void (*fn_CreateBridgeDescription_Ctor_t)(void* self);

// FUN_1400329b0(self) — CreateDragDescription::CreateInPlace (exe, unhookable, 12 bytes)
// Called from CreateDragDescription::Cleanup to re-construct in place.
typedef void (*fn_CreateDragDescription_Ctor_t)(void* self);

// FUN_140004920(self) — CreateFencedDescription::CreateInPlace (exe, unhookable, 12 bytes)
// Called from CreateFencedDescription::Cleanup to re-construct in place.
typedef void (*fn_CreateFencedDescription_Ctor_t)(void* self);

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

// FUN_140101a80(entityUI, retval, description) — toolbar UTILITY PANEL toggler.
// ⚠ The old name/description here ("set player follow-building") was a MISNOMER.
// The decompile find-or-creates the ToolUI child DialogInstance for a Description,
// tracks its position in the BST at arg1+0x50, and AddDialog/CloseDialog's it.
// Same function as toolbardialog.cpp's Toolbar_Utility_Impl; registered at the
// address via Toolbar_Utility_Reg so both names resolve to the one body.
// arg1 is the EntityUI at gameWorld+0x120, NOT the PlayerController.
typedef void (*fn_SetFollowBuilding_t)(void* playerController, void* retval, uint64_t entityPtr);
extern fn_SetFollowBuilding_t fn_SetFollowBuilding;

// FUN_140102b60(playerController, mode) — set player controller edit mode
// mode=0 clears current selection/edit.
typedef void (*fn_SetEditMode_t)(void* playerController, uint64_t mode);
extern fn_SetEditMode_t fn_SetEditMode;

// ============================================================
// Phase G Wave 1: SaveLoadDialog DoLoad/DoSave/DoDelete + EventNotifyDialog NavigateToEvent
// ============================================================

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
// NOTE: the body of exe 0x1400e1600 (the two door pointers once bound to it are gone).
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

    // Get type IDs from Factory (retail: **import slot 0x140152760)
    void* factoryObj = ::Object::Factory::_singleton;
    if (!factoryObj) {
        *(uintptr_t*)ret = 0;
        return retval;
    }

    uint32_t dialogTypeId =
        reinterpret_cast<const ::Object::Factory*>(factoryObj)   // B.3.3 bridge
            ->GetTypeIdFromName(part1, true);
    if (dialogTypeId == 0) {
        *(uintptr_t*)ret = 0;
        return retval;
    }

    uint32_t controllerTypeId = 0;
    if (partCount == 2 && part2[0] != 0) {
        controllerTypeId =
            reinterpret_cast<const ::Object::Factory*>(factoryObj)   // B.3.3 bridge
                ->GetTypeIdFromName(part2, true);
    }

    // Get UI singleton
    void* gameWorld = *g_pGameWorldSingleton;
    if (!gameWorld) {
        *(uintptr_t*)ret = 0;
        return retval;
    }
    UI::UserInterface* ui =
        *reinterpret_cast<UI::UserInterface**>((uintptr_t)gameWorld + 0x98);
    if (!ui || !fn_IsA) {
        *(uintptr_t*)ret = 0;
        return retval;
    }

    // Walk the dialog list.
    // `_dialogs.GetFirst()` IS retail's UI::UserInterface::GetFirstDialog
    // (0x180031150, literally `*retval = this->_dialogs._head`), reached here
    // through dispatch slot 0x140151ca0; the Iterator is the bare node pointer,
    // so `it.node` is exactly what retail hands to CheckPtr, and `++it` is
    // retail's `node = *(node + 0x10)`.
    // Original: LEA RDX,[RBP+0x48]; MOV RCX,RSI; CALL [0x140151ca0]
    for (auto it = ui->_dialogs.GetFirst(); it; ++it) {
        void* node = it.node;
        // CheckPtr on the iterator node (InternalBasePtr) before dereferencing.
        // Original: MOV RCX,RSI; CALL [0x140152ab0] (fn_InternalBasePtr_CheckPtr)

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
                {
                    uintptr_t controllerBuf[2] = {};
                    ::new (static_cast<void*>(controllerBuf)) ::Object::InternalPtr<UI::DialogController>(
                        reinterpret_cast<UI::DialogInstance*>(*(void**)node)->GetController(controllerTypeId));  // B.3.6 bridge
                    if (controllerBuf[0] != 0) {
                        // Controller matched — return this dialog
                        *(uintptr_t*)ret = (uintptr_t)*(void**)node;
                        return retval;
                    }
                    // Controller didn't match — continue searching
                }
            }
        }
        // `++it` is retail's next-node step:
        // MOV RAX,[RBP+0x48]; MOV RSI,[RAX+0x10]; MOV [RBP+0x48],RSI
    }

    // Not found
    *(uintptr_t*)ret = 0;
    return retval;
}

// --- Game_TriggerDisasterByType (0x140060110) — REIMPLEMENTED inline ---
// Iterates ExternalFactory for entities with ComponentDescription (DAT_1401c7318),
// finds SelectDescription sub-component (DAT_1401c48a8), checks type match,
// removes matching entity via Game_RemoveEntity.
void Game_TriggerDisasterByType_Impl(int disasterType);

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

void Game_SpawnResources_Impl(void* citizenListPtr, uint32_t resourceFlag, int count);

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
// NOTE: fn_FindComponent_Character keeps its historical name but finds ModelComponent
// (DAT_1401c7a50). Body: code/Game/Dialog/toolbardialog.cpp.

void Toolbar_CenterOnEntity_Impl(void* toolController, void* parentDialogPtr, void* entityPtr);

// ============================================================
// Work class helpers (shared by BuildStructure, ClearArea, ClearStorage)
// ============================================================

// Tree node removal by key (CitizenAI_impl_5520 @ 0x140025520)
// Searches binary tree at *param_1 for node matching param_2's key, removes it.
// Used by OnComplete/OnCancel to remove citizen from assignment tree.
typedef void (*fn_TreeRemoveByKey_t)(void* treeRootPtr, void* keyIBP);
extern fn_TreeRemoveByKey_t fn_TreeRemoveByKey;

// Recursive tree node free (FUN_140014c30 @ 0x140014c30)
// Recursively frees a binary tree (children at +0x10 and +0x18).
typedef void (*fn_TreeRecursiveFree_t)(void* treeRootPtr, void* node);
extern fn_TreeRecursiveFree_t fn_TreeRecursiveFree;

// (fn_ObjectInternal_Dtor — the ??1Internal door via slot 0x140152770 — went
// at ENDGAME B.3.9 EXA: never called, only a readiness conjunct.)

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

// The three "…C" finders are exe-resolved function pointers rather than the inline
// type-id form above. All three were misnamed; the names below come from the type-id
// globals in standalone/factory_registrations.csv. Behaviour is unchanged — only the
// names were wrong, and every call site was already passing/using the right object.

// FUN_1400223c0 — FindComponent type DAT_1401c7c80 = "RawMaterialComponent" (csv:288).
// RCX=entity, RDX=outIBP
typedef void (*fn_FindComponent_RawMaterialC_t)(int64_t entity, void* outIBP);
extern fn_FindComponent_RawMaterialC_t fn_FindComponent_RawMaterialC;

// FUN_1400052c0 — FindComponent type DAT_1401c7b30 = "NaturalResourceComponent" (csv:282).
// RCX=entity, RDX=outIBP
typedef void (*fn_FindComponent_NaturalResourceC_t)(int64_t entity, void* outIBP);
extern fn_FindComponent_NaturalResourceC_t fn_FindComponent_NaturalResourceC;

// FUN_140003050 — the exe-pointer twin of the inline fn_FindComponent_Work above
// (DAT_1401c8070 = "WorkComponent"). RCX=entity, RDX=outIBP
typedef void (*fn_FindComponent_WorkC_t)(int64_t entity, void* outIBP);
extern fn_FindComponent_WorkC_t fn_FindComponent_WorkC;

// FUN_1400f8620 — TryAssignWorkplace
// RCX=assignMgr, RDX=citizenRef, R8D=workerCount, R9B=flag
typedef uint64_t (*fn_TryAssignWorkplace_t)(uintptr_t mgr, void* citizenRef, int count, char flag);
extern fn_TryAssignWorkplace_t fn_TryAssignWorkplace;

// FUN_140006330 — scan a DESCRIPTION list (list+0x28 array / +0x30 count) for the entry
// whose type id is DAT_1401c7cb8 = "RawMaterialDescription" (csv:289). Not a component
// finder: 0x1400223c0 (fn_FindComponent_RawMaterialC) is the component one.
// RCX=descList, RDX=out
typedef void (*fn_FindDescription_RawMaterial_t)(int64_t descList, void* out);
extern fn_FindDescription_RawMaterial_t fn_FindDescription_RawMaterial;

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


void fn_FindComponent_Unknown7ee8(void* entity, void* retval);

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

// FUN_1400bf790: Pathfinding_FindPath — misnomer; the citizen work-action-chain
// builder. Arg 8 is the raw-material template ENTITY POINTER (64-bit!) — it was
// `long` (32-bit on Win64) until 2026-07-20, which truncated the pointer at the
// call boundary and crashed the first ClearStorage haul (upgrade path test).
typedef void (*fn_Pathfinding_FindPath_t)(void* outResult, void* citizenPtr, int pathType,
    void* destEntityIBP, float* destPos, void* srcEntityIBP, float* srcPos,
    int64_t materialDescEntity, int itemCount, int flags, int param11, char param12,
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

// FindComponent_WorkPlace (0x140002e10) — type DWORD_1401c80e0 (WorkPlaceComponent).
// ⚠ RENAMED 2026-08-09. This was called "..._BuildDesc" and documented as
// BuildDescription, which is WRONG: 0x1401c80e0's only [DATA] writer is
// WorkPlaceComponent_RegisterType (0x14014feb5) — every other xref is a read. The
// behaviour was never affected (the ports call retail's address, so they always got a
// WorkPlaceComponent), but the name invited the classic bug of treating the result as a
// description. The existing call sites corroborate the corrected name: buildstructure.cpp
// reads +0x60 as the builder/roster count and +0x50 as the accepting flag, both of which
// are WorkPlaceComponent fields. Fifth instance of the misnamed-finder class.
typedef void* (*fn_Entity_FindComponent_WorkPlace_t)(uintptr_t entity, void* retIBP);
extern fn_Entity_FindComponent_WorkPlace_t fn_Entity_FindComponent_WorkPlace;

// FindComponent_MatDesc (0x140003050) — type DWORD_1401c8070 (MaterialDescription)
// Note: same address as fn_FindComponent_WorkC, different type ID baked in
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

// CitizenComponent_HasVacancy (0x140017050) — WorkPlaceComponent roster (+0x60) < seat
// capacity (its StorageLocationComponent/WorkComponent +0x30, lazily CalcMaxWorkers'd).
// Reimplemented INLINE: it was ResolveAddr(Module::Exe,0x140017050), which is NULL in
// standalone (that exe address isn't registered), and citizencomponent.cpp:1176/1193 call
// it UNGUARDED — so once a placed building reaches construction and a citizen evaluates
// switching workplaces, the null call crashed (VCALL-of-0). Byte-faithful to 0x140017050
// (FUN_140003050 = fn_FindComponent_Work, FUN_14012e5c0 = fn_StorageCountQuery), plus
// one SA safety guard the original lacks: a null storage-loc component → "no vacancy"
// rather than dereferencing null+0x30. Works in both DLL and standalone builds.
inline bool fn_CitizenComponent_HasVacancy(uintptr_t workplaceComp) {
    int roster = *reinterpret_cast<int*>(workplaceComp + 0x60);
    uint64_t slComp = 0;
    fn_FindComponent_Work(*reinterpret_cast<void**>(workplaceComp + 0x10), &slComp);
    if (slComp == 0) return false;
    uintptr_t sl = static_cast<uintptr_t>(slComp);
    if (*reinterpret_cast<int*>(sl + 0x30) == 0) {
        int cap = fn_StorageCountQuery(reinterpret_cast<void*>(sl));
        *reinterpret_cast<int*>(sl + 0x30) = cap;
    }
    return roster < *reinterpret_cast<int*>(sl + 0x30);
}

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


// Object::InternalBasePtr::Serialize — REMOVED, ENDGAME B.3.4a closing 2d
// (0 CALLs; every site is now a member call on a real InternalBasePtr).

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
// (FUN_14001b340, the matching resize, went with the B.3.2 closing pass at zero
// callers — buildinggoal.cpp sizes the array through Collection::Array<...>.)

// FUN_14001b1d0 — compute string hash for building type entry
typedef void (*fn_StringHash_t)(void* str);

// System::Filename::~Filename() — the class destructor (runtime/types/system/Filename.h)
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


// fn_ExeArrayResizeFill (FUN_140003110, the three-argument Array<T>::SetSize(n,
// fill), stride 8 align 8) went with the B.3.2 closing pass: its six call sites
// in code/Game/Entity/componententity.cpp are now
// Collection::Array<GameColl::Ptr8>::SetSize(n, fill).

// ============================================================
// CitizenBehavior dependencies
// ============================================================

// CitizenBehavior::ProcessPaths (0x1400d6f00) — process pending path calculations.
typedef uint64_t (*fn_CitizenBehavior_ProcessPaths_t)(void* pathGraph);

// CitizenBehavior::BuildPathGraph (0x1400d70c0) — build/rebuild path distance graph.
typedef void (*fn_CitizenBehavior_BuildPathGraph_t)(void* pathGraph);

// CitizenBehavior::ShuffleSchools (0x1400896a0) — Fisher-Yates shuffle on schools.
typedef void (*fn_CitizenBehavior_ShuffleSchools_t)(void* self);

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

// Graphics::MeshInstance::UpdateHierarchy — update mesh bone/transform hierarchy
// IAT dispatch slot 0x140152888. RCX=MeshInstance*
typedef void (*fn_MeshInstance_UpdateHierarchy_t)(void* mesh);
extern fn_MeshInstance_UpdateHierarchy_t fn_MeshInstance_UpdateHierarchy;

// ============================================================
// Graphics::EdgeMesh family — selection-outline tessellation
// (used by the ported entity-highlight render chain in
//  code/Game/Player/tool_base_render.cpp). All resolved from
//  Runtime-x64.dll by mangled name (the exe reaches them via the
//  dispatch slots noted; those slots are empty in standalone).
// ============================================================

// System::MemoryAllocator::MemAlloc — STATIC (size, alignment) -> void*
// ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z   (exe slot 0x140152b60)
// NOTE: distinct from fn_MemAlloc (the member form taking the singleton).
typedef void* (*fn_MemAllocStatic_t)(uint64_t size, int alignment);
extern fn_MemAllocStatic_t fn_MemAllocStatic;

// Volume::Box::Box() — default ctor. exe slot 0x1401513f8
// ??0Box@Volume@@QEAA@XZ
typedef void* (*fn_Box_Ctor_t)(void* self);
extern fn_Box_Ctor_t fn_Box_Ctor;

// Volume::Box::Extend(const Vector3&) const -> Box (value return). exe slot 0x1401513f0
// ?Extend@Box@Volume@@QEBA?AV12@AEBVVector3@Math@@@Z
// Member value-return ABI: RCX=this(box), RDX=retbuf(Box*), R8=Vector3* -> returns retbuf.
typedef void* (*fn_Box_Extend_t)(void* box, void* retbuf, const void* vec3);
extern fn_Box_Extend_t fn_Box_Extend;

// Volume::Box::operator=(const Box&) -> Box&. exe slot 0x1401513b8
// ??4Box@Volume@@QEAAAEAV01@AEBV01@@Z   RCX=this, RDX=src -> returns this
typedef void* (*fn_Box_Assign_t)(void* dst, const void* src);
extern fn_Box_Assign_t fn_Box_Assign;

// Graphics::EdgeMesh::EdgeMesh() (fn_EdgeMesh_Ctor) is GONE in ENDGAME B.3.9 GFX:
// the class is concrete; its one caller placement-news it.

// Graphics::EdgeMesh::TriangleVertex::TriangleVertex(const Vector3&). exe slot 0x140151378
// ??0TriangleVertex@EdgeMesh@Graphics@@QEAA@AEBVVector3@Math@@@Z   -> returns this (8-byte vtx)
typedef void* (*fn_EdgeMesh_TriangleVertex_Ctor_t)(void* self, const void* vec3);
extern fn_EdgeMesh_TriangleVertex_Ctor_t fn_EdgeMesh_TriangleVertex_Ctor;

// Graphics::EdgeMesh::EdgeVertex::EdgeVertex(const TriangleVertex&, TriangleVertex&, int). slot 0x140151390
// ??0EdgeVertex@EdgeMesh@Graphics@@QEAA@AEBUTriangleVertex@12@0H@Z  -> returns this (16-byte vtx)
typedef void* (*fn_EdgeMesh_EdgeVertex_Ctor_t)(void* self, const void* tvA, const void* tvB, int flags);
extern fn_EdgeMesh_EdgeVertex_Ctor_t fn_EdgeMesh_EdgeVertex_Ctor;

// Graphics::EdgeMesh::Set (?Set@EdgeMesh@Graphics@@QEAA_NHPEBUTriangleList@
// GraphicsMesh@2@H0HPEAGHPEAXHH1H2HH1H2HH1H2HAEBVBox@Volume@@@Z, 0x1800a23e0, exe
// slot 0x140151380) is GONE in ENDGAME B.3.5 (b35-close): it is a member of the
// real `Graphics::EdgeMesh` now. This typedef held a SECOND transcription of the
// same 25 slots, grouped symmetrically as four {int,ushort*,int,void*,int} streams;
// the mangled name's own grouping is 6/5/5/4 because each group's trailing int is
// the NEXT pair's index count. Both spellings are ABI-identical (every slot is an
// int or a pointer), which is why nothing ever caught the difference -- see
// runtime/types/graphics/Mesh.h for the tokenisation and the proof from the body.

// ENDGAME B.3.5 (b35-K2): `fn_EdgeMeshInstance_SetMesh`
// (?SetMesh@EdgeMeshInstance@Graphics@@, exe slot 0x140151368) is GONE — zero
// callers. `fn_EdgeMeshInstance_Ctor` is GONE too (ENDGAME B.3.9 GFX): the class
// is concrete; HighlightComponent placement-news it at +0x28.

// CharacterComponent_impl_08c0 (0x1401008c0) — tile debug overlay rendering
// Submits colored tile arrays to the scene. Used by all Draw functions.
// RCX=self, RDX=Context*, R8=tileArray1, R9=tileArray2
typedef void (*fn_TileRender_t)(void* self, void* context, void* tileArray1, void* tileArray2);
extern fn_TileRender_t fn_TileRender;

// FUN_1400dbf90 — fenced placement region parameter computation
// Computes fence layout params (bounds, rotation, etc.) from FencedTool state.
// Returns bool (true=valid placement).
typedef bool (*fn_FencedPlacementParams_t)(void* self, uint32_t* outParam2, uint32_t* outParam3,
    uint32_t* outParam4, uint32_t* outParam5, float* outParam6, uint32_t* outParam7);
extern fn_FencedPlacementParams_t fn_FencedPlacementParams;

// ============================================================
// EntityList dependencies (Agent A)
// ============================================================

// IBP BST tree insert (0x140021a30)
typedef void* (*fn_IBPTreeInsert_t)(void* treeRootPtr, void* outBuf, void* ibpPtr);
extern fn_IBPTreeInsert_t fn_IBPTreeInsert;

// IBP BST tree remove (0x14001a6e0)
typedef void (*fn_IBPTreeRemove_t)(void* treeRootPtr, void* node);
extern fn_IBPTreeRemove_t fn_IBPTreeRemove;

// System::Time::Serialize(Serializer&) — REMOVED, ENDGAME B.3.4b System closing
// (0 CALLs). Callers use System::Time::Serialize(); the Runtime export
// ?Serialize@Time@System@@QEAAXAEAVSerializer@IO@@@Z stays bound.

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

// ============================================================
// M3.5 — MenuScene::Serialize generic helpers (Geometry/Transform serialize,
// dynamic-array resize/serialize, ExternalPtr serialize)
// ============================================================

// Runtime DLL ctors used by Reimpl_Geometry_Init when the cleaned body runs
// (standalone mode: exe 0x1400b00e0 is registered to the wrapper).
//   ??0VertexBuffer@Video@@QEAA@XZ     — Runtime 0x1800f55f0
//   ??0IndexBuffer@Video@@QEAA@XZ      — Runtime 0x1800f3490
// (fn_Graphics_Geometry_Ctor went at ENDGAME B.3.9 EXA: zero readers.)
typedef void (*fn_Video_VertexBuffer_Ctor_t)(void* self);
extern fn_Video_VertexBuffer_Ctor_t fn_Video_VertexBuffer_Ctor;
typedef void (*fn_Video_IndexBuffer_Ctor_t)(void* self);
extern fn_Video_IndexBuffer_Ctor_t fn_Video_IndexBuffer_Ctor;

// System::Color::Serialize — REMOVED, ENDGAME B.3.4b System closing (0 CALLs).
// Callers use System::Color::Serialize(); exe slot 0x140151ee0 stays registered
// and the export `?Serialize@Color@System@@QEAAXAEAVSerializer@IO@@@Z` stays bound.

// IO::Serializer::Serialize(float&) — REMOVED, ENDGAME B.3.4a closing 2d
// (0 CALLs). Slot 0x140152a60 stays registered in address_registry.cpp.

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
// Both share the standard array-resize shape (header = { void* data;
// int32 count; int32 cap }). They differ in element stride and MemAlloc
// alignment.
//
//   stride 0x02, align 2 — TradeWork_impl_sub_c890         @ 0x14001c890
//                          (uint16 array, used for index-buffer + sub-arrays)
//   stride 0x10, align 2 — FUN_14001ef10                   @ 0x14001ef10
//                          (mesh-vertex 16-byte struct copies 2 qwords/elem)
//
// The other three of the original five (0x14001efd0 stride-16 align-4,
// 0x14001f0a0 array-of-arrays, 0x1400021d0 stride-4 align-4) went with the
// B.3.2 closing pass at zero callers — those sites are Collection::Array<T>
// now. 0x1400021d0 still has ~20 citing sites in code/Game, but they call
// GameColl::I32::SetSize, not a pointer.
extern fn_Array_Resize_t fn_Array_Resize_Stride02;       // 0x14001c890  M3.5 Path B session 5
extern fn_Array_Resize_t fn_Array_Resize_Stride16B;      // 0x14001ef10  M3.5 Path B session 5

// ============================================================
// Runtime DLL primitives used by the MenuScene serialize helpers
// (resolved by mangled name; many already exist, listed here for reference)
// ============================================================

// IO::Serializer::GetEncoding (slot 0x1401529f8) and
// IO::Serializer::Serialize(uint&) (slot 0x140152a50) — REMOVED, ENDGAME
// B.3.4a closing 2d (0 CALLs). Both slots stay registered in
// standalone/address_registry.cpp; only these readers died.

// Object::ExternalFactory::IsReferenceModeEnabled — dispatch slot 0x140151218.

// Object::ExternalFactory::GetReferenceId — dispatch slot 0x140151220.

// Object::ExternalFactory::GetPathForReferenceId — dispatch slot 0x140151228.
// Member function returning Filename: hidden retval ABI -> RCX=this, RDX=retbuf, R8=refId.

// ============================================================
// MenuScene serialize helper — typeInfo storage (standalone-only)
// ============================================================
// In the original exe these are .rdata BaseTypeInfo*-pointer slots filled by
// the Runtime DLL's type-registration plumbing during startup. Standalone
// either resolves them from Runtime DLL exports or leaves them null and the
// helpers gracefully no-op (cascade fix is a later step).
//
// ENDGAME B.3.9 TR: the GraphicsMesh (0x140151f20) / Particle (0x140151420) import-slot
// pointers and the Weather cell pointer (0x1401c6198) are gone -- their readers use
// Graphics::GraphicsMesh::TypeId() / Graphics::Particle::TypeId() / Weather::TypeId().
// MaterialInstance's stays: Graphics::MaterialInstance::TypeId() is not ported yet.

// M3.5 Path B session 3 — additional typed-ExternalPtr serialize helper.
// FUN_140007140 is byte-identical to FUN_14001b8e0 (helper #9, GraphicsMesh)
// except for the typeInfo source slot. Reads BaseTypeInfo* from .rdata slot
// 0x140151350. Confirmed via shared xrefs: CharacterComponent_impl_08c0
// FindOrLoads "Terrain/Selection.rsc" then calls
// Graphics::MaterialInstance::Setup on the result, validating against the
// same slot — so 0x140151350 is the BaseTypeInfo* pointer slot for
// Graphics::MaterialInstance.
extern uint32_t** g_ppTypeInfo_MaterialInstance;  // .rdata slot 0x140151350

// ============================================================
// M3.5 Path B session 4 — per-element 0x60B serialize + typed-ExternalPtr
// array serialize (Geometry::Serialize step 10 + step 13 cascades)
// ============================================================

// TradeWork_impl_sub_0150 (0x140020150) — ~150 instructions, byte-identical
// shape to FUN_14001bcc0/FUN_14001bff0 (Array_Serialize_External). Loops
// `count` times, per-element FindOrLoad + IsDerivedFrom against the
// BaseTypeInfo* at .rdata slot 0x140151350 (Graphics::MaterialInstance — same
// slot as FUN_140007140; verified via disassembly:
//   140020340: MOV RAX, qword ptr [0x140151350]
//   140020347: MOV EDX, dword ptr [RAX]
//   14002034c: CALL [0x140152a98] ; IsDerivedFrom).
// Used by Geometry::Serialize step 13 for the +0x210 array (count +0x218).
// The fn_Array_Serialize_External_MaterialInstance pointer onto it went with the
// B.3.2 closing pass (zero callers; geometry_serialize.cpp drives it directly).

// ============================================================
// M3.5 Path B session 8 — final assembly: Geometry::Serialize body + extra
// ============================================================

// First-class typedefs for the IO::Serializer dispatch slots that session 8
// exercises heavily. Session 4 had these as locally-cached lazy-resolve
// statics; session 8 promotes them to externs so geometry_serialize.cpp +
// future serialize sites can share. Pointers themselves are resolved in
// EngineFuncsInit() — fall back to ResolveExport for the standalone-mode
// case where the .rdata slot is unfilled until Runtime DLL loads.
//
// All slots verified by Ghidra MCP: `disassemble_function 0x1400b04d0 CALL`.
// M3.5 Path B session 8
//
// ENDGAME B.3.4 step 0 — TWO OF THESE WERE ALIASES AND ARE GONE.
// `fn_Ser_Slot_SerInt_AC8` (0x140152ac8) and `g_fn_Serialize_Int` named the SAME
// retail dispatch slot by two routes: the former read the slot's contents
// (`ResolveAddr(Module::Exe, 0x140152ac8)` then `*sp`), the latter resolves the
// mangled export `?Serialize@Serializer@IO@@QEAAXAEAHPEBD@Z` — and
// `standalone/address_registry.cpp:1628` binds that slot to the same
// `RtSerializer_Serialize_Int` the export resolves to. Likewise
// `fn_Ser_Slot_SerFloat_A60` (0x140152a60) aliased `fn_Serializer_Serialize_Float`.
// Two names for one member meant two caller batches could independently "finish"
// it, so B34_RULES.md §6.5(b) collapses them BEFORE the caller batches start:
// the 28 `SerInt_AC8` call sites moved to `g_fn_Serialize_Int` (a pure token
// rename — the two typedefs differ only in spelling `int32_t*` vs `void*` for
// argument 2 and are ABI-identical), and `SerFloat_A60` was already zero-CALL.
// THE DISPATCH-SLOT ROWS THEMSELVES STAY BOUND (B34_RULES §6.5(g)): they are PE
// import slots that retail-shaped exe code reads, not our call sites.
//
// ENDGAME B.3.4a closing — THE REST ARE GONE TOO, for the opposite reason: they
// were never aliases, they simply ran out of CALLERS once the caller batches
// converted. `fn_Ser_Slot_SerUShortArr_488` (0x140152488) and
// `fn_Ser_Slot_SerByteArr_618` (0x140152618) went in closing 2a;
// `fn_Ser_Slot_SerFloat_A60` went in step 0; and closing 2d takes the last
// three, `fn_Ser_Slot_SerByte_410` (0x140152410),
// `fn_Ser_Slot_SerSizedRaw_3F8` (0x1401523f8) and `_400` (0x140152400),
// together with the readiness guards in code/Game that were their last readers
// and with the two typedefs `fn_Ser_SerByte_t` / `fn_Ser_SerSizedRaw_t`, which
// had no other user. Nothing in this block survives but the history.
// Same rule as above: the SLOT ROWS STAY BOUND in address_registry.cpp.

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
// Runtime export pointers for the per-item / collect Draw bodies.
//
// ENDGAME B.3.5 (b35-close): the VIDEO and GRAPHICS pointers of this block are
// GONE — `Renderer::{GetCullCamera, AddDrawItem, GetShader}`,
// `Camera::GetFrustum`, `Geometry::{GetTransform, UpdateAttached,
// GetSubTransform, GetInstanceTransform, SetSharedInstanceData, Clean}` and the
// seven `Video::Context` primitives are all members of real C++ classes now, and
// `code/Game/Game/geometry_draw.cpp` calls them as members (batch b35-K1). What
// remains in this block is the `Volume::Frustum` pair, the `Math` helpers and
// `Graphics::InterpolatedState<Transform>::Lerp`, whose receivers are not part of
// step 5. The nine vtable slots above still describe the derived exe-phase
// Geometry class, which is deliberately NOT converted (B35_RULES section 11).

typedef uint32_t (*fn_Frustum_Contains_t)(void* frustum, void* box, uint32_t mask);
extern fn_Frustum_Contains_t fn_Frustum_Contains;
// ?Contains@Frustum@Volume@@QEBAIAEBVBox@2@I@Z

typedef void* (*fn_Frustum_MulTransform3_t)(void* frustum, void* retval, void* xform3);
extern fn_Frustum_MulTransform3_t fn_Frustum_MulTransform3;
// ??DFrustum@Volume@@QEBA?AV01@AEBVTransform3@Math@@@Z (operator*)

// ?Inverse@Transform@Math@@QEBA?AV12@XZ

// ?AsMatrix@Transform@Math@@QEBA?AVMatrix@2@XZ

// ??0Transform3@Math@@QEAA@AEBV01@@Z

// ??0Vector3@Math@@QEAA@MMM@Z

// ?Lerp@?$InterpolatedState@VTransform@Math@@@Graphics@@QEBA?AVTransform@Math@@
// AEBVScalar@4@@Z (0x18001aff0) is GONE in ENDGAME B.3.5 (b35-close). Two caller
// batches kept it with the reason "no such class exists in runtime/types/graphics
// at all"; the closing pass authored the class
// (runtime/types/graphics/InterpolatedState.h) and ported the member instead of
// keeping the door, so both call sites -- geometry_draw.cpp and
// Component/statusicon_geometry.cpp -- are member calls now.

// ============================================================
// Initialization
// ============================================================

// Resolve all function pointers. Call after ResolveInit().
// Returns true if all critical pointers were resolved.
bool EngineFuncsInit();
