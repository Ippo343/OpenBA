// code/Game/Component/boatcomponent.cpp
// Clean reimplementation of BoatComponent lifecycle methods.
//
// BoatComponent (0x98 bytes) manages fishing boat state:
// entity refs, position vectors, route waypoints, dock ID.
// Secondary vtable sub-object at +0x78 (ControllerComponent pattern).
//
// Vtable at 0x140158f40. Non-trivial override:
//   Shutdown (0x1400465d0) -- 95-instruction complex serialization
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/boatcomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "math/Math.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IsDerivedFrom dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

// FUN_14000d030 -- resize Vector3 array. ENDGAME B.3.2 step 2: this IS
// Collection::Array<Vector3>::SetSize, implemented below as
// BoatComponent_ResizeVec3Array (kept, and still registered by
// reg_component_nature.cpp for other callers) — this file's own call site
// now goes straight through the GameColl bridge instead of resolving itself.

// Type IDs
static uint32_t* s_TypeId_ComponentEntity = nullptr; // TypeInfo_ComponentEntity
static uint32_t* s_TypeId_1401c7a50 = nullptr;       // DWORD_1401c7a50
static uint32_t* s_TypeId_1401c7078 = nullptr;       // DAT_1401c7078

static bool s_initialized = false;

// ============================================================
// BoatComponent::Shutdown (vtable slot [3], 0x1400465d0)
// ============================================================
// Serializes:
//   1. Component_Serialize base
//   2. Empty serializer block (version 0) -- format compat
//   3. Serializer(version=0):
//      - InternalBasePtr at +0x28 (entity ref, validated vs ComponentEntity)
//      - Vector3 at +0x40 (position)
//      - Float at +0x4c
//      - InternalBasePtr at +0x30 (validated vs type 0x1401c7a50)
//      - InternalBasePtr at +0x38 (validated vs type 0x1401c7078)
//      - Vector3 at +0x50 (direction/target)
//      - Int at +0x70 (dock ID)
//      - Vector3 array at +0x60 (count at +0x68) -- route waypoints
void BoatComponent_Shutdown(BoatComponent* self, void* stream) {
    BoatComponent_InitFuncs();
    uintptr_t selfAddr = (uintptr_t)self;

    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    // Empty serializer block (format compat)
    alignas(8) uint8_t serializer1[24];
    ::new (static_cast<void*>(serializer1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer1)->~Serializer();                                       // B.3.4 bridge

    // Main serializer block
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize InternalBasePtr at +0x28 (entity ref)
    reinterpret_cast<Object::InternalBasePtr*>((void*)(selfAddr + 0x28))->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge

    // Validate +0x28 against ComponentEntity type
    if (*(void**)(selfAddr + 0x28) != nullptr) {
        if (s_fn_IsDerivedFrom_dispatch && s_TypeId_ComponentEntity) {
            bool valid = s_fn_IsDerivedFrom_dispatch(
                *(void**)(selfAddr + 0x28), *s_TypeId_ComponentEntity);
            if (!valid)
                *(uint64_t*)(selfAddr + 0x28) = 0;
        }
    }

    // Serialize Vector3 at +0x40 (position) -- slot 0x140152a68
    reinterpret_cast<Math::Vector3*>(selfAddr + 0x40)->Serialize(serializer);   // B.3.1 bridge

    // Serialize float at +0x4c
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(selfAddr + 0x4c), nullptr);   // B.3.4 bridge

    // Serialize InternalBasePtr at +0x30
    reinterpret_cast<Object::InternalBasePtr*>((void*)(selfAddr + 0x30))->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge

    // Validate +0x30 against type 0x1401c7a50
    if (*(void**)(selfAddr + 0x30) != nullptr) {
        if (s_fn_IsDerivedFrom_dispatch && s_TypeId_1401c7a50) {
            bool valid = s_fn_IsDerivedFrom_dispatch(
                *(void**)(selfAddr + 0x30), *s_TypeId_1401c7a50);
            if (!valid)
                *(uint64_t*)(selfAddr + 0x30) = 0;
        }
    }

    // Serialize InternalBasePtr at +0x38
    reinterpret_cast<Object::InternalBasePtr*>((void*)(selfAddr + 0x38))->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge

    // Validate +0x38 against type 0x1401c7078
    if (*(void**)(selfAddr + 0x38) != nullptr) {
        if (s_fn_IsDerivedFrom_dispatch && s_TypeId_1401c7078) {
            bool valid = s_fn_IsDerivedFrom_dispatch(
                *(void**)(selfAddr + 0x38), *s_TypeId_1401c7078);
            if (!valid)
                *(uint64_t*)(selfAddr + 0x38) = 0;
        }
    }

    // Serialize Vector3 at +0x50 (direction/target)
    reinterpret_cast<Math::Vector3*>(selfAddr + 0x50)->Serialize(serializer);   // B.3.1 bridge

    // Serialize int at +0x70 (dock ID)
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(selfAddr + 0x70), nullptr);   // B.3.4 bridge

    // Serialize Vector3 array at +0x60 (count at +0x68)
    uint32_t localCount = *(uint32_t*)(selfAddr + 0x68);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(&localCount), nullptr);   // B.3.4 bridge

    // If loading, resize array
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        // B.3.2 bridge: exe 0x14000d030 IS Collection::Array<Vector3>::SetSize(n).
        GameColl::AsArray<GameColl::Vec3>((void*)(selfAddr + 0x60)).SetSize((int)localCount);
    }

    // Serialize each Vector3 in the array (12 bytes each)
    void* arrayBase = *(void**)(selfAddr + 0x60);
    if ((int)localCount > 0 && arrayBase) {
        uint8_t* ptr = (uint8_t*)arrayBase;
        for (uint32_t i = 0; i < localCount; i++) {
            reinterpret_cast<Math::Vector3*>(ptr)->Serialize(serializer);   // B.3.1 bridge
            ptr += 12; // sizeof(Vector3) = 12 bytes
        }
    }

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// Reimpl of FUN_14000d030 — resize Vector3 array (stride 0xC, align 4)
// ============================================================
// Array struct: { void* data(+0), int count(+8), int capacity(+0xc) }.
// Grows storage when newCount exceeds capacity (capacity = newCount, or
// newCount*3 on the exactly-one-more growth case), copies min(old,new)
// 12-byte (3-float) elements, zero-inits the rest, frees the old block.
// NON-STATIC so the standalone registrar can take its address.
void BoatComponent_ResizeVec3Array(void* arrayField, int newCount) {
    struct Vec3Array { uint8_t* data; int count; int capacity; };
    Vec3Array* arr = reinterpret_cast<Vec3Array*>(arrayField);

    int oldCount = arr->count;
    if (newCount == oldCount) return;

    uint8_t* oldData = arr->data;
    arr->count = newCount;

    if (arr->capacity < newCount) {
        arr->capacity = newCount;
        // exactly-one-more growth heuristic from the original
        if (newCount == oldCount + 1 && oldCount != 0)
            arr->capacity = newCount * 3;
        arr->data = reinterpret_cast<uint8_t*>(
            fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)arr->capacity * 0xC, 4));
    }

    // Number of elements carried over from the old buffer
    int keep = (oldData == arr->data) ? oldCount : 0;

    for (int i = keep; i < newCount; ++i) {
        uint8_t* dst = arr->data + (size_t)i * 0xC;
        if (i < oldCount) {
            // copy 12 bytes (3 floats) from the old buffer
            const uint8_t* src = oldData + (size_t)i * 0xC;
            *reinterpret_cast<uint32_t*>(dst + 0) = *reinterpret_cast<const uint32_t*>(src + 0);
            *reinterpret_cast<uint32_t*>(dst + 4) = *reinterpret_cast<const uint32_t*>(src + 4);
            *reinterpret_cast<uint32_t*>(dst + 8) = *reinterpret_cast<const uint32_t*>(src + 8);
        } else {
            // zero-init new elements
            *reinterpret_cast<uint64_t*>(dst + 0) = 0;
            *reinterpret_cast<uint32_t*>(dst + 8) = 0;
        }
    }

    if (oldData != arr->data && oldData != nullptr) {
        fn_MemFreeStatic(oldData);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void BoatComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    // Type IDs
    s_TypeId_ComponentEntity = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7cf0));  // 0x1401c7cf0 = ResidenceComponent (TypeInfo)
    s_TypeId_1401c7a50 = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7a50));
    s_TypeId_1401c7078 = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7078));

    s_initialized = true;

