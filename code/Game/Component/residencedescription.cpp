// code/Game/Component/residencedescription.cpp
// Clean reimplementation of ResidenceDescription (entity description block).
//
// ResidenceDescription (0x30 bytes) — housing parameters for a residence.
// Extends Object::External (0x20 base). Vtable at 0x140164350.
// Parent type "Description".  (ToolKit: ResidenceDescription = housing.)
//
// Factory  : 0x1400374a0  (MemAlloc 0x30 + ctor)
// CreateInPlace: 0x140037500  (the exe ctor only sets the vtable; the four
//   serialized fields at +0x20..+0x2c are left uninitialised by the exe and
//   filled by Serialize on load. We zero them for safety.)
// Serialize (vtable +0x28, 0x14012a3d0), stream order:
//   int   @ +0x20   (maxFamilies / capacity)
//   int   @ +0x24   (max occupants)
//   float @ +0x28
//   float @ +0x2c
// GetVersion (vtable +0x18, 0x14012a460): mov eax,2; ret -> file version 2.
// Dtor (vtable +0x00, 0x1400375b0): vtable reset + External::~External + free
//   (no owned String/array fields).
//
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kResidenceDescriptionVtable = 0x140164350;

static void ResidenceDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kResidenceDescriptionVtable);
    *(void**)self = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(int32_t*)(s + 0x20) = 0;   // int   (default; exe leaves uninit)
    *(int32_t*)(s + 0x24) = 0;   // int
    *(float*)  (s + 0x28) = 0.0f;// float
    *(float*)  (s + 0x2c) = 0.0f;// float
}

// ============================================================
// ResidenceDescription_Serialize (vtable +0x28, 0x14012a3d0)
// ============================================================
void ResidenceDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor ||
        !g_fn_Serialize_Int || !fn_Serializer_Serialize_Float) {
        return;
    }
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    g_fn_Serialize_Int(serializer, s + 0x20, nullptr);                 // +0x20 int
    g_fn_Serialize_Int(serializer, s + 0x24, nullptr);                 // +0x24 int
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x28), nullptr); // +0x28 float
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x2c), nullptr); // +0x2c float
    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

// ============================================================
// ResidenceDescription_Dtor (vtable +0x00, 0x1400375b0)
// ============================================================
void* ResidenceDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kResidenceDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t ResidenceDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ResidenceDescription");
    return cached;
}

static int   ResidenceDescription_VtblStub_Return2(void* /*self*/) { return 2; } // GetVersion
static int   ResidenceDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* ResidenceDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* ResidenceDescription_CreateInPlace(void* memory);

static void* g_ResidenceDescription_Vtable[11] = {
    reinterpret_cast<void*>(&ResidenceDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&ResidenceDescription_VtblStub_ReturnNull),// +0x08 GetTypeInfo
    reinterpret_cast<void*>(&ResidenceDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&ResidenceDescription_VtblStub_Return2),   // +0x18 GetVersion
    reinterpret_cast<void*>(&ResidenceDescription_VtblStub_Return0),   // +0x20 Compile
    reinterpret_cast<void*>(&ResidenceDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&ResidenceDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&ResidenceDescription_VtblStub_Return0),   // +0x38 GetDesc
    reinterpret_cast<void*>(&ResidenceDescription_VtblStub_ReturnNull),// +0x40 Slot8
    reinterpret_cast<void*>(&ResidenceDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&ResidenceDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterResidenceDescriptionVtable() {
    RegisterExeAddr(kResidenceDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_ResidenceDescription_Vtable));
}

extern "C" void* ResidenceDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
    if (!p) return nullptr;
    ResidenceDescription_Init(p);
    return p;
}

static void* ResidenceDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    ResidenceDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(ResidenceDescription, "Description",
              &ResidenceDescription_Factory, &ResidenceDescription_CreateInPlace);

#endif // STANDALONE_EXE
