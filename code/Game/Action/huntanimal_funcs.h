#pragma once
// HuntAnimal function declarations -- clean game logic layer.
// These functions are called from engine/reimpl/huntanimal.cpp hook wrappers.

#include <cstdint>

class HuntAnimal;

// Resolve all local function pointers (call once before any _Impl)
bool HuntAnimal_ResolveFuncs();

// Slot 1: VectorDeletingDtor (0x1400226f0) -- returns DAT_1401c5f70
uint64_t HuntAnimal_VectorDtor_Impl();

// Slot 2: BaseDtor (0x1400226e0) -- returns TypeInfo_HuntAnimal
uint32_t HuntAnimal_BaseDtor_Impl();

// Slot 3: Serialize (0x1400c8260)
void HuntAnimal_Serialize_Impl(HuntAnimal* self, void* stream);

// Slot 4: AbstractImpl (0x1400c8250) -- too small to hook (9 bytes)
// Returns 1 if huntStarted (+0x38) is 0, else 0.
int HuntAnimal_AbstractImpl_Impl(HuntAnimal* self, void* actionStack);

// Slot 5: Execute (0x1400c8240) -- too small to hook (9 bytes)
// Returns 1 if huntStarted (+0x38) is 0, else 0.
int HuntAnimal_Execute_Impl(HuntAnimal* self, void* actionStack, void* time);

// Slot 6: Enter (0x1400c7de0) -- complex hunting logic
void HuntAnimal_Enter_Impl(HuntAnimal* self);

// Slot 9: GetTarget (0x1400c7d60) -- checks herd and animal entity parents
bool HuntAnimal_GetTarget_Impl(HuntAnimal* self, void* param);
