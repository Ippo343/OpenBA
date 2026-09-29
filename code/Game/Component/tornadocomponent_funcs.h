#pragma once
// Function declarations for the reimplemented TornadoComponent lane.

#include "engine/types/TornadoComponent.h"   // the forwarder: also carries TornadoDescriptionFields
#include <cstdint>

// The tornado's bodies are members (TornadoComponent.h): BuildPathFractal 0x140058980,
// Path::FindNext 0x1400a70c0, Path::Advance 0x1400a71e0, DamageTick 0x140058040;
// ParticleComponent::FadeOutEmitter 0x140053720 and AmbientEmitterComponent::MoveTo
// 0x140042960 (their classes' headers). The strike (0x1400583c0) reads nothing of its
// first argument: a file-local function.
extern "C" {
// B4 FORWARD: 1 outside caller (Component/firecomponent_ignite.cpp) -> ParticleComponent::FadeOutEmitter
void  Tornado_ParticleReverse_53720(void* particleComp, int index);
}
