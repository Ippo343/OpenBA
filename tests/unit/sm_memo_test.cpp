// ============================================================================
// tests/unit/sm_memo_test.cpp -- SMMEMO: the exact no-op skip of the state machine's
// slot-8 tick (src/game/Component/sm_memo.h, statemachinecomponent.cpp)
// ============================================================================
// The memo helpers are checked directly, then two copies of one scripted world run side by
// side: one always runs the slot-8 tick in full, the other wraps it exactly as
// StateMachineComponent::Tick does (StateWord, Hit, the trans30 count, Record). Both run a
// literal copy of StateMachine::Tick (0x1400a9660, statemachinecomponent.cpp) over plain
// structs: a root node owning state 0 with no handler (RootController), table nodes that
// dispatch like ControllerLive::Dispatcher (bounds test, null handler -> 0, the trans30
// count in the handler branch only), chains linked like Startup and relinked like
// RelinkAndResume. Handlers log every call, return 0 or 1 and request transitions (the
// RequestStateTransition idiom, nested tick included) from a hash of the step, so the
// scripts are rich but identical in both worlds. After every step the machines (current
// node, four state bytes) and the handler logs must be identical. Two deliberately broken
// memos (no handler count; a key without the pending byte) must diverge: the world can
// tell.
// ============================================================================
#include "unit.h"
#include "Component/sm_memo.h"

#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

namespace {

using SmMemo::StateWord;

uint64_t Mix(uint64_t x) {   // splitmix64
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}
uint64_t Hash(uint64_t a, uint64_t b, uint64_t c = 0, uint64_t d = 0, uint64_t e = 0) {
    return Mix(Mix(Mix(Mix(Mix(a) ^ b) ^ c) ^ d) ^ e);
}

// One state-table entry (controller_state_table.h StateEntry): handler ids, 0 = null.
struct Entry {
    int id;
    int enter, t30, t38, update;
    std::vector<int> targets;      // transitions[] targetIds
    std::vector<int> transHandler; // transitions[] handler ids (0 = null)
};
struct Table {
    std::vector<Entry> e;
    int Find(int id) const {       // TableFindIndex
        for (int i = 0; i < (int)e.size(); ++i) if (e[(size_t)i].id == id) return i;
        return -1;
    }
};

struct Node {
    int id = 0;                    // stable across worlds (logs and compares)
    const Table* table = nullptr;
    Node* parent = nullptr;        // +0x08
    Node* child = nullptr;         // +0x10
};

enum Variant { kExact = 0, kNoCount = 1, kNoPendingKey = 2 };

struct Machine {
    Node* root = nullptr;
    std::vector<Node*> ctrl;       // the controller nodes in component order
    Node* current = nullptr;
    int8_t currentInput = -1, activeState = -1, pendingState = 0, transitionState = -1;
    uint32_t tickMemo = 0;
    int id = 0;
    uint64_t nonce = 0;            // handler calls so far (the same in both worlds while they agree)
};

struct World;
void SmTick(World& w, Machine& m, bool useAlt, bool run);

struct World {
    std::deque<Node> nodes;        // stable addresses
    std::vector<Machine> machines;
    std::vector<uint64_t> log;
    int step = 0;
    bool memo = false;
    Variant variant = kExact;
    long long skipped = 0;
    long long full8 = 0;
    long long t30Calls = 0;

    // A handler call: log it, then maybe request a transition, then return 0 / 1. The
    // decision is a hash of (step, machine, kind, node, state index, nth call of this kind
    // in this step for this machine), the same in both worlds while they agree.
    char Handler(Machine& m, int kind, const Node* n, int idx, int hid) {
        const uint64_t h = Hash((uint64_t)step, (uint64_t)m.id, (uint64_t)kind,
                                (uint64_t)n->id * 4096u + (uint64_t)(idx * 64 + hid), m.nonce++);
        log.push_back(((uint64_t)step << 32) ^ ((uint64_t)m.id << 20) ^ ((uint64_t)kind << 16) ^
                      ((uint64_t)n->id << 8) ^ (uint64_t)(idx & 0xff));
        if ((h & 0xff) < 40 && m.activeState >= 0 &&
            m.activeState < (int)m.current->table->e.size()) {   // ~16%: request a transition
            const Entry& e = m.current->table->e[(size_t)m.activeState];
            if (!e.targets.empty()) Request(m, e.targets[(size_t)((h >> 8) % e.targets.size())]);
        }
        return (char)((h >> 16) & 1);
    }

