#pragma once
// Function declarations for deeply reimplemented ProfessionList methods.
// Called from engine/reimpl/professionlist.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "ProfessionList.h"
#include <cstdint>

// Init: Initializes all ProfessionList fields to defaults (mirrors factory init).
void ProfessionList_Init(ProfessionList* self);

// Dtor (vtable +0x00): Frees array, calls External::~External, conditionally frees self.
void* ProfessionList_Dtor(ProfessionList* self, uint32_t flags);

// Cleanup (vtable +0x30, Slot6): dtor(self,0) + External::External + set vtable + zero fields.
void ProfessionList_Cleanup(ProfessionList* self);

// Slot8 (vtable +0x40): Reset/reinit, returns null if self is null.
void* ProfessionList_Slot8(ProfessionList* self);

// Serialize (vtable +0x28, Slot5): Serializes ExternalPtr refs via FUN_140018020.
// Returns 1 (matches original Slot5's `undefined1` return; avoids garbage-EAX bug #7).
int ProfessionList_Serialize(ProfessionList* self, void* stream);

// One-time init: resolve local function pointers needed by ProfessionList reimpl.
void ProfessionList_InitFuncs();
