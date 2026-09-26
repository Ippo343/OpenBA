#pragma once
// code/Game/Component/controller_state_table.h
// Phase 3 / Keystone D step 3 — REAL per-controller state-type table + the real
// generic state-node dispatcher virtuals, for the standalone exe.
//
// Each building ControllerComponent (Build, StorageLocation, …) owns a private
// "state-type table": an array of 0xf8-byte entries describing the states its
// embedded state node can be in. In the original exe this table lives in the
// component's .data (e.g. BuildComponent: array DAT_1401d3ca0, count DAT_1401d4c20)
// and is filled at startup by `<X>Description_Setup -> helper -> sub_f500/sub_f620`.
// The node's *secondary* vtable methods (Step/Enter/Update/OnState/Transition) are
// tiny GENERIC dispatchers that index this table — they are byte-identical across
// controllers except for the hard-coded table base.
//
// In standalone the exe .data table is not mapped, so we allocate our own table
// (one StateTable per controller-type, via the Dispatcher<Tag> template) and a set
// of ported generic dispatchers that read it. This replaces the crash-safe stub
// node vtable (controller_live_base.cpp) with the REAL mechanism: Step performs the
// real state-id->index search, and Enter/Update/OnState/Transition dispatch through
// the table to per-state handlers.
//
// ── Table entry layout (0xf8 bytes, recovered from BuildComponent_Controller_*
//    dispatchers 0x14002f6d0 / 0x140002b60 / 0x140002b90 / 0x140002bc0 /
//    0x140002bf0 / 0x140002c20 and the populators sub_f500/sub_f620): ────────────
//   +0x00 int    id              (the state-id Step matches against)
//   +0x08 void*  enterHandler    (node vtable slot +0x18 dispatcher tail-calls it)
//   +0x10 void*  trans30Handler  (node vtable slot +0x30 dispatcher)
//   +0x18 void*  trans38Handler  (node vtable slot +0x38 dispatcher; = setup fn)
//   +0x20 void*  updateHandler   (node vtable slot +0x20 dispatcher)
//   +0x28 Trans  transitions[8]  (per-state transition list, {targetId, handler})
//   +0xa8 int    transitionCount
//   …pad to 0xf8
//
// ── Node-interface ABI (recovered from StateMachine::Tick 0x1400a9660 disasm; the
//    state index is carried in R8D / R9D — Ghidra DROPS these in the decompile): ─
//   slot 0 (+0x00) Step(node,     int input)              -> int index / -1
//   slot 3 (+0x18) Enter(node, sm, int idx)              -> char
//   slot 4 (+0x20) Update(node, sm, int idx)             -> char
//   slot 5 (+0x28) OnState(node, sm, int actIdx, int trIdx) -> void
//   slot 6 (+0x30) Transition(node, sm, void* time, int idx) -> char
//   slot 7 (+0x38) Transition(node, sm, void* time, int idx) -> char
// Each handler is tail-called as `handler(*(node+0x18))` (the dispatcher does
// `MOV RCX,[RCX+0x18]; JMP handler`); a null handler is a graceful no-op (return 0).
//
// Handlers are null in standalone until the per-state behaviour functions are
// ported (the deep next layer — e.g. BuildDescription_helper_8290 is a large
// GameWorld/ResourceGather routine). With null handlers the SM still runs the REAL
// table-driven structure (Step returns real indices, transitions are validated
// against the real graph), it just performs no per-state work yet.

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Our port of StateMachine::Tick (retail 0x1400a9660), defined in
// statemachinecomponent.cpp and registered for that address. RequestStateTransition
// below needs it for the retail nested-tick idiom on a not-yet-entered SM.
void StateMachine_TickReimpl(void* smP, int64_t time, char param3, char param4);

