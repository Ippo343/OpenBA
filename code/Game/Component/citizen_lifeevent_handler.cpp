// code/Game/Component/citizen_lifeevent_handler.cpp
// CitizenComponent_HandleLifeEvent (0x140116dd0) + its five previously-unported
// helpers — the citizen-DEATH path, formerly a "Level-2 deep closure" defer.
// Closure audit 2026-07-19: every sub-callee is now registered, so the whole
// chain ports cleanly:
//
//   0x140116dd0 HandleLifeEvent        (this file)  — orchestrator, returns bool
//   0x140113a70 FamilyRingOrFlag_3a70  (this file)  — OR life-event flag around the
//                                        family ring (+0x260 head / +0x268 next)
//   0x140089740 FindNearestWithSlot    (this file)  — nearest cemetery (path-dist,
//                                        reachability 0xe1f, has free grave slot)
//   0x140046c60 PickFreeSlot_6c60      (this file)  — random-start scan of the
//                                        grave-cell array (+0x48/+0x50) for a cell
//                                        not occupied by a TombstoneComponent
//   0x140046a90 SpawnTombstone_6a90    (this file)  — claim map cell (state 2),
//                                        pathfinder dirty+enqueue, ctor tombstone
//                                        entity from CemeteryDescription+0x20,
//                                        place + register in the entity list
//   0x140004800 FindTombstoneComp_4800 (this file)  — component scan by type-id
//   0x140057b20 PlaceAtCell_7b20       (this file)  — stamp position component
//                                        (x,y,0x10001,yaw) + model transform via
//                                        Transform::FromYawPitchOffset at terrain h
//
// Death flags OR'd into +0x88 of self/partner(+0x258)/parent(+0x250)+family ring:
//   0x40 = buried (grave found), 0x02 = eventType 2 with no grave, 0x80 = other
//   death with no grave. Then PostLifeEvent(type, severity 1) posts the event-log
//   message and the citizen entity is queued on GameUpdateLoop+0xd0 for removal
//   (unless entity flag bit 10 at +0x40 already marks it).
//
// Type-id .rdata globals (storage + fill in standalone/reg_citizen_lifeevent_handler.cpp):
//   0x1401c7900 InteractComponent (already registered by reg_work_tail_fire)
//   0x1401c4a30 TombstoneComponent   (NEW storage)
//   0x1401c40c8 CemeteryDescription  (NEW storage)
//
// Retail callers of 0x140116dd0: CitizenComponent_Update (6 CALL sites) — already
// resolving through the registry (citizencomponent.cpp s_fn_HandleLifeEvent).

// graphics/Geometry.h FIRST: it pulls runtime/types/volume/Volume.h, whose
// global-scope `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()` once
// <windows.h> has been seen through resolve.h.  ENDGAME B.3.5 caller batch I2.
#include "graphics/Geometry.h"
#include <new>
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "Entity/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Component/coll_bridge.h"
#include "math/Math.h"
#include <cstdint>
#include <cstdio>
#include <cmath>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/InteractComponent.h"     // ENDGAME B.3.9 C1: 0x1401c7900 cell class
#include "Component/TombstoneComponent.h"    // ENDGAME B.3.9 C1: 0x1401c4a30 cell class
#include "Component/CemeteryDescription.h"   // ENDGAME B.3.9 C1: 0x1401c40c8 cell class
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Work/exe_bodies.h"
#include "Game/exe_bodies.h"
#include "Entity/componententity_funcs.h"
#include "system/MemoryAllocator.h"   // B3R: the allocator doors
#include "Component/CitizenComponent.h"      // B.4 W1 COMP-b: HandleLifeEvent is a member
#include "Component/CharacterComponent.h"
#include "Entity/ComponentEntity.h"
#include "Entity/CitizenBehavior.h"          // cemeteries (+0x40 / +0x48)
#include "Entity/EntityList.h"               // removalList (+0xD0)
#include "Component/CemeteryComponent.h"     // plots (+0x48 / +0x50)
#include "Component/MapComponent.h"          // the footprint rect / rotation
#include "Component/ModelComponent.h"        // transform / yaw / meshInstance
#include "Game/Map.h"                        // size / cells / pathfinder / regionGraph
#include "Game/PathfinderThread.h"           // SetCellTerrain (0x1400a6fe0)