    // Node slot 1, FindTransition (Dispatcher::RequestTransition).
    static int FindTransition(const Node* n, int curIdx, int* desired) {
        if (curIdx < 0 || curIdx >= (int)n->table->e.size()) return -1;
        const Entry& e = n->table->e[(size_t)curIdx];
        for (int i = 0; i < (int)e.targets.size(); ++i) if (e.targets[(size_t)i] == *desired) return i;
        *desired = -1;
        return -1;
    }
    // ControllerLive::RequestStateTransition.
    void Request(Machine& m, int desiredStateId) {
        if (m.activeState < 0) SmTick(*this, m, false, false);
        Node* node = m.current;
        int curIdx = m.activeState;
        int desired = desiredStateId;
        int idx = FindTransition(node, curIdx, &desired);
        if (idx >= 0) {
            m.transitionState = (int8_t)idx;
            m.pendingState = (int8_t)desired;
        }
    }

    // The node slots (Dispatcher<Tag> / RootController).
    char Enter(Machine& m, Node* n, int idx) {
        if (idx < 0 || idx >= (int)n->table->e.size()) return 0;
        const int h = n->table->e[(size_t)idx].enter;
        return h ? Handler(m, 3, n, idx, h) : 0;
    }
    char Update(Machine& m, Node* n, int idx) {
        if (idx < 0 || idx >= (int)n->table->e.size()) return 0;
        const int h = n->table->e[(size_t)idx].update;
        return h ? Handler(m, 4, n, idx, h) : 0;
    }
    void OnTransition(Machine& m, Node* n, int actIdx, int trIdx) {
        if (actIdx < 0 || actIdx >= (int)n->table->e.size()) return;
        if (trIdx < 0 || trIdx >= 8) return;
        const Entry& e = n->table->e[(size_t)actIdx];
        if (trIdx >= (int)e.transHandler.size()) return;
        const int h = e.transHandler[(size_t)trIdx];
        if (h) Handler(m, 5, n, actIdx, h);
    }
    char Trans30(Machine& m, Node* n, int idx) {   // slot 6
        if (idx < 0 || idx >= (int)n->table->e.size()) return 0;
        const int h = n->table->e[(size_t)idx].t30;
        if (!h) return 0;
        ++SmMemo::g_trans30Calls;                    // as Dispatcher::Trans30
        ++t30Calls;
        return Handler(m, 6, n, idx, h);
    }
    char Trans38(Machine& m, Node* n, int idx) {   // slot 7
        if (idx < 0 || idx >= (int)n->table->e.size()) return 0;
        const int h = n->table->e[(size_t)idx].t38;
        return h ? Handler(m, 7, n, idx, h) : 0;
    }

    // StateMachineComponent::Startup: link in component order, the first default pending.
    void Startup(Machine& m, int defaultState) {
        m.tickMemo = SmMemo::kNone;
        Node* tail = m.root;
        bool got = false;
        for (Node* n : m.ctrl) {
            if (!got) { m.pendingState = (int8_t)defaultState; got = true; }
            n->parent = tail; tail->child = n; tail = n;
        }
    }
    // StateMachineComponent::RelinkAndResume (after a load: current = root, bytes kept).
    void Relink(Machine& m) {
        m.tickMemo = SmMemo::kNone;
        Node* tail = m.root;
        for (Node* n : m.ctrl) { n->parent = tail; tail->child = n; tail = n; }
        if (m.currentInput < 0) return;
        Node* node = m.current;
        while (node->child) node = node->child;
        m.current = node;
        while (node->table->Find(m.currentInput) < 0) { node = m.current->parent; m.current = node; }
    }

