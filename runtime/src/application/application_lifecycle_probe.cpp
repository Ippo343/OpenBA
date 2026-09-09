// ============================================================================
// runtime/src/application/application_lifecycle_probe.cpp — Wave 12 lane B
//   The lockstep synthetic-subject oracle for the Application lifecycle family.
// ============================================================================
//
// WHY A PROBE AND NOT A LIVE DIFF
// -------------------------------
// Four of this lane's five bodies cannot be compared at their call sites, and
// each for its own structural reason rather than for want of effort:
//
//   ~Application   FREES the tree it walks. Replaying the shipped body on a
//                  scratch copy hands it pointers ours has already released
//                  (Wave 9 lane M's released-pointer detector exists to name
//                  exactly that); replaying it on the live object double-frees
//                  every node.
//   copy ctor      ALLOCATES. Two runs build two trees at two addresses, so a
//                  whole-object memcmp differs at +0x18 however correct the
//                  port is.
//   operator=      allocates AND frees, both of the above at once.
//   PlatformShutdown  is a one-shot Win32 teardown; the replay's DestroyWindow
//                  gets an invalid handle and its UnregisterClassW finds no
//                  class, so the second run measures the first run's SUCCESS as
//                  a failure. It has no probe here and says so in its own
//                  banner — it is verified by execution on the live shutdown
//                  path, which is an execution record, not a comparison.
//
// The fifth, PlatformConstruct, is three stores with no side effects and gets a
// full byte-diff in its own body; it is re-run here only as a control.
//
// So: PRIVATE subjects with byte-identical starting state, the same script on
// both, ours through the ported bodies and theirs through the shipped ones. The
// shipped bodies are reached through **`ResolveExportRaw`, never
// `ResolveExport`** — the latter consults the port registry FIRST and this lane
// registers every one of these names, so it would have handed back our own
// functions and the probe would have compared ours to ours and always agreed
// (reference_resolveaddr_consults_port_registry_first.md).
//
// ---------------------------------------------------------------------------
// THE INSTRUMENT THAT MAKES "FREES EVERY NODE" A MEASUREMENT
// ---------------------------------------------------------------------------
// A destructor over an AVL tree that frees only the root is a leak wearing a
// byte-identity claim, and NEITHER of the two obvious instruments can see it:
//
//   * a byte comparison of the Application afterwards is IDENTICAL in both
//     cases — root and count are zeroed either way. It is structurally blind.
//   * an entry counter says the body ran, which it did.
//
// (reference_diagnostic_field_cannot_express_the_answer.md: the test is "could
// this field express the answer if the answer were different?" — for both of
// those the answer is no.)
//
// The instrument that CAN express it is the allocator's own live-block counter,
// `System::MemoryAllocator::allocCount` at +0x638
// (runtime/types/system/MemoryAllocator.h, pinned by the allocator port). It is
// snapshotted around each side's destruction of an N-node tree, and the two
// DELTAS are compared against each other and against N. A root-only destroy
// moves it by 1 where retail's moves it by N.
//
// That counter belongs to the shared allocator both sides use, so it is a
// DLL-CONSULTING comparison: it is not our body reporting on itself.
//
// NEGATIVE CONTROL, and it is the one that matters. A third identical tree is
// destroyed by a deliberately WRONG destructor — root only, no recursion — and
// the same instrument must report a delta of 1 rather than N. If it does not,
// every green line above it is void, and the probe says so in those words.
//
// A SECOND CONTROL covers the tree comparison used for the copy paths: one
// node's `balance` is corrupted in a clone and the fingerprint must notice.
// Without it a fingerprint that only carried keys would report green for a
// clone that dropped the AVL bookkeeping entirely.
//
// WHEN IT RUNS
// ------------
// Self-scheduled from the first `Application::PlatformUpdate` (application_win.
// cpp), for the reason set_avl_probe.cpp records: it ALLOCATES, and at
// `RuntimePortInit()` time System::MemoryAllocator has been constructed but not
// STARTED. The first frame is provably past that point.
//
// Opt-in: `BANISHED_RT_APPB_PROBE=1`.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/application/Application.h"
#include "../../types/system/MemoryAllocator.h"
#include "../collection/set_avl.h"

