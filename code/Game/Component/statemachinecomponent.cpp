// code/Game/Component/statemachinecomponent.cpp
// Clean reimplementation of StateMachineComponent vtable methods.
//
// StateMachineComponent (0x58 bytes) -- state machine tick dispatcher.
// Vtable: 0x140164520
//
// The component wraps a "controller" sub-object at +0x28 and a state machine
// pointer at +0x48. Most vtable slots delegate to StateMachine_Tick with
// different parameter combinations. Serialize/OnUpdate2 iterate the owning
// entity's components to find and link ControllerComponents.
//
// Layout:
//   +0x00: Component base (0x28 bytes: vtable, next, owner, unk18, flags)
//   +0x28: Controller sub-object (secondary vtable + 2 qwords)
//   +0x40: ownerComponent (back-pointer to self)
//   +0x48: controllerSelf (points to self + 0x28)
//   +0x50: stateFlags (uint32, init 0xFF00FFFF)
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/statemachinecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Locally resolved function pointers
// ============================================================

// StateMachine_Tick(stateMachine*, Time, bool processTransitions, bool processActions)
// Located at 0x1400a9660 in Application-x64.exe
typedef void (*fn_StateMachine_Tick_t)(void* stateMachine, int64_t deltaTime, char param3, char param4);
static fn_StateMachine_Tick_t s_fn_Tick = nullptr;

// Component_Serialize(this, Stream*) -- Component base serialize at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer::Serialize(this, char*, const char*) -- byte serialize
// Dispatch slot 0x140152418 in exe .rdata
typedef void (*fn_Serialize_Char_t)(void* self, void* field, const char* name);
static fn_Serialize_Char_t s_fn_Serialize_Char = nullptr;

// System::Time::Time(retval, float) -- construct Time from float
// Already in engine_funcs as fn_Time_Ctor_Float

// Object::Internal::~Internal -- already in engine_funcs as fn_ObjectInternal_Dtor

// ControllerComponent type ID at 0x1401c73c0 (filled at runtime by RegisterType)
static uint32_t* s_TypeId_ControllerComponent = nullptr;

// StateMachineComponent vtable address (for dtor vtable restoration)
static void* s_vtable_SMC = nullptr;
// Purecall vtable for controller sub-object (0x14015f070)
static void* s_vtable_purecall = nullptr;
// Component base vtable (0x140162920)
static void* s_vtable_ComponentBase = nullptr;
// StateMachineComponent type ID at 0x1401c7d98
static uint32_t* s_TypeId_SMC = nullptr;

static bool s_resolved = false;

// ============================================================
// Resolve helpers (call once from Install)
// ============================================================
void StateMachineComponent_ResolveHelpers() {
    if (s_resolved) return;

    s_fn_Tick = reinterpret_cast<fn_StateMachine_Tick_t>(
        ResolveAddr(Module::Exe, 0x1400a9660));

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    // Resolve byte serializer from dispatch slot
    {
        void** slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152418));
        if (slot && *slot)
            s_fn_Serialize_Char = reinterpret_cast<fn_Serialize_Char_t>(*slot);
    }

    s_TypeId_ControllerComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c73c0));

    s_TypeId_SMC = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7d98));

    s_vtable_SMC = reinterpret_cast<void*>(
        ResolveAddr(Module::Exe, 0x140164520));

    s_vtable_purecall = reinterpret_cast<void*>(
        ResolveAddr(Module::Exe, 0x14015f070));

    s_vtable_ComponentBase = reinterpret_cast<void*>(
        ResolveAddr(Module::Exe, 0x140162920));

    s_resolved = true;

#ifdef _DEBUG
    printf("[SMC] Helpers resolved: Tick=%p CompSerialize=%p SerializeChar=%p "
           "TypeCtrl=%p TypeSMC=%p\n",
           (void*)s_fn_Tick, (void*)s_fn_Component_Serialize,
           (void*)s_fn_Serialize_Char,
           (void*)s_TypeId_ControllerComponent, (void*)s_TypeId_SMC);
#endif
}

// ============================================================
// ScalarDtor (vtable slot [0], 0x1400384a0)
// ============================================================
void* StateMachineComponent_ScalarDtor(StateMachineComponent* self, uint32_t flags) {
    // Restore vtables in destruction order: SMC -> purecall -> Component base
    self->vtable = s_vtable_SMC;

    uint8_t* base = reinterpret_cast<uint8_t*>(self);
    // Controller sub-object at +0x28 gets purecall vtable
    *reinterpret_cast<void**>(base + 0x28) = s_vtable_purecall;
    // Then set to Component base vtable
    self->vtable = s_vtable_ComponentBase;

    // Call Object::Internal::~Internal
    if (fn_ObjectInternal_Dtor) {
        fn_ObjectInternal_Dtor(self);
    }

    // Conditionally free memory
    if (flags & 1) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self);
        }
    }

    return self;
}