namespace ControllerLive {

#pragma pack(push, 1)
struct TransEntry {        // 0x10 bytes
    int   targetId;        // +0x00
    int   _pad04;          // +0x04
    void* handler;         // +0x08
};
struct EventEntry {        // 0x10 bytes — same shape as TransEntry
    int   eventId;         // +0x00
    int   _pad04;          // +0x04
    void* handler;         // +0x08
};
struct StateEntry {        // 0xf8 bytes
    int        id;             // +0x00
    int        _pad04;         // +0x04
    void*      enterHandler;   // +0x08
    void*      trans30Handler; // +0x10
    void*      trans38Handler; // +0x18
    void*      updateHandler;  // +0x20
    TransEntry transitions[8]; // +0x28 .. +0xa8
    int        transitionCount;// +0xa8
    uint8_t    _pad_ac[4];     // +0xac
    EventEntry events[4];      // +0xb0 .. +0xf0   (see the PostEvent note below)
    int        eventCount;     // +0xf0
    uint8_t    _tail[0xf8 - 0xf4];
};
#pragma pack(pop)
static_assert(sizeof(TransEntry) == 0x10, "TransEntry must be 0x10");
static_assert(sizeof(EventEntry) == 0x10, "EventEntry must be 0x10");
static_assert(sizeof(StateEntry) == 0xf8, "StateEntry must be 0xf8");
static_assert(offsetof(StateEntry, events) == 0xb0, "events must sit at +0xb0");
static_assert(offsetof(StateEntry, eventCount) == 0xf0, "eventCount must sit at +0xf0");

struct StateTable {
    StateEntry entries[16];
    int        count;
};

// ── Populate API — replicates sub_f500 (register state) / sub_f620 (add a
//    transition to the most-recently-registered state). ──────────────────────────
inline void TableRegisterState(StateTable& t, int id, void* enterH,
                               void* trans30H, void* trans38H, void* updateH) {
    if (t.count >= 16) return;
    StateEntry& e = t.entries[t.count];
    e.id = id;
    e.enterHandler = enterH;
    e.trans30Handler = trans30H;
    e.trans38Handler = trans38H;
    e.updateHandler = updateH;
    e.transitionCount = 0;
    ++t.count;
}
inline void TableAddTransition(StateTable& t, int targetId, void* handler) {
    if (t.count <= 0) return;
    StateEntry& e = t.entries[t.count - 1];
    if (e.transitionCount >= 8) return;
    TransEntry& tr = e.transitions[e.transitionCount];
    tr.targetId = targetId;
    tr.handler = handler;
    ++e.transitionCount;
}
// Replicates retail's AddEvent (0x140009c70): append {eventId, handler} to the
// most-recently-registered state's event list. The panel's Command (component vtable
// idx14) posts node events 3/4/5 through node slot 2, and without this the post finds
// an empty list and the button click dies silently.
inline void TableAddEvent(StateTable& t, int eventId, void* handler) {
    if (t.count <= 0) return;
    StateEntry& e = t.entries[t.count - 1];
    if (e.eventCount >= 4) return;   // retail's capacity too: (0xf0-0xb0)/0x10 == 4
    EventEntry& ev = e.events[e.eventCount];
    ev.eventId = eventId;
    ev.handler = handler;
    ++e.eventCount;
}
// The real Step search: first entry whose id == input, else -1 (orig 0x14002f6d0).
inline int TableFindIndex(const StateTable& t, int input) {
    for (int i = 0; i < t.count; ++i)
        if (t.entries[i].id == input) return i;
    return -1;
}

// ── Per-controller dispatcher + table, generated once per Tag type. Mirrors the
//    exe's per-controller dispatcher functions (each hard-codes its own table). ───
template <class Tag>
struct Dispatcher {
    static StateTable table;
    static void*      vtbl[16];

