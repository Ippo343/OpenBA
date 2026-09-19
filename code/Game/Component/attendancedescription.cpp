// code/Game/Component/attendancedescription.cpp
// AttendanceDescription (entity description block) — gathering/attendance parameters.
// Extends Object::External (0x20 base). Vtable 0x140158d70. Parent "Description".
// Size 0x28. GetVersion=1.
//
// Factory 0x14014a740-area (MemAlloc 0x28). Ctor default: int @ +0x20 = 0xf (15).
// Serialize (vtable +0x28, 0x1400446b0), stream order:  int @ +0x20.
// GetVersion (vtable +0x18, 0x140044700): mov eax,1; ret.
// Decompiled READ-ONLY from Application-x64.exe (Foundation C wave 2).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

static const uintptr_t kAttendanceDescriptionVtable = 0x140158d70;

static void AttendanceDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kAttendanceDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(int32_t*)(s + 0x20) = 0xf;
}

void AttendanceDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Int) return;
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x20), nullptr);   // B.3.4 bridge  (int +0x20)
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

void* AttendanceDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kAttendanceDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t AttendanceDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("AttendanceDescription");
    return cached;
}
static int   AttendanceDescription_Stub_Return1(void* /*self*/) { return 1; } // GetVersion=1
static int   AttendanceDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* AttendanceDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* AttendanceDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* AttendanceDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "AttendanceComponent"); }

static void* g_AttendanceDescription_Vtable[11] = {
    reinterpret_cast<void*>(&AttendanceDescription_Dtor),             // +0x00
    reinterpret_cast<void*>(&AttendanceDescription_Stub_ReturnNull),  // +0x08
    reinterpret_cast<void*>(&AttendanceDescription_GetTypeId),        // +0x10
    reinterpret_cast<void*>(&AttendanceDescription_Stub_Return1),     // +0x18 GetVersion=1
    reinterpret_cast<void*>(&AttendanceDescription_Stub_Return0),     // +0x20 Compile
    reinterpret_cast<void*>(&AttendanceDescription_Serialize),        // +0x28 Serialize
    reinterpret_cast<void*>(&AttendanceDescription_Stub_Return0),     // +0x30
    reinterpret_cast<void*>(&AttendanceDescription_Stub_Return0),     // +0x38
    reinterpret_cast<void*>(&AttendanceDescription_GetComponentTypeId),  // +0x40
    reinterpret_cast<void*>(&AttendanceDescription_CreateInPlace),    // +0x48 CreateInPlace
    reinterpret_cast<void*>(&AttendanceDescription_Stub_ReturnNull),  // +0x50 guard
};

extern "C" void RegisterAttendanceDescriptionVtable() {
    RegisterExeAddr(kAttendanceDescriptionVtable, reinterpret_cast<uintptr_t>(g_AttendanceDescription_Vtable));
}
extern "C" void* AttendanceDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
    if (!p) return nullptr;
    AttendanceDescription_Init(p);
    return p;
}
static void* AttendanceDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    AttendanceDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(AttendanceDescription, "Description",
              &AttendanceDescription_Factory, &AttendanceDescription_CreateInPlace);
