// code/Game/Component/animaudioevent.cpp
// Foundation C (extension) — AnimAudioEvent factory for standalone.
//
// AnimAudioEvent is a Graphics::AnimationEvent subclass (0x30 bytes) loaded as
// part of the Character animation closure when an entity template with a
// Character component is deserialized (Template\Citizen.rsc). Without a real
// factory it was a NullFactory stub → Factory::CreateObject returns null →
// crash during the citizen-template FindOrLoad ("[NullFactory] Hit for type:
// AnimAudioEvent").
//
// Layout (from AnimAudioEvent_Factory 0x140016f00 + Serialize 0x140087ed0):
//   +0x00 vtable (0x14015e0c0)
//   +0x08 Graphics::AnimationEvent base (name/time, ctor-managed)
//   +0x20 External*  audio resource reference (default 0)
//   +0x28 float      volume/scale (default 1.0)
//
// Serialize (vtable +0x28) reads exactly two subclass fields: the External ref
// at +0x20 (via the generic External-pointer serializer FUN_1400019a0, which is
// stream-identical to fn_ExternalPtr_BuildSerialize / 0x140002340 — only the
// type-validation global differs, not the bytes consumed) and the float at
// +0x28. GetVersion (vtable +0x18) = 2 (verified: exe body `mov eax,2; ret`).
//
// The Graphics::AnimationEvent base ctor/dtor are Runtime-x64.dll exports
// (resolved by mangled name); the rest mirrors componentdescription.cpp.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

static constexpr uintptr_t kVtableAddr = 0x14015e0c0;
static constexpr int       kSize       = 0x30;
static constexpr int       OFF_REF     = 0x20;   // External* audio ref
static constexpr int       OFF_FLOAT   = 0x28;   // float volume/scale

// Graphics::AnimationEvent base ctor (void) / virtual dtor — Runtime exports.
typedef void* (*AnimEvent_Ctor_t)(void* self);
typedef void  (*AnimEvent_Dtor_t)(void* self);
static AnimEvent_Ctor_t ResolveAnimEventCtor() {
    static AnimEvent_Ctor_t fn = nullptr;
    static bool tried = false;
    if (!tried) { tried = true;
        fn = reinterpret_cast<AnimEvent_Ctor_t>(ResolveExport("??0AnimationEvent@Graphics@@QEAA@XZ"));
    }
    return fn;
}
static AnimEvent_Dtor_t ResolveAnimEventDtor() {
    static AnimEvent_Dtor_t fn = nullptr;
    static bool tried = false;
    if (!tried) { tried = true;
        fn = reinterpret_cast<AnimEvent_Dtor_t>(ResolveExport("??1AnimationEvent@Graphics@@UEAA@XZ"));
    }
    return fn;
}

static void* AnimAudioEvent_CtorImpl(void* mem) {
    if (!mem) return nullptr;
    uint8_t* s = reinterpret_cast<uint8_t*>(mem);
    AnimEvent_Ctor_t baseCtor = ResolveAnimEventCtor();
    if (baseCtor) baseCtor(mem);                              // Graphics::AnimationEvent base
    *(void**)mem = (void*)ResolveAddr(Module::Exe, kVtableAddr);
    *(void**)(s + OFF_REF)   = nullptr;                       // audio ref default 0
    *(float*)(s + OFF_FLOAT) = 1.0f;                          // default 1.0
    return mem;
}

extern "C" void AnimAudioEvent_Serialize(void* self, void* stream) {
    if (!self) return;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor) return;
    uint8_t serBuf[24];
    g_fn_Serializer_Ctor(serBuf, stream, 0);
    // +0x20 external audio reference (stream-identical generic external-ptr serialize)
    if (fn_ExternalPtr_BuildSerialize)
        fn_ExternalPtr_BuildSerialize(s + OFF_REF, serBuf);
    // +0x28 float
    if (fn_Serializer_Serialize_Float)
        fn_Serializer_Serialize_Float(serBuf, (float*)(s + OFF_FLOAT), nullptr);
    if (g_fn_Serializer_Dtor) g_fn_Serializer_Dtor(serBuf);
}

extern "C" void* AnimAudioEvent_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtableAddr);
    AnimEvent_Dtor_t baseDtor = ResolveAnimEventDtor();
    if (baseDtor) baseDtor(self);                             // base destructor
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t AnimAudioEvent_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("AnimAudioEvent");
    return cached;
}
static int   AAE_Return2(void* /*self*/)    { return 2; }    // GetVersion = 2
static int   AAE_Return0(void* /*self*/)    { return 0; }
static void* AAE_ReturnNull(void*, ...)     { return nullptr; }
extern "C" void* AnimAudioEvent_CreateInPlace(void* mem);

// vtable @ 0x14015e0c0 (Object::External 11-slot layout).
static void* g_AAE_Vtable[11] = {
    reinterpret_cast<void*>(&AnimAudioEvent_Dtor),       // +0x00 dtor
    reinterpret_cast<void*>(&AAE_ReturnNull),            // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&AnimAudioEvent_GetTypeId),  // +0x10 GetTypeId
    reinterpret_cast<void*>(&AAE_Return2),               // +0x18 GetVersion=2
    reinterpret_cast<void*>(&AAE_Return0),               // +0x20 Compile thunk
    reinterpret_cast<void*>(&AnimAudioEvent_Serialize),  // +0x28 Serialize
    reinterpret_cast<void*>(&AAE_Return0),               // +0x30 Cleanup
    reinterpret_cast<void*>(&AAE_Return0),               // +0x38 GetDesc forwarder
    reinterpret_cast<void*>(&AAE_ReturnNull),            // +0x40
    reinterpret_cast<void*>(&AnimAudioEvent_CreateInPlace), // +0x48 CreateInPlace
    reinterpret_cast<void*>(&AAE_ReturnNull),            // +0x50 guard
};

extern "C" void RegisterAnimAudioEventVtable() {
    RegisterExeAddr(kVtableAddr, reinterpret_cast<uintptr_t>(g_AAE_Vtable));
}

extern "C" void* AnimAudioEvent_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, kSize, 8);
    return p ? AnimAudioEvent_CtorImpl(p) : nullptr;
}
extern "C" void* AnimAudioEvent_CreateInPlace(void* mem) {
    return mem ? AnimAudioEvent_CtorImpl(mem) : nullptr;
}

REGISTER_TYPE(AnimAudioEvent, "Graphics::AnimationEvent",
              &AnimAudioEvent_Factory, &AnimAudioEvent_CreateInPlace);
#endif
