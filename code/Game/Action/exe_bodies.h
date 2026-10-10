// code/Game/Action/exe_bodies.h
//
// Declarations of the C bodies in code/Game/Action/ that were reached through the address
// registry (RegisterExeAddr rows, the readers' ResolveAddr(Module::Exe, ...) / helper /
// X_ResolveFuncs / engine_funcs doors), or through a local extern prototype in the caller.
// B3R_RULES R5: every DIRECT body is declared in exactly one header, written from its
// DEFINITION (linkage included), and each defining .cpp includes this header so the
// compiler checks the pair. Readers call these by name through this header (R2).
// Making them members of real classes is B.4's work; this header is where it finds them.
//
// Grouped by defining file; the retail address is the trailing comment on each line.
// `uint` in a definition (a file-local typedef there) is spelled `unsigned int` here.
// The address -> name table is docs/runtime_port/endgame_b/b3r_session/data/P0a_declared.tsv.
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include "collection/Collection.h"   // Collection::Array (QwordArraySetSize's typed entry points)
#include "math/Math.h"               // Math::Vector3 (Vector3Slot's member)

// ---- action_base_cnr.cpp
class Map;                    // Game/Map.h
class GrowthComponent;        // Component/GrowthComponent.h
class ModelComponent;         // Component/ModelComponent.h
class NaturalResourceComponent;   // Component/NaturalResourceComponent.h
class ComponentEntity;        // Entity/ComponentEntity.h
uint64_t CheckCellTerrain(const Map* map, int cellIndex);  // 0x14009cb00
void NaturalResourceSetAnimation(NaturalResourceComponent* self, uint32_t animIdx);  // 0x140126440
uint64_t NaturalResourcePlaceAtCell(NaturalResourceComponent* self, int cellIndex);  // 0x140126540
void RemoveNaturalResourceCore(NaturalResourceComponent* self, ComponentEntity* const* citizenIBP, char harvestFlag);  // 0x1401260f0

// ---- action_base_complete.cpp
class CitizenComponent;       // Component/CitizenComponent.h
class WorkScheduler;          // Game/WorkScheduler.h
bool TryCompleteTask(const WorkScheduler* sched, CitizenComponent* const* entityRef, int taskIdx, ComponentEntity* const* targetRef, uint32_t taskParam);  // 0x1400f8760
void TaskReassign(const WorkScheduler* sched, CitizenComponent* const* entityRef, int taskIdx, ComponentEntity* const* targetRef, uint32_t taskParam);  // 0x1400f8800
void TaskCleanup(WorkScheduler* sched, int taskIdx);  // 0x1400f8e30
void HideCitizenTool(CitizenComponent* citizen);  // 0x1401176e0
void ShowCitizenTool(CitizenComponent* citizen, int toolId);  // 0x1401173b0

// ---- action_base_hunt.cpp
class HuntComponent;          // Component/HuntComponent.h
class ComponentEntity;        // Entity/ComponentEntity.h
template <class T> struct RawArray;   // Action/PathSearch.h
void QwordArraySetSize(RawArray<void*>* arr, int newCount, void* const* fill);  // 0x140003110
// The typed entry points of the one erased core 0x140003110: every 8-byte-element
// Collection::Array<T*> (IBPArray, CitizenPtrList<T>, ...) or Array<InternalPtr<T>>
// (PastureComponent::animals, MultiObjectComponent::children) is handed to it.
namespace Object { template <class T> class InternalPtr; }
template <class E, class A, class S, class T>
inline void QwordArraySetSize(Collection::Array<E, A, S>* arr, int newCount, T* const* fill) {
    static_assert(std::is_same<E, T*>::value || std::is_same<E, Object::InternalPtr<T>>::value,
                  "an 8-byte T* / InternalPtr<T> slot array, filled with a T*");
    QwordArraySetSize(reinterpret_cast<RawArray<void*>*>(arr), newCount, reinterpret_cast<void* const*>(fill));  // CAST-OK: elem_stride
}
template <class E, class A, class S, class T>
inline void QwordArraySetSize(Collection::Array<E, A, S>* arr, int newCount, const Object::InternalPtr<T>* fill) {
    static_assert(std::is_same<E, T*>::value || std::is_same<E, Object::InternalPtr<T>>::value,
                  "an 8-byte T* / InternalPtr<T> slot array, filled with an InternalPtr<T>");
    QwordArraySetSize(reinterpret_cast<RawArray<void*>*>(arr), newCount, reinterpret_cast<void* const*>(fill));  // CAST-OK: elem_stride
}
void MarkEntityForRemoval(ComponentEntity* entity);  // 0x1400c2ea0

