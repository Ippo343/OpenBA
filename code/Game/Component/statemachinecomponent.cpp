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
//
// ⚠ UNREGISTERED DEAD CODE — kept as the faithful reference port only. Do NOT
// register it for 0x14012b6a0: it writes the first controller's default into
// +0x52 UNCONDITIONALLY, which is exactly the clobber the lazy-link fix in
// LinkControllers_Standalone exists to avoid (retail can only afford this
// because its linker runs at component creation, when +0x52 is factory-zero).
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
    // Node-interface ABI (verified from StateMachine::Tick 0x1400a9660 disasm): the
    // state index is carried in R8D for Enter/Update and R9D for Transition (time in
    // R8). Ghidra DROPS these in the decompile; the real dispatchers read them.
    typedef char (*VFn_enterupd_t)(int64_t* node, int64_t* sm, int idx);
    typedef int  (*VFn_step_t)(int64_t* node, int input);
    typedef void (*VFn_onstate_t)(int64_t* node, int64_t* sm, int a, int b);
    typedef char (*VFn_trans_t)(int64_t* node, int64_t* sm, int64_t time, int idx);

    if (*(char*)((uintptr_t)sm + 10) < 0) goto LAB_transition;
    do {
        do {
            if ((char)sm[1] >= 0) {
                int iVar5 = (int)*(char*)((uintptr_t)sm + 9);
                int64_t* node = (int64_t*)sm[0];
                while (((iVar5 < 0 ||
                        (((VFn_enterupd_t)(*(uintptr_t*)(*node + 0x20)))(node, sm, iVar5) == '\0')) &&
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
                    (((VFn_enterupd_t)(*(uintptr_t*)(*node + 0x18)))(node, sm, iVar5) == '\0')) &&
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
                        (((VFn_trans_t)(*(uintptr_t*)(*node + 0x30)))(node, sm, time, iVar5) == '\0')) &&
                       ((node = (int64_t*)node[1]) != nullptr))) {
                    iVar5 = ((VFn_step_t)(*(uintptr_t*)*node))(node, (int)(char)sm[1]);
                }
            } else {
                while (((iVar5 < 0 ||
                        (((VFn_trans_t)(*(uintptr_t*)(*node + 0x38)))(node, sm, time, iVar5) == '\0')) &&
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
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
#include <cstdlib>
#include <cstring>

// ------------------------------------------------------------------
// Phase 3 / Keystone D — StateMachineComponent live-wiring brick.
//
// SM is a *pure dispatcher*: its Update (+0x48) just runs StateMachine::Tick
// over the state-node chain rooted at the controller sub-object (+0x28). On a
// freshly-spawned standalone citizen that chain is EMPTY — the root's child
// pointer (+0x38) is 0 — because the chain is only populated by the +0x20 slot
// (StateMachineComponent_Serialize) calling each ControllerComponent's
// CreateState(+0x88), and the ControllerComponents (CitizenAI/Citizen) are
// still inert in standalone (their CreateState is a no-op stub). Moreover the
// bare root's Step virtual (orig 0x140038630) searches a global state-type
// table (0x1401e2830) that is empty in standalone and returns -1, which would
// drive Tick's parent-walk into a null deref.
//
// So this brick brings the Update slot LIVE but CRASH-SAFE: it body-inits the
// SM fields exactly as the ctor (0x14012ba30) does, then GUARDS Tick to a
// no-op until a real child state node is linked (base+0x38 != 0). It produces
// no observable behavior yet — that arrives when the ControllerComponents go
// live and link states (the next Phase-3 step). What it proves: +0x48 routing,
// +0x20 body-init, and flag-eligibility on the citizen tier, crash-safe under
// the autostart sim. Default (env off) => fully inert => byte-unchanged run.
// ------------------------------------------------------------------

// Safe-stub secondary controller-node vtable. The real one (0x1401644d0) points
// at exe code that is unmapped in standalone; the guard means Tick never derefs
// this in the current build, but a stub whose Step (slot 0) returns 0 (>=0, so
// Tick's `while (c<0)` parent-walk is skipped) keeps a future empty-chain tick
// safe too (vs. the real Step's crashing -1 on an empty state-type table).
static int64_t SM_StubSecondaryVirtual(void*) { return 0; }
static void* s_smStubSecondaryVtable[16];

// Recipe step 2 (body-init), routed to slot +0x20 (StartupComponents dispatch).
// Faithful field init from the ctor 0x14012ba30 (idempotent). Component base +
// flags(0xcd) are already set by the inert factory; this adds the SM-specific
// controller sub-object + back-pointers + state word.
static void StateMachineComponent_BodyInit(StateMachineComponent* self) {
    if (!self) return;
    uint8_t* base = reinterpret_cast<uint8_t*>(self);

    // Fill the stub secondary vtable once.
    if (s_smStubSecondaryVtable[0] == nullptr) {
        for (int i = 0; i < 16; ++i)
            s_smStubSecondaryVtable[i] =
                reinterpret_cast<void*>(&SM_StubSecondaryVirtual);
    }

    // +0x28 controller sub-object vtable, +0x30/+0x38 zeroed children,
    // +0x40 ownerComponent = self, +0x48 controllerSelf = self+0x28,
    // +0x50 stateFlags = 0xFF00FFFF.
    *reinterpret_cast<void**>(base + 0x28) =
        reinterpret_cast<void*>(s_smStubSecondaryVtable);
    *reinterpret_cast<uint64_t*>(base + 0x30) = 0;
    *reinterpret_cast<uint64_t*>(base + 0x38) = 0;
    *reinterpret_cast<void**>(base + 0x40) = self;
    *reinterpret_cast<void**>(base + 0x48) = base + 0x28;
    *reinterpret_cast<uint32_t*>(base + 0x50) = 0xff00ffff;
}

// Standalone variant of the controller-linker (real linker =
// StateMachineComponent_Startup 0x14012b6a0). Walks the owner entity's components
// and links each *live* ControllerComponent's embedded state node into the SM
// chain (a live controller = CreateState (vtable +0x88) returns a non-null,
// non-self node; inert components' +0x88 stub returns null and links nothing).
//
// DEFAULT-STATE selection: retail 0x14012b6a0 reads GetDefaultState (+0x80) from
// the FIRST component in array order that IsDerivedFrom(ControllerComponent) —
// liveness is not a concept there. In standalone most controllers are inert
// (their +0x80 is Inert_Stub), so we keep taking the default from the first
// LIVE controller — with one retail-ordering constraint that fixes the
// 2026-07-20 stockpile regression: a live DESTROYCOMPONENT may supply the
// default ONLY when it is also the first ControllerComponent-derived component
// in array order (registry-chain test, Object::IsTypeDerivedFromByName). The old
// first-LIVE rule made DestroyComponent "first" on a stockpile and its default of
// 4 booted the fresh entity straight into haul-away demolition. Retail gives
// destroy's 4 only to entities where destroy genuinely is the first controller
// (the starting supply cart's haul-away-then-vanish); for a destroy-preceded-by-
// inert entity we leave +0x52 = 0 (BodyInit value) — a state no live table
// registers, so every node stays dormant until a real request (the removal tool's
// state-3 pump) arrives. +0x52 is the Tick block's PENDING field: whatever lands
// here is ENTERED (enter-handler and all) on the first tick, which is why a wrong
// default here demolishes buildings.
//
// MEASURED 2026-07-27 (BANISHED_SM_TRACE LINK-DECIDE, start-town entities) — what
// retail's first-in-order rule ACTUALLY yields, with each GetDefaultState read out
// of the shipped exe vtable rather than inferred:
//   * 19-component buildings (StorageBarn + all 6 WoodHouses): first controller is
//     [14] ClearAreaComponent, +0x80 = 0x140003730 = `xor eax,eax; ret` -> 0.
//     BuildComponent (default 1, vtable +0x80 = 0x14002f2f0) is only [15], so its 1
//     is NEVER the linker-supplied default in retail.
//   * 13-component stockpile (StorageYard): first controller is [9]
//     AbsorbResourceComponent, also 0x140003730 -> 0. StorageLocationComponent
//     (default 2, +0x80 = 0x1400026c0) is LAST at [12].
// So retail's linker stamps 0 on every one of these entities, and the real spawn
// states (build's 1, storage's 2) are posted by something else. Our first-LIVE rule
// is therefore a DEVIATION that happens to reproduce the visible outcome for
// buildings — not a degraded copy of retail. Do not "fix" it toward first-in-order
// expecting the stockpile to gain state 2: in this build that reads Inert_Stub's 0,
// and in retail it reads a genuine 0.
// Resolve a runtime component type-id back to a readable name by matching it
// against the type registry (diagnostics only; "?" when the type is not one of
// the names we register). Shared by the linker's decision trace and the
// per-tick owner dump below.
static const char* SMTrace_ComponentName(uint32_t tid) {
    static const char* const kCandidates[] = {
        "StateMachineComponent", "StorageLocationComponent", "StorageComponent",
        "WorkPlaceComponent", "ResidenceComponent", "AttendanceComponent",
        "HappinessComponent", "HealerComponent", "CemeteryComponent",
        "ImmigrationComponent", "MarketLocationComponent", "NomadComponent",
        "OrchardComponent", "TradeComponent", "WellComponent",
        "AbsorbResourceComponent", "BuildComponent", "ClearAreaComponent",
        "ClearResourceComponent", "CropFieldComponent", "DestroyComponent",
        "MultiObjectComponent", "PastureComponent", "ResourceProduceComponent",
        "CitizenComponent", "CitizenAIComponent", "CharacterComponent",
        "StorageDisplayComponent", "RawMaterialComponent", "ModelComponent",
        "GrowthComponent", "FlammableComponent", "DecalComponent",
        "MapComponent", "PickingComponent", "InteractComponent",
        "HighlightComponent", "NaturalResourceComponent", "UIComponent",
        "ParticleComponent", "WorkComponent", "StatusIconComponent",
    };
    if (tid == 0) return "?";
    for (const char* cand : kCandidates)
        if (::Object::GetTypeIdByName(cand) == tid) return cand;
    return "?";
}

static void StateMachineComponent_LinkControllers_Standalone(StateMachineComponent* self) {
    uint8_t* base = reinterpret_cast<uint8_t*>(self);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(base + 0x10);
    void* owner = *reinterpret_cast<void**>(base + 0x10);
    if (!owner) return;
    int cnt = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(owner) + 0x38);
    if (cnt <= 0) return;

    uint8_t* controllerNode = base + 0x28;  // SM root node (controllerSelf target)
    // A pending state already written into +0x52 WINS over the controller default.
    // Retail links controllers at component-creation time, when +0x52 is still the
    // factory zero, so the default-state write can never destroy a caller's intent.
    // Standalone links LAZILY (first Update), which is AFTER StartTown_SpawnStructures
    // stamps its pre-built structures with +0x52 = 2. Overwriting that stamp put every
    // founding house back into BuildComponent's state 1 (under construction) — a state
    // the residence table has no entry for — so ResidenceComponent's state-2 Enter
    // (helper_9de0, 0x140129de0) never ran and townMgr+0x20 stayed null forever. That
    // one null is what made the move-out finder (CitizenComponent_Update_sub_0e70) and
    // everything downstream of it dead. Treat a non-sentinel +0x52 as "already decided".
    //
    // TWO sentinel values must NOT count as intent (task-F audit, 2026-07-26):
    //   0x00 — the ctor/BodyInit placeholder retail expects the linker to overwrite.
    //   0xff — StateMachine::Tick's "no pending state". Two registered paths
    //   (ConsumeProduceAction_helper_b310's priming Tick and StateMachine_impl_dfa0's
    //   inline copy) can run Tick on a NOT-YET-LINKED chain, which writes 0xffff into
    //   +0x52/+0x53; honouring that 0xff here would suppress the controller default
    //   FOREVER (permanent dormancy), where pre-fix the next link pass recovered it.
    //   A 0xff at link time can only have come from such an empty-chain Tick — retail
    //   never ticks an unlinked SM — so ignoring it is always correct.
    uint8_t pending52 = *(base + 0x52);
    bool gotDefault = (pending52 != 0x00 && pending52 != 0xff);
    bool sawEarlierController = false;      // any ControllerComponent-derived comp seen
    uint32_t tidDestroy = ::Object::GetTypeIdByName("DestroyComponent");
    int linked = 0;

    // Decision trace (BANISHED_SM_TRACE, default off => byte-unchanged). Dumps the
    // owner's components IN ARRAY ORDER with the three facts the default-state rule
    // turns on — controller-derived?, live?, GetDefaultState — plus what retail's
    // first-in-order rule would have taken versus what we took. Bounded to the first
    // kSMTraceEntities entities so a full town does not flood the log.
    static const int kSMTraceEntities = 24;
    static int s_decideTraceState = -1;
    static int s_decideTraceCount = 0;
    static void* s_decideSeen[kSMTraceEntities] = {};
    if (s_decideTraceState < 0) {
        const char* e = std::getenv("BANISHED_SM_TRACE");
        s_decideTraceState = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
    }
    // Once per SM, not once per call: an SM that links nothing keeps firstChild
    // null and re-enters this linker every single tick, which would otherwise
    // burn the whole budget on the first entity.
    bool decideTrace = false;
    if (s_decideTraceState == 1 && s_decideTraceCount < kSMTraceEntities) {
        decideTrace = true;
        for (int s = 0; s < s_decideTraceCount; ++s)
            if (s_decideSeen[s] == (void*)self) { decideTrace = false; break; }
    }
    int retailFirstDefault = -1;    // GetDefaultState of the FIRST controller in order
    int retailFirstIdx = -1;
    if (decideTrace) {
        s_decideSeen[s_decideTraceCount++] = (void*)self;
        fprintf(stderr, "[SMTrace] LINK-DECIDE sm=%p owner=%p comps=%d pending(+0x52)=0x%02x%s\n",
                (void*)self, owner, cnt, pending52,
                gotDefault ? " (already decided)" : "");
        fflush(stderr);
    }
    for (int i = 0; i < cnt; ++i) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(base + 0x10);
        owner = *reinterpret_cast<void**>(base + 0x10);
        if (!owner) break;
        uintptr_t arr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uint8_t*>(owner) + 0x30);
        void** ibp = reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(ibp);
        void* comp = *ibp;
        if (!comp) continue;

        uintptr_t vt = *reinterpret_cast<uintptr_t*>(comp);
        typedef uint32_t (*GetTypeId_t)(void*);
        auto gt = *reinterpret_cast<GetTypeId_t*>(vt + 0x10);
        uint32_t tid = gt ? gt(comp) : 0;
        bool isController =
            ::Object::IsTypeDerivedFromByName(tid, "ControllerComponent");

        typedef void* (*CreateState_t)(void*);
        auto cs = *reinterpret_cast<CreateState_t*>(vt + 0x88);
        void* node = cs ? cs(comp) : nullptr;
        bool isLive = (node != nullptr && node != comp);

        if (decideTrace) {
            // GetDefaultState on an INERT component is safe: MakeInertTyped fills
            // every slot but 0/0x10 with Inert_Stub, which returns null (=> 0). That
            // zero is the whole point of this line — it shows that applying retail's
            // first-in-order rule verbatim would read 0 off an inert controller, not
            // the real default that component's exe code returns.
            typedef int (*GetDefaultState_t)(void*);
            auto gdT = *reinterpret_cast<GetDefaultState_t*>(vt + 0x80);
            int dT = gdT ? gdT(comp) : 0;
            if (isController && retailFirstIdx < 0) {
                retailFirstIdx = i;
                retailFirstDefault = dT;
            }
            fprintf(stderr, "[SMTrace]   [%d] %-26s tid=0x%08x ctrl=%d live=%d default=%d\n",
                    i, SMTrace_ComponentName(tid), tid, isController ? 1 : 0,
                    isLive ? 1 : 0, dT);
            fflush(stderr);
        }

        if (!gotDefault && isLive) {
            // Retail-order constraint (destroy only): skip a live destroy that
            // is not the entity's first controller; its 4 must never become the
            // spawn default of an ordinary building.
            bool destroyOutOfOrder =
                (tidDestroy != 0 && tid == tidDestroy && sawEarlierController);
            if (!destroyOutOfOrder) {
                typedef int (*GetDefaultState_t)(void*);
                auto gd = *reinterpret_cast<GetDefaultState_t*>(vt + 0x80);
                int d = gd ? gd(comp) : 0;
                // A ZERO default is indistinguishable from the BodyInit value already
                // sitting in +0x52 — writing it changes nothing, and CLAIMING it would
                // only block a later live controller's real default. That bites the
                // moment WorkPlaceComponent goes live (its GetDefaultState 0x140003730
                // returns 0): if the WPC precedes BuildComponent in the entity's array,
                // it must not rob a fresh foundation of Build's 1 (under construction).
                // So a zero default neither writes nor claims; keep looking.
                //
                // ...UNLESS the live controller genuinely OWNS state 0 (2026-07-27).
                // ClearAreaComponent's default really is 0 and 0 really is its start
                // state (helper_8010 registers 0 -> {1,3} -> 2), so blanket-skipping
                // zero robbed every placed building of its clear-the-footprint pass:
                // measured on a hand-placed house over trees — ClearArea live at [14]
                // with default 0, we took BuildComponent's 1 from [16] instead, State0
                // never ran, no clear-work was ever posted and the trees stayed up.
                // Ask the node whether it can actually HANDLE state 0 (Step(0) >= 0):
                // that separates a real 0 from Inert_Stub's 0, and from a live
                // controller whose 0 just means "I have no opinion" (WorkPlaceComponent
                // returns 0 and registers no state 0, so it still keeps looking and
                // still cannot rob a fresh foundation of Build's 1).
                bool ownsZero = false;
                if (d == 0) {
                    typedef int (*Step_t)(void*, int);
                    uintptr_t nvt = *reinterpret_cast<uintptr_t*>(node);
                    auto stepFn = *reinterpret_cast<Step_t*>(nvt + 0x00);
                    ownsZero = stepFn && stepFn(node, 0) >= 0;
                }
                if (d != 0 || ownsZero) {
                    *(base + 0x52) = static_cast<uint8_t>(d);
                    gotDefault = true;
                }
            }
        }
        if (isController && !isLive) sawEarlierController = true;
        if (isLive) sawEarlierController = true;

        if (!isLive) continue;  // inert / not a controller: nothing to link

        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(node) + 0x08) = controllerNode;  // node parent
        *reinterpret_cast<void**>(controllerNode + 0x10) = node;                              // prev child
        controllerNode = reinterpret_cast<uint8_t*>(node);
        ++linked;
    }

    if (decideTrace) {
        fprintf(stderr, "[SMTrace] LINK-RESULT sm=%p linked=%d ours(+0x52)=%u "
                        "retailFirstCtrl=[%d]->default=%d\n",
                (void*)self, linked, *(base + 0x52), retailFirstIdx, retailFirstDefault);
        fflush(stderr);
    }

    if (linked > 0) {
        static int s_traceState = -1;
        if (s_traceState < 0) {
            const char* e = std::getenv("BANISHED_SM_TRACE");
            s_traceState = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
        }
        if (s_traceState == 1) {
            fprintf(stderr, "[SMTrace] LINKED %d controller state node(s) into SM %p "
                            "(firstChild=%p defaultState=%u)\n",
                    linked, (void*)self, *reinterpret_cast<void**>(base + 0x38),
                    *(base + 0x52));
            fflush(stderr);
        }
    }
}

