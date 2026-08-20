// code/Game/Component/pasturecomponent_states.cpp
// Animal-spawn wave / pasture lane — the PastureComponent CONTROLLER state machine.
//
// The pasture's behaviour is not in its own Update: PastureComponent is a
// ControllerComponent whose embedded state node (at comp+0x48, back-pointer at
// comp+0x60) is linked and ticked by the entity's StateMachineComponent. The state
// table is built once by PastureDescription_helper_81a0 (0x1401281a0):
//
//   sub_6940(2, helper_70c0, .., sub_67e0)   -> state 2  enter=70c0  perFrame=67e0
//   sub_6a60(3)                              ->   state 2 transitions -> {3}
//   sub_6940(3, helper_6e40, .., 0)          -> state 3  enter=6e40  (no perFrame)
//   sub_6a60(6) sub_6a60(4) sub_6a60(5)      ->   state 3 transitions -> {6,4,5}
//
// sub_6940 writes the record's +0x00 id, +0x08 fnA and +0x18 fnB; the node vtable's
// slot-3 dispatcher (0x140036bf0) tail-calls entry+0x08 and the slot-7 dispatcher
// (0x140036cb0) tail-calls entry+0x18 — i.e. fnA is ENTER and fnB is the PER-FRAME
// handler, exactly the enterHandler / trans38Handler fields of ControllerLive's
// StateTable. sub_6a60 appends {id,0} to the last record's transition list.
//
// The controller's GetDefaultState (primary vtable slot 16, 0x1400026c0) is
// `MOV EAX,2; RET` — the pasture's DEFAULT state is 2, the operational one, so a
// built pasture starts producing as soon as its state machine starts. State 3 is
// the teardown (release every animal back to the world's free-roaming pool and
// unregister the pasture).
//
// ⚠ Return-AL discipline: both ENTER handlers end `& 0xffffffffffffff00` (AL = 0,
// the "walk-stop" tail that lets sibling state nodes still run); the per-frame
// handler returns AL = 1. Ported verbatim — returning 1 from an enter here would
// starve every node behind the pasture.
//
// Faithful offset-by-offset port. Opt-in via BANISHED_LIVE_PASTURE (see the live
// factory in pasturecomponent.cpp); needs BANISHED_LIVE_STATEMACHINE so the node is
// actually linked and ticked.

#include "Component/controller_state_table.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Spawn side (pasturecomponent_spawn.cpp).
extern "C" void Pasture_SpawnLivestock(void* pasture, void* parentAnimalIBP);
extern "C" bool Pasture_AutoAssignType(void* pasture);

// ============================================================
// Resolved helpers
// ============================================================
namespace {

typedef void  (*fn_EraseByValue_t)(void* arrayField, void* valueIBP);         // 0x140002cc0
typedef void  (*fn_ResizeFill_t)(void* arrayField, int newCount, void* val);  // 0x140003110
typedef void* (*fn_Find_t)(int64_t entity, void* outIBP);
typedef void  (*fn_FlagNotify_t)(void* entity);                               // 0x1400c3470
typedef void  (*fn_Helper5ed0_t)(void* animalComp);                           // 0x140105ed0
typedef void  (*fn_Reassign_t)(uintptr_t animalComp);                         // 0x1401240a0
typedef uint64_t (*fn_Capacity_t)(uintptr_t pasture);                         // 0x140128570
typedef void  (*fn_ActivateWorkplace_t)(int64_t wpc, char enable);            // 0x14012fc00
typedef uint64_t (*fn_StampPastureCells_t)(void* grid, int x, int y, int w, int h,
                                           void* tmpl);                       // 0x14009f770
typedef int   (*fn_FootprintCount_t)(int64_t posComp, int kind);              // 0x140123070
typedef void  (*fn_FootprintCell_t)(int64_t posComp, float* out3, int kind,
                                    int idx, float* opt);                     // 0x140122e20
typedef void* (*fn_FindRawMatDesc_t)(int64_t tmpl, void* outIBP);             // 0x140006330
typedef void* (*fn_CellOccupant_t)(int64_t occ, void* outIBP);                // 0x1400223c0
typedef void  (*fn_GridDirty_t)(int64_t fine, int x, int y);                  // 0x1400a8c90
typedef void  (*fn_GridEnqueue_t)(void* thread, int cell, int kind, uint64_t);// 0x1400a6fe0
typedef void  (*fn_PlaceEntityAtCell_t)(int64_t comp, int cell, float yaw);   // 0x140129760
// NAME CORRECTION (2026-08-09): 0x140013320 is NOT a storage finder — engine_funcs.h:797
// documents it as fn_FindComponent_Tracker (type-id .rdata DWORD_1401c4b10 =
// TrackerComponent), and 0x140058fd0 is the generation-aware {template,count} upsert
// into that tracker (ported as ActRm_ArrayUpsert_58fd0). The old "Storage"/"StorageNote"
// names are the FIFTH instance of the documented "misnamed component finders" trap, and
// they are exactly why this call pair sat unused: it reads as optional storage
// bookkeeping when it is in fact the ONLY thing that feeds the production-statistics
// panel (ProductionUI reads TrackerComponent+0x28/+0x38).
typedef void* (*fn_FindTracker_13320_t)(int64_t entity, void* outIBP);        // 0x140013320
typedef void  (*fn_TrackerUpsert_8fd0_t)(int64_t tracker, int64_t tmpl, int n);// 0x140058fd0
typedef uint32_t (*fn_ResourceIndex_t)(void* world, int64_t tmpl);            // 0x140088f90
typedef void* (*fn_FindWorkComp_t)(int64_t entity, void* outIBP);             // 0x140003050
typedef void  (*fn_StopWorkSlot_t)(uintptr_t workComp);                       // 0x14012eb50
typedef void  (*fn_PostWorkSched_t)(uintptr_t workComp, uint32_t code,
                                    char flag, void* workIBP);                // 0x14012ec60
typedef void  (*fn_WorkSetAmount_t)(int64_t comp, uint32_t v);                // 0x140044810
typedef void  (*fn_RegisterEntity_t)(void* entityList, void* entIBP, int cat);// 0x1400c48a0
typedef void* (*fn_EntityCopyCtor_t)(void* mem, int64_t tmpl);                // 0x1400c3720

fn_EraseByValue_t       s_Erase        = nullptr;
fn_ResizeFill_t         s_Append       = nullptr;
fn_Find_t               s_FindMapComp  = nullptr;   // 0x140004680
fn_Find_t               s_FindAnimal   = nullptr;   // 0x140005500
fn_Find_t               s_FindPosition = nullptr;   // 0x140002ed0
fn_Find_t               s_FindWorkplace= nullptr;   // 0x140002e10
fn_FlagNotify_t         s_FlagNotify   = nullptr;
fn_Helper5ed0_t         s_Helper5ed0   = nullptr;
fn_Reassign_t           s_Reassign     = nullptr;
fn_Capacity_t           s_Capacity     = nullptr;
fn_ActivateWorkplace_t  s_ActivateWP   = nullptr;
fn_StampPastureCells_t  s_StampCells   = nullptr;
fn_FootprintCount_t     s_FootprintN   = nullptr;
fn_FootprintCell_t      s_FootprintAt  = nullptr;
fn_FindRawMatDesc_t     s_FindRawDesc  = nullptr;
fn_CellOccupant_t       s_CellOccupant = nullptr;
fn_GridDirty_t          s_GridDirty    = nullptr;
fn_GridEnqueue_t        s_GridEnqueue  = nullptr;
fn_PlaceEntityAtCell_t  s_PlaceAtCell  = nullptr;
fn_FindTracker_13320_t  s_FindTracker  = nullptr;
fn_TrackerUpsert_8fd0_t s_TrackerUpsert= nullptr;
fn_ResourceIndex_t      s_ResIndex     = nullptr;
fn_FindWorkComp_t       s_FindWork     = nullptr;
fn_StopWorkSlot_t       s_StopWork     = nullptr;
fn_PostWorkSched_t      s_PostWork     = nullptr;
fn_WorkSetAmount_t      s_WorkSetAmt   = nullptr;
fn_RegisterEntity_t     s_RegEntity    = nullptr;
fn_EntityCopyCtor_t     s_EntityCopy   = nullptr;

uint32_t s_tidAnimalComp   = 0;   // 0x1401c6f98  AnimalComponent
uint32_t s_tidLivestockDsc = 0;   // 0x1401c79a8  LivestockDescription
uint32_t s_tidRawMatComp   = 0;   // 0x1401c7c80  RawMaterialComponent
uint32_t s_tidWorkComp     = 0;   // 0x1401c3fb0  (work/production component)
// 0x140172204 = 90.0f — the quarter-turn the dropped pile is rotated by.
const float* s_pQuarterTurn = nullptr;

bool s_resolved = false;

template <class T> T R(uintptr_t a) { return reinterpret_cast<T>(ResolveAddr(Module::Exe, a)); }
uint32_t Tid(uintptr_t rdata) {
    uintptr_t g = ResolveAddr(Module::Exe, rdata);
    return g ? *reinterpret_cast<uint32_t*>(g) : 0u;
}

void Resolve() {
    if (s_resolved) return;
    s_resolved = true;
    s_Erase        = R<fn_EraseByValue_t>(0x140002cc0);
    s_Append       = R<fn_ResizeFill_t>(0x140003110);
    s_FindMapComp  = R<fn_Find_t>(0x140004680);
    s_FindAnimal   = R<fn_Find_t>(0x140005500);
    s_FindPosition = R<fn_Find_t>(0x140002ed0);
    s_FindWorkplace= R<fn_Find_t>(0x140002e10);
    s_FlagNotify   = R<fn_FlagNotify_t>(0x1400c3470);
    s_Helper5ed0   = R<fn_Helper5ed0_t>(0x140105ed0);
    s_Reassign     = R<fn_Reassign_t>(0x1401240a0);
    s_Capacity     = R<fn_Capacity_t>(0x140128570);
    s_ActivateWP   = R<fn_ActivateWorkplace_t>(0x14012fc00);
    s_StampCells   = R<fn_StampPastureCells_t>(0x14009f770);
    s_FootprintN   = R<fn_FootprintCount_t>(0x140123070);
    s_FootprintAt  = R<fn_FootprintCell_t>(0x140122e20);
    s_FindRawDesc  = R<fn_FindRawMatDesc_t>(0x140006330);
    s_CellOccupant = R<fn_CellOccupant_t>(0x1400223c0);
    s_GridDirty    = R<fn_GridDirty_t>(0x1400a8c90);
    s_GridEnqueue  = R<fn_GridEnqueue_t>(0x1400a6fe0);
    s_PlaceAtCell  = R<fn_PlaceEntityAtCell_t>(0x140129760);
    s_FindTracker  = R<fn_FindTracker_13320_t>(0x140013320);
    s_TrackerUpsert= R<fn_TrackerUpsert_8fd0_t>(0x140058fd0);
    s_ResIndex     = R<fn_ResourceIndex_t>(0x140088f90);
    s_FindWork     = R<fn_FindWorkComp_t>(0x140003050);
    s_StopWork     = R<fn_StopWorkSlot_t>(0x14012eb50);
    s_PostWork     = R<fn_PostWorkSched_t>(0x14012ec60);
    s_WorkSetAmt   = R<fn_WorkSetAmount_t>(0x140044810);
    s_RegEntity    = R<fn_RegisterEntity_t>(0x1400c48a0);
    s_EntityCopy   = R<fn_EntityCopyCtor_t>(0x1400c3720);
    s_tidAnimalComp   = Tid(0x1401c6f98);
    s_tidLivestockDsc = Tid(0x1401c79a8);
    s_tidRawMatComp   = Tid(0x1401c7c80);
    s_tidWorkComp     = Tid(0x1401c3fb0);
    s_pQuarterTurn    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172204));
}

