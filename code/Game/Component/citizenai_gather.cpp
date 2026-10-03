// code/Game/Component/citizenai_gather.cpp
//
// Brick 3b handler wave 4 — faithful 1:1 ports of the two near-identical CitizenAIComponent
// resource-gathering state handlers + one private helper they share-by-shape:
//
//   0x14010bc90  CitizenAI_State_GatherResources   (state id 6)  -> dd30 + CheckWorkplaceValid
//                                                    + Storage/QueryCount/GetResourceCount
//                                                    + Position/2e20 + FindPath + ef40 transfer-loop
//                                                    + MeshInstance hide-render branch + 12c3f0
//   0x14010b3e0  CitizenAI_State_GatherResources2  (state id 7)  -> same, with the private
//                                                    Storage_GetResourceCount2 + a resource-slot
//                                                    selection loop (count matching RawMaterial
//                                                    slots, Random pick, transfer)
//   0x14012ab00  Storage_GetResourceCount2  (private to Gather2; closure = Entity_FindComponent_Storage
//                                            @0x140006270 (REG) + IsDerivedFrom only -> closure-safe)
//
// Both handlers make their state ENTERABLE by validating/walking-to the citizen workplace and
// (when a carry target is found) building the pickup path + transferring resources onto the carrier.
// ValidateAndEnterState (citizenai_candidatelist.cpp) dispatches them by ResolveAddr; returning 1
// commits the state. Transcribed from the Ghidra decompile + disassembly (FUN 0x14010bc90 /
// 0x14010b3e0), offset-by-offset.
//
// SELF CHAIN: param_1 = CitizenAIComponent. self = param_1+0x38 (sibling CitizenComponent IBP).
//   citizen = *self. citizen+0x238 = workplace entity, +0x10 = own entity IBP, +0x18 = description
//   (CitizenDescription, +0xd8 = gather-rate scalar), +0x278 = CharacterComponent (+0x68 =
//   ModelComponent transform, +0x158/+0x15c/+0x160 = world pos). Success epilogue writes the
//   action-state flags to *self (+0x54 |= 4, +0x30 = -1, +0xb0 = 0 then +0xb0 = stateNum) and
//   returns CONCAT71(cz>>8, 1). Cooldown-fail stamps Time(rate * mult) into the CitizenAIComponent
//   timer (Gather: param_1+0x70; Gather2: param_1+0x78) and returns ...&0xff00. The
//   found-but-empty branch (resource count 0) runs the MeshInstance "hide" render branch on the
//   workplace MeshInstance (mirrors citizenai_handlers2.cpp SeekHappiness hide-render).
//
// GameWorld singleton (g_pGameWorldSingleton @ DAT_1401c3dd8): +0x60 = Graphics::Scene,
//   +0xb0 = (sub @+0x30, +0x50 = town carry capacity), +0xd8 = Random, +0xf0 = entity-list pair,
//   +0xf8 = MapGrid (int* width at [0]).
//
// Registered exe sub-callees (resolved via ResolveAddr; bodies NOT duplicated):
//   0x14008dd30  CitizenAI_State_sub_dd30           (citizenai_findtarget.cpp)   [find carry target]
//   0x1401158d0  CitizenComponent_CheckWorkplaceValid (reg_component_gap_A.cpp)
//   0x140002f90  Entity_FindComponent_MeshInstance  (registered)
//   0x140002ed0  Entity_FindComponent_Position      (reg_work_base.cpp)
//   0x140006270  Entity_FindComponent_Storage       (registered)  [citizen+0x10 variant]
//   0x140008090  Entity_FindComponent_Storage       (reg_component_people.cpp)   [workplace variant]
//   0x140122e20  Entity_impl_2e20                    (reg_pf_road.cpp)  [resolve world pos]
//   0x140087fb0  Entity_impl_7fb0                    (reg_pf_road.cpp)  [workplace anchor cell]
//   0x14008ef40  Entity_impl_ef40                    (reg_component_gap_A.cpp)  [carrier slot iter]
//   0x1400c3470  FUN_1400c3470 NotifyEntityChanged   (reg_action_hunt)
//   0x140116540  FUN_140116540  town carry-capacity  (registered)
//   0x14012c3f0  FUN_14012c3f0  MeshInstance set-flag (registered)
//   0x14012cf10  StorageComponent_QueryCount         (registered)
//   0x14012a9d0  Storage_GetResourceCount            (reg_component_people.cpp)  [Gather]
//   0x14012ab00  Storage_GetResourceCount2           (THIS FILE)                 [Gather2]
//   0x1400bf790  Pathfinding_FindPath                (registered)
//   0x1400ca230  Resource_TransferToCarrier          (registered)
//   0x140115e60  CitizenAI_State_LongTermPlan        (another agent registers; null-guarded)
//
// KEY ABI (verified): Entity_impl_ef40 (0x14008ef40) IGNORES its first arg — real sig
//   (ignoredArg1, int64_t* out, void* entityIBP, int category). Mirror the decompile but arg1
//   is don't-care (the decompile passes the citizen there; harmless).
//
// SELF-CONTAINED-BRICK type-ids (UNFILLED 0xFFFFFFFF in standalone; resolved via
//   Object::GetTypeIdByName, every scan-result deref null-guarded):
//     DWORD_1401c7190  -> "CitizenDescription"     [cooldown-rate IsDerivedFrom on citizen+0x18]
//     DWORD_1401c7cb8  -> "RawMaterialDescription"  [resource-slot child-scan IsDerivedFrom]
//
// .rdata float consts (read from the exe):
//     DAT_140172234 = 300.0f  (found-but-empty cooldown multiplier)
//     DAT_1401721fc =  75.0f  (no-valid-workplace cooldown multiplier)
//
// The MeshInstance/AddObject/RemoveObject render sub-branch touches the still-INERT ModelComponent
// — every deref through it is null-guarded so it stays crash-safe (it is NOT exercised at
// world-start).


