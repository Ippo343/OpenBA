// code/Game/Component/citizenaidescription.cpp
// Clean reimplementation of CitizenAIDescription (an Object::External "Description").
//
// CitizenAIDescription (0x20 bytes) — extends Object::External (0x20 base).
// Vtable at 0x1401624f0.
//
// This description carries NO serialized fields — the CitizenAI behaviour is
// entirely code-driven. Factory (0x14014e750 area) allocates 0x20 bytes, runs the
// External base ctor, sets the vtable. Serialize (0x14010fc40) just constructs +
// destructs an IO::Serializer (reads nothing). GetVersion (0x14010fc80) = 1.
//
// Decompiled from Application-x64.exe (read-only Ghidra recon).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace {
constexpr uintptr_t kVtableAddr    = 0x1401624f0;
constexpr uintptr_t kSerializeAddr = 0x14010fc40;
constexpr uint32_t  kFileVersion   = 1;
} // namespace

// ----- ctor / init -------------------------------------------------------
static void* CitizenAIDescription_Ctor_Impl(void* mem) {
    // Object::External base ctor (sets base vtable + zeroes base fields).
    if (fn_External_External) fn_External_External(mem);
    // Set our type vtable.
    uintptr_t vt = ResolveAddr(Module::Exe, kVtableAddr);
    if (vt) *reinterpret_cast<void**>(mem) = reinterpret_cast<void*>(vt);
    return mem;
}

// ----- Serialize (vtable +0x28) -----------------------------------------
// No fields. We still create + destroy a Serializer for stream-state parity
// with the original (advances no bytes, identical behaviour).
void CitizenAIDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    (void)self;
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    g_fn_Serializer_Dtor(serializer);
#else
    typedef void (*fn_t)(void*, void*);
    fn_t orig = reinterpret_cast<fn_t>(ResolveAddr(Module::Exe, kSerializeAddr));
    if (orig) orig(self, stream);
#endif
}

// ----- Dtor (vtable +0x00) ----------------------------------------------
void* CitizenAIDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kVtableAddr);
    if (vt) *reinterpret_cast<void**>(self) = reinterpret_cast<void*>(vt);
    // 0x20 = External base only, no heap-owning fields to release.
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// ============================================================
// STANDALONE_EXE: Factory + Vtable + REGISTER_TYPE
// ============================================================
#ifdef STANDALONE_EXE

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t CitizenAIDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("CitizenAIDescription");
    return cached;
}

static void* CitizenAIDescription_Dtor_Tramp(void* self, uint32_t flags) {
    return CitizenAIDescription_Dtor(self, flags);
}
static void  CitizenAIDescription_Serialize_Tramp(void* self, void* stream) {
    CitizenAIDescription_Serialize(self, stream);
}
static int   CitizenAIDescription_VtblReturn1(void* /*self*/) { return (int)kFileVersion; } // GetVersion
static int   CitizenAIDescription_VtblReturn0(void* /*self*/) { return 0; }
static void* CitizenAIDescription_VtblReturnNull(void* /*self*/, ...) { return nullptr; }

extern "C" void* CitizenAIDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    return CitizenAIDescription_Ctor_Impl(memory);
}

static void  CitizenAIDescription_Cleanup_Tramp(void* self) {
    typedef void* (*dtor_t)(void*, uint32_t);
    dtor_t d = *reinterpret_cast<dtor_t*>(*reinterpret_cast<uintptr_t*>(self));
    if (d) d(self, 0);
    CitizenAIDescription_Ctor_Impl(self);
}
static void* CitizenAIDescription_Slot8_Tramp(void* self) {
    if (!self) return nullptr;
    CitizenAIDescription_Ctor_Impl(self);
    return self;
}

// Vtable @ 0x1401624f0 (Object::External, verified via read_memory):
//   +0x00 Dtor 0x14002fc90 | +0x08 GetTypeInfo 0x14002fb70 | +0x10 GetTypeId 0x14002fb60
//   +0x18 GetVersion 0x14010fc80 (mov eax,1) | +0x20 Compile 0x14003a912
//   +0x28 Serialize 0x14010fc40 (empty) | +0x30 Cleanup 0x14002fc30
//   +0x38 GetDesc 0x140001640 | +0x40 Slot8 0x14002fc80 | +0x48 0x14002fbe0
static void* g_CitizenAIDescription_Vtable[11] = {
    reinterpret_cast<void*>(&CitizenAIDescription_Dtor_Tramp),      // +0x00
    reinterpret_cast<void*>(&CitizenAIDescription_VtblReturnNull),  // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&CitizenAIDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&CitizenAIDescription_VtblReturn1),     // +0x18 GetVersion
    reinterpret_cast<void*>(&CitizenAIDescription_VtblReturn0),     // +0x20 Compile
    reinterpret_cast<void*>(&CitizenAIDescription_Serialize_Tramp), // +0x28
    reinterpret_cast<void*>(&CitizenAIDescription_Cleanup_Tramp),   // +0x30
    reinterpret_cast<void*>(&CitizenAIDescription_VtblReturn0),     // +0x38 GetDesc
    reinterpret_cast<void*>(&CitizenAIDescription_Slot8_Tramp),     // +0x40
    reinterpret_cast<void*>(&CitizenAIDescription_VtblReturnNull),  // +0x48 guard
    reinterpret_cast<void*>(&CitizenAIDescription_VtblReturnNull),  // +0x50 guard
};

extern "C" void RegisterCitizenAIDescriptionVtable() {
    RegisterExeAddr(kVtableAddr, reinterpret_cast<uintptr_t>(g_CitizenAIDescription_Vtable));
}

extern "C" void* CitizenAIDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x20, 8);
    if (!p) return nullptr;
    return CitizenAIDescription_Ctor_Impl(p);
}

REGISTER_TYPE(CitizenAIDescription, "Description",
              &CitizenAIDescription_Factory, &CitizenAIDescription_CreateInPlace);

#endif // STANDALONE_EXE
