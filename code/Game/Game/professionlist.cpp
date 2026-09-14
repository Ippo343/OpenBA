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
#include "collection/Collection.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

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
    // globals so the SAME Serialize works in the standalone exe.
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
//
// ENDGAME B.3.2 step 2: the array free is `Collection::Array<void*>::~Array`
// (re-decompiled 2026-09-13: MemFree(+0x20); +0x20 = 0; one qword over +0x28 —
// ArrayDtorImpl exactly; Profession* is trivially destructible so there is no
// element step). Trait-derived row: byteCopy, no init, no destroy, align 8 —
// the same row Startup's append below uses.
void* ProfessionList_Dtor(ProfessionList* self, uint32_t flags) {
    // B.3.2 bridge: ProfessionList (engine/types) still spells the header as
    // `arrayData` + a 64-bit `arraySize` covering count and capacity.
    reinterpret_cast<Collection::Array<void*>*>(&self->arrayData)->~Array();

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
// Returns 1 — matching the original Slot5 (0x1400a8e30) which returns `undefined1`
// (1). The vtable slot is invoked by the engine's External::Load; a `void` return
// here leaves garbage in the upper bytes of EAX (trampoline bug class #7), which
// External::Load may read as the Serialize result. Return a full int so EAX is
// fully defined.
int ProfessionList_Serialize(ProfessionList* self, void* stream) {
    ProfessionList_InitFuncs();   // idempotent; ensures serializer fns resolved (standalone)
    if (!s_fn_Serializer_Ctor || !s_fn_Serializer_Dtor) {
        return 1;
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
    return 1;
}

// ============================================================
// ProfessionList::Startup (0x1400a90a0) — build the profession ARRAY
// ============================================================
// Iterates every Profession external in the ExternalFactory (TypeInfo_Profession
// @ 0x1401c5a98), pushes each with the player-selectable flag (+0x4e) set into
// the dynamic array at +0x20/+0x28/+0x2c (grow ×3), then sorts ascending by the
// profession's sort priority (+0x48) — FUN_14001e7c0 is an introsort on that key;
// an insertion sort gives the same ordering (N ≈ a dozen professions).
//
// Called from GameUpdateLoop::Startup (gameupdateloop.cpp resolves exe 0x1400a90a0
// via fn_ProfessionList_Startup — registered for standalone below). Without it the
// array stays empty (count 0) and every profession-array consumer (TownHallUI
// General/Professions tabs, ProfessionUI) sees zero professions and shows blank.
//
// ENDGAME B.3.2 step 2: the push WAS a hand-written copy of `rtcoll::SetSizeCore`
// in its three-argument form. Retail 0x1400a90a0 (re-decompiled 2026-09-13)
// inlines it with the found Profession* as the fill: `n = count+1; if (n !=
// count) {...}` (the exact-equal early-out, always taken), `_size = n`, `if (cap
// < n) { cap = n; if (count != 0) cap = n*3; MemAlloc(cap << 3, 8) }`, copy the
// prefix on a move else start at count, `dst[i] = i < count ? old[i] : it`, skip
// a null computed address, `if (old != data) MemFree(old)`. The capacity
// spelling `(count != 0) ? n*3 : n` equals the reference `n == old+1 && old != 0
// ? n*3 : n` BECAUSE n is count+1 by construction — this loop is the only
// writer. So it is `Array<void*>::SetSize(count+1, it)` on the trait-derived row
// (byteCopy, no init, no destroy, align 8). The previous port's
// `if (!buf) { count restore; continue; }` was an out-of-memory-only guard
// retail does not have (SetSizeCore skips a null element address instead).
namespace Object { uint32_t GetTypeIdByName(const char* name); }  // fwd (full header below)
extern "C" void ProfessionList_Startup_Impl(void* profList) {
    uintptr_t pl = (uintptr_t)profList;
    if (!pl || !fn_ExternalFactory_FindNext || !fn_IsDerivedFrom ||
        !g_pExternalFactorySingleton || !*g_pExternalFactorySingleton)
        return;

    // B.3.2 bridge: ProfessionList+0x20 is still a raw header in the exe struct.
    auto* profArray = reinterpret_cast<Collection::Array<void*>*>(pl + 0x20);

    // Profession type id: raw .rdata dword when the exe is mapped, else live type tree.
    uint32_t profTid = 0;
    {
        uint32_t* p = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c5a98));
        if (p && *p) profTid = *p;
    }
    if (!profTid) profTid = ::Object::GetTypeIdByName("Profession");
    if (!profTid) return;

    void* factory = *g_pExternalFactorySingleton;
    for (void* it = fn_ExternalFactory_FindNext(factory, profTid, nullptr); it;
         it = fn_ExternalFactory_FindNext(factory, profTid, it)) {
        if (!fn_IsDerivedFrom(it, profTid)) continue;
        if (*(uint8_t*)((uintptr_t)it + 0x4e) == 0) continue;  // not player-selectable

        // Growable push into +0x20 (buf) / +0x28 (count) / +0x2c (cap).
        profArray->SetSize(profArray->GetSize() + 1, it);
    }

    // Sort ascending by sort priority (Profession+0x48) — FUN_14001e7c0 semantics.
    int n = *(int*)(pl + 0x28);
    uintptr_t* arr = *(uintptr_t**)(pl + 0x20);
    if (n > 1 && arr) {
        for (int i = 1; i < n; i++) {
            uintptr_t key = arr[i];
            int keyPri = *(int*)(key + 0x48);
            int j = i - 1;
            while (j >= 0 && *(int*)(arr[j] + 0x48) > keyPri) {
                arr[j + 1] = arr[j];
                j--;
            }
            arr[j + 1] = key;
        }
    }

#ifdef _DEBUG
    printf("[ProfessionList] Startup: %d professions in array (tid=0x%08x)\n",
           *(int*)(pl + 0x28), profTid);
#endif
}

// ============================================================
// Standalone factory scaffolding
// ============================================================
// ProfessionList (Object::External, 0x50) is loaded from Profession.rsc during the
// in-game world build (GameUpdateLoop::Load step 1-4 / climate setup). In standalone
// it was a NullFactory stub → IsDerivedFrom(NULL) crash @Runtime+0xC039A. Real factory
// + the reimpl'd Serialize above (loads the 4 Profession refs) make it loadable.
// vtable @ 0x14015ef50 (9 slots); GetVersion=2, Serialize +0x28, factory 0x14001e5c0.
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
    // ProfessionList::Startup (array populate + sort) — GameUpdateLoop::Startup
    // resolves exe 0x1400a90a0 (fn_ProfessionList_Startup) and calls it when the
    // profession list is live; unregistered it was NULL and the call was skipped,
    // leaving the profession array empty (TownHallUI blank-tab root cause).
    RegisterExeAddr(0x1400a90a0, reinterpret_cast<uintptr_t>(&ProfessionList_Startup_Impl));
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
