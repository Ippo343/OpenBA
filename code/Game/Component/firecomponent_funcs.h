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
// (FireComponent.h).
class FireComponent;
// 0x14009fa80 is Map::StampDanger (Game/Map.h), defined in firecomponent_ignite.cpp.
class AmbientEmitterComponent; class DestroyComponent;
namespace System { class Time; }
extern "C" void FireComp_StartAudioEmitter_42c60(AmbientEmitterComponent* ambientComp, int idx);  // B8: stays free -- a second port of AmbientEmitterComponent::PlaySlot (0x140042c60) with guards PlaySlot lacks; moving its caller onto PlaySlot changes behaviour (known_port_issues row 1080)

// ---- shared with the TORNADO lane, firecomponent_smstate.cpp ----
// DestroyComponent::IsBeingDemolished / BeginBurning / StopBurning (DestroyComponent.h).
extern "C" bool FireComponent_IsBeingDemolished_11e690(DestroyComponent* destroyComp);   // 0x14011e690 B8: stays free -- nothing calls it; its only use is its address printed by the tornado init trace (Component/tornadocomponent.cpp:225-227), and a member has no plain address (D3). The tree's null test was dropped (C3; row 1105)
extern "C" bool FireComponent_BeginBurning_11e7b0(DestroyComponent* destroyComp, const System::Time* burnTime);  // 0x14011e7b0 B8: stays free -- nothing calls it; its only use is its address printed by the tornado init trace (Component/tornadocomponent.cpp:225-227), and a member has no plain address (D3). The tree's null test was dropped (C3; row 1106)
extern "C" void FireComponent_StopBurning_11ea10(DestroyComponent* destroyComp);         // 0x14011ea10 B8: stays free -- nothing calls it; its only use is its address printed by the tornado init trace (Component/tornadocomponent.cpp:225-227), and a member has no plain address (D3). The tree's null test was dropped (C3; row 1107)