    // The slot-8 tick: StateMachineComponent::Tick, with or without the memo.
    void Slot8(Machine& m) {
        if (!memo) { SmTick(*this, m, false, true); ++full8; return; }
        const uint32_t word = Key(StateWord(m.currentInput, m.activeState, m.pendingState, m.transitionState));
        if (SmMemo::Hit(word, m.tickMemo)) { ++skipped; return; }
        const uint32_t calls0 = SmMemo::g_trans30Calls;
        SmTick(*this, m, false, true);
        ++full8;
        const uint32_t after = Key(StateWord(m.currentInput, m.activeState, m.pendingState, m.transitionState));
        m.tickMemo = SmMemo::Record(calls0, variant == kNoCount ? calls0 : SmMemo::g_trans30Calls, after);
    }
    uint32_t Key(uint32_t word) const {   // the broken variant drops the pending byte
        return variant == kNoPendingKey ? ((word & 0xff00ffffu) | 0x00800000u) : word;
    }
};

// A literal copy of StateMachine::Tick (0x1400a9660, statemachinecomponent.cpp).
void SmTick(World& w, Machine& m, bool useAlt, bool run) {
    if (m.pendingState < 0) goto LAB_transition;
    do {
        do {
            if (m.currentInput >= 0) {
                int iVar5 = m.activeState;
                Node* node = m.current;
                while (((iVar5 < 0 || w.Update(m, node, iVar5) == '\0') &&
                        ((node = node->parent) != nullptr))) {
                    iVar5 = node->table->Find(m.currentInput);
                }
                if (m.transitionState >= 0) w.OnTransition(m, m.current, m.activeState, m.transitionState);
            }
            m.currentInput = m.pendingState;
            Node* tail = m.current;
            for (Node* p = m.current->child; p != nullptr; p = p->child) tail = p;
            m.current = tail;
            char c = (char)(tail->table->Find(m.pendingState) & 0xff);
            m.activeState = (int8_t)c;
            while (c < 0) {
                Node* up = m.current->parent;
                m.current = up;
                c = (char)(up->table->Find(m.currentInput) & 0xff);
                m.activeState = (int8_t)c;
            }
            int iVar5 = m.activeState;
            Node* node = m.current;
            m.pendingState = -1;
            m.transitionState = -1;
            while (((iVar5 < 0 || w.Enter(m, node, iVar5) == '\0') &&
                    ((node = node->parent) != nullptr))) {
                iVar5 = node->table->Find(m.currentInput);
            }
        } while (m.pendingState >= 0);
    LAB_transition:
        if (run && m.currentInput >= 0) {
            int iVar5 = m.activeState;
            Node* node = m.current;
            if (!useAlt) {
                while (((iVar5 < 0 || w.Trans30(m, node, iVar5) == '\0') &&
                        ((node = node->parent) != nullptr))) {
                    iVar5 = node->table->Find(m.currentInput);
                }
            } else {
                while (((iVar5 < 0 || w.Trans38(m, node, iVar5) == '\0') &&
                        ((node = node->parent) != nullptr))) {
                    iVar5 = node->table->Find(m.currentInput);
                }
            }
            run = false;
        }
    } while (m.pendingState >= 0);
}

// ---- the tables (static, built once, shared by both worlds) -------------------------------
struct Tables {
    Table root, clearArea, build, workplace, boat, residence, destroy;
    Tables() {
        root.e = {{0, 0, 0, 0, 0, {}, {}}};   // RootController: one empty state 0
        // ClearArea-like: 0 -> {1, 3}, 0x15 -> {1, 3}, 1 -> {2, 3}, 2 -> {3}; enter handlers.
        clearArea.e = {{0, 11, 0, 12, 13, {1, 3}, {0, 0}}, {0x15, 0, 0, 0, 0, {1, 3}, {0, 0}},
                       {1, 14, 0, 0, 0, {2, 3}, {0, 0}}, {2, 15, 0, 0, 0, {3}, {0}}};
        // Build-like: 1 -> {8, 3}, 8 -> {2} (with a transition handler), 2 -> {3}.
        build.e = {{1, 21, 0, 22, 0, {8, 3}, {0, 0}}, {8, 23, 0, 24, 25, {2}, {26}}, {2, 0, 0, 0, 0, {3}, {0}}};
        // WorkPlace-like: 0 -> {1, 3}, 2 -> {3}, 3 -> {6, 4}; no per-frame handler.
        workplace.e = {{0, 31, 0, 0, 0, {1, 3}, {0, 0}}, {2, 32, 0, 0, 0, {3}, {0}}, {3, 33, 0, 0, 0, {6, 4}, {0, 0}}};
        // Boat-like: trans30 handlers in states 0, 1, 3, 4; state 2 has none.
        boat.e = {{0, 41, 42, 0, 0, {1}, {0}}, {1, 43, 44, 0, 0, {2}, {0}}, {2, 45, 0, 0, 0, {3}, {0}},
                  {3, 46, 47, 0, 0, {4}, {0}}, {4, 48, 49, 0, 0, {0, 2}, {0, 0}}};
        // Residence-like: 2 (trans38 heat tick) -> {3}, 3 -> {6, 4}.
        residence.e = {{2, 51, 0, 52, 0, {3}, {0}}, {3, 53, 0, 0, 0, {6, 4}, {0, 0}}};
        // Destroy-like: 3 -> {4, 6}, 6 -> {7}, 7 -> {0, 8, 5}, 4 -> {5}, 5 -> {0}; 8 owned here too.
        destroy.e = {{3, 61, 0, 0, 0, {4, 6}, {0, 0}}, {6, 62, 0, 63, 64, {7}, {0}},
                     {7, 0, 0, 0, 0, {0, 8, 5}, {0, 0, 0}}, {4, 65, 0, 66, 0, {5}, {0}},
                     {5, 67, 0, 68, 0, {0}, {0}}, {8, 69, 0, 0, 0, {3}, {0}}};
    }
};
const Tables& T() { static const Tables t; return t; }

// Builds one world: the same machines, chains and defaults whatever the variant.
void BuildWorld(World& w, int nMachines) {
    const Tables& t = T();
    int nodeId = 1;
    w.machines.resize((size_t)nMachines);
    for (int i = 0; i < nMachines; ++i) {
        Machine& m = w.machines[(size_t)i];
        m.id = i;
        w.nodes.push_back(Node{nodeId++, &t.root, nullptr, nullptr});
        m.root = &w.nodes.back();
        m.current = m.root;
        std::vector<const Table*> chain;
        int def = 0;
        switch (i % 5) {
        case 0: chain = {&t.clearArea, &t.build, &t.workplace, &t.destroy}; def = 0; break;
        case 1: chain = {&t.build, &t.residence, &t.destroy}; def = 1; break;
        case 2: chain = {&t.boat}; def = (i / 5) % 2 ? 2 : 0; break;
        case 3: chain = {&t.workplace, &t.build, &t.residence, &t.destroy}; def = 2; break;
        default: chain = {&t.clearArea, &t.residence, &t.destroy}; def = 0x15; break;
        }
        for (const Table* tb : chain) {
            w.nodes.push_back(Node{nodeId++, tb, nullptr, nullptr});
            m.ctrl.push_back(&w.nodes.back());
        }
        w.Startup(m, def);
        SmTick(w, m, false, false);   // OnAdded: the register-time tick
    }
}

// One step: per machine, maybe an outside request (an event, a panel command, another
// component), maybe a bucket Update (slot 9: trans38 column), maybe a reload (Persist +
// RelinkAndResume), maybe a deep request (pending + transition bytes written directly, as
// the start town and harnesses do), then the slot-8 tick, several times (speed N).
void Step(World& w) {
    for (Machine& m : w.machines) {
        const uint64_t h = Hash(0xABCDu, (uint64_t)w.step, (uint64_t)m.id);
        if ((h & 0x3f) == 0 && m.activeState >= 0) {
            const Entry& e = m.current->table->e[(size_t)m.activeState];
            if (!e.targets.empty()) w.Request(m, e.targets[(size_t)((h >> 8) % e.targets.size())]);
        }
        if (((h >> 6) & 0x3) == 0) SmTick(w, m, true, true);   // bucket round-robin
        if (((h >> 10) & 0xff) == 7) {                         // a save / load round trip
            m.current = m.root;
            w.Relink(m);
        }
        if (((h >> 18) & 0x1ff) == 3 && m.currentInput >= 0) {   // a direct pending stamp (start town)
            m.pendingState = m.currentInput;
        }
        const int iters = 1 + (int)((h >> 28) % 10);
        for (int k = 0; k < iters; ++k) w.Slot8(m);
    }
    ++w.step;
}

bool SameMachines(const World& a, const World& b) {
    if (a.machines.size() != b.machines.size()) return false;
    for (size_t i = 0; i < a.machines.size(); ++i) {
        const Machine& x = a.machines[i];
        const Machine& y = b.machines[i];
        if (x.current->id != y.current->id) return false;
        if (x.currentInput != y.currentInput || x.activeState != y.activeState ||
            x.pendingState != y.pendingState || x.transitionState != y.transitionState) return false;
    }
    return true;
}

// Runs faithful vs memo (with `variant`) for `steps`; returns the first step that differs,
// -1 when the two worlds stayed identical throughout.
int RunTwoWorlds(Variant variant, int machines, int steps, World* outMemo = nullptr) {
    World full, memo;
    memo.memo = true;
    memo.variant = variant;
    BuildWorld(full, machines);
    BuildWorld(memo, machines);
    for (int s = 0; s < steps; ++s) {
        Step(full);
        Step(memo);
        if (!SameMachines(full, memo) || full.log != memo.log) {
            if (outMemo) { outMemo->skipped = memo.skipped; outMemo->full8 = memo.full8; }
            return s;
        }
        full.log.clear();   // compared: keep the logs per step
        memo.log.clear();
    }
    if (outMemo) {
        outMemo->skipped = memo.skipped;
        outMemo->full8 = memo.full8;
        outMemo->t30Calls = memo.t30Calls;
    }
    return -1;
}

} // namespace

