// code/Game/Component/naturalresourcecomponent.cpp
// Clean reimplementation of NaturalResourceComponent lifecycle methods.
//
// NaturalResourceComponent (0x30 bytes) extends Component with:
//   +0x28: float  resourceScale  (initialized to 1.0f)
//   +0x2C: uint16 resourceFlags  (initialized to 0x100)
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
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// Resolved helpers
// ============================================================

// Entity_ClearPathQueue(entity, outIBP)
typedef void (*fn_ClearPathQueue_t)(void* entity, void* outIBP);
static fn_ClearPathQueue_t s_fn_ClearPathQueue = nullptr;

// FUN_1400c3470 — entity flag update helper
typedef void (*fn_EntityFlagUpdate_t)(void* entity);
static fn_EntityFlagUpdate_t s_fn_EntityFlagUpdate = nullptr;

// FUN_1401260f0 — reassign natural resource
typedef void (*fn_ReassignResource_t)(void* self, void* ibp, char flag);
static fn_ReassignResource_t s_fn_ReassignResource = nullptr;

static bool s_initialized = false;

// ============================================================
// NaturalResourceComponent::OnRemove (0x140125c40)
// ============================================================
// Checks animation state, clears path queue, modifies flags,
// potentially reassigns natural resource ownership.

void NaturalResourceComponent_OnRemove(NaturalResourceComponent* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    // CheckPtr on owner IBP

    // Clear path queue — get model component from entity
    uintptr_t modelIBP = 0;
    if (s_fn_ClearPathQueue)
        s_fn_ClearPathQueue(*(void**)(selfAddr + 0x10), &modelIBP);

    // CheckPtr on model IBP

    // Check if mesh is animating
    bool isAnimating = reinterpret_cast<Graphics::MeshInstance*>(modelIBP + 0x28)->IsAnimating();   // B.3.5 bridge
    if (isAnimating) return;

    // Clear bit 2 of flags (+0x20)
    uint32_t oldFlags = *(uint32_t*)(selfAddr + 0x20);
    uint32_t newFlags = oldFlags & ~0x4u;
    *(uint32_t*)(selfAddr + 0x20) = newFlags;

    // If flags changed in low 16 bits and entity exists, update
    if (((newFlags ^ oldFlags) & 0xFFFF) != 0 && *(uintptr_t*)(selfAddr + 0x10) != 0) {
        if (s_fn_EntityFlagUpdate)
            s_fn_EntityFlagUpdate(*(void**)(selfAddr + 0x10));
    }

    // If bit 21 of flags is set, reassign resource
    if ((*(uint32_t*)(selfAddr + 0x20) >> 0x15) & 1) {
        uint8_t nullIBP[16];
        if (fn_InternalBasePtr_Ctor)
            new (nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        if (s_fn_ReassignResource)
            s_fn_ReassignResource((void*)selfAddr, nullIBP, 0);
    }
}

// ============================================================
// Reimpl of FUN_140005200 — find component by type, wrap in InternalBasePtr
// ============================================================
// Iterates the entity's component IBP array (data at entity+0x30, count at
// entity+0x38). For each slot, CheckPtr then call the component's GetTypeId
// (vtable+0x10); on the first slot whose type id == DAT_1401c4720, re-reads
// that slot, runs IsDerivedFrom against the same id, and wraps the result
// (or null) into the caller-provided InternalBasePtr (outIBP). Matches the
// existing fn_ClearPathQueue_t typedef: void(void* entity, void* outIBP).
// NON-STATIC so the standalone registrar can take its address.
void NaturalResourceComponent_FindComponentByType(void* entity, void* outIBP) {
    uintptr_t ent = (uintptr_t)entity;
    void* found = nullptr;

    const uint32_t wantId = ParticleComponent::TypeId();   // ti cell 0x1401c4720

    int count = *(int*)(ent + 0x38);
    uintptr_t arrBase = *(uintptr_t*)(ent + 0x30);

    for (int i = 0; i < count; ++i) {
        void** slot = reinterpret_cast<void**>(arrBase + (uintptr_t)i * 8);

        void* comp = *slot;
        // virtual Type() (slot 2)
        uint32_t compId = 0;
        if (comp) compId = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge

        if (compId == wantId) {
            // re-read the matching slot (matches original's second CheckPtr)
            void** matchSlot = reinterpret_cast<void**>(arrBase + (uintptr_t)i * 8);
            void* candidate = *matchSlot;
            if (candidate && fn_IsDerivedFrom &&
                fn_IsDerivedFrom(candidate, wantId)) {
                found = candidate;
            }
            break;
        }
    }

    if (fn_InternalBasePtr_Ctor)
        new (outIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void NaturalResourceComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_ClearPathQueue = reinterpret_cast<fn_ClearPathQueue_t>(
        ResolveAddr(Module::Exe, 0x140005200));  // Entity_ClearPathQueue used in decomp
    s_fn_EntityFlagUpdate = reinterpret_cast<fn_EntityFlagUpdate_t>(
        ResolveAddr(Module::Exe, 0x1400c3470));
    s_fn_ReassignResource = reinterpret_cast<fn_ReassignResource_t>(
        ResolveAddr(Module::Exe, 0x1401260f0));

    s_initialized = true;
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
    : resourceScale(1.0f), resourceFlags(0x100), _pad2E{} {}

// [0] 0x1400366b0: ~NaturalResourceComponent (0x140125d80: restamp +
// ~Internal, no own work), MemFree on &1.
Object::Base* NaturalResourceComponent::DeletingDtor(unsigned flags) {
    this->~NaturalResourceComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// ============================================================
// NaturalResourceComponent::Persist (vtable slot [3], 0x140125cf0)
// ============================================================

void NaturalResourceComponent::Persist(void* stream) {
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(resourceScale, nullptr);   // B.3.4 bridge

    // resourceFlags is uint16_t at +0x2C; the original serializes
    // the two bytes individually as bool at +0x2C and +0x2D
    bool* flagLow  = reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(this) + 0x2C);
    bool* flagHigh = reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(this) + 0x2D);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*flagLow, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*flagHigh, nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// [8] 0x140125c40. PORT GAP (retail 0x140125c40) — B38 D9: the body is ported
// above (NaturalResourceComponent_OnRemove) but the tree's table never
// installed it; the inert stub is kept here. Installing it is its own titled
// commit.
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

DEFINE_EXE_VTABLE(NaturalResourceComponent, "_ZTV24NaturalResourceComponent")
DEFINE_EXE_TYPE(NaturalResourceComponent, "Component")
