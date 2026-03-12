// engine/reimpl/animatenaturalresource.cpp
// Reimplementation of AnimateNaturalResource action methods from Application-x64.exe.
//
// AnimateNaturalResource — 0x30 bytes. Triggers and waits for animation on a natural resource.
// Inherits: Object::Internal -> Action (0x20) -> AnimateNaturalResource
// Vtable at 0x14015fb10 (10 slots).
//
// Overrides vs Action base:
//   Serialize  (0x1400c5660) — serializes status + entity ref at +0x20, int at +0x28, bool at +0x2C
//   _purecall  (0x1400c5600) — looks up model comp, sets animation index, returns _field2C
//   Execute    (0x1400c55b0) — checks if entity model is still animating
//   CheckEntity(0x1400c55a0) — 3 bytes (ret 0), too small to hook
//   GetTarget  (0x1400c5560) — returns true if param matches +0x20
//   Enter/Exit use base no-ops.

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Helper function pointers
// ============================================================

// Object::InternalBasePtr::CheckPtr
typedef void (*fn_CheckPtr_t)(void* ibp);
static fn_CheckPtr_t fn_CheckPtr = nullptr;

// Object::InternalBasePtr::Serialize
typedef void (*fn_IBP_Serialize_t)(void* ibp, void* serializer);
static fn_IBP_Serialize_t fn_IBP_Serialize = nullptr;

// IO::Serializer lifecycle
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize (bool / int overloads)
typedef void (*fn_Serialize_Bool_t)(void* self, void* field, const char* name);
static fn_Serialize_Bool_t fn_Serialize_Bool = nullptr;
typedef void (*fn_Serialize_Int_t)(void* self, void* field, const char* name);
static fn_Serialize_Int_t fn_Serialize_Int = nullptr;

// FUN_1400052c0 — get NaturalResourceComponent from entity
typedef void* (*fn_GetNRComp_t)(void* entity, void* outIBP);
static fn_GetNRComp_t fn_GetNRComp = nullptr;

// FUN_140126440 — set animation index on NaturalResourceComponent
typedef void (*fn_SetAnimIndex_t)(void* nrComp, uint32_t animIdx);
static fn_SetAnimIndex_t fn_SetAnimIndex = nullptr;

// Entity_ClearPathQueue / get model component (FUN_140005500 used here as entity->modelcomp)
typedef void* (*fn_GetModelComp_t)(void* entity, void* outIBP);
static fn_GetModelComp_t fn_GetModelComp = nullptr;

// Graphics::MeshInstance::IsAnimating(meshInst)
typedef bool (*fn_IsAnimating_t)(void* meshInst);
static fn_IsAnimating_t fn_IsAnimating = nullptr;

static uint32_t* g_TypeInfo_ComponentEntity = nullptr;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookPurecall;
static HookInfo g_hookExecute;
static HookInfo g_hookGetTarget;

// ============================================================
// AnimateNaturalResource::Serialize (0x1400c5660)
// Serializes: status(+0x18), entity ptr(+0x20) with type check, int(+0x28), bool(+0x2C)
// ============================================================
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "AnimateNaturalResource::Serialize";
    uint8_t* s = (uint8_t*)self;
    uint8_t ser1[16];
    uint8_t ser2[24];

    // Serialize status
    fn_Serializer_Ctor(ser1, stream, 0);
    fn_Serialize_Bool(ser1, s + 0x18, nullptr);
    fn_Serializer_Dtor(ser1);

    // Serialize entity ref + extra fields
    fn_Serializer_Ctor(ser2, stream, 0);
    fn_IBP_Serialize(s + 0x20, ser2);
    if (*(void**)(s + 0x20) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x20), *g_TypeInfo_ComponentEntity)) {
            *(void**)(s + 0x20) = nullptr;
        }
    }
    fn_Serialize_Int(ser2, s + 0x28, nullptr);
    fn_Serialize_Bool(ser2, s + 0x2C, nullptr);
    fn_Serializer_Dtor(ser2);
}

// ============================================================
// AnimateNaturalResource::_purecall (0x1400c5600)
// Gets NR component from entity at +0x20, calls SetAnimIndex with +0x28.
// Returns _field2C (bool at +0x2C).
// ============================================================
static int Reimpl_Purecall(void* self, void* actionStack) {
    (void)actionStack;
    g_lastHook = "AnimateNaturalResource::Purecall";
    uint8_t* s = (uint8_t*)self;
    void* localIBP;

    fn_CheckPtr(s + 0x20);
    fn_GetNRComp(*(void**)(s + 0x20), &localIBP);
    fn_CheckPtr(&localIBP);
    fn_SetAnimIndex(localIBP, *(uint32_t*)(s + 0x28));
    return (*(uint8_t*)(s + 0x2C) != 0) ? 1 : 0;
}

