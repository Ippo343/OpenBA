// code/Game/Component/growthcomponent.cpp
// Clean reimplementation of GrowthComponent::Update from Application-x64.exe.
//
// GrowthComponent manages tree/crop growth with seasonal temperature modulation,
// harvest threshold logic, and sub-resource spawning.
//
// Update address: 0x140120050
// Struct size: 0x50, vtable at 0x140163590
//
// Key logic:
//   1. Compute base growth rate from config (1.0 / (config+0x28 * 60.0))
//   2. Apply seasonal temperature modulation if config+0x2C flag set
//   3. Accumulate growth: resource+0x28 += dt * growthRate * tempModifier
//   4. If growth < 0 and not flagged: reset and harvest
//   5. If growth > threshold (config+0x54): spawn sub-resources on timer
//   6. If growth > growthScale and conditions met: harvest, reset growth
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/growthcomponent_funcs.h"
#include "Component/ModelComponent.h"   // B.3.8 CB1a (TypeId)
#include <new>
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "Action/exe_bodies.h"            // ActHunt_*, ActCNR_* helpers
#include "Work/exe_bodies.h"              // WorkBase_FindComponent_52c0, WorkLeaf_FootprintOwnerCheck_120c30
#include "Entity/componententity_funcs.h" // ComponentEntity_CtorFromDescription, EntityList_RegisterEntity_Impl
#include "Game/exe_constants.h"           // kRdata_*
#include "math/Math.h"                    // Math::Vector4, Math::Scalar
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (standalone sibling-id fallback)
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/exe_bodies.h"

// ============================================================
// Helpers, called by name (B3R)
// ============================================================
//   0x1400c3470 ActHunt_NotifyEntityChanged_c3470 -- flag notification
//   0x14011fba0 GrowthComponent_SpawnSubResource   0x140120470 GrowthComponent_SetResourceAmount
//   0x140120410 GrowthComponent_GetResourceIndex   0x140126780 GrowthComponent_ResetGrowthState
//   0x140118950 GrowthComponent_ModifyFlags        (all below)
// DAT_1401c3dd8 (the game world) is read through g_pGameWorldSingleton, the engine_funcs
// global bound to the same cell.

namespace {
// The .rdata float constants (Game/exe_constants.h) that are stored as bit patterns.
inline float RdataF(uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof f); return f; }
}  // namespace

// Float constants from .rdata:
//   0x14017214c = 1.0f                     0x140172234 = 300.0f (the growth-time scale;
//   0x1401721d0 = freeze threshold (32.0f)   the "60.0f" this file used to call it was a
//   0x140172288 = growth rate at freeze      wrong label -- the cell has always held 300.0f)
//   0x1401722b0 = double mask for fabsf (0x7FFFFFFFFFFFFFFF)
// Visual-scale smoothing constants used by slot 10 (0x14011fea0):
//   0x1401720b8 = 0.001f (snap threshold)   0x140172144 = 0.99f (previous weight)
//   0x1401720c0 = 0.01f  (target weight)    0x1401720f4 = 0.15f (crop x/y ramp)
// Math::Vector4::Vector4(const Scalar&) (import slot 0x140152958) broadcasts one float
// into all four lanes.

#include "Object/factory_registration.h"
#include "Component/GrowthComponent.h"
#include "Component/GrowthDescription.h"
#include "Component/NaturalResourceComponent.h"
#include "Component/MapComponent.h"
#include "Game/Map.h"
#include "Component/ComponentDescription.h"
#include "Entity/ComponentEntity.h"

