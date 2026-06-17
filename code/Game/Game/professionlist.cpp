// code/Game/Game/professionlist.cpp
// Clean reimplementation of ProfessionList game logic.
//
// ProfessionList (0x50 bytes) -- container for profession references.
// Extends Object::External (0x20 base).
// Vtable at 0x14015ef50.
//
// Contains a dynamic array at +0x20/+0x28 and four ExternalPtr<Profession>
// references at +0x30, +0x38, +0x40, +0x48.
//
// Decompiled from Application-x64.exe.

#include "Game/professionlist_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Local function pointers resolved at init time
// ============================================================

// IO::Serializer::Ctor(self, stream, version)
static void (*s_fn_Serializer_Ctor)(void* self, void* stream, int version) = nullptr;
// IO::Serializer::Dtor(self)
static void (*s_fn_Serializer_Dtor)(void* self) = nullptr;

// FUN_140018020(ExternalPtr<Profession>*, Serializer*) -- typed ExternalPtr serialize
// This is a specialized serializer that checks IsDerivedFrom(TypeInfo_Profession).
static void (*s_fn_ExternalPtrProf_Serialize)(void* field, void* serializer) = nullptr;

static bool s_funcsInited = false;

// ============================================================
// ProfessionList_InitFuncs -- resolve local pointers
// ============================================================
void ProfessionList_InitFuncs() {
    if (s_funcsInited) return;
    s_funcsInited = true;

    uintptr_t iat;

    // Serializer ctor/dtor via IAT
    iat = ResolveAddr(Module::Exe, 0x140152ad8);
    if (iat) s_fn_Serializer_Ctor = *reinterpret_cast<void(**)(void*, void*, int)>(iat);
    iat = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iat) s_fn_Serializer_Dtor = *reinterpret_cast<void(**)(void*)>(iat);

    // FUN_140018020 -- specialized ExternalPtr<Profession> serialize (direct exe function)
    s_fn_ExternalPtrProf_Serialize = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x140018020));

    // Standalone fallbacks: the exe IAT dispatch slots resolve to NULL when the
    // original exe isn't mapped. Fall back to the Runtime-resolved engine_funcs
    // globals so the SAME Serialize works in both modes.
    if (!s_fn_Serializer_Ctor) s_fn_Serializer_Ctor = reinterpret_cast<void(*)(void*, void*, int)>((uintptr_t)g_fn_Serializer_Ctor);
    if (!s_fn_Serializer_Dtor) s_fn_Serializer_Dtor = reinterpret_cast<void(*)(void*)>((uintptr_t)g_fn_Serializer_Dtor);
    if (!s_fn_ExternalPtrProf_Serialize) s_fn_ExternalPtrProf_Serialize = reinterpret_cast<void(*)(void*, void*)>((uintptr_t)fn_ExternalPtr_BuildSerialize);

#ifdef _DEBUG
    printf("[ProfessionList] InitFuncs: SerCtor=%p SerDtor=%p ExtPtrProfSer=%p\n",
           (void*)s_fn_Serializer_Ctor, (void*)s_fn_Serializer_Dtor,
           (void*)s_fn_ExternalPtrProf_Serialize);
#endif
}

// ============================================================
// ProfessionList::Init -- Initialize all fields to defaults
// ============================================================
// Mirrors the inline ctor from ProfessionList_Factory (0x14001e5c0).
// Calls Object::External::External for base init, then zeros all fields.
void ProfessionList_Init(ProfessionList* self) {
    // Initialize External base class
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set ProfessionList vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, ProfessionListAddr::Vtable);
    self->vtable = s_vtable;

    // Zero all ProfessionList-specific fields
    self->arrayData = nullptr;
    self->arraySize = 0;
    self->profRef1 = nullptr;
    self->profRef2 = nullptr;
    self->profRef3 = nullptr;
    self->profRef4 = nullptr;
}

