// code/Game/Work/work_base_standalone.cpp
// Shared standalone factory/vtable helpers for the Work family.
// See work_base_standalone.h and notes/FACTORY_CASCADE_145_BRIEF.md.

#include "work_base_standalone.h"
#include "engine_funcs.h"
#include <cstdint>

// slot1 (+0x08) — exe returns DAT_xxxx (per-type, statically null).
extern "C" void* Work_Slot1(void) { return nullptr; }

// slot5 (+0x28) — exe 0x1400f7540 is `xor eax,eax; ret` (returns 0).
extern "C" int Work_Slot5(void) { return 0; }

// slot12 (+0x60) — exe 0x140006b10 is a void nop.
extern "C" void Work_Slot12(void) {}

// Shared allocator. The per-type factory runs the type's own *_Ctor_Impl on this
// memory (the ctor calls Object::Internal::Internal, zeroes Work + type fields,
// and installs the vtable via ResolveAddr).
extern "C" void* Work_Alloc(int size) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    return fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(size), 8);
}

// Generic ctor for Work types lacking a dedicated *_Ctor_Impl. Mirrors the common
// Work-base init seen in BuildRoad_Ctor_Impl: Object::Internal::Internal, then the
// Work base fields (the two task-index sentinels at +0x20 / +0x28 default to -1),
// the rest zeroed, and the type vtable installed.
extern "C" void* Work_GenericCtor(void* mem, int size, void* vtable) {
    if (!mem) return nullptr;
    if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(mem);
    uint8_t* s = reinterpret_cast<uint8_t*>(mem);
    for (int i = 0x10; i < size; ++i) s[i] = 0;
    // Work base defaults (from the exe Work ctors): +0x20 and +0x28 = -1.
    if (size > 0x24) *reinterpret_cast<int32_t*>(s + 0x20) = -1;
    if (size > 0x2c) *reinterpret_cast<int32_t*>(s + 0x28) = -1;
    *reinterpret_cast<void**>(mem) = vtable;
    return mem;
}

// Generic ScalarDtor for Work types lacking a dedicated *_ScalarDtor_Impl.
extern "C" void* Work_GenericScalarDtor(void* self, unsigned int flags) {
    if (!self) return self;
    if (fn_ObjectInternal_Dtor) fn_ObjectInternal_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// No-op Serialize stand-in (see header note).
extern "C" void Work_SerializeNop(void* /*self*/, void* /*stream*/) {}

// Fallback slot returning 0 (false / lowest priority).
extern "C" int Work_SlotRet0(void) { return 0; }

