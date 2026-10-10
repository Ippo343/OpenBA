#pragma once
// src/game/Entity/exe_bodies.h -- declarations of the exe bodies DEFINED in
// src/game/Entity/ (ENDGAME B.3R, block W0-P0c, B3R_RULES R5).
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
//   EntityList_PostUpdateNodeHandler       0x1400c2c70  (src/game/Entity/entitylist_funcs.h)
//   EntityList_PreUpdate                   0x1400c3bd0  (src/game/Entity/entitylist_funcs.h)
//   EntityList_PostUpdateStaging           0x1400c3ab0  (src/game/Entity/entitylist_funcs.h)
//   EntityList_RemoveEntity                0x1400c38e0  (src/game/Entity/entitylist_funcs.h)

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
// (B8 ENT: the FindComponent family is ComponentEntity::FindComponent<T>; the file is gone.)
