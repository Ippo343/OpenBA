// code/Game/Component/citizenai_handlers.cpp
//
// Faithful C++ ports of the CitizenAIComponent state HANDLERS (Brick 3b) — the
// functions ValidateAndEnterState dispatches per state-id. Each handler that
// PUSHES an Action/Work object onto the citizen's action stack
// (CharacterComponent+0x48) makes its state ENTERABLE: once an action is pushed,
// CharacterComponent+0x60 (stack top index) becomes >= 0, TransitionState records
// the state id, and CitizenAIComponent::Update returns early at its tail (the
// stack-non-empty gate) so no re-entry/unbounded growth occurs while the action
// executor (move-tick, Brick 4b) is still inert.
//
// Functions ported in this file:
//   0x1400c26c0  PlayAnimation_Action_Ctor5  (the 3-action builder: pushes a
//                SendComponentMessage[+0x58] + optional PlayAnimation + MoveTo)
//   0x14010a2b0  CitizenAI_State_Wander
//
// All exe sub-callees are already registered (resolved via ResolveAddr):
//   0x14009ccf0  WorkGrid_FindCell        0x14009f3e0  MapGetTerrainPos
//   0x1400c9170  MoveTo_Action_Ctor       0x1400c54e0  ActionStack push
//   0x1400c5400  ActionStack cleanup      0x1400cb1d0  SendComponentMessage CopyCtor
//   0x1400012d0  Entity_FindComponent_Family
// Action vtables (exe .rdata): vtable_Action 0x14015faa0, vtable_PlayAnimation
//   0x140158950.  Float consts: 0x140172114 (0.5), 0x140172180 (3.0),
//   0x1401721a0 (7.0), 0x140172280 (-1.0, MoveTo randomized-speed sentinel).
//


#include <new>
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Action/PlayAnimation.h"   // B.3.8 CB1a (Vtable)
#include "Action/Action.h"   // B.3.8 CB1a (Vtable)
#include "Component/CitizenAIComponent.h"   // ENDGAME B.3.9 CB: kCitizenAIMessageTargetTypeId
#include "Component/CitizenComponent.h"
#include "Component/CharacterComponent.h"
#include "Component/ModelComponent.h"   // the model translation
#include "Entity/ComponentEntity.h"
#include "Game/Map.h"                   // Map::size (m_game)
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Work/exe_bodies.h"
#include "Game/exe_bodies.h"
#include "Game/exe_constants.h"
#include "system/MemoryAllocator.h"   // B3R: the allocator doors

namespace {

// --- registered exe sub-callees: called by name (Action/Work/Game exe_bodies.h); the
//     .rdata constants are exe_constants.h ---

} // namespace

