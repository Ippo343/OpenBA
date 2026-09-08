// ============================================================================
// runtime/src/collection/set_clear_probe.cpp — Wave 9 lane SAVE2
//   The differential oracle for `Set<Pair<String,String>>::Clear`.
// ============================================================================
//
// THE PROBLEM THIS SOLVES. A destructor cannot be replayed: "call ours, call
// theirs, compare" double-frees every block, and a scratch COPY of the subject
// aliases the same heap (SYS2 wrote that up for `~SaveLoad` and concluded no
// oracle existed there). The way out here is the one
// reference_synthetic_subject_beats_a_fake.md points at: do not copy one
// subject, BUILD TWO — two private trees with identical contents, destroyed
// side by side by the two implementations.
//
// WHAT IS COMPARED, and why it is a comparison against RETAIL rather than a
// self-check. Everything `Clear` does that outlives it is heap traffic, and the
// allocator counts its own: `MemoryAllocator::allocCount` (+0x638, live blocks)
// and `allocatedBytes` (+0x630), both pinned by Wave 2 with static_asserts in
// runtime/types/system/MemoryAllocator.h. Snapshot them around each side and
// the deltas say exactly how many blocks and how many bytes that side handed
// back. A port that misses a subtree, frees a node twice, releases the key and
// not the value, or takes the wrong arm of the refcount test moves those
// numbers. Post-state (`root`, `count`) is compared too, but it is the weak
// half — both sides trivially end at zero.
//
// WHAT IT CANNOT SEE, stated because a partial oracle that reads like a full
// one is worse than none (lane PICK rule 1: ask what your comparison point
// cannot see). The counters are a SUM. They cannot see the ORDER of the frees,
// so a post-order destroy and a pre-order destroy that both visit every node
// exactly once compare equal here. Two things reduce that gap and neither
// closes it: the order is only four instructions of the transcription (children
// loaded before the free, which is checked by reading, not by running), and any
// order defect that reads a freed node's child pointer walks a free-list link
// and shows up as a wrong block count almost immediately. It is named rather
// than hidden.
//
// THE CORPUS AND ITS CONTROLS. Three shapes, chosen to reach branches the
// settings map itself would reach only by luck:
//   1. empty      — the `root == 0` arm, which still stores the two zeroes
//   2. a 7-node balanced tree — both recursions, three levels
//   3. a 5-node tree in which TWO nodes share ONE value String — the arm of
//      the string release that does NOT free (refcount 2 -> 1, slot set NULL).
//      Without it every release in the corpus takes the same arm, and
//      reference_uniform_population_is_not_a_verification.md is exactly that
//      failure: four waves of green oracles over a population of one value.
//
// EFFECTIVENESS IS COUNTED INSIDE THE FUNCTION UNDER TEST, not from outside
// (WAVE9.md, "count branches taken INSIDE the function"): set_clear.cpp keeps
// four counters — recursive entries, null-child returns, string releases that
// FREED, string releases that only DECREMENTED — and this probe prints them and
// says so when any is zero.
//
// NEGATIVE CONTROL. A deliberately wrong Clear that skips the right subtree is
// run on a third identical tree; the comparison must catch it. If it does not,
// every green line above is void.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "set_avl.h"
#include "array_internal.h"

uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

namespace rtsetclear {
using rtsetavl::Node;
using rtsetavl::Set;
void Clear(Set* set);
void DestroySubtree(void* ignored, Node* n);
void DestroyElement(void* node);
extern long g_subtreeEnter;   // recursive entries with a non-null node
extern long g_subtreeNull;    // recursive entries that returned immediately
extern long g_strFreed;       // string releases whose refcount reached 0
extern long g_strShared;      // string releases whose refcount did not
}