// graphics/Scene.h FIRST: it pulls runtime/types/volume/Volume.h, whose
// global-scope `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()` once
// <windows.h> has been seen through resolve.h, which breaks Volume.h's own
// `sizeof(Rectangle)` assert.  ENDGAME B.3.5 caller batch I2.
#include "graphics/Scene.h"
#include "Component/RawMaterialDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/CitizenDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/CitizenAIComponent.h"
#include "Component/CitizenComponent.h"
#include "Component/CharacterComponent.h"
#include "Component/InteractComponent.h"
#include "Component/ResidenceComponent.h"
#include "Component/StatusIconComponent.h"
#include "Component/StorageComponent.h"
#include "Component/ModelComponent.h"
#include "Component/ComponentDescription.h"
#include "Entity/ComponentEntity.h"
#include "Entity/CitizenBehavior.h"   // GameUpdateLoop +0xF0 (the dd30 candidate lists)
#include "Game/Map.h"                 // GameUpdateLoop +0xF8
#include "Game/ProfessionList.h"
#include "Game/Profession.h"
#include "graphics/Mesh.h"
#include "Component/Component.h"   // B.3.x W0: Component slots by name
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "Game/exe_bodies.h"   // B3R STORAGE: g_gameSettings
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "scene_watch.h"
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Work/exe_bodies.h"

