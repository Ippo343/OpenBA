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

#include "Component/autopickupcomponent_funcs.h"
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

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// The +0x30 int array. ENDGAME B.3.2 step 2: FUN_1400021d0 IS
// `Collection::Array<int32>::SetSize`, MSVC-monomorphised into the exe, so the
// resolved pointer is gone and the member call takes its place.

// IsDerivedFrom dispatch slot
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

// Type ID for AutoPickupDescription at DAT_1401c3fe8
static uint32_t* s_TypeId_AutoPickupDesc = nullptr;

static bool s_initialized = false;

// ============================================================
// AutoPickupComponent::Shutdown (vtable slot [3], 0x1400449d0)
// ============================================================
// Serializes:
//   1. Component_Serialize base
//   2. Serializer(version=0): serialize isEnabled bool at +0x28
//   3. Read count from +0x38, serialize it
//   4. If loading: resize array at +0x30 via FUN_1400021d0
//   5. Bulk-serialize int array elements
void AutoPickupComponent_Shutdown(AutoPickupComponent* self, void* stream) {
    // resolver call added at top, save/load wave 2026-08-28: InitFuncs was declared
    // but never called anywhere, so every s_fn_* below stayed null and load silently
    // skipped this component's fields.
    AutoPickupComponent_InitFuncs();
    uintptr_t selfAddr = (uintptr_t)self;

    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize isEnabled bool at +0x28
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(selfAddr + 0x28), nullptr);   // B.3.4 bridge

    // Read count from +0x38, serialize it
    int localCount = *(int*)(selfAddr + 0x38);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(localCount, nullptr);   // B.3.4 bridge

    // If loading (stream mode == 0), resize array at +0x30
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        // B.3.2 bridge: +0x30 is the { int32* data; int32 size; int32 capacity }
        // header; AutoPickupComponent is not a real class yet (§B.2). I32NoInit — exe
        // 0x1400021d0 (called here at 140044a40) copies only the old prefix and leaves a
        // grown tail as-found. Safe: this arm runs only when loading, and the bulk int
        // read immediately below (140044a55) writes all localCount elements from the
        // stream before any of them is read.
        GameColl::AsArray<GameColl::I32NoInit>((void*)(selfAddr + 0x30)).SetSize(localCount);
    }

    // Bulk-serialize int array
    int* arrayPtr = *(int**)(selfAddr + 0x30);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(localCount, arrayPtr, nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();                                       // B.3.4 bridge
}

// ============================================================
// AutoPickupComponent::Serialize (vtable slot [4], 0x140044990)
// ============================================================
// Checks IsDerivedFrom on +0x18 with type DAT_1401c3fe8,
// reads byte at +0x20 from the derived object and stores in isEnabled (+0x28).
void AutoPickupComponent_Serialize(AutoPickupComponent* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    void* obj = *(void**)(selfAddr + 0x18);
    void* config = nullptr;

    if (obj && s_fn_IsDerivedFrom_dispatch && s_TypeId_AutoPickupDesc) {
        bool derived = s_fn_IsDerivedFrom_dispatch(obj, *s_TypeId_AutoPickupDesc);
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

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    s_TypeId_AutoPickupDesc = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c3fe8));

    s_initialized = true;

#ifdef _DEBUG
    printf("[AutoPickupComponent] Funcs resolved: CompSerialize=%p TypeId=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_TypeId_AutoPickupDesc);
#endif
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

// [3] 0x1400449d0 — isEnabled bool + the int array.
void AutoPickupComponent::Persist(void* stream) { AutoPickupComponent_Shutdown(this, stream); }

// [4] 0x140044990. PORT GAP (retail 0x140044990) — B38 D9: the body is ported
// above (AutoPickupComponent_Serialize: isEnabled from the description) but the
// tree's table never installed it; the inert stub is kept here.
void AutoPickupComponent::Startup() {}
// [13] 0x140044970 / [14] 0x140044930. PORT GAP — B38 D9: never ported; the
// tree's table left them inert (13 answered 0); kept inert here.
uint32_t AutoPickupComponent::CommandMask(int) { return 0; }
void AutoPickupComponent::Command(int) {}

// [0] 0x140003590: ~AutoPickupComponent (0x140044a80; its array free is a PORT GAP, see the header), MemFree on &1.
Object::Base* AutoPickupComponent::DeletingDtor(unsigned flags) {
    this->~AutoPickupComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

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

DEFINE_EXE_VTABLE(AutoPickupComponent, "_ZTV19AutoPickupComponent")
DEFINE_EXE_TYPE(AutoPickupComponent, "Component")