using RtApplication::Application;
using rtsetavl::Node;
using rtsetavl::Set;

// The ported bodies under test.
extern "C" void         RtApp_Dtor(Application* self);
extern "C" Application*  RtApp_CopyCtor(Application* self, const Application* other);
extern "C" Application*  RtApp_Assign(Application* self, const Application* other);
extern "C" void          RtApp_PlatformConstruct(Application* self, void* platformHandle);
extern "C" uint64_t      RtApp_LifecycleFreeCount();

// The already-ported insert, used only to BUILD subjects. It is not under test
// here — set_avl_probe.cpp is its oracle — and the shipped copy is what builds
// both sides, so the starting state is retail's rather than ours.
extern "C" void RtApp_RegisterMessageCallback(Application* app, unsigned id, void* cb);

namespace {

bool g_armed   = false;
bool g_running = false;
bool g_done    = false;

inline Set* SetOf(Application* a) {
    return reinterpret_cast<Set*>(reinterpret_cast<uint8_t*>(a) + 0x18);
}

// ---------------------------------------------------------------------------
// The allocator's live-block counter. Reached through RT_DATA_SLOT — the
// PORT-AWARE door — because on a minted/cutover run the singleton lives in our
// storage, not the DLL's, and a raw read would sample a slot nobody writes.
// ---------------------------------------------------------------------------
int AllocCount() {
    using Alloc = rt::system::MemoryAllocator;
    Alloc** slot = RT_DATA_SLOT(Alloc**, "?_singleton@MemoryAllocator@System@@0PEAV12@EA");
    if (!slot || !*slot) return -1;
    return (*slot)->allocCount;
}

// ---------------------------------------------------------------------------
// The fingerprint: a pre-order walk carrying SHAPE, balance and height — not
// just keys. Same construction as set_avl_probe.cpp's, and for the same reason:
// a clone that produced the right key set with the wrong bookkeeping breaks the
// very next operation and nothing else would see it. `.` for a null child so
// two differently-shaped trees with the same key sequence cannot collide.
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

// A fingerprint built by walking left/right cannot see a wrong `parent`, and a
// wrong parent is what a mis-transcribed clone produces while looking perfect.
bool ParentsConsistent(const Node* n, const Node* expectParent) {
    if (!n) return true;
    if (n->parent != expectParent) return false;
    return ParentsConsistent(n->left, n) && ParentsConsistent(n->right, n);
}

int CountNodes(const Node* n) {
    return n ? 1 + CountNodes(n->left) + CountNodes(n->right) : 0;
}

// The element's +0x08 qword, so a clone that dropped it would show.
void* CbFor(unsigned id) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(0xCB000000u) + id);
}

// ---------------------------------------------------------------------------
// The population. NOT uniform, and each group earns its place
// (reference_uniform_population_is_not_a_verification.md):
//   * an ASCENDING run and a DESCENDING run force rotations in both directions,
//     so the trees under test are genuinely restructured rather than spines;
//   * a zig-zag block forces the double rotations;
//   * ids above 0x7FFFFFFF, because the descent compare is UNSIGNED — with only
//     small ids a signed misreading orders identically and is invisible;
//   * enough nodes (39) that a root-only destroy is unmistakable against a full
//     one in the allocator counter.
// ---------------------------------------------------------------------------
const unsigned kIds[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    900, 890, 880, 870, 860, 850,
    500, 300, 400, 250, 270, 260, 600, 550, 560, 555,
    0x80000005u, 0xFFFFFFF0u, 0x7FFFFFFFu, 0x80000000u, 0xC0000000u,
    0x40000000u, 0x00FF00FFu,
};
constexpr int kN = static_cast<int>(sizeof(kIds) / sizeof(kIds[0]));