namespace {

// --- registered exe sub-callees: called by name (Component/Action/Work exe_bodies.h) ---
// NB: WorkAct_GatherRawMatCtor_bf790's 8th param is a 64-bit RawMaterialDescription* (0 =
// "any material"), NOT a 32-bit `long`. On Win64 `long` is 32-bit: a `long` there made the
// caller write only the low dword of the 8-byte stack slot, leaving the high dword as stack
// garbage, which the callee (exe Pathfinding_FindPath, which reads the full 8 bytes via
// CONCAT44) passed to the PickupStored ctor (ca520) and crashed once LIVE_STORAGE made
// storage non-empty. The declaration's int64_t writes the full 0.

static uint32_t s_tid_CitizenDescription     = 0;  // DWORD_1401c7190
static uint32_t s_tid_RawMaterialDescription = 0;  // DWORD_1401c7cb8

static const float kMultEmpty   = 300.0f;  // DAT_140172234
static const float kMultInvalid = 75.0f;   // DAT_1401721fc

static bool s_init = false;
static void EnsureInit() {
    if (s_init) return;

    if (s_tid_CitizenDescription == 0)     s_tid_CitizenDescription     = CitizenDescription::TypeId();
    if (s_tid_RawMaterialDescription == 0) s_tid_RawMaterialDescription = RawMaterialDescription::TypeId();
    s_init = true;
}

// the citizen's ModelComponent (CharacterComponent +0x68).
static ModelComponent* CitizenModel(CitizenComponent* citizen) {
    CharacterComponent* ch = citizen->character;
    if (ch == nullptr) return nullptr;
    return ch->modelComponent;
}

// model world pos (transform.translation +0x158/+0x15c) -> clamped grid cell (w*row + col).
static uint32_t ModelCell(const ModelComponent* model, const Map* map) {
    int w = map->size;
    int col = static_cast<int>(model->transform.translation.x + static_cast<float>(w >> 1));
    int row = static_cast<int>(model->transform.translation.y + static_cast<float>(w >> 1));
    if (col < 0) col = 0; if (w <= col) col = w - 1;
    if (row < 0) row = 0; if (w <= row) row = w - 1;
    return static_cast<uint32_t>(w * row + col);
}

// cooldown rate scalar: ( (IsDerivedFrom(citizen->_description, CitizenDescription) ? desc : 0)->_fd8 ) * mult.
// Original reads *(0+0xd8) when the IsDerivedFrom check fails — guard to 0 to stay crash-safe.
static int64_t CooldownStamp(CitizenComponent* citizen, float mult) {
    Description* desc = citizen->_description;
    bool ok = (desc && Object::IsDerivedFromNullSafe(desc, s_tid_CitizenDescription));
    float rate = ok ? static_cast<CitizenDescription*>(desc)->_fd8 : 0.0f;
    System::Time stamp = {0};
    RtSystem_Time_Ctor(&stamp, rate * mult);
    return stamp._ticks;
}

// The shared StatusIconComponent "show icon" branch (decompile: found==0 + home storage empty).
// Mirrors citizenai_handlers2.cpp SeekHappiness. flagBit distinguishes Gather (0x40)
// from Gather2 (0x80) — the OR into the icon mask. Every deref null-guarded.
// The home storage finder is 0x140008090 (the ResidenceComponent finder) for
// both callers; `getCount` is the caller's count body, passed by name.
static void HideRenderWorkplaceMesh(CitizenComponent* citizen, int (*getCount)(int64_t),
                                    uint32_t flagBit) {
    if (citizen->home == nullptr) return;

    ResidenceComponent* storComp = citizen->home->FindComponent<ResidenceComponent>();
    int cnt = storComp ? getCount(reinterpret_cast<int64_t>(storComp)) : 0;   // LEFT: SIG CitizenComp_Storage_GetResourceCount / Storage_GetResourceCount2 take int64_t (wave X)
    if (cnt != 0) return;  // not empty -> nothing to show

    StatusIconComponent* mi = citizen->home->FindComponent<StatusIconComponent>();
    if (mi == nullptr) return;
    ComponentEntity* inner = mi->_entity;
    // gated: ((inner->_entityFlags >> 10) & 1) == 0
    if (inner == nullptr || (inner->_entityFlags >> 10 & 1) != 0) return;

    // OR the icon mask, flagBit
    mi->_geometry.activeMask |= flagBit;
    uint32_t changed;
    if (mi->_geometry.activeMask == 0) {
        // mask empty -> remove from scene
        if (-1 < mi->iconId) {
            GameUpdateLoop::_singleton->m_graphicsScene
                ->RemoveObject(static_cast<uint32_t>(mi->iconId));
            mi->iconId = -1;
        }
        uint32_t before = mi->_flags;
        uint32_t after = before & 0xfffffffb;
        mi->_flags = after;
        changed = after ^ before;
    } else {
        // mask non-empty -> ensure in scene + run the slot-8 Tick
        GameSettings* settings = g_gameSettings;
        if (settings && settings->statusIconEnable != '\0' &&
            mi->iconId < 0) {
            uint32_t id = GameUpdateLoop::_singleton->m_graphicsScene
                ->AddObject(&mi->_geometry);
            mi->iconId = static_cast<int32_t>(id);
            SceneWatch_Note(id, &mi->_geometry, "CitizenAI::Gather carry-mesh");
        }
        System::Time t0 = {0};
        RtSystem_Time_Ctor(&t0, 0.0f);
        mi->Tick(&t0);
        uint32_t before = mi->_flags;
        uint32_t after = before | 4;
        mi->_flags = after;
        changed = after ^ before;
    }
    if ((changed & 0xffff) != 0 && mi->_entity != nullptr) {
        ActHunt_NotifyEntityChanged_c3470(mi->_entity);
    }
}

// Scan an entity's child list (slots at entity+0x28, count at entity+0x30) for the first child
// whose vtable+0x10 type id == RawMaterialDescription, IsDerivedFrom-confirmed. Returns the child
// (or null). Mirrors the inner do/while in the decompile.
// (B4 P2 CAI-2: the "entity" is a material template, ComponentDescription +0x28 / +0x30.)
static RawMaterialDescription* FindRawMaterialChild(const ComponentDescription* entity) {
    if (entity == nullptr) return nullptr;
    int count = entity->_subDescCount;
    Description** slots = entity->_subDescs;
    for (int i = 0; i < count; ++i) {
        Description* child = slots[i];
        if (child == nullptr) continue;
        uint32_t tid = child->Type();   // slot 2
        if (tid == s_tid_RawMaterialDescription) {
            if (Object::IsDerivedFromNullSafe(child, s_tid_RawMaterialDescription))
                return static_cast<RawMaterialDescription*>(child);
            return nullptr;
        }
    }
    return nullptr;
}

} // namespace