int s_trace = -1;
bool Trace() {
    if (s_trace < 0) {
        const char* e = std::getenv("BANISHED_PASTURE_TRACE");
        s_trace = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
    }
    return s_trace == 1;
}

inline void CheckPtr(void* p) { if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p); }
inline uint64_t MakeIBP(void* obj) {
    uint64_t r = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&r, obj);
    else r = reinterpret_cast<uint64_t>(obj);
    return r;
}
inline uintptr_t World() {
    return (g_pGameWorldSingleton && *g_pGameWorldSingleton)
             ? reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) : 0;
}
inline uintptr_t GameState() {
    uintptr_t w = World();
    return w ? *reinterpret_cast<uintptr_t*>(w + 0xf0) : 0;
}

// Pasture field accessors (raw offsets, matching the retail decompile).
inline void*&   PastureType(void* p)   { return *reinterpret_cast<void**>((uintptr_t)p + 0x28); }
inline uintptr_t PastureAnimals(void* p){ return *reinterpret_cast<uintptr_t*>((uintptr_t)p + 0x30); }
inline int&     PastureCount(void* p)  { return *reinterpret_cast<int*>((uintptr_t)p + 0x38); }
inline int&     PastureCap(void* p)    { return *reinterpret_cast<int*>((uintptr_t)p + 0x40); }
inline int&     PastureAccum(void* p)  { return *reinterpret_cast<int*>((uintptr_t)p + 0x44); }
inline void*    PastureOwner(void* p)  { return *reinterpret_cast<void**>((uintptr_t)p + 0x10); }

// The component find-loop retail inlines everywhere: walk the entity's component
// IBP array (entity+0x30, count entity+0x38), compare vtable+0x10 (GetTypeId) to
// `tid`, then confirm with IsDerivedFrom.
void* FindCompOnEntity(uintptr_t entity, uint32_t tid) {
    if (!entity || !tid) return nullptr;
    int n = *reinterpret_cast<int*>(entity + 0x38);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(entity + 0x30);
    if (!arr) return nullptr;
    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
        CheckPtr(slot);
        void* comp = *slot;
        if (!comp) continue;
        uint32_t t = reinterpret_cast<uint32_t(*)(void*)>((*(void***)comp)[2])(comp);
        if (t != tid) continue;
        return (fn_IsDerivedFrom && fn_IsDerivedFrom(comp, tid)) ? comp : nullptr;
    }
    return nullptr;
}

// Same loop over a TEMPLATE/description sub-block list (+0x28 array, +0x30 count).
void* FindDescOnTemplate(uintptr_t tmpl, uint32_t tid) {
    if (!tmpl || !tid) return nullptr;
    int n = *reinterpret_cast<int*>(tmpl + 0x30);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(tmpl + 0x28);
    if (!arr) return nullptr;
    for (int i = 0; i < n; ++i) {
        void* d = *reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
        if (!d) continue;
        uint32_t t = reinterpret_cast<uint32_t(*)(void*)>((*(void***)d)[2])(d);
        if (t != tid) continue;
        return (fn_IsDerivedFrom && fn_IsDerivedFrom(d, tid)) ? d : nullptr;
    }
    return nullptr;
}

}  // namespace

