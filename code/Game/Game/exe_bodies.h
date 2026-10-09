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
//   (ResizeStride0x10 0x140017640 is CitizenIntAssocArray::SetSize since B6 row 15)

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
class Minimap;             // Game/Minimap.h (Minimap_PaintTerrain)
class WeatherSimulation;   // Game/WeatherSimulation.h (Game_WeatherDtor)
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree (B3R Leftovers)

// ---- citizen_names.cpp
class NameDatabase;
extern "C" System::String* NameDB_GenerateName(System::String* retStr, const NameDatabase* db, System::Random* rng, int minLen, int maxLen);  // 0x1400a4b30
extern "C" System::String* NameDB_PickAndGenerate(System::String* retStr, System::Random* rng, char female);  // 0x1400a4c60

// ---- citizenlist_serialize.cpp

// ---- eventlog.cpp
struct EventLogEntry;   // Game/EventLog.h

class EventLog; struct EventLogEntry; class GameUpdateLoop;
namespace Object { class InternalBasePtr; }
// The two below keep their null-log guard (not pure forwarders).
extern "C" void EventLog_AddEvent(EventLog* eventLog, int eventType, int severity, const Object::InternalBasePtr* entityIBP);  // 0x140095580 -> EventLog::AddEvent
extern "C" void EventLog_AddCitizenEvent(EventLog* eventLog, int eventType, int severity, const Object::InternalBasePtr* entityIBP, const System::String* nameStr, const System::String* titleStr, const System::String* extraStr);  // 0x140094e00 -> EventLog::AddCitizenEvent (nameStr: &CitizenComponent::name)
// The shared leaves the crop-infestation writer (diseasecomponent.cpp) uses.

extern "C" void EventLog_YearString_Reg(GameUpdateLoop* gameWorld, System::String* retStr);  // retail inlines it
extern "C" void EventLog_SeasonString_Reg(GameUpdateLoop* gameWorld, const EventLog* eventLog, System::String* retStr);
extern "C" int  EventLog_TileIndex_Reg(void* unused, const Object::InternalBasePtr* entityIBP);  // 0x140095030 (RCX dead)

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
class CitizenBehavior;
void Game_SpawnResources_Impl(CitizenBehavior* citizenList, uint32_t resourceFlag, int count);

// ---- game_leaf_helpers.cpp
class CharacterGrid;   // Component/CharacterGrid.h
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

// ---- gameupdateloop_load.cpp

// ---- gameupdateloop_serialize.cpp
#include "Entity/EntityList.h"   // UpdateBucket (an alias of Collection::Array since B.4 X4-COMP)
namespace IO { class Serializer; }
extern "C" void BucketTree_Serialize_21ba0(IO::Serializer* serializer, void* , UpdateBucket* buckets);  // 0x140021ba0

// ---- gameupdateloop_teardown.cpp
struct EntityList; class CitizenBehavior; class PlayerToolManager; class WorkScheduler;
extern "C" void EntityList_Shutdown_c3d30(EntityList* self);  // 0x1400c3d30
extern "C" void EntityUI_Dtor_1024d0(PlayerToolManager* self);  // 0x1401024d0
extern "C" void Tutorial_Dtor_f8ff0(WorkScheduler* self);  // 0x1400f8ff0

// ---- geometry_serialize.cpp
class WorldGeometry;
namespace IO { class Stream; }
extern "C" void Reimpl_TradeWork_impl_04d0(WorldGeometry* self, IO::Stream* stream);  // 0x1400b04d0 WorldGeometry::Serialize  // B4 FORWARD: 1 outside caller(s)

// ---- goals_serialize.cpp
class WorkScheduler;
namespace IO { class Stream; }
extern "C" void Goals_Serialize_f9240(WorkScheduler* self, IO::Stream* stream, void* , int32_t* outParam);  // 0x1400f9240  // B4 FORWARD: 1 outside caller(s)

// ---- keystone_a_gamedtor.cpp
class Map;

// ---- mapquery.cpp
extern "C" float* Reimpl_MapGetTerrainPos(Map* game, float* outPos3, const float* inXY2, char flag, float* outTri);  // 0x14009f3e0 (outTri: a Vector3, 0x1400aa810 writes three floats)

// ---- minimap_paint.cpp
class WorldGeometry;
namespace Video { class Context; }
namespace Graphics { class SunLight; }
extern "C" void Minimap_PaintTerrain(Minimap* minimap, Video::Context* context, WorldGeometry* worldGeom, Graphics::SunLight* sunLight);  // 0x1400a5140  // B4 FORWARD: 1 outside caller(s)