// ============================================================
// ProfessionList::Dtor (vtable +0x00) -- Scalar deleting destructor
// ============================================================
// Original: 0x14001e760
// Frees the dynamic array at +0x20, zeros +0x20 and +0x28,
// then calls External::~External. Conditionally frees self.
void* ProfessionList_Dtor(ProfessionList* self, uint32_t flags) {
    // Free the dynamic array
    if (self->arrayData) {
        fn_MemFreeStatic(self->arrayData);
    }
    self->arrayData = nullptr;
    self->arraySize = 0;

    // Call External::~External for base cleanup
    if (fn_External_Dtor) {
        fn_External_Dtor(self);
    }

    // Conditional free
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// ProfessionList::Cleanup (vtable +0x30, Slot6) -- Reset to defaults
// ============================================================
// Original: 0x14001e690
// Calls Dtor via vtable with flags=0, then re-inits.
void ProfessionList_Cleanup(ProfessionList* self) {
    // Call Dtor with flags=0 (don't free memory)
    typedef void* (*fn_VtableDtor_t)(void* self, uint32_t flags);
    fn_VtableDtor_t dtor = *(fn_VtableDtor_t*)(*(uintptr_t*)self);
    dtor(self, 0);

    // Re-construct in place
    ProfessionList_Init(self);
}

// ============================================================
// ProfessionList::Slot8 (vtable +0x40) -- Reset/reinit with null check
// ============================================================
// Original: 0x14001e630
// If self is null, returns null. Otherwise re-inits to defaults.
void* ProfessionList_Slot8(ProfessionList* self) {
    if (!self) return nullptr;
    ProfessionList_Init(self);
    return self;
}

// ============================================================
// ProfessionList::Serialize (vtable +0x28, Slot5) -- Serialize refs
// ============================================================
// Original: 0x1400a8e30
// Creates a Serializer(stream, version=0), then serializes four
// ExternalPtr<Profession> refs at +0x30, +0x38, +0x40, +0x48
// using FUN_140018020 (typed ExternalPtr serialize).
void ProfessionList_Serialize(ProfessionList* self, void* stream) {
    ProfessionList_InitFuncs();   // idempotent; ensures serializer fns resolved (standalone)
    if (!s_fn_Serializer_Ctor || !s_fn_Serializer_Dtor) {
        return;
    }

    // Stack-allocated serializer (24 bytes)
    uint8_t serializer[24];
    s_fn_Serializer_Ctor(serializer, stream, 0);

    // Serialize four ExternalPtr<Profession> references
    if (s_fn_ExternalPtrProf_Serialize) {
        s_fn_ExternalPtrProf_Serialize(&self->profRef1, serializer);  // +0x30
        s_fn_ExternalPtrProf_Serialize(&self->profRef2, serializer);  // +0x38
        s_fn_ExternalPtrProf_Serialize(&self->profRef3, serializer);  // +0x40
        s_fn_ExternalPtrProf_Serialize(&self->profRef4, serializer);  // +0x48
    }

    s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// Standalone factory scaffolding
// ============================================================
// ProfessionList (Object::External, 0x50) is loaded from Profession.rsc during the
// in-game world build (GameUpdateLoop::Load step 1-4 / climate setup). In standalone
// it was a NullFactory stub → IsDerivedFrom(NULL) crash @Runtime+0xC039A. Real factory
// + the reimpl'd Serialize above (loads the 4 Profession refs) make it loadable.
// vtable @ 0x14015ef50 (9 slots); GetVersion=2, Serialize +0x28, factory 0x14001e5c0.
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t ProfessionList_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ProfessionList");
    return cached;
}
static int   PL_Return2(void* /*self*/) { return 2; }   // GetVersion = 2
static int   PL_Return0(void* /*self*/) { return 0; }
static void* PL_ReturnNull(void*, ...)  { return nullptr; }

static void* g_PL_Vtable[9] = {
    reinterpret_cast<void*>(&ProfessionList_Dtor),      // +0x00 Dtor
    reinterpret_cast<void*>(&PL_ReturnNull),            // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&ProfessionList_GetTypeId), // +0x10 GetTypeId (used by IsDerivedFrom)
    reinterpret_cast<void*>(&PL_Return2),               // +0x18 GetVersion=2
    reinterpret_cast<void*>(&PL_Return0),               // +0x20 Compile thunk
    reinterpret_cast<void*>(&ProfessionList_Serialize), // +0x28 Serialize
    reinterpret_cast<void*>(&ProfessionList_Cleanup),   // +0x30 Cleanup
    reinterpret_cast<void*>(&PL_Return0),               // +0x38
    reinterpret_cast<void*>(&ProfessionList_Slot8),     // +0x40 CreateInPlace
};

extern "C" void RegisterProfessionListVtable() {
    RegisterExeAddr(ProfessionListAddr::Vtable, reinterpret_cast<uintptr_t>(g_PL_Vtable));
}

extern "C" void* ProfessionList_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x50, 8);
    if (!p) return nullptr;
    ProfessionList_Init(reinterpret_cast<ProfessionList*>(p));
    return p;
}
extern "C" void* ProfessionList_CreateInPlace(void* mem) {
    if (!mem) return nullptr;
    ProfessionList_Init(reinterpret_cast<ProfessionList*>(mem));
    return mem;
}

REGISTER_TYPE(ProfessionList, "Object::External",
              &ProfessionList_Factory, &ProfessionList_CreateInPlace);
#endif
