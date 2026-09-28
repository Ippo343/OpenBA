#pragma once
// code/Game/Game/exe_bodies.h -- the exe bodies DEFINED in code/Game/Game/ that the registry
// reaches by retail address (B3R_RULES R5, block W0-P0c).
//
// One declaration per body, copied from its DEFINITION (linkage included),
// grouped by defining file; the retail address(es) the body is registered
// under are the trailing comment. The defining .cpp includes this header so
// the compiler checks each pair. Readers call these by name (W1); nothing
// here changes what runs. Making them members is B.4's work.
//
// Declared in another header already (not redeclared here):
//   Game::RegisterDefaultInputBindings       0x1400992f0  (code/Game/Game/inputbindings.h)
//
// Internal linkage in the defining file (static / anonymous namespace):
// not declared -- that would change linkage, which this step does not do.
//   ResizeStride0x10                         0x140017640  (citizenlist_serialize.cpp:339, anonymous namespace)
//   ResizeStride0x20                         0x1400177c0  (citizenlist_serialize.cpp:350, anonymous namespace)

#include <cstdint>

// ---- citizen_names.cpp
extern "C" void* NameDB_GenerateName(void* retStr, void* db, void* rng, int minLen, int maxLen);  // 0x1400a4b30
extern "C" void* NameDB_PickAndGenerate(void* retStr, void* rng, char female);  // 0x1400a4c60

// ---- citizenlist_serialize.cpp
extern "C" void CitizenList_Serialize_8f760(void* self, void* stream, void* , int* );  // 0x14008f760
extern "C" void CitizenList_PostLoad_8f580(void* self);  // 0x14008f580

// ---- eventlog.cpp
extern "C" void EventLog_AddEvent(void* eventLog, int eventType, int severity, void* entityIBP);  // 0x140095580
extern "C" void EventLog_AddEventWithName(void* eventLog, int eventType, int severity, void* entityIBP, void* nameStr);  // 0x140095750
extern "C" void EventLog_AddCitizenEvent(void* eventLog, int eventType, int severity, void* entityIBP, void* nameStr, void* titleStr, void* extraStr);  // 0x140094e00

// ---- game_leaf_helpers.cpp
void Game_DestroyStringArray18(void* arrayHeader);  // 0x14001b030
void Game_EventNotifierCleanup(void* arrayHeader);  // 0x14000a8a0
void Game_MusicPlayerDtor(void* self);  // 0x140018220
void Game_ImageCaptureDtor(void* self);  // 0x1400a4fc0
void Game_WeatherDtor(void* self);  // 0x140093eb0

// ---- game_settings_persist.cpp
extern "C" void GameSettings_Load(int32_t* settings);  // 0x140098b60
extern "C" void GameSettings_Save(int32_t* settings);  // 0x140098970

// ---- gamegrid_serialize.cpp
extern "C" void GameGrid_Serialize_9f100(void* selfPtr, void* stream);  // 0x14009f100

// ---- gameupdateloop.cpp
extern "C" float CalcWindFactor_Reg(void* time);  // 0x1401027c0

// ---- gameupdateloop_load.cpp
extern "C" void* Reimpl_FUN_1400983f0(void* self, void* outTransform);  // 0x1400983f0
extern "C" void* Reimpl_FUN_140098660(void* camera, float fovY, float nearZ, float farZ);  // 0x140098660

// ---- gameupdateloop_serialize.cpp
extern "C" void BucketTree_Serialize_21ba0(void* serializer, void* , void* bucketsPtr);  // 0x140021ba0

// ---- gameupdateloop_teardown.cpp
extern "C" void EntityList_Shutdown_c3d30(void* selfV);  // 0x1400c3d30
extern "C" void EntityList_Dtor_c3e60(void* selfV);  // 0x1400c3e60
extern "C" void CitizenList_Dtor_907b0(void* selfV);  // 0x1400907b0
extern "C" void EntityUI_Dtor_1024d0(void* selfV);  // 0x1401024d0
extern "C" void Tutorial_Dtor_f8ff0(void* selfV);  // 0x1400f8ff0

// ---- geometry_serialize.cpp
extern "C" void Reimpl_TradeWork_impl_04d0(void* self, void* stream);  // 0x1400b04d0

// ---- goals_serialize.cpp
extern "C" void Goals_Serialize_f9240(void* selfV, void* stream, void* , int32_t* outParam);  // 0x1400f9240

// ---- keystone_a_gamedtor.cpp
extern "C" void Game_Dtor_9ed60(int64_t param_1);  // 0x14009ed60

// ---- mapquery.cpp
extern "C" float* Reimpl_MapGetTerrainPos(void* game, float* outPos3, float* inXY2, char flag, void* outTri);  // 0x14009f3e0

// ---- minimap_paint.cpp
extern "C" void Minimap_PaintTerrain(void* minimap, void* context, void* worldGeom, void* sunLight);  // 0x1400a5140

// ---- professionlist.cpp
extern "C" void ProfessionList_Startup_Impl(void* profList);  // 0x1400a90a0

