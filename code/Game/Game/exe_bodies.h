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
extern "C" System::String* NameDB_GenerateName(System::String* retStr, const NameDatabase* db, System::Random* rng, int minLen, int maxLen);  // 0x1400a4b30  // B8: stays free -- free in retail: RCX is the String return slot (0x1400a4c25 MOV RCX,R12 before the String ctor), RDX the NameDatabase; an MSVC member's RCX is this (and the port's db null test has no this to sit on)
extern "C" System::String* NameDB_PickAndGenerate(System::String* retStr, System::Random* rng, char female);  // 0x1400a4c60  // B8: stays free -- free in retail: no NameDatabase argument (RCX the String return slot, RDX the Random, R8B the flag; it loads Female / Male.ndb itself, 0x1400a4c86)

// ---- citizenlist_serialize.cpp

// ---- eventlog.cpp
struct EventLogEntry;   // Game/EventLog.h

class EventLog; struct EventLogEntry; class GameUpdateLoop;
namespace Object { class InternalBasePtr; }
// The shared leaves the crop-infestation writer (diseasecomponent.cpp) uses.

extern "C" void EventLog_YearString_Reg(GameUpdateLoop* gameWorld, System::String* retStr);  // retail inlines it  // B8: stays free -- no retail body: retail inlines the year string (AddCitizenEvent 0x140094e00, DiseaseComponent::Infect 0x1400483a0); the export of eventlog.cpp's static EventLog_YearString for one caller, the pair of EventLog_SeasonString_Reg (MISC's ruling)
extern "C" void EventLog_SeasonString_Reg(GameUpdateLoop* gameWorld, const EventLog* eventLog, System::String* retStr);
extern "C" int  EventLog_TileIndex_Reg(void* unused, const Object::InternalBasePtr* entityIBP);  // 0x140095030 (RCX dead)  // B8: free in retail -- RCX dead: at 0x1400484b0 it is what the import call [0x140151ee8] before it left

// ---- game.cpp
// --- Game_TriggerDisasterByType (0x140060110) — REIMPLEMENTED inline ---
// Iterates ExternalFactory for entities with ComponentDescription (DAT_1401c7318),
// finds SelectDescription sub-component (DAT_1401c48a8), checks type match,
// removes matching entity via Game_RemoveEntity.
void Game_TriggerDisasterByType_Impl(int disasterType);  // B8: free in retail -- first parameter the disaster type (an int), no object

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

// ---- game_leaf_helpers.cpp
class CharacterGrid;   // Component/CharacterGrid.h

// ---- game_settings_persist.cpp

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
extern "C" void BucketTree_Serialize_21ba0(IO::Serializer* serializer, void* , UpdateBucket* buckets);  // 0x140021ba0  // B9: stays free -- the template is real, its element step is missing (B9 RT2 reading, row 1230): 0x1400336c0, 0x14001bbf0, 0x140021ba0, 0x14001bcc0 / 0x14001bff0 and the DLL's ExternalPtr array bodies (0x18005d4a0) are ONE body `Serializer::Serialize<T>(int n, T* d, const char* name)`: `TEST EDX,EDX / JLE <ret>`, then per element T's own serialize inlined; R9 (name) is never read (0x14001bcc0 / 0x18005d4a0 only home it) and every inner call passes R8 = 0, so the step is `d[i].Serialize(*this)`. The tree has that member for ExternalPtr<T> only; here T = UpdateBucket (Collection::Array<InternalPtr<ComponentEntity>>), n = 5 const-propagated (MOV EBP,5 at 0x140021bc2; EDX homed unread), and the element step is an inlined `Array<T>::Serialize(Serializer&)`: count -> Serialize(int&, 0) 0x140021be2, `if reading` SetSize 0x1400032e0, then the same template over InternalPtr<ComponentEntity> (`TEST RDI,RDI / JLE` 0x140021c05, IBP Serialize 0x140021c16 + IsDerivedFrom 0x1401c5d70). The tree has no Array<T>::Serialize (Collection.h hazard 6 is a DLL-row fact; the exe has one) (B9 RT2)

// ---- gameupdateloop_teardown.cpp
struct EntityList; class CitizenBehavior; class PlayerToolManager; class WorkScheduler;

// ---- geometry_serialize.cpp
class WorldGeometry;
namespace IO { class Stream; }