// Scan an entity's component array (+0x30 ptr / +0x38 count) for type-id `tid`,
// IsDerivedFrom-checked — the shared body of 4800/6c60/9740's inner loops.
// The type id is the component's vtable slot [2] (Type()).
static Component* FindComponentByTypeId(ComponentEntity* entity, uint32_t tid) {
    Component* found = nullptr;
    int n = entity->_componentCount;                           // +0x38
    Object::InternalPtr<Component>* comps = entity->_components;   // +0x30
    for (int i = 0; i < n; i++) {
        Component* c = comps[i].Get();
        if (c && c->Type() == tid) {
            if (Object::IsDerivedFromNullSafe(c, tid)) found = c;
            break;   // retail breaks on first type-id match, derived or not
        }
    }
    return found;
}

// ---------------------------------------------------------------- 0x140113a70
// CitizenComponent::OrFamilyFlags: walk the circular children ring (+0x260 head,
// +0x268 next) OR-ing the life-event flag into each child's +0x88 status word.
void CitizenComponent::OrFamilyFlags(uint32_t flags) {
    CitizenComponent** head = &_firstChild;
    CitizenComponent* cur = *head;
    if (!cur) return;
    do {
        cur->happinessFlags |= flags;
        cur = cur->_nextSibling;
        // [STANDALONE GUARD] retail rings are always circular; a half-built ring
        // would spin *(0+0x88) here, so bail instead.
        if (!cur) return;
    } while (cur != *head);
}

// ---------------------------------------------------------------- 0x140046c60
// Pick a grave slot: random start index, walk the cemetery's grave-cell array
// (+0x48 int cells, +0x50 count) with wrap. A slot is usable if its map cell is
// empty OR its occupant entity has no TombstoneComponent. Returns slot index or
// 0xffffffff when every cell already holds a tombstone.
extern "C" uint32_t Cemetery_PickFreeSlot_6c60(CemeteryComponent* self) {
    GameUpdateLoop* world = GameUpdateLoop::_singleton;
    if (!world) return 0xffffffffu;
    int count = self->plotCount;                                 // +0x50
    if (count <= 0) return 0xffffffffu;
    System::Random* rng = world->m_random;                       // +0xd8
    int idx = static_cast<int>(rng->Rand(static_cast<uint32_t>(count)));
    Map* map = world->m_game;                                    // +0xf8
    uint32_t tid = TombstoneComponent::TypeId();
    for (int tries = 0; tries < count; tries++) {
        int cell = self->plots[idx];                             // +0x48
        ComponentEntity* occupant = map->cells[cell].occupant;   // +0x80 -> +0x08, stride 0x30
        if (!occupant) return static_cast<uint32_t>(idx);      // empty cell — free slot
        Component* ts = FindComponentByTypeId(occupant, tid);
        Component* tsIBP = ts;
        if (!tsIBP) return static_cast<uint32_t>(idx);         // occupant is not a tombstone
        idx = (idx + 1 >= count) ? 0 : idx + 1;
    }
    return 0xffffffffu;
}

