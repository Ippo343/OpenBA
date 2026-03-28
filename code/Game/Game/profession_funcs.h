#pragma once
// Function declarations for deeply reimplemented Profession methods.
// Called from engine/reimpl/profession.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "Profession.h"
#include <cstdint>

// Init: Initializes all Profession fields to defaults (mirrors ctor at 0x1400a9030).
void Profession_Init(Profession* self);

// Dtor (vtable +0x00): Destroys strings, calls External::~External, conditionally frees.
void* Profession_Dtor(Profession* self, uint32_t flags);

// Cleanup (vtable +0x30, Slot6): Destroys and re-constructs in place (reset).
void Profession_Cleanup(Profession* self);

// Slot8 (vtable +0x40): Reset/reinit, returns null if self is null.
void* Profession_Slot8(Profession* self);

// Serialize (vtable +0x28, Slot5): Serializes all fields via IO::Serializer.
void Profession_Serialize(Profession* self, void* stream);

// One-time init: resolve local function pointers needed by Profession reimpl.
void Profession_InitFuncs();
