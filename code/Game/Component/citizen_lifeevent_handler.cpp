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

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cmath>

// ---------------------------------------------------------------- resolved deps
typedef void*    (*fn_FindCompPos_2ed0_t)(uintptr_t comp, void* outIBP);            // 0x140002ed0
typedef void*    (*fn_ResolvePos_2e20_t)(void* comp, float* outVec3, int, int, float*); // 0x140122e20
typedef int      (*fn_PathDist_f810_t)(int* map, uint32_t src, uint32_t dst, char); // 0x14009f810
typedef uint64_t (*fn_ValidateCell_a00f0_t)(int* map, uint32_t kind, uint32_t src, int dst); // 0x1400a00f0
typedef void     (*fn_MarkRemoval_c2ea0_t)(void* entity);                           // 0x1400c2ea0
typedef void     (*fn_ArrayPush_3110_t)(void* arrayHdr, int newCount, void* elem);  // 0x140003110
typedef void     (*fn_FineCellDirty_a8c90_t)(int64_t self, int x, int y);           // 0x1400a8c90
typedef void     (*fn_PfEnqueue_a6fe0_t)(int64_t thread, uint32_t cell, void* cmd, void* world); // 0x1400a6fe0
typedef void*    (*fn_FindResData_4680_t)(uintptr_t entity, void* outIBP);          // 0x140004680
typedef void*    (*fn_CtorFromDesc_c3720_t)(void* self, void* description);         // 0x1400c3720
typedef void     (*fn_RegisterEntity_c48a0_t)(void* entityList, void* entityIBP, int category); // 0x1400c48a0
typedef void*    (*fn_FindModel_3f60_t)(uintptr_t entity, void* outIBP);            // 0x140003f60
typedef float*   (*fn_TerrainPos_9f3e0_t)(void* map, float* outPos3, float* inXY2, char, void*); // 0x14009f3e0
typedef void     (*fn_PostLifeEvent_t)(void* self, int eventType, int severity, const char* s); // 0x140113af0

static fn_FindCompPos_2ed0_t    s_findCompPos    = nullptr;
static fn_ResolvePos_2e20_t     s_resolvePos     = nullptr;
static fn_PathDist_f810_t       s_pathDist       = nullptr;
static fn_ValidateCell_a00f0_t  s_validateCell   = nullptr;
static fn_MarkRemoval_c2ea0_t   s_markRemoval    = nullptr;
static fn_ArrayPush_3110_t      s_arrayPush      = nullptr;
static fn_FineCellDirty_a8c90_t s_fineCellDirty  = nullptr;
static fn_PfEnqueue_a6fe0_t     s_pfEnqueue      = nullptr;
static fn_FindResData_4680_t    s_findResData    = nullptr;
static fn_CtorFromDesc_c3720_t  s_ctorFromDesc   = nullptr;
static fn_RegisterEntity_c48a0_t s_registerEnt   = nullptr;
static fn_FindModel_3f60_t      s_findModel      = nullptr;
static fn_TerrainPos_9f3e0_t    s_terrainPos     = nullptr;
static fn_PostLifeEvent_t       s_postLifeEvent  = nullptr;

// Type-id .rdata globals (value 0 until FillTypeIdGlobals_* after Engine::Startup).
static const uint32_t* s_tid_Interact  = nullptr;   // 0x1401c7900 InteractComponent
static const uint32_t* s_tid_Tombstone = nullptr;   // 0x1401c4a30 TombstoneComponent
static const uint32_t* s_tid_CemDesc   = nullptr;   // 0x1401c40c8 CemeteryDescription

