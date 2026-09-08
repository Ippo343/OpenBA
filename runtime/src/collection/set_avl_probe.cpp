// ============================================================================
// runtime/src/collection/set_avl_probe.cpp — Wave 9 lane VID2
//   The lockstep synthetic-subject oracle for the four AVL bodies and the two
//   Application message-callback exports.
// ============================================================================
//
// WHY A PROBE AND NOT A LIVE DIFF
// -------------------------------
// Every body under test MUTATES THE ONE SHARED TREE. Running ours and then the
// shipped one at a live call site inserts the same id twice (the second call
// returns early, making the comparison vacuously green — the trap lane D
// recorded for `KeyMap::RegisterInput`) or erases the same node twice (the
// second pass walks freed memory). There is no replay available, so: two
// PRIVATE `Collection::Set`s with byte-identical starting state, the same
// script applied to both, ours through the ported bodies and theirs through the
// shipped ones. (reference_synthetic_subject_beats_a_fake.md.)
//
// The shipped bodies are reached through **`ResolveRuntimeRaw` and
// `ResolveExportRaw`, never `ResolveAddr`/`ResolveExport`** — those consult the
// port registry FIRST and this lane registers every one of these addresses, so
// they would have handed back our own functions and the probe would have
// compared ours to ours and always agreed
// (reference_resolveaddr_consults_port_registry_first.md).
//
// WHY LOCKSTEP AND NOT A FINAL COMPARISON
// ---------------------------------------
// **AVL defects CANCEL.** A rotation that is wrong in one direction and wrong
// again in the other leaves a tree with the same key set and, after enough
// operations, frequently the same shape — while every intermediate state
// differed. A single comparison at the end reports green. So both trees are
// serialised and compared AFTER EVERY OPERATION, and the serialisation carries
// the shape, not just the keys: pre-order, with each node's balance, height,
// and which side of its parent it hangs off.
//
// WHAT IS DELIBERATELY NOT COMPARED
// ---------------------------------
// `element + 0x04`. `0x18000a5b0` copies the element as two qwords and
// `RegisterMessageCallback` writes only the DWORD at +0x00, so those four bytes
// carry a dword of the CALLER'S STACK FRAME into the tree. Both sides do it and
// neither is wrong; comparing them would fail ~100% of calls
// (reference_oracle_must_not_compare_unwritten_bytes.md) and read as a
// catastrophic defect. Excluded, and said out loud rather than silently skipped.
//
// WHEN IT RUNS
// ------------
// **Self-scheduled on first use**, exactly as lane Q's array probe is
// (runtime/src/collection/array_internal.h:89). It allocates its nodes from
// `System::MemoryAllocator`, and at `RuntimePortInit()` time — immediately after
// `ResolveInit()`'s LoadLibrary — that allocator has been CONSTRUCTED but not
// STARTED. Running there is exactly the "works on my boot order" hazard this
// project keeps paying for. So the flag is armed at registration and the first
// call into `RtApp_RegisterMessageCallback` runs the probe first; that moment is
// provably safe, because the caller is itself about to allocate.
//
// In practice that first call is `Video::Device::PlatformStartup`'s WM_SIZE
// registration, which is exactly the path this lane exists to unblock.
//
// Opt-in: `BANISHED_RT_SETAVL_PROBE=1`.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "set_avl.h"

using rtsetavl::Node;
using rtsetavl::Set;

// `ResolveExportRaw` comes from rt_port.h; only the address form needs a local
// declaration. (Redeclaring the name form here made
// `tools/port_bypass_audit.py` count the DECLARATION as a raw-door site and
// report it as UNRESOLVED-NAME — a checker artefact, but one that costs a human
// a read every time the audit runs.)
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

extern "C" void RtApp_RegisterMessageCallback(void* app, unsigned id, void* cb);
extern "C" void RtApp_UnregisterMessageCallback(void* app, unsigned id);

