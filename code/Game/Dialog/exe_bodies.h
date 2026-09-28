#pragma once
// code/Game/Dialog/exe_bodies.h -- declarations of the exe bodies DEFINED in
// code/Game/Dialog/ (ENDGAME B.3R, block W0-P0c, B3R_RULES R5).
//
// Every body the registry reaches by retail address, declared once, copied
// from its DEFINITION (linkage included), grouped by defining file; the
// retail address(es) it is registered under are the trailing comment. The
// defining .cpp includes this header so the compiler checks each pair.
// Readers call these by name (W1); nothing here changes what runs. Making
// them members is B.4's work.
//
// Also here: the declarations and inline bodies of this module that lived in
// engine/funcs/engine_funcs.h, moved verbatim with their comments (that
// header includes this one, so none of its includers changes).
//
// Declared in another header already (not redeclared here):
//   AutoSaveDialog_Slot4                   0x14005e120  (code/Game/Dialog/AutoSaveDialog.h)
//   Dialog_FindComponent_d6c0              0x14000d6c0  (code/Game/Dialog/dialog_exe_helpers.h)
//   ToolbarDialog_Ctor_Impl                0x1400b9910  (code/Game/Dialog/ToolbarDialog.h)

#include <cstdint>
#include "engine_funcs.h"   // the globals the moved inline bodies still read
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree (B3R Leftovers)

class MainMenuDialog;  // code/Game/Dialog/MainMenuDialog.h
class TradeUI;         // code/Game/Dialog/TradeUI.h

// ---- autosavedialog.cpp
extern "C" void* AutoSaveDialog_Ctor_Standalone(void* self, void* ui, void* extRes, void* parentIBP);  // 0x14005e230

// ---- citizenui.cpp
// fn_FindComponent_CitizenAI — FUN_140011050, Entity::FindComponent<CitizenAIComponent>
// (compares against [0x1401c70e8] at 0x140011093, IsDerivedFrom at 0x1400110ec).
// ENDGAME B.3.9 CB: defined in code/Game/Dialog/citizenui.cpp over
// CitizenAIComponent::TypeId(); the cell is unregistered, so the old inline over
// the cell pointer only worked when a dialog re-pointed that global.
void fn_FindComponent_CitizenAI(void* entity, void* retval);

// ---- edittool_select.cpp
extern "C" void EditTool_SelectEntity_Impl(void* selfV, void* entityIBPv);  // 0x1400fd3b0

// ---- entityui.cpp
// fn_EntityUI_SetEditMode — reimplemented in entityui.cpp as EntityUI_SetEditMode_Impl
// FUN_140102b60: sets player controller edit state (BST lookup + Factory::CreateObject)
void EntityUI_SetEditMode_Impl(void* playerController, uint64_t mode);

// fn_EntityUI_SelectEntity — reimplemented in entityui.cpp as EntityUI_SelectEntity_Impl
// FUN_140102e30: BST insert/update for entity tracking
void EntityUI_SelectEntity_Impl(void* playerController, void* entityPtr,
                                 uint32_t x, uint32_t y);

// ---- eventnotifydialog.cpp
extern "C" void* EventNotifyDialog_Ctor_Standalone(void* self, void* ui, void* extRes, void* parentIBP);  // 0x140068ab0

// ---- mainmenudialog.cpp
void MainMenuDialog_ReturnToMenu(MainMenuDialog* self);  // 0x1400b4790

// ---- messagedialog.cpp
void* MessageDialog_Ctor_Standalone(void* self, void* ui, void* extRes, void* parentIBP);  // 0x14006d300

// ---- mod_dialog_vtables.cpp
extern "C" void* ModDialog_Ctor_Standalone(void* self, void* userInterface, void* externalRes, void* parentIBP, void* parentMenuIBP);  // 0x140070030
extern "C" void ModDialog_CloseAndReload(void* self);  // 0x14006e9b0

