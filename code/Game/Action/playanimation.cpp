// code/Game/Action/playanimation.cpp
// Clean C++ reimplementation of PlayAnimation action methods.
// Source: Game/Action/PlayAnimation.cpp (from profile build debug strings)
//
// PlayAnimation -- 0x40 bytes. Plays an animation on a citizen entity.
// Inherits: Object::Internal -> Action (0x20) -> PlayAnimation
// Vtable at 0x140158950 (10 slots).
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- it is portable for standalone exe.

#include "PlayAnimation.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h -- PlayAnimation-specific helpers)
// ============================================================

// Object::InternalBasePtr::CheckPtr -- via IAT at 0x140152ab0
static void (*s_fn_CheckPtr)(void* ibp) = nullptr;

// Object::InternalBasePtr::Serialize -- via export
static void (*s_fn_IBP_Serialize)(void* ibp, void* serializer) = nullptr;

// System::Time::Serialize(this, Serializer&)
static void (*s_fn_Time_Serialize)(void* time, void* serializer) = nullptr;

// Entity_FindComponent_Family (0x1400012d0)
static void (*s_fn_FindFamily)(void* entity, void* outIBP) = nullptr;

// FUN_14010a1f0 -- set animation on family component
static void (*s_fn_SetAnimation)(void* familyComp, uint32_t animId, float p1, float p2) = nullptr;

// DAT_14017214c -- float constant used in animation setup
static float* s_AnimFloat = nullptr;

// DAT_1401c4218 -- TypeInfo for ComponentEntity type check
static uint32_t* s_TypeInfo_ComponentEntity = nullptr;

static bool s_playanimation_funcs_resolved = false;

// ============================================================
// Resolution -- called once from Install
// ============================================================
bool PlayAnimation_ResolveFuncs() {
    if (s_playanimation_funcs_resolved) return true;

    // CheckPtr via IAT at [0x140152ab0]
    uintptr_t iatEntry = ResolveAddr(Module::Exe, 0x140152ab0);
    if (iatEntry) s_fn_CheckPtr = *reinterpret_cast<void(**)(void*)>(iatEntry);

    s_TypeInfo_ComponentEntity = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c4218);
    s_AnimFloat = (float*)ResolveAddr(Module::Exe, 0x14017214c);
    s_fn_FindFamily = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x1400012d0));
    s_fn_SetAnimation = reinterpret_cast<void(*)(void*, uint32_t, float, float)>(
        ResolveAddr(Module::Exe, 0x14010a1f0));
    s_fn_Time_Serialize = reinterpret_cast<void(*)(void*, void*)>(
        ResolveExport("?Serialize@Time@System@@QEAAXAEAVSerializer@IO@@@Z"));
    s_fn_IBP_Serialize = reinterpret_cast<void(*)(void*, void*)>(
        ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z"));

    bool ok = s_fn_CheckPtr && s_TypeInfo_ComponentEntity && s_AnimFloat &&
              s_fn_FindFamily && s_fn_SetAnimation && s_fn_Time_Serialize &&
              s_fn_IBP_Serialize &&
              g_fn_Serializer_Ctor && g_fn_Serializer_Dtor &&
              g_fn_Serialize_Bool && g_fn_Serialize_Int &&
              fn_IsDerivedFrom;

#ifdef _DEBUG
    printf("[PlayAnimation] ResolveFuncs: CheckPtr=%p FindFamily=%p SetAnim=%p AnimFloat=%p\n",
           (void*)s_fn_CheckPtr, (void*)s_fn_FindFamily,
           (void*)s_fn_SetAnimation, (void*)s_AnimFloat);
    printf("[PlayAnimation]   TimeSer=%p IBPSer=%p TypeInfo=%p\n",
           (void*)s_fn_Time_Serialize, (void*)s_fn_IBP_Serialize,
           (void*)s_TypeInfo_ComponentEntity);
    if (!ok) printf("[PlayAnimation] ERROR: some function pointers not resolved!\n");
#endif

    s_playanimation_funcs_resolved = ok;
    return ok;
}

// ============================================================
// PlayAnimation::Serialize (0x140042480)
// Serializes: status(+0x18), entity(+0x20) ComponentEntity, int(+0x28),
//   Time(+0x30), Time(+0x38)
// ============================================================
void PlayAnimation_Serialize_Impl(void* self, void* stream) {
    PlayAnimation_ResolveFuncs();   // lazy-resolve: action may be built inline by ba6b0, not our ctor
    uint8_t* s = (uint8_t*)self;
    uint8_t ser1[24];
    uint8_t ser2[24];

    g_fn_Serializer_Ctor(ser1, stream, 0);
    g_fn_Serialize_Bool(ser1, s + 0x18, nullptr);
    g_fn_Serializer_Dtor(ser1);

    g_fn_Serializer_Ctor(ser2, stream, 0);
    s_fn_IBP_Serialize(s + 0x20, ser2);
    if (*(void**)(s + 0x20) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x20), *s_TypeInfo_ComponentEntity)) {
            *(void**)(s + 0x20) = nullptr;
        }
    }
    g_fn_Serialize_Int(ser2, s + 0x28, nullptr);
    s_fn_Time_Serialize(s + 0x30, ser2);
    s_fn_Time_Serialize(s + 0x38, ser2);
    g_fn_Serializer_Dtor(ser2);
}