// ============================================================
// 0x14012ab00 — Storage_GetResourceCount2  (private helper for Gather2)
//   Walks the storage component's row entries (entity+0x10 -> Storage; rows at +0x28, count +0x30,
//   stride 0x10). For each row finds the RawMaterialDescription child; if its +0x5c bit6 set, adds
//   (row+8 - row+0xc) to the running count. Returns the total. Closure = Entity_FindComponent_Storage
//   + IsDerivedFrom only -> closure-safe.
// ============================================================
//   (B4 P2 CAI-2: param_1 is a Component, only its +0x10 entity is read; the rows are
//    StorageComponent::Item.)
extern "C" int Storage_GetResourceCount2(int64_t param_1) {
    Component* self = reinterpret_cast<Component*>(param_1);   // LEFT: SIG Storage_GetResourceCount2 (Component/exe_bodies.h, wave X)
    EnsureInit();
    // 0x140006270 (was its WorkBase_FindComponent_6270 forwarder)
    StorageComponent* stor = self->_entity->FindComponent<StorageComponent>();
    int total = 0;
    if (stor == nullptr) return 0;
    int count = stor->slotCount;
    for (int i = 0; i < count; ++i) {
        StorageComponent::Item* rows = stor->storageSlots;
        ComponentDescription* row = rows[i]._resource;
        RawMaterialDescription* rm = FindRawMaterialChild(row);
        if (rm && (rm->_flags >> 6 & 1) != 0) {
            total += rows[i]._count - rows[i]._reserve;
        }
    }
    return total;
}

// Gather (state 6) carrier transfer loop (decompile tail of the found!=0 branch). Walks the
// carrier-slot list produced by ef40, transferring while the citizen carry-capacity (capLeft)
// holds. ef40's entityIBP arg is &found (the carry-target, NOT the home); its first arg is
// IGNORED. Mirror the decompile's chaining: first call from the citizen, subsequent calls
// from the prior path-result entity (pathResult).
static void GatherTransferLoop(CitizenComponent* citizen, int64_t pathResult, int64_t found, int capLeft) {
    int64_t entIBP = found;                  // &found is the ef40 entityIBP arg (RBP+0x6f)
    int64_t carrierSlot = 0;                 // ef40 out (RBP+0x77 / local_res18)
    CitizenComp_Entity_impl_ef40(citizen, &carrierSlot, &entIBP, 0);
    while (carrierSlot != 0) {
        RawMaterialDescription* rm = FindRawMaterialChild(
            reinterpret_cast<ComponentDescription*>(carrierSlot));   // LEFT: SIG CitizenComp_Entity_impl_ef40 (the slot out is int64_t*, wave X)
        int slotCost = rm ? rm->_weight : 0x7fffffff;
        if (capLeft < slotCost) break;
        ActChild_TransferToCarrier_ca230(pathResult, carrierSlot, 1);
        capLeft -= slotCost;
        entIBP = found;
        CitizenComp_Entity_impl_ef40(reinterpret_cast<void*>(pathResult), &carrierSlot, &entIBP, 0);   // LEFT: SIG CitizenComp_Entity_impl_ef40 (arg 1 ignored; the tree passes pathResult)
    }
}