// ---------------------------------------------------------------- 0x140089740
// Find the nearest cemetery with a reachable free grave slot. listMgr =
// *(world+0xf0): +0x40 component-ptr array, +0x48 count. Distance = path
// distance (f810) or euclidean + mapSize when unreachable-by-path; candidate
// must pass ValidateCell(0xe1f) reachability and have PickFreeSlot >= 0.
extern "C" CemeteryComponent** Cemetery_FindNearestWithSlot_9740(CitizenBehavior* listMgr, CemeteryComponent** out,
                                                                  uint32_t citizenCell) {
    *out = nullptr;
    GameUpdateLoop* world = GameUpdateLoop::_singleton;
    if (!world || !listMgr) return out;
    Map* map = world->m_game;                                    // +0xf8
    int* mapGrid = reinterpret_cast<int*>(map);   // LEFT: SIG WorkBase_ValidateCell_a00f0 (int* map; Work/exe_bodies.h, wave X)
    int best = 0x7fffffff;
    int count = listMgr->cemeteryCount;                          // +0x48
    uint32_t tidInteract = InteractComponent::TypeId();
    for (int i = 0; i < count; i++) {
        CemeteryComponent** slot = &listMgr->cemeteries[i];      // +0x40
        CemeteryComponent* comp = *slot;
        if (!comp) continue;   // [STANDALONE GUARD]
        ComponentEntity* entity = comp->_entity;                 // +0x10
        // The cemetery's map position = its InteractComponent's resolved position.
        Component* ic = FindComponentByTypeId(entity, tidInteract);
        InteractComponent* icIBP = static_cast<InteractComponent*>(ic);   // IsDerivedFrom-checked above
        float pos[3] = {0.0f, 0.0f, 0.0f};
        InteractNodePosition(icIBP, pos, 0, 0, nullptr);         // 0x140122e20
        int size = map->size;
        int cx = static_cast<int>(pos[0] + static_cast<float>(size >> 1));
        int cy = static_cast<int>(pos[1] + static_cast<float>(size >> 1));
        if (cx < 0) cx = 0;
        if (size <= cx) cx = size - 1;
        if (cy < 0) cy = 0;
        if (size <= cy) cy = size - 1;
        uint32_t cemCell = static_cast<uint32_t>(size * cy + cx);
        int dist = map->PathDistance(citizenCell, cemCell, 1);   // 0x14009f810
        if (dist < 0) {
            int dy = static_cast<int>(citizenCell) / size - static_cast<int>(cemCell) / size;
            int dx = static_cast<int>(citizenCell) % size - static_cast<int>(cemCell) % size;
            dist = static_cast<int>(sqrtf(static_cast<float>(dy * dy + dx * dx))) + size;
        }
        if (dist < best
            && (WorkBase_ValidateCell_a00f0(mapGrid, 0xe1f, citizenCell, static_cast<int>(cemCell)) & 0xff) != 0) {
            if (static_cast<int>(Cemetery_PickFreeSlot_6c60(*slot)) >= 0) {
                *out = *slot;
                best = dist;
            }
        }
    }
    return out;
}

// ---------------------------------------------------------------- 0x140004800
// Wrap the entity's TombstoneComponent (or null) in the out InternalBasePtr.
extern "C" TombstoneComponent** ComponentEntity_FindTombstoneComp_4800(ComponentEntity* entity,
                                                                       TombstoneComponent** outIBP) {
    Component* found = FindComponentByTypeId(entity, TombstoneComponent::TypeId());
    *outIBP = static_cast<TombstoneComponent*>(found);   // IsDerivedFrom-checked; the InternalBasePtr ctor's one store
    return outIBP;
}

