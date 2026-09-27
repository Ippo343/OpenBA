// code/Game/Action/createnaturalresource.cpp
// Clean C++ reimplementation of CreateNaturalResource action methods.
// Source: Game/Action/CreateNaturalResource.cpp (from profile build debug strings)
//
// CreateNaturalResource -- 0x38 bytes. Creates a natural resource entity in the world.
// Inherits: Object::Internal -> Action (0x20) -> CreateNaturalResource
// Vtable at 0x14015fca0 (10 slots).
//
// Decompiled from Application-x64.exe using Ghidra.

#include "CreateNaturalResource.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (diagnostic below)
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include "Component/GrowthComponent.h"   // ENDGAME B.3.8 CB3: GrowthComponent::TypeId()
#include "Entity/ComponentEntity.h"

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h -- CreateNaturalResource-specific helpers)
// ============================================================

// FUN_140002340 -- serialize StringHash/type ref
static void (*s_fn_SerializeTypeRef)(void* field, void* serializer) = nullptr;

// ComponentEntity_CopyCtor (0x1400c3720) -- clone entity from description
static void* (*s_fn_ComponentEntity_CopyCtor)(void* self, void* desc) = nullptr;

// CreateNaturalResource_helper_cb00 (0x14009cb00) -- check terrain validity
static uint64_t (*s_fn_CheckTerrain)(void* mapGrid, int cellIndex) = nullptr;

// FUN_1400052c0 -- find component by type (returns IBP; member ABI: RCX=entity, RDX=retbuf)
static void* (*s_fn_FindComponent_Model)(int64_t entity, void* retbuf) = nullptr;

// Entity_impl_6540 (0x140126540) -- set up model at cell
static void (*s_fn_SetupModelAtCell)(int64_t comp, int cellIndex) = nullptr;

// FUN_14000c460 -- find component (GrowthComponent?)
static void* (*s_fn_FindComponent_Growth)(int64_t entity, void* retbuf) = nullptr;

// FUN_140120db0 -- set growth progress
static void (*s_fn_SetGrowthProgress)(int64_t comp, float progress) = nullptr;

// Entity_ClearPathQueue (0x140003f60) -- find component (NaturalResourceComponent?)
static void* (*s_fn_FindComponent_NR)(int64_t entity, void* retbuf) = nullptr;

// CreateNaturalResource_helper_52f0 (0x1401252f0) -- get NR variant count
static uint32_t (*s_fn_NR_GetVariantCount)(int64_t comp) = nullptr;

// FUN_140125320 -- get NR sub-variant count
static uint32_t (*s_fn_NR_GetSubVariantCount)(int64_t comp, int variant) = nullptr;

// FUN_1401253d0 -- set NR variant
static void (*s_fn_NR_SetVariant)(int64_t* comp, uint16_t variant, int16_t subVariant) = nullptr;

// FUN_1400c48a0 -- register entity with entity list
static void (*s_fn_RegisterEntity)(void** entityList, void* entityIBP, int typeGroup) = nullptr;

static bool s_cnr_funcs_resolved = false;