// Recipe step 1 (Update), routed to slot +0x48 (the per-tick slot the EntityList
// bucket round-robin calls). Signature matches COMPONENT_LIVE_FACTORY's
// UpdateVSlot adapter: (self, int64_t* time). The Time pointer is forwarded
// opaquely to Tick (which never derefs it — it only passes it on to the
// Transition virtuals), matching the original StateMachineComponent::Update.
static void StateMachineComponent_UpdateAdapter(StateMachineComponent* self,
                                                int64_t* time) {
    if (!self) return;
    uint8_t* base = reinterpret_cast<uint8_t*>(self);

    // Resolve s_fn_Tick (StateMachine::Tick @0x1400a9660 -> our reimpl). Do it here
    // (idempotent). Without it s_fn_Tick stays null and Update is a silent no-op.
    StateMachineComponent_ResolveHelpers();

    // Belt-and-suspenders body-init if StartupComponents did not dispatch +0x20.
    if (*reinterpret_cast<void**>(base + 0x48) == nullptr)
        StateMachineComponent_BodyInit(self);

    // Lazy controller-link: until a state node is linked, try to link live
    // ControllerComponents on this entity (runs after all components are
    // constructed; idempotent once linked since firstChild then != 0).
    if (*reinterpret_cast<void**>(base + 0x38) == nullptr)
        StateMachineComponent_LinkControllers_Standalone(self);

    void* firstChild = *reinterpret_cast<void**>(base + 0x38);

    // Bring-up trace (env BANISHED_SM_TRACE, default off => byte-unchanged):
    // confirms the +0x48 slot is entered live and reports whether a state node
    // is linked yet (firstChild). Follows ONE component across ticks.
    {
        static int s_traceState = -1;
        static int s_traceCount = 0;
        static void* s_lockComp = nullptr;
        if (s_traceState < 0) {
            const char* e = std::getenv("BANISHED_SM_TRACE");
            s_traceState = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
        }
        if (s_traceState == 1 && s_lockComp == nullptr) {
            s_lockComp = self;
            // One-time owner-entity component dump: identify which components
            // sit alongside this StateMachineComponent (esp. ControllerComponents
            // the SM links via CreateState). Match each component's runtime
            // type-id against candidate names via Object::GetTypeIdByName.
            static const char* kCandidates[] = {
                "StateMachineComponent", "StorageLocationComponent",
                "WorkPlaceComponent", "ResidenceComponent", "AttendanceComponent",
                "HappinessComponent", "HealerComponent", "CemeteryComponent",
                "ImmigrationComponent", "MarketLocationComponent", "NomadComponent",
                "OrchardComponent", "TradeComponent", "WellComponent",
                "AbsorbResourceComponent", "BuildComponent", "ClearAreaComponent",
                "ClearResourceComponent", "CropFieldComponent", "DestroyComponent",
                "MultiObjectComponent", "PastureComponent", "ResourceProduceComponent",
                "CitizenComponent", "CitizenAIComponent", "CharacterComponent",
                "StorageDisplayComponent", "RawMaterialComponent", "ModelComponent",
                "GrowthComponent", "FlammableComponent",
            };
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(base + 0x10);
            void* owner = *reinterpret_cast<void**>(base + 0x10);
            if (owner) {
                int cnt = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(owner) + 0x38);
                uintptr_t arr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uint8_t*>(owner) + 0x30);
                fprintf(stderr, "[SMTrace] === owner=%p compCount=%d ===\n", owner, cnt);
                for (int ci = 0; ci < cnt && ci < 32; ++ci) {
                    void** ibp = reinterpret_cast<void**>(arr + (uintptr_t)ci * 8);
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(ibp);
                    void* comp = *ibp;
                    uint32_t tid = 0;
                    if (comp) {
                        uintptr_t vt = *reinterpret_cast<uintptr_t*>(comp);
                        typedef uint32_t (*getType_t)(void*);
                        auto gt = *reinterpret_cast<getType_t*>(vt + 0x10);
                        if (gt) tid = gt(comp);
                    }
                    const char* nm = "?";
                    for (const char* cand : kCandidates) {
                        if (tid != 0 && ::Object::GetTypeIdByName(cand) == tid) { nm = cand; break; }
                    }
                    fprintf(stderr, "[SMTrace]   [%d] comp=%p tid=0x%08x %s\n", ci, comp, tid, nm);
                }
                fflush(stderr);
            }
        }
        if (s_traceState == 1 && self == s_lockComp && s_traceCount < 20) {
            fprintf(stderr,
                "[SMTrace] comp=%p ctrlSelf=%p firstChild=%p stateFlags=%08x "
                "%s\n",
                (void*)self, *reinterpret_cast<void**>(base + 0x48),
                firstChild, *reinterpret_cast<uint32_t*>(base + 0x50),
                firstChild ? "TICK" : "noop(no state linked)");
            fflush(stderr);
            s_traceCount++;
        }
    }

    // GUARD: no child state node linked yet -> nothing to tick. Skipping is
    // correct (the engine's SM does nothing until a ControllerComponent links a
    // state) and avoids the bare-root Step(-1) parent-walk null deref.
    if (firstChild == nullptr) return;

    // Forward the Time pointer opaquely as the int64 param (Tick passes it
    // straight through to the Transition virtuals; nothing in this path derefs
    // it as a value).
    StateMachineComponent_Update(self, reinterpret_cast<int64_t>(time));
}