// ============================================================
// PastureComponent_Slot14_sub_3cc0 (0x140123cc0) — bind ONE livestock component
// into `pastureIBP` (or, with a null target, release it into the free-roaming pool).
//
//   if (animal->pasture == target) return;                 // already there
//   erase the animal from its current owner list (the world free list at
//       gameState+0x80 when unowned, else oldPasture+0x30) and, when it HAD a
//       pasture, re-run AnimalComponent_helper_5ed0 so its tile/bounds are recomputed
//   animal->pasture = target
//   append it to the new owner list (target+0x30, or the world free list) and, when
//       it now HAS a pasture, FUN_1401240a0 to copy the pasture rect onto the animal
// ============================================================
extern "C" void Pasture_BindAnimal(void* animalComp, void* pastureIBP) {
    Resolve();
    if (!animalComp || !pastureIBP) return;
    CheckPtr(pastureIBP);
    uintptr_t a = reinterpret_cast<uintptr_t>(animalComp);
    void* target = *reinterpret_cast<void**>(pastureIBP);
    void* cur = *reinterpret_cast<void**>(a + 0x28);
    if (cur == target) return;

    uintptr_t gs = GameState();
    uint64_t selfIBP = MakeIBP(animalComp);
    if (cur == nullptr) {
        if (gs && s_Erase) s_Erase(reinterpret_cast<void*>(gs + 0x80), &selfIBP);
    } else {
        CheckPtr(reinterpret_cast<void*>(a + 0x28));
        if (s_Erase) s_Erase(reinterpret_cast<void*>((uintptr_t)cur + 0x30), &selfIBP);
        CheckPtr(reinterpret_cast<void*>(a + 0x10));
        uint64_t animIBP = 0;
        if (s_FindAnimal) s_FindAnimal(*reinterpret_cast<int64_t*>(a + 0x10), &animIBP);
        CheckPtr(&animIBP);
        if (animIBP && s_Helper5ed0) s_Helper5ed0(reinterpret_cast<void*>(animIBP));
    }

    *reinterpret_cast<void**>(a + 0x28) = target;
    uint64_t againIBP = MakeIBP(animalComp);
    if (target) {
        CheckPtr(reinterpret_cast<void*>(a + 0x28));
        uintptr_t t = reinterpret_cast<uintptr_t>(target);
        if (s_Append) s_Append(reinterpret_cast<void*>(t + 0x30),
                               *reinterpret_cast<int*>(t + 0x38) + 1, &againIBP);
        if (s_Reassign) s_Reassign(a);
        if (Trace())
            std::fprintf(stderr, "[Pasture] bind animal=%p -> pasture=%p (count=%d cap=%d)\n",
                         animalComp, target, PastureCount(target), PastureCap(target));
    } else if (gs && s_Append) {
        s_Append(reinterpret_cast<void*>(gs + 0x80),
                 *reinterpret_cast<int*>(gs + 0x88) + 1, &againIBP);
    }
}

// ============================================================
// PastureDescription_helper_sub_7140 (0x140127140) — ADOPT free-roaming livestock.
//
// Runs on the pasture's state-2 enter and every state-2 frame. If the pasture is
// under capacity, it scans the world's free-roaming livestock list (gameState+0x80,
// count +0x88) for animals whose ENTITY was cloned from the same prototype as the
// pasture's assigned type (entity+0x28 == pasture+0x28) and binds them in, stopping
// as soon as the pasture is full.
//
// Retail snapshots every match into a heap array before binding, because binding
// erases from the very list it is walking. We snapshot the same way into a fixed
// buffer, stopping the collection once we already hold more candidates than the
// pasture can take — behaviourally identical, no allocation.
//
// This is the path that turns the "easy start" free-roaming cows/sheep/chickens
// (spawned by the start-config simple pool) into a stocked pasture.
// ============================================================
extern "C" void Pasture_AdoptWanderers(void* pasture) {
    Resolve();
    if (!pasture) return;
    uintptr_t gs = GameState();
    if (!gs) return;
    int freeCount = *reinterpret_cast<int*>(gs + 0x88);
    if (freeCount == 0) return;

    const int capacity = s_Capacity ? static_cast<int>(s_Capacity((uintptr_t)pasture)) : 0;
    if (PastureCount(pasture) >= capacity) return;

    void* wanted = PastureType(pasture);
    uintptr_t freeList = *reinterpret_cast<uintptr_t*>(gs + 0x80);
    if (!freeList) return;

    static const int kMaxCandidates = 64;
    void* candidates[kMaxCandidates];
    int nCand = 0;
    const int needed = capacity - PastureCount(pasture);
    for (int i = 0; i < freeCount && nCand < kMaxCandidates && nCand < needed; ++i) {
        uint64_t compIBP = *reinterpret_cast<uint64_t*>(freeList + (uintptr_t)i * 8);
        CheckPtr(&compIBP);
        if (!compIBP) continue;
        uint64_t ownerIBP = *reinterpret_cast<uint64_t*>(compIBP + 0x10);
        CheckPtr(&ownerIBP);
        if (!ownerIBP) continue;
        if (*reinterpret_cast<void**>(ownerIBP + 0x28) != wanted) continue;
        candidates[nCand++] = reinterpret_cast<void*>(compIBP);
    }
    if (nCand == 0) return;

    for (int i = 0; i < nCand; ++i) {
        uint64_t pastureIBP = MakeIBP(pasture);
        CheckPtr(&candidates[i]);
        Pasture_BindAnimal(candidates[i], &pastureIBP);
        if (PastureCount(pasture) >= capacity) break;
    }
    if (Trace())
        std::fprintf(stderr, "[Pasture] adopt: pasture=%p took %d free-roaming animals "
                     "(count=%d cap=%d)\n", pasture, nCand, PastureCount(pasture), capacity);
}

// ============================================================
// PastureDescription_helper_7370 (0x140127370) — REPRODUCTION.
//
// For each livestock in the pasture, find the AnimalComponent on its entity and test
// the breed bit (flags bit 21 / 0x200000, set by the animal's own behaviour update).
// When set: clear the bit (BTR 0x15), notify the entity of the flag change, and — if
// the pasture holds fewer than capacity+3 animals (retail's `LEA R13D,[RAX+3]`) —
// spawn a calf/lamb/chick with this animal as the parent.
// ============================================================
extern "C" void Pasture_Reproduce(void* pasture) {
    Resolve();
    if (!pasture) return;
    const int limit = (s_Capacity ? static_cast<int>(s_Capacity((uintptr_t)pasture)) : 0) + 3;

    for (int i = 0; i < PastureCount(pasture); ++i) {
        uintptr_t arr = PastureAnimals(pasture);
        if (!arr) return;
        uint64_t* slot = reinterpret_cast<uint64_t*>(arr + (uintptr_t)i * 8);
        CheckPtr(slot);
        if (!*slot) continue;
        uint64_t ownerIBP = *reinterpret_cast<uint64_t*>(*slot + 0x10);
        CheckPtr(&ownerIBP);
        void* animal = FindCompOnEntity(static_cast<uintptr_t>(ownerIBP), s_tidAnimalComp);
        uint64_t animalIBP = MakeIBP(animal);
        CheckPtr(&animalIBP);
        if (!animalIBP) continue;

        uintptr_t an = static_cast<uintptr_t>(animalIBP);
        uint32_t oldFlags = *reinterpret_cast<uint32_t*>(an + 0x20);
        // Breed-chain diagnostic: the latch that sets bit 21 requires +0x40!=0
        // AND flag bit 23 (0x800000, data-driven from the livestock template's
        // Component flags). Log each animal's state so a soak shows which
        // precondition fails when births stay at 0.
        static uint32_t s_breedCheckCalls = 0;
        if (Trace() && (s_breedCheckCalls++ % 4096) == 0) {
            std::fprintf(stderr, "[Pasture] breed-check: animal=%p flags=0x%08x "
                         "sex40=%d age=%.2f\n",
                         reinterpret_cast<void*>(an), oldFlags,
                         (int)*reinterpret_cast<uint8_t*>(an + 0x40),
                         *reinterpret_cast<float*>(an + 0x44));
            std::fflush(stderr);
        }
        if ((oldFlags & 0x200000u) == 0) continue;          // not ready to breed

        uint32_t newFlags = oldFlags & ~0x200000u;
        *reinterpret_cast<uint32_t*>(an + 0x20) = newFlags;
        if (((newFlags ^ oldFlags) & 0xffff) != 0 && *reinterpret_cast<int64_t*>(an + 0x10) != 0) {
            CheckPtr(reinterpret_cast<void*>(an + 0x10));
            if (s_FlagNotify) s_FlagNotify(*reinterpret_cast<void**>(an + 0x10));
        }
        if (PastureCount(pasture) < limit) {
            std::fprintf(stderr, "[Pasture] BIRTH: pasture=%p parent=%p (count=%d limit=%d)\n",
                         pasture, animal, PastureCount(pasture), limit);
            std::fflush(stderr);
            uint64_t parent = animalIBP;
            Pasture_SpawnLivestock(pasture, &parent);
        }
    }
}

