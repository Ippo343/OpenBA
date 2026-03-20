#pragma once
// Function declarations for deeply reimplemented Pickup methods.
// Called from engine/reimpl/pickup.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "Pickup.h"
#include <cstdint>

// ============================================================
// Pickup -- Item pickup work action (0x60 bytes)
// Inherits: Object::Internal -> Work (0x48) -> Pickup
// Vtable: 0x140161810 (12 slots)
//
// Unlike most Work classes, Pickup has its OWN Execute override
// (0x1400f38f0) that handles re-targeting when the citizen changes
// workplace during an active pickup.
// ============================================================

// Slot0 (vtable +0x00): ScalarDtor. Calls dtor body, conditional free.
void* Pickup_ScalarDtor_Impl(Pickup* self, uint32_t flags);

// Slot1 (vtable +0x08): VectorDtor / GetTypeIndex. Returns type index constant.
// Tiny, not hooked. Implemented for standalone exe.
uint64_t Pickup_Slot1_Impl(Pickup* self);

// Slot2 (vtable +0x10): BaseDtor / GetTypeId. Returns type ID constant.
// Tiny, not hooked. Implemented for standalone exe.
uint32_t Pickup_Slot2_Impl(Pickup* self);

// Slot3/Serialize (vtable +0x18): Work::Serialize + Serializer(v2)
//   + Serialize(status) + InternalBasePtr::Serialize(citizenIBP) + IsDerivedFrom validate
//   + if version > 0: ArraySerialize(candidateList)
void Pickup_Serialize_Impl(Pickup* self, void* stream);

// Slot4/CreateWork (vtable +0x20): Complex pickup target assignment logic.
// Searches for valid pickup targets, creates PerformWork actions.
// Returns: 0=continue, 1=done/no-work, 2=spawned-work.
// Uses int return type (bug class 7).
// NOTE: Calls through to original function -- too complex for inline reimpl.
int Pickup_Slot4_Impl(Pickup* self, void* entityIBP, uint64_t param3, uint64_t param4);

// Slot5/Execute (vtable +0x28): UNIQUE to Pickup -- handles re-targeting.
// If citizen changed workplace, cancels old work and re-runs Slot4.
// Returns: 0=false (no active work), nonzero=active.
int Pickup_Execute_Impl(Pickup* self, void* entityIBP, void* param3);

// Slot6/OnComplete (vtable +0x30): Resets citizenIBP to null, sets status=2.
// Returns false.
bool Pickup_Slot6_Impl(Pickup* self);

// Slot7/OnCancel (vtable +0x38): Resets citizenIBP to null, sets status=0.
// Returns int (bug class 7 -- action executor reads EAX).
int Pickup_Slot7_Impl(Pickup* self);

// Slot8/CancelAll (vtable +0x40): If status==1, cancels citizen's work.
// Returns int (bug class 7 -- action executor reads EAX).
int Pickup_Slot8_Impl(Pickup* self);

// Slot9/Validate (vtable +0x48): Validates citizen entity via FUN_1400f7100.
// If invalid, resets citizenIBP to null and status=0.
// Returns true if valid (or no citizen assigned).
bool Pickup_Slot9_Impl(Pickup* self, void* param2);

// Slot10/GetTarget (vtable +0x50): Returns global entity from GameWorld+0xb0+0x30.
void* Pickup_Slot10_Impl(Pickup* self, void* outPtr);

// Slot11/HasWorker (vtable +0x58): Returns (citizenIBP != null).
// 10 bytes, unhookable. Implemented for standalone exe.
bool Pickup_Slot11_Impl(Pickup* self);

// DtorBody: Sets vtables, calls Work base dtor.
void Pickup_DtorBody(Pickup* self);

// Resolve all function pointers needed by Pickup.
// Called once during hook installation.
bool Pickup_ResolveFuncs();
