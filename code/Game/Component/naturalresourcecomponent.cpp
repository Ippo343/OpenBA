// code/Game/Component/naturalresourcecomponent.cpp
// Clean reimplementation of NaturalResourceComponent lifecycle methods.
//
// NaturalResourceComponent (0x30 bytes) extends Component with:
//   +0x28: float  resourceScale  (initialized to 1.0f)
//   +0x2C: bool   mature, +0x2D bool harvestable (initialized by one word store 0x100)
//
// Vtable at 0x140163e10. Two non-trivial overrides:
//   Shutdown (0x140125cf0) — Serialize resourceScale + 2 bool fields
//   OnRemove (0x140125c40) — Complex animation/path logic
//

// graphics/Mesh.h comes FIRST: it pulls runtime/types/volume/Volume.h, whose
// global-scope `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()`
// function once <windows.h> has been seen (via resolve.h), which turns
// Volume.h's own `sizeof(Rectangle)` assert into a hard error.
#include "graphics/Mesh.h"   // ENDGAME B.3.5 caller batch I2
#include "Component/naturalresourcecomponent_funcs.h"
#include "Component/ParticleComponent.h"
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "Action/exe_bodies.h"   // ActCNR_RemoveNatRes_1260f0
#include "Entity/ComponentEntity.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/exe_bodies.h"

// ============================================================
// NaturalResourceComponent::OnRemove (0x140125c40)
// ============================================================
// Checks animation state, clears path queue, modifies flags,
// potentially reassigns natural resource ownership.
//
// Nothing calls this body (the class's slot 8 is a PORT GAP, below). B3R: its three
// doors (0x140005200, 0x1400c3470, 0x1401260f0, filled only by an init function that
// nothing called) are the bodies by name.

//
// NOT INSTALLED (B.4 COMP-d): retail's finder 0x140005200 is FindComponent<ParticleComponent>
// (cell 0x1401c4720, the same finder diseasecomponent.cpp uses), and a ParticleComponent is
// 0x48 bytes, yet this body reads its +0x28 as a Graphics::MeshInstance. The tick bit it
// clears is set by AnimateNaturalResource (0x140126440), which animates the MODEL
// component (0x140003f60). Installing it as slot 8 needs a ruling on that read.
void NaturalResourceComponent_OnRemove(NaturalResourceComponent* self) {
    ParticleComponent* found = self->_entity->FindComponent<ParticleComponent>();   // 0x140005200

    // LEFT: retail reads the found component's +0x28 as a MeshInstance (see above).
    bool isAnimating = reinterpret_cast<const Graphics::MeshInstance*>(
        reinterpret_cast<const uint8_t*>(found) + 0x28)->IsAnimating();
    if (isAnimating) return;

    // Clear bit 2 of the flags
    uint32_t oldFlags = self->_flags;
    uint32_t newFlags = oldFlags & ~0x4u;
    self->_flags = newFlags;

    // If flags changed in low 16 bits and entity exists, update
    if (((newFlags ^ oldFlags) & 0xFFFF) != 0 && self->_entity != nullptr) {
        self->_entity->RefreshEntityFlags();   // 0x1400c3470
    }

    // If bit 21 of flags is set, reassign resource
    if ((self->_flags >> 0x15) & 1) {
        void* nullRef = nullptr;   // InternalPtr (the ctor is a plain store)
        // LEFT: ActCNR_RemoveNatRes_1260f0 takes the component as int64_t (Action's body).
        ActCNR_RemoveNatRes_1260f0(reinterpret_cast<int64_t>(self), &nullRef, 0);   // 0x1401260f0
    }
}

// ============================================================
// 0x140005200 — FindComponent<ParticleComponent> into an InternalPtr out-slot.
// ============================================================
// B4 FORWARD: 1 outside caller file (diseasecomponent.cpp).
void NaturalResourceComponent_FindComponentByType(void* entity, void* outIBP) {
    new (outIBP) ::Object::InternalPtr<ParticleComponent>(
        reinterpret_cast<ComponentEntity*>(entity)->FindComponent<ParticleComponent>());
}

// ===== ENDGAME B.3.8 CIT — NaturalResourceComponent as a REAL class =====
// (NaturalResourceComponent.h). Before: a hand-expanded COMPONENT_INERT_FACTORY
// (48-slot NaturalResourceComponent_InertVtable, slot 3 the serializer,
// Inert_ScalarDtor in slot 0) plus the ctor defaults patched in after the
// zero-fill. After: the compiler's 16-slot vtable and a real ctor/dtor chain
// (B38_RULES D1, D2); the ctor defaults are the ctor's own.
#include "Object/factory_registration.h"
#include "Component/NaturalResourceComponent.h"

// 0x140125dc0 (heap twin inlined in create 0x140036550). Component() is the
// CALL 0x1401188b0; the 0x140163e10 stamp is the compiler's; then
//   140125de6: MOV dword ptr [RBX+0x28], 0x3f800000   ; resourceScale = 1.0f
//   140125ded: MOV word  ptr [RBX+0x2c], 0x100        ; +0x2c = 0, +0x2d = 1
// That second store is a PACKED 16-bit write covering BOTH bools: +0x2c (mature)
// = 0, +0x2d (harvestable) = 1. Missing the +0x2d half is not cosmetic:
// harvestable = 0 makes FUN_1401260f0 early-out, so a chopped tree/rock/ore
// yields no resource pile. No flag bits are added (flags stay 0x80).
NaturalResourceComponent::NaturalResourceComponent()
    : resourceScale(1.0f), mature(false), harvestable(true), _pad2E{} {}

// ============================================================
// NaturalResourceComponent::Persist (vtable slot [3], 0x140125cf0)
// ============================================================

void NaturalResourceComponent::Persist(IO::Stream& stream) {
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    IO::Serializer ser(stream, 0);

    ser.Serialize(resourceScale, nullptr);
    ser.Serialize(mature, nullptr);
    ser.Serialize(harvestable, nullptr);
}

// [8] 0x140125c40. PORT GAP (retail 0x140125c40) — B38 D9: the body is ported
// above (NaturalResourceComponent_OnRemove) but the tree's table never
// installed it; the inert stub is kept here (see the NOT INSTALLED note above).
void NaturalResourceComponent::Tick(void*) {}

// create 0x140036550: MemAlloc(0x30, 8), then the (inlined) ctor.
void* NaturalResourceComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(NaturalResourceComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) NaturalResourceComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* NaturalResourceComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) NaturalResourceComponent());
}

DEFINE_EXE_TYPE(NaturalResourceComponent, "Component")
