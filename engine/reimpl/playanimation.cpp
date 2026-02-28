// engine/reimpl/playanimation.cpp
// Reimplementation of PlayAnimation action methods from Application-x64.exe.
//
// PlayAnimation — 0x40 bytes. Plays an animation on a citizen entity.
// Inherits: Object::Internal -> Action (0x20) -> PlayAnimation
// Vtable at 0x140158950 (10 slots).
//
// Overrides vs Action base:
//   Serialize   (0x140042480) — serializes status, entity(+0x20) ComponentEntity, int(+0x28), Time(+0x30), Time(+0x38)
//   _purecall   (0x140042420) — finds family component, calls animation setup
//   Execute     (0x140042400) — accumulates time in _field38, returns (_field38 < _field30)
//   CheckEntity (0x1400423f0) — 3 bytes (ret 0), too small to hook
//   GetTarget   (0x1400423b0) — returns true if param matches entity at +0x20
//   Enter uses base. Exit uses base.
//
// Field layout (0x40 bytes):
//   +0x20: void*    citizenEntity  (entity ref, ComponentEntity type)
//   +0x28: int32_t  animationId    (animation index/hash)
//   +0x2C: padding
//   +0x30: int64_t  duration       (Time — target animation duration in ticks)
//   +0x38: int64_t  elapsed        (Time — accumulated elapsed ticks)

#include "PlayAnimation.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Helper function pointers
// ============================================================

typedef void (*fn_CheckPtr_t)(void* ibp);
static fn_CheckPtr_t fn_CheckPtr = nullptr;

typedef void (*fn_IBP_Serialize_t)(void* ibp, void* serializer);
static fn_IBP_Serialize_t fn_IBP_Serialize = nullptr;

typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;
typedef void (*fn_Serialize_Bool_t)(void* self, void* field, const char* name);
static fn_Serialize_Bool_t fn_Serialize_Bool = nullptr;
typedef void (*fn_Serialize_Int_t)(void* self, void* field, const char* name);
static fn_Serialize_Int_t fn_Serialize_Int = nullptr;

// System::Time::Serialize(this, Serializer&)
typedef void (*fn_Time_Serialize_t)(void* time, void* serializer);
static fn_Time_Serialize_t fn_Time_Serialize = nullptr;

// Entity_FindComponent_Family — finds family/character component
typedef void (*fn_FindFamily_t)(void* entity, void* outIBP);
static fn_FindFamily_t fn_FindFamily = nullptr;

// FUN_14010a1f0 — set animation on family component
// Params: familyComp, animId, float_param, float_param
typedef void (*fn_SetAnimation_t)(void* familyComp, uint32_t animId, float p1, float p2);
static fn_SetAnimation_t fn_SetAnimation = nullptr;

// DAT_14017214c — float constant used in animation setup
static float* g_AnimFloat = nullptr;

static uint32_t* g_TypeInfo_ComponentEntity = nullptr;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookPurecall;
static HookInfo g_hookExecute;
static HookInfo g_hookGetTarget;

// ============================================================
// PlayAnimation::Serialize (0x140042480)
// Serializes: status(+0x18), entity(+0x20) ComponentEntity, int(+0x28), Time(+0x30), Time(+0x38)
// ============================================================
static void Reimpl_Serialize(void* self, void* stream) {
    uint8_t* s = (uint8_t*)self;
    uint8_t ser1[16];
    uint8_t ser2[24];

    fn_Serializer_Ctor(ser1, stream, 0);
    fn_Serialize_Bool(ser1, s + 0x18, nullptr);
    fn_Serializer_Dtor(ser1);

    fn_Serializer_Ctor(ser2, stream, 0);
    fn_IBP_Serialize(s + 0x20, ser2);
    if (*(void**)(s + 0x20) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x20), *g_TypeInfo_ComponentEntity)) {
            *(void**)(s + 0x20) = nullptr;
        }
    }
    fn_Serialize_Int(ser2, s + 0x28, nullptr);
    fn_Time_Serialize(s + 0x30, ser2);
    fn_Time_Serialize(s + 0x38, ser2);
    fn_Serializer_Dtor(ser2);
}

