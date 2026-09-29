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

// ============================================================
// FUN_140119470 — weighted-random index over a float-weight array.
// If *(int*)(param_1+0x50) == -1: sum the weights[ +0x58 ] (count +0x60), draw
// r = RandFloat(world.random), walk the weights again returning the first index where
// the running sum exceeds r*total. Otherwise (or no weights) returns 0.
// (Returns ulonglong index; non-negative *(int*)(+0x50) returns that value.)
// ============================================================
uint64_t WeightedRandomIndex_119470(int64_t param_1) {
    uint32_t sel = *reinterpret_cast<uint32_t*>(param_1 + 0x50);
    uint64_t result = sel;
    if ((int)sel < 0) {
        if (sel == 0xffffffff) {
            int n = *reinterpret_cast<int*>(param_1 + 0x60);
            float total = 0.0f;
            if (0 < n) {
                float* w = *reinterpret_cast<float**>(param_1 + 0x58);
                for (int k = 0; k < n; ++k) total += w[k];
            }
            void* random = GameUpdateLoop::_singleton
                ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) + 0xd8)
                : nullptr;
            float r = random ? reinterpret_cast<System::Random*>(random)->RandFloat() : 0.0f;   // B.3.4b bridge
            result = 0;
            if (0 < n) {
                float* w = *reinterpret_cast<float**>(param_1 + 0x58);
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
