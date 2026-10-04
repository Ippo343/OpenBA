// code/Game/Action/action_base_cnr.cpp
// CreateNaturalResource / AnimateNaturalResource / RemoveNaturalResource
// shared exe sub-helpers (Wave 3, Action).
//
// The per-class Action .cpp files (createnaturalresource.cpp, animatenaturalresource.cpp,
// removenaturalresource.cpp) call these by name through Action/exe_bodies.h (B3R); the
// registration (RegisterActionCnrBatch in standalone/reg_action_cnr.cpp) stays for
// readers other blocks still have.
//
// Each function is a literal port of the decompiled exe helper — no behavior is
// invented or "improved". Every exe sub-call is a call of its body by name.
#include "Action/exe_bodies.h"
//

// The runtime type headers come FIRST: they pull runtime/types/volume/Volume.h,
// whose global-scope `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()`
// once <windows.h> has been seen through resolve.h.  ENDGAME B.3.5 batch I2.
#include "graphics/Geometry.h"
#include "graphics/Mesh.h"      // ENDGAME B.3.5 batch K1: reaches Volume.h, so BEFORE resolve.h
#include <new>
#include "object/External.h"
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "Entity/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Work/exe_bodies.h"              // WorkBase_FindComponent_3050, WorkAcc_FindComponent_*
#include "Component/exe_bodies.h"         // FloorComponent_FindResData, ModelComponentNotify_Reimpl
#include "Player/exe_bodies.h"            // RemovalCalc_helper_8d50
#include "Game/exe_bodies.h"              // Reimpl_MapGetTerrainPos
#include "Entity/componententity_funcs.h" // ComponentEntity_CtorFromDescription, EntityList_RegisterEntity_Impl
#include "Game/exe_constants.h"           // kRdata_*
#include "system/MemoryAllocator.h"       // System::MemoryAllocator::MemAlloc
#include <cstring>
#include "math/Math.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include "Component/ModelComponent.h"
#include "Component/GrowthComponent.h"
#include "Entity/ComponentEntity.h"   // ComponentEntity::FindComponent<T>
#include "Component/ModelDescription.h"
#include "Component/NaturalResourceDescription.h"
#include "Component/AutoPickupComponent.h"
#include "Component/ComponentDescription.h"
#include "Component/GrowthDescription.h"
#include "Component/MapComponent.h"
#include "Component/NaturalResourceComponent.h"
#include "Component/RawMaterialComponent.h"
#include "Component/RawMaterialDescription.h"
#include "Component/WorkComponent.h"
#include "Entity/CitizenBehavior.h"
#include "Entity/EntityList.h"
#include "Game/Map.h"

// ============================================================
// Exe sub-helpers and constants (shared by this batch), called by name (B3R).
//   0x140003f60 ActCNR_FindComponent_3f60 (this file)   0x140003050 WorkBase_FindComponent_3050
//   0x140004680 FloorComponent_FindResData               0x1400c2b70 ModelComponentNotify_Reimpl
//   0x1400c3470 ActHunt_NotifyEntityChanged_c3470        0x140006330 WorkAcc_FindComponent_6330
// 0x1401260f0 closure sub-helpers:
//   0x1400c2ea0 ActHunt_MarkEntityRemoval_c2ea0          0x140003110 ActHunt_DynArraySetCount_3110
//   0x1400f8d50 RemovalCalc_helper_8d50 (RemovalTool_helper_8d50)
//   0x1400c3720 ComponentEntity_CtorFromDescription (ComponentEntity_CopyCtor)
//   0x1400223c0 ActRm_FindComponent_223c0                0x140129760 ActRm_PositionEntity_9760
//   0x140022480 FUN_140022480 -- NOT PORTED (PORT GAP, below)
//   0x140088ef0 ActRm_RegisterRawMaterial_88ef0          0x1400c48a0 EntityList_RegisterEntity_Impl
//   0x140022520 WorkAcc_FindComponent_22520              0x140044810 ActRm_DynArrayPush4_44810
// 0x140126540 closure: 0x14009f3e0 Reimpl_MapGetTerrainPos (CharacterComponent_impl_f3e0)
// 0x140126440 closure: 0x1401250f0 ActMvDep_AnimHelper_50f0 (AnimalComponent_helper_50f0)
// Import slots: 0x140152958 Math::Vector4::Vector4(const Scalar&), 0x140152b00
// System::Time::Time(float) = RtSystem_Time_Ctor.
// Float constants (Game/exe_constants.h): 0x1401720f4 (divisor in FUN_140120db0, a bit
// pattern), 0x140172234 (time scale in FUN_140120db0), 0x140172114 (half-bias in
// Entity_impl_6540), 0x140172220 (yaw scale in Entity_impl_6540 / 6440), 0x140172204
// (amount scale in FUN_1401260f0), 0x14017214c (g_fUVScale).
// ============================================================
namespace {

inline float RdataF(uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof f); return f; }

}  // namespace

