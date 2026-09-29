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
// are spawned as FREE-ROAMING entities.
//
// CORRECTION: they are NOT spawned by the start-config simple pool. That pool
// (startConfig+0x90, count +0x98) is `_populations` and holds exactly one entry,
// PopulationDeer — verified from a live run (simple(+0x98)=1). The livestock come from
// the THIRD agri-pool loop's FUN_1400a0b10 call, ported below as
// StartTown_SpawnStartingHerd. LivestockComponent::OnAdd
// (0x140123c70) files each one into the world's free-roaming livestock list at
// gameState+0x80, and when the player later builds a pasture and picks a type, the
// pasture's state-2 adopt pass (sub_7140) pulls the matching wanderers in. That is the
// "the pasture stocks itself at town start" behaviour — there is no auto-assignment of
// pasture+0x28 anywhere in retail.
//
// For standalone bring-up (no pasture dialog yet) BANISHED_PASTURE_AUTOTYPE lets the
// pasture pick its type itself. That is NOT retail behaviour, so it is opt-in only.

#include "Component/PastureComponent.h"
#include "Component/LivestockComponent.h"
#include "Component/AnimalComponent.h"
#include "Component/CharacterComponent.h"
#include "Component/ComponentDescription.h"
#include "Component/MapComponent.h"
#include "Component/ModelComponent.h"
#include "Entity/CitizenBehavior.h"   // CitizenBehavior::pastures (GameUpdateLoop::m_citizenList)
#include "Entity/ComponentEntity.h"
#include "Game/Map.h"                 // Map::size (GameUpdateLoop::m_game)
#include "math/Math.h"
#include <new>
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/exe_bodies.h"
#include "Component/herdcomponent_funcs.h"     // HerdComponent_Helper7400 (0x140107400)
#include "Entity/componententity_funcs.h"      // ComponentEntity_CtorFromDescription, EntityList_RegisterEntity_Impl
#include "Game/exe_constants.h"

// The exe helpers this file calls, by retail address:
//   0x14002f1d0 FindComponent<LivestockComponent>   0x140005500 FindComponent<AnimalComponent>
//   0x1400012d0 FindComponent<CharacterComponent>   0x140004680 FindComponent<MapComponent>
//   0x1400c3720 ComponentEntity_CtorFromDescription 0x1400c48a0 EntityList_RegisterEntity_Impl
//   0x140107400 AnimalComponent::Place              0x140107320 AnimalComponent::UpdateBehavior
//   0x140128570 PastureComponent::ComputeCapacity
//   .rdata 0x140172220 yaw spread (180.0f), 0x140172114 half cell (0.5f)
// LEFT (B.4 COMP-d): GameUpdateLoop m_game / m_citizenList / m_entityList are void* (W0
// header); ModelComponent::transform is raw storage (H5).

