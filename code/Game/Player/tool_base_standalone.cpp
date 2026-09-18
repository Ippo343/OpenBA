// code/Game/Player/tool_base_standalone.cpp
// Shared standalone factory/vtable helpers for the Tool family.
// See tool_base_standalone.h and notes/FACTORY_CASCADE_145_BRIEF.md.
//
// Mirrors the common <Tool>_Factory body (e.g. BridgeTool_Factory 0x14002dc40-ish:
// MemAlloc(size,8) -> <Tool>_Ctor) and the small shared base vtable slots
// (slot1 -> null, slot3 -> Object::Internal::Persist, purecall placeholders).

#include "tool_base_standalone.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// slot0 ScalarDtor — generic Internal teardown (used by ToolSetting types).
extern "C" void* Tool_ScalarDtor(void* self, unsigned int flags) {
    if (!self) return self;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (*(void**)(s + 0x10) != nullptr && fn_InternalBasePtr_DeletePtr)
        reinterpret_cast< ::Object::InternalBasePtr*>(s + 0x10)->DeletePtr();   // B.3.3 bridge
    if (fn_ObjectInternal_Dtor) fn_ObjectInternal_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// slot1 (+0x08) — exe 0x14002d790 et al. return DAT_1401c3df8 (statically null).
extern "C" void* Tool_Slot1(void) { return nullptr; }

// slot3 (+0x18) Serialize — exe 0x14003a95a Object::Internal::Persist base.
// No-op for a fresh dormant object.
extern "C" void* Tool_Persist(void) { return nullptr; }

// generic safe stub for any non-reimplemented slot.
extern "C" void* Tool_Ret0(void) { return nullptr; }

extern "C" void* Tool_InitInPlace(void* p, int size, void* vtable, int dragBase) {
    if (!p) return nullptr;
    if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(p);
    uint8_t* s = reinterpret_cast<uint8_t*>(p);
    for (int i = 0x10; i < size; ++i) s[i] = 0;
    if (dragBase && size >= 0x38) {
        // DragTool base: startCell/endCell/lastEndCell default to -1 (bytes
        // [0x2c,0x38)); dragState/singleClick (+0x28) stay 0.
        std::memset(s + 0x2c, 0xFF, 0x38 - 0x2c);
    }
    *(void**)p = vtable;
    return p;
}

extern "C" void* Tool_Alloc(int size, void* vtable, int dragBase) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(size), 8);
    if (!p) return nullptr;
    return Tool_InitInPlace(p, size, vtable, dragBase);
}