// Forward decl: ActCNR_SetAnimIndex_126440 tail-calls RemoveNatRes (defined below).
void ActCNR_RemoveNatRes_1260f0(int64_t self, void* citizenIBP, char harvestFlag);

// ============================================================
// Reimpl of Entity_ClearPathQueue (FUN_140003f60) — FindComponent, type 0x1401c7a50.
// (Called from createnaturalresource.cpp as FindComponent_NR and from
// animatenaturalresource.cpp as GetModelComp.)
// ============================================================
void* ActCNR_FindComponent_3f60(uintptr_t entity, void* retIBP) {
    return new (retIBP) ::Object::InternalPtr<ModelComponent>(
        reinterpret_cast<ComponentEntity*>(entity)->FindComponent<ModelComponent>());   // B4 FORWARD: receiver typed by the callers' blocks   // LEFT: SIG ActCNR_FindComponent_3f60
}


// ============================================================
// Reimpl of CreateNaturalResource_helper_cb00 (FUN_14009cb00) — cell terrain
// availability check. Returns 1 (true) in the low byte if the cell is unavailable
// (terrain bit 0/1, field_0x2A set, or pending in the field_0xE8 tree), else 0.
// ============================================================
uint64_t CheckCellTerrain(const Map* map, int cellIndex) {
    const MapCell& rec = map->cells[cellIndex];
    uint32_t passBits = 1u << (rec.terrainType & 0x1f);
    uint64_t high = passBits;   // retail's RAX before the tree walk
    if (((passBits & 2) == 0) && ((passBits & 1) == 0) && (rec.field_0x2A == 0xff)) {
        const AvlNode28* node = map->field_0xE8;
        while (true) {
            while (true) {
                if (node == nullptr) return 0;
                if (node->key <= cellIndex) break;
                node = node->left;    // +0x10
            }
            if (cellIndex <= node->key) break;
            node = node->right;       // +0x18
        }
        // LEFT: retail returns the node pointer's bits with the low byte replaced by 1
        // (callers read only the low byte as the bool).
        high = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(node));   // CAST-OK: ptr_int
    }
    // CONCAT71(high>>8, 1)
    return (high & ~0xffull) | 1u;
}

// ============================================================
// Reimpl of FUN_140120db0 — GrowthComponent: set the growth progress.
// ============================================================
void SetGrowthProgress(GrowthComponent* self, float progress) {
    float growth = progress * self->growthScale;
    float uvScale = kRdata_14017214c;
    self->resource.Get()->resourceScale = growth;
    float clamped = (uvScale < growth) ? uvScale : growth;
    self->displayScale = clamped;

    ModelComponent* target = self->model.Get();
    if (!self->config->_b64) {
        float g = (growth <= uvScale) ? growth : uvScale;
        float scalar = g;
        {
            Math::Vector4 vec4((Math::Scalar(scalar)));   // slot 0x140152958
            target->meshInstance._color[0] = vec4.x;
            target->meshInstance._color[1] = vec4.y;
            target->meshInstance._color[2] = vec4.z;
            target->meshInstance._color[3] = vec4.w;
        }
    } else {
        float divisor = RdataF(kRdata_1401720f4);   // 0x1401720f4
        float scaled = clamped / divisor;
        float lo = 0.0f;
        if ((0.0f <= scaled) && (lo = scaled, uvScale < scaled)) lo = uvScale;
        float hi = growth;
        if (uvScale < growth) hi = uvScale;
        target->meshInstance._color[0] = lo;
        target->meshInstance._color[1] = lo;
        target->meshInstance._color[2] = hi;
        target->meshInstance._color[3] = uvScale;
    }

    const GrowthDescription* desc = self->config;
    float rf = GameUpdateLoop::_singleton->m_random->RandFloat();
    float timeScale = kRdata_140172234;   // 0x140172234
    System::Time timeObj = {};
    RtSystem_Time_Ctor(&timeObj, rf * static_cast<float>(desc->_i50) * timeScale);   // slot 0x140152b00

    uint32_t flagsOld = self->_flags;
    self->spawnTimer = timeObj;
    uint32_t flagsNew = flagsOld & 0xffffffef;
    self->_flags = flagsNew;
    if ((((flagsNew ^ flagsOld) & 0xffff) != 0) && (self->_entity != nullptr)) {
        NotifyEntityChanged(self->_entity);   // 0x1400c3470
    }
}

