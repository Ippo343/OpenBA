// code/Game/Component/pasturecomponent_spawn.cpp
// Animal-spawn wave / pasture lane — livestock instantiation for a pasture.
//
// Three retail bodies live here:
//
//  * Game_helper_8670 (0x140128670) — spawn ONE livestock into a pasture. Clones the
//    pasture's assigned prototype entity (pasture+0x28) with MemAlloc(0x48) +
//    ComponentEntity_CopyCtor, positions it (next to its parent when this is a birth,
//    otherwise at a random cell inside the pasture footprint), decides whether it is
//    born male/female from the pasture's current mix, binds it into the pasture, and
//    registers the entity with the world entity list.
//
//  * Game_SpawnItemsInStorage (0x140060290) — a MISNAMED debug-menu body: it walks the
//    world pasture registry and tops every typed pasture up to capacity, 5 animals per
//    call. Its only code caller is the debug menu's "fill pastures" slot. Ported as
//    Pasture_FillAllPastures.
//
//  * PastureUI_Slot11 (0x1400b4c40) — the ONLY writer of pasture+0x28 in the whole exe:
//    the pasture dialog's "set livestock type" handler. Its core is ported as
//    Pasture_SetLivestockType so any caller (dialog, harness, easy-start bring-up) sets
//    the type exactly the way retail does, including the 0 -> non-zero edge that fires
//    the component's command-2 vtable slot (which posts the PastureWork job).
//
// ── How a pasture gets stocked (established from the shipped data + the exe) ────────
// The Easy/Medium/Hard start conditions (Game\StartCondition\*.rsc) each reference
// Template\LivestockChicken.rsc, Template\LivestockCow.rsc and Template\LivestockSheep.rsc
// alongside Template\Deer.rsc — but NOT Template\Pasture.rsc. So the starting livestock
// are spawned as FREE-ROAMING entities by the start-config simple pool
// (ComponentEntity_impl_09e0 / StartTown_SpawnSimplePool). LivestockComponent::OnAdd
// (0x140123c70) files each one into the world's free-roaming livestock list at
// gameState+0x80, and when the player later builds a pasture and picks a type, the
// pasture's state-2 adopt pass (sub_7140) pulls the matching wanderers in. That is the
// "the pasture stocks itself at town start" behaviour — there is no auto-assignment of
// pasture+0x28 anywhere in retail.
//
// For standalone bring-up (no pasture dialog yet) BANISHED_PASTURE_AUTOTYPE lets the
// pasture pick its type itself. That is NOT retail behaviour, so it is opt-in only.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" void Pasture_BindAnimal(void* animalComp, void* pastureIBP);

// starttown.cpp — resolve a "Template\\Name.rsc" path to its loaded template object.
extern "C" void* StartTown_ResolveTemplateByPath(const char* path);

namespace {

typedef void* (*fn_Find_t)(int64_t entity, void* outIBP);
typedef void* (*fn_EntityCopyCtor_t)(void* mem, int64_t tmpl);                 // 0x1400c3720
typedef void  (*fn_RegisterEntity_t)(void* entityList, void* entIBP, int cat); // 0x1400c48a0
typedef void  (*fn_HerdPlace_t)(int64_t animalComp, void* pos2, float yaw,
                                bool female);                                  // 0x140107400
typedef void  (*fn_AnimalBehaviour_t)(void* animalComp);                       // 0x140107320
typedef uint64_t (*fn_Capacity_t)(uintptr_t pasture);                          // 0x140128570

fn_Find_t           s_FindLivestock = nullptr;   // 0x14002f1d0 (LivestockComponent finder)
fn_Find_t           s_FindAnimal    = nullptr;   // 0x140005500 (AnimalComponent finder)
fn_Find_t           s_FindFamily    = nullptr;   // 0x1400012d0
fn_Find_t           s_FindMapComp   = nullptr;   // 0x140004680
fn_EntityCopyCtor_t s_EntityCopy    = nullptr;
fn_RegisterEntity_t s_RegEntity     = nullptr;
fn_HerdPlace_t      s_HerdPlace     = nullptr;
fn_AnimalBehaviour_t s_AnimalBehav  = nullptr;
fn_Capacity_t       s_Capacity      = nullptr;
const float*        s_pYawSpread    = nullptr;   // 0x140172220
const float*        s_pHalfCell     = nullptr;   // 0x140172114
bool                s_resolved      = false;

template <class T> T R(uintptr_t a) { return reinterpret_cast<T>(ResolveAddr(Module::Exe, a)); }

void Resolve() {
    if (s_resolved) return;
    s_resolved = true;
    s_FindLivestock = R<fn_Find_t>(0x14002f1d0);
    s_FindAnimal    = R<fn_Find_t>(0x140005500);
    s_FindFamily    = R<fn_Find_t>(0x1400012d0);
    s_FindMapComp   = R<fn_Find_t>(0x140004680);
    s_EntityCopy    = R<fn_EntityCopyCtor_t>(0x1400c3720);
    s_RegEntity     = R<fn_RegisterEntity_t>(0x1400c48a0);
    s_HerdPlace     = R<fn_HerdPlace_t>(0x140107400);
    s_AnimalBehav   = R<fn_AnimalBehaviour_t>(0x140107320);
    s_Capacity      = R<fn_Capacity_t>(0x140128570);
    s_pYawSpread    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172220));
    s_pHalfCell     = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172114));
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
} // namespace (reopened below)

