// code/Game/Component/component_factory_base.cpp
// Shared inert-component factory infrastructure — see component_factory_base.h.

#include "Component/component_factory_base.h"
#include "Object/internal_factory_audit.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstring>
#include <cstdlib>


namespace {

// Kill-switch for the Object::Internal unregistration below (BANISHED_INTERNAL_UNREG=0
// restores the old, leaking behavior for an A/B run against the ObjAudit tally).
bool UnregisterOnFree() {
    static int cached = -1;
    if (cached < 0) {
        const char* v = std::getenv("BANISHED_INTERNAL_UNREG");
        cached = (v && (std::strcmp(v, "0") == 0 || std::strcmp(v, "false") == 0)) ? 0 : 1;
    }
    return cached != 0;
}

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
    if (self && UnregisterOnFree() && fn_ObjectInternal_Dtor && ObjAudit::IsRegistered(self)) {
        fn_ObjectInternal_Dtor(self);
    } else {
        ObjAudit::NoteUnregisteredFree(self, "InertComponent");
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

void EnsureVtable() {
    if (g_vtableInit) return;
    for (size_t i = 1; i < ComponentFactory::kInertVtableSlots; ++i)
        g_InertComponentVtable[i] = reinterpret_cast<void*>(&Inert_Stub);
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
}

}  // namespace

namespace ComponentFactory {

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

// Live-feature check for COMPONENT_LIVE_FACTORY and every BANISHED_LIVE_* gate.
// Policy (2026-07-19): once a feature is verified working it goes DEFAULT-ON via
// kDefaultOnFlags below; the env var then only serves as a kill-switch (=0/false).
// Flags NOT in the table keep the original opt-in behavior (env var or the master
// BANISHED_LIVE_COMPONENTS must be set), so unproven/harness gates stay dormant.
static bool EnvOn(const char* v) {
    return v && v[0] && std::strcmp(v, "0") != 0 && std::strcmp(v, "false") != 0;
}
// Verified-working features: the Makefile's former SA_LIVE_ENV set (all
// "user-verified working" per its comment) + BANISHED_LIVE_CITIZEN_CARRY
// (promoted per its own "fold into SA_LIVE_ENV once verified" note).
static const char* const kDefaultOnFlags[] = {
    "BANISHED_LIVE_TOOLBAR",
    "BANISHED_GAME_PRELOAD",
    "BANISHED_LIVE_STATUSICON",
    "BANISHED_LIVE_STATUSICON_DRAW",
    "BANISHED_LIVE_CITIZEN_STATUSICON",
    "BANISHED_LIVE_CITIZEN",
    "BANISHED_LIVE_CITIZEN_UPDATE",
    "BANISHED_LIVE_CITIZENAI",
    "BANISHED_LIVE_CHARACTER",
    "BANISHED_LIVE_CHARACTER_MOVETICK",
    "BANISHED_LIVE_STATEMACHINE",
    "BANISHED_LIVE_RESIDENCE",
    "BANISHED_LIVE_HOUSING",
    "BANISHED_LIVE_STORAGE",
    "BANISHED_LIVE_REGIONMAP",
    "BANISHED_LIVE_STORAGEDISPLAY",
    "BANISHED_LIVE_DECAL",
    "BANISHED_LIVE_TOOLS",
    "BANISHED_LIVE_PICKING",
    "BANISHED_LIVE_PARTICLE",
    "BANISHED_SELECTION_OUTLINE",
    "BANISHED_LIVE_EVENTS",
    "BANISHED_LIVE_CITIZEN_CARRY",
    // Organic construction pipeline — verified end-to-end 2026-07-22 (foundation ->
    // real gather/haul -> real build labor via MsgReceive -> completion -> staffed).
    // The three were formerly independent opt-ins; graduated together as one feature.
    "BANISHED_LIVE_BUILDCTRL",     // BuildComponent state machine (foundation->1->8->2)
    "BANISHED_BUILD_LIVE_HAUL",    // posts gather + build work to the citizen ledger
    "BANISHED_BUILDER_NATURAL",    // site->profession BST + tick-driven Builder pool
};
bool LiveEnabled(const char* envVar) {
    if (envVar) {
        const char* v = std::getenv(envVar);
        if (v && v[0]) return EnvOn(v);  // explicit setting wins, on OR off
        for (const char* f : kDefaultOnFlags)
            if (std::strcmp(f, envVar) == 0) return true;
    }
    return EnvOn(std::getenv("BANISHED_LIVE_COMPONENTS"));
}

}  // namespace ComponentFactory