namespace {

using rtsetavl::Node;
using rtsetavl::Set;

bool g_armed = false;
bool g_running = false;

// ---- the allocator's own counters -----------------------------------------
struct AllocSnap { int32_t blocks; int64_t bytes; };

uint8_t* AllocatorSingleton() {
    void** slot = RT_DATA_SLOT(void**, "?_singleton@MemoryAllocator@System@@0PEAV12@EA");
    return slot ? static_cast<uint8_t*>(*slot) : nullptr;
}

bool Snap(AllocSnap* out) {
    uint8_t* a = AllocatorSingleton();
    if (!a) return false;
    out->blocks = *reinterpret_cast<int32_t*>(a + 0x638);
    out->bytes  = *reinterpret_cast<int64_t*>(a + 0x630);
    return true;
}

// ---- subject construction --------------------------------------------------
// A System::String built the way retail builds one: a zeroed one-pointer object
// handed to ?Set@String@System@@QEAAXPEBD@Z, which allocates
// (len + 1 + 8) bytes and writes length at -8 and refcount 1 at -4.
void StringSet(void* str, const char* text) {
    using Fn = void (*)(void*, const char*);
    RT_EXPORT_ONCE(Fn, "?Set@String@System@@QEAAXPEBD@Z")(str, text);
}

// Node element: { char* key; char* value; } — Persistence.h's settings pair.
struct PairNode {
    char*   key;      // +0x00
    char*   value;    // +0x08
    Node*   parent;   // +0x10
    Node*   left;     // +0x18
    Node*   right;    // +0x20
    int16_t balance;  // +0x28
    int16_t height;   // +0x2a
    uint32_t pad;     // +0x2c
};
static_assert(sizeof(PairNode) == 0x30, "the settings map's node is 0x30 bytes");

PairNode* NewNode(const char* k, const char* v) {
    PairNode* n = static_cast<PairNode*>(rtcoll::MemAlloc(0x30, 8));
    if (!n) return nullptr;
    std::memset(n, 0, sizeof(*n));
    StringSet(&n->key, k);
    if (v) StringSet(&n->value, v);
    return n;
}

void Link(PairNode* parent, PairNode* left, PairNode* right) {
    parent->left  = reinterpret_cast<Node*>(left);
    parent->right = reinterpret_cast<Node*>(right);
    if (left)  left->parent  = reinterpret_cast<Node*>(parent);
    if (right) right->parent = reinterpret_cast<Node*>(parent);
}

// ---- the three shapes ------------------------------------------------------
// `tag` differentiates nothing in the comparison (both sides get the same tag);
// it exists so a leaked block is identifiable in a heap dump.
int BuildBalanced7(Set* set) {
    PairNode* n[7];
    static const char* keys[7] = { "d", "b", "f", "a", "c", "e", "g" };
    for (int i = 0; i < 7; ++i) {
        char v[32];
        std::snprintf(v, sizeof(v), "value-%d-padding", i);
        n[i] = NewNode(keys[i], v);
        if (!n[i]) return 0;
    }
    Link(n[0], n[1], n[2]);
    Link(n[1], n[3], n[4]);
    Link(n[2], n[5], n[6]);
    set->root  = reinterpret_cast<Node*>(n[0]);
    set->count = 7;
    return 7;
}

// Two nodes hold the SAME value buffer, refcount bumped to 2 by hand exactly as
// System::String's copy ctor does (INC dword [p-4]). Releasing the first must
// take the decrement-only arm and NOT free.
int BuildShared5(Set* set) {
    PairNode* n[5];
    static const char* keys[5] = { "k2", "k1", "k4", "k3", "k5" };
    for (int i = 0; i < 5; ++i) {
        n[i] = NewNode(keys[i], (i == 1 || i == 3) ? nullptr : "shared-or-own");
        if (!n[i]) return 0;
    }
    // n[1] and n[3] share n[0]'s value buffer.
    n[1]->value = n[0]->value;
    n[3]->value = n[0]->value;
    *reinterpret_cast<int32_t*>(n[0]->value - 4) += 2;   // refcount 1 -> 3
    Link(n[0], n[1], n[2]);
    Link(n[1], nullptr, n[3]);
    Link(n[2], nullptr, n[4]);
    set->root  = reinterpret_cast<Node*>(n[0]);
    set->count = 5;
    return 5;
}

// The deliberately wrong Clear used as the negative control: right subtree
// skipped. Everything else identical to the port.
void ClearSkippingRight(Set* set) {
    Node* n = set->root;
    if (n) {
        rtsetclear::DestroySubtree(set, n->left);
        // n->right deliberately NOT destroyed
        rtsetclear::DestroyElement(n);
        rtsetavl::SetNodeFree(set, n);
    }
    set->root  = nullptr;
    set->count = 0;
}

using ClearFn = void (*)(Set*);

struct Result { int32_t blocks; int64_t bytes; int32_t rootZero; int32_t count; };

Result RunOne(int (*build)(Set*), ClearFn clear) {
    Result r = { 0, 0, 0, -1 };
    Set set = {};
    if (!build(&set)) return r;
    AllocSnap a{}, b{};
    if (!Snap(&a)) return r;
    clear(&set);
    if (!Snap(&b)) return r;
    r.blocks   = a.blocks - b.blocks;      // positive == blocks handed back
    r.bytes    = a.bytes - b.bytes;
    r.rootZero = (set.root == nullptr) ? 1 : 0;
    r.count    = set.count;
    return r;
}

Result RunEmpty(ClearFn clear) {
    Result r = { 0, 0, 0, -1 };
    Set set = {};
    set.count = 4242;                      // stale count: retail zeroes it anyway
    AllocSnap a{}, b{};
    if (!Snap(&a)) return r;
    clear(&set);
    if (!Snap(&b)) return r;
    r.blocks   = a.blocks - b.blocks;
    r.bytes    = a.bytes - b.bytes;
    r.rootZero = (set.root == nullptr) ? 1 : 0;
    r.count    = set.count;
    return r;
}

bool Same(const Result& x, const Result& y) {
    return x.blocks == y.blocks && x.bytes == y.bytes &&
           x.rootZero == y.rootZero && x.count == y.count;
}

void Report(const char* what, const Result& o, const Result& t, int* mismatches) {
    const bool ok = Same(o, t);
    if (!ok) ++*mismatches;
    std::printf("[SetClear] %-10s %s  ours[blocks=%d bytes=%lld root0=%d count=%d]"
                "  theirs[blocks=%d bytes=%lld root0=%d count=%d]\n",
                what, ok ? "OK  " : "DIFF", o.blocks, (long long)o.bytes, o.rootZero, o.count,
                t.blocks, (long long)t.bytes, t.rootZero, t.count);
}

}  // namespace

