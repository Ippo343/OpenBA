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

#include "PerformWork.h"
#include "object/TypeInfo.h"
#include "Action/exe_bodies.h"   // ActMv_* helpers
#include "Game/exe_constants.h"  // kRdata_*
#include "math/Math.h"           // Math::Vector3::FromYawPitch, Math::Scalar
#include <cstring>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdio>
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include "Component/CitizenComponent.h"   // ENDGAME B.3.8 CB3: CitizenComponent::TypeId()

// ============================================================
// Helpers, called by name (B3R)
//   0x1400012d0 ActMv_FindComponentFamily_12d0 (Entity_FindComponent_Family)
//   0x14010a1f0 ActMv_SetAnimation_a1f0 (AnimalComponent_impl_a1f0) -- SetAnimation
//               (familyComponent, animId, param3, param4)
//   0x14010a060 ActMv_SetFacingDirection_a060 (PerformWork_helper_a060) -- SetFacingDirection
//               (familyComponent, vector3)
//   import slot 0x1401512a0 ?FromYawPitch@Vector3@Math@@ -> Math::Vector3::FromYawPitch
//   0x140116650 ActMv_CitizenBreakCheck_116650 (PerformWork_helper_6650) -- citizen break
//               check (entity) -- returns bool (true = needs break)
//
// Float constants (Game/exe_constants.h): 0x14017214c = 1.0f, 0x1401722a0 = -180.0f
// (yaw lower bound), 0x140172220 = 180.0f (yaw upper bound).
//
// TypeInfo for IsDerivedFrom check in Serialize -- cell 0x1401c7158, read directly
// through CitizenComponent::TypeId() (D5, ENDGAME B.3.9 CG3). The env-gated
// TypeId_CitizenComponent_FC / by-name fallback pair this used to juggle read the
// SAME cell and always agreed (verified in the 2026-08-26 graduation A/B), so there
// is nothing left to fall back to.
//
// ENDGAME B.3.9 CB: retail reads this record field ONLY in the class's own TypeName
// slot (0x1401c5fe0 = the NAME field (+8) of PerformWork's record, read by
// PerformWork_Slot1 0x140022b80); DECLARE_EXE_TYPE answers that slot, so the tree's copy of it is gone.
// ============================================================

namespace {
// The .rdata float constants (Game/exe_constants.h) that are stored as bit patterns.
inline float RdataF(uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof f); return f; }
}  // namespace

