// code/Game/Component/marketdescription.cpp
// MarketDescription (entity description block) — market fill-ratio parameters.
// Extends Object::External (0x20 base). Vtable 0x140159cf0. Parent "Description".
// Size 0x38. GetVersion=3 (slot +0x18 @ 0x14004ec80: mov eax,3; ret).
//
// Ctor defaults (MarketDescription_Factory):
//   float @ +0x20 = 1.0 (0x3f800000)   (global fill ratio)
//   array @ +0x28 = {ptr=0, count/cap=0 @ +0x30}  (per-resource fill ratios)
// Serialize (vtable +0x28, 0x14004ebb0), stream order:
//   float @ +0x20
//   int   count @ +0x30 -> resize +0x28 array (element stride 8: {enum @+0, float @+4})
//   per-element: enum @+0 (serializer encoding vcall +0x98), float @+4
// ToolKit fields: fill ratios (global + per-resource).
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kMarketDescriptionVtable = 0x140159cf0;

static inline int MD_SerStreamErr(void* serBuf) {
    return *reinterpret_cast<int*>(
        reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serBuf)) + 8);
}

static void MarketDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kMarketDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(float*)   (s + 0x20) = 1.0f;
    *(uint64_t*)(s + 0x28) = 0;   // array ptr
    *(uint64_t*)(s + 0x30) = 0;   // count + cap
}

void MarketDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Int ||
        !fn_Serializer_Serialize_Float) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);

    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x20), nullptr);

    // array @ +0x28 (count @ +0x30, element stride 8: {enum @+0, float @+4})
    g_fn_Serialize_Int(serializer, s + 0x30, nullptr);
    int n = *(int32_t*)(s + 0x30);
    if (MD_SerStreamErr(serializer) == 0 && n > 0 && fn_MemAlloc && g_pMemAllocSingleton) {
        void* buf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)n * 8, 8);
        *(void**)(s + 0x28) = buf;
        *(int32_t*)(s + 0x34) = n;
        if (buf) for (int i = 0; i < n; ++i) ((uint64_t*)buf)[i] = 0;
    }
    if (n > 0) {
        uint8_t* e = *(uint8_t**)(s + 0x28);
        for (int i = 0; i < n && e; ++i, e += 8) {
            SerializerEncodingVCall(serializer, 0x98, e + 0);      // enum @+0
            fn_Serializer_Serialize_Float(serializer, (float*)(e + 4), nullptr); // float @+4
        }
    }
    g_fn_Serializer_Dtor(serializer);
}

void* MarketDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kMarketDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    void* arr = *(void**)(s + 0x28);
    if (arr && fn_MemFreeStatic) fn_MemFreeStatic(arr);
    *(void**)(s + 0x28) = nullptr;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t MarketDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("MarketDescription");
    return cached;
}
static int   MarketDescription_Stub_Return3(void* /*self*/) { return 3; } // GetVersion
static int   MarketDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* MarketDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* MarketDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* MarketDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "MarketComponent"); }

static void* g_MarketDescription_Vtable[11] = {
    reinterpret_cast<void*>(&MarketDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&MarketDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&MarketDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&MarketDescription_Stub_Return3),    // +0x18 GetVersion=3
    reinterpret_cast<void*>(&MarketDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&MarketDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&MarketDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&MarketDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&MarketDescription_GetComponentTypeId), // +0x40
    reinterpret_cast<void*>(&MarketDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&MarketDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterMarketDescriptionVtable() {
    RegisterExeAddr(kMarketDescriptionVtable, reinterpret_cast<uintptr_t>(g_MarketDescription_Vtable));
}
extern "C" void* MarketDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    if (!p) return nullptr;
    MarketDescription_Init(p);
    return p;
}
static void* MarketDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    MarketDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(MarketDescription, "Description",
              &MarketDescription_Factory, &MarketDescription_CreateInPlace);
