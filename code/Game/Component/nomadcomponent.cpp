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
// Reimpl of FUN_140103480 (Component reg wave "people")
// Ghidra mislabels this "TradeWork_impl_3480"; its real callers are entity-removal
// paths (CitizenComponent_OnRemove, DestroyDescription helpers, NomadComponent_OnCreate):
// it tears down any controller/trade reference for the entity. Steps:
//   1. If the controller at +0x28 reports active (vtable+0x50), call FUN_140102b60(self,0).
//   2. TradeWork_impl_c690 detaches the entity from the work list (+0x60).
//   3. Walk the UI's open dialogs; for any whose controller derives from
//      g_TypeId_PageToggleUI (DAT_1401c4f00) and whose bound entity (+0x30) == param_2,
//      close that dialog and return.
// Sub-deps + the member-return UI calls resolved via .rdata dispatch slots.
// Non-static for registration. Faithful structural port of 0x140103480.
// ============================================================
typedef char  (*fn_ctrl_active_t)(void* ctrl);             // vtable+0x50
typedef void  (*fn_FUN_140102b60_t)(int64_t self, int v);  // 0x140102b60
typedef void  (*fn_TradeWork_c690_t)(int64_t* listBase, void* ibp); // 0x14002c690
typedef void* (*fn_GetFirstDialog_t)(void* ui);            // slot 0x140151ca0 (writes out via hidden ptr in orig)
typedef void* (*fn_GetController_t)(void* dialog, void* retbuf); // slot 0x140151768 (member, non-trivial ret)
typedef void  (*fn_CloseDialog_slot_t)(void* ui, void* dlgPtr);// slot 0x140151550
typedef void  (*fn_IBP_CtorFrom_t)(void* dst, void* src);  // slot 0x140152a90

static fn_FUN_140102b60_t   s_fn_FUN_140102b60 = nullptr;
static fn_TradeWork_c690_t  s_fn_TradeWork_c690 = nullptr;
static fn_GetFirstDialog_t  s_fn_GetFirstDialog = nullptr;
static fn_GetController_t    s_fn_GetController = nullptr;
static fn_CloseDialog_slot_t s_fn_CloseDialog_slot = nullptr;
static fn_IBP_CtorFrom_t     s_fn_IBP_CtorFrom = nullptr;
static uint32_t*             s_TypeId_PageToggleUI = nullptr; // DAT_1401c4f00
static bool s_fn3480Resolved = false;

static void Ensure3480() {
    if (s_fn3480Resolved) return;
    s_fn_FUN_140102b60 = reinterpret_cast<fn_FUN_140102b60_t>(ResolveAddr(Module::Exe, 0x140102b60));
    s_fn_TradeWork_c690 = reinterpret_cast<fn_TradeWork_c690_t>(ResolveAddr(Module::Exe, 0x14002c690));
    s_TypeId_PageToggleUI = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4f00));
    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140151ca0);
    if (slot) s_fn_GetFirstDialog = *reinterpret_cast<fn_GetFirstDialog_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140151768);
    if (slot) s_fn_GetController = *reinterpret_cast<fn_GetController_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140151550);
    if (slot) s_fn_CloseDialog_slot = *reinterpret_cast<fn_CloseDialog_slot_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a90);
    if (slot) s_fn_IBP_CtorFrom = *reinterpret_cast<fn_IBP_CtorFrom_t*>(slot);
    s_fn3480Resolved = true;
}

void NomadComp_FUN_140103480(int64_t param_1, void* param_2) {
    Ensure3480();
    uint8_t* self = reinterpret_cast<uint8_t*>(param_1);

    if (*reinterpret_cast<int64_t*>(self + 0x28) != 0) {
        fn_InternalBasePtr_CheckPtr(self + 0x28);
        int64_t ctrl = *reinterpret_cast<int64_t*>(self + 0x28);
        int64_t vt = *reinterpret_cast<int64_t*>(ctrl);
        char active = (*reinterpret_cast<fn_ctrl_active_t*>(vt + 0x50))(reinterpret_cast<void*>(ctrl));
        if (active != '\0' && s_fn_FUN_140102b60) s_fn_FUN_140102b60(param_1, 0);
    }

    int64_t entity = *reinterpret_cast<int64_t*>(param_2);
    if (s_fn_TradeWork_c690)
        s_fn_TradeWork_c690(reinterpret_cast<int64_t*>(self + 0x60), &entity);

    void* ui = *reinterpret_cast<void**>(
        reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0x98);
    if (!s_fn_GetFirstDialog) return;
    int64_t* dlg = reinterpret_cast<int64_t*>(s_fn_GetFirstDialog(ui));
    while (dlg != nullptr) {
        int64_t* cur = dlg;
        if (*dlg != 0) {
            fn_InternalBasePtr_CheckPtr(dlg);
            void* ctrlBuf[2] = {nullptr, nullptr};
            if (s_fn_GetController) s_fn_GetController(reinterpret_cast<void*>(*cur), ctrlBuf);
            void* found = nullptr;
            if (ctrlBuf[0] != nullptr) {
                fn_InternalBasePtr_CheckPtr(ctrlBuf);
                if (s_TypeId_PageToggleUI && fn_IsDerivedFrom(ctrlBuf[0], *s_TypeId_PageToggleUI))
                    found = ctrlBuf[0];
            }
            int64_t ibp = 0;
            fn_InternalBasePtr_Ctor(&ibp, found);
            if (ibp != 0) {
                fn_InternalBasePtr_CheckPtr(&ibp);
                int64_t bound = *reinterpret_cast<int64_t*>(ibp + 0x30);
                fn_InternalBasePtr_CheckPtr(param_2);
                if (bound == *reinterpret_cast<int64_t*>(param_2)) {
                    void* dlgPtr = reinterpret_cast<void*>(*dlg);
                    if (s_fn_CloseDialog_slot) s_fn_CloseDialog_slot(ui, &dlgPtr);
                    return;
                }
            }
        }
        dlg = reinterpret_cast<int64_t*>(dlg[2]);
    }
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