// ============================================================
// PerformWork::Serialize (0x1400C9550)
// Serializes: status(+0x18), entity(+0x20) with CitizenAI type check,
//   float(+0x28), int(+0x2C), Time(+0x30), Time(+0x38), bool(+0x40), int(+0x44)
// ============================================================
void PerformWork::Persist(void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    alignas(8) uint8_t ser1[24];
    alignas(8) uint8_t ser2[24];

    // First serializer: serialize status byte (+0x18)
    ::new (static_cast<void*>(ser1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(_status, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();   // B.3.4 bridge

    // Second serializer: serialize all PerformWork-specific fields
    ::new (static_cast<void*>(ser2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize work target entity ref (+0x20)
    reinterpret_cast<Object::InternalBasePtr*>(s + 0x20)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (workTarget != nullptr) {
        if (!::Object::IsDerivedFromNullSafe(workTarget, CitizenComponent::TypeId())) {
            workTarget = nullptr;
        }
    }

    // Serialize facing yaw (+0x28) as float
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(facingYaw, nullptr);   // B.3.4 bridge

    // Serialize animation ID (+0x2C) as int
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(s + 0x2C), nullptr);   // B.3.4 bridge

    // Serialize required time (+0x30) and elapsed time (+0x38) as Time
    reinterpret_cast<System::Time*>(s + 0x30)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4b bridge
    reinterpret_cast<System::Time*>(s + 0x38)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4b bridge

    // Serialize use tool flag (+0x40) as bool
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<bool*>(s + 0x40), nullptr);   // B.3.4 bridge

    // Serialize field44 (+0x44) as int
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(s + 0x44), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser2)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// PerformWork::AbstractImpl (0x1400C9420) -- slot 4
// Sets animation on entity's family component, sets facing direction
// if yaw is in valid range, and optionally decrements tool durability.
// Always returns 1 (action stays active, Execute checks completion).
// ============================================================
int  PerformWork::AbstractImpl(void* actionStack) {
    (void)actionStack;
    uint8_t* s = reinterpret_cast<uint8_t*>(this);

    void* entity = workTarget;
    uint32_t animId = *(uint32_t*)(s + 0x2C);

    // Get family component from entity's +0x278 field and set animation
    uint8_t* ent = (uint8_t*)entity;
    void* familyComp = *(void**)(ent + 0x278);
    ActMv_SetAnimation_a1f0(familyComp, animId, kRdata_14017214c, 0.0f);   // 0x14010a1f0

    // If yaw is in valid range [-180, 180], set facing direction
    float yaw = facingYaw;
    if (yaw >= RdataF(kRdata_1401722a0) && yaw <= kRdata_140172220) {
        float yawVal = yaw;
        float pitch = 0.0f;

        // Get the entity's parent for direction setting
        void* entity2 = workTarget;
        uintptr_t parentEntity = *(uintptr_t*)((uint8_t*)entity2 + 0x10);
        uintptr_t parentIBP = parentEntity;

        // Find family component on the parent
        void* familyIBP = nullptr;
        ActMv_FindComponentFamily_12d0(parentIBP, &familyIBP);   // 0x1400012d0
        void* familyPtr = *(void**)&familyIBP;

        // Convert yaw to direction vector and set it
        Math::Vector3 dirVec = Math::Vector3::FromYawPitch(Math::Scalar(yawVal), Math::Scalar(pitch));
        ActMv_SetFacingDirection_a060(familyPtr, &dirVec);   // 0x14010a060
    }

    // If use tool flag is set, decrement tool durability
    if (useTool != 0) {
        void* ent3 = workTarget;
        if (*(int32_t*)((uint8_t*)ent3 + 0xAC) > 0) {
            void* ent4 = workTarget;
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
int  PerformWork::Execute(void* actionStack, void* time) {
    (void)actionStack;

    int64_t elapsed = elapsedTime;
    int64_t required = requiredTime;

    if (elapsed >= required) {
        // Work time complete -- check if citizen needs a break
        void* entity = workTarget;
        uint8_t needsBreak = ActMv_CitizenBreakCheck_116650(entity);   // 0x140116650
        // If needs break, return 2; otherwise return 0 (done)
        return needsBreak ? 2 : 0;
    }

    // Still working -- accumulate delta time
    int64_t dt = *(int64_t*)time;
    elapsedTime = elapsed + dt;
    return 1;
}

// ============================================================
// PerformWork::Exit (0x1400C9370) -- slot 7
// Resets character animation to idle (animation 0, rate 1.0, 1.0).
// ============================================================
void PerformWork::Exit(void* actionStack) {
    (void)actionStack;

    // Get work target entity
    void* entity = workTarget;

    // Get family component at entity+0x278, reset animation to idle
    uint8_t* ent = (uint8_t*)entity;
    void* familyComp = *(void**)(ent + 0x278);
    ActMv_SetAnimation_a1f0(familyComp, 0, kRdata_14017214c, kRdata_14017214c);   // 0x14010a1f0
}

// ============================================================
// PerformWork::CheckEntity (0x1400C9320) -- slot 8
// Identical to Exit: resets character animation to idle.
// ============================================================
void PerformWork::CheckEntity() {

    // Get work target entity
    void* entity = workTarget;

    // Get family component at entity+0x278, reset animation to idle
    uint8_t* ent = (uint8_t*)entity;
    void* familyComp = *(void**)(ent + 0x278);
    ActMv_SetAnimation_a1f0(familyComp, 0, kRdata_14017214c, kRdata_14017214c);   // 0x14010a1f0
}

// ============================================================
// PerformWork::GetTarget (0x1400C92D0) -- slot 9
// Returns true if param matches the work target entity's parent (+0x10).
// ============================================================
bool PerformWork::GetTarget(void* param) {

    void* entity = workTarget;
    // Get parent entity at entity+0x10
    uintptr_t parentEntity = *(uintptr_t*)((uint8_t*)entity + 0x10);

    return parentEntity == *(uintptr_t*)param;
}

// ============================================================
// class PerformWork (PerformWork.h) — ENDGAME B.3.8 ACT.
// The hand-built g_PerformWork_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"

// create 0x140022b90. Retail's default ctor (0x1400c96b0) stores +0x44 = -1;
// the tree's factory zero-filled every field instead. B38 B8-1: the retail stores are kept; the
// fields retail leaves unwritten stay zero-filled (MemAlloc gives no guarantee).
PerformWork::PerformWork()
    : workTarget(nullptr),
      facingYaw(0.0f),
      animationId(0),
      requiredTime(0),
      elapsedTime(0),
      useTool(0),
      _pad41{},
      field44(-1) {}

// [3] 0x1400c9550
// [4] 0x1400c9420
// [5] 0x1400c93c0
// [7] 0x1400c9370
// [8] 0x1400c9320
// [9] 0x1400c92d0

void* PerformWork::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PerformWork), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) PerformWork());
}
void* PerformWork::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) PerformWork());
}


