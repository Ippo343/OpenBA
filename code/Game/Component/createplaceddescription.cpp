// code/Game/Component/createplaceddescription.cpp
// Clean reimplementation of CreatePlacedDescription (entity description block).
//
// CreatePlacedDescription (0x58 bytes) — single-placement build tool parameters.
// Extends Object::External (0x20 base). Vtable at 0x140162cf0.
// Parent type "Description".
//
// Factory  : 0x140032a60  (MemAlloc 0x58 + ctor 0x140119a60)
// CreateInPlace: 0x140032aa0
// Ctor defaults (from CreatePlacedDescription_Ctor 0x140119a60):
//   int   @ +0x20 = 0   (enum: placement category)
//   String@ +0x28 = 0   (empty)
//   void* @ +0x30 = 0   (int[] array base — enum elements, stride 4)
//   int   @ +0x38 = 0   (array count, cap @+0x3c)
//   int   @ +0x40 = 0
//   int   @ +0x44 = 1
//   int   @ +0x48 = 1
//   void* @ +0x50 = 0   (ExternalRef: created description, weak)
// Serialize (vtable +0x28, 0x1401198d0), stream order:
//   enum         @ +0x20                 (encoding vtable +0x98 dispatch)
//   int count    @ +0x38                 (+ resize int[] @+0x30 on load)
//   for each element (stride 4 at base +0x30):
//     enum       @ elem                  (encoding vtable +0x98 dispatch)
//   String       @ +0x28
//   int          @ +0x44
//   int          @ +0x48
//   int          @ +0x40
//   ExternalRef  @ +0x50                 (FUN_140002340)
// GetVersion (vtable +0x18, 0x1401199f0): mov eax,4; ret -> file version 4.
// Dtor (vtable +0x00, 0x140032ad0): vtable reset + MemFree(array @+0x30) +
//   String::~String(+0x28) + External::~External + free.
//
// STANDALONE notes: fresh-load int[] alloc mirrors FUN_1400021d0 (stride 4);
// per-element + +0x20 enums use the serializer-encoding +0x98 dispatch;
// the +0x50 ExternalRef reuses the generic ExternalPtr serialize reimpl.
//
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

extern "C" void Reimpl_ExternalPtr_Serialize_StringTable(void* refField, void* serializer);

static const uintptr_t kCreatePlacedDescriptionVtable = 0x140162cf0;

static void CreatePlacedDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kCreatePlacedDescriptionVtable);
    *(void**)self = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(int32_t*) (s + 0x20) = 0;   // enum
    *(uint64_t*)(s + 0x28) = 0;   // String (empty)
    *(uint64_t*)(s + 0x30) = 0;   // int[] base + count@+0x38 cleared
    *(int32_t*) (s + 0x40) = 0;   // int
    *(int32_t*) (s + 0x44) = 1;   // int
    *(int32_t*) (s + 0x48) = 1;   // int
    *(uint64_t*)(s + 0x50) = 0;   // ExternalRef (weak)
}

// ============================================================
// CreatePlacedDescription_Serialize (vtable +0x28, 0x1401198d0)
// ============================================================
void CreatePlacedDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Int ||
        !g_fn_String_Serialize || !fn_MemAlloc || !g_pMemAllocSingleton) {
        return;
    }
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);

    // enum @ +0x20
    SerializerEncodingVCall(serializer, 0x98, s + 0x20);

    // int[] count @ +0x38, base @ +0x30 (stride 4)
    int count = *(int32_t*)(s + 0x38);
    g_fn_Serialize_Int(serializer, s + 0x38, nullptr);
    count = *(int32_t*)(s + 0x38);

    uint8_t* arr = nullptr;
    if (count > 0) {
        arr = reinterpret_cast<uint8_t*>(
            fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)count * 4, 4));
    }
    *(void**)(s + 0x30)   = arr;
    *(int32_t*)(s + 0x38) = count;
    *(int32_t*)(s + 0x3c) = count;

    uint8_t* e = arr;
    for (int i = 0; i < count; ++i, e += 4) {
        *(int32_t*)e = 0;
        SerializerEncodingVCall(serializer, 0x98, e);   // enum element
    }

    g_fn_String_Serialize(s + 0x28, serializer);                   // String +0x28
    g_fn_Serialize_Int(serializer, s + 0x44, nullptr);             // int    +0x44
    g_fn_Serialize_Int(serializer, s + 0x48, nullptr);             // int    +0x48
    g_fn_Serialize_Int(serializer, s + 0x40, nullptr);             // int    +0x40
    Reimpl_ExternalPtr_Serialize_StringTable(s + 0x50, serializer);// ref    +0x50

    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

// ============================================================
// CreatePlacedDescription_Dtor (vtable +0x00, 0x140032ad0)
// ============================================================
void* CreatePlacedDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kCreatePlacedDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_MemFreeStatic && *(void**)(s + 0x30)) {
        fn_MemFreeStatic(*(void**)(s + 0x30));   // int[] array
    }
    *(uint64_t*)(s + 0x30) = 0;
    *(uint64_t*)(s + 0x38) = 0;
    if (fn_String_Dtor) fn_String_Dtor(s + 0x28);  // String +0x28
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t CreatePlacedDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("CreatePlacedDescription");
    return cached;
}

static int   CreatePlacedDescription_VtblStub_Return4(void* /*self*/) { return 4; } // GetVersion
static int   CreatePlacedDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* CreatePlacedDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* CreatePlacedDescription_CreateInPlace(void* memory);

static void* g_CreatePlacedDescription_Vtable[11] = {
    reinterpret_cast<void*>(&CreatePlacedDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&CreatePlacedDescription_VtblStub_ReturnNull),// +0x08 Slot1
    reinterpret_cast<void*>(&CreatePlacedDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&CreatePlacedDescription_VtblStub_Return4),   // +0x18 GetVersion
    reinterpret_cast<void*>(&CreatePlacedDescription_VtblStub_Return0),   // +0x20 Compile
    reinterpret_cast<void*>(&CreatePlacedDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&CreatePlacedDescription_VtblStub_Return0),   // +0x30 Draw/Cleanup
    reinterpret_cast<void*>(&CreatePlacedDescription_VtblStub_Return0),   // +0x38 GetDesc
    reinterpret_cast<void*>(&CreatePlacedDescription_VtblStub_ReturnNull),// +0x40 Slot8
    reinterpret_cast<void*>(&CreatePlacedDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&CreatePlacedDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterCreatePlacedDescriptionVtable() {
    RegisterExeAddr(kCreatePlacedDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_CreatePlacedDescription_Vtable));
}

extern "C" void* CreatePlacedDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x58, 8);
    if (!p) return nullptr;
    CreatePlacedDescription_Init(p);
    return p;
}

static void* CreatePlacedDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    CreatePlacedDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(CreatePlacedDescription, "Description",
              &CreatePlacedDescription_Factory, &CreatePlacedDescription_CreateInPlace);

#endif // STANDALONE_EXE