static bool s_resolved = false;
static void EnsureResolved() {
    if (s_resolved) return;
    s_resolved = true;
    s_findCompPos   = reinterpret_cast<fn_FindCompPos_2ed0_t>(ResolveAddr(Module::Exe, 0x140002ed0));
    s_resolvePos    = reinterpret_cast<fn_ResolvePos_2e20_t>(ResolveAddr(Module::Exe, 0x140122e20));
    s_pathDist      = reinterpret_cast<fn_PathDist_f810_t>(ResolveAddr(Module::Exe, 0x14009f810));
    s_validateCell  = reinterpret_cast<fn_ValidateCell_a00f0_t>(ResolveAddr(Module::Exe, 0x1400a00f0));
    s_markRemoval   = reinterpret_cast<fn_MarkRemoval_c2ea0_t>(ResolveAddr(Module::Exe, 0x1400c2ea0));
    s_arrayPush     = reinterpret_cast<fn_ArrayPush_3110_t>(ResolveAddr(Module::Exe, 0x140003110));
    s_fineCellDirty = reinterpret_cast<fn_FineCellDirty_a8c90_t>(ResolveAddr(Module::Exe, 0x1400a8c90));
    s_pfEnqueue     = reinterpret_cast<fn_PfEnqueue_a6fe0_t>(ResolveAddr(Module::Exe, 0x1400a6fe0));
    s_findResData   = reinterpret_cast<fn_FindResData_4680_t>(ResolveAddr(Module::Exe, 0x140004680));
    s_ctorFromDesc  = reinterpret_cast<fn_CtorFromDesc_c3720_t>(ResolveAddr(Module::Exe, 0x1400c3720));
    s_registerEnt   = reinterpret_cast<fn_RegisterEntity_c48a0_t>(ResolveAddr(Module::Exe, 0x1400c48a0));
    s_findModel     = reinterpret_cast<fn_FindModel_3f60_t>(ResolveAddr(Module::Exe, 0x140003f60));
    s_terrainPos    = reinterpret_cast<fn_TerrainPos_9f3e0_t>(ResolveAddr(Module::Exe, 0x14009f3e0));
    s_postLifeEvent = reinterpret_cast<fn_PostLifeEvent_t>(ResolveAddr(Module::Exe, 0x140113af0));
    s_tid_Interact  = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7900));
    s_tid_Tombstone = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4a30));
    s_tid_CemDesc   = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c40c8));
}

static inline void ChkPtr(void* p) {
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p);
}

// Component vtable slot [2] (offset 0x10) = GetTypeId(this).
static inline uint32_t CompTypeId(void* comp) {
    void** vt = *reinterpret_cast<void***>(comp);
    return reinterpret_cast<uint32_t (*)(void*)>(vt[2])(comp);
}