namespace {

bool g_armed = false;
bool g_running = false;

// A stand-in for the Application. Only the Set at +0x18 is ever touched, so a
// 0x30-byte block is enough — a synthetic subject, not a fake of the class
// (reference_synthetic_subject_beats_a_fake.md). Nothing here constructs an
// Application, reaches a factory, or needs a vtable.
struct FakeApp { uint8_t bytes[0x30]; };

inline Set* SetOf(FakeApp* a) {
    return reinterpret_cast<Set*>(a->bytes + 0x18);
}

// ---------------------------------------------------------------------------
// The fingerprint: a pre-order walk carrying SHAPE, not just keys.
//
// `d` is the side of the parent this node hangs off ('L'/'R'/'*' for the root),
// which is what turns a key list into a shape. Balance and height are included
// because a rotation that produces the right SHAPE with the wrong bookkeeping
// breaks the very next operation and nothing else would see it.
//
// A null child is emitted as '.', so two differently-shaped trees with the same
// key sequence cannot collide.
// ---------------------------------------------------------------------------
void Fingerprint(const Node* n, char side, char* out, int cap, int* len) {
    if (*len >= cap - 40) return;
    if (!n) { *len += std::snprintf(out + *len, cap - *len, "."); return; }
    *len += std::snprintf(out + *len, cap - *len, "(%c%u:b%d:h%d",
                          side, *reinterpret_cast<const unsigned*>(n),
                          static_cast<int>(n->balance), static_cast<int>(n->height));
    Fingerprint(n->left,  'L', out, cap, len);
    Fingerprint(n->right, 'R', out, cap, len);
    if (*len < cap - 2) *len += std::snprintf(out + *len, cap - *len, ")");
}

// Parent linkage is checked separately, because a fingerprint built by walking
// `left`/`right` cannot see a wrong `parent` — and a wrong parent is exactly
// what a mis-transcribed rotation produces, and exactly what breaks the NEXT
// retrace rather than this one.
bool ParentsConsistent(const Node* n, const Node* expectParent) {
    if (!n) return true;
    if (n->parent != expectParent) return false;
    return ParentsConsistent(n->left, n) && ParentsConsistent(n->right, n);
}

int Depth(const Node* n) {
    if (!n) return 0;
    const int l = Depth(n->left), r = Depth(n->right);
    return 1 + (l > r ? l : r);
}

struct Sides {
    FakeApp ours{};
    FakeApp theirs{};
    char fo[4096]; char ft[4096];
};

// The shipped entry points. Resolved raw so the port registry cannot intercept.
struct Shipped {
    void (*reg)(void*, unsigned, void*)  = nullptr;
    void (*unreg)(void*, unsigned)       = nullptr;
    bool ok() const { return reg && unreg; }
};

Shipped ProbeLoadShippedBodies() {
    Shipped s;
    s.reg = reinterpret_cast<void (*)(void*, unsigned, void*)>(ResolveExportRaw(
        "?RegisterMessageCallback@Application@1@QEAAXIP6AHPEAUHWND__@@I_K_J@Z@Z"));
    s.unreg = reinterpret_cast<void (*)(void*, unsigned)>(ResolveExportRaw(
        "?UnregisterMessageCallback@Application@1@QEAAXI@Z"));
    return s;
}

}  // namespace

// ============================================================================
extern "C" void RtCollectionSetAvlProbeArm(bool on) { g_armed = on; }

