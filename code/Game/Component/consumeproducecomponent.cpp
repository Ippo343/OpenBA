// code/Game/Component/consumeproducecomponent.cpp
// Clean reimplementation of ConsumeProduceComponent lifecycle methods from Application-x64.exe.
//
// ConsumeProduceComponent extends ResourceProduceComponent (0x70 bytes).
// Two non-trivial vtable overrides:
//   Shutdown  (0x140118e00) — parent Shutdown + versioned serialization of consume fields
//   Serialize (0x140118ef0) — init consume data from building template
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/consumeproducecomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include "io/Stream.h"      // ENDGAME B.3.4 callers batch H: IO::Stream::GetDirection
#include <cstdint>
#include <cstdio>
#include <new>

// ============================================================
// Resolved helpers
// ============================================================

// FUN_140023a00 IS Collection::Array<float>::SetSize (ENDGAME B.3.2 step 2), but
// with a derived row that DEVIATES from the header's zero-tail assumption: the
// ported body (ConsumeProduceComponent_ResizeArray below) has NO else-arm write
// for the new tail at all — it leaves the grown slots holding whatever MemAlloc
// returned. That is exactly SpecFor<float>'s behaviour for a trivially
// default-constructible T (init == nullptr), so the call sites below go through
// GameColl::AsArray<float> (a bare scalar, not GameColl::F32 which would zero).
// Both resolved-pointer names below pointed at this same address and are gone;
// the function is KEPT (still registered by reg_component_economy.cpp for
// other callers) and this file's own two call sites now call it directly.

// DWORD_1401c7388 — ConsumeProduceDescription type ID
static uint32_t* s_TypeId_ConsumeProduceDescription = nullptr;

// fn_IsDerivedFrom — dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    uintptr_t slot;
    // ConsumeProduceDescription type ID at 0x1401c7388
    s_TypeId_ConsumeProduceDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7388));

    // IsDerivedFrom via dispatch slot
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    s_initialized = true;
}

// ============================================================
// ConsumeProduceComponent::Shutdown (vtable slot [3], 0x140118e00)
// ============================================================
// Calls parent ResourceProduceComponent_Shutdown, then serializes:
//   - consumeResourceId (+0x50)
//   - consumeCount (+0x60) via local copy, with array resize on load
//   - float array at consumeRates (+0x58)
//   - consumeState (+0x68)
//   - Version-0 migration: if consumeResourceId < 0 on load, reset to 0

