// code/Game/Game/profession.cpp
// Clean reimplementation of Profession game logic.
//
// Profession (0x60 bytes) -- citizen profession definition.
// Extends Object::External (0x20 base).
// Vtable at 0x14015efa0.
//
// Defines a profession with name, description, priority, and skill multipliers.
// Fields include ExternalPtrs at +0x20/+0x28, System::Strings at +0x30/+0x38/+0x40,
// and numeric fields (int, bools, floats) from +0x48 to +0x58.
//
// Decompiled from Application-x64.exe.

#include "Game/profession_funcs.h"
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

// IO::Serializer::Serialize(int*) via IAT at 0x140152ac8
static void (*s_fn_Serialize_Int)(void* ser, void* field, const char* name) = nullptr;
// IO::Serializer::Serialize(bool*) via IAT at 0x140152a58
static void (*s_fn_Serialize_Bool)(void* ser, void* field, const char* name) = nullptr;
// IO::Serializer::Serialize(float*) via IAT at 0x140152a60
static void (*s_fn_Serialize_Float)(void* ser, void* field, const char* name) = nullptr;

// System::String::Serialize(this, serializer)
static void (*s_fn_String_Serialize)(void* str, void* serializer) = nullptr;
// System::String::~String(this)
static void (*s_fn_String_Dtor)(void* str) = nullptr;

// FUN_14000bf40(ExternalPtr*, Serializer*) -- ExternalPtr serialize variant
static void (*s_fn_ExternalPtr_Serialize2)(void* field, void* serializer) = nullptr;
// FUN_14000bd30(ExternalPtr*, Serializer*) -- ExternalPtr serialize
static void (*s_fn_ExternalPtr_Serialize)(void* field, void* serializer) = nullptr;

static bool s_funcsInited = false;

// ============================================================
// Profession_InitFuncs -- resolve local pointers
// ============================================================
void Profession_InitFuncs() {
    if (s_funcsInited) return;
    s_funcsInited = true;

    uintptr_t iat;

    // Serializer ctor/dtor
    iat = ResolveAddr(Module::Exe, 0x140152ad8);
    if (iat) s_fn_Serializer_Ctor = *reinterpret_cast<void(**)(void*, void*, int)>(iat);
    iat = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iat) s_fn_Serializer_Dtor = *reinterpret_cast<void(**)(void*)>(iat);

    // Serialize overloads
    iat = ResolveAddr(Module::Exe, 0x140152ac8);
    if (iat) s_fn_Serialize_Int = *reinterpret_cast<void(**)(void*, void*, const char*)>(iat);
    iat = ResolveAddr(Module::Exe, 0x140152a58);
    if (iat) s_fn_Serialize_Bool = *reinterpret_cast<void(**)(void*, void*, const char*)>(iat);
    iat = ResolveAddr(Module::Exe, 0x140152a60);
    if (iat) s_fn_Serialize_Float = *reinterpret_cast<void(**)(void*, void*, const char*)>(iat);

    // String funcs from engine_funcs globals
    s_fn_String_Serialize = reinterpret_cast<void(*)(void*, void*)>((uintptr_t)g_fn_String_Serialize);
    s_fn_String_Dtor = reinterpret_cast<void(*)(void*)>((uintptr_t)fn_String_Dtor);

    // ExternalPtr serialize helpers (direct exe functions)
    s_fn_ExternalPtr_Serialize2 = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x14000bf40));
    s_fn_ExternalPtr_Serialize = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x14000bd30));

    // Standalone fallbacks: the exe IAT dispatch slots above resolve to NULL when
    // the original exe isn't mapped (standalone). Fall back to the Runtime-resolved
    // engine_funcs globals so the SAME Serialize works in both modes. The
    // ExternalPtr variants are stream-identical to fn_ExternalPtr_BuildSerialize.
    if (!s_fn_Serializer_Ctor)  s_fn_Serializer_Ctor  = reinterpret_cast<void(*)(void*, void*, int)>((uintptr_t)g_fn_Serializer_Ctor);
    if (!s_fn_Serializer_Dtor)  s_fn_Serializer_Dtor  = reinterpret_cast<void(*)(void*)>((uintptr_t)g_fn_Serializer_Dtor);
    if (!s_fn_Serialize_Int)    s_fn_Serialize_Int    = reinterpret_cast<void(*)(void*, void*, const char*)>((uintptr_t)g_fn_Serialize_Int);
    if (!s_fn_Serialize_Bool)   s_fn_Serialize_Bool   = reinterpret_cast<void(*)(void*, void*, const char*)>((uintptr_t)g_fn_Serialize_Bool);
    if (!s_fn_Serialize_Float)  s_fn_Serialize_Float  = reinterpret_cast<void(*)(void*, void*, const char*)>((uintptr_t)fn_Serializer_Serialize_Float);
    if (!s_fn_ExternalPtr_Serialize2) s_fn_ExternalPtr_Serialize2 = reinterpret_cast<void(*)(void*, void*)>((uintptr_t)fn_ExternalPtr_BuildSerialize);
    if (!s_fn_ExternalPtr_Serialize)  s_fn_ExternalPtr_Serialize  = reinterpret_cast<void(*)(void*, void*)>((uintptr_t)fn_ExternalPtr_BuildSerialize);

