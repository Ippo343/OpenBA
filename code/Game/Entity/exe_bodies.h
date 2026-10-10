#pragma once
// code/Game/Entity/exe_bodies.h -- declarations of the exe bodies DEFINED in
// code/Game/Entity/ (ENDGAME B.3R, block W0-P0c, B3R_RULES R5).
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
//   EntityList_PostUpdateNodeHandler       0x1400c2c70  (code/Game/Entity/entitylist_funcs.h)
//   EntityList_PreUpdate                   0x1400c3bd0  (code/Game/Entity/entitylist_funcs.h)
//   EntityList_PostUpdateStaging           0x1400c3ab0  (code/Game/Entity/entitylist_funcs.h)
//   EntityList_RemoveEntity                0x1400c38e0  (code/Game/Entity/entitylist_funcs.h)

#include <cstdint>
#include "object/TypeInfo.h"   // ::Object::Base (FindComponentByTypeId)
#include "Entity/ComponentEntity.h"   // ComponentEntity (the finders' receiver)
class AttendanceComponent;
class BuildComponent;
class CemeteryComponent;
class CitizenComponent;
class ClearAreaComponent;
class ConsumeProduceComponent;
class DestroyComponent;
class FireComponent;
class ImmigrationComponent;
class MapComponent;
class ModelComponent;
class MultiObjectComponent;
class OrchardComponent;
class PastureComponent;
class StateMachineComponent;
class StatusIconComponent;
class StorageComponent;
class StorageLocationComponent;
class TrackerComponent;
class TradeComponent;
class WorkComponent;
class WorkPlaceComponent;

// ---- entity_findcomponent.cpp
// Entity::FindComponent — generic implementation (moved from engine/funcs/engine_funcs.h
// and engine_funcs.cpp, B3R W2-EZ). Searches entity component array at +0x30 (ptr) /
// +0x38 (count). Calls vtable[2] (+0x10) on each component to get type ID, confirms
// with IsDerivedFrom. Stores result via InternalBasePtr::InternalBasePtr(retval, found).
// REIMPLEMENTED: decompiled from all FindComponent_X variants (identical algorithm).
Component* Entity_FindComponentByExeTypeId(ComponentEntity* entity, const uint32_t* typeIdPtr);
// The typed out-slot form every caller uses: the found component (null on a miss)
// stored into the caller's T* slot. The core matched T's id and IsDerivedFrom.
template <class T>
inline void Entity_FindComponentByExeTypeId(ComponentEntity* entity, T** retval, uint32_t* typeIdPtr) {
    *retval = static_cast<T*>(Entity_FindComponentByExeTypeId(entity, typeIdPtr));
}

// FindComponent inline wrappers — call Entity_FindComponentByExeTypeId with correct type ID.
// Original exe functions (FUN_140XXXXXX) are no longer needed.