// Gather2 (state 7) resource-slot SELECTION + transfer (decompile LAB_14010b92f .. LAB_14010baf1 +
// LAB_14010bb8a). `found` = the carry-target entity; pathResult = the FindPath result entity (the
// carrier handle for Resource_TransferToCarrier). Outer loop re-scans `found`'s Storage each pass:
//   pass 1 — count rows whose RawMaterialDescription child has +0x5c bit6 set AND (row+8 - row+0xc)
//            > 0; track a preferred index (last match whose rm+0x58 byte != 0), else stays -1.
//   if any match: if no preferred, Random-pick chosen = Rand(matchCount).
//   pass 2 — walk to the chosen-th match; transfer it if capLeft >= rm+0x48; deduct and re-loop.
// Returns when a pass yields no transfer. Every scan-deref null-guarded.
static void SelectAndTransfer(int64_t found, int64_t pathResult, int capLeft) {
    int transfers = 0;

    for (;;) {
        // Storage(found) -> stor
        int64_t storWrap = found;
        StorageComponent* storBuf[2] = {nullptr, nullptr};
        WorkBase_FindComponent_6270(storWrap, storBuf);
        StorageComponent* stor = storBuf[0];
        if (stor == nullptr) {
            return;
        }

        int rowCount = stor->slotCount;                         // local_60
        int chosenIdx = -1;                                     // local_res8 low
        int matchCount = 0;                                    // pvVar21

        // pass 1: count matches + preferred index
        StorageComponent::Item* rowsBase = nullptr;  // local_res10 (= *(stor+0x28))
        for (int cur = 0; cur < rowCount; ++cur) {
            rowsBase = stor->storageSlots;
            ComponentDescription* child = rowsBase[cur]._resource;
            RawMaterialDescription* rm = FindRawMaterialChild(child);
            bool flagged = rm && (rm->_flags >> 6 & 1) != 0;
            int avail = rowsBase[cur]._count - rowsBase[cur]._reserve;
            if (flagged && avail > 0) {
                if (rm->_unknown58)
                    chosenIdx = matchCount;
                ++matchCount;
            }
        }

        if (matchCount == 0) return;
        if (chosenIdx < 0) {
            System::Random* rng = GameUpdateLoop::_singleton->m_random;
            chosenIdx = rng ? (int)rng->Rand((uint32_t)matchCount) : 0;
        }

        // pass 2: find the chosenIdx-th match
        int matchCounter = 0;   // local_res10 low
        ComponentDescription* hitChild = nullptr;   // the row child to transfer (lVar5)
        int hitCursor = -1;
        for (int cur = 0; cur < rowCount; ++cur) {
            StorageComponent::Item* rb = stor->storageSlots;   // local_88
            ComponentDescription* child = rb[cur]._resource;
            RawMaterialDescription* rm = FindRawMaterialChild(child);
            bool flagged = rm && (rm->_flags >> 6 & 1) != 0;
            int avail = rb[cur]._count - rb[cur]._reserve;
            if (flagged && avail > 0) {
                if (matchCounter == chosenIdx) { hitChild = child; hitCursor = cur; break; }
                ++matchCounter;
            }
        }

        if (hitChild == nullptr || hitCursor < 0) {
            return;   // LAB_14010bb8a: lVar5 == 0 -> done
        }

        // LAB_14010bb8a: resolve the chosen row child's RawMaterial cost, transfer if capLeft allows
        RawMaterialDescription* rm = FindRawMaterialChild(hitChild);
        int cost = rm ? rm->_weight : 0x7fffffff;
        if (capLeft < cost) {
            return;
        }
        ActChild_TransferToCarrier_ca230(pathResult, reinterpret_cast<int64_t>(hitChild), 1);   // LEFT: SIG ActChild_TransferToCarrier_ca230 takes int64_t (wave X)
        ++transfers;
        capLeft -= cost;
        // goto LAB_14010b92f: re-loop with reduced capacity
    }
}

