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

// Resolve internal function pointers (called once at startup).
void FireComponent_InitFuncs();

// ---- leaf bodies, firecomponent_ignite.cpp ----
extern "C" void FireComponent_Ignite_610(void* self);          // 0x140049610
extern "C" void FireComponent_Spread_130(void* self);          // 0x140049130
extern "C" void FireComponent_Extinguish_4c0(void* self);      // 0x1400494c0
extern "C" void FireComponent_WaterApplied_090(void* self, int msg);  // 0x140049090
extern "C" void FireComp_MarkDanger_99c0(void* self, short delta);    // 0x1400499c0
extern "C" void FireComp_StampDangerRect_9fa80(int* grid, uint64_t unused,
                                               int xMin, int yMin, int xMax, int yMax,
                                               short delta);           // 0x14009fa80
extern "C" void FireComp_StartAudioEmitter_42c60(void* ambientComp, int idx);
extern "C" void* FireWork_CtorFull_e9bd0(void* mem, uint64_t* pEntityIBP, uint32_t capacity);

// ---- shared with the TORNADO lane, firecomponent_smstate.cpp ----
// The tornado building-damage arm resolves these through ResolveAddr; they are
// ported and registered ONCE so both lanes dispatch the same body.
extern "C" bool FireComponent_IsBeingDemolished_11e690(void* destroyComp);   // 0x14011e690
extern "C" bool FireComponent_BeginBurning_11e7b0(void* destroyComp,
                                                  const int64_t* burnTime);  // 0x14011e7b0
extern "C" void FireComponent_StopBurning_11ea10(void* destroyComp);         // 0x14011ea10


// Type-id globals, filled after Engine::Startup from game_main.cpp.
extern "C" void FillTypeIdGlobals_fire();