// ============================================================
// PlayAnimation::_purecall (0x140042420)
// Finds family component, calls animation setup.
// Returns 1 (action stays active, Execute checks completion).
// ============================================================
static uint64_t Reimpl_Purecall(void* self) {
    uint8_t* s = (uint8_t*)self;
    void* localIBP;

    fn_CheckPtr(s + 0x20);
    fn_FindFamily(*(void**)(s + 0x20), &localIBP);
    fn_CheckPtr(&localIBP);
    fn_SetAnimation(localIBP, *(uint32_t*)(s + 0x28), *g_AnimFloat, *g_AnimFloat);
    return 1;
}

// ============================================================
// PlayAnimation::Execute (0x140042400)
// 21 bytes. Accumulates elapsed time, returns true while still animating.
//   self->_field38 += *time;
//   return (self->_field38 < self->_field30);
// ============================================================
static bool Reimpl_Execute(void* self, void* time_ptr) {
    uint8_t* s = (uint8_t*)self;
    int64_t elapsed = *(int64_t*)time_ptr;
    *(int64_t*)(s + 0x38) += elapsed;
    return *(int64_t*)(s + 0x38) < *(int64_t*)(s + 0x30);
}

// CheckEntity at 0x1400423f0 is 3 bytes (ret 0), too small to hook.

// ============================================================
// PlayAnimation::GetTarget (0x1400423b0)
// Returns true if param matches entity at +0x20.
// ============================================================
static bool Reimpl_GetTarget(void* self, void* param) {
    uint8_t* s = (uint8_t*)self;
    fn_CheckPtr(param);
    return *(void**)(s + 0x20) == *(void**)param;
}

// ============================================================
// Install / Remove
// ============================================================

static void ResolveHelpers() {
    // CheckPtr is called indirectly via IAT at [0x140152ab0]
    uintptr_t iatEntry = ResolveAddr(Module::Exe, 0x140152ab0);
    if (iatEntry) fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(iatEntry);
    g_TypeInfo_ComponentEntity = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c4218);
    g_AnimFloat = (float*)ResolveAddr(Module::Exe, 0x14017214c);
    fn_FindFamily = (fn_FindFamily_t)ResolveAddr(Module::Exe, 0x1400012d0); // Entity_FindComponent_Family
    fn_SetAnimation = (fn_SetAnimation_t)ResolveAddr(Module::Exe, 0x14010a1f0);
    fn_Time_Serialize = (fn_Time_Serialize_t)ResolveExport("?Serialize@Time@System@@QEAAXAEAVSerializer@IO@@@Z");
    fn_Serializer_Ctor = (fn_Serializer_Ctor_t)ResolveExport("?Serializer@IO@@QEAA@PEAVStream@1@H@Z");
    fn_Serializer_Dtor = (fn_Serializer_Dtor_t)ResolveExport("??1Serializer@IO@@QEAA@XZ");
    fn_Serialize_Bool = (fn_Serialize_Bool_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEA_NPEBD@Z");
    fn_Serialize_Int = (fn_Serialize_Int_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAint@PEBD@Z");
    fn_IBP_Serialize = (fn_IBP_Serialize_t)ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z");
}

void InstallPlayAnimationSerialize() {
    ResolveHelpers();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140042480);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 14);
#ifdef _DEBUG
    printf("[PlayAnimation] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemovePlayAnimationSerialize() { RemoveHook(&g_hookSerialize); }

void InstallPlayAnimationPurecall() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140042420);
    InstallHook(target, (void*)Reimpl_Purecall, &g_hookPurecall, 13);
#ifdef _DEBUG
    printf("[PlayAnimation] Purecall hooked at %p\n", (void*)target);
#endif
}
void RemovePlayAnimationPurecall() { RemoveHook(&g_hookPurecall); }

void InstallPlayAnimationExecute() {
    // 0x140042400 is 21 bytes total. First instruction boundary >= 12 is at offset 13:
    //   49 8B 00 (3) + 48 01 41 38 (4) + 48 8B 51 38 (4) + 33 C0 (2) = 13
    uintptr_t target = ResolveAddr(Module::Exe, 0x140042400);
    InstallHook(target, (void*)Reimpl_Execute, &g_hookExecute, 13);
#ifdef _DEBUG
    printf("[PlayAnimation] Execute hooked at %p\n", (void*)target);
#endif
}
void RemovePlayAnimationExecute() { RemoveHook(&g_hookExecute); }

void InstallPlayAnimationGetTarget() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400423b0);
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 13);
#ifdef _DEBUG
    printf("[PlayAnimation] GetTarget hooked at %p\n", (void*)target);
#endif
}
void RemovePlayAnimationGetTarget() { RemoveHook(&g_hookGetTarget); }