// Scan an entity's component array (+0x30 ptr / +0x38 count) for type-id `tid`,
// IsDerivedFrom-checked — the shared body of 4800/6c60/9740's inner loops.
static void* FindComponentByTypeId(uint8_t* entity, uint32_t tid) {
    void* found = nullptr;
    int n = *reinterpret_cast<int*>(entity + 0x38);
    void** comps = *reinterpret_cast<void***>(entity + 0x30);
    for (int i = 0; i < n; i++) {
        ChkPtr(&comps[i]);
        void* c = comps[i];
        if (c && CompTypeId(c) == tid) {
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, tid)) found = c;
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
        ChkPtr(&cur);
        *reinterpret_cast<uint32_t*>(cur + 0x88) |= flags;
        cur = *reinterpret_cast<uint8_t**>(cur + 0x268);
        ChkPtr(head);
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
    EnsureResolved();
    uint8_t* self = reinterpret_cast<uint8_t*>(cemCompV);
    uint8_t* world = g_pGameWorldSingleton ? reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) : nullptr;
    if (!world) return 0xffffffffu;
    int count = *reinterpret_cast<int*>(self + 0x50);
    if (count <= 0) return 0xffffffffu;
    void* rng = *reinterpret_cast<void**>(world + 0xd8);
    int idx = fn_Random_Rand ? static_cast<int>(fn_Random_Rand(rng, static_cast<uint32_t>(count))) : 0;
    int64_t map = *reinterpret_cast<int64_t*>(world + 0xf8);
    uint32_t tid = s_tid_Tombstone ? *s_tid_Tombstone : 0;
    for (int tries = 0; tries < count; tries++) {
        int cell = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(self + 0x48)
                                           + static_cast<int64_t>(idx) * 4);
        uint8_t* occupant = *reinterpret_cast<uint8_t**>(*reinterpret_cast<int64_t*>(map + 0x80)
                                                         + 8 + static_cast<int64_t>(cell) * 0x30);
        if (!occupant) return static_cast<uint32_t>(idx);      // empty cell — free slot
        ChkPtr(&occupant);
        void* ts = FindComponentByTypeId(occupant, tid);
        int64_t tsIBP = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&tsIBP, ts);
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
    EnsureResolved();
    uint8_t* listMgr = reinterpret_cast<uint8_t*>(listMgrV);
    *out = nullptr;
    uint8_t* world = g_pGameWorldSingleton ? reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) : nullptr;
    if (!world || !listMgr) return out;
    int* map = *reinterpret_cast<int**>(world + 0xf8);
    int best = 0x7fffffff;
    int count = *reinterpret_cast<int*>(listMgr + 0x48);
    uint32_t tidInteract = s_tid_Interact ? *s_tid_Interact : 0;
    for (int i = 0; i < count; i++) {
        void** slot = reinterpret_cast<void**>(*reinterpret_cast<int64_t*>(listMgr + 0x40)
                                               + static_cast<int64_t>(i) * 8);
        ChkPtr(slot);
        uint8_t* comp = reinterpret_cast<uint8_t*>(*slot);
        if (!comp) continue;   // [STANDALONE GUARD]
        uint8_t* entity = *reinterpret_cast<uint8_t**>(comp + 0x10);
        ChkPtr(&entity);
        // The cemetery's map position = its InteractComponent's resolved position.
        void* ic = FindComponentByTypeId(entity, tidInteract);
        int64_t icIBP = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&icIBP, ic);
        ChkPtr(&icIBP);
        float pos[3] = {0.0f, 0.0f, 0.0f};
        if (s_resolvePos) s_resolvePos(reinterpret_cast<void*>(icIBP), pos, 0, 0, nullptr);
        int size = *map;
        int cx = static_cast<int>(pos[0] + static_cast<float>(size >> 1));
        int cy = static_cast<int>(pos[1] + static_cast<float>(size >> 1));
        if (cx < 0) cx = 0;
        if (size <= cx) cx = size - 1;
        if (cy < 0) cy = 0;
        if (size <= cy) cy = size - 1;
        uint32_t cemCell = static_cast<uint32_t>(size * cy + cx);
        int dist = s_pathDist ? s_pathDist(map, citizenCell, cemCell, 1) : -1;
        if (dist < 0) {
            int dy = static_cast<int>(citizenCell) / size - static_cast<int>(cemCell) / size;
            int dx = static_cast<int>(citizenCell) % size - static_cast<int>(cemCell) % size;
            dist = static_cast<int>(sqrtf(static_cast<float>(dy * dy + dx * dx))) + size;
        }
        if (dist < best && s_validateCell
            && (s_validateCell(map, 0xe1f, citizenCell, static_cast<int>(cemCell)) & 0xff) != 0) {
            ChkPtr(slot);
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
    EnsureResolved();
    void* found = FindComponentByTypeId(reinterpret_cast<uint8_t*>(entityV),
                                        s_tid_Tombstone ? *s_tid_Tombstone : 0);
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(outIBP, found);
    else *outIBP = found;
    return outIBP;
}

// ---------------------------------------------------------------- 0x140057b20
// Place the tombstone at its grave cell: stamp the position component
// (x,y at +0x28/+0x2a, extent 0x10001 at +0x2c, yaw degrees normalized to
// [0,360) at +0x30) and set the model component's transform (+0x148 quat+pos,
// +0x168 yaw) from Transform::FromYawPitchOffset at terrain height.
extern "C" void Entity_PlaceAtCell_7b20(void* tombCompV, int cell, float yawDeg) {
    EnsureResolved();
    uint8_t* self = reinterpret_cast<uint8_t*>(tombCompV);
    uint8_t* world = g_pGameWorldSingleton ? reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) : nullptr;
    if (!world || !self) return;
    int* map = *reinterpret_cast<int**>(world + 0xf8);
    int size = *map;
    uintptr_t entity = *reinterpret_cast<uintptr_t*>(self + 0x10);
    ChkPtr(self + 0x10);
    int64_t posIBP = 0;
    if (s_findResData) s_findResData(entity, &posIBP);
    int64_t modelIBP = 0;
    if (s_findModel) s_findModel(entity, &modelIBP);
    if (!posIBP || !modelIBP) return;   // [STANDALONE GUARD] retail derefs unguarded
    ChkPtr(&posIBP);
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
    float terr[3] = {0.0f, 0.0f, 0.0f};
    if (s_terrainPos) s_terrainPos(map, terr, inXY, '\0', nullptr);
    ChkPtr(&modelIBP);
    float pitch = 0.0f;
    float yawF = yawDeg;
    uint8_t xform[80] = {0};
    if (fn_Transform_FromYawPitchOffset) {
        fn_Transform_FromYawPitchOffset(xform, &yawF, &pitch, terr);
        const uint32_t* pv = reinterpret_cast<const uint32_t*>(xform);
        uint32_t* dst = reinterpret_cast<uint32_t*>(modelIBP + 0x148);
        for (int i = 0; i < 7; i++) dst[i] = pv[i];
    }
    *reinterpret_cast<float*>(modelIBP + 0x168) = yawDeg;
    if (fn_Geometry_SetTransform)
        fn_Geometry_SetTransform(reinterpret_cast<void*>(modelIBP + 0x28),
                                 reinterpret_cast<void*>(modelIBP + 0x148));
}

