// code/Game/Component/immigrationcomponent_states.cpp
// The IMMIGRATION (nomad arrival) state machine of the town hall — ported 2026-09-05
// for the Immigrants achievement (Ach27, stat 28 = nomads admitted).
//
// ImmigrationComponent (0x68, a ControllerComponent on Template\TownHall.rsc) is the
// town hall's nomad-arrival controller. In retail its behaviour is the state table
// registered once by ImmigrationDescription_Setup -> helper_e6c0 (0x14004e6c0):
//
//   state 2    IDLE        enter d290 / tick d220 : if CanAccept -> 0xf
//                          transitions -> {3, 0xf}
//   state 0xf  WAITING     enter d1a0 : timer = (24 + rand*48) months
//                          tick  d0d0 : timer -= dt; when expired AND month in
//                                       [desc+0x50, desc+0x54] (spring) -> 0x10 if
//                                       CanAccept else back to 2
//                          transitions -> {3, 0x10, 2}
//   state 0x10 ARRIVING    enter d090 : reset + Spawn the group at a map edge
//                          tick  d070 : re-spawn if the group is empty
//                          transitions -> {3 (dismiss on the way out), 0x11}
//                          event 6  cff0 : one nomad reached the hall; all there -> 0x11
//   state 0x11 WAITING-FOR-ANSWER
//                          enter ceb0 : event-log "nomads have arrived" (type 0xe)
//                          tick  ce00 : month >= desc+0x58 (autumn) -> dismiss -> 2
//                          transitions -> {3 (dismiss), 2}
//                          event 7  cd90 : ALLOW  = Accept (convert to citizens) -> 2
//                          event 8  cd20 : DENY   = Dismiss (send them home)     -> 2
//
// The Allow/Deny buttons of the town hall's "Nomads" tab (ImmigrationUI_Slot9,
// immigrationui.cpp) post events 7/8 through the StateMachineComponent node — the
// same node-vtable slot 2 (PostEvent) the Dispatcher below implements.
//
// Component layout (retail ctor 0x14004e640): ImmigrationComponent.h.
// ImmigrationDescription: ImmigrationDescription.h (fields named from their use here).
//
// Opt-in: BANISHED_LIVE_IMMIGRATION (or the global live-components switch). Default off keeps
// the component inert exactly as before this port.
//
// Test fixtures (all env-gated, byte-unchanged when unset — NOT retail behaviour):
//   BANISHED_NOMAD_TRACE=1            stderr trace of every state/event/spawn/accept
//   BANISHED_NOMAD_TIMER_SCALE=<f>    multiply the 2..6-year wait timer (0.001 = now)
//   BANISHED_NOMAD_ANY_SEASON=1       ignore the spring window and the autumn leave
//   BANISHED_NOMAD_AUTOACCEPT=<n>     n ticks after the group is waiting, post the
//                                     ALLOW event (7) through the SM node, exactly
//                                     as the Allow button does; a NEGATIVE n posts
//                                     DENY (event 8) after |n| ticks instead
//
// Faithful offset-by-offset port of 0x14004cd00 .. 0x14004e4b0 and 0x140050030 /
// 0x1400501a0 (nomad -> citizen conversion).
//
// LEFT (B.4 COMP-d): GameUpdateLoop m_citizenList / m_game / m_dayCounter /
// m_entityList are void* (W0 header); the Map's cell record and CitizenBehavior's
// +0x160 tree nodes have no type; StateMachineComponent::controllerSelf is void*
// (COMP-e); PointList / Pf_ / ModelComponent bodies owned by other blocks take int*
// or int64_t*; ModelComponent's MeshInstance / Transform are raw storage (H5).

// graphics/*.h FIRST: they pull runtime/types/volume/Volume.h, whose global-scope
// `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()` once <windows.h> has
// been seen through resolve.h.  ENDGAME B.3.5 caller batch I2.
#include "graphics/Mesh.h"
#include "Component/ResidenceComponent.h"   // B.3.8 CB1a (TypeId)
#include "Component/NomadDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/NomadComponent.h"   // B.3.8 CB1a (TypeId)
#include "Component/InteractDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/InteractComponent.h"
#include "Component/ImmigrationDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/ImmigrationComponent.h"
#include "Component/CitizenDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/CitizenComponent.h"
#include "Component/ModelComponent.h"
#include "Component/StateMachineComponent.h"
#include "Component/ComponentDescription.h"
#include "Entity/ComponentEntity.h"
#include "Entity/CitizenBehavior.h"
#include "Game/ProfessionList.h"
#include <new>
#include "object/External.h"
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "Action/exe_bodies.h"            // Pf_*, ActCNR_NR_SetVariant_1253d0
#include "Component/exe_bodies.h"
#include "Game/exe_bodies.h"              // Reimpl_MapGetTerrainPos, EventLog_AddEvent
#include "Player/exe_bodies.h"            // ToolTile_SetTransformFromYaw_3870
#include "Entity/componententity_funcs.h" // EntityList_RegisterEntity_Impl
#include "Component/coll_bridge.h"
#include "Object/factory_registration.h"
#include "Component/controller_state_table.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

