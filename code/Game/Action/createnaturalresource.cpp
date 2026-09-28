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
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Action/exe_bodies.h"            // ActCNR_* helpers
#include "Work/exe_bodies.h"              // WorkBase_FindComponent_52c0
#include "Game/exe_bodies.h"              // Reimpl_ExternalPtr_Serialize_StringTable
#include "Entity/componententity_funcs.h" // ComponentEntity_CtorFromDescription, EntityList_RegisterEntity_Impl
#include "system/MemoryAllocator.h"       // System::MemoryAllocator::MemAlloc
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
// Helpers, called by name (B3R)
//   0x140002340 Reimpl_ExternalPtr_Serialize_StringTable -- serialize StringHash/type ref
//   0x1400c3720 ComponentEntity_CtorFromDescription      -- clone entity from description
//   0x14009cb00 ActCNR_CheckTerrain_cb00                 -- check terrain validity
//   0x1400052c0 WorkBase_FindComponent_52c0  -- find component by type (ModelComponent;
//               member ABI: RCX=entity, RDX=retbuf)
//   0x140126540 ActCNR_SetupModelAtCell_126540 (Entity_impl_6540) -- set up model at cell
//   0x14000c460 ActCNR_FindComponent_c460    -- find component (GrowthComponent)
//   0x140120db0 ActCNR_SetGrowthProgress_120db0 -- set growth progress
//   0x140003f60 ActCNR_FindComponent_3f60    -- find component (NaturalResourceComponent)
//   0x1401252f0 ActCNR_NR_GetVariantCount_1252f0 / 0x140125320 ActCNR_NR_GetSubVariantCount_125320
//   0x1401253d0 ActCNR_NR_SetVariant_1253d0  -- set NR variant
//   0x1400c48a0 EntityList_RegisterEntity_Impl -- register entity with entity list
// ============================================================