// ============================================================
// 0x14010bc90 — CitizenAIComponent::State_GatherResources  (state id 6)
// ============================================================
uint64_t CitizenAIComponent::State_GatherResources() {
    EnsureInit();

    bool haveWorkplace = (citizen->home != nullptr);
    char valid = 0;
    if (haveWorkplace) {
        valid = citizen->CanReach(citizen->home);
    }

    if (haveWorkplace && valid != 0) {
        // own-storage QueryCount: the citizen entity's StorageComponent -> QueryCount
        StorageComponent* own = citizen->_entity->FindComponent<StorageComponent>();
        int qc = (own) ? CitizenComp_StorageComponent_QueryCount(reinterpret_cast<int64_t>(own)) : 0;   // LEFT: SIG CitizenComp_StorageComponent_QueryCount takes int64_t (wave X)
        if (qc > 0) {
            // 0x140115e60 LongTermPlan returns nothing; its caller keeps only the low byte
            // (ValidateAndEnterState), so the upper bits of r carry no value.
            citizen->LongTermPlan();
            uint64_t r = 0;
            queuedState = 6;
            return (r & 0xffffffffffffff00ULL) | 1;
        }

        // compute the citizen grid cell
        ModelComponent* model = CitizenModel(citizen);
        Map* mg = GameUpdateLoop::_singleton->m_game;   // +0xF8
        int w = mg->size;
        uint32_t cell = ModelCell(model, mg);

        // bGoal = home ? 7fb0(&home) : -1
        uint32_t bGoal;
        if (citizen->home == nullptr) {
            bGoal = 0xffffffff;
        } else {
            ComponentEntity* wp = citizen->home;
            bGoal = Pf_EntityAnchorCell_7fb0(&wp, nullptr);
        }

        // dd30 find carry target
        float dest[4] = {0, 0, 0, 0};   // local_70
        int64_t found = 0;              // local_res10
        int64_t outComp = 0;           // local_res18
        int64_t* listpair = reinterpret_cast<int64_t*>(GameUpdateLoop::_singleton->m_citizenList);   // LEFT: SIG CitizenAI_FindBestTarget_dd30 (Component/exe_bodies.h, wave X)
        CitizenAI_FindBestTarget_dd30(listpair, &found, cell, bGoal, &outComp, dest, 0);
        (void)w;

        if (found == 0) {
            // ---- cooldown + status-icon branch ----
            int64_t stamp = CooldownStamp(citizen, kMultEmpty);
            timers[6] = stamp;
            citizen->happinessFlags |= 4;
            HideRenderWorkplaceMesh(citizen, &CitizenComp_Storage_GetResourceCount, 0x40);   // 0x14012a9d0
            return static_cast<uint64_t>(stamp) & 0xffffffffffffff00ULL;
        }

        // ---- found != 0: build path + transfer ----
        // 1) StatusIcon set-flag(6) on the home
        if (citizen->home != nullptr) {
            StatusIconComponent* icon = citizen->home->FindComponent<StatusIconComponent>();
            icon->SetFlag(6, '\0');   // 0x14012c3f0 (was its CitizenComp_FUN_14012c3f0 forwarder)
        }

        // 2) InteractComponent(home) -> 2e20 -> destPos; FindPath
        InteractComponent* pp = citizen->home->FindComponent<InteractComponent>();
        float destPos[4] = {0, 0, 0, 0};  // local_60
        Pf_EntityResolvePos_2e20(pp, destPos, 0, 0, nullptr);

        CitizenComponent* citizenVal = citizen;                         // local_88
        ComponentEntity* destEntityIBP = citizenVal->home;              // local_res18
        int64_t pathResult = 0;                                          // local_res20
        WorkAct_GatherRawMatCtor_bf790(&pathResult, &citizenVal, -1,
                       reinterpret_cast<uint64_t*>(&destEntityIBP), destPos,   // LEFT: SIG WorkAct_GatherRawMatCtor_bf790 (Action/exe_bodies.h, wave X)
                       reinterpret_cast<void**>(&found),   // LEFT: SIG WorkAct_GatherRawMatCtor_bf790
                       dest, 0, -1, 0, 1, '\x01', 1, 0);

        // 3) capLeft = max( the profession list's field_0x30 carry amount, FUN_140116540(citizen) )
        int townCap = GameUpdateLoop::_singleton->m_professionList->field_0x30->priority;
        int citCap = WorkLeaf_FreeStorageCapacity_116540(reinterpret_cast<intptr_t>(citizen));   // LEFT: SIG WorkLeaf_FreeStorageCapacity_116540 takes intptr_t (wave X)
        int capLeft = (townCap < citCap) ? citCap : townCap;

        // 4) transfer loop (ef40 walks carrier slots of `found`, the carry target)
        GatherTransferLoop(citizen, pathResult, found, capLeft);

        // 5) set citizen action-state flags + return 1
        citizen->statusFlags |= 4;
        citizen->field_0x30 = -1;
        citizen->field_0xB0 = 0;
        citizen->field_0xB0 = 1;
        return 1;  // CONCAT71(citizen >> 8, 1): the caller reads the low byte
    }

    // ---- no reachable home: cooldown(_fd8 * 75.0) -> timers[6] ----
    int64_t stamp = CooldownStamp(citizen, kMultInvalid);
    timers[6] = stamp;
    return static_cast<uint64_t>(stamp) & 0xffffffffffffff00ULL;
}

