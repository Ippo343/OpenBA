#pragma once
// code/Game/Dialog/exe_bodies.h -- the exe bodies DEFINED in code/Game/Dialog/ that the registry
// reaches by retail address (B3R_RULES R5, block W0-P0c).
//
// One declaration per body, copied from its DEFINITION (linkage included),
// grouped by defining file; the retail address(es) the body is registered
// under are the trailing comment. The defining .cpp includes this header so
// the compiler checks each pair. Readers call these by name (W1); nothing
// here changes what runs. Making them members is B.4's work.
//
// Declared in another header already (not redeclared here):
//   AutoSaveDialog_Slot4                     0x14005e120  (code/Game/Dialog/AutoSaveDialog.h)
//   Dialog_FindComponent_d6c0                0x14000d6c0  (code/Game/Dialog/dialog_exe_helpers.h)
//   ToolbarDialog_Ctor_Impl                  0x1400b9910  (code/Game/Dialog/ToolbarDialog.h)

#include <cstdint>

class MainMenuDialog;  // code/Game/Dialog/MainMenuDialog.h
class TradeUI;         // code/Game/Dialog/TradeUI.h

// ---- autosavedialog.cpp
extern "C" void* AutoSaveDialog_Ctor_Standalone(void* self, void* ui, void* extRes, void* parentIBP);  // 0x14005e230

// ---- edittool_select.cpp
extern "C" void EditTool_SelectEntity_Impl(void* selfV, void* entityIBPv);  // 0x1400fd3b0

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

// ---- toolbar_utility_helpers.cpp
extern "C" void ToolbarUtility_FindDialogByControllerType_0270(void* , void* retBuf, uint64_t param);  // 0x140100270
extern "C" void ToolbarUtility_BstInsert_4890(int64_t* rootHolder, void** retNode, uint64_t* insertData);  // 0x140014890

// ---- toolbardialog.cpp
void Toolbar_ShowGroup_Impl(void* gameSubsys, int64_t groupId);  // 0x140102b60
extern "C" void* Toolbar_Utility_Reg(void* gameSubsys, void* retBuf, uint64_t param);  // 0x140101a80

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
