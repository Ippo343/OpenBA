// code/Game/Action/action_base_standalone.cpp
// Shared standalone factory/vtable helpers for the Action family.
// See action_base_standalone.h and notes/FACTORY_CASCADE_145_BRIEF.md.
//
// These mirror the small shared functions referenced by every Action subtype's
// exe vtable (0x140001260 dtor, 0x1400011e0 slot1, 0x1400c4a90 base Execute,
// 0x1400c4b90/0x1400c4ba0 Enter/Exit nops) plus the common factory body
// (PlayAnimation_Factory 0x14014a5..: MemAlloc -> Internal::Internal -> zero
// type fields -> install per-type vtable). All Action subtypes are 0x40 bytes.

#include "action_base_standalone.h"
#include "engine_funcs.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// slot0 (+0x00) ScalarDtor — generic Internal teardown (exe 0x140001260).
// The exe also rewrites the vtable to the Action base before destruction; that
// is cosmetic (no virtual dispatch happens during base teardown), so we skip it.
extern "C" void* Action_ScalarDtor(void* self, unsigned int flags) {
    if (!self) return self;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (*(void**)(s + 0x10) != nullptr && fn_InternalBasePtr_DeletePtr)
        reinterpret_cast< ::Object::InternalBasePtr*>(s + 0x10)->DeletePtr();   // B.3.3 bridge
    if (fn_ObjectInternal_Dtor) fn_ObjectInternal_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// slot1 (+0x08) — exe 0x1400011e0 returns DAT_1401c3df8, which is statically null.
extern "C" void* Action_Slot1(void) { return nullptr; }

// slot5 (+0x28) base Execute — exe 0x1400c4a90 returns 0.
extern "C" int Action_BaseExecute(void) { return 0; }

// slot6/slot7 (+0x30/+0x38) Enter/Exit — exe nops.
extern "C" void Action_Enter(void) {}
extern "C" void Action_Exit(void) {}

// slot8 (+0x40) base CheckEntity — exe 0x1400c4a30 is a void nop. Reused for the
// handful of per-type CheckEntity slots the exe leaves as a 3-byte void no-op.
extern "C" void Action_BaseCheckEntity(void) {}

// slot9 (+0x48) base GetTarget — exe 0x1400c4a40 returns 0 (false).
extern "C" int Action_BaseGetTarget(void) { return 0; }

// Shared in-place init. Object::Internal::Internal sets up the 0x20-byte base;
// the exe factory then zeroes the type-specific fields and installs the vtable.
// Zeroing [0x10, size) covers exactly the fields the exe clears after the base
// ctor (it explicitly re-zeros +0x10/+0x18, then the type fields above 0x20).
extern "C" void* Action_InitInPlace(void* p, int size, void* vtable) {
    if (!p) return nullptr;
    if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(p);
    uint8_t* s = reinterpret_cast<uint8_t*>(p);
    for (int i = 0x10; i < size; ++i) s[i] = 0;
    *(void**)p = vtable;
    return p;
}

extern "C" void* Action_Alloc(int size, void* vtable) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(size), 8);
    if (!p) return nullptr;
    return Action_InitInPlace(p, size, vtable);
}