// ---- action_base_move.cpp
class CharacterComponent;     // Component/CharacterComponent.h
class CitizenComponent;       // Component/CitizenComponent.h
class ComponentEntity;        // Entity/ComponentEntity.h
namespace Math { class Vector3; }
CharacterComponent* FindCharacterComponent(const ComponentEntity* entity);  // 0x1400012d0
void SetFacingDirection(CharacterComponent* character, const Math::Vector3& dir);  // 0x14010a060
void SetAnimation(CharacterComponent* character, uint32_t animId, float p3, float p4);  // 0x14010a1f0
uint8_t CitizenBreakCheck(CitizenComponent* citizen);  // 0x140116650
class CharacterDescription;   // Component/CharacterDescription.h
class MoveTo;                 // Action/MoveTo.h
struct ActionStack;           // Action/Action.h
namespace IO { class Serializer; }
void ActMv_ExternalPtrSerialize_22970(CharacterDescription** slot, IO::Serializer* ser);  // 0x140022970
void ActMv_UpdateFacing_c8d20(MoveTo* self);  // 0x1400c8d20
bool ActMv_PathSetup_c8c10(MoveTo* self, const float* worldPos, void*, ActionStack* param4);  // 0x1400c8c10 (retail returns AL = 1)

// ---- action_base_movedeps.cpp
class ModelComponent;         // Component/ModelComponent.h
namespace System { class Time; class Random; }
bool BreakTimeCheck(System::Time* time, const float* pair, System::Random* rng);  // 0x1400a9290

// ---- action_base_rawmat.cpp
class AutoPickupComponent;    // Component/AutoPickupComponent.h
class CitizenBehavior;        // Entity/CitizenBehavior.h
class Component;              // Component/Component.h
class ComponentDescription;   // Component/ComponentDescription.h
class ComponentEntity;        // Entity/ComponentEntity.h
class StorageComponent;       // Component/StorageComponent.h
class TrackerComponent;       // Component/TrackerComponent.h
void AutoPickupAppend(AutoPickupComponent* self, uint32_t value);  // 0x140044810
int ResourceCategory(const ComponentDescription* material);  // 0x140088f90
void TrackerUpsert(TrackerComponent* tracker, ComponentDescription* material, int value);  // 0x140058fd0
void RegisterRawMaterial(CitizenBehavior* town, ComponentEntity* const* source, char placeNear, ComponentDescription* material, int quantity);  // 0x140088ef0
void PositionRawMaterial(Component* self, int cellIndex, float rotation);  // 0x140129760
// 0x14000da70 Array<T>::SetSize / 0x14000d9d0 Array<T>::operator= over a 16-byte
// {material, count, pad} element: ONE retail body each serving four element types
// (TrackerComponent::Entry, ConsumeRawMaterial::Entry, PickupStoredRawMaterial::Entry,
// MarketWork::Carry). The core works on its own element view; callers use the typed
// wrappers MaterialArraySetSize / MaterialArrayAssign over their {data, count, capacity}
// holder.
template <class T> struct RawArray;   // Action/PathSearch.h
struct MaterialCount16 {               // the core's own 16-byte element view
    ComponentDescription* material;    // +0x00 ExternalPtr<ComponentDescription>
    int32_t               count;       // +0x08
    int32_t               pad0C;       // +0x0C (the SetSize tail leaves it unwritten)
};
static_assert(sizeof(MaterialCount16) == 0x10, "16-byte stride (0x14000da70)");
static_assert(offsetof(MaterialCount16, material) == 0x0, "MaterialCount16::material");
static_assert(offsetof(MaterialCount16, count) == 0x8, "MaterialCount16::count");
static_assert(offsetof(MaterialCount16, pad0C) == 0xC, "MaterialCount16::pad0C");
void ActRm_ArrayResize16_da70(RawArray<MaterialCount16>* arr, int newCount);  // 0x14000da70
void ActRm_VectorClone_d9d0(RawArray<MaterialCount16>* dst, const RawArray<MaterialCount16>* src);  // 0x14000d9d0
template <class Holder>
inline void MaterialArraySetSize(Holder* arr, int newCount) {
    static_assert(sizeof(Holder) == 0x10, "an Array<16-byte element> {data, count, capacity}");
    ActRm_ArrayResize16_da70(reinterpret_cast<RawArray<MaterialCount16>*>(arr), newCount);  // CAST-OK: elem_stride
}
template <class Holder>
inline void MaterialArrayAssign(Holder* dst, const Holder* src) {
    static_assert(sizeof(Holder) == 0x10, "an Array<16-byte element> {data, count, capacity}");
    ActRm_VectorClone_d9d0(reinterpret_cast<RawArray<MaterialCount16>*>(dst),  // CAST-OK: elem_stride
                           reinterpret_cast<const RawArray<MaterialCount16>*>(src));  // CAST-OK: elem_stride
}