// ---- newgamedialog.cpp
extern "C" void StartNewGame_Reg(void* worldSetup, void* nameStr, void* seed, void* terrainSize, char disastersOff, void* mapGen, void* terrainGen, void* climateGen, unsigned scenarioResIdx, void* savedStateIBP);  // 0x1400b4540
void* NewGameDialog_Ctor_Standalone(void* self, void* ui, void* extRes, void* nullParent, void* parentMenuIBP);  // 0x140071040

// ---- optioninputbinddialog.cpp
void* OptionInputBindDialog_Ctor_Standalone(void* self, void* ui, void* extRes, void* parentIBP);  // 0x140071d30

// ---- optionsdialog.cpp
extern "C" void ApplyKeyMapToUI_Impl(void* keyMap, void* userInterface);  // 0x140099260
void* OptionsDialog_Ctor_Standalone(void* self, void* ui, void* extRes, void* parentIBP);  // 0x140075150

// ---- saveloaddialog.cpp
extern "C" void* SaveLoadDialog_Ctor_8270(void* self, void* ui, void* extRes, void* parentIBP, uint8_t mode, void* enginePtr, void* gameWorldPtr);  // 0x1400b8270

// SaveLoadDialog helper: FUN_1400b6ce0 — populates the file list table
// NOW REIMPLEMENTED in code/Game/Dialog/saveloaddialog.cpp as SaveLoadDialog_PopulateFileList()
// Old typedef kept for reference:
// typedef void (*fn_SaveLoadDialog_PopulateFileList_t)(void* self);
// extern fn_SaveLoadDialog_PopulateFileList_t fn_SaveLoadDialog_PopulateFileList;
class SaveLoadDialog;
void SaveLoadDialog_PopulateFileList(SaveLoadDialog* self);

// ---- storageui.cpp
// StorageUI sort function (FUN_1400213f0) — sorts 0x18-byte entries by resource name.
// Quicksort/introsort with inline comparator using UI::String → System::String.
// StorageUI_SortItems — reimplemented as clean C++ introsort in storageui.cpp
// Sorts array of 0x18-byte resource display structs alphabetically by resource name.
// Each struct: [0]=entityPtr, [8]=resourceTypePtr, [0x10]=count
// Original: FUN_1400213f0 (introsort) + FUN_1400b8370 (comparator) + FUN_140015ad0 (insertion sort)
void StorageUI_SortItems(void* items, int count, void* scratch, void* scratch2);

// ---- toolbar_utility_helpers.cpp
extern "C" void ToolbarUtility_FindDialogByControllerType_0270(void* , void* retBuf, uint64_t param);  // 0x140100270
extern "C" void ToolbarUtility_BstInsert_4890(int64_t* rootHolder, void** retNode, uint64_t* insertData);  // 0x140014890

// ---- toolbardialog.cpp
void Toolbar_ShowGroup_Impl(void* gameSubsys, int64_t groupId);  // 0x140102b60
extern "C" void* Toolbar_Utility_Reg(void* gameSubsys, void* retBuf, uint64_t param);  // 0x140101a80

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

// ---- toolui.cpp
// FUN_14012e100: UIDescription_AddControllers — REIMPLEMENTED in code/Game/Dialog/toolui.cpp
// Iterates UIDescription controller entries, checks for TabElement targets,
// and calls the appropriate AddController overload for each entry.
// Old fn_* pointer removed; replaced by UIDesc_AddControllers_Impl().
void UIDesc_AddControllers_Impl(void* uiDesc, void* dialogInstPtr, void* dataSource);

// ---- townhall_helpers.cpp
extern "C" void TownHall_GroupRatingInit(void* rating, void* groupIBPPtr);  // 0x14006abf0
extern "C" void TownHall_GroupRatingUpdate(void* rating, float value, float maxValue);  // 0x14006aed0
extern "C" void TownHall_ProfArrayPush(void* arrayBase, int newCount, void* entry);  // 0x140015800
extern "C" void TownHall_ProdArrayPush(void* arrayBase, int newCount, void* entry);  // 0x1400159b0
extern "C" void TownHall_AvlRebalance28(void* treeRoot, void* nodePtr);  // 0x1400149a0
extern "C" int TownHall_QueryProductionStat(void* townMgr, int seriesIdx, int startTick, int endTick);  // 0x140090f20
extern "C" void TownHall_InventorySortAsc(void* items, int count, void*, void*);  // 0x140015200
extern "C" void TownHall_InventorySortDesc(void* items, int count, void*, void*);  // 0x140015500
extern "C" void TownHall_RemoveWorker(void* citizenList, uint64_t professionId);  // 0x140091180