UNIT_TEST(sm_memo_state_word) {
    CHECK_EQ(StateWord(2, 1, -1, -1), 0xffff0102u);
    CHECK_EQ(StateWord(-1, -1, 0, -1), 0xff00ffffu);   // the ctor's quad 0xff00ffff
    CHECK_EQ(StateWord(0, 0, -1, -1), 0xffff0000u);
    CHECK(!SmMemo::PendingOf(StateWord(5, 2, -1, 3)));
    CHECK(SmMemo::PendingOf(StateWord(5, 2, 0, 3)));
    CHECK(SmMemo::PendingOf(StateWord(5, 2, 8, -1)));
    CHECK(SmMemo::PendingOf(StateWord(0, 0, 0, 0)));
    CHECK(!SmMemo::PendingOf(StateWord(-1, -1, -1, -1)));
}

UNIT_TEST(sm_memo_record_and_hit) {
    using SmMemo::kNone;
    const uint32_t idle = StateWord(2, 0, -1, -1);
    const uint32_t memo = SmMemo::Record(7, 7, idle);        // no handler called: memo
    CHECK_EQ(memo, ~idle);                                   // stored inverted
    CHECK_EQ(SmMemo::Record(7, 8, idle), kNone);             // a handler ran: no memo
    CHECK_EQ(SmMemo::Record(0xffffffffu, 0u, idle), kNone);  // wrapped count still differs
    CHECK_EQ(SmMemo::Record(3, 3, StateWord(2, 0, 4, 1)), kNone);   // pending: never memoised
    CHECK(SmMemo::Hit(idle, memo));
    CHECK(!SmMemo::Hit(StateWord(2, 0, 3, 0), memo));       // a request since: decline
    CHECK(!SmMemo::Hit(StateWord(3, 0, -1, -1), memo));      // another input
    CHECK(!SmMemo::Hit(StateWord(2, 1, -1, -1), memo));      // another index
    CHECK(!SmMemo::Hit(StateWord(2, 0, -1, 0), memo));       // another transition byte
    CHECK(!SmMemo::Hit(idle, kNone));                        // no memo
    // kNone (a new or cleared memo) never matches a pending word, the all-zero one included
    // (stored plainly, 0 would), and matches only the all -1 word, where the full tick is a
    // no-op (nothing pending, input < 0).
    CHECK(!SmMemo::Hit(StateWord(0, 0, 0, 0), kNone));
    CHECK(SmMemo::Hit(StateWord(-1, -1, -1, -1), kNone));
    for (int a = -128; a < 128; a += 3)
        for (int p = 0; p < 128; p += 5)
            CHECK(!SmMemo::Hit(StateWord((int8_t)a, (int8_t)(a + 1), (int8_t)p, (int8_t)(a ^ p)), kNone));
}