// ---------------------------------------------------------------- 0x140057b20
// Place the tombstone at its grave cell: stamp the position component
// (x,y at +0x28/+0x2a, extent 0x10001 at +0x2c, yaw degrees normalized to
// [0,360) at +0x30) and set the model component's transform (+0x148 quat+pos,
// +0x168 yaw) from Transform::FromYawPitchOffset at terrain height.
extern "C" void Entity_PlaceAtCell_7b20(TombstoneComponent* self, int cell, float yawDeg) {
    GameUpdateLoop* world = GameUpdateLoop::_singleton;
    if (!world || !self) return;
    Map* map = world->m_game;                                    // +0xf8
    int size = map->size;
    ComponentEntity* entity = self->_entity;                     // +0x10
    MapComponent* posIBP = entity->FindComponent<MapComponent>();        // 0x140004680
    ModelComponent* modelIBP = entity->FindComponent<ModelComponent>();  // 0x140003f60
    if (!posIBP || !modelIBP) return;   // [STANDALONE GUARD] retail derefs unguarded
    int yaw = static_cast<int>(yawDeg);
    posIBP->tileX0 = static_cast<int16_t>(cell % size);         // +0x28
    posIBP->tileY0 = static_cast<int16_t>(cell / size);         // +0x2a
    posIBP->tileX1 = 1;                                          // +0x2c / +0x2e: the dword 0x10001 (H7)
    posIBP->tileY1 = 1;
    posIBP->extraField = yaw;                                    // +0x30
    if (yaw < 0) {
        do { yaw += 360; } while (yaw < 0);
        posIBP->extraField = yaw;
    }
    // World-space cell centre: cell - (halfMap - 0.5f). 0.5f = exe DAT_140172114.
    float half = static_cast<float>(size >> 1) - 0.5f;
    float inXY[3] = { static_cast<float>(cell % size) - half,
                      static_cast<float>(cell / size) - half, 0.0f };
    Math::Vector3 terr(0.0f, 0.0f, 0.0f);
    Reimpl_MapGetTerrainPos(map, &terr.x, inXY, '\0', nullptr);
    Math::Scalar pitch(0.0f);
    Math::Scalar yawF(yawDeg);
    Math::Transform xform = Math::Transform::FromYawPitchOffset(yawF, pitch, terr);
    modelIBP->transform = xform;                                 // +0x148 (7-dword copy)
    modelIBP->yaw = yawDeg;                                      // +0x168
    modelIBP->meshInstance.SetTransform(modelIBP->transform);    // +0x28 Geometry::SetTransform
}

// ---------------------------------------------------------------- 0x140046a90
// Claim the grave cell and spawn the tombstone entity there.
// cemComp = cemetery component, cell = grave cell index, citizenCell unused
// (matches retail), worldArg = game-world singleton (passed to the pathfinder cmd).
extern "C" void Cemetery_SpawnTombstone_6a90(CemeteryComponent* cem, int cell,
                                             uint64_t /*citizenCell*/, void* /*worldArg*/) {
    GameUpdateLoop* world = GameUpdateLoop::_singleton;
    if (!world || !cem) return;
    ComponentEntity* nullIBP = nullptr;                          // the InternalBasePtr(nullptr) ctor
    Map* map = world->m_game;                                    // +0xf8
    MapCell* cellRec = map->cells;                               // +0x80, stride 0x30
    uint8_t oldState = cellRec[cell].terrainType;                // +0x29
    bool stateWasWalk = ((1u << (oldState & 0x1f)) & 0x1e0u) != 0;   // old state 5..8
    *reinterpret_cast<uint8_t*>(&map->field_0xA2) |= static_cast<uint8_t>(stateWasWalk);   // LEFT: Map::field_0xA2 is bool (Game/Map.h, not this block's); the tree ORs the byte
    cellRec[cell].terrainType = 2;
    int size = map->size;
    if (stateWasWalk)
        map->regionGraph->MarkCellDirty(cell % size, cell / size);   // +0x70, 0x1400a8c90
    map->pathfinder->SetCellTerrain(cell, 2);                        // +0x68, 0x1400a6fe0
    cellRec[cell].occupant = nullIBP;  // = null (+0x08)
    // Tombstone yaw = the cemetery's own placed rotation (position component +0x30).
    MapComponent* resIBP = cem->_entity->FindComponent<MapComponent>();   // 0x140004680
    int rot = resIBP ? resIBP->extraField : 0;
    // Tombstone description lives at CemeteryDescription+0x20.
    Description* desc = cem->_description;                       // +0x18
    Description* cemDescBase = (desc
                     && Object::IsDerivedFromNullSafe(desc, CemeteryDescription::TypeId())) ? desc : nullptr;
    // [STANDALONE GUARD] retail trusts the IsDerivedFrom gate then derefs
    // unguarded; type-ids can be unfilled early in standalone, so trust the
    // raw description (mirrors tombstonecomponent.cpp's OnCreate note).
    if (!cemDescBase) cemDescBase = desc;
    if (!cemDescBase) return;
    CemeteryDescription* cemDesc = static_cast<CemeteryDescription*>(cemDescBase);
    void* tombDesc = cemDesc->_ref20;                            // +0x20
    if (!tombDesc) return;   // [STANDALONE GUARD]
    ComponentEntity* built = nullptr;
    ComponentEntity* mem = static_cast<ComponentEntity*>(System::MemoryAllocator::MemAlloc(0x48, 8));
    if (mem) built = ComponentEntity_CtorFromDescription(mem, tombDesc);
    ComponentEntity* entIBP = built;
    if (!entIBP) return;   // [STANDALONE GUARD] alloc/ctor failed
    TombstoneComponent* tsIBP = nullptr;
    ComponentEntity_FindTombstoneComp_4800(entIBP, &tsIBP);
    if (tsIBP) Entity_PlaceAtCell_7b20(tsIBP, cell, static_cast<float>(rot));
    ComponentEntity* regIBP = entIBP;
    EntityList_RegisterEntity_Impl(world->m_entityList, &regIBP,   // +0xe8
                                   *reinterpret_cast<int*>(reinterpret_cast<int64_t>(tombDesc) + 0x20));   // LEFT: CemeteryDescription::_ref20 is void* (Component/CemeteryDescription.h, not this block's)
}

