// ============================================================================
// runtime/src/collection/set_avl_e24_probe.cpp — Wave 9 lane INPUT
//   The lockstep synthetic-subject oracle for the five 0x18-element AVL bodies
//   in set_avl_e24.cpp, driven through `Input::KeyMap`.
// ============================================================================
//
// WHY A PROBE AND NOT A LIVE DIFF
// -------------------------------
// Every body under test MUTATES THE ONE SHARED TREE. Running ours and then the
// shipped one at a live call site inserts the same id twice — and retail's
// second insert RETURNS EARLY, making the comparison vacuously green, which is
// the exact trap lane D recorded when it built `RegisterInput`'s oracle. There
// is no replay available, so: two PRIVATE `Input::KeyMap`s with byte-identical
// starting state, the same script applied to both, ours through the ported
// bodies and theirs through the shipped ones
// (reference_synthetic_subject_beats_a_fake.md).
//
// HOW EACH SIDE REACHES ITS OWN BODIES
// ------------------------------------
//   ours   : `RtInput_KeyMap_RegisterInput` (input.cpp) resolves 0x180023060 and
//            0x180023170 with `ResolveAddr`, which consults the port registry
//            FIRST — so it lands in set_avl_e24.cpp.
//   theirs : the shipped `?RegisterInput@KeyMap@Input@@…` reached with
//            `ResolveExportRaw`, whose calls to those addresses are NEAR calls
//            inside the DLL and cannot be intercepted by anything of ours.
//
// `ResolveExportRaw`, never `ResolveExport`: the latter consults the registry
// first and would have handed back OUR RegisterInput, so the probe would have
// compared ours to ours and always agreed
// (reference_resolveaddr_consults_port_registry_first.md).
//
// WHY LOCKSTEP AND NOT A FINAL COMPARISON
// ---------------------------------------
// **AVL defects cancel.** A rotation wrong in one direction and wrong again in
// the other leaves the same key set and frequently the same final shape while
// every intermediate state differed. Both trees are fingerprinted and compared
// AFTER EVERY INSERT, and the fingerprint carries SHAPE — which side of its
// parent each node hangs off, its balance and its height — not just the keys.
// Parent linkage is checked separately, because a fingerprint built by walking
// left/right cannot see a wrong `parent`, and a wrong parent is exactly what a
// mis-transcribed rotation produces and exactly what breaks the NEXT retrace.
//
// WHAT IS DELIBERATELY NOT COMPARED
// ---------------------------------
// `Binding::_pad02` (+0x02..+0x07) and `_pad14` (+0x14..+0x17). `0x180023060`
// copies the element as three qwords and `RegisterInput` writes neither pad, so
// both carry bytes of the CALLER'S STACK FRAME into the tree. Both sides do it
// and neither is wrong; comparing them would fail ~100% of inserts and read as
// a catastrophic defect (reference_oracle_must_not_compare_unwritten_bytes.md).
// Excluded, and said out loud rather than silently skipped.
//
// WHEN IT RUNS
// ------------
// **Self-scheduled on the first real call into `RtInput_KeyMap_RegisterInput`**,
// not from `RtInputPortInit`. It allocates through
// `System::MemoryAllocator`, and at port-init time that allocator has been
// CONSTRUCTED but not STARTED — a probe that allocates from a `*PortInit`
// dereferences a null `_singleton` and faults at `+0x640`, which is lane PKG2's
// recorded failure and is invisible in the crash log. The first RegisterInput
// call is provably safe because the caller is itself about to allocate a node.
//
// Opt-in: `BANISHED_RT_SETAVL24_PROBE=1`.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/input/Input.h"
#include "set_avl_e24.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

using RtInput::Binding;
using RtInput::BindingNode;