namespace {

// ---------------------------------------------------------------- constants
constexpr float kSecondsPerMonth = 300.0f;   // DAT_140172234
constexpr float kSecondsPerYear  = 3600.0f;  // DAT_14017225c
constexpr float kMonthsPerYear   = 12.0f;    // DAT_1401721b8
constexpr float kHalfCell        = 0.5f;     // DAT_140172114
constexpr float kYawRange        = 180.0f;   // DAT_140172220

// ---------------------------------------------------------------- helpers, by name (B3R)
//   0x140003ea0 FindComponent<StateMachineComponent>
//   0x140122e20 Pf_EntityResolvePos_2e20 (Entity_impl_2e20)   0x140123070 Pf_RecursiveChildSum_123070
//   0x1400c3720 ComponentEntity(ComponentDescription*)
//   0x1400c48a0 EntityList_RegisterEntity_Impl                 0x14009c480 Pf_ZoneRefresh_9c480
//   0x140003870 ToolTile_SetTransformFromYaw_3870              0x1401253d0 ActCNR_NR_SetVariant_1253d0
//   0x140117080 CitizenComponent::Initialize
//   0x140002ed0 FindComponent<InteractComponent> (the engine_funcs global that bound it
//               was misnamed fn_Entity_FindComponent_Position)
// 0x140003110 was Collection::Array<void*>::SetSize(n, fill) monomorphised
// into the exe (ENDGAME B.3.2 step 2) — converted at its call site below.

// Type ids by name, resolved lazily from the live factory tree (the exe .rdata
// globals are not all registered in standalone; see the ImmigrationUI stub note).
uint32_t TidImmDesc()   { static uint32_t t = 0; if (!t) t = ImmigrationDescription::TypeId(); return t; }
uint32_t TidNomadDesc() { static uint32_t t = 0; if (!t) t = NomadDescription::TypeId();       return t; }
uint32_t TidResidence() { static uint32_t t = 0; if (!t) t = ResidenceComponent::TypeId();     return t; }
uint32_t TidCitDesc()   { static uint32_t t = 0; if (!t) t = CitizenDescription::TypeId();     return t; }
// cell 0x1401c7938 (TypeId_InteractDescription_FC) always fell back to
// InteractDescription::TypeId() unconditionally once filled; converting by cell
// (D5 exception) is mechanically equivalent.
uint32_t TidInteractDesc() { static uint32_t t = 0; if (!t) t = InteractDescription::TypeId(); return t; }

// ---------------------------------------------------------------- small accessors
inline GameUpdateLoop* GW() { return GameUpdateLoop::_singleton; }
// LEFT: GameUpdateLoop::m_citizenList is void* (W0 header).
inline CitizenBehavior* TownMgr(GameUpdateLoop* gw) { return reinterpret_cast<CitizenBehavior*>(gw->m_citizenList); }
// LEFT: GameUpdateLoop::m_game is void* (W0 header); the Map functions take int*.
inline int*     Grid(GameUpdateLoop* gw) { return reinterpret_cast<int*>(gw->m_game); }

// desc = comp->_description if IsDerivedFrom(ImmigrationDescription) else null — retail
// dereferences the null; a town hall without its description is a broken template.
ImmigrationDescription* ImmDesc(ImmigrationComponent* comp) {
    Description* d = comp->_description;
    if (d && ::Object::IsDerivedFromNullSafe(d, TidImmDesc())) return static_cast<ImmigrationDescription*>(d);
    static bool warned = false;
    if (!warned) { warned = true; std::fprintf(stderr, "[ImmCtrl] comp=%p has no ImmigrationDescription (desc=%p tid=%08x)\n", static_cast<void*>(comp), static_cast<void*>(d), TidImmDesc()); std::fflush(stderr); }
    return nullptr;
}

// The month of the in-game year: the clock at gw+0x128 holds {year int, Time-in-year}.
// LEFT: GameUpdateLoop::m_dayCounter is void* (W0 header; no DayCounter class yet).
int MonthOfYear(GameUpdateLoop* gw) {
    void* clock = gw->m_dayCounter;
    if (!clock) return 0;
    float secs = reinterpret_cast<const System::Time*>(reinterpret_cast<uint8_t*>(clock) + 8)->AsSeconds();
    int m = static_cast<int>((secs / kSecondsPerYear) * kMonthsPerYear);
    if (m > 11) m = 0;
    return m;
}

// Post an event to an entity's StateMachineComponent through its node vtable slot 2,
// the exact call shape retail uses: node->PostEvent(node, &sm, curIdx, eventId).
void PostEventToEntity(ComponentEntity* entity, int eventId) {
    if (!entity) return;
    StateMachineComponent* smc = entity->FindComponent<StateMachineComponent>();   // 0x140003ea0
    if (!smc) return;
    void* node = smc->controllerSelf;
    if (!node) return;
    // LEFT: StateMachineComponent::controllerSelf is void* (COMP-e's header).
    reinterpret_cast<ControllerComponent::Controller*>(node)->PostEvent(   // slot 2
        &smc->controllerSelf, static_cast<int8_t>(smc->stateFlags >> 8), eventId);
}

}  // namespace

