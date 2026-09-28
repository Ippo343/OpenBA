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
#include "engine_funcs.h"
#include "resolve.h"
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

// Component vtable slot [2] (offset 0x10) = GetTypeId(this).
static inline uint32_t CompTypeId(void* comp) {
    return reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge (slot 2)
}

// Scan an entity's component array (+0x30 ptr / +0x38 count) for type-id `tid`,
// IsDerivedFrom-checked — the shared body of 4800/6c60/9740's inner loops.
static void* FindComponentByTypeId(uint8_t* entity, uint32_t tid) {
    void* found = nullptr;
    int n = *reinterpret_cast<int*>(entity + 0x38);
    void** comps = *reinterpret_cast<void***>(entity + 0x30);
    for (int i = 0; i < n; i++) {
        void* c = comps[i];
        if (c && CompTypeId(c) == tid) {
            if (Object::IsDerivedFromNullSafe(c, tid)) found = c;
            break;   // retail breaks on first type-id match, derived or not
        }
    }
    return found;
}

// ---------------------------------------------------------------- 0x140113a70
// Walk the circular family ring (+0x260 head, +0x268 next) OR-ing the life-event
// flag into each member's +0x88 status word.
extern "C" void Citizen_FamilyRingOrFlag_3a70(void* citizenV, uint32_t flags) {
    uint8_t* self = reinterpret_cast<uint8_t*>(citizenV);
    uint8_t** head = reinterpret_cast<uint8_t**>(self + 0x260);
    uint8_t* cur = *head;
    if (!cur) return;
    do {
        *reinterpret_cast<uint32_t*>(cur + 0x88) |= flags;
        cur = *reinterpret_cast<uint8_t**>(cur + 0x268);
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
extern "C" uint32_t Cemetery_PickFreeSlot_6c60(void* cemCompV) {
    uint8_t* self = reinterpret_cast<uint8_t*>(cemCompV);
    uint8_t* world = g_pGameWorldSingleton ? reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) : nullptr;
    if (!world) return 0xffffffffu;
    int count = *reinterpret_cast<int*>(self + 0x50);
    if (count <= 0) return 0xffffffffu;
    void* rng = *reinterpret_cast<void**>(world + 0xd8);
    int idx = static_cast<int>(reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(count)));   // B.3.4b bridge
    int64_t map = *reinterpret_cast<int64_t*>(world + 0xf8);
    uint32_t tid = TombstoneComponent::TypeId();
    for (int tries = 0; tries < count; tries++) {
        int cell = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(self + 0x48)
                                           + static_cast<int64_t>(idx) * 4);
        uint8_t* occupant = *reinterpret_cast<uint8_t**>(*reinterpret_cast<int64_t*>(map + 0x80)
                                                         + 8 + static_cast<int64_t>(cell) * 0x30);
        if (!occupant) return static_cast<uint32_t>(idx);      // empty cell — free slot
        void* ts = FindComponentByTypeId(occupant, tid);
        int64_t tsIBP = 0;
        new (&tsIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(ts));   // B.3.3 bridge
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
extern "C" void** Cemetery_FindNearestWithSlot_9740(void* listMgrV, void** out, uint32_t citizenCell) {
    uint8_t* listMgr = reinterpret_cast<uint8_t*>(listMgrV);
    *out = nullptr;
    uint8_t* world = g_pGameWorldSingleton ? reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) : nullptr;
    if (!world || !listMgr) return out;
    int* map = *reinterpret_cast<int**>(world + 0xf8);
    int best = 0x7fffffff;
    int count = *reinterpret_cast<int*>(listMgr + 0x48);
    uint32_t tidInteract = InteractComponent::TypeId();
    for (int i = 0; i < count; i++) {
        void** slot = reinterpret_cast<void**>(*reinterpret_cast<int64_t*>(listMgr + 0x40)
                                               + static_cast<int64_t>(i) * 8);
        uint8_t* comp = reinterpret_cast<uint8_t*>(*slot);
        if (!comp) continue;   // [STANDALONE GUARD]
        uint8_t* entity = *reinterpret_cast<uint8_t**>(comp + 0x10);
        // The cemetery's map position = its InteractComponent's resolved position.
        void* ic = FindComponentByTypeId(entity, tidInteract);
        int64_t icIBP = 0;
        new (&icIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(ic));   // B.3.3 bridge
        float pos[3] = {0.0f, 0.0f, 0.0f};
        Pf_EntityResolvePos_2e20(reinterpret_cast<void*>(icIBP), pos, 0, 0, nullptr);
        int size = *map;
        int cx = static_cast<int>(pos[0] + static_cast<float>(size >> 1));
        int cy = static_cast<int>(pos[1] + static_cast<float>(size >> 1));
        if (cx < 0) cx = 0;
        if (size <= cx) cx = size - 1;
        if (cy < 0) cy = 0;
        if (size <= cy) cy = size - 1;
        uint32_t cemCell = static_cast<uint32_t>(size * cy + cx);
        int dist = Pf_IsDstReachable_f810(map, citizenCell, cemCell, 1);
        if (dist < 0) {
            int dy = static_cast<int>(citizenCell) / size - static_cast<int>(cemCell) / size;
            int dx = static_cast<int>(citizenCell) % size - static_cast<int>(cemCell) % size;
            dist = static_cast<int>(sqrtf(static_cast<float>(dy * dy + dx * dx))) + size;
        }
        if (dist < best
            && (WorkBase_ValidateCell_a00f0(map, 0xe1f, citizenCell, static_cast<int>(cemCell)) & 0xff) != 0) {
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
extern "C" void** ComponentEntity_FindTombstoneComp_4800(void* entityV, void** outIBP) {
    void* found = FindComponentByTypeId(reinterpret_cast<uint8_t*>(entityV),
                                        TombstoneComponent::TypeId());
    new (outIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
    return outIBP;
}

// ---------------------------------------------------------------- 0x140057b20
// Place the tombstone at its grave cell: stamp the position component
// (x,y at +0x28/+0x2a, extent 0x10001 at +0x2c, yaw degrees normalized to
// [0,360) at +0x30) and set the model component's transform (+0x148 quat+pos,
// +0x168 yaw) from Transform::FromYawPitchOffset at terrain height.
extern "C" void Entity_PlaceAtCell_7b20(void* tombCompV, int cell, float yawDeg) {
    uint8_t* self = reinterpret_cast<uint8_t*>(tombCompV);
    uint8_t* world = g_pGameWorldSingleton ? reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) : nullptr;
    if (!world || !self) return;
    int* map = *reinterpret_cast<int**>(world + 0xf8);
    int size = *map;
    uintptr_t entity = *reinterpret_cast<uintptr_t*>(self + 0x10);
    int64_t posIBP = 0;
    FloorComponent_FindResData(reinterpret_cast<void*>(entity), &posIBP);
    int64_t modelIBP = 0;
    ActCNR_FindComponent_3f60(entity, &modelIBP);
    if (!posIBP || !modelIBP) return;   // [STANDALONE GUARD] retail derefs unguarded
    int yaw = static_cast<int>(yawDeg);
    *reinterpret_cast<int16_t*>(posIBP + 0x28) = static_cast<int16_t>(cell % size);
    *reinterpret_cast<int16_t*>(posIBP + 0x2a) = static_cast<int16_t>(cell / size);
    *reinterpret_cast<uint32_t*>(posIBP + 0x2c) = 0x10001;
    *reinterpret_cast<int*>(posIBP + 0x30) = yaw;
    if (yaw < 0) {
        do { yaw += 360; } while (yaw < 0);
        *reinterpret_cast<int*>(posIBP + 0x30) = yaw;
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
    *reinterpret_cast<Math::Transform*>(modelIBP + 0x148) = xform;   // B.3.1 bridge (7-dword copy)
    *reinterpret_cast<float*>(modelIBP + 0x168) = yawDeg;
    reinterpret_cast<Graphics::Geometry*>(modelIBP + 0x28)
        ->SetTransform(*reinterpret_cast<const Math::Transform*>(modelIBP + 0x148));   // B.3.5 bridge
}

// ---------------------------------------------------------------- 0x140046a90
// Claim the grave cell and spawn the tombstone entity there.
// cemComp = cemetery component, cell = grave cell index, citizenCell unused
// (matches retail), worldArg = game-world singleton (passed to the pathfinder cmd).
extern "C" void Cemetery_SpawnTombstone_6a90(void* cemCompV, int cell,
                                             uint64_t /*citizenCell*/, void* worldArg) {
    uint8_t* cem = reinterpret_cast<uint8_t*>(cemCompV);
    uint8_t* world = g_pGameWorldSingleton ? reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) : nullptr;
    if (!world || !cem) return;
    int64_t nullIBP = 0;
    new (&nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    uint8_t* map = *reinterpret_cast<uint8_t**>(world + 0xf8);
    int64_t cellRec = *reinterpret_cast<int64_t*>(map + 0x80);
    uint8_t oldState = *reinterpret_cast<uint8_t*>(cellRec + 0x29 + static_cast<int64_t>(cell) * 0x30);
    bool stateWasWalk = ((1u << (oldState & 0x1f)) & 0x1e0u) != 0;   // old state 5..8
    *reinterpret_cast<uint8_t*>(map + 0xa2) |= static_cast<uint8_t>(stateWasWalk);
    *reinterpret_cast<uint8_t*>(cellRec + 0x29 + static_cast<int64_t>(cell) * 0x30) = 2;
    int size = *reinterpret_cast<int*>(map);
    if (stateWasWalk)
        Pf_FineCellDirty_a8c90(*reinterpret_cast<int64_t*>(map + 0x70), cell % size, cell / size);
    Pf_EnqueueCmd2_a6fe0(*reinterpret_cast<int64_t*>(map + 0x68), static_cast<uint32_t>(cell),
                         reinterpret_cast<void*>(2), worldArg);
    *reinterpret_cast<int64_t*>(cellRec + 8 + static_cast<int64_t>(cell) * 0x30) = nullIBP;  // = null
    // Tombstone yaw = the cemetery's own placed rotation (position component +0x30).
    int64_t resIBP = 0;
    void* resIBPp = FloorComponent_FindResData(*reinterpret_cast<void**>(cem + 0x10), &resIBP);
    int rot = (resIBPp && *reinterpret_cast<int64_t*>(resIBPp))
        ? *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(resIBPp) + 0x30) : 0;
    // Tombstone description lives at CemeteryDescription+0x20.
    void* desc = *reinterpret_cast<void**>(cem + 0x18);
    void* cemDesc = (desc
                     && Object::IsDerivedFromNullSafe(desc, CemeteryDescription::TypeId())) ? desc : nullptr;
    // [STANDALONE GUARD] retail trusts the IsDerivedFrom gate then derefs
    // unguarded; type-ids can be unfilled early in standalone, so trust the
    // raw description (mirrors tombstonecomponent.cpp's OnCreate note).
    if (!cemDesc) cemDesc = desc;
    if (!cemDesc) return;
    void* tombDesc = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(cemDesc) + 0x20);
    if (!tombDesc) return;   // [STANDALONE GUARD]
    void* built = nullptr;
    void* mem = System::MemoryAllocator::MemAlloc(0x48, 8);
    if (mem) built = ComponentEntity_CtorFromDescription(static_cast<ComponentEntity*>(mem), tombDesc);
    int64_t entIBP = 0;
    new (&entIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(built));   // B.3.3 bridge
    if (!entIBP) return;   // [STANDALONE GUARD] alloc/ctor failed
    int64_t tsIBP = 0;
    ComponentEntity_FindTombstoneComp_4800(reinterpret_cast<void*>(entIBP),
                                           reinterpret_cast<void**>(&tsIBP));
    if (tsIBP) Entity_PlaceAtCell_7b20(reinterpret_cast<void*>(tsIBP), cell, static_cast<float>(rot));
    int64_t regIBP = entIBP;
    EntityList_RegisterEntity_Impl(*reinterpret_cast<void**>(world + 0xe8), &regIBP,
                                   *reinterpret_cast<int*>(reinterpret_cast<int64_t>(tombDesc) + 0x20));
}

// ---------------------------------------------------------------- 0x140116dd0
// Citizen death handler. Returns 1 (handled) or 0 when the world is in the
// bit-8 state at +0xd0 (loading/shutdown) — retail early-outs with AL=0 there.
extern "C" uint64_t CitizenComponent_HandleLifeEvent(void* selfV, int eventType, const char* strParam) {
    uint8_t* self = reinterpret_cast<uint8_t*>(selfV);
    uint8_t* world = g_pGameWorldSingleton ? reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) : nullptr;
    if (!world) return 0;   // [STANDALONE GUARD]
    if ((*reinterpret_cast<uint32_t*>(world + 0xd0) >> 8 & 1) != 0) return 0;

    // --- Death position: workplace (+0x238) if employed, else home (+0x278) ---
    float pos[3] = {0.0f, 0.0f, 0.0f};
    if (*reinterpret_cast<int64_t*>(self + 0x238) == 0) {
        uint8_t* home = *reinterpret_cast<uint8_t**>(self + 0x278);
        if (home) {   // [STANDALONE GUARD] retail derefs unguarded
            uint8_t* homeEnt = *reinterpret_cast<uint8_t**>(home + 0x68);
            if (homeEnt) {
                pos[0] = *reinterpret_cast<float*>(homeEnt + 0x158);
                pos[1] = *reinterpret_cast<float*>(homeEnt + 0x15c);
                pos[2] = *reinterpret_cast<float*>(homeEnt + 0x160);
            }
        }
    } else {
        int64_t ibp = 0;
        void* p = WorkBase_FindComponent_2ed0(*reinterpret_cast<uintptr_t*>(self + 0x238), &ibp);
        if (p)
            Pf_EntityResolvePos_2e20(*reinterpret_cast<void**>(p), pos, 0, 0, nullptr);
    }

    // --- Clamp to a map cell ---
    int size = **reinterpret_cast<int**>(world + 0xf8);
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
    const uintptr_t townMgrForTrace = *reinterpret_cast<uintptr_t*>(world + 0xf0);
    const int cemCountForTrace =
        townMgrForTrace ? *reinterpret_cast<int*>(townMgrForTrace + 0x48) : -1;

    void* cem = nullptr;
    Cemetery_FindNearestWithSlot_9740(*reinterpret_cast<void**>(world + 0xf0), &cem, cell);
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
        const int plotsForTrace =
            *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(cem) + 0x50);
        int slot = static_cast<int>(Cemetery_PickFreeSlot_6c60(cem));
        if (slot >= 0) {   // [STANDALONE GUARD] retail indexes unguarded (9740 just proved >= 0)
            int graveCell = *reinterpret_cast<int*>(
                *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(cem) + 0x48)
                + static_cast<int64_t>(slot) * 4);
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

    // --- OR the flag into partner (+0x258), parent (+0x250) + its ring, own ring ---
    if (*reinterpret_cast<int64_t*>(self + 0x258) != 0) {
        *reinterpret_cast<uint32_t*>(*reinterpret_cast<int64_t*>(self + 0x258) + 0x88) |= flags;
    }
    if (*reinterpret_cast<int64_t*>(self + 0x250) != 0) {
        *reinterpret_cast<uint32_t*>(*reinterpret_cast<int64_t*>(self + 0x250) + 0x88) |= flags;
        Citizen_FamilyRingOrFlag_3a70(*reinterpret_cast<void**>(self + 0x250), flags);
    }
    Citizen_FamilyRingOrFlag_3a70(selfV, flags);

    // --- Post the event-log message (severity 1) ---
    CitizenComponent_PostLifeEvent(selfV, eventType, 1, strParam);

    // --- Queue the citizen entity for removal on GameUpdateLoop (+0xd0 list) ---
    int64_t entIBP = *reinterpret_cast<int64_t*>(self + 0x10);
    uint8_t* loop = *reinterpret_cast<uint8_t**>(world + 0xe8);
    if (entIBP != 0 && loop) {
        if ((*reinterpret_cast<uint32_t*>(entIBP + 0x40) >> 10 & 1) == 0) {
            ActHunt_MarkEntityRemoval_c2ea0(reinterpret_cast<void*>(entIBP));
            // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
            GameColl::AsArray<GameColl::Ptr8>(loop + 0xd0)
                .SetSizeFill(*reinterpret_cast<int*>(loop + 0xd8) + 1, &entIBP);
        }
    }
    return 1;
}