// ============================================================
// Helper: do harvest/reset sequence
// ============================================================
// Shared by both negative-growth and threshold-harvest paths.
static void DoHarvestReset(GrowthComponent* comp, const Map* resourceMgr, float harvestAmount) {
    // Set resource amount
    comp->SetResourceAmount(harvestAmount);   // 0x140120470

    // Update resource manager entry with owner position
    ComponentDescription* ownerPos = comp->_entity->_description;
    int resIdx = comp->GetResourceIndex();   // 0x140120410
    // grid base = *(resourceMgr+0x80) = the Map cell grid (game+0x80, built in
    // standalone by MapTileGrid_Build: dim*dim records, stride 0x30). Write the
    // owner position 8-byte handle into this tile's record at +0x10. The original
    // (GrowthComponent::Update 0x140120050) is:
    //   *(*(resourceMgr+0x80) + 0x10 + resIdx*0x30) = ownerPos
    // i.e. ONE deref of resourceMgr+0x80, then index by resIdx*0x30. A prior port
    // had an extra deref (*(uintptr_t*)gridBase), which read the zeroed first qword
    // of tile-record 0 and wrote to ~0x10 → access violation (misattributed to a
    // "missing resourceMgr grid"; the grid exists — see maptilegrid.cpp).
    // The Map's cells are untyped (GAME-b's header): LEFT.
    uintptr_t gridBase = reinterpret_cast<uintptr_t>(resourceMgr->cells);   // Map +0x80
    *reinterpret_cast<ComponentDescription**>(
        gridBase + 0x10 + (int64_t)resIdx * 0x30) = ownerPos;

    NaturalResourceComponent* resource = comp->resource.Get();

    // Reset growth to 0 (a dword 0 store: resourceScale = 0.0f, the same bits)
    resource->resourceScale = 0.0f;

    // Clear growth flag byte (+0x2D, the high byte of NaturalResourceComponent's
    // resourceFlags in COMP-d's header: LEFT)
    *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(resource) + 0x2D) = 0;

    // Reset growth state
    resource->ResetGrowthState();   // 0x140126780
}

// ============================================================
// GrowthComponent::Update (vtable slot [9], 0x140120050)
// ============================================================

// Step-2 body-init (vtable slot [4], +0x20, original 0x1401206f0). Forward-declared
// here so the Update guard can lazily invoke it if StartupComponents did not run.

// ============================================================
// Reimplemented exe helpers (callee bodies for the guarded call sites above)
// ============================================================
//
// These were previously unregistered exe addresses (ResolveAddr -> 0 -> silently
// skipped). Faithful 1:1 ports of the original FUN_* helpers so that the guarded
// calls in GrowthComponent_Update / DoHarvestReset actually fire.

// Sub-helpers used by the bodies below, called by name (B3R):
//   0x140004680 FindComponent<MapComponent> -- the footprint (tile x / y)
//   0x140120c30 WorkLeaf_FootprintOwnerCheck_120c30 -- spawn-site validity check
//               (growthComp/config, x, y) -> bool(low byte)
//   0x140005200 FindComponent<ParticleComponent>'s neighbour: see SpawnSubResource
//   0x1400c3720 ComponentEntity_CtorFromDescription -- ComponentEntity copy-ctor
//   0x1400052c0 FindComponent<NaturalResourceComponent> / 0x14000c460
//               FindComponent<GrowthComponent>
//   0x140126540 ActCNR_SetupModelAtCell_126540 (Entity_impl_6540) -- place a freshly-spawned
//               sub-resource entity: records its tile (+0x28/+0x2a), builds a yaw transform
//               via FromYawPitchOffset and writes it to the entity transform
//               (+0x148..+0x168), then Geometry::SetTransform. (The literal was once the
//               dropped-digit typo 0x140006540, mid-HealerDescription_Factory.)
//   0x140120db0 ActCNR_SetGrowthProgress_120db0 -- set growth value (float) on entity
//   0x140125320 / 0x1401253d0 ActCNR_NR_GetSubVariantCount_125320 / ActCNR_NR_SetVariant_1253d0
//   0x1400c48a0 EntityList_RegisterEntity_Impl -- register new entity into world entity list
//   0x140003f60 FindComponent<ModelComponent> (the GrowthComponent::Startup +0x30 accessor)
//   0x140126440 ActCNR_SetAnimIndex_126440 (AnimateNaturalResource_helper_6440) -- anim kick
// Random on world singleton (+0xd8); world resource manager (+0xf8); world flags (+0xd0).

// Reimpl of FUN_140118950 — Component::ModifyFlags: +0x20 = (f|or)&~andNot; notify the
// entity when the low word changed. Every component class reaches it (Animal, Growth).
void Component::ModifyFlags(uint32_t orMask, uint32_t andNotMask) {
    uint32_t oldF = _flags;
    uint32_t newF = (oldF | orMask) & ~andNotMask;
    _flags = newF;
    if (((newF ^ oldF) & 0xFFFF) != 0 && _entity != nullptr) {
        ActHunt_NotifyEntityChanged_c3470(_entity);
    }
}
// B4 FORWARD: 1 outside caller file (Component/animalcomponent.cpp); the closing
// wave rewires it.
void GrowthComponent_ModifyFlags(void* comp, uint32_t orMask, uint32_t andNotMask) {
    reinterpret_cast<Component*>(comp)->ModifyFlags(orMask, andNotMask);
}

