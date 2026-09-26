#pragma once
// Build/Work action types — binary-compatible struct layouts.
// Application-x64.exe, build 170910.
//
// These are all Work-derived (0x48 byte base), NOT Action-derived (0x20 byte base).
// Decompiled from constructors which all call Object::Internal::Internal() then
// init Work fields at +0x10..+0x40, then set derived vtable and init derived fields.
//
// Work vtable layout (12 slots, 96 bytes):
//   [0] ScalarDeletingDtor    [1] VectorDeletingDtor   [2] BaseDtor
//   [3] Serialize             [4] Execute/Update       [5] shared (0x1400f7540)
//   [6] OnComplete            [7] OnCancel             [8] Cleanup
//   [9] Validate              [10] GetTarget            [11] GetPriority

#include <cstdint>
#include <cstddef>
#include "Work/BuildRoad.h"   // ENDGAME B.3.8 (WRK): the real class; its POD is gone (B8-4)
#include "Work/BuildStructure.h"   // ENDGAME B.3.8 (WRK): the real class; its POD is gone (B8-4)
#include "Work/ClearArea.h"   // ENDGAME B.3.8 (WRK): the real class; its POD is gone (B8-4)
#include "Work/ConsumeProduce.h"   // ENDGAME B.3.8 (WRK): the real class; its POD is gone (B8-4)
#include "Work/GatherRawMaterials.h"   // ENDGAME B.3.8 (WRK): the real class; its POD is gone (B8-4)
#include "Work/Pickup.h"   // ENDGAME B.3.8 (WRK): the real class; its POD is gone (B8-4)
#include "Work/PlantArea.h"   // ENDGAME B.3.8 (WRK): the real class; its POD is gone (B8-4)
#include "Work/PlantOrchard.h"   // ENDGAME B.3.8 (WRK): the real class; its POD is gone (B8-4)

// BuildRoad — the real class code/Game/Work/BuildRoad.h (ENDGAME B.3.8).
// BuildStructure — the real class code/Game/Work/BuildStructure.h (ENDGAME B.3.8).

// ClearArea — the real class code/Game/Work/ClearArea.h (ENDGAME B.3.8).
// ConsumeProduce (the POD was ConsumeProduceAction) — the real class
// code/Game/Work/ConsumeProduce.h (ENDGAME B.3.8).

// GatherRawMaterials — the real class code/Game/Work/GatherRawMaterials.h (ENDGAME B.3.8).

// PlantArea — the real class code/Game/Work/PlantArea.h (ENDGAME B.3.8).

// PlantOrchard — the real class code/Game/Work/PlantOrchard.h (ENDGAME B.3.8).

// Pickup — the real class code/Game/Work/Pickup.h (ENDGAME B.3.8).
