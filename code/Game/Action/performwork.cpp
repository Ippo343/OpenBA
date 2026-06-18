// code/Game/Action/performwork.cpp
// Clean C++ reimplementation of PerformWork action methods.
// Source: Game/Action/PerformWork.cpp (from profile build debug strings)
//
// PerformWork -- 0x48 bytes. Generic work animation action used by every
// citizen performing work. Handles tool durability, facing direction via yaw,
// elapsed vs required time tracking.
// Inherits: Object::Internal -> Action (0x20) -> PerformWork
// Vtable at 0x14015fea0 (10 slots).
//
// Vtable (10 slots):
//   [0] +0x00 ScalarDtor    = 0x140022BD0
//   [1] +0x08 VectorDtor    = 0x140022B80 (returns DAT_1401c5fe0, unhookable)
//   [2] +0x10 BaseDtor      = 0x140022B70 (returns TypeInfo, unhookable)
//   [3] +0x18 Serialize     = 0x1400C9550
//   [4] +0x20 AbstractImpl  = 0x1400C9420 (animation + tool logic)
//   [5] +0x28 Execute       = 0x1400C93C0 (time accumulation + completion check)
//   [6] +0x30 Enter         = 0x1400C4B90 (base Action, void nop)
//   [7] +0x38 Exit          = 0x1400C9370 (reset animation)
//   [8] +0x40 CheckEntity   = 0x1400C9320 (reset animation, same as Exit)
//   [9] +0x48 GetTarget     = 0x1400C92D0 (compare entity parent)
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- it is portable for standalone exe.

#include "PerformWork.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h -- PerformWork-specific helpers)
// ============================================================

// Object::InternalBasePtr::CheckPtr -- via IAT at 0x140152ab0
static void (*s_fn_CheckPtr)(void* ibp) = nullptr;

// Object::InternalBasePtr::Serialize -- via IAT at 0x140152aa0
static void (*s_fn_IBP_Serialize)(void* ibp, void* serializer) = nullptr;

// Object::InternalBasePtr::DeletePtr -- via export
static void (*s_fn_IBP_DeletePtr)(void* ibp) = nullptr;

// Object::Internal::~Internal -- via IAT at 0x140152770
static void (*s_fn_ObjectInternal_Dtor)(void* self) = nullptr;

// System::Time::Serialize(this, Serializer&) -- via export
static void (*s_fn_Time_Serialize)(void* time, void* serializer) = nullptr;

// IO::Serializer::Serialize(float&, const char*) -- via IAT at 0x140152a60
static void (*s_fn_Serialize_Float)(void* serializer, void* field, const char* name) = nullptr;

// Entity_FindComponent_Family (0x1400012d0)
static void* (*s_fn_FindFamily)(void* entity, void* outIBP) = nullptr;

// AnimalComponent_impl_a1f0 -- SetAnimation (0x14010a1f0)
// Params: (familyComponent, animId, param3, param4)
static void (*s_fn_SetAnimation)(void* familyComp, uint32_t animId, float p3, float p4) = nullptr;

// PerformWork_helper_a060 -- SetFacingDirection (0x14010a060)
// Params: (familyComponent, vector3)
static void (*s_fn_SetFacingDirection)(void* familyComp, void* vector3) = nullptr;

// Math::Vector3::FromYawPitch -- via dispatch slot at 0x1401512a0
// Params: (retbuf, yaw_ptr, pitch_ptr) -- returns retbuf
typedef void* (*fn_FromYawPitch_t)(void* retbuf, float* yaw, float* pitch);
static fn_FromYawPitch_t s_fn_FromYawPitch = nullptr;

// PerformWork_helper_6650 -- citizen break check (0x140116650)
// Params: (entity) -- returns bool (true = needs break)
typedef uint8_t (*fn_CitizenBreakCheck_t)(void* entity);
static fn_CitizenBreakCheck_t s_fn_CitizenBreakCheck = nullptr;

// DAT_14017214c -- float constant 1.0f
static float* s_fConst1 = nullptr;

// DAT_1401722a0 -- float constant -180.0f (yaw lower bound)
static float* s_fYawMin = nullptr;

// DAT_140172220 -- float constant 180.0f (yaw upper bound)
static float* s_fYawMax = nullptr;