// ============================================================
// Reimpl of FUN_14002f1d0 — the standard entity find-loop for LivestockComponent
// (type id 0x1401c7970; same template as ActHunt_FindComponent_5500).
//
// This finder was NEVER registered, so s_FindLivestock resolved null and the
// spawn path silently skipped Pasture_BindAnimal for EVERY spawned or born
// livestock: fill-time sheep only reached the pasture because the state-2
// ADOPT pass rescued them from the free-roam pool, and NEWBORNS (pasture at
// capacity, adopt won't take them) stayed unbound free-roamers forever. It is
// also why every "[Pasture] SPAWN" trace printed count 0/5 — the bind that
// should have run just before never did.
// ============================================================
extern "C" void* Pasture_FindLivestockComp(int64_t entity, void* retIBP) {
    static const uint32_t* pTid = nullptr;
    if (!pTid)
        pTid = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7970));
    void* found = nullptr;
    if (pTid && *pTid && entity) {
        int n = *reinterpret_cast<int*>(entity + 0x38);
        uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x30);
        for (int i = 0; i < n && list; ++i) {
            void** slot = reinterpret_cast<void**>(list + static_cast<int64_t>(i) * 8);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
            void* comp = *slot;
            if (!comp) continue;
            uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(
                *reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
            if (t == *pTid) {
                found = (fn_IsDerivedFrom && fn_IsDerivedFrom(comp, *pTid)) ? comp : nullptr;
                break;
            }
        }
    }
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(retIBP, found);
    return retIBP;
}

namespace {

inline void*& PastureType(void* p)  { return *reinterpret_cast<void**>((uintptr_t)p + 0x28); }
inline uintptr_t PastureAnimals(void* p) { return *reinterpret_cast<uintptr_t*>((uintptr_t)p + 0x30); }
inline int&   PastureCount(void* p) { return *reinterpret_cast<int*>((uintptr_t)p + 0x38); }
inline int&   PastureCap(void* p)   { return *reinterpret_cast<int*>((uintptr_t)p + 0x40); }

}  // namespace