// ============================================================
// PastureComponent slot 15 (+0x78 "OnMessage", 0x140127720) — the BREED TAG.
//
// This is the missing link of livestock reproduction. The herder's work chain
// ends in a SendComponentMessage(msg=0) at the pasture (same dispatch the build
// labor credit uses, see buildcomponent.cpp Slot15). On each message the retail
// handler picks ONE RANDOM pastured animal and ORs flag bit 23 (0x800000) into
// its AnimalComponent — the "breed-eligible" mark. AnimalComponent::Update's
// offspring latch requires exactly that bit (plus +0x40 pose byte and adult
// age) before it will arm the offspring timer that sets bit 21, which
// Pasture_Reproduce then consumes to spawn a calf/lamb/chick. NOTHING else in
// the exe ever sets bit 23, so without this slot no livestock can ever breed —
// the reproduction rate is deliberately driven by herdsman labor.
//
// ABI matches BuildComponent's slot 15: (self RCX, int msgCode RDX, rest unused).
// The `(new ^ old) & 0xffff` notify can never fire here (bit 23 is above the
// mask); kept for fidelity like the identical dead test in helper_7400.
// ============================================================
extern "C" void Pasture_MsgReceive(void* pasture, int msg) {
    Resolve();
    if (!pasture || msg != 0) return;
    const int count = PastureCount(pasture);
    if (count == 0) return;
    uintptr_t w = World();
    uintptr_t arr = PastureAnimals(pasture);
    if (!w || !arr || !fn_Random_Rand) return;

    void* rng = *reinterpret_cast<void**>(w + 0xd8);
    uint32_t idx = fn_Random_Rand(rng, static_cast<uint32_t>(count));
    void** slot = reinterpret_cast<void**>(arr + (uintptr_t)idx * 8);
    CheckPtr(slot);
    if (!*slot) return;

    uint64_t entIBP = *reinterpret_cast<uint64_t*>((uintptr_t)*slot + 0x10);
    CheckPtr(&entIBP);
    uint64_t animalIBP = 0;
    if (s_FindAnimal) s_FindAnimal(static_cast<int64_t>(entIBP), &animalIBP);
    CheckPtr(&animalIBP);
    if (!animalIBP) return;

    uintptr_t an = static_cast<uintptr_t>(animalIBP);
    uint32_t oldF = *reinterpret_cast<uint32_t*>(an + 0x20);
    uint32_t newF = oldF | 0x800000u;
    *reinterpret_cast<uint32_t*>(an + 0x20) = newF;
    if (((newF ^ oldF) & 0xffffu) != 0 && *reinterpret_cast<int64_t*>(an + 0x10) != 0) {
        CheckPtr(reinterpret_cast<void*>(an + 0x10));
        if (s_FlagNotify) s_FlagNotify(*reinterpret_cast<void**>(an + 0x10));
    }
    if (Trace() && (oldF & 0x800000u) == 0) {
        std::fprintf(stderr, "[Pasture] breed-tag: pasture=%p animal=%p flags 0x%08x -> 0x%08x\n",
                     pasture, reinterpret_cast<void*>(an), oldF, newF);
        std::fflush(stderr);
    }
}

// ============================================================
// Shared release body — PastureDescription_helper_6e40 (0x140126e40), byte-identical
// to the PastureComponent vtable-slot-7 body at 0x1401277e0.
//
// Walk the pasture's animal list from the BACK, unassigning each animal (pasture ptr
// -> null) so it returns to the world's free-roaming pool, then erase the pasture
// itself from the world pasture registry (gameState+0x90).
//
// ⚠ The loop terminates because the unassign path ERASES the animal from
// pasture+0x30, shrinking pasture+0x38. FUN_140002cc0 / FUN_140003110 are
// DynArray8_EraseByValue / DynArray8_ResizeFill — an earlier port of this function
// mislabelled them "AddToList"/"InsertInList", so nothing ever shrank the list and
// the `while (count >= 1)` spun forever.
// ============================================================
extern "C" void Pasture_ReleaseAllAnimals(void* pasture) {
    Resolve();
    if (!pasture) return;
    uintptr_t gs = GameState();

    int guard = 0;
    while (PastureCount(pasture) >= 1) {
        if (++guard > 4096) {                       // cannot happen in retail; see above
            std::fprintf(stderr, "[Pasture] release loop guard tripped on %p (count=%d)\n",
                         pasture, PastureCount(pasture));
            std::fflush(stderr);
            break;
        }
        uintptr_t arr = PastureAnimals(pasture);
        if (!arr) break;
        uint64_t* slot = reinterpret_cast<uint64_t*>(
            arr + (uintptr_t)(PastureCount(pasture) - 1) * 8);
        CheckPtr(slot);
        void* animal = reinterpret_cast<void*>(*slot);
        if (!animal) break;
        uintptr_t a = reinterpret_cast<uintptr_t>(animal);
        void* cur = *reinterpret_cast<void**>(a + 0x28);
        if (cur == nullptr) break;                  // already free: retail would spin here

        uint64_t ibp = MakeIBP(animal);
        CheckPtr(reinterpret_cast<void*>(a + 0x28));
        if (s_Erase) s_Erase(reinterpret_cast<void*>((uintptr_t)cur + 0x30), &ibp);
        CheckPtr(reinterpret_cast<void*>(a + 0x10));
        void* animalComp = FindCompOnEntity(*reinterpret_cast<uintptr_t*>(a + 0x10),
                                            s_tidAnimalComp);
        uint64_t animalIBP = MakeIBP(animalComp);
        CheckPtr(&animalIBP);
        if (animalIBP && s_Helper5ed0) s_Helper5ed0(reinterpret_cast<void*>(animalIBP));

        *reinterpret_cast<void**>(a + 0x28) = nullptr;
        uint64_t backIBP = MakeIBP(animal);
        if (gs && s_Append)
            s_Append(reinterpret_cast<void*>(gs + 0x80),
                     *reinterpret_cast<int*>(gs + 0x88) + 1, &backIBP);
    }

    uint64_t selfIBP = MakeIBP(pasture);
    if (gs && s_Erase) s_Erase(reinterpret_cast<void*>(gs + 0x90), &selfIBP);
}