// Reimpl of FUN_140120410 (GrowthComponent get tile index = y*mapW + x from the footprint).
// Retail loads the shorts with MOVSX.
int GrowthComponent::GetResourceIndex() {
    GameUpdateLoop* world = GameUpdateLoop::_singleton;
    if (!world) return 0;
    // GameUpdateLoop::m_game (+0xF8) is still void* (GAME-a): the one bridge is LEFT.
    const Map* map = reinterpret_cast<const Map*>(world->m_game);
    MapComponent* res = _entity->FindComponent<MapComponent>();   // 0x140004680
    int16_t x = static_cast<int16_t>(res->tileX0);
    int16_t y = static_cast<int16_t>(res->tileY0);
    return (int)y * map->size + (int)x;
}

// Reimpl of FUN_140120470 (GrowthComponent set per-tile growth byte at grid +0x2c)
void GrowthComponent::SetResourceAmount(float amount) {
    GameUpdateLoop* world = GameUpdateLoop::_singleton;
    if (!world) return;
    // m_game void* (GAME-a) and Map's cells untyped: the grid reads are LEFT.
    const Map* map = reinterpret_cast<const Map*>(world->m_game);
    MapComponent* res = _entity->FindComponent<MapComponent>();   // 0x140004680
    uintptr_t grid = reinterpret_cast<uintptr_t>(map->cells);   // Map +0x80
    int16_t x = static_cast<int16_t>(res->tileX0);
    int16_t y = static_cast<int16_t>(res->tileY0);
    int64_t cell = (int64_t)((int)x + (int)y * map->size);
    // DAT_14017222c is the byte<->float scale (255.0)
    const float divisor = RdataF(kRdata_14017222c);
    uint8_t* cellByte = reinterpret_cast<uint8_t*>(grid + 0x2c + cell * 0x30);
    float fv = (float)(*cellByte) / divisor;
    if (0.0f <= amount) fv = fv * fv * amount;
    else fv = fv - amount;
    int iv = (int)(fv * divisor);
    if (iv < 0) { *cellByte = 0; return; }
    *cellByte = (iv > 0xFF) ? 0xFF : (uint8_t)iv;
}

// Reimpl of FUN_140126780 — NaturalResourceComponent::ResetGrowthState: set bit
// 0x200000, notify, animate (RCX = the natural-resource component).
void NaturalResourceComponent::ResetGrowthState() {
    uint32_t oldF = _flags;
    uint32_t newF = oldF | 0x200000;
    _flags = newF;
    if (((newF ^ oldF) & 0xFFFF) != 0 && _entity != nullptr) {
        ActHunt_NotifyEntityChanged_c3470(_entity);
    }
    ActCNR_SetAnimIndex_126440(reinterpret_cast<int64_t>(this), 2);   // 0x140126440
}
// B4 FORWARD: 1 outside caller file (Component/tornadocomponent.cpp).
void GrowthComponent_ResetGrowthState(void* entityPtr) {
    reinterpret_cast<NaturalResourceComponent*>(entityPtr)->ResetGrowthState();
}