// ============================================================
// Game_helper_8670 (0x140128670) — spawn ONE livestock into `pasture`.
//   param_2 (parentAnimalIBP) is an InternalBasePtr: non-null for a birth (the new
//   animal appears next to its parent), null for a fresh stocking (random footprint
//   cell). The male/female roll uses the pasture's current mix: retail counts the
//   animals whose AnimalComponent+0x40 byte is 0 and rolls RandFloat against
//   thatCount/total, so a pasture short of one sex tends to produce it.
// ============================================================
extern "C" void Pasture_SpawnLivestock(void* pasture, void* parentAnimalIBP) {
    Resolve();
    if (!pasture) return;
    uintptr_t w = World();
    if (!w) return;
    void* rng = *reinterpret_cast<void**>(w + 0xd8);
    int64_t tmpl = *reinterpret_cast<int64_t*>((uintptr_t)pasture + 0x28);
    if (tmpl == 0) return;
    if (!s_EntityCopy || !fn_MemAlloc) return;

    void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
    void* ent = mem ? s_EntityCopy(mem, tmpl) : nullptr;
    if (!ent) return;
    uint64_t entIBP = MakeIBP(ent);
    CheckPtr(&entIBP);

    uint64_t livestockIBP = 0;
    if (s_FindLivestock) s_FindLivestock(static_cast<int64_t>(entIBP), &livestockIBP);
    CheckPtr(&entIBP);
    uint64_t animalIBP = 0;
    if (s_FindAnimal) s_FindAnimal(static_cast<int64_t>(entIBP), &animalIBP);

    // ── Position ──────────────────────────────────────────────────────────────
    float posX = 0.0f, posY = 0.0f;
    uint64_t parentFamily = 0;
    if (parentAnimalIBP && *reinterpret_cast<int64_t*>(parentAnimalIBP) != 0) {
        CheckPtr(parentAnimalIBP);
        int64_t parentOwner = *reinterpret_cast<int64_t*>(
            *reinterpret_cast<int64_t*>(parentAnimalIBP) + 0x10);
        CheckPtr(&parentOwner);
        if (s_FindFamily) s_FindFamily(parentOwner, &parentFamily);
    }

    if (parentFamily != 0) {
        // Birth: jitter around the parent's model position.
        float jitterY = fn_Random_RandFloatTol(rng);
        float jitterX = fn_Random_RandFloatTol(rng);
        CheckPtr(&parentFamily);
        uintptr_t fam = static_cast<uintptr_t>(parentFamily);
        CheckPtr(reinterpret_cast<void*>(fam + 0x68));
        uintptr_t model = *reinterpret_cast<uintptr_t*>(fam + 0x68);
        if (model) {
            posX = *reinterpret_cast<float*>(model + 0x158) + jitterX;
            posY = *reinterpret_cast<float*>(model + 0x15c) + jitterY;
        }
    } else if (s_FindMapComp) {
        // Fresh stocking: a random cell inside the pasture's map footprint.
        CheckPtr(reinterpret_cast<void*>((uintptr_t)pasture + 0x10));
        uint64_t mapIBP = 0;
        s_FindMapComp(*reinterpret_cast<int64_t*>((uintptr_t)pasture + 0x10), &mapIBP);
        CheckPtr(&mapIBP);
        if (mapIBP) {
            uintptr_t mc = static_cast<uintptr_t>(mapIBP);
            int16_t x0 = *reinterpret_cast<int16_t*>(mc + 0x28);
            int16_t y0 = *reinterpret_cast<int16_t*>(mc + 0x2a);
            int16_t fw = *reinterpret_cast<int16_t*>(mc + 0x2c);
            int16_t fh = *reinterpret_cast<int16_t*>(mc + 0x2e);
            uint32_t dx = fn_Random_Rand ? fn_Random_Rand(rng, (uint32_t)fw) : 0;
            uint32_t dy = fn_Random_Rand ? fn_Random_Rand(rng, (uint32_t)fh) : 0;
            int* grid = *reinterpret_cast<int**>(w + 0xf8);
            float jitterY = fn_Random_RandFloatTol(rng);
            float jitterX = fn_Random_RandFloatTol(rng);
            if (grid) {
                const int dim = *grid;
                const int cell = dim * (y0 + (int)dy) + x0 + (int)dx;
                const float half = static_cast<float>(dim >> 1) -
                                   (s_pHalfCell ? *s_pHalfCell : 0.5f);
                float world2[3] = { static_cast<float>(cell % dim) - half,
                                    static_cast<float>(cell / dim) - half, 0.0f };
                float out3[3] = {0, 0, 0};
                if (fn_MapGetTerrainPos)
                    fn_MapGetTerrainPos(grid, out3, world2, 0, nullptr);
                posX = out3[0] + jitterX;
                posY = out3[1] + jitterY;
            }
        }
    }

    // ── Sex roll from the pasture's current mix ───────────────────────────────
    int flagged = 0;
    const int total = PastureCount(pasture);
    for (int i = 0; i < total; ++i) {
        uintptr_t arr = PastureAnimals(pasture);
        if (!arr) break;
        uint64_t* slot = reinterpret_cast<uint64_t*>(arr + (uintptr_t)i * 8);
        CheckPtr(slot);
        if (!*slot) continue;
        uint64_t ownerIBP = *reinterpret_cast<uint64_t*>(*slot + 0x10);
        CheckPtr(&ownerIBP);
        uint64_t peerAnimal = 0;
        if (s_FindAnimal) s_FindAnimal(static_cast<int64_t>(ownerIBP), &peerAnimal);
        CheckPtr(&peerAnimal);
        if (peerAnimal && *reinterpret_cast<char*>(static_cast<uintptr_t>(peerAnimal) + 0x40) == 0)
            ++flagged;
    }
    bool female = true;
    if (total != 0) {
        float roll = fn_Random_RandFloat ? fn_Random_RandFloat(rng) : 0.5f;
        female = roll < (static_cast<float>(flagged) / static_cast<float>(total));
    }

    // ── Place, bind, activate, register ───────────────────────────────────────
    CheckPtr(&animalIBP);
    if (animalIBP && s_HerdPlace) {
        float yaw = fn_Random_RandFloatTol(rng) * (s_pYawSpread ? *s_pYawSpread : 180.0f);
        float pos2[2] = { posX, posY };
        s_HerdPlace(static_cast<int64_t>(animalIBP), pos2, yaw, female);
    }

    uint64_t pastureIBP = MakeIBP(pasture);
    CheckPtr(&livestockIBP);
    if (livestockIBP)
        Pasture_BindAnimal(reinterpret_cast<void*>(livestockIBP), &pastureIBP);

    // A fresh stocking randomises the production phase so a pasture full of animals
    // does not yield everything on the same tick; a birth starts at 0.
    if (animalIBP) {
        float phase = 0.0f;
        if (parentAnimalIBP && *reinterpret_cast<int64_t*>(parentAnimalIBP) == 0)
            phase = fn_Random_RandFloat ? fn_Random_RandFloat(rng) : 0.0f;
        *reinterpret_cast<float*>(static_cast<uintptr_t>(animalIBP) + 0x44) = phase;
        if (s_AnimalBehav) s_AnimalBehav(reinterpret_cast<void*>(animalIBP));
    }

    uint64_t regIBP = entIBP;
    if (s_RegEntity)
        s_RegEntity(*reinterpret_cast<void**>(w + 0xe8), &regIBP,
                    *reinterpret_cast<int*>(tmpl + 0x20));

    std::fprintf(stderr, "[Pasture] SPAWN livestock ent=%p into pasture=%p at (%.1f,%.1f) "
                 "female=%d birth=%d (count now %d/%d)\n",
                 ent, pasture, posX, posY, (int)female,
                 (int)(parentFamily != 0), PastureCount(pasture), PastureCap(pasture));
    std::fflush(stderr);
}