// TypeInfo for IsDerivedFrom check in Serialize -- DAT_1401c7158 (CitizenAIComponent_FC)
// Already in engine_funcs.h as g_TypeId_CitizenAIComponent_FC

// Vtable addresses for dtor chain
static uintptr_t s_vtable_PerformWork = 0;
static uintptr_t s_vtable_Action = 0;

// TypeInfo for BaseDtor (slot 2) -- DAT at TypeInfo_PerformWork
static uint32_t* s_typeInfo_PerformWork = nullptr;

static bool s_funcs_resolved = false;

// ============================================================
// Resolution -- called once from Install
// ============================================================
bool PerformWork_ResolveFuncs() {
    if (s_funcs_resolved) return true;

    // CheckPtr via IAT at [0x140152ab0]
    uintptr_t iatCheckPtr = ResolveAddr(Module::Exe, 0x140152ab0);
    if (iatCheckPtr) s_fn_CheckPtr = *reinterpret_cast<void(**)(void*)>(iatCheckPtr);

    // IBP::Serialize via IAT at [0x140152aa0]
    uintptr_t iatIBPSer = ResolveAddr(Module::Exe, 0x140152aa0);
    if (iatIBPSer) s_fn_IBP_Serialize = *reinterpret_cast<void(**)(void*, void*)>(iatIBPSer);

    // IBP::DeletePtr
    s_fn_IBP_DeletePtr = (void(*)(void*))fn_InternalBasePtr_DeletePtr;

    // Object::Internal::~Internal via IAT at [0x140152770]
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152770);
    if (iatDtor) s_fn_ObjectInternal_Dtor = *reinterpret_cast<void(**)(void*)>(iatDtor);

    // Time::Serialize via export
    s_fn_Time_Serialize = reinterpret_cast<void(*)(void*, void*)>(
        ResolveExport("?Serialize@Time@System@@QEAAXAEAVSerializer@IO@@@Z"));

    // Serialize_Float via IAT at [0x140152a60]
    uintptr_t iatSerFloat = ResolveAddr(Module::Exe, 0x140152a60);
    if (iatSerFloat) s_fn_Serialize_Float = *reinterpret_cast<void(**)(void*, void*, const char*)>(iatSerFloat);

    // FindFamily (0x1400012d0)
    s_fn_FindFamily = reinterpret_cast<void*(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x1400012d0));

    // SetAnimation (0x14010a1f0)
    s_fn_SetAnimation = reinterpret_cast<void(*)(void*, uint32_t, float, float)>(
        ResolveAddr(Module::Exe, 0x14010a1f0));

    // SetFacingDirection (0x14010a060)
    s_fn_SetFacingDirection = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x14010a060));

    // Math::Vector3::FromYawPitch via dispatch slot at [0x1401512a0]
    uintptr_t iatFromYaw = ResolveAddr(Module::Exe, 0x1401512a0);
    if (iatFromYaw) s_fn_FromYawPitch = *reinterpret_cast<fn_FromYawPitch_t*>(iatFromYaw);

    // Citizen break check (0x140116650)
    s_fn_CitizenBreakCheck = reinterpret_cast<fn_CitizenBreakCheck_t>(
        ResolveAddr(Module::Exe, 0x140116650));

    // Float constants
    s_fConst1 = (float*)ResolveAddr(Module::Exe, 0x14017214c);
    s_fYawMin = (float*)ResolveAddr(Module::Exe, 0x1401722a0);
    s_fYawMax = (float*)ResolveAddr(Module::Exe, 0x140172220);

    // Vtable addresses
    s_vtable_PerformWork = ResolveAddr(Module::Exe, 0x14015fea0);
    s_vtable_Action = ResolveAddr(Module::Exe, 0x14015faa0);

    // TypeInfo for BaseDtor -- read from PerformWork slot 2 return value address
    // Slot 2 at 0x140022B70 returns TypeInfo_PerformWork which is DAT at that location
    // Actually the decompilation shows it returns TypeInfo_PerformWork directly
    // We can get it from the vtable slot 1 return: DAT_1401c5fe0
    s_typeInfo_PerformWork = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5fe0));

    bool ok = s_fn_CheckPtr && s_fn_IBP_Serialize && s_fn_IBP_DeletePtr &&
              s_fn_ObjectInternal_Dtor && s_fn_Time_Serialize &&
              s_fn_Serialize_Float && s_fn_FindFamily &&
              s_fn_SetAnimation && s_fn_SetFacingDirection &&
              s_fn_FromYawPitch && s_fn_CitizenBreakCheck &&
              s_fConst1 && s_fYawMin && s_fYawMax &&
              s_vtable_PerformWork && s_vtable_Action &&
              g_TypeId_CitizenAIComponent_FC &&
              g_fn_Serializer_Ctor && g_fn_Serializer_Dtor &&
              g_fn_Serialize_Bool && g_fn_Serialize_Int &&
              fn_IsDerivedFrom && fn_MemFreeStatic;

