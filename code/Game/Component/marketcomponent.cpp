// code/Game/Component/marketcomponent.cpp
// Clean reimplementation of MarketComponent lifecycle methods from Application-x64.exe.
//
// MarketComponent is a ResourceProduceComponent subclass (0x50 bytes) with no
// additional fields beyond the parent. Only one non-trivial vtable override:
//   Shutdown (0x14004ead0) — calls ResourceProduceComponent_Shutdown + empty Serializer block
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/MarketComponent.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>


// ============================================================================
// ENDGAME B.3.8 MI-B — MarketComponent as a REAL class (MarketComponent.h).
//
// Before: CONTROLLER_LIVE_FACTORY_STATES (48-slot table filled per create, slots
// 3/13/14/18 written from InstallLive, Inert_ScalarDtor in slot 0, the shared RP
// dispatcher stamped at +0x28). After: the compiler's 22-slot vtable (13/14/16/17
// and 19-21 inherited from ResourceProduceComponent, as retail's table holds), a
// real ctor/dtor chain, the controller the base's member (B38_RULES D1, D2, D6).
// ============================================================================
#include "Object/factory_registration.h"

// 0x14004eb70: the RP ctor (CALL 0x14012b3d0) and the compiler's stamp. Nothing else.
MarketComponent::MarketComponent() : ResourceProduceComponent() {}

// [0] 0x140008440: restamps + ~Internal (the compiler chain), MemFree on &1.
Object::Base* MarketComponent::DeletingDtor(unsigned flags) {
    this->~MarketComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140008220: MemAlloc(0x50, 8), then the ctor.
void* MarketComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(MarketComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) MarketComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* MarketComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) MarketComponent());
}

// ============================================================
// MarketComponent::Shutdown (vtable slot [3], 0x14004ead0)
// ============================================================
// Calls parent ResourceProduceComponent_Shutdown, then creates/destroys
// an empty Serializer block (version 0). This empty block exists for
// binary format compatibility (versioned sub-section with no fields).
//
// Original disassembly:
//   CALL ResourceProduceComponent_Shutdown (0x14012b4a0)
//   CALL Serializer_Ctor(local, stream, 0)
//   CALL Serializer_Dtor(local)

void MarketComponent::Persist(void* stream) {
    // Base ResourceProduceComponent::Shutdown (0x14012b4a0), a direct call in retail.
    ResourceProduceComponent::Persist(stream);

    // Empty serializer block (version 0)
    alignas(8) uint8_t serializerBuf[24];
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge
}

// [18] 0x14004ea40 MarketComponent::CreateWork lives in resourceproduce_creatework.cpp.

DEFINE_EXE_VTABLE(MarketComponent, "_ZTV15MarketComponent")
DEFINE_EXE_TYPE(MarketComponent, "ResourceProduceComponent")