// The shipped entry points, resolved RAW so the port registry cannot intercept.
struct Shipped {
    void (*dtor)(Application*)                             = nullptr;
    Application* (*copyCtor)(Application*, const Application*) = nullptr;
    Application* (*assign)(Application*, const Application*)   = nullptr;
    void (*platformConstruct)(Application*, void*)         = nullptr;
    void (*reg)(Application*, unsigned, void*)             = nullptr;
    bool ok() const {
        return dtor && copyCtor && assign && platformConstruct && reg;
    }
};

Shipped LoadShipped() {
    Shipped s;
    s.dtor = reinterpret_cast<void (*)(Application*)>(
        ResolveExportRaw("??1Application@0@QEAA@XZ"));
    s.copyCtor = reinterpret_cast<Application* (*)(Application*, const Application*)>(
        ResolveExportRaw("??0Application@0@QEAA@AEBV00@@Z"));
    s.assign = reinterpret_cast<Application* (*)(Application*, const Application*)>(
        ResolveExportRaw("??4Application@0@QEAAAEAV00@AEBV00@@Z"));
    s.platformConstruct = reinterpret_cast<void (*)(Application*, void*)>(
        ResolveExportRaw("?PlatformConstruct@Application@1@AEAAXPEAX@Z"));
    s.reg = reinterpret_cast<void (*)(Application*, unsigned, void*)>(ResolveExportRaw(
        "?RegisterMessageCallback@Application@1@QEAAXIP6AHPEAUHWND__@@I_K_J@Z@Z"));
    return s;
}

// Build an N-node subject with the SHIPPED insert, so the starting state is
// retail's and only the function under test is ours.
void Build(const Shipped& sh, Application* app) {
    std::memset(app, 0, sizeof(Application));
    for (unsigned id : kIds) sh.reg(app, id, CbFor(id));
}

// The six bytes NEITHER the copy constructor NOR operator= writes: +0x12..+0x17
// and +0x24. Plus the eight bytes at +0x18, the tree ROOT, which is a heap
// pointer and MUST differ between two runs that each allocated their own tree.
// Excluded and said out loud rather than silently skipped
// (reference_oracle_must_not_compare_unwritten_bytes.md).
bool ByteEqualExcludingHeap(const void* a, const void* b, char* why, size_t whyN) {
    const uint8_t* x = static_cast<const uint8_t*>(a);
    const uint8_t* y = static_cast<const uint8_t*>(b);
    for (size_t i = 0; i < sizeof(Application); ++i) {
        const bool skip = (i >= 0x12 && i < 0x18) ||    // never written by either
                          (i >= 0x18 && i < 0x20) ||    // Set::root, a heap pointer
                          (i >= 0x24 && i < 0x28);      // never written by either
        if (skip) continue;
        if (x[i] != y[i]) {
            std::snprintf(why, whyN, "byte +0x%02x ours=0x%02x theirs=0x%02x",
                          static_cast<unsigned>(i), x[i], y[i]);
            return false;
        }
    }
    std::snprintf(why, whyN, "equal");
    return true;
}

}  // namespace

// ============================================================================
extern "C" void RtApp_LifecycleProbeArm(bool on) { g_armed = on; }

