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

