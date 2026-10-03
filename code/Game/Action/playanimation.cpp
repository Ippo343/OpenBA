// code/Game/Action/playanimation.cpp
// Clean C++ reimplementation of PlayAnimation action methods.
// Source: Game/Action/PlayAnimation.cpp (from profile build debug strings)
//
// PlayAnimation -- 0x40 bytes. Plays an animation on a citizen entity.
// Inherits: Object::Internal -> Action (0x20) -> PlayAnimation
// Vtable at 0x140158950 (10 slots).
//
// Decompiled from Application-x64.exe using Ghidra.

#include "PlayAnimation.h"
#include "object/TypeInfo.h"
#include "Action/exe_bodies.h"   // FindCharacterComponent, SetAnimation
#include "Game/exe_constants.h"  // kRdata_14017214c
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "Game/force_work.h"   // ForceWork::Readable — VirtualQuery-backed crash-safe probe
#include <cstdio>
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include "Entity/ComponentEntity.h"

// ============================================================
// Helpers, called by name (B3R)
//   0x1400012d0 ActMv_FindComponentFamily_12d0 (Entity_FindComponent_Family)
//   0x14010a1f0 ActMv_SetAnimation_a1f0 -- set animation on family component
// DAT_14017214c (1.0f, the animation-setup float) is kRdata_14017214c (Game/exe_constants.h).
// ============================================================

// ============================================================
// PlayAnimation::Serialize (0x140042480)
// Serializes: status(+0x18), entity(+0x20) ComponentEntity, int(+0x28),
//   Time(+0x30), Time(+0x38)
// ============================================================
void PlayAnimation::Persist(IO::Stream& stream) {
    Action::Persist(stream);   // inlined in retail: Serializer(stream, 0), status +0x18

    IO::Serializer ser(stream, 0);
    _field20.InternalBasePtr::Serialize(ser);
    if (_field20.Get() != nullptr) {
        if (!::Object::IsDerivedFromNullSafe(_field20.Get(), ComponentEntity::TypeId())) {
            _field20 = nullptr;
        }
    }
    ser.Serialize(animation, nullptr);   // +0x28 int
    _field30.Serialize(ser);             // +0x30 Time
    _field38.Serialize(ser);             // +0x38 Time
}

// ============================================================
// PlayAnimation::AbstractImpl / _purecall (0x140042420)
// Finds the entity's CharacterComponent, plays the animation.
// Returns 1 (action stays active, Execute checks completion).
// ============================================================
// [STALE-TARGET GUARD + DIAG] The PlayAnimation target entity IBP at +0x20 is an
// owning-weak ref into the InternalFactory handle table (see project-pile-collection-next:
// objects register via the real Object::Internal ctor -> AddObject, and CheckPtr validates
// by index+generation). In standalone, an object built WITHOUT the real ctor is never
// table-registered, so CheckPtr above cannot invalidate a ref to it once it is freed:
// *(s+0x20) then holds a freed pointer or an unresolved packed handle (gen<<32|idx), and
// FindFamily's `*(int*)(entity+0x38)` faults (observed on the LIVE_DESTROY + RAWMAT_PICKUP
// haul path 2026-07-24, value 0x1700000c04d). Retail relies on proper invalidation — the ref
// reads null and the null guard below returns 0. Reproduce that outcome: if the target is not
// a readable, plausibly-live object, log its identity (to pin the unregistered build-site for
// the real fix) then abort the action exactly like the null path.
static bool PlayAnim_TargetLooksLive(const ComponentEntity* target) {
    // Plausible-pointer + readability: the object header (vtable @+0, index @+0x08,
    // gen @+0x0c) plus a little slack must be mapped. A packed handle (~0x1700000c04d)
    // or a freed/unmapped page fails here.
    if (!ForceWork::Readable(target, 0x40)) {
        static int n = 0;
        if (n < 16) { ++n;
            std::fprintf(stderr, "[PlayAnimGuard] ABORT: target=%p UNREADABLE "
                                 "(unregistered/freed weak ref) — aborting action\n",
                         static_cast<const void*>(target));
            std::fflush(stderr);
        }
        return false;
    }
    // Readable: capture the vtable + factory index/gen so the object type can be
    // identified offline (match vtable vs registered vtables / nm reimpl-x64.exe).
    void* vt = *reinterpret_cast<void* const*>(target);  // CAST-OK: crash_walk
    if (!ForceWork::Readable(vt, 0x18)) {
        static int n = 0;
        if (n < 16) { ++n;
            std::fprintf(stderr, "[PlayAnimGuard] ABORT: target=%p vtable=%p UNREADABLE "
                                 "vtable — aborting action\n", static_cast<const void*>(target), vt);
            std::fflush(stderr);
        }
        return false;
    }
    // Looks live. Log identity once-ish so a real (registered) target is also visible for
    // comparison against the stale cases (throttled).
    {
        static int n = 0;
        if (n < 24) { ++n;
            uint32_t idx = static_cast<uint32_t>(target->GetFactoryIndex());
            uint32_t gen = target->GetGeneration();
            std::fprintf(stderr, "[PlayAnimGuard] target=%p vtable=%p factIdx=%u gen=0x%08x (live)\n",
                         static_cast<const void*>(target), vt, idx, gen);
            std::fflush(stderr);
        }
    }
    return true;
}