void ConsumeProduceComponent_Shutdown(ConsumeProduceComponent* self, void* stream) {
    EnsureHelpers();

    // Base ResourceProduceComponent::Shutdown (0x14012b4a0), a direct call in retail.
    self->ResourceProduceComponent::Persist(stream);

    // Create versioned serializer (version 1)
    alignas(8) uint8_t serBuf[24];
    int64_t* pSerData = (int64_t*)serBuf;
    ::new (static_cast<void*>(serBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge

    // Serialize consumeResourceId at +0x50
    reinterpret_cast<IO::Serializer*>(serBuf)->Serialize(self->consumeResourceId, nullptr);   // B.3.4 bridge

    // Copy consume count to local (original uses local_res8, NOT direct field access).
    // On load, the resize function reads +0x60 internally as old count to decide
    // whether reallocation is needed. Writing directly to +0x60 before resize would
    // make old == new, causing resize to skip reallocation.
    int localCount = self->consumeCount;
    reinterpret_cast<IO::Serializer*>(serBuf)->Serialize(localCount, nullptr);   // B.3.4 bridge

    // On load (direction == 0): resize array if needed
    if (*(int*)(pSerData[0] + 8) == 0) {
        // B.3.2 bridge: exe 0x140023a00 IS Collection::Array<float>::SetSize(n)
        // (bare float — see the derived-row note above the typedefs).
        GameColl::AsArray<float>(&self->consumeRates).SetSize(localCount);
    }

    // Serialize float array using local count and rates pointer
    reinterpret_cast<IO::Serializer*>(serBuf)->Serialize(localCount, self->consumeRates, nullptr);   // B.3.4 bridge

    // Serialize consumeState at +0x68
    reinterpret_cast<IO::Serializer*>(serBuf)->Serialize(self->consumeState, nullptr);   // B.3.4 bridge

    // Version-0 migration: on load, if consumeResourceId < 0, reset to 0
    int version = reinterpret_cast<IO::Serializer*>(serBuf)->GetVersion();   // B.3.4 bridge
    if (version == 0 && self->consumeResourceId < 0) {
        // Check if we're loading (stream direction == 0)
        if (static_cast<int>(reinterpret_cast<IO::Stream*>(stream)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Stream::GetDirection, retail 0x180006200)
            self->consumeResourceId = 0;
        }
    }

    reinterpret_cast<IO::Serializer*>(serBuf)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// ConsumeProduceComponent::Serialize (vtable slot [4], 0x140118ef0)
// ============================================================
// Looks up entity's component list via owner (+0x10), iterates components
// to find one matching ConsumeProduceDescription type ID (0x1401c7388),
// then copies consume slot info (count at +0x30, resourceId at +0x4c),
// and initializes the consumeRates float array to 1.0f per slot.
//
// Original decompilation:
//   CheckPtr(self+0x10)
//   entity = *(self+0x10)
//   compList = entity->+0x28 (component list: +0x28=array, +0x30=count)
//   for i in range(compList.count):
//     comp = compList[i]
//     typeId = comp->vtable[+0x10]()  // GetTypeId virtual
//     if typeId == DWORD_1401c7388:
//       desc = compList[i]
//       if IsDerivedFrom(desc, DWORD_1401c7388): desc is valid
//       break
//   ResizeConsumeArray(self+0x58, *(desc+0x30))   // count from description
//   self->consumeResourceId = *(desc+0x4c)        // resource ID from description
//   for i in range(self->consumeCount):
//     consumeRates[i] = 1.0f

void ConsumeProduceComponent_Serialize(ConsumeProduceComponent* self) {
    EnsureHelpers();

    if (!s_fn_IsDerivedFrom_dispatch || !s_TypeId_ConsumeProduceDescription)
        return;

    void* entity = self->_entity;
    if (!entity) return;

    // Walk the entity DESCRIPTION's child list, not the component array.
    //
    // ComponentEntity keeps its description at +0x28 and its COMPONENT array at +0x30
    // (count +0x38); a description keeps its CHILD list at +0x28 with the count at
    // +0x30. What is being searched for here is ConsumeProduce*Description*, so retail
    // costs one extra deref: 0x140118ef0 does `RBP = *(entity+0x28)` and only then
    // reads [RBP+0x30] as the count and [RBP+0x28] as the array. Its slot-18 sibling
    // (0x140118d00) does exactly the same.
    //
    // This read the two fields straight off the entity, so `compCount` was the low half
    // of the component-array POINTER (a huge positive number) and `compArray` was the
    // description object -- the very first iteration would have called GetTypeId on the
    // description's own vtable pointer. It never faulted only because slot 4 was wired
    // nowhere and this whole body was dead. See the two-lists note in the wave memo.
    uintptr_t entityAddr = (uintptr_t)entity;
    uintptr_t descRoot = *(uintptr_t*)(entityAddr + 0x28);
    if (!descRoot) return;
    void** compArray = *(void***)(descRoot + 0x28);
    int compCount = *(int*)(descRoot + 0x30);

    // Find ConsumeProduceDescription in component list
    void* desc = nullptr;
    uint32_t targetTypeId = *s_TypeId_ConsumeProduceDescription;

    for (int i = 0; i < compCount; i++) {
        void* comp = compArray[i];
        if (!comp) continue;

        // Call GetTypeId virtual at vtable+0x10
        uintptr_t vtable = *(uintptr_t*)comp;
        typedef uint32_t (*fn_GetTypeId_t)(void*);
        fn_GetTypeId_t getTypeId = *(fn_GetTypeId_t*)(vtable + 0x10);
        uint32_t typeId = getTypeId(comp);

        if (typeId == targetTypeId) {
            // Verify with IsDerivedFrom
            if (s_fn_IsDerivedFrom_dispatch(comp, targetTypeId)) {
                desc = comp;
            }
            break;
        }
    }

    if (!desc) return;

    // Resize consume array from description count at +0x30
    int descCount = *(int*)((uintptr_t)desc + 0x30);
    // B.3.2 bridge: exe 0x140023a00 IS Collection::Array<float>::SetSize(n).
    GameColl::AsArray<float>((void*)((uintptr_t)self + 0x58)).SetSize(descCount);

    // Copy resource ID from description at +0x4c
    self->consumeResourceId = *(int*)((uintptr_t)desc + 0x4c);

    // Initialize all consume rates to 1.0f
    float* rates = self->consumeRates;
    if (rates && self->consumeCount > 0) {
        for (int i = 0; i < self->consumeCount; i++) {
            rates[i] = 1.0f;
        }
    }
}

// ============================================================
// Reimpl of FUN_140023a00 (ConsumeProduceComponent_sub_3a00:
//   grow/shrink a 4-byte-element (int/float) array; copies only the overlap,
//   new tail elements are left for the caller to initialise)
// ============================================================
void ConsumeProduceComponent_ResizeArray(void* arrayFieldPtr, int newCount) {
    int64_t* field = (int64_t*)arrayFieldPtr;   // [0]=ptr, [1] low=count, high=capacity
    int oldCount = (int)field[1];
    if (newCount == oldCount) return;

    int32_t* oldBuf = (int32_t*)field[0];
    *(int*)((uintptr_t)field + 8) = newCount;
    if (*(int*)((uintptr_t)field + 0xc) < newCount) {
        *(int*)((uintptr_t)field + 0xc) = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            *(int*)((uintptr_t)field + 0xc) = newCount * 3;
        }
        void* nb = fn_MemAlloc(g_pMemAllocSingleton,
                               (uint64_t)((int64_t)*(int*)((uintptr_t)field + 0xc) << 2), 4);
        field[0] = (int64_t)nb;
    }

    int start = (oldBuf == (int32_t*)field[0]) ? oldCount : 0;
    int32_t* dst = (int32_t*)field[0];
    for (int i = start; i < newCount; i++) {
        if (i < oldCount && (dst + i) != nullptr) {
            dst[i] = oldBuf[i];
        }
    }

    if (oldBuf != (int32_t*)field[0]) {
        if (oldBuf) fn_MemFreeStatic(oldBuf);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void ConsumeProduceComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[ConsumeProduceComponent] Funcs resolved: TypeId=%p IsDerived=%p\n",
           (void*)s_TypeId_ConsumeProduceDescription,
           (void*)s_fn_IsDerivedFrom_dispatch);
#endif
}

// ============================================================================
// ENDGAME B.3.8 MI-B — ConsumeProduceComponent as a REAL class
// (ConsumeProduceComponent.h).
//
// Before: CONTROLLER_LIVE_FACTORY_STATES (48-slot table filled per create, slots
// 3/4/13/14/18 written from InstallLive, Inert_ScalarDtor in slot 0, the shared RP
// dispatcher stamped at +0x28). After: the compiler's 22-slot vtable (13/14 — the
// panel "work" checkbox pair — 16/17 and 19-21 inherited from
// ResourceProduceComponent, which is what retail's table holds), a real ctor/dtor
// chain, the controller the base's member (B38_RULES D1, D2, D6).
// ============================================================================
#include "Object/factory_registration.h"

// Slot 18 (+0x90) CreateWork -- retail 0x140118d00, ported in
// resourceproduce_creatework.cpp alongside its five siblings.
extern "C" uintptr_t* ConsumeProduceComponent_CreateWork(void*, uintptr_t*);

// 0x140119050: the RP ctor (CALL 0x14012b3d0), the compiler's stamp, then
// +0x50 = -1, +0x58 = +0x60 = 0 (qwords), +0x68 = 0, `_flags |= 0x80` (already
// set by Component()). +0x50 = -1 is "no recipe selected" until Startup (slot 4)
// reads the description's +0x4c; the tree's factory zero-filled it (recipe 0).
ConsumeProduceComponent::ConsumeProduceComponent()
    : ResourceProduceComponent(), consumeResourceId(-1), _pad54{}, consumeRates(nullptr),
      consumeCount(0), consumeCapacity(0), consumeState(0), _pad6C{} {
    OrFlagsInCtor(0x80);
}

// [0] 0x140032460: CALL the dtor 0x140118fd0, MemFree on &1.
Object::Base* ConsumeProduceComponent::DeletingDtor(unsigned flags) {
    this->~ConsumeProduceComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}
// 0x140118fd0: MemFree(+0x58), +0x58 = +0x60 = 0, then the restamps and ~Internal
// (the compiler chain).
ConsumeProduceComponent::~ConsumeProduceComponent() {
    Object::detail::InternalMemFree(consumeRates);
    consumeRates = nullptr;
    consumeCount = 0;
    consumeCapacity = 0;
}

// create 0x140032300: MemAlloc(0x70, 8), then the ctor.
void* ConsumeProduceComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ConsumeProduceComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ConsumeProduceComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* ConsumeProduceComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ConsumeProduceComponent());
}

// [3] 0x140118e00 (serialize) and [4] 0x140118ef0 (Startup: init the consume config
// from the building's ConsumeProduceDescription).
void       ConsumeProduceComponent::Persist(void* stream)     { ConsumeProduceComponent_Shutdown(this, stream); }
void       ConsumeProduceComponent::Startup()                 { ConsumeProduceComponent_Serialize(this); }
uintptr_t* ConsumeProduceComponent::CreateWork(uintptr_t* out) { return ConsumeProduceComponent_CreateWork(this, out); }  // [18] 0x140118d00

DEFINE_EXE_VTABLE(ConsumeProduceComponent, "_ZTV23ConsumeProduceComponent")
DEFINE_EXE_TYPE(ConsumeProduceComponent, "ResourceProduceComponent")
