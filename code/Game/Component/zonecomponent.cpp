// code/Game/Component/zonecomponent.cpp
// Clean reimplementation of ZoneComponent lifecycle methods from Application-x64.exe.
//
// ZoneComponent (0x28 bytes) — minimal component, manages zone rendering.
// No additional fields beyond Component base.
//
// Vtable at 0x140164e30. Three non-trivial overrides:
//   Shutdown (0x1401302F0) — Serialize + conditional zone map registration
//   Draw     (0x1401301F0) — Zone rendering (complex, many deps)
//   OnCreate (0x140130080) — Zone creation + tree walk (complex)
//
// Draw and OnCreate are deeply coupled to zone rendering subsystem.
// We call the original exe functions for these.
//
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/zonecomponent_funcs.h"
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

typedef int (*fn_Serializer_GetVersion_t)(void* self);
static fn_Serializer_GetVersion_t s_fn_Serializer_GetVersion = nullptr;

// FUN_140088e20 — zone map registration
typedef void (*fn_ZoneMapAdd_t)(void* zoneMap, void* entityPtr);
static fn_ZoneMapAdd_t s_fn_ZoneMapAdd = nullptr;

// Original exe functions for complex methods
typedef void (*fn_Draw_Orig_t)(void* self);
static fn_Draw_Orig_t s_fn_Draw_Orig = nullptr;

typedef void (*fn_OnCreate_Orig_t)(void* self);
static fn_OnCreate_Orig_t s_fn_OnCreate_Orig = nullptr;

static void** s_pGameState = nullptr;

static bool s_initialized = false;

// ============================================================
// ZoneComponent::Shutdown (0x1401302F0)
// ============================================================
// Serializes component, then conditionally adds entity to zone map.
// If serializer version==0 and stream is reading (mode==0),
// registers the owning entity into the zone map at GameState+0xF0.

void ZoneComponent_Shutdown(ZoneComponent* self, void* stream) {
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer, stream, 1);

    if (s_fn_Serializer_GetVersion) {
        int version = s_fn_Serializer_GetVersion(serializer);
        if (version == 0 && *(int*)((uintptr_t)stream + 8) == 0) {
            void* entityPtr = self->owner;
            if (s_pGameState && *s_pGameState && s_fn_ZoneMapAdd) {
                void* zoneMap = *(void**)((uintptr_t)*s_pGameState + 0xF0);
                s_fn_ZoneMapAdd(zoneMap, &entityPtr);
            }
        }
    }

    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// ZoneComponent::Draw (0x1401301F0)
// ============================================================
// Complex zone rendering with IsDerivedFrom checks, coordinate
// extraction, and calls to zone rendering system.

void ZoneComponent_Draw(ZoneComponent* self) {
    if (s_fn_Draw_Orig)
        s_fn_Draw_Orig(self);
}

// ============================================================
// ZoneComponent::OnCreate (0x140130080)
// ============================================================
// Complex zone creation with coordinate extraction, IsDerivedFrom check,
// zone rendering setup, and binary tree walk for insertion.

void ZoneComponent_OnCreate(ZoneComponent* self) {
    if (s_fn_OnCreate_Orig)
        s_fn_OnCreate_Orig(self);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void ZoneComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_ZoneMapAdd = reinterpret_cast<fn_ZoneMapAdd_t>(
        ResolveAddr(Module::Exe, 0x140088e20));
    s_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    // Original exe functions for Draw and OnCreate
    s_fn_Draw_Orig = reinterpret_cast<fn_Draw_Orig_t>(
        ResolveAddr(Module::Exe, 0x1401301F0));
    s_fn_OnCreate_Orig = reinterpret_cast<fn_OnCreate_Orig_t>(
        ResolveAddr(Module::Exe, 0x140130080));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a48);
    if (slot) s_fn_Serializer_GetVersion = *reinterpret_cast<fn_Serializer_GetVersion_t*>(slot);

    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// In standalone the original exe is not mapped, so Object::Factory::CreateObject
// must resolve to a C++ factory here. Produces an inert-but-valid ZoneComponent
// (size 0x28, +0x20 capability flags 0x83, Object::Internal base + shared stub
// vtable, zeroed body). Size from ZoneComponent_Factory MemAlloc; flags from the
// Component_Ctor (+0x20=0x80) chain OR'd with the leaf ctor bits.
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(ZoneComponent, 0x28, 0x83)
REGISTER_TYPE(ZoneComponent, "Component",
              &ZoneComponent_Factory, &ZoneComponent_CreateInPlace);
#endif // STANDALONE_EXE