// ============================================================
// State 2 ENTER — PastureDescription_helper_70c0 (0x1401270c0).
//   * adopt any matching free-roaming livestock (sub_7140)
//   * append this pasture to the world pasture registry (gameState+0x90/+0x98)
//   * find the owner's WorkPlaceComponent and set its active flag to 0
// Returns AL = 0 (`& 0xff..00`) so sibling state nodes still run.
// ============================================================
extern "C" char Pasture_State2Enter(void* data, void* /*sm*/) {
    Resolve();
    if (!data) return 0;
    std::fprintf(stderr, "[Pasture] state 2 ENTER (operational) pasture=%p type=%p\n",
                 data, PastureType(data));
    std::fflush(stderr);

    Pasture_AdoptWanderers(data);

    uintptr_t gs = GameState();
    uint64_t selfIBP = MakeIBP(data);
    if (gs && s_Append)
        s_Append(reinterpret_cast<void*>(gs + 0x90),
                 *reinterpret_cast<int*>(gs + 0x98) + 1, &selfIBP);

    CheckPtr(reinterpret_cast<void*>((uintptr_t)data + 0x10));
    uint64_t wpc = 0;
    if (s_FindWorkplace) s_FindWorkplace(*reinterpret_cast<int64_t*>((uintptr_t)data + 0x10), &wpc);
    CheckPtr(&wpc);
    if (wpc && s_ActivateWP) s_ActivateWP(static_cast<int64_t>(wpc), 0);

    // Bring-up only, opt-in: retail relies on the pasture dialog to set the type, so
    // an untyped pasture would adopt nothing. No-op unless BANISHED_PASTURE_AUTOTYPE.
    // MUST run AFTER the workplace deactivation above: SetLivestockType fires the
    // slot-14 cmd-2 herder TOGGLE, whose retail invariant is accepting==0 at first
    // type assignment (the dialog can only run once the pasture is operational).
    // With the call before the zeroing, the toggle flipped the builder-path leftover
    // 1 -> 0 ("herder OFF") and the herder never spawned.
    if (PastureType(data) == nullptr) Pasture_AutoAssignType(data);
    return 0;
}

// ============================================================
// State 3 ENTER — PastureDescription_helper_6e40 (0x140126e40): teardown.
// Returns AL = 0.
// ============================================================
extern "C" char Pasture_State3Enter(void* data, void* /*sm*/) {
    if (!data) return 0;
    std::fprintf(stderr, "[Pasture] state 3 ENTER (teardown) pasture=%p count=%d\n",
                 data, PastureCount(data));
    std::fflush(stderr);
    Pasture_ReleaseAllAnimals(data);
    return 0;
}

