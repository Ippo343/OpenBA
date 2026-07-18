#pragma once
// Function declarations for deeply reimplemented ParticleComponent methods.

#include "ParticleComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x140053270): Serialize emitter bool array.
void ParticleComponent_Shutdown(ParticleComponent* self, void* stream);

// Serialize (vtable slot [4], 0x140053230): Setup + zero emitter flags.
void ParticleComponent_Serialize(ParticleComponent* self);

// Draw (vtable slot [6], 0x140052f70): Update emitter timing/opacity.
void ParticleComponent_Draw(ParticleComponent* self);

// OnCreate (vtable slot [7], 0x140052e80): Remove scene objects, clear flags.
void ParticleComponent_OnCreate(ParticleComponent* self);

// OnRemove (vtable slot [8], 0x140052b30): Position particles relative to model.
void ParticleComponent_OnRemove(ParticleComponent* self, void* time);

// OnUpdate2 (vtable slot [11], 0x1400531b0): Call setup + update active emitters.
void ParticleComponent_OnUpdate2(ParticleComponent* self);

void ParticleComponent_InitFuncs();

// ---- Full helper-closure ports (standalone-registered at their exe addrs) ----
extern "C" void ParticleComponent_Setup(ParticleComponent* self);                       // 0x140052480
extern "C" void ParticleComponent_Place(ParticleComponent* self, int index);            // 0x140052560
extern "C" void ParticleComponent_StopEmitter(ParticleComponent* self, int index);      // 0x140052990
extern "C" void ParticleComponent_StartEmitter(ParticleComponent* self, int index);     // 0x140052a50
extern "C" void ParticleComponent_StartEmitterFade(ParticleComponent* self, int index); // 0x140053660
extern "C" void ParticleComponent_UpdateTick(ParticleComponent* self, void* time);      // 0x140052b30
extern "C" void ParticleComponent_Event(ParticleComponent* self, int eventCode);        // 0x140053090
