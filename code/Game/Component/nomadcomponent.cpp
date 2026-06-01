// code/Game/Component/nomadcomponent.cpp
// Clean reimplementation of NomadComponent lifecycle methods from Application-x64.exe.
//
// NomadComponent (0x78 bytes) — Component with SHIFTED controller at +0x58.
// Fields: +0x28=IBP, +0x30=Vector3, +0x3C=Vector3, +0x48=Time, +0x50=int
//
// Vtable at 0x14015a020. Two non-trivial overrides:
//   Shutdown (0x14004fc40) — Serialize IBP + 2 Vector3 + int + Time
//   OnCreate (0x14004fd20) — Register with nomad list
//
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/nomadcomponent_funcs.h"
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

typedef void (*fn_Serializer_SerializeInt_t)(void* self, int* value, const char* name);
static fn_Serializer_SerializeInt_t s_fn_Serializer_SerializeInt = nullptr;

// Object::InternalBasePtr::Serialize
typedef void (*fn_IBP_Serialize_t)(void* ibp, void* serializer);
static fn_IBP_Serialize_t s_fn_IBP_Serialize = nullptr;

// Math::Vector3::Serialize
typedef void (*fn_Vector3_Serialize_t)(void* vec3, void* serializer);
static fn_Vector3_Serialize_t s_fn_Vector3_Serialize = nullptr;

// System::Time::Serialize
typedef void (*fn_Time_Serialize_t)(void* time, void* serializer);
static fn_Time_Serialize_t s_fn_Time_Serialize = nullptr;

// IsDerivedFrom
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_d = nullptr;

// FUN_140103480 — register with nomad list
typedef void (*fn_RegisterNomad_t)(void* nomadList, void* ibp);
static fn_RegisterNomad_t s_fn_RegisterNomad = nullptr;

static void** s_pGameState = nullptr;
static uint32_t* s_pComponentEntityTypeId = nullptr;

static bool s_initialized = false;

// ============================================================
// NomadComponent::Shutdown (0x14004fc40)
// ============================================================
// Component_Serialize + empty controller serializer + second serializer:
//   IBP at +0x28, Vector3 at +0x30, Vector3 at +0x3C, int at +0x50, Time at +0x48

void NomadComponent_Shutdown(NomadComponent* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    // Empty controller serializer block
    uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer1, stream, 0);
    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer1);

    // Main data serializer
    uint8_t serializer2[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer2, stream, 0);

    // Serialize IBP at +0x28
    if (s_fn_IBP_Serialize)
        s_fn_IBP_Serialize((void*)(selfAddr + 0x28), serializer2);

    // Validate IBP after deserialize
    void* ibpVal = *(void**)(selfAddr + 0x28);
    if (ibpVal != nullptr && s_fn_IsDerivedFrom_d && s_pComponentEntityTypeId) {
        if (!s_fn_IsDerivedFrom_d(ibpVal, *s_pComponentEntityTypeId)) {
            *(uintptr_t*)(selfAddr + 0x28) = 0;
        }
    }

    // Serialize Vector3 at +0x30 (12 bytes)
    if (s_fn_Vector3_Serialize)
        s_fn_Vector3_Serialize((void*)(selfAddr + 0x30), serializer2);

    // Serialize Vector3 at +0x3C (12 bytes)
    if (s_fn_Vector3_Serialize)
        s_fn_Vector3_Serialize((void*)(selfAddr + 0x3C), serializer2);

    // Serialize int at +0x50
    if (s_fn_Serializer_SerializeInt)
        s_fn_Serializer_SerializeInt(serializer2, (int*)(selfAddr + 0x50), nullptr);

    // Serialize Time at +0x48
    if (s_fn_Time_Serialize)
        s_fn_Time_Serialize((void*)(selfAddr + 0x48), serializer2);

    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer2);
}

// ============================================================
// NomadComponent::OnCreate (0x14004fd20)
// ============================================================
// Registers owner entity with game state nomad list at +0x120.

void NomadComponent_OnCreate(NomadComponent* self) {
    if (!s_pGameState || !*s_pGameState || !s_fn_RegisterNomad) return;

    // Create IBP from owner (copy the owner pointer)
    uintptr_t localIBP[4] = {0};
    localIBP[0] = *(uintptr_t*)((uintptr_t)self + 0x10);

    void* nomadList = *(void**)((uintptr_t)*s_pGameState + 0x120);
    s_fn_RegisterNomad(nomadList, localIBP);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void NomadComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_RegisterNomad = reinterpret_cast<fn_RegisterNomad_t>(
        ResolveAddr(Module::Exe, 0x140103480));
    s_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));
    s_pComponentEntityTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c72e8));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serializer_SerializeInt = *reinterpret_cast<fn_Serializer_SerializeInt_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_d = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152aa8);
    if (slot) s_fn_Time_Serialize = *reinterpret_cast<fn_Time_Serialize_t*>(slot);

    s_fn_IBP_Serialize = reinterpret_cast<fn_IBP_Serialize_t>(
        ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z"));
    s_fn_Vector3_Serialize = reinterpret_cast<fn_Vector3_Serialize_t>(
        ResolveExport("?Serialize@Vector3@Math@@QEAAXAEAVSerializer@IO@@@Z"));

    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// In standalone the original exe is not mapped, so Object::Factory::CreateObject
// must resolve to a C++ factory here. Produces an inert-but-valid NomadComponent
// (size 0x78, +0x20 capability flags 0x82, Object::Internal base + shared stub
// vtable, zeroed body). Size from NomadComponent_Factory MemAlloc; flags from the
// Component_Ctor (+0x20=0x80) chain OR'd with the leaf ctor bits.
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(NomadComponent, 0x78, 0x82)
REGISTER_TYPE(NomadComponent, "ControllerComponent",
              &NomadComponent_Factory, &NomadComponent_CreateInPlace);
#endif // STANDALONE_EXE