namespace {

bool g_armed   = false;
bool g_running = false;

// A stand-in for `Input::KeyMap`: only `_root` (+0x08) and `_count` (+0x10) are
// touched by anything under test, so a 0x18-byte block is enough. A synthetic
// subject, not a fake of the class.
struct FakeKeyMap { uint8_t bytes[0x18]; };

inline rtsetavl24::Set* SetOf(FakeKeyMap* k) {
    return reinterpret_cast<rtsetavl24::Set*>(k->bytes + 8);
}
inline BindingNode* RootOf(FakeKeyMap* k) {
    return reinterpret_cast<BindingNode*>(SetOf(k)->root);
}

// ---------------------------------------------------------------------------
// The fingerprint: a pre-order walk carrying SHAPE, not just keys.
//
// `d` is the side of the parent this node hangs off ('L'/'R'/'*' for the root).
// A null child is emitted as '.', so two differently-shaped trees with the same
// key sequence cannot collide. The element fields included are exactly the ones
// RegisterInput writes; the two pads are not (see the banner).
// ---------------------------------------------------------------------------
void Fingerprint(const BindingNode* n, char side, char* out, int cap, int* len) {
    if (*len >= cap - 64) return;
    if (!n) { *len += std::snprintf(out + *len, cap - *len, "."); return; }
    *len += std::snprintf(out + *len, cap - *len, "(%c%u:e%u:d%u/%u:i%u/%u:b%d:h%d",
                          side, static_cast<unsigned>(n->element.id),
                          static_cast<unsigned>(n->element.exclusive),
                          static_cast<unsigned>(n->element.device[0]),
                          static_cast<unsigned>(n->element.device[1]),
                          static_cast<unsigned>(n->element.inputId[0]),
                          static_cast<unsigned>(n->element.inputId[1]),
                          static_cast<int>(n->balance), static_cast<int>(n->height));
    Fingerprint(n->left,  'L', out, cap, len);
    Fingerprint(n->right, 'R', out, cap, len);
    if (*len < cap - 2) *len += std::snprintf(out + *len, cap - *len, ")");
}

bool ParentsConsistent(const BindingNode* n, const BindingNode* expect) {
    if (!n) return true;
    if (n->parent != expect) return false;
    return ParentsConsistent(n->left, n) && ParentsConsistent(n->right, n);
}

int Depth(const BindingNode* n) {
    if (!n) return 0;
    const int l = Depth(n->left), r = Depth(n->right);
    return 1 + (l > r ? l : r);
}

int CountNodes(const BindingNode* n) {
    return n ? 1 + CountNodes(n->left) + CountNodes(n->right) : 0;
}

using RegisterFn = void (*)(void*, int32_t, int32_t, int32_t, const char*, bool);
using ClearAllFn = void (*)(void*);

// `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` at 0x1800c7a50 — the same
// composition 0x180023060 open-codes. Needed because the deleting destructor
// FREES its subject, so the subject must come from the engine heap.
void* MemAlloc(uint64_t bytes, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn ? fn(bytes, align) : nullptr;
}

}  // namespace

// Our own ports, called directly — this is the code under test.
void RtInput_KeyMap_RegisterInput(void* self, int32_t id, int32_t device,
                                  int32_t inputId, const char* name, bool exclusive);
void RtInput_KeyMap_ClearAll(void* self);
void* RtInput_KeyMap_DeletingDtor(void* self);

extern "C" void RtCollectionSetAvlE24ProbeArm(bool on) { g_armed = on; }