// ============================================================
// CanAccept — port of 0x14004e3f0
// population >= desc+0x28 AND every required template in desc+0x30[0..+0x38) has a
// built instance (townMgr+0x160 BST: key@+0, count@+0x10, left@+0x20, right@+0x28).
// ============================================================
bool ImmigrationComponent::CanAccept() {
    GameUpdateLoop* gw = GW();
    ImmigrationDescription* desc = ImmDesc(this);
    if (!gw || !desc) return false;
    CitizenBehavior* tm = TownMgr(gw);
    if (tm->citizenCount < desc->_minPopulation) return false;
    int req = desc->_requiredBuildingCount;
    if (req < 1) return true;
    // LEFT: CitizenBehavior::field_0x160 is the raw root of a tree with no node type (WORK-a).
    uint64_t* root = reinterpret_cast<uint64_t*>(tm->field_0x160);
    ComponentDescription** keys = desc->_requiredBuildings;
    for (int i = 0; i < req; ++i) {
        uint64_t key = reinterpret_cast<uint64_t>(keys[i]);   // LEFT: the node key is a raw word
        uint64_t* n = root;
        while (n) {
            if (key < n[0])       n = reinterpret_cast<uint64_t*>(n[4]);
            else if (key <= n[0]) break;
            else                  n = reinterpret_cast<uint64_t*>(n[5]);
        }
        if (!n || *reinterpret_cast<int*>(&n[2]) == 0) return false;
    }
    return true;
}

// ============================================================
// FindSpawnCell — port of 0x14004e270
// Up to 50 tries: a random cell within desc+0x5c of (x, y) that is walkable
// (flag bit in 0xe07) and — after a zone refresh — in the same pathing zone
// (tile+0x26) as the town hall's cell, or on a zone-less bridge/road class (0x1e0).
// ============================================================
int ImmigrationComponent::FindSpawnCell(int x, int y, int hallCell) {
    GameUpdateLoop* gw = GW();
    ImmigrationDescription* desc = ImmDesc(this);
    if (!gw || !desc) return -1;
    System::Random* rng = gw->m_random;
    int* grid = Grid(gw);
    const int W = *grid;
    const int reach = desc->_spawnEdgeDistance;
    for (int t = 0; t < 50; ++t) {
        int cx = x + rng->RandTol(static_cast<int32_t>(reach));
        int cy = y + rng->RandTol(static_cast<int32_t>(reach));
        if (cx < 0) cx = 0; else if (cx >= W) cx = W - 1;
        if (cy < 0) cy = 0; else if (cy >= W) cy = W - 1;
        int c = W * cy + cx;
        // LEFT: the Map's 0x30-byte cell record (array at +0x80) has no type yet (GAME-b).
        const uint8_t* tiles = *reinterpret_cast<uint8_t**>(reinterpret_cast<uint8_t*>(grid) + 0x80);
        uint32_t fc = 1u << (tiles[static_cast<uintptr_t>(c) * 0x30 + 0x29] & 0x1f);
        if ((fc & 0xe07u) == 0) continue;
        Pf_ZoneRefresh_9c480(grid);   // 0x14009c480 (RCX only)
        tiles = *reinterpret_cast<uint8_t**>(reinterpret_cast<uint8_t*>(grid) + 0x80);
        const uint8_t* tc = tiles + static_cast<uintptr_t>(c) * 0x30;
        const uint8_t* th = tiles + static_cast<uintptr_t>(hallCell) * 0x30;
        uint32_t mh = 1u << (th[0x29] & 0x1f);
        uint32_t mc = 1u << (tc[0x29] & 0x1f);
        uint32_t u = mc | mh;
        uint32_t u8 = u | 0xe1fu;
        if ((u8 & mc) && (u8 & mh)) {
            if (u & 0x1e0u) return c;
            int16_t zc = *reinterpret_cast<const int16_t*>(tc + 0x26);
            int16_t zh = *reinterpret_cast<const int16_t*>(th + 0x26);
            if (zc != 0 && zh != 0 && zc == zh) return c;
        }
    }
    return -1;
}