// ============================================================
// The state-2 PRODUCTION tail of sub_67e0 — split out so a missing helper skips
// production without touching the adopt/reproduce head.
//
//   stamp the footprint cells with the assigned type (FUN_14009f770)
//   livestockDesc = LivestockDescription on the assigned template
//   produced      = livestockDesc+0x20 (the raw-material template it yields)
//   rawDesc       = RawMaterialDescription on `produced`  (FUN_140006330)
//   stock/limit   = gameState+0x170 table, entry stride 0x20, index livestockDesc+0x28
//   while (pasture+0x44 > rawDesc+0x40):
//       pasture+0x44 -= rawDesc+0x40
//       if (stock < limit) drop one pile of `produced` on a free footprint cell
//
// LANDED 2026-08-09: the FUN_140013320 -> CreateRawMaterial_helper_8fd0 tail is now
// called (see the loop below). It is the TrackerComponent upsert, not "storage
// bookkeeping" — it is what the production-statistics panel (ProductionUI) reads, so
// the panel was blank purely because this pair was never called. The pile-quantity
// write (rawComp+0x28 = perUnit) that retail does in the same statement group landed
// with it.
//
// DEFERRED (resolved above so the addresses stay recorded, still not called): retail's
// cell path also RESERVES the target cell when it is occupied — FUN_1400223c0 to read
// the occupant, then tile flag byte +0x29 = 2 plus FUN_1400a8c90 / FUN_1400a6fe0 to
// republish it to the pathfinder — and pokes the work component through FUN_140003050
// -> FUN_140044810 (type 0x1401c3fb0). We only place on an ALREADY-free cell and skip
// that tail.
// ============================================================
namespace {

// DROP-OUTCOME METER (BANISHED_PASTURE_TRACE=1). The accumulator is spent BEFORE the
// three skip tests below, so a pasture can consume production every cycle and still
// place nothing and upsert nothing — which reads on the panel exactly like 'no
// production at all'. Name the arm.
void PastureDropReport(int arm, void* self) {
    if (!Trace()) return;
    static const char* kArms[5] = {"DROPPED", "NO_STOCKTABLE", "STOCK_AT_LIMIT",
                                   "NO_POS_OR_HELPERS", "NO_FREE_CELL"};
    static long long n[5] = {0, 0, 0, 0, 0};
    if ((n[arm]++ % 20) != 0) return;
    std::fprintf(stderr, "[Pasture] produce-drop -> %s pasture=%p (n=%lld)\n",
                 kArms[arm], self, n[arm]);
    std::fflush(stderr);
}

void PastureProduce(void* self) {
    if (!s_FindMapComp || !s_Capacity || !s_FindRawDesc) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    void* tmpl = PastureType(self);
    if (!tmpl) return;
    uintptr_t gs = GameState();
    uintptr_t w = World();
    if (!gs || !w) return;
    int* grid = *reinterpret_cast<int**>(w + 0xf8);
    if (!grid) return;

    CheckPtr(reinterpret_cast<void*>(s + 0x10));
    uint64_t mapIBP = 0;
    s_FindMapComp(*reinterpret_cast<int64_t*>(s + 0x10), &mapIBP);
    CheckPtr(&mapIBP);
    if (!mapIBP) return;
    uintptr_t mc = static_cast<uintptr_t>(mapIBP);
    if (s_StampCells)
        s_StampCells(grid, *reinterpret_cast<int16_t*>(mc + 0x28),
                     *reinterpret_cast<int16_t*>(mc + 0x2a),
                     *reinterpret_cast<int16_t*>(mc + 0x2c),
                     *reinterpret_cast<int16_t*>(mc + 0x2e), tmpl);

    void* lsDesc = FindDescOnTemplate(reinterpret_cast<uintptr_t>(tmpl), s_tidLivestockDsc);
    if (!lsDesc) return;
    int64_t produced = *reinterpret_cast<int64_t*>((uintptr_t)lsDesc + 0x20);
    if (produced == 0) return;

    uint64_t rawDescIBP = 0;
    s_FindRawDesc(produced, &rawDescIBP);
    if (!rawDescIBP) return;
    const int perUnit = *reinterpret_cast<int*>(static_cast<uintptr_t>(rawDescIBP) + 0x40);
    if (perUnit <= 0) return;

    CheckPtr(reinterpret_cast<void*>(s + 0x10));
    uint64_t posIBP = 0;
    if (s_FindPosition) s_FindPosition(*reinterpret_cast<int64_t*>(s + 0x10), &posIBP);

    uintptr_t stockTable = *reinterpret_cast<uintptr_t*>(gs + 0x170);
    const int resIdx = *reinterpret_cast<int*>((uintptr_t)lsDesc + 0x28);

    // ACCUMULATOR METER (BANISHED_PASTURE_TRACE=1). The production loop below is a
    // pure function of pasture+0x44 vs rawDesc+0x40; when the panel shows nothing the
    // first question is whether the accumulator is moving at all (it is credited by
    // LivestockComponent::Update, one tick per pastured animal per interval), not
    // whether the drop path works. Report both numbers, not just the outcome.
    if (Trace()) {
        static long long nProd = 0;
        if ((nProd++ % 600) == 0) {
            std::fprintf(stderr, "[Pasture] produce-check: pasture=%p accum(+0x44)=%d "
                         "perUnit=%d animals=%d\n",
                         self, PastureAccum(self), perUnit, PastureCount(self));
            std::fflush(stderr);
        }
    }

    int rounds = 0;
    while (PastureAccum(self) > perUnit && ++rounds <= 16) {
        PastureAccum(self) -= perUnit;
        if (!stockTable) { PastureDropReport(1, self); continue; }
        const int limit = *reinterpret_cast<int*>(stockTable + (uintptr_t)resIdx * 0x20 + 8);
        const int stock = *reinterpret_cast<int*>(stockTable + (uintptr_t)resIdx * 0x20);
        if (stock >= limit) { PastureDropReport(2, self); continue; }
        if (!posIBP || !s_FootprintN || !s_FootprintAt || !s_EntityCopy) {
            PastureDropReport(3, self);
            continue;
        }

        // Find a footprint cell with no occupant and drop the produce pile there.
        CheckPtr(&posIBP);
        const int nCells = s_FootprintN(static_cast<int64_t>(posIBP), 2);
        int placedCell = -1;
        for (int c = 0; c < nCells && placedCell < 0; ++c) {
            float xy[3] = {0, 0, 0};
            CheckPtr(&posIBP);
            s_FootprintAt(static_cast<int64_t>(posIBP), xy, 2, c, nullptr);
            const int dim = *grid;
            int gx = static_cast<int>(xy[0] + static_cast<float>(dim >> 1));
            int gy = static_cast<int>(xy[1] + static_cast<float>(dim >> 1));
            if (gx < 0) gx = 0;
            if (gx >= dim) gx = dim - 1;
            if (gy < 0) gy = 0;
            if (gy >= dim) gy = dim - 1;
            const int cell = dim * gy + gx;
            // grid is an int*; retail's `*(longlong*)(grid + 0x20)` is byte offset 0x80.
            uintptr_t tiles = *reinterpret_cast<uintptr_t*>(
                reinterpret_cast<uintptr_t>(grid) + 0x80);
            if (!tiles) break;
            uint64_t occ = *reinterpret_cast<uint64_t*>(tiles + (uintptr_t)cell * 0x30 + 8);
            if (occ == 0) { placedCell = cell; break; }
        }
        if (placedCell < 0) {
            PastureDropReport(4, self);
            // OCCUPANT DUMP: name what actually holds every candidate cell. cell+8 is
            // the ENTITY raster; a pasture whose own footprint claim (or its own
            // livestock) sits on every drop cell can never place a pile, and the panel
            // then reads identically to 'no production'.
            if (Trace()) {
                static long long nDump = 0;
                if ((nDump++ % 20) == 0) {
                    uintptr_t tiles = *reinterpret_cast<uintptr_t*>(
                        reinterpret_cast<uintptr_t>(grid) + 0x80);
                    std::fprintf(stderr, "[Pasture] drop-cells: pasture=%p nCells=%d\n",
                                 self, nCells);
                    // nCells comes from Pf_RecursiveChildSum_123070(pos, 2): the kind-2
                    // count on the position component's InteractDescription (+0x50),
                    // summed over child entities. Zero has three different causes -- no
                    // description, a description that fails the InteractDescription cast
                    // (a silently-zeroed seed), or a genuinely empty point list. Print
                    // all three inputs so they can never be confused again.
                    // MEASURED 2026-08-19: a pasture's own InteractDescription is empty
                    // in Pasture.rsc; every create point comes from the AnimalShelter
                    // SECTION, reached through the master's interact CHILD list, which
                    // InteractComponent slot 6 rebuilds from the MultiObject children.
                    // children=0 therefore means the pasture has no sections at all --
                    // i.e. it was spawned by BANISHED_BUILD_DEMO_PASTURE rather than
                    // placed with the fenced tool. Not a defect in this file.
                    {
                        uintptr_t pc = static_cast<uintptr_t>(posIBP);
                        void* desc = pc ? *reinterpret_cast<void**>(pc + 0x18) : nullptr;
                        const char* dn = "<none>";
                        uint32_t dt = 0;
                        if (desc) {
                            dt = reinterpret_cast<uint32_t(*)(void*)>((*(void***)desc)[2])(desc);
                            const char* n = ::Object::GetTypeNameById(dt);
                            if (n) dn = n;
                        }
                        std::fprintf(stderr, "  pos=%p desc=%p type=%s(0x%x) "
                                     "kinds[0/1/2]=%d/%d/%d children=%d\n",
                                     reinterpret_cast<void*>(pc), desc, dn, dt,
                                     desc ? *reinterpret_cast<int*>((uintptr_t)desc + 0x30) : -1,
                                     desc ? *reinterpret_cast<int*>((uintptr_t)desc + 0x40) : -1,
                                     desc ? *reinterpret_cast<int*>((uintptr_t)desc + 0x50) : -1,
                                     pc ? *reinterpret_cast<int*>(pc + 0x30) : -1);
                        // and the point-list external the three arrays are parsed
                        // from (InteractDescription+0x20): an empty child list means
                        // the model carried no named points at all, which is a
                        // different failure from 'no CREATE points'.
                        void* ext = desc ? *reinterpret_cast<void**>((uintptr_t)desc + 0x20)
                                         : nullptr;
                        std::fprintf(stderr, "  pointList ext=%p children=%d\n",
                                     ext,
                                     ext ? *reinterpret_cast<int*>((uintptr_t)ext + 0x38) : -1);
                    }
                    for (int c = 0; c < nCells && c < 8 && tiles; ++c) {
                        float xy[3] = {0, 0, 0};
                        CheckPtr(&posIBP);
                        s_FootprintAt(static_cast<int64_t>(posIBP), xy, 2, c, nullptr);
                        const int dim = *grid;
                        int gx = static_cast<int>(xy[0] + static_cast<float>(dim >> 1));
                        int gy = static_cast<int>(xy[1] + static_cast<float>(dim >> 1));
                        if (gx < 0) gx = 0; if (gx >= dim) gx = dim - 1;
                        if (gy < 0) gy = 0; if (gy >= dim) gy = dim - 1;
                        const int cell = dim * gy + gx;
                        uint64_t occ = *reinterpret_cast<uint64_t*>(
                            tiles + (uintptr_t)cell * 0x30 + 8);
                        const char* what = "?";
                        if (occ) {
                            uintptr_t ent = static_cast<uintptr_t>(occ);
                            int nc = *reinterpret_cast<int*>(ent + 0x38);
                            uintptr_t arr = *reinterpret_cast<uintptr_t*>(ent + 0x30);
                            if (nc > 0 && arr) {
                                void* comp = *reinterpret_cast<void**>(arr);
                                if (comp) {
                                    uint32_t t = reinterpret_cast<uint32_t(*)(void*)>(
                                        (*(void***)comp)[2])(comp);
                                    const char* n = ::Object::GetTypeNameById(t);
                                    if (n) what = n;
                                }
                            }
                        }
                        std::fprintf(stderr, "  cell[%d] idx=%d occ=%p firstComp=%s%s\n",
                                     c, cell, reinterpret_cast<void*>(occ), what,
                                     (occ == (uint64_t)(uintptr_t)PastureOwner(self))
                                         ? "  <- THE PASTURE ITSELF" : "");
                    }
                    std::fflush(stderr);
                }
            }
            break;
        }
        PastureDropReport(0, self);

        void* mem = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8) : nullptr;
        void* ent = mem ? s_EntityCopy(mem, produced) : nullptr;
        if (!ent) break;
        uint64_t entIBP = MakeIBP(ent);
        void* rawComp = FindCompOnEntity(reinterpret_cast<uintptr_t>(ent), s_tidRawMatComp);
        if (rawComp && s_PlaceAtCell) {
            // Position the pile on the cell with a random quarter-turn:
            // CreateRawMaterial_helper_9760(comp, cell, RandTol(rng,2) * 90.0f).
            const float quarter = s_pQuarterTurn ? *s_pQuarterTurn : 90.0f;
            int roll = fn_Random_RandTol ? fn_Random_RandTol(
                           *reinterpret_cast<void**>(w + 0xd8), 2) : 0;
            s_PlaceAtCell(reinterpret_cast<int64_t>(rawComp), placedCell,
                          static_cast<float>(roll) * quarter);
        }
        // Retail writes the pile's quantity onto the RawMaterialComponent right here
        // (0x1401267e0: `*(local_c8+0x28) = *(rawDesc+0x40); *(local_c8+0x30) = 0;`).
        // We were skipping it, so every pile we dropped carried quantity 0 — found
        // while porting the tracker tail below, NOT a pre-existing known gap.
        if (rawComp) {
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(rawComp) + 0x28) = perUnit;
            *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(rawComp) + 0x30) = 0;
        }

        // ---- retail tail: TrackerComponent upsert, BEFORE the town stock bump ----
        // 0x1401267e0:
        //     entity = *(self+0x10);
        //     if (entity) { FUN_140013320(entity,&trackerIBP);
        //                   if (trackerIBP) helper_8fd0(tracker, produced, perUnit); }
        //     idx = FUN_140088f90(<unused>, produced);  // stock bump below
        // This is what fills TrackerComponent+0x38 (the "produced" list) that
        // ProductionUI's panel reads, so without it the production page is blank even
        // when piles are appearing. Same three-call shape as the already-ported
        // ActRm_RegisterRawMaterial_88ef0 (0x140088ef0), which is a useful cross-check.
        CheckPtr(reinterpret_cast<void*>(s + 0x10));
        int64_t pastureEntity = *reinterpret_cast<int64_t*>(s + 0x10);
        if (pastureEntity && s_FindTracker) {
            uint64_t trackerIBP = 0;
            s_FindTracker(pastureEntity, &trackerIBP);
            if (trackerIBP) {
                CheckPtr(&trackerIBP);
                if (s_TrackerUpsert)
                    s_TrackerUpsert(static_cast<int64_t>(trackerIBP), produced, perUnit);
            }
        }

        if (s_RegEntity)
            s_RegEntity(*reinterpret_cast<void**>(w + 0xe8), &entIBP,
                        *reinterpret_cast<int*>(produced + 0x20));
        // NB (pre-existing, unchanged): retail calls RegisterEntity AFTER the stock
        // bump, we call it before. Left as-is rather than reordered under this task.
        // FUN_140088f90's first argument is unused by the callee
        // (ActRm_GetResourceCategory_88f90(void* /*unused*/, entity)), so retail's
        // register-shared `puVar21` — gameWorld, or the entity, or the tracker,
        // depending on which branch ran — is observationally irrelevant; passing the
        // world here is equivalent.
        if (s_ResIndex) {
            uint32_t idx = s_ResIndex(reinterpret_cast<void*>(w), produced);
            if (static_cast<int>(idx) >= 0 && stockTable) {
                int* pStock = reinterpret_cast<int*>(stockTable + (uintptr_t)(int)idx * 0x20 + 0x10);
                *pStock += perUnit;
            }
        }
        std::fprintf(stderr, "[Pasture] produce: pasture=%p resource=%p cell=%d "
                     "(accum now %d)\n", self, (void*)produced, placedCell, PastureAccum(self));
        std::fflush(stderr);
    }
}

}  // namespace

