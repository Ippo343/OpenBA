// code/Game/Action/pickuprawmaterial.cpp
// Clean C++ reimplementation of PickupRawMaterial action methods.
// Source: Game/Action/PickupRawMaterial.cpp (from profile build debug strings)
//
// PickupRawMaterial -- 0x30 bytes. Picks up a raw material from a source entity.
// Inherits: Object::Internal -> Action (0x20) -> PickupRawMaterial
// Factory: 0x140022c60, Vtable: 0x14015ff10
//
// Vtable (10 slots):
//   [0] +0x00 ScalarDtor    = (base Action)
//   [1] +0x08 VectorDtor    = (base Action, unhookable)
//   [2] +0x10 BaseDtor      = (base Action, unhookable)
//   [3] +0x18 Serialize     = 0x1400c99e0
//   [4] +0x20 AbstractImpl  = 0x1400c98d0 (picks up raw material)
//   [5] +0x28 Execute       = 0x1400c4a90 (base Action, returns 0)
//   [6] +0x30 Enter         = 0x1400c4b90 (base Action, void nop)
//   [7] +0x38 Exit          = 0x1400c4ba0 (base Action, void nop)
//   [8] +0x40 CheckEntity   = 0x1400c4a90 (base Action)
//   [9] +0x48 GetTarget     = 0x1400c9860
//
// Field layout (0x30 bytes):
//   +0x00-0x1F: Action base (vtable, Internal base, entity ptr, status byte)
//   +0x20: InternalBasePtr  sourceBuilding  (building to pick up from)
//   +0x28: InternalBasePtr  sourceEntity    (entity carrying the raw material)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "PickupRawMaterial.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "Component/coll_bridge.h"
#include <cstdio>
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h -- PickupRawMaterial-specific helpers)
// ============================================================

// FUN_1400223c0 -- FindComponent for CitizenAI-like component (DAT_1401c7c80)
// Params: (entity, retval_IBP) -- finds component by type ID DAT_1401c7c80
// Returns InternalBasePtr with found component.
typedef void* (*s_fn_FindComponent_7c80_t)(void* entity, void* retval);
static s_fn_FindComponent_7c80_t s_fn_FindComponent_7c80 = nullptr;

// FUN_14012d000 -- StorageComponent::AddItem(storage, itemType, count)
// Adds count of itemType to the storage component's inventory.
typedef void (*s_fn_Storage_AddItem_t)(void* storageComp, int64_t itemType, int count);
static s_fn_Storage_AddItem_t s_fn_Storage_AddItem = nullptr;

// FUN_1400c2ea0 -- Entity flag set (sets bit 0x400 on entity +0x40 flags, optionally exits components)
typedef void (*s_fn_EntityFlagSet_t)(void* entity);
static s_fn_EntityFlagSet_t s_fn_EntityFlagSet = nullptr;

// FUN_140003110 (DynArrayPush 8-byte elements, used to add the entity to the game
// world list) is the exe-side Collection::Array<Ptr8>::SetSize(n,fill) mono-
// morphisation -- ENDGAME B.3.2 step 2 converted its one call site below to a
// GameColl::AsArray<GameColl::Ptr8> member call, so its resolved-pointer stand-in
// is gone.

// DAT_1401c7158 -- type ID for IsDerivedFrom check on +0x20
static uint32_t* s_typeId_1401c7158 = nullptr;

static bool s_funcs_resolved = false;

// ============================================================
// Resolution -- called once from Install
// ============================================================
bool PickupRawMaterial_ResolveFuncs() {
    if (s_funcs_resolved) return true;

    s_fn_FindComponent_7c80 = reinterpret_cast<s_fn_FindComponent_7c80_t>(
        ResolveAddr(Module::Exe, 0x1400223c0));

    s_fn_Storage_AddItem = reinterpret_cast<s_fn_Storage_AddItem_t>(
        ResolveAddr(Module::Exe, 0x14012d000));

    s_fn_EntityFlagSet = reinterpret_cast<s_fn_EntityFlagSet_t>(
        ResolveAddr(Module::Exe, 0x1400c2ea0));

    s_typeId_1401c7158 = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7158));

    bool ok = s_fn_FindComponent_7c80 && s_fn_Storage_AddItem &&
              s_fn_EntityFlagSet &&
              s_typeId_1401c7158 && g_TypeId_ComponentEntity &&
              g_pGameWorldSingleton &&
              fn_IsDerivedFrom;

