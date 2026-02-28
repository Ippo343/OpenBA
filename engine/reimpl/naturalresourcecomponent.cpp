// engine/reimpl/naturalresourcecomponent.cpp
// Reimplementation of NaturalResourceComponent lifecycle methods.
//
// NaturalResourceComponent (0x30 bytes) extends Component with:
//   +0x28: float  resourceScale  (initialized to 1.0f)
//   +0x2C: uint16 resourceFlags  (initialized to 0x100 — two packed bytes: 0x00, 0x01)
//
// Vtable at 0x140163e10 (12 slots):
//   +0x00 Dtor           = 0x1400366b0 (too small to hook)
//   +0x08 GetDescription = 0x140036540 (7 bytes, unhookable)
//   +0x10 Startup        = 0x140036530 (7 bytes, unhookable)
//   +0x18 Shutdown       = 0x140125cf0 (HOOKED — serializes resourceScale + 2 bool fields)
//   +0x20 Serialize      = 0x140118a50 (no-op)
//   +0x28 Slot5          = 0x140118a60 (no-op)
//   +0x30 Draw           = 0x140118820 (no-op)
//   +0x38 OnCreate       = 0x140118830 (no-op)
//   +0x40 OnRemove       = 0x140125c40 (HOOKED — passthrough, complex animation/path logic)
//   +0x48 Update         = 0x140118a80 (no-op)
//   +0x50 Slot10         = 0x140118a90 (no-op)
//   +0x58 OnUpdate2      = 0x140118aa0 (no-op)

#include "NaturalResourceComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================

static HookInfo g_hookShutdown;
static HookInfo g_hookOnRemove;

// ============================================================
// Local function pointers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize overloads (through .rdata dispatch)
typedef void (*fn_Serializer_SerializeFloat_t)(void* self, float* value, const char* name);
static fn_Serializer_SerializeFloat_t fn_Serializer_SerializeFloat = nullptr;

typedef void (*fn_Serializer_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_Serializer_SerializeBool_t fn_Serializer_SerializeBool = nullptr;

// ============================================================
// Shutdown (0x140125cf0)
// Serializes component state:
//   1. Component_Serialize (base: owner ref, flags)
//   2. IO::Serializer block with 3 fields:
//      - float at +0x28 (resourceScale)
//      - bool  at +0x2C (low byte of resourceFlags)
//      - bool  at +0x2D (high byte of resourceFlags)
//
// Prologue: PUSH RDI(1) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 14 bytes
// Next instruction at offset 15. Safe prologue size: 15.
// ============================================================

static void Reimpl_Shutdown(NaturalResourceComponent* self, void* stream) {
    fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    fn_Serializer_Ctor(serializer, stream, 0);

    fn_Serializer_SerializeFloat(serializer, &self->resourceScale, nullptr);

    // resourceFlags is uint16_t at +0x2C; the original serializes
    // the two bytes individually as bool at +0x2C and +0x2D
    bool* flagLow  = reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(self) + 0x2C);
    bool* flagHigh = reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(self) + 0x2D);
    fn_Serializer_SerializeBool(serializer, flagLow, nullptr);
    fn_Serializer_SerializeBool(serializer, flagHigh, nullptr);

    fn_Serializer_Dtor(serializer);
}

// ============================================================
// OnRemove (0x140125c40) — passthrough
// Complex logic: checks animation state, clears path queue,
// modifies flags, and potentially reassigns natural resource ownership.
// Too many dependencies to cleanly reimplement; passthrough via trampoline.
// ============================================================

typedef void (*fn_OnRemove_Orig_t)(NaturalResourceComponent* self);

static void Reimpl_OnRemove(NaturalResourceComponent* self) {
    ((fn_OnRemove_Orig_t)g_hookOnRemove.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallNaturalResourceComponentShutdown() {
    fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ad8)));
    fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ad0)));
    fn_Serializer_SerializeFloat = reinterpret_cast<fn_Serializer_SerializeFloat_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152a60)));
    fn_Serializer_SerializeBool = reinterpret_cast<fn_Serializer_SerializeBool_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152a58)));

    if (!fn_Component_Serialize || !fn_Serializer_Ctor || !fn_Serializer_Dtor ||
        !fn_Serializer_SerializeFloat || !fn_Serializer_SerializeBool) {
#ifdef _DEBUG
        printf("[NaturalResourceComponent] ERROR: Failed to resolve serializer functions\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x140125cf0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[NaturalResourceComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}

void RemoveNaturalResourceComponentShutdown() {
    RemoveHook(&g_hookShutdown);
}

void InstallNaturalResourceComponentOnRemove() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140125c40);
    InstallHook(target, (void*)Reimpl_OnRemove, &g_hookOnRemove, 13);
#ifdef _DEBUG
    printf("[NaturalResourceComponent] OnRemove hooked (passthrough) at %p\n", (void*)target);
#endif
}

void RemoveNaturalResourceComponentOnRemove() {
    RemoveHook(&g_hookOnRemove);
}
