// code/Game/Component/autopickupcomponent.cpp
// Clean reimplementation of AutoPickupComponent lifecycle methods.
//
// AutoPickupComponent (0x40 bytes) auto-collects dropped resources near buildings.
// Fields: +0x28=isEnabled (byte), +0x30=array ptr, +0x38=array count.
//
// Vtable at 0x140158df0. Non-trivial overrides:
//   Shutdown  (0x1400449d0) -- Serializes isEnabled bool + int array
//   Serialize (0x140044990) -- IsDerivedFrom check to init isEnabled from config
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/AutoPickupComponent.h"
#include "Component/AutoPickupDescription.h"   // ENDGAME B.3.9 CB: 0x1401c3fe8 cell class
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>

// ============================================================
// Resolved helpers
// ============================================================

// The +0x30 int array. ENDGAME B.3.2 step 2: FUN_1400021d0 IS
// `Collection::Array<int32>::SetSize`, MSVC-monomorphised into the exe, so the
// resolved pointer is gone and the member call takes its place.

// IsDerivedFrom dispatch slot
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

static bool s_initialized = false;

// ============================================================
// AutoPickupComponent::Serialize (vtable slot [4], 0x140044990)
// ============================================================
// Checks IsDerivedFrom on +0x18 with type DAT_1401c3fe8 (AutoPickupDescription::TypeId();
// ENDGAME B.3.9 CB -- the cell is unregistered, so the old pointer read made this a no-op),
// reads byte at +0x20 from the derived object and stores in isEnabled (+0x28).
void AutoPickupComponent_InitFuncs();
void AutoPickupComponent_Serialize(AutoPickupComponent* self) {
    AutoPickupComponent_InitFuncs();   // its only user now (B.3.8: Persist is folded into the member)
    uintptr_t selfAddr = (uintptr_t)self;

    void* obj = *(void**)(selfAddr + 0x18);
    void* config = nullptr;

    if (obj && s_fn_IsDerivedFrom_dispatch) {
        bool derived = s_fn_IsDerivedFrom_dispatch(obj, AutoPickupDescription::TypeId());
        if (derived) config = obj;
    }

    if (config) {
        *(uint8_t*)(selfAddr + 0x28) = *(uint8_t*)((uintptr_t)config + 0x20);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void AutoPickupComponent_InitFuncs() {
    if (s_initialized) return;

    uintptr_t slot;

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    s_initialized = true;

}

// ============================================================================
// ENDGAME B.3.8 VIS — AutoPickupComponent as a REAL class (AutoPickupComponent.h).
//
// Before: COMPONENT_INERT_FACTORY_SER — a 48-slot table filled per create,
// Inert_ScalarDtor in slot 0 (only ~Internal).
// After: the compiler's 16-slot vtable and a real ctor/dtor chain
// (B38_RULES D1, D2); slot bodies are the ones the old table installed.
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/AutoPickupComponent.h"

// 0x140044ae0: Component() is the CALL 0x1401188b0; the 0x140158df0 stamp is
// the compiler's; +0x30 = +0x38 = 0; flags |= 0x80. PORT GAP (retail
// 0x140044ae0) — B38 D1: retail writes isEnabled (+0x28) = 1; the tree's
// zero-fill is kept here and the retail value is its own titled commit.
AutoPickupComponent::AutoPickupComponent()
    : isEnabled(0), _pad29{}, field30(nullptr), field38(nullptr) {
    OrFlagsInCtor(0x80);
}

// [3] 0x1400449d0 — Component::Persist (a direct CALL 0x1401189b0), then ONE
// Serializer scope: isEnabled (+0x28), the int array's count (+0x38; on a load the
// +0x30 array is resized to it) and the array elements in bulk.
void AutoPickupComponent::Persist(void* stream) {
    Component::Persist(stream);
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (stream is void* in Internal::Persist)
    ser.Serialize(*reinterpret_cast<bool*>(&isEnabled), nullptr);
    int count = *reinterpret_cast<int*>(&field38);
    ser.Serialize(count, nullptr);
    if (static_cast<int>(ser.GetDirection()) == 0) {   // IO::Serializer::GetDirection, retail 0x180006210
        // B.3.2 bridge: +0x30 is the { int32* data; int32 size; int32 capacity }
        // header. I32NoInit — exe 0x1400021d0 (called at 140044a40) copies only the
        // old prefix and leaves a grown tail as-found. Safe: this arm runs only when
        // loading, and the bulk read below (140044a55) writes all `count` elements
        // from the stream before any of them is read.
        GameColl::AsArray<GameColl::I32NoInit>(&field30).SetSize(count);
    }
    ser.Serialize(count, *reinterpret_cast<int**>(&field30), nullptr);
}

// [4] 0x140044990. PORT GAP (retail 0x140044990) — B38 D9: the body is ported
// above (AutoPickupComponent_Serialize: isEnabled from the description) but the
// tree's table never installed it; the inert stub is kept here.
void AutoPickupComponent::Startup() {}
// [13] 0x140044970 / [14] 0x140044930. PORT GAP — B38 D9: never ported; the
// tree's table left them inert (13 answered 0); kept inert here.
uint32_t AutoPickupComponent::CommandMask(int) { return 0; }
void AutoPickupComponent::Command(int) {}

// create 0x1400033c0: MemAlloc(0x40, 8), then the ctor.
void* AutoPickupComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(AutoPickupComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) AutoPickupComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* AutoPickupComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) AutoPickupComponent());
}

DEFINE_EXE_TYPE(AutoPickupComponent, "Component")