    // slot 0 (+0x00): Step(node /*ignored*/, input) -> index / -1
    static int Step(void* /*node*/, int input) {
        int idx = TableFindIndex(table, input);
        return idx;
    }
    // node+0x18 holds the controller-data pointer the handler expects.
    static void* NodeData(void* node) {
        return *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(node) + 0x18);
    }
    // slot 3 (+0x18): Enter — dispatch table[idx].enterHandler(node+0x18, sm)
    // The exe enter dispatcher tail-calls `MOV RCX,[node+0x18]; JMP handler`, so a real
    // handler receives RCX=controller-data AND RDX=sm (preserved through the tail-JMP,
    // bug-class #6). Forward sm as the 2nd arg; 1-arg handlers ignore the extra RDX.
    static char Enter(void* node, void* sm, int idx) {
        if (idx < 0 || idx >= table.count) return 0;
        void* h = table.entries[idx].enterHandler;
        if (!h) return 0;
        return reinterpret_cast<char (*)(void*, void*)>(h)(NodeData(node), sm);
    }
    // slot 1 (+0x08): RequestTransition(node, curIdx, int* pDesiredStateId) -> transIdx / -1
    // The state handlers call this to request a transition: it searches the CURRENT
    // state's registered transition list for one whose targetId == *pDesired and returns
    // that transition's index (which StateMachine::Tick later feeds to OnState/slot28 to
    // run the transition action). Mirrors the exe node slot +0x08 dispatcher (Well's
    // 0x1400100c0, read 2026-09-26): on a MISS retail stores -1 to *pDesired
    // (0x140010103 `MOV dword [R10],-1; OR EAX,-1; RET`) — B38_RULES D9 #9. A hit
    // leaves *pDesired alone. The idx/null bounds rejection is the tree's (retail has
    // none); it returns -1 without touching *pDesired, as before.
    static int RequestTransition(void* /*node*/, int curIdx, int* pDesired) {
        if (!pDesired || curIdx < 0 || curIdx >= table.count) return -1;
        const StateEntry& e = table.entries[curIdx];
        int desired = *pDesired;
        for (int i = 0; i < e.transitionCount; ++i)
            if (e.transitions[i].targetId == desired) return i;
        *pDesired = -1;
        return -1;
    }
    // slot 8 (+0x40): retail (Well 0x140010260) `XOR EAX,EAX; CMP [count],EAX;
    // SETNZ AL; RET` — true iff the class's state table has any state (D9 #9).
    static char HasStates(void* /*node*/) { return table.count != 0 ? 1 : 0; }
    // slot 4 (+0x20): Update — dispatch table[idx].updateHandler(node+0x18, sm)
    // Like Enter, the exe update dispatcher tail-calls `MOV RCX,[node+0x18]; JMP handler`
    // preserving RDX=sm (bug-class #6), so real update handlers (e.g. sub_81e0/sub_76e0)
    // receive (controller-data, sm) and use sm to request state transitions.
    static char Update(void* node, void* sm, int idx) {
        if (idx < 0 || idx >= table.count) return 0;
        void* h = table.entries[idx].updateHandler;
        if (!h) return 0;
        return reinterpret_cast<char (*)(void*, void*)>(h)(NodeData(node), sm);
    }
    // slot 5 (+0x28): OnState — table[actIdx].transitions[trIdx].handler(node+0x18)
    static void OnState(void* node, void* /*sm*/, int actIdx, int trIdx) {
        if (actIdx < 0 || actIdx >= table.count) return;
        if (trIdx < 0 || trIdx >= 8) return;
        void* h = table.entries[actIdx].transitions[trIdx].handler;
        if (!h) return;
        reinterpret_cast<void (*)(void*)>(h)(NodeData(node));
    }
    // slot 6 (+0x30): Transition — dispatch table[idx].trans30Handler(node+0x18, sm, time)
    // ⚠ THIS IS ALSO A PER-FRAME HANDLER, and it is the ONLY per-frame column some
    // controllers use. StateMachine::Tick's run phase picks +0x38 when its `useAlt`
    // argument is 1 and +0x30 when it is 0 (0x1400a97b1 vs 0x1400a97e3), and retail
    // drives BOTH every frame through two different StateMachineComponent vtable slots:
    // idx 9 = 0x14012b620 Tick(...,1,1) from the EntityList bucket round-robin, and
    // idx 8 = 0x14012b640 Tick(...,0,1) from EntityList::TreeNodeHandler. BoatComponent's
    // state table (0x1400468a0) registers NO trans38 handler at all — every per-frame
    // handler it owns is in this column.
    //
    // Same tail-JMP ABI as Trans38: the exe dispatcher does `MOV RCX,[RCX+0x18]; JMP h`
    // with RDX=sm and R8=time passing through, so handlers receive (data, sm, time).
    // Forwarding them is required, not cosmetic — 0x140045250 and 0x140044ff0 both read
    // sm to request their state transition. Handlers that take fewer arguments simply
    // ignore the extra registers.
    static char Trans30(void* node, void* sm, void* time, int idx) {
        if (idx < 0 || idx >= table.count) return 0;
        void* h = table.entries[idx].trans30Handler;
        if (!h) return 0;
        return reinterpret_cast<char (*)(void*, void*, void*)>(h)(NodeData(node), sm, time);
    }
    // slot 7 (+0x38): DrawAlt — dispatch table[idx].trans38Handler(node+0x18, sm, time)
    // ⚠ This is the PER-FRAME handler. StateMachineComponent::Update calls
    // StateMachine::Tick(sm, time, useAltDraw=1, doDraw=1) every frame, and Tick's
    // DRAW phase (useAltDraw=1) invokes node slot +0x38 every frame — whereas the
    // UPDATE phase (slot +0x20) only runs when a transition is pending. So the real
    // per-tick state work (BuildDescription sub_76e0/sub_81e0 proportional build +
    // transition-request) lives in the trans38Handler.
    // The exe slot-7 dispatcher (e.g. ResidenceComponent's 0x140037a00) tail-JMPs
    // with RCX=[node+0x18] and RDX/R8 passing through — so handlers receive
    // (data, sm, Time* dt). Handlers that don't need dt (2-arg ports like the
    // BuildComponent state work) simply ignore R8; the ResidenceComponent heat
    // tick (0x140129970) dereferences it for its heat countdown.
    static char Trans38(void* node, void* sm, void* time, int idx) {
        if (idx < 0 || idx >= table.count) return 0;
        void* h = table.entries[idx].trans38Handler;
        if (!h) return 0;
        return reinterpret_cast<char (*)(void*, void*, void*)>(h)(NodeData(node), sm, time);
    }
    // slot 2 (+0x10): PostEvent(node, sm, int curIdx, int eventId) -> char
    // Ported 1:1 from the retail dispatcher at 0x140009da0 (orchard node vtable
    // 0x14015a130 index 2), read as disassembly rather than trusting the vtable's
    // Ghidra names, which are wrong by position for this whole family:
    //     MOVSXD RAX,R8D ; LEA R8,[table] ; IMUL RAX,RAX,0xf8 ; ADD RAX,R8
    //     MOVSXD R10,[RAX+0xf0]        <- eventCount, INLINE at +0xf0
    //     ADD RAX,0xb0                 <- event array base
    //     CMP [RAX],R9D / ADD RAX,0x10 <- linear scan, stride 0x10, id at +0x00
    //     hit:  MOV RCX,[RCX+0x18] ; JMP [RAX+0x8]   <- tail-call, RDX=sm survives
    //     miss: XOR AL,AL ; RET
    // So a handler is called as handler(NodeData(node), sm) and returns char — the
    // same shape as Enter/Update. A miss is a graceful 0, exactly as retail.
    static char PostEvent(void* node, void* sm, int curIdx, int eventId) {
        if (curIdx < 0 || curIdx >= table.count) return 0;
        const StateEntry& e = table.entries[curIdx];
        for (int i = 0; i < e.eventCount; ++i) {
            if (e.events[i].eventId != eventId) continue;
            void* h = e.events[i].handler;
            if (!h) return 0;
            return reinterpret_cast<char (*)(void*, void*)>(h)(NodeData(node), sm);
        }
        return 0;
    }
    // Any other slot (dtor/GetTypeId/Serialize/…) — safe no-op.
    static void* Stub(void*, ...) { return nullptr; }

