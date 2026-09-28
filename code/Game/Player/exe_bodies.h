#pragma once
// code/Game/Player/exe_bodies.h -- the exe bodies DEFINED in code/Game/Player/ that the registry
// reaches by retail address (B3R_RULES R5, block W0-P0c).
//
// One declaration per body, copied from its DEFINITION (linkage included),
// grouped by defining file; the retail address(es) the body is registered
// under are the trailing comment. The defining .cpp includes this header so
// the compiler checks each pair. Readers call these by name (W1); nothing
// here changes what runs. Making them members is B.4's work.

#include <cstdint>

// ---- bridgetool_validate.cpp
extern "C" int BridgeTool_SnapEndCell_Impl(void* self, int endCell);  // 0x1400fb710
extern "C" uint32_t BridgeTool_ValidatePath_Impl(void* self, int startCell, int endCell, int* outLength);  // 0x1400fb2d0

// ---- keystone_a_removalcalcpath.cpp
extern "C" void* RemovalCalc_helper_8d50(int64_t param_1, void** param_2, int param_3);  // 0x1400f8d50
extern "C" void RemovalCalc_StateMachine_dfa0(int64_t param_1);  // 0x14011dfa0
extern "C" uint64_t RemovalCalc_helper_c700(uint32_t* param_1, int param_2);  // 0x14009c700
extern "C" void RemovalCalc_CitizenAI_9ad0(int64_t param_1, int param_2);  // 0x1400f9ad0

// ---- pathtool_setselected.cpp
extern "C" void PathTool_helper_d710(int64_t param_1, uint64_t* param_2);  // 0x1400dd710

// ---- tool_base_createbar.cpp
extern "C" void UpdateToolUI_fc60(void* param_1, int param_2, int param_3, void* param_4, char param_5, char param_6, char param_7, int param_8, int param_9, int param_10, void* param_11, void* param_12);  // 0x1400ffc60

// ---- tool_base_drag.cpp
void* DragTool_FindDescription_5160(uintptr_t descList, void* retval);  // 0x140005160
void DragTool_CloseToolDialog_ff980(uint64_t* param_1);  // 0x1400ff980
void DragTool_PlayTickSound_ffbf0(uintptr_t param_1);  // 0x1400ffbf0
void DragTool_PlayPlaceSound_1000f0(uint64_t );  // 0x1401000f0
void DragTool_PlayErrorSound_100140(uint64_t );  // 0x140100140
void DragPlacementTool_ClearPathEntities_1018d0(void* self, int minCol, int minRow, int maxCol, int maxRow, int64_t entityTemplate);  // 0x1401018d0

// ---- tool_base_edit.cpp
void* PlayerEdit_EntityAtCell_a0280(uintptr_t entityList, void* retIBP, int cellIndex);  // 0x1400a0280
float PlayerEdit_AvgTerrainHeight_101730(void* , int p2, int p3, int p4, int p5);  // 0x140101730
uint64_t PlayerEdit_FencedAnchorCalc_dbf90(uintptr_t self, uint32_t* param_2, uint32_t* param_3, uint32_t* param_4, uint32_t* param_5, float* param_6, uint32_t* param_7);  // 0x1400dbf90

// ---- tool_base_highlight.cpp
extern "C" void TradeWork_helper_14b00(int64_t param_1, void* node);  // 0x140014b00
extern "C" void TradeWork_impl_4b40(int64_t* param_1, int64_t* param_2);  // 0x140014b40
extern "C" int Player_HighlightAdd_55430(int64_t param_1, int param_2);  // 0x140055430

// ---- tool_base_highlight_tess.cpp
extern "C" void TradeWork_impl_ace40(int64_t param_1, int param_2, char param_3, int param_4, int param_5);  // 0x1400ace40

// ---- tool_base_path.cpp
void* PlayerPath_FindComponent_5780(uintptr_t entity, void* retIBP);  // 0x140005780
void* PlayerPath_FindComponent_3ea0(uintptr_t entity, void* retIBP);  // 0x140003ea0
uint64_t PlayerPath_Sub_11e710(uintptr_t param_1);  // 0x14011e710
bool PlayerPath_CharComp_f5d0(int* param_1, int param_2, int param_3, uint32_t param_4, float param_5, float param_6, float param_7, char param_8, char param_9, int64_t param_10);  // 0x14009f5d0
uint64_t PlayerPath_Sub_4670(uintptr_t param_1, void* param_2);  // 0x140104670
uint64_t PlayerPath_Sub_4770(uintptr_t param_1, void* param_2);  // 0x140104770
uint64_t PlayerPath_Sub_48a0(uintptr_t param_1, void* param_2, int param_3, int param_4);  // 0x1401048a0

