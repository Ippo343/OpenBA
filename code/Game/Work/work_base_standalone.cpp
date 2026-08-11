// code/Game/Work/work_base_standalone.cpp
// Shared standalone factory/vtable helpers for the Work family.
// See work_base_standalone.h and notes/FACTORY_CASCADE_145_BRIEF.md.

#include "work_base_standalone.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include "AutoPickupWork.h"                // WorkAddr::Vtable / AutoPickupWorkAddr::Vtable
#include <cstdint>
#include <cstdio>

// Work::Serialize (0x1400f7400), ported in work_base_reimpl.cpp — slot 3 of the base vtable.
extern void WorkBase_Serialize_f7400(void*, void*);

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

// ============================================================
// The Work BASE vtable — exe 0x140161a80 (WorkAddr::Vtable)
// ============================================================
// This is the vtable retail installs in Work_Ctor25 (0x1400f72e0):
//     1400f730a  LEA RAX,[0x140161a80] ; 1400f7311  MOV [RDI],RAX
// and the one every AutoPickupWork-family dtor writes back on the way down the
// base chain. It was registered NOWHERE, so ~9 files' ResolveAddr of it returned 0
// and those writes stored NULL into the object's vtable pointer.
//
// ⚠ Do NOT confuse it with 0x140161300 / 0x140161380. Those are RTTI TYPE
// DESCRIPTORS, not vtables — the bytes read {0x140006b10, "FishingWork\0"} and
// {0x140006b10, "ForesterWork\0"}, each sitting 0x20 before the real vtable of that
// concrete class. harvestorchard.cpp and harvestarea.cpp had grabbed those two
// addresses as "vtable_Work"/"vtable_AutoPickupWork" (adjacency error); registering
// them under those names would have installed a pointer to ASCII string data as a
// live object's vtable. See reference_wrong_but_nonzero_typeid — same trap, one
// level down.
//
// Slot values below are the retail bytes at 0x140161a80, matched against ports we
// already have. 0x14003addc is MSVC's _purecall.
static uint32_t Work_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("Work");
    return c;
}

// Stand-in for retail's _purecall slots. Retail aborts here; a null would be a
// call-of-0 that the crash dumper reports as a mystery vtable slot, so this names
// itself instead. Reaching it at all is a bug — the base vtable is only ever
// installed transiently, between Object::Internal::Internal and the leaf ctor's
// own vtable write (and again on the way down the dtor chain).
extern "C" int Work_PureCall(void) {
    static bool reported = false;
    if (!reported) {
        reported = true;
        std::fprintf(stderr, "[WorkBase] _purecall slot dispatched on a Work/AutoPickupWork "
                             "base vtable — the object was used while only its BASE vtable "
                             "was installed. This should be unreachable.\n");
        std::fflush(stderr);
    }
    return 0;
}

static void* g_Work_Vtable[13] = {
    (void*)&Work_GenericScalarDtor, // +0x00 ScalarDtor  (0x14002ca60, unported: same shape)
    (void*)&Work_Slot1,             // +0x08 dtor thunk  (0x14002ca50, unported)
    (void*)&Work_GetTypeId,         // +0x10 GetTypeId   (0x14002ca40, unported)
    (void*)&WorkBase_Serialize_f7400,// +0x18 Serialize  (0x1400f7400, PORTED)
    (void*)&Work_PureCall,          // +0x20 _purecall
    (void*)&Work_Slot5,             // +0x28 Execute     (0x1400f7540 = `return 0`)
    (void*)&Work_PureCall,          // +0x30 _purecall
    (void*)&Work_PureCall,          // +0x38 _purecall
    (void*)&Work_PureCall,          // +0x40 _purecall
    (void*)&Work_PureCall,          // +0x48 _purecall
    (void*)&Work_PureCall,          // +0x50 _purecall
    (void*)&Work_PureCall,          // +0x58 _purecall
    (void*)&Work_Slot12,            // +0x60 void nop    (0x140006b10)
};

extern "C" void RegisterWorkBaseVtable() {
    RegisterExeAddr(WorkAddr::Vtable, (uintptr_t)g_Work_Vtable);   // 0x140161a80
}