// ============================================================
// Game_helper_8670 (0x140128670) — spawn ONE livestock into this pasture.
//   `parent` is non-null for a birth (the new animal appears next to its parent), null
//   for a fresh stocking (random footprint cell). The male/female roll uses the
//   pasture's current mix: retail counts the animals whose AnimalComponent canGraze
//   byte is 0 and rolls RandFloat against thatCount/total, so a pasture short of one
//   sex tends to produce it.
// ============================================================
void PastureComponent::SpawnLivestock(AnimalComponent* const* parent) {
    GameUpdateLoop* w = GameUpdateLoop::_singleton;
    if (!w) return;
    System::Random* rng = w->m_random;
    ComponentDescription* tmpl = livestockType;
    if (tmpl == nullptr) return;

    ComponentEntity* mem = static_cast<ComponentEntity*>(System::MemoryAllocator::MemAlloc(0x48, 8));
    ComponentEntity* ent = mem ? ComponentEntity_CtorFromDescription(mem, tmpl) : nullptr;
    if (!ent) return;

    LivestockComponent* livestock = ent->FindComponent<LivestockComponent>();   // 0x14002f1d0
    AnimalComponent* animal = ent->FindComponent<AnimalComponent>();            // 0x140005500

    // ── Position ──────────────────────────────────────────────────────────────
    float posX = 0.0f, posY = 0.0f;
    CharacterComponent* parentCharacter = nullptr;
    if (parent && *parent != nullptr) {
        ComponentEntity* parentOwner = (*parent)->_entity;
        parentCharacter = parentOwner->FindComponent<CharacterComponent>();   // 0x1400012d0
    }

    if (parentCharacter != nullptr) {
        // Birth: jitter around the parent's model position.
        float jitterY = fn_Random_RandFloatTol(rng);
        float jitterX = fn_Random_RandFloatTol(rng);
        ModelComponent* model = parentCharacter->modelComponent;
        if (model) {
            // LEFT: ModelComponent::transform is raw storage (H5).
            const Math::Vector3& p =
                reinterpret_cast<const Math::Transform*>(model->transform)->translation;
            posX = p.x + jitterX;
            posY = p.y + jitterY;
        }
    } else {
        // Fresh stocking: a random cell inside the pasture's map footprint.
        MapComponent* mc = _entity->FindComponent<MapComponent>();   // 0x140004680
        if (mc) {
            int16_t x0 = static_cast<int16_t>(mc->tileX0);
            int16_t y0 = static_cast<int16_t>(mc->tileY0);
            int16_t fw = static_cast<int16_t>(mc->tileX1);
            int16_t fh = static_cast<int16_t>(mc->tileY1);
            uint32_t dx = rng->Rand((uint32_t)fw);
            uint32_t dy = rng->Rand((uint32_t)fh);
            // LEFT: GameUpdateLoop::m_game is void* (W0 header).
            const Map* grid = reinterpret_cast<const Map*>(w->m_game);
            float jitterY = fn_Random_RandFloatTol(rng);
            float jitterX = fn_Random_RandFloatTol(rng);
            if (grid) {
                const int dim = grid->size;
                const int cell = dim * (y0 + (int)dy) + x0 + (int)dx;
                const float half = static_cast<float>(dim >> 1) - kRdata_140172114;
                float world2[3] = { static_cast<float>(cell % dim) - half,
                                    static_cast<float>(cell / dim) - half, 0.0f };
                float out3[3] = {0, 0, 0};
                Reimpl_MapGetTerrainPos(w->m_game, out3, world2, 0, nullptr);
                posX = out3[0] + jitterX;
                posY = out3[1] + jitterY;
            }
        }
    }

    // ── Sex roll from the pasture's current mix ───────────────────────────────
    int flagged = 0;
    const int total = animalCount;
    for (int i = 0; i < total; ++i) {
        LivestockComponent** arr = animals;
        if (!arr) break;
        LivestockComponent* peerLivestock = arr[i];
        if (!peerLivestock) continue;
        AnimalComponent* peerAnimal = peerLivestock->_entity->FindComponent<AnimalComponent>();   // 0x140005500
        if (peerAnimal && !peerAnimal->canGraze)
            ++flagged;
    }
    bool female = true;
    if (total != 0) {
        float roll = rng->RandFloat();
        female = roll < (static_cast<float>(flagged) / static_cast<float>(total));
    }

    // ── Place, bind, activate, register ───────────────────────────────────────
    if (animal) {
        float yaw = fn_Random_RandFloatTol(rng) * kRdata_140172220;
        float pos2[2] = { posX, posY };
        // LEFT: the tree hands Place a two-float position (retail's Vector3 has z = 0);
        // the forwarder takes the component as int64_t.
        HerdComponent_Helper7400(reinterpret_cast<int64_t>(animal), pos2, yaw, (char)female);
    }

    PastureComponent* pastureRef = this;
    if (livestock)
        livestock->SetPasture(&pastureRef);   // 0x140123cc0

    // A fresh stocking randomises the production phase so a pasture full of animals
    // does not yield everything on the same tick; a birth starts at 0.
    if (animal) {
        float phase = 0.0f;
        if (parent && *parent == nullptr)
            phase = rng->RandFloat();
        animal->age = phase;
        animal->UpdateBehavior();   // 0x140107320
    }

    ComponentEntity* regRef = ent;
    // LEFT: GameUpdateLoop::m_entityList is void* (W0 header); the forwarder takes it.
    EntityList_RegisterEntity_Impl(w->m_entityList, &regRef, tmpl->_category);

    std::fprintf(stderr, "[Pasture] SPAWN livestock ent=%p into pasture=%p at (%.1f,%.1f) "
                 "female=%d birth=%d (count now %d/%d)\n",
                 static_cast<void*>(ent), static_cast<void*>(this), posX, posY, (int)female,
                 (int)(parentCharacter != nullptr), animalCount, capacity);
    std::fflush(stderr);
}