UNIT_TEST(sm_memo_shadow_diff) {
    int a = 0, b = 0;
    CHECK_EQ(SmMemo::ShadowDiff(4, 4, 9, 9, &a, &a), 0LL);
    CHECK_EQ(SmMemo::ShadowDiff(4, 5, 9, 9, &a, &a), 1LL);
    CHECK_EQ(SmMemo::ShadowDiff(4, 4, 9, 8, &a, &a), 2LL);
    CHECK_EQ(SmMemo::ShadowDiff(4, 4, 9, 9, &a, &b), 4LL);
    CHECK_EQ(SmMemo::ShadowDiff(4, 5, 9, 8, &a, &b), 7LL);
}

UNIT_TEST(sm_memo_prof_window) {
    SmMemo::ProfWindow w;
    CHECK_EQ(w.SavedTicks(), 0.0);
    w.Add(100, false); w.Add(120, false); w.Add(20, true); w.Add(30, true); w.Add(40, true);
    CHECK_EQ(w.calls, 5LL);
    CHECK_EQ(w.skips, 3LL);
    CHECK_EQ(w.Full(), 2LL);
    CHECK_EQ(w.FullTicks(), 220LL);
    // (110 - 30) x 3
    CHECK(w.SavedTicks() > 239.99 && w.SavedTicks() < 240.01);
    w.Reset();
    CHECK_EQ(w.calls, 0LL);
}

// The two-worlds test: the memo wrapper never changes a state byte, a current node or a
// handler call, over 60 machines x 3,000 steps x 1-10 slot-8 ticks per step.
UNIT_TEST(sm_memo_two_worlds_bit_identical) {
    World stats;
    const int diverged = RunTwoWorlds(kExact, 60, 3000, &stats);
    CHECK_EQ(diverged, -1);
    // The script must exercise both sides: skips, full ticks and trans30 handler calls.
    CHECK(stats.skipped > 100000);
    CHECK(stats.full8 > 10000);
    CHECK(stats.t30Calls > 1000);
    std::printf("    sm_memo two worlds: %lld skipped, %lld full slot-8 ticks, %lld trans30 calls\n",
                stats.skipped, stats.full8, stats.t30Calls);
}

// The world can tell: a memo that ignores the handler count, or one whose key drops the
// pending byte, diverges from the faithful world.
UNIT_TEST(sm_memo_broken_variants_diverge) {
    CHECK(RunTwoWorlds(kNoCount, 60, 3000) >= 0);
    CHECK(RunTwoWorlds(kNoPendingKey, 60, 3000) >= 0);
}