// ---- action_child_ctors.cpp
class ComponentEntity;        // Entity/ComponentEntity.h
float HeadingToward(const float* worldPos, ComponentEntity* const* entityIBP);  // 0x1400c29a0

// ---- action_child_ctors2.cpp
class ConsumeProduceComponent;   // Component/ConsumeProduceComponent.h
uint64_t PickConsumeRecipe(const ConsumeProduceComponent* self);  // 0x140119470

// ---- action_pf_astar.cpp

// ---- action_pf_avl.cpp
struct PathGraph;
extern "C" void PathPairs_Clear_25b30(PathGraph* pg);  // 0x140025b30

// ---- action_pf_ctors.cpp
struct ActionStack;
class Component;              // Component/Component.h
class ComponentDescription;   // Component/ComponentDescription.h
class ComponentEntity;        // Entity/ComponentEntity.h
class InteractComponent;      // Component/InteractComponent.h
class StorageComponent;       // Component/StorageComponent.h
template <class T> struct RawArray;   // Action/PathSearch.h
void Pf_SetFlagRefreshUpdate_107260(Component* self);  // 0x140107260
void Pf_StorageDropOff_116760(Component* self, ComponentDescription* material, int count);  // 0x140116760
int Pf_StorageCountDelta_1167c0(Component* self, const ComponentDescription* material);  // 0x1401167c0

// ---- action_pf_geom.cpp
int Pf_RayTriIntersect_aa810(void*, const float* P, const float* D, const float* maxT, const float* V0, const float* V1, const float* V2, float* outT, float* outNormal);  // 0x1400aa810
int Pf_TileShapeLut_ae180(void*, char p2, char p3, char p4, char p5, char p6, char p7, char p8, char p9);  // 0x1400ae180
class PopulationComponent;    // Component/PopulationComponent.h
int Pf_PastureTargetPick_53ca0(const PopulationComponent* population, int herdCell);  // 0x140053ca0

// ---- action_pf_pools.cpp
// (0x14001ee40 / 0x14001f8d0 / 0x14001fa80, the GeometryBatch arrays' SetSize forms,
// are Collection::Array<T>::SetSize on the typed members: Game/WorldGeometryElements.h.)
struct GeometryBatchVertex;   // Game/WorldGeometryElements.h

// ---- action_pf_region.cpp