// ---------------------------------------------------------------- 0x140046a90
// Claim the grave cell and spawn the tombstone entity there.
// cemComp = cemetery component, cell = grave cell index, citizenCell unused
// (matches retail), worldArg = game-world singleton (passed to the pathfinder cmd).
extern "C" void Cemetery_SpawnTombstone_6a90(void* cemCompV, int cell,
                                             uint64_t /*citizenCell*/, void* worldArg) {
    EnsureResolved();
    uint8_t* cem = reinterpret_cast<uint8_t*>(cemCompV);
    uint8_t* world = g_pGameWorldSingleton ? reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) : nullptr;
    if (!world || !cem) return;
    int64_t nullIBP = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
    uint8_t* map = *reinterpret_cast<uint8_t**>(world + 0xf8);
    int64_t cellRec = *reinterpret_cast<int64_t*>(map + 0x80);
    uint8_t oldState = *reinterpret_cast<uint8_t*>(cellRec + 0x29 + static_cast<int64_t>(cell) * 0x30);
    bool stateWasWalk = ((1u << (oldState & 0x1f)) & 0x1e0u) != 0;   // old state 5..8
    *reinterpret_cast<uint8_t*>(map + 0xa2) |= static_cast<uint8_t>(stateWasWalk);
    *reinterpret_cast<uint8_t*>(cellRec + 0x29 + static_cast<int64_t>(cell) * 0x30) = 2;
    int size = *reinterpret_cast<int*>(map);
    if (stateWasWalk && s_fineCellDirty)
        s_fineCellDirty(*reinterpret_cast<int64_t*>(map + 0x70), cell % size, cell / size);
    if (s_pfEnqueue)
        s_pfEnqueue(*reinterpret_cast<int64_t*>(map + 0x68), static_cast<uint32_t>(cell),
                    reinterpret_cast<void*>(2), worldArg);
    *reinterpret_cast<int64_t*>(cellRec + 8 + static_cast<int64_t>(cell) * 0x30) = nullIBP;  // = null
    // Tombstone yaw = the cemetery's own placed rotation (position component +0x30).
    ChkPtr(cem + 0x10);
    int64_t resIBP = 0;
    void* resIBPp = s_findResData
        ? s_findResData(*reinterpret_cast<uintptr_t*>(cem + 0x10), &resIBP) : nullptr;
    ChkPtr(resIBPp);
    int rot = (resIBPp && *reinterpret_cast<int64_t*>(resIBPp))
        ? *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(resIBPp) + 0x30) : 0;
    // Tombstone description lives at CemeteryDescription+0x20.
    void* desc = *reinterpret_cast<void**>(cem + 0x18);
    void* cemDesc = (desc && fn_IsDerivedFrom && s_tid_CemDesc
                     && fn_IsDerivedFrom(desc, *s_tid_CemDesc)) ? desc : nullptr;
    // [STANDALONE GUARD] retail trusts the IsDerivedFrom gate then derefs
    // unguarded; type-ids can be unfilled early in standalone, so trust the
    // raw description (mirrors tombstonecomponent.cpp's OnCreate note).
    if (!cemDesc) cemDesc = desc;
    if (!cemDesc) return;
    void* tombDesc = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(cemDesc) + 0x20);
    if (!tombDesc) return;   // [STANDALONE GUARD]
    void* built = nullptr;
    void* mem = (fn_MemAlloc && g_pMemAllocSingleton)
        ? fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8) : nullptr;
    if (mem && s_ctorFromDesc) built = s_ctorFromDesc(mem, tombDesc);
    int64_t entIBP = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&entIBP, built);
    ChkPtr(&entIBP);
    if (!entIBP) return;   // [STANDALONE GUARD] alloc/ctor failed
    int64_t tsIBP = 0;
    ComponentEntity_FindTombstoneComp_4800(reinterpret_cast<void*>(entIBP),
                                           reinterpret_cast<void**>(&tsIBP));
    ChkPtr(&tsIBP);
    if (tsIBP) Entity_PlaceAtCell_7b20(reinterpret_cast<void*>(tsIBP), cell, static_cast<float>(rot));
    int64_t regIBP = entIBP;
    if (s_registerEnt)
        s_registerEnt(*reinterpret_cast<void**>(world + 0xe8), &regIBP,
                      *reinterpret_cast<int*>(reinterpret_cast<int64_t>(tombDesc) + 0x20));
}