// ---- professionlist.cpp
class ProfessionList;
extern "C" void ProfessionList_Startup_Impl(ProfessionList* profList);  // 0x1400a90a0  // B4 FORWARD: 1 outside caller(s)

// ---- serialize_arrays.cpp
class PlayerController;
extern "C" void Reimpl_Transform_SerializeWrapper(PlayerController* controller, IO::Stream* stream);  // 0x1400988c0  // B4 FORWARD: 1 outside caller(s)
extern "C" void Reimpl_Array_Resize_Stride08(void* arrayHeader, int newCount);  // 0x140001bb0
// 0x14000c520 Reimpl_Array_Resize_Stride10_Spawn: declared in Component/SpawnDescription.h (it takes the typed entry array).
struct MenuSceneMesh;
namespace IO { class Serializer; }
extern "C" void Reimpl_Array_Serialize_Stride20(IO::Serializer* serializer, uint32_t count, MenuSceneMesh* meshes);  // 0x14001bbf0
extern "C" void Reimpl_Array_Serialize_External_GraphicsMesh(IO::Serializer* serializer, int count, void* arrayBase);  // 0x14001bcc0
extern "C" void Reimpl_Array_Serialize_External_Particle(IO::Serializer* serializer, int count, void* arrayBase);  // 0x14001bff0
extern "C" void Reimpl_Array_Serialize_External_EdgeMesh(IO::Serializer* serializer, int count, void* arrayBase);  // 0x140007580
extern "C" void Reimpl_Array_Serialize_External_MaterialInstance(IO::Serializer* serializer, int count, void* arrayBase);  // 0x140020150

