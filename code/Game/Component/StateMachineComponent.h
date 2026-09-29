#pragma once
// ============================================================================
// code/Game/Component/StateMachineComponent.h — ENDGAME B.3.8 block VIS
//
// `StateMachineComponent` as a real C++ class with a compiler-generated vtable
// (B38_RULES.md D1-D4, D6). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "StateMachineComponent", parent "Component", ti 0x1401c7d98
//   create 0x1400382d0: MemAlloc(0x58, 8) + ctor 0x14012ba30
//   ctor   0x14012ba30: Component ctor, stamp 0x140164520; the MEMBER at +0x28:
//                       stamp 0x14015f070 (the interface), +0x30 = +0x38 = 0,
//                       stamp 0x1401644d0 (its own table), +0x40 = this;
//                       +0x48 = this+0x28; +0x50 = 0xff00ffff; flags |= 0xcd
//   dtor   0x14012b9e0: restamp 0x140164520, member restamp 0x14015f070,
//                       restamp 0x140162920, ~Internal (no own work)
//   table  0x140164520, 16 slots:
//     [0]  0x1400384a0 scalar deleting dtor  [1]/[2] TypeName/Type
//     [3]  0x14012b940 Persist (the four state bytes)
//     [4]  0x14012b6a0 Startup (the controller linker)
//     [6]  0x14012b660 OnAdded (Tick(sm, Time(0), 0, 0) — the register-time tick)
//     [8]  0x14012b640 Tick    (Tick(sm, time, 0, 1) — the tree-walk tick)
//     [9]  0x14012b620 Update  (Tick(sm, time, 1, 1) — the bucket tick)
//     [11] 0x14012b7d0 Slot11  (relink + walk) PORT GAP
//     every other slot = Component's
//   member table 0x1401644d0, 9 slots (the controller interface 0x14015f070,
//   ControllerComponent::Controller): the ROOT node of the state-node chain.
//
// NOT MULTIPLE INHERITANCE (B38_RULES §0.1, D6): StateMachineComponent derives
// Component only; the root node is a MEMBER at +0x28, and +0x48.. is the
// StateMachine the tick runs (+0x48 current node, +0x50 input / +0x51 active /
// +0x52 pending / +0x53 transition state bytes).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Component/ControllerComponent.h"
#include "Object/exe_type.h"

class StateMachineComponent final : public Component {
    DECLARE_EXE_TYPE(StateMachineComponent)

public:
    // The root node of the state-node chain (member table 0x1401644d0). The
    // controllers' nodes hang off it (+0x08 parent, +0x10 child).
    // PORT GAP (retail 0x140038630-0x140038800) — B38 D9: retail's root answers
    // from a global state-type table (0x1401e2830, empty in standalone: its
    // FindState returns -1 and would drive Tick's parent-walk into a null).
    // Every slot answers 0 here, which is what the tree's stub table
    // (s_smStubSecondaryVtable) answered.
    class RootController final : public ControllerComponent::Controller {
    public:
        explicit RootController(Component* owner) { _owner = owner; }
        ~RootController() = default;

        int  FindState(int stateId) override;                                    // [0] 0x140038630
        int  FindTransition(int stateIndex, int* desiredStateId) override;       // [1] 0x140038660
        char PostEvent(void* stateMachine, int stateIndex, int eventId) override; // [2] 0x1400386c0
        char Enter(void* stateMachine, int stateIndex) override;                 // [3] 0x140038710
        char Update(void* stateMachine, int stateIndex) override;                // [4] 0x140038740
        void OnTransition(void* stateMachine, int stateIndex,
                          int transitionIndex) override;                         // [5] 0x140038770
        char Tick(void* stateMachine, void* time, int stateIndex) override;      // [6] 0x1400387a0
        char TickAlt(void* stateMachine, void* time, int stateIndex) override;   // [7] 0x1400387d0
        char HasStates() override;                                               // [8] 0x140038800
    };

    StateMachineComponent();                                      // 0x14012ba30

    // 0x14012b9e0
    ~StateMachineComponent() override = default;                  // [0]  0x1400384a0
    void Persist(void* stream) override;                          // [3]  0x14012b940
    void Startup() override;                                      // [4]  0x14012b6a0
    void OnAdded() override;                                      // [6]  0x14012b660
    void Tick(void* time) override;                               // [8]  0x14012b640
    void Update(void* time) override;                             // [9]  0x14012b620
    void Slot11() override;                                       // [11] 0x14012b7d0 PORT GAP

    // PUBLIC while the ported bodies read them (B38 decision 1).
    RootController _controller;   // +0x28 root node (vptr, +0x30 parent, +0x38 first child, +0x40 owner)
    // +0x48.. is the StateMachine StateMachine::Tick (0x1400a9660) runs: `&controllerSelf`
    // is its address. The four state bytes are signed (Tick's MOVSX / JS tests; Persist
    // 0x14012b940 serializes each through Serialize(signed char&), import 0x140152418);
    // the retail ctor writes them as one dword 0xff00ffff (input, active, transition
    // = -1, pending = 0).
    ControllerComponent::Controller* controllerSelf; // +0x48 current node (retail ctor this+0x28)
    int8_t   currentInput;        // +0x50 the state the machine was asked for last
    int8_t   activeState;         // +0x51 the running state's table index (-1: none)
    int8_t   pendingState;        // +0x52 the requested state (-1: nothing pending)
    int8_t   transitionState;     // +0x53 the transition being run (-1: none)
    uint8_t  _pad54[4];           // +0x54
};

static_assert(sizeof(StateMachineComponent) == 0x58, "StateMachineComponent is 0x58 (create 0x1400382d0)");
static_assert(alignof(StateMachineComponent) == 8, "StateMachineComponent alignment");
static_assert(sizeof(StateMachineComponent::RootController) == 0x20, "the root node is 0x20 bytes");
static_assert(offsetof(StateMachineComponent, _controller) == 0x28, "member root node at +0x28 (0x14012ba5b)");
static_assert(offsetof(StateMachineComponent, controllerSelf) == 0x48, "StateMachine current node +0x48 (0x14012ba86)");
static_assert(offsetof(StateMachineComponent, currentInput) == 0x50, "state bytes +0x50 (0x14012ba8a)");
static_assert(offsetof(StateMachineComponent, activeState) == 0x51, "active state +0x51 (Persist 0x14012b983)");
static_assert(offsetof(StateMachineComponent, pendingState) == 0x52, "pending state +0x52 (Persist 0x14012b995)");
static_assert(offsetof(StateMachineComponent, transitionState) == 0x53, "transition state +0x53 (Persist 0x14012b9a7)");