// ============================================================
// NomadComponent::Init — port of 0x140050030
// Bind a freshly cloned nomad to its town hall: target (+0x28), spawn position
// (+0x30), meeting position (+0x3c), gender (+0x50); place the model at the spawn
// point with a random yaw, pick the mesh variant and material for the gender.
// ============================================================
void NomadComponent::Init(ComponentEntity* const* hallEntity, const Math::Vector3* spawn,
                          const Math::Vector3* meet) {
    GameUpdateLoop* gw = GW();
    if (!gw) return;
    Description* nd = _description;
    NomadDescription* ndesc = (nd && ::Object::IsDerivedFromNullSafe(nd, TidNomadDesc()))
                                  ? static_cast<NomadDescription*>(nd) : nullptr;
    hall = *hallEntity;
    spawnPoint = *spawn;
    meetPoint = *meet;
    System::Random* rng = gw->m_random;
    gender = static_cast<int32_t>(rng->Rand(2u));

    ModelComponent* model = _entity->FindComponent<ModelComponent>();   // DAT_1401c7a50
    if (!model) return;
    float yaw = rng->RandFloatTol() * kYawRange;
    // LEFT: 0x140003870 / 0x1401253d0 (ModelComponent's transform-from-yaw and mesh
    // select) are other blocks' free bodies taking an integer receiver.
    ToolTile_SetTransformFromYaw_3870(reinterpret_cast<uintptr_t>(model), &spawnPoint, yaw);   // 0x140003870
    ActCNR_NR_SetVariant_1253d0(reinterpret_cast<int64_t*>(model),   // 0x1401253d0
                                static_cast<uint16_t>(gender), 0);
    if (ndesc) {
        Graphics::MaterialInstance* mat = (gender == 0) ? ndesc->_materialGender0 : ndesc->_materialGender1;
        // LEFT: ModelComponent::meshInstance is raw storage (H5).
        reinterpret_cast<Graphics::MeshInstance*>(model->meshInstance)
            ->SetMaterialInstance(0, Graphics::ExternalPtrHandle{mat});
    }
}