// ---- serialize_externals.cpp
// ExternalPtr<T>::Serialize, one retail instantiation each (ICF serves several T
// per address). B4 X7-GAME-A: the cores take the slot as the External* retail
// serializes; the callers' typed wrappers below hand them a T* field's slot.
namespace Object { class External; }
namespace IO { class Serializer; }
void ExternalPtrSerialize_GraphicsMesh(::Object::External*& ref, IO::Serializer& serializer);     // 0x14001b8e0
void ExternalPtrSerialize_Weather(::Object::External*& ref, IO::Serializer& serializer);          // 0x1400196a0
void ExternalPtrSerialize_StringTable(::Object::External*& ref, IO::Serializer& serializer);      // 0x14000bf40 (B7 LX: that instantiation only)
void ExternalPtrSerialize_ComponentDescription(::Object::External*& ref, IO::Serializer& serializer);  // 0x140002340
void ExternalPtrSerialize_Profession(::Object::External*& ref, IO::Serializer& serializer);  // 0x140018020
void ExternalPtrSerialize_SpriteSheet(::Object::External*& ref, IO::Serializer& serializer);  // 0x14000bd30
void ExternalPtrSerialize_PickingMesh(::Object::External*& ref, IO::Serializer& serializer);  // 0x14000b7b0
void ExternalPtrSerialize_SoundEffect(::Object::External*& ref, IO::Serializer& serializer);  // 0x1400019a0
void ExternalPtrSerialize_PointList(::Object::External*& ref, IO::Serializer& serializer);  // 0x14000ab20
void ExternalPtrSerialize_Particle(::Object::External*& ref, IO::Serializer& serializer);  // 0x14000a910
void ExternalPtrSerialize_AnimationGroup(::Object::External*& ref, IO::Serializer& serializer);  // 0x140035b40
void ExternalPtrSerialize_Dialog(::Object::External*& ref, IO::Serializer& serializer);  // 0x140028650
void ExternalPtrSerialize_ElementDescription(::Object::External*& ref, IO::Serializer& serializer);  // 0x140027ad0
void ExternalPtrSerialize_DialogControllerConfig(::Object::External*& ref, IO::Serializer& serializer);  // 0x140039840
void ExternalPtrSerialize_Toolbar(::Object::External*& ref, IO::Serializer& serializer);
void ExternalPtrSerialize_EnvironmentDesc(::Object::External*& ref, IO::Serializer& serializer);  // 0x1400192a0
void ExternalPtrSerialize_Typed_7140(::Object::External*& ref, IO::Serializer& serializer);       // 0x140007140
void ExternalPtrSerialize_MapGenerator(::Object::External*& ref, IO::Serializer& serializer);     // 0x140019ec0
void ExternalPtrSerialize_TerrainGenerator(::Object::External*& ref, IO::Serializer& serializer); // 0x14001a0c0
void ExternalPtrSerialize_ClimateGenerator(::Object::External*& ref, IO::Serializer& serializer); // 0x1400194a0
// The field is an ExternalPtr<T> held as `T*` (or as the External* / void* / uint64_t its
// header still declares): its eight bytes ARE the External* the core serializes.
// The null test is the old C entry points' (`if (!refField || !serializer) return`).
template <void (*Core)(::Object::External*&, IO::Serializer&), class T>
inline void ExternalPtrSerializeSlot(T* refField, IO::Serializer* serializer) {
    static_assert(sizeof(T) == 8, "an ExternalPtr slot is the eight bytes of an External*");
    if (!refField || !serializer) return;
    Core(*reinterpret_cast<::Object::External**>(refField), *serializer);   // an ExternalPtr<T> slot held as T* handed to the one erased External*& core (a T* slot is not an External*; retail shares the body across T by ICF)  // CAST-OK: elem_stride
}
template <class T> inline void Reimpl_ExternalPtr_Serialize_GraphicsMesh(T* f, IO::Serializer* s)     { ExternalPtrSerializeSlot<&ExternalPtrSerialize_GraphicsMesh>(f, s); }
template <class T> inline void Reimpl_ExternalPtr_Serialize_Weather(T* f, IO::Serializer* s)          { ExternalPtrSerializeSlot<&ExternalPtrSerialize_Weather>(f, s); }
template <class T> inline void Reimpl_ExternalPtr_Serialize_StringTable(T* f, IO::Serializer* s)      { ExternalPtrSerializeSlot<&ExternalPtrSerialize_StringTable>(f, s); }
template <class T> inline void Reimpl_ExternalPtr_Serialize_Toolbar(T* f, IO::Serializer* s)          { ExternalPtrSerializeSlot<&ExternalPtrSerialize_Toolbar>(f, s); }
template <class T> inline void Reimpl_ExternalPtr_Serialize_EnvironmentDesc(T* f, IO::Serializer* s)  { ExternalPtrSerializeSlot<&ExternalPtrSerialize_EnvironmentDesc>(f, s); }
template <class T> inline void Reimpl_ExternalPtr_Serialize_Typed_7140(T* f, IO::Serializer* s)       { ExternalPtrSerializeSlot<&ExternalPtrSerialize_Typed_7140>(f, s); }
template <class T> inline void Reimpl_ExternalPtr_Serialize_MapGenerator(T* f, IO::Serializer* s)     { ExternalPtrSerializeSlot<&ExternalPtrSerialize_MapGenerator>(f, s); }
template <class T> inline void Reimpl_ExternalPtr_Serialize_TerrainGenerator(T* f, IO::Serializer* s) { ExternalPtrSerializeSlot<&ExternalPtrSerialize_TerrainGenerator>(f, s); }
template <class T> inline void Reimpl_ExternalPtr_Serialize_ClimateGenerator(T* f, IO::Serializer* s) { ExternalPtrSerializeSlot<&ExternalPtrSerialize_ClimateGenerator>(f, s); }
template <class T> inline void Reimpl_ExternalPtr_Serialize_ComponentDescription(T* f, IO::Serializer* s) { ExternalPtrSerializeSlot<&ExternalPtrSerialize_ComponentDescription>(f, s); }
template <class T> inline void Reimpl_ExternalPtr_Serialize_Profession(T* f, IO::Serializer* s) { ExternalPtrSerializeSlot<&ExternalPtrSerialize_Profession>(f, s); }
template <class T> inline void Reimpl_ExternalPtr_Serialize_SpriteSheet(T* f, IO::Serializer* s) { ExternalPtrSerializeSlot<&ExternalPtrSerialize_SpriteSheet>(f, s); }
template <class T> inline void Reimpl_ExternalPtr_Serialize_PickingMesh(T* f, IO::Serializer* s) { ExternalPtrSerializeSlot<&ExternalPtrSerialize_PickingMesh>(f, s); }
template <class T> inline void Reimpl_ExternalPtr_Serialize_SoundEffect(T* f, IO::Serializer* s) { ExternalPtrSerializeSlot<&ExternalPtrSerialize_SoundEffect>(f, s); }
template <class T> inline void Reimpl_ExternalPtr_Serialize_PointList(T* f, IO::Serializer* s) { ExternalPtrSerializeSlot<&ExternalPtrSerialize_PointList>(f, s); }
template <class T> inline void Reimpl_ExternalPtr_Serialize_Particle(T* f, IO::Serializer* s) { ExternalPtrSerializeSlot<&ExternalPtrSerialize_Particle>(f, s); }
template <class T> inline void Reimpl_ExternalPtr_Serialize_AnimationGroup(T* f, IO::Serializer* s) { ExternalPtrSerializeSlot<&ExternalPtrSerialize_AnimationGroup>(f, s); }
template <class T> inline void Reimpl_ExternalPtr_Serialize_Dialog(T* f, IO::Serializer* s) { ExternalPtrSerializeSlot<&ExternalPtrSerialize_Dialog>(f, s); }
template <class T> inline void Reimpl_ExternalPtr_Serialize_ElementDescription(T* f, IO::Serializer* s) { ExternalPtrSerializeSlot<&ExternalPtrSerialize_ElementDescription>(f, s); }
template <class T> inline void Reimpl_ExternalPtr_Serialize_DialogControllerConfig(T* f, IO::Serializer* s) { ExternalPtrSerializeSlot<&ExternalPtrSerialize_DialogControllerConfig>(f, s); }

