// code/Game/Component/destroydescription.cpp
// Clean reimplementation of DestroyDescription (entity sub-description).
//
// DestroyDescription (0x38 bytes) — extends Object::External (0x20 base).
// Parent type name: "Description". Vtable @ 0x1401631b0.  GetVersion = 5.
//
// Field layout. The names are the AUTHORED ones — they are the field-name strings in
// the resource compiler (Tools-x64.exe) and the keys used in Template/*.rsc:
//   +0x20: void* _replaceEntity   ExternalPtr<ComponentDescription> (default 0)
//   +0x28: int32 _damagedSkin     (ctor default 1)  rubble MATERIAL-SET index
//   +0x2c: int32 _damagedDecal    (ctor default 1)  rubble DECAL state
//   +0x30: int32 _minDamageLevel  (NO ctor default) rubble MESH-VARIANT floor
// Consumed by the state-6 collapse — see destroycomponent_state6.cpp.
//
// Serialize (FUN_14011de70) stream order:
//   1. ExternalPtr serialize (+0x20)   FUN_140002340
//   2. int (+0x28)
//   3. int (+0x2c)
//   4. int (+0x30)
//
// ⚠ _minDamageLevel IS GARBAGE IN THE SHIPPED DATA — and that is retail-faithful.
// A 2026-08-23 note claimed the third int "reads off the end of the record". It does
// not. Measured on the live path, the record is 61 decompressed bytes and the three
// int reads land at cursor 41/45/49, all in range. Compiling the kit's own sources
// with Tools-x64.exe proves why: Template/Chapel.rsc (which authors only
// _damagedSkin/_damagedDecal) compiles to a record whose _minDamageLevel slot holds
// 0xE1E1E1E1, while Template/Market.rsc (the ONE template in the whole game that
// authors `int _minDamageLevel = 1`) compiles to the same 61-byte record with a 1
// there. Both decompress to 61 bytes; the slot is always present, and the compiler
// fills it with uninitialised memory when the .rsc omits it. Retail's ctor
// (0x140033c30) sets only +0x20/+0x28/+0x2c, so nothing ever overwrites it either.
//
// The consumer is written to survive this: 0x14011d5d0 clamps the stepped-down mesh
// variant to >= 0 FIRST and only then raises it to _minDamageLevel, so a hugely
// negative floor simply never binds and the building collapses to variant 0. Market
// stops at variant 1. Do NOT "fix" the value here — see destroycomponent_state6.cpp.
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

namespace {
constexpr uintptr_t kVtable  = 0x1401631b0;
constexpr int       kVersion = 5;
}

static void* DestroyDescription_Ctor_Impl(void* mem) {
    if (fn_External_External) fn_External_External(mem);
    uintptr_t s = (uintptr_t)mem;
    *(void**)mem = (void*)ResolveAddr(Module::Exe, kVtable);
    *(uint64_t*)(s + 0x20) = 0;   // _replaceEntity
    *(uint32_t*)(s + 0x28) = 1;   // _damagedSkin
    *(uint32_t*)(s + 0x2c) = 1;   // _damagedDecal
    // NOTE: retail deliberately leaves +0x30 (_minDamageLevel) unset here — Serialize
    // always writes it, because the compiler always emits the slot (with garbage when
    // the .rsc omits the field). We zero it only so an un-serialized description is
    // deterministic; the loaded value overwrites it either way.
    *(uint32_t*)(s + 0x30) = 0;   // _minDamageLevel
    return mem;
}

// Serialize (vtable +0x28, FUN_14011de70)
void DestroyDescription_Serialize(void* self, void* stream) {
    uintptr_t s = (uintptr_t)self;
    alignas(8) uint8_t serializer[24];
    if (!g_fn_Serializer_Ctor || !g_fn_Serialize_Int || !fn_ExternalPtr_BuildSerialize) {
        return;
    }
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // 1. _replaceEntity (+0x20). Retail calls FUN_140002340, the
    //    ExternalPtr<ComponentDescription> variant, which gates the resolved object on
    //    IsDerivedFrom(_, DWORD_1401c7318) = ComponentDescription. This used to call
    //    fn_ExternalPtr_Serialize2 (FUN_14000bf40) — byte-identical on the wire, but it
    //    gates on the StringTable type id instead, so a non-null _replaceEntity failed
    //    the cast and was stored as NULL. Only Template/IronMine.rsc and
    //    Template/Quarry.rsc author it (`ComponentDescription _replaceEntity =
    //    "destroyed"`), which is why the swap was invisible for so long.
    fn_ExternalPtr_BuildSerialize((void*)(s + 0x20), serializer);
    // 2..4. three ints
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x28), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x2c), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x30), nullptr);   // B.3.4 bridge

    if (g_fn_Serializer_Dtor) reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// Dtor (vtable +0x00, FUN_140033d70) — scalar fields only, no heap.
void* DestroyDescription_Dtor(void* self, uint32_t flags) {
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable);
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t DestroyDescription_GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("DestroyDescription");
    return cached;
}
static int   Vtbl_GetVersion(void*) { return kVersion; }
static int   Vtbl_Return0(void*) { return 0; }
static void* Vtbl_ReturnNull(void*, ...) { return nullptr; }

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* DestroyDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "DestroyComponent"); }

static void* g_Vtable[11] = {
    reinterpret_cast<void*>(&DestroyDescription_Dtor),      // +0x00
    reinterpret_cast<void*>(&Vtbl_ReturnNull),              // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&DestroyDescription_GetTypeId), // +0x10
    reinterpret_cast<void*>(&Vtbl_GetVersion),              // +0x18 (=5)
    reinterpret_cast<void*>(&Vtbl_Return0),                 // +0x20 Compile
    reinterpret_cast<void*>(&DestroyDescription_Serialize), // +0x28
    reinterpret_cast<void*>(&Vtbl_Return0),                 // +0x30 Cleanup
    reinterpret_cast<void*>(&Vtbl_Return0),                 // +0x38 GetDesc
    reinterpret_cast<void*>(&DestroyDescription_GetComponentTypeId),              // +0x40 Slot8
    reinterpret_cast<void*>(&Vtbl_ReturnNull),              // +0x48 guard
    reinterpret_cast<void*>(&Vtbl_ReturnNull),              // +0x50 guard
};

extern "C" void RegisterDestroyDescriptionVtable() {
    RegisterExeAddr(kVtable, reinterpret_cast<uintptr_t>(g_Vtable));
}
extern "C" void* DestroyDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    if (!p) return nullptr;
    return DestroyDescription_Ctor_Impl(p);
}
extern "C" void* DestroyDescription_CreateInPlace(void* mem) {
    if (!mem) return nullptr;
    return DestroyDescription_Ctor_Impl(mem);
}

REGISTER_TYPE(DestroyDescription, "Description",
              &DestroyDescription_Factory, &DestroyDescription_CreateInPlace);