// ============================================================
// State 2 PER-FRAME — PastureDescription_helper_81a0_sub_67e0 (0x1401267e0).
//   sub_7140 (adopt) -> helper_7370 (reproduce) -> production/slaughter tail.
// Returns AL = 1 (retail's `CONCAT71(..,1)`).
// ============================================================
extern "C" char Pasture_State2PerFrame(void* data, void* /*sm*/, void* /*time*/) {
    Resolve();
    if (!data) return 1;
    Pasture_AdoptWanderers(data);
    Pasture_Reproduce(data);
    PastureProduce(data);
    return 1;
}

// ============================================================
// Dispatcher + state table.
// ============================================================
namespace { struct PastureCtrlTag {}; }
using PastureDispatch = ::ControllerLive::Dispatcher<PastureCtrlTag>;

extern "C" void** PastureCtrl_NodeVtable() { return PastureDispatch::Vtable(); }

extern "C" void PastureCtrl_PopulateTable() {
    using namespace ::ControllerLive;
    StateTable& t = PastureDispatch::table;
    if (t.count != 0) return;                       // idempotent
    // state 2 (the controller's DEFAULT state): operational.
    //   enter = helper_70c0, per-frame (trans38) = sub_67e0, transitions -> {3}
    TableRegisterState(t, 2, reinterpret_cast<void*>(&Pasture_State2Enter),
                       nullptr, reinterpret_cast<void*>(&Pasture_State2PerFrame), nullptr);
    TableAddTransition(t, 3, nullptr);
    // state 3: teardown. enter = helper_6e40, no per-frame, transitions -> {6,4,5}
    TableRegisterState(t, 3, reinterpret_cast<void*>(&Pasture_State3Enter),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 6, nullptr);
    TableAddTransition(t, 4, nullptr);
    TableAddTransition(t, 5, nullptr);
    std::fprintf(stderr, "[Pasture] state table populated: %d states (2=operational, "
                 "3=teardown)\n", t.count);
    std::fflush(stderr);
}

// PastureWork factory-with-entity (code/Game/Work/pasturework.cpp): MemAlloc(0x58,8)
// + the retail ctor 0x1400f2d80. `entityRefSlot` points at an 8-byte entity ref.
extern "C" void* PastureWork_CreateForEntity(void* entityRefSlot);

// ============================================================
// PastureComponent slot 13 (0x140127dd0) — capability query for the three
// pasture commands. Shared preamble sums the FREE capacity across every OTHER
// pasture of the same livestock type (world registry gameState+0x90/+0x98).
//
//   cmd 0 "empty the pasture":  our count fits elsewhere (0 < count <= free)
//   cmd 1 "split the herd":     count > 9 AND free >= 5
//   cmd 2 "toggle the herder":  bit 0 = has a livestock type (+0x28 != 0),
//                               bit 1 = workplace currently accepting (+0x50)
//
// The cmd 0/1 results are additionally masked by hasType (uVar5 in retail).
// Slot 14 dispatches this through the vtable and tests only bit 0.
// ============================================================
extern "C" uint64_t Pasture_CanCommand(void* pasture, int cmd) {
    Resolve();
    if (!pasture) return 0;

    int freeElsewhere = 0;
    uintptr_t gs = GameState();
    const int n = gs ? *reinterpret_cast<int*>(gs + 0x98) : 0;
    uintptr_t list = gs ? *reinterpret_cast<uintptr_t*>(gs + 0x90) : 0;
    for (int i = 0; i < n && list; ++i) {
        void* p = *reinterpret_cast<void**>(list + (uintptr_t)i * 8);
        if (!p || p == pasture) continue;
        CheckPtr(&p);
        if (PastureType(p) != PastureType(pasture)) continue;
        freeElsewhere += static_cast<int>(s_Capacity ? s_Capacity((uintptr_t)p) : 0)
                         - PastureCount(p);
    }

    const uint64_t hasType = (PastureType(pasture) != nullptr) ? 1u : 0u;
    if (cmd == 0) {
        const int cnt = PastureCount(pasture);
        return (cnt > 0 && cnt <= freeElsewhere) ? hasType : 0;
    }
    if (cmd == 1) {
        if (PastureCount(pasture) > 9)
            return (freeElsewhere < 5) ? 0 : hasType;
        return 0;
    }
    if (cmd == 2) {
        uint64_t wpcIBP = 0;
        CheckPtr(reinterpret_cast<uint8_t*>(pasture) + 0x10);
        if (s_FindWorkplace)
            s_FindWorkplace(*reinterpret_cast<int64_t*>((uintptr_t)pasture + 0x10), &wpcIBP);
        CheckPtr(&wpcIBP);
        uint64_t r = hasType;
        if (wpcIBP && *reinterpret_cast<char*>(wpcIBP + 0x50) != 0) r |= 2;
        return r;
    }
    return 0;
}