// ---- serialize_externals_array.cpp
namespace IO { class Serializer; }
class ComponentDescription;
extern "C" void Reimpl_ExternalPtrArray_Serialize(IO::Serializer* serializer, int count, ComponentDescription** arrayBase, void* );  // 0x1400049c0

// ---- serialize_externals_typed.cpp
// (the 0x140007140 family's cores are declared with serialize_externals.cpp's above)

// ---- serialize_heightmap_mesh.cpp
extern "C" void* Reimpl_TradeWork_helper_e270(const WorldGeometry* self, float* out, int col, int row);  // 0x1400ae270 WorldGeometry::TerrainNormal  // B4 FORWARD: 1 outside caller(s)

// ---- serialize_resize_helpers.cpp
extern "C" void Reimpl_Array_Resize_Stride02(void* arrayHeader, int newCount);  // 0x14001c890
extern "C" void Reimpl_Array_Resize_Stride16B(void* arrayHeader, int newCount);  // 0x14001ef10
extern "C" void Reimpl_Array_Resize_Stride04(void* arrayHeader, int newCount);  // 0x1400021d0
extern "C" void Reimpl_Array_Resize_Byte(void* arrayHeader, int newCount);  // 0x14000a590

// ---- serialize_struct60b.cpp: GeometryBatch's members and GeometryBatch_Resize
// (0x14001f250 / 0x14001f5e0) are declared in Game/WorldGeometryElements.h.
template <class T> struct RawArray;   // Action/PathSearch.h

// ---- town_storage_seed.cpp
class Component; class MapGenerator;
extern "C" void TownRegistry_RegisterStorageLocation(const Component* storageLocationComp);  // 0x14012d230
extern "C" void TownStorage_BuildCompleteInit_7ff0(Component* componentWithEntity);  // 0x140107ff0
extern "C" void StartTown_SeedStartingResources(MapGenerator* startConfig);  // 0x1400a1f00

// ---- town_workplace_registry.cpp
class CitizenBehavior; class ComponentEntity; class Profession;
extern "C" void ResidenceComponent_impl_95a0(CitizenBehavior* town, ComponentEntity** workplaceRef, const Profession* profession);  // 0x1400895a0
extern "C" void TownRegistry_ShuffleProfessionList(CitizenBehavior* townMgr, const Profession* professionKey);  // 0x140089470
extern "C" void* FindAvailableBuilding(CitizenBehavior* townMgr, void* out, const Profession* professionKey);  // 0x14008d100
class WorkPlaceComponent;
extern "C" void TownRegistry_RegisterWorkplace(CitizenBehavior* townMgr, const WorkPlaceComponent* workplaceComp);

// ---- unreg_lane5_musicplayer.cpp
class MusicPlayer;   // Game/MusicPlayer.h
extern "C" void MusicPlayer_Update(MusicPlayer* musicPlayer);  // 0x1400a40e0  B4 FORWARD -> MusicPlayer::Update (1 outside caller: gameupdateloop.cpp)

// ---- w2_takescreenshot.cpp

// ---- weather_particles.cpp
extern "C" void* WeatherParticle_GeoCtor(void* mem);  // 0x1400d3f10
class WeatherParticleGeometry;
class Weather;
extern "C" void WeatherParticle_Config(WeatherParticleGeometry* geo, Weather* descriptor);  // 0x1400d3f80
// 0x1400198a0 Reimpl_WeatherArray_Resize: declared in Component/EnvironmentDescription.h (it takes the typed array).

// ---- weather_sim.cpp
class WeatherSimulation; namespace System { class Time; }
extern "C" void Weather_ResetTime_Reimpl(WeatherSimulation* weatherSelf, const System::Time* time);  // 0x1400944e0 -> WeatherSimulation::Update (B4 FORWARD)

