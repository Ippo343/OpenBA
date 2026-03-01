// engine/reimpl/marketcomponent.cpp
// Reimplementation of MarketComponent lifecycle methods from Application-x64.exe.
//
// MarketComponent is a ResourceProduceComponent subclass (0x50 bytes) with no
// additional fields beyond the parent. Only one non-trivial vtable override:
//
// Methods reimplemented:
//   - Shutdown (0x14004ead0) — calls ResourceProduceComponent_Shutdown + empty Serializer block
//
// All other vtable slots are no-ops.

#include "MarketComponent.h"
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
// Local helper function pointers
// ============================================================

// ResourceProduceComponent_Shutdown (0x14012b4a0) — parent Shutdown
// Calls Component_Serialize + empty serializer + serialize produceFlag at +0x48
typedef void (*fn_ResourceProduceComponent_Shutdown_t)(void* self, void* stream);
static fn_ResourceProduceComponent_Shutdown_t fn_ParentShutdown = nullptr;

// IO::Serializer ctor/dtor via IAT
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

// ============================================================
// Shutdown (0x14004ead0)
// Calls parent ResourceProduceComponent_Shutdown, then creates/destroys
// an empty Serializer block (version 0).
// ============================================================
static void Reimpl_Shutdown(MarketComponent* self, void* stream) {
    g_lastHook = "MarketComponent::Shutdown";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[MarketComponent::Shutdown] self=%p stream=%p\n",
               (void*)self, stream);
        callCount++;
    }
#endif

    // Call parent ResourceProduceComponent::Shutdown
    fn_ParentShutdown(self, stream);

    // Empty serializer block (version 0)
    uint8_t serializerBuf[16];
    fn_Serializer_Ctor(serializerBuf, stream, 0);
    fn_Serializer_Dtor(serializerBuf);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallMarketComponentShutdown() {
    fn_ParentShutdown = reinterpret_cast<fn_ResourceProduceComponent_Shutdown_t>(
        ResolveAddr(Module::Exe, 0x14012b4a0));

    uintptr_t iatCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatCtor) fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(iatCtor);
    if (iatDtor) fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(iatDtor);

#ifdef _DEBUG
    printf("[MarketComponent] ParentShutdown = %p\n", (void*)fn_ParentShutdown);
    printf("[MarketComponent] Serializer_Ctor = %p\n", (void*)fn_Serializer_Ctor);
    printf("[MarketComponent] Serializer_Dtor = %p\n", (void*)fn_Serializer_Dtor);
#endif

    if (!fn_ParentShutdown || !fn_Serializer_Ctor || !fn_Serializer_Dtor) {
#ifdef _DEBUG
        printf("[MarketComponent] ERROR: failed to resolve helpers, skipping hook\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x14004ead0);
    // Prologue: PUSH RBX(2) + SUB RSP,0x30(4) + MOV RBX,RDX(3) + CALL(5) = 14
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[MarketComponent] Shutdown hooked at %p (prologue=14)\n", (void*)target);
#endif
}

void RemoveMarketComponentShutdown() { RemoveHook(&g_hookShutdown); }