// ============================================================
// Serialize (0x1400c6890)
// Serializes: status(+0x18), typeRef(+0x20), int(+0x30), versioned entity(+0x28)
// ============================================================
void CreateNaturalResource::Persist(void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    alignas(8) uint8_t ser1[24];
    alignas(8) uint8_t ser2[24];

    // Serialize status (version 0)
    ::new (static_cast<void*>(ser1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(_status, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();   // B.3.4 bridge

    // Serialize resource data (version 1)
    ::new (static_cast<void*>(ser2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge
    Reimpl_ExternalPtr_Serialize_StringTable(s + 0x20, ser2);   // 0x140002340
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(s + 0x30), nullptr);   // B.3.4 bridge

    int version = reinterpret_cast<IO::Serializer*>(ser2)->GetVersion();   // B.3.4 bridge
    if (version > 0) {
        reinterpret_cast<Object::InternalBasePtr*>(s + 0x28)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
        if (parentEntity != nullptr) {
            if (!::Object::IsDerivedFromNullSafe(parentEntity, ComponentEntity::TypeId())) {
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
    (void)actionStack;
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    uintptr_t gameWorld = (uintptr_t)GameUpdateLoop::_singleton;
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
    uint64_t terrainResult = ActCNR_CheckTerrain_cb00((void*)mapGrid, cellIndex);   // 0x14009cb00
    if ((char)terrainResult != '\0') {
        CNR_BAIL("terrain check says the cell is unavailable");
    }

    // Check terrain byte flag
    if (*(char*)(cellArrayBase + (int64_t)cellIndex * 0x30 + 0x28) != '\0') {
        CNR_BAIL("tile status byte (+0x28) is set");
    }

    // Allocate a new ComponentEntity copied from description at +0x20
    void* desc = descEntity;
    void* newEntityMem = System::MemoryAllocator::MemAlloc(0x48, 8);
    void* newEntity = nullptr;
    if (newEntityMem != nullptr) {
        newEntity = ComponentEntity_CtorFromDescription(static_cast<ComponentEntity*>(newEntityMem), desc);   // 0x1400c3720
    }

    // Wrap in InternalBasePtr
    int64_t entityIBP = 0;
    new (&entityIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(newEntity));   // B.3.3 bridge

    // Find and set up ModelComponent -- place at cell
    void* modelCompIBP = nullptr;
    WorkBase_FindComponent_52c0(static_cast<uintptr_t>(entityIBP), &modelCompIBP);   // 0x1400052c0
    ActCNR_SetupModelAtCell_126540(*(int64_t*)&modelCompIBP, cellIndex);   // 0x140126540

    // Find and set up GrowthComponent -- set progress to 0.0
    void* growthCompIBP = nullptr;
    ActCNR_FindComponent_c460(static_cast<uintptr_t>(entityIBP), &growthCompIBP);   // 0x14000c460
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
                uint32_t t = reinterpret_cast<Object::Base*>(c)->Type();   // B.3.x bridge (slot 2)
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
    ActCNR_SetGrowthProgress_120db0(*(int64_t*)&growthCompIBP, 0.0f);   // 0x140120db0

    // Find NaturalResourceComponent and randomize variant
    void* randomPtr = *(void**)(gameWorld + 0xd8);

    void* nrCompIBP1 = nullptr;
    ActCNR_FindComponent_3f60(static_cast<uintptr_t>(entityIBP), &nrCompIBP1);   // 0x140003f60
    uint32_t variantCount = ActCNR_NR_GetVariantCount_1252f0(*(int64_t*)&nrCompIBP1);   // 0x1401252f0
    uint32_t variant = reinterpret_cast<System::Random*>(randomPtr)->Rand(static_cast<uint32_t>(variantCount));   // B.3.4b bridge

    void* nrCompIBP2 = nullptr;
    ActCNR_FindComponent_3f60(static_cast<uintptr_t>(entityIBP), &nrCompIBP2);   // 0x140003f60
    int64_t nrComp = *(int64_t*)&nrCompIBP2;

    void* nrCompIBP3 = nullptr;
    ActCNR_FindComponent_3f60(static_cast<uintptr_t>(entityIBP), &nrCompIBP3);   // 0x140003f60
    int64_t* nrCompPtr = *(int64_t**)&nrCompIBP3;

    uint32_t subVariantCount = ActCNR_NR_GetSubVariantCount_125320(nrComp, (int)variant);   // 0x140125320
    uint32_t subVariant = reinterpret_cast<System::Random*>(randomPtr)->Rand(static_cast<uint32_t>(subVariantCount));   // B.3.4b bridge
    ActCNR_NR_SetVariant_1253d0(nrCompPtr, (uint16_t)variant, (int16_t)subVariant);   // 0x1401253d0

    // Register entity with the entity list
    void* entityListPtr = *(void**)(gameWorld + 0xe8);
    void* entityRef = (void*)entityIBP;
    int typeGroup = *(int*)(*(uintptr_t*)(s + 0x20) + 0x20);
    EntityList_RegisterEntity_Impl(entityListPtr, &entityRef, typeGroup);   // 0x1400c48a0



    return 0;
}

// ============================================================
// GetTarget (0x1400c6610)
// Returns true if param matches entity at +0x28.
// ============================================================
bool CreateNaturalResource::GetTarget(void* param) {
    return (*(void**)param == parentEntity) ? 1 : 0;
}

// ============================================================
// class CreateNaturalResource (CreateNaturalResource.h) — ENDGAME B.3.8 ACT.
// The hand-built g_CreateNaturalResource_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"

// create 0x1400221d0 (inlined ctor). The tree's factory zeroed every field above
// +0x10; kept (not a behaviour change of this step).
CreateNaturalResource::CreateNaturalResource()
    : descEntity(nullptr),
      parentEntity(nullptr),
      cellIndex(0),
      _pad34{} {}

// [3] 0x1400c6890
// [4] 0x1400c6650
// [9] 0x1400c6610

void* CreateNaturalResource::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(CreateNaturalResource), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CreateNaturalResource());
}
void* CreateNaturalResource::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CreateNaturalResource());
}


DEFINE_EXE_TYPE(CreateNaturalResource, "Action")
