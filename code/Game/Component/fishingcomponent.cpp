// code/Game/Component/fishingcomponent.cpp
// Clean reimplementation of FishingComponent lifecycle methods from Application-x64.exe.
//
// FishingComponent (0x58 bytes) extends ResourceProduceComponent with:
//   +0x50: uint8  fishingFlag  (serialized as bool)
//   +0x54: float  fishingRate  (serialized)
//
// Vtable at 0x140163230. One non-trivial hookable override:
//   Shutdown (0x14011f050) — ResourceProduceComponent::Shutdown + own serializer
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/fishingcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>

// ============================================================
// FishingComponent::Shutdown (vtable slot [3], 0x14011f050)
// ============================================================
// ResourceProduceComponent::Shutdown(self, stream)
// Serializer s(stream, 0)
// s.Serialize(&self[0x50], null)  // bool: fishingFlag
// s.Serialize(&self[0x54], null)  // float: fishingRate
// ~Serializer()

void FishingComponent_Shutdown(FishingComponent* self, void* stream) {
    // Base ResourceProduceComponent::Shutdown (0x14012b4a0), a direct call in retail.
    self->ResourceProduceComponent::Persist(stream);

    // Serialize FishingComponent's own fields
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    bool* pFishingFlag = reinterpret_cast<bool*>(&self->fishingFlag);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*pFishingFlag, nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->fishingRate, nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// Init: nothing left to resolve (the base Persist is a direct member call now)
// ============================================================
void FishingComponent_InitFuncs() {}

// ============================================================================
// ENDGAME B.3.8 MI-B — FishingComponent as a REAL class (FishingComponent.h).
//
// Before: CONTROLLER_LIVE_FACTORY_STATES — a 48-slot hand-built table filled per
// create (slots 3/13/14/18-21 written from InstallLive), Inert_ScalarDtor in slot 0
// and the shared RP dispatcher stamped into raw bytes at +0x28. After: the
// compiler's 22-slot vtable (13/14/16/17 inherited from ResourceProduceComponent,
// which is what retail's table holds), a real ctor/dtor chain, the controller the
// base's member (B38_RULES D1, D2, D6).
// ============================================================================
#include "Object/factory_registration.h"

// Slots 18-21: the retail bodies, ported elsewhere and reached through these.
extern "C" uintptr_t* FishingComponent_CreateWork(void*, uintptr_t*);   // resourceproduce_creatework.cpp
extern "C" char  FishingComponent_OnEnterProducing(void* self);        // fishingcomponent_produce.cpp
extern "C" char  FishingComponent_OnExitProducing(void* self);
extern "C" char  FishingComponent_ProduceTick(void* self, void* time);

// 0x14011f130: the RP ctor (CALL 0x14012b3d0), the compiler's stamp, then
// `MOV byte [RBX+0x50],0`. Retail leaves +0x54 unwritten; the tree's factory
// zeroed it and that is kept (no reader precedes the first ProduceTick/Persist).
FishingComponent::FishingComponent()
    : ResourceProduceComponent(), fishingFlag(0), _pad51{}, fishingRate(0.0f) {}

// [0] 0x140034560: restamps + ~Internal (the compiler chain), MemFree on &1.
Object::Base* FishingComponent::DeletingDtor(unsigned flags) {
    this->~FishingComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140034350: MemAlloc(0x58, 8), then the ctor.
void* FishingComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(FishingComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) FishingComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* FishingComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) FishingComponent());
}

void       FishingComponent::Persist(void* stream)     { FishingComponent_Shutdown(this, stream); }  // [3]  0x14011f050
uintptr_t* FishingComponent::CreateWork(uintptr_t* out) { return FishingComponent_CreateWork(this, out); }  // [18] 0x14011efe0
char       FishingComponent::OnEnterProducing()        { return FishingComponent_OnEnterProducing(this); }  // [19] 0x14011ee80
char       FishingComponent::OnExitProducing()         { return FishingComponent_OnExitProducing(this); }   // [20] 0x14011ee50
char       FishingComponent::ProduceTick(void* time)   { return FishingComponent_ProduceTick(this, time); } // [21] 0x14011ec10

DEFINE_EXE_VTABLE(FishingComponent, "_ZTV16FishingComponent")
DEFINE_EXE_TYPE(FishingComponent, "ResourceProduceComponent")