// fn_FindComponent_Build — DAT_1401c79e0, original FUN_140004680. MISNOMER: the type id at
// DAT_1401c79e0 is MapComponent, not BuildComponent (registrar MapComponent_RegisterType
// 0x14014f375). Bodies: code/Game/Entity/entity_findcomponent.cpp.
void fn_FindComponent_Build(ComponentEntity* entity, MapComponent** retval);
// fn_FindComponent_BuildState — DAT_1401c7d98, original FUN_140003ea0
void fn_FindComponent_BuildState(ComponentEntity* entity, StateMachineComponent** retval);
// fn_FindComponent_ConsumeProduce — DAT_1401c7350, original FUN_14000b6f0
void fn_FindComponent_ConsumeProduce(ComponentEntity* entity, ConsumeProduceComponent** retval);
// fn_FindComponent_Position — DAT_1401c7008, original FUN_14000d6c0
void fn_FindComponent_Position(ComponentEntity* entity, BuildComponent** retval);
// fn_FindComponent_Destroy — DAT_1401c75f0, original FUN_140005780
void fn_FindComponent_Destroy(ComponentEntity* entity, DestroyComponent** retval);
// fn_FindComponent_Building — DAT_1401c4b80, original FUN_140015140.
// ⚠ MISNAMED, kept only because the name is load-bearing across the tree: 0x140015140
// matches DWORD_1401c4b80 = TradeComponent (verified at instruction level 2026-08-21),
// so this is a TRADING-POST finder, not a "building" finder. Anything reading its result
// as "is there a building here?" is wrong — see the animal habitat test in
// animalcomponent.cpp, whose comment said exactly that until it was corrected.
// Consequence for gating: this finder returns null for EVERY entity while
// BANISHED_LIVE_TRADE_TIDS is off, and non-null only for a trading post when it is on.
void fn_FindComponent_Building(ComponentEntity* entity, TradeComponent** retval);
// fn_FindComponent_Work — DAT_1401c8070 = "WorkComponent" (factory_registrations.csv:306),
// original FUN_140003050. Was named fn_FindComponent_Workplace, which is a DIFFERENT type:
// WorkPlaceComponent is DAT_1401c80e0 / fn_FindComponent_WorkPlace below. Retail uses both,
// side by side, in WorkPlaceUI::OnCreate (0x140087080) and in the builder-profession pair
// (0x14012f520 / 0x14012f670). Confusing them costs crashes — a WorkComponent read as a
// WorkPlaceComponent puts the small work id (+0x28) where a roster heap pointer (+0x58) is
// expected. Fields used off a WorkComponent: +0x18 WorkDescription, +0x28 scheduler work id
// (-1 = none), +0x30 cached max workers (WorkComponent_CalcMaxWorkers).
void fn_FindComponent_Work(ComponentEntity* entity, WorkComponent** retval);
// fn_FindComponent_Attendance — DAT_1401c3f40, original FUN_1400105e0
void fn_FindComponent_Attendance(ComponentEntity* entity, AttendanceComponent** retval);
// fn_FindComponent_Pasture — DAT_1401c7ba0, original FUN_140004fe0
void fn_FindComponent_Pasture(ComponentEntity* entity, PastureComponent** retval);
// fn_FindComponent_Orchard — DAT_1401c46b0, original FUN_140005440
void fn_FindComponent_Orchard(ComponentEntity* entity, OrchardComponent** retval);
// fn_FindComponent_WorkPlace — DAT_1401c80e0 = "WorkPlaceComponent"
// (factory_registrations.csv:308), original FUN_140002e10. Was named
// fn_FindComponent_BuildState2. Fields: +0x48 active profession key, +0x58 roster base,
// +0x60 roster count, +0x18 WorkPlaceDescription (whose +0x20 is the natural profession).
void fn_FindComponent_WorkPlace(ComponentEntity* entity, WorkPlaceComponent** retval);
// fn_FindComponent_SM — DAT_1401c71c8, original FUN_14000a170
void fn_FindComponent_SM(ComponentEntity* entity, ClearAreaComponent** retval);
// fn_FindComponent_Citizen — DAT_1401c7158, original FUN_140009790
void fn_FindComponent_Citizen(ComponentEntity* entity, CitizenComponent** retval);
// fn_FindComponent_Storage — DAT_1401c7e78, original FUN_140006270
void fn_FindComponent_Storage(ComponentEntity* entity, StorageComponent** retval);
// fn_FindComponent_Cemetery — DAT_1401c4090, original FUN_140010a00
void fn_FindComponent_Cemetery(ComponentEntity* entity, CemeteryComponent** retval);
// fn_FindComponent_Immigration — DAT_1401c4480, original FUN_140012020
void fn_FindComponent_Immigration(ComponentEntity* entity, ImmigrationComponent** retval);
// fn_FindComponent_Tracker — DAT_1401c4b10, original FUN_140013320
void fn_FindComponent_Tracker(ComponentEntity* entity, TrackerComponent** retval);
// fn_FindComponent_MeshInstance — DAT_1401c7e08,
// StatusIconComponent's BaseTypeInfo (writer StatusIconComponent_RegisterType
// 0x14014fab5; real HappinessComponent id is 0x1401c42c0). Original FUN_140002f90.
void fn_FindComponent_MeshInstance(ComponentEntity* entity, StatusIconComponent** retval);
// fn_FindComponent_Character — DAT_1401c7a50 = ModelComponent (writer ModelComponent_RegisterType);
// finds ModelComponent. Function name kept for callers; the "Character" label is historical.
// Original FUN_140003f60.
void fn_FindComponent_Character(ComponentEntity* entity, ModelComponent** retval);

// FindComponent_MultiObject — inline wrapper for MultiObjectComponent lookup
void fn_FindComponent_MultiObject(ComponentEntity* entity, MultiObjectComponent** retval);

// fn_FindComponent_Fire — searches entity for FireComponent (DAT_1401c41e0)
void fn_FindComponent_Fire(ComponentEntity* entity, FireComponent** retval);

void fn_FindComponent_Unknown7ee8(ComponentEntity* entity, StorageLocationComponent** retval);