// ============================================================
// 0x14010b3e0 — CitizenAIComponent::State_GatherResources2  (state id 7)
// ============================================================
uint64_t CitizenAIComponent::State_GatherResources2() {
    EnsureInit();

    bool haveWorkplace = (citizen->home != nullptr);
    char valid = 0;
    if (haveWorkplace) {
        valid = citizen->CanReach(citizen->home);
    }

    if (haveWorkplace && valid != 0) {
        // own-storage QueryCount
        StorageComponent* own = citizen->_entity->FindComponent<StorageComponent>();
        int qc = (own) ? CitizenComp_StorageComponent_QueryCount(reinterpret_cast<int64_t>(own)) : 0;   // LEFT: SIG CitizenComp_StorageComponent_QueryCount takes int64_t (wave X)
        if (qc > 0) {
            // 0x140115e60 LongTermPlan returns nothing; its caller keeps only the low byte
            // (ValidateAndEnterState), so the upper bits of r carry no value.
            citizen->LongTermPlan();
            uint64_t r = 0;
            queuedState = 7;
            return (r & 0xffffffffffffff00ULL) | 1;
        }

        // compute cell
        ModelComponent* model = CitizenModel(citizen);
        Map* mg = GameUpdateLoop::_singleton->m_game;   // +0xF8
        int w = mg->size;
        uint32_t cell = ModelCell(model, mg);

        // bGoal = home ? 7fb0(&home) : -1
        uint32_t bGoal;
        if (citizen->home == nullptr) {
            bGoal = 0xffffffff;
        } else {
            ComponentEntity* wp = citizen->home;
            bGoal = Pf_EntityAnchorCell_7fb0(&wp, nullptr);
        }

        // dd30 find carry target (category arg = 1 here, vs 0 in Gather)
        float srcPos[4] = {0, 0, 0, 0};  // local_88
        int64_t found = 0;               // local_70
        int64_t outComp = 0;             // local_res10
        int64_t* listpair = reinterpret_cast<int64_t*>(GameUpdateLoop::_singleton->m_citizenList);   // LEFT: SIG CitizenAI_FindBestTarget_dd30 (Component/exe_bodies.h, wave X)
        CitizenAI_FindBestTarget_dd30(listpair, &found, cell, bGoal, &outComp, srcPos, 1);
        (void)w;

        if (found == 0) {
            // ---- cooldown + status-icon branch ----
            citizen->happinessFlags |= 4;
            int64_t stamp = CooldownStamp(citizen, kMultEmpty);
            timers[7] = stamp;
            HideRenderWorkplaceMesh(citizen, &Storage_GetResourceCount2, 0x80);   // 0x14012ab00
            return static_cast<uint64_t>(stamp) & 0xffffffffffffff00ULL;
        }

        // ---- found != 0: StatusIcon flag(7), InteractComponent+2e20, FindPath ----
        if (citizen->home != nullptr) {
            StatusIconComponent* icon = citizen->home->FindComponent<StatusIconComponent>();
            icon->SetFlag(7, '\0');   // 0x14012c3f0 (was its CitizenComp_FUN_14012c3f0 forwarder)
        }

        InteractComponent* pp = citizen->home->FindComponent<InteractComponent>();
        float destPos[4] = {0, 0, 0, 0};  // local_98
        Pf_EntityResolvePos_2e20(pp, destPos, 0, 0, nullptr);

        CitizenComponent* citizenVal = citizen;                         // local_res20
        ComponentEntity* destEntityIBP = citizenVal->home;              // local_res18
        int64_t pathResult = 0;                                          // local_78
        WorkAct_GatherRawMatCtor_bf790(&pathResult, &citizenVal, -1,
                       reinterpret_cast<uint64_t*>(&destEntityIBP), destPos,   // LEFT: SIG WorkAct_GatherRawMatCtor_bf790 (Action/exe_bodies.h, wave X)
                       reinterpret_cast<void**>(&found),   // LEFT: SIG WorkAct_GatherRawMatCtor_bf790
                       srcPos, 0, -1, 0, 1, '\x01', 1, 0);

        // capLeft = max( the profession list's field_0x30 carry amount, FUN_140116540(citizen) )
        int townCap = GameUpdateLoop::_singleton->m_professionList->field_0x30->priority;
        int citCap = WorkLeaf_FreeStorageCapacity_116540(reinterpret_cast<intptr_t>(citizen));   // LEFT: SIG WorkLeaf_FreeStorageCapacity_116540 takes intptr_t (wave X)
        int capLeft = (townCap < citCap) ? citCap : townCap;
        // Distribution diagnostic (firewood-evenness): the per-trip take budget is
        // max(town default, citizen free carry) in WEIGHT units — degenerate values
        // here (garbage townCap from an unfilled gw+0xb0 chain, or a huge citizen
        // storage capacity) make single citizens drain the barn.
        { static int n = 0; if (n < 12) { ++n;
            fprintf(stderr, "[Gather2] budget: townCap=%d citCap=%d -> capLeft=%d\n",
                    townCap, citCap, capLeft);
            fflush(stderr); } }

        // Gather2's resource-slot SELECTION loop (decompile LAB_14010b92f .. LAB_14010baf1):
        //   pass 1 — count carrier rows whose RawMaterialDescription +0x5c bit6 set and have
        //            (row+8 - row+0xc) > 0; track a preferred index (rows whose +0x58 byte != 0).
        //   if any match: if the preferred index is unset, Random-pick one of the matches.
        //   pass 2 — walk to the chosen match and TransferToCarrier it.
        // Mirrors the decompile offset-by-offset; every scan-deref null-guarded. The carrier-slot
        // entity is found via ef40 (the loop's storage component is `found`'s Storage).
        SelectAndTransfer(found, pathResult, capLeft);

        // ---- set citizen action-state flags + return 1 ----
        citizen->statusFlags |= 4;
        citizen->field_0x30 = -1;
        citizen->field_0xB0 = 0;
        citizen->field_0xB0 = 2;
        return 1;  // CONCAT71(citizen >> 8, 1): the caller reads the low byte
    }

    // ---- no reachable home: cooldown(_fd8 * 75.0) -> timers[7] ----
    int64_t stamp = CooldownStamp(citizen, kMultInvalid);
    timers[7] = stamp;
    return static_cast<uint64_t>(stamp) & 0xffffffffffffff00ULL;
}