#ifdef _DEBUG
    printf("[BoatComponent] Funcs resolved: CompSerialize=%p\n",
           (void*)s_fn_Component_Serialize);
#endif
}

// ============================================================================
// ENDGAME B.3.8 MI-A — BoatComponent as a REAL class (BoatComponent.h).
//
// Before: a 48-slot hand-built `BoatComponent_LiveVtable` filled at every
// create (FillTypedVtable + InstallController), `Inert_ScalarDtor` in slot 0 (only `~Internal`), the
// ControllerLive::Dispatcher<BoatCtrlTag> table stamped into raw bytes at +0x78.
// After: the compiler's 18-slot vtable, a real constructor and destructor
// chain, the controller a member object (B38_RULES D1, D2, D6).
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/BoatComponent.h"

// ── Controller state machine (boatcomponent_states.cpp) ──────────────────────────
// BoatComponent is a state-machine component in exactly TradeComponent's shape, and the
// geometry is read off its retail vtable 0x140158f40 rather than guessed:
//   slot 16 = 0x140003730 (`XOR EAX,EAX; RET`)  => the machine starts in state 0
//   slot 17 = 0x140003720 (`LEA RAX,[RCX+0x78]`) => member at +0x78, back-pointer at +0x90
// The state table is built ONCE at static-init time, as retail's static initializer
// does (B38 D6/D7).
extern "C" void BoatCtrl_PopulateTable();
static const bool s_boatStateTableBuilt = (BoatCtrl_PopulateTable(), true);