// ============================================================
// Startup/GetType (vtable slot [2], 0x1400382b0)
// ============================================================
uint32_t StateMachineComponent_Startup(StateMachineComponent* /*self*/) {
    // Returns the registered type ID for StateMachineComponent.
    // This is filled at runtime by RegisterType.
    if (s_TypeId_SMC)
        return *s_TypeId_SMC;
    return 0;
}

// ============================================================
// Shutdown (vtable slot [3], 0x14012b940)
// ============================================================
void StateMachineComponent_Shutdown(StateMachineComponent* self, void* stream) {
    // Call Component base serialize first
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Create IO::Serializer with version 0
    uint8_t serializer[24]; // IO::Serializer is 24 bytes (stack local)
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(serializer, stream, 0);
    }

    // Serialize 4 individual bytes from stateFlags at +0x50, +0x51, +0x52, +0x53
    uint8_t* base = reinterpret_cast<uint8_t*>(self);
    if (s_fn_Serialize_Char) {
        s_fn_Serialize_Char(serializer, base + 0x50, nullptr);
        s_fn_Serialize_Char(serializer, base + 0x51, nullptr);
        s_fn_Serialize_Char(serializer, base + 0x52, nullptr);
        s_fn_Serialize_Char(serializer, base + 0x53, nullptr);
    }

    // Destroy serializer
    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// Serialize (vtable slot [4], 0x14012b6a0)
// Post-deserialization re-linking of ControllerComponents.
// ============================================================
void StateMachineComponent_Serialize(StateMachineComponent* self) {
    uint8_t* base = reinterpret_cast<uint8_t*>(self);
    // Controller sub-object pointer (linked list tail tracker)
    uint8_t* controllerNode = base + 0x28; // starts at controller sub-object
    bool gotDefaultState = false;

    // Check owner InternalBasePtr
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(base + 0x10);

    // Get owner entity
    void* ownerEntity = *reinterpret_cast<void**>(base + 0x10);
    if (!ownerEntity) return;

    // Get component count: owner + 0x38 = count, owner + 0x30 = array base
    int componentCount = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(ownerEntity) + 0x38);
    if (componentCount <= 0) return;

    uintptr_t arrayBase = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uint8_t*>(ownerEntity) + 0x30);

    for (int i = 0; i < componentCount; i++) {
        // Re-check owner pointer each iteration (CheckPtr may update it)
        if (fn_InternalBasePtr_CheckPtr)
            fn_InternalBasePtr_CheckPtr(base + 0x10);

        ownerEntity = *reinterpret_cast<void**>(base + 0x10);
        arrayBase = *reinterpret_cast<uintptr_t*>(
            reinterpret_cast<uint8_t*>(ownerEntity) + 0x30);

        // Read component pointer from array
        void* tempIBP = nullptr; // local InternalBasePtr for component
        void* comp = *reinterpret_cast<void**>(arrayBase + i * 8);

        // CheckPtr on the component IBP
        tempIBP = comp;
        if (fn_InternalBasePtr_CheckPtr)
            fn_InternalBasePtr_CheckPtr(&tempIBP);
        comp = tempIBP;

        // Check if this component is derived from ControllerComponent
        void* candidate = nullptr;
        if (fn_IsDerivedFrom && s_TypeId_ControllerComponent) {
            if (fn_IsDerivedFrom(comp, *s_TypeId_ControllerComponent))
                candidate = comp;
        }

        // Wrap in InternalBasePtr
        void* ibpValue = nullptr;
        if (fn_InternalBasePtr_Ctor)
            fn_InternalBasePtr_Ctor(&ibpValue, candidate);

        if (ibpValue != nullptr) {
            if (!gotDefaultState) {
                // First ControllerComponent: call virtual +0x80 (GetDefaultState)
                if (fn_InternalBasePtr_CheckPtr)
                    fn_InternalBasePtr_CheckPtr(&ibpValue);
                uintptr_t vtbl = *reinterpret_cast<uintptr_t*>(ibpValue);
                typedef uint8_t (*GetDefaultState_t)(void*);
                auto fn = *reinterpret_cast<GetDefaultState_t*>(vtbl + 0x80);
                uint8_t defaultState = fn(ibpValue);
                gotDefaultState = true;
                // Store default state at +0x52
                *(base + 0x52) = defaultState;
            }

            // Call virtual +0x88 (CreateState) to get a state node
            if (fn_InternalBasePtr_CheckPtr)
                fn_InternalBasePtr_CheckPtr(&ibpValue);
            uintptr_t vtbl = *reinterpret_cast<uintptr_t*>(ibpValue);
            typedef void* (*CreateState_t)(void*);
            auto fn = *reinterpret_cast<CreateState_t*>(vtbl + 0x88);
            void* stateNode = fn(ibpValue);

            // Link the state node into the controller chain:
            // stateNode + 0x08 = pointer to controller sub-object
            // controllerNode + 0x10 = pointer to this state node
            *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(stateNode) + 0x08) =
                reinterpret_cast<void*>(controllerNode);
            *reinterpret_cast<void**>(controllerNode + 0x10) =
                stateNode;
            controllerNode = reinterpret_cast<uint8_t*>(stateNode);
        }

        // Re-check count (may have changed if CheckPtr triggered lazy load)
        if (fn_InternalBasePtr_CheckPtr)
            fn_InternalBasePtr_CheckPtr(base + 0x10);
        ownerEntity = *reinterpret_cast<void**>(base + 0x10);
        componentCount = *reinterpret_cast<int*>(
            reinterpret_cast<uint8_t*>(ownerEntity) + 0x38);
    }
}

