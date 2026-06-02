// code/Game/Component/tradedescription.cpp
// TradeDescription (entity description block) — trading post / merchant config.
// Extends Object::External (0x20 base). Vtable 0x14015ad60. Parent "Description".
// Size 0x70. GetVersion=7 (slot +0x18 @ 0x14005ca00: mov eax,7; ret).
//
// Ctor defaults (TradeDescription_sub_caa0): all fields zero except
//   float @ +0x68 = 0x3eaa7efa (~0.333)
//   int   @ +0x6c = 3
// Serialize (vtable +0x28, 0x14005c7f0) — Serializer ctor with version=1, stream order
// (verified disasm CALLs at 0x14005c7f0):
//   ExternalPtr @ +0x30  (FUN_140002340 -> fn_ExternalPtr_BuildSerialize)
//   int count   @ +0x50 -> resize +0x48 merchant array (element 0x48), per merchant:
//        ExternalPtr @+0x00 (FUN_14000bf40)
//        String      @+0x08
//        int count   @+0x28 -> resize ext-array @+0x20 -> Desc_LoadExternalArray
//        int count   @+0x38 -> resize int-array @+0x30 -> per-elem enum vcall +0x98
//        int         @+0x40
//        enum @+0x10, enum @+0x14, enum @+0x18 (encoding vcall +0x98)
//   int count   @ +0x60 -> resize +0x58 array (element stride 0x14), per element:
//        int @+0x00, float @+0x04, float @+0x08, int @+0x0c, int @+0x10
//   ExternalPtr @ +0x20  (FUN_14000bd30 -> fn_ExternalPtr_BuildSerialize)
//   ExternalPtr @ +0x28  (FUN_14000bf40 -> fn_ExternalPtr_BuildSerialize)
//   int count   @ +0x40 -> resize +0x38 array (element stride 0x18), per element:
//        enum @+0x00 (encoding vcall +0x98), String @+0x08, String @+0x10
//   float @ +0x68
//   int   @ +0x6c
// ToolKit fields: merchants (ships / goods / prices).
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

extern "C" void Desc_LoadExternalArray(void* serializer, int count, void* arrayBase);

static const uintptr_t kTradeDescriptionVtable = 0x14015ad60;

static inline int TD_SerStreamErr(void* serBuf) {
    return *reinterpret_cast<int*>(
        reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serBuf)) + 8);
}

// Allocate + zero a count*stride array into (basePtr, countAt). Returns the buffer.
static void* TD_AllocArray(void* self, uint8_t* basePtr, int n, uint64_t stride) {
    void* buf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)n * stride, 8);
    *(void**)basePtr = buf;
    if (buf) for (uint64_t b = 0; b < (uint64_t)n * stride; ++b) ((uint8_t*)buf)[b] = 0;
    (void)self;
    return buf;
}

static void TradeDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kTradeDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    for (int off = 0x20; off < 0x68; off += 8) *(uint64_t*)(s + off) = 0;
    *(uint32_t*)(s + 0x68) = 0x3eaa7efau; // float ~0.333
    *(int32_t*) (s + 0x6c) = 3;
}

void TradeDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Int ||
        !g_fn_String_Serialize || !fn_Serializer_Serialize_Float ||
        !fn_ExternalPtr_BuildSerialize || !fn_MemAlloc || !g_pMemAllocSingleton) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 1);

    fn_ExternalPtr_BuildSerialize(s + 0x30, serializer);   // ExternalPtr +0x30

    // merchant array @ +0x48 (count @ +0x50, element stride 0x48)
    g_fn_Serialize_Int(serializer, s + 0x50, nullptr);
    int merchN = *(int32_t*)(s + 0x50);
    if (TD_SerStreamErr(serializer) == 0 && merchN > 0) TD_AllocArray(self, s + 0x48, merchN, 0x48);
    if (merchN > 0) {
        uint8_t* m = *(uint8_t**)(s + 0x48);
        for (int i = 0; i < merchN && m; ++i, m += 0x48) {
            fn_ExternalPtr_BuildSerialize(m + 0x00, serializer);  // ExternalPtr
            g_fn_String_Serialize(m + 0x08, serializer);          // String
            // ext-ref array @ m+0x20 (count @ m+0x28, stride 8)
            g_fn_Serialize_Int(serializer, m + 0x28, nullptr);
            int extN = *(int32_t*)(m + 0x28);
            if (TD_SerStreamErr(serializer) == 0 && extN > 0) TD_AllocArray(self, m + 0x20, extN, 8);
            Desc_LoadExternalArray(serializer, extN, *(void**)(m + 0x20));
            // int array @ m+0x30 (count @ m+0x38, stride 4) per-elem enum vcall +0x98
            g_fn_Serialize_Int(serializer, m + 0x38, nullptr);
            int intN = *(int32_t*)(m + 0x38);
            if (TD_SerStreamErr(serializer) == 0 && intN > 0) TD_AllocArray(self, m + 0x30, intN, 4);
            uint8_t* ia = *(uint8_t**)(m + 0x30);
            for (int j = 0; j < intN && ia; ++j, ia += 4)
                SerializerEncodingVCall(serializer, 0x98, ia);
            g_fn_Serialize_Int(serializer, m + 0x40, nullptr);    // int @+0x40
            SerializerEncodingVCall(serializer, 0x98, m + 0x10);  // enum @+0x10
            SerializerEncodingVCall(serializer, 0x98, m + 0x14);  // enum @+0x14
            SerializerEncodingVCall(serializer, 0x98, m + 0x18);  // enum @+0x18
        }
    }

    // price array @ +0x58 (count @ +0x60, element stride 0x14)
    g_fn_Serialize_Int(serializer, s + 0x60, nullptr);
    int priceN = *(int32_t*)(s + 0x60);
    if (TD_SerStreamErr(serializer) == 0 && priceN > 0) TD_AllocArray(self, s + 0x58, priceN, 0x14);
    if (priceN > 0) {
        uint8_t* p = *(uint8_t**)(s + 0x58);
        for (int i = 0; i < priceN && p; ++i, p += 0x14) {
            g_fn_Serialize_Int(serializer, p + 0x00, nullptr);
            fn_Serializer_Serialize_Float(serializer, (float*)(p + 0x04), nullptr);
            fn_Serializer_Serialize_Float(serializer, (float*)(p + 0x08), nullptr);
            g_fn_Serialize_Int(serializer, p + 0x0c, nullptr);
            g_fn_Serialize_Int(serializer, p + 0x10, nullptr);
        }
    }

    fn_ExternalPtr_BuildSerialize(s + 0x20, serializer);   // ExternalPtr +0x20
    fn_ExternalPtr_BuildSerialize(s + 0x28, serializer);   // ExternalPtr +0x28

    // string-pair array @ +0x38 (count @ +0x40, element stride 0x18)
    g_fn_Serialize_Int(serializer, s + 0x40, nullptr);
    int spN = *(int32_t*)(s + 0x40);
    if (TD_SerStreamErr(serializer) == 0 && spN > 0) TD_AllocArray(self, s + 0x38, spN, 0x18);
    if (spN > 0) {
        uint8_t* sp = *(uint8_t**)(s + 0x38);
        for (int i = 0; i < spN && sp; ++i, sp += 0x18) {
            SerializerEncodingVCall(serializer, 0x98, sp + 0x00); // enum @+0x00
            g_fn_String_Serialize(sp + 0x08, serializer);         // String @+0x08
            g_fn_String_Serialize(sp + 0x10, serializer);         // String @+0x10
        }
    }

    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x68), nullptr);
    g_fn_Serialize_Int(serializer, s + 0x6c, nullptr);
    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

void* TradeDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kTradeDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_MemFreeStatic) {
        // merchant array (free its inner arrays + strings first)
        int merchN = *(int32_t*)(s + 0x54);
        uint8_t* m = *(uint8_t**)(s + 0x48);
        if (m) {
            for (int i = 0; i < merchN; ++i, m += 0x48) {
                if (*(void**)(m + 0x20)) fn_MemFreeStatic(*(void**)(m + 0x20));
                if (*(void**)(m + 0x30)) fn_MemFreeStatic(*(void**)(m + 0x30));
                if (fn_String_Dtor) fn_String_Dtor(m + 0x08);
            }
            fn_MemFreeStatic(*(void**)(s + 0x48));
        }
        // string-pair array (free strings first)
        int spN = *(int32_t*)(s + 0x44);
        uint8_t* sp = *(uint8_t**)(s + 0x38);
        if (sp) {
            for (int i = 0; i < spN; ++i, sp += 0x18) {
                if (fn_String_Dtor) { fn_String_Dtor(sp + 0x08); fn_String_Dtor(sp + 0x10); }
            }
            fn_MemFreeStatic(*(void**)(s + 0x38));
        }
        if (*(void**)(s + 0x58)) fn_MemFreeStatic(*(void**)(s + 0x58));
    }
    *(void**)(s + 0x48) = nullptr;
    *(void**)(s + 0x38) = nullptr;
    *(void**)(s + 0x58) = nullptr;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t TradeDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("TradeDescription");
    return cached;
}
static int   TradeDescription_Stub_Return7(void* /*self*/) { return 7; } // GetVersion
static int   TradeDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* TradeDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* TradeDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* TradeDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "TradeComponent"); }

static void* g_TradeDescription_Vtable[11] = {
    reinterpret_cast<void*>(&TradeDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&TradeDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&TradeDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&TradeDescription_Stub_Return7),    // +0x18 GetVersion=7
    reinterpret_cast<void*>(&TradeDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&TradeDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&TradeDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&TradeDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&TradeDescription_GetComponentTypeId), // +0x40
    reinterpret_cast<void*>(&TradeDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&TradeDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterTradeDescriptionVtable() {
    RegisterExeAddr(kTradeDescriptionVtable, reinterpret_cast<uintptr_t>(g_TradeDescription_Vtable));
}
extern "C" void* TradeDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x70, 8);
    if (!p) return nullptr;
    TradeDescription_Init(p);
    return p;
}
static void* TradeDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    TradeDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(TradeDescription, "Description",
              &TradeDescription_Factory, &TradeDescription_CreateInPlace);
#endif