// ---- tool_base_place.cpp
void PlacementToolPlace_FlipVariant_d960(void* selfp);  // 0x1400fd960

// ---- tool_base_ray.cpp
extern "C" uint8_t PlayerRay_TerrainMarch_aa530(uintptr_t param_1, float* param_2, float* param_3, float* param_4, float* param_5, float* param_6);  // 0x1400aa530
extern "C" uint8_t PlayerRay_GetMouseRay_1005a0(void* , float* outOrigin, float* outDir);  // 0x1401005a0
extern "C" int PlayerRay_ScreenToCell_101270(uintptr_t param_1);  // 0x140101270
extern "C" uint8_t PlayerRay_PlacementPosCalc_fda00(uintptr_t param_1, int* param_2, int* param_3, int* param_4, int* param_5, float* param_6, float* param_7);  // 0x1400fda00

// ---- tool_base_render.cpp
extern "C" void HighlightSelect_Impl(void* self);  // 0x1400fc9e0
extern "C" void HighlightClear_Impl(void* self);  // 0x1400fcb40
extern "C" void HighlightRepaint_Impl(void* self);  // 0x14004c4a0

// ---- tool_base_screenrect.cpp
extern "C" uint32_t PlayerScreenRect_BuildCorners_ec70(uint64_t , int param_2, int param_3, int param_4, int param_5, float* param_6);  // 0x1400fec70
extern "C" uint8_t PlayerScreenRect_WorldToScreen_098210(uintptr_t param_1, float* param_2, float* param_3);  // 0x140098210
extern "C" uint8_t PlayerScreenRect_AccumulateRect_fe960(uint64_t , uint32_t param_2, float* param_3, void* param_4);  // 0x1400fe960
extern "C" uint8_t PlayerScreenRect_Entity_0380(uintptr_t , void* param_2, uint64_t* param_3);  // 0x140100380

// ---- tool_base_tile.cpp
void ToolTile_SetTransformFromYaw_3870(uintptr_t self, const void* offset, float yaw);  // 0x140003870
unsigned long long ToolTile_FootprintValidity_d580(int* param_1, int param_2, int param_3, int param_4, int param_5, unsigned int param_6, long long* param_7, long long* param_8, float param_9, float param_10, float param_11, char param_12, long long* param_13, int param_14, long long param_15);  // 0x14009d580
unsigned long long ToolTile_ValidateCreate_fd650(long long param_1, int param_2, int param_3, int param_4, int param_5, const void* param_6, float param_7);  // 0x1400fd650

// ---- tool_base_tilerender.cpp
extern "C" void PlayerTileRender_DrawOverlay_08c0(uint64_t , void* ctx, int64_t* param_3, int64_t* param_4);  // 0x1401008c0
extern "C" void PlayerTileRender_BuildRecords_1380(uint64_t param_1, void* ctx, int param_3, int param_4, uint32_t param_5, int param_6, uint32_t param_7, int64_t* param_8, int64_t* param_9, float param_10, float param_11, float param_12, char param_13, char param_14, int64_t* param_15, int param_16, int param_17, char param_18, int64_t param_19);  // 0x140101380

// ---- tool_base_toolui.cpp
extern "C" void* ToolUI_OpenToolDialog_ff9e0(void* param_1, void* param_2, uint64_t param_3);  // 0x1400ff9e0

// ---- tool_base_trade.cpp
extern "C" void PlayerTrade_Retrace_25920(int64_t* root, int64_t* cursor);  // 0x140025920
extern "C" void PlayerTrade_BSTRemove_25740(int64_t* root, void* nodeArg);  // 0x140025740
extern "C" void PlayerTrade_BSTFindRemove_2c690(int64_t* root, void* keyIBP);  // 0x14002c690
extern "C" void PlayerTrade_EntityUI_66b0(int64_t param_1);  // 0x1400666b0
extern "C" void PlayerTrade_EditTool_66800(int64_t param_1);  // 0x140066800