// B4 FORWARD: 1 outside caller file (Dialog/debugui.cpp).
extern "C" void Pasture_SpawnLivestock(void* pasture, void* parentAnimalIBP) {
    if (!pasture) return;
    reinterpret_cast<PastureComponent*>(pasture)->SpawnLivestock(
        reinterpret_cast<AnimalComponent* const*>(parentAnimalIBP));
}

// ============================================================
// Game_SpawnItemsInStorage (0x140060290) — MISNAMED in Ghidra; this is the debug
// menu's "fill pastures" body. For every pasture in the town registry that has a
// livestock type, spawn animals until it is at capacity, at most 5 per pasture per call.
// ============================================================
extern "C" int Pasture_FillAllPastures() {
    GameUpdateLoop* w = GameUpdateLoop::_singleton;
    // LEFT: GameUpdateLoop::m_citizenList is void* (W0 header).
    CitizenBehavior* town = w ? reinterpret_cast<CitizenBehavior*>(w->m_citizenList) : nullptr;
    if (!town) return 0;
    const int nPastures = town->pastureCount;
    PastureComponent** list = town->pastures;
    if (nPastures <= 0 || !list) return 0;

    int spawned = 0;
    for (int i = 0; i < nPastures; ++i) {
        PastureComponent* pasture = list[i];
        if (!pasture || pasture->livestockType == nullptr) continue;
        for (int k = 0; k < 5; ++k) {
            const int cap = pasture->ComputeCapacity();   // 0x140128570
            if (pasture->animalCount >= cap) break;
            AnimalComponent* nullParent = nullptr;
            pasture->SpawnLivestock(&nullParent);   // 0x140128670
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
// The command-2 slot is PastureComponent::Command (0x140127a60), which posts the
// PastureWork job and flips the workplace on — hence the deliberate double write:
// the slot is invoked with the type already visible, then the field is re-stamped.
// ============================================================
extern "C" void Pasture_SetLivestockType(PastureComponent* pasture, ComponentDescription* newTypeTemplate) {
    if (!pasture) return;
    ComponentDescription* prev = pasture->livestockType;
    if (prev == nullptr && newTypeTemplate != nullptr) {
        pasture->livestockType = newTypeTemplate;
        pasture->Command(2);   // slot 14, +0x70
    }
    pasture->livestockType = newTypeTemplate;
    pasture->capacity = pasture->ComputeCapacity();

    std::fprintf(stderr, "[Pasture] set livestock type: pasture=%p %p -> %p (capacity=%d)\n",
                 static_cast<void*>(pasture), static_cast<void*>(prev),
                 static_cast<void*>(newTypeTemplate), pasture->capacity);
    std::fflush(stderr);
}


// ============================================================
// Reimpl of FUN_1400a0b10 — the START-CONDITION livestock grant ("easy-start cows")
// ============================================================
// This is the PRIMARY way a town gets its first livestock, and it is NOT trade.
// FUN_1400a3300 (StartTown) runs three agri-pool loops at its tail; the THIRD one
// (pool sc+0xd0, size sc+0xd8, count sc+0xe8) is the livestock pool, and its body is
//     1400a35dc: CALL 0x1400a0b10        <- spawn the herd     (RCX=sc, RDX=centre, R8=tmpl)
//     1400a35e7: CALL 0x140091a20        <- availability-add   (RCX=townMgr, RDX=tmpl)
// Loop bound VERIFIED from `CMP R12D,[RDI+0xe8]` at 0x1400a35ef.
//
// Complete CALL set of 0x1400a0b10 contains no trade leaf (no 0x14005ace0, no trading
// post, no storage): it is MemAlloc -> CopyCtor -> place -> register, the same primitive
// shape as Pasture_SpawnOneLivestock. Livestock via TRADE is real but SECONDARY; the two
// paths are independent, so this does not depend on the unported trade lane.
//
// Easy.rsc: _livestockCount=1, _livestockOffsetX/Y=0, _livestockRadius=4,
//           _livestockGroupCount=8.  Medium/Hard set _livestockCount=0 — no starting herd
//           there, which is faithful, not a bug.
// So an Easy start rolls ONE of the three types in _initialLivestockList
// [Chicken, Cow, Sheep] and spawns EIGHT free-roaming animals of it. A pasture later set
// to either of the other two types will correctly show 0 animals until trade exists.
//
// These animals are free-roaming, NOT placed in a pasture. LivestockComponent::OnAdd
// (0x140123c70) files them into gameState+0x80, and the pasture state-2 adopt pass
// (sub_7140, pasturecomponent_states.cpp:287) pulls matching ones in once the player
// builds a pasture and picks that type. Both of those are already ported and live.
extern "C" int StartTown_SpawnStartingHerd(void* startConfig, const float* centre, void* tmpl) {
    // LEFT: the start condition has no class here (starttown.cpp hands it as void*).
    uint8_t* sc = reinterpret_cast<uint8_t*>(startConfig);
    if (!sc || !tmpl || !centre) return 0;

    const int groupCount = *reinterpret_cast<int32_t*>(sc + 0xf8);
    if (groupCount <= 0) return 0;

    GameUpdateLoop* w = GameUpdateLoop::_singleton;
    if (!w) return 0;
    System::Random* rng = w->m_random;

    // Retail scans the entity's component array inline and compares GetTypeId (vtable
    // +0x10) against DAT_1401c6f98 = AnimalComponent — the same loop FindComponent<T>
    // instantiates; it does NOT call the 0x140005500 finder.

    // Retail seeds the alternating sex from ONE Rand(2) before the loop and flips it every
    // iteration, so a group of 8 comes out 4/4 rather than 8 independent coin flips.
    bool female = rng->Rand(2u) == 0u;

    const float offX      = *reinterpret_cast<float*>(sc + 0xec);
    const float offY      = *reinterpret_cast<float*>(sc + 0xf0);
    const float radius    = *reinterpret_cast<float*>(sc + 0xf4);
    const float yawSpread = kRdata_140172220;  // 0x140172220 = 180.0f

    // LEFT: starttown.cpp hands the template as void*.
    const ComponentDescription* templ = reinterpret_cast<const ComponentDescription*>(tmpl);

    int spawned = 0;
    for (int i = 0; i < groupCount; ++i) {
        ComponentEntity* mem = static_cast<ComponentEntity*>(System::MemoryAllocator::MemAlloc(0x48, 8));
        ComponentEntity* ent = mem ? ComponentEntity_CtorFromDescription(mem, tmpl) : nullptr;
        if (!ent) continue;

        // Inline AnimalComponent scan (retail LAB_1400a0c40 path).
        AnimalComponent* animal = ent->FindComponent<AnimalComponent>();

        // Retail evaluates RandFloatTol TWICE and applies the results CROSSED: the FIRST
        // scales Y, the SECOND scales X (decompile: X += fVar13*r, Y += fVar12*r where
        // fVar12 is the earlier call). The world RNG is shared, so evaluating these in
        // the wrong order desynchronises every later roll in the game.
        const float tolY = fn_Random_RandFloatTol(rng);
        const float tolX = fn_Random_RandFloatTol(rng);
        const float px = centre[0] + offX + tolX * radius;
        const float py = centre[1] + offY + tolY * radius;

        if (animal) {
            const float yaw = fn_Random_RandFloatTol(rng) * yawSpread;
            // THREE floats, not two: retail builds a full Vector3 (Z = centre Z) and the
            // callee reads all three.
            const Math::Vector3 pos3(px, py, centre[2]);
            animal->Place(&pos3, yaw, female);   // 0x140107400
        }
        female = !female;   // alternate AFTER placing, as retail does

        if (animal) {
            // age, randomised so the herd doesn't move in lockstep.
            animal->age = rng->RandFloat();
            animal->UpdateBehavior();   // 0x140107320
        }

        ComponentEntity* regRef = ent;
        // LEFT: GameUpdateLoop::m_entityList is void* (W0 header); the forwarder takes it.
        EntityList_RegisterEntity_Impl(w->m_entityList, &regRef, templ->_category);
        ++spawned;
    }

    std::fprintf(stderr, "[StartHerd] spawned %d/%d livestock from template %p "
                 "at (%.1f,%.1f) radius=%.1f\n",
                 spawned, groupCount, tmpl, centre[0] + offX, centre[1] + offY, radius);
    std::fflush(stderr);
    return spawned;
}