// ---- townhallui.cpp
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

// ---- tradeui_buyentries.cpp
extern "C" void TradeUI_GetBuyEntries_59670(void* tradeState, void* outArray);  // 0x140059670

// ---- tradeui_checkout.cpp
extern "C" void TradeDescription_ExtendMerchantStay(void* tradeComp);  // 0x14005a490
extern "C" void TradeUI_OnTradeButton(TradeUI* self, void* , void* , int64_t );  // 0x140081420

// ---- tradeui_leaves.cpp
extern "C" void TradeUI_DynArrayClear_16050(int64_t* header);  // 0x140016050
extern "C" void TradeUI_DynArrayClear_16130(int64_t* header);  // 0x140016130
extern "C" void TradeUI_DynArrayClear_16200(int64_t* header);  // 0x140016200
extern "C" void* TradeUI_GetOrderName_15fd0(int64_t tradeState, void* out);  // 0x140015fd0
extern "C" void TradeUI_AddPurchaseOrder_59490(int64_t tradeState, int64_t resourceEntity, int8_t addFlag);  // 0x140059490
extern "C" void* TradeUI_StorageTree_Insert_e1c0(int64_t* tree, void** out, const uint64_t* entry);  // 0x14000e1c0

// ---- tradeui_price.cpp
extern "C" int TradeUI_GetResourcePrice_a520(void* tradeState, void* resourceEntity);  // 0x14005a520

// ---- tradeui_sort.cpp
extern "C" void TradeUI_IntroSort_ByName_162d0(void* array, int count, void* , int64_t );  // 0x1400162d0
extern "C" void TradeUI_IntroSort_ByComponent_165d0(void* array, int count, void* ctx);  // 0x1400165d0

// ---- unreg_lane2_mapparamsdialog_ctor.cpp
extern "C" void* MapParamsDialog_Ctor_Standalone(void* self, void* ui, void* extRes, void* parentIBP);  // 0x14006bd80

// ---- engine/funcs/engine_funcs.cpp (B3R Leftovers: declared here, copied from the
// definition, so moddialog.cpp calls it by name; W2 moves the body into this module)
void fn_ModDialog_ArrayPush58_CppImpl(void* arrayHeader, int newCount, void* elementPtr);  // 0x140012a60

// ---- inline bodies (moved from engine/funcs/engine_funcs.h; no .cpp)

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

// TrackerComponent rebuild: FUN_14000d9d0 — moves/rebuilds tracker item arrays
// Called when game world item count changes.
// param_1 = dst array descriptor (uint64_t[2]: ptr, count+capacity)
// param_2 = src array descriptor (uint64_t[2]: ptr, count+capacity)
// Frees old dst buffer, allocates count*16 bytes, copies 16-byte entries from src.
inline void fn_TrackerComponent_Rebuild(void* dstArray, void* srcArray) {
    if (dstArray == srcArray) return;
    uint64_t* dst = (uint64_t*)dstArray;
    uint64_t* src = (uint64_t*)srcArray;
    System::MemoryAllocator::MemFree((void*)*dst);
    dst[1] = 0;
    dst[0] = 0;
    int count = (int)(uint32_t)src[1];  // low 32 bits of src[1]
    dst[1] = (uint64_t)(uint32_t)count;
    if (count > 0) {
        // High word of dst[1]+4 = capacity (same as count on fresh alloc)
        *(int*)((uintptr_t)dst + 0xc) = count;
        void* newBuf = System::MemoryAllocator::MemAlloc((uint64_t)count * 16, 8);
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
        void* randomPtr = *(void**)((uintptr_t)GameUpdateLoop::_singleton + 0xd8);
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