// ============================================================
// AnimateNaturalResource::Execute (0x1400c55b0)
// Checks if entity's model is still animating. Returns true while animating.
// ============================================================
static int Reimpl_Execute(void* self, void* actionStack, void* time) {
    (void)actionStack;
    (void)time;
    g_lastHook = "AnimateNaturalResource::Execute";
    uint8_t* s = (uint8_t*)self;
    void* localIBP[4];

    fn_CheckPtr(s + 0x20);
    fn_GetModelComp(*(void**)(s + 0x20), localIBP);
    fn_CheckPtr(localIBP);
    return fn_IsAnimating((uint8_t*)(localIBP[0]) + 0x28) ? 1 : 0;
}

// ============================================================
// AnimateNaturalResource::GetTarget (0x1400c5560)
// Returns true if param matches entity at +0x20.
// ============================================================
static bool Reimpl_GetTarget(void* self, void* param) {
    g_lastHook = "AnimateNaturalResource::GetTarget";
    uint8_t* s = (uint8_t*)self;
    fn_CheckPtr(param);
    return *(void**)(s + 0x20) == *(void**)param;
}

// CheckEntity at 0x1400c55a0 is 3 bytes (ret 0), too small to hook.

// ============================================================
// Install / Remove
// ============================================================

static void ResolveHelpers() {
    // CheckPtr is called indirectly via IAT at [0x140152ab0]
    uintptr_t iatEntry = ResolveAddr(Module::Exe, 0x140152ab0);
    if (iatEntry) fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(iatEntry);
    g_TypeInfo_ComponentEntity = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c4218);
    fn_Serializer_Ctor = (fn_Serializer_Ctor_t)ResolveExport("?Serializer@IO@@QEAA@PEAVStream@1@H@Z");
    fn_Serializer_Dtor = (fn_Serializer_Dtor_t)ResolveExport("??1Serializer@IO@@QEAA@XZ");
    fn_Serialize_Bool = (fn_Serialize_Bool_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEA_NPEBD@Z");
    fn_Serialize_Int = (fn_Serialize_Int_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAint@PEBD@Z");
    fn_IBP_Serialize = (fn_IBP_Serialize_t)ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z");
    fn_GetNRComp = (fn_GetNRComp_t)ResolveAddr(Module::Exe, 0x1400052c0);
    fn_SetAnimIndex = (fn_SetAnimIndex_t)ResolveAddr(Module::Exe, 0x140126440);
    fn_GetModelComp = (fn_GetModelComp_t)ResolveAddr(Module::Exe, 0x140003f60);
    // IsAnimating is called indirectly via IAT at [0x1401514b0]
    uintptr_t iatIsAnim = ResolveAddr(Module::Exe, 0x1401514b0);
    if (iatIsAnim) fn_IsAnimating = *reinterpret_cast<fn_IsAnimating_t*>(iatIsAnim);
}

void InstallAnimateNaturalResourceSerialize() {
    ResolveHelpers();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5660);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[AnimateNaturalResource] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveAnimateNaturalResourceSerialize() { RemoveHook(&g_hookSerialize); }

void InstallAnimateNaturalResourcePurecall() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5600);
    InstallHook(target, (void*)Reimpl_Purecall, &g_hookPurecall, 13);
#ifdef _DEBUG
    printf("[AnimateNaturalResource] Purecall hooked at %p\n", (void*)target);
#endif
}
void RemoveAnimateNaturalResourcePurecall() { RemoveHook(&g_hookPurecall); }

void InstallAnimateNaturalResourceExecute() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c55b0);
    InstallHook(target, (void*)Reimpl_Execute, &g_hookExecute, 13);
#ifdef _DEBUG
    printf("[AnimateNaturalResource] Execute hooked at %p\n", (void*)target);
#endif
}
void RemoveAnimateNaturalResourceExecute() { RemoveHook(&g_hookExecute); }

void InstallAnimateNaturalResourceGetTarget() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5560);
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 13);
#ifdef _DEBUG
    printf("[AnimateNaturalResource] GetTarget hooked at %p\n", (void*)target);
#endif
}
void RemoveAnimateNaturalResourceGetTarget() { RemoveHook(&g_hookGetTarget); }