#ifdef _DEBUG
    printf("[Profession] InitFuncs: SerCtor=%p SerDtor=%p SerInt=%p SerBool=%p SerFloat=%p\n",
           (void*)s_fn_Serializer_Ctor, (void*)s_fn_Serializer_Dtor,
           (void*)s_fn_Serialize_Int, (void*)s_fn_Serialize_Bool, (void*)s_fn_Serialize_Float);
    printf("[Profession] InitFuncs: StrSer=%p StrDtor=%p ExtPtrSer=%p ExtPtrSer2=%p\n",
           (void*)s_fn_String_Serialize, (void*)s_fn_String_Dtor,
           (void*)s_fn_ExternalPtr_Serialize, (void*)s_fn_ExternalPtr_Serialize2);
#endif
}

// ============================================================
// Profession::Init -- Initialize all fields to defaults
// ============================================================
// Mirrors Profession_Ctor at 0x1400a9030.
// Calls Object::External::External for base init, then sets
// Profession-specific fields to their default values.
void Profession_Init(Profession* self) {
    // Initialize External base class
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set Profession vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, ProfessionAddr::Vtable);
    self->vtable = s_vtable;

    // ExternalPtr fields
    self->externalPtr1 = nullptr;
    self->externalPtr2 = nullptr;

    // String fields (zero = empty COW string)
    self->stringName = 0;
    self->stringDesc = 0;
    self->stringTool = 0;

    // Numeric fields
    self->field48 = 0;
    self->field4C = 0;
    self->field4D = 0;
    self->isAvailable = 1;
    self->priority = 100;
    self->skillMult1 = 1.0f;
    self->skillMult2 = 1.0f;
}

