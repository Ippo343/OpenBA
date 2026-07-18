#pragma once
// RemoveAnimal function declarations -- clean game logic layer.
// These functions are called from engine/reimpl/removeanimal.cpp hook wrappers.

#include <cstdint>

struct RemoveAnimal;

// Resolve all local function pointers (call once before any _Impl)
bool RemoveAnimal_ResolveFuncs();

// Slot 0: ScalarDeletingDtor (0x140022ee0)
void* RemoveAnimal_ScalarDtor_Impl(RemoveAnimal* self, uint32_t flags);

// Slot 1: VectorDeletingDtor (0x140022e60) -- returns DAT_1401c6088
uint64_t RemoveAnimal_VectorDtor_Impl();

// Slot 2: BaseDtor (0x140022e50) -- returns TypeInfo_RemoveAnimal
uint32_t RemoveAnimal_BaseDtor_Impl();

// Slot 3: Serialize (0x1400ca870)
void RemoveAnimal_Serialize_Impl(RemoveAnimal* self, void* stream);

// Slot 4: AbstractImpl (0x1400ca7f0) -- marks entity for removal
// Returns int per bug class 7.
int RemoveAnimal_AbstractImpl_Impl(RemoveAnimal* self, void* actionStack);

// Slot 8: CheckEntity (0x1400ca760) -- clears "in use" bit on model component
void RemoveAnimal_CheckEntity_Impl(RemoveAnimal* self);

// Slot 9: GetTarget (0x1400ca720) -- returns true if param matches entity at +0x20
bool RemoveAnimal_GetTarget_Impl(RemoveAnimal* self, void* param);
