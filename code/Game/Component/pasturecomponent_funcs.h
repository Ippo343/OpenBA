#pragma once
// Function declarations for deeply reimplemented PastureComponent methods.
// Called from engine/reimpl/pasturecomponent.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

#include "PastureComponent.h"
#include <cstdint>

// ============================================================
// PastureComponent — pasture entity with shifted controller sub-object
// Vtable: 0x140163f60
// Size: 0x68 bytes
// ============================================================

// Shutdown (vtable slot [3], 0x140127f40): Complex entity ref serialization.
// Component_Serialize + empty serializer + versioned block with IBP array at +0x28,
// counted entity array at +0x30/+0x38, IsDerivedFrom validation,
// int fields at +0x40 and +0x44.
void PastureComponent_Shutdown(PastureComponent* self, void* stream);

// Draw (vtable slot [6], 0x140127a30): Compute pasture capacity.
// Calls FUN_140128570 to compute capacity, stores at +0x40.
void PastureComponent_Draw(PastureComponent* self);

// OnCreate (vtable slot [7], 0x1401277e0): Complex entity ref setup.
// Iterates entity ref array backwards, validates references, registers
// with population manager lists, reassigns entity refs.
void PastureComponent_OnCreate(PastureComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void PastureComponent_InitFuncs();
