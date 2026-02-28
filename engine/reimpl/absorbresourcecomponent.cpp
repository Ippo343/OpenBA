// engine/reimpl/absorbresourcecomponent.cpp
// Reimplementation of AbsorbResourceComponent lifecycle methods from Application-x64.exe.
//
// AbsorbResourceComponent is a ControllerComponent subclass (0x48 bytes) with no
// additional fields. It has only one non-trivial vtable override:
//
// Methods reimplemented:
//   - Shutdown (0x140105c00) — serialize via Component_Serialize + two empty Serializer blocks
//
// All other vtable slots are no-ops (no Draw, no Update, no OnCreate, etc.)

#include "AbsorbResourceComponent.h"
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

// Component_Serialize (base class Shutdown) at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t fn_Component_Serialize = nullptr;

// IO::Serializer::Serializer(this, stream, version)
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;

// IO::Serializer::~Serializer(this)
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

// ============================================================
// Shutdown (0x140105c00)
// Pattern: Component_Serialize + two empty Serializer ctor/dtor pairs
// Same pattern as MarketLocationComponent::Shutdown and StorageLocationComponent::Shutdown
// ============================================================
static void Reimpl_Shutdown(AbsorbResourceComponent* self, void* stream) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[AbsorbResourceComponent::Shutdown] self=%p stream=%p\n",
               (void*)self, stream);
        callCount++;
    }
#endif

    // Call base Component_Serialize
    fn_Component_Serialize(self, stream);

    // First empty serializer block (version 0)
    uint8_t serializerBuf1[16];
    fn_Serializer_Ctor(serializerBuf1, stream, 0);
    fn_Serializer_Dtor(serializerBuf1);

    // Second empty serializer block (version 0)
    uint8_t serializerBuf2[16];
    fn_Serializer_Ctor(serializerBuf2, stream, 0);
    fn_Serializer_Dtor(serializerBuf2);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallAbsorbResourceComponentShutdown() {
    fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    // IO::Serializer ctor/dtor — resolved via IAT at [0x140152ad8] and [0x140152ad0]
    uintptr_t iatCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatCtor) fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(iatCtor);
    if (iatDtor) fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(iatDtor);

#ifdef _DEBUG
    printf("[AbsorbResourceComponent] Component_Serialize = %p\n", (void*)fn_Component_Serialize);
    printf("[AbsorbResourceComponent] Serializer_Ctor = %p\n", (void*)fn_Serializer_Ctor);
    printf("[AbsorbResourceComponent] Serializer_Dtor = %p\n", (void*)fn_Serializer_Dtor);
#endif

    if (!fn_Component_Serialize || !fn_Serializer_Ctor || !fn_Serializer_Dtor) {
#ifdef _DEBUG
        printf("[AbsorbResourceComponent] ERROR: failed to resolve helpers, skipping hook\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x140105c00);
    // Prologue: PUSH RBX(2) + SUB RSP,0x40(4) + MOV RBX,RDX(3) = 9, + CALL(5) = 14
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[AbsorbResourceComponent] Shutdown hooked at %p (prologue=14)\n", (void*)target);
#endif
}

void RemoveAbsorbResourceComponentShutdown() { RemoveHook(&g_hookShutdown); }