#ifdef _DEBUG
    printf("[PickupRawMaterial] ResolveFuncs: FC7c80=%p AddItem=%p FlagSet=%p\n",
           (void*)s_fn_FindComponent_7c80, (void*)s_fn_Storage_AddItem,
           (void*)s_fn_EntityFlagSet);
    if (!ok) printf("[PickupRawMaterial] ERROR: some function pointers not resolved!\n");
#endif

    s_funcs_resolved = ok;
    return ok;
}

// ============================================================
// PickupRawMaterial::Serialize (0x1400c99e0)
// Serializes: status(+0x18), entity(+0x20) with type check DAT_1401c7158,
//             entity(+0x28) with type check ComponentEntity
// ============================================================
void PickupRawMaterial_Serialize_Impl(PickupRawMaterial* self, void* stream) {
    uint8_t* s = (uint8_t*)self;
    alignas(8) uint8_t ser1[24];
    alignas(8) uint8_t ser2[24];

    // Serialize status byte (+0x18)
    ::new (static_cast<void*>(ser1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(*reinterpret_cast<bool*>(s + 0x18), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();   // B.3.4 bridge

    // Serialize entity refs
    ::new (static_cast<void*>(ser2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize source building (+0x20) with type check
    reinterpret_cast<Object::InternalBasePtr*>(s + 0x20)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (*(void**)(s + 0x20) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x20), *s_typeId_1401c7158)) {
            *(void**)(s + 0x20) = nullptr;
        }
    }

    // Serialize source entity (+0x28) with ComponentEntity type check
    reinterpret_cast<Object::InternalBasePtr*>(s + 0x28)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (*(void**)(s + 0x28) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x28), *g_TypeId_ComponentEntity)) {
            *(void**)(s + 0x28) = nullptr;
        }
    }

    reinterpret_cast<IO::Serializer*>(ser2)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// PickupRawMaterial::AbstractImpl (0x1400c98d0) -- slot 4
// Picks up raw material from source entity (+0x28):
// 1. Finds RawMaterial description component on source entity's component array
// 2. Gets quantity (+0x28 on that component) and item type
// 3. Finds storage on source building (+0x20) -> parent (+0x10)
// 4. Adds item to storage via FUN_14012d000
// 5. Checks entity flags, marks entity for removal if needed
// Returns 0 (action complete).
// ============================================================
int PickupRawMaterial_AbstractImpl_Impl(PickupRawMaterial* self, void* actionStack) {
    PickupRawMaterial_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    (void)actionStack;
    uint8_t* s = (uint8_t*)self;

    // Get source entity at +0x28
    void* sourceEntity = *(void**)(s + 0x28);

    // Find RawMaterial description component on source entity via FUN_1400223c0
    // This is a FindComponent variant using DAT_1401c7c80 type ID
    uint8_t resultIBP[16]; // InternalBasePtr result
    s_fn_FindComponent_7c80((void*)(uintptr_t)sourceEntity, resultIBP);

    // CheckPtr on the result to resolve lazy pointer

    // Read quantity from the found component: *(int*)(component + 0x28)
    void* rawMatDesc = *(void**)resultIBP;
    int quantity = *(int*)((uintptr_t)rawMatDesc + 0x28);

    // Read raw material type from source entity (+0x28 field = ExternalPtr)
    int64_t itemType = *(int64_t*)((uintptr_t)(*(void**)(s + 0x28)) + 0x28);

    // Navigate to source building (+0x20) -> parent entity (+0x10)
    void* building = *(void**)(s + 0x20);
    void* parentEntity = *(void**)((uintptr_t)building + 0x10);

    // Find StorageComponent on parent entity
    uintptr_t storageIBP = 0;
    fn_FindComponent_Storage((void*)parentEntity, &storageIBP);

    // CheckPtr + add item to storage
    s_fn_Storage_AddItem((void*)storageIBP, itemType, quantity);

    // Get source entity again, check if it should be flagged for removal
    void* entityRef = *(void**)(s + 0x28);
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t worldList = *(uintptr_t*)(gameWorld + 0xe8);

    if (entityRef != nullptr) {
        // Store entity ptr for flag check
        uintptr_t localEntity = (uintptr_t)entityRef;

        // Check bit 10 of flags at +0x40
        uint32_t flags = *(uint32_t*)(localEntity + 0x40);
        if (((flags >> 10) & 1) == 0) {
            // Entity not yet flagged -- call entity flag set and add to world removal list
            s_fn_EntityFlagSet((void*)localEntity);

            // Push entity ref to world list at worldList + 0xd0.
            // B.3.2 bridge: FUN_140003110 == Collection::Array<Ptr8>::SetSize(n,fill)
            // (3-arg form; ActHunt_DynArraySetCount_3110 in action_base_hunt.cpp).
            int currentCount = *(int*)(worldList + 0xd8);
            GameColl::AsArray<GameColl::Ptr8>((void*)(worldList + 0xd0))
                .SetSize(currentCount + 1,
                         *reinterpret_cast<const GameColl::Ptr8*>(&localEntity));
        }
    }

    return 0;
}