// ============================================================
// PastureComponent slot 14 (0x140127a60) — execute a pasture command.
// Guard: vtable slot 13 (above), bit 0. This is the slot the pasture dialog's
// SetLivestockType invokes with cmd 2 the first time a type is assigned — the
// ORGANIC start of the herder chain:
//
//   cmd 2: toggle the herder. Flip the owner entity's WorkPlaceComponent
//     accepting flag (Entity_impl_fc00). If it lands OFF: stop the entity's
//     work slot (FUN_14012eb50). If it lands ON: MemAlloc(0x58) + the retail
//     PastureWork ctor and post it to the work scheduler
//     (CitizenAI_impl_ec60, code=1) — a citizen then claims it and tends the
//     pasture, whose completed cycles send the slot-15 msg-0 breed tag.
//
//   cmd 0 / cmd 1: transfer animals to the other same-type pastures — all of
//     them (cmd 0) or floor(half) (cmd 1, keep = count - count/2). Fill the
//     pasture with the most free capacity first, then sweep the rest, moving
//     the FIRST animal of our slot array each step via Pasture_BindAnimal
//     (which erases it from our array, so the loop advances).
// ============================================================
extern "C" void Pasture_Command(void* pasture, int cmd) {
    Resolve();
    if (!pasture) return;

    void** vt = *reinterpret_cast<void***>(pasture);
    const uint64_t can = (vt && vt[13])
        ? reinterpret_cast<uint64_t (*)(void*, int)>(vt[13])(pasture, cmd) : 0;
    if ((can & 1) == 0) {
        if (Trace()) {
            std::fprintf(stderr, "[Pasture] command %d refused by slot13 (mask=0x%llx) on %p\n",
                         cmd, (unsigned long long)can, pasture);
            std::fflush(stderr);
        }
        return;
    }

    if (cmd == 2) {
        uint8_t* selfB = reinterpret_cast<uint8_t*>(pasture);
        uint64_t wcIBP = 0, wpcIBP = 0;
        CheckPtr(selfB + 0x10);
        if (s_FindWork)      s_FindWork(*reinterpret_cast<int64_t*>(selfB + 0x10), &wcIBP);
        CheckPtr(selfB + 0x10);
        if (s_FindWorkplace) s_FindWorkplace(*reinterpret_cast<int64_t*>(selfB + 0x10), &wpcIBP);
        if (!wcIBP || !wpcIBP || !s_ActivateWP) {   // standalone guard; retail assumes both
            std::fprintf(stderr, "[Pasture] cmd 2: missing %s%s on %p — herder toggle skipped\n",
                         wcIBP ? "" : "WorkComponent ", wpcIBP ? "" : "WorkPlaceComponent",
                         pasture);
            std::fflush(stderr);
            return;
        }

        CheckPtr(&wpcIBP);
        const char cur = *reinterpret_cast<char*>(wpcIBP + 0x50);
        CheckPtr(&wpcIBP);
        s_ActivateWP(static_cast<int64_t>(wpcIBP), cur == 0 ? 1 : 0);
        CheckPtr(&wpcIBP);
        if (*reinterpret_cast<char*>(wpcIBP + 0x50) == 0) {
            CheckPtr(&wcIBP);
            if (s_StopWork) s_StopWork(static_cast<uintptr_t>(wcIBP));
            CheckPtr(&wpcIBP);
            s_ActivateWP(static_cast<int64_t>(wpcIBP), 0);
            if (Trace()) {
                std::fprintf(stderr, "[Pasture] herder OFF: pasture=%p work slot stopped\n",
                             pasture);
                std::fflush(stderr);
            }
        } else {
            uint64_t entRef = *reinterpret_cast<uint64_t*>(selfB + 0x10);
            void* work = PastureWork_CreateForEntity(&entRef);
            uint64_t workIBP = MakeIBP(work);
            CheckPtr(&wcIBP);
            if (s_PostWork) s_PostWork(static_cast<uintptr_t>(wcIBP), 1, 1, &workIBP);
            CheckPtr(&wpcIBP);
            s_ActivateWP(static_cast<int64_t>(wpcIBP), 1);
            if (Trace()) {
                std::fprintf(stderr, "[Pasture] herder ON: pasture=%p PastureWork=%p posted "
                             "(code=1)\n", pasture, work);
                std::fflush(stderr);
            }
        }
        return;
    }

    // cmd 0 / cmd 1 — transfer animals out.
    uintptr_t gs = GameState();
    const int n = gs ? *reinterpret_cast<int*>(gs + 0x98) : 0;
    uintptr_t list = gs ? *reinterpret_cast<uintptr_t*>(gs + 0x90) : 0;
    const int keep = (cmd != 0)
        ? PastureCount(pasture) - (PastureCount(pasture) >> 1) : 0;

    void* best = nullptr;
    int bestFree = 0;
    for (int i = 0; i < n && list; ++i) {
        void* p = *reinterpret_cast<void**>(list + (uintptr_t)i * 8);
        if (!p || p == pasture) continue;
        CheckPtr(&p);
        if (PastureType(p) != PastureType(pasture)) continue;
        const int freeN = static_cast<int>(s_Capacity ? s_Capacity((uintptr_t)p) : 0)
                          - PastureCount(p);
        if (freeN > bestFree) { bestFree = freeN; best = p; }
    }

    int moved = 0;
    auto transferInto = [&](void* dst) {
        while (keep < PastureCount(pasture)) {
            if (static_cast<int>(s_Capacity ? s_Capacity((uintptr_t)dst) : 0)
                    <= PastureCount(dst)) break;
            uintptr_t arr = PastureAnimals(pasture);
            if (!arr) break;                        // standalone guard
            void** first = reinterpret_cast<void**>(arr);
            CheckPtr(first);
            if (!*first) break;                     // standalone guard
            uint64_t dstRef = reinterpret_cast<uint64_t>(dst);
            Pasture_BindAnimal(*first, &dstRef);
            ++moved;
        }
    };
    if (best) transferInto(best);
    for (int i = 0; i < n && list; ++i) {
        void* p = *reinterpret_cast<void**>(list + (uintptr_t)i * 8);
        if (!p || p == pasture || p == best) continue;
        CheckPtr(&p);
        if (PastureType(p) != PastureType(pasture)) continue;
        transferInto(p);
    }
    if (Trace()) {
        std::fprintf(stderr, "[Pasture] cmd %d: moved %d animal(s) out of %p (keep=%d, "
                     "now %d)\n", cmd, moved, pasture, keep, PastureCount(pasture));
        std::fflush(stderr);
    }
}

// ============================================================
// 0x14009f770 — map-grid cell owner-ref stamp (shared leaf).
// ------------------------------------------------------------
// Writes an 8-byte ref (the owning entity/template value the caller passes)
// into cell+0x10 for every cell of the (x,y,w,h) footprint rect. Cell array at
// mapGrid+0x80, stride 0x30. Retail callers: the pasture state-2 per-frame
// (sub_67e0 — stamps the pasture's cells so pathing/occupancy queries see it)
// and HarvestOrchard_impl_0800. Was resolved by s_StampCells above but never
// registered — the null guard made footprint stamping a silent no-op until
// 2026-07-30 (deferred item from the animal wave's Update 2).
// ============================================================
extern "C" uint64_t MapGrid_StampCellRefs_9f770(void* grid, int x, int y, int w, int h,
                                                void* ref) {
    if (!grid || w <= 0) return 0;
    const int dim = *reinterpret_cast<int*>(grid);
    uintptr_t cells = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(grid) + 0x80);
    if (!cells) return 0;
    for (int64_t dx = 0; dx < w; ++dx) {
        for (int64_t dy = 0; dy < h; ++dy) {
            const int64_t cell = (dy + y) * dim + x + dx;
            *reinterpret_cast<void**>(cells + 0x10 + cell * 0x30) = ref;
        }
    }
    return 0;
}