// ---- serialize_arrays.cpp
extern "C" void Reimpl_Transform_SerializeWrapper(void* transform, void* stream);  // 0x1400988c0
extern "C" void Reimpl_Array_Resize_Stride20(void* arrayHeader, int newCount);  // 0x14001baf0
extern "C" void Reimpl_Array_Resize_Stride08(void* arrayHeader, int newCount);  // 0x140001bb0
extern "C" void Reimpl_Array_Resize_Stride14(void* arrayHeader, int newCount);  // 0x14001bf10
extern "C" void Reimpl_Array_Resize_Stride10_Spawn(void* arrayHeader, int newCount);  // 0x14000c520
extern "C" void Reimpl_Array_Serialize_Stride20(void* serializer, uint32_t count, void* arrayBase);  // 0x14001bbf0
extern "C" void Reimpl_Array_Serialize_External_GraphicsMesh(void* serializer, int count, void* arrayBase);  // 0x14001bcc0
extern "C" void Reimpl_Array_Serialize_External_Particle(void* serializer, int count, void* arrayBase);  // 0x14001bff0

// ---- serialize_externals.cpp
extern "C" void Reimpl_ExternalPtr_Serialize_GraphicsMesh(void* refField, void* serializer);  // 0x14001b8e0
extern "C" void Reimpl_ExternalPtr_Serialize_Weather(void* refField, void* serializer);  // 0x1400196a0
extern "C" void Reimpl_ExternalPtr_Serialize_StringTable(void* refField, void* serializer);  // 0x14000bf40, 0x14000b7b0, 0x14000bd30, 0x140002340, 0x140018020
extern "C" void Reimpl_ExternalPtr_Serialize_EnvironmentDesc(void* refField, void* serializer);  // 0x1400192a0

// ---- serialize_externals_array.cpp
extern "C" void Reimpl_ExternalPtrArray_Serialize(void* serializer, int count, long long arrayBase, void* );  // 0x1400049c0

// ---- serialize_externals_typed.cpp
extern "C" void Reimpl_ExternalPtr_Serialize_Typed_7140(void* refField, void* serializer);  // 0x140007140
extern "C" void Reimpl_ExternalPtr_Serialize_MapGenerator(void* refField, void* serializer);  // 0x140019ec0
extern "C" void Reimpl_ExternalPtr_Serialize_TerrainGenerator(void* refField, void* serializer);  // 0x14001a0c0
extern "C" void Reimpl_ExternalPtr_Serialize_ClimateGenerator(void* refField, void* serializer);  // 0x1400194a0

// ---- serialize_heightmap_mesh.cpp
extern "C" void* Reimpl_TradeWork_helper_e270(void* self, void* retbuf, int col, int row);  // 0x1400ae270

// ---- serialize_resize_helpers.cpp
extern "C" void Reimpl_Array_Resize_Stride02(void* arrayHeader, int newCount);  // 0x14001c890
extern "C" void Reimpl_Array_Resize_Stride16B(void* arrayHeader, int newCount);  // 0x14001ef10
extern "C" void Reimpl_Array_Resize_Stride04(void* arrayHeader, int newCount);  // 0x1400021d0
extern "C" void Reimpl_Array_Resize_Byte(void* arrayHeader, int newCount);  // 0x14000a590

// ---- serialize_struct60b.cpp
extern "C" void Reimpl_Struct60B_Resize(void* arrayHeader, unsigned int newCount);  // 0x14001f250
extern "C" void* Reimpl_Struct60B_CopyCtor(void* dst, const void* src);  // 0x14001f5e0
extern "C" void Reimpl_Struct60B_DtorRange(void* unused, void* first, int count);  // 0x14001fb80
extern "C" void* Reimpl_Struct60B_SubArrayCopyStride14(void* dst, const void* src);  // 0x14001fdb0
extern "C" void* Reimpl_Struct60B_SubArrayCopyStride02(void* dst, const void* src);  // 0x14001fe60

// ---- town_workplace_registry.cpp
extern "C" void ResidenceComponent_impl_95a0(int64_t param_1, void* param_2, uint64_t param_3);  // 0x1400895a0
extern "C" void TownRegistry_ShuffleProfessionList(int64_t townMgr, uint64_t professionKey);  // 0x140089470
extern "C" void* FindAvailableBuilding(int64_t townMgr, void* out, uint64_t professionKey);  // 0x14008d100

// ---- unreg_lane5_musicplayer.cpp
extern "C" void MusicPlayer_Update(void* musicPlayer);  // 0x1400a40e0

// ---- w2_takescreenshot.cpp
extern "C" void GameUpdateLoop_TakeScreenshot(void* self);  // 0x140095da0

// ---- weather_particles.cpp
extern "C" void* WeatherParticle_GeoCtor(void* mem);  // 0x1400d3f10
extern "C" void WeatherParticle_Config(void* geo, void* descriptor);  // 0x1400d3f80
extern "C" void Reimpl_WeatherArray_Resize(void* hdr, int newCount);  // 0x1400198a0

// ---- weather_sim.cpp
extern "C" void Weather_ResetTime_Reimpl(void* weatherSelf, void* time);  // 0x1400944e0

// ---- worldsetup_reload.cpp
extern "C" void WorldSetup_LoadFromSave(void* worldSetup, void* savedState, void* scenarioIdx, void* retvalIbpPtr);  // 0x1400b49f0
