// code/Game/Action/action_base_standalone.h
// Shared standalone factory/vtable helpers for the Action family
// (Object::Internal-derived, 10-slot vtable). See notes/FACTORY_CASCADE_145_BRIEF.md.
//
// Every Action subtype's standalone vtable reuses these shared slots:
//   slot0 (+0x00) ScalarDtor  -> Action_ScalarDtor   (generic, exe 0x140001260)
//   slot1 (+0x08)             -> Action_Slot1        (returns DAT_1401c3df8 == null)
//   slot5 (+0x28) Execute     -> Action_BaseExecute  (base actions, returns 0)
//   slot6 (+0x30) Enter       -> Action_Enter        (void nop)
//   slot7 (+0x38) Exit        -> Action_Exit         (void nop)
// Per-type slots (GetTypeId, Serialize, AbstractImpl, [Execute], CheckEntity,
// GetTarget) point at the type's own *_Impl functions in its reimpl file.
#pragma once
#include <cstdint>

extern "C" {

// slot0: generic Internal dtor. DeletePtr base IBP @+0x10, Object::Internal::
// ~Internal, MemFree if (flags&1). Vtable-reset is cosmetic and skipped.
void* Action_ScalarDtor(void* self, unsigned int flags);

// slot1: exe returns DAT_1401c3df8, which is statically null.
void* Action_Slot1(void);

// slot5: base Action Execute (exe 0x1400c4a90) -> returns 0.
int Action_BaseExecute(void);

// slot6/slot7: base Action Enter/Exit (exe 0x1400c4b90 / 0x1400c4ba0) -> void nop.
void Action_Enter(void);
void Action_Exit(void);

// slot8: base Action CheckEntity (exe 0x1400c4a30) -> void nop. Also reused for
// per-type CheckEntity stubs that the exe leaves as a 3-byte void no-op.
void Action_BaseCheckEntity(void);

// slot9: base Action GetTarget (exe 0x1400c4a40) -> returns 0 (false).
int Action_BaseGetTarget(void);

// Shared init: Object::Internal::Internal(p); zero type fields [0x10, size);
// install per-type vtable. Mirrors PlayAnimation_Factory's body.
void* Action_InitInPlace(void* p, int size, void* vtable);

// Shared factory: MemAlloc(size, 8) then Action_InitInPlace.
void* Action_Alloc(int size, void* vtable);

}  // extern "C"