// ------------------------------------------------------------------
// Slot 4 (+0x20) — retail StateMachineComponent::Startup (0x14012b6a0). Retail's
// slot-4 handler is EXACTLY the controller-linker; the field init lives in the ctor
// (0x14012ba30), which the inert factory does not run, so we do both here.
//
// Linking at Startup instead of lazily on the first Update is what makes the
// insta-build spawn path reachable: ComponentEntity_StartupComponents runs this pass
// after every component of the entity exists but BEFORE the spawner's
// SetModel/RegisterEntity bracket, so the chain is already linked when OnAddedPass
// dispatches slot 6 below. With lazy linking the first tick is a frame later, by which
// time StartTown has cleared the instant-build bit and the site takes the staged path.
//
// Gated (BANISHED_LIVE_SM_ONADDED) together with the slot-6 tick: on its own, earlier
// linking only moves the +0x52 default-state write earlier, but that write is what the
// 2026-07-20 stockpile regression turned on, so it does not go live independently.
static void StateMachineComponent_StartupSlot(StateMachineComponent* self) {
    if (!self) return;
    StateMachineComponent_BodyInit(self);
    if (::ComponentFactory::LiveEnabled("BANISHED_LIVE_SM_ONADDED") &&
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(self) + 0x38) == nullptr) {
        StateMachineComponent_ResolveHelpers();
        StateMachineComponent_LinkControllers_Standalone(self);
    }
}