#ifdef _DEBUG
    printf("[PerformWork] ResolveFuncs: CheckPtr=%p IBPSer=%p SetAnim=%p\n",
           (void*)s_fn_CheckPtr, (void*)s_fn_IBP_Serialize,
           (void*)s_fn_SetAnimation);
    printf("[PerformWork]   SetDir=%p FromYaw=%p BreakChk=%p\n",
           (void*)s_fn_SetFacingDirection, (void*)s_fn_FromYawPitch,
           (void*)s_fn_CitizenBreakCheck);
    printf("[PerformWork]   SerFloat=%p FindFamily=%p TimeSer=%p\n",
           (void*)s_fn_Serialize_Float, (void*)s_fn_FindFamily,
           (void*)s_fn_Time_Serialize);
    if (!ok) printf("[PerformWork] ERROR: some function pointers not resolved!\n");
#endif

    s_funcs_resolved = ok;
    return ok;
}

// ============================================================
// PerformWork::ScalarDtor (0x140022BD0)
// Set vtables, cleanup entity ref, call ~Internal, optionally free.
// ============================================================
void* PerformWork_ScalarDtor_Impl(PerformWork* self, uint32_t flags) {
    uint8_t* s = (uint8_t*)self;

    // Set PerformWork vtable (for correct virtual dispatch during destruction)
    *(uintptr_t*)s = s_vtable_PerformWork;
    // Set Action vtable (base class destructor phase)
    *(uintptr_t*)s = s_vtable_Action;

    // If entity ref at +0x10 is non-null, delete it
    if (*(void**)(s + 0x10) != nullptr) {
        s_fn_IBP_DeletePtr(s + 0x10);
    }

    // Call Object::Internal::~Internal
    s_fn_ObjectInternal_Dtor(self);

    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// PerformWork::BaseDtor (0x140022B70) -- slot 2
// Returns TypeInfo for PerformWork (tiny, unhookable).
// ============================================================
uint32_t PerformWork_BaseDtor_Impl() {
    return *s_typeInfo_PerformWork;
}

// ============================================================
// PerformWork::Serialize (0x1400C9550)
// Serializes: status(+0x18), entity(+0x20) with CitizenAI type check,
//   float(+0x28), int(+0x2C), Time(+0x30), Time(+0x38), bool(+0x40), int(+0x44)
// ============================================================
void PerformWork_Serialize_Impl(PerformWork* self, void* stream) {
    uint8_t* s = (uint8_t*)self;
    uint8_t ser1[24];
    uint8_t ser2[24];

    // First serializer: serialize status byte (+0x18)
    g_fn_Serializer_Ctor(ser1, stream, 0);
    g_fn_Serialize_Bool(ser1, s + 0x18, nullptr);
    g_fn_Serializer_Dtor(ser1);

    // Second serializer: serialize all PerformWork-specific fields
    g_fn_Serializer_Ctor(ser2, stream, 0);

    // Serialize work target entity ref (+0x20)
    s_fn_IBP_Serialize(s + 0x20, ser2);
    if (*(void**)(s + 0x20) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x20), *g_TypeId_CitizenAIComponent_FC)) {
            *(void**)(s + 0x20) = nullptr;
        }
    }

    // Serialize facing yaw (+0x28) as float
    s_fn_Serialize_Float(ser2, s + 0x28, nullptr);

    // Serialize animation ID (+0x2C) as int
    g_fn_Serialize_Int(ser2, s + 0x2C, nullptr);

    // Serialize required time (+0x30) and elapsed time (+0x38) as Time
    s_fn_Time_Serialize(s + 0x30, ser2);
    s_fn_Time_Serialize(s + 0x38, ser2);

    // Serialize use tool flag (+0x40) as bool
    g_fn_Serialize_Bool(ser2, s + 0x40, nullptr);

    // Serialize field44 (+0x44) as int
    g_fn_Serialize_Int(ser2, s + 0x44, nullptr);

    g_fn_Serializer_Dtor(ser2);
}