// Reimpl of FUN_14011fba0 (GrowthComponent spawn sub-resource onto a nearby tile)
void GrowthComponent::SpawnSubResource(GrowthDescription* cfg) {
    GameUpdateLoop* world = GameUpdateLoop::_singleton;
    if (!world) return;
    int* mapW = reinterpret_cast<int*>(world->m_game);   // m_game void* (GAME-a): LEFT
    System::Random* rng = world->m_random;
    // world flag bit 10 must be clear
    if ((world->m_gameFlags >> 10 & 1) != 0) return;

    float roll = rng->RandFloat();
    const float uvScale = RdataF(kRdata_140172140);   // 0x140172140
    int spawnChanceDenom = cfg->_i48;
    if (spawnChanceDenom == 0) return;
    if (roll > uvScale / (float)spawnChanceDenom) return;

    MapComponent* res = _entity->FindComponent<MapComponent>();   // 0x140004680
    int16_t baseX = static_cast<int16_t>(res->tileX0);
    int16_t baseY = static_cast<int16_t>(res->tileY0);

    int tol = cfg->_i4c;
    int x = (int)baseX + rng->RandTol(static_cast<int32_t>(tol));
    int y = (int)baseY + rng->RandTol(static_cast<int32_t>(tol));
    if (!(x >= 0 && y >= 0 && x < *mapW && y < *mapW)) return;

    // PORT GAP 0x14009f5d0 CharacterComponent_impl_f5d0: not ported here; retail calls it
    // here, (mapW, x, y, 4, (int)[0x140172140], (int)[0x140172248], 0.0f, 0, 0, 0), as the
    // tile placement test. This file's pointer to it was never filled (no resolve), so
    // the tree has always taken the not-placed arm: the sub-resource is not spawned.
    // (Its port PlayerPath_CharComp_f5d0 exists; wiring it is a behaviour change.)
    const bool placed = false;
    if (!placed) return;

    int64_t valid = WorkLeaf_FootprintOwnerCheck_120c30(
        reinterpret_cast<intptr_t>(cfg), x, y);   // 0x140120c30
    if ((char)valid == '\0') return;

    int mapWv = *mapW;
    int64_t parentIBP = 0;
    NaturalResourceComponent_FindComponentByType(_entity, &parentIBP);   // 0x140005200
    int16_t variantSeed = *reinterpret_cast<int16_t*>(*reinterpret_cast<uintptr_t*>(&parentIBP) + 0x164);

    // Allocate + copy-construct the new ComponentEntity from this entity's description.
    ComponentEntity* mem = (System::MemoryAllocator::_singleton)
                    ? static_cast<ComponentEntity*>(System::MemoryAllocator::MemAlloc(0x48, 8)) : nullptr;
    ComponentEntity* newEnt = nullptr;
    if (mem) {
        newEnt = ComponentEntity_CtorFromDescription(mem, _entity->_description);   // 0x1400c3720
    }
    ::Object::InternalPtr<ComponentEntity> newIBP(newEnt);

    NaturalResourceComponent* acc1 = newEnt->FindComponent<NaturalResourceComponent>();   // 0x1400052c0
    ActCNR_SetupModelAtCell_126540(reinterpret_cast<int64_t>(acc1), mapWv * y + x);   // 0x140126540

    GrowthComponent* acc2 = newEnt->FindComponent<GrowthComponent>();   // 0x14000c460
    ActCNR_SetGrowthProgress_120db0(reinterpret_cast<int64_t>(acc2), 0.0f);   // 0x140120db0

    int64_t acc3 = 0;
    NaturalResourceComponent_FindComponentByType(newEnt, &acc3);   // 0x140005200
    int64_t variantTarget = acc3;

    int64_t acc4 = 0;
    NaturalResourceComponent_FindComponentByType(newEnt, &acc4);   // 0x140005200
    int64_t* variantWrite = reinterpret_cast<int64_t*>(acc4);

    uint32_t range = ActCNR_NR_GetSubVariantCount_125320(variantTarget, (int)variantSeed);   // 0x140125320
    // Retail's call site (FUN_14011fba0 at 0x14011fe2c) is `CALL [0x1401512e8]`,
    // the import slot for ?Rand@Random@System@@QEAAII@Z — the uint overload.
    uint32_t roll2 = rng->Rand(static_cast<uint32_t>(range));
    ActCNR_NR_SetVariant_1253d0(variantWrite,   // 0x1401253d0
                                static_cast<uint16_t>(variantSeed), (short)roll2);

    ::Object::InternalPtr<ComponentEntity> regArg = newIBP;
    EntityList_RegisterEntity_Impl(world->m_entityList, &regArg,   // 0x1400c48a0
        _entity->_description->_category);
}

// ===== ENDGAME B.3.8 CIT — GrowthComponent as a REAL class (GrowthComponent.h) =====
// Before: a 48-slot hand-built GrowthComponent_LiveVtable filled at every create
// (slots 3 Serialize, 4 Startup, 9 Update, 10 PostUpdate), Inert_ScalarDtor in
// slot 0. After: the compiler's 16-slot vtable and a real ctor/dtor chain
// (B38_RULES D1, D2).