// ============================================================
// Draw (vtable slot [6], 0x14012b660)
// ============================================================
void StateMachineComponent_Draw(StateMachineComponent* self) {
    if (!s_fn_Tick) return;

    // Construct Time(0.0) on stack
    int64_t zeroTime = 0;
    if (fn_Time_Ctor_Float) {
        fn_Time_Ctor_Float(&zeroTime, 0.0f);
    }

    // StateMachine_Tick(&controllerSelf, zeroTime, false, false)
    s_fn_Tick(&self->controllerSelf, zeroTime, 0, 0);
}

// ============================================================
// OnRemove (vtable slot [8], 0x14012b640)
// ============================================================
void StateMachineComponent_OnRemove(StateMachineComponent* self, int64_t deltaTime) {
    if (!s_fn_Tick) return;

    // StateMachine_Tick(&controllerSelf, deltaTime, false, true)
    s_fn_Tick(&self->controllerSelf, deltaTime, 0, 1);
}

// ============================================================
// Update (vtable slot [9], 0x14012b620)
// ============================================================
void StateMachineComponent_Update(StateMachineComponent* self, int64_t deltaTime) {
    if (!s_fn_Tick) return;

    // StateMachine_Tick(&controllerSelf, deltaTime, true, true)
    s_fn_Tick(&self->controllerSelf, deltaTime, 1, 1);
}

// ============================================================
// OnUpdate2 (vtable slot [11], 0x14012b7d0)
// Same ControllerComponent iteration as Serialize, then walks
// the state machine linked list to dispatch a virtual call.
// ============================================================
void StateMachineComponent_OnUpdate2(StateMachineComponent* self) {
    uint8_t* base = reinterpret_cast<uint8_t*>(self);
    uint8_t* controllerNode = base + 0x28;

    // Check owner InternalBasePtr
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(base + 0x10);

    void* ownerEntity = *reinterpret_cast<void**>(base + 0x10);
    if (!ownerEntity) goto state_walk;

    {
        int componentCount = *reinterpret_cast<int*>(
            reinterpret_cast<uint8_t*>(ownerEntity) + 0x38);
        if (componentCount <= 0) goto state_walk;

        for (int i = 0; i < componentCount; i++) {
            if (fn_InternalBasePtr_CheckPtr)
                fn_InternalBasePtr_CheckPtr(base + 0x10);

            ownerEntity = *reinterpret_cast<void**>(base + 0x10);
            uintptr_t arrayBase = *reinterpret_cast<uintptr_t*>(
                reinterpret_cast<uint8_t*>(ownerEntity) + 0x30);

            void* tempIBP = *reinterpret_cast<void**>(arrayBase + i * 8);

            if (fn_InternalBasePtr_CheckPtr)
                fn_InternalBasePtr_CheckPtr(&tempIBP);
            void* comp = tempIBP;

            void* candidate = nullptr;
            if (fn_IsDerivedFrom && s_TypeId_ControllerComponent) {
                if (fn_IsDerivedFrom(comp, *s_TypeId_ControllerComponent))
                    candidate = comp;
            }

            void* ibpValue = nullptr;
            if (fn_InternalBasePtr_Ctor)
                fn_InternalBasePtr_Ctor(&ibpValue, candidate);

            if (ibpValue != nullptr) {
                // Call virtual +0x88 (CreateState) to get a state node
                if (fn_InternalBasePtr_CheckPtr)
                    fn_InternalBasePtr_CheckPtr(&ibpValue);
                uintptr_t vtbl = *reinterpret_cast<uintptr_t*>(ibpValue);
                typedef void* (*CreateState_t)(void*);
                auto fn = *reinterpret_cast<CreateState_t*>(vtbl + 0x88);
                void* stateNode = fn(ibpValue);

                // Link into chain
                *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(stateNode) + 0x08) =
                    reinterpret_cast<void*>(controllerNode);
                *reinterpret_cast<void**>(controllerNode + 0x10) =
                    stateNode;
                controllerNode = reinterpret_cast<uint8_t*>(stateNode);
            }

            // Re-check count
            if (fn_InternalBasePtr_CheckPtr)
                fn_InternalBasePtr_CheckPtr(base + 0x10);
            ownerEntity = *reinterpret_cast<void**>(base + 0x10);
            componentCount = *reinterpret_cast<int*>(
                reinterpret_cast<uint8_t*>(ownerEntity) + 0x38);
        }
    }

