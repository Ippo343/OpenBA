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
#include "Component/ControllerComponent.h"
#include "Component/DestroyComponent.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (state-2 workplace hand-off)
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <new>

// ============================================================
// Locally resolved function pointers
// ============================================================

// StateMachine::Tick (0x1400a9660) is StateMachine_TickReimpl below — the address
// is registered to it (reg_component_economy.cpp), so the members call it
// directly instead of through a lazily resolved pointer (B38 D7 / B6-5).
void StateMachine_TickReimpl(void* smP, int64_t time, char param3, char param4);

// ControllerComponent type ID at 0x1401c73c0 (filled at runtime by RegisterType).
// Read only by the two reference ports below that no slot installs
// (StateMachineComponent_Serialize, StateMachineComponent_OnUpdate2).
static uint32_t* s_TypeId_ControllerComponent = nullptr;
static bool s_resolved = false;

// ============================================================
// Resolve helpers — for the two uninstalled reference ports only.
// ============================================================
void StateMachineComponent_ResolveHelpers() {
    if (s_resolved) return;
    s_TypeId_ControllerComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c73c0));
    s_resolved = true;
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
    StateMachineComponent_ResolveHelpers();
    uint8_t* base = reinterpret_cast<uint8_t*>(self);
    // Controller sub-object pointer (linked list tail tracker)
    uint8_t* controllerNode = base + 0x28; // starts at controller sub-object
    bool gotDefaultState = false;

    // Check owner InternalBasePtr

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

        ownerEntity = *reinterpret_cast<void**>(base + 0x10);
        arrayBase = *reinterpret_cast<uintptr_t*>(
            reinterpret_cast<uint8_t*>(ownerEntity) + 0x30);

        // Read component pointer from array
        void* tempIBP = nullptr; // local InternalBasePtr for component
        void* comp = *reinterpret_cast<void**>(arrayBase + i * 8);

        // CheckPtr on the component IBP
        tempIBP = comp;
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
            new (&ibpValue) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(candidate));   // B.3.3 bridge

        if (ibpValue != nullptr) {
            if (!gotDefaultState) {
                // First ControllerComponent: call virtual +0x80 (GetDefaultState)
                uint8_t defaultState = static_cast<uint8_t>(
                    reinterpret_cast<ControllerComponent*>(ibpValue)->GetDefaultState());   // B.3.8 bridge (slot 16)
                gotDefaultState = true;
                // Store default state at +0x52
                *(base + 0x52) = defaultState;
            }

            // Call virtual +0x88 (CreateState) to get a state node
            void* stateNode = reinterpret_cast<ControllerComponent*>(ibpValue)->GetController();   // B.3.8 bridge (slot 17)

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
        ownerEntity = *reinterpret_cast<void**>(base + 0x10);
        componentCount = *reinterpret_cast<int*>(
            reinterpret_cast<uint8_t*>(ownerEntity) + 0x38);
    }
}

// ============================================================
// Draw (vtable slot [6], 0x14012b660)
// ============================================================
void StateMachineComponent_Draw(StateMachineComponent* self) {

    // Construct Time(0.0) on stack
    int64_t zeroTime = 0;
    if (fn_Time_Ctor_Float) {
        fn_Time_Ctor_Float(&zeroTime, 0.0f);
    }

    // StateMachine_Tick(&controllerSelf, zeroTime, false, false)
    StateMachine_TickReimpl(&self->controllerSelf, zeroTime, 0, 0);
}

// ============================================================
// OnRemove (vtable slot [8], 0x14012b640)
// ============================================================
void StateMachineComponent_OnRemove(StateMachineComponent* self, int64_t deltaTime) {

    // StateMachine_Tick(&controllerSelf, deltaTime, false, true)
    StateMachine_TickReimpl(&self->controllerSelf, deltaTime, 0, 1);
}

// ============================================================
// Update (vtable slot [9], 0x14012b620)
// ============================================================
void StateMachineComponent_Update(StateMachineComponent* self, int64_t deltaTime) {

    // StateMachine_Tick(&controllerSelf, deltaTime, true, true)
    StateMachine_TickReimpl(&self->controllerSelf, deltaTime, 1, 1);
}