// ============================================================
// PlayAnimation::AbstractImpl / _purecall (0x140042420)
// Finds family component, calls animation setup.
// Returns 1 (action stays active, Execute checks completion).
// ============================================================
int PlayAnimation_AbstractImpl_Impl(void* self, void* actionStack) {
    PlayAnimation_ResolveFuncs();
    (void)actionStack;
    uint8_t* s = (uint8_t*)self;
    void* localIBP;

    s_fn_CheckPtr(s + 0x20);
    if (*(void**)(s + 0x20) == nullptr) {
        return 0;
    }
    s_fn_FindFamily(*(void**)(s + 0x20), &localIBP);
    s_fn_CheckPtr(&localIBP);
    if (localIBP == nullptr) {
        return 0;
    }
    s_fn_SetAnimation(localIBP, *(uint32_t*)(s + 0x28), *s_AnimFloat, *s_AnimFloat);
    return 1;
}

// ============================================================
// PlayAnimation::Execute (0x140042400)
// Accumulates elapsed time, returns true while still animating.
//   self->_field38 += *time;
//   return (self->_field38 < self->_field30);
// ============================================================
int PlayAnimation_Execute_Impl(void* self, void* actionStack, void* time_ptr) {
    PlayAnimation_ResolveFuncs();
    (void)actionStack;
    uint8_t* s = (uint8_t*)self;
    int64_t elapsed = *(int64_t*)time_ptr;
    *(int64_t*)(s + 0x38) += elapsed;
    return (*(int64_t*)(s + 0x38) < *(int64_t*)(s + 0x30)) ? 1 : 0;
}

// ============================================================
// PlayAnimation::CheckEntity (0x1400423f0)
// 3 bytes -- trivial return 0. Too small to hook.
// Implemented for standalone exe completeness.
// ============================================================
int PlayAnimation_CheckEntity_Impl(void* /*self*/) {
    return 0;
}

// ============================================================
// PlayAnimation::GetTarget (0x1400423b0)
// Returns true if param matches entity at +0x20.
// ============================================================
bool PlayAnimation_GetTarget_Impl(void* self, void* param) {
    PlayAnimation_ResolveFuncs();
    uint8_t* s = (uint8_t*)self;
    s_fn_CheckPtr(param);
    return *(void**)(s + 0x20) == *(void**)param;
}

// ============================================================
// Standalone factory + vtable (keystone for the Action family).
// See action_base_standalone.{h,cpp} for the shared slot helpers.
// Vtable @ 0x140158950 (10 slots), size 0x40, parent "Action".
// ============================================================
#ifdef STANDALONE_EXE
#include "action_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static constexpr uintptr_t kPlayAnimation_VtableAddr = 0x140158950;
static constexpr int       kPlayAnimation_Size       = 0x40;

// slot2 (+0x10) GetTypeId — exe returns TypeInfo_PlayAnimation.
static uint32_t PlayAnimation_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("PlayAnimation");
    return cached;
}

static void* g_PlayAnimation_Vtable[10] = {
    reinterpret_cast<void*>(&Action_ScalarDtor),               // +0x00 ScalarDtor
    reinterpret_cast<void*>(&Action_Slot1),                    // +0x08 slot1
    reinterpret_cast<void*>(&PlayAnimation_GetTypeId),         // +0x10 GetTypeId
    reinterpret_cast<void*>(&PlayAnimation_Serialize_Impl),    // +0x18 Serialize
    reinterpret_cast<void*>(&PlayAnimation_AbstractImpl_Impl), // +0x20 AbstractImpl
    reinterpret_cast<void*>(&PlayAnimation_Execute_Impl),      // +0x28 Execute
    reinterpret_cast<void*>(&Action_Enter),                    // +0x30 Enter
    reinterpret_cast<void*>(&Action_Exit),                     // +0x38 Exit
    reinterpret_cast<void*>(&PlayAnimation_CheckEntity_Impl),  // +0x40 CheckEntity
    reinterpret_cast<void*>(&PlayAnimation_GetTarget_Impl),    // +0x48 GetTarget
};

extern "C" void RegisterPlayAnimationVtable() {
    RegisterExeAddr(kPlayAnimation_VtableAddr,
                    reinterpret_cast<uintptr_t>(g_PlayAnimation_Vtable));
}

extern "C" void* PlayAnimation_CreateInPlace(void* mem) {
    PlayAnimation_ResolveFuncs();
    return Action_InitInPlace(mem, kPlayAnimation_Size, g_PlayAnimation_Vtable);
}

extern "C" void* PlayAnimation_Factory(void) {
    PlayAnimation_ResolveFuncs();
    return Action_Alloc(kPlayAnimation_Size, g_PlayAnimation_Vtable);
}

REGISTER_TYPE(PlayAnimation, "Action", &PlayAnimation_Factory, &PlayAnimation_CreateInPlace);

#endif  // STANDALONE_EXE