// ---- goals_serialize.cpp
class WorkScheduler;
namespace IO { class Stream; }

// ---- keystone_a_gamedtor.cpp
class Map;

// ---- mapquery.cpp

// ---- minimap_paint.cpp
class WorldGeometry;
namespace Video { class Context; }
namespace Graphics { class SunLight; }

// ---- professionlist.cpp
class ProfessionList;

// ---- serialize_arrays.cpp
class PlayerController;
extern "C" void Reimpl_Array_Resize_Stride08(void* arrayHeader, int newCount);  // 0x140001bb0  // B9: stays free -- erased core over holders that are not one Collection::Array: WorldGeometry::_batchMaterials (WorldGeometryArray), the +0x1E0 outlines and roadtool's +0x218 refs are RawArray-shaped headers, AmbientEmitterDescription::_emitters a raw {data, count, capacity} triple; MenuScene +0x40 / +0x60, Tutorial steps and TutorialStep's arrays are Collection::Array<T*> on the trait row, whose init is null (tail unwritten) where 0x140001bb0 zeroes it (`TEST RDX,RDX / MOV [RDX],R10`); moving needs those holders typed on a zero-tail row (kSpec_Ptr8Zero: plain qword copy as 0x140001c31, zero tail) -- header work outside RT's files; the port-only null test went in B9 RT
// 0x14000c520 Reimpl_Array_Resize_Stride10_Spawn: declared in Component/SpawnDescription.h (it takes the typed entry array).
struct MenuSceneMesh;
namespace IO { class Serializer; }
extern "C" void Reimpl_Array_Serialize_Stride20(IO::Serializer* serializer, uint32_t count, MenuSceneMesh* meshes);  // 0x14001bbf0  // B9: stays free -- the template is real, its element step is missing (B9 RT2 reading, row 1230): 0x1400336c0, 0x14001bbf0, 0x140021ba0, 0x14001bcc0 / 0x14001bff0 and the DLL's ExternalPtr array bodies (0x18005d4a0) are ONE body `Serializer::Serialize<T>(int n, T* d, const char* name)`: `TEST EDX,EDX / JLE <ret>`, then per element T's own serialize inlined; R9 (name) is never read (0x14001bcc0 / 0x18005d4a0 only home it) and every inner call passes R8 = 0, so the step is `d[i].Serialize(*this)`. The tree has that member for ExternalPtr<T> only; here T = MenuSceneMesh and MenuSceneMesh has no Serialize(IO::Serializer&) (MenuScene.h; its body: seven Serialize(float&) 0x14001bc1a..0x14001bc79, two Serialize(short&) 0x14001bc89 / 0x14001bc99) (B9 RT2)

// ---- serialize_heightmap_mesh.cpp

// ---- serialize_resize_helpers.cpp

// ---- serialize_struct60b.cpp: GeometryBatch's members and GeometryBatch_Resize
// (0x14001f250 / 0x14001f5e0) are declared in Game/WorldGeometryElements.h.
template <class T> struct RawArray;   // Action/PathSearch.h

// ---- town_storage_seed.cpp
class Component; class MapGenerator;

// ---- town_workplace_registry.cpp
class CitizenBehavior; class ComponentEntity; class Profession;
class WorkPlaceComponent;
extern "C" void TownRegistry_RegisterWorkplace(CitizenBehavior* townMgr, const WorkPlaceComponent* workplaceComp);  // B8: stays free -- harness / port-only: no retail body (the INVENTORY address 0x140089470 is the shuffle's); the tree's spawn-time registration wiring over CitizenBehavior::RegisterWorkplace 0x1400895a0 (buildcomponent_state1.cpp, starttown.cpp)

// ---- unreg_lane5_musicplayer.cpp
class MusicPlayer;   // Game/MusicPlayer.h

// ---- w2_takescreenshot.cpp

// ---- weather_particles.cpp
class WeatherParticleGeometry;
class Weather;
// 0x1400198a0 Reimpl_WeatherArray_Resize: declared in Component/EnvironmentDescription.h (it takes the typed array).

// ---- weather_sim.cpp
class WeatherSimulation; namespace System { class Time; }

// ---- worldgen_f390_shims.cpp

// ---- worldsetup_reload.cpp
class MainMenuDialog; namespace Save { class SavedState; }

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