// ============================================================
// OnUpdate2 (vtable slot [11], 0x14012b7d0)
// Same ControllerComponent iteration as Serialize, then walks
// the state machine linked list to dispatch a virtual call.
// ============================================================
void StateMachineComponent_OnUpdate2(StateMachineComponent* self) {
    StateMachineComponent_ResolveHelpers();
    uint8_t* base = reinterpret_cast<uint8_t*>(self);
    uint8_t* controllerNode = base + 0x28;

    // Check owner InternalBasePtr

    void* ownerEntity = *reinterpret_cast<void**>(base + 0x10);
    if (!ownerEntity) goto state_walk;

    {
        int componentCount = *reinterpret_cast<int*>(
            reinterpret_cast<uint8_t*>(ownerEntity) + 0x38);
        if (componentCount <= 0) goto state_walk;

        for (int i = 0; i < componentCount; i++) {

            ownerEntity = *reinterpret_cast<void**>(base + 0x10);
            uintptr_t arrayBase = *reinterpret_cast<uintptr_t*>(
                reinterpret_cast<uint8_t*>(ownerEntity) + 0x30);

            void* tempIBP = *reinterpret_cast<void**>(arrayBase + i * 8);

            void* comp = tempIBP;

            void* candidate = nullptr;
            if (fn_IsDerivedFrom && s_TypeId_ControllerComponent) {
                if (fn_IsDerivedFrom(comp, *s_TypeId_ControllerComponent))
                    candidate = comp;
            }

            void* ibpValue = nullptr;
            if (fn_InternalBasePtr_Ctor)
                new (&ibpValue) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>(candidate));   // B.3.3 bridge

            if (ibpValue != nullptr) {
                // Call virtual +0x88 (CreateState) to get a state node
                void* stateNode = reinterpret_cast<ControllerComponent*>(ibpValue)->GetController();   // B.3.8 bridge (slot 17)

                // Link into chain
                *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(stateNode) + 0x08) =
                    reinterpret_cast<void*>(controllerNode);
                *reinterpret_cast<void**>(controllerNode + 0x10) =
                    stateNode;
                controllerNode = reinterpret_cast<uint8_t*>(stateNode);
            }

            // Re-check count
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

// ============================================================================
// ENDGAME B.3.8 VIS — StateMachineComponent as a REAL class
// (StateMachineComponent.h).
//
// Before: a 48-slot StateMachineComponent_LiveVtable filled per create (slot 8
// patched in on the first SM call of the process), Inert_ScalarDtor in slot 0,
// the root node's vptr a 16-entry all-zero stub table stamped by BodyInit.
// After: the compiler's 16-slot vtable, a real ctor/dtor chain (B38_RULES D1,
// D2) and the root node as a real member (D6) whose 9 slots answer 0 like the
// stub did. The LAZY body-init shape below is kept (the ctor leaves +0x48 null
// and the state quad 0 exactly as the old zero-filling factory did; see the ctor).
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/StateMachineComponent.h"
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

// Recipe step 2 (body-init), routed to slot +0x20 (StartupComponents dispatch).
// Faithful field init from the ctor 0x14012ba30 (idempotent). Component base +
// flags(0xcd) are already set by the inert factory; this adds the SM-specific
// controller sub-object + back-pointers + state word.
static void StateMachineComponent_BodyInit(StateMachineComponent* self) {
    if (!self) return;
    uint8_t* base = reinterpret_cast<uint8_t*>(self);

    // +0x28 root node: its vptr is the member's compiler table, stamped by the
    // ctor (the old stamp of a stub table here is gone); +0x30/+0x38 zeroed
    // children, +0x40 ownerComponent = self, +0x48 controllerSelf = self+0x28,
    // +0x50 stateFlags = 0xFF00FFFF.
    *reinterpret_cast<uint64_t*>(base + 0x30) = 0;
    *reinterpret_cast<uint64_t*>(base + 0x38) = 0;
    *reinterpret_cast<void**>(base + 0x40) = self;
    *reinterpret_cast<void**>(base + 0x48) = base + 0x28;

    // The four state bytes at +0x50 (currentInput / activeState / pendingState /
    // transitionState) are stamped ONLY for a genuinely fresh component.
    //
    // BodyInit does two jobs: rebuild the runtime sub-object pointers (+0x28..+0x48),
    // and seed the state. Retail's ctor (0x14012ba30) can do both because it only ever
    // runs at construction. Ours is called lazily, guarded on `+0x48 == nullptr` —
    // "have the runtime pointers been built yet?" — and a STREAM-RESTORED component
    // satisfies that guard too: slot 3 (Shutdown/Serialize) round-trips the four state
    // bytes, but the sub-object pointers are runtime-only and are NOT serialized. So on
    // a loaded world BodyInit rebuilt the pointers (correct) and then threw the restored
    // state away (wrong), handing the machine back to the linker as if it were new.
    //
    // Measured on the tutorial save, the starting stockpile:
    //   [SMTrace] BodyInit CLOBBERS state +0x50..53 was ffff0002 (active=0 pending=0xff)
    //             ret=StateMachineComponent_TreeTickVSlot
    // then the linker saw the re-stamped pending=0x00, wrote a controller default, and
    // the next Tick fired the state ENTER a second time — re-running every state-2
    // registrar (storage, cemetery, healer, happiness, attendance, workplace) for an
    // entity that was already registered by the save stream.
    //
    // The factory zero-fill (MakeInertTyped) leaves all four bytes 0, and no SAVED
    // machine can look like that: a settled machine always stores pendingState 0xff
    // ("nothing pending"), never 0x00, which is the ctor placeholder the linker is
    // supposed to overwrite. So an all-zero quad means "fresh", and anything else means
    // the bytes already carry meaning — either the stream's, or a previous BodyInit's.
    const uint32_t stateBefore = *reinterpret_cast<uint32_t*>(base + 0x50);
    if (stateBefore == 0x00000000u) {
        *reinterpret_cast<uint32_t*>(base + 0x50) = 0xff00ffff;
    }
}


static void StateMachineComponent_LinkControllers_Standalone(StateMachineComponent* self) {
    uint8_t* base = reinterpret_cast<uint8_t*>(self);
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

    // THIRD case (2026-09-04): a STREAM-RESTORED state machine is already in its
    // state and must not be given a default at all.
    //
    // BodyInit stamps +0x50..+0x53 = 0xff00ffff, so a NEVER-RUN machine has
    // activeState(+0x51) == 0xff. Shutdown/Serialize round-trips all four bytes, so a
    // component restored from a save comes back with a REAL activeState there and
    // pendingState(+0x52) == 0xff ("nothing pending") because it had already settled
    // when the save was written.
    //
    // 0xff is exactly the value the rule above does NOT count as intent, so the linker
    // wrote the controller's default into +0x52 — and the next StateMachine::Tick read
    // that as a pending transition and fired the state ENTER handler a second time, on
    // a machine that was already in that state.
    //
    // Retail cannot hit this: its linker runs at component CREATION, when the machine
    // has never had a state. Ours links LAZILY on the first Update, which for a loaded
    // world is after the stream has restored the state — so only standalone re-enters.
    //
    // Measured on the tutorial save: the one starting stockpile came back registered
    // TWICE in the town storage list (townMgr+0x00), because StorageLocationComponent's
    // state-2 enter (TownRegistry_RegisterStorageLocation, retail 0x14012d230, which has
    // no dedup because retail never double-calls it) ran again on load:
    //     [TownCensus] LOAD exit : entA=1          <- restored by the stream
    //     [StoreLoc] APPEND entity=...c9d0 count=2 ret=StorageLoc_State2Enter
    //     [StoreLoc] [0] entity=...c9d0   [1] entity=...c9d0
    // That made TutorialGoalCount's StorageLocation read 4 where retail reads 3, and
    // since goalstoragebarnfinished sets no `_greaterEqual` the compare is EQUALITY —
    // so stepCutter7 could never complete.
    //
    // The blast radius is every state-2 registrar, not just storage: cemetery, healer,
    // happiness, attendance and workplace all register from a state enter, so any loaded
    // world was double-registering each of them.
    //
    // Deliberately NOT keyed on +0x52 alone: the existing 0xff rule above must stay, for
    // the empty-chain Tick paths it documents (those write 0xffff into +0x52/+0x53 and
    // leave +0x51 alone, so they cannot trip this new test).
    const uint8_t active51 = *(base + 0x51);
    const bool streamRestored = (active51 != 0xff);
    if (streamRestored && !gotDefault) {
        gotDefault = true;
    }
    bool sawEarlierController = false;      // any ControllerComponent-derived comp seen
    uint32_t tidDestroy = DestroyComponent::TypeId();
    int linked = 0;

    for (int i = 0; i < cnt; ++i) {
        owner = *reinterpret_cast<void**>(base + 0x10);
        if (!owner) break;
        uintptr_t arr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uint8_t*>(owner) + 0x30);
        void** ibp = reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
        void* comp = *ibp;
        if (!comp) continue;

        uint32_t tid = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge (slot 2)
        bool isController =
            ::Object::IsTypeDerivedFromByName(tid, "ControllerComponent");
        // Slots 16/17 exist only on ControllerComponent's table (18 slots); a plain
        // Component has 16 (B38_RULES §2.1). Retail's linker (the 0x14012b6a0 port above)
        // asks IsDerivedFrom first; this lazy copy called
        // +0x88 on EVERY sibling, which only worked while every component carried the
        // 48-slot inert table. A non-controller was never live here (its inert slot 17
        // returned null), so skipping it is the same behaviour.
        if (!isController) continue;

        void* node = reinterpret_cast<ControllerComponent*>(comp)->GetController();   // B.3.8 bridge (slot 17)
        bool isLive = (node != nullptr && node != comp);


        if (!gotDefault && isLive) {
            // Retail-order constraint (destroy only): skip a live destroy that
            // is not the entity's first controller; its 4 must never become the
            // spawn default of an ordinary building.
            bool destroyOutOfOrder =
                (tidDestroy != 0 && tid == tidDestroy && sawEarlierController);
            if (!destroyOutOfOrder) {
                int d = reinterpret_cast<ControllerComponent*>(comp)->GetDefaultState();   // B.3.8 bridge (slot 16)
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
                    ownsZero = reinterpret_cast<ControllerComponent::Controller*>(node)->FindState(0) >= 0;   // B.3.8 bridge (slot 0)
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


}




// ============================================================================
// The members.
// ============================================================================

// 0x14012ba30: Component() is the CALL 0x1401188b0; the 0x140164520 stamp and
// the member's 0x14015f070 / 0x1401644d0 stamps are the compiler's; the member
// gets +0x30 = +0x38 = 0 and +0x40 = this (retail); flags |= 0xcd.
// PORT GAP (retail 0x14012ba86 / 0x14012ba8a) — B38 D1: retail also writes
// +0x48 = this+0x28 and +0x50 = 0xff00ffff here. The tree's factory zero-filled
// both, and BodyInit (slot 4, or lazily from the first tick) writes them — with
// the stream-restore rule that an all-zero state quad means "fresh". That lazy
// shape is kept unchanged; moving the two stores into the ctor is its own
// titled commit.
StateMachineComponent::StateMachineComponent()
    : _controller(this), controllerSelf(nullptr), stateFlags(0), _pad54{} {
    OrFlagsInCtor(0xcd);
}

// ============================================================
// Shutdown (vtable slot [3], 0x14012b940)
// ============================================================
void StateMachineComponent::Persist(void* stream) {
    // Component::Persist first (0x1401189b0, a direct call in retail too). It
    // used to go through a lazily resolved pointer whose null state desynced the
    // stream on the load path (save/load wave 2026-08-28); the qualified member
    // call has no such state.
    Component::Persist(stream);

    // Create IO::Serializer with version 0
    alignas(8) uint8_t serializer[24]; // IO::Serializer is 24 bytes (stack local)
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize 4 individual bytes from stateFlags at +0x50, +0x51, +0x52, +0x53
    uint8_t* base = reinterpret_cast<uint8_t*>(this);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<unsigned char*>(base + 0x50), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<unsigned char*>(base + 0x51), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<unsigned char*>(base + 0x52), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<unsigned char*>(base + 0x53), nullptr);   // B.3.4 bridge

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
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
// Together with the slot-6 tick: on its own, earlier
// linking only moves the +0x52 default-state write earlier, but that write is what the
// 2026-07-20 stockpile regression turned on, so it does not go live independently.
void StateMachineComponent::Startup() {
    StateMachineComponent_BodyInit(this);
    if (
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(this) + 0x38) == nullptr) {
        StateMachineComponent_LinkControllers_Standalone(this);
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
void StateMachineComponent::OnAdded() {
    uint8_t* base = reinterpret_cast<uint8_t*>(this);
    if (*reinterpret_cast<void**>(base + 0x48) == nullptr)
        StateMachineComponent_BodyInit(this);
    if (*reinterpret_cast<void**>(base + 0x38) == nullptr)
        StateMachineComponent_LinkControllers_Standalone(this);
    // Same guard as the Update adapter: an unlinked chain would drive Tick's
    // parent-walk through the bare root's Step(-1).
    if (*reinterpret_cast<void**>(base + 0x38) == nullptr) return;
    StateMachineComponent_Draw(this);   // Tick(sm, Time(0.0f), 0, 0)
}
// ------------------------------------------------------------------
// Slot 8 (+0x40) — retail 0x14012b640: StateMachine::Tick(sm, time, 0, 1).
// THE SECOND PER-FRAME TICK, and until now the missing half of the dispatcher.
//
// Retail drives the state machine from TWO vtable slots every frame, not one:
//   idx 9 = 0x14012b620  Tick(sm, time, useAlt=1, run=1)  <- EntityList bucket
//                                                            round-robin (vtbl[9])
//   idx 8 = 0x14012b640  Tick(sm, time, useAlt=0, run=1)  <- EntityList::TreeNodeHandler
//                                                            (entitylist.cpp, vtbl[8])
// The `useAlt` flag is what selects the node's per-frame column: Tick's run phase calls
// node slot +0x38 when it is 1 and node slot +0x30 when it is 0 (0x1400a97b1 vs
// 0x1400a97e3). We had only ever wired idx 9, so every trans30 handler in the project
// was unreachable — which is exactly why the merchant boat parked: BoatComponent's state
// table (0x1400468a0) registers NO trans38 handler at all, only trans30.
//
// The TreeNodeHandler side of this was already ported and correct; it calls vtbl[8] on
// components whose flag bit 2 is set. Only the SMC end was stubbed.
//
// SAFETY: at the time of writing NO other controller table in code/Game/Component
// registers a non-null trans30 handler (checked across every TableRegisterState call
// site), so for every existing controller this slot dispatches into the table, finds a
// null handler and returns 0 — the same graceful no-op retail performs for an
// unregistered slot. It is gated anyway, on the boat controller's own flag, so an A/B
// run can isolate it.
void StateMachineComponent::Tick(void* t) {
    uint8_t* base = reinterpret_cast<uint8_t*>(this);
    if (*reinterpret_cast<void**>(base + 0x48) == nullptr)
        StateMachineComponent_BodyInit(this);
    // Same guard as the Update adapter: an unlinked chain would drive Tick's parent-walk
    // through the bare root's Step(-1).
    if (*reinterpret_cast<void**>(base + 0x38) == nullptr) return;
    StateMachineComponent_OnRemove(this, reinterpret_cast<int64_t>(t));   // Tick(...,0,1)
}
// Recipe step 1 (Update), routed to slot +0x48 (the per-tick slot the EntityList
// bucket round-robin calls). Signature matches COMPONENT_LIVE_FACTORY's
// UpdateVSlot adapter: (self, int64_t* time). The Time pointer is forwarded
// opaquely to Tick (which never derefs it — it only passes it on to the
// Transition virtuals), matching the original StateMachineComponent::Update.
void StateMachineComponent::Update(void* timeV) {
    int64_t* time = static_cast<int64_t*>(timeV);
    uint8_t* base = reinterpret_cast<uint8_t*>(this);

    // Belt-and-suspenders body-init if StartupComponents did not dispatch +0x20.
    if (*reinterpret_cast<void**>(base + 0x48) == nullptr)
        StateMachineComponent_BodyInit(this);

    // Lazy controller-link: until a state node is linked, try to link live
    // ControllerComponents on this entity (runs after all components are
    // constructed; idempotent once linked since firstChild then != 0).
    if (*reinterpret_cast<void**>(base + 0x38) == nullptr)
        StateMachineComponent_LinkControllers_Standalone(this);

    void* firstChild = *reinterpret_cast<void**>(base + 0x38);



    if (firstChild == nullptr) return;


    // Forward the Time pointer opaquely as the int64 param (Tick passes it
    // straight through to the Transition virtuals; nothing in this path derefs
    // it as a value).
    StateMachineComponent_Update(this, reinterpret_cast<int64_t>(time));
}
// [11] 0x14012b7d0. PORT GAP (retail 0x14012b7d0) — B38 D9: the body is ported
// above (StateMachineComponent_OnUpdate2) but the tree's table never installed
// it; the inert stub is kept here.
void StateMachineComponent::Slot11() {}

// The root node's 9 slots (member table 0x1401644d0). PORT GAP, see the header:
// every slot answers 0, as the tree's stub table did.
int  StateMachineComponent::RootController::FindState(int) { return 0; }
int  StateMachineComponent::RootController::FindTransition(int, int*) { return 0; }
char StateMachineComponent::RootController::PostEvent(void*, int, int) { return 0; }
char StateMachineComponent::RootController::Enter(void*, int) { return 0; }
char StateMachineComponent::RootController::Update(void*, int) { return 0; }
void StateMachineComponent::RootController::OnTransition(void*, int, int) {}
char StateMachineComponent::RootController::Tick(void*, void*, int) { return 0; }
char StateMachineComponent::RootController::TickAlt(void*, void*, int) { return 0; }
char StateMachineComponent::RootController::HasStates() { return 0; }

// [0] 0x1400384a0: ~StateMachineComponent (0x14012b9e0: restamps + ~Internal; the member has no dtor slot), MemFree on &1.
Object::Base* StateMachineComponent::DeletingDtor(unsigned flags) {
    this->~StateMachineComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x1400382d0: MemAlloc(0x58, 8), then the ctor.
void* StateMachineComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(StateMachineComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) StateMachineComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* StateMachineComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) StateMachineComponent());
}

DEFINE_EXE_VTABLE(StateMachineComponent, "_ZTV21StateMachineComponent")
DEFINE_EXE_TYPE(StateMachineComponent, "Component")