// ============================================================
// PerformWork::AbstractImpl (0x1400C9420) -- slot 4
// Sets animation on entity's family component, sets facing direction
// if yaw is in valid range, and optionally decrements tool durability.
// Always returns 1 (action stays active, Execute checks completion).
// ============================================================
int PerformWork_AbstractImpl_Impl(PerformWork* self, void* actionStack) {
    (void)actionStack;
    uint8_t* s = (uint8_t*)self;

    // CheckPtr on work target entity at +0x20
    s_fn_CheckPtr(s + 0x20);
    void* entity = *(void**)(s + 0x20);
    uint32_t animId = *(uint32_t*)(s + 0x2C);

    // Get family component from entity's +0x278 field and set animation
    uint8_t* ent = (uint8_t*)entity;
    s_fn_CheckPtr(ent + 0x278);
    void* familyComp = *(void**)(ent + 0x278);
    s_fn_SetAnimation(familyComp, animId, *s_fConst1, 0.0f);

    // If yaw is in valid range [-180, 180], set facing direction
    float yaw = *(float*)(s + 0x28);
    if (yaw >= *s_fYawMin && yaw <= *s_fYawMax) {
        float yawVal = yaw;
        float pitch = 0.0f;

        // Get the entity's parent for direction setting
        s_fn_CheckPtr(s + 0x20);
        void* entity2 = *(void**)(s + 0x20);
        uintptr_t parentEntity = *(uintptr_t*)((uint8_t*)entity2 + 0x10);
        uintptr_t parentIBP = parentEntity;
        s_fn_CheckPtr(&parentIBP);

        // Find family component on the parent
        void* familyIBP = nullptr;
        s_fn_FindFamily((void*)parentIBP, &familyIBP);
        s_fn_CheckPtr(&familyIBP);
        void* familyPtr = *(void**)&familyIBP;

        // Convert yaw to direction vector and set it
        uint8_t vector3Buf[24];  // Vector3 + Scalar temp
        void* dirVec = s_fn_FromYawPitch(vector3Buf, &yawVal, &pitch);
        s_fn_SetFacingDirection(familyPtr, dirVec);
    }

    // If use tool flag is set, decrement tool durability
    if (*(uint8_t*)(s + 0x40) != 0) {
        s_fn_CheckPtr(s + 0x20);
        void* ent3 = *(void**)(s + 0x20);
        if (*(int32_t*)((uint8_t*)ent3 + 0xAC) > 0) {
            s_fn_CheckPtr(s + 0x20);
            void* ent4 = *(void**)(s + 0x20);
            int32_t* toolDurability = (int32_t*)((uint8_t*)ent4 + 0xAC);
            (*toolDurability)--;
            if (*toolDurability < 0) {
                *toolDurability = 0;
            }
        }
    }

    return 1;
}

// ============================================================
// PerformWork::Execute (0x1400C93C0) -- slot 5
// Accumulates elapsed time (+0x38) from delta time.
// When elapsed >= required, checks if citizen needs a break.
// Returns: 0 = done, 1 = still working, 2 = needs break
// ============================================================
int PerformWork_Execute_Impl(PerformWork* self, void* actionStack, void* deltaTime) {
    (void)actionStack;
    uint8_t* s = (uint8_t*)self;

    int64_t elapsed = *(int64_t*)(s + 0x38);
    int64_t required = *(int64_t*)(s + 0x30);

    if (elapsed >= required) {
        // Work time complete -- check if citizen needs a break
        s_fn_CheckPtr(s + 0x20);
        void* entity = *(void**)(s + 0x20);
        uint8_t needsBreak = s_fn_CitizenBreakCheck(entity);
        // If needs break, return 2; otherwise return 0 (done)
        return needsBreak ? 2 : 0;
    }

    // Still working -- accumulate delta time
    int64_t dt = *(int64_t*)deltaTime;
    *(int64_t*)(s + 0x38) = elapsed + dt;
    return 1;
}