extern "C" void RtApp_LifecycleProbeMaybeRun() {
    if (!g_armed || g_running || g_done) return;
    g_running = true;

    Shipped sh = LoadShipped();
    if (!sh.ok()) {
        printf("[AppLifecycle] SKIP -- the shipped Application lifecycle exports "
               "are not reachable (cutover, or the DLL is absent). NO COMPARISON "
               "WAS MADE.\n");
        g_running = false;
        g_done = true;
        return;
    }
    if (AllocCount() < 0) {
        printf("[AppLifecycle] SKIP -- System::MemoryAllocator::_singleton is "
               "not resolvable, so the free-count instrument is unavailable and "
               "the destructor could only be checked by an instrument that is "
               "STRUCTURALLY blind to the defect it exists to catch. NO "
               "COMPARISON WAS MADE.\n");
        g_running = false;
        g_done = true;
        return;
    }

    int failures = 0;
    char detail[192];

    // ---------------------------------------------------------------------
    // 1. PlatformConstruct — a full byte diff, the one row with a real
    //    call-site oracle. Repeated here so the probe's report is complete.
    // ---------------------------------------------------------------------
    {
        alignas(8) uint8_t ours[sizeof(Application)];
        alignas(8) uint8_t theirs[sizeof(Application)];
        std::memset(ours,   0xCD, sizeof(ours));
        std::memset(theirs, 0xCD, sizeof(theirs));
        void* const kArg = reinterpret_cast<void*>(static_cast<uintptr_t>(0x1234ABCD5678u));
        RtApp_PlatformConstruct(reinterpret_cast<Application*>(ours), kArg);
        sh.platformConstruct(reinterpret_cast<Application*>(theirs), kArg);
        const bool same = std::memcmp(ours, theirs, sizeof(Application)) == 0;
        if (!same) ++failures;
        printf("[AppLifecycle] PlatformConstruct: full 0x30-byte diff %s\n",
               same ? "EQUAL" : "*** DIFFERS ***");
    }

    // ---------------------------------------------------------------------
    // 2. The copy constructor. Two destinations pre-filled 0xCD (so the six
    //    bytes neither side writes compare equal in both), one source built by
    //    the shipped insert.
    // ---------------------------------------------------------------------
    {
        static Application src;
        Build(sh, &src);
        const int srcNodes = CountNodes(SetOf(&src)->root);

        alignas(8) uint8_t ob[sizeof(Application)], tb[sizeof(Application)];
        std::memset(ob, 0xCD, sizeof(ob));
        std::memset(tb, 0xCD, sizeof(tb));
        Application* o = reinterpret_cast<Application*>(ob);
        Application* t = reinterpret_cast<Application*>(tb);

        RtApp_CopyCtor(o, &src);
        sh.copyCtor(t, &src);

        const bool bytesOk = ByteEqualExcludingHeap(ob, tb, detail, sizeof(detail));
        char fo[4096], ft[4096];
        int lo = 0, lt = 0; fo[0] = ft[0] = 0;
        Fingerprint(SetOf(o)->root, '*', fo, sizeof(fo), &lo);
        Fingerprint(SetOf(t)->root, '*', ft, sizeof(ft), &lt);
        const bool shapeOk   = std::strcmp(fo, ft) == 0;
        const bool parentsOk = ParentsConsistent(SetOf(o)->root, nullptr);
        const bool countOk   = SetOf(o)->count == SetOf(t)->count;
        // The clone must also be a DEEP copy: no node of it may be a node of
        // the source. Checking the root alone is enough to catch a shallow
        // copy, because a shallow copy shares the root by construction.
        const bool deepOk = SetOf(o)->root != SetOf(&src)->root;
        if (!(bytesOk && shapeOk && parentsOk && countOk && deepOk)) ++failures;
        printf("[AppLifecycle] copy-ctor: bytes=%s(%s) shape=%s parents=%s "
               "count=%s deep=%s nodes=%d\n",
               bytesOk ? "EQUAL" : "***DIFFERS***", detail,
               shapeOk ? "EQUAL" : "***DIFFERS***",
               parentsOk ? "OK" : "***BROKEN***",
               countOk ? "EQUAL" : "***DIFFERS***",
               deepOk ? "OK" : "***SHALLOW***", srcNodes);
        if (!shapeOk) {
            printf("[AppLifecycle]   ours   %s\n[AppLifecycle]   theirs %s\n", fo, ft);
        }

        // Release the two clones and the source with the SHIPPED destructor, so
        // the probe does not leak 3*N nodes into a run it may share with the
        // stress harness.
        sh.dtor(o); sh.dtor(t); sh.dtor(&src);
    }

    // ---------------------------------------------------------------------
    // 3. operator=. The destination starts with a DIFFERENT non-empty tree, so
    //    the Clear branch (0x18000a1ad) is the point of the test — that is the
    //    only path in this lane that frees an existing tree during an assign,
    //    and a port that skipped it would leak silently and compare green on
    //    every other column.
    // ---------------------------------------------------------------------
    {
        static Application src, o, t;
        Build(sh, &src);
        // A destination population deliberately DISJOINT from the source's, so
        // a Clear that did nothing leaves ids nothing else would produce.
        std::memset(&o, 0, sizeof(o));
        std::memset(&t, 0, sizeof(t));
        for (unsigned i = 0; i < 12; ++i) {
            sh.reg(&o, 0x11110000u + i, CbFor(i));
            sh.reg(&t, 0x11110000u + i, CbFor(i));
        }

        const int before = AllocCount();
        RtApp_Assign(&o, &src);
        const int afterOurs = AllocCount();
        sh.assign(&t, &src);
        const int afterTheirs = AllocCount();

        // ours: -12 old nodes +kN new; theirs: the same. Compared as DELTAS.
        const int dOurs   = afterOurs   - before;
        const int dTheirs = afterTheirs - afterOurs;

        const bool bytesOk = ByteEqualExcludingHeap(&o, &t, detail, sizeof(detail));
        char fo[4096], ft[4096];
        int lo = 0, lt = 0; fo[0] = ft[0] = 0;
        Fingerprint(SetOf(&o)->root, '*', fo, sizeof(fo), &lo);
        Fingerprint(SetOf(&t)->root, '*', ft, sizeof(ft), &lt);
        const bool shapeOk   = std::strcmp(fo, ft) == 0;
        const bool parentsOk = ParentsConsistent(SetOf(&o)->root, nullptr);
        const bool deltaOk   = dOurs == dTheirs;
        const bool clearedOk = dOurs == (kN - 12);
        if (!(bytesOk && shapeOk && parentsOk && deltaOk && clearedOk)) ++failures;
        printf("[AppLifecycle] operator=: bytes=%s(%s) shape=%s parents=%s "
               "allocCount delta ours=%+d theirs=%+d expected=%+d %s\n",
               bytesOk ? "EQUAL" : "***DIFFERS***", detail,
               shapeOk ? "EQUAL" : "***DIFFERS***",
               parentsOk ? "OK" : "***BROKEN***",
               dOurs, dTheirs, kN - 12,
               (deltaOk && clearedOk) ? "" : "*** the Clear branch did not free "
                                             "the old tree ***");
        sh.dtor(&o); sh.dtor(&t); sh.dtor(&src);
    }

    // ---------------------------------------------------------------------
    // 4. THE DESTRUCTOR, and the instrument this whole file exists for.
    // ---------------------------------------------------------------------
    int dOurs = 0, dTheirs = 0, dBroken = 0;
    bool dtorBytesOk = false;
    {
        static Application a, b;
        Build(sh, &a);
        Build(sh, &b);
        const int nodesA = CountNodes(SetOf(&a)->root);
        const int nodesB = CountNodes(SetOf(&b)->root);

        const uint64_t freedBefore = RtApp_LifecycleFreeCount();
        const int c0 = AllocCount();
        RtApp_Dtor(&a);
        const int c1 = AllocCount();
        sh.dtor(&b);
        const int c2 = AllocCount();

        dOurs   = c0 - c1;      // blocks released by OUR body
        dTheirs = c1 - c2;      // blocks released by the SHIPPED body
        const uint64_t oursFreed = RtApp_LifecycleFreeCount() - freedBefore;

        dtorBytesOk = std::memcmp(&a, &b, sizeof(Application)) == 0;
        const bool deltaOk = (dOurs == dTheirs) && (dOurs == nodesA);
        if (!(dtorBytesOk && deltaOk && nodesA == nodesB)) ++failures;

        printf("[AppLifecycle] ~Application: nodes=%d  allocCount released "
               "ours=%d theirs=%d  our own free-counter=%llu  object bytes=%s %s\n",
               nodesA, dOurs, dTheirs, (unsigned long long)oursFreed,
               dtorBytesOk ? "EQUAL" : "***DIFFER***",
               deltaOk ? "" : "*** THE TWO SIDES DID NOT FREE THE SAME NUMBER "
                              "OF BLOCKS ***");
    }

    // ---------------------------------------------------------------------
    // 5. NEGATIVE CONTROL 1 — the defect the destructor row exists to catch.
    //    A root-only destroy on an identical tree. If the allocCount instrument
    //    reports the same delta as the real destructor, it is blind and every
    //    green line above it is void.
    // ---------------------------------------------------------------------
    bool neg1 = false;
    {
        static Application bad;
        Build(sh, &bad);
        const int nodes = CountNodes(SetOf(&bad)->root);
        Set* s = SetOf(&bad);
        Node* root = s->root;
        const int c0 = AllocCount();
        if (root) rtsetavl::SetNodeFree(s, root);   // ROOT ONLY. No recursion.
        const int c1 = AllocCount();
        dBroken = c0 - c1;
        s->root = nullptr;
        s->count = 0;
        neg1 = (dBroken != dOurs) && (dBroken < nodes);
        // The tree's other nodes are now unreachable. That is the point of the
        // control and it leaks kN-1 blocks by design, once per process.
    }

    // ---------------------------------------------------------------------
    // 6. NEGATIVE CONTROL 2 — shape sensitivity of the clone comparison. One
    //    node's balance is corrupted; the fingerprint must notice. Without this
    //    a fingerprint carrying only keys would pass a clone that dropped the
    //    AVL bookkeeping.
    // ---------------------------------------------------------------------
    bool neg2 = false;
    {
        static Application src, o, t;
        Build(sh, &src);
        std::memset(&o, 0, sizeof(o));
        std::memset(&t, 0, sizeof(t));
        RtApp_CopyCtor(&o, &src);
        sh.copyCtor(&t, &src);
        if (SetOf(&o)->root) SetOf(&o)->root->balance += 1;
        char a[2048], b[2048];
        int la = 0, lb = 0; a[0] = b[0] = 0;
        Fingerprint(SetOf(&o)->root, '*', a, sizeof(a), &la);
        Fingerprint(SetOf(&t)->root, '*', b, sizeof(b), &lb);
        neg2 = std::strcmp(a, b) != 0;
        sh.dtor(&o); sh.dtor(&t); sh.dtor(&src);
    }

    printf("[AppLifecycle] nodes-per-subject=%d failures=%d\n"
           "[AppLifecycle] negative-controls: root-only-destroy=%s (broken "
           "delta=%d vs real %d)  shape-sensitivity=%s\n",
           kN, failures,
           neg1 ? "CAUGHT" : "*** MISSED ***", dBroken, dOurs,
           neg2 ? "CAUGHT" : "*** MISSED ***");

    if (!neg1 || !neg2) {
        printf("[AppLifecycle] *** A NEGATIVE CONTROL WAS MISSED: the "
               "instrument is not sensitive to the defect it exists to catch, "
               "so every green line above is void ***\n");
    }
    if (dOurs <= 1) {
        printf("[AppLifecycle] *** the destructor released %d block(s) for a "
               "%d-node tree: either the subject was not built or the walk is "
               "not recursing ***\n", dOurs, kN);
    }

    g_running = false;
    g_done = true;
}
