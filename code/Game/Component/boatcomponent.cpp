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

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

typedef void (*fn_Serialize_Int_t)(void* serializer, int* value, const char* name);
static fn_Serialize_Int_t s_fn_Serialize_Int = nullptr;

typedef void (*fn_Serialize_Float_t)(void* serializer, float* value, const char* name);
static fn_Serialize_Float_t s_fn_Serialize_Float = nullptr;

// Object::InternalBasePtr::Serialize -- dispatch slot at 0x140152aa0
typedef void (*fn_IBP_Serialize_t)(void* ibp, void* serializer);
static fn_IBP_Serialize_t s_fn_IBP_Serialize = nullptr;

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
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        ::new (static_cast<void*>(serializer1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer1)->~Serializer();                                       // B.3.4 bridge
    }

    // Main serializer block
    alignas(8) uint8_t serializer[24];
    if (!s_fn_Serializer_Ctor) return;
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize InternalBasePtr at +0x28 (entity ref)
    if (s_fn_IBP_Serialize)
        s_fn_IBP_Serialize((void*)(selfAddr + 0x28), serializer);

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
    if (s_fn_Serialize_Float)
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(selfAddr + 0x4c), nullptr);   // B.3.4 bridge

    // Serialize InternalBasePtr at +0x30
    if (s_fn_IBP_Serialize)
        s_fn_IBP_Serialize((void*)(selfAddr + 0x30), serializer);

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
    if (s_fn_IBP_Serialize)
        s_fn_IBP_Serialize((void*)(selfAddr + 0x38), serializer);

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
    if (s_fn_Serialize_Int)
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(selfAddr + 0x70), nullptr);   // B.3.4 bridge

    // Serialize Vector3 array at +0x60 (count at +0x68)
    uint32_t localCount = *(uint32_t*)(selfAddr + 0x68);
    if (s_fn_Serialize_Int)
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(&localCount), nullptr);   // B.3.4 bridge

    // If loading, resize array
    uintptr_t serializerObj = *(uintptr_t*)serializer;
    if (*(int*)(serializerObj + 8) == 0) {
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

    if (s_fn_Serializer_Dtor)
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

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serialize_Int = *reinterpret_cast<fn_Serialize_Int_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a60);
    if (slot) s_fn_Serialize_Float = *reinterpret_cast<fn_Serialize_Float_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152aa0);
    if (slot) s_fn_IBP_Serialize = *reinterpret_cast<fn_IBP_Serialize_t*>(slot);

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
    printf("[BoatComponent] Funcs resolved: CompSerialize=%p IBP_Serialize=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_IBP_Serialize);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// ── Controller state machine (boatcomponent_states.cpp) ──────────────────────────
// BoatComponent is a state-machine component in exactly TradeComponent's shape, and the
// geometry is read off its retail vtable 0x140158f40 rather than guessed:
//   slot 16 = 0x140003730 (`XOR EAX,EAX; RET`)  => the machine starts in state 0
//   slot 17 = 0x140003720 (`LEA RAX,[RCX+0x78]`) => node at +0x78, back-pointer at +0x90
// The ctor (0x140046800) confirms both: it writes node vtable 0x140158ef0 to +0x78 and
// `this` to +0x90 (= node+0x18).
extern "C" void** BoatCtrl_NodeVtable();
extern "C" void   BoatCtrl_PopulateTable();

static uint32_t BoatComponent_LiveGetTypeId2(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("BoatComponent");
    return cached;
}
static void* BoatComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

static int   BoatComponent_GetDefaultState(void*) { return 0; }
static void* BoatComponent_CreateState(void* self) {
    return reinterpret_cast<uint8_t*>(self) + 0x78;
}
static void* BoatComponent_LiveGetTypeInfo(void*) {
    static void* cached = nullptr;
    if (!cached) cached = ::Object::GetTypeInfoByName("BoatComponent");
    return cached;
}

static void BoatComponent_ShutdownVSlot(void* self, void* stream) {
    BoatComponent_Shutdown(reinterpret_cast<BoatComponent*>(self), stream);
}

// Wire the node ON THE INSTANCE, after MakeInertTyped/InitInertTyped — FillTypedVtable
// rewrites the vtable slots and would erase anything written earlier.
// See reference_live_vtable_slot_must_be_wired_in_installlive.
static void BoatComponent_InstallController(void* mem) {
    // Slot 3 (stream Shutdown/Serialize, retail 0x1400465d0).
    BoatComponent_LiveVtable[3] = reinterpret_cast<void*>(&BoatComponent_ShutdownVSlot);
    BoatComponent_LiveVtable[1]  = reinterpret_cast<void*>(&BoatComponent_LiveGetTypeInfo);
    BoatComponent_LiveVtable[16] = reinterpret_cast<void*>(&BoatComponent_GetDefaultState);
    BoatComponent_LiveVtable[17] = reinterpret_cast<void*>(&BoatComponent_CreateState);
    BoatCtrl_PopulateTable();
    if (!mem) return;
    uint8_t* p = reinterpret_cast<uint8_t*>(mem);
    *reinterpret_cast<void**>(p + 0x78) = reinterpret_cast<void*>(BoatCtrl_NodeVtable());
    *reinterpret_cast<void**>(p + 0x90) = mem;   // node+0x18 = owning component
}

extern "C" void* BoatComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x98, 0x80,
        BoatComponent_LiveVtable,
        reinterpret_cast<void*>(&BoatComponent_LiveGetTypeId2));
    BoatComponent_InstallController(p);
    return p;
}

static void* BoatComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x98, 0x80,
        BoatComponent_LiveVtable,
        reinterpret_cast<void*>(&BoatComponent_LiveGetTypeId2));
    BoatComponent_InstallController(mem);
    return mem;
}

REGISTER_TYPE(BoatComponent, "ControllerComponent",
              &BoatComponent_Factory, &BoatComponent_CreateInPlace);