// 0x1401208f0. Component() is the CALL 0x1401188b0; the 0x140163590 stamp is
// the compiler's; +0x28..+0x40 = 0; +0x48 (the smoothed displayed scale) =
// +0x4c (growthScale) = 1.0f (0x14012092d / 0x140120934); flags |= 0xcb.
GrowthComponent::GrowthComponent()
    : resource(), model(), config(nullptr), spawnTimer{0},
      displayScale(1.0f), growthScale(1.0f) {
    OrFlagsInCtor(0xcb);
}

// ============================================================
// GrowthComponent::Startup (vtable slot [4], +0x20, original 0x1401206f0)
// ============================================================
// Body-init: populate the component fields from the owning entity (+0x10) and
// the GrowthDescription template (+0x18). Called at spawn time by
// ComponentEntity_StartupComponents (which dispatches vtable+0x20 on every
// component whose capability flags have bit 7 — GrowthComponent's 0xcb does).
//
//   resource@+0x28 = first sibling component of the natural-resource type
//                    (FUN_1400052c0, by type id DAT_1401c7b30).
//   +0x30          = sibling of type DAT_1401c7a50 (FUN_140003f60).
//   config@+0x38   = the GrowthDescription (comp+0x18). The original validates
//                    IsDerivedFrom(desc, GrowthDescription typeId 0x1401c77e8);
//                    in standalone the factory tree is incomplete so IsDerivedFrom
//                    is unreliable (see ComponentEntity_CtorFromDescription's
//                    IsDerivedFromOrTrust note) — trust comp+0x18, which the
//                    copy-ctor set to this component's own GrowthDescription.
//   growthScale@+0x4c = config+0x20 <= 0 ? -1.0 (never harvest)
//                       : RandFloatTol(rng) * config+0x24 + config+0x20.
void GrowthComponent::Startup() {
    ComponentEntity* owner = _entity;
    if (!owner) return;   // not yet wired into an entity

    // resource@+0x28 — sibling natural-resource component.
    resource = owner->FindComponent<NaturalResourceComponent>();   // 0x1400052c0

    // +0x30 — sibling ModelComponent (type DAT_1401c7a50), used by FUN_140120db0 to
    // write the growth-staged mesh scale. The shared FUN_140003f60 reimpl finds it via
    // the raw .rdata ModelComponent type-id (0x1401c7a50), which resolves NULL in
    // standalone → it returned null, so +0x30 never linked and tree growth-staging was a
    // no-op (BANISHED_MATURE_TREES stood in). Prefer the shared finder; if it yields
    // null, fall back to a
    // SELF-CONTAINED ModelComponent scan via GetTypeIdByName. Scoped here so it cannot
    // change the shared produce/animation callers of 3f60 (verified paths).
    ModelComponent* res30 = owner->FindComponent<ModelComponent>();   // 0x140003f60
    if (res30 == nullptr) {
        const uint32_t s_tidModel = ModelComponent::TypeId();   // B.3.8: was a lazy by-name cache (B6-5)
        if (s_tidModel) {
            int n = owner->_componentCount;
            Object::InternalPtr<Component>* arr = owner->_components;
            for (int i = 0; i < n && arr; ++i) {
                Component* c = arr[i].Get();
                if (!c) continue;
                uint32_t t = c->Type();   // slot 2
                if (t == s_tidModel) { res30 = static_cast<ModelComponent*>(c); break; }
            }
        }
    }
    model = res30;

    // config@+0x38 — trust the description the copy-ctor stored at +0x18.
    Description* desc0 = _description;
    GrowthDescription* cfg = static_cast<GrowthDescription*>(desc0);
    config = cfg;

    // growthScale@+0x4c.
    float minScale = cfg ? cfg->_f20 : -1.0f;
    if (minScale <= 0.0f) {
        growthScale = -1.0f;   // 0xbf800000
    } else {
        float range = cfg->_f24;
        float r = 0.0f;
        GameUpdateLoop* world = GameUpdateLoop::_singleton;
        if (world) {
            r = world->m_random->RandFloatTol();
        }
        growthScale = r * range + minScale;
    }
}
void GrowthComponent::OnAdded() {}                                  // [6] 0x140120690: retail's own `RET`

// [7] 0x140120560 (283 B). PORT GAP (retail 0x140120560) — B38 D9: not ported;
// the tree's table held the inert stub here, kept.
void GrowthComponent::OnRemove() {}