// ============================================================
// Reimpl of CreateNaturalResource_helper_52f0 (FUN_1401252f0) — the model's mesh-group
// count: the ModelDescription (type-gated) _meshGroupCount.
// ============================================================
uint32_t ModelMeshGroupCount(const ModelComponent* model) {
    Description* desc = model->_description;
    const ModelDescription* d = (::Object::IsDerivedFromNullSafe(desc, ModelDescription::TypeId()))
                                    ? static_cast<const ModelDescription*>(desc) : nullptr;
    return static_cast<uint32_t>(d->_meshGroupCount);
}

// ============================================================
// Reimpl of FUN_140125320 — the mesh count of mesh group `group`.
// ============================================================
uint32_t ModelMeshCount(const ModelComponent* model, int group) {
    Description* desc = model->_description;
    const ModelDescription* d = (::Object::IsDerivedFromNullSafe(desc, ModelDescription::TypeId()))
                                    ? static_cast<const ModelDescription*>(desc) : nullptr;
    return static_cast<uint32_t>(d->_meshGroups[group].meshCount);
}

// ============================================================
// Reimpl of FUN_1401253d0 — ModelComponent: select the mesh (group, index).
// Writes meshGroup and meshIndex, and on transitions (de)activates the model: slot 7
// (hide) when an old index was active; sets the mesh + slot 6 (show) when the new index is
// valid; finally notifies the entity (0x1400c2b70, 0).
// ============================================================
void ModelSelectMesh(ModelComponent* model, uint16_t variant, int16_t subVariant) {
    // type-gate the description; the result is unused here.
    ::Object::IsDerivedFromNullSafe(model->_description, ModelDescription::TypeId());

    model->meshGroup = static_cast<int16_t>(variant);
    if (model->meshIndex >= 0) {
        model->OnRemove();   // slot 7
    }
    model->meshIndex = subVariant;
    if (subVariant >= 0) {
        Description* descMesh = model->_description;
        const ModelDescription* d = (::Object::IsDerivedFromNullSafe(descMesh, ModelDescription::TypeId()))
                                        ? static_cast<const ModelDescription*>(descMesh) : nullptr;
        int16_t sub = model->meshIndex;
        int16_t var = model->meshGroup;
        Graphics::GraphicsMesh* mesh = d->_meshGroups[var].meshes[sub];
        model->meshInstance.SetMesh(Graphics::ExternalPtrHandle{mesh});
        model->OnAdded();   // slot 6
    }
    ModelComponentNotify_Reimpl(model->_entity, 0);   // 0x1400c2b70
}

// ============================================================
// Reimpl of AnimateNaturalResource_helper_6440 (FUN_140126440) — NaturalResourceComponent:
// set the animation index.
// If the description (type-gated 0x1401c7b68) _b64 == 0: if the dead bit (_flags bit 21) is
// set, run RemoveNatRes (FUN_1401260f0) with a null IBP. Else: play the animation on the
// entity's ModelComponent, set _flags bit 2, and on a low-16-bit flag change with a
// non-null entity, notify the entity.
// ============================================================
void NaturalResourceSetAnimation(NaturalResourceComponent* self, uint32_t animIdx) {
    Description* desc = self->_description;
    const NaturalResourceDescription* d = (::Object::IsDerivedFromNullSafe(desc, NaturalResourceDescription::TypeId()))
                                              ? static_cast<const NaturalResourceDescription*>(desc) : nullptr;

    if (!d->_b64) {
        if (((self->_flags >> 0x15) & 1) != 0) {
            ComponentEntity* nullIBP = nullptr;
            RemoveNaturalResourceCore(self, &nullIBP, 0);
        }
    } else {
        ModelComponent* model = self->_entity->FindComponent<ModelComponent>();   // 0x140003f60
        float uvScale = kRdata_14017214c;
        PlayModelAnimation(model, animIdx, 0.0f, 0.0f, uvScale);   // 0x1401250f0
        uint32_t flagsOld = self->_flags;
        uint32_t flagsNew = flagsOld | 4;
        self->_flags = flagsNew;
        if ((((flagsNew ^ flagsOld) & 0xffff) != 0) && (self->_entity != nullptr)) {
            NotifyEntityChanged(self->_entity);   // 0x1400c3470
        }
    }
}

