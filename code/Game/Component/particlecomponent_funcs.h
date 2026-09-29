#pragma once
// Function declarations for deeply reimplemented ParticleComponent methods.

#include "ParticleComponent.h"
#include <cstdint>

// B4 FORWARD to ParticleComponent::StartEmitterFade (0x140053660): 2 outside callers
// (diseasecomponent.cpp, firecomponent_ignite.cpp).
extern "C" void ParticleComponent_StartEmitterFade(ParticleComponent* self, int index); // 0x140053660