// ============================================================
// Resolution -- called once from Install
// ============================================================
bool CreateNaturalResource_ResolveFuncs() {
    if (s_cnr_funcs_resolved) return true;

    s_fn_SerializeTypeRef = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x140002340));

    s_fn_ComponentEntity_CopyCtor = reinterpret_cast<void*(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x1400c3720));

    s_fn_CheckTerrain = reinterpret_cast<uint64_t(*)(void*, int)>(
        ResolveAddr(Module::Exe, 0x14009cb00));

    s_fn_FindComponent_Model = reinterpret_cast<void*(*)(int64_t, void*)>(
        ResolveAddr(Module::Exe, 0x1400052c0));

    s_fn_SetupModelAtCell = reinterpret_cast<void(*)(int64_t, int)>(
        ResolveAddr(Module::Exe, 0x140126540));

    s_fn_FindComponent_Growth = reinterpret_cast<void*(*)(int64_t, void*)>(
        ResolveAddr(Module::Exe, 0x14000c460));

    s_fn_SetGrowthProgress = reinterpret_cast<void(*)(int64_t, float)>(
        ResolveAddr(Module::Exe, 0x140120db0));

    s_fn_FindComponent_NR = reinterpret_cast<void*(*)(int64_t, void*)>(
        ResolveAddr(Module::Exe, 0x140003f60));

    s_fn_NR_GetVariantCount = reinterpret_cast<uint32_t(*)(int64_t)>(
        ResolveAddr(Module::Exe, 0x1401252f0));

    s_fn_NR_GetSubVariantCount = reinterpret_cast<uint32_t(*)(int64_t, int)>(
        ResolveAddr(Module::Exe, 0x140125320));

    s_fn_NR_SetVariant = reinterpret_cast<void(*)(int64_t*, uint16_t, int16_t)>(
        ResolveAddr(Module::Exe, 0x1401253d0));

    s_fn_RegisterEntity = reinterpret_cast<void(*)(void**, void*, int)>(
        ResolveAddr(Module::Exe, 0x1400c48a0));

    bool ok = s_fn_SerializeTypeRef && s_fn_ComponentEntity_CopyCtor &&
              s_fn_CheckTerrain && s_fn_FindComponent_Model &&
              s_fn_SetupModelAtCell && s_fn_FindComponent_Growth &&
              s_fn_SetGrowthProgress && s_fn_FindComponent_NR &&
              s_fn_NR_GetVariantCount && s_fn_NR_GetSubVariantCount &&
              s_fn_NR_SetVariant && s_fn_RegisterEntity &&
              fn_InternalBasePtr_DeletePtr &&
              fn_MemFreeStatic &&
              fn_InternalBasePtr_Ctor && fn_IsDerivedFrom &&
              g_pGameWorldSingleton && fn_MemAlloc && g_pMemAllocSingleton;

#ifdef _DEBUG
    printf("[CreateNaturalResource] ResolveFuncs: SerTypeRef=%p CopyCtor=%p CheckTerrain=%p\n",
           (void*)s_fn_SerializeTypeRef, (void*)s_fn_ComponentEntity_CopyCtor,
           (void*)s_fn_CheckTerrain);
    printf("[CreateNaturalResource]   FindModel=%p SetupModel=%p FindGrowth=%p SetGrowth=%p\n",
           (void*)s_fn_FindComponent_Model, (void*)s_fn_SetupModelAtCell,
           (void*)s_fn_FindComponent_Growth, (void*)s_fn_SetGrowthProgress);
    printf("[CreateNaturalResource]   FindNR=%p NRVarCount=%p NRSubVar=%p NRSetVar=%p\n",
           (void*)s_fn_FindComponent_NR, (void*)s_fn_NR_GetVariantCount,
           (void*)s_fn_NR_GetSubVariantCount, (void*)s_fn_NR_SetVariant);
    printf("[CreateNaturalResource]   Register=%p\n",
           (void*)s_fn_RegisterEntity);
    if (!ok) printf("[CreateNaturalResource] ERROR: some function pointers not resolved!\n");
#endif

    s_cnr_funcs_resolved = ok;
    return ok;
}

