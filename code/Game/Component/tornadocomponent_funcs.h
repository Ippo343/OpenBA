#pragma once
// Function declarations for deeply reimplemented TornadoComponent methods.
// No dependency on hooks.h — portable to standalone exe.

#include "TornadoComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x140058b80): Serialize Vector3 + tornadoId + path array + Time + direction.
void TornadoComponent_Shutdown(TornadoComponent* self, void* stream);

// Serialize (vtable slot [4], 0x1400585a0): Full tornado initialization — random endpoints,
// path generation, notification message. 236 instructions.
void TornadoComponent_Serialize(TornadoComponent* self);

// OnRemove (vtable slot [8], 0x140057cd0): Tornado movement, pathfinding, audio,
// entity damage, building interaction. 210 instructions.
void TornadoComponent_OnRemove(TornadoComponent* self, void* time);

void TornadoComponent_InitFuncs();