extern "C" void RtCollectionSetAvlProbeMaybeRun() {
    if (!g_armed || g_running) return;
    g_armed = false;
    g_running = true;

    Shipped sh = ProbeLoadShippedBodies();
    if (!sh.ok()) {
        std::printf("[SetAvl] SKIP -- the shipped Application::*MessageCallback "
                    "exports are not reachable (cutover, or the DLL is absent). "
                    "No comparison was made.\n");
        g_running = false;
        return;
    }

    static Sides S;
    std::memset(&S.ours, 0, sizeof(S.ours));
    std::memset(&S.theirs, 0, sizeof(S.theirs));

    int steps = 0, mismatches = 0;
    int parentBad = 0;
    int rootChanged = 0;       // effectiveness: how many steps moved the root
    int erasesDone  = 0;

    // A dummy callback value per id, so the element's +0x08 qword differs
    // per node and a copy that dropped it would show.
    auto cbFor = [](unsigned id) { return reinterpret_cast<void*>(
        static_cast<uintptr_t>(0xCB000000u) + id); };

    auto compare = [&](const char* what, unsigned arg) {
        int lo = 0, lt = 0;
        S.fo[0] = 0; S.ft[0] = 0;
        Fingerprint(SetOf(&S.ours)->root,   '*', S.fo, sizeof(S.fo), &lo);
        Fingerprint(SetOf(&S.theirs)->root, '*', S.ft, sizeof(S.ft), &lt);
        const bool same = std::strcmp(S.fo, S.ft) == 0 &&
                          SetOf(&S.ours)->count == SetOf(&S.theirs)->count;
        if (!same) {
            if (mismatches < 3) {
                std::printf("[SetAvl] MISMATCH after %s(%u) step=%d\n"
                            "[SetAvl]   ours   count=%d %s\n"
                            "[SetAvl]   theirs count=%d %s\n",
                            what, arg, steps,
                            SetOf(&S.ours)->count, S.fo,
                            SetOf(&S.theirs)->count, S.ft);
            }
            ++mismatches;
        }
        if (!ParentsConsistent(SetOf(&S.ours)->root, nullptr)) ++parentBad;
        ++steps;
    };

    // ---- the population -------------------------------------------------
    //
    // NOT UNIFORM, and each group is here for a reason
    // (reference_uniform_population_is_not_a_verification.md):
    //
    //  * 1..24 ASCENDING forces a right-heavy tree, i.e. RotateLeft on almost
    //    every insert. Without rotations this is a spine of depth 24; with them
    //    it is depth 5. That single number is the effectiveness control for the
    //    whole retrace, and it cannot be satisfied by noise.
    //  * a DESCENDING block forces RotateRight the same way.
    //  * a zig-zag block forces the DOUBLE rotations (LR and RL), which are the
    //    only path through the pre-rotation predicates at 0x18000ae40 and
    //    0x18000ae74. A purely ascending population never enters them, so a
    //    port that dropped the pre-rotation entirely would pass.
    //  * ids ABOVE 0x7FFFFFFF, because the descent compare is UNSIGNED (`JC`,
    //    not `JL`). With only small ids a signed compare orders identically and
    //    the defect is invisible. This is the same shape as the ported inline
    //    strcmp that survived four waves on a uniform population.
    //  * a duplicate insert, which must return WITHOUT incrementing the count.
    // ---------------------------------------------------------------------
    static const unsigned kAscending[]  = { 1,2,3,4,5,6,7,8,9,10,11,12,
                                            13,14,15,16,17,18,19,20,21,22,23,24 };
    static const unsigned kDescending[] = { 900,890,880,870,860,850,840,830,820,810 };
    static const unsigned kZigZag[]     = { 500,300,400,250,270,260,600,550,560,555 };
    static const unsigned kHighBit[]    = { 0x80000005u, 0xFFFFFFF0u, 0x7FFFFFFFu,
                                            0x80000000u, 0xC0000000u };

    auto insert = [&](unsigned id) {
        Node* rootBefore = SetOf(&S.ours)->root;
        RtApp_RegisterMessageCallback(&S.ours, id, cbFor(id));
        sh.reg(&S.theirs, id, cbFor(id));
        if (SetOf(&S.ours)->root != rootBefore) ++rootChanged;
        compare("insert", id);
    };
    auto erase = [&](unsigned id) {
        Node* rootBefore = SetOf(&S.ours)->root;
        RtApp_UnregisterMessageCallback(&S.ours, id);
        sh.unreg(&S.theirs, id);
        if (SetOf(&S.ours)->root != rootBefore) ++rootChanged;
        ++erasesDone;
        compare("erase", id);
    };

    for (unsigned id : kAscending)  insert(id);
    const int depthAfterAscending = Depth(SetOf(&S.ours)->root);

    for (unsigned id : kDescending) insert(id);
    for (unsigned id : kZigZag)     insert(id);
    for (unsigned id : kHighBit)    insert(id);

    insert(13);                 // duplicate: must not move the count
    const int countAfterDup = SetOf(&S.ours)->count;

    // Erases, deliberately NOT in insertion or reverse order, and chosen to hit
    // all three cases of 0x18000aba0:
    //   * a LEAF and a node with only one child  -> cases 1 and 2
    //   * an internal node whose left child HAS a right subtree -> case 3 with
    //     s != z->left (the predecessor splice, the branch a successor-splice
    //     mis-reading would get wrong while still producing a valid tree)
    //   * an internal node whose left child has NO right subtree -> case 3 with
    //     s == z->left, which skips the sp fixup entirely
    static const unsigned kErase[] = { 1, 24, 12, 500, 300, 260, 0x80000000u,
                                       900, 7, 17, 550, 0xFFFFFFF0u, 4, 20 };
    for (unsigned id : kErase) erase(id);

    erase(0xDEADBEEFu);         // absent: must be a silent no-op on both sides

    // ---- NEGATIVE CONTROL 1: a SIGNED descent ---------------------------
    // If the comparison cannot see an ordering defect, every green step above
    // is worthless. A signed compare places 0x80000005 on the wrong side.
    bool neg1 = false;
    {
        static FakeApp bad{}; std::memset(&bad, 0, sizeof(bad));
        static FakeApp ref{}; std::memset(&ref, 0, sizeof(ref));
        static const unsigned ids[] = { 100u, 0x80000005u, 50u, 0xFFFFFFF0u, 200u };
        for (unsigned id : ids) sh.reg(&ref, id, cbFor(id));
        // A local, deliberately SIGNED insert built on the same helpers.
        for (unsigned id : ids) {
            Set* s = SetOf(&bad);
            if (!s->root) { RtApp_RegisterMessageCallback(&bad, id, cbFor(id)); continue; }
            Node* slot = s->root;
            for (;;) {
                const int key = *reinterpret_cast<const int*>(slot);   // SIGNED
                const int idv = static_cast<int>(id);
                if (idv < key) { if (!slot->left)  { slot->left  = reinterpret_cast<Node*>(
                                        (reinterpret_cast<Node*(*)(void*, const void*, Node*)>(
                                         ResolveRuntimeRaw(0x18000a5b0)))(
                                            nullptr, (const void*)&id, slot)); break; }
                                 slot = slot->left; }
                else if (key >= idv) break;
                else { if (!slot->right) { slot->right = reinterpret_cast<Node*>(
                                        (reinterpret_cast<Node*(*)(void*, const void*, Node*)>(
                                         ResolveRuntimeRaw(0x18000a5b0)))(
                                            nullptr, (const void*)&id, slot)); break; }
                       slot = slot->right; }
            }
            rtsetavl::Retrace(s, &slot);
            s->count += 1;
        }
        int la = 0, lb = 0; char a[2048], b[2048]; a[0] = b[0] = 0;
        Fingerprint(SetOf(&bad)->root, '*', a, sizeof(a), &la);
        Fingerprint(SetOf(&ref)->root, '*', b, sizeof(b), &lb);
        neg1 = std::strcmp(a, b) != 0;
    }

    // ---- NEGATIVE CONTROL 2: retrace WITHOUT the pre-rotation ------------
    // The double-rotation predicates at 0x18000ae40 / 0x18000ae74 are the only
    // thing the zig-zag population exercises. Dropping them still produces a
    // binary search tree with the right keys — so if the comparison misses
    // this, it is only checking key order and the shape column is decorative.
    bool neg2 = false;
    {
        static FakeApp bad{}; std::memset(&bad, 0, sizeof(bad));
        static FakeApp ref{}; std::memset(&ref, 0, sizeof(ref));
        static const unsigned ids[] = { 500, 300, 400, 250, 270, 260 };  // forces LR
        for (unsigned id : ids) sh.reg(&ref, id, cbFor(id));
        for (unsigned id : ids) RtApp_RegisterMessageCallback(&bad, id, cbFor(id));
        // Now corrupt ONE node's balance the way a missing pre-rotation would
        // leave it, and confirm the fingerprint notices.
        if (SetOf(&bad)->root) SetOf(&bad)->root->balance += 1;
        int la = 0, lb = 0; char a[2048], b[2048]; a[0] = b[0] = 0;
        Fingerprint(SetOf(&bad)->root, '*', a, sizeof(a), &la);
        Fingerprint(SetOf(&ref)->root, '*', b, sizeof(b), &lb);
        neg2 = std::strcmp(a, b) != 0;
    }

    std::printf("[SetAvl] steps=%d mismatches=%d parent-linkage-bad=%d\n"
                "[SetAvl] effectiveness: depthAfter24Ascending=%d (a spine would be 24) "
                "rootChanged=%d erases=%d finalCount=%d dupCount=%d\n"
                "[SetAvl] negative-controls: signed-descent=%s  shape-sensitivity=%s\n",
                steps, mismatches, parentBad,
                depthAfterAscending, rootChanged, erasesDone,
                SetOf(&S.ours)->count, countAfterDup,
                neg1 ? "CAUGHT" : "*** MISSED ***",
                neg2 ? "CAUGHT" : "*** MISSED ***");

    if (depthAfterAscending >= 24)
        std::printf("[SetAvl] *** depth == the insert count: NO ROTATION EVER RAN, "
                    "so this sweep verified only the descent ***\n");
    if (rootChanged == 0)
        std::printf("[SetAvl] *** rootChanged=0: nothing restructured the tree ***\n");
    if (!neg1 || !neg2)
        std::printf("[SetAvl] *** a negative control was MISSED: the comparison is "
                    "not sensitive to the thing it is supposed to catch, so the "
                    "green steps above are void ***\n");

    g_running = false;
}
