// engine/reimpl/forestercomponent.cpp
// Reimplementation of ForesterComponent lifecycle methods.
//
// ForesterComponent (0x58 bytes) extends ResourceProduceComponent with:
//   +0x50: uint16 foresterFlags (initialized to 0x101 — two packed bytes)
//
// Vtable at 0x140163460 (12 slots):
//   +0x00 Dtor           = 0x140034a70 (too small to hook)
//   +0x08 GetDescription = 0x140034860 (7 bytes, unhookable)
//   +0x10 Startup        = 0x140034850 (7 bytes, unhookable)
//   +0x18 Shutdown       = 0x14011f960 (HOOKED — base + 2 bool fields)
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

#include "ForesterComponent.h"
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
// Calls Component_Serialize + empty serializer + serializes +0x48 (produceFlag)
typedef void (*fn_ResourceProduceShutdown_t)(void* self, void* stream);
static fn_ResourceProduceShutdown_t fn_ResourceProduceShutdown = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

typedef void (*fn_Serializer_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_Serializer_SerializeBool_t fn_Serializer_SerializeBool = nullptr;

// ============================================================
// Shutdown (0x14011f960)
// Decompiled:
//   FUN_14012b4a0(self, stream)    // ResourceProduceComponent::Shutdown
//   Serializer s(stream, 0)
//   s.Serialize(&self[0x50], null)  // bool at +0x50 (foresterFlags low byte)
//   s.Serialize(&self[0x51], null)  // bool at +0x51 (foresterFlags high byte)
//   ~Serializer()
//
// Prologue: PUSH RDI(1) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 14
// Next instruction at offset 15. Safe prologue size: 15.
// ============================================================

static void Reimpl_Shutdown(ForesterComponent* self, void* stream) {
    g_lastHook = "ForesterComponent::Shutdown";
    // Call base ResourceProduceComponent::Shutdown
    fn_ResourceProduceShutdown(self, stream);

    // Serialize ForesterComponent's own fields
    uint8_t serializer[24];
    fn_Serializer_Ctor(serializer, stream, 0);

    // foresterFlags at +0x50 is uint16, serialized as two individual bools
    bool* flagLow  = reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(self) + 0x50);
    bool* flagHigh = reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(self) + 0x51);
    fn_Serializer_SerializeBool(serializer, flagLow, nullptr);
    fn_Serializer_SerializeBool(serializer, flagHigh, nullptr);

    fn_Serializer_Dtor(serializer);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallForesterComponentShutdown() {
    fn_ResourceProduceShutdown = reinterpret_cast<fn_ResourceProduceShutdown_t>(
        ResolveAddr(Module::Exe, 0x14012b4a0));
    fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ad8)));
    fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ad0)));
    fn_Serializer_SerializeBool = reinterpret_cast<fn_Serializer_SerializeBool_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152a58)));

    if (!fn_ResourceProduceShutdown || !fn_Serializer_Ctor ||
        !fn_Serializer_Dtor || !fn_Serializer_SerializeBool) {
#ifdef _DEBUG
        printf("[ForesterComponent] ERROR: Failed to resolve functions\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x14011f960);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[ForesterComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}

void RemoveForesterComponentShutdown() {
    RemoveHook(&g_hookShutdown);
}
