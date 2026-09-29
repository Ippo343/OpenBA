#pragma once
// Function declarations for the deeply reimplemented FireComponent methods.

#include "engine/types/FireComponent.h"   // the forwarder: also carries FireDescriptionField
#include <cstdint>

// ============================================================
// FireComponent — building fire simulation
// Vtable: 0x140159380 (16 slots; FireDescription's vtable starts at 0x140159400)
// Size:   0x48 bytes
// Overridden slots: 0, 1, 2, 3, 9, 15 — see engine/types/FireComponent.h for the
// byte-verified table. Slots 4-8 and 10-14 are bare `RET` base stubs.
// ============================================================

// ---- leaf bodies, firecomponent_ignite.cpp ----
// FireComponent::Ignite / SpreadToNeighbors / Extinguish / MarkDanger are members
// (FireComponent.h). B4 FORWARD: 1 outside caller file (Dialog/debugui.cpp).
extern "C" void FireComponent_Ignite_610(void* self);          // 0x140049610
// 0x14009fa80 is Map::StampDanger (Game/Map.h), defined in firecomponent_ignite.cpp.
extern "C" void FireComp_StartAudioEmitter_42c60(void* ambientComp, int idx);

// ---- shared with the TORNADO lane, firecomponent_smstate.cpp ----
// DestroyComponent::IsBeingDemolished / BeginBurning / StopBurning (DestroyComponent.h).
// B4 FORWARD: 1 outside caller file (Component/tornadocomponent.cpp).
extern "C" bool FireComponent_IsBeingDemolished_11e690(void* destroyComp);   // 0x14011e690
extern "C" bool FireComponent_BeginBurning_11e7b0(void* destroyComp,
                                                  const int64_t* burnTime);  // 0x14011e7b0
extern "C" void FireComponent_StopBurning_11ea10(void* destroyComp);         // 0x14011ea10