extern "C" void RtCollectionSetClearProbeArm(bool on) { g_armed = on; }

extern "C" void RtCollectionSetClearProbeMaybeRun() {
    if (!g_armed || g_running) return;
    g_armed = false;
    g_running = true;

    ClearFn theirs = reinterpret_cast<ClearFn>(ResolveRuntimeRaw(0x18000f7d0));
    if (!theirs) {
        std::printf("[SetClear] SKIP -- the shipped 0x18000f7d0 is not reachable "
                    "(cutover, or the DLL is absent). NO COMPARISON WAS MADE.\n");
        g_running = false;
        return;
    }
    if (!AllocatorSingleton()) {
        std::printf("[SetClear] SKIP -- MemoryAllocator::_singleton is null, so the "
                    "counters this oracle compares do not exist yet. "
                    "NO COMPARISON WAS MADE.\n");
        g_running = false;
        return;
    }

    const long e0 = rtsetclear::g_subtreeEnter, n0 = rtsetclear::g_subtreeNull;
    const long f0 = rtsetclear::g_strFreed,     s0 = rtsetclear::g_strShared;

    int mismatches = 0;

    // Ours first, then theirs, on freshly built identical subjects each time.
    Result oe = RunEmpty(&rtsetclear::Clear);
    Result te = RunEmpty(theirs);
    Report("empty", oe, te, &mismatches);

    Result ob = RunOne(&BuildBalanced7, &rtsetclear::Clear);
    Result tb = RunOne(&BuildBalanced7, theirs);
    Report("balanced7", ob, tb, &mismatches);

    Result os = RunOne(&BuildShared5, &rtsetclear::Clear);
    Result ts = RunOne(&BuildShared5, theirs);
    Report("shared5", os, ts, &mismatches);

    // ---- NEGATIVE CONTROL --------------------------------------------------
    // The wrong Clear leaks the right subtree, so it frees strictly fewer
    // blocks. If this reads OK the comparison is not measuring what it claims.
    Result on = RunOne(&BuildBalanced7, &ClearSkippingRight);
    const bool caught = !Same(on, tb);
    std::printf("[SetClear] negative-control skip-right: %s "
                "(blocks=%d vs retail %d)\n",
                caught ? "CAUGHT" : "*** MISSED ***", on.blocks, tb.blocks);

    // ---- EFFECTIVENESS, counted inside the bodies under test ---------------
    const long dEnter  = rtsetclear::g_subtreeEnter - e0;
    const long dNull   = rtsetclear::g_subtreeNull  - n0;
    const long dFreed  = rtsetclear::g_strFreed     - f0;
    const long dShared = rtsetclear::g_strShared    - s0;
    std::printf("[SetClear] comparisons=3 mismatches=%d\n"
                "[SetClear] effectiveness (inside our bodies): recurse=%ld "
                "null-child=%ld strRelease-freed=%ld strRelease-shared=%ld\n",
                mismatches, dEnter, dNull, dFreed, dShared);

    if (dEnter == 0)
        std::printf("[SetClear] *** recurse=0: the recursive helper never ran, so "
                    "only the root arm was tested ***\n");
    if (dShared == 0)
        std::printf("[SetClear] *** strRelease-shared=0: every string release took "
                    "the SAME arm, which is the uniform-population failure "
                    "(reference_uniform_population_is_not_a_verification.md) ***\n");
    if (!caught)
        std::printf("[SetClear] *** the negative control was MISSED: the allocator "
                    "deltas are not sensitive to a skipped subtree, so the green "
                    "lines above are void ***\n");

    g_running = false;
}