// ============================================================
// 0x1400c26c0 — PlayAnimation_Action_Ctor5
// ------------------------------------------------------------
// Builds the citizen's wander/animation action triple and installs it:
//   (1) FUN_1400c5400 clears the action list at CharacterComponent::actions.
//   (2) Allocates a SendComponentMessage action (0x40 bytes), CopyCtor'd from the
//       entity IBP + msg-id DWORD_1401c70e8, and stores its IBP at
//       CharacterComponent::field_0x58 (DeletePtr the old one first).
//   (3) If animTime > 0: allocates a PlayAnimation action (0x40 bytes), inits its
//       vtable/entity(+0x20)/index(+0x28=0)/Time(+0x30)/+0x38=0, and PUSHES it.
//   (4) Allocates a MoveTo action (0x480 bytes), constructs it via MoveTo_Action_Ctor
//       toward `dest`, and PUSHES it.
//
// param_1 = the InternalPtr<CitizenComponent> (by address).
// param_2 = float* destination (world XY+; z filled by terrain query upstream).
// param_3 = animation time in seconds.
// ============================================================
static void CitizenAI_PlayAnimationActionCtor5(CitizenComponent* const* param_1, float* dest, float animTime) {
    CitizenComponent* citizen = *param_1;
    CharacterComponent* character = citizen->character;
    int64_t* stack = reinterpret_cast<int64_t*>(&character->actions);   // LEFT: SIG Pf_ActStackPush_54e0 (int64_t* stack; Action/exe_bodies.h)

    WorkBase_TaskListCleanup_c5400(stack);

    // (2) SendComponentMessage action -> CharacterComponent::field_0x58
    void* msgAction = nullptr;
    void* mem = System::MemoryAllocator::MemAlloc(0x40, 8);
    if (mem) {
        uint64_t destIBP = 0;
        new (&destIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        uint32_t msgId[2] = { kCitizenAIMessageTargetTypeId, 0u };  // PORT GAP: CitizenAIComponent::OnMessage (see CitizenAIComponent.h)
        uint64_t entityIBP = reinterpret_cast<uint64_t>(citizen->_entity);   // LEFT: SIG Pf_SendComponentMessageCopyCtor_b1d0 (uint64_t* IBP; Action/exe_bodies.h)
        msgAction = Pf_SendComponentMessageCopyCtor_b1d0(mem, &entityIBP, msgId, 0, &destIBP, 0);
    }
    Action* msgIBP = nullptr;
    new (&msgIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(msgAction));   // B.3.3 bridge; LEFT: SIG Pf_SendComponentMessageCopyCtor_b1d0 returns void*
    if (character->field_0x58 != nullptr) {
        reinterpret_cast< ::Object::InternalBasePtr*>(&character->field_0x58)->DeletePtr();   // B.3.3 bridge; LEFT: CharacterComponent::field_0x58 is Action*, not an InternalPtr (CharacterComponent.h, WORK-3)
    }
    character->field_0x58 = msgIBP;

    // (3) PlayAnimation action (only when animTime > 0)
    if (0.0f < animTime) {
        void* pmem = System::MemoryAllocator::MemAlloc(0x40, 8);
        PlayAnimation* animAction = nullptr;
        if (pmem) {
            System::Time timeStore{};
            RtSystem_Time_Ctor(&timeStore, animTime);
            ComponentEntity* entityIBP = citizen->_entity;
            // ENDGAME B.3.9 EXA: the real class (was the `??0Internal` door and
            // two stamps); then retail's inlined leaf stores.
            PlayAnimation* a = ::new (pmem) PlayAnimation();
            a->_field20        = entityIBP;   // raw IBP copy, as retail
            a->animation       = 0;
            a->_field30        = timeStore;
            a->_field38._ticks = 0;
            animAction = a;
        }
        Action* animIBP = animAction;   // the InternalPtr<Action> the push copies
        Pf_ActStackPush_54e0(stack, &animIBP);
    }

    // (4) MoveTo action (always)
    void* moveAction = nullptr;
    void* mmem = System::MemoryAllocator::MemAlloc(0x480, 8);
    if (mmem) {
        CharacterComponent* familyOut = nullptr;   // the InternalPtr the finder writes
        ActMv_FindComponentFamily_12d0(reinterpret_cast<uintptr_t>(citizen->_entity), &familyOut);   // LEFT: SIG ActMv_FindComponentFamily_12d0 (uintptr_t entity; Action/exe_bodies.h)
        if (familyOut == nullptr) {
            // Standalone: the CharacterComponent type-id global (DAT_1401c7078) read by
            // Entity_FindComponent_Family is unfilled, so the list-scan returns null. The
            // "family" component IS the citizen's own CharacterComponent,
            // populated by Brick 2/4a — use it directly (Brick self-contained pattern).
            familyOut = character;
        }
        float speed = kRdata_140172280;
        moveAction = Pf_MoveToCtor_9170(mmem, &familyOut, 1 /*Slow*/, 0, dest, 0xe1f, 0, speed);
    }
    int64_t moveIBP = 0;
    new (&moveIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(moveAction));   // B.3.3 bridge; LEFT: SIG Pf_MoveToCtor_9170 returns void*
    Pf_ActStackPush_54e0(stack, &moveIBP);
}

// ============================================================
// 0x14010a2b0 — CitizenAIComponent::State_Wander
// ------------------------------------------------------------
// Picks a nearby walkable cell and queues a MoveTo (+ animation) toward it.
//   target      = citizen->_parent                (bail if none)
//   if (target->spouse != 0 && Rand(1)==0) target = target->spouse
//   grid        = the world Map (GameUpdateLoop::m_game, untyped: raw)
//   mapSize     = *grid
//   For both `target` and the citizen read their ModelComponent world pos
//   (character -> modelComponent, +0x158 x / +0x15c y: ModelComponent's raw
//   transform), add (mapSize>>1), clamp to [0, mapSize-1], form a cell index
//   mapSize*yCell + xCell.
//   cell = WorkGrid_FindCell(grid, targetCell, selfCell, 0xe1f); bail if < 0.
//   Build a world Vector3 from the found cell { (cell%mapSize)-K, (cell/mapSize)-K, 0 }
//   where K = (float)(mapSize>>1) - 0.5, terrain-resolve it, then queue the action
//   triple via PlayAnimation_Action_Ctor5 with animTime = RandFloat*7 + 3.
// Returns true iff a cell was found (and the action queued).
//
// ABI note: WorkGrid_FindCell (0x14009ccf0) is integer-only and does NOT touch XMM0,
// so the original's `extraout_XMM0_Da` is the stale (float)(mapSize>>1) left in XMM0
// by the position math — reproduced here as `halfMapF`.
// ============================================================
bool CitizenAIComponent::State_Wander() {
    CitizenComponent* target = citizen->_parent;
    if (target == nullptr) return false;

    System::Random* rng = (GameUpdateLoop::_singleton != nullptr)
                ? GameUpdateLoop::_singleton->m_random
                : nullptr;

    if (target->spouse != nullptr &&
        rng && rng->Rand(1u) == 0) {
        target = target->spouse;
    }

    Map* grid = (GameUpdateLoop::_singleton != nullptr)
                ? GameUpdateLoop::_singleton->m_game
                : nullptr;
    if (!grid) return false;

    // --- cell coords from `target` model position ---
    const ModelComponent* tModel = target->character->modelComponent;
    int mapSize = grid->size;
    int half = mapSize >> 1;
    int tx = static_cast<int>(tModel->transform.translation.x + static_cast<float>(half));
    int ty = static_cast<int>(tModel->transform.translation.y + static_cast<float>(half));
    if (tx < 0) tx = 0;
    if (mapSize <= tx) tx = mapSize - 1;
    if (ty < 0) ty = 0;
    if (mapSize <= ty) ty = mapSize - 1;

    // --- cell coords from the citizen's model position ---
    const ModelComponent* sModel = citizen->character->modelComponent;
    int sx = static_cast<int>(sModel->transform.translation.x + static_cast<float>(half));
    int sy = static_cast<int>(sModel->transform.translation.y + static_cast<float>(half));
    if (sx < 0) sx = 0;
    if (mapSize <= sx) sx = mapSize - 1;
    if (sy < 0) sy = 0;
    if (mapSize <= sy) sy = mapSize - 1;

    int cell = WorkGrid_FindCell_9ccf0(reinterpret_cast<int*>(grid), mapSize * ty + tx, mapSize * sy + sx, 0xe1f);   // LEFT: SIG WorkGrid_FindCell_9ccf0 (int* map; Work/exe_bodies.h)
    if (cell < 0) return false;

    float kBias = kRdata_140172114;
    float K = static_cast<float>(half) - kBias;
    float inVec[3] = {
        static_cast<float>(cell % mapSize) - K,
        static_cast<float>(cell / mapSize) - K,
        0.0f,
    };
    float worldPos[3] = { 0.0f, 0.0f, 0.0f };
    Reimpl_MapGetTerrainPos(grid, worldPos, inVec, '\0', nullptr);

    CitizenComponent* citizenIBP = citizen;   // the CitizenComponent in a stack IBP holder
    float r = rng ? rng->RandFloat() : 0.0f;
    float animTime = r * kRdata_1401721a0 + kRdata_140172180;
    CitizenAI_PlayAnimationActionCtor5(&citizenIBP, worldPos, animTime);

    return true;
}

