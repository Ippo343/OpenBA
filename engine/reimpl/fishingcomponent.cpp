// engine/reimpl/fishingcomponent.cpp
// Reimplementation of FishingComponent lifecycle methods.
//
// FishingComponent (0x58 bytes) extends ResourceProduceComponent with:
//   +0x50: uint8  fishingFlag  (initialized to 0, serialized as bool)
//   +0x54: float  fishingRate  (serialized by Shutdown)
//
// Vtable at 0x140163230 (12 slots):
//   +0x00 Dtor           = 0x140034560 (too small to hook)
//   +0x08 GetDescription = 0x140034340 (7 bytes, unhookable)
//   +0x10 Startup        = 0x140034330 (7 bytes, unhookable)
//   +0x18 Shutdown       = 0x14011f050 (HOOKED — base + bool + float)
//   +0x20 Serialize      = 0x140118a50 (no-op)
//   +0x28 Slot5          = 0x140118a60 (no-op)
//   +0x30 Draw           = 0x140118820 (no-op)
//   +0x38 OnCreate       = 0x140118830 (no-op)
//   +0x40 OnRemove       = 0x140118a70 (no-op)
//   +0x48 Update         = 0x140118a80 (no-op)
//   +0x50 Slot10         = 0x140118a90 (no-op)
//   +0x58 OnUpdate2      = 0x140118aa0 (no-op)
//
// Only Shutdown is hookable. All other overrides are no-ops.

#include "FishingComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================

static HookInfo g_hookShutdown;

// ============================================================
// Local function pointers
// ============================================================

// ResourceProduceComponent::Shutdown (base) at 0x14012b4a0
typedef void (*fn_ResourceProduceShutdown_t)(void* self, void* stream);
static fn_ResourceProduceShutdown_t fn_ResourceProduceShutdown = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

typedef void (*fn_Serializer_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_Serializer_SerializeBool_t fn_Serializer_SerializeBool = nullptr;

typedef void (*fn_Serializer_SerializeFloat_t)(void* self, float* value, const char* name);
static fn_Serializer_SerializeFloat_t fn_Serializer_SerializeFloat = nullptr;

// ============================================================
// Shutdown (0x14011f050)
// Decompiled:
//   ResourceProduceComponent::Shutdown(self, stream)
//   Serializer s(stream, 0)
//   s.Serialize(&self[0x50], null)  // bool: fishingFlag
//   s.Serialize(&self[0x54], null)  // float: fishingRate
//   ~Serializer()
//
// Prologue: PUSH RDI(1) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 14 → 15 bytes
// ============================================================

static void Reimpl_Shutdown(FishingComponent* self, void* stream) {
    fn_ResourceProduceShutdown(self, stream);

    uint8_t serializer[24];
    fn_Serializer_Ctor(serializer, stream, 0);

    bool* pFishingFlag = reinterpret_cast<bool*>(&self->fishingFlag);
    fn_Serializer_SerializeBool(serializer, pFishingFlag, nullptr);
    fn_Serializer_SerializeFloat(serializer, &self->fishingRate, nullptr);

    fn_Serializer_Dtor(serializer);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallFishingComponentShutdown() {
    fn_ResourceProduceShutdown = reinterpret_cast<fn_ResourceProduceShutdown_t>(
        ResolveAddr(Module::Exe, 0x14012b4a0));
    fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ad8)));
    fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ad0)));
    fn_Serializer_SerializeBool = reinterpret_cast<fn_Serializer_SerializeBool_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152a58)));
    fn_Serializer_SerializeFloat = reinterpret_cast<fn_Serializer_SerializeFloat_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152a60)));

    if (!fn_ResourceProduceShutdown || !fn_Serializer_Ctor ||
        !fn_Serializer_Dtor || !fn_Serializer_SerializeBool ||
        !fn_Serializer_SerializeFloat) {
#ifdef _DEBUG
        printf("[FishingComponent] ERROR: Failed to resolve functions\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x14011f050);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[FishingComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}

void RemoveFishingComponentShutdown() {
    RemoveHook(&g_hookShutdown);
}
