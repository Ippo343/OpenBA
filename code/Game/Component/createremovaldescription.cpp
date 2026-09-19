// code/Game/Component/createremovaldescription.cpp
// Clean reimplementation of CreateRemovalDescription (entity description block).
//
// CreateRemovalDescription (0x48 bytes) — removal/demolish placement tool.
// Extends Object::External (0x20 base). Vtable at 0x140162d80. Parent "Description".
//   +0x20: bool
//   +0x28: void* heap ExternalPtr array base (8-byte elements)
//   +0x30: int   count
//   +0x38: ExternalPtr (single)
//   +0x40..+0x43: 4 bools
//
// Factory  : MemAlloc 0x48 + External ctor + vtable + field defaults (zero).
// Serialize (vtable +0x28, 0x140119ac0 CreateRemovalDescription_Slot5), stream order:
//   bool  @ +0x20
//   int count (local copy of +0x30)
//   [if loading: resize array @+0x28 to count via FUN_140001bb0 = Array<ptr>::SetSize]
//   ExternalRefArray(count) @ *(+0x28)  (FUN_1400049c0 / fn_ExternalRefArray_Serialize)
//   ExternalPtr @ +0x38  (FUN_140002340 / fn_ExternalPtr_BuildSerialize)
//   bool  @ +0x40   bool @ +0x41   bool @ +0x42   bool @ +0x43
// GetVersion (vtable +0x18, 0x140119bc0): mov eax,1; ret -> version 1.
// Dtor (vtable +0x00, 0x140032ca0): MemFree(array @+0x28) + External::~External + free.
//
// NOTE: count is serialized into a local; the on-disk count then drives array resize +
// the per-element load. The resize is `Collection::Array<Ptr8>::SetSize` (B.3.2).
// fn_ExternalRefArray_Serialize -> exe FUN_1400049c0, null in standalone until 0x1400049c0
// is registered (REPORTED). param4 is a dead init -> nullptr safe.
//
// Field knowledge harvested from engine/reimpl/createremovaldescription.cpp.
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include "Component/coll_bridge.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

static const uintptr_t kCreateRemovalDescriptionVtable = 0x140162d80;

static void CreateRemovalDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kCreateRemovalDescriptionVtable);
    *(void**)self = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint8_t*) (s + 0x20) = 0;   // bool
    *(uint64_t*)(s + 0x28) = 0;   // array base
    *(uint64_t*)(s + 0x30) = 0;   // count (+cap)
    *(uint64_t*)(s + 0x38) = 0;   // ExternalPtr
    *(uint32_t*)(s + 0x40) = 0;   // 4 bools
}

void CreateRemovalDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!fn_ExternalRefArray_Serialize || !fn_ExternalPtr_BuildSerialize) {
        return;
    }
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x20), nullptr);   // bool +0x20   // B.3.4 bridge

    int count = *(int*)(s + 0x30);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(count, nullptr);      // int count   // B.3.4 bridge

    // On load (stream mode == 0): resize the heap ExternalPtr array at +0x28.
    void* serField0 = *(void**)serializer;
    if (serField0 && static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        // B.3.2: Collection::Array<T>::SetSize — retail 0x140001bb0, stride 8 align 8, new tail zeroed.
        // Element is the 8-byte ExternalPtr slot the ExternalRefArray serializer walks below.
        GameColl::ArrayAt<GameColl::Ptr8>(s, 0x28).SetSize(count);
    }

    fn_ExternalRefArray_Serialize(serializer, count, *(void**)(s + 0x28), nullptr); // ExtPtr[count]
    fn_ExternalPtr_BuildSerialize(s + 0x38, serializer);  // ExternalPtr +0x38

    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x40), nullptr);   // bool +0x40   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x41), nullptr);   // bool +0x41   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x42), nullptr);   // bool +0x42   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x43), nullptr);   // bool +0x43   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

void* CreateRemovalDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kCreateRemovalDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_MemFreeStatic && *(void**)(s + 0x28)) {
        fn_MemFreeStatic(*(void**)(s + 0x28));   // heap ExternalPtr array
    }
    *(uint64_t*)(s + 0x28) = 0;
    *(uint64_t*)(s + 0x30) = 0;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}


#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t CreateRemovalDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("CreateRemovalDescription");
    return cached;
}

static int   CreateRemovalDescription_VtblStub_Return1(void* /*self*/) { return 1; } // GetVersion
static int   CreateRemovalDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* CreateRemovalDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* CreateRemovalDescription_CreateInPlace(void* memory);

static void* g_CreateRemovalDescription_Vtable[11] = {
    reinterpret_cast<void*>(&CreateRemovalDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&CreateRemovalDescription_VtblStub_ReturnNull),// +0x08 Slot1
    reinterpret_cast<void*>(&CreateRemovalDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&CreateRemovalDescription_VtblStub_Return1),   // +0x18 GetVersion
    reinterpret_cast<void*>(&CreateRemovalDescription_VtblStub_Return0),   // +0x20 Compile
    reinterpret_cast<void*>(&CreateRemovalDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&CreateRemovalDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&CreateRemovalDescription_VtblStub_Return0),   // +0x38 GetDesc
    reinterpret_cast<void*>(&CreateRemovalDescription_VtblStub_ReturnNull),// +0x40 Slot8
    reinterpret_cast<void*>(&CreateRemovalDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&CreateRemovalDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterCreateRemovalDescriptionVtable() {
    RegisterExeAddr(kCreateRemovalDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_CreateRemovalDescription_Vtable));
}

extern "C" void* CreateRemovalDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
    if (!p) return nullptr;
    CreateRemovalDescription_Init(p);
    return p;
}

static void* CreateRemovalDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    CreateRemovalDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(CreateRemovalDescription, "Description",
              &CreateRemovalDescription_Factory, &CreateRemovalDescription_CreateInPlace);