// ============================================================
// Spawn — port of 0x14004dbe0
// Pick a map edge, find a walkable edge cell in the hall's pathing zone, size the
// group from the population (5..15%) plus a random share of the empty house slots
// (minimum 2), then clone the nomad template once per member at that edge and
// register each into the world.
// ============================================================
void ImmigrationComponent::Spawn() {
    GameUpdateLoop* gw = GW();
    ImmigrationDescription* desc = ImmDesc(this);
    if (!gw || !desc) return;
    System::Random* rng = gw->m_random;
    int* grid = Grid(gw);
    const int W = *grid;
    const int edge = desc->_spawnEdgeDistance;

    InteractComponent* pc = _entity->FindComponent<InteractComponent>();   // 0x140002ed0

    int x = 0, y = 0, hallCell = 0, cell = -1;
    for (int t = 0; t < 10; ++t) {
        uint32_t r = rng->Rand(4u);
        if (static_cast<int>(r) < 2) {
            x = (r == 0) ? edge : (W - edge) - 1;
            y = static_cast<int>(rng->Rand(static_cast<uint32_t>(W)));
        } else {
            x = static_cast<int>(rng->Rand(static_cast<uint32_t>(W)));
            y = (r == 2) ? edge : (W - edge) - 1;
        }
        float hp[3] = {0, 0, 0};
        Pf_EntityResolvePos_2e20(pc, hp, 0, 0, nullptr);
        int cx = static_cast<int>(hp[0] + static_cast<float>(W >> 1));
        int cy = static_cast<int>(hp[1] + static_cast<float>(W >> 1));
        if (cx < 0) cx = 0;
        if (cx >= W) cx = W - 1;
        if (cy < 0) cy = 0;
        if (cy >= W) cy = W - 1;
        hallCell = cy * W + cx;
        cell = FindSpawnCell(x, y, hallCell);
        if (cell >= 0) break;
    }
    if (cell < 0) {
        return;
    }

    CitizenBehavior* tm = TownMgr(gw);
    const int pop = tm->citizenCount;
    const float fLo = desc->_groupFractionMin;
    const float fRand = rng->RandFloat();
    const float fHi = desc->_groupFractionMax;

    // Empty household slots across every registered house (townMgr+0x20/+0x28).
    int emptySlots = 0;
    {
        int nh = tm->field_0x28;
        ComponentEntity** houses = tm->field_0x20;
        for (int i = 0; i < nh; ++i) {
            ComponentEntity* h = houses[i];
            ResidenceComponent* rc = (h && TidResidence())
                                         ? h->FindComponent<ResidenceComponent>() : nullptr;
            if (!rc) continue;
            int slots = rc->residentSlotCount;
            // LEFT: ResidenceComponent::residentSlots is void* (COMP-e's header; 0x10 stride).
            const uint8_t* arr = reinterpret_cast<const uint8_t*>(rc->residentSlots);
            for (int s = 0; s < slots; ++s)
                if (*reinterpret_cast<const int*>(arr + static_cast<uintptr_t>(s) * 0x10 + 8) == 0) ++emptySlots;
        }
    }
    // Retail calls Rand(emptySlots*2) unguarded; skipped only when the range is 0.
    int extra = (emptySlots > 0) ? static_cast<int>(rng->Rand(static_cast<uint32_t>(emptySlots * 2))) : 0;
    int n = extra + static_cast<int>(static_cast<float>(pop) * (fRand * (fHi - fLo) + fLo));
    if (n < 2) n = 2;


    for (int k = 0; k < n; ++k) {
        int c = FindSpawnCell(x, y, hallCell);
        if (c < 0) c = cell;

        // A random "use point" on the town hall for this nomad to stand at.
        Description* idesc = pc->_description;
        InteractDescription* id = (idesc && ::Object::IsDerivedFromNullSafe(idesc, TidInteractDesc()))
                                      ? static_cast<InteractDescription*>(idesc) : nullptr;
        uint32_t nPts = id ? static_cast<uint32_t>(id->_createNodes.count) : 0;
        int nChildren = pc->useLocationCount;
        for (int ci = 0; ci < nChildren; ++ci) {
            // LEFT: 0x140123070 takes the InteractComponent as an int64_t (ACT's signature).
            nPts += static_cast<uint32_t>(Pf_RecursiveChildSum_123070(
                reinterpret_cast<int64_t>(pc->useLocations[ci]), 2));   // 0x140123070
        }
        uint32_t ptIdx = rng->Rand(static_cast<uint32_t>(nPts));
        Math::Vector3 meet(0.0f, 0.0f, 0.0f);
        Pf_EntityResolvePos_2e20(pc, &meet.x, 2, static_cast<int>(ptIdx), nullptr);

        // Clone the nomad template.
        void* mem = System::MemoryAllocator::MemAlloc(0x48, 8);
        ComponentEntity* ent = mem ? new (mem) ComponentEntity(desc->_nomad) : nullptr;   // 0x1400c3720

        // World position of the spawn cell, snapped to the terrain.
        float half = static_cast<float>(W >> 1) - kHalfCell;
        float cellPos[3] = { static_cast<float>(c % W) - half, static_cast<float>(c / W) - half, 0.0f };
        Math::Vector3 spawn(0.0f, 0.0f, 0.0f);
        Reimpl_MapGetTerrainPos(grid, &spawn.x, cellPos, '\0', nullptr);

        // Bind its NomadComponent to this hall and drop it into the world.
        NomadComponent* nomad = ent ? ent->FindComponent<NomadComponent>() : nullptr;
        if (nomad) nomad->Init(&_entity, &spawn, &meet);   // 0x140050030
        if (ent) {
            int category = ent->_description->_category;
            ComponentEntity* regIBP = ent;
            // LEFT: GameUpdateLoop::m_entityList is void* (W0 header); the forwarder takes it.
            EntityList_RegisterEntity_Impl(gw->m_entityList, &regIBP, category);
        }
        // Append to the group array (+0x58/+0x60/+0x64) — retail inlines this
        // DynArray8 push; the registered resize does the same growth policy.
        int count = groupCount;
        ComponentEntity* pushIBP = ent;
        // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
        GameColl::AsArray<GameColl::Ptr8>(&group).SetSizeFill(count + 1, &pushIBP);
    }
    arrivedCount = 0;
}

// ============================================================
// Dismiss — port of 0x14004d300: event 1 (LEAVE) to every nomad, drop the group.
// ============================================================
void ImmigrationComponent::Dismiss() {
    int n = groupCount;
    ComponentEntity** arr = group;
    for (int i = 0; i < n; ++i) {
        PostEventToEntity(arr[i], 1);
    }
    if (arr) System::MemoryAllocator::MemFree(arr);
    group = nullptr;
    groupCount = 0;
    groupCapacity = 0;
    arrivedCount = 0;
}