// ============================================================
// PickupRawMaterial::GetTarget (0x1400c9860) -- slot 9
// Returns true if param matches entity at +0x28, or
// matches the parent entity (+0x10) of the building at +0x20.
// ============================================================
bool PickupRawMaterial_GetTarget_Impl(PickupRawMaterial* self, void* param) {
    PickupRawMaterial_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    uint8_t* s = (uint8_t*)self;

    // Check direct match with +0x28
    uintptr_t paramVal = *(uintptr_t*)param;

    if (*(uintptr_t*)(s + 0x28) == paramVal) {
        return true;
    }

    // Check match with +0x20's parent (+0x10)
    uintptr_t building = *(uintptr_t*)(s + 0x20);
    uintptr_t parentEntity = *(uintptr_t*)(building + 0x10);
    return parentEntity == *(uintptr_t*)param;
}

// ============================================================
// class PickupRawMaterial (PickupRawMaterial.h) — ENDGAME B.3.8 ACT.
// The hand-built g_PickupRawMaterial_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

// create 0x140022c60 (inlined ctor). The tree's factory zeroed every field above
// +0x10; kept (not a behaviour change of this step).
PickupRawMaterial::PickupRawMaterial()
    : _field20(nullptr),
      _field28(nullptr) {}

// [0] 0x140022cd0
Object::Base* PickupRawMaterial::DeletingDtor(unsigned flags) {
    this->~PickupRawMaterial();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [3] 0x1400c99e0
void PickupRawMaterial::Persist(void* stream) { PickupRawMaterial_Serialize_Impl(this, stream); }
// [4] 0x1400c98d0
int  PickupRawMaterial::AbstractImpl(void* actionStack) { return PickupRawMaterial_AbstractImpl_Impl(this, actionStack); }
// [9] 0x1400c9860
bool PickupRawMaterial::GetTarget(void* param) { return PickupRawMaterial_GetTarget_Impl(this, param); }

void* PickupRawMaterial::CreateNewObject() {
    PickupRawMaterial_ResolveFuncs();
    void* p = Object::detail::InternalMemAlloc(sizeof(PickupRawMaterial), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) PickupRawMaterial());
}
void* PickupRawMaterial::CreateInPlaceObject(void* storage) {
    PickupRawMaterial_ResolveFuncs();
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) PickupRawMaterial());
}

DEFINE_EXE_VTABLE(PickupRawMaterial, "_ZTV17PickupRawMaterial")

// B38 D5: 0x14015ff10 is still read back BY ADDRESS outside this block
// (work_base_actionctor.cpp:164),
// so the row stays, serving the compiler table (every such reader builds the
// same bytes the ctor builds: Internal(), +0x10/+0x18 zeroed, the fields).
extern "C" void RegisterPickupRawMaterialVtable() {
    RegisterExeAddr(0x14015ff10, reinterpret_cast<uintptr_t>(PickupRawMaterial::Vtable()));
}

DEFINE_EXE_TYPE(PickupRawMaterial, "Action")