// ---------------------------------------------------------------- 0x140116dd0
// Citizen death handler. Returns 1 (handled) or 0 when the world is in the
// bit-8 state at +0xd0 (loading/shutdown) — retail early-outs with AL=0 there.
extern "C" uint64_t CitizenComponent_HandleLifeEvent(void* selfV, int eventType, const char* strParam) {
    EnsureResolved();
    uint8_t* self = reinterpret_cast<uint8_t*>(selfV);
    uint8_t* world = g_pGameWorldSingleton ? reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) : nullptr;
    if (!world) return 0;   // [STANDALONE GUARD]
    if ((*reinterpret_cast<uint32_t*>(world + 0xd0) >> 8 & 1) != 0) return 0;

    // --- Death position: workplace (+0x238) if employed, else home (+0x278) ---
    float pos[3] = {0.0f, 0.0f, 0.0f};
    if (*reinterpret_cast<int64_t*>(self + 0x238) == 0) {
        ChkPtr(self + 0x278);
        uint8_t* home = *reinterpret_cast<uint8_t**>(self + 0x278);
        if (home) {   // [STANDALONE GUARD] retail derefs unguarded
            ChkPtr(home + 0x68);
            uint8_t* homeEnt = *reinterpret_cast<uint8_t**>(home + 0x68);
            if (homeEnt) {
                pos[0] = *reinterpret_cast<float*>(homeEnt + 0x158);
                pos[1] = *reinterpret_cast<float*>(homeEnt + 0x15c);
                pos[2] = *reinterpret_cast<float*>(homeEnt + 0x160);
            }
        }
    } else {
        ChkPtr(self + 0x238);
        int64_t ibp = 0;
        void* p = s_findCompPos
            ? s_findCompPos(*reinterpret_cast<uintptr_t*>(self + 0x238), &ibp) : nullptr;
        ChkPtr(p);
        if (p && s_resolvePos)
            s_resolvePos(*reinterpret_cast<void**>(p), pos, 0, 0, nullptr);
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
        ChkPtr(&cem);
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
        ChkPtr(self + 0x258);
        *reinterpret_cast<uint32_t*>(*reinterpret_cast<int64_t*>(self + 0x258) + 0x88) |= flags;
    }
    if (*reinterpret_cast<int64_t*>(self + 0x250) != 0) {
        ChkPtr(self + 0x250);
        *reinterpret_cast<uint32_t*>(*reinterpret_cast<int64_t*>(self + 0x250) + 0x88) |= flags;
        ChkPtr(self + 0x250);
        Citizen_FamilyRingOrFlag_3a70(*reinterpret_cast<void**>(self + 0x250), flags);
    }
    Citizen_FamilyRingOrFlag_3a70(selfV, flags);

    // --- Post the event-log message (severity 1) ---
    if (s_postLifeEvent) s_postLifeEvent(selfV, eventType, 1, strParam);

    // --- Queue the citizen entity for removal on GameUpdateLoop (+0xd0 list) ---
    int64_t entIBP = *reinterpret_cast<int64_t*>(self + 0x10);
    uint8_t* loop = *reinterpret_cast<uint8_t**>(world + 0xe8);
    if (entIBP != 0 && loop) {
        ChkPtr(&entIBP);
        if ((*reinterpret_cast<uint32_t*>(entIBP + 0x40) >> 10 & 1) == 0) {
            ChkPtr(&entIBP);
            if (s_markRemoval) s_markRemoval(reinterpret_cast<void*>(entIBP));
            if (s_arrayPush)
                s_arrayPush(loop + 0xd0, *reinterpret_cast<int*>(loop + 0xd8) + 1, &entIBP);
        }
    }
    return 1;
}