// [9] 0x140120050. RDX is a pointer to the elapsed System::Time (EntityList
// bucket tick 0x1400c42af).
void GrowthComponent::Update(void* time) {
    System::Time* deltaTime = static_cast<System::Time*>(time);

    // [LIVE-COMPONENT BRING-UP GUARD] On an inert-spawned standalone component the
    // body is zeroed unless Startup ran. config(+0x38) and the resource
    // sub-component(+0x28) are populated by GrowthComponent_Startup (step 2 of the
    // recipe), which the entity-spawn pipeline invokes via vtable+0x20 in
    // ComponentEntity_StartupComponents. As a belt-and-suspenders, if the body is
    // still zero but the description(+0x18) is present, run the body-init now
    // (idempotent). If it is still incomplete afterward, bail safely — everything
    // below derefs config/resource (and .rdata float constants) unconditionally.
    // See notes/STANDALONE_REG_WAVES_PLAN.md "Recipe to bring a component to life".
    //
    // ⚠ A STREAM-RESTORED component must NOT take the full Startup path. Retail's
    // GrowthComponent::Serialize (0x1401207c0) persists exactly +0x28 (resource IBP),
    // +0x30 (model IBP), +0x48 (displayed scale), +0x4c (growthScale) and +0x40 (timer)
    // — everything Startup computes EXCEPT +0x38, which is just a copy of the
    // description already sitting at +0x18. So after a load the one genuinely missing
    // field is +0x38, and running the whole body-init to get it RE-ROLLS +0x4c from the
    // RNG (see GrowthComponent_Startup), throwing away the per-instance harvest
    // threshold the save restored. That is a silent load-fidelity bug: the plant keeps
    // its growth but changes when it becomes harvestable, differently on every load.
    //
    // A non-null +0x28 is the tell: only the stream puts a resource IBP there before
    // Startup has ever run. Fill +0x38 and leave every restored field alone.
    Description* desc0 = _description;
    if (config == nullptr || resource.Get() == nullptr) {
        if (config == nullptr && resource.Get() != nullptr) {
            config = static_cast<GrowthDescription*>(desc0);
        } else if (_description != nullptr) {
            GrowthComponent::Startup();
        }
        if (config == nullptr || resource.Get() == nullptr)
            return;
    }

    const float fOne = kRdata_14017214c;             // 1.0f
    const float fGrowthTimeScale = kRdata_140172234; // 300.0f (this read was labelled "60.0f")

    GrowthDescription* cfg = config;

    // Base growth rate: 1.0 / (config+0x28 * 300.0)
    float configGrowthTime = cfg->_f28;
    float growthRate = fOne / (configGrowthTime * fGrowthTimeScale);

    // Get current growth value from resource component at +0x28
    NaturalResourceComponent* resourceComp = resource.Get();
    float currentGrowth = resourceComp->resourceScale;

    // Apply seasonal temperature modulation
    float tempModifier = fOne;
    if (cfg->_b2c) {
        // Get current temperature from world singleton
        // world->weather(+0x130)->temp(+0x18). GameUpdateLoop::m_weather is still
        // void* (GAME-a): the temperature read is LEFT.
        uintptr_t weatherPtr = reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton->m_weather);
        float currentTemp = *reinterpret_cast<float*>(weatherPtr + 0x18);

        // Compute temperature distance from optimal
        float optimalTemp = cfg->_f30;
        float tempRange = cfg->_f34;

        // fabsf via double abs with bitmask (matches original bitwise pattern)
        double diff = static_cast<double>(currentTemp - optimalTemp);
        uint64_t rawBits;
        __builtin_memcpy(&rawBits, &diff, sizeof(rawBits));
        rawBits &= kRdata_1401722b0;
        __builtin_memcpy(&diff, &rawBits, sizeof(diff));
        tempModifier = static_cast<float>(diff) / tempRange;

        if (tempModifier < 0.0f || fOne < tempModifier) {
            // Out of range: check freeze
            tempModifier = 0.0f;
            if (currentTemp <= RdataF(kRdata_1401721d0)) {
                tempModifier = RdataF(kRdata_140172288);
            }
        } else {
            // Parabolic falloff: 1 - dist^2
            tempModifier = fOne - tempModifier * tempModifier;
        }
    }

    // Get resource manager from world (m_game void*, GAME-a: LEFT)
    const Map* resourceMgr = reinterpret_cast<const Map*>(GameUpdateLoop::_singleton->m_game);

    // Compute time delta in seconds
    float dtSeconds = deltaTime->AsSeconds();

    // Accumulate growth
    float newGrowth = currentGrowth + dtSeconds * growthRate * tempModifier;

    if (newGrowth >= 0.0f ||
        ((resource.Get()->_flags >> 0x14 & 1) != 0)) {
        // Growth is positive OR entity has special flag (bit 20)

        // Store new growth value
        resource.Get()->resourceScale = newGrowth;

        // Set flag bit 0x10 (growth active)
        uint32_t oldFlags = _flags;
        uint32_t newFlags = oldFlags | 0x10;
        _flags = newFlags;

        // Notify entity if flags changed
        if (((newFlags ^ oldFlags) & 0xFFFF) != 0 && _entity != nullptr) {
            ActHunt_NotifyEntityChanged_c3470(_entity);
        }

        // Sub-resource spawning check
        if (cfg->_b44) {
            float spawnThreshold = cfg->_f54;
            if (spawnThreshold <= newGrowth && newGrowth != spawnThreshold) {
                // Accumulate spawn timer at comp+0x40
                spawnTimer._ticks += deltaTime->_ticks;

                float spawnTimerSec = spawnTimer.AsSeconds();
                float spawnInterval = static_cast<float>(cfg->_i50) * fGrowthTimeScale;

                if (spawnInterval < spawnTimerSec) {
                    do {
                        SpawnSubResource(cfg);   // 0x14011fba0
                        spawnTimerSec -= spawnInterval;
                    } while (spawnInterval < spawnTimerSec);

                    // Reset spawn timer to remainder
                    System::Time remainderTime;
                    RtSystem_Time_Ctor(&remainderTime, spawnTimerSec);
                    spawnTimer._ticks = remainderTime._ticks;
                }
            }
        }

        // Harvest check: growthScale threshold
        float harvestAt = growthScale;
        if (harvestAt > 0.0f && harvestAt < newGrowth) {
            // Check entity doesn't have bit 20 set
            if ((resource.Get()->_flags >> 0x14 & 1) == 0) {
                // Check world flag (bit 10 of world+0xD0)
                if ((GameUpdateLoop::_singleton->m_gameFlags >> 10 & 1) == 0) {
                    // Clamp growth to 1.0
                    float harvestAmount = (newGrowth <= fOne) ? newGrowth : fOne;

                    // Set resource amount scaled by config+0x40
                    float configResourceScale = cfg->_f40;
                    DoHarvestReset(this, resourceMgr, harvestAmount * configResourceScale);

                    // Clear flags bits 3,4 via ModifyFlags(this, 0, 0x18)
                    ModifyFlags(0, 0x18);   // 0x140118950
                }
            }
        }
    } else {
        // Growth went negative -- do full harvest/reset
        float configResourceScale = cfg->_f40;
        DoHarvestReset(this, resourceMgr, configResourceScale);

        // Clear flags bits 3,4 directly
        uint32_t oldFlags = _flags;
        uint32_t newFlags = oldFlags & 0xFFFFFFE7;
        _flags = newFlags;

        // Notify entity if flags changed
        if (((newFlags ^ oldFlags) & 0xFFFF) != 0 && _entity != nullptr) {
            ActHunt_NotifyEntityChanged_c3470(_entity);
        }
    }
}