    static void** Vtable() {
        if (vtbl[0] == nullptr) {
            for (int i = 0; i < 16; ++i) vtbl[i] = reinterpret_cast<void*>(&Stub);
            vtbl[0] = reinterpret_cast<void*>(&Step);
            vtbl[1] = reinterpret_cast<void*>(&RequestTransition);
            vtbl[2] = reinterpret_cast<void*>(&PostEvent);
            vtbl[3] = reinterpret_cast<void*>(&Enter);
            vtbl[4] = reinterpret_cast<void*>(&Update);
            vtbl[5] = reinterpret_cast<void*>(&OnState);
            vtbl[6] = reinterpret_cast<void*>(&Trans30);
            vtbl[7] = reinterpret_cast<void*>(&Trans38);
            vtbl[8] = reinterpret_cast<void*>(&HasStates);   // retail slot 8 (D9 #9)
        }
        return vtbl;
    }
};
template <class Tag> StateTable Dispatcher<Tag>::table = {};
template <class Tag> void*      Dispatcher<Tag>::vtbl[16] = {};

// Request a state transition from a ported state handler, replicating the exe idiom:
//   int d=desiredStateId; int i = node->slot08(node, sm->index, &d);
//   if (i>=0) { sm[+0xb]=i (transition index); sm[+0xa]=d (pending state id); }
// The SM object layout: +0x00 cur-node, +0x09 current node-local index (int8),
// +0x0a pending state id (int8), +0x0b transition-target index (int8). StateMachine::Tick
// picks up +0x0a/+0x0b on its next pass to run the transition action and enter the target.
//
// When sm->index (+0x09) is < 0 the SM has never entered a state (a pending default is
// still undrained), so the slot-8 lookup has no current state to search from. Retail
// runs a nested StateMachine_Tick(sm, Time(0), 0, 0) first to drain the pending enter,
// then re-reads BOTH the node pointer and +0x09 before requesting — the tick can change
// the active node. Verified at two retail call sites: 0x14012ae60 (the RP pause path)
// and 0x14012b310 (ConsumeProduceAction_helper_b310). We do the same. With both flag
// args 0 the nested Tick skips its action phase — the only consumer of the time arg —
// so a plain 0 stands in for retail's Time(0.0) byte-for-byte. If the SM has nothing
// pending either, the nested Tick is a no-op, +0x09 stays negative, and the slot-8
// dispatcher bounds-rejects the request exactly as before.
inline void RequestStateTransition(void* sm, int desiredStateId) {
    if (!sm) return;
    uint8_t* s = reinterpret_cast<uint8_t*>(sm);
    if (*reinterpret_cast<int8_t*>(s + 0x09) < 0)
        ::StateMachine_TickReimpl(sm, 0, 0, 0);
    void* node = *reinterpret_cast<void**>(s + 0x00);   // re-read after the nested tick
    if (!node) return;
    void* vt = *reinterpret_cast<void**>(node);
    if (!vt) return;
    typedef int (*RT_t)(void*, int, int*);
    RT_t rt = *reinterpret_cast<RT_t*>(reinterpret_cast<uint8_t*>(vt) + 0x08);
    if (!rt) return;
    int curIdx = *reinterpret_cast<int8_t*>(s + 0x09);  // re-read after the nested tick
    int desired = desiredStateId;
    int idx = rt(node, curIdx, &desired);
    if (idx >= 0) {
        *reinterpret_cast<int8_t*>(s + 0x0b) = static_cast<int8_t>(idx);
        *reinterpret_cast<int8_t*>(s + 0x0a) = static_cast<int8_t>(desired);
    }
}

}  // namespace ControllerLive