// ============================================================
// NomadComponent::ConvertToCitizen — port of 0x1400501a0: clone the citizen template
// for one nomad at the nomad's current position and run the per-citizen deferred init
// (name, age, gender, family link). Returns the new CitizenComponent (out IBP).
// ============================================================
CitizenComponent* NomadComponent::ConvertToCitizen(CitizenComponent** out, CitizenComponent* const* parent,
                                                   int genderArg, char isChild) {
    GameUpdateLoop* gw = GW();
    *out = nullptr;
    if (!gw) return nullptr;
    Description* nd = _description;
    NomadDescription* ndesc = (nd && ::Object::IsDerivedFromNullSafe(nd, TidNomadDesc()))
                                  ? static_cast<NomadDescription*>(nd) : nullptr;
    if (!ndesc) return nullptr;
    System::Random* rng = gw->m_random;

    void* mem = System::MemoryAllocator::MemAlloc(0x48, 8);
    ComponentEntity* ent = mem ? new (mem) ComponentEntity(ndesc->_citizen) : nullptr;   // 0x1400c3720
    *out = ent->FindComponent<CitizenComponent>();
    CitizenComponent* cit = *out;
    if (!cit) return nullptr;
    Description* cd = cit->_description;
    CitizenDescription* cdesc = (cd && ::Object::IsDerivedFromNullSafe(cd, TidCitDesc()))
                                    ? static_cast<CitizenDescription*>(cd) : nullptr;

    // Spawn the citizen where the nomad stands (its model transform translation).
    ModelComponent* model = _entity->FindComponent<ModelComponent>();
    Math::Vector3 pos(0.0f, 0.0f, 0.0f);
    if (model) {
        // LEFT: ModelComponent::transform is raw storage (H5).
        pos = reinterpret_cast<const Math::Transform*>(model->transform)->translation;
    }
    float age;
    float aLo = cdesc ? cdesc->minPregnancyAge : 0.0f;
    float aHi = cdesc ? cdesc->maxPregnancyAge : 0.0f;
    if (isChild == 0) age = rng->RandFloat() * (aHi - aLo) + aLo;
    else              age = rng->RandFloat() * aLo;
    uint32_t r = rng->Rand(1u);
    char flag = (r == 0 || isChild != 0) ? 0 : 1;
    Profession* key = gw->m_professionList->field_0x30;
    cit->Initialize(&pos.x, static_cast<uint32_t>(genderArg), isChild, age, flag,
                    parent ? *parent : nullptr, key);   // 0x140117080

    ComponentEntity* regIBP = ent;
    {
        int category = ndesc->_citizen->_category;
        // LEFT: GameUpdateLoop::m_entityList is void* (W0 header); the forwarder takes it.
        EntityList_RegisterEntity_Impl(gw->m_entityList, &regIBP, category);
    }
    return *out;
}

// ============================================================
// Accept — port of 0x14004d450 (the ALLOW handler's body).
// Seeds the town's disease clock (townMgr+0x1a8 chance, +0x1a0 duration), adds the
// group to the NOMADS-ADMITTED counter at townMgr+0x1b0 (achievement stat 28), then
// converts the group in fours: adult (gender 1), adult (gender 0) married to it,
// and two children of the second adult; every converted nomad gets event 0
// (DESTROY). Finally frees the group array.
// ============================================================
void ImmigrationComponent::Accept() {
    GameUpdateLoop* gw = GW();
    ImmigrationDescription* desc = ImmDesc(this);
    if (!gw || !desc) return;
    CitizenBehavior* tm = TownMgr(gw);

    float chance = desc->_diseaseChance;
    if (tm->field_0x1A8 < chance) tm->field_0x1A8 = chance;
    System::Time dur;
    dur._ticks = 0;
    RtSystem_Time_Ctor(&dur, static_cast<float>(desc->_diseaseMonths) * kSecondsPerMonth);
    if (tm->field_0x1A0 < dur._ticks) tm->field_0x1A0 = dur._ticks;

    const int n = groupCount;
    TownMgr(gw)->field_0x1B0 += n;

    ComponentEntity** arr = group;
    auto nomadAt = [&](int i) -> ComponentEntity* {
        return arr[i];
    };
    auto convert = [&](int i, CitizenComponent* const* parent, int gender, char child) -> CitizenComponent* {
        ComponentEntity* ent = nomadAt(i);
        NomadComponent* nomad = ent ? ent->FindComponent<NomadComponent>() : nullptr;
        CitizenComponent* cit = nullptr;
        nomad->ConvertToCitizen(&cit, parent, gender, child);   // 0x1400501a0 (unguarded, as the tree's)
        PostEventToEntity(nomadAt(i), 0);
        return cit;
    };

    int genderC = 0;
    for (int i = 0; i < n; ) {
        CitizenComponent* nullIBP = nullptr;
        CitizenComponent* a = convert(i, &nullIBP, 1, 0);
        if (i + 1 >= n) break;
        CitizenComponent* nullIBP2 = nullptr;
        CitizenComponent* b = convert(i + 1, &nullIBP2, 0, 0);
        if (b) b->spouse = a;
        if (a) a->spouse = b;
        if (i + 2 >= n) break;
        CitizenComponent* parent = b;
        convert(i + 2, &parent, genderC, 1);
        if (i + 3 >= n) break;
        int genderD = (genderC == 0) ? 1 : 0;
        CitizenComponent* parent2 = b;
        convert(i + 3, &parent2, genderD, 1);
        genderC = (genderD == 0) ? 1 : 0;
        i += 4;
    }
    if (arr) System::MemoryAllocator::MemFree(arr);
    group = nullptr;
    groupCount = 0;
    groupCapacity = 0;
    arrivedCount = 0;
}