// ============================================================
// Game_SpawnItemsInStorage (0x140060290) — MISNAMED in Ghidra; this is the debug
// menu's "fill pastures" body. For every pasture in the world registry that has an
// assigned type, spawn animals until it is at capacity, at most 5 per pasture per call.
// ============================================================
extern "C" int Pasture_FillAllPastures() {
    Resolve();
    uintptr_t gs = GameState();
    if (!gs) return 0;
    const int nPastures = *reinterpret_cast<int*>(gs + 0x98);
    uintptr_t list = *reinterpret_cast<uintptr_t*>(gs + 0x90);
    if (nPastures <= 0 || !list) return 0;

    int spawned = 0;
    for (int i = 0; i < nPastures; ++i) {
        uint64_t pastureIBP = *reinterpret_cast<uint64_t*>(list + (uintptr_t)i * 8);
        CheckPtr(&pastureIBP);
        void* pasture = reinterpret_cast<void*>(pastureIBP);
        if (!pasture || PastureType(pasture) == nullptr) continue;
        for (int k = 0; k < 5; ++k) {
            CheckPtr(&pastureIBP);
            const int cap = s_Capacity ? static_cast<int>(s_Capacity((uintptr_t)pasture)) : 0;
            if (PastureCount(pasture) >= cap) break;
            uint64_t nullParent = MakeIBP(nullptr);
            Pasture_SpawnLivestock(pasture, &nullParent);
            ++spawned;
        }
    }
    if (spawned)
        std::fprintf(stderr, "[Pasture] fill-pastures: spawned %d animal(s) across %d pasture(s)\n",
                     spawned, nPastures);
    std::fflush(stderr);
    return spawned;
}

