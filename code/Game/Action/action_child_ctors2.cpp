// code/Game/Action/action_child_ctors2.cpp
// FUN_140119470: the weighted-random product index (no class yet; B.4 inventory A
// attributes it, low confidence, to ConsumeProduceUI).
//
// B.4 R0-EXE: the rest of this file moved, text unchanged, to the classes' files:
//   CompleteRoad door 0x1400c59d0                          -> completeroad.cpp
//   HuntAnimal ctor + door 0x1400c8400                     -> huntanimal.cpp
//   DropOffRawMaterial ctor + door 0x1400c7cd0             -> dropoffrawmaterial.cpp
//   ConsumeRawMaterial ctors + doors 0x1400c63b0 / 0x1400c6440 and its
//     consume-list append 0x1400c64b0                     -> consumerawmaterial.cpp
//   PickupStoredRawMaterial ctor + door 0x1400ca520 and its
//     carrier-list transfer 0x1400ca230                   -> pickupstoredrawmaterial.cpp
//   the StateMachineComponent find 0x14000b6f0 (ComponentEntity FindComponent<T>)
//                                                          -> Entity/entity_findcomponent.cpp
#include "Action/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include <cstdint>
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include "Component/ConsumeProduceComponent.h"

// ============================================================
// FUN_140119470 — weighted-random index over a float-weight array.
// If *(int*)(param_1+0x50) == -1: sum the weights[ +0x58 ] (count +0x60), draw
// r = RandFloat(world.random), walk the weights again returning the first index where
// the running sum exceeds r*total. Otherwise (or no weights) returns 0.
// (Returns ulonglong index; non-negative *(int*)(+0x50) returns that value.)
// ============================================================
uint64_t PickConsumeRecipe(const ConsumeProduceComponent* self) {
    uint32_t sel = static_cast<uint32_t>(self->consumeResourceId);
    uint64_t result = sel;
    if ((int)sel < 0) {
        if (sel == 0xffffffff) {
            int n = self->consumeCount;
            float total = 0.0f;
            if (0 < n) {
                const float* w = self->consumeRates;
                for (int k = 0; k < n; ++k) total += w[k];
            }
            System::Random* random = GameUpdateLoop::_singleton
                ? GameUpdateLoop::_singleton->m_random
                : nullptr;
            float r = random ? random->RandFloat() : 0.0f;
            result = 0;
            if (0 < n) {
                const float* w = self->consumeRates;
                float running = 0.0f;
                for (int k = 0; k < n; ++k) {
                    running += w[k];
                    if (r * total < running) return (uint64_t)k;
                }
            }
        }
        result = 0;
    }
    return result;
}