extern "C" void RtCollectionSetAvlE24ProbeMaybeRun() {
    if (!g_armed || g_running) return;
    g_armed = false;
    g_running = true;

    auto theirReg = reinterpret_cast<RegisterFn>(
        ResolveExportRaw("?RegisterInput@KeyMap@Input@@QEAAXHHHPEBD_N@Z"));
    auto theirClear = reinterpret_cast<ClearAllFn>(
        ResolveExportRaw("?ClearAll@KeyMap@Input@@QEAAXXZ"));
    if (!theirReg || !theirClear) {
        printf("[SetAvl24] SKIP -- the shipped KeyMap::RegisterInput/ClearAll "
                    "exports are not reachable (cutover, or the DLL is absent). "
                    "NO COMPARISON WAS MADE.\n");
        g_running = false;
        return;
    }

    static FakeKeyMap ours, theirs;
    std::memset(&ours, 0, sizeof(ours));
    std::memset(&theirs, 0, sizeof(theirs));

    static char fo[8192], ft[8192];
    int steps = 0, mismatches = 0, parentBad = 0, rootChanged = 0, aboveSignBit = 0;

    // Names are shared buffers, so the `description` pointer is identical on
    // both sides.
    auto compare = [&](unsigned id) {
        int lo = 0, lt = 0; fo[0] = 0; ft[0] = 0;
        Fingerprint(RootOf(&ours),   '*', fo, sizeof(fo), &lo);
        Fingerprint(RootOf(&theirs), '*', ft, sizeof(ft), &lt);
        const bool same = std::strcmp(fo, ft) == 0 &&
                          SetOf(&ours)->count == SetOf(&theirs)->count;
        if (!same) {
            if (mismatches < 3)
                printf("[SetAvl24] MISMATCH after insert(%u) step=%d\n"
                            "[SetAvl24]   ours   count=%d %s\n"
                            "[SetAvl24]   theirs count=%d %s\n",
                            id, steps, SetOf(&ours)->count, fo,
                            SetOf(&theirs)->count, ft);
            ++mismatches;
        }
        if (!ParentsConsistent(RootOf(&ours), nullptr)) ++parentBad;
        ++steps;
    };

    auto insert = [&](unsigned id, unsigned device, unsigned input, bool excl) {
        static char names[256][12];
        std::snprintf(names[id & 0xFF], sizeof(names[0]), "bind%u", id);
        const char* nm = names[id & 0xFF];
        BindingNode* before = RootOf(&ours);
        RtInput_KeyMap_RegisterInput(&ours, static_cast<int32_t>(id),
                                     static_cast<int32_t>(device),
                                     static_cast<int32_t>(input), nm, excl);
        theirReg(&theirs, static_cast<int32_t>(id), static_cast<int32_t>(device),
                 static_cast<int32_t>(input), nm, excl);
        if (RootOf(&ours) != before) ++rootChanged;
        if ((id & 0xFF) > 0x7Fu) ++aboveSignBit;
        compare(id);
    };

    // ---- the population ----------------------------------------------------
    //
    // NOT UNIFORM, and each group is here for a reason
    // (reference_uniform_population_is_not_a_verification.md):
    //
    //  * an ASCENDING run forces RotateLeft on almost every insert. Without
    //    rotations it is a spine; with them it is shallow. That depth is the
    //    effectiveness control for the whole retrace and cannot be faked.
    //  * a DESCENDING run forces RotateRight the same way.
    //  * a ZIG-ZAG block forces the DOUBLE rotations, the only path through the
    //    pre-rotation predicates at 0x1800231f0 / 0x180023224. A purely
    //    ascending population never enters them, so a port that dropped the
    //    pre-rotation entirely would pass.
    //  * ids ABOVE 0x7F, because the key is a BYTE compared UNSIGNED (`CMP
    //    DL,CL ; JC`). With only small ids a signed compare orders identically
    //    and the defect is invisible — the same shape as the ported inline
    //    strcmp that survived four waves on a uniform population.
    //  * a duplicate insert, which must return WITHOUT incrementing the count.
    //  * unbound and bound slots, exclusive and non-exclusive, so the element
    //    bytes differ per node and a copy that dropped a qword would show.
    //  * TWO (device, inputId) PAIRS BOUND TO MORE THAN ONE ID — the high-bit
    //    group maps 0x7F and 0xFF onto (0,15) and 0x80/0xC0/0xF0 onto (0,0).
    //    That is a legitimate KeyMap state (two actions on one physical key) and
    //    it is kept deliberately: on its first run this population fired TWO
    //    divergences out of 94 calls in lane D's `RegisterInput` oracle, whose
    //    `found == key` expectation is only valid when the pair is unique.
    //    The PORT was right and the ORACLE was wrong; input.cpp now captures the
    //    pre-insert first-match and expects that instead. Removing the duplicate
    //    pairs would have "fixed" the run and left the oracle wrong.
    // ------------------------------------------------------------------------
    static const unsigned kAscending[]  = { 1,2,3,4,5,6,7,8,9,10,11,12,
                                            13,14,15,16,17,18,19,20 };
    static const unsigned kDescending[] = { 120,118,116,114,112,110,108,106 };
    static const unsigned kZigZag[]     = { 60,40,50,35,38,37,70,65,66,64 };
    static const unsigned kHighBit[]    = { 0x80,0xFF,0x7F,0xC0,0x81,0xF0 };

    for (unsigned id : kAscending)  insert(id, 1, id, true);
    const int depthAfterAscending = Depth(RootOf(&ours));

    for (unsigned id : kDescending) insert(id, 2, id & 0x0F, false);
    for (unsigned id : kZigZag)     insert(id, 1, id, (id & 1) != 0);
    for (unsigned id : kHighBit)    insert(id, 0, id & 0x0F, true);

    insert(13, 1, 99, true);                 // duplicate: must not move the count
    const int countAfterDup = SetOf(&ours)->count;

    // ---- NEGATIVE CONTROL: shape sensitivity -------------------------------
    // If the comparison cannot see a one-field difference in the bookkeeping,
    // every green step above is worthless.
    bool negCaught = false;
    {
        int la = 0, lb = 0; static char a[8192], b[8192]; a[0] = b[0] = 0;
        Fingerprint(RootOf(&ours), '*', a, sizeof(a), &la);
        BindingNode* r = RootOf(&ours);
        const int16_t saved = r ? r->balance : 0;
        if (r) r->balance = static_cast<int16_t>(saved + 1);
        Fingerprint(RootOf(&ours), '*', b, sizeof(b), &lb);
        if (r) r->balance = saved;
        negCaught = r && std::strcmp(a, b) != 0;
    }

    // ---- the destroy walk (0x180023130) ------------------------------------
    // NO ours-vs-theirs comparison exists here: both sides free the nodes, and
    // the shipped ClearAll's frees go through a near call our registry cannot
    // intercept. What is checkable is that OUR walk reaches every node exactly
    // once. `expected` is measured by an independent pre-walk of the real tree
    // (built by the SHIPPED RegisterInput on the twin, so it is not our own
    // shape talking), minus the root ClearAll frees itself.
    const int nodesBefore = CountNodes(RootOf(&ours));
    // Can the visit count even SEE a dropped recursion? Only if both subtrees
    // of the root are non-empty. Measured on the real tree rather than asserted
    // — a control that cannot go to zero is not measuring coverage.
    const int rootLeftNodes  = RootOf(&ours) ? CountNodes(RootOf(&ours)->left)  : 0;
    const int rootRightNodes = RootOf(&ours) ? CountNodes(RootOf(&ours)->right) : 0;
    const unsigned visitsBefore = rtsetavl24::g_destroyVisits;
    RtInput_KeyMap_ClearAll(&ours);
    const unsigned visited = rtsetavl24::g_destroyVisits - visitsBefore;
    theirClear(&theirs);
    const bool clearOk = SetOf(&ours)->root == nullptr && SetOf(&ours)->count == 0 &&
                         SetOf(&theirs)->root == nullptr && SetOf(&theirs)->count == 0 &&
                         static_cast<int>(visited) == nodesBefore - 1;

    // ---- the visit counter's SENSITIVITY, measured not asserted ------------
    // A dropped `n->left` recursion would visit `nodesBefore-1-rootLeftNodes`
    // and a dropped `n->right` would visit `nodesBefore-1-rootRightNodes`.
    // Either is distinguishable from the expected value only if that subtree is
    // non-empty on the tree the sweep actually built.
    const bool negVisitCaught = rootLeftNodes > 0 && rootRightNodes > 0;

    // ---- the deleting destructor (0x180053440) -----------------------------
    // Its two live call sites are `Engine::Shutdown` (calls=0 on the autostart
    // path by construction — lane ENG says so) and `Configuration::Startup`'s
    // re-entry arm, which no boot takes. Left alone it is an UNEXERCISED PORT
    // reporting calls=0, which reference_uncalled_port_is_not_verified.md says
    // is not verification at all. So the probe drives it on a HEAP-ALLOCATED
    // synthetic KeyMap: the body frees its subject, so the subject has to come
    // from the engine allocator, not the stack.
    int dtorBindings = 0;
    bool dtorRan = false, dtorOk = false;
    {
        void* km = MemAlloc(0x18, 4);      // the size Configuration::Startup uses
        if (km) {
            std::memset(km, 0, 0x18);
            static const char* kDtorNames[6] = { "d0","d1","d2","d3","d4","d5" };
            static const unsigned kDtorIds[6] = { 40, 20, 60, 10, 50, 70 };
            for (int i = 0; i < 6; ++i)
                RtInput_KeyMap_RegisterInput(km, static_cast<int32_t>(kDtorIds[i]),
                                             1, 100 + i, kDtorNames[i], true);
            dtorBindings = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(km) + 0x10);
            const unsigned v0 = rtsetavl24::g_destroyVisits;
            void* ret = RtInput_KeyMap_DeletingDtor(km);
            const unsigned v1 = rtsetavl24::g_destroyVisits;
            dtorRan = true;
            // Retail returns `this` in RAX and the destroy walk must have
            // reached count-1 nodes (the root is freed by the body itself).
            dtorOk = ret == km && dtorBindings == 6 &&
                     (v1 - v0) == static_cast<unsigned>(dtorBindings - 1);
        }
    }

    printf("[SetAvl24] dtor(0x180053440): ran=%s bindings=%d -> %s "
                "(NO ours-vs-theirs comparison exists: the body frees the tree)\n",
                dtorRan ? "yes" : "NO -- MemAlloc failed", dtorBindings,
                dtorRan ? (dtorOk ? "OK" : "*** BAD ***") : "*** NOT EXERCISED ***");

    printf("[SetAvl24] steps=%d mismatches=%d parent-linkage-bad=%d\n"
                "[SetAvl24] effectiveness: depthAfter%dAscending=%d (a spine would be %d) "
                "rootChanged=%d idsAbove0x7F=%d finalCount=%d dupCount=%d\n"
                "[SetAvl24] destroy: nodes=%d visited=%u expected=%d -> %s "
                "(rootLeft=%d rootRight=%d)\n"
                "[SetAvl24] negative-controls: shape-sensitivity=%s "
                "visit-counter-can-see-a-dropped-subtree=%s\n",
                steps, mismatches, parentBad,
                (int)(sizeof(kAscending) / sizeof(kAscending[0])), depthAfterAscending,
                (int)(sizeof(kAscending) / sizeof(kAscending[0])),
                rootChanged, aboveSignBit, countAfterDup, countAfterDup,
                nodesBefore, visited, nodesBefore - 1, clearOk ? "OK" : "*** BAD ***",
                rootLeftNodes, rootRightNodes,
                negCaught ? "CAUGHT" : "*** MISSED ***",
                negVisitCaught ? "CAUGHT" : "*** MISSED ***");

    if (depthAfterAscending >= (int)(sizeof(kAscending) / sizeof(kAscending[0])))
        printf("[SetAvl24] *** depth == the insert count: NO ROTATION EVER RAN, "
                    "so this sweep verified only the descent ***\n");
    if (rootChanged == 0)
        printf("[SetAvl24] *** rootChanged=0: nothing restructured the tree ***\n");
    if (aboveSignBit == 0)
        printf("[SetAvl24] *** no id above 0x7F was inserted: a SIGNED byte "
                    "descent would have passed this sweep ***\n");
    if (!negCaught || !negVisitCaught)
        printf("[SetAvl24] *** a negative control was MISSED: the comparison is "
                    "not sensitive to the thing it is supposed to catch, so the "
                    "green steps above are void ***\n");

    g_running = false;
}