// ============================================================
// Profession::Dtor (vtable +0x00) -- Scalar deleting destructor
// ============================================================
// Original: 0x14001e6f0
// Destroys 3 strings at +0x40, +0x38, +0x30 (in reverse order),
// then calls External::~External for base cleanup.
// Conditionally frees memory if flags & 1.
void* Profession_Dtor(Profession* self, uint32_t flags) {
    // Destroy strings in reverse order (matching original)
    if (s_fn_String_Dtor) {
        s_fn_String_Dtor(&self->stringTool);  // +0x40
        s_fn_String_Dtor(&self->stringDesc);  // +0x38
        s_fn_String_Dtor(&self->stringName);  // +0x30
    }

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
// Profession::Cleanup (vtable +0x30, Slot6) -- Reset to defaults
// ============================================================
// Original: 0x14001e580
// Calls Dtor via vtable with flags=0, then re-constructs via Init.
void Profession_Cleanup(Profession* self) {
    // Call Dtor with flags=0 (don't free memory)
    typedef void* (*fn_VtableDtor_t)(void* self, uint32_t flags);
    fn_VtableDtor_t dtor = *(fn_VtableDtor_t*)(*(uintptr_t*)self);
    dtor(self, 0);

    // Re-construct in place
    Profession_Init(self);
}

// ============================================================
// Profession::Slot8 (vtable +0x40) -- Reset/reinit with null check
// ============================================================
// Original: 0x14001e570
// If self is null, returns null. Otherwise re-inits to defaults.
void* Profession_Slot8(Profession* self) {
    if (!self) return nullptr;
    Profession_Init(self);
    return self;
}

// ============================================================
// Profession::Serialize (vtable +0x28, Slot5) -- Serialize all fields
// ============================================================
// Original: 0x1400a8f10
// Creates a Serializer(stream, version=0), then serializes each field.
void Profession_Serialize(Profession* self, void* stream) {
    Profession_InitFuncs();   // idempotent; ensures serializer fns resolved (standalone)
    if (!s_fn_Serializer_Ctor || !s_fn_Serializer_Dtor) {
        // Cannot serialize without serializer functions
        return;
    }

    // Stack-allocated serializer (24 bytes)
    uint8_t serializer[24];
    s_fn_Serializer_Ctor(serializer, stream, 0);

    // Serialize ExternalPtr fields
    if (s_fn_ExternalPtr_Serialize2) {
        s_fn_ExternalPtr_Serialize2((void*)&self->externalPtr1, serializer);  // +0x20
    }
    if (s_fn_ExternalPtr_Serialize) {
        s_fn_ExternalPtr_Serialize((void*)&self->externalPtr2, serializer);   // +0x28
    }

    // Serialize String fields
    if (s_fn_String_Serialize) {
        s_fn_String_Serialize(&self->stringName, serializer);  // +0x30
        s_fn_String_Serialize(&self->stringDesc, serializer);  // +0x38
        s_fn_String_Serialize(&self->stringTool, serializer);  // +0x40
    }

    // Serialize numeric fields
    if (s_fn_Serialize_Int) {
        s_fn_Serialize_Int(serializer, &self->field48, nullptr);     // +0x48
    }
    if (s_fn_Serialize_Bool) {
        s_fn_Serialize_Bool(serializer, &self->field4C, nullptr);    // +0x4C
        s_fn_Serialize_Bool(serializer, &self->field4D, nullptr);    // +0x4D
    }
    if (s_fn_Serialize_Float) {
        s_fn_Serialize_Float(serializer, &self->skillMult1, nullptr); // +0x54
        s_fn_Serialize_Float(serializer, &self->skillMult2, nullptr); // +0x58
    }
    if (s_fn_Serialize_Int) {
        s_fn_Serialize_Int(serializer, &self->priority, nullptr);     // +0x50
    }
    if (s_fn_Serialize_Bool) {
        s_fn_Serialize_Bool(serializer, &self->isAvailable, nullptr); // +0x4E
    }

    s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// Standalone factory scaffolding
// ============================================================
// Profession is an Object::External type referenced by CitizenDescription (the
// citizen-template closure FindOrLoads Profession\*.rsc). In standalone it was a
// NullFactory stub → crash. Real factory + the reimpl'd Serialize above (now with
// engine-global fallbacks) make it loadable. vtable @ 0x14015efa0 (9 slots,
// +0x00..+0x40; GetVersion=3, Serialize +0x28, CreateInPlace +0x40).
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t Profession_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("Profession");
    return cached;
}
static int   Prof_Return3(void* /*self*/)  { return 3; }   // GetVersion = 3
static int   Prof_Return0(void* /*self*/)  { return 0; }
static void* Prof_ReturnNull(void*, ...)   { return nullptr; }

static void* g_Prof_Vtable[9] = {
    reinterpret_cast<void*>(&Profession_Dtor),       // +0x00 dtor
    reinterpret_cast<void*>(&Prof_ReturnNull),       // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&Profession_GetTypeId),  // +0x10 GetTypeId
    reinterpret_cast<void*>(&Prof_Return3),          // +0x18 GetVersion=3
    reinterpret_cast<void*>(&Prof_Return0),          // +0x20 Compile thunk
    reinterpret_cast<void*>(&Profession_Serialize),  // +0x28 Serialize
    reinterpret_cast<void*>(&Profession_Cleanup),    // +0x30 Cleanup
    reinterpret_cast<void*>(&Prof_Return0),          // +0x38 GetDesc forwarder
    reinterpret_cast<void*>(&Profession_Slot8),      // +0x40 CreateInPlace
};

extern "C" void RegisterProfessionVtable() {
    RegisterExeAddr(ProfessionAddr::Vtable, reinterpret_cast<uintptr_t>(g_Prof_Vtable));
}

extern "C" void* Profession_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x60, 8);
    if (!p) return nullptr;
    Profession_Init(reinterpret_cast<Profession*>(p));
    return p;
}
extern "C" void* Profession_CreateInPlace(void* mem) {
    if (!mem) return nullptr;
    Profession_Init(reinterpret_cast<Profession*>(mem));
    return mem;
}

REGISTER_TYPE(Profession, "Object::External",
              &Profession_Factory, &Profession_CreateInPlace);
#endif