// [10] 0x14011fea0 — dispatched by EntityList::PostUpdateNodeHandler (flags bit
// 4): ease the model's visual scale toward the accumulated growth.
// ============================================================
// GrowthComponent::ApplyVisualScale (vtable slot [10], +0x50, original 0x14011fea0)
// ============================================================
// THE per-frame link between a resource's growth value and how big it is drawn.
// EntityList::PostUpdateNodeHandler (0x1400c2c70) dispatches component slot 10 on every
// component whose flags carry bit 4, and GrowthComponent::Update is what sets that bit
// each time growth advances. Without this body the bit was set, the dispatch happened,
// and the inert stub returned — so the MeshInstance visual scale stayed at the engine
// ctor default of (1,1,1,1) and EVERY tree/crop rendered full-grown from the frame it
// was planted. Measured before the fix: a just-spawned crop at growth 0.000 already had
// visualScale=(1,1,1,1) with a mesh bound and a live scene id.
//
// Shape (1:1 with the disassembly):
//   target = min(resource+0x28 growth, 1.0), then raised to the description floor
//            (desc+0x60) so a resource never draws smaller than its minimum.
//   self+0x48 is the SMOOTHED displayed scale: snap when already within 0.001, else
//            ease 1% per tick (0.99*cur + 0.01*target) — this is what makes a tree grow
//            visibly rather than pop between sizes.
//   desc+0x64 selects the CROP form: x/y spread to full within the first 15% of growth
//            (0x1401720f4 = 0.15f) while z carries the height, versus the uniform
//            Vector4-broadcast form used by trees and every other natural resource.
//   The flag-4 clear only happens once the scale has CONVERGED, which is what keeps the
//   component in the post-update list across the whole ramp.
void GrowthComponent::PostUpdate() {
    // Retail derefs +0x28/+0x38/+0x30 unguarded — they are populated by
    // GrowthComponent::Startup, which runs on every spawned resource before the first
    // post-update pass. A null here means Startup did not run, which is the bug to fix.
    const bool gated = (resource.Get() == nullptr || config == nullptr ||
                        model.Get() == nullptr);
    if (gated) return;

    const float kOne          = kRdata_14017214c;          // 0x14017214c
    const float kEpsilon      = RdataF(kRdata_1401720b8);  // 0x1401720b8
    const float kSmoothOld    = kRdata_140172144;          // 0x140172144
    const float kSmoothNew    = RdataF(kRdata_1401720c0);  // 0x1401720c0
    const float kCropSpreadDiv = RdataF(kRdata_1401720f4); // 0x1401720f4

    float target = resource.Get()->resourceScale;   // growth 0..n
    if (target > kOne) target = kOne;

    GrowthDescription* desc = config;
    float descFloor = desc->_f60;
    if (descFloor > target) target = descFloor;

    float cur = displayScale;
    const float diff = std::fabs(target - cur);
    const bool converged = (kEpsilon > diff);
    if (converged) {
        displayScale = target;
    } else {
        displayScale = cur * kSmoothOld + target * kSmoothNew;
    }

    const float scale = displayScale;

    // The scale lanes are the model's MeshInstance::_color (model+0x134..+0x140 =
    // meshInstance+0x10c..+0x118).
    float* color = model.Get()->meshInstance._color;

    if (desc->_b64) {
        // Crop form: spread (x,y) reaches full early, height (z) tracks growth.
        float spread = scale / kCropSpreadDiv;
        if (spread < 0.0f)      spread = 0.0f;
        else if (spread > kOne) spread = kOne;
        color[0] = spread;
        color[1] = spread;
        color[2] = scale;
        color[3] = kOne;
    } else {
        // Uniform form: Math::Vector4(const Scalar&) broadcasts the scale into 4 lanes.
        float scalar = scale;
        alignas(16) uint8_t vec4[16];
        ::new (static_cast<void*>(vec4)) Math::Vector4(
            *reinterpret_cast<const Math::Scalar*>(&scalar));   // slot 0x140152958, B.3.x bridge
        {
            const uint32_t* v = reinterpret_cast<const uint32_t*>(vec4);
            uint32_t* lanes = reinterpret_cast<uint32_t*>(color);   // the four dword stores as retail
            lanes[0] = v[0];
            lanes[1] = v[1];
            lanes[2] = v[2];
            lanes[3] = v[3];
        }
    }

    if (!converged) return;   // stay registered until the ease-in finishes

    uint32_t oldFlags = _flags;
    uint32_t newFlags = oldFlags & ~0x10u;
    _flags = newFlags;
    if (((newFlags ^ oldFlags) & 0xffff) != 0 && _entity != nullptr) {
        ActHunt_NotifyEntityChanged_c3470(_entity);
    }
}

// [11] 0x1401206a0 (64 B). PORT GAP (retail 0x1401206a0) — B38 D9: not ported;
// the tree's table held the inert stub here, kept.
void GrowthComponent::Slot11() {}

// create 0x140034b10: MemAlloc(0x50, 8), then the ctor.
void* GrowthComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(GrowthComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) GrowthComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* GrowthComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) GrowthComponent());
}

DEFINE_EXE_TYPE(GrowthComponent, "Component")
