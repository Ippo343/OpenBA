#pragma once
// Function declarations for deeply reimplemented ParticleComponent methods.

#include "ParticleComponent.h"
#include <cstdint>

void ParticleComponent_InitFuncs();

// ---- Full helper-closure ports (standalone-registered at their exe addrs) ----
extern "C" void ParticleComponent_Setup(ParticleComponent* self);                       // 0x140052480
extern "C" void ParticleComponent_Place(ParticleComponent* self, int index);            // 0x140052560
extern "C" void ParticleComponent_StopEmitter(ParticleComponent* self, int index);      // 0x140052990
extern "C" void ParticleComponent_StartEmitter(ParticleComponent* self, int index);     // 0x140052a50
extern "C" void ParticleComponent_StartEmitterFade(ParticleComponent* self, int index); // 0x140053660