// 0x140046800. Component() is the CALL 0x1401188b0; the stamps are the compiler's;
// +0x28..+0x68 = 0, +0x70 = -1 (0x14004685c: the route index, "no waypoint yet" —
// the route builder resets it to 0 with the first point, boatcomponent_route.cpp);
// `_controller(this)` is the member's back-pointer at +0x90. No flag OR (0x80).
BoatComponent::BoatComponent()
    : data28(0), data30(0), data38(0), data40(0), data48(0), _pad4C{}, data50(0), data58(0),
      _pad5C{}, data60(0), data68(0), data70(-1), _pad74{}, _controller(this) {}

// [0] 0x140003970: ~BoatComponent (0x140046770), MemFree on &1.
Object::Base* BoatComponent::DeletingDtor(unsigned flags) {
    this->~BoatComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}
// 0x140046770: MemFree(+0x60), +0x60 = +0x68 = 0 — PORT GAP (B38 D2): the tree's
// slot 0 never freed the route array; the retail body is its own titled commit.
BoatComponent::~BoatComponent() = default;

// create 0x1400036e0: MemAlloc(0x98, 8), then the ctor.
void* BoatComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(BoatComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) BoatComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* BoatComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) BoatComponent());
}

// [3] 0x1400465d0 (the body resolves its own helpers first).
void BoatComponent::Persist(void* stream) { BoatComponent_Shutdown(this, stream); }

DEFINE_EXE_VTABLE(BoatComponent, "_ZTV13BoatComponent")
DEFINE_EXE_TYPE(BoatComponent, "ControllerComponent")