// ---- action_pf_road.cpp
class WorldGeometry;
void Pf_RoadQuad_cc10(WorldGeometry* geo, int x, int y, GeometryBatchVertex* quad, int dirIdx, uint8_t blend);  // 0x1400acc10
int Pf_CellQuadRay_aa20(WorldGeometry* geo, int x, int y, const float* dir, const float* origin, const float* maxT, float* outHit, float* outT, float* outNormal);  // 0x1400aaa20 (body: RayTestCell, action_pf_road.cpp)
float Pf_CellQuadRayWrap_0f90(WorldGeometry* geo, const float* point, float* outNormal);  // 0x1400b0f90 (body: TerrainHeightUnder, action_pf_road.cpp)
// The caller's slot for a by-value Math::Vector3 result that retail leaves UNCONSTRUCTED.
// 0x140122e20 returns a Vector3 through the hidden return slot (RDX) and writes x, y, z on
// every path, so its retail callers hand it bare stack (e.g. 0x14010d9ab, 0x1400e52d3). A
// Math::Vector3 local would run the zeroing default ctor (0x180015070: three stores);
// this holds the Vector3 without constructing it (the anonymous union only suppresses
// the member's ctor; nothing is read as another type). `&slot.v` is the Math::Vector3*.
// The ctor is always_inline so -O0 emits no call for it.
struct Vector3Slot {
    union { Math::Vector3 v; };
    [[gnu::always_inline]] Vector3Slot() {}
};
static_assert(sizeof(Vector3Slot) == sizeof(Math::Vector3), "Vector3Slot is exactly a Vector3");
int PickReachableCell(const Component* target, uint32_t goalCell, float* outPos);  // 0x14004edb0
int EntityAnchorCell(ComponentEntity* const* entityIBP, float* outPos);  // 0x140087fb0
int Pf_EntityAnchorCell_7fb0(ComponentEntity* const* param_1IBP, float* param_2);  // 0x140087fb0 B4 FORWARD -> EntityAnchorCell

// ---- action_pf_roots_drop.cpp
class AnimalDescription;      // Component/AnimalDescription.h
class CitizenComponent;       // Component/CitizenComponent.h
class DropOffRawMaterial;     // Action/DropOffRawMaterial.h
struct ActionStack;           // Action/Action.h
void DropRawMaterialOnGround(CitizenComponent* citizen, ComponentDescription* material, int count);  // 0x1400c78c0
void SpillAllStorage(DropOffRawMaterial* self);  // 0x1400c7570
uint64_t DropOffExec(DropOffRawMaterial* self, ActionStack* stack, StorageComponent* const* target, ComponentDescription* material);  // 0x1400c7620
void RemoveAnimalActions(CitizenComponent* const* citizenIBP, uint32_t task, float* destination, uint32_t createFlag, Component* const* animal, const AnimalDescription* description, uint32_t gate);  // 0x1400bbea0

// ---- action_pf_roots_misc.cpp
class CharacterComponent;     // Component/CharacterComponent.h
class CharacterGrid;          // Component/CharacterGrid.h
class HerdComponent;          // Component/HerdComponent.h
namespace Math { class Scalar; }
void QueryNearestCharacter(const CharacterGrid* grid, CharacterComponent** best, const float* point, const Math::Scalar* radius, int typeId, CharacterComponent* const* exclude);  // 0x140092260

// ---- action_pf_roots_road.cpp

// ---- action_pf_roots_route.cpp
int FindBestAdjacentCell(const Component* target, uint32_t goalCell, float* outPos);  // 0x14012d5d0
namespace Object { template <class T> class InternalPtr; }
ComponentEntity** PickBestDropTarget(const CitizenBehavior* lists, ComponentEntity** best, uint32_t goalCell, const ComponentDescription* material, int count, char useCapacity, float* outPos, const Object::InternalPtr<ComponentEntity>* exclude);  // 0x14008d590

// ---- keystone_a_zone.cpp
class PathfinderThread;       // Game/PathfinderThread.h
namespace Object { template <class T> class InternalPtr; }
namespace System { class String; }
int RoadTool_helper_6e30(PathfinderThread* pf, int start, int goal, int passableTypes, int32_t* path, int maxNodes);  // 0x1400a6e30
void ZoneComponent_sub_d360(Map* map, const Object::InternalPtr<ComponentEntity>* owner, int x, int y, unsigned int w, int h, const System::String* footprint, int rotation);  // 0x14009d360
int RoadTool_sub_ce00(Map* map, unsigned int passableTypes, unsigned int start, uint64_t goal, void* unused5, int32_t* path, uint8_t manhattan, uint8_t respectBlocked);  // 0x14009ce00

// ---- region_graph_build.cpp

// ---- region_paircache.cpp
struct RegionPairCache;   // Action/PathSearch.h
extern "C" RegionPairCache* Pf_RegionPairCache_Build();  // no retail address (the tail of 0x14009e8c0; caller code/Game/Game/region_seed.cpp)
void Pf_RegionPairCacheTick_5d10(RegionPairCache* cache, const int64_t* nowTicks);  // 0x1400a5d10