// ------------------------------------------------------------------
// Slot 6 (+0x30) — retail 0x14012b660: StateMachine::Tick(sm, Time(0.0f), 0, 0).
// Verified slot: its only vtable xref is 0x140164550 == vtable(0x140164520) + 0x30.
// (Ghidra names it StateMachineComponent_Draw — the familiar slot-6 mislabel.)
//
// This is the one-shot register-time tick, and it is the ENGINE of retail's
// insta-build: Tick's outer loop runs while a pending state is set, so a state's enter
// handler that requests a transition is serviced inside the SAME call. During
// StartTown's instant-build bracket that means 1 (State1Enter -> req 8) -> 8
// (State8Enter INSTANT: meshes to final frame, delivered = target -> req 2) -> 2, all
// before RegisterEntity returns and the bit is cleared.
static void StateMachineComponent_OnAddedSlot(StateMachineComponent* self) {
    if (!self) return;
    uint8_t* base = reinterpret_cast<uint8_t*>(self);
    StateMachineComponent_ResolveHelpers();
    if (*reinterpret_cast<void**>(base + 0x48) == nullptr)
        StateMachineComponent_BodyInit(self);
    if (*reinterpret_cast<void**>(base + 0x38) == nullptr)
        StateMachineComponent_LinkControllers_Standalone(self);
    // Same guard as the Update adapter: an unlinked chain would drive Tick's
    // parent-walk through the bare root's Step(-1).
    if (*reinterpret_cast<void**>(base + 0x38) == nullptr) return;
    StateMachineComponent_Draw(self);   // Tick(sm, Time(0.0f), 0, 0)
}

COMPONENT_LIVE_FACTORY_SOA(StateMachineComponent, 0x58, 0xcd,
                           "BANISHED_LIVE_STATEMACHINE",
                           StateMachineComponent_UpdateAdapter,
                           StateMachineComponent_StartupSlot,
                           "BANISHED_LIVE_SM_ONADDED",
                           StateMachineComponent_OnAddedSlot)
REGISTER_TYPE(StateMachineComponent, "Component",
              &StateMachineComponent_Factory, &StateMachineComponent_CreateInPlace);
