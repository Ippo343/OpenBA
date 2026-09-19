// code/Game/Component/herbalistdescription.cpp
// HerbalistDescription (entity description block) — herbalist gathering parameters.
// Extends Object::External (0x20 base). Vtable 0x140159910. Parent "Description".
// Size 0x38. GetVersion=2 (slot +0x18 @ 0x14004b380: mov eax,2; ret).
//
// Ctor defaults (HerbalistDescription_Factory):
//   External*-array @ +0x20 = {ptr=0, count=0 @ +0x28}  (herb material refs)
//   int/enum        @ +0x30 = 6
// Serialize (vtable +0x28, 0x14004b2e0), stream order:
//   int count @ +0x28 -> resize +0x20 External*-array -> per-element FindOrLoad
//   (FUN_1400049c0 == Desc_LoadExternalArray)
//   enum @ +0x30 (encoding vcall +0x98)
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

extern "C" void Desc_LoadExternalArray(void* serializer, int count, void* arrayBase);

static const uintptr_t kHerbalistDescriptionVtable = 0x140159910;

static inline int HD_SerStreamErr(void* serBuf) {
    return *reinterpret_cast<int*>(
        reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serBuf)) + 8);
}

static void HerbalistDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kHerbalistDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;   // External*-array ptr
    *(uint64_t*)(s + 0x28) = 0;   // count + cap
    *(int32_t*) (s + 0x30) = 6;
}

void HerbalistDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // External*-array @ +0x20 (count @ +0x28, stride 8)
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x28), nullptr);   // B.3.4 bridge
    int n = *(int32_t*)(s + 0x28);
    if (HD_SerStreamErr(serializer) == 0 && n > 0 && fn_MemAlloc && g_pMemAllocSingleton) {
        void* buf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)n * 8, 8);
        *(void**)(s + 0x20) = buf;
        *(int32_t*)(s + 0x2c) = n;
        if (buf) for (int i = 0; i < n; ++i) ((void**)buf)[i] = nullptr;
    }
    Desc_LoadExternalArray(serializer, n, *(void**)(s + 0x20));

    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<unsigned int*>(s + 0x30), nullptr);   // B.3.4 bridge (enum +0x30)
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

void* HerbalistDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kHerbalistDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    void* arr = *(void**)(s + 0x20);
    if (arr && fn_MemFreeStatic) fn_MemFreeStatic(arr);
    *(void**)(s + 0x20) = nullptr;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t HerbalistDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("HerbalistDescription");
    return cached;
}
static int   HerbalistDescription_Stub_Return2(void* /*self*/) { return 2; } // GetVersion
static int   HerbalistDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* HerbalistDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* HerbalistDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* HerbalistDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "HerbalistComponent"); }

static void* g_HerbalistDescription_Vtable[11] = {
    reinterpret_cast<void*>(&HerbalistDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&HerbalistDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&HerbalistDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&HerbalistDescription_Stub_Return2),    // +0x18 GetVersion=2
    reinterpret_cast<void*>(&HerbalistDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&HerbalistDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&HerbalistDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&HerbalistDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&HerbalistDescription_GetComponentTypeId), // +0x40
    reinterpret_cast<void*>(&HerbalistDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&HerbalistDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterHerbalistDescriptionVtable() {
    RegisterExeAddr(kHerbalistDescriptionVtable, reinterpret_cast<uintptr_t>(g_HerbalistDescription_Vtable));
}
extern "C" void* HerbalistDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    if (!p) return nullptr;
    HerbalistDescription_Init(p);
    return p;
}
static void* HerbalistDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    HerbalistDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(HerbalistDescription, "Description",
              &HerbalistDescription_Factory, &HerbalistDescription_CreateInPlace);