// ---------------------------------------------------------------- 0x140116dd0
// Citizen death handler. Returns 1 (handled) or 0 when the world is in the
// bit-8 state at +0xd0 (loading/shutdown) — retail early-outs with AL=0 there.
uint64_t CitizenComponent::HandleLifeEvent(int eventType, const char* strParam) {
    void* selfV = this;
    GameUpdateLoop* world = GameUpdateLoop::_singleton;
    if (!world) return 0;   // [STANDALONE GUARD]
    if ((world->m_gameFlags >> 8 & 1) != 0) return 0;

    // --- Death position: the home's InteractComponent (+0x238) if housed, else the
    // citizen's own model (+0x278 -> +0x68, raw transform storage) ---
    float pos[3] = {0.0f, 0.0f, 0.0f};
    if (home == nullptr) {
        CharacterComponent* ch = character;
        if (ch) {   // [STANDALONE GUARD] retail derefs unguarded
            ModelComponent* homeEnt = ch->modelComponent;
            if (homeEnt) {
                pos[0] = homeEnt->transform.translation.x;   // +0x158
                pos[1] = homeEnt->transform.translation.y;   // +0x15c
                pos[2] = homeEnt->transform.translation.z;   // +0x160
            }
        }
    } else {
        InteractNodePosition(home->FindComponent<InteractComponent>(), pos, 0, 0, nullptr);   // 0x140122e20
    }

    // --- Clamp to a map cell ---
    int size = world->m_game->size;
    int cx = static_cast<int>(pos[0] + static_cast<float>(size >> 1));
    int cy = static_cast<int>(pos[1] + static_cast<float>(size >> 1));
    if (cx < 0) cx = 0;
    if (size <= cx) cx = size - 1;
    if (cy < 0) cy = 0;
    if (size <= cy) cy = size - 1;
    uint32_t cell = static_cast<uint32_t>(size * cy + cx);

    // --- Find a cemetery; spawn the tombstone; pick the family flag ---
    // BURIAL OUTCOME TRACE. Every arm below used to be silent, so "nobody was buried"
    // and "everybody was buried" produced identical logs -- and for the whole life of
    // this port the answer was always the first one, because the cemetery list at
    // *(gw+0xf0)+0x40 had no writer at all. Report the town-level count, the per-cemetery
    // plot count, and which branch was taken, so a regression names itself.
    CitizenBehavior* const townMgrForTrace = world->m_citizenList;
    const int cemCountForTrace =
        townMgrForTrace ? townMgrForTrace->cemeteryCount : -1;   // +0x48

    CemeteryComponent* cem = nullptr;
    Cemetery_FindNearestWithSlot_9740(world->m_citizenList, &cem, cell);
    uint32_t flags;
    if (!cem) {
        flags = (eventType != 2) ? 0x80u : 0x2u;
        // eventType names the CAUSE; without it every death in the log looks alike:
        // 0 starvation, 1 thirst, 2 old age, 3 health, 6 disease, 7 other.
        static const char* kCause[] = {"starvation", "thirst", "old-age", "health",
                                       "?4", "?5", "disease", "other"};
        fprintf(stderr, "[Burial] NO GRAVE: citizen=%p cause=%d(%s) cell=%u "
                        "townCemeteries=%d (0 => the state-2 registrar never ran; "
                        ">0 => every cemetery is full, unreachable, or has an empty "
                        "plot array) flags=0x%x\n",
                selfV, eventType,
                (eventType >= 0 && eventType < 8) ? kCause[eventType] : "?",
                cell, cemCountForTrace, flags);
        fflush(stderr);
    } else {
        flags = 0x40u;
        const int plotsForTrace = cem->plotCount;   // +0x50
        int slot = static_cast<int>(Cemetery_PickFreeSlot_6c60(cem));
        if (slot >= 0) {   // [STANDALONE GUARD] retail indexes unguarded (9740 just proved >= 0)
            int graveCell = cem->plots[slot];       // +0x48
            Cemetery_SpawnTombstone_6a90(cem, graveCell, cell, world);
            fprintf(stderr, "[Burial] BURIED: citizen=%p cem=%p townCemeteries=%d "
                            "plots=%d slot=%d graveCell=%d\n",
                    selfV, cem, cemCountForTrace, plotsForTrace, slot, graveCell);
        } else {
            // The flag still says buried (retail sets 0x40 before picking), but no
            // tombstone exists -- worth saying out loud, it is not the same outcome.
            fprintf(stderr, "[Burial] CEMETERY FOUND BUT NO SLOT: citizen=%p cem=%p "
                            "plots=%d (plots==0 => BuildPlotGrid bailed; see "
                            "[CemeteryPlots]) flags=0x%x\n",
                    selfV, cem, plotsForTrace, flags);
        }
        fflush(stderr);
    }

    // --- OR the flag into the parent (+0x258), the spouse (+0x250) + the spouse's
    //     children ring, and our own children ring ---
    if (_parent != nullptr) {
        _parent->happinessFlags |= flags;
    }
    if (spouse != nullptr) {
        spouse->happinessFlags |= flags;
        spouse->OrFamilyFlags(flags);
    }
    OrFamilyFlags(flags);

    // --- Post the event-log message (severity 1) ---
    PostLifeEvent(eventType, 1, strParam);

    // --- Queue the citizen entity for removal (the EntityList's removal list at
    //     +0xd0) ---
    ComponentEntity* entIBP = _entity;
    EntityList* loop = world->m_entityList;
    if (entIBP != nullptr && loop) {
        if ((entIBP->_entityFlags >> 10 & 1) == 0) {
            MarkEntityForRemoval(entIBP);   // 0x1400c2ea0
            // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
            GameColl::AsArray<GameColl::Ptr8>(&loop->removalList)
                .SetSizeFill(loop->removalList.count + 1, &entIBP);   // +0xd0 / +0xd8
        }
    }
    return 1;
}