// ============================================================
// Reimpl of Entity_impl_6540 (FUN_140126540) — CreateNaturalResource model placement.
// Positions the entity's MapComponent and ModelComponent at the cell, with jitter.
// ============================================================
uint64_t NaturalResourcePlaceAtCell(NaturalResourceComponent* self, int cellIndex) {
    GameUpdateLoop* gameWorld = GameUpdateLoop::_singleton;
    Description* desc = self->_description;
    Map* map = gameWorld->m_game;
    System::Random* rng = gameWorld->m_random;
    int gridW = map->size;

    const NaturalResourceDescription* d = (::Object::IsDerivedFromNullSafe(desc, NaturalResourceDescription::TypeId()))
                                              ? static_cast<const NaturalResourceDescription*>(desc) : nullptr;
    float jitter = d->_f60;

    float halfBias = kRdata_140172114;   // 0x140172114
    float half = static_cast<float>(map->size >> 1) - halfBias;
    float cellX = static_cast<float>(cellIndex % map->size) - half;
    float cellY = static_cast<float>(cellIndex / map->size) - half;

    float inCell[2] = { cellX, cellY };
    float outPos[3] = { 0.0f, 0.0f, 0.0f };
    Reimpl_MapGetTerrainPos(map, outPos, inCell, '\0', nullptr);   // 0x14009f3e0

    float r1 = rng->RandFloatTol();
    float r2 = rng->RandFloatTol();
    float worldX = r1 * jitter + outPos[1];
    float worldY = r2 * jitter + outPos[0];
    float worldZ = outPos[2] + 0.0f;
    float yawScale = kRdata_140172220;   // 0x140172220
    float yaw = rng->RandFloatTol() * yawScale;

    MapComponent* pos = self->_entity->FindComponent<MapComponent>();       // 0x140004680
    ModelComponent* model = self->_entity->FindComponent<ModelComponent>(); // 0x140003f60

    pos->tileX0 = static_cast<int16_t>(cellIndex % gridW);
    pos->tileY0 = static_cast<int16_t>(cellIndex / gridW);
    // One qword store of 0x10001 in retail (H7): width 1, height 1, extraField 0.
    pos->tileX1 = 1;
    pos->tileY1 = 1;
    pos->extraField = 0;

    float pitch = 0.0f;
    Math::Vector3 offset(worldY, worldX, worldZ);  // FromYawPitchOffset offset = {local_98, local_94, local_90}
    Math::Transform xform = Math::Transform::FromYawPitchOffset(yaw, pitch, offset);
    model->transform = xform;
    model->yaw = yaw;
    model->meshInstance.SetTransform(model->transform);
    return 1;
}