// ---- worldgen_f390_shims.cpp
// System::Random::RandFloatTol(this) -> float in [-1.0, 1.0). Defined as an
// extern "C" link shim in code/Game/Game/worldgen_f390_shims.cpp (lazily resolves
// the Runtime export "?RandFloatTol@Random@System@@QEAAMXZ"); declared there and
// reused by callers that need the tolerance-range primitive.
namespace System { class Random; }
extern "C" float fn_Random_RandFloatTol(System::Random* self);

// ---- worldsetup_reload.cpp
class MainMenuDialog; namespace Save { class SavedState; }
extern "C" void WorldSetup_LoadFromSave(MainMenuDialog* worldSetup, Save::SavedState* savedState, int32_t scenarioIdx, const Object::InternalBasePtr* retvalIbpPtr);  // 0x1400b49f0

// ---- inline bodies (moved from engine/funcs/engine_funcs.h; no .cpp)

// SetEngineNextLoop — FUN_140097fb0(game, loaderLoop): the running game's UI
// (GameUpdateLoop::m_userInterface +0x98) gets the hourglass cursor and a fade to
// black, then the loop is queued in GameUpdateLoop::m_nextLoop (+0xC8).
// SetDialogLoaderNextLoop — FUN_14009a8f0(menuLoop, loaderLoop): the menu loop's UI
// (DialogLoopT::GetUserInterface, +0x40) fades to black, then the loop is queued in
// MenuDialogLoopT::_pendingLoop (+0xb68).
// Both are defined in Game/worldsetup_reload.cpp (they were inline here, reached
// through uintptr_t offsets).
class GameUpdateLoop;
class MenuDialogLoopT;
namespace Engine { class UpdateLoop; }
void SetEngineNextLoop(GameUpdateLoop* game, Engine::UpdateLoop* loaderLoop);            // 0x140097fb0
void SetDialogLoaderNextLoop(MenuDialogLoopT* menuLoop, Engine::UpdateLoop* loaderLoop);  // 0x14009a8f0

// GetLoadingSoundString — FUN_140099130 — NOW REIMPLEMENTED INLINE
// Creates a Random, generates rand 0-11, formats "Dialog/Loading.rsc:gameLoad%i".
// Old typedef kept for reference:
// typedef void* (*fn_GetLoadingSoundString_t)(uint64_t unused, void* retStr,
//                                              uint64_t unused2, uint64_t unused3);
// extern fn_GetLoadingSoundString_t fn_GetLoadingSoundString;
inline System::String* GetLoadingSoundString(System::String* retStr) {
    // 0x140099151: the 0x9c4-byte Random at [RSP+0x30] of a 0xa00 frame, built by
    // ??0Random@System@@QEAA@XZ 0x1800c7f20 (CALL [0x140152b50]); no dtor.
    System::Random random;
    // the uint overload ?Rand@Random@System@@QEAAII@Z (exe slot
    // 0x1401512e8, Runtime 0x1800c8040) — `12u` picks it over Rand() and
    // Rand(int,int).
    uint32_t variant = random.Rand(12u);
    // BANISHED_MENU_SCENE pins every random front-end pick, this one included:
    // the loading image it selects stays resident and is numbered into the
    // save's reference table, so an unpinned pick shifts every reference id
    // after "Dialog\Loading.rsc:gameLoadN" (tools/resave_gate/).
    if (Opt().menuScene >= 0)
        variant = static_cast<unsigned>(Opt().menuScene) % 12u;
    // ?Format@String@System@@SA?AV12@PEBDZZ  retail 0x1800c9730
    *retStr = System::String::Format("Dialog/Loading.rsc:gameLoad%i", variant);
    return retStr;
}

// ---- citizenlist_serialize.cpp (B3R G-Game: external linkage given; read by citizenbehavior.cpp)

// ---- B3R G-Game: bodies of this module called directly (formerly through local extern
// prototypes in geometry_serialize.cpp / worldgen.cpp); not registered.
// serialize_struct60b_serialize.cpp
// serialize_heightmap_mesh.cpp
// serialize_image_upload.cpp
// serialize_resize_helpers.cpp
extern "C" void Reimpl_Array_Resize_Stride16A(void* arrayHeader, int newCount);
namespace Math { class Vector2; }
extern "C" void Reimpl_Array_Resize_ArrayOfArrays(RawArray<RawArray<Math::Vector2>>* arrayHeader, int newCount);  // WorldGeometry +0x1E0