// ============================================================
// PerformWork::Exit (0x1400C9370) -- slot 7
// Resets character animation to idle (animation 0, rate 1.0, 1.0).
// ============================================================
void PerformWork_Exit_Impl(PerformWork* self, void* actionStack) {
    (void)actionStack;
    uint8_t* s = (uint8_t*)self;

    // Get work target entity
    s_fn_CheckPtr(s + 0x20);
    void* entity = *(void**)(s + 0x20);

    // Get family component at entity+0x278, reset animation to idle
    uint8_t* ent = (uint8_t*)entity;
    s_fn_CheckPtr(ent + 0x278);
    void* familyComp = *(void**)(ent + 0x278);
    s_fn_SetAnimation(familyComp, 0, *s_fConst1, *s_fConst1);
}

// ============================================================
// PerformWork::CheckEntity (0x1400C9320) -- slot 8
// Identical to Exit: resets character animation to idle.
// ============================================================
void PerformWork_CheckEntity_Impl(PerformWork* self) {
    uint8_t* s = (uint8_t*)self;

    // Get work target entity
    s_fn_CheckPtr(s + 0x20);
    void* entity = *(void**)(s + 0x20);

    // Get family component at entity+0x278, reset animation to idle
    uint8_t* ent = (uint8_t*)entity;
    s_fn_CheckPtr(ent + 0x278);
    void* familyComp = *(void**)(ent + 0x278);
    s_fn_SetAnimation(familyComp, 0, *s_fConst1, *s_fConst1);
}

// ============================================================
// PerformWork::GetTarget (0x1400C92D0) -- slot 9
// Returns true if param matches the work target entity's parent (+0x10).
// ============================================================
bool PerformWork_GetTarget_Impl(PerformWork* self, void* param) {
    uint8_t* s = (uint8_t*)self;

    // CheckPtr on work target
    s_fn_CheckPtr(s + 0x20);
    void* entity = *(void**)(s + 0x20);
    // Get parent entity at entity+0x10
    uintptr_t parentEntity = *(uintptr_t*)((uint8_t*)entity + 0x10);

    // CheckPtr on param
    s_fn_CheckPtr(param);
    return parentEntity == *(uintptr_t*)param;
}

// ============================================================
// Standalone factory + vtable — Action family (notes/FACTORY_CASCADE_145_BRIEF.md).
// Vtable @ 0x14015fea0 (10 slots), size 0x48, parent "Action".
// Shared slot helpers live in action_base_standalone.{h,cpp}.
// ============================================================
#ifdef STANDALONE_EXE
#include "action_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t PerformWork_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("PerformWork");
    return c;
}

static void* g_PerformWork_Vtable[10] = {
    (void*)&Action_ScalarDtor,        // +0x00 ScalarDtor
    (void*)&Action_Slot1,             // +0x08 slot1 (returns null)
    (void*)&PerformWork_GetTypeId,           // +0x10 GetTypeId
    (void*)&PerformWork_Serialize_Impl,      // +0x18 Serialize
    (void*)&PerformWork_AbstractImpl_Impl,   // +0x20 AbstractImpl
    (void*)&PerformWork_Execute_Impl,   // +0x28 Execute
    (void*)&Action_Enter,  // +0x30 Enter
    (void*)&PerformWork_Exit_Impl,   // +0x38 Exit
    (void*)&PerformWork_CheckEntity_Impl,  // +0x40 CheckEntity
    (void*)&PerformWork_GetTarget_Impl, // +0x48 GetTarget
};

extern "C" void RegisterPerformWorkVtable() {
    RegisterExeAddr(0x14015fea0, (uintptr_t)g_PerformWork_Vtable);
}
extern "C" void* PerformWork_CreateInPlace(void* mem) {
    PerformWork_ResolveFuncs();
    return Action_InitInPlace(mem, 0x48, g_PerformWork_Vtable);
}
extern "C" void* PerformWork_Factory(void) {
    PerformWork_ResolveFuncs();
    return Action_Alloc(0x48, g_PerformWork_Vtable);
}
REGISTER_TYPE(PerformWork, "Action", &PerformWork_Factory, &PerformWork_CreateInPlace);
#endif // STANDALONE_EXE