// ============================================================
// Reimpl of FUN_1401260f0 — RemoveNaturalResource core removal/harvest.
// `citizenIBP` = the harvesting citizen's entity IBP, `harvestFlag`.
// Marks the entity for removal, then (if harvestable) spawns a raw-material entity at the
// cell, registers the harvest amount, and claims the cell for the pile.
// ============================================================
void RemoveNaturalResourceCore(NaturalResourceComponent* self, ComponentEntity* const* citizenIBP, char harvestFlag) {
    GameUpdateLoop* gameWorld = GameUpdateLoop::_singleton;
    ComponentEntity* parent = self->_entity;
    EntityList* entityList = gameWorld->m_entityList;

    // Mark the entity for removal unless already marked (bit 10 of its flags).
    if (parent != nullptr) {
        if (((parent->_entityFlags >> 10) & 1) == 0) {
            MarkEntityForRemoval(parent);   // 0x1400c2ea0
            ActHunt_DynArraySetCount_3110(&entityList->removalList,   // 0x140003110
                                          entityList->removalList.count + 1, &parent);
        }
    }

    if (!self->harvestable) {   // not harvestable
        return;
    }

    Description* desc = self->_description;
    const NaturalResourceDescription* d = (::Object::IsDerivedFromNullSafe(desc, NaturalResourceDescription::TypeId()))
                                              ? static_cast<const NaturalResourceDescription*>(desc) : nullptr;
    ComponentDescription* material = d->_building;
    if (material == nullptr) {
        return;
    }

    float uvScale = kRdata_14017214c;
    float frac = uvScale;
    if (self->resourceScale <= uvScale) frac = self->resourceScale;

    int amount;
    // LEFT: 0x140006330's C declaration (Work/exe_bodies.h, WORK-a's header) takes the
    // template as an integer.
    const RawMaterialDescription* raw = nullptr;
    WorkAcc_FindComponent_6330(reinterpret_cast<uintptr_t>(material), &raw);   // 0x140006330   // LEFT: SIG WorkAcc_FindComponent_6330
    if (!self->mature) {
        amount = raw->_lowCreateCount;
    } else {
        amount = raw->_highCreateCount;
    }
    amount = static_cast<int>(static_cast<float>(amount) * frac);
    if (amount <= 0) {
        return;
    }

    // The cell index from the entity's MapComponent tile.
    const MapComponent* pos = self->_entity->FindComponent<MapComponent>();   // 0x140004680
    int cell = static_cast<int>(pos->tileY0) * gameWorld->m_game->size + static_cast<int>(pos->tileX0);

    // ── ECONOMY ACCOUNTING (always) ─────────────────────────────────────────
    // Increment the town raw-material stock — the essential produce effect the economy loop
    // consumes. It needs only the citizen, the material and the yield, NOT the spawned
    // item entity, so it runs unconditionally.
    ComponentEntity* regIBP = *citizenIBP;
    RegisterRawMaterial(gameWorld->m_citizenList, &regIBP, harvestFlag, d->_building, amount);   // 0x140088ef0

    // Query the existing surface work at the cell (for its id at +0x24).
    // LEFT: 0x1400f8d50's C declaration (Player/exe_bodies.h) takes the scheduler as an
    // integer; the object it returns (+0x24 read) has no settled class.
    int64_t surfIBP = 0;
    RemovalCalc_helper_8d50(reinterpret_cast<int64_t>(gameWorld->m_tutorial), reinterpret_cast<void**>(&surfIBP), cell);   // 0x1400f8d50   // LEFT: SIG RemovalCalc_helper_8d50 (Player/exe_bodies.h)
    uint32_t surfMatId = 0xffffffff;
    if (surfIBP != 0) {
        surfMatId = *reinterpret_cast<uint32_t*>(surfIBP + 0x24);   // LEFT: the returned object's class is not settled (0x1400f8d50)
    }

    // Spawn a new raw-material entity cloned from the resource description's template.
    ComponentEntity* mem = static_cast<ComponentEntity*>(System::MemoryAllocator::MemAlloc(0x48, 8));
    ComponentEntity* newEntity = mem
                      ? ComponentEntity_CtorFromDescription(mem, d->_building)   // 0x1400c3720
                      : nullptr;

    RawMaterialComponent* rawMat = newEntity->FindComponent<RawMaterialComponent>();   // 0x1400223c0
    int rt = gameWorld->m_random->RandTol(2);
    // DAT_140172204 = 90.0f (rotation scale).
    float amtScale = kRdata_140172204;   // 0x140172204
    if (rawMat != nullptr) PositionRawMaterial(rawMat, cell, static_cast<float>(rt) * amtScale);   // 0x140129760

    if (*citizenIBP != nullptr) {
        // PORT GAP 0x140022480 FUN_140022480: not ported; retail calls it here,
        // FUN_140022480(*(citizen+0x28), &local_res8) where local_res8 = cell. It was never
        // registered, so the call was always skipped: the null arm (no call) is the code.
    }

    if (rawMat != nullptr) {
        rawMat->quantity = amount;
        rawMat->profession = nullptr;
    }

    if (surfIBP != 0) {
        WorkComponent* work = newEntity->FindComponent<WorkComponent>();   // 0x140003050
        work->workFlags = surfMatId;
    }

    ComponentEntity* listIBP = newEntity;
    EntityList_RegisterEntity_Impl(gameWorld->m_entityList, &listIBP,   // 0x1400c48a0
                   static_cast<int>(d->_building->_category));

    // ── GRID CLAIM (dc20's net effect, scoped to this pile) ──────────────────
    // Retail claims the cell via c48a0 -> ComponentEntity_OnAddedPass -> the pile
    // MapComponent's vtable+0x30 (MapComponent::Draw 0x140124370) -> sub_41b0
    // (0x1401241b0) -> dc20 (0x14009dc20), whose net effect is MapCell::occupant = the
    // entity plus the component's _flags bit 0x100000. Standalone's MapComponent is inert,
    // so the pile spawned with the occupant still null — and GetTilesInArea
    // (FUN_14009fc00) only reports a cell whose occupant is non-null, so the pile was
    // invisible to every tile scan (the ClearArea::Execute Phase 3 haul gather). The same
    // spawn-site collapse starttown.cpp applies to worldgen resources, kept SCOPED to the
    // pile (1260f0 marked the tree at its top via c2ea0, which nulls the occupant first).
    // Bit 0x100000 is set so the removal-side un-mark stays symmetric.
    if (newEntity != nullptr) {
        MapComponent* mc = newEntity->FindComponent<MapComponent>();   // 0x140004680
        Map* map = gameWorld->m_game;
        MapCell* cells = map ? map->cells : nullptr;
        const int dim = map ? map->size : 0;
        // 41b0's own gate: skip when occupancy is already written for this component.
        if (mc && cells && dim > 0 && cell >= 0 && cell < dim * dim &&
            ((mc->_flags >> 20) & 1) == 0) {
            cells[cell].occupant = newEntity;
            mc->_flags |= 0x100000u;
        }
    }

    if ((harvestFlag != 0) && (*citizenIBP != nullptr)) {
        AutoPickupComponent* out = (*citizenIBP)->FindComponent<AutoPickupComponent>();   // 0x140022520
        if (out != nullptr) {
            WorkComponent* work = newEntity->FindComponent<WorkComponent>();   // 0x140003050
            uint32_t v = static_cast<uint32_t>(work->workId);
            AutoPickupAppend(out, v);   // 0x140044810
        }
    }
}