// ============================================================
// FUN_1400c9720 — PerformWork_Action Ctor (7-field variant)
// Base Internal ctor, Action -> PerformWork vtable; copies entity IBP (+0x20), anim
// index (+0x28, float arg XMM2), action type (+0x2c), Time (+0x30), flag (+0x40), int
// (+0x44). If the flag is set and the entity's +0xac < 1, scales the Time by the
// component's +0x114 seconds (Time::Time(comp.f114); self.time = AsSeconds * AsSeconds).
// animIndex arrives in XMM2 (a float); keep it as a 32-bit store at +0x28.
// ============================================================
void* Pf_PerformWorkCtor7_9720(void* self, void* entity, float animIndex, uint32_t actionType,
                               int64_t time, uint8_t param_6, int param_7) {
    // ENDGAME B.3.8 ACT: the real ctor (PerformWork.h). A by-address door.
    return static_cast<Object::Base*>(
        new (self) PerformWork(entity, animIndex, actionType, time, param_6, param_7));
}

// 0x1400c9720 — the ctor body, moved from the door above; after Action() the
// retail stores are kept verbatim at their raw offsets.
PerformWork::PerformWork(void* entity, float animIndex, uint32_t actionType, int64_t time,
                         uint8_t param_6, int param_7) {
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    *reinterpret_cast<uint64_t*>(s + 0x20) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x20) = *reinterpret_cast<uint64_t*>(entity);
    *reinterpret_cast<float*>(s + 0x28) = animIndex;
    *reinterpret_cast<uint32_t*>(s + 0x2c) = actionType;
    *reinterpret_cast<int64_t*>(s + 0x30) = time;
    *reinterpret_cast<uint64_t*>(s + 0x38) = 0;
    s[0x40] = param_6;
    *reinterpret_cast<int*>(s + 0x44) = param_7;
    if (param_6 != 0) {
        if (*reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(entity) + 0xac) < 1) {
            void* obj = *reinterpret_cast<void**>(*reinterpret_cast<int64_t*>(s + 0x20) + 0x18);
            // IsDerivedFromOrTrust: the original gates `comp = IsDerivedFrom(obj,c7190) ? obj
            // : null` then UNCONDITIONALLY reads comp+0x114 — i.e. it ASSUMES obj is the type
            // (it would crash too if not). In standalone IsDerivedFrom returns false on the
            // partial factory tree, so we TRUST obj (it IS the linked +0x18 desc) and only
            // guard the genuinely-null case (skip the work-duration scaling -> base time).
            void* comp = obj;
            if (comp) {
                // Time scaleTime(comp.f114); self.time = AsSeconds(self.time) * AsSeconds(scaleTime)
                float compSecs = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(comp) + 0x114);
                uint64_t scaleTimeStore = 0;
                RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&scaleTimeStore), compSecs);
                float selfSecs  = *reinterpret_cast<float*>(s + 0x30);
                float scaleSecs = *reinterpret_cast<float*>(&scaleTimeStore);
                uint64_t prod = 0;
                RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&prod), selfSecs * scaleSecs);
                *reinterpret_cast<uint64_t*>(s + 0x30) = prod;
            }
        }
    }
}

DEFINE_EXE_TYPE(PerformWork, "Action")