// ============================================================================
// ENDGAME B.3.8 (B38_RULES.md D6) — the table-driven controller as a REAL
// member class. A converted ControllerComponent leaf embeds one of these BY
// VALUE at its retail offset instead of stamping `Dispatcher<Tag>::Vtable()`
// into raw bytes; its compiler vtable has retail's 9 slots (the hand-built
// `Dispatcher<Tag>::vtbl` has 16, slots 9-15 `Stub`, which retail does not
// have). Every override forwards to the SAME Dispatcher<Tag> static the old
// table held, so the behaviour is byte-for-byte the old table's.
// ============================================================================
#include "Component/ControllerComponent.h"

namespace ControllerLive {

template <class Tag>
class TableController final : public ::ControllerComponent::Controller {
public:
    explicit TableController(::Component* owner) { _owner = owner; }
    ~TableController() = default;

    int  FindState(int stateId) override { return Dispatcher<Tag>::Step(this, stateId); }
    int  FindTransition(int stateIndex, int* desired) override {
        return Dispatcher<Tag>::RequestTransition(this, stateIndex, desired);
    }
    char PostEvent(void* sm, int stateIndex, int eventId) override {
        return Dispatcher<Tag>::PostEvent(this, sm, stateIndex, eventId);
    }
    char Enter(void* sm, int stateIndex) override { return Dispatcher<Tag>::Enter(this, sm, stateIndex); }
    char Update(void* sm, int stateIndex) override { return Dispatcher<Tag>::Update(this, sm, stateIndex); }
    void OnTransition(void* sm, int stateIndex, int transitionIndex) override {
        Dispatcher<Tag>::OnState(this, sm, stateIndex, transitionIndex);
    }
    char Tick(void* sm, void* time, int stateIndex) override {
        return Dispatcher<Tag>::Trans30(this, sm, time, stateIndex);
    }
    char TickAlt(void* sm, void* time, int stateIndex) override {
        return Dispatcher<Tag>::Trans38(this, sm, time, stateIndex);
    }
    // [8] retail (Well 0x140010260): `table count != 0` (B38_RULES D9 #9).
    char HasStates() override { return Dispatcher<Tag>::HasStates(this); }
};

}  // namespace ControllerLive
