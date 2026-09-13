// code/Game/Component/component_factory_base.cpp
// Shared inert-component factory infrastructure — see component_factory_base.h.

#include "Component/component_factory_base.h"
#include "Object/internal_factory_audit.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstring>
#include <cstdlib>


// Ported Component base Serialize (absorbresourcecomponent.cpp, retail 0x1401189b0).
void Component_SerializeReimpl(void* self, void* stream);

namespace {

// Shared scalar-deleting destructor (vtable slot +0x00). For a freshly-spawned
// inert component the body is zeroed (no owned arrays/refs to release), so this
// only frees the object itself when the deleting flag (bit 0) is set — matching
// the tail of every component's *_ScalarDtor.
//
// InitInert/InitInertTyped run the real Object::Internal::Internal, which enters the
// object into the InternalFactory handle table (obj+0x08 = index, obj+0x0c = generation).
// Freeing without the matching ~Internal leaves a dangling table entry: the next
// unrelated ~Internal swap-removes it and executes `*(int*)(freedPtr+0x08) = idx`,
// a 4-byte store into a block the allocator has already reused. That is the
// demolish-churn heap corruption (see Object/internal_factory_audit.h mode (A)).
// Every real component's *_ScalarDtor in the exe ends in ~Internal, so calling it
// here is also the faithful behavior.
// Guarded by IsRegistered: if fn_ObjectInternal_Ctor was NULL when this instance was
// built (or it came from some other path), obj+0x08 is heap garbage and calling
// ~Internal would swap-remove through that garbage index — corruption mode (B).
// Only objects genuinely present in the table are unregistered.
void* Inert_ScalarDtor(void* self, uint32_t flags) {
    if (self && fn_ObjectInternal_Dtor && ObjAudit::IsRegistered(self)) {
        fn_ObjectInternal_Dtor(self);
    }
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// Safe no-op for every non-dtor slot. Returns 0 in RAX, which reads correctly as
// void / int / pointer under the Win64 ABI regardless of the slot's real
// signature.
void* Inert_Stub(void*, ...) { return nullptr; }

// One vtable shared by every inert component that does NOT need a real GetTypeId
// (legacy InitInert path). 48 slots is comfortable headroom over the largest real
// component vtable, so any in-range virtual call lands on a stub rather than
// reading past the array.
void* g_InertComponentVtable[ComponentFactory::kInertVtableSlots] = {
    reinterpret_cast<void*>(&Inert_ScalarDtor),  // +0x00 ScalarDtor
    // slots +0x08 .. — filled below at first use.
};
bool g_vtableInit = false;

// Component base stream-Serialize (retail 0x1401189b0, ported in
// absorbresourcecomponent.cpp). Save/load wave 2026-08-27: slot 3 (+0x18) must
// NOT be an inert no-op — the save/load persist machinery vcalls it on every
// component, and retail's base restores the owner-entity IBP (+0x10), the
// capability flags (+0x20) and the description ref (+0x18). A no-op left every
// unported component with a NULL owner after load (the post-load +0x58 refresh
// then crashed in the first component that dereferenced it). Types with a real
// ported Serialize still win — their InstallLive/Finish fills overwrite [3]
// after FillTypedVtable runs.
void Component_SerializeReimpl_Fwd(void* self, void* stream) {
    ::Component_SerializeReimpl(self, stream);
}

void EnsureVtable() {
    if (g_vtableInit) return;
    for (size_t i = 1; i < ComponentFactory::kInertVtableSlots; ++i)
        g_InertComponentVtable[i] = reinterpret_cast<void*>(&Inert_Stub);
    g_InertComponentVtable[3] = reinterpret_cast<void*>(&Component_SerializeReimpl_Fwd);
    g_vtableInit = true;
}

// Fill a per-type inert vtable (Foundation E): every slot a stub except +0x00
// (scalar dtor) and +0x10 (the type's real GetTypeId). Idempotent — slot +0x10 is
// the fill sentinel, so this runs once per type and is reused for every instance.
void FillTypedVtable(void** vt, void* getTypeIdFn) {
    if (vt[2] == getTypeIdFn) return;            // already filled for this type
    vt[0] = reinterpret_cast<void*>(&Inert_ScalarDtor);   // +0x00 ScalarDtor
    for (size_t i = 1; i < ComponentFactory::kInertVtableSlots; ++i)
        vt[i] = reinterpret_cast<void*>(&Inert_Stub);
    vt[2] = getTypeIdFn;                          // +0x10 GetTypeId (real)
    vt[3] = reinterpret_cast<void*>(&Component_SerializeReimpl_Fwd);  // +0x18 base Serialize
}

}  // namespace

namespace ComponentFactory {

// The address every inert slot carries. Exposed so harnesses can tell a slot that has
// been reimplemented from one still holding the shared stub — the vtable array itself
// lives in an anonymous namespace and is not reachable from outside this file, and
// "is this slot live" is otherwise unanswerable from a probe.
void* InertStubSlot() {
    EnsureVtable();
    return g_InertComponentVtable[1];
}

// The base-only stream-Serialize every typed vtable starts with (slot [3]).
// Exposed so the slot-3 inventory dump (factory_registration.cpp) can tell a
// class with a REAL wired serializer from one silently losing its per-instance
// state on save/load (save/load wave 2026-08-28 — this trap shipped 21+ times).
void* BaseSerializeSlot() {
    EnsureVtable();
    return g_InertComponentVtable[3];
}

void InitInert(void* self, size_t size, uint32_t flags) {
    EnsureVtable();
    // Object::Internal base (sets +0x00 vtable + internal linkage at +0x08).
    if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(self);

    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(void**)s            = g_InertComponentVtable;  // +0x00 our shared vtable
    *(void**)(s + 0x10)   = nullptr;                 // +0x10 owner IBP
    *(void**)(s + 0x18)   = nullptr;                 // +0x18 description
    *(uint32_t*)(s + 0x20) = flags;                  // +0x20 capability flags

    // Zero the component body (everything past the Component base, +0x28..size).
    // The Object::Internal base fields (+0x00 vtable, +0x08 linkage) and the
    // Component base (+0x10/+0x18/+0x20) are set explicitly above.
    if (size > 0x28)
        std::memset(s + 0x28, 0, size - 0x28);
}

void* MakeInert(size_t size, uint32_t flags) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)size, 8);
    if (!p) return nullptr;
    InitInert(p, size, flags);
    return p;
}

void InitInertTyped(void* self, size_t size, uint32_t flags,
                    void** vtable, void* getTypeIdFn) {
    FillTypedVtable(vtable, getTypeIdFn);
    if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(self);

    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(void**)s            = vtable;                   // +0x00 per-type vtable
    *(void**)(s + 0x10)   = nullptr;                  // +0x10 owner IBP
    *(void**)(s + 0x18)   = nullptr;                  // +0x18 description
    *(uint32_t*)(s + 0x20) = flags;                   // +0x20 capability flags
    if (size > 0x28)
        std::memset(s + 0x28, 0, size - 0x28);
}

void* MakeInertTyped(size_t size, uint32_t flags,
                     void** vtable, void* getTypeIdFn) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)size, 8);
    if (!p) return nullptr;
    InitInertTyped(p, size, flags, vtable, getTypeIdFn);
    return p;
}


}  // namespace ComponentFactory

