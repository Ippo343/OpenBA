#pragma once
// Function declarations for deeply reimplemented DestroyComponent methods.
// Called from engine/reimpl/destroycomponent.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "DestroyComponent.h"
#include <cstdint>

// ============================================================
// DestroyComponent -- building demolition/destruction
// Vtable: 0x140163120
// Size: 0x68 bytes
// ============================================================

// Shutdown (vtable slot [3], 0x14011DB50): Serializes all destroy state fields.
// Calls Component_Serialize base, then two serializer blocks:
//   1. Empty (controller compat)
//   2. field54 (int), field58 (int), field51 (bool), field60 (InternalBasePtr),
//      destroyFlags (bool), field48 (Time)
void DestroyComponent_Shutdown(DestroyComponent* self, void* stream);

// Serialize (vtable slot [4], 0x14011DB00): OnCreate -- copies mesh reference.
// Reads _unk18, does IsDerivedFrom check, copies 8 bytes from building+0x20
// into field60.
void DestroyComponent_Serialize(DestroyComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void DestroyComponent_InitFuncs();