state_walk:
    // Second part: walk the state machine linked list if stateFlags byte is >= 0
    {
        uint8_t stateFlag = *(base + 0x50);
        if (static_cast<int8_t>(stateFlag) < 0) return; // JS: signed < 0 means bit 7 set

        // self->controllerSelf (at +0x48) points to the current state node
        void** currentNodePtr = reinterpret_cast<void**>(base + 0x48);
        void* currentNode = *currentNodePtr;

        // Walk to tail: follow next pointers at node + 0x10
        void* next = *reinterpret_cast<void**>(
            reinterpret_cast<uint8_t*>(currentNode) + 0x10);
        while (next != nullptr) {
            currentNode = next;
            next = *reinterpret_cast<void**>(
                reinterpret_cast<uint8_t*>(currentNode) + 0x10);
        }

        // Update controllerSelf to point to the tail
        *currentNodePtr = currentNode;

        // Call virtual slot [0] of the tail node's vtable with stateFlag as arg
        // Returns int: negative means walk back up the parent chain
        uintptr_t nodeVtbl = *reinterpret_cast<uintptr_t*>(currentNode);
        typedef int (*VSlot0_t)(void* self, int state);
        auto fn0 = *reinterpret_cast<VSlot0_t*>(nodeVtbl);
        int result = fn0(currentNode, static_cast<int>(static_cast<int8_t>(stateFlag)));

        while (result < 0) {
            // Walk up: follow parent pointer at node + 0x08
            void* parentNode = *reinterpret_cast<void**>(
                reinterpret_cast<uint8_t*>(*currentNodePtr) + 0x08);
            *currentNodePtr = parentNode;

            // Call slot [0] on parent
            nodeVtbl = *reinterpret_cast<uintptr_t*>(parentNode);
            fn0 = *reinterpret_cast<VSlot0_t*>(nodeVtbl);
            stateFlag = *(base + 0x50);
            result = fn0(parentNode, static_cast<int>(static_cast<int8_t>(stateFlag)));
        }
    }
}