// ============================================================
// State handlers (the controller hands them this component and its StateMachine)
// ============================================================

// state 2 IDLE — enter 0x14004d290 (returns 0), tick 0x14004d220 (returns 1)
char ImmigrationComponent::EnterIdle(void* sm) {
    if (CanAccept()) ::ControllerLive::RequestStateTransition(sm, 0xf);
    return 0;
}
char ImmigrationComponent::TickIdle(void* sm) {
    if (CanAccept()) ::ControllerLive::RequestStateTransition(sm, 0xf);
    return 1;
}

// state 0xf WAITING — enter 0x14004d1a0: timer = (base + rand*range) months
char ImmigrationComponent::EnterWaiting() {
    GameUpdateLoop* gw = GW();
    ImmigrationDescription* desc = ImmDesc(this);
    if (!gw || !desc) return 0;
    float r = gw->m_random->RandFloat();
    float months = r * desc->_waitRandomMonths + desc->_waitMonths;
    float secs = months * kSecondsPerMonth;
    System::Time t;
    t._ticks = 0;
    RtSystem_Time_Ctor(&t, secs);
    waitTimer = t;
    return 0;
}
// tick 0x14004d0d0: countdown; in season -> 0x10 if CanAccept else 2
char ImmigrationComponent::TickWaiting(void* sm, void* time) {
    GameUpdateLoop* gw = GW();
    ImmigrationDescription* desc = ImmDesc(this);
    if (!gw || !desc) return 1;
    // LEFT: the controller hands the tick its elapsed System::Time as void*.
    waitTimer._ticks -= time ? reinterpret_cast<const System::Time*>(time)->_ticks : 0;
    int month = MonthOfYear(gw);
    bool inSeason = (desc->_arrivalMonthFirst <= month && month <= desc->_arrivalMonthLast);
    if (waitTimer._ticks < 1 && inSeason) {
        bool ok = CanAccept();
        ::ControllerLive::RequestStateTransition(sm, ok ? 0x10 : 2);
    }
    return 1;
}

// state 0x10 ARRIVING — enter 0x14004d090: reset the group and spawn it
char ImmigrationComponent::EnterArriving() {
    arrivedCount = 0;
    ComponentEntity** old = group;
    if (old) System::MemoryAllocator::MemFree(old);
    group = nullptr;
    groupCount = 0;
    groupCapacity = 0;
    Spawn();   // 0x14004dbe0
    return 1;
}
// tick 0x14004d070: spawn again if the group is empty
char ImmigrationComponent::TickArriving() {
    if (groupCount == 0) Spawn();   // 0x14004dbe0
    return 1;
}
// event 6 0x14004cff0: a nomad reached the hall
char ImmigrationComponent::OnArrived(void* sm) {
    int32_t arrived = ++arrivedCount;
    if (arrived == groupCount) ::ControllerLive::RequestStateTransition(sm, 0x11);
    return 1;
}
// transition 0x10/0x11 -> 3 handler 0x14004cd00: dismiss on the way out
void ImmigrationComponent::OnLeaveGroup() {
    Dismiss();   // 0x14004d300
}

// state 0x11 WAITING FOR AN ANSWER — enter 0x14004ceb0: the event-log line
char ImmigrationComponent::EnterAnswer() {
    GameUpdateLoop* gw = GW();
    if (gw) {
        ComponentEntity* ownerIBP = _entity;
        EventLog_AddEvent(gw->m_eventNotifier, 0xe, 2, &ownerIBP);
    }
    return 1;
}
// tick 0x14004ce00: past the leave month -> dismiss -> 2
char ImmigrationComponent::TickAnswer(void* sm) {
    GameUpdateLoop* gw = GW();
    ImmigrationDescription* desc = ImmDesc(this);
    if (!gw || !desc) return 1;
    if (desc->_leaveMonth <= MonthOfYear(gw)) {
        Dismiss();   // 0x14004d300
        ::ControllerLive::RequestStateTransition(sm, 2);
    }
    return 1;
}
// event 7 0x14004cd90: ALLOW
char ImmigrationComponent::OnAllow(void* sm) {
    Accept();   // 0x14004d450
    ::ControllerLive::RequestStateTransition(sm, 2);
    return 1;
}
// event 8 0x14004cd20: DENY
char ImmigrationComponent::OnDeny(void* sm) {
    Dismiss();   // 0x14004d300
    ::ControllerLive::RequestStateTransition(sm, 2);
    return 1;
}