// ===========================================================================
// B4 FORWARD: the C spellings other blocks' files still call, one line each.
// ===========================================================================

// B4 FORWARD: outside callers (Action/createnaturalresource.cpp; Component/spawncomponent.cpp)
uint64_t ActCNR_CheckTerrain_cb00(void* mapGrid, int cellIndex) {
    return CheckCellTerrain(reinterpret_cast<const Map*>(mapGrid), cellIndex);   // LEFT: SIG ActCNR_CheckTerrain_cb00
}

// B4 FORWARD: outside callers (Component/growthcomponent.cpp)
void ActCNR_SetGrowthProgress_120db0(int64_t self, float progress) {
    SetGrowthProgress(reinterpret_cast<GrowthComponent*>(self), progress);   // LEFT: SIG ActCNR_SetGrowthProgress_120db0
}


// B4 FORWARD: outside callers (Action/createnaturalresource.cpp)
uint32_t ActCNR_NR_GetSubVariantCount_125320(int64_t comp, int variant) {
    return ModelMeshCount(reinterpret_cast<const ModelComponent*>(comp), variant);   // LEFT: SIG ActCNR_NR_GetSubVariantCount_125320
}

// B4 FORWARD: outside callers (Action/createnaturalresource.cpp; Component files)
void ActCNR_NR_SetVariant_1253d0(int64_t* comp, uint16_t variant, int16_t subVariant) {
    ModelSelectMesh(reinterpret_cast<ModelComponent*>(comp), variant, subVariant);   // LEFT: SIG ActCNR_NR_SetVariant_1253d0
}

// B4 FORWARD: outside callers (Action/animatenaturalresource.cpp; Component files)
void ActCNR_SetAnimIndex_126440(int64_t self, uint32_t animIdx) {
    NaturalResourceSetAnimation(reinterpret_cast<NaturalResourceComponent*>(self), animIdx);   // LEFT: SIG ActCNR_SetAnimIndex_126440
}

// B4 FORWARD: outside callers (Action/createnaturalresource.cpp; Game/starttown.cpp)
uint64_t ActCNR_SetupModelAtCell_126540(int64_t self, int cellIndex) {
    return NaturalResourcePlaceAtCell(reinterpret_cast<NaturalResourceComponent*>(self), cellIndex);   // LEFT: SIG ActCNR_SetupModelAtCell_126540
}

// B4 FORWARD: outside callers (Action/removenaturalresource.cpp; Component, Work files)
void ActCNR_RemoveNatRes_1260f0(int64_t self, void* citizenIBP, char harvestFlag) {
    RemoveNaturalResourceCore(reinterpret_cast<NaturalResourceComponent*>(self),   // LEFT: SIG ActCNR_RemoveNatRes_1260f0
                              reinterpret_cast<ComponentEntity* const*>(citizenIBP), harvestFlag);   // LEFT: SIG ActCNR_RemoveNatRes_1260f0
}