// ============================================================
// Serialize (0x1400c6890)
// Serializes: status(+0x18), typeRef(+0x20), int(+0x30), versioned entity(+0x28)
// ============================================================
void CreateNaturalResource::Persist(void* stream) {
    // Lazy-resolve like every other method on this exe-ctor-built action. Serialize
    // was the one that never got it, so a save taken while one of these actions was
    // in flight called a null serializer pointer (EXEC of 0). Found live on
    // CompleteRoad once roads started being built; these four are the same shape.
    CreateNaturalResource_ResolveFuncs();
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    alignas(8) uint8_t ser1[24];
    alignas(8) uint8_t ser2[24];

    // Serialize status (version 0)
    ::new (static_cast<void*>(ser1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(_status, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();   // B.3.4 bridge

    // Serialize resource data (version 1)
    ::new (static_cast<void*>(ser2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge
    s_fn_SerializeTypeRef(s + 0x20, ser2);
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(s + 0x30), nullptr);   // B.3.4 bridge

    int version = reinterpret_cast<IO::Serializer*>(ser2)->GetVersion();   // B.3.4 bridge
    if (version > 0) {
        reinterpret_cast<Object::InternalBasePtr*>(s + 0x28)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
        if (parentEntity != nullptr) {
            if (!fn_IsDerivedFrom(parentEntity, ComponentEntity::TypeId())) {
                parentEntity = nullptr;
            }
        }
    }
    reinterpret_cast<IO::Serializer*>(ser2)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// AbstractImpl (0x1400c6650)
// Creates a natural resource entity in the world.
// Checks if the map cell is available, allocates entity from description,
// sets up model/NR/growth components, randomizes variant, registers entity.
// Returns 0 on success, 2 on failure (cell occupied or invalid).
// ============================================================
int  CreateNaturalResource::AbstractImpl(void* actionStack) {
    CreateNaturalResource_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    (void)actionStack;
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t mapGrid = *(uintptr_t*)(gameWorld + 0xf8);

    const int32_t cellIndex = this->cellIndex;
    uintptr_t cellArrayBase = *(uintptr_t*)(mapGrid + 0x80);

    // The four early-outs below all return the same failure code, so a caller can only see
    // "no resource was created", never why. That matters here: the orchard plant chain sends
    // a bind message immediately after this action and dereferences the entity this action
    // was supposed to leave on the tile, so a silent bail surfaces as a null deref two frames
    // later in a different subsystem. Name the branch, a few times per run.
    static int s_bailReported = 0;
    #define CNR_BAIL(why) do {                                                      \
        if (s_bailReported++ < 4) {                                                 \
            std::fprintf(stderr, "[CreateNR] cell %d: %s — no resource created\n",  \
                         cellIndex, (why));                                         \
            std::fflush(stderr);                                                    \
        }                                                                           \
        return 2;                                                                   \
    } while (0)

    // Check if the cell already has an entity at the structure slot (+8)
    int64_t existingEntity = *(int64_t*)(cellArrayBase + 8 + (int64_t)cellIndex * 0x30);
    if (existingEntity != 0) {
        CNR_BAIL("tile already holds an entity");
    }

    // Check if the cell's owner matches our parent entity
    int64_t cellOwner = *(int64_t*)(cellArrayBase + (int64_t)cellIndex * 0x30);
    if (cellOwner != *(int64_t*)(s + 0x28)) {
        CNR_BAIL("tile owner != the action's parent entity");
    }

    // Check terrain validity for this cell
    uint64_t terrainResult = s_fn_CheckTerrain((void*)mapGrid, cellIndex);
    if ((char)terrainResult != '\0') {
        CNR_BAIL("terrain check says the cell is unavailable");
    }

    // Check terrain byte flag
    if (*(char*)(cellArrayBase + (int64_t)cellIndex * 0x30 + 0x28) != '\0') {
        CNR_BAIL("tile status byte (+0x28) is set");
    }

    // Allocate a new ComponentEntity copied from description at +0x20
    void* desc = descEntity;
    void* newEntityMem = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
    void* newEntity = nullptr;
    if (newEntityMem != nullptr) {
        newEntity = s_fn_ComponentEntity_CopyCtor(newEntityMem, desc);
    }

    // Wrap in InternalBasePtr
    int64_t entityIBP = 0;
    new (&entityIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(newEntity));   // B.3.3 bridge

    // Find and set up ModelComponent -- place at cell
    void* modelCompIBP = nullptr;
    s_fn_FindComponent_Model(entityIBP, &modelCompIBP);
    s_fn_SetupModelAtCell(*(int64_t*)&modelCompIBP, cellIndex);

    // Find and set up GrowthComponent -- set progress to 0.0
    void* growthCompIBP = nullptr;
    s_fn_FindComponent_Growth(entityIBP, &growthCompIBP);
    // Retail derefs both finder results unguarded, and it is right to: the description this
    // action carries is always a natural-resource template that HAS both components. When one
    // comes back null the description (or the entity copy) is wrong upstream, and the crash
    // alone cannot say which. Report the whole chain once, then let the faithful deref stand.
    if (!modelCompIBP || !growthCompIBP || !newEntity) {
        static bool s_reported = false;
        if (!s_reported) {
            s_reported = true;
            std::fprintf(stderr, "[CreateNR] MISSING COMPONENT on the spawned resource: "
                         "desc=%p descTypeGroup=%d cell=%d entityMem=%p newEntity=%p "
                         "modelComp=%p growthComp=%p\n",
                         desc, desc ? *(int*)((uintptr_t)desc + 0x20) : -1, cellIndex,
                         newEntityMem, newEntity, modelCompIBP, growthCompIBP);
            // Two very different causes look identical from here: the TEMPLATE genuinely has
            // no GrowthComponent, or the finder's type-id .rdata slot is unregistered/zero so
            // it misses on every entity (reference_registered_but_filled_with_zero). Separate
            // them by re-scanning with the by-NAME id, which does not depend on that slot.
            const uint32_t tidByName = GrowthComponent::TypeId();
            int compN = newEntity ? *(int32_t*)((uint8_t*)newEntity + 0x38) : 0;
            uintptr_t compA = newEntity ? *(uintptr_t*)((uint8_t*)newEntity + 0x30) : 0;
            int growthByName = 0;
            for (int ci = 0; ci < compN && compA; ++ci) {
                void* c = *(void**)(compA + (uintptr_t)ci * 8);
                if (!c) continue;
                uint32_t t = (*(uint32_t(**)(void*))(*(uintptr_t*)c + 0x10))(c);
                if (tidByName && t == tidByName) ++growthByName;
            }
            std::fprintf(stderr, "[CreateNR]   GrowthComponent::TypeId()=%u | components on entity=%d, "
                         "matching by NAME=%d  => %s\n",
                         tidByName, compN, growthByName,
                         growthByName > 0 ? "THE TEMPLATE HAS ONE — the finder's type-id slot is "
                                            "the bug" : "the template genuinely has no "
                                            "GrowthComponent — look upstream at the description");
            std::fflush(stderr);
        }
    }
    s_fn_SetGrowthProgress(*(int64_t*)&growthCompIBP, 0.0f);

    // Find NaturalResourceComponent and randomize variant
    void* randomPtr = *(void**)(gameWorld + 0xd8);

    void* nrCompIBP1 = nullptr;
    s_fn_FindComponent_NR(entityIBP, &nrCompIBP1);
    uint32_t variantCount = s_fn_NR_GetVariantCount(*(int64_t*)&nrCompIBP1);
    uint32_t variant = reinterpret_cast<System::Random*>(randomPtr)->Rand(static_cast<uint32_t>(variantCount));   // B.3.4b bridge

    void* nrCompIBP2 = nullptr;
    s_fn_FindComponent_NR(entityIBP, &nrCompIBP2);
    int64_t nrComp = *(int64_t*)&nrCompIBP2;

    void* nrCompIBP3 = nullptr;
    s_fn_FindComponent_NR(entityIBP, &nrCompIBP3);
    int64_t* nrCompPtr = *(int64_t**)&nrCompIBP3;

    uint32_t subVariantCount = s_fn_NR_GetSubVariantCount(nrComp, (int)variant);
    uint32_t subVariant = reinterpret_cast<System::Random*>(randomPtr)->Rand(static_cast<uint32_t>(subVariantCount));   // B.3.4b bridge
    s_fn_NR_SetVariant(nrCompPtr, (uint16_t)variant, (int16_t)subVariant);

    // Register entity with the entity list
    void* entityListPtr = *(void**)(gameWorld + 0xe8);
    void* entityRef = (void*)entityIBP;
    int typeGroup = *(int*)(*(uintptr_t*)(s + 0x20) + 0x20);
    s_fn_RegisterEntity((void**)entityListPtr, &entityRef, typeGroup);



    return 0;
}

// ============================================================
// GetTarget (0x1400c6610)
// Returns true if param matches entity at +0x28.
// ============================================================
bool CreateNaturalResource::GetTarget(void* param) {
    CreateNaturalResource_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    return (*(void**)param == parentEntity) ? 1 : 0;
}

// ============================================================
// class CreateNaturalResource (CreateNaturalResource.h) — ENDGAME B.3.8 ACT.
// The hand-built g_CreateNaturalResource_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

// create 0x1400221d0 (inlined ctor). The tree's factory zeroed every field above
// +0x10; kept (not a behaviour change of this step).
CreateNaturalResource::CreateNaturalResource()
    : descEntity(nullptr),
      parentEntity(nullptr),
      cellIndex(0),
      _pad34{} {}

// [0] 0x140022240
Object::Base* CreateNaturalResource::DeletingDtor(unsigned flags) {
    this->~CreateNaturalResource();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [3] 0x1400c6890
// [4] 0x1400c6650
// [9] 0x1400c6610

void* CreateNaturalResource::CreateNewObject() {
    CreateNaturalResource_ResolveFuncs();
    void* p = Object::detail::InternalMemAlloc(sizeof(CreateNaturalResource), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CreateNaturalResource());
}
void* CreateNaturalResource::CreateInPlaceObject(void* storage) {
    CreateNaturalResource_ResolveFuncs();
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CreateNaturalResource());
}

DEFINE_EXE_VTABLE(CreateNaturalResource, "_ZTV21CreateNaturalResource")

DEFINE_EXE_TYPE(CreateNaturalResource, "Action")