// 0x1400a9ac0 — request a transition on a StateMachine (shared idiom).
extern "C" void Imm_RequestStateTransition_9ac0(void* sm, int stateId) {
    ::ControllerLive::RequestStateTransition(sm, stateId);
}

// ============================================================
// Dispatcher + table (replicates ImmigrationDescription_helper_e6c0, 0x14004e6c0)
// ============================================================
// ENDGAME B.3.8 MI-A: the tag lives with the class (its controller member is
// ControllerLive::TableController<ImmigrationCtrlTag>, ImmigrationComponent.h).
#include "Component/ImmigrationComponent.h"
using ImmigrationDispatch = ::ControllerLive::Dispatcher<ImmigrationCtrlTag>;

// The table (controller_state_table.h) holds plain handler addresses called as
// handler([node+0x18], sm[, time]): [node+0x18] is the controller's back-pointer, this
// component, so each entry is a one-line adapter that calls the member.
namespace {
char ImmEnterIdle(ImmigrationComponent* c, void* sm)                { return c->EnterIdle(sm); }        // 0x14004d290
char ImmTickIdle(ImmigrationComponent* c, void* sm, void*)          { return c->TickIdle(sm); }         // 0x14004d220
char ImmEnterWaiting(ImmigrationComponent* c, void*)                { return c->EnterWaiting(); }       // 0x14004d1a0
char ImmTickWaiting(ImmigrationComponent* c, void* sm, void* time)  { return c->TickWaiting(sm, time); } // 0x14004d0d0
char ImmEnterArriving(ImmigrationComponent* c, void*)               { return c->EnterArriving(); }      // 0x14004d090
char ImmTickArriving(ImmigrationComponent* c, void*, void*)         { return c->TickArriving(); }       // 0x14004d070
char ImmOnArrived(ImmigrationComponent* c, void* sm)                { return c->OnArrived(sm); }        // 0x14004cff0
void ImmOnLeaveGroup(ImmigrationComponent* c)                       { c->OnLeaveGroup(); }              // 0x14004cd00
char ImmEnterAnswer(ImmigrationComponent* c, void*)                 { return c->EnterAnswer(); }        // 0x14004ceb0
char ImmTickAnswer(ImmigrationComponent* c, void* sm, void*)        { return c->TickAnswer(sm); }       // 0x14004ce00
char ImmOnAllow(ImmigrationComponent* c, void* sm)                  { return c->OnAllow(sm); }          // 0x14004cd90
char ImmOnDeny(ImmigrationComponent* c, void* sm)                   { return c->OnDeny(sm); }           // 0x14004cd20
}  // namespace

extern "C" void ImmigrationCtrl_PopulateTable() {
    using namespace ::ControllerLive;
    StateTable& t = ImmigrationDispatch::table;
    if (t.count != 0) return;
    // sub_7ba0(id, ENTER, p3, p4, TICK) -> {id, enter@+0x08, trans38@+0x18}
    TableRegisterState(t, 2, reinterpret_cast<void*>(&ImmEnterIdle), nullptr,
                       reinterpret_cast<void*>(&ImmTickIdle), nullptr);
    TableAddTransition(t, 3, nullptr);
    TableAddTransition(t, 0xf, nullptr);

    TableRegisterState(t, 0xf, reinterpret_cast<void*>(&ImmEnterWaiting), nullptr,
                       reinterpret_cast<void*>(&ImmTickWaiting), nullptr);
    TableAddTransition(t, 3, nullptr);
    TableAddTransition(t, 0x10, nullptr);
    TableAddTransition(t, 2, nullptr);

    TableRegisterState(t, 0x10, reinterpret_cast<void*>(&ImmEnterArriving), nullptr,
                       reinterpret_cast<void*>(&ImmTickArriving), nullptr);
    TableAddTransition(t, 3, reinterpret_cast<void*>(&ImmOnLeaveGroup));
    TableAddTransition(t, 0x11, nullptr);
    TableAddEvent(t, 6, reinterpret_cast<void*>(&ImmOnArrived));

    TableRegisterState(t, 0x11, reinterpret_cast<void*>(&ImmEnterAnswer), nullptr,
                       reinterpret_cast<void*>(&ImmTickAnswer), nullptr);
    TableAddTransition(t, 3, reinterpret_cast<void*>(&ImmOnLeaveGroup));
    TableAddTransition(t, 2, nullptr);
    TableAddEvent(t, 7, reinterpret_cast<void*>(&ImmOnAllow));
    TableAddEvent(t, 8, reinterpret_cast<void*>(&ImmOnDeny));
}