// ============================================================
// PastureUI_Slot11 (0x1400b4c40) core — set the pasture's livestock type.
//   if (pasture->type == 0 && newType != 0) { pasture->type = newType;
//                                             vtable[14](pasture, 2); }
//   pasture->type = newType;
//   pasture->capacity = FUN_140128570(pasture);
// The command-2 slot is PastureComponent::Slot14 (0x140127a60), which posts the
// PastureWork job and flips the workplace on — hence the deliberate double write:
// the slot is invoked with the type already visible, then the field is re-stamped.
// ============================================================
extern "C" void Pasture_SetLivestockType(void* pasture, void* newTypeTemplate) {
    Resolve();
    if (!pasture) return;
    void* prev = PastureType(pasture);
    if (prev == nullptr && newTypeTemplate != nullptr) {
        PastureType(pasture) = newTypeTemplate;
        void** vt = *reinterpret_cast<void***>(pasture);
        void* cmd = vt ? vt[14] : nullptr;              // vtable +0x70
        if (cmd) reinterpret_cast<void(*)(void*, int)>(cmd)(pasture, 2);
    }
    PastureType(pasture) = newTypeTemplate;
    if (s_Capacity) PastureCap(pasture) = static_cast<int>(s_Capacity((uintptr_t)pasture));

    std::fprintf(stderr, "[Pasture] set livestock type: pasture=%p %p -> %p (capacity=%d)\n",
                 pasture, prev, newTypeTemplate, PastureCap(pasture));
    std::fflush(stderr);
}

// ============================================================
// Bring-up only (NOT retail): give an untyped pasture a livestock type so the
// state-2 adopt pass has something to match while the pasture dialog is unported.
//
//   BANISHED_PASTURE_AUTOTYPE=1                -> first free-roaming livestock's prototype
//   BANISHED_PASTURE_AUTOTYPE=Sheep|Cow|Chicken-> Template\Livestock<Name>.rsc
//
// Returns true when a type was assigned.
// ============================================================
extern "C" bool Pasture_AutoAssignType(void* pasture) {
    Resolve();
    if (!pasture || PastureType(pasture) != nullptr) return false;
    const char* want = std::getenv("BANISHED_PASTURE_AUTOTYPE");
    if (!want || !want[0] || std::strcmp(want, "0") == 0) return false;

    void* tmpl = nullptr;
    if (std::strcmp(want, "1") != 0) {
        char path[128];
        std::snprintf(path, sizeof(path), "Template\\Livestock%s.rsc", want);
        tmpl = StartTown_ResolveTemplateByPath(path);
        if (!tmpl)
            std::fprintf(stderr, "[Pasture] AUTOTYPE: could not resolve %s\n", path);
    }
    if (!tmpl) {
        // Fall back to whatever is already wandering the map.
        uintptr_t gs = GameState();
        const int n = gs ? *reinterpret_cast<int*>(gs + 0x88) : 0;
        uintptr_t freeList = gs ? *reinterpret_cast<uintptr_t*>(gs + 0x80) : 0;
        for (int i = 0; i < n && freeList; ++i) {
            uint64_t compIBP = *reinterpret_cast<uint64_t*>(freeList + (uintptr_t)i * 8);
            CheckPtr(&compIBP);
            if (!compIBP) continue;
            uint64_t ownerIBP = *reinterpret_cast<uint64_t*>(compIBP + 0x10);
            CheckPtr(&ownerIBP);
            if (!ownerIBP) continue;
            tmpl = *reinterpret_cast<void**>(ownerIBP + 0x28);
            if (tmpl) break;
        }
    }
    if (!tmpl) {
        std::fprintf(stderr, "[Pasture] AUTOTYPE: no livestock prototype available for %p\n",
                     pasture);
        std::fflush(stderr);
        return false;
    }
    std::fprintf(stderr, "[Pasture] AUTOTYPE (bring-up, not retail): assigning %p to pasture %p\n",
                 tmpl, pasture);
    Pasture_SetLivestockType(pasture, tmpl);
    return true;
}