// ============================================================
// Reimpl of FUN_1400a9660 (StateMachine::Tick: advance the active state-node
//   chain, firing Enter/Update/Exit/Transition virtuals)
// ============================================================
// Faithful port of the Ghidra decompilation. param_1 points at the StateMachine.
// Byte fields: +0x08 = currentInput, +0x09 = activeState, +0x0a = pendingState
// (0xff sentinel = none), +0x0b = transitionState. param_1[0] = active node ptr.
// Node vtable slots used: +0x00 Step(int)->int, +0x18 Enter, +0x20 Update,
// +0x28 OnState, +0x30/+0x38 Transition; node+0x08 parent, node+0x10 first child.
void StateMachine_TickReimpl(void* smP, int64_t time, char param3, char param4) {
    int64_t* sm = (int64_t*)smP;
    typedef char (*VFn_self_sm_t)(int64_t* node, int64_t* sm);
    typedef int  (*VFn_step_t)(int64_t* node, int input);
    typedef void (*VFn_onstate_t)(int64_t* node, int64_t* sm, int a, int b);
    typedef char (*VFn_trans_t)(int64_t* node, int64_t* sm, int64_t time);

    if (*(char*)((uintptr_t)sm + 10) < 0) goto LAB_transition;
    do {
        do {
            if ((char)sm[1] >= 0) {
                int iVar5 = (int)*(char*)((uintptr_t)sm + 9);
                int64_t* node = (int64_t*)sm[0];
                while (((iVar5 < 0 ||
                        (((VFn_self_sm_t)(*(uintptr_t*)(*node + 0x20)))(node, sm) == '\0')) &&
                       ((node = (int64_t*)node[1]) != nullptr))) {
                    iVar5 = ((VFn_step_t)(*(uintptr_t*)*node))(node, (int)(char)sm[1]);
                }
                if (*(char*)((uintptr_t)sm + 0xb) >= 0) {
                    ((VFn_onstate_t)(*(uintptr_t*)(*(int64_t*)sm[0] + 0x28)))(
                        (int64_t*)sm[0], sm,
                        (int)*(char*)((uintptr_t)sm + 9),
                        (int)*(char*)((uintptr_t)sm + 0xb));
                }
            }
            *(char*)((uintptr_t)sm + 8) = *(char*)((uintptr_t)sm + 10);
            // Walk to the chain tail (node+0x10 = child pointer)
            int64_t* tail = (int64_t*)sm[0];
            for (int64_t* p = (int64_t*)((int64_t*)sm[0])[2]; p != nullptr; p = (int64_t*)p[2]) {
                tail = p;
            }
            sm[0] = (int64_t)tail;
            char c = ((VFn_step_t)(*(uintptr_t*)*tail))(tail, (int)*(char*)((uintptr_t)sm + 10)) & 0xff;
            *(char*)((uintptr_t)sm + 9) = c;
            while (c < 0) {
                int64_t* up = *(int64_t**)(sm[0] + 8);
                sm[0] = (int64_t)up;
                c = ((VFn_step_t)(*(uintptr_t*)*up))(up, (int)(char)sm[1]) & 0xff;
                *(char*)((uintptr_t)sm + 9) = c;
            }
            int iVar5 = (int)*(char*)((uintptr_t)sm + 9);
            int64_t* node = (int64_t*)sm[0];
            *(int16_t*)((uintptr_t)sm + 10) = (int16_t)0xffff;
            while (((iVar5 < 0 ||
                    (((VFn_self_sm_t)(*(uintptr_t*)(*node + 0x18)))(node, sm) == '\0')) &&
                   ((node = (int64_t*)node[1]) != nullptr))) {
                iVar5 = ((VFn_step_t)(*(uintptr_t*)*node))(node, (int)(char)sm[1]);
            }
        } while (*(char*)((uintptr_t)sm + 10) >= 0);
    LAB_transition:
        if (param4 != '\0' && (char)sm[1] >= 0) {
            int iVar5 = (int)*(char*)((uintptr_t)sm + 9);
            int64_t* node = (int64_t*)sm[0];
            if (param3 == '\0') {
                while (((iVar5 < 0 ||
                        (((VFn_trans_t)(*(uintptr_t*)(*node + 0x30)))(node, sm, time) == '\0')) &&
                       ((node = (int64_t*)node[1]) != nullptr))) {
                    iVar5 = ((VFn_step_t)(*(uintptr_t*)*node))(node, (int)(char)sm[1]);
                }
            } else {
                while (((iVar5 < 0 ||
                        (((VFn_trans_t)(*(uintptr_t*)(*node + 0x38)))(node, sm, time) == '\0')) &&
                       ((node = (int64_t*)node[1]) != nullptr))) {
                    iVar5 = ((VFn_step_t)(*(uintptr_t*)*node))(node, (int)(char)sm[1]);
                }
            }
            param4 = '\0';
        }
    } while (*(char*)((uintptr_t)sm + 10) >= 0);
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// size 0x58 (StateMachineComponent_Factory MemAlloc), flags 0xcd (Ctor
// 0x14012ba30: Component base sets +0x20=0x80, leaf OR 0xcd => 0xcd).
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(StateMachineComponent, 0x58, 0xcd)
REGISTER_TYPE(StateMachineComponent, "Component",
              &StateMachineComponent_Factory, &StateMachineComponent_CreateInPlace);
#endif // STANDALONE_EXE
