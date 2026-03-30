// code/Game/Component/naturalresourcecomponent.cpp
// Clean reimplementation of NaturalResourceComponent lifecycle methods.
//
// NaturalResourceComponent (0x30 bytes) extends Component with:
//   +0x28: float  resourceScale  (initialized to 1.0f)
//   +0x2C: uint16 resourceFlags  (initialized to 0x100)
//
// Vtable at 0x140163e10. Two non-trivial overrides:
//   Shutdown (0x140125cf0) — Serialize resourceScale + 2 bool fields
//   OnRemove (0x140125c40) — Complex animation/path logic
//
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/naturalresourcecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

typedef void (*fn_SerializeFloat_t)(void* self, float* value, const char* name);
static fn_SerializeFloat_t s_fn_SerializeFloat = nullptr;

typedef void (*fn_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_SerializeBool_t s_fn_SerializeBool = nullptr;

// CheckPtr
typedef void (*fn_CheckPtr_t)(void* ptr);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;

// Entity_ClearPathQueue(entity, outIBP)
typedef void (*fn_ClearPathQueue_t)(void* entity, void* outIBP);
static fn_ClearPathQueue_t s_fn_ClearPathQueue = nullptr;

// Graphics::MeshInstance::IsAnimating(meshInstance) -> bool
typedef bool (*fn_IsAnimating_t)(void* meshInstance);
static fn_IsAnimating_t s_fn_IsAnimating = nullptr;

// FUN_1400c3470 — entity flag update helper
typedef void (*fn_EntityFlagUpdate_t)(void* entity);
static fn_EntityFlagUpdate_t s_fn_EntityFlagUpdate = nullptr;

// FUN_1401260f0 — reassign natural resource
typedef void (*fn_ReassignResource_t)(void* self, void* ibp, char flag);
static fn_ReassignResource_t s_fn_ReassignResource = nullptr;

static bool s_initialized = false;

// ============================================================
// NaturalResourceComponent::Shutdown (0x140125cf0)
// ============================================================

void NaturalResourceComponent_Shutdown(NaturalResourceComponent* self, void* stream) {
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer, stream, 0);

    if (s_fn_SerializeFloat)
        s_fn_SerializeFloat(serializer, &self->resourceScale, nullptr);

    // resourceFlags is uint16_t at +0x2C; the original serializes
    // the two bytes individually as bool at +0x2C and +0x2D
    bool* flagLow  = reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(self) + 0x2C);
    bool* flagHigh = reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(self) + 0x2D);
    if (s_fn_SerializeBool) {
        s_fn_SerializeBool(serializer, flagLow, nullptr);
        s_fn_SerializeBool(serializer, flagHigh, nullptr);
    }

    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// NaturalResourceComponent::OnRemove (0x140125c40)
// ============================================================
// Checks animation state, clears path queue, modifies flags,
// potentially reassigns natural resource ownership.

void NaturalResourceComponent_OnRemove(NaturalResourceComponent* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    // CheckPtr on owner IBP
    if (s_fn_CheckPtr)
        s_fn_CheckPtr((void*)(selfAddr + 0x10));

    // Clear path queue — get model component from entity
    uintptr_t modelIBP = 0;
    if (s_fn_ClearPathQueue)
        s_fn_ClearPathQueue(*(void**)(selfAddr + 0x10), &modelIBP);

    // CheckPtr on model IBP
    if (s_fn_CheckPtr)
        s_fn_CheckPtr(&modelIBP);

    // Check if mesh is animating
    if (s_fn_IsAnimating) {
        bool isAnimating = s_fn_IsAnimating((void*)(modelIBP + 0x28));
        if (isAnimating) return;
    }

    // Clear bit 2 of flags (+0x20)
    uint32_t oldFlags = *(uint32_t*)(selfAddr + 0x20);
    uint32_t newFlags = oldFlags & ~0x4u;
    *(uint32_t*)(selfAddr + 0x20) = newFlags;

    // If flags changed in low 16 bits and entity exists, update
    if (((newFlags ^ oldFlags) & 0xFFFF) != 0 && *(uintptr_t*)(selfAddr + 0x10) != 0) {
        if (s_fn_CheckPtr)
            s_fn_CheckPtr((void*)(selfAddr + 0x10));
        if (s_fn_EntityFlagUpdate)
            s_fn_EntityFlagUpdate(*(void**)(selfAddr + 0x10));
    }

    // If bit 21 of flags is set, reassign resource
    if ((*(uint32_t*)(selfAddr + 0x20) >> 0x15) & 1) {
        uint8_t nullIBP[16];
        if (fn_InternalBasePtr_Ctor)
            fn_InternalBasePtr_Ctor(nullIBP, nullptr);
        if (s_fn_ReassignResource)
            s_fn_ReassignResource((void*)selfAddr, nullIBP, 0);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void NaturalResourceComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a60);
    if (slot) s_fn_SerializeFloat = *reinterpret_cast<fn_SerializeFloat_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a58);
    if (slot) s_fn_SerializeBool = *reinterpret_cast<fn_SerializeBool_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ab0);
    if (slot) s_fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(slot);

    s_fn_ClearPathQueue = reinterpret_cast<fn_ClearPathQueue_t>(
        ResolveAddr(Module::Exe, 0x140005200));  // Entity_ClearPathQueue used in decomp
    s_fn_IsAnimating = reinterpret_cast<fn_IsAnimating_t>(
        ResolveExport("?IsAnimating@MeshInstance@Graphics@@QEBA_NXZ"));
    s_fn_EntityFlagUpdate = reinterpret_cast<fn_EntityFlagUpdate_t>(
        ResolveAddr(Module::Exe, 0x1400c3470));
    s_fn_ReassignResource = reinterpret_cast<fn_ReassignResource_t>(
        ResolveAddr(Module::Exe, 0x1401260f0));

    s_initialized = true;
}
