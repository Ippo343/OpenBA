// code/Game/Component/consumeproducedescription.cpp
// ConsumeProduceDescription (entity description block) — production-building recipe.
// Extends Object::External (0x20 base). Vtable 0x140162b30. Parent "Description".
// Size 0x58. GetVersion=12 (slot +0x18 @ 0x140119200: mov eax,0xC; ret).
//
// Ctor defaults (ConsumeProduceDescription_sub_9270):
//   ExternalPtr  @ +0x20 = 0
//   array        @ +0x28 = {ptr=0, count/cap=0 @ +0x30}   (recipe entries)
//   Type         @ +0x38 = 0   (tool typeId; serialized as a name)
//   int          @ +0x3c = 0
//   int/enum     @ +0x40 = 1
//   float        @ +0x44 = 4.0 (0x40800000)
//   int          @ +0x48 = -1  (0xffffffff)
//   int          @ +0x4c = 0
//   int/enum     @ +0x50 = 0xb (11)
// Serialize (vtable +0x28, 0x1401190e0), stream order (verified disasm CALLs):
//   ExternalPtr @ +0x20  (FUN_14000bf40 -> fn_ExternalPtr_BuildSerialize)
//   int count   @ +0x30 -> resize +0x28 outer array (stride 0x28), per outer element:
//        inner int count @+0x08 -> resize inner array @+0x00 (stride 0x10):
//             per inner element: ExternalPtr @+0x00 (FUN_140002340), int @+0x08
//        ExternalPtr @+0x10 (FUN_140002340)
//        enum @+0x18 (encoding vcall +0x98)
//        String @+0x20
//   int   @ +0x40
//   enum  @ +0x50 (encoding vcall +0x98)
//   Type  @ +0x38 (Object::Type::Serialize, dispatch slot 0x1401516b0)
//   int   @ +0x3c
//   float @ +0x44
//   int   @ +0x48
//   int   @ +0x4c
// ToolKit fields: inputs / outputs / work time / tool.
// Decompiled READ-ONLY from Application-x64.exe.
//
// NOTE for orchestrator: the Type field @ +0x38 uses Object::Type::Serialize via
// IO::Serializer dispatch slot 0x1401516b0. We resolve that slot inline below; it
// is filled from Runtime-x64.dll at startup (same mechanism as g_fn_String_Serialize
// = slot 0x140151238). If it is unfilled in standalone the Type read is skipped,
// which would desync the stream — register/wire slot 0x1401516b0 if needed.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kConsumeProduceDescriptionVtable = 0x140162b30;
static const uintptr_t kSlot_TypeSerialize = 0x1401516b0; // Object::Type::Serialize dispatch slot

static inline int CP_SerStreamErr(void* serBuf) {
    return *reinterpret_cast<int*>(
        reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serBuf)) + 8);
}

static void ConsumeProduceDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kConsumeProduceDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;           // ExternalPtr
    *(uint64_t*)(s + 0x28) = 0;           // array ptr
    *(uint64_t*)(s + 0x30) = 0;           // count + cap
    *(uint64_t*)(s + 0x38) = 0;           // Type + int@+0x3c
    *(int32_t*) (s + 0x40) = 1;
    *(uint32_t*)(s + 0x44) = 0x40800000u; // float 4.0
    *(int32_t*) (s + 0x48) = -1;
    *(int32_t*) (s + 0x4c) = 0;
    *(int32_t*) (s + 0x50) = 0xb;
}

void ConsumeProduceDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Int ||
        !g_fn_String_Serialize || !fn_Serializer_Serialize_Float ||
        !fn_ExternalPtr_BuildSerialize || !fn_MemAlloc || !g_pMemAllocSingleton) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);

    fn_ExternalPtr_BuildSerialize(s + 0x20, serializer);   // ExternalPtr +0x20

    // outer recipe array @ +0x28 (count @ +0x30, element stride 0x28)
    g_fn_Serialize_Int(serializer, s + 0x30, nullptr);
    int outerN = *(int32_t*)(s + 0x30);
    if (CP_SerStreamErr(serializer) == 0 && outerN > 0) {
        void* buf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)outerN * 0x28, 8);
        *(void**)(s + 0x28) = buf;
        *(int32_t*)(s + 0x34) = outerN;
        if (buf) for (uint64_t b = 0; b < (uint64_t)outerN * 0x28; ++b) ((uint8_t*)buf)[b] = 0;
    }
    if (outerN > 0) {
        uint8_t* e = *(uint8_t**)(s + 0x28);
        for (int i = 0; i < outerN && e; ++i, e += 0x28) {
            // inner array @ e+0x00 (count @ e+0x08, element stride 0x10)
            g_fn_Serialize_Int(serializer, e + 0x08, nullptr);
            int innerN = *(int32_t*)(e + 0x08);
            if (CP_SerStreamErr(serializer) == 0 && innerN > 0) {
                void* ibuf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)innerN * 0x10, 8);
                *(void**)(e + 0x00) = ibuf;
                if (ibuf) for (uint64_t b = 0; b < (uint64_t)innerN * 0x10; ++b) ((uint8_t*)ibuf)[b] = 0;
            }
            uint8_t* ie = *(uint8_t**)(e + 0x00);
            for (int j = 0; j < innerN && ie; ++j, ie += 0x10) {
                fn_ExternalPtr_BuildSerialize(ie + 0x00, serializer); // ExternalPtr
                g_fn_Serialize_Int(serializer, ie + 0x08, nullptr);   // int
            }
            fn_ExternalPtr_BuildSerialize(e + 0x10, serializer);      // ExternalPtr +0x10
            SerializerEncodingVCall(serializer, 0x98, e + 0x18);      // enum +0x18
            g_fn_String_Serialize(e + 0x20, serializer);              // String +0x20
        }
    }

    g_fn_Serialize_Int(serializer, s + 0x40, nullptr);
    SerializerEncodingVCall(serializer, 0x98, s + 0x50);   // enum +0x50

    // Type @ +0x38 via Object::Type::Serialize dispatch slot 0x1401516b0
    {
        uintptr_t slotAddr = ResolveAddr(Module::Exe, kSlot_TypeSerialize);
        typedef void (*TypeSer_t)(void* typeField, void* serializer);
        TypeSer_t fnTypeSer = slotAddr ? *reinterpret_cast<TypeSer_t*>(slotAddr) : nullptr;
        if (fnTypeSer) fnTypeSer(s + 0x38, serializer);
        // else: slot unwired in standalone — Type read skipped (would desync; see header note)
    }

    g_fn_Serialize_Int(serializer, s + 0x3c, nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x44), nullptr);
    g_fn_Serialize_Int(serializer, s + 0x48, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x4c, nullptr);
    g_fn_Serializer_Dtor(serializer);
}

void* ConsumeProduceDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kConsumeProduceDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    // free inner arrays then the outer recipe array
    int outerN = *(int32_t*)(s + 0x34);
    uint8_t* e = *(uint8_t**)(s + 0x28);
    if (e && fn_MemFreeStatic) {
        for (int i = 0; i < outerN; ++i, e += 0x28) {
            void* inner = *(void**)(e + 0x00);
            if (inner) fn_MemFreeStatic(inner);
            if (fn_String_Dtor) fn_String_Dtor(e + 0x20);
        }
        fn_MemFreeStatic(*(void**)(s + 0x28));
    }
    *(void**)(s + 0x28) = nullptr;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t ConsumeProduceDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ConsumeProduceDescription");
    return cached;
}
static int   ConsumeProduceDescription_Stub_Return12(void* /*self*/) { return 12; } // GetVersion
static int   ConsumeProduceDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* ConsumeProduceDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* ConsumeProduceDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* ConsumeProduceDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "ConsumeProduceComponent"); }

static void* g_ConsumeProduceDescription_Vtable[11] = {
    reinterpret_cast<void*>(&ConsumeProduceDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&ConsumeProduceDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&ConsumeProduceDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&ConsumeProduceDescription_Stub_Return12),   // +0x18 GetVersion=12
    reinterpret_cast<void*>(&ConsumeProduceDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&ConsumeProduceDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&ConsumeProduceDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&ConsumeProduceDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&ConsumeProduceDescription_GetComponentTypeId), // +0x40
    reinterpret_cast<void*>(&ConsumeProduceDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&ConsumeProduceDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterConsumeProduceDescriptionVtable() {
    RegisterExeAddr(kConsumeProduceDescriptionVtable, reinterpret_cast<uintptr_t>(g_ConsumeProduceDescription_Vtable));
}
extern "C" void* ConsumeProduceDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x58, 8);
    if (!p) return nullptr;
    ConsumeProduceDescription_Init(p);
    return p;
}
static void* ConsumeProduceDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    ConsumeProduceDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(ConsumeProduceDescription, "Description",
              &ConsumeProduceDescription_Factory, &ConsumeProduceDescription_CreateInPlace);
