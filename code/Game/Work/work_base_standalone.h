// code/Game/Work/work_base_standalone.h
// Shared standalone factory/vtable helpers for the Work family
// (Object::Internal -> Work (0x48) -> <Type>, 13-slot vtable).
// See notes/FACTORY_CASCADE_145_BRIEF.md.
//
// Work vtable layout (13 slots, ground-truthed from Ghidra):
//   slot0  (+0x00) ScalarDtor   -> per-type <Type>_ScalarDtor_Impl
//   slot1  (+0x08)              -> Work_Slot1   (exe returns DAT_xxxx == null)
//   slot2  (+0x10) GetTypeId    -> per-type (GetTypeIdByName)
//   slot3  (+0x18) Serialize    -> per-type <Type>_Serialize_Impl
//   slot4  (+0x20) Execute      -> per-type <Type>_Execute[_Impl]
//   slot5  (+0x28)              -> Work_Slot5   (exe 0x1400f7540, returns 0)
//   slot6  (+0x30) OnComplete   -> per-type <Type>_OnComplete_Impl
//   slot7  (+0x38) OnCancel     -> per-type <Type>_OnCancel_Impl
//   slot8  (+0x40) Cleanup      -> per-type <Type>_Cleanup_Impl
//   slot9  (+0x48) Validate     -> per-type <Type>_Validate_Impl
//   slot10 (+0x50) GetTarget    -> per-type <Type>_GetTarget_Impl
//   slot11 (+0x58) GetPriority  -> per-type <Type>_GetPriority_Impl
//   slot12 (+0x60)              -> Work_Slot12  (exe 0x140006b10, void nop)
//
// The per-type factory is `MemAlloc(size,8)` then the type's own `*_Ctor_Impl`,
// which installs the vtable via ResolveAddr(Module::Exe, <vtableAddr>) — so each
// type registers its standalone vtable at that exe address (RegisterExeAddr).
#pragma once
#include <cstdint>

extern "C" {

// slot1: exe returns a per-type DAT_xxxx global, all statically null.
void* Work_Slot1(void);

// slot5: exe 0x1400f7540 -> `return 0` (bool/undefined1).
int Work_Slot5(void);

// slot12: exe 0x140006b10 -> void nop.
void Work_Slot12(void);

// Fallback for a per-type slot whose reimpl is missing (e.g. a GetPriority that
// was COMDAT-folded / not separately reimplemented). Returns 0 (false / lowest
// priority) — safe stand-in so the vtable slot is never null. Only reached if the
// slot is actually dispatched (dormant for the factory-cascade landmine fix).
int Work_SlotRet0(void);

// Shared allocator: MemAlloc(size, 8). The per-type factory then runs the type's
// own *_Ctor_Impl on the returned memory. Returns null on allocation failure.
void* Work_Alloc(int size);

// Generic ctor for Work types whose reimpl has no dedicated *_Ctor_Impl yet:
// Object::Internal::Internal(mem); zero type fields [0x10, size); install vtable.
// Produces a valid, non-null, correctly-vtabled object (removes the
// IsDerivedFrom(NULL) landmine). Per-type ctors that set non-zero field defaults
// are preferred where they exist; this is the faithful-enough fallback.
void* Work_GenericCtor(void* mem, int size, void* vtable);

// Generic ScalarDtor for Work types with no dedicated *_ScalarDtor_Impl:
// Object::Internal::~Internal(self); MemFree if (flags & 1).
void* Work_GenericScalarDtor(void* self, unsigned int flags);

// No-op Serialize stand-in for types whose reimpl omits Serialize. (Work objects
// are runtime-created, not part of the worldgen serialize stream, so this is
// never reached in the current standalone scenarios.)
void Work_SerializeNop(void* self, void* stream);

}  // extern "C"