int  PlayAnimation::AbstractImpl(void* actionStack) {
    (void)actionStack;

    if (_field20.Get() == nullptr) {
        return 0;
    }
    ComponentEntity* target = _field20.Get();
    if (!PlayAnim_TargetLooksLive(target)) {
        return 0;   // stale/unregistered target — abort like retail's invalidated-null path
    }
    CharacterComponent* character = FindCharacterComponent(target);   // 0x1400012d0
    if (character == nullptr) {
        return 0;
    }
    SetAnimation(character, static_cast<uint32_t>(animation), kRdata_14017214c, kRdata_14017214c);   // 0x14010a1f0
    return 1;
}

// ============================================================
// PlayAnimation::Execute (0x140042400)
// Accumulates elapsed time, returns true while still animating.
//   self->_field38 += *time;
//   return (self->_field38 < self->_field30);
// ============================================================
int  PlayAnimation::Execute(void* actionStack, void* time) {
    (void)actionStack;
    int64_t elapsed = *(int64_t*)time;   // LEFT: Action::Execute's time parameter is void* (Action.h)
    _field38._ticks += elapsed;
    return (_field38._ticks < _field30._ticks) ? 1 : 0;
}

// ============================================================
// PlayAnimation::GetTarget (0x1400423b0)
// Returns true if param matches entity at +0x20.
// ============================================================
bool PlayAnimation::GetTarget(void* param) {
    return _field20.Get() == *(void**)param;   // LEFT: Action::GetTarget's parameter is void* (Action.h)
}

// ============================================================
// class PlayAnimation (PlayAnimation.h) — ENDGAME B.3.8 ACT.
// The hand-built g_PlayAnimation_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"

// create 0x1400011f0 (inlined ctor). The tree's factory zeroed every field above
// +0x10; kept (not a behaviour change of this step).
PlayAnimation::PlayAnimation()
    : _field20(nullptr),
      animation(0),
      _pad2C{},
      _field30{0},
      _field38{0} {}

// [3] 0x140042480
// [4] 0x140042420
// [5] 0x140042400
// [8] 0x1400423f0 — the leaf's own `RET 0` (same bytes as the base body)
void PlayAnimation::CheckEntity() {}
// [9] 0x1400423b0

void* PlayAnimation::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PlayAnimation), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) PlayAnimation());
}
void* PlayAnimation::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) PlayAnimation());
}


DEFINE_EXE_TYPE(PlayAnimation, "Action")
