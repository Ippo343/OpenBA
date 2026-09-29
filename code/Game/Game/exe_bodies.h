#pragma once
// code/Game/Game/exe_bodies.h -- declarations of the exe bodies DEFINED in
// code/Game/Game/ (ENDGAME B.3R, block W0-P0c, B3R_RULES R5).
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
// header is gone since B3R W2-EZ: its includers include this one directly).
//
// Declared in another header already (not redeclared here):
//   Game::RegisterDefaultInputBindings     0x1400992f0  (code/Game/Game/inputbindings.h)
//
// Internal linkage in the defining file (static / anonymous namespace):
// not declared -- that would change linkage, which this step does not do.
//   ResizeStride0x10                       0x140017640  (citizenlist_serialize.cpp:339, anonymous namespace)
//   ResizeStride0x20                       0x1400177c0  (citizenlist_serialize.cpp:350, anonymous namespace)

#include <cstdint>
// The inline bodies' own needs (they reached these through engine/funcs/engine_funcs.h
// until B3R W2-EZ deleted it):
#include "options.h"            // Opt() (GetLoadingSoundString)
#include "system/String.h"      // System::String::Format
#include "system/Value.h"       // System::Random
#include "ui/UserInterface.h"   // UI::UserInterface::SetCursor / SetFullscreenFade
#include "object/External.h"    // Object::ExternalPtr<UI::Cursor>
#include "Game/game.h"   // B3R STORAGE: g_gameContainer
#include "Game/GameSettings.h"   // g_gameSettings's struct
class Minimap;             // Game/Minimap.h (Game_ImageCaptureDtor)
class WeatherSimulation;   // Game/WeatherSimulation.h (Game_WeatherDtor)
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree (B3R Leftovers)

// ---- citizen_names.cpp
extern "C" void* NameDB_GenerateName(void* retStr, void* db, void* rng, int minLen, int maxLen);  // 0x1400a4b30
extern "C" void* NameDB_PickAndGenerate(void* retStr, void* rng, char female);  // 0x1400a4c60

// ---- citizenlist_serialize.cpp

// ---- eventlog.cpp
extern "C" void EventLog_AddEvent(void* eventLog, int eventType, int severity, void* entityIBP);  // 0x140095580 B4 FORWARD -> EventLog::AddEvent
extern "C" void EventLog_AddCitizenEvent(void* eventLog, int eventType, int severity, void* entityIBP, void* nameStr, void* titleStr, void* extraStr);  // 0x140094e00 B4 FORWARD -> EventLog::AddCitizenEvent

// ---- game.cpp
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
void Game_SpawnResources_Impl(void* citizenListPtr, uint32_t resourceFlag, int count);

// ---- game_leaf_helpers.cpp
void Game_DestroyStringArray18(void* arrayHeader);  // 0x14001b030
void Game_EventNotifierCleanup(void* arrayHeader);  // 0x14000a8a0
void Game_MusicPlayerDtor(void* self);  // 0x140018220
void Game_ImageCaptureDtor(Minimap* self);  // 0x1400a4fc0
void Game_WeatherDtor(WeatherSimulation* self);  // 0x140093eb0

// ---- game_settings_persist.cpp
extern "C" void GameSettings_Load(GameSettings* settings);  // 0x140098b60
extern "C" void GameSettings_Save(GameSettings* settings);  // 0x140098970

// ---- game_main.cpp
// The exe cell 0x1401c3de0 (B3R STORAGE): the 0x34-byte settings block GameMain
// allocates and stores there (0x14004202e), a `GameSettings` (Game/GameSettings.h).
// A namespace-scope pointer; was the file-static g_storage_GameSettingsPtr of
// standalone/address_registry.cpp, read through g_pGameSettingsPtr /
// g_pNewGameSettings. Null outside RunGameMain's settings lifetime.
extern GameSettings* g_gameSettings;

// ---- gameupdateloop.cpp
extern "C" float CalcWindFactor_Reg(void* time);  // 0x1401027c0

// ---- gameupdateloop_load.cpp

// ---- gameupdateloop_serialize.cpp
struct UpdateBucket;   // Entity/EntityList.h
namespace IO { class Serializer; }
extern "C" void BucketTree_Serialize_21ba0(IO::Serializer* serializer, void* , UpdateBucket* buckets);  // 0x140021ba0

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

// ---- worldgen_f390_shims.cpp
// System::Random::RandFloatTol(this) -> float in [-1.0, 1.0). Defined as an
// extern "C" link shim in code/Game/Game/worldgen_f390_shims.cpp (lazily resolves
// the Runtime export "?RandFloatTol@Random@System@@QEAAMXZ"); declared there and
// reused by callers that need the tolerance-range primitive.
extern "C" float fn_Random_RandFloatTol(void* self);

// ---- worldsetup_reload.cpp
extern "C" void WorldSetup_LoadFromSave(void* worldSetup, void* savedState, void* scenarioIdx, void* retvalIbpPtr);  // 0x1400b49f0

// ---- inline bodies (moved from engine/funcs/engine_funcs.h; no .cpp)

// SetEngineNextLoop — sets next loop on the MainLoop engine singleton
// FUN_140097fb0(enginePtr, loaderLoop): sets UI cursor, fullscreen fade, stores next loop ptr
// Called when self->m_loaderLoop1 (+0xE0) is null (use engine at +0xE8).
// REPLACED: was fn_SetEngineNextLoop (ResolveAddr(Module::Exe, 0x140097fb0))
// Now inline C++ — calls runtime UI functions (SetCursor, SetFullscreenFade).
inline void SetEngineNextLoop(uintptr_t enginePtr, void* loaderLoop) {
    void* ui = *(void**)(enginePtr + 0x98);
    // Set cursor to default (cursor ptr from KeyMap singleton+0x38)
    uint64_t cursorVal = *(uint64_t*)((uintptr_t)(g_gameContainer) + 0x38);
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
    // ??0Random@System@@QEAA@XZ 0x1800c7f20, called by name (B3R Leftovers).
    RtSystem_Random_Ctor(reinterpret_cast<System::Random*>(rng));
    // B.3.4b bridge: the uint overload ?Rand@Random@System@@QEAAII@Z (exe slot
    // 0x1401512e8, Runtime 0x1800c8040) — `12u` picks it over Rand() and
    // Rand(int,int).
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

// ---- citizenlist_serialize.cpp (B3R G-Game: external linkage given; read by citizenbehavior.cpp)
void ResizeStride0x10(void* hdr, int n);  // 0x140017640
void ResizeStride0x20(void* hdr, int n);  // 0x1400177c0

// ---- B3R G-Game: bodies of this module called directly (formerly through local extern
// prototypes in geometry_serialize.cpp / worldgen.cpp); not registered.
// serialize_struct60b_serialize.cpp
extern "C" void Reimpl_Struct60B_Serialize(void* serializer, int count, void* arrayBase);
extern "C" void Reimpl_Array_Serialize_External_MaterialInstance(void* serializer, int count, void* arrayBase);
// serialize_heightmap_mesh.cpp
extern "C" void Reimpl_TradeWork_sub_c1e0(void* self);
// serialize_image_upload.cpp
extern "C" void Reimpl_TradeWork_sub_c780(void* self);
// serialize_resize_helpers.cpp
extern "C" void Reimpl_Array_Resize_Stride16A(void* arrayHeader, int newCount);
extern "C" void Reimpl_Array_Resize_ArrayOfArrays(void* arrayHeader, int newCount);
